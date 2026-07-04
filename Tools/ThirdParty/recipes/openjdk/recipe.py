from pathlib import Path

from thirdparty import RecipeBase
from thirdparty.errors import RecipeInvalidConfiguration
from thirdparty.files import copy, get, rm, symlinks

class Recipe(RecipeBase):
    name = "openjdk"
    version = "25.0.2"
    license = (
        "GPL-2.0-only WITH Classpath-exception-2.0",
        "GPL-2.0-only WITH OpenJDK-assembly-exception-1.0",
    )

    _SOURCE_DATA = {
        ("Linux", "ARM"): {
            "url": "https://download.java.net/java/GA/jdk{version}/b1e0dfa218384cb9959bdcb897162d4e/10/GPL/openjdk-{version}_linux-aarch64_bin.tar.gz",
            "sha256": "671208d205e70c9805da45a483f670d49dd64654990a7b7223ccffb2abb070dd",
        },
        ("Linux", "X64"): {
            "url": "https://download.java.net/java/GA/jdk{version}/b1e0dfa218384cb9959bdcb897162d4e/10/GPL/openjdk-{version}_linux-x64_bin.tar.gz",
            "sha256": "555ce0821e4fe175ea50d54518cd6fbece9663c1998de529bc6ce429534457df",
        },
        ("Mac", "ARM"): {
            "url": "https://download.java.net/java/GA/jdk{version}/b1e0dfa218384cb9959bdcb897162d4e/10/GPL/openjdk-{version}_macos-aarch64_bin.tar.gz",
            "sha256": "7581b0d1752cd5acbf39e286c03f07b6cd6c205b562eb2fe753ff0253cf4c1bf",
        },
        ("Mac", "X64"): {
            "url": "https://download.java.net/java/GA/jdk{version}/b1e0dfa218384cb9959bdcb897162d4e/10/GPL/openjdk-{version}_macos-x64_bin.tar.gz",
            "sha256": "4ec2f4bc47b057fdf9cda07af27fae8f3605e90fa963d4240d63baeb46ede460",
        },
        ("Windows", "X64"): {
            "url": "https://download.java.net/java/GA/jdk{version}/b1e0dfa218384cb9959bdcb897162d4e/10/GPL/openjdk-{version}_windows-x64_bin.zip",
            "sha256": "74784a0c07258f32d36e9224dd79187c566d831c30d47dc06888d4212087331d",
        },
    }

    def validate(self):
        if (self.settings.os, self.settings.arch) not in self._SOURCE_DATA:
            raise RecipeInvalidConfiguration("OpenJDK not available for this platform")

    def build(self):
        source = self._source
        get(
            self,
            url=source["url"],
            sha256=source["sha256"],
            destination=self.folders.build,
            strip_root=True)

    def package(self):
        symlinks.remove_broken_symlinks(self, self._jdk_home)

        copy(self, "*", src=self._jdk_home / "bin", dst=self.folders.package / "bin")
        copy(self, "*", src=self._jdk_home / "include", dst=self.folders.package / "include")
        copy(self, "*", src=self._jdk_home / "lib", dst=self.folders.package / "lib")
        copy(self, "*", src=self._jdk_home / "jmods", dst=self.folders.package / "lib" / "jmods")
        copy(self, "*", src=self._jdk_home / "conf", dst=self.folders.package / "conf")
        copy(self, "*", src=self._jdk_home / "legal", dst=self.folders.package / "licenses")
        copy(self, "release", src=self._jdk_home, dst=self.folders.package, keep_path=False)

        if self.settings.os == "Windows":
            for runtime_dll in ("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
                rm(self, runtime_dll, self.folders.package / "bin")

    def package_info(self):
        java_home = self.folders.package
        bin_dir = java_home / "bin"
        java_exe = bin_dir / ("java.exe" if self.settings.os == "Windows" else "java")

        self.info.buildenv.define_path("JAVA_HOME", java_home)
        self.info.runenv.define_path("JAVA_HOME", java_home)
        self.info.buildenv.prepend_path("PATH", bin_dir)
        self.info.runenv.prepend_path("PATH", bin_dir)
        self.info.conf.define("user.openjdk:java_home", str(java_home))
        self.info.conf.define("user.openjdk:java", str(java_exe))

    @property
    def _jdk_home(self) -> Path:
        if self.settings.os == "Mac":
            return self.folders.build / "Contents" / "Home"
        return self.folders.build

    @property
    def _source(self) -> dict[str, str]:
        source = dict(self._SOURCE_DATA[(self.settings.os, self.settings.arch)])
        source["url"] = source["url"].format(version=self.version)
        return source
