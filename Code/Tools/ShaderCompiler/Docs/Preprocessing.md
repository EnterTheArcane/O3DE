# Native AZSL Preprocessing

AZSL preprocesses source tokens before ANTLR parsing. Normal compilation creates no flattened translation unit, UTF-32 copy or preprocessing product. `azslc -E` explicitly exports preprocessed text.

## Ownership

A `CompilationUnit` owns source buffers, generated spellings, final tokens, dependencies and diagnostics. It is noncopyable and nonmovable, accepts one `PreprocessRequest`, and must outlive parsing, diagnostics, reflection and emission. `PreprocessResult` contains borrowed spans. `Dependencies()` and `Diagnostics()` also remain available after preprocessing fails. Exceptions own their formatted messages and survive unit destruction.

`SourceManager` reads each canonical file once into stable storage. Include instances retain their lookup paths separately, so aliases still control relative includes and repeated inclusion can execute under different macro definitions. `TextSpan` stores a buffer id and checked 32-bit UTF-8 byte offsets. Generated spellings use stable blocks of at least 64 KiB. Ordinary macro expansion copies descriptors, not spelling strings. Suppression ids are internal to expansion and cleared before publishing tokens.

Source locations distinguish use sites, spelling origins, includes and `#line` presumed locations. Diagnostic traversal reports up to 32 distinct origin notes, including both pasted operands. Filename views must not be retained while further line remaps are added. Physical line tables handle LF/CRLF and BOMs. A sparse UTF-8 continuation-byte index avoids rescanning line prefixes for columns.

`PreprocessingTokenSource` supplies source-backed `SourceToken` objects to `CommonTokenStream`. The compiler uses `TokenTextView()` to borrow text, while ANTLR's `getText()` returns an owning string. Token indices and source byte offsets are different coordinates. The adapter derives fixed spellings and compound punctuation from the generated vocabulary. It classifies other spellings with the generated lexer over a bounded UTF-8 view and caches successful classifications. No separate AZSL keyword grammar is maintained.

## Dialect

The baseline matches the engine's C++-style `-C -+` preprocessing configuration. It excludes trigraph/digraph translation, `include_next`, GNU named variadics/comma swallowing, C++20 `__VA_OPT__` and compiler-specific switches. Unsupported active directives and switches are errors.

Supported behavior:

- Object/function macros, variadics, argument prescan, rescanning, stringification, token pasting and empty arguments. Arguments beside `#`/`##` retain unexpanded spellings. Empty-expansion spacing follows checked Clang results, including documented cases where MCPP differs. Function suppression uses the invocation/closing-parenthesis intersection. Identical redefinitions are accepted, incompatible ones diagnose.
- Conditional directives and `defined`, C++ boolean constants and alternative operators. Undefined identifiers become zero. Expressions use signed/unsigned 64-bit values with explicit unsigned storage, conventional precedence and short-circuit logical/conditional evaluation. Unevaluated operands still require valid syntax. Division by zero and invalid shifts diagnose only when evaluated.
- Quoted/angled and macro-generated includes, guards, `#pragma once`, ordered forced headers and repeated inclusion. Direct header names have a dedicated lexical context, with literal backslashes and no internal macro expansion or comment recognition. Macro-generated quoted names also retain backslashes.
- `#undef`, `#error`, `#warning`, `#line` and retained non-`once` pragmas. Built-ins are `__FILE__`, `__LINE__`, `__DATE__`, `__TIME__`, `__cplusplus` (199711L) and `__AZSL__`. Location built-ins use presumed locations.
- UTF-8, BOMs, LF/CRLF continuations, comments and preprocessing character constants. Ordinary constants preserve byte packing into 64 bits and existing escape behavior. Wide `L` constants represent one Unicode scalar, independent of host `wchar_t`, with checked standard, hexadecimal, octal and universal escapes. Unknown escapes, invalid scalars and multiple wide characters diagnose. Universal escape decoding applies only to wide constants.

Inactive branches retain lexical structure and conditional nesting, but skip include names and nested `#ifdef`/`#ifndef` operands. They do not expand ordinary macros, open includes, execute errors/warnings or undergo AZSL parsing. Comments remain available on ANTLR's comment channel.

Default limits are include depth 256, macro-argument/conditional/expression depth 256, 1,000,000 expansions and 4,000,000 scanned/buffered tokens. Each invocation has fresh macro/include state. Existing diagnostics keep their numbers. Preprocessing adds error 600 and warning 601. The frontend warning callback applies suppression/promotion when a warning executes, even if a later error stops preprocessing.

## Command Line

Native preprocessing is the default for every extension. `-I`, `-D` and `-U` accept attached or separate values. Include directories and define/undefine operations retain command order. Later command-line definitions override earlier ones. `--include file` and `--include=file` are repeatable. Comment preservation (`-C`) and the compatibility dialect (`-+`) are defaults. `-` reads stdin and resolves includes from the working directory.

`-E` writes to stdout or `-o file`, using conservative spacing, AZSL compound punctuation and location markers. Newlines are normalized before platform stream translation. `--preprocessed` reads historical exports without executing macros/includes and accepts legacy inline line markers. Legacy fixtures use `.preprocessed` plus explicit `--preprocessed` arguments.

## Dependency Reports

`--dependencies` runs normal preprocessing and exits before parsing. It excludes `-E`. JSON goes to stdout or `-o file`, with `-o -` explicitly selecting stdout. Diagnostics stay on stderr and honor warning policy.

```json
{"version": 1, "complete": true, "dependencies": ["/absolute/root.azsl", "/absolute/header.azsli"]}
```

Paths are sorted, deduplicated and absolute. They include the root, forced/transitive headers and unsuccessful search candidates preceding each selected header. A missing include records all examined candidates. Lookup aliases are preserved. Inactive includes add no candidates. This lets Asset Processor detect creation of a higher-priority header.

Source errors, including invalid UTF-8 stdin, return 1 with `complete: false` and all dependencies reached before failure. Processing stops because a missing header may affect later macro conditions. Success returns 0. Command-line, output-write and process failures are not successful empty reports.

## Builder Integration and Migration

`CreateJobs` and `ProcessJob` use the same ordered compiler arguments for each platform/RHI/supervariant environment. Discovery invokes `--dependencies -o -` once per distinct environment within a request. There is no persistent cache or regex fallback. Valid partial reports register known dependencies and still schedule compilation to report the source error. Launch failures, timeouts, malformed UTF-8/JSON, inconsistent completion status and unsupported versions fail discovery.

Use `AddBuildArguments.preprocessor` in `shader_build_options.settings` for ordered `-D`, `-U`, `-I` and `--include` arguments. Relative configured paths resolve against the project. Select the configuration directory with `/O3DE/Atom/Shaders/Build/ConfigPath`. The obsolete `PreprocessorOptions` reflection and `shader_global_build_options.json` example are removed.

Quoted includes search the including directory before configured directories. Builder arguments put configured preprocessor paths before automatic source/project/ShaderLib/Gems paths. Additional `AddBuildArguments.azslc` paths follow them. The RHI header is a forced include. Argument arrays retain their boundaries, including Windows CRT quoting.

Staged `Builders/ShaderHeaders` is watched in source and installed-engine builds. External headers must also reside in Asset Processor watched locations for incremental notifications. No late source dependencies are introduced through `ProcessJobResponse`.

No prepended AZSL or `.azslin` product is generated. Product sub-id 2 remains reserved. HLSL/reflection, shader and variant identifiers remain stable. Material-generated sources, platform intermediates and shader-option macros remain supported. Builder version increments invalidate old products. Dependency discovery writes no compilation products or profiling files.

## Prior Art

- [Clang dependency collection](https://clang.llvm.org/doxygen/DependencyFile_8cpp_source.html): collect include-resolution events from the real preprocessor before emitting a report. AZSL additionally reports absent search candidates needed by Asset Processor.
- [GCC token spacing](https://gcc.gnu.org/onlinedocs/cppinternals/Token-Spacing.html): preserve whitespace through empty expansions and avoid changing token boundaries during text export. Default regressions contain checked expectations rather than requiring another preprocessor.

- [Slang frontend overview](https://shader-slang.org/slang/design/overview.html#preprocessing): separate preprocessing, replacement sequences and a buffered token stream. The corresponding [preprocessor implementation](https://github.com/shader-slang/slang/blob/961e4e59ee0181ad645825c5db3a677d5a829a9f/source/slang/slang-preprocessor.cpp) was reviewed at revision `961e4e59ee0181ad645825c5db3a677d5a829a9f`, particularly pretokenized input streams, their ownership and dependency callbacks.
- [Clang token design](https://clang.llvm.org/docs/InternalsManual.html#the-token-class): source-backed spellings and preprocessing flags.
- [Clang source locations](https://clang.llvm.org/docs/InternalsManual.html#the-sourcelocation-and-sourcemanager-classes): spelling/use locations, include context and presumed locations.
- [Clang TokenLexer.cpp](https://github.com/llvm/llvm-project/blob/012428c5e864d5e976638eb9e19ea207cd2cb829/clang/lib/Lex/TokenLexer.cpp): argument handling, pasting and rescanning, reviewed at revision `012428c5e864d5e976638eb9e19ea207cd2cb829`. These algorithms informed the design. No implementation text was imported.
- [glslang Scan.cpp](https://github.com/KhronosGroup/glslang/blob/efa016659ffc4f2ae566b6b1db71a70655ac33a1/glslang/MachineIndependent/Scan.cpp): translating preprocessing tokens to parser tokens, reviewed at revision `efa016659ffc4f2ae566b6b1db71a70655ac33a1`. No implementation text was imported.
- [ANTLR C++ TokenSource](https://github.com/antlr/antlr4/blob/8e6fd9147b3c9d36b60e2b6656871a55227efb1b/runtime/Cpp/runtime/src/TokenSource.h): the adapter implements the repository-pinned 4.13.2 runtime at revision `8e6fd9147b3c9d36b60e2b6656871a55227efb1b`. The matching `ANTLRInputStream` implementation owns decoded UTF-32, which is why the normal frontend bypasses it.
- [Microsoft command-line parsing](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments): argument-array serialization on Windows.
- [GCC include syntax](https://gcc.gnu.org/onlinedocs/gcc-9.4.0/cpp/Include-Syntax.html): header-name lexical context and literal backslash handling.

The preprocessing implementation is original code. If later work imports or adapts upstream implementation text, record the exact upstream revision and retain the relevant license here and in the imported source.
