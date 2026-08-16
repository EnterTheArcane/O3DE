/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <ProviderModuleApi.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/Module/DynamicModuleHandle.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/parallel/atomic.h>

namespace Jolt::Tests
{
    namespace
    {
        class ModuleCustomShapeProvider final
            : public ICustomShapeProvider
        {
        public:
            AZ_RTTI(
                ModuleCustomShapeProvider,
                "{AD5102F2-F13E-4D17-8FC9-E946918DA08C}",
                ICustomShapeProvider);

            [[nodiscard]]
            AZ::TypeId GetId() const override
            {
                return azrtti_typeid<ModuleCustomShapeProvider>();
            }

            [[nodiscard]]
            AZ::u64 GetVersion() const override
            {
                return 1;
            }

            [[nodiscard]]
            bool Cook(
                const AZStd::span<const AZ::u8> input,
                CustomShapeData& output) const override
            {
                if (input.size() != 1
                    || input.front() == 0)
                {
                    return false;
                }

                ++m_cookCount;
                const float halfExtent = 0.25f * input.front();
                output.m_vertices = {
                    {-halfExtent, -halfExtent, -halfExtent},
                    {halfExtent, -halfExtent, -halfExtent},
                    {-halfExtent, halfExtent, -halfExtent},
                    {halfExtent, halfExtent, -halfExtent},
                    {-halfExtent, -halfExtent, halfExtent},
                    {halfExtent, -halfExtent, halfExtent},
                    {-halfExtent, halfExtent, halfExtent},
                    {halfExtent, halfExtent, halfExtent},
                };
                output.m_dependencies = {{"Objects/Jolt/CrossModule.shape", 0x4a6f6c74}};
                output.m_runtimeData.assign(input.begin(), input.end());
                output.m_geometryKind = CustomShapeGeometryKind::Convex;
                return true;
            }

            [[nodiscard]]
            CustomShapeDispatchResult Collide(
                [[maybe_unused]] const ICustomShapeView& firstShape,
                [[maybe_unused]] const ICustomShapeView& secondShape,
                [[maybe_unused]] const CustomShapeCollisionSettings& settings,
                ICustomShapeCollisionCollector& collector) const override
            {
                ++m_collisionCount;
                [[maybe_unused]] const bool accepted = collector.AddHit({
                    .m_firstContactPosition = AZ::Vector3::CreateZero(),
                    .m_secondContactPosition = AZ::Vector3::CreateAxisX(),
                    .m_penetrationAxis = AZ::Vector3::CreateAxisX(),
                    .m_firstSubShapeId = SubShapeId::Root,
                    .m_secondSubShapeId = SubShapeId::Root,
                    .m_penetrationDepth = 0.5f,
                });
                return CustomShapeDispatchResult::Handled;
            }

            [[nodiscard]]
            CustomShapeDispatchResult Cast(
                [[maybe_unused]] const ICustomShapeView& firstShape,
                [[maybe_unused]] const ICustomShapeView& secondShape,
                [[maybe_unused]] const CustomShapeCastSettings& settings,
                ICustomShapeCastCollector& collector) const override
            {
                ++m_castCount;
                [[maybe_unused]] const bool accepted = collector.AddHit({
                    .m_collision = {
                        .m_firstContactPosition = AZ::Vector3::CreateZero(),
                        .m_secondContactPosition = AZ::Vector3::CreateAxisX(),
                        .m_penetrationAxis = AZ::Vector3::CreateAxisX(),
                        .m_firstSubShapeId = SubShapeId::Root,
                        .m_secondSubShapeId = SubShapeId::Root,
                    },
                    .m_fraction = 0.25f,
                });
                return CustomShapeDispatchResult::Handled;
            }

            mutable AZStd::atomic_uint32_t m_castCount = 0;
            mutable AZStd::atomic_uint32_t m_collisionCount = 0;
            mutable AZStd::atomic_uint32_t m_cookCount = 0;
        };

        class ModuleCustomPathProvider final
            : public ICustomPathProvider
        {
        public:
            AZ_RTTI(
                ModuleCustomPathProvider,
                "{48C5A1C7-1574-4E22-A574-8C2177E5BC01}",
                ICustomPathProvider);

            [[nodiscard]]
            AZ::TypeId GetId() const override
            {
                return azrtti_typeid<ModuleCustomPathProvider>();
            }

            [[nodiscard]]
            AZ::u64 GetVersion() const override
            {
                return 1;
            }

            [[nodiscard]]
            float GetMaximumFraction(
                const AZStd::span<const AZ::u8> data) const override
            {
                if (data.size() != 1
                    || data.front() == 0)
                {
                    return 0.0f;
                }

                return static_cast<float>(data.front());
            }

            [[nodiscard]]
            bool FindClosestFraction(
                const AZStd::span<const AZ::u8> data,
                const AZ::Vector3& position,
                [[maybe_unused]] const float fractionHint,
                float& fraction) const override
            {
                const float maximumFraction = GetMaximumFraction(data);
                if (maximumFraction <= 0.0f
                    || !position.IsFinite())
                {
                    return false;
                }

                fraction = AZStd::clamp(position.GetX(), 0.0f, maximumFraction);
                return true;
            }

            [[nodiscard]]
            bool Sample(
                const AZStd::span<const AZ::u8> data,
                const float fraction,
                CustomPathPoint& point) const override
            {
                const float maximumFraction = GetMaximumFraction(data);
                if (!AZ::IsFiniteFloat(fraction)
                    || fraction < 0.0f
                    || fraction > maximumFraction)
                {
                    return false;
                }

                ++m_sampleCount;
                point = {
                    .m_normal = AZ::Vector3::CreateAxisZ(),
                    .m_position = AZ::Vector3::CreateAxisX(fraction),
                    .m_tangent = AZ::Vector3::CreateAxisX(),
                };
                return true;
            }

            mutable AZStd::atomic_uint32_t m_sampleCount = 0;
        };

        class ModuleCustomConstraintProvider final
            : public ICustomConstraintProvider
        {
        public:
            AZ_RTTI(
                ModuleCustomConstraintProvider,
                "{1485DB8E-32E7-49CD-A8A0-8AE21CF1B374}",
                ICustomConstraintProvider);

            [[nodiscard]]
            AZ::TypeId GetId() const override
            {
                return azrtti_typeid<ModuleCustomConstraintProvider>();
            }

            [[nodiscard]]
            AZ::u64 GetVersion() const override
            {
                return 1;
            }

            [[nodiscard]]
            AZ::u32 GetMaximumRowCount(
                const AZStd::span<const AZ::u8> data) const override
            {
                if (data.size() == 1
                    && data.front() == 1)
                {
                    return 1;
                }

                return 0;
            }

            [[nodiscard]]
            AZ::u32 GetStateByteCount(
                [[maybe_unused]] const AZStd::span<const AZ::u8> data) const override
            {
                return 1;
            }

            [[nodiscard]]
            bool InitializeState(
                [[maybe_unused]] const AZStd::span<const AZ::u8> data,
                const AZStd::span<AZ::u8> state) const override
            {
                if (state.size() != 1)
                {
                    return false;
                }

                state.front() = 0;
                return true;
            }

            [[nodiscard]]
            AZ::u32 PreparePositionRows(
                const CustomConstraintContext& context,
                [[maybe_unused]] const AZStd::span<const AZ::u8> data,
                [[maybe_unused]] const AZStd::span<AZ::u8> state,
                const AZStd::span<CustomConstraintRow> rows) const override
            {
                if (rows.empty())
                {
                    return 0;
                }

                ++m_positionCount;
                rows.front().m_firstLinear = -AZ::Vector3::CreateAxisZ();
                rows.front().m_secondLinear = AZ::Vector3::CreateAxisZ();
                rows.front().m_error = static_cast<float>(
                    context.m_secondFrame.m_position.m_z
                    - context.m_firstFrame.m_position.m_z);
                return 1;
            }

            [[nodiscard]]
            AZ::u32 PrepareVelocityRows(
                [[maybe_unused]] const CustomConstraintContext& context,
                [[maybe_unused]] const AZStd::span<const AZ::u8> data,
                const AZStd::span<AZ::u8> state,
                const AZStd::span<CustomConstraintRow> rows) const override
            {
                if (rows.empty()
                    || state.size() != 1)
                {
                    return 0;
                }

                ++m_velocityCount;
                ++state.front();
                rows.front().m_firstLinear = -AZ::Vector3::CreateAxisZ();
                rows.front().m_secondLinear = AZ::Vector3::CreateAxisZ();
                return 1;
            }

            mutable AZStd::atomic_uint32_t m_positionCount = 0;
            mutable AZStd::atomic_uint32_t m_velocityCount = 0;
        };

        ModuleCustomConstraintProvider s_constraintProvider;
        ModuleCustomPathProvider s_pathProvider;
        ModuleCustomShapeProvider s_shapeProvider;
    } // namespace
} // namespace Jolt::Tests

extern "C" AZ_DLL_EXPORT Jolt::ICustomConstraintProvider* JoltGetCustomConstraintProvider()
{
    return &Jolt::Tests::s_constraintProvider;
}

extern "C" AZ_DLL_EXPORT Jolt::ICustomPathProvider* JoltGetCustomPathProvider()
{
    return &Jolt::Tests::s_pathProvider;
}

extern "C" AZ_DLL_EXPORT Jolt::ICustomShapeProvider* JoltGetCustomShapeProvider()
{
    return &Jolt::Tests::s_shapeProvider;
}

extern "C" AZ_DLL_EXPORT bool JoltGetProviderEvidence(
    Jolt::Tests::ProviderEvidence* evidence)
{
    if (!evidence)
    {
        return false;
    }

    *evidence = {
        .m_castCount = Jolt::Tests::s_shapeProvider.m_castCount.load(),
        .m_collisionCount = Jolt::Tests::s_shapeProvider.m_collisionCount.load(),
        .m_cookCount = Jolt::Tests::s_shapeProvider.m_cookCount.load(),
        .m_pathSampleCount = Jolt::Tests::s_pathProvider.m_sampleCount.load(),
        .m_positionConstraintCount = Jolt::Tests::s_constraintProvider.m_positionCount.load(),
        .m_velocityConstraintCount = Jolt::Tests::s_constraintProvider.m_velocityCount.load(),
    };
    return true;
}

extern "C" AZ_DLL_EXPORT void JoltResetProviderEvidence()
{
    Jolt::Tests::s_shapeProvider.m_castCount.store(0);
    Jolt::Tests::s_shapeProvider.m_collisionCount.store(0);
    Jolt::Tests::s_shapeProvider.m_cookCount.store(0);
    Jolt::Tests::s_pathProvider.m_sampleCount.store(0);
    Jolt::Tests::s_constraintProvider.m_positionCount.store(0);
    Jolt::Tests::s_constraintProvider.m_velocityCount.store(0);
}

extern "C" AZ_DLL_EXPORT void InitializeDynamicModule()
{
}

extern "C" AZ_DLL_EXPORT void UninitializeDynamicModule()
{
}
