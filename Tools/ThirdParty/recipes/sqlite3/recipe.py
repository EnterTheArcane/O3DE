from thirdparty import RecipeBase
from thirdparty.tools.apple import is_apple_os
from thirdparty.tools.cmake import CMake, CMakeToolchain
from thirdparty.tools.files import get, load, save
import os


class Recipe(RecipeBase):
    name = "sqlite3"
    version = "3.53.1"
    license = "Unlicense"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "threadsafe": [0, 1, 2],
        "enable_column_metadata": [True, False],
        "enable_dbstat_vtab": [True, False],
        "enable_explain_comments": [True, False],
        "enable_fts3": [True, False],
        "enable_fts3_parenthesis": [True, False],
        "enable_fts4": [True, False],
        "enable_fts5": [True, False],
        "enable_icu": [True, False],
        "enable_json1": [True, False],
        "enable_memsys5": [True, False],
        "enable_soundex": [True, False],
        "enable_preupdate_hook": [True, False],
        "enable_rtree": [True, False],
        "use_alloca": [True, False],
        "use_uri": [True, False],
        "omit_load_extension": [True, False],
        "omit_deprecated": [True, False],
        "enable_math_functions": [True, False],
        "enable_unlock_notify": [True, False],
        "enable_default_secure_delete": [True, False],
        "disable_gethostuuid": [True, False],
        "max_column": [None, "ANY"],
        "max_variable_number": [None, "ANY"],
        "max_blob_size": [None, "ANY"],
        "build_executable": [True, False],
        "enable_default_vfs": [True, False],
        "enable_dbpage_vtab": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "threadsafe": 1,
        "enable_column_metadata": True,
        "enable_dbstat_vtab": False,
        "enable_explain_comments": False,
        "enable_fts3": False,
        "enable_fts3_parenthesis": False,
        "enable_fts4": False,
        "enable_fts5": False,
        "enable_icu": False,
        "enable_json1": False,
        "enable_memsys5": False,
        "enable_soundex": False,
        "enable_preupdate_hook": False,
        "enable_rtree": True,
        "use_alloca": False,
        "use_uri": False,
        "omit_load_extension": False,
        "omit_deprecated": False,
        "enable_math_functions": True,
        "enable_unlock_notify": True,
        "enable_default_secure_delete": False,
        "disable_gethostuuid": False,
        "max_column": None,  # Uses default value from source
        "max_variable_number": None,  # Uses default value from source
        "max_blob_size": None,  # Uses default value from source
        "build_executable": True,
        "enable_default_vfs": True,
        "enable_dbpage_vtab": False,
    }

    exports_sources = "CMakeLists.txt"

    def requirements(self) -> list[str]:
        return []  # icu is optional (enable_icu defaults to False)

    def source(self):
        get(
            url="https://sqlite.org/2026/sqlite-amalgamation-3530100.zip",
            dest=self.source_folder,
            sha256="36ad6e7f38540a3b21a2ac36340833f0a9e426bc1c752751c3ba669466827eae",
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["SQLITE3_SRC_DIR"] = self.source_folder.replace("\\", "/")
        tc.variables["SQLITE3_VERSION"] = self.version
        tc.variables["SQLITE3_BUILD_EXECUTABLE"] = self.options.build_executable
        tc.variables["THREADSAFE"] = self.options.threadsafe
        tc.variables["ENABLE_COLUMN_METADATA"] = self.options.enable_column_metadata
        tc.variables["ENABLE_DBSTAT_VTAB"] = self.options.enable_dbstat_vtab
        tc.variables["ENABLE_EXPLAIN_COMMENTS"] = self.options.enable_explain_comments
        tc.variables["ENABLE_FTS3"] = self.options.enable_fts3
        tc.variables["ENABLE_FTS3_PARENTHESIS"] = self.options.enable_fts3_parenthesis
        tc.variables["ENABLE_FTS4"] = self.options.enable_fts4
        tc.variables["ENABLE_FTS5"] = self.options.enable_fts5
        tc.variables["ENABLE_ICU"] = self.options.enable_icu
        tc.variables["ENABLE_JSON1"] = self.options.enable_json1
        tc.variables["ENABLE_MEMSYS5"] = self.options.enable_memsys5
        tc.variables["ENABLE_PREUPDATE_HOOK"] = self.options.enable_preupdate_hook
        tc.variables["ENABLE_SOUNDEX"] = self.options.enable_soundex
        tc.variables["ENABLE_RTREE"] = self.options.enable_rtree
        tc.variables["ENABLE_UNLOCK_NOTIFY"] = self.options.enable_unlock_notify
        tc.variables["ENABLE_DEFAULT_SECURE_DELETE"] = (
            self.options.enable_default_secure_delete
        )
        tc.variables["USE_ALLOCA"] = self.options.use_alloca
        tc.variables["USE_URI"] = self.options.use_uri
        tc.variables["OMIT_LOAD_EXTENSION"] = self.options.omit_load_extension
        tc.variables["OMIT_DEPRECATED"] = self.options.omit_deprecated
        tc.variables["ENABLE_MATH_FUNCTIONS"] = self.options.enable_math_functions
        tc.variables["HAVE_FDATASYNC"] = True
        tc.variables["HAVE_GMTIME_R"] = True
        tc.variables["HAVE_LOCALTIME_R"] = False  # Windows
        tc.variables["HAVE_POSIX_FALLOCATE"] = False  # Windows
        tc.variables["HAVE_STRERROR_R"] = True
        tc.variables["HAVE_USLEEP"] = True
        tc.variables["DISABLE_GETHOSTUUID"] = self.options.disable_gethostuuid
        if self.options.max_column:
            tc.variables["MAX_COLUMN"] = self.options.max_column
        if self.options.max_variable_number:
            tc.variables["MAX_VARIABLE_NUMBER"] = self.options.max_variable_number
        if self.options.max_blob_size:
            tc.variables["MAX_BLOB_SIZE"] = self.options.max_blob_size
        tc.variables["DISABLE_DEFAULT_VFS"] = not self.options.enable_default_vfs
        tc.variables["ENABLE_DBPAGE_VTAB"] = self.options.enable_dbpage_vtab
        tc.generate()

    def build(self):
        import shutil

        shutil.copy(
            os.path.join(os.path.dirname(os.path.abspath(__file__)), "CMakeLists.txt"),
            os.path.normpath(
                os.path.join(self.source_folder, os.pardir, "CMakeLists.txt")
            ),
        )
        cmake = CMake(self)
        cmake.configure(build_script_folder=os.path.join(self.source_folder, os.pardir))
        cmake.build()

    def _extract_license(self):
        header = load(os.path.join(self.source_folder, "sqlite3.h"))
        license_content = header[3 : header.find("***", 1)]
        return license_content

    def package(self):
        save(
            os.path.join(self.package_folder, "licenses", "LICENSE"),
            self._extract_license(),
        )
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # Qt 6 uses INPUT_sqlite="system" which calls find_package(SQLite3) and
        # expects the SQLite::SQLite3 imported target.
        self.cpp_info.libs = ["sqlite3"]
        self.cpp_info.set_property("cmake_file_name", "SQLite3")
        self.cpp_info.set_property("cmake_target_name", "SQLite::SQLite3")
