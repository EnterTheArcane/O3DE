import traceback
from collections import OrderedDict
from importlib import invalidate_caches, util as imp_util
import inspect
import os
import sys
import types
import uuid
from threading import Lock

from pathlib import Path

from thirdparty._internal.output import Output
from thirdparty.cmake import cmake_layout
from thirdparty.google import bazel_layout
from thirdparty.microsoft import vs_layout
from thirdparty._internal.errors import recipe_exception_formatter, NotFoundException
from thirdparty.errors import RecipeException
from thirdparty._internal.model.recipe_base import RecipeBase
from thirdparty._internal.model.options import Options
from thirdparty._internal.model.refs import RecipeReference
from thirdparty._internal.util.config_parser import TextINIParse
from thirdparty._internal.util.files import chdir, load_user_encoded
from thirdparty._internal.detect import detect_settings, make_conf
from thirdparty._internal.model.dependencies import RecipeDependencies
from thirdparty.env import Environment


class RecipeLoader:

    def __init__(self, pyreq_loader=None, recipe_helpers=None):
        self._pyreq_loader = pyreq_loader
        self._cached_recipe_classes = {}
        self._recipe_helpers = recipe_helpers
        invalidate_caches()

    def load_basic(self, recipe_path, graph_lock=None, display="",
                   update=None, check_update=None):
        """ loads a recipe basic object without evaluating anything
        """
        return self.load_basic_module(recipe_path, graph_lock, display,
                                      update, check_update)[0]

    def load_basic_module(self, recipe_path, graph_lock=None, display="",
                          update=None, check_update=None, tested_python_requires=None):
        """ loads a recipe basic object without evaluating anything, returns the module too
        """
        cached = self._cached_recipe_classes.get(recipe_path)
        if cached:
            recipe = cached[0](display)
            recipe._recipe_runtime = self._recipe_helpers
            if hasattr(recipe, "init") and callable(recipe.init):
                with recipe_exception_formatter(recipe, "init"):
                    recipe.init()
            return recipe, cached[1]

        try:
            module, recipe = _parse_recipe(recipe_path)
            if isinstance(tested_python_requires, RecipeReference):
                if getattr(recipe, "python_requires", None) == "tested_reference_str":
                    recipe.python_requires = tested_python_requires.repr_notime()
            elif tested_python_requires:
                if getattr(recipe, "python_requires", None) != "tested_reference_str":
                    Output().warning("test_package/recipe.py should declare 'python_requires"
                                          " = \"tested_reference_str\"'", warn_tag="deprecated")
                recipe.python_requires = tested_python_requires

            if self._pyreq_loader:
                self._pyreq_loader.load_py_requires(recipe, self, graph_lock, None,
                                                    update, check_update)

            recipe.recipe_folder = os.path.dirname(recipe_path)
            recipe.recipe_path = Path(recipe.recipe_folder)

            self._cached_recipe_classes[recipe_path] = (recipe, module)
            result = recipe(display)

            result._recipe_runtime = self._recipe_helpers
            if hasattr(result, "init") and callable(result.init):
                with recipe_exception_formatter(result, "init"):
                    result.init()
            return result, module
        except RecipeException as e:
            raise RecipeException("Error loading recipe at '{}': {}".format(recipe_path, e))

    def load_named(self, recipe_path, name, version, graph_lock=None,
                   update=None, check_update=None, tested_python_requires=None):
        """ loads the basic recipe object and evaluates its name and version
        """
        recipe, _ = self.load_basic_module(recipe_path, graph_lock,
                                              update=update, check_update=check_update,
                                              tested_python_requires=tested_python_requires)

        # Export does a check on existing name & version
        if name:
            if recipe.name and name != recipe.name:
                raise RecipeException("Package recipe with name %s!=%s" % (name, recipe.name))
            recipe.name = name

        if version:
            if recipe.version and version != recipe.version:
                raise RecipeException("Package recipe with version %s!=%s"
                                     % (version, recipe.version))
            recipe.version = version

        if hasattr(recipe, "set_name"):
            with recipe_exception_formatter("recipe.py", "set_name"):
                recipe.set_name()
        if hasattr(recipe, "set_version"):
            with recipe_exception_formatter("recipe.py", "set_version"):
                recipe.set_version()

        return recipe

    def load_export(self, recipe_path, name, version, graph_lock=None):
        """ loads the recipe and evaluates its name, version, and enforce its existence
        """
        recipe = self.load_named(recipe_path, name, version, graph_lock)
        if not recipe.name:
            raise RecipeException("recipe didn't specify name")
        if not recipe.version:
            raise RecipeException("recipe didn't specify version")

        ref = RecipeReference(recipe.name, recipe.version)
        recipe.display_name = str(ref)
        return recipe

    def load_consumer(self, recipe_path, name=None, version=None,
                      graph_lock=None, update=None, check_update=None,
                      tested_python_requires=None):
        """ loads a recipe.py in user space. Might have name/version or not
        """
        recipe = self.load_named(recipe_path, name, version, graph_lock,
                                    update, check_update,
                                    tested_python_requires=tested_python_requires)

        ref = RecipeReference(recipe.name, recipe.version)
        if str(ref):
            recipe.display_name = "%s (%s)" % (os.path.basename(recipe_path), str(ref))
        else:
            recipe.display_name = os.path.basename(recipe_path)
        recipe._is_consumer_recipe = True
        return recipe

    def load_recipe(self, recipe_path, ref, graph_lock=None,
                       update=None, check_update=None):
        """ load a recipe with a full reference, name, version, user and channel are obtained
        from the reference, not evaluated. Main way to load from the cache
        """
        try:
            recipe, _ = self.load_basic_module(recipe_path, graph_lock, str(ref),
                                                  update=update, check_update=check_update)
        except Exception as e:
            raise RecipeException("%s: Cannot load recipe.\n%s" % (str(ref), str(e)))

        recipe.name = ref.name
        recipe.version = str(ref.version)
        return recipe

    def load_recipe_txt(self, recipe_txt_path):
        if not os.path.exists(recipe_txt_path):
            raise NotFoundException("Recipe file not found!")

        try:
            contents = load_user_encoded(recipe_txt_path)
        except Exception as e:
            raise RecipeException(f"Cannot load recipe.txt:\n{e}")
        path, basename = os.path.split(recipe_txt_path)
        display_name = basename
        recipe = self._parse_recipe_txt(contents, path, display_name)
        recipe._recipe_runtime = self._recipe_helpers
        recipe._is_consumer_recipe = True
        return recipe

    @staticmethod
    def _parse_recipe_txt(contents, path, display_name):
        recipe = RecipeBase(display_name)

        try:
            parser = RecipeTextLoader(contents)
        except Exception as e:
            raise RecipeException("%s:\n%s" % (path, str(e)))
        for reference in parser.requirements:
            recipe.requires(reference)
        for build_reference in parser.tool_requirements:
            # TODO: Improve this interface
            recipe.requires.tool_require(build_reference)
        for ref in parser.test_requirements:
            # TODO: Improve this interface
            recipe.requires.test_require(ref)

        if parser.layout:
            layout_method = {"cmake_layout": cmake_layout,
                             "vs_layout": vs_layout,
                             "bazel_layout": bazel_layout}.get(parser.layout)
            if not layout_method:
                raise RecipeException("Unknown predefined layout '{}' declared in "
                                     "recipe.txt".format(parser.layout))

            def layout(_self):
                layout_method(_self)

            recipe.layout = types.MethodType(layout, recipe)

        try:
            recipe.options = Options.loads(parser.options)
        except Exception:
            raise RecipeException("Error while parsing [options] in recipe.txt\n"
                                 "Options should be specified as 'pkg/*:option=value'")
        return recipe

    def load_virtual(self, requires=None, tool_requires=None, python_requires=None, graph_lock=None,
                     update=None, check_updates=None):
        # If user don't specify namespace in options, assume that it is
        # for the reference (keep compatibility)
        recipe = RecipeBase(display_name="cli")
        recipe._recipe_runtime = self._recipe_helpers

        if tool_requires:
            for reference in tool_requires:
                recipe.requires.tool_require(repr(reference))
        if requires:
            for reference in requires:
                recipe.requires(repr(reference))

        if python_requires:
            recipe.python_requires = [pr.repr_notime() for pr in python_requires]

        if self._pyreq_loader:
            self._pyreq_loader.load_py_requires(recipe, self, graph_lock, None,
                                                update, check_updates)

        recipe._is_consumer_recipe = True
        return recipe


def _parse_module(recipe_module, module_id):
    """ Parses a python in-memory module, to extract the class defining the Recipe.
    @param recipe_module: the module to be processed
    @return: the main RecipeBase class from the module
    """
    result = None
    for name, attr in recipe_module.__dict__.items():
        if (name.startswith("_") or not inspect.isclass(attr) or
                attr.__dict__.get("__module__") != module_id):
            continue

        if issubclass(attr, RecipeBase) and attr != RecipeBase:
            if result is None:
                result = attr
            else:
                raise RecipeException("More than 1 recipe in the file")

    if result is None:
        raise RecipeException("No subclass of RecipeBase")

    return result


_load_python_lock = Lock()  # Loading our Python files is not thread-safe (modifies sys)


def _parse_recipe(recipe_path):
    with _load_python_lock:
        module, module_id = _load_python_file(recipe_path)
    try:
        recipe = _parse_module(module, module_id)
        return module, recipe
    except Exception as e:  # re-raise with file name
        raise RecipeException("%s: %s" % (recipe_path, str(e)))


def try_load_recipe_class(recipes_root: Path, name: str) -> type[RecipeBase] | None:
    """Load the recipe class from ``recipes/<name>/recipe.py``.

    Delegates to the central recipe parser (``_parse_recipe``), which loads the file under
    a lock with a unique module id, inserts the recipe directory on ``sys.path`` so recipes
    can import sibling helpers, and validates that exactly one RecipeBase subclass is
    defined.  Returns ``None`` if the recipe is missing or fails to load/validate.
    """
    recipe_path = recipes_root / name / "recipe.py"
    if not recipe_path.exists():
        return None
    try:
        module, cls = _parse_recipe(str(recipe_path))
        if not (isinstance(cls, type) and issubclass(cls, RecipeBase)):
            return None
        # Collect implicit tool_requires from the recipe's DIRECT imports only.  A module's
        # namespace contains only the names it imported/defined itself, so build-system
        # helpers pulled in transitively by thirdparty.* are never counted here.
        implicit: set[str] = set()
        for obj in vars(module).values():
            implicit.update(getattr(obj, "_implicit_tool_requires", ()))
        cls._implicit_tool_requires = frozenset(implicit)
        return cls
    except Exception:
        return None


def resolve_version(recipe_cls: type[RecipeBase]) -> str:
    v = getattr(recipe_cls, "version", None)
    return str(v) if v else "latest"


# ---------------------------------------------------------------------------
# Recipe runtime services + requirement shim, used when instantiating a recipe
# ---------------------------------------------------------------------------
class RecipeRuntime:
    """Runtime services recipe methods touch during graph resolution and building.

    This system has no Conan cache, remotes, or profiles, so recipes are driven directly;
    this supplies the handful of services they reference (global conf + HTTP requester).
    """

    def __init__(self, conf):
        # Lazy import: rest.http_requester imports loader.load_python_file, so a module-level
        # import here would be circular.
        from thirdparty._internal.rest.http_requester import HttpRequester
        self.global_conf = conf
        self.requester = HttpRequester(conf)
        self.home_folder = None
        # Optional compiler-flag-mapping hook read by CppInfo._evaluate_cond when a consumer
        # recipe reads a dependency's cflags/cxxflags/linkflags.  This system has no flags
        # plugin, so it stays None (flags are returned unchanged) — matching Conan's default.
        self.flags_map = None


class VersionResolvingRequirements:
    """Wraps the requirements object to accept bare package names (no version).

    Recipes in this system use ``self.requires("abseil")`` without a version.  The underlying
    Requirements model rejects that, so we intercept every call, look up the matching local
    recipe to find its version, and convert the ref to ``"abseil/20260107.1"`` before forwarding.
    """

    def __init__(self, inner, recipes_root: Path) -> None:
        self._inner = inner
        self._recipes_root = recipes_root

    def _resolve(self, ref: str) -> str:
        if ref and "/" not in ref and "@" not in ref:
            cls = try_load_recipe_class(self._recipes_root, ref)
            if cls:
                return f"{ref}/{resolve_version(cls)}"
        return ref

    def __call__(self, str_ref, **kwargs):
        resolved = self._resolve(str_ref)
        # Skip host requires with no local recipe (and no explicit version) — these are
        # system-provided libraries (e.g. libalsa/libudev/libusb on Linux) that this framework
        # does not vendor; the build links the system copy via find_package/pkg-config.
        if resolved and "/" not in resolved:
            return
        return self._inner(resolved, **kwargs)

    def tool_require(self, ref, **kwargs):
        resolved = self._resolve(ref)
        # Skip tool deps that have no local recipe and no explicit version — they are
        # system-provided tools (e.g. gperf, pkg-config) and cannot be version-resolved.
        # Passing an unversioned name to Requirements.tool_require() raises an error that
        # would abort the entire build_requirements() call, preventing later tool_requires
        # (e.g. meson) from being registered.
        if resolved and "/" not in resolved:
            return
        return self._inner.tool_require(resolved, **kwargs)

    def __len__(self):
        # Dunder lookups bypass __getattr__, so delegate explicitly (run_configure_method
        # uses len(recipe.requires) to detect requires added during configure()).
        return len(self._inner)

    def __getattr__(self, name):
        return getattr(self._inner, name)


def make_probe_recipe(
    recipe_cls: type[RecipeBase],
    recipes_root: Path,
    name: str,
    version: str,
    build_type: str,
    jobs: int | None = None,
    target_os: str | None = None,
    target_arch: str | None = None,
) -> RecipeBase:
    """Instantiate a recipe with just enough state (settings, conf, requires shim) to
    drive ``config_options()``/``configure()``/``requirements()``/``build_requirements()``.

    ``target_os``/``target_arch`` select the HOST/target platform (default: build machine).
    ``settings`` is the target platform; ``settings_build`` is always the build machine.
    No build folders are created — this is for dependency discovery only.  ``build.py``
    layers folder setup on top of this for actual builds.
    """
    recipe = recipe_cls(display_name=name)
    recipe.version = version
    recipe.recipe_folder = str(recipes_root / name)

    recipe.settings = detect_settings(build_type, target_os, target_arch)
    if target_os is None and target_arch is None:
        recipe.settings_build = recipe.settings
    else:
        recipe.settings_build = detect_settings(build_type)
    recipe.settings_target = None
    conf = make_conf(jobs=jobs)
    recipe.conf = conf
    recipe._recipe_runtime = RecipeRuntime(conf)
    recipe._recipe_dependencies = RecipeDependencies(OrderedDict())
    recipe._recipe_buildenv = Environment()
    recipe._recipe_runenv = Environment()
    recipe.requires = VersionResolvingRequirements(recipe.requires, recipes_root)
    # Give the probe a graph node so recipes can read self.ref / self.context during
    # config/requirements (e.g. cross-build recipes doing tool_requires(str(self.ref))).
    from thirdparty._internal.graph.graph import Node, CONTEXT_HOST, RECIPE_INCACHE
    recipe._recipe_node = Node(name, version, context=CONTEXT_HOST, recipe_state=RECIPE_INCACHE)
    # Mirror RecipeLoader: run the recipe's init() hook if it defines one.
    if hasattr(recipe, "init") and callable(recipe.init):
        recipe.init()
    return recipe


def load_python_file(recipe_path):
    """ From a given path, obtain the in memory python import module
    """
    with _load_python_lock:
        module, module_id = _load_python_file(recipe_path)
    return module, module_id


def _load_python_file(recipe_path):
    """ From a given path, obtain the in memory python import module
    """

    if not os.path.exists(recipe_path):
        raise NotFoundException("%s not found!" % recipe_path)

    def new_print(*args, **kwargs):  # Make sure that all user python files print() goes to stderr
        kwargs.setdefault("file", sys.stderr)
        print(*args, **kwargs)

    module_id = str(uuid.uuid1())
    current_dir = os.path.dirname(recipe_path)
    sys.path.insert(0, current_dir)
    try:
        old_modules = list(sys.modules.keys())
        with chdir(current_dir):
            old_dont_write_bytecode = sys.dont_write_bytecode
            try:
                sys.dont_write_bytecode = True
                spec = imp_util.spec_from_file_location(module_id, recipe_path)
                loaded = imp_util.module_from_spec(spec)
                spec.loader.exec_module(loaded)
                sys.dont_write_bytecode = old_dont_write_bytecode
            except ImportError:
                raise

        # These lines are necessary, otherwise local recipe imports with same name
        # collide, but no error, and overwrite other packages imports!!
        added_modules = set(sys.modules).difference(old_modules)
        for added in added_modules:
            module = sys.modules[added]
            if module:
                try:
                    try:
                        # Most modules will have __file__ != None
                        folder = os.path.dirname(module.__file__)
                    except (AttributeError, TypeError):
                        # But __file__ might not exist or equal None
                        # Like some builtins and Namespace packages py3
                        folder = module.__path__._path[0]
                except AttributeError:  # In case the module.__path__ doesn't exist
                    pass
                else:
                    if folder.startswith(current_dir):
                        module = sys.modules.pop(added)
                        module.print = new_print
                        sys.modules["%s.%s" % (module_id, added)] = module
    except RecipeException:
        raise
    except Exception:
        trace = traceback.format_exc().split('\n')
        raise RecipeException("Unable to load recipe in %s\n%s" % (recipe_path,
                                                                     '\n'.join(trace[3:])))
    finally:
        sys.path.pop(0)

    loaded.print = new_print
    return loaded, module_id


class RecipeTextLoader:
    """Parse a recipe.txt file"""

    def __init__(self, input_text):
        # Prefer composition over inheritance, the __getattr__ was breaking things
        self._config_parser = TextINIParse(input_text,  ["requires", "options",
                                                         "imports", "tool_requires", "test_requires",
                                                         "layout"],
                                           strip_comments=True)

    @property
    def layout(self):
        """returns the declared layout"""
        tmp = [r.strip() for r in self._config_parser.layout.splitlines()]
        if len(tmp) > 1:
            raise RecipeException("Only one layout can be declared in the [layout] section of "
                                 "the recipe.txt")
        return tmp[0] if tmp else None

    @property
    def requirements(self):
        """returns a list of requires
        EX:  "OpenCV/2.4.10@phil/stable"
        """
        return [r.strip() for r in self._config_parser.requires.splitlines()]

    @property
    def tool_requirements(self):
        """returns a list of tool_requires
        EX:  "OpenCV/2.4.10@phil/stable"
        """

        return [r.strip() for r in self._config_parser.tool_requires.splitlines()]

    @property
    def test_requirements(self):
        """returns a list of test_requires
        EX:  "gtest/2.4.10@phil/stable"
        """

        return [r.strip() for r in self._config_parser.test_requires.splitlines()]

    @property
    def options(self):
        return self._config_parser.options
