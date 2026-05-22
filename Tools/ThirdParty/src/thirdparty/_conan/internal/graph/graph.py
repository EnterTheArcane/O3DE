from thirdparty._conan.internal.graph.graph_error import GraphError, GraphConflictError
RECIPE_DOWNLOADED = "Downloaded"
RECIPE_INCACHE = "Cache"
RECIPE_UPDATED = "Updated"
RECIPE_INCACHE_DATE_UPDATED = "Cache (Updated date)"
RECIPE_NEWER = "Newer"
RECIPE_NOT_IN_REMOTE = "Not in remote"
RECIPE_UPDATEABLE = "Update available"
RECIPE_EDITABLE = "Editable"
RECIPE_CONSUMER = "Consumer"
RECIPE_VIRTUAL = "Cli"
RECIPE_PLATFORM = "Platform"
BINARY_CACHE = "Cache"
BINARY_DOWNLOAD = "Download"
BINARY_UPDATE = "Update"
BINARY_BUILD = "Build"
BINARY_MISSING = "Missing"
BINARY_SKIP = "Skip"
BINARY_EDITABLE = "Editable"
BINARY_EDITABLE_BUILD = "EditableBuild"
BINARY_INVALID = "Invalid"
BINARY_PLATFORM = "Platform"
CONTEXT_HOST = "host"
CONTEXT_BUILD = "build"


class Overrides:
    def __init__(self):
        self._overrides = {}

    def __bool__(self):
        return bool(self._overrides)

