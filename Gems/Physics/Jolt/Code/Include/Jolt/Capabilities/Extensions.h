/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Jolt/Configuration.h>
#include <Jolt/Extension.h>

namespace Jolt
{
    class IBodyPairCollider;
    class IContactCallbacks;
    class ICustomConstraintProvider;
    class ICustomConvexShapeProvider;
    class ICustomPathProvider;
    class ICustomShapeProvider;
    class IGroupFilter;
    class ISimulationShapeFilter;
    class ISoftBodyContactCallbacks;
    class IStepListener;
    class IVehicleCallbacks;
    class IVehicleCollisionFilter;
    class IVirtualCharacterContactCallbacks;
    class Runtime;

    class JOLT_API Extensions
    {
    public:
        //! Returns the active capability, or nullptr if no global System is active.
        //! The pointer is non-owning and must not be acquired or used while System destruction can occur.
        [[nodiscard]]
        static Extensions* Get();

        //! Registers caller-owned callback or provider storage.
        //! The extension identity and version are captured at registration and remain immutable.
        //! The extension must remain alive until UnregisterExtension succeeds. A host lease keeps
        //! dynamically loaded code and its storage resident while registered or in use.
        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IBodyPairCollider* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IContactCallbacks* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ICustomConstraintProvider* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ICustomConvexShapeProvider* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ICustomPathProvider* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ICustomShapeProvider* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IGroupFilter* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ISimulationShapeFilter* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            ISoftBodyContactCallbacks* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IStepListener* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IVehicleCallbacks* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IVehicleCollisionFilter* extension,
            ExtensionHostLease hostLease);

        [[nodiscard]]
        ExtensionRegistrationResult RegisterExtension(
            IVirtualCharacterContactCallbacks* extension,
            ExtensionHostLease hostLease);

        //! Fails with InUse while any world resource or component retains the extension.
        [[nodiscard]]
        ExtensionRegistrationStatus UnregisterExtension(ExtensionHandle extensionHandle);

        [[nodiscard]]
        bool GetExtensionInformation(
            ExtensionHandle extensionHandle,
            ExtensionInformation& information) const;

        [[nodiscard]]
        bool FindExtensionInformation(
            ExtensionKind extensionKind,
            const AZ::TypeId& extensionId,
            ExtensionInformation& information) const;

    private:
        friend class Runtime;

        Extensions() = default;
        ~Extensions() = default;
    };
} // namespace Jolt
