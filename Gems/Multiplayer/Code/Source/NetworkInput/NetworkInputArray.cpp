/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Multiplayer/NetworkInput/NetworkInputArray.h>
#include <Multiplayer/NetworkEntity/INetworkEntityManager.h>
#include <AzNetworking/Serialization/ISerializer.h>
#include <AzNetworking/Serialization/DeltaSerializer.h>
#include <AzNetworking/Serialization/Internal/DecodeContext.h>
#include <AzCore/std/optional.h>

namespace Multiplayer
{
    AZ_CVAR(bool, net_useInputDeltaSerialization, false, nullptr, AZ::ConsoleFunctorFlags::Null, "If true, inputs will use delta-serialization to reduce RPC bandwidth");

    NetworkInputArray::NetworkInputArray()
        : m_owner()
        , m_inputs()
    {
        ;
    }

    NetworkInputArray::NetworkInputArray(const ConstNetworkEntityHandle& entityHandle)
        : m_owner(entityHandle)
        , m_inputs()
    {
        NetBindComponent* netBindComponent = entityHandle.GetNetBindComponent();
        if (netBindComponent)
        {
            for (AZStd::size_t i = 0; i < m_inputs.size(); ++i)
            {
                m_inputs[i].m_networkInput.AttachNetBindComponent(netBindComponent);
            }
        }
    }

    NetworkInput& NetworkInputArray::operator[](uint32_t index)
    {
        return m_inputs[index].m_networkInput;
    }

    const NetworkInput& NetworkInputArray::operator[](uint32_t index) const
    {
        return m_inputs[index].m_networkInput;
    }

    bool NetworkInputArray::Serialize(AzNetworking::ISerializer& serializer)
    {
        const bool isReading = serializer.GetSerializerMode() == AzNetworking::SerializerMode::WriteToObject;
        AZStd::optional<decltype(m_inputs)> candidateInputs;
        if (isReading)
        {
            candidateInputs.emplace(m_inputs);
        }
        auto* inputsPointer = &m_inputs;
        if (isReading)
        {
            inputsPointer = &*candidateInputs;
        }
        auto& inputs = *inputsPointer;

        if (net_useInputDeltaSerialization)
        {
            // Use delta-serialization to compress input RPC bandwidth usage
            // Always serialize the full first element
            if (!inputs[0].m_networkInput.Serialize(serializer))
            {
                return false;
            }

            // For each subsequent element
            for (uint32_t i = 1; i < inputs.size(); ++i)
            {
                if (serializer.GetSerializerMode() == AzNetworking::SerializerMode::WriteToObject)
                {
                    AzNetworking::SerializerDelta deltaSerializer;
                    // Read out the delta
                    if (!deltaSerializer.Serialize(serializer))
                    {
                        return false;
                    }
                    // Start with previous value
                    inputs[i].m_networkInput = inputs[i - 1].m_networkInput;
                    // Then apply delta
                    AzNetworking::DeltaSerializerApply applySerializer(deltaSerializer);
                    AzNetworking::Internal::DecodeForwardScope decodeScope(serializer, applySerializer);
                    if (!applySerializer.ApplyDelta(inputs[i].m_networkInput))
                    {
                        return false;
                    }
                }
                else
                {
                    AzNetworking::SerializerDelta deltaSerializer;
                    // Create the delta
                    AzNetworking::DeltaSerializerCreate createSerializer(deltaSerializer);
                    if (!createSerializer.CreateDelta(inputs[i - 1].m_networkInput, inputs[i].m_networkInput))
                    {
                        return false;
                    }
                    // Then write out the delta
                    if (!deltaSerializer.Serialize(serializer))
                    {
                        return false;
                    }
                }
            }
        }
        else
        {
            if (!serializer.Serialize(inputs, "InputArray"))
            {
                return false;
            }
        }

        if (isReading)
        {
            m_inputs = AZStd::move(*candidateInputs);
        }
        return true;
    }
}
