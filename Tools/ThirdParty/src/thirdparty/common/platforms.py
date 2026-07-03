from enum import StrEnum


class Arch(StrEnum):
    ARM = "ARM"
    X64 = "X64"


class Os(StrEnum):
    ANDROID = "Android"
    IOS = "iOS"
    LINUX = "Linux"
    MAC = "Mac"
    TVOS = "tvOS"
    WINDOWS = "Windows"

class BuildType(StrEnum):
    DEBUG = "Debug"
    PROFILE = "Profile"
    RELEASE = "Release"
