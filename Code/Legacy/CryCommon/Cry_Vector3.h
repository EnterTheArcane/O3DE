/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


// Description : Common 3D vector class.
//
// CryCommon->AzCore migration: the Cry `Vec3` type (Vec3_tpl<f32>) and the generic Vec3_tpl<T>
// template (plus Ang3_tpl / AngleAxis_tpl / Plane_tpl and the Vec3_Zero/etc. constants) were
// removed. Use AZ::Vector3 directly (Euler angles are an AZ::Vector3; use AZ::Plane /
// AZ::Quaternion::CreateFromAxisAngle for the former Plane/AngleAxis types). This header now
// just forwards to the AzCore vector header for any legacy include sites that remain.
#pragma once

#include <AzCore/Math/Vector3.h>
