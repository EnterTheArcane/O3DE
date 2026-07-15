/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Connexion3D/IDeviceDriver.h>

#include <AzCore/PlatformIncl.h>
#include <AzCore/std/containers/vector.h>

namespace AZ::Editor::Connexion3D
{
    class WindowsDeviceDriver final
        : public IDeviceDriver
    {
    public:
        WindowsDeviceDriver();
        bool GetInputMessageData(void* nativeMessageHandle, InputMessage& message) override;

    private:
        bool InitializeDevice();

        AZStd::vector<RAWINPUTDEVICELIST> m_deviceList;
        AZStd::vector<RAWINPUTDEVICE> m_registeredDevices;
        float m_multiplier = 1.0f;
    };
} // namespace AZ::Editor::Connexion3D
