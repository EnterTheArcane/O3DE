/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


// Description : Common 2D vector class.
//
// CryCommon->AzCore migration: the Cry `Vec2` type (Vec2_tpl<f32>) and the generic Vec2_tpl<T>
// template were removed. Use AZ::Vector2 directly. This header now just forwards to the AzCore
// vector header for any legacy include sites that remain.
#pragma once

#include <AzCore/Math/Vector2.h>
