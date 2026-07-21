/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

// ###################################################################################
// ##  CryCommon -> AzCore migration: the legacy Cry `Quat` / `QuatT` types and the   ##
// ##  `Quat_tpl` / `QuatT_tpl` / `AngleAxis_tpl` templates were REMOVED. They had no  ##
// ##  real users (only internal Cry cross-references + deprecation tests).            ##
// ##  Use AZ::Quaternion (and AZ::Transform for the rotation+translation case).       ##
// ###################################################################################

#include <AzCore/Math/Quaternion.h>
