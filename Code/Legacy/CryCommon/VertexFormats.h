/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <CryCommon/Cry_Math.h>

enum EVertexFormat : uint8
{
    eVF_Unknown,

    // Base stream
    eVF_P3F_C4B_T2F,
    eVF_P3S_C4B_T2S,

    // Additional streams
    eVF_W4B_I4S,  // Skinned weights/indices stream.
    eVF_P3F,       // Velocity stream.

    // Lens effects simulation

    eVF_P2F_C4B_T2F_F4B, // UI
    eVF_P3F_C4B,// Auxiliary geometry
    eVF_Max,
};

struct UCol
{
    union
    {
        uint32 dcolor;
        uint8  bcolor[4];

        struct
        {
            uint8 b, g, r, a;
        };
        struct
        {
            uint8 z, y, x, w;
        };
    };

    // get normal vector from unsigned 8bit integers (can't point up/down and is not normal)
    ILINE AZ::Vector3 GetN()
    {
        return AZ::Vector3
               (
            (bcolor[0] - 128.0f) / 127.5f,
            (bcolor[1] - 128.0f) / 127.5f,
            (bcolor[2] - 128.0f) / 127.5f
               );
    }
};

struct Vec3f16
    : public CryHalf4
{
    _inline Vec3f16()
    {
    }
    _inline Vec3f16(f32 _x, f32 _y, f32 _z)
    {
        x = CryConvertFloatToHalf(_x);
        y = CryConvertFloatToHalf(_y);
        z = CryConvertFloatToHalf(_z);
        w = CryConvertFloatToHalf(1.0f);
    }
    float operator[](int i) const
    {
        assert(i <= 3);
        return CryConvertHalfToFloat(((CryHalf*)this)[i]);
    }
    _inline Vec3f16& operator = (const AZ::Vector3& sl)
    {
        x = CryConvertFloatToHalf(sl.GetX());
        y = CryConvertFloatToHalf(sl.GetY());
        z = CryConvertFloatToHalf(sl.GetZ());
        w = CryConvertFloatToHalf(1.0f);
        return *this;
    }
    _inline Vec3f16& operator = (const AZ::Vector4& sl)
    {
        x = CryConvertFloatToHalf(sl.GetX());
        y = CryConvertFloatToHalf(sl.GetY());
        z = CryConvertFloatToHalf(sl.GetZ());
        w = CryConvertFloatToHalf(sl.GetW());
        return *this;
    }
    _inline AZ::Vector3 ToVec3() const
    {
        AZ::Vector3 v;
        v.SetX(CryConvertHalfToFloat(x));
        v.SetY(CryConvertHalfToFloat(y));
        v.SetZ(CryConvertHalfToFloat(z));
        return v;
    }
};

struct SVF_P3F_C4B
{
    AZ::Vector3 xyz;
    UCol color;
};

struct SVF_P3F_C4B_T2F
{
    AZ::Vector3 xyz;
    UCol color;
    AZ::Vector2 st;
};

struct SVF_P2F_C4B_T2F_F4B
{
    AZ::Vector2 xy;
    UCol color;
    AZ::Vector2 st;
    uint8 texIndex;
    uint8 texHasColorChannel;
    uint8 texIndex2;
    uint8 pad;
};

struct SVF_P3F
{
    AZ::Vector3 xyz;
};
