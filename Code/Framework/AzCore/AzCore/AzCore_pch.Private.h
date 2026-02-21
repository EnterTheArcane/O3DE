/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// AzCore Private Precompiled Header
// Includes the public PCH plus internal/private headers only used by AzCore's
// own translation units. Not propagated to dependent targets.

#pragma once

#include <AzCore/AzCore_pch.Public.h>

// Internal headers (included transitively through public headers, but precompiling
// them explicitly ensures they are covered in non-unity build configurations)
#include <AzCore/Asset/AssetInternal/WeakAsset.h>
#include <AzCore/EBus/Internal/BusContainer.h>
#include <AzCore/EBus/Internal/CallstackEntry.h>
#include <AzCore/EBus/Internal/Debug.h>
#include <AzCore/EBus/Internal/Handlers.h>
#include <AzCore/EBus/Internal/StoragePolicies.h>
#include <AzCore/Jobs/Internal/JobManagerBase.h>
#include <AzCore/Jobs/Internal/JobManagerWorkStealing.h>
#include <AzCore/Jobs/Internal/JobNotify.h>
#include <AzCore/Math/Internal/MathTypes.h>
#include <AzCore/Module/Internal/ModuleManagerSearchPathTool.h>
#include <AzCore/Name/Internal/NameData.h>
#include <AzCore/Task/Internal/Task.h>
#include <AzCore/Task/Internal/TaskConfig.h>
