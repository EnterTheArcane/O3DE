/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Vector4.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Matrix3x4.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Plane.h>
#include <Cry_Math.h>
#include <Cry_Color.h>

inline AZ::Vector2 LYVec2ToAZVec2(const Vec2& source)
{
    return AZ::Vector2(source.GetX(), source.GetY());
}

inline Vec2 AZVec2ToLYVec2(const AZ::Vector2& source)
{
    return Vec2(source.GetX(), source.GetY());
}

inline AZ::Vector3 LYVec3ToAZVec3(const Vec3& source)
{
    return AZ::Vector3(source.GetX(), source.GetY(), source.GetZ());
}

inline Vec3 AZVec3ToLYVec3(const AZ::Vector3& source)
{
    return Vec3(source.GetX(), source.GetY(), source.GetZ());
}

inline AZ::Vector3 LYAng3ToAZVec3(const Ang3& source)
{
    return AZ::Vector3(source.x, source.y, source.z);
}

inline Ang3 AZVec3ToLYAng3(const AZ::Vector3& source)
{
    return Ang3(source.GetX(), source.GetY(), source.GetZ());
}

inline AZ::Vector4 LYVec4ToAZVec4(const Vec4& source)
{
    return AZ::Vector4(source.GetX(), source.GetY(), source.GetZ(), source.GetW());
}

inline Vec4 AZVec4ToLYVec4(const AZ::Vector4& source)
{
    return Vec4(source.GetX(), source.GetY(), source.GetZ(), source.GetW());
}

inline AZ::Color LYVec3ToAZColor(const Vec3& source)
{
    return AZ::Color(source.GetX(), source.GetY(), source.GetZ(), 1.0f);
}

inline Vec3 AZColorToLYVec3(const AZ::Color& source)
{
    return Vec3(source.GetR(), source.GetG(), source.GetB());
}

inline Vec4 AZColorToLYVec4(const AZ::Color& source)
{
    return Vec4(source.GetR(), source.GetG(), source.GetB(), source.GetA());
}

// Disable the deprecated-declarations warning for all conversion operators of ColorF
AZ_PUSH_DISABLE_WARNING(4996, "-Wdeprecated-declarations");
inline ColorF AZColorToLYColorF(const AZ::Color& source)
{
    return ColorF(source.ToU32());
}

inline AZ::Color LYColorFToAZColor(const ColorF& source)
{
    return AZ::Color(source.GetR(), source.GetG(), source.GetB(), source.GetA());
}
AZ_POP_DISABLE_WARNING

// Disable the deprecated-declarations warning for all conversion operators of ColorB
AZ_PUSH_DISABLE_WARNING(4996, "-Wdeprecated-declarations");
inline ColorB AZColorToLYColorB(const AZ::Color& source)
{
    return ColorB(source.ToU32());
}

inline AZ::Color LYColorBToAZColor(const ColorB& source)
{
    return AZ::Color(source.r, source.g, source.b, source.a);
}
AZ_POP_DISABLE_WARNING

// CryCommon->AzCore migration: the Matrix33/Matrix34/Transform bridge helpers were removed
// along with the (dead) Cry Matrix33/Matrix34 types. Use AZ::Matrix3x3 / AZ::Matrix3x4 /
// AZ::Transform directly.

inline AZ::Plane LyPlaneToAZPlane(const ::Plane& source)
{
    return AZ::Plane::CreateFromNormalAndDistance(LYVec3ToAZVec3(source.n), source.d);
}

inline ::Plane AZPlaneToLyPlane(const AZ::Plane& source)
{
    ::Plane resultPlane;
    resultPlane.Set(AZVec3ToLYVec3(source.GetNormal()), source.GetDistance());

    return resultPlane;
}
