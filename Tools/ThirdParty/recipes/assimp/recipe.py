# Ported from conan-center-index/assimp by port_recipe.py
# REVIEW: verify all transforms are correct before building

from pathlib import Path

from thirdparty import RecipeBase
from thirdparty.tools.cmake import CMake, CMakeDeps, CMakeToolchain
from thirdparty.tools.files import collect_libs, copy, get, replace_in_file, rmdir
from thirdparty.tools.microsoft import is_msvc, is_msvc_static_runtime
from thirdparty.tools.scm import Version
import os


class Recipe(RecipeBase):
    name = "assimp"
    version = "6.0.2"
    license = "BSD-3-Clause"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "double_precision": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "double_precision": False,
    }

    _format_option_map = {
        "with_3d": ("ASSIMP_BUILD_3D_IMPORTER", "5.0.0"),
        "with_3ds": ("ASSIMP_BUILD_3DS_IMPORTER", "5.0.0"),
        "with_3ds_exporter": ("ASSIMP_BUILD_3DS_EXPORTER", "5.0.0"),
        "with_3mf": ("ASSIMP_BUILD_3MF_IMPORTER", "5.0.0"),
        "with_3mf_exporter": ("ASSIMP_BUILD_3MF_EXPORTER", "5.0.0"),
        "with_ac": ("ASSIMP_BUILD_AC_IMPORTER", "5.0.0"),
        "with_amf": ("ASSIMP_BUILD_AMF_IMPORTER", "5.0.0"),
        "with_ase": ("ASSIMP_BUILD_ASE_IMPORTER", "5.0.0"),
        "with_assbin": ("ASSIMP_BUILD_ASSBIN_IMPORTER", "5.0.0"),
        "with_assbin_exporter": ("ASSIMP_BUILD_ASSBIN_EXPORTER", "5.0.0"),
        "with_assxml_exporter": ("ASSIMP_BUILD_ASSXML_EXPORTER", "5.0.0"),
        "with_assjson_exporter": ("ASSIMP_BUILD_ASSJSON_EXPORTER", "5.0.0"),
        "with_b3d": ("ASSIMP_BUILD_B3D_IMPORTER", "5.0.0"),
        "with_blend": ("ASSIMP_BUILD_BLEND_IMPORTER", "5.0.0"),
        "with_bvh": ("ASSIMP_BUILD_BVH_IMPORTER", "5.0.0"),
        "with_ms3d": ("ASSIMP_BUILD_MS3D_IMPORTER", "5.0.0"),
        "with_cob": ("ASSIMP_BUILD_COB_IMPORTER", "5.0.0"),
        "with_collada": ("ASSIMP_BUILD_COLLADA_IMPORTER", "5.0.0"),
        "with_collada_exporter": ("ASSIMP_BUILD_COLLADA_EXPORTER", "5.0.0"),
        "with_csm": ("ASSIMP_BUILD_CSM_IMPORTER", "5.0.0"),
        "with_dxf": ("ASSIMP_BUILD_DXF_IMPORTER", "5.0.0"),
        "with_fbx": ("ASSIMP_BUILD_FBX_IMPORTER", "5.0.0"),
        "with_fbx_exporter": ("ASSIMP_BUILD_FBX_EXPORTER", "5.0.0"),
        "with_gltf": ("ASSIMP_BUILD_GLTF_IMPORTER", "5.0.0"),
        "with_gltf_exporter": ("ASSIMP_BUILD_GLTF_EXPORTER", "5.0.0"),
        "with_hmp": ("ASSIMP_BUILD_HMP_IMPORTER", "5.0.0"),
        "with_ifc": ("ASSIMP_BUILD_IFC_IMPORTER", "5.0.0"),
        "with_irr": ("ASSIMP_BUILD_IRR_IMPORTER", "5.0.0"),
        "with_irrmesh": ("ASSIMP_BUILD_IRRMESH_IMPORTER", "5.0.0"),
        "with_lwo": ("ASSIMP_BUILD_LWO_IMPORTER", "5.0.0"),
        "with_lws": ("ASSIMP_BUILD_LWS_IMPORTER", "5.0.0"),
        "with_md2": ("ASSIMP_BUILD_MD2_IMPORTER", "5.0.0"),
        "with_md3": ("ASSIMP_BUILD_MD3_IMPORTER", "5.0.0"),
        "with_md5": ("ASSIMP_BUILD_MD5_IMPORTER", "5.0.0"),
        "with_mdc": ("ASSIMP_BUILD_MDC_IMPORTER", "5.0.0"),
        "with_mdl": ("ASSIMP_BUILD_MDL_IMPORTER", "5.0.0"),
        "with_mmd": ("ASSIMP_BUILD_MMD_IMPORTER", "5.0.0"),
        "with_ndo": ("ASSIMP_BUILD_NDO_IMPORTER", "5.0.0"),
        "with_nff": ("ASSIMP_BUILD_NFF_IMPORTER", "5.0.0"),
        "with_obj": ("ASSIMP_BUILD_OBJ_IMPORTER", "5.0.0"),
        "with_obj_exporter": ("ASSIMP_BUILD_OBJ_EXPORTER", "5.0.0"),
        "with_off": ("ASSIMP_BUILD_OFF_IMPORTER", "5.0.0"),
        "with_ogre": ("ASSIMP_BUILD_OGRE_IMPORTER", "5.0.0"),
        "with_opengex": ("ASSIMP_BUILD_OPENGEX_IMPORTER", "5.0.0"),
        "with_opengex_exporter": ("ASSIMP_BUILD_OPENGEX_EXPORTER", "5.0.0"),
        "with_pbrt_exporter": ("ASSIMP_BUILD_PBRT_EXPORTER", "5.1.0"),
        "with_ply": ("ASSIMP_BUILD_PLY_IMPORTER", "5.0.0"),
        "with_ply_exporter": ("ASSIMP_BUILD_PLY_EXPORTER", "5.0.0"),
        "with_q3bsp": ("ASSIMP_BUILD_Q3BSP_IMPORTER", "5.0.0"),
        "with_q3d": ("ASSIMP_BUILD_Q3D_IMPORTER", "5.0.0"),
        "with_raw": ("ASSIMP_BUILD_RAW_IMPORTER", "5.0.0"),
        "with_sib": ("ASSIMP_BUILD_SIB_IMPORTER", "5.0.0"),
        "with_smd": ("ASSIMP_BUILD_SMD_IMPORTER", "5.0.0"),
        "with_step": ("ASSIMP_BUILD_STEP_IMPORTER", "5.0.0"),
        "with_step_exporter": ("ASSIMP_BUILD_STEP_EXPORTER", "5.0.0"),
        "with_stl": ("ASSIMP_BUILD_STL_IMPORTER", "5.0.0"),
        "with_stl_exporter": ("ASSIMP_BUILD_STL_EXPORTER", "5.0.0"),
        "with_terragen": ("ASSIMP_BUILD_TERRAGEN_IMPORTER", "5.0.0"),
        "with_x": ("ASSIMP_BUILD_X_IMPORTER", "5.0.0"),
        "with_x_exporter": ("ASSIMP_BUILD_X_EXPORTER", "5.0.0"),
        "with_x3d": ("ASSIMP_BUILD_X3D_IMPORTER", "5.0.0"),
        "with_x3d_exporter": ("ASSIMP_BUILD_X3D_EXPORTER", "5.0.0"),
        "with_xgl": ("ASSIMP_BUILD_XGL_IMPORTER", "5.0.0"),
        "with_m3d": ("ASSIMP_BUILD_M3D_IMPORTER", "5.1.0"),
        "with_m3d_exporter": ("ASSIMP_BUILD_M3D_EXPORTER", "5.1.0"),
        "with_iqm": ("ASSIMP_BUILD_IQM_IMPORTER", "5.2.0"),
    }
    options.update(dict.fromkeys(_format_option_map, [True, False]))
    default_options.update(dict.fromkeys(_format_option_map, True))

    @property
    def _depends_on_kuba_zip(self):
        return self.options.with_3mf_exporter

    @property
    def _depends_on_poly2tri(self):
        return self.options.with_blend or self.options.with_ifc

    @property
    def _depends_on_rapidjson(self):
        return self.options.with_gltf or self.options.with_gltf_exporter

    @property
    def _depends_on_draco(self):
        return self.options.with_gltf or self.options.with_gltf_exporter

    @property
    def _depends_on_clipper(self):
        return self.options.with_ifc

    @property
    def _depends_on_stb(self):
        return (
            self.options.with_m3d
            or self.options.with_m3d_exporter
            or self.options.with_pbrt_exporter
        )

    @property
    def _depends_on_openddlparser(self):
        return self.options.with_opengex

    def requirements(self) -> list[str]:
        return ["zlib"]  # All other deps are bundled in assimp's contrib/

    def source(self):
        get(
            url="https://github.com/assimp/assimp/archive/refs/tags/v6.0.2.tar.gz",
            dest=self.source_folder,
            sha256="d1822d9a19c9205d6e8bc533bf897174ddb360ce504680f294170cc1d6319751",
        )
        self._patch_sources()

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["ASSIMP_ANDROID_JNIIOSYSTEM"] = False
        tc.variables["ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT"] = False
        tc.variables["ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT"] = False
        tc.variables["ASSIMP_BUILD_ASSIMP_TOOLS"] = False
        tc.variables["ASSIMP_BUILD_DOCS"] = False
        tc.variables["ASSIMP_BUILD_DRACO"] = False
        tc.variables["ASSIMP_BUILD_FRAMEWORK"] = False
        tc.variables["ASSIMP_BUILD_MINIZIP"] = True  # use bundled minizip from contrib/
        tc.variables["ASSIMP_BUILD_SAMPLES"] = False
        tc.variables["ASSIMP_BUILD_TESTS"] = False
        tc.variables["ASSIMP_BUILD_ZLIB"] = False
        tc.variables["ASSIMP_DOUBLE_PRECISION"] = self.options.double_precision
        tc.variables["ASSIMP_HUNTER_ENABLED"] = False
        tc.variables["ASSIMP_IGNORE_GIT_HASH"] = True
        tc.variables["ASSIMP_INJECT_DEBUG_POSTFIX"] = False
        tc.variables["ASSIMP_INSTALL"] = True
        tc.variables["ASSIMP_INSTALL_PDB"] = False
        tc.variables["ASSIMP_NO_EXPORT"] = False
        tc.variables["ASSIMP_OPT_BUILD_PACKAGES"] = False
        tc.variables["ASSIMP_RAPIDJSON_NO_MEMBER_ITERATOR"] = False
        tc.variables["ASSIMP_UBSAN"] = False
        tc.variables["ASSIMP_WARNINGS_AS_ERRORS"] = False
        tc.variables["USE_STATIC_CRT"] = False
        tc.cache_variables["ASSIMP_BUILD_USE_CCACHE"] = False

        for option, (definition, _) in self._format_option_map.items():
            value = self.options.get(option)
            if value is not None:
                tc.variables[definition] = value
        if self.is_windows:
            tc.preprocessor_definitions["NOMINMAX"] = 1

        tc.cache_variables["WITH_CLIPPER"] = self._depends_on_clipper
        tc.cache_variables["WITH_DRACO"] = self._depends_on_draco
        tc.cache_variables["WITH_KUBAZIP"] = self._depends_on_kuba_zip
        tc.cache_variables["WITH_OPENDDL"] = self._depends_on_openddlparser
        tc.cache_variables["WITH_POLY2TRI"] = self._depends_on_poly2tri
        tc.cache_variables["WITH_RAPIDJSON"] = self._depends_on_rapidjson
        tc.cache_variables["WITH_STB"] = self._depends_on_stb
        tc.generate()

        cd = CMakeDeps(self)
        cd.generate()

    def _patch_sources(self):
        # Remove forced compiler/linker flags that conflict with our build settings
        for pattern in [
            "-fPIC",
            "-g ",
            "SET(CMAKE_POSITION_INDEPENDENT_CODE ON)",
            'SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_DEBUG /Zi /Od")',
            'SET(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG:FULL /PDBALTPATH:%_PDB% /OPT:REF /OPT:ICF")',
        ]:
            replace_in_file(
                os.path.join(self.source_folder, "CMakeLists.txt"), pattern, ""
            )

        for pattern in ["-Werror", "/WX"]:
            replace_in_file(
                os.path.join(self.source_folder, "CMakeLists.txt"), pattern, ""
            )
            replace_in_file(
                os.path.join(self.source_folder, "code", "CMakeLists.txt"), pattern, ""
            )

        # All other contrib libs (clipper, poly2tri, rapidjson, pugixml, etc.) remain bundled.
        # ASSIMP_BUILD_MINIZIP=True uses the bundled minizip from contrib/.

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        cmake = CMake(self)
        cmake.install()
        rmdir(os.path.join(self.package_folder, "lib", "pkgconfig"))
