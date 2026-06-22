from enum import Enum


class PackageType(Enum):
    LIBRARY = "library"  # abstract type, should contain shared option to define
    STATIC = "static-library"
    SHARED = "shared-library"
    HEADER = "header-library"
    BUILD_SCRIPTS = "build-scripts"
    APP = "application"
    PYTHON = "python-require"
    CONF = "configuration"
    UNKNOWN = "unknown"

    def __str__(self):
        return self.value

    def __eq__(self, other):
        # This is useful for comparing with string type at user code, like ``package_type == "xxx"``
        return super().__eq__(PackageType(other))
