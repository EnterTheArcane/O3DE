/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
// Description : Common vector class
#pragma once

#include <AzCore/Math/Vector4.h>

// ###################################################################################
// ##  !!! TEMPORARY CRYCOMMON -> AZCORE MIGRATION SHIM -- REMOVE IN WAVE 3 !!!       ##
// ##                                                                                ##
// ##  `Vec4` was a standalone CryCommon struct; it is now a TEMPORARY alias of       ##
// ##  AZ::Vector4 so the whole engine builds against AzCore. AZ::Vector4 already      ##
// ##  provides the arithmetic / compare / Dot / GetLength operators the old struct    ##
// ##  had, plus TEMPORARY public x/y/z/w fields (see Vector4.h) for legacy field      ##
// ##  access.                                                                        ##
// ###################################################################################
// CryCommon->AzCore migration: the `Vec4` alias was removed once all callers moved to AZ::Vector4.
// This header now just forwards to <AzCore/Math/Vector4.h>.
