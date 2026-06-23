import traceback
from collections import OrderedDict
from importlib import util as imp_util
import inspect
import os
import sys
import uuid
from threading import Lock

from pathlib import Path

from thirdparty._internal.errors import NotFoundException
from thirdparty.errors import RecipeException
from thirdparty._internal.model.recipe_base import RecipeBase
from thirdparty._internal.util.files import chdir
from thirdparty._internal.util.detect import detect_settings, make_conf
from thirdparty._internal.model.dependencies import RecipeDependencies
from thirdparty.env import Environment


class RecipeLoader:
    """Loads and caches recipe CLASSES from ``recipes/<name>/recipe.py``.

    The parsed recipe class is cached per path, so repeated lookups — version injection
    during ``requirements()``, the config-probe, graph resolution — don't re-parse the same
    file.  Instantiation is left to the caller (``make_probe_recipe`` / build.py), since each
    build/probe needs a fresh recipe object with its own folders + settings.
    """

    def __init__(self):
        self._cache: dict[str, type[RecipeBase] | None] = {}

    def load_class(self, recipes_root: Path, name: str) -> type[RecipeBase] | None:
        recipe_path = Path(recipes_root) / name / "recipe.py"
        key = str(recipe_path)
        if key not in self._cache:
            self._cache[key] = self._parse_class(recipe_path)
        return self._cache[key]

    @staticmethod
    def _parse_class(recipe_path: Path) -> type[RecipeBase] | None:
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


# Shared loader instance: caches parsed recipe classes so repeated lookups (version
# injection during requirements(), the config-probe, graph resolution) don't re-parse the
# same recipe.py file.
_RECIPE_LOADER = RecipeLoader()


def try_load_recipe_class(recipes_root: Path, name: str) -> type[RecipeBase] | None:
    """Load (and cache) the recipe class from ``recipes/<name>/recipe.py``.

    Delegates to the shared :class:`RecipeLoader`, which parses the file under a lock with a
    unique module id, inserts the recipe directory on ``sys.path`` so recipes can import
    sibling helpers, validates that exactly one RecipeBase subclass is defined, and caches
    the result.  Returns ``None`` if the recipe is missing or fails to load/validate.
    """
    return _RECIPE_LOADER.load_class(recipes_root, name)


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

