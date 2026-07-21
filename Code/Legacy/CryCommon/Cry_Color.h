/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


// Description : 8-bit RGBA color (ColorB).
//
// CryCommon->AzCore migration: this used to be a generic `Color_tpl<T>` template with `ColorF`
// (float) and `ColorB` (uint8) instantiations. `ColorF` is now AZ::Color and the float
// instantiation was unused, so the template was replaced with a concrete `ColorB` struct (uint8
// 0-255) for faster compiles. `ColorB` has no clean AZ::Color analog (float 0-1) and remains a
// deprecated Cry type -- prefer AZ::Color.
#pragma once

#include <platform.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Math/MathUtils.h>  // AZ::GetMin / AZ::GetMax

//////////////////////////////////////////////////////////////////////////////////////////////
// 8-bit RGBA color structure. O3DE_DEPRECATION_NOTICE(GHI-19504) - Use AZ::Color.
//////////////////////////////////////////////////////////////////////////////////////////////
struct ColorB
{
    uint8 r, g, b, a;

    ILINE ColorB() {}

    ILINE ColorB(uint8 _r, uint8 _g, uint8 _b, uint8 _a) : r(_r), g(_g), b(_b), a(_a) {}
    ILINE ColorB(uint8 _r, uint8 _g, uint8 _b) : r(_r), g(_g), b(_b), a(255) {}

    // Works together with pack_abgr8888 / RGBA8 macro.
    ILINE ColorB(const unsigned int abgr) { *(unsigned int*)(&r) = abgr; }

    ILINE ColorB(const float c)
        : r((uint8)(c * 255)), g((uint8)(c * 255)), b((uint8)(c * 255)), a((uint8)(c * 255)) {}

    AZ_PUSH_DISABLE_WARNING(4996, "-Wdeprecated-declarations");
    ILINE ColorB(const AZ::Color& c)
        : r((uint8)(c.GetR() * 255)), g((uint8)(c.GetG() * 255)), b((uint8)(c.GetB() * 255)), a((uint8)(c.GetA() * 255)) {}
    ILINE ColorB(const AZ::Color& c, float fAlpha)
        : r((uint8)(c.GetR() * 255)), g((uint8)(c.GetG() * 255)), b((uint8)(c.GetB() * 255)), a((uint8)(fAlpha * 255)) {}
    AZ_POP_DISABLE_WARNING;

    ILINE ColorB(const AZ::Vector3& c, float fAlpha)
        : r((uint8)(c.GetX() * 255)), g((uint8)(c.GetY() * 255)), b((uint8)(c.GetZ() * 255)), a((uint8)(fAlpha * 255)) {}
    ILINE ColorB(const AZ::Vector4& c)
        : r((uint8)(c.GetX() * 255)), g((uint8)(c.GetY() * 255)), b((uint8)(c.GetZ() * 255)), a((uint8)(c.GetW() * 255)) {}

    ILINE ColorB(const AZ::Vector3& vVec)
        : r((uint8)vVec.GetX()), g((uint8)vVec.GetY()), b((uint8)vVec.GetZ()), a((uint8)1) {}

    ILINE ColorB& operator=(const AZ::Vector3& v) { r = (uint8)v.GetX(); g = (uint8)v.GetY(); b = (uint8)v.GetZ(); a = (uint8)1; return *this; }

    ILINE uint8& operator[](int index)       { assert(index >= 0 && index <= 3); return ((uint8*)this)[index]; }
    ILINE uint8  operator[](int index) const { assert(index >= 0 && index <= 3); return ((uint8*)this)[index]; }

    ILINE void Set(float fR, float fG, float fB, float fA = 1.0f)
    {
        r = static_cast<uint8>(fR);
        g = static_cast<uint8>(fG);
        b = static_cast<uint8>(fB);
        a = static_cast<uint8>(fA);
    }

    ILINE ColorB operator+(const ColorB& v) const { return ColorB(uint8(r + v.r), uint8(g + v.g), uint8(b + v.b), uint8(a + v.a)); }
    ILINE ColorB operator-(const ColorB& v) const { return ColorB(uint8(r - v.r), uint8(g - v.g), uint8(b - v.b), uint8(a - v.a)); }
    ILINE ColorB operator*(const ColorB& v) const { return ColorB(uint8(r * v.r), uint8(g * v.g), uint8(b * v.b), uint8(a * v.a)); }
    ILINE ColorB& operator+=(const ColorB& v) { r += v.r; g += v.g; b += v.b; a += v.a; return *this; }
    ILINE ColorB& operator-=(const ColorB& v) { r -= v.r; g -= v.g; b -= v.b; a -= v.a; return *this; }
    ILINE ColorB& operator*=(const ColorB& v) { r *= v.r; g *= v.g; b *= v.b; a *= v.a; return *this; }

    ILINE bool operator==(const ColorB& v) const { return (r == v.r) && (g == v.g) && (b == v.b) && (a == v.a); }
    ILINE bool operator!=(const ColorB& v) const { return !(*this == v); }

    ILINE unsigned int pack_abgr8888() const { return ((unsigned int)a << 24) | ((unsigned int)b << 16) | ((unsigned int)g << 8) | (unsigned int)r; }
    ILINE unsigned int pack_argb8888() const { return ((unsigned int)a << 24) | ((unsigned int)r << 16) | ((unsigned int)g << 8) | (unsigned int)b; }

    ILINE AZ::Vector3 toVec3() const    { return AZ::Vector3(r, g, b); }
    ILINE AZ::Vector3 toVector3() const { return AZ::Vector3(r, g, b); }
    ILINE AZ::Vector4 toVector4() const { return AZ::Vector4(r, g, b, a); }

    ILINE void clamp(uint8 bottom = 0, uint8 top = 255)
    {
        r = AZ::GetMin(top, AZ::GetMax(bottom, r));
        g = AZ::GetMin(top, AZ::GetMax(bottom, g));
        b = AZ::GetMin(top, AZ::GetMax(bottom, b));
        a = AZ::GetMin(top, AZ::GetMax(bottom, a));
    }

    ILINE AZStd::array<uint8, 4> GetAsArray() const { return AZStd::array<uint8, 4>{ { r, g, b, a } }; }
};

ILINE ColorB operator*(uint8 s, const ColorB& v) { return ColorB(uint8(v.r * s), uint8(v.g * s), uint8(v.b * s), uint8(v.a * s)); }
