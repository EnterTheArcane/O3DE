/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/CustomConstraint.h>
#include <Jolt/CustomShape.h>
#include <Jolt/Path.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt
{
    class Runtime;

    class JOLT_API Extensions
    {
    public:
        [[nodiscard]]
        static Extensions* Get();

        //! The caller retains ownership and must keep provider alive until unregistration succeeds.
        [[nodiscard]]
        bool RegisterCustomConstraintProvider(ICustomConstraintProvider* provider);

        bool UnregisterCustomConstraintProvider(ICustomConstraintProvider* provider);

        //! The caller retains ownership and must keep provider alive until unregistration succeeds.
        [[nodiscard]]
        bool RegisterCustomPathProvider(ICustomPathProvider* provider);

        bool UnregisterCustomPathProvider(ICustomPathProvider* provider);

        //! Keeps the provider alive across concurrent CookShape calls until unregistration completes.
        [[nodiscard]]
        bool RegisterCustomConvexShapeProvider(ICustomConvexShapeProvider* provider);

        bool UnregisterCustomConvexShapeProvider(ICustomConvexShapeProvider* provider);

        [[nodiscard]]
        ProviderRegistrationResult RegisterCustomShapeProvider(ICustomShapeProvider* provider);

        [[nodiscard]]
        ProviderRegistrationResult UnregisterCustomShapeProvider(ICustomShapeProvider* provider);

    private:
        friend class Runtime;

        Extensions() = default;
        ~Extensions() = default;

        static AZStd::atomic<Extensions*> s_instance;
    };
} // namespace Jolt
