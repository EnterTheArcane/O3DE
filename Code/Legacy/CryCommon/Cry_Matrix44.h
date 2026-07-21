/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

// ###################################################################################
// ##  CryCommon -> AzCore migration: the legacy Cry `Matrix44` type and the           ##
// ##  `Matrix44_tpl` template were REMOVED (no real users). Use AZ::Matrix4x4.         ##
// ###################################################################################

#include <AzCore/Math/Matrix4x4.h>
