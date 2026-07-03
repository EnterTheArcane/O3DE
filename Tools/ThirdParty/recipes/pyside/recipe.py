import os

from thirdparty import RecipeBase, RecipeOptions
from thirdparty.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.files import copy, get
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class _Options(RecipeOptions):
    shared: bool = True
    pic: bool = True


class Recipe(RecipeBase[_Options]):
    name = "pyside"
    version = "6.11.1"
    license = "LGPL-3.0-only"

    def latest_version(self):
        repo = GithubRepository(self, "qtproject/pyside-pyside-setup")
        return Version(repo.latest_release.removeprefix("v"))

    def requirements(self):
        self.requires_tool("cmake")
        self.requires_tool("cpython")
        self.requires("cpython")
        self.requires("llvm")
        self.requires("qt")

    def source(self):
        version_major = Version(self.version).major
        get(
            self,
            url=f"https://download.qt.io/official_releases/QtForPython/pyside{version_major}/PySide{version_major}-{self.version}-src/pyside-setup-everywhere-src-{self.version}.tar.xz",
            sha256="6ffd9835bb0dd2c56f061d62f1616bb1707cfc0202b80e3165d6be087f3965e2",
            destination=self.folders.source,
            strip_root=True)

    def generate(self):
        llvm_pkg = self.dependencies["llvm"].folders.package
        cpython_pkg = self.dependencies["cpython"].folders.package
        qt_pkg = self.dependencies["qt"].folders.package

        tc = CMakeToolchain(self)
        tc.variables["BUILD_TESTS"] = False
        tc.variables["INSTALL_TESTS"] = False
        tc.variables["CMAKE_VERBOSE_MAKEFILE"] = False
        tc.variables["FORCE_LIMITED_API"] = "no"
        if self.settings.os == "Windows":
            tc.cache_variables["CMAKE_LINKER_TYPE"] = "MSVC"
            tc.variables["DISABLE_PYI"] = True

        tc.variables["CLANG_INSTALL_DIR"] = llvm_pkg.as_posix()
        tc.variables["Clang_DIR"] = (llvm_pkg / "lib" / "cmake" / "clang").as_posix()

        python_root = cpython_pkg.as_posix()
        tc.variables["Python3_ROOT_DIR"] = python_root
        tc.variables["Python3_FIND_STRATEGY"] = "LOCATION"
        # Shiboken uses find_package(Python ...) (unversioned), so set Python_ variables too.
        tc.variables["Python_ROOT_DIR"] = python_root
        tc.variables["Python_FIND_STRATEGY"] = "LOCATION"
        cpython_ver = str(self.dependencies["cpython"].version)
        ver_parts = cpython_ver.split(".")
        py_maj, py_min = ver_parts[0], ver_parts[1]
        py_exe = f"{python_root}/bin/python.exe" if self.settings.os == "Windows" else f"{python_root}/bin/python{py_maj}.{py_min}"
        py_inc = f"{python_root}/bin/include" if self.settings.os == "Windows" else f"{python_root}/include/python{py_maj}.{py_min}"
        py_lib = f"{python_root}/bin/libs/python{py_maj}{py_min}.lib" if self.settings.os == "Windows" else f"{python_root}/lib/libpython{py_maj}.{py_min}.so"
        tc.variables["Python_EXECUTABLE"] = py_exe
        tc.variables["Python3_EXECUTABLE"] = py_exe
        tc.variables["Python_INCLUDE_DIR"] = py_inc
        tc.variables["Python3_INCLUDE_DIR"] = py_inc
        tc.variables["Python_LIBRARY"] = py_lib
        tc.variables["Python3_LIBRARY"] = py_lib
        if self.settings.os == "Windows":
            tc.variables["Python3_FIND_REGISTRY"] = "NEVER"
            tc.variables["Python_FIND_REGISTRY"] = "NEVER"

        qt_version = str(self.dependencies["qt"].version)
        qt_pkg_fwd = qt_pkg.as_posix()
        qt_cmake_dir = (qt_pkg / "lib" / "cmake" / "Qt6").as_posix()
        tc.variables["Qt6Core_VERSION"] = qt_version
        tc.variables["QT6_INSTALL_PREFIX"] = qt_pkg_fwd
        tc.variables["QT6_INSTALL_BINS"] = "bin"
        tc.variables["QT6_INSTALL_LIBS"] = "lib"
        tc.variables["QT6_INSTALL_LIBEXECS"] = "bin"
        # Use Qt6's own native cmake configs (not CMakeDeps-generated) so that
        # find_package(Qt6 REQUIRED COMPONENTS Core) properly sets Qt6Core_FOUND
        # and creates all Qt6-specific target properties (e.g. QT_DARWIN_MIN_DEPLOYMENT_TARGET).
        tc.variables["Qt6_DIR"] = qt_cmake_dir
        # Don't override CMAKE_PREFIX_PATH - let CMakeToolchain set it to include all dependencies
        # (zlib, pcre2, etc.) that Qt's FindWrap*.cmake modules need to find
        tc.variables["QT_DEBUG_FIND_PACKAGE"] = "ON"
        # Prevent Qt6GuiConfig from auto-loading plugins (e.g. QTuioTouch → Qt6Network
        # → WrapOpenSSL::WrapOpenSSL) which would cause "target not found" errors.
        tc.variables["QT_SKIP_AUTO_PLUGIN_INCLUSION"] = "ON"

        if self.settings.os == "Mac":
            llvm_lib = (llvm_pkg / "lib").as_posix()
            # LLVM's libc++ does NOT re-export libc++abi symbols (unlike system libc++).
            # Without explicit -lc++abi, std::length_error and similar symbols are attributed
            # to libc++ at link time (via the system SDK stub), but LLVM's libc++.1.dylib
            # does not export them → runtime crash when DYLD_LIBRARY_PATH loads LLVM's libc++.
            # Explicitly linking -lc++abi ensures symbols are attributed to libc++abi, which
            # does export them. CMAKE_BUILD_RPATH ensures shiboken6 can find libc++abi at
            # runtime during the cmake build's code-generation step.
            tc.variables["CMAKE_EXE_LINKER_FLAGS"] = f"-L{llvm_lib} -lc++abi"
            tc.variables["CMAKE_SHARED_LINKER_FLAGS"] = f"-L{llvm_lib} -lc++abi"
            tc.variables["CMAKE_BUILD_RPATH"] = llvm_lib

        if self.settings.os == "Windows":
            # Qt6::Gui statically links the vendored harfbuzz, whose Uniscribe backend
            # (hb-uniscribe.cc) references Windows system symbols (ScriptItemize/ScriptShape/
            # ScriptPlace/ScriptFreeCache from usp10, UuidCreate from rpcrt4, plus gdi32/user32).
            # harfbuzz declares these in its info system_libs, but Qt6's own cmake config
            # (used here via cmake_find_mode=none) does not propagate them, so PySide6's
            # bindings fail to link with LNK2019.  Add them explicitly.  PySide6's .pyd modules
            # are CMake MODULE libraries, so CMAKE_MODULE_LINKER_FLAGS is the one that matters;
            # SHARED/EXE are set too for completeness.  Use /DEFAULTLIB: (not bare names) so the
            # linker searches them LAST — bare names land at the start of the link line, before
            # harfbuzz.lib, so MSVC's single-pass import-lib resolution would still miss the
            # symbols (classic link-order issue).
            hb_system_libs = ("/DEFAULTLIB:usp10.lib /DEFAULTLIB:rpcrt4.lib "
                              "/DEFAULTLIB:gdi32.lib /DEFAULTLIB:user32.lib")
            tc.variables["CMAKE_MODULE_LINKER_FLAGS"] = hb_system_libs
            tc.variables["CMAKE_SHARED_LINKER_FLAGS"] = hb_system_libs
            tc.variables["CMAKE_EXE_LINKER_FLAGS"] = hb_system_libs

        tc.generate()

        fix_script_content = (
            "get_property(_bst DIRECTORY . PROPERTY BUILDSYSTEM_TARGETS)\n"
            "foreach(_t IN LISTS _bst)\n"
            "    if(TARGET ${_t})\n"
            "        get_target_property(_type ${_t} TYPE)\n"
            '        if(NOT _type STREQUAL "INTERFACE_LIBRARY")\n'
            '            set_property(TARGET ${_t} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")\n'
            "        endif()\n"
            "    endif()\n"
            "endforeach()\n"
            "unset(_bst)\n"
            "unset(_t)\n"
            "unset(_type)\n"
        )
        fix_script_path = self.folders.generators / "fix_msvc_runtime.cmake"
        fix_script_fwd = fix_script_path.as_posix()
        with open(fix_script_path, "w") as f:
            f.write(fix_script_content)

        helper_content = (
            "if(MSVC)\n"
            '    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")\n'
            '    string(REGEX REPLACE "/M[TtDd]+" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")\n'
            '    string(STRIP "${CMAKE_CXX_FLAGS_RELEASE}" CMAKE_CXX_FLAGS_RELEASE)\n'
            '    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")\n'
            '    string(REGEX REPLACE "/M[TtDd]+" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")\n'
            '    string(STRIP "${CMAKE_C_FLAGS_RELEASE}" CMAKE_C_FLAGS_RELEASE)\n'
            '    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MD")\n'
            f'    cmake_language(DEFER DIRECTORY "${{CMAKE_CURRENT_SOURCE_DIR}}" CALL\n'
            f'        include "{fix_script_fwd}")\n'
            "endif()\n"
            # NOTE: do not pre-define Qt6::PlatformCommonInternal here. Qt 6.11.1 exports it
            # as part of its platform target export set; defining it before find_package(Qt6)
            # makes Qt's _qt_internal_check_multiple_inclusion fail ("some but not all targets
            # already defined"). Let Qt's own config create it (with the right MSVC flags).
            "# Stub target for WrapOpenSSL so Qt6NetworkTargets generate step succeeds\n"
            "# when Qt6Network is transitively found but OpenSSL find modules are not loaded.\n"
            "if(NOT TARGET WrapOpenSSL::WrapOpenSSL)\n"
            "    add_library(WrapOpenSSL::WrapOpenSSL INTERFACE IMPORTED)\n"
            "    find_package(OpenSSL QUIET)\n"
            "    if(TARGET OpenSSL::SSL)\n"
            "        target_link_libraries(WrapOpenSSL::WrapOpenSSL INTERFACE OpenSSL::SSL OpenSSL::Crypto)\n"
            "    endif()\n"
            "endif()\n"
        )
        helper_path = self.folders.generators / "qt_pyside6_internal_targets.cmake"
        with open(helper_path, "w") as f:
            f.write(helper_content)
        toolchain_path = self.folders.generators / "recipe_toolchain.cmake"
        with open(toolchain_path, "a") as f:
            f.write(f'\nset(CMAKE_PROJECT_INCLUDE "{helper_path.as_posix()}")\n')

        deps = CMakeDeps(self)
        deps.set_property("llvm", "cmake_find_mode", "none")
        deps.set_property("cpython", "cmake_find_mode", "none")
        # Qt has its own cmake config files that properly set Qt6Core_FOUND and
        # Qt6-specific target properties. CMakeDeps-generated Qt6Config.cmake would
        # create Qt6::Core as an IMPORTED target but NOT set Qt6Core_FOUND, causing
        # ShibokenHelpers.cmake to fatal-error on macOS. Use Qt's native cmake instead.
        deps.set_property("qt", "cmake_find_mode", "none")
        deps.generate()

    def build(self):
        if self.settings.os == "Mac":
            self._patch_qtcore_cmake()
            self._patch_pyside_tools_cmake()
        cmake = CMake(self)
        cmake.configure()
        if self.settings.os == "Mac":
            self._patch_shiboken_wrapper()
        # Build shibokenmodule first and stage it to the package prefix.
        # PySide6's pyi stub generation (QtCore_pyi etc.) imports shiboken6 from
        # PYTHON_SITE_PACKAGES at cmake build time, but shiboken6 is only installed
        # there during cmake --install (i.e. package()). Pre-staging the just-built
        # shibokenmodule lets the pyi targets find it before install runs.
        cmake.build(target="shibokenmodule")
        self._stage_shiboken6_for_pyi()
        cmake.build()

    def package(self):
        copy(self, "LICENSE.FDL", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(self, "LICENSE.GPL2", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(self, "LICENSE.GPL3", src=self.folders.source, dst=self.folders.package / "licenses")
        copy(self, "LICENSE.LGPL3", src=self.folders.source, dst=self.folders.package / "licenses")
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.info.set_property("cmake_find_mode", "none")
        self.info.builddirs = [
            os.path.join("lib", "cmake", "Shiboken6"),
            os.path.join("lib", "cmake", "PySide6"),
        ]

        # Shiboken6 runtime library
        shiboken = self.info.components["shiboken6"]
        shiboken.set_property("cmake_file_name", "Shiboken6")
        shiboken.set_property("cmake_target_name", "Shiboken6::libshiboken")
        shiboken.libs = ["shiboken6"]
        shiboken.includedirs = [os.path.join("include", "shiboken6")]
        shiboken.requires = ["cpython::cpython"]
        if self.settings.os in ("Linux", "FreeBSD"):
            shiboken.system_libs = ["pthread", "dl"]

        # Expose the shiboken6 generator location via conf
        self.info.conf.define(
            "user.pyside:shiboken6_generator",
            (self.folders.package / "bin" / "shiboken6").as_posix())
        self.info.conf.define(
            "user.pyside:pyside_dir",
            self.folders.package.as_posix())
        self.info.conf.define(
            "user.pyside6:shiboken6_generator",
            (self.folders.package / "bin" / "shiboken6").as_posix())
        self.info.conf.define(
            "user.pyside6:pyside6_dir",
            self.folders.package.as_posix())

        bin_dir = self.folders.package / "bin"
        self.info.buildenv.prepend_path("PATH", bin_dir)

    def _patch_qtcore_cmake(self):
        """Force-load Darwin permission plugin archives and weak-link their frameworks.

        PySide6's qtcore.cpp glue file contains Q_IMPORT_PLUGIN calls for 5 Darwin
        permission plugins (Camera, Microphone, Bluetooth, Contacts, Calendar). These
        expand to static initializer objects that call qt_static_plugin_QDarwin*Plugin().
        Those symbols are defined in the permission .a archives.

        The macOS linker with -undefined dynamic_lookup allows the undefined references at
        link time, but at dlopen time (RTLD_NOW) the flat-namespace lookup fails because
        the archives are not included in the link by default (QT_SKIP_AUTO_PLUGIN_INCLUSION
        is ON, so no cmake genex includes them either).

        Fix:
        1. -force_load each archive so the qt_static_plugin_* symbols are defined.
        2. -weak_framework for each framework the archives depend on, so their ObjC class
           symbols are optional at dlopen time. dyld sets them to NULL if not found instead
           of raising a symbol-not-found error. (The frameworks exist on macOS so they will
           be loaded; weak just prevents hard failure if somehow absent.)
        """
        cmake_path = self.folders.source / "sources" / "pyside6" / "PySide6" / "QtCore" / "CMakeLists.txt"
        with open(cmake_path, "r") as f:
            content = f.read()
        if "_pyside6_qtcore_permplugin_patched" in content:
            return  # Already patched (idempotent on retry)
        patch = (
            "\n# Recipe patch: force-load Darwin permission plugin archives into QtCore.so to\n"
            "# satisfy qt_static_plugin_QDarwin*Plugin symbols referenced by Q_IMPORT_PLUGIN\n"
            "# calls in PySide6's qtcore.cpp glue file. Also weak-link the Apple frameworks\n"
            "# so their ObjC class symbols are optional at dlopen time.\n"
            "set(_pyside6_qtcore_permplugin_patched TRUE)  # recipe patch sentinel\n"
            "if(APPLE AND TARGET QtCore)\n"
            "    file(GLOB _pyside6_perm_libs\n"
            '        "${QT6_INSTALL_PREFIX}/plugins/permissions/libqdarwin*.a")\n'
            "    foreach(_perm_lib IN LISTS _pyside6_perm_libs)\n"
            '        target_link_options(QtCore PRIVATE "LINKER:-force_load,${_perm_lib}")\n'
            "    endforeach()\n"
            "    target_link_options(QtCore PRIVATE\n"
            '        "LINKER:-weak_framework,AVFoundation"\n'
            '        "LINKER:-weak_framework,Contacts"\n'
            '        "LINKER:-weak_framework,CoreBluetooth"\n'
            '        "LINKER:-weak_framework,CoreLocation"\n'
            '        "LINKER:-weak_framework,EventKit"\n'
            "    )\n"
            "endif()\n"
        )
        with open(cmake_path, "a") as f:
            f.write(patch)

    def _patch_pyside_tools_cmake(self):
        """Guard .app bundle directory installs with existence checks.

        pyside-tools/CMakeLists.txt unconditionally installs Assistant.app,
        Designer.app, and Linguist.app from Qt's bin directory. These .app bundles
        are optional Qt GUI tools that are not built in our headless Qt package.
        Without the existence guard, cmake --install fails with 'file INSTALL cannot
        find' for each missing .app bundle.
        """
        cmake_path = self.folders.source / "sources" / "pyside-tools" / "CMakeLists.txt"
        with open(cmake_path, "r") as f:
            content = f.read()
        old = (
            "    foreach(directory ${directories})\n"
            "        install(DIRECTORY \"${directory}\"\n"
            "                DESTINATION bin\n"
            "                FILE_PERMISSIONS\n"
            "                OWNER_EXECUTE OWNER_WRITE OWNER_READ\n"
            "                GROUP_EXECUTE GROUP_READ\n"
            "                WORLD_EXECUTE WORLD_READ\n"
            "                PATTERN \"android_utilities.py\" EXCLUDE) # excluding the symlink\n"
            "    endforeach()"
        )
        new = (
            "    foreach(directory ${directories})\n"
            "        if(EXISTS \"${directory}\")\n"
            "        install(DIRECTORY \"${directory}\"\n"
            "                DESTINATION bin\n"
            "                FILE_PERMISSIONS\n"
            "                OWNER_EXECUTE OWNER_WRITE OWNER_READ\n"
            "                GROUP_EXECUTE GROUP_READ\n"
            "                WORLD_EXECUTE WORLD_READ\n"
            "                PATTERN \"android_utilities.py\" EXCLUDE) # excluding the symlink\n"
            "        endif()\n"
            "    endforeach()"
        )
        if old not in content:
            return  # Already patched or source changed
        with open(cmake_path, "w") as f:
            f.write(content.replace(old, new, 1))

    def _patch_shiboken_wrapper(self):
        """Remove LLVM's lib dir from DYLD_LIBRARY_PATH in shiboken_wrapper.sh.

        PySide6's cmake sets DYLD_LIBRARY_PATH to include LLVM's lib dir so that
        shiboken6 can find Qt/LLVM libs at runtime. However, this causes
        libclang.dylib's hard-coded dependency on /usr/lib/libc++.1.dylib to be
        intercepted by LLVM's libc++.1.dylib (via the libc++.1.dylib -> libc++.1.0.dylib
        symlink in LLVM's lib dir). LLVM's libc++ does NOT re-export ___cxa_guard_release
        (that symbol is defined in libc++abi), so dyld aborts with "Symbol not found".

        shiboken6 and libclang already use @rpath to find LLVM libs, so removing
        LLVM's lib dir from DYLD_LIBRARY_PATH is safe.
        """
        import glob
        llvm_pkg = self.dependencies["llvm"].folders.package
        llvm_lib = llvm_pkg / "lib"

        search_pattern = (self.folders.build / "**" / "shiboken_wrapper.sh").as_posix()
        found = list(glob.glob(search_pattern, recursive=True, include_hidden=True))

        for wrapper_path in found:
            with open(wrapper_path, "r") as f:
                content = f.read()
            # Remove LLVM lib dir from colon-separated path lists, handling
            # it appearing at the start, middle, or end of the value.
            patched = (
                content
                .replace(f":{llvm_lib}:", ":")
                .replace(f"{llvm_lib}:", "")
                .replace(f":{llvm_lib}", "")
            )
            if patched != content:
                with open(wrapper_path, "w") as f:
                    f.write(patched)

    def _stage_shiboken6_for_pyi(self):
        """Copy shiboken6 Python module to the package prefix for pyi stub generation.

        PySide6's cmake pyi targets (e.g. QtCore_pyi) run generate_pyi.py, which
        does 'import shiboken6' with PYTHON_SITE_PACKAGES as a sys.path entry.
        PYTHON_SITE_PACKAGES resolves to {package_folder}/lib/pythonX.Y/site-packages/
        (the install destination), which is empty until cmake --install runs in
        package(). By copying the just-built shiboken6 files there first, we let
        the pyi targets succeed during cmake --build.
        """
        import glob
        import shutil

        cpython_ver = str(self.dependencies["cpython"].version)
        py_maj, py_min = cpython_ver.split(".")[:2]

        # After cmake.build(target="shibokenmodule"), the shiboken6 Python package
        # lives in {build_folder}/sources/shiboken6/ because shibokenmodule sets
        # LIBRARY_OUTPUT_DIRECTORY to ${CMAKE_CURRENT_BINARY_DIR}/.. (i.e. one level
        # above the shibokenmodule build dir).
        src_dir = self.folders.build / "sources" / "shiboken6"
        # PYTHON_SITE_PACKAGES (where generate_pyi.py imports shiboken6 from) follows the
        # interpreter's layout: Windows cpython uses lib/site-packages, while Unix uses
        # lib/pythonX.Y/site-packages.
        if self.settings.os == "Windows":
            site_pkgs = self.folders.package / "lib" / "site-packages"
        else:
            site_pkgs = self.folders.package / "lib" / f"python{py_maj}.{py_min}" / "site-packages"
        dst_dir = site_pkgs / "shiboken6"
        os.makedirs(dst_dir, exist_ok=True)

        # Copy Python files and extension modules from the shiboken6 build output dir.
        patterns = [
            src_dir / "*.py",
            src_dir / "*.pyi",
            src_dir / "py.typed",
            src_dir / "Shiboken*.so",  # macOS / Linux Python extension
            src_dir / "Shiboken*.dylib",  # alternate macOS extension name
            src_dir / "Shiboken*.pyd",  # Windows Python extension
        ]
        for pattern in patterns:
            for src_file in glob.glob(pattern.as_posix()):
                shutil.copy2(src_file, dst_dir / os.path.basename(src_file))

        # _config.py is generated in the shibokenmodule subdirectory, not the parent dir.
        config_src = src_dir / "shibokenmodule" / "_config.py"
        if os.path.exists(config_src):
            shutil.copy2(config_src, dst_dir / "_config.py")
