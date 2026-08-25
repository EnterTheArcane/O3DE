/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Handle.h>

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/std/containers/vector.h>

namespace Jolt
{
    class ISceneRequests
        : public AZ::ComponentBus
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        [[nodiscard]]
        virtual AZStd::vector<BodyHandle> CopyBodies() const = 0;

        [[nodiscard]]
        virtual AZStd::vector<ConstraintHandle> CopyConstraints() const = 0;

        [[nodiscard]]
        virtual SceneDefinitionHandle GetDefinitionHandle() const = 0;

        [[nodiscard]]
        virtual SceneInstanceHandle GetInstanceHandle() const = 0;

        [[nodiscard]]
        virtual bool IsReady() const = 0;
    };

    using SceneRequestBus = AZ::EBus<ISceneRequests>;

    class ISceneNotifications
        : public AZ::ComponentBus
    {
    public:
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;

        virtual void OnSceneReady(SceneInstanceHandle instanceHandle) = 0;

        virtual void OnSceneReloading(SceneInstanceHandle instanceHandle) = 0;

        virtual void OnSceneReleased() = 0;
    };

    using SceneNotificationBus = AZ::EBus<ISceneNotifications>;
} // namespace Jolt
