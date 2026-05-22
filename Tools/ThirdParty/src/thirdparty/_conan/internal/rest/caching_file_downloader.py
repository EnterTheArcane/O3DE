import hashlib
import os
import urllib.request


class SourcesCachingDownloader:
    def __init__(self, conanfile):
        self._output = conanfile.output

    def download(self, urls, file_path, retry, retry_wait, verify_ssl, auth, headers, md5, sha1, sha256):
        if isinstance(urls, str):
            urls = [urls]
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
                        raise RuntimeError(f"SHA256 mismatch: expected {sha256}, got {digest}")
                return
            except Exception as e:
                last_err = e
                self._output.warning(f"Download failed ({url}): {e}")
        raise RuntimeError(f"All download URLs failed: {last_err}")

