/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Connexion3D/IDeviceDriver.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace AZ::Editor::Connexion3D
{
    AZStd::unique_ptr<IDeviceDriver> CreateDeviceDriver();
} // namespace AZ::Editor::Connexion3D
