/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/RTTI.h>

namespace AZ::Editor::Connexion3D
{
    struct InputMessage
    {
        bool m_gotTranslation = false;
        bool m_gotRotation = false;
        int m_rawTranslation[3] = {};
        int m_rawRotation[3] = {};
        AZ::Vector3 m_translation = AZ::Vector3::CreateZero();
        AZ::Vector3 m_rotation = AZ::Vector3::CreateZero();
        AZ::u8 m_buttons[3] = {};
    };

    class IDeviceDriver
    {
    public:
        AZ_RTTI(IDeviceDriver, "{4F46F22C-F58E-4B31-9F40-410CDA4B26A8}");
        virtual ~IDeviceDriver() = default;

        //! Converts a platform-native input event into a Connexion3D input message.
        virtual bool GetInputMessageData(void* nativeMessageHandle, InputMessage& message) = 0;
    };
} // namespace AZ::Editor::Connexion3D
