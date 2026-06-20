# Keep everything private until we review what is really needed and refactor passing "conanfile"
# from conan.tools.apple.apple import apple_dot_clean
# from conan.tools.apple.apple import apple_sdk_name
# from conan.tools.apple.apple import apple_deployment_target_flag
from thirdparty.apple.apple import fix_apple_shared_install_name, is_apple_os, to_apple_arch, XCRun
from thirdparty.apple.xcodedeps import XcodeDeps
from thirdparty.apple.xcodebuild import XcodeBuild
from thirdparty.apple.xcodetoolchain import XcodeToolchain
