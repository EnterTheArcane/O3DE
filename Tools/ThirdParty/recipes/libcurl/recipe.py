import os

from thirdparty import RecipeBase
from thirdparty.apple import is_apple_os
from thirdparty.cmake import CMake, CMakeToolchain, CMakeConfigDeps
from thirdparty.env import VirtualBuildEnv
from thirdparty.files import copy, download, get, replace_in_file, rmdir
from thirdparty.microsoft import is_msvc
from thirdparty.scm import Version
from thirdparty.scm.github import GithubRepository


class Recipe(RecipeBase):
    name = "libcurl"
    version = "8.20.0"
    license = "curl"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "build_executable": [True, False],
        "with_ssl": [False, "openssl", "wolfssl", "schannel", "mbedtls", "libressl"],
        "with_file": [True, False],
        "with_ftp": [True, False],
        "with_http": [True, False],
        "with_ldap": [True, False],
        "with_rtsp": [True, False],
        "with_dict": [True, False],
        "with_telnet": [True, False],
        "with_tftp": [True, False],
        "with_pop3": [True, False],
        "with_imap": [True, False],
        "with_smb": [True, False],
        "with_smtp": [True, False],
        "with_gopher": [True, False],
        "with_mqtt": [True, False],
        "with_libssh2": [True, False],
        "with_libidn": [True, False],
        "with_libgsasl": [True, False],
        "with_libpsl": [True, False],
        "with_largemaxwritesize": [True, False],
        "with_nghttp2": [True, False],
        "with_zlib": [True, False],
        "with_brotli": [True, False],
        "with_zstd": [True, False],
        "with_c_ares": [True, False],
        "with_threaded_resolver": [True, False],
        "with_proxy": [True, False],
        "with_crypto_auth": [True, False],
        "with_ntlm": [True, False],
        "with_cookies": [True, False],
        "with_ipv6": [True, False],
        "with_docs": [True, False],
        "with_misc_docs": [True, False],
        "with_verbose_debug": [True, False],
        "with_symbol_hiding": [True, False],
        "with_unix_sockets": [True, False],
        "with_verbose_strings": [True, False],
        "with_ca_bundle": [False, "auto", "ANY"],
        "with_ca_path": [False, "auto", "ANY"],
        "with_ca_fallback": [True, False],
        "with_form_api": [True, False],
        "with_websockets": [True, False],
        "with_apple_sectrust": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "build_executable": False,
        "with_ssl": "openssl",
        "with_dict": True,
        "with_file": True,
        "with_ftp": True,
        "with_gopher": True,
        "with_http": True,
        "with_imap": True,
        "with_ldap": False,
        "with_mqtt": True,
        "with_pop3": True,
        "with_rtsp": True,
        "with_smb": False,
        "with_smtp": True,
        "with_telnet": True,
        "with_tftp": True,
        "with_libssh2": False,
        "with_libidn": False,
        "with_libgsasl": False,
        "with_libpsl": False,
        "with_largemaxwritesize": False,
        "with_nghttp2": False,
        "with_zlib": True,
        "with_brotli": False,
        "with_zstd": False,
        "with_c_ares": False,
        "with_threaded_resolver": True,
        "with_proxy": True,
        "with_crypto_auth": True,
        "with_ntlm": False,
        "with_cookies": True,
        "with_ipv6": True,
        "with_docs": False,
        "with_misc_docs": False,
        "with_verbose_debug": True,
        "with_symbol_hiding": False,
        "with_unix_sockets": True,
        "with_verbose_strings": True,
        "with_ca_bundle": "auto",
        "with_ca_path": "auto",
        "with_ca_fallback": False,
        "with_form_api": True,
        "with_websockets": True,
        "with_apple_sectrust": False,
    }

    @property
    def _is_mingw(self):
        return self.settings.os == "Windows" and self.settings.compiler == "gcc"

    @property
    def _is_win_x_android(self):
        return self.settings.os == "Android" and self.settings_build.os == "Windows"

    def config_options(self):
        del self.options.with_libgsasl
        if self.settings.os == "Windows":
            del self.options.fPIC
        if not is_apple_os(self):
            del self.options.with_apple_sectrust

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def requirements(self):
        if self.options.with_ssl == "openssl":
            self.requires("openssl")
        elif self.options.with_ssl == "libressl":
            self.requires("libressl")
        elif self.options.with_ssl == "wolfssl":
            self.requires("wolfssl")
        elif self.options.with_ssl == "mbedtls":
            self.requires("mbedtls")
        if self.settings.os == "Linux" and self.options.with_ldap:
            self.requires("openldap")
        if self.options.with_nghttp2:
            self.requires("libnghttp2")
        if self.options.with_libssh2:
            self.requires("libssh2")
        if self.options.with_zlib:
            self.requires("zlib")
        if self.options.with_brotli:
            self.requires("brotli")
        if self.options.with_zstd:
            self.requires("zstd")
        if self.options.with_c_ares:
            self.requires("c-ares")
        if self.options.get_safe("with_libpsl"):
            self.requires("libpsl")
        if self.options.with_libidn:
            self.requires("libidn2")

    def build_requirements(self):
        if self._is_using_cmake_build:
            self.tool_requires("cmake")
            if self._is_win_x_android:
                self.tool_requires("ninja")
        else:
            self.tool_requires("libtool")
            if not self.conf.get("tools.gnu:pkg_config", check_type=str):
                self.tool_requires("pkgconf")
            if self.settings.os in ["tvOS", "watchOS"]:
                self.tool_requires("gnu-config")
            if self.settings_build.os == "Windows":
                self.win_bash = True
                if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                    self.tool_requires("msys2")

    def latest_version(self):
        repo = GithubRepository(self, "curl/curl")
        return Version(repo.latest_tag("curl-").removeprefix("curl-").replace("_", "."))

    def source(self):
        get(
            self,
            url="https://curl.se/download/curl-8.20.0.tar.xz",
            sha256="63fe2dc148ba0ceae89922ef838f7e5c946272c2e78b7c59fab4b79d3ce2b896",
            destination=self.source_folder,
            strip_root=True)
        cert_url = self.conf.get("user.libcurl.cert:url", check_type=str) or "https://curl.se/ca/cacert-2025-11-04.pem"
        cert_sha256 = self.conf.get("user.libcurl.cert:sha256", check_type=str) or "8ac40bdd3d3e151a6b4078d2b2029796e8f843e3f86fbf2adbc4dd9f05e79def"
        download(
            self,
            cert_url,
            os.path.join(self.source_folder, "cacert.pem"),
            verify=True,
            sha256=cert_sha256)
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"), "find_package(NGHTTP2 MODULE)", "find_package(NGHTTP2 CONFIG REQUIRED)")
        replace_in_file(self, os.path.join(self.source_folder, "CMakeLists.txt"), "find_package(Cares MODULE REQUIRED)", "find_package(Cares CONFIG REQUIRED)")
        replace_in_file(self, os.path.join(self.source_folder, "CMake", "Macros.cmake"), "find_package(${_find_name})", "find_package(${_find_name} CONFIG REQUIRED)")
        replace_in_file(self, os.path.join(self.source_folder, "CMake", "Macros.cmake"), "find_package(${_find_name} MODULE)", "find_package(${_find_name} CONFIG REQUIRED)")
        replace_in_file(self, os.path.join(self.source_folder, "CMake", "Macros.cmake"), "find_package(${_find_name} REQUIRED)", "find_package(${_find_name} CONFIG REQUIRED)")
        replace_in_file(self, os.path.join(self.source_folder, "CMake", "Macros.cmake"), "find_package(${_find_name} MODULE REQUIRED)", "find_package(${_find_name} CONFIG REQUIRED)")

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()
        if self._is_win_x_android:
            tc = CMakeToolchain(self, generator="Ninja")
        else:
            tc = CMakeToolchain(self)
        tc.variables["ENABLE_UNICODE"] = True
        tc.variables["BUILD_TESTING"] = False
        tc.variables["BUILD_CURL_EXE"] = self.options.build_executable
        tc.cache_variables["ENABLE_CURL_MANUAL"] = False
        tc.variables["CURL_DISABLE_LDAP"] = not self.options.with_ldap
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["CURL_STATICLIB"] = not self.options.shared
        tc.variables["CMAKE_DEBUG_POSTFIX"] = ""
        tc.variables["CURL_USE_SCHANNEL"] = self.options.with_ssl == "schannel"
        tc.variables["CURL_USE_OPENSSL"] = self.options.with_ssl in ("openssl", "libressl")
        tc.variables["CURL_USE_WOLFSSL"] = self.options.with_ssl == "wolfssl"
        tc.variables["CURL_USE_MBEDTLS"] = self.options.with_ssl == "mbedtls"
        tc.variables["USE_NGHTTP2"] = self.options.with_nghttp2
        tc.variables["CURL_ZLIB"] = self.options.with_zlib
        tc.variables["CURL_BROTLI"] = self.options.with_brotli
        tc.variables["CURL_ZSTD"] = self.options.with_zstd
        tc.variables["CURL_USE_LIBPSL"] = self.options.with_libpsl
        tc.variables["CURL_USE_LIBSSH2"] = self.options.with_libssh2
        tc.variables["ENABLE_ARES"] = self.options.with_c_ares
        tc.variables["CURL_ENABLE_SMB"] = self.options.with_smb
        if not self.options.with_c_ares:
            tc.variables["ENABLE_THREADED_RESOLVER"] = self.options.with_threaded_resolver
        tc.variables["CURL_DISABLE_PROXY"] = not self.options.with_proxy
        tc.variables["USE_LIBIDN2"] = self.options.with_libidn
        if self.options.with_libidn:
            # Recipe won't generate this variable as we're setting prefixes,
            # and CMake might not either as it's looking for Libidn2
            # Ensure it's there
            tc.cache_variables["LIBIDN2_FOUND"] = True
        tc.variables["CURL_DISABLE_RTSP"] = not self.options.with_rtsp
        tc.variables["CURL_DISABLE_CRYPTO_AUTH"] = not self.options.with_crypto_auth
        tc.variables["CURL_DISABLE_VERBOSE_STRINGS"] = not self.options.with_verbose_strings
        if self.options.with_ssl == "libressl":
            tc.variables["CURL_DISABLE_SRP"] = True
        if "with_form_api" in self.options:
            tc.variables["CURL_DISABLE_FORM_API"] = not self.options.with_form_api
        if "with_websockets" in self.options:
            tc.variables["CURL_DISABLE_WEBSOCKETS"] = not self.options.with_websockets

        # Also disables NTLM_WB if set to false
        tc.variables["CURL_ENABLE_NTLM"] = self.options.with_ntlm

        if self.options.with_ca_bundle:
            tc.cache_variables["CURL_CA_BUNDLE"] = str(self.options.with_ca_bundle)
        else:
            tc.cache_variables["CURL_CA_BUNDLE"] = "none"

        if self.options.with_ca_path:
            tc.cache_variables["CURL_CA_PATH"] = str(self.options.with_ca_path)
        else:
            tc.cache_variables["CURL_CA_PATH"] = "none"

        tc.cache_variables["CURL_CA_FALLBACK"] = self.options.with_ca_fallback

        # TODO: refactor this and consider `CMAKE_TRY_COMPILE_CONFIGURATION` for all platforms
        #       see upstream issue 12180
        tc.variables["HAVE_SSL_SET0_WBIO"] = False
        tc.variables["HAVE_OPENSSL_SRP"] = True
        tc.variables["HAVE_SSL_CTX_SET_QUIC_METHOD"] = True

        if is_msvc(self):
            tc.cache_variables["CMAKE_TRY_COMPILE_CONFIGURATION"] = str(self.settings.build_type)

        if self.options.with_libssh2:
            # Not generated automatically
            tc.cache_variables["LIBSSH2_FOUND"] = True

        tc.generate()

        deps = CMakeConfigDeps(self)
        deps.set_property("wolfssl", "cmake_additional_variables_prefixes", ["WolfSSL", "WOLFSSL"])
        deps.set_property("wolfssl", "cmake_file_name", "WolfSSL")

        if self.options.with_brotli:
            deps.set_property("brotli", "cmake_file_name", "Brotli")
            deps.set_property("brotli", "cmake_target_name", "CURL::brotli")
            deps.set_property("brotli", "cmake_additional_variables_prefixes", ["BROTLI", ])
            deps.set_property("brotli", "cmake_extra_variables", {"BROTLI_FOUND": "1"})

        if self.options.with_zstd:
            deps.set_property("zstd", "cmake_file_name", "Zstd")
            deps.set_property("zstd", "cmake_target_name", "CURL::zstd")
            deps.set_property("zstd", "cmake_additional_variables_prefixes", ["ZSTD", ])
            deps.set_property("zstd", "cmake_extra_variables", {"ZSTD_FOUND": "1", "ZSTD_VERSION": str(self.dependencies["zstd"].ref.version)})

        if self.options.with_c_ares:
            deps.set_property("c-ares", "cmake_file_name", "Cares")
            deps.set_property("c-ares", "cmake_target_name", "CURL::cares")

        if self.options.with_libidn:
            deps.set_property("libidn2", "cmake_file_name", "Libidn2")
            deps.set_property("libidn2", "cmake_target_name", "CURL::libidn2")
            deps.set_property("libidn2", "cmake_additional_variables_prefixes", ["LIBIDN2"])

        if self.options.get_safe("with_libpsl"):
            deps.set_property("libpsl", "cmake_target_name", "CURL::libpsl")

        if self.options.with_libssh2:
            deps.set_property("libssh2", "cmake_target_name", "CURL::libssh2")

        if self.options.with_nghttp2:
            deps.set_property("libnghttp2", "cmake_file_name", "NGHTTP2")
            deps.set_property("libnghttp2", "cmake_target_name", "CURL::nghttp2")

        if self.options.with_ssl == "wolfssl":
            deps.set_property("wolfssl", "cmake_target_name", "CURL::wolfssl")
        # Now the rest of the dependencies that don't use the imported target directly
        # (openssl, zlib)

        if self.options.with_ssl == "mbedtls":
            deps.set_property("mbedtls", "cmake_target_name", "CURL::mbedtls")

        deps.generate()

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def _patch_sources(self):
        if self.options.with_largemaxwritesize:
            replace_in_file(
                self,
                os.path.join(self.source_folder, "include", "curl", "curl.h"),
                "define CURL_MAX_WRITE_SIZE 16384",
                "define CURL_MAX_WRITE_SIZE 10485760")

    def package(self):
        copy(self, "COPYING", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        copy(self, "cacert.pem", src=self.source_folder, dst=os.path.join(self.package_folder, "res"))
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "CURL")
        self.cpp_info.set_property("cmake_target_name", "CURL::libcurl")
        self.cpp_info.set_property("pkg_config_name", "libcurl")

        self.cpp_info.components["curl"].resdirs = ["res"]
        if is_msvc(self):
            self.cpp_info.components["curl"].libs = ["libcurl_imp"] if self.options.shared else ["libcurl"]
        else:
            self.cpp_info.components["curl"].libs = ["curl"]

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.components["curl"].system_libs = ["rt", "pthread"]
        elif self.settings.os == "Windows":
            # used on Windows for VS build, native and cross mingw build
            self.cpp_info.components["curl"].system_libs = ["ws2_32", "bcrypt", "iphlpapi"]
            if self.options.with_ldap:
                self.cpp_info.components["curl"].system_libs.append("wldap32")
            if self.options.with_ssl == "libressl":
                self.cpp_info.components["curl"].system_libs.append("crypt32")
            if self.options.with_ssl == "schannel":
                self.cpp_info.components["curl"].system_libs.extend(["crypt32", "secur32"])
        elif is_apple_os(self):
            self.cpp_info.components["curl"].frameworks.append("CoreFoundation")
            self.cpp_info.components["curl"].frameworks.append("CoreServices")
            self.cpp_info.components["curl"].frameworks.append("SystemConfiguration")
            if self.options.get_safe("with_apple_sectrust"):
                self.cpp_info.components["curl"].frameworks.append("Security")
            if self.options.with_ldap:
                self.cpp_info.components["curl"].system_libs.append("ldap")

        if self._is_mingw:
            # provide pthread for dependent packages
            self.cpp_info.components["curl"].cflags.append("-pthread")
            self.cpp_info.components["curl"].exelinkflags.append("-pthread")
            self.cpp_info.components["curl"].sharedlinkflags.append("-pthread")

        if not self.options.shared:
            self.cpp_info.components["curl"].defines.append("CURL_STATICLIB=1")

        if self.options.with_ssl == "openssl":
            self.cpp_info.components["curl"].requires.append("openssl::openssl")
        if self.options.with_ssl == "libressl":
            self.cpp_info.components["curl"].requires.append("libressl::libressl")
        if self.options.with_ssl == "wolfssl":
            self.cpp_info.components["curl"].requires.append("wolfssl::wolfssl")
        if self.options.with_ssl == "mbedtls":
            self.cpp_info.components["curl"].requires.append("mbedtls::mbedtls")
        if self.settings.os == "Linux" and self.options.with_ldap:
            self.cpp_info.components["curl"].requires.append("openldap::openldap")
        if self.options.with_nghttp2:
            self.cpp_info.components["curl"].requires.append("libnghttp2::libnghttp2")
        if self.options.with_libssh2:
            self.cpp_info.components["curl"].requires.append("libssh2::libssh2")
        if self.options.with_zlib:
            self.cpp_info.components["curl"].requires.append("zlib::zlib")
        if self.options.with_brotli:
            self.cpp_info.components["curl"].requires.append("brotli::brotli")
        if self.options.with_zstd:
            self.cpp_info.components["curl"].requires.append("zstd::zstd")
        if self.options.with_c_ares:
            self.cpp_info.components["curl"].requires.append("c-ares::c-ares")
        if self.options.get_safe("with_libpsl"):
            self.cpp_info.components["curl"].requires.append("libpsl::libpsl")
        if self.options.with_libidn:
            self.cpp_info.components["curl"].requires.append("libidn2::libidn2")

        self.cpp_info.components["curl"].set_property("cmake_target_name", "CURL::libcurl")
        self.cpp_info.components["curl"].set_property("pkg_config_name", "libcurl")
