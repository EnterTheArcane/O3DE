/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/NativeApi.h>

#include <Box3D/Allocator.h>
#include <Box3D/Diagnostics.h>
#include <Box3D/FloatEnvironment.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Jobs/JobCompletion.h>
#include <AzCore/Jobs/JobContext.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/string/fixed_string.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <box3d/box3d.h>

#include <cmath>
#include <cstdint>

namespace Box3D
{
    Recording::Recording(void* data)
        : m_data(data)
    {
    }

    Recording::~Recording()
    {
        b3DestroyRecording(static_cast<b3Recording*>(m_data));
    }

    HullGeometry::HullGeometry(void* data)
        : m_data(data)
    {
    }

    HullGeometry::~HullGeometry()
    {
        if (m_data != nullptr)
        {
            b3DestroyHull(static_cast<b3HullData*>(m_data));
        }
    }

    MeshGeometry::MeshGeometry(void* data)
        : m_data(data)
    {
    }

    MeshGeometry::~MeshGeometry()
    {
        if (m_data != nullptr)
        {
            b3DestroyMesh(static_cast<b3MeshData*>(m_data));
        }
    }

    AZ::u8 MeshGeometry::GetMaterialIndex(size_t faceIndex) const
    {
        const auto* mesh = static_cast<const b3MeshData*>(m_data);
        return mesh != nullptr && faceIndex < aznumeric_cast<size_t>(mesh->triangleCount) ? b3GetMeshMaterialIndices(mesh)[faceIndex] : 0;
    }

    HeightFieldGeometry::HeightFieldGeometry(void* data)
        : m_data(data)
    {
    }

    HeightFieldGeometry::~HeightFieldGeometry()
    {
        if (m_data != nullptr)
        {
            b3DestroyHeightField(static_cast<b3HeightFieldData*>(m_data));
        }
    }

    AZ::u8 HeightFieldGeometry::GetMaterialIndex(size_t faceIndex) const
    {
        const auto* heightField = static_cast<const b3HeightFieldData*>(m_data);
        const size_t faceCount = heightField != nullptr
            ? 2 * aznumeric_cast<size_t>(heightField->columnCount - 1) * aznumeric_cast<size_t>(heightField->rowCount - 1)
            : 0;
        const AZ::u8* materialIndices = heightField != nullptr ? b3GetHeightFieldMaterialIndices(heightField) : nullptr;
        return faceIndex < faceCount && materialIndices != nullptr ? materialIndices[faceIndex >> 1] : 0;
    }

    CompoundGeometry::CompoundGeometry(void* data, AZStd::vector<AZ::u8>&& materialSources)
        : m_data(data)
        , m_materialSources(AZStd::move(materialSources))
    {
    }

    CompoundGeometry::~CompoundGeometry()
    {
        if (m_data != nullptr)
        {
            b3DestroyCompound(static_cast<b3CompoundData*>(m_data));
        }
    }

    AZStd::span<const AZ::u8> CompoundGeometry::GetMaterialSources() const
    {
        return m_materialSources;
    }

    struct GeometryCastAccess final
    {
        static const b3HullData* Get(const HullGeometry& geometry)
        {
            return static_cast<const b3HullData*>(geometry.m_data);
        }

        static const b3MeshData* Get(const MeshGeometry& geometry)
        {
            return static_cast<const b3MeshData*>(geometry.m_data);
        }

        static const b3HeightFieldData* Get(const HeightFieldGeometry& geometry)
        {
            return static_cast<const b3HeightFieldData*>(geometry.m_data);
        }

        static const b3CompoundData* Get(const CompoundGeometry& geometry)
        {
            return static_cast<const b3CompoundData*>(geometry.m_data);
        }
    };

    namespace
    {
        constexpr float MinimumGeometryScale = 0.01f;

        bool IsValidGeometryScale(const AZ::Vector3& scale, bool allowNegative = true)
        {
            return scale.IsFinite() && AZStd::abs(scale.GetX()) >= MinimumGeometryScale &&
                AZStd::abs(scale.GetY()) >= MinimumGeometryScale && AZStd::abs(scale.GetZ()) >= MinimumGeometryScale &&
                (allowNegative || scale.GetMinElement() > 0.0f);
        }

        bool IsRigidTransform(const AZ::Transform& transform)
        {
            const AZ::Quaternion rotation = transform.GetRotation();
            return transform.GetTranslation().IsFinite() && rotation.IsFinite() &&
                AZ::IsClose(rotation.GetLengthSq(), 1.0f, 20.0f * AZ::Constants::FloatEpsilon) &&
                AZ::IsClose(transform.GetUniformScale(), 1.0f, AZ::Constants::Tolerance);
        }

        bool IsIdentityScale(const AZ::Vector3& scale)
        {
            return scale.IsClose(AZ::Vector3::CreateOne(), AZ::Constants::Tolerance);
        }

        bool IsIdentityPose(const AZ::Transform& transform)
        {
            return transform.GetTranslation().IsZero(AZ::Constants::Tolerance) &&
                transform.GetRotation().IsIdentity(AZ::Constants::Tolerance);
        }

        AZ::Vector3 TransformGeometryPoint(const GeometryTransform& transform, const AZ::Vector3& point)
        {
            return transform.m_postScale * transform.m_localTransform.TransformPoint(transform.m_scale * point);
        }

        AZ::Vector3 TransformGeometryVector(const GeometryTransform& transform, const AZ::Vector3& vector)
        {
            return transform.m_postScale * transform.m_localTransform.TransformVector(transform.m_scale * vector);
        }

        bool GetSimilarityScale(const GeometryTransform& transform, float& scale)
        {
            const AZ::Vector3 basisX = TransformGeometryVector(transform, AZ::Vector3::CreateAxisX());
            const AZ::Vector3 basisY = TransformGeometryVector(transform, AZ::Vector3::CreateAxisY());
            const AZ::Vector3 basisZ = TransformGeometryVector(transform, AZ::Vector3::CreateAxisZ());
            const float lengthSq = basisX.GetLengthSq();
            const float tolerance = AZStd::max(1.0f, lengthSq) * 1.0e-4f;
            if (!AZ::IsFiniteFloat(lengthSq) || lengthSq < MinimumGeometryScale * MinimumGeometryScale ||
                !AZ::IsClose(basisY.GetLengthSq(), lengthSq, tolerance) || !AZ::IsClose(basisZ.GetLengthSq(), lengthSq, tolerance) ||
                !AZ::IsClose(basisX.Dot(basisY), 0.0f, tolerance) || !AZ::IsClose(basisX.Dot(basisZ), 0.0f, tolerance) ||
                !AZ::IsClose(basisY.Dot(basisZ), 0.0f, tolerance))
            {
                return false;
            }
            scale = std::sqrt(lengthSq);
            return true;
        }

        AZStd::atomic<IMaterialCallbacks*> s_materialCallbacks{};

        float MixFriction(float valueA, uint64_t materialIdA, float valueB, uint64_t materialIdB)
        {
            if (const IMaterialCallbacks* callbacks = s_materialCallbacks.load(AZStd::memory_order_acquire))
            {
                return callbacks->MixFriction(valueA, materialIdA, valueB, materialIdB);
            }
            return std::sqrt(valueA * valueB);
        }

        float MixRestitution(float valueA, uint64_t materialIdA, float valueB, uint64_t materialIdB)
        {
            if (const IMaterialCallbacks* callbacks = s_materialCallbacks.load(AZStd::memory_order_acquire))
            {
                return callbacks->MixRestitution(valueA, materialIdA, valueB, materialIdB);
            }
            return AZStd::max(valueA, valueB);
        }

        AZ::Vector3 FromNative(b3Vec3 value);
        AZ::Vector3 FromNativePosition(b3Pos value);
        void* CreateReplayDebugShape(const b3DebugShape* source, void* context);
        void DestroyReplayDebugShape(void* shape, void* context);

        struct NativeTask final
            : public AZ::Job
        {
            AZ_CLASS_ALLOCATOR(NativeTask, AZ::ThreadPoolAllocator);

            NativeTask(b3TaskCallback* task, void* taskContext, const char* name, AZ::JobContext* jobContext)
                : AZ::Job(false, jobContext)
                , m_task(task)
                , m_taskContext(taskContext)
                , m_completion(jobContext)
                , m_name(name != nullptr ? name : "Box3D task")
            {
                SetDependent(&m_completion);
            }

            void Process() override
            {
                AZ_PROFILE_SCOPE(Physics, m_name.c_str());
                const DeterministicFloatScope floatScope;
                m_task(m_taskContext);
            }

            b3TaskCallback* m_task;
            void* m_taskContext;
            AZ::JobCompletion m_completion;
            AZStd::fixed_string<64> m_name;
        };

        void* EnqueueNativeTask(b3TaskCallback* task, void* taskContext, void* userContext, const char* taskName)
        {
            auto* nativeTaskContext = static_cast<NativeTaskContext*>(userContext);
            if (nativeTaskContext == nullptr || nativeTaskContext->m_jobContext == nullptr ||
                nativeTaskContext->m_workerCount.load(AZStd::memory_order_acquire) <= 1)
            {
                const DeterministicFloatScope floatScope;
                task(taskContext);
                return nullptr;
            }

            auto* nativeTask = aznew NativeTask(task, taskContext, taskName, nativeTaskContext->m_jobContext);
            nativeTask->Start();
            return nativeTask;
        }

        void FinishNativeTask(void* task, [[maybe_unused]] void* userContext)
        {
            auto* nativeTask = static_cast<NativeTask*>(task);
            if (nativeTask == nullptr)
            {
                return;
            }

            nativeTask->m_completion.StartAndWaitForCompletion();
            delete nativeTask;
        }

        bool FilterContact(b3ShapeId shapeA, b3ShapeId shapeB, void* context)
        {
            return static_cast<IContactCallbacks*>(context)->ShouldCollide({ b3StoreShapeId(shapeA) }, { b3StoreShapeId(shapeB) });
        }

        bool FilterPreSolveContact(b3ShapeId shapeA, b3ShapeId shapeB, b3Pos position, b3Vec3 normal, void* context)
        {
            return static_cast<IContactCallbacks*>(context)->BeforeSolve(
                { b3StoreShapeId(shapeA) }, { b3StoreShapeId(shapeB) }, FromNativePosition(position), FromNative(normal));
        }

        void* CreateNativeDebugShape(const b3DebugShape* shape, void* context)
        {
            return CreateReplayDebugShape(shape, context);
        }

        void DestroyNativeDebugShape(void* shape, void* context)
        {
            DestroyReplayDebugShape(shape, context);
        }

        void* AllocateNativeMemory(int32_t size, int32_t alignment)
        {
            return size > 0 && alignment > 0 ? AZ::AllocatorInstance<NativeAllocator>::Get().Allocate(
                                                   aznumeric_cast<size_t>(size), aznumeric_cast<size_t>(alignment), 0, "Box3D Native")
                                             : nullptr;
        }

        void FreeNativeMemory(void* memory)
        {
            AZ::AllocatorInstance<NativeAllocator>::Get().DeAllocate(memory);
        }

        int ReportNativeAssertion(
            [[maybe_unused]] const char* condition, [[maybe_unused]] const char* fileName, [[maybe_unused]] int lineNumber)
        {
            AZ_Error(
                "Box3D",
                false,
                "Native assertion failed: %s (%s:%d)",
                condition != nullptr ? condition : "<unknown>",
                fileName != nullptr ? fileName : "<unknown>",
                lineNumber);
            return 1;
        }

        void ReportNativeWarning([[maybe_unused]] const char* message)
        {
            AZ_Warning("Box3D", false, "%s", message != nullptr ? message : "<empty native warning>");
        }

        void InstallNativeCallbacks()
        {
            [[maybe_unused]] static const bool callbacksInstalled = []
            {
                b3SetLengthUnitsPerMeter(1.0f);
                b3SetAllocator(AllocateNativeMemory, FreeNativeMemory);
                b3SetAssertFcn(ReportNativeAssertion);
                b3SetLogFcn(ReportNativeWarning);
                return true;
            }();
        }

        b3Vec3 ToNative(const AZ::Vector3& value)
        {
            return { value.GetX(), -value.GetZ(), value.GetY() };
        }

        b3Pos ToNativePosition(const AZ::Vector3& value)
        {
            return { value.GetX(), -value.GetZ(), value.GetY() };
        }

        AZ::Vector3 FromNative(b3Vec3 value)
        {
            return { value.x, value.z, -value.y };
        }

        AZ::Vector3 FromNativePosition(b3Pos value)
        {
            return { aznumeric_cast<float>(value.x), aznumeric_cast<float>(value.z), aznumeric_cast<float>(-value.y) };
        }

        b3Vec3 ToNativeScale(const AZ::Vector3& value)
        {
            return { value.GetX(), value.GetZ(), value.GetY() };
        }

        AZ::Aabb FromNative(b3AABB value)
        {
            return AZ::Aabb::CreateFromMinMax(
                AZ::Vector3(
                    aznumeric_cast<float>(value.lowerBound.x),
                    aznumeric_cast<float>(value.lowerBound.z),
                    aznumeric_cast<float>(-value.upperBound.y)),
                AZ::Vector3(
                    aznumeric_cast<float>(value.upperBound.x),
                    aznumeric_cast<float>(value.upperBound.z),
                    aznumeric_cast<float>(-value.lowerBound.y)));
        }

        bool ConvertCastOutput(const b3CastOutput& output, GeometryCastHit& hit)
        {
            if (!output.hit)
            {
                return false;
            }

            hit = { FromNative(output.point), FromNative(output.normal), output.fraction,
                    output.materialIndex,     output.triangleIndex,      output.childIndex };
            return true;
        }

        b3Quat ToNative(const AZ::Quaternion& value)
        {
            return { { value.GetX(), -value.GetZ(), value.GetY() }, value.GetW() };
        }

        AZ::Quaternion FromNative(b3Quat value)
        {
            return AZ::Quaternion(value.v.x, value.v.z, -value.v.y, value.s);
        }

        b3WorldTransform ToNative(const AZ::Transform& value)
        {
            return { ToNativePosition(value.GetTranslation()), ToNative(value.GetRotation()) };
        }

        AZ::Transform FromNative(b3WorldTransform value)
        {
            return AZ::Transform::CreateFromQuaternionAndTranslation(FromNative(value.q), FromNativePosition(value.p));
        }

        AZ::Transform FromNativeLocalTransform(b3Transform value)
        {
            return AZ::Transform::CreateFromQuaternionAndTranslation(FromNative(value.q), FromNative(value.p));
        }

        b3Transform ToNativeJointFrame(const AZ::Transform& value)
        {
            const AZ::Quaternion basis(0.7071067811865475f, 0.0f, 0.0f, 0.7071067811865475f);
            const AZ::Quaternion rotation = basis * value.GetRotation();
            return { ToNative(value.GetTranslation()), { { rotation.GetX(), rotation.GetY(), rotation.GetZ() }, rotation.GetW() } };
        }

        b3Matrix3 ToNative(const AZ::Matrix3x3& value)
        {
            return { ToNative(value.GetColumn(0)), -ToNative(value.GetColumn(2)), ToNative(value.GetColumn(1)) };
        }

        AZ::Matrix3x3 FromNative(b3Matrix3 value)
        {
            return AZ::Matrix3x3::CreateFromColumns(FromNative(value.cx), FromNative(value.cz), -FromNative(value.cy));
        }

        b3BodyType ToNative(NativeBodyType type)
        {
            switch (type)
            {
            case NativeBodyType::Kinematic:
                return b3_kinematicBody;
            case NativeBodyType::Dynamic:
                return b3_dynamicBody;
            case NativeBodyType::Static:
            default:
                return b3_staticBody;
            }
        }

        NativeBodyType FromNative(b3BodyType type)
        {
            switch (type)
            {
            case b3_kinematicBody:
                return NativeBodyType::Kinematic;
            case b3_dynamicBody:
                return NativeBodyType::Dynamic;
            case b3_staticBody:
            default:
                return NativeBodyType::Static;
            }
        }

        b3SurfaceMaterial ToNative(const SurfaceMaterial& material)
        {
            b3SurfaceMaterial nativeMaterial = b3DefaultSurfaceMaterial();
            nativeMaterial.friction = material.m_friction;
            nativeMaterial.restitution = material.m_restitution;
            nativeMaterial.rollingResistance = material.m_rollingResistance;
            nativeMaterial.tangentVelocity = ToNative(material.m_tangentVelocity);
            nativeMaterial.userMaterialId = material.m_userId;
            nativeMaterial.customColor = material.m_debugColor;
            return nativeMaterial;
        }

        struct NativeShapeDefinition final
        {
            b3ShapeDef m_value = b3DefaultShapeDef();
            AZStd::vector<b3SurfaceMaterial> m_materials;
        };

        NativeShapeDefinition ToNative(const NativeShapeConfiguration& configuration)
        {
            NativeShapeDefinition result;
            result.m_materials.reserve(configuration.m_materials.size());
            for (const SurfaceMaterial& material : configuration.m_materials)
            {
                result.m_materials.push_back(ToNative(material));
            }

            result.m_value.userData = configuration.m_userData;
            result.m_value.materials = result.m_materials.data();
            result.m_value.materialCount = aznumeric_cast<int>(result.m_materials.size());
            result.m_value.baseMaterial = ToNative(configuration.m_baseMaterial);
            result.m_value.density = configuration.m_density;
            result.m_value.explosionScale = configuration.m_explosionScale;
            result.m_value.filter.categoryBits = configuration.m_categoryBits;
            result.m_value.filter.maskBits = configuration.m_maskBits;
            result.m_value.filter.groupIndex = configuration.m_groupIndex;
            result.m_value.isSensor = configuration.m_isSensor;
            result.m_value.enableCustomFiltering = configuration.m_enableCustomFiltering;
            result.m_value.enableSensorEvents = configuration.m_enableSensorEvents;
            result.m_value.enableContactEvents = configuration.m_enableContactEvents;
            result.m_value.enableHitEvents = configuration.m_enableHitEvents;
            result.m_value.enablePreSolveEvents = configuration.m_enablePreSolveEvents;
            result.m_value.invokeContactCreation = configuration.m_invokeContactCreation;
            result.m_value.updateBodyMass = configuration.m_updateBodyMass;
            return result;
        }

        struct CastContext final
        {
            CastCallback m_callback;
            void* m_context;
        };

        float OnCastHit(
            b3ShapeId shapeId,
            b3Pos point,
            b3Vec3 normal,
            float fraction,
            uint64_t userMaterialId,
            int triangleIndex,
            int childIndex,
            void* context)
        {
            auto* castContext = static_cast<CastContext*>(context);
            const CastHit hit{ { b3StoreShapeId(shapeId) },
                               b3Shape_GetUserData(shapeId),
                               FromNativePosition(point),
                               FromNative(normal),
                               fraction,
                               userMaterialId,
                               triangleIndex,
                               childIndex };
            return castContext->m_callback(hit, castContext->m_context);
        }

        struct OverlapContext final
        {
            OverlapCallback m_callback;
            void* m_context;
        };

        bool OnOverlapHit(b3ShapeId shapeId, void* context)
        {
            auto* overlapContext = static_cast<OverlapContext*>(context);
            const OverlapCandidate candidate{ { b3StoreShapeId(shapeId) }, b3Shape_GetUserData(shapeId) };
            return overlapContext->m_callback(candidate, overlapContext->m_context);
        }

        struct BodyOverlapContext final
        {
            b3BodyId m_bodyId;
            OverlapPredicate m_predicate;
            void* m_context;
            bool m_overlapped = false;
        };

        bool OnBodyOverlapHit(b3ShapeId shapeId, void* context)
        {
            auto* overlapContext = static_cast<BodyOverlapContext*>(context);
            const b3BodyId bodyId = b3Shape_GetBody(shapeId);
            if (b3StoreBodyId(bodyId) != b3StoreBodyId(overlapContext->m_bodyId))
            {
                return true;
            }

            const OverlapCandidate candidate{ { b3StoreShapeId(shapeId) }, b3Shape_GetUserData(shapeId) };
            if (overlapContext->m_predicate == nullptr || overlapContext->m_predicate(candidate, overlapContext->m_context))
            {
                overlapContext->m_overlapped = true;
                return false;
            }
            return true;
        }

        struct MoverEntry final
        {
            b3CollisionPlane m_plane;
            MoverPlane m_result;
        };

        struct MoverScratchData final
        {
            explicit MoverScratchData(size_t maximumPlaneCount)
                : m_planeCapacity(maximumPlaneCount)
            {
                m_entries.reserve(maximumPlaneCount);
                m_solverPlanes.reserve(maximumPlaneCount);
                m_results.reserve(maximumPlaneCount);
            }

            AZStd::vector<MoverEntry> m_entries;
            AZStd::vector<b3CollisionPlane> m_solverPlanes;
            AZStd::vector<MoverPlane> m_results;
            b3Pos m_origin = b3Pos_zero;
            BodyId m_ignoredBodyIdA;
            BodyId m_ignoredBodyIdB;
            size_t m_planeCapacity = 0;
            size_t m_requiredPlaneCount = 0;
            bool m_overflow = false;
            bool m_invalidPlane = false;
        };

        struct JointScratchData final
        {
            AZStd::vector<b3JointId> m_nativeJoints;
            AZStd::vector<JointId> m_joints;
        };

        bool CollectMoverPlanes(b3ShapeId shapeId, const b3PlaneResult* planeResults, int planeCount, void* context)
        {
            auto& data = *static_cast<MoverScratchData*>(context);
            const b3BodyId bodyId = b3Shape_GetBody(shapeId);
            const ShapeId storedShapeId{ b3StoreShapeId(shapeId) };
            const BodyId storedBodyId{ b3StoreBodyId(bodyId) };
            if (storedBodyId == data.m_ignoredBodyIdA || storedBodyId == data.m_ignoredBodyIdB)
            {
                return true;
            }
            void* shapeUserData = b3Shape_GetUserData(shapeId);
            void* bodyUserData = b3Body_GetUserData(bodyId);
            data.m_requiredPlaneCount += aznumeric_cast<size_t>(AZStd::max(planeCount, 0));
            if (b3Shape_IsSensor(shapeId) || !b3Body_IsEnabled(bodyId))
            {
                return true;
            }
            for (int planeIndex = 0; planeIndex < planeCount; ++planeIndex)
            {
                if (data.m_entries.size() == data.m_planeCapacity)
                {
                    data.m_overflow = true;
                    continue;
                }
                b3PlaneResult plane = planeResults[planeIndex];
                if (!std::isfinite(plane.plane.normal.x) || !std::isfinite(plane.plane.normal.y) || !std::isfinite(plane.plane.normal.z) ||
                    !std::isfinite(plane.plane.offset) || !std::isfinite(plane.point.x) || !std::isfinite(plane.point.y) ||
                    !std::isfinite(plane.point.z) || b3LengthSquared(plane.plane.normal) == 0.0f)
                {
                    data.m_invalidPlane = true;
                    continue;
                }
                plane.plane.normal.x = plane.plane.normal.x == 0.0f ? 0.0f : plane.plane.normal.x;
                plane.plane.normal.y = plane.plane.normal.y == 0.0f ? 0.0f : plane.plane.normal.y;
                plane.plane.normal.z = plane.plane.normal.z == 0.0f ? 0.0f : plane.plane.normal.z;
                plane.plane.offset = plane.plane.offset == 0.0f ? 0.0f : plane.plane.offset;
                plane.point.x = plane.point.x == 0.0f ? 0.0f : plane.point.x;
                plane.point.y = plane.point.y == 0.0f ? 0.0f : plane.point.y;
                plane.point.z = plane.point.z == 0.0f ? 0.0f : plane.point.z;
                data.m_entries.push_back(
                    { { plane.plane, (AZStd::numeric_limits<float>::max)(), 0.0f, true },
                      { storedShapeId,
                        storedBodyId,
                        shapeUserData,
                        bodyUserData,
                        FromNativePosition(b3OffsetPos(data.m_origin, plane.point)),
                        FromNative(plane.plane.normal),
                        0.0f } });
            }
            return true;
        }

        bool FilterMoverShape(b3ShapeId shapeId, void* context)
        {
            const b3BodyId bodyId = b3Shape_GetBody(shapeId);
            const auto& data = *static_cast<const MoverScratchData*>(context);
            const BodyId storedBodyId{ b3StoreBodyId(bodyId) };
            return storedBodyId != data.m_ignoredBodyIdA && storedBodyId != data.m_ignoredBodyIdB && !b3Shape_IsSensor(shapeId) &&
                b3Body_IsEnabled(bodyId);
        }

        bool LessMoverEntry(const MoverEntry& left, const MoverEntry& right)
        {
            if (left.m_result.m_bodyId.m_value != right.m_result.m_bodyId.m_value)
            {
                return left.m_result.m_bodyId.m_value < right.m_result.m_bodyId.m_value;
            }
            if (left.m_result.m_shapeId.m_value != right.m_result.m_shapeId.m_value)
            {
                return left.m_result.m_shapeId.m_value < right.m_result.m_shapeId.m_value;
            }
            const b3Plane& leftPlane = left.m_plane.plane;
            const b3Plane& rightPlane = right.m_plane.plane;
            if (leftPlane.normal.x != rightPlane.normal.x)
            {
                return leftPlane.normal.x < rightPlane.normal.x;
            }
            if (leftPlane.normal.y != rightPlane.normal.y)
            {
                return leftPlane.normal.y < rightPlane.normal.y;
            }
            if (leftPlane.normal.z != rightPlane.normal.z)
            {
                return leftPlane.normal.z < rightPlane.normal.z;
            }
            if (leftPlane.offset != rightPlane.offset)
            {
                return leftPlane.offset < rightPlane.offset;
            }
            if (left.m_result.m_point.GetX() != right.m_result.m_point.GetX())
            {
                return left.m_result.m_point.GetX() < right.m_result.m_point.GetX();
            }
            if (left.m_result.m_point.GetY() != right.m_result.m_point.GetY())
            {
                return left.m_result.m_point.GetY() < right.m_result.m_point.GetY();
            }
            return left.m_result.m_point.GetZ() < right.m_result.m_point.GetZ();
        }

        b3ShapeProxy ToNativeProxy(AZStd::span<const AZ::Vector3> points, float radius, b3Vec3* nativePoints)
        {
            const size_t pointCount = AZStd::min(points.size(), size_t{ B3_MAX_SHAPE_CAST_POINTS });
            for (size_t pointIndex = 0; pointIndex < pointCount; ++pointIndex)
            {
                nativePoints[pointIndex] = ToNative(points[pointIndex]);
            }
            return { nativePoints, aznumeric_cast<int>(pointCount), radius };
        }

        constexpr AZ::u32 ToUnsigned(int value)
        {
            return value > 0 ? static_cast<AZ::u32>(value) : 0;
        }

        struct DebugContext final
        {
            IDebugRenderer& m_renderer;
        };

        enum class ReplayDebugPrimitiveType : AZ::u8
        {
            Sphere,
            Capsule,
            Triangles,
        };

        struct ReplayDebugPrimitive final
        {
            ReplayDebugPrimitiveType m_type = ReplayDebugPrimitiveType::Triangles;
            AZ::Transform m_localTransform = AZ::Transform::CreateIdentity();
            AZ::Vector3 m_pointA = AZ::Vector3::CreateZero();
            AZ::Vector3 m_pointB = AZ::Vector3::CreateZero();
            AZStd::vector<AZ::Vector3> m_vertices;
            AZStd::vector<AZ::u32> m_indices;
            float m_radius = 0.0f;
        };

        struct ReplayDebugShape final
        {
            AZStd::vector<ReplayDebugPrimitive> m_primitives;
        };

        AZ::Color GetDebugColor(b3HexColor color)
        {
            const AZ::u32 packed = static_cast<AZ::u32>(color);
            return AZ::Color::CreateFromRgba(
                static_cast<AZ::u8>(packed >> 16), static_cast<AZ::u8>(packed >> 8), static_cast<AZ::u8>(packed), 255);
        }

        DebugMaterialPreset GetDebugMaterial(b3HexColor color)
        {
            return static_cast<DebugMaterialPreset>(
                AZStd::min(static_cast<AZ::u32>(color) >> 24, static_cast<AZ::u32>(DebugMaterialPreset::Metallic)));
        }

        bool DrawDebugShape(void* shape, b3WorldTransform transform, b3HexColor color, void* context)
        {
            const auto* debugShape = static_cast<const ReplayDebugShape*>(shape);
            if (debugShape == nullptr)
            {
                return false;
            }

            IDebugRenderer& renderer = static_cast<DebugContext*>(context)->m_renderer;
            const AZ::Transform worldTransform = FromNative(transform);
            const AZ::Color debugColor = GetDebugColor(color);
            const DebugMaterialPreset debugMaterial = GetDebugMaterial(color);
            for (const ReplayDebugPrimitive& primitive : debugShape->m_primitives)
            {
                const AZ::Transform primitiveTransform = worldTransform * primitive.m_localTransform;
                switch (primitive.m_type)
                {
                case ReplayDebugPrimitiveType::Sphere:
                    renderer.DrawSphere(primitiveTransform.TransformPoint(primitive.m_pointA), primitive.m_radius, debugColor, 1.0f);
                    break;
                case ReplayDebugPrimitiveType::Capsule:
                    renderer.DrawCapsule(
                        primitiveTransform.TransformPoint(primitive.m_pointA),
                        primitiveTransform.TransformPoint(primitive.m_pointB),
                        primitive.m_radius,
                        debugColor,
                        1.0f);
                    break;
                case ReplayDebugPrimitiveType::Triangles:
                    if (!renderer.DrawTriangles(primitive.m_vertices, primitive.m_indices, primitiveTransform, debugColor, debugMaterial))
                    {
                        return false;
                    }
                    break;
                }
            }
            return true;
        }

        void* CreateReplayDebugShape(const b3DebugShape* source, [[maybe_unused]] void* context)
        {
            if (source == nullptr)
            {
                return nullptr;
            }

            auto debugShape = AZStd::make_unique<ReplayDebugShape>();
            const auto appendSphere = [&debugShape](const b3Sphere& sphere, const AZ::Transform& localTransform)
            {
                ReplayDebugPrimitive& primitive = debugShape->m_primitives.emplace_back();
                primitive.m_type = ReplayDebugPrimitiveType::Sphere;
                primitive.m_localTransform = localTransform;
                primitive.m_pointA = FromNative(sphere.center);
                primitive.m_radius = sphere.radius;
            };
            const auto appendCapsule = [&debugShape](const b3Capsule& capsule, const AZ::Transform& localTransform)
            {
                ReplayDebugPrimitive& primitive = debugShape->m_primitives.emplace_back();
                primitive.m_type = ReplayDebugPrimitiveType::Capsule;
                primitive.m_localTransform = localTransform;
                primitive.m_pointA = FromNative(capsule.center1);
                primitive.m_pointB = FromNative(capsule.center2);
                primitive.m_radius = capsule.radius;
            };
            const auto appendHull = [&debugShape](const b3HullData* hull, const AZ::Transform& localTransform)
            {
                if (hull == nullptr || hull->vertexCount < 4 || hull->vertexOffset <= 0 || hull->edgeOffset <= 0 || hull->faceOffset <= 0)
                {
                    return false;
                }

                ReplayDebugPrimitive& primitive = debugShape->m_primitives.emplace_back();
                primitive.m_localTransform = localTransform;
                const auto* nativeVertices = reinterpret_cast<const b3Vec3*>(reinterpret_cast<const AZ::u8*>(hull) + hull->vertexOffset);
                const auto* edges = reinterpret_cast<const b3HullHalfEdge*>(reinterpret_cast<const AZ::u8*>(hull) + hull->edgeOffset);
                const auto* faces = reinterpret_cast<const b3HullFace*>(reinterpret_cast<const AZ::u8*>(hull) + hull->faceOffset);
                primitive.m_vertices.reserve(aznumeric_cast<size_t>(hull->vertexCount));
                for (int index = 0; index < hull->vertexCount; ++index)
                {
                    primitive.m_vertices.push_back(FromNative(nativeVertices[index]));
                }
                for (int faceIndex = 0; faceIndex < hull->faceCount; ++faceIndex)
                {
                    const AZ::u8 firstEdge = faces[faceIndex].edge;
                    AZ::u8 edge = edges[firstEdge].next;
                    AZ::u8 nextEdge = edges[edge].next;
                    while (nextEdge != firstEdge)
                    {
                        primitive.m_indices.insert(
                            primitive.m_indices.end(), { edges[firstEdge].origin, edges[edge].origin, edges[nextEdge].origin });
                        edge = nextEdge;
                        nextEdge = edges[edge].next;
                    }
                }
                return !primitive.m_indices.empty();
            };
            const auto appendMesh = [&debugShape](const b3MeshData* mesh, const b3Vec3& scale, const AZ::Transform& localTransform)
            {
                if (mesh == nullptr || mesh->vertexCount < 3 || mesh->triangleCount < 1 || mesh->vertexOffset <= 0 ||
                    mesh->triangleOffset <= 0)
                {
                    return false;
                }

                ReplayDebugPrimitive& primitive = debugShape->m_primitives.emplace_back();
                primitive.m_localTransform = localTransform;
                const auto* nativeVertices = reinterpret_cast<const b3Vec3*>(reinterpret_cast<const AZ::u8*>(mesh) + mesh->vertexOffset);
                const auto* nativeTriangles =
                    reinterpret_cast<const b3MeshTriangle*>(reinterpret_cast<const AZ::u8*>(mesh) + mesh->triangleOffset);
                primitive.m_vertices.reserve(aznumeric_cast<size_t>(mesh->vertexCount));
                for (int index = 0; index < mesh->vertexCount; ++index)
                {
                    const b3Vec3 vertex = nativeVertices[index];
                    primitive.m_vertices.push_back(FromNative(b3Vec3{ vertex.x * scale.x, vertex.y * scale.y, vertex.z * scale.z }));
                }
                primitive.m_indices.reserve(3 * aznumeric_cast<size_t>(mesh->triangleCount));
                for (int index = 0; index < mesh->triangleCount; ++index)
                {
                    const b3MeshTriangle& triangle = nativeTriangles[index];
                    primitive.m_indices.insert(
                        primitive.m_indices.end(),
                        { aznumeric_cast<AZ::u32>(triangle.index1),
                          aznumeric_cast<AZ::u32>(triangle.index2),
                          aznumeric_cast<AZ::u32>(triangle.index3) });
                }
                return true;
            };

            switch (source->type)
            {
            case b3_sphereShape:
                appendSphere(*source->sphere, AZ::Transform::CreateIdentity());
                break;
            case b3_capsuleShape:
                appendCapsule(*source->capsule, AZ::Transform::CreateIdentity());
                break;
            case b3_hullShape:
                appendHull(source->hull, AZ::Transform::CreateIdentity());
                break;
            case b3_meshShape:
                appendMesh(source->mesh->data, source->mesh->scale, AZ::Transform::CreateIdentity());
                break;
            case b3_heightShape:
                {
                    const b3HeightFieldData& heightField = *source->heightField;
                    if (heightField.columnCount < 2 || heightField.rowCount < 2 || heightField.heightsOffset <= 0)
                    {
                        return nullptr;
                    }
                    const auto* heights =
                        reinterpret_cast<const AZ::u16*>(reinterpret_cast<const AZ::u8*>(&heightField) + heightField.heightsOffset);
                    const auto* materials = heightField.materialOffset > 0
                        ? reinterpret_cast<const AZ::u8*>(reinterpret_cast<const AZ::u8*>(&heightField) + heightField.materialOffset)
                        : nullptr;
                    ReplayDebugPrimitive& primitive = debugShape->m_primitives.emplace_back();
                    primitive.m_vertices.reserve(aznumeric_cast<size_t>(heightField.columnCount) * heightField.rowCount);
                    for (int row = 0; row < heightField.rowCount; ++row)
                    {
                        for (int column = 0; column < heightField.columnCount; ++column)
                        {
                            const size_t index = aznumeric_cast<size_t>(row) * heightField.columnCount + column;
                            const float height = heightField.minHeight + heightField.heightScale * heights[index];
                            primitive.m_vertices.push_back(FromNative(
                                b3Vec3{ column * heightField.scale.x, height * heightField.scale.y, row * heightField.scale.z }));
                        }
                    }
                    primitive.m_indices.reserve(6 * aznumeric_cast<size_t>(heightField.columnCount - 1) * (heightField.rowCount - 1));
                    for (int row = 0; row + 1 < heightField.rowCount; ++row)
                    {
                        for (int column = 0; column + 1 < heightField.columnCount; ++column)
                        {
                            const size_t materialIndex = aznumeric_cast<size_t>(row) * (heightField.columnCount - 1) + column;
                            if (materials != nullptr && materials[materialIndex] == B3_HEIGHT_FIELD_HOLE)
                            {
                                continue;
                            }
                            const AZ::u32 upperLeft = aznumeric_cast<AZ::u32>(row * heightField.columnCount + column);
                            const AZ::u32 upperRight = upperLeft + 1;
                            const AZ::u32 lowerLeft = upperLeft + aznumeric_cast<AZ::u32>(heightField.columnCount);
                            const AZ::u32 lowerRight = lowerLeft + 1;
                            if (heightField.clockwise)
                            {
                                primitive.m_indices.insert(
                                    primitive.m_indices.end(), { upperLeft, lowerRight, upperRight, upperLeft, lowerLeft, lowerRight });
                            }
                            else
                            {
                                primitive.m_indices.insert(
                                    primitive.m_indices.end(), { upperLeft, upperRight, lowerRight, upperLeft, lowerRight, lowerLeft });
                            }
                        }
                    }
                    break;
                }
            case b3_compoundShape:
                {
                    const b3CompoundData& compound = *source->compound;
                    debugShape->m_primitives.reserve(
                        aznumeric_cast<size_t>(compound.sphereCount + compound.capsuleCount + compound.hullCount + compound.meshCount));
                    const auto* spheres =
                        reinterpret_cast<const b3CompoundSphere*>(reinterpret_cast<const AZ::u8*>(&compound) + compound.sphereOffset);
                    for (int index = 0; index < compound.sphereCount; ++index)
                    {
                        appendSphere(spheres[index].sphere, AZ::Transform::CreateIdentity());
                    }
                    const auto* capsules =
                        reinterpret_cast<const b3CompoundCapsule*>(reinterpret_cast<const AZ::u8*>(&compound) + compound.capsuleOffset);
                    for (int index = 0; index < compound.capsuleCount; ++index)
                    {
                        appendCapsule(capsules[index].capsule, AZ::Transform::CreateIdentity());
                    }
                    const auto* hulls =
                        reinterpret_cast<const b3CompoundHull*>(reinterpret_cast<const AZ::u8*>(&compound) + compound.hullOffset);
                    for (int index = 0; index < compound.hullCount; ++index)
                    {
                        appendHull(hulls[index].hull, FromNativeLocalTransform(hulls[index].transform));
                    }
                    const auto* meshes =
                        reinterpret_cast<const b3CompoundMesh*>(reinterpret_cast<const AZ::u8*>(&compound) + compound.meshOffset);
                    for (int index = 0; index < compound.meshCount; ++index)
                    {
                        appendMesh(meshes[index].meshData, meshes[index].scale, FromNativeLocalTransform(meshes[index].transform));
                    }
                    break;
                }
            default:
                return nullptr;
            }

            if (debugShape->m_primitives.empty())
            {
                return nullptr;
            }
            return debugShape.release();
        }

        void DestroyReplayDebugShape(void* shape, [[maybe_unused]] void* context)
        {
            delete static_cast<ReplayDebugShape*>(shape);
        }

        bool DrawReplayDebugShape(void* shape, b3WorldTransform transform, b3HexColor color, void* context)
        {
            return DrawDebugShape(shape, transform, color, context);
        }

        void DrawDebugLine(b3Pos start, b3Pos end, b3HexColor color, void* context)
        {
            static_cast<DebugContext*>(context)->m_renderer.DrawLine(
                FromNativePosition(start), FromNativePosition(end), GetDebugColor(color));
        }

        void DrawDebugTransform(b3WorldTransform transform, void* context)
        {
            static_cast<DebugContext*>(context)->m_renderer.DrawTransform(FromNative(transform));
        }

        void DrawDebugPoint(b3Pos position, float size, b3HexColor color, void* context)
        {
            static_cast<DebugContext*>(context)->m_renderer.DrawPoint(FromNativePosition(position), size, GetDebugColor(color));
        }

        void DrawDebugSphere(b3Pos position, float radius, b3HexColor color, float opacity, void* context)
        {
            static_cast<DebugContext*>(context)->m_renderer.DrawSphere(FromNativePosition(position), radius, GetDebugColor(color), opacity);
        }

        void DrawDebugCapsule(b3Pos start, b3Pos end, float radius, b3HexColor color, float opacity, void* context)
        {
            static_cast<DebugContext*>(context)->m_renderer.DrawCapsule(
                FromNativePosition(start), FromNativePosition(end), radius, GetDebugColor(color), opacity);
        }

        void DrawDebugBounds(b3AABB bounds, b3HexColor color, void* context)
        {
            static_cast<DebugContext*>(context)->m_renderer.DrawBounds(FromNative(bounds), GetDebugColor(color));
        }

        void DrawDebugBox(b3Vec3 extents, b3WorldTransform transform, b3HexColor color, void* context)
        {
            static_cast<DebugContext*>(context)->m_renderer.DrawBox(
                AZ::Vector3(extents.x, extents.z, extents.y), FromNative(transform), GetDebugColor(color));
        }

        void DrawDebugText(b3Pos position, const char* value, b3HexColor color, void* context)
        {
            static_cast<DebugContext*>(context)->m_renderer.DrawText(
                FromNativePosition(position), value != nullptr ? value : "", GetDebugColor(color));
        }

        b3DebugDraw MakeDebugDraw(
            const DebugDrawSettings& settings, DebugContext& context, bool (*drawShape)(void*, b3WorldTransform, b3HexColor, void*))
        {
            b3DebugDraw draw = b3DefaultDebugDraw();
            draw.DrawShapeFcn = drawShape;
            draw.DrawSegmentFcn = DrawDebugLine;
            draw.DrawTransformFcn = DrawDebugTransform;
            draw.DrawPointFcn = DrawDebugPoint;
            draw.DrawSphereFcn = DrawDebugSphere;
            draw.DrawCapsuleFcn = DrawDebugCapsule;
            draw.DrawBoundsFcn = DrawDebugBounds;
            draw.DrawBoxFcn = DrawDebugBox;
            draw.DrawStringFcn = DrawDebugText;
            if (settings.m_bounds.IsValid())
            {
                draw.drawingBounds = {
                    { settings.m_bounds.GetMin().GetX(), -settings.m_bounds.GetMax().GetZ(), settings.m_bounds.GetMin().GetY() },
                    { settings.m_bounds.GetMax().GetX(), -settings.m_bounds.GetMin().GetZ(), settings.m_bounds.GetMax().GetY() }
                };
            }
            draw.forceScale = settings.m_forceScale;
            draw.jointScale = settings.m_jointScale;
            draw.drawShapes = settings.m_drawShapes;
            draw.drawJoints = settings.m_drawJoints;
            draw.drawJointExtras = settings.m_drawJointExtras;
            draw.drawBounds = settings.m_drawBounds;
            draw.drawMass = settings.m_drawMass;
            draw.drawBodyNames = settings.m_drawBodyNames;
            draw.drawContacts = settings.m_drawContacts;
            draw.drawAnchorA = AZStd::clamp(settings.m_drawContactAnchor, AZ::s32{ -1 }, AZ::s32{ 1 });
            draw.drawGraphColors = settings.m_drawGraphColors;
            draw.drawContactFeatures = settings.m_drawContactFeatures;
            draw.drawContactNormals = settings.m_drawContactNormals;
            draw.drawContactForces = settings.m_drawContactForces;
            draw.drawFrictionForces = settings.m_drawFrictionForces;
            draw.drawIslands = settings.m_drawIslands;
            draw.context = &context;
            return draw;
        }
    } // namespace

    AZStd::shared_ptr<const CompoundGeometry> CompoundGeometry::CloneWithMaterials(AZStd::span<const SurfaceMaterial> materials) const
    {
        const auto* compound = static_cast<const b3CompoundData*>(m_data);
        if (compound == nullptr || materials.empty() || m_materialSources.size() != aznumeric_cast<size_t>(compound->materialCount))
        {
            return {};
        }

        AZStd::vector<b3SurfaceMaterial> nativeMaterials;
        nativeMaterials.reserve(materials.size());
        for (const SurfaceMaterial& material : materials)
        {
            nativeMaterials.push_back(ToNative(material));
        }

        AZStd::vector<b3CompoundCapsuleDef> capsules;
        capsules.reserve(compound->capsuleCount);
        for (int capsuleIndex = 0; capsuleIndex < compound->capsuleCount; ++capsuleIndex)
        {
            const b3CompoundCapsule capsule = b3GetCompoundCapsule(compound, capsuleIndex);
            if (capsule.materialIndex < 0 || capsule.materialIndex >= compound->materialCount)
            {
                return {};
            }
            const AZ::u8 sourceIndex = m_materialSources[capsule.materialIndex];
            if (sourceIndex >= nativeMaterials.size())
            {
                return {};
            }
            capsules.push_back({ capsule.capsule, nativeMaterials[sourceIndex] });
        }

        AZStd::vector<b3CompoundHullDef> hulls;
        hulls.reserve(compound->hullCount);
        for (int hullIndex = 0; hullIndex < compound->hullCount; ++hullIndex)
        {
            const b3CompoundHull hull = b3GetCompoundHull(compound, hullIndex);
            if (hull.materialIndex < 0 || hull.materialIndex >= compound->materialCount)
            {
                return {};
            }
            const AZ::u8 sourceIndex = m_materialSources[hull.materialIndex];
            if (sourceIndex >= nativeMaterials.size())
            {
                return {};
            }
            hulls.push_back({ hull.hull, hull.transform, nativeMaterials[sourceIndex] });
        }

        AZStd::vector<AZStd::array<b3SurfaceMaterial, B3_MAX_COMPOUND_MESH_MATERIALS>> meshMaterials;
        AZStd::vector<b3CompoundMeshDef> meshes;
        meshMaterials.reserve(compound->meshCount);
        meshes.reserve(compound->meshCount);
        for (int meshIndex = 0; meshIndex < compound->meshCount; ++meshIndex)
        {
            const b3CompoundMesh mesh = b3GetCompoundMesh(compound, meshIndex);
            if (mesh.meshData == nullptr || mesh.meshData->materialCount < 1 ||
                mesh.meshData->materialCount > B3_MAX_COMPOUND_MESH_MATERIALS)
            {
                return {};
            }
            AZStd::array<b3SurfaceMaterial, B3_MAX_COMPOUND_MESH_MATERIALS>& mappedMaterials = meshMaterials.emplace_back();
            for (int materialIndex = 0; materialIndex < mesh.meshData->materialCount; ++materialIndex)
            {
                const int sourceIndex = mesh.materialIndices[materialIndex];
                if (sourceIndex < 0 || sourceIndex >= compound->materialCount)
                {
                    return {};
                }
                const AZ::u8 providerSourceIndex = m_materialSources[sourceIndex];
                if (providerSourceIndex >= nativeMaterials.size())
                {
                    return {};
                }
                mappedMaterials[materialIndex] = nativeMaterials[providerSourceIndex];
            }
            meshes.push_back({ mesh.meshData, mesh.transform, mesh.scale, mappedMaterials.data(), mesh.meshData->materialCount });
        }

        AZStd::vector<b3CompoundSphereDef> spheres;
        spheres.reserve(compound->sphereCount);
        for (int sphereIndex = 0; sphereIndex < compound->sphereCount; ++sphereIndex)
        {
            const b3CompoundSphere sphere = b3GetCompoundSphere(compound, sphereIndex);
            if (sphere.materialIndex < 0 || sphere.materialIndex >= compound->materialCount)
            {
                return {};
            }
            const AZ::u8 sourceIndex = m_materialSources[sphere.materialIndex];
            if (sourceIndex >= nativeMaterials.size())
            {
                return {};
            }
            spheres.push_back({ sphere.sphere, nativeMaterials[sourceIndex] });
        }

        b3CompoundDef definition{};
        definition.capsules = capsules.data();
        definition.capsuleCount = aznumeric_cast<int>(capsules.size());
        definition.hulls = hulls.data();
        definition.hullCount = aznumeric_cast<int>(hulls.size());
        definition.meshes = meshes.data();
        definition.meshCount = aznumeric_cast<int>(meshes.size());
        definition.spheres = spheres.data();
        definition.sphereCount = aznumeric_cast<int>(spheres.size());
        b3CompoundData* clone = b3CreateCompound(&definition);
        if (clone == nullptr)
        {
            return {};
        }
        AZStd::vector<AZ::u8> cloneMaterialSources;
        cloneMaterialSources.reserve(aznumeric_cast<size_t>(clone->materialCount));
        const b3SurfaceMaterial* cloneMaterials = b3GetCompoundMaterials(clone);
        for (int materialIndex = 0; materialIndex < clone->materialCount; ++materialIndex)
        {
            size_t inputIndex = 0;
            while (inputIndex < nativeMaterials.size() &&
                   nativeMaterials[inputIndex].userMaterialId != cloneMaterials[materialIndex].userMaterialId)
            {
                ++inputIndex;
            }
            if (inputIndex == nativeMaterials.size() || inputIndex > AZStd::numeric_limits<AZ::u8>::max())
            {
                b3DestroyCompound(clone);
                return {};
            }
            cloneMaterialSources.push_back(aznumeric_cast<AZ::u8>(inputIndex));
        }
        return AZStd::shared_ptr<const CompoundGeometry>(aznew CompoundGeometry(clone, AZStd::move(cloneMaterialSources)));
    }

    MoverScratch::MoverScratch(size_t maximumPlaneCount)
        : m_data(aznew MoverScratchData(maximumPlaneCount))
    {
    }

    MoverScratch::~MoverScratch()
    {
        delete static_cast<MoverScratchData*>(m_data);
    }

    AZStd::span<const MoverPlane> MoverScratch::GetPlanes() const
    {
        const auto* data = static_cast<const MoverScratchData*>(m_data);
        return data != nullptr ? AZStd::span<const MoverPlane>(data->m_results) : AZStd::span<const MoverPlane>();
    }

    JointScratch::JointScratch()
        : m_data(aznew JointScratchData)
    {
    }

    JointScratch::~JointScratch()
    {
        delete static_cast<JointScratchData*>(m_data);
    }

    AZStd::span<const JointId> JointScratch::GetJoints() const
    {
        const auto* data = static_cast<const JointScratchData*>(m_data);
        return data != nullptr ? AZStd::span<const JointId>(data->m_joints) : AZStd::span<const JointId>();
    }

    class ReplayInstance final
        : public IReplay
    {
    public:
        explicit ReplayInstance(b3RecPlayer* player)
            : m_player(player)
        {
            const DeterministicFloatScope floatScope;
            b3RecPlayer_SetDebugShapeCallbacks(m_player, CreateReplayDebugShape, DestroyReplayDebugShape, nullptr);
        }

        ~ReplayInstance() override
        {
            b3RecPlayer_Destroy(m_player);
        }

        ReplayInfo GetInfo() const override
        {
            const b3RecPlayerInfo info = b3RecPlayer_GetInfo(m_player);
            return {
                ToUnsigned(info.frameCount), ToUnsigned(info.workerCount), info.timeStep, ToUnsigned(info.subStepCount), info.lengthScale,
                FromNative(info.bounds)
            };
        }

        bool Step() override
        {
            const DeterministicFloatScope floatScope;
            return b3RecPlayer_StepFrame(m_player);
        }

        void Restart() override
        {
            const DeterministicFloatScope floatScope;
            b3RecPlayer_Restart(m_player);
        }

        void Seek(AZ::u32 frame) override
        {
            const DeterministicFloatScope floatScope;
            b3RecPlayer_SeekFrame(
                m_player, aznumeric_cast<int>(AZStd::min(frame, aznumeric_cast<AZ::u32>((AZStd::numeric_limits<int>::max)()))));
        }

        AZ::u32 GetFrame() const override
        {
            return ToUnsigned(b3RecPlayer_GetFrame(m_player));
        }

        bool IsAtEnd() const override
        {
            return b3RecPlayer_IsAtEnd(m_player);
        }

        bool HasDiverged() const override
        {
            return b3RecPlayer_HasDiverged(m_player);
        }

        AZ::s32 GetDivergenceFrame() const override
        {
            return b3RecPlayer_GetDivergeFrame(m_player);
        }

        void SetWorkerCount(AZ::u32 workerCount) override
        {
            b3RecPlayer_SetWorkerCount(
                m_player,
                aznumeric_cast<int>(AZStd::clamp(workerCount, AZ::u32{ 1 }, aznumeric_cast<AZ::u32>((AZStd::numeric_limits<int>::max)()))));
        }

        void SetKeyframePolicy(size_t budgetBytes, AZ::u32 minimumInterval) override
        {
            b3RecPlayer_SetKeyframePolicy(
                m_player,
                budgetBytes,
                aznumeric_cast<int>(AZStd::min(minimumInterval, aznumeric_cast<AZ::u32>((AZStd::numeric_limits<int>::max)()))));
        }

        size_t GetKeyframeBudget() const override
        {
            return b3RecPlayer_GetKeyframeBudget(m_player);
        }

        AZ::u32 GetMinimumKeyframeInterval() const override
        {
            return ToUnsigned(b3RecPlayer_GetKeyframeMinInterval(m_player));
        }

        AZ::u32 GetKeyframeInterval() const override
        {
            return ToUnsigned(b3RecPlayer_GetKeyframeInterval(m_player));
        }

        size_t GetKeyframeBytes() const override
        {
            return b3RecPlayer_GetKeyframeBytes(m_player);
        }

        AZ::u32 GetBodyCount() const override
        {
            return ToUnsigned(b3RecPlayer_GetBodyCount(m_player));
        }

        bool GetBody(AZ::u32 index, ReplayBody& body) const override
        {
            body = {};
            if (index >= GetBodyCount())
            {
                return false;
            }

            const b3BodyId bodyId = b3RecPlayer_GetBodyId(m_player, aznumeric_cast<int>(index));
            if (!b3Body_IsValid(bodyId))
            {
                return true;
            }
            body.m_transform = FromNative(b3Body_GetTransform(bodyId));
            body.m_linearVelocity = FromNative(b3Body_GetLinearVelocity(bodyId));
            body.m_angularVelocity = FromNative(b3Body_GetAngularVelocity(bodyId));
            body.m_exists = true;
            body.m_awake = b3Body_IsAwake(bodyId);
            return true;
        }

        AZ::u32 GetFrameQueryCount() const override
        {
            return ToUnsigned(b3RecPlayer_GetFrameQueryCount(m_player));
        }

        bool GetFrameQuery(AZ::u32 index, ReplayQuery& query) const override
        {
            query = {};
            if (index >= GetFrameQueryCount())
            {
                return false;
            }

            const b3RecQueryInfo info = b3RecPlayer_GetFrameQuery(m_player, aznumeric_cast<int>(index));
            switch (info.type)
            {
            case b3_recQueryOverlapAABB:
                query.m_type = ReplayQueryType::OverlapAabb;
                break;
            case b3_recQueryOverlapShape:
                query.m_type = ReplayQueryType::OverlapShape;
                break;
            case b3_recQueryCastRay:
                query.m_type = ReplayQueryType::Raycast;
                break;
            case b3_recQueryCastShape:
                query.m_type = ReplayQueryType::ShapeCast;
                break;
            case b3_recQueryCastRayClosest:
                query.m_type = ReplayQueryType::ClosestRaycast;
                break;
            case b3_recQueryCastMover:
                query.m_type = ReplayQueryType::MoverCast;
                break;
            case b3_recQueryCollideMover:
                query.m_type = ReplayQueryType::MoverCollision;
                break;
            default:
                return false;
            }
            query.m_categoryBits = info.filter.categoryBits;
            query.m_maskBits = info.filter.maskBits;
            query.m_bounds = FromNative(info.aabb);
            query.m_origin = FromNativePosition(info.origin);
            query.m_translation = FromNative(info.translation);
            query.m_hitCount = ToUnsigned(info.hitCount);
            query.m_key = info.key;
            query.m_id = info.id;
            query.m_name = AZ::Name(info.name != nullptr ? info.name : "");
            return true;
        }

        bool GetFrameQueryHit(AZ::u32 queryIndex, AZ::u32 hitIndex, ReplayQueryHit& hit) const override
        {
            hit = {};
            ReplayQuery query;
            if (!GetFrameQuery(queryIndex, query) || hitIndex >= query.m_hitCount)
            {
                return false;
            }

            const b3RecQueryHit nativeHit =
                b3RecPlayer_GetFrameQueryHit(m_player, aznumeric_cast<int>(queryIndex), aznumeric_cast<int>(hitIndex));
            if (nativeHit.shape.index1 > 0)
            {
                hit.m_shapeId.m_index = aznumeric_cast<AZ::u32>(nativeHit.shape.index1 - 1);
                hit.m_shapeId.m_generation = nativeHit.shape.generation;
            }
            hit.m_position = FromNativePosition(nativeHit.point);
            hit.m_normal = FromNative(nativeHit.normal);
            hit.m_fraction = nativeHit.fraction;
            return true;
        }

        bool Draw(const DebugDrawSettings& settings, IDebugRenderer& renderer, AZ::s32 queryIndex, AZ::s32 selectedQueryIndex) override
        {
            if (!AZ::IsFiniteFloat(settings.m_forceScale) || settings.m_forceScale < 0.0f || !AZ::IsFiniteFloat(settings.m_jointScale) ||
                settings.m_jointScale < 0.0f || queryIndex < -1 || selectedQueryIndex < -1 ||
                queryIndex >= aznumeric_cast<AZ::s32>(GetFrameQueryCount()) ||
                selectedQueryIndex >= aznumeric_cast<AZ::s32>(GetFrameQueryCount()))
            {
                return false;
            }

            DebugContext context{ renderer };
            b3DebugDraw draw = MakeDebugDraw(settings, context, DrawReplayDebugShape);
            b3World_Draw(b3RecPlayer_GetWorldId(m_player), &draw, settings.m_maskBits);
            b3RecPlayer_DrawFrameQueries(m_player, &draw, queryIndex, selectedQueryIndex);
            return true;
        }

    private:
        b3RecPlayer* m_player = nullptr;
    };

    void SetMaterialCallbacks(IMaterialCallbacks* callbacks)
    {
        s_materialCallbacks.store(callbacks, AZStd::memory_order_release);
    }

    Version GetVersion()
    {
        const b3Version version = b3GetVersion();
        return { version.major, version.minor, version.revision };
    }

    AZStd::unique_ptr<Recording> CreateRecording(size_t initialCapacityBytes)
    {
        if (initialCapacityBytes > aznumeric_cast<size_t>((AZStd::numeric_limits<int>::max)()))
        {
            return nullptr;
        }
        b3Recording* recording = b3CreateRecording(aznumeric_cast<int>(initialCapacityBytes));
        return recording != nullptr ? AZStd::unique_ptr<Recording>(aznew Recording(recording)) : nullptr;
    }

    bool StartRecording(WorldId worldId, Recording& recording)
    {
        auto* nativeRecording = static_cast<b3Recording*>(recording.m_data);
        if (!IsValid(worldId) || nativeRecording == nullptr)
        {
            return false;
        }
        const DeterministicFloatScope floatScope;
        b3World_StartRecording(b3LoadWorldId(worldId.m_value), nativeRecording);
        return b3Recording_GetSize(nativeRecording) > 0;
    }

    bool StopRecording(WorldId worldId, Recording& recording, AZStd::vector<AZ::u8>& data)
    {
        data.clear();
        auto* nativeRecording = static_cast<b3Recording*>(recording.m_data);
        if (!IsValid(worldId) || nativeRecording == nullptr)
        {
            return false;
        }
        b3World_StopRecording(b3LoadWorldId(worldId.m_value));
        const int size = b3Recording_GetSize(nativeRecording);
        const AZ::u8* bytes = b3Recording_GetData(nativeRecording);
        if (size <= 0 || bytes == nullptr)
        {
            return false;
        }
        data.assign(bytes, bytes + size);
        return true;
    }

    bool ValidateRecording(AZStd::span<const AZ::u8> data, AZ::u32 workerCount)
    {
        const DeterministicFloatScope floatScope;
        return !data.empty() && data.size() <= aznumeric_cast<size_t>((AZStd::numeric_limits<int>::max)()) && workerCount > 0 &&
            workerCount <= aznumeric_cast<AZ::u32>((AZStd::numeric_limits<int>::max)()) &&
            b3ValidateReplay(data.data(), aznumeric_cast<int>(data.size()), aznumeric_cast<int>(workerCount));
    }

    AZStd::unique_ptr<IReplay> CreateReplay(AZStd::span<const AZ::u8> data, AZ::u32 workerCount)
    {
        if (data.empty() || data.size() > aznumeric_cast<size_t>((AZStd::numeric_limits<int>::max)()) || workerCount == 0 ||
            workerCount > aznumeric_cast<AZ::u32>((AZStd::numeric_limits<int>::max)()))
        {
            return nullptr;
        }
        const DeterministicFloatScope floatScope;
        b3RecPlayer* player = b3RecPlayer_Create(data.data(), aznumeric_cast<int>(data.size()), aznumeric_cast<int>(workerCount));
        return player != nullptr ? AZStd::unique_ptr<IReplay>(aznew ReplayInstance(player)) : nullptr;
    }

    void SetLengthUnitsPerMeter(float lengthUnitsPerMeter)
    {
        InstallNativeCallbacks();
        b3SetLengthUnitsPerMeter(lengthUnitsPerMeter);
    }

    void SetStallWarningThreshold(float seconds)
    {
        InstallNativeCallbacks();
        b3SetStallThreshold(seconds);
    }

    bool DrawWorld(WorldId worldId, const DebugDrawSettings& settings, IDebugRenderer& renderer)
    {
        if (!IsValid(worldId) || !AZ::IsFiniteFloat(settings.m_forceScale) || settings.m_forceScale < 0.0f ||
            !AZ::IsFiniteFloat(settings.m_jointScale) || settings.m_jointScale < 0.0f)
        {
            return false;
        }

        DebugContext context{ renderer };
        b3DebugDraw draw = MakeDebugDraw(settings, context, DrawDebugShape);
        b3World_Draw(b3LoadWorldId(worldId.m_value), &draw, settings.m_maskBits);
        return true;
    }

    WorldId CreateWorld(const NativeWorldConfiguration& configuration)
    {
        InstallNativeCallbacks();
        b3WorldDef definition = b3DefaultWorldDef();
        definition.gravity = ToNative(configuration.m_gravity);
        definition.restitutionThreshold = configuration.m_restitutionThreshold;
        definition.hitEventThreshold = configuration.m_hitEventThreshold;
        definition.contactHertz = configuration.m_contactHertz;
        definition.contactDampingRatio = configuration.m_contactDampingRatio;
        definition.contactSpeed = configuration.m_contactSpeed;
        definition.maximumLinearSpeed = configuration.m_maximumLinearSpeed;
        definition.frictionCallback = configuration.m_frictionCallback != nullptr ? MixFriction : nullptr;
        definition.restitutionCallback = configuration.m_restitutionCallback != nullptr ? MixRestitution : nullptr;
        definition.workerCount = configuration.m_workerCount;
        if (configuration.m_taskContext != nullptr && configuration.m_taskContext->m_jobContext != nullptr)
        {
            definition.enqueueTask = EnqueueNativeTask;
            definition.finishTask = FinishNativeTask;
            definition.userTaskContext = configuration.m_taskContext;
        }
        definition.createDebugShape = CreateNativeDebugShape;
        definition.destroyDebugShape = DestroyNativeDebugShape;
        definition.capacity.staticShapeCount = aznumeric_cast<int>(configuration.m_staticShapeCapacity);
        definition.capacity.dynamicShapeCount = aznumeric_cast<int>(configuration.m_dynamicShapeCapacity);
        definition.capacity.staticBodyCount = aznumeric_cast<int>(configuration.m_staticBodyCapacity);
        definition.capacity.dynamicBodyCount = aznumeric_cast<int>(configuration.m_dynamicBodyCapacity);
        definition.capacity.contactCount = aznumeric_cast<int>(configuration.m_contactCapacity);
        definition.enableSleep = configuration.m_enableSleep;
        definition.enableContinuous = configuration.m_enableContinuous;
        const b3WorldId world = b3CreateWorld(&definition);
        b3World_SetContactRecycleDistance(world, configuration.m_contactRecycleDistance);
        b3World_EnableWarmStarting(world, configuration.m_enableWarmStarting);
        b3World_EnableSpeculative(world, configuration.m_enableSpeculative);
        return { b3StoreWorldId(world) };
    }

    void DestroyWorld(WorldId worldId)
    {
        if (IsValid(worldId))
        {
            b3DestroyWorld(b3LoadWorldId(worldId.m_value));
        }
    }

    bool IsValid(WorldId worldId)
    {
        return worldId.IsValid() && b3World_IsValid(b3LoadWorldId(worldId.m_value));
    }

    void Step(WorldId worldId, float timeStep, AZ::u32 subStepCount)
    {
        const DeterministicFloatScope floatScope;
        b3World_Step(b3LoadWorldId(worldId.m_value), timeStep, aznumeric_cast<int>(subStepCount));
    }

    void ReconfigureWorld(WorldId worldId, const NativeWorldConfiguration& configuration)
    {
        const b3WorldId world = b3LoadWorldId(worldId.m_value);
        b3World_SetRestitutionThreshold(world, configuration.m_restitutionThreshold);
        b3World_SetHitEventThreshold(world, configuration.m_hitEventThreshold);
        b3World_SetContactTuning(world, configuration.m_contactHertz, configuration.m_contactDampingRatio, configuration.m_contactSpeed);
        b3World_SetContactRecycleDistance(world, configuration.m_contactRecycleDistance);
        b3World_SetMaximumLinearSpeed(world, configuration.m_maximumLinearSpeed);
        b3World_SetFrictionCallback(world, configuration.m_frictionCallback != nullptr ? MixFriction : nullptr);
        b3World_SetRestitutionCallback(world, configuration.m_restitutionCallback != nullptr ? MixRestitution : nullptr);
        b3World_SetWorkerCount(world, aznumeric_cast<int>(configuration.m_workerCount));
        b3World_EnableSleeping(world, configuration.m_enableSleep);
        b3World_EnableContinuous(world, configuration.m_enableContinuous);
        b3World_EnableWarmStarting(world, configuration.m_enableWarmStarting);
        b3World_EnableSpeculative(world, configuration.m_enableSpeculative);
    }

    void SetContactCallbacks(WorldId worldId, IContactCallbacks* callbacks, bool enableCustomFilterCallback, bool enablePreSolveCallback)
    {
        if (!IsValid(worldId))
        {
            return;
        }
        const b3WorldId world = b3LoadWorldId(worldId.m_value);
        b3World_SetCustomFilterCallback(world, callbacks != nullptr && enableCustomFilterCallback ? FilterContact : nullptr, callbacks);
        b3World_SetPreSolveCallback(world, callbacks != nullptr && enablePreSolveCallback ? FilterPreSolveContact : nullptr, callbacks);
    }

    void SetGravity(WorldId worldId, const AZ::Vector3& gravity)
    {
        b3World_SetGravity(b3LoadWorldId(worldId.m_value), ToNative(gravity));
    }

    AZ::Vector3 GetGravity(WorldId worldId)
    {
        return FromNative(b3World_GetGravity(b3LoadWorldId(worldId.m_value)));
    }

    AZ::Aabb GetWorldAabb(WorldId worldId)
    {
        return IsValid(worldId) ? FromNative(b3World_GetBounds(b3LoadWorldId(worldId.m_value))) : AZ::Aabb::CreateNull();
    }

    bool RebuildStaticTree(WorldId worldId)
    {
        if (!IsValid(worldId))
        {
            return false;
        }
        b3World_RebuildStaticTree(b3LoadWorldId(worldId.m_value));
        return true;
    }

    bool Explode(WorldId worldId, const AZ::Vector3& position, AZ::u64 maskBits, float radius, float falloff, float impulsePerArea)
    {
        if (!IsValid(worldId) || !position.IsFinite() || !AZ::IsFiniteFloat(radius) || radius < 0.0f || !AZ::IsFiniteFloat(falloff) ||
            falloff < 0.0f || !AZ::IsFiniteFloat(impulsePerArea))
        {
            return false;
        }

        b3ExplosionDef definition = b3DefaultExplosionDef();
        definition.maskBits = maskBits;
        definition.position = ToNativePosition(position);
        definition.radius = radius;
        definition.falloff = falloff;
        definition.impulsePerArea = impulsePerArea;
        b3World_Explode(b3LoadWorldId(worldId.m_value), &definition);
        return true;
    }

    bool GetStatistics(WorldId worldId, StatisticsFlags flags, WorldStatistics& statistics)
    {
        statistics = {};
        if (!IsValid(worldId))
        {
            return false;
        }

        const b3WorldId nativeWorldId = b3LoadWorldId(worldId.m_value);
        if (HasAnyFlag(flags, StatisticsFlags::Profile))
        {
            const b3Profile profile = b3World_GetProfile(nativeWorldId);
            statistics.m_lastStep.m_total = Milliseconds(profile.step);
            statistics.m_lastStep.m_pairGeneration = Milliseconds(profile.pairs);
            statistics.m_lastStep.m_collision = Milliseconds(profile.collide);
            statistics.m_lastStep.m_solver = Milliseconds(profile.solve);
            statistics.m_lastStep.m_solverSetup = Milliseconds(profile.solverSetup);
            statistics.m_lastStep.m_constraints = Milliseconds(profile.constraints);
            statistics.m_lastStep.m_prepareConstraints = Milliseconds(profile.prepareConstraints);
            statistics.m_lastStep.m_integrateVelocities = Milliseconds(profile.integrateVelocities);
            statistics.m_lastStep.m_warmStart = Milliseconds(profile.warmStart);
            statistics.m_lastStep.m_solveImpulses = Milliseconds(profile.solveImpulses);
            statistics.m_lastStep.m_integratePositions = Milliseconds(profile.integratePositions);
            statistics.m_lastStep.m_relaxImpulses = Milliseconds(profile.relaxImpulses);
            statistics.m_lastStep.m_applyRestitution = Milliseconds(profile.applyRestitution);
            statistics.m_lastStep.m_storeImpulses = Milliseconds(profile.storeImpulses);
            statistics.m_lastStep.m_splitIslands = Milliseconds(profile.splitIslands);
            statistics.m_lastStep.m_updateTransforms = Milliseconds(profile.transforms);
            statistics.m_lastStep.m_sensorHits = Milliseconds(profile.sensorHits);
            statistics.m_lastStep.m_jointEvents = Milliseconds(profile.jointEvents);
            statistics.m_lastStep.m_hitEvents = Milliseconds(profile.hitEvents);
            statistics.m_lastStep.m_refitBroadPhase = Milliseconds(profile.refit);
            statistics.m_lastStep.m_continuousCollision = Milliseconds(profile.bullets);
            statistics.m_lastStep.m_sleepIslands = Milliseconds(profile.sleepIslands);
            statistics.m_lastStep.m_sensorOverlap = Milliseconds(profile.sensors);
        }

        if (HasAnyFlag(flags, StatisticsFlags::Counters))
        {
            static_assert(ConstraintGraphColorCount == B3_GRAPH_COLOR_COUNT);
            static_assert(ContactManifoldBucketCount == B3_CONTACT_MANIFOLD_COUNT_BUCKETS);

            const b3Counters counters = b3World_GetCounters(nativeWorldId);
            SimulationCounters& result = statistics.m_counters;
            result.m_workerCount = ToUnsigned(b3World_GetWorkerCount(nativeWorldId));
            result.m_bodyCount = ToUnsigned(counters.bodyCount);
            result.m_awakeBodyCount = ToUnsigned(b3World_GetAwakeBodyCount(nativeWorldId));
            result.m_shapeCount = ToUnsigned(counters.shapeCount);
            result.m_contactCount = ToUnsigned(counters.contactCount);
            result.m_awakeContactCount = ToUnsigned(counters.awakeContactCount);
            result.m_recycledContactCount = ToUnsigned(counters.recycledContactCount);
            result.m_jointCount = ToUnsigned(counters.jointCount);
            result.m_islandCount = ToUnsigned(counters.islandCount);
            result.m_taskCount = ToUnsigned(counters.taskCount);
            result.m_staticTreeHeight = ToUnsigned(counters.staticTreeHeight);
            result.m_dynamicTreeHeight = ToUnsigned(counters.treeHeight);
            result.m_separatingAxisCallCount = ToUnsigned(counters.satCallCount);
            result.m_separatingAxisCacheHitCount = ToUnsigned(counters.satCacheHitCount);
            result.m_maxToiDistanceIterations = ToUnsigned(counters.distanceIterations);
            result.m_maxToiPushBackIterations = ToUnsigned(counters.pushBackIterations);
            result.m_maxToiRootIterations = ToUnsigned(counters.rootIterations);
            result.m_stackHighWaterBytes = ToUnsigned(counters.stackUsed);
            result.m_largestWorkerArenaPeakBytes = ToUnsigned(counters.arenaCapacity);
            result.m_globalAllocatedBytes = ToUnsigned(counters.byteCount);
            for (size_t index = 0; index < ConstraintGraphColorCount; ++index)
            {
                result.m_constraintGraphOccupancy[index] = ToUnsigned(counters.colorCounts[index]);
            }
            for (size_t index = 0; index < ContactManifoldBucketCount; ++index)
            {
                result.m_contactManifoldHistogram[index] = ToUnsigned(counters.manifoldCounts[index]);
            }
        }

        if (HasAnyFlag(flags, StatisticsFlags::Capacity))
        {
            const b3Capacity capacity = b3World_GetMaxCapacity(nativeWorldId);
            statistics.m_capacityHighWaterMarks = { ToUnsigned(capacity.staticShapeCount),
                                                    ToUnsigned(capacity.dynamicShapeCount),
                                                    ToUnsigned(capacity.staticBodyCount),
                                                    ToUnsigned(capacity.dynamicBodyCount),
                                                    ToUnsigned(capacity.contactCount) };
        }

        if (HasAnyFlag(flags, StatisticsFlags::Bounds))
        {
            statistics.m_worldBounds = FromNative(b3World_GetBounds(nativeWorldId));
        }

        return true;
    }

    void GetStepEvents(
        WorldId worldId,
        StepEventFlags flags,
        AZStd::vector<BodyMove>& bodyMoves,
        AZStd::vector<SensorTransition>& sensorTransitions,
        AZStd::vector<ContactTransition>& contactTransitions,
        AZStd::vector<ContactHit>& contactHits,
        AZStd::vector<NativeJointThresholdEvent>& jointEvents)
    {
        bodyMoves.clear();
        sensorTransitions.clear();
        contactTransitions.clear();
        contactHits.clear();
        jointEvents.clear();
        if (!IsValid(worldId))
        {
            return;
        }

        const b3WorldId nativeWorldId = b3LoadWorldId(worldId.m_value);
        if ((flags & StepEventFlags::BodyMoves) != StepEventFlags::None)
        {
            const b3BodyEvents bodyEvents = b3World_GetBodyEvents(nativeWorldId);
            bodyMoves.reserve(bodyEvents.moveCount);
            for (int eventIndex = 0; eventIndex < bodyEvents.moveCount; ++eventIndex)
            {
                const b3BodyMoveEvent& event = bodyEvents.moveEvents[eventIndex];
                bodyMoves.push_back({ { b3StoreBodyId(event.bodyId) }, event.userData, FromNative(event.transform), event.fellAsleep });
            }
        }

        if ((flags & StepEventFlags::Sensors) != StepEventFlags::None)
        {
            const b3SensorEvents sensorEvents = b3World_GetSensorEvents(nativeWorldId);
            sensorTransitions.reserve(sensorEvents.beginCount + sensorEvents.endCount);
            for (int eventIndex = 0; eventIndex < sensorEvents.beginCount; ++eventIndex)
            {
                const b3SensorBeginTouchEvent& event = sensorEvents.beginEvents[eventIndex];
                sensorTransitions.push_back(
                    { SensorTransitionType::Begin, { b3StoreShapeId(event.sensorShapeId) }, { b3StoreShapeId(event.visitorShapeId) } });
            }
            for (int eventIndex = 0; eventIndex < sensorEvents.endCount; ++eventIndex)
            {
                const b3SensorEndTouchEvent& event = sensorEvents.endEvents[eventIndex];
                sensorTransitions.push_back(
                    { SensorTransitionType::End, { b3StoreShapeId(event.sensorShapeId) }, { b3StoreShapeId(event.visitorShapeId) } });
            }
        }

        if ((flags & (StepEventFlags::Contacts | StepEventFlags::ContactHits)) != StepEventFlags::None)
        {
            const b3ContactEvents contactEvents = b3World_GetContactEvents(nativeWorldId);
            if ((flags & StepEventFlags::Contacts) != StepEventFlags::None)
            {
                contactTransitions.reserve(contactEvents.beginCount + contactEvents.endCount);
                for (int eventIndex = 0; eventIndex < contactEvents.beginCount; ++eventIndex)
                {
                    const b3ContactBeginTouchEvent& event = contactEvents.beginEvents[eventIndex];
                    AZ::u32 contactId[3];
                    b3StoreContactId(event.contactId, contactId);
                    contactTransitions.push_back(
                        { ContactTransitionType::Begin,
                          { contactId[0], contactId[1], contactId[2] },
                          { b3StoreShapeId(event.shapeIdA) },
                          { b3StoreShapeId(event.shapeIdB) } });
                }
                for (int eventIndex = 0; eventIndex < contactEvents.endCount; ++eventIndex)
                {
                    const b3ContactEndTouchEvent& event = contactEvents.endEvents[eventIndex];
                    AZ::u32 contactId[3];
                    b3StoreContactId(event.contactId, contactId);
                    contactTransitions.push_back(
                        { ContactTransitionType::End,
                          { contactId[0], contactId[1], contactId[2] },
                          { b3StoreShapeId(event.shapeIdA) },
                          { b3StoreShapeId(event.shapeIdB) } });
                }
            }

            if ((flags & StepEventFlags::ContactHits) != StepEventFlags::None)
            {
                contactHits.reserve(contactEvents.hitCount);
                for (int eventIndex = 0; eventIndex < contactEvents.hitCount; ++eventIndex)
                {
                    const b3ContactHitEvent& event = contactEvents.hitEvents[eventIndex];
                    contactHits.push_back(
                        { { b3StoreShapeId(event.shapeIdA) },
                          { b3StoreShapeId(event.shapeIdB) },
                          FromNativePosition(event.point),
                          FromNative(event.normal),
                          event.approachSpeed,
                          event.userMaterialIdA,
                          event.userMaterialIdB,
                          event.triangleIndex,
                          event.childIndexA,
                          event.childIndexB });
                }
            }
        }

        if ((flags & StepEventFlags::Joints) != StepEventFlags::None)
        {
            const b3JointEvents nativeJointEvents = b3World_GetJointEvents(nativeWorldId);
            jointEvents.reserve(nativeJointEvents.count);
            for (int eventIndex = 0; eventIndex < nativeJointEvents.count; ++eventIndex)
            {
                const b3JointEvent& event = nativeJointEvents.jointEvents[eventIndex];
                jointEvents.push_back({ { b3StoreJointId(event.jointId) }, event.userData, InvalidJointHandle });
            }
        }
    }

    namespace
    {
        bool ConvertContactData(const b3ContactData& nativeData, ContactData& contactData)
        {
            contactData = {};
            if (!b3Shape_IsValid(nativeData.shapeIdA) || !b3Shape_IsValid(nativeData.shapeIdB))
            {
                return false;
            }

            const b3BodyId bodyIdA = b3Shape_GetBody(nativeData.shapeIdA);
            const b3BodyId bodyIdB = b3Shape_GetBody(nativeData.shapeIdB);
            contactData.m_shapeIdA = { b3StoreShapeId(nativeData.shapeIdA) };
            contactData.m_shapeIdB = { b3StoreShapeId(nativeData.shapeIdB) };
            contactData.m_bodyIdA = { b3StoreBodyId(bodyIdA) };
            contactData.m_bodyIdB = { b3StoreBodyId(bodyIdB) };
            contactData.m_shapeUserDataA = b3Shape_GetUserData(nativeData.shapeIdA);
            contactData.m_shapeUserDataB = b3Shape_GetUserData(nativeData.shapeIdB);
            contactData.m_bodyUserDataA = b3Body_GetUserData(bodyIdA);
            contactData.m_bodyUserDataB = b3Body_GetUserData(bodyIdB);
            contactData.m_childIndexA = nativeData.childIndexA;
            contactData.m_childIndexB = nativeData.childIndexB;
            const b3Pos centerA = b3Body_GetWorldCenterOfMass(bodyIdA);
            const b3Pos centerB = b3Body_GetWorldCenterOfMass(bodyIdB);
            for (int manifoldIndex = 0; manifoldIndex < nativeData.manifoldCount; ++manifoldIndex)
            {
                const b3Manifold& manifold = nativeData.manifolds[manifoldIndex];
                for (int pointIndex = 0; pointIndex < manifold.pointCount; ++pointIndex)
                {
                    const b3ManifoldPoint& point = manifold.points[pointIndex];
                    const b3Pos pointA = centerA + point.anchorA;
                    const b3Pos pointB = centerB + point.anchorB;
                    contactData.m_points.push_back(
                        { 0.5f * (FromNativePosition(pointA) + FromNativePosition(pointB)),
                          FromNative(manifold.normal),
                          point.totalNormalImpulse * FromNative(manifold.normal),
                          point.separation,
                          point.triangleIndex });
                }
            }
            return true;
        }

        void ConvertAndSortContacts(AZStd::span<const b3ContactData> nativeContacts, AZStd::vector<ContactData>& contacts)
        {
            contacts.clear();
            contacts.reserve(nativeContacts.size());
            for (const b3ContactData& nativeContact : nativeContacts)
            {
                ContactData contact;
                if (ConvertContactData(nativeContact, contact))
                {
                    contacts.push_back(AZStd::move(contact));
                }
            }
            AZStd::sort(
                contacts.begin(),
                contacts.end(),
                [](const ContactData& left, const ContactData& right)
                {
                    const AZ::u64 leftLow = AZStd::min(left.m_shapeIdA.m_value, left.m_shapeIdB.m_value);
                    const AZ::u64 rightLow = AZStd::min(right.m_shapeIdA.m_value, right.m_shapeIdB.m_value);
                    return leftLow != rightLow ? leftLow < rightLow
                                               : AZStd::max(left.m_shapeIdA.m_value, left.m_shapeIdB.m_value) <
                            AZStd::max(right.m_shapeIdA.m_value, right.m_shapeIdB.m_value);
                });
        }
    } // namespace

    bool GetContactData(ContactId contactId, ContactData& contactData)
    {
        contactData = {};
        if (!contactId.IsValid())
        {
            return false;
        }

        AZ::u32 storedId[] = { contactId.m_index, contactId.m_world, contactId.m_generation };
        const b3ContactId nativeContactId = b3LoadContactId(storedId);
        if (!b3Contact_IsValid(nativeContactId))
        {
            return false;
        }

        return ConvertContactData(b3Contact_GetData(nativeContactId), contactData);
    }

    bool GetTouchingContacts(BodyId bodyId, AZStd::vector<ContactData>& contacts)
    {
        contacts.clear();
        if (!IsValid(bodyId))
        {
            return false;
        }

        const b3BodyId nativeBodyId = b3LoadBodyId(bodyId.m_value);
        const int capacity = b3Body_GetContactCapacity(nativeBodyId);
        AZStd::vector<b3ContactData> nativeContacts(aznumeric_cast<size_t>(capacity));
        const int count = b3Body_GetContactData(nativeBodyId, nativeContacts.data(), capacity);
        ConvertAndSortContacts(AZStd::span(nativeContacts).first(aznumeric_cast<size_t>(count)), contacts);
        return true;
    }

    bool GetTouchingContacts(ShapeId shapeId, AZStd::vector<ContactData>& contacts)
    {
        contacts.clear();
        if (!IsValid(shapeId))
        {
            return false;
        }

        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        const int capacity = b3Shape_GetContactCapacity(nativeShapeId);
        AZStd::vector<b3ContactData> nativeContacts(aznumeric_cast<size_t>(capacity));
        const int count = b3Shape_GetContactData(nativeShapeId, nativeContacts.data(), capacity);
        ConvertAndSortContacts(AZStd::span(nativeContacts).first(aznumeric_cast<size_t>(count)), contacts);
        return true;
    }

    bool GetShapeData(ShapeId shapeId, ShapeData& shapeData)
    {
        shapeData = {};
        if (!IsValid(shapeId))
        {
            return false;
        }

        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        const b3BodyId bodyId = b3Shape_GetBody(nativeShapeId);
        shapeData = { shapeId, { b3StoreBodyId(bodyId) }, b3Shape_GetUserData(nativeShapeId), b3Body_GetUserData(bodyId) };
        return true;
    }

    bool GetSensorOverlaps(BodyId bodyId, AZStd::vector<SensorOverlapData>& overlaps)
    {
        overlaps.clear();
        if (!IsValid(bodyId))
        {
            return false;
        }

        const b3BodyId nativeBodyId = b3LoadBodyId(bodyId.m_value);
        const int shapeCount = b3Body_GetShapeCount(nativeBodyId);
        AZStd::vector<b3ShapeId> sensorIds(aznumeric_cast<size_t>(shapeCount));
        const int storedShapeCount = b3Body_GetShapes(nativeBodyId, sensorIds.data(), shapeCount);
        for (int shapeIndex = 0; shapeIndex < storedShapeCount; ++shapeIndex)
        {
            const b3ShapeId sensorId = sensorIds[shapeIndex];
            const int capacity = b3Shape_GetSensorCapacity(sensorId);
            AZStd::vector<b3ShapeId> visitorIds(aznumeric_cast<size_t>(capacity));
            const int count = b3Shape_GetSensorData(sensorId, visitorIds.data(), capacity);
            ShapeData sensor;
            if (!GetShapeData({ b3StoreShapeId(sensorId) }, sensor))
            {
                continue;
            }
            for (int visitorIndex = 0; visitorIndex < count; ++visitorIndex)
            {
                ShapeData visitor;
                if (GetShapeData({ b3StoreShapeId(visitorIds[visitorIndex]) }, visitor))
                {
                    overlaps.push_back({ sensor, visitor });
                }
            }
        }
        AZStd::sort(
            overlaps.begin(),
            overlaps.end(),
            [](const SensorOverlapData& left, const SensorOverlapData& right)
            {
                return left.m_sensor.m_shapeId.m_value != right.m_sensor.m_shapeId.m_value
                    ? left.m_sensor.m_shapeId.m_value < right.m_sensor.m_shapeId.m_value
                    : left.m_visitor.m_shapeId.m_value < right.m_visitor.m_shapeId.m_value;
            });
        return true;
    }

    bool GetSensorOverlaps(ShapeId shapeId, AZStd::vector<SensorOverlapData>& overlaps)
    {
        overlaps.clear();
        if (!IsValid(shapeId))
        {
            return false;
        }

        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        const int capacity = b3Shape_GetSensorCapacity(nativeShapeId);
        AZStd::vector<b3ShapeId> visitorIds(aznumeric_cast<size_t>(capacity));
        const int count = b3Shape_GetSensorData(nativeShapeId, visitorIds.data(), capacity);
        ShapeData sensor;
        if (!GetShapeData(shapeId, sensor))
        {
            return false;
        }
        overlaps.reserve(aznumeric_cast<size_t>(count));
        for (int visitorIndex = 0; visitorIndex < count; ++visitorIndex)
        {
            ShapeData visitor;
            if (GetShapeData({ b3StoreShapeId(visitorIds[visitorIndex]) }, visitor))
            {
                overlaps.push_back({ sensor, visitor });
            }
        }
        AZStd::sort(
            overlaps.begin(),
            overlaps.end(),
            [](const SensorOverlapData& left, const SensorOverlapData& right)
            {
                return left.m_visitor.m_shapeId.m_value < right.m_visitor.m_shapeId.m_value;
            });
        return true;
    }

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
        MoverResult& result)
    {
        result = {};
        result.m_position = position;
        auto* data = static_cast<MoverScratchData*>(scratch.m_data);
        if (!IsValid(worldId) || data == nullptr || !position.IsFinite() || !center1.IsFinite() || !center2.IsFinite() ||
            !translation.IsFinite() || !AZ::IsFiniteFloat(radius) || radius <= 0.0f || !AZ::IsFiniteFloat(deltaTime) || deltaTime < 0.0f ||
            maximumIterations == 0 || !AZ::IsFiniteFloat(minimumTranslation) || minimumTranslation < 0.0f)
        {
            return false;
        }

        const b3WorldId nativeWorldId = b3LoadWorldId(worldId.m_value);
        const b3Capsule mover{ ToNative(center1), ToNative(center2), radius };
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = categoryBits;
        filter.maskBits = maskBits;
        const b3Pos target = ToNativePosition(position + translation);
        b3Pos current = ToNativePosition(position);
        const float toleranceSquared = minimumTranslation * minimumTranslation;

        const auto gatherPlanes = [&]()
        {
            data->m_entries.clear();
            data->m_solverPlanes.clear();
            data->m_results.clear();
            data->m_origin = current;
            data->m_ignoredBodyIdA = ignoredBodyIdA;
            data->m_ignoredBodyIdB = ignoredBodyIdB;
            data->m_requiredPlaneCount = 0;
            data->m_overflow = false;
            data->m_invalidPlane = false;
            const int planeCapacity = aznumeric_cast<int>(data->m_planeCapacity);
            if (b3World_CollideMoverWithCapacity(
                    nativeWorldId,
                    current,
                    &mover,
                    planeCapacity,
                    true,
                    b3LoadBodyId(ignoredBodyIdA.m_value),
                    b3LoadBodyId(ignoredBodyIdB.m_value),
                    filter,
                    CollectMoverPlanes,
                    data))
            {
                data->m_overflow = true;
                data->m_requiredPlaneCount = AZStd::max(data->m_requiredPlaneCount, data->m_planeCapacity + 1);
            }
            if (data->m_overflow)
            {
                result.m_overflow = true;
                result.m_requiredPlaneCount = aznumeric_cast<AZ::u32>(
                    AZStd::min(data->m_requiredPlaneCount, aznumeric_cast<size_t>((AZStd::numeric_limits<AZ::u32>::max)())));
                return false;
            }
            if (data->m_invalidPlane)
            {
                result.m_invalidPlane = true;
                return false;
            }
            AZStd::sort(data->m_entries.begin(), data->m_entries.end(), LessMoverEntry);
            size_t uniquePlaneCount = 0;
            for (size_t planeIndex = 0; planeIndex < data->m_entries.size(); ++planeIndex)
            {
                const MoverEntry& entry = data->m_entries[planeIndex];
                if (uniquePlaneCount > 0)
                {
                    MoverEntry& previous = data->m_entries[uniquePlaneCount - 1];
                    const float normalDot = b3Dot(previous.m_plane.plane.normal, entry.m_plane.plane.normal);
                    if (previous.m_result.m_shapeId == entry.m_result.m_shapeId && normalDot >= 0.99999f)
                    {
                        if (entry.m_plane.plane.offset > previous.m_plane.plane.offset)
                        {
                            previous = entry;
                        }
                        continue;
                    }
                }

                if (uniquePlaneCount != planeIndex)
                {
                    data->m_entries[uniquePlaneCount] = entry;
                }
                ++uniquePlaneCount;
            }
            data->m_entries.resize(uniquePlaneCount);
            for (const MoverEntry& entry : data->m_entries)
            {
                data->m_solverPlanes.push_back(entry.m_plane);
            }
            return true;
        };

        const auto publishPlanes = [&]()
        {
            for (size_t planeIndex = 0; planeIndex < data->m_entries.size(); ++planeIndex)
            {
                MoverPlane plane = data->m_entries[planeIndex].m_result;
                plane.m_push = data->m_solverPlanes[planeIndex].push;
                data->m_results.push_back(plane);
            }
            result.m_planeCount = aznumeric_cast<AZ::u32>(data->m_results.size());
            result.m_requiredPlaneCount = result.m_planeCount;
        };

        for (AZ::u32 iteration = 0; iteration < maximumIterations; ++iteration)
        {
            if (!gatherPlanes())
            {
                return false;
            }

            const b3Vec3 targetDelta = b3SubPos(target, current);
            const b3PlaneSolverResult solved =
                b3SolvePlanes(targetDelta, data->m_solverPlanes.data(), aznumeric_cast<int>(data->m_solverPlanes.size()));
            result.m_iterationCount += aznumeric_cast<AZ::u32>(AZStd::max(solved.iterationCount, 0));
            b3Vec3 step = solved.delta;
            const float fraction = b3World_CastMover(nativeWorldId, current, &mover, step, filter, FilterMoverShape, data);
            step *= fraction;
            current = b3OffsetPos(current, step);
            if (b3LengthSquared(step) <= toleranceSquared)
            {
                break;
            }
        }

        if (!gatherPlanes())
        {
            return false;
        }
        b3PlaneSolverResult finalSolve =
            b3SolvePlanes(b3SubPos(target, current), data->m_solverPlanes.data(), aznumeric_cast<int>(data->m_solverPlanes.size()));
        result.m_iterationCount += aznumeric_cast<AZ::u32>(AZStd::max(finalSolve.iterationCount, 0));
        if (b3LengthSquared(finalSolve.delta) > toleranceSquared)
        {
            const float fraction = b3World_CastMover(nativeWorldId, current, &mover, finalSolve.delta, filter, FilterMoverShape, data);
            finalSolve.delta *= fraction;
            current = b3OffsetPos(current, finalSolve.delta);
            if (!gatherPlanes())
            {
                return false;
            }
            finalSolve =
                b3SolvePlanes(b3SubPos(target, current), data->m_solverPlanes.data(), aznumeric_cast<int>(data->m_solverPlanes.size()));
            result.m_iterationCount += aznumeric_cast<AZ::u32>(AZStd::max(finalSolve.iterationCount, 0));
        }
        publishPlanes();
        result.m_position = FromNativePosition(current);
        if (deltaTime > 0.0f)
        {
            const b3Vec3 requestedVelocity = ToNative(translation / deltaTime);
            result.m_clippedVelocity =
                FromNative(b3ClipVector(requestedVelocity, data->m_solverPlanes.data(), aznumeric_cast<int>(data->m_solverPlanes.size())));
        }
        return true;
    }

    void CastRay(
        WorldId worldId,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        CastCallback callback,
        void* context)
    {
        if (!IsValid(worldId) || callback == nullptr)
        {
            return;
        }

        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = categoryBits;
        filter.maskBits = maskBits;
        CastContext castContext{ callback, context };
        const b3Pos origin = ToNativePosition(start);
        b3World_CastRay(b3LoadWorldId(worldId.m_value), origin, ToNative(translation), filter, OnCastHit, &castContext);
    }

    bool CastRayClosest(
        WorldId worldId,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        ClosestCastHit& hit)
    {
        if (!IsValid(worldId))
        {
            return false;
        }
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = categoryBits;
        filter.maskBits = maskBits;
        const b3RayResult result =
            b3World_CastRayClosest(b3LoadWorldId(worldId.m_value), ToNativePosition(start), ToNative(translation), filter);
        if (!result.hit)
        {
            return false;
        }

        hit = {};
        hit.m_shapeId = { b3StoreShapeId(result.shapeId) };
        hit.m_position = FromNativePosition(result.point);
        hit.m_normal = FromNative(result.normal);
        hit.m_fraction = result.fraction;
        hit.m_userMaterialId = result.userMaterialId;
        hit.m_triangleIndex = result.triangleIndex;
        hit.m_childIndex = result.childIndex;
        hit.m_nodeVisits = ToUnsigned(result.nodeVisits);
        hit.m_leafVisits = ToUnsigned(result.leafVisits);
        return true;
    }

    bool CastRay(
        BodyId bodyId, const AZ::Vector3& start, const AZ::Vector3& translation, AZ::u64 categoryBits, AZ::u64 maskBits, CastHit& hit)
    {
        if (!IsValid(bodyId))
        {
            return false;
        }

        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = categoryBits;
        filter.maskBits = maskBits;
        const b3Pos origin = ToNativePosition(start);
        const b3BodyId nativeBodyId = b3LoadBodyId(bodyId.m_value);
        const b3BodyCastResult result =
            b3Body_CastRay(nativeBodyId, origin, ToNative(translation), filter, 1.0f, b3Body_GetTransform(nativeBodyId));
        if (!result.hit)
        {
            return false;
        }
        const int normalizedChildIndex = b3Shape_GetType(result.shapeId) == b3_compoundShape ? result.childIndex : -1;

        hit = { { b3StoreShapeId(result.shapeId) },
                b3Shape_GetUserData(result.shapeId),
                FromNativePosition(result.point),
                FromNative(result.normal),
                result.fraction,
                result.userMaterialId,
                result.triangleIndex,
                normalizedChildIndex };
        return true;
    }

    bool CastShape(
        BodyId bodyId,
        const AZ::Vector3& origin,
        AZStd::span<const AZ::Vector3> points,
        float radius,
        const AZ::Vector3& translation,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        CastHit& hit)
    {
        hit = {};
        if (!IsValid(bodyId) || !origin.IsFinite() || !translation.IsFinite() || !AZ::IsFiniteFloat(radius) || radius < 0.0f ||
            points.empty() || points.size() > B3_MAX_SHAPE_CAST_POINTS ||
            !AZStd::all_of(
                points.begin(),
                points.end(),
                [](const AZ::Vector3& point)
                {
                    return point.IsFinite();
                }))
        {
            return false;
        }

        b3Vec3 nativePoints[B3_MAX_SHAPE_CAST_POINTS];
        const b3ShapeProxy proxy = ToNativeProxy(points, radius, nativePoints);
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = categoryBits;
        filter.maskBits = maskBits;
        const b3BodyId nativeBodyId = b3LoadBodyId(bodyId.m_value);
        const b3BodyCastResult result = b3Body_CastShape(
            nativeBodyId, ToNativePosition(origin), &proxy, ToNative(translation), filter, 1.0f, false, b3Body_GetTransform(nativeBodyId));
        if (!result.hit)
        {
            return false;
        }

        hit = { { b3StoreShapeId(result.shapeId) },
                b3Shape_GetUserData(result.shapeId),
                FromNativePosition(result.point),
                FromNative(result.normal),
                result.fraction,
                result.userMaterialId,
                result.triangleIndex,
                b3Shape_GetType(result.shapeId) == b3_compoundShape ? result.childIndex : -1 };
        return true;
    }

    bool OverlapShape(
        BodyId bodyId,
        const AZ::Vector3& origin,
        AZStd::span<const AZ::Vector3> points,
        float radius,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        OverlapPredicate predicate,
        void* context)
    {
        if (!IsValid(bodyId) || !origin.IsFinite() || !AZ::IsFiniteFloat(radius) || radius < 0.0f || points.empty() ||
            points.size() > B3_MAX_SHAPE_CAST_POINTS ||
            !AZStd::all_of(
                points.begin(),
                points.end(),
                [](const AZ::Vector3& point)
                {
                    return point.IsFinite();
                }))
        {
            return false;
        }

        b3Vec3 nativePoints[B3_MAX_SHAPE_CAST_POINTS];
        const b3ShapeProxy proxy = ToNativeProxy(points, radius, nativePoints);
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = categoryBits;
        filter.maskBits = maskBits;
        const b3BodyId nativeBodyId = b3LoadBodyId(bodyId.m_value);
        if (predicate == nullptr)
        {
            return b3Body_OverlapShape(nativeBodyId, ToNativePosition(origin), &proxy, filter, b3Body_GetTransform(nativeBodyId));
        }

        BodyOverlapContext overlapContext{ nativeBodyId, predicate, context };
        b3World_OverlapShape(b3Body_GetWorld(nativeBodyId), ToNativePosition(origin), &proxy, filter, OnBodyOverlapHit, &overlapContext);
        return overlapContext.m_overlapped;
    }

    bool CastRaySphere(
        const AZ::Vector3& center, float radius, const AZ::Vector3& start, const AZ::Vector3& translation, GeometryCastHit& hit)
    {
        if (!center.IsFinite() || !start.IsFinite() || !translation.IsFinite() || !AZ::IsFiniteFloat(radius) || radius < 0.0f)
        {
            return false;
        }

        const b3Sphere sphere{ ToNative(center), radius };
        const b3RayCastInput input{ ToNative(start), ToNative(translation), 1.0f };
        return ConvertCastOutput(b3RayCastSphere(&sphere, &input), hit);
    }

    bool CastRayCapsule(
        const AZ::Vector3& center1,
        const AZ::Vector3& center2,
        float radius,
        const AZ::Vector3& start,
        const AZ::Vector3& translation,
        GeometryCastHit& hit)
    {
        if (!center1.IsFinite() || !center2.IsFinite() || !start.IsFinite() || !translation.IsFinite() || !AZ::IsFiniteFloat(radius) ||
            radius < 0.0f)
        {
            return false;
        }

        const b3Capsule capsule{ ToNative(center1), ToNative(center2), radius };
        const b3RayCastInput input{ ToNative(start), ToNative(translation), 1.0f };
        return ConvertCastOutput(b3RayCastCapsule(&capsule, &input), hit);
    }

    bool CastRay(const HullGeometry& geometry, const AZ::Vector3& start, const AZ::Vector3& translation, GeometryCastHit& hit)
    {
        const b3HullData* hull = GeometryCastAccess::Get(geometry);
        if (hull == nullptr || !start.IsFinite() || !translation.IsFinite())
        {
            return false;
        }

        const b3RayCastInput input{ ToNative(start), ToNative(translation), 1.0f };
        return ConvertCastOutput(b3RayCastHull(hull, &input), hit);
    }

    bool CastRay(const MeshGeometry& geometry, const AZ::Vector3& start, const AZ::Vector3& translation, GeometryCastHit& hit)
    {
        const b3MeshData* meshData = GeometryCastAccess::Get(geometry);
        if (meshData == nullptr || !start.IsFinite() || !translation.IsFinite())
        {
            return false;
        }

        const b3Mesh mesh{ meshData, b3Vec3{ 1.0f, 1.0f, 1.0f } };
        const b3RayCastInput input{ ToNative(start), ToNative(translation), 1.0f };
        return ConvertCastOutput(b3RayCastMesh(&mesh, &input), hit);
    }

    bool CastRay(const HeightFieldGeometry& geometry, const AZ::Vector3& start, const AZ::Vector3& translation, GeometryCastHit& hit)
    {
        const b3HeightFieldData* heightField = GeometryCastAccess::Get(geometry);
        if (heightField == nullptr || !start.IsFinite() || !translation.IsFinite())
        {
            return false;
        }

        const b3RayCastInput input{ ToNative(start), ToNative(translation), 1.0f };
        return ConvertCastOutput(b3RayCastHeightField(heightField, &input), hit);
    }

    bool CastRay(const CompoundGeometry& geometry, const AZ::Vector3& start, const AZ::Vector3& translation, GeometryCastHit& hit)
    {
        const b3CompoundData* compound = GeometryCastAccess::Get(geometry);
        if (compound == nullptr || !start.IsFinite() || !translation.IsFinite())
        {
            return false;
        }

        const b3RayCastInput input{ ToNative(start), ToNative(translation), 1.0f };
        return ConvertCastOutput(b3RayCastCompound(compound, &input), hit);
    }

    AZ::Aabb GetAabb(const CompoundGeometry& geometry, const AZ::Transform& transform)
    {
        const b3CompoundData* compound = GeometryCastAccess::Get(geometry);
        if (compound == nullptr || !transform.GetTranslation().IsFinite() || !transform.GetRotation().IsFinite())
        {
            return AZ::Aabb::CreateNull();
        }

        const b3WorldTransform nativeTransform = ToNative(transform);
        return FromNative(b3ComputeCompoundAABB(compound, { ToNative(transform.GetTranslation()), nativeTransform.q }));
    }

    void CastShape(
        WorldId worldId,
        const AZ::Vector3& origin,
        AZStd::span<const AZ::Vector3> points,
        float radius,
        const AZ::Vector3& translation,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        CastCallback callback,
        void* context)
    {
        if (!IsValid(worldId) || points.empty() || points.size() > B3_MAX_SHAPE_CAST_POINTS || callback == nullptr)
        {
            return;
        }

        b3Vec3 nativePoints[B3_MAX_SHAPE_CAST_POINTS];
        const b3ShapeProxy proxy = ToNativeProxy(points, radius, nativePoints);
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = categoryBits;
        filter.maskBits = maskBits;
        CastContext castContext{ callback, context };
        const b3Pos nativeOrigin = ToNativePosition(origin);
        b3World_CastShape(b3LoadWorldId(worldId.m_value), nativeOrigin, &proxy, ToNative(translation), filter, OnCastHit, &castContext);
    }

    TreeVisitStatistics OverlapShape(
        WorldId worldId,
        const AZ::Vector3& origin,
        AZStd::span<const AZ::Vector3> points,
        float radius,
        AZ::u64 categoryBits,
        AZ::u64 maskBits,
        OverlapCallback callback,
        void* context)
    {
        if (!IsValid(worldId) || points.empty() || points.size() > B3_MAX_SHAPE_CAST_POINTS || callback == nullptr)
        {
            return {};
        }
        AZ_PROFILE_SCOPE(Physics, "Box3D::Native::OverlapShape");

        b3Vec3 nativePoints[B3_MAX_SHAPE_CAST_POINTS];
        const b3ShapeProxy proxy = ToNativeProxy(points, radius, nativePoints);
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = categoryBits;
        filter.maskBits = maskBits;
        OverlapContext overlapContext{ callback, context };
        const b3Pos nativeOrigin = ToNativePosition(origin);
        const b3TreeStats statistics =
            b3World_OverlapShape(b3LoadWorldId(worldId.m_value), nativeOrigin, &proxy, filter, OnOverlapHit, &overlapContext);
        return { ToUnsigned(statistics.nodeVisits), ToUnsigned(statistics.leafVisits) };
    }

    TreeVisitStatistics OverlapAabb(
        WorldId worldId, const AZ::Aabb& bounds, AZ::u64 categoryBits, AZ::u64 maskBits, OverlapCallback callback, void* context)
    {
        if (!IsValid(worldId) || !bounds.IsValid() || callback == nullptr)
        {
            return {};
        }
        AZ_PROFILE_SCOPE(Physics, "Box3D::Native::OverlapAabb");

        const AZ::Vector3 minimum = bounds.GetMin();
        const AZ::Vector3 maximum = bounds.GetMax();
        const b3AABB nativeBounds = { { minimum.GetX(), -maximum.GetZ(), minimum.GetY() },
                                      { maximum.GetX(), -minimum.GetZ(), maximum.GetY() } };
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = categoryBits;
        filter.maskBits = maskBits;
        OverlapContext overlapContext{ callback, context };
        const b3TreeStats statistics =
            b3World_OverlapAABB(b3LoadWorldId(worldId.m_value), nativeBounds, filter, OnOverlapHit, &overlapContext);
        return { ToUnsigned(statistics.nodeVisits), ToUnsigned(statistics.leafVisits) };
    }

    BodyId CreateBody(WorldId worldId, const BodyConfiguration& configuration)
    {
        b3BodyDef definition = b3DefaultBodyDef();
        definition.type = ToNative(configuration.m_type);
        const b3WorldTransform transform = ToNative(configuration.m_transform);
        definition.position = transform.p;
        definition.rotation = transform.q;
        definition.linearVelocity = ToNative(configuration.m_linearVelocity);
        definition.angularVelocity = ToNative(configuration.m_angularVelocity);
        definition.linearDamping = configuration.m_linearDamping;
        definition.angularDamping = configuration.m_angularDamping;
        definition.gravityScale = configuration.m_gravityScale;
        definition.sleepThreshold = configuration.m_sleepThreshold;
        definition.name = configuration.m_name.data();
        definition.userData = configuration.m_userData;
        definition.motionLocks = { configuration.m_motionLocks.m_linearX,  configuration.m_motionLocks.m_linearZ,
                                   configuration.m_motionLocks.m_linearY,  configuration.m_motionLocks.m_angularX,
                                   configuration.m_motionLocks.m_angularZ, configuration.m_motionLocks.m_angularY };
        definition.enableSleep = configuration.m_enableSleep;
        definition.isAwake = configuration.m_isAwake;
        definition.isBullet = configuration.m_isBullet;
        definition.isEnabled = configuration.m_isEnabled;
        definition.allowFastRotation = configuration.m_allowFastRotation;
        definition.enableContactRecycling = configuration.m_enableContactRecycling;
        return { b3StoreBodyId(b3CreateBody(b3LoadWorldId(worldId.m_value), &definition)) };
    }

    void DestroyBody(BodyId bodyId)
    {
        if (IsValid(bodyId))
        {
            b3DestroyBody(b3LoadBodyId(bodyId.m_value));
        }
    }

    bool IsValid(BodyId bodyId)
    {
        return bodyId.IsValid() && b3Body_IsValid(b3LoadBodyId(bodyId.m_value));
    }

    NativeBodyType GetBodyType(BodyId bodyId)
    {
        return FromNative(b3Body_GetType(b3LoadBodyId(bodyId.m_value)));
    }

    bool SetBodyType(BodyId bodyId, NativeBodyType type)
    {
        if (!IsValid(bodyId))
        {
            return false;
        }
        const b3BodyId nativeId = b3LoadBodyId(bodyId.m_value);
        b3Body_SetType(nativeId, ToNative(type));
        return b3Body_GetType(nativeId) == ToNative(type);
    }

    void SetBodyName(BodyId bodyId, AZ::Name name)
    {
        b3Body_SetName(b3LoadBodyId(bodyId.m_value), name.IsEmpty() ? nullptr : name.GetCStr());
    }

    AZ::Transform GetBodyTransform(BodyId bodyId)
    {
        return FromNative(b3Body_GetTransform(b3LoadBodyId(bodyId.m_value)));
    }

    void SetBodyTransform(BodyId bodyId, const AZ::Transform& transform)
    {
        const b3WorldTransform nativeTransform = ToNative(transform);
        b3Body_SetTransform(b3LoadBodyId(bodyId.m_value), nativeTransform.p, nativeTransform.q);
    }

    AZ::Vector3 GetBodyLocalPoint(BodyId bodyId, const AZ::Vector3& worldPoint)
    {
        return FromNative(b3Body_GetLocalPoint(b3LoadBodyId(bodyId.m_value), ToNativePosition(worldPoint)));
    }

    AZ::Vector3 GetBodyWorldPoint(BodyId bodyId, const AZ::Vector3& localPoint)
    {
        return FromNativePosition(b3Body_GetWorldPoint(b3LoadBodyId(bodyId.m_value), ToNative(localPoint)));
    }

    AZ::Vector3 GetBodyLocalVector(BodyId bodyId, const AZ::Vector3& worldVector)
    {
        return FromNative(b3Body_GetLocalVector(b3LoadBodyId(bodyId.m_value), ToNative(worldVector)));
    }

    AZ::Vector3 GetBodyWorldVector(BodyId bodyId, const AZ::Vector3& localVector)
    {
        return FromNative(b3Body_GetWorldVector(b3LoadBodyId(bodyId.m_value), ToNative(localVector)));
    }

    AZ::Vector3 GetLinearVelocity(BodyId bodyId)
    {
        return FromNative(b3Body_GetLinearVelocity(b3LoadBodyId(bodyId.m_value)));
    }

    void SetLinearVelocity(BodyId bodyId, const AZ::Vector3& velocity)
    {
        b3Body_SetLinearVelocity(b3LoadBodyId(bodyId.m_value), ToNative(velocity));
    }

    AZ::Vector3 GetAngularVelocity(BodyId bodyId)
    {
        return FromNative(b3Body_GetAngularVelocity(b3LoadBodyId(bodyId.m_value)));
    }

    void SetAngularVelocity(BodyId bodyId, const AZ::Vector3& velocity)
    {
        b3Body_SetAngularVelocity(b3LoadBodyId(bodyId.m_value), ToNative(velocity));
    }

    AZ::Vector3 GetLinearVelocityAtLocalPoint(BodyId bodyId, const AZ::Vector3& localPoint)
    {
        return FromNative(b3Body_GetLocalPointVelocity(b3LoadBodyId(bodyId.m_value), ToNative(localPoint)));
    }

    AZ::Vector3 GetLinearVelocityAtWorldPoint(BodyId bodyId, const AZ::Vector3& worldPoint)
    {
        const b3Pos point = ToNativePosition(worldPoint);
        return FromNative(b3Body_GetWorldPointVelocity(b3LoadBodyId(bodyId.m_value), point));
    }

    void SetKinematicTarget(BodyId bodyId, const AZ::Transform& transform, float timeStep, bool wake)
    {
        b3Body_SetTargetTransform(b3LoadBodyId(bodyId.m_value), ToNative(transform), timeStep, wake);
    }

    void ApplyLinearImpulse(BodyId bodyId, const AZ::Vector3& impulse, bool wake)
    {
        b3Body_ApplyLinearImpulseToCenter(b3LoadBodyId(bodyId.m_value), ToNative(impulse), wake);
    }

    void ApplyLinearImpulse(BodyId bodyId, const AZ::Vector3& impulse, const AZ::Vector3& worldPoint, bool wake)
    {
        const b3Pos point = ToNativePosition(worldPoint);
        b3Body_ApplyLinearImpulse(b3LoadBodyId(bodyId.m_value), ToNative(impulse), point, wake);
    }

    void ApplyAngularImpulse(BodyId bodyId, const AZ::Vector3& impulse, bool wake)
    {
        b3Body_ApplyAngularImpulse(b3LoadBodyId(bodyId.m_value), ToNative(impulse), wake);
    }

    NativeMassProperties GetMassProperties(BodyId bodyId)
    {
        const b3MassData massData = b3Body_GetMassData(b3LoadBodyId(bodyId.m_value));
        return { FromNative(massData.inertia), FromNative(massData.center), massData.mass };
    }

    NativeMassProperties GetShapeMassProperties(ShapeId shapeId)
    {
        if (!IsValid(shapeId))
        {
            return {};
        }

        const b3MassData massData = b3Shape_ComputeMassData(b3LoadShapeId(shapeId.m_value));
        return { FromNative(massData.inertia), FromNative(massData.center), massData.mass };
    }

    void SetMassProperties(BodyId bodyId, const NativeMassProperties& properties)
    {
        const b3MassData massData = { properties.m_mass, ToNative(properties.m_center), ToNative(properties.m_inertia) };
        b3Body_SetMassData(b3LoadBodyId(bodyId.m_value), massData);
    }

    void ApplyMassFromShapes(BodyId bodyId)
    {
        b3Body_ApplyMassFromShapes(b3LoadBodyId(bodyId.m_value));
    }

    AZ::Matrix3x3 GetWorldInverseInertia(BodyId bodyId)
    {
        return FromNative(b3Body_GetWorldInverseRotationalInertia(b3LoadBodyId(bodyId.m_value)));
    }

    AZ::Vector3 GetWorldCenterOfMass(BodyId bodyId)
    {
        return FromNativePosition(b3Body_GetWorldCenterOfMass(b3LoadBodyId(bodyId.m_value)));
    }

    float GetLinearDamping(BodyId bodyId)
    {
        return b3Body_GetLinearDamping(b3LoadBodyId(bodyId.m_value));
    }

    void SetLinearDamping(BodyId bodyId, float damping)
    {
        b3Body_SetLinearDamping(b3LoadBodyId(bodyId.m_value), damping);
    }

    float GetAngularDamping(BodyId bodyId)
    {
        return b3Body_GetAngularDamping(b3LoadBodyId(bodyId.m_value));
    }

    void SetAngularDamping(BodyId bodyId, float damping)
    {
        b3Body_SetAngularDamping(b3LoadBodyId(bodyId.m_value), damping);
    }

    float GetGravityScale(BodyId bodyId)
    {
        return b3Body_GetGravityScale(b3LoadBodyId(bodyId.m_value));
    }

    void SetGravityScale(BodyId bodyId, float scale)
    {
        b3Body_SetGravityScale(b3LoadBodyId(bodyId.m_value), scale);
    }

    bool IsAwake(BodyId bodyId)
    {
        return b3Body_IsAwake(b3LoadBodyId(bodyId.m_value));
    }

    void SetAwake(BodyId bodyId, bool awake)
    {
        b3Body_SetAwake(b3LoadBodyId(bodyId.m_value), awake);
    }

    float GetSleepThreshold(BodyId bodyId)
    {
        return b3Body_GetSleepThreshold(b3LoadBodyId(bodyId.m_value));
    }

    void SetSleepThreshold(BodyId bodyId, float threshold)
    {
        b3Body_SetSleepThreshold(b3LoadBodyId(bodyId.m_value), threshold);
    }

    bool IsBodyEnabled(BodyId bodyId)
    {
        return b3Body_IsEnabled(b3LoadBodyId(bodyId.m_value));
    }

    void SetBodyEnabled(BodyId bodyId, bool enabled)
    {
        if (enabled)
        {
            b3Body_Enable(b3LoadBodyId(bodyId.m_value));
        }
        else
        {
            b3Body_Disable(b3LoadBodyId(bodyId.m_value));
        }
    }

    void SetBodyHitEventsEnabled(BodyId bodyId, bool enabled)
    {
        if (IsValid(bodyId))
        {
            b3Body_EnableHitEvents(b3LoadBodyId(bodyId.m_value), enabled);
        }
    }

    void ApplyForce(BodyId bodyId, const AZ::Vector3& force, bool wake)
    {
        if (IsValid(bodyId) && force.IsFinite())
        {
            b3Body_ApplyForceToCenter(b3LoadBodyId(bodyId.m_value), ToNative(force), wake);
        }
    }

    void ApplyForce(BodyId bodyId, const AZ::Vector3& force, const AZ::Vector3& worldPoint, bool wake)
    {
        if (IsValid(bodyId) && force.IsFinite() && worldPoint.IsFinite())
        {
            b3Body_ApplyForce(b3LoadBodyId(bodyId.m_value), ToNative(force), ToNativePosition(worldPoint), wake);
        }
    }

    void ApplyTorque(BodyId bodyId, const AZ::Vector3& torque, bool wake)
    {
        if (IsValid(bodyId) && torque.IsFinite())
        {
            b3Body_ApplyTorque(b3LoadBodyId(bodyId.m_value), ToNative(torque), wake);
        }
    }

    NativeMotionLocks GetMotionLocks(BodyId bodyId)
    {
        if (!IsValid(bodyId))
        {
            return {};
        }
        const b3MotionLocks locks = b3Body_GetMotionLocks(b3LoadBodyId(bodyId.m_value));
        return { locks.linearX, locks.linearZ, locks.linearY, locks.angularX, locks.angularZ, locks.angularY };
    }

    void SetMotionLocks(BodyId bodyId, const NativeMotionLocks& locks)
    {
        if (IsValid(bodyId))
        {
            b3Body_SetMotionLocks(
                b3LoadBodyId(bodyId.m_value),
                { locks.m_linearX, locks.m_linearZ, locks.m_linearY, locks.m_angularX, locks.m_angularZ, locks.m_angularY });
        }
    }

    void SetBullet(BodyId bodyId, bool enabled)
    {
        b3Body_SetBullet(b3LoadBodyId(bodyId.m_value), enabled);
    }

    bool IsBullet(BodyId bodyId)
    {
        return b3Body_IsBullet(b3LoadBodyId(bodyId.m_value));
    }

    void SetBodySleepEnabled(BodyId bodyId, bool enabled)
    {
        if (IsValid(bodyId))
        {
            b3Body_EnableSleep(b3LoadBodyId(bodyId.m_value), enabled);
        }
    }

    bool IsBodySleepEnabled(BodyId bodyId)
    {
        return IsValid(bodyId) && b3Body_IsSleepEnabled(b3LoadBodyId(bodyId.m_value));
    }

    void SetBodyContactRecyclingEnabled(BodyId bodyId, bool enabled)
    {
        if (IsValid(bodyId))
        {
            b3Body_EnableContactRecycling(b3LoadBodyId(bodyId.m_value), enabled);
        }
    }

    bool IsBodyContactRecyclingEnabled(BodyId bodyId)
    {
        return IsValid(bodyId) && b3Body_IsContactRecyclingEnabled(b3LoadBodyId(bodyId.m_value));
    }

    AZ::Aabb GetBodyAabb(BodyId bodyId)
    {
        if (!IsValid(bodyId))
        {
            return AZ::Aabb::CreateNull();
        }
        const b3AABB aabb = b3Body_ComputeAABB(b3LoadBodyId(bodyId.m_value));
        return FromNative(aabb);
    }

    bool GetBodyClosestPoint(BodyId bodyId, const AZ::Vector3& target, AZ::Vector3& position, float& distance)
    {
        position = AZ::Vector3::CreateZero();
        distance = 0.0f;
        if (!IsValid(bodyId) || !target.IsFinite())
        {
            return false;
        }

        b3Vec3 nativePosition;
        const float nativeDistance = b3Body_GetClosestPoint(b3LoadBodyId(bodyId.m_value), &nativePosition, ToNative(target));
        if (!(std::isfinite)(nativeDistance) || nativeDistance == (AZStd::numeric_limits<float>::max)())
        {
            return false;
        }

        position = FromNative(nativePosition);
        distance = nativeDistance;
        return position.IsFinite();
    }

    bool GetBodyJoints(BodyId bodyId, JointScratch& scratch)
    {
        auto* data = static_cast<JointScratchData*>(scratch.m_data);
        if (!IsValid(bodyId) || data == nullptr)
        {
            return false;
        }

        const b3BodyId nativeBodyId = b3LoadBodyId(bodyId.m_value);
        const int jointCount = b3Body_GetJointCount(nativeBodyId);
        if (jointCount == 0)
        {
            data->m_nativeJoints.clear();
            data->m_joints.clear();
            return true;
        }
        data->m_nativeJoints.resize(aznumeric_cast<size_t>(jointCount));
        const int storedCount = b3Body_GetJoints(nativeBodyId, data->m_nativeJoints.data(), jointCount);
        if (storedCount < 0 || storedCount > jointCount)
        {
            data->m_joints.clear();
            return false;
        }
        data->m_joints.resize(aznumeric_cast<size_t>(storedCount));
        for (int jointIndex = 0; jointIndex < storedCount; ++jointIndex)
        {
            data->m_joints[aznumeric_cast<size_t>(jointIndex)] = { b3StoreJointId(
                data->m_nativeJoints[aznumeric_cast<size_t>(jointIndex)]) };
        }
        return storedCount == jointCount;
    }

    JointId CreateJoint(WorldId worldId, const NativeJointConfiguration& configuration)
    {
        if (!IsValid(worldId) || !IsValid(configuration.m_bodyA) || !IsValid(configuration.m_bodyB))
        {
            return {};
        }
        if (configuration.m_kind == JointKind::Spherical &&
            (!(std::isfinite)(configuration.m_spherical.m_coneAngle) || configuration.m_spherical.m_coneAngle < 0.0f ||
             configuration.m_spherical.m_coneAngle > SphericalJointConfiguration::MaximumConeAngle))
        {
            return {};
        }

        const b3BodyId bodyA = b3LoadBodyId(configuration.m_bodyA.m_value);
        const b3BodyId bodyB = b3LoadBodyId(configuration.m_bodyB.m_value);
        const b3WorldId world = b3LoadWorldId(worldId.m_value);
        if (b3StoreWorldId(b3Body_GetWorld(bodyA)) != worldId.m_value || b3StoreWorldId(b3Body_GetWorld(bodyB)) != worldId.m_value)
        {
            return {};
        }

        const b3Transform frameA = ToNativeJointFrame(configuration.m_localFrameA);
        const b3Transform frameB = ToNativeJointFrame(configuration.m_localFrameB);
        const auto configureBase = [&](b3JointDef& base)
        {
            base.bodyIdA = bodyA;
            base.bodyIdB = bodyB;
            base.localFrameA = frameA;
            base.localFrameB = frameB;
            base.userData = configuration.m_userData;
            base.forceThreshold = configuration.m_forceThreshold;
            base.torqueThreshold = configuration.m_torqueThreshold;
            base.constraintHertz = configuration.m_constraintHertz;
            base.constraintDampingRatio = configuration.m_constraintDampingRatio;
            base.drawScale = configuration.m_drawScale;
            base.collideConnected = configuration.m_collideConnected;
        };

        b3JointId jointId = b3_nullJointId;
        switch (configuration.m_kind)
        {
        case JointKind::Parallel:
            {
                b3ParallelJointDef definition = b3DefaultParallelJointDef();
                configureBase(definition.base);
                definition.hertz = configuration.m_parallel.m_hertz;
                definition.dampingRatio = configuration.m_parallel.m_dampingRatio;
                definition.maxTorque = configuration.m_parallel.m_maxTorque;
                jointId = b3CreateParallelJoint(world, &definition);
                break;
            }
        case JointKind::Distance:
            {
                b3DistanceJointDef definition = b3DefaultDistanceJointDef();
                configureBase(definition.base);
                definition.length = configuration.m_distance.m_length;
                definition.lowerSpringForce = configuration.m_distance.m_lowerSpringForce;
                definition.upperSpringForce = configuration.m_distance.m_upperSpringForce;
                definition.hertz = configuration.m_distance.m_hertz;
                definition.dampingRatio = configuration.m_distance.m_dampingRatio;
                definition.minLength = configuration.m_distance.m_minLength;
                definition.maxLength = configuration.m_distance.m_maxLength;
                definition.maxMotorForce = configuration.m_distance.m_maxMotorForce;
                definition.motorSpeed = configuration.m_distance.m_motorSpeed;
                definition.enableSpring = configuration.m_distance.m_enableSpring;
                definition.enableLimit = configuration.m_distance.m_enableLimit;
                definition.enableMotor = configuration.m_distance.m_enableMotor;
                jointId = b3CreateDistanceJoint(world, &definition);
                break;
            }
        case JointKind::Filter:
            {
                b3FilterJointDef definition = b3DefaultFilterJointDef();
                configureBase(definition.base);
                jointId = b3CreateFilterJoint(world, &definition);
                break;
            }
        case JointKind::Motor:
            {
                b3MotorJointDef definition = b3DefaultMotorJointDef();
                configureBase(definition.base);
                definition.linearVelocity = ToNative(configuration.m_motor.m_linearVelocity);
                definition.angularVelocity = ToNative(configuration.m_motor.m_angularVelocity);
                definition.maxVelocityForce = configuration.m_motor.m_maxVelocityForce;
                definition.maxVelocityTorque = configuration.m_motor.m_maxVelocityTorque;
                definition.linearHertz = configuration.m_motor.m_linearHertz;
                definition.linearDampingRatio = configuration.m_motor.m_linearDampingRatio;
                definition.maxSpringForce = configuration.m_motor.m_maxSpringForce;
                definition.angularHertz = configuration.m_motor.m_angularHertz;
                definition.angularDampingRatio = configuration.m_motor.m_angularDampingRatio;
                definition.maxSpringTorque = configuration.m_motor.m_maxSpringTorque;
                jointId = b3CreateMotorJoint(world, &definition);
                break;
            }
        case JointKind::Prismatic:
            {
                b3PrismaticJointDef definition = b3DefaultPrismaticJointDef();
                configureBase(definition.base);
                definition.hertz = configuration.m_prismatic.m_hertz;
                definition.dampingRatio = configuration.m_prismatic.m_dampingRatio;
                definition.targetTranslation = configuration.m_prismatic.m_targetTranslation;
                definition.lowerTranslation = configuration.m_prismatic.m_lowerTranslation;
                definition.upperTranslation = configuration.m_prismatic.m_upperTranslation;
                definition.maxMotorForce = configuration.m_prismatic.m_maxMotorForce;
                definition.motorSpeed = configuration.m_prismatic.m_motorSpeed;
                definition.enableSpring = configuration.m_prismatic.m_enableSpring;
                definition.enableLimit = configuration.m_prismatic.m_enableLimit;
                definition.enableMotor = configuration.m_prismatic.m_enableMotor;
                jointId = b3CreatePrismaticJoint(world, &definition);
                break;
            }
        case JointKind::Revolute:
            {
                b3RevoluteJointDef definition = b3DefaultRevoluteJointDef();
                configureBase(definition.base);
                definition.targetAngle = configuration.m_revolute.m_targetAngle;
                definition.hertz = configuration.m_revolute.m_hertz;
                definition.dampingRatio = configuration.m_revolute.m_dampingRatio;
                definition.lowerAngle = configuration.m_revolute.m_lowerAngle;
                definition.upperAngle = configuration.m_revolute.m_upperAngle;
                definition.maxMotorTorque = configuration.m_revolute.m_maxMotorTorque;
                definition.motorSpeed = configuration.m_revolute.m_motorSpeed;
                definition.enableSpring = configuration.m_revolute.m_enableSpring;
                definition.enableLimit = configuration.m_revolute.m_enableLimit;
                definition.enableMotor = configuration.m_revolute.m_enableMotor;
                jointId = b3CreateRevoluteJoint(world, &definition);
                break;
            }
        case JointKind::Spherical:
            {
                b3SphericalJointDef definition = b3DefaultSphericalJointDef();
                configureBase(definition.base);
                definition.targetRotation = ToNative(configuration.m_spherical.m_targetRotation);
                definition.motorVelocity = ToNative(configuration.m_spherical.m_motorVelocity);
                definition.hertz = configuration.m_spherical.m_hertz;
                definition.dampingRatio = configuration.m_spherical.m_dampingRatio;
                definition.coneAngle = configuration.m_spherical.m_coneAngle;
                definition.lowerTwistAngle = configuration.m_spherical.m_lowerTwistAngle;
                definition.upperTwistAngle = configuration.m_spherical.m_upperTwistAngle;
                definition.maxMotorTorque = configuration.m_spherical.m_maxMotorTorque;
                definition.enableSpring = configuration.m_spherical.m_enableSpring;
                definition.enableConeLimit = configuration.m_spherical.m_enableConeLimit;
                definition.enableTwistLimit = configuration.m_spherical.m_enableTwistLimit;
                definition.enableMotor = configuration.m_spherical.m_enableMotor;
                jointId = b3CreateSphericalJoint(world, &definition);
                break;
            }
        case JointKind::Weld:
            {
                b3WeldJointDef definition = b3DefaultWeldJointDef();
                configureBase(definition.base);
                definition.linearHertz = configuration.m_weld.m_linearHertz;
                definition.angularHertz = configuration.m_weld.m_angularHertz;
                definition.linearDampingRatio = configuration.m_weld.m_linearDampingRatio;
                definition.angularDampingRatio = configuration.m_weld.m_angularDampingRatio;
                jointId = b3CreateWeldJoint(world, &definition);
                break;
            }
        case JointKind::Wheel:
            {
                b3WheelJointDef definition = b3DefaultWheelJointDef();
                configureBase(definition.base);
                definition.suspensionHertz = configuration.m_wheel.m_suspensionHertz;
                definition.suspensionDampingRatio = configuration.m_wheel.m_suspensionDampingRatio;
                definition.lowerSuspensionLimit = configuration.m_wheel.m_lowerSuspensionLimit;
                definition.upperSuspensionLimit = configuration.m_wheel.m_upperSuspensionLimit;
                definition.maxSpinTorque = configuration.m_wheel.m_maxSpinTorque;
                definition.spinSpeed = configuration.m_wheel.m_spinSpeed;
                definition.steeringHertz = configuration.m_wheel.m_steeringHertz;
                definition.steeringDampingRatio = configuration.m_wheel.m_steeringDampingRatio;
                definition.targetSteeringAngle = configuration.m_wheel.m_targetSteeringAngle;
                definition.maxSteeringTorque = configuration.m_wheel.m_maxSteeringTorque;
                definition.lowerSteeringLimit = configuration.m_wheel.m_lowerSteeringLimit;
                definition.upperSteeringLimit = configuration.m_wheel.m_upperSteeringLimit;
                definition.enableSuspensionSpring = configuration.m_wheel.m_enableSuspensionSpring;
                definition.enableSuspensionLimit = configuration.m_wheel.m_enableSuspensionLimit;
                definition.enableSpinMotor = configuration.m_wheel.m_enableSpinMotor;
                definition.enableSteering = configuration.m_wheel.m_enableSteering;
                definition.enableSteeringLimit = configuration.m_wheel.m_enableSteeringLimit;
                jointId = b3CreateWheelJoint(world, &definition);
                break;
            }
        }
        return { b3StoreJointId(jointId) };
    }

    bool UpdateJoint(JointId jointId, const NativeJointConfiguration& configuration)
    {
        if (!IsValid(jointId))
        {
            return false;
        }
        if (configuration.m_kind == JointKind::Spherical &&
            (!(std::isfinite)(configuration.m_spherical.m_coneAngle) || configuration.m_spherical.m_coneAngle < 0.0f ||
             configuration.m_spherical.m_coneAngle > SphericalJointConfiguration::MaximumConeAngle))
        {
            return false;
        }

        const b3JointId joint = b3LoadJointId(jointId.m_value);
        b3JointType expectedType = b3_filterJoint;
        switch (configuration.m_kind)
        {
        case JointKind::Parallel:
            expectedType = b3_parallelJoint;
            break;
        case JointKind::Distance:
            expectedType = b3_distanceJoint;
            break;
        case JointKind::Filter:
            expectedType = b3_filterJoint;
            break;
        case JointKind::Motor:
            expectedType = b3_motorJoint;
            break;
        case JointKind::Prismatic:
            expectedType = b3_prismaticJoint;
            break;
        case JointKind::Revolute:
            expectedType = b3_revoluteJoint;
            break;
        case JointKind::Spherical:
            expectedType = b3_sphericalJoint;
            break;
        case JointKind::Weld:
            expectedType = b3_weldJoint;
            break;
        case JointKind::Wheel:
            expectedType = b3_wheelJoint;
            break;
        }
        if (b3Joint_GetType(joint) != expectedType)
        {
            return false;
        }

        b3Joint_SetLocalFrameA(joint, ToNativeJointFrame(configuration.m_localFrameA));
        b3Joint_SetLocalFrameB(joint, ToNativeJointFrame(configuration.m_localFrameB));
        b3Joint_SetCollideConnected(joint, configuration.m_collideConnected);
        b3Joint_SetUserData(joint, configuration.m_userData);
        b3Joint_SetConstraintTuning(joint, configuration.m_constraintHertz, configuration.m_constraintDampingRatio);
        b3Joint_SetForceThreshold(joint, configuration.m_forceThreshold);
        b3Joint_SetTorqueThreshold(joint, configuration.m_torqueThreshold);

        switch (configuration.m_kind)
        {
        case JointKind::Parallel:
            b3ParallelJoint_SetSpringHertz(joint, configuration.m_parallel.m_hertz);
            b3ParallelJoint_SetSpringDampingRatio(joint, configuration.m_parallel.m_dampingRatio);
            b3ParallelJoint_SetMaxTorque(joint, configuration.m_parallel.m_maxTorque);
            break;
        case JointKind::Distance:
            b3DistanceJoint_SetLength(joint, configuration.m_distance.m_length);
            b3DistanceJoint_EnableSpring(joint, configuration.m_distance.m_enableSpring);
            b3DistanceJoint_SetSpringForceRange(
                joint, configuration.m_distance.m_lowerSpringForce, configuration.m_distance.m_upperSpringForce);
            b3DistanceJoint_SetSpringHertz(joint, configuration.m_distance.m_hertz);
            b3DistanceJoint_SetSpringDampingRatio(joint, configuration.m_distance.m_dampingRatio);
            b3DistanceJoint_EnableLimit(joint, configuration.m_distance.m_enableLimit);
            b3DistanceJoint_SetLengthRange(joint, configuration.m_distance.m_minLength, configuration.m_distance.m_maxLength);
            b3DistanceJoint_EnableMotor(joint, configuration.m_distance.m_enableMotor);
            b3DistanceJoint_SetMotorSpeed(joint, configuration.m_distance.m_motorSpeed);
            b3DistanceJoint_SetMaxMotorForce(joint, configuration.m_distance.m_maxMotorForce);
            break;
        case JointKind::Filter:
            break;
        case JointKind::Motor:
            b3MotorJoint_SetLinearVelocity(joint, ToNative(configuration.m_motor.m_linearVelocity));
            b3MotorJoint_SetAngularVelocity(joint, ToNative(configuration.m_motor.m_angularVelocity));
            b3MotorJoint_SetMaxVelocityForce(joint, configuration.m_motor.m_maxVelocityForce);
            b3MotorJoint_SetMaxVelocityTorque(joint, configuration.m_motor.m_maxVelocityTorque);
            b3MotorJoint_SetLinearHertz(joint, configuration.m_motor.m_linearHertz);
            b3MotorJoint_SetLinearDampingRatio(joint, configuration.m_motor.m_linearDampingRatio);
            b3MotorJoint_SetMaxSpringForce(joint, configuration.m_motor.m_maxSpringForce);
            b3MotorJoint_SetAngularHertz(joint, configuration.m_motor.m_angularHertz);
            b3MotorJoint_SetAngularDampingRatio(joint, configuration.m_motor.m_angularDampingRatio);
            b3MotorJoint_SetMaxSpringTorque(joint, configuration.m_motor.m_maxSpringTorque);
            break;
        case JointKind::Prismatic:
            b3PrismaticJoint_EnableSpring(joint, configuration.m_prismatic.m_enableSpring);
            b3PrismaticJoint_SetSpringHertz(joint, configuration.m_prismatic.m_hertz);
            b3PrismaticJoint_SetSpringDampingRatio(joint, configuration.m_prismatic.m_dampingRatio);
            b3PrismaticJoint_SetTargetTranslation(joint, configuration.m_prismatic.m_targetTranslation);
            b3PrismaticJoint_EnableLimit(joint, configuration.m_prismatic.m_enableLimit);
            b3PrismaticJoint_SetLimits(joint, configuration.m_prismatic.m_lowerTranslation, configuration.m_prismatic.m_upperTranslation);
            b3PrismaticJoint_EnableMotor(joint, configuration.m_prismatic.m_enableMotor);
            b3PrismaticJoint_SetMotorSpeed(joint, configuration.m_prismatic.m_motorSpeed);
            b3PrismaticJoint_SetMaxMotorForce(joint, configuration.m_prismatic.m_maxMotorForce);
            break;
        case JointKind::Revolute:
            b3RevoluteJoint_EnableSpring(joint, configuration.m_revolute.m_enableSpring);
            b3RevoluteJoint_SetSpringHertz(joint, configuration.m_revolute.m_hertz);
            b3RevoluteJoint_SetSpringDampingRatio(joint, configuration.m_revolute.m_dampingRatio);
            b3RevoluteJoint_SetTargetAngle(joint, configuration.m_revolute.m_targetAngle);
            b3RevoluteJoint_EnableLimit(joint, configuration.m_revolute.m_enableLimit);
            b3RevoluteJoint_SetLimits(joint, configuration.m_revolute.m_lowerAngle, configuration.m_revolute.m_upperAngle);
            b3RevoluteJoint_EnableMotor(joint, configuration.m_revolute.m_enableMotor);
            b3RevoluteJoint_SetMotorSpeed(joint, configuration.m_revolute.m_motorSpeed);
            b3RevoluteJoint_SetMaxMotorTorque(joint, configuration.m_revolute.m_maxMotorTorque);
            break;
        case JointKind::Spherical:
            b3SphericalJoint_EnableConeLimit(joint, configuration.m_spherical.m_enableConeLimit);
            b3SphericalJoint_SetConeLimit(joint, configuration.m_spherical.m_coneAngle);
            b3SphericalJoint_EnableTwistLimit(joint, configuration.m_spherical.m_enableTwistLimit);
            b3SphericalJoint_SetTwistLimits(
                joint, configuration.m_spherical.m_lowerTwistAngle, configuration.m_spherical.m_upperTwistAngle);
            b3SphericalJoint_EnableSpring(joint, configuration.m_spherical.m_enableSpring);
            b3SphericalJoint_SetSpringHertz(joint, configuration.m_spherical.m_hertz);
            b3SphericalJoint_SetSpringDampingRatio(joint, configuration.m_spherical.m_dampingRatio);
            b3SphericalJoint_SetTargetRotation(joint, ToNative(configuration.m_spherical.m_targetRotation));
            b3SphericalJoint_EnableMotor(joint, configuration.m_spherical.m_enableMotor);
            b3SphericalJoint_SetMotorVelocity(joint, ToNative(configuration.m_spherical.m_motorVelocity));
            b3SphericalJoint_SetMaxMotorTorque(joint, configuration.m_spherical.m_maxMotorTorque);
            break;
        case JointKind::Weld:
            b3WeldJoint_SetLinearHertz(joint, configuration.m_weld.m_linearHertz);
            b3WeldJoint_SetLinearDampingRatio(joint, configuration.m_weld.m_linearDampingRatio);
            b3WeldJoint_SetAngularHertz(joint, configuration.m_weld.m_angularHertz);
            b3WeldJoint_SetAngularDampingRatio(joint, configuration.m_weld.m_angularDampingRatio);
            break;
        case JointKind::Wheel:
            b3WheelJoint_EnableSuspension(joint, configuration.m_wheel.m_enableSuspensionSpring);
            b3WheelJoint_SetSuspensionHertz(joint, configuration.m_wheel.m_suspensionHertz);
            b3WheelJoint_SetSuspensionDampingRatio(joint, configuration.m_wheel.m_suspensionDampingRatio);
            b3WheelJoint_EnableSuspensionLimit(joint, configuration.m_wheel.m_enableSuspensionLimit);
            b3WheelJoint_SetSuspensionLimits(
                joint, configuration.m_wheel.m_lowerSuspensionLimit, configuration.m_wheel.m_upperSuspensionLimit);
            b3WheelJoint_EnableSpinMotor(joint, configuration.m_wheel.m_enableSpinMotor);
            b3WheelJoint_SetSpinMotorSpeed(joint, configuration.m_wheel.m_spinSpeed);
            b3WheelJoint_SetMaxSpinTorque(joint, configuration.m_wheel.m_maxSpinTorque);
            b3WheelJoint_EnableSteering(joint, configuration.m_wheel.m_enableSteering);
            b3WheelJoint_SetSteeringHertz(joint, configuration.m_wheel.m_steeringHertz);
            b3WheelJoint_SetSteeringDampingRatio(joint, configuration.m_wheel.m_steeringDampingRatio);
            b3WheelJoint_SetMaxSteeringTorque(joint, configuration.m_wheel.m_maxSteeringTorque);
            b3WheelJoint_EnableSteeringLimit(joint, configuration.m_wheel.m_enableSteeringLimit);
            b3WheelJoint_SetSteeringLimits(joint, configuration.m_wheel.m_lowerSteeringLimit, configuration.m_wheel.m_upperSteeringLimit);
            b3WheelJoint_SetTargetSteeringAngle(joint, configuration.m_wheel.m_targetSteeringAngle);
            break;
        }

        return true;
    }

    void DestroyJoint(JointId jointId, bool wakeAttachedBodies)
    {
        if (IsValid(jointId))
        {
            b3DestroyJoint(b3LoadJointId(jointId.m_value), wakeAttachedBodies);
        }
    }

    void WakeJointBodies(JointId jointId)
    {
        if (IsValid(jointId))
        {
            b3Joint_WakeBodies(b3LoadJointId(jointId.m_value));
        }
    }

    bool IsValid(JointId jointId)
    {
        return jointId.IsValid() && b3Joint_IsValid(b3LoadJointId(jointId.m_value));
    }

    void* GetJointUserData(JointId jointId)
    {
        return IsValid(jointId) ? b3Joint_GetUserData(b3LoadJointId(jointId.m_value)) : nullptr;
    }

    AZ::Vector3 GetJointConstraintForce(JointId jointId)
    {
        return IsValid(jointId) ? FromNative(b3Joint_GetConstraintForce(b3LoadJointId(jointId.m_value))) : AZ::Vector3::CreateZero();
    }

    AZ::Vector3 GetJointConstraintTorque(JointId jointId)
    {
        return IsValid(jointId) ? FromNative(b3Joint_GetConstraintTorque(b3LoadJointId(jointId.m_value))) : AZ::Vector3::CreateZero();
    }

    float GetJointLinearSeparation(JointId jointId)
    {
        return IsValid(jointId) ? b3Joint_GetLinearSeparation(b3LoadJointId(jointId.m_value)) : 0.0f;
    }

    float GetJointAngularSeparation(JointId jointId)
    {
        return IsValid(jointId) ? b3Joint_GetAngularSeparation(b3LoadJointId(jointId.m_value)) : 0.0f;
    }

    JointState GetJointState(JointId jointId, JointKind kind)
    {
        if (!IsValid(jointId))
        {
            return AZStd::monostate{};
        }

        const b3JointId joint = b3LoadJointId(jointId.m_value);
        switch (kind)
        {
        case JointKind::Distance:
            if (b3Joint_GetType(joint) == b3_distanceJoint)
            {
                return DistanceJointState{ b3DistanceJoint_GetCurrentLength(joint), b3DistanceJoint_GetMotorForce(joint) };
            }
            break;
        case JointKind::Prismatic:
            if (b3Joint_GetType(joint) == b3_prismaticJoint)
            {
                return PrismaticJointState{ b3PrismaticJoint_GetTranslation(joint),
                                            b3PrismaticJoint_GetSpeed(joint),
                                            b3PrismaticJoint_GetMotorForce(joint) };
            }
            break;
        case JointKind::Revolute:
            if (b3Joint_GetType(joint) == b3_revoluteJoint)
            {
                return RevoluteJointState{ b3RevoluteJoint_GetAngle(joint), b3RevoluteJoint_GetMotorTorque(joint) };
            }
            break;
        case JointKind::Spherical:
            if (b3Joint_GetType(joint) == b3_sphericalJoint)
            {
                return SphericalJointState{ FromNative(b3SphericalJoint_GetMotorTorque(joint)),
                                            b3SphericalJoint_GetConeAngle(joint),
                                            b3SphericalJoint_GetTwistAngle(joint) };
            }
            break;
        case JointKind::Wheel:
            if (b3Joint_GetType(joint) == b3_wheelJoint)
            {
                return WheelJointState{ b3WheelJoint_GetSpinSpeed(joint),
                                        b3WheelJoint_GetSpinTorque(joint),
                                        b3WheelJoint_GetSteeringAngle(joint),
                                        b3WheelJoint_GetSteeringTorque(joint) };
            }
            break;
        case JointKind::Parallel:
        case JointKind::Filter:
        case JointKind::Motor:
        case JointKind::Weld:
            break;
        }
        return AZStd::monostate{};
    }

    ShapeId CreateSphereShape(BodyId bodyId, const NativeShapeConfiguration& configuration, const AZ::Vector3& center, float radius)
    {
        if (!IsValid(bodyId) || !center.IsFinite() || !AZ::IsFiniteFloat(radius) || radius <= 0.0f)
        {
            return {};
        }
        NativeShapeDefinition definition = ToNative(configuration);
        const b3Sphere sphere = { ToNative(center), radius };
        return { b3StoreShapeId(b3CreateSphereShape(b3LoadBodyId(bodyId.m_value), &definition.m_value, &sphere)) };
    }

    ShapeId CreateCapsuleShape(
        BodyId bodyId, const NativeShapeConfiguration& configuration, const AZ::Vector3& center1, const AZ::Vector3& center2, float radius)
    {
        if (!IsValid(bodyId) || !center1.IsFinite() || !center2.IsFinite() || !AZ::IsFiniteFloat(radius) || radius <= 0.0f)
        {
            return {};
        }
        NativeShapeDefinition definition = ToNative(configuration);
        const b3Capsule capsule = { ToNative(center1), ToNative(center2), radius };
        return { b3StoreShapeId(b3CreateCapsuleShape(b3LoadBodyId(bodyId.m_value), &definition.m_value, &capsule)) };
    }

    bool SetSphereShape(ShapeId shapeId, const AZ::Vector3& center, float radius)
    {
        if (!IsValid(shapeId) || !center.IsFinite() || !AZ::IsFiniteFloat(radius) || radius <= 0.0f)
        {
            return false;
        }
        const b3Sphere sphere = { ToNative(center), radius };
        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        b3Shape_SetSphere(nativeShapeId, &sphere);
        return b3Shape_GetType(nativeShapeId) == b3_sphereShape;
    }

    bool SetCapsuleShape(ShapeId shapeId, const AZ::Vector3& center1, const AZ::Vector3& center2, float radius)
    {
        if (!IsValid(shapeId) || !center1.IsFinite() || !center2.IsFinite() || !AZ::IsFiniteFloat(radius) || radius <= 0.0f)
        {
            return false;
        }
        const b3Capsule capsule = { ToNative(center1), ToNative(center2), radius };
        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        b3Shape_SetCapsule(nativeShapeId, &capsule);
        return b3Shape_GetType(nativeShapeId) == b3_capsuleShape;
    }

    ShapeId CreateBoxShape(
        BodyId bodyId, const NativeShapeConfiguration& configuration, const AZ::Vector3& halfExtents, const AZ::Transform& localTransform)
    {
        const AZStd::unique_ptr<HullGeometry> geometry = CreateBoxGeometry(halfExtents, GeometryTransform{ localTransform });
        return geometry != nullptr ? CreateHullShape(bodyId, configuration, *geometry) : ShapeId{};
    }

    ShapeId CreateHullShape(
        BodyId bodyId,
        const NativeShapeConfiguration& configuration,
        AZStd::span<const AZ::Vector3> points,
        const AZ::Vector3& scale,
        const AZ::Transform& localTransform)
    {
        const AZStd::unique_ptr<HullGeometry> geometry = CreateHullGeometry(points, GeometryTransform{ localTransform, scale });
        return geometry != nullptr ? CreateHullShape(bodyId, configuration, *geometry) : ShapeId{};
    }

    AZStd::unique_ptr<HullGeometry> CreateBoxGeometry(const AZ::Vector3& halfExtents, const GeometryTransform& transform)
    {
        InstallNativeCallbacks();
        if (!halfExtents.IsFinite() || halfExtents.GetMinElement() <= 0.0f || !IsRigidTransform(transform.m_localTransform) ||
            !IsValidGeometryScale(transform.m_scale) || !IsValidGeometryScale(transform.m_postScale))
        {
            return {};
        }

        const b3Vec3 nativeHalfExtents = ToNativeScale(halfExtents);
        const b3BoxHull box = b3MakeBoxHull(nativeHalfExtents.x, nativeHalfExtents.y, nativeHalfExtents.z);
        const b3WorldTransform worldTransform = ToNative(transform.m_localTransform);
        const b3Transform localTransform = { ToNative(transform.m_localTransform.GetTranslation()), worldTransform.q };
        if (IsIdentityScale(transform.m_scale) && IsIdentityScale(transform.m_postScale))
        {
            const b3BoxHull transformedBox =
                b3MakeTransformedBoxHull(nativeHalfExtents.x, nativeHalfExtents.y, nativeHalfExtents.z, localTransform);
            b3HullData* hull = b3CloneHull(&transformedBox.base);
            return hull != nullptr ? AZStd::unique_ptr<HullGeometry>(aznew HullGeometry(hull)) : nullptr;
        }
        b3HullData* localHull = IsIdentityPose(transform.m_localTransform) && IsIdentityScale(transform.m_scale)
            ? b3CloneHull(&box.base)
            : b3CloneAndTransformHull(&box.base, localTransform, ToNativeScale(transform.m_scale));
        if (localHull == nullptr)
        {
            return {};
        }
        b3HullData* hull = localHull;
        if (!IsIdentityScale(transform.m_postScale))
        {
            hull = b3CloneAndTransformHull(localHull, b3Transform_identity, ToNativeScale(transform.m_postScale));
            b3DestroyHull(localHull);
        }
        return hull != nullptr ? AZStd::unique_ptr<HullGeometry>(aznew HullGeometry(hull)) : nullptr;
    }

    AZStd::unique_ptr<HullGeometry> CreateCylinderGeometry(
        float height, float radius, AZ::u8 subdivisionCount, const GeometryTransform& transform)
    {
        InstallNativeCallbacks();
        if (!AZ::IsFiniteFloat(height) || height <= 0.0f || !AZ::IsFiniteFloat(radius) || radius <= 0.0f || subdivisionCount < 3 ||
            subdivisionCount > 32 || !IsRigidTransform(transform.m_localTransform) || !IsValidGeometryScale(transform.m_scale) ||
            !IsValidGeometryScale(transform.m_postScale))
        {
            return {};
        }

        b3HullData* cylinder = b3CreateCylinder(height, radius, -0.5f * height, subdivisionCount);
        if (cylinder == nullptr)
        {
            return {};
        }
        const b3WorldTransform worldTransform = ToNative(transform.m_localTransform);
        const b3Transform localTransform = { ToNative(transform.m_localTransform.GetTranslation()), worldTransform.q };
        b3HullData* localHull = cylinder;
        if (!IsIdentityPose(transform.m_localTransform) || !IsIdentityScale(transform.m_scale))
        {
            localHull = b3CloneAndTransformHull(cylinder, localTransform, ToNativeScale(transform.m_scale));
            b3DestroyHull(cylinder);
        }
        if (localHull == nullptr)
        {
            return {};
        }
        b3HullData* hull = localHull;
        if (!IsIdentityScale(transform.m_postScale))
        {
            hull = b3CloneAndTransformHull(localHull, b3Transform_identity, ToNativeScale(transform.m_postScale));
            b3DestroyHull(localHull);
        }
        return hull != nullptr ? AZStd::unique_ptr<HullGeometry>(aznew HullGeometry(hull)) : nullptr;
    }

    AZStd::unique_ptr<HullGeometry> CreateHullGeometry(AZStd::span<const AZ::Vector3> points, const GeometryTransform& transform)
    {
        InstallNativeCallbacks();
        if (points.size() < 4 || points.size() > 254 || !IsRigidTransform(transform.m_localTransform) ||
            !IsValidGeometryScale(transform.m_scale) || !IsValidGeometryScale(transform.m_postScale))
        {
            return {};
        }

        AZStd::vector<b3Vec3> nativePoints;
        nativePoints.reserve(points.size());
        for (const AZ::Vector3& point : points)
        {
            if (!point.IsFinite())
            {
                return {};
            }
            nativePoints.push_back(ToNative(point));
        }

        b3HullData* hull = b3CreateHull(nativePoints.data(), aznumeric_cast<int>(nativePoints.size()), 254);
        if (hull == nullptr)
        {
            return {};
        }

        const b3WorldTransform worldTransform = ToNative(transform.m_localTransform);
        const b3Transform localTransform = { ToNative(transform.m_localTransform.GetTranslation()), worldTransform.q };
        b3HullData* localHull = hull;
        if (!IsIdentityPose(transform.m_localTransform) || !IsIdentityScale(transform.m_scale))
        {
            localHull = b3CloneAndTransformHull(hull, localTransform, ToNativeScale(transform.m_scale));
            b3DestroyHull(hull);
        }
        if (localHull == nullptr)
        {
            return {};
        }
        b3HullData* transformedHull = localHull;
        if (!IsIdentityScale(transform.m_postScale))
        {
            transformedHull = b3CloneAndTransformHull(localHull, b3Transform_identity, ToNativeScale(transform.m_postScale));
            b3DestroyHull(localHull);
        }
        return transformedHull != nullptr ? AZStd::unique_ptr<HullGeometry>(aznew HullGeometry(transformedHull)) : nullptr;
    }

    ShapeId CreateHullShape(BodyId bodyId, const NativeShapeConfiguration& configuration, const HullGeometry& geometry)
    {
        const b3HullData* hull = GeometryCastAccess::Get(geometry);
        if (!IsValid(bodyId) || hull == nullptr)
        {
            return {};
        }

        NativeShapeDefinition definition = ToNative(configuration);
        return { b3StoreShapeId(b3CreateHullShape(b3LoadBodyId(bodyId.m_value), &definition.m_value, hull)) };
    }

    bool SetHullShape(ShapeId shapeId, const HullGeometry& geometry)
    {
        const b3HullData* hull = GeometryCastAccess::Get(geometry);
        if (!IsValid(shapeId) || hull == nullptr)
        {
            return false;
        }
        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        b3Shape_SetHull(nativeShapeId, hull);
        return b3Shape_GetType(nativeShapeId) == b3_hullShape;
    }

    AZStd::unique_ptr<MeshGeometry> CreateMeshGeometry(
        AZStd::span<const AZ::Vector3> vertices,
        AZStd::span<const AZ::u32> indices,
        AZStd::span<const AZ::u8> materialIndices,
        float weldTolerance,
        bool weldVertices,
        bool useMedianSplit,
        bool identifyEdges)
    {
        InstallNativeCallbacks();
        if (vertices.size() < 3 || indices.empty() || indices.size() % 3 != 0 || !AZ::IsFiniteFloat(weldTolerance) ||
            weldTolerance < 0.0f || (!materialIndices.empty() && materialIndices.size() != indices.size() / 3))
        {
            return {};
        }

        AZStd::vector<b3Vec3> nativeVertices;
        nativeVertices.reserve(vertices.size());
        for (const AZ::Vector3& vertex : vertices)
        {
            if (!vertex.IsFinite())
            {
                return {};
            }
            nativeVertices.push_back(ToNative(vertex));
        }

        AZStd::vector<int32_t> nativeIndices;
        nativeIndices.reserve(indices.size());
        for (AZ::u32 index : indices)
        {
            if (index >= vertices.size() || index > aznumeric_cast<AZ::u32>((AZStd::numeric_limits<int32_t>::max)()))
            {
                return {};
            }
            nativeIndices.push_back(aznumeric_cast<int32_t>(index));
        }

        AZStd::vector<AZ::u8> nativeMaterialIndices(materialIndices.begin(), materialIndices.end());

        b3MeshDef definition{};
        definition.vertices = nativeVertices.data();
        definition.indices = nativeIndices.data();
        definition.materialIndices = nativeMaterialIndices.empty() ? nullptr : nativeMaterialIndices.data();
        definition.vertexCount = aznumeric_cast<int>(nativeVertices.size());
        definition.triangleCount = aznumeric_cast<int>(nativeIndices.size() / 3);
        definition.weldTolerance = weldTolerance;
        definition.weldVertices = weldVertices;
        definition.useMedianSplit = useMedianSplit;
        definition.identifyEdges = identifyEdges;
        AZStd::vector<int> degenerateTriangles(definition.triangleCount + 1);
        b3MeshData* mesh = b3CreateMesh(&definition, degenerateTriangles.data(), aznumeric_cast<int>(degenerateTriangles.size()));
        return mesh != nullptr ? AZStd::unique_ptr<MeshGeometry>(aznew MeshGeometry(mesh)) : nullptr;
    }

    ShapeId CreateMeshShape(BodyId bodyId, const NativeShapeConfiguration& configuration, const MeshGeometry& geometry)
    {
        if (!IsValid(bodyId) || geometry.m_data == nullptr || GetBodyType(bodyId) != NativeBodyType::Static)
        {
            return {};
        }

        NativeShapeDefinition definition = ToNative(configuration);
        return { b3StoreShapeId(b3CreateMeshShape(
            b3LoadBodyId(bodyId.m_value),
            &definition.m_value,
            static_cast<const b3MeshData*>(geometry.m_data),
            b3Vec3{ 1.0f, 1.0f, 1.0f })) };
    }

    bool SetMeshShape(ShapeId shapeId, const MeshGeometry& geometry)
    {
        const b3MeshData* mesh = GeometryCastAccess::Get(geometry);
        if (!IsValid(shapeId) || mesh == nullptr ||
            GetBodyType({ b3StoreBodyId(b3Shape_GetBody(b3LoadShapeId(shapeId.m_value))) }) != NativeBodyType::Static)
        {
            return false;
        }
        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        b3Shape_SetMesh(nativeShapeId, mesh, b3Vec3{ 1.0f, 1.0f, 1.0f });
        return b3Shape_GetType(nativeShapeId) == b3_meshShape;
    }

    AZStd::unique_ptr<HeightFieldGeometry> CreateHeightFieldGeometry(
        AZStd::span<const float> heights,
        AZStd::span<const AZ::u8> materialIndices,
        size_t columnCount,
        size_t rowCount,
        const AZ::Vector2& gridResolution,
        const AZ::Vector3& scale,
        float minimumHeight,
        float maximumHeight,
        bool clockwise)
    {
        InstallNativeCallbacks();
        constexpr size_t maximumByteCount = aznumeric_cast<size_t>((AZStd::numeric_limits<int>::max)());
        if (columnCount < 2 || rowCount < 2 || columnCount > maximumByteCount || rowCount > maximumByteCount ||
            !gridResolution.IsFinite() || gridResolution.GetX() <= 0.0f || gridResolution.GetY() <= 0.0f || !scale.IsFinite() ||
            scale.GetMinElement() <= 0.0f || columnCount > maximumByteCount / rowCount ||
            columnCount - 1 > maximumByteCount / (rowCount - 1))
        {
            return {};
        }

        const size_t heightCount = columnCount * rowCount;
        const size_t cellCount = (columnCount - 1) * (rowCount - 1);
        if (heightCount != heights.size() || cellCount != materialIndices.size() ||
            heightCount > (maximumByteCount - 7) / sizeof(AZ::u16) || cellCount > maximumByteCount - 7 ||
            cellCount > (maximumByteCount - 7) / 2)
        {
            return {};
        }

        const auto alignToEightBytes = [](size_t byteCount)
        {
            return (byteCount + 7) & ~size_t{ 7 };
        };
        size_t nativeByteCount = alignToEightBytes(sizeof(b3HeightFieldData));
        const size_t compressedHeightBytes = alignToEightBytes(heightCount * sizeof(AZ::u16));
        const size_t materialBytes = alignToEightBytes(cellCount);
        const size_t flagBytes = alignToEightBytes(2 * cellCount);
        if (compressedHeightBytes > maximumByteCount - nativeByteCount)
        {
            return {};
        }
        nativeByteCount += compressedHeightBytes;
        if (materialBytes > maximumByteCount - nativeByteCount)
        {
            return {};
        }
        nativeByteCount += materialBytes;
        if (flagBytes > maximumByteCount - nativeByteCount)
        {
            return {};
        }

        AZStd::vector<float> nativeHeights(heightCount);
        float sampleMinimumHeight = (AZStd::numeric_limits<float>::max)();
        float sampleMaximumHeight = (AZStd::numeric_limits<float>::lowest)();
        for (size_t sampleIndex = 0; sampleIndex < heightCount; ++sampleIndex)
        {
            const float height = heights[sampleIndex];
            if (!(std::isfinite)(height))
            {
                return {};
            }
            sampleMinimumHeight = AZStd::min(sampleMinimumHeight, height);
            sampleMaximumHeight = AZStd::max(sampleMaximumHeight, height);
            nativeHeights[sampleIndex] = -height;
        }

        const bool useSampleBounds =
            minimumHeight == (AZStd::numeric_limits<float>::lowest)() && maximumHeight == (AZStd::numeric_limits<float>::max)();
        if (useSampleBounds)
        {
            minimumHeight = sampleMinimumHeight;
            maximumHeight = sampleMaximumHeight;
        }
        if (!(std::isfinite)(minimumHeight) || !(std::isfinite)(maximumHeight) || minimumHeight > maximumHeight ||
            !(std::isfinite)(maximumHeight - minimumHeight))
        {
            return {};
        }

        b3HeightFieldDef definition{};
        definition.heights = nativeHeights.data();
        definition.materialIndices = const_cast<AZ::u8*>(materialIndices.data());
        definition.scale = { gridResolution.GetX() * scale.GetX(), scale.GetZ(), gridResolution.GetY() * scale.GetY() };
        definition.countX = aznumeric_cast<int>(columnCount);
        definition.countZ = aznumeric_cast<int>(rowCount);
        definition.globalMinimumHeight = -maximumHeight;
        definition.globalMaximumHeight = -minimumHeight;
        definition.clockwiseWinding = clockwise;
        b3HeightFieldData* heightField = b3CreateHeightField(&definition);
        return heightField != nullptr ? AZStd::unique_ptr<HeightFieldGeometry>(aznew HeightFieldGeometry(heightField)) : nullptr;
    }

    ShapeId CreateHeightFieldShape(BodyId bodyId, const NativeShapeConfiguration& configuration, const HeightFieldGeometry& geometry)
    {
        if (!IsValid(bodyId) || geometry.m_data == nullptr || GetBodyType(bodyId) != NativeBodyType::Static)
        {
            return {};
        }

        NativeShapeDefinition definition = ToNative(configuration);
        return { b3StoreShapeId(b3CreateHeightFieldShape(
            b3LoadBodyId(bodyId.m_value), &definition.m_value, static_cast<const b3HeightFieldData*>(geometry.m_data))) };
    }

    AZStd::unique_ptr<CompoundGeometry> CreateCompoundGeometry(const CompoundGeometry::Definition& definition)
    {
        InstallNativeCallbacks();
        const size_t childCount =
            definition.m_spheres.size() + definition.m_capsules.size() + definition.m_hulls.size() + definition.m_meshes.size();
        if (childCount == 0 || childCount >= B3_MAX_CHILD_SHAPES)
        {
            return {};
        }

        AZStd::vector<b3CompoundSphereDef> spheres;
        spheres.reserve(definition.m_spheres.size());
        for (const CompoundGeometry::Sphere& sphere : definition.m_spheres)
        {
            if (!sphere.m_center.IsFinite() || !AZ::IsFiniteFloat(sphere.m_radius) || sphere.m_radius <= 0.0f)
            {
                return {};
            }
            spheres.push_back({ { ToNative(sphere.m_center), sphere.m_radius }, ToNative(sphere.m_material) });
        }

        AZStd::vector<b3CompoundCapsuleDef> capsules;
        capsules.reserve(definition.m_capsules.size());
        for (const CompoundGeometry::Capsule& capsule : definition.m_capsules)
        {
            if (!capsule.m_center1.IsFinite() || !capsule.m_center2.IsFinite() || !AZ::IsFiniteFloat(capsule.m_radius) ||
                capsule.m_radius <= 0.0f)
            {
                return {};
            }
            capsules.push_back(
                { { ToNative(capsule.m_center1), ToNative(capsule.m_center2), capsule.m_radius }, ToNative(capsule.m_material) });
        }

        AZStd::vector<b3CompoundHullDef> hulls;
        hulls.reserve(definition.m_hulls.size());
        for (const CompoundGeometry::Hull& hull : definition.m_hulls)
        {
            if (hull.m_geometry == nullptr || GeometryCastAccess::Get(*hull.m_geometry) == nullptr ||
                !hull.m_transform.GetTranslation().IsFinite() || !hull.m_transform.GetRotation().IsFinite())
            {
                return {};
            }
            const b3WorldTransform transform = ToNative(hull.m_transform);
            hulls.push_back(
                { GeometryCastAccess::Get(*hull.m_geometry),
                  { ToNative(hull.m_transform.GetTranslation()), transform.q },
                  ToNative(hull.m_material) });
        }

        size_t meshMaterialCount = 0;
        for (const CompoundGeometry::Mesh& mesh : definition.m_meshes)
        {
            const b3MeshData* geometry = mesh.m_geometry != nullptr ? GeometryCastAccess::Get(*mesh.m_geometry) : nullptr;
            if (geometry == nullptr || mesh.m_materials.empty() || mesh.m_materials.size() > B3_MAX_COMPOUND_MESH_MATERIALS ||
                mesh.m_materials.size() != aznumeric_cast<size_t>(geometry->materialCount) ||
                !mesh.m_transform.GetTranslation().IsFinite() || !mesh.m_transform.GetRotation().IsFinite() || !mesh.m_scale.IsFinite() ||
                mesh.m_scale.GetX() == 0.0f || mesh.m_scale.GetY() == 0.0f || mesh.m_scale.GetZ() == 0.0f)
            {
                return {};
            }
            meshMaterialCount += mesh.m_materials.size();
        }

        AZStd::vector<b3SurfaceMaterial> meshMaterials;
        meshMaterials.reserve(meshMaterialCount);
        AZStd::vector<b3CompoundMeshDef> meshes;
        meshes.reserve(definition.m_meshes.size());
        for (const CompoundGeometry::Mesh& mesh : definition.m_meshes)
        {
            const size_t materialOffset = meshMaterials.size();
            for (const SurfaceMaterial& material : mesh.m_materials)
            {
                meshMaterials.push_back(ToNative(material));
            }
            const b3WorldTransform transform = ToNative(mesh.m_transform);
            meshes.push_back(
                { GeometryCastAccess::Get(*mesh.m_geometry),
                  { ToNative(mesh.m_transform.GetTranslation()), transform.q },
                  ToNativeScale(mesh.m_scale),
                  meshMaterials.data() + materialOffset,
                  aznumeric_cast<int>(mesh.m_materials.size()) });
        }

        b3CompoundDef nativeDefinition{};
        nativeDefinition.spheres = spheres.data();
        nativeDefinition.sphereCount = aznumeric_cast<int>(spheres.size());
        nativeDefinition.capsules = capsules.data();
        nativeDefinition.capsuleCount = aznumeric_cast<int>(capsules.size());
        nativeDefinition.hulls = hulls.data();
        nativeDefinition.hullCount = aznumeric_cast<int>(hulls.size());
        nativeDefinition.meshes = meshes.data();
        nativeDefinition.meshCount = aznumeric_cast<int>(meshes.size());
        b3CompoundData* compound = b3CreateCompound(&nativeDefinition);
        if (compound == nullptr)
        {
            return {};
        }

        AZStd::vector<AZ::u8> materialSources;
        materialSources.reserve(aznumeric_cast<size_t>(compound->materialCount));
        const b3SurfaceMaterial* nativeMaterials = b3GetCompoundMaterials(compound);
        for (int materialIndex = 0; materialIndex < compound->materialCount; ++materialIndex)
        {
            size_t sourceIndex = 0;
            while (sourceIndex < definition.m_sourceMaterials.size() &&
                   definition.m_sourceMaterials[sourceIndex].m_userId != nativeMaterials[materialIndex].userMaterialId)
            {
                ++sourceIndex;
            }
            if (!definition.m_sourceMaterials.empty() && sourceIndex == definition.m_sourceMaterials.size())
            {
                b3DestroyCompound(compound);
                return {};
            }
            if (sourceIndex > AZStd::numeric_limits<AZ::u8>::max())
            {
                b3DestroyCompound(compound);
                return {};
            }
            materialSources.push_back(aznumeric_cast<AZ::u8>(sourceIndex));
        }
        return AZStd::unique_ptr<CompoundGeometry>(aznew CompoundGeometry(compound, AZStd::move(materialSources)));
    }

    ShapeId CreateCompoundShape(BodyId bodyId, const NativeShapeConfiguration& configuration, const CompoundGeometry& geometry)
    {
        if (!IsValid(bodyId) || geometry.m_data == nullptr || GetBodyType(bodyId) != NativeBodyType::Static || configuration.m_isSensor)
        {
            return {};
        }

        NativeShapeDefinition definition = ToNative(configuration);
        return { b3StoreShapeId(b3CreateCompoundShape(
            b3LoadBodyId(bodyId.m_value), &definition.m_value, static_cast<const b3CompoundData*>(geometry.m_data))) };
    }

    NativeGeometry CookGeometry(
        const ShapeGeometry& geometry, const GeometryTransform& transform, AZStd::span<const SurfaceMaterial> materials)
    {
        if (!IsRigidTransform(transform.m_localTransform) || !IsValidGeometryScale(transform.m_scale) ||
            !IsValidGeometryScale(transform.m_postScale))
        {
            return AZStd::monostate{};
        }

        return AZStd::visit(
            [&transform, materials](const auto& shapeGeometry) -> NativeGeometry
            {
                using Geometry = AZStd::remove_cvref_t<decltype(shapeGeometry)>;
                if constexpr (AZStd::is_same_v<Geometry, SphereShapeConfiguration>)
                {
                    if (!AZ::IsFiniteFloat(shapeGeometry.m_radius) || shapeGeometry.m_radius <= 0.0f)
                    {
                        return AZStd::monostate{};
                    }
                    float scale = 0.0f;
                    if (!GetSimilarityScale(transform, scale))
                    {
                        return AZStd::monostate{};
                    }
                    return NativeSphereGeometry{ TransformGeometryPoint(transform, AZ::Vector3::CreateZero()),
                                                 shapeGeometry.m_radius * scale };
                }
                else if constexpr (AZStd::is_same_v<Geometry, CapsuleShapeConfiguration>)
                {
                    if (!AZ::IsFiniteFloat(shapeGeometry.m_height) || shapeGeometry.m_height <= 0.0f ||
                        !AZ::IsFiniteFloat(shapeGeometry.m_radius) || shapeGeometry.m_radius <= 0.0f)
                    {
                        return AZStd::monostate{};
                    }
                    float scale = 0.0f;
                    if (!GetSimilarityScale(transform, scale))
                    {
                        return AZStd::monostate{};
                    }
                    const float halfSegment = AZStd::max(0.5f * shapeGeometry.m_height - shapeGeometry.m_radius, 0.0f);
                    return NativeCapsuleGeometry{ TransformGeometryPoint(transform, -AZ::Vector3::CreateAxisZ(halfSegment)),
                                                  TransformGeometryPoint(transform, AZ::Vector3::CreateAxisZ(halfSegment)),
                                                  shapeGeometry.m_radius * scale };
                }
                else if constexpr (AZStd::is_same_v<Geometry, BoxShapeConfiguration>)
                {
                    return AZStd::shared_ptr<const HullGeometry>(CreateBoxGeometry(shapeGeometry.m_halfExtents, transform));
                }
                else if constexpr (AZStd::is_same_v<Geometry, CylinderShapeConfiguration>)
                {
                    if (shapeGeometry.m_sideCount > AZStd::numeric_limits<AZ::u8>::max())
                    {
                        return AZStd::monostate{};
                    }
                    return AZStd::shared_ptr<const HullGeometry>(CreateCylinderGeometry(
                        shapeGeometry.m_height, shapeGeometry.m_radius, aznumeric_cast<AZ::u8>(shapeGeometry.m_sideCount), transform));
                }
                else if constexpr (AZStd::is_same_v<Geometry, ConvexHullShapeConfiguration>)
                {
                    return AZStd::shared_ptr<const HullGeometry>(CreateHullGeometry(shapeGeometry.m_vertices, transform));
                }
                else if constexpr (AZStd::is_same_v<Geometry, TriangleMeshShapeConfiguration>)
                {
                    if (shapeGeometry.m_indices.size() % 3 != 0)
                    {
                        return AZStd::monostate{};
                    }
                    AZStd::vector<AZ::Vector3> transformedVertices;
                    transformedVertices.reserve(shapeGeometry.m_vertices.size());
                    for (const AZ::Vector3& vertex : shapeGeometry.m_vertices)
                    {
                        transformedVertices.push_back(TransformGeometryPoint(transform, vertex));
                    }
                    AZStd::vector<AZ::u32> transformedIndices = shapeGeometry.m_indices;
                    const AZ::Vector3 basisX = TransformGeometryVector(transform, AZ::Vector3::CreateAxisX());
                    const AZ::Vector3 basisY = TransformGeometryVector(transform, AZ::Vector3::CreateAxisY());
                    const AZ::Vector3 basisZ = TransformGeometryVector(transform, AZ::Vector3::CreateAxisZ());
                    if (basisX.Cross(basisY).Dot(basisZ) < 0.0f)
                    {
                        for (size_t index = 0; index < transformedIndices.size(); index += 3)
                        {
                            AZStd::swap(transformedIndices[index + 1], transformedIndices[index + 2]);
                        }
                    }
                    return AZStd::shared_ptr<const MeshGeometry>(CreateMeshGeometry(
                        transformedVertices,
                        transformedIndices,
                        shapeGeometry.m_materialIndices,
                        shapeGeometry.m_weldTolerance,
                        shapeGeometry.m_weldVertices,
                        shapeGeometry.m_useMedianSplit,
                        shapeGeometry.m_identifyEdges));
                }
                else if constexpr (AZStd::is_same_v<Geometry, HeightfieldShapeConfiguration>)
                {
                    if (!transform.m_localTransform.GetTranslation().IsZero() || !transform.m_localTransform.GetRotation().IsIdentity() ||
                        !IsValidGeometryScale(transform.m_scale, false) || !IsValidGeometryScale(transform.m_postScale, false))
                    {
                        return AZStd::monostate{};
                    }

                    const auto [minimumHeight, maximumHeight] =
                        AZStd::minmax_element(shapeGeometry.m_samples.begin(), shapeGeometry.m_samples.end());
                    if (minimumHeight == shapeGeometry.m_samples.end())
                    {
                        return AZStd::monostate{};
                    }

                    AZStd::vector<AZ::u8> defaultMaterialIndices;
                    AZStd::span<const AZ::u8> materialIndices = shapeGeometry.m_materialIndices;
                    if (materialIndices.empty())
                    {
                        if (shapeGeometry.m_columnCount < 2 || shapeGeometry.m_rowCount < 2 ||
                            shapeGeometry.m_columnCount - 1 > (AZStd::numeric_limits<size_t>::max)() / (shapeGeometry.m_rowCount - 1))
                        {
                            return AZStd::monostate{};
                        }
                        defaultMaterialIndices.resize(
                            aznumeric_cast<size_t>(shapeGeometry.m_columnCount - 1) * aznumeric_cast<size_t>(shapeGeometry.m_rowCount - 1));
                        materialIndices = defaultMaterialIndices;
                    }

                    const float quantizationMinimum = shapeGeometry.m_useSharedHeightRange ? shapeGeometry.m_minimumHeight : *minimumHeight;
                    const float quantizationMaximum = shapeGeometry.m_useSharedHeightRange ? shapeGeometry.m_maximumHeight : *maximumHeight;
                    if (!AZ::IsFiniteFloat(quantizationMinimum) || !AZ::IsFiniteFloat(quantizationMaximum) ||
                        quantizationMinimum > *minimumHeight || quantizationMaximum < *maximumHeight ||
                        quantizationMinimum > quantizationMaximum)
                    {
                        return AZStd::monostate{};
                    }

                    return AZStd::shared_ptr<const HeightFieldGeometry>(CreateHeightFieldGeometry(
                        shapeGeometry.m_samples,
                        materialIndices,
                        shapeGeometry.m_columnCount,
                        shapeGeometry.m_rowCount,
                        AZ::Vector2(shapeGeometry.m_scale.GetX(), shapeGeometry.m_scale.GetY()),
                        AZ::Vector3(1.0f, 1.0f, shapeGeometry.m_scale.GetZ()) * transform.m_scale * transform.m_postScale,
                        quantizationMinimum,
                        quantizationMaximum,
                        shapeGeometry.m_clockwise));
                }
                else if constexpr (AZStd::is_same_v<Geometry, CompoundShapeConfiguration>)
                {
                    AZStd::vector<CompoundGeometry::Sphere> spheres;
                    AZStd::vector<CompoundGeometry::Capsule> capsules;
                    AZStd::vector<CompoundGeometry::Hull> hulls;
                    AZStd::vector<CompoundGeometry::Mesh> meshes;
                    AZStd::vector<AZStd::unique_ptr<HullGeometry>> hullGeometries;
                    AZStd::vector<AZStd::unique_ptr<MeshGeometry>> meshGeometries;
                    AZStd::vector<AZStd::vector<SurfaceMaterial>> meshMaterials;
                    spheres.reserve(shapeGeometry.m_children.size());
                    capsules.reserve(shapeGeometry.m_children.size());
                    hulls.reserve(shapeGeometry.m_children.size());
                    meshes.reserve(shapeGeometry.m_children.size());
                    hullGeometries.reserve(shapeGeometry.m_children.size());
                    meshGeometries.reserve(shapeGeometry.m_children.size());
                    meshMaterials.reserve(shapeGeometry.m_children.size());

                    for (const CompoundChildShapeConfiguration& child : shapeGeometry.m_children)
                    {
                        if (!IsRigidTransform(child.m_localTransform) || !IsValidGeometryScale(child.m_scale) ||
                            (!materials.empty() && child.m_materialIndex >= materials.size()))
                        {
                            return AZStd::monostate{};
                        }
                        const SurfaceMaterial material = !materials.empty() ? materials[child.m_materialIndex] : SurfaceMaterial{};
                        const auto transformPoint = [&transform, &child](const AZ::Vector3& point)
                        {
                            const AZ::Vector3 childPoint = child.m_localTransform.TransformPoint(child.m_scale * point);
                            return transform.m_postScale * transform.m_localTransform.TransformPoint(transform.m_scale * childPoint);
                        };
                        const AZ::Vector3 center = transformPoint(AZ::Vector3::CreateZero());
                        const AZ::Vector3 basisX = transformPoint(AZ::Vector3::CreateAxisX()) - center;
                        const AZ::Vector3 basisY = transformPoint(AZ::Vector3::CreateAxisY()) - center;
                        const AZ::Vector3 basisZ = transformPoint(AZ::Vector3::CreateAxisZ()) - center;
                        const float basisLengthSq = basisX.GetLengthSq();
                        const float similarityTolerance = AZStd::max(1.0f, basisLengthSq) * 1.0e-4f;
                        const bool isSimilarity = AZ::IsFiniteFloat(basisLengthSq) &&
                            basisLengthSq >= MinimumGeometryScale * MinimumGeometryScale &&
                            AZ::IsClose(basisY.GetLengthSq(), basisLengthSq, similarityTolerance) &&
                            AZ::IsClose(basisZ.GetLengthSq(), basisLengthSq, similarityTolerance) &&
                            AZ::IsClose(basisX.Dot(basisY), 0.0f, similarityTolerance) &&
                            AZ::IsClose(basisX.Dot(basisZ), 0.0f, similarityTolerance) &&
                            AZ::IsClose(basisY.Dot(basisZ), 0.0f, similarityTolerance);
                        const bool created = AZStd::visit(
                            [&](const auto& childGeometry)
                            {
                                using ChildGeometry = AZStd::remove_cvref_t<decltype(childGeometry)>;
                                if constexpr (AZStd::is_same_v<ChildGeometry, SphereShapeConfiguration>)
                                {
                                    if (!isSimilarity || !AZ::IsFiniteFloat(childGeometry.m_radius) || childGeometry.m_radius <= 0.0f)
                                    {
                                        return false;
                                    }
                                    spheres.push_back({ center, material, childGeometry.m_radius * std::sqrt(basisLengthSq) });
                                    return true;
                                }
                                else if constexpr (AZStd::is_same_v<ChildGeometry, CapsuleShapeConfiguration>)
                                {
                                    if (!isSimilarity || !AZ::IsFiniteFloat(childGeometry.m_height) || childGeometry.m_height <= 0.0f ||
                                        !AZ::IsFiniteFloat(childGeometry.m_radius) || childGeometry.m_radius <= 0.0f)
                                    {
                                        return false;
                                    }
                                    const float halfSegment = AZStd::max(0.5f * childGeometry.m_height - childGeometry.m_radius, 0.0f);
                                    capsules.push_back(
                                        { transformPoint(-AZ::Vector3::CreateAxisZ(halfSegment)),
                                          transformPoint(AZ::Vector3::CreateAxisZ(halfSegment)),
                                          material,
                                          childGeometry.m_radius * std::sqrt(basisLengthSq) });
                                    return true;
                                }
                                else if constexpr (AZStd::is_same_v<ChildGeometry, BoxShapeConfiguration>)
                                {
                                    if (!childGeometry.m_halfExtents.IsFinite() || childGeometry.m_halfExtents.GetMinElement() <= 0.0f)
                                    {
                                        return false;
                                    }
                                    AZStd::fixed_vector<AZ::Vector3, 8> points;
                                    for (float x : { -childGeometry.m_halfExtents.GetX(), childGeometry.m_halfExtents.GetX() })
                                    {
                                        for (float y : { -childGeometry.m_halfExtents.GetY(), childGeometry.m_halfExtents.GetY() })
                                        {
                                            for (float z : { -childGeometry.m_halfExtents.GetZ(), childGeometry.m_halfExtents.GetZ() })
                                            {
                                                points.push_back(transformPoint(AZ::Vector3(x, y, z)));
                                            }
                                        }
                                    }
                                    hullGeometries.push_back(CreateHullGeometry(points, GeometryTransform{}));
                                    if (hullGeometries.back() == nullptr)
                                    {
                                        return false;
                                    }
                                    hulls.push_back({ hullGeometries.back().get(), AZ::Transform::CreateIdentity(), material });
                                    return true;
                                }
                                else if constexpr (AZStd::is_same_v<ChildGeometry, CylinderShapeConfiguration>)
                                {
                                    if (childGeometry.m_sideCount < 3 || childGeometry.m_sideCount > 32 ||
                                        !AZ::IsFiniteFloat(childGeometry.m_height) || childGeometry.m_height <= 0.0f ||
                                        !AZ::IsFiniteFloat(childGeometry.m_radius) || childGeometry.m_radius <= 0.0f)
                                    {
                                        return false;
                                    }
                                    AZStd::fixed_vector<AZ::Vector3, 64> points;
                                    const float halfHeight = 0.5f * childGeometry.m_height;
                                    for (AZ::u32 side = 0; side < childGeometry.m_sideCount; ++side)
                                    {
                                        const float angle = 2.0f * AZ::Constants::Pi * aznumeric_cast<float>(side) /
                                            aznumeric_cast<float>(childGeometry.m_sideCount);
                                        float sine = 0.0f;
                                        float cosine = 0.0f;
                                        AZ::SinCos(angle, sine, cosine);
                                        const AZ::Vector3 radial(childGeometry.m_radius * cosine, childGeometry.m_radius * sine, 0.0f);
                                        points.push_back(transformPoint(radial - AZ::Vector3::CreateAxisZ(halfHeight)));
                                        points.push_back(transformPoint(radial + AZ::Vector3::CreateAxisZ(halfHeight)));
                                    }
                                    hullGeometries.push_back(CreateHullGeometry(points, GeometryTransform{}));
                                    if (hullGeometries.back() == nullptr)
                                    {
                                        return false;
                                    }
                                    hulls.push_back({ hullGeometries.back().get(), AZ::Transform::CreateIdentity(), material });
                                    return true;
                                }
                                else if constexpr (AZStd::is_same_v<ChildGeometry, ConvexHullShapeConfiguration>)
                                {
                                    AZStd::vector<AZ::Vector3> points;
                                    points.reserve(childGeometry.m_vertices.size());
                                    for (const AZ::Vector3& vertex : childGeometry.m_vertices)
                                    {
                                        if (!vertex.IsFinite())
                                        {
                                            return false;
                                        }
                                        points.push_back(transformPoint(vertex));
                                    }
                                    hullGeometries.push_back(CreateHullGeometry(points, GeometryTransform{}));
                                    if (hullGeometries.back() == nullptr)
                                    {
                                        return false;
                                    }
                                    hulls.push_back({ hullGeometries.back().get(), AZ::Transform::CreateIdentity(), material });
                                    return true;
                                }
                                else if constexpr (AZStd::is_same_v<ChildGeometry, TriangleMeshShapeConfiguration>)
                                {
                                    if (childGeometry.m_indices.size() % 3 != 0)
                                    {
                                        return false;
                                    }

                                    const size_t faceCount = childGeometry.m_indices.size() / 3;
                                    AZStd::vector<AZ::u8> materialIndices;
                                    AZStd::vector<SurfaceMaterial> childMaterials;
                                    if (childGeometry.m_materialIndices.empty())
                                    {
                                        if (child.m_materialIndex != 0 && materials.empty())
                                        {
                                            return false;
                                        }
                                        materialIndices.resize(faceCount, 0);
                                        childMaterials.push_back(material);
                                    }
                                    else
                                    {
                                        if (childGeometry.m_materialIndices.size() != faceCount)
                                        {
                                            return false;
                                        }

                                        AZStd::vector<AZ::u8> sourceIndices;
                                        sourceIndices.reserve(B3_MAX_COMPOUND_MESH_MATERIALS);
                                        materialIndices.reserve(faceCount);
                                        for (AZ::u8 sourceIndex : childGeometry.m_materialIndices)
                                        {
                                            if (materials.empty() || sourceIndex >= materials.size())
                                            {
                                                return false;
                                            }

                                            const auto existing = AZStd::find(sourceIndices.begin(), sourceIndices.end(), sourceIndex);
                                            if (existing == sourceIndices.end())
                                            {
                                                if (sourceIndices.size() == B3_MAX_COMPOUND_MESH_MATERIALS)
                                                {
                                                    return false;
                                                }
                                                sourceIndices.push_back(sourceIndex);
                                                childMaterials.push_back(materials[sourceIndex]);
                                            }
                                            materialIndices.push_back(
                                                aznumeric_cast<AZ::u8>(
                                                    AZStd::find(sourceIndices.begin(), sourceIndices.end(), sourceIndex) -
                                                    sourceIndices.begin()));
                                        }
                                    }

                                    AZStd::vector<AZ::Vector3> transformedVertices;
                                    transformedVertices.reserve(childGeometry.m_vertices.size());
                                    for (const AZ::Vector3& vertex : childGeometry.m_vertices)
                                    {
                                        if (!vertex.IsFinite())
                                        {
                                            return false;
                                        }
                                        transformedVertices.push_back(transformPoint(vertex));
                                    }
                                    AZStd::vector<AZ::u32> transformedIndices = childGeometry.m_indices;
                                    if (basisX.Cross(basisY).Dot(basisZ) < 0.0f)
                                    {
                                        for (size_t index = 0; index < transformedIndices.size(); index += 3)
                                        {
                                            AZStd::swap(transformedIndices[index + 1], transformedIndices[index + 2]);
                                        }
                                    }
                                    meshGeometries.push_back(CreateMeshGeometry(
                                        transformedVertices,
                                        transformedIndices,
                                        materialIndices,
                                        childGeometry.m_weldTolerance,
                                        childGeometry.m_weldVertices,
                                        childGeometry.m_useMedianSplit,
                                        childGeometry.m_identifyEdges));
                                    if (meshGeometries.back() == nullptr)
                                    {
                                        return false;
                                    }
                                    meshMaterials.push_back(AZStd::move(childMaterials));
                                    meshes.push_back(
                                        { meshGeometries.back().get(),
                                          meshMaterials.back(),
                                          AZ::Transform::CreateIdentity(),
                                          AZ::Vector3::CreateOne() });
                                    return true;
                                }
                            },
                            child.m_geometry);
                        if (!created)
                        {
                            return AZStd::monostate{};
                        }
                    }

                    return AZStd::shared_ptr<const CompoundGeometry>(
                        CreateCompoundGeometry({ spheres, capsules, hulls, meshes, materials }));
                }
            },
            geometry);
    }

    ShapeId CreateShape(BodyId bodyId, const NativeShapeConfiguration& configuration, const NativeGeometry& geometry)
    {
        return AZStd::visit(
            [bodyId, &configuration](const auto& nativeGeometry) -> ShapeId
            {
                using Geometry = AZStd::remove_cvref_t<decltype(nativeGeometry)>;
                if constexpr (AZStd::is_same_v<Geometry, AZStd::monostate>)
                {
                    return {};
                }
                else if constexpr (AZStd::is_same_v<Geometry, NativeSphereGeometry>)
                {
                    return CreateSphereShape(bodyId, configuration, nativeGeometry.m_center, nativeGeometry.m_radius);
                }
                else if constexpr (AZStd::is_same_v<Geometry, NativeCapsuleGeometry>)
                {
                    return CreateCapsuleShape(
                        bodyId, configuration, nativeGeometry.m_center1, nativeGeometry.m_center2, nativeGeometry.m_radius);
                }
                else if constexpr (AZStd::is_same_v<Geometry, AZStd::shared_ptr<const HullGeometry>>)
                {
                    return nativeGeometry != nullptr ? CreateHullShape(bodyId, configuration, *nativeGeometry) : ShapeId{};
                }
                else if constexpr (AZStd::is_same_v<Geometry, AZStd::shared_ptr<const MeshGeometry>>)
                {
                    return nativeGeometry != nullptr ? CreateMeshShape(bodyId, configuration, *nativeGeometry) : ShapeId{};
                }
                else if constexpr (AZStd::is_same_v<Geometry, AZStd::shared_ptr<const HeightFieldGeometry>>)
                {
                    return nativeGeometry != nullptr ? CreateHeightFieldShape(bodyId, configuration, *nativeGeometry) : ShapeId{};
                }
                else if constexpr (AZStd::is_same_v<Geometry, AZStd::shared_ptr<const CompoundGeometry>>)
                {
                    return nativeGeometry != nullptr ? CreateCompoundShape(bodyId, configuration, *nativeGeometry) : ShapeId{};
                }
            },
            geometry);
    }

    bool SetShapeGeometry(ShapeId shapeId, const NativeGeometry& geometry)
    {
        if (!IsValid(shapeId))
        {
            return false;
        }

        NativeShapeState state;
        if (!GetShapeState(shapeId, state))
        {
            return false;
        }
        return AZStd::visit(
            [shapeId, shapeType = state.m_type](const auto& nativeGeometry)
            {
                using Geometry = AZStd::remove_cvref_t<decltype(nativeGeometry)>;
                if constexpr (AZStd::is_same_v<Geometry, NativeSphereGeometry>)
                {
                    return shapeType == NativeShapeType::Sphere &&
                        SetSphereShape(shapeId, nativeGeometry.m_center, nativeGeometry.m_radius);
                }
                else if constexpr (AZStd::is_same_v<Geometry, NativeCapsuleGeometry>)
                {
                    return shapeType == NativeShapeType::Capsule &&
                        SetCapsuleShape(shapeId, nativeGeometry.m_center1, nativeGeometry.m_center2, nativeGeometry.m_radius);
                }
                else if constexpr (AZStd::is_same_v<Geometry, AZStd::shared_ptr<const HullGeometry>>)
                {
                    return shapeType == NativeShapeType::Hull && nativeGeometry != nullptr && SetHullShape(shapeId, *nativeGeometry);
                }
                else if constexpr (AZStd::is_same_v<Geometry, AZStd::shared_ptr<const MeshGeometry>>)
                {
                    return shapeType == NativeShapeType::Mesh && nativeGeometry != nullptr && SetMeshShape(shapeId, *nativeGeometry);
                }
                else
                {
                    return false;
                }
            },
            geometry);
    }

    AZ::Aabb GetAabb(const NativeGeometry& geometry)
    {
        return AZStd::visit(
            [](const auto& nativeGeometry)
            {
                using Geometry = AZStd::remove_cvref_t<decltype(nativeGeometry)>;
                if constexpr (AZStd::is_same_v<Geometry, AZStd::monostate>)
                {
                    return AZ::Aabb::CreateNull();
                }
                else if constexpr (AZStd::is_same_v<Geometry, NativeSphereGeometry>)
                {
                    const b3Sphere sphere{ ToNative(nativeGeometry.m_center), nativeGeometry.m_radius };
                    return FromNative(b3ComputeSphereAABB(&sphere, b3Transform_identity));
                }
                else if constexpr (AZStd::is_same_v<Geometry, NativeCapsuleGeometry>)
                {
                    const b3Capsule capsule{ ToNative(nativeGeometry.m_center1),
                                             ToNative(nativeGeometry.m_center2),
                                             nativeGeometry.m_radius };
                    return FromNative(b3ComputeCapsuleAABB(&capsule, b3Transform_identity));
                }
                else if constexpr (AZStd::is_same_v<Geometry, AZStd::shared_ptr<const HullGeometry>>)
                {
                    return nativeGeometry != nullptr
                        ? FromNative(b3ComputeHullAABB(GeometryCastAccess::Get(*nativeGeometry), b3Transform_identity))
                        : AZ::Aabb::CreateNull();
                }
                else if constexpr (AZStd::is_same_v<Geometry, AZStd::shared_ptr<const MeshGeometry>>)
                {
                    return nativeGeometry != nullptr
                        ? FromNative(
                              b3ComputeMeshAABB(GeometryCastAccess::Get(*nativeGeometry), b3Transform_identity, b3Vec3{ 1.0f, 1.0f, 1.0f }))
                        : AZ::Aabb::CreateNull();
                }
                else if constexpr (AZStd::is_same_v<Geometry, AZStd::shared_ptr<const HeightFieldGeometry>>)
                {
                    return nativeGeometry != nullptr
                        ? FromNative(b3ComputeHeightFieldAABB(GeometryCastAccess::Get(*nativeGeometry), b3Transform_identity))
                        : AZ::Aabb::CreateNull();
                }
                else if constexpr (AZStd::is_same_v<Geometry, AZStd::shared_ptr<const CompoundGeometry>>)
                {
                    return nativeGeometry != nullptr ? GetAabb(*nativeGeometry) : AZ::Aabb::CreateNull();
                }
            },
            geometry);
    }

    bool CastRay(const NativeGeometry& geometry, const AZ::Vector3& start, const AZ::Vector3& translation, GeometryCastHit& hit)
    {
        if (!start.IsFinite() || !translation.IsFinite() || translation.IsZero())
        {
            return false;
        }
        return AZStd::visit(
            [&start, &translation, &hit](const auto& nativeGeometry)
            {
                using Geometry = AZStd::remove_cvref_t<decltype(nativeGeometry)>;
                if constexpr (AZStd::is_same_v<Geometry, AZStd::monostate>)
                {
                    return false;
                }
                else if constexpr (AZStd::is_same_v<Geometry, NativeSphereGeometry>)
                {
                    return CastRaySphere(nativeGeometry.m_center, nativeGeometry.m_radius, start, translation, hit);
                }
                else if constexpr (AZStd::is_same_v<Geometry, NativeCapsuleGeometry>)
                {
                    return CastRayCapsule(
                        nativeGeometry.m_center1, nativeGeometry.m_center2, nativeGeometry.m_radius, start, translation, hit);
                }
                else
                {
                    return nativeGeometry != nullptr ? CastRay(*nativeGeometry, start, translation, hit) : false;
                }
            },
            geometry);
    }

    void DestroyShape(ShapeId shapeId, bool updateBodyMass)
    {
        if (IsValid(shapeId))
        {
            b3DestroyShape(b3LoadShapeId(shapeId.m_value), updateBodyMass);
        }
    }

    bool IsValid(ShapeId shapeId)
    {
        return shapeId.IsValid() && b3Shape_IsValid(b3LoadShapeId(shapeId.m_value));
    }

    bool GetShapeState(ShapeId shapeId, NativeShapeState& state)
    {
        state = {};
        if (!IsValid(shapeId))
        {
            return false;
        }

        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        switch (b3Shape_GetType(nativeShapeId))
        {
        case b3_sphereShape:
            state.m_type = NativeShapeType::Sphere;
            break;
        case b3_capsuleShape:
            state.m_type = NativeShapeType::Capsule;
            break;
        case b3_hullShape:
            state.m_type = NativeShapeType::Hull;
            break;
        case b3_meshShape:
            state.m_type = NativeShapeType::Mesh;
            break;
        case b3_heightShape:
            state.m_type = NativeShapeType::Heightfield;
            break;
        case b3_compoundShape:
            state.m_type = NativeShapeType::Compound;
            break;
        default:
            return false;
        }

        const b3Filter filter = b3Shape_GetFilter(nativeShapeId);
        state.m_categoryBits = filter.categoryBits;
        state.m_maskBits = filter.maskBits;
        state.m_groupIndex = filter.groupIndex;
        state.m_density = b3Shape_GetDensity(nativeShapeId);
        state.m_friction = b3Shape_GetFriction(nativeShapeId);
        state.m_restitution = b3Shape_GetRestitution(nativeShapeId);
        state.m_isSensor = b3Shape_IsSensor(nativeShapeId);
        state.m_enableSensorEvents = b3Shape_AreSensorEventsEnabled(nativeShapeId);
        state.m_enableContactEvents = b3Shape_AreContactEventsEnabled(nativeShapeId);
        state.m_enableHitEvents = b3Shape_AreHitEventsEnabled(nativeShapeId);
        state.m_enablePreSolveEvents = b3Shape_ArePreSolveEventsEnabled(nativeShapeId);
        return true;
    }

    void SetShapeFilter(ShapeId shapeId, AZ::u64 categoryBits, AZ::u64 maskBits, AZ::s32 groupIndex)
    {
        b3Filter filter = b3Shape_GetFilter(b3LoadShapeId(shapeId.m_value));
        filter.categoryBits = categoryBits;
        filter.maskBits = maskBits;
        filter.groupIndex = groupIndex;
        b3Shape_SetFilter(b3LoadShapeId(shapeId.m_value), filter, true);
    }

    bool SetShapeDensity(ShapeId shapeId, float density, bool updateBodyMass)
    {
        if (!IsValid(shapeId) || !AZ::IsFiniteFloat(density) || density < 0.0f)
        {
            return false;
        }
        b3Shape_SetDensity(b3LoadShapeId(shapeId.m_value), density, updateBodyMass);
        return true;
    }

    bool SetShapeFriction(ShapeId shapeId, float friction)
    {
        if (!IsValid(shapeId) || !AZ::IsFiniteFloat(friction) || friction < 0.0f)
        {
            return false;
        }
        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        if (b3Shape_GetType(nativeShapeId) == b3_compoundShape)
        {
            return false;
        }
        b3Shape_SetFriction(nativeShapeId, friction);
        return true;
    }

    bool SetShapeRestitution(ShapeId shapeId, float restitution)
    {
        if (!IsValid(shapeId) || !AZ::IsFiniteFloat(restitution) || restitution < 0.0f)
        {
            return false;
        }
        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        if (b3Shape_GetType(nativeShapeId) == b3_compoundShape)
        {
            return false;
        }
        b3Shape_SetRestitution(nativeShapeId, restitution);
        return true;
    }

    size_t GetShapeMaterialCount(ShapeId shapeId)
    {
        return IsValid(shapeId) ? aznumeric_cast<size_t>(b3Shape_GetMeshMaterialCount(b3LoadShapeId(shapeId.m_value))) : 0;
    }

    bool GetShapeMaterials(ShapeId shapeId, AZStd::span<SurfaceMaterial> materials)
    {
        if (!IsValid(shapeId) || materials.size() != GetShapeMaterialCount(shapeId))
        {
            return false;
        }
        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        for (size_t materialIndex = 0; materialIndex < materials.size(); ++materialIndex)
        {
            const b3SurfaceMaterial material = b3Shape_GetMeshSurfaceMaterial(nativeShapeId, aznumeric_cast<int>(materialIndex));
            SurfaceMaterial& output = materials[materialIndex];
            output.m_tangentVelocity = FromNative(material.tangentVelocity);
            output.m_userId = material.userMaterialId;
            output.m_debugColor = material.customColor;
            output.m_friction = material.friction;
            output.m_restitution = material.restitution;
            output.m_rollingResistance = material.rollingResistance;
        }
        return true;
    }

    void SetShapeSurfaceMaterial(ShapeId shapeId, const SurfaceMaterial& material)
    {
        if (IsValid(shapeId))
        {
            b3Shape_SetSurfaceMaterial(b3LoadShapeId(shapeId.m_value), ToNative(material));
        }
    }

    bool SetShapeMaterials(ShapeId shapeId, AZStd::span<const SurfaceMaterial> materials)
    {
        if (!IsValid(shapeId) || materials.empty())
        {
            return false;
        }

        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        if (b3Shape_GetType(nativeShapeId) == b3_compoundShape)
        {
            return false;
        }
        if (aznumeric_cast<size_t>(b3Shape_GetMeshMaterialCount(nativeShapeId)) != materials.size())
        {
            return false;
        }
        for (size_t materialIndex = 0; materialIndex < materials.size(); ++materialIndex)
        {
            b3Shape_SetMeshMaterial(nativeShapeId, ToNative(materials[materialIndex]), aznumeric_cast<int>(materialIndex));
        }
        return true;
    }

    bool SetShapeEventSubscriptions(ShapeId shapeId, bool sensorEvents, bool contactEvents, bool hitEvents, bool preSolveEvents)
    {
        if (!IsValid(shapeId))
        {
            return false;
        }
        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        b3Shape_EnableSensorEvents(nativeShapeId, sensorEvents);
        b3Shape_EnableContactEvents(nativeShapeId, contactEvents);
        b3Shape_EnableHitEvents(nativeShapeId, hitEvents);
        b3Shape_EnablePreSolveEvents(nativeShapeId, preSolveEvents);
        return true;
    }

    AZ::Aabb GetShapeAabb(ShapeId shapeId)
    {
        const b3AABB aabb = b3Shape_GetAABB(b3LoadShapeId(shapeId.m_value));
        return FromNative(aabb);
    }

    bool GetShapeClosestPoint(ShapeId shapeId, const AZ::Vector3& target, AZ::Vector3& position)
    {
        if (!IsValid(shapeId) || !target.IsFinite())
        {
            return false;
        }
        position = FromNative(b3Shape_GetClosestPoint(b3LoadShapeId(shapeId.m_value), ToNative(target)));
        return position.IsFinite();
    }

    bool CastRay(ShapeId shapeId, const AZ::Vector3& start, const AZ::Vector3& translation, GeometryCastHit& hit)
    {
        hit = {};
        if (!IsValid(shapeId) || !start.IsFinite() || !translation.IsFinite() || translation.IsZero())
        {
            return false;
        }

        const b3ShapeId nativeShapeId = b3LoadShapeId(shapeId.m_value);
        const b3WorldCastOutput output = b3Shape_RayCast(nativeShapeId, ToNativePosition(start), ToNative(translation));
        if (!output.hit)
        {
            return false;
        }
        AZ::u64 userMaterialId = 0;
        if (0 <= output.materialIndex && output.materialIndex < b3Shape_GetMeshMaterialCount(nativeShapeId))
        {
            userMaterialId = b3Shape_GetMeshSurfaceMaterial(nativeShapeId, output.materialIndex).userMaterialId;
        }
        hit = { FromNativePosition(output.point),
                FromNative(output.normal),
                output.fraction,
                output.materialIndex,
                output.triangleIndex,
                output.childIndex,
                userMaterialId };
        return true;
    }

    bool ApplyWind(ShapeId shapeId, const AZ::Vector3& velocity, float drag, float lift, float maximumSpeed, bool wake)
    {
        if (!IsValid(shapeId) || !velocity.IsFinite() || !AZ::IsFiniteFloat(drag) || drag < 0.0f || !AZ::IsFiniteFloat(lift) ||
            !AZ::IsFiniteFloat(maximumSpeed) || maximumSpeed < 0.0f)
        {
            return false;
        }

        b3Shape_ApplyWind(b3LoadShapeId(shapeId.m_value), ToNative(velocity), drag, lift, maximumSpeed, wake);
        return true;
    }
} // namespace Box3D
