/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

// ###################################################################################
// ##  CryCommon -> AzCore migration: the legacy Cry `Matrix34` type and the           ##
// ##  `Matrix34_tpl` template were REMOVED (no real users). Use AZ::Matrix3x4 /        ##
// ##  AZ::Transform.                                                                  ##
// ###################################################################################

#include <AzCore/Math/Matrix3x4.h>
