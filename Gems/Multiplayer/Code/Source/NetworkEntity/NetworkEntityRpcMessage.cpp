/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Multiplayer/NetworkEntity/NetworkEntityRpcMessage.h>
#include <Multiplayer/IMultiplayer.h>
#include <AzNetworking/ConnectionLayer/IConnection.h>
#include <AzNetworking/Serialization/Internal/SymbolAdmissionPolicy.h>
#include <AzCore/Console/ILogger.h>

namespace Multiplayer
{
    NetworkEntityRpcMessage::NetworkEntityRpcMessage(NetworkEntityRpcMessage&& rhs)
        : m_rpcDeliveryType(rhs.m_rpcDeliveryType)
        , m_entityId(rhs.m_entityId)
        , m_componentId(rhs.m_componentId)
        , m_rpcIndex(rhs.m_rpcIndex)
        , m_data(AZStd::move(rhs.m_data))
        , m_isReliable(rhs.m_isReliable)
    {
        ;
    }

    NetworkEntityRpcMessage::NetworkEntityRpcMessage(const NetworkEntityRpcMessage& rhs)
        : m_rpcDeliveryType(rhs.m_rpcDeliveryType)
        , m_entityId(rhs.m_entityId)
        , m_componentId(rhs.m_componentId)
        , m_rpcIndex(rhs.m_rpcIndex)
        , m_isReliable(rhs.m_isReliable)
    {
        if (rhs.m_data != nullptr)
        {
            m_data = AZStd::make_unique<AzNetworking::PacketEncodingBuffer>();
            (*m_data) = (*rhs.m_data); // Deep-copy
        }
    }

    NetworkEntityRpcMessage::NetworkEntityRpcMessage(RpcDeliveryType rpcDeliveryType, NetEntityId entityId, NetComponentId componentId, RpcIndex rpcIndex, ReliabilityType isReliable)
        : m_rpcDeliveryType(rpcDeliveryType)
        , m_entityId(entityId)
        , m_componentId(componentId)
        , m_rpcIndex(rpcIndex)
        , m_isReliable(isReliable)
    {
        ;
    }

    NetworkEntityRpcMessage& NetworkEntityRpcMessage::operator =(NetworkEntityRpcMessage&& rhs)
    {
        m_rpcDeliveryType = rhs.m_rpcDeliveryType;
        m_entityId = rhs.m_entityId;
        m_componentId = rhs.m_componentId;
        m_rpcIndex = rhs.m_rpcIndex;
        m_isReliable = rhs.m_isReliable;
        m_data = AZStd::move(rhs.m_data);
        return *this;
    }

    NetworkEntityRpcMessage& NetworkEntityRpcMessage::operator =(const NetworkEntityRpcMessage& rhs)
    {
        m_rpcDeliveryType = rhs.m_rpcDeliveryType;
        m_entityId = rhs.m_entityId;
        m_componentId = rhs.m_componentId;
        m_rpcIndex = rhs.m_rpcIndex;
        m_isReliable = rhs.m_isReliable;
        if (rhs.m_data != nullptr)
        {
            m_data = AZStd::make_unique<AzNetworking::PacketEncodingBuffer>();
            *m_data = (*rhs.m_data);
        }
        else
        {
            m_data.reset();
        }

        return *this;
    }

    bool NetworkEntityRpcMessage::operator ==(const NetworkEntityRpcMessage& rhs) const
    {
        // Note that we intentionally don't compare the blob buffers themselves
        return ((m_rpcDeliveryType == rhs.m_rpcDeliveryType)
             && (m_entityId == rhs.m_entityId)
             && (m_componentId == rhs.m_componentId)
             && (m_rpcIndex == rhs.m_rpcIndex));
    }

    bool NetworkEntityRpcMessage::operator !=(const NetworkEntityRpcMessage& rhs) const
    {
        return !(*this == rhs);
    }

    uint32_t NetworkEntityRpcMessage::GetEstimatedSerializeSize() const
    {
        static constexpr uint32_t sizeOfFields = sizeof(RpcDeliveryType)
            + sizeof(NetEntityId)
            + sizeof(NetComponentId)
            + sizeof(RpcIndex);

        // ByteBuffer serializes its logical size and SerializeBytes serializes its
        // bounded byte count before the payload.
        size_t dataSize = 0;
        if (m_data)
        {
            dataSize = m_data->GetSize();
        }
        const uint32_t sizeOfBlob = static_cast<uint32_t>(
            sizeof(uint16_t) + sizeof(uint16_t) + dataSize);

        // No sliceId, remote replicator already exists so we don't need to know what type of entity this is
        return sizeOfFields + sizeOfBlob;
    }

    RpcDeliveryType NetworkEntityRpcMessage::GetRpcDeliveryType() const
    {
        return m_rpcDeliveryType;
    }

    void NetworkEntityRpcMessage::SetRpcDeliveryType(RpcDeliveryType value)
    {
        m_rpcDeliveryType = value;
    }

    NetEntityId NetworkEntityRpcMessage::GetEntityId() const
    {
        return m_entityId;
    }

    NetComponentId NetworkEntityRpcMessage::GetComponentId() const
    {
        return m_componentId;
    }

    RpcIndex NetworkEntityRpcMessage::GetRpcIndex() const
    {
        return m_rpcIndex;
    }

    bool NetworkEntityRpcMessage::SetRpcParams(IRpcParamStruct& params)
    {
        auto candidateData = AZStd::make_unique<AzNetworking::PacketEncodingBuffer>();
        RpcInputSerializer serializer(
            candidateData->GetBuffer(),
            static_cast<uint32_t>(candidateData->GetCapacity()));
        if (params.Serialize(serializer))
        {
            if (serializer.GetSize() == 0)
            {
                m_data.reset();
                return true;
            }

            candidateData->Resize(serializer.GetSize());
            m_data = AZStd::move(candidateData);
            return true;
        }

        return false;
    }

    bool NetworkEntityRpcMessage::GetRpcParams(IRpcParamStruct& outParams)
    {
        const uint8_t* buffer = nullptr;
        uint32_t size = 0;
        if (m_data)
        {
            buffer = m_data->GetBuffer();
            size = static_cast<uint32_t>(m_data->GetSize());
        }
        RpcOutputSerializer serializer(buffer, size);
        return outParams.Serialize(serializer);
    }

    bool NetworkEntityRpcMessage::GetRpcParams(
        IRpcParamStruct& outParams,
        AzNetworking::IConnection& invokingConnection)
    {
        auto& admissionPolicy = AzNetworking::Internal::GetSymbolAdmissionPolicy(invokingConnection);
        const AzNetworking::Internal::SymbolSerializationContext symbolSerializationContext{
            AzNetworking::SymbolAdmission::NetworkOrigin,
            &admissionPolicy};
        const uint8_t* buffer = nullptr;
        uint32_t size = 0;
        if (m_data)
        {
            buffer = m_data->GetBuffer();
            size = static_cast<uint32_t>(m_data->GetSize());
        }
        RpcOutputSerializer serializer(buffer, size, symbolSerializationContext);
        return outParams.Serialize(serializer);
    }

    bool NetworkEntityRpcMessage::Serialize(AzNetworking::ISerializer& serializer)
    {
        RpcDeliveryType rpcDeliveryType = m_rpcDeliveryType;
        NetEntityId entityId = m_entityId;
        NetComponentId componentId = m_componentId;
        RpcIndex rpcIndex = m_rpcIndex;
        serializer.Serialize(rpcDeliveryType, "RpcDeliveryType");
        serializer.Serialize(entityId, "EntityId");
        serializer.Serialize(componentId, "ComponentId");
        serializer.Serialize(rpcIndex, "RpcIndex");
        if (!serializer.IsValid())
        {
            return false;
        }

        using BlobSize = AZ::SizeType<AZ::RequiredBytesForValue<AzNetworking::MaxPacketSize>(), false>::Type;
        BlobSize blobSize = 0;
        if (m_data)
        {
            blobSize = static_cast<BlobSize>(m_data->GetSize());
        }
        serializer.BeginObject("data");
        serializer.Serialize(blobSize, "Size", BlobSize{0}, static_cast<BlobSize>(AzNetworking::MaxPacketSize));
        if (!serializer.IsValid())
        {
            return false;
        }

        AZStd::unique_ptr<AzNetworking::PacketEncodingBuffer> candidateData;
        if (serializer.GetSerializerMode() == AzNetworking::SerializerMode::ReadFromObject)
        {
            uint8_t emptyBuffer = 0;
            uint8_t* buffer = &emptyBuffer;
            if (m_data)
            {
                buffer = m_data->GetBuffer();
            }
            uint32_t serializedSize = blobSize;
            serializer.SerializeBytes(
                buffer,
                AzNetworking::MaxPacketSize,
                false,
                serializedSize,
                "Buffer");
            serializer.EndObject("data");
            if (!serializer.IsValid() || serializedSize != blobSize)
            {
                return false;
            }
        }
        else
        {
            uint8_t emptyBuffer = 0;
            uint8_t* buffer = &emptyBuffer;
            if (blobSize > 0)
            {
                candidateData = AZStd::make_unique<AzNetworking::PacketEncodingBuffer>();
                if (!candidateData->Resize(blobSize))
                {
                    return false;
                }
                buffer = candidateData->GetBuffer();
            }

            uint32_t serializedSize = blobSize;
            serializer.SerializeBytes(
                buffer,
                AzNetworking::MaxPacketSize,
                false,
                serializedSize,
                "Buffer");
            serializer.EndObject("data");
            if (!serializer.IsValid() || serializedSize != blobSize)
            {
                return false;
            }

        }

        // We intentionally do not serialize the reliability flag, or any other RPC metadata
        if (!serializer.IsValid())
        {
            return false;
        }

        if (serializer.GetSerializerMode() == AzNetworking::SerializerMode::WriteToObject)
        {
            m_rpcDeliveryType = rpcDeliveryType;
            m_entityId = entityId;
            m_componentId = componentId;
            m_rpcIndex = rpcIndex;
            m_data = AZStd::move(candidateData);
        }

        return true;
    }

    void NetworkEntityRpcMessage::SetReliability(ReliabilityType reliabilityType)
    {
        m_isReliable = reliabilityType;
    }

    ReliabilityType NetworkEntityRpcMessage::GetReliability() const
    {
        return m_isReliable;
    }
}
