from thirdparty.cmake.toolchain.toolchain import CMakeToolchain
from thirdparty.cmake.cmake import CMake
from thirdparty.cmake.cmakeconfigdeps.cmakeconfigdeps import CMakeConfigDeps
from thirdparty.cmake.layout import cmake_layout


def CMakeDeps(recipe):  # noqa
    if recipe.conf.get("tools.cmake.cmakedeps:new",
                          choices=["will_break_next", "recipe_will_break"]) == "will_break_next":
        recipe.output.warning("On the fly replacement of CMakeDeps by CMakeConfigDeps generator, "
                                 "because 'tools.cmake.cmakedeps:new' incubating conf activated. "
                                 "This conf is incubating and will break in next releases. "
                                 "CMakeConfigDeps is now experimental and can be used as such in "
                                 "recipes.")
        return CMakeConfigDeps(recipe)
    from thirdparty.cmake.cmakedeps.cmakedeps import CMakeDeps as _CMakeDeps
    return _CMakeDeps(recipe)
