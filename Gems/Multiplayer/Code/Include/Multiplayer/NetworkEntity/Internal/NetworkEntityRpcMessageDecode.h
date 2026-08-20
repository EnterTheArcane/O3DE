/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Multiplayer/NetworkEntity/NetworkEntityRpcMessage.h>
#include <AzNetworking/ConnectionLayer/IConnection.h>
#include <AzNetworking/ConnectionLayer/Internal/ConnectionDecodeAccess.h>
#include <AzNetworking/Serialization/Internal/DecodeContext.h>

namespace Multiplayer::Internal
{
    class NetworkEntityRpcMessageDecode final
    {
    public:
        [[nodiscard]]
        static bool GetRpcParams(
            NetworkEntityRpcMessage& message,
            IRpcParamStruct& outParams,
            AzNetworking::IConnection& invokingConnection)
        {
            const uint8_t* buffer = nullptr;
            uint32_t size = 0;
            if (message.m_data)
            {
                buffer = message.m_data->GetBuffer();
                size = static_cast<uint32_t>(message.m_data->GetSize());
            }

            AzNetworking::Internal::DecodeSession<RpcOutputSerializer> decodeSession{
                AzNetworking::Internal::ConnectionDecodeAccess::GetPermanentAdmissionCount(invokingConnection),
                buffer,
                size};
            return message.DecodeRpcParams(outParams, decodeSession.GetSerializer());
        }
    };
} // namespace Multiplayer::Internal
