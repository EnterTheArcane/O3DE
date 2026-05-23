
class FileProgress:
    """Minimal stand-in for conan's progress-reporting file wrapper.
    Used as a context manager that opens `path` in binary mode and returns the
    underlying file object (e.g. for zipfile.ZipFile)."""

    def __init__(self, path, msg="", mode="r", **kw):
        self._path = path
        self._file = None

    def __enter__(self):
        self._file = open(self._path, "rb")
        return self._file

    def __exit__(self, *args):
        if self._file is not None:
            self._file.close()
        return False

