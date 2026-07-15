/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Platform/DeviceDriverFactory.h>

namespace AZ::Editor::Connexion3D
{
    class NullDeviceDriver final
        : public IDeviceDriver
    {
    public:
        bool GetInputMessageData([[maybe_unused]] void* nativeMessageHandle, InputMessage& message) override
        {
            message = {};
            return false;
        }
    };

    AZStd::unique_ptr<IDeviceDriver> CreateDeviceDriver()
    {
        return AZStd::make_unique<NullDeviceDriver>();
    }
} // namespace AZ::Editor::Connexion3D
