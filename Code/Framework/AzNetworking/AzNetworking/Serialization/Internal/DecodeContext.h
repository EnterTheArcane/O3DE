/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzNetworking/Serialization/ISerializer.h>
#include <AzCore/std/utility/move.h>

namespace AzNetworking::Internal
{
    class DecodeContext final
    {
    public:
        explicit DecodeContext(AZ::u32& permanentAdmissionCount)
            : m_permanentAdmissionCount{&permanentAdmissionCount}
        {
        }

        [[nodiscard]]
        AZ::u32& GetPermanentAdmissionCount() const
        {
            return *m_permanentAdmissionCount;
        }

    private:
        AZ::u32* m_permanentAdmissionCount = nullptr;
    };

    class DecodeAccess final
    {
    public:
        [[nodiscard]]
        static DecodeContext* Get(const ISerializer& serializer)
        {
            return serializer.m_decodeContext;
        }

        static void Attach(
            ISerializer& serializer,
            DecodeContext* context)
        {
            serializer.m_decodeContext = context;
        }
    };

    template<class Serializer>
    class DecodeSession final
    {
    public:
        AZ_DISABLE_COPY_MOVE(DecodeSession);

        template<class... Args>
        explicit DecodeSession(
            AZ::u32& permanentAdmissionCount,
            Args&&... args)
            : m_context{permanentAdmissionCount}
            , m_serializer(AZStd::forward<Args>(args)...)
        {
            DecodeAccess::Attach(m_serializer, &m_context);
        }

        ~DecodeSession()
        {
            DecodeAccess::Attach(m_serializer, nullptr);
        }

        [[nodiscard]]
        Serializer& GetSerializer()
        {
            return m_serializer;
        }

    private:
        DecodeContext m_context;
        Serializer m_serializer;
    };

    class DecodeForwardScope final
    {
    public:
        AZ_DISABLE_COPY_MOVE(DecodeForwardScope);

        DecodeForwardScope(
            const ISerializer& source,
            ISerializer& target)
            : m_target{target}
            , m_previousContext{DecodeAccess::Get(target)}
        {
            DecodeAccess::Attach(m_target, DecodeAccess::Get(source));
        }

        ~DecodeForwardScope()
        {
            DecodeAccess::Attach(m_target, m_previousContext);
        }

    private:
        ISerializer& m_target;
        DecodeContext* m_previousContext = nullptr;
    };
} // namespace AzNetworking::Internal
