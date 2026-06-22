import os

_EXTENSIONS_FOLDER = "extensions"
_PLUGINS = "plugins"


class HomePaths:
    """ pure computing of paths in the home, not caching anything
    """
    def __init__(self, home_folder):
        self._home = home_folder

    @property
    def global_conf_path(self):
        return os.path.join(self._home, "global.conf")

    @property
    def global_conf_path_user(self):
        return os.path.join(self._home, "global_user.conf")

    @property
    def auth_source_plugin_path(self):
        return os.path.join(self._home, _EXTENSIONS_FOLDER, _PLUGINS, "auth_source.py")

    @property
    def default_sources_backup_folder(self):
        return os.path.join(self._home, "sources")

    @property
    def settings_path(self):
        return os.path.join(self._home, "settings.yml")

    @property
    def settings_path_user(self):
        return os.path.join(self._home, "settings_user.yml")



