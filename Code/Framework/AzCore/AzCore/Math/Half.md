# AZ::Half

`AzCore/Math/Half.h` defines a two-byte binary16 storage type. It has alignment 2, standard layout and trivial copying. Default construction produces positive zero. Storage is one unsigned 16-bit encoding, with native memory byte order. No serialized or network byte order is implied.

## Numeric and raw operations

Construction from `float`, `double` and standard integral types up to 64 bits is explicit and constexpr. Boolean, enumeration, user-defined and `long double` inputs require an explicit conversion to a supported input type. Copying preserves every bit. Numerical assignment requires construction or compound assignment.

Separate float, double, signed-integral and unsigned-integral constructor overloads express the conversion paths. A deleted fallback prevents unsupported inputs from entering through an argument conversion to float or double. Conversion helpers live directly in Half.h as implementation details.

```cpp
AZ::Half stored(0.3333333333333333);
float accumulator = stored;
accumulator *= 0.8f;
stored = AZ::Half(accumulator);

constexpr AZ::Half encoded = AZ::Half::FromBits(0x8000);
static_assert(encoded.IsZero() && encoded.IsNegative());
static_assert(encoded.GetBits() == 0x8000);
```

Encoding rounds to nearest, ties to even, relative to the represented input. Double conversion is direct, with no intermediate float. Subnormals use gradual underflow. Overflow produces signed infinity, with the nearest-even boundary at magnitude 65520. Signed zero is preserved. Numerical NaNs become quiet NaNs, without a payload or sign preservation guarantee. Every finite half value widens exactly to float, including subnormals.

Encoding and decoding are independent of the active rounding mode and FTZ/DAZ settings. They do not change floating-point control state. Floating-point exception flags and trapping behavior are not portable guarantees. Fast-math configurations that discard NaNs or signed zero cannot provide the complete numerical contract.

`FromBits`, `GetBits`, copying and unary plus preserve every encoding. Unary minus flips only the sign bit. These operations preserve signaling NaN representations. Classification methods inspect the stored bits, and `IsNegative` includes negative zero and negative NaNs.

## Expressions and containers

Implicit widening to float supplies binary arithmetic and comparisons. `Half + Half` returns float, as do the other three binary arithmetic operations. Mixed floating expressions use the wider built-in type. Integral operands follow built-in arithmetic after Half widens. Compound assignments compute the promoted expression and then encode once into Half. Arithmetic follows the computation type's normal floating-point environment and compiler rules.

```cpp
const AZ::Half a(2048);
const AZ::Half b(1);
auto precise = a + b - a; // float, equal to 1
AZ::Half rounded = a;
rounded += b;            // Half, rounds back to 2048
```

Comparisons are numerical. Both zero encodings compare equal, NaNs compare unequal, and ordered comparisons with a NaN are false. Use `GetBits()` to compare representations. `AZStd::hash<Half>` normalizes signed zero. As with floating-point keys, NaN is unsuitable when a container requires reflexive equality.

`std::numeric_limits<Half>` and its existing `AZStd` alias provide binary16 limits. `min()` is the smallest positive normal value, `denorm_min()` is the smallest positive subnormal, and `lowest()` is the most negative finite value. `is_iec559` is conservatively false because Half does not implement a complete IEEE arithmetic and exception environment. Built-in floating-point and arithmetic type traits remain false. There is no formatting, parsing, reflection or serialization surface in this release. Use a float explicitly with formatting, varargs and arithmetic-constrained generic APIs.

## Engine integration

Atom ImageProcessing uses Half for R16F, RG16F and RGBA16F channels. RPI image-data retrieval reads their two-byte channel encodings explicitly. Both paths use alignment-safe loads and stores. Neither path changes gamma, channel order or alpha association. Image builder version 36 invalidates products affected by corrected half conversion, including exceptional values and very small inputs.

Existing CryHalf types and data remain separate. A future ColorHalf adoption should replace its private conversion codec with Half construction and raw access while preserving its four raw channel fields, layout, UUID, serialization names and rounding after each color operation. Scalar float-promoted expressions do not change ColorHalf's own arithmetic contract.

C++23's optional `std::float16_t` does not automatically replace this wrapper. A future native backend must preserve this public API, storage and expression behavior.

## Design evidence

The implementation reuses and extends the O3DE color branch's portable codec. It adds no production dependency or ISA requirement. The following sources informed the contract, without importing their libraries.

| Source | Applied lesson |
| --- | --- |
| [UE FFloat16 public API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/FFloat16), inspected 2026-10-29 | Storage and conversion focus, explicit legacy conversion distinctions. Public documentation was inspected, not a UE source checkout |
| [Imath 3.2.1](https://github.com/AcademySoftwareFoundation/Imath/blob/v3.2.1/src/Imath/half.h) | Float-promoted arithmetic, rounded compound assignment, raw access and limits |
| [SourceForge half 2.2.1](https://half.sourceforge.net/) | Rounding boundaries, direct conversion and integer edge cases. A full half math library exceeds this type's scope |
| [ROCm half at 207ee58595a64](https://github.com/ROCm/half/tree/207ee58595a64b5c4a70df221f1e6e704b807811) | Related older implementation, not independent confirmation. Its default rounding configuration is not adopted |
| [DirectXMath at f5d82574b034](https://github.com/microsoft/DirectXMath/blob/f5d82574b03417d905fdd2f85bfa9bf7a2f1deac/Inc/DirectXPackedVector.h) | Separate raw encoding, numerical conversion and packed processing |
| [.NET Half at e94a98398207](https://github.com/dotnet/runtime/blob/e94a983982077208ea0217e8ea646d02f05d430f/src/libraries/System.Private.CoreLib/src/System/Half.cs) | C# comparison for direct double conversion and exceptional values. Half-valued operators are not adopted |
| [Unity Mathematics at f53664de3dad](https://github.com/Unity-Technologies/Unity.Mathematics/blob/f53664de3dad88869a91ef8ebdbecf356e1312d5/src/Unity.Mathematics/half.cs) | C# comparison. Bitwise equality and double-through-float conversion are not adopted |
| [WG21 P1467R9](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p1467r9.html) | Optional extended floating types and future interoperability |
| [SoftFloat 3e](https://www.jhauser.us/arithmetic/SoftFloat.html) | Independent qualification oracle, used only outside the production build |

Native acceptance tests live in `Math/HalfTests.cpp`, with benchmarks in `Math/HalfBenchmarks.cpp`. Integration tests exercise actual RPI and ImageProcessing entry points. Platform results and remaining gates belong in the accompanying qualification evidence, rather than being inferred from a standalone probe.
