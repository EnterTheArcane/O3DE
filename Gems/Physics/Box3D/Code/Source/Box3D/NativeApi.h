/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Box3D/Joints.h>
#include <Box3D/ShapeConfiguration.h>
#include <Box3D/Statistics.h>
#include <Box3D/SystemConfiguration.h>

#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/base.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string_view.h>

#include <cstddef>

namespace AZ
{
    class JobContext;
}

namespace Box3D
{
    class IReplay;
    class IDebugRenderer;
    struct DebugDrawSettings;
    struct GeometryCastHit;
    struct GeometryTransform;
    struct NativeShapeConfiguration;

    struct SurfaceMaterial final
    {
        AZ::Vector3 m_tangentVelocity = AZ::Vector3::CreateZero();

        AZ::u64 m_userId = 0;
        AZ::u32 m_debugColor = 0;

        float m_friction = 0.6f;
        float m_restitution = 0.0f;
        float m_rollingResistance = 0.0f;
    };

    struct Version final
    {
        AZ::s32 m_major{};
        AZ::s32 m_minor{};
        AZ::s32 m_revision{};

        constexpr bool operator==(const Version&) const = default;
    };

    struct WorldId final
    {
        AZ::u32 m_value{};

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_value != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        constexpr bool operator==(const WorldId&) const noexcept = default;
    };

    struct BodyId final
    {
        AZ::u64 m_value{};

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_value != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        constexpr bool operator==(const BodyId&) const noexcept = default;
    };

    struct ShapeId final
    {
        AZ::u64 m_value{};

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_value != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        [[nodiscard]]
        constexpr AZ::u32 GetIndex() const noexcept
        {
            const AZ::u64 index = m_value >> 32;
            if (index != 0)
            {
                return static_cast<AZ::u32>(index - 1);
            }

            return AZStd::numeric_limits<AZ::u32>::max();
        }

        constexpr bool operator==(const ShapeId&) const noexcept = default;
    };

    struct JointId final
    {
        AZ::u64 m_value{};

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_value != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        constexpr bool operator==(const JointId&) const noexcept = default;
    };

    enum class JointKind : AZ::u8
    {
        None = 0,
        Distance = 1,
        Filter,
        Motor,
        Parallel,
        Prismatic,
        Revolute,
        Spherical,
        Weld,
        Wheel,
    };

    struct NativeJointConfiguration final
    {
        struct Parallel final
        {
            float m_hertz = 1.0f;
            float m_dampingRatio = 1.0f;
            float m_maxTorque = AZStd::numeric_limits<float>::max();
        };

        struct Distance final
        {
            float m_length = 1.0f;

            float m_lowerSpringForce = -AZStd::numeric_limits<float>::max();
            float m_upperSpringForce = AZStd::numeric_limits<float>::max();
            float m_hertz = 0.0f;
            float m_dampingRatio = 0.0f;

            float m_minLength = 0.0f;
            float m_maxLength = AZStd::numeric_limits<float>::max();

            float m_maxMotorForce = 0.0f;
            float m_motorSpeed = 0.0f;

            bool m_enableSpring = false;
            bool m_enableLimit = false;
            bool m_enableMotor = false;
        };

        struct Motor final
        {
            AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();
            AZ::Vector3 m_angularVelocity = AZ::Vector3::CreateZero();
            float m_maxVelocityForce = 0.0f;
            float m_maxVelocityTorque = 0.0f;

            float m_linearHertz = 0.0f;
            float m_linearDampingRatio = 0.0f;
            float m_maxSpringForce = 0.0f;

            float m_angularHertz = 0.0f;
            float m_angularDampingRatio = 0.0f;
            float m_maxSpringTorque = 0.0f;
        };

        struct Prismatic final
        {
            float m_hertz = 0.0f;
            float m_dampingRatio = 0.0f;
            float m_targetTranslation = 0.0f;

            float m_lowerTranslation = 0.0f;
            float m_upperTranslation = 0.0f;

            float m_maxMotorForce = 0.0f;
            float m_motorSpeed = 0.0f;

            bool m_enableSpring = false;
            bool m_enableLimit = false;
            bool m_enableMotor = false;
        };

        struct Revolute final
        {
            float m_targetAngle = 0.0f;
            float m_hertz = 0.0f;
            float m_dampingRatio = 0.0f;

            float m_lowerAngle = 0.0f;
            float m_upperAngle = 0.0f;

            float m_maxMotorTorque = 0.0f;
            float m_motorSpeed = 0.0f;

            bool m_enableSpring = false;
            bool m_enableLimit = false;
            bool m_enableMotor = false;
        };

        struct Spherical final
        {
            AZ::Quaternion m_targetRotation = AZ::Quaternion::CreateIdentity();
            AZ::Vector3 m_motorVelocity = AZ::Vector3::CreateZero();

            float m_hertz = 0.0f;
            float m_dampingRatio = 0.0f;

            float m_coneAngle = 0.0f;
            float m_lowerTwistAngle = 0.0f;
            float m_upperTwistAngle = 0.0f;

            float m_maxMotorTorque = 0.0f;

            bool m_enableSpring = false;
            bool m_enableConeLimit = false;
            bool m_enableTwistLimit = false;
            bool m_enableMotor = false;
        };

        struct Weld final
        {
            float m_linearHertz = 0.0f;
            float m_angularHertz = 0.0f;

            float m_linearDampingRatio = 0.0f;
            float m_angularDampingRatio = 0.0f;
        };

        struct Wheel final
        {
            float m_suspensionHertz = 1.0f;
            float m_suspensionDampingRatio = 0.7f;
            float m_lowerSuspensionLimit = 0.0f;
            float m_upperSuspensionLimit = 0.0f;

            float m_maxSpinTorque = 0.0f;
            float m_spinSpeed = 0.0f;

            float m_steeringHertz = 1.0f;
            float m_steeringDampingRatio = 0.7f;
            float m_targetSteeringAngle = 0.0f;
            float m_maxSteeringTorque = 0.0f;
            float m_lowerSteeringLimit = 0.0f;
            float m_upperSteeringLimit = 0.0f;

            bool m_enableSuspensionSpring = true;
            bool m_enableSuspensionLimit = false;
            bool m_enableSpinMotor = false;
            bool m_enableSteering = false;
            bool m_enableSteeringLimit = false;
        };

        BodyId m_bodyA;
        BodyId m_bodyB;

        AZ::Transform m_localFrameA = AZ::Transform::CreateIdentity();
        AZ::Transform m_localFrameB = AZ::Transform::CreateIdentity();

        void* m_userData = nullptr;

        float m_forceThreshold = AZStd::numeric_limits<float>::max();
        float m_torqueThreshold = AZStd::numeric_limits<float>::max();

        float m_constraintHertz = 60.0f;
        float m_constraintDampingRatio = 2.0f;

        float m_drawScale = 1.0f;

        Parallel m_parallel;
        Distance m_distance;
        Motor m_motor;
        Prismatic m_prismatic;
        Revolute m_revolute;
        Spherical m_spherical;
        Weld m_weld;
        Wheel m_wheel;

        JointKind m_kind = JointKind::None;
        bool m_collideConnected = false;
    };

    class HullGeometry final
    {
    public:
        AZ_CLASS_ALLOCATOR(HullGeometry, AZ::SystemAllocator);

        ~HullGeometry();
        AZ_DISABLE_COPY_MOVE(HullGeometry);

    private:
        explicit HullGeometry(void* data);

        friend AZStd::unique_ptr<HullGeometry> CreateBoxGeometry(
            const AZ::Vector3& halfExtents,
            const GeometryTransform& transform);

        friend AZStd::unique_ptr<HullGeometry> CreateCylinderGeometry(
            float height,
            float radius,
            AZ::u8 subdivisionCount,
            const GeometryTransform& transform);

        friend AZStd::unique_ptr<HullGeometry> CreateHullGeometry(
            AZStd::span<const AZ::Vector3> points,
            const GeometryTransform& transform);

        friend struct GeometryCastAccess;
        friend ShapeId CreateHullShape(
            BodyId bodyId,
            const NativeShapeConfiguration& configuration,
            const HullGeometry& geometry);

        void* m_data = nullptr;
    };

    class MeshGeometry final
    {
    public:
        AZ_CLASS_ALLOCATOR(MeshGeometry, AZ::SystemAllocator);

        ~MeshGeometry();
        AZ_DISABLE_COPY_MOVE(MeshGeometry);

        [[nodiscard]]
        AZ::u8 GetMaterialIndex(size_t faceIndex) const;

    private:
        explicit MeshGeometry(void* data);

        friend AZStd::unique_ptr<MeshGeometry> CreateMeshGeometry(
            AZStd::span<const AZ::Vector3> vertices,
            AZStd::span<const AZ::u32> indices,
            AZStd::span<const AZ::u8> materialIndices,
            float weldTolerance,
            bool weldVertices,
            bool useMedianSplit,
            bool identifyEdges);

        friend ShapeId CreateMeshShape(
            BodyId bodyId,
            const NativeShapeConfiguration& configuration,
            const MeshGeometry& geometry);

        friend struct GeometryCastAccess;

        void* m_data = nullptr;
    };

    class HeightFieldGeometry final
    {
    public:
        AZ_CLASS_ALLOCATOR(HeightFieldGeometry, AZ::SystemAllocator);

        ~HeightFieldGeometry();
        AZ_DISABLE_COPY_MOVE(HeightFieldGeometry);

        [[nodiscard]]
        AZ::u8 GetMaterialIndex(size_t faceIndex) const;

    private:
        explicit HeightFieldGeometry(void* data);

        friend AZStd::unique_ptr<HeightFieldGeometry> CreateHeightFieldGeometry(
            AZStd::span<const float> heights,
            AZStd::span<const AZ::u8> materialIndices,
            size_t columnCount,
            size_t rowCount,
            const AZ::Vector2& gridResolution,
            const AZ::Vector3& scale,
            float minimumHeight,
            float maximumHeight,
            bool clockwise);

        friend ShapeId CreateHeightFieldShape(
            BodyId bodyId,
            const NativeShapeConfiguration& configuration,
            const HeightFieldGeometry& geometry);

        friend struct GeometryCastAccess;

        void* m_data = nullptr;
    };

    class CompoundGeometry final
    {
    public:
        AZ_CLASS_ALLOCATOR(CompoundGeometry, AZ::SystemAllocator);

        struct Sphere final
        {
            AZ::Vector3 m_center = AZ::Vector3::CreateZero();
            SurfaceMaterial m_material;
            float m_radius = 0.5f;
        };

        struct Capsule final
        {
            AZ::Vector3 m_center1 = AZ::Vector3::CreateZero();
            AZ::Vector3 m_center2 = AZ::Vector3::CreateAxisZ();
            SurfaceMaterial m_material;
            float m_radius = 0.5f;
        };

        struct Hull final
        {
            const HullGeometry* m_geometry = nullptr;
            AZ::Transform m_transform = AZ::Transform::CreateIdentity();
            SurfaceMaterial m_material;
        };

        struct Mesh final
        {
            const MeshGeometry* m_geometry = nullptr;
            AZStd::span<const SurfaceMaterial> m_materials;
            AZ::Transform m_transform = AZ::Transform::CreateIdentity();
            AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
        };

        struct Definition final
        {
            AZStd::span<const Sphere> m_spheres;
            AZStd::span<const Capsule> m_capsules;
            AZStd::span<const Hull> m_hulls;
            AZStd::span<const Mesh> m_meshes;
            AZStd::span<const SurfaceMaterial> m_sourceMaterials;
        };

        ~CompoundGeometry();

        AZ_DISABLE_COPY_MOVE(CompoundGeometry);

        [[nodiscard]]
        AZStd::span<const AZ::u8> GetMaterialSources() const;

        [[nodiscard]]
        AZStd::shared_ptr<const CompoundGeometry> CloneWithMaterials(AZStd::span<const SurfaceMaterial> materials) const;

    private:
        explicit CompoundGeometry(
            void* data,
            AZStd::vector<AZ::u8>&& materialSources);

        friend AZStd::unique_ptr<CompoundGeometry> CreateCompoundGeometry(const Definition& definition);

        friend ShapeId CreateCompoundShape(
            BodyId bodyId,
            const NativeShapeConfiguration& configuration,
            const CompoundGeometry& geometry);

        friend struct GeometryCastAccess;

        void* m_data = nullptr;
        AZStd::vector<AZ::u8> m_materialSources;
    };

    struct NativeSphereGeometry final
    {
        AZ::Vector3 m_center = AZ::Vector3::CreateZero();
        float m_radius = 0.5f;
    };

    struct NativeCapsuleGeometry final
    {
        AZ::Vector3 m_center1 = AZ::Vector3::CreateZero();
        AZ::Vector3 m_center2 = AZ::Vector3::CreateAxisZ();
        float m_radius = 0.25f;
    };

    enum class NativeShapeType : AZ::u8
    {
        Sphere,
        Capsule,
        Hull,
        Mesh,
        Heightfield,
        Compound,
    };

    struct NativeShapeState final
    {
        NativeShapeType m_type = NativeShapeType::Sphere;

        AZ::u64 m_categoryBits = 1;
        AZ::u64 m_maskBits = AZStd::numeric_limits<AZ::u64>::max();
        AZ::s32 m_groupIndex = 0;

        float m_density = 0.0f;
        float m_friction = 0.0f;
        float m_restitution = 0.0f;

        bool m_isSensor = false;
        bool m_enableSensorEvents = false;
        bool m_enableContactEvents = false;
        bool m_enableHitEvents = false;
        bool m_enablePreSolveEvents = false;
    };

    using NativeGeometry = AZStd::variant<
        AZStd::monostate,
        NativeSphereGeometry,
        NativeCapsuleGeometry,
        AZStd::shared_ptr<const HullGeometry>,
        AZStd::shared_ptr<const MeshGeometry>,
        AZStd::shared_ptr<const HeightFieldGeometry>,
        AZStd::shared_ptr<const CompoundGeometry>>;

    struct GeometryTransform final
    {
        GeometryTransform() = default;

        explicit GeometryTransform(
            AZ::Transform transform)
            : m_localTransform(AZStd::move(transform))
        {
            m_scale = AZ::Vector3::CreateOne() * m_localTransform.ExtractUniformScale();
        }

        GeometryTransform(
            AZ::Transform transform,
            const float uniformScale)
            : m_localTransform(AZStd::move(transform))
            , m_postScale(AZ::Vector3::CreateOne() * uniformScale)
        {
            m_scale = AZ::Vector3::CreateOne() * m_localTransform.ExtractUniformScale();
        }

        GeometryTransform(
            AZ::Transform transform,
            const AZ::Vector3& scale,
            const AZ::Vector3& postScale)
            : m_localTransform(AZStd::move(transform))
            , m_scale(scale)
            , m_postScale(postScale)
        {
        }

        AZ::Transform m_localTransform = AZ::Transform::CreateIdentity();
        AZ::Vector3 m_scale = AZ::Vector3::CreateOne();
        AZ::Vector3 m_postScale = AZ::Vector3::CreateOne();
    };

    [[nodiscard]]
    NativeGeometry CookGeometry(
        const ShapeGeometry& geometry,
        const GeometryTransform& transform,
        AZStd::span<const SurfaceMaterial> materials);

    [[nodiscard]]
    ShapeId CreateShape(
        BodyId bodyId,
        const NativeShapeConfiguration& configuration,
        const NativeGeometry& geometry);

    [[nodiscard]]
    bool SetShapeGeometry(
        ShapeId shapeId,
        const NativeGeometry& geometry);

    [[nodiscard]]
    AZ::Aabb GetAabb(const NativeGeometry& geometry);

    [[nodiscard]]
    bool CastRay(
        const NativeGeometry& geometry,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        GeometryCastHit& hit);

    enum class NativeBodyType : AZ::u8
    {
        Static,
        Kinematic,
        Dynamic,
    };

    struct NativeMotionLocks final
    {
        bool m_linearX = false;
        bool m_linearY = false;
        bool m_linearZ = false;

        bool m_angularX = false;
        bool m_angularY = false;
        bool m_angularZ = false;
    };

    struct BodyConfiguration final
    {
        AZ::Transform m_transform = AZ::Transform::CreateIdentity();
        AZ::Vector3 m_linearVelocity = AZ::Vector3::CreateZero();
        AZ::Vector3 m_angularVelocity = AZ::Vector3::CreateZero();

        AZStd::string_view m_name;
        void* m_userData = nullptr;

        float m_linearDamping = 0.0f;
        float m_angularDamping = 0.0f;
        float m_gravityScale = 1.0f;
        float m_sleepThreshold = 0.05f;

        NativeMotionLocks m_motionLocks;
        NativeBodyType m_type = NativeBodyType::Static;

        bool m_enableSleep = true;
        bool m_isAwake = true;
        bool m_isBullet = false;
        bool m_isEnabled = true;
        bool m_allowFastRotation = false;
        bool m_enableContactRecycling = true;
    };

    struct NativeMassProperties final
    {
        AZ::Matrix3x3 m_inertia = AZ::Matrix3x3::CreateIdentity();
        AZ::Vector3 m_center = AZ::Vector3::CreateZero();
        float m_mass = 1.0f;
    };

    struct NativeShapeConfiguration final
    {
        AZStd::span<const SurfaceMaterial> m_materials;
        SurfaceMaterial m_baseMaterial;

        AZ::u64 m_categoryBits = 1;
        AZ::u64 m_maskBits = AZStd::numeric_limits<AZ::u64>::max();
        AZ::s32 m_groupIndex = 0;

        void* m_userData = nullptr;

        float m_density = 1.0f;
        float m_explosionScale = 1.0f;

        bool m_isSensor = false;
        bool m_enableCustomFiltering = false;
        bool m_enableSensorEvents = false;
        bool m_enableContactEvents = true;
        bool m_enableHitEvents = true;
        bool m_enablePreSolveEvents = false;
        bool m_invokeContactCreation = true;
        bool m_updateBodyMass = true;
    };

    struct CastHit final
    {
        ShapeId m_shapeId;
        void* m_shapeUserData = nullptr;

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();

        float m_fraction = 0.0f;
        AZ::u64 m_userMaterialId = 0;

        AZ::s32 m_triangleIndex = -1;
        AZ::s32 m_childIndex = -1;
    };

    struct GeometryCastHit final
    {
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();

        float m_fraction = 0.0f;

        AZ::s32 m_materialIndex = -1;
        AZ::s32 m_triangleIndex = -1;
        AZ::s32 m_childIndex = -1;

        AZ::u64 m_userMaterialId = 0;
    };

    struct ClosestCastHit final
    {
        ShapeId m_shapeId;

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();

        float m_fraction = 0.0f;
        AZ::u64 m_userMaterialId = 0;

        AZ::s32 m_triangleIndex = -1;
        AZ::s32 m_childIndex = -1;

        AZ::u32 m_nodeVisits = 0;
        AZ::u32 m_leafVisits = 0;
    };

    struct TreeVisitStatistics final
    {
        AZ::u32 m_nodeVisits = 0;
        AZ::u32 m_leafVisits = 0;
    };

    using CastCallback = float (*)(const CastHit& hit, void* context);

    struct OverlapCandidate final
    {
        ShapeId m_shapeId;
        void* m_shapeUserData = nullptr;
    };

    using OverlapCallback = bool (*)(const OverlapCandidate& candidate, void* context);
    using OverlapPredicate = bool (*)(const OverlapCandidate& candidate, void* context);

    struct ContactId final
    {
        AZ::u32 m_index = 0;
        AZ::u32 m_world = 0;
        AZ::u32 m_generation = 0;

        [[nodiscard]]
        constexpr bool IsValid() const noexcept
        {
            return m_index != 0;
        }

        constexpr explicit operator bool() const noexcept
        {
            return IsValid();
        }

        constexpr bool operator==(const ContactId&) const noexcept = default;
    };

    enum class ContactTransitionType : AZ::u8
    {
        Begin,
        End,
    };

    struct ContactTransition final
    {
        ContactTransitionType m_type = ContactTransitionType::Begin;

        ContactId m_contactId;
        ShapeId m_shapeIdA;
        ShapeId m_shapeIdB;
    };

    enum class SensorTransitionType : AZ::u8
    {
        Begin,
        End,
    };

    struct SensorTransition final
    {
        SensorTransitionType m_type = SensorTransitionType::Begin;

        ShapeId m_sensorShapeId;
        ShapeId m_visitorShapeId;
    };

    struct BodyMove final
    {
        BodyId m_bodyId;
        void* m_bodyUserData = nullptr;

        AZ::Transform m_transform = AZ::Transform::CreateIdentity();

        bool m_fellAsleep = false;
    };

    struct NativeJointThresholdEvent final
    {
        JointId m_jointId;
        void* m_jointUserData = nullptr;
        JointHandle m_jointHandle;
    };

    struct ContactHit final
    {
        ShapeId m_shapeIdA;
        ShapeId m_shapeIdB;

        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();

        float m_approachSpeed = 0.0f;
        AZ::u64 m_userMaterialIdA = 0;
        AZ::u64 m_userMaterialIdB = 0;

        AZ::s32 m_triangleIndex = -1;
        AZ::s32 m_childIndexA = -1;
        AZ::s32 m_childIndexB = -1;
    };

    struct NativeContactPoint final
    {
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();
        AZ::Vector3 m_impulse = AZ::Vector3::CreateZero();

        float m_separation = 0.0f;
        AZ::s32 m_triangleIndex = -1;
    };

    struct ContactData final
    {
        ShapeId m_shapeIdA;
        ShapeId m_shapeIdB;
        BodyId m_bodyIdA;
        BodyId m_bodyIdB;

        void* m_shapeUserDataA = nullptr;
        void* m_shapeUserDataB = nullptr;
        void* m_bodyUserDataA = nullptr;
        void* m_bodyUserDataB = nullptr;

        AZ::s32 m_childIndexA = -1;
        AZ::s32 m_childIndexB = -1;

        size_t m_firstPoint = 0;
        size_t m_pointCount = 0;
    };

    struct ShapeData final
    {
        ShapeId m_shapeId;
        BodyId m_bodyId;

        void* m_shapeUserData = nullptr;
        void* m_bodyUserData = nullptr;
    };

    struct SensorOverlapData final
    {
        ShapeData m_sensor;
        ShapeData m_visitor;
    };

    struct MoverPlane final
    {
        ShapeId m_shapeId;
        BodyId m_bodyId;

        void* m_shapeUserData = nullptr;
        void* m_bodyUserData = nullptr;

        AZ::Vector3 m_point = AZ::Vector3::CreateZero();
        AZ::Vector3 m_normal = AZ::Vector3::CreateZero();

        float m_push = 0.0f;
    };

    struct MoverResult final
    {
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        AZ::Vector3 m_clippedVelocity = AZ::Vector3::CreateZero();

        AZ::u32 m_planeCount = 0;
        AZ::u32 m_requiredPlaneCount = 0;
        AZ::u32 m_iterationCount = 0;

        bool m_overflow = false;
        bool m_invalidPlane = false;
    };

    class MoverScratch final
    {
    public:
        explicit MoverScratch(size_t maximumPlaneCount = 256);

        ~MoverScratch();

        AZ_DISABLE_COPY_MOVE(MoverScratch);

        [[nodiscard]]
        AZStd::span<const MoverPlane> GetPlanes() const;

    private:
        friend bool MoveCapsule(
            WorldId,
            const AZ::Vector3&,
            const AZ::Vector3&,
            const AZ::Vector3&,
            float,
            const AZ::Vector3&,
            float,
            AZ::u64,
            AZ::u64,
            BodyId,
            BodyId,
            AZ::u32,
            float,
            MoverScratch&,
            MoverResult&);

        void* m_data = nullptr;
    };

    class JointScratch final
    {
    public:
        JointScratch();

        ~JointScratch();

        AZ_DISABLE_COPY_MOVE(JointScratch);

        [[nodiscard]]
        AZStd::span<const JointId> GetJoints() const;

    private:
        friend bool GetBodyJoints(
            BodyId,
            JointScratch&);

        void* m_data = nullptr;
    };

    class ContactScratch final
    {
    public:
        ContactScratch();

        ~ContactScratch();

        AZ_DISABLE_COPY_MOVE(ContactScratch);

    private:
        friend bool GetTouchingContacts(
            BodyId,
            ContactScratch&,
            AZStd::vector<ContactData>&,
            AZStd::vector<NativeContactPoint>&);

        friend bool GetTouchingContacts(
            ShapeId,
            ContactScratch&,
            AZStd::vector<ContactData>&,
            AZStd::vector<NativeContactPoint>&);

        void* m_data = nullptr;
    };

    class SensorScratch final
    {
    public:
        SensorScratch();

        ~SensorScratch();

        AZ_DISABLE_COPY_MOVE(SensorScratch);

    private:
        friend bool GetSensorOverlaps(
            BodyId,
            SensorScratch&,
            AZStd::vector<SensorOverlapData>&);

        friend bool GetSensorOverlaps(
            ShapeId,
            SensorScratch&,
            AZStd::vector<SensorOverlapData>&);

        void* m_data = nullptr;
    };

    class IContactCallbacks
    {
    public:
        virtual ~IContactCallbacks() = default;

        virtual bool BeforeSolve(
            ShapeId shapeA,
            ShapeId shapeB,
            const AZ::Vector3& position,
            const AZ::Vector3& normal) = 0;

        virtual bool ShouldCollide(
            ShapeId shapeA,
            ShapeId shapeB) = 0;
    };

    //! Resolves provider material handles before invoking application-defined mixing policy.
    class IMaterialCallbacks
    {
    public:
        virtual ~IMaterialCallbacks() = default;

        [[nodiscard]]
        virtual float MixFriction(
            float valueA,
            AZ::u64 materialIdA,
            float valueB,
            AZ::u64 materialIdB) const = 0;

        [[nodiscard]]
        virtual float MixRestitution(
            float valueA,
            AZ::u64 materialIdA,
            float valueB,
            AZ::u64 materialIdB) const = 0;
    };

    class Recording final
    {
    public:
        ~Recording();

        AZ_DISABLE_COPY_MOVE(Recording);

    private:
        explicit Recording(void* data);

        friend AZStd::unique_ptr<Recording> CreateRecording(size_t initialCapacityBytes);

        friend bool StartRecording(
            WorldId worldId,
            Recording& recording);

        friend bool StopRecording(
            WorldId worldId,
            Recording& recording,
            AZStd::vector<AZ::u8>& data);

        void* m_data = nullptr;
    };

    struct NativeTaskContext final
    {
        AZ::JobContext* m_jobContext = nullptr;
        AZStd::atomic<AZ::u32> m_workerCount{1};
    };

    struct NativeWorldConfiguration final
    {
        AZ::Vector3 m_gravity = AZ::Vector3::CreateZero();

        float m_restitutionThreshold = 1.0f;
        float m_hitEventThreshold = 1.0f;

        float m_contactHertz = 30.0f;
        float m_contactDampingRatio = 10.0f;
        float m_contactSpeed = 3.0f;
        float m_contactRecycleDistance = 0.05f;

        float m_maximumLinearSpeed = 400.0f;

        AZ::u32 m_workerCount = 1;
        NativeTaskContext* m_taskContext = nullptr;

        MaterialMixCallback m_frictionCallback = nullptr;
        MaterialMixCallback m_restitutionCallback = nullptr;

        AZ::u32 m_staticShapeCapacity = 0;
        AZ::u32 m_dynamicShapeCapacity = 0;
        AZ::u32 m_staticBodyCapacity = 0;
        AZ::u32 m_dynamicBodyCapacity = 0;
        AZ::u32 m_contactCapacity = 0;

        bool m_enableSleep = true;
        bool m_enableContinuous = true;
        bool m_enableWarmStarting = true;
        bool m_enableSpeculative = true;
    };

    [[nodiscard]]
    Version GetVersion();

    void SetMaterialCallbacks(IMaterialCallbacks* callbacks);

    [[nodiscard]]
    AZStd::unique_ptr<Recording> CreateRecording(size_t initialCapacityBytes);

    [[nodiscard]]
    bool StartRecording(
        WorldId worldId,
        Recording& recording);

    [[nodiscard]]
    bool StopRecording(
        WorldId worldId,
        Recording& recording,
        AZStd::vector<AZ::u8>& data);

    [[nodiscard]]
    bool ValidateRecording(
        AZStd::span<const AZ::u8> data,
        AZ::u32 workerCount);

    [[nodiscard]]
    AZStd::unique_ptr<IReplay> CreateReplay(
        AZStd::span<const AZ::u8> data,
        AZ::u32 workerCount);

    void SetLengthUnitsPerMeter(float lengthUnitsPerMeter);

    void SetStallWarningThreshold(float seconds);

    [[nodiscard]]
    bool DrawWorld(
        WorldId worldId,
        const DebugDrawSettings& settings,
        IDebugRenderer& renderer);

    [[nodiscard]]
    WorldId CreateWorld(const NativeWorldConfiguration& configuration);

    void DestroyWorld(WorldId worldId);

    [[nodiscard]]
    bool IsValid(WorldId worldId);

    void Step(
        WorldId worldId,
        float timeStep,
        AZ::u32 subStepCount);

    void ReconfigureWorld(
        WorldId worldId,
        const NativeWorldConfiguration& configuration);

    void SetContactCallbacks(
        WorldId worldId,
        IContactCallbacks* callbacks,
        bool enableCustomFilterCallback,
        bool enablePreSolveCallback);

    void SetGravity(
        WorldId worldId,
        const AZ::Vector3& gravity);

    [[nodiscard]]
    AZ::Vector3 GetGravity(WorldId worldId);

    [[nodiscard]]
    AZ::Aabb GetWorldAabb(WorldId worldId);

    [[nodiscard]]
    bool RebuildStaticTree(WorldId worldId);

    [[nodiscard]]
    bool Explode(
        WorldId worldId,
        const AZ::Vector3& position,
        AZ::u64 maskBits,
        float radius,
        float falloff,
        float impulsePerArea);

    [[nodiscard]]
    bool GetStatistics(
        WorldId worldId,
        StatisticsFlags flags,
        WorldStatistics& statistics);

    enum class StepEventFlags : AZ::u8
    {
        None = 0,
        BodyMoves = 1 << 0,
        Sensors = 1 << 1,
        Contacts = 1 << 2,
        ContactHits = 1 << 3,
        Joints = 1 << 4,
        All = 0x1f,
    };

    AZ_DEFINE_ENUM_BITWISE_OPERATORS(StepEventFlags)

    void GetStepEvents(
        WorldId worldId,
        StepEventFlags flags,
        AZStd::vector<BodyMove>& bodyMoves,
        AZStd::vector<SensorTransition>& sensorTransitions,
        AZStd::vector<ContactTransition>& contactTransitions,
        AZStd::vector<ContactHit>& contactHits,
        AZStd::vector<NativeJointThresholdEvent>& jointEvents);

    [[nodiscard]]
    bool GetContactData(
        ContactId contactId,
        ContactData& contactData,
        AZStd::vector<NativeContactPoint>& points);

    [[nodiscard]]
    bool GetTouchingContacts(
        BodyId bodyId,
        ContactScratch& scratch,
        AZStd::vector<ContactData>& contacts,
        AZStd::vector<NativeContactPoint>& points);

    [[nodiscard]]
    bool GetTouchingContacts(
        ShapeId shapeId,
        ContactScratch& scratch,
        AZStd::vector<ContactData>& contacts,
        AZStd::vector<NativeContactPoint>& points);

    [[nodiscard]]
    bool GetShapeData(
        ShapeId shapeId,
        ShapeData& shapeData);

    [[nodiscard]]
    bool GetSensorOverlaps(
        BodyId bodyId,
        SensorScratch& scratch,
        AZStd::vector<SensorOverlapData>& overlaps);

    [[nodiscard]]
    bool GetSensorOverlaps(
        ShapeId shapeId,
        SensorScratch& scratch,
        AZStd::vector<SensorOverlapData>& overlaps);

    [[nodiscard]]
    bool MoveCapsule(
        WorldId worldId,
        const AZ::Vector3& position,
        const AZ::Vector3& center1,
        const AZ::Vector3& center2,
        float radius,
        const AZ::Vector3& translation,
        float deltaTime,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        BodyId ignoredBodyIdA,
        BodyId ignoredBodyIdB,
        AZ::u32 maximumIterations,
        float minimumTranslation,
        MoverScratch& scratch,
        MoverResult& result);

    void CastRay(
        WorldId worldId,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        CastCallback callback,
        void* context);

    [[nodiscard]]
    bool CastRayClosest(
        WorldId worldId,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        ClosestCastHit& hit);

    [[nodiscard]]
    bool CastRay(
        BodyId bodyId,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        CastHit& hit);

    [[nodiscard]]
    bool CastRaySphere(
        const AZ::Vector3& center,
        float radius,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        GeometryCastHit& hit);

    [[nodiscard]]
    bool CastRayCapsule(
        const AZ::Vector3& center1,
        const AZ::Vector3& center2,
        float radius,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        GeometryCastHit& hit);

    [[nodiscard]]
    bool CastRay(
        const HullGeometry& geometry,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        GeometryCastHit& hit);

    [[nodiscard]]
    bool CastRay(
        const MeshGeometry& geometry,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        GeometryCastHit& hit);

    [[nodiscard]]
    bool CastRay(
        const HeightFieldGeometry& geometry,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        GeometryCastHit& hit);

    [[nodiscard]]
    bool CastRay(
        const CompoundGeometry& geometry,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        GeometryCastHit& hit);

    [[nodiscard]]
    AZ::Aabb GetAabb(
        const CompoundGeometry& geometry,
        const AZ::Transform& transform = AZ::Transform::CreateIdentity());

    void CastShape(
        WorldId worldId,
        const AZ::Vector3& origin,
        AZStd::span<const AZ::Vector3> points,
        float radius,
        const AZ::Vector3& translation,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        CastCallback callback,
        void* context);

    TreeVisitStatistics OverlapShape(
        WorldId worldId,
        const AZ::Vector3& origin,
        AZStd::span<const AZ::Vector3> points,
        float radius,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        OverlapCallback callback,
        void* context);

    TreeVisitStatistics OverlapAabb(
        WorldId worldId,
        const AZ::Aabb& bounds,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        OverlapCallback callback,
        void* context);

    [[nodiscard]]
    bool CastShape(
        BodyId bodyId,
        const AZ::Vector3& origin,
        AZStd::span<const AZ::Vector3> points,
        float radius,
        const AZ::Vector3& translation,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        CastHit& hit);

    [[nodiscard]]
    bool OverlapShape(
        BodyId bodyId,
        const AZ::Vector3& origin,
        AZStd::span<const AZ::Vector3> points,
        float radius,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        OverlapPredicate predicate = nullptr,
        void* context = nullptr);

    [[nodiscard]]
    BodyId CreateBody(
        WorldId worldId,
        const BodyConfiguration& configuration);

    void DestroyBody(BodyId bodyId);

    [[nodiscard]]
    bool IsValid(BodyId bodyId);

    [[nodiscard]]
    NativeBodyType GetBodyType(BodyId bodyId);

    [[nodiscard]]
    bool SetBodyType(
        BodyId bodyId,
        NativeBodyType type);

    void SetBodyName(
        BodyId bodyId,
        AZ::Name name);

    [[nodiscard]]
    AZ::Transform GetBodyTransform(BodyId bodyId);

    void SetBodyTransform(
        BodyId bodyId,
        const AZ::Transform& transform);

    [[nodiscard]]
    AZ::Vector3 GetBodyLocalPoint(
        BodyId bodyId,
        const AZ::Vector3& worldPoint);

    [[nodiscard]]
    AZ::Vector3 GetBodyWorldPoint(
        BodyId bodyId,
        const AZ::Vector3& localPoint);

    [[nodiscard]]
    AZ::Vector3 GetBodyLocalVector(
        BodyId bodyId,
        const AZ::Vector3& worldVector);

    [[nodiscard]]
    AZ::Vector3 GetBodyWorldVector(
        BodyId bodyId,
        const AZ::Vector3& localVector);

    [[nodiscard]]
    AZ::Vector3 GetLinearVelocity(BodyId bodyId);

    void SetLinearVelocity(
        BodyId bodyId,
        const AZ::Vector3& velocity);

    [[nodiscard]]
    AZ::Vector3 GetAngularVelocity(BodyId bodyId);

    void SetAngularVelocity(
        BodyId bodyId,
        const AZ::Vector3& velocity);

    [[nodiscard]]
    AZ::Vector3 GetLinearVelocityAtLocalPoint(
        BodyId bodyId,
        const AZ::Vector3& localPoint);

    [[nodiscard]]
    AZ::Vector3 GetLinearVelocityAtWorldPoint(
        BodyId bodyId,
        const AZ::Vector3& worldPoint);

    void SetKinematicTarget(
        BodyId bodyId,
        const AZ::Transform& transform,
        float timeStep,
        bool wake);

    void ApplyLinearImpulse(
        BodyId bodyId,
        const AZ::Vector3& impulse,
        bool wake);

    void ApplyLinearImpulse(
        BodyId bodyId,
        const AZ::Vector3& impulse,
        const AZ::Vector3& worldPoint,
        bool wake);

    void ApplyAngularImpulse(
        BodyId bodyId,
        const AZ::Vector3& impulse,
        bool wake);

    void ApplyForce(
        BodyId bodyId,
        const AZ::Vector3& force,
        bool wake);

    void ApplyForce(
        BodyId bodyId,
        const AZ::Vector3& force,
        const AZ::Vector3& worldPoint,
        bool wake);

    void ApplyTorque(
        BodyId bodyId,
        const AZ::Vector3& torque,
        bool wake);

    [[nodiscard]]
    NativeMotionLocks GetMotionLocks(BodyId bodyId);

    void SetMotionLocks(
        BodyId bodyId,
        const NativeMotionLocks& locks);

    [[nodiscard]]
    NativeMassProperties GetMassProperties(BodyId bodyId);

    [[nodiscard]]
    NativeMassProperties GetShapeMassProperties(ShapeId shapeId);

    void SetMassProperties(
        BodyId bodyId,
        const NativeMassProperties& properties);

    void ApplyMassFromShapes(BodyId bodyId);

    [[nodiscard]]
    AZ::Matrix3x3 GetWorldInverseInertia(BodyId bodyId);

    [[nodiscard]]
    AZ::Vector3 GetWorldCenterOfMass(BodyId bodyId);

    [[nodiscard]]
    float GetLinearDamping(BodyId bodyId);

    void SetLinearDamping(
        BodyId bodyId,
        float damping);

    [[nodiscard]]
    float GetAngularDamping(BodyId bodyId);
    void SetAngularDamping(
        BodyId bodyId,
        float damping);
    [[nodiscard]]
    float GetGravityScale(BodyId bodyId);
    void SetGravityScale(
        BodyId bodyId,
        float scale);
    [[nodiscard]]
    bool IsAwake(BodyId bodyId);
    void SetAwake(
        BodyId bodyId,
        bool awake);
    [[nodiscard]]
    float GetSleepThreshold(BodyId bodyId);
    void SetSleepThreshold(
        BodyId bodyId,
        float threshold);
    [[nodiscard]]
    bool IsBodyEnabled(BodyId bodyId);
    void SetBodyEnabled(
        BodyId bodyId,
        bool enabled);
    void SetBodyHitEventsEnabled(
        BodyId bodyId,
        bool enabled);
    void SetBullet(
        BodyId bodyId,
        bool enabled);
    [[nodiscard]]
    bool IsBullet(BodyId bodyId);
    void SetBodySleepEnabled(
        BodyId bodyId,
        bool enabled);
    [[nodiscard]]
    bool IsBodySleepEnabled(BodyId bodyId);
    void SetBodyContactRecyclingEnabled(
        BodyId bodyId,
        bool enabled);
    [[nodiscard]]
    bool IsBodyContactRecyclingEnabled(BodyId bodyId);
    [[nodiscard]]
    AZ::Aabb GetBodyAabb(BodyId bodyId);
    [[nodiscard]]
    bool GetBodyClosestPoint(
        BodyId bodyId,
        const AZ::Vector3& target,
        AZ::Vector3& position,
        float& distance);
    [[nodiscard]]
    bool GetBodyJoints(
        BodyId bodyId,
        JointScratch& scratch);

    [[nodiscard]]
    JointId CreateJoint(
        WorldId worldId,
        const NativeJointConfiguration& configuration);
    [[nodiscard]]
    bool UpdateJoint(
        JointId jointId,
        const NativeJointConfiguration& configuration);
    void DestroyJoint(
        JointId jointId,
        bool wakeAttachedBodies = true);
    void WakeJointBodies(JointId jointId);
    [[nodiscard]]
    bool IsValid(JointId jointId);
    [[nodiscard]]
    void* GetJointUserData(JointId jointId);
    [[nodiscard]]
    AZ::Vector3 GetJointConstraintForce(JointId jointId);
    [[nodiscard]]
    AZ::Vector3 GetJointConstraintTorque(JointId jointId);
    [[nodiscard]]
    float GetJointLinearSeparation(JointId jointId);
    [[nodiscard]]
    float GetJointAngularSeparation(JointId jointId);
    [[nodiscard]]
    JointState GetJointState(
        JointId jointId,
        JointKind kind);

    [[nodiscard]]
    ShapeId CreateSphereShape(
        BodyId bodyId,
        const NativeShapeConfiguration& configuration,
        const AZ::Vector3& center,
        float radius);
    [[nodiscard]]
    ShapeId CreateCapsuleShape(
        BodyId bodyId,
        const NativeShapeConfiguration& configuration,
        const AZ::Vector3& center1,
        const AZ::Vector3& center2,
        float radius);
    [[nodiscard]]
    bool SetSphereShape(
        ShapeId shapeId,
        const AZ::Vector3& center,
        float radius);
    [[nodiscard]]
    bool SetCapsuleShape(
        ShapeId shapeId,
        const AZ::Vector3& center1,
        const AZ::Vector3& center2,
        float radius);
    [[nodiscard]]
    ShapeId CreateBoxShape(
        BodyId bodyId,
        const NativeShapeConfiguration& configuration,
        const AZ::Vector3& halfExtents,
        const AZ::Transform& localTransform);
    [[nodiscard]]
    ShapeId CreateHullShape(
        BodyId bodyId,
        const NativeShapeConfiguration& configuration,
        AZStd::span<const AZ::Vector3> points,
        const AZ::Vector3& scale,
        const AZ::Transform& localTransform);
    [[nodiscard]]
    AZStd::unique_ptr<HullGeometry> CreateBoxGeometry(
        const AZ::Vector3& halfExtents,
        const GeometryTransform& transform);
    [[nodiscard]]
    AZStd::unique_ptr<HullGeometry> CreateCylinderGeometry(
        float height,
        float radius,
        AZ::u8 subdivisionCount,
        const GeometryTransform& transform);
    [[nodiscard]]
    AZStd::unique_ptr<HullGeometry> CreateHullGeometry(
        AZStd::span<const AZ::Vector3> points,
        const GeometryTransform& transform);
    [[nodiscard]]
    ShapeId CreateHullShape(
        BodyId bodyId,
        const NativeShapeConfiguration& configuration,
        const HullGeometry& geometry);
    [[nodiscard]]
    bool SetHullShape(
        ShapeId shapeId,
        const HullGeometry& geometry);
    [[nodiscard]]
    AZStd::unique_ptr<MeshGeometry> CreateMeshGeometry(
        AZStd::span<const AZ::Vector3> vertices,
        AZStd::span<const AZ::u32> indices,
        AZStd::span<const AZ::u8> materialIndices = {},
        float weldTolerance = 0.0f,
        bool weldVertices = false,
        bool useMedianSplit = false,
        bool identifyEdges = true);
    [[nodiscard]]
    ShapeId CreateMeshShape(
        BodyId bodyId,
        const NativeShapeConfiguration& configuration,
        const MeshGeometry& geometry);
    [[nodiscard]]
    bool SetMeshShape(
        ShapeId shapeId,
        const MeshGeometry& geometry);
    [[nodiscard]]
    AZStd::unique_ptr<HeightFieldGeometry> CreateHeightFieldGeometry(
        AZStd::span<const float> heights,
        AZStd::span<const AZ::u8> materialIndices,
        size_t columnCount,
        size_t rowCount,
        const AZ::Vector2& gridResolution,
        const AZ::Vector3& scale,
        float minimumHeight,
        float maximumHeight,
        bool clockwise);
    [[nodiscard]]
    ShapeId CreateHeightFieldShape(
        BodyId bodyId,
        const NativeShapeConfiguration& configuration,
        const HeightFieldGeometry& geometry);
    [[nodiscard]]
    AZStd::unique_ptr<CompoundGeometry> CreateCompoundGeometry(const CompoundGeometry::Definition& definition);
    [[nodiscard]]
    ShapeId CreateCompoundShape(
        BodyId bodyId,
        const NativeShapeConfiguration& configuration,
        const CompoundGeometry& geometry);
    void DestroyShape(
        ShapeId shapeId,
        bool updateBodyMass = true);
    [[nodiscard]]
    bool IsValid(ShapeId shapeId);
    [[nodiscard]]
    bool GetShapeState(
        ShapeId shapeId,
        NativeShapeState& state);
    void SetShapeFilter(
        ShapeId shapeId,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        AZ::s32 groupIndex);
    [[nodiscard]]
    bool SetShapeDensity(
        ShapeId shapeId,
        float density,
        bool updateBodyMass);
    [[nodiscard]]
    bool SetShapeFriction(
        ShapeId shapeId,
        float friction);
    [[nodiscard]]
    bool SetShapeRestitution(
        ShapeId shapeId,
        float restitution);
    [[nodiscard]]
    size_t GetShapeMaterialCount(ShapeId shapeId);
    [[nodiscard]]
    bool GetShapeMaterials(
        ShapeId shapeId,
        AZStd::span<SurfaceMaterial> materials);
    void SetShapeSurfaceMaterial(
        ShapeId shapeId,
        const SurfaceMaterial& material);
    [[nodiscard]]
    bool SetShapeMaterials(
        ShapeId shapeId,
        AZStd::span<const SurfaceMaterial> materials);
    [[nodiscard]]
    bool SetShapeEventSubscriptions(
        ShapeId shapeId,
        bool sensorEvents,
        bool contactEvents,
        bool hitEvents,
        bool preSolveEvents);
    [[nodiscard]]
    AZ::Aabb GetShapeAabb(ShapeId shapeId);
    [[nodiscard]]
    bool GetShapeClosestPoint(
        ShapeId shapeId,
        const AZ::Vector3& target,
        AZ::Vector3& position);
    [[nodiscard]]
    bool CastRay(
        ShapeId shapeId,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        GeometryCastHit& hit);
    [[nodiscard]]
    bool ApplyWind(
        ShapeId shapeId,
        const AZ::Vector3& velocity,
        float drag,
        float lift,
        float maximumSpeed,
        bool wake);
} // namespace Box3D
