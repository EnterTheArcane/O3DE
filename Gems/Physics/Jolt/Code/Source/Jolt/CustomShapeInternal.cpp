/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/CustomShapeInternal.h>

#include <Jolt/MaterialInternal.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/std/containers/fixed_vector.h>

#include <Jolt/Physics/Collision/CollideSoftBodyVertexIterator.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRenderer.h>
#endif

#include <cmath>

namespace Jolt
{
    namespace
    {
        [[nodiscard]]
        AZ::Vector3 FromNativeCustomVector(const JPH::Vec3Arg value)
        {
            return {value.GetX(), value.GetY(), value.GetZ()};
        }

        [[nodiscard]]
        JPH::Vec3 ToNativeCustomVector(const AZ::Vector3& value)
        {
            return {value.GetX(), value.GetY(), value.GetZ()};
        }

        [[nodiscard]]
        AZ::Transform FromNativeCustomTransform(JPH::Mat44Arg transform)
        {
            return AZ::Transform::CreateFromMatrix3x3AndTranslation(
                AZ::Matrix3x3::CreateFromColumns(
                    FromNativeCustomVector(transform.GetColumn3(0)),
                    FromNativeCustomVector(transform.GetColumn3(1)),
                    FromNativeCustomVector(transform.GetColumn3(2))),
                FromNativeCustomVector(transform.GetTranslation()));
        }

        [[nodiscard]]
        ShapeKind FromNativeShapeKind(const JPH::EShapeSubType shapeKind)
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
        MaterialHandle FromNativeMaterial(const JPH::PhysicsMaterial* material)
        {
            if (!material || material == JPH::PhysicsMaterial::sDefault)
            {
                return {};
            }
            return static_cast<const NativeMaterial*>(material)->GetHandle();
        }

        [[nodiscard]]
        bool ToNativeBackFaceMode(
            const BackFaceMode source,
            JPH::EBackFaceMode& target)
        {
            if (source == BackFaceMode::Ignore)
            {
                target = JPH::EBackFaceMode::IgnoreBackFaces;
                return true;
            }
            if (source == BackFaceMode::Collide)
            {
                target = JPH::EBackFaceMode::CollideWithBackFaces;
                return true;
            }
            return false;
        }

        class CustomShape final
            : public JPH::Shape
        {
        public:
            JPH_OVERRIDE_NEW_DELETE

            CustomShape()
                : JPH::Shape(JPH::EShapeType::User1, JPH::EShapeSubType::User1)
            {
            }

            CustomShape(
                const JPH::Shape* delegate,
                const CustomShapeInfo& info,
                ICustomShapeProvider& provider,
                AZStd::vector<AZ::u8> runtimeData)
                : JPH::Shape(JPH::EShapeType::User1, JPH::EShapeSubType::User1)
                , m_delegate(const_cast<JPH::Shape*>(delegate))
                , m_info(info)
                , m_provider(&provider)
                , m_runtimeData(AZStd::move(runtimeData))
            {
            }

            [[nodiscard]]
            const JPH::Shape* GetDelegate() const
            {
                return m_delegate;
            }

            [[nodiscard]]
            const CustomShapeInfo& GetInfo() const
            {
                return m_info;
            }

            [[nodiscard]]
            ICustomShapeProvider* GetProvider() const
            {
                return m_provider;
            }

            [[nodiscard]]
            AZStd::span<const AZ::u8> GetRuntimeData() const
            {
                return m_runtimeData;
            }

            void SetProvider(ICustomShapeProvider& provider)
            {
                m_provider = &provider;
            }

            [[nodiscard]]
            bool IsValid() const
            {
                return m_delegate;
            }

            bool MustBeStatic() const override
            {
                return m_delegate && m_delegate->MustBeStatic();
            }

            JPH::Vec3 GetCenterOfMass() const override
            {
                if (m_delegate)
                {
                    return m_delegate->GetCenterOfMass();
                }
                return JPH::Vec3::sZero();
            }

            JPH::AABox GetLocalBounds() const override
            {
                if (m_delegate)
                {
                    return m_delegate->GetLocalBounds();
                }
                return {};
            }

            JPH::uint GetSubShapeIDBitsRecursive() const override
            {
                if (m_delegate)
                {
                    return m_delegate->GetSubShapeIDBitsRecursive();
                }
                return 0;
            }

            float GetInnerRadius() const override
            {
                if (m_delegate)
                {
                    return m_delegate->GetInnerRadius();
                }
                return 0.0f;
            }

            JPH::MassProperties GetMassProperties() const override
            {
                if (m_delegate)
                {
                    return m_delegate->GetMassProperties();
                }
                return {};
            }

            const JPH::Shape* GetLeafShape(
                const JPH::SubShapeID& subShapeId,
                JPH::SubShapeID& remainder) const override
            {
                if (m_delegate)
                {
                    return m_delegate->GetLeafShape(subShapeId, remainder);
                }
                return nullptr;
            }

            const JPH::PhysicsMaterial* GetMaterial(
                const JPH::SubShapeID& subShapeId) const override
            {
                if (m_delegate)
                {
                    return m_delegate->GetMaterial(subShapeId);
                }
                return nullptr;
            }

            JPH::Vec3 GetSurfaceNormal(
                const JPH::SubShapeID& subShapeId,
                const JPH::Vec3Arg localSurfacePosition) const override
            {
                if (m_delegate)
                {
                    return m_delegate->GetSurfaceNormal(subShapeId, localSurfacePosition);
                }
                return JPH::Vec3::sAxisY();
            }

            void GetSupportingFace(
                const JPH::SubShapeID& subShapeId,
                const JPH::Vec3Arg direction,
                const JPH::Vec3Arg scale,
                JPH::Mat44Arg centerOfMassTransform,
                SupportingFace& vertices) const override
            {
                if (m_delegate)
                {
                    m_delegate->GetSupportingFace(
                        subShapeId,
                        direction,
                        scale,
                        centerOfMassTransform,
                        vertices);
                }
            }

            JPH::uint64 GetSubShapeUserData(
                const JPH::SubShapeID& subShapeId) const override
            {
                if (m_delegate)
                {
                    if (m_delegate->GetSubType() == JPH::EShapeSubType::Mesh)
                    {
                        return static_cast<const JPH::MeshShape*>(m_delegate.GetPtr())
                            ->GetTriangleUserData(subShapeId);
                    }
                    return m_delegate->GetSubShapeUserData(subShapeId);
                }
                return 0;
            }

            void GetSubmergedVolume(
                JPH::Mat44Arg centerOfMassTransform,
                const JPH::Vec3Arg scale,
                const JPH::Plane& surface,
                float& totalVolume,
                float& submergedVolume,
                JPH::Vec3& centerOfBuoyancy
#ifdef JPH_DEBUG_RENDERER
                , JPH::RVec3Arg baseOffset
#endif
                ) const override
            {
                if (m_delegate)
                {
                    m_delegate->GetSubmergedVolume(
                        centerOfMassTransform,
                        scale,
                        surface,
                        totalVolume,
                        submergedVolume,
                        centerOfBuoyancy
#ifdef JPH_DEBUG_RENDERER
                        , baseOffset
#endif
                    );
                }
            }

#ifdef JPH_DEBUG_RENDERER
            void Draw(
                JPH::DebugRenderer* renderer,
                JPH::RMat44Arg centerOfMassTransform,
                const JPH::Vec3Arg scale,
                const JPH::ColorArg color,
                const bool useMaterialColors,
                const bool drawWireframe) const override
            {
                if (m_delegate)
                {
                    m_delegate->Draw(
                        renderer,
                        centerOfMassTransform,
                        scale,
                        color,
                        useMaterialColors,
                        drawWireframe);
                }
            }
#endif

            bool CastRay(
                const JPH::RayCast& ray,
                const JPH::SubShapeIDCreator& subShapeIdCreator,
                JPH::RayCastResult& hit) const override
            {
                return m_delegate && m_delegate->CastRay(ray, subShapeIdCreator, hit);
            }

            void CastRay(
                const JPH::RayCast& ray,
                const JPH::RayCastSettings& settings,
                const JPH::SubShapeIDCreator& subShapeIdCreator,
                JPH::CastRayCollector& collector,
                const JPH::ShapeFilter& filter) const override
            {
                if (m_delegate)
                {
                    m_delegate->CastRay(ray, settings, subShapeIdCreator, collector, filter);
                }
            }

            void CollidePoint(
                const JPH::Vec3Arg point,
                const JPH::SubShapeIDCreator& subShapeIdCreator,
                JPH::CollidePointCollector& collector,
                const JPH::ShapeFilter& filter) const override
            {
                if (m_delegate)
                {
                    m_delegate->CollidePoint(point, subShapeIdCreator, collector, filter);
                }
            }

            void CollideSoftBodyVertices(
                JPH::Mat44Arg centerOfMassTransform,
                const JPH::Vec3Arg scale,
                const JPH::CollideSoftBodyVertexIterator& vertices,
                const JPH::uint vertexCount,
                const int collidingShapeIndex) const override
            {
                if (m_delegate)
                {
                    m_delegate->CollideSoftBodyVertices(
                        centerOfMassTransform,
                        scale,
                        vertices,
                        vertexCount,
                        collidingShapeIndex);
                }
            }

            void CollectTransformedShapes(
                const JPH::AABox& bounds,
                const JPH::Vec3Arg centerOfMassPosition,
                const JPH::QuatArg rotation,
                const JPH::Vec3Arg scale,
                const JPH::SubShapeIDCreator& subShapeIdCreator,
                JPH::TransformedShapeCollector& collector,
                const JPH::ShapeFilter& filter) const override
            {
                if (m_delegate)
                {
                    m_delegate->CollectTransformedShapes(
                        bounds,
                        centerOfMassPosition,
                        rotation,
                        scale,
                        subShapeIdCreator,
                        collector,
                        filter);
                }
            }

            void GetTrianglesStart(
                GetTrianglesContext& context,
                const JPH::AABox& bounds,
                const JPH::Vec3Arg centerOfMassPosition,
                const JPH::QuatArg rotation,
                const JPH::Vec3Arg scale) const override
            {
                if (m_delegate)
                {
                    m_delegate->GetTrianglesStart(
                        context,
                        bounds,
                        centerOfMassPosition,
                        rotation,
                        scale);
                }
            }

            int GetTrianglesNext(
                GetTrianglesContext& context,
                const int maximumTriangleCount,
                JPH::Float3* triangleVertices,
                const JPH::PhysicsMaterial** materials) const override
            {
                if (m_delegate)
                {
                    return m_delegate->GetTrianglesNext(
                        context,
                        maximumTriangleCount,
                        triangleVertices,
                        materials);
                }
                return 0;
            }

            void SaveBinaryState(JPH::StreamOut& stream) const override
            {
                JPH::Shape::SaveBinaryState(stream);
                stream.WriteBytes(&m_info.m_providerId, sizeof(m_info.m_providerId));
                stream.Write(m_info.m_providerVersion);
                stream.Write(m_info.m_sourceHash);
                const AZ::u32 runtimeDataSize = aznumeric_cast<AZ::u32>(m_runtimeData.size());
                stream.Write(runtimeDataSize);
                if (runtimeDataSize > 0)
                {
                    stream.WriteBytes(m_runtimeData.data(), runtimeDataSize);
                }
                m_delegate->SaveBinaryState(stream);
            }

            void SaveMaterialState(JPH::PhysicsMaterialList& materials) const override
            {
                if (m_delegate)
                {
                    m_delegate->SaveMaterialState(materials);
                }
            }

            void RestoreMaterialState(
                const JPH::PhysicsMaterialRefC* materials,
                const JPH::uint materialCount) override
            {
                if (m_delegate)
                {
                    m_delegate->RestoreMaterialState(materials, materialCount);
                }
            }

            Stats GetStats() const override
            {
                if (!m_delegate)
                {
                    return {sizeof(*this), 0};
                }
                const Stats delegateStats = m_delegate->GetStats();
                return {sizeof(*this) + delegateStats.mSizeBytes, delegateStats.mNumTriangles};
            }

            float GetVolume() const override
            {
                if (m_delegate)
                {
                    return m_delegate->GetVolume();
                }
                return 0.0f;
            }

            bool IsValidScale(const JPH::Vec3Arg scale) const override
            {
                return m_delegate && m_delegate->IsValidScale(scale);
            }

            JPH::Vec3 MakeScaleValid(const JPH::Vec3Arg scale) const override
            {
                if (m_delegate)
                {
                    return m_delegate->MakeScaleValid(scale);
                }
                return JPH::Vec3::sOne();
            }

        protected:
            void RestoreBinaryState(JPH::StreamIn& stream) override
            {
                JPH::Shape::RestoreBinaryState(stream);
                stream.ReadBytes(&m_info.m_providerId, sizeof(m_info.m_providerId));
                stream.Read(m_info.m_providerVersion);
                stream.Read(m_info.m_sourceHash);
                AZ::u32 runtimeDataSize = 0;
                stream.Read(runtimeDataSize);
                if (runtimeDataSize > MaximumCustomShapeRuntimeDataSize)
                {
                    return;
                }
                m_runtimeData.resize(runtimeDataSize);
                if (runtimeDataSize > 0)
                {
                    stream.ReadBytes(m_runtimeData.data(), runtimeDataSize);
                }
                JPH::Shape::ShapeResult delegateResult = JPH::Shape::sRestoreFromBinaryState(stream);
                if (!delegateResult.HasError()
                    && delegateResult.Get()->GetSubType() != JPH::EShapeSubType::User1)
                {
                    m_delegate = delegateResult.Get();
                }
            }

        private:
            JPH::Ref<JPH::Shape> m_delegate;
            CustomShapeInfo m_info;
            ICustomShapeProvider* m_provider = nullptr;
            AZStd::vector<AZ::u8> m_runtimeData;
        };

        class CustomShapeView final
            : public ICustomShapeView
        {
        public:
            AZ_RTTI(CustomShapeView, "{C02BF506-9C77-4A8F-93A2-D413B2DB7741}", ICustomShapeView);

            CustomShapeView(
                const JPH::Shape& shape,
                const JPH::Vec3Arg scale,
                JPH::Mat44Arg transform)
                : m_shape(shape)
                , m_scale(scale)
                , m_transform(transform)
            {
                if (shape.GetSubType() == JPH::EShapeSubType::User1)
                {
                    m_customShape = static_cast<const CustomShape*>(&shape);
                }
            }

            AZ::TypeId GetProviderId() const override
            {
                if (m_customShape)
                {
                    return m_customShape->GetInfo().m_providerId;
                }
                return AZ::TypeId::CreateNull();
            }

            AZ::u64 GetProviderVersion() const override
            {
                if (m_customShape)
                {
                    return m_customShape->GetInfo().m_providerVersion;
                }
                return 0;
            }

            AZStd::span<const AZ::u8> GetRuntimeData() const override
            {
                if (m_customShape)
                {
                    return m_customShape->GetRuntimeData();
                }
                return {};
            }

            AZ::Transform GetCenterOfMassTransform() const override
            {
                return FromNativeCustomTransform(m_transform);
            }

            AZ::Vector3 GetScale() const override
            {
                return FromNativeCustomVector(m_scale);
            }

            ShapeProperties GetProperties() const override
            {
                const JPH::AABox bounds = m_shape.GetLocalBounds();
                const JPH::MassProperties massProperties = m_shape.GetMassProperties();
                const JPH::Vec3 centerOfMass = m_shape.GetCenterOfMass();
                return {
                    .m_inertia = AZ::Matrix3x3::CreateFromColumns(
                        {
                            massProperties.mInertia(0, 0),
                            massProperties.mInertia(1, 0),
                            massProperties.mInertia(2, 0),
                        },
                        {
                            massProperties.mInertia(0, 1),
                            massProperties.mInertia(1, 1),
                            massProperties.mInertia(2, 1),
                        },
                        {
                            massProperties.mInertia(0, 2),
                            massProperties.mInertia(1, 2),
                            massProperties.mInertia(2, 2),
                        }),
                    .m_localBounds = AZ::Aabb::CreateFromMinMax(
                        FromNativeCustomVector(bounds.mMin),
                        FromNativeCustomVector(bounds.mMax)),
                    .m_centerOfMass = FromNativeCustomVector(centerOfMass),
                    .m_innerRadius = m_shape.GetInnerRadius(),
                    .m_mass = massProperties.mMass,
                    .m_volume = m_shape.GetVolume(),
                    .m_subShapeIdBitCount = m_shape.GetSubShapeIDBitsRecursive(),
                    .m_kind = FromNativeShapeKind(m_shape.GetSubType()),
                    .m_mustBeStatic = m_shape.MustBeStatic(),
                };
            }

            ShapeStats GetStats() const override
            {
                const JPH::AABox bounds = m_shape.GetLocalBounds();
                const JPH::Shape::Stats stats = m_shape.GetStats();
                return {
                    .m_localBounds = AZ::Aabb::CreateFromMinMax(
                        FromNativeCustomVector(bounds.mMin),
                        FromNativeCustomVector(bounds.mMax)),
                    .m_memorySize = stats.mSizeBytes,
                    .m_triangleCount = stats.mNumTriangles,
                };
            }

            MaterialHandle GetMaterial(const SubShapeId subShapeId) const override
            {
                JPH::SubShapeID nativeSubShapeId;
                nativeSubShapeId.SetValue(subShapeId.GetValue());
                return FromNativeMaterial(m_shape.GetMaterial(nativeSubShapeId));
            }

            AZ::u64 GetUserData(const SubShapeId subShapeId) const override
            {
                JPH::SubShapeID nativeSubShapeId;
                nativeSubShapeId.SetValue(subShapeId.GetValue());
                return m_shape.GetSubShapeUserData(nativeSubShapeId);
            }

            bool GetSurfaceNormal(
                const SubShapeId subShapeId,
                const AZ::Vector3& localSurfacePosition,
                AZ::Vector3& normal) const override
            {
                if (!localSurfacePosition.IsFinite())
                {
                    return false;
                }

                JPH::SubShapeID nativeSubShapeId;
                nativeSubShapeId.SetValue(subShapeId.GetValue());
                normal = FromNativeCustomVector(m_shape.GetSurfaceNormal(
                    nativeSubShapeId,
                    ToNativeCustomVector(localSurfacePosition)));
                return normal.IsFinite() && !normal.IsZero();
            }

            QueryResult GetSupportingFace(
                const SubShapeId subShapeId,
                const AZ::Vector3& direction,
                const AZStd::span<AZ::Vector3> vertices) const override
            {
                if (!direction.IsFinite() || direction.IsZero())
                {
                    return {};
                }

                JPH::SubShapeID nativeSubShapeId;
                nativeSubShapeId.SetValue(subShapeId.GetValue());
                JPH::Shape::SupportingFace nativeVertices;
                m_shape.GetSupportingFace(
                    nativeSubShapeId,
                    ToNativeCustomVector(direction),
                    m_scale,
                    JPH::Mat44::sIdentity(),
                    nativeVertices);
                QueryResult result = {
                    .m_hitCount = aznumeric_cast<AZ::u32>(AZStd::min(
                        vertices.size(),
                        aznumeric_cast<size_t>(nativeVertices.size()))),
                    .m_requiredHitCount = aznumeric_cast<AZ::u32>(nativeVertices.size()),
                };
                for (AZ::u32 vertexIndex = 0; vertexIndex < result.m_hitCount; ++vertexIndex)
                {
                    vertices[vertexIndex] = FromNativeCustomVector(nativeVertices[vertexIndex]);
                }
                return result;
            }

            bool Raycast(
                const CustomShapeRaycastRequest& request,
                CustomShapeRaycastHit& hit) const override
            {
                if (!request.m_start.IsFinite()
                    || !request.m_displacement.IsFinite()
                    || request.m_displacement.IsZero())
                {
                    return false;
                }

                JPH::RayCastSettings settings;
                if (!ToNativeBackFaceMode(
                        request.m_convexBackFaceMode,
                        settings.mBackFaceModeConvex)
                    || !ToNativeBackFaceMode(
                        request.m_triangleBackFaceMode,
                        settings.mBackFaceModeTriangles))
                {
                    return false;
                }
                settings.mTreatConvexAsSolid = request.m_treatConvexAsSolid;

                const JPH::RayCast ray(
                    ToNativeCustomVector(request.m_start),
                    ToNativeCustomVector(request.m_displacement));
                JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> collector;
                const JPH::ShapeFilter filter;
                m_shape.CastRay(
                    ray,
                    settings,
                    JPH::SubShapeIDCreator(),
                    collector,
                    filter);
                if (!collector.HadHit())
                {
                    return false;
                }

                const JPH::RayCastResult& nativeHit = collector.mHit;
                const JPH::Vec3 position = ray.GetPointOnRay(nativeHit.mFraction);
                const JPH::Vec3 normal = m_shape.GetSurfaceNormal(
                    nativeHit.mSubShapeID2,
                    position);
                hit = {
                    .m_position = FromNativeCustomVector(position),
                    .m_normal = FromNativeCustomVector(normal),
                    .m_materialHandle = FromNativeMaterial(m_shape.GetMaterial(nativeHit.mSubShapeID2)),
                    .m_subShapeId = SubShapeId(nativeHit.mSubShapeID2.GetValue()),
                    .m_userData = m_shape.GetSubShapeUserData(nativeHit.mSubShapeID2),
                    .m_fraction = nativeHit.mFraction,
                    .m_isBackFaceHit = normal.Dot(ray.mDirection) > 0.0f,
                };
                return hit.m_position.IsFinite()
                    && hit.m_normal.IsFinite()
                    && AZ::IsFiniteFloat(hit.m_fraction);
            }

            bool CollidePoint(const AZ::Vector3& point) const override
            {
                if (!point.IsFinite())
                {
                    return false;
                }

                JPH::AnyHitCollisionCollector<JPH::CollidePointCollector> collector;
                const JPH::ShapeFilter filter;
                m_shape.CollidePoint(
                    ToNativeCustomVector(point),
                    JPH::SubShapeIDCreator(),
                    collector,
                    filter);
                return collector.HadHit();
            }

            QueryResult CollectTriangles(
                const AZ::Aabb& bounds,
                const AZStd::span<CustomShapeTriangleVertices> triangles) const override
            {
                if (!bounds.IsValid()
                    || !bounds.GetMin().IsFinite()
                    || !bounds.GetMax().IsFinite())
                {
                    return {};
                }

                JPH::Shape::GetTrianglesContext context;
                m_shape.GetTrianglesStart(
                    context,
                    JPH::AABox(
                        ToNativeCustomVector(bounds.GetMin()),
                        ToNativeCustomVector(bounds.GetMax())),
                    JPH::Vec3::sZero(),
                    JPH::Quat::sIdentity(),
                    m_scale);
                constexpr int TriangleBatchSize = 32;
                AZStd::array<JPH::Float3, TriangleBatchSize * 3> nativeVertices;
                AZStd::array<const JPH::PhysicsMaterial*, TriangleBatchSize> nativeMaterials;
                QueryResult result;
                while (true)
                {
                    const int triangleCount = m_shape.GetTrianglesNext(
                        context,
                        TriangleBatchSize,
                        nativeVertices.data(),
                        nativeMaterials.data());
                    if (triangleCount == 0)
                    {
                        break;
                    }

                    for (int triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
                    {
                        ++result.m_requiredHitCount;
                        if (result.m_hitCount >= triangles.size())
                        {
                            continue;
                        }

                        const size_t firstVertexIndex = aznumeric_cast<size_t>(triangleIndex) * 3;
                        const auto fromNativeVertex = [](const JPH::Float3& vertex)
                        {
                            return AZ::Vector3(vertex.x, vertex.y, vertex.z);
                        };
                        triangles[result.m_hitCount] = {
                            .m_first = fromNativeVertex(nativeVertices[firstVertexIndex]),
                            .m_second = fromNativeVertex(nativeVertices[firstVertexIndex + 1]),
                            .m_third = fromNativeVertex(nativeVertices[firstVertexIndex + 2]),
                            .m_materialHandle = FromNativeMaterial(nativeMaterials[triangleIndex]),
                        };
                        ++result.m_hitCount;
                    }
                }
                return result;
            }

        private:
            const JPH::Shape& m_shape;
            const CustomShape* m_customShape = nullptr;
            JPH::Vec3 m_scale;
            JPH::Mat44 m_transform;
        };

        [[nodiscard]]
        bool IsValidFace(const AZStd::span<const AZ::Vector3> face)
        {
            return face.size() <= MaximumSupportingFaceVertexCount
                && AZStd::all_of(
                    face.begin(),
                    face.end(),
                    [](const AZ::Vector3& vertex)
                    {
                        return vertex.IsFinite();
                    });
        }

        [[nodiscard]]
        bool CanMergeSubShapeId(
            const JPH::Shape& shape,
            const SubShapeId subShapeId)
        {
            const JPH::uint bitCount = shape.GetSubShapeIDBitsRecursive();
            if (subShapeId == SubShapeId::Root)
            {
                return bitCount == 0;
            }
            if (bitCount == 0)
            {
                return false;
            }
            if (bitCount >= AZStd::numeric_limits<SubShapeId::ValueType>::digits)
            {
                return true;
            }
            return subShapeId.GetValue() < (SubShapeId::ValueType{1} << bitCount);
        }

        [[nodiscard]]
        JPH::SubShapeID MergeSubShapeId(
            const JPH::SubShapeIDCreator& creator,
            const SubShapeId subShapeId,
            const JPH::Shape& shape)
        {
            const JPH::uint bitCount = shape.GetSubShapeIDBitsRecursive();
            if (bitCount == 0)
            {
                return creator.GetID();
            }

            return creator.PushID(subShapeId.GetValue(), bitCount).GetID();
        }

        class CustomShapeCollisionCollector final
            : public ICustomShapeCollisionCollector
        {
        public:
            AZ_RTTI(
                CustomShapeCollisionCollector,
                "{E37D7F5A-1EF6-4DB1-89F9-8D46AF0EB9CD}",
                ICustomShapeCollisionCollector);

            CustomShapeCollisionCollector(
                const JPH::Shape& firstShape,
                const JPH::Shape& secondShape,
                const JPH::SubShapeIDCreator& firstSubShapeId,
                const JPH::SubShapeIDCreator& secondSubShapeId,
                JPH::CollideShapeCollector& collector)
                : m_firstShape(firstShape)
                , m_secondShape(secondShape)
                , m_firstSubShapeId(firstSubShapeId)
                , m_secondSubShapeId(secondSubShapeId)
                , m_collector(collector)
            {
            }

            float GetEarlyOutFraction() const override
            {
                return m_collector.GetEarlyOutFraction();
            }

            bool AddHit(const CustomShapeCollisionHit& hit) override
            {
                if (!hit.m_firstContactPosition.IsFinite()
                    || !hit.m_secondContactPosition.IsFinite()
                    || !hit.m_penetrationAxis.IsFinite()
                    || hit.m_penetrationAxis.IsZero()
                    || !AZ::IsFiniteFloat(hit.m_penetrationDepth)
                    || !IsValidFace(hit.m_firstFace)
                    || !IsValidFace(hit.m_secondFace)
                    || !CanMergeSubShapeId(m_firstShape, hit.m_firstSubShapeId)
                    || !CanMergeSubShapeId(m_secondShape, hit.m_secondSubShapeId)
                    || m_hits.size() >= m_hits.max_size())
                {
                    m_invalid = true;
                    return false;
                }

                JPH::CollideShapeResult nativeHit(
                    ToNativeCustomVector(hit.m_firstContactPosition),
                    ToNativeCustomVector(hit.m_secondContactPosition),
                    ToNativeCustomVector(hit.m_penetrationAxis),
                    hit.m_penetrationDepth,
                    MergeSubShapeId(m_firstSubShapeId, hit.m_firstSubShapeId, m_firstShape),
                    MergeSubShapeId(m_secondSubShapeId, hit.m_secondSubShapeId, m_secondShape),
                    JPH::BodyID());
                for (const AZ::Vector3& vertex : hit.m_firstFace)
                {
                    nativeHit.mShape1Face.push_back(ToNativeCustomVector(vertex));
                }
                for (const AZ::Vector3& vertex : hit.m_secondFace)
                {
                    nativeHit.mShape2Face.push_back(ToNativeCustomVector(vertex));
                }
                m_hits.push_back(AZStd::move(nativeHit));
                return true;
            }

            [[nodiscard]]
            bool IsInvalid() const
            {
                return m_invalid;
            }

            [[nodiscard]]
            bool HasOutput() const
            {
                return !m_hits.empty();
            }

            void Commit()
            {
                for (const JPH::CollideShapeResult& hit : m_hits)
                {
                    m_collector.AddHit(hit);
                    if (m_collector.ShouldEarlyOut())
                    {
                        break;
                    }
                }
            }

        private:
            const JPH::Shape& m_firstShape;
            const JPH::Shape& m_secondShape;
            JPH::SubShapeIDCreator m_firstSubShapeId;
            JPH::SubShapeIDCreator m_secondSubShapeId;
            JPH::CollideShapeCollector& m_collector;
            AZStd::fixed_vector<JPH::CollideShapeResult, MaximumCustomShapeDispatchHitCount> m_hits;
            bool m_invalid = false;
        };

        class CustomShapeCastCollector final
            : public ICustomShapeCastCollector
        {
        public:
            AZ_RTTI(
                CustomShapeCastCollector,
                "{78C4D632-7C88-45F9-AC62-AB507219AFCC}",
                ICustomShapeCastCollector);

            CustomShapeCastCollector(
                const JPH::Shape& firstShape,
                const JPH::Shape& secondShape,
                const JPH::SubShapeIDCreator& firstSubShapeId,
                const JPH::SubShapeIDCreator& secondSubShapeId,
                JPH::CastShapeCollector& collector)
                : m_firstShape(firstShape)
                , m_secondShape(secondShape)
                , m_firstSubShapeId(firstSubShapeId)
                , m_secondSubShapeId(secondSubShapeId)
                , m_collector(collector)
            {
            }

            float GetEarlyOutFraction() const override
            {
                return m_collector.GetEarlyOutFraction();
            }

            bool AddHit(const CustomShapeCastHit& hit) override
            {
                if (!AZ::IsFiniteFloat(hit.m_fraction)
                    || hit.m_fraction < 0.0f
                    || hit.m_fraction > m_collector.GetPositiveEarlyOutFraction()
                    || !hit.m_collision.m_firstContactPosition.IsFinite()
                    || !hit.m_collision.m_secondContactPosition.IsFinite()
                    || !hit.m_collision.m_penetrationAxis.IsFinite()
                    || hit.m_collision.m_penetrationAxis.IsZero()
                    || !AZ::IsFiniteFloat(hit.m_collision.m_penetrationDepth)
                    || !IsValidFace(hit.m_collision.m_firstFace)
                    || !IsValidFace(hit.m_collision.m_secondFace)
                    || !CanMergeSubShapeId(m_firstShape, hit.m_collision.m_firstSubShapeId)
                    || !CanMergeSubShapeId(m_secondShape, hit.m_collision.m_secondSubShapeId)
                    || m_hits.size() >= m_hits.max_size())
                {
                    m_invalid = true;
                    return false;
                }

                JPH::ShapeCastResult nativeHit(
                    hit.m_fraction,
                    ToNativeCustomVector(hit.m_collision.m_firstContactPosition),
                    ToNativeCustomVector(hit.m_collision.m_secondContactPosition),
                    ToNativeCustomVector(hit.m_collision.m_penetrationAxis),
                    hit.m_isBackFaceHit,
                    MergeSubShapeId(
                        m_firstSubShapeId,
                        hit.m_collision.m_firstSubShapeId,
                        m_firstShape),
                    MergeSubShapeId(
                        m_secondSubShapeId,
                        hit.m_collision.m_secondSubShapeId,
                        m_secondShape),
                    JPH::BodyID());
                nativeHit.mPenetrationDepth = hit.m_collision.m_penetrationDepth;
                for (const AZ::Vector3& vertex : hit.m_collision.m_firstFace)
                {
                    nativeHit.mShape1Face.push_back(ToNativeCustomVector(vertex));
                }
                for (const AZ::Vector3& vertex : hit.m_collision.m_secondFace)
                {
                    nativeHit.mShape2Face.push_back(ToNativeCustomVector(vertex));
                }
                m_hits.push_back(AZStd::move(nativeHit));
                return true;
            }

            [[nodiscard]]
            bool IsInvalid() const
            {
                return m_invalid;
            }

            [[nodiscard]]
            bool HasOutput() const
            {
                return !m_hits.empty();
            }

            void Commit()
            {
                for (const JPH::ShapeCastResult& hit : m_hits)
                {
                    m_collector.AddHit(hit);
                    if (m_collector.ShouldEarlyOut())
                    {
                        break;
                    }
                }
            }

        private:
            const JPH::Shape& m_firstShape;
            const JPH::Shape& m_secondShape;
            JPH::SubShapeIDCreator m_firstSubShapeId;
            JPH::SubShapeIDCreator m_secondSubShapeId;
            JPH::CastShapeCollector& m_collector;
            AZStd::fixed_vector<JPH::ShapeCastResult, MaximumCustomShapeDispatchHitCount> m_hits;
            bool m_invalid = false;
        };

        [[nodiscard]]
        const JPH::Shape* UnwrapCustomShape(const JPH::Shape* shape)
        {
            if (shape && shape->GetSubType() == JPH::EShapeSubType::User1)
            {
                return static_cast<const CustomShape*>(shape)->GetDelegate();
            }
            return shape;
        }

        [[nodiscard]]
        const CustomShape* GetCustomShape(const JPH::Shape* shape)
        {
            if (shape && shape->GetSubType() == JPH::EShapeSubType::User1)
            {
                return static_cast<const CustomShape*>(shape);
            }
            return nullptr;
        }

        [[nodiscard]]
        CustomShapeCollisionSettings FromNativeCollisionSettings(
            const JPH::CollideShapeSettings& settings)
        {
            CustomShapeCollisionSettings result = {
                .m_activeEdgeMovementDirection = FromNativeCustomVector(settings.mActiveEdgeMovementDirection),
                .m_collisionTolerance = settings.mCollisionTolerance,
                .m_internalEdgeRemovalVertexTolerance =
                    sqrtf(settings.mInternalEdgeRemovalVertexToleranceSq),
                .m_maximumSeparationDistance = settings.mMaxSeparationDistance,
                .m_penetrationTolerance = settings.mPenetrationTolerance,
            };
            if (settings.mBackFaceMode == JPH::EBackFaceMode::CollideWithBackFaces)
            {
                result.m_backFaceMode = BackFaceMode::Collide;
            }
            if (settings.mActiveEdgeMode == JPH::EActiveEdgeMode::CollideWithAll)
            {
                result.m_activeEdgeMode = ActiveEdgeMode::CollideWithAll;
            }
            if (settings.mCollectFacesMode == JPH::ECollectFacesMode::CollectFaces)
            {
                result.m_faceCollectionMode = FaceCollectionMode::Collect;
            }
            return result;
        }

        [[nodiscard]]
        CustomShapeCastSettings FromNativeCastSettings(
            const JPH::ShapeCast& shapeCast,
            const JPH::ShapeCastSettings& settings)
        {
            CustomShapeCastSettings result = {
                .m_displacement = FromNativeCustomVector(shapeCast.mDirection),
                .m_activeEdgeMovementDirection = FromNativeCustomVector(settings.mActiveEdgeMovementDirection),
                .m_collisionTolerance = settings.mCollisionTolerance,
                .m_extraConvexRadius = settings.mExtraConvexRadius,
                .m_penetrationTolerance = settings.mPenetrationTolerance,
                .m_returnDeepestPoint = settings.mReturnDeepestPoint,
                .m_useShrunkenShapeAndConvexRadius = settings.mUseShrunkenShapeAndConvexRadius,
            };
            if (settings.mBackFaceModeConvex == JPH::EBackFaceMode::CollideWithBackFaces)
            {
                result.m_convexBackFaceMode = BackFaceMode::Collide;
            }
            if (settings.mBackFaceModeTriangles == JPH::EBackFaceMode::CollideWithBackFaces)
            {
                result.m_triangleBackFaceMode = BackFaceMode::Collide;
            }
            if (settings.mActiveEdgeMode == JPH::EActiveEdgeMode::CollideWithAll)
            {
                result.m_activeEdgeMode = ActiveEdgeMode::CollideWithAll;
            }
            if (settings.mCollectFacesMode == JPH::ECollectFacesMode::CollectFaces)
            {
                result.m_faceCollectionMode = FaceCollectionMode::Collect;
            }
            return result;
        }

        [[nodiscard]]
        bool DispatchCustomCollision(
            const CustomShape& customShape,
            const CustomShapeView& firstView,
            const CustomShapeView& secondView,
            const CustomShapeCollisionSettings& settings,
            CustomShapeCollisionCollector& collector)
        {
            ICustomShapeProvider* provider = customShape.GetProvider();
            if (!provider)
            {
                return false;
            }

            const CustomShapeDispatchResult result = provider->Collide(
                firstView,
                secondView,
                settings,
                collector);
            if (collector.IsInvalid()
                || result == CustomShapeDispatchResult::Invalid
                || result == CustomShapeDispatchResult::None
                || (result == CustomShapeDispatchResult::Unsupported && collector.HasOutput()))
            {
                AZ_Error(
                    "Jolt",
                    false,
                    "Custom-shape provider returned invalid collision output.");
                return true;
            }
            if (result == CustomShapeDispatchResult::Handled)
            {
                collector.Commit();
                return true;
            }
            return false;
        }

        [[nodiscard]]
        bool DispatchCustomCast(
            const CustomShape& customShape,
            const CustomShapeView& firstView,
            const CustomShapeView& secondView,
            const CustomShapeCastSettings& settings,
            CustomShapeCastCollector& collector)
        {
            ICustomShapeProvider* provider = customShape.GetProvider();
            if (!provider)
            {
                return false;
            }

            const CustomShapeDispatchResult result = provider->Cast(
                firstView,
                secondView,
                settings,
                collector);
            if (collector.IsInvalid()
                || result == CustomShapeDispatchResult::Invalid
                || result == CustomShapeDispatchResult::None
                || (result == CustomShapeDispatchResult::Unsupported && collector.HasOutput()))
            {
                AZ_Error(
                    "Jolt",
                    false,
                    "Custom-shape provider returned invalid cast output.");
                return true;
            }
            if (result == CustomShapeDispatchResult::Handled)
            {
                collector.Commit();
                return true;
            }
            return false;
        }

        void CollideCustomShape(
            const JPH::Shape* firstShape,
            const JPH::Shape* secondShape,
            const JPH::Vec3Arg firstScale,
            const JPH::Vec3Arg secondScale,
            JPH::Mat44Arg firstTransform,
            JPH::Mat44Arg secondTransform,
            const JPH::SubShapeIDCreator& firstSubShapeId,
            const JPH::SubShapeIDCreator& secondSubShapeId,
            const JPH::CollideShapeSettings& settings,
            JPH::CollideShapeCollector& collector,
            const JPH::ShapeFilter& filter)
        {
            const CustomShape* firstCustomShape = GetCustomShape(firstShape);
            const CustomShape* secondCustomShape = GetCustomShape(secondShape);
            const CustomShapeView firstView(*firstShape, firstScale, firstTransform);
            const CustomShapeView secondView(*secondShape, secondScale, secondTransform);
            CustomShapeCollisionCollector customCollector(
                *firstShape,
                *secondShape,
                firstSubShapeId,
                secondSubShapeId,
                collector);
            const CustomShapeCollisionSettings customSettings = FromNativeCollisionSettings(settings);
            if (firstCustomShape
                && DispatchCustomCollision(
                    *firstCustomShape,
                    firstView,
                    secondView,
                    customSettings,
                    customCollector))
            {
                return;
            }
            if (secondCustomShape
                && (!firstCustomShape
                    || secondCustomShape->GetProvider() != firstCustomShape->GetProvider())
                && DispatchCustomCollision(
                    *secondCustomShape,
                    firstView,
                    secondView,
                    customSettings,
                    customCollector))
            {
                return;
            }

            firstShape = UnwrapCustomShape(firstShape);
            secondShape = UnwrapCustomShape(secondShape);
            if (firstShape && secondShape)
            {
                JPH::CollisionDispatch::sCollideShapeVsShape(
                    firstShape,
                    secondShape,
                    firstScale,
                    secondScale,
                    firstTransform,
                    secondTransform,
                    firstSubShapeId,
                    secondSubShapeId,
                    settings,
                    collector,
                    filter);
            }
        }

        void CastCustomShape(
            const JPH::ShapeCast& shapeCast,
            const JPH::ShapeCastSettings& settings,
            const JPH::Shape* targetShape,
            const JPH::Vec3Arg targetScale,
            const JPH::ShapeFilter& filter,
            JPH::Mat44Arg targetTransform,
            const JPH::SubShapeIDCreator& firstSubShapeId,
            const JPH::SubShapeIDCreator& secondSubShapeId,
            JPH::CastShapeCollector& collector)
        {
            const CustomShape* firstCustomShape = GetCustomShape(shapeCast.mShape);
            const CustomShape* secondCustomShape = GetCustomShape(targetShape);
            const CustomShapeView firstView(
                *shapeCast.mShape,
                shapeCast.mScale,
                shapeCast.mCenterOfMassStart);
            const CustomShapeView secondView(*targetShape, targetScale, targetTransform);
            CustomShapeCastCollector customCollector(
                *shapeCast.mShape,
                *targetShape,
                firstSubShapeId,
                secondSubShapeId,
                collector);
            const CustomShapeCastSettings customSettings = FromNativeCastSettings(shapeCast, settings);
            if (firstCustomShape
                && DispatchCustomCast(
                    *firstCustomShape,
                    firstView,
                    secondView,
                    customSettings,
                    customCollector))
            {
                return;
            }
            if (secondCustomShape
                && (!firstCustomShape
                    || secondCustomShape->GetProvider() != firstCustomShape->GetProvider())
                && DispatchCustomCast(
                    *secondCustomShape,
                    firstView,
                    secondView,
                    customSettings,
                    customCollector))
            {
                return;
            }

            const JPH::Shape* castShape = UnwrapCustomShape(shapeCast.mShape);
            targetShape = UnwrapCustomShape(targetShape);
            if (castShape && targetShape)
            {
                const JPH::ShapeCast unwrappedCast(
                    castShape,
                    shapeCast.mScale,
                    shapeCast.mCenterOfMassStart,
                    shapeCast.mDirection,
                    shapeCast.mShapeWorldBounds);
                JPH::CollisionDispatch::sCastShapeVsShapeLocalSpace(
                    unwrappedCast,
                    settings,
                    targetShape,
                    targetScale,
                    filter,
                    targetTransform,
                    firstSubShapeId,
                    secondSubShapeId,
                    collector);
            }
        }
    } // namespace

    void RegisterCustomShapeType()
    {
        JPH::ShapeFunctions& functions = JPH::ShapeFunctions::sGet(JPH::EShapeSubType::User1);
        functions.mConstruct = []() -> JPH::Shape*
        {
            return new CustomShape();
        };
        functions.mColor = JPH::Color::sPurple;

        for (const JPH::EShapeSubType type : JPH::sAllSubShapeTypes)
        {
            JPH::CollisionDispatch::sRegisterCollideShape(
                JPH::EShapeSubType::User1,
                type,
                CollideCustomShape);
            JPH::CollisionDispatch::sRegisterCollideShape(
                type,
                JPH::EShapeSubType::User1,
                CollideCustomShape);
            JPH::CollisionDispatch::sRegisterCastShape(
                JPH::EShapeSubType::User1,
                type,
                CastCustomShape);
            JPH::CollisionDispatch::sRegisterCastShape(
                type,
                JPH::EShapeSubType::User1,
                CastCustomShape);
        }
    }

    NativeShapeResult CreateNativeCustomShape(
        const CustomShapeData& data,
        const CustomShapeInfo& info,
        ICustomShapeProvider& provider,
        const JPH::PhysicsMaterialList& materials,
        const float density,
        const AZ::u64 userData)
    {
        ShapeConfiguration delegateConfiguration;
        delegateConfiguration.m_density = density;
        if (data.m_geometryKind == CustomShapeGeometryKind::Convex)
        {
            delegateConfiguration.m_geometry = ConvexHullShapeConfiguration{
                .m_points = data.m_vertices,
                .m_hullTolerance = data.m_hullTolerance,
                .m_maximumConvexRadius = data.m_maximumConvexRadius,
                .m_maximumConvexRadiusError = data.m_maximumConvexRadiusError,
            };
        }
        else if (data.m_geometryKind == CustomShapeGeometryKind::Mesh)
        {
            MeshShapeConfiguration mesh;
            mesh.m_vertices = data.m_vertices;
            mesh.m_triangles.reserve(data.m_triangles.size());
            for (const CustomShapeTriangle& triangle : data.m_triangles)
            {
                mesh.m_triangles.push_back({
                    .m_firstVertex = triangle.m_firstVertex,
                    .m_secondVertex = triangle.m_secondVertex,
                    .m_thirdVertex = triangle.m_thirdVertex,
                    .m_materialIndex = triangle.m_materialIndex,
                    .m_userData = triangle.m_userData,
                });
            }
            mesh.m_activeEdgeCosineThreshold = data.m_activeEdgeCosineThreshold;
            mesh.m_maximumTrianglesPerLeaf = data.m_maximumTrianglesPerLeaf;
            mesh.m_perTriangleUserData = data.m_perTriangleUserData;
            delegateConfiguration.m_geometry = AZStd::move(mesh);
        }
        else
        {
            return {.m_error = "Custom shape geometry kind is invalid."};
        }

        NativeShapeResult delegateResult = CreateNativeShape(delegateConfiguration, materials);
        if (!delegateResult)
        {
            return delegateResult;
        }
        const_cast<JPH::Shape*>(delegateResult.m_shape.GetPtr())->SetUserData(userData);
        return {
            .m_shape = new CustomShape(
                delegateResult.m_shape,
                info,
                provider,
                data.m_runtimeData),
        };
    }

    bool BindNativeCustomShapeProvider(
        JPH::Shape& shape,
        ICustomShapeProvider& provider)
    {
        if (shape.GetSubType() != JPH::EShapeSubType::User1)
        {
            return false;
        }

        auto& customShape = static_cast<CustomShape&>(shape);
        if (!customShape.IsValid()
            || customShape.GetInfo().m_providerId != provider.GetId()
            || customShape.GetInfo().m_providerVersion != provider.GetVersion())
        {
            return false;
        }
        customShape.SetProvider(provider);
        return true;
    }

    bool GetNativeCustomShapeInfo(
        const JPH::Shape& shape,
        CustomShapeInfo& info)
    {
        if (shape.GetSubType() != JPH::EShapeSubType::User1)
        {
            return false;
        }
        const auto& customShape = static_cast<const CustomShape&>(shape);
        if (!customShape.IsValid())
        {
            return false;
        }
        info = customShape.GetInfo();
        return true;
    }

    bool IsNativeCustomShapeValid(const JPH::Shape& shape)
    {
        if (shape.GetSubType() != JPH::EShapeSubType::User1)
        {
            return true;
        }
        return static_cast<const CustomShape&>(shape).IsValid();
    }
} // namespace Jolt
