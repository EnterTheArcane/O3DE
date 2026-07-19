/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// The Atom global prelude: one module every Slang file sees, automatically.
//
// The shader builder force-includes this module into every file of every Slang compile — authors
// never import it. It establishes the explicitly sized, spelled-out type vocabulary and
// re-exports the ShaderResourceGroup authoring attributes, so a shader needs zero boilerplate
// imports. All new first-party Slang code (ShaderLib ports, material templates, samples) uses
// these aliases instead of raw float3/float4x4.

__exported import Atom.RPI.ShaderResourceGroup;

// SCALAR

public typealias i8  = int8_t;
public typealias u8  = uint8_t;
public typealias i16 = int16_t;
public typealias u16 = uint16_t;
public typealias i32 = int32_t;
public typealias u32 = uint32_t;
public typealias i64 = int64_t;
public typealias u64 = uint64_t;

public typealias f16 = float16_t;
public typealias f32 = float32_t;
public typealias f64 = float64_t;

// VECTOR

public typealias Vector<T, let N : int> = vector<T, N>;

public typealias Vector2<T> = Vector<T, 2>;
public typealias Vector3<T> = Vector<T, 3>;
public typealias Vector4<T> = Vector<T, 4>;

public typealias Vector2I = Vector2<i32>;
public typealias Vector2U = Vector2<u32>;
public typealias Vector2F = Vector2<f32>;
public typealias Vector2D = Vector2<f64>;

public typealias Vector3I = Vector3<i32>;
public typealias Vector3U = Vector3<u32>;
public typealias Vector3F = Vector3<f32>;
public typealias Vector3D = Vector3<f64>;

public typealias Vector4I = Vector4<i32>;
public typealias Vector4U = Vector4<u32>;
public typealias Vector4F = Vector4<f32>;
public typealias Vector4D = Vector4<f64>;

// MATRIX

public typealias Matrix<T, let R : int, let C : int> = matrix<T, R, C>;

public typealias Matrix2x2<T> = Matrix<T, 2, 2>;
public typealias Matrix2x3<T> = Matrix<T, 2, 3>;
public typealias Matrix2x4<T> = Matrix<T, 2, 4>;

public typealias Matrix3x2<T> = Matrix<T, 3, 2>;
public typealias Matrix3x3<T> = Matrix<T, 3, 3>;
public typealias Matrix3x4<T> = Matrix<T, 3, 4>;

public typealias Matrix4x2<T> = Matrix<T, 4, 2>;
public typealias Matrix4x3<T> = Matrix<T, 4, 3>;
public typealias Matrix4x4<T> = Matrix<T, 4, 4>;

// REAL
//
// The platform-sized floating point family AZSL exposes through
// Atom/RPI/Platform/<OS>/AzslcPlatformHeader.azsli, where `real` is full precision on desktop and
// half precision on platforms that opt into 16-bit. Ported AZSL uses these names throughout (~2000
// references across ShaderLib), so they live here rather than in ported sources.
//
// They are fixed to 32-bit, which matches every platform the Slang path currently targets. A
// 16-bit platform cannot be served by an #ifdef here: this is a module, and modules do not see
// macros defined by whoever imports them. Selecting half precision means shipping a platform
// prelude that declares these aliases against f16 instead — the same force-include mechanism the
// per-API ApiPrelude.slang files already use.
//
// New Slang code should prefer the explicit f32/Vector3F spellings; `real` exists for parity with
// the AZSL sources it was ported from.
public typealias real = f32;
public typealias real2 = Vector2F;
public typealias real3 = Vector3F;
public typealias real4 = Vector4F;

public typealias real3x3 = Matrix3x3<f32>;
public typealias real3x4 = Matrix3x4<f32>;
public typealias real4x4 = Matrix4x4<f32>;
