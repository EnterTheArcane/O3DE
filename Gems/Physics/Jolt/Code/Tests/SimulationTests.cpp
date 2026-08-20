/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SystemInternal.h>
#include <Jolt/SceneAsset.h>

#include <AzTest/AzTest.h>

#include <AzCore/UnitTest/UnitTest.h>
#include <AzCore/Jobs/JobCompletion.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Jobs/JobFunction.h>
#include <AzCore/Jobs/JobManager.h>
#include <AzCore/Jobs/JobManagerDesc.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Utils/TypeHash.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/functional.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/thread.h>

#include <cfenv>
#include <chrono>
#include <cmath>
#include <cstring>

namespace Jolt
{
    namespace
    {
        inline constexpr AZ::TypeId TestGroupFilterStateTypeId{"{A1000001-0101-4101-8101-000000000001}"};
        inline constexpr AZ::TypeId RecordingSoftBodyContactStateTypeId{"{A1000002-0102-4102-8102-000000000002}"};
        inline constexpr AZ::TypeId RecordingContactStateTypeId{"{A1000003-0103-4103-8103-000000000003}"};
        inline constexpr AZ::TypeId RecordingVehicleStateTypeId{"{A1000004-0104-4104-8104-000000000004}"};
        inline constexpr AZ::TypeId RecordingVehicleFilterStateTypeId{"{A1000005-0105-4105-8105-000000000005}"};
        inline constexpr AZ::TypeId RecordingSimulationShapeFilterStateTypeId{"{A1000006-0106-4106-8106-000000000006}"};
        inline constexpr AZ::TypeId BlockingStepListenerStateTypeId{"{A1000007-0107-4107-8107-000000000007}"};
        inline constexpr AZ::TypeId RecordingVirtualCharacterContactStateTypeId{"{A1000009-0109-4109-8109-000000000009}"};
        inline constexpr AZ::TypeId RecordingStepListenerStateTypeId{"{A100000A-010A-410A-810A-00000000000A}"};
        inline constexpr AZ::TypeId StatefulStepListenerStateTypeId{"{A100000B-010B-410B-810B-00000000000B}"};
        inline constexpr AZ::TypeId MutatingStepListenerStateTypeId{"{A100000C-010C-410C-810C-00000000000C}"};
        inline constexpr AZ::TypeId RecordingBodyPairColliderStateTypeId{"{A100000D-010D-410D-810D-00000000000D}"};
        inline constexpr AZ::TypeId InvalidBodyPairColliderStateTypeId{"{A100000E-010E-410E-810E-00000000000E}"};

        template<class Interface>
        class ScopedExtensionRegistration final
        {
        public:
            ScopedExtensionRegistration(
                Runtime& runtime,
                Interface* extension)
                : m_runtime(runtime)
            {
                const ExtensionRegistrationResult result = m_runtime.RegisterExtension(extension, {});
                EXPECT_EQ(result.m_status, ExtensionRegistrationStatus::Success);
                m_handle = result.m_handle;
            }

            ~ScopedExtensionRegistration()
            {
                if (m_handle)
                {
                    EXPECT_EQ(
                        m_runtime.UnregisterExtension(m_handle),
                        ExtensionRegistrationStatus::Success);
                }
            }

            AZ_DISABLE_COPY_MOVE(ScopedExtensionRegistration);

            [[nodiscard]]
            ExtensionHandle GetHandle() const
            {
                return m_handle;
            }

            [[nodiscard]]
            ExtensionRegistrationStatus Unregister()
            {
                const ExtensionRegistrationStatus status = m_runtime.UnregisterExtension(m_handle);
                if (status == ExtensionRegistrationStatus::Success)
                {
                    m_handle = ExtensionHandle::Invalid;
                }
                return status;
            }

        private:
            Runtime& m_runtime;
            ExtensionHandle m_handle;
        };

        [[nodiscard]]
        bool IsFiniteWorldPosition(
            const WorldPosition& position)
        {
            return std::isfinite(position.m_x)
                && std::isfinite(position.m_y)
                && std::isfinite(position.m_z);
        }

        class NativeAllocationCounterScope final
        {
        public:
            NativeAllocationCounterScope()
            {
                AcquireNativeMemoryStatistics();
                [[maybe_unused]] const NativeMemoryStatistics initialStatistics = GetNativeMemoryStatistics(true);
            }

            ~NativeAllocationCounterScope()
            {
                ReleaseNativeMemoryStatistics();
            }

            AZ_DISABLE_COPY_MOVE(NativeAllocationCounterScope);

            [[nodiscard]]
            size_t GetAllocationCount() const
            {
                const NativeMemoryStatistics statistics = GetNativeMemoryStatistics(false);
                return statistics.m_allocationCount + statistics.m_reallocationCount;
            }
        };

        struct SphereOnFloor final
        {
            WorldHandle m_worldHandle;
            ShapeHandle m_floorShapeHandle;
            ShapeHandle m_sphereShapeHandle;
            BodyHandle m_floorBodyHandle;
            BodyHandle m_sphereBodyHandle;
        };

        class TestGroupFilter final
            : public IGroupFilter
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return TestGroupFilterStateTypeId;
            }

            [[nodiscard]]
            bool CanCollide(
                const CollisionGroup firstGroup,
                const CollisionGroup secondGroup) const override
            {
                return m_collisionEnabled
                    && firstGroup.m_groupId == secondGroup.m_groupId
                    && firstGroup.m_subGroupId != secondGroup.m_subGroupId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return m_stateHash;
            }

            [[nodiscard]]
            size_t GetStateByteCount() const override
            {
                return sizeof(m_stateHash) + sizeof(AZ::u8);
            }

            bool CaptureState(const AZStd::span<AZ::u8> state) const override
            {
                if (state.size() != GetStateByteCount())
                {
                    return false;
                }

                std::memcpy(state.data(), &m_stateHash, sizeof(m_stateHash));
                state[sizeof(m_stateHash)] = m_collisionEnabled;
                return true;
            }

            bool PrepareRestoreState(const AZStd::span<const AZ::u8> state) const override
            {
                return state.size() == GetStateByteCount()
                    && state[sizeof(m_stateHash)] <= 1;
            }

            void CommitRestoreState(const AZStd::span<const AZ::u8> state) const override
            {
                std::memcpy(&m_stateHash, state.data(), sizeof(m_stateHash));
                m_collisionEnabled = state[sizeof(m_stateHash)] != 0;
            }

            mutable AZ::u64 m_stateHash = 1;
            mutable bool m_collisionEnabled = false;
        };

        class TestCustomConvexShapeProvider final
            : public ICustomConvexShapeProvider
        {
        public:
            AZ_RTTI(
                TestCustomConvexShapeProvider,
                "{669A66FE-E06F-4099-AD4D-45BB86D808CA}",
                ICustomConvexShapeProvider);

            [[nodiscard]]
            AZ::TypeId GetId() const override
            {
                return azrtti_typeid<TestCustomConvexShapeProvider>();
            }

            [[nodiscard]]
            AZ::u64 GetVersion() const override
            {
                return 7;
            }

            [[nodiscard]]
            bool Cook(
                const AZStd::span<const AZ::u8> input,
                CustomConvexShapeData& output) const override
            {
                ++m_cookCount;
                if (input.size() != 1 || input.front() == 0)
                {
                    return false;
                }

                const float halfExtent = 0.25f * input.front();
                output.m_points = {
                    {-halfExtent, -halfExtent, -halfExtent},
                    {halfExtent, -halfExtent, -halfExtent},
                    {-halfExtent, halfExtent, -halfExtent},
                    {halfExtent, halfExtent, -halfExtent},
                    {-halfExtent, -halfExtent, halfExtent},
                    {halfExtent, -halfExtent, halfExtent},
                    {-halfExtent, halfExtent, halfExtent},
                    {halfExtent, halfExtent, halfExtent},
                };
                return true;
            }

            mutable AZStd::atomic<AZ::u32> m_cookCount = 0;
        };

        class TestCustomShapeProvider final
            : public ICustomShapeProvider
        {
        public:
            AZ_RTTI(
                TestCustomShapeProvider,
                "{848D48B9-DF40-4B35-B4C5-F35A4DA93D57}",
                ICustomShapeProvider);

            [[nodiscard]]
            AZ::TypeId GetId() const override
            {
                return azrtti_typeid<TestCustomShapeProvider>();
            }

            [[nodiscard]]
            AZ::u64 GetVersion() const override
            {
                return m_version;
            }

            [[nodiscard]]
            bool Cook(
                const AZStd::span<const AZ::u8> input,
                CustomShapeData& output) const override
            {
                ++m_cookCount;
                if (input.size() != 1)
                {
                    return false;
                }
                if (input.front() == 0)
                {
                    return true;
                }
                if (input.front() == 5)
                {
                    output.m_vertices = {
                        {-1.0f, -1.0f, 0.0f},
                        {1.0f, -1.0f, 0.0f},
                        {1.0f, 1.0f, 0.0f},
                        {-1.0f, 1.0f, 0.0f},
                    };
                    output.m_triangles = {
                        {
                            .m_firstVertex = 0,
                            .m_secondVertex = 1,
                            .m_thirdVertex = 2,
                            .m_materialIndex = 0,
                            .m_userData = 41,
                        },
                        {
                            .m_firstVertex = 0,
                            .m_secondVertex = 2,
                            .m_thirdVertex = 3,
                            .m_materialIndex = 1,
                            .m_userData = 42,
                        },
                    };
                    output.m_dependencies = {{"Objects/CustomMesh.source", 0x87654321}};
                    output.m_geometryKind = CustomShapeGeometryKind::Mesh;
                    output.m_perTriangleUserData = true;
                    output.m_runtimeData = {input.front(), 7};
                    return true;
                }

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
                output.m_dependencies = {{"Objects/CustomShape.source", 0x12345678}};
                output.m_geometryKind = CustomShapeGeometryKind::Convex;
                output.m_runtimeData = {input.front(), 7};
                return true;
            }

            CustomShapeDispatchResult Collide(
                const ICustomShapeView& firstShape,
                const ICustomShapeView& secondShape,
                const CustomShapeCollisionSettings& settings,
                ICustomShapeCollisionCollector& collector) const override
            {
                if (!m_useDispatch)
                {
                    return CustomShapeDispatchResult::Unsupported;
                }

                ++m_collisionCount;
                const ICustomShapeView* customShape = &firstShape;
                const ICustomShapeView* otherShape = &secondShape;
                if (customShape->GetProviderId().IsNull())
                {
                    customShape = &secondShape;
                    otherShape = &firstShape;
                }
                AZStd::array<AZ::Vector3, MaximumSupportingFaceVertexCount> supportingFace;
                const QueryResult supportingFaceResult = customShape->GetSupportingFace(
                    SubShapeId::Root,
                    AZ::Vector3::CreateAxisX(),
                    supportingFace);
                CustomShapeRaycastHit raycastHit;
                const bool raycastSucceeded = customShape->Raycast(
                    {
                        .m_start = {2.0f, 0.0f, 0.0f},
                        .m_displacement = {-4.0f, 0.0f, 0.0f},
                    },
                    raycastHit);
                AZStd::array<CustomShapeTriangleVertices, 16> triangles;
                const QueryResult triangleResult = customShape->CollectTriangles(
                    AZ::Aabb::CreateCenterRadius(AZ::Vector3::CreateZero(), 8.0f),
                    triangles);
                m_viewEvidence = customShape->GetProviderId() == GetId()
                    && customShape->GetProviderVersion() == m_expectedViewVersion
                    && customShape->GetRuntimeData().size() == 2
                    && customShape->GetProperties().m_kind == ShapeKind::Custom
                    && customShape->GetStats().m_memorySize > 0
                    && customShape->GetMaterial(SubShapeId::Root)
                    && customShape->GetUserData(SubShapeId::Root) == 0xABCDEF
                    && supportingFaceResult.m_requiredHitCount > 0
                    && raycastSucceeded
                    && triangleResult.m_requiredHitCount > 0
                    && (otherShape->GetProviderId().IsNull()
                        || otherShape->GetProviderId() == GetId())
                    && settings.m_faceCollectionMode == FaceCollectionMode::Collect;

                AZStd::array firstFace = {
                    AZ::Vector3(0.0f, -0.5f, -0.5f),
                    AZ::Vector3(0.0f, 0.5f, -0.5f),
                    AZ::Vector3(0.0f, 0.5f, 0.5f),
                };
                CustomShapeCollisionHit hit = {
                    .m_firstFace = firstFace,
                    .m_firstContactPosition = AZ::Vector3::CreateZero(),
                    .m_secondContactPosition = AZ::Vector3::CreateAxisX(),
                    .m_penetrationAxis = AZ::Vector3::CreateAxisX(),
                    .m_firstSubShapeId = SubShapeId::Root,
                    .m_secondSubShapeId = SubShapeId::Root,
                    .m_penetrationDepth = 0.75f,
                };
                if (m_returnOverwideSubShapeId)
                {
                    hit.m_firstSubShapeId = SubShapeId(0);
                }
                for (AZ::u32 hitIndex = 0; hitIndex < m_dispatchHitCount; ++hitIndex)
                {
                    if (!collector.AddHit(hit))
                    {
                        break;
                    }
                }
                if (m_returnInvalidOutput)
                {
                    hit.m_penetrationAxis.SetX(AZStd::numeric_limits<float>::quiet_NaN());
                    [[maybe_unused]] const bool accepted = collector.AddHit(hit);
                }
                return CustomShapeDispatchResult::Handled;
            }

            CustomShapeDispatchResult Cast(
                const ICustomShapeView&,
                const ICustomShapeView&,
                const CustomShapeCastSettings& settings,
                ICustomShapeCastCollector& collector) const override
            {
                if (!m_useDispatch)
                {
                    return CustomShapeDispatchResult::Unsupported;
                }

                ++m_castCount;
                m_castEvidence = settings.m_displacement == AZ::Vector3::CreateAxisX(4.0f);
                CustomShapeCastHit hit = {
                    .m_collision = {
                        .m_firstContactPosition = AZ::Vector3::CreateZero(),
                        .m_secondContactPosition = AZ::Vector3::CreateAxisX(),
                        .m_penetrationAxis = AZ::Vector3::CreateAxisX(),
                        .m_firstSubShapeId = SubShapeId::Root,
                        .m_secondSubShapeId = SubShapeId::Root,
                        .m_penetrationDepth = 0.0f,
                    },
                    .m_fraction = 0.25f,
                };
                if (m_returnOverwideSubShapeId)
                {
                    hit.m_collision.m_firstSubShapeId = SubShapeId(0);
                }
                for (AZ::u32 hitIndex = 0; hitIndex < m_dispatchHitCount; ++hitIndex)
                {
                    if (!collector.AddHit(hit))
                    {
                        break;
                    }
                }
                if (m_returnInvalidOutput)
                {
                    hit.m_fraction = AZStd::numeric_limits<float>::quiet_NaN();
                    [[maybe_unused]] const bool accepted = collector.AddHit(hit);
                }
                return CustomShapeDispatchResult::Handled;
            }

            mutable AZStd::atomic<AZ::u32> m_cookCount = 0;
            mutable AZStd::atomic<AZ::u32> m_collisionCount = 0;
            mutable AZStd::atomic<AZ::u32> m_castCount = 0;
            mutable AZStd::atomic<bool> m_castEvidence = false;
            mutable AZStd::atomic<bool> m_viewEvidence = false;
            AZ::u64 m_expectedViewVersion = 19;
            AZ::u64 m_version = 19;
            AZ::u32 m_dispatchHitCount = 1;
            bool m_returnInvalidOutput = false;
            bool m_returnOverwideSubShapeId = false;
            bool m_useDispatch = false;
        };

        class RecordingTransformedShapeCollisionCollector final
            : public ITransformedShapeCollisionCollector
        {
        public:
            AZ_RTTI(
                RecordingTransformedShapeCollisionCollector,
                "{92E44B6B-2F2E-4096-BE6A-A2F5DF4C6732}",
                ITransformedShapeCollisionCollector);

            float GetEarlyOutFraction() const override
            {
                return m_earlyOutFraction;
            }

            bool AddHit(
                const TransformedShapeCollisionHit& hit,
                const AZStd::span<const WorldPosition> firstFace,
                const AZStd::span<const WorldPosition> secondFace) override
            {
                m_hit = hit;
                m_firstFaceVertexCount = aznumeric_cast<AZ::u32>(firstFace.size());
                m_secondFaceVertexCount = aznumeric_cast<AZ::u32>(secondFace.size());
                ++m_hitCount;
                m_earlyOutFraction = -hit.m_penetrationDepth;
                if (m_increaseEarlyOutFraction)
                {
                    m_earlyOutFraction = AZStd::numeric_limits<float>::max();
                }
                return m_continueTraversal;
            }

            TransformedShapeCollisionHit m_hit;
            float m_earlyOutFraction = AZStd::numeric_limits<float>::max();
            AZ::u32 m_firstFaceVertexCount = 0;
            AZ::u32 m_secondFaceVertexCount = 0;
            AZ::u32 m_hitCount = 0;
            bool m_continueTraversal = false;
            bool m_increaseEarlyOutFraction = false;
        };

        class RecordingTransformedShapeCastCollector final
            : public ITransformedShapeCastCollector
        {
        public:
            AZ_RTTI(
                RecordingTransformedShapeCastCollector,
                "{74CAC71F-54B6-47F5-A20B-C4ED96A0177D}",
                ITransformedShapeCastCollector);

            float GetEarlyOutFraction() const override
            {
                return m_earlyOutFraction;
            }

            bool AddHit(
                const TransformedShapeCastHit& hit,
                const AZStd::span<const WorldPosition> firstFace,
                const AZStd::span<const WorldPosition> secondFace) override
            {
                m_hit = hit;
                m_firstFaceVertexCount = aznumeric_cast<AZ::u32>(firstFace.size());
                m_secondFaceVertexCount = aznumeric_cast<AZ::u32>(secondFace.size());
                ++m_hitCount;
                m_earlyOutFraction = hit.m_fraction;
                if (m_increaseEarlyOutFraction)
                {
                    m_earlyOutFraction = 2.0f;
                }
                return m_continueTraversal;
            }

            TransformedShapeCastHit m_hit;
            float m_earlyOutFraction = 1.0f;
            AZ::u32 m_firstFaceVertexCount = 0;
            AZ::u32 m_secondFaceVertexCount = 0;
            AZ::u32 m_hitCount = 0;
            bool m_continueTraversal = false;
            bool m_increaseEarlyOutFraction = false;
        };

        class TestCustomPathProvider final
            : public ICustomPathProvider
        {
        public:
            AZ_RTTI(
                TestCustomPathProvider,
                "{0B431543-2B6E-42B5-A893-C5B6BC842302}",
                ICustomPathProvider);

            [[nodiscard]]
            AZ::TypeId GetId() const override
            {
                return azrtti_typeid<TestCustomPathProvider>();
            }

            [[nodiscard]]
            AZ::u64 GetVersion() const override
            {
                return 13;
            }

            [[nodiscard]]
            float GetMaximumFraction(
                const AZStd::span<const AZ::u8> data) const override
            {
                if (data.size() != 1 || data.front() == 0)
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
                if (maximumFraction <= 0.0f || !position.IsFinite())
                {
                    return false;
                }

                ++m_closestCount;
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
                    .m_normal = AZ::Vector3::CreateAxisZ(3.0f),
                    .m_position = AZ::Vector3::CreateAxisX(fraction),
                    .m_tangent = AZ::Vector3::CreateAxisX(2.0f),
                };
                return true;
            }

            mutable AZStd::atomic<AZ::u32> m_closestCount = 0;
            mutable AZStd::atomic<AZ::u32> m_sampleCount = 0;
        };

        class TestCustomConstraintProvider final
            : public ICustomConstraintProvider
        {
        public:
            AZ_RTTI(
                TestCustomConstraintProvider,
                "{C5195B30-40B7-413A-80B3-FD85514723F8}",
                ICustomConstraintProvider);

            [[nodiscard]]
            AZ::TypeId GetId() const override
            {
                return azrtti_typeid<TestCustomConstraintProvider>();
            }

            [[nodiscard]]
            AZ::u64 GetVersion() const override
            {
                return 11;
            }

            [[nodiscard]]
            AZ::u32 GetMaximumRowCount(
                const AZStd::span<const AZ::u8> data) const override
            {
                if (data.size() == 1 && data.front() == 1)
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
                ++m_positionCallCount;
                if (rows.empty())
                {
                    return 0;
                }

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
                [[maybe_unused]] const AZStd::span<AZ::u8> state,
                const AZStd::span<CustomConstraintRow> rows) const override
            {
                ++m_velocityCallCount;
                if (rows.empty() || state.size() != 1)
                {
                    return 0;
                }

                ++state.front();
                rows.front().m_firstLinear = -AZ::Vector3::CreateAxisZ();
                rows.front().m_secondLinear = AZ::Vector3::CreateAxisZ();
                return 1;
            }

            mutable AZStd::atomic_uint32_t m_positionCallCount = 0;
            mutable AZStd::atomic_uint32_t m_velocityCallCount = 0;
        };

        class NameDictionaryScope final
        {
        public:
            NameDictionaryScope()
            {
                if (!AZ::Interface<AZ::NameDictionary>::Get())
                {
                    AZ::NameDictionary::Create();
                    m_created = true;
                }
            }

            ~NameDictionaryScope()
            {
                if (m_created)
                {
                    AZ::NameDictionary::Destroy();
                }
            }

            AZ_DISABLE_COPY_MOVE(NameDictionaryScope);

        private:
            bool m_created = false;
        };

        class RecordingSoftBodyContactCallbacks final
            : public ISoftBodyContactCallbacks
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return RecordingSoftBodyContactStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                AZ::u64 stateHash = static_cast<AZ::u64>(m_decision);
                stateHash ^= AZStd::hash<float>{}(m_responseSettings.m_softBodyInverseMassScale);
                stateHash ^= AZStd::hash<float>{}(m_responseSettings.m_otherBodyInverseMassScale) << 1;
                stateHash ^= AZStd::hash<float>{}(m_responseSettings.m_otherBodyInverseInertiaScale) << 2;
                stateHash ^= static_cast<AZ::u64>(m_responseSettings.m_isSensor) << 63;
                return stateHash;
            }

            SoftBodyContactDecision OnContactValidate(
                const BodyHandle softBodyHandle,
                const BodyHandle otherBodyHandle,
                SoftBodyContactSettings& settings) override
            {
                ++m_validationCount;
                m_softBodyHandle = softBodyHandle;
                m_otherBodyHandle = otherBodyHandle;
                m_lastSettings = settings;
                settings = m_responseSettings;
                return m_decision;
            }

            void OnContactAdded(
                const BodyHandle softBodyHandle,
                const ISoftBodyContactManifold& manifold) override
            {
                ++m_addedCount;
                m_softBodyHandle = softBodyHandle;
                m_vertexCount = manifold.GetVertexCount();
                m_sensorCount = manifold.GetSensorCount();
                if (m_sensorCount >= m_maximumSensorCount)
                {
                    m_maximumSensorCount = m_sensorCount;
                    m_sensorBodyHandles.clear();
                    m_sensorBodyHandles.reserve(m_sensorCount);
                    for (AZ::u32 sensorIndex = 0; sensorIndex < m_sensorCount; ++sensorIndex)
                    {
                        m_sensorBodyHandles.push_back(manifold.GetSensorBody(sensorIndex));
                    }
                }
                for (AZ::u32 vertexIndex = 0; vertexIndex < m_vertexCount; ++vertexIndex)
                {
                    SoftBodyContactPoint contact;
                    if (manifold.GetContact(vertexIndex, contact))
                    {
                        ++m_contactCount;
                        m_lastContact = contact;
                    }
                }
            }

            SoftBodyContactPoint m_lastContact;
            SoftBodyContactSettings m_lastSettings;
            SoftBodyContactSettings m_responseSettings = {
                .m_softBodyInverseMassScale = 0.75f,
                .m_otherBodyInverseMassScale = 0.5f,
                .m_otherBodyInverseInertiaScale = 0.25f,
            };
            AZStd::vector<BodyHandle> m_sensorBodyHandles;
            BodyHandle m_otherBodyHandle;
            BodyHandle m_softBodyHandle;
            AZ::u32 m_addedCount = 0;
            AZ::u32 m_contactCount = 0;
            AZ::u32 m_maximumSensorCount = 0;
            AZ::u32 m_sensorCount = 0;
            AZ::u32 m_validationCount = 0;
            AZ::u32 m_vertexCount = 0;
            SoftBodyContactDecision m_decision = SoftBodyContactDecision::Accept;
        };

        class RecordingContactCallbacks final
            : public IContactCallbacks
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return RecordingContactStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return static_cast<AZ::u64>(m_decision);
            }

            ContactDecision OnContactValidate(
                const ContactValidation& contact) override
            {
                ++m_validationCount;
                m_lastValidation = contact;
                return m_decision;
            }

            void OnContactAdded(
                const IContactManifold& manifold,
                ContactSettings& settings) override
            {
                ++m_addedCount;
                RecordManifold(manifold);
                ContactResponseConfiguration configuration;
                configuration.m_combinedFriction = settings.m_combinedFriction;
                configuration.m_combinedRestitution = settings.m_combinedRestitution;
                m_estimatedResponse = manifold.EstimateResponse(
                    configuration,
                    m_responseEstimate,
                    m_contactImpulses);
                ModifySettings(settings);
            }

            void OnContactPersisted(
                const IContactManifold& manifold,
                ContactSettings& settings) override
            {
                ++m_persistedCount;
                RecordManifold(manifold);
                ModifySettings(settings);
            }

            void OnContactRemoved(
                const ContactEvent& contact) override
            {
                ++m_removedCount;
                m_lastRemoved = contact;
            }

            void ModifySettings(
                ContactSettings& settings)
            {
                m_initialSettings = settings;
                settings.m_combinedFriction = 0.75f;
                settings.m_combinedRestitution = 0.1f;
                settings.m_firstBodyInverseInertiaScale = 0.8f;
                settings.m_firstBodyInverseMassScale = 0.7f;
                settings.m_secondBodyInverseInertiaScale = 0.6f;
                settings.m_secondBodyInverseMassScale = 0.5f;
                settings.m_relativeLinearSurfaceVelocity = AZ::Vector3::CreateAxisX(0.25f);
            }

            void RecordManifold(
                const IContactManifold& manifold)
            {
                m_firstBodyHandle = manifold.GetFirstBody();
                m_secondBodyHandle = manifold.GetSecondBody();
                m_firstShapeHandle = manifold.GetFirstShape();
                m_secondShapeHandle = manifold.GetSecondShape();
                if (manifold.GetPointCount() > 0)
                {
                    [[maybe_unused]] const bool pointFound = manifold.GetPoint(0, m_lastPoint);
                }
            }

            ContactEvent m_lastRemoved;
            ContactPoint m_lastPoint;
            ContactResponseEstimate m_responseEstimate;
            ContactSettings m_initialSettings;
            ContactValidation m_lastValidation;
            AZStd::array<float, 1> m_contactImpulses;
            BodyHandle m_firstBodyHandle;
            BodyHandle m_secondBodyHandle;
            ShapeHandle m_firstShapeHandle;
            ShapeHandle m_secondShapeHandle;
            AZ::u32 m_addedCount = 0;
            AZ::u32 m_persistedCount = 0;
            AZ::u32 m_removedCount = 0;
            AZ::u32 m_validationCount = 0;
            bool m_estimatedResponse = false;
            ContactDecision m_decision = ContactDecision::AcceptAllForPair;
        };

        class RecordingVehicleCallbacks final
            : public IVehicleCallbacks
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return RecordingVehicleStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return 0;
            }

            void OnPreStep(
                const VehicleHandle vehicleHandle,
                const StepInformation& information,
                IVehicleStepContext& context) override
            {
                ++m_preStepCount;
                m_lastInformation = information;
                m_lastVehicleHandle = vehicleHandle;
                AZStd::array<WheelState, 4> wheels;
                WheeledVehicleState state;
                m_contextStateQuerySucceeded =
                    context.GetWheeledVehicleState(vehicleHandle, state, wheels).IsComplete();
                m_contextInputUpdateSucceeded = context.SetWheeledVehicleInput(
                    vehicleHandle,
                    {.m_forward = 1.0f});
            }

            void OnPostCollide(
                const VehicleHandle vehicleHandle,
                const StepInformation& information,
                [[maybe_unused]] IVehicleStepContext& context) override
            {
                ++m_postCollideCount;
                m_lastInformation = information;
                m_lastVehicleHandle = vehicleHandle;
            }

            void OnPostStep(
                const VehicleHandle vehicleHandle,
                const StepInformation& information,
                [[maybe_unused]] IVehicleStepContext& context) override
            {
                ++m_postStepCount;
                m_lastInformation = information;
                m_lastVehicleHandle = vehicleHandle;
            }

            void OnCombineFriction(VehicleFrictionCalculation& calculation) override
            {
                ++m_frictionCount;
                m_lastContactBodyHandle = calculation.m_contactBodyHandle;
                m_lastVehicleHandle = calculation.m_vehicleHandle;
                calculation.m_lateralFriction *= 0.75f;
                calculation.m_longitudinalFriction *= 0.75f;
            }

            void OnCalculateTireImpulses(VehicleTireImpulseCalculation& calculation) override
            {
                ++m_tireImpulseCount;
                m_lastVehicleHandle = calculation.m_vehicleHandle;
                calculation.m_lateralImpulse *= 0.9f;
                calculation.m_longitudinalImpulse *= 0.9f;
            }

            StepInformation m_lastInformation;
            BodyHandle m_lastContactBodyHandle;
            VehicleHandle m_lastVehicleHandle;
            AZ::u32 m_frictionCount = 0;
            AZ::u32 m_postCollideCount = 0;
            AZ::u32 m_postStepCount = 0;
            AZ::u32 m_preStepCount = 0;
            AZ::u32 m_tireImpulseCount = 0;
            bool m_contextInputUpdateSucceeded = false;
            bool m_contextStateQuerySucceeded = false;
        };

        class RecordingVehicleCollisionFilter final
            : public IVehicleCollisionFilter
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return RecordingVehicleFilterStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return AZStd::hash<BodyHandle>{}(m_rejectedBodyHandle);
            }

            [[nodiscard]]
            size_t GetStateByteCount() const override
            {
                return sizeof(AZ::u64);
            }

            bool CaptureState(const AZStd::span<AZ::u8> state) const override
            {
                if (state.size() != sizeof(AZ::u64))
                {
                    return false;
                }

                const AZ::u64 value = Internal::HandleAccess::ToValue(m_rejectedBodyHandle);
                std::memcpy(state.data(), &value, sizeof(value));
                return true;
            }

            bool PrepareRestoreState(const AZStd::span<const AZ::u8> state) const override
            {
                return state.size() == sizeof(AZ::u64);
            }

            void CommitRestoreState(const AZStd::span<const AZ::u8> state) const override
            {
                AZ::u64 value = 0;
                std::memcpy(&value, state.data(), sizeof(value));
                m_rejectedBodyHandle = Internal::HandleAccess::FromValue<BodyHandle>(value);
            }

            [[nodiscard]]
            bool ShouldCollide([[maybe_unused]] const BodyHandle bodyHandle) const override
            {
                ++m_bodyCount;
                return bodyHandle != m_rejectedBodyHandle;
            }

            [[nodiscard]]
            bool ShouldCollide(const BroadPhaseLayer broadPhaseLayer) const override
            {
                ++m_broadPhaseCount;
                m_receivedInvalidBroadPhaseLayer = m_receivedInvalidBroadPhaseLayer || !broadPhaseLayer;
                m_receivedMovingBroadPhaseLayer =
                    m_receivedMovingBroadPhaseLayer
                    || broadPhaseLayer == DefaultBroadPhaseLayers::Moving;
                return true;
            }

            [[nodiscard]]
            bool ShouldCollide(const ObjectLayer objectLayer) const override
            {
                ++m_objectLayerCount;
                m_receivedInvalidObjectLayer = m_receivedInvalidObjectLayer || !objectLayer;
                m_receivedMovingObjectLayer =
                    m_receivedMovingObjectLayer
                    || objectLayer == DefaultLayers::Moving;
                return true;
            }

            mutable BodyHandle m_rejectedBodyHandle;
            mutable AZ::u32 m_bodyCount = 0;
            mutable AZ::u32 m_broadPhaseCount = 0;
            mutable AZ::u32 m_objectLayerCount = 0;
            mutable bool m_receivedInvalidBroadPhaseLayer = false;
            mutable bool m_receivedInvalidObjectLayer = false;
            mutable bool m_receivedMovingBroadPhaseLayer = false;
            mutable bool m_receivedMovingObjectLayer = false;
        };

        class RecordingSimulationShapeFilter final
            : public ISimulationShapeFilter
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return RecordingSimulationShapeFilterStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return AZStd::hash<BodyHandle>{}(m_rejectedBodyHandle);
            }

            [[nodiscard]]
            size_t GetStateByteCount() const override
            {
                return sizeof(AZ::u64);
            }

            bool CaptureState(const AZStd::span<AZ::u8> state) const override
            {
                if (state.size() != sizeof(AZ::u64))
                {
                    return false;
                }

                const AZ::u64 value = Internal::HandleAccess::ToValue(m_rejectedBodyHandle);
                std::memcpy(state.data(), &value, sizeof(value));
                return true;
            }

            bool PrepareRestoreState(const AZStd::span<const AZ::u8> state) const override
            {
                return state.size() == sizeof(AZ::u64);
            }

            void CommitRestoreState(const AZStd::span<const AZ::u8> state) const override
            {
                AZ::u64 value = 0;
                std::memcpy(&value, state.data(), sizeof(value));
                m_rejectedBodyHandle = Internal::HandleAccess::FromValue<BodyHandle>(value);
            }

            [[nodiscard]]
            bool ShouldCollide(
                const SimulationShape& firstShape,
                const SimulationShape& secondShape) const override
            {
                ++m_callCount;
                m_lastFirstShape = firstShape;
                m_lastSecondShape = secondShape;
                return firstShape.m_bodyHandle != m_rejectedBodyHandle
                    && secondShape.m_bodyHandle != m_rejectedBodyHandle;
            }

            mutable BodyHandle m_rejectedBodyHandle;
            mutable SimulationShape m_lastFirstShape;
            mutable SimulationShape m_lastSecondShape;
            mutable AZ::u32 m_callCount = 0;
        };

        class RecordingDebugRenderer final
            : public IDebugRenderer
        {
        public:
            void DrawGeometry(
                const AZStd::span<const DebugVertex> vertices,
                const AZStd::span<const AZ::u32> indices,
                const DebugGeometryConfiguration& configuration) override
            {
                ++m_geometryCount;
                m_vertexCount += aznumeric_cast<AZ::u32>(vertices.size());
                m_indexCount += aznumeric_cast<AZ::u32>(indices.size());
                for (const DebugVertex& vertex : vertices)
                {
                    m_vertexColorSum += vertex.m_colorRgba8;
                }
                m_minimumTranslationX = AZStd::min(
                    m_minimumTranslationX,
                    configuration.m_translation.m_x);
                m_maximumTranslationX = AZStd::max(
                    m_maximumTranslationX,
                    configuration.m_translation.m_x);
            }

            void DrawLine(
                const WorldPosition& start,
                const WorldPosition& end,
                [[maybe_unused]] const AZ::Color& color) override
            {
                ++m_lineCount;
                m_minimumLinePositionX = AZStd::min(
                    m_minimumLinePositionX,
                    AZStd::min(start.m_x, end.m_x));
                m_maximumLinePositionX = AZStd::max(
                    m_maximumLinePositionX,
                    AZStd::max(start.m_x, end.m_x));
            }

            void DrawText(
                [[maybe_unused]] const WorldPosition& position,
                [[maybe_unused]] const AZStd::string_view text,
                [[maybe_unused]] const AZ::Color& color,
                [[maybe_unused]] const float height) override
            {
                ++m_textCount;
            }

            void DrawTriangle(
                [[maybe_unused]] const WorldPosition& first,
                [[maybe_unused]] const WorldPosition& second,
                [[maybe_unused]] const WorldPosition& third,
                [[maybe_unused]] const AZ::Color& color,
                [[maybe_unused]] const bool castsShadow) override
            {
                ++m_triangleCount;
            }

            double m_minimumTranslationX = AZStd::numeric_limits<double>::max();
            double m_maximumTranslationX = AZStd::numeric_limits<double>::lowest();
            double m_minimumLinePositionX = AZStd::numeric_limits<double>::max();
            double m_maximumLinePositionX = AZStd::numeric_limits<double>::lowest();
            AZ::u64 m_vertexColorSum = 0;
            AZ::u32 m_geometryCount = 0;
            AZ::u32 m_indexCount = 0;
            AZ::u32 m_lineCount = 0;
            AZ::u32 m_textCount = 0;
            AZ::u32 m_triangleCount = 0;
            AZ::u32 m_vertexCount = 0;
        };

        class SelectedDebugFilter final
            : public IDebugFilter
        {
        public:
            [[nodiscard]]
            bool ShouldDraw(
                const BodyHandle bodyHandle) const override
            {
                ++m_callCount;
                return bodyHandle == m_selectedBodyHandle;
            }

            [[nodiscard]]
            bool ShouldDraw(
                const HairHandle hairHandle) const override
            {
                ++m_hairCallCount;
                return hairHandle == m_selectedHairHandle;
            }

            BodyHandle m_selectedBodyHandle;
            HairHandle m_selectedHairHandle;
            mutable AZ::u32 m_callCount = 0;
            mutable AZ::u32 m_hairCallCount = 0;
        };

        class RecordingQueryFilter final
            : public IQueryFilter
        {
        public:
            [[nodiscard]]
            bool ShouldIncludeBody(
                const BodyHandle bodyHandle) const override
            {
                ++m_bodyCallCount;
                return bodyHandle != m_rejectedBodyHandle;
            }

            [[nodiscard]]
            bool ShouldIncludeShape(
                const SimulationShape& shape) const override
            {
                ++m_shapeCallCount;
                m_lastShape = shape;
                return shape.m_kind != m_rejectedShapeKind;
            }

            [[nodiscard]]
            bool ShouldIncludeShapePair(
                const SimulationShape& queryShape,
                const SimulationShape& targetShape) const override
            {
                ++m_shapePairCallCount;
                m_lastQueryShape = queryShape;
                m_lastTargetShape = targetShape;
                if (queryShape.m_kind == m_rejectedQueryShapeKind)
                {
                    return false;
                }

                return ShouldIncludeShape(targetShape);
            }

            BodyHandle m_rejectedBodyHandle;
            ShapeKind m_rejectedQueryShapeKind = ShapeKind::None;
            ShapeKind m_rejectedShapeKind = ShapeKind::None;

            mutable SimulationShape m_lastQueryShape;
            mutable SimulationShape m_lastShape;
            mutable SimulationShape m_lastTargetShape;

            mutable AZ::u32 m_bodyCallCount = 0;
            mutable AZ::u32 m_shapeCallCount = 0;
            mutable AZ::u32 m_shapePairCallCount = 0;
        };

        template<class Predicate>
        [[nodiscard]]
        bool WaitUntil(
            Predicate&& predicate,
            const std::chrono::milliseconds timeout = std::chrono::seconds(5))
        {
            const std::chrono::steady_clock::time_point deadline =
                std::chrono::steady_clock::now() + timeout;
            while (!predicate())
            {
                if (std::chrono::steady_clock::now() >= deadline)
                {
                    return false;
                }
                AZStd::this_thread::yield();
            }
            return true;
        }

        class BlockingQueryFilter final
            : public IQueryFilter
        {
        public:
            [[nodiscard]]
            bool ShouldIncludeBody(
                [[maybe_unused]] const BodyHandle bodyHandle) const override
            {
                m_enterCount.fetch_add(1, AZStd::memory_order_release);
                if (std::fegetround() != FE_TONEAREST)
                {
                    m_sawCanonicalFloatEnvironment.store(false, AZStd::memory_order_release);
                }
                while (!m_release.load(AZStd::memory_order_acquire))
                {
                    AZStd::this_thread::yield();
                }
                return true;
            }

            mutable AZStd::atomic_bool m_release{false};
            mutable AZStd::atomic_bool m_sawCanonicalFloatEnvironment{true};
            mutable AZStd::atomic_uint32_t m_enterCount{0};
        };

        class BlockingStepListener final
            : public IStepListener
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return BlockingStepListenerStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return 0;
            }

            void OnStep(
                [[maybe_unused]] const StepInformation& information,
                [[maybe_unused]] IStepContext& context) override
            {
                m_entered.store(true, AZStd::memory_order_release);
                while (!m_release.load(AZStd::memory_order_acquire))
                {
                    AZStd::this_thread::yield();
                }
            }

            AZStd::atomic_bool m_entered{false};
            AZStd::atomic_bool m_release{false};
        };

        class RecordingCharacterCollisionFilter final
            : public ICharacterCollisionFilter
        {
        public:
            [[nodiscard]]
            bool ShouldIncludeBody(
                const BodyHandle bodyHandle) const override
            {
                ++m_bodyCallCount;
                return bodyHandle != m_rejectedBodyHandle;
            }

            [[nodiscard]]
            bool ShouldIncludeShape(
                const SimulationShape& shape) const override
            {
                ++m_shapeCallCount;
                m_lastShape = shape;
                return shape.m_kind != m_rejectedShapeKind;
            }

            [[nodiscard]]
            bool ShouldIncludeShapePair(
                const SimulationShape& queryShape,
                const SimulationShape& targetShape) const override
            {
                ++m_shapePairCallCount;
                m_lastQueryShape = queryShape;
                m_lastTargetShape = targetShape;
                return ShouldIncludeShape(targetShape);
            }

            [[nodiscard]]
            bool ShouldIncludeCharacter(
                const CharacterHandle characterHandle) const override
            {
                ++m_characterCallCount;
                return characterHandle != m_rejectedCharacterHandle;
            }

            [[nodiscard]]
            bool ShouldIncludeVirtualCharacter(
                const VirtualCharacterHandle characterHandle) const override
            {
                ++m_virtualCharacterCallCount;
                return characterHandle != m_rejectedVirtualCharacterHandle;
            }

            BodyHandle m_rejectedBodyHandle;
            CharacterHandle m_rejectedCharacterHandle;
            VirtualCharacterHandle m_rejectedVirtualCharacterHandle;
            ShapeKind m_rejectedShapeKind = ShapeKind::None;

            mutable SimulationShape m_lastQueryShape;
            mutable SimulationShape m_lastShape;
            mutable SimulationShape m_lastTargetShape;

            mutable AZ::u32 m_bodyCallCount = 0;
            mutable AZ::u32 m_characterCallCount = 0;
            mutable AZ::u32 m_shapeCallCount = 0;
            mutable AZ::u32 m_shapePairCallCount = 0;
            mutable AZ::u32 m_virtualCharacterCallCount = 0;
        };

        class RecordingVirtualCharacterContactCallbacks final
            : public IVirtualCharacterContactCallbacks
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return RecordingVirtualCharacterContactStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return m_stateHash;
            }

            void OnAdjustBodyVelocity(
                const VirtualCharacterHandle characterHandle,
                const BodyHandle bodyHandle,
                AZ::Vector3& linearVelocity,
                [[maybe_unused]] AZ::Vector3& angularVelocity) override
            {
                ++m_adjustBodyVelocityCount;
                m_characterHandle = characterHandle;
                m_bodyHandle = bodyHandle;
                linearVelocity.SetX(0.25f);
            }

            [[nodiscard]]
            bool OnContactValidate(
                const VirtualCharacterHandle characterHandle,
                const VirtualCharacterContact& contact) override
            {
                ++m_bodyValidationCount;
                m_characterHandle = characterHandle;
                m_bodyHandle = contact.m_bodyHandle;
                return true;
            }

            void OnContactAdded(
                const VirtualCharacterHandle characterHandle,
                const VirtualCharacterContact& contact,
                VirtualCharacterContactSettings& settings) override
            {
                ++m_bodyAddedCount;
                m_characterHandle = characterHandle;
                m_bodyHandle = contact.m_bodyHandle;
                settings.m_canPushCharacter = false;
                settings.m_canReceiveImpulses = false;
            }

            void OnContactPersisted(
                const VirtualCharacterHandle characterHandle,
                const VirtualCharacterContact& contact,
                VirtualCharacterContactSettings& settings) override
            {
                ++m_bodyPersistedCount;
                m_characterHandle = characterHandle;
                m_bodyHandle = contact.m_bodyHandle;
                settings.m_canPushCharacter = false;
                settings.m_canReceiveImpulses = false;
            }

            void OnContactRemoved(
                const VirtualCharacterHandle characterHandle,
                const BodyHandle bodyHandle,
                [[maybe_unused]] const SubShapeId subShapeId) override
            {
                ++m_bodyRemovedCount;
                m_characterHandle = characterHandle;
                m_removedBodyHandle = bodyHandle;
            }

            void OnContactSolve(
                const VirtualCharacterHandle characterHandle,
                const VirtualCharacterContactSolve& contact,
                AZ::Vector3& newCharacterVelocity) override
            {
                ++m_bodySolveCount;
                m_characterHandle = characterHandle;
                m_bodyHandle = contact.m_bodyHandle;
                newCharacterVelocity = AZ::Vector3::CreateZero();
            }

            [[nodiscard]]
            bool OnCharacterContactValidate(
                const VirtualCharacterHandle characterHandle,
                const VirtualCharacterContact& contact) override
            {
                ++m_characterValidationCount;
                m_characterHandle = characterHandle;
                m_otherCharacterHandle = contact.m_characterHandle;
                return true;
            }

            void OnCharacterContactAdded(
                const VirtualCharacterHandle characterHandle,
                const VirtualCharacterContact& contact,
                VirtualCharacterContactSettings& settings) override
            {
                ++m_characterAddedCount;
                m_characterHandle = characterHandle;
                m_otherCharacterHandle = contact.m_characterHandle;
                settings.m_canPushCharacter = false;
            }

            void OnCharacterContactPersisted(
                const VirtualCharacterHandle characterHandle,
                const VirtualCharacterContact& contact,
                VirtualCharacterContactSettings& settings) override
            {
                ++m_characterPersistedCount;
                m_characterHandle = characterHandle;
                m_otherCharacterHandle = contact.m_characterHandle;
                settings.m_canPushCharacter = false;
            }

            void OnCharacterContactRemoved(
                const VirtualCharacterHandle characterHandle,
                const VirtualCharacterHandle otherCharacterHandle,
                [[maybe_unused]] const SubShapeId subShapeId) override
            {
                ++m_characterRemovedCount;
                m_characterHandle = characterHandle;
                m_removedCharacterHandle = otherCharacterHandle;
            }

            void OnCharacterContactSolve(
                const VirtualCharacterHandle characterHandle,
                const VirtualCharacterContactSolve& contact,
                AZ::Vector3& newCharacterVelocity) override
            {
                ++m_characterSolveCount;
                m_characterHandle = characterHandle;
                m_otherCharacterHandle = contact.m_characterHandle;
                newCharacterVelocity = AZ::Vector3::CreateZero();
            }

            BodyHandle m_bodyHandle;
            BodyHandle m_removedBodyHandle;
            VirtualCharacterHandle m_characterHandle;
            VirtualCharacterHandle m_otherCharacterHandle;
            VirtualCharacterHandle m_removedCharacterHandle;
            AZ::u64 m_stateHash = 0;
            AZ::u32 m_adjustBodyVelocityCount = 0;
            AZ::u32 m_bodyAddedCount = 0;
            AZ::u32 m_bodyPersistedCount = 0;
            AZ::u32 m_bodyRemovedCount = 0;
            AZ::u32 m_bodySolveCount = 0;
            AZ::u32 m_bodyValidationCount = 0;
            AZ::u32 m_characterAddedCount = 0;
            AZ::u32 m_characterPersistedCount = 0;
            AZ::u32 m_characterRemovedCount = 0;
            AZ::u32 m_characterSolveCount = 0;
            AZ::u32 m_characterValidationCount = 0;
        };

        class RecordingStepListener final
            : public IStepListener
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return RecordingStepListenerStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return AZStd::hash<BodyHandle>{}(m_bodyHandle)
                    ^ (AZStd::hash<ConstraintHandle>{}(m_constraintHandle) << 1);
            }

            void OnStep(
                const StepInformation& information,
                IStepContext& context) override
            {
                ++m_callCount;
                m_sawFirst = m_sawFirst || information.m_isFirst;
                m_sawLast = m_sawLast || information.m_isLast;
                m_lastDeltaTime = information.m_deltaTime;

                BodyState state;
                m_readState = context.GetBodyState(m_bodyHandle, state);
                if (information.m_isFirst && m_readState)
                {
                    m_addedForce = context.AddForceAndTorque(
                        m_bodyHandle,
                        AZ::Vector3::CreateAxisX(2.0f),
                        AZ::Vector3::CreateZero());
                    m_addedImpulse = context.AddImpulse(
                        m_bodyHandle,
                        AZ::Vector3::CreateAxisX(0.5f));
                    m_disabledConstraint = context.SetConstraintEnabled(m_constraintHandle, false);
                }
                if (information.m_isLast)
                {
                    m_setVelocities = context.SetBodyVelocities(
                        m_bodyHandle,
                        AZ::Vector3::CreateAxisX(3.0f),
                        AZ::Vector3::CreateZero());
                }
            }

            BodyHandle m_bodyHandle;
            ConstraintHandle m_constraintHandle;
            float m_lastDeltaTime = 0.0f;
            AZ::u32 m_callCount = 0;
            bool m_addedForce = false;
            bool m_addedImpulse = false;
            bool m_disabledConstraint = false;
            bool m_readState = false;
            bool m_sawFirst = false;
            bool m_sawLast = false;
            bool m_setVelocities = false;
        };

        class StatefulStepListener final
            : public IStepListener
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return m_typeId;
            }

            [[nodiscard]]
            AZ::u32 GetStateVersion() const override
            {
                return m_version;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return m_state;
            }

            [[nodiscard]]
            size_t GetStateByteCount() const override
            {
                return sizeof(m_state);
            }

            bool CaptureState(const AZStd::span<AZ::u8> state) const override
            {
                if (state.size() != sizeof(m_state))
                {
                    return false;
                }
                std::memcpy(state.data(), &m_state, sizeof(m_state));
                return true;
            }

            bool PrepareRestoreState(const AZStd::span<const AZ::u8> state) const override
            {
                ++m_prepareCount;
                if (m_remainingRestoreFailureCount > 0)
                {
                    --m_remainingRestoreFailureCount;
                    return false;
                }
                return state.size() == sizeof(m_state);
            }

            void CommitRestoreState(const AZStd::span<const AZ::u8> state) const override
            {
                ++m_commitCount;
                std::memcpy(&m_state, state.data(), sizeof(m_state));
                if (m_corruptCommitState)
                {
                    ++m_state;
                }
            }

            void OnStep(
                [[maybe_unused]] const StepInformation& information,
                [[maybe_unused]] IStepContext& context) override
            {
            }

            mutable AZ::u64 m_state = 0;
            mutable AZ::u32 m_commitCount = 0;
            mutable AZ::u32 m_prepareCount = 0;
            mutable AZ::u32 m_remainingRestoreFailureCount = 0;
            AZ::TypeId m_typeId = StatefulStepListenerStateTypeId;
            AZ::u32 m_version = 0;
            bool m_corruptCommitState = false;
        };

        class MutatingStepListener final
            : public IStepListener
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return MutatingStepListenerStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return AZStd::hash<WorldHandle>{}(m_worldHandle)
                    ^ (AZStd::hash<ExtensionHandle>{}(m_targetListenerHandle) << 1)
                    ^ (AZStd::hash<ExtensionHandle>{}(m_selfHandle) << 2);
            }

            void OnStep(
                [[maybe_unused]] const StepInformation& information,
                [[maybe_unused]] IStepContext& context) override
            {
                ++m_attemptCount;
                m_addRejected = !m_system->AddStepListener(m_worldHandle, m_targetListenerHandle);
                m_removeRejected = !m_system->RemoveStepListener(m_worldHandle, m_selfHandle);
            }

            Runtime* m_system = nullptr;
            ExtensionHandle m_targetListenerHandle;
            ExtensionHandle m_selfHandle;
            WorldHandle m_worldHandle;
            AZ::u32 m_attemptCount = 0;
            bool m_addRejected = false;
            bool m_removeRejected = false;
        };

        enum class BodyCollisionOperation : AZ::u8
        {
            None = 0,
            Any,
            AnyPerLeaf,
            CustomHit,
            Deepest,
            DeepestPerLeaf,
            Default,
        };

        class RecordingBodyPairCollider final
            : public IBodyPairCollider
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return RecordingBodyPairColliderStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return static_cast<AZ::u64>(m_operation);
            }

            void Collide(
                const BodyPairCollisionInput& input,
                const BodyPairCollisionSettings& settings,
                IBodyPairCollisionCollector& collector) override
            {
                ++m_callCount;
                m_lastInput = input;
                m_lastSettings = settings;
                m_sawEarlyOut = collector.ShouldEarlyOut();
                switch (m_operation)
                {
                case BodyCollisionOperation::Any:
                    m_operationSucceeded = collector.CollideAny(settings);
                    return;
                case BodyCollisionOperation::AnyPerLeaf:
                    m_operationSucceeded = collector.CollideAnyPerLeaf(settings);
                    return;
                case BodyCollisionOperation::CustomHit:
                {
                    const WorldPosition& firstPosition = input.m_first.m_centerOfMassTransform.m_position;
                    const WorldPosition& secondPosition = input.m_second.m_centerOfMassTransform.m_position;
                    AZ::Vector3 penetrationAxis(
                        aznumeric_cast<float>(secondPosition.m_x - firstPosition.m_x),
                        aznumeric_cast<float>(secondPosition.m_y - firstPosition.m_y),
                        aznumeric_cast<float>(secondPosition.m_z - firstPosition.m_z));
                    penetrationAxis.Normalize();
                    const AZ::Vector3 firstContactOffset = penetrationAxis * 0.5f;
                    const AZ::Vector3 secondContactOffset = -firstContactOffset;
                    const BodyPairCollisionHit hit = {
                        .m_contactPositionOnFirst = {
                            .m_x = firstPosition.m_x + firstContactOffset.GetX(),
                            .m_y = firstPosition.m_y + firstContactOffset.GetY(),
                            .m_z = firstPosition.m_z + firstContactOffset.GetZ(),
                        },
                        .m_contactPositionOnSecond = {
                            .m_x = secondPosition.m_x + secondContactOffset.GetX(),
                            .m_y = secondPosition.m_y + secondContactOffset.GetY(),
                            .m_z = secondPosition.m_z + secondContactOffset.GetZ(),
                        },
                        .m_penetrationAxis = penetrationAxis,
                        .m_firstSubShapeId = SubShapeId::Root,
                        .m_secondSubShapeId = SubShapeId::Root,
                        .m_penetrationDepth = 0.25f,
                    };
                    m_operationSucceeded = collector.AddHit(hit);
                    return;
                }
                case BodyCollisionOperation::Deepest:
                    m_operationSucceeded = collector.CollideDeepest(settings);
                    return;
                case BodyCollisionOperation::DeepestPerLeaf:
                    m_operationSucceeded = collector.CollideDeepestPerLeaf(settings);
                    return;
                case BodyCollisionOperation::Default:
                    m_operationSucceeded = collector.CollideDefault(settings);
                    return;
                case BodyCollisionOperation::None:
                    return;
                }
            }

            BodyPairCollisionInput m_lastInput;
            BodyPairCollisionSettings m_lastSettings;
            BodyCollisionOperation m_operation = BodyCollisionOperation::None;
            AZ::u32 m_callCount = 0;
            bool m_operationSucceeded = false;
            bool m_sawEarlyOut = false;
        };

        class InvalidBodyPairCollider final
            : public IBodyPairCollider
        {
        public:
            [[nodiscard]]
            AZ::TypeId GetStateTypeId() const override
            {
                return InvalidBodyPairColliderStateTypeId;
            }

            [[nodiscard]]
            AZ::u64 GetStateHash() const override
            {
                return 0;
            }

            void Collide(
                const BodyPairCollisionInput& input,
                const BodyPairCollisionSettings& settings,
                IBodyPairCollisionCollector& collector) override
            {
                ++m_callCount;
                BodyPairCollisionSettings invalidSettings = settings;
                invalidSettings.m_collisionTolerance = 0.0f;
                m_invalidSettingsRejected = !collector.CollideDefault(invalidSettings);

                const BodyPairCollisionHit invalidHit = {
                    .m_contactPositionOnFirst = input.m_first.m_centerOfMassTransform.m_position,
                    .m_contactPositionOnSecond = input.m_second.m_centerOfMassTransform.m_position,
                    .m_firstSubShapeId = SubShapeId::Root,
                    .m_secondSubShapeId = SubShapeId::Root,
                    .m_penetrationDepth = 0.25f,
                };
                m_invalidHitRejected = !collector.AddHit(invalidHit);
            }

            AZ::u32 m_callCount = 0;
            bool m_invalidHitRejected = false;
            bool m_invalidSettingsRejected = false;
        };

        [[nodiscard]]
        SphereOnFloor CreateSphereOnFloor(
            Runtime& system,
            const WorldHandle worldHandle)
        {
            SphereOnFloor scene;
            scene.m_worldHandle = worldHandle;

            ShapeConfiguration floorShapeConfiguration;
            floorShapeConfiguration.m_geometry = BoxShapeConfiguration{
                .m_dimensions = AZ::Vector3(20.0f, 20.0f, 1.0f),
            };
            scene.m_floorShapeHandle = system.CreateShape(scene.m_worldHandle, floorShapeConfiguration);

            ShapeConfiguration sphereShapeConfiguration;
            sphereShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
            scene.m_sphereShapeHandle = system.CreateShape(scene.m_worldHandle, sphereShapeConfiguration);

            BodyConfiguration floorBodyConfiguration;
            floorBodyConfiguration.m_shapeHandle = scene.m_floorShapeHandle;
            floorBodyConfiguration.m_transform.m_position.m_z = -0.5;
            floorBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
            floorBodyConfiguration.m_motionType = MotionType::Static;
            scene.m_floorBodyHandle = system.CreateBody(scene.m_worldHandle, floorBodyConfiguration);

            BodyConfiguration sphereBodyConfiguration;
            sphereBodyConfiguration.m_shapeHandle = scene.m_sphereShapeHandle;
            sphereBodyConfiguration.m_transform.m_position.m_z = 2.0;
            scene.m_sphereBodyHandle = system.CreateBody(scene.m_worldHandle, sphereBodyConfiguration);
            return scene;
        }

        [[nodiscard]]
        SphereOnFloor CreateSphereOnFloor(
            Runtime& system)
        {
            return CreateSphereOnFloor(system, system.GetDefaultWorldHandle());
        }

        void DestroySphereOnFloor(
            Runtime& system,
            const SphereOnFloor& scene)
        {
            EXPECT_TRUE(system.DestroyBody(scene.m_worldHandle, scene.m_sphereBodyHandle));
            EXPECT_TRUE(system.DestroyBody(scene.m_worldHandle, scene.m_floorBodyHandle));
            EXPECT_TRUE(system.DestroyShape(scene.m_worldHandle, scene.m_sphereShapeHandle));
            EXPECT_TRUE(system.DestroyShape(scene.m_worldHandle, scene.m_floorShapeHandle));
        }

        [[nodiscard]]
        SystemConfiguration CreateSerialSystemConfiguration()
        {
            SystemConfiguration configuration;
            configuration.m_defaultWorld.m_workerCount = 1;
            return configuration;
        }

        [[nodiscard]]
        bool ChangeWorkerRoundingModes(
            AZ::JobContext& jobContext,
            const size_t workerCount,
            const int newRoundingMode,
            const int expectedRoundingMode)
        {
            AZStd::atomic_size_t startedWorkerCount{0};
            AZStd::atomic_bool releaseWorkers{false};
            AZStd::atomic_bool succeeded{true};
            AZ::JobCompletion completion(&jobContext);
            completion.Reset(true);
            for (size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
            {
                AZ::Job* job = AZ::CreateJobFunction(
                    [&startedWorkerCount, &releaseWorkers, &succeeded, newRoundingMode, expectedRoundingMode]
                    {
                        if (std::fegetround() != expectedRoundingMode
                            || std::fesetround(newRoundingMode) != 0)
                        {
                            succeeded.store(false);
                        }
                        startedWorkerCount.fetch_add(1);
                        while (!releaseWorkers.load())
                        {
                            AZStd::this_thread::yield();
                        }
                    },
                    true,
                    &jobContext);
                job->SetDependent(&completion);
                job->Start();
            }

            while (startedWorkerCount.load() != workerCount)
            {
                AZStd::this_thread::yield();
            }
            releaseWorkers.store(true);
            completion.StartAndWaitForCompletion();
            return succeeded.load();
        }

        struct HairDeterminismResult final
        {
            AZStd::vector<HairVertexState> m_vertexStates;
            AZStd::vector<AZ::Vector3> m_renderPositions;
            AZStd::vector<HairGridCellState> m_gridCells;
            bool m_succeeded = false;
        };

        [[nodiscard]]
        HairDeterminismResult SimulateDeterministicHair(
            const AZ::u32 workerCount,
            AZ::JobContext* jobContext)
        {
            constexpr AZ::u32 strandCount = 256;
            constexpr AZ::u32 verticesPerStrand = 8;
            SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
            systemConfiguration.m_defaultWorld.m_workerCount = workerCount;
            Runtime system(systemConfiguration, jobContext);
            HairDeterminismResult result;
            if (!system)
            {
                return result;
            }

            HairDefinitionConfiguration definitionConfiguration;
            definitionConfiguration.m_vertices.reserve(strandCount * verticesPerStrand);
            definitionConfiguration.m_strands.reserve(strandCount);
            for (AZ::u32 strandIndex = 0; strandIndex < strandCount; ++strandIndex)
            {
                const AZ::u32 beginVertex = aznumeric_cast<AZ::u32>(definitionConfiguration.m_vertices.size());
                const float rootX = 0.01f * aznumeric_cast<float>(strandIndex % 16);
                const float rootY = 0.01f * aznumeric_cast<float>(strandIndex / 16);
                for (AZ::u32 vertexIndex = 0; vertexIndex < verticesPerStrand; ++vertexIndex)
                {
                    definitionConfiguration.m_vertices.push_back({
                        .m_position = AZ::Vector3(
                            rootX,
                            rootY,
                            1.0f - 0.02f * aznumeric_cast<float>(vertexIndex)),
                        .m_inverseMass = 1.0f,
                    });
                }
                definitionConfiguration.m_vertices[beginVertex].m_inverseMass = 0.0f;
                definitionConfiguration.m_strands.push_back({
                    .m_beginVertex = beginVertex,
                    .m_endVertex = aznumeric_cast<AZ::u32>(definitionConfiguration.m_vertices.size()),
                    .m_materialIndex = 0,
                });
            }
            definitionConfiguration.m_materials.resize(1);
            definitionConfiguration.m_materials[0].m_simulationStrandFraction = 1.0f;
            definitionConfiguration.m_scalpVertices = {
                AZ::Vector3(-1.0f, -1.0f, 1.0f),
                AZ::Vector3(1.0f, -1.0f, 1.0f),
                AZ::Vector3(0.0f, 1.0f, 1.0f),
            };
            definitionConfiguration.m_scalpTriangles = {
                {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
            };
            definitionConfiguration.m_scalpInverseBindPoses = {
                AZ::Transform::CreateIdentity(),
            };
            definitionConfiguration.m_scalpSkinWeights = {
                {.m_jointIndex = 0, .m_weight = 1.0f},
                {.m_jointIndex = 0, .m_weight = 1.0f},
                {.m_jointIndex = 0, .m_weight = 1.0f},
            };
            definitionConfiguration.m_scalpSkinWeightsPerVertex = 1;
            definitionConfiguration.m_gridSizeX = 16;
            definitionConfiguration.m_gridSizeY = 16;
            definitionConfiguration.m_gridSizeZ = 16;
            const HairDefinitionHandle definitionHandle =
                system.CreateHairDefinition(definitionConfiguration);
            HairDefinitionState definitionState;
            if (!definitionHandle
                || !system.GetHairDefinitionState(definitionHandle, definitionState))
            {
                return result;
            }

            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            ShapeConfiguration collisionShapeConfiguration;
            collisionShapeConfiguration.m_geometry = ConvexHullShapeConfiguration{
                .m_points = {
                    AZ::Vector3(-0.25f, -0.25f, -0.25f),
                    AZ::Vector3(-0.25f, -0.25f, 0.25f),
                    AZ::Vector3(-0.25f, 0.25f, -0.25f),
                    AZ::Vector3(-0.25f, 0.25f, 0.25f),
                    AZ::Vector3(0.25f, -0.25f, -0.25f),
                    AZ::Vector3(0.25f, -0.25f, 0.25f),
                    AZ::Vector3(0.25f, 0.25f, -0.25f),
                    AZ::Vector3(0.25f, 0.25f, 0.25f),
                },
                .m_maximumConvexRadius = 0.02f,
            };
            const ShapeHandle collisionShapeHandle = system.CreateShape(
                worldHandle,
                collisionShapeConfiguration);
            BodyConfiguration collisionBodyConfiguration;
            collisionBodyConfiguration.m_shapeHandle = collisionShapeHandle;
            collisionBodyConfiguration.m_transform.m_position = {
                .m_x = 0.08,
                .m_y = 0.08,
                .m_z = 0.8,
            };
            collisionBodyConfiguration.m_motionType = MotionType::Static;
            collisionBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
            const BodyHandle collisionBodyHandle = system.CreateBody(
                worldHandle,
                collisionBodyConfiguration);
            HairConfiguration hairConfiguration;
            hairConfiguration.m_definitionHandle = definitionHandle;
            hairConfiguration.m_objectLayer = DefaultLayers::Moving;
            const HairHandle hairHandle = system.CreateHair(worldHandle, hairConfiguration);
            if (!collisionShapeHandle
                || !collisionBodyHandle
                || !hairHandle)
            {
                return result;
            }

            constexpr AZ::u32 stepCount = 12;
            AZStd::array<AZ::Transform, 1> jointModelTransforms;
            for (AZ::u32 stepIndex = 0; stepIndex < stepCount; ++stepIndex)
            {
                jointModelTransforms[0] = AZ::Transform::CreateTranslation(
                    AZ::Vector3::CreateAxisX(0.001f * aznumeric_cast<float>(stepIndex)));
                if (stepIndex == 1)
                {
                    WorldTransform movedTransform;
                    movedTransform.m_position.m_x = 0.01;
                    if (!system.SetHairTransform(
                        worldHandle,
                        hairHandle,
                        movedTransform,
                        false))
                    {
                        return result;
                    }
                }
                if (!system.UpdateHair(
                    worldHandle,
                    hairHandle,
                    1.0f / 60.0f,
                    AZ::Transform::CreateIdentity(),
                    jointModelTransforms))
                {
                    return result;
                }
            }

            result.m_vertexStates.resize(definitionState.m_simulationVertexCount);
            result.m_renderPositions.resize(definitionState.m_renderVertexCount);
            result.m_gridCells.resize(definitionState.m_gridCellCount);
            const HairReadbackBuffers buffers{
                .m_vertexStates = result.m_vertexStates,
                .m_renderPositions = result.m_renderPositions,
                .m_gridCells = result.m_gridCells,
            };
            HairReadbackResult readbackResult;
            result.m_succeeded = system.GetHairReadback(
                worldHandle,
                hairHandle,
                buffers,
                readbackResult)
                && readbackResult.m_vertexStates.IsComplete()
                && readbackResult.m_renderPositions.IsComplete()
                && readbackResult.m_gridCells.IsComplete();
            return result;
        }

        [[nodiscard]]
        WorldStateDigest SimulateDeterministicStack(
            const AZ::u32 workerCount,
            AZ::JobContext* jobContext)
        {
            SystemConfiguration configuration;
            configuration.m_defaultWorld.m_workerCount = workerCount;
            Runtime system(configuration, jobContext);
            if (!system)
            {
                return {};
            }

            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            ShapeConfiguration floorShapeConfiguration;
            floorShapeConfiguration.m_geometry = BoxShapeConfiguration{
                .m_dimensions = AZ::Vector3(20.0f, 20.0f, 1.0f),
            };
            const ShapeHandle floorShapeHandle = system.CreateShape(worldHandle, floorShapeConfiguration);
            BodyConfiguration floorConfiguration;
            floorConfiguration.m_shapeHandle = floorShapeHandle;
            floorConfiguration.m_transform.m_position.m_z = -0.5;
            floorConfiguration.m_motionType = MotionType::Static;
            [[maybe_unused]] const BodyHandle floorBodyHandle =
                system.CreateBody(worldHandle, floorConfiguration);

            ShapeConfiguration sphereShapeConfiguration;
            sphereShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.25f};
            const ShapeHandle sphereShapeHandle = system.CreateShape(worldHandle, sphereShapeConfiguration);
            for (AZ::u32 bodyIndex = 0; bodyIndex < 64; ++bodyIndex)
            {
                BodyConfiguration bodyConfiguration;
                bodyConfiguration.m_shapeHandle = sphereShapeHandle;
                bodyConfiguration.m_transform.m_position = {
                    .m_x = static_cast<double>(bodyIndex % 4) * 0.55 - 0.825,
                    .m_y = static_cast<double>((bodyIndex / 4) % 4) * 0.55 - 0.825,
                    .m_z = static_cast<double>(bodyIndex / 16) * 0.55 + 0.3,
                };
                [[maybe_unused]] const BodyHandle bodyHandle =
                    system.CreateBody(worldHandle, bodyConfiguration);
            }

            for (AZ::u32 step = 0; step < 180; ++step)
            {
                if (!system.StepWorld(worldHandle, 1.0f / 60.0f))
                {
                    return {};
                }
            }

            WorldStateDigest digest;
            if (!system.GetWorldStateDigest(worldHandle, digest))
            {
                return {};
            }
            return digest;
        }

        [[nodiscard]]
        WorldStateDigest SimulateDeterministicSoftBodies(
            const AZ::u32 workerCount,
            AZ::JobContext* jobContext)
        {
            SystemConfiguration configuration;
            configuration.m_defaultWorld.m_gravity = AZ::Vector3(0.0f, 0.0f, -4.0f);
            configuration.m_defaultWorld.m_workerCount = workerCount;
            Runtime system(configuration, jobContext);
            if (!system)
            {
                return {};
            }

            SoftBodyDefinitionConfiguration definitionConfiguration;
            definitionConfiguration.m_vertices = {
                {.m_position = AZ::Vector3(-0.5f, 0.0f, 0.0f)},
                {.m_position = AZ::Vector3(0.5f, 0.0f, 0.0f)},
                {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
            };
            definitionConfiguration.m_faces = {
                {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
            };
            definitionConfiguration.m_edgeConstraints = {
                {.m_firstVertex = 0, .m_secondVertex = 1},
                {.m_firstVertex = 1, .m_secondVertex = 2},
                {.m_firstVertex = 2, .m_secondVertex = 0},
            };
            const SoftBodyDefinitionHandle definitionHandle =
                system.CreateSoftBodyDefinition(definitionConfiguration);
            if (!definitionHandle)
            {
                return {};
            }

            constexpr AZ::u32 bodyCount = 32;
            AZStd::array<BodyHandle, bodyCount> bodyHandles;
            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
            {
                SoftBodyConfiguration bodyConfiguration;
                bodyConfiguration.m_definitionHandle = definitionHandle;
                bodyConfiguration.m_transform.m_position = {
                    2.0 * aznumeric_cast<double>(bodyIndex % 8),
                    2.0 * aznumeric_cast<double>(bodyIndex / 8),
                    5.0 + 0.125 * aznumeric_cast<double>(bodyIndex),
                };
                bodyHandles[bodyIndex] = system.CreateSoftBody(worldHandle, bodyConfiguration);
                if (!bodyHandles[bodyIndex])
                {
                    return {};
                }
            }

            for (AZ::u32 step = 0; step < 30; ++step)
            {
                if (!system.StepWorld(worldHandle, 1.0f / 60.0f))
                {
                    return {};
                }
            }

            const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
            if (!snapshotHandle)
            {
                return {};
            }

            const auto simulateFuture = [&system, worldHandle, &bodyHandles]
            {
                for (AZ::u32 step = 0; step < 60; ++step)
                {
                    if (step % 15 == 0)
                    {
                        for (size_t bodyIndex = 0; bodyIndex < bodyHandles.size(); ++bodyIndex)
                        {
                            const float force = 0.25f * aznumeric_cast<float>(bodyIndex % 4 + 1);
                            if (!system.AddForce(
                                worldHandle,
                                bodyHandles[bodyIndex],
                                AZ::Vector3::CreateAxisX(force)))
                            {
                                return false;
                            }
                        }
                    }

                    if (!system.StepWorld(worldHandle, 1.0f / 60.0f))
                    {
                        return false;
                    }
                }

                return true;
            };

            if (!simulateFuture())
            {
                return {};
            }
            WorldStateDigest firstDigest;
            if (!system.GetWorldStateDigest(worldHandle, firstDigest)
                || !system.RestoreWorldState(worldHandle, snapshotHandle)
                || !simulateFuture())
            {
                return {};
            }

            WorldStateDigest replayDigest;
            if (!system.GetWorldStateDigest(worldHandle, replayDigest)
                || replayDigest != firstDigest)
            {
                return {};
            }
            return replayDigest;
        }

        [[nodiscard]]
        WorldStateDigest SimulateDeterministicCustomConstraints(
            const AZ::u32 workerCount,
            AZ::JobContext* jobContext)
        {
            TestCustomConstraintProvider provider;
            SystemConfiguration configuration;
            configuration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
            configuration.m_defaultWorld.m_workerCount = workerCount;
            Runtime system(configuration, jobContext);
            if (!system)
            {
                return {};
            }
            const ExtensionRegistrationResult providerRegistration = system.RegisterExtension(&provider, {});
            if (!providerRegistration)
            {
                return {};
            }

            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.25f};
            const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
            if (!shapeHandle)
            {
                return {};
            }

            for (AZ::u32 pairIndex = 0; pairIndex < 32; ++pairIndex)
            {
                const double x = static_cast<double>(pairIndex) * 2.0;
                BodyConfiguration firstBodyConfiguration;
                firstBodyConfiguration.m_shapeHandle = shapeHandle;
                firstBodyConfiguration.m_transform.m_position.m_x = x;
                firstBodyConfiguration.m_motionType = MotionType::Static;
                const BodyHandle firstBodyHandle = system.CreateBody(
                    worldHandle,
                    firstBodyConfiguration);

                BodyConfiguration secondBodyConfiguration;
                secondBodyConfiguration.m_shapeHandle = shapeHandle;
                secondBodyConfiguration.m_transform.m_position = {
                    .m_x = x,
                    .m_z = 2.0,
                };
                const BodyHandle secondBodyHandle = system.CreateBody(
                    worldHandle,
                    secondBodyConfiguration);
                if (!firstBodyHandle || !secondBodyHandle)
                {
                    return {};
                }

                CustomConstraintConfiguration customConfiguration;
                customConfiguration.m_data = {1};
                customConfiguration.m_firstFrame.m_position = {
                    .m_x = x,
                    .m_z = 1.0,
                };
                customConfiguration.m_secondFrame.m_position = customConfiguration.m_firstFrame.m_position;
                customConfiguration.m_providerId = provider.GetId();
                customConfiguration.m_space = ConstraintSpace::World;

                ConstraintConfiguration constraintConfiguration;
                constraintConfiguration.m_firstBodyHandle = firstBodyHandle;
                constraintConfiguration.m_secondBodyHandle = secondBodyHandle;
                constraintConfiguration.m_geometry = customConfiguration;
                if (!system.CreateConstraint(worldHandle, constraintConfiguration)
                    || !system.AddImpulse(
                        worldHandle,
                        secondBodyHandle,
                        AZ::Vector3::CreateAxisZ(static_cast<float>(pairIndex + 1))))
                {
                    return {};
                }
            }

            for (AZ::u32 step = 0; step < 60; ++step)
            {
                if (!system.StepWorld(worldHandle, 1.0f / 60.0f))
                {
                    return {};
                }
            }

            WorldStateDigest digest;
            if (!system.GetWorldStateDigest(worldHandle, digest))
            {
                return {};
            }
            return digest;
        }

        [[nodiscard]]
        WorldStateDigest SimulateDeterministicVehicleCallbacks(
            const AZ::u32 workerCount,
            AZ::JobContext* jobContext)
        {
            SystemConfiguration configuration;
            configuration.m_defaultWorld.m_workerCount = workerCount;
            Runtime system(configuration, jobContext);
            if (!system)
            {
                return {};
            }

            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            ShapeConfiguration floorShapeConfiguration;
            floorShapeConfiguration.m_geometry = BoxShapeConfiguration{
                .m_dimensions = AZ::Vector3(20.0f, 20.0f, 1.0f),
            };
            const ShapeHandle floorShapeHandle = system.CreateShape(worldHandle, floorShapeConfiguration);
            BodyConfiguration floorConfiguration;
            floorConfiguration.m_shapeHandle = floorShapeHandle;
            floorConfiguration.m_transform.m_position.m_z = -0.5;
            floorConfiguration.m_motionType = MotionType::Static;
            const BodyHandle floorBodyHandle = system.CreateBody(worldHandle, floorConfiguration);

            ShapeConfiguration chassisShapeConfiguration;
            chassisShapeConfiguration.m_density = 250.0f;
            chassisShapeConfiguration.m_geometry = BoxShapeConfiguration{
                .m_dimensions = AZ::Vector3(1.8f, 4.0f, 0.6f),
            };
            const ShapeHandle chassisShapeHandle = system.CreateShape(worldHandle, chassisShapeConfiguration);
            BodyConfiguration chassisConfiguration;
            chassisConfiguration.m_shapeHandle = chassisShapeHandle;
            chassisConfiguration.m_transform.m_position.m_z = 0.8;
            const BodyHandle chassisBodyHandle = system.CreateBody(worldHandle, chassisConfiguration);

            WheeledVehicleConfiguration vehicleConfiguration;
            vehicleConfiguration.m_bodyHandle = chassisBodyHandle;
            vehicleConfiguration.m_wheels = {
                {.m_position = AZ::Vector3(-0.8f, 1.4f, -0.25f), .m_maximumHandBrakeTorque = 0.0f},
                {.m_position = AZ::Vector3(0.8f, 1.4f, -0.25f), .m_maximumHandBrakeTorque = 0.0f},
                {.m_position = AZ::Vector3(-0.8f, -1.4f, -0.25f), .m_maximumSteerAngle = 0.0f},
                {.m_position = AZ::Vector3(0.8f, -1.4f, -0.25f), .m_maximumSteerAngle = 0.0f},
            };
            vehicleConfiguration.m_differentials = {
                {.m_leftWheel = 0, .m_rightWheel = 1},
            };
            const VehicleHandle vehicleHandle =
                system.CreateWheeledVehicle(worldHandle, vehicleConfiguration);
            if (!floorShapeHandle
                || !floorBodyHandle
                || !chassisShapeHandle
                || !chassisBodyHandle
                || !vehicleHandle)
            {
                return {};
            }

            RecordingVehicleCallbacks callbacks;
            RecordingVehicleCollisionFilter filter;
            const ExtensionRegistrationResult callbacksRegistration = system.RegisterExtension(&callbacks, {});
            const ExtensionRegistrationResult filterRegistration = system.RegisterExtension(&filter, {});
            if (!callbacksRegistration
                || !filterRegistration
                || !system.SetVehicleCallbacks(worldHandle, vehicleHandle, callbacksRegistration.m_handle)
                || !system.SetVehicleCollisionFilter(worldHandle, vehicleHandle, filterRegistration.m_handle))
            {
                return {};
            }

            for (AZ::u32 step = 0; step < 120; ++step)
            {
                if (!system.StepWorld(worldHandle, 1.0f / 60.0f))
                {
                    return {};
                }
            }

            WorldStateDigest digest;
            if (!system.GetWorldStateDigest(worldHandle, digest))
            {
                return {};
            }
            if (!system.SetVehicleCallbacks(worldHandle, vehicleHandle, ExtensionHandle::Invalid)
                || !system.SetVehicleCollisionFilter(worldHandle, vehicleHandle, ExtensionHandle::Invalid)
                || system.UnregisterExtension(callbacksRegistration.m_handle) != ExtensionRegistrationStatus::Success
                || system.UnregisterExtension(filterRegistration.m_handle) != ExtensionRegistrationStatus::Success)
            {
                return {};
            }
            return digest;
        }

        [[nodiscard]]
        WorldStateDigest SimulateDeterministicRagdoll(
            const AZ::u32 workerCount,
            AZ::JobContext* jobContext)
        {
            NameDictionaryScope nameDictionaryScope;
            SystemConfiguration configuration;
            configuration.m_defaultWorld.m_workerCount = workerCount;
            Runtime system(configuration, jobContext);
            if (!system)
            {
                return {};
            }

            SkeletonDefinitionConfiguration skeletonConfiguration;
            skeletonConfiguration.m_joints = {
                {.m_name = AZ::Name("root"), .m_parentIndex = -1},
                {.m_name = AZ::Name("child"), .m_parentIndex = 0},
            };
            const SkeletonDefinitionHandle skeletonHandle =
                system.CreateSkeletonDefinition(skeletonConfiguration);
            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.25f};
            const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
            if (!skeletonHandle || !shapeHandle)
            {
                return {};
            }

            RagdollDefinitionConfiguration definitionConfiguration;
            definitionConfiguration.m_skeletonHandle = skeletonHandle;
            definitionConfiguration.m_parts.resize(2);
            definitionConfiguration.m_parts[0].m_body.m_shapeHandle = shapeHandle;
            definitionConfiguration.m_parts[1].m_body.m_shapeHandle = shapeHandle;
            definitionConfiguration.m_parts[1].m_body.m_transform.m_position.m_z = 1.0;
            HingeConstraintConfiguration hinge;
            hinge.m_firstPoint.m_z = 0.5;
            hinge.m_secondPoint.m_z = -0.5;
            definitionConfiguration.m_parts[1].m_toParent = hinge;
            const RagdollDefinitionHandle definitionHandle =
                system.CreateRagdollDefinition(worldHandle, definitionConfiguration);
            if (!definitionHandle)
            {
                return {};
            }

            RagdollConfiguration ragdollConfiguration;
            ragdollConfiguration.m_definitionHandle = definitionHandle;
            ragdollConfiguration.m_rootPosition.m_z = 4.0;
            const RagdollHandle ragdollHandle = system.CreateRagdoll(worldHandle, ragdollConfiguration);
            if (!ragdollHandle
                || !system.AddRagdollImpulse(
                    worldHandle,
                    ragdollHandle,
                    AZ::Vector3(0.5f, -0.25f, 0.75f)))
            {
                return {};
            }

            for (AZ::u32 step = 0; step < 120; ++step)
            {
                if (!system.StepWorld(worldHandle, 1.0f / 60.0f))
                {
                    return {};
                }
            }
            WorldStateDigest digest;
            if (!system.GetWorldStateDigest(worldHandle, digest))
            {
                return {};
            }
            return digest;
        }
    } // namespace

    TEST(SimulationTests, RuntimeInfoReportsDeterministicCpuHair)
    {
        SystemConfiguration configuration = CreateSerialSystemConfiguration();
        configuration.m_createDefaultWorld = false;
        Runtime system(configuration, nullptr);
        ASSERT_TRUE(system);

        const RuntimeInfo runtimeInfo = system.GetRuntimeInfo();
        EXPECT_EQ(runtimeInfo.m_hairDeterminism, DeterminismCertification::SameBinary);
    }

    TEST(SimulationTests, NativeDiagnosticStatisticsReportAvailabilityCapacityAndReset)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const RuntimeInfo runtimeInfo = system.GetRuntimeInfo();
        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);
        AZStd::array<BroadPhaseStatistics, 64> broadPhaseStatistics;
        AZStd::array<NarrowPhaseStatistics, 64> narrowPhaseStatistics;
        const DiagnosticStatisticsResult initialBroadPhaseResult = system.GetBroadPhaseStatistics(
            scene.m_worldHandle,
            broadPhaseStatistics,
            true);
        const DiagnosticStatisticsResult initialNarrowPhaseResult = system.GetNarrowPhaseStatistics(
            narrowPhaseStatistics,
            true);

        RaycastHit raycastHit;
        ASSERT_TRUE(system.RaycastClosest(
            scene.m_worldHandle,
            RaycastRequest{
                .m_start = {.m_z = 5.0},
                .m_displacement = -AZ::Vector3::CreateAxisZ(10.0f),
            },
            raycastHit));

        AZStd::array<ShapeOverlapHit, 4> overlapHits;
        const QueryResult overlapResult = system.CollideShape(
            scene.m_worldHandle,
            ShapeOverlapRequest{
                .m_shapeHandle = scene.m_sphereShapeHandle,
                .m_transform = {
                    .m_position = {.m_z = 0.5},
                },
            },
            overlapHits);
        EXPECT_GT(overlapResult.m_hitCount, 0);

        if (runtimeInfo.m_broadPhaseStatistics)
        {
            EXPECT_NE(initialBroadPhaseResult.m_status, DiagnosticStatisticsStatus::Unavailable);
            const DiagnosticStatisticsResult overflowResult = system.GetBroadPhaseStatistics(
                scene.m_worldHandle,
                {},
                false);
            EXPECT_EQ(overflowResult.m_status, DiagnosticStatisticsStatus::Overflow);
            EXPECT_GT(overflowResult.m_requiredCount, 0);

            const DiagnosticStatisticsResult result = system.GetBroadPhaseStatistics(
                scene.m_worldHandle,
                broadPhaseStatistics,
                true);
            EXPECT_TRUE(result);
            EXPECT_GT(result.m_count, 0);
            EXPECT_GT(broadPhaseStatistics.front().m_queryCount, 0);
            EXPECT_NE(broadPhaseStatistics.front().m_queryKind, BroadPhaseQueryKind::None);

            const DiagnosticStatisticsResult resetResult = system.GetBroadPhaseStatistics(
                scene.m_worldHandle,
                broadPhaseStatistics,
                false);
            EXPECT_TRUE(resetResult);
            EXPECT_EQ(resetResult.m_count, 0);
        }
        else
        {
            EXPECT_EQ(initialBroadPhaseResult.m_status, DiagnosticStatisticsStatus::Unavailable);
            EXPECT_EQ(
                system.GetBroadPhaseStatistics(scene.m_worldHandle, broadPhaseStatistics, false).m_status,
                DiagnosticStatisticsStatus::Unavailable);
        }

        if (runtimeInfo.m_narrowPhaseStatistics)
        {
            EXPECT_NE(initialNarrowPhaseResult.m_status, DiagnosticStatisticsStatus::Unavailable);
            const DiagnosticStatisticsResult overflowResult = system.GetNarrowPhaseStatistics({}, false);
            EXPECT_EQ(overflowResult.m_status, DiagnosticStatisticsStatus::Overflow);
            EXPECT_GT(overflowResult.m_requiredCount, 0);

            const DiagnosticStatisticsResult result = system.GetNarrowPhaseStatistics(
                narrowPhaseStatistics,
                true);
            EXPECT_TRUE(result);
            EXPECT_GT(result.m_count, 0);
            EXPECT_GT(narrowPhaseStatistics.front().m_queryCount, 0);
            EXPECT_NE(narrowPhaseStatistics.front().m_queryKind, NarrowPhaseQueryKind::None);

            const DiagnosticStatisticsResult resetResult = system.GetNarrowPhaseStatistics(
                narrowPhaseStatistics,
                false);
            EXPECT_TRUE(resetResult);
            EXPECT_EQ(resetResult.m_count, 0);
        }
        else
        {
            EXPECT_EQ(initialNarrowPhaseResult.m_status, DiagnosticStatisticsStatus::Unavailable);
            EXPECT_EQ(
                system.GetNarrowPhaseStatistics(narrowPhaseStatistics, false).m_status,
                DiagnosticStatisticsStatus::Unavailable);
        }

        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, PerformanceStatisticsAreOptInResettableAndAllocationFreeToRead)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);

        WorldPerformanceStatistics statistics;
        ASSERT_TRUE(system.GetPerformanceStatistics(
            scene.m_worldHandle,
            statistics,
            false));
        EXPECT_EQ(statistics.m_enabledFlags, PerformanceStatisticsFlags::None);
        EXPECT_EQ(statistics.m_queryCount, 0);

        const auto invalidFlags = static_cast<PerformanceStatisticsFlags>(
            static_cast<AZ::u16>(PerformanceStatisticsFlags::All) | (1 << 15));
        EXPECT_FALSE(system.ConfigurePerformanceStatistics(scene.m_worldHandle, invalidFlags));
        ASSERT_TRUE(system.ConfigurePerformanceStatistics(
            scene.m_worldHandle,
            PerformanceStatisticsFlags::All));

        RaycastHit hit;
        ASSERT_TRUE(system.RaycastClosest(
            scene.m_worldHandle,
            RaycastRequest{
                .m_start = {.m_z = 5.0},
                .m_displacement = -AZ::Vector3::CreateAxisZ(10.0f),
            },
            hit));
        ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.OptimizeBroadPhase(scene.m_worldHandle));

        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(scene.m_worldHandle);
        ASSERT_TRUE(snapshotHandle);
        ASSERT_TRUE(system.RestoreWorldState(scene.m_worldHandle, snapshotHandle));

        ASSERT_TRUE(system.GetPerformanceStatistics(scene.m_worldHandle, statistics, false));
        EXPECT_EQ(statistics.m_enabledFlags, PerformanceStatisticsFlags::All);
        EXPECT_GT(statistics.m_intervalNanoseconds, 0);
        EXPECT_EQ(statistics.m_bodies.m_count, 2);
        EXPECT_GE(statistics.m_bodies.m_highWaterCount, statistics.m_bodies.m_count);
        EXPECT_EQ(statistics.m_shapes.m_count, 2);
        EXPECT_GT(statistics.m_wrapperRetainedBytes, 0);
        EXPECT_GT(statistics.m_lockCount, 0);
        EXPECT_EQ(statistics.m_queryCount, 1);
        EXPECT_EQ(statistics.m_queryHitCount, 1);
        EXPECT_GT(statistics.m_queryNanoseconds, 0);
        EXPECT_EQ(statistics.m_snapshotCaptureCount, 1);
        EXPECT_EQ(statistics.m_snapshotRestoreCount, 1);
        EXPECT_GT(statistics.m_snapshotBytes, 0);
        EXPECT_GE(statistics.m_snapshotPeakBytes, statistics.m_snapshotBytes);
        EXPECT_EQ(statistics.m_simulationStepCount, 1);
        EXPECT_GT(statistics.m_simulationNanoseconds, 0);
        EXPECT_EQ(statistics.m_broadPhaseOptimizeCount, 1);
        EXPECT_GT(statistics.m_broadPhaseOptimizeNanoseconds, 0);

        ASSERT_TRUE(system.GetPerformanceStatistics(scene.m_worldHandle, statistics, true));
        EXPECT_EQ(statistics.m_queryCount, 1);

        ASSERT_TRUE(system.GetPerformanceStatistics(scene.m_worldHandle, statistics, false));
        EXPECT_EQ(statistics.m_queryCount, 0);
        EXPECT_EQ(statistics.m_processNativeAllocationCount, 0);
        EXPECT_EQ(statistics.m_processNativeFreeCount, 0);
        EXPECT_EQ(statistics.m_snapshotCaptureCount, 0);
        EXPECT_EQ(statistics.m_snapshotRestoreCount, 0);
        EXPECT_EQ(statistics.m_simulationStepCount, 0);

        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, snapshotHandle));
        EXPECT_TRUE(system.ConfigurePerformanceStatistics(
            scene.m_worldHandle,
            PerformanceStatisticsFlags::None));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, SnapshotRecaptureReusesRetainedStorage)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(system.ConfigurePerformanceStatistics(
            scene.m_worldHandle,
            PerformanceStatisticsFlags::Resources | PerformanceStatisticsFlags::Snapshots));

        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(scene.m_worldHandle);
        ASSERT_TRUE(snapshotHandle);

        WorldPerformanceStatistics initialStatistics;
        ASSERT_TRUE(system.GetPerformanceStatistics(scene.m_worldHandle, initialStatistics, true));
        const AZ::u64 retainedBytes = initialStatistics.m_stateSnapshots.m_retainedBytes;
        ASSERT_GT(retainedBytes, 0);

        for (AZ::u32 iteration = 0; iteration < 16; ++iteration)
        {
            ASSERT_TRUE(system.CaptureWorldState(scene.m_worldHandle, snapshotHandle));
        }

        WorldPerformanceStatistics reuseStatistics;
        ASSERT_TRUE(system.GetPerformanceStatistics(scene.m_worldHandle, reuseStatistics, false));
        EXPECT_EQ(reuseStatistics.m_stateSnapshots.m_retainedBytes, retainedBytes);
        EXPECT_EQ(reuseStatistics.m_snapshotCaptureCount, 16);
        EXPECT_EQ(reuseStatistics.m_snapshotFailureCount, 0);

        BodyState capturedState;
        ASSERT_TRUE(system.GetBodyState(scene.m_worldHandle, scene.m_sphereBodyHandle, capturedState));
        WorldTransform movedTransform = capturedState.m_transform;
        movedTransform.m_position.m_x = 10.0;
        ASSERT_TRUE(system.SetBodyTransform(scene.m_worldHandle, scene.m_sphereBodyHandle, movedTransform, true));
        EXPECT_FALSE(system.CaptureWorldState(
            scene.m_worldHandle,
            snapshotHandle,
            {
                .m_flags = StateSnapshotFlags::Bodies,
                .m_filterBodies = true,
            },
            AZStd::array{BodyHandle::Invalid}));
        EXPECT_TRUE(system.IsValid(scene.m_worldHandle, snapshotHandle));
        ASSERT_TRUE(system.RestoreWorldState(scene.m_worldHandle, snapshotHandle));
        BodyState restoredState;
        ASSERT_TRUE(system.GetBodyState(scene.m_worldHandle, scene.m_sphereBodyHandle, restoredState));
        EXPECT_EQ(restoredState.m_transform.m_position, capturedState.m_transform.m_position);

        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, snapshotHandle));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, DynamicSphereSettlesOnStaticFloor)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);
        EXPECT_EQ(system.GetConfiguration().m_defaultWorld.m_workerCount, 1);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_worldHandle);
        ASSERT_TRUE(scene.m_floorShapeHandle);
        ASSERT_TRUE(scene.m_sphereShapeHandle);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);

        EXPECT_FALSE(system.DestroyShape(scene.m_worldHandle, scene.m_sphereShapeHandle));

        for (AZ::u32 step = 0; step < 180; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }

        BodyState sphereState;
        ASSERT_TRUE(system.GetBodyState(scene.m_worldHandle, scene.m_sphereBodyHandle, sphereState));
        EXPECT_NEAR(sphereState.m_transform.m_position.m_z, 0.5, 0.02);
        EXPECT_NEAR(sphereState.m_linearVelocity.GetLength(), 0.0f, 0.01f);
        EXPECT_TRUE(system.GetEvents(scene.m_worldHandle).GetActivations().empty());
        EXPECT_TRUE(system.GetEvents(scene.m_worldHandle).GetContacts().empty());

        DestroySphereOnFloor(system, scene);
        EXPECT_FALSE(system.IsValid(scene.m_worldHandle, scene.m_sphereBodyHandle));
        EXPECT_FALSE(system.IsValid(scene.m_worldHandle, scene.m_sphereShapeHandle));
    }

    TEST(SimulationTests, ParallelWorldReportsNativeJobAndScheduledTaskStatistics)
    {
        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(4);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);

        SystemConfiguration configuration = CreateSerialSystemConfiguration();
        configuration.m_defaultWorld.m_workerCount = 4;
        Runtime system(configuration, &jobContext);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);
        ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));

        WorldStatistics statistics;
        ASSERT_TRUE(system.GetWorldStatistics(scene.m_worldHandle, statistics));
        EXPECT_GT(statistics.m_lastUpdateJobCount, 0);
        EXPECT_GT(statistics.m_lastUpdateTaskCount, 0);
        EXPECT_GT(statistics.m_lastUpdateMaximumTaskCount, 0);
        EXPECT_LE(statistics.m_lastUpdateMaximumTaskCount, 3);
        EXPECT_LE(statistics.m_lastUpdateMaximumTaskCount, statistics.m_lastUpdateTaskCount);

        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, AutomaticSingleWorkerWorldsStepConcurrently)
    {
        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(2);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);

        SystemConfiguration configuration = CreateSerialSystemConfiguration();
        configuration.m_defaultWorld.m_autoSimulate = true;
        Runtime system(configuration, &jobContext);
        ASSERT_TRUE(system);

        const WorldHandle firstWorldHandle = system.GetDefaultWorldHandle();
        const WorldHandle secondWorldHandle = system.CreateWorld(configuration.m_defaultWorld);
        ASSERT_TRUE(firstWorldHandle);
        ASSERT_TRUE(secondWorldHandle);

        BlockingStepListener firstListener;
        BlockingStepListener secondListener;
        ScopedExtensionRegistration firstRegistration(system, &firstListener);
        ScopedExtensionRegistration secondRegistration(system, &secondListener);
        ASSERT_TRUE(system.AddStepListener(firstWorldHandle, firstRegistration.GetHandle()));
        ASSERT_TRUE(system.AddStepListener(secondWorldHandle, secondRegistration.GetHandle()));

        SimulationResult result;
        AZStd::thread stepThread(
            [&system, &result]
            {
                result = system.StepAutoSimulatedWorldsDetailed(1.0f / 60.0f);
            });
        const bool bothWorldsEntered = WaitUntil(
            [&firstListener, &secondListener]
            {
                return firstListener.m_entered.load(AZStd::memory_order_acquire)
                    && secondListener.m_entered.load(AZStd::memory_order_acquire);
            });
        firstListener.m_release.store(true, AZStd::memory_order_release);
        secondListener.m_release.store(true, AZStd::memory_order_release);
        stepThread.join();

        EXPECT_TRUE(bothWorldsEntered);
        EXPECT_TRUE(result);
        EXPECT_EQ(result.m_stepCount, 2);
        EXPECT_TRUE(system.RemoveStepListener(firstWorldHandle, firstRegistration.GetHandle()));
        EXPECT_TRUE(system.RemoveStepListener(secondWorldHandle, secondRegistration.GetHandle()));
        EXPECT_TRUE(system.DestroyWorld(secondWorldHandle));
    }

    TEST(SimulationTests, WorldSlotReuseDoesNotReviveMemberHandles)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle firstWorldHandle = system.CreateWorld(WorldConfiguration{});
        ASSERT_TRUE(firstWorldHandle);
        const SphereOnFloor firstScene = CreateSphereOnFloor(system, firstWorldHandle);
        ASSERT_TRUE(firstScene.m_floorShapeHandle);
        ASSERT_TRUE(firstScene.m_sphereShapeHandle);
        ASSERT_TRUE(firstScene.m_floorBodyHandle);
        ASSERT_TRUE(firstScene.m_sphereBodyHandle);
        const StateSnapshotHandle firstBodySnapshot =
            system.CaptureBodyState(firstWorldHandle, firstScene.m_sphereBodyHandle);
        const StateSnapshotHandle firstStateSnapshot = system.CaptureWorldState(firstWorldHandle);
        ASSERT_TRUE(firstBodySnapshot);
        ASSERT_TRUE(firstStateSnapshot);

        EXPECT_TRUE(system.DestroyStateSnapshot(firstWorldHandle, firstBodySnapshot));
        EXPECT_TRUE(system.DestroyStateSnapshot(firstWorldHandle, firstStateSnapshot));
        DestroySphereOnFloor(system, firstScene);
        ASSERT_TRUE(system.DestroyWorld(firstWorldHandle));

        const WorldHandle secondWorldHandle = system.CreateWorld(WorldConfiguration{});
        ASSERT_TRUE(secondWorldHandle);
        const SphereOnFloor secondScene = CreateSphereOnFloor(system, secondWorldHandle);
        ASSERT_TRUE(secondScene.m_floorShapeHandle);
        ASSERT_TRUE(secondScene.m_sphereShapeHandle);
        ASSERT_TRUE(secondScene.m_floorBodyHandle);
        ASSERT_TRUE(secondScene.m_sphereBodyHandle);
        const StateSnapshotHandle secondBodySnapshot =
            system.CaptureBodyState(secondWorldHandle, secondScene.m_sphereBodyHandle);
        const StateSnapshotHandle secondStateSnapshot = system.CaptureWorldState(secondWorldHandle);
        ASSERT_TRUE(secondBodySnapshot);
        ASSERT_TRUE(secondStateSnapshot);

        Internal::WorldHandleParts firstWorldParts;
        Internal::WorldHandleParts secondWorldParts;
        ASSERT_TRUE(Internal::DecodeWorldHandle(firstWorldHandle, firstWorldParts));
        ASSERT_TRUE(Internal::DecodeWorldHandle(secondWorldHandle, secondWorldParts));
        EXPECT_EQ(firstWorldParts.m_index, secondWorldParts.m_index);
        EXPECT_NE(firstWorldParts.m_generation, secondWorldParts.m_generation);

        EXPECT_NE(firstScene.m_floorShapeHandle, secondScene.m_floorShapeHandle);
        EXPECT_NE(firstScene.m_sphereShapeHandle, secondScene.m_sphereShapeHandle);
        EXPECT_NE(firstScene.m_floorBodyHandle, secondScene.m_floorBodyHandle);
        EXPECT_NE(firstScene.m_sphereBodyHandle, secondScene.m_sphereBodyHandle);
        EXPECT_NE(firstBodySnapshot, secondBodySnapshot);
        EXPECT_NE(firstStateSnapshot, secondStateSnapshot);

        EXPECT_FALSE(system.IsValid(secondWorldHandle, firstScene.m_floorShapeHandle));
        EXPECT_FALSE(system.IsValid(secondWorldHandle, firstScene.m_sphereShapeHandle));
        EXPECT_FALSE(system.IsValid(secondWorldHandle, firstScene.m_floorBodyHandle));
        EXPECT_FALSE(system.IsValid(secondWorldHandle, firstScene.m_sphereBodyHandle));
        EXPECT_FALSE(system.IsValid(secondWorldHandle, firstBodySnapshot));
        EXPECT_FALSE(system.IsValid(secondWorldHandle, firstStateSnapshot));

        EXPECT_TRUE(system.DestroyStateSnapshot(secondWorldHandle, secondBodySnapshot));
        EXPECT_TRUE(system.DestroyStateSnapshot(secondWorldHandle, secondStateSnapshot));
        DestroySphereOnFloor(system, secondScene);
        EXPECT_TRUE(system.DestroyWorld(secondWorldHandle));
    }

    TEST(SimulationTests, TwoThreadWorldUsesOneBackgroundJobWorker)
    {
        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(1);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);

        SystemConfiguration configuration = CreateSerialSystemConfiguration();
        configuration.m_defaultWorld.m_workerCount = 2;
        Runtime system(configuration, &jobContext);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);
        ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));

        WorldStatistics statistics;
        ASSERT_TRUE(system.GetWorldStatistics(scene.m_worldHandle, statistics));
        EXPECT_GT(statistics.m_lastUpdateJobCount, 0);
        EXPECT_GT(statistics.m_lastUpdateTaskCount, 0);
        EXPECT_EQ(statistics.m_lastUpdateMaximumTaskCount, 1);

        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, BodySimulationMembershipPreservesAllocatedState)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_transform.m_position.m_z = 5.0;
        bodyConfiguration.m_startInSimulation = false;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);
        EXPECT_TRUE(system.IsValid(worldHandle, bodyHandle));
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, bodyHandle));

        BodyState initialState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, initialState));
#if defined(JPH_TRACK_SIMULATION_STATS)
        BodySimulationStatistics statistics;
        EXPECT_TRUE(system.GetBodySimulationStatistics(worldHandle, bodyHandle, statistics));
#else
        BodySimulationStatistics statistics;
        EXPECT_FALSE(system.GetBodySimulationStatistics(worldHandle, bodyHandle, statistics));
#endif
        EXPECT_FALSE(system.GetBodySimulationStatistics(
            worldHandle,
            BodyHandle::Invalid,
            statistics));
        EXPECT_FALSE(initialState.m_isActive);
        EXPECT_FALSE(initialState.m_isInSimulation);
        EXPECT_FALSE(system.AddForce(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX()));
        EXPECT_FALSE(system.AddImpulse(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX()));

        BodyConfiguration currentConfiguration;
        ASSERT_TRUE(system.GetBodyConfiguration(worldHandle, bodyHandle, currentConfiguration));
        EXPECT_FALSE(currentConfiguration.m_startInSimulation);
        const StateSnapshotHandle membershipSnapshot = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(membershipSnapshot);

        for (AZ::u32 step = 0; step < 10; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }
        BodyState unaddedState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, unaddedState));
        EXPECT_DOUBLE_EQ(unaddedState.m_transform.m_position.m_z, initialState.m_transform.m_position.m_z);

        ASSERT_TRUE(system.AddBodyToSimulation(worldHandle, bodyHandle, false));
        EXPECT_TRUE(system.IsBodyInSimulation(worldHandle, bodyHandle));
        EXPECT_FALSE(system.AddBodyToSimulation(worldHandle, bodyHandle, true));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, membershipSnapshot));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, membershipSnapshot));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, unaddedState));
        EXPECT_TRUE(unaddedState.m_isInSimulation);
        EXPECT_FALSE(unaddedState.m_isActive);

        ASSERT_TRUE(system.ActivateBody(worldHandle, bodyHandle));
        for (AZ::u32 step = 0; step < 10; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }
        BodyState simulatedState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, simulatedState));
        EXPECT_LT(simulatedState.m_transform.m_position.m_z, initialState.m_transform.m_position.m_z);
#if defined(JPH_TRACK_SIMULATION_STATS)
        ASSERT_TRUE(system.GetBodySimulationStatistics(worldHandle, bodyHandle, statistics));
        EXPECT_EQ(statistics.m_collisionStepCount, 1);
        EXPECT_GT(statistics.m_broadPhaseTicks + statistics.m_narrowPhaseTicks, 0);
#endif

        ASSERT_TRUE(system.RemoveBodyFromSimulation(worldHandle, bodyHandle));
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, bodyHandle));
        EXPECT_FALSE(system.RemoveBodyFromSimulation(worldHandle, bodyHandle));
        EXPECT_FALSE(system.ActivateBody(worldHandle, bodyHandle));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, simulatedState));
        EXPECT_FALSE(simulatedState.m_isActive);
        EXPECT_FALSE(simulatedState.m_isInSimulation);

        const double removedPosition = simulatedState.m_transform.m_position.m_z;
        for (AZ::u32 step = 0; step < 10; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, simulatedState));
        EXPECT_DOUBLE_EQ(simulatedState.m_transform.m_position.m_z, removedPosition);

        ASSERT_TRUE(system.AddBodyToSimulation(worldHandle, bodyHandle, true));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, simulatedState));
        EXPECT_LT(simulatedState.m_transform.m_position.m_z, removedPosition);

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, ExplicitBodyIdsPreserveRigidAndSoftSimulationIdentity)
    {
        const BodyId invalidBodyId;
        EXPECT_FALSE(invalidBodyId);
        EXPECT_EQ(invalidBodyId.GetIndex(), AZStd::numeric_limits<AZ::u32>::max());
        EXPECT_EQ(invalidBodyId.GetSequenceNumber(), 0);

        const BodyId maximumBodyId(
            BodyId::MaximumIndex,
            AZStd::numeric_limits<AZ::u8>::max());
        EXPECT_TRUE(maximumBodyId);
        EXPECT_EQ(maximumBodyId.GetIndex(), BodyId::MaximumIndex);
        EXPECT_EQ(maximumBodyId.GetSequenceNumber(), AZStd::numeric_limits<AZ::u8>::max());

        const BodyId invalidIndex(BodyId::MaximumIndex + 1, 1);
        EXPECT_FALSE(invalidIndex);

        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_startInSimulation = false;
        const BodyId rigidBodyId(27, 42);
        BodyHandle rigidBodyHandle = system.CreateBodyWithId(
            worldHandle,
            rigidBodyId,
            bodyConfiguration);
        ASSERT_TRUE(rigidBodyHandle);

        BodyId returnedBodyId;
        ASSERT_TRUE(system.GetBodyId(worldHandle, rigidBodyHandle, returnedBodyId));
        EXPECT_EQ(returnedBodyId, rigidBodyId);
        EXPECT_FALSE(system.CreateBodyWithId(worldHandle, rigidBodyId, bodyConfiguration));
        EXPECT_FALSE(system.CreateBodyWithId(worldHandle, invalidBodyId, bodyConfiguration));
        EXPECT_FALSE(system.CreateBodyWithId(worldHandle, BodyId(65'536, 1), bodyConfiguration));

        returnedBodyId = rigidBodyId;
        EXPECT_FALSE(system.GetBodyId(worldHandle, BodyHandle::Invalid, returnedBodyId));
        EXPECT_FALSE(returnedBodyId);

        ASSERT_TRUE(system.DestroyBody(worldHandle, rigidBodyHandle));
        rigidBodyHandle = system.CreateBodyWithId(
            worldHandle,
            rigidBodyId,
            bodyConfiguration);
        ASSERT_TRUE(rigidBodyHandle);
        ASSERT_TRUE(system.GetBodyId(worldHandle, rigidBodyHandle, returnedBodyId));
        EXPECT_EQ(returnedBodyId, rigidBodyId);
        EXPECT_TRUE(system.DestroyBody(worldHandle, rigidBodyHandle));

        const BodyHandle automaticBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(automaticBodyHandle);
        ASSERT_TRUE(system.GetBodyId(worldHandle, automaticBodyHandle, returnedBodyId));
        EXPECT_TRUE(returnedBodyId);
        EXPECT_TRUE(system.DestroyBody(worldHandle, automaticBodyHandle));

        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(-1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        definitionConfiguration.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        SoftBodyConfiguration softBodyConfiguration;
        softBodyConfiguration.m_definitionHandle = definitionHandle;
        softBodyConfiguration.m_manualUpdate = true;
        const BodyId softBodyId(31, 11);
        const BodyHandle softBodyHandle = system.CreateSoftBodyWithId(
            worldHandle,
            softBodyId,
            softBodyConfiguration);
        ASSERT_TRUE(softBodyHandle);
        ASSERT_TRUE(system.GetBodyId(worldHandle, softBodyHandle, returnedBodyId));
        EXPECT_EQ(returnedBodyId, softBodyId);
        EXPECT_FALSE(system.CreateSoftBodyWithId(
            worldHandle,
            softBodyId,
            softBodyConfiguration));

        EXPECT_TRUE(system.DestroyBody(worldHandle, softBodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, AppliesCompleteRigidBodyConfigurationOutsideSimulation)
    {
        NameDictionaryScope nameDictionaryScope;
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration sphereConfiguration;
        sphereConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle sphereHandle = system.CreateShape(worldHandle, sphereConfiguration);
        ASSERT_TRUE(sphereHandle);

        ShapeConfiguration boxConfiguration;
        boxConfiguration.m_geometry = BoxShapeConfiguration{.m_dimensions = AZ::Vector3(2.0f, 3.0f, 4.0f)};
        const ShapeHandle boxHandle = system.CreateShape(worldHandle, boxConfiguration);
        ASSERT_TRUE(boxHandle);

        BodyConfiguration configuration;
        configuration.m_shapeHandle = sphereHandle;
        configuration.m_startInSimulation = false;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, configuration);
        ASSERT_TRUE(bodyHandle);

        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);

        configuration.m_shapeHandle = boxHandle;
        configuration.m_transform.m_position = {.m_x = 2.0, .m_y = 3.0, .m_z = 4.0};
        configuration.m_transform.m_rotation = AZ::Quaternion::CreateRotationZ(0.25f);
        configuration.m_linearVelocity = AZ::Vector3(1.0f, 2.0f, 3.0f);
        configuration.m_angularVelocity = AZ::Vector3(0.25f, 0.5f, 0.75f);
        configuration.m_entityId = AZ::EntityId(91);
        configuration.m_name = AZ::Name("reconfigured_body");
        configuration.m_userData = 0x1234'5678'9abc'def0;
        configuration.m_friction = 0.7f;
        configuration.m_restitution = 0.4f;
        configuration.m_linearDamping = 0.2f;
        configuration.m_angularDamping = 0.3f;
        configuration.m_gravityFactor = 0.5f;
        ASSERT_TRUE(system.ApplyBodyConfiguration(worldHandle, bodyHandle, configuration));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, sphereHandle));
        EXPECT_FALSE(system.DestroyShape(worldHandle, boxHandle));

        BodyConfiguration applied;
        ASSERT_TRUE(system.GetBodyConfiguration(worldHandle, bodyHandle, applied));
        EXPECT_EQ(applied.m_shapeHandle, boxHandle);
        EXPECT_EQ(applied.m_entityId, configuration.m_entityId);
        EXPECT_EQ(applied.m_name, configuration.m_name);
        EXPECT_EQ(applied.m_userData, configuration.m_userData);
        BodyState bodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, bodyState));
        EXPECT_EQ(bodyState.m_userData, configuration.m_userData);
        AZ::u64 userData = 0;
        ASSERT_TRUE(system.GetBodyUserData(worldHandle, bodyHandle, userData));
        EXPECT_EQ(userData, configuration.m_userData);
        constexpr AZ::u64 updatedUserData = 0x1111'2222'3333'4444;
        ASSERT_TRUE(system.SetBodyUserData(worldHandle, bodyHandle, updatedUserData));
        ASSERT_TRUE(system.GetBodyUserData(worldHandle, bodyHandle, userData));
        EXPECT_EQ(userData, updatedUserData);
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, bodyState));
        EXPECT_EQ(bodyState.m_userData, updatedUserData);
        EXPECT_NEAR(applied.m_transform.m_position.m_x, 2.0, 1.0e-6);
        EXPECT_NEAR(applied.m_transform.m_position.m_y, 3.0, 1.0e-6);
        EXPECT_NEAR(applied.m_transform.m_position.m_z, 4.0, 1.0e-6);
        EXPECT_TRUE(applied.m_linearVelocity.IsClose(configuration.m_linearVelocity));
        EXPECT_TRUE(applied.m_angularVelocity.IsClose(configuration.m_angularVelocity));
        EXPECT_FLOAT_EQ(applied.m_friction, configuration.m_friction);
        EXPECT_FLOAT_EQ(applied.m_restitution, configuration.m_restitution);
        EXPECT_FLOAT_EQ(applied.m_linearDamping, configuration.m_linearDamping);
        EXPECT_FLOAT_EQ(applied.m_angularDamping, configuration.m_angularDamping);
        EXPECT_FLOAT_EQ(applied.m_gravityFactor, configuration.m_gravityFactor);

        BodyConfiguration invalidConfiguration = configuration;
        invalidConfiguration.m_startInSimulation = true;
        EXPECT_FALSE(system.ApplyBodyConfiguration(worldHandle, bodyHandle, invalidConfiguration));
        ASSERT_TRUE(system.AddBodyToSimulation(worldHandle, bodyHandle, true));
        EXPECT_FALSE(system.ApplyBodyConfiguration(worldHandle, bodyHandle, configuration));

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, boxHandle));
    }

    TEST(SimulationTests, ApplyingBodyConfigurationInvalidatesPreviousContacts)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);

        WorldTransform contactTransform;
        contactTransform.m_position.m_z = 0.4;
        ASSERT_TRUE(system.SetBodyTransform(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle,
            contactTransform,
            false));
        ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.WereBodiesInContact(
            scene.m_worldHandle,
            scene.m_floorBodyHandle,
            scene.m_sphereBodyHandle));

        ASSERT_TRUE(system.RemoveBodyFromSimulation(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle));
        BodyConfiguration configuration;
        configuration.m_shapeHandle = scene.m_sphereShapeHandle;
        configuration.m_transform.m_position.m_z = 10.0;
        configuration.m_startInSimulation = false;
        ASSERT_TRUE(system.ApplyBodyConfiguration(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle,
            configuration));
        ASSERT_TRUE(system.AddBodyToSimulation(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle,
            false));
        ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        EXPECT_FALSE(system.WereBodiesInContact(
            scene.m_worldHandle,
            scene.m_floorBodyHandle,
            scene.m_sphereBodyHandle));

        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, ApplyingBodyConfigurationResetsAttachedConstraintWarmStarts)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration firstBodyConfiguration;
        firstBodyConfiguration.m_shapeHandle = shapeHandle;
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, firstBodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);

        BodyConfiguration secondBodyConfiguration = firstBodyConfiguration;
        secondBodyConfiguration.m_transform.m_position.m_z = 2.0;
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, secondBodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        FixedConstraintConfiguration fixedConfiguration;
        fixedConfiguration.m_space = ConstraintSpace::World;
        fixedConfiguration.m_autoDetectPoint = true;
        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = firstBodyHandle;
        constraintConfiguration.m_secondBodyHandle = secondBodyHandle;
        constraintConfiguration.m_geometry = fixedConfiguration;
        const ConstraintHandle constraintHandle = system.CreateConstraint(
            worldHandle,
            constraintConfiguration);
        ASSERT_TRUE(constraintHandle);

        ASSERT_TRUE(system.AddImpulse(
            worldHandle,
            secondBodyHandle,
            AZ::Vector3::CreateAxisX(10.0f)));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        ConstraintMeasurements measurements;
        ASSERT_TRUE(system.GetConstraintMeasurements(worldHandle, constraintHandle, measurements));
        ASSERT_TRUE(AZStd::holds_alternative<FixedMeasurements>(measurements));
        const FixedMeasurements& solvedMeasurements = AZStd::get<FixedMeasurements>(measurements);
        EXPECT_GT(
            solvedMeasurements.m_positionImpulse.GetLengthSq()
                + solvedMeasurements.m_rotationImpulse.GetLengthSq(),
            0.0f);

        ASSERT_TRUE(system.RemoveBodyFromSimulation(worldHandle, secondBodyHandle));
        ASSERT_TRUE(system.GetBodyConfiguration(
            worldHandle,
            secondBodyHandle,
            secondBodyConfiguration));
        secondBodyConfiguration.m_startInSimulation = false;
        secondBodyConfiguration.m_transform.m_position.m_x = 3.0;
        ASSERT_TRUE(system.ApplyBodyConfiguration(
            worldHandle,
            secondBodyHandle,
            secondBodyConfiguration));

        ASSERT_TRUE(system.GetConstraintMeasurements(worldHandle, constraintHandle, measurements));
        ASSERT_TRUE(AZStd::holds_alternative<FixedMeasurements>(measurements));
        const FixedMeasurements& resetMeasurements = AZStd::get<FixedMeasurements>(measurements);
        EXPECT_TRUE(resetMeasurements.m_positionImpulse.IsZero());
        EXPECT_TRUE(resetMeasurements.m_rotationImpulse.IsZero());

        EXPECT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, BodyCenterOfMassTransformIncludesShapeOffset)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration childConfiguration;
        childConfiguration.m_geometry = BoxShapeConfiguration{};
        const ShapeHandle childHandle = system.CreateShape(worldHandle, childConfiguration);
        ASSERT_TRUE(childHandle);

        DecoratedShapeConfiguration offsetConfiguration;
        offsetConfiguration.m_geometry = OffsetCenterOfMassShapeConfiguration{
            .m_shapeHandle = childHandle,
            .m_offset = AZ::Vector3::CreateAxisX(0.5f),
        };
        const ShapeHandle offsetHandle = system.CreateShape(worldHandle, offsetConfiguration);
        ASSERT_TRUE(offsetHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = offsetHandle;
        bodyConfiguration.m_transform = {
            .m_position = {
                .m_x = 2.0,
                .m_y = 3.0,
                .m_z = 4.0,
            },
            .m_rotation = AZ::Quaternion::CreateRotationZ(AZ::Constants::HalfPi),
        };
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        WorldTransform centerOfMassTransform;
        ASSERT_TRUE(system.GetBodyCenterOfMassTransform(
            worldHandle,
            bodyHandle,
            centerOfMassTransform));
        EXPECT_NEAR(centerOfMassTransform.m_position.m_x, 2.0, 1.0e-6);
        EXPECT_NEAR(centerOfMassTransform.m_position.m_y, 3.5, 1.0e-6);
        EXPECT_NEAR(centerOfMassTransform.m_position.m_z, 4.0, 1.0e-6);
        EXPECT_TRUE(centerOfMassTransform.m_rotation.IsClose(bodyConfiguration.m_transform.m_rotation));

        EXPECT_FALSE(system.GetBodyCenterOfMassTransform(
            worldHandle,
            BodyHandle::Invalid,
            centerOfMassTransform));

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, offsetHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, childHandle));
    }

    TEST(SimulationTests, WorldRuntimeSettingsAreValidatedAndInvalidateSnapshotsWhenChanged)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        AZ::Vector3 gravity = AZ::Vector3::CreateZero();
        ASSERT_TRUE(system.GetWorldGravity(worldHandle, gravity));
        EXPECT_TRUE(gravity.IsClose(AZ::Vector3(0.0f, 0.0f, -9.81f)));

        const StateSnapshotHandle gravitySnapshot = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(gravitySnapshot);
        EXPECT_TRUE(system.SetWorldGravity(worldHandle, gravity));
        EXPECT_TRUE(system.RestoreWorldState(worldHandle, gravitySnapshot));

        const AZ::Vector3 updatedGravity(0.0f, -4.0f, 0.0f);
        EXPECT_TRUE(system.SetWorldGravity(worldHandle, updatedGravity));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, gravitySnapshot));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, gravitySnapshot));
        ASSERT_TRUE(system.GetWorldGravity(worldHandle, gravity));
        EXPECT_EQ(gravity, updatedGravity);
        EXPECT_FALSE(system.SetWorldGravity(
            worldHandle,
            AZ::Vector3(AZStd::numeric_limits<float>::quiet_NaN())));
        ASSERT_TRUE(system.GetWorldGravity(worldHandle, gravity));
        EXPECT_EQ(gravity, updatedGravity);

        SimulationConfiguration configuration;
        ASSERT_TRUE(system.GetSimulationConfiguration(worldHandle, configuration));
        const StateSnapshotHandle configurationSnapshot = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(configurationSnapshot);
        EXPECT_TRUE(system.UpdateSimulationConfiguration(worldHandle, configuration));
        EXPECT_TRUE(system.RestoreWorldState(worldHandle, configurationSnapshot));

        configuration.m_baumgarte = 0.3f;
        configuration.m_velocityStepCount = 12;
        EXPECT_TRUE(system.UpdateSimulationConfiguration(worldHandle, configuration));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, configurationSnapshot));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, configurationSnapshot));

        SimulationConfiguration queriedConfiguration;
        ASSERT_TRUE(system.GetSimulationConfiguration(worldHandle, queriedConfiguration));
        EXPECT_EQ(queriedConfiguration, configuration);
        configuration.m_baumgarte = AZStd::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(system.UpdateSimulationConfiguration(worldHandle, configuration));
        ASSERT_TRUE(system.GetSimulationConfiguration(worldHandle, queriedConfiguration));
        EXPECT_FLOAT_EQ(queriedConfiguration.m_baumgarte, 0.3f);

        EXPECT_FALSE(system.GetWorldGravity(WorldHandle::Invalid, gravity));
        EXPECT_FALSE(system.GetSimulationConfiguration(
            WorldHandle::Invalid,
            queriedConfiguration));
    }

    TEST(SimulationTests, BulkBodyMembershipIsAtomicAndSupportsExistingConstraints)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_startInSimulation = false;
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        bodyConfiguration.m_transform.m_position.m_z = 2.0;
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);
        ASSERT_TRUE(secondBodyHandle);

        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = firstBodyHandle;
        constraintConfiguration.m_secondBodyHandle = secondBodyHandle;
        constraintConfiguration.m_geometry = PointConstraintConfiguration{};
        const ConstraintHandle constraintHandle = system.CreateConstraint(worldHandle, constraintConfiguration);
        ASSERT_TRUE(constraintHandle);

        const AZStd::array bodyHandles = {firstBodyHandle, secondBodyHandle};
        const AZStd::array duplicateBodyHandles = {firstBodyHandle, firstBodyHandle};
        const AZStd::array invalidBodyHandles = {firstBodyHandle, BodyHandle::Invalid};
        EXPECT_FALSE(system.AddBodiesToSimulation(worldHandle, duplicateBodyHandles, true));
        EXPECT_FALSE(system.AddBodiesToSimulation(worldHandle, invalidBodyHandles, true));
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, firstBodyHandle));
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, secondBodyHandle));

        EXPECT_TRUE(system.AddBodiesToSimulation(
            worldHandle,
            AZStd::span<const BodyHandle>{},
            true));
        ASSERT_TRUE(system.AddBodiesToSimulation(worldHandle, bodyHandles, true));
        EXPECT_TRUE(system.IsBodyInSimulation(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.IsBodyInSimulation(worldHandle, secondBodyHandle));
        EXPECT_FALSE(system.AddBodiesToSimulation(worldHandle, bodyHandles, true));

        EXPECT_FALSE(system.RemoveBodiesFromSimulation(worldHandle, duplicateBodyHandles));
        EXPECT_FALSE(system.RemoveBodiesFromSimulation(worldHandle, invalidBodyHandles));
        EXPECT_TRUE(system.IsBodyInSimulation(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.IsBodyInSimulation(worldHandle, secondBodyHandle));

        EXPECT_TRUE(system.RemoveBodiesFromSimulation(
            worldHandle,
            AZStd::span<const BodyHandle>{}));
        ASSERT_TRUE(system.RemoveBodiesFromSimulation(worldHandle, bodyHandles));
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, firstBodyHandle));
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, secondBodyHandle));
        EXPECT_FALSE(system.RemoveBodiesFromSimulation(worldHandle, bodyHandles));

        EXPECT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, ConstraintSimulationMembershipIsAtomicAndPreservesAllocatedState)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        bodyConfiguration.m_transform.m_position.m_z = 2.0;
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        bodyConfiguration.m_transform.m_position.m_z = 4.0;
        const BodyHandle thirdBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);
        ASSERT_TRUE(secondBodyHandle);
        ASSERT_TRUE(thirdBodyHandle);

        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = firstBodyHandle;
        constraintConfiguration.m_secondBodyHandle = secondBodyHandle;
        constraintConfiguration.m_geometry = PointConstraintConfiguration{};
        constraintConfiguration.m_startInSimulation = false;
        const ConstraintHandle firstConstraintHandle =
            system.CreateConstraint(worldHandle, constraintConfiguration);
        constraintConfiguration.m_firstBodyHandle = secondBodyHandle;
        constraintConfiguration.m_secondBodyHandle = thirdBodyHandle;
        const ConstraintHandle secondConstraintHandle =
            system.CreateConstraint(worldHandle, constraintConfiguration);
        ASSERT_TRUE(firstConstraintHandle);
        ASSERT_TRUE(secondConstraintHandle);
        EXPECT_FALSE(system.IsConstraintInSimulation(worldHandle, firstConstraintHandle));
        EXPECT_FALSE(system.IsConstraintInSimulation(worldHandle, secondConstraintHandle));

        ConstraintState state;
        ASSERT_TRUE(system.GetConstraintState(worldHandle, firstConstraintHandle, state));
        EXPECT_FALSE(state.m_active);
        EXPECT_TRUE(state.m_enabled);
        EXPECT_FALSE(state.m_isInSimulation);
        EXPECT_TRUE(system.SetConstraintEnabled(worldHandle, firstConstraintHandle, false));
        EXPECT_TRUE(system.SetConstraintEnabled(worldHandle, firstConstraintHandle, true));
        ASSERT_TRUE(system.GetConstraintState(worldHandle, firstConstraintHandle, state));
        EXPECT_FALSE(state.m_active);
        EXPECT_TRUE(state.m_enabled);
        EXPECT_FALSE(state.m_isInSimulation);

        ConstraintConfiguration currentConfiguration;
        ASSERT_TRUE(system.GetConstraintConfiguration(
            worldHandle,
            firstConstraintHandle,
            currentConfiguration));
        EXPECT_FALSE(currentConfiguration.m_startInSimulation);

        const StateSnapshotHandle membershipSnapshot = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(membershipSnapshot);
        ASSERT_TRUE(system.AddConstraintToSimulation(worldHandle, firstConstraintHandle));
        EXPECT_TRUE(system.IsConstraintInSimulation(worldHandle, firstConstraintHandle));
        EXPECT_FALSE(system.AddConstraintToSimulation(worldHandle, firstConstraintHandle));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, membershipSnapshot));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, membershipSnapshot));
        ASSERT_TRUE(system.RemoveConstraintFromSimulation(worldHandle, firstConstraintHandle));
        EXPECT_FALSE(system.IsConstraintInSimulation(worldHandle, firstConstraintHandle));
        EXPECT_FALSE(system.RemoveConstraintFromSimulation(worldHandle, firstConstraintHandle));

        const AZStd::array constraintHandles = {firstConstraintHandle, secondConstraintHandle};
        const AZStd::array duplicateConstraintHandles = {firstConstraintHandle, firstConstraintHandle};
        const AZStd::array invalidConstraintHandles = {firstConstraintHandle, ConstraintHandle::Invalid};
        EXPECT_FALSE(system.AddConstraintsToSimulation(worldHandle, duplicateConstraintHandles));
        EXPECT_FALSE(system.AddConstraintsToSimulation(worldHandle, invalidConstraintHandles));
        EXPECT_FALSE(system.IsConstraintInSimulation(worldHandle, firstConstraintHandle));
        EXPECT_FALSE(system.IsConstraintInSimulation(worldHandle, secondConstraintHandle));

        EXPECT_TRUE(system.AddConstraintsToSimulation(
            worldHandle,
            AZStd::span<const ConstraintHandle>{}));
        ASSERT_TRUE(system.AddConstraintsToSimulation(worldHandle, constraintHandles));
        EXPECT_TRUE(system.IsConstraintInSimulation(worldHandle, firstConstraintHandle));
        EXPECT_TRUE(system.IsConstraintInSimulation(worldHandle, secondConstraintHandle));
        EXPECT_FALSE(system.AddConstraintsToSimulation(worldHandle, constraintHandles));

        EXPECT_FALSE(system.RemoveConstraintsFromSimulation(worldHandle, duplicateConstraintHandles));
        EXPECT_FALSE(system.RemoveConstraintsFromSimulation(worldHandle, invalidConstraintHandles));
        EXPECT_TRUE(system.IsConstraintInSimulation(worldHandle, firstConstraintHandle));
        EXPECT_TRUE(system.IsConstraintInSimulation(worldHandle, secondConstraintHandle));

        EXPECT_TRUE(system.RemoveConstraintsFromSimulation(
            worldHandle,
            AZStd::span<const ConstraintHandle>{}));
        ASSERT_TRUE(system.RemoveConstraintsFromSimulation(worldHandle, constraintHandles));
        EXPECT_FALSE(system.IsConstraintInSimulation(worldHandle, firstConstraintHandle));
        EXPECT_FALSE(system.IsConstraintInSimulation(worldHandle, secondConstraintHandle));
        EXPECT_FALSE(system.RemoveConstraintsFromSimulation(worldHandle, constraintHandles));

        EXPECT_FALSE(system.DestroyConstraints(worldHandle, duplicateConstraintHandles));
        EXPECT_FALSE(system.DestroyConstraints(worldHandle, invalidConstraintHandles));
        EXPECT_TRUE(system.IsValid(worldHandle, firstConstraintHandle));
        EXPECT_TRUE(system.IsValid(worldHandle, secondConstraintHandle));
        EXPECT_TRUE(system.DestroyConstraints(
            worldHandle,
            AZStd::span<const ConstraintHandle>{}));
        EXPECT_TRUE(system.DestroyConstraints(worldHandle, constraintHandles));
        EXPECT_FALSE(system.IsValid(worldHandle, firstConstraintHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, secondConstraintHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, thirdBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, BulkBodyDestructionIsAtomicAcrossMixedMembership)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        const BodyHandle addedBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        bodyConfiguration.m_startInSimulation = false;
        bodyConfiguration.m_transform.m_position.m_z = 2.0;
        const BodyHandle unaddedBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(addedBodyHandle);
        ASSERT_TRUE(unaddedBodyHandle);
        ASSERT_TRUE(system.SetBodyMoveEventsEnabled(worldHandle, addedBodyHandle, true));

        const AZStd::array bodyHandles = {addedBodyHandle, unaddedBodyHandle};
        const AZStd::array duplicateBodyHandles = {addedBodyHandle, addedBodyHandle};
        EXPECT_FALSE(system.DestroyBodies(worldHandle, duplicateBodyHandles));
        EXPECT_TRUE(system.IsValid(worldHandle, addedBodyHandle));
        EXPECT_TRUE(system.IsValid(worldHandle, unaddedBodyHandle));

        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = addedBodyHandle;
        constraintConfiguration.m_secondBodyHandle = unaddedBodyHandle;
        constraintConfiguration.m_geometry = PointConstraintConfiguration{};
        const ConstraintHandle constraintHandle = system.CreateConstraint(worldHandle, constraintConfiguration);
        ASSERT_TRUE(constraintHandle);
        EXPECT_FALSE(system.DestroyBodies(worldHandle, bodyHandles));
        EXPECT_TRUE(system.IsValid(worldHandle, addedBodyHandle));
        EXPECT_TRUE(system.IsValid(worldHandle, unaddedBodyHandle));

        ASSERT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        EXPECT_TRUE(system.DestroyBodies(
            worldHandle,
            AZStd::span<const BodyHandle>{}));
        ASSERT_TRUE(system.DestroyBodies(worldHandle, bodyHandles));
        EXPECT_FALSE(system.IsValid(worldHandle, addedBodyHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, unaddedBodyHandle));
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, addedBodyHandle));
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, unaddedBodyHandle));

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_TRUE(system.GetEvents(worldHandle).GetBodyMoves().empty());
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, ContactCallbacksValidateModifyInspectAndRemoveContacts)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        RecordingContactCallbacks callbacks;
        ScopedExtensionRegistration registration(system, &callbacks);
        ASSERT_TRUE(system.SetContactCallbacks(scene.m_worldHandle, registration.GetHandle()));
        for (AZ::u32 step = 0; step < 180 && callbacks.m_persistedCount == 0; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }

        EXPECT_GT(callbacks.m_validationCount, 0);
        EXPECT_GT(callbacks.m_addedCount, 0);
        EXPECT_GT(callbacks.m_persistedCount, 0);
        EXPECT_TRUE(
            (callbacks.m_firstBodyHandle == scene.m_floorBodyHandle
                && callbacks.m_secondBodyHandle == scene.m_sphereBodyHandle)
            || (callbacks.m_firstBodyHandle == scene.m_sphereBodyHandle
                && callbacks.m_secondBodyHandle == scene.m_floorBodyHandle));
        EXPECT_TRUE(
            (callbacks.m_firstShapeHandle == scene.m_floorShapeHandle
                && callbacks.m_secondShapeHandle == scene.m_sphereShapeHandle)
            || (callbacks.m_firstShapeHandle == scene.m_sphereShapeHandle
                && callbacks.m_secondShapeHandle == scene.m_floorShapeHandle));
        EXPECT_TRUE(std::isfinite(callbacks.m_lastPoint.m_positionOnFirstBody.m_z));
        EXPECT_TRUE(std::isfinite(callbacks.m_lastPoint.m_positionOnSecondBody.m_z));
        EXPECT_GE(callbacks.m_initialSettings.m_combinedFriction, 0.0f);
        EXPECT_GE(callbacks.m_initialSettings.m_combinedRestitution, 0.0f);
        EXPECT_TRUE(callbacks.m_estimatedResponse);
        EXPECT_EQ(callbacks.m_responseEstimate.m_impulseCount, 1);
        EXPECT_EQ(callbacks.m_responseEstimate.m_requiredImpulseCount, 1);
        EXPECT_TRUE(callbacks.m_responseEstimate.IsComplete());
        EXPECT_GE(callbacks.m_contactImpulses[0], 0.0f);
        EXPECT_TRUE(callbacks.m_responseEstimate.m_firstLinearVelocity.IsFinite());
        EXPECT_TRUE(callbacks.m_responseEstimate.m_secondLinearVelocity.IsFinite());
        EXPECT_TRUE(callbacks.m_responseEstimate.m_firstTangent.IsFinite());
        EXPECT_TRUE(callbacks.m_responseEstimate.m_secondTangent.IsFinite());
        EXPECT_TRUE(std::isfinite(callbacks.m_responseEstimate.m_frictionPoint.m_z));
        EXPECT_TRUE(system.GetEvents(scene.m_worldHandle).GetContacts().empty());

        WorldTransform separatedTransform;
        separatedTransform.m_position.m_z = 5.0;
        ASSERT_TRUE(system.SetBodyTransform(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle,
            separatedTransform,
            true));
        ASSERT_TRUE(system.SetBodyVelocities(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle,
            AZ::Vector3::CreateZero(),
            AZ::Vector3::CreateZero()));
        for (AZ::u32 step = 0; step < 3 && callbacks.m_removedCount == 0; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }
        EXPECT_GT(callbacks.m_removedCount, 0);
        EXPECT_EQ(callbacks.m_lastRemoved.m_phase, EventPhase::End);
        EXPECT_TRUE(system.GetEvents(scene.m_worldHandle).GetContacts().empty());

        EXPECT_TRUE(system.SetContactCallbacks(scene.m_worldHandle, ExtensionHandle::Invalid));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, ExtensionRegistryTracksIdentityKindDependenciesAndStaleHandles)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        EXPECT_EQ(
            system.RegisterExtension(static_cast<IContactCallbacks*>(nullptr), {}).m_status,
            ExtensionRegistrationStatus::Invalid);

        RecordingContactCallbacks firstCallbacks;
        RecordingContactCallbacks secondCallbacks;
        const ExtensionRegistrationResult firstRegistration = system.RegisterExtension(&firstCallbacks, {});
        const ExtensionRegistrationResult secondRegistration = system.RegisterExtension(&secondCallbacks, {});
        ASSERT_TRUE(firstRegistration);
        ASSERT_TRUE(secondRegistration);
        EXPECT_NE(firstRegistration.m_handle, secondRegistration.m_handle);

        const ExtensionRegistrationResult duplicateRegistration = system.RegisterExtension(&firstCallbacks, {});
        EXPECT_EQ(duplicateRegistration.m_status, ExtensionRegistrationStatus::AlreadyRegistered);
        EXPECT_EQ(duplicateRegistration.m_handle, firstRegistration.m_handle);

        ExtensionInformation information;
        ASSERT_TRUE(system.GetExtensionInformation(firstRegistration.m_handle, information));
        EXPECT_EQ(information.m_id, RecordingContactStateTypeId);
        EXPECT_EQ(information.m_kind, ExtensionKind::ContactCallbacks);
        EXPECT_EQ(information.m_dependentCount, 0);
        EXPECT_FALSE(information.m_hasHostLease);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        EXPECT_FALSE(system.SetSimulationShapeFilter(worldHandle, firstRegistration.m_handle));
        ASSERT_TRUE(system.SetContactCallbacks(worldHandle, firstRegistration.m_handle));
        ASSERT_TRUE(system.GetExtensionInformation(firstRegistration.m_handle, information));
        EXPECT_EQ(information.m_dependentCount, 1);
        EXPECT_EQ(
            system.UnregisterExtension(firstRegistration.m_handle),
            ExtensionRegistrationStatus::InUse);

        ASSERT_TRUE(system.SetContactCallbacks(worldHandle, secondRegistration.m_handle));
        ASSERT_TRUE(system.GetExtensionInformation(firstRegistration.m_handle, information));
        EXPECT_EQ(information.m_dependentCount, 0);
        ASSERT_TRUE(system.GetExtensionInformation(secondRegistration.m_handle, information));
        EXPECT_EQ(information.m_dependentCount, 1);

        EXPECT_EQ(
            system.UnregisterExtension(firstRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
        EXPECT_FALSE(system.GetExtensionInformation(firstRegistration.m_handle, information));
        EXPECT_FALSE(system.SetContactCallbacks(worldHandle, firstRegistration.m_handle));
        EXPECT_EQ(
            system.UnregisterExtension(firstRegistration.m_handle),
            ExtensionRegistrationStatus::NotRegistered);

        EXPECT_TRUE(system.SetContactCallbacks(worldHandle, ExtensionHandle::Invalid));
        EXPECT_EQ(
            system.UnregisterExtension(secondRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
    }

    TEST(SimulationTests, ContactValidationCanRejectAnEntireBodyPair)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        RecordingContactCallbacks callbacks;
        callbacks.m_decision = ContactDecision::RejectAllForPair;
        ScopedExtensionRegistration registration(system, &callbacks);
        ASSERT_TRUE(system.SetContactCallbacks(scene.m_worldHandle, registration.GetHandle()));
        for (AZ::u32 step = 0; step < 120; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }

        BodyState sphereState;
        ASSERT_TRUE(system.GetBodyState(scene.m_worldHandle, scene.m_sphereBodyHandle, sphereState));
        EXPECT_GT(callbacks.m_validationCount, 0);
        EXPECT_EQ(callbacks.m_addedCount, 0);
        EXPECT_LT(sphereState.m_transform.m_position.m_z, -2.0);

        EXPECT_TRUE(system.SetContactCallbacks(scene.m_worldHandle, ExtensionHandle::Invalid));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, MaterialCombineModesControlInitialContactSettings)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        WorldRuntimeConfiguration runtimeConfiguration;
        ASSERT_TRUE(system.GetWorldRuntimeConfiguration(worldHandle, runtimeConfiguration));
        runtimeConfiguration.m_frictionCombineMode = MaterialCombineMode::Average;
        runtimeConfiguration.m_restitutionCombineMode = MaterialCombineMode::Average;
        ASSERT_TRUE(system.UpdateWorldRuntimeConfiguration(worldHandle, runtimeConfiguration));

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        BodyRuntimeConfiguration sphereConfiguration;
        sphereConfiguration.m_friction = 0.8f;
        sphereConfiguration.m_restitution = 0.8f;
        ASSERT_TRUE(system.UpdateBodyRuntimeConfiguration(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle,
            sphereConfiguration,
            true));

        RecordingContactCallbacks callbacks;
        ScopedExtensionRegistration registration(system, &callbacks);
        ASSERT_TRUE(system.SetContactCallbacks(scene.m_worldHandle, registration.GetHandle()));
        for (AZ::u32 step = 0; step < 180 && callbacks.m_addedCount == 0; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }

        ASSERT_GT(callbacks.m_addedCount, 0);
        EXPECT_NEAR(callbacks.m_initialSettings.m_combinedFriction, 0.5f, 1.0e-6f);
        EXPECT_NEAR(callbacks.m_initialSettings.m_combinedRestitution, 0.4f, 1.0e-6f);

        EXPECT_TRUE(system.SetContactCallbacks(scene.m_worldHandle, ExtensionHandle::Invalid));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, WorldRuntimeConfigurationControlsAutomaticSteppingAndEnabledState)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);
        WorldRuntimeConfiguration configuration;
        ASSERT_TRUE(system.GetWorldRuntimeConfiguration(worldHandle, configuration));
        configuration.m_fixedTimeStep = 1.0f / 120.0f;
        configuration.m_collisionStepCount = 2;
        configuration.m_maximumCatchUpSteps = 2;
        configuration.m_autoSimulate = false;
        ASSERT_TRUE(system.UpdateWorldRuntimeConfiguration(worldHandle, configuration));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, snapshotHandle));

        RecordingStepListener listener;
        ScopedExtensionRegistration registration(system, &listener);
        ASSERT_TRUE(system.AddStepListener(worldHandle, registration.GetHandle()));
        EXPECT_TRUE(system.StepAutoSimulatedWorlds(1.0f / 60.0f));
        EXPECT_EQ(listener.m_callCount, 0);

        configuration.m_autoSimulate = true;
        ASSERT_TRUE(system.UpdateWorldRuntimeConfiguration(worldHandle, configuration));
        EXPECT_TRUE(system.StepAutoSimulatedWorlds(1.0f / 60.0f));
        EXPECT_EQ(listener.m_callCount, 4);

        configuration.m_enabled = false;
        ASSERT_TRUE(system.UpdateWorldRuntimeConfiguration(worldHandle, configuration));
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_EQ(listener.m_callCount, 4);

        WorldRuntimeConfiguration returnedConfiguration;
        ASSERT_TRUE(system.GetWorldRuntimeConfiguration(worldHandle, returnedConfiguration));
        EXPECT_EQ(returnedConfiguration, configuration);

        WorldRuntimeConfiguration invalidConfiguration = configuration;
        invalidConfiguration.m_fixedTimeStep = 0.0f;
        EXPECT_FALSE(system.UpdateWorldRuntimeConfiguration(worldHandle, invalidConfiguration));
        invalidConfiguration = configuration;
        invalidConfiguration.m_frictionCombineMode = MaterialCombineMode::None;
        EXPECT_FALSE(system.UpdateWorldRuntimeConfiguration(worldHandle, invalidConfiguration));
        invalidConfiguration = configuration;
        invalidConfiguration.m_workerCount = 0;
        EXPECT_FALSE(system.UpdateWorldRuntimeConfiguration(worldHandle, invalidConfiguration));

        EXPECT_TRUE(system.RemoveStepListener(worldHandle, registration.GetHandle()));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
    }

    TEST(SimulationTests, RuntimeWorkerCountPreservesSnapshotsAndDeterministicState)
    {
        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(8);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);

        Runtime system(CreateSerialSystemConfiguration(), &jobContext);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(scene.m_worldHandle);
        ASSERT_TRUE(snapshotHandle);

        constexpr AZStd::array workerCounts = {AZ::u32{1}, AZ::u32{4}, AZ::u32{8}};
        AZStd::array<WorldStateDigest, workerCounts.size()> digests;
        WorldRuntimeConfiguration configuration;
        ASSERT_TRUE(system.GetWorldRuntimeConfiguration(scene.m_worldHandle, configuration));
        for (size_t workerIndex = 0; workerIndex < workerCounts.size(); ++workerIndex)
        {
            configuration.m_workerCount = workerCounts[workerIndex];
            ASSERT_TRUE(system.UpdateWorldRuntimeConfiguration(scene.m_worldHandle, configuration));
            ASSERT_TRUE(system.RestoreWorldState(scene.m_worldHandle, snapshotHandle));
            bool scheduledParallelJob = false;
            for (AZ::u32 step = 0; step < 180; ++step)
            {
                ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
                WorldStatistics statistics;
                ASSERT_TRUE(system.GetWorldStatistics(scene.m_worldHandle, statistics));
                scheduledParallelJob = scheduledParallelJob
                    || statistics.m_lastUpdateJobCount > 0;
            }

            ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, digests[workerIndex]));
            if (workerCounts[workerIndex] == 1)
            {
                EXPECT_FALSE(scheduledParallelJob);
            }
            else
            {
                EXPECT_TRUE(scheduledParallelJob);
            }
        }

        EXPECT_EQ(digests[0], digests[1]);
        EXPECT_EQ(digests[0], digests[2]);

        WorldRuntimeConfiguration returnedConfiguration;
        ASSERT_TRUE(system.GetWorldRuntimeConfiguration(scene.m_worldHandle, returnedConfiguration));
        EXPECT_EQ(returnedConfiguration.m_workerCount, 8);

        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, snapshotHandle));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, BodyPairColliderSupportsEveryNativeCollisionMode)
    {
        constexpr AZStd::array operations = {
            BodyCollisionOperation::Any,
            BodyCollisionOperation::AnyPerLeaf,
            BodyCollisionOperation::Deepest,
            BodyCollisionOperation::DeepestPerLeaf,
            BodyCollisionOperation::Default,
        };
        for (const BodyCollisionOperation operation : operations)
        {
            Runtime system(CreateSerialSystemConfiguration(), nullptr);
            ASSERT_TRUE(system);

            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
            ASSERT_TRUE(snapshotHandle);
            RecordingBodyPairCollider collider;
            collider.m_operation = operation;
            ScopedExtensionRegistration registration(system, &collider);
            ASSERT_TRUE(system.SetBodyPairCollider(worldHandle, registration.GetHandle()));
            EXPECT_FALSE(system.RestoreWorldState(worldHandle, snapshotHandle));

            const SphereOnFloor scene = CreateSphereOnFloor(system);
            for (AZ::u32 step = 0; step < 180; ++step)
            {
                ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
            }

            BodyState state;
            ASSERT_TRUE(system.GetBodyState(worldHandle, scene.m_sphereBodyHandle, state));
            EXPECT_GT(collider.m_callCount, 0);
            EXPECT_TRUE(collider.m_operationSucceeded);
            EXPECT_FALSE(collider.m_sawEarlyOut);
            EXPECT_EQ(collider.m_lastInput.m_first.m_bodyHandle, scene.m_sphereBodyHandle);
            EXPECT_EQ(collider.m_lastInput.m_first.m_shapeHandle, scene.m_sphereShapeHandle);
            EXPECT_EQ(collider.m_lastInput.m_first.m_motionType, MotionType::Dynamic);
            EXPECT_EQ(collider.m_lastInput.m_second.m_bodyHandle, scene.m_floorBodyHandle);
            EXPECT_EQ(collider.m_lastInput.m_second.m_shapeHandle, scene.m_floorShapeHandle);
            EXPECT_EQ(collider.m_lastInput.m_second.m_motionType, MotionType::Static);
            EXPECT_GT(collider.m_lastSettings.m_collisionTolerance, 0.0f);
            EXPECT_GT(collider.m_lastSettings.m_penetrationTolerance, 0.0f);
            EXPECT_GT(state.m_transform.m_position.m_z, 0.4);

            EXPECT_TRUE(system.SetBodyPairCollider(worldHandle, ExtensionHandle::Invalid));
            EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
            DestroySphereOnFloor(system, scene);
        }
    }

    TEST(SimulationTests, BodyPairColliderCanRejectBodyPairs)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RecordingBodyPairCollider collider;
        ScopedExtensionRegistration registration(system, &collider);
        ASSERT_TRUE(system.SetBodyPairCollider(worldHandle, registration.GetHandle()));
        const SphereOnFloor scene = CreateSphereOnFloor(system);
        for (AZ::u32 step = 0; step < 120; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, scene.m_sphereBodyHandle, state));
        EXPECT_GT(collider.m_callCount, 0);
        EXPECT_LT(state.m_transform.m_position.m_z, -2.0);

        EXPECT_TRUE(system.SetBodyPairCollider(worldHandle, ExtensionHandle::Invalid));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, BodyPairColliderCanSubmitCustomContactHits)
    {
        SystemConfiguration configuration = CreateSerialSystemConfiguration();
        configuration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(configuration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        RecordingBodyPairCollider collider;
        collider.m_operation = BodyCollisionOperation::CustomHit;
        ScopedExtensionRegistration colliderRegistration(system, &collider);
        ASSERT_TRUE(system.SetBodyPairCollider(worldHandle, colliderRegistration.GetHandle()));
        RecordingContactCallbacks contactCallbacks;
        ScopedExtensionRegistration contactRegistration(system, &contactCallbacks);
        ASSERT_TRUE(system.SetContactCallbacks(worldHandle, contactRegistration.GetHandle()));

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration staticBodyConfiguration;
        staticBodyConfiguration.m_shapeHandle = shapeHandle;
        staticBodyConfiguration.m_motionType = MotionType::Static;
        staticBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        const BodyHandle staticBodyHandle = system.CreateBody(worldHandle, staticBodyConfiguration);
        ASSERT_TRUE(staticBodyHandle);

        BodyConfiguration dynamicBodyConfiguration;
        dynamicBodyConfiguration.m_shapeHandle = shapeHandle;
        dynamicBodyConfiguration.m_transform.m_position.m_z = 0.75;
        const BodyHandle dynamicBodyHandle = system.CreateBody(worldHandle, dynamicBodyConfiguration);
        ASSERT_TRUE(dynamicBodyHandle);

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_GT(collider.m_callCount, 0);
        EXPECT_TRUE(collider.m_operationSucceeded);
        EXPECT_GT(contactCallbacks.m_addedCount, 0);
        EXPECT_TRUE(
            (contactCallbacks.m_firstBodyHandle == dynamicBodyHandle
                && contactCallbacks.m_secondBodyHandle == staticBodyHandle)
            || (contactCallbacks.m_firstBodyHandle == staticBodyHandle
                && contactCallbacks.m_secondBodyHandle == dynamicBodyHandle));

        EXPECT_TRUE(system.SetContactCallbacks(worldHandle, ExtensionHandle::Invalid));
        EXPECT_TRUE(system.SetBodyPairCollider(worldHandle, ExtensionHandle::Invalid));
        EXPECT_TRUE(system.DestroyBody(worldHandle, dynamicBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, staticBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, BodyPairColliderRejectsInvalidSettingsAndHits)
    {
        SystemConfiguration configuration = CreateSerialSystemConfiguration();
        configuration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(configuration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        InvalidBodyPairCollider collider;
        ScopedExtensionRegistration registration(system, &collider);
        ASSERT_TRUE(system.SetBodyPairCollider(worldHandle, registration.GetHandle()));

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration staticBodyConfiguration;
        staticBodyConfiguration.m_shapeHandle = shapeHandle;
        staticBodyConfiguration.m_motionType = MotionType::Static;
        staticBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        const BodyHandle staticBodyHandle = system.CreateBody(worldHandle, staticBodyConfiguration);
        ASSERT_TRUE(staticBodyHandle);

        BodyConfiguration dynamicBodyConfiguration;
        dynamicBodyConfiguration.m_shapeHandle = shapeHandle;
        dynamicBodyConfiguration.m_transform.m_position.m_z = 0.75;
        const BodyHandle dynamicBodyHandle = system.CreateBody(worldHandle, dynamicBodyConfiguration);
        ASSERT_TRUE(dynamicBodyHandle);

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_GT(collider.m_callCount, 0);
        EXPECT_TRUE(collider.m_invalidHitRejected);
        EXPECT_TRUE(collider.m_invalidSettingsRejected);
        EXPECT_FALSE(system.WereBodiesInContact(worldHandle, dynamicBodyHandle, staticBodyHandle));

        EXPECT_TRUE(system.SetBodyPairCollider(worldHandle, ExtensionHandle::Invalid));
        EXPECT_TRUE(system.DestroyBody(worldHandle, dynamicBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, staticBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, SimulationShapeFilterReceivesProviderShapeIdentity)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        RecordingSimulationShapeFilter filter;
        filter.m_rejectedBodyHandle = scene.m_sphereBodyHandle;
        ScopedExtensionRegistration registration(system, &filter);
        ASSERT_TRUE(system.SetSimulationShapeFilter(scene.m_worldHandle, registration.GetHandle()));
        for (AZ::u32 step = 0; step < 120; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }

        BodyState sphereState;
        ASSERT_TRUE(system.GetBodyState(scene.m_worldHandle, scene.m_sphereBodyHandle, sphereState));
        EXPECT_GT(filter.m_callCount, 0);
        EXPECT_TRUE(
            (filter.m_lastFirstShape.m_rootShapeHandle == scene.m_floorShapeHandle
                && filter.m_lastSecondShape.m_rootShapeHandle == scene.m_sphereShapeHandle)
            || (filter.m_lastFirstShape.m_rootShapeHandle == scene.m_sphereShapeHandle
                && filter.m_lastSecondShape.m_rootShapeHandle == scene.m_floorShapeHandle));
        EXPECT_TRUE(
            (filter.m_lastFirstShape.m_kind == ShapeKind::Box
                && filter.m_lastSecondShape.m_kind == ShapeKind::Sphere)
            || (filter.m_lastFirstShape.m_kind == ShapeKind::Sphere
                && filter.m_lastSecondShape.m_kind == ShapeKind::Box));
        EXPECT_LT(sphereState.m_transform.m_position.m_z, -2.0);

        EXPECT_TRUE(system.SetSimulationShapeFilter(scene.m_worldHandle, ExtensionHandle::Invalid));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, StepListenerUsesNoLockMutationContextForEveryCollisionStep)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        WorldRuntimeConfiguration runtimeConfiguration;
        ASSERT_TRUE(system.GetWorldRuntimeConfiguration(worldHandle, runtimeConfiguration));
        runtimeConfiguration.m_collisionStepCount = 2;
        ASSERT_TRUE(system.UpdateWorldRuntimeConfiguration(worldHandle, runtimeConfiguration));

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration firstBodyConfiguration;
        firstBodyConfiguration.m_shapeHandle = shapeHandle;
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, firstBodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);

        BodyConfiguration secondBodyConfiguration;
        secondBodyConfiguration.m_shapeHandle = shapeHandle;
        secondBodyConfiguration.m_transform.m_position.m_z = 2.0;
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, secondBodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        FixedConstraintConfiguration fixed;
        fixed.m_autoDetectPoint = true;
        fixed.m_space = ConstraintSpace::World;
        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = firstBodyHandle;
        constraintConfiguration.m_secondBodyHandle = secondBodyHandle;
        constraintConfiguration.m_geometry = fixed;
        constraintConfiguration.m_userData = 0x0fed'cba9'8765'4321;
        const ConstraintHandle constraintHandle = system.CreateConstraint(worldHandle, constraintConfiguration);
        ASSERT_TRUE(constraintHandle);

        RecordingStepListener listener;
        listener.m_bodyHandle = firstBodyHandle;
        listener.m_constraintHandle = constraintHandle;
        RecordingStepListener secondListener;
        secondListener.m_bodyHandle = firstBodyHandle;
        secondListener.m_constraintHandle = constraintHandle;
        RecordingStepListener mutationTarget;
        MutatingStepListener mutatingListener;
        mutatingListener.m_system = &system;
        mutatingListener.m_worldHandle = worldHandle;
        ScopedExtensionRegistration listenerRegistration(system, &listener);
        ScopedExtensionRegistration secondListenerRegistration(system, &secondListener);
        ScopedExtensionRegistration mutationTargetRegistration(system, &mutationTarget);
        ScopedExtensionRegistration mutatingListenerRegistration(system, &mutatingListener);
        mutatingListener.m_targetListenerHandle = mutationTargetRegistration.GetHandle();
        mutatingListener.m_selfHandle = mutatingListenerRegistration.GetHandle();
        ASSERT_TRUE(system.AddStepListener(worldHandle, listenerRegistration.GetHandle()));
        ASSERT_TRUE(system.AddStepListener(worldHandle, secondListenerRegistration.GetHandle()));
        ASSERT_TRUE(system.AddStepListener(worldHandle, mutatingListenerRegistration.GetHandle()));
        EXPECT_FALSE(system.AddStepListener(worldHandle, listenerRegistration.GetHandle()));
        EXPECT_FALSE(system.AddStepListener(worldHandle, ExtensionHandle::Invalid));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_TRUE(system.RemoveStepListener(worldHandle, listenerRegistration.GetHandle()));
        EXPECT_TRUE(system.RemoveStepListener(worldHandle, secondListenerRegistration.GetHandle()));
        EXPECT_TRUE(system.RemoveStepListener(worldHandle, mutatingListenerRegistration.GetHandle()));
        EXPECT_FALSE(system.RemoveStepListener(worldHandle, secondListenerRegistration.GetHandle()));
        EXPECT_FALSE(system.RemoveStepListener(worldHandle, ExtensionHandle::Invalid));

        EXPECT_EQ(listener.m_callCount, 2);
        EXPECT_EQ(secondListener.m_callCount, 2);
        EXPECT_EQ(mutatingListener.m_attemptCount, 2);
        EXPECT_TRUE(mutatingListener.m_addRejected);
        EXPECT_TRUE(mutatingListener.m_removeRejected);
        EXPECT_EQ(mutationTarget.m_callCount, 0);
        EXPECT_TRUE(listener.m_sawFirst);
        EXPECT_TRUE(listener.m_sawLast);
        EXPECT_TRUE(listener.m_readState);
        EXPECT_TRUE(listener.m_setVelocities);
        EXPECT_TRUE(listener.m_addedForce);
        EXPECT_TRUE(listener.m_addedImpulse);
        EXPECT_TRUE(listener.m_disabledConstraint);
        EXPECT_NEAR(listener.m_lastDeltaTime, 1.0f / 120.0f, 1.0e-6f);

        BodyState bodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, firstBodyHandle, bodyState));
        EXPECT_GT(bodyState.m_transform.m_position.m_x, 0.0);
        EXPECT_NEAR(bodyState.m_linearVelocity.GetX(), 3.0f, 0.02f);
        ConstraintState constraintState;
        ASSERT_TRUE(system.GetConstraintState(worldHandle, constraintHandle, constraintState));
        EXPECT_FALSE(constraintState.m_enabled);

        EXPECT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, WorldRuntimeConfigurationTogglesEventCollection)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        WorldRuntimeConfiguration configuration;
        ASSERT_TRUE(system.GetWorldRuntimeConfiguration(worldHandle, configuration));
        configuration.m_collectActivationEvents = true;
        configuration.m_collectContactEvents = true;
        ASSERT_TRUE(system.UpdateWorldRuntimeConfiguration(worldHandle, configuration));

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_FALSE(system.GetEvents(worldHandle).GetActivations().empty());

        bool foundContact = false;
        for (AZ::u32 step = 0; step < 180 && !foundContact; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
            foundContact = !system.GetEvents(worldHandle).GetContacts().empty();
        }
        EXPECT_TRUE(foundContact);

        configuration.m_collectActivationEvents = false;
        configuration.m_collectContactEvents = false;
        ASSERT_TRUE(system.UpdateWorldRuntimeConfiguration(worldHandle, configuration));
        EXPECT_TRUE(system.GetEvents(worldHandle).GetActivations().empty());
        EXPECT_TRUE(system.GetEvents(worldHandle).GetContacts().empty());

        ASSERT_TRUE(system.DeactivateBody(worldHandle, scene.m_sphereBodyHandle));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_TRUE(system.GetEvents(worldHandle).GetActivations().empty());
        EXPECT_TRUE(system.GetEvents(worldHandle).GetContacts().empty());
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, ConfiguredObjectLayerTableControlsCollisionPairs)
    {
        SystemConfiguration configuration = CreateSerialSystemConfiguration();
        configuration.m_defaultWorld.m_objectLayers.push_back({
            .m_name = AZ::Name::FromStringLiteral("Ghost", nullptr),
            .m_broadPhaseLayer = DefaultBroadPhaseLayers::Moving,
        });
        Runtime system(configuration, nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_sphereBodyHandle);
        const ObjectLayer ghostLayer{3};
        EXPECT_TRUE(system.SetBodyObjectLayer(scene.m_worldHandle, scene.m_sphereBodyHandle, ghostLayer));
        EXPECT_FALSE(system.SetBodyObjectLayer(scene.m_worldHandle, scene.m_sphereBodyHandle, ObjectLayer{4}));

        for (AZ::u32 step = 0; step < 90; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }

        BodyState sphereState;
        ASSERT_TRUE(system.GetBodyState(scene.m_worldHandle, scene.m_sphereBodyHandle, sphereState));
        EXPECT_LT(sphereState.m_transform.m_position.m_z, -2.0);
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, GroupFilterTableControlsBodyPairsAndOwnsItsLifetime)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        systemConfiguration.m_defaultWorld.m_collectContactEvents = true;
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const CollisionSubGroupId firstSubGroup{0};
        const CollisionSubGroupId secondSubGroup{1};
        GroupFilterTableConfiguration filterConfiguration;
        filterConfiguration.m_subGroupCount = 2;
        filterConfiguration.m_disabledPairs = {{firstSubGroup, secondSubGroup}};
        const GroupFilterHandle filterHandle = system.CreateGroupFilterTable(filterConfiguration);
        ASSERT_TRUE(filterHandle);

        bool collisionEnabled = true;
        ASSERT_TRUE(system.GetSubGroupCollisionEnabled(
            filterHandle,
            firstSubGroup,
            secondSubGroup,
            collisionEnabled));
        EXPECT_FALSE(collisionEnabled);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_collisionGroup = {
            .m_filterHandle = filterHandle,
            .m_groupId = CollisionGroupId{7},
            .m_subGroupId = firstSubGroup,
        };
        bodyConfiguration.m_motionType = MotionType::Static;
        bodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        const BodyHandle staticBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(staticBodyHandle);

        bodyConfiguration.m_collisionGroup.m_subGroupId = secondSubGroup;
        bodyConfiguration.m_motionType = MotionType::Dynamic;
        bodyConfiguration.m_objectLayer = DefaultLayers::Moving;
        const BodyHandle dynamicBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(dynamicBodyHandle);
        EXPECT_FALSE(system.DestroyGroupFilter(filterHandle));

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_TRUE(system.GetEvents(worldHandle).GetContacts().empty());

        ASSERT_TRUE(system.DeactivateBody(worldHandle, dynamicBodyHandle));
        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, dynamicBodyHandle, state));
        EXPECT_FALSE(state.m_isActive);
        ASSERT_TRUE(system.SetSubGroupCollisionEnabled(
            filterHandle,
            firstSubGroup,
            secondSubGroup,
            true));
        ASSERT_TRUE(system.GetBodyState(worldHandle, dynamicBodyHandle, state));
        EXPECT_TRUE(state.m_isActive);
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        bool foundBegin = false;
        for (const ContactEvent& contact : system.GetEvents(worldHandle).GetContacts())
        {
            if (contact.m_phase == EventPhase::Begin)
            {
                foundBegin = true;
            }
        }
        EXPECT_TRUE(foundBegin);

        ASSERT_TRUE(system.GetBodyState(worldHandle, dynamicBodyHandle, state));
        EXPECT_EQ(state.m_collisionGroup.m_filterHandle, filterHandle);
        EXPECT_EQ(state.m_collisionGroup.m_groupId, CollisionGroupId{7});
        EXPECT_EQ(state.m_collisionGroup.m_subGroupId, secondSubGroup);

        WorldStateDigest enabledDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, enabledDigest));
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);

        ASSERT_TRUE(system.DeactivateBody(worldHandle, dynamicBodyHandle));
        ASSERT_TRUE(system.GetBodyState(worldHandle, dynamicBodyHandle, state));
        EXPECT_FALSE(state.m_isActive);
        ASSERT_TRUE(system.SetSubGroupCollisionEnabled(
            filterHandle,
            firstSubGroup,
            secondSubGroup,
            false));
        ASSERT_TRUE(system.GetBodyState(worldHandle, dynamicBodyHandle, state));
        EXPECT_TRUE(state.m_isActive);
        WorldStateDigest disabledDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, disabledDigest));
        EXPECT_NE(disabledDigest, enabledDigest);
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, snapshotHandle));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        bool foundEnd = false;
        for (const ContactEvent& contact : system.GetEvents(worldHandle).GetContacts())
        {
            if (contact.m_phase == EventPhase::End)
            {
                foundEnd = true;
            }
        }
        EXPECT_TRUE(foundEnd);

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, dynamicBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, staticBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyGroupFilter(filterHandle));
    }

    TEST(SimulationTests, CustomGroupFilterRestoresContactsAndCallbackState)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        systemConfiguration.m_defaultWorld.m_collectContactEvents = true;
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        TestGroupFilter filter;
        EXPECT_FALSE(system.CreateGroupFilter(2, ExtensionHandle::Invalid));
        ScopedExtensionRegistration registration(system, &filter);
        const GroupFilterHandle filterHandle = system.CreateGroupFilter(2, registration.GetHandle());
        ASSERT_TRUE(filterHandle);
        EXPECT_FALSE(system.NotifyGroupFilterChanged(GroupFilterHandle::Invalid));

        const CollisionSubGroupId firstSubGroup{0};
        const CollisionSubGroupId secondSubGroup{1};
        bool collisionEnabled = false;
        EXPECT_FALSE(system.GetSubGroupCollisionEnabled(
            filterHandle,
            firstSubGroup,
            secondSubGroup,
            collisionEnabled));
        EXPECT_FALSE(system.SetSubGroupCollisionEnabled(
            filterHandle,
            firstSubGroup,
            secondSubGroup,
            true));

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_collisionGroup = {
            .m_filterHandle = filterHandle,
            .m_groupId = CollisionGroupId{7},
            .m_subGroupId = firstSubGroup,
        };
        bodyConfiguration.m_motionType = MotionType::Static;
        bodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        const BodyHandle staticBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(staticBodyHandle);

        bodyConfiguration.m_collisionGroup.m_subGroupId = secondSubGroup;
        bodyConfiguration.m_motionType = MotionType::Dynamic;
        bodyConfiguration.m_objectLayer = DefaultLayers::Moving;
        const BodyHandle dynamicBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(dynamicBodyHandle);
        EXPECT_FALSE(system.DestroyGroupFilter(filterHandle));

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_TRUE(system.GetEvents(worldHandle).GetContacts().empty());

        ASSERT_TRUE(system.DeactivateBody(worldHandle, dynamicBodyHandle));
        filter.m_collisionEnabled = true;
        filter.m_stateHash = 2;
        ASSERT_TRUE(system.NotifyGroupFilterChanged(filterHandle));

        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, dynamicBodyHandle, state));
        EXPECT_TRUE(state.m_isActive);
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        bool foundBegin = false;
        for (const ContactEvent& contact : system.GetEvents(worldHandle).GetContacts())
        {
            if (contact.m_phase == EventPhase::Begin)
            {
                foundBegin = true;
            }
        }
        EXPECT_TRUE(foundBegin);

        WorldStateDigest enabledDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, enabledDigest));
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);

        ASSERT_TRUE(system.DeactivateBody(worldHandle, dynamicBodyHandle));
        filter.m_collisionEnabled = false;
        filter.m_stateHash = 3;
        ASSERT_TRUE(system.NotifyGroupFilterChanged(filterHandle));
        ASSERT_TRUE(system.GetBodyState(worldHandle, dynamicBodyHandle, state));
        EXPECT_TRUE(state.m_isActive);

        WorldStateDigest disabledDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, disabledDigest));
        EXPECT_NE(disabledDigest, enabledDigest);
        EXPECT_TRUE(system.RestoreWorldState(worldHandle, snapshotHandle));
        EXPECT_TRUE(filter.m_collisionEnabled);
        EXPECT_EQ(filter.m_stateHash, 2);
        EXPECT_TRUE(system.WereBodiesInContact(
            worldHandle,
            staticBodyHandle,
            dynamicBodyHandle));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        bool foundEnd = false;
        for (const ContactEvent& contact : system.GetEvents(worldHandle).GetContacts())
        {
            if (contact.m_phase == EventPhase::End)
            {
                foundEnd = true;
            }
        }
        EXPECT_FALSE(foundEnd);

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, dynamicBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, staticBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyGroupFilter(filterHandle));
    }

    TEST(SimulationTests, AutoSimulationUsesFixedStepsAndBoundsCatchUp)
    {
        SystemConfiguration configuration = CreateSerialSystemConfiguration();
        configuration.m_defaultWorld.m_fixedTimeStep = 0.1f;
        configuration.m_defaultWorld.m_gravity = AZ::Vector3::CreateAxisZ(-10.0f);
        configuration.m_defaultWorld.m_maximumCatchUpSteps = 2;
        Runtime system(configuration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_transform.m_position.m_z = 10.0;
        bodyConfiguration.m_linearDamping = 0.0f;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        SimulationResult simulationResult = system.StepAutoSimulatedWorldsDetailed(1.05f);
        ASSERT_TRUE(simulationResult);
        EXPECT_EQ(simulationResult.m_stepCount, 2);
        EXPECT_GT(simulationResult.m_updateNanoseconds, 0);
        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_NEAR(state.m_linearVelocity.GetZ(), -2.0f, 1.0e-4f);

        simulationResult = system.StepAutoSimulatedWorldsDetailed(0.04f);
        ASSERT_TRUE(simulationResult);
        EXPECT_EQ(simulationResult.m_stepCount, 0);
        BodyState unchangedState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, unchangedState));
        EXPECT_NEAR(unchangedState.m_linearVelocity.GetZ(), state.m_linearVelocity.GetZ(), 1.0e-4f);

        simulationResult = system.StepAutoSimulatedWorldsDetailed(0.02f);
        ASSERT_TRUE(simulationResult);
        EXPECT_EQ(simulationResult.m_stepCount, 1);
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_NEAR(state.m_linearVelocity.GetZ(), -3.0f, 1.0e-4f);

        const SimulationResult invalidResult = system.StepWorldDetailed(worldHandle, 0.0f);
        EXPECT_FALSE(invalidResult);
        EXPECT_EQ(
            invalidResult.m_errors & SimulationError::InvalidRequest,
            SimulationError::InvalidRequest);

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, HandlesCannotCrossWorldBoundaries)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle firstWorldHandle = system.GetDefaultWorldHandle();
        const WorldHandle secondWorldHandle = system.CreateWorld(WorldConfiguration{});
        ASSERT_TRUE(firstWorldHandle);
        ASSERT_TRUE(secondWorldHandle);

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle firstShapeHandle = system.CreateShape(firstWorldHandle, shapeConfiguration);
        ASSERT_TRUE(firstShapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = firstShapeHandle;
        EXPECT_FALSE(system.CreateBody(secondWorldHandle, bodyConfiguration));
        EXPECT_FALSE(system.DestroyShape(secondWorldHandle, firstShapeHandle));

        EXPECT_TRUE(system.DestroyShape(firstWorldHandle, firstShapeHandle));
        EXPECT_TRUE(system.DestroyWorld(secondWorldHandle));
    }

    TEST(SimulationTests, SimulationConfigurationIsValidatedAndDeterministicByConstruction)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        WorldConfiguration worldConfiguration;
        worldConfiguration.m_simulation.m_positionStepCount = 0;
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(system.CreateWorld(worldConfiguration));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        worldConfiguration.m_simulation.m_positionStepCount = 2;
        worldConfiguration.m_capacity.m_maxBodies = AZStd::numeric_limits<AZ::u32>::max();
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(system.CreateWorld(worldConfiguration));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        worldConfiguration.m_capacity.m_maxBodies = 65'536;
        worldConfiguration.m_capacity.m_bodyMutexCount = 3;
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(system.CreateWorld(worldConfiguration));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        worldConfiguration.m_capacity.m_bodyMutexCount = 0;
        worldConfiguration.m_capacity.m_maxBodyPairs = AZStd::numeric_limits<AZ::u32>::max();
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(system.CreateWorld(worldConfiguration));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        worldConfiguration.m_capacity.m_maxBodyPairs = 65'536;
        worldConfiguration.m_capacity.m_maxContactConstraints =
            AZStd::numeric_limits<AZ::u32>::max();
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(system.CreateWorld(worldConfiguration));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        worldConfiguration.m_capacity.m_maxContactConstraints = 10'240;
        worldConfiguration.m_simulation.m_positionStepCount = 4;
        worldConfiguration.m_simulation.m_velocityStepCount = 12;
        worldConfiguration.m_simulation.m_bodyPairCacheMaximumDeltaRotation = AZ::Constants::HalfPi;
        const WorldHandle worldHandle = system.CreateWorld(worldConfiguration);
        ASSERT_TRUE(worldHandle);
        EXPECT_TRUE(system.DestroyWorld(worldHandle));
    }

    TEST(SimulationTests, DestroyWorldRejectsLiveResourcesWithoutInvalidatingThem)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.CreateWorld(WorldConfiguration{});
        ASSERT_TRUE(worldHandle);

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        EXPECT_FALSE(system.DestroyWorld(worldHandle));
        EXPECT_TRUE(system.IsValid(worldHandle, shapeHandle));

        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyWorld(worldHandle));
    }

    TEST(SimulationTests, CreatesEveryAnalyticAndConvexPrimitive)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ASSERT_TRUE(worldHandle);

        ConvexHullShapeConfiguration convexHull;
        convexHull.m_points = {
            AZ::Vector3(-1.0f, -1.0f, -1.0f),
            AZ::Vector3(-1.0f, 1.0f, 1.0f),
            AZ::Vector3(1.0f, -1.0f, 1.0f),
            AZ::Vector3(1.0f, 1.0f, -1.0f),
        };

        MeshShapeConfiguration mesh;
        mesh.m_vertices = {
            AZ::Vector3(-1.0f, -1.0f, 0.0f),
            AZ::Vector3(1.0f, -1.0f, 0.0f),
            AZ::Vector3(0.0f, 1.0f, 0.0f),
        };
        mesh.m_triangles = {MeshTriangle{
            .m_firstVertex = 0,
            .m_secondVertex = 1,
            .m_thirdVertex = 2,
        }};

        const AZStd::vector<ShapeGeometry> geometries = {
            BoxShapeConfiguration{},
            CapsuleShapeConfiguration{},
            convexHull,
            CylinderShapeConfiguration{},
            EmptyShapeConfiguration{},
            mesh,
            PlaneShapeConfiguration{},
            SphereShapeConfiguration{},
            TaperedCapsuleShapeConfiguration{},
            TaperedCylinderShapeConfiguration{},
            TriangleShapeConfiguration{},
        };

        for (const ShapeGeometry& geometry : geometries)
        {
            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = geometry;
            const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
            ASSERT_TRUE(shapeHandle);
            EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        }
    }

    TEST(SimulationTests, PrimitiveShapeStateReturnsExactNativeValues)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const MaterialHandle materialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(materialHandle);
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        const auto verifyState = [&]<class Geometry>(const Geometry& expected)
        {
            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = expected;
            shapeConfiguration.m_density = 37.0f;
            shapeConfiguration.m_userData = 91;
            if constexpr (!AZStd::is_same_v<Geometry, EmptyShapeConfiguration>)
            {
                shapeConfiguration.m_materials = {materialHandle};
            }

            const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
            ASSERT_TRUE(shapeHandle);

            PrimitiveShapeState state;
            ASSERT_TRUE(system.GetPrimitiveShapeState(worldHandle, shapeHandle, state));
            const auto* actual = AZStd::get_if<Geometry>(&state.m_geometry);
            ASSERT_TRUE(actual);

            if constexpr (AZStd::is_same_v<Geometry, BoxShapeConfiguration>)
            {
                EXPECT_TRUE(actual->m_dimensions.IsClose(expected.m_dimensions));
                EXPECT_FLOAT_EQ(actual->m_convexRadius, expected.m_convexRadius);
            }
            else if constexpr (AZStd::is_same_v<Geometry, CapsuleShapeConfiguration>)
            {
                EXPECT_FLOAT_EQ(actual->m_cylinderHeight, expected.m_cylinderHeight);
                EXPECT_FLOAT_EQ(actual->m_radius, expected.m_radius);
            }
            else if constexpr (AZStd::is_same_v<Geometry, CylinderShapeConfiguration>)
            {
                EXPECT_FLOAT_EQ(actual->m_height, expected.m_height);
                EXPECT_FLOAT_EQ(actual->m_radius, expected.m_radius);
                EXPECT_FLOAT_EQ(actual->m_convexRadius, expected.m_convexRadius);
            }
            else if constexpr (AZStd::is_same_v<Geometry, EmptyShapeConfiguration>)
            {
                EXPECT_TRUE(actual->m_centerOfMass.IsClose(expected.m_centerOfMass));
            }
            else if constexpr (AZStd::is_same_v<Geometry, PlaneShapeConfiguration>)
            {
                EXPECT_TRUE(actual->m_normal.IsClose(expected.m_normal.GetNormalized()));
                EXPECT_FLOAT_EQ(actual->m_distance, expected.m_distance);
                EXPECT_FLOAT_EQ(actual->m_halfExtent, expected.m_halfExtent);
            }
            else if constexpr (AZStd::is_same_v<Geometry, SphereShapeConfiguration>)
            {
                EXPECT_FLOAT_EQ(actual->m_radius, expected.m_radius);
            }
            else if constexpr (AZStd::is_same_v<Geometry, TaperedCapsuleShapeConfiguration>)
            {
                EXPECT_FLOAT_EQ(actual->m_height, expected.m_height);
                EXPECT_FLOAT_EQ(actual->m_bottomRadius, expected.m_bottomRadius);
                EXPECT_FLOAT_EQ(actual->m_topRadius, expected.m_topRadius);
            }
            else if constexpr (AZStd::is_same_v<Geometry, TaperedCylinderShapeConfiguration>)
            {
                EXPECT_FLOAT_EQ(actual->m_height, expected.m_height);
                EXPECT_FLOAT_EQ(actual->m_bottomRadius, expected.m_bottomRadius);
                EXPECT_FLOAT_EQ(actual->m_topRadius, expected.m_topRadius);
                EXPECT_FLOAT_EQ(actual->m_convexRadius, expected.m_convexRadius);
            }
            else if constexpr (AZStd::is_same_v<Geometry, TriangleShapeConfiguration>)
            {
                EXPECT_TRUE(actual->m_firstVertex.IsClose(expected.m_firstVertex));
                EXPECT_TRUE(actual->m_secondVertex.IsClose(expected.m_secondVertex));
                EXPECT_TRUE(actual->m_thirdVertex.IsClose(expected.m_thirdVertex));
                EXPECT_FLOAT_EQ(actual->m_convexRadius, expected.m_convexRadius);
            }

            EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        };

        verifyState(BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(2.0f, 3.0f, 4.0f),
            .m_convexRadius = 0.1f,
        });
        verifyState(CapsuleShapeConfiguration{
            .m_cylinderHeight = 2.5f,
            .m_radius = 0.4f,
        });
        verifyState(CylinderShapeConfiguration{
            .m_height = 3.0f,
            .m_radius = 0.7f,
            .m_convexRadius = 0.1f,
        });
        verifyState(EmptyShapeConfiguration{
            .m_centerOfMass = AZ::Vector3(1.0f, 2.0f, 3.0f),
        });
        verifyState(PlaneShapeConfiguration{
            .m_normal = 2.0f * AZ::Vector3::CreateAxisZ(),
            .m_distance = 4.0f,
            .m_halfExtent = 20.0f,
        });
        verifyState(SphereShapeConfiguration{.m_radius = 0.75f});
        verifyState(TaperedCapsuleShapeConfiguration{
            .m_height = 3.0f,
            .m_bottomRadius = 0.7f,
            .m_topRadius = 0.3f,
        });
        verifyState(TaperedCylinderShapeConfiguration{
            .m_height = 3.5f,
            .m_bottomRadius = 0.8f,
            .m_topRadius = 0.4f,
            .m_convexRadius = 0.1f,
        });
        verifyState(TriangleShapeConfiguration{
            .m_firstVertex = AZ::Vector3(-2.0f, -1.0f, 0.0f),
            .m_secondVertex = AZ::Vector3(2.0f, -1.0f, 0.0f),
            .m_thirdVertex = AZ::Vector3(0.0f, 3.0f, 0.0f),
            .m_convexRadius = 0.05f,
        });

        ShapeConfiguration unsupportedConfiguration;
        unsupportedConfiguration.m_geometry = ConvexHullShapeConfiguration{
            .m_points = {
                AZ::Vector3(-1.0f, -1.0f, -1.0f),
                AZ::Vector3(-1.0f, 1.0f, 1.0f),
                AZ::Vector3(1.0f, -1.0f, 1.0f),
                AZ::Vector3(1.0f, 1.0f, -1.0f),
            },
        };
        const ShapeHandle unsupportedHandle = system.CreateShape(
            worldHandle,
            unsupportedConfiguration);
        ASSERT_TRUE(unsupportedHandle);
        PrimitiveShapeState state;
        EXPECT_FALSE(system.GetPrimitiveShapeState(worldHandle, unsupportedHandle, state));
        EXPECT_TRUE(system.DestroyShape(worldHandle, unsupportedHandle));

        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, ConvexHullTopologyExposesCenterOfMassRelativeNativeGeometry)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const AZStd::vector<AZ::Vector3> sourcePoints = {
            AZ::Vector3::CreateZero(),
            AZ::Vector3::CreateAxisX(4.0f),
            AZ::Vector3::CreateAxisY(2.0f),
            AZ::Vector3::CreateAxisZ(3.0f),
        };
        ShapeConfiguration configuration;
        configuration.m_geometry = ConvexHullShapeConfiguration{
            .m_points = sourcePoints,
            .m_maximumConvexRadius = 0.1f,
        };
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, configuration);
        ASSERT_TRUE(shapeHandle);

        ConvexHullState state;
        ASSERT_TRUE(system.GetConvexHullState(worldHandle, shapeHandle, state));
        EXPECT_GT(state.m_convexRadius, 0.0f);
        EXPECT_LE(state.m_convexRadius, 0.1f);
        EXPECT_EQ(state.m_faceCount, 4);
        EXPECT_EQ(state.m_pointCount, 4);

        ShapeProperties properties;
        ASSERT_TRUE(system.GetShapeProperties(worldHandle, shapeHandle, properties));
        EXPECT_FALSE(properties.m_centerOfMass.IsZero());

        BufferResult bufferResult = system.GetConvexHullPointsRelativeToCenterOfMass(
            worldHandle,
            shapeHandle,
            {});
        EXPECT_EQ(bufferResult.m_count, 0);
        EXPECT_EQ(bufferResult.m_requiredCount, state.m_pointCount);
        AZStd::vector<AZ::Vector3> points(state.m_pointCount);
        bufferResult = system.GetConvexHullPointsRelativeToCenterOfMass(
            worldHandle,
            shapeHandle,
            points);
        EXPECT_TRUE(bufferResult.IsComplete());
        EXPECT_EQ(bufferResult.m_count, state.m_pointCount);
        for (const AZ::Vector3& point : points)
        {
            bool sourcePointFound = false;
            for (const AZ::Vector3& sourcePoint : sourcePoints)
            {
                if ((point + properties.m_centerOfMass).IsClose(sourcePoint))
                {
                    sourcePointFound = true;
                    break;
                }
            }
            EXPECT_TRUE(sourcePointFound);
        }

        bufferResult = system.GetConvexHullPlanesRelativeToCenterOfMass(
            worldHandle,
            shapeHandle,
            {});
        EXPECT_EQ(bufferResult.m_count, 0);
        EXPECT_EQ(bufferResult.m_requiredCount, state.m_faceCount);
        AZStd::vector<AZ::Plane> planes(state.m_faceCount);
        bufferResult = system.GetConvexHullPlanesRelativeToCenterOfMass(
            worldHandle,
            shapeHandle,
            planes);
        EXPECT_TRUE(bufferResult.IsComplete());
        EXPECT_EQ(bufferResult.m_count, state.m_faceCount);

        for (AZ::u32 faceIndex = 0; faceIndex < state.m_faceCount; ++faceIndex)
        {
            bufferResult = system.GetConvexHullFaceVertexIndices(
                worldHandle,
                shapeHandle,
                faceIndex,
                {});
            ASSERT_GE(bufferResult.m_requiredCount, 3);

            AZStd::vector<AZ::u32> vertexIndices(bufferResult.m_requiredCount - 1);
            BufferResult undersizedResult = system.GetConvexHullFaceVertexIndices(
                worldHandle,
                shapeHandle,
                faceIndex,
                vertexIndices);
            EXPECT_EQ(undersizedResult.m_count, 0);
            EXPECT_EQ(undersizedResult.m_requiredCount, bufferResult.m_requiredCount);

            vertexIndices.resize(bufferResult.m_requiredCount);
            bufferResult = system.GetConvexHullFaceVertexIndices(
                worldHandle,
                shapeHandle,
                faceIndex,
                vertexIndices);
            ASSERT_TRUE(bufferResult.IsComplete());
            for (size_t faceVertexIndex = 0; faceVertexIndex < vertexIndices.size(); ++faceVertexIndex)
            {
                ASSERT_LT(vertexIndices[faceVertexIndex], points.size());
                EXPECT_NEAR(
                    planes[faceIndex].GetPointDist(points[vertexIndices[faceVertexIndex]]),
                    0.0f,
                    1.0e-5f);
                for (size_t previousIndex = 0; previousIndex < faceVertexIndex; ++previousIndex)
                {
                    EXPECT_NE(vertexIndices[faceVertexIndex], vertexIndices[previousIndex]);
                }
            }

            const AZ::Vector3 firstEdge = points[vertexIndices[1]] - points[vertexIndices[0]];
            const AZ::Vector3 secondEdge = points[vertexIndices[2]] - points[vertexIndices[0]];
            EXPECT_GT(firstEdge.Cross(secondEdge).Dot(planes[faceIndex].GetNormal()), 0.0f);
            for (const AZ::Vector3& point : points)
            {
                EXPECT_LE(planes[faceIndex].GetPointDist(point), 1.0e-5f);
            }
        }

        EXPECT_EQ(
            system.GetConvexHullFaceVertexIndices(
                worldHandle,
                shapeHandle,
                state.m_faceCount,
                {}).m_requiredCount,
            0);

        DecoratedShapeConfiguration scaledConfiguration;
        scaledConfiguration.m_geometry = ScaledShapeConfiguration{
            .m_shapeHandle = shapeHandle,
            .m_scale = AZ::Vector3(2.0f),
        };
        const ShapeHandle scaledHandle = system.CreateShape(worldHandle, scaledConfiguration);
        ASSERT_TRUE(scaledHandle);
        EXPECT_FALSE(system.GetConvexHullState(worldHandle, scaledHandle, state));
        EXPECT_TRUE(system.DestroyShape(worldHandle, scaledHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, WorldShapesExposeNativePropertiesAndSurfaceData)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const MaterialHandle materialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(materialHandle);

        ShapeConfiguration configuration;
        configuration.m_geometry = SphereShapeConfiguration{.m_radius = 2.0f};
        configuration.m_materials = {materialHandle};
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, configuration);
        ASSERT_TRUE(shapeHandle);

        ShapeStats stats;
        ASSERT_TRUE(system.GetShapeStats(worldHandle, shapeHandle, stats));
        EXPECT_TRUE(stats.m_localBounds.IsValid());
        EXPECT_TRUE(stats.m_localBounds.Contains(AZ::Vector3::CreateAxisX(2.0f)));
        EXPECT_TRUE(stats.m_localBounds.Contains(AZ::Vector3::CreateAxisX(-2.0f)));
        EXPECT_GT(stats.m_memorySize, 0);
        EXPECT_EQ(stats.m_triangleCount, 0);
        ShapeStats recursiveStats;
        ASSERT_TRUE(system.GetShapeStatsRecursive(worldHandle, shapeHandle, recursiveStats));
        EXPECT_GE(recursiveStats.m_memorySize, stats.m_memorySize);
        EXPECT_EQ(recursiveStats.m_triangleCount, stats.m_triangleCount);

        ShapeProperties properties;
        ASSERT_TRUE(system.GetShapeProperties(worldHandle, shapeHandle, properties));
        EXPECT_EQ(properties.m_kind, ShapeKind::Sphere);
        EXPECT_TRUE(properties.m_centerOfMass.IsZero());
        EXPECT_FLOAT_EQ(properties.m_innerRadius, 2.0f);
        EXPECT_NEAR(properties.m_volume, 4.0f / 3.0f * AZ::Constants::Pi * 8.0f, 1.0e-4f);
        EXPECT_NEAR(properties.m_mass, properties.m_volume * configuration.m_density, 1.0e-2f);
        EXPECT_FALSE(properties.m_mustBeStatic);

        SubmergedVolumeRequest submergedVolumeRequest;
        submergedVolumeRequest.m_centerOfMassTransform.m_position = {
            .m_x = 1'000'000'000.0,
            .m_y = -2'000'000'000.0,
            .m_z = 3'000'000'000.0,
        };
        submergedVolumeRequest.m_surfacePosition =
            submergedVolumeRequest.m_centerOfMassTransform.m_position;
        SubmergedVolumeResult submergedVolumeResult;
        ASSERT_TRUE(system.GetShapeSubmergedVolume(
            worldHandle,
            shapeHandle,
            submergedVolumeRequest,
            submergedVolumeResult));
        EXPECT_NEAR(submergedVolumeResult.m_totalVolume, properties.m_volume, 1.0e-4f);
        EXPECT_NEAR(submergedVolumeResult.m_submergedVolume, 0.5f * properties.m_volume, 1.0e-4f);
        EXPECT_NEAR(
            submergedVolumeResult.m_centerOfBuoyancy.m_x,
            submergedVolumeRequest.m_centerOfMassTransform.m_position.m_x,
            1.0e-6);
        EXPECT_NEAR(
            submergedVolumeResult.m_centerOfBuoyancy.m_y,
            submergedVolumeRequest.m_centerOfMassTransform.m_position.m_y,
            1.0e-6);
        EXPECT_NEAR(
            submergedVolumeResult.m_centerOfBuoyancy.m_z,
            submergedVolumeRequest.m_centerOfMassTransform.m_position.m_z - 0.75,
            1.0e-6);

        submergedVolumeRequest.m_surfaceNormal = AZ::Vector3::CreateZero();
        EXPECT_FALSE(system.GetShapeSubmergedVolume(
            worldHandle,
            shapeHandle,
            submergedVolumeRequest,
            submergedVolumeResult));
        submergedVolumeRequest.m_surfaceNormal = AZ::Vector3::CreateAxisZ();
        submergedVolumeRequest.m_scale = AZ::Vector3(2.0f, 1.0f, 1.0f);
        EXPECT_FALSE(system.GetShapeSubmergedVolume(
            worldHandle,
            shapeHandle,
            submergedVolumeRequest,
            submergedVolumeResult));

        ShapeConfiguration planeConfiguration;
        planeConfiguration.m_geometry = PlaneShapeConfiguration{};
        const ShapeHandle planeShapeHandle = system.CreateShape(worldHandle, planeConfiguration);
        ASSERT_TRUE(planeShapeHandle);
        EXPECT_FALSE(system.GetShapeSubmergedVolume(
            worldHandle,
            planeShapeHandle,
            submergedVolumeRequest,
            submergedVolumeResult));

        CompoundShapeConfiguration unsupportedCompoundConfiguration;
        unsupportedCompoundConfiguration.m_children = {
            {.m_shapeHandle = planeShapeHandle},
        };
        const ShapeHandle unsupportedCompoundHandle =
            system.CreateShape(worldHandle, unsupportedCompoundConfiguration);
        ASSERT_TRUE(unsupportedCompoundHandle);
        EXPECT_FALSE(system.GetShapeSubmergedVolume(
            worldHandle,
            unsupportedCompoundHandle,
            submergedVolumeRequest,
            submergedVolumeResult));

        BodyConfiguration planeBodyConfiguration;
        planeBodyConfiguration.m_shapeHandle = planeShapeHandle;
        planeBodyConfiguration.m_motionType = MotionType::Static;
        planeBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        const BodyHandle planeBodyHandle = system.CreateBody(worldHandle, planeBodyConfiguration);
        ASSERT_TRUE(planeBodyHandle);
        EXPECT_FALSE(system.GetBodySubmergedVolume(
            worldHandle,
            planeBodyHandle,
            submergedVolumeRequest.m_surfacePosition,
            submergedVolumeRequest.m_surfaceNormal,
            submergedVolumeResult));
        EXPECT_TRUE(system.DestroyBody(worldHandle, planeBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, unsupportedCompoundHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, planeShapeHandle));

        MaterialHandle surfaceMaterialHandle;
        EXPECT_TRUE(system.GetShapeMaterial(
            worldHandle,
            shapeHandle,
            SubShapeId::Root,
            surfaceMaterialHandle));
        EXPECT_EQ(surfaceMaterialHandle, materialHandle);
        EXPECT_FALSE(system.GetShapeMaterial(
            worldHandle,
            shapeHandle,
            SubShapeId(0),
            surfaceMaterialHandle));

        AZ::Vector3 surfaceNormal = AZ::Vector3::CreateZero();
        EXPECT_TRUE(system.GetShapeSurfaceNormal(
            worldHandle,
            shapeHandle,
            SubShapeId::Root,
            AZ::Vector3::CreateAxisX(2.0f),
            surfaceNormal));
        EXPECT_TRUE(surfaceNormal.IsClose(AZ::Vector3::CreateAxisX()));
        EXPECT_FALSE(system.GetShapeSurfaceNormal(
            worldHandle,
            shapeHandle,
            SubShapeId(0),
            AZ::Vector3::CreateAxisX(2.0f),
            surfaceNormal));

        const AZ::Vector3 uniformScale(2.0f);
        const AZ::Vector3 nonUniformScale(2.0f, 1.0f, 1.0f);
        EXPECT_TRUE(system.IsShapeScaleValid(worldHandle, shapeHandle, uniformScale));
        EXPECT_FALSE(system.IsShapeScaleValid(worldHandle, shapeHandle, nonUniformScale));
        AZ::Vector3 validScale = AZ::Vector3::CreateZero();
        EXPECT_TRUE(system.MakeShapeScaleValid(
            worldHandle,
            shapeHandle,
            nonUniformScale,
            validScale));
        EXPECT_TRUE(system.IsShapeScaleValid(worldHandle, shapeHandle, validScale));

        EXPECT_FALSE(system.GetShapeSurfaceNormal(
            worldHandle,
            shapeHandle,
            SubShapeId::Root,
            AZ::Vector3(
                AZStd::numeric_limits<float>::quiet_NaN(),
                0.0f,
                0.0f),
            surfaceNormal));

        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_FALSE(system.GetShapeStats(worldHandle, shapeHandle, stats));
        EXPECT_FALSE(system.GetShapeStatsRecursive(worldHandle, shapeHandle, recursiveStats));
        EXPECT_FALSE(system.GetShapeProperties(worldHandle, shapeHandle, properties));
        EXPECT_FALSE(system.GetShapeSubmergedVolume(
            worldHandle,
            shapeHandle,
            submergedVolumeRequest,
            submergedVolumeResult));
        EXPECT_FALSE(system.GetShapeMaterial(
            worldHandle,
            shapeHandle,
            SubShapeId::Root,
            surfaceMaterialHandle));
        EXPECT_FALSE(system.IsShapeScaleValid(worldHandle, shapeHandle, uniformScale));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, SkeletonMappingsPreserveProviderOwnershipAndCallerBuffers)
    {
        NameDictionaryScope nameDictionaryScope;
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SkeletonDefinitionConfiguration sourceConfiguration;
        sourceConfiguration.m_joints = {
            {.m_name = AZ::Name("root"), .m_parentIndex = -1},
            {.m_name = AZ::Name("child"), .m_parentIndex = 0},
        };
        const SkeletonDefinitionHandle sourceHandle =
            system.CreateSkeletonDefinition(sourceConfiguration);
        ASSERT_TRUE(sourceHandle);

        AZ::u32 sourceJointIndex = AZStd::numeric_limits<AZ::u32>::max();
        EXPECT_TRUE(system.FindSkeletonJoint(sourceHandle, AZ::Name("child"), sourceJointIndex));
        EXPECT_EQ(sourceJointIndex, 1);
        EXPECT_FALSE(system.FindSkeletonJoint(sourceHandle, AZ::Name("missing"), sourceJointIndex));

        SkeletonDefinitionConfiguration targetConfiguration;
        targetConfiguration.m_joints = {
            {.m_name = AZ::Name("visual_root"), .m_parentIndex = -1},
            {.m_name = AZ::Name("twist"), .m_parentIndex = 0},
            {.m_name = AZ::Name("visual_child"), .m_parentIndex = 1},
            {.m_name = AZ::Name("unmapped_leaf"), .m_parentIndex = 0},
        };
        const SkeletonDefinitionHandle targetHandle =
            system.CreateSkeletonDefinition(targetConfiguration);
        ASSERT_TRUE(targetHandle);

        AZStd::array<SkeletonJoint, 1> truncatedJoints;
        const QueryResult truncatedResult = system.GetSkeletonJoints(sourceHandle, truncatedJoints);
        EXPECT_EQ(truncatedResult.m_hitCount, 1);
        EXPECT_EQ(truncatedResult.m_requiredHitCount, 2);
        EXPECT_FALSE(truncatedResult.IsComplete());

        SkeletonMapperConfiguration mapperConfiguration;
        mapperConfiguration.m_sourceSkeletonHandle = sourceHandle;
        mapperConfiguration.m_targetSkeletonHandle = targetHandle;
        mapperConfiguration.m_sourceNeutralModelTransforms = {
            AZ::Transform::CreateIdentity(),
            AZ::Transform::CreateTranslation(AZ::Vector3(0.0f, 2.0f, 0.0f)),
        };
        mapperConfiguration.m_targetNeutralModelTransforms = {
            AZ::Transform::CreateIdentity(),
            AZ::Transform::CreateTranslation(AZ::Vector3(0.0f, 1.0f, 0.0f)),
            AZ::Transform::CreateTranslation(AZ::Vector3(0.0f, 2.0f, 0.0f)),
            AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX()),
        };
        mapperConfiguration.m_jointMappings = {
            {.m_sourceJoint = 0, .m_targetJoint = 0},
            {.m_sourceJoint = 1, .m_targetJoint = 2},
        };

        SkeletonMapperConfiguration invalidMapperConfiguration = mapperConfiguration;
        invalidMapperConfiguration.m_lockedTargetTranslations = {1, 0, 0, 0};
        EXPECT_FALSE(system.CreateSkeletonMapper(invalidMapperConfiguration));

        mapperConfiguration.m_lockedTargetTranslations = {0, 0, 1, 0};
        const SkeletonMapperHandle mapperHandle = system.CreateSkeletonMapper(mapperConfiguration);
        ASSERT_TRUE(mapperHandle);
        EXPECT_FALSE(system.DestroySkeletonDefinition(sourceHandle));
        EXPECT_FALSE(system.DestroySkeletonDefinition(targetHandle));

        SkeletonMapperState mapperState;
        ASSERT_TRUE(system.GetSkeletonMapperState(mapperHandle, mapperState));
        EXPECT_EQ(mapperState.m_sourceJointCount, 2);
        EXPECT_EQ(mapperState.m_targetJointCount, 4);
        EXPECT_EQ(mapperState.m_mappingCount, 2);
        EXPECT_EQ(mapperState.m_chainCount, 1);
        EXPECT_EQ(mapperState.m_lockedTranslationCount, 1);
        EXPECT_EQ(mapperState.m_unmappedJointCount, 1);

        AZStd::array<SkeletonMapperMappingState, 2> mappingStates;
        EXPECT_TRUE(system.GetSkeletonMapperMappings(mapperHandle, mappingStates).IsComplete());
        EXPECT_EQ(mappingStates[0].m_sourceJointIndex, 0);
        EXPECT_EQ(mappingStates[0].m_targetJointIndex, 0);
        EXPECT_EQ(mappingStates[1].m_sourceJointIndex, 1);
        EXPECT_EQ(mappingStates[1].m_targetJointIndex, 2);
        EXPECT_TRUE(mappingStates[0].m_sourceToTarget.IsClose(AZ::Transform::CreateIdentity()));
        EXPECT_TRUE(mappingStates[0].m_targetToSource.IsClose(AZ::Transform::CreateIdentity()));

        SkeletonMapperChainState chainState;
        ASSERT_TRUE(system.GetSkeletonMapperChainState(mapperHandle, 0, chainState));
        EXPECT_EQ(chainState.m_sourceJointCount, 2);
        EXPECT_EQ(chainState.m_targetJointCount, 3);
        EXPECT_FALSE(system.GetSkeletonMapperChainState(mapperHandle, 1, chainState));

        AZStd::array<AZ::u32, 2> sourceChain;
        EXPECT_TRUE(system.GetSkeletonMapperSourceChain(mapperHandle, 0, sourceChain).IsComplete());
        EXPECT_EQ(sourceChain[0], 0);
        EXPECT_EQ(sourceChain[1], 1);
        AZStd::array<AZ::u32, 3> targetChain;
        EXPECT_TRUE(system.GetSkeletonMapperTargetChain(mapperHandle, 0, targetChain).IsComplete());
        EXPECT_EQ(targetChain[0], 0);
        EXPECT_EQ(targetChain[1], 1);
        EXPECT_EQ(targetChain[2], 2);

        AZStd::array<SkeletonMapperUnmappedJoint, 1> unmappedJoints;
        EXPECT_TRUE(system.GetSkeletonMapperUnmappedJoints(mapperHandle, unmappedJoints).IsComplete());
        EXPECT_EQ(unmappedJoints[0].m_jointIndex, 3);
        EXPECT_EQ(unmappedJoints[0].m_parentJointIndex, 0);

        AZStd::array<SkeletonMapperLockedTranslation, 1> lockedTranslations;
        EXPECT_TRUE(system.GetSkeletonMapperLockedTranslations(mapperHandle, lockedTranslations).IsComplete());
        EXPECT_EQ(lockedTranslations[0].m_jointIndex, 2);
        EXPECT_EQ(lockedTranslations[0].m_parentJointIndex, 1);
        EXPECT_TRUE(lockedTranslations[0].m_translation.IsClose(AZ::Vector3::CreateAxisY()));

        AZ::u32 mappedJointIndex = AZStd::numeric_limits<AZ::u32>::max();
        EXPECT_TRUE(system.GetMappedSkeletonJoint(mapperHandle, 1, mappedJointIndex));
        EXPECT_EQ(mappedJointIndex, 2);
        EXPECT_FALSE(system.GetMappedSkeletonJoint(mapperHandle, 2, mappedJointIndex));

        bool translationLocked = false;
        EXPECT_TRUE(system.IsSkeletonJointTranslationLocked(mapperHandle, 2, translationLocked));
        EXPECT_TRUE(translationLocked);
        EXPECT_TRUE(system.IsSkeletonJointTranslationLocked(mapperHandle, 1, translationLocked));
        EXPECT_FALSE(translationLocked);
        EXPECT_TRUE(system.IsSkeletonJointTranslationLocked(mapperHandle, 3, translationLocked));
        EXPECT_FALSE(translationLocked);
        EXPECT_FALSE(system.IsSkeletonJointTranslationLocked(mapperHandle, 4, translationLocked));

        const AZStd::array<AZ::Transform, 2> sourceModelTransforms = {
            AZ::Transform::CreateIdentity(),
            AZ::Transform::CreateTranslation(AZ::Vector3(0.0f, 2.0f, 0.0f)),
        };
        const AZStd::array<AZ::Transform, 4> targetLocalTransforms = {
            AZ::Transform::CreateIdentity(),
            AZ::Transform::CreateTranslation(AZ::Vector3(0.0f, 1.0f, 0.0f)),
            AZ::Transform::CreateTranslation(AZ::Vector3(0.0f, 1.0f, 0.0f)),
            AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX()),
        };
        AZStd::array<AZ::Transform, 4> targetModelTransforms;
        ASSERT_TRUE(system.MapSkeletonPose(
            mapperHandle,
            sourceModelTransforms,
            targetLocalTransforms,
            targetModelTransforms));
        EXPECT_TRUE(targetModelTransforms[2].IsClose(sourceModelTransforms[1]));

        AZStd::array<AZ::Transform, 2> reverseMappedTransforms;
        ASSERT_TRUE(system.MapSkeletonPoseReverse(
            mapperHandle,
            targetModelTransforms,
            reverseMappedTransforms));
        EXPECT_TRUE(reverseMappedTransforms[0].IsClose(sourceModelTransforms[0]));
        EXPECT_TRUE(reverseMappedTransforms[1].IsClose(sourceModelTransforms[1]));

        EXPECT_TRUE(system.DestroySkeletonMapper(mapperHandle));
        EXPECT_TRUE(system.DestroySkeletonDefinition(sourceHandle));
        EXPECT_TRUE(system.DestroySkeletonDefinition(targetHandle));
    }

    TEST(SimulationTests, SkeletalAnimationsSampleReusablePosesWithoutNativeTypes)
    {
        NameDictionaryScope nameDictionaryScope;
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SkeletonDefinitionConfiguration skeletonConfiguration;
        skeletonConfiguration.m_joints = {
            {.m_name = AZ::Name("root"), .m_parentIndex = -1},
            {.m_name = AZ::Name("child"), .m_parentIndex = 0},
        };
        const SkeletonDefinitionHandle skeletonHandle =
            system.CreateSkeletonDefinition(skeletonConfiguration);
        ASSERT_TRUE(skeletonHandle);

        SkeletalAnimationConfiguration animationConfiguration;
        animationConfiguration.m_joints = {
            {
                .m_name = AZ::Name("root"),
                .m_keyframes = {
                    {},
                    {
                        .m_translation = AZ::Vector3::CreateAxisX(2.0f),
                        .m_time = 1.0f,
                    },
                },
            },
            {
                .m_name = AZ::Name("child"),
                .m_keyframes = {
                    {.m_translation = AZ::Vector3::CreateAxisY()},
                    {
                        .m_translation = AZ::Vector3::CreateAxisY(3.0f),
                        .m_time = 1.0f,
                    },
                },
            },
        };
        const SkeletalAnimationHandle animationHandle =
            system.CreateSkeletalAnimation(animationConfiguration);
        ASSERT_TRUE(animationHandle);

        SkeletalAnimationState animationState;
        ASSERT_TRUE(system.GetSkeletalAnimationState(animationHandle, animationState));
        EXPECT_FLOAT_EQ(animationState.m_duration, 1.0f);
        EXPECT_EQ(animationState.m_jointCount, 2);
        EXPECT_TRUE(animationState.m_isLooping);

        AZ::Name animatedJointName;
        EXPECT_TRUE(system.GetSkeletalAnimatedJointName(animationHandle, 1, animatedJointName));
        EXPECT_EQ(animatedJointName, AZ::Name("child"));
        EXPECT_FALSE(system.GetSkeletalAnimatedJointName(animationHandle, 2, animatedJointName));

        AZStd::array<SkeletalAnimationKeyframe, 1> truncatedKeyframes;
        const QueryResult keyframeResult =
            system.GetSkeletalAnimationKeyframes(animationHandle, 0, truncatedKeyframes);
        EXPECT_EQ(keyframeResult.m_hitCount, 1);
        EXPECT_EQ(keyframeResult.m_requiredHitCount, 2);
        EXPECT_FALSE(keyframeResult.IsComplete());
        EXPECT_TRUE(truncatedKeyframes[0].m_rotation.IsClose(AZ::Quaternion::CreateIdentity()));
        EXPECT_TRUE(truncatedKeyframes[0].m_translation.IsZero());
        EXPECT_FLOAT_EQ(truncatedKeyframes[0].m_time, 0.0f);
        EXPECT_EQ(
            system.GetSkeletalAnimationKeyframes(animationHandle, 2, truncatedKeyframes).m_requiredHitCount,
            0);

        const SkeletonPoseHandle poseHandle = system.CreateSkeletonPose(skeletonHandle);
        ASSERT_TRUE(poseHandle);
        EXPECT_FALSE(system.DestroySkeletonDefinition(skeletonHandle));
        ASSERT_TRUE(system.SetSkeletonPoseRootOffset(
            poseHandle,
            {.m_x = 100.0, .m_y = -25.0, .m_z = 4.0}));

        SkeletonPoseState poseState;
        ASSERT_TRUE(system.GetSkeletonPoseState(poseHandle, poseState));
        EXPECT_EQ(poseState.m_skeletonHandle, skeletonHandle);
        EXPECT_EQ(poseState.m_jointCount, 2);
        EXPECT_DOUBLE_EQ(poseState.m_rootOffset.m_x, 100.0);

        ASSERT_TRUE(system.SampleSkeletalAnimation(animationHandle, poseHandle, 0.5f));
        AZStd::array<AZ::Transform, 2> modelTransforms;
        const QueryResult modelResult =
            system.GetSkeletonPoseModelTransforms(poseHandle, modelTransforms);
        EXPECT_TRUE(modelResult.IsComplete());
        EXPECT_TRUE(modelTransforms[0].GetTranslation().IsClose(AZ::Vector3::CreateAxisX()));
        EXPECT_TRUE(modelTransforms[1].GetTranslation().IsClose(AZ::Vector3(1.0f, 2.0f, 0.0f)));

        SkeletalAnimationConfiguration invalidUpdate = animationConfiguration;
        invalidUpdate.m_joints[1].m_name = AZ::Name("root");
        EXPECT_FALSE(system.UpdateSkeletalAnimation(animationHandle, invalidUpdate));
        ASSERT_TRUE(system.SampleSkeletalAnimation(animationHandle, poseHandle, 0.5f));
        ASSERT_TRUE(system.GetSkeletonPoseModelTransforms(poseHandle, modelTransforms).IsComplete());
        EXPECT_TRUE(modelTransforms[0].GetTranslation().IsClose(AZ::Vector3::CreateAxisX()));

        SkeletalAnimationConfiguration updatedAnimation = animationConfiguration;
        AZStd::swap(updatedAnimation.m_joints[0], updatedAnimation.m_joints[1]);
        updatedAnimation.m_joints[1].m_keyframes[1].m_translation = AZ::Vector3::CreateAxisX(4.0f);
        updatedAnimation.m_isLooping = false;
        ASSERT_TRUE(system.UpdateSkeletalAnimation(animationHandle, updatedAnimation));
        ASSERT_TRUE(system.SampleSkeletalAnimation(animationHandle, poseHandle, 0.5f));
        ASSERT_TRUE(system.GetSkeletonPoseModelTransforms(poseHandle, modelTransforms).IsComplete());
        EXPECT_TRUE(modelTransforms[0].GetTranslation().IsClose(AZ::Vector3::CreateAxisX(2.0f)));

        AZStd::array<AZ::Transform, 1> truncatedTransforms;
        const QueryResult truncatedResult =
            system.GetSkeletonPoseLocalTransforms(poseHandle, truncatedTransforms);
        EXPECT_EQ(truncatedResult.m_hitCount, 1);
        EXPECT_EQ(truncatedResult.m_requiredHitCount, 2);
        EXPECT_FALSE(truncatedResult.IsComplete());

        EXPECT_TRUE(system.SetSkeletalAnimationLooping(animationHandle, false));
        ASSERT_TRUE(system.SampleSkeletalAnimation(animationHandle, poseHandle, 2.0f));
        ASSERT_TRUE(system.GetSkeletonPoseModelTransforms(poseHandle, modelTransforms).IsComplete());
        EXPECT_TRUE(modelTransforms[0].GetTranslation().IsClose(AZ::Vector3::CreateAxisX(4.0f)));

        EXPECT_TRUE(system.ScaleSkeletalAnimation(animationHandle, 2.0f));
        ASSERT_TRUE(system.SampleSkeletalAnimation(animationHandle, poseHandle, 1.0f));
        ASSERT_TRUE(system.GetSkeletonPoseModelTransforms(poseHandle, modelTransforms).IsComplete());
        EXPECT_TRUE(modelTransforms[0].GetTranslation().IsClose(AZ::Vector3::CreateAxisX(8.0f)));

        AZStd::array<AZ::Transform, 2> localTransformsBeforeFailure;
        ASSERT_TRUE(
            system.GetSkeletonPoseLocalTransforms(poseHandle, localTransformsBeforeFailure).IsComplete());
        AZStd::array<AZ::Transform, 2> invalidLocalTransforms = {
            AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisZ(99.0f)),
            AZ::Transform::CreateUniformScale(2.0f),
        };
        EXPECT_FALSE(system.SetSkeletonPoseLocalTransforms(poseHandle, invalidLocalTransforms));
        AZStd::array<AZ::Transform, 2> localTransformsAfterFailure;
        ASSERT_TRUE(
            system.GetSkeletonPoseLocalTransforms(poseHandle, localTransformsAfterFailure).IsComplete());
        EXPECT_TRUE(localTransformsAfterFailure[0].IsClose(localTransformsBeforeFailure[0]));
        EXPECT_TRUE(localTransformsAfterFailure[1].IsClose(localTransformsBeforeFailure[1]));
        invalidLocalTransforms[0] = AZ::Transform::CreateIdentity();
        invalidLocalTransforms[1] = AZ::Transform::CreateIdentity();
        EXPECT_TRUE(system.SetSkeletonPoseLocalTransforms(poseHandle, invalidLocalTransforms));
        EXPECT_TRUE(system.SetSkeletonPoseModelTransforms(poseHandle, modelTransforms));

        SkeletalAnimationConfiguration incompatibleConfiguration;
        incompatibleConfiguration.m_joints = {
            {
                .m_name = AZ::Name("missing"),
                .m_keyframes = {{}},
            },
        };
        const SkeletalAnimationHandle incompatibleHandle =
            system.CreateSkeletalAnimation(incompatibleConfiguration);
        ASSERT_TRUE(incompatibleHandle);
        EXPECT_FALSE(system.SampleSkeletalAnimation(incompatibleHandle, poseHandle, 0.0f));
        EXPECT_TRUE(system.DestroySkeletalAnimation(incompatibleHandle));

        EXPECT_TRUE(system.DestroySkeletonPose(poseHandle));
        EXPECT_FALSE(system.IsValid(poseHandle));
        EXPECT_TRUE(system.DestroySkeletalAnimation(animationHandle));
        EXPECT_FALSE(system.IsValid(animationHandle));
        EXPECT_TRUE(system.DestroySkeletonDefinition(skeletonHandle));
    }

    TEST(SimulationTests, SkeletonAndAnimationArchivesRestoreValidatedNativeResources)
    {
        NameDictionaryScope nameDictionaryScope;
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SkeletonDefinitionConfiguration skeletonConfiguration;
        skeletonConfiguration.m_joints = {
            {.m_name = AZ::Name("root"), .m_parentIndex = -1},
            {.m_name = AZ::Name("child"), .m_parentIndex = 0},
        };
        const SkeletonDefinitionHandle sourceSkeleton =
            system.CreateSkeletonDefinition(skeletonConfiguration);
        ASSERT_TRUE(sourceSkeleton);

        SkeletalAnimationConfiguration animationConfiguration;
        animationConfiguration.m_joints = {
            {
                .m_name = AZ::Name("root"),
                .m_keyframes = {
                    {},
                    {
                        .m_translation = AZ::Vector3::CreateAxisX(2.0f),
                        .m_time = 1.0f,
                    },
                },
            },
        };
        const SkeletalAnimationHandle sourceAnimation =
            system.CreateSkeletalAnimation(animationConfiguration);
        ASSERT_TRUE(sourceAnimation);

        SkeletonDefinitionArchive skeletonArchive;
        SkeletalAnimationArchive animationArchive;
        ASSERT_TRUE(system.ExportSkeletonDefinition(sourceSkeleton, skeletonArchive));
        ASSERT_TRUE(system.ExportSkeletalAnimation(sourceAnimation, animationArchive));
        EXPECT_FALSE(skeletonArchive.m_binaryState.empty());
        EXPECT_FALSE(animationArchive.m_binaryState.empty());
        EXPECT_EQ(skeletonArchive.m_formatVersion, 1);
        EXPECT_EQ(animationArchive.m_formatVersion, 1);
        EXPECT_EQ(skeletonArchive.m_jointCount, 2);
        EXPECT_EQ(animationArchive.m_jointCount, 1);

        EXPECT_TRUE(system.DestroySkeletalAnimation(sourceAnimation));
        EXPECT_TRUE(system.DestroySkeletonDefinition(sourceSkeleton));
        const SkeletonDefinitionHandle restoredSkeleton =
            system.ImportSkeletonDefinition(skeletonArchive);
        const SkeletalAnimationHandle restoredAnimation =
            system.ImportSkeletalAnimation(animationArchive);
        ASSERT_TRUE(restoredSkeleton);
        ASSERT_TRUE(restoredAnimation);

        AZStd::array<SkeletonJoint, 2> joints;
        ASSERT_TRUE(system.GetSkeletonJoints(restoredSkeleton, joints).IsComplete());
        EXPECT_EQ(joints[0].m_name, AZ::Name("root"));
        EXPECT_EQ(joints[1].m_name, AZ::Name("child"));
        EXPECT_EQ(joints[1].m_parentIndex, 0);

        const SkeletonPoseHandle poseHandle = system.CreateSkeletonPose(restoredSkeleton);
        ASSERT_TRUE(poseHandle);
        ASSERT_TRUE(system.SampleSkeletalAnimation(restoredAnimation, poseHandle, 0.5f));
        AZStd::array<AZ::Transform, 2> localTransforms;
        ASSERT_TRUE(system.GetSkeletonPoseLocalTransforms(poseHandle, localTransforms).IsComplete());
        EXPECT_TRUE(localTransforms[0].GetTranslation().IsClose(AZ::Vector3::CreateAxisX()));

        SkeletonDefinitionArchive corruptSkeletonArchive = skeletonArchive;
        corruptSkeletonArchive.m_binaryState.back() ^= 0xff;
        EXPECT_FALSE(system.ImportSkeletonDefinition(corruptSkeletonArchive));
        SkeletalAnimationArchive corruptAnimationArchive = animationArchive;
        ++corruptAnimationArchive.m_buildFingerprint;
        EXPECT_FALSE(system.ImportSkeletalAnimation(corruptAnimationArchive));

        EXPECT_TRUE(system.DestroySkeletonPose(poseHandle));
        EXPECT_TRUE(system.DestroySkeletalAnimation(restoredAnimation));
        EXPECT_TRUE(system.DestroySkeletonDefinition(restoredSkeleton));
    }

    TEST(SimulationTests, RagdollOwnsChildrenAndSupportsAllocationFreePoseControl)
    {
        NameDictionaryScope nameDictionaryScope;
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        SkeletonDefinitionConfiguration skeletonConfiguration;
        skeletonConfiguration.m_joints = {
            {
                .m_name = AZ::Name("root"),
                .m_parentIndex = -1,
            },
            {
                .m_name = AZ::Name("child"),
                .m_parentIndex = 0,
            },
        };
        const SkeletonDefinitionHandle skeletonHandle =
            system.CreateSkeletonDefinition(skeletonConfiguration);
        ASSERT_TRUE(skeletonHandle);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.25f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        RagdollDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_skeletonHandle = skeletonHandle;
        definitionConfiguration.m_baseConstraintPriority = 7;
        definitionConfiguration.m_parts.resize(2);
        definitionConfiguration.m_parts[0].m_body.m_shapeHandle = shapeHandle;
        definitionConfiguration.m_parts[1].m_body.m_shapeHandle = shapeHandle;
        definitionConfiguration.m_parts[1].m_body.m_transform.m_position.m_z = 1.0;
        HingeConstraintConfiguration hinge;
        hinge.m_firstPoint.m_z = 0.5;
        hinge.m_secondPoint.m_z = -0.5;
        definitionConfiguration.m_parts[1].m_toParent = hinge;

        const RagdollDefinitionHandle definitionHandle =
            system.CreateRagdollDefinition(worldHandle, definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        AZStd::array<AZ::s32, 1> truncatedConstraintIndices;
        const QueryResult bodyConstraintResult = system.GetRagdollBodyConstraintIndices(
            worldHandle,
            definitionHandle,
            truncatedConstraintIndices);
        EXPECT_EQ(bodyConstraintResult.m_hitCount, 1);
        EXPECT_EQ(bodyConstraintResult.m_requiredHitCount, 2);
        EXPECT_EQ(truncatedConstraintIndices[0], -1);

        AZStd::array<RagdollConstraintBodyPair, 1> constraintBodyPairs;
        const QueryResult constraintBodyResult = system.GetRagdollConstraintBodyPairs(
            worldHandle,
            definitionHandle,
            constraintBodyPairs);
        EXPECT_TRUE(constraintBodyResult.IsComplete());
        EXPECT_EQ(constraintBodyPairs[0].m_firstBodyIndex, 0);
        EXPECT_EQ(constraintBodyPairs[0].m_secondBodyIndex, 1);
        EXPECT_FALSE(system.DestroySkeletonDefinition(skeletonHandle));
        EXPECT_FALSE(system.DestroyShape(worldHandle, shapeHandle));

        RagdollConfiguration ragdollConfiguration;
        ragdollConfiguration.m_definitionHandle = definitionHandle;
        ragdollConfiguration.m_rootPosition.m_z = 3.0;
        ragdollConfiguration.m_entityId = AZ::EntityId(41);
        ragdollConfiguration.m_name = AZ::Name("test_ragdoll");
        const RagdollHandle ragdollHandle = system.CreateRagdoll(worldHandle, ragdollConfiguration);
        ASSERT_TRUE(ragdollHandle);
        EXPECT_FALSE(system.DestroyRagdollDefinition(worldHandle, definitionHandle));

        AZStd::array<BodyHandle, 2> bodyHandles;
        const QueryResult bodyResult = system.GetRagdollBodies(worldHandle, ragdollHandle, bodyHandles);
        EXPECT_EQ(bodyResult.m_hitCount, 2);
        EXPECT_EQ(bodyResult.m_requiredHitCount, 2);
        EXPECT_TRUE(bodyHandles[0]);
        EXPECT_TRUE(bodyHandles[1]);
        EXPECT_FALSE(system.DestroyBody(worldHandle, bodyHandles[0]));

        AZStd::array<ConstraintHandle, 1> constraintHandles;
        const QueryResult constraintResult =
            system.GetRagdollConstraints(worldHandle, ragdollHandle, constraintHandles);
        EXPECT_EQ(constraintResult.m_hitCount, 1);
        EXPECT_EQ(constraintResult.m_requiredHitCount, 1);
        EXPECT_TRUE(constraintHandles[0]);
        ConstraintState constraintState;
        ASSERT_TRUE(system.GetConstraintState(worldHandle, constraintHandles[0], constraintState));
        EXPECT_EQ(constraintState.m_solver.m_priority, 7);
        EXPECT_FALSE(system.DestroyConstraint(worldHandle, constraintHandles[0]));
        EXPECT_TRUE(system.IsConstraintInSimulation(worldHandle, constraintHandles[0]));
        EXPECT_FALSE(system.RemoveConstraintFromSimulation(worldHandle, constraintHandles[0]));
        EXPECT_FALSE(system.RemoveConstraintsFromSimulation(worldHandle, constraintHandles));
        EXPECT_FALSE(system.DestroyConstraints(worldHandle, constraintHandles));
        EXPECT_TRUE(system.IsConstraintInSimulation(worldHandle, constraintHandles[0]));

        RagdollState state;
        ASSERT_TRUE(system.GetRagdollState(worldHandle, ragdollHandle, state));
        EXPECT_EQ(state.m_definitionHandle, definitionHandle);
        EXPECT_EQ(state.m_entityId, ragdollConfiguration.m_entityId);
        EXPECT_EQ(state.m_bodyCount, 2);
        EXPECT_EQ(state.m_constraintCount, 1);
        EXPECT_NEAR(state.m_rootTransform.m_position.m_z, 3.0, 1.0e-5);
        EXPECT_TRUE(state.m_isInSimulation);

        EXPECT_TRUE(system.SetRagdollCollisionGroupId(worldHandle, ragdollHandle, 77));
        ASSERT_TRUE(system.GetRagdollState(worldHandle, ragdollHandle, state));
        EXPECT_EQ(state.m_collisionGroupId, 77);

        AZStd::array<AZ::Transform, 2> pose;
        WorldPosition rootPosition;
        const QueryResult poseResult =
            system.GetRagdollPose(worldHandle, ragdollHandle, rootPosition, pose);
        ASSERT_EQ(poseResult.m_hitCount, 2);
        EXPECT_NEAR(rootPosition.m_z, 3.0, 1.0e-5);
        EXPECT_NEAR(pose[1].GetTranslation().GetZ(), 1.0f, 1.0e-5f);

        pose[1].SetTranslation(AZ::Vector3(0.0f, 0.0f, 1.5f));
        EXPECT_TRUE(system.SetRagdollPose(worldHandle, ragdollHandle, rootPosition, pose));
        EXPECT_TRUE(system.DriveRagdollKinematically(
            worldHandle,
            ragdollHandle,
            rootPosition,
            pose,
            1.0f / 60.0f));
        EXPECT_TRUE(system.DriveRagdollMotors(worldHandle, ragdollHandle, pose));
        EXPECT_TRUE(system.DriveRagdollMotors(
            worldHandle,
            ragdollHandle,
            pose,
            pose,
            1.0f / 60.0f));
        EXPECT_TRUE(system.SetRagdollVelocity(
            worldHandle,
            ragdollHandle,
            AZ::Vector3(1.0f, 0.0f, 0.0f),
            AZ::Vector3::CreateZero()));
        EXPECT_TRUE(system.SetRagdollLinearVelocity(
            worldHandle,
            ragdollHandle,
            AZ::Vector3(2.0f, 0.0f, 0.0f)));
        EXPECT_FALSE(system.SetRagdollLinearVelocity(
            worldHandle,
            ragdollHandle,
            AZ::Vector3(AZStd::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f)));

        AZ::Vector3 preservedLinearVelocity;
        ASSERT_TRUE(system.GetBodyLinearVelocity(worldHandle, bodyHandles[0], preservedLinearVelocity));
        EXPECT_TRUE(system.IsRagdollInSimulation(worldHandle, ragdollHandle));
        EXPECT_TRUE(system.RemoveRagdollFromSimulation(worldHandle, ragdollHandle));
        EXPECT_FALSE(system.RemoveRagdollFromSimulation(worldHandle, ragdollHandle));
        EXPECT_FALSE(system.IsRagdollInSimulation(worldHandle, ragdollHandle));
        EXPECT_TRUE(system.IsValid(worldHandle, ragdollHandle));
        EXPECT_TRUE(system.IsValid(worldHandle, bodyHandles[0]));
        EXPECT_TRUE(system.IsValid(worldHandle, constraintHandles[0]));
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, bodyHandles[0]));
        EXPECT_FALSE(system.IsConstraintInSimulation(worldHandle, constraintHandles[0]));
        EXPECT_FALSE(system.ActivateRagdoll(worldHandle, ragdollHandle));
        AZ::Vector3 removedLinearVelocity;
        ASSERT_TRUE(system.GetBodyLinearVelocity(worldHandle, bodyHandles[0], removedLinearVelocity));
        EXPECT_TRUE(removedLinearVelocity.IsClose(preservedLinearVelocity));
        ASSERT_TRUE(system.GetRagdollState(worldHandle, ragdollHandle, state));
        EXPECT_FALSE(state.m_isActive);
        EXPECT_FALSE(state.m_isInSimulation);

        EXPECT_TRUE(system.AddRagdollToSimulation(worldHandle, ragdollHandle, true));
        EXPECT_FALSE(system.AddRagdollToSimulation(worldHandle, ragdollHandle, true));
        EXPECT_TRUE(system.IsRagdollInSimulation(worldHandle, ragdollHandle));
        EXPECT_TRUE(system.IsBodyInSimulation(worldHandle, bodyHandles[0]));
        EXPECT_TRUE(system.IsConstraintInSimulation(worldHandle, constraintHandles[0]));
        AZ::Vector3 restoredLinearVelocity;
        ASSERT_TRUE(system.GetBodyLinearVelocity(worldHandle, bodyHandles[0], restoredLinearVelocity));
        EXPECT_NEAR(restoredLinearVelocity.GetX(), preservedLinearVelocity.GetX(), 1.0e-6f);
        EXPECT_NEAR(restoredLinearVelocity.GetY(), preservedLinearVelocity.GetY(), 1.0e-6f);
        EXPECT_NEAR(restoredLinearVelocity.GetZ(), preservedLinearVelocity.GetZ(), 1.0e-6f);

        EXPECT_TRUE(system.AddRagdollLinearVelocity(
            worldHandle,
            ragdollHandle,
            AZ::Vector3(0.0f, 1.0f, 0.0f)));
        EXPECT_TRUE(system.AddRagdollImpulse(
            worldHandle,
            ragdollHandle,
            AZ::Vector3(0.0f, 0.0f, 1.0f)));
        EXPECT_TRUE(system.ResetRagdollWarmStart(worldHandle, ragdollHandle));
        EXPECT_TRUE(system.ActivateRagdoll(worldHandle, ragdollHandle));
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        EXPECT_TRUE(system.RemoveRagdollFromSimulation(worldHandle, ragdollHandle));

        ShapeConfiguration externalShapeConfiguration;
        externalShapeConfiguration.m_geometry = BoxShapeConfiguration{};
        const ShapeHandle externalShapeHandle = system.CreateShape(worldHandle, externalShapeConfiguration);
        ASSERT_TRUE(externalShapeHandle);
        BodyConfiguration externalBodyConfiguration;
        externalBodyConfiguration.m_shapeHandle = externalShapeHandle;
        externalBodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle externalBodyHandle = system.CreateBody(worldHandle, externalBodyConfiguration);
        ASSERT_TRUE(externalBodyHandle);
        ConstraintConfiguration externalConstraintConfiguration;
        externalConstraintConfiguration.m_firstBodyHandle = bodyHandles[0];
        externalConstraintConfiguration.m_secondBodyHandle = externalBodyHandle;
        externalConstraintConfiguration.m_geometry = FixedConstraintConfiguration{};
        const ConstraintHandle externalConstraintHandle =
            system.CreateConstraint(worldHandle, externalConstraintConfiguration);
        ASSERT_TRUE(externalConstraintHandle);

        EXPECT_FALSE(system.DestroyRagdoll(worldHandle, ragdollHandle));
        EXPECT_TRUE(system.IsValid(worldHandle, ragdollHandle));
        EXPECT_TRUE(system.IsValid(worldHandle, bodyHandles[0]));
        EXPECT_TRUE(system.IsValid(worldHandle, constraintHandles[0]));

        EXPECT_TRUE(system.DestroyConstraint(worldHandle, externalConstraintHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, externalBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, externalShapeHandle));
        EXPECT_TRUE(system.DestroyRagdoll(worldHandle, ragdollHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, ragdollHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, bodyHandles[0]));
        EXPECT_FALSE(system.IsValid(worldHandle, constraintHandles[0]));
        EXPECT_TRUE(system.DestroyRagdollDefinition(worldHandle, definitionHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroySkeletonDefinition(skeletonHandle));
    }

    TEST(SimulationTests, SoftBodyDefinitionsAreSharedAndTypeSafe)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const MaterialHandle materialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(materialHandle);
        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(-1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        definitionConfiguration.m_faces = {{.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2}};
        definitionConfiguration.m_materials = {materialHandle};
        definitionConfiguration.m_edgeConstraints = {
            {.m_firstVertex = 0, .m_secondVertex = 1},
            {.m_firstVertex = 1, .m_secondVertex = 2},
            {.m_firstVertex = 2, .m_secondVertex = 0},
        };
        definitionConfiguration.m_rodStretchShearConstraints = {
            {.m_firstVertex = 0, .m_secondVertex = 1},
            {.m_firstVertex = 1, .m_secondVertex = 2},
        };
        definitionConfiguration.m_rodBendTwistConstraints = {
            {.m_firstRod = 0, .m_secondRod = 1},
        };
        definitionConfiguration.m_inverseBinds = {
            {.m_transform = AZ::Transform::CreateIdentity(), .m_jointIndex = 0},
        };
        SoftBodySkinConstraint skinConstraint;
        skinConstraint.m_weights[0] = {.m_inverseBindIndex = 0, .m_weight = 1.0f};
        skinConstraint.m_vertex = 0;
        skinConstraint.m_maximumDistance = 0.0f;
        definitionConfiguration.m_skinConstraints = {skinConstraint};
        definitionConfiguration.m_createFaceConstraints = false;
        SoftBodyOptimizationRemap optimizationRemap;
        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration, &optimizationRemap);
        ASSERT_TRUE(definitionHandle);
        EXPECT_EQ(optimizationRemap.m_edgeConstraints.size(), 3);
        EXPECT_EQ(optimizationRemap.m_rodBendTwistConstraints.size(), 1);
        EXPECT_EQ(optimizationRemap.m_rodStretchShearConstraints.size(), 2);
        EXPECT_EQ(optimizationRemap.m_skinConstraints.size(), 1);
        EXPECT_FALSE(system.DestroyMaterial(materialHandle));

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        SoftBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_definitionHandle = definitionHandle;
        bodyConfiguration.m_transform.m_position.m_z = 5.0;
        const BodyHandle bodyHandle = system.CreateSoftBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);
        EXPECT_FALSE(system.DestroySoftBodyDefinition(definitionHandle));
        EXPECT_FALSE(system.AddImpulse(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX()));

        AZ::Vector3 linearVelocity;
        ASSERT_TRUE(system.GetBodyLinearVelocity(worldHandle, bodyHandle, linearVelocity));
        EXPECT_TRUE(linearVelocity.IsClose(AZ::Vector3::CreateZero()));
        ASSERT_TRUE(system.AddForce(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX(3.0f)));

        float initialInverseMass = 0.0f;
        ASSERT_TRUE(system.GetBodyInverseMass(worldHandle, bodyHandle, initialInverseMass));
        EXPECT_NEAR(initialInverseMass, 1.0f / 3.0f, 1.0e-5f);

        AZ::Matrix3x3 initialInverseInertia;
        ASSERT_TRUE(system.GetBodyInverseInertia(worldHandle, bodyHandle, initialInverseInertia));
        EXPECT_FALSE(initialInverseInertia.IsClose(AZ::Matrix3x3::CreateZero()));

        AZStd::array<SoftBodyVertex, 2> truncatedVertices;
        const QueryResult truncatedResult =
            system.GetSoftBodyVertices(worldHandle, bodyHandle, truncatedVertices);
        EXPECT_EQ(truncatedResult.m_hitCount, 2);
        EXPECT_EQ(truncatedResult.m_requiredHitCount, 3);
        EXPECT_FALSE(truncatedResult.IsComplete());

        AZStd::array<SoftBodyFace, 1> faces;
        const QueryResult faceResult = system.GetSoftBodyFaces(worldHandle, bodyHandle, faces);
        ASSERT_TRUE(faceResult.IsComplete());
        ASSERT_EQ(faceResult.m_hitCount, 1);
        EXPECT_EQ(faces[0].m_firstVertex, 0);
        EXPECT_EQ(faces[0].m_secondVertex, 1);
        EXPECT_EQ(faces[0].m_thirdVertex, 2);
        EXPECT_EQ(faces[0].m_materialIndex, 0);

        AZStd::array<MaterialHandle, 1> materials;
        const QueryResult materialResult = system.GetSoftBodyMaterials(worldHandle, bodyHandle, materials);
        ASSERT_TRUE(materialResult.IsComplete());
        ASSERT_EQ(materialResult.m_hitCount, 1);
        EXPECT_EQ(materials[0], materialHandle);

        AZ::Aabb localBounds = AZ::Aabb::CreateNull();
        ASSERT_TRUE(system.GetSoftBodyLocalBounds(worldHandle, bodyHandle, localBounds));
        EXPECT_TRUE(localBounds.IsValid());
        EXPECT_TRUE(localBounds.Contains(AZ::Vector3(-1.0f, 0.0f, 0.0f)));
        EXPECT_TRUE(localBounds.Contains(AZ::Vector3(1.0f, 0.0f, 0.0f)));
        EXPECT_TRUE(localBounds.Contains(AZ::Vector3(0.0f, 1.0f, 0.0f)));

        EXPECT_TRUE(system.SetSoftBodyVertexInverseMass(worldHandle, bodyHandle, 0, 0.0f));
        EXPECT_TRUE(system.SetSoftBodyVertexVelocity(
            worldHandle,
            bodyHandle,
            1,
            AZ::Vector3::CreateAxisZ(-1.0f)));
        EXPECT_FALSE(system.SetSoftBodyVertexInverseMass(worldHandle, bodyHandle, 3, 1.0f));
        EXPECT_FALSE(system.SetSoftBodyVertexVelocity(
            worldHandle,
            bodyHandle,
            3,
            AZ::Vector3::CreateZero()));

        const AZStd::array inverseMasses = {0.25f, 0.5f};
        EXPECT_TRUE(system.SetSoftBodyVertexInverseMasses(
            worldHandle,
            bodyHandle,
            1,
            inverseMasses));
        const AZStd::array invalidInverseMasses = {
            0.75f,
            AZStd::numeric_limits<float>::quiet_NaN(),
        };
        EXPECT_FALSE(system.SetSoftBodyVertexInverseMasses(
            worldHandle,
            bodyHandle,
            1,
            invalidInverseMasses));
        EXPECT_FALSE(system.SetSoftBodyVertexInverseMasses(
            worldHandle,
            bodyHandle,
            2,
            inverseMasses));

        ASSERT_TRUE(system.RecalculateSoftBodyMassProperties(worldHandle, bodyHandle, false));
        float updatedInverseMass = 1.0f;
        ASSERT_TRUE(system.GetBodyInverseMass(worldHandle, bodyHandle, updatedInverseMass));
        EXPECT_FLOAT_EQ(updatedInverseMass, 0.0f);

        AZ::Matrix3x3 updatedInverseInertia;
        ASSERT_TRUE(system.GetBodyInverseInertia(worldHandle, bodyHandle, updatedInverseInertia));
        EXPECT_TRUE(updatedInverseInertia.IsClose(AZ::Matrix3x3::CreateZero()));

        const AZStd::array velocities = {
            AZ::Vector3::CreateAxisZ(-1.0f),
            AZ::Vector3::CreateAxisZ(-2.0f),
            AZ::Vector3::CreateAxisZ(-3.0f),
        };
        EXPECT_TRUE(system.SetSoftBodyVertexVelocities(
            worldHandle,
            bodyHandle,
            0,
            velocities));
        const AZStd::array invalidVelocities = {
            AZ::Vector3::CreateAxisX(4.0f),
            AZ::Vector3(
                AZStd::numeric_limits<float>::quiet_NaN(),
                0.0f,
                0.0f),
        };
        EXPECT_FALSE(system.SetSoftBodyVertexVelocities(
            worldHandle,
            bodyHandle,
            1,
            invalidVelocities));
        EXPECT_FALSE(system.SetSoftBodyVertexVelocities(
            worldHandle,
            bodyHandle,
            2,
            velocities));

        AZStd::array<SoftBodyVertex, 3> updatedVertices;
        const QueryResult updatedVertexResult =
            system.GetSoftBodyVertices(worldHandle, bodyHandle, updatedVertices);
        ASSERT_TRUE(updatedVertexResult.IsComplete());
        EXPECT_FLOAT_EQ(updatedVertices[0].m_inverseMass, 0.0f);
        EXPECT_FLOAT_EQ(updatedVertices[1].m_inverseMass, 0.25f);
        EXPECT_FLOAT_EQ(updatedVertices[2].m_inverseMass, 0.5f);
        EXPECT_TRUE(updatedVertices[0].m_velocity.IsClose(velocities[0]));
        EXPECT_TRUE(updatedVertices[1].m_velocity.IsClose(velocities[1]));
        EXPECT_TRUE(updatedVertices[2].m_velocity.IsClose(velocities[2]));

        AZStd::array<SoftBodyRodState, 2> rods;
        const QueryResult rodResult = system.GetSoftBodyRodStates(worldHandle, bodyHandle, rods);
        EXPECT_TRUE(rodResult.IsComplete());
        EXPECT_EQ(rodResult.m_hitCount, 2);

        SoftBodyRuntimeConfiguration runtimeConfiguration;
        ASSERT_TRUE(system.GetSoftBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            runtimeConfiguration));
        runtimeConfiguration.m_enableSkinConstraints = false;
        runtimeConfiguration.m_iterationCount = 7;
        runtimeConfiguration.m_pressure = 2.0f;
        runtimeConfiguration.m_skinnedMaximumDistanceMultiplier = 0.75f;
        runtimeConfiguration.m_vertexRadius = 0.01f;
        ASSERT_TRUE(system.UpdateSoftBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            runtimeConfiguration));

        SoftBodyRuntimeConfiguration updatedRuntimeConfiguration;
        ASSERT_TRUE(system.GetSoftBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            updatedRuntimeConfiguration));
        EXPECT_FALSE(updatedRuntimeConfiguration.m_enableSkinConstraints);
        EXPECT_EQ(updatedRuntimeConfiguration.m_iterationCount, 7);
        EXPECT_FLOAT_EQ(updatedRuntimeConfiguration.m_pressure, 2.0f);
        EXPECT_FLOAT_EQ(updatedRuntimeConfiguration.m_skinnedMaximumDistanceMultiplier, 0.75f);
        EXPECT_FLOAT_EQ(updatedRuntimeConfiguration.m_vertexRadius, 0.01f);

        updatedRuntimeConfiguration.m_pressure = 0.0f;
        ASSERT_TRUE(system.UpdateSoftBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            updatedRuntimeConfiguration));

        const AZStd::array<AZ::Transform, 1> jointTransformsRelativeToCenterOfMass = {
            AZ::Transform::CreateIdentity(),
        };
        EXPECT_TRUE(system.SkinSoftBody(
            worldHandle,
            bodyHandle,
            jointTransformsRelativeToCenterOfMass,
            true));

        float volume = 0.0f;
        EXPECT_TRUE(system.GetSoftBodyVolume(worldHandle, bodyHandle, volume));
        EXPECT_TRUE(AZ::IsFiniteFloat(volume));

        for (AZ::u32 step = 0; step < 10; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_EQ(state.m_kind, BodyKind::Soft);
        EXPECT_FALSE(state.m_shapeHandle);
        EXPECT_LT(state.m_transform.m_position.m_z, 5.0);

        AZStd::array<SoftBodyVertex, 3> vertices;
        const QueryResult vertexResult = system.GetSoftBodyVertices(worldHandle, bodyHandle, vertices);
        EXPECT_TRUE(vertexResult.IsComplete());
        EXPECT_EQ(vertexResult.m_hitCount, 3);
        EXPECT_FLOAT_EQ(vertices[0].m_inverseMass, 0.0f);

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, SoftBodiesExposeAverageVelocityAndAcceptGlobalForce)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(-1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        definitionConfiguration.m_faces = {{.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2}};
        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        SoftBodyConfiguration configuration;
        configuration.m_definitionHandle = definitionHandle;
        configuration.m_userData = 0x1234'5678'9abc'def0;
        const BodyHandle bodyHandle = system.CreateSoftBody(worldHandle, configuration);
        ASSERT_TRUE(bodyHandle);

        AZ::u64 userData = 0;
        ASSERT_TRUE(system.GetBodyUserData(worldHandle, bodyHandle, userData));
        EXPECT_EQ(userData, configuration.m_userData);
        constexpr AZ::u64 updatedUserData = 0x1111'2222'3333'4444;
        ASSERT_TRUE(system.SetBodyUserData(worldHandle, bodyHandle, updatedUserData));
        ASSERT_TRUE(system.GetBodyUserData(worldHandle, bodyHandle, userData));
        EXPECT_EQ(userData, updatedUserData);
        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_EQ(state.m_userData, updatedUserData);

        AZ::Vector3 linearVelocity;
        ASSERT_TRUE(system.GetBodyLinearVelocity(worldHandle, bodyHandle, linearVelocity));
        EXPECT_TRUE(linearVelocity.IsClose(AZ::Vector3::CreateZero()));
        ASSERT_TRUE(system.AddForce(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX(3.0f)));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.GetBodyLinearVelocity(worldHandle, bodyHandle, linearVelocity));
        EXPECT_GT(linearVelocity.GetX(), 0.0f);

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
    }

    TEST(SimulationTests, SkinnedSoftBodiesRemainFiniteUntilTheirFirstPose)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(0.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        definitionConfiguration.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        definitionConfiguration.m_inverseBinds = {
            {.m_transform = AZ::Transform::CreateIdentity(), .m_jointIndex = 0},
        };
        for (AZ::u32 vertexIndex = 0; vertexIndex < definitionConfiguration.m_vertices.size(); ++vertexIndex)
        {
            SoftBodySkinConstraint constraint;
            constraint.m_weights[0] = {.m_inverseBindIndex = 0, .m_weight = 1.0f};
            constraint.m_vertex = vertexIndex;
            definitionConfiguration.m_skinConstraints.push_back(constraint);
        }

        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        SoftBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_definitionHandle = definitionHandle;
        const BodyHandle bodyHandle = system.CreateSoftBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        SoftBodyRuntimeConfiguration runtimeConfiguration;
        ASSERT_TRUE(system.GetSoftBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            runtimeConfiguration));
        EXPECT_TRUE(runtimeConfiguration.m_enableSkinConstraints);

        const StateSnapshotHandle bodySnapshotHandle =
            system.CaptureBodyState(worldHandle, bodyHandle);
        ASSERT_TRUE(bodySnapshotHandle);
        const StateSnapshotHandle worldSnapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(worldSnapshotHandle);

        const auto expectFiniteVertices = [&system, worldHandle, bodyHandle]()
        {
            AZStd::array<SoftBodyVertex, 3> vertices;
            EXPECT_TRUE(system.GetSoftBodyVertices(worldHandle, bodyHandle, vertices).IsComplete());
            for (const SoftBodyVertex& vertex : vertices)
            {
                EXPECT_TRUE(vertex.m_position.IsFinite());
                EXPECT_TRUE(vertex.m_velocity.IsFinite());
            }
        };

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        expectFiniteVertices();

        const AZStd::array pose = {AZ::Transform::CreateIdentity()};
        ASSERT_TRUE(system.SkinSoftBody(worldHandle, bodyHandle, pose, true));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        expectFiniteVertices();

        ASSERT_TRUE(system.RestoreBodyState(worldHandle, bodySnapshotHandle));
        ASSERT_TRUE(system.GetSoftBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            runtimeConfiguration));
        EXPECT_TRUE(runtimeConfiguration.m_enableSkinConstraints);
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        expectFiniteVertices();

        ASSERT_TRUE(system.SkinSoftBody(worldHandle, bodyHandle, pose, true));
        ASSERT_TRUE(system.RestoreWorldState(worldHandle, worldSnapshotHandle));
        ASSERT_TRUE(system.GetSoftBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            runtimeConfiguration));
        EXPECT_TRUE(runtimeConfiguration.m_enableSkinConstraints);
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        expectFiniteVertices();

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, bodySnapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, worldSnapshotHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
    }

    TEST(SimulationTests, SoftBodySkinningAppliesBlendedInverseBindsAndRestoresExactly)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(0.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        definitionConfiguration.m_faces = {{.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2}};

        AZ::Transform secondInverseBind = AZ::Transform::CreateIdentity();
        secondInverseBind.SetTranslation(AZ::Vector3::CreateAxisX(-2.0f));
        definitionConfiguration.m_inverseBinds = {
            {.m_transform = AZ::Transform::CreateIdentity(), .m_jointIndex = 0},
            {.m_transform = secondInverseBind, .m_jointIndex = 1},
        };
        for (AZ::u32 vertexIndex = 0; vertexIndex < definitionConfiguration.m_vertices.size(); ++vertexIndex)
        {
            SoftBodySkinConstraint constraint;
            constraint.m_weights[0] = {.m_inverseBindIndex = 0, .m_weight = 0.5f};
            constraint.m_weights[1] = {.m_inverseBindIndex = 1, .m_weight = 0.5f};
            constraint.m_vertex = vertexIndex;
            constraint.m_maximumDistance = 0.0f;
            definitionConfiguration.m_skinConstraints.push_back(constraint);
        }

        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        SoftBodyConfiguration configuration;
        configuration.m_definitionHandle = definitionHandle;
        configuration.m_manualUpdate = true;
        const BodyHandle bodyHandle = system.CreateSoftBody(worldHandle, configuration);
        ASSERT_TRUE(bodyHandle);

        AZ::Transform secondBindPose = AZ::Transform::CreateIdentity();
        secondBindPose.SetTranslation(AZ::Vector3::CreateAxisX(2.0f));
        const AZStd::array bindPose = {
            AZ::Transform::CreateIdentity(),
            secondBindPose,
        };
        ASSERT_TRUE(system.SkinSoftBody(worldHandle, bodyHandle, bindPose, true));

        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);

        AZ::Transform firstTargetPose = AZ::Transform::CreateIdentity();
        firstTargetPose.SetTranslation(AZ::Vector3::CreateAxisX(2.0f));
        AZ::Transform secondTargetPose = AZ::Transform::CreateIdentity();
        secondTargetPose.SetTranslation(AZ::Vector3::CreateAxisX(4.0f));
        const AZStd::array targetPose = {
            firstTargetPose,
            secondTargetPose,
        };
        ASSERT_TRUE(system.SkinSoftBody(worldHandle, bodyHandle, targetPose, true));

        AZStd::array<SoftBodyVertex, 3> skinnedVertices;
        ASSERT_TRUE(system.GetSoftBodyVertices(worldHandle, bodyHandle, skinnedVertices).IsComplete());
        for (size_t vertexIndex = 0; vertexIndex < skinnedVertices.size(); ++vertexIndex)
        {
            const AZ::Vector3 expectedPosition =
                definitionConfiguration.m_vertices[vertexIndex].m_position + AZ::Vector3::CreateAxisX(2.0f);
            EXPECT_TRUE(skinnedVertices[vertexIndex].m_position.IsClose(expectedPosition));
            EXPECT_TRUE(skinnedVertices[vertexIndex].m_velocity.IsClose(AZ::Vector3::CreateZero()));
        }

        WorldStateDigest firstTargetDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, firstTargetDigest));
        ASSERT_TRUE(system.RestoreWorldState(worldHandle, snapshotHandle));
        ASSERT_TRUE(system.SkinSoftBody(worldHandle, bodyHandle, targetPose, true));
        WorldStateDigest secondTargetDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, secondTargetDigest));
        EXPECT_EQ(secondTargetDigest, firstTargetDigest);

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
    }

    TEST(SimulationTests, SoftBodySkinConstraintsEnforceDistanceBackstopAndRuntimeControls)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(0.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        definitionConfiguration.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        definitionConfiguration.m_inverseBinds = {
            {.m_transform = AZ::Transform::CreateIdentity(), .m_jointIndex = 0},
        };
        for (AZ::u32 vertexIndex = 0; vertexIndex < definitionConfiguration.m_vertices.size(); ++vertexIndex)
        {
            SoftBodySkinConstraint constraint;
            constraint.m_weights[0] = {.m_inverseBindIndex = 0, .m_weight = 1.0f};
            constraint.m_vertex = vertexIndex;
            constraint.m_backstopDistance = 0.0f;
            constraint.m_backstopRadius = 1.0f;
            constraint.m_maximumDistance = 0.25f;
            definitionConfiguration.m_skinConstraints.push_back(constraint);
        }

        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        SoftBodyConfiguration configuration;
        configuration.m_definitionHandle = definitionHandle;
        configuration.m_linearDamping = 0.0f;
        configuration.m_manualUpdate = true;
        configuration.m_updatePosition = false;
        const BodyHandle bodyHandle = system.CreateSoftBody(worldHandle, configuration);
        ASSERT_TRUE(bodyHandle);

        const AZStd::array bindPose = {AZ::Transform::CreateIdentity()};
        ASSERT_TRUE(system.SkinSoftBody(worldHandle, bodyHandle, bindPose, true));
        const AZStd::array drivenVelocities = {
            AZ::Vector3(2.0f, 0.0f, -2.0f),
            AZ::Vector3(2.0f, 0.0f, -2.0f),
            AZ::Vector3(2.0f, 0.0f, -2.0f),
        };
        ASSERT_TRUE(system.SetSoftBodyVertexVelocities(
            worldHandle,
            bodyHandle,
            0,
            drivenVelocities));
        ASSERT_TRUE(system.SkinSoftBody(worldHandle, bodyHandle, bindPose, false));
        ASSERT_TRUE(system.UpdateSoftBodyManually(worldHandle, bodyHandle, 0.5f));

        AZStd::array<SoftBodyVertex, 3> constrainedVertices;
        ASSERT_TRUE(system.GetSoftBodyVertices(worldHandle, bodyHandle, constrainedVertices).IsComplete());
        for (size_t vertexIndex = 0; vertexIndex < constrainedVertices.size(); ++vertexIndex)
        {
            const AZ::Vector3 displacement =
                constrainedVertices[vertexIndex].m_position
                - definitionConfiguration.m_vertices[vertexIndex].m_position;
            EXPECT_LE(displacement.GetLength(), 0.2501f);
            const AZ::Vector3 backstopCenter =
                definitionConfiguration.m_vertices[vertexIndex].m_position
                - AZ::Vector3::CreateAxisZ();
            EXPECT_GE(
                (constrainedVertices[vertexIndex].m_position - backstopCenter).GetLength(),
                0.9999f);
        }

        SoftBodyRuntimeConfiguration runtimeConfiguration;
        ASSERT_TRUE(system.GetSoftBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            runtimeConfiguration));
        runtimeConfiguration.m_enableSkinConstraints = false;
        ASSERT_TRUE(system.UpdateSoftBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            runtimeConfiguration));
        ASSERT_TRUE(system.SkinSoftBody(worldHandle, bodyHandle, bindPose, true));
        ASSERT_TRUE(system.SetSoftBodyVertexVelocities(
            worldHandle,
            bodyHandle,
            0,
            drivenVelocities));
        ASSERT_TRUE(system.SkinSoftBody(worldHandle, bodyHandle, bindPose, false));
        ASSERT_TRUE(system.UpdateSoftBodyManually(worldHandle, bodyHandle, 0.5f));

        AZStd::array<SoftBodyVertex, 3> unconstrainedVertices;
        ASSERT_TRUE(system.GetSoftBodyVertices(worldHandle, bodyHandle, unconstrainedVertices).IsComplete());
        EXPECT_GT(
            (unconstrainedVertices[0].m_position - definitionConfiguration.m_vertices[0].m_position).GetLength(),
            0.25f);

        runtimeConfiguration.m_enableSkinConstraints = true;
        runtimeConfiguration.m_skinnedMaximumDistanceMultiplier = 0.0f;
        ASSERT_TRUE(system.UpdateSoftBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            runtimeConfiguration));
        ASSERT_TRUE(system.SkinSoftBody(worldHandle, bodyHandle, bindPose, true));
        ASSERT_TRUE(system.SetSoftBodyVertexVelocities(
            worldHandle,
            bodyHandle,
            0,
            drivenVelocities));
        ASSERT_TRUE(system.SkinSoftBody(worldHandle, bodyHandle, bindPose, false));
        ASSERT_TRUE(system.UpdateSoftBodyManually(worldHandle, bodyHandle, 0.5f));

        AZStd::array<SoftBodyVertex, 3> hardSkinnedVertices;
        ASSERT_TRUE(system.GetSoftBodyVertices(worldHandle, bodyHandle, hardSkinnedVertices).IsComplete());
        for (size_t vertexIndex = 0; vertexIndex < hardSkinnedVertices.size(); ++vertexIndex)
        {
            EXPECT_TRUE(hardSkinnedVertices[vertexIndex].m_position.IsClose(
                definitionConfiguration.m_vertices[vertexIndex].m_position));
            EXPECT_TRUE(hardSkinnedVertices[vertexIndex].m_velocity.IsClose(AZ::Vector3::CreateZero()));
        }

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
    }

    TEST(SimulationTests, SoftBodyDefinitionArchivesRoundTripOptimizedNativeData)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const MaterialHandle materialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(materialHandle);

        SoftBodyDefinitionConfiguration configuration;
        configuration.m_vertices = {
            {.m_position = AZ::Vector3(0.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        configuration.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        configuration.m_materials = {materialHandle};
        configuration.m_edgeConstraints = {
            {.m_firstVertex = 0, .m_secondVertex = 1},
            {.m_firstVertex = 1, .m_secondVertex = 2},
            {.m_firstVertex = 2, .m_secondVertex = 0},
        };
        const SoftBodyDefinitionHandle sourceHandle = system.CreateSoftBodyDefinition(configuration);
        ASSERT_TRUE(sourceHandle);
        SoftBodyDefinitionState sourceState;
        ASSERT_TRUE(system.GetSoftBodyDefinitionState(sourceHandle, sourceState));

        SoftBodyDefinitionArchive archive;
        AZStd::vector<MaterialHandle> materialHandles;
        ASSERT_TRUE(system.ExportSoftBodyDefinition(sourceHandle, archive, materialHandles));
        EXPECT_FALSE(archive.m_binaryState.empty());
        EXPECT_NE(archive.m_buildFingerprint, 0);
        EXPECT_NE(archive.m_contentHash, 0);
        EXPECT_NE(archive.m_formatVersion, 0);
        ASSERT_EQ(materialHandles.size(), 1);
        EXPECT_EQ(materialHandles[0], materialHandle);

        SoftBodyDefinitionArchive trailingDataArchive = archive;
        trailingDataArchive.m_binaryState.push_back(0);
        trailingDataArchive.m_contentHash = static_cast<AZ::u64>(AZ::TypeHash64(
            trailingDataArchive.m_binaryState.data(),
            trailingDataArchive.m_binaryState.size()));
        EXPECT_FALSE(system.ImportSoftBodyDefinition(trailingDataArchive, materialHandles));

        SoftBodyDefinitionArchive corruptedArchive = archive;
        ++corruptedArchive.m_contentHash;
        EXPECT_FALSE(system.ImportSoftBodyDefinition(corruptedArchive, materialHandles));

        const SoftBodyDefinitionHandle importedHandle =
            system.ImportSoftBodyDefinition(archive, materialHandles);
        ASSERT_TRUE(importedHandle);
        SoftBodyDefinitionState importedState;
        ASSERT_TRUE(system.GetSoftBodyDefinitionState(importedHandle, importedState));
        EXPECT_EQ(importedState.m_vertexCount, sourceState.m_vertexCount);
        EXPECT_EQ(importedState.m_faceCount, sourceState.m_faceCount);
        EXPECT_EQ(importedState.m_materialCount, sourceState.m_materialCount);
        EXPECT_EQ(importedState.m_edgeConstraintCount, sourceState.m_edgeConstraintCount);

        EXPECT_TRUE(system.DestroySoftBodyDefinition(importedHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(sourceHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, AppliesCompleteSoftBodyConfigurationOutsideSimulation)
    {
        NameDictionaryScope nameDictionaryScope;
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const MaterialHandle materialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(materialHandle);

        SoftBodyDefinitionConfiguration triangleDefinition;
        triangleDefinition.m_vertices = {
            {.m_position = AZ::Vector3(0.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        triangleDefinition.m_faces = {{.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2}};
        triangleDefinition.m_materials = {materialHandle};
        const SoftBodyDefinitionHandle triangleDefinitionHandle =
            system.CreateSoftBodyDefinition(triangleDefinition);
        ASSERT_TRUE(triangleDefinitionHandle);

        SoftBodyDefinitionConfiguration quadDefinition;
        quadDefinition.m_vertices = {
            {.m_position = AZ::Vector3(0.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 1.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        quadDefinition.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
            {.m_firstVertex = 0, .m_secondVertex = 2, .m_thirdVertex = 3},
        };
        quadDefinition.m_materials = {materialHandle};
        const SoftBodyDefinitionHandle quadDefinitionHandle =
            system.CreateSoftBodyDefinition(quadDefinition);
        ASSERT_TRUE(quadDefinitionHandle);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        SoftBodyConfiguration configuration;
        configuration.m_definitionHandle = triangleDefinitionHandle;
        configuration.m_manualUpdate = true;
        const BodyHandle bodyHandle = system.CreateSoftBody(worldHandle, configuration);
        ASSERT_TRUE(bodyHandle);

        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);

        configuration.m_definitionHandle = quadDefinitionHandle;
        configuration.m_transform.m_position = {.m_x = 4.0, .m_y = 3.0, .m_z = 2.0};
        configuration.m_transform.m_rotation = AZ::Quaternion::CreateRotationX(0.25f);
        configuration.m_entityId = AZ::EntityId(92);
        configuration.m_name = AZ::Name("reconfigured_soft_body");
        configuration.m_friction = 0.6f;
        configuration.m_gravityFactor = 0.4f;
        configuration.m_linearDamping = 0.3f;
        configuration.m_maximumLinearVelocity = 100.0f;
        configuration.m_pressure = 2.0f;
        configuration.m_restitution = 0.2f;
        configuration.m_skinnedMaximumDistanceMultiplier = 0.75f;
        configuration.m_vertexRadius = 0.05f;
        configuration.m_iterationCount = 8;
        configuration.m_allowSleeping = false;
        configuration.m_enableSkinConstraints = false;
        configuration.m_facesDoubleSided = true;
        configuration.m_makeRotationIdentity = false;
        configuration.m_updatePosition = false;
        ASSERT_TRUE(system.ApplySoftBodyConfiguration(worldHandle, bodyHandle, configuration));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(triangleDefinitionHandle));
        EXPECT_FALSE(system.DestroySoftBodyDefinition(quadDefinitionHandle));

        AZStd::array<SoftBodyVertex, 4> vertices;
        const QueryResult vertexResult = system.GetSoftBodyVertices(worldHandle, bodyHandle, vertices);
        EXPECT_TRUE(vertexResult.IsComplete());
        EXPECT_EQ(vertexResult.m_hitCount, 4);

        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_EQ(state.m_entityId, configuration.m_entityId);
        EXPECT_EQ(state.m_name, configuration.m_name);
        EXPECT_NEAR(state.m_transform.m_position.m_x, 4.0, 1.0e-6);
        EXPECT_NEAR(state.m_transform.m_position.m_y, 3.0, 1.0e-6);
        EXPECT_NEAR(state.m_transform.m_position.m_z, 2.0, 1.0e-6);

        SoftBodyRuntimeConfiguration runtime;
        ASSERT_TRUE(system.GetSoftBodyRuntimeConfiguration(worldHandle, bodyHandle, runtime));
        EXPECT_FLOAT_EQ(runtime.m_friction, configuration.m_friction);
        EXPECT_FLOAT_EQ(runtime.m_gravityFactor, configuration.m_gravityFactor);
        EXPECT_FLOAT_EQ(runtime.m_linearDamping, configuration.m_linearDamping);
        EXPECT_FLOAT_EQ(runtime.m_maximumLinearVelocity, configuration.m_maximumLinearVelocity);
        EXPECT_FLOAT_EQ(runtime.m_pressure, configuration.m_pressure);
        EXPECT_FLOAT_EQ(runtime.m_restitution, configuration.m_restitution);
        EXPECT_FLOAT_EQ(
            runtime.m_skinnedMaximumDistanceMultiplier,
            configuration.m_skinnedMaximumDistanceMultiplier);
        EXPECT_FLOAT_EQ(runtime.m_vertexRadius, configuration.m_vertexRadius);
        EXPECT_EQ(runtime.m_iterationCount, configuration.m_iterationCount);
        EXPECT_EQ(runtime.m_allowSleeping, configuration.m_allowSleeping);
        EXPECT_EQ(runtime.m_enableSkinConstraints, configuration.m_enableSkinConstraints);
        EXPECT_EQ(runtime.m_facesDoubleSided, configuration.m_facesDoubleSided);
        EXPECT_EQ(runtime.m_updatePosition, configuration.m_updatePosition);

        SoftBodyConfiguration invalidConfiguration = configuration;
        invalidConfiguration.m_manualUpdate = false;
        EXPECT_FALSE(system.ApplySoftBodyConfiguration(worldHandle, bodyHandle, invalidConfiguration));
        ASSERT_TRUE(system.AddBodyToSimulation(worldHandle, bodyHandle, true));
        EXPECT_FALSE(system.ApplySoftBodyConfiguration(worldHandle, bodyHandle, configuration));

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(quadDefinitionHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, SoftBodyDefinitionsExposeComputedConstraintDataWithoutAllocatingOutputs)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const MaterialHandle materialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(materialHandle);

        SoftBodyDefinitionConfiguration configuration;
        configuration.m_vertices = {
            {.m_position = AZ::Vector3(0.0f, 0.0f, 0.0f), .m_inverseMass = 0.0f},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 1.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 1.0f)},
        };
        configuration.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
            {.m_firstVertex = 0, .m_secondVertex = 3, .m_thirdVertex = 1},
        };
        configuration.m_materials = {materialHandle};
        configuration.m_edgeConstraints = {
            {
                .m_firstVertex = 0,
                .m_secondVertex = 1,
                .m_compliance = 0.125f,
                .m_restLength = 2.0f,
            },
        };
        configuration.m_dihedralBendConstraints = {
            {
                .m_firstVertex = 0,
                .m_secondVertex = 1,
                .m_thirdVertex = 2,
                .m_fourthVertex = 3,
                .m_compliance = 0.25f,
                .m_initialAngle = 0.375f,
                .m_calculateInitialAngle = false,
            },
        };
        configuration.m_longRangeConstraints = {
            {
                .m_fixedVertex = 0,
                .m_dynamicVertex = 3,
                .m_maximumDistance = 4.0f,
            },
        };
        configuration.m_rodStretchShearConstraints = {
            {
                .m_firstVertex = 0,
                .m_secondVertex = 1,
                .m_bishopRotation = AZ::Quaternion::CreateIdentity(),
                .m_compliance = 0.5f,
                .m_inverseMass = 0.75f,
                .m_restLength = 1.25f,
                .m_calculateBishopRotation = false,
            },
            {
                .m_firstVertex = 1,
                .m_secondVertex = 2,
                .m_bishopRotation = AZ::Quaternion::CreateIdentity(),
                .m_calculateBishopRotation = false,
            },
        };
        const AZ::Quaternion initialRodRotation =
            AZ::Quaternion::CreateRotationZ(0.25f);
        configuration.m_rodBendTwistConstraints = {
            {
                .m_firstRod = 0,
                .m_secondRod = 1,
                .m_initialRotation = initialRodRotation,
                .m_compliance = 0.625f,
                .m_calculateInitialRotation = false,
            },
        };
        configuration.m_volumeConstraints = {
            {
                .m_firstVertex = 0,
                .m_secondVertex = 1,
                .m_thirdVertex = 2,
                .m_fourthVertex = 3,
                .m_compliance = 0.75f,
                .m_restVolume = 2.5f,
                .m_calculateRestVolume = false,
            },
        };
        configuration.m_inverseBinds = {
            {
                .m_transform = AZ::Transform::CreateTranslation(AZ::Vector3(1.0f, 2.0f, 3.0f)),
                .m_jointIndex = 7,
            },
        };
        SoftBodySkinConstraint skinConstraint;
        skinConstraint.m_weights[0] = {
            .m_inverseBindIndex = 0,
            .m_weight = 1.0f,
        };
        skinConstraint.m_vertex = 3;
        skinConstraint.m_backstopDistance = 0.125f;
        skinConstraint.m_backstopRadius = 0.25f;
        skinConstraint.m_maximumDistance = 0.5f;
        configuration.m_skinConstraints = {skinConstraint};
        configuration.m_createFaceConstraints = false;
        configuration.m_optimize = false;

        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(configuration);
        ASSERT_TRUE(definitionHandle);

        SoftBodyDefinitionState state;
        ASSERT_TRUE(system.GetSoftBodyDefinitionState(definitionHandle, state));
        EXPECT_EQ(state.m_vertexCount, 4);
        EXPECT_EQ(state.m_faceCount, 2);
        EXPECT_EQ(state.m_materialCount, 1);
        EXPECT_EQ(state.m_dihedralBendConstraintCount, 1);
        EXPECT_EQ(state.m_edgeConstraintCount, 1);
        EXPECT_EQ(state.m_longRangeConstraintCount, 1);
        EXPECT_EQ(state.m_rodBendTwistConstraintCount, 1);
        EXPECT_EQ(state.m_rodStretchShearConstraintCount, 2);
        EXPECT_EQ(state.m_skinConstraintCount, 1);
        EXPECT_EQ(state.m_volumeConstraintCount, 1);
        EXPECT_EQ(state.m_inverseBindCount, 1);

        AZStd::array<SoftBodyVertex, 2> vertices;
        const QueryResult vertexResult =
            system.GetSoftBodyDefinitionVertices(definitionHandle, vertices);
        EXPECT_EQ(vertexResult.m_hitCount, 2);
        EXPECT_EQ(vertexResult.m_requiredHitCount, 4);
        EXPECT_FALSE(vertexResult.IsComplete());
        EXPECT_TRUE(vertices[1].m_position.IsClose(AZ::Vector3::CreateAxisX()));

        AZStd::array<SoftBodyFace, 2> faces;
        EXPECT_TRUE(system.GetSoftBodyDefinitionFaces(definitionHandle, faces).IsComplete());
        EXPECT_EQ(faces[1].m_thirdVertex, 1);

        AZStd::array<MaterialHandle, 1> materials;
        EXPECT_TRUE(system.GetSoftBodyDefinitionMaterials(definitionHandle, materials).IsComplete());
        EXPECT_EQ(materials[0], materialHandle);

        AZStd::array<SoftBodyDihedralBendConstraint, 1> dihedralConstraints;
        EXPECT_TRUE(system.GetSoftBodyDefinitionDihedralBendConstraints(
            definitionHandle,
            dihedralConstraints).IsComplete());
        EXPECT_FLOAT_EQ(dihedralConstraints[0].m_initialAngle, 0.375f);
        EXPECT_FALSE(dihedralConstraints[0].m_calculateInitialAngle);

        AZStd::array<SoftBodyEdgeConstraint, 1> edgeConstraints;
        EXPECT_TRUE(system.GetSoftBodyDefinitionEdgeConstraints(
            definitionHandle,
            edgeConstraints).IsComplete());
        EXPECT_FLOAT_EQ(edgeConstraints[0].m_restLength, 2.0f);

        AZStd::array<SoftBodyInverseBind, 1> inverseBinds;
        EXPECT_TRUE(system.GetSoftBodyDefinitionInverseBinds(
            definitionHandle,
            inverseBinds).IsComplete());
        EXPECT_EQ(inverseBinds[0].m_jointIndex, 7);
        EXPECT_TRUE(inverseBinds[0].m_transform.GetTranslation().IsClose(
            AZ::Vector3(1.0f, 2.0f, 3.0f)));

        AZStd::array<SoftBodyLongRangeConstraint, 1> longRangeConstraints;
        EXPECT_TRUE(system.GetSoftBodyDefinitionLongRangeConstraints(
            definitionHandle,
            longRangeConstraints).IsComplete());
        EXPECT_FLOAT_EQ(longRangeConstraints[0].m_maximumDistance, 4.0f);

        AZStd::array<SoftBodyRodBendTwistConstraint, 1> rodBendTwistConstraints;
        EXPECT_TRUE(system.GetSoftBodyDefinitionRodBendTwistConstraints(
            definitionHandle,
            rodBendTwistConstraints).IsComplete());
        EXPECT_TRUE(rodBendTwistConstraints[0].m_initialRotation.IsClose(initialRodRotation));
        EXPECT_FALSE(rodBendTwistConstraints[0].m_calculateInitialRotation);

        AZStd::array<SoftBodyRodStretchShearConstraint, 2> rodStretchShearConstraints;
        EXPECT_TRUE(system.GetSoftBodyDefinitionRodStretchShearConstraints(
            definitionHandle,
            rodStretchShearConstraints).IsComplete());
        EXPECT_FLOAT_EQ(rodStretchShearConstraints[0].m_restLength, 1.25f);
        EXPECT_FLOAT_EQ(rodStretchShearConstraints[0].m_inverseMass, 0.75f);
        EXPECT_FALSE(rodStretchShearConstraints[0].m_calculateBishopRotation);

        AZStd::array<SoftBodySkinConstraint, 1> skinConstraints;
        EXPECT_TRUE(system.GetSoftBodyDefinitionSkinConstraints(
            definitionHandle,
            skinConstraints).IsComplete());
        EXPECT_EQ(skinConstraints[0].m_vertex, 3);
        EXPECT_FLOAT_EQ(skinConstraints[0].m_weights[0].m_weight, 1.0f);
        EXPECT_FLOAT_EQ(skinConstraints[0].m_maximumDistance, 0.5f);
        EXPECT_FLOAT_EQ(skinConstraints[0].GetWeight(0).m_weight, 1.0f);
        EXPECT_FLOAT_EQ(skinConstraints[0].GetWeight(4).m_weight, 0.0f);
        EXPECT_FALSE(skinConstraints[0].SetWeight(
            4,
            {}));
        EXPECT_TRUE(skinConstraints[0].SetWeight(
            1,
            {
                .m_inverseBindIndex = 0,
                .m_weight = 0.25f,
            }));
        EXPECT_FLOAT_EQ(skinConstraints[0].GetWeight(1).m_weight, 0.25f);

        AZStd::array<SoftBodyVolumeConstraint, 1> volumeConstraints;
        EXPECT_TRUE(system.GetSoftBodyDefinitionVolumeConstraints(
            definitionHandle,
            volumeConstraints).IsComplete());
        EXPECT_FLOAT_EQ(volumeConstraints[0].m_restVolume, 2.5f);
        EXPECT_FALSE(volumeConstraints[0].m_calculateRestVolume);

        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
        state.m_vertexCount = 4;
        EXPECT_FALSE(system.GetSoftBodyDefinitionState(definitionHandle, state));
        EXPECT_EQ(state.m_vertexCount, 0);
        EXPECT_EQ(system.GetSoftBodyDefinitionVertices(definitionHandle, vertices).m_requiredHitCount, 0);

        configuration.m_materials.clear();
        const SoftBodyDefinitionHandle defaultMaterialDefinitionHandle =
            system.CreateSoftBodyDefinition(configuration);
        ASSERT_TRUE(defaultMaterialDefinitionHandle);
        ASSERT_TRUE(system.GetSoftBodyDefinitionState(defaultMaterialDefinitionHandle, state));
        EXPECT_EQ(state.m_materialCount, 1);
        materials[0] = materialHandle;
        EXPECT_TRUE(system.GetSoftBodyDefinitionMaterials(
            defaultMaterialDefinitionHandle,
            materials).IsComplete());
        EXPECT_FALSE(materials[0]);
        EXPECT_TRUE(system.DestroySoftBodyDefinition(defaultMaterialDefinitionHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, FacelessCosseratRodsCreateAndSimulate)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(0.0f, 0.0f, 0.0f), .m_inverseMass = 0.0f},
            {.m_position = AZ::Vector3(0.0f, 0.0f, 1.0f)},
            {.m_position = AZ::Vector3(0.0f, 0.0f, 2.0f)},
            {.m_position = AZ::Vector3(0.0f, 0.0f, 3.0f)},
        };
        definitionConfiguration.m_rodStretchShearConstraints = {
            {.m_firstVertex = 0, .m_secondVertex = 1},
            {.m_firstVertex = 1, .m_secondVertex = 2},
            {.m_firstVertex = 2, .m_secondVertex = 3},
        };
        definitionConfiguration.m_rodBendTwistConstraints = {
            {.m_firstRod = 0, .m_secondRod = 1},
            {.m_firstRod = 1, .m_secondRod = 2},
        };

        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        SoftBodyConfiguration bodyConfiguration;
        bodyConfiguration.m_definitionHandle = definitionHandle;
        const BodyHandle bodyHandle = system.CreateSoftBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        AZStd::array<SoftBodyFace, 1> faces;
        const QueryResult faceResult = system.GetSoftBodyFaces(worldHandle, bodyHandle, faces);
        EXPECT_TRUE(faceResult.IsComplete());
        EXPECT_EQ(faceResult.m_hitCount, 0);
        EXPECT_EQ(faceResult.m_requiredHitCount, 0);

        AZStd::array<SoftBodyRodState, 3> rods;
        const QueryResult rodResult = system.GetSoftBodyRodStates(worldHandle, bodyHandle, rods);
        EXPECT_TRUE(rodResult.IsComplete());
        EXPECT_EQ(rodResult.m_hitCount, 3);

        for (AZ::u32 step = 0; step < 10; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        AZStd::array<SoftBodyVertex, 4> vertices;
        const QueryResult vertexResult = system.GetSoftBodyVertices(worldHandle, bodyHandle, vertices);
        EXPECT_TRUE(vertexResult.IsComplete());
        EXPECT_EQ(vertexResult.m_hitCount, 4);
        for (const SoftBodyVertex& vertex : vertices)
        {
            EXPECT_TRUE(vertex.m_position.IsFinite());
            EXPECT_TRUE(vertex.m_velocity.IsFinite());
        }
        EXPECT_GT(vertices[3].m_velocity.GetLengthSq(), 0.0f);

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
    }

    TEST(SimulationTests, ManualSoftBodiesOnlyAdvanceThroughExplicitUpdates)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(-1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        definitionConfiguration.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        definitionConfiguration.m_createFaceConstraints = true;
        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        SoftBodyConfiguration manualConfiguration;
        manualConfiguration.m_definitionHandle = definitionHandle;
        manualConfiguration.m_transform.m_position.m_z = 5.0;
        manualConfiguration.m_manualUpdate = true;
        const BodyHandle manualBodyHandle = system.CreateSoftBody(worldHandle, manualConfiguration);
        ASSERT_TRUE(manualBodyHandle);
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, manualBodyHandle));
        EXPECT_FALSE(system.ActivateBody(worldHandle, manualBodyHandle));
        EXPECT_FALSE(system.DeactivateBody(worldHandle, manualBodyHandle));
        EXPECT_FALSE(system.SetBodyMoveEventsEnabled(worldHandle, manualBodyHandle, true));

        BodyState initialState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, manualBodyHandle, initialState));
        for (AZ::u32 step = 0; step < 10; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        BodyState worldSteppedState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, manualBodyHandle, worldSteppedState));
        EXPECT_DOUBLE_EQ(worldSteppedState.m_transform.m_position.m_x, initialState.m_transform.m_position.m_x);
        EXPECT_DOUBLE_EQ(worldSteppedState.m_transform.m_position.m_y, initialState.m_transform.m_position.m_y);
        EXPECT_DOUBLE_EQ(worldSteppedState.m_transform.m_position.m_z, initialState.m_transform.m_position.m_z);
        EXPECT_TRUE(worldSteppedState.m_transform.m_rotation.IsClose(initialState.m_transform.m_rotation));
        EXPECT_FALSE(system.UpdateSoftBodyManually(worldHandle, manualBodyHandle, 0.0f));

        for (AZ::u32 step = 0; step < 10; ++step)
        {
            ASSERT_TRUE(system.UpdateSoftBodyManually(worldHandle, manualBodyHandle, 1.0f / 60.0f));
        }

        BodyState manuallySteppedState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, manualBodyHandle, manuallySteppedState));
        EXPECT_LT(manuallySteppedState.m_transform.m_position.m_z, initialState.m_transform.m_position.m_z);

        ASSERT_TRUE(system.AddBodyToSimulation(worldHandle, manualBodyHandle, true));
        EXPECT_TRUE(system.IsBodyInSimulation(worldHandle, manualBodyHandle));
        EXPECT_FALSE(system.UpdateSoftBodyManually(worldHandle, manualBodyHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.RemoveBodyFromSimulation(worldHandle, manualBodyHandle));
        EXPECT_FALSE(system.IsBodyInSimulation(worldHandle, manualBodyHandle));
        EXPECT_TRUE(system.UpdateSoftBodyManually(worldHandle, manualBodyHandle, 1.0f / 60.0f));

        SoftBodyConfiguration automaticConfiguration;
        automaticConfiguration.m_definitionHandle = definitionHandle;
        const BodyHandle automaticBodyHandle = system.CreateSoftBody(worldHandle, automaticConfiguration);
        ASSERT_TRUE(automaticBodyHandle);
        EXPECT_FALSE(system.UpdateSoftBodyManually(worldHandle, automaticBodyHandle, 1.0f / 60.0f));

        EXPECT_TRUE(system.DestroyBody(worldHandle, automaticBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, manualBodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
    }

    TEST(SimulationTests, SoftBodyContactCallbacksExposeSettingsAndManifold)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration floorShapeConfiguration;
        floorShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(20.0f, 20.0f, 1.0f),
        };
        const ShapeHandle floorShapeHandle = system.CreateShape(worldHandle, floorShapeConfiguration);
        ASSERT_TRUE(floorShapeHandle);

        BodyConfiguration floorConfiguration;
        floorConfiguration.m_shapeHandle = floorShapeHandle;
        floorConfiguration.m_transform.m_position.m_z = -0.5;
        floorConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        floorConfiguration.m_motionType = MotionType::Static;
        const BodyHandle floorBodyHandle = system.CreateBody(worldHandle, floorConfiguration);
        ASSERT_TRUE(floorBodyHandle);

        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(-1.0f, -1.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, -1.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        definitionConfiguration.m_faces = {{.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2}};
        definitionConfiguration.m_edgeConstraints = {
            {.m_firstVertex = 0, .m_secondVertex = 1},
            {.m_firstVertex = 1, .m_secondVertex = 2},
            {.m_firstVertex = 2, .m_secondVertex = 0},
        };
        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        SoftBodyConfiguration softBodyConfiguration;
        softBodyConfiguration.m_definitionHandle = definitionHandle;
        softBodyConfiguration.m_transform.m_position.m_z = 2.0;
        softBodyConfiguration.m_vertexRadius = 0.05f;
        const BodyHandle softBodyHandle = system.CreateSoftBody(worldHandle, softBodyConfiguration);
        ASSERT_TRUE(softBodyHandle);

        RecordingSoftBodyContactCallbacks callbacks;
        ScopedExtensionRegistration registration(system, &callbacks);
        ASSERT_TRUE(system.SetSoftBodyContactCallbacks(worldHandle, registration.GetHandle()));
        for (AZ::u32 step = 0; step < 120 && callbacks.m_addedCount == 0; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        EXPECT_GT(callbacks.m_validationCount, 0);
        EXPECT_GT(callbacks.m_addedCount, 0);
        EXPECT_GT(callbacks.m_contactCount, 0);
        EXPECT_EQ(callbacks.m_softBodyHandle, softBodyHandle);
        EXPECT_EQ(callbacks.m_otherBodyHandle, floorBodyHandle);
        EXPECT_EQ(callbacks.m_lastContact.m_otherBodyHandle, floorBodyHandle);
        EXPECT_EQ(callbacks.m_vertexCount, 3);
        EXPECT_TRUE(callbacks.m_lastContact.m_normal.IsFinite());
        EXPECT_TRUE(std::isfinite(callbacks.m_lastContact.m_position.m_x));
        EXPECT_TRUE(std::isfinite(callbacks.m_lastContact.m_position.m_y));
        EXPECT_TRUE(std::isfinite(callbacks.m_lastContact.m_position.m_z));
        EXPECT_FLOAT_EQ(callbacks.m_lastSettings.m_softBodyInverseMassScale, 1.0f);
        EXPECT_FLOAT_EQ(callbacks.m_lastSettings.m_otherBodyInverseMassScale, 1.0f);
        EXPECT_FLOAT_EQ(callbacks.m_lastSettings.m_otherBodyInverseInertiaScale, 1.0f);

        EXPECT_TRUE(system.SetSoftBodyContactCallbacks(worldHandle, ExtensionHandle::Invalid));
        EXPECT_TRUE(system.DestroyBody(worldHandle, softBodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, floorBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, floorShapeHandle));
    }

    TEST(SimulationTests, SoftBodyContactValidationAndMassScalingControlResponse)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration floorShapeConfiguration;
        floorShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(20.0f, 20.0f, 1.0f),
        };
        const ShapeHandle floorShapeHandle = system.CreateShape(worldHandle, floorShapeConfiguration);
        ASSERT_TRUE(floorShapeHandle);

        BodyConfiguration floorConfiguration;
        floorConfiguration.m_shapeHandle = floorShapeHandle;
        floorConfiguration.m_transform.m_position.m_z = -0.5;
        floorConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        floorConfiguration.m_motionType = MotionType::Static;
        const BodyHandle floorBodyHandle = system.CreateBody(worldHandle, floorConfiguration);
        ASSERT_TRUE(floorBodyHandle);

        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(-1.0f, -1.0f, 0.0f)},
            {.m_position = AZ::Vector3(1.0f, -1.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        definitionConfiguration.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        definitionConfiguration.m_edgeConstraints = {
            {.m_firstVertex = 0, .m_secondVertex = 1},
            {.m_firstVertex = 1, .m_secondVertex = 2},
            {.m_firstVertex = 2, .m_secondVertex = 0},
        };
        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        const auto createFallingSoftBody = [&system, worldHandle, definitionHandle]
        {
            SoftBodyConfiguration configuration;
            configuration.m_definitionHandle = definitionHandle;
            configuration.m_transform.m_position.m_z = 2.0;
            configuration.m_vertexRadius = 0.05f;
            return system.CreateSoftBody(worldHandle, configuration);
        };

        RecordingSoftBodyContactCallbacks rejectedCallbacks;
        rejectedCallbacks.m_decision = SoftBodyContactDecision::Reject;
        ScopedExtensionRegistration rejectedRegistration(system, &rejectedCallbacks);
        ASSERT_TRUE(system.SetSoftBodyContactCallbacks(worldHandle, rejectedRegistration.GetHandle()));
        const BodyHandle rejectedBodyHandle = createFallingSoftBody();
        ASSERT_TRUE(rejectedBodyHandle);
        for (AZ::u32 step = 0; step < 120; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        BodyState rejectedBodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, rejectedBodyHandle, rejectedBodyState));
        EXPECT_GT(rejectedCallbacks.m_validationCount, 0);
        EXPECT_EQ(rejectedCallbacks.m_addedCount, 0);
        EXPECT_LT(rejectedBodyState.m_transform.m_position.m_z, -1.0);
        ASSERT_TRUE(system.DestroyBody(worldHandle, rejectedBodyHandle));

        RecordingSoftBodyContactCallbacks invalidSettingsCallbacks;
        invalidSettingsCallbacks.m_responseSettings.m_softBodyInverseMassScale =
            AZStd::numeric_limits<float>::quiet_NaN();
        ScopedExtensionRegistration invalidSettingsRegistration(system, &invalidSettingsCallbacks);
        ASSERT_TRUE(system.SetSoftBodyContactCallbacks(worldHandle, invalidSettingsRegistration.GetHandle()));
        const BodyHandle invalidSettingsBodyHandle = createFallingSoftBody();
        ASSERT_TRUE(invalidSettingsBodyHandle);
        for (AZ::u32 step = 0; step < 120; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        BodyState invalidSettingsBodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, invalidSettingsBodyHandle, invalidSettingsBodyState));
        EXPECT_GT(invalidSettingsCallbacks.m_validationCount, 0);
        EXPECT_EQ(invalidSettingsCallbacks.m_addedCount, 0);
        EXPECT_LT(invalidSettingsBodyState.m_transform.m_position.m_z, -1.0);
        ASSERT_TRUE(system.DestroyBody(worldHandle, invalidSettingsBodyHandle));

        RecordingSoftBodyContactCallbacks responsiveCallbacks;
        responsiveCallbacks.m_responseSettings = {};
        ScopedExtensionRegistration responsiveRegistration(system, &responsiveCallbacks);
        ASSERT_TRUE(system.SetSoftBodyContactCallbacks(worldHandle, responsiveRegistration.GetHandle()));
        const BodyHandle responsiveBodyHandle = createFallingSoftBody();
        ASSERT_TRUE(responsiveBodyHandle);
        for (AZ::u32 step = 0; step < 120; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        BodyState responsiveBodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, responsiveBodyHandle, responsiveBodyState));
        EXPECT_GT(responsiveCallbacks.m_addedCount, 0);
        EXPECT_GT(responsiveBodyState.m_transform.m_position.m_z, -0.5);
        ASSERT_TRUE(system.DestroyBody(worldHandle, responsiveBodyHandle));

        RecordingSoftBodyContactCallbacks zeroMassScaleCallbacks;
        zeroMassScaleCallbacks.m_responseSettings = {};
        zeroMassScaleCallbacks.m_responseSettings.m_softBodyInverseMassScale = 0.0f;
        ScopedExtensionRegistration zeroMassScaleRegistration(system, &zeroMassScaleCallbacks);
        ASSERT_TRUE(system.SetSoftBodyContactCallbacks(worldHandle, zeroMassScaleRegistration.GetHandle()));
        const BodyHandle zeroMassScaleBodyHandle = createFallingSoftBody();
        ASSERT_TRUE(zeroMassScaleBodyHandle);
        for (AZ::u32 step = 0; step < 120; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        BodyState zeroMassScaleBodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, zeroMassScaleBodyHandle, zeroMassScaleBodyState));
        EXPECT_GT(zeroMassScaleCallbacks.m_validationCount, 0);
        EXPECT_EQ(zeroMassScaleCallbacks.m_addedCount, 0);
        EXPECT_LT(zeroMassScaleBodyState.m_transform.m_position.m_z, -1.0);

        EXPECT_TRUE(system.SetSoftBodyContactCallbacks(worldHandle, ExtensionHandle::Invalid));
        EXPECT_TRUE(system.DestroyBody(worldHandle, zeroMassScaleBodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, floorBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, floorShapeHandle));
    }

    TEST(SimulationTests, LinearCastRigidBodiesUseSoftBodyContactCallbacks)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(0.0f, -2.0f, -2.0f)},
            {.m_position = AZ::Vector3(0.0f, 2.0f, -2.0f)},
            {.m_position = AZ::Vector3(0.0f, 0.0f, 2.0f)},
        };
        definitionConfiguration.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        definitionConfiguration.m_edgeConstraints = {
            {.m_firstVertex = 0, .m_secondVertex = 1},
            {.m_firstVertex = 1, .m_secondVertex = 2},
            {.m_firstVertex = 2, .m_secondVertex = 0},
        };
        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        SoftBodyConfiguration softBodyConfiguration;
        softBodyConfiguration.m_definitionHandle = definitionHandle;
        softBodyConfiguration.m_facesDoubleSided = true;
        softBodyConfiguration.m_gravityFactor = 0.0f;
        softBodyConfiguration.m_linearDamping = 0.0f;
        softBodyConfiguration.m_updatePosition = false;
        const BodyHandle softBodyHandle = system.CreateSoftBody(worldHandle, softBodyConfiguration);
        ASSERT_TRUE(softBodyHandle);

        ShapeConfiguration sphereShapeConfiguration;
        sphereShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.25f};
        const ShapeHandle sphereShapeHandle = system.CreateShape(worldHandle, sphereShapeConfiguration);
        ASSERT_TRUE(sphereShapeHandle);

        BodyConfiguration sphereConfiguration;
        sphereConfiguration.m_shapeHandle = sphereShapeHandle;
        sphereConfiguration.m_transform.m_position.m_x = 2.0;
        sphereConfiguration.m_linearVelocity = AZ::Vector3::CreateAxisX(-300.0f);
        sphereConfiguration.m_angularDamping = 0.0f;
        sphereConfiguration.m_gravityFactor = 0.0f;
        sphereConfiguration.m_linearDamping = 0.0f;
        sphereConfiguration.m_massProperties = {
            .m_mass = 200.0f,
            .m_mode = MassPropertiesMode::CalculateInertia,
        };
        sphereConfiguration.m_motionQuality = MotionQuality::Continuous;
        const BodyHandle sphereBodyHandle = system.CreateBody(worldHandle, sphereConfiguration);
        ASSERT_TRUE(sphereBodyHandle);

        RecordingSoftBodyContactCallbacks callbacks;
        callbacks.m_responseSettings = {};
        ScopedExtensionRegistration registration(system, &callbacks);
        ASSERT_TRUE(system.SetSoftBodyContactCallbacks(worldHandle, registration.GetHandle()));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        EXPECT_GT(callbacks.m_validationCount, 0);
        EXPECT_GT(callbacks.m_addedCount, 0);
        EXPECT_EQ(callbacks.m_softBodyHandle, softBodyHandle);
        EXPECT_EQ(callbacks.m_otherBodyHandle, sphereBodyHandle);

        BodyState sphereState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, sphereBodyHandle, sphereState));
        EXPECT_GT(sphereState.m_transform.m_position.m_x, -1.0);
        AZ::Vector3 softBodyVelocity;
        ASSERT_TRUE(system.GetBodyLinearVelocity(worldHandle, softBodyHandle, softBodyVelocity));
        EXPECT_LT(softBodyVelocity.GetX(), 0.0f);

        EXPECT_TRUE(system.SetSoftBodyContactCallbacks(worldHandle, ExtensionHandle::Invalid));
        EXPECT_TRUE(system.DestroyBody(worldHandle, sphereBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, sphereShapeHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, softBodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
    }

    TEST(SimulationTests, SoftBodyContactSettingsExposeMultipleSensorsWithoutCollisionResponse)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration sensorShapeConfiguration;
        sensorShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(4.0f, 4.0f, 1.0f),
        };
        const ShapeHandle sensorShapeHandle = system.CreateShape(worldHandle, sensorShapeConfiguration);
        ASSERT_TRUE(sensorShapeHandle);

        BodyConfiguration sensorConfiguration;
        sensorConfiguration.m_shapeHandle = sensorShapeHandle;
        sensorConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        sensorConfiguration.m_motionType = MotionType::Static;
        const BodyHandle firstSensorBodyHandle = system.CreateBody(worldHandle, sensorConfiguration);
        const BodyHandle secondSensorBodyHandle = system.CreateBody(worldHandle, sensorConfiguration);
        ASSERT_TRUE(firstSensorBodyHandle);
        ASSERT_TRUE(secondSensorBodyHandle);

        SoftBodyDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(-0.5f, -0.5f, 0.0f)},
            {.m_position = AZ::Vector3(0.5f, -0.5f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 0.5f, 0.0f)},
        };
        definitionConfiguration.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        const SoftBodyDefinitionHandle definitionHandle =
            system.CreateSoftBodyDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);

        SoftBodyConfiguration softBodyConfiguration;
        softBodyConfiguration.m_definitionHandle = definitionHandle;
        softBodyConfiguration.m_transform.m_position.m_z = 0.25;
        const BodyHandle softBodyHandle = system.CreateSoftBody(worldHandle, softBodyConfiguration);
        ASSERT_TRUE(softBodyHandle);

        RecordingSoftBodyContactCallbacks callbacks;
        callbacks.m_responseSettings.m_isSensor = true;
        ScopedExtensionRegistration registration(system, &callbacks);
        ASSERT_TRUE(system.SetSoftBodyContactCallbacks(worldHandle, registration.GetHandle()));
        for (AZ::u32 step = 0; step < 10 && callbacks.m_maximumSensorCount < 2; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        EXPECT_GT(callbacks.m_validationCount, 0);
        EXPECT_GT(callbacks.m_addedCount, 0);
        EXPECT_EQ(callbacks.m_contactCount, 0);
        EXPECT_EQ(callbacks.m_maximumSensorCount, 2);
        EXPECT_NE(
            AZStd::find(
                callbacks.m_sensorBodyHandles.begin(),
                callbacks.m_sensorBodyHandles.end(),
                firstSensorBodyHandle),
            callbacks.m_sensorBodyHandles.end());
        EXPECT_NE(
            AZStd::find(
                callbacks.m_sensorBodyHandles.begin(),
                callbacks.m_sensorBodyHandles.end(),
                secondSensorBodyHandle),
            callbacks.m_sensorBodyHandles.end());

        EXPECT_TRUE(system.SetSoftBodyContactCallbacks(worldHandle, ExtensionHandle::Invalid));
        EXPECT_TRUE(system.DestroyBody(worldHandle, softBodyHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(definitionHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondSensorBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstSensorBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, sensorShapeHandle));
    }

    TEST(SimulationTests, HairUsesSharedDefinitionsAndCallerOwnedStateBuffers)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        HairDefinitionConfiguration definitionConfiguration;
        definitionConfiguration.m_vertices = {
            {.m_position = AZ::Vector3(0.0f, 0.0f, 1.0f), .m_inverseMass = 0.0f},
            {.m_position = AZ::Vector3(0.0f, 0.0f, 0.5f)},
            {.m_position = AZ::Vector3::CreateZero()},
        };
        definitionConfiguration.m_strands = {
            {.m_beginVertex = 0, .m_endVertex = 3, .m_materialIndex = 0},
        };
        definitionConfiguration.m_materials.resize(1);
        definitionConfiguration.m_materials[0].m_enableCollision = true;
        definitionConfiguration.m_materials[0].m_globalPose = {
            .m_minimum = 0.0f,
            .m_maximum = 0.0f,
        };
        definitionConfiguration.m_scalpVertices = {
            AZ::Vector3(-1.0f, -1.0f, 1.0f),
            AZ::Vector3(1.0f, -1.0f, 1.0f),
            AZ::Vector3(0.0f, 1.0f, 1.0f),
        };
        definitionConfiguration.m_scalpTriangles = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        definitionConfiguration.m_scalpInverseBindPoses = {
            AZ::Transform::CreateIdentity(),
        };
        definitionConfiguration.m_scalpSkinWeights = {
            {.m_jointIndex = 0, .m_weight = 1.0f},
            {.m_jointIndex = 0, .m_weight = 1.0f},
            {.m_jointIndex = 0, .m_weight = 1.0f},
        };
        definitionConfiguration.m_scalpSkinWeightsPerVertex = 1;
        definitionConfiguration.m_gridSizeX = 2;
        definitionConfiguration.m_gridSizeY = 2;
        definitionConfiguration.m_gridSizeZ = 2;

        HairDefinitionConfiguration overflowingGridConfiguration = definitionConfiguration;
        overflowingGridConfiguration.m_gridSizeX = AZStd::numeric_limits<AZ::u32>::max();
        overflowingGridConfiguration.m_gridSizeY = AZStd::numeric_limits<AZ::u32>::max();
        overflowingGridConfiguration.m_gridSizeZ = AZStd::numeric_limits<AZ::u32>::max();
        EXPECT_FALSE(system.CreateHairDefinition(overflowingGridConfiguration));

        const HairDefinitionHandle definitionHandle =
            system.CreateHairDefinition(definitionConfiguration);
        ASSERT_TRUE(definitionHandle);
        EXPECT_TRUE(system.IsValid(definitionHandle));

        HairDefinitionState definitionState;
        ASSERT_TRUE(system.GetHairDefinitionState(definitionHandle, definitionState));
        EXPECT_TRUE(definitionState.m_simulationBounds.IsValid());
        EXPECT_EQ(definitionState.m_gridCellCount, 8);
        EXPECT_EQ(definitionState.m_jointCount, 1);
        EXPECT_EQ(definitionState.m_maximumVerticesPerStrand, 3);
        EXPECT_EQ(definitionState.m_paddedSimulationVertexCount, 3);
        EXPECT_EQ(definitionState.m_renderVertexCount, 3);
        EXPECT_EQ(definitionState.m_scalpVertexCount, 3);
        EXPECT_EQ(definitionState.m_simulationVertexCount, 3);
        EXPECT_GT(definitionState.m_densityScale, 0.0f);
        EXPECT_FLOAT_EQ(definitionState.m_maximumHairToScalpDistanceSquared, 0.0f);

        const QueryResult neutralDensitySize = system.GetHairNeutralDensity(definitionHandle, {});
        EXPECT_EQ(neutralDensitySize.m_hitCount, 0);
        EXPECT_EQ(neutralDensitySize.m_requiredHitCount, 8);
        AZStd::array<float, 8> neutralDensity;
        EXPECT_TRUE(system.GetHairNeutralDensity(definitionHandle, neutralDensity).IsComplete());
        EXPECT_TRUE(AZStd::any_of(
            neutralDensity.begin(),
            neutralDensity.end(),
            [](const float density)
            {
                return density > 0.0f;
            }));

        AZ::Transform jointModelTransform = AZ::Transform::CreateTranslation(AZ::Vector3::CreateAxisX(2.0f));
        const AZStd::array scalpJointModelTransforms = {jointModelTransform};
        AZStd::array<AZ::Transform, 1> preparedJointTransforms;
        AZStd::array<AZ::Vector3, 3> skinnedScalpVertices;
        EXPECT_FALSE(system.SkinHairScalpVertices(
            definitionHandle,
            AZ::Transform::CreateIdentity(),
            scalpJointModelTransforms,
            {},
            skinnedScalpVertices));
        ASSERT_TRUE(system.SkinHairScalpVertices(
            definitionHandle,
            AZ::Transform::CreateIdentity(),
            scalpJointModelTransforms,
            preparedJointTransforms,
            skinnedScalpVertices));
        EXPECT_TRUE(preparedJointTransforms[0].IsClose(jointModelTransform));
        EXPECT_TRUE(skinnedScalpVertices[0].IsClose(AZ::Vector3(1.0f, -1.0f, 1.0f)));
        EXPECT_TRUE(skinnedScalpVertices[1].IsClose(AZ::Vector3(3.0f, -1.0f, 1.0f)));
        EXPECT_TRUE(skinnedScalpVertices[2].IsClose(AZ::Vector3(2.0f, 1.0f, 1.0f)));

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();

        ShapeConfiguration primitiveShapeConfiguration;
        primitiveShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle primitiveShapeHandle = system.CreateShape(
            worldHandle,
            primitiveShapeConfiguration);
        ASSERT_TRUE(primitiveShapeHandle);

        BodyConfiguration primitiveBodyConfiguration;
        primitiveBodyConfiguration.m_shapeHandle = primitiveShapeHandle;
        primitiveBodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle primitiveBodyHandle = system.CreateBody(
            worldHandle,
            primitiveBodyConfiguration);
        ASSERT_TRUE(primitiveBodyHandle);

        HairConfiguration hairConfiguration;
        hairConfiguration.m_definitionHandle = definitionHandle;
        hairConfiguration.m_objectLayer = DefaultLayers::Moving;
        hairConfiguration.m_scalpToHeadTransform.SetTranslation(AZ::Vector3::CreateAxisZ(0.1f));
        hairConfiguration.m_scalpToHeadTransform.SetUniformScale(0.75f);
        const HairHandle hairHandle = system.CreateHair(worldHandle, hairConfiguration);
        ASSERT_TRUE(hairHandle);
        EXPECT_FALSE(system.DestroyHairDefinition(definitionHandle));

        HairState state;
        ASSERT_TRUE(system.GetHairState(worldHandle, hairHandle, state));
        EXPECT_EQ(state.m_simulationVertexCount, 3);
        EXPECT_EQ(state.m_renderVertexCount, 3);
        EXPECT_EQ(state.m_scalpVertexCount, 3);
        EXPECT_EQ(state.m_gridCellCount, 8);
        EXPECT_EQ(state.m_gridSizeX, 2);
        EXPECT_EQ(state.m_gridSizeY, 2);
        EXPECT_EQ(state.m_gridSizeZ, 2);
        EXPECT_TRUE(state.m_scalpToHeadTransform.IsClose(hairConfiguration.m_scalpToHeadTransform));
        EXPECT_TRUE(state.m_teleported);

        AZ::Transform invalidScalpToHeadTransform = AZ::Transform::CreateIdentity();
        invalidScalpToHeadTransform.SetUniformScale(0.0f);
        EXPECT_FALSE(system.SetHairScalpToHeadTransform(
            worldHandle,
            hairHandle,
            invalidScalpToHeadTransform));

        const StateSnapshotHandle configurationSnapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(configurationSnapshotHandle);
        AZ::Transform scalpToHeadTransform = AZ::Transform::CreateIdentity();
        scalpToHeadTransform.SetTranslation(AZ::Vector3::CreateAxisX(0.25f));
        scalpToHeadTransform.SetUniformScale(0.5f);
        ASSERT_TRUE(system.SetHairScalpToHeadTransform(
            worldHandle,
            hairHandle,
            scalpToHeadTransform));
        ASSERT_TRUE(system.GetHairState(worldHandle, hairHandle, state));
        EXPECT_TRUE(state.m_scalpToHeadTransform.IsClose(scalpToHeadTransform));
        EXPECT_TRUE(state.m_teleported);
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, configurationSnapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, configurationSnapshotHandle));

        const AZStd::array<AZ::Transform, 1> jointModelTransforms{
            AZ::Transform::CreateIdentity(),
        };
        WorldTransform movedTransform;
        movedTransform.m_position.m_x = 0.25;
        ASSERT_TRUE(system.SetHairTransform(
            worldHandle,
            hairHandle,
            movedTransform,
            false));
        EXPECT_TRUE(system.UpdateHair(
            worldHandle,
            hairHandle,
            1.0f / 60.0f,
            AZ::Transform::CreateIdentity(),
            jointModelTransforms));
        EXPECT_FALSE(system.EnableHairAutoUpdate(
            worldHandle,
            hairHandle,
            AZ::Transform::CreateIdentity(),
            {}));
        ASSERT_TRUE(system.EnableHairAutoUpdate(
            worldHandle,
            hairHandle,
            AZ::Transform::CreateIdentity(),
            jointModelTransforms));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        const StateSnapshotHandle autoUpdateSnapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(autoUpdateSnapshotHandle);
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        AZStd::array<HairVertexState, 3> firstReplayStates;
        ASSERT_TRUE(system.GetHairVertexStates(
            worldHandle,
            hairHandle,
            firstReplayStates).IsComplete());
        ASSERT_TRUE(system.RestoreWorldState(worldHandle, autoUpdateSnapshotHandle));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        AZStd::array<HairVertexState, 3> secondReplayStates;
        ASSERT_TRUE(system.GetHairVertexStates(
            worldHandle,
            hairHandle,
            secondReplayStates).IsComplete());
        for (size_t vertexIndex = 0; vertexIndex < firstReplayStates.size(); ++vertexIndex)
        {
            EXPECT_EQ(
                secondReplayStates[vertexIndex].m_localPosition,
                firstReplayStates[vertexIndex].m_localPosition);
            EXPECT_EQ(
                secondReplayStates[vertexIndex].m_localRotation,
                firstReplayStates[vertexIndex].m_localRotation);
            EXPECT_EQ(
                secondReplayStates[vertexIndex].m_localLinearVelocity,
                firstReplayStates[vertexIndex].m_localLinearVelocity);
            EXPECT_EQ(
                secondReplayStates[vertexIndex].m_localAngularVelocity,
                firstReplayStates[vertexIndex].m_localAngularVelocity);
        }

        ASSERT_TRUE(system.EnableHairAutoUpdate(
            worldHandle,
            hairHandle,
            AZ::Transform::CreateIdentity(),
            jointModelTransforms));
        EXPECT_TRUE(system.RestoreWorldState(worldHandle, autoUpdateSnapshotHandle));

        AZStd::array<AZ::Transform, 1> movedJointModelTransforms = jointModelTransforms;
        movedJointModelTransforms[0].SetTranslation(AZ::Vector3::CreateAxisY());
        ASSERT_TRUE(system.EnableHairAutoUpdate(
            worldHandle,
            hairHandle,
            AZ::Transform::CreateIdentity(),
            movedJointModelTransforms));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, autoUpdateSnapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, autoUpdateSnapshotHandle));
        EXPECT_TRUE(system.DisableHairAutoUpdate(worldHandle, hairHandle));

        AZStd::array<HairVertexState, 3> vertexStates;
        AZStd::array<AZ::Vector3, 3> renderPositions;
        AZStd::array<AZ::Vector3, 3> scalpPositions;
        AZStd::array<HairGridCellState, 8> gridCells;
        const HairReadbackBuffers readbackBuffers{
            .m_vertexStates = vertexStates,
            .m_renderPositions = renderPositions,
            .m_scalpPositions = scalpPositions,
            .m_gridCells = gridCells,
        };
        HairReadbackResult readbackResult;
        ASSERT_TRUE(system.GetHairReadback(
            worldHandle,
            hairHandle,
            readbackBuffers,
            readbackResult));
        EXPECT_TRUE(readbackResult.m_vertexStates.IsComplete());
        EXPECT_TRUE(readbackResult.m_renderPositions.IsComplete());
        EXPECT_TRUE(readbackResult.m_scalpPositions.IsComplete());
        EXPECT_TRUE(readbackResult.m_gridCells.IsComplete());

        const QueryResult vertexResult = system.GetHairVertexStates(
            worldHandle,
            hairHandle,
            vertexStates);
        EXPECT_TRUE(vertexResult.IsComplete());
        EXPECT_EQ(vertexResult.m_hitCount, 3);
        for (const HairVertexState& vertexState : vertexStates)
        {
            EXPECT_TRUE(vertexState.m_localPosition.IsFinite());
            EXPECT_TRUE(vertexState.m_localRotation.IsFinite());
            EXPECT_TRUE(vertexState.m_localLinearVelocity.IsFinite());
            EXPECT_TRUE(vertexState.m_localAngularVelocity.IsFinite());
        }

        const QueryResult renderResult = system.GetHairRenderPositions(
            worldHandle,
            hairHandle,
            renderPositions);
        EXPECT_TRUE(renderResult.IsComplete());
        EXPECT_EQ(renderResult.m_hitCount, 3);
        for (const AZ::Vector3& renderPosition : renderPositions)
        {
            EXPECT_TRUE(renderPosition.IsFinite());
        }

        const QueryResult scalpResult = system.GetHairScalpPositions(
            worldHandle,
            hairHandle,
            scalpPositions);
        EXPECT_TRUE(scalpResult.IsComplete());
        EXPECT_EQ(scalpResult.m_hitCount, 3);
        for (const AZ::Vector3& scalpPosition : scalpPositions)
        {
            EXPECT_TRUE(scalpPosition.IsFinite());
        }

        const QueryResult gridResult = system.GetHairGridCellStates(
            worldHandle,
            hairHandle,
            gridCells);
        EXPECT_TRUE(gridResult.IsComplete());
        EXPECT_EQ(gridResult.m_hitCount, 8);
        for (const HairGridCellState& gridCell : gridCells)
        {
            EXPECT_TRUE(gridCell.GetVelocity().IsFinite());
            EXPECT_TRUE(AZ::IsFiniteFloat(gridCell.GetDensity()));
        }

        DebugDrawSettings debugSettings;
        debugSettings.m_flags = DebugDrawFlags::None;
        debugSettings.m_hair.m_flags = DebugHairDrawFlags::All;
        RecordingDebugRenderer hairRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            debugSettings,
            hairRenderer));
        EXPECT_GT(hairRenderer.m_lineCount, 0);

        SelectedDebugFilter debugFilter;
        debugFilter.m_selectedHairHandle = hairHandle;
        RecordingDebugRenderer selectedHairRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            debugSettings,
            selectedHairRenderer,
            &debugFilter));
        EXPECT_EQ(debugFilter.m_hairCallCount, 1);
        EXPECT_EQ(selectedHairRenderer.m_lineCount, hairRenderer.m_lineCount);

        debugFilter.m_selectedHairHandle = HairHandle::Invalid;
        RecordingDebugRenderer rejectedHairRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            debugSettings,
            rejectedHairRenderer,
            &debugFilter));
        EXPECT_EQ(debugFilter.m_hairCallCount, 2);
        EXPECT_EQ(rejectedHairRenderer.m_lineCount, 0);

        debugSettings.m_hair.m_strandBegin = 1;
        debugSettings.m_hair.m_strandEnd = 0;
        RecordingDebugRenderer invalidHairRenderer;
        EXPECT_FALSE(system.DrawDebug(
            worldHandle,
            debugSettings,
            invalidHairRenderer));

        HairReadbackResult countOnlyResult;
        ASSERT_TRUE(system.GetHairReadback(
            worldHandle,
            hairHandle,
            {},
            countOnlyResult));
        EXPECT_EQ(countOnlyResult.m_vertexStates.m_requiredHitCount, 3);
        EXPECT_EQ(countOnlyResult.m_renderPositions.m_requiredHitCount, 3);
        EXPECT_EQ(countOnlyResult.m_scalpPositions.m_requiredHitCount, 3);
        EXPECT_EQ(countOnlyResult.m_gridCells.m_requiredHitCount, 8);

        WorldStateDigest capturedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, capturedDigest));
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);
        StateValidationResult validationResult;
        ASSERT_TRUE(system.ValidateWorldState(
            worldHandle,
            snapshotHandle,
            validationResult));
        EXPECT_TRUE(validationResult.m_matches);

        ASSERT_TRUE(system.CaptureWorldState(worldHandle, snapshotHandle));
        ASSERT_TRUE(system.ValidateWorldState(
            worldHandle,
            snapshotHandle,
            validationResult));
        EXPECT_TRUE(validationResult.m_matches);

        movedTransform.m_position.m_x = 0.5;
        ASSERT_TRUE(system.SetHairTransform(
            worldHandle,
            hairHandle,
            movedTransform,
            false));
        WorldStateDigest transformedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, transformedDigest));
        EXPECT_NE(transformedDigest, capturedDigest);
        EXPECT_TRUE(system.UpdateHair(
            worldHandle,
            hairHandle,
            1.0f / 60.0f,
            AZ::Transform::CreateIdentity(),
            jointModelTransforms));
        WorldStateDigest advancedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, advancedDigest));
        EXPECT_NE(advancedDigest, capturedDigest);
        ASSERT_TRUE(system.ValidateWorldState(
            worldHandle,
            snapshotHandle,
            validationResult));
        EXPECT_FALSE(validationResult.m_matches);
        WorldStateDigest digestAfterValidation;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, digestAfterValidation));
        EXPECT_EQ(digestAfterValidation, advancedDigest);

        ASSERT_TRUE(system.RestoreWorldState(worldHandle, snapshotHandle));
        WorldStateDigest restoredDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, restoredDigest));
        EXPECT_EQ(restoredDigest, capturedDigest);
        ASSERT_TRUE(system.ValidateWorldState(
            worldHandle,
            snapshotHandle,
            validationResult));
        EXPECT_TRUE(validationResult.m_matches);
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));

        EXPECT_TRUE(system.DestroyHair(worldHandle, hairHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, hairHandle));
        EXPECT_FALSE(system.GetHairReadback(
            worldHandle,
            hairHandle,
            {},
            countOnlyResult));
        EXPECT_TRUE(system.DestroyHairDefinition(definitionHandle));
        EXPECT_FALSE(system.IsValid(definitionHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, primitiveBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, primitiveShapeHandle));
    }

    TEST(SimulationTests, VirtualCharactersUseProviderHandlesAndGroundState)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration floorShapeConfiguration;
        floorShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(20.0f, 20.0f, 1.0f),
        };
        const ShapeHandle floorShapeHandle = system.CreateShape(worldHandle, floorShapeConfiguration);
        ASSERT_TRUE(floorShapeHandle);

        BodyConfiguration floorConfiguration;
        floorConfiguration.m_shapeHandle = floorShapeHandle;
        floorConfiguration.m_transform.m_position.m_z = -0.5;
        floorConfiguration.m_motionType = MotionType::Static;
        const BodyHandle floorBodyHandle = system.CreateBody(worldHandle, floorConfiguration);
        ASSERT_TRUE(floorBodyHandle);

        ShapeConfiguration characterShapeConfiguration;
        characterShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle characterShapeHandle = system.CreateShape(worldHandle, characterShapeConfiguration);
        ASSERT_TRUE(characterShapeHandle);

        VirtualCharacterConfiguration characterConfiguration;
        characterConfiguration.m_shapeHandle = characterShapeHandle;
        characterConfiguration.m_transform.m_position.m_z = 2.0;
        characterConfiguration.m_entityId = AZ::EntityId(42);
        characterConfiguration.m_userData = 0x9876'5432'10fe'dcba;
        const VirtualCharacterHandle characterHandle =
            system.CreateVirtualCharacter(worldHandle, characterConfiguration);
        ASSERT_TRUE(characterHandle);
        EXPECT_TRUE(system.IsValid(worldHandle, characterHandle));
        EXPECT_FALSE(system.DestroyShape(worldHandle, characterShapeHandle));
        EXPECT_TRUE(system.SetVirtualCharacterVelocity(
            worldHandle,
            characterHandle,
            AZ::Vector3(0.0f, 0.0f, -3.0f)));

        VirtualCharacterUpdateConfiguration updateConfiguration;
        updateConfiguration.m_extended = false;
        ASSERT_TRUE(system.UpdateVirtualCharacter(
            worldHandle,
            characterHandle,
            1.0f / 60.0f,
            updateConfiguration));
        ASSERT_TRUE(system.SetVirtualCharacterTransform(
            worldHandle,
            characterHandle,
            characterConfiguration.m_transform));
        ASSERT_TRUE(system.EnableVirtualCharacterAutoUpdate(
            worldHandle,
            characterHandle,
            updateConfiguration));
        for (AZ::u32 step = 0; step < 90; ++step)
        {
            ASSERT_TRUE(system.StepAutoSimulatedWorlds(1.0f / 60.0f));
        }

        VirtualCharacterState state;
        ASSERT_TRUE(system.GetVirtualCharacterState(worldHandle, characterHandle, state));
        EXPECT_EQ(state.m_shapeHandle, characterShapeHandle);
        EXPECT_EQ(state.m_userData, characterConfiguration.m_userData);
        AZ::u64 userData = 0;
        ASSERT_TRUE(system.GetVirtualCharacterUserData(worldHandle, characterHandle, userData));
        EXPECT_EQ(userData, characterConfiguration.m_userData);
        constexpr AZ::u64 updatedUserData = 0x1111'2222'3333'4444;
        ASSERT_TRUE(system.SetVirtualCharacterUserData(worldHandle, characterHandle, updatedUserData));
        ASSERT_TRUE(system.GetVirtualCharacterUserData(worldHandle, characterHandle, userData));
        EXPECT_EQ(userData, updatedUserData);
        ASSERT_TRUE(system.GetVirtualCharacterState(worldHandle, characterHandle, state));
        EXPECT_EQ(state.m_userData, updatedUserData);
        EXPECT_EQ(state.m_groundBodyHandle, floorBodyHandle);
        EXPECT_EQ(state.m_groundState, GroundState::OnGround);
        EXPECT_TRUE(state.m_isSupported);
        EXPECT_NEAR(state.m_transform.m_position.m_z, 0.5, 0.05);

        const AZStd::span<const VirtualCharacterMoveEvent> moves =
            system.GetEvents(worldHandle).GetVirtualCharacterMoves();
        ASSERT_EQ(moves.size(), 1);
        EXPECT_EQ(moves.front().m_characterHandle, characterHandle);
        EXPECT_EQ(moves.front().m_entityId, characterConfiguration.m_entityId);
        EXPECT_NEAR(moves.front().m_transform.m_position.m_z, state.m_transform.m_position.m_z, 1.0e-6);
        EXPECT_TRUE(moves.front().m_transform.m_rotation.IsClose(state.m_transform.m_rotation));

        ASSERT_TRUE(system.DisableVirtualCharacterAutoUpdate(worldHandle, characterHandle));
        WorldTransform disabledTransform = characterConfiguration.m_transform;
        disabledTransform.m_position.m_z = 3.0;
        ASSERT_TRUE(system.SetVirtualCharacterTransform(worldHandle, characterHandle, disabledTransform));
        ASSERT_TRUE(system.StepAutoSimulatedWorlds(1.0f / 60.0f));
        ASSERT_TRUE(system.GetVirtualCharacterState(worldHandle, characterHandle, state));
        EXPECT_NEAR(state.m_transform.m_position.m_z, disabledTransform.m_position.m_z, 1.0e-6);
        EXPECT_TRUE(state.m_transform.m_rotation.IsClose(disabledTransform.m_rotation));
        EXPECT_TRUE(system.GetEvents(worldHandle).GetVirtualCharacterMoves().empty());

        ShapeConfiguration replacementShapeConfiguration;
        replacementShapeConfiguration.m_geometry = CapsuleShapeConfiguration{
            .m_cylinderHeight = 0.5f,
            .m_radius = 0.25f,
        };
        const ShapeHandle replacementShapeHandle = system.CreateShape(
            worldHandle,
            replacementShapeConfiguration);
        ASSERT_TRUE(replacementShapeHandle);
        EXPECT_FALSE(system.SetVirtualCharacterShape(
            worldHandle,
            characterHandle,
            replacementShapeHandle,
            -1.0f));
        ASSERT_TRUE(system.SetVirtualCharacterShape(
            worldHandle,
            characterHandle,
            replacementShapeHandle,
            0.1f));
        ASSERT_TRUE(system.GetVirtualCharacterState(worldHandle, characterHandle, state));
        EXPECT_EQ(state.m_shapeHandle, replacementShapeHandle);

        EXPECT_TRUE(system.DestroyVirtualCharacter(worldHandle, characterHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, characterHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, replacementShapeHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, characterShapeHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, floorBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, floorShapeHandle));
    }

    TEST(SimulationTests, VirtualCharacterInnerBodyUsesProviderIdentityAndOwnership)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration characterShapeConfiguration;
        characterShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle characterShapeHandle =
            system.CreateShape(worldHandle, characterShapeConfiguration);
        ASSERT_TRUE(characterShapeHandle);

        ShapeConfiguration innerShapeConfiguration;
        innerShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(0.8f, 0.8f, 1.0f),
        };
        const ShapeHandle innerShapeHandle =
            system.CreateShape(worldHandle, innerShapeConfiguration);
        ASSERT_TRUE(innerShapeHandle);

        VirtualCharacterConfiguration characterConfiguration;
        characterConfiguration.m_shapeHandle = characterShapeHandle;
        characterConfiguration.m_innerBodyShapeHandle = innerShapeHandle;
        characterConfiguration.m_transform.m_position = WorldPosition(2.0, 0.0, 1.0);
        characterConfiguration.m_entityId = AZ::EntityId(42);
        characterConfiguration.m_name = AZ::Name::FromStringLiteral("InnerBodyCharacter", nullptr);
        const VirtualCharacterHandle characterHandle =
            system.CreateVirtualCharacter(worldHandle, characterConfiguration);
        ASSERT_TRUE(characterHandle);

        VirtualCharacterState characterState;
        ASSERT_TRUE(system.GetVirtualCharacterState(
            worldHandle,
            characterHandle,
            characterState));
        ASSERT_TRUE(characterState.m_innerBodyHandle);
        EXPECT_EQ(characterState.m_innerBodyShapeHandle, innerShapeHandle);
        EXPECT_FALSE(system.DestroyBody(worldHandle, characterState.m_innerBodyHandle));
        EXPECT_FALSE(system.DestroyShape(worldHandle, innerShapeHandle));

        BodyState innerBodyState;
        ASSERT_TRUE(system.GetBodyState(
            worldHandle,
            characterState.m_innerBodyHandle,
            innerBodyState));
        EXPECT_EQ(innerBodyState.m_entityId, characterConfiguration.m_entityId);
        EXPECT_EQ(innerBodyState.m_name, characterConfiguration.m_name);
        EXPECT_EQ(innerBodyState.m_shapeHandle, innerShapeHandle);
        EXPECT_EQ(innerBodyState.m_kind, BodyKind::Rigid);
        EXPECT_EQ(innerBodyState.m_motionType, MotionType::Kinematic);
        EXPECT_NEAR(innerBodyState.m_transform.m_position.m_x, 2.0, 1.0e-6);
        EXPECT_NEAR(
            innerBodyState.m_transform.m_position.m_z,
            1.0 + characterConfiguration.m_characterPadding,
            1.0e-6);

        RaycastRequest raycastRequest;
        raycastRequest.m_start = WorldPosition(2.0, 0.0, 4.0);
        raycastRequest.m_displacement = AZ::Vector3(0.0f, 0.0f, -6.0f);
        RaycastHit raycastHit;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, raycastRequest, raycastHit));
        EXPECT_EQ(raycastHit.m_bodyHandle, characterState.m_innerBodyHandle);
        EXPECT_EQ(raycastHit.m_shapeHandle, innerShapeHandle);

        WorldTransform movedTransform = characterConfiguration.m_transform;
        movedTransform.m_position.m_x = 4.0;
        ASSERT_TRUE(system.SetVirtualCharacterTransform(
            worldHandle,
            characterHandle,
            movedTransform));
        ASSERT_TRUE(system.GetBodyState(
            worldHandle,
            characterState.m_innerBodyHandle,
            innerBodyState));
        EXPECT_NEAR(innerBodyState.m_transform.m_position.m_x, 4.0, 1.0e-6);

        raycastRequest.m_start.m_x = 4.0;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, raycastRequest, raycastHit));
        EXPECT_EQ(raycastHit.m_bodyHandle, characterState.m_innerBodyHandle);

        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);
        WorldTransform changedTransform = movedTransform;
        changedTransform.m_position.m_x = 6.0;
        ASSERT_TRUE(system.SetVirtualCharacterTransform(
            worldHandle,
            characterHandle,
            changedTransform));
        StateValidationResult validationResult;
        ASSERT_TRUE(system.ValidateWorldState(
            worldHandle,
            snapshotHandle,
            validationResult));
        EXPECT_FALSE(validationResult.m_matches);
        ASSERT_TRUE(system.GetVirtualCharacterState(
            worldHandle,
            characterHandle,
            characterState));
        EXPECT_NEAR(characterState.m_transform.m_position.m_x, 6.0, 1.0e-6);
        ASSERT_TRUE(system.RestoreWorldState(worldHandle, snapshotHandle));
        ASSERT_TRUE(system.GetVirtualCharacterState(
            worldHandle,
            characterHandle,
            characterState));
        EXPECT_NEAR(characterState.m_transform.m_position.m_x, 4.0, 1.0e-6);
        ASSERT_TRUE(system.GetBodyState(
            worldHandle,
            characterState.m_innerBodyHandle,
            innerBodyState));
        EXPECT_NEAR(innerBodyState.m_transform.m_position.m_x, 4.0, 1.0e-6);
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));

        ShapeConfiguration replacementShapeConfiguration;
        replacementShapeConfiguration.m_geometry = CapsuleShapeConfiguration{
            .m_cylinderHeight = 0.7f,
            .m_radius = 0.25f,
        };
        const ShapeHandle replacementShapeHandle =
            system.CreateShape(worldHandle, replacementShapeConfiguration);
        ASSERT_TRUE(replacementShapeHandle);
        ASSERT_TRUE(system.SetVirtualCharacterInnerBodyShape(
            worldHandle,
            characterHandle,
            replacementShapeHandle));
        ASSERT_TRUE(system.GetVirtualCharacterState(
            worldHandle,
            characterHandle,
            characterState));
        EXPECT_EQ(characterState.m_innerBodyShapeHandle, replacementShapeHandle);
        EXPECT_TRUE(system.DestroyShape(worldHandle, innerShapeHandle));
        EXPECT_FALSE(system.DestroyShape(worldHandle, replacementShapeHandle));

        const BodyHandle innerBodyHandle = characterState.m_innerBodyHandle;
        EXPECT_TRUE(system.DestroyVirtualCharacter(worldHandle, characterHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, characterHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, innerBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, replacementShapeHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, characterShapeHandle));
    }

    TEST(SimulationTests, VirtualCharacterCollisionChecksAreBoundedDeterministicAndNonMutating)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration bodyShapeConfiguration;
        bodyShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(1.0f, 1.0f, 1.0f),
        };
        const ShapeHandle bodyShapeHandle =
            system.CreateShape(worldHandle, bodyShapeConfiguration);
        ASSERT_TRUE(bodyShapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = bodyShapeHandle;
        bodyConfiguration.m_transform.m_position.m_x = -0.5;
        bodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        ShapeConfiguration characterShapeConfiguration;
        characterShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle characterShapeHandle =
            system.CreateShape(worldHandle, characterShapeConfiguration);
        ASSERT_TRUE(characterShapeHandle);

        VirtualCharacterConfiguration characterConfiguration;
        characterConfiguration.m_shapeHandle = characterShapeHandle;
        characterConfiguration.m_transform.m_position = WorldPosition(10.0, 0.0, 0.0);
        const VirtualCharacterHandle characterHandle =
            system.CreateVirtualCharacter(worldHandle, characterConfiguration);
        ASSERT_TRUE(characterHandle);

        characterConfiguration.m_transform.m_position = WorldPosition(0.8, 0.0, 0.0);
        const VirtualCharacterHandle otherCharacterHandle =
            system.CreateVirtualCharacter(worldHandle, characterConfiguration);
        ASSERT_TRUE(otherCharacterHandle);

        CharacterCollisionRequest request;
        request.m_transform.m_position = WorldPosition(0.0, 0.0, 0.0);
        request.m_movementDirection = AZ::Vector3::CreateAxisX();
        AZStd::array<CharacterCollisionHit, 8> hits;
        const QueryResult result = system.CheckVirtualCharacterCollision(
            worldHandle,
            characterHandle,
            request,
            hits,
            nullptr);
        ASSERT_TRUE(result.IsComplete());
        ASSERT_GE(result.m_hitCount, 2);

        bool foundBody = false;
        bool foundCharacter = false;
        for (AZ::u32 hitIndex = 0; hitIndex < result.m_hitCount; ++hitIndex)
        {
            const CharacterCollisionHit& hit = hits[hitIndex];
            EXPECT_TRUE(hit.m_penetrationAxis.IsFinite());
            EXPECT_TRUE(std::isfinite(hit.m_penetrationDepth));
            if (hit.m_bodyHandle == bodyHandle)
            {
                foundBody = true;
                EXPECT_FALSE(hit.m_characterHandle);
                EXPECT_FALSE(hit.m_virtualCharacterHandle);
                EXPECT_EQ(hit.m_shapeHandle, bodyShapeHandle);
            }
            if (hit.m_virtualCharacterHandle == otherCharacterHandle)
            {
                foundCharacter = true;
                EXPECT_FALSE(hit.m_bodyHandle);
                EXPECT_EQ(hit.m_shapeHandle, characterShapeHandle);
            }
        }
        EXPECT_TRUE(foundBody);
        EXPECT_TRUE(foundCharacter);

        VirtualCharacterState unchangedState;
        ASSERT_TRUE(system.GetVirtualCharacterState(
            worldHandle,
            characterHandle,
            unchangedState));
        EXPECT_NEAR(unchangedState.m_transform.m_position.m_x, 10.0, 1.0e-6);

        AZStd::array<CharacterCollisionHit, 1> boundedHits;
        const QueryResult boundedResult = system.CheckVirtualCharacterCollision(
            worldHandle,
            characterHandle,
            request,
            boundedHits,
            nullptr);
        ASSERT_EQ(boundedResult.m_hitCount, 1);
        EXPECT_EQ(boundedResult.m_requiredHitCount, result.m_requiredHitCount);
        EXPECT_FALSE(boundedResult.IsComplete());
        EXPECT_EQ(boundedHits.front().m_bodyHandle, hits.front().m_bodyHandle);
        EXPECT_EQ(boundedHits.front().m_characterHandle, hits.front().m_characterHandle);
        EXPECT_EQ(
            boundedHits.front().m_virtualCharacterHandle,
            hits.front().m_virtualCharacterHandle);
        EXPECT_EQ(boundedHits.front().m_shapeHandle, hits.front().m_shapeHandle);

        RecordingCharacterCollisionFilter filter;
        filter.m_rejectedVirtualCharacterHandle = otherCharacterHandle;
        filter.m_rejectedShapeKind = ShapeKind::Box;
        const QueryResult filteredResult = system.CheckVirtualCharacterCollision(
            worldHandle,
            characterHandle,
            request,
            hits,
            &filter);
        EXPECT_EQ(filteredResult.m_hitCount, 0);
        EXPECT_TRUE(filteredResult.IsComplete());
        EXPECT_GT(filter.m_bodyCallCount, 0);
        EXPECT_GT(filter.m_shapeCallCount, 0);
        EXPECT_GT(filter.m_virtualCharacterCallCount, 0);

        EXPECT_TRUE(system.DestroyVirtualCharacter(worldHandle, otherCharacterHandle));
        EXPECT_TRUE(system.DestroyVirtualCharacter(worldHandle, characterHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, characterShapeHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, bodyShapeHandle));
    }

    TEST(SimulationTests, VirtualCharacterRuntimeLocomotionAndContactsUseProviderTypes)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration floorShapeConfiguration;
        floorShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(20.0f, 20.0f, 1.0f),
        };
        const ShapeHandle floorShapeHandle = system.CreateShape(worldHandle, floorShapeConfiguration);
        ASSERT_TRUE(floorShapeHandle);

        BodyConfiguration floorConfiguration;
        floorConfiguration.m_shapeHandle = floorShapeHandle;
        floorConfiguration.m_transform.m_position.m_z = -0.5;
        floorConfiguration.m_motionType = MotionType::Static;
        const BodyHandle floorBodyHandle = system.CreateBody(worldHandle, floorConfiguration);
        ASSERT_TRUE(floorBodyHandle);

        ShapeConfiguration characterShapeConfiguration;
        characterShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle characterShapeHandle = system.CreateShape(worldHandle, characterShapeConfiguration);
        ASSERT_TRUE(characterShapeHandle);

        VirtualCharacterConfiguration characterConfiguration;
        characterConfiguration.m_shapeHandle = characterShapeHandle;
        characterConfiguration.m_transform.m_position.m_z = 0.5;
        const VirtualCharacterHandle characterHandle =
            system.CreateVirtualCharacter(worldHandle, characterConfiguration);
        ASSERT_TRUE(characterHandle);
        RecordingVirtualCharacterContactCallbacks callbacks;
        ScopedExtensionRegistration registration(system, &callbacks);
        ASSERT_TRUE(system.SetVirtualCharacterContactCallbacks(
            worldHandle,
            characterHandle,
            registration.GetHandle()));

        VirtualCharacterRuntimeConfiguration runtimeConfiguration;
        ASSERT_TRUE(system.GetVirtualCharacterRuntimeConfiguration(
            worldHandle,
            characterHandle,
            runtimeConfiguration));
        runtimeConfiguration.m_shapeOffset = AZ::Vector3(0.01f, 0.02f, 0.03f);
        runtimeConfiguration.m_supportingPlaneNormal = AZ::Vector3(0.0f, 0.1f, 1.0f);
        runtimeConfiguration.m_up = AZ::Vector3(0.0f, 0.05f, 1.0f);
        runtimeConfiguration.m_hitReductionCosMaximumAngle = 0.95f;
        runtimeConfiguration.m_mass = 85.0f;
        runtimeConfiguration.m_maximumSlopeAngle = 0.65f;
        runtimeConfiguration.m_maximumStrength = 125.0f;
        runtimeConfiguration.m_penetrationRecoverySpeed = 0.75f;
        runtimeConfiguration.m_supportingPlaneDistance = -0.25f;
        runtimeConfiguration.m_maximumHitCount = 64;
        runtimeConfiguration.m_enhancedInternalEdgeRemoval = true;
        ASSERT_TRUE(system.UpdateVirtualCharacterRuntimeConfiguration(
            worldHandle,
            characterHandle,
            runtimeConfiguration));

        VirtualCharacterRuntimeConfiguration updatedRuntimeConfiguration;
        ASSERT_TRUE(system.GetVirtualCharacterRuntimeConfiguration(
            worldHandle,
            characterHandle,
            updatedRuntimeConfiguration));
        EXPECT_TRUE(updatedRuntimeConfiguration.m_shapeOffset.IsClose(runtimeConfiguration.m_shapeOffset));
        EXPECT_TRUE(
            updatedRuntimeConfiguration.m_supportingPlaneNormal.IsClose(
                runtimeConfiguration.m_supportingPlaneNormal.GetNormalized()));
        EXPECT_TRUE(updatedRuntimeConfiguration.m_up.IsClose(runtimeConfiguration.m_up.GetNormalized()));
        EXPECT_FLOAT_EQ(
            updatedRuntimeConfiguration.m_hitReductionCosMaximumAngle,
            runtimeConfiguration.m_hitReductionCosMaximumAngle);
        EXPECT_FLOAT_EQ(updatedRuntimeConfiguration.m_mass, runtimeConfiguration.m_mass);
        EXPECT_NEAR(
            updatedRuntimeConfiguration.m_maximumSlopeAngle,
            runtimeConfiguration.m_maximumSlopeAngle,
            1.0e-6f);
        EXPECT_FLOAT_EQ(
            updatedRuntimeConfiguration.m_maximumStrength,
            runtimeConfiguration.m_maximumStrength);
        EXPECT_FLOAT_EQ(
            updatedRuntimeConfiguration.m_penetrationRecoverySpeed,
            runtimeConfiguration.m_penetrationRecoverySpeed);
        EXPECT_FLOAT_EQ(
            updatedRuntimeConfiguration.m_supportingPlaneDistance,
            runtimeConfiguration.m_supportingPlaneDistance);
        EXPECT_EQ(updatedRuntimeConfiguration.m_maximumHitCount, runtimeConfiguration.m_maximumHitCount);
        EXPECT_EQ(
            updatedRuntimeConfiguration.m_enhancedInternalEdgeRemoval,
            runtimeConfiguration.m_enhancedInternalEdgeRemoval);

        VirtualCharacterRuntimeConfiguration invalidRuntimeConfiguration = runtimeConfiguration;
        invalidRuntimeConfiguration.m_mass = 0.0f;
        EXPECT_FALSE(system.UpdateVirtualCharacterRuntimeConfiguration(
            worldHandle,
            characterHandle,
            invalidRuntimeConfiguration));

        runtimeConfiguration.m_shapeOffset = AZ::Vector3::CreateZero();
        ASSERT_TRUE(system.UpdateVirtualCharacterRuntimeConfiguration(
            worldHandle,
            characterHandle,
            runtimeConfiguration));

        VirtualCharacterUpdateConfiguration directUpdateConfiguration;
        directUpdateConfiguration.m_extended = false;
        ASSERT_TRUE(system.SetVirtualCharacterVelocity(
            worldHandle,
            characterHandle,
            AZ::Vector3(0.0f, 0.0f, -1.0f)));
        ASSERT_TRUE(system.UpdateVirtualCharacter(
            worldHandle,
            characterHandle,
            1.0f / 60.0f,
            directUpdateConfiguration));

        ASSERT_TRUE(system.RefreshVirtualCharacterContacts(
            worldHandle,
            characterHandle));
        AZStd::array<VirtualCharacterContact, 32> contacts;
        QueryResult contactResult = system.GetVirtualCharacterContacts(
            worldHandle,
            characterHandle,
            contacts);
        ASSERT_TRUE(contactResult.IsComplete());
        ASSERT_GT(contactResult.m_hitCount, 0);
        bool foundFloor = false;
        for (AZ::u32 contactIndex = 0; contactIndex < contactResult.m_hitCount; ++contactIndex)
        {
            const VirtualCharacterContact& contact = contacts[contactIndex];
            if (contact.m_bodyHandle == floorBodyHandle)
            {
                foundFloor = true;
                EXPECT_EQ(contact.m_shapeHandle, floorShapeHandle);
                EXPECT_TRUE(contact.m_hadCollision);
                EXPECT_TRUE(std::isfinite(contact.m_position.m_x));
                EXPECT_TRUE(std::isfinite(contact.m_position.m_y));
                EXPECT_TRUE(std::isfinite(contact.m_position.m_z));
                EXPECT_TRUE(contact.m_contactNormal.IsFinite());
                EXPECT_TRUE(contact.m_surfaceNormal.IsFinite());
            }
        }
        EXPECT_TRUE(foundFloor);
        EXPECT_TRUE(system.HasVirtualCharacterCollidedWith(
            worldHandle,
            characterHandle,
            floorBodyHandle));
        EXPECT_GT(callbacks.m_bodyValidationCount, 0);
        EXPECT_GT(callbacks.m_bodyAddedCount, 0);
        EXPECT_GT(callbacks.m_adjustBodyVelocityCount, 0);
        EXPECT_GT(callbacks.m_bodySolveCount, 0);
        EXPECT_EQ(callbacks.m_characterHandle, characterHandle);
        EXPECT_EQ(callbacks.m_bodyHandle, floorBodyHandle);
        ASSERT_TRUE(system.RefreshVirtualCharacterContacts(
            worldHandle,
            characterHandle));
        EXPECT_GT(callbacks.m_bodyPersistedCount, 0);

        const AZ::Vector3 desiredVelocity(1.0f, 0.0f, 0.0f);
        AZ::Vector3 adjustedVelocity;
        ASSERT_TRUE(system.CancelVirtualCharacterVelocityTowardsSteepSlopes(
            worldHandle,
            characterHandle,
            desiredVelocity,
            adjustedVelocity));
        EXPECT_TRUE(adjustedVelocity.IsClose(desiredVelocity));
        EXPECT_FALSE(system.CanVirtualCharacterWalkStairs(
            worldHandle,
            characterHandle,
            desiredVelocity));
        EXPECT_TRUE(system.UpdateVirtualCharacterGroundVelocity(worldHandle, characterHandle));

        WorldTransform raisedTransform = characterConfiguration.m_transform;
        raisedTransform.m_position.m_z = 0.65;
        ASSERT_TRUE(system.SetVirtualCharacterTransform(
            worldHandle,
            characterHandle,
            raisedTransform));
        ASSERT_TRUE(system.StickVirtualCharacterToFloor(
            worldHandle,
            characterHandle,
            AZ::Vector3(0.0f, 0.0f, -0.3f)));

        VirtualCharacterState groundedState;
        ASSERT_TRUE(system.GetVirtualCharacterState(worldHandle, characterHandle, groundedState));
        EXPECT_NEAR(groundedState.m_transform.m_position.m_z, 0.5, 0.05);

        RecordingQueryFilter filter;
        filter.m_rejectedBodyHandle = floorBodyHandle;
        ASSERT_TRUE(system.SetVirtualCharacterTransform(
            worldHandle,
            characterHandle,
            raisedTransform));
        EXPECT_FALSE(system.StickVirtualCharacterToFloor(
            worldHandle,
            characterHandle,
            AZ::Vector3(0.0f, 0.0f, -0.3f),
            &filter));
        EXPECT_GT(filter.m_bodyCallCount, 0);

        VirtualCharacterConfiguration otherConfiguration = characterConfiguration;
        otherConfiguration.m_transform.m_position = WorldPosition(0.8, 0.0, 0.5);
        const VirtualCharacterHandle otherCharacterHandle =
            system.CreateVirtualCharacter(worldHandle, otherConfiguration);
        ASSERT_TRUE(otherCharacterHandle);
        ASSERT_TRUE(system.SetVirtualCharacterTransform(
            worldHandle,
            characterHandle,
            characterConfiguration.m_transform));
        ASSERT_TRUE(system.BeginVirtualCharacterContactTracking(worldHandle, characterHandle));
        ASSERT_TRUE(system.BeginVirtualCharacterContactTracking(worldHandle, characterHandle));
        ASSERT_TRUE(system.SetVirtualCharacterVelocity(
            worldHandle,
            characterHandle,
            AZ::Vector3(1.0f, 0.0f, 0.0f)));
        ASSERT_TRUE(system.UpdateVirtualCharacter(
            worldHandle,
            characterHandle,
            1.0f / 60.0f,
            directUpdateConfiguration));
        ASSERT_TRUE(system.RefreshVirtualCharacterContacts(worldHandle, characterHandle));
        ASSERT_TRUE(system.EndVirtualCharacterContactTracking(worldHandle, characterHandle));
        ASSERT_TRUE(system.EndVirtualCharacterContactTracking(worldHandle, characterHandle));
        EXPECT_FALSE(system.EndVirtualCharacterContactTracking(worldHandle, characterHandle));

        contactResult = system.GetVirtualCharacterContacts(
            worldHandle,
            characterHandle,
            contacts);
        ASSERT_TRUE(contactResult.IsComplete());
        bool foundCharacter = false;
        for (AZ::u32 contactIndex = 0; contactIndex < contactResult.m_hitCount; ++contactIndex)
        {
            const VirtualCharacterContact& contact = contacts[contactIndex];
            if (contact.m_characterHandle == otherCharacterHandle)
            {
                foundCharacter = true;
                EXPECT_EQ(contact.m_shapeHandle, characterShapeHandle);
            }
        }
        EXPECT_TRUE(foundCharacter);
        EXPECT_TRUE(system.HaveVirtualCharactersCollided(
            worldHandle,
            characterHandle,
            otherCharacterHandle));
        EXPECT_GT(callbacks.m_characterValidationCount, 0);
        EXPECT_GT(callbacks.m_characterAddedCount, 0);
        EXPECT_GT(callbacks.m_characterSolveCount, 0);
        EXPECT_EQ(callbacks.m_otherCharacterHandle, otherCharacterHandle);

        VirtualCharacterStairConfiguration invalidStairConfiguration;
        EXPECT_FALSE(system.WalkVirtualCharacterStairs(
            worldHandle,
            characterHandle,
            invalidStairConfiguration));

        EXPECT_TRUE(system.DestroyVirtualCharacter(worldHandle, otherCharacterHandle));
        ASSERT_TRUE(system.RefreshVirtualCharacterContacts(worldHandle, characterHandle));
        EXPECT_GT(callbacks.m_characterRemovedCount, 0);
        EXPECT_EQ(callbacks.m_removedCharacterHandle, otherCharacterHandle);

        EXPECT_TRUE(system.DestroyBody(worldHandle, floorBodyHandle));
        ASSERT_TRUE(system.RefreshVirtualCharacterContacts(worldHandle, characterHandle));
        EXPECT_GT(callbacks.m_bodyRemovedCount, 0);
        EXPECT_EQ(callbacks.m_removedBodyHandle, floorBodyHandle);
        EXPECT_TRUE(system.SetVirtualCharacterContactCallbacks(
            worldHandle,
            characterHandle,
            ExtensionHandle::Invalid));
        EXPECT_TRUE(system.DestroyVirtualCharacter(worldHandle, characterHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, characterShapeHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, floorShapeHandle));
    }

    TEST(SimulationTests, CharactersOwnBodiesAndRefreshGroundStateAfterSimulation)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration floorShapeConfiguration;
        floorShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(20.0f, 20.0f, 1.0f),
        };
        const ShapeHandle floorShapeHandle = system.CreateShape(worldHandle, floorShapeConfiguration);
        ASSERT_TRUE(floorShapeHandle);

        BodyConfiguration floorConfiguration;
        floorConfiguration.m_shapeHandle = floorShapeHandle;
        floorConfiguration.m_transform.m_position.m_z = -0.5;
        floorConfiguration.m_motionType = MotionType::Static;
        const BodyHandle floorBodyHandle = system.CreateBody(worldHandle, floorConfiguration);
        ASSERT_TRUE(floorBodyHandle);

        ShapeConfiguration characterShapeConfiguration;
        characterShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle characterShapeHandle = system.CreateShape(worldHandle, characterShapeConfiguration);
        ASSERT_TRUE(characterShapeHandle);

        CharacterConfiguration characterConfiguration;
        characterConfiguration.m_shapeHandle = characterShapeHandle;
        characterConfiguration.m_transform.m_position.m_z = 2.0;
        characterConfiguration.m_userData = 0x1234'5678'9abc'def0;
        const CharacterHandle characterHandle = system.CreateCharacter(worldHandle, characterConfiguration);
        ASSERT_TRUE(characterHandle);
        EXPECT_FALSE(system.DestroyShape(worldHandle, characterShapeHandle));

        CharacterState initialState;
        ASSERT_TRUE(system.GetCharacterState(worldHandle, characterHandle, initialState));
        ASSERT_TRUE(initialState.m_bodyHandle);
        EXPECT_EQ(initialState.m_userData, characterConfiguration.m_userData);
        AZ::u64 userData = 0;
        ASSERT_TRUE(system.GetCharacterUserData(worldHandle, characterHandle, userData));
        EXPECT_EQ(userData, characterConfiguration.m_userData);
        constexpr AZ::u64 updatedUserData = 0x1111'2222'3333'4444;
        ASSERT_TRUE(system.SetCharacterUserData(worldHandle, characterHandle, updatedUserData));
        ASSERT_TRUE(system.GetCharacterUserData(worldHandle, characterHandle, userData));
        EXPECT_EQ(userData, updatedUserData);
        ASSERT_TRUE(system.GetBodyUserData(worldHandle, initialState.m_bodyHandle, userData));
        EXPECT_EQ(userData, updatedUserData);
        ASSERT_TRUE(system.GetCharacterState(worldHandle, characterHandle, initialState));
        EXPECT_EQ(initialState.m_userData, updatedUserData);
        EXPECT_FALSE(system.DestroyBody(worldHandle, initialState.m_bodyHandle));
        EXPECT_FALSE(system.RemoveBodyFromSimulation(worldHandle, initialState.m_bodyHandle));

        characterConfiguration.m_transform.m_position = WorldPosition(0.8, 0.0, 2.0);
        const CharacterHandle otherCharacterHandle =
            system.CreateCharacter(worldHandle, characterConfiguration);
        ASSERT_TRUE(otherCharacterHandle);
        CharacterState otherCharacterState;
        ASSERT_TRUE(system.GetCharacterState(
            worldHandle,
            otherCharacterHandle,
            otherCharacterState));

        CharacterCollisionRequest collisionRequest;
        collisionRequest.m_transform = initialState.m_transform;
        collisionRequest.m_movementDirection = AZ::Vector3::CreateAxisX();
        AZStd::array<CharacterCollisionHit, 2> collisionHits;
        QueryResult collisionResult = system.CheckCharacterCollision(
            worldHandle,
            characterHandle,
            collisionRequest,
            collisionHits,
            nullptr);
        ASSERT_TRUE(collisionResult.IsComplete());
        ASSERT_EQ(collisionResult.m_hitCount, 1);
        EXPECT_EQ(collisionHits.front().m_bodyHandle, otherCharacterState.m_bodyHandle);
        EXPECT_EQ(collisionHits.front().m_characterHandle, otherCharacterHandle);
        EXPECT_FALSE(collisionHits.front().m_virtualCharacterHandle);
        EXPECT_EQ(collisionHits.front().m_shapeHandle, characterShapeHandle);

        RecordingCharacterCollisionFilter collisionFilter;
        collisionResult = system.CheckCharacterCollision(
            worldHandle,
            characterHandle,
            collisionRequest,
            collisionHits,
            &collisionFilter);
        ASSERT_EQ(collisionResult.m_hitCount, 1);
        EXPECT_GT(collisionFilter.m_shapePairCallCount, 0);
        EXPECT_EQ(collisionFilter.m_lastQueryShape.m_rootShapeHandle, characterShapeHandle);
        EXPECT_EQ(collisionFilter.m_lastQueryShape.m_kind, ShapeKind::Sphere);
        EXPECT_EQ(collisionFilter.m_lastTargetShape.m_bodyHandle, otherCharacterState.m_bodyHandle);
        EXPECT_EQ(collisionFilter.m_lastTargetShape.m_rootShapeHandle, characterShapeHandle);
        EXPECT_EQ(collisionFilter.m_lastTargetShape.m_kind, ShapeKind::Sphere);

        collisionFilter.m_rejectedCharacterHandle = otherCharacterHandle;
        collisionResult = system.CheckCharacterCollision(
            worldHandle,
            characterHandle,
            collisionRequest,
            collisionHits,
            &collisionFilter);
        EXPECT_TRUE(collisionResult.IsComplete());
        EXPECT_EQ(collisionResult.m_hitCount, 0);
        EXPECT_GT(collisionFilter.m_bodyCallCount, 0);
        EXPECT_GT(collisionFilter.m_characterCallCount, 0);
        EXPECT_TRUE(system.DestroyCharacter(worldHandle, otherCharacterHandle));

        CharacterRuntimeConfiguration runtimeConfiguration;
        ASSERT_TRUE(system.GetCharacterRuntimeConfiguration(
            worldHandle,
            characterHandle,
            runtimeConfiguration));
        runtimeConfiguration.m_supportingPlaneNormal = AZ::Vector3(0.0f, 0.1f, 1.0f);
        runtimeConfiguration.m_up = AZ::Vector3(0.0f, 0.05f, 1.0f);
        runtimeConfiguration.m_maximumSeparationDistance = 0.075f;
        runtimeConfiguration.m_maximumSlopeAngle = 0.7f;
        runtimeConfiguration.m_supportingPlaneDistance = -0.3f;
        ASSERT_TRUE(system.UpdateCharacterRuntimeConfiguration(
            worldHandle,
            characterHandle,
            runtimeConfiguration));

        CharacterRuntimeConfiguration updatedRuntimeConfiguration;
        ASSERT_TRUE(system.GetCharacterRuntimeConfiguration(
            worldHandle,
            characterHandle,
            updatedRuntimeConfiguration));
        EXPECT_EQ(updatedRuntimeConfiguration.m_objectLayer, runtimeConfiguration.m_objectLayer);
        EXPECT_TRUE(
            updatedRuntimeConfiguration.m_supportingPlaneNormal.IsClose(
                runtimeConfiguration.m_supportingPlaneNormal.GetNormalized()));
        EXPECT_TRUE(updatedRuntimeConfiguration.m_up.IsClose(runtimeConfiguration.m_up.GetNormalized()));
        EXPECT_FLOAT_EQ(
            updatedRuntimeConfiguration.m_maximumSeparationDistance,
            runtimeConfiguration.m_maximumSeparationDistance);
        EXPECT_NEAR(
            updatedRuntimeConfiguration.m_maximumSlopeAngle,
            runtimeConfiguration.m_maximumSlopeAngle,
            1.0e-6f);
        EXPECT_FLOAT_EQ(
            updatedRuntimeConfiguration.m_supportingPlaneDistance,
            runtimeConfiguration.m_supportingPlaneDistance);

        CharacterRuntimeConfiguration invalidRuntimeConfiguration = runtimeConfiguration;
        invalidRuntimeConfiguration.m_up = AZ::Vector3::CreateZero();
        EXPECT_FALSE(system.UpdateCharacterRuntimeConfiguration(
            worldHandle,
            characterHandle,
            invalidRuntimeConfiguration));
        EXPECT_FALSE(system.SetBodyMotionType(
            worldHandle,
            initialState.m_bodyHandle,
            MotionType::Kinematic,
            false));
        EXPECT_FALSE(system.SetBodyObjectLayer(
            worldHandle,
            initialState.m_bodyHandle,
            DefaultLayers::NonMoving));

        ShapeConfiguration replacementShapeConfiguration;
        replacementShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.4f};
        const ShapeHandle replacementShapeHandle = system.CreateShape(
            worldHandle,
            replacementShapeConfiguration);
        ASSERT_TRUE(replacementShapeHandle);
        EXPECT_FALSE(system.SetCharacterShape(
            worldHandle,
            characterHandle,
            replacementShapeHandle,
            -1.0f));
        ASSERT_TRUE(system.SetCharacterShape(
            worldHandle,
            characterHandle,
            replacementShapeHandle,
            0.1f));

        WorldTransform movedCharacterTransform;
        movedCharacterTransform.m_position = {
            .m_x = 1.0,
            .m_z = 3.0,
        };
        ASSERT_TRUE(system.SetCharacterTransform(
            worldHandle,
            characterHandle,
            movedCharacterTransform,
            true));
        ASSERT_TRUE(system.SetCharacterVelocity(
            worldHandle,
            characterHandle,
            AZ::Vector3::CreateAxisX(2.0f)));
        ASSERT_TRUE(system.AddCharacterImpulse(
            worldHandle,
            characterHandle,
            AZ::Vector3::CreateAxisZ(80.0f)));

        CharacterState mutatedState;
        ASSERT_TRUE(system.GetCharacterState(worldHandle, characterHandle, mutatedState));
        EXPECT_EQ(mutatedState.m_shapeHandle, replacementShapeHandle);
        EXPECT_EQ(mutatedState.m_transform.m_position, movedCharacterTransform.m_position);
        EXPECT_FLOAT_EQ(mutatedState.m_linearVelocity.GetX(), 2.0f);
        EXPECT_GT(mutatedState.m_linearVelocity.GetZ(), 0.0f);

        for (AZ::u32 step = 0; step < 120; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        CharacterState state;
        ASSERT_TRUE(system.GetCharacterState(worldHandle, characterHandle, state));
        EXPECT_EQ(state.m_bodyHandle, initialState.m_bodyHandle);
        EXPECT_EQ(state.m_groundBodyHandle, floorBodyHandle);
        EXPECT_EQ(state.m_groundState, GroundState::OnGround);
        EXPECT_TRUE(state.m_isSupported);

        DebugCaptureConfiguration captureConfiguration;
        captureConfiguration.m_flags = DebugCaptureFlags::CharacterGround;
        ASSERT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        DebugCaptureStatistics captureStatistics;
        ASSERT_TRUE(system.GetDebugCaptureStatistics(worldHandle, captureStatistics));
        EXPECT_GT(captureStatistics.m_lineCount, 0);
        captureConfiguration.m_flags = DebugCaptureFlags::None;
        ASSERT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));

        EXPECT_TRUE(system.DestroyCharacter(worldHandle, characterHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, characterHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, initialState.m_bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, replacementShapeHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, characterShapeHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, floorBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, floorShapeHandle));
    }

    TEST(SimulationTests, VehiclesOwnTheirChassisConstraintAndExposeBoundedState)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration floorShapeConfiguration;
        floorShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(40.0f, 40.0f, 1.0f),
        };
        const ShapeHandle floorShapeHandle = system.CreateShape(worldHandle, floorShapeConfiguration);
        ASSERT_TRUE(floorShapeHandle);

        BodyConfiguration floorConfiguration;
        floorConfiguration.m_shapeHandle = floorShapeHandle;
        floorConfiguration.m_transform.m_position.m_z = -0.5;
        floorConfiguration.m_motionType = MotionType::Static;
        const BodyHandle floorBodyHandle = system.CreateBody(worldHandle, floorConfiguration);
        ASSERT_TRUE(floorBodyHandle);

        ShapeConfiguration chassisShapeConfiguration;
        chassisShapeConfiguration.m_density = 250.0f;
        chassisShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(1.8f, 4.0f, 0.6f),
        };
        const ShapeHandle chassisShapeHandle = system.CreateShape(worldHandle, chassisShapeConfiguration);
        ASSERT_TRUE(chassisShapeHandle);

        BodyConfiguration chassisConfiguration;
        chassisConfiguration.m_shapeHandle = chassisShapeHandle;
        chassisConfiguration.m_transform.m_position.m_z = 0.8;
        chassisConfiguration.m_gravityFactor = 0.75f;
        const BodyHandle chassisBodyHandle = system.CreateBody(worldHandle, chassisConfiguration);
        ASSERT_TRUE(chassisBodyHandle);

        WheeledVehicleConfiguration vehicleConfiguration;
        vehicleConfiguration.m_bodyHandle = chassisBodyHandle;
        vehicleConfiguration.m_wheels = {
            {.m_position = AZ::Vector3(-0.8f, 1.4f, -0.25f), .m_maximumHandBrakeTorque = 0.0f},
            {.m_position = AZ::Vector3(0.8f, 1.4f, -0.25f), .m_maximumHandBrakeTorque = 0.0f},
            {.m_position = AZ::Vector3(-0.8f, -1.4f, -0.25f), .m_maximumSteerAngle = 0.0f},
            {.m_position = AZ::Vector3(0.8f, -1.4f, -0.25f), .m_maximumSteerAngle = 0.0f},
        };
        vehicleConfiguration.m_differentials = {
            {.m_leftWheel = 0, .m_rightWheel = 1},
        };
        vehicleConfiguration.m_antiRollBars = {
            {.m_leftWheel = 0, .m_rightWheel = 1},
            {.m_leftWheel = 2, .m_rightWheel = 3},
        };
        WheeledVehicleConfiguration invalidDifferentialConfiguration = vehicleConfiguration;
        invalidDifferentialConfiguration.m_differentials[0].m_engineTorqueRatio = 0.8f;
        EXPECT_FALSE(system.CreateWheeledVehicle(worldHandle, invalidDifferentialConfiguration));
        const VehicleHandle vehicleHandle =
            system.CreateWheeledVehicle(worldHandle, vehicleConfiguration);
        ASSERT_TRUE(vehicleHandle);
        EXPECT_FALSE(system.DestroyBody(worldHandle, chassisBodyHandle));

        const StateSnapshotHandle callbackSnapshot = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(callbackSnapshot);
        RecordingVehicleCallbacks vehicleCallbacks;
        ScopedExtensionRegistration vehicleCallbacksRegistration(system, &vehicleCallbacks);
        ASSERT_TRUE(system.SetVehicleCallbacks(
            worldHandle,
            vehicleHandle,
            vehicleCallbacksRegistration.GetHandle()));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, callbackSnapshot));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, callbackSnapshot));

        RecordingVehicleCollisionFilter vehicleCollisionFilter;
        ScopedExtensionRegistration vehicleFilterRegistration(system, &vehicleCollisionFilter);
        ASSERT_TRUE(system.SetVehicleCollisionFilter(
            worldHandle,
            vehicleHandle,
            vehicleFilterRegistration.GetHandle()));
        vehicleCollisionFilter.m_rejectedBodyHandle = floorBodyHandle;
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        AZStd::array<WheelState, 4> filteredWheels;
        WheeledVehicleState filteredState;
        ASSERT_TRUE(system.GetWheeledVehicleState(
            worldHandle,
            vehicleHandle,
            filteredState,
            filteredWheels).IsComplete());
        EXPECT_TRUE(AZStd::none_of(
            filteredWheels.begin(),
            filteredWheels.end(),
            [](const WheelState& wheel)
            {
                return wheel.m_hasContact;
            }));
        vehicleCollisionFilter.m_rejectedBodyHandle = BodyHandle::Invalid;

        DebugCaptureConfiguration captureConfiguration;
        captureConfiguration.m_flags = DebugCaptureFlags::VehicleContacts;
        ASSERT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        DebugCaptureStatistics captureStatistics;
        ASSERT_TRUE(system.GetDebugCaptureStatistics(worldHandle, captureStatistics));
        EXPECT_GT(captureStatistics.m_lineCount, 0);
        captureConfiguration.m_flags = DebugCaptureFlags::None;
        ASSERT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));

        VehicleRuntimeConfiguration runtimeConfiguration;
        ASSERT_TRUE(system.GetVehicleRuntimeConfiguration(
            worldHandle,
            vehicleHandle,
            runtimeConfiguration));
        EXPECT_FALSE(runtimeConfiguration.m_overrideGravity);
        EXPECT_FLOAT_EQ(runtimeConfiguration.m_maximumPitchRollAngle, AZ::Constants::Pi);
        EXPECT_EQ(runtimeConfiguration.m_collisionTestIntervalActive, 1);
        EXPECT_EQ(runtimeConfiguration.m_collisionTestIntervalInactive, 1);

        runtimeConfiguration.m_gravityOverride = AZ::Vector3(0.0f, 0.0f, -4.0f);
        runtimeConfiguration.m_maximumPitchRollAngle = AZ::Constants::QuarterPi;
        runtimeConfiguration.m_collisionTestIntervalActive = 2;
        runtimeConfiguration.m_collisionTestIntervalInactive = 5;
        runtimeConfiguration.m_overrideGravity = true;
        ASSERT_TRUE(system.UpdateVehicleRuntimeConfiguration(
            worldHandle,
            vehicleHandle,
            runtimeConfiguration));

        VehicleRuntimeConfiguration updatedRuntimeConfiguration;
        ASSERT_TRUE(system.GetVehicleRuntimeConfiguration(
            worldHandle,
            vehicleHandle,
            updatedRuntimeConfiguration));
        EXPECT_EQ(updatedRuntimeConfiguration.m_gravityOverride, runtimeConfiguration.m_gravityOverride);
        EXPECT_FLOAT_EQ(
            updatedRuntimeConfiguration.m_maximumPitchRollAngle,
            runtimeConfiguration.m_maximumPitchRollAngle);
        EXPECT_EQ(
            updatedRuntimeConfiguration.m_collisionTestIntervalActive,
            runtimeConfiguration.m_collisionTestIntervalActive);
        EXPECT_EQ(
            updatedRuntimeConfiguration.m_collisionTestIntervalInactive,
            runtimeConfiguration.m_collisionTestIntervalInactive);
        EXPECT_TRUE(updatedRuntimeConfiguration.m_overrideGravity);

        runtimeConfiguration.m_overrideGravity = false;
        ASSERT_TRUE(system.UpdateVehicleRuntimeConfiguration(
            worldHandle,
            vehicleHandle,
            runtimeConfiguration));
        BodyConfiguration restoredBodyConfiguration;
        ASSERT_TRUE(system.GetBodyConfiguration(
            worldHandle,
            chassisBodyHandle,
            restoredBodyConfiguration));
        EXPECT_FLOAT_EQ(restoredBodyConfiguration.m_gravityFactor, 0.75f);
        runtimeConfiguration.m_overrideGravity = true;
        ASSERT_TRUE(system.UpdateVehicleRuntimeConfiguration(
            worldHandle,
            vehicleHandle,
            runtimeConfiguration));

        VehicleCollisionConfiguration collisionConfiguration;
        ASSERT_TRUE(system.GetVehicleCollisionConfiguration(
            worldHandle,
            vehicleHandle,
            collisionConfiguration));
        EXPECT_EQ(collisionConfiguration.m_mode, VehicleCollisionTestMode::Ray);
        collisionConfiguration.m_mode = VehicleCollisionTestMode::Sphere;
        collisionConfiguration.m_sphereRadius = 0.2f;
        ASSERT_TRUE(system.UpdateVehicleCollisionConfiguration(
            worldHandle,
            vehicleHandle,
            collisionConfiguration));
        VehicleCollisionConfiguration updatedCollisionConfiguration;
        ASSERT_TRUE(system.GetVehicleCollisionConfiguration(
            worldHandle,
            vehicleHandle,
            updatedCollisionConfiguration));
        EXPECT_EQ(updatedCollisionConfiguration.m_mode, VehicleCollisionTestMode::Sphere);
        EXPECT_FLOAT_EQ(updatedCollisionConfiguration.m_sphereRadius, 0.2f);

        VehicleEngineConfiguration engineConfiguration;
        ASSERT_TRUE(system.GetVehicleEngineConfiguration(
            worldHandle,
            vehicleHandle,
            engineConfiguration));
        engineConfiguration.m_maximumTorque = 650.0f;
        engineConfiguration.m_normalizedTorque = {
            AZ::Vector2(0.0f, 0.5f),
            AZ::Vector2(1.0f, 1.0f),
        };
        ASSERT_TRUE(system.UpdateVehicleEngineConfiguration(
            worldHandle,
            vehicleHandle,
            engineConfiguration));
        VehicleEngineConfiguration updatedEngineConfiguration;
        ASSERT_TRUE(system.GetVehicleEngineConfiguration(
            worldHandle,
            vehicleHandle,
            updatedEngineConfiguration));
        EXPECT_FLOAT_EQ(updatedEngineConfiguration.m_maximumTorque, 650.0f);
        ASSERT_EQ(updatedEngineConfiguration.m_normalizedTorque.size(), 2);
        EXPECT_EQ(updatedEngineConfiguration.m_normalizedTorque[0], AZ::Vector2(0.0f, 0.5f));
        engineConfiguration.m_inertia = 0.0f;
        EXPECT_FALSE(system.UpdateVehicleEngineConfiguration(
            worldHandle,
            vehicleHandle,
            engineConfiguration));

        VehicleTransmissionConfiguration transmissionConfiguration;
        ASSERT_TRUE(system.GetVehicleTransmissionConfiguration(
            worldHandle,
            vehicleHandle,
            transmissionConfiguration));
        transmissionConfiguration.m_mode = TransmissionMode::Manual;
        transmissionConfiguration.m_clutchStrength = 12.0f;
        ASSERT_TRUE(system.UpdateVehicleTransmissionConfiguration(
            worldHandle,
            vehicleHandle,
            transmissionConfiguration));
        VehicleTransmissionConfiguration updatedTransmissionConfiguration;
        ASSERT_TRUE(system.GetVehicleTransmissionConfiguration(
            worldHandle,
            vehicleHandle,
            updatedTransmissionConfiguration));
        EXPECT_EQ(updatedTransmissionConfiguration.m_mode, TransmissionMode::Manual);
        EXPECT_FLOAT_EQ(updatedTransmissionConfiguration.m_clutchStrength, 12.0f);

        const VehiclePowertrainControl powertrainControl = {
            .m_currentGear = 1,
            .m_clutchFriction = 0.75f,
            .m_engineRpm = 2'500.0f,
        };
        ASSERT_TRUE(system.SetVehiclePowertrainControl(
            worldHandle,
            vehicleHandle,
            powertrainControl));
        VehiclePowertrainState powertrainState;
        ASSERT_TRUE(system.GetVehiclePowertrainState(
            worldHandle,
            vehicleHandle,
            powertrainState));
        EXPECT_EQ(powertrainState.m_currentGear, 1);
        EXPECT_FLOAT_EQ(powertrainState.m_clutchFriction, 0.75f);
        EXPECT_FLOAT_EQ(powertrainState.m_currentRatio, transmissionConfiguration.m_forwardGearRatios[0]);
        EXPECT_FLOAT_EQ(powertrainState.m_engineRpm, 2'500.0f);

        float engineTorque = 0.0f;
        ASSERT_TRUE(system.CalculateVehicleEngineTorque(
            worldHandle,
            vehicleHandle,
            1.0f,
            engineTorque));
        EXPECT_GT(engineTorque, 0.0f);
        EXPECT_FALSE(system.CalculateVehicleEngineTorque(
            worldHandle,
            vehicleHandle,
            1.1f,
            engineTorque));
        ASSERT_TRUE(system.ApplyVehicleEngineTorque(
            worldHandle,
            vehicleHandle,
            100.0f,
            1.0f / 60.0f));
        ASSERT_TRUE(system.ApplyVehicleEngineDamping(
            worldHandle,
            vehicleHandle,
            1.0f / 60.0f));
        EXPECT_FALSE(system.ApplyVehicleEngineDamping(
            worldHandle,
            vehicleHandle,
            -1.0f));

        float differentialLimitedSlipRatio = 0.0f;
        ASSERT_TRUE(system.GetVehicleDifferentialLimitedSlipRatio(
            worldHandle,
            vehicleHandle,
            differentialLimitedSlipRatio));
        EXPECT_FLOAT_EQ(differentialLimitedSlipRatio, 1.4f);
        ASSERT_TRUE(system.SetVehicleDifferentialLimitedSlipRatio(
            worldHandle,
            vehicleHandle,
            1.8f));
        ASSERT_TRUE(system.GetVehicleDifferentialLimitedSlipRatio(
            worldHandle,
            vehicleHandle,
            differentialLimitedSlipRatio));
        EXPECT_FLOAT_EQ(differentialLimitedSlipRatio, 1.8f);
        EXPECT_FALSE(system.SetVehicleDifferentialLimitedSlipRatio(
            worldHandle,
            vehicleHandle,
            1.0f));

        WheelBasis wheelBasis;
        ASSERT_TRUE(system.GetWheelLocalBasis(
            worldHandle,
            vehicleHandle,
            0,
            wheelBasis));
        EXPECT_TRUE(wheelBasis.m_forward.IsNormalized());
        EXPECT_TRUE(wheelBasis.m_right.IsNormalized());
        EXPECT_TRUE(wheelBasis.m_up.IsNormalized());
        EXPECT_NEAR(wheelBasis.m_forward.Dot(wheelBasis.m_up), 0.0f, 1.0e-5f);

        AZ::Transform wheelLocalTransform;
        ASSERT_TRUE(system.GetWheelLocalTransform(
            worldHandle,
            vehicleHandle,
            0,
            AZ::Vector3::CreateAxisX(),
            AZ::Vector3::CreateAxisZ(),
            wheelLocalTransform));
        EXPECT_TRUE(wheelLocalTransform.IsFinite());
        WorldTransform wheelWorldTransform;
        ASSERT_TRUE(system.GetWheelWorldTransform(
            worldHandle,
            vehicleHandle,
            0,
            AZ::Vector3::CreateAxisX(),
            AZ::Vector3::CreateAxisZ(),
            wheelWorldTransform));
        EXPECT_TRUE(std::isfinite(wheelWorldTransform.m_position.m_x));
        EXPECT_TRUE(std::isfinite(wheelWorldTransform.m_position.m_y));
        EXPECT_TRUE(std::isfinite(wheelWorldTransform.m_position.m_z));
        EXPECT_TRUE(wheelWorldTransform.m_rotation.IsFinite());
        EXPECT_FALSE(system.GetWheelLocalTransform(
            worldHandle,
            vehicleHandle,
            4,
            AZ::Vector3::CreateAxisX(),
            AZ::Vector3::CreateAxisZ(),
            wheelLocalTransform));
        EXPECT_FALSE(system.GetWheelLocalTransform(
            worldHandle,
            vehicleHandle,
            0,
            AZ::Vector3::CreateAxisX(),
            AZ::Vector3::CreateAxisX(),
            wheelLocalTransform));

        AZStd::array<VehicleAntiRollBarConfiguration, 1> truncatedAntiRollBars;
        QueryResult antiRollBarResult = system.QueryVehicleAntiRollBars(
            worldHandle,
            vehicleHandle,
            truncatedAntiRollBars);
        EXPECT_EQ(antiRollBarResult.m_hitCount, 1);
        EXPECT_EQ(antiRollBarResult.m_requiredHitCount, 2);
        const AZStd::array updatedAntiRollBars = {
            VehicleAntiRollBarConfiguration{
                .m_leftWheel = 0,
                .m_rightWheel = 1,
                .m_stiffness = 1'250.0f,
            },
        };
        ASSERT_TRUE(system.UpdateVehicleAntiRollBars(
            worldHandle,
            vehicleHandle,
            updatedAntiRollBars));
        antiRollBarResult = system.QueryVehicleAntiRollBars(
            worldHandle,
            vehicleHandle,
            truncatedAntiRollBars);
        EXPECT_TRUE(antiRollBarResult.IsComplete());
        EXPECT_FLOAT_EQ(truncatedAntiRollBars[0].m_stiffness, 1'250.0f);

        AZStd::array<VehicleDifferentialConfiguration, 1> differentials;
        QueryResult differentialResult = system.QueryVehicleDifferentials(
            worldHandle,
            vehicleHandle,
            differentials);
        ASSERT_TRUE(differentialResult.IsComplete());
        EXPECT_EQ(differentials[0].m_leftWheel, 0);
        differentials[0].m_engineTorqueRatio = 0.8f;
        EXPECT_FALSE(system.UpdateVehicleDifferentials(
            worldHandle,
            vehicleHandle,
            differentials));
        differentials[0].m_engineTorqueRatio = 1.0f;
        ASSERT_TRUE(system.UpdateVehicleDifferentials(
            worldHandle,
            vehicleHandle,
            differentials));
        differentialResult = system.QueryVehicleDifferentials(
            worldHandle,
            vehicleHandle,
            differentials);
        ASSERT_TRUE(differentialResult.IsComplete());
        EXPECT_FLOAT_EQ(differentials[0].m_engineTorqueRatio, 1.0f);
        differentials[0].m_leftWheel = 4;
        EXPECT_FALSE(system.UpdateVehicleDifferentials(
            worldHandle,
            vehicleHandle,
            differentials));

        WheelMotion wheelMotion = {
            .m_angularVelocity = 3.0f,
            .m_rotationAngle = AZ::Constants::QuarterPi,
            .m_steerAngle = 0.125f,
        };
        EXPECT_TRUE(system.SetWheelMotion(
            worldHandle,
            vehicleHandle,
            0,
            wheelMotion));
        EXPECT_FALSE(system.SetWheelMotion(
            worldHandle,
            vehicleHandle,
            4,
            wheelMotion));
        wheelMotion.m_rotationAngle = -1.0f;
        EXPECT_FALSE(system.SetWheelMotion(
            worldHandle,
            vehicleHandle,
            0,
            wheelMotion));

        VehicleRuntimeConfiguration invalidRuntimeConfiguration = runtimeConfiguration;
        invalidRuntimeConfiguration.m_maximumPitchRollAngle = -1.0f;
        EXPECT_FALSE(system.UpdateVehicleRuntimeConfiguration(
            worldHandle,
            vehicleHandle,
            invalidRuntimeConfiguration));

        AZStd::array<WheelState, 2> truncatedWheels;
        WheeledVehicleState truncatedState;
        const QueryResult truncatedResult = system.GetWheeledVehicleState(
            worldHandle,
            vehicleHandle,
            truncatedState,
            truncatedWheels);
        EXPECT_EQ(truncatedResult.m_hitCount, 2);
        EXPECT_EQ(truncatedResult.m_requiredHitCount, 4);
        EXPECT_FALSE(truncatedResult.IsComplete());

        AZStd::array<WheelState, 4> initialWheels;
        WheeledVehicleState initialState;
        const QueryResult initialResult = system.GetWheeledVehicleState(
            worldHandle,
            vehicleHandle,
            initialState,
            initialWheels);
        ASSERT_TRUE(initialResult.IsComplete());
        EXPECT_FLOAT_EQ(initialWheels[0].m_angularVelocity, 3.0f);
        EXPECT_FLOAT_EQ(initialWheels[0].m_rotationAngle, AZ::Constants::QuarterPi);
        EXPECT_FLOAT_EQ(initialWheels[0].m_steerAngle, 0.125f);

        EXPECT_TRUE(system.SetWheeledVehicleInput(
            worldHandle,
            vehicleHandle,
            {.m_forward = 1.0f}));
        for (AZ::u32 step = 0; step < 120; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        AZStd::array<WheelState, 4> wheels;
        WheeledVehicleState state;
        const QueryResult stateResult =
            system.GetWheeledVehicleState(worldHandle, vehicleHandle, state, wheels);
        EXPECT_TRUE(stateResult.IsComplete());
        EXPECT_EQ(state.m_bodyHandle, chassisBodyHandle);
        EXPECT_EQ(state.m_wheelCount, 4);
        EXPECT_TRUE(AZStd::any_of(
            wheels.begin(),
            wheels.end(),
            [](const WheelState& wheel)
            {
                return wheel.m_hasContact;
            }));

        EXPECT_EQ(vehicleCallbacks.m_lastVehicleHandle, vehicleHandle);
        EXPECT_EQ(vehicleCallbacks.m_lastContactBodyHandle, floorBodyHandle);
        EXPECT_GT(vehicleCallbacks.m_frictionCount, 0);
        EXPECT_GT(vehicleCallbacks.m_postCollideCount, 0);
        EXPECT_GT(vehicleCallbacks.m_postStepCount, 0);
        EXPECT_GT(vehicleCallbacks.m_preStepCount, 0);
        EXPECT_GT(vehicleCallbacks.m_tireImpulseCount, 0);
        EXPECT_TRUE(vehicleCallbacks.m_contextInputUpdateSucceeded);
        EXPECT_TRUE(vehicleCallbacks.m_contextStateQuerySucceeded);
        EXPECT_FLOAT_EQ(vehicleCallbacks.m_lastInformation.m_deltaTime, 1.0f / 60.0f);
        EXPECT_GT(vehicleCollisionFilter.m_bodyCount, 0);
        EXPECT_GT(vehicleCollisionFilter.m_broadPhaseCount, 0);
        EXPECT_GT(vehicleCollisionFilter.m_objectLayerCount, 0);
        EXPECT_TRUE(vehicleCollisionFilter.m_receivedMovingBroadPhaseLayer);
        EXPECT_TRUE(vehicleCollisionFilter.m_receivedMovingObjectLayer);
        EXPECT_FALSE(vehicleCollisionFilter.m_receivedInvalidBroadPhaseLayer);
        EXPECT_FALSE(vehicleCollisionFilter.m_receivedInvalidObjectLayer);

        const AZ::u32 preStepCount = vehicleCallbacks.m_preStepCount;
        EXPECT_TRUE(system.SetVehicleCallbacks(worldHandle, vehicleHandle, ExtensionHandle::Invalid));
        EXPECT_TRUE(system.SetVehicleCollisionFilter(worldHandle, vehicleHandle, ExtensionHandle::Invalid));
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_EQ(vehicleCallbacks.m_preStepCount, preStepCount);

        EXPECT_TRUE(system.DestroyVehicle(worldHandle, vehicleHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, vehicleHandle));
        ASSERT_TRUE(system.GetBodyConfiguration(
            worldHandle,
            chassisBodyHandle,
            restoredBodyConfiguration));
        EXPECT_FLOAT_EQ(restoredBodyConfiguration.m_gravityFactor, 0.75f);

        MotorcycleConfiguration motorcycleConfiguration;
        motorcycleConfiguration.m_wheeled = vehicleConfiguration;
        motorcycleConfiguration.m_wheeled.m_antiRollBars.clear();
        motorcycleConfiguration.m_wheeled.m_wheels = {
            {.m_position = AZ::Vector3(0.0f, 1.4f, -0.25f), .m_maximumHandBrakeTorque = 0.0f},
            {.m_position = AZ::Vector3(0.0f, -1.4f, -0.25f), .m_maximumSteerAngle = 0.0f},
        };
        motorcycleConfiguration.m_wheeled.m_differentials = {
            {.m_leftWheel = 0, .m_rightWheel = 1},
        };
        const VehicleHandle motorcycleHandle =
            system.CreateMotorcycle(worldHandle, motorcycleConfiguration);
        ASSERT_TRUE(motorcycleHandle);

        AZStd::array<WheelState, 2> motorcycleWheels;
        MotorcycleState motorcycleState;
        QueryResult motorcycleResult = system.GetMotorcycleState(
            worldHandle,
            motorcycleHandle,
            motorcycleState,
            motorcycleWheels);
        EXPECT_TRUE(motorcycleResult.IsComplete());
        EXPECT_EQ(motorcycleState.m_wheeled.m_bodyHandle, chassisBodyHandle);
        EXPECT_EQ(motorcycleState.m_wheeled.m_wheelCount, 2);
        EXPECT_NEAR(motorcycleState.m_wheelBase, 2.8f, 1.0e-4f);
        EXPECT_TRUE(motorcycleState.m_controller.m_enableLeanController);

        MotorcycleControllerUpdateConfiguration controllerConfiguration;
        controllerConfiguration.m_enableLeanController = false;
        controllerConfiguration.m_springConstant = 4'000.0f;
        ASSERT_TRUE(
            system.UpdateMotorcycleController(
                worldHandle,
                motorcycleHandle,
                controllerConfiguration));
        motorcycleResult = system.GetMotorcycleState(
            worldHandle,
            motorcycleHandle,
            motorcycleState,
            motorcycleWheels);
        EXPECT_TRUE(motorcycleResult.IsComplete());
        EXPECT_FALSE(motorcycleState.m_controller.m_enableLeanController);
        EXPECT_FLOAT_EQ(motorcycleState.m_controller.m_springConstant, 4'000.0f);
        EXPECT_TRUE(system.DestroyVehicle(worldHandle, motorcycleHandle));

        TrackedVehicleConfiguration trackedConfiguration;
        trackedConfiguration.m_bodyHandle = chassisBodyHandle;
        trackedConfiguration.m_wheels = {
            {.m_common = {.m_position = AZ::Vector3(-0.8f, 1.2f, -0.25f)}},
            {.m_common = {.m_position = AZ::Vector3(-0.8f, -1.2f, -0.25f)}},
            {.m_common = {.m_position = AZ::Vector3(0.8f, 1.2f, -0.25f)}},
            {.m_common = {.m_position = AZ::Vector3(0.8f, -1.2f, -0.25f)}},
        };
        trackedConfiguration.m_tracks[0].m_wheels = {0, 1};
        trackedConfiguration.m_tracks[0].m_drivenWheel = 0;
        trackedConfiguration.m_tracks[1].m_wheels = {2, 3};
        trackedConfiguration.m_tracks[1].m_drivenWheel = 2;

        const VehicleHandle trackedHandle =
            system.CreateTrackedVehicle(worldHandle, trackedConfiguration);
        ASSERT_TRUE(trackedHandle);

        RecordingVehicleCallbacks trackedCallbacks;
        ScopedExtensionRegistration trackedCallbacksRegistration(system, &trackedCallbacks);
        ASSERT_TRUE(system.SetVehicleCallbacks(
            worldHandle,
            trackedHandle,
            trackedCallbacksRegistration.GetHandle()));

        AZStd::array<WheelState, 4> trackedWheels;
        TrackedVehicleState trackedState;
        const QueryResult trackedResult = system.GetTrackedVehicleState(
            worldHandle,
            trackedHandle,
            trackedState,
            trackedWheels);
        EXPECT_TRUE(trackedResult.IsComplete());
        EXPECT_EQ(trackedState.m_bodyHandle, chassisBodyHandle);
        EXPECT_EQ(trackedState.m_kind, VehicleKind::Tracked);
        EXPECT_EQ(trackedState.m_wheelCount, 4);

        VehicleTrackConfiguration trackConfiguration;
        ASSERT_TRUE(system.GetVehicleTrackConfiguration(
            worldHandle,
            trackedHandle,
            0,
            trackConfiguration));
        ASSERT_EQ(trackConfiguration.m_wheels.size(), 2);
        EXPECT_EQ(trackConfiguration.m_drivenWheel, 0);
        trackConfiguration.m_angularDamping = 0.75f;
        ASSERT_TRUE(system.UpdateVehicleTrackConfiguration(
            worldHandle,
            trackedHandle,
            0,
            trackConfiguration));
        VehicleTrackConfiguration updatedTrackConfiguration;
        ASSERT_TRUE(system.GetVehicleTrackConfiguration(
            worldHandle,
            trackedHandle,
            0,
            updatedTrackConfiguration));
        EXPECT_FLOAT_EQ(updatedTrackConfiguration.m_angularDamping, 0.75f);
        EXPECT_FALSE(system.GetVehicleTrackConfiguration(
            worldHandle,
            trackedHandle,
            2,
            updatedTrackConfiguration));
        EXPECT_FALSE(system.SetVehicleDifferentialLimitedSlipRatio(
            worldHandle,
            trackedHandle,
            1.8f));
        ASSERT_TRUE(system.SetVehicleTrackAngularVelocity(
            worldHandle,
            trackedHandle,
            0,
            4.0f));
        TrackedVehicleState updatedTrackedState;
        ASSERT_TRUE(system.GetTrackedVehicleState(
            worldHandle,
            trackedHandle,
            updatedTrackedState,
            trackedWheels).IsComplete());
        EXPECT_FLOAT_EQ(updatedTrackedState.m_tracks[0].m_angularVelocity, 4.0f);
        EXPECT_FALSE(system.SetVehicleTrackAngularVelocity(
            worldHandle,
            trackedHandle,
            2,
            4.0f));

        EXPECT_TRUE(system.SetTrackedVehicleInput(
            worldHandle,
            trackedHandle,
            {.m_forward = 1.0f, .m_leftRatio = 0.75f, .m_rightRatio = 1.0f}));
        EXPECT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_GT(trackedCallbacks.m_preStepCount, 0);
        EXPECT_EQ(trackedCallbacks.m_tireImpulseCount, 0);
        EXPECT_TRUE(system.SetVehicleCallbacks(worldHandle, trackedHandle, ExtensionHandle::Invalid));
        EXPECT_TRUE(system.DestroyVehicle(worldHandle, trackedHandle));

        EXPECT_TRUE(system.DestroyBody(worldHandle, chassisBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, chassisShapeHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, floorBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, floorShapeHandle));
    }

    TEST(SimulationTests, WorldSnapshotsRestoreExactStateAndRejectTopologyChanges)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SphereOnFloor scene = CreateSphereOnFloor(system);
        for (AZ::u32 step = 0; step < 15; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }

        WorldStateDigest capturedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, capturedDigest));
        ASSERT_GT(capturedDigest.m_stateByteCount, 0);
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(scene.m_worldHandle);
        ASSERT_TRUE(snapshotHandle);
        const AZ::u64 capturedEventSequence = system.GetEvents(scene.m_worldHandle).GetSequence();
        EXPECT_TRUE(system.IsValid(scene.m_worldHandle, snapshotHandle));
        StateValidationResult validationResult;
        ASSERT_TRUE(system.ValidateWorldState(
            scene.m_worldHandle,
            snapshotHandle,
            validationResult));
        EXPECT_TRUE(validationResult.m_matches);

        for (AZ::u32 step = 0; step < 30; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }
        WorldStateDigest advancedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, advancedDigest));
        EXPECT_NE(advancedDigest, capturedDigest);
        ASSERT_TRUE(system.ValidateWorldState(
            scene.m_worldHandle,
            snapshotHandle,
            validationResult));
        EXPECT_FALSE(validationResult.m_matches);
        WorldStateDigest digestAfterValidation;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, digestAfterValidation));
        EXPECT_EQ(digestAfterValidation, advancedDigest);

        ASSERT_TRUE(system.RestoreWorldState(scene.m_worldHandle, snapshotHandle));
        EXPECT_EQ(system.GetEvents(scene.m_worldHandle).GetSequence(), capturedEventSequence);
        WorldStateDigest restoredDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, restoredDigest));
        EXPECT_EQ(restoredDigest, capturedDigest);

        for (AZ::u32 step = 0; step < 5; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }
        WorldStateDigest recapturedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, recapturedDigest));
        ASSERT_NE(recapturedDigest, capturedDigest);
        ASSERT_TRUE(system.CaptureWorldState(scene.m_worldHandle, snapshotHandle));
        ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.RestoreWorldState(scene.m_worldHandle, snapshotHandle));
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, restoredDigest));
        EXPECT_EQ(restoredDigest, recapturedDigest);

        ShapeConfiguration extraShapeConfiguration;
        extraShapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle extraShapeHandle = system.CreateShape(scene.m_worldHandle, extraShapeConfiguration);
        ASSERT_TRUE(extraShapeHandle);
        BodyConfiguration extraBodyConfiguration;
        extraBodyConfiguration.m_shapeHandle = extraShapeHandle;
        const BodyHandle extraBodyHandle = system.CreateBody(scene.m_worldHandle, extraBodyConfiguration);
        ASSERT_TRUE(extraBodyHandle);
        EXPECT_FALSE(system.RestoreWorldState(scene.m_worldHandle, snapshotHandle));

        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyBody(scene.m_worldHandle, extraBodyHandle));
        EXPECT_TRUE(system.DestroyShape(scene.m_worldHandle, extraShapeHandle));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, WorldSnapshotArchivesRoundTripAndRejectCorruptionAtomically)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SphereOnFloor scene = CreateSphereOnFloor(system);
        for (AZ::u32 step = 0; step < 20; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }

        WorldStateDigest capturedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, capturedDigest));
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(scene.m_worldHandle);
        ASSERT_TRUE(snapshotHandle);
        StateSnapshotArchive archive;
        ASSERT_TRUE(system.ExportWorldStateArchive(
            scene.m_worldHandle,
            AZStd::array{snapshotHandle},
            archive));
        EXPECT_FALSE(archive.m_binaryState.empty());
        EXPECT_NE(archive.m_buildFingerprint, 0);
        EXPECT_NE(archive.m_contentHash, 0);
        EXPECT_EQ(archive.m_formatVersion, 5);
        EXPECT_EQ(archive.m_snapshotCount, 1);

        const WorldHandle secondWorldHandle = system.CreateWorld(WorldConfiguration{});
        ASSERT_TRUE(secondWorldHandle);
        SphereOnFloor secondScene = CreateSphereOnFloor(system, secondWorldHandle);
        AZStd::array<StateSnapshotHandle, 1> crossWorldHandles;
        ASSERT_TRUE(system.ImportWorldStateArchive(
            secondWorldHandle,
            archive,
            crossWorldHandles));
        ASSERT_TRUE(system.RestoreWorldState(secondWorldHandle, crossWorldHandles.front()));
        WorldStateDigest crossWorldDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(secondWorldHandle, crossWorldDigest));
        EXPECT_EQ(crossWorldDigest, capturedDigest);
        EXPECT_TRUE(system.DestroyStateSnapshot(secondWorldHandle, crossWorldHandles.front()));
        DestroySphereOnFloor(system, secondScene);
        EXPECT_TRUE(system.DestroyWorld(secondWorldHandle));

        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, snapshotHandle));

        for (AZ::u32 step = 0; step < 30; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        }
        WorldStateDigest advancedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, advancedDigest));
        ASSERT_NE(advancedDigest, capturedDigest);

        StateSnapshotArchive corruptedArchive = archive;
        corruptedArchive.m_binaryState.back() ^= 0x80;
        AZStd::array<StateSnapshotHandle, 1> importedHandles;
        EXPECT_FALSE(system.ImportWorldStateArchive(
            scene.m_worldHandle,
            corruptedArchive,
            importedHandles));
        EXPECT_FALSE(importedHandles.front());

        StateSnapshotArchive trailingDataArchive = archive;
        trailingDataArchive.m_binaryState.push_back(0);
        trailingDataArchive.m_contentHash = static_cast<AZ::u64>(AZ::TypeHash64(
            trailingDataArchive.m_binaryState));
        EXPECT_FALSE(system.ImportWorldStateArchive(
            scene.m_worldHandle,
            trailingDataArchive,
            importedHandles));
        EXPECT_FALSE(importedHandles.front());

        StateSnapshotArchive mismatchedBuildArchive = archive;
        ++mismatchedBuildArchive.m_buildFingerprint;
        EXPECT_FALSE(system.ImportWorldStateArchive(
            scene.m_worldHandle,
            mismatchedBuildArchive,
            importedHandles));
        EXPECT_FALSE(importedHandles.front());

        WorldStateDigest digestAfterRejectedImport;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, digestAfterRejectedImport));
        EXPECT_EQ(digestAfterRejectedImport, advancedDigest);

        ASSERT_TRUE(system.ImportWorldStateArchive(
            scene.m_worldHandle,
            archive,
            importedHandles));
        ASSERT_TRUE(importedHandles.front());
        ASSERT_TRUE(system.RestoreWorldState(scene.m_worldHandle, importedHandles.front()));

        WorldStateDigest restoredDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, restoredDigest));
        EXPECT_EQ(restoredDigest, capturedDigest);
        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, importedHandles.front()));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, ImportedValidatedSnapshotsRecoverFromCorruptNativeState)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SphereOnFloor scene = CreateSphereOnFloor(system);
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(
            scene.m_worldHandle,
            {
                .m_flags = StateSnapshotFlags::Bodies,
                .m_restoreSafety = RestoreSafety::Validated,
            },
            {});
        ASSERT_TRUE(snapshotHandle);
        StateSnapshotArchive archive;
        ASSERT_TRUE(system.ExportWorldStateArchive(
            scene.m_worldHandle,
            AZStd::array{snapshotHandle},
            archive));
        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, snapshotHandle));

        ASSERT_TRUE(system.SetBodyPosition(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle,
            {.m_x = 4.0, .m_z = 3.0},
            true));
        BodyState stateBeforeRestore;
        ASSERT_TRUE(system.GetBodyState(scene.m_worldHandle, scene.m_sphereBodyHandle, stateBeforeRestore));

        constexpr size_t archiveHeaderByteCount = sizeof(AZ::u32) * 3
            + sizeof(AZ::u64)
            + sizeof(AZ::u8);
        constexpr size_t nativeStateOffset = archiveHeaderByteCount + sizeof(AZ::u32);
        ASSERT_GT(archive.m_binaryState.size(), nativeStateOffset);
        archive.m_binaryState[nativeStateOffset] = 0;
        archive.m_contentHash = static_cast<AZ::u64>(AZ::TypeHash64(archive.m_binaryState));

        AZStd::array<StateSnapshotHandle, 1> importedHandles;
        ASSERT_TRUE(system.ImportWorldStateArchive(scene.m_worldHandle, archive, importedHandles));
        const StateRestoreResult result = system.RestoreWorldState(
            scene.m_worldHandle,
            importedHandles.front());
        EXPECT_EQ(result.m_status, StateRestoreStatus::Rejected);
        EXPECT_TRUE(system.IsValid(scene.m_worldHandle));

        BodyState stateAfterRestore;
        ASSERT_TRUE(system.GetBodyState(scene.m_worldHandle, scene.m_sphereBodyHandle, stateAfterRestore));
        EXPECT_EQ(stateAfterRestore.m_transform.m_position, stateBeforeRestore.m_transform.m_position);
        EXPECT_EQ(stateAfterRestore.m_linearVelocity, stateBeforeRestore.m_linearVelocity);

        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, importedHandles.front()));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, MultipartWorldSnapshotArchivesPreserveBatchIdentity)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_transform.m_position = {.m_x = -4.0};
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);
        bodyConfiguration.m_transform.m_position = {.m_x = 4.0};
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        const AZStd::array bodyHandles = {firstBodyHandle, secondBodyHandle};
        const AZStd::array<AZ::u32, 2> partitionBodyCounts = {1, 1};
        AZStd::array<StateSnapshotHandle, 2> snapshotHandles;
        ASSERT_TRUE(system.CaptureWorldStateParts(
            worldHandle,
            {
                .m_flags = StateSnapshotFlags::Bodies,
                .m_filterBodies = true,
            },
            bodyHandles,
            partitionBodyCounts,
            snapshotHandles));

        StateSnapshotArchive archive;
        ASSERT_TRUE(system.ExportWorldStateArchive(worldHandle, snapshotHandles, archive));
        EXPECT_EQ(archive.m_snapshotCount, 2);
        StateSnapshotArchive incompleteArchive;
        EXPECT_FALSE(system.ExportWorldStateArchive(
            worldHandle,
            AZStd::span<const StateSnapshotHandle>(snapshotHandles).first(1),
            incompleteArchive));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandles[1]));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandles[0]));

        WorldTransform movedTransform;
        movedTransform.m_position = {.m_x = 20.0};
        ASSERT_TRUE(system.SetBodyTransform(worldHandle, firstBodyHandle, movedTransform, true));
        movedTransform.m_position = {.m_x = -20.0};
        ASSERT_TRUE(system.SetBodyTransform(worldHandle, secondBodyHandle, movedTransform, true));

        AZStd::array<StateSnapshotHandle, 2> importedHandles;
        ASSERT_TRUE(system.ImportWorldStateArchive(worldHandle, archive, importedHandles));
        EXPECT_FALSE(system.RestoreWorldStateParts(
            worldHandle,
            AZStd::array{importedHandles[0], importedHandles[0]}));
        ASSERT_TRUE(system.RestoreWorldStateParts(worldHandle, importedHandles));

        BodyState firstState;
        BodyState secondState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, firstBodyHandle, firstState));
        ASSERT_TRUE(system.GetBodyState(worldHandle, secondBodyHandle, secondState));
        EXPECT_DOUBLE_EQ(firstState.m_transform.m_position.m_x, -4.0);
        EXPECT_DOUBLE_EQ(secondState.m_transform.m_position.m_x, 4.0);

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, importedHandles[1]));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, importedHandles[0]));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, WorldSnapshotsRejectVirtualCharacterCallbackChanges)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        VirtualCharacterConfiguration characterConfiguration;
        characterConfiguration.m_shapeHandle = shapeHandle;
        const VirtualCharacterHandle characterHandle =
            system.CreateVirtualCharacter(worldHandle, characterConfiguration);
        ASSERT_TRUE(characterHandle);

        RecordingVirtualCharacterContactCallbacks callbacks;
        ScopedExtensionRegistration registration(system, &callbacks);
        ASSERT_TRUE(system.SetVirtualCharacterContactCallbacks(
            worldHandle,
            characterHandle,
            registration.GetHandle()));
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);

        callbacks.m_stateHash = 1;
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, snapshotHandle));
        callbacks.m_stateHash = 0;
        EXPECT_TRUE(system.RestoreWorldState(worldHandle, snapshotHandle));

        EXPECT_TRUE(system.SetVirtualCharacterContactCallbacks(
            worldHandle,
            characterHandle,
            registration.GetHandle()));
        EXPECT_TRUE(system.RestoreWorldState(worldHandle, snapshotHandle));

        EXPECT_TRUE(system.SetVirtualCharacterContactCallbacks(
            worldHandle,
            characterHandle,
            ExtensionHandle::Invalid));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, snapshotHandle));

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyVirtualCharacter(worldHandle, characterHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, WorldSnapshotsRestoreMutableGlobalCallbackState)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SphereOnFloor scene = CreateSphereOnFloor(system);
        RecordingSimulationShapeFilter filter;
        ScopedExtensionRegistration registration(system, &filter);
        ASSERT_TRUE(system.SetSimulationShapeFilter(scene.m_worldHandle, registration.GetHandle()));

        WorldStateDigest capturedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, capturedDigest));
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(scene.m_worldHandle);
        ASSERT_TRUE(snapshotHandle);
        StateSnapshotArchive archive;
        ASSERT_TRUE(system.ExportWorldStateArchive(
            scene.m_worldHandle,
            AZStd::array{snapshotHandle},
            archive));

        filter.m_rejectedBodyHandle = scene.m_sphereBodyHandle;
        WorldStateDigest changedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, changedDigest));
        EXPECT_NE(changedDigest, capturedDigest);
        StateValidationResult validationResult;
        ASSERT_TRUE(system.ValidateWorldState(
            scene.m_worldHandle,
            snapshotHandle,
            validationResult));
        EXPECT_FALSE(validationResult.m_matches);

        EXPECT_TRUE(system.RestoreWorldState(scene.m_worldHandle, snapshotHandle));
        EXPECT_FALSE(filter.m_rejectedBodyHandle);
        WorldStateDigest restoredDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(scene.m_worldHandle, restoredDigest));
        EXPECT_EQ(restoredDigest, capturedDigest);

        filter.m_rejectedBodyHandle = scene.m_sphereBodyHandle;
        AZStd::array<StateSnapshotHandle, 1> importedHandles;
        ASSERT_TRUE(system.ImportWorldStateArchive(
            scene.m_worldHandle,
            archive,
            importedHandles));
        ASSERT_TRUE(system.RestoreWorldState(scene.m_worldHandle, importedHandles.front()));
        EXPECT_FALSE(filter.m_rejectedBodyHandle);
        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, importedHandles.front()));

        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, snapshotHandle));
        EXPECT_TRUE(system.SetSimulationShapeFilter(scene.m_worldHandle, ExtensionHandle::Invalid));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, TransactionalRestoreRecoversCallbackStateAfterParticipantFailure)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        StatefulStepListener firstListener;
        StatefulStepListener secondListener;
        firstListener.m_state = 10;
        secondListener.m_state = 20;
        ScopedExtensionRegistration firstRegistration(system, &firstListener);
        ScopedExtensionRegistration secondRegistration(system, &secondListener);
        ASSERT_TRUE(system.AddStepListener(worldHandle, firstRegistration.GetHandle()));
        ASSERT_TRUE(system.AddStepListener(worldHandle, secondRegistration.GetHandle()));

        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);
        firstListener.m_state = 30;
        secondListener.m_state = 40;
        secondListener.m_remainingRestoreFailureCount = 1;

        const StateRestoreResult rejectedRestore = system.RestoreWorldState(worldHandle, snapshotHandle);
        EXPECT_EQ(rejectedRestore.m_status, StateRestoreStatus::Rejected);
        EXPECT_EQ(firstListener.m_state, 30);
        EXPECT_EQ(secondListener.m_state, 40);
        EXPECT_EQ(firstListener.m_commitCount, 0);
        EXPECT_EQ(secondListener.m_commitCount, 0);

        ASSERT_TRUE(system.RestoreWorldState(worldHandle, snapshotHandle));
        EXPECT_EQ(firstListener.m_state, 10);
        EXPECT_EQ(secondListener.m_state, 20);
        EXPECT_EQ(firstListener.m_commitCount, 1);
        EXPECT_EQ(secondListener.m_commitCount, 1);

        const StateSnapshotHandle validatedSnapshotHandle = system.CaptureWorldState(
            worldHandle,
            {
                .m_restoreSafety = RestoreSafety::Validated,
            },
            {});
        ASSERT_TRUE(validatedSnapshotHandle);
        firstListener.m_state = 50;
        secondListener.m_state = 60;
        secondListener.m_remainingRestoreFailureCount = 1;
        const AZ::u64 eventSequenceBeforeRestore = system.GetEvents(worldHandle).GetSequence();

        EXPECT_FALSE(system.RestoreWorldState(worldHandle, validatedSnapshotHandle));
        EXPECT_EQ(firstListener.m_state, 50);
        EXPECT_EQ(secondListener.m_state, 60);
        EXPECT_EQ(firstListener.m_commitCount, 1);
        EXPECT_EQ(secondListener.m_commitCount, 1);
        EXPECT_EQ(system.GetEvents(worldHandle).GetSequence(), eventSequenceBeforeRestore);

        ASSERT_TRUE(system.RestoreWorldState(worldHandle, validatedSnapshotHandle));
        EXPECT_EQ(firstListener.m_state, 10);
        EXPECT_EQ(secondListener.m_state, 20);

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, validatedSnapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.RemoveStepListener(worldHandle, secondRegistration.GetHandle()));
        EXPECT_TRUE(system.RemoveStepListener(worldHandle, firstRegistration.GetHandle()));
    }

    TEST(SimulationTests, UnrecoverableRestoreQuarantinesTheWorld)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.CreateWorld(WorldConfiguration{});
        ASSERT_TRUE(worldHandle);
        StatefulStepListener listener;
        listener.m_state = 10;
        ScopedExtensionRegistration registration(system, &listener);
        ASSERT_TRUE(system.AddStepListener(worldHandle, registration.GetHandle()));

        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);
        listener.m_state = 20;
        listener.m_corruptCommitState = true;

        AZ_TEST_START_TRACE_SUPPRESSION;
        const StateRestoreResult result = system.RestoreWorldState(worldHandle, snapshotHandle);
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(result.m_status, StateRestoreStatus::StateIndeterminate);
        EXPECT_FALSE(system.IsValid(worldHandle));
        EXPECT_FALSE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        const SimulationResult automaticStepResult = system.StepAutoSimulatedWorldsDetailed(1.0f / 60.0f);
        EXPECT_EQ(automaticStepResult.m_errors & SimulationError::InvalidRequest, SimulationError::InvalidRequest);
        EXPECT_TRUE(system.DestroyWorld(worldHandle));
    }

    TEST(SimulationTests, SnapshotArchivesRejectRollbackParticipantSchemaMismatches)
    {
        StateSnapshotArchive archive;
        {
            Runtime sourceSystem(CreateSerialSystemConfiguration(), nullptr);
            ASSERT_TRUE(sourceSystem);

            const WorldHandle sourceWorldHandle = sourceSystem.GetDefaultWorldHandle();
            StatefulStepListener sourceListener;
            sourceListener.m_state = 77;
            ScopedExtensionRegistration sourceRegistration(sourceSystem, &sourceListener);
            ASSERT_TRUE(sourceSystem.AddStepListener(sourceWorldHandle, sourceRegistration.GetHandle()));

            const StateSnapshotHandle sourceSnapshotHandle =
                sourceSystem.CaptureWorldState(sourceWorldHandle);
            ASSERT_TRUE(sourceSnapshotHandle);
            ASSERT_TRUE(sourceSystem.ExportWorldStateArchive(
                sourceWorldHandle,
                AZStd::array{sourceSnapshotHandle},
                archive));
            EXPECT_TRUE(sourceSystem.DestroyStateSnapshot(
                sourceWorldHandle,
                sourceSnapshotHandle));
            EXPECT_TRUE(sourceSystem.RemoveStepListener(sourceWorldHandle, sourceRegistration.GetHandle()));
        }

        {
            Runtime targetSystem(CreateSerialSystemConfiguration(), nullptr);
            ASSERT_TRUE(targetSystem);

            const WorldHandle targetWorldHandle = targetSystem.GetDefaultWorldHandle();
            StatefulStepListener targetListener;
            targetListener.m_state = 22;
            targetListener.m_typeId = InvalidBodyPairColliderStateTypeId;
            ScopedExtensionRegistration targetRegistration(targetSystem, &targetListener);
            ASSERT_TRUE(targetSystem.AddStepListener(targetWorldHandle, targetRegistration.GetHandle()));

            AZStd::array<StateSnapshotHandle, 1> importedHandles;
            EXPECT_FALSE(targetSystem.ImportWorldStateArchive(
                targetWorldHandle,
                archive,
                importedHandles));
            EXPECT_FALSE(importedHandles.front());
            EXPECT_EQ(targetListener.m_state, 22);

            targetListener.m_typeId = StatefulStepListenerStateTypeId;
            targetListener.m_version = 1;
            EXPECT_FALSE(targetSystem.ImportWorldStateArchive(
                targetWorldHandle,
                archive,
                importedHandles));
            EXPECT_FALSE(importedHandles.front());
            EXPECT_EQ(targetListener.m_state, 22);

            targetListener.m_version = 0;
            ASSERT_TRUE(targetSystem.ImportWorldStateArchive(
                targetWorldHandle,
                archive,
                importedHandles));
            ASSERT_TRUE(importedHandles.front());
            ASSERT_TRUE(targetSystem.RestoreWorldState(
                targetWorldHandle,
                importedHandles.front()));
            EXPECT_EQ(targetListener.m_state, 77);

            EXPECT_TRUE(targetSystem.DestroyStateSnapshot(
                targetWorldHandle,
                importedHandles.front()));
            EXPECT_TRUE(targetSystem.RemoveStepListener(targetWorldHandle, targetRegistration.GetHandle()));
        }
    }

    TEST(SimulationTests, FilteredSnapshotsRestoreIndependentWorldPartitions)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration firstConfiguration;
        firstConfiguration.m_shapeHandle = shapeHandle;
        firstConfiguration.m_transform.m_position = {.m_x = -10.0};
        firstConfiguration.m_linearVelocity = AZ::Vector3::CreateAxisX(2.0f);
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, firstConfiguration);
        ASSERT_TRUE(firstBodyHandle);

        BodyConfiguration secondConfiguration;
        secondConfiguration.m_shapeHandle = shapeHandle;
        secondConfiguration.m_transform.m_position = {.m_x = 10.0};
        secondConfiguration.m_linearVelocity = AZ::Vector3::CreateAxisY(3.0f);
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, secondConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        const AZStd::array firstPartitionBodies = {firstBodyHandle};
        const AZStd::array secondPartitionBodies = {secondBodyHandle};
        const StateSnapshotHandle firstSnapshotHandle = system.CaptureWorldState(
            worldHandle,
            {
                .m_flags = StateSnapshotFlags::Bodies,
                .m_filterBodies = true,
            },
            firstPartitionBodies);
        ASSERT_TRUE(firstSnapshotHandle);
        const StateSnapshotHandle secondSnapshotHandle = system.CaptureWorldState(
            worldHandle,
            {
                .m_flags = StateSnapshotFlags::Bodies,
                .m_restoreSafety = RestoreSafety::Validated,
                .m_filterBodies = true,
            },
            secondPartitionBodies);
        ASSERT_TRUE(secondSnapshotHandle);

        BodyState capturedFirstState;
        BodyState capturedSecondState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, firstBodyHandle, capturedFirstState));
        ASSERT_TRUE(system.GetBodyState(worldHandle, secondBodyHandle, capturedSecondState));

        WorldTransform movedFirstTransform;
        movedFirstTransform.m_position = {.m_x = -20.0, .m_y = 4.0};
        WorldTransform movedSecondTransform;
        movedSecondTransform.m_position = {.m_x = 20.0, .m_y = -4.0};
        ASSERT_TRUE(system.SetBodyTransformAndVelocities(
            worldHandle,
            firstBodyHandle,
            movedFirstTransform,
            AZ::Vector3::CreateAxisZ(5.0f),
            AZ::Vector3::CreateAxisX()));
        ASSERT_TRUE(system.SetBodyTransformAndVelocities(
            worldHandle,
            secondBodyHandle,
            movedSecondTransform,
            AZ::Vector3::CreateAxisZ(-5.0f),
            AZ::Vector3::CreateAxisY()));

        StateValidationResult validationResult;
        EXPECT_FALSE(system.ValidateWorldState(
            worldHandle,
            firstSnapshotHandle,
            validationResult));
        EXPECT_FALSE(system.CaptureWorldState(
            worldHandle,
            {.m_filterBodies = true},
            AZStd::array{BodyHandle::Invalid}));
        EXPECT_FALSE(system.CaptureWorldState(
            worldHandle,
            firstSnapshotHandle,
            {.m_filterBodies = true},
            AZStd::array{BodyHandle::Invalid}));
        EXPECT_TRUE(system.IsValid(worldHandle, firstSnapshotHandle));
        EXPECT_FALSE(system.CaptureWorldState(
            worldHandle,
            {.m_flags = StateSnapshotFlags::None},
            {}));
        EXPECT_FALSE(system.CaptureWorldState(
            worldHandle,
            {.m_restoreSafety = RestoreSafety::None},
            {}));

        BodyConfiguration unrelatedConfiguration;
        unrelatedConfiguration.m_shapeHandle = shapeHandle;
        unrelatedConfiguration.m_transform.m_position = {.m_x = 30.0};
        const BodyHandle unrelatedBodyHandle = system.CreateBody(worldHandle, unrelatedConfiguration);
        ASSERT_TRUE(unrelatedBodyHandle);
        ASSERT_TRUE(system.SetBodyLinearDamping(worldHandle, unrelatedBodyHandle, 0.25f));

        ASSERT_TRUE(system.RestoreWorldState(worldHandle, firstSnapshotHandle));
        BodyState restoredFirstState;
        BodyState stillMovedSecondState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, firstBodyHandle, restoredFirstState));
        ASSERT_TRUE(system.GetBodyState(worldHandle, secondBodyHandle, stillMovedSecondState));
        EXPECT_DOUBLE_EQ(
            restoredFirstState.m_transform.m_position.m_x,
            capturedFirstState.m_transform.m_position.m_x);
        EXPECT_DOUBLE_EQ(
            restoredFirstState.m_transform.m_position.m_y,
            capturedFirstState.m_transform.m_position.m_y);
        EXPECT_DOUBLE_EQ(
            restoredFirstState.m_transform.m_position.m_z,
            capturedFirstState.m_transform.m_position.m_z);
        EXPECT_EQ(restoredFirstState.m_linearVelocity, capturedFirstState.m_linearVelocity);
        EXPECT_DOUBLE_EQ(stillMovedSecondState.m_transform.m_position.m_x, movedSecondTransform.m_position.m_x);
        EXPECT_DOUBLE_EQ(stillMovedSecondState.m_transform.m_position.m_y, movedSecondTransform.m_position.m_y);
        EXPECT_DOUBLE_EQ(stillMovedSecondState.m_transform.m_position.m_z, movedSecondTransform.m_position.m_z);

        ASSERT_TRUE(system.RestoreWorldState(worldHandle, secondSnapshotHandle));
        BodyState restoredSecondState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, secondBodyHandle, restoredSecondState));
        EXPECT_DOUBLE_EQ(
            restoredSecondState.m_transform.m_position.m_x,
            capturedSecondState.m_transform.m_position.m_x);
        EXPECT_DOUBLE_EQ(
            restoredSecondState.m_transform.m_position.m_y,
            capturedSecondState.m_transform.m_position.m_y);
        EXPECT_DOUBLE_EQ(
            restoredSecondState.m_transform.m_position.m_z,
            capturedSecondState.m_transform.m_position.m_z);
        EXPECT_EQ(restoredSecondState.m_linearVelocity, capturedSecondState.m_linearVelocity);

        const StateSnapshotHandle selectedConfigurationSnapshotHandle = system.CaptureWorldState(
            worldHandle,
            {
                .m_flags = StateSnapshotFlags::Bodies,
                .m_filterBodies = true,
            },
            firstPartitionBodies);
        ASSERT_TRUE(selectedConfigurationSnapshotHandle);
        ASSERT_TRUE(system.SetBodyLinearDamping(worldHandle, firstBodyHandle, 0.25f));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, selectedConfigurationSnapshotHandle));

        const StateSnapshotHandle unrelatedWorldConfigurationSnapshotHandle = system.CaptureWorldState(
            worldHandle,
            {
                .m_flags = StateSnapshotFlags::Bodies,
                .m_filterBodies = true,
            },
            firstPartitionBodies);
        ASSERT_TRUE(unrelatedWorldConfigurationSnapshotHandle);
        ASSERT_TRUE(system.SetWorldGravity(worldHandle, AZ::Vector3::CreateAxisZ(-1.0f)));
        EXPECT_TRUE(system.RestoreWorldState(worldHandle, unrelatedWorldConfigurationSnapshotHandle));

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, unrelatedWorldConfigurationSnapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, selectedConfigurationSnapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, secondSnapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, firstSnapshotHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, unrelatedBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, FilteredSnapshotsTrackOnlyOwnedConstraintConfiguration)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_transform.m_position = {.m_x = -10.0};
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);

        bodyConfiguration.m_transform.m_position = {.m_x = -8.0};
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        bodyConfiguration.m_transform.m_position = {.m_x = 8.0};
        const BodyHandle unrelatedFirstBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(unrelatedFirstBodyHandle);

        bodyConfiguration.m_transform.m_position = {.m_x = 10.0};
        const BodyHandle unrelatedSecondBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(unrelatedSecondBodyHandle);

        FixedConstraintConfiguration fixedConfiguration;
        fixedConfiguration.m_space = ConstraintSpace::World;
        fixedConfiguration.m_autoDetectPoint = true;

        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = firstBodyHandle;
        constraintConfiguration.m_secondBodyHandle = secondBodyHandle;
        constraintConfiguration.m_geometry = fixedConfiguration;
        const ConstraintHandle constraintHandle = system.CreateConstraint(worldHandle, constraintConfiguration);
        ASSERT_TRUE(constraintHandle);

        constraintConfiguration.m_firstBodyHandle = unrelatedFirstBodyHandle;
        constraintConfiguration.m_secondBodyHandle = unrelatedSecondBodyHandle;
        const ConstraintHandle unrelatedConstraintHandle = system.CreateConstraint(worldHandle, constraintConfiguration);
        ASSERT_TRUE(unrelatedConstraintHandle);

        const AZStd::array partitionBodies = {firstBodyHandle, secondBodyHandle};
        const StateSnapshotConfiguration snapshotConfiguration = {
            .m_flags = StateSnapshotFlags::Bodies | StateSnapshotFlags::Constraints,
            .m_filterBodies = true,
        };
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(
            worldHandle,
            snapshotConfiguration,
            partitionBodies);
        ASSERT_TRUE(snapshotHandle);

        ASSERT_TRUE(system.UpdateConstraintSolverConfiguration(
            worldHandle,
            unrelatedConstraintHandle,
            {
                .m_priority = 2,
                .m_positionStepCount = 3,
                .m_velocityStepCount = 4,
            }));
        EXPECT_TRUE(system.RestoreWorldState(worldHandle, snapshotHandle));

        const StateSnapshotHandle selectedMutationSnapshotHandle = system.CaptureWorldState(
            worldHandle,
            snapshotConfiguration,
            partitionBodies);
        ASSERT_TRUE(selectedMutationSnapshotHandle);
        ASSERT_TRUE(system.UpdateConstraintSolverConfiguration(
            worldHandle,
            constraintHandle,
            {
                .m_priority = 5,
                .m_positionStepCount = 6,
                .m_velocityStepCount = 7,
            }));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, selectedMutationSnapshotHandle));

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, selectedMutationSnapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyConstraint(worldHandle, unrelatedConstraintHandle));
        EXPECT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, unrelatedSecondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, unrelatedFirstBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, FilteredSnapshotsRestoreDisjointContactPartitions)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration staticBodyConfiguration;
        staticBodyConfiguration.m_shapeHandle = shapeHandle;
        staticBodyConfiguration.m_motionType = MotionType::Static;
        staticBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        staticBodyConfiguration.m_transform.m_position = {.m_x = -10.0};
        const BodyHandle firstStaticBodyHandle = system.CreateBody(worldHandle, staticBodyConfiguration);
        ASSERT_TRUE(firstStaticBodyHandle);

        BodyConfiguration dynamicBodyConfiguration;
        dynamicBodyConfiguration.m_shapeHandle = shapeHandle;
        dynamicBodyConfiguration.m_transform.m_position = {.m_x = -10.0, .m_z = 0.75};
        const BodyHandle firstDynamicBodyHandle = system.CreateBody(worldHandle, dynamicBodyConfiguration);
        ASSERT_TRUE(firstDynamicBodyHandle);

        staticBodyConfiguration.m_transform.m_position = {.m_x = 10.0};
        const BodyHandle secondStaticBodyHandle = system.CreateBody(worldHandle, staticBodyConfiguration);
        ASSERT_TRUE(secondStaticBodyHandle);

        dynamicBodyConfiguration.m_transform.m_position = {.m_x = 10.0, .m_z = 0.75};
        const BodyHandle secondDynamicBodyHandle = system.CreateBody(worldHandle, dynamicBodyConfiguration);
        ASSERT_TRUE(secondDynamicBodyHandle);

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.WereBodiesInContact(
            worldHandle,
            firstStaticBodyHandle,
            firstDynamicBodyHandle));
        ASSERT_TRUE(system.WereBodiesInContact(
            worldHandle,
            secondStaticBodyHandle,
            secondDynamicBodyHandle));
        BodyState capturedFirstDynamicState;
        ASSERT_TRUE(system.GetBodyState(
            worldHandle,
            firstDynamicBodyHandle,
            capturedFirstDynamicState));
        BodyState capturedSecondDynamicState;
        ASSERT_TRUE(system.GetBodyState(
            worldHandle,
            secondDynamicBodyHandle,
            capturedSecondDynamicState));

        constexpr StateSnapshotFlags snapshotFlags =
            StateSnapshotFlags::Bodies | StateSnapshotFlags::Contacts;
        const AZStd::array secondPartitionBodies = {
            firstDynamicBodyHandle,
            secondDynamicBodyHandle,
        };
        const AZStd::array partitionBodies = {
            firstStaticBodyHandle,
            secondStaticBodyHandle,
            firstDynamicBodyHandle,
            secondDynamicBodyHandle,
        };
        const AZStd::array<AZ::u32, 2> partitionBodyCounts = {2, 2};
        AZStd::array<StateSnapshotHandle, 2> snapshotHandles;
        ASSERT_TRUE(system.CaptureWorldStateParts(
            worldHandle,
            {
                .m_flags = snapshotFlags,
                .m_filterBodies = true,
            },
            partitionBodies,
            partitionBodyCounts,
            snapshotHandles));
        const StateSnapshotHandle firstSnapshotHandle = snapshotHandles[0];
        const StateSnapshotHandle secondSnapshotHandle = snapshotHandles[1];
        const StateSnapshotHandle mismatchedSnapshotHandle = system.CaptureWorldState(
            worldHandle,
            {
                .m_flags = StateSnapshotFlags::Bodies,
                .m_filterBodies = true,
            },
            secondPartitionBodies);
        ASSERT_TRUE(mismatchedSnapshotHandle);

        WorldTransform separatedFirstTransform;
        separatedFirstTransform.m_position = {.m_x = -10.0, .m_z = 5.0};
        ASSERT_TRUE(system.SetBodyTransformAndVelocities(
            worldHandle,
            firstDynamicBodyHandle,
            separatedFirstTransform,
            AZ::Vector3::CreateZero(),
            AZ::Vector3::CreateZero()));
        WorldTransform separatedSecondTransform;
        separatedSecondTransform.m_position = {.m_x = 10.0, .m_z = 5.0};
        ASSERT_TRUE(system.SetBodyTransformAndVelocities(
            worldHandle,
            secondDynamicBodyHandle,
            separatedSecondTransform,
            AZ::Vector3::CreateZero(),
            AZ::Vector3::CreateZero()));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        ASSERT_FALSE(system.WereBodiesInContact(
            worldHandle,
            firstStaticBodyHandle,
            firstDynamicBodyHandle));
        ASSERT_FALSE(system.WereBodiesInContact(
            worldHandle,
            secondStaticBodyHandle,
            secondDynamicBodyHandle));

        EXPECT_FALSE(system.RestoreWorldStateParts(worldHandle, {}));
        EXPECT_FALSE(system.RestoreWorldStateParts(
            worldHandle,
            AZStd::array{firstSnapshotHandle}));
        EXPECT_FALSE(system.RestoreWorldStateParts(
            worldHandle,
            AZStd::array{firstSnapshotHandle, firstSnapshotHandle}));
        EXPECT_FALSE(system.RestoreWorldStateParts(
            worldHandle,
            AZStd::array{firstSnapshotHandle, mismatchedSnapshotHandle}));
        BodyState stateAfterRejectedRestore;
        ASSERT_TRUE(system.GetBodyState(worldHandle, firstDynamicBodyHandle, stateAfterRejectedRestore));
        EXPECT_DOUBLE_EQ(
            stateAfterRejectedRestore.m_transform.m_position.m_z,
            separatedFirstTransform.m_position.m_z);
        ASSERT_TRUE(system.RestoreWorldStateParts(
            worldHandle,
            AZStd::array{firstSnapshotHandle, secondSnapshotHandle}));
        EXPECT_TRUE(system.WereBodiesInContact(
            worldHandle,
            firstStaticBodyHandle,
            firstDynamicBodyHandle));
        EXPECT_TRUE(system.WereBodiesInContact(
            worldHandle,
            secondStaticBodyHandle,
            secondDynamicBodyHandle));

        BodyState firstDynamicState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, firstDynamicBodyHandle, firstDynamicState));
        EXPECT_DOUBLE_EQ(
            firstDynamicState.m_transform.m_position.m_z,
            capturedFirstDynamicState.m_transform.m_position.m_z);
        BodyState secondDynamicState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, secondDynamicBodyHandle, secondDynamicState));
        EXPECT_DOUBLE_EQ(
            secondDynamicState.m_transform.m_position.m_z,
            capturedSecondDynamicState.m_transform.m_position.m_z);

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, mismatchedSnapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, secondSnapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, firstSnapshotHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondDynamicBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondStaticBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstDynamicBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstStaticBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, ContactSnapshotsRestoreRemovalEventProvenance)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_collectContactEvents = true;
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(system.SetBodyPosition(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle,
            {.m_z = 0.45},
            true));
        ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.WereBodiesInContact(
            scene.m_worldHandle,
            scene.m_floorBodyHandle,
            scene.m_sphereBodyHandle));

        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(scene.m_worldHandle);
        ASSERT_TRUE(snapshotHandle);
        StateSnapshotArchive archive;
        ASSERT_TRUE(system.ExportWorldStateArchive(
            scene.m_worldHandle,
            AZStd::array{snapshotHandle},
            archive));
        EXPECT_EQ(archive.m_formatVersion, 5);

        ASSERT_TRUE(system.SetBodyPosition(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle,
            {.m_z = 5.0},
            true));
        ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        ASSERT_FALSE(system.WereBodiesInContact(
            scene.m_worldHandle,
            scene.m_floorBodyHandle,
            scene.m_sphereBodyHandle));

        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, snapshotHandle));
        AZStd::array<StateSnapshotHandle, 1> importedHandles;
        ASSERT_TRUE(system.ImportWorldStateArchive(scene.m_worldHandle, archive, importedHandles));
        ASSERT_TRUE(system.RestoreWorldState(scene.m_worldHandle, importedHandles.front()));
        ASSERT_TRUE(system.WereBodiesInContact(
            scene.m_worldHandle,
            scene.m_floorBodyHandle,
            scene.m_sphereBodyHandle));

        ASSERT_TRUE(system.SetBodyPosition(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle,
            {.m_z = 5.0},
            true));
        ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        AZ::u32 removalEventCount = 0;
        for (const ContactEvent& event : system.GetEvents(scene.m_worldHandle).GetContacts())
        {
            if (event.m_phase == EventPhase::End
                && ((event.m_firstBodyHandle == scene.m_floorBodyHandle
                        && event.m_secondBodyHandle == scene.m_sphereBodyHandle)
                    || (event.m_firstBodyHandle == scene.m_sphereBodyHandle
                        && event.m_secondBodyHandle == scene.m_floorBodyHandle)))
            {
                ++removalEventCount;
            }
        }
        EXPECT_EQ(removalEventCount, 1);

        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, importedHandles.front()));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, PrewarmedFilteredStateRecaptureDoesNotAllocateNativeScratch)
    {
        constexpr AZ::u32 bodyCount = 128;
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        AZStd::vector<BodyHandle> bodyHandles;
        bodyHandles.reserve(bodyCount);
        for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
        {
            BodyConfiguration bodyConfiguration;
            bodyConfiguration.m_shapeHandle = shapeHandle;
            bodyConfiguration.m_transform.m_position = {
                .m_x = aznumeric_cast<double>(bodyIndex) * 2.0,
            };
            const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
            ASSERT_TRUE(bodyHandle);
            bodyHandles.push_back(bodyHandle);
        }

        AZStd::vector<ConstraintHandle> constraintHandles;
        constraintHandles.reserve(bodyHandles.size() - 1);
        for (size_t bodyIndex = 1; bodyIndex < bodyHandles.size(); ++bodyIndex)
        {
            ConstraintConfiguration constraintConfiguration;
            constraintConfiguration.m_firstBodyHandle = bodyHandles[bodyIndex - 1];
            constraintConfiguration.m_secondBodyHandle = bodyHandles[bodyIndex];
            constraintConfiguration.m_geometry = PointConstraintConfiguration{};
            const ConstraintHandle constraintHandle = system.CreateConstraint(
                worldHandle,
                constraintConfiguration);
            ASSERT_TRUE(constraintHandle);
            constraintHandles.push_back(constraintHandle);
        }

        const StateSnapshotConfiguration snapshotConfiguration{
            .m_flags = StateSnapshotFlags::Bodies | StateSnapshotFlags::Constraints,
            .m_filterBodies = true,
        };
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(
            worldHandle,
            snapshotConfiguration,
            bodyHandles);
        ASSERT_TRUE(snapshotHandle);
        ASSERT_TRUE(system.CaptureWorldState(
            worldHandle,
            snapshotHandle,
            snapshotConfiguration,
            bodyHandles));
        ASSERT_TRUE(system.CaptureWorldState(
            worldHandle,
            snapshotHandle,
            snapshotConfiguration,
            bodyHandles));

        {
            NativeAllocationCounterScope allocationCounter;
            for (AZ::u32 captureIndex = 0; captureIndex < 32; ++captureIndex)
            {
                ASSERT_TRUE(system.CaptureWorldState(
                    worldHandle,
                    snapshotHandle,
                    snapshotConfiguration,
                    bodyHandles));
            }
            EXPECT_EQ(allocationCounter.GetAllocationCount(), 0);
        }

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        for (const ConstraintHandle constraintHandle : constraintHandles)
        {
            EXPECT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        }
        for (const BodyHandle bodyHandle : bodyHandles)
        {
            EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        }
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, PrewarmedContactStateRecaptureDoesNotAllocateNativeScratch)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(system.SetBodyPosition(
            scene.m_worldHandle,
            scene.m_sphereBodyHandle,
            {.m_z = 0.45},
            true));
        ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.WereBodiesInContact(
            scene.m_worldHandle,
            scene.m_floorBodyHandle,
            scene.m_sphereBodyHandle));

        const AZStd::array bodyHandles{
            scene.m_floorBodyHandle,
            scene.m_sphereBodyHandle,
        };
        const StateSnapshotConfiguration snapshotConfiguration{
            .m_flags = StateSnapshotFlags::Contacts,
            .m_filterBodies = true,
        };
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(
            scene.m_worldHandle,
            snapshotConfiguration,
            bodyHandles);
        ASSERT_TRUE(snapshotHandle);
        ASSERT_TRUE(system.CaptureWorldState(
            scene.m_worldHandle,
            snapshotHandle,
            snapshotConfiguration,
            bodyHandles));

        {
            NativeAllocationCounterScope allocationCounter;
            for (AZ::u32 captureIndex = 0; captureIndex < 32; ++captureIndex)
            {
                ASSERT_TRUE(system.CaptureWorldState(
                    scene.m_worldHandle,
                    snapshotHandle,
                    snapshotConfiguration,
                    bodyHandles));
            }
            EXPECT_EQ(allocationCounter.GetAllocationCount(), 0);
        }

        EXPECT_TRUE(system.DestroyStateSnapshot(scene.m_worldHandle, snapshotHandle));
        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, BodySnapshotsRestoreWithoutWholeWorldTopologyCoupling)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_transform.m_position = {.m_x = 3.0, .m_y = 4.0, .m_z = 5.0};
        bodyConfiguration.m_linearVelocity = AZ::Vector3(1.0f, 2.0f, 3.0f);
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);
        const StateSnapshotHandle snapshotHandle = system.CaptureBodyState(worldHandle, bodyHandle);
        ASSERT_TRUE(snapshotHandle);
        EXPECT_TRUE(system.IsValid(worldHandle, snapshotHandle));
        const AZ::u64 eventSequenceBeforeRestore = system.GetEvents(worldHandle).GetSequence();
        WorldStatistics statistics;
        ASSERT_TRUE(system.GetWorldStatistics(worldHandle, statistics));
        EXPECT_EQ(statistics.m_stateSnapshotCount, 1);

        BodyConfiguration unrelatedConfiguration;
        unrelatedConfiguration.m_shapeHandle = shapeHandle;
        unrelatedConfiguration.m_transform.m_position = {.m_x = 100.0};
        const BodyHandle unrelatedBodyHandle = system.CreateBody(worldHandle, unrelatedConfiguration);
        ASSERT_TRUE(unrelatedBodyHandle);

        WorldTransform movedTransform;
        movedTransform.m_position = {.m_x = -8.0, .m_y = -9.0, .m_z = -10.0};
        ASSERT_TRUE(system.SetBodyTransformAndVelocities(
            worldHandle,
            bodyHandle,
            movedTransform,
            AZ::Vector3::CreateAxisZ(12.0f),
            AZ::Vector3::CreateAxisY()));
        ASSERT_TRUE(system.RestoreBodyState(worldHandle, snapshotHandle));
        EXPECT_NE(system.GetEvents(worldHandle).GetSequence(), eventSequenceBeforeRestore);

        BodyState restoredState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, restoredState));
        EXPECT_DOUBLE_EQ(restoredState.m_transform.m_position.m_x, 3.0);
        EXPECT_DOUBLE_EQ(restoredState.m_transform.m_position.m_y, 4.0);
        EXPECT_DOUBLE_EQ(restoredState.m_transform.m_position.m_z, 5.0);
        EXPECT_EQ(restoredState.m_linearVelocity, AZ::Vector3(1.0f, 2.0f, 3.0f));

        ASSERT_TRUE(system.SetBodyVelocities(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisX(7.0f),
            AZ::Vector3::CreateZero()));
        ASSERT_TRUE(system.CaptureBodyState(worldHandle, bodyHandle, snapshotHandle));
        ASSERT_TRUE(system.SetBodyVelocities(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisY(9.0f),
            AZ::Vector3::CreateZero()));
        ASSERT_TRUE(system.RestoreBodyState(worldHandle, snapshotHandle));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, restoredState));
        EXPECT_EQ(restoredState.m_linearVelocity, AZ::Vector3::CreateAxisX(7.0f));

        ASSERT_TRUE(system.SetBodyMotionType(worldHandle, bodyHandle, MotionType::Static, false));
        BodyState stateBeforeRejectedRestore;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, stateBeforeRejectedRestore));
        const StateRestoreResult rejectedRestore = system.RestoreBodyState(worldHandle, snapshotHandle);
        EXPECT_EQ(rejectedRestore.m_status, StateRestoreStatus::Rejected);
        BodyState stateAfterRejectedRestore;
        BodyConfiguration configurationAfterRejectedRestore;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, stateAfterRejectedRestore));
        ASSERT_TRUE(system.GetBodyConfiguration(worldHandle, bodyHandle, configurationAfterRejectedRestore));
        EXPECT_EQ(stateAfterRejectedRestore.m_transform.m_position, stateBeforeRejectedRestore.m_transform.m_position);
        EXPECT_EQ(stateAfterRejectedRestore.m_linearVelocity, stateBeforeRejectedRestore.m_linearVelocity);
        EXPECT_EQ(configurationAfterRejectedRestore.m_motionType, MotionType::Static);

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_FALSE(system.RestoreBodyState(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, snapshotHandle));
        ASSERT_TRUE(system.GetWorldStatistics(worldHandle, statistics));
        EXPECT_EQ(statistics.m_stateSnapshotCount, 0);
        EXPECT_TRUE(system.DestroyBody(worldHandle, unrelatedBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, StateDigestIsDeterministicAcrossWorkerCounts)
    {
        const WorldStateDigest serialDigest = SimulateDeterministicStack(1, nullptr);
        ASSERT_GT(serialDigest.m_stateByteCount, 0);

        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(4);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);
        const WorldStateDigest externalSerialDigest = SimulateDeterministicStack(1, &jobContext);
        const WorldStateDigest externalParallelDigest = SimulateDeterministicStack(4, &jobContext);

        EXPECT_EQ(externalSerialDigest, serialDigest);
        EXPECT_EQ(externalParallelDigest, serialDigest);

        const WorldStateDigest serialSoftBodyDigest = SimulateDeterministicSoftBodies(1, nullptr);
        ASSERT_GT(serialSoftBodyDigest.m_stateByteCount, 0);
        const WorldStateDigest externalSerialSoftBodyDigest =
            SimulateDeterministicSoftBodies(1, &jobContext);
        const WorldStateDigest externalParallelSoftBodyDigest =
            SimulateDeterministicSoftBodies(4, &jobContext);
        EXPECT_EQ(externalSerialSoftBodyDigest, serialSoftBodyDigest);
        EXPECT_EQ(externalParallelSoftBodyDigest, serialSoftBodyDigest);

        const WorldStateDigest serialCustomConstraintDigest =
            SimulateDeterministicCustomConstraints(1, nullptr);
        ASSERT_GT(serialCustomConstraintDigest.m_stateByteCount, 0);
        const WorldStateDigest externalSerialCustomConstraintDigest =
            SimulateDeterministicCustomConstraints(1, &jobContext);
        const WorldStateDigest externalParallelCustomConstraintDigest =
            SimulateDeterministicCustomConstraints(4, &jobContext);
        EXPECT_EQ(externalSerialCustomConstraintDigest, serialCustomConstraintDigest);
        EXPECT_EQ(externalParallelCustomConstraintDigest, serialCustomConstraintDigest);

        const WorldStateDigest serialVehicleDigest =
            SimulateDeterministicVehicleCallbacks(1, nullptr);
        ASSERT_GT(serialVehicleDigest.m_stateByteCount, 0);
        const WorldStateDigest externalSerialVehicleDigest =
            SimulateDeterministicVehicleCallbacks(1, &jobContext);
        const WorldStateDigest externalParallelVehicleDigest =
            SimulateDeterministicVehicleCallbacks(4, &jobContext);
        EXPECT_EQ(externalSerialVehicleDigest, serialVehicleDigest);
        EXPECT_EQ(externalParallelVehicleDigest, serialVehicleDigest);

        const WorldStateDigest serialRagdollDigest = SimulateDeterministicRagdoll(1, nullptr);
        ASSERT_GT(serialRagdollDigest.m_stateByteCount, 0);
        const WorldStateDigest externalSerialRagdollDigest =
            SimulateDeterministicRagdoll(1, &jobContext);
        const WorldStateDigest externalParallelRagdollDigest =
            SimulateDeterministicRagdoll(4, &jobContext);
        EXPECT_EQ(externalSerialRagdollDigest, serialRagdollDigest);
        EXPECT_EQ(externalParallelRagdollDigest, serialRagdollDigest);
    }

    TEST(SimulationTests, SimulationRestoresCallingThreadFloatEnvironment)
    {
        const int originalRoundingMode = std::fegetround();
        ASSERT_NE(originalRoundingMode, -1);
        ASSERT_EQ(std::fesetround(FE_DOWNWARD), 0);

        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        const bool stepSucceeded = system.StepWorld(system.GetDefaultWorldHandle(), 1.0f / 60.0f);
        const int restoredRoundingMode = std::fegetround();

        ASSERT_EQ(std::fesetround(originalRoundingMode), 0);
        EXPECT_TRUE(stepSucceeded);
        EXPECT_EQ(restoredRoundingMode, FE_DOWNWARD);
    }

    TEST(SimulationTests, ResourceConstructionIsDeterministicAcrossCallingThreadRoundingModes)
    {
        const int originalRoundingMode = std::fegetround();
        ASSERT_NE(originalRoundingMode, -1);

        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        HermitePathConfiguration configuration;
        configuration.m_points = {
            {
                .m_normal = AZ::Vector3(-1.0f, 2.0f, 0.0f),
                .m_position = AZ::Vector3::CreateZero(),
                .m_tangent = AZ::Vector3(2.0f, 1.0f, 0.0f),
            },
            {
                .m_normal = AZ::Vector3(2.0f, 1.0f, 0.0f),
                .m_position = AZ::Vector3(4.0f, 2.0f, 1.0f),
                .m_tangent = AZ::Vector3(1.0f, -2.0f, 1.0f),
            },
        };
        WorldConfiguration worldConfiguration;
        worldConfiguration.m_gravity = AZ::Vector3(1.0f, -2.0f, -9.81f);

        ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
        const PathHandle baselinePathHandle = system.CreatePath(configuration);
        PathSample baselineSample;
        const bool baselineSampled = system.SamplePath(baselinePathHandle, 0.375f, baselineSample);
        PathSample baselineClosest;
        const bool baselineClosestFound = system.FindClosestPathPoint(
            baselinePathHandle,
            AZ::Vector3(2.0f, 1.0f, 0.25f),
            0.5f,
            baselineClosest);
        const WorldHandle baselineWorldHandle = system.CreateWorld(worldConfiguration);
        WorldStateDigest baselineDigest;
        const bool baselineDigestRead = system.GetWorldStateDigest(baselineWorldHandle, baselineDigest);

        ASSERT_EQ(std::fesetround(FE_DOWNWARD), 0);
        const PathHandle poisonedPathHandle = system.CreatePath(configuration);
        PathSample poisonedSample;
        const bool poisonedSampled = system.SamplePath(poisonedPathHandle, 0.375f, poisonedSample);
        PathSample poisonedClosest;
        const bool poisonedClosestFound = system.FindClosestPathPoint(
            poisonedPathHandle,
            AZ::Vector3(2.0f, 1.0f, 0.25f),
            0.5f,
            poisonedClosest);
        const WorldHandle poisonedWorldHandle = system.CreateWorld(worldConfiguration);
        WorldStateDigest poisonedDigest;
        const bool poisonedDigestRead = system.GetWorldStateDigest(poisonedWorldHandle, poisonedDigest);
        const int restoredRoundingMode = std::fegetround();

        const int resetResult = std::fesetround(originalRoundingMode);
        ASSERT_EQ(resetResult, 0);
        ASSERT_TRUE(baselinePathHandle);
        ASSERT_TRUE(poisonedPathHandle);
        ASSERT_TRUE(baselineSampled);
        ASSERT_TRUE(poisonedSampled);
        ASSERT_TRUE(baselineClosestFound);
        ASSERT_TRUE(poisonedClosestFound);
        ASSERT_TRUE(baselineWorldHandle);
        ASSERT_TRUE(poisonedWorldHandle);
        ASSERT_TRUE(baselineDigestRead);
        ASSERT_TRUE(poisonedDigestRead);
        EXPECT_EQ(restoredRoundingMode, FE_DOWNWARD);
        EXPECT_EQ(poisonedSample.m_position, baselineSample.m_position);
        EXPECT_EQ(poisonedSample.m_tangent, baselineSample.m_tangent);
        EXPECT_EQ(poisonedSample.m_normal, baselineSample.m_normal);
        EXPECT_EQ(poisonedSample.m_binormal, baselineSample.m_binormal);
        EXPECT_EQ(poisonedSample.m_fraction, baselineSample.m_fraction);
        EXPECT_EQ(poisonedClosest.m_position, baselineClosest.m_position);
        EXPECT_EQ(poisonedClosest.m_tangent, baselineClosest.m_tangent);
        EXPECT_EQ(poisonedClosest.m_normal, baselineClosest.m_normal);
        EXPECT_EQ(poisonedClosest.m_binormal, baselineClosest.m_binormal);
        EXPECT_EQ(poisonedClosest.m_fraction, baselineClosest.m_fraction);
        EXPECT_EQ(poisonedDigest, baselineDigest);

        EXPECT_TRUE(system.DestroyWorld(poisonedWorldHandle));
        EXPECT_TRUE(system.DestroyWorld(baselineWorldHandle));
        EXPECT_TRUE(system.DestroyPath(poisonedPathHandle));
        EXPECT_TRUE(system.DestroyPath(baselinePathHandle));
    }

    TEST(SimulationTests, ShapeOverlapAndCastQueriesPreserveProviderIdentity)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration targetShapeConfiguration;
        targetShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(2.0f, 2.0f, 2.0f),
        };
        const ShapeHandle targetShapeHandle = system.CreateShape(worldHandle, targetShapeConfiguration);
        ASSERT_TRUE(targetShapeHandle);
        BodyConfiguration targetBodyConfiguration;
        targetBodyConfiguration.m_shapeHandle = targetShapeHandle;
        targetBodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle targetBodyHandle = system.CreateBody(worldHandle, targetBodyConfiguration);
        ASSERT_TRUE(targetBodyHandle);

        ShapeConfiguration queryShapeConfiguration;
        queryShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle queryShapeHandle = system.CreateShape(worldHandle, queryShapeConfiguration);
        ASSERT_TRUE(queryShapeHandle);

        ShapeOverlapRequest overlapRequest;
        overlapRequest.m_shapeHandle = queryShapeHandle;
        EXPECT_TRUE(system.OverlapShapeAny(worldHandle, overlapRequest));
        AZStd::array<ShapeOverlapHit, 2> overlapHits;
        const QueryResult overlapResult = system.CollideShape(worldHandle, overlapRequest, overlapHits);
        ASSERT_EQ(overlapResult.m_hitCount, 1);
        EXPECT_TRUE(overlapResult.IsComplete());
        EXPECT_EQ(overlapHits[0].m_bodyHandle, targetBodyHandle);
        EXPECT_EQ(overlapHits[0].m_shapeHandle, targetShapeHandle);
        EXPECT_GT(overlapHits[0].m_penetrationDepth, 0.0f);

        AZStd::array<OverlapHit, 2> identityHits;
        const QueryResult identityResult = system.OverlapShape(worldHandle, overlapRequest, identityHits);
        ASSERT_EQ(identityResult.m_hitCount, 1);
        EXPECT_TRUE(identityResult.IsComplete());
        EXPECT_EQ(identityHits[0].m_bodyHandle, targetBodyHandle);
        EXPECT_EQ(identityHits[0].m_shapeHandle, targetShapeHandle);

        RecordingQueryFilter queryFilter;
        overlapRequest.m_filter.m_callback = &queryFilter;
        EXPECT_TRUE(system.OverlapShapeAny(worldHandle, overlapRequest));
        EXPECT_GT(queryFilter.m_shapePairCallCount, 0);
        EXPECT_EQ(queryFilter.m_lastQueryShape.m_rootShapeHandle, queryShapeHandle);
        EXPECT_FALSE(queryFilter.m_lastQueryShape.m_bodyHandle);
        EXPECT_EQ(queryFilter.m_lastQueryShape.m_kind, ShapeKind::Sphere);
        EXPECT_EQ(queryFilter.m_lastTargetShape.m_bodyHandle, targetBodyHandle);
        EXPECT_EQ(queryFilter.m_lastTargetShape.m_rootShapeHandle, targetShapeHandle);
        EXPECT_EQ(queryFilter.m_lastTargetShape.m_kind, ShapeKind::Box);

        queryFilter.m_rejectedQueryShapeKind = ShapeKind::Sphere;
        EXPECT_FALSE(system.OverlapShapeAny(worldHandle, overlapRequest));
        EXPECT_EQ(system.CollideShape(worldHandle, overlapRequest, overlapHits).m_hitCount, 0);
        queryFilter.m_rejectedQueryShapeKind = ShapeKind::None;
        overlapRequest.m_filter.m_callback = nullptr;

        overlapRequest.m_removeInternalEdges = true;
        EXPECT_TRUE(system.OverlapShapeAny(worldHandle, overlapRequest));
        const QueryResult edgeRemovedOverlapResult =
            system.CollideShape(worldHandle, overlapRequest, overlapHits);
        EXPECT_EQ(edgeRemovedOverlapResult.m_hitCount, 1);
        EXPECT_TRUE(edgeRemovedOverlapResult.IsComplete());

        ShapeCastRequest castRequest;
        castRequest.m_shapeHandle = queryShapeHandle;
        castRequest.m_start.m_position.m_x = -3.0;
        castRequest.m_displacement = AZ::Vector3::CreateAxisX(6.0f);
        const AZ::u32 previousShapePairCallCount = queryFilter.m_shapePairCallCount;
        castRequest.m_filter.m_callback = &queryFilter;
        ShapeCastHit closestHit;
        ASSERT_TRUE(system.CastShapeClosest(worldHandle, castRequest, closestHit));
        EXPECT_GT(queryFilter.m_shapePairCallCount, previousShapePairCallCount);
        castRequest.m_filter.m_callback = nullptr;
        EXPECT_EQ(closestHit.m_bodyHandle, targetBodyHandle);
        EXPECT_EQ(closestHit.m_shapeHandle, targetShapeHandle);
        EXPECT_NEAR(closestHit.m_fraction, 0.25f, 0.01f);

        castRequest.m_extraConvexRadius = 0.25f;
        ShapeCastHit inflatedHit;
        ASSERT_TRUE(system.CastShapeClosest(worldHandle, castRequest, inflatedHit));
        EXPECT_LT(inflatedHit.m_fraction, closestHit.m_fraction);
        castRequest.m_extraConvexRadius = AZStd::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(system.CastShapeClosest(worldHandle, castRequest, inflatedHit));
        castRequest.m_extraConvexRadius = 0.0f;

        AZStd::array<ShapeCastHit, 2> castHits;
        const QueryResult castResult = system.CastShapeAll(worldHandle, castRequest, castHits);
        ASSERT_EQ(castResult.m_hitCount, 1);
        EXPECT_TRUE(castResult.IsComplete());
        EXPECT_EQ(castHits[0].m_bodyHandle, targetBodyHandle);
        EXPECT_EQ(castHits[0].m_querySubShapeId, closestHit.m_querySubShapeId);
        EXPECT_EQ(castHits[0].m_targetSubShapeId, closestHit.m_targetSubShapeId);

        EXPECT_TRUE(system.DestroyShape(worldHandle, queryShapeHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, targetBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, targetShapeHandle));
    }

    TEST(SimulationTests, ShapeQueriesExposeScaleAndBoundedSupportingFaces)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration boxConfiguration;
        boxConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3::CreateOne(),
        };
        const ShapeHandle boxShapeHandle = system.CreateShape(worldHandle, boxConfiguration);
        ASSERT_TRUE(boxShapeHandle);

        BodyConfiguration overlapBodyConfiguration;
        overlapBodyConfiguration.m_shapeHandle = boxShapeHandle;
        overlapBodyConfiguration.m_transform.m_position.m_x = 1.25;
        overlapBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        overlapBodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle overlapBodyHandle = system.CreateBody(
            worldHandle,
            overlapBodyConfiguration);
        ASSERT_TRUE(overlapBodyHandle);

        ShapeOverlapRequest overlapRequest;
        overlapRequest.m_shapeHandle = boxShapeHandle;
        overlapRequest.m_scale = AZ::Vector3(2.0f, 1.0f, 1.0f);
        overlapRequest.m_faceCollectionMode = FaceCollectionMode::Collect;
        AZStd::array<ShapeOverlapHit, 1> overlapHits;
        const WorldPosition sentinel = {
            .m_x = 101.0,
            .m_y = 202.0,
            .m_z = 303.0,
        };
        AZStd::array<WorldPosition, 3> overlapQueryFaces = {sentinel, sentinel, sentinel};
        AZStd::array<WorldPosition, 3> overlapTargetFaces = {sentinel, sentinel, sentinel};
        const QueryResult overlapResult = system.CollideShape(
            worldHandle,
            overlapRequest,
            overlapHits,
            {
                .m_queryVertices = AZStd::span(overlapQueryFaces).first(2),
                .m_targetVertices = AZStd::span(overlapTargetFaces).first(2),
            });
        ASSERT_EQ(overlapResult.m_hitCount, 1);
        EXPECT_TRUE(overlapResult.IsComplete());
        EXPECT_EQ(overlapHits[0].m_bodyHandle, overlapBodyHandle);
        EXPECT_EQ(overlapHits[0].m_queryFaceVertexCount, 4);
        EXPECT_EQ(overlapHits[0].m_targetFaceVertexCount, 4);
        EXPECT_TRUE(IsFiniteWorldPosition(overlapQueryFaces[0]));
        EXPECT_TRUE(IsFiniteWorldPosition(overlapQueryFaces[1]));
        EXPECT_TRUE(IsFiniteWorldPosition(overlapTargetFaces[0]));
        EXPECT_TRUE(IsFiniteWorldPosition(overlapTargetFaces[1]));
        EXPECT_EQ(overlapQueryFaces[2], sentinel);
        EXPECT_EQ(overlapTargetFaces[2], sentinel);

        overlapRequest.m_faceCollectionMode = FaceCollectionMode::None;
        overlapRequest.m_scale = AZ::Vector3::CreateOne();
        EXPECT_FALSE(system.OverlapShapeAny(worldHandle, overlapRequest));
        overlapRequest.m_scale = AZ::Vector3::CreateZero();
        EXPECT_FALSE(system.OverlapShapeAny(worldHandle, overlapRequest));
        overlapRequest.m_scale = AZ::Vector3(2.0f, 1.0f, 1.0f);
        overlapRequest.m_internalEdgeRemovalVertexTolerance =
            AZStd::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(system.OverlapShapeAny(worldHandle, overlapRequest));
        overlapRequest.m_internalEdgeRemovalVertexTolerance = 1.0e-4f;
        overlapRequest.m_backFaceMode = static_cast<BackFaceMode>(255);
        EXPECT_FALSE(system.OverlapShapeAny(worldHandle, overlapRequest));

        BodyConfiguration castBodyConfiguration;
        castBodyConfiguration.m_shapeHandle = boxShapeHandle;
        castBodyConfiguration.m_transform.m_position = {
            .m_x = 3.0,
            .m_y = 4.0,
        };
        castBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        castBodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle castBodyHandle = system.CreateBody(
            worldHandle,
            castBodyConfiguration);
        ASSERT_TRUE(castBodyHandle);

        ShapeCastRequest castRequest;
        castRequest.m_shapeHandle = boxShapeHandle;
        castRequest.m_start.m_position.m_y = 4.0;
        castRequest.m_scale = AZ::Vector3(2.0f, 1.0f, 1.0f);
        castRequest.m_displacement = AZ::Vector3::CreateAxisX(6.0f);
        castRequest.m_faceCollectionMode = FaceCollectionMode::Collect;
        AZStd::array<WorldPosition, MaximumSupportingFaceVertexCount> castQueryFaces;
        AZStd::array<WorldPosition, MaximumSupportingFaceVertexCount> castTargetFaces;
        ShapeCastHit scaledCastHit;
        ASSERT_TRUE(system.CastShapeClosest(
            worldHandle,
            castRequest,
            scaledCastHit,
            {
                .m_queryVertices = castQueryFaces,
                .m_targetVertices = castTargetFaces,
            }));
        EXPECT_EQ(scaledCastHit.m_bodyHandle, castBodyHandle);
        EXPECT_NEAR(scaledCastHit.m_fraction, 0.25f, 1.0e-4f);
        EXPECT_EQ(scaledCastHit.m_queryFaceVertexCount, 4);
        EXPECT_EQ(scaledCastHit.m_targetFaceVertexCount, 4);
        for (AZ::u8 vertexIndex = 0; vertexIndex < scaledCastHit.m_queryFaceVertexCount; ++vertexIndex)
        {
            EXPECT_TRUE(IsFiniteWorldPosition(castQueryFaces[vertexIndex]));
            EXPECT_NEAR(castQueryFaces[vertexIndex].m_x, 2.5, 1.0e-4);
        }
        for (AZ::u8 vertexIndex = 0; vertexIndex < scaledCastHit.m_targetFaceVertexCount; ++vertexIndex)
        {
            EXPECT_TRUE(IsFiniteWorldPosition(castTargetFaces[vertexIndex]));
            EXPECT_NEAR(castTargetFaces[vertexIndex].m_x, 2.5, 1.0e-4);
        }

        castRequest.m_faceCollectionMode = FaceCollectionMode::None;
        castRequest.m_scale = AZ::Vector3::CreateOne();
        ShapeCastHit unscaledCastHit;
        ASSERT_TRUE(system.CastShapeClosest(worldHandle, castRequest, unscaledCastHit));
        EXPECT_NEAR(unscaledCastHit.m_fraction, 1.0f / 3.0f, 1.0e-4f);
        EXPECT_LT(scaledCastHit.m_fraction, unscaledCastHit.m_fraction);
        castRequest.m_convexBackFaceMode = static_cast<BackFaceMode>(255);
        EXPECT_FALSE(system.CastShapeClosest(worldHandle, castRequest, unscaledCastHit));

        EXPECT_TRUE(system.DestroyBody(worldHandle, castBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, overlapBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, boxShapeHandle));
    }

    TEST(SimulationTests, StandaloneShapesExposeLocalRaycastsAndPointCollision)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const MaterialHandle materialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(materialHandle);

        ShapeConfiguration boxConfiguration;
        boxConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3::CreateOne(),
        };
        boxConfiguration.m_materials = {materialHandle};
        const ShapeHandle boxShapeHandle = system.CreateShape(worldHandle, boxConfiguration);
        ASSERT_TRUE(boxShapeHandle);

        DecoratedShapeConfiguration offsetConfiguration;
        offsetConfiguration.m_geometry = OffsetCenterOfMassShapeConfiguration{
            .m_shapeHandle = boxShapeHandle,
            .m_offset = AZ::Vector3::CreateAxisX(0.5f),
        };
        const ShapeHandle offsetShapeHandle = system.CreateShape(worldHandle, offsetConfiguration);
        ASSERT_TRUE(offsetShapeHandle);

        RecordingQueryFilter filter;
        ShapeRaycastRequest raycastRequest;
        raycastRequest.m_shapeHandle = offsetShapeHandle;
        raycastRequest.m_start = AZ::Vector3::CreateAxisX(-2.0f);
        raycastRequest.m_displacement = AZ::Vector3::CreateAxisX(4.0f);
        raycastRequest.m_filter = &filter;
        ShapeRaycastHit closestHit;
        ASSERT_TRUE(system.RaycastShapeClosest(worldHandle, raycastRequest, closestHit));
        EXPECT_NEAR(closestHit.m_fraction, 0.375f, 1.0e-4f);
        EXPECT_NEAR(closestHit.m_position.GetX(), -0.5f, 1.0e-4f);
        EXPECT_NEAR(closestHit.m_normal.GetX(), -1.0f, 1.0e-4f);
        EXPECT_EQ(closestHit.m_materialHandle, materialHandle);
        EXPECT_EQ(filter.m_lastShape.m_rootShapeHandle, offsetShapeHandle);
        EXPECT_FALSE(filter.m_lastShape.m_bodyHandle);

        AZStd::array<ShapeRaycastHit, 1> raycastHits;
        const QueryResult raycastResult = system.RaycastShapeAll(
            worldHandle,
            raycastRequest,
            raycastHits);
        ASSERT_TRUE(raycastResult.IsComplete());
        ASSERT_EQ(raycastResult.m_hitCount, 1);
        EXPECT_EQ(raycastHits[0].m_subShapeId, closestHit.m_subShapeId);

        AZStd::array<ShapePointHit, 1> pointHits;
        const QueryResult pointResult = system.CollideShapePoint(
            worldHandle,
            offsetShapeHandle,
            AZ::Vector3::CreateZero(),
            &filter,
            pointHits);
        ASSERT_TRUE(pointResult.IsComplete());
        ASSERT_EQ(pointResult.m_hitCount, 1);
        EXPECT_EQ(pointHits[0].m_materialHandle, materialHandle);
        EXPECT_TRUE(system.CollideShapePointAny(
            worldHandle,
            offsetShapeHandle,
            AZ::Vector3::CreateZero()));
        EXPECT_FALSE(system.CollideShapePointAny(
            worldHandle,
            offsetShapeHandle,
            AZ::Vector3::CreateAxisX(2.0f)));

        filter.m_rejectedShapeKind = ShapeKind::Box;
        EXPECT_FALSE(system.CollideShapePointAny(
            worldHandle,
            offsetShapeHandle,
            AZ::Vector3::CreateZero(),
            &filter));
        filter.m_rejectedShapeKind = ShapeKind::None;

        raycastRequest.m_convexBackFaceMode = static_cast<BackFaceMode>(255);
        EXPECT_FALSE(system.RaycastShapeClosest(worldHandle, raycastRequest, closestHit));

        const ShapeHandle scaledShapeHandle = system.ScaleShape(
            worldHandle,
            boxShapeHandle,
            AZ::Vector3(2.0f, 1.0f, 1.0f));
        ASSERT_TRUE(scaledShapeHandle);
        ShapeRaycastRequest scaledRaycastRequest;
        scaledRaycastRequest.m_shapeHandle = scaledShapeHandle;
        scaledRaycastRequest.m_start = AZ::Vector3::CreateAxisX(-2.0f);
        scaledRaycastRequest.m_displacement = AZ::Vector3::CreateAxisX(4.0f);
        ShapeRaycastHit scaledHit;
        ASSERT_TRUE(system.RaycastShapeClosest(worldHandle, scaledRaycastRequest, scaledHit));
        EXPECT_NEAR(scaledHit.m_fraction, 0.25f, 1.0e-4f);
        EXPECT_TRUE(system.CollideShapePointAny(
            worldHandle,
            scaledShapeHandle,
            AZ::Vector3::CreateAxisX(0.75f)));
        EXPECT_FALSE(system.CollideShapePointAny(
            worldHandle,
            boxShapeHandle,
            AZ::Vector3::CreateAxisX(0.75f)));

        ShapeTriangleCollectionRequest triangleRequest;
        triangleRequest.m_shapeHandle = boxShapeHandle;
        triangleRequest.m_transform = AZ::Transform::CreateUniformScale(2.0f);
        triangleRequest.m_transform.SetTranslation(AZ::Vector3::CreateAxisZ(3.0f));
        triangleRequest.m_boundsCenter = AZ::Vector3::CreateAxisZ(3.0f);
        triangleRequest.m_boundsHalfExtents = AZ::Vector3(2.0f);
        AZStd::array<ShapeTriangle, 1> triangles;
        const QueryResult triangleResult = system.CollectShapeTriangles(
            worldHandle,
            triangleRequest,
            triangles);
        EXPECT_EQ(triangleResult.m_hitCount, 1);
        EXPECT_EQ(triangleResult.m_requiredHitCount, 12);
        EXPECT_FALSE(triangleResult.IsComplete());
        EXPECT_EQ(triangles[0].m_materialHandle, materialHandle);
        EXPECT_TRUE(triangles[0].m_firstVertex.IsFinite());
        EXPECT_GE(triangles[0].m_firstVertex.GetZ(), 2.0f);
        EXPECT_LE(triangles[0].m_firstVertex.GetZ(), 4.0f);

        triangleRequest.m_shapeHandle = offsetShapeHandle;
        triangleRequest.m_transform = AZ::Transform::CreateIdentity();
        triangleRequest.m_boundsCenter = AZ::Vector3::CreateZero();
        triangleRequest.m_boundsHalfExtents = AZ::Vector3::CreateOne();
        const QueryResult decoratedTriangleResult = system.CollectShapeTriangles(
            worldHandle,
            triangleRequest,
            triangles);
        EXPECT_EQ(decoratedTriangleResult.m_hitCount, 1);
        EXPECT_EQ(decoratedTriangleResult.m_requiredHitCount, 12);

        EXPECT_TRUE(system.DestroyShape(worldHandle, scaledShapeHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, offsetShapeHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, boxShapeHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, SphereOverlapRejectsRotatedBoxBoundsAndFallsBackForOtherShapes)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration boxConfiguration;
        boxConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(4.0f, 0.2f, 0.2f),
        };
        const ShapeHandle boxShapeHandle = system.CreateShape(worldHandle, boxConfiguration);
        ASSERT_TRUE(boxShapeHandle);

        BodyConfiguration rejectedBoxConfiguration;
        rejectedBoxConfiguration.m_shapeHandle = boxShapeHandle;
        rejectedBoxConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        rejectedBoxConfiguration.m_motionType = MotionType::Static;
        rejectedBoxConfiguration.m_transform.m_position = {
            .m_x = 1.5,
            .m_y = -1.5,
            .m_z = 0.25,
        };
        rejectedBoxConfiguration.m_transform.m_rotation =
            AZ::Quaternion::CreateRotationZ(AZ::Constants::QuarterPi);
        const BodyHandle rejectedBoxHandle = system.CreateBody(
            worldHandle,
            rejectedBoxConfiguration);
        ASSERT_TRUE(rejectedBoxHandle);

        BodyConfiguration overlappingBoxConfiguration;
        overlappingBoxConfiguration.m_shapeHandle = boxShapeHandle;
        overlappingBoxConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        overlappingBoxConfiguration.m_motionType = MotionType::Static;
        overlappingBoxConfiguration.m_transform.m_position.m_z = 0.25;
        const BodyHandle overlappingBoxHandle = system.CreateBody(
            worldHandle,
            overlappingBoxConfiguration);
        ASSERT_TRUE(overlappingBoxHandle);

        ShapeConfiguration sphereConfiguration;
        sphereConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle sphereShapeHandle = system.CreateShape(worldHandle, sphereConfiguration);
        ASSERT_TRUE(sphereShapeHandle);

        BodyConfiguration sphereBodyConfiguration;
        sphereBodyConfiguration.m_shapeHandle = sphereShapeHandle;
        sphereBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        sphereBodyConfiguration.m_motionType = MotionType::Static;
        sphereBodyConfiguration.m_transform.m_position = {
            .m_x = 0.75,
            .m_z = 0.25,
        };
        const BodyHandle sphereBodyHandle = system.CreateBody(
            worldHandle,
            sphereBodyConfiguration);
        ASSERT_TRUE(sphereBodyHandle);

        sphereBodyConfiguration.m_transform.m_position.m_x = 1.05;
        const BodyHandle separatedSphereBodyHandle = system.CreateBody(
            worldHandle,
            sphereBodyConfiguration);
        ASSERT_TRUE(separatedSphereBodyHandle);

        ShapeOverlapRequest request;
        request.m_shapeHandle = sphereShapeHandle;
        request.m_transform.m_position.m_z = 0.25;
        AZStd::array<OverlapHit, 5> hits;
        QueryResult result = system.OverlapShape(worldHandle, request, hits);
        ASSERT_EQ(result.m_hitCount, 2);
        ASSERT_EQ(result.m_requiredHitCount, 2);
        EXPECT_EQ(hits[0].m_bodyHandle, overlappingBoxHandle);
        EXPECT_EQ(hits[1].m_bodyHandle, sphereBodyHandle);

        request.m_maximumSeparationDistance = 0.1f;
        result = system.OverlapShape(worldHandle, request, hits);
        ASSERT_EQ(result.m_hitCount, 3);
        ASSERT_EQ(result.m_requiredHitCount, 3);
        EXPECT_EQ(hits[2].m_bodyHandle, separatedSphereBodyHandle);
        request.m_maximumSeparationDistance = 0.0f;

        request.m_filter.m_collisionLayer = DefaultLayers::NonMoving;
        result = system.OverlapShape(worldHandle, request, hits);
        EXPECT_EQ(result.m_hitCount, 0);
        EXPECT_TRUE(result.IsComplete());
        request.m_filter.m_collisionLayer = DefaultLayers::Moving;
        result = system.OverlapShape(worldHandle, request, hits);
        EXPECT_EQ(result.m_hitCount, 2);
        EXPECT_TRUE(result.IsComplete());
        request.m_filter.m_collisionLayer = {};

        ShapeConfiguration triangleConfiguration;
        triangleConfiguration.m_geometry = TriangleShapeConfiguration{};
        const ShapeHandle triangleShapeHandle = system.CreateShape(
            worldHandle,
            triangleConfiguration);
        ASSERT_TRUE(triangleShapeHandle);
        BodyConfiguration triangleBodyConfiguration;
        triangleBodyConfiguration.m_shapeHandle = triangleShapeHandle;
        triangleBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        triangleBodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle triangleBodyHandle = system.CreateBody(
            worldHandle,
            triangleBodyConfiguration);
        ASSERT_TRUE(triangleBodyHandle);

        result = system.OverlapShape(worldHandle, request, hits);
        ASSERT_EQ(result.m_hitCount, 3);
        ASSERT_EQ(result.m_requiredHitCount, 3);
        EXPECT_EQ(hits[0].m_bodyHandle, overlappingBoxHandle);
        EXPECT_EQ(hits[1].m_bodyHandle, sphereBodyHandle);
        EXPECT_EQ(hits[2].m_bodyHandle, triangleBodyHandle);
    }

    TEST(SimulationTests, ShapeOverlapPreservesStableOrderBeyondLocalResultCapacity)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration targetShapeConfiguration;
        targetShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(0.05f, 0.05f, 0.05f),
        };
        const ShapeHandle targetShapeHandle = system.CreateShape(worldHandle, targetShapeConfiguration);
        ASSERT_TRUE(targetShapeHandle);

        constexpr size_t targetCount = 40;
        AZStd::array<BodyHandle, targetCount> targetBodyHandles;
        for (size_t targetIndex = 0; targetIndex < targetCount; ++targetIndex)
        {
            BodyConfiguration targetBodyConfiguration;
            targetBodyConfiguration.m_shapeHandle = targetShapeHandle;
            targetBodyConfiguration.m_motionType = MotionType::Static;
            targetBodyConfiguration.m_transform.m_position.m_x = 0.1 * static_cast<double>(targetIndex);
            targetBodyHandles[targetIndex] = system.CreateBody(worldHandle, targetBodyConfiguration);
            ASSERT_TRUE(targetBodyHandles[targetIndex]);
        }

        ShapeConfiguration queryShapeConfiguration;
        queryShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 10.0f};
        const ShapeHandle queryShapeHandle = system.CreateShape(worldHandle, queryShapeConfiguration);
        ASSERT_TRUE(queryShapeHandle);

        ShapeOverlapRequest request;
        request.m_shapeHandle = queryShapeHandle;
        AZStd::array<ShapeOverlapHit, 8> truncatedHits;
        const QueryResult truncatedResult = system.CollideShape(worldHandle, request, truncatedHits);
        EXPECT_EQ(truncatedResult.m_hitCount, truncatedHits.size());
        EXPECT_EQ(truncatedResult.m_requiredHitCount, targetCount);
        EXPECT_FALSE(truncatedResult.IsComplete());
        for (size_t hitIndex = 0; hitIndex < truncatedHits.size(); ++hitIndex)
        {
            EXPECT_EQ(truncatedHits[hitIndex].m_bodyHandle, targetBodyHandles[hitIndex]);
        }

        AZStd::array<ShapeOverlapHit, targetCount> completeHits;
        const QueryResult completeResult = system.CollideShape(worldHandle, request, completeHits);
        EXPECT_EQ(completeResult.m_hitCount, targetCount);
        EXPECT_TRUE(completeResult.IsComplete());
        for (size_t hitIndex = 0; hitIndex < completeHits.size(); ++hitIndex)
        {
            EXPECT_EQ(completeHits[hitIndex].m_bodyHandle, targetBodyHandles[hitIndex]);
        }

        AZStd::array<OverlapHit, 8> truncatedIdentityHits;
        const QueryResult truncatedIdentityResult =
            system.OverlapShape(worldHandle, request, truncatedIdentityHits);
        EXPECT_EQ(truncatedIdentityResult.m_hitCount, truncatedIdentityHits.size());
        EXPECT_EQ(truncatedIdentityResult.m_requiredHitCount, targetCount);
        EXPECT_FALSE(truncatedIdentityResult.IsComplete());
        for (size_t hitIndex = 0; hitIndex < truncatedIdentityHits.size(); ++hitIndex)
        {
            EXPECT_EQ(truncatedIdentityHits[hitIndex].m_bodyHandle, targetBodyHandles[hitIndex]);
        }

        AZStd::array<OverlapHit, targetCount> completeIdentityHits;
        const QueryResult completeIdentityResult =
            system.OverlapShape(worldHandle, request, completeIdentityHits);
        EXPECT_EQ(completeIdentityResult.m_hitCount, targetCount);
        EXPECT_TRUE(completeIdentityResult.IsComplete());
        for (size_t hitIndex = 0; hitIndex < completeIdentityHits.size(); ++hitIndex)
        {
            EXPECT_EQ(completeIdentityHits[hitIndex].m_bodyHandle, targetBodyHandles[hitIndex]);
        }
    }

    TEST(SimulationTests, BroadPhaseQueriesExposeAllNativeGeometryModes)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(2.0f, 2.0f, 2.0f),
        };
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);
        bodyConfiguration.m_transform.m_position.m_x = 4.0;
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        WorldStatistics statistics;
        ASSERT_TRUE(system.GetWorldStatistics(worldHandle, statistics));
        EXPECT_EQ(statistics.m_bodyCount, 2);
        EXPECT_EQ(statistics.m_staticBodyCount, 2);
        EXPECT_EQ(statistics.m_shapeCount, 1);
        EXPECT_GT(statistics.m_shapeBytes, 0);
        EXPECT_GT(statistics.m_tempAllocatorCapacityBytes, 0);

        AZStd::array<BroadPhaseHit, 2> overlapHits;
        BroadPhaseOverlapRequest overlapRequest;
        overlapRequest.m_geometry = BroadPhaseAabb{
            .m_halfExtents = AZ::Vector3::CreateOne(),
        };
        QueryResult overlapResult = system.OverlapBroadPhase(worldHandle, overlapRequest, overlapHits);
        ASSERT_EQ(overlapResult.m_hitCount, 1);
        EXPECT_EQ(overlapHits[0].m_bodyHandle, firstBodyHandle);
        EXPECT_TRUE(system.OverlapBroadPhaseAny(worldHandle, overlapRequest));

        overlapRequest.m_geometry = BroadPhaseOrientedBox{
            .m_transform = {
                .m_position = {.m_x = 4.0},
                .m_rotation = AZ::Quaternion::CreateRotationZ(AZ::Constants::QuarterPi),
            },
            .m_halfExtents = AZ::Vector3(0.5f, 2.0f, 0.5f),
        };
        overlapResult = system.OverlapBroadPhase(worldHandle, overlapRequest, overlapHits);
        ASSERT_EQ(overlapResult.m_hitCount, 1);
        EXPECT_EQ(overlapHits[0].m_bodyHandle, secondBodyHandle);

        overlapRequest.m_geometry = BroadPhasePoint{.m_position = {.m_x = 4.0}};
        overlapResult = system.OverlapBroadPhase(worldHandle, overlapRequest, overlapHits);
        EXPECT_EQ(overlapResult.m_hitCount, 1);

        overlapRequest.m_geometry = BroadPhaseSphere{
            .m_center = {.m_x = 4.0},
            .m_radius = 0.5f,
        };
        overlapResult = system.OverlapBroadPhase(worldHandle, overlapRequest, overlapHits);
        EXPECT_EQ(overlapResult.m_hitCount, 1);

        overlapRequest.m_geometry = BroadPhaseAabb{
            .m_center = {.m_x = 2.0},
            .m_halfExtents = AZ::Vector3(4.0f, 2.0f, 2.0f),
        };
        overlapResult = system.OverlapBroadPhase(
            worldHandle,
            overlapRequest,
            AZStd::span<BroadPhaseHit>(overlapHits.data(), 1));
        EXPECT_EQ(overlapResult.m_hitCount, 1);
        EXPECT_EQ(overlapResult.m_requiredHitCount, 2);
        EXPECT_FALSE(overlapResult.IsComplete());

        BroadPhaseCastRequest castRequest;
        castRequest.m_geometry = BroadPhaseRay{
            .m_start = {.m_x = -4.0},
            .m_displacement = AZ::Vector3::CreateAxisX(10.0f),
        };
        BroadPhaseCastHit closestHit;
        ASSERT_TRUE(system.CastBroadPhaseClosest(worldHandle, castRequest, closestHit));
        EXPECT_EQ(closestHit.m_bodyHandle, firstBodyHandle);
        EXPECT_NEAR(closestHit.m_fraction, 0.3f, 0.01f);

        AZStd::array<BroadPhaseCastHit, 2> castHits;
        QueryResult castResult = system.CastBroadPhaseAll(worldHandle, castRequest, castHits);
        ASSERT_EQ(castResult.m_hitCount, 2);
        EXPECT_TRUE(castResult.IsComplete());
        EXPECT_EQ(castHits[0].m_bodyHandle, firstBodyHandle);
        EXPECT_EQ(castHits[1].m_bodyHandle, secondBodyHandle);
        EXPECT_LT(castHits[0].m_fraction, castHits[1].m_fraction);

        castRequest.m_geometry = BroadPhaseAabbCast{
            .m_start = {
                .m_center = {.m_x = -4.0},
                .m_halfExtents = AZ::Vector3(0.25f),
            },
            .m_displacement = AZ::Vector3::CreateAxisX(10.0f),
        };
        ASSERT_TRUE(system.CastBroadPhaseClosest(worldHandle, castRequest, closestHit));
        EXPECT_EQ(closestHit.m_bodyHandle, firstBodyHandle);
        castResult = system.CastBroadPhaseAll(worldHandle, castRequest, castHits);
        EXPECT_EQ(castResult.m_hitCount, 2);

        BroadPhaseAabb bounds;
        ASSERT_TRUE(system.GetBroadPhaseBounds(worldHandle, bounds));
        EXPECT_NEAR(bounds.m_center.m_x, 2.0, 0.01);
        EXPECT_GE(bounds.m_halfExtents.GetX(), 3.0f);

        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, GeometryQueriesReturnSupportingFacesAndWorldTrianglesWithoutAllocatingOutputs)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_origin = {
            .m_x = 1'000'000'000.0,
            .m_y = -1'000'000'000.0,
            .m_z = 500'000'000.0,
        };
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const MaterialHandle materialHandle = system.CreateMaterial({});
        ASSERT_TRUE(materialHandle);

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(2.0f),
        };
        shapeConfiguration.m_materials = {materialHandle};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        DecoratedShapeConfiguration scaledShapeConfiguration;
        scaledShapeConfiguration.m_geometry = ScaledShapeConfiguration{
            .m_shapeHandle = shapeHandle,
            .m_scale = AZ::Vector3(2.0f),
        };
        const ShapeHandle scaledShapeHandle = system.CreateShape(
            worldHandle,
            scaledShapeConfiguration);
        ASSERT_TRUE(scaledShapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = scaledShapeHandle;
        bodyConfiguration.m_transform.m_position = {
            .m_x = systemConfiguration.m_defaultWorld.m_origin.m_x + 2.0,
            .m_y = systemConfiguration.m_defaultWorld.m_origin.m_y + 3.0,
            .m_z = systemConfiguration.m_defaultWorld.m_origin.m_z + 4.0,
        };
        bodyConfiguration.m_transform.m_rotation = AZ::Quaternion::CreateRotationX(0.35f);
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        SupportingFaceRequest faceRequest;
        faceRequest.m_bodyHandle = bodyHandle;
        AZStd::array<WorldPosition, 2> truncatedVertices;
        const QueryResult truncatedFaceResult = system.GetSupportingFace(
            worldHandle,
            faceRequest,
            truncatedVertices);
        EXPECT_EQ(truncatedFaceResult.m_hitCount, 2);
        EXPECT_EQ(truncatedFaceResult.m_requiredHitCount, 4);
        EXPECT_FALSE(truncatedFaceResult.IsComplete());
        EXPECT_LT(
            truncatedVertices[0].m_z,
            bodyConfiguration.m_transform.m_position.m_z);

        faceRequest.m_subShapeId = SubShapeId(0);
        EXPECT_EQ(system.GetSupportingFace(worldHandle, faceRequest, truncatedVertices).m_hitCount, 0);

        TriangleCollectionRequest triangleRequest;
        triangleRequest.m_bodyHandle = bodyHandle;
        triangleRequest.m_bounds = {
            .m_center = bodyConfiguration.m_transform.m_position,
            .m_halfExtents = AZ::Vector3(3.0f),
        };
        AZStd::array<TransformedTriangle, 1> triangles;
        const QueryResult triangleResult = system.CollectTriangles(
            worldHandle,
            triangleRequest,
            triangles);
        EXPECT_EQ(triangleResult.m_hitCount, 1);
        EXPECT_GT(triangleResult.m_requiredHitCount, 1);
        EXPECT_FALSE(triangleResult.IsComplete());
        EXPECT_EQ(triangles[0].m_materialHandle, materialHandle);
        EXPECT_GT(triangles[0].m_firstVertex.m_x, 999'999'990.0);

        ShapeCollectionRequest shapeCollectionRequest;
        shapeCollectionRequest.m_bounds = triangleRequest.m_bounds;
        AZStd::array<TransformedShape, 1> transformedShapes;
        const QueryResult shapeCollectionResult = system.CollectShapesInBounds(
            worldHandle,
            shapeCollectionRequest,
            transformedShapes);
        ASSERT_TRUE(shapeCollectionResult.IsComplete());
        ASSERT_EQ(shapeCollectionResult.m_hitCount, 1);
        ASSERT_TRUE(transformedShapes[0]);
        EXPECT_EQ(transformedShapes[0].GetBodyHandle(), bodyHandle);
        EXPECT_EQ(transformedShapes[0].GetMaterialHandle(), materialHandle);
        EXPECT_EQ(transformedShapes[0].GetShapeHandle(), scaledShapeHandle);
        EXPECT_TRUE(transformedShapes[0].GetScale().IsClose(AZ::Vector3(2.0f)));
        EXPECT_NEAR(
            transformedShapes[0].GetTransform().m_position.m_x,
            bodyConfiguration.m_transform.m_position.m_x,
            1.0e-5);
        EXPECT_TRUE(transformedShapes[0].GetTransform().m_rotation.IsClose(
            bodyConfiguration.m_transform.m_rotation));
        EXPECT_NEAR(
            transformedShapes[0].GetBounds().m_halfExtents.GetX(),
            2.0f,
            1.0e-5f);

        TransformedShape copiedShape = transformedShapes[0];
        TransformedShape retainedShape = AZStd::move(copiedShape);
        ASSERT_TRUE(retainedShape);

        TransformedShapeRaycastRequest transformedRaycastRequest;
        transformedRaycastRequest.m_start = bodyConfiguration.m_transform.m_position;
        transformedRaycastRequest.m_start.m_x -= 5.0;
        transformedRaycastRequest.m_displacement = AZ::Vector3::CreateAxisX(10.0f);
        RaycastHit transformedRaycastHit;
        ASSERT_TRUE(system.RaycastTransformedShapeClosest(
            worldHandle,
            retainedShape,
            transformedRaycastRequest,
            transformedRaycastHit));
        EXPECT_EQ(transformedRaycastHit.m_bodyHandle, bodyHandle);
        EXPECT_EQ(transformedRaycastHit.m_materialHandle, materialHandle);
        EXPECT_EQ(transformedRaycastHit.m_shapeHandle, scaledShapeHandle);
        EXPECT_NEAR(transformedRaycastHit.m_fraction, 0.3f, 1.0e-5f);

        AZStd::array<RaycastHit, 1> transformedRaycastHits;
        const QueryResult transformedRaycastResult = system.RaycastTransformedShapeAll(
            worldHandle,
            retainedShape,
            transformedRaycastRequest,
            transformedRaycastHits);
        EXPECT_TRUE(transformedRaycastResult.IsComplete());
        EXPECT_EQ(transformedRaycastResult.m_hitCount, 1);

        AZStd::array<OverlapHit, 1> transformedPointHits;
        const QueryResult transformedPointResult = system.CollideTransformedShapePoint(
            worldHandle,
            retainedShape,
            bodyConfiguration.m_transform.m_position,
            nullptr,
            transformedPointHits);
        EXPECT_TRUE(transformedPointResult.IsComplete());
        EXPECT_EQ(transformedPointResult.m_hitCount, 1);
        EXPECT_TRUE(system.CollideTransformedShapePointAny(
            worldHandle,
            retainedShape,
            bodyConfiguration.m_transform.m_position));

        AZStd::array<TransformedShape, 1> childShapes;
        const QueryResult childResult = system.CollectTransformedShapeChildren(
            worldHandle,
            retainedShape,
            triangleRequest.m_bounds,
            nullptr,
            childShapes);
        EXPECT_TRUE(childResult.IsComplete());
        EXPECT_EQ(childResult.m_hitCount, 1);
        EXPECT_EQ(childShapes[0].GetSubShapeId(), retainedShape.GetSubShapeId());

        const QueryResult transformedTriangleResult = system.CollectTransformedShapeTriangles(
            worldHandle,
            retainedShape,
            triangleRequest.m_bounds,
            triangles);
        EXPECT_EQ(transformedTriangleResult.m_hitCount, 1);
        EXPECT_EQ(transformedTriangleResult.m_requiredHitCount, 12);

        AZ::Vector3 surfaceNormal;
        ASSERT_TRUE(system.GetTransformedShapeSurfaceNormal(
            worldHandle,
            retainedShape,
            transformedRaycastHit.m_subShapeId,
            transformedRaycastHit.m_position,
            surfaceNormal));
        EXPECT_TRUE(surfaceNormal.IsClose(-AZ::Vector3::CreateAxisX()));

        AZStd::array<WorldPosition, 4> transformedFaceVertices;
        const QueryResult transformedFaceResult = system.GetTransformedShapeSupportingFace(
            worldHandle,
            retainedShape,
            transformedRaycastHit.m_subShapeId,
            AZ::Vector3::CreateAxisX(),
            transformedFaceVertices);
        EXPECT_TRUE(transformedFaceResult.IsComplete());
        EXPECT_EQ(transformedFaceResult.m_hitCount, 4);

        WorldTransform secondShapeTransform = bodyConfiguration.m_transform;
        secondShapeTransform.m_position.m_x += 2.5;
        TransformedShape secondRetainedShape;
        ASSERT_TRUE(system.RetainShape(
            worldHandle,
            shapeHandle,
            secondShapeTransform,
            1.0f,
            secondRetainedShape));

        AZStd::array<TransformedShapeCollisionHit, 8> pairHits;
        AZStd::array<WorldPosition, 8 * MaximumSupportingFaceVertexCount> firstFaceVertices;
        AZStd::array<WorldPosition, 8 * MaximumSupportingFaceVertexCount> secondFaceVertices;
        TransformedShapeCollisionRequest collisionRequest;
        collisionRequest.m_faceCollectionMode = FaceCollectionMode::Collect;
        const QueryResult pairResult = system.CollideTransformedShapes(
            worldHandle,
            retainedShape,
            secondRetainedShape,
            collisionRequest,
            pairHits,
            {
                .m_queryVertices = firstFaceVertices,
                .m_targetVertices = secondFaceVertices,
            });
        ASSERT_GT(pairResult.m_hitCount, 0);
        EXPECT_TRUE(pairResult.IsComplete());
        EXPECT_EQ(pairHits.front().m_firstBodyHandle, bodyHandle);
        EXPECT_EQ(pairHits.front().m_firstMaterialHandle, materialHandle);
        EXPECT_EQ(pairHits.front().m_firstShapeHandle, scaledShapeHandle);
        EXPECT_EQ(pairHits.front().m_secondShapeHandle, shapeHandle);
        EXPECT_GT(pairHits.front().m_firstFaceVertexCount, 0);
        EXPECT_GT(pairHits.front().m_secondFaceVertexCount, 0);

        AZStd::atomic_bool beginConcurrentQueries{false};
        AZStd::atomic_bool firstConcurrentQuerySucceeded{true};
        AZStd::atomic_bool secondConcurrentQuerySucceeded{true};
        AZStd::atomic_bool firstFloatEnvironmentRestored{false};
        AZStd::atomic_bool secondFloatEnvironmentRestored{false};
        AZStd::array<TransformedShapeCollisionHit, 1> firstConcurrentHits;
        AZStd::array<TransformedShapeCollisionHit, 1> secondConcurrentHits;
        const auto runConcurrentQueries =
            [&](const int roundingMode,
                AZStd::atomic_bool& succeeded,
                AZStd::atomic_bool& restored,
                AZStd::array<TransformedShapeCollisionHit, 1>& concurrentHits)
            {
                std::fesetround(roundingMode);
                while (!beginConcurrentQueries.load(AZStd::memory_order_acquire))
                {
                    AZStd::this_thread::yield();
                }

                for (AZ::u32 queryIndex = 0; queryIndex < 256; ++queryIndex)
                {
                    const QueryResult result = system.CollideTransformedShapes(
                        worldHandle,
                        retainedShape,
                        secondRetainedShape,
                        collisionRequest,
                        concurrentHits,
                        {});
                    if (result.m_hitCount == 0)
                    {
                        succeeded.store(false, AZStd::memory_order_release);
                        break;
                    }
                }
                restored.store(
                    std::fegetround() == roundingMode,
                    AZStd::memory_order_release);
            };
        AZStd::thread firstQueryThread(
            runConcurrentQueries,
            FE_DOWNWARD,
            AZStd::ref(firstConcurrentQuerySucceeded),
            AZStd::ref(firstFloatEnvironmentRestored),
            AZStd::ref(firstConcurrentHits));
        AZStd::thread secondQueryThread(
            runConcurrentQueries,
            FE_UPWARD,
            AZStd::ref(secondConcurrentQuerySucceeded),
            AZStd::ref(secondFloatEnvironmentRestored),
            AZStd::ref(secondConcurrentHits));
        beginConcurrentQueries.store(true, AZStd::memory_order_release);
        firstQueryThread.join();
        secondQueryThread.join();

        EXPECT_TRUE(firstConcurrentQuerySucceeded.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(secondConcurrentQuerySucceeded.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(firstFloatEnvironmentRestored.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(secondFloatEnvironmentRestored.load(AZStd::memory_order_acquire));
        EXPECT_EQ(
            firstConcurrentHits.front().m_firstSubShapeId,
            secondConcurrentHits.front().m_firstSubShapeId);
        EXPECT_EQ(
            firstConcurrentHits.front().m_secondSubShapeId,
            secondConcurrentHits.front().m_secondSubShapeId);
        EXPECT_FLOAT_EQ(
            firstConcurrentHits.front().m_penetrationDepth,
            secondConcurrentHits.front().m_penetrationDepth);

        const QueryResult placedPairResult = system.CollideTransformedShapes(
            worldHandle,
            ShapePlacement{
                .m_shapeHandle = scaledShapeHandle,
                .m_transform = bodyConfiguration.m_transform,
            },
            ShapePlacement{
                .m_shapeHandle = shapeHandle,
                .m_transform = secondShapeTransform,
            },
            collisionRequest,
            pairHits,
            {});
        EXPECT_EQ(placedPairResult.m_hitCount, pairResult.m_hitCount);

        ShapeConfiguration recyclableShapeConfiguration;
        recyclableShapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle staleShapeHandle = system.CreateShape(
            worldHandle,
            recyclableShapeConfiguration);
        ASSERT_TRUE(staleShapeHandle);
        ASSERT_TRUE(system.DestroyShape(worldHandle, staleShapeHandle));
        const ShapeHandle replacementShapeHandle = system.CreateShape(
            worldHandle,
            recyclableShapeConfiguration);
        ASSERT_TRUE(replacementShapeHandle);
        ASSERT_NE(staleShapeHandle, replacementShapeHandle);
        pairHits.front().m_penetrationDepth = 123.0f;
        const QueryResult stalePlacementResult = system.CollideTransformedShapes(
            worldHandle,
            ShapePlacement{
                .m_shapeHandle = staleShapeHandle,
                .m_transform = bodyConfiguration.m_transform,
            },
            ShapePlacement{
                .m_shapeHandle = shapeHandle,
                .m_transform = secondShapeTransform,
            },
            collisionRequest,
            pairHits,
            {});
        EXPECT_EQ(stalePlacementResult.m_hitCount, 0);
        EXPECT_FLOAT_EQ(pairHits.front().m_penetrationDepth, 123.0f);
        EXPECT_TRUE(system.DestroyShape(worldHandle, replacementShapeHandle));

        secondShapeTransform.m_position.m_x += 4.5;
        ASSERT_TRUE(system.RetainShape(
            worldHandle,
            shapeHandle,
            secondShapeTransform,
            1.0f,
            secondRetainedShape));
        AZStd::array<TransformedShapeCastHit, 4> pairCastHits;
        TransformedShapeCastRequest pairCastRequest;
        pairCastRequest.m_displacement = AZ::Vector3::CreateAxisX(10.0f);
        const QueryResult pairCastResult = system.CastTransformedShape(
            worldHandle,
            retainedShape,
            secondRetainedShape,
            pairCastRequest,
            pairCastHits,
            {});
        ASSERT_GT(pairCastResult.m_hitCount, 0);
        EXPECT_TRUE(pairCastResult.IsComplete());
        EXPECT_EQ(pairCastHits.front().m_collision.m_firstShapeHandle, scaledShapeHandle);
        EXPECT_EQ(pairCastHits.front().m_collision.m_secondShapeHandle, shapeHandle);
        EXPECT_NEAR(pairCastHits.front().m_fraction, 0.4f, 1.0e-4f);

        const QueryResult placedCastResult = system.CastTransformedShape(
            worldHandle,
            ShapePlacement{
                .m_shapeHandle = scaledShapeHandle,
                .m_transform = bodyConfiguration.m_transform,
            },
            ShapePlacement{
                .m_shapeHandle = shapeHandle,
                .m_transform = secondShapeTransform,
            },
            pairCastRequest,
            pairCastHits,
            {});
        EXPECT_EQ(placedCastResult.m_hitCount, pairCastResult.m_hitCount);

        EXPECT_EQ(
            system.CollideTransformedShapes(
                worldHandle,
                ShapePlacement{
                    .m_shapeHandle = ShapeHandle::Invalid,
                },
                ShapePlacement{
                    .m_shapeHandle = shapeHandle,
                    .m_transform = secondShapeTransform,
                },
                collisionRequest,
                pairHits,
                {}).m_hitCount,
            0);

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.RaycastTransformedShapeClosest(
            worldHandle,
            retainedShape,
            transformedRaycastRequest,
            transformedRaycastHit));

        const WorldHandle secondWorldHandle = system.CreateWorld({});
        ASSERT_TRUE(secondWorldHandle);
        EXPECT_FALSE(system.RaycastTransformedShapeClosest(
            secondWorldHandle,
            retainedShape,
            transformedRaycastRequest,
            transformedRaycastHit));
        EXPECT_TRUE(system.DestroyWorld(secondWorldHandle));

        Runtime otherSystem(
            CreateSerialSystemConfiguration(),
            nullptr,
            SystemRegistration::Isolated);
        ASSERT_TRUE(otherSystem);
        EXPECT_FALSE(otherSystem.RaycastTransformedShapeClosest(
            otherSystem.GetDefaultWorldHandle(),
            retainedShape,
            transformedRaycastRequest,
            transformedRaycastHit));

        EXPECT_FALSE(system.DestroyShape(worldHandle, scaledShapeHandle));
        EXPECT_FALSE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_FALSE(system.DestroyWorld(worldHandle));
        secondRetainedShape = {};
        childShapes[0] = {};
        retainedShape = {};
        transformedShapes[0] = {};
        EXPECT_TRUE(system.DestroyShape(worldHandle, scaledShapeHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, DiagnosticDrawingBatchesGeometryFiltersBodiesAndRestoresWorldCoordinates)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        systemConfiguration.m_defaultWorld.m_origin = {
            .m_x = 1'000'000'000.0,
            .m_y = -1'000'000'000.0,
            .m_z = 500'000'000.0,
        };
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ConvexHullShapeConfiguration convexHull;
        convexHull.m_points = {
            AZ::Vector3(-1.0f, -1.0f, -1.0f),
            AZ::Vector3(-1.0f, 1.0f, 1.0f),
            AZ::Vector3(1.0f, -1.0f, 1.0f),
            AZ::Vector3(1.0f, 1.0f, -1.0f),
        };
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = convexHull;
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration firstBodyConfiguration;
        firstBodyConfiguration.m_shapeHandle = shapeHandle;
        firstBodyConfiguration.m_transform.m_position = systemConfiguration.m_defaultWorld.m_origin;
        firstBodyConfiguration.m_transform.m_position.m_x += 1.0;
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, firstBodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);

        BodyConfiguration secondBodyConfiguration;
        secondBodyConfiguration.m_shapeHandle = shapeHandle;
        secondBodyConfiguration.m_transform.m_position = systemConfiguration.m_defaultWorld.m_origin;
        secondBodyConfiguration.m_transform.m_position.m_x += 3.0;
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, secondBodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        FixedConstraintConfiguration fixed;
        fixed.m_autoDetectPoint = true;
        fixed.m_space = ConstraintSpace::World;
        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = firstBodyHandle;
        constraintConfiguration.m_secondBodyHandle = secondBodyHandle;
        constraintConfiguration.m_geometry = fixed;
        const ConstraintHandle constraintHandle = system.CreateConstraint(
            worldHandle,
            constraintConfiguration);
        ASSERT_TRUE(constraintHandle);

        DebugDrawSettings settings;
        settings.m_cameraPosition = firstBodyConfiguration.m_transform.m_position;
        RecordingDebugRenderer allBodiesRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            settings,
            allBodiesRenderer));
        EXPECT_EQ(allBodiesRenderer.m_geometryCount, 2);
        EXPECT_GT(allBodiesRenderer.m_vertexCount, 0);
        EXPECT_EQ(allBodiesRenderer.m_vertexCount % 3, 0);
        EXPECT_NEAR(
            allBodiesRenderer.m_minimumTranslationX,
            firstBodyConfiguration.m_transform.m_position.m_x,
            1.0e-5);
        EXPECT_NEAR(
            allBodiesRenderer.m_maximumTranslationX,
            secondBodyConfiguration.m_transform.m_position.m_x,
            1.0e-5);

        settings.m_flags = DebugDrawFlags::Shapes
            | DebugDrawFlags::ConvexHullFaceOutlines;
        RecordingDebugRenderer outlinedRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            settings,
            outlinedRenderer));
        EXPECT_GT(outlinedRenderer.m_lineCount, 0);
        settings.m_flags = DebugDrawFlags::Shapes;

        SelectedDebugFilter filter;
        filter.m_selectedBodyHandle = firstBodyHandle;
        RecordingDebugRenderer filteredRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            settings,
            filteredRenderer,
            &filter));
        EXPECT_GE(filter.m_callCount, 2);
        EXPECT_EQ(filteredRenderer.m_geometryCount, 1);
        EXPECT_EQ(filteredRenderer.m_lineCount, 0);
        EXPECT_NEAR(
            filteredRenderer.m_minimumTranslationX,
            firstBodyConfiguration.m_transform.m_position.m_x,
            1.0e-5);

        settings.m_flags = DebugDrawFlags::Constraints
            | DebugDrawFlags::ConstraintLimits
            | DebugDrawFlags::ConstraintReferenceFrames;
        RecordingDebugRenderer constraintRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            settings,
            constraintRenderer));
        EXPECT_GT(constraintRenderer.m_lineCount, 0);

        DebugCaptureConfiguration captureConfiguration;
        captureConfiguration.m_flags = DebugCaptureFlags::Constraints
            | DebugCaptureFlags::ConstraintLimits
            | DebugCaptureFlags::ConstraintReferenceFrames;
        ASSERT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        DebugCaptureStatistics captureStatistics;
        ASSERT_TRUE(system.GetDebugCaptureStatistics(worldHandle, captureStatistics));
        EXPECT_GT(captureStatistics.m_lineCount, 0);
        captureConfiguration.m_flags = DebugCaptureFlags::None;
        ASSERT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));

        settings.m_shapeColor = DebugShapeColor::None;
        RecordingDebugRenderer invalidRenderer;
        EXPECT_FALSE(system.DrawDebug(
            worldHandle,
            settings,
            invalidRenderer));
        EXPECT_EQ(invalidRenderer.m_geometryCount, 0);
        EXPECT_EQ(invalidRenderer.m_lineCount, 0);

        EXPECT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, NativeShapeDetailFlagsAreScopedToEachDiagnosticDraw)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        MeshShapeConfiguration mesh;
        mesh.m_vertices = {
            AZ::Vector3(-1.0f, -1.0f, 0.0f),
            AZ::Vector3(1.0f, -1.0f, 0.0f),
            AZ::Vector3(0.0f, 1.0f, 0.0f),
        };
        mesh.m_triangles = {MeshTriangle{
            .m_firstVertex = 0,
            .m_secondVertex = 1,
            .m_thirdVertex = 2,
        }};
        ShapeConfiguration meshShapeConfiguration;
        meshShapeConfiguration.m_geometry = mesh;
        const ShapeHandle meshShapeHandle = system.CreateShape(
            worldHandle,
            meshShapeConfiguration);
        ASSERT_TRUE(meshShapeHandle);

        HeightfieldShapeConfiguration heightfield;
        heightfield.m_sampleCount = 4;
        heightfield.m_heights.resize(16, 0.0f);
        ShapeConfiguration heightfieldShapeConfiguration;
        heightfieldShapeConfiguration.m_geometry = heightfield;
        const ShapeHandle heightfieldShapeHandle = system.CreateShape(
            worldHandle,
            heightfieldShapeConfiguration);
        ASSERT_TRUE(heightfieldShapeHandle);

        BodyConfiguration meshBodyConfiguration;
        meshBodyConfiguration.m_shapeHandle = meshShapeHandle;
        meshBodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle meshBodyHandle = system.CreateBody(
            worldHandle,
            meshBodyConfiguration);
        ASSERT_TRUE(meshBodyHandle);

        BodyConfiguration heightfieldBodyConfiguration;
        heightfieldBodyConfiguration.m_shapeHandle = heightfieldShapeHandle;
        heightfieldBodyConfiguration.m_motionType = MotionType::Static;
        heightfieldBodyConfiguration.m_transform.m_position.m_x = 4.0;
        const BodyHandle heightfieldBodyHandle = system.CreateBody(
            worldHandle,
            heightfieldBodyConfiguration);
        ASSERT_TRUE(heightfieldBodyHandle);

        DebugDrawSettings settings;
        settings.m_shapeColor = DebugShapeColor::Material;
        RecordingDebugRenderer baselineRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            settings,
            baselineRenderer));

        settings.m_flags = DebugDrawFlags::Shapes
            | DebugDrawFlags::MeshTriangleGroups;
        RecordingDebugRenderer groupedRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            settings,
            groupedRenderer));
        EXPECT_NE(groupedRenderer.m_vertexColorSum, baselineRenderer.m_vertexColorSum);

        settings.m_flags = DebugDrawFlags::Shapes
            | DebugDrawFlags::MeshTriangleOutlines;
        RecordingDebugRenderer meshOutlineRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            settings,
            meshOutlineRenderer));
        EXPECT_GT(meshOutlineRenderer.m_lineCount, baselineRenderer.m_lineCount);

        settings.m_flags = DebugDrawFlags::Shapes
            | DebugDrawFlags::HeightfieldTriangleOutlines;
        RecordingDebugRenderer heightfieldOutlineRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            settings,
            heightfieldOutlineRenderer));
        EXPECT_GT(heightfieldOutlineRenderer.m_lineCount, baselineRenderer.m_lineCount);

        settings.m_flags = DebugDrawFlags::Shapes;
        RecordingDebugRenderer restoredRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            settings,
            restoredRenderer));
        EXPECT_EQ(restoredRenderer.m_lineCount, baselineRenderer.m_lineCount);
        EXPECT_EQ(restoredRenderer.m_vertexColorSum, baselineRenderer.m_vertexColorSum);

        EXPECT_TRUE(system.DestroyBody(worldHandle, heightfieldBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, meshBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, heightfieldShapeHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, meshShapeHandle));
    }

    TEST(SimulationTests, SimulationDebugCaptureIsBoundedReplaysWorldCoordinatesAndCanBeDisabled)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_origin = {
            .m_x = 1'000'000'000.0,
            .m_y = -1'000'000'000.0,
            .m_z = 500'000'000.0,
        };
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration floorShapeConfiguration;
        floorShapeConfiguration.m_geometry = BoxShapeConfiguration{
            .m_dimensions = AZ::Vector3(20.0f, 20.0f, 1.0f),
        };
        const ShapeHandle floorShapeHandle = system.CreateShape(
            worldHandle,
            floorShapeConfiguration);
        ASSERT_TRUE(floorShapeHandle);

        ShapeConfiguration sphereShapeConfiguration;
        sphereShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle sphereShapeHandle = system.CreateShape(
            worldHandle,
            sphereShapeConfiguration);
        ASSERT_TRUE(sphereShapeHandle);

        BodyConfiguration floorBodyConfiguration;
        floorBodyConfiguration.m_shapeHandle = floorShapeHandle;
        floorBodyConfiguration.m_transform.m_position = systemConfiguration.m_defaultWorld.m_origin;
        floorBodyConfiguration.m_transform.m_position.m_z -= 0.5;
        floorBodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        floorBodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle floorBodyHandle = system.CreateBody(
            worldHandle,
            floorBodyConfiguration);
        ASSERT_TRUE(floorBodyHandle);

        BodyConfiguration sphereBodyConfiguration;
        sphereBodyConfiguration.m_shapeHandle = sphereShapeHandle;
        sphereBodyConfiguration.m_transform.m_position = systemConfiguration.m_defaultWorld.m_origin;
        sphereBodyConfiguration.m_transform.m_position.m_z += 0.45;
        const BodyHandle sphereBodyHandle = system.CreateBody(
            worldHandle,
            sphereBodyConfiguration);
        ASSERT_TRUE(sphereBodyHandle);

        DebugCaptureConfiguration captureConfiguration;
        captureConfiguration.m_flags = DebugCaptureFlags::ContactManifolds;
        captureConfiguration.m_lineCapacity = 1;
        ASSERT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));

        DebugCaptureStatistics statistics;
        ASSERT_TRUE(system.GetDebugCaptureStatistics(worldHandle, statistics));
        EXPECT_EQ(statistics.m_lineCount, 0);
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.GetDebugCaptureStatistics(worldHandle, statistics));
        EXPECT_EQ(statistics.m_lineCount, 1);
        EXPECT_GT(statistics.m_droppedLineCount, 0);

        DebugDrawSettings drawSettings;
        drawSettings.m_flags = DebugDrawFlags::CapturedSimulation;
        RecordingDebugRenderer renderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            drawSettings,
            renderer));
        EXPECT_EQ(renderer.m_geometryCount, statistics.m_geometryCount);
        EXPECT_EQ(renderer.m_lineCount, statistics.m_lineCount);
        EXPECT_EQ(renderer.m_textCount, statistics.m_textCount);
        EXPECT_EQ(renderer.m_triangleCount, statistics.m_triangleCount);
        EXPECT_GT(renderer.m_minimumLinePositionX, systemConfiguration.m_defaultWorld.m_origin.m_x - 20.0);
        EXPECT_LT(renderer.m_maximumLinePositionX, systemConfiguration.m_defaultWorld.m_origin.m_x + 20.0);

        DebugCaptureConfiguration invalidConfiguration = captureConfiguration;
        invalidConfiguration.m_flags = static_cast<DebugCaptureFlags>(1u << 31);
        EXPECT_FALSE(system.ConfigureDebugCapture(worldHandle, invalidConfiguration));

        captureConfiguration.m_flags = DebugCaptureFlags::None;
        ASSERT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));
        ASSERT_TRUE(system.GetDebugCaptureStatistics(worldHandle, statistics));
        EXPECT_EQ(statistics.m_geometryCount, 0);
        EXPECT_EQ(statistics.m_lineCount, 0);
        EXPECT_EQ(statistics.m_textCount, 0);
        EXPECT_EQ(statistics.m_triangleCount, 0);

        RecordingDebugRenderer disabledRenderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            drawSettings,
            disabledRenderer));
        EXPECT_EQ(disabledRenderer.m_geometryCount, 0);
        EXPECT_EQ(disabledRenderer.m_lineCount, 0);
        EXPECT_EQ(disabledRenderer.m_textCount, 0);
        EXPECT_EQ(disabledRenderer.m_triangleCount, 0);

        EXPECT_TRUE(system.DestroyBody(worldHandle, sphereBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, floorBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, sphereShapeHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, floorShapeHandle));
    }

    TEST(SimulationTests, SimulationDebugCaptureCoversDirectCharacterAndBuoyancyOperations)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration sphereShapeConfiguration;
        sphereShapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle sphereShapeHandle = system.CreateShape(
            worldHandle,
            sphereShapeConfiguration);
        ASSERT_TRUE(sphereShapeHandle);

        VirtualCharacterConfiguration characterConfiguration;
        characterConfiguration.m_shapeHandle = sphereShapeHandle;
        characterConfiguration.m_supportingPlaneDistance = 0.0f;
        const VirtualCharacterHandle characterHandle = system.CreateVirtualCharacter(
            worldHandle,
            characterConfiguration);
        ASSERT_TRUE(characterHandle);

        DebugCaptureConfiguration captureConfiguration;
        captureConfiguration.m_flags = DebugCaptureFlags::VirtualCharacterSupportingVolumes;
        ASSERT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));

        VirtualCharacterUpdateConfiguration updateConfiguration;
        updateConfiguration.m_extended = false;
        ASSERT_TRUE(system.UpdateVirtualCharacter(
            worldHandle,
            characterHandle,
            1.0f / 60.0f,
            updateConfiguration));

        DebugCaptureStatistics characterStatistics;
        ASSERT_TRUE(system.GetDebugCaptureStatistics(worldHandle, characterStatistics));
        EXPECT_GT(characterStatistics.m_lineCount, 0);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = sphereShapeHandle;
        const BodyHandle bodyHandle = system.CreateBody(
            worldHandle,
            bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        captureConfiguration.m_flags = DebugCaptureFlags::SubmergedVolumes;
        ASSERT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));
        BuoyancyConfiguration buoyancyConfiguration;
        buoyancyConfiguration.m_surfacePosition.m_z = 1.0;
        ASSERT_TRUE(system.ApplyBuoyancyImpulse(
            worldHandle,
            bodyHandle,
            buoyancyConfiguration));

        DebugCaptureStatistics buoyancyStatistics;
        ASSERT_TRUE(system.GetDebugCaptureStatistics(worldHandle, buoyancyStatistics));
        EXPECT_GT(
            buoyancyStatistics.m_geometryCount
                + buoyancyStatistics.m_lineCount
                + buoyancyStatistics.m_textCount
                + buoyancyStatistics.m_triangleCount,
            0);

        DebugDrawSettings drawSettings;
        drawSettings.m_flags = DebugDrawFlags::CapturedSimulation;
        RecordingDebugRenderer renderer;
        ASSERT_TRUE(system.DrawDebug(
            worldHandle,
            drawSettings,
            renderer));
        EXPECT_EQ(renderer.m_geometryCount, buoyancyStatistics.m_geometryCount);
        EXPECT_EQ(renderer.m_lineCount, buoyancyStatistics.m_lineCount);
        EXPECT_EQ(renderer.m_textCount, buoyancyStatistics.m_textCount);
        EXPECT_EQ(renderer.m_triangleCount, buoyancyStatistics.m_triangleCount);

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyVirtualCharacter(worldHandle, characterHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, sphereShapeHandle));
    }

    TEST(SimulationTests, SimulationDebugCaptureIncludesBroadPhaseBoundsAndQueries)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        DebugCaptureConfiguration captureConfiguration;
        captureConfiguration.m_flags = DebugCaptureFlags::BroadPhaseBounds | DebugCaptureFlags::Queries;
        captureConfiguration.m_lineCapacity = 32;
        ASSERT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        DebugCaptureStatistics beforeQuery;
        ASSERT_TRUE(system.GetDebugCaptureStatistics(worldHandle, beforeQuery));
        EXPECT_EQ(beforeQuery.m_lineCount, 12);
        EXPECT_EQ(beforeQuery.m_droppedLineCount, 0);

        RaycastRequest request;
        request.m_start.m_z = 2.0;
        request.m_displacement = AZ::Vector3::CreateAxisZ(-4.0f);
        RaycastHit hit;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));

        DebugCaptureStatistics afterQuery;
        ASSERT_TRUE(system.GetDebugCaptureStatistics(worldHandle, afterQuery));
        EXPECT_EQ(afterQuery.m_lineCount, beforeQuery.m_lineCount + 2);
        EXPECT_EQ(afterQuery.m_droppedLineCount, 0);

        DebugDrawSettings drawSettings;
        drawSettings.m_flags = DebugDrawFlags::CapturedSimulation;
        RecordingDebugRenderer renderer;
        ASSERT_TRUE(system.DrawDebug(worldHandle, drawSettings, renderer));
        EXPECT_EQ(renderer.m_lineCount, afterQuery.m_lineCount);

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, SimulationDebugCaptureIsIsolatedAcrossConcurrentWorldSteps)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle captureWorldHandle = system.GetDefaultWorldHandle();
        const WorldHandle contactWorldHandle = system.CreateWorld(systemConfiguration.m_defaultWorld);
        ASSERT_TRUE(contactWorldHandle);
        const SphereOnFloor contactScene = CreateSphereOnFloor(system, contactWorldHandle);
        ASSERT_TRUE(contactScene.m_floorShapeHandle);
        ASSERT_TRUE(contactScene.m_sphereShapeHandle);
        ASSERT_TRUE(contactScene.m_floorBodyHandle);
        ASSERT_TRUE(contactScene.m_sphereBodyHandle);

        DebugCaptureConfiguration captureConfiguration;
        captureConfiguration.m_flags = DebugCaptureFlags::ContactManifolds;
        ASSERT_TRUE(system.ConfigureDebugCapture(captureWorldHandle, captureConfiguration));

        AZStd::atomic_bool stepsSucceeded = true;
        AZStd::thread captureThread(
            [&system, captureWorldHandle, &stepsSucceeded]()
            {
                for (AZ::u32 step = 0; step < 120; ++step)
                {
                    if (!system.StepWorld(captureWorldHandle, 1.0f / 60.0f))
                    {
                        stepsSucceeded.store(false);
                    }
                }
            });
        AZStd::thread contactThread(
            [&system, contactWorldHandle, &stepsSucceeded]()
            {
                for (AZ::u32 step = 0; step < 120; ++step)
                {
                    if (!system.StepWorld(contactWorldHandle, 1.0f / 60.0f))
                    {
                        stepsSucceeded.store(false);
                    }
                }
            });
        captureThread.join();
        contactThread.join();
        EXPECT_TRUE(stepsSucceeded.load());

        DebugCaptureStatistics statistics;
        ASSERT_TRUE(system.GetDebugCaptureStatistics(captureWorldHandle, statistics));
        EXPECT_EQ(statistics.m_geometryCount, 0);
        EXPECT_EQ(statistics.m_lineCount, 0);
        EXPECT_EQ(statistics.m_textCount, 0);
        EXPECT_EQ(statistics.m_triangleCount, 0);

        captureConfiguration.m_flags = DebugCaptureFlags::None;
        EXPECT_TRUE(system.ConfigureDebugCapture(captureWorldHandle, captureConfiguration));
        DestroySphereOnFloor(system, contactScene);
        EXPECT_TRUE(system.DestroyWorld(contactWorldHandle));
    }

    TEST(SimulationTests, AutomaticSimulationCaptureAccumulatesEveryCatchUpStep)
    {
        const auto captureLineCount = [](const AZ::u32 catchUpStepCount)
        {
            SystemConfiguration configuration = CreateSerialSystemConfiguration();
            configuration.m_defaultWorld.m_fixedTimeStep = 0.1f;
            configuration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
            configuration.m_defaultWorld.m_maximumCatchUpSteps = 2;
            Runtime system(configuration, nullptr);
            EXPECT_TRUE(system);

            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
            const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
            EXPECT_TRUE(shapeHandle);

            VirtualCharacterConfiguration characterConfiguration;
            characterConfiguration.m_shapeHandle = shapeHandle;
            characterConfiguration.m_supportingPlaneDistance = 0.0f;
            const VirtualCharacterHandle characterHandle = system.CreateVirtualCharacter(
                worldHandle,
                characterConfiguration);
            EXPECT_TRUE(characterHandle);

            VirtualCharacterUpdateConfiguration updateConfiguration;
            updateConfiguration.m_extended = false;
            EXPECT_TRUE(system.EnableVirtualCharacterAutoUpdate(
                worldHandle,
                characterHandle,
                updateConfiguration));

            DebugCaptureConfiguration captureConfiguration;
            captureConfiguration.m_flags = DebugCaptureFlags::VirtualCharacterSupportingVolumes;
            EXPECT_TRUE(system.ConfigureDebugCapture(worldHandle, captureConfiguration));

            const SimulationResult result = system.StepAutoSimulatedWorldsDetailed(
                configuration.m_defaultWorld.m_fixedTimeStep * catchUpStepCount);
            EXPECT_TRUE(result);
            EXPECT_EQ(result.m_stepCount, catchUpStepCount);

            DebugCaptureStatistics statistics;
            EXPECT_TRUE(system.GetDebugCaptureStatistics(worldHandle, statistics));

            EXPECT_TRUE(system.DestroyVirtualCharacter(worldHandle, characterHandle));
            EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
            return statistics.m_lineCount;
        };

        const AZ::u32 singleStepLineCount = captureLineCount(1);
        const AZ::u32 twoStepLineCount = captureLineCount(2);
        ASSERT_GT(singleStepLineCount, 0);
        EXPECT_EQ(twoStepLineCount, singleStepLineCount * 2);
    }

    TEST(SimulationTests, MutatesBodiesWithoutExposingNativeState)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ASSERT_TRUE(worldHandle);

        ShapeConfiguration sphereConfiguration;
        sphereConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle sphereShapeHandle = system.CreateShape(worldHandle, sphereConfiguration);
        ASSERT_TRUE(sphereShapeHandle);

        ShapeConfiguration boxConfiguration;
        boxConfiguration.m_geometry = BoxShapeConfiguration{};
        const ShapeHandle boxShapeHandle = system.CreateShape(worldHandle, boxConfiguration);
        ASSERT_TRUE(boxShapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = sphereShapeHandle;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        WorldTransform transform;
        EXPECT_TRUE(system.DeactivateBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.SetBodyTransformWhenChanged(
            worldHandle,
            bodyHandle,
            transform,
            true));
        BodyState bodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, bodyState));
        EXPECT_FALSE(bodyState.m_isActive);

        transform.m_position = {.m_x = 1.0, .m_y = 2.0, .m_z = 3.0};
        EXPECT_TRUE(system.SetBodyTransformWhenChanged(
            worldHandle,
            bodyHandle,
            transform,
            true));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, bodyState));
        EXPECT_TRUE(bodyState.m_isActive);
        EXPECT_EQ(bodyState.m_transform.m_position, transform.m_position);
        EXPECT_TRUE(bodyState.m_transform.m_rotation.IsClose(transform.m_rotation));

        EXPECT_TRUE(system.SetBodyTransform(worldHandle, bodyHandle, transform, false));
        const WorldPosition directPosition = {.m_x = 4.0, .m_y = 5.0, .m_z = 6.0};
        const AZ::Quaternion directRotation = AZ::Quaternion::CreateRotationZ(0.25f);
        EXPECT_TRUE(system.SetBodyPosition(
            worldHandle,
            bodyHandle,
            directPosition,
            false));
        EXPECT_TRUE(system.SetBodyRotation(
            worldHandle,
            bodyHandle,
            directRotation,
            false));

        WorldPosition returnedPosition;
        AZ::Quaternion returnedRotation;
        EXPECT_TRUE(system.GetBodyPosition(worldHandle, bodyHandle, returnedPosition));
        EXPECT_TRUE(system.GetBodyRotation(worldHandle, bodyHandle, returnedRotation));
        EXPECT_EQ(returnedPosition, directPosition);
        EXPECT_TRUE(returnedRotation.IsClose(directRotation));
        EXPECT_TRUE(system.SetBodyTransform(worldHandle, bodyHandle, transform, false));

        MotionType returnedMotionType = MotionType::None;
        ObjectLayer returnedObjectLayer = ObjectLayer::Invalid;
        CollisionGroupConfiguration returnedCollisionGroup;
        ShapeHandle returnedShapeHandle;
        ASSERT_TRUE(system.GetBodyMotionType(worldHandle, bodyHandle, returnedMotionType));
        ASSERT_TRUE(system.GetBodyObjectLayer(worldHandle, bodyHandle, returnedObjectLayer));
        ASSERT_TRUE(system.GetBodyCollisionGroup(worldHandle, bodyHandle, returnedCollisionGroup));
        ASSERT_TRUE(system.GetBodyShape(worldHandle, bodyHandle, returnedShapeHandle));
        EXPECT_EQ(returnedMotionType, MotionType::Dynamic);
        EXPECT_EQ(returnedObjectLayer, DefaultLayers::Moving);
        EXPECT_EQ(returnedCollisionGroup.m_filterHandle, bodyConfiguration.m_collisionGroup.m_filterHandle);
        EXPECT_EQ(returnedCollisionGroup.m_groupId, bodyConfiguration.m_collisionGroup.m_groupId);
        EXPECT_EQ(returnedCollisionGroup.m_subGroupId, bodyConfiguration.m_collisionGroup.m_subGroupId);
        EXPECT_EQ(returnedShapeHandle, sphereShapeHandle);

        BroadPhaseAabb bodyBounds;
        ASSERT_TRUE(system.GetBodyBounds(worldHandle, bodyHandle, bodyBounds));
        EXPECT_EQ(bodyBounds.m_center, transform.m_position);
        EXPECT_TRUE(bodyBounds.m_halfExtents.IsClose(AZ::Vector3(0.5f)));

        MaterialHandle bodyMaterialHandle;
        MaterialHandle shapeMaterialHandle;
        ASSERT_TRUE(system.GetBodyMaterial(
            worldHandle,
            bodyHandle,
            SubShapeId::Root,
            bodyMaterialHandle));
        ASSERT_TRUE(system.GetShapeMaterial(
            worldHandle,
            sphereShapeHandle,
            SubShapeId::Root,
            shapeMaterialHandle));
        EXPECT_EQ(bodyMaterialHandle, shapeMaterialHandle);

        const WorldPosition sphereSurfacePosition = {
            .m_x = transform.m_position.m_x + 0.5,
            .m_y = transform.m_position.m_y,
            .m_z = transform.m_position.m_z,
        };
        AZ::Vector3 bodySurfaceNormal;
        ASSERT_TRUE(system.GetBodySurfaceNormal(
            worldHandle,
            bodyHandle,
            SubShapeId::Root,
            sphereSurfacePosition,
            bodySurfaceNormal));
        EXPECT_TRUE(bodySurfaceNormal.IsClose(AZ::Vector3::CreateAxisX()));

        SubmergedVolumeResult bodySubmergedVolume;
        ASSERT_TRUE(system.GetBodySubmergedVolume(
            worldHandle,
            bodyHandle,
            transform.m_position,
            AZ::Vector3::CreateAxisZ(),
            bodySubmergedVolume));
        EXPECT_NEAR(bodySubmergedVolume.m_submergedVolume, 0.5f * bodySubmergedVolume.m_totalVolume, 1.0e-5f);
        EXPECT_NEAR(
            bodySubmergedVolume.m_centerOfBuoyancy.m_z,
            transform.m_position.m_z - 0.1875,
            1.0e-6);

        EXPECT_TRUE(system.SetBodyVelocities(
            worldHandle,
            bodyHandle,
            AZ::Vector3(1.0f, 2.0f, 3.0f),
            AZ::Vector3(0.0f, 0.0f, 1.0f)));
        EXPECT_TRUE(system.AddForce(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX()));
        EXPECT_TRUE(system.AddForceAtPosition(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisY(),
            transform.m_position));
        EXPECT_TRUE(system.AddTorque(worldHandle, bodyHandle, AZ::Vector3::CreateAxisZ()));
        EXPECT_TRUE(system.AddImpulse(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX()));
        EXPECT_TRUE(system.AddImpulseAtPosition(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisY(),
            transform.m_position));
        EXPECT_TRUE(system.AddAngularImpulse(worldHandle, bodyHandle, AZ::Vector3::CreateAxisZ()));
        EXPECT_TRUE(system.DeactivateBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.ActivateBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.ResetBodySleepTimer(worldHandle, bodyHandle));
        EXPECT_TRUE(
            system.SetBodyTransformAndVelocities(
                worldHandle,
                bodyHandle,
                transform,
                AZ::Vector3::CreateAxisX(),
                AZ::Vector3::CreateAxisZ()));
        EXPECT_TRUE(
            system.AddBodyVelocities(
                worldHandle,
                bodyHandle,
                AZ::Vector3::CreateAxisY(),
                AZ::Vector3::CreateAxisZ()));
        EXPECT_TRUE(system.SetBodyLinearVelocity(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisX(2.0f)));
        EXPECT_TRUE(system.SetBodyAngularVelocity(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisY()));
        EXPECT_TRUE(system.AddBodyLinearVelocity(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisZ()));
        EXPECT_TRUE(system.AddBodyVelocities(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisY(),
            AZ::Vector3::CreateAxisZ()));

        AZ::Vector3 linearVelocity;
        AZ::Vector3 angularVelocity;
        EXPECT_TRUE(system.GetBodyVelocities(
            worldHandle,
            bodyHandle,
            linearVelocity,
            angularVelocity));
        EXPECT_TRUE(linearVelocity.IsClose(AZ::Vector3(2.0f, 1.0f, 1.0f)));
        EXPECT_TRUE(angularVelocity.IsClose(AZ::Vector3(0.0f, 1.0f, 1.0f)));

        linearVelocity = AZ::Vector3::CreateZero();
        angularVelocity = AZ::Vector3::CreateZero();
        EXPECT_TRUE(system.GetBodyLinearVelocity(worldHandle, bodyHandle, linearVelocity));
        EXPECT_TRUE(system.GetBodyAngularVelocity(worldHandle, bodyHandle, angularVelocity));
        EXPECT_TRUE(linearVelocity.IsClose(AZ::Vector3(2.0f, 1.0f, 1.0f)));
        EXPECT_TRUE(angularVelocity.IsClose(AZ::Vector3(0.0f, 1.0f, 1.0f)));

        AZ::Vector3 pointVelocity;
        ASSERT_TRUE(system.GetBodyPointVelocity(
            worldHandle,
            bodyHandle,
            transform.m_position,
            pointVelocity));
        EXPECT_TRUE(pointVelocity.IsClose(AZ::Vector3(2.0f, 1.0f, 1.0f), 1.0e-5f));

        EXPECT_TRUE(system.SetBodyVelocities(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateZero(),
            AZ::Vector3::CreateZero()));
        EXPECT_TRUE(system.DeactivateBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.AddForce(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisX(),
            false));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, bodyState));
        EXPECT_FALSE(bodyState.m_isActive);
        EXPECT_TRUE(system.AddForce(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisX(),
            true));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, bodyState));
        EXPECT_TRUE(bodyState.m_isActive);

        EXPECT_TRUE(
            system.AddForceAndTorque(
                worldHandle,
                bodyHandle,
                AZ::Vector3::CreateAxisX(),
                AZ::Vector3::CreateAxisZ()));

        AZ::Vector3 accumulatedForce;
        AZ::Vector3 accumulatedTorque;
        ASSERT_TRUE(system.GetBodyAccumulatedForceAndTorque(
            worldHandle,
            bodyHandle,
            accumulatedForce,
            accumulatedTorque));
        EXPECT_FLOAT_EQ(accumulatedForce.GetX(), 3.0f);
        EXPECT_FLOAT_EQ(accumulatedForce.GetY(), 1.0f);
        EXPECT_FLOAT_EQ(accumulatedForce.GetZ(), 0.0f);
        EXPECT_FLOAT_EQ(accumulatedTorque.GetX(), 0.0f);
        EXPECT_FLOAT_EQ(accumulatedTorque.GetY(), 0.0f);
        EXPECT_FLOAT_EQ(accumulatedTorque.GetZ(), 2.0f);

        ASSERT_TRUE(system.ResetBodyAccumulatedForce(worldHandle, bodyHandle));
        ASSERT_TRUE(system.GetBodyAccumulatedForceAndTorque(
            worldHandle,
            bodyHandle,
            accumulatedForce,
            accumulatedTorque));
        EXPECT_TRUE(accumulatedForce.IsZero());
        EXPECT_FLOAT_EQ(accumulatedTorque.GetZ(), 2.0f);

        ASSERT_TRUE(system.ResetBodyAccumulatedTorque(worldHandle, bodyHandle));
        ASSERT_TRUE(system.GetBodyAccumulatedForceAndTorque(
            worldHandle,
            bodyHandle,
            accumulatedForce,
            accumulatedTorque));
        EXPECT_TRUE(accumulatedForce.IsZero());
        EXPECT_TRUE(accumulatedTorque.IsZero());

        ASSERT_TRUE(system.SetBodyVelocities(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisX(),
            AZ::Vector3::CreateAxisY()));
        ASSERT_TRUE(system.AddForceAndTorque(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisY(),
            AZ::Vector3::CreateAxisZ()));
        ASSERT_TRUE(system.ResetBodyMotion(worldHandle, bodyHandle));
        ASSERT_TRUE(system.GetBodyVelocities(
            worldHandle,
            bodyHandle,
            linearVelocity,
            angularVelocity));
        EXPECT_TRUE(linearVelocity.IsZero());
        EXPECT_TRUE(angularVelocity.IsZero());
        ASSERT_TRUE(system.GetBodyAccumulatedForceAndTorque(
            worldHandle,
            bodyHandle,
            accumulatedForce,
            accumulatedTorque));
        EXPECT_TRUE(accumulatedForce.IsZero());
        EXPECT_TRUE(accumulatedTorque.IsZero());

        WorldTransform invalidTransform;
        invalidTransform.m_position.m_x = AZStd::numeric_limits<double>::quiet_NaN();
        EXPECT_FALSE(system.SetBodyPosition(
            worldHandle,
            bodyHandle,
            invalidTransform.m_position,
            true));
        EXPECT_FALSE(system.SetBodyRotation(
            worldHandle,
            bodyHandle,
            AZ::Quaternion::CreateZero(),
            true));
        EXPECT_FALSE(system.SetBodyTransformWhenChanged(
            worldHandle,
            bodyHandle,
            invalidTransform,
            true));

        BuoyancyConfiguration buoyancy;
        buoyancy.m_surfacePosition = {.m_z = 4.0};
        EXPECT_TRUE(system.ApplyBuoyancyImpulse(worldHandle, bodyHandle, buoyancy));

        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_DOUBLE_EQ(state.m_transform.m_position.m_x, 1.0);
        EXPECT_DOUBLE_EQ(state.m_transform.m_position.m_y, 2.0);
        EXPECT_DOUBLE_EQ(state.m_transform.m_position.m_z, 3.0);

        EXPECT_TRUE(system.SetBodyShape(worldHandle, bodyHandle, boxShapeHandle, true, false));
        EXPECT_TRUE(system.DestroyShape(worldHandle, sphereShapeHandle));
        EXPECT_FALSE(system.DestroyShape(worldHandle, boxShapeHandle));

        const CollisionGroupConfiguration replacementCollisionGroup{
            .m_groupId = CollisionGroupId{17},
            .m_subGroupId = CollisionSubGroupId{3},
        };
        ASSERT_TRUE(system.SetBodyCollisionGroup(
            worldHandle,
            bodyHandle,
            replacementCollisionGroup,
            true));
        ASSERT_TRUE(system.GetBodyCollisionGroup(
            worldHandle,
            bodyHandle,
            returnedCollisionGroup));
        EXPECT_EQ(returnedCollisionGroup.m_groupId, replacementCollisionGroup.m_groupId);
        EXPECT_EQ(returnedCollisionGroup.m_subGroupId, replacementCollisionGroup.m_subGroupId);

        WorldTransform kinematicTarget = transform;
        kinematicTarget.m_position.m_x = 2.0;
        EXPECT_FALSE(system.MoveBodyKinematically(
            worldHandle,
            bodyHandle,
            kinematicTarget,
            1.0f / 60.0f));
        ASSERT_TRUE(system.SetBodyMotionType(worldHandle, bodyHandle, MotionType::Kinematic, false));
        ASSERT_TRUE(system.MoveBodyKinematically(
            worldHandle,
            bodyHandle,
            kinematicTarget,
            1.0f / 60.0f));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_NEAR(state.m_transform.m_position.m_x, kinematicTarget.m_position.m_x, 1.0e-6);

        EXPECT_TRUE(system.SetBodyMotionType(worldHandle, bodyHandle, MotionType::Static, false));
        EXPECT_TRUE(system.SetBodyObjectLayer(worldHandle, bodyHandle, DefaultLayers::NonMoving));
        EXPECT_TRUE(system.GetBodyMotionType(worldHandle, bodyHandle, returnedMotionType));
        EXPECT_TRUE(system.GetBodyObjectLayer(worldHandle, bodyHandle, returnedObjectLayer));
        EXPECT_EQ(returnedMotionType, MotionType::Static);
        EXPECT_EQ(returnedObjectLayer, DefaultLayers::NonMoving);
        EXPECT_FALSE(system.SetBodyLinearVelocity(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisX()));
        EXPECT_FALSE(system.AddForce(
            worldHandle,
            bodyHandle,
            AZ::Vector3::CreateAxisX()));

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_FALSE(system.GetBodyBounds(worldHandle, bodyHandle, bodyBounds));
        EXPECT_FALSE(system.GetBodyMaterial(
            worldHandle,
            bodyHandle,
            SubShapeId::Root,
            bodyMaterialHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, boxShapeHandle));
    }

    TEST(SimulationTests, ReusesBodyCapacityAcrossCreateDestroyCycles)
    {
        constexpr AZ::u32 bodyCount = 128;
        SystemConfiguration configuration = CreateSerialSystemConfiguration();
        configuration.m_defaultWorld.m_capacity.m_maxBodies = bodyCount + 1;
        configuration.m_defaultWorld.m_capacity.m_maxBodyPairs = bodyCount * 16;
        configuration.m_defaultWorld.m_capacity.m_maxContactConstraints = bodyCount * 16;
        Runtime system(configuration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        AZStd::vector<BodyHandle> bodyHandles;
        bodyHandles.reserve(bodyCount);
        constexpr AZ::u32 cycleCount = 16;
        for (AZ::u32 cycle = 0; cycle < cycleCount; ++cycle)
        {
            for (AZ::u32 bodyIndex = 0; bodyIndex < bodyCount; ++bodyIndex)
            {
                BodyConfiguration bodyConfiguration;
                bodyConfiguration.m_shapeHandle = shapeHandle;
                bodyConfiguration.m_transform.m_position.m_z = bodyIndex;
                const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
                ASSERT_TRUE(bodyHandle);
                bodyHandles.push_back(bodyHandle);
            }

            for (const BodyHandle bodyHandle : bodyHandles)
            {
                EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
            }
            bodyHandles.clear();
        }

        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, ConfiguredMassPropertiesControlImpulseResponse)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_linearDamping = 0.0f;
        bodyConfiguration.m_angularDamping = 0.0f;
        bodyConfiguration.m_massProperties = {
            .m_inertia = AZ::Matrix3x3::CreateDiagonal(AZ::Vector3(2.0f, 4.0f, 8.0f)),
            .m_mass = 4.0f,
            .m_mode = MassPropertiesMode::Provided,
        };
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        EXPECT_TRUE(system.AddImpulse(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX(8.0f)));
        EXPECT_TRUE(system.AddAngularImpulse(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX(4.0f)));

        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_NEAR(state.m_linearVelocity.GetX(), 2.0f, 1.0e-5f);
        EXPECT_NEAR(state.m_angularVelocity.GetX(), 2.0f, 1.0e-5f);

        BodyConfiguration currentConfiguration;
        ASSERT_TRUE(system.GetBodyConfiguration(worldHandle, bodyHandle, currentConfiguration));
        EXPECT_EQ(currentConfiguration.m_massProperties.m_mode, MassPropertiesMode::Provided);
        EXPECT_NEAR(currentConfiguration.m_massProperties.m_mass, 4.0f, 1.0e-5f);
        EXPECT_TRUE(currentConfiguration.m_massProperties.m_inertia.IsClose(
            bodyConfiguration.m_massProperties.m_inertia,
            1.0e-5f));

        BodyRuntimeConfiguration runtimeConfiguration;
        runtimeConfiguration.m_angularDamping = 0.0f;
        runtimeConfiguration.m_linearDamping = 0.0f;
        runtimeConfiguration.m_massProperties = {
            .m_inertia = AZ::Matrix3x3::CreateDiagonal(AZ::Vector3(4.0f, 8.0f, 16.0f)),
            .m_mass = 8.0f,
            .m_mode = MassPropertiesMode::Provided,
        };
        ASSERT_TRUE(
            system.UpdateBodyRuntimeConfiguration(
                worldHandle,
                bodyHandle,
                runtimeConfiguration,
                true));

        BodyRuntimeConfiguration currentRuntimeConfiguration;
        ASSERT_TRUE(system.GetBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            currentRuntimeConfiguration));
        EXPECT_EQ(currentRuntimeConfiguration.m_massProperties.m_mode, MassPropertiesMode::Provided);
        EXPECT_NEAR(currentRuntimeConfiguration.m_massProperties.m_mass, 8.0f, 1.0e-5f);
        EXPECT_TRUE(currentRuntimeConfiguration.m_massProperties.m_inertia.IsClose(
            runtimeConfiguration.m_massProperties.m_inertia,
            1.0e-5f));

        ASSERT_TRUE(
            system.SetBodyVelocities(
                worldHandle,
                bodyHandle,
                AZ::Vector3::CreateZero(),
                AZ::Vector3::CreateZero()));
        EXPECT_TRUE(system.AddImpulse(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX(8.0f)));
        EXPECT_TRUE(system.AddAngularImpulse(worldHandle, bodyHandle, AZ::Vector3::CreateAxisX(4.0f)));
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandle, state));
        EXPECT_NEAR(state.m_linearVelocity.GetX(), 1.0f, 1.0e-5f);
        EXPECT_NEAR(state.m_angularVelocity.GetX(), 1.0f, 1.0e-5f);

        AZ::Matrix3x3 inverseInertia;
        ASSERT_TRUE(system.GetBodyInverseInertia(worldHandle, bodyHandle, inverseInertia));
        EXPECT_NEAR(inverseInertia.GetElement(0, 0), 0.25f, 1.0e-5f);

        float inverseMass = 0.0f;
        ASSERT_TRUE(system.GetBodyInverseMass(worldHandle, bodyHandle, inverseMass));
        EXPECT_NEAR(inverseMass, 0.125f, 1.0e-5f);

        const StateSnapshotHandle propertySnapshot = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(propertySnapshot);
        EXPECT_TRUE(system.SetBodyFriction(worldHandle, bodyHandle, 0.75f));
        EXPECT_TRUE(system.SetBodyRestitution(worldHandle, bodyHandle, 0.5f));
        EXPECT_TRUE(system.SetBodyGravityFactor(worldHandle, bodyHandle, -0.25f));
        EXPECT_TRUE(system.SetBodyMaximumLinearVelocity(worldHandle, bodyHandle, 42.0f));
        EXPECT_TRUE(system.SetBodyMaximumAngularVelocity(worldHandle, bodyHandle, 21.0f));
        EXPECT_TRUE(system.SetBodyMotionQuality(worldHandle, bodyHandle, MotionQuality::Continuous));
        EXPECT_TRUE(system.SetBodyManifoldReductionEnabled(worldHandle, bodyHandle, false));
        EXPECT_TRUE(system.SetBodySensor(worldHandle, bodyHandle, true));
        EXPECT_TRUE(system.SetBodyLinearDamping(worldHandle, bodyHandle, 0.15f));
        EXPECT_TRUE(system.SetBodyAngularDamping(worldHandle, bodyHandle, 0.25f));
        EXPECT_TRUE(system.SetBodySleepingAllowed(worldHandle, bodyHandle, false));
        EXPECT_TRUE(system.SetBodyGyroscopicForceEnabled(worldHandle, bodyHandle, true));
        EXPECT_TRUE(system.SetBodyKinematicVsNonDynamicCollisionEnabled(worldHandle, bodyHandle, true));
        EXPECT_TRUE(system.SetBodyEnhancedInternalEdgeRemovalEnabled(worldHandle, bodyHandle, true));
        EXPECT_TRUE(system.SetBodySolverStepCounts(worldHandle, bodyHandle, 7, 3));

        float friction = 0.0f;
        float restitution = 0.0f;
        float gravityFactor = 0.0f;
        float linearDamping = 0.0f;
        float angularDamping = 0.0f;
        float maximumLinearVelocity = 0.0f;
        float maximumAngularVelocity = 0.0f;
        MotionQuality motionQuality = MotionQuality::None;
        bool manifoldReductionEnabled = true;
        bool sensor = false;
        bool sleepingAllowed = true;
        bool gyroscopicForceEnabled = false;
        bool kinematicVsNonDynamicCollisionEnabled = false;
        bool enhancedInternalEdgeRemovalEnabled = false;
        AZ::u8 velocityStepCount = 0;
        AZ::u8 positionStepCount = 0;
        EXPECT_TRUE(system.GetBodyFriction(worldHandle, bodyHandle, friction));
        EXPECT_TRUE(system.GetBodyRestitution(worldHandle, bodyHandle, restitution));
        EXPECT_TRUE(system.GetBodyGravityFactor(worldHandle, bodyHandle, gravityFactor));
        EXPECT_TRUE(system.GetBodyMaximumLinearVelocity(
            worldHandle,
            bodyHandle,
            maximumLinearVelocity));
        EXPECT_TRUE(system.GetBodyMaximumAngularVelocity(
            worldHandle,
            bodyHandle,
            maximumAngularVelocity));
        EXPECT_TRUE(system.GetBodyMotionQuality(worldHandle, bodyHandle, motionQuality));
        EXPECT_TRUE(system.IsBodyManifoldReductionEnabled(
            worldHandle,
            bodyHandle,
            manifoldReductionEnabled));
        EXPECT_TRUE(system.IsBodySensor(worldHandle, bodyHandle, sensor));
        EXPECT_TRUE(system.GetBodyLinearDamping(worldHandle, bodyHandle, linearDamping));
        EXPECT_TRUE(system.GetBodyAngularDamping(worldHandle, bodyHandle, angularDamping));
        EXPECT_TRUE(system.IsBodySleepingAllowed(worldHandle, bodyHandle, sleepingAllowed));
        EXPECT_TRUE(system.IsBodyGyroscopicForceEnabled(worldHandle, bodyHandle, gyroscopicForceEnabled));
        EXPECT_TRUE(system.IsBodyKinematicVsNonDynamicCollisionEnabled(
            worldHandle,
            bodyHandle,
            kinematicVsNonDynamicCollisionEnabled));
        EXPECT_TRUE(system.IsBodyEnhancedInternalEdgeRemovalEnabled(
            worldHandle,
            bodyHandle,
            enhancedInternalEdgeRemovalEnabled));
        EXPECT_TRUE(system.GetBodySolverStepCounts(
            worldHandle,
            bodyHandle,
            velocityStepCount,
            positionStepCount));
        EXPECT_FLOAT_EQ(friction, 0.75f);
        EXPECT_FLOAT_EQ(restitution, 0.5f);
        EXPECT_FLOAT_EQ(gravityFactor, -0.25f);
        EXPECT_FLOAT_EQ(maximumLinearVelocity, 42.0f);
        EXPECT_FLOAT_EQ(maximumAngularVelocity, 21.0f);
        EXPECT_EQ(motionQuality, MotionQuality::Continuous);
        EXPECT_FALSE(manifoldReductionEnabled);
        EXPECT_TRUE(sensor);
        EXPECT_FLOAT_EQ(linearDamping, 0.15f);
        EXPECT_FLOAT_EQ(angularDamping, 0.25f);
        EXPECT_FALSE(sleepingAllowed);
        EXPECT_TRUE(gyroscopicForceEnabled);
        EXPECT_TRUE(kinematicVsNonDynamicCollisionEnabled);
        EXPECT_TRUE(enhancedInternalEdgeRemovalEnabled);
        EXPECT_EQ(velocityStepCount, 7);
        EXPECT_EQ(positionStepCount, 3);

        ASSERT_TRUE(system.GetBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            currentRuntimeConfiguration));
        EXPECT_FLOAT_EQ(currentRuntimeConfiguration.m_friction, friction);
        EXPECT_FLOAT_EQ(currentRuntimeConfiguration.m_restitution, restitution);
        EXPECT_FLOAT_EQ(currentRuntimeConfiguration.m_gravityFactor, gravityFactor);
        EXPECT_FLOAT_EQ(
            currentRuntimeConfiguration.m_maximumLinearVelocity,
            maximumLinearVelocity);
        EXPECT_FLOAT_EQ(
            currentRuntimeConfiguration.m_maximumAngularVelocity,
            maximumAngularVelocity);
        EXPECT_EQ(currentRuntimeConfiguration.m_motionQuality, motionQuality);
        EXPECT_EQ(currentRuntimeConfiguration.m_useManifoldReduction, manifoldReductionEnabled);
        EXPECT_EQ(currentRuntimeConfiguration.m_isSensor, sensor);
        EXPECT_FLOAT_EQ(currentRuntimeConfiguration.m_linearDamping, linearDamping);
        EXPECT_FLOAT_EQ(currentRuntimeConfiguration.m_angularDamping, angularDamping);
        EXPECT_EQ(currentRuntimeConfiguration.m_allowSleeping, sleepingAllowed);
        EXPECT_EQ(currentRuntimeConfiguration.m_applyGyroscopicForce, gyroscopicForceEnabled);
        EXPECT_EQ(
            currentRuntimeConfiguration.m_collideKinematicVsNonDynamic,
            kinematicVsNonDynamicCollisionEnabled);
        EXPECT_EQ(
            currentRuntimeConfiguration.m_enhancedInternalEdgeRemoval,
            enhancedInternalEdgeRemovalEnabled);
        EXPECT_EQ(currentRuntimeConfiguration.m_velocityStepCount, velocityStepCount);
        EXPECT_EQ(currentRuntimeConfiguration.m_positionStepCount, positionStepCount);

        EXPECT_FALSE(system.RestoreWorldState(worldHandle, propertySnapshot));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, propertySnapshot));
        EXPECT_FALSE(system.SetBodyFriction(worldHandle, bodyHandle, -1.0f));
        EXPECT_FALSE(system.SetBodyRestitution(worldHandle, bodyHandle, 2.0f));
        EXPECT_FALSE(system.SetBodyGravityFactor(
            worldHandle,
            bodyHandle,
            AZStd::numeric_limits<float>::quiet_NaN()));
        EXPECT_FALSE(system.SetBodyMaximumLinearVelocity(worldHandle, bodyHandle, -1.0f));
        EXPECT_FALSE(system.SetBodyMaximumAngularVelocity(worldHandle, bodyHandle, -1.0f));
        EXPECT_FALSE(system.SetBodyMotionQuality(worldHandle, bodyHandle, MotionQuality::None));
        EXPECT_FALSE(system.SetBodyLinearDamping(worldHandle, bodyHandle, -1.0f));
        EXPECT_FALSE(system.SetBodyAngularDamping(
            worldHandle,
            bodyHandle,
            AZStd::numeric_limits<float>::quiet_NaN()));

        bodyConfiguration.m_massProperties.m_inertia.SetElement(0, 1, 1.0f);
        EXPECT_FALSE(system.CreateBody(worldHandle, bodyConfiguration));

        bodyConfiguration.m_massProperties = runtimeConfiguration.m_massProperties;
        bodyConfiguration.m_friction = -1.0f;
        EXPECT_FALSE(system.CreateBody(worldHandle, bodyConfiguration));
        bodyConfiguration.m_friction = 0.2f;
        bodyConfiguration.m_restitution = 2.0f;
        EXPECT_FALSE(system.CreateBody(worldHandle, bodyConfiguration));
        bodyConfiguration.m_restitution = 0.0f;
        bodyConfiguration.m_allowedDofs = static_cast<AllowedDofs>(0x80);
        EXPECT_FALSE(system.CreateBody(worldHandle, bodyConfiguration));

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_FALSE(system.GetBodyRuntimeConfiguration(
            worldHandle,
            bodyHandle,
            currentRuntimeConfiguration));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, StaticBodiesRequireMotionStorageBeforeChangingMotionType)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle fixedBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(fixedBodyHandle);
        EXPECT_FALSE(system.SetBodyMotionType(worldHandle, fixedBodyHandle, MotionType::Dynamic, false));

        float inverseMass = 0.0f;
        EXPECT_FALSE(system.GetBodyInverseMass(worldHandle, fixedBodyHandle, inverseMass));
        AZ::Matrix3x3 inverseInertia;
        EXPECT_FALSE(system.GetBodyInverseInertia(worldHandle, fixedBodyHandle, inverseInertia));

        bodyConfiguration.m_allowDynamicOrKinematic = true;
        const BodyHandle mutableBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(mutableBodyHandle);
        EXPECT_TRUE(system.SetBodyMotionType(worldHandle, mutableBodyHandle, MotionType::Dynamic, false));

        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, mutableBodyHandle, state));
        EXPECT_EQ(state.m_motionType, MotionType::Dynamic);

        EXPECT_TRUE(system.DestroyBody(worldHandle, mutableBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, fixedBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, ClosestRaycastReturnsProviderIdentityAndSurfaceData)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const MaterialHandle materialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(materialHandle);
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{};
        shapeConfiguration.m_materials = {materialHandle};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        bodyConfiguration.m_transform.m_position.m_z = -2.0;
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        RaycastRequest request;
        request.m_start.m_z = 5.0;
        request.m_displacement = AZ::Vector3(0.0f, 0.0f, -10.0f);
        RaycastHit hit;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_TRUE(system.RaycastAny(worldHandle, request));
        EXPECT_EQ(hit.m_bodyHandle, bodyHandle);
        EXPECT_EQ(hit.m_materialHandle, materialHandle);
        EXPECT_EQ(hit.m_shapeHandle, shapeHandle);
        EXPECT_NEAR(hit.m_fraction, 0.45f, 1.0e-5f);
        EXPECT_NEAR(hit.m_position.m_z, 0.5, 1.0e-5);
        EXPECT_TRUE(hit.m_normal.IsClose(AZ::Vector3::CreateAxisZ()));

        request.m_filter.m_collisionLayer = DefaultLayers::NonMoving;
        EXPECT_FALSE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_FALSE(system.RaycastAny(worldHandle, request));
        request.m_filter.m_collisionLayer = DefaultLayers::Moving;
        EXPECT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_TRUE(system.RaycastAny(worldHandle, request));

        RaycastHit allHits[1];
        const QueryResult raycastResult = system.RaycastAll(worldHandle, request, allHits);
        EXPECT_EQ(raycastResult.m_hitCount, 1);
        EXPECT_EQ(raycastResult.m_requiredHitCount, 2);
        EXPECT_FALSE(raycastResult.IsComplete());
        EXPECT_EQ(allHits[0].m_bodyHandle, bodyHandle);

        RecordingQueryFilter queryFilter;
        queryFilter.m_rejectedBodyHandle = bodyHandle;
        request.m_filter.m_callback = &queryFilter;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, secondBodyHandle);
        EXPECT_TRUE(system.RaycastAny(worldHandle, request));
        const QueryResult filteredRaycastResult = system.RaycastAll(worldHandle, request, allHits);
        ASSERT_EQ(filteredRaycastResult.m_hitCount, 1);
        EXPECT_TRUE(filteredRaycastResult.IsComplete());
        EXPECT_EQ(allHits[0].m_bodyHandle, secondBodyHandle);
        EXPECT_GT(queryFilter.m_bodyCallCount, 0);
        EXPECT_GT(queryFilter.m_shapeCallCount, 0);

        BroadPhaseOverlapRequest broadPhaseOverlapRequest;
        broadPhaseOverlapRequest.m_geometry = BroadPhaseAabb{
            .m_center = {.m_z = -1.0},
            .m_halfExtents = AZ::Vector3(2.0f, 2.0f, 4.0f),
        };
        broadPhaseOverlapRequest.m_filter.m_callback = &queryFilter;
        BroadPhaseHit broadPhaseHits[2];
        const QueryResult broadPhaseOverlapResult =
            system.OverlapBroadPhase(worldHandle, broadPhaseOverlapRequest, broadPhaseHits);
        ASSERT_EQ(broadPhaseOverlapResult.m_hitCount, 1);
        EXPECT_TRUE(broadPhaseOverlapResult.IsComplete());
        EXPECT_EQ(broadPhaseHits[0].m_bodyHandle, secondBodyHandle);
        EXPECT_TRUE(system.OverlapBroadPhaseAny(worldHandle, broadPhaseOverlapRequest));

        BroadPhaseCastRequest broadPhaseCastRequest;
        broadPhaseCastRequest.m_geometry = BroadPhaseRay{
            .m_start = {.m_z = 5.0},
            .m_displacement = AZ::Vector3(0.0f, 0.0f, -10.0f),
        };
        broadPhaseCastRequest.m_filter.m_callback = &queryFilter;
        BroadPhaseCastHit broadPhaseCastHit;
        ASSERT_TRUE(system.CastBroadPhaseClosest(worldHandle, broadPhaseCastRequest, broadPhaseCastHit));
        EXPECT_EQ(broadPhaseCastHit.m_bodyHandle, secondBodyHandle);
        BroadPhaseCastHit broadPhaseCastHits[2];
        const QueryResult broadPhaseCastResult =
            system.CastBroadPhaseAll(worldHandle, broadPhaseCastRequest, broadPhaseCastHits);
        ASSERT_EQ(broadPhaseCastResult.m_hitCount, 1);
        EXPECT_TRUE(broadPhaseCastResult.IsComplete());
        EXPECT_EQ(broadPhaseCastHits[0].m_bodyHandle, secondBodyHandle);

        queryFilter.m_rejectedBodyHandle = BodyHandle::Invalid;
        queryFilter.m_rejectedShapeKind = ShapeKind::Box;
        EXPECT_FALSE(system.RaycastAny(worldHandle, request));

        OverlapHit overlapHits[2];
        PointOverlapRequest pointRequest;
        pointRequest.m_filter.m_callback = &queryFilter;
        EXPECT_FALSE(system.OverlapPointAny(worldHandle, pointRequest));
        pointRequest.m_filter.m_callback = nullptr;
        const QueryResult overlapResult = system.OverlapPoint(worldHandle, pointRequest, overlapHits);
        EXPECT_EQ(overlapResult.m_hitCount, 1);
        EXPECT_EQ(overlapResult.m_requiredHitCount, 1);
        EXPECT_TRUE(overlapResult.IsComplete());
        EXPECT_EQ(overlapHits[0].m_bodyHandle, bodyHandle);
        EXPECT_TRUE(system.OverlapPointAny(worldHandle, PointOverlapRequest{}));

        PointOverlapRequest missedPoint;
        missedPoint.m_position.m_x = 100.0;
        EXPECT_FALSE(system.OverlapPointAny(worldHandle, missedPoint));
        EXPECT_TRUE(system.OptimizeBroadPhase(worldHandle));

        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, QueriesRunConcurrentlyAndRestoreEachThreadFloatEnvironment)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);

        BlockingQueryFilter filter;
        RaycastRequest request;
        request.m_start.m_z = 5.0;
        request.m_displacement = -10.0f * AZ::Vector3::CreateAxisZ();
        request.m_filter.m_callback = &filter;

        AZStd::atomic_bool firstFound{false};
        AZStd::atomic_bool firstRestored{false};
        AZStd::atomic_bool secondFound{false};
        AZStd::atomic_bool secondRestored{false};
        AZStd::thread firstThread(
            [&system, &scene, &request, &firstFound, &firstRestored]()
            {
                std::fesetround(FE_DOWNWARD);
                firstFound.store(
                    system.RaycastAny(scene.m_worldHandle, request),
                    AZStd::memory_order_release);
                firstRestored.store(
                    std::fegetround() == FE_DOWNWARD,
                    AZStd::memory_order_release);
            });
        AZStd::thread secondThread(
            [&system, &scene, &request, &secondFound, &secondRestored]()
            {
                std::fesetround(FE_UPWARD);
                secondFound.store(
                    system.RaycastAny(scene.m_worldHandle, request),
                    AZStd::memory_order_release);
                secondRestored.store(
                    std::fegetround() == FE_UPWARD,
                    AZStd::memory_order_release);
            });

        const bool bothQueriesEntered = WaitUntil(
            [&filter]()
            {
                return filter.m_enterCount.load(AZStd::memory_order_acquire) >= 2;
            });
        filter.m_release.store(true, AZStd::memory_order_release);
        firstThread.join();
        secondThread.join();

        EXPECT_TRUE(bothQueriesEntered);
        EXPECT_TRUE(firstFound.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(secondFound.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(firstRestored.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(secondRestored.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(filter.m_sawCanonicalFloatEnvironment.load(AZStd::memory_order_acquire));

        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, TopologyMutationWaitsForInFlightQuery)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);

        BlockingQueryFilter filter;
        RaycastRequest request;
        request.m_start.m_z = 5.0;
        request.m_displacement = -10.0f * AZ::Vector3::CreateAxisZ();
        request.m_filter.m_callback = &filter;

        AZStd::atomic_bool queryFound{false};
        AZStd::thread queryThread(
            [&system, &scene, &request, &queryFound]()
            {
                queryFound.store(
                    system.RaycastAny(scene.m_worldHandle, request),
                    AZStd::memory_order_release);
            });
        const bool queryEntered = WaitUntil(
            [&filter]()
            {
                return filter.m_enterCount.load(AZStd::memory_order_acquire) > 0;
            });

        AZStd::atomic_bool mutationCompleted{false};
        AZStd::atomic_bool mutationStarted{false};
        AZStd::atomic_bool mutationSucceeded{false};
        AZStd::thread mutationThread(
            [&system, &scene, &mutationCompleted, &mutationStarted, &mutationSucceeded]()
            {
                mutationStarted.store(true, AZStd::memory_order_release);
                mutationSucceeded.store(
                    system.DestroyBody(scene.m_worldHandle, scene.m_sphereBodyHandle),
                    AZStd::memory_order_release);
                mutationCompleted.store(true, AZStd::memory_order_release);
            });
        const bool mutationStartedWhileQueryWasBlocked = WaitUntil(
            [&mutationStarted]()
            {
                return mutationStarted.load(AZStd::memory_order_acquire);
            });
        for (AZ::u32 attempt = 0; attempt < 50'000; ++attempt)
        {
            AZStd::this_thread::yield();
        }
        const bool mutationCompletedWhileQueryWasBlocked =
            mutationCompleted.load(AZStd::memory_order_acquire);

        filter.m_release.store(true, AZStd::memory_order_release);
        queryThread.join();
        mutationThread.join();

        EXPECT_TRUE(queryEntered);
        EXPECT_TRUE(mutationStartedWhileQueryWasBlocked);
        EXPECT_FALSE(mutationCompletedWhileQueryWasBlocked);
        EXPECT_TRUE(queryFound.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(mutationSucceeded.load(AZStd::memory_order_acquire));

        EXPECT_TRUE(system.DestroyBody(scene.m_worldHandle, scene.m_floorBodyHandle));
        EXPECT_TRUE(system.DestroyShape(scene.m_worldHandle, scene.m_sphereShapeHandle));
        EXPECT_TRUE(system.DestroyShape(scene.m_worldHandle, scene.m_floorShapeHandle));
    }

    TEST(SimulationTests, BroadPhaseQueryProgressesDuringSimulationUpdate)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);

        BlockingStepListener listener;
        ScopedExtensionRegistration registration(system, &listener);
        ASSERT_TRUE(system.AddStepListener(scene.m_worldHandle, registration.GetHandle()));
        AZStd::atomic_bool stepSucceeded{false};
        AZStd::thread stepThread(
            [&system, &scene, &stepSucceeded]()
            {
                stepSucceeded.store(
                    system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f),
                    AZStd::memory_order_release);
            });
        const bool stepEntered = WaitUntil(
            [&listener]()
            {
                return listener.m_entered.load(AZStd::memory_order_acquire);
            });

        BroadPhaseOverlapRequest request;
        request.m_geometry = BroadPhaseAabb{
            .m_center = {.m_z = 1.0},
            .m_halfExtents = AZ::Vector3(10.0f),
        };
        AZStd::atomic_bool queryCompleted{false};
        AZStd::atomic_bool queryFound{false};
        AZStd::thread queryThread(
            [&system, &scene, &request, &queryCompleted, &queryFound]()
            {
                queryFound.store(
                    system.OverlapBroadPhaseAny(scene.m_worldHandle, request),
                    AZStd::memory_order_release);
                queryCompleted.store(true, AZStd::memory_order_release);
            });
        const bool queryCompletedDuringStep = WaitUntil(
            [&queryCompleted]()
            {
                return queryCompleted.load(AZStd::memory_order_acquire);
            });

        listener.m_release.store(true, AZStd::memory_order_release);
        queryThread.join();
        stepThread.join();

        EXPECT_TRUE(stepEntered);
        EXPECT_TRUE(queryCompletedDuringStep);
        EXPECT_TRUE(queryFound.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(stepSucceeded.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(system.RemoveStepListener(scene.m_worldHandle, registration.GetHandle()));

        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, OptimizedSphereOverlapLocksBodiesDuringSimulationUpdate)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);

        BlockingStepListener listener;
        ScopedExtensionRegistration registration(system, &listener);
        ASSERT_TRUE(system.AddStepListener(scene.m_worldHandle, registration.GetHandle()));
        AZStd::atomic_bool stepSucceeded{false};
        AZStd::thread stepThread(
            [&system, &scene, &stepSucceeded]()
            {
                stepSucceeded.store(
                    system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f),
                    AZStd::memory_order_release);
            });
        const bool stepEntered = WaitUntil(
            [&listener]()
            {
                return listener.m_entered.load(AZStd::memory_order_acquire);
            });

        ShapeOverlapRequest request;
        request.m_shapeHandle = scene.m_sphereShapeHandle;
        request.m_transform.m_position.m_z = 1.0;
        AZStd::array<OverlapHit, 4> hits;
        AZStd::atomic_bool queryCompleted{false};
        QueryResult queryResult;
        AZStd::thread queryThread(
            [&system, &scene, &request, &hits, &queryCompleted, &queryResult]()
            {
                queryResult = system.OverlapShape(scene.m_worldHandle, request, hits);
                queryCompleted.store(true, AZStd::memory_order_release);
            });
        const bool queryBlockedDuringStep = !WaitUntil(
            [&queryCompleted]()
            {
                return queryCompleted.load(AZStd::memory_order_acquire);
            },
            AZStd::chrono::milliseconds(10));

        listener.m_release.store(true, AZStd::memory_order_release);
        queryThread.join();
        stepThread.join();

        EXPECT_TRUE(stepEntered);
        EXPECT_TRUE(queryBlockedDuringStep);
        EXPECT_TRUE(queryCompleted.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(queryResult.IsComplete());
        EXPECT_GE(queryResult.m_hitCount, 1);
        EXPECT_TRUE(AZStd::any_of(
            hits.begin(),
            hits.begin() + queryResult.m_hitCount,
            [&scene](const OverlapHit& hit)
            {
                return hit.m_bodyHandle == scene.m_sphereBodyHandle;
            }));
        EXPECT_TRUE(stepSucceeded.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(system.RemoveStepListener(scene.m_worldHandle, registration.GetHandle()));

        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, ClosestRaycastBoxFastPathMatchesCanonicalNarrowPhase)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const MaterialHandle materialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(materialHandle);

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{};
        shapeConfiguration.m_materials = {materialHandle};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        WorldTransform movedTransform;
        movedTransform.m_position = {
            .m_x = 2.0,
            .m_y = 3.0,
            .m_z = 4.0,
        };
        ASSERT_TRUE(system.SetBodyTransform(worldHandle, bodyHandle, movedTransform, false));

        const AZStd::array<RaycastRequest, 9> requests = {{
            {
                .m_start = {.m_x = 7.0, .m_y = 3.0, .m_z = 4.0},
                .m_displacement = AZ::Vector3(-10.0f, 0.0f, 0.0f),
            },
            {
                .m_start = {.m_x = -3.0, .m_y = 3.0, .m_z = 4.0},
                .m_displacement = AZ::Vector3(10.0f, 0.0f, 0.0f),
            },
            {
                .m_start = {.m_x = 2.0, .m_y = 8.0, .m_z = 4.0},
                .m_displacement = AZ::Vector3(0.0f, -10.0f, 0.0f),
            },
            {
                .m_start = {.m_x = 2.0, .m_y = -2.0, .m_z = 4.0},
                .m_displacement = AZ::Vector3(0.0f, 10.0f, 0.0f),
            },
            {
                .m_start = {.m_x = 2.0, .m_y = 3.0, .m_z = 9.0},
                .m_displacement = AZ::Vector3(0.0f, 0.0f, -10.0f),
            },
            {
                .m_start = {.m_x = 2.0, .m_y = 3.0, .m_z = -1.0},
                .m_displacement = AZ::Vector3(0.0f, 0.0f, 10.0f),
            },
            {
                .m_start = {.m_x = 7.0, .m_y = 8.0, .m_z = 9.0},
                .m_displacement = AZ::Vector3(-10.0f, -10.0f, -10.0f),
            },
            {
                .m_start = {.m_x = 2.0, .m_y = 3.0, .m_z = 4.0},
                .m_displacement = AZ::Vector3::CreateAxisX(10.0f),
            },
            {
                .m_start = {.m_x = 2.5, .m_y = 3.0, .m_z = 4.0},
                .m_displacement = AZ::Vector3::CreateAxisY(10.0f),
            },
        }};

        RecordingQueryFilter referenceFilter;
        for (RaycastRequest request : requests)
        {
            RaycastHit optimizedHit;
            ASSERT_TRUE(system.RaycastClosest(worldHandle, request, optimizedHit));

            request.m_filter.m_callback = &referenceFilter;
            RaycastHit referenceHit;
            ASSERT_TRUE(system.RaycastClosest(worldHandle, request, referenceHit));

            EXPECT_EQ(optimizedHit.m_bodyHandle, referenceHit.m_bodyHandle);
            EXPECT_EQ(optimizedHit.m_materialHandle, referenceHit.m_materialHandle);
            EXPECT_EQ(optimizedHit.m_shapeHandle, referenceHit.m_shapeHandle);
            EXPECT_EQ(optimizedHit.m_subShapeId, referenceHit.m_subShapeId);
            EXPECT_NEAR(optimizedHit.m_fraction, referenceHit.m_fraction, 1.0e-6f);
            EXPECT_NEAR(optimizedHit.m_position.m_x, referenceHit.m_position.m_x, 1.0e-6);
            EXPECT_NEAR(optimizedHit.m_position.m_y, referenceHit.m_position.m_y, 1.0e-6);
            EXPECT_NEAR(optimizedHit.m_position.m_z, referenceHit.m_position.m_z, 1.0e-6);
            EXPECT_TRUE(optimizedHit.m_normal.IsClose(referenceHit.m_normal, 1.0e-6f));
        }

        EXPECT_EQ(referenceFilter.m_bodyCallCount, requests.size());
        EXPECT_EQ(referenceFilter.m_shapeCallCount, requests.size());
        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, ClosestPerBodyCastsReturnOneCanonicalHitForEachBody)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration sphereConfiguration;
        sphereConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const ShapeHandle sphereHandle = system.CreateShape(worldHandle, sphereConfiguration);
        ASSERT_TRUE(sphereHandle);

        CompoundShapeConfiguration compoundConfiguration;
        compoundConfiguration.m_children = {
            {
                .m_position = AZ::Vector3::CreateAxisZ(1.0f),
                .m_shapeHandle = sphereHandle,
            },
            {
                .m_position = -AZ::Vector3::CreateAxisZ(1.0f),
                .m_shapeHandle = sphereHandle,
            },
        };
        const ShapeHandle compoundHandle = system.CreateShape(worldHandle, compoundConfiguration);
        ASSERT_TRUE(compoundHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = compoundHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle compoundBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(compoundBodyHandle);

        bodyConfiguration.m_shapeHandle = sphereHandle;
        bodyConfiguration.m_transform.m_position.m_z = -4.0;
        const BodyHandle sphereBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(sphereBodyHandle);

        RaycastRequest raycastRequest;
        raycastRequest.m_start.m_z = 5.0;
        raycastRequest.m_displacement = -12.0f * AZ::Vector3::CreateAxisZ();
        AZStd::array<RaycastHit, 3> allRaycastHits;
        const QueryResult allRaycastResult = system.RaycastAll(
            worldHandle,
            raycastRequest,
            allRaycastHits);
        ASSERT_EQ(allRaycastResult.m_hitCount, 3);
        ASSERT_TRUE(allRaycastResult.IsComplete());

        AZStd::array<RaycastHit, 2> raycastHits;
        const QueryResult raycastResult = system.RaycastClosestPerBody(
            worldHandle,
            raycastRequest,
            raycastHits);
        ASSERT_EQ(raycastResult.m_hitCount, 2);
        ASSERT_TRUE(raycastResult.IsComplete());
        EXPECT_EQ(raycastHits[0].m_bodyHandle, compoundBodyHandle);
        EXPECT_EQ(raycastHits[0].m_subShapeId, allRaycastHits[0].m_subShapeId);
        EXPECT_EQ(raycastHits[1].m_bodyHandle, sphereBodyHandle);

        AZStd::array<RaycastHit, 1> truncatedRaycastHits;
        const QueryResult truncatedRaycastResult = system.RaycastClosestPerBody(
            worldHandle,
            raycastRequest,
            truncatedRaycastHits);
        EXPECT_EQ(truncatedRaycastResult.m_hitCount, 1);
        EXPECT_EQ(truncatedRaycastResult.m_requiredHitCount, 2);

        ShapeConfiguration queryConfiguration;
        queryConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.25f};
        const ShapeHandle queryShapeHandle = system.CreateShape(worldHandle, queryConfiguration);
        ASSERT_TRUE(queryShapeHandle);

        ShapeCastRequest shapeCastRequest;
        shapeCastRequest.m_shapeHandle = queryShapeHandle;
        shapeCastRequest.m_start.m_position.m_z = 5.0;
        shapeCastRequest.m_displacement = -12.0f * AZ::Vector3::CreateAxisZ();
        AZStd::array<ShapeCastHit, 3> allShapeCastHits;
        const QueryResult allShapeCastResult = system.CastShapeAll(
            worldHandle,
            shapeCastRequest,
            allShapeCastHits);
        ASSERT_EQ(allShapeCastResult.m_hitCount, 3);
        ASSERT_TRUE(allShapeCastResult.IsComplete());

        AZStd::array<ShapeCastHit, 2> shapeCastHits;
        const QueryResult shapeCastResult = system.CastShapeClosestPerBody(
            worldHandle,
            shapeCastRequest,
            shapeCastHits);
        ASSERT_EQ(shapeCastResult.m_hitCount, 2);
        ASSERT_TRUE(shapeCastResult.IsComplete());
        EXPECT_EQ(shapeCastHits[0].m_bodyHandle, compoundBodyHandle);
        EXPECT_EQ(shapeCastHits[0].m_targetSubShapeId, allShapeCastHits[0].m_targetSubShapeId);
        EXPECT_EQ(shapeCastHits[1].m_bodyHandle, sphereBodyHandle);

        EXPECT_TRUE(system.DestroyShape(worldHandle, queryShapeHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, sphereBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, compoundBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, compoundHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, sphereHandle));
    }

    TEST(SimulationTests, ClosestQueriesResolveExactTiesCanonically)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration boxConfiguration;
        boxConfiguration.m_geometry = BoxShapeConfiguration{};
        const ShapeHandle boxHandle = system.CreateShape(worldHandle, boxConfiguration);
        ASSERT_TRUE(boxHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = boxHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        ShapeConfiguration sphereConfiguration;
        sphereConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.25f};
        const ShapeHandle sphereHandle = system.CreateShape(worldHandle, sphereConfiguration);
        ASSERT_TRUE(sphereHandle);

        RaycastRequest raycastRequest;
        raycastRequest.m_start.m_z = 5.0;
        raycastRequest.m_displacement = AZ::Vector3(0.0f, 0.0f, -10.0f);

        ShapeCastRequest shapeCastRequest;
        shapeCastRequest.m_shapeHandle = sphereHandle;
        shapeCastRequest.m_start.m_position.m_z = 5.0;
        shapeCastRequest.m_displacement = AZ::Vector3(0.0f, 0.0f, -10.0f);

        BroadPhaseCastRequest broadPhaseCastRequest;
        broadPhaseCastRequest.m_geometry = BroadPhaseRay{
            .m_start = {.m_z = 5.0},
            .m_displacement = AZ::Vector3(0.0f, 0.0f, -10.0f),
        };

        for (AZ::u32 phase = 0; phase < 3; ++phase)
        {
            SCOPED_TRACE(phase);

            RaycastHit raycastHit;
            ASSERT_TRUE(system.RaycastClosest(worldHandle, raycastRequest, raycastHit));
            EXPECT_EQ(raycastHit.m_bodyHandle, firstBodyHandle);
            AZStd::array<RaycastHit, 2> raycastHits;
            const QueryResult raycastResult = system.RaycastAll(
                worldHandle,
                raycastRequest,
                raycastHits);
            ASSERT_EQ(raycastResult.m_hitCount, 2);
            EXPECT_EQ(raycastHit.m_bodyHandle, raycastHits[0].m_bodyHandle);

            RecordingQueryFilter filter;
            raycastRequest.m_filter.m_callback = &filter;
            ASSERT_TRUE(system.RaycastClosest(worldHandle, raycastRequest, raycastHit));
            EXPECT_EQ(raycastHit.m_bodyHandle, firstBodyHandle);
            raycastRequest.m_filter.m_callback = nullptr;

            ShapeCastHit shapeCastHit;
            ASSERT_TRUE(system.CastShapeClosest(worldHandle, shapeCastRequest, shapeCastHit));
            EXPECT_EQ(shapeCastHit.m_bodyHandle, firstBodyHandle);
            AZStd::array<ShapeCastHit, 2> shapeCastHits;
            const QueryResult shapeCastResult = system.CastShapeAll(
                worldHandle,
                shapeCastRequest,
                shapeCastHits);
            ASSERT_EQ(shapeCastResult.m_hitCount, 2);
            EXPECT_EQ(shapeCastHit.m_bodyHandle, shapeCastHits[0].m_bodyHandle);

            BroadPhaseCastHit broadPhaseCastHit;
            ASSERT_TRUE(system.CastBroadPhaseClosest(worldHandle, broadPhaseCastRequest, broadPhaseCastHit));
            EXPECT_EQ(broadPhaseCastHit.m_bodyHandle, firstBodyHandle);
            AZStd::array<BroadPhaseCastHit, 2> broadPhaseCastHits;
            QueryResult broadPhaseCastResult = system.CastBroadPhaseAll(
                worldHandle,
                broadPhaseCastRequest,
                broadPhaseCastHits);
            ASSERT_EQ(broadPhaseCastResult.m_hitCount, 2);
            EXPECT_EQ(broadPhaseCastHit.m_bodyHandle, broadPhaseCastHits[0].m_bodyHandle);

            broadPhaseCastRequest.m_geometry = BroadPhaseAabbCast{
                .m_start = {
                    .m_center = {.m_z = 5.0},
                    .m_halfExtents = AZ::Vector3(0.25f),
                },
                .m_displacement = AZ::Vector3(0.0f, 0.0f, -10.0f),
            };
            ASSERT_TRUE(system.CastBroadPhaseClosest(worldHandle, broadPhaseCastRequest, broadPhaseCastHit));
            EXPECT_EQ(broadPhaseCastHit.m_bodyHandle, firstBodyHandle);
            broadPhaseCastResult = system.CastBroadPhaseAll(
                worldHandle,
                broadPhaseCastRequest,
                broadPhaseCastHits);
            ASSERT_EQ(broadPhaseCastResult.m_hitCount, 2);
            EXPECT_EQ(broadPhaseCastHit.m_bodyHandle, broadPhaseCastHits[0].m_bodyHandle);

            broadPhaseCastRequest.m_geometry = BroadPhaseRay{
                .m_start = {.m_z = 5.0},
                .m_displacement = AZ::Vector3(0.0f, 0.0f, -10.0f),
            };

            if (phase == 0)
            {
                ASSERT_TRUE(system.OptimizeBroadPhase(worldHandle));
            }
            else if (phase == 1)
            {
                ASSERT_TRUE(system.RemoveBodyFromSimulation(worldHandle, firstBodyHandle));
                ASSERT_TRUE(system.AddBodyToSimulation(worldHandle, firstBodyHandle, false));
            }
        }

        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, sphereHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, boxHandle));
    }

    TEST(SimulationTests, ClosestRaycastBatchUsesCallerStorageAndRestoresWorkerFloatEnvironment)
    {
        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(4);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);

        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_workerCount = 4;
        Runtime system(systemConfiguration, &jobContext);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = BoxShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);
        bodyConfiguration.m_transform.m_position.m_z = -2.0;
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        constexpr size_t requestCount = 512;
        AZStd::array<RaycastRequest, requestCount> requests;
        for (size_t requestIndex = 0; requestIndex < requests.size(); ++requestIndex)
        {
            requests[requestIndex].m_start = {.m_z = 5.0};
            requests[requestIndex].m_displacement = AZ::Vector3(0.0f, 0.0f, -10.0f);
            if (requestIndex % 2 != 0)
            {
                requests[requestIndex].m_start.m_x = 10.0;
            }
        }

        AZStd::array<ClosestRaycastResult, requestCount / 2> truncatedResults;
        BufferResult batchResult = system.RaycastClosestBatch(worldHandle, requests, truncatedResults);
        EXPECT_EQ(batchResult.m_count, truncatedResults.size());
        EXPECT_EQ(batchResult.m_requiredCount, requests.size());
        EXPECT_FALSE(batchResult.IsComplete());
        EXPECT_TRUE(batchResult.HasOverflow());

        AZStd::array<ClosestRaycastResult, requestCount> results;
        for (ClosestRaycastResult& result : results)
        {
            result.m_found = true;
            result.m_hit.m_bodyHandle = firstBodyHandle;
            result.m_hit.m_shapeHandle = shapeHandle;
        }
        batchResult = system.RaycastClosestBatch(worldHandle, requests, results);
        EXPECT_EQ(batchResult.m_count, requests.size());
        EXPECT_TRUE(batchResult.IsComplete());
        EXPECT_FALSE(batchResult.HasOverflow());
        for (size_t requestIndex = 0; requestIndex < results.size(); ++requestIndex)
        {
            SCOPED_TRACE(requestIndex);
            RaycastHit scalarHit;
            const bool scalarFound = system.RaycastClosest(worldHandle, requests[requestIndex], scalarHit);
            EXPECT_EQ(results[requestIndex].m_found, scalarFound);
            if (!scalarFound)
            {
                EXPECT_FALSE(results[requestIndex].m_hit.m_bodyHandle);
                EXPECT_FALSE(results[requestIndex].m_hit.m_shapeHandle);
                continue;
            }

            EXPECT_EQ(results[requestIndex].m_hit.m_bodyHandle, scalarHit.m_bodyHandle);
            EXPECT_EQ(results[requestIndex].m_hit.m_shapeHandle, scalarHit.m_shapeHandle);
            EXPECT_EQ(results[requestIndex].m_hit.m_materialHandle, scalarHit.m_materialHandle);
            EXPECT_EQ(results[requestIndex].m_hit.m_subShapeId, scalarHit.m_subShapeId);
            EXPECT_EQ(results[requestIndex].m_hit.m_fraction, scalarHit.m_fraction);
            EXPECT_EQ(results[requestIndex].m_hit.m_position, scalarHit.m_position);
            EXPECT_EQ(results[requestIndex].m_hit.m_normal, scalarHit.m_normal);
        }

        RecordingQueryFilter queryFilter;
        queryFilter.m_rejectedBodyHandle = firstBodyHandle;
        requests[0].m_filter.m_callback = &queryFilter;
        batchResult = system.RaycastClosestBatch(worldHandle, requests, results);
        ASSERT_EQ(batchResult.m_count, requests.size());
        ASSERT_TRUE(results[0].m_found);
        EXPECT_EQ(results[0].m_hit.m_bodyHandle, secondBodyHandle);
        EXPECT_GT(queryFilter.m_bodyCallCount, 0);
        EXPECT_GT(queryFilter.m_shapeCallCount, 0);
        requests[0].m_filter.m_callback = nullptr;

        batchResult = system.RaycastClosestBatch(worldHandle, requests, results);
        ASSERT_EQ(batchResult.m_count, requests.size());
        const AZStd::array<ClosestRaycastResult, requestCount> baselineResults = results;
        const int callerRoundingMode = std::fegetround();
        const size_t workerCount = jobManager.GetNumWorkerThreads();
        const bool poisonedWorkers = ChangeWorkerRoundingModes(
            jobContext,
            workerCount,
            FE_UPWARD,
            FE_TONEAREST);
        AZStd::array<ClosestRaycastResult, requestCount> poisonedResults;
        batchResult = system.RaycastClosestBatch(worldHandle, requests, poisonedResults);
        const bool restoredWorkers = ChangeWorkerRoundingModes(
            jobContext,
            workerCount,
            FE_TONEAREST,
            FE_UPWARD);

        EXPECT_TRUE(poisonedWorkers);
        EXPECT_TRUE(restoredWorkers);
        EXPECT_EQ(std::fegetround(), callerRoundingMode);
        EXPECT_EQ(batchResult.m_count, requests.size());
        for (size_t requestIndex = 0; requestIndex < poisonedResults.size(); ++requestIndex)
        {
            SCOPED_TRACE(requestIndex);
            const ClosestRaycastResult& baseline = baselineResults[requestIndex];
            const ClosestRaycastResult& poisoned = poisonedResults[requestIndex];
            EXPECT_EQ(poisoned.m_found, baseline.m_found);
            if (!baseline.m_found)
            {
                continue;
            }

            EXPECT_EQ(poisoned.m_hit.m_bodyHandle, baseline.m_hit.m_bodyHandle);
            EXPECT_EQ(poisoned.m_hit.m_shapeHandle, baseline.m_hit.m_shapeHandle);
            EXPECT_EQ(poisoned.m_hit.m_materialHandle, baseline.m_hit.m_materialHandle);
            EXPECT_EQ(poisoned.m_hit.m_subShapeId, baseline.m_hit.m_subShapeId);
            EXPECT_EQ(poisoned.m_hit.m_fraction, baseline.m_hit.m_fraction);
            EXPECT_EQ(poisoned.m_hit.m_position, baseline.m_hit.m_position);
            EXPECT_EQ(poisoned.m_hit.m_normal, baseline.m_hit.m_normal);
        }

        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, ClosestRaycastBatchReleasesReusableJobsBeforeContextDestruction)
    {
        constexpr size_t requestCount = 256;
        constexpr AZ::u32 lifetimeCount = 3;
        for (AZ::u32 lifetimeIndex = 0; lifetimeIndex < lifetimeCount; ++lifetimeIndex)
        {
            SCOPED_TRACE(lifetimeIndex);
            AZ::JobManagerDesc jobManagerDescriptor;
            jobManagerDescriptor.m_workerThreads.resize(4);
            AZ::JobManager jobManager(jobManagerDescriptor);
            AZ::JobContext jobContext(jobManager);

            SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
            systemConfiguration.m_defaultWorld.m_workerCount = 4;
            Runtime system(systemConfiguration, &jobContext);
            ASSERT_TRUE(system);

            const WorldHandle worldHandle = system.GetDefaultWorldHandle();
            ShapeConfiguration shapeConfiguration;
            shapeConfiguration.m_geometry = BoxShapeConfiguration{};
            const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
            ASSERT_TRUE(shapeHandle);

            BodyConfiguration bodyConfiguration;
            bodyConfiguration.m_shapeHandle = shapeHandle;
            bodyConfiguration.m_motionType = MotionType::Static;
            const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
            ASSERT_TRUE(bodyHandle);

            AZStd::array<RaycastRequest, requestCount> requests;
            for (RaycastRequest& request : requests)
            {
                request.m_start = {.m_z = 5.0};
                request.m_displacement = -10.0f * AZ::Vector3::CreateAxisZ();
            }
            AZStd::array<ClosestRaycastResult, requestCount> results;
            const BufferResult result = system.RaycastClosestBatch(worldHandle, requests, results);
            EXPECT_EQ(result.m_count, requestCount);
            EXPECT_TRUE(result.IsComplete());

            EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
            EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        }
    }

    TEST(SimulationTests, HairCpuComputeIsExactAcrossWorkerCountsAndRestoresWorkerFloatEnvironment)
    {
        const HairDeterminismResult serialResult = SimulateDeterministicHair(1, nullptr);
        ASSERT_TRUE(serialResult.m_succeeded);

        AZ::JobManagerDesc jobManagerDescriptor;
        jobManagerDescriptor.m_workerThreads.resize(8);
        AZ::JobManager jobManager(jobManagerDescriptor);
        AZ::JobContext jobContext(jobManager);
        const size_t jobManagerWorkerCount = jobManager.GetNumWorkerThreads();
        ASSERT_TRUE(ChangeWorkerRoundingModes(
            jobContext,
            jobManagerWorkerCount,
            FE_UPWARD,
            FE_TONEAREST));

        constexpr AZStd::array parallelWorkerCounts = {AZ::u32{4}, AZ::u32{8}};
        constexpr AZ::u32 repetitionCount = 4;
        for (const AZ::u32 workerCount : parallelWorkerCounts)
        {
            SCOPED_TRACE(workerCount);
            for (AZ::u32 repetition = 0; repetition < repetitionCount; ++repetition)
            {
                SCOPED_TRACE(repetition);
                const HairDeterminismResult parallelResult =
                    SimulateDeterministicHair(workerCount, &jobContext);
                ASSERT_TRUE(parallelResult.m_succeeded);
                ASSERT_EQ(parallelResult.m_vertexStates.size(), serialResult.m_vertexStates.size());
                ASSERT_EQ(parallelResult.m_renderPositions.size(), serialResult.m_renderPositions.size());
                ASSERT_EQ(parallelResult.m_gridCells.size(), serialResult.m_gridCells.size());
                for (size_t vertexIndex = 0; vertexIndex < serialResult.m_vertexStates.size(); ++vertexIndex)
                {
                    const HairVertexState& serialState = serialResult.m_vertexStates[vertexIndex];
                    const HairVertexState& parallelState = parallelResult.m_vertexStates[vertexIndex];
                    EXPECT_EQ(parallelState.m_localPosition, serialState.m_localPosition);
                    EXPECT_EQ(parallelState.m_localRotation, serialState.m_localRotation);
                    EXPECT_EQ(parallelState.m_localLinearVelocity, serialState.m_localLinearVelocity);
                    EXPECT_EQ(parallelState.m_localAngularVelocity, serialState.m_localAngularVelocity);
                }
                for (size_t vertexIndex = 0; vertexIndex < serialResult.m_renderPositions.size(); ++vertexIndex)
                {
                    EXPECT_EQ(
                        parallelResult.m_renderPositions[vertexIndex],
                        serialResult.m_renderPositions[vertexIndex]);
                }
                for (size_t cellIndex = 0; cellIndex < serialResult.m_gridCells.size(); ++cellIndex)
                {
                    EXPECT_EQ(
                        parallelResult.m_gridCells[cellIndex].m_velocityAndDensity,
                        serialResult.m_gridCells[cellIndex].m_velocityAndDensity);
                }
            }
        }

        EXPECT_TRUE(ChangeWorkerRoundingModes(
            jobContext,
            jobManagerWorkerCount,
            FE_TONEAREST,
            FE_UPWARD));
    }

    TEST(SimulationTests, MaterialsRemainValidWhileShapesReferenceThem)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        MaterialConfiguration materialConfiguration;
        materialConfiguration.m_debugName = "TestMaterial";
        materialConfiguration.m_debugColor = AZ::Colors::Red;
        const MaterialHandle materialHandle = system.CreateMaterial(materialConfiguration);
        ASSERT_TRUE(materialHandle);
        EXPECT_TRUE(system.IsValid(materialHandle));

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        shapeConfiguration.m_materials = {materialHandle};
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        EXPECT_FALSE(system.DestroyMaterial(materialHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
        EXPECT_FALSE(system.IsValid(materialHandle));
        EXPECT_FALSE(system.CreateShape(worldHandle, shapeConfiguration));
    }

    TEST(SimulationTests, CookedShapesShareNativeDataAcrossWorlds)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);
        EXPECT_EQ(Cooking::Get(), static_cast<Cooking*>(&system));

        const MaterialHandle firstMaterialHandle = system.CreateMaterial(MaterialConfiguration{});
        const MaterialHandle secondMaterialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(firstMaterialHandle);
        ASSERT_TRUE(secondMaterialHandle);

        ShapeConfiguration configuration;
        MeshShapeConfiguration mesh;
        mesh.m_vertices = {
            AZ::Vector3(-3.0f, -1.0f, 0.0f),
            AZ::Vector3(-1.0f, -1.0f, 0.0f),
            AZ::Vector3(-2.0f, 1.0f, 0.0f),
            AZ::Vector3(1.0f, -1.0f, 0.0f),
            AZ::Vector3(3.0f, -1.0f, 0.0f),
            AZ::Vector3(2.0f, 1.0f, 0.0f),
        };
        mesh.m_triangles = {
            {.m_secondVertex = 1, .m_thirdVertex = 2, .m_userData = 37},
            {
                .m_firstVertex = 3,
                .m_secondVertex = 4,
                .m_thirdVertex = 5,
                .m_materialIndex = 1,
                .m_userData = 73,
            },
        };
        mesh.m_perTriangleUserData = true;
        configuration.m_geometry = AZStd::move(mesh);
        configuration.m_materials = {firstMaterialHandle, secondMaterialHandle};
        configuration.m_userData = 41;

        const CookedShapeHandle cookedShapeHandle = system.CookShape(configuration);
        ASSERT_TRUE(cookedShapeHandle);
        EXPECT_TRUE(system.IsValid(cookedShapeHandle));

        ShapeStats stats;
        ASSERT_TRUE(system.GetStats(cookedShapeHandle, stats));
        EXPECT_TRUE(stats.m_localBounds.IsValid());
        EXPECT_GT(stats.m_memorySize, 0);
        EXPECT_EQ(stats.m_triangleCount, 2);
        ShapeStats recursiveStats;
        ASSERT_TRUE(system.GetStatsRecursive(cookedShapeHandle, recursiveStats));
        EXPECT_GE(recursiveStats.m_memorySize, stats.m_memorySize);
        EXPECT_EQ(recursiveStats.m_triangleCount, stats.m_triangleCount);

        ShapeProperties properties;
        ASSERT_TRUE(system.GetProperties(cookedShapeHandle, properties));
        EXPECT_TRUE(properties.m_inertia.IsClose(AZ::Matrix3x3::CreateZero()));
        EXPECT_TRUE(properties.m_localBounds.IsValid());
        EXPECT_TRUE(properties.m_centerOfMass.IsZero());
        EXPECT_FLOAT_EQ(properties.m_innerRadius, 0.0f);
        EXPECT_FLOAT_EQ(properties.m_mass, 0.0f);
        EXPECT_FLOAT_EQ(properties.m_volume, 0.0f);
        EXPECT_GT(properties.m_subShapeIdBitCount, 0);
        EXPECT_EQ(properties.m_kind, ShapeKind::Mesh);
        EXPECT_TRUE(properties.m_mustBeStatic);

        CookedRaycastHit cookedHit;
        ASSERT_TRUE(
            system.Raycast(
                cookedShapeHandle,
                AZ::Vector3(-2.0f, 0.0f, 1.0f),
                -AZ::Vector3::CreateAxisZ(),
                2.0f,
                cookedHit));
        EXPECT_NEAR(cookedHit.m_position.GetZ(), 0.0f, 1.0e-5f);
        EXPECT_NEAR(cookedHit.m_distance, 1.0f, 1.0e-5f);
        EXPECT_EQ(cookedHit.m_materialHandle, firstMaterialHandle);

        const SubShapeId firstTriangleSubShapeId = cookedHit.m_subShapeId;
        CookedRaycastHit secondCookedHit;
        ASSERT_TRUE(
            system.Raycast(
                cookedShapeHandle,
                AZ::Vector3(2.0f, 0.0f, 1.0f),
                -AZ::Vector3::CreateAxisZ(),
                2.0f,
                secondCookedHit));
        EXPECT_EQ(secondCookedHit.m_materialHandle, secondMaterialHandle);

        BufferResult bufferResult = system.GetMeshMaterials(cookedShapeHandle, {});
        EXPECT_EQ(bufferResult.m_count, 0);
        EXPECT_EQ(bufferResult.m_requiredCount, 2);
        EXPECT_TRUE(bufferResult.HasOverflow());
        AZStd::array<MaterialHandle, 2> meshMaterials;
        bufferResult = system.GetMeshMaterials(cookedShapeHandle, meshMaterials);
        EXPECT_TRUE(bufferResult.IsComplete());
        EXPECT_EQ(meshMaterials[0], firstMaterialHandle);
        EXPECT_EQ(meshMaterials[1], secondMaterialHandle);

        AZ::u32 materialIndex = AZStd::numeric_limits<AZ::u32>::max();
        ASSERT_TRUE(system.GetMeshTriangleMaterialIndex(
            cookedShapeHandle,
            firstTriangleSubShapeId,
            materialIndex));
        EXPECT_EQ(materialIndex, 0);
        ASSERT_TRUE(system.GetMeshTriangleMaterialIndex(
            cookedShapeHandle,
            secondCookedHit.m_subShapeId,
            materialIndex));
        EXPECT_EQ(materialIndex, 1);

        CookedDecoratedShapeConfiguration decoratedConfiguration;
        decoratedConfiguration.m_geometry = CookedRotatedTranslatedShapeConfiguration{
            .m_shapeHandle = cookedShapeHandle,
            .m_position = 5.0f * AZ::Vector3::CreateAxisX(),
        };
        const CookedShapeHandle decoratedShapeHandle = system.CookShape(decoratedConfiguration);
        ASSERT_TRUE(decoratedShapeHandle);
        bufferResult = system.GetMeshMaterials(decoratedShapeHandle, meshMaterials);
        EXPECT_TRUE(bufferResult.IsComplete());
        EXPECT_EQ(meshMaterials[0], firstMaterialHandle);
        EXPECT_EQ(meshMaterials[1], secondMaterialHandle);
        CookedRaycastHit decoratedHit;
        ASSERT_TRUE(
            system.Raycast(
                decoratedShapeHandle,
                AZ::Vector3(7.0f, 0.0f, 1.0f),
                -AZ::Vector3::CreateAxisZ(),
                2.0f,
                decoratedHit));
        ASSERT_TRUE(system.GetMeshTriangleMaterialIndex(
            decoratedShapeHandle,
            decoratedHit.m_subShapeId,
            materialIndex));
        EXPECT_EQ(materialIndex, 1);

        AZ::u64 userData = 0;
        ASSERT_TRUE(system.GetUserData(cookedShapeHandle, userData));
        EXPECT_EQ(userData, 41);
        ASSERT_TRUE(system.GetSubShapeUserData(cookedShapeHandle, firstTriangleSubShapeId, userData));
        EXPECT_EQ(userData, 41);
        AZ::u32 triangleUserData = 0;
        ASSERT_TRUE(system.GetMeshTriangleUserData(
            cookedShapeHandle,
            firstTriangleSubShapeId,
            triangleUserData));
        EXPECT_EQ(triangleUserData, 37);
        ASSERT_TRUE(system.GetMeshTriangleUserData(
            cookedShapeHandle,
            secondCookedHit.m_subShapeId,
            triangleUserData));
        EXPECT_EQ(triangleUserData, 73);
        EXPECT_FALSE(
            system.Raycast(
                cookedShapeHandle,
                AZ::Vector3(-2.0f, 0.0f, 1.0f),
                AZ::Vector3::CreateAxisZ(),
                2.0f,
                cookedHit));

        const WorldHandle firstWorldHandle = system.GetDefaultWorldHandle();
        const WorldHandle secondWorldHandle = system.CreateWorld(WorldConfiguration{});
        ASSERT_TRUE(secondWorldHandle);
        const ShapeHandle firstShapeHandle = system.CreateShape(firstWorldHandle, cookedShapeHandle);
        const ShapeHandle secondShapeHandle = system.CreateShape(secondWorldHandle, cookedShapeHandle);
        ASSERT_TRUE(firstShapeHandle);
        ASSERT_TRUE(secondShapeHandle);

        ASSERT_TRUE(system.GetShapeUserData(firstWorldHandle, firstShapeHandle, userData));
        EXPECT_EQ(userData, 41);
        ASSERT_TRUE(system.GetShapeSubShapeUserData(
            firstWorldHandle,
            firstShapeHandle,
            firstTriangleSubShapeId,
            userData));
        EXPECT_EQ(userData, 41);
        ASSERT_TRUE(system.GetMeshTriangleUserData(
            firstWorldHandle,
            firstShapeHandle,
            firstTriangleSubShapeId,
            triangleUserData));
        EXPECT_EQ(triangleUserData, 37);

        bufferResult = system.GetMeshMaterials(firstWorldHandle, firstShapeHandle, {});
        EXPECT_EQ(bufferResult.m_requiredCount, 2);
        bufferResult = system.GetMeshMaterials(firstWorldHandle, firstShapeHandle, meshMaterials);
        EXPECT_TRUE(bufferResult.IsComplete());
        EXPECT_EQ(meshMaterials[0], firstMaterialHandle);
        EXPECT_EQ(meshMaterials[1], secondMaterialHandle);
        ASSERT_TRUE(system.GetMeshTriangleMaterialIndex(
            firstWorldHandle,
            firstShapeHandle,
            secondCookedHit.m_subShapeId,
            materialIndex));
        EXPECT_EQ(materialIndex, 1);

        EXPECT_FALSE(system.DestroyCookedShape(cookedShapeHandle));
        EXPECT_FALSE(system.DestroyMaterial(firstMaterialHandle));
        EXPECT_FALSE(system.DestroyMaterial(secondMaterialHandle));
        EXPECT_TRUE(system.DestroyShape(firstWorldHandle, firstShapeHandle));
        EXPECT_FALSE(system.DestroyCookedShape(cookedShapeHandle));
        EXPECT_TRUE(system.DestroyShape(secondWorldHandle, secondShapeHandle));
        EXPECT_TRUE(system.DestroyWorld(secondWorldHandle));
        EXPECT_TRUE(system.DestroyCookedShape(decoratedShapeHandle));
        EXPECT_TRUE(system.DestroyCookedShape(cookedShapeHandle));
        EXPECT_FALSE(system.IsValid(cookedShapeHandle));
        EXPECT_FALSE(system.GetStats(cookedShapeHandle, stats));
        EXPECT_TRUE(system.DestroyMaterial(firstMaterialHandle));
        EXPECT_TRUE(system.DestroyMaterial(secondMaterialHandle));
    }

    TEST(SimulationTests, CookingRejectsInvalidGeometryAndStaleHandles)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        ShapeConfiguration invalidConfiguration;
        invalidConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.0f};
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(system.CookShape(invalidConfiguration));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        ShapeConfiguration validConfiguration;
        validConfiguration.m_geometry = SphereShapeConfiguration{};
        const CookedShapeHandle firstHandle = system.CookShape(validConfiguration);
        ASSERT_TRUE(firstHandle);

        AZStd::array<MaterialHandle, 1> materialHandles = {MaterialHandle::Invalid};
        const BufferResult materialResult = system.GetMeshMaterials(firstHandle, materialHandles);
        EXPECT_EQ(materialResult.m_count, 0);
        EXPECT_EQ(materialResult.m_requiredCount, 0);
        EXPECT_FALSE(materialHandles.front());
        AZ::u32 materialIndex = AZStd::numeric_limits<AZ::u32>::max();
        EXPECT_FALSE(system.GetMeshTriangleMaterialIndex(
            firstHandle,
            SubShapeId::Root,
            materialIndex));
        EXPECT_EQ(materialIndex, AZStd::numeric_limits<AZ::u32>::max());

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle sphereShapeHandle = system.CreateShape(worldHandle, validConfiguration);
        ASSERT_TRUE(sphereShapeHandle);
        const BufferResult worldMaterialResult = system.GetMeshMaterials(
            worldHandle,
            sphereShapeHandle,
            materialHandles);
        EXPECT_EQ(worldMaterialResult.m_count, 0);
        EXPECT_EQ(worldMaterialResult.m_requiredCount, 0);
        EXPECT_FALSE(system.GetMeshTriangleMaterialIndex(
            worldHandle,
            sphereShapeHandle,
            SubShapeId::Root,
            materialIndex));
        EXPECT_TRUE(system.DestroyShape(worldHandle, sphereShapeHandle));

        EXPECT_TRUE(system.DestroyCookedShape(firstHandle));

        const CookedShapeHandle secondHandle = system.CookShape(validConfiguration);
        ASSERT_TRUE(secondHandle);
        EXPECT_NE(firstHandle, secondHandle);
        EXPECT_FALSE(system.CreateShape(system.GetDefaultWorldHandle(), firstHandle));
        EXPECT_TRUE(system.DestroyCookedShape(secondHandle));
    }

    TEST(SimulationTests, CustomConvexShapesCookOnceAndRemainNativeAtRuntime)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        TestCustomConvexShapeProvider provider;
        EXPECT_EQ(
            system.RegisterExtension(static_cast<ICustomConvexShapeProvider*>(nullptr), {}).m_status,
            ExtensionRegistrationStatus::Invalid);
        const ExtensionRegistrationResult providerRegistration = system.RegisterExtension(&provider, {});
        ASSERT_TRUE(providerRegistration);
        EXPECT_EQ(
            system.RegisterExtension(&provider, {}).m_status,
            ExtensionRegistrationStatus::AlreadyRegistered);

        const MaterialHandle materialHandle = system.CreateMaterial({
            .m_debugName = "Custom convex",
            .m_debugColor = AZ::Colors::Orange,
        });
        ASSERT_TRUE(materialHandle);

        ShapeConfiguration configuration;
        configuration.m_geometry = CustomConvexShapeConfiguration{
            .m_data = {4},
            .m_editorBounds = AZ::Aabb::CreateCenterRadius(AZ::Vector3::CreateZero(), 1.0f),
            .m_providerId = provider.GetId(),
        };
        configuration.m_materials = {materialHandle};
        configuration.m_userData = 91;

        const CookedShapeHandle cookedShapeHandle = system.CookShape(configuration);
        ASSERT_TRUE(cookedShapeHandle);
        EXPECT_EQ(provider.m_cookCount.load(), 1);

        ShapeProperties properties;
        ASSERT_TRUE(system.GetProperties(cookedShapeHandle, properties));
        EXPECT_EQ(properties.m_kind, ShapeKind::CustomConvex);
        EXPECT_FALSE(properties.m_mustBeStatic);
        EXPECT_GT(properties.m_innerRadius, 0.0f);
        EXPECT_GT(properties.m_volume, 0.0f);
        EXPECT_NEAR(
            properties.m_mass,
            properties.m_volume * configuration.m_density,
            1.0e-2f);
        EXPECT_TRUE(properties.m_localBounds.IsClose(
            AZ::Aabb::CreateCenterRadius(AZ::Vector3::CreateZero(), 1.0f),
            1.0e-5f));

        CustomConvexShapeInfo info;
        ASSERT_TRUE(system.GetCustomConvexShapeInfo(cookedShapeHandle, info));
        EXPECT_EQ(info.m_providerId, provider.GetId());
        EXPECT_EQ(info.m_providerVersion, provider.GetVersion());
        EXPECT_NE(info.m_sourceHash, 0);
        AZStd::array<CustomShapeDependency, 1> customDependencies;
        const BufferResult dependencyResult = system.GetCustomShapeDependencies(
            cookedShapeHandle,
            customDependencies);
        ASSERT_TRUE(dependencyResult.IsComplete());
        ASSERT_EQ(dependencyResult.m_count, 0);

        CookedRaycastHit hit;
        ASSERT_TRUE(system.Raycast(
            cookedShapeHandle,
            AZ::Vector3::CreateAxisZ(2.0f),
            -AZ::Vector3::CreateAxisZ(),
            4.0f,
            hit));
        EXPECT_NEAR(hit.m_distance, 1.0f, 1.0e-5f);

        CookedShapeArchive archive;
        AZStd::vector<MaterialHandle> materialHandles;
        AZStd::vector<CookedShapeHandle> childShapeHandles;
        ASSERT_TRUE(system.ExportShape(
            cookedShapeHandle,
            archive,
            materialHandles,
            childShapeHandles));
        EXPECT_TRUE(archive.m_dependencies.empty());

        ASSERT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::NotRegistered);

        const CookedShapeHandle importedShapeHandle = system.ImportShape(
            archive,
            materialHandles,
            childShapeHandles);
        ASSERT_TRUE(importedShapeHandle);
        ASSERT_TRUE(system.GetCustomConvexShapeInfo(importedShapeHandle, info));
        EXPECT_EQ(info.m_providerId, provider.GetId());
        EXPECT_EQ(info.m_providerVersion, provider.GetVersion());
        ASSERT_TRUE(system.GetCustomShapeDependencies(
            importedShapeHandle,
            customDependencies).IsComplete());
        EXPECT_EQ(provider.m_cookCount.load(), 1);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, importedShapeHandle);
        ASSERT_TRUE(shapeHandle);
        const BodyHandle bodyHandle = system.CreateBody(
            worldHandle,
            BodyConfiguration{
                .m_shapeHandle = shapeHandle,
                .m_motionType = MotionType::Static,
            });
        ASSERT_TRUE(bodyHandle);

        RaycastHit worldHit;
        ASSERT_TRUE(system.RaycastClosest(
            worldHandle,
            RaycastRequest{
                .m_start = {.m_z = 2.0},
                .m_displacement = -AZ::Vector3::CreateAxisZ(4.0f),
            },
            worldHit));
        EXPECT_EQ(worldHit.m_bodyHandle, bodyHandle);
        EXPECT_EQ(worldHit.m_materialHandle, materialHandle);
        EXPECT_EQ(worldHit.m_shapeHandle, shapeHandle);

        AZStd::array<ShapeOverlapHit, 1> overlapHits;
        const QueryResult overlapResult = system.CollideShape(
            worldHandle,
            ShapeOverlapRequest{
                .m_shapeHandle = shapeHandle,
            },
            overlapHits);
        ASSERT_EQ(overlapResult.m_hitCount, 1);
        EXPECT_EQ(overlapHits.front().m_bodyHandle, bodyHandle);
        EXPECT_EQ(overlapHits.front().m_materialHandle, materialHandle);

        ShapeCastHit castHit;
        ASSERT_TRUE(system.CastShapeClosest(
            worldHandle,
            ShapeCastRequest{
                .m_shapeHandle = shapeHandle,
                .m_start = {
                    .m_position = {.m_x = -3.0},
                },
                .m_displacement = AZ::Vector3::CreateAxisX(6.0f),
            },
            castHit));
        EXPECT_EQ(castHit.m_bodyHandle, bodyHandle);
        EXPECT_EQ(castHit.m_materialHandle, materialHandle);

        AZStd::array<WorldPosition, MaximumSupportingFaceVertexCount> supportingFace;
        const QueryResult supportingFaceResult = system.GetSupportingFace(
            worldHandle,
            SupportingFaceRequest{
                .m_bodyHandle = bodyHandle,
            },
            supportingFace);
        EXPECT_EQ(supportingFaceResult.m_hitCount, 4);
        EXPECT_TRUE(supportingFaceResult.IsComplete());

        AZStd::array<TransformedTriangle, 12> triangles;
        const QueryResult triangleResult = system.CollectTriangles(
            worldHandle,
            TriangleCollectionRequest{
                .m_bodyHandle = bodyHandle,
                .m_bounds = {
                    .m_halfExtents = AZ::Vector3(2.0f),
                },
            },
            triangles);
        EXPECT_EQ(triangleResult.m_hitCount, 12);
        EXPECT_TRUE(triangleResult.IsComplete());
        EXPECT_EQ(triangles.front().m_materialHandle, materialHandle);

        RecordingDebugRenderer renderer;
        ASSERT_TRUE(system.DrawDebug(worldHandle, {}, renderer));
        EXPECT_EQ(renderer.m_geometryCount, 1);
        EXPECT_GT(renderer.m_vertexCount, 0);

        WorldStateDigest capturedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, capturedDigest));
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);

        WorldTransform movedTransform;
        movedTransform.m_position.m_x = 3.0;
        ASSERT_TRUE(system.SetBodyTransform(
            worldHandle,
            bodyHandle,
            movedTransform,
            false));
        WorldStateDigest movedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, movedDigest));
        EXPECT_NE(movedDigest, capturedDigest);

        ASSERT_TRUE(system.RestoreWorldState(worldHandle, snapshotHandle));
        WorldStateDigest restoredDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, restoredDigest));
        EXPECT_EQ(restoredDigest, capturedDigest);
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));

        configuration.m_geometry = CustomConvexShapeConfiguration{
            .m_data = {0},
            .m_providerId = provider.GetId(),
        };
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(system.CookShape(configuration));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyCookedShape(importedShapeHandle));
        EXPECT_TRUE(system.DestroyCookedShape(cookedShapeHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, CustomShapesRetainProvidersAcrossCookingArchivesAndRuntimeUse)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        TestCustomShapeProvider provider;
        EXPECT_EQ(
            system.RegisterExtension(static_cast<ICustomShapeProvider*>(nullptr), {}).m_status,
            ExtensionRegistrationStatus::Invalid);
        provider.m_version = 0;
        EXPECT_EQ(
            system.RegisterExtension(&provider, {}).m_status,
            ExtensionRegistrationStatus::Invalid);
        provider.m_version = 19;
        ExtensionRegistrationResult providerRegistration = system.RegisterExtension(&provider, {});
        ASSERT_TRUE(providerRegistration);
        ExtensionInformation providerInformation;
        ASSERT_TRUE(system.GetExtensionInformation(providerRegistration.m_handle, providerInformation));
        EXPECT_EQ(providerInformation.m_version, 19);
        EXPECT_EQ(
            system.RegisterExtension(&provider, {}).m_status,
            ExtensionRegistrationStatus::AlreadyRegistered);
        provider.m_version = 23;
        ASSERT_TRUE(system.GetExtensionInformation(providerRegistration.m_handle, providerInformation));
        EXPECT_EQ(providerInformation.m_version, 19);

        ShapeConfiguration invalidConfiguration;
        invalidConfiguration.m_geometry = CustomShapeConfiguration{
            .m_data = {0},
            .m_providerId = provider.GetId(),
        };
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(system.CookShape(invalidConfiguration));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        const MaterialHandle materialHandle = system.CreateMaterial({
            .m_debugName = "General custom shape",
            .m_debugColor = AZ::Colors::Purple,
        });
        ASSERT_TRUE(materialHandle);
        const MaterialHandle secondMaterialHandle = system.CreateMaterial({
            .m_debugName = "General custom mesh triangle",
            .m_debugColor = AZ::Colors::Cyan,
        });
        ASSERT_TRUE(secondMaterialHandle);

        ShapeConfiguration configuration;
        configuration.m_geometry = CustomShapeConfiguration{
            .m_data = {4},
            .m_editorBounds = AZ::Aabb::CreateCenterRadius(AZ::Vector3::CreateZero(), 1.0f),
            .m_providerId = provider.GetId(),
        };
        configuration.m_materials = {materialHandle};
        configuration.m_userData = 0xABCDEF;

        const CookedShapeHandle cookedShapeHandle = system.CookShape(configuration);
        ASSERT_TRUE(cookedShapeHandle);
        EXPECT_EQ(provider.m_cookCount.load(), 2);
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::InUse);

        ShapeProperties properties;
        ASSERT_TRUE(system.GetProperties(cookedShapeHandle, properties));
        EXPECT_EQ(properties.m_kind, ShapeKind::Custom);
        EXPECT_FALSE(properties.m_mustBeStatic);
        EXPECT_GT(properties.m_innerRadius, 0.0f);
        EXPECT_GT(properties.m_volume, 0.0f);

        AZ::u64 subShapeUserData = 0;
        ASSERT_TRUE(system.GetSubShapeUserData(
            cookedShapeHandle,
            SubShapeId::Root,
            subShapeUserData));
        EXPECT_EQ(subShapeUserData, configuration.m_userData);

        CustomShapeInfo info;
        ASSERT_TRUE(system.GetCustomShapeInfo(cookedShapeHandle, info));
        EXPECT_EQ(info.m_providerId, provider.GetId());
        EXPECT_EQ(info.m_providerVersion, providerInformation.m_version);
        EXPECT_NE(info.m_sourceHash, 0);

        ShapeConfiguration meshConfiguration;
        meshConfiguration.m_geometry = CustomShapeConfiguration{
            .m_data = {5},
            .m_editorBounds = AZ::Aabb::CreateCenterHalfExtents(
                AZ::Vector3::CreateZero(),
                AZ::Vector3(1.0f, 1.0f, 0.01f)),
            .m_providerId = provider.GetId(),
        };
        meshConfiguration.m_materials = {materialHandle, secondMaterialHandle};
        const CookedShapeHandle meshShapeHandle = system.CookShape(meshConfiguration);
        ASSERT_TRUE(meshShapeHandle);
        EXPECT_EQ(provider.m_cookCount.load(), 3);

        ASSERT_TRUE(system.GetProperties(meshShapeHandle, properties));
        EXPECT_EQ(properties.m_kind, ShapeKind::Custom);
        EXPECT_TRUE(properties.m_mustBeStatic);

        CookedRaycastHit meshHit;
        ASSERT_TRUE(system.Raycast(
            meshShapeHandle,
            {0.5f, -0.5f, 1.0f},
            -AZ::Vector3::CreateAxisZ(),
            2.0f,
            meshHit));
        EXPECT_EQ(meshHit.m_materialHandle, materialHandle);
        ASSERT_TRUE(system.GetSubShapeUserData(
            meshShapeHandle,
            meshHit.m_subShapeId,
            subShapeUserData));
        EXPECT_EQ(subShapeUserData, 41);

        ASSERT_TRUE(system.Raycast(
            meshShapeHandle,
            {-0.5f, 0.5f, 1.0f},
            -AZ::Vector3::CreateAxisZ(),
            2.0f,
            meshHit));
        EXPECT_EQ(meshHit.m_materialHandle, secondMaterialHandle);
        ASSERT_TRUE(system.GetSubShapeUserData(
            meshShapeHandle,
            meshHit.m_subShapeId,
            subShapeUserData));
        EXPECT_EQ(subShapeUserData, 42);

        CookedShapeArchive archive;
        AZStd::vector<MaterialHandle> materialHandles;
        AZStd::vector<CookedShapeHandle> childShapeHandles;
        ASSERT_TRUE(system.ExportShape(
            cookedShapeHandle,
            archive,
            materialHandles,
            childShapeHandles));

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, cookedShapeHandle);
        ASSERT_TRUE(shapeHandle);
        const BodyHandle bodyHandle = system.CreateBody(
            worldHandle,
            BodyConfiguration{
                .m_shapeHandle = shapeHandle,
                .m_motionType = MotionType::Static,
            });
        ASSERT_TRUE(bodyHandle);

        RaycastHit raycastHit;
        ASSERT_TRUE(system.RaycastClosest(
            worldHandle,
            RaycastRequest{
                .m_start = {.m_z = 2.0},
                .m_displacement = -AZ::Vector3::CreateAxisZ(4.0f),
            },
            raycastHit));
        EXPECT_EQ(raycastHit.m_bodyHandle, bodyHandle);
        EXPECT_EQ(raycastHit.m_materialHandle, materialHandle);
        EXPECT_EQ(raycastHit.m_shapeHandle, shapeHandle);

        TransformedShape retainedShape;
        ASSERT_TRUE(system.RetainShape(
            worldHandle,
            shapeHandle,
            {},
            1.0f,
            retainedShape));
        EXPECT_EQ(retainedShape.GetUserData(), configuration.m_userData);

        ShapeConfiguration sphereConfiguration;
        sphereConfiguration.m_geometry = SphereShapeConfiguration{
            .m_radius = 0.5f,
        };
        const CookedShapeHandle sphereCookedShapeHandle = system.CookShape(sphereConfiguration);
        ASSERT_TRUE(sphereCookedShapeHandle);
        const ShapeHandle sphereShapeHandle = system.CreateShape(
            worldHandle,
            sphereCookedShapeHandle);
        ASSERT_TRUE(sphereShapeHandle);
        TransformedShape retainedSphere;
        WorldTransform sphereTransform;
        sphereTransform.m_position.m_x = 10.0;
        ASSERT_TRUE(system.RetainShape(
            worldHandle,
            sphereShapeHandle,
            sphereTransform,
            1.0f,
            retainedSphere));

        provider.m_useDispatch = true;
        AZStd::array<TransformedShapeCollisionHit, 2> collisionHits;
        AZStd::array<WorldPosition, MaximumSupportingFaceVertexCount * 2> firstFaceVertices;
        AZStd::array<WorldPosition, MaximumSupportingFaceVertexCount * 2> secondFaceVertices;
        QueryResult pairResult = system.CollideTransformedShapes(
            worldHandle,
            retainedShape,
            retainedSphere,
            {
                .m_faceCollectionMode = FaceCollectionMode::Collect,
            },
            collisionHits,
            {
                .m_queryVertices = firstFaceVertices,
                .m_targetVertices = secondFaceVertices,
            });
        ASSERT_TRUE(pairResult.IsComplete());
        ASSERT_EQ(pairResult.m_hitCount, 1);
        EXPECT_FLOAT_EQ(collisionHits[0].m_penetrationDepth, 0.75f);
        EXPECT_EQ(collisionHits[0].m_firstFaceVertexCount, 3);
        EXPECT_EQ(provider.m_collisionCount.load(), 1);
        EXPECT_TRUE(provider.m_viewEvidence.load());

        RecordingTransformedShapeCollisionCollector collisionCollector;
        EXPECT_TRUE(system.CollideTransformedShapes(
            worldHandle,
            retainedShape,
            retainedSphere,
            {
                .m_faceCollectionMode = FaceCollectionMode::Collect,
            },
            collisionCollector));
        EXPECT_EQ(collisionCollector.m_hitCount, 1);
        EXPECT_EQ(collisionCollector.m_firstFaceVertexCount, 3);
        EXPECT_FLOAT_EQ(collisionCollector.m_hit.m_penetrationDepth, 0.75f);

        RecordingTransformedShapeCollisionCollector invalidCollisionCollector;
        invalidCollisionCollector.m_earlyOutFraction =
            AZStd::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(system.CollideTransformedShapes(
            worldHandle,
            retainedShape,
            retainedSphere,
            {},
            invalidCollisionCollector));
        EXPECT_EQ(invalidCollisionCollector.m_hitCount, 0);

        pairResult = system.CollideTransformedShapes(
            worldHandle,
            retainedSphere,
            retainedShape,
            {
                .m_faceCollectionMode = FaceCollectionMode::Collect,
            },
            collisionHits,
            {
                .m_queryVertices = firstFaceVertices,
                .m_targetVertices = secondFaceVertices,
            });
        ASSERT_TRUE(pairResult.IsComplete());
        ASSERT_EQ(pairResult.m_hitCount, 1);
        EXPECT_EQ(provider.m_collisionCount.load(), 3);
        EXPECT_TRUE(provider.m_viewEvidence.load());

        TransformedShape retainedCustomCopy;
        WorldTransform customCopyTransform;
        customCopyTransform.m_position.m_y = 10.0;
        ASSERT_TRUE(system.RetainShape(
            worldHandle,
            shapeHandle,
            customCopyTransform,
            1.0f,
            retainedCustomCopy));
        pairResult = system.CollideTransformedShapes(
            worldHandle,
            retainedShape,
            retainedCustomCopy,
            {
                .m_faceCollectionMode = FaceCollectionMode::Collect,
            },
            collisionHits,
            {
                .m_queryVertices = firstFaceVertices,
                .m_targetVertices = secondFaceVertices,
            });
        ASSERT_TRUE(pairResult.IsComplete());
        ASSERT_EQ(pairResult.m_hitCount, 1);
        EXPECT_EQ(provider.m_collisionCount.load(), 4);
        EXPECT_TRUE(provider.m_viewEvidence.load());

        AZStd::array<TransformedShapeCastHit, 2> castHits;
        const QueryResult castResult = system.CastTransformedShape(
            worldHandle,
            retainedShape,
            retainedSphere,
            {
                .m_displacement = AZ::Vector3::CreateAxisX(4.0f),
            },
            castHits,
            {});
        ASSERT_TRUE(castResult.IsComplete());
        ASSERT_EQ(castResult.m_hitCount, 1);
        EXPECT_FLOAT_EQ(castHits[0].m_fraction, 0.25f);
        EXPECT_EQ(provider.m_castCount.load(), 1);
        EXPECT_TRUE(provider.m_castEvidence.load());

        RecordingTransformedShapeCastCollector castCollector;
        EXPECT_TRUE(system.CastTransformedShape(
            worldHandle,
            retainedShape,
            retainedSphere,
            {
                .m_displacement = AZ::Vector3::CreateAxisX(4.0f),
            },
            castCollector));
        EXPECT_EQ(castCollector.m_hitCount, 1);
        EXPECT_FLOAT_EQ(castCollector.m_hit.m_fraction, 0.25f);

        const QueryResult reverseCastResult = system.CastTransformedShape(
            worldHandle,
            retainedSphere,
            retainedShape,
            {
                .m_displacement = AZ::Vector3::CreateAxisX(4.0f),
            },
            castHits,
            {});
        ASSERT_TRUE(reverseCastResult.IsComplete());
        EXPECT_EQ(reverseCastResult.m_hitCount, 1);
        EXPECT_EQ(provider.m_castCount.load(), 3);

        provider.m_returnInvalidOutput = true;
        AZ_TEST_START_TRACE_SUPPRESSION;
        pairResult = system.CollideTransformedShapes(
            worldHandle,
            retainedShape,
            retainedSphere,
            {
                .m_faceCollectionMode = FaceCollectionMode::Collect,
            },
            collisionHits,
            {
                .m_queryVertices = firstFaceVertices,
                .m_targetVertices = secondFaceVertices,
            });
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(pairResult.m_hitCount, 0);

        RecordingTransformedShapeCollisionCollector invalidProviderCollisionCollector;
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_TRUE(system.CollideTransformedShapes(
            worldHandle,
            retainedShape,
            retainedSphere,
            {},
            invalidProviderCollisionCollector));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(invalidProviderCollisionCollector.m_hitCount, 0);

        RecordingTransformedShapeCastCollector invalidProviderCastCollector;
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_TRUE(system.CastTransformedShape(
            worldHandle,
            retainedShape,
            retainedSphere,
            {
                .m_displacement = AZ::Vector3::CreateAxisX(4.0f),
            },
            invalidProviderCastCollector));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(invalidProviderCastCollector.m_hitCount, 0);

        provider.m_returnInvalidOutput = false;

        provider.m_returnOverwideSubShapeId = true;
        AZ_TEST_START_TRACE_SUPPRESSION;
        pairResult = system.CollideTransformedShapes(
            worldHandle,
            retainedShape,
            retainedSphere,
            {},
            collisionHits,
            {});
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(pairResult.m_hitCount, 0);

        AZ_TEST_START_TRACE_SUPPRESSION;
        const QueryResult overwideCastResult = system.CastTransformedShape(
            worldHandle,
            retainedShape,
            retainedSphere,
            {
                .m_displacement = AZ::Vector3::CreateAxisX(4.0f),
            },
            castHits,
            {});
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(overwideCastResult.m_hitCount, 0);
        provider.m_returnOverwideSubShapeId = false;

        provider.m_dispatchHitCount = MaximumCustomShapeDispatchHitCount + 1;
        AZ_TEST_START_TRACE_SUPPRESSION;
        pairResult = system.CollideTransformedShapes(
            worldHandle,
            retainedShape,
            retainedSphere,
            {},
            collisionHits,
            {});
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(pairResult.m_hitCount, 0);

        AZ_TEST_START_TRACE_SUPPRESSION;
        const QueryResult excessiveCastResult = system.CastTransformedShape(
            worldHandle,
            retainedShape,
            retainedSphere,
            {
                .m_displacement = AZ::Vector3::CreateAxisX(4.0f),
            },
            castHits,
            {});
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(excessiveCastResult.m_hitCount, 0);
        provider.m_dispatchHitCount = 1;
        provider.m_useDispatch = false;

        retainedShape = {};
        retainedSphere = {};
        retainedCustomCopy = {};

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyCookedShape(meshShapeHandle));
        EXPECT_TRUE(system.DestroyCookedShape(cookedShapeHandle));
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::Success);

        EXPECT_FALSE(system.ImportShape(
            archive,
            materialHandles,
            childShapeHandles));
        provider.m_version = providerInformation.m_version;
        providerRegistration = system.RegisterExtension(&provider, {});
        ASSERT_TRUE(providerRegistration);

        const CookedShapeHandle importedShapeHandle = system.ImportShape(
            archive,
            materialHandles,
            childShapeHandles);
        ASSERT_TRUE(importedShapeHandle);
        ASSERT_TRUE(system.GetCustomShapeInfo(importedShapeHandle, info));
        EXPECT_EQ(info.m_providerId, provider.GetId());
        EXPECT_EQ(info.m_providerVersion, provider.GetVersion());
        EXPECT_EQ(provider.m_cookCount.load(), 3);
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::InUse);

        const ShapeHandle importedWorldShapeHandle = system.CreateShape(
            worldHandle,
            importedShapeHandle);
        ASSERT_TRUE(importedWorldShapeHandle);
        ASSERT_TRUE(system.RetainShape(
            worldHandle,
            importedWorldShapeHandle,
            {},
            1.0f,
            retainedShape));
        ASSERT_TRUE(system.RetainShape(
            worldHandle,
            sphereShapeHandle,
            sphereTransform,
            1.0f,
            retainedSphere));
        provider.m_useDispatch = true;
        pairResult = system.CollideTransformedShapes(
            worldHandle,
            retainedShape,
            retainedSphere,
            {
                .m_faceCollectionMode = FaceCollectionMode::Collect,
            },
            collisionHits,
            {
                .m_queryVertices = firstFaceVertices,
                .m_targetVertices = secondFaceVertices,
            });
        ASSERT_TRUE(pairResult.IsComplete());
        EXPECT_EQ(pairResult.m_hitCount, 1);
        EXPECT_EQ(provider.m_collisionCount.load(), 9);
        EXPECT_TRUE(provider.m_viewEvidence.load());

        AZStd::atomic_bool beginConcurrentQueries{false};
        AZStd::atomic_bool firstConcurrentQuerySucceeded{true};
        AZStd::atomic_bool secondConcurrentQuerySucceeded{true};
        AZStd::atomic_bool firstFloatEnvironmentRestored{false};
        AZStd::atomic_bool secondFloatEnvironmentRestored{false};
        const auto runConcurrentQueries =
            [&](const int roundingMode,
                AZStd::atomic_bool& succeeded,
                AZStd::atomic_bool& restored)
            {
                std::fesetround(roundingMode);
                while (!beginConcurrentQueries.load(AZStd::memory_order_acquire))
                {
                    AZStd::this_thread::yield();
                }

                for (AZ::u32 queryIndex = 0; queryIndex < 128; ++queryIndex)
                {
                    AZStd::array<TransformedShapeCollisionHit, 1> concurrentHits;
                    AZStd::array<WorldPosition, 4> concurrentFirstFaceVertices;
                    AZStd::array<WorldPosition, 4> concurrentSecondFaceVertices;
                    const QueryResult concurrentResult = system.CollideTransformedShapes(
                        worldHandle,
                        retainedShape,
                        retainedSphere,
                        {
                            .m_faceCollectionMode = FaceCollectionMode::Collect,
                        },
                        concurrentHits,
                        {
                            .m_queryVertices = concurrentFirstFaceVertices,
                            .m_targetVertices = concurrentSecondFaceVertices,
                        });
                    if (!concurrentResult.IsComplete()
                        || concurrentResult.m_hitCount != 1
                        || concurrentHits.front().m_penetrationDepth != 0.75f)
                    {
                        succeeded.store(false, AZStd::memory_order_release);
                        break;
                    }
                }

                restored.store(
                    std::fegetround() == roundingMode,
                    AZStd::memory_order_release);
            };
        AZStd::thread firstQueryThread(
            runConcurrentQueries,
            FE_DOWNWARD,
            AZStd::ref(firstConcurrentQuerySucceeded),
            AZStd::ref(firstFloatEnvironmentRestored));
        AZStd::thread secondQueryThread(
            runConcurrentQueries,
            FE_UPWARD,
            AZStd::ref(secondConcurrentQuerySucceeded),
            AZStd::ref(secondFloatEnvironmentRestored));
        beginConcurrentQueries.store(true, AZStd::memory_order_release);
        firstQueryThread.join();
        secondQueryThread.join();

        EXPECT_TRUE(firstConcurrentQuerySucceeded.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(secondConcurrentQuerySucceeded.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(firstFloatEnvironmentRestored.load(AZStd::memory_order_acquire));
        EXPECT_TRUE(secondFloatEnvironmentRestored.load(AZStd::memory_order_acquire));
        EXPECT_EQ(provider.m_collisionCount.load(), 265);

        provider.m_useDispatch = false;
        retainedShape = {};
        retainedSphere = {};
        EXPECT_TRUE(system.DestroyShape(worldHandle, importedWorldShapeHandle));

        EXPECT_TRUE(system.DestroyCookedShape(importedShapeHandle));
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::NotRegistered);
        EXPECT_TRUE(system.DestroyShape(worldHandle, sphereShapeHandle));
        EXPECT_TRUE(system.DestroyCookedShape(sphereCookedShapeHandle));
        EXPECT_TRUE(system.DestroyMaterial(secondMaterialHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, ClosestRaycastBatchesFromIndependentCallersRunConcurrently)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_floorBodyHandle);
        ASSERT_TRUE(scene.m_sphereBodyHandle);

        BlockingQueryFilter filter;
        AZStd::array<RaycastRequest, 1> firstRequests;
        firstRequests[0].m_start.m_z = 5.0;
        firstRequests[0].m_displacement = -10.0f * AZ::Vector3::CreateAxisZ();
        firstRequests[0].m_filter.m_callback = &filter;
        const AZStd::array<RaycastRequest, 1> secondRequests = firstRequests;
        AZStd::array<ClosestRaycastResult, 1> firstResults;
        AZStd::array<ClosestRaycastResult, 1> secondResults;
        BufferResult firstBatchResult;
        BufferResult secondBatchResult;

        AZStd::thread firstThread(
            [&]()
            {
                firstBatchResult = system.RaycastClosestBatch(
                    scene.m_worldHandle,
                    firstRequests,
                    firstResults);
            });
        AZStd::thread secondThread(
            [&]()
            {
                secondBatchResult = system.RaycastClosestBatch(
                    scene.m_worldHandle,
                    secondRequests,
                    secondResults);
            });

        const bool bothBatchesEntered = WaitUntil(
            [&filter]()
            {
                return filter.m_enterCount.load(AZStd::memory_order_acquire) >= 2;
            });
        filter.m_release.store(true, AZStd::memory_order_release);
        firstThread.join();
        secondThread.join();

        EXPECT_TRUE(bothBatchesEntered);
        EXPECT_TRUE(firstBatchResult.IsComplete());
        EXPECT_TRUE(secondBatchResult.IsComplete());
        EXPECT_TRUE(firstResults[0].m_found);
        EXPECT_TRUE(secondResults[0].m_found);

        DestroySphereOnFloor(system, scene);
    }

    TEST(SimulationTests, CustomConstraintsBatchRowsAndRetainProvidersUntilDestruction)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        TestCustomConstraintProvider provider;
        EXPECT_EQ(
            system.RegisterExtension(static_cast<ICustomConstraintProvider*>(nullptr), {}).m_status,
            ExtensionRegistrationStatus::Invalid);
        const ExtensionRegistrationResult providerRegistration = system.RegisterExtension(&provider, {});
        ASSERT_TRUE(providerRegistration);
        EXPECT_EQ(
            system.RegisterExtension(&provider, {}).m_status,
            ExtensionRegistrationStatus::AlreadyRegistered);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.25f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration firstBodyConfiguration;
        firstBodyConfiguration.m_shapeHandle = shapeHandle;
        firstBodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, firstBodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);

        BodyConfiguration secondBodyConfiguration;
        secondBodyConfiguration.m_shapeHandle = shapeHandle;
        secondBodyConfiguration.m_transform.m_position.m_z = 2.0;
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, secondBodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        CustomConstraintConfiguration customConfiguration;
        customConfiguration.m_data = {1};
        customConfiguration.m_firstFrame.m_position.m_z = 1.0;
        customConfiguration.m_secondFrame.m_position.m_z = 1.0;
        customConfiguration.m_providerId = provider.GetId();
        customConfiguration.m_space = ConstraintSpace::World;

        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = firstBodyHandle;
        constraintConfiguration.m_secondBodyHandle = secondBodyHandle;
        constraintConfiguration.m_geometry = customConfiguration;
        const ConstraintHandle constraintHandle = system.CreateConstraint(
            worldHandle,
            constraintConfiguration);
        ASSERT_TRUE(constraintHandle);
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::InUse);

        ConstraintState constraintState;
        ASSERT_TRUE(system.GetConstraintState(worldHandle, constraintHandle, constraintState));
        EXPECT_EQ(constraintState.m_kind, ConstraintKind::Custom);

        ConstraintConfiguration returnedConfiguration;
        ASSERT_TRUE(system.GetConstraintConfiguration(
            worldHandle,
            constraintHandle,
            returnedConfiguration));
        const auto* returnedCustom = AZStd::get_if<CustomConstraintConfiguration>(
            &returnedConfiguration.m_geometry);
        ASSERT_TRUE(returnedCustom);
        EXPECT_EQ(returnedCustom->m_providerId, provider.GetId());
        EXPECT_EQ(returnedCustom->m_data, customConfiguration.m_data);
        EXPECT_EQ(returnedCustom->m_space, ConstraintSpace::LocalToCenterOfMass);

        CustomConstraintInfo customInfo;
        ASSERT_TRUE(system.GetCustomConstraintInfo(
            worldHandle,
            constraintHandle,
            customInfo));
        EXPECT_EQ(customInfo.m_providerId, provider.GetId());
        EXPECT_EQ(customInfo.m_providerVersion, provider.GetVersion());
        EXPECT_EQ(customInfo.m_maximumRowCount, 1);
        EXPECT_EQ(customInfo.m_stateByteCount, 1);

        ASSERT_TRUE(system.AddImpulse(
            worldHandle,
            secondBodyHandle,
            AZ::Vector3::CreateAxisZ(10.0f)));
        for (AZ::u32 step = 0; step < 30; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }
        EXPECT_EQ(provider.m_velocityCallCount.load(), 30);
        EXPECT_GT(provider.m_positionCallCount.load(), 0);

        BodyState secondBodyState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, secondBodyHandle, secondBodyState));
        EXPECT_NEAR(secondBodyState.m_transform.m_position.m_z, 2.0, 1.0e-4);

        ConstraintMeasurements measurements;
        ASSERT_TRUE(system.GetConstraintMeasurements(worldHandle, constraintHandle, measurements));
        const auto* customMeasurements = AZStd::get_if<CustomConstraintMeasurements>(&measurements);
        ASSERT_TRUE(customMeasurements);
        EXPECT_EQ(customMeasurements->m_rowCount, 1);

        AZStd::array<float, 1> customImpulses = {};
        BufferResult impulseResult = system.GetCustomConstraintImpulses(
            worldHandle,
            constraintHandle,
            {});
        EXPECT_EQ(impulseResult.m_count, 0);
        EXPECT_EQ(impulseResult.m_requiredCount, 1);
        impulseResult = system.GetCustomConstraintImpulses(
            worldHandle,
            constraintHandle,
            customImpulses);
        EXPECT_EQ(impulseResult.m_count, 1);
        EXPECT_TRUE(AZ::IsFiniteFloat(customImpulses.front()));

        AZStd::array<AZ::u8, 1> customState = {};
        BufferResult stateResult = system.GetCustomConstraintState(
            worldHandle,
            constraintHandle,
            {});
        EXPECT_EQ(stateResult.m_count, 0);
        EXPECT_EQ(stateResult.m_requiredCount, 1);
        stateResult = system.GetCustomConstraintState(
            worldHandle,
            constraintHandle,
            customState);
        EXPECT_EQ(stateResult.m_count, 1);
        EXPECT_EQ(customState.front(), 30);

        customState.front() = 7;
        ASSERT_TRUE(system.SetCustomConstraintState(
            worldHandle,
            constraintHandle,
            customState));

        WorldStateDigest capturedDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, capturedDigest));
        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);
        ASSERT_TRUE(system.AddImpulse(
            worldHandle,
            secondBodyHandle,
            AZ::Vector3::CreateAxisZ(5.0f)));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        ASSERT_TRUE(system.RestoreWorldState(worldHandle, snapshotHandle));
        WorldStateDigest restoredDigest;
        ASSERT_TRUE(system.GetWorldStateDigest(worldHandle, restoredDigest));
        EXPECT_EQ(restoredDigest, capturedDigest);
        customState.front() = 0;
        stateResult = system.GetCustomConstraintState(
            worldHandle,
            constraintHandle,
            customState);
        EXPECT_EQ(stateResult.m_count, 1);
        EXPECT_EQ(customState.front(), 7);

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::Success);

        customConfiguration.m_data = {0};
        constraintConfiguration.m_geometry = customConfiguration;
        EXPECT_FALSE(system.CreateConstraint(worldHandle, constraintConfiguration));

        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, CookedShapeArchivesRoundTripMaterialsAndRejectMismatchedData)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const MaterialHandle materialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(materialHandle);

        ShapeConfiguration configuration;
        configuration.m_geometry = SphereShapeConfiguration{.m_radius = 2.0f};
        configuration.m_materials = {materialHandle};
        configuration.m_userData = 73;
        const CookedShapeHandle sourceHandle = system.CookShape(configuration);
        ASSERT_TRUE(sourceHandle);

        CookedShapeArchive archive;
        AZStd::vector<MaterialHandle> materialHandles;
        AZStd::vector<CookedShapeHandle> childShapeHandles;
        ASSERT_TRUE(system.ExportShape(sourceHandle, archive, materialHandles, childShapeHandles));
        EXPECT_FALSE(archive.m_binaryState.empty());
        EXPECT_EQ(archive.m_materialCount, 1);
        EXPECT_EQ(archive.m_childShapeCount, 0);
        ASSERT_EQ(materialHandles.size(), 1);
        EXPECT_EQ(materialHandles.front(), materialHandle);
        EXPECT_TRUE(childShapeHandles.empty());

        const CookedShapeHandle importedHandle = system.ImportShape(
            archive,
            materialHandles,
            childShapeHandles);
        ASSERT_TRUE(importedHandle);

        CookedRaycastHit hit;
        ASSERT_TRUE(
            system.Raycast(
                importedHandle,
                AZ::Vector3::CreateAxisZ(3.0f),
                -AZ::Vector3::CreateAxisZ(),
                6.0f,
                hit));
        EXPECT_EQ(hit.m_materialHandle, materialHandle);
        EXPECT_NEAR(hit.m_distance, 1.0f, 1.0e-5f);
        AZ::u64 userData = 0;
        ASSERT_TRUE(system.GetUserData(importedHandle, userData));
        EXPECT_EQ(userData, 73);
        ASSERT_TRUE(system.GetSubShapeUserData(importedHandle, hit.m_subShapeId, userData));
        EXPECT_EQ(userData, 73);
        EXPECT_FALSE(system.DestroyMaterial(materialHandle));

        CookedShapeArchive corruptArchive = archive;
        corruptArchive.m_binaryState.back() ^= 0xff;
        EXPECT_FALSE(system.ImportShape(corruptArchive, materialHandles, childShapeHandles));

        CookedShapeArchive truncatedArchive = archive;
        truncatedArchive.m_binaryState.pop_back();
        truncatedArchive.m_contentHash = static_cast<AZ::u64>(AZ::TypeHash64(
            truncatedArchive.m_binaryState.data(),
            truncatedArchive.m_binaryState.size()));
        EXPECT_FALSE(system.ImportShape(truncatedArchive, materialHandles, childShapeHandles));

        CookedShapeArchive mismatchedArchive = archive;
        ++mismatchedArchive.m_buildFingerprint;
        EXPECT_FALSE(system.ImportShape(mismatchedArchive, materialHandles, childShapeHandles));

        mismatchedArchive = archive;
        ++mismatchedArchive.m_formatVersion;
        EXPECT_FALSE(system.ImportShape(mismatchedArchive, materialHandles, childShapeHandles));
        EXPECT_FALSE(system.ImportShape(archive, {}, childShapeHandles));

        EXPECT_TRUE(system.DestroyCookedShape(sourceHandle));
        EXPECT_TRUE(system.DestroyCookedShape(importedHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, CookedShapeArchivesRemapCompoundDependenciesAndRetainThem)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        ShapeConfiguration boxConfiguration;
        boxConfiguration.m_geometry = BoxShapeConfiguration{};
        boxConfiguration.m_userData = 11;
        const CookedShapeHandle sourceBoxHandle = system.CookShape(boxConfiguration);
        ASSERT_TRUE(sourceBoxHandle);

        CookedShapeArchive boxArchive;
        AZStd::vector<MaterialHandle> boxMaterials;
        AZStd::vector<CookedShapeHandle> boxChildren;
        ASSERT_TRUE(system.ExportShape(sourceBoxHandle, boxArchive, boxMaterials, boxChildren));
        ASSERT_EQ(boxMaterials.size(), 1);
        EXPECT_FALSE(boxMaterials.front());
        const CookedShapeHandle importedBoxHandle = system.ImportShape(boxArchive, boxMaterials, boxChildren);
        ASSERT_TRUE(importedBoxHandle);

        CookedDecoratedShapeConfiguration scaledConfiguration;
        scaledConfiguration.m_geometry = CookedScaledShapeConfiguration{
            .m_shapeHandle = sourceBoxHandle,
            .m_scale = AZ::Vector3(2.0f, 1.0f, 0.5f),
        };
        scaledConfiguration.m_userData = 13;
        const CookedShapeHandle sourceScaledHandle = system.CookShape(scaledConfiguration);
        ASSERT_TRUE(sourceScaledHandle);

        CookedShapeArchive scaledArchive;
        AZStd::vector<MaterialHandle> scaledMaterials;
        AZStd::vector<CookedShapeHandle> scaledChildren;
        ASSERT_TRUE(system.ExportShape(sourceScaledHandle, scaledArchive, scaledMaterials, scaledChildren));
        ASSERT_EQ(scaledChildren.size(), 1);
        EXPECT_EQ(scaledChildren.front(), sourceBoxHandle);
        const AZStd::array importedScaledChildren = {importedBoxHandle};
        const CookedShapeHandle importedScaledHandle = system.ImportShape(
            scaledArchive,
            scaledMaterials,
            importedScaledChildren);
        ASSERT_TRUE(importedScaledHandle);

        CookedCompoundShapeConfiguration compoundConfiguration;
        compoundConfiguration.m_children = {
            {
                .m_position = -AZ::Vector3::CreateAxisX(2.0f),
                .m_shapeHandle = sourceScaledHandle,
                .m_userData = 19,
            },
            {
                .m_position = AZ::Vector3::CreateAxisX(2.0f),
                .m_shapeHandle = sourceBoxHandle,
                .m_userData = 23,
            },
        };
        compoundConfiguration.m_userData = 17;
        const CookedShapeHandle sourceCompoundHandle = system.CookShape(compoundConfiguration);
        ASSERT_TRUE(sourceCompoundHandle);

        CookedShapeArchive compoundArchive;
        AZStd::vector<MaterialHandle> compoundMaterials;
        AZStd::vector<CookedShapeHandle> compoundChildren;
        ASSERT_TRUE(system.ExportShape(
            sourceCompoundHandle,
            compoundArchive,
            compoundMaterials,
            compoundChildren));
        ASSERT_EQ(compoundChildren.size(), 2);
        const AZStd::array importedCompoundChildren = {importedScaledHandle, importedBoxHandle};
        const CookedShapeHandle importedCompoundHandle = system.ImportShape(
            compoundArchive,
            compoundMaterials,
            importedCompoundChildren);
        ASSERT_TRUE(importedCompoundHandle);

        EXPECT_TRUE(system.DestroyCookedShape(sourceCompoundHandle));
        EXPECT_TRUE(system.DestroyCookedShape(sourceScaledHandle));
        EXPECT_TRUE(system.DestroyCookedShape(sourceBoxHandle));
        EXPECT_FALSE(system.DestroyCookedShape(importedScaledHandle));
        EXPECT_FALSE(system.DestroyCookedShape(importedBoxHandle));

        CookedRaycastHit hit;
        EXPECT_TRUE(
            system.Raycast(
                importedCompoundHandle,
                AZ::Vector3(2.0f, 0.0f, 3.0f),
                -AZ::Vector3::CreateAxisZ(),
                6.0f,
                hit));

        AZ::u64 userData = 0;
        ASSERT_TRUE(system.GetUserData(importedCompoundHandle, userData));
        EXPECT_EQ(userData, 17);
        ASSERT_TRUE(system.GetSubShapeUserData(importedCompoundHandle, hit.m_subShapeId, userData));
        EXPECT_EQ(userData, 11);

        AZ::u32 childCount = 0;
        ASSERT_TRUE(system.GetCompoundChildCount(importedCompoundHandle, childCount));
        EXPECT_EQ(childCount, 2);
        AZ::u32 childIndex = 0;
        ASSERT_TRUE(system.GetCompoundChildIndex(
            importedCompoundHandle,
            hit.m_subShapeId,
            childIndex));
        EXPECT_EQ(childIndex, 1);
        CookedCompoundChildConfiguration child;
        ASSERT_TRUE(system.GetCompoundChild(importedCompoundHandle, childIndex, child));
        EXPECT_EQ(child.m_shapeHandle, importedBoxHandle);
        EXPECT_EQ(child.m_userData, 23);

        CookedShapeHandle directChildHandle;
        SubShapeTransform directChildTransform;
        ASSERT_TRUE(system.GetDirectChildShape(
            importedCompoundHandle,
            hit.m_subShapeId,
            directChildHandle,
            directChildTransform));
        EXPECT_EQ(directChildHandle, importedBoxHandle);
        EXPECT_TRUE(directChildTransform.m_centerOfMassPosition.IsClose(AZ::Vector3::CreateAxisX(2.0f)));
        EXPECT_TRUE(directChildTransform.m_rotation.IsClose(AZ::Quaternion::CreateIdentity()));
        EXPECT_TRUE(directChildTransform.m_scale.IsClose(AZ::Vector3::CreateOne()));
        EXPECT_EQ(directChildTransform.m_remainder, SubShapeId::Root);
        EXPECT_FALSE(system.GetDirectChildShape(
            importedBoxHandle,
            SubShapeId(0),
            directChildHandle,
            directChildTransform));

        EXPECT_TRUE(system.DestroyCookedShape(importedCompoundHandle));
        EXPECT_TRUE(system.DestroyCookedShape(importedScaledHandle));
        EXPECT_TRUE(system.DestroyCookedShape(importedBoxHandle));
    }

    TEST(SimulationTests, CookedCompositionsRetainChildrenAndInstantiateAsOneShape)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        ShapeConfiguration boxConfiguration;
        boxConfiguration.m_geometry = BoxShapeConfiguration{};
        const CookedShapeHandle boxHandle = system.CookShape(boxConfiguration);
        ASSERT_TRUE(boxHandle);

        CookedDecoratedShapeConfiguration scaledConfiguration;
        scaledConfiguration.m_geometry = CookedScaledShapeConfiguration{
            .m_shapeHandle = boxHandle,
            .m_scale = AZ::Vector3(2.0f, 1.0f, 0.5f),
        };
        const CookedShapeHandle scaledHandle = system.CookShape(scaledConfiguration);
        ASSERT_TRUE(scaledHandle);

        CookedCompoundShapeConfiguration compoundConfiguration;
        compoundConfiguration.m_children = {
            {
                .m_position = -AZ::Vector3::CreateAxisX(2.0f),
                .m_shapeHandle = scaledHandle,
            },
            {
                .m_position = AZ::Vector3::CreateAxisX(2.0f),
                .m_shapeHandle = boxHandle,
            },
            {
                .m_position = AZ::Vector3::CreateAxisY(2.0f),
                .m_shapeHandle = boxHandle,
            },
        };
        const CookedShapeHandle compoundHandle = system.CookShape(compoundConfiguration);
        ASSERT_TRUE(compoundHandle);

        EXPECT_FALSE(system.DestroyCookedShape(boxHandle));
        EXPECT_FALSE(system.DestroyCookedShape(scaledHandle));

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, compoundHandle);
        ASSERT_TRUE(shapeHandle);

        AZ::u32 childCount = 0;
        ASSERT_TRUE(system.GetCompoundChildCount(worldHandle, shapeHandle, childCount));
        ASSERT_EQ(childCount, 3);
        CompoundChildConfiguration scaledChild;
        CompoundChildConfiguration firstBoxChild;
        CompoundChildConfiguration secondBoxChild;
        ASSERT_TRUE(system.GetCompoundChild(worldHandle, shapeHandle, 0, scaledChild));
        ASSERT_TRUE(system.GetCompoundChild(worldHandle, shapeHandle, 1, firstBoxChild));
        ASSERT_TRUE(system.GetCompoundChild(worldHandle, shapeHandle, 2, secondBoxChild));
        EXPECT_NE(scaledChild.m_shapeHandle, firstBoxChild.m_shapeHandle);
        EXPECT_EQ(firstBoxChild.m_shapeHandle, secondBoxChild.m_shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        bodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        ShapeCollectionRequest request;
        request.m_bounds.m_halfExtents = AZ::Vector3(8.0f);
        AZStd::array<TransformedShape, 3> transformedShapes;
        const QueryResult result = system.CollectShapesInBounds(worldHandle, request, transformedShapes);
        EXPECT_EQ(result.m_hitCount, 3);
        EXPECT_TRUE(result.IsComplete());
        EXPECT_EQ(transformedShapes[0].GetShapeHandle(), shapeHandle);
        EXPECT_EQ(transformedShapes[1].GetShapeHandle(), shapeHandle);

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        for (TransformedShape& transformedShape : transformedShapes)
        {
            transformedShape = {};
        }
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, scaledChild.m_shapeHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, firstBoxChild.m_shapeHandle));
        EXPECT_TRUE(system.DestroyCookedShape(compoundHandle));
        EXPECT_TRUE(system.DestroyCookedShape(scaledHandle));
        EXPECT_TRUE(system.DestroyCookedShape(boxHandle));
    }

    TEST(SimulationTests, SceneSourcesCompileCookedDependencyGraphsTransactionally)
    {
        NameDictionaryScope nameDictionaryScope;
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SceneSourceData sourceData;
        sourceData.m_name = AZ::Name("CompiledScene");
        sourceData.m_materials.push_back(MaterialConfiguration{});
        sourceData.m_shapes.push_back(SceneSourceShapeData{
            .m_geometry = BoxShapeConfiguration{},
            .m_materialIndices = {0},
            .m_userData = 101,
        });
        sourceData.m_shapes.push_back(SceneSourceScaledShape{
            .m_scale = AZ::Vector3(2.0f, 1.0f, 0.5f),
            .m_userData = 202,
            .m_shapeIndex = 0,
        });
        sourceData.m_shapes.push_back(SceneSourceCompoundShape{
            .m_children = {
                {
                    .m_position = -AZ::Vector3::CreateAxisX(2.0f),
                    .m_shapeIndex = 1,
                    .m_userData = 11,
                },
                {
                    .m_position = AZ::Vector3::CreateAxisX(2.0f),
                    .m_shapeIndex = 0,
                    .m_userData = 22,
                },
            },
            .m_userData = 303,
        });
        SceneAssetRigidBody sourceBody;
        sourceBody.m_shapeIndex = 2;
        sourceData.m_bodies.emplace_back(sourceBody);

        SceneSourceSoftBodyDefinition sourceSoftBodyDefinition;
        sourceSoftBodyDefinition.m_vertices = {
            {.m_position = AZ::Vector3(-0.5f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.5f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        sourceSoftBodyDefinition.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        sourceSoftBodyDefinition.m_edgeConstraints = {
            {.m_firstVertex = 0, .m_secondVertex = 1},
            {.m_firstVertex = 1, .m_secondVertex = 2},
            {.m_firstVertex = 2, .m_secondVertex = 0},
        };
        sourceData.m_softBodyDefinitions.push_back(AZStd::move(sourceSoftBodyDefinition));

        SceneAssetSoftBody sourceSoftBody;
        sourceSoftBody.m_definitionIndex = 0;
        sourceSoftBody.m_manualUpdate = true;
        sourceData.m_bodies.emplace_back(sourceSoftBody);

        SceneAssetData assetData;
        ASSERT_TRUE(system.BuildSceneAsset(sourceData, assetData));
        EXPECT_EQ(assetData.m_name, sourceData.m_name);
        ASSERT_EQ(assetData.m_softBodyDefinitions.size(), 1);
        EXPECT_FALSE(assetData.m_softBodyDefinitions[0].m_archive.m_binaryState.empty());
        EXPECT_NE(assetData.m_softBodyDefinitions[0].m_archive.m_buildFingerprint, 0);
        EXPECT_NE(assetData.m_softBodyDefinitions[0].m_archive.m_contentHash, 0);
        EXPECT_TRUE(assetData.m_softBodyDefinitions[0].m_materialIndices.empty());
        ASSERT_EQ(assetData.m_bodies.size(), 2);
        const SceneAssetSoftBody* compiledSoftBody =
            AZStd::get_if<SceneAssetSoftBody>(&assetData.m_bodies[1]);
        ASSERT_TRUE(compiledSoftBody);
        EXPECT_TRUE(compiledSoftBody->m_manualUpdate);
        ASSERT_EQ(assetData.m_shapes.size(), 3);
        ASSERT_EQ(assetData.m_shapes[0].m_materialIndices.size(), 1);
        EXPECT_EQ(assetData.m_shapes[0].m_materialIndices[0], 0);
        ASSERT_EQ(assetData.m_shapes[1].m_childShapeIndices.size(), 1);
        EXPECT_EQ(assetData.m_shapes[1].m_childShapeIndices[0], 0);
        ASSERT_EQ(assetData.m_shapes[2].m_childShapeIndices.size(), 2);
        EXPECT_EQ(assetData.m_shapes[2].m_childShapeIndices[0], 1);
        EXPECT_EQ(assetData.m_shapes[2].m_childShapeIndices[1], 0);

        const SceneDefinitionHandle definitionHandle = system.CreateSceneDefinition(assetData);
        ASSERT_TRUE(definitionHandle);
        const SceneInstanceHandle instanceHandle = system.InstantiateScene(
            system.GetDefaultWorldHandle(),
            definitionHandle);
        ASSERT_TRUE(instanceHandle);

        AZStd::array<BodyHandle, 2> bodyHandles;
        ASSERT_TRUE(system.GetSceneBodies(
            system.GetDefaultWorldHandle(),
            instanceHandle,
            bodyHandles).IsComplete());

        BodyState bodyState;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ASSERT_TRUE(system.GetBodyState(worldHandle, bodyHandles[0], bodyState));
        AZ::u64 userData = 0;
        ASSERT_TRUE(system.GetShapeUserData(worldHandle, bodyState.m_shapeHandle, userData));
        EXPECT_EQ(userData, 303);

        RaycastRequest request;
        request.m_start = WorldPosition(2.0, 0.0, 5.0);
        request.m_displacement = -10.0f * AZ::Vector3::CreateAxisZ();
        RaycastHit hit;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, bodyHandles[0]);
        ASSERT_TRUE(system.GetShapeSubShapeUserData(
            worldHandle,
            bodyState.m_shapeHandle,
            hit.m_subShapeId,
            userData));
        EXPECT_EQ(userData, 101);

        AZ::u32 childIndex = 0;
        ASSERT_TRUE(system.GetCompoundChildIndex(
            worldHandle,
            bodyState.m_shapeHandle,
            hit.m_subShapeId,
            childIndex));
        EXPECT_EQ(childIndex, 1);
        CompoundChildConfiguration child;
        ASSERT_TRUE(system.GetCompoundChild(
            worldHandle,
            bodyState.m_shapeHandle,
            childIndex,
            child));
        EXPECT_EQ(child.m_userData, 22);

        EXPECT_TRUE(system.UpdateSoftBodyManually(
            worldHandle,
            bodyHandles[1],
            1.0f / 60.0f));

        EXPECT_TRUE(system.DestroySceneInstance(worldHandle, instanceHandle));
        EXPECT_TRUE(system.DestroySceneDefinition(definitionHandle));

        SceneSourceData invalidSource = sourceData;
        invalidSource.m_shapes[1] = SceneSourceScaledShape{
            .m_shapeIndex = 1,
        };
        SceneAssetData unchanged;
        unchanged.m_name = AZ::Name("Unchanged");
        EXPECT_FALSE(system.BuildSceneAsset(invalidSource, unchanged));
        EXPECT_EQ(unchanged.m_name, AZ::Name("Unchanged"));
        EXPECT_TRUE(unchanged.m_shapes.empty());
    }

    TEST(SimulationTests, SceneAssetsLoadHandleFreeDependenciesAndOwnTheirRuntimeResources)
    {
        NameDictionaryScope nameDictionaryScope;
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SceneAssetData assetData;
        assetData.m_name = AZ::Name("HandleFreeScene");
        assetData.m_materials.push_back(MaterialConfiguration{});
        assetData.m_groupFilters.push_back({.m_subGroupCount = 2});
        assetData.m_paths.push_back({
            .m_points = {
                {
                    .m_position = -AZ::Vector3::CreateAxisX(2.0f),
                },
                {
                    .m_position = AZ::Vector3::CreateAxisX(2.0f),
                },
            },
        });

        const MaterialHandle sourceMaterial = system.CreateMaterial(assetData.m_materials.front());
        ASSERT_TRUE(sourceMaterial);
        ShapeConfiguration sourceConfiguration;
        sourceConfiguration.m_geometry = BoxShapeConfiguration{};
        sourceConfiguration.m_materials = {sourceMaterial};
        const CookedShapeHandle sourceShape = system.CookShape(sourceConfiguration);
        ASSERT_TRUE(sourceShape);

        SceneAssetShape assetShape;
        AZStd::vector<MaterialHandle> exportedMaterials;
        AZStd::vector<CookedShapeHandle> exportedChildren;
        ASSERT_TRUE(system.ExportShape(
            sourceShape,
            assetShape.m_archive,
            exportedMaterials,
            exportedChildren));
        ASSERT_EQ(exportedMaterials.size(), 1);
        EXPECT_EQ(exportedMaterials.front(), sourceMaterial);
        EXPECT_TRUE(exportedChildren.empty());
        assetShape.m_materialIndices = {0};
        assetData.m_shapes.push_back(AZStd::move(assetShape));

        EXPECT_TRUE(system.DestroyCookedShape(sourceShape));
        EXPECT_TRUE(system.DestroyMaterial(sourceMaterial));

        SceneAssetRigidBody firstBody;
        firstBody.m_shapeIndex = 0;
        firstBody.m_groupFilterIndex = 0;
        firstBody.m_collisionGroupId = CollisionGroupId(1);
        firstBody.m_collisionSubGroupId = CollisionSubGroupId(0);
        firstBody.m_transform.m_position.m_x = -1.0;
        assetData.m_bodies.emplace_back(firstBody);

        SceneAssetRigidBody secondBody = firstBody;
        secondBody.m_collisionSubGroupId = CollisionSubGroupId(1);
        secondBody.m_transform.m_position.m_x = 1.0;
        assetData.m_bodies.emplace_back(secondBody);

        SceneAssetConstraint constraint;
        constraint.m_geometry = PathConstraintComponentConfiguration{
            .m_pathEntityId = AZ::EntityId(),
        };
        constraint.m_firstBodyIndex = 0;
        constraint.m_secondBodyIndex = 1;
        constraint.m_pathIndex = 0;
        assetData.m_constraints.push_back(constraint);

        const SceneDefinitionHandle definitionHandle = system.CreateSceneDefinition(assetData);
        ASSERT_TRUE(definitionHandle);

        SceneDefinitionState definitionState;
        ASSERT_TRUE(system.GetSceneDefinitionState(definitionHandle, definitionState));
        EXPECT_EQ(definitionState.m_name, assetData.m_name);
        EXPECT_EQ(definitionState.m_bodyCount, 2);
        EXPECT_EQ(definitionState.m_constraintCount, 1);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const SceneInstanceHandle instanceHandle = system.InstantiateScene(worldHandle, definitionHandle);
        ASSERT_TRUE(instanceHandle);
        EXPECT_FALSE(system.DestroySceneDefinition(definitionHandle));

        AZStd::array<BodyHandle, 2> bodyHandles;
        const QueryResult bodies = system.GetSceneBodies(worldHandle, instanceHandle, bodyHandles);
        EXPECT_TRUE(bodies.IsComplete());
        EXPECT_EQ(bodies.m_hitCount, 2);

        EXPECT_TRUE(system.DestroySceneInstance(worldHandle, instanceHandle));
        EXPECT_TRUE(system.DestroySceneDefinition(definitionHandle));
        EXPECT_FALSE(system.IsValid(definitionHandle));
    }

    TEST(SimulationTests, SceneAssetLoadingRollsBackAfterInvalidLocalDependency)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        SceneAssetData assetData;
        assetData.m_materials.push_back(MaterialConfiguration{});
        assetData.m_groupFilters.push_back({.m_subGroupCount = 2});
        assetData.m_paths.push_back({
            .m_points = {
                HermitePathPoint{},
                {
                    .m_position = AZ::Vector3::CreateAxisX(),
                },
            },
        });

        ShapeConfiguration sourceConfiguration;
        sourceConfiguration.m_geometry = SphereShapeConfiguration{};
        const CookedShapeHandle sourceShape = system.CookShape(sourceConfiguration);
        ASSERT_TRUE(sourceShape);

        SceneAssetShape assetShape;
        AZStd::vector<MaterialHandle> exportedMaterials;
        AZStd::vector<CookedShapeHandle> exportedChildren;
        ASSERT_TRUE(system.ExportShape(
            sourceShape,
            assetShape.m_archive,
            exportedMaterials,
            exportedChildren));
        assetShape.m_materialIndices = {InvalidAssetIndex};
        assetShape.m_childShapeIndices = {0};
        assetData.m_shapes.push_back(assetShape);
        EXPECT_FALSE(system.CreateSceneDefinition(assetData));

        assetData.m_shapes.front().m_childShapeIndices.clear();
        const SceneDefinitionHandle definitionHandle = system.CreateSceneDefinition(assetData);
        ASSERT_TRUE(definitionHandle);
        EXPECT_TRUE(system.DestroySceneDefinition(definitionHandle));
        EXPECT_TRUE(system.DestroyCookedShape(sourceShape));
    }

    TEST(SimulationTests, SceneDefinitionsInstantiateMixedBodiesTransactionallyAndRetainDependencies)
    {
        NameDictionaryScope nameDictionaryScope;
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        ShapeConfiguration sphereConfiguration;
        sphereConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.5f};
        const CookedShapeHandle cookedShapeHandle = system.CookShape(sphereConfiguration);
        ASSERT_TRUE(cookedShapeHandle);

        SoftBodyDefinitionConfiguration softBodyDefinition;
        softBodyDefinition.m_vertices = {
            {.m_position = AZ::Vector3(-0.5f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.5f, 0.0f, 0.0f)},
            {.m_position = AZ::Vector3(0.0f, 1.0f, 0.0f)},
        };
        softBodyDefinition.m_faces = {
            {.m_firstVertex = 0, .m_secondVertex = 1, .m_thirdVertex = 2},
        };
        softBodyDefinition.m_edgeConstraints = {
            {.m_firstVertex = 0, .m_secondVertex = 1},
            {.m_firstVertex = 1, .m_secondVertex = 2},
            {.m_firstVertex = 2, .m_secondVertex = 0},
        };
        const SoftBodyDefinitionHandle softBodyDefinitionHandle =
            system.CreateSoftBodyDefinition(softBodyDefinition);
        ASSERT_TRUE(softBodyDefinitionHandle);

        SceneRigidBodyConfiguration staticBody;
        staticBody.m_cookedShapeHandle = cookedShapeHandle;
        staticBody.m_body.m_motionType = MotionType::Static;
        staticBody.m_body.m_objectLayer = DefaultLayers::NonMoving;

        SceneRigidBodyConfiguration dynamicBody;
        dynamicBody.m_cookedShapeHandle = cookedShapeHandle;
        dynamicBody.m_body.m_transform.m_position.m_z = 2.0;

        SoftBodyConfiguration softBody;
        softBody.m_definitionHandle = softBodyDefinitionHandle;
        softBody.m_transform.m_position.m_x = 3.0;
        softBody.m_transform.m_position.m_z = 2.0;

        SceneConstraintConfiguration constraint;
        constraint.m_firstBodyIndex = 0;
        constraint.m_secondBodyIndex = 1;
        FixedConstraintConfiguration fixed;
        fixed.m_autoDetectPoint = true;
        fixed.m_space = ConstraintSpace::World;
        constraint.m_constraint.m_geometry = fixed;

        SceneConfiguration sceneConfiguration;
        sceneConfiguration.m_bodies = {staticBody, dynamicBody, softBody};
        sceneConfiguration.m_constraints = {constraint};
        sceneConfiguration.m_name = AZ::Name("MixedScene");
        const SceneDefinitionHandle definitionHandle = system.CreateSceneDefinition(sceneConfiguration);
        ASSERT_TRUE(definitionHandle);
        EXPECT_TRUE(system.IsValid(definitionHandle));
        EXPECT_FALSE(system.DestroyCookedShape(cookedShapeHandle));
        EXPECT_FALSE(system.DestroySoftBodyDefinition(softBodyDefinitionHandle));

        SceneDefinitionState definitionState;
        ASSERT_TRUE(system.GetSceneDefinitionState(definitionHandle, definitionState));
        EXPECT_EQ(definitionState.m_name, sceneConfiguration.m_name);
        EXPECT_EQ(definitionState.m_bodyCount, 3);
        EXPECT_EQ(definitionState.m_rigidBodyCount, 2);
        EXPECT_EQ(definitionState.m_softBodyCount, 1);
        EXPECT_EQ(definitionState.m_constraintCount, 1);
        EXPECT_EQ(definitionState.m_instanceCount, 0);

        const WorldHandle firstWorldHandle = system.GetDefaultWorldHandle();
        const WorldHandle secondWorldHandle = system.CreateWorld(WorldConfiguration{});
        ASSERT_TRUE(secondWorldHandle);
        const SceneInstanceHandle firstInstanceHandle =
            system.InstantiateScene(firstWorldHandle, definitionHandle);
        const SceneInstanceHandle secondInstanceHandle =
            system.InstantiateScene(secondWorldHandle, definitionHandle);
        ASSERT_TRUE(firstInstanceHandle);
        ASSERT_TRUE(secondInstanceHandle);
        EXPECT_FALSE(system.DestroySceneDefinition(definitionHandle));
        WorldStatistics firstWorldStatistics;
        ASSERT_TRUE(system.GetWorldStatistics(firstWorldHandle, firstWorldStatistics));
        EXPECT_EQ(firstWorldStatistics.m_sceneInstanceCount, 1);
        WorldStatistics secondWorldStatistics;
        ASSERT_TRUE(system.GetWorldStatistics(secondWorldHandle, secondWorldStatistics));
        EXPECT_EQ(secondWorldStatistics.m_sceneInstanceCount, 1);

        ASSERT_TRUE(system.GetSceneDefinitionState(definitionHandle, definitionState));
        EXPECT_EQ(definitionState.m_instanceCount, 2);
        SceneInstanceState instanceState;
        ASSERT_TRUE(system.GetSceneInstanceState(firstWorldHandle, firstInstanceHandle, instanceState));
        EXPECT_EQ(instanceState.m_definitionHandle, definitionHandle);
        EXPECT_EQ(instanceState.m_bodyCount, 3);
        EXPECT_EQ(instanceState.m_rigidBodyCount, 2);
        EXPECT_EQ(instanceState.m_softBodyCount, 1);
        EXPECT_EQ(instanceState.m_constraintCount, 1);

        AZStd::array<BodyHandle, 2> truncatedBodies;
        const QueryResult truncatedResult =
            system.GetSceneBodies(firstWorldHandle, firstInstanceHandle, truncatedBodies);
        EXPECT_EQ(truncatedResult.m_hitCount, 2);
        EXPECT_EQ(truncatedResult.m_requiredHitCount, 3);
        EXPECT_FALSE(truncatedResult.IsComplete());

        AZStd::array<BodyHandle, 3> firstBodies;
        EXPECT_TRUE(system.GetSceneBodies(firstWorldHandle, firstInstanceHandle, firstBodies).IsComplete());
        BodyState firstRigidState;
        BodyState secondRigidState;
        ASSERT_TRUE(system.GetBodyState(firstWorldHandle, firstBodies[0], firstRigidState));
        ASSERT_TRUE(system.GetBodyState(firstWorldHandle, firstBodies[1], secondRigidState));
        EXPECT_EQ(firstRigidState.m_shapeHandle, secondRigidState.m_shapeHandle);
        EXPECT_FALSE(system.DestroyBody(firstWorldHandle, firstBodies[0]));

        AZStd::array<ConstraintHandle, 1> firstConstraints;
        EXPECT_TRUE(system.GetSceneConstraints(
            firstWorldHandle,
            firstInstanceHandle,
            firstConstraints).IsComplete());
        EXPECT_FALSE(system.DestroyConstraint(firstWorldHandle, firstConstraints[0]));
        EXPECT_TRUE(system.IsConstraintInSimulation(firstWorldHandle, firstConstraints[0]));
        EXPECT_FALSE(system.RemoveConstraintFromSimulation(firstWorldHandle, firstConstraints[0]));
        EXPECT_FALSE(system.RemoveConstraintsFromSimulation(firstWorldHandle, firstConstraints));
        EXPECT_FALSE(system.DestroyConstraints(firstWorldHandle, firstConstraints));
        EXPECT_TRUE(system.IsConstraintInSimulation(firstWorldHandle, firstConstraints[0]));

        ShapeConfiguration externalShapeConfiguration;
        externalShapeConfiguration.m_geometry = BoxShapeConfiguration{};
        const ShapeHandle externalShapeHandle =
            system.CreateShape(firstWorldHandle, externalShapeConfiguration);
        ASSERT_TRUE(externalShapeHandle);
        BodyConfiguration externalBodyConfiguration;
        externalBodyConfiguration.m_shapeHandle = externalShapeHandle;
        externalBodyConfiguration.m_transform.m_position.m_x = 5.0;
        const BodyHandle externalBodyHandle =
            system.CreateBody(firstWorldHandle, externalBodyConfiguration);
        ASSERT_TRUE(externalBodyHandle);
        ConstraintConfiguration externalConstraintConfiguration;
        externalConstraintConfiguration.m_firstBodyHandle = firstBodies[1];
        externalConstraintConfiguration.m_secondBodyHandle = externalBodyHandle;
        externalConstraintConfiguration.m_geometry = fixed;
        const ConstraintHandle externalConstraintHandle =
            system.CreateConstraint(firstWorldHandle, externalConstraintConfiguration);
        ASSERT_TRUE(externalConstraintHandle);

        EXPECT_FALSE(system.DestroySceneInstance(firstWorldHandle, firstInstanceHandle));
        EXPECT_TRUE(system.IsValid(firstWorldHandle, firstInstanceHandle));
        EXPECT_TRUE(system.IsValid(firstWorldHandle, firstBodies[0]));
        EXPECT_TRUE(system.IsValid(firstWorldHandle, firstConstraints[0]));
        EXPECT_TRUE(system.DestroyConstraint(firstWorldHandle, externalConstraintHandle));
        EXPECT_TRUE(system.DestroyBody(firstWorldHandle, externalBodyHandle));
        EXPECT_TRUE(system.DestroyShape(firstWorldHandle, externalShapeHandle));

        EXPECT_TRUE(system.DestroySceneInstance(firstWorldHandle, firstInstanceHandle));
        EXPECT_FALSE(system.IsValid(firstWorldHandle, firstInstanceHandle));
        EXPECT_FALSE(system.IsValid(firstWorldHandle, firstBodies[0]));
        EXPECT_FALSE(system.IsValid(firstWorldHandle, firstConstraints[0]));
        ASSERT_TRUE(system.GetWorldStatistics(firstWorldHandle, firstWorldStatistics));
        EXPECT_EQ(firstWorldStatistics.m_sceneInstanceCount, 0);
        EXPECT_TRUE(system.DestroySceneInstance(secondWorldHandle, secondInstanceHandle));
        EXPECT_TRUE(system.DestroyWorld(secondWorldHandle));
        ASSERT_TRUE(system.GetSceneDefinitionState(definitionHandle, definitionState));
        EXPECT_EQ(definitionState.m_instanceCount, 0);
        EXPECT_TRUE(system.DestroySceneDefinition(definitionHandle));
        EXPECT_FALSE(system.IsValid(definitionHandle));
        EXPECT_TRUE(system.DestroySoftBodyDefinition(softBodyDefinitionHandle));
        EXPECT_TRUE(system.DestroyCookedShape(cookedShapeHandle));
    }

    TEST(SimulationTests, SceneInstantiationRollsBackEveryReservedResourceAfterValidationFailure)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const CookedShapeHandle cookedShapeHandle = system.CookShape(shapeConfiguration);
        ASSERT_TRUE(cookedShapeHandle);

        SceneRigidBodyConfiguration validBody;
        validBody.m_cookedShapeHandle = cookedShapeHandle;
        SceneRigidBodyConfiguration invalidBody = validBody;
        invalidBody.m_body.m_friction = -1.0f;
        SceneConfiguration sceneConfiguration;
        sceneConfiguration.m_bodies = {validBody, invalidBody};
        const SceneDefinitionHandle definitionHandle = system.CreateSceneDefinition(sceneConfiguration);
        ASSERT_TRUE(definitionHandle);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        WorldStatistics before;
        ASSERT_TRUE(system.GetWorldStatistics(worldHandle, before));
        EXPECT_FALSE(system.InstantiateScene(worldHandle, definitionHandle));

        WorldStatistics after;
        ASSERT_TRUE(system.GetWorldStatistics(worldHandle, after));
        EXPECT_EQ(after.m_bodyCount, before.m_bodyCount);
        EXPECT_EQ(after.m_constraintCount, before.m_constraintCount);
        EXPECT_EQ(after.m_shapeCount, before.m_shapeCount);
        SceneDefinitionState definitionState;
        ASSERT_TRUE(system.GetSceneDefinitionState(definitionHandle, definitionState));
        EXPECT_EQ(definitionState.m_instanceCount, 0);

        EXPECT_TRUE(system.DestroySceneDefinition(definitionHandle));
        EXPECT_TRUE(system.DestroyCookedShape(cookedShapeHandle));
    }

    TEST(SimulationTests, SceneDefinitionsRejectForwardAndMismatchedConstraintDependencies)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const CookedShapeHandle cookedShapeHandle = system.CookShape(shapeConfiguration);
        ASSERT_TRUE(cookedShapeHandle);

        SceneRigidBodyConfiguration firstBody;
        firstBody.m_cookedShapeHandle = cookedShapeHandle;
        SceneRigidBodyConfiguration secondBody = firstBody;
        secondBody.m_body.m_transform.m_position.m_x = 2.0;
        SceneConstraintConfiguration gearConstraint;
        gearConstraint.m_constraint.m_geometry = GearConstraintConfiguration{};
        gearConstraint.m_firstBodyIndex = 0;
        gearConstraint.m_secondBodyIndex = 1;
        gearConstraint.m_firstDependencyIndex = 1;
        gearConstraint.m_secondDependencyIndex = 0;
        SceneConfiguration sceneConfiguration;
        sceneConfiguration.m_bodies = {firstBody, secondBody};
        sceneConfiguration.m_constraints = {gearConstraint};
        EXPECT_FALSE(system.CreateSceneDefinition(sceneConfiguration));
        EXPECT_TRUE(system.DestroyCookedShape(cookedShapeHandle));
    }

    TEST(SimulationTests, SceneDefinitionsResolveConstraintDependenciesWithoutExposingWorldHandles)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const CookedShapeHandle cookedShapeHandle = system.CookShape(shapeConfiguration);
        ASSERT_TRUE(cookedShapeHandle);

        SceneRigidBodyConfiguration firstBody;
        firstBody.m_cookedShapeHandle = cookedShapeHandle;
        firstBody.m_body.m_motionType = MotionType::Static;
        firstBody.m_body.m_objectLayer = DefaultLayers::NonMoving;
        SceneRigidBodyConfiguration secondBody;
        secondBody.m_cookedShapeHandle = cookedShapeHandle;
        secondBody.m_body.m_transform.m_position.m_x = 2.0;

        SceneConstraintConfiguration firstHinge;
        firstHinge.m_firstBodyIndex = 0;
        firstHinge.m_secondBodyIndex = 1;
        firstHinge.m_constraint.m_geometry = HingeConstraintConfiguration{};
        SceneConstraintConfiguration secondHinge = firstHinge;
        SceneConstraintConfiguration gear;
        gear.m_firstBodyIndex = 0;
        gear.m_secondBodyIndex = 1;
        gear.m_firstDependencyIndex = 0;
        gear.m_secondDependencyIndex = 1;
        gear.m_constraint.m_geometry = GearConstraintConfiguration{};

        SceneConfiguration sceneConfiguration;
        sceneConfiguration.m_bodies = {firstBody, secondBody};
        sceneConfiguration.m_constraints = {firstHinge, secondHinge, gear};
        const SceneDefinitionHandle definitionHandle = system.CreateSceneDefinition(sceneConfiguration);
        ASSERT_TRUE(definitionHandle);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const SceneInstanceHandle instanceHandle = system.InstantiateScene(worldHandle, definitionHandle);
        ASSERT_TRUE(instanceHandle);
        AZStd::array<ConstraintHandle, 3> constraintHandles;
        ASSERT_TRUE(system.GetSceneConstraints(worldHandle, instanceHandle, constraintHandles).IsComplete());
        ConstraintConfiguration resolvedGear;
        ASSERT_TRUE(system.GetConstraintConfiguration(worldHandle, constraintHandles[2], resolvedGear));
        const GearConstraintConfiguration* gearGeometry =
            AZStd::get_if<GearConstraintConfiguration>(&resolvedGear.m_geometry);
        ASSERT_TRUE(gearGeometry);
        EXPECT_EQ(gearGeometry->m_firstHingeConstraintHandle, constraintHandles[0]);
        EXPECT_EQ(gearGeometry->m_secondHingeConstraintHandle, constraintHandles[1]);

        EXPECT_TRUE(system.DestroySceneInstance(worldHandle, instanceHandle));
        EXPECT_TRUE(system.DestroySceneDefinition(definitionHandle));
        EXPECT_TRUE(system.DestroyCookedShape(cookedShapeHandle));
    }

    TEST(SimulationTests, SceneDefinitionsRetainUniquePathAndCollisionFilterResources)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        GroupFilterTableConfiguration groupFilterConfiguration;
        groupFilterConfiguration.m_subGroupCount = 2;
        const GroupFilterHandle groupFilterHandle =
            system.CreateGroupFilterTable(groupFilterConfiguration);
        ASSERT_TRUE(groupFilterHandle);

        HermitePathConfiguration pathConfiguration;
        pathConfiguration.m_points = {
            {.m_position = AZ::Vector3::CreateZero()},
            {.m_position = AZ::Vector3::CreateAxisX(4.0f)},
        };
        const PathHandle pathHandle = system.CreatePath(pathConfiguration);
        ASSERT_TRUE(pathHandle);

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const CookedShapeHandle cookedShapeHandle = system.CookShape(shapeConfiguration);
        ASSERT_TRUE(cookedShapeHandle);

        SceneRigidBodyConfiguration firstBody;
        firstBody.m_cookedShapeHandle = cookedShapeHandle;
        firstBody.m_body.m_motionType = MotionType::Static;
        firstBody.m_body.m_objectLayer = DefaultLayers::NonMoving;
        firstBody.m_body.m_collisionGroup.m_filterHandle = groupFilterHandle;
        SceneRigidBodyConfiguration secondBody;
        secondBody.m_cookedShapeHandle = cookedShapeHandle;
        secondBody.m_body.m_transform.m_position.m_x = 2.0;
        secondBody.m_body.m_collisionGroup.m_filterHandle = groupFilterHandle;

        SceneConstraintConfiguration pathConstraint;
        pathConstraint.m_firstBodyIndex = 0;
        pathConstraint.m_secondBodyIndex = 1;
        pathConstraint.m_constraint.m_geometry = PathConstraintConfiguration{.m_pathHandle = pathHandle};
        SceneConfiguration sceneConfiguration;
        sceneConfiguration.m_bodies = {firstBody, secondBody};
        sceneConfiguration.m_constraints = {pathConstraint};
        const SceneDefinitionHandle definitionHandle = system.CreateSceneDefinition(sceneConfiguration);
        ASSERT_TRUE(definitionHandle);
        EXPECT_FALSE(system.DestroyGroupFilter(groupFilterHandle));
        EXPECT_FALSE(system.DestroyPath(pathHandle));

        EXPECT_TRUE(system.DestroySceneDefinition(definitionHandle));
        EXPECT_TRUE(system.DestroyGroupFilter(groupFilterHandle));
        EXPECT_TRUE(system.DestroyPath(pathHandle));
        EXPECT_TRUE(system.DestroyCookedShape(cookedShapeHandle));
    }

    TEST(SimulationTests, ComposedShapesPreserveDependenciesAndProviderIdentity)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const MaterialHandle materialHandle = system.CreateMaterial(MaterialConfiguration{});
        ASSERT_TRUE(materialHandle);

        ShapeConfiguration childConfiguration;
        childConfiguration.m_geometry = BoxShapeConfiguration{};
        childConfiguration.m_materials = {materialHandle};
        childConfiguration.m_userData = 101;
        const ShapeHandle childHandle = system.CreateShape(worldHandle, childConfiguration);
        ASSERT_TRUE(childHandle);

        DecoratedShapeConfiguration offsetConfiguration;
        offsetConfiguration.m_geometry = OffsetCenterOfMassShapeConfiguration{
            .m_shapeHandle = childHandle,
            .m_offset = AZ::Vector3::CreateAxisZ(-0.25f),
        };
        offsetConfiguration.m_userData = 102;
        const ShapeHandle offsetHandle = system.CreateShape(worldHandle, offsetConfiguration);
        ASSERT_TRUE(offsetHandle);

        DecoratedShapeConfiguration rotatedConfiguration;
        rotatedConfiguration.m_geometry = RotatedTranslatedShapeConfiguration{
            .m_shapeHandle = offsetHandle,
            .m_position = AZ::Vector3::CreateAxisX(0.25f),
        };
        rotatedConfiguration.m_userData = 103;
        const ShapeHandle rotatedHandle = system.CreateShape(worldHandle, rotatedConfiguration);
        ASSERT_TRUE(rotatedHandle);

        DecoratedShapeConfiguration scaledConfiguration;
        scaledConfiguration.m_geometry = ScaledShapeConfiguration{
            .m_shapeHandle = rotatedHandle,
            .m_scale = AZ::Vector3(1.0f, 2.0f, 0.5f),
        };
        scaledConfiguration.m_userData = 104;
        const ShapeHandle scaledHandle = system.CreateShape(worldHandle, scaledConfiguration);
        ASSERT_TRUE(scaledHandle);

        DecoratedShapeConfiguration queriedConfiguration;
        ASSERT_TRUE(system.GetDecoratedShapeConfiguration(
            worldHandle,
            offsetHandle,
            queriedConfiguration));
        const auto* queriedOffset = AZStd::get_if<OffsetCenterOfMassShapeConfiguration>(
            &queriedConfiguration.m_geometry);
        ASSERT_TRUE(queriedOffset);
        EXPECT_EQ(queriedOffset->m_shapeHandle, childHandle);
        EXPECT_TRUE(queriedOffset->m_offset.IsClose(AZ::Vector3::CreateAxisZ(-0.25f)));
        EXPECT_EQ(queriedConfiguration.m_userData, 102);

        ASSERT_TRUE(system.GetDecoratedShapeConfiguration(
            worldHandle,
            rotatedHandle,
            queriedConfiguration));
        const auto* queriedRotated = AZStd::get_if<RotatedTranslatedShapeConfiguration>(
            &queriedConfiguration.m_geometry);
        ASSERT_TRUE(queriedRotated);
        EXPECT_EQ(queriedRotated->m_shapeHandle, offsetHandle);
        EXPECT_TRUE(queriedRotated->m_position.IsClose(AZ::Vector3::CreateAxisX(0.25f)));
        EXPECT_TRUE(queriedRotated->m_rotation.IsClose(AZ::Quaternion::CreateIdentity()));
        EXPECT_EQ(queriedConfiguration.m_userData, 103);

        ASSERT_TRUE(system.GetDecoratedShapeConfiguration(
            worldHandle,
            scaledHandle,
            queriedConfiguration));
        const auto* queriedScaled = AZStd::get_if<ScaledShapeConfiguration>(
            &queriedConfiguration.m_geometry);
        ASSERT_TRUE(queriedScaled);
        EXPECT_EQ(queriedScaled->m_shapeHandle, rotatedHandle);
        EXPECT_TRUE(queriedScaled->m_scale.IsClose(AZ::Vector3(1.0f, 2.0f, 0.5f)));
        EXPECT_EQ(queriedConfiguration.m_userData, 104);
        EXPECT_FALSE(system.GetDecoratedShapeConfiguration(
            worldHandle,
            childHandle,
            queriedConfiguration));

        CompoundShapeConfiguration compoundConfiguration;
        compoundConfiguration.m_children = {
            {
                .m_position = AZ::Vector3::CreateAxisX(-1.0f),
                .m_shapeHandle = scaledHandle,
                .m_userData = 17,
            },
            {
                .m_position = AZ::Vector3::CreateAxisX(1.0f),
                .m_shapeHandle = childHandle,
                .m_userData = 23,
            },
        };
        compoundConfiguration.m_userData = 105;
        const ShapeHandle compoundHandle = system.CreateShape(worldHandle, compoundConfiguration);
        ASSERT_TRUE(compoundHandle);

        CompoundShapeConfiguration mutableConfiguration = compoundConfiguration;
        mutableConfiguration.m_kind = CompoundShapeKind::Mutable;
        const ShapeHandle mutableHandle = system.CreateShape(worldHandle, mutableConfiguration);
        ASSERT_TRUE(mutableHandle);

        EXPECT_FALSE(system.DestroyShape(worldHandle, childHandle));
        EXPECT_FALSE(system.DestroyShape(worldHandle, scaledHandle));
        EXPECT_FALSE(system.DestroyMaterial(materialHandle));

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = compoundHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        bodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        ShapeCollectionRequest collectionRequest;
        collectionRequest.m_bounds.m_halfExtents = AZ::Vector3(4.0f);
        AZStd::array<TransformedShape, 1> truncatedShapes;
        const QueryResult truncatedCollection =
            system.CollectShapesInBounds(worldHandle, collectionRequest, truncatedShapes);
        EXPECT_EQ(truncatedCollection.m_hitCount, 1);
        EXPECT_EQ(truncatedCollection.m_requiredHitCount, 2);
        EXPECT_FALSE(truncatedCollection.IsComplete());

        AZStd::array<TransformedShape, 2> transformedShapes;
        const QueryResult completeCollection =
            system.CollectShapesInBounds(worldHandle, collectionRequest, transformedShapes);
        ASSERT_EQ(completeCollection.m_hitCount, 2);
        EXPECT_TRUE(completeCollection.IsComplete());
        EXPECT_EQ(transformedShapes[0].GetBodyHandle(), bodyHandle);
        EXPECT_EQ(transformedShapes[1].GetBodyHandle(), bodyHandle);
        EXPECT_EQ(transformedShapes[0].GetMaterialHandle(), materialHandle);
        EXPECT_EQ(transformedShapes[1].GetMaterialHandle(), materialHandle);
        EXPECT_EQ(transformedShapes[0].GetShapeHandle(), compoundHandle);
        EXPECT_EQ(transformedShapes[1].GetShapeHandle(), compoundHandle);
        EXPECT_NE(transformedShapes[0].GetSubShapeId(), transformedShapes[1].GetSubShapeId());
        EXPECT_TRUE(transformedShapes[0].GetScale().IsClose(AZ::Vector3(1.0f, 2.0f, 0.5f)));
        EXPECT_TRUE(transformedShapes[1].GetScale().IsClose(AZ::Vector3::CreateOne()));

        collectionRequest.m_filter.m_collisionLayer = DefaultLayers::NonMoving;
        const QueryResult rejectedCollection =
            system.CollectShapesInBounds(worldHandle, collectionRequest, transformedShapes);
        EXPECT_EQ(rejectedCollection.m_hitCount, 0);
        collectionRequest.m_filter.m_collisionLayer = DefaultLayers::Moving;
        const QueryResult filteredCollection =
            system.CollectShapesInBounds(worldHandle, collectionRequest, transformedShapes);
        EXPECT_EQ(filteredCollection.m_hitCount, 2);

        RecordingQueryFilter collectionFilter;
        collectionFilter.m_rejectedBodyHandle = bodyHandle;
        collectionRequest.m_filter.m_callback = &collectionFilter;
        const QueryResult callbackFilteredCollection =
            system.CollectShapesInBounds(worldHandle, collectionRequest, transformedShapes);
        EXPECT_EQ(callbackFilteredCollection.m_hitCount, 0);
        EXPECT_GT(collectionFilter.m_bodyCallCount, 0);
        collectionRequest.m_filter.m_callback = nullptr;

        RaycastRequest request;
        request.m_start = {.m_x = 1.0, .m_z = 5.0};
        request.m_displacement = AZ::Vector3::CreateAxisZ(-10.0f);
        RaycastHit hit;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_bodyHandle, bodyHandle);
        EXPECT_EQ(hit.m_materialHandle, materialHandle);
        EXPECT_EQ(hit.m_shapeHandle, compoundHandle);

        AZ::u64 userData = 0;
        ASSERT_TRUE(system.GetShapeUserData(worldHandle, childHandle, userData));
        EXPECT_EQ(userData, 101);
        ASSERT_TRUE(system.GetShapeUserData(worldHandle, offsetHandle, userData));
        EXPECT_EQ(userData, 102);
        ASSERT_TRUE(system.GetShapeUserData(worldHandle, rotatedHandle, userData));
        EXPECT_EQ(userData, 103);
        ASSERT_TRUE(system.GetShapeUserData(worldHandle, scaledHandle, userData));
        EXPECT_EQ(userData, 104);
        ASSERT_TRUE(system.GetShapeUserData(worldHandle, compoundHandle, userData));
        EXPECT_EQ(userData, 105);
        ASSERT_TRUE(system.GetShapeSubShapeUserData(
            worldHandle,
            compoundHandle,
            hit.m_subShapeId,
            userData));
        EXPECT_EQ(userData, 101);

        AZ::u32 childCount = 0;
        ASSERT_TRUE(system.GetCompoundChildCount(worldHandle, compoundHandle, childCount));
        EXPECT_EQ(childCount, 2);
        AZ::u32 childIndex = 0;
        ASSERT_TRUE(system.GetCompoundChildIndex(
            worldHandle,
            compoundHandle,
            hit.m_subShapeId,
            childIndex));
        EXPECT_EQ(childIndex, 1);
        CompoundChildConfiguration compoundChild;
        ASSERT_TRUE(system.GetCompoundChild(
            worldHandle,
            compoundHandle,
            childIndex,
            compoundChild));
        EXPECT_EQ(compoundChild.m_shapeHandle, childHandle);
        EXPECT_EQ(compoundChild.m_userData, 23);

        ShapeHandle directChildHandle;
        SubShapeTransform directChildTransform;
        ASSERT_TRUE(system.GetDirectChildShape(
            worldHandle,
            compoundHandle,
            hit.m_subShapeId,
            directChildHandle,
            directChildTransform));
        EXPECT_EQ(directChildHandle, childHandle);
        ShapeProperties compoundProperties;
        ASSERT_TRUE(system.GetShapeProperties(
            worldHandle,
            compoundHandle,
            compoundProperties));
        const AZ::Vector3 expectedChildCenterOfMassPosition =
            AZ::Vector3::CreateAxisX() - compoundProperties.m_centerOfMass;
        EXPECT_TRUE(directChildTransform.m_centerOfMassPosition.IsClose(expectedChildCenterOfMassPosition));
        EXPECT_TRUE(directChildTransform.m_rotation.IsClose(AZ::Quaternion::CreateIdentity()));
        EXPECT_TRUE(directChildTransform.m_scale.IsClose(AZ::Vector3::CreateOne()));
        EXPECT_EQ(directChildTransform.m_remainder, SubShapeId::Root);
        EXPECT_FALSE(system.GetDirectChildShape(
            worldHandle,
            childHandle,
            SubShapeId(0),
            directChildHandle,
            directChildTransform));

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        truncatedShapes.front() = {};
        for (TransformedShape& transformedShape : transformedShapes)
        {
            transformedShape = {};
        }
        EXPECT_TRUE(system.DestroyShape(worldHandle, mutableHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, compoundHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, scaledHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, rotatedHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, offsetHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, childHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, MutableCompoundEditsPreserveBodyAndDependencyIdentity)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration boxConfiguration;
        boxConfiguration.m_geometry = BoxShapeConfiguration{};
        const ShapeHandle boxHandle = system.CreateShape(worldHandle, boxConfiguration);
        ASSERT_TRUE(boxHandle);

        ShapeConfiguration sphereConfiguration;
        sphereConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle sphereHandle = system.CreateShape(worldHandle, sphereConfiguration);
        ASSERT_TRUE(sphereHandle);

        CompoundShapeConfiguration compoundConfiguration;
        compoundConfiguration.m_kind = CompoundShapeKind::Mutable;
        const ShapeHandle compoundHandle = system.CreateShape(worldHandle, compoundConfiguration);
        ASSERT_TRUE(compoundHandle);

        AZ::u32 childCount = 1;
        ASSERT_TRUE(system.GetCompoundChildCount(worldHandle, compoundHandle, childCount));
        EXPECT_EQ(childCount, 0);

        CompoundChildConfiguration child{
            .m_position = -AZ::Vector3::CreateAxisX(2.0f),
            .m_shapeHandle = boxHandle,
            .m_userData = 17,
        };
        AZ::u32 childIndex = AppendCompoundChildIndex;
        ASSERT_TRUE(
            system.AddMutableCompoundChild(
                worldHandle,
                compoundHandle,
                child,
                AppendCompoundChildIndex,
                childIndex));
        EXPECT_EQ(childIndex, 0);
        EXPECT_FALSE(system.DestroyShape(worldHandle, boxHandle));

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = compoundHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        bodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        OverlapHit overlapHits[2];
        PointOverlapRequest overlapRequest;
        overlapRequest.m_position.m_x = -2.0;
        QueryResult overlapResult = system.OverlapPoint(worldHandle, overlapRequest, overlapHits);
        ASSERT_EQ(overlapResult.m_hitCount, 1);
        EXPECT_EQ(overlapHits[0].m_bodyHandle, bodyHandle);
        EXPECT_EQ(overlapHits[0].m_shapeHandle, compoundHandle);

        child.m_position = AZ::Vector3::CreateAxisX(2.0f);
        child.m_userData = 23;
        ASSERT_TRUE(
            system.UpdateMutableCompoundChild(
                worldHandle,
                compoundHandle,
                childIndex,
                child));

        CompoundChildConfiguration currentChild;
        ASSERT_TRUE(
            system.GetCompoundChild(
                worldHandle,
                compoundHandle,
                childIndex,
                currentChild));
        EXPECT_EQ(currentChild.m_shapeHandle, boxHandle);
        EXPECT_TRUE(currentChild.m_position.IsClose(child.m_position));
        EXPECT_TRUE(currentChild.m_rotation.IsClose(child.m_rotation));
        EXPECT_EQ(currentChild.m_userData, child.m_userData);

        overlapResult = system.OverlapPoint(worldHandle, overlapRequest, overlapHits);
        EXPECT_EQ(overlapResult.m_hitCount, 0);
        overlapRequest.m_position.m_x = 2.0;
        overlapResult = system.OverlapPoint(worldHandle, overlapRequest, overlapHits);
        ASSERT_EQ(overlapResult.m_hitCount, 1);
        EXPECT_EQ(overlapHits[0].m_bodyHandle, bodyHandle);

        child = {
            .m_position = AZ::Vector3::CreateZero(),
            .m_shapeHandle = sphereHandle,
            .m_userData = 31,
        };
        ASSERT_TRUE(system.AddMutableCompoundChild(worldHandle, compoundHandle, child, 0, childIndex));
        EXPECT_EQ(childIndex, 0);
        ASSERT_TRUE(system.GetCompoundChildCount(worldHandle, compoundHandle, childCount));
        EXPECT_EQ(childCount, 2);
        EXPECT_FALSE(system.DestroyShape(worldHandle, sphereHandle));

        const AZStd::array positions = {
            -AZ::Vector3::CreateAxisX(4.0f),
            AZ::Vector3::CreateAxisX(4.0f),
        };
        const AZStd::array rotations = {
            AZ::Quaternion::CreateRotationZ(AZ::Constants::HalfPi),
            AZ::Quaternion::CreateIdentity(),
        };
        ASSERT_TRUE(
            system.UpdateMutableCompoundChildTransforms(
                worldHandle,
                compoundHandle,
                0,
                positions,
                rotations));

        ASSERT_TRUE(system.GetCompoundChild(worldHandle, compoundHandle, 0, currentChild));
        EXPECT_EQ(currentChild.m_shapeHandle, sphereHandle);
        EXPECT_TRUE(currentChild.m_position.IsClose(positions[0]));
        EXPECT_TRUE(currentChild.m_rotation.IsClose(rotations[0]));
        EXPECT_EQ(currentChild.m_userData, 31);

        ASSERT_TRUE(system.GetCompoundChild(worldHandle, compoundHandle, 1, currentChild));
        EXPECT_EQ(currentChild.m_shapeHandle, boxHandle);
        EXPECT_TRUE(currentChild.m_position.IsClose(positions[1]));
        EXPECT_TRUE(currentChild.m_rotation.IsClose(rotations[1]));
        EXPECT_EQ(currentChild.m_userData, 23);

        EXPECT_FALSE(
            system.UpdateMutableCompoundChildTransforms(
                worldHandle,
                compoundHandle,
                1,
                positions,
                rotations));
        EXPECT_FALSE(system.GetCompoundChild(worldHandle, compoundHandle, 2, currentChild));

        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);
        EXPECT_FALSE(system.AdjustMutableCompoundCenterOfMass(
            worldHandle,
            boxHandle,
            true,
            true));
        ASSERT_TRUE(system.AdjustMutableCompoundCenterOfMass(
            worldHandle,
            compoundHandle,
            true,
            true));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));

        ASSERT_TRUE(system.RemoveMutableCompoundChild(worldHandle, compoundHandle, 0));
        ASSERT_TRUE(system.RemoveMutableCompoundChild(worldHandle, compoundHandle, 0));
        ASSERT_TRUE(system.GetCompoundChildCount(worldHandle, compoundHandle, childCount));
        EXPECT_EQ(childCount, 0);
        EXPECT_TRUE(system.IsValid(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, sphereHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, boxHandle));

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, compoundHandle));
    }

    TEST(SimulationTests, MutableShapesCloneIndependentlyAndRetainDependencies)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration boxConfiguration;
        boxConfiguration.m_geometry = BoxShapeConfiguration{};
        const ShapeHandle boxHandle = system.CreateShape(worldHandle, boxConfiguration);
        ASSERT_TRUE(boxHandle);
        EXPECT_FALSE(system.CloneShape(worldHandle, boxHandle));

        CompoundShapeConfiguration compoundConfiguration;
        compoundConfiguration.m_kind = CompoundShapeKind::Mutable;
        compoundConfiguration.m_children.push_back({.m_shapeHandle = boxHandle});
        const ShapeHandle compoundHandle = system.CreateShape(worldHandle, compoundConfiguration);
        ASSERT_TRUE(compoundHandle);

        const ShapeHandle compoundCloneHandle = system.CloneShape(worldHandle, compoundHandle);
        ASSERT_TRUE(compoundCloneHandle);
        ASSERT_TRUE(system.RemoveMutableCompoundChild(worldHandle, compoundHandle, 0));

        AZ::u32 childCount = 0;
        ASSERT_TRUE(system.GetCompoundChildCount(worldHandle, compoundHandle, childCount));
        EXPECT_EQ(childCount, 0);
        ASSERT_TRUE(system.GetCompoundChildCount(worldHandle, compoundCloneHandle, childCount));
        EXPECT_EQ(childCount, 1);
        EXPECT_FALSE(system.DestroyShape(worldHandle, boxHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, compoundHandle));
        EXPECT_FALSE(system.DestroyShape(worldHandle, boxHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, compoundCloneHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, boxHandle));

        MaterialConfiguration materialConfiguration;
        materialConfiguration.m_debugName = "ClonedHeightfieldMaterial";
        const MaterialHandle materialHandle = system.CreateMaterial(materialConfiguration);
        ASSERT_TRUE(materialHandle);

        HeightfieldShapeConfiguration heightfield;
        heightfield.m_sampleCount = 4;
        heightfield.m_blockSize = 2;
        heightfield.m_heights.resize(16, 0.0f);
        heightfield.m_materialIndices.resize(9, 0);

        ShapeConfiguration heightfieldConfiguration;
        heightfieldConfiguration.m_geometry = heightfield;
        heightfieldConfiguration.m_materials = {materialHandle};
        const ShapeHandle heightfieldHandle = system.CreateShape(worldHandle, heightfieldConfiguration);
        ASSERT_TRUE(heightfieldHandle);

        const ShapeHandle heightfieldCloneHandle = system.CloneShape(worldHandle, heightfieldHandle);
        ASSERT_TRUE(heightfieldCloneHandle);
        const HeightfieldRegion updateRegion{
            .m_columnCount = 2,
            .m_rowCount = 2,
        };
        const AZStd::array<float, 4> updatedHeights = {1.0f, 2.0f, 3.0f, 4.0f};
        ASSERT_TRUE(system.UpdateHeightfieldHeights(
            worldHandle,
            heightfieldHandle,
            updateRegion,
            updatedHeights));

        AZStd::array<float, 4> cloneHeights;
        const QueryResult queryResult = system.GetHeightfieldHeights(
            worldHandle,
            heightfieldCloneHandle,
            updateRegion,
            cloneHeights);
        ASSERT_TRUE(queryResult.IsComplete());
        EXPECT_TRUE(AZStd::all_of(
            cloneHeights.begin(),
            cloneHeights.end(),
            [](const float height)
            {
                return height == 0.0f;
            }));

        EXPECT_TRUE(system.DestroyShape(worldHandle, heightfieldHandle));
        EXPECT_FALSE(system.DestroyMaterial(materialHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, heightfieldCloneHandle));
        EXPECT_TRUE(system.DestroyMaterial(materialHandle));
    }

    TEST(SimulationTests, HeightfieldUsesPositiveYRowsAndZUpHeights)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        HeightfieldShapeConfiguration heightfield;
        heightfield.m_sampleCount = 4;
        heightfield.m_origin = AZ::Vector3(-1.5f, -1.5f, 0.0f);
        heightfield.m_heights = {
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            2.0f,
            2.0f,
            2.0f,
            2.0f,
            3.0f,
            3.0f,
            3.0f,
            3.0f,
        };

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = heightfield;
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        bodyConfiguration.m_motionType = MotionType::Dynamic;
        EXPECT_FALSE(system.CreateBody(worldHandle, bodyConfiguration));

        RaycastRequest request;
        request.m_start = {.m_x = -0.5, .m_y = 0.5, .m_z = 10.0};
        request.m_displacement = AZ::Vector3(0.0f, 0.0f, -20.0f);
        RaycastHit hit;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_NEAR(hit.m_position.m_z, 2.0, 0.02);
        EXPECT_GT(hit.m_normal.GetZ(), 0.0f);

        AZ::Vector3 surfacePosition;
        SubShapeId surfaceSubShapeId;
        ASSERT_TRUE(system.ProjectOntoHeightfield(
            worldHandle,
            shapeHandle,
            AZ::Vector3(-0.5f, 0.5f, 10.0f),
            surfacePosition,
            surfaceSubShapeId));
        EXPECT_NEAR(surfacePosition.GetX(), -0.5f, 1.0e-4f);
        EXPECT_NEAR(surfacePosition.GetY(), 0.5f, 1.0e-4f);
        EXPECT_NEAR(surfacePosition.GetZ(), 2.0f, 1.0e-4f);
        EXPECT_NE(surfaceSubShapeId, SubShapeId::Root);
        EXPECT_FALSE(system.ProjectOntoHeightfield(
            worldHandle,
            shapeHandle,
            AZ::Vector3(100.0f, 100.0f, 10.0f),
            surfacePosition,
            surfaceSubShapeId));

        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, HeightfieldRuntimeDataUsesProviderCoordinatesAndSafeMutation)
    {
        Runtime system(CreateSerialSystemConfiguration(), nullptr);
        ASSERT_TRUE(system);

        MaterialConfiguration firstMaterialConfiguration;
        firstMaterialConfiguration.m_debugName = "FirstHeightfieldMaterial";
        const MaterialHandle firstMaterialHandle = system.CreateMaterial(firstMaterialConfiguration);
        ASSERT_TRUE(firstMaterialHandle);

        MaterialConfiguration secondMaterialConfiguration;
        secondMaterialConfiguration.m_debugName = "SecondHeightfieldMaterial";
        const MaterialHandle secondMaterialHandle = system.CreateMaterial(secondMaterialConfiguration);
        ASSERT_TRUE(secondMaterialHandle);

        HeightfieldShapeConfiguration heightfield;
        heightfield.m_sampleCount = 4;
        heightfield.m_blockSize = 2;
        heightfield.m_bitsPerSample = 16;
        heightfield.m_materialCapacity = 2;
        heightfield.m_origin = AZ::Vector3(-1.5f, -1.5f, 0.0f);
        heightfield.m_heights = {
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            1.0f,
            1.0f,
            2.0f,
            2.0f,
            2.0f,
            2.0f,
            3.0f,
            3.0f,
            3.0f,
            NoCollisionHeight,
        };
        heightfield.m_materialIndices.resize(9, 0);

        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = heightfield;
        shapeConfiguration.m_materials = {firstMaterialHandle, secondMaterialHandle};
        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_objectLayer = DefaultLayers::NonMoving;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle bodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(bodyHandle);

        HeightfieldState state;
        ASSERT_TRUE(system.GetHeightfieldState(worldHandle, shapeHandle, state));
        EXPECT_EQ(state.m_blockSize, 2);
        EXPECT_EQ(state.m_materialCount, 2);
        EXPECT_EQ(state.m_sampleCount, 4);
        EXPECT_TRUE(state.m_canUpdateHeights);

        AZ::Vector3 position = AZ::Vector3::CreateZero();
        ASSERT_TRUE(system.GetHeightfieldPosition(worldHandle, shapeHandle, 1, 2, position));
        EXPECT_NEAR(position.GetX(), -0.5f, 1.0e-4f);
        EXPECT_NEAR(position.GetY(), 0.5f, 1.0e-4f);
        EXPECT_NEAR(position.GetZ(), 2.0f, 0.01f);

        bool noCollision = false;
        ASSERT_TRUE(system.IsHeightfieldNoCollision(worldHandle, shapeHandle, 3, 3, noCollision));
        EXPECT_TRUE(noCollision);
        EXPECT_FALSE(system.IsHeightfieldNoCollision(worldHandle, shapeHandle, 4, 0, noCollision));

        const HeightfieldRegion fullHeightRegion{
            .m_columnCount = 4,
            .m_rowCount = 4,
        };
        AZStd::array<float, 1> insufficientHeights;
        QueryResult queryResult = system.GetHeightfieldHeights(
            worldHandle,
            shapeHandle,
            fullHeightRegion,
            insufficientHeights);
        EXPECT_EQ(queryResult.m_hitCount, 0);
        EXPECT_EQ(queryResult.m_requiredHitCount, 16);

        AZStd::array<float, 16> queriedHeights;
        queryResult = system.GetHeightfieldHeights(
            worldHandle,
            shapeHandle,
            fullHeightRegion,
            queriedHeights);
        EXPECT_TRUE(queryResult.IsComplete());
        EXPECT_EQ(queryResult.m_hitCount, queriedHeights.size());
        EXPECT_NEAR(queriedHeights[1], 0.0f, 0.01f);
        EXPECT_NEAR(queriedHeights[9], 2.0f, 0.01f);
        EXPECT_EQ(queriedHeights[15], NoCollisionHeight);

        const HeightfieldRegion updateRegion{
            .m_columnCount = 2,
            .m_rowCount = 2,
        };
        const AZStd::array<float, 4> updatedHeights = {0.25f, 0.5f, 0.75f, 1.0f};
        EXPECT_TRUE(system.UpdateHeightfieldHeights(
            worldHandle,
            shapeHandle,
            updateRegion,
            updatedHeights));

        AZStd::array<float, 4> queriedUpdatedHeights;
        queryResult = system.GetHeightfieldHeights(
            worldHandle,
            shapeHandle,
            updateRegion,
            queriedUpdatedHeights);
        ASSERT_TRUE(queryResult.IsComplete());
        for (size_t heightIndex = 0; heightIndex < updatedHeights.size(); ++heightIndex)
        {
            EXPECT_NEAR(queriedUpdatedHeights[heightIndex], updatedHeights[heightIndex], 0.01f);
        }

        const HeightfieldRegion unalignedRegion{
            .m_startColumn = 1,
            .m_columnCount = 2,
            .m_rowCount = 2,
        };
        EXPECT_FALSE(system.UpdateHeightfieldHeights(
            worldHandle,
            shapeHandle,
            unalignedRegion,
            updatedHeights));

        const AZStd::array<AZ::u8, 4> updatedMaterialIndices = {1, 0, 0, 1};
        const AZStd::array<MaterialHandle, 2> materialHandles = {
            firstMaterialHandle,
            secondMaterialHandle,
        };
        EXPECT_TRUE(system.UpdateHeightfieldMaterials(
            worldHandle,
            shapeHandle,
            updateRegion,
            updatedMaterialIndices,
            materialHandles));

        AZStd::array<AZ::u8, 4> queriedMaterialIndices;
        queryResult = system.GetHeightfieldMaterialIndices(
            worldHandle,
            shapeHandle,
            updateRegion,
            queriedMaterialIndices);
        ASSERT_TRUE(queryResult.IsComplete());
        EXPECT_EQ(queriedMaterialIndices, updatedMaterialIndices);

        AZStd::array<MaterialHandle, 2> queriedMaterials;
        queryResult = system.GetHeightfieldMaterials(
            worldHandle,
            shapeHandle,
            queriedMaterials);
        ASSERT_TRUE(queryResult.IsComplete());
        EXPECT_EQ(queriedMaterials, materialHandles);

        RaycastRequest request;
        request.m_start = {.m_x = -1.25, .m_y = -1.25, .m_z = 10.0};
        request.m_displacement = AZ::Vector3(0.0f, 0.0f, -20.0f);
        RaycastHit hit;
        ASSERT_TRUE(system.RaycastClosest(worldHandle, request, hit));
        EXPECT_EQ(hit.m_shapeHandle, shapeHandle);
        EXPECT_EQ(hit.m_materialHandle, secondMaterialHandle);

        HeightfieldSubShapeCoordinates coordinates;
        ASSERT_TRUE(system.GetHeightfieldSubShapeCoordinates(
            worldHandle,
            shapeHandle,
            hit.m_subShapeId,
            coordinates));
        EXPECT_EQ(coordinates.m_column, 0);
        EXPECT_EQ(coordinates.m_row, 0);
        EXPECT_LT(coordinates.m_triangle, 2);

        DecoratedShapeConfiguration decoratedConfiguration;
        decoratedConfiguration.m_geometry = ScaledShapeConfiguration{
            .m_shapeHandle = shapeHandle,
            .m_scale = AZ::Vector3(2.0f),
        };
        const ShapeHandle decoratedShapeHandle = system.CreateShape(
            worldHandle,
            decoratedConfiguration);
        ASSERT_TRUE(decoratedShapeHandle);
        EXPECT_FALSE(system.UpdateHeightfieldHeights(
            worldHandle,
            shapeHandle,
            updateRegion,
            updatedHeights));

        EXPECT_TRUE(system.DestroyShape(worldHandle, decoratedShapeHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, bodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyMaterial(secondMaterialHandle));
        EXPECT_TRUE(system.DestroyMaterial(firstMaterialHandle));
    }

    TEST(SimulationTests, ContactEventsPreserveIdentityThroughBodyDestruction)
    {
        SystemConfiguration configuration = CreateSerialSystemConfiguration();
        configuration.m_defaultWorld.m_collectActivationEvents = true;
        configuration.m_defaultWorld.m_collectContactEvents = true;
        Runtime system(configuration, nullptr);
        ASSERT_TRUE(system);

        const SphereOnFloor scene = CreateSphereOnFloor(system);
        ASSERT_TRUE(scene.m_sphereBodyHandle);

        bool receivedBegin = false;
        bool receivedActivation = false;
        for (AZ::u32 step = 0; step < 120 && !receivedBegin; ++step)
        {
            ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
            const EventView events = system.GetEvents(scene.m_worldHandle);
            for (const ActivationEvent& activation : events.GetActivations())
            {
                if (activation.m_bodyHandle == scene.m_sphereBodyHandle
                    && activation.m_state == ActivationState::Active)
                {
                    receivedActivation = true;
                }
            }
            for (const ContactEvent& contact : events.GetContacts())
            {
                if (contact.m_phase == EventPhase::Begin)
                {
                    EXPECT_TRUE(
                        (contact.m_firstBodyHandle == scene.m_floorBodyHandle
                            && contact.m_secondBodyHandle == scene.m_sphereBodyHandle)
                        || (contact.m_firstBodyHandle == scene.m_sphereBodyHandle
                            && contact.m_secondBodyHandle == scene.m_floorBodyHandle));
                    EXPECT_FALSE(events.GetContactPoints(contact).empty());
                    receivedBegin = true;
                }
            }
        }
        EXPECT_TRUE(receivedActivation);
        ASSERT_TRUE(receivedBegin);
        EXPECT_TRUE(system.WereBodiesInContact(
            scene.m_worldHandle,
            scene.m_floorBodyHandle,
            scene.m_sphereBodyHandle));

        EXPECT_TRUE(system.DestroyBody(scene.m_worldHandle, scene.m_sphereBodyHandle));
        ASSERT_TRUE(system.StepWorld(scene.m_worldHandle, 1.0f / 60.0f));
        const EventView events = system.GetEvents(scene.m_worldHandle);
        bool receivedEnd = false;
        for (const ContactEvent& contact : events.GetContacts())
        {
            if (contact.m_phase == EventPhase::End)
            {
                EXPECT_TRUE(
                    contact.m_firstBodyHandle == scene.m_sphereBodyHandle
                    || contact.m_secondBodyHandle == scene.m_sphereBodyHandle);
                EXPECT_TRUE(events.GetContactPoints(contact).empty());
                receivedEnd = true;
            }
        }
        EXPECT_TRUE(receivedEnd);

        EXPECT_TRUE(system.DestroyBody(scene.m_worldHandle, scene.m_floorBodyHandle));
        EXPECT_TRUE(system.DestroyShape(scene.m_worldHandle, scene.m_sphereShapeHandle));
        EXPECT_TRUE(system.DestroyShape(scene.m_worldHandle, scene.m_floorShapeHandle));
    }

    TEST(SimulationTests, BodyMoveEventsAreOptInAndRemainStableAfterSubscriptionRemoval)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration firstConfiguration;
        firstConfiguration.m_shapeHandle = shapeHandle;
        firstConfiguration.m_transform.m_position.m_y = -4.0;
        firstConfiguration.m_linearVelocity = AZ::Vector3::CreateAxisX(1.0f);
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, firstConfiguration);
        ASSERT_TRUE(firstBodyHandle);

        BodyConfiguration secondConfiguration = firstConfiguration;
        secondConfiguration.m_transform.m_position.m_y = 4.0;
        secondConfiguration.m_linearVelocity = AZ::Vector3::CreateAxisX(2.0f);
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, secondConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_TRUE(system.GetEvents(worldHandle).GetBodyMoves().empty());

        ASSERT_TRUE(system.SetBodyMoveEventsEnabled(worldHandle, firstBodyHandle, true));
        ASSERT_TRUE(system.SetBodyMoveEventsEnabled(worldHandle, secondBodyHandle, true));
        ASSERT_TRUE(system.SetBodyMoveEventsEnabled(worldHandle, firstBodyHandle, false));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));

        const AZStd::span<const BodyMoveEvent> moves = system.GetEvents(worldHandle).GetBodyMoves();
        ASSERT_EQ(moves.size(), 1);
        EXPECT_EQ(moves.front().m_bodyHandle, secondBodyHandle);
        EXPECT_GT(moves.front().m_transform.m_position.m_x, 0.0);
        EXPECT_NEAR(moves.front().m_transform.m_position.m_y, 4.0, 1.0e-6);

        ASSERT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_TRUE(system.GetEvents(worldHandle).GetBodyMoves().empty());

        EXPECT_FALSE(system.SetBodyMoveEventsEnabled(worldHandle, secondBodyHandle, true));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, FixedConstraintOwnsBodiesAndPreservesRelativePose)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration firstBodyConfiguration;
        firstBodyConfiguration.m_shapeHandle = shapeHandle;
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, firstBodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);

        BodyConfiguration secondBodyConfiguration;
        secondBodyConfiguration.m_shapeHandle = shapeHandle;
        secondBodyConfiguration.m_transform.m_position.m_z = 2.0;
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, secondBodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        FixedConstraintConfiguration fixed;
        fixed.m_space = ConstraintSpace::World;
        fixed.m_autoDetectPoint = true;
        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = firstBodyHandle;
        constraintConfiguration.m_secondBodyHandle = secondBodyHandle;
        constraintConfiguration.m_geometry = fixed;
        const ConstraintHandle constraintHandle = system.CreateConstraint(worldHandle, constraintConfiguration);
        ASSERT_TRUE(constraintHandle);
        EXPECT_TRUE(system.IsValid(worldHandle, constraintHandle));
        EXPECT_FALSE(system.DestroyBody(worldHandle, firstBodyHandle));

        ConstraintState constraintState;
        ASSERT_TRUE(system.GetConstraintState(worldHandle, constraintHandle, constraintState));
        EXPECT_EQ(constraintState.m_firstBodyHandle, firstBodyHandle);
        EXPECT_EQ(constraintState.m_secondBodyHandle, secondBodyHandle);
        EXPECT_EQ(constraintState.m_userData, constraintConfiguration.m_userData);
        EXPECT_EQ(constraintState.m_kind, ConstraintKind::Fixed);
        EXPECT_TRUE(constraintState.m_enabled);
        AZ::u64 userData = 0;
        ASSERT_TRUE(system.GetConstraintUserData(worldHandle, constraintHandle, userData));
        EXPECT_EQ(userData, constraintConfiguration.m_userData);
        constexpr AZ::u64 updatedUserData = 0x1111'2222'3333'4444;
        ASSERT_TRUE(system.SetConstraintUserData(worldHandle, constraintHandle, updatedUserData));
        ASSERT_TRUE(system.GetConstraintUserData(worldHandle, constraintHandle, userData));
        EXPECT_EQ(userData, updatedUserData);
        ASSERT_TRUE(system.GetConstraintState(worldHandle, constraintHandle, constraintState));
        EXPECT_EQ(constraintState.m_userData, updatedUserData);

        float debugDrawSize = 0.0f;
        ASSERT_TRUE(system.GetConstraintDebugDrawSize(
            worldHandle,
            constraintHandle,
            debugDrawSize));
        EXPECT_FLOAT_EQ(debugDrawSize, 1.0f);
        EXPECT_FALSE(system.SetConstraintDebugDrawSize(worldHandle, constraintHandle, 0.0f));
        EXPECT_TRUE(system.SetConstraintDebugDrawSize(worldHandle, constraintHandle, 2.5f));
        ASSERT_TRUE(system.GetConstraintDebugDrawSize(
            worldHandle,
            constraintHandle,
            debugDrawSize));
        EXPECT_FLOAT_EQ(debugDrawSize, 2.5f);

        const ConstraintSolverConfiguration solverConfiguration{
            .m_priority = 7,
            .m_positionStepCount = 3,
            .m_velocityStepCount = 4,
        };
        ASSERT_TRUE(
            system.UpdateConstraintSolverConfiguration(
                worldHandle,
                constraintHandle,
                solverConfiguration));
        ASSERT_TRUE(system.GetConstraintState(worldHandle, constraintHandle, constraintState));
        EXPECT_EQ(constraintState.m_solver.m_priority, 7);
        EXPECT_EQ(constraintState.m_solver.m_positionStepCount, 3);
        EXPECT_EQ(constraintState.m_solver.m_velocityStepCount, 4);
        EXPECT_TRUE(system.ResetConstraintWarmStart(worldHandle, constraintHandle));

        EXPECT_TRUE(system.SetConstraintEnabled(worldHandle, constraintHandle, false));
        ASSERT_TRUE(system.GetConstraintState(worldHandle, constraintHandle, constraintState));
        EXPECT_FALSE(constraintState.m_enabled);
        EXPECT_TRUE(system.SetConstraintEnabled(worldHandle, constraintHandle, true));
        EXPECT_TRUE(system.AddImpulse(worldHandle, secondBodyHandle, AZ::Vector3::CreateAxisX(10.0f)));
        for (AZ::u32 step = 0; step < 30; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        BodyState firstState;
        BodyState secondState;
        ASSERT_TRUE(system.GetBodyState(worldHandle, firstBodyHandle, firstState));
        ASSERT_TRUE(system.GetBodyState(worldHandle, secondBodyHandle, secondState));
        const AZ::Vector3 separation(
            static_cast<float>(secondState.m_transform.m_position.m_x - firstState.m_transform.m_position.m_x),
            static_cast<float>(secondState.m_transform.m_position.m_y - firstState.m_transform.m_position.m_y),
            static_cast<float>(secondState.m_transform.m_position.m_z - firstState.m_transform.m_position.m_z));
        EXPECT_NEAR(separation.GetLength(), 2.0f, 0.01f);

        ConstraintMeasurements measurements;
        ASSERT_TRUE(system.GetConstraintMeasurements(worldHandle, constraintHandle, measurements));
        EXPECT_TRUE(AZStd::holds_alternative<FixedMeasurements>(measurements));

        EXPECT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, constraintHandle));

        const AZStd::vector<ConstraintGeometry> additionalGeometries = {
            ConeConstraintConfiguration{.m_halfConeAngle = 0.5f},
            DistanceConstraintConfiguration{},
            GearConstraintConfiguration{},
            HingeConstraintConfiguration{},
            PointConstraintConfiguration{},
            PulleyConstraintConfiguration{},
            RackAndPinionConstraintConfiguration{},
            SixDofConstraintConfiguration{},
            SliderConstraintConfiguration{},
            SwingTwistConstraintConfiguration{},
        };
        for (const ConstraintGeometry& geometry : additionalGeometries)
        {
            constraintConfiguration.m_geometry = geometry;
            const ConstraintHandle additionalConstraintHandle =
                system.CreateConstraint(worldHandle, constraintConfiguration);
            ASSERT_TRUE(additionalConstraintHandle);

            AZStd::visit(
                [&](const auto& constraintGeometry)
                {
                    using Geometry = AZStd::remove_cvref_t<decltype(constraintGeometry)>;
                    if constexpr (AZStd::is_same_v<Geometry, ConeConstraintConfiguration>)
                    {
                        EXPECT_TRUE(system.UpdateConeLimit(worldHandle, additionalConstraintHandle, 0.75f));
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, DistanceConstraintConfiguration>)
                    {
                        EXPECT_TRUE(
                            system.UpdateDistanceLimits(
                                worldHandle,
                                additionalConstraintHandle,
                                0.5f,
                                2.0f,
                                SpringConfiguration{}));
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, HingeConstraintConfiguration>)
                    {
                        EXPECT_TRUE(
                            system.UpdateHingeLimits(
                                worldHandle,
                                additionalConstraintHandle,
                                -0.5f,
                                0.5f,
                                SpringConfiguration{},
                                1.0f));
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, PointConstraintConfiguration>)
                    {
                        EXPECT_TRUE(
                            system.UpdatePointAnchors(
                                worldHandle,
                                additionalConstraintHandle,
                                ConstraintSpace::World,
                                WorldPosition{},
                                WorldPosition{.m_x = 1.0}));
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, PulleyConstraintConfiguration>)
                    {
                        EXPECT_TRUE(
                            system.UpdatePulleyLimits(
                                worldHandle,
                                additionalConstraintHandle,
                                0.5f,
                                4.0f));
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, SixDofConstraintConfiguration>)
                    {
                        const AZStd::array<SixDofAxisLimitConfiguration, 6> limits = {};
                        EXPECT_TRUE(system.UpdateSixDofLimits(worldHandle, additionalConstraintHandle, limits));
                        const AZStd::array<MotorConfiguration, 6> motors = {};
                        EXPECT_TRUE(system.UpdateSixDofMotors(
                            worldHandle,
                            additionalConstraintHandle,
                            motors,
                            AZ::Vector3::CreateZero(),
                            AZ::Quaternion::CreateIdentity(),
                            AZ::Vector3::CreateZero(),
                            AZ::Vector3::CreateZero()));
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, SliderConstraintConfiguration>)
                    {
                        EXPECT_TRUE(system.UpdateSliderMotor(
                            worldHandle,
                            additionalConstraintHandle,
                            MotorConfiguration{},
                            0.25f,
                            0.5f));
                        EXPECT_TRUE(
                            system.UpdateSliderLimits(
                                worldHandle,
                                additionalConstraintHandle,
                                -1.0f,
                                1.0f,
                                SpringConfiguration{},
                                1.0f));
                    }
                    else if constexpr (AZStd::is_same_v<Geometry, SwingTwistConstraintConfiguration>)
                    {
                        EXPECT_TRUE(system.UpdateSwingTwistMotors(
                            worldHandle,
                            additionalConstraintHandle,
                            MotorConfiguration{},
                            MotorConfiguration{},
                            AZ::Vector3::CreateZero(),
                            AZ::Quaternion::CreateIdentity()));
                        EXPECT_TRUE(
                            system.UpdateSwingTwistLimits(
                                worldHandle,
                                additionalConstraintHandle,
                                0.5f,
                                0.5f,
                                -0.5f,
                                0.5f,
                                1.0f));
                    }
                },
                geometry);

            ConstraintConfiguration currentConfiguration;
            ASSERT_TRUE(
                system.GetConstraintConfiguration(
                    worldHandle,
                    additionalConstraintHandle,
                    currentConfiguration));
            EXPECT_EQ(currentConfiguration.m_firstBodyHandle, firstBodyHandle);
            EXPECT_EQ(currentConfiguration.m_secondBodyHandle, secondBodyHandle);
            EXPECT_EQ(currentConfiguration.m_geometry.index(), geometry.index());
            EXPECT_TRUE(system.DestroyConstraint(worldHandle, additionalConstraintHandle));
        }

        constraintConfiguration.m_geometry = HingeConstraintConfiguration{};
        const ConstraintHandle firstHingeHandle = system.CreateConstraint(worldHandle, constraintConfiguration);
        const ConstraintHandle secondHingeHandle = system.CreateConstraint(worldHandle, constraintConfiguration);
        ASSERT_TRUE(firstHingeHandle);
        ASSERT_TRUE(secondHingeHandle);

        GearConstraintConfiguration gear;
        gear.m_firstHingeConstraintHandle = firstHingeHandle;
        gear.m_secondHingeConstraintHandle = secondHingeHandle;
        constraintConfiguration.m_geometry = gear;
        const ConstraintHandle gearHandle = system.CreateConstraint(worldHandle, constraintConfiguration);
        ASSERT_TRUE(gearHandle);
        ConstraintConfiguration currentGearConfiguration;
        ASSERT_TRUE(system.GetConstraintConfiguration(worldHandle, gearHandle, currentGearConfiguration));
        const GearConstraintConfiguration* currentGear =
            AZStd::get_if<GearConstraintConfiguration>(&currentGearConfiguration.m_geometry);
        ASSERT_TRUE(currentGear);
        EXPECT_EQ(currentGear->m_firstHingeConstraintHandle, firstHingeHandle);
        EXPECT_EQ(currentGear->m_secondHingeConstraintHandle, secondHingeHandle);
        EXPECT_FALSE(system.DestroyConstraint(worldHandle, firstHingeHandle));
        const AZStd::array constraintsToDestroy = {
            firstHingeHandle,
            gearHandle,
            secondHingeHandle,
        };
        EXPECT_TRUE(system.DestroyConstraints(worldHandle, constraintsToDestroy));
        EXPECT_FALSE(system.IsValid(worldHandle, firstHingeHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, secondHingeHandle));
        EXPECT_FALSE(system.IsValid(worldHandle, gearHandle));

        HermitePathConfiguration pathConfiguration;
        pathConfiguration.m_points = {
            {.m_position = AZ::Vector3::CreateZero()},
            {.m_position = AZ::Vector3::CreateAxisX(10.0f)},
        };
        const PathHandle pathHandle = system.CreatePath(pathConfiguration);
        ASSERT_TRUE(pathHandle);

        PathState invalidPathState{
            .m_maximumFraction = 2.0f,
            .m_isLooping = true,
        };
        EXPECT_FALSE(system.GetPathState(PathHandle::Invalid, invalidPathState));
        EXPECT_FLOAT_EQ(invalidPathState.m_maximumFraction, 0.0f);
        EXPECT_FALSE(invalidPathState.m_isLooping);

        PathState pathState;
        ASSERT_TRUE(system.GetPathState(pathHandle, pathState));
        EXPECT_FLOAT_EQ(pathState.m_maximumFraction, 1.0f);
        EXPECT_FALSE(pathState.m_isLooping);

        PathSample pathSample;
        EXPECT_FALSE(system.SamplePath(pathHandle, 2.0f, pathSample));
        EXPECT_FALSE(pathSample);
        ASSERT_TRUE(system.SamplePath(pathHandle, 0.5f, pathSample));
        EXPECT_TRUE(pathSample);
        EXPECT_TRUE(pathSample.m_position.IsClose(AZ::Vector3::CreateAxisX(5.0f)));
        EXPECT_FLOAT_EQ(pathSample.m_fraction, 0.5f);

        PathSample closestPathSample;
        ASSERT_TRUE(system.FindClosestPathPoint(
            pathHandle,
            AZ::Vector3(7.0f, 2.0f, 0.0f),
            0.5f,
            closestPathSample));
        EXPECT_TRUE(closestPathSample);
        EXPECT_TRUE(closestPathSample.m_position.IsClose(AZ::Vector3::CreateAxisX(7.0f), 1.0e-4f));
        EXPECT_GT(closestPathSample.m_fraction, 0.0f);
        EXPECT_LT(closestPathSample.m_fraction, pathState.m_maximumFraction);

        pathConfiguration.m_points[1].m_position = AZ::Vector3::CreateAxisX(20.0f);
        const PathHandle replacementPathHandle = system.CreatePath(pathConfiguration);
        ASSERT_TRUE(replacementPathHandle);

        constraintConfiguration.m_geometry = PathConstraintConfiguration{.m_pathHandle = pathHandle};
        const ConstraintHandle pathConstraintHandle = system.CreateConstraint(worldHandle, constraintConfiguration);
        ASSERT_TRUE(pathConstraintHandle);
        EXPECT_FALSE(system.DestroyPath(pathHandle));
        EXPECT_TRUE(
            system.UpdatePathProperties(
                worldHandle,
                pathConstraintHandle,
                replacementPathHandle,
                1.0f,
                2.0f));
        EXPECT_TRUE(system.UpdatePathMotor(
            worldHandle,
            pathConstraintHandle,
            MotorConfiguration{},
            0.5f,
            1.0f));
        ConstraintConfiguration currentPathConfiguration;
        ASSERT_TRUE(system.GetConstraintConfiguration(worldHandle, pathConstraintHandle, currentPathConfiguration));
        const PathConstraintConfiguration* currentPath =
            AZStd::get_if<PathConstraintConfiguration>(&currentPathConfiguration.m_geometry);
        ASSERT_TRUE(currentPath);
        EXPECT_EQ(currentPath->m_pathHandle, replacementPathHandle);
        EXPECT_FLOAT_EQ(currentPath->m_pathFraction, 1.0f);
        EXPECT_FLOAT_EQ(currentPath->m_maximumFrictionForce, 2.0f);
        EXPECT_TRUE(system.DestroyPath(pathHandle));
        EXPECT_FALSE(system.DestroyPath(replacementPathHandle));
        EXPECT_TRUE(system.DestroyConstraint(worldHandle, pathConstraintHandle));
        EXPECT_TRUE(system.DestroyPath(replacementPathHandle));

        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, CustomPathsRetainProvidersAndExposeDeterministicSamples)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        TestCustomPathProvider provider;
        EXPECT_EQ(
            system.RegisterExtension(static_cast<ICustomPathProvider*>(nullptr), {}).m_status,
            ExtensionRegistrationStatus::Invalid);
        const ExtensionRegistrationResult providerRegistration = system.RegisterExtension(&provider, {});
        ASSERT_TRUE(providerRegistration);
        EXPECT_EQ(
            system.RegisterExtension(&provider, {}).m_status,
            ExtensionRegistrationStatus::AlreadyRegistered);

        CustomPathConfiguration invalidConfiguration;
        invalidConfiguration.m_providerId = provider.GetId();
        EXPECT_FALSE(system.CreatePath(invalidConfiguration));

        CustomPathConfiguration configuration;
        configuration.m_data = {8};
        configuration.m_providerId = provider.GetId();
        configuration.m_isLooping = true;
        const PathHandle pathHandle = system.CreatePath(configuration);
        ASSERT_TRUE(pathHandle);
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::InUse);

        PathState state;
        ASSERT_TRUE(system.GetPathState(pathHandle, state));
        EXPECT_FLOAT_EQ(state.m_maximumFraction, 8.0f);
        EXPECT_TRUE(state.m_isLooping);

        CustomPathInfo info;
        ASSERT_TRUE(system.GetCustomPathInfo(pathHandle, info));
        EXPECT_EQ(info.m_providerId, provider.GetId());
        EXPECT_EQ(info.m_providerVersion, provider.GetVersion());
        EXPECT_NE(info.m_sourceHash, 0);

        PathSample sample;
        ASSERT_TRUE(system.SamplePath(pathHandle, 3.0f, sample));
        EXPECT_TRUE(sample.m_position.IsClose(AZ::Vector3::CreateAxisX(3.0f)));
        EXPECT_TRUE(sample.m_tangent.IsClose(AZ::Vector3::CreateAxisX()));
        EXPECT_TRUE(sample.m_normal.IsClose(AZ::Vector3::CreateAxisZ()));
        EXPECT_TRUE(sample.m_binormal.IsClose(AZ::Vector3::CreateAxisY()));

        PathSample closestSample;
        ASSERT_TRUE(system.FindClosestPathPoint(
            pathHandle,
            AZ::Vector3(5.0f, 4.0f, 0.0f),
            1.0f,
            closestSample));
        EXPECT_FLOAT_EQ(closestSample.m_fraction, 5.0f);
        EXPECT_TRUE(closestSample.m_position.IsClose(AZ::Vector3::CreateAxisX(5.0f)));
        EXPECT_GT(provider.m_closestCount.load(), 0);
        EXPECT_GT(provider.m_sampleCount.load(), 2);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{.m_radius = 0.25f};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration firstBodyConfiguration;
        firstBodyConfiguration.m_shapeHandle = shapeHandle;
        firstBodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle firstBodyHandle = system.CreateBody(worldHandle, firstBodyConfiguration);
        ASSERT_TRUE(firstBodyHandle);

        BodyConfiguration secondBodyConfiguration;
        secondBodyConfiguration.m_shapeHandle = shapeHandle;
        secondBodyConfiguration.m_transform.m_position.m_x = 1.0;
        const BodyHandle secondBodyHandle = system.CreateBody(worldHandle, secondBodyConfiguration);
        ASSERT_TRUE(secondBodyHandle);

        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = firstBodyHandle;
        constraintConfiguration.m_secondBodyHandle = secondBodyHandle;
        constraintConfiguration.m_geometry = PathConstraintConfiguration{
            .m_pathHandle = pathHandle,
        };
        const AZ::u32 sampleCountBeforeConstraint = provider.m_sampleCount.load();
        const ConstraintHandle constraintHandle = system.CreateConstraint(
            worldHandle,
            constraintConfiguration);
        ASSERT_TRUE(constraintHandle);
        ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        EXPECT_GT(provider.m_sampleCount.load(), sampleCountBeforeConstraint);

        const PathHandle duplicatePathHandle = system.CreatePath(configuration);
        ASSERT_TRUE(duplicatePathHandle);
        CustomPathInfo duplicateInfo;
        ASSERT_TRUE(system.GetCustomPathInfo(duplicatePathHandle, duplicateInfo));
        EXPECT_EQ(duplicateInfo.m_sourceHash, info.m_sourceHash);

        EXPECT_TRUE(system.DestroyPath(duplicatePathHandle));
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::InUse);
        EXPECT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, secondBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, firstBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
        EXPECT_TRUE(system.DestroyPath(pathHandle));
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::Success);
        EXPECT_EQ(
            system.UnregisterExtension(providerRegistration.m_handle),
            ExtensionRegistrationStatus::NotRegistered);
    }

    TEST(SimulationTests, BodyEnumerationIsBoundedStableAndFilterable)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_motionType = MotionType::Static;
        bodyConfiguration.m_activate = false;
        const BodyHandle staticBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(staticBodyHandle);

        bodyConfiguration.m_motionType = MotionType::Dynamic;
        bodyConfiguration.m_activate = true;
        const BodyHandle dynamicBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(dynamicBodyHandle);

        AZStd::array<BodyHandle, 1> boundedBodies;
        QueryResult result = system.GetBodies(
            worldHandle,
            BodyKind::None,
            false,
            boundedBodies);
        EXPECT_EQ(result.m_hitCount, 1);
        EXPECT_EQ(result.m_requiredHitCount, 2);
        EXPECT_EQ(boundedBodies[0], staticBodyHandle);

        AZStd::array<BodyHandle, 2> activeBodies;
        result = system.GetBodies(
            worldHandle,
            BodyKind::Rigid,
            true,
            activeBodies);
        EXPECT_EQ(result.m_hitCount, 1);
        EXPECT_EQ(result.m_requiredHitCount, 1);
        EXPECT_EQ(activeBodies[0], dynamicBodyHandle);

        const AZStd::array bodyHandles = {
            staticBodyHandle,
            dynamicBodyHandle,
        };
        ASSERT_TRUE(system.DeactivateBodies(worldHandle, bodyHandles));
        result = system.GetBodies(
            worldHandle,
            BodyKind::Rigid,
            true,
            activeBodies);
        EXPECT_EQ(result.m_hitCount, 0);
        EXPECT_EQ(result.m_requiredHitCount, 0);

        BroadPhaseAabb activationBounds;
        activationBounds.m_halfExtents = AZ::Vector3::CreateOne();
        ASSERT_TRUE(system.ActivateBodiesInBounds(worldHandle, activationBounds));
        result = system.GetBodies(
            worldHandle,
            BodyKind::Rigid,
            true,
            activeBodies);
        EXPECT_EQ(result.m_hitCount, 1);
        EXPECT_EQ(result.m_requiredHitCount, 1);
        EXPECT_EQ(activeBodies[0], dynamicBodyHandle);

        activationBounds.m_halfExtents.SetX(-1.0f);
        EXPECT_FALSE(system.ActivateBodiesInBounds(worldHandle, activationBounds));
        activationBounds.m_halfExtents = AZ::Vector3::CreateOne();
        EXPECT_FALSE(
            system.ActivateBodiesInBounds(
                worldHandle,
                activationBounds,
                ObjectLayer{999}));

        ASSERT_TRUE(system.DeactivateBodies(worldHandle, bodyHandles));
        ASSERT_TRUE(system.ActivateBodies(worldHandle, bodyHandles));
        result = system.GetBodies(
            worldHandle,
            BodyKind::Rigid,
            true,
            activeBodies);
        EXPECT_EQ(result.m_hitCount, 1);
        EXPECT_EQ(result.m_requiredHitCount, 1);
        EXPECT_EQ(activeBodies[0], dynamicBodyHandle);

        const AZStd::array invalidBodyHandles = {
            dynamicBodyHandle,
            BodyHandle::Invalid,
        };
        EXPECT_FALSE(system.DeactivateBodies(worldHandle, invalidBodyHandles));
        result = system.GetBodies(
            worldHandle,
            BodyKind::Rigid,
            true,
            activeBodies);
        EXPECT_EQ(result.m_hitCount, 1);
        EXPECT_EQ(result.m_requiredHitCount, 1);
        EXPECT_EQ(activeBodies[0], dynamicBodyHandle);

        EXPECT_TRUE(system.InvalidateBodyContactCache(worldHandle, dynamicBodyHandle));
        EXPECT_FALSE(system.InvalidateBodyContactCache(worldHandle, BodyHandle::Invalid));

        ASSERT_TRUE(system.DeactivateBody(worldHandle, dynamicBodyHandle));
        result = system.GetBodies(
            worldHandle,
            BodyKind::Rigid,
            true,
            activeBodies);
        EXPECT_EQ(result.m_hitCount, 0);
        EXPECT_EQ(result.m_requiredHitCount, 0);

        EXPECT_TRUE(system.DestroyBody(worldHandle, dynamicBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, staticBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }

    TEST(SimulationTests, HingeMotorCreationAppliesItsStateAndVelocityTarget)
    {
        SystemConfiguration systemConfiguration = CreateSerialSystemConfiguration();
        systemConfiguration.m_defaultWorld.m_gravity = AZ::Vector3::CreateZero();
        Runtime system(systemConfiguration, nullptr);
        ASSERT_TRUE(system);

        const WorldHandle worldHandle = system.GetDefaultWorldHandle();
        ShapeConfiguration shapeConfiguration;
        shapeConfiguration.m_geometry = SphereShapeConfiguration{};
        const ShapeHandle shapeHandle = system.CreateShape(worldHandle, shapeConfiguration);
        ASSERT_TRUE(shapeHandle);

        BodyConfiguration bodyConfiguration;
        bodyConfiguration.m_shapeHandle = shapeHandle;
        bodyConfiguration.m_isSensor = true;
        bodyConfiguration.m_motionType = MotionType::Static;
        const BodyHandle staticBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(staticBodyHandle);

        bodyConfiguration.m_motionType = MotionType::Dynamic;
        const BodyHandle dynamicBodyHandle = system.CreateBody(worldHandle, bodyConfiguration);
        ASSERT_TRUE(dynamicBodyHandle);

        HingeConstraintConfiguration hingeConfiguration;
        hingeConfiguration.m_motor.m_state = MotorState::Velocity;
        hingeConfiguration.m_targetAngularVelocity = 2.0f;
        ConstraintConfiguration constraintConfiguration;
        constraintConfiguration.m_firstBodyHandle = staticBodyHandle;
        constraintConfiguration.m_secondBodyHandle = dynamicBodyHandle;
        constraintConfiguration.m_geometry = hingeConfiguration;
        const ConstraintHandle constraintHandle = system.CreateConstraint(worldHandle, constraintConfiguration);
        ASSERT_TRUE(constraintHandle);

        for (AZ::u32 step = 0; step < 30; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }

        BodyState state;
        ASSERT_TRUE(system.GetBodyState(worldHandle, dynamicBodyHandle, state));
        EXPECT_NEAR(state.m_angularVelocity.GetY(), 2.0f, 0.05f);

        const StateSnapshotHandle snapshotHandle = system.CaptureWorldState(worldHandle);
        ASSERT_TRUE(snapshotHandle);
        MotorConfiguration motor;
        motor.m_state = MotorState::Velocity;
        ASSERT_TRUE(system.UpdateHingeMotor(
            worldHandle,
            constraintHandle,
            motor,
            0.0f,
            -1.0f));
        EXPECT_FALSE(system.RestoreWorldState(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.SetBodyVelocities(
            worldHandle,
            dynamicBodyHandle,
            AZ::Vector3::CreateZero(),
            AZ::Vector3::CreateZero()));
        for (AZ::u32 step = 0; step < 30; ++step)
        {
            ASSERT_TRUE(system.StepWorld(worldHandle, 1.0f / 60.0f));
        }
        ASSERT_TRUE(system.GetBodyState(worldHandle, dynamicBodyHandle, state));
        EXPECT_NEAR(state.m_angularVelocity.GetY(), -1.0f, 0.05f);

        ConstraintMeasurements measurements;
        ASSERT_TRUE(system.GetConstraintMeasurements(worldHandle, constraintHandle, measurements));
        const HingeMeasurements* hingeMeasurements = AZStd::get_if<HingeMeasurements>(&measurements);
        ASSERT_TRUE(hingeMeasurements);
        EXPECT_TRUE(AZ::IsFiniteFloat(hingeMeasurements->m_angle));

        motor.m_state = MotorState::PositionAndVelocity;
        motor.m_spring.m_mode = SpringMode::MassNormalizedStiffnessAndDamping;
        motor.m_spring.m_frequencyOrStiffness = 3.0f;
        motor.m_spring.m_damping = 0.5f;
        ASSERT_TRUE(system.UpdateHingeMotor(
            worldHandle,
            constraintHandle,
            motor,
            0.25f,
            -0.5f));

        ConstraintConfiguration appliedConfiguration;
        ASSERT_TRUE(system.GetConstraintConfiguration(
            worldHandle,
            constraintHandle,
            appliedConfiguration));
        const HingeConstraintConfiguration* appliedHinge =
            AZStd::get_if<HingeConstraintConfiguration>(&appliedConfiguration.m_geometry);
        ASSERT_TRUE(appliedHinge);
        EXPECT_EQ(appliedHinge->m_motor.m_state, MotorState::PositionAndVelocity);
        EXPECT_EQ(
            appliedHinge->m_motor.m_spring.m_mode,
            SpringMode::MassNormalizedStiffnessAndDamping);
        EXPECT_FLOAT_EQ(appliedHinge->m_motor.m_spring.m_frequencyOrStiffness, 3.0f);
        EXPECT_FLOAT_EQ(appliedHinge->m_motor.m_spring.m_damping, 0.5f);
        EXPECT_FLOAT_EQ(appliedHinge->m_targetAngle, 0.25f);
        EXPECT_FLOAT_EQ(appliedHinge->m_targetAngularVelocity, -0.5f);

        const AZ::Quaternion bodySpaceTarget =
            AZ::Quaternion::CreateRotationY(0.5f);
        ASSERT_TRUE(system.SetHingeTargetOrientation(
            worldHandle,
            constraintHandle,
            bodySpaceTarget));
        ASSERT_TRUE(system.GetConstraintConfiguration(
            worldHandle,
            constraintHandle,
            appliedConfiguration));
        appliedHinge = AZStd::get_if<HingeConstraintConfiguration>(&appliedConfiguration.m_geometry);
        ASSERT_TRUE(appliedHinge);
        EXPECT_NEAR(appliedHinge->m_targetAngle, 0.5f, 1.0e-5f);
        EXPECT_FALSE(system.SetHingeTargetOrientation(
            worldHandle,
            constraintHandle,
            AZ::Quaternion::CreateZero()));

        EXPECT_TRUE(system.DestroyStateSnapshot(worldHandle, snapshotHandle));
        EXPECT_TRUE(system.DestroyConstraint(worldHandle, constraintHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, dynamicBodyHandle));
        EXPECT_TRUE(system.DestroyBody(worldHandle, staticBodyHandle));
        EXPECT_TRUE(system.DestroyShape(worldHandle, shapeHandle));
    }
} // namespace Jolt
