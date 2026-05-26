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

        cache_dir = _downloads_cache_dir()
        cache_dir.mkdir(parents=True, exist_ok=True)
        last_err = None
        for url in urls:
            tmp_fd, tmp_path = tempfile.mkstemp(dir=cache_dir, suffix=".tmp")
            os.close(tmp_fd)
            try:
                self._output.info(f"Downloading {url}")
                req = urllib.request.Request(url, headers=headers or {})
                with urllib.request.urlopen(req) as resp, open(tmp_path, "wb") as f:
                    f.write(resp.read())
                if sha256:
                    with open(tmp_path, "rb") as f:
                        digest = hashlib.sha256(f.read()).hexdigest()
                    if digest != sha256:
                        raise RuntimeError(f"SHA256 mismatch: expected {sha256}, got {digest}")
                    # Promote to the keyed cache entry atomically.
                    cache_path = cache_dir / sha256
                    if not cache_path.is_file():
                        os.replace(tmp_path, cache_path)
                    else:
                        os.unlink(tmp_path)
                    tmp_path = None
                    os.makedirs(os.path.dirname(file_path) or ".", exist_ok=True)
                    shutil.copy2(cache_path, file_path)
                else:
                    os.makedirs(os.path.dirname(file_path) or ".", exist_ok=True)
                    shutil.move(tmp_path, file_path)
                    tmp_path = None
                return
            except Exception as e:
                if tmp_path:
                    try:
                        os.unlink(tmp_path)
                    except OSError:
                        pass
                last_err = e
                self._output.warning(f"Download failed ({url}): {e}")
        raise RuntimeError(f"All download URLs failed: {last_err}")
