/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SystemInternal.h>

#include <ProviderModuleApi.h>

#include <AzTest/AzTest.h>

#include <AzCore/Module/DynamicModuleHandle.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/smart_ptr/weak_ptr.h>

namespace Jolt::Tests
{
    namespace
    {
        class LoadedProviders final
        {
        public:
            LoadedProviders() = default;

            [[nodiscard]]
            bool Load()
            {
                AZStd::unique_ptr<AZ::DynamicModuleHandle> module =
                    AZ::DynamicModuleHandle::Create("Jolt.TestProviders");
                if (!module
                    || !module->Load(AZ::DynamicModuleHandle::LoadFlags::InitFuncRequired))
                {
                    return false;
                }

                const auto getConstraintProvider =
                    module->GetFunction<GetCustomConstraintProviderFunction>(
                        GetCustomConstraintProviderFunctionName);
                const auto getPathProvider =
                    module->GetFunction<GetCustomPathProviderFunction>(
                        GetCustomPathProviderFunctionName);
                const auto getShapeProvider =
                    module->GetFunction<GetCustomShapeProviderFunction>(
                        GetCustomShapeProviderFunctionName);
                m_getEvidence = module->GetFunction<GetProviderEvidenceFunction>(
                    GetProviderEvidenceFunctionName);
                m_resetEvidence = module->GetFunction<ResetProviderEvidenceFunction>(
                    ResetProviderEvidenceFunctionName);
                if (!getConstraintProvider
                    || !getPathProvider
                    || !getShapeProvider
                    || !m_getEvidence
                    || !m_resetEvidence)
                {
                    return false;
                }

                m_constraintProvider = getConstraintProvider();
                m_pathProvider = getPathProvider();
                m_shapeProvider = getShapeProvider();
                if (!m_constraintProvider
                    || !m_pathProvider
                    || !m_shapeProvider)
                {
                    return false;
                }

                m_module = AZStd::shared_ptr<AZ::DynamicModuleHandle>(AZStd::move(module));
                m_weakModule = m_module;
                m_resetEvidence();
                return true;
            }

            void ReleaseOwner()
            {
                m_constraintProvider = nullptr;
                m_pathProvider = nullptr;
                m_shapeProvider = nullptr;
                m_getEvidence = nullptr;
                m_resetEvidence = nullptr;
                m_module.reset();
            }

            [[nodiscard]]
            ExtensionHostLease GetHostLease() const
            {
                return ExtensionHostLease(m_module);
            }

            [[nodiscard]]
            bool IsLoaded() const
            {
                const AZStd::shared_ptr<AZ::DynamicModuleHandle> module = m_weakModule.lock();
                return module && module->IsLoaded();
            }

            [[nodiscard]]
            ProviderEvidence GetEvidence() const
            {
                ProviderEvidence evidence;
                if (m_getEvidence
                    && m_getEvidence(&evidence))
                {
                    return evidence;
                }

                return {};
            }

            AZ_DISABLE_COPY_MOVE(LoadedProviders);

            AZStd::shared_ptr<AZ::DynamicModuleHandle> m_module;
            AZStd::weak_ptr<AZ::DynamicModuleHandle> m_weakModule;
            ICustomConstraintProvider* m_constraintProvider = nullptr;
            ICustomPathProvider* m_pathProvider = nullptr;
            ICustomShapeProvider* m_shapeProvider = nullptr;
            GetProviderEvidenceFunction m_getEvidence = nullptr;
            ResetProviderEvidenceFunction m_resetEvidence = nullptr;
        };
    } // namespace

    TEST(CrossModuleProviderTests, ProvidersRetainModuleResourcesAndReloadWithoutStaleState)
    {
        SystemConfiguration systemConfiguration;
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        systemConfiguration.m_defaultWorld.m_workerCount = 1;
        Runtime system(
            systemConfiguration,
            nullptr,
            SystemRegistration::Isolated);
        ASSERT_TRUE(system);

        LoadedProviders providers;
        ASSERT_TRUE(providers.Load());
        const ExtensionRegistrationResult shapeRegistration =
            system.RegisterExtension(providers.m_shapeProvider, providers.GetHostLease());
        const ExtensionRegistrationResult pathRegistration =
            system.RegisterExtension(providers.m_pathProvider, providers.GetHostLease());
        const ExtensionRegistrationResult constraintRegistration =
            system.RegisterExtension(providers.m_constraintProvider, providers.GetHostLease());
        ASSERT_TRUE(shapeRegistration);
        ASSERT_TRUE(pathRegistration);
        ASSERT_TRUE(constraintRegistration);

        ShapeConfiguration customShapeConfiguration;
        customShapeConfiguration.m_geometry = CustomShapeConfiguration{
            .m_data = {4},
            .m_providerId = providers.m_shapeProvider->GetId(),
        };
        customShapeConfiguration.m_userData = 0x4a6f6c74;
        const CookedShapeHandle cookedShapeHandle = system.CookShape(customShapeConfiguration);
        ASSERT_TRUE(cookedShapeHandle);

        CookedShapeArchive archive;
        AZStd::vector<MaterialHandle> materialHandles;
        AZStd::vector<CookedShapeHandle> childShapeHandles;
        ASSERT_TRUE(system.ExportShape(
            cookedShapeHandle,
            archive,
            materialHandles,
            childShapeHandles));
        ASSERT_EQ(archive.m_dependencies.size(), 1);
        EXPECT_EQ(archive.m_dependencies.front().m_path, "Objects/Jolt/CrossModule.shape");

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle customShapeHandle = system.CreateShape(
            worldHandle,
            cookedShapeHandle);
        ASSERT_TRUE(customShapeHandle);

        ShapeConfiguration sphereConfiguration;
        sphereConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle sphereShapeHandle = system.CreateShape(
            worldHandle,
            sphereConfiguration);
        ASSERT_TRUE(sphereShapeHandle);

        TransformedShape retainedCustomShape;
        ASSERT_TRUE(system.RetainShape(
            worldHandle,
            customShapeHandle,
            {},
            1.0f,
            retainedCustomShape));
        TransformedShape retainedSphereShape;
        WorldTransform sphereTransform;
        sphereTransform.m_position.m_x = 4.0;
        ASSERT_TRUE(system.RetainShape(
            worldHandle,
            sphereShapeHandle,
            sphereTransform,
            1.0f,
            retainedSphereShape));

        AZStd::array<TransformedShapeCollisionHit, 1> collisionHits;
        const QueryResult collisionResult = system.CollideTransformedShapes(
            worldHandle,
            retainedCustomShape,
            retainedSphereShape,
            {},
            collisionHits,
            {});
        ASSERT_TRUE(collisionResult.IsComplete());
        ASSERT_EQ(collisionResult.m_hitCount, 1);
        EXPECT_FLOAT_EQ(collisionHits.front().m_penetrationDepth, 0.5f);

        AZStd::array<TransformedShapeCastHit, 1> castHits;
        const QueryResult castResult = system.CastTransformedShape(
            worldHandle,
            retainedCustomShape,
            retainedSphereShape,
            {
                .m_displacement = AZ::Vector3::CreateAxisX(4.0f),
            },
            castHits,
            {});
        ASSERT_TRUE(castResult.IsComplete());
        ASSERT_EQ(castResult.m_hitCount, 1);
        EXPECT_FLOAT_EQ(castHits.front().m_fraction, 0.25f);

        CustomPathConfiguration pathConfiguration;
        pathConfiguration.m_data = {8};
        pathConfiguration.m_providerId = providers.m_pathProvider->GetId();
        const PathHandle pathHandle = system.CreatePath(pathConfiguration);
        ASSERT_TRUE(pathHandle);
        PathSample pathSample;
        ASSERT_TRUE(system.SamplePath(pathHandle, 3.0f, pathSample));
        EXPECT_TRUE(pathSample.m_position.IsClose(AZ::Vector3::CreateAxisX(3.0f)));

        const BodyHandle staticBodyHandle = system.CreateBody(
            worldHandle,
            BodyConfiguration{
                .m_shapeHandle = customShapeHandle,
                .m_motionType = MotionType::Static,
            });
        ASSERT_TRUE(staticBodyHandle);
        BodyConfiguration dynamicBodyConfiguration;
        dynamicBodyConfiguration.m_shapeHandle = sphereShapeHandle;
        dynamicBodyConfiguration.m_transform.m_position.m_z = 2.0;
        const BodyHandle dynamicBodyHandle = system.CreateBody(
            worldHandle,
            dynamicBodyConfiguration);
        ASSERT_TRUE(dynamicBodyHandle);

        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = staticBodyHandle;
        constraintConfiguration.m_secondBodyHandle = dynamicBodyHandle;
        constraintConfiguration.m_geometry = CustomConstraintConfiguration{
            .m_data = {1},
            .m_providerId = providers.m_constraintProvider->GetId(),
        };
        const ConstraintHandle constraintHandle = system.CreateConstraint(
            worldHandle,
            constraintConfiguration);
        ASSERT_TRUE(constraintHandle);
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        const ProviderEvidence evidence = providers.GetEvidence();
        EXPECT_EQ(evidence.m_castCount, 1);
        EXPECT_EQ(evidence.m_collisionCount, 1);
        EXPECT_EQ(evidence.m_cookCount, 1);
        EXPECT_GT(evidence.m_pathSampleCount, 0);
        EXPECT_GT(evidence.m_positionConstraintCount, 0);
        EXPECT_EQ(evidence.m_velocityConstraintCount, 1);

        EXPECT_EQ(
            system.UnregisterExtension(shapeRegistration.m_handle),
            ExtensionRegistrationStatus::InUse);
        EXPECT_EQ(
            system.UnregisterExtension(pathRegistration.m_handle),
            ExtensionRegistrationStatus::InUse);
        EXPECT_EQ(
            system.UnregisterExtension(constraintRegistration.m_handle),
            ExtensionRegistrationStatus::InUse);

        const ShapeHandle staleShapeHandle = customShapeHandle;
        const PathHandle stalePathHandle = pathHandle;
        const ConstraintHandle staleConstraintHandle = constraintHandle;

        ASSERT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        ASSERT_TRUE(system.DestroyBody(worldHandle, dynamicBodyHandle));
        ASSERT_TRUE(system.DestroyBody(worldHandle, staticBodyHandle));
        ASSERT_TRUE(system.DestroyPath(pathHandle));
        retainedSphereShape = {};
        retainedCustomShape = {};
        ASSERT_TRUE(system.DestroyShape(worldHandle, sphereShapeHandle));
        ASSERT_TRUE(system.DestroyShape(worldHandle, customShapeHandle));
        ASSERT_TRUE(system.DestroyCookedShape(cookedShapeHandle));

        providers.ReleaseOwner();
        EXPECT_TRUE(providers.IsLoaded());
        ASSERT_EQ(
            system.UnregisterExtension(shapeRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
        ASSERT_EQ(
            system.UnregisterExtension(pathRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
        ASSERT_EQ(
            system.UnregisterExtension(constraintRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
        EXPECT_FALSE(providers.IsLoaded());
        EXPECT_FALSE(system.ImportShape(
            archive,
            materialHandles,
            childShapeHandles));

        EXPECT_FALSE(system.IsValid(worldHandle, staleShapeHandle));
        EXPECT_FALSE(system.IsValid(stalePathHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, staleConstraintHandle));

        ASSERT_TRUE(providers.Load());
        const ExtensionRegistrationResult reloadedShapeRegistration =
            system.RegisterExtension(providers.m_shapeProvider, providers.GetHostLease());
        const ExtensionRegistrationResult reloadedPathRegistration =
            system.RegisterExtension(providers.m_pathProvider, providers.GetHostLease());
        const ExtensionRegistrationResult reloadedConstraintRegistration =
            system.RegisterExtension(providers.m_constraintProvider, providers.GetHostLease());
        ASSERT_TRUE(reloadedShapeRegistration);
        ASSERT_TRUE(reloadedPathRegistration);
        ASSERT_TRUE(reloadedConstraintRegistration);

        const CookedShapeHandle reloadedShapeHandle = system.ImportShape(
            archive,
            materialHandles,
            childShapeHandles);
        ASSERT_TRUE(reloadedShapeHandle);
        CustomShapeInfo reloadedInfo;
        ASSERT_TRUE(system.GetCustomShapeInfo(reloadedShapeHandle, reloadedInfo));
        EXPECT_EQ(reloadedInfo.m_providerId, providers.m_shapeProvider->GetId());
        EXPECT_EQ(reloadedInfo.m_providerVersion, providers.m_shapeProvider->GetVersion());
        ASSERT_TRUE(system.DestroyCookedShape(reloadedShapeHandle));

        providers.ReleaseOwner();
        EXPECT_TRUE(providers.IsLoaded());
        ASSERT_EQ(
            system.UnregisterExtension(reloadedShapeRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
        ASSERT_EQ(
            system.UnregisterExtension(reloadedPathRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
        ASSERT_EQ(
            system.UnregisterExtension(reloadedConstraintRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
        EXPECT_FALSE(providers.IsLoaded());
    }
} // namespace Jolt::Tests
