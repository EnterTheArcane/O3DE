# ThirdParty

Setup:
```bash
python -m venv .venv
.venv\Scripts\activate  # Windows
source .venv/bin/activate  # Linux/macOS
pip install -e "."
```

Usage:
```bash
thirdparty --help
thirdparty list
thirdparty build
```

Type checking:
```bash
pyright
```

Docker:
```bash
docker build -t o3de/thirdparty:linux containers/linux
docker run --rm -it -v $(pwd):/o3de o3de/thirdparty:linux bash
```
