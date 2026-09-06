# Clang preprocessing regression inputs

These 36 files are unchanged inputs from `clang/test/Preprocessor` in LLVM release
`llvmorg-20.1.8`, commit `87f0227cb60147a26a1eeb4fb06e3b505e9c7261`.
Their upstream license is retained in [LICENSE.TXT](LICENSE.TXT).
They are test data; AZSLC does not build, link or execute any Clang library.
Their original whitespace is retained. The local `.gitattributes` prevents
checkout newline conversion and whitespace cleanup of these hashed inputs.

`cases.json` records the SHA-256 of each input and the expected preprocessing token
spellings, or an expected rejection under strict C++20 preprocessing. Expectations
were captured using Clang 23.1.0 with `-E -P -x c++ -std=c++20 -pedantic-errors`.
The original upstream RUN/CHECK annotations are retained as provenance. This
harness does not execute those annotations or claim to run Clang's entire suite.

The selection exercises argument prescan, rescan suppression, function-call
boundaries, placemarkers, token pasting, stringification, whitespace, variadics,
`__VA_OPT__`, raw strings, `_Pragma`, and conditional integer conversions.
Positive results are compared as tokens. Negative inputs require a preprocessing
diagnostic; subsequent parser diagnostics in upstream annotations are outside
this harness.

The parent `preprocessing_conformance.py` runs these expectations without an
external compiler. Its optional `--reference /path/to/clang` also reruns the
reference comparison. Reference output is lexed in preprocessed mode; the separate
fixed-token lexical tests provide independent expectations for token boundaries.

When updating fixtures, retain their original contents and license, record the
new upstream revision, regenerate hashes and expectations, and review each
behavioral difference. Expected output must not be updated merely to make an
AZSLC failure pass.
