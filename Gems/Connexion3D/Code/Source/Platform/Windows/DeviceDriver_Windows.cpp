/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Platform/DeviceDriverFactory.h>
#include <Platform/Windows/DeviceDriver_Windows.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/std/containers/vector.h>

namespace AZ::Editor::Connexion3D
{
    namespace
    {
        constexpr USHORT RawInputUsagePage = 1;
        constexpr USHORT MultiAxisControllerUsage = 8;

        int ReadSigned16(const BYTE* bytes)
        {
            return static_cast<SHORT>(bytes[0] | (bytes[1] << 8));
        }
    } // namespace

    WindowsDeviceDriver::WindowsDeviceDriver()
    {
        InitializeDevice();
    }

    bool WindowsDeviceDriver::InitializeDevice()
    {
        UINT deviceCount = 0;
        if (GetRawInputDeviceList(nullptr, &deviceCount, sizeof(RAWINPUTDEVICELIST)) != 0)
        {
            return false;
        }

        m_deviceList.resize(deviceCount);
        if (deviceCount > 0
            && GetRawInputDeviceList(m_deviceList.data(), &deviceCount, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1))
        {
            return false;
        }

        for (const RAWINPUTDEVICELIST& device : m_deviceList)
        {
            if (device.dwType != RIM_TYPEHID)
            {
                continue;
            }

            RID_DEVICE_INFO deviceInfo{};
            deviceInfo.cbSize = sizeof(deviceInfo);
            UINT deviceInfoSize = sizeof(deviceInfo);
            if (GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICEINFO, &deviceInfo, &deviceInfoSize) == static_cast<UINT>(-1))
            {
                continue;
            }

            const RID_DEVICE_INFO_HID& hidInfo = deviceInfo.hid;
            if (hidInfo.usUsagePage == RawInputUsagePage && hidInfo.usUsage == MultiAxisControllerUsage)
            {
                m_registeredDevices.push_back(RAWINPUTDEVICE{ hidInfo.usUsagePage, hidInfo.usUsage, 0, nullptr });
            }
        }

        return m_registeredDevices.empty()
            || RegisterRawInputDevices(
                m_registeredDevices.data(), aznumeric_cast<UINT>(m_registeredDevices.size()), sizeof(RAWINPUTDEVICE)) != FALSE;
    }

    bool WindowsDeviceDriver::GetInputMessageData(void* nativeMessageHandle, InputMessage& message)
    {
        message = {};

        UINT rawInputSize = 0;
        HRAWINPUT rawInputHandle = static_cast<HRAWINPUT>(nativeMessageHandle);
        if (GetRawInputData(rawInputHandle, RID_INPUT, nullptr, &rawInputSize, sizeof(RAWINPUTHEADER)) != 0 || rawInputSize == 0)
        {
            return false;
        }

        AZStd::vector<AZ::u8> rawInputBuffer(rawInputSize);
        if (GetRawInputData(
                rawInputHandle, RID_INPUT, rawInputBuffer.data(), &rawInputSize, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
        {
            return false;
        }

        const RAWINPUT* rawInput = reinterpret_cast<const RAWINPUT*>(rawInputBuffer.data());
        if (rawInput->header.dwType != RIM_TYPEHID)
        {
            return true;
        }

        const RAWHID& hid = rawInput->data.hid;
        const size_t byteCount = static_cast<size_t>(hid.dwSizeHid) * hid.dwCount;
        const BYTE* bytes = hid.bRawData;
        if (byteCount == 0)
        {
            return true;
        }

        if ((bytes[0] == 1 || bytes[0] == 2) && byteCount >= 7)
        {
            int* rawValues = bytes[0] == 1 ? message.m_rawTranslation : message.m_rawRotation;
            rawValues[0] = ReadSigned16(bytes + 1);
            rawValues[1] = ReadSigned16(bytes + 3);
            rawValues[2] = ReadSigned16(bytes + 5);

            AZ::Vector3& values = bytes[0] == 1 ? message.m_translation : message.m_rotation;
            values = AZ::Vector3(
                rawValues[0] / 255.0f * m_multiplier,
                rawValues[1] / 255.0f * m_multiplier,
                rawValues[2] / 255.0f * m_multiplier);

            message.m_gotTranslation = bytes[0] == 1;
            message.m_gotRotation = bytes[0] == 2;
        }
        else if (bytes[0] == 3 && byteCount >= 4)
        {
            message.m_buttons[0] = bytes[1];
            message.m_buttons[1] = bytes[2];
            message.m_buttons[2] = bytes[3];

            AZ_TracePrintf("Connexion3D", "Button mask: %.2x %.2x %.2x\n", bytes[3], bytes[2], bytes[1]);

            if (message.m_buttons[0] == 1)
            {
                m_multiplier /= 2.0f;
            }
            else if (message.m_buttons[0] == 2)
            {
                m_multiplier *= 2.0f;
            }
        }

        return true;
    }

    AZStd::unique_ptr<IDeviceDriver> CreateDeviceDriver()
    {
        return AZStd::make_unique<WindowsDeviceDriver>();
    }
} // namespace AZ::Editor::Connexion3D
