from conan.tools.files import *
from thirdparty._host.patches import apply_patches, export_conandata_patches

def apply_conandata_patches(conanfile):
    apply_patches(conanfile)
