/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SystemInternal.h>

#include <Jolt/AssetBuilderSystem.h>
#include <Jolt/ComponentDependencyManager.h>
#include <Jolt/CustomConvexShape.h>
#include <Jolt/CustomShapeInternal.h>
#include <Jolt/DebugRenderer.h>
#include <Jolt/FloatEnvironment.h>
#include <Jolt/NativeRuntime.h>
#include <Jolt/NativeShapeFactory.h>
#include <Jolt/OperationInternal.h>
#include <Jolt/Profiler.h>

#include <Jolt/HandleEncoding.h>
#include <Jolt/World.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Jobs/JobEmpty.h>
#include <AzCore/Jobs/JobFunction.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Utils/TypeHash.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/utility/move.h>

#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Compute/CPU/ComputeSystemCPU.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Physics/Constraints/PathConstraintPathHermite.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/NarrowPhaseStats.h>
#include <Jolt/Core/UnorderedSet.h>
#include <Jolt/Shaders/HairWrapper.h>
#include <Jolt/Skeleton/SkeletalAnimation.h>
#include <Jolt/Skeleton/Skeleton.h>

#include <cmath>
#include <cstring>

namespace Jolt
{
    namespace
    {
        constexpr AZ::u32 MaximumSubGroupCount = 65'536;
        constexpr AZ::u32 CookedShapeArchiveFormatVersion = 2;
        constexpr AZ::u32 SkeletalAnimationArchiveFormatVersion = 1;
        constexpr AZ::u32 SkeletonDefinitionArchiveFormatVersion = 1;
        constexpr AZ::u32 SoftBodyDefinitionArchiveFormatVersion = 1;
        constexpr size_t MaximumNativeArchiveSize = size_t{1} << 30;

        [[nodiscard]]
        AZ::u64 CalculateCookedShapeArchiveHash(
            const AZStd::span<const AZ::u8> binaryState,
            const AZStd::span<const CustomShapeDependency> dependencies,
            const AZ::TypeId& providerId,
            const AZ::u64 providerVersion)
        {
            AZ::HashValue64 hash = AZ::TypeHash64(
                binaryState.data(),
                binaryState.size());
            hash = AZ::TypeHash64(providerId, hash);
            hash = AZ::TypeHash64(providerVersion, hash);
            hash = AZ::TypeHash64(dependencies.size(), hash);
            for (const CustomShapeDependency& dependency : dependencies)
            {
                hash = AZ::TypeHash64(
                    reinterpret_cast<const AZ::u8*>(dependency.m_path.data()),
                    dependency.m_path.size(),
                    hash);
                hash = AZ::TypeHash64(dependency.m_contentHash, hash);
            }
            return static_cast<AZ::u64>(hash);
        }

        [[nodiscard]]
        ShapeKind FromNativeShapeKind(
            const JPH::EShapeSubType shapeKind)
        {
            switch (shapeKind)
            {
            case JPH::EShapeSubType::Box:
                return ShapeKind::Box;
            case JPH::EShapeSubType::Capsule:
                return ShapeKind::Capsule;
            case JPH::EShapeSubType::ConvexHull:
                return ShapeKind::ConvexHull;
            case JPH::EShapeSubType::User1:
                return ShapeKind::Custom;
            case JPH::EShapeSubType::UserConvex1:
                return ShapeKind::CustomConvex;
            case JPH::EShapeSubType::Cylinder:
                return ShapeKind::Cylinder;
            case JPH::EShapeSubType::Empty:
                return ShapeKind::Empty;
            case JPH::EShapeSubType::HeightField:
                return ShapeKind::Heightfield;
            case JPH::EShapeSubType::Mesh:
                return ShapeKind::Mesh;
            case JPH::EShapeSubType::MutableCompound:
                return ShapeKind::MutableCompound;
            case JPH::EShapeSubType::OffsetCenterOfMass:
                return ShapeKind::OffsetCenterOfMass;
            case JPH::EShapeSubType::Plane:
                return ShapeKind::Plane;
            case JPH::EShapeSubType::RotatedTranslated:
                return ShapeKind::RotatedTranslated;
            case JPH::EShapeSubType::Scaled:
                return ShapeKind::Scaled;
            case JPH::EShapeSubType::SoftBody:
                return ShapeKind::SoftBody;
            case JPH::EShapeSubType::Sphere:
                return ShapeKind::Sphere;
            case JPH::EShapeSubType::StaticCompound:
                return ShapeKind::StaticCompound;
            case JPH::EShapeSubType::TaperedCapsule:
                return ShapeKind::TaperedCapsule;
            case JPH::EShapeSubType::TaperedCylinder:
                return ShapeKind::TaperedCylinder;
            case JPH::EShapeSubType::Triangle:
                return ShapeKind::Triangle;
            default:
                return ShapeKind::None;
            }
        }

        [[nodiscard]]
        AZ::Transform FromNativeTransform(
            JPH::Mat44Arg transform)
        {
            const JPH::Vec3 firstColumn = transform.GetColumn3(0);
            const JPH::Vec3 secondColumn = transform.GetColumn3(1);
            const JPH::Vec3 thirdColumn = transform.GetColumn3(2);
            const JPH::Vec3 translation = transform.GetTranslation();
            return AZ::Transform::CreateFromMatrix3x3AndTranslation(
                AZ::Matrix3x3::CreateFromColumns(
                    {firstColumn.GetX(), firstColumn.GetY(), firstColumn.GetZ()},
                    {secondColumn.GetX(), secondColumn.GetY(), secondColumn.GetZ()},
                    {thirdColumn.GetX(), thirdColumn.GetY(), thirdColumn.GetZ()}),
                {translation.GetX(), translation.GetY(), translation.GetZ()});
        }

        [[nodiscard]]
        PathSample FromNativePathSample(
            const JPH::PathConstraintPath& path,
            const float fraction)
        {
            JPH::Vec3 position;
            JPH::Vec3 tangent;
            JPH::Vec3 normal;
            JPH::Vec3 binormal;
            path.GetPointOnPath(fraction, position, tangent, normal, binormal);
            return {
                .m_position = {position.GetX(), position.GetY(), position.GetZ()},
                .m_tangent = {tangent.GetX(), tangent.GetY(), tangent.GetZ()},
                .m_normal = {normal.GetX(), normal.GetY(), normal.GetZ()},
                .m_binormal = {binormal.GetX(), binormal.GetY(), binormal.GetZ()},
                .m_fraction = fraction,
                .m_valid = true,
            };
        }

        [[nodiscard]]
        bool IsValidCustomPathPoint(
            const CustomPathPoint& point)
        {
            if (!point.m_position.IsFinite()
                || !point.m_tangent.IsFinite()
                || point.m_tangent.IsZero()
                || !point.m_normal.IsFinite()
                || point.m_normal.IsZero())
            {
                return false;
            }

            const AZ::Vector3 tangent = point.m_tangent.GetNormalized();
            const AZ::Vector3 normal = point.m_normal.GetNormalized();
            return AZStd::abs(tangent.Dot(normal)) < 1.0f - 1.0e-4f;
        }

        class CustomPathAdapter final
            : public JPH::PathConstraintPath
        {
            JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_NO_EXPORT, CustomPathAdapter)

        public:
            CustomPathAdapter() = default;

            CustomPathAdapter(
                ICustomPathProvider& provider,
                AZStd::vector<AZ::u8> data,
                const float maximumFraction)
                : m_provider(&provider)
                , m_data(AZStd::move(data))
                , m_maximumFraction(maximumFraction)
            {
            }

            float GetPathMaxFraction() const override
            {
                return m_maximumFraction;
            }

            float GetClosestPoint(
                const JPH::Vec3Arg position,
                const float fractionHint) const override
            {
                const float clampedHint = AZStd::clamp(fractionHint, 0.0f, m_maximumFraction);
                if (!m_provider)
                {
                    return clampedHint;
                }

                float fraction = clampedHint;
                if (!m_provider->FindClosestFraction(
                        m_data,
                        {position.GetX(), position.GetY(), position.GetZ()},
                        clampedHint,
                        fraction)
                    || !AZ::IsFiniteFloat(fraction)
                    || fraction < 0.0f
                    || fraction > m_maximumFraction)
                {
                    AZ_Assert(false, "A custom path provider returned an invalid closest fraction.");
                    return clampedHint;
                }

                return fraction;
            }

            void GetPointOnPath(
                const float fraction,
                JPH::Vec3& position,
                JPH::Vec3& tangent,
                JPH::Vec3& normal,
                JPH::Vec3& binormal) const override
            {
                CustomPathPoint point;
                if (!m_provider
                    || !m_provider->Sample(m_data, fraction, point)
                    || !IsValidCustomPathPoint(point))
                {
                    AZ_Assert(false, "A custom path provider returned an invalid path point.");
                    position = JPH::Vec3::sZero();
                    tangent = JPH::Vec3::sAxisX();
                    normal = JPH::Vec3::sAxisZ();
                    binormal = JPH::Vec3::sAxisY();
                    return;
                }

                const AZ::Vector3 normalizedTangent = point.m_tangent.GetNormalized();
                const AZ::Vector3 orthogonalNormal =
                    (point.m_normal - normalizedTangent * normalizedTangent.Dot(point.m_normal)).GetNormalized();
                const AZ::Vector3 normalizedBinormal = orthogonalNormal.Cross(normalizedTangent);
                position = JPH::Vec3(
                    point.m_position.GetX(),
                    point.m_position.GetY(),
                    point.m_position.GetZ());
                tangent = JPH::Vec3(
                    normalizedTangent.GetX(),
                    normalizedTangent.GetY(),
                    normalizedTangent.GetZ());
                normal = JPH::Vec3(
                    orthogonalNormal.GetX(),
                    orthogonalNormal.GetY(),
                    orthogonalNormal.GetZ());
                binormal = JPH::Vec3(
                    normalizedBinormal.GetX(),
                    normalizedBinormal.GetY(),
                    normalizedBinormal.GetZ());
            }

        private:
            ICustomPathProvider* m_provider = nullptr;
            AZStd::vector<AZ::u8> m_data;
            float m_maximumFraction = 0.0f;
        };

        AZ_PUSH_DISABLE_WARNING(4505, "-Wunused-function")
        JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(CustomPathAdapter)
        {
            JPH_ADD_BASE_CLASS(CustomPathAdapter, JPH::PathConstraintPath)
        }
        AZ_POP_DISABLE_WARNING

        class GroupFilterAdapter final
            : public JPH::GroupFilter
        {
            JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_NO_EXPORT, GroupFilterAdapter)

        public:
            GroupFilterAdapter() = default;

            explicit GroupFilterAdapter(
                IGroupFilter* callbacks)
                : m_callbacks(callbacks)
            {
            }

            [[nodiscard]]
            bool CanCollide(
                const JPH::CollisionGroup& firstGroup,
                const JPH::CollisionGroup& secondGroup) const override
            {
                if (!m_callbacks)
                {
                    return false;
                }

                return m_callbacks->CanCollide(
                    {
                        .m_groupId = CollisionGroupId(firstGroup.GetGroupID()),
                        .m_subGroupId = CollisionSubGroupId(firstGroup.GetSubGroupID()),
                    },
                    {
                        .m_groupId = CollisionGroupId(secondGroup.GetGroupID()),
                        .m_subGroupId = CollisionSubGroupId(secondGroup.GetSubGroupID()),
                    });
            }

            [[nodiscard]]
            IGroupFilter* GetCallbacks() const
            {
                return m_callbacks;
            }

        private:
            IGroupFilter* m_callbacks = nullptr;
        };

        AZ_PUSH_DISABLE_WARNING(4505, "-Wunused-function")
        JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(GroupFilterAdapter)
        {
            JPH_ADD_BASE_CLASS(GroupFilterAdapter, JPH::GroupFilter)
        }
        AZ_POP_DISABLE_WARNING

        class NativeArchiveWriter final
            : public JPH::StreamOut
        {
        public:
            void WriteBytes(
                const void* data,
                const size_t byteCount) override
            {
                if (m_failed || byteCount > MaximumNativeArchiveSize - m_data.size())
                {
                    m_failed = true;
                    return;
                }

                const size_t previousSize = m_data.size();
                m_data.resize(previousSize + byteCount);
                if (byteCount > 0)
                {
                    std::memcpy(m_data.data() + previousSize, data, byteCount);
                }
            }

            [[nodiscard]]
            bool IsFailed() const override
            {
                return m_failed;
            }

            [[nodiscard]]
            AZStd::vector<AZ::u8> TakeData()
            {
                return AZStd::move(m_data);
            }

        private:
            AZStd::vector<AZ::u8> m_data;
            bool m_failed = false;
        };

        class NativeArchiveReader final
            : public JPH::StreamIn
        {
        public:
            explicit NativeArchiveReader(
                const AZStd::span<const AZ::u8> data)
                : m_data(data)
            {
            }

            void ReadBytes(
                void* data,
                const size_t byteCount) override
            {
                if (m_failed || byteCount > m_data.size() - m_offset)
                {
                    m_failed = true;
                    m_eof = true;
                    if (byteCount > 0)
                    {
                        std::memset(data, 0, byteCount);
                    }
                    return;
                }

                if (byteCount > 0)
                {
                    std::memcpy(data, m_data.data() + m_offset, byteCount);
                    m_offset += byteCount;
                }
            }

            [[nodiscard]]
            bool IsEOF() const override
            {
                return m_eof;
            }

            [[nodiscard]]
            bool IsFailed() const override
            {
                return m_failed;
            }

            [[nodiscard]]
            bool IsFullyConsumed() const
            {
                return m_offset == m_data.size();
            }

        private:
            AZStd::span<const AZ::u8> m_data;
            size_t m_offset = 0;
            bool m_eof = false;
            bool m_failed = false;
        };

        [[nodiscard]]
        constexpr AZ::u64 MixGroupFilterValue(AZ::u64 value)
        {
            value += 0x9e3779b97f4a7c15ULL;
            value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
            return value ^ (value >> 31);
        }

        [[nodiscard]]
        constexpr AZ::u64 GetSubGroupPairHash(
            CollisionSubGroupId firstSubGroup,
            CollisionSubGroupId secondSubGroup)
        {
            if (secondSubGroup.GetValue() < firstSubGroup.GetValue())
            {
                const CollisionSubGroupId temporary = firstSubGroup;
                firstSubGroup = secondSubGroup;
                secondSubGroup = temporary;
            }
            const AZ::u64 pair = static_cast<AZ::u64>(firstSubGroup.GetValue()) << 32
                | secondSubGroup.GetValue();
            return MixGroupFilterValue(pair);
        }

        [[nodiscard]]
        bool IsValidSubGroupPair(
            const CollisionSubGroupId firstSubGroup,
            const CollisionSubGroupId secondSubGroup,
            const AZ::u32 subGroupCount)
        {
            return firstSubGroup
                && secondSubGroup
                && firstSubGroup != secondSubGroup
                && firstSubGroup.GetValue() < subGroupCount
                && secondSubGroup.GetValue() < subGroupCount;
        }

        [[nodiscard]]
        JPH::Mat44 ToNativeSystemTransform(
            const AZ::Transform& transform)
        {
            JPH::Mat44 result = JPH::Mat44::sIdentity();
            const AZ::Vector3 basisX = transform.GetBasisX();
            const AZ::Vector3 basisY = transform.GetBasisY();
            const AZ::Vector3 basisZ = transform.GetBasisZ();
            const AZ::Vector3 translation = transform.GetTranslation();
            result.SetColumn3(0, {basisX.GetX(), basisX.GetY(), basisX.GetZ()});
            result.SetColumn3(1, {basisY.GetX(), basisY.GetY(), basisY.GetZ()});
            result.SetColumn3(2, {basisZ.GetX(), basisZ.GetY(), basisZ.GetZ()});
            result.SetTranslation({translation.GetX(), translation.GetY(), translation.GetZ()});
            return result;
        }

        [[nodiscard]]
        JPH::Quat ToNativeSystemRotation(
            const AZ::Quaternion& rotation)
        {
            const AZ::Quaternion normalizedRotation = rotation.GetNormalized();
            return {
                normalizedRotation.GetX(),
                normalizedRotation.GetY(),
                normalizedRotation.GetZ(),
                normalizedRotation.GetW(),
            };
        }

        [[nodiscard]]
        AZ::Quaternion FromNativeSystemRotation(
            const JPH::QuatArg rotation)
        {
            return AZ::Quaternion(
                rotation.GetX(),
                rotation.GetY(),
                rotation.GetZ(),
                rotation.GetW());
        }

        template<class SourceContainer, class Target, class Converter>
        [[nodiscard]]
        QueryResult CopyConvertedRange(
            const SourceContainer& source,
            const AZStd::span<Target> target,
            const Converter& converter)
        {
            const size_t copiedCount = AZStd::min(source.size(), target.size());
            for (size_t index = 0; index < copiedCount; ++index)
            {
                target[index] = converter(source[index]);
            }
            return {
                .m_hitCount = aznumeric_cast<AZ::u32>(copiedCount),
                .m_requiredHitCount = aznumeric_cast<AZ::u32>(source.size()),
            };
        }

        [[nodiscard]]
        JPH::Vec3 ToNativeSystemVector(
            const AZ::Vector3& vector)
        {
            return {vector.GetX(), vector.GetY(), vector.GetZ()};
        }

        [[nodiscard]]
        bool IsValidHairGradient(
            const HairGradient& gradient)
        {
            return AZ::IsFiniteFloat(gradient.m_minimum)
                && AZ::IsFiniteFloat(gradient.m_maximum)
                && AZ::IsFiniteFloat(gradient.m_minimumFraction)
                && AZ::IsFiniteFloat(gradient.m_maximumFraction)
                && gradient.m_minimumFraction >= 0.0f
                && gradient.m_maximumFraction <= 1.0f
                && gradient.m_minimumFraction < gradient.m_maximumFraction;
        }

        [[nodiscard]]
        JPH::HairSettings::Gradient ToNativeHairGradient(
            const HairGradient& gradient)
        {
            return {
                gradient.m_minimum,
                gradient.m_maximum,
                gradient.m_minimumFraction,
                gradient.m_maximumFraction,
            };
        }
    } // namespace

    AZStd::unique_ptr<System> CreateAssetBuilderSystem()
    {
        SystemConfiguration configuration;
        configuration.m_createDefaultWorld = false;
        AZStd::unique_ptr<System> system = AZStd::make_unique<System>(AZStd::move(configuration));
        if (!*system)
        {
            return {};
        }

        return system;
    }

    RuntimeImplementation::RuntimeImplementation(
        SystemConfiguration configuration,
        AZ::JobContext* jobContext,
        const SystemRegistration registration)
        : m_nativeRuntime(configuration.m_softBodyTriangleThickness)
        , m_configuration(AZStd::move(configuration))
        , m_jobContext(jobContext)
    {
        if (!m_nativeRuntime)
        {
            return;
        }

        if (registration == SystemRegistration::Global && GetRuntime())
        {
            AZ_Error("Jolt", false, "Only one Jolt system can be active at a time.");
            return;
        }

        m_operationPool = Internal::OperationPool::Create(m_jobContext);
        m_debugRenderer = GetNativeDebugRenderer();
        m_dependencyManager = AZStd::make_unique<ComponentDependencyManager>();
        if (m_configuration.m_createDefaultWorld)
        {
            m_defaultWorldHandle = CreateWorld(m_configuration.m_defaultWorld);
            if (!m_defaultWorldHandle)
            {
                AZ_Error("Jolt", false, "Failed to create the configured default world.");
                return;
            }
        }
        m_initialized = true;
    }

    RuntimeImplementation::~RuntimeImplementation()
    {
        if (m_operationPool)
        {
            m_operationPool->Drain();
            m_operationPool->Shutdown();
            m_operationPool = nullptr;
        }
    }

    Runtime::Runtime(
        SystemConfiguration configuration,
        AZ::JobContext* jobContext,
        const SystemRegistration registration)
        : RuntimeImplementation(AZStd::move(configuration), jobContext, registration)
    {
        if (!static_cast<bool>(*this) || registration == SystemRegistration::Isolated)
        {
            return;
        }

        m_registered = PublishCapabilities();
        if (!m_registered)
        {
            AZ_Error("Jolt", false, "Only one Jolt system can be active at a time.");
            Invalidate();
        }
    }

    Runtime::~Runtime()
    {
        if (m_registered)
        {
            UnpublishCapabilities();
        }
    }

    bool Runtime::PublishCapabilities()
    {
        RuntimeConfiguration* expectedRuntimeConfiguration = nullptr;
        if (!RuntimeConfiguration::s_instance.compare_exchange_strong(
                expectedRuntimeConfiguration,
                static_cast<RuntimeConfiguration*>(this),
                AZStd::memory_order_acq_rel,
                AZStd::memory_order_acquire))
        {
            return false;
        }

        Extensions::s_instance.store(static_cast<Extensions*>(this), AZStd::memory_order_release);
        Materials::s_instance.store(static_cast<Materials*>(this), AZStd::memory_order_release);
        CollisionFilters::s_instance.store(static_cast<CollisionFilters*>(this), AZStd::memory_order_release);
        Cooking::s_instance.store(static_cast<Cooking*>(this), AZStd::memory_order_release);
        Paths::s_instance.store(static_cast<Paths*>(this), AZStd::memory_order_release);
        Skeletons::s_instance.store(static_cast<Skeletons*>(this), AZStd::memory_order_release);
        Scenes::s_instance.store(static_cast<Scenes*>(this), AZStd::memory_order_release);
        Worlds::s_instance.store(static_cast<Worlds*>(this), AZStd::memory_order_release);
        WorldSimulation::s_instance.store(static_cast<WorldSimulation*>(this), AZStd::memory_order_release);
        WorldQueries::s_instance.store(static_cast<WorldQueries*>(this), AZStd::memory_order_release);
        Shapes::s_instance.store(static_cast<Shapes*>(this), AZStd::memory_order_release);
        Bodies::s_instance.store(static_cast<Bodies*>(this), AZStd::memory_order_release);
        Constraints::s_instance.store(static_cast<Constraints*>(this), AZStd::memory_order_release);
        Characters::s_instance.store(static_cast<Characters*>(this), AZStd::memory_order_release);
        Vehicles::s_instance.store(static_cast<Vehicles*>(this), AZStd::memory_order_release);
        Ragdolls::s_instance.store(static_cast<Ragdolls*>(this), AZStd::memory_order_release);
        SoftBodies::s_instance.store(static_cast<SoftBodies*>(this), AZStd::memory_order_release);
        Hair::s_instance.store(static_cast<Hair*>(this), AZStd::memory_order_release);
        Rollback::s_instance.store(static_cast<Rollback*>(this), AZStd::memory_order_release);
        Diagnostics::s_instance.store(static_cast<Diagnostics*>(this), AZStd::memory_order_release);
        return true;
    }

    void Runtime::UnpublishCapabilities()
    {
        RuntimeConfiguration::s_instance.store(nullptr, AZStd::memory_order_release);
        Extensions::s_instance.store(nullptr, AZStd::memory_order_release);
        Materials::s_instance.store(nullptr, AZStd::memory_order_release);
        CollisionFilters::s_instance.store(nullptr, AZStd::memory_order_release);
        Cooking::s_instance.store(nullptr, AZStd::memory_order_release);
        Paths::s_instance.store(nullptr, AZStd::memory_order_release);
        Skeletons::s_instance.store(nullptr, AZStd::memory_order_release);
        Scenes::s_instance.store(nullptr, AZStd::memory_order_release);
        Worlds::s_instance.store(nullptr, AZStd::memory_order_release);
        WorldSimulation::s_instance.store(nullptr, AZStd::memory_order_release);
        WorldQueries::s_instance.store(nullptr, AZStd::memory_order_release);
        Shapes::s_instance.store(nullptr, AZStd::memory_order_release);
        Bodies::s_instance.store(nullptr, AZStd::memory_order_release);
        Constraints::s_instance.store(nullptr, AZStd::memory_order_release);
        Characters::s_instance.store(nullptr, AZStd::memory_order_release);
        Vehicles::s_instance.store(nullptr, AZStd::memory_order_release);
        Ragdolls::s_instance.store(nullptr, AZStd::memory_order_release);
        SoftBodies::s_instance.store(nullptr, AZStd::memory_order_release);
        Hair::s_instance.store(nullptr, AZStd::memory_order_release);
        Rollback::s_instance.store(nullptr, AZStd::memory_order_release);
        Diagnostics::s_instance.store(nullptr, AZStd::memory_order_release);
    }

    Runtime* GetRuntime()
    {
        return static_cast<Runtime*>(RuntimeConfiguration::Get());
    }

    System::System(
        SystemConfiguration configuration,
        AZ::JobContext* jobContext)
        : m_runtime(AZStd::make_unique<Runtime>(
              AZStd::move(configuration),
              jobContext,
              SystemRegistration::Global))
    {
        if (!static_cast<bool>(*m_runtime))
        {
            m_runtime.reset();
        }
    }

    System::~System() = default;

    System::operator bool() const noexcept
    {
        return m_runtime && static_cast<bool>(*m_runtime);
    }

    const SystemConfiguration& RuntimeImplementation::GetConfiguration() const
    {
        return m_configuration;
    }

    RuntimeInfo RuntimeImplementation::GetRuntimeInfo() const
    {
        return m_nativeRuntime.GetRuntimeInfo();
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        IBodyPairCollider* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            extension->GetStateTypeId(),
            extension->GetStateVersion(),
            ExtensionKind::BodyPairCollider,
            false,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        IContactCallbacks* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            extension->GetStateTypeId(),
            extension->GetStateVersion(),
            ExtensionKind::ContactCallbacks,
            false,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        ICustomConstraintProvider* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        const AZ::TypeId id = extension->GetId();
        const AZ::u64 version = extension->GetVersion();
        if (id.IsNull() || version == 0)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            id,
            version,
            ExtensionKind::CustomConstraintProvider,
            true,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        ICustomConvexShapeProvider* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        const AZ::TypeId id = extension->GetId();
        const AZ::u64 version = extension->GetVersion();
        if (id.IsNull() || version == 0)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            id,
            version,
            ExtensionKind::CustomConvexShapeProvider,
            true,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        ICustomPathProvider* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        const AZ::TypeId id = extension->GetId();
        const AZ::u64 version = extension->GetVersion();
        if (id.IsNull() || version == 0)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            id,
            version,
            ExtensionKind::CustomPathProvider,
            true,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        ICustomShapeProvider* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        const AZ::TypeId id = extension->GetId();
        const AZ::u64 version = extension->GetVersion();
        if (id.IsNull() || version == 0)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            id,
            version,
            ExtensionKind::CustomShapeProvider,
            true,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        IGroupFilter* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            extension->GetStateTypeId(),
            extension->GetStateVersion(),
            ExtensionKind::GroupFilter,
            false,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        ISimulationShapeFilter* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            extension->GetStateTypeId(),
            extension->GetStateVersion(),
            ExtensionKind::SimulationShapeFilter,
            false,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        ISoftBodyContactCallbacks* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            extension->GetStateTypeId(),
            extension->GetStateVersion(),
            ExtensionKind::SoftBodyContactCallbacks,
            false,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        IStepListener* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            extension->GetStateTypeId(),
            extension->GetStateVersion(),
            ExtensionKind::StepListener,
            false,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        IVehicleCallbacks* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            extension->GetStateTypeId(),
            extension->GetStateVersion(),
            ExtensionKind::VehicleCallbacks,
            false,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        IVehicleCollisionFilter* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            extension->GetStateTypeId(),
            extension->GetStateVersion(),
            ExtensionKind::VehicleCollisionFilter,
            false,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtension(
        IVirtualCharacterContactCallbacks* extension,
        ExtensionHostLease hostLease)
    {
        if (!extension)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }
        return RegisterExtensionEntry(
            extension,
            extension->GetStateTypeId(),
            extension->GetStateVersion(),
            ExtensionKind::VirtualCharacterContactCallbacks,
            false,
            AZStd::move(hostLease));
    }

    ExtensionRegistrationResult RuntimeImplementation::RegisterExtensionEntry(
        void* extension,
        const AZ::TypeId id,
        const AZ::u64 version,
        const ExtensionKind kind,
        const bool uniqueIdentity,
        ExtensionHostLease hostLease)
    {
        if (!extension || id.IsNull() || kind == ExtensionKind::None)
        {
            return {.m_status = ExtensionRegistrationStatus::Invalid};
        }

        AZStd::lock_guard lock(m_extensionMutex);
        for (AZ::u32 extensionIndex = 0; extensionIndex < m_extensionSlots.size(); ++extensionIndex)
        {
            const ExtensionSlot& slot = m_extensionSlots[extensionIndex];
            if (slot.m_extension
                && slot.m_kind == kind
                && (slot.m_extension == extension || (uniqueIdentity && slot.m_id == id)))
            {
                return {
                    .m_handle = Internal::MakeResourceHandle<ExtensionHandle>(extensionIndex, slot.m_generation),
                    .m_status = ExtensionRegistrationStatus::AlreadyRegistered,
                };
            }
        }

        AZ::u32 extensionIndex = 0;
        if (!m_freeExtensionSlots.empty())
        {
            extensionIndex = m_freeExtensionSlots.back();
            m_freeExtensionSlots.pop_back();
        }
        else
        {
            if (m_extensionSlots.size() >= Internal::HandlePayloadMask)
            {
                return {.m_status = ExtensionRegistrationStatus::CapacityExhausted};
            }
            extensionIndex = aznumeric_cast<AZ::u32>(m_extensionSlots.size());
            m_extensionSlots.emplace_back();
        }

        ExtensionSlot& slot = m_extensionSlots[extensionIndex];
        slot.m_hostLease = AZStd::move(hostLease);
        slot.m_extension = extension;
        slot.m_id = id;
        slot.m_version = version;
        slot.m_dependentCount = 0;
        slot.m_kind = kind;
        return {
            .m_handle = Internal::MakeResourceHandle<ExtensionHandle>(extensionIndex, slot.m_generation),
            .m_status = ExtensionRegistrationStatus::Success,
        };
    }

    ExtensionRegistrationStatus RuntimeImplementation::UnregisterExtension(
        const ExtensionHandle extensionHandle)
    {
        ExtensionHostLease releasedHost;
        {
            AZStd::lock_guard lock(m_extensionMutex);
            Internal::ResourceHandleParts parts;
            if (!Internal::DecodeResourceHandle(extensionHandle, parts)
                || parts.m_index >= m_extensionSlots.size())
            {
                return ExtensionRegistrationStatus::NotRegistered;
            }

            ExtensionSlot& slot = m_extensionSlots[parts.m_index];
            if (!slot.m_extension || slot.m_generation != parts.m_generation)
            {
                return ExtensionRegistrationStatus::NotRegistered;
            }
            if (slot.m_dependentCount > 0)
            {
                return ExtensionRegistrationStatus::InUse;
            }

            releasedHost = AZStd::move(slot.m_hostLease);
            slot.m_extension = nullptr;
            slot.m_id = AZ::TypeId::CreateNull();
            slot.m_version = 0;
            slot.m_kind = ExtensionKind::None;
            if (Internal::AdvanceGeneration(slot.m_generation))
            {
                m_freeExtensionSlots.push_back(parts.m_index);
            }
        }
        return ExtensionRegistrationStatus::Success;
    }

    bool RuntimeImplementation::GetExtensionInformation(
        const ExtensionHandle extensionHandle,
        ExtensionInformation& information) const
    {
        AZStd::lock_guard lock(m_extensionMutex);
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(extensionHandle, parts)
            || parts.m_index >= m_extensionSlots.size())
        {
            return false;
        }

        const ExtensionSlot& slot = m_extensionSlots[parts.m_index];
        if (!slot.m_extension || slot.m_generation != parts.m_generation)
        {
            return false;
        }

        information = {
            .m_id = slot.m_id,
            .m_version = slot.m_version,
            .m_dependentCount = slot.m_dependentCount,
            .m_kind = slot.m_kind,
            .m_hasHostLease = static_cast<bool>(slot.m_hostLease),
        };
        return true;
    }

    void* RuntimeImplementation::AcquireExtension(
        const ExtensionHandle extensionHandle,
        const ExtensionKind kind)
    {
        AZStd::lock_guard lock(m_extensionMutex);
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(extensionHandle, parts)
            || parts.m_index >= m_extensionSlots.size())
        {
            return nullptr;
        }

        ExtensionSlot& slot = m_extensionSlots[parts.m_index];
        if (!slot.m_extension
            || slot.m_generation != parts.m_generation
            || slot.m_kind != kind
            || slot.m_dependentCount == AZStd::numeric_limits<AZ::u32>::max())
        {
            return nullptr;
        }

        ++slot.m_dependentCount;
        return slot.m_extension;
    }

    bool RuntimeImplementation::RetainExtension(
        const ExtensionHandle extensionHandle,
        const ExtensionKind kind)
    {
        if (AcquireExtension(extensionHandle, kind))
        {
            return true;
        }
        return false;
    }

    void* RuntimeImplementation::AcquireExtension(
        const ExtensionKind kind,
        const AZ::TypeId id,
        const AZ::u64 requiredVersion,
        ExtensionHandle& extensionHandle,
        AZ::u64* registeredVersion)
    {
        AZStd::lock_guard lock(m_extensionMutex);
        for (AZ::u32 extensionIndex = 0; extensionIndex < m_extensionSlots.size(); ++extensionIndex)
        {
            ExtensionSlot& slot = m_extensionSlots[extensionIndex];
            if (slot.m_extension
                && slot.m_kind == kind
                && slot.m_id == id
                && (requiredVersion == 0 || slot.m_version == requiredVersion)
                && slot.m_dependentCount != AZStd::numeric_limits<AZ::u32>::max())
            {
                ++slot.m_dependentCount;
                extensionHandle = Internal::MakeResourceHandle<ExtensionHandle>(extensionIndex, slot.m_generation);
                if (registeredVersion)
                {
                    *registeredVersion = slot.m_version;
                }
                return slot.m_extension;
            }
        }

        extensionHandle = ExtensionHandle::Invalid;
        if (registeredVersion)
        {
            *registeredVersion = 0;
        }
        return nullptr;
    }

    void RuntimeImplementation::ReleaseExtension(
        const ExtensionHandle extensionHandle)
    {
        AZStd::lock_guard lock(m_extensionMutex);
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(extensionHandle, parts)
            || parts.m_index >= m_extensionSlots.size())
        {
            AZ_Assert(false, "Extension ownership is inconsistent.");
            return;
        }

        ExtensionSlot& slot = m_extensionSlots[parts.m_index];
        AZ_Assert(
            slot.m_extension
                && slot.m_generation == parts.m_generation
                && slot.m_dependentCount > 0,
            "Extension ownership is inconsistent.");
        if (slot.m_extension
            && slot.m_generation == parts.m_generation
            && slot.m_dependentCount > 0)
        {
            --slot.m_dependentCount;
        }
    }

    ICustomShapeProvider* RuntimeImplementation::AcquireCustomShapeProvider(
        const AZ::TypeId providerId,
        const AZ::u64 requiredVersion,
        ExtensionHandle& extensionHandle,
        AZ::u64* registeredVersion)
    {
        return static_cast<ICustomShapeProvider*>(AcquireExtension(
            ExtensionKind::CustomShapeProvider,
            providerId,
            requiredVersion,
            extensionHandle,
            registeredVersion));
    }

    void RuntimeImplementation::ReleaseCustomShapeProvider(
        const ExtensionHandle extensionHandle)
    {
        ReleaseExtension(extensionHandle);
    }

    ICustomConstraintProvider* RuntimeImplementation::AcquireCustomConstraintProvider(
        const AZ::TypeId providerId,
        const AZStd::span<const AZ::u8> data,
        AZ::u32& maximumRowCount,
        AZ::u32& stateByteCount,
        AZ::u64& providerVersion,
        ExtensionHandle& extensionHandle)
    {
        auto* provider = static_cast<ICustomConstraintProvider*>(AcquireExtension(
            ExtensionKind::CustomConstraintProvider,
            providerId,
            0,
            extensionHandle,
            &providerVersion));
        if (!provider)
        {
            return nullptr;
        }

        maximumRowCount = provider->GetMaximumRowCount(data);
        if (maximumRowCount == 0)
        {
            ReleaseExtension(extensionHandle);
            extensionHandle = ExtensionHandle::Invalid;
            return nullptr;
        }

        stateByteCount = provider->GetStateByteCount(data);
        return provider;
    }

    void RuntimeImplementation::ReleaseCustomConstraintProvider(
        const ExtensionHandle extensionHandle)
    {
        ReleaseExtension(extensionHandle);
    }

    ICustomPathProvider* RuntimeImplementation::AcquireCustomPathProvider(
        const AZ::TypeId providerId,
        const AZStd::span<const AZ::u8> data,
        float& maximumFraction,
        AZ::u64& providerVersion,
        ExtensionHandle& extensionHandle)
    {
        auto* provider = static_cast<ICustomPathProvider*>(AcquireExtension(
            ExtensionKind::CustomPathProvider,
            providerId,
            0,
            extensionHandle,
            &providerVersion));
        if (!provider)
        {
            return nullptr;
        }

        maximumFraction = provider->GetMaximumFraction(data);
        if (!AZ::IsFiniteFloat(maximumFraction) || maximumFraction <= 0.0f)
        {
            ReleaseExtension(extensionHandle);
            extensionHandle = ExtensionHandle::Invalid;
            return nullptr;
        }

        CustomPathPoint startPoint;
        CustomPathPoint endPoint;
        if (!provider->Sample(data, 0.0f, startPoint)
            || !IsValidCustomPathPoint(startPoint)
            || !provider->Sample(data, maximumFraction, endPoint)
            || !IsValidCustomPathPoint(endPoint))
        {
            ReleaseExtension(extensionHandle);
            extensionHandle = ExtensionHandle::Invalid;
            return nullptr;
        }

        return provider;
    }

    void RuntimeImplementation::ReleaseCustomPathProvider(
        const ExtensionHandle extensionHandle)
    {
        ReleaseExtension(extensionHandle);
    }

    MaterialHandle RuntimeImplementation::CreateMaterial(
        const MaterialConfiguration& configuration)
    {
        if (!configuration.m_debugColor.IsFinite())
        {
            return {};
        }

        AZStd::lock_guard lock(m_materialMutex);
        AZ::u32 materialIndex = 0;
        if (!m_freeMaterialSlots.empty())
        {
            materialIndex = m_freeMaterialSlots.back();
            m_freeMaterialSlots.pop_back();
        }
        else
        {
            if (m_materialSlots.size() >= Internal::HandlePayloadMask)
            {
                return {};
            }
            materialIndex = static_cast<AZ::u32>(m_materialSlots.size());
            m_materialSlots.emplace_back();
        }

        MaterialSlot& slot = m_materialSlots[materialIndex];
        const MaterialHandle materialHandle =
            Internal::MakeResourceHandle<MaterialHandle>(materialIndex, slot.m_generation);
        slot.m_material = new NativeMaterial(materialHandle, configuration);
        return materialHandle;
    }

    bool RuntimeImplementation::DestroyMaterial(
        const MaterialHandle materialHandle)
    {
        AZStd::lock_guard lock(m_materialMutex);
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(materialHandle, parts)
            || parts.m_index >= m_materialSlots.size())
        {
            return false;
        }

        MaterialSlot& slot = m_materialSlots[parts.m_index];
        if (!slot.m_material || slot.m_generation != parts.m_generation || slot.m_referenceCount > 0)
        {
            return false;
        }

        slot.m_material = nullptr;
        if (Internal::AdvanceGeneration(slot.m_generation))
        {
            m_freeMaterialSlots.push_back(parts.m_index);
        }
        return true;
    }

    bool RuntimeImplementation::IsValid(
        const MaterialHandle materialHandle) const
    {
        AZStd::shared_lock lock(m_materialMutex);
        return FindMaterialUnlocked(materialHandle);
    }

    CookedShapeHandle RuntimeImplementation::CookShape(
        const ShapeConfiguration& configuration)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::CookShape");
        const DeterministicFloatScope floatScope;
        if (!AZ::IsFiniteFloat(configuration.m_density) || configuration.m_density <= 0.0f)
        {
            return {};
        }

        const bool isHeightfield = AZStd::holds_alternative<HeightfieldShapeConfiguration>(configuration.m_geometry);
        const bool isMesh = AZStd::holds_alternative<MeshShapeConfiguration>(configuration.m_geometry);
        const bool isEmpty = AZStd::holds_alternative<EmptyShapeConfiguration>(configuration.m_geometry);
        const bool isCustom = AZStd::holds_alternative<CustomShapeConfiguration>(configuration.m_geometry);
        if ((isEmpty && !configuration.m_materials.empty())
            || (!isHeightfield && !isMesh && !isCustom && configuration.m_materials.size() > 1))
        {
            return {};
        }

        JPH::PhysicsMaterialList materials;
        if (!AcquireMaterials(configuration.m_materials, materials))
        {
            return {};
        }

        NativeShapeResult nativeResult;
        AZ::TypeId customProviderId = AZ::TypeId::CreateNull();
        ExtensionHandle customProviderExtension;
        AZ::u64 customProviderVersion = 0;
        AZStd::vector<CustomShapeDependency> customDependencies;
        if (const auto* customConfiguration = AZStd::get_if<CustomShapeConfiguration>(
            &configuration.m_geometry))
        {
            ICustomShapeProvider* provider = AcquireCustomShapeProvider(
                customConfiguration->m_providerId,
                0,
                customProviderExtension,
                &customProviderVersion);
            if (!provider)
            {
                nativeResult.m_error = "No compatible custom-shape provider is registered for this identifier.";
            }
            else
            {
                CustomShapeData data;
                if (!provider->Cook(customConfiguration->m_data, data))
                {
                    nativeResult.m_error = "The custom-shape provider rejected its input data.";
                    ReleaseCustomShapeProvider(customProviderExtension);
                    customProviderExtension = ExtensionHandle::Invalid;
                }
                else if (data.m_runtimeData.size() > MaximumCustomShapeRuntimeDataSize)
                {
                    nativeResult.m_error = "Custom-shape runtime data exceeds the supported archive limit.";
                    ReleaseCustomShapeProvider(customProviderExtension);
                    customProviderExtension = ExtensionHandle::Invalid;
                }
                else if (data.m_geometryKind == CustomShapeGeometryKind::Convex
                    && configuration.m_materials.size() > 1)
                {
                    nativeResult.m_error = "A convex custom shape accepts at most one material.";
                    ReleaseCustomShapeProvider(customProviderExtension);
                    customProviderExtension = ExtensionHandle::Invalid;
                }
                else if (AZStd::any_of(
                    data.m_dependencies.begin(),
                    data.m_dependencies.end(),
                    [](const CustomShapeDependency& dependency)
                    {
                        return dependency.m_path.empty() || dependency.m_contentHash == 0;
                    }))
                {
                    nativeResult.m_error = "Custom-shape dependencies require a path and content hash.";
                    ReleaseCustomShapeProvider(customProviderExtension);
                    customProviderExtension = ExtensionHandle::Invalid;
                }
                else
                {
                    AZ::HashValue64 sourceHash = AZ::TypeHash64(
                        customProviderVersion,
                        AZ::HashValue64(static_cast<AZ::u64>(customConfiguration->m_providerId.GetHash())));
                    if (!customConfiguration->m_data.empty())
                    {
                        sourceHash = AZ::TypeHash64(
                            customConfiguration->m_data.data(),
                            customConfiguration->m_data.size(),
                            sourceHash);
                    }
                    for (const AZ::Vector3& vertex : data.m_vertices)
                    {
                        const AZStd::array components = {
                            vertex.GetX(),
                            vertex.GetY(),
                            vertex.GetZ(),
                        };
                        sourceHash = AZ::TypeHash64(components, sourceHash);
                    }
                    for (const CustomShapeTriangle& triangle : data.m_triangles)
                    {
                        const AZStd::array values = {
                            triangle.m_firstVertex,
                            triangle.m_secondVertex,
                            triangle.m_thirdVertex,
                            triangle.m_materialIndex,
                            triangle.m_userData,
                        };
                        sourceHash = AZ::TypeHash64(values, sourceHash);
                    }
                    for (const CustomShapeDependency& dependency : data.m_dependencies)
                    {
                        sourceHash = AZ::TypeHash64(
                            reinterpret_cast<const AZ::u8*>(dependency.m_path.data()),
                            dependency.m_path.size(),
                            sourceHash);
                        sourceHash = AZ::TypeHash64(dependency.m_contentHash, sourceHash);
                    }
                    if (!data.m_runtimeData.empty())
                    {
                        sourceHash = AZ::TypeHash64(
                            data.m_runtimeData.data(),
                            data.m_runtimeData.size(),
                            sourceHash);
                    }
                    sourceHash = AZ::TypeHash64(data.m_activeEdgeCosineThreshold, sourceHash);
                    sourceHash = AZ::TypeHash64(data.m_hullTolerance, sourceHash);
                    sourceHash = AZ::TypeHash64(data.m_maximumConvexRadius, sourceHash);
                    sourceHash = AZ::TypeHash64(data.m_maximumConvexRadiusError, sourceHash);
                    sourceHash = AZ::TypeHash64(data.m_maximumTrianglesPerLeaf, sourceHash);
                    sourceHash = AZ::TypeHash64(data.m_geometryKind, sourceHash);
                    sourceHash = AZ::TypeHash64(data.m_perTriangleUserData, sourceHash);

                    nativeResult = CreateNativeCustomShape(
                        data,
                        {
                            .m_providerId = customConfiguration->m_providerId,
                            .m_providerVersion = customProviderVersion,
                            .m_sourceHash = static_cast<AZ::u64>(sourceHash),
                        },
                        *provider,
                        materials,
                        configuration.m_density,
                        configuration.m_userData);
                    if (nativeResult)
                    {
                        customProviderId = customConfiguration->m_providerId;
                        customDependencies = AZStd::move(data.m_dependencies);
                    }
                    else
                    {
                        ReleaseCustomShapeProvider(customProviderExtension);
                        customProviderExtension = ExtensionHandle::Invalid;
                    }
                }
            }
        }
        else if (const auto* customConvexConfiguration = AZStd::get_if<CustomConvexShapeConfiguration>(
            &configuration.m_geometry))
        {
            ExtensionHandle providerExtension;
            AZ::u64 providerVersion = 0;
            auto* provider = static_cast<ICustomConvexShapeProvider*>(AcquireExtension(
                ExtensionKind::CustomConvexShapeProvider,
                customConvexConfiguration->m_providerId,
                0,
                providerExtension,
                &providerVersion));
            if (!provider)
            {
                nativeResult.m_error = "No custom convex-shape provider is registered for this identifier.";
            }
            else
            {
                CustomConvexShapeData data;
                if (!provider->Cook(customConvexConfiguration->m_data, data))
                {
                    nativeResult.m_error = "The custom convex-shape provider rejected its input data.";
                }
                else
                {
                    AZ::HashValue64 sourceHash = AZ::TypeHash64(
                        providerVersion,
                        AZ::HashValue64(static_cast<AZ::u64>(customConvexConfiguration->m_providerId.GetHash())));
                    if (!customConvexConfiguration->m_data.empty())
                    {
                        sourceHash = AZ::TypeHash64(
                            customConvexConfiguration->m_data.data(),
                            customConvexConfiguration->m_data.size(),
                            sourceHash);
                    }
                    for (const AZ::Vector3& point : data.m_points)
                    {
                        const AZStd::array components = {
                            point.GetX(),
                            point.GetY(),
                            point.GetZ(),
                        };
                        sourceHash = AZ::TypeHash64(components, sourceHash);
                    }
                    sourceHash = AZ::TypeHash64(data.m_hullTolerance, sourceHash);
                    sourceHash = AZ::TypeHash64(data.m_maximumConvexRadius, sourceHash);
                    sourceHash = AZ::TypeHash64(data.m_maximumConvexRadiusError, sourceHash);

                    const JPH::PhysicsMaterial* material = nullptr;
                    if (!materials.empty())
                    {
                        material = materials.front();
                    }
                    nativeResult = CreateNativeCustomConvexShape(
                        data,
                        {
                            .m_providerId = customConvexConfiguration->m_providerId,
                            .m_providerVersion = providerVersion,
                            .m_sourceHash = static_cast<AZ::u64>(sourceHash),
                        },
                        material,
                        configuration.m_density);
                }
                ReleaseExtension(providerExtension);
            }
        }
        else
        {
            nativeResult = CreateNativeShape(configuration, materials);
        }
        if (!nativeResult)
        {
            AZ_Error("Jolt", false, "Failed to cook shape: %s", nativeResult.m_error.c_str());
            ReleaseMaterials(configuration.m_materials);
            return {};
        }
        const_cast<JPH::Shape*>(nativeResult.m_shape.GetPtr())->SetUserData(configuration.m_userData);

        const CookedShapeHandle cookedShapeHandle = StoreCookedShape(
            nativeResult.m_shape,
            configuration.m_materials,
            {},
            customProviderId,
            customProviderExtension,
            AZStd::move(customDependencies));
        if (!cookedShapeHandle)
        {
            ReleaseMaterials(configuration.m_materials);
            if (customProviderExtension)
            {
                ReleaseCustomShapeProvider(customProviderExtension);
            }
        }
        return cookedShapeHandle;
    }

    CookedShapeHandle RuntimeImplementation::CookShape(
        const CookedCompoundShapeConfiguration& configuration)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::CookCompoundShape");
        const DeterministicFloatScope floatScope;
        if (configuration.m_children.empty())
        {
            return {};
        }

        JPH::StaticCompoundShapeSettings staticSettings;
        AZStd::vector<CookedShapeHandle> childHandles;
        childHandles.reserve(configuration.m_children.size());
        for (const CookedCompoundChildConfiguration& child : configuration.m_children)
        {
            const float rotationLengthSq = child.m_rotation.GetLengthSq();
            if (!child.m_position.IsFinite()
                || !child.m_rotation.IsFinite()
                || !AZ::IsFiniteFloat(rotationLengthSq)
                || rotationLengthSq <= 0.0f)
            {
                for (const CookedShapeHandle childHandle : childHandles)
                {
                    ReleaseCookedShape(childHandle);
                }
                return {};
            }

            JPH::RefConst<JPH::Shape> childShape;
            if (!AcquireCookedShape(child.m_shapeHandle, childShape))
            {
                for (const CookedShapeHandle childHandle : childHandles)
                {
                    ReleaseCookedShape(childHandle);
                }
                return {};
            }
            childHandles.push_back(child.m_shapeHandle);

            staticSettings.AddShape(
                ToNativeSystemVector(child.m_position),
                ToNativeSystemRotation(child.m_rotation),
                childShape,
                child.m_userData);
        }

        JPH::Shape::ShapeResult nativeResult = staticSettings.Create();

        CookedShapeHandle cookedShapeHandle;
        if (!nativeResult.HasError())
        {
            nativeResult.Get()->SetUserData(configuration.m_userData);
            cookedShapeHandle = StoreCookedShape(nativeResult.Get(), {}, childHandles);
        }
        else
        {
            AZ_Error("Jolt", false, "Failed to cook compound shape: %s", nativeResult.GetError().c_str());
        }

        for (const CookedShapeHandle childHandle : childHandles)
        {
            ReleaseCookedShape(childHandle);
        }
        return cookedShapeHandle;
    }

    CookedShapeHandle RuntimeImplementation::CookShape(
        const CookedDecoratedShapeConfiguration& configuration)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::CookDecoratedShape");
        const DeterministicFloatScope floatScope;
        JPH::Shape::ShapeResult nativeResult;
        CookedShapeHandle childHandle;
        bool childAcquired = false;
        AZStd::visit(
            [&](const auto& geometry)
            {
                using Geometry = AZStd::remove_cvref_t<decltype(geometry)>;
                childHandle = geometry.m_shapeHandle;
                JPH::RefConst<JPH::Shape> childShape;
                if (!AcquireCookedShape(childHandle, childShape))
                {
                    nativeResult.SetError("Cooked decorated shape child handle is invalid.");
                    return;
                }
                childAcquired = true;

                if constexpr (AZStd::is_same_v<Geometry, CookedOffsetCenterOfMassShapeConfiguration>)
                {
                    if (!geometry.m_offset.IsFinite())
                    {
                        nativeResult.SetError("Center of mass offset must be finite.");
                        return;
                    }

                    JPH::OffsetCenterOfMassShapeSettings settings(
                        ToNativeSystemVector(geometry.m_offset),
                        childShape);
                    nativeResult = settings.Create();
                }
                else if constexpr (AZStd::is_same_v<Geometry, CookedRotatedTranslatedShapeConfiguration>)
                {
                    const float rotationLengthSq = geometry.m_rotation.GetLengthSq();
                    if (!geometry.m_position.IsFinite()
                        || !geometry.m_rotation.IsFinite()
                        || !AZ::IsFiniteFloat(rotationLengthSq)
                        || rotationLengthSq <= 0.0f)
                    {
                        nativeResult.SetError("Decorated shape transform must be finite.");
                        return;
                    }

                    JPH::RotatedTranslatedShapeSettings settings(
                        ToNativeSystemVector(geometry.m_position),
                        ToNativeSystemRotation(geometry.m_rotation),
                        childShape);
                    nativeResult = settings.Create();
                }
                else if constexpr (AZStd::is_same_v<Geometry, CookedScaledShapeConfiguration>)
                {
                    if (!geometry.m_scale.IsFinite()
                        || geometry.m_scale.GetX() == 0.0f
                        || geometry.m_scale.GetY() == 0.0f
                        || geometry.m_scale.GetZ() == 0.0f)
                    {
                        nativeResult.SetError("Decorated shape scale must be finite and nonzero.");
                        return;
                    }

                    JPH::ScaledShapeSettings settings(
                        childShape,
                        ToNativeSystemVector(geometry.m_scale));
                    nativeResult = settings.Create();
                }
            },
            configuration.m_geometry);

        CookedShapeHandle cookedShapeHandle;
        if (!nativeResult.HasError())
        {
            nativeResult.Get()->SetUserData(configuration.m_userData);
            cookedShapeHandle = StoreCookedShape(nativeResult.Get(), {}, {childHandle});
        }
        else
        {
            AZ_Error("Jolt", false, "Failed to cook decorated shape: %s", nativeResult.GetError().c_str());
        }

        if (childAcquired)
        {
            ReleaseCookedShape(childHandle);
        }
        return cookedShapeHandle;
    }

    Operation<CookedShapeHandle> RuntimeImplementation::CookShapeAsync(
        const ShapeConfiguration& configuration)
    {
        if (!m_operationPool)
        {
            return {};
        }

        struct Work final
        {
            RuntimeImplementation* m_runtime = nullptr;
            ShapeConfiguration m_configuration;
        };

        return m_operationPool->CreateOperation<CookedShapeHandle>(
            Work{
                .m_runtime = this,
                .m_configuration = configuration,
            },
            [](Work& work, CookedShapeHandle& result)
            {
                result = work.m_runtime->CookShape(work.m_configuration);
                return static_cast<bool>(result);
            });
    }

    Operation<CookedShapeHandle> RuntimeImplementation::CookShapeAsync(
        const CookedCompoundShapeConfiguration& configuration)
    {
        if (!m_operationPool)
        {
            return {};
        }

        struct Work final
        {
            RuntimeImplementation* m_runtime = nullptr;
            CookedCompoundShapeConfiguration m_configuration;
        };

        return m_operationPool->CreateOperation<CookedShapeHandle>(
            Work{
                .m_runtime = this,
                .m_configuration = configuration,
            },
            [](Work& work, CookedShapeHandle& result)
            {
                result = work.m_runtime->CookShape(work.m_configuration);
                return static_cast<bool>(result);
            });
    }

    Operation<CookedShapeHandle> RuntimeImplementation::CookShapeAsync(
        const CookedDecoratedShapeConfiguration& configuration)
    {
        if (!m_operationPool)
        {
            return {};
        }

        struct Work final
        {
            RuntimeImplementation* m_runtime = nullptr;
            CookedDecoratedShapeConfiguration m_configuration;
        };

        return m_operationPool->CreateOperation<CookedShapeHandle>(
            Work{
                .m_runtime = this,
                .m_configuration = configuration,
            },
            [](Work& work, CookedShapeHandle& result)
            {
                result = work.m_runtime->CookShape(work.m_configuration);
                return static_cast<bool>(result);
            });
    }

    Operation<SceneInstanceHandle> RuntimeImplementation::InstantiateSceneAsync(
        const WorldHandle worldHandle,
        const SceneDefinitionHandle definitionHandle)
    {
        if (!m_operationPool)
        {
            return {};
        }

        struct Work final
        {
            RuntimeImplementation* m_runtime = nullptr;
            WorldHandle m_worldHandle;
            SceneDefinitionHandle m_definitionHandle;
        };

        return m_operationPool->CreateOperation<SceneInstanceHandle>(
            Work{
                .m_runtime = this,
                .m_worldHandle = worldHandle,
                .m_definitionHandle = definitionHandle,
            },
            [](Work& work, SceneInstanceHandle& result)
            {
                result = work.m_runtime->InstantiateScene(work.m_worldHandle, work.m_definitionHandle);
                return static_cast<bool>(result);
            });
    }

    bool RuntimeImplementation::ExportSkeletonDefinition(
        const SkeletonDefinitionHandle skeletonHandle,
        SkeletonDefinitionArchive& archive) const
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::ExportSkeletonDefinition");
        const DeterministicFloatScope floatScope;

        JPH::Ref<JPH::Skeleton> skeleton;
        AZ::u32 jointCount = 0;
        {
            AZStd::shared_lock lock(m_skeletonMutex);
            const SkeletonDefinitionSlot* slot = FindSkeletonDefinitionUnlocked(skeletonHandle);
            if (!slot)
            {
                return false;
            }

            skeleton = slot->m_skeleton;
            jointCount = aznumeric_cast<AZ::u32>(slot->m_joints.size());
        }

        NativeArchiveWriter writer;
        skeleton->SaveBinaryState(writer);
        if (writer.IsFailed())
        {
            return false;
        }

        SkeletonDefinitionArchive exportedArchive;
        exportedArchive.m_binaryState = writer.TakeData();
        if (exportedArchive.m_binaryState.empty())
        {
            return false;
        }

        exportedArchive.m_buildFingerprint = GetNativeBuildFingerprint();
        exportedArchive.m_contentHash = static_cast<AZ::u64>(AZ::TypeHash64(
            exportedArchive.m_binaryState.data(),
            exportedArchive.m_binaryState.size()));
        exportedArchive.m_formatVersion = SkeletonDefinitionArchiveFormatVersion;
        exportedArchive.m_jointCount = jointCount;
        archive = AZStd::move(exportedArchive);
        return true;
    }

    SkeletonDefinitionHandle RuntimeImplementation::ImportSkeletonDefinition(
        const SkeletonDefinitionArchive& archive)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::ImportSkeletonDefinition");
        const DeterministicFloatScope floatScope;
        if (archive.m_formatVersion != SkeletonDefinitionArchiveFormatVersion
            || archive.m_buildFingerprint != GetNativeBuildFingerprint()
            || archive.m_binaryState.empty()
            || archive.m_binaryState.size() > MaximumNativeArchiveSize
            || archive.m_jointCount == 0
            || archive.m_jointCount >= Internal::HandlePayloadMask
            || archive.m_contentHash != static_cast<AZ::u64>(AZ::TypeHash64(
                archive.m_binaryState.data(),
                archive.m_binaryState.size())))
        {
            return {};
        }

        NativeArchiveReader reader(archive.m_binaryState);
        JPH::Skeleton::SkeletonResult result = JPH::Skeleton::sRestoreFromBinaryState(reader);
        if (result.HasError()
            || reader.IsFailed()
            || !reader.IsFullyConsumed()
            || !result.Get()->AreJointsCorrectlyOrdered()
            || aznumeric_cast<AZ::u32>(result.Get()->GetJointCount()) != archive.m_jointCount)
        {
            return {};
        }

        AZStd::vector<SkeletonJoint> joints;
        joints.reserve(archive.m_jointCount);
        for (const JPH::Skeleton::Joint& nativeJoint : result.Get()->GetJoints())
        {
            AZ::Name name(nativeJoint.mName.c_str());
            if (name.IsEmpty()
                || nativeJoint.mParentJointIndex < -1
                || nativeJoint.mParentJointIndex >= aznumeric_cast<AZ::s32>(joints.size())
                || AZStd::find_if(
                    joints.begin(),
                    joints.end(),
                    [&name](const SkeletonJoint& joint)
                    {
                        return joint.m_name == name;
                    }) != joints.end())
            {
                return {};
            }

            joints.push_back({
                .m_name = AZStd::move(name),
                .m_parentIndex = nativeJoint.mParentJointIndex,
            });
        }

        return StoreSkeletonDefinition(
            result.Get(),
            AZStd::move(joints));
    }

    bool RuntimeImplementation::ExportSkeletalAnimation(
        const SkeletalAnimationHandle animationHandle,
        SkeletalAnimationArchive& archive) const
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::ExportSkeletalAnimation");
        const DeterministicFloatScope floatScope;

        JPH::Ref<JPH::SkeletalAnimation> animation;
        AZ::u32 jointCount = 0;
        {
            AZStd::shared_lock lock(m_skeletonMutex);
            const SkeletalAnimationSlot* slot = FindSkeletalAnimationUnlocked(animationHandle);
            if (!slot)
            {
                return false;
            }

            animation = slot->m_animation;
            jointCount = aznumeric_cast<AZ::u32>(slot->m_jointNames.size());
        }

        NativeArchiveWriter writer;
        animation->SaveBinaryState(writer);
        if (writer.IsFailed())
        {
            return false;
        }

        SkeletalAnimationArchive exportedArchive;
        exportedArchive.m_binaryState = writer.TakeData();
        if (exportedArchive.m_binaryState.empty())
        {
            return false;
        }

        exportedArchive.m_buildFingerprint = GetNativeBuildFingerprint();
        exportedArchive.m_contentHash = static_cast<AZ::u64>(AZ::TypeHash64(
            exportedArchive.m_binaryState.data(),
            exportedArchive.m_binaryState.size()));
        exportedArchive.m_formatVersion = SkeletalAnimationArchiveFormatVersion;
        exportedArchive.m_jointCount = jointCount;
        archive = AZStd::move(exportedArchive);
        return true;
    }

    SkeletalAnimationHandle RuntimeImplementation::ImportSkeletalAnimation(
        const SkeletalAnimationArchive& archive)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::ImportSkeletalAnimation");
        const DeterministicFloatScope floatScope;
        if (archive.m_formatVersion != SkeletalAnimationArchiveFormatVersion
            || archive.m_buildFingerprint != GetNativeBuildFingerprint()
            || archive.m_binaryState.empty()
            || archive.m_binaryState.size() > MaximumNativeArchiveSize
            || archive.m_jointCount >= Internal::HandlePayloadMask
            || archive.m_contentHash != static_cast<AZ::u64>(AZ::TypeHash64(
                archive.m_binaryState.data(),
                archive.m_binaryState.size())))
        {
            return {};
        }

        NativeArchiveReader reader(archive.m_binaryState);
        JPH::SkeletalAnimation::AnimationResult result =
            JPH::SkeletalAnimation::sRestoreFromBinaryState(reader);
        if (result.HasError()
            || reader.IsFailed()
            || !reader.IsFullyConsumed()
            || result.Get()->GetAnimatedJoints().size() != archive.m_jointCount)
        {
            return {};
        }

        AZStd::vector<AZ::Name> jointNames;
        jointNames.reserve(archive.m_jointCount);
        for (const JPH::SkeletalAnimation::AnimatedJoint& nativeJoint : result.Get()->GetAnimatedJoints())
        {
            AZ::Name name(nativeJoint.mJointName.c_str());
            if (name.IsEmpty()
                || nativeJoint.mKeyframes.empty()
                || AZStd::find(jointNames.begin(), jointNames.end(), name) != jointNames.end())
            {
                return {};
            }

            float previousTime = -1.0f;
            for (const JPH::SkeletalAnimation::Keyframe& keyframe : nativeJoint.mKeyframes)
            {
                if (!keyframe.mRotation.IsNormalized()
                    || !std::isfinite(keyframe.mTranslation.GetX())
                    || !std::isfinite(keyframe.mTranslation.GetY())
                    || !std::isfinite(keyframe.mTranslation.GetZ())
                    || !std::isfinite(keyframe.mTime)
                    || keyframe.mTime < 0.0f
                    || keyframe.mTime <= previousTime)
                {
                    return {};
                }
                previousTime = keyframe.mTime;
            }
            jointNames.push_back(AZStd::move(name));
        }

        return StoreSkeletalAnimation(
            result.Get(),
            AZStd::move(jointNames));
    }

    bool RuntimeImplementation::ExportShape(
        const CookedShapeHandle cookedShapeHandle,
        CookedShapeArchive& archive,
        AZStd::vector<MaterialHandle>& materialHandles,
        AZStd::vector<CookedShapeHandle>& childShapeHandles) const
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::ExportShape");
        const DeterministicFloatScope floatScope;

        JPH::RefConst<JPH::Shape> shape;
        AZStd::vector<CookedShapeHandle> storedChildHandles;
        AZStd::vector<CustomShapeDependency> storedDependencies;
        {
            AZStd::shared_lock lock(m_cookedShapeMutex);
            const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
            if (!slot)
            {
                return false;
            }

            shape = slot->m_shape;
            storedChildHandles = slot->m_childHandles;
            storedDependencies = slot->m_customDependencies;
        }

        AZ::TypeId providerId = AZ::TypeId::CreateNull();
        AZ::u64 providerVersion = 0;
        CustomShapeInfo customShapeInfo;
        if (GetNativeCustomShapeInfo(*shape, customShapeInfo))
        {
            providerId = customShapeInfo.m_providerId;
            providerVersion = customShapeInfo.m_providerVersion;
        }
        else
        {
            CustomConvexShapeInfo customConvexShapeInfo;
            if (GetNativeCustomConvexShapeInfo(*shape, customConvexShapeInfo))
            {
                providerId = customConvexShapeInfo.m_providerId;
                providerVersion = customConvexShapeInfo.m_providerVersion;
            }
        }

        NativeArchiveWriter writer;
        shape->SaveBinaryState(writer);
        if (writer.IsFailed())
        {
            return false;
        }

        JPH::PhysicsMaterialList nativeMaterials;
        shape->SaveMaterialState(nativeMaterials);
        if (nativeMaterials.size() > AZStd::numeric_limits<AZ::u32>::max())
        {
            return false;
        }

        AZStd::vector<MaterialHandle> exportedMaterialHandles;
        exportedMaterialHandles.reserve(nativeMaterials.size());
        for (const JPH::PhysicsMaterial* material : nativeMaterials)
        {
            if (!material)
            {
                exportedMaterialHandles.push_back(MaterialHandle::Invalid);
                continue;
            }

            const MaterialHandle materialHandle = FindMaterialHandle(material);
            if (!materialHandle)
            {
                return false;
            }
            exportedMaterialHandles.push_back(materialHandle);
        }

        JPH::ShapeList nativeChildShapes;
        shape->SaveSubShapeState(nativeChildShapes);
        if (nativeChildShapes.size() != storedChildHandles.size()
            || nativeChildShapes.size() > AZStd::numeric_limits<AZ::u32>::max())
        {
            return false;
        }

        {
            AZStd::shared_lock lock(m_cookedShapeMutex);
            for (size_t childIndex = 0; childIndex < storedChildHandles.size(); ++childIndex)
            {
                const CookedShapeSlot* childSlot = FindCookedShapeUnlocked(storedChildHandles[childIndex]);
                if (!childSlot || childSlot->m_shape.GetPtr() != nativeChildShapes[childIndex].GetPtr())
                {
                    return false;
                }
            }
        }

        CookedShapeArchive exportedArchive;
        exportedArchive.m_binaryState = writer.TakeData();
        exportedArchive.m_dependencies = AZStd::move(storedDependencies);
        exportedArchive.m_providerId = providerId;
        if (exportedArchive.m_binaryState.empty())
        {
            return false;
        }
        exportedArchive.m_buildFingerprint = GetNativeBuildFingerprint();
        exportedArchive.m_contentHash = CalculateCookedShapeArchiveHash(
            exportedArchive.m_binaryState,
            exportedArchive.m_dependencies,
            exportedArchive.m_providerId,
            providerVersion);
        exportedArchive.m_providerVersion = providerVersion;
        exportedArchive.m_formatVersion = CookedShapeArchiveFormatVersion;
        exportedArchive.m_materialCount = static_cast<AZ::u32>(exportedMaterialHandles.size());
        exportedArchive.m_childShapeCount = static_cast<AZ::u32>(storedChildHandles.size());

        archive = AZStd::move(exportedArchive);
        materialHandles = AZStd::move(exportedMaterialHandles);
        childShapeHandles = AZStd::move(storedChildHandles);
        return true;
    }

    CookedShapeHandle RuntimeImplementation::ImportShape(
        const CookedShapeArchive& archive,
        const AZStd::span<const MaterialHandle> materialHandles,
        const AZStd::span<const CookedShapeHandle> childShapeHandles)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::ImportShape");
        const DeterministicFloatScope floatScope;
        if (archive.m_formatVersion != CookedShapeArchiveFormatVersion
            || archive.m_buildFingerprint != GetNativeBuildFingerprint()
            || archive.m_binaryState.empty()
            || archive.m_binaryState.size() > MaximumNativeArchiveSize
            || archive.m_materialCount != materialHandles.size()
            || archive.m_childShapeCount != childShapeHandles.size()
            || AZStd::any_of(
                archive.m_dependencies.begin(),
                archive.m_dependencies.end(),
                [](const CustomShapeDependency& dependency)
                {
                    return dependency.m_path.empty() || dependency.m_contentHash == 0;
                })
            || archive.m_contentHash != CalculateCookedShapeArchiveHash(
                archive.m_binaryState,
                archive.m_dependencies,
                archive.m_providerId,
                archive.m_providerVersion))
        {
            return {};
        }

        NativeArchiveReader reader(archive.m_binaryState);
        JPH::Shape::ShapeResult nativeResult = JPH::Shape::sRestoreFromBinaryState(reader);
        if (nativeResult.HasError()
            || reader.IsFailed()
            || !reader.IsFullyConsumed()
            || !IsNativeCustomShapeValid(*nativeResult.Get())
            || !IsNativeCustomConvexShapeValid(*nativeResult.Get()))
        {
            return {};
        }

        AZ::TypeId providerId = AZ::TypeId::CreateNull();
        AZ::u64 providerVersion = 0;
        CustomShapeInfo restoredCustomShapeInfo;
        if (GetNativeCustomShapeInfo(*nativeResult.Get(), restoredCustomShapeInfo))
        {
            providerId = restoredCustomShapeInfo.m_providerId;
            providerVersion = restoredCustomShapeInfo.m_providerVersion;
        }
        else
        {
            CustomConvexShapeInfo restoredCustomConvexShapeInfo;
            if (GetNativeCustomConvexShapeInfo(*nativeResult.Get(), restoredCustomConvexShapeInfo))
            {
                providerId = restoredCustomConvexShapeInfo.m_providerId;
                providerVersion = restoredCustomConvexShapeInfo.m_providerVersion;
            }
        }
        if (archive.m_providerId != providerId
            || archive.m_providerVersion != providerVersion)
        {
            return {};
        }

        JPH::PhysicsMaterialList expectedMaterials;
        nativeResult.Get()->SaveMaterialState(expectedMaterials);
        JPH::ShapeList expectedChildShapes;
        nativeResult.Get()->SaveSubShapeState(expectedChildShapes);
        if (expectedMaterials.size() != materialHandles.size()
            || expectedChildShapes.size() != childShapeHandles.size())
        {
            return {};
        }

        AZStd::vector<MaterialHandle> retainedMaterialHandles;
        retainedMaterialHandles.reserve(materialHandles.size());
        for (const MaterialHandle materialHandle : materialHandles)
        {
            if (materialHandle)
            {
                retainedMaterialHandles.push_back(materialHandle);
            }
        }

        JPH::PhysicsMaterialList acquiredMaterials;
        if (!AcquireMaterials(retainedMaterialHandles, acquiredMaterials))
        {
            return {};
        }

        JPH::PhysicsMaterialList nativeMaterials;
        nativeMaterials.reserve(materialHandles.size());
        size_t acquiredMaterialIndex = 0;
        for (const MaterialHandle materialHandle : materialHandles)
        {
            if (materialHandle)
            {
                nativeMaterials.push_back(acquiredMaterials[acquiredMaterialIndex]);
                ++acquiredMaterialIndex;
            }
            else
            {
                nativeMaterials.emplace_back(nullptr);
            }
        }

        JPH::ShapeList nativeChildShapes;
        nativeChildShapes.reserve(childShapeHandles.size());
        size_t acquiredChildCount = 0;
        for (const CookedShapeHandle childShapeHandle : childShapeHandles)
        {
            JPH::RefConst<JPH::Shape> childShape;
            if (!AcquireCookedShape(childShapeHandle, childShape))
            {
                for (size_t childIndex = 0; childIndex < acquiredChildCount; ++childIndex)
                {
                    ReleaseCookedShape(childShapeHandles[childIndex]);
                }
                ReleaseMaterials(retainedMaterialHandles);
                return {};
            }
            nativeChildShapes.push_back(AZStd::move(childShape));
            ++acquiredChildCount;
        }

        nativeResult.Get()->RestoreMaterialState(
            nativeMaterials.data(),
            static_cast<JPH::uint>(nativeMaterials.size()));
        nativeResult.Get()->RestoreSubShapeState(
            nativeChildShapes.data(),
            static_cast<JPH::uint>(nativeChildShapes.size()));

        CustomShapeInfo customShapeInfo;
        AZ::TypeId customProviderId = AZ::TypeId::CreateNull();
        ExtensionHandle customProviderExtension;
        if (GetNativeCustomShapeInfo(*nativeResult.Get(), customShapeInfo))
        {
            ICustomShapeProvider* provider = AcquireCustomShapeProvider(
                customShapeInfo.m_providerId,
                customShapeInfo.m_providerVersion,
                customProviderExtension);
            if (!provider
                || !BindNativeCustomShapeProvider(*nativeResult.Get(), *provider))
            {
                if (provider)
                {
                    ReleaseCustomShapeProvider(customProviderExtension);
                    customProviderExtension = ExtensionHandle::Invalid;
                }
                for (const CookedShapeHandle childShapeHandle : childShapeHandles)
                {
                    ReleaseCookedShape(childShapeHandle);
                }
                ReleaseMaterials(retainedMaterialHandles);
                return {};
            }
            customProviderId = customShapeInfo.m_providerId;
        }

        const CookedShapeHandle cookedShapeHandle = StoreCookedShape(
            nativeResult.Get(),
            retainedMaterialHandles,
            {childShapeHandles.begin(), childShapeHandles.end()},
            customProviderId,
            customProviderExtension,
            archive.m_dependencies);
        for (const CookedShapeHandle childShapeHandle : childShapeHandles)
        {
            ReleaseCookedShape(childShapeHandle);
        }
        if (!cookedShapeHandle)
        {
            ReleaseMaterials(retainedMaterialHandles);
            if (customProviderExtension)
            {
                ReleaseCustomShapeProvider(customProviderExtension);
            }
        }
        return cookedShapeHandle;
    }

    bool RuntimeImplementation::DestroyCookedShape(
        const CookedShapeHandle cookedShapeHandle)
    {
        AZStd::vector<MaterialHandle> materialHandles;
        ExtensionHandle customProviderExtension;
        {
            AZStd::lock_guard lock(m_cookedShapeMutex);
            Internal::ResourceHandleParts parts;
            if (!Internal::DecodeResourceHandle(cookedShapeHandle, parts)
                || parts.m_index >= m_cookedShapeSlots.size())
            {
                return false;
            }

            CookedShapeSlot& slot = m_cookedShapeSlots[parts.m_index];
            if (!slot.m_shape
                || slot.m_generation != parts.m_generation
                || slot.m_referenceCount > 0
                || slot.m_parentCount > 0)
            {
                return false;
            }

            slot.m_shape = nullptr;
            materialHandles = AZStd::move(slot.m_materialHandles);
            customProviderExtension = slot.m_customProviderExtension;
            slot.m_customProviderId = AZ::TypeId::CreateNull();
            slot.m_customProviderExtension = ExtensionHandle::Invalid;
            slot.m_customDependencies.clear();
            for (const CookedShapeHandle childHandle : slot.m_childHandles)
            {
                CookedShapeSlot* childSlot = FindCookedShapeUnlocked(childHandle);
                AZ_Assert(childSlot && childSlot->m_parentCount > 0, "Cooked shape ownership is inconsistent.");
                if (childSlot && childSlot->m_parentCount > 0)
                {
                    --childSlot->m_parentCount;
                }
            }
            slot.m_childHandles.clear();
            if (Internal::AdvanceGeneration(slot.m_generation))
            {
                m_freeCookedShapeSlots.push_back(parts.m_index);
            }
        }

        ReleaseMaterials(materialHandles);
        if (customProviderExtension)
        {
            ReleaseCustomShapeProvider(customProviderExtension);
        }
        return true;
    }

    bool RuntimeImplementation::IsValid(
        const CookedShapeHandle cookedShapeHandle) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        return FindCookedShapeUnlocked(cookedShapeHandle);
    }

    bool RuntimeImplementation::GetStats(
        const CookedShapeHandle cookedShapeHandle,
        ShapeStats& stats) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot)
        {
            return false;
        }

        const JPH::Shape::Stats nativeStats = slot->m_shape->GetStats();
        const JPH::AABox nativeBounds = slot->m_shape->GetLocalBounds();
        stats = {
            .m_localBounds = AZ::Aabb::CreateFromMinMax(
                {nativeBounds.mMin.GetX(), nativeBounds.mMin.GetY(), nativeBounds.mMin.GetZ()},
                {nativeBounds.mMax.GetX(), nativeBounds.mMax.GetY(), nativeBounds.mMax.GetZ()}),
            .m_memorySize = nativeStats.mSizeBytes,
            .m_triangleCount = nativeStats.mNumTriangles,
        };
        return true;
    }

    bool RuntimeImplementation::GetStatsRecursive(
        const CookedShapeHandle cookedShapeHandle,
        ShapeStats& stats) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot)
        {
            return false;
        }

        JPH::Shape::VisitedShapes visitedShapes;
        const JPH::Shape::Stats nativeStats = slot->m_shape->GetStatsRecursive(visitedShapes);
        const JPH::AABox nativeBounds = slot->m_shape->GetLocalBounds();
        stats = {
            .m_localBounds = AZ::Aabb::CreateFromMinMax(
                {nativeBounds.mMin.GetX(), nativeBounds.mMin.GetY(), nativeBounds.mMin.GetZ()},
                {nativeBounds.mMax.GetX(), nativeBounds.mMax.GetY(), nativeBounds.mMax.GetZ()}),
            .m_memorySize = nativeStats.mSizeBytes,
            .m_triangleCount = nativeStats.mNumTriangles,
        };
        return true;
    }

    bool RuntimeImplementation::GetProperties(
        const CookedShapeHandle cookedShapeHandle,
        ShapeProperties& properties) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot)
        {
            return false;
        }

        const JPH::AABox nativeBounds = slot->m_shape->GetLocalBounds();
        const JPH::MassProperties nativeMassProperties = slot->m_shape->GetMassProperties();
        const JPH::Vec3 nativeCenterOfMass = slot->m_shape->GetCenterOfMass();
        const ShapeKind shapeKind = FromNativeShapeKind(slot->m_shape->GetSubType());

        properties = {
            .m_inertia = AZ::Matrix3x3::CreateFromColumns(
                {
                    nativeMassProperties.mInertia(0, 0),
                    nativeMassProperties.mInertia(1, 0),
                    nativeMassProperties.mInertia(2, 0),
                },
                {
                    nativeMassProperties.mInertia(0, 1),
                    nativeMassProperties.mInertia(1, 1),
                    nativeMassProperties.mInertia(2, 1),
                },
                {
                    nativeMassProperties.mInertia(0, 2),
                    nativeMassProperties.mInertia(1, 2),
                    nativeMassProperties.mInertia(2, 2),
                }),
            .m_localBounds = AZ::Aabb::CreateFromMinMax(
                {nativeBounds.mMin.GetX(), nativeBounds.mMin.GetY(), nativeBounds.mMin.GetZ()},
                {nativeBounds.mMax.GetX(), nativeBounds.mMax.GetY(), nativeBounds.mMax.GetZ()}),
            .m_centerOfMass = {
                nativeCenterOfMass.GetX(),
                nativeCenterOfMass.GetY(),
                nativeCenterOfMass.GetZ(),
            },
            .m_innerRadius = slot->m_shape->GetInnerRadius(),
            .m_mass = nativeMassProperties.mMass,
            .m_volume = slot->m_shape->GetVolume(),
            .m_subShapeIdBitCount = slot->m_shape->GetSubShapeIDBitsRecursive(),
            .m_kind = shapeKind,
            .m_mustBeStatic = slot->m_shape->MustBeStatic(),
        };
        return true;
    }

    bool RuntimeImplementation::GetUserData(
        const CookedShapeHandle cookedShapeHandle,
        AZ::u64& userData) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot)
        {
            return false;
        }

        userData = slot->m_shape->GetUserData();
        return true;
    }

    bool RuntimeImplementation::GetCustomConvexShapeInfo(
        const CookedShapeHandle cookedShapeHandle,
        CustomConvexShapeInfo& info) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        return slot && GetNativeCustomConvexShapeInfo(*slot->m_shape, info);
    }

    bool RuntimeImplementation::GetCustomShapeInfo(
        const CookedShapeHandle cookedShapeHandle,
        CustomShapeInfo& info) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        return slot && GetNativeCustomShapeInfo(*slot->m_shape, info);
    }

    BufferResult RuntimeImplementation::GetCustomShapeDependencies(
        const CookedShapeHandle cookedShapeHandle,
        const AZStd::span<CustomShapeDependency> dependencies) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot
            || slot->m_customProviderId.IsNull()
            || slot->m_customDependencies.size() > AZStd::numeric_limits<AZ::u32>::max())
        {
            return {};
        }

        const AZ::u32 requiredCount = aznumeric_cast<AZ::u32>(slot->m_customDependencies.size());
        if (dependencies.size() < slot->m_customDependencies.size())
        {
            return {.m_requiredCount = requiredCount};
        }

        AZStd::copy(
            slot->m_customDependencies.begin(),
            slot->m_customDependencies.end(),
            dependencies.begin());
        return {
            .m_count = requiredCount,
            .m_requiredCount = requiredCount,
        };
    }

    bool RuntimeImplementation::GetSubShapeUserData(
        const CookedShapeHandle cookedShapeHandle,
        const SubShapeId subShapeId,
        AZ::u64& userData) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot || !IsPotentiallyValidSubShapeId(*slot->m_shape, subShapeId))
        {
            return false;
        }

        JPH::SubShapeID nativeSubShapeId;
        nativeSubShapeId.SetValue(subShapeId.GetValue());
        userData = slot->m_shape->GetSubShapeUserData(nativeSubShapeId);
        return true;
    }

    bool RuntimeImplementation::GetDirectChildShape(
        const CookedShapeHandle cookedShapeHandle,
        const SubShapeId subShapeId,
        CookedShapeHandle& childShapeHandle,
        SubShapeTransform& transform) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot)
        {
            return false;
        }

        AZ::u32 childIndex = 0;
        SubShapeTransform resolvedTransform;
        if (!GetNativeDirectChildShape(
                *slot->m_shape,
                subShapeId,
                resolvedTransform,
                childIndex))
        {
            return false;
        }

        CookedShapeHandle resolvedHandle = cookedShapeHandle;
        if (childIndex != AZStd::numeric_limits<AZ::u32>::max())
        {
            if (childIndex >= slot->m_childHandles.size())
            {
                return false;
            }
            resolvedHandle = slot->m_childHandles[childIndex];
        }

        childShapeHandle = resolvedHandle;
        transform = resolvedTransform;
        return true;
    }

    BufferResult RuntimeImplementation::GetMeshMaterials(
        const CookedShapeHandle cookedShapeHandle,
        const AZStd::span<MaterialHandle> materialHandles) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot)
        {
            return {};
        }

        const JPH::MeshShape* mesh = FindNativeMesh(*slot->m_shape);
        if (!mesh)
        {
            return {};
        }
        const JPH::PhysicsMaterialList& nativeMaterials = mesh->GetMaterialList();
        if (nativeMaterials.size() > AZStd::numeric_limits<AZ::u32>::max())
        {
            return {};
        }

        const AZ::u32 requiredCount = aznumeric_cast<AZ::u32>(nativeMaterials.size());
        if (materialHandles.size() < nativeMaterials.size())
        {
            return {.m_requiredCount = requiredCount};
        }

        for (size_t materialIndex = 0; materialIndex < nativeMaterials.size(); ++materialIndex)
        {
            const MaterialHandle materialHandle = FindMaterialHandle(nativeMaterials[materialIndex]);
            if (!materialHandle)
            {
                return {};
            }
            materialHandles[materialIndex] = materialHandle;
        }

        return {
            .m_count = requiredCount,
            .m_requiredCount = requiredCount,
        };
    }

    bool RuntimeImplementation::GetMeshTriangleMaterialIndex(
        const CookedShapeHandle cookedShapeHandle,
        const SubShapeId subShapeId,
        AZ::u32& materialIndex) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        return slot && GetNativeMeshTriangleMaterialIndex(*slot->m_shape, subShapeId, materialIndex);
    }

    bool RuntimeImplementation::GetMeshTriangleUserData(
        const CookedShapeHandle cookedShapeHandle,
        const SubShapeId subShapeId,
        AZ::u32& userData) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        return slot && GetNativeMeshTriangleUserData(*slot->m_shape, subShapeId, userData);
    }

    bool RuntimeImplementation::GetCompoundChildCount(
        const CookedShapeHandle cookedShapeHandle,
        AZ::u32& childCount) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot || slot->m_shape->GetSubType() != JPH::EShapeSubType::StaticCompound)
        {
            return false;
        }

        const auto* compound = static_cast<const JPH::StaticCompoundShape*>(slot->m_shape.GetPtr());
        childCount = compound->GetNumSubShapes();
        return true;
    }

    bool RuntimeImplementation::GetCompoundChild(
        const CookedShapeHandle cookedShapeHandle,
        const AZ::u32 childIndex,
        CookedCompoundChildConfiguration& child) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot
            || slot->m_shape->GetSubType() != JPH::EShapeSubType::StaticCompound
            || childIndex >= slot->m_childHandles.size())
        {
            return false;
        }

        const auto* compound = static_cast<const JPH::StaticCompoundShape*>(slot->m_shape.GetPtr());
        const JPH::CompoundShape::SubShape& nativeChild = compound->GetSubShape(childIndex);
        const JPH::Quat nativeRotation = nativeChild.GetRotation();
        const JPH::Vec3 nativePosition = nativeChild.GetPositionCOM()
            + compound->GetCenterOfMass()
            - nativeRotation * nativeChild.mShape->GetCenterOfMass();
        child = {
            .m_position = AZ::Vector3(
                nativePosition.GetX(),
                nativePosition.GetY(),
                nativePosition.GetZ()),
            .m_rotation = AZ::Quaternion(
                nativeRotation.GetX(),
                nativeRotation.GetY(),
                nativeRotation.GetZ(),
                nativeRotation.GetW()),
            .m_shapeHandle = slot->m_childHandles[childIndex],
            .m_userData = compound->GetCompoundUserData(childIndex),
        };
        return true;
    }

    bool RuntimeImplementation::GetCompoundChildIndex(
        const CookedShapeHandle cookedShapeHandle,
        const SubShapeId subShapeId,
        AZ::u32& childIndex) const
    {
        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot
            || slot->m_shape->GetSubType() != JPH::EShapeSubType::StaticCompound
            || !IsPotentiallyValidSubShapeId(*slot->m_shape, subShapeId))
        {
            return false;
        }

        const auto* compound = static_cast<const JPH::StaticCompoundShape*>(slot->m_shape.GetPtr());
        JPH::SubShapeID nativeSubShapeId;
        nativeSubShapeId.SetValue(subShapeId.GetValue());
        JPH::SubShapeID remainder;
        const AZ::u32 nativeChildIndex = compound->GetSubShapeIndexFromID(nativeSubShapeId, remainder);
        if (nativeChildIndex >= slot->m_childHandles.size())
        {
            return false;
        }

        childIndex = nativeChildIndex;
        return true;
    }

    bool RuntimeImplementation::Raycast(
        const CookedShapeHandle cookedShapeHandle,
        const AZ::Vector3& start,
        const AZ::Vector3& direction,
        const float distance,
        CookedRaycastHit& hit) const
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::RaycastCookedShape");
        const DeterministicFloatScope floatScope;
        if (!start.IsFinite()
            || !direction.IsFinite()
            || direction.IsZero()
            || !AZ::IsFiniteFloat(distance)
            || distance <= 0.0f)
        {
            return false;
        }

        AZStd::shared_lock lock(m_cookedShapeMutex);
        const CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot)
        {
            return false;
        }

        const AZ::Vector3 normalizedDirection = direction.GetNormalized();
        const AZ::Vector3 displacement = normalizedDirection * distance;
        const JPH::Vec3 centerOfMass = slot->m_shape->GetCenterOfMass();
        const JPH::RayCast ray(
            JPH::Vec3(
                start.GetX() - centerOfMass.GetX(),
                start.GetY() - centerOfMass.GetY(),
                start.GetZ() - centerOfMass.GetZ()),
            ToNativeSystemVector(displacement));
        JPH::RayCastResult nativeHit;
        if (!slot->m_shape->CastRay(ray, JPH::SubShapeIDCreator(), nativeHit))
        {
            return false;
        }

        const JPH::Vec3 nativeHitPosition = ray.GetPointOnRay(nativeHit.mFraction);
        const JPH::Vec3 nativeNormal = slot->m_shape->GetSurfaceNormal(
            nativeHit.mSubShapeID2,
            nativeHitPosition);
        hit = {
            .m_position = start + displacement * nativeHit.mFraction,
            .m_normal = {nativeNormal.GetX(), nativeNormal.GetY(), nativeNormal.GetZ()},
            .m_materialHandle = FindMaterialHandle(slot->m_shape->GetMaterial(nativeHit.mSubShapeID2)),
            .m_subShapeId = SubShapeId(nativeHit.mSubShapeID2.GetValue()),
            .m_distance = distance * nativeHit.mFraction,
            .m_fraction = nativeHit.mFraction,
        };
        return true;
    }

    GroupFilterHandle RuntimeImplementation::CreateGroupFilter(
        const AZ::u32 subGroupCount,
        const ExtensionHandle extensionHandle)
    {
        if (subGroupCount > MaximumSubGroupCount)
        {
            return {};
        }

        auto* filter = static_cast<IGroupFilter*>(AcquireExtension(extensionHandle, ExtensionKind::GroupFilter));
        if (!filter)
        {
            return {};
        }

        JPH::Ref<JPH::GroupFilter> nativeFilter = new GroupFilterAdapter(filter);
        const AZ::u64 stateHash = MixGroupFilterValue(subGroupCount)
            ^ MixGroupFilterValue(filter->GetStateHash());
        const GroupFilterHandle filterHandle = StoreGroupFilter(
            AZStd::move(nativeFilter),
            subGroupCount,
            stateHash,
            true,
            extensionHandle);
        if (!filterHandle)
        {
            ReleaseExtension(extensionHandle);
        }
        return filterHandle;
    }

    GroupFilterHandle RuntimeImplementation::CreateGroupFilterTable(
        const GroupFilterTableConfiguration& configuration)
    {
        if (configuration.m_subGroupCount > MaximumSubGroupCount)
        {
            return {};
        }
        for (const SubGroupPair& disabledPair : configuration.m_disabledPairs)
        {
            if (!IsValidSubGroupPair(
                    disabledPair.m_first,
                    disabledPair.m_second,
                    configuration.m_subGroupCount))
            {
                return {};
            }
        }

        JPH::Ref<JPH::GroupFilterTable> filter = new JPH::GroupFilterTable(configuration.m_subGroupCount);
        AZ::u64 stateHash = MixGroupFilterValue(configuration.m_subGroupCount);
        for (const SubGroupPair& disabledPair : configuration.m_disabledPairs)
        {
            if (filter->IsCollisionEnabled(disabledPair.m_first.GetValue(), disabledPair.m_second.GetValue()))
            {
                filter->DisableCollision(disabledPair.m_first.GetValue(), disabledPair.m_second.GetValue());
                stateHash ^= GetSubGroupPairHash(disabledPair.m_first, disabledPair.m_second);
            }
        }

        JPH::Ref<JPH::GroupFilter> baseFilter = filter.GetPtr();
        return StoreGroupFilter(
            AZStd::move(baseFilter),
            configuration.m_subGroupCount,
            stateHash,
            false);
    }

    GroupFilterHandle RuntimeImplementation::StoreGroupFilter(
        JPH::Ref<JPH::GroupFilter> filter,
        const AZ::u32 subGroupCount,
        const AZ::u64 stateHash,
        const bool isCustom,
        const ExtensionHandle extensionHandle)
    {
        AZStd::lock_guard lock(m_groupFilterMutex);
        AZ::u32 filterIndex = 0;
        if (!m_freeGroupFilterSlots.empty())
        {
            filterIndex = m_freeGroupFilterSlots.back();
            m_freeGroupFilterSlots.pop_back();
        }
        else
        {
            if (m_groupFilterSlots.size() >= Internal::HandlePayloadMask)
            {
                return {};
            }
            filterIndex = static_cast<AZ::u32>(m_groupFilterSlots.size());
            m_groupFilterSlots.emplace_back();
        }

        GroupFilterSlot& slot = m_groupFilterSlots[filterIndex];
        slot.m_filter = AZStd::move(filter);
        slot.m_stateHash = stateHash;
        slot.m_subGroupCount = subGroupCount;
        slot.m_isCustom = isCustom;
        slot.m_extensionHandle = extensionHandle;
        return Internal::MakeResourceHandle<GroupFilterHandle>(filterIndex, slot.m_generation);
    }

    bool RuntimeImplementation::DestroyGroupFilter(
        const GroupFilterHandle filterHandle)
    {
        ExtensionHandle extensionHandle;
        {
            AZStd::lock_guard lock(m_groupFilterMutex);
            Internal::ResourceHandleParts parts;
            if (!Internal::DecodeResourceHandle(filterHandle, parts)
                || parts.m_index >= m_groupFilterSlots.size())
            {
                return false;
            }

            GroupFilterSlot& slot = m_groupFilterSlots[parts.m_index];
            if (!slot.m_filter || slot.m_generation != parts.m_generation || slot.m_referenceCount > 0)
            {
                return false;
            }

            extensionHandle = slot.m_extensionHandle;
            slot.m_filter = nullptr;
            slot.m_stateHash = 0;
            slot.m_subGroupCount = 0;
            slot.m_isCustom = false;
            slot.m_extensionHandle = ExtensionHandle::Invalid;
            if (Internal::AdvanceGeneration(slot.m_generation))
            {
                m_freeGroupFilterSlots.push_back(parts.m_index);
            }
        }

        if (extensionHandle)
        {
            ReleaseExtension(extensionHandle);
        }
        return true;
    }

    bool RuntimeImplementation::IsValid(
        const GroupFilterHandle filterHandle) const
    {
        AZStd::shared_lock lock(m_groupFilterMutex);
        return FindGroupFilterUnlocked(filterHandle);
    }

    bool RuntimeImplementation::NotifyGroupFilterChanged(
        const GroupFilterHandle filterHandle)
    {
        AZStd::lock_guard worldLock(m_worldMutex);
        AZStd::lock_guard filterLock(m_groupFilterMutex);
        GroupFilterSlot* slot = FindGroupFilterUnlocked(filterHandle);
        if (!slot || !slot->m_isCustom)
        {
            return false;
        }

        const auto* filter = static_cast<const GroupFilterAdapter*>(slot->m_filter.GetPtr());
        IGroupFilter* callbacks = filter->GetCallbacks();
        if (!callbacks)
        {
            return false;
        }

        slot->m_stateHash = MixGroupFilterValue(slot->m_subGroupCount)
            ^ MixGroupFilterValue(callbacks->GetStateHash());
        RefreshGroupFilterInWorlds(filterHandle);
        return true;
    }

    bool RuntimeImplementation::GetSubGroupCollisionEnabled(
        const GroupFilterHandle filterHandle,
        const CollisionSubGroupId firstSubGroup,
        const CollisionSubGroupId secondSubGroup,
        bool& enabled) const
    {
        AZStd::shared_lock lock(m_groupFilterMutex);
        const GroupFilterSlot* slot = FindGroupFilterUnlocked(filterHandle);
        if (!slot
            || slot->m_isCustom
            || !IsValidSubGroupPair(firstSubGroup, secondSubGroup, slot->m_subGroupCount))
        {
            return false;
        }

        const auto* filter = static_cast<const JPH::GroupFilterTable*>(slot->m_filter.GetPtr());
        enabled = filter->IsCollisionEnabled(firstSubGroup.GetValue(), secondSubGroup.GetValue());
        return true;
    }

    bool RuntimeImplementation::SetSubGroupCollisionEnabled(
        const GroupFilterHandle filterHandle,
        const CollisionSubGroupId firstSubGroup,
        const CollisionSubGroupId secondSubGroup,
        const bool enabled)
    {
        AZStd::lock_guard worldLock(m_worldMutex);
        AZStd::lock_guard filterLock(m_groupFilterMutex);
        GroupFilterSlot* slot = FindGroupFilterUnlocked(filterHandle);
        if (!slot
            || slot->m_isCustom
            || !IsValidSubGroupPair(firstSubGroup, secondSubGroup, slot->m_subGroupCount))
        {
            return false;
        }

        auto* filter = static_cast<JPH::GroupFilterTable*>(slot->m_filter.GetPtr());
        const bool currentValue = filter->IsCollisionEnabled(
            firstSubGroup.GetValue(),
            secondSubGroup.GetValue());
        if (currentValue == enabled)
        {
            return true;
        }
        if (enabled)
        {
            filter->EnableCollision(firstSubGroup.GetValue(), secondSubGroup.GetValue());
        }
        else
        {
            filter->DisableCollision(firstSubGroup.GetValue(), secondSubGroup.GetValue());
        }
        slot->m_stateHash ^= GetSubGroupPairHash(firstSubGroup, secondSubGroup);
        RefreshGroupFilterInWorlds(filterHandle);
        return true;
    }

    void RuntimeImplementation::RefreshGroupFilterInWorlds(
        const GroupFilterHandle filterHandle)
    {
        for (WorldSlot& worldSlot : m_worldSlots)
        {
            if (worldSlot.m_world)
            {
                worldSlot.m_world->NotifyGroupFilterChanged(filterHandle);
            }
        }
    }

    PathHandle RuntimeImplementation::CreatePath(
        const HermitePathConfiguration& configuration)
    {
        const DeterministicFloatScope floatScope;
        if (!configuration.IsValid())
        {
            return {};
        }

        JPH::Ref<JPH::PathConstraintPathHermite> path = new JPH::PathConstraintPathHermite();
        for (const HermitePathPoint& point : configuration.m_points)
        {
            if (!point.m_position.IsFinite()
                || !point.m_tangent.IsFinite()
                || point.m_tangent.IsZero()
                || !point.m_normal.IsFinite()
                || point.m_normal.IsZero()
                || !AZ::IsClose(point.m_tangent.GetNormalized().Dot(point.m_normal.GetNormalized()), 0.0f, 1.0e-3f))
            {
                return {};
            }
            path->AddPoint(
                JPH::Vec3(point.m_position.GetX(), point.m_position.GetY(), point.m_position.GetZ()),
                JPH::Vec3(point.m_tangent.GetX(), point.m_tangent.GetY(), point.m_tangent.GetZ()),
                JPH::Vec3(point.m_normal.GetX(), point.m_normal.GetY(), point.m_normal.GetZ()).Normalized());
        }
        path->SetIsLooping(configuration.m_isLooping);

        return StorePath(path.GetPtr());
    }

    PathHandle RuntimeImplementation::CreatePath(
        const CustomPathConfiguration& configuration)
    {
        const DeterministicFloatScope floatScope;
        if (configuration.m_providerId.IsNull())
        {
            return {};
        }

        float maximumFraction = 0.0f;
        AZ::u64 providerVersion = 0;
        ExtensionHandle providerExtension;
        ICustomPathProvider* provider = AcquireCustomPathProvider(
            configuration.m_providerId,
            configuration.m_data,
            maximumFraction,
            providerVersion,
            providerExtension);
        if (!provider)
        {
            return {};
        }

        JPH::Ref<CustomPathAdapter> path = new CustomPathAdapter(
            *provider,
            configuration.m_data,
            maximumFraction);
        path->SetIsLooping(configuration.m_isLooping);

        AZ::HashValue64 sourceHash = AZ::TypeHash64(
            providerVersion,
            AZ::HashValue64(static_cast<AZ::u64>(configuration.m_providerId.GetHash())));
        sourceHash = AZ::TypeHash64(configuration.m_isLooping, sourceHash);
        if (!configuration.m_data.empty())
        {
            sourceHash = AZ::TypeHash64(
                configuration.m_data.data(),
                configuration.m_data.size(),
                sourceHash);
        }

        const PathHandle pathHandle = StorePath(
            path.GetPtr(),
            configuration.m_providerId,
            providerExtension,
            providerVersion,
            static_cast<AZ::u64>(sourceHash));
        if (!pathHandle)
        {
            ReleaseCustomPathProvider(providerExtension);
        }
        return pathHandle;
    }

    PathHandle RuntimeImplementation::StorePath(
        JPH::RefConst<JPH::PathConstraintPath> path,
        const AZ::TypeId customProviderId,
        const ExtensionHandle customProviderExtension,
        const AZ::u64 customProviderVersion,
        const AZ::u64 sourceHash)
    {
        if (!path)
        {
            return {};
        }

        AZStd::lock_guard lock(m_pathMutex);
        AZ::u32 pathIndex = 0;
        if (!m_freePathSlots.empty())
        {
            pathIndex = m_freePathSlots.back();
            m_freePathSlots.pop_back();
        }
        else
        {
            if (m_pathSlots.size() >= Internal::HandlePayloadMask)
            {
                return {};
            }
            pathIndex = static_cast<AZ::u32>(m_pathSlots.size());
            m_pathSlots.emplace_back();
        }

        PathSlot& slot = m_pathSlots[pathIndex];
        slot.m_path = AZStd::move(path);
        slot.m_customProviderId = customProviderId;
        slot.m_customProviderExtension = customProviderExtension;
        slot.m_customProviderVersion = customProviderVersion;
        slot.m_sourceHash = sourceHash;
        return Internal::MakeResourceHandle<PathHandle>(pathIndex, slot.m_generation);
    }

    bool RuntimeImplementation::DestroyPath(
        const PathHandle pathHandle)
    {
        ExtensionHandle customProviderExtension;
        {
            AZStd::lock_guard lock(m_pathMutex);
            Internal::ResourceHandleParts parts;
            if (!Internal::DecodeResourceHandle(pathHandle, parts)
                || parts.m_index >= m_pathSlots.size())
            {
                return false;
            }

            PathSlot& slot = m_pathSlots[parts.m_index];
            if (!slot.m_path || slot.m_generation != parts.m_generation || slot.m_constraintCount > 0)
            {
                return false;
            }

            slot.m_path = nullptr;
            customProviderExtension = slot.m_customProviderExtension;
            slot.m_customProviderId = AZ::TypeId::CreateNull();
            slot.m_customProviderExtension = ExtensionHandle::Invalid;
            slot.m_customProviderVersion = 0;
            slot.m_sourceHash = 0;
            if (Internal::AdvanceGeneration(slot.m_generation))
            {
                m_freePathSlots.push_back(parts.m_index);
            }
        }

        if (customProviderExtension)
        {
            ReleaseCustomPathProvider(customProviderExtension);
        }
        return true;
    }

    bool RuntimeImplementation::IsValid(
        const PathHandle pathHandle) const
    {
        AZStd::shared_lock lock(m_pathMutex);
        return FindPathUnlocked(pathHandle);
    }

    bool RuntimeImplementation::GetPathState(
        const PathHandle pathHandle,
        PathState& state) const
    {
        const DeterministicFloatScope floatScope;
        state = {};
        AZStd::shared_lock lock(m_pathMutex);
        const PathSlot* slot = FindPathUnlocked(pathHandle);
        if (!slot)
        {
            return false;
        }

        state = {
            .m_maximumFraction = slot->m_path->GetPathMaxFraction(),
            .m_isLooping = slot->m_path->IsLooping(),
        };
        return true;
    }

    bool RuntimeImplementation::GetCustomPathInfo(
        const PathHandle pathHandle,
        CustomPathInfo& info) const
    {
        info = {};
        AZStd::shared_lock lock(m_pathMutex);
        const PathSlot* slot = FindPathUnlocked(pathHandle);
        if (!slot || slot->m_customProviderId.IsNull())
        {
            return false;
        }

        info = {
            .m_providerId = slot->m_customProviderId,
            .m_providerVersion = slot->m_customProviderVersion,
            .m_sourceHash = slot->m_sourceHash,
        };
        return true;
    }

    bool RuntimeImplementation::SamplePath(
        const PathHandle pathHandle,
        const float fraction,
        PathSample& sample) const
    {
        const DeterministicFloatScope floatScope;
        sample = {};
        AZStd::shared_lock lock(m_pathMutex);
        const PathSlot* slot = FindPathUnlocked(pathHandle);
        if (!slot
            || !AZ::IsFiniteFloat(fraction)
            || fraction < 0.0f
            || fraction > slot->m_path->GetPathMaxFraction())
        {
            return false;
        }

        sample = FromNativePathSample(*slot->m_path, fraction);
        return true;
    }

    bool RuntimeImplementation::FindClosestPathPoint(
        const PathHandle pathHandle,
        const AZ::Vector3& position,
        const float fractionHint,
        PathSample& sample) const
    {
        const DeterministicFloatScope floatScope;
        sample = {};
        AZStd::shared_lock lock(m_pathMutex);
        const PathSlot* slot = FindPathUnlocked(pathHandle);
        if (!slot
            || !position.IsFinite()
            || !AZ::IsFiniteFloat(fractionHint)
            || fractionHint < 0.0f
            || fractionHint > slot->m_path->GetPathMaxFraction())
        {
            return false;
        }

        const float fraction = slot->m_path->GetClosestPoint(
            JPH::Vec3(position.GetX(), position.GetY(), position.GetZ()),
            fractionHint);
        sample = FromNativePathSample(*slot->m_path, fraction);
        return true;
    }

    SoftBodyDefinitionHandle RuntimeImplementation::CreateSoftBodyDefinition(
        const SoftBodyDefinitionConfiguration& configuration,
        SoftBodyOptimizationRemap* optimizationRemap)
    {
        const DeterministicFloatScope floatScope;
        if (optimizationRemap)
        {
            *optimizationRemap = {};
        }
        if (configuration.m_vertices.empty()
            || !AZ::IsFiniteFloat(configuration.m_shearAngleTolerance)
            || configuration.m_shearAngleTolerance < 0.0f
            || configuration.m_shearAngleTolerance > AZ::Constants::Pi)
        {
            return {};
        }

        size_t materialCount = configuration.m_materials.size();
        if (materialCount == 0)
        {
            materialCount = 1;
        }
        for (const SoftBodyVertex& vertex : configuration.m_vertices)
        {
            if (!vertex.m_position.IsFinite()
                || !vertex.m_velocity.IsFinite()
                || !AZ::IsFiniteFloat(vertex.m_inverseMass)
                || vertex.m_inverseMass < 0.0f)
            {
                return {};
            }
        }
        for (const SoftBodyFace& face : configuration.m_faces)
        {
            if (face.m_firstVertex >= configuration.m_vertices.size()
                || face.m_secondVertex >= configuration.m_vertices.size()
                || face.m_thirdVertex >= configuration.m_vertices.size()
                || face.m_firstVertex == face.m_secondVertex
                || face.m_firstVertex == face.m_thirdVertex
                || face.m_secondVertex == face.m_thirdVertex
                || face.m_materialIndex >= materialCount)
            {
                return {};
            }
        }
        for (const SoftBodyVertexAttributes& attributes : configuration.m_vertexAttributes)
        {
            if (!AZ::IsFiniteFloat(attributes.m_bendCompliance)
                || attributes.m_bendCompliance < 0.0f
                || !AZ::IsFiniteFloat(attributes.m_compliance)
                || attributes.m_compliance < 0.0f
                || !AZ::IsFiniteFloat(attributes.m_longRangeMaximumDistanceMultiplier)
                || attributes.m_longRangeMaximumDistanceMultiplier <= 0.0f
                || !AZ::IsFiniteFloat(attributes.m_shearCompliance)
                || attributes.m_shearCompliance < 0.0f)
            {
                return {};
            }
        }
        for (const SoftBodyEdgeConstraint& constraint : configuration.m_edgeConstraints)
        {
            if (constraint.m_firstVertex >= configuration.m_vertices.size()
                || constraint.m_secondVertex >= configuration.m_vertices.size()
                || constraint.m_firstVertex == constraint.m_secondVertex
                || !AZ::IsFiniteFloat(constraint.m_compliance)
                || constraint.m_compliance < 0.0f
                || (constraint.m_restLength != -1.0f
                    && (!AZ::IsFiniteFloat(constraint.m_restLength)
                        || constraint.m_restLength < 0.0f)))
            {
                return {};
            }
        }
        for (const SoftBodyDihedralBendConstraint& constraint : configuration.m_dihedralBendConstraints)
        {
            if (constraint.m_firstVertex >= configuration.m_vertices.size()
                || constraint.m_secondVertex >= configuration.m_vertices.size()
                || constraint.m_thirdVertex >= configuration.m_vertices.size()
                || constraint.m_fourthVertex >= configuration.m_vertices.size()
                || !AZ::IsFiniteFloat(constraint.m_compliance)
                || constraint.m_compliance < 0.0f
                || (!constraint.m_calculateInitialAngle
                    && (!AZ::IsFiniteFloat(constraint.m_initialAngle)
                        || constraint.m_initialAngle < -AZ::Constants::Pi
                        || constraint.m_initialAngle > AZ::Constants::Pi)))
            {
                return {};
            }
        }
        for (const SoftBodyLongRangeConstraint& constraint : configuration.m_longRangeConstraints)
        {
            if (constraint.m_fixedVertex >= configuration.m_vertices.size()
                || constraint.m_dynamicVertex >= configuration.m_vertices.size()
                || constraint.m_fixedVertex == constraint.m_dynamicVertex
                || !AZ::IsFiniteFloat(constraint.m_maximumDistance)
                || constraint.m_maximumDistance < 0.0f)
            {
                return {};
            }
        }
        for (const SoftBodyRodStretchShearConstraint& constraint : configuration.m_rodStretchShearConstraints)
        {
            if (constraint.m_firstVertex >= configuration.m_vertices.size()
                || constraint.m_secondVertex >= configuration.m_vertices.size()
                || constraint.m_firstVertex == constraint.m_secondVertex
                || !AZ::IsFiniteFloat(constraint.m_compliance)
                || constraint.m_compliance < 0.0f
                || (constraint.m_restLength != -1.0f
                    && (!AZ::IsFiniteFloat(constraint.m_restLength)
                        || constraint.m_restLength <= 0.0f))
                || (constraint.m_inverseMass != -1.0f
                    && (!AZ::IsFiniteFloat(constraint.m_inverseMass)
                        || constraint.m_inverseMass < 0.0f))
                || (!constraint.m_calculateBishopRotation
                    && (!constraint.m_bishopRotation.IsFinite()
                        || constraint.m_bishopRotation.GetLengthSq() <= 0.0f)))
            {
                return {};
            }
        }
        for (const SoftBodyRodBendTwistConstraint& constraint : configuration.m_rodBendTwistConstraints)
        {
            if (constraint.m_firstRod >= configuration.m_rodStretchShearConstraints.size()
                || constraint.m_secondRod >= configuration.m_rodStretchShearConstraints.size()
                || constraint.m_firstRod == constraint.m_secondRod
                || !AZ::IsFiniteFloat(constraint.m_compliance)
                || constraint.m_compliance < 0.0f
                || (!constraint.m_calculateInitialRotation
                    && (!constraint.m_initialRotation.IsFinite()
                        || constraint.m_initialRotation.GetLengthSq() <= 0.0f)))
            {
                return {};
            }
        }
        for (const SoftBodyVolumeConstraint& constraint : configuration.m_volumeConstraints)
        {
            if (constraint.m_firstVertex >= configuration.m_vertices.size()
                || constraint.m_secondVertex >= configuration.m_vertices.size()
                || constraint.m_thirdVertex >= configuration.m_vertices.size()
                || constraint.m_fourthVertex >= configuration.m_vertices.size()
                || !AZ::IsFiniteFloat(constraint.m_compliance)
                || constraint.m_compliance < 0.0f
                || (!constraint.m_calculateRestVolume
                    && !AZ::IsFiniteFloat(constraint.m_restVolume)))
            {
                return {};
            }
        }
        for (const SoftBodyInverseBind& inverseBind : configuration.m_inverseBinds)
        {
            if (!inverseBind.m_transform.IsFinite())
            {
                return {};
            }
        }
        for (const SoftBodySkinConstraint& constraint : configuration.m_skinConstraints)
        {
            if (constraint.m_vertex >= configuration.m_vertices.size()
                || !AZ::IsFiniteFloat(constraint.m_backstopDistance)
                || !AZ::IsFiniteFloat(constraint.m_backstopRadius)
                || constraint.m_backstopRadius < 0.0f
                || !AZ::IsFiniteFloat(constraint.m_maximumDistance)
                || constraint.m_maximumDistance < 0.0f)
            {
                return {};
            }

            float totalWeight = 0.0f;
            for (const SoftBodySkinWeight& weight : constraint.m_weights)
            {
                if (!AZ::IsFiniteFloat(weight.m_weight)
                    || weight.m_weight < 0.0f
                    || (weight.m_weight > 0.0f
                        && weight.m_inverseBindIndex >= configuration.m_inverseBinds.size()))
                {
                    return {};
                }
                totalWeight += weight.m_weight;
            }
            if (!AZ::IsFiniteFloat(totalWeight) || totalWeight <= 0.0f)
            {
                return {};
            }
        }

        JPH::PhysicsMaterialList materials;
        if (!AcquireMaterials(configuration.m_materials, materials))
        {
            return {};
        }

        JPH::Ref<JPH::SoftBodySharedSettings> settings = new JPH::SoftBodySharedSettings();
        settings->mVertices.reserve(configuration.m_vertices.size());
        for (const SoftBodyVertex& vertex : configuration.m_vertices)
        {
            settings->mVertices.emplace_back(
                JPH::Float3(vertex.m_position.GetX(), vertex.m_position.GetY(), vertex.m_position.GetZ()),
                JPH::Float3(vertex.m_velocity.GetX(), vertex.m_velocity.GetY(), vertex.m_velocity.GetZ()),
                vertex.m_inverseMass);
        }
        settings->mFaces.reserve(configuration.m_faces.size());
        for (const SoftBodyFace& face : configuration.m_faces)
        {
            settings->AddFace({
                face.m_firstVertex,
                face.m_secondVertex,
                face.m_thirdVertex,
                face.m_materialIndex,
            });
        }
        if (!materials.empty())
        {
            settings->mMaterials = materials;
        }

        JPH::Array<JPH::SoftBodySharedSettings::VertexAttributes> attributes;
        if (configuration.m_vertexAttributes.empty())
        {
            attributes.emplace_back();
        }
        else
        {
            attributes.reserve(configuration.m_vertexAttributes.size());
            for (const SoftBodyVertexAttributes& source : configuration.m_vertexAttributes)
            {
                JPH::SoftBodySharedSettings::ELRAType attachmentType =
                    JPH::SoftBodySharedSettings::ELRAType::None;
                if (source.m_longRangeAttachmentType == SoftBodyLongRangeAttachmentType::EuclideanDistance)
                {
                    attachmentType = JPH::SoftBodySharedSettings::ELRAType::EuclideanDistance;
                }
                else if (source.m_longRangeAttachmentType == SoftBodyLongRangeAttachmentType::GeodesicDistance)
                {
                    attachmentType = JPH::SoftBodySharedSettings::ELRAType::GeodesicDistance;
                }
                attributes.emplace_back(
                    source.m_compliance,
                    source.m_shearCompliance,
                    source.m_bendCompliance,
                    attachmentType,
                    source.m_longRangeMaximumDistanceMultiplier);
            }
        }

        JPH::SoftBodySharedSettings::EBendType bendType = JPH::SoftBodySharedSettings::EBendType::None;
        if (configuration.m_bendType == SoftBodyBendType::Dihedral)
        {
            bendType = JPH::SoftBodySharedSettings::EBendType::Dihedral;
        }
        else if (configuration.m_bendType == SoftBodyBendType::Distance)
        {
            bendType = JPH::SoftBodySharedSettings::EBendType::Distance;
        }
        if (configuration.m_createFaceConstraints)
        {
            settings->CreateConstraints(
                attributes.data(),
                static_cast<JPH::uint>(attributes.size()),
                bendType,
                configuration.m_shearAngleTolerance);
        }

        const size_t edgeConstraintStart = settings->mEdgeConstraints.size();
        settings->mEdgeConstraints.reserve(
            settings->mEdgeConstraints.size() + configuration.m_edgeConstraints.size());
        for (const SoftBodyEdgeConstraint& constraint : configuration.m_edgeConstraints)
        {
            settings->mEdgeConstraints.emplace_back(
                constraint.m_firstVertex,
                constraint.m_secondVertex,
                constraint.m_compliance);
        }
        if (!configuration.m_edgeConstraints.empty())
        {
            settings->CalculateEdgeLengths();
            for (size_t sourceIndex = 0; sourceIndex < configuration.m_edgeConstraints.size(); ++sourceIndex)
            {
                const float restLength = configuration.m_edgeConstraints[sourceIndex].m_restLength;
                if (restLength >= 0.0f)
                {
                    settings->mEdgeConstraints[edgeConstraintStart + sourceIndex].mRestLength = restLength;
                }
            }
        }

        const size_t dihedralBendConstraintStart = settings->mDihedralBendConstraints.size();
        settings->mDihedralBendConstraints.reserve(
            settings->mDihedralBendConstraints.size() + configuration.m_dihedralBendConstraints.size());
        for (const SoftBodyDihedralBendConstraint& constraint : configuration.m_dihedralBendConstraints)
        {
            settings->mDihedralBendConstraints.emplace_back(
                constraint.m_firstVertex,
                constraint.m_secondVertex,
                constraint.m_thirdVertex,
                constraint.m_fourthVertex,
                constraint.m_compliance);
        }
        if (!configuration.m_dihedralBendConstraints.empty())
        {
            settings->CalculateBendConstraintConstants();
            for (size_t sourceIndex = 0; sourceIndex < configuration.m_dihedralBendConstraints.size(); ++sourceIndex)
            {
                const SoftBodyDihedralBendConstraint& source =
                    configuration.m_dihedralBendConstraints[sourceIndex];
                if (!source.m_calculateInitialAngle)
                {
                    settings->mDihedralBendConstraints[dihedralBendConstraintStart + sourceIndex].mInitialAngle =
                        source.m_initialAngle;
                }
            }
        }

        settings->mLRAConstraints.reserve(
            settings->mLRAConstraints.size() + configuration.m_longRangeConstraints.size());
        for (const SoftBodyLongRangeConstraint& constraint : configuration.m_longRangeConstraints)
        {
            settings->mLRAConstraints.emplace_back(
                constraint.m_fixedVertex,
                constraint.m_dynamicVertex,
                constraint.m_maximumDistance);
        }

        const size_t rodStretchShearConstraintStart = settings->mRodStretchShearConstraints.size();
        settings->mRodStretchShearConstraints.reserve(
            settings->mRodStretchShearConstraints.size() + configuration.m_rodStretchShearConstraints.size());
        for (const SoftBodyRodStretchShearConstraint& constraint : configuration.m_rodStretchShearConstraints)
        {
            settings->mRodStretchShearConstraints.emplace_back(
                constraint.m_firstVertex,
                constraint.m_secondVertex,
                constraint.m_compliance);
        }
        const size_t rodBendTwistConstraintStart = settings->mRodBendTwistConstraints.size();
        settings->mRodBendTwistConstraints.reserve(
            settings->mRodBendTwistConstraints.size() + configuration.m_rodBendTwistConstraints.size());
        for (const SoftBodyRodBendTwistConstraint& constraint : configuration.m_rodBendTwistConstraints)
        {
            settings->mRodBendTwistConstraints.emplace_back(
                constraint.m_firstRod,
                constraint.m_secondRod,
                constraint.m_compliance);
        }
        if (!configuration.m_rodStretchShearConstraints.empty())
        {
            settings->CalculateRodProperties();
            for (size_t sourceIndex = 0; sourceIndex < configuration.m_rodStretchShearConstraints.size(); ++sourceIndex)
            {
                const SoftBodyRodStretchShearConstraint& source =
                    configuration.m_rodStretchShearConstraints[sourceIndex];
                JPH::SoftBodySharedSettings::RodStretchShear& target =
                    settings->mRodStretchShearConstraints[rodStretchShearConstraintStart + sourceIndex];
                if (source.m_restLength >= 0.0f)
                {
                    target.mLength = source.m_restLength;
                }
                if (source.m_inverseMass >= 0.0f)
                {
                    target.mInvMass = source.m_inverseMass;
                }
                if (!source.m_calculateBishopRotation)
                {
                    target.mBishop = ToNativeSystemRotation(source.m_bishopRotation);
                }
            }
            for (size_t sourceIndex = 0; sourceIndex < configuration.m_rodBendTwistConstraints.size(); ++sourceIndex)
            {
                const SoftBodyRodBendTwistConstraint& source =
                    configuration.m_rodBendTwistConstraints[sourceIndex];
                if (!source.m_calculateInitialRotation)
                {
                    settings->mRodBendTwistConstraints[rodBendTwistConstraintStart + sourceIndex].mOmega0 =
                        ToNativeSystemRotation(source.m_initialRotation);
                }
            }
        }

        const size_t volumeConstraintStart = settings->mVolumeConstraints.size();
        settings->mVolumeConstraints.reserve(
            settings->mVolumeConstraints.size() + configuration.m_volumeConstraints.size());
        for (const SoftBodyVolumeConstraint& constraint : configuration.m_volumeConstraints)
        {
            settings->mVolumeConstraints.emplace_back(
                constraint.m_firstVertex,
                constraint.m_secondVertex,
                constraint.m_thirdVertex,
                constraint.m_fourthVertex,
                constraint.m_compliance);
        }
        if (!configuration.m_volumeConstraints.empty())
        {
            settings->CalculateVolumeConstraintVolumes();
            for (size_t sourceIndex = 0; sourceIndex < configuration.m_volumeConstraints.size(); ++sourceIndex)
            {
                const SoftBodyVolumeConstraint& source = configuration.m_volumeConstraints[sourceIndex];
                if (!source.m_calculateRestVolume)
                {
                    settings->mVolumeConstraints[volumeConstraintStart + sourceIndex].mSixRestVolume =
                        6.0f * source.m_restVolume;
                }
            }
        }
        settings->mInvBindMatrices.reserve(configuration.m_inverseBinds.size());
        for (const SoftBodyInverseBind& inverseBind : configuration.m_inverseBinds)
        {
            settings->mInvBindMatrices.emplace_back(
                inverseBind.m_jointIndex,
                ToNativeSystemTransform(inverseBind.m_transform));
        }
        settings->mSkinnedConstraints.reserve(configuration.m_skinConstraints.size());
        for (const SoftBodySkinConstraint& constraint : configuration.m_skinConstraints)
        {
            JPH::SoftBodySharedSettings::Skinned nativeConstraint(
                constraint.m_vertex,
                constraint.m_maximumDistance,
                constraint.m_backstopDistance,
                constraint.m_backstopRadius);
            size_t nativeWeightIndex = 0;
            for (const SoftBodySkinWeight& weight : constraint.m_weights)
            {
                if (weight.m_weight > 0.0f)
                {
                    nativeConstraint.mWeights[nativeWeightIndex] = {
                        weight.m_inverseBindIndex,
                        weight.m_weight,
                    };
                    ++nativeWeightIndex;
                }
            }
            nativeConstraint.NormalizeWeights();
            settings->mSkinnedConstraints.push_back(nativeConstraint);
        }
        if (!configuration.m_skinConstraints.empty())
        {
            settings->CalculateSkinnedConstraintNormals();
        }

        SoftBodyOptimizationRemap generatedOptimizationRemap;
        const auto copyRemap =
            [](const JPH::Array<JPH::uint>& source, AZStd::vector<AZ::u32>& target)
            {
                target.reserve(source.size());
                for (const JPH::uint index : source)
                {
                    target.push_back(index);
                }
            };
        const auto createIdentityRemap =
            [](const size_t size, AZStd::vector<AZ::u32>& target)
            {
                target.reserve(size);
                for (size_t index = 0; index < size; ++index)
                {
                    target.push_back(aznumeric_cast<AZ::u32>(index));
                }
            };
        if (configuration.m_optimize)
        {
            JPH::SoftBodySharedSettings::OptimizationResults results;
            settings->Optimize(results);
            copyRemap(results.mDihedralBendRemap, generatedOptimizationRemap.m_dihedralBendConstraints);
            copyRemap(results.mEdgeRemap, generatedOptimizationRemap.m_edgeConstraints);
            copyRemap(results.mLRARemap, generatedOptimizationRemap.m_longRangeConstraints);
            copyRemap(results.mRodBendTwistConstraintRemap, generatedOptimizationRemap.m_rodBendTwistConstraints);
            copyRemap(results.mRodStretchShearConstraintRemap, generatedOptimizationRemap.m_rodStretchShearConstraints);
            copyRemap(results.mSkinnedRemap, generatedOptimizationRemap.m_skinConstraints);
            copyRemap(results.mVolumeRemap, generatedOptimizationRemap.m_volumeConstraints);
        }
        else
        {
            createIdentityRemap(
                settings->mDihedralBendConstraints.size(),
                generatedOptimizationRemap.m_dihedralBendConstraints);
            createIdentityRemap(settings->mEdgeConstraints.size(), generatedOptimizationRemap.m_edgeConstraints);
            createIdentityRemap(settings->mLRAConstraints.size(), generatedOptimizationRemap.m_longRangeConstraints);
            createIdentityRemap(
                settings->mRodBendTwistConstraints.size(),
                generatedOptimizationRemap.m_rodBendTwistConstraints);
            createIdentityRemap(
                settings->mRodStretchShearConstraints.size(),
                generatedOptimizationRemap.m_rodStretchShearConstraints);
            createIdentityRemap(settings->mSkinnedConstraints.size(), generatedOptimizationRemap.m_skinConstraints);
            createIdentityRemap(settings->mVolumeConstraints.size(), generatedOptimizationRemap.m_volumeConstraints);
        }

        const SoftBodyDefinitionHandle definitionHandle = StoreSoftBodyDefinition(
            settings,
            configuration.m_materials);
        if (definitionHandle && optimizationRemap)
        {
            *optimizationRemap = AZStd::move(generatedOptimizationRemap);
        }
        return definitionHandle;
    }

    bool RuntimeImplementation::ExportSoftBodyDefinition(
        const SoftBodyDefinitionHandle definitionHandle,
        SoftBodyDefinitionArchive& archive,
        AZStd::vector<MaterialHandle>& materialHandles) const
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::ExportSoftBodyDefinition");
        const DeterministicFloatScope floatScope;

        JPH::RefConst<JPH::SoftBodySharedSettings> settings;
        AZStd::vector<MaterialHandle> exportedMaterialHandles;
        {
            AZStd::shared_lock lock(m_softBodyDefinitionMutex);
            const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
            if (!slot)
            {
                return false;
            }
            settings = slot->m_settings;
            exportedMaterialHandles = slot->m_materialHandles;
        }

        NativeArchiveWriter writer;
        settings->SaveBinaryState(writer);
        if (writer.IsFailed())
        {
            return false;
        }

        SoftBodyDefinitionArchive exportedArchive;
        exportedArchive.m_binaryState = writer.TakeData();
        if (exportedArchive.m_binaryState.empty()
            || exportedMaterialHandles.size() > AZStd::numeric_limits<AZ::u32>::max())
        {
            return false;
        }
        exportedArchive.m_buildFingerprint = GetNativeBuildFingerprint();
        exportedArchive.m_contentHash = static_cast<AZ::u64>(AZ::TypeHash64(
            exportedArchive.m_binaryState.data(),
            exportedArchive.m_binaryState.size()));
        exportedArchive.m_formatVersion = SoftBodyDefinitionArchiveFormatVersion;
        exportedArchive.m_materialCount = aznumeric_cast<AZ::u32>(exportedMaterialHandles.size());

        archive = AZStd::move(exportedArchive);
        materialHandles = AZStd::move(exportedMaterialHandles);
        return true;
    }

    SoftBodyDefinitionHandle RuntimeImplementation::ImportSoftBodyDefinition(
        const SoftBodyDefinitionArchive& archive,
        const AZStd::span<const MaterialHandle> materialHandles)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::RuntimeImplementation::ImportSoftBodyDefinition");
        const DeterministicFloatScope floatScope;
        if (archive.m_formatVersion != SoftBodyDefinitionArchiveFormatVersion
            || archive.m_buildFingerprint != GetNativeBuildFingerprint()
            || archive.m_binaryState.empty()
            || archive.m_binaryState.size() > MaximumNativeArchiveSize
            || archive.m_materialCount != materialHandles.size()
            || archive.m_contentHash != static_cast<AZ::u64>(AZ::TypeHash64(
                archive.m_binaryState.data(),
                archive.m_binaryState.size())))
        {
            return {};
        }

        NativeArchiveReader reader(archive.m_binaryState);
        JPH::Ref<JPH::SoftBodySharedSettings> settings = new JPH::SoftBodySharedSettings();
        settings->RestoreBinaryState(reader);
        if (reader.IsFailed() || !reader.IsFullyConsumed())
        {
            return {};
        }

        JPH::PhysicsMaterialList nativeMaterials;
        if (!AcquireMaterials(materialHandles, nativeMaterials))
        {
            return {};
        }
        if (!nativeMaterials.empty())
        {
            settings->mMaterials = AZStd::move(nativeMaterials);
        }

        return StoreSoftBodyDefinition(
            settings,
            {materialHandles.begin(), materialHandles.end()});
    }

    bool RuntimeImplementation::DestroySoftBodyDefinition(
        const SoftBodyDefinitionHandle definitionHandle)
    {
        AZStd::lock_guard lock(m_softBodyDefinitionMutex);
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(definitionHandle, parts)
            || parts.m_index >= m_softBodyDefinitionSlots.size())
        {
            return false;
        }

        SoftBodyDefinitionSlot& slot = m_softBodyDefinitionSlots[parts.m_index];
        if (!slot.m_settings || slot.m_generation != parts.m_generation || slot.m_bodyCount > 0)
        {
            return false;
        }

        slot.m_settings = nullptr;
        ReleaseMaterials(slot.m_materialHandles);
        slot.m_materialHandles.clear();
        if (Internal::AdvanceGeneration(slot.m_generation))
        {
            m_freeSoftBodyDefinitionSlots.push_back(parts.m_index);
        }
        return true;
    }

    bool RuntimeImplementation::IsValid(
        const SoftBodyDefinitionHandle definitionHandle) const
    {
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        return FindSoftBodyDefinitionUnlocked(definitionHandle);
    }

    bool RuntimeImplementation::GetSoftBodyDefinitionState(
        const SoftBodyDefinitionHandle definitionHandle,
        SoftBodyDefinitionState& state) const
    {
        state = {};
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return false;
        }

        const JPH::SoftBodySharedSettings& settings = *slot->m_settings;
        state = {
            .m_vertexCount = aznumeric_cast<AZ::u32>(settings.mVertices.size()),
            .m_faceCount = aznumeric_cast<AZ::u32>(settings.mFaces.size()),
            .m_materialCount = aznumeric_cast<AZ::u32>(settings.mMaterials.size()),
            .m_dihedralBendConstraintCount =
                aznumeric_cast<AZ::u32>(settings.mDihedralBendConstraints.size()),
            .m_edgeConstraintCount = aznumeric_cast<AZ::u32>(settings.mEdgeConstraints.size()),
            .m_longRangeConstraintCount = aznumeric_cast<AZ::u32>(settings.mLRAConstraints.size()),
            .m_rodBendTwistConstraintCount =
                aznumeric_cast<AZ::u32>(settings.mRodBendTwistConstraints.size()),
            .m_rodStretchShearConstraintCount =
                aznumeric_cast<AZ::u32>(settings.mRodStretchShearConstraints.size()),
            .m_skinConstraintCount = aznumeric_cast<AZ::u32>(settings.mSkinnedConstraints.size()),
            .m_volumeConstraintCount = aznumeric_cast<AZ::u32>(settings.mVolumeConstraints.size()),
            .m_inverseBindCount = aznumeric_cast<AZ::u32>(settings.mInvBindMatrices.size()),
        };
        return true;
    }

    QueryResult RuntimeImplementation::GetSoftBodyDefinitionDihedralBendConstraints(
        const SoftBodyDefinitionHandle definitionHandle,
        const AZStd::span<SoftBodyDihedralBendConstraint> constraints) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        return CopyConvertedRange(
            slot->m_settings->mDihedralBendConstraints,
            constraints,
            [](const JPH::SoftBodySharedSettings::DihedralBend& source)
            {
                return SoftBodyDihedralBendConstraint{
                    .m_firstVertex = source.mVertex[0],
                    .m_secondVertex = source.mVertex[1],
                    .m_thirdVertex = source.mVertex[2],
                    .m_fourthVertex = source.mVertex[3],
                    .m_compliance = source.mCompliance,
                    .m_initialAngle = source.mInitialAngle,
                    .m_calculateInitialAngle = false,
                };
            });
    }

    QueryResult RuntimeImplementation::GetSoftBodyDefinitionEdgeConstraints(
        const SoftBodyDefinitionHandle definitionHandle,
        const AZStd::span<SoftBodyEdgeConstraint> constraints) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        return CopyConvertedRange(
            slot->m_settings->mEdgeConstraints,
            constraints,
            [](const JPH::SoftBodySharedSettings::Edge& source)
            {
                return SoftBodyEdgeConstraint{
                    .m_firstVertex = source.mVertex[0],
                    .m_secondVertex = source.mVertex[1],
                    .m_compliance = source.mCompliance,
                    .m_restLength = source.mRestLength,
                };
            });
    }

    QueryResult RuntimeImplementation::GetSoftBodyDefinitionFaces(
        const SoftBodyDefinitionHandle definitionHandle,
        const AZStd::span<SoftBodyFace> faces) const
    {
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        return CopyConvertedRange(
            slot->m_settings->mFaces,
            faces,
            [](const JPH::SoftBodySharedSettings::Face& source)
            {
                return SoftBodyFace{
                    .m_firstVertex = source.mVertex[0],
                    .m_secondVertex = source.mVertex[1],
                    .m_thirdVertex = source.mVertex[2],
                    .m_materialIndex = source.mMaterialIndex,
                };
            });
    }

    QueryResult RuntimeImplementation::GetSoftBodyDefinitionInverseBinds(
        const SoftBodyDefinitionHandle definitionHandle,
        const AZStd::span<SoftBodyInverseBind> inverseBinds) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        return CopyConvertedRange(
            slot->m_settings->mInvBindMatrices,
            inverseBinds,
            [](const JPH::SoftBodySharedSettings::InvBind& source)
            {
                return SoftBodyInverseBind{
                    .m_transform = FromNativeTransform(source.mInvBind),
                    .m_jointIndex = source.mJointIndex,
                };
            });
    }

    QueryResult RuntimeImplementation::GetSoftBodyDefinitionLongRangeConstraints(
        const SoftBodyDefinitionHandle definitionHandle,
        const AZStd::span<SoftBodyLongRangeConstraint> constraints) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        return CopyConvertedRange(
            slot->m_settings->mLRAConstraints,
            constraints,
            [](const JPH::SoftBodySharedSettings::LRA& source)
            {
                return SoftBodyLongRangeConstraint{
                    .m_fixedVertex = source.mVertex[0],
                    .m_dynamicVertex = source.mVertex[1],
                    .m_maximumDistance = source.mMaxDistance,
                };
            });
    }

    QueryResult RuntimeImplementation::GetSoftBodyDefinitionMaterials(
        const SoftBodyDefinitionHandle definitionHandle,
        const AZStd::span<MaterialHandle> materials) const
    {
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        const size_t materialCount = slot->m_settings->mMaterials.size();
        const size_t copiedCount = AZStd::min(materialCount, materials.size());
        if (slot->m_materialHandles.empty())
        {
            AZStd::fill_n(materials.begin(), copiedCount, MaterialHandle::Invalid);
        }
        else
        {
            AZStd::copy(
                slot->m_materialHandles.begin(),
                slot->m_materialHandles.begin() + copiedCount,
                materials.begin());
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copiedCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(materialCount),
        };
    }

    QueryResult RuntimeImplementation::GetSoftBodyDefinitionRodBendTwistConstraints(
        const SoftBodyDefinitionHandle definitionHandle,
        const AZStd::span<SoftBodyRodBendTwistConstraint> constraints) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        return CopyConvertedRange(
            slot->m_settings->mRodBendTwistConstraints,
            constraints,
            [](const JPH::SoftBodySharedSettings::RodBendTwist& source)
            {
                return SoftBodyRodBendTwistConstraint{
                    .m_firstRod = source.mRod[0],
                    .m_secondRod = source.mRod[1],
                    .m_initialRotation = FromNativeSystemRotation(source.mOmega0),
                    .m_compliance = source.mCompliance,
                    .m_calculateInitialRotation = false,
                };
            });
    }

    QueryResult RuntimeImplementation::GetSoftBodyDefinitionRodStretchShearConstraints(
        const SoftBodyDefinitionHandle definitionHandle,
        const AZStd::span<SoftBodyRodStretchShearConstraint> constraints) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        return CopyConvertedRange(
            slot->m_settings->mRodStretchShearConstraints,
            constraints,
            [](const JPH::SoftBodySharedSettings::RodStretchShear& source)
            {
                return SoftBodyRodStretchShearConstraint{
                    .m_firstVertex = source.mVertex[0],
                    .m_secondVertex = source.mVertex[1],
                    .m_bishopRotation = FromNativeSystemRotation(source.mBishop),
                    .m_compliance = source.mCompliance,
                    .m_inverseMass = source.mInvMass,
                    .m_restLength = source.mLength,
                    .m_calculateBishopRotation = false,
                };
            });
    }

    QueryResult RuntimeImplementation::GetSoftBodyDefinitionSkinConstraints(
        const SoftBodyDefinitionHandle definitionHandle,
        const AZStd::span<SoftBodySkinConstraint> constraints) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        return CopyConvertedRange(
            slot->m_settings->mSkinnedConstraints,
            constraints,
            [](const JPH::SoftBodySharedSettings::Skinned& source)
            {
                SoftBodySkinConstraint result{
                    .m_vertex = source.mVertex,
                    .m_backstopDistance = source.mBackStopDistance,
                    .m_backstopRadius = source.mBackStopRadius,
                    .m_maximumDistance = source.mMaxDistance,
                };
                for (size_t weightIndex = 0; weightIndex < result.m_weights.size(); ++weightIndex)
                {
                    result.m_weights[weightIndex] = {
                        .m_inverseBindIndex = source.mWeights[weightIndex].mInvBindIndex,
                        .m_weight = source.mWeights[weightIndex].mWeight,
                    };
                }
                return result;
            });
    }

    QueryResult RuntimeImplementation::GetSoftBodyDefinitionVertices(
        const SoftBodyDefinitionHandle definitionHandle,
        const AZStd::span<SoftBodyVertex> vertices) const
    {
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        return CopyConvertedRange(
            slot->m_settings->mVertices,
            vertices,
            [](const JPH::SoftBodySharedSettings::Vertex& source)
            {
                const JPH::Vec3 position(source.mPosition);
                const JPH::Vec3 velocity(source.mVelocity);
                return SoftBodyVertex{
                    .m_position = {position.GetX(), position.GetY(), position.GetZ()},
                    .m_velocity = {velocity.GetX(), velocity.GetY(), velocity.GetZ()},
                    .m_inverseMass = source.mInvMass,
                };
            });
    }

    QueryResult RuntimeImplementation::GetSoftBodyDefinitionVolumeConstraints(
        const SoftBodyDefinitionHandle definitionHandle,
        const AZStd::span<SoftBodyVolumeConstraint> constraints) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_softBodyDefinitionMutex);
        const SoftBodyDefinitionSlot* slot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        return CopyConvertedRange(
            slot->m_settings->mVolumeConstraints,
            constraints,
            [](const JPH::SoftBodySharedSettings::Volume& source)
            {
                return SoftBodyVolumeConstraint{
                    .m_firstVertex = source.mVertex[0],
                    .m_secondVertex = source.mVertex[1],
                    .m_thirdVertex = source.mVertex[2],
                    .m_fourthVertex = source.mVertex[3],
                    .m_compliance = source.mCompliance,
                    .m_restVolume = source.mSixRestVolume / 6.0f,
                    .m_calculateRestVolume = false,
                };
            });
    }

    HairDefinitionHandle RuntimeImplementation::CreateHairDefinition(
        const HairDefinitionConfiguration& configuration)
    {
        const DeterministicFloatScope floatScope;
        if (configuration.m_vertices.empty()
            || configuration.m_strands.empty()
            || configuration.m_materials.empty()
            || configuration.m_gridSizeX < 2
            || configuration.m_gridSizeY < 2
            || configuration.m_gridSizeZ < 2)
        {
            return {};
        }

        const AZ::u64 gridArea =
            static_cast<AZ::u64>(configuration.m_gridSizeX) * configuration.m_gridSizeY;
        if (gridArea > AZStd::numeric_limits<AZ::u32>::max() / configuration.m_gridSizeZ
            || configuration.m_iterationsPerSecond == 0
            || (configuration.m_verticesPerStrand == 1)
            || !AZ::IsFiniteFloat(configuration.m_maximumDeltaTime)
            || configuration.m_maximumDeltaTime <= 0.0f
            || !configuration.m_initialGravity.IsFinite()
            || !configuration.m_simulationBoundsPadding.IsFinite()
            || configuration.m_simulationBoundsPadding.GetMinElement() < 0.0f)
        {
            return {};
        }

        for (const HairVertex& vertex : configuration.m_vertices)
        {
            if (!vertex.m_position.IsFinite()
                || !AZ::IsFiniteFloat(vertex.m_inverseMass)
                || vertex.m_inverseMass < 0.0f)
            {
                return {};
            }
        }
        for (const HairStrand& strand : configuration.m_strands)
        {
            if (strand.m_beginVertex >= configuration.m_vertices.size()
                || strand.m_endVertex > configuration.m_vertices.size()
                || strand.m_endVertex - strand.m_beginVertex < 2
                || strand.m_materialIndex >= configuration.m_materials.size())
            {
                return {};
            }
        }
        for (const HairMaterialConfiguration& material : configuration.m_materials)
        {
            if (!IsValidHairGradient(material.m_globalPose)
                || !IsValidHairGradient(material.m_gravityFactor)
                || !IsValidHairGradient(material.m_gridVelocityFactor)
                || !IsValidHairGradient(material.m_radius)
                || !IsValidHairGradient(material.m_skinGlobalPose)
                || !IsValidHairGradient(material.m_worldTransformInfluence)
                || !AZ::IsFiniteFloat(material.m_angularDamping)
                || material.m_angularDamping < 0.0f
                || !AZ::IsFiniteFloat(material.m_bendCompliance)
                || material.m_bendCompliance < 0.0f
                || !AZ::IsFiniteFloat(material.m_friction)
                || material.m_friction < 0.0f
                || !AZ::IsFiniteFloat(material.m_gravityPreloadFactor)
                || !AZ::IsFiniteFloat(material.m_gridDensityForceFactor)
                || !AZ::IsFiniteFloat(material.m_inertiaMultiplier)
                || material.m_inertiaMultiplier <= 0.0f
                || !AZ::IsFiniteFloat(material.m_linearDamping)
                || material.m_linearDamping < 0.0f
                || !AZ::IsFiniteFloat(material.m_maximumAngularVelocity)
                || material.m_maximumAngularVelocity < 0.0f
                || !AZ::IsFiniteFloat(material.m_maximumLinearVelocity)
                || material.m_maximumLinearVelocity < 0.0f
                || !AZ::IsFiniteFloat(material.m_simulationStrandFraction)
                || material.m_simulationStrandFraction <= 0.0f
                || material.m_simulationStrandFraction > 1.0f
                || !AZ::IsFiniteFloat(material.m_stretchCompliance)
                || material.m_stretchCompliance < 0.0f)
            {
                return {};
            }
            for (const float multiplier : material.m_bendComplianceMultipliers)
            {
                if (!AZ::IsFiniteFloat(multiplier) || multiplier < 0.0f)
                {
                    return {};
                }
            }
        }

        const bool hasScalp = !configuration.m_scalpVertices.empty()
            || !configuration.m_scalpTriangles.empty()
            || !configuration.m_scalpInverseBindPoses.empty()
            || !configuration.m_scalpSkinWeights.empty()
            || configuration.m_scalpSkinWeightsPerVertex > 0;
        if (hasScalp
            && (configuration.m_scalpVertices.empty()
                || configuration.m_scalpTriangles.empty()
                || configuration.m_scalpInverseBindPoses.empty()
                || configuration.m_scalpSkinWeightsPerVertex == 0
                || configuration.m_scalpSkinWeights.size()
                    != configuration.m_scalpVertices.size() * configuration.m_scalpSkinWeightsPerVertex))
        {
            return {};
        }
        for (const AZ::Vector3& vertex : configuration.m_scalpVertices)
        {
            if (!vertex.IsFinite())
            {
                return {};
            }
        }
        for (const HairTriangle& triangle : configuration.m_scalpTriangles)
        {
            if (triangle.m_firstVertex >= configuration.m_scalpVertices.size()
                || triangle.m_secondVertex >= configuration.m_scalpVertices.size()
                || triangle.m_thirdVertex >= configuration.m_scalpVertices.size()
                || triangle.m_firstVertex == triangle.m_secondVertex
                || triangle.m_firstVertex == triangle.m_thirdVertex
                || triangle.m_secondVertex == triangle.m_thirdVertex)
            {
                return {};
            }
        }
        for (const AZ::Transform& inverseBindPose : configuration.m_scalpInverseBindPoses)
        {
            if (!inverseBindPose.IsFinite())
            {
                return {};
            }
        }
        for (const HairSkinWeight& skinWeight : configuration.m_scalpSkinWeights)
        {
            if (skinWeight.m_jointIndex >= configuration.m_scalpInverseBindPoses.size()
                || !AZ::IsFiniteFloat(skinWeight.m_weight)
                || skinWeight.m_weight < 0.0f)
            {
                return {};
            }
        }

        JPH::Ref<JPH::HairSettings> settings = new JPH::HairSettings();
        settings->mMaterials.reserve(configuration.m_materials.size());
        for (const HairMaterialConfiguration& material : configuration.m_materials)
        {
            JPH::HairSettings::Material nativeMaterial;
            nativeMaterial.mGlobalPose = ToNativeHairGradient(material.m_globalPose);
            nativeMaterial.mGravityFactor = ToNativeHairGradient(material.m_gravityFactor);
            nativeMaterial.mGridVelocityFactor = ToNativeHairGradient(material.m_gridVelocityFactor);
            nativeMaterial.mHairRadius = ToNativeHairGradient(material.m_radius);
            nativeMaterial.mSkinGlobalPose = ToNativeHairGradient(material.m_skinGlobalPose);
            nativeMaterial.mWorldTransformInfluence = ToNativeHairGradient(material.m_worldTransformInfluence);
            nativeMaterial.mBendComplianceMultiplier = {
                material.m_bendComplianceMultipliers[0],
                material.m_bendComplianceMultipliers[1],
                material.m_bendComplianceMultipliers[2],
                material.m_bendComplianceMultipliers[3],
            };
            nativeMaterial.mAngularDamping = material.m_angularDamping;
            nativeMaterial.mBendCompliance = material.m_bendCompliance;
            nativeMaterial.mFriction = material.m_friction;
            nativeMaterial.mGravityPreloadFactor = material.m_gravityPreloadFactor;
            nativeMaterial.mGridDensityForceFactor = material.m_gridDensityForceFactor;
            nativeMaterial.mInertiaMultiplier = material.m_inertiaMultiplier;
            nativeMaterial.mLinearDamping = material.m_linearDamping;
            nativeMaterial.mMaxAngularVelocity = material.m_maximumAngularVelocity;
            nativeMaterial.mMaxLinearVelocity = material.m_maximumLinearVelocity;
            nativeMaterial.mSimulationStrandsFraction = material.m_simulationStrandFraction;
            nativeMaterial.mStretchCompliance = material.m_stretchCompliance;
            nativeMaterial.mEnableCollision = material.m_enableCollision;
            nativeMaterial.mEnableLRA = material.m_enableLongRangeAttachments;
            settings->mMaterials.push_back(nativeMaterial);
        }

        JPH::Array<JPH::HairSettings::SVertex> vertices;
        vertices.reserve(configuration.m_vertices.size());
        for (const HairVertex& vertex : configuration.m_vertices)
        {
            vertices.emplace_back(
                JPH::Float3(vertex.m_position.GetX(), vertex.m_position.GetY(), vertex.m_position.GetZ()),
                vertex.m_inverseMass);
        }
        JPH::Array<JPH::HairSettings::SStrand> strands;
        strands.reserve(configuration.m_strands.size());
        for (const HairStrand& strand : configuration.m_strands)
        {
            strands.emplace_back(
                strand.m_beginVertex,
                strand.m_endVertex,
                strand.m_materialIndex);
        }
        if (configuration.m_verticesPerStrand >= 2)
        {
            JPH::HairSettings::sResample(vertices, strands, configuration.m_verticesPerStrand);
        }

        settings->mScalpVertices.reserve(configuration.m_scalpVertices.size());
        for (const AZ::Vector3& vertex : configuration.m_scalpVertices)
        {
            settings->mScalpVertices.emplace_back(vertex.GetX(), vertex.GetY(), vertex.GetZ());
        }
        settings->mScalpTriangles.reserve(configuration.m_scalpTriangles.size());
        for (const HairTriangle& triangle : configuration.m_scalpTriangles)
        {
            settings->mScalpTriangles.emplace_back(
                triangle.m_firstVertex,
                triangle.m_secondVertex,
                triangle.m_thirdVertex);
        }
        settings->mScalpInverseBindPose.reserve(configuration.m_scalpInverseBindPoses.size());
        for (const AZ::Transform& inverseBindPose : configuration.m_scalpInverseBindPoses)
        {
            settings->mScalpInverseBindPose.push_back(ToNativeSystemTransform(inverseBindPose));
        }
        settings->mScalpSkinWeights.reserve(configuration.m_scalpSkinWeights.size());
        for (const HairSkinWeight& skinWeight : configuration.m_scalpSkinWeights)
        {
            JPH::HairSettings::SkinWeight nativeSkinWeight;
            nativeSkinWeight.mJointIdx = skinWeight.m_jointIndex;
            nativeSkinWeight.mWeight = skinWeight.m_weight;
            settings->mScalpSkinWeights.push_back(nativeSkinWeight);
        }

        settings->mScalpNumSkinWeightsPerVertex = configuration.m_scalpSkinWeightsPerVertex;
        settings->mNumIterationsPerSecond = configuration.m_iterationsPerSecond;
        settings->mMaxDeltaTime = configuration.m_maximumDeltaTime;
        settings->mGridSize = JPH::UVec4(
            configuration.m_gridSizeX,
            configuration.m_gridSizeY,
            configuration.m_gridSizeZ,
            0);
        settings->mSimulationBoundsPadding = JPH::Vec3(
            configuration.m_simulationBoundsPadding.GetX(),
            configuration.m_simulationBoundsPadding.GetY(),
            configuration.m_simulationBoundsPadding.GetZ());
        settings->mInitialGravity = JPH::Vec3(
            configuration.m_initialGravity.GetX(),
            configuration.m_initialGravity.GetY(),
            configuration.m_initialGravity.GetZ());
        settings->InitRenderAndSimulationStrands(vertices, strands);
        float maximumHairToScalpDistanceSquared = 0.0f;
        settings->Init(maximumHairToScalpDistanceSquared);

        if (!EnsureHairRuntime())
        {
            return {};
        }

        AZStd::lock_guard lock(m_hairDefinitionMutex);
        if (!settings->InitCompute(m_hairRuntime->m_computeSystem))
        {
            AZ_Error("Jolt", false, "Failed to allocate the hair definition compute buffers.");
            return {};
        }

        AZ::u32 definitionIndex = 0;
        if (!m_freeHairDefinitionSlots.empty())
        {
            definitionIndex = m_freeHairDefinitionSlots.back();
            m_freeHairDefinitionSlots.pop_back();
        }
        else
        {
            if (m_hairDefinitionSlots.size() >= Internal::HandlePayloadMask)
            {
                return {};
            }
            definitionIndex = aznumeric_cast<AZ::u32>(m_hairDefinitionSlots.size());
            m_hairDefinitionSlots.emplace_back();
        }

        HairDefinitionSlot& slot = m_hairDefinitionSlots[definitionIndex];
        slot.m_settings = settings;
        slot.m_maximumHairToScalpDistanceSquared = maximumHairToScalpDistanceSquared;
        return Internal::MakeResourceHandle<HairDefinitionHandle>(definitionIndex, slot.m_generation);
    }

    bool RuntimeImplementation::DestroyHairDefinition(
        const HairDefinitionHandle definitionHandle)
    {
        AZStd::lock_guard lock(m_hairDefinitionMutex);
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(definitionHandle, parts)
            || parts.m_index >= m_hairDefinitionSlots.size())
        {
            return false;
        }

        HairDefinitionSlot& slot = m_hairDefinitionSlots[parts.m_index];
        if (!slot.m_settings || slot.m_generation != parts.m_generation || slot.m_instanceCount > 0)
        {
            return false;
        }

        slot.m_settings = nullptr;
        slot.m_maximumHairToScalpDistanceSquared = 0.0f;
        if (Internal::AdvanceGeneration(slot.m_generation))
        {
            m_freeHairDefinitionSlots.push_back(parts.m_index);
        }
        return true;
    }

    bool RuntimeImplementation::IsValid(
        const HairDefinitionHandle definitionHandle) const
    {
        AZStd::shared_lock lock(m_hairDefinitionMutex);
        return FindHairDefinitionUnlocked(definitionHandle);
    }

    bool RuntimeImplementation::GetHairDefinitionState(
        const HairDefinitionHandle definitionHandle,
        HairDefinitionState& state) const
    {
        AZStd::shared_lock lock(m_hairDefinitionMutex);
        const HairDefinitionSlot* slot = FindHairDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return false;
        }

        const JPH::HairSettings& settings = *slot->m_settings;
        const JPH::Vec3 minimum = settings.mSimulationBounds.mMin;
        const JPH::Vec3 maximum = settings.mSimulationBounds.mMax;
        state = {
            .m_simulationBounds = AZ::Aabb::CreateFromMinMax(
                {minimum.GetX(), minimum.GetY(), minimum.GetZ()},
                {maximum.GetX(), maximum.GetY(), maximum.GetZ()}),
            .m_gridCellCount = aznumeric_cast<AZ::u32>(settings.mNeutralDensity.size()),
            .m_jointCount = aznumeric_cast<AZ::u32>(settings.mScalpInverseBindPose.size()),
            .m_maximumVerticesPerStrand = settings.mMaxVerticesPerStrand,
            .m_paddedSimulationVertexCount = settings.GetNumVerticesPadded(),
            .m_renderVertexCount = aznumeric_cast<AZ::u32>(settings.mRenderVertices.size()),
            .m_scalpVertexCount = aznumeric_cast<AZ::u32>(settings.mScalpVertices.size()),
            .m_simulationVertexCount = aznumeric_cast<AZ::u32>(settings.mSimVertices.size()),
            .m_densityScale = settings.mDensityScale,
            .m_maximumHairToScalpDistanceSquared = slot->m_maximumHairToScalpDistanceSquared,
        };
        return true;
    }

    QueryResult RuntimeImplementation::GetHairNeutralDensity(
        const HairDefinitionHandle definitionHandle,
        const AZStd::span<float> density) const
    {
        AZStd::shared_lock lock(m_hairDefinitionMutex);
        const HairDefinitionSlot* slot = FindHairDefinitionUnlocked(definitionHandle);
        if (!slot)
        {
            return {};
        }

        const JPH::Array<float>& nativeDensity = slot->m_settings->mNeutralDensity;
        const size_t hitCount = AZStd::min(density.size(), nativeDensity.size());
        if (hitCount > 0)
        {
            AZStd::copy(nativeDensity.begin(), nativeDensity.begin() + hitCount, density.begin());
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(hitCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(nativeDensity.size()),
        };
    }

    bool RuntimeImplementation::SkinHairScalpVertices(
        const HairDefinitionHandle definitionHandle,
        const AZ::Transform& jointToHair,
        const AZStd::span<const AZ::Transform> jointModelTransforms,
        const AZStd::span<AZ::Transform> preparedJointTransforms,
        const AZStd::span<AZ::Vector3> scalpVertices) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_hairDefinitionMutex);
        const HairDefinitionSlot* slot = FindHairDefinitionUnlocked(definitionHandle);
        if (!slot || !jointToHair.IsFinite())
        {
            return false;
        }

        const JPH::HairSettings& settings = *slot->m_settings;
        const size_t jointCount = settings.mScalpInverseBindPose.size();
        const size_t vertexCount = settings.mScalpVertices.size();
        if (jointModelTransforms.size() != jointCount
            || preparedJointTransforms.size() < jointCount
            || scalpVertices.size() < vertexCount)
        {
            return false;
        }

        for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex)
        {
            if (!jointModelTransforms[jointIndex].IsFinite())
            {
                return false;
            }
        }

        for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex)
        {
            preparedJointTransforms[jointIndex] =
                jointToHair
                * jointModelTransforms[jointIndex]
                * FromNativeTransform(settings.mScalpInverseBindPose[jointIndex]);
        }

        for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
        {
            AZ::Vector3& skinnedVertex = scalpVertices[vertexIndex];
            skinnedVertex = AZ::Vector3::CreateZero();
            const JPH::Float3& nativeVertex = settings.mScalpVertices[vertexIndex];
            const AZ::Vector3 vertex{
                nativeVertex.x,
                nativeVertex.y,
                nativeVertex.z,
            };
            const size_t firstWeightIndex = vertexIndex * settings.mScalpNumSkinWeightsPerVertex;
            const size_t weightEnd = firstWeightIndex + settings.mScalpNumSkinWeightsPerVertex;
            for (size_t weightIndex = firstWeightIndex; weightIndex < weightEnd; ++weightIndex)
            {
                const JPH::HairSettings::SkinWeight& weight = settings.mScalpSkinWeights[weightIndex];
                if (weight.mWeight <= 0.0f)
                {
                    continue;
                }

                skinnedVertex +=
                    preparedJointTransforms[weight.mJointIdx].TransformPoint(vertex) * weight.mWeight;
            }
        }
        return true;
    }

    WorldHandle RuntimeImplementation::CreateWorld(
        const WorldConfiguration& configuration)
    {
        const DeterministicFloatScope floatScope;
        AZStd::lock_guard lock(m_worldMutex);
        AZ::u32 worldIndex = 0;
        bool appendedSlot = false;
        if (!m_freeWorldSlots.empty())
        {
            worldIndex = m_freeWorldSlots.back();
            m_freeWorldSlots.pop_back();
        }
        else
        {
            if (m_worldSlots.size() >= Internal::MaximumWorldCount)
            {
                return {};
            }
            worldIndex = static_cast<AZ::u32>(m_worldSlots.size());
            m_worldSlots.emplace_back();
            appendedSlot = true;
        }

        WorldSlot& slot = m_worldSlots[worldIndex];
        const WorldHandle worldHandle = Internal::MakeWorldHandle(worldIndex, slot.m_generation);
        slot.m_world = AZStd::make_unique<World>(
            *this,
            m_worldMemberGenerationSources[worldIndex],
            worldHandle,
            worldIndex,
            configuration,
            m_jobContext);
        if (!*slot.m_world)
        {
            slot.m_world.reset();
            if (appendedSlot)
            {
                m_worldSlots.pop_back();
            }
            else
            {
                m_freeWorldSlots.push_back(worldIndex);
            }
            return {};
        }

        return worldHandle;
    }

    bool RuntimeImplementation::DestroyWorld(
        const WorldHandle worldHandle)
    {
        AZStd::lock_guard lock(m_worldMutex);
        Internal::WorldHandleParts parts;
        if (!Internal::DecodeWorldHandle(worldHandle, parts)
            || parts.m_index >= m_worldSlots.size())
        {
            return false;
        }

        WorldSlot& slot = m_worldSlots[parts.m_index];
        if (!slot.m_world || slot.m_generation != parts.m_generation)
        {
            return false;
        }

        if (!slot.m_world->IsStateIndeterminate()
            && slot.m_world->HasLiveResources())
        {
            return false;
        }

        const bool hadDebugCapture = slot.m_world->IsDebugCaptureEnabled();
        slot.m_world.reset();
        if (hadDebugCapture)
        {
            [[maybe_unused]] const AZ::u32 previousCaptureWorldCount = m_debugCaptureWorldCount.fetch_sub(1);
            AZ_Assert(previousCaptureWorldCount > 0, "The Jolt debug-capture world count underflowed.");
        }
        if (Internal::AdvanceGeneration(slot.m_generation))
        {
            m_freeWorldSlots.push_back(parts.m_index);
        }
        if (m_defaultWorldHandle == worldHandle)
        {
            m_defaultWorldHandle = {};
        }
        return true;
    }

    WorldHandle RuntimeImplementation::GetDefaultWorldHandle() const
    {
        AZStd::shared_lock lock(m_worldMutex);
        return m_defaultWorldHandle;
    }

    const IWorldQueries* RuntimeImplementation::GetWorldQueries(
        const WorldHandle worldHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        return FindWorldUnlocked(worldHandle);
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        return FindWorldUnlocked(worldHandle);
    }

    bool RuntimeImplementation::GetWorldGravity(
        const WorldHandle worldHandle,
        AZ::Vector3& gravity) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetGravity(gravity);
    }

    bool RuntimeImplementation::SetWorldGravity(
        const WorldHandle worldHandle,
        const AZ::Vector3& gravity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetGravity(gravity);
    }

    bool RuntimeImplementation::GetSimulationConfiguration(
        const WorldHandle worldHandle,
        SimulationConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetSimulationConfiguration(configuration);
    }

    bool RuntimeImplementation::UpdateSimulationConfiguration(
        const WorldHandle worldHandle,
        const SimulationConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateSimulationConfiguration(configuration);
    }

    bool RuntimeImplementation::GetWorldRuntimeConfiguration(
        const WorldHandle worldHandle,
        WorldRuntimeConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetRuntimeConfiguration(configuration);
    }

    bool RuntimeImplementation::UpdateWorldRuntimeConfiguration(
        const WorldHandle worldHandle,
        const WorldRuntimeConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateRuntimeConfiguration(configuration);
    }

    bool RuntimeImplementation::StepWorld(
        const WorldHandle worldHandle,
        const float fixedTimeStep)
    {
        return static_cast<bool>(StepWorldDetailed(worldHandle, fixedTimeStep));
    }

    SimulationResult RuntimeImplementation::StepWorldDetailed(
        const WorldHandle worldHandle,
        const float fixedTimeStep)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {.m_errors = SimulationError::InvalidRequest};
        }

        if (m_debugCaptureWorldCount.load() > 0)
        {
            AZStd::lock_guard rendererLock(GetNativeDebugRendererMutex());
            return world->StepDetailed(fixedTimeStep, m_debugRenderer);
        }
        return world->StepDetailed(fixedTimeStep, nullptr);
    }

    Operation<SimulationResult> RuntimeImplementation::StepWorldAsync(
        const WorldHandle worldHandle,
        const float fixedTimeStep)
    {
        if (!m_operationPool)
        {
            return {};
        }

        struct Work final
        {
            RuntimeImplementation* m_runtime = nullptr;
            WorldHandle m_worldHandle;
            float m_fixedTimeStep = 0.0f;
        };

        return m_operationPool->CreateOperation<SimulationResult>(
            Work{
                .m_runtime = this,
                .m_worldHandle = worldHandle,
                .m_fixedTimeStep = fixedTimeStep,
            },
            [](Work& work, SimulationResult& result)
            {
                result = work.m_runtime->StepWorldDetailed(work.m_worldHandle, work.m_fixedTimeStep);
                return static_cast<bool>(result);
            });
    }

    bool RuntimeImplementation::StepAutoSimulatedWorlds(
        const float elapsedTime)
    {
        return static_cast<bool>(StepAutoSimulatedWorldsDetailedInternal(elapsedTime, {}, nullptr));
    }

    SimulationResult RuntimeImplementation::StepAutoSimulatedWorldsDetailed(
        const float elapsedTime)
    {
        return StepAutoSimulatedWorldsDetailedInternal(elapsedTime, {}, nullptr);
    }

    Operation<AutoSimulationOperationResult> RuntimeImplementation::StepAutoSimulatedWorldsAsync(
        const float elapsedTime)
    {
        if (!m_operationPool)
        {
            return {};
        }

        struct Work final
        {
            RuntimeImplementation* m_runtime = nullptr;
            float m_elapsedTime = 0.0f;
        };

        return m_operationPool->CreateOperation<AutoSimulationOperationResult>(
            Work{
                .m_runtime = this,
                .m_elapsedTime = elapsedTime,
            },
            [](Work& work, AutoSimulationOperationResult& result)
            {
                result.m_simulationResult = work.m_runtime->StepAutoSimulatedWorldsDetailed(
                    work.m_elapsedTime,
                    result.m_eventBatches,
                    result.m_eventBatchCount);
                return static_cast<bool>(result.m_simulationResult);
            });
    }

    SimulationResult RuntimeImplementation::StepAutoSimulatedWorldsDetailed(
        const float elapsedTime,
        AZStd::span<WorldEventBatch, MaximumWorldCount> eventBatches,
        AZ::u32& eventBatchCount)
    {
        eventBatchCount = 0;
        return StepAutoSimulatedWorldsDetailedInternal(elapsedTime, eventBatches, &eventBatchCount);
    }

    SimulationResult RuntimeImplementation::StepAutoSimulatedWorldsDetailedInternal(
        const float elapsedTime,
        AZStd::span<WorldEventBatch> eventBatches,
        AZ::u32* eventBatchCount)
    {
        struct AutoSimulationEntry final
        {
            World* m_world = nullptr;
            WorldHandle m_worldHandle;
            SimulationResult m_result;
        };

        AZStd::shared_lock lock(m_worldMutex);
        AZStd::unique_lock<AZStd::mutex> rendererLock(GetNativeDebugRendererMutex(), AZStd::defer_lock);
        DebugRenderer* debugRenderer = nullptr;
        if (m_debugCaptureWorldCount.load() > 0)
        {
            rendererLock.lock();
            debugRenderer = m_debugRenderer;
        }

        AZStd::fixed_vector<AutoSimulationEntry, Internal::MaximumWorldCount> worlds;
        bool canStepConcurrently = !debugRenderer
            && m_jobContext
            && m_jobContext->GetJobManager().GetNumWorkerThreads() > 0;
        for (AZ::u32 worldIndex = 0; worldIndex < m_worldSlots.size(); ++worldIndex)
        {
            WorldSlot& slot = m_worldSlots[worldIndex];
            if (slot.m_world)
            {
                worlds.push_back({
                    .m_world = slot.m_world.get(),
                    .m_worldHandle = Internal::MakeWorldHandle(worldIndex, slot.m_generation),
                });
                canStepConcurrently = canStepConcurrently
                    && slot.m_world->GetEffectiveWorkerCount() == 1;
            }
        }
        canStepConcurrently = canStepConcurrently && worlds.size() > 1;

        if (canStepConcurrently)
        {
            AZ::JobEmpty completion(false, m_jobContext);
            AZStd::fixed_vector<AZ::Job*, Internal::MaximumWorldCount - 1> jobs;
            for (size_t worldIndex = 1; worldIndex < worlds.size(); ++worldIndex)
            {
                AZ::Job* job = AZ::CreateJobFunction(
                    [&worlds, worldIndex, elapsedTime]
                    {
                        AutoSimulationEntry& entry = worlds[worldIndex];
                        entry.m_result = entry.m_world->StepAutomaticallyDetailed(
                            elapsedTime,
                            nullptr);
                    },
                    true,
                    m_jobContext);
                job->SetDependent(&completion);
                jobs.push_back(job);
            }

            for (AZ::Job* job : jobs)
            {
                job->Start();
            }
            worlds.front().m_result = worlds.front().m_world->StepAutomaticallyDetailed(
                elapsedTime,
                nullptr);
            completion.StartAndWaitForCompletion();
        }
        else
        {
            for (AutoSimulationEntry& entry : worlds)
            {
                entry.m_result = entry.m_world->StepAutomaticallyDetailed(elapsedTime, debugRenderer);
            }
        }

        SimulationResult aggregate;
        for (AutoSimulationEntry& entry : worlds)
        {
            aggregate.m_errors |= entry.m_result.m_errors;
            aggregate.m_stepCount += entry.m_result.m_stepCount;
            aggregate.m_updateNanoseconds += entry.m_result.m_updateNanoseconds;
            if (eventBatchCount && entry.m_result.m_stepCount > 0)
            {
                AZ_Assert(*eventBatchCount < eventBatches.size(), "The automatic-simulation event buffer is too small.");
                eventBatches[*eventBatchCount] = {
                    .m_events = entry.m_world->GetEvents(),
                    .m_worldHandle = entry.m_worldHandle,
                };
                ++*eventBatchCount;
            }
        }
        return aggregate;
    }

    EventBatch RuntimeImplementation::GetEvents(
        const WorldHandle worldHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->GetEvents();
    }

    bool RuntimeImplementation::SetContactCallbacks(
        const WorldHandle worldHandle,
        const ExtensionHandle extensionHandle)
    {
        auto* callbacks = static_cast<IContactCallbacks*>(AcquireExtension(
            extensionHandle,
            ExtensionKind::ContactCallbacks));
        if (extensionHandle && !callbacks)
        {
            return false;
        }

        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            if (extensionHandle)
            {
                ReleaseExtension(extensionHandle);
            }
            return false;
        }
        return world->SetContactCallbacks(extensionHandle, callbacks);
    }

    bool RuntimeImplementation::SetBodyPairCollider(
        const WorldHandle worldHandle,
        const ExtensionHandle extensionHandle)
    {
        auto* collider = static_cast<IBodyPairCollider*>(AcquireExtension(
            extensionHandle,
            ExtensionKind::BodyPairCollider));
        if (extensionHandle && !collider)
        {
            return false;
        }

        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            if (extensionHandle)
            {
                ReleaseExtension(extensionHandle);
            }
            return false;
        }
        return world->SetBodyPairCollider(extensionHandle, collider);
    }

    bool RuntimeImplementation::SetSimulationShapeFilter(
        const WorldHandle worldHandle,
        const ExtensionHandle extensionHandle)
    {
        auto* filter = static_cast<ISimulationShapeFilter*>(AcquireExtension(
            extensionHandle,
            ExtensionKind::SimulationShapeFilter));
        if (extensionHandle && !filter)
        {
            return false;
        }

        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            if (extensionHandle)
            {
                ReleaseExtension(extensionHandle);
            }
            return false;
        }
        return world->SetSimulationShapeFilter(extensionHandle, filter);
    }

    bool RuntimeImplementation::SetSoftBodyContactCallbacks(
        const WorldHandle worldHandle,
        const ExtensionHandle extensionHandle)
    {
        auto* callbacks = static_cast<ISoftBodyContactCallbacks*>(AcquireExtension(
            extensionHandle,
            ExtensionKind::SoftBodyContactCallbacks));
        if (extensionHandle && !callbacks)
        {
            return false;
        }

        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            if (extensionHandle)
            {
                ReleaseExtension(extensionHandle);
            }
            return false;
        }
        return world->SetSoftBodyContactCallbacks(extensionHandle, callbacks);
    }

    bool RuntimeImplementation::AddStepListener(
        const WorldHandle worldHandle,
        const ExtensionHandle extensionHandle)
    {
        auto* listener = static_cast<IStepListener*>(AcquireExtension(
            extensionHandle,
            ExtensionKind::StepListener));
        if (!listener)
        {
            return false;
        }

        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            ReleaseExtension(extensionHandle);
            return false;
        }
        return world->AddStepListener(extensionHandle, listener);
    }

    bool RuntimeImplementation::RemoveStepListener(
        const WorldHandle worldHandle,
        const ExtensionHandle extensionHandle)
    {
        auto* listener = static_cast<IStepListener*>(AcquireExtension(
            extensionHandle,
            ExtensionKind::StepListener));
        if (!listener)
        {
            return false;
        }

        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            ReleaseExtension(extensionHandle);
            return false;
        }
        return world->RemoveStepListener(extensionHandle, listener);
    }

    HairHandle RuntimeImplementation::CreateHair(
        const WorldHandle worldHandle,
        const HairConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->CreateHair(configuration);
    }

    bool RuntimeImplementation::DestroyHair(
        const WorldHandle worldHandle,
        const HairHandle hairHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyHair(hairHandle);
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle,
        const HairHandle hairHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsValid(hairHandle);
    }

    bool RuntimeImplementation::SetHairTransform(
        const WorldHandle worldHandle,
        const HairHandle hairHandle,
        const WorldTransform& worldTransform,
        const bool teleport)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetHairTransform(hairHandle, worldTransform, teleport);
    }

    bool RuntimeImplementation::SetHairScalpToHeadTransform(
        const WorldHandle worldHandle,
        const HairHandle hairHandle,
        const AZ::Transform& scalpToHeadTransform)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetHairScalpToHeadTransform(hairHandle, scalpToHeadTransform);
    }

    bool RuntimeImplementation::UpdateHair(
        const WorldHandle worldHandle,
        const HairHandle hairHandle,
        const float deltaTime,
        const AZ::Transform& jointToHair,
        const AZStd::span<const AZ::Transform> jointModelTransforms)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateHair(
            hairHandle,
            deltaTime,
            jointToHair,
            jointModelTransforms);
    }

    bool RuntimeImplementation::EnableHairAutoUpdate(
        const WorldHandle worldHandle,
        const HairHandle hairHandle,
        const AZ::Transform& jointToHair,
        const AZStd::span<const AZ::Transform> jointModelTransforms)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->EnableHairAutoUpdate(hairHandle, jointToHair, jointModelTransforms);
    }

    bool RuntimeImplementation::DisableHairAutoUpdate(
        const WorldHandle worldHandle,
        const HairHandle hairHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DisableHairAutoUpdate(hairHandle);
    }

    bool RuntimeImplementation::GetHairState(
        const WorldHandle worldHandle,
        const HairHandle hairHandle,
        HairState& state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetHairState(hairHandle, state);
    }

    bool RuntimeImplementation::GetHairReadback(
        const WorldHandle worldHandle,
        const HairHandle hairHandle,
        const HairReadbackBuffers& buffers,
        HairReadbackResult& result) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetHairReadback(hairHandle, buffers, result);
    }

    QueryResult RuntimeImplementation::GetHairVertexStates(
        const WorldHandle worldHandle,
        const HairHandle hairHandle,
        const AZStd::span<HairVertexState> states) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->GetHairVertexStates(hairHandle, states);
    }

    QueryResult RuntimeImplementation::GetHairRenderPositions(
        const WorldHandle worldHandle,
        const HairHandle hairHandle,
        const AZStd::span<AZ::Vector3> positions) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->GetHairRenderPositions(hairHandle, positions);
    }

    QueryResult RuntimeImplementation::GetHairScalpPositions(
        const WorldHandle worldHandle,
        const HairHandle hairHandle,
        const AZStd::span<AZ::Vector3> positions) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->GetHairScalpPositions(hairHandle, positions);
    }

    QueryResult RuntimeImplementation::GetHairGridCellStates(
        const WorldHandle worldHandle,
        const HairHandle hairHandle,
        const AZStd::span<HairGridCellState> states) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->GetHairGridCellStates(hairHandle, states);
    }

    ShapeHandle RuntimeImplementation::CreateShape(
        const WorldHandle worldHandle,
        const ShapeConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateShape(configuration);
        }

        return {};
    }

    ShapeHandle RuntimeImplementation::CreateShape(
        const WorldHandle worldHandle,
        const CompoundShapeConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateShape(configuration);
        }

        return {};
    }

    ShapeHandle RuntimeImplementation::CreateShape(
        const WorldHandle worldHandle,
        const DecoratedShapeConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateShape(configuration);
        }

        return {};
    }

    ShapeHandle RuntimeImplementation::CreateShape(
        const WorldHandle worldHandle,
        const CookedShapeHandle cookedShapeHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateShape(cookedShapeHandle);
        }

        return {};
    }

    ShapeHandle RuntimeImplementation::CloneShape(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CloneShape(shapeHandle);
        }

        return {};
    }

    ShapeHandle RuntimeImplementation::ScaleShape(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::Vector3& scale)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->ScaleShape(shapeHandle, scale);
    }

    bool RuntimeImplementation::DestroyShape(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyShape(shapeHandle);
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsValid(shapeHandle);
    }

    bool RuntimeImplementation::GetShapeStats(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        ShapeStats& stats) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetShapeStats(shapeHandle, stats);
    }

    bool RuntimeImplementation::GetShapeStatsRecursive(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        ShapeStats& stats) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetShapeStatsRecursive(shapeHandle, stats);
    }

    bool RuntimeImplementation::GetShapeProperties(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        ShapeProperties& properties) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetShapeProperties(shapeHandle, properties);
    }

    bool RuntimeImplementation::GetShapeSubmergedVolume(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const SubmergedVolumeRequest& request,
        SubmergedVolumeResult& result) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetShapeSubmergedVolume(shapeHandle, request, result);
    }

    bool RuntimeImplementation::GetPrimitiveShapeState(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        PrimitiveShapeState& state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetPrimitiveShapeState(shapeHandle, state);
    }

    bool RuntimeImplementation::GetConvexHullState(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        ConvexHullState& state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetConvexHullState(shapeHandle, state);
    }

    BufferResult RuntimeImplementation::GetConvexHullPointsRelativeToCenterOfMass(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZStd::span<AZ::Vector3> points) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetConvexHullPointsRelativeToCenterOfMass(shapeHandle, points);
        }
        return {};
    }

    BufferResult RuntimeImplementation::GetConvexHullPlanesRelativeToCenterOfMass(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZStd::span<AZ::Plane> planes) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetConvexHullPlanesRelativeToCenterOfMass(shapeHandle, planes);
        }
        return {};
    }

    BufferResult RuntimeImplementation::GetConvexHullFaceVertexIndices(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::u32 faceIndex,
        const AZStd::span<AZ::u32> vertexIndices) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetConvexHullFaceVertexIndices(
                shapeHandle,
                faceIndex,
                vertexIndices);
        }
        return {};
    }

    bool RuntimeImplementation::GetShapeMaterial(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const SubShapeId subShapeId,
        MaterialHandle& materialHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetShapeMaterial(
                shapeHandle,
                subShapeId,
                materialHandle);
    }

    bool RuntimeImplementation::GetShapeSurfaceNormal(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const SubShapeId subShapeId,
        const AZ::Vector3& localSurfacePosition,
        AZ::Vector3& normal) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetShapeSurfaceNormal(
                shapeHandle,
                subShapeId,
                localSurfacePosition,
                normal);
    }

    bool RuntimeImplementation::GetShapeUserData(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        AZ::u64& userData) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetShapeUserData(shapeHandle, userData);
    }

    bool RuntimeImplementation::GetShapeSubShapeUserData(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const SubShapeId subShapeId,
        AZ::u64& userData) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetShapeSubShapeUserData(
                shapeHandle,
                subShapeId,
                userData);
    }

    bool RuntimeImplementation::GetDirectChildShape(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const SubShapeId subShapeId,
        ShapeHandle& childShapeHandle,
        SubShapeTransform& transform) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetDirectChildShape(
                shapeHandle,
                subShapeId,
                childShapeHandle,
                transform);
    }

    bool RuntimeImplementation::GetDecoratedShapeConfiguration(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        DecoratedShapeConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetDecoratedShapeConfiguration(
                shapeHandle,
                configuration);
    }

    BufferResult RuntimeImplementation::GetMeshMaterials(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZStd::span<MaterialHandle> materialHandles) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetMeshMaterials(shapeHandle, materialHandles);
        }
        return {};
    }

    bool RuntimeImplementation::GetMeshTriangleMaterialIndex(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const SubShapeId subShapeId,
        AZ::u32& materialIndex) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetMeshTriangleMaterialIndex(
                shapeHandle,
                subShapeId,
                materialIndex);
    }

    bool RuntimeImplementation::GetMeshTriangleUserData(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const SubShapeId subShapeId,
        AZ::u32& userData) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetMeshTriangleUserData(
                shapeHandle,
                subShapeId,
                userData);
    }

    bool RuntimeImplementation::IsShapeScaleValid(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::Vector3& scale) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsShapeScaleValid(shapeHandle, scale);
    }

    bool RuntimeImplementation::MakeShapeScaleValid(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::Vector3& scale,
        AZ::Vector3& validScale) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->MakeShapeScaleValid(
                shapeHandle,
                scale,
                validScale);
    }

    bool RuntimeImplementation::GetHeightfieldState(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        HeightfieldState& state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetHeightfieldState(shapeHandle, state);
    }

    bool RuntimeImplementation::GetHeightfieldPosition(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::u32 column,
        const AZ::u32 row,
        AZ::Vector3& position) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetHeightfieldPosition(
                shapeHandle,
                column,
                row,
                position);
    }

    bool RuntimeImplementation::ProjectOntoHeightfield(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::Vector3& localPosition,
        AZ::Vector3& surfacePosition,
        SubShapeId& subShapeId) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->ProjectOntoHeightfield(
                shapeHandle,
                localPosition,
                surfacePosition,
                subShapeId);
    }

    bool RuntimeImplementation::IsHeightfieldNoCollision(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::u32 column,
        const AZ::u32 row,
        bool& noCollision) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->IsHeightfieldNoCollision(
                shapeHandle,
                column,
                row,
                noCollision);
    }

    QueryResult RuntimeImplementation::GetHeightfieldHeights(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const HeightfieldRegion& region,
        const AZStd::span<float> heights) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetHeightfieldHeights(shapeHandle, region, heights);
        }
        return {};
    }

    QueryResult RuntimeImplementation::GetHeightfieldMaterialIndices(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const HeightfieldRegion& region,
        const AZStd::span<AZ::u8> materialIndices) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetHeightfieldMaterialIndices(shapeHandle, region, materialIndices);
        }
        return {};
    }

    QueryResult RuntimeImplementation::GetHeightfieldMaterials(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZStd::span<MaterialHandle> materialHandles) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetHeightfieldMaterials(shapeHandle, materialHandles);
        }
        return {};
    }

    bool RuntimeImplementation::GetHeightfieldSubShapeCoordinates(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const SubShapeId subShapeId,
        HeightfieldSubShapeCoordinates& coordinates) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetHeightfieldSubShapeCoordinates(
                shapeHandle,
                subShapeId,
                coordinates);
    }

    bool RuntimeImplementation::UpdateHeightfieldHeights(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const HeightfieldRegion& region,
        const AZStd::span<const float> heights,
        const HeightfieldUpdateConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateHeightfieldHeights(
                shapeHandle,
                region,
                heights,
                configuration);
    }

    bool RuntimeImplementation::UpdateHeightfieldMaterials(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const HeightfieldRegion& region,
        const AZStd::span<const AZ::u8> materialIndices,
        const AZStd::span<const MaterialHandle> materialHandles,
        const bool activateBodies)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateHeightfieldMaterials(
                shapeHandle,
                region,
                materialIndices,
                materialHandles,
                activateBodies);
    }

    bool RuntimeImplementation::AddMutableCompoundChild(
        const WorldHandle worldHandle,
        const ShapeHandle compoundShapeHandle,
        const CompoundChildConfiguration& child,
        const AZ::u32 insertionIndex,
        AZ::u32& childIndex,
        const MutableCompoundUpdateConfiguration& updateConfiguration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->AddMutableCompoundChild(
                compoundShapeHandle,
                child,
                insertionIndex,
                childIndex,
                updateConfiguration);
    }

    bool RuntimeImplementation::RemoveMutableCompoundChild(
        const WorldHandle worldHandle,
        const ShapeHandle compoundShapeHandle,
        const AZ::u32 childIndex,
        const MutableCompoundUpdateConfiguration& updateConfiguration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->RemoveMutableCompoundChild(
                compoundShapeHandle,
                childIndex,
                updateConfiguration);
    }

    bool RuntimeImplementation::UpdateMutableCompoundChild(
        const WorldHandle worldHandle,
        const ShapeHandle compoundShapeHandle,
        const AZ::u32 childIndex,
        const CompoundChildConfiguration& child,
        const MutableCompoundUpdateConfiguration& updateConfiguration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateMutableCompoundChild(
                compoundShapeHandle,
                childIndex,
                child,
                updateConfiguration);
    }

    bool RuntimeImplementation::UpdateMutableCompoundChildTransforms(
        const WorldHandle worldHandle,
        const ShapeHandle compoundShapeHandle,
        const AZ::u32 startIndex,
        const AZStd::span<const AZ::Vector3> positions,
        const AZStd::span<const AZ::Quaternion> rotations,
        const MutableCompoundUpdateConfiguration& updateConfiguration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateMutableCompoundChildTransforms(
                compoundShapeHandle,
                startIndex,
                positions,
                rotations,
                updateConfiguration);
    }

    bool RuntimeImplementation::AdjustMutableCompoundCenterOfMass(
        const WorldHandle worldHandle,
        const ShapeHandle compoundShapeHandle,
        const bool updateMassProperties,
        const bool activateBodies)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->AdjustMutableCompoundCenterOfMass(
                compoundShapeHandle,
                updateMassProperties,
                activateBodies);
    }

    bool RuntimeImplementation::GetCompoundChildCount(
        const WorldHandle worldHandle,
        const ShapeHandle compoundShapeHandle,
        AZ::u32& childCount) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetCompoundChildCount(compoundShapeHandle, childCount);
    }

    bool RuntimeImplementation::GetCompoundChild(
        const WorldHandle worldHandle,
        const ShapeHandle compoundShapeHandle,
        const AZ::u32 childIndex,
        CompoundChildConfiguration& child) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetCompoundChild(
                compoundShapeHandle,
                childIndex,
                child);
    }

    bool RuntimeImplementation::GetCompoundChildIndex(
        const WorldHandle worldHandle,
        const ShapeHandle compoundShapeHandle,
        const SubShapeId subShapeId,
        AZ::u32& childIndex) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetCompoundChildIndex(
                compoundShapeHandle,
                subShapeId,
                childIndex);
    }

    BodyHandle RuntimeImplementation::CreateBody(
        const WorldHandle worldHandle,
        const BodyConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateBody(configuration);
        }

        return {};
    }

    BodyHandle RuntimeImplementation::CreateBodyWithId(
        const WorldHandle worldHandle,
        const BodyId bodyId,
        const BodyConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateBodyWithId(bodyId, configuration);
        }

        return {};
    }

    BodyHandle RuntimeImplementation::CreateSoftBody(
        const WorldHandle worldHandle,
        const SoftBodyConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateSoftBody(configuration);
        }

        return {};
    }

    BodyHandle RuntimeImplementation::CreateSoftBodyWithId(
        const WorldHandle worldHandle,
        const BodyId bodyId,
        const SoftBodyConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateSoftBodyWithId(bodyId, configuration);
        }

        return {};
    }

    bool RuntimeImplementation::AddBodyToSimulation(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddBodyToSimulation(bodyHandle, activate);
    }

    bool RuntimeImplementation::AddBodiesToSimulation(
        const WorldHandle worldHandle,
        const AZStd::span<const BodyHandle> bodyHandles,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddBodiesToSimulation(bodyHandles, activate);
    }

    bool RuntimeImplementation::RemoveBodyFromSimulation(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->RemoveBodyFromSimulation(bodyHandle);
    }

    bool RuntimeImplementation::RemoveBodiesFromSimulation(
        const WorldHandle worldHandle,
        const AZStd::span<const BodyHandle> bodyHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->RemoveBodiesFromSimulation(bodyHandles);
    }

    bool RuntimeImplementation::DestroyBody(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyBody(bodyHandle);
    }

    bool RuntimeImplementation::DestroyBodies(
        const WorldHandle worldHandle,
        const AZStd::span<const BodyHandle> bodyHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyBodies(bodyHandles);
    }

    bool RuntimeImplementation::IsBodyInSimulation(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsBodyInSimulation(bodyHandle);
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsValid(bodyHandle);
    }

    bool RuntimeImplementation::SetBodyMoveEventsEnabled(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool enabled)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyMoveEventsEnabled(bodyHandle, enabled);
    }

    RagdollDefinitionHandle RuntimeImplementation::CreateRagdollDefinition(
        const WorldHandle worldHandle,
        const RagdollDefinitionConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateRagdollDefinition(configuration);
        }

        return {};
    }

    bool RuntimeImplementation::DestroyRagdollDefinition(
        const WorldHandle worldHandle,
        const RagdollDefinitionHandle definitionHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyRagdollDefinition(definitionHandle);
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle,
        const RagdollDefinitionHandle definitionHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsValid(definitionHandle);
    }

    QueryResult RuntimeImplementation::GetRagdollBodyConstraintIndices(
        const WorldHandle worldHandle,
        const RagdollDefinitionHandle definitionHandle,
        const AZStd::span<AZ::s32> constraintIndices) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetRagdollBodyConstraintIndices(definitionHandle, constraintIndices);
        }

        return {};
    }

    QueryResult RuntimeImplementation::GetRagdollConstraintBodyPairs(
        const WorldHandle worldHandle,
        const RagdollDefinitionHandle definitionHandle,
        const AZStd::span<RagdollConstraintBodyPair> bodyPairs) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetRagdollConstraintBodyPairs(definitionHandle, bodyPairs);
        }

        return {};
    }

    RagdollHandle RuntimeImplementation::CreateRagdoll(
        const WorldHandle worldHandle,
        const RagdollConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateRagdoll(configuration);
        }

        return {};
    }

    bool RuntimeImplementation::AddRagdollToSimulation(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddRagdollToSimulation(ragdollHandle, activate);
    }

    bool RuntimeImplementation::RemoveRagdollFromSimulation(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->RemoveRagdollFromSimulation(ragdollHandle);
    }

    bool RuntimeImplementation::DestroyRagdoll(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyRagdoll(ragdollHandle);
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsValid(ragdollHandle);
    }

    bool RuntimeImplementation::IsRagdollInSimulation(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsRagdollInSimulation(ragdollHandle);
    }

    bool RuntimeImplementation::GetRagdollState(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        RagdollState& state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetRagdollState(ragdollHandle, state);
    }

    bool RuntimeImplementation::SetRagdollCollisionGroupId(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const AZ::u32 collisionGroupId)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetRagdollCollisionGroupId(ragdollHandle, collisionGroupId);
    }

    QueryResult RuntimeImplementation::GetRagdollBodies(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const AZStd::span<BodyHandle> bodyHandles) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetRagdollBodies(ragdollHandle, bodyHandles);
        }

        return {};
    }

    QueryResult RuntimeImplementation::GetRagdollConstraints(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const AZStd::span<ConstraintHandle> constraintHandles) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetRagdollConstraints(ragdollHandle, constraintHandles);
        }

        return {};
    }

    bool RuntimeImplementation::ActivateRagdoll(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ActivateRagdoll(ragdollHandle);
    }

    bool RuntimeImplementation::SetRagdollPose(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const WorldPosition rootPosition,
        const AZStd::span<const AZ::Transform> modelTransforms)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetRagdollPose(ragdollHandle, rootPosition, modelTransforms);
    }

    QueryResult RuntimeImplementation::GetRagdollPose(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        WorldPosition& rootPosition,
        const AZStd::span<AZ::Transform> modelTransforms) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetRagdollPose(ragdollHandle, rootPosition, modelTransforms);
        }

        return {};
    }

    bool RuntimeImplementation::DriveRagdollKinematically(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const WorldPosition rootPosition,
        const AZStd::span<const AZ::Transform> modelTransforms,
        const float deltaTime)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->DriveRagdollKinematically(
                ragdollHandle,
                rootPosition,
                modelTransforms,
                deltaTime);
    }

    bool RuntimeImplementation::DriveRagdollMotors(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const AZStd::span<const AZ::Transform> modelTransforms)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DriveRagdollMotors(ragdollHandle, modelTransforms);
    }

    bool RuntimeImplementation::DriveRagdollMotors(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const AZStd::span<const AZ::Transform> previousModelTransforms,
        const AZStd::span<const AZ::Transform> modelTransforms,
        const float deltaTime)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->DriveRagdollMotors(
                ragdollHandle,
                previousModelTransforms,
                modelTransforms,
                deltaTime);
    }

    bool RuntimeImplementation::ResetRagdollWarmStart(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ResetRagdollWarmStart(ragdollHandle);
    }

    bool RuntimeImplementation::SetRagdollVelocity(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const AZ::Vector3 linearVelocity,
        const AZ::Vector3 angularVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->SetRagdollVelocity(
                ragdollHandle,
                linearVelocity,
                angularVelocity);
    }

    bool RuntimeImplementation::SetRagdollLinearVelocity(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const AZ::Vector3 linearVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetRagdollLinearVelocity(ragdollHandle, linearVelocity);
    }

    bool RuntimeImplementation::AddRagdollLinearVelocity(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const AZ::Vector3 linearVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddRagdollLinearVelocity(ragdollHandle, linearVelocity);
    }

    bool RuntimeImplementation::AddRagdollImpulse(
        const WorldHandle worldHandle,
        const RagdollHandle ragdollHandle,
        const AZ::Vector3 impulse)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddRagdollImpulse(ragdollHandle, impulse);
    }

    ConstraintHandle RuntimeImplementation::CreateConstraint(
        const WorldHandle worldHandle,
        const ConstraintConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->CreateConstraint(configuration);
    }

    bool RuntimeImplementation::AddConstraintToSimulation(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddConstraintToSimulation(constraintHandle);
    }

    bool RuntimeImplementation::AddConstraintsToSimulation(
        const WorldHandle worldHandle,
        const AZStd::span<const ConstraintHandle> constraintHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddConstraintsToSimulation(constraintHandles);
    }

    bool RuntimeImplementation::RemoveConstraintFromSimulation(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->RemoveConstraintFromSimulation(constraintHandle);
    }

    bool RuntimeImplementation::RemoveConstraintsFromSimulation(
        const WorldHandle worldHandle,
        const AZStd::span<const ConstraintHandle> constraintHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->RemoveConstraintsFromSimulation(constraintHandles);
    }

    bool RuntimeImplementation::DestroyConstraint(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyConstraint(constraintHandle);
    }

    bool RuntimeImplementation::DestroyConstraints(
        const WorldHandle worldHandle,
        const AZStd::span<const ConstraintHandle> constraintHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyConstraints(constraintHandles);
    }

    bool RuntimeImplementation::IsConstraintInSimulation(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsConstraintInSimulation(constraintHandle);
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsValid(constraintHandle);
    }

    bool RuntimeImplementation::SetConstraintEnabled(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const bool enabled)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetConstraintEnabled(constraintHandle, enabled);
    }

    bool RuntimeImplementation::GetConstraintState(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        ConstraintState& state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetConstraintState(constraintHandle, state);
    }

    bool RuntimeImplementation::GetConstraintConfiguration(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        ConstraintConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetConstraintConfiguration(constraintHandle, configuration);
    }

    bool RuntimeImplementation::GetConstraintUserData(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        AZ::u64& userData) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetConstraintUserData(constraintHandle, userData);
    }

    bool RuntimeImplementation::SetConstraintUserData(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const AZ::u64 userData)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetConstraintUserData(constraintHandle, userData);
    }

    bool RuntimeImplementation::GetConstraintDebugDrawSize(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        float& debugDrawSize) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetConstraintDebugDrawSize(constraintHandle, debugDrawSize);
    }

    bool RuntimeImplementation::SetConstraintDebugDrawSize(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const float debugDrawSize)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetConstraintDebugDrawSize(constraintHandle, debugDrawSize);
    }

    bool RuntimeImplementation::GetConstraintMeasurements(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        ConstraintMeasurements& measurements) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetConstraintMeasurements(constraintHandle, measurements);
    }

    bool RuntimeImplementation::GetCustomConstraintInfo(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        CustomConstraintInfo& info) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetCustomConstraintInfo(constraintHandle, info);
    }

    BufferResult RuntimeImplementation::GetCustomConstraintImpulses(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const AZStd::span<float> impulses) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->GetCustomConstraintImpulses(constraintHandle, impulses);
    }

    BufferResult RuntimeImplementation::GetCustomConstraintState(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const AZStd::span<AZ::u8> state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->GetCustomConstraintState(constraintHandle, state);
    }

    bool RuntimeImplementation::SetCustomConstraintState(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const AZStd::span<const AZ::u8> state)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetCustomConstraintState(constraintHandle, state);
    }

    bool RuntimeImplementation::ResetConstraintWarmStart(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ResetConstraintWarmStart(constraintHandle);
    }

    bool RuntimeImplementation::UpdateConstraintSolverConfiguration(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const ConstraintSolverConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateConstraintSolverConfiguration(
                constraintHandle,
                configuration);
    }

    bool RuntimeImplementation::UpdateConeLimit(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const float halfConeAngle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateConeLimit(constraintHandle, halfConeAngle);
    }

    bool RuntimeImplementation::UpdateDistanceLimits(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const float minimumDistance,
        const float maximumDistance,
        const SpringConfiguration& spring)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateDistanceLimits(
                constraintHandle,
                minimumDistance,
                maximumDistance,
                spring);
    }

    bool RuntimeImplementation::UpdateHingeLimits(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const float minimumAngle,
        const float maximumAngle,
        const SpringConfiguration& spring,
        const float maximumFrictionTorque)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateHingeLimits(
                constraintHandle,
                minimumAngle,
                maximumAngle,
                spring,
                maximumFrictionTorque);
    }

    bool RuntimeImplementation::UpdateHingeMotor(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const MotorConfiguration& motor,
        const float targetAngle,
        const float targetAngularVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateHingeMotor(
                constraintHandle,
                motor,
                targetAngle,
                targetAngularVelocity);
    }

    bool RuntimeImplementation::SetHingeTargetOrientation(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const AZ::Quaternion& targetOrientation)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->SetHingeTargetOrientation(
                constraintHandle,
                targetOrientation);
    }

    bool RuntimeImplementation::UpdatePathMotor(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const MotorConfiguration& motor,
        const float targetPathFraction,
        const float targetVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdatePathMotor(
                constraintHandle,
                motor,
                targetPathFraction,
                targetVelocity);
    }

    bool RuntimeImplementation::UpdatePathProperties(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const PathHandle pathHandle,
        const float pathFraction,
        const float maximumFrictionForce)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdatePathProperties(
                constraintHandle,
                pathHandle,
                pathFraction,
                maximumFrictionForce);
    }

    bool RuntimeImplementation::UpdatePointAnchors(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const ConstraintSpace space,
        const WorldPosition& firstPoint,
        const WorldPosition& secondPoint)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdatePointAnchors(
                constraintHandle,
                space,
                firstPoint,
                secondPoint);
    }

    bool RuntimeImplementation::UpdatePulleyLimits(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const float minimumLength,
        const float maximumLength)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdatePulleyLimits(
                constraintHandle,
                minimumLength,
                maximumLength);
    }

    bool RuntimeImplementation::UpdateSixDofLimits(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const AZStd::span<const SixDofAxisLimitConfiguration> axes)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateSixDofLimits(constraintHandle, axes);
    }

    bool RuntimeImplementation::UpdateSixDofMotors(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const AZStd::span<const MotorConfiguration> motors,
        const AZ::Vector3& targetAngularVelocity,
        const AZ::Quaternion& targetOrientation,
        const AZ::Vector3& targetPosition,
        const AZ::Vector3& targetVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateSixDofMotors(
                constraintHandle,
                motors,
                targetAngularVelocity,
                targetOrientation,
                targetPosition,
                targetVelocity);
    }

    bool RuntimeImplementation::UpdateSliderMotor(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const MotorConfiguration& motor,
        const float targetPosition,
        const float targetVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateSliderMotor(
                constraintHandle,
                motor,
                targetPosition,
                targetVelocity);
    }

    bool RuntimeImplementation::UpdateSliderLimits(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const float minimumPosition,
        const float maximumPosition,
        const SpringConfiguration& spring,
        const float maximumFrictionForce)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateSliderLimits(
                constraintHandle,
                minimumPosition,
                maximumPosition,
                spring,
                maximumFrictionForce);
    }

    bool RuntimeImplementation::UpdateSwingTwistMotors(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const MotorConfiguration& swingMotor,
        const MotorConfiguration& twistMotor,
        const AZ::Vector3& targetAngularVelocity,
        const AZ::Quaternion& targetOrientation)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateSwingTwistMotors(
                constraintHandle,
                swingMotor,
                twistMotor,
                targetAngularVelocity,
                targetOrientation);
    }

    bool RuntimeImplementation::UpdateSwingTwistLimits(
        const WorldHandle worldHandle,
        const ConstraintHandle constraintHandle,
        const float normalHalfConeAngle,
        const float planeHalfConeAngle,
        const float twistMinimumAngle,
        const float twistMaximumAngle,
        const float maximumFrictionTorque)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateSwingTwistLimits(
                constraintHandle,
                normalHalfConeAngle,
                planeHalfConeAngle,
                twistMinimumAngle,
                twistMaximumAngle,
                maximumFrictionTorque);
    }

    bool RuntimeImplementation::GetBodyState(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        BodyState& state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyState(bodyHandle, state);
    }

    bool RuntimeImplementation::GetBodyCenterOfMassTransform(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        WorldTransform& transform) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyCenterOfMassTransform(bodyHandle, transform);
    }

    bool RuntimeImplementation::GetBodyConfiguration(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        BodyConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyConfiguration(bodyHandle, configuration);
    }

    bool RuntimeImplementation::GetBodyUserData(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZ::u64& userData) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyUserData(bodyHandle, userData);
    }

    bool RuntimeImplementation::SetBodyUserData(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::u64 userData)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyUserData(bodyHandle, userData);
    }

    bool RuntimeImplementation::GetBodyRuntimeConfiguration(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        BodyRuntimeConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyRuntimeConfiguration(bodyHandle, configuration);
    }

    bool RuntimeImplementation::GetBodySimulationStatistics(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        BodySimulationStatistics& statistics) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodySimulationStatistics(bodyHandle, statistics);
    }

    bool RuntimeImplementation::ApplyBodyConfiguration(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const BodyConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ApplyBodyConfiguration(bodyHandle, configuration);
    }

    QueryResult RuntimeImplementation::GetSoftBodyVertices(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZStd::span<SoftBodyVertex> vertices) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetSoftBodyVertices(bodyHandle, vertices);
        }
        return {};
    }

    QueryResult RuntimeImplementation::GetSoftBodyFaces(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZStd::span<SoftBodyFace> faces) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetSoftBodyFaces(bodyHandle, faces);
        }
        return {};
    }

    bool RuntimeImplementation::GetSoftBodyLocalBounds(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZ::Aabb& bounds) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetSoftBodyLocalBounds(bodyHandle, bounds);
    }

    QueryResult RuntimeImplementation::GetSoftBodyMaterials(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZStd::span<MaterialHandle> materials) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetSoftBodyMaterials(bodyHandle, materials);
        }
        return {};
    }

    QueryResult RuntimeImplementation::GetSoftBodyRodStates(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZStd::span<SoftBodyRodState> rods) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetSoftBodyRodStates(bodyHandle, rods);
        }
        return {};
    }

    bool RuntimeImplementation::GetSoftBodyRuntimeConfiguration(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        SoftBodyRuntimeConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetSoftBodyRuntimeConfiguration(bodyHandle, configuration);
    }

    bool RuntimeImplementation::ApplySoftBodyConfiguration(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const SoftBodyConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ApplySoftBodyConfiguration(bodyHandle, configuration);
    }

    bool RuntimeImplementation::GetSoftBodyVolume(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        float& volume) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetSoftBodyVolume(bodyHandle, volume);
    }

    bool RuntimeImplementation::RecalculateSoftBodyMassProperties(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->RecalculateSoftBodyMassProperties(bodyHandle, activate);
    }

    bool RuntimeImplementation::SkinSoftBody(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZStd::span<const AZ::Transform> jointTransformsRelativeToCenterOfMass,
        const bool hardSkinAll)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->SkinSoftBody(
                bodyHandle,
                jointTransformsRelativeToCenterOfMass,
                hardSkinAll);
    }

    bool RuntimeImplementation::UpdateSoftBodyManually(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const float deltaTime)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateSoftBodyManually(bodyHandle, deltaTime);
    }

    bool RuntimeImplementation::UpdateSoftBodyRuntimeConfiguration(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const SoftBodyRuntimeConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateSoftBodyRuntimeConfiguration(bodyHandle, configuration);
    }

    bool RuntimeImplementation::SetSoftBodyVertexInverseMass(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::u32 vertexIndex,
        const float inverseMass)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetSoftBodyVertexInverseMass(bodyHandle, vertexIndex, inverseMass);
    }

    bool RuntimeImplementation::SetSoftBodyVertexInverseMasses(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::u32 startVertexIndex,
        const AZStd::span<const float> inverseMasses)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->SetSoftBodyVertexInverseMasses(
                bodyHandle,
                startVertexIndex,
                inverseMasses);
    }

    bool RuntimeImplementation::SetSoftBodyVertexVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::u32 vertexIndex,
        const AZ::Vector3& velocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetSoftBodyVertexVelocity(bodyHandle, vertexIndex, velocity);
    }

    bool RuntimeImplementation::SetSoftBodyVertexVelocities(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::u32 startVertexIndex,
        const AZStd::span<const AZ::Vector3> velocities)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->SetSoftBodyVertexVelocities(
                bodyHandle,
                startVertexIndex,
                velocities);
    }

    VirtualCharacterHandle RuntimeImplementation::CreateVirtualCharacter(
        const WorldHandle worldHandle,
        const VirtualCharacterConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateVirtualCharacter(configuration);
        }
        return {};
    }

    bool RuntimeImplementation::DestroyVirtualCharacter(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyVirtualCharacter(characterHandle);
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsValid(characterHandle);
    }

    bool RuntimeImplementation::GetVirtualCharacterState(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        VirtualCharacterState& state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetVirtualCharacterState(characterHandle, state);
    }

    bool RuntimeImplementation::GetVirtualCharacterUserData(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        AZ::u64& userData) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetVirtualCharacterUserData(characterHandle, userData);
    }

    bool RuntimeImplementation::SetVirtualCharacterUserData(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const AZ::u64 userData)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetVirtualCharacterUserData(characterHandle, userData);
    }

    bool RuntimeImplementation::GetVirtualCharacterRuntimeConfiguration(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        VirtualCharacterRuntimeConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetVirtualCharacterRuntimeConfiguration(characterHandle, configuration);
    }

    QueryResult RuntimeImplementation::CheckVirtualCharacterCollision(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const CharacterCollisionRequest& request,
        const AZStd::span<CharacterCollisionHit> hits,
        const ICharacterCollisionFilter* filter) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CheckVirtualCharacterCollision(
                characterHandle,
                request,
                hits,
                filter);
        }

        return {};
    }

    bool RuntimeImplementation::UpdateVirtualCharacterRuntimeConfiguration(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const VirtualCharacterRuntimeConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateVirtualCharacterRuntimeConfiguration(characterHandle, configuration);
    }

    bool RuntimeImplementation::SetVirtualCharacterShape(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const ShapeHandle shapeHandle,
        const float maximumPenetrationDepth)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->SetVirtualCharacterShape(characterHandle, shapeHandle, maximumPenetrationDepth);
    }

    bool RuntimeImplementation::SetVirtualCharacterInnerBodyShape(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const ShapeHandle shapeHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->SetVirtualCharacterInnerBodyShape(
                characterHandle,
                shapeHandle);
    }

    bool RuntimeImplementation::SetVirtualCharacterTransform(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const WorldTransform& transform)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetVirtualCharacterTransform(characterHandle, transform);
    }

    bool RuntimeImplementation::SetVirtualCharacterVelocity(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const AZ::Vector3& velocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetVirtualCharacterVelocity(characterHandle, velocity);
    }

    bool RuntimeImplementation::CancelVirtualCharacterVelocityTowardsSteepSlopes(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const AZ::Vector3& desiredVelocity,
        AZ::Vector3& adjustedVelocity) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->CancelVirtualCharacterVelocityTowardsSteepSlopes(
                characterHandle,
                desiredVelocity,
                adjustedVelocity);
    }

    bool RuntimeImplementation::BeginVirtualCharacterContactTracking(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->BeginVirtualCharacterContactTracking(characterHandle);
    }

    bool RuntimeImplementation::EndVirtualCharacterContactTracking(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->EndVirtualCharacterContactTracking(characterHandle);
    }

    bool RuntimeImplementation::SetVirtualCharacterContactCallbacks(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const ExtensionHandle extensionHandle)
    {
        auto* callbacks = static_cast<IVirtualCharacterContactCallbacks*>(AcquireExtension(
            extensionHandle,
            ExtensionKind::VirtualCharacterContactCallbacks));
        if (extensionHandle && !callbacks)
        {
            return false;
        }

        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            if (extensionHandle)
            {
                ReleaseExtension(extensionHandle);
            }
            return false;
        }
        return world->SetVirtualCharacterContactCallbacks(characterHandle, extensionHandle, callbacks);
    }

    bool RuntimeImplementation::CanVirtualCharacterWalkStairs(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const AZ::Vector3& desiredVelocity) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->CanVirtualCharacterWalkStairs(characterHandle, desiredVelocity);
    }

    bool RuntimeImplementation::WalkVirtualCharacterStairs(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const VirtualCharacterStairConfiguration& configuration,
        const IQueryFilter* filter)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return false;
        }

        if (m_debugCaptureWorldCount.load() > 0)
        {
            AZStd::lock_guard rendererLock(GetNativeDebugRendererMutex());
            return world->WalkVirtualCharacterStairs(
                characterHandle,
                configuration,
                filter,
                m_debugRenderer);
        }
        return world->WalkVirtualCharacterStairs(
            characterHandle,
            configuration,
            filter,
            nullptr);
    }

    bool RuntimeImplementation::StickVirtualCharacterToFloor(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const AZ::Vector3& stepDown,
        const IQueryFilter* filter)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return false;
        }

        if (m_debugCaptureWorldCount.load() > 0)
        {
            AZStd::lock_guard rendererLock(GetNativeDebugRendererMutex());
            return world->StickVirtualCharacterToFloor(
                characterHandle,
                stepDown,
                filter,
                m_debugRenderer);
        }
        return world->StickVirtualCharacterToFloor(
            characterHandle,
            stepDown,
            filter,
            nullptr);
    }

    bool RuntimeImplementation::RefreshVirtualCharacterContacts(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const IQueryFilter* filter)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->RefreshVirtualCharacterContacts(characterHandle, filter);
    }

    bool RuntimeImplementation::UpdateVirtualCharacterGroundVelocity(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateVirtualCharacterGroundVelocity(characterHandle);
    }

    QueryResult RuntimeImplementation::GetVirtualCharacterContacts(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const AZStd::span<VirtualCharacterContact> contacts) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetVirtualCharacterContacts(characterHandle, contacts);
        }
        return {};
    }

    bool RuntimeImplementation::HasVirtualCharacterCollidedWith(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const BodyHandle bodyHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->HasVirtualCharacterCollidedWith(characterHandle, bodyHandle);
    }

    bool RuntimeImplementation::HaveVirtualCharactersCollided(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle firstCharacterHandle,
        const VirtualCharacterHandle secondCharacterHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->HaveVirtualCharactersCollided(
                firstCharacterHandle,
                secondCharacterHandle);
    }

    bool RuntimeImplementation::UpdateVirtualCharacter(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const float deltaTime,
        const VirtualCharacterUpdateConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return false;
        }

        if (m_debugCaptureWorldCount.load() > 0)
        {
            AZStd::lock_guard rendererLock(GetNativeDebugRendererMutex());
            return world->UpdateVirtualCharacter(
                characterHandle,
                deltaTime,
                configuration,
                m_debugRenderer);
        }
        return world->UpdateVirtualCharacter(
            characterHandle,
            deltaTime,
            configuration,
            nullptr);
    }

    bool RuntimeImplementation::EnableVirtualCharacterAutoUpdate(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle,
        const VirtualCharacterUpdateConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->EnableVirtualCharacterAutoUpdate(characterHandle, configuration);
    }

    bool RuntimeImplementation::DisableVirtualCharacterAutoUpdate(
        const WorldHandle worldHandle,
        const VirtualCharacterHandle characterHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DisableVirtualCharacterAutoUpdate(characterHandle);
    }

    CharacterHandle RuntimeImplementation::CreateCharacter(
        const WorldHandle worldHandle,
        const CharacterConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateCharacter(configuration);
        }
        return {};
    }

    bool RuntimeImplementation::DestroyCharacter(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyCharacter(characterHandle);
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsValid(characterHandle);
    }

    bool RuntimeImplementation::GetCharacterState(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        CharacterState& state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetCharacterState(characterHandle, state);
    }

    bool RuntimeImplementation::GetCharacterUserData(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        AZ::u64& userData) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetCharacterUserData(characterHandle, userData);
    }

    bool RuntimeImplementation::SetCharacterUserData(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        const AZ::u64 userData)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetCharacterUserData(characterHandle, userData);
    }

    bool RuntimeImplementation::GetCharacterRuntimeConfiguration(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        CharacterRuntimeConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetCharacterRuntimeConfiguration(
                characterHandle,
                configuration);
    }

    QueryResult RuntimeImplementation::CheckCharacterCollision(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        const CharacterCollisionRequest& request,
        const AZStd::span<CharacterCollisionHit> hits,
        const ICharacterCollisionFilter* filter) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CheckCharacterCollision(
                characterHandle,
                request,
                hits,
                filter);
        }

        return {};
    }

    bool RuntimeImplementation::UpdateCharacterRuntimeConfiguration(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        const CharacterRuntimeConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateCharacterRuntimeConfiguration(
                characterHandle,
                configuration);
    }

    bool RuntimeImplementation::SetCharacterShape(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        const ShapeHandle shapeHandle,
        const float maximumPenetrationDepth)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetCharacterShape(characterHandle, shapeHandle, maximumPenetrationDepth);
    }

    bool RuntimeImplementation::SetCharacterTransform(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        const WorldTransform& transform,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetCharacterTransform(characterHandle, transform, activate);
    }

    bool RuntimeImplementation::SetCharacterVelocity(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        const AZ::Vector3& velocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetCharacterVelocity(characterHandle, velocity);
    }

    bool RuntimeImplementation::AddCharacterImpulse(
        const WorldHandle worldHandle,
        const CharacterHandle characterHandle,
        const AZ::Vector3& impulse)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddCharacterImpulse(characterHandle, impulse);
    }

    bool RuntimeImplementation::ApplyVehicleEngineDamping(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const float deltaTime)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ApplyVehicleEngineDamping(vehicleHandle, deltaTime);
    }

    bool RuntimeImplementation::ApplyVehicleEngineTorque(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const float torque,
        const float deltaTime)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ApplyVehicleEngineTorque(vehicleHandle, torque, deltaTime);
    }

    bool RuntimeImplementation::CalculateVehicleEngineTorque(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const float acceleration,
        float& torque) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->CalculateVehicleEngineTorque(vehicleHandle, acceleration, torque);
    }

    VehicleHandle RuntimeImplementation::CreateWheeledVehicle(
        const WorldHandle worldHandle,
        const WheeledVehicleConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateWheeledVehicle(configuration);
        }
        return {};
    }

    VehicleHandle RuntimeImplementation::CreateMotorcycle(
        const WorldHandle worldHandle,
        const MotorcycleConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateMotorcycle(configuration);
        }

        return {};
    }

    VehicleHandle RuntimeImplementation::CreateTrackedVehicle(
        const WorldHandle worldHandle,
        const TrackedVehicleConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CreateTrackedVehicle(configuration);
        }

        return {};
    }

    bool RuntimeImplementation::DestroyVehicle(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyVehicle(vehicleHandle);
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsValid(vehicleHandle);
    }

    QueryResult RuntimeImplementation::GetWheeledVehicleState(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        WheeledVehicleState& state,
        AZStd::span<WheelState> wheels) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetWheeledVehicleState(vehicleHandle, state, wheels);
        }
        return {};
    }

    QueryResult RuntimeImplementation::GetMotorcycleState(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        MotorcycleState& state,
        const AZStd::span<WheelState> wheels) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetMotorcycleState(vehicleHandle, state, wheels);
        }

        return {};
    }

    QueryResult RuntimeImplementation::GetTrackedVehicleState(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        TrackedVehicleState& state,
        const AZStd::span<WheelState> wheels) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetTrackedVehicleState(vehicleHandle, state, wheels);
        }

        return {};
    }

    bool RuntimeImplementation::GetVehicleRuntimeConfiguration(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        VehicleRuntimeConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetVehicleRuntimeConfiguration(
                vehicleHandle,
                configuration);
    }

    bool RuntimeImplementation::GetVehicleCollisionConfiguration(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        VehicleCollisionConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetVehicleCollisionConfiguration(
                vehicleHandle,
                configuration);
    }

    bool RuntimeImplementation::GetVehicleEngineConfiguration(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        VehicleEngineConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetVehicleEngineConfiguration(
                vehicleHandle,
                configuration);
    }

    bool RuntimeImplementation::GetVehicleDifferentialLimitedSlipRatio(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        float& ratio) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetVehicleDifferentialLimitedSlipRatio(vehicleHandle, ratio);
    }

    bool RuntimeImplementation::GetVehiclePowertrainState(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        VehiclePowertrainState& state) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetVehiclePowertrainState(
                vehicleHandle,
                state);
    }

    bool RuntimeImplementation::GetVehicleTransmissionConfiguration(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        VehicleTransmissionConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetVehicleTransmissionConfiguration(
                vehicleHandle,
                configuration);
    }

    bool RuntimeImplementation::GetVehicleTrackConfiguration(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const AZ::u32 trackIndex,
        VehicleTrackConfiguration& configuration) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetVehicleTrackConfiguration(
                vehicleHandle,
                trackIndex,
                configuration);
    }

    bool RuntimeImplementation::GetWheelLocalBasis(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const AZ::u32 wheelIndex,
        WheelBasis& basis) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetWheelLocalBasis(vehicleHandle, wheelIndex, basis);
    }

    bool RuntimeImplementation::GetWheelLocalTransform(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp,
        AZ::Transform& transform) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetWheelLocalTransform(
                vehicleHandle,
                wheelIndex,
                wheelRight,
                wheelUp,
                transform);
    }

    bool RuntimeImplementation::GetWheelWorldTransform(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const AZ::u32 wheelIndex,
        const AZ::Vector3& wheelRight,
        const AZ::Vector3& wheelUp,
        WorldTransform& transform) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetWheelWorldTransform(
                vehicleHandle,
                wheelIndex,
                wheelRight,
                wheelUp,
                transform);
    }

    QueryResult RuntimeImplementation::QueryVehicleAntiRollBars(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const AZStd::span<VehicleAntiRollBarConfiguration> antiRollBars) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->QueryVehicleAntiRollBars(vehicleHandle, antiRollBars);
        }
        return {};
    }

    QueryResult RuntimeImplementation::QueryVehicleDifferentials(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const AZStd::span<VehicleDifferentialConfiguration> differentials) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->QueryVehicleDifferentials(vehicleHandle, differentials);
        }
        return {};
    }

    bool RuntimeImplementation::UpdateVehicleRuntimeConfiguration(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const VehicleRuntimeConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateVehicleRuntimeConfiguration(
                vehicleHandle,
                configuration);
    }

    bool RuntimeImplementation::SetWheelMotion(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const AZ::u32 wheelIndex,
        const WheelMotion& motion)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->SetWheelMotion(
                vehicleHandle,
                wheelIndex,
                motion);
    }

    bool RuntimeImplementation::SetVehiclePowertrainControl(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const VehiclePowertrainControl& control)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->SetVehiclePowertrainControl(
                vehicleHandle,
                control);
    }

    bool RuntimeImplementation::SetVehicleCallbacks(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const ExtensionHandle extensionHandle)
    {
        auto* callbacks = static_cast<IVehicleCallbacks*>(AcquireExtension(
            extensionHandle,
            ExtensionKind::VehicleCallbacks));
        if (extensionHandle && !callbacks)
        {
            return false;
        }

        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            if (extensionHandle)
            {
                ReleaseExtension(extensionHandle);
            }
            return false;
        }
        return world->SetVehicleCallbacks(vehicleHandle, extensionHandle, callbacks);
    }

    bool RuntimeImplementation::SetVehicleCollisionFilter(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const ExtensionHandle extensionHandle)
    {
        auto* filter = static_cast<IVehicleCollisionFilter*>(AcquireExtension(
            extensionHandle,
            ExtensionKind::VehicleCollisionFilter));
        if (extensionHandle && !filter)
        {
            return false;
        }

        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            if (extensionHandle)
            {
                ReleaseExtension(extensionHandle);
            }
            return false;
        }
        return world->SetVehicleCollisionFilter(vehicleHandle, extensionHandle, filter);
    }

    bool RuntimeImplementation::SetVehicleDifferentialLimitedSlipRatio(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const float ratio)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetVehicleDifferentialLimitedSlipRatio(vehicleHandle, ratio);
    }

    bool RuntimeImplementation::SetVehicleTrackAngularVelocity(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const AZ::u32 trackIndex,
        const float angularVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetVehicleTrackAngularVelocity(vehicleHandle, trackIndex, angularVelocity);
    }

    bool RuntimeImplementation::SetWheeledVehicleInput(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const WheeledVehicleInput& input)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetWheeledVehicleInput(vehicleHandle, input);
    }

    bool RuntimeImplementation::UpdateMotorcycleController(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const MotorcycleControllerUpdateConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateMotorcycleController(vehicleHandle, configuration);
    }

    bool RuntimeImplementation::UpdateVehicleAntiRollBars(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const AZStd::span<const VehicleAntiRollBarConfiguration> antiRollBars)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateVehicleAntiRollBars(vehicleHandle, antiRollBars);
    }

    bool RuntimeImplementation::UpdateVehicleCollisionConfiguration(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const VehicleCollisionConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateVehicleCollisionConfiguration(
                vehicleHandle,
                configuration);
    }

    bool RuntimeImplementation::UpdateVehicleDifferentials(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const AZStd::span<const VehicleDifferentialConfiguration> differentials)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->UpdateVehicleDifferentials(vehicleHandle, differentials);
    }

    bool RuntimeImplementation::UpdateVehicleEngineConfiguration(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const VehicleEngineConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateVehicleEngineConfiguration(
                vehicleHandle,
                configuration);
    }

    bool RuntimeImplementation::UpdateVehicleTransmissionConfiguration(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const VehicleTransmissionConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateVehicleTransmissionConfiguration(
                vehicleHandle,
                configuration);
    }

    bool RuntimeImplementation::UpdateVehicleTrackConfiguration(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const AZ::u32 trackIndex,
        const VehicleTrackConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateVehicleTrackConfiguration(
                vehicleHandle,
                trackIndex,
                configuration);
    }

    bool RuntimeImplementation::SetTrackedVehicleInput(
        const WorldHandle worldHandle,
        const VehicleHandle vehicleHandle,
        const TrackedVehicleInput& input)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetTrackedVehicleInput(vehicleHandle, input);
    }

    StateSnapshotHandle RuntimeImplementation::CaptureBodyState(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CaptureBodyState(bodyHandle);
        }

        return {};
    }

    Operation<StateSnapshotHandle> RuntimeImplementation::CaptureBodyStateAsync(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        if (!m_operationPool)
        {
            return {};
        }

        struct Work final
        {
            RuntimeImplementation* m_runtime = nullptr;
            WorldHandle m_worldHandle;
            BodyHandle m_bodyHandle;
        };

        return m_operationPool->CreateOperation<StateSnapshotHandle>(
            Work{
                .m_runtime = this,
                .m_worldHandle = worldHandle,
                .m_bodyHandle = bodyHandle,
            },
            [](Work& work, StateSnapshotHandle& result)
            {
                result = work.m_runtime->CaptureBodyState(work.m_worldHandle, work.m_bodyHandle);
                return static_cast<bool>(result);
            });
    }

    bool RuntimeImplementation::CaptureBodyState(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const StateSnapshotHandle snapshotHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->CaptureBodyState(bodyHandle, snapshotHandle);
    }

    StateRestoreResult RuntimeImplementation::RestoreBodyState(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {.m_status = StateRestoreStatus::Rejected};
        }
        return world->RestoreBodyState(snapshotHandle);
    }

    Operation<StateRestoreResult> RuntimeImplementation::RestoreBodyStateAsync(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle)
    {
        if (!m_operationPool)
        {
            return {};
        }

        struct Work final
        {
            RuntimeImplementation* m_runtime = nullptr;
            WorldHandle m_worldHandle;
            StateSnapshotHandle m_snapshotHandle;
        };

        return m_operationPool->CreateOperation<StateRestoreResult>(
            Work{
                .m_runtime = this,
                .m_worldHandle = worldHandle,
                .m_snapshotHandle = snapshotHandle,
            },
            [](Work& work, StateRestoreResult& result)
            {
                result = work.m_runtime->RestoreBodyState(work.m_worldHandle, work.m_snapshotHandle);
                return static_cast<bool>(result);
            });
    }

    StateSnapshotHandle RuntimeImplementation::CaptureWorldState(
        const WorldHandle worldHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CaptureState();
        }
        return {};
    }

    Operation<StateSnapshotHandle> RuntimeImplementation::CaptureWorldStateAsync(
        const WorldHandle worldHandle)
    {
        if (!m_operationPool)
        {
            return {};
        }

        struct Work final
        {
            RuntimeImplementation* m_runtime = nullptr;
            WorldHandle m_worldHandle;
        };

        return m_operationPool->CreateOperation<StateSnapshotHandle>(
            Work{
                .m_runtime = this,
                .m_worldHandle = worldHandle,
            },
            [](Work& work, StateSnapshotHandle& result)
            {
                result = work.m_runtime->CaptureWorldState(work.m_worldHandle);
                return static_cast<bool>(result);
            });
    }

    bool RuntimeImplementation::CaptureWorldState(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->CaptureState(snapshotHandle);
    }

    StateSnapshotHandle RuntimeImplementation::CaptureWorldState(
        const WorldHandle worldHandle,
        const StateSnapshotConfiguration& configuration,
        const AZStd::span<const BodyHandle> bodyHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CaptureState(configuration, bodyHandles);
        }

        return {};
    }

    bool RuntimeImplementation::CaptureWorldState(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle,
        const StateSnapshotConfiguration& configuration,
        const AZStd::span<const BodyHandle> bodyHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->CaptureState(
                snapshotHandle,
                configuration,
                bodyHandles);
    }

    bool RuntimeImplementation::CaptureWorldStateParts(
        const WorldHandle worldHandle,
        const StateSnapshotConfiguration& configuration,
        const AZStd::span<const BodyHandle> bodyHandles,
        const AZStd::span<const AZ::u32> partitionBodyCounts,
        const AZStd::span<StateSnapshotHandle> snapshotHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->CaptureStateParts(
                configuration,
                bodyHandles,
                partitionBodyCounts,
                snapshotHandles);
    }

    bool RuntimeImplementation::ExportWorldStateArchive(
        const WorldHandle worldHandle,
        const AZStd::span<const StateSnapshotHandle> snapshotHandles,
        StateSnapshotArchive& archive)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ExportStateArchive(snapshotHandles, archive);
    }

    bool RuntimeImplementation::ImportWorldStateArchive(
        const WorldHandle worldHandle,
        const StateSnapshotArchive& archive,
        const AZStd::span<StateSnapshotHandle> snapshotHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ImportStateArchive(archive, snapshotHandles);
    }

    bool RuntimeImplementation::DestroyStateSnapshot(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DestroyStateSnapshot(snapshotHandle);
    }

    bool RuntimeImplementation::IsValid(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsValid(snapshotHandle);
    }

    StateRestoreResult RuntimeImplementation::RestoreWorldState(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {.m_status = StateRestoreStatus::Rejected};
        }
        return world->RestoreState(snapshotHandle);
    }

    Operation<StateRestoreResult> RuntimeImplementation::RestoreWorldStateAsync(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle)
    {
        if (!m_operationPool)
        {
            return {};
        }

        struct Work final
        {
            RuntimeImplementation* m_runtime = nullptr;
            WorldHandle m_worldHandle;
            StateSnapshotHandle m_snapshotHandle;
        };

        return m_operationPool->CreateOperation<StateRestoreResult>(
            Work{
                .m_runtime = this,
                .m_worldHandle = worldHandle,
                .m_snapshotHandle = snapshotHandle,
            },
            [](Work& work, StateRestoreResult& result)
            {
                result = work.m_runtime->RestoreWorldState(work.m_worldHandle, work.m_snapshotHandle);
                return static_cast<bool>(result);
            });
    }

    StateRestoreResult RuntimeImplementation::RestoreWorldStateParts(
        const WorldHandle worldHandle,
        const AZStd::span<const StateSnapshotHandle> snapshotHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {.m_status = StateRestoreStatus::Rejected};
        }
        return world->RestoreStateParts(snapshotHandles);
    }

    bool RuntimeImplementation::ValidateWorldState(
        const WorldHandle worldHandle,
        const StateSnapshotHandle snapshotHandle,
        StateValidationResult& result)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ValidateState(snapshotHandle, result);
    }

    bool RuntimeImplementation::GetWorldStateDigest(
        const WorldHandle worldHandle,
        WorldStateDigest& digest) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetStateDigest(digest);
    }

    bool RuntimeImplementation::GetWorldStatistics(
        const WorldHandle worldHandle,
        WorldStatistics& statistics) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetStatistics(statistics);
    }

    bool RuntimeImplementation::ConfigurePerformanceStatistics(
        const WorldHandle worldHandle,
        const PerformanceStatisticsFlags flags)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ConfigurePerformanceStatistics(flags);
    }

    bool RuntimeImplementation::GetPerformanceStatistics(
        const WorldHandle worldHandle,
        WorldPerformanceStatistics& statistics,
        const bool reset)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetPerformanceStatistics(statistics, reset);
    }

    DiagnosticStatisticsResult RuntimeImplementation::GetBroadPhaseStatistics(
        const WorldHandle worldHandle,
        const AZStd::span<BroadPhaseStatistics> statistics,
        const bool reset)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }
        return world->GetBroadPhaseStatistics(statistics, reset);
    }

    DiagnosticStatisticsResult RuntimeImplementation::GetNarrowPhaseStatistics(
        const AZStd::span<NarrowPhaseStatistics> statistics,
        const bool reset)
    {
#ifdef JPH_TRACK_NARROWPHASE_STATS
        const auto readCounter = [reset](JPH::atomic<JPH::uint64>& counter)
        {
            if (reset)
            {
                return counter.exchange(0);
            }
            return counter.load();
        };

        DiagnosticStatisticsResult result;
        for (JPH::uint firstIndex = 0; firstIndex < JPH::NumSubShapeTypes; ++firstIndex)
        {
            for (JPH::uint secondIndex = 0; secondIndex < JPH::NumSubShapeTypes; ++secondIndex)
            {
                for (AZ::u8 queryIndex = 0; queryIndex < 2; ++queryIndex)
                {
                    JPH::NarrowPhaseStat* nativeStatistics = &JPH::NarrowPhaseStat::sCollideShape[firstIndex][secondIndex];
                    NarrowPhaseQueryKind queryKind = NarrowPhaseQueryKind::Collide;
                    if (queryIndex == 1)
                    {
                        nativeStatistics = &JPH::NarrowPhaseStat::sCastShape[firstIndex][secondIndex];
                        queryKind = NarrowPhaseQueryKind::Cast;
                    }

                    NarrowPhaseStatistics value{
                        .m_totalTicks = readCounter(nativeStatistics->mTotalTicks),
                        .m_childTicks = readCounter(nativeStatistics->mChildTicks),
                        .m_queryCount = readCounter(nativeStatistics->mNumQueries),
                        .m_hitsReported = readCounter(nativeStatistics->mHitsReported),
                        .m_firstShapeKind = FromNativeShapeKind(JPH::sAllSubShapeTypes[firstIndex]),
                        .m_secondShapeKind = FromNativeShapeKind(JPH::sAllSubShapeTypes[secondIndex]),
                        .m_queryKind = queryKind,
                    };
                    if (value.m_totalTicks == 0
                        && value.m_childTicks == 0
                        && value.m_queryCount == 0
                        && value.m_hitsReported == 0)
                    {
                        continue;
                    }

                    if (result.m_requiredCount < statistics.size())
                    {
                        statistics[result.m_requiredCount] = value;
                        ++result.m_count;
                    }
                    ++result.m_requiredCount;
                }
            }
        }

        result.m_status = DiagnosticStatisticsStatus::Complete;
        if (result.m_requiredCount > statistics.size())
        {
            result.m_status = DiagnosticStatisticsStatus::Overflow;
        }
        return result;
#else
        AZ_UNUSED(statistics);
        AZ_UNUSED(reset);
        DiagnosticStatisticsResult result;
        result.m_status = DiagnosticStatisticsStatus::Unavailable;
        return result;
#endif
    }

    bool RuntimeImplementation::DrawDebug(
        const WorldHandle worldHandle,
        const DebugDrawSettings& settings,
        IDebugRenderer& renderer,
        const IDebugFilter* filter)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world || !m_debugRenderer)
        {
            return false;
        }

        AZStd::lock_guard rendererLock(GetNativeDebugRendererMutex());
        return world->DrawDebug(
            settings,
            renderer,
            *m_debugRenderer,
            filter);
    }

    bool RuntimeImplementation::ConfigureDebugCapture(
        const WorldHandle worldHandle,
        const DebugCaptureConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world || !m_debugRenderer)
        {
            return false;
        }

        AZStd::lock_guard rendererLock(GetNativeDebugRendererMutex());
        const bool wasEnabled = world->IsDebugCaptureEnabled();
        if (!world->ConfigureDebugCapture(configuration))
        {
            return false;
        }

        const bool isEnabled = world->IsDebugCaptureEnabled();
        if (wasEnabled == isEnabled)
        {
            return true;
        }
        if (isEnabled)
        {
            m_debugCaptureWorldCount.fetch_add(1);
            return true;
        }

        [[maybe_unused]] const AZ::u32 previousCaptureWorldCount = m_debugCaptureWorldCount.fetch_sub(1);
        AZ_Assert(previousCaptureWorldCount > 0, "The Jolt debug-capture world count underflowed.");
        return true;
    }

    bool RuntimeImplementation::GetDebugCaptureStatistics(
        const WorldHandle worldHandle,
        DebugCaptureStatistics& statistics) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetDebugCaptureStatistics(statistics);
    }

    QueryResult RuntimeImplementation::GetBodies(
        const WorldHandle worldHandle,
        const BodyKind kind,
        const bool activeOnly,
        const AZStd::span<BodyHandle> bodies) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->GetBodies(
                kind,
                activeOnly,
                bodies);
        }

        return {};
    }

    bool RuntimeImplementation::GetBodyId(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        BodyId& bodyId) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyId(bodyHandle, bodyId);
    }

    bool RuntimeImplementation::ActivateBody(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ActivateBody(bodyHandle);
    }

    bool RuntimeImplementation::ActivateBodies(
        const WorldHandle worldHandle,
        const AZStd::span<const BodyHandle> bodyHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ActivateBodies(bodyHandles);
    }

    bool RuntimeImplementation::ActivateBodiesInBounds(
        const WorldHandle worldHandle,
        const BroadPhaseAabb& bounds,
        const ObjectLayer collisionLayer)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->ActivateBodiesInBounds(
                bounds,
                collisionLayer);
    }

    bool RuntimeImplementation::DeactivateBody(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DeactivateBody(bodyHandle);
    }

    bool RuntimeImplementation::DeactivateBodies(
        const WorldHandle worldHandle,
        const AZStd::span<const BodyHandle> bodyHandles)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->DeactivateBodies(bodyHandles);
    }

    bool RuntimeImplementation::ResetBodySleepTimer(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ResetBodySleepTimer(bodyHandle);
    }

    bool RuntimeImplementation::InvalidateBodyContactCache(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->InvalidateBodyContactCache(bodyHandle);
    }

    bool RuntimeImplementation::GetBodyPointVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const WorldPosition& point,
        AZ::Vector3& velocity) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyPointVelocity(bodyHandle, point, velocity);
    }

    bool RuntimeImplementation::GetBodyMotionType(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        MotionType& motionType) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyMotionType(bodyHandle, motionType);
    }

    bool RuntimeImplementation::GetBodyObjectLayer(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        ObjectLayer& objectLayer) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyObjectLayer(bodyHandle, objectLayer);
    }

    bool RuntimeImplementation::GetBodyCollisionGroup(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        CollisionGroupConfiguration& collisionGroup) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyCollisionGroup(bodyHandle, collisionGroup);
    }

    bool RuntimeImplementation::GetBodyShape(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        ShapeHandle& shapeHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyShape(bodyHandle, shapeHandle);
    }

    bool RuntimeImplementation::GetBodyAccumulatedForceAndTorque(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZ::Vector3& force,
        AZ::Vector3& torque) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetBodyAccumulatedForceAndTorque(
                bodyHandle,
                force,
                torque);
    }

    bool RuntimeImplementation::ResetBodyAccumulatedForce(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ResetBodyAccumulatedForce(bodyHandle);
    }

    bool RuntimeImplementation::ResetBodyAccumulatedTorque(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ResetBodyAccumulatedTorque(bodyHandle);
    }

    bool RuntimeImplementation::ResetBodyMotion(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->ResetBodyMotion(bodyHandle);
    }

    bool RuntimeImplementation::GetBodyBounds(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        BroadPhaseAabb& bounds) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyBounds(bodyHandle, bounds);
    }

    bool RuntimeImplementation::GetBodySubmergedVolume(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const WorldPosition& surfacePosition,
        const AZ::Vector3& surfaceNormal,
        SubmergedVolumeResult& result) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetBodySubmergedVolume(
                bodyHandle,
                surfacePosition,
                surfaceNormal,
                result);
    }

    bool RuntimeImplementation::GetBodySurfaceNormal(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const SubShapeId subShapeId,
        const WorldPosition& surfacePosition,
        AZ::Vector3& normal) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetBodySurfaceNormal(
                bodyHandle,
                subShapeId,
                surfacePosition,
                normal);
    }

    bool RuntimeImplementation::GetBodyMaterial(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const SubShapeId subShapeId,
        MaterialHandle& materialHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetBodyMaterial(
                bodyHandle,
                subShapeId,
                materialHandle);
    }

    bool RuntimeImplementation::GetBodyPosition(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        WorldPosition& position) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyPosition(bodyHandle, position);
    }

    bool RuntimeImplementation::GetBodyRotation(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZ::Quaternion& rotation) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyRotation(bodyHandle, rotation);
    }

    bool RuntimeImplementation::GetBodyVelocities(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZ::Vector3& linearVelocity,
        AZ::Vector3& angularVelocity) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetBodyVelocities(
                bodyHandle,
                linearVelocity,
                angularVelocity);
    }

    bool RuntimeImplementation::GetBodyLinearVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZ::Vector3& linearVelocity) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyLinearVelocity(bodyHandle, linearVelocity);
    }

    bool RuntimeImplementation::GetBodyAngularVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZ::Vector3& angularVelocity) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyAngularVelocity(bodyHandle, angularVelocity);
    }

    bool RuntimeImplementation::SetBodyPosition(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const WorldPosition& position,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyPosition(bodyHandle, position, activate);
    }

    bool RuntimeImplementation::SetBodyRotation(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Quaternion& rotation,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyRotation(bodyHandle, rotation, activate);
    }

    bool RuntimeImplementation::SetBodyTransform(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const WorldTransform& transform,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyTransform(bodyHandle, transform, activate);
    }

    bool RuntimeImplementation::SetBodyTransformWhenChanged(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const WorldTransform& transform,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyTransformWhenChanged(bodyHandle, transform, activate);
    }

    bool RuntimeImplementation::SetBodyVelocities(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& linearVelocity,
        const AZ::Vector3& angularVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyVelocities(bodyHandle, linearVelocity, angularVelocity);
    }

    bool RuntimeImplementation::SetBodyLinearVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& linearVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyLinearVelocity(bodyHandle, linearVelocity);
    }

    bool RuntimeImplementation::SetBodyAngularVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& angularVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyAngularVelocity(bodyHandle, angularVelocity);
    }

    bool RuntimeImplementation::AddBodyVelocities(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& linearVelocity,
        const AZ::Vector3& angularVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddBodyVelocities(bodyHandle, linearVelocity, angularVelocity);
    }

    bool RuntimeImplementation::AddBodyLinearVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& linearVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddBodyLinearVelocity(bodyHandle, linearVelocity);
    }

    bool RuntimeImplementation::SetBodyTransformAndVelocities(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const WorldTransform& transform,
        const AZ::Vector3& linearVelocity,
        const AZ::Vector3& angularVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->SetBodyTransformAndVelocities(
                bodyHandle,
                transform,
                linearVelocity,
                angularVelocity);
    }

    bool RuntimeImplementation::MoveBodyKinematically(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const WorldTransform& target,
        const float duration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->MoveBodyKinematically(bodyHandle, target, duration);
    }

    bool RuntimeImplementation::AddForce(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& force,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddForce(bodyHandle, force, activate);
    }

    bool RuntimeImplementation::AddForceAtPosition(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& force,
        const WorldPosition& position,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddForceAtPosition(bodyHandle, force, position, activate);
    }

    bool RuntimeImplementation::AddTorque(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& torque,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddTorque(bodyHandle, torque, activate);
    }

    bool RuntimeImplementation::AddForceAndTorque(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& force,
        const AZ::Vector3& torque,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddForceAndTorque(bodyHandle, force, torque, activate);
    }

    bool RuntimeImplementation::ApplyBuoyancyImpulse(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const BuoyancyConfiguration& configuration)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return false;
        }

        if (m_debugCaptureWorldCount.load() > 0)
        {
            AZStd::lock_guard rendererLock(GetNativeDebugRendererMutex());
            return world->ApplyBuoyancyImpulse(
                bodyHandle,
                configuration,
                m_debugRenderer);
        }
        return world->ApplyBuoyancyImpulse(
            bodyHandle,
            configuration,
            nullptr);
    }

    bool RuntimeImplementation::GetBodyFriction(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        float& friction) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyFriction(bodyHandle, friction);
    }

    bool RuntimeImplementation::SetBodyFriction(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const float friction)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyFriction(bodyHandle, friction);
    }

    bool RuntimeImplementation::GetBodyRestitution(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        float& restitution) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyRestitution(bodyHandle, restitution);
    }

    bool RuntimeImplementation::SetBodyRestitution(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const float restitution)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyRestitution(bodyHandle, restitution);
    }

    bool RuntimeImplementation::GetBodyGravityFactor(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        float& gravityFactor) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyGravityFactor(bodyHandle, gravityFactor);
    }

    bool RuntimeImplementation::SetBodyGravityFactor(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const float gravityFactor)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyGravityFactor(bodyHandle, gravityFactor);
    }

    bool RuntimeImplementation::GetBodyMaximumLinearVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        float& maximumLinearVelocity) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyMaximumLinearVelocity(bodyHandle, maximumLinearVelocity);
    }

    bool RuntimeImplementation::SetBodyMaximumLinearVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const float maximumLinearVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyMaximumLinearVelocity(bodyHandle, maximumLinearVelocity);
    }

    bool RuntimeImplementation::GetBodyMaximumAngularVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        float& maximumAngularVelocity) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyMaximumAngularVelocity(bodyHandle, maximumAngularVelocity);
    }

    bool RuntimeImplementation::SetBodyMaximumAngularVelocity(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const float maximumAngularVelocity)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyMaximumAngularVelocity(bodyHandle, maximumAngularVelocity);
    }

    bool RuntimeImplementation::GetBodyMotionQuality(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        MotionQuality& motionQuality) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyMotionQuality(bodyHandle, motionQuality);
    }

    bool RuntimeImplementation::SetBodyMotionQuality(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const MotionQuality motionQuality)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyMotionQuality(bodyHandle, motionQuality);
    }

    bool RuntimeImplementation::IsBodyManifoldReductionEnabled(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        bool& enabled) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsBodyManifoldReductionEnabled(bodyHandle, enabled);
    }

    bool RuntimeImplementation::SetBodyManifoldReductionEnabled(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool enabled)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyManifoldReductionEnabled(bodyHandle, enabled);
    }

    bool RuntimeImplementation::IsBodySensor(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        bool& sensor) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsBodySensor(bodyHandle, sensor);
    }

    bool RuntimeImplementation::SetBodySensor(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool sensor)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodySensor(bodyHandle, sensor);
    }

    bool RuntimeImplementation::GetBodyLinearDamping(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        float& linearDamping) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyLinearDamping(bodyHandle, linearDamping);
    }

    bool RuntimeImplementation::SetBodyLinearDamping(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const float linearDamping)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyLinearDamping(bodyHandle, linearDamping);
    }

    bool RuntimeImplementation::GetBodyAngularDamping(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        float& angularDamping) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyAngularDamping(bodyHandle, angularDamping);
    }

    bool RuntimeImplementation::SetBodyAngularDamping(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const float angularDamping)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyAngularDamping(bodyHandle, angularDamping);
    }

    bool RuntimeImplementation::IsBodySleepingAllowed(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        bool& sleepingAllowed) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsBodySleepingAllowed(bodyHandle, sleepingAllowed);
    }

    bool RuntimeImplementation::SetBodySleepingAllowed(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool sleepingAllowed)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodySleepingAllowed(bodyHandle, sleepingAllowed);
    }

    bool RuntimeImplementation::IsBodyGyroscopicForceEnabled(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        bool& enabled) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsBodyGyroscopicForceEnabled(bodyHandle, enabled);
    }

    bool RuntimeImplementation::SetBodyGyroscopicForceEnabled(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool enabled)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyGyroscopicForceEnabled(bodyHandle, enabled);
    }

    bool RuntimeImplementation::IsBodyKinematicVsNonDynamicCollisionEnabled(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        bool& enabled) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsBodyKinematicVsNonDynamicCollisionEnabled(bodyHandle, enabled);
    }

    bool RuntimeImplementation::SetBodyKinematicVsNonDynamicCollisionEnabled(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool enabled)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyKinematicVsNonDynamicCollisionEnabled(bodyHandle, enabled);
    }

    bool RuntimeImplementation::IsBodyEnhancedInternalEdgeRemovalEnabled(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        bool& enabled) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->IsBodyEnhancedInternalEdgeRemovalEnabled(bodyHandle, enabled);
    }

    bool RuntimeImplementation::SetBodyEnhancedInternalEdgeRemovalEnabled(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const bool enabled)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyEnhancedInternalEdgeRemovalEnabled(bodyHandle, enabled);
    }

    bool RuntimeImplementation::GetBodySolverStepCounts(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZ::u8& velocityStepCount,
        AZ::u8& positionStepCount) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->GetBodySolverStepCounts(
                bodyHandle,
                velocityStepCount,
                positionStepCount);
    }

    bool RuntimeImplementation::SetBodySolverStepCounts(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::u8 velocityStepCount,
        const AZ::u8 positionStepCount)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->SetBodySolverStepCounts(
                bodyHandle,
                velocityStepCount,
                positionStepCount);
    }

    bool RuntimeImplementation::UpdateBodyRuntimeConfiguration(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const BodyRuntimeConfiguration& configuration,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->UpdateBodyRuntimeConfiguration(
                bodyHandle,
                configuration,
                activate);
    }

    bool RuntimeImplementation::GetBodyInverseInertia(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        AZ::Matrix3x3& inverseInertia) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyInverseInertia(bodyHandle, inverseInertia);
    }

    bool RuntimeImplementation::GetBodyInverseMass(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        float& inverseMass) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBodyInverseMass(bodyHandle, inverseMass);
    }

    bool RuntimeImplementation::AddImpulse(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& impulse)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddImpulse(bodyHandle, impulse);
    }

    bool RuntimeImplementation::AddImpulseAtPosition(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& impulse,
        const WorldPosition& position)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddImpulseAtPosition(bodyHandle, impulse, position);
    }

    bool RuntimeImplementation::AddAngularImpulse(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const AZ::Vector3& angularImpulse)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->AddAngularImpulse(bodyHandle, angularImpulse);
    }

    bool RuntimeImplementation::SetBodyShape(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const ShapeHandle shapeHandle,
        const bool updateMassProperties,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyShape(bodyHandle, shapeHandle, updateMassProperties, activate);
    }

    bool RuntimeImplementation::SetBodyMotionType(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const MotionType motionType,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyMotionType(bodyHandle, motionType, activate);
    }

    bool RuntimeImplementation::SetBodyObjectLayer(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const ObjectLayer objectLayer)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyObjectLayer(bodyHandle, objectLayer);
    }

    bool RuntimeImplementation::SetBodyCollisionGroup(
        const WorldHandle worldHandle,
        const BodyHandle bodyHandle,
        const CollisionGroupConfiguration& collisionGroup,
        const bool activate)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->SetBodyCollisionGroup(bodyHandle, collisionGroup, activate);
    }

    bool RuntimeImplementation::RaycastShapeClosest(
        const WorldHandle worldHandle,
        const ShapeRaycastRequest& request,
        ShapeRaycastHit& hit) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->RaycastShapeClosest(request, hit);
    }

    QueryResult RuntimeImplementation::RaycastShapeAll(
        const WorldHandle worldHandle,
        const ShapeRaycastRequest& request,
        const AZStd::span<ShapeRaycastHit> hits) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->RaycastShapeAll(request, hits);
    }

    QueryResult RuntimeImplementation::CollideShapePoint(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::Vector3& localPosition,
        const IQueryFilter* filter,
        const AZStd::span<ShapePointHit> hits) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->CollideShapePoint(
            shapeHandle,
            localPosition,
            filter,
            hits);
    }

    bool RuntimeImplementation::CollideShapePointAny(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const AZ::Vector3& localPosition,
        const IQueryFilter* filter) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world
            && world->CollideShapePointAny(
                shapeHandle,
                localPosition,
                filter);
    }

    QueryResult RuntimeImplementation::CollectShapeTriangles(
        const WorldHandle worldHandle,
        const ShapeTriangleCollectionRequest& request,
        const AZStd::span<ShapeTriangle> triangles) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->CollectShapeTriangles(request, triangles);
    }

    bool RuntimeImplementation::RaycastTransformedShapeClosest(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const TransformedShapeRaycastRequest& request,
        RaycastHit& hit) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->RaycastTransformedShapeClosest(shape, request, hit);
    }

    QueryResult RuntimeImplementation::RaycastTransformedShapeAll(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const TransformedShapeRaycastRequest& request,
        const AZStd::span<RaycastHit> hits) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->RaycastTransformedShapeAll(shape, request, hits);
    }

    QueryResult RuntimeImplementation::CollideTransformedShapePoint(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const WorldPosition& position,
        const IQueryFilter* filter,
        const AZStd::span<OverlapHit> hits) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->CollideTransformedShapePoint(shape, position, filter, hits);
    }

    bool RuntimeImplementation::CollideTransformedShapePointAny(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const WorldPosition& position,
        const IQueryFilter* filter) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->CollideTransformedShapePointAny(shape, position, filter);
    }

    QueryResult RuntimeImplementation::CollectTransformedShapeChildren(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const BroadPhaseAabb& bounds,
        const IQueryFilter* filter,
        const AZStd::span<TransformedShape> children) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->CollectTransformedShapeChildren(shape, bounds, filter, children);
    }

    QueryResult RuntimeImplementation::CollectTransformedShapeTriangles(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const BroadPhaseAabb& bounds,
        const AZStd::span<TransformedTriangle> triangles) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->CollectTransformedShapeTriangles(shape, bounds, triangles);
    }

    bool RuntimeImplementation::GetTransformedShapeSurfaceNormal(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const SubShapeId subShapeId,
        const WorldPosition& position,
        AZ::Vector3& normal) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetTransformedShapeSurfaceNormal(shape, subShapeId, position, normal);
    }

    QueryResult RuntimeImplementation::GetTransformedShapeSupportingFace(
        const WorldHandle worldHandle,
        const TransformedShape& shape,
        const SubShapeId subShapeId,
        const AZ::Vector3& direction,
        const AZStd::span<WorldPosition> vertices) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->GetTransformedShapeSupportingFace(shape, subShapeId, direction, vertices);
    }

    bool RuntimeImplementation::RetainShape(
        const WorldHandle worldHandle,
        const ShapeHandle shapeHandle,
        const WorldTransform& transform,
        const float uniformScale,
        TransformedShape& shape) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->RetainShape(shapeHandle, transform, uniformScale, shape);
    }

    QueryResult RuntimeImplementation::CollideTransformedShapes(
        const WorldHandle worldHandle,
        const TransformedShape& firstShape,
        const TransformedShape& secondShape,
        const TransformedShapeCollisionRequest& request,
        const AZStd::span<TransformedShapeCollisionHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->CollideTransformedShapes(
            firstShape,
            secondShape,
            request,
            hits,
            faceBuffers);
    }

    bool RuntimeImplementation::CollideTransformedShapes(
        const WorldHandle worldHandle,
        const TransformedShape& firstShape,
        const TransformedShape& secondShape,
        const TransformedShapeCollisionRequest& request,
        ITransformedShapeCollisionCollector& collector) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->CollideTransformedShapes(
            firstShape,
            secondShape,
            request,
            collector);
    }

    QueryResult RuntimeImplementation::CollideTransformedShapes(
        const WorldHandle worldHandle,
        const ShapePlacement& firstShape,
        const ShapePlacement& secondShape,
        const TransformedShapeCollisionRequest& request,
        const AZStd::span<TransformedShapeCollisionHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        TransformedShape retainedFirstShape;
        TransformedShape retainedSecondShape;
        if (!world->RetainShape(
                firstShape.m_shapeHandle,
                firstShape.m_transform,
                firstShape.m_uniformScale,
                retainedFirstShape)
            || !world->RetainShape(
                secondShape.m_shapeHandle,
                secondShape.m_transform,
                secondShape.m_uniformScale,
                retainedSecondShape))
        {
            return {};
        }

        return world->CollideTransformedShapes(
            retainedFirstShape,
            retainedSecondShape,
            request,
            hits,
            faceBuffers);
    }

    QueryResult RuntimeImplementation::CastTransformedShape(
        const WorldHandle worldHandle,
        const TransformedShape& firstShape,
        const TransformedShape& secondShape,
        const TransformedShapeCastRequest& request,
        const AZStd::span<TransformedShapeCastHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->CastTransformedShape(
            firstShape,
            secondShape,
            request,
            hits,
            faceBuffers);
    }

    bool RuntimeImplementation::CastTransformedShape(
        const WorldHandle worldHandle,
        const TransformedShape& firstShape,
        const TransformedShape& secondShape,
        const TransformedShapeCastRequest& request,
        ITransformedShapeCastCollector& collector) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->CastTransformedShape(
            firstShape,
            secondShape,
            request,
            collector);
    }

    QueryResult RuntimeImplementation::CastTransformedShape(
        const WorldHandle worldHandle,
        const ShapePlacement& firstShape,
        const ShapePlacement& secondShape,
        const TransformedShapeCastRequest& request,
        const AZStd::span<TransformedShapeCastHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        TransformedShape retainedFirstShape;
        TransformedShape retainedSecondShape;
        if (!world->RetainShape(
                firstShape.m_shapeHandle,
                firstShape.m_transform,
                firstShape.m_uniformScale,
                retainedFirstShape)
            || !world->RetainShape(
                secondShape.m_shapeHandle,
                secondShape.m_transform,
                secondShape.m_uniformScale,
                retainedSecondShape))
        {
            return {};
        }

        return world->CastTransformedShape(
            retainedFirstShape,
            retainedSecondShape,
            request,
            hits,
            faceBuffers);
    }

    bool RuntimeImplementation::RaycastClosest(
        const WorldHandle worldHandle,
        const RaycastRequest& request,
        RaycastHit& hit) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->RaycastClosest(request, hit);
    }

    BufferResult RuntimeImplementation::RaycastClosestBatch(
        const WorldHandle worldHandle,
        const AZStd::span<const RaycastRequest> requests,
        const AZStd::span<ClosestRaycastResult> results) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->RaycastClosestBatch(requests, results);
    }

    Operation<RaycastBatchOperationResult> RuntimeImplementation::RaycastClosestBatchAsync(
        const WorldHandle worldHandle,
        const AZStd::span<const RaycastRequest> requests) const
    {
        if (!m_operationPool
            || AZStd::any_of(
                requests.begin(),
                requests.end(),
                [](const RaycastRequest& request)
                {
                    return request.m_filter.m_callback;
                }))
        {
            return {};
        }

        struct Work final
        {
            const RuntimeImplementation* m_runtime = nullptr;
            WorldHandle m_worldHandle;
            AZStd::vector<RaycastRequest> m_requests;
        };

        Work work{
            .m_runtime = this,
            .m_worldHandle = worldHandle,
        };
        work.m_requests.assign(requests.begin(), requests.end());
        return m_operationPool->CreateOperation<RaycastBatchOperationResult>(
            AZStd::move(work),
            [](Work& operationWork, RaycastBatchOperationResult& result)
            {
                result.m_results.resize(operationWork.m_requests.size());
                result.m_bufferResult = operationWork.m_runtime->RaycastClosestBatch(
                    operationWork.m_worldHandle,
                    operationWork.m_requests,
                    result.m_results);
                return result.m_bufferResult.m_requiredCount == operationWork.m_requests.size();
            });
    }

    bool RuntimeImplementation::RaycastAny(
        const WorldHandle worldHandle,
        const RaycastRequest& request) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->RaycastAny(request);
    }

    QueryResult RuntimeImplementation::RaycastAll(
        const WorldHandle worldHandle,
        const RaycastRequest& request,
        const AZStd::span<RaycastHit> hits) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->RaycastAll(request, hits);
    }

    QueryResult RuntimeImplementation::RaycastClosestPerBody(
        const WorldHandle worldHandle,
        const RaycastRequest& request,
        const AZStd::span<RaycastHit> hits) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->RaycastClosestPerBody(request, hits);
    }

    QueryResult RuntimeImplementation::OverlapPoint(
        const WorldHandle worldHandle,
        const PointOverlapRequest& request,
        const AZStd::span<OverlapHit> hits) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->OverlapPoint(request, hits);
    }

    bool RuntimeImplementation::OverlapPointAny(
        const WorldHandle worldHandle,
        const PointOverlapRequest& request) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->OverlapPointAny(request);
    }

    QueryResult RuntimeImplementation::CollideShape(
        const WorldHandle worldHandle,
        const ShapeOverlapRequest& request,
        const AZStd::span<ShapeOverlapHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CollideShape(request, hits, faceBuffers);
        }
        return {};
    }

    QueryResult RuntimeImplementation::OverlapShape(
        const WorldHandle worldHandle,
        const ShapeOverlapRequest& request,
        const AZStd::span<OverlapHit> hits) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->OverlapShape(request, hits);
        }
        return {};
    }

    bool RuntimeImplementation::OverlapShapeAny(
        const WorldHandle worldHandle,
        const ShapeOverlapRequest& request) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->OverlapShapeAny(request);
    }

    bool RuntimeImplementation::CastShapeClosest(
        const WorldHandle worldHandle,
        const ShapeCastRequest& request,
        ShapeCastHit& hit,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->CastShapeClosest(request, hit, faceBuffers);
    }

    QueryResult RuntimeImplementation::CastShapeAll(
        const WorldHandle worldHandle,
        const ShapeCastRequest& request,
        const AZStd::span<ShapeCastHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CastShapeAll(request, hits, faceBuffers);
        }
        return {};
    }

    QueryResult RuntimeImplementation::CastShapeClosestPerBody(
        const WorldHandle worldHandle,
        const ShapeCastRequest& request,
        const AZStd::span<ShapeCastHit> hits,
        const ShapeQueryFaceBuffers& faceBuffers) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

            return world->CastShapeClosestPerBody(request, hits, faceBuffers);
    }

    QueryResult RuntimeImplementation::OverlapBroadPhase(
        const WorldHandle worldHandle,
        const BroadPhaseOverlapRequest& request,
        const AZStd::span<BroadPhaseHit> hits) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->OverlapBroadPhase(request, hits);
        }
        return {};
    }

    bool RuntimeImplementation::OverlapBroadPhaseAny(
        const WorldHandle worldHandle,
        const BroadPhaseOverlapRequest& request) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->OverlapBroadPhaseAny(request);
    }

    bool RuntimeImplementation::CastBroadPhaseClosest(
        const WorldHandle worldHandle,
        const BroadPhaseCastRequest& request,
        BroadPhaseCastHit& hit) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->CastBroadPhaseClosest(request, hit);
    }

    QueryResult RuntimeImplementation::CastBroadPhaseAll(
        const WorldHandle worldHandle,
        const BroadPhaseCastRequest& request,
        const AZStd::span<BroadPhaseCastHit> hits) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (world)
        {
            return world->CastBroadPhaseAll(request, hits);
        }
        return {};
    }

    QueryResult RuntimeImplementation::CollectShapesInBounds(
        const WorldHandle worldHandle,
        const ShapeCollectionRequest& request,
        const AZStd::span<TransformedShape> shapes) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->CollectShapesInBounds(request, shapes);
    }

    QueryResult RuntimeImplementation::GetSupportingFace(
        const WorldHandle worldHandle,
        const SupportingFaceRequest& request,
        const AZStd::span<WorldPosition> vertices) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->GetSupportingFace(request, vertices);
    }

    QueryResult RuntimeImplementation::CollectTriangles(
        const WorldHandle worldHandle,
        const TriangleCollectionRequest& request,
        const AZStd::span<TransformedTriangle> triangles) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        if (!world)
        {
            return {};
        }

        return world->CollectTriangles(request, triangles);
    }

    bool RuntimeImplementation::GetBroadPhaseBounds(
        const WorldHandle worldHandle,
        BroadPhaseAabb& bounds) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->GetBroadPhaseBounds(bounds);
    }

    bool RuntimeImplementation::OptimizeBroadPhase(
        const WorldHandle worldHandle)
    {
        AZStd::shared_lock lock(m_worldMutex);
        World* world = FindWorldUnlocked(worldHandle);
        return world && world->OptimizeBroadPhase();
    }

    bool RuntimeImplementation::WereBodiesInContact(
        const WorldHandle worldHandle,
        const BodyHandle firstBodyHandle,
        const BodyHandle secondBodyHandle) const
    {
        AZStd::shared_lock lock(m_worldMutex);
        const World* world = FindWorldUnlocked(worldHandle);
        return world && world->WereBodiesInContact(firstBodyHandle, secondBodyHandle);
    }

    World* RuntimeImplementation::FindWorldUnlocked(
        const WorldHandle worldHandle)
    {
        return const_cast<World*>(static_cast<const RuntimeImplementation&>(*this).FindWorldUnlocked(worldHandle));
    }

    const World* RuntimeImplementation::FindWorldUnlocked(
        const WorldHandle worldHandle) const
    {
        Internal::WorldHandleParts parts;
        if (!Internal::DecodeWorldHandle(worldHandle, parts)
            || parts.m_index >= m_worldSlots.size())
        {
            return nullptr;
        }

        const WorldSlot& slot = m_worldSlots[parts.m_index];
        if (slot.m_generation != parts.m_generation)
        {
            return nullptr;
        }

        if (!slot.m_world || slot.m_world->IsStateIndeterminate())
        {
            return nullptr;
        }
        return slot.m_world.get();
    }

    bool RuntimeImplementation::AcquireMaterials(
        const AZStd::span<const MaterialHandle> materialHandles,
        JPH::PhysicsMaterialList& materials)
    {
        AZStd::lock_guard lock(m_materialMutex);
        for (const MaterialHandle materialHandle : materialHandles)
        {
            if (!FindMaterialUnlocked(materialHandle))
            {
                return false;
            }
        }

        materials.reserve(materialHandles.size());
        for (const MaterialHandle materialHandle : materialHandles)
        {
            Internal::ResourceHandleParts parts;
            [[maybe_unused]] const bool decoded = Internal::DecodeResourceHandle(materialHandle, parts);
            AZ_Assert(decoded, "A validated material handle could not be decoded.");
            MaterialSlot& slot = m_materialSlots[parts.m_index];
            ++slot.m_referenceCount;
            materials.emplace_back(static_cast<const JPH::PhysicsMaterial*>(slot.m_material.GetPtr()));
        }
        return true;
    }

    void RuntimeImplementation::ReleaseMaterials(
        const AZStd::span<const MaterialHandle> materialHandles)
    {
        AZStd::lock_guard lock(m_materialMutex);
        for (const MaterialHandle materialHandle : materialHandles)
        {
            Internal::ResourceHandleParts parts;
            const bool decoded = Internal::DecodeResourceHandle(materialHandle, parts);
            AZ_Assert(decoded && parts.m_index < m_materialSlots.size(), "Material reference ownership is inconsistent.");
            if (!decoded || parts.m_index >= m_materialSlots.size())
            {
                continue;
            }
            MaterialSlot& slot = m_materialSlots[parts.m_index];
            AZ_Assert(
                slot.m_material && slot.m_generation == parts.m_generation && slot.m_referenceCount > 0,
                "Material reference ownership is inconsistent.");
            if (slot.m_material && slot.m_generation == parts.m_generation && slot.m_referenceCount > 0)
            {
                --slot.m_referenceCount;
            }
        }
    }

    const RuntimeImplementation::MaterialSlot* RuntimeImplementation::FindMaterialUnlocked(
        const MaterialHandle materialHandle) const
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(materialHandle, parts)
            || parts.m_index >= m_materialSlots.size())
        {
            return nullptr;
        }

        const MaterialSlot& slot = m_materialSlots[parts.m_index];
        if (slot.m_generation != parts.m_generation || !slot.m_material)
        {
            return nullptr;
        }

        return &slot;
    }

    MaterialHandle RuntimeImplementation::FindMaterialHandle(
        const JPH::PhysicsMaterial* material) const
    {
        if (!material || material == JPH::PhysicsMaterial::sDefault)
        {
            return {};
        }

        return static_cast<const NativeMaterial*>(material)->GetHandle();
    }

    bool RuntimeImplementation::AcquireCookedShape(
        const CookedShapeHandle cookedShapeHandle,
        JPH::RefConst<JPH::Shape>& shape)
    {
        AZStd::lock_guard lock(m_cookedShapeMutex);
        CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        if (!slot)
        {
            return false;
        }

        if (slot->m_referenceCount == AZStd::numeric_limits<AZ::u32>::max())
        {
            return false;
        }

        ++slot->m_referenceCount;
        shape = slot->m_shape;
        return true;
    }

    CookedShapeHandle RuntimeImplementation::StoreCookedShape(
        const JPH::Shape* shape,
        AZStd::vector<MaterialHandle> materialHandles,
        AZStd::vector<CookedShapeHandle> childHandles,
        const AZ::TypeId customProviderId,
        const ExtensionHandle customProviderExtension,
        AZStd::vector<CustomShapeDependency> customDependencies)
    {
        if (!shape)
        {
            return {};
        }

        AZStd::lock_guard lock(m_cookedShapeMutex);
        for (size_t childIndex = 0; childIndex < childHandles.size(); ++childIndex)
        {
            CookedShapeSlot* childSlot = FindCookedShapeUnlocked(childHandles[childIndex]);
            if (!childSlot)
            {
                return {};
            }

            size_t duplicateCount = 0;
            for (size_t previousIndex = 0; previousIndex < childIndex; ++previousIndex)
            {
                if (childHandles[previousIndex] == childHandles[childIndex])
                {
                    ++duplicateCount;
                }
            }
            if (duplicateCount >= AZStd::numeric_limits<AZ::u32>::max() - childSlot->m_parentCount)
            {
                return {};
            }
        }

        AZ::u32 cookedShapeIndex = 0;
        if (!m_freeCookedShapeSlots.empty())
        {
            cookedShapeIndex = m_freeCookedShapeSlots.back();
            m_freeCookedShapeSlots.pop_back();
        }
        else
        {
            if (m_cookedShapeSlots.size() >= Internal::HandlePayloadMask)
            {
                return {};
            }
            cookedShapeIndex = static_cast<AZ::u32>(m_cookedShapeSlots.size());
            m_cookedShapeSlots.emplace_back();
        }

        CookedShapeSlot& slot = m_cookedShapeSlots[cookedShapeIndex];
        slot.m_shape = shape;
        slot.m_materialHandles = AZStd::move(materialHandles);
        slot.m_childHandles = AZStd::move(childHandles);
        slot.m_customDependencies = AZStd::move(customDependencies);
        slot.m_customProviderId = customProviderId;
        slot.m_customProviderExtension = customProviderExtension;
        for (const CookedShapeHandle childHandle : slot.m_childHandles)
        {
            ++FindCookedShapeUnlocked(childHandle)->m_parentCount;
        }
        return Internal::MakeResourceHandle<CookedShapeHandle>(cookedShapeIndex, slot.m_generation);
    }

    void RuntimeImplementation::ReleaseCookedShape(
        const CookedShapeHandle cookedShapeHandle)
    {
        AZStd::lock_guard lock(m_cookedShapeMutex);
        CookedShapeSlot* slot = FindCookedShapeUnlocked(cookedShapeHandle);
        AZ_Assert(slot && slot->m_referenceCount > 0, "Cooked shape ownership is inconsistent.");
        if (slot && slot->m_referenceCount > 0)
        {
            --slot->m_referenceCount;
        }
    }

    RuntimeImplementation::CookedShapeSlot* RuntimeImplementation::FindCookedShapeUnlocked(
        const CookedShapeHandle cookedShapeHandle)
    {
        return const_cast<CookedShapeSlot*>(
            static_cast<const RuntimeImplementation&>(*this).FindCookedShapeUnlocked(cookedShapeHandle));
    }

    const RuntimeImplementation::CookedShapeSlot* RuntimeImplementation::FindCookedShapeUnlocked(
        const CookedShapeHandle cookedShapeHandle) const
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(cookedShapeHandle, parts)
            || parts.m_index >= m_cookedShapeSlots.size())
        {
            return nullptr;
        }

        const CookedShapeSlot& slot = m_cookedShapeSlots[parts.m_index];
        if (!slot.m_shape || slot.m_generation != parts.m_generation)
        {
            return nullptr;
        }

        return &slot;
    }

    bool RuntimeImplementation::AcquireCollisionGroup(
        const CollisionGroupConfiguration& configuration,
        JPH::CollisionGroup& collisionGroup)
    {
        if (!configuration.m_filterHandle)
        {
            collisionGroup = JPH::CollisionGroup(
                nullptr,
                configuration.m_groupId.GetValue(),
                configuration.m_subGroupId.GetValue());
            return true;
        }

        AZStd::lock_guard lock(m_groupFilterMutex);
        GroupFilterSlot* slot = FindGroupFilterUnlocked(configuration.m_filterHandle);
        if (!slot
            || (configuration.m_groupId
                && (!configuration.m_subGroupId
                    || configuration.m_subGroupId.GetValue() >= slot->m_subGroupCount)))
        {
            return false;
        }

        ++slot->m_referenceCount;
        collisionGroup = JPH::CollisionGroup(
            slot->m_filter,
            configuration.m_groupId.GetValue(),
            configuration.m_subGroupId.GetValue());
        return true;
    }

    void RuntimeImplementation::ReleaseGroupFilter(
        const GroupFilterHandle filterHandle)
    {
        if (!filterHandle)
        {
            return;
        }

        AZStd::lock_guard lock(m_groupFilterMutex);
        Internal::ResourceHandleParts parts;
        const bool decoded = Internal::DecodeResourceHandle(filterHandle, parts);
        AZ_Assert(decoded && parts.m_index < m_groupFilterSlots.size(), "Group filter ownership is inconsistent.");
        if (!decoded || parts.m_index >= m_groupFilterSlots.size())
        {
            return;
        }

        GroupFilterSlot& slot = m_groupFilterSlots[parts.m_index];
        AZ_Assert(
            slot.m_filter && slot.m_generation == parts.m_generation && slot.m_referenceCount > 0,
            "Group filter ownership is inconsistent.");
        if (slot.m_filter && slot.m_generation == parts.m_generation && slot.m_referenceCount > 0)
        {
            --slot.m_referenceCount;
        }
    }

    bool RuntimeImplementation::GetGroupFilterStateHash(
        const GroupFilterHandle filterHandle,
        AZ::u64& stateHash) const
    {
        AZStd::shared_lock lock(m_groupFilterMutex);
        const GroupFilterSlot* slot = FindGroupFilterUnlocked(filterHandle);
        if (!slot)
        {
            return false;
        }

        stateHash = slot->m_stateHash;
        return true;
    }

    bool RuntimeImplementation::CaptureGroupFilterParticipantState(
        const GroupFilterHandle filterHandle,
        AZStd::vector<AZ::u8>& state,
        AZ::TypeId& typeId,
        AZ::u64& stateHash,
        AZ::u32& version) const
    {
        AZStd::shared_lock lock(m_groupFilterMutex);
        const GroupFilterSlot* slot = FindGroupFilterUnlocked(filterHandle);
        if (!slot)
        {
            return false;
        }

        state.clear();
        typeId = AZ::TypeId::CreateNull();
        stateHash = 0;
        version = 0;
        if (!slot->m_isCustom)
        {
            return true;
        }

        const auto* filter = static_cast<const GroupFilterAdapter*>(slot->m_filter.GetPtr());
        const IGroupFilter* callbacks = filter->GetCallbacks();
        if (!callbacks)
        {
            return false;
        }
        typeId = callbacks->GetStateTypeId();
        version = callbacks->GetStateVersion();
        if (typeId.IsNull())
        {
            typeId = AZ::TypeId::CreateNull();
            version = 0;
            return false;
        }
        const size_t stateByteCount = callbacks->GetStateByteCount();
        if (stateByteCount >= (size_t{1} << 30))
        {
            return false;
        }
        state.resize(stateByteCount + 1);
        state.front() = 1;
        if (!callbacks->CaptureState(AZStd::span(state).subspan(1)))
        {
            state.clear();
            return false;
        }
        stateHash = callbacks->GetStateHash();
        return true;
    }

    bool RuntimeImplementation::PrepareGroupFilterParticipantRestore(
        const GroupFilterHandle filterHandle,
        const AZStd::span<const AZ::u8> state,
        const AZ::TypeId typeId,
        const AZ::u64 stateHash,
        const AZ::u32 version) const
    {
        AZStd::shared_lock lock(m_groupFilterMutex);
        const GroupFilterSlot* slot = FindGroupFilterUnlocked(filterHandle);
        const bool present = !state.empty();
        if (!slot
            || slot->m_isCustom != present)
        {
            return false;
        }
        if (!present)
        {
            return typeId.IsNull()
                && stateHash == 0
                && version == 0;
        }

        const auto* filter = static_cast<const GroupFilterAdapter*>(slot->m_filter.GetPtr());
        const IGroupFilter* callbacks = filter->GetCallbacks();
        if (!callbacks
            || callbacks->GetStateTypeId() != typeId
            || callbacks->GetStateVersion() != version
            || state.front() != 1
            || !callbacks->PrepareRestoreState(state.subspan(1)))
        {
            return false;
        }
        return true;
    }

    bool RuntimeImplementation::CommitGroupFilterParticipantRestore(
        const GroupFilterHandle filterHandle,
        const AZStd::span<const AZ::u8> state,
        const AZ::u64 stateHash)
    {
        AZStd::lock_guard lock(m_groupFilterMutex);
        GroupFilterSlot* slot = FindGroupFilterUnlocked(filterHandle);
        if (!slot || !slot->m_isCustom || state.empty() || state.front() != 1)
        {
            return false;
        }

        auto* filter = static_cast<GroupFilterAdapter*>(slot->m_filter.GetPtr());
        const IGroupFilter* callbacks = filter->GetCallbacks();
        if (!callbacks)
        {
            return false;
        }
        callbacks->CommitRestoreState(state.subspan(1));
        if (callbacks->GetStateHash() != stateHash)
        {
            return false;
        }
        slot->m_stateHash = MixGroupFilterValue(slot->m_subGroupCount)
            ^ MixGroupFilterValue(stateHash);
        return true;
    }

    RuntimeImplementation::GroupFilterSlot* RuntimeImplementation::FindGroupFilterUnlocked(
        const GroupFilterHandle filterHandle)
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(filterHandle, parts)
            || parts.m_index >= m_groupFilterSlots.size())
        {
            return nullptr;
        }

        GroupFilterSlot& slot = m_groupFilterSlots[parts.m_index];
        if (slot.m_generation != parts.m_generation || !slot.m_filter)
        {
            return nullptr;
        }

        return &slot;
    }

    const RuntimeImplementation::GroupFilterSlot* RuntimeImplementation::FindGroupFilterUnlocked(
        const GroupFilterHandle filterHandle) const
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(filterHandle, parts)
            || parts.m_index >= m_groupFilterSlots.size())
        {
            return nullptr;
        }

        const GroupFilterSlot& slot = m_groupFilterSlots[parts.m_index];
        if (slot.m_generation != parts.m_generation || !slot.m_filter)
        {
            return nullptr;
        }

        return &slot;
    }

    bool RuntimeImplementation::AcquirePath(
        const PathHandle pathHandle,
        JPH::RefConst<JPH::PathConstraintPath>& path)
    {
        AZStd::lock_guard lock(m_pathMutex);
        Internal::ResourceHandleParts parts;
        const PathSlot* constPathSlot = FindPathUnlocked(pathHandle);
        if (!constPathSlot
            || !Internal::DecodeResourceHandle(pathHandle, parts)
            || constPathSlot->m_constraintCount == AZStd::numeric_limits<AZ::u32>::max())
        {
            return false;
        }

        PathSlot& pathSlot = m_pathSlots[parts.m_index];
        ++pathSlot.m_constraintCount;
        path = pathSlot.m_path;
        return true;
    }

    void RuntimeImplementation::ReleasePath(
        const PathHandle pathHandle)
    {
        AZStd::lock_guard lock(m_pathMutex);
        Internal::ResourceHandleParts parts;
        const bool decoded = Internal::DecodeResourceHandle(pathHandle, parts);
        AZ_Assert(decoded && parts.m_index < m_pathSlots.size(), "Path reference ownership is inconsistent.");
        if (!decoded || parts.m_index >= m_pathSlots.size())
        {
            return;
        }

        PathSlot& slot = m_pathSlots[parts.m_index];
        AZ_Assert(
            slot.m_path && slot.m_generation == parts.m_generation && slot.m_constraintCount > 0,
            "Path reference ownership is inconsistent.");
        if (slot.m_path && slot.m_generation == parts.m_generation && slot.m_constraintCount > 0)
        {
            --slot.m_constraintCount;
        }
    }

    const RuntimeImplementation::PathSlot* RuntimeImplementation::FindPathUnlocked(
        const PathHandle pathHandle) const
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(pathHandle, parts)
            || parts.m_index >= m_pathSlots.size())
        {
            return nullptr;
        }

        const PathSlot& slot = m_pathSlots[parts.m_index];
        if (slot.m_generation != parts.m_generation || !slot.m_path)
        {
            return nullptr;
        }

        return &slot;
    }

    SoftBodyDefinitionHandle RuntimeImplementation::StoreSoftBodyDefinition(
        JPH::RefConst<JPH::SoftBodySharedSettings> settings,
        AZStd::vector<MaterialHandle> materialHandles)
    {
        AZStd::lock_guard lock(m_softBodyDefinitionMutex);
        AZ::u32 definitionIndex = 0;
        if (!m_freeSoftBodyDefinitionSlots.empty())
        {
            definitionIndex = m_freeSoftBodyDefinitionSlots.back();
            m_freeSoftBodyDefinitionSlots.pop_back();
        }
        else
        {
            if (m_softBodyDefinitionSlots.size() >= Internal::HandlePayloadMask)
            {
                ReleaseMaterials(materialHandles);
                return {};
            }
            definitionIndex = static_cast<AZ::u32>(m_softBodyDefinitionSlots.size());
            m_softBodyDefinitionSlots.emplace_back();
        }

        SoftBodyDefinitionSlot& slot = m_softBodyDefinitionSlots[definitionIndex];
        slot.m_settings = AZStd::move(settings);
        slot.m_materialHandles = AZStd::move(materialHandles);
        return Internal::MakeResourceHandle<SoftBodyDefinitionHandle>(definitionIndex, slot.m_generation);
    }

    bool RuntimeImplementation::AcquireSoftBodyDefinition(
        const SoftBodyDefinitionHandle definitionHandle,
        JPH::RefConst<JPH::SoftBodySharedSettings>& settings)
    {
        AZStd::lock_guard lock(m_softBodyDefinitionMutex);
        Internal::ResourceHandleParts parts;
        const SoftBodyDefinitionSlot* definitionSlot = FindSoftBodyDefinitionUnlocked(definitionHandle);
        if (!definitionSlot
            || !Internal::DecodeResourceHandle(definitionHandle, parts)
            || definitionSlot->m_bodyCount == AZStd::numeric_limits<AZ::u32>::max())
        {
            return false;
        }

        SoftBodyDefinitionSlot& mutableSlot = m_softBodyDefinitionSlots[parts.m_index];
        ++mutableSlot.m_bodyCount;
        settings = mutableSlot.m_settings;
        return true;
    }

    void RuntimeImplementation::ReleaseSoftBodyDefinition(
        const SoftBodyDefinitionHandle definitionHandle)
    {
        AZStd::lock_guard lock(m_softBodyDefinitionMutex);
        Internal::ResourceHandleParts parts;
        const bool decoded = Internal::DecodeResourceHandle(definitionHandle, parts);
        AZ_Assert(
            decoded && parts.m_index < m_softBodyDefinitionSlots.size(),
            "Soft body definition ownership is inconsistent.");
        if (!decoded || parts.m_index >= m_softBodyDefinitionSlots.size())
        {
            return;
        }

        SoftBodyDefinitionSlot& slot = m_softBodyDefinitionSlots[parts.m_index];
        AZ_Assert(
            slot.m_settings && slot.m_generation == parts.m_generation && slot.m_bodyCount > 0,
            "Soft body definition ownership is inconsistent.");
        if (slot.m_settings && slot.m_generation == parts.m_generation && slot.m_bodyCount > 0)
        {
            --slot.m_bodyCount;
        }
    }

    const RuntimeImplementation::SoftBodyDefinitionSlot* RuntimeImplementation::FindSoftBodyDefinitionUnlocked(
        const SoftBodyDefinitionHandle definitionHandle) const
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(definitionHandle, parts)
            || parts.m_index >= m_softBodyDefinitionSlots.size())
        {
            return nullptr;
        }

        const SoftBodyDefinitionSlot& slot = m_softBodyDefinitionSlots[parts.m_index];
        if (slot.m_generation != parts.m_generation || !slot.m_settings)
        {
            return nullptr;
        }

        return &slot;
    }

    bool RuntimeImplementation::AcquireHairDefinition(
        const HairDefinitionHandle definitionHandle,
        JPH::RefConst<JPH::HairSettings>& settings)
    {
        AZStd::lock_guard lock(m_hairDefinitionMutex);
        Internal::ResourceHandleParts parts;
        const HairDefinitionSlot* definitionSlot = FindHairDefinitionUnlocked(definitionHandle);
        if (!definitionSlot
            || !Internal::DecodeResourceHandle(definitionHandle, parts)
            || definitionSlot->m_instanceCount == AZStd::numeric_limits<AZ::u32>::max())
        {
            return false;
        }

        HairDefinitionSlot& mutableSlot = m_hairDefinitionSlots[parts.m_index];
        ++mutableSlot.m_instanceCount;
        settings = mutableSlot.m_settings;
        return true;
    }

    void RuntimeImplementation::ReleaseHairDefinition(
        const HairDefinitionHandle definitionHandle)
    {
        AZStd::lock_guard lock(m_hairDefinitionMutex);
        Internal::ResourceHandleParts parts;
        const bool decoded = Internal::DecodeResourceHandle(definitionHandle, parts);
        AZ_Assert(
            decoded && parts.m_index < m_hairDefinitionSlots.size(),
            "Hair definition ownership is inconsistent.");
        if (!decoded || parts.m_index >= m_hairDefinitionSlots.size())
        {
            return;
        }

        HairDefinitionSlot& slot = m_hairDefinitionSlots[parts.m_index];
        AZ_Assert(
            slot.m_settings && slot.m_generation == parts.m_generation && slot.m_instanceCount > 0,
            "Hair definition ownership is inconsistent.");
        if (slot.m_settings && slot.m_generation == parts.m_generation && slot.m_instanceCount > 0)
        {
            --slot.m_instanceCount;
        }
    }

    const RuntimeImplementation::HairDefinitionSlot* RuntimeImplementation::FindHairDefinitionUnlocked(
        const HairDefinitionHandle definitionHandle) const
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(definitionHandle, parts)
            || parts.m_index >= m_hairDefinitionSlots.size())
        {
            return nullptr;
        }

        const HairDefinitionSlot& slot = m_hairDefinitionSlots[parts.m_index];
        if (slot.m_generation != parts.m_generation || !slot.m_settings)
        {
            return nullptr;
        }

        return &slot;
    }

    bool RuntimeImplementation::EnsureHairRuntime()
    {
        AZStd::lock_guard lock(m_hairDefinitionMutex);
        if (m_hairRuntime)
        {
            return true;
        }

        JPH::ComputeSystemResult computeResult = JPH::CreateComputeSystemCPU();

        if (computeResult.HasError())
        {
            AZ_Error(
                "Jolt",
                false,
                "Failed to create the CPU hair compute system: %s",
                computeResult.GetError().c_str());
            return false;
        }

        auto runtime = AZStd::make_unique<HairRuntime>();
        runtime->m_computeSystem = computeResult.Get();
        const JPH::Ref<JPH::ComputeSystemCPU> cpuComputeSystem =
            JPH::StaticCast<JPH::ComputeSystemCPU>(runtime->m_computeSystem);
        JPH::HairRegisterShaders(cpuComputeSystem);
        runtime->m_shaders = new JPH::HairShaders();
        runtime->m_shaders->Init(runtime->m_computeSystem);
        if (!runtime->m_shaders->mApplyDeltaTransformCS
            || !runtime->m_shaders->mApplyGlobalPoseCS
            || !runtime->m_shaders->mCalculateCollisionPlanesCS
            || !runtime->m_shaders->mCalculateRenderPositionsCS
            || !runtime->m_shaders->mGridAccumulateCS
            || !runtime->m_shaders->mGridClearCS
            || !runtime->m_shaders->mGridNormalizeCS
            || !runtime->m_shaders->mIntegrateCS
            || !runtime->m_shaders->mSkinRootsCS
            || !runtime->m_shaders->mSkinVerticesCS
            || !runtime->m_shaders->mTeleportCS
            || !runtime->m_shaders->mUpdateRootsCS
            || !runtime->m_shaders->mUpdateStrandsCS
            || !runtime->m_shaders->mUpdateVelocityCS
            || !runtime->m_shaders->mUpdateVelocityIntegrateCS)
        {
            AZ_Error("Jolt", false, "The CPU hair compute system did not register every required shader.");
            return false;
        }

        m_hairRuntime = AZStd::move(runtime);
        return true;
    }
} // namespace Jolt
