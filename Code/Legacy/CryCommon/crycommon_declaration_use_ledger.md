# CryCommon declaration and use ledger

Generated deterministically by `scripts/commit_validation/generate_crycommon_ledger.py`.

## Coverage summary

| Metric | Count |
| --- | ---: |
| CryCommon public header files scanned | 85 |
| Aggregated declaration identifiers | 1575 |
| Declaration sites | 2310 |
| Unclassified declaration candidates | 29 |
| Explicit `CryCommon/...` include directives outside CryCommon | 97 |
| Resolved unqualified CryCommon include directives outside CryCommon | 391 |
| Ambiguous/unresolved CryCommon-like include directives | 0 |
| `Legacy::CryCommon` dependency declarations | 68 |
| CMake files containing those dependencies | 34 |
| Symbols with best-effort zero external references | 753 |
| Symbols flagged low-confidence/ambiguous | 483 |

## Interpretation limits

- Declarations are found lexically in installed/public CryCommon headers; this is not a Clang AST.
- Nested declaration visibility, overload identity, aliases, conditional compilation, generated/restricted source, and operator uses can be ambiguous.
- External references are identifier matches with comments, literals, include directives, locally declared macro names, build output, Cache, and third-party `External` trees removed; uses in other preprocessor directives and macro replacement bodies are counted.
- A reference is “inferred transitive” only when the source includes a CryCommon header whose in-tree include closure reaches the declaration header.
- “Unresolved reference” includes PCH, target-propagated, generated, and coincidentally same-named symbols. Zero matches never authorize deletion by themselves.
- The CSV records exact external include edges, direct and inferred intra-CryCommon include edges, and one bounded row per symbol/file match; aggregate report numbers can therefore be audited back to paths.
- Serialized names in assets and runtime-generated source are outside the symbol counter and require the separate serialization/layout validation plan.

## Symbol dispositions

| Disposition | Symbols | Meaning |
| --- | ---: | --- |
| `active_external_use` | 401 | Best-effort external references exist. |
| `ambiguous_external_use` | 239 | References exist, but the identifier cannot be attributed reliably. |
| `ambiguous_zero_use` | 97 | No match was found for an intrinsically ambiguous identifier. |
| `compatibility_retained` | 185 | Foundational source-compatibility declaration; not a deletion candidate. |
| `deferred_systemic` | 256 | Ownership/lifecycle/system migration is deferred. |
| `dependency_forward_declaration` | 22 | Dependency name declared by a CryCommon header; matches are not attributed as CryCommon uses. |
| `retirement_in_progress` | 2 | Declared in StlUtils/MiniQueue and still externally referenced. |
| `zero_external_use_unresolved` | 342 | No match found; export/install/transitive review still required. |
| `zero_use_retirement_candidate` | 31 | Retirement header declaration with no best-effort external match. |

## Headers with external include edges

| Header | Explicit directives/files | Unqualified directives/files | Category |
| --- | ---: | ---: | --- |
| `AnimKey.h` | 0/0 | 9/9 | `general_public_surface` |
| `BaseTypes.h` | 0/0 | 2/2 | `general_public_surface` |
| `CryFile.h` | 0/0 | 5/5 | `general_public_surface` |
| `CryListenerSet.h` | 1/1 | 1/1 | `deferred_systemic` |
| `CryPath.h` | 2/2 | 10/10 | `general_public_surface` |
| `CrySystemBus.h` | 3/3 | 9/9 | `deferred_systemic` |
| `CryVersion.h` | 0/0 | 2/2 | `general_public_surface` |
| `Cry_Color.h` | 1/1 | 1/1 | `compatibility_retained` |
| `Cry_Math.h` | 1/1 | 0/0 | `compatibility_retained` |
| `Cry_Matrix44.h` | 1/1 | 0/0 | `compatibility_retained` |
| `ICmdLine.h` | 0/0 | 4/4 | `general_public_surface` |
| `IConsole.h` | 3/3 | 25/25 | `deferred_systemic` |
| `IFont.h` | 3/3 | 9/9 | `general_public_surface` |
| `IGem.h` | 0/0 | 19/19 | `deferred_systemic` |
| `IIndexedMesh.h` | 0/0 | 3/3 | `general_public_surface` |
| `ILevelSystem.h` | 1/1 | 8/8 | `general_public_surface` |
| `ILocalizationManager.h` | 1/1 | 6/6 | `general_public_surface` |
| `ILog.h` | 0/0 | 10/10 | `general_public_surface` |
| `IMovieSystem.h` | 0/0 | 40/40 | `general_public_surface` |
| `IRenderer.h` | 1/1 | 9/9 | `general_public_surface` |
| `ISerialize.h` | 0/0 | 3/3 | `general_public_surface` |
| `ISplines.h` | 0/0 | 6/6 | `general_public_surface` |
| `ISystem.h` | 6/6 | 59/59 | `deferred_systemic` |
| `ITexture.h` | 0/0 | 2/2 | `general_public_surface` |
| `IValidator.h` | 0/0 | 1/1 | `general_public_surface` |
| `IWindowMessageHandler.h` | 0/0 | 1/1 | `general_public_surface` |
| `IXml.h` | 0/0 | 13/13 | `general_public_surface` |
| `LCGRandom.h` | 1/1 | 0/0 | `compatibility_retained` |
| `LoadScreenBus.h` | 2/2 | 2/2 | `general_public_surface` |
| `Maestro/Bus/EditorSequenceAgentComponentBus.h` | 0/0 | 2/2 | `general_public_surface` |
| `Maestro/Bus/EditorSequenceBus.h` | 1/1 | 0/0 | `general_public_surface` |
| `Maestro/Bus/EditorSequenceComponentBus.h` | 5/5 | 4/4 | `general_public_surface` |
| `Maestro/Bus/SequenceAgentComponentBus.h` | 0/0 | 4/4 | `general_public_surface` |
| `Maestro/Bus/SequenceComponentBus.h` | 2/2 | 4/4 | `general_public_surface` |
| `Maestro/Types/AnimNodeType.h` | 13/13 | 18/18 | `general_public_surface` |
| `Maestro/Types/AnimParamType.h` | 24/24 | 17/17 | `general_public_surface` |
| `Maestro/Types/AnimValueType.h` | 12/12 | 18/18 | `general_public_surface` |
| `Maestro/Types/AssetBlendKey.h` | 2/2 | 8/8 | `general_public_surface` |
| `Maestro/Types/AssetBlends.h` | 0/0 | 4/4 | `general_public_surface` |
| `Maestro/Types/SequenceType.h` | 4/4 | 3/3 | `general_public_surface` |
| `MainThreadRenderRequestBus.h` | 1/1 | 2/2 | `general_public_surface` |
| `MathConversion.h` | 1/1 | 0/0 | `compatibility_retained` |
| `Mocks/ICVarMock.h` | 0/0 | 1/1 | `general_public_surface` |
| `Mocks/IConsoleMock.h` | 0/0 | 5/4 | `general_public_surface` |
| `Mocks/ICryPakMock.h` | 0/0 | 3/3 | `general_public_surface` |
| `Mocks/ISystemMock.h` | 1/1 | 5/5 | `general_public_surface` |
| `MultiThread_Containers.h` | 0/0 | 2/2 | `general_public_surface` |
| `ProjectDefines.h` | 0/0 | 1/1 | `general_public_surface` |
| `Random.h` | 0/0 | 2/2 | `general_public_surface` |
| `Range.h` | 0/0 | 11/11 | `general_public_surface` |
| `ScopedVariableSetter.h` | 0/0 | 3/3 | `general_public_surface` |
| `SerializeFwd.h` | 0/0 | 1/1 | `general_public_surface` |
| `SimpleSerialize.h` | 0/0 | 2/2 | `general_public_surface` |
| `TimeValue.h` | 4/4 | 1/1 | `deferred_systemic` |
| `VectorMap.h` | 0/0 | 1/1 | `general_public_surface` |
| `VertexFormats.h` | 0/0 | 1/1 | `general_public_surface` |
| `XMLBinaryHeaders.h` | 0/0 | 2/2 | `general_public_surface` |
| `platform.h` | 0/0 | 4/4 | `general_public_surface` |
| `smartptr.h` | 0/0 | 3/3 | `general_public_surface` |

## Most referenced declaration identifiers

Dependency-only forward declarations remain in the CSV but are omitted from this ranking because unrelated use of the dependency type is not evidence of CryCommon use.

| Identifier | Kind(s) | Header(s) | Uses/files | Direct/transitive/unresolved files | Disposition |
| --- | --- | --- | ---: | ---: | --- |
| `Attribute` | `class;struct` | `Vertex.h;XMLBinaryHeaders.h` | 11500/1270 | 1/1/1268 | `ambiguous_external_use` |
| `Event` | `enum_value` | `Maestro/Types/AnimNodeType.h;Maestro/Types/AnimParamType.h` | 7850/1035 | 20/0/1015 | `ambiguous_external_use` |
| `Entity` | `enum_value` | `Maestro/Types/AnimNodeType.h` | 5087/999 | 8/0/991 | `ambiguous_external_use` |
| `find` | `template_function` | `StlUtils.h` | 4940/1239 | 0/1/1238 | `retirement_in_progress` |
| `Color` | `enum_value` | `Vertex.h` | 4156/768 | 0/0/768 | `ambiguous_external_use` |
| `Value` | `alias` | `ScopedVariableSetter.h` | 3721/427 | 0/0/427 | `ambiguous_external_use` |
| `Format` | `class` | `Vertex.h` | 3088/291 | 0/1/290 | `active_external_use` |
| `iterator` | `alias` | `VectorMap.h` | 2838/331 | 0/0/331 | `active_external_use` |
| `Vector4` | `class;enum_value` | `IXml.h;Maestro/Types/AnimValueType.h` | 2812/323 | 9/7/307 | `ambiguous_external_use` |
| `Physics` | `enum_value` | `Maestro/Types/AnimParamType.h` | 2801/323 | 2/0/321 | `ambiguous_external_use` |
| `uint32` | `alias` | `BaseTypes.h` | 2320/400 | 0/34/366 | `active_external_use` |
| `Component` | `enum_value` | `Maestro/Types/AnimNodeType.h` | 2165/1148 | 9/0/1139 | `ambiguous_external_use` |
| `max` | `template_function` | `Cry_Math.h;LinuxSpecific.h` | 2033/733 | 0/26/707 | `compatibility_retained` |
| `Node` | `struct` | `XMLBinaryHeaders.h` | 1810/318 | 1/0/317 | `active_external_use` |
| `size_type` | `alias` | `VectorMap.h` | 1533/92 | 0/0/92 | `active_external_use` |
| `ValueType` | `alias` | `ISplines.h` | 1425/168 | 2/1/165 | `active_external_use` |
| `Vec4` | `struct` | `Cry_Math.h;Cry_Vector4.h` | 1370/34 | 0/0/34 | `compatibility_retained` |
| `min` | `template_function` | `Cry_Math.h;LinuxSpecific.h` | 1249/423 | 0/17/406 | `compatibility_retained` |
| `Vec3` | `alias` | `Cry_Vector3.h` | 1199/28 | 0/1/27 | `compatibility_retained` |
| `const_iterator` | `alias` | `VectorMap.h` | 1133/211 | 0/0/211 | `active_external_use` |
| `Visibility` | `enum_value` | `Maestro/Types/AnimParamType.h` | 1128/467 | 5/0/462 | `ambiguous_external_use` |
| `hash` | `template_type` | `Maestro/Bus/SequenceAgentComponentBus.h;Maestro/Bus/SequenceComponentBus.h` | 1091/255 | 0/0/255 | `active_external_use` |
| `value_type` | `alias` | `Cry_Vector2.h;Cry_Vector3.h;Cry_Vector4.h;ISplines.h;MultiThread_Containers.h;VectorMap.h` | 847/148 | 1/0/147 | `compatibility_retained` |
| `pointer` | `alias` | `VectorMap.h` | 825/121 | 0/0/121 | `active_external_use` |
| `Vec2` | `alias` | `Cry_Vector2.h` | 799/21 | 0/0/21 | `compatibility_retained` |
| `swap` | `free_function;template_function` | `IXml.h;smartptr.h` | 705/215 | 0/9/206 | `active_external_use` |
| `XmlNodeRef` | `class` | `ILocalizationManager.h;IMovieSystem.h;IXml.h` | 678/147 | 37/25/85 | `active_external_use` |
| `NodeIndex` | `alias` | `XMLBinaryHeaders.h` | 654/94 | 1/0/93 | `active_external_use` |
| `uint8` | `alias` | `BaseTypes.h` | 596/108 | 0/9/99 | `active_external_use` |
| `BusIdType` | `alias` | `Maestro/Bus/EditorSequenceAgentComponentBus.h;Maestro/Bus/SequenceAgentComponentBus.h` | 548/239 | 0/0/239 | `ambiguous_external_use` |
| `int32` | `alias` | `BaseTypes.h` | 523/88 | 0/7/81 | `active_external_use` |
| `reference` | `alias` | `VectorMap.h` | 509/92 | 0/0/92 | `active_external_use` |
| `Material` | `enum_value` | `Maestro/Types/AnimNodeType.h` | 497/117 | 3/0/114 | `ambiguous_external_use` |
| `String` | `enum_value` | `Maestro/Types/AnimValueType.h` | 470/167 | 7/0/160 | `ambiguous_external_use` |
| `uint16` | `alias` | `BaseTypes.h` | 462/88 | 0/2/86 | `active_external_use` |
| `Unknown` | `enum_value` | `Maestro/Types/AnimValueType.h` | 453/228 | 8/3/217 | `ambiguous_external_use` |
| `Invalid` | `enum_value` | `Maestro/Types/AnimNodeType.h;Maestro/Types/AnimParamType.h` | 449/207 | 22/0/185 | `ambiguous_external_use` |
| `allocator_type` | `alias` | `VectorMap.h` | 431/38 | 0/0/38 | `active_external_use` |
| `AnimParamType` | `enum` | `IMovieSystem.h;Maestro/Types/AnimParamType.h` | 410/43 | 42/0/1 | `active_external_use` |
| `Group` | `enum_value` | `Maestro/Types/AnimNodeType.h` | 409/159 | 6/0/153 | `ambiguous_external_use` |
| `Camera` | `enum_value` | `Maestro/Types/AnimParamType.h` | 376/105 | 7/0/98 | `ambiguous_external_use` |
| `HandleType` | `alias` | `AppleSpecific.h;LinuxSpecific.h` | 376/84 | 0/9/75 | `active_external_use` |
| `Plane` | `alias` | `Cry_Vector3.h` | 364/43 | 0/0/43 | `compatibility_retained` |
| `ISplineInterpolator` | `struct` | `IMovieSystem.h;ISplines.h` | 336/20 | 8/0/12 | `active_external_use` |
| `Range` | `alias` | `Range.h` | 328/76 | 8/11/57 | `active_external_use` |
| `byte` | `alias` | `AndroidSpecific.h;AppleSpecific.h;Linux32Specific.h;Linux64Specific.h` | 326/68 | 0/1/67 | `active_external_use` |
| `AnimValueType` | `enum` | `IMovieSystem.h;Maestro/Bus/EditorSequenceComponentBus.h;Maestro/Types/AnimValueType.h` | 286/47 | 42/1/4 | `active_external_use` |
| `const_pointer` | `alias` | `VectorMap.h` | 283/36 | 0/0/36 | `active_external_use` |
| `key_type` | `alias` | `ISplines.h;VectorMap.h` | 282/31 | 2/1/28 | `ambiguous_external_use` |
| `GetFileName` | `free_function` | `CryPath.h` | 263/143 | 4/0/139 | `active_external_use` |
| `Lerp` | `template_function` | `Cry_Math.h` | 258/100 | 0/0/100 | `compatibility_retained` |
| `AnimNodeType` | `enum` | `IMovieSystem.h;Maestro/Types/AnimNodeType.h` | 256/36 | 34/0/2 | `active_external_use` |
| `GetPath` | `free_function` | `CryPath.h` | 251/82 | 5/0/77 | `active_external_use` |
| `CAnimParamType` | `class` | `IMovieSystem.h` | 241/60 | 11/6/43 | `active_external_use` |
| `Vector` | `enum_value` | `Maestro/Types/AnimValueType.h` | 241/15 | 5/0/10 | `ambiguous_external_use` |
| `MutexType` | `alias` | `LoadScreenBus.h` | 236/153 | 0/0/153 | `active_external_use` |
| `queue` | `template_type` | `MultiThread_Containers.h` | 234/71 | 2/0/69 | `active_external_use` |
| `DWORD` | `alias` | `AndroidSpecific.h;AppleSpecific.h;Linux32Specific.h;Win64specific.h` | 226/62 | 0/6/56 | `active_external_use` |
| `IAnimSequence` | `struct` | `IMovieSystem.h` | 225/45 | 11/9/25 | `active_external_use` |
| `IAnimTrack` | `struct` | `IMovieSystem.h` | 222/63 | 21/5/37 | `active_external_use` |
| `IConsole` | `struct` | `IConsole.h;ISystem.h` | 219/93 | 13/3/77 | `deferred_systemic` |
| `wchar_t` | `alias` | `AppleSpecific.h;LinuxSpecific.h` | 215/60 | 0/6/54 | `active_external_use` |
| `Select` | `enum_value` | `Maestro/Types/AnimValueType.h` | 203/90 | 7/0/83 | `ambiguous_external_use` |
| `const_reference` | `alias` | `VectorMap.h` | 191/38 | 0/0/38 | `active_external_use` |
| `Bool` | `enum_value` | `Maestro/Types/AnimValueType.h` | 189/61 | 8/0/53 | `ambiguous_external_use` |
| `IAnimNode` | `struct` | `IMovieSystem.h` | 180/38 | 9/8/21 | `active_external_use` |
| `ICVar` | `struct` | `IConsole.h` | 175/42 | 12/9/21 | `deferred_systemic` |
| `Construct` | `template_function` | `platform.h` | 162/16 | 0/0/16 | `active_external_use` |
| `ReplaceExtension` | `free_function` | `CryPath.h` | 161/80 | 3/0/77 | `active_external_use` |
| `__m128` | `alias` | `Linux_Win32Wrapper.h` | 158/5 | 0/0/5 | `active_external_use` |
| `reverse_iterator` | `alias` | `VectorMap.h` | 157/37 | 0/0/37 | `active_external_use` |
| `AttributeType` | `enum` | `Vertex.h` | 145/14 | 0/0/14 | `active_external_use` |
| `const_reverse_iterator` | `alias` | `VectorMap.h` | 145/29 | 0/0/29 | `active_external_use` |
| `uint` | `alias` | `BaseTypes.h;platform.h` | 144/54 | 0/11/43 | `active_external_use` |
| `IMovieSystem` | `struct` | `IMovieSystem.h;ISystem.h` | 143/34 | 11/10/13 | `deferred_systemic` |
| `HANDLE` | `alias` | `AppleSpecific.h;LinuxSpecific.h` | 141/44 | 0/5/39 | `active_external_use` |
| `Normal` | `enum_value` | `Vertex.h` | 136/87 | 0/0/87 | `ambiguous_external_use` |
| `BezierSpline` | `template_type` | `ISplines.h` | 126/12 | 2/0/10 | `active_external_use` |
| `Float` | `enum_value` | `Maestro/Types/AnimParamType.h;Maestro/Types/AnimValueType.h` | 121/36 | 12/0/24 | `ambiguous_external_use` |
| `SetFlags` | `template_function` | `platform.h` | 117/66 | 0/19/47 | `active_external_use` |
| `type_identity` | `enum` | `Cry_Math.h` | 116/15 | 0/0/15 | `compatibility_retained` |
| `Rotation` | `enum_value` | `Maestro/Types/AnimParamType.h` | 112/33 | 9/0/24 | `ambiguous_external_use` |
| `SequenceComponentRequestBus` | `alias` | `Maestro/Bus/SequenceComponentBus.h` | 108/6 | 3/3/0 | `active_external_use` |
| `Console` | `enum_value` | `Maestro/Types/AnimParamType.h` | 100/42 | 7/0/35 | `ambiguous_external_use` |
| `Position` | `enum_value` | `Maestro/Types/AnimParamType.h;Vertex.h` | 97/34 | 10/0/24 | `ambiguous_external_use` |
| `Scale` | `enum_value` | `Maestro/Types/AnimParamType.h` | 97/45 | 8/0/37 | `ambiguous_external_use` |
| `GetISystem` | `free_function` | `ISystem.h` | 95/22 | 6/7/9 | `deferred_systemic` |
| `ISystem` | `struct` | `CrySystemBus.h;IConsole.h;IFont.h;ISystem.h` | 95/58 | 34/6/18 | `deferred_systemic` |
| `Indices` | `enum_value` | `Vertex.h` | 94/11 | 0/0/11 | `ambiguous_external_use` |
| `UINT` | `alias` | `AppleSpecific.h;LinuxSpecific.h` | 92/47 | 0/11/36 | `active_external_use` |
| `Capture` | `enum_value` | `Maestro/Types/AnimParamType.h` | 90/40 | 6/0/34 | `ambiguous_external_use` |
| `SequenceComponentRequests` | `class` | `Maestro/Bus/SequenceComponentBus.h` | 89/10 | 3/5/2 | `active_external_use` |
| `FontFamily` | `struct` | `IFont.h` | 88/9 | 1/0/8 | `active_external_use` |
| `IKey` | `struct` | `AnimKey.h;IMovieSystem.h` | 88/33 | 14/0/19 | `active_external_use` |
| `IConsoleCmdArgs` | `struct` | `IConsole.h;ILevelSystem.h` | 87/33 | 9/8/16 | `deferred_systemic` |
| `BOOL` | `alias` | `AppleSpecific.h;LinuxSpecific.h` | 86/36 | 0/3/33 | `active_external_use` |
| `Animation` | `enum_value` | `Maestro/Types/AnimParamType.h` | 82/36 | 7/0/29 | `ambiguous_external_use` |
| `SAnimContext` | `struct` | `IMovieSystem.h` | 82/36 | 3/6/27 | `active_external_use` |
| `CryLogAlways` | `free_function;function_macro` | `ISystem.h` | 79/17 | 3/4/10 | `deferred_systemic` |
| `CryWarning` | `free_function` | `ISystem.h` | 77/18 | 10/2/6 | `deferred_systemic` |

## CMake dependency ledger

| Target (best effort) | Location |
| --- | --- |
| `EditorCore` | `Code/Editor/CMakeLists.txt:33` |
| `EditorLib` | `Code/Editor/CMakeLists.txt:114` |
| `Editor` | `Code/Editor/CMakeLists.txt:158` |
| `EditorCore.Tests` | `Code/Editor/CMakeLists.txt:205` |
| `EditorLib.Tests` | `Code/Editor/CMakeLists.txt:234` |
| `Launcher.Static` | `Code/LauncherUnified/CMakeLists.txt:29` |
| `Launcher.Headless.Static` | `Code/LauncherUnified/CMakeLists.txt:48` |
| `Launcher.Game.Static` | `Code/LauncherUnified/CMakeLists.txt:65` |
| `Launcher.Server.Static` | `Code/LauncherUnified/CMakeLists.txt:81` |
| `Launcher.Unified.Static` | `Code/LauncherUnified/CMakeLists.txt:100` |
| `CrySystem.Static` | `Code/Legacy/CrySystem/CMakeLists.txt:31` |
| `CrySystem` | `Code/Legacy/CrySystem/CMakeLists.txt:57` |
| `CrySystem.XMLBinary` | `Code/Legacy/CrySystem/XML/CMakeLists.txt:19` |
| `RemoteConsoleCore` | `Code/Tools/RemoteConsole/CMakeLists.txt:25` |
| `${gem_name}.Static` | `Gems/AssetValidation/Code/CMakeLists.txt:23` |
| `${gem_name}.Static` | `Gems/AtomLyIntegration/AtomBridge/Code/CMakeLists.txt:30` |
| `${gem_name}` | `Gems/AtomLyIntegration/AtomFont/Code/CMakeLists.txt:26` |
| `${gem_name}.Editor.Static` | `Gems/AtomLyIntegration/CommonFeatures/Code/CMakeLists.txt:149` |
| `${gem_name}.Private.Object` | `Gems/AudioSystem/Code/CMakeLists.txt:55` |
| `${gem_name}.Tests` | `Gems/AudioSystem/Code/CMakeLists.txt:113` |
| `${gem_name}.Editor.Private.Object` | `Gems/AudioSystem/Code/CMakeLists.txt:233` |
| `${gem_name}.Static` | `Gems/CertificateManager/Code/CMakeLists.txt:23` |
| `${gem_name}.Static` | `Gems/DebugDraw/Code/CMakeLists.txt:39` |
| `${gem_name}.Editor` | `Gems/DebugDraw/Code/CMakeLists.txt:92` |
| `${gem_name}StaticLib` | `Gems/EMotionFX/Code/CMakeLists.txt:34` |
| `${gem_name}.Tests` | `Gems/EMotionFX/Code/CMakeLists.txt:181` |
| `${gem_name}.Headers` | `Gems/GameStateSamples/Code/CMakeLists.txt:27` |
| `${gem_name}.Static` | `Gems/Gestures/Code/CMakeLists.txt:23` |
| `${gem_name}` | `Gems/Gestures/Code/CMakeLists.txt:40` |
| `${gem_name}.ImGuiLYUtils` | `Gems/ImGui/Code/CMakeLists.txt:57` |
| `${gem_name}.Editor.Static` | `Gems/LandscapeCanvas/Code/CMakeLists.txt:32` |
| `${gem_name}.Static` | `Gems/LmbrCentral/Code/CMakeLists.txt:27` |
| `${gem_name}` | `Gems/LmbrCentral/Code/CMakeLists.txt:56` |
| `${gem_name}.Editor.Static` | `Gems/LmbrCentral/Code/CMakeLists.txt:96` |
| `LmbrCentral.Tests` | `Gems/LmbrCentral/Code/Tests/CMakeLists.txt:36` |
| `LmbrCentral.Editor.Tests` | `Gems/LmbrCentral/Code/Tests/CMakeLists.txt:64` |
| `${gem_name}.Static` | `Gems/LyShine/Code/CMakeLists.txt:25` |
| `${gem_name}` | `Gems/LyShine/Code/CMakeLists.txt:49` |
| `${gem_name}.Tools.Static` | `Gems/LyShine/Code/CMakeLists.txt:101` |
| `${gem_name}.Tools` | `Gems/LyShine/Code/CMakeLists.txt:129` |
| `${gem_name}.Builders.Static` | `Gems/LyShine/Code/CMakeLists.txt:159` |
| `${gem_name}.Builders` | `Gems/LyShine/Code/CMakeLists.txt:189` |
| `${gem_name}.Tests` | `Gems/LyShine/Code/CMakeLists.txt:221` |
| `${gem_name}.Editor.Tests` | `Gems/LyShine/Code/CMakeLists.txt:254` |
| `${gem_name}.Static` | `Gems/LyShineExamples/Code/CMakeLists.txt:23` |
| `${gem_name}.Static` | `Gems/Maestro/Code/CMakeLists.txt:21` |
| `${gem_name}` | `Gems/Maestro/Code/CMakeLists.txt:42` |
| `${gem_name}.Editor` | `Gems/Maestro/Code/CMakeLists.txt:87` |
| `${gem_name}.Tests` | `Gems/Maestro/Code/CMakeLists.txt:130` |
| `${gem_name}.Static` | `Gems/MessagePopup/Code/CMakeLists.txt:21` |
| `set:MICROPHONE_BUILD_DEPENDENCIES` | `Gems/Microphone/Code/CMakeLists.txt:23` |
| `${gem_name}.Editor.Static` | `Gems/PhysX/Core/PhysX5/CMakeLists.txt:145` |
| `${gem_name}` | `Gems/PhysX/Debug/PhysX5/CMakeLists.txt:46` |
| `${gem_name}.Editor` | `Gems/PhysX/Debug/PhysX5/CMakeLists.txt:100` |
| `${gem_name}.Static` | `Gems/SceneLoggingExample/Code/CMakeLists.txt:25` |
| `${gem_name}.Editor.Static` | `Gems/ScriptCanvas/Code/CMakeLists.txt:200` |
| `${gem_name}` | `Gems/ScriptedEntityTweener/Code/CMakeLists.txt:39` |
| `${gem_name}.Static` | `Gems/TextureAtlas/Code/CMakeLists.txt:19` |
| `${gem_name}` | `Gems/TextureAtlas/Code/CMakeLists.txt:36` |
| `${gem_name}.Editor` | `Gems/TextureAtlas/Code/CMakeLists.txt:64` |
| `${gem_name}.Static` | `Gems/TickBusOrderViewer/Code/CMakeLists.txt:21` |
| `${gem_name}.Static` | `Gems/Vegetation/Code/CMakeLists.txt:27` |
| `${gem_name}` | `Gems/Vegetation/Code/CMakeLists.txt:45` |
| `${gem_name}.Editor` | `Gems/Vegetation/Code/CMakeLists.txt:85` |
| `${gem_name}.Tests` | `Gems/Vegetation/Code/CMakeLists.txt:128` |
| `${gem_name}.Static` | `Gems/VirtualGamepad/Code/CMakeLists.txt:23` |
| `${gem_name}.Static` | `Gems/WhiteBox/Code/CMakeLists.txt:47` |
| `${gem_name}.Editor.Static` | `Gems/WhiteBox/Code/CMakeLists.txt:99` |

## Unresolved declaration candidates

These candidates are deliberately retained in the ledger instead of being silently classified as unused.

| Location | Candidate |
| --- | --- |
| `AnimKey.h:538` | `AZ_TYPE_INFO_SPECIALIZE(IKey, );` |
| `AnimKey.h:539` | `AZ_TYPE_INFO_SPECIALIZE(IBoolKey, );` |
| `AnimKey.h:540` | `AZ_TYPE_INFO_SPECIALIZE(ICaptureKey, );` |
| `AnimKey.h:541` | `AZ_TYPE_INFO_SPECIALIZE(ICharacterKey, );` |
| `AnimKey.h:542` | `AZ_TYPE_INFO_SPECIALIZE(ICommentKey, );` |
| `AnimKey.h:543` | `AZ_TYPE_INFO_SPECIALIZE(IConsoleKey, );` |
| `AnimKey.h:544` | `AZ_TYPE_INFO_SPECIALIZE(IDiscreteFloatKey, );` |
| `AnimKey.h:545` | `AZ_TYPE_INFO_SPECIALIZE(IEventKey, );` |
| `AnimKey.h:546` | `AZ_TYPE_INFO_SPECIALIZE(ILookAtKey, );` |
| `AnimKey.h:547` | `AZ_TYPE_INFO_SPECIALIZE(IScreenFaderKey, );` |
| `AnimKey.h:548` | `AZ_TYPE_INFO_SPECIALIZE(ISelectKey, );` |
| `AnimKey.h:549` | `AZ_TYPE_INFO_SPECIALIZE(ISequenceKey, );` |
| `AnimKey.h:550` | `AZ_TYPE_INFO_SPECIALIZE(ISoundKey, );` |
| `AnimKey.h:551` | `AZ_TYPE_INFO_SPECIALIZE(ITimeRangeKey, );` |
| `AnimKey.h:552` | `AZ_TYPE_INFO_SPECIALIZE(IStringKey, );` |
| `CrySystemBus.h:79` | `DECLARE_EBUS_EXTERN(CrySystemRequests);` |
| `Cry_Color.h:285` | `AZ_PUSH_DISABLE_WARNING(4996, );` |
| `Cry_Color.h:331` | `AZ_PUSH_DISABLE_WARNING(4996, );` |
| `Cry_Vector2.h:367` | `AZ_TYPE_INFO_SPECIALIZE(Vec2, );` |
| `Cry_Vector3.h:1376` | `AZ_TYPE_INFO_SPECIALIZE(Vec3, );` |
| `ISplines.h:1268` | `AZ_TYPE_INFO_SPECIALIZE(spline::SplineKey<AZ::Vector2>, );` |
| `Maestro/Types/AssetBlendKey.h:45` | `AZ_TYPE_INFO_SPECIALIZE(IAssetBlendKey, );` |
| `MathConversion.h:79` | `AZ_PUSH_DISABLE_WARNING(4996, );` |
| `MathConversion.h:89` | `AZ_POP_DISABLE_WARNING AZ_PUSH_DISABLE_WARNING(4996, );` |
| `MathConversion.h:102` | `AZ_POP_DISABLE_WARNING AZ_PUSH_DISABLE_WARNING(4996, );` |
| `MathConversion.h:121` | `AZ_POP_DISABLE_WARNING AZ_PUSH_DISABLE_WARNING(4996, );` |
| `Range.h:130` | `AZ_TYPE_INFO_SPECIALIZE(Range, );` |
| `Vertex.h:40` | `static AttributeUsageData AttributeUsageDataTable[(uint)AttributeUsage::NumUsages] = {` |
| `Vertex.h:91` | `static AttributeTypeData AttributeTypeDataTable[(unsigned int)AZ::Vertex::AttributeType::NumTypes] = {` |

The CSV beside this report is the full per-symbol/header/dependency ledger. Regenerate both artifacts with:

```console
python3 scripts/commit_validation/generate_crycommon_ledger.py
```

Verify that checked-in artifacts are current with:

```console
python3 scripts/commit_validation/generate_crycommon_ledger.py --check
```
