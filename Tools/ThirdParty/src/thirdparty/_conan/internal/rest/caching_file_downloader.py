import hashlib
import os
import shutil
import tempfile
import urllib.request
from pathlib import Path


def _downloads_cache_dir() -> Path:
    """Return the downloads cache directory.

    Defaults to ``~/.o3de/ThirdParty/Downloads``.
    Override the base directory with the ``O3DE_THIRDPARTY_CACHE`` env var,
    e.g. ``O3DE_THIRDPARTY_CACHE=D:/cache`` → ``D:/cache/Downloads``.
    """
    base = os.environ.get("O3DE_THIRDPARTY_CACHE", str(Path.home() / ".o3de" / "ThirdParty"))
    return Path(base) / "Downloads"


class SourcesCachingDownloader:
    def __init__(self, conanfile):
        self._output = conanfile.output

    def download(self, urls, file_path, retry, retry_wait, verify_ssl, auth, headers, md5, sha1, sha256):
        if isinstance(urls, str):
            urls = [urls]

        # Serve from the local cache when we have a sha256 key.
        if sha256:
            cache_path = _downloads_cache_dir() / sha256
            if cache_path.is_file():
                self._output.info(f"Using cached download [{sha256[:16]}…]")
                os.makedirs(os.path.dirname(file_path) or ".", exist_ok=True)
                shutil.copy2(cache_path, file_path)
                return

        last_err = None
        for url in urls:
            try:
                self._output.info(f"Downloading {url}")
                os.makedirs(os.path.dirname(file_path) or ".", exist_ok=True)
                req = urllib.request.Request(url, headers=headers or {})
                with urllib.request.urlopen(req) as resp, open(file_path, "wb") as f:
                    f.write(resp.read())
                if sha256:
                    with open(file_path, "rb") as f:
                        digest = hashlib.sha256(f.read()).hexdigest()
                    if digest != sha256:
                        os.unlink(file_path)
                        raise RuntimeError(f"SHA256 mismatch: expected {sha256}, got {digest}")
                    # Store in cache atomically (temp-file + rename) to avoid partial cache entries.
                    cache_dir = _downloads_cache_dir()
                    cache_dir.mkdir(parents=True, exist_ok=True)
                    cache_path = cache_dir / sha256
                    if not cache_path.is_file():
                        tmp_fd, tmp_path = tempfile.mkstemp(dir=cache_dir)
                        try:
                            os.close(tmp_fd)
                            shutil.copy2(file_path, tmp_path)
                            os.replace(tmp_path, cache_path)
                        except Exception:
                            try:
                                os.unlink(tmp_path)
                            except OSError:
                                pass
                return
            except Exception as e:
                last_err = e
                self._output.warning(f"Download failed ({url}): {e}")
        raise RuntimeError(f"All download URLs failed: {last_err}")


