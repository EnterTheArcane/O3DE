import traceback
from importlib import invalidate_caches, util as imp_util
import inspect
import os
import sys
import types
import uuid
from threading import Lock

import yaml

from pathlib import Path

from thirdparty._internal.api.output import Output
from thirdparty.cmake import cmake_layout
from thirdparty.google import bazel_layout
from thirdparty.microsoft import vs_layout
from thirdparty._internal.errors import recipe_exception_formatter, NotFoundException
from thirdparty.errors import RecipeException
from thirdparty._internal.model.recipe_base import RecipeBase
from thirdparty._internal.model.options import Options
from thirdparty._internal.model.refs import RecipeReference
from thirdparty._internal.paths import DATA_YML
from thirdparty._internal.util.config_parser import TextINIParse
from thirdparty._internal.util.files import load, chdir, load_user_encoded


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

            # Load and populate dynamic fields from the data file
            recipe_data = self._load_data(recipe_path)
            recipe.recipe_data = recipe_data

            self._cached_recipe_classes[recipe_path] = (recipe, module)
            result = recipe(display)

            result._recipe_runtime = self._recipe_helpers
            if hasattr(result, "init") and callable(result.init):
                with recipe_exception_formatter(result, "init"):
                    result.init()
            return result, module
        except RecipeException as e:
            raise RecipeException("Error loading recipe at '{}': {}".format(recipe_path, e))

    @staticmethod
    def _load_data(recipe_path):
        data_path = os.path.join(os.path.dirname(recipe_path), DATA_YML)
        if not os.path.exists(data_path):
            return None

        try:
            data = yaml.safe_load(load(data_path))
        except Exception as e:
            raise RecipeException("Invalid yml format at {}: {}".format(DATA_YML, e))

        return data or {}

    def load_named(self, recipe_path, name, version, user, channel, graph_lock=None,
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

        if user:
            if recipe.user and user != recipe.user:
                raise RecipeException("Package recipe with user %s!=%s"
                                     % (user, recipe.user))
            recipe.user = user

        if channel:
            if recipe.channel and channel != recipe.channel:
                raise RecipeException("Package recipe with channel %s!=%s"
                                     % (channel, recipe.channel))
            recipe.channel = channel

        if recipe.channel and not recipe.user:
            raise RecipeException(f"{recipe_path}: Can't specify channel '{recipe.channel}' without user")

        if hasattr(recipe, "set_name"):
            with recipe_exception_formatter("recipe.py", "set_name"):
                recipe.set_name()
        if hasattr(recipe, "set_version"):
            with recipe_exception_formatter("recipe.py", "set_version"):
                recipe.set_version()

        return recipe

    def load_export(self, recipe_path, name, version, user, channel, graph_lock=None):
        """ loads the recipe and evaluates its name, version, and enforce its existence
        """
        recipe = self.load_named(recipe_path, name, version, user, channel, graph_lock)
        if not recipe.name:
            raise RecipeException("recipe didn't specify name")
        if not recipe.version:
            raise RecipeException("recipe didn't specify version")

        ref = RecipeReference(recipe.name, recipe.version, recipe.user, recipe.channel)
        recipe.display_name = str(ref)
        return recipe

    def load_consumer(self, recipe_path, name=None, version=None, user=None,
                      channel=None, graph_lock=None, update=None, check_update=None,
                      tested_python_requires=None):
        """ loads a recipe.py in user space. Might have name/version or not
        """
        recipe = self.load_named(recipe_path, name, version, user, channel, graph_lock,
                                    update, check_update,
                                    tested_python_requires=tested_python_requires)

        ref = RecipeReference(recipe.name, recipe.version, recipe.user, recipe.channel)
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
        recipe.user = ref.user
        recipe.channel = ref.channel
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

        recipe.generators = parser.generators
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
        recipe.generators = []  # remove the default txt generator
        return recipe


def _parse_module(recipe_module, module_id):
    """ Parses a python in-memory module, to extract the classes, mainly the main
    class defining the Recipe, but also process possible existing generators
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
        self._config_parser = TextINIParse(input_text,  ["requires", "generators", "options",
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

    @property
    def generators(self):
        return self._config_parser.generators.splitlines()



