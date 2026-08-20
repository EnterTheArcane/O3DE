/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/Editor/SoftBodyComponent.h>

#include <Jolt/SoftBodyComponent.h>
#include <Jolt/Editor/DebugDraw.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Matrix3x4.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/utility/move.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>

namespace Jolt::Editor
{
    namespace
    {
        constexpr float SkinJointRadius = 0.025f;

        void CalculateSkinNormals(
            const SoftBodyDefinitionConfiguration& definition,
            AZStd::vector<AZ::Vector3>& normals,
            AZStd::vector<AZ::u8>& skinnedVertices)
        {
            normals.assign(definition.m_vertices.size(), AZ::Vector3::CreateZero());
            skinnedVertices.assign(definition.m_vertices.size(), 0);
            for (const SoftBodySkinConstraint& constraint : definition.m_skinConstraints)
            {
                if (constraint.m_vertex < skinnedVertices.size())
                {
                    skinnedVertices[constraint.m_vertex] = 1;
                }
            }

            for (const SoftBodyFace& face : definition.m_faces)
            {
                if (face.m_firstVertex >= definition.m_vertices.size()
                    || face.m_secondVertex >= definition.m_vertices.size()
                    || face.m_thirdVertex >= definition.m_vertices.size()
                    || !skinnedVertices[face.m_firstVertex]
                    || !skinnedVertices[face.m_secondVertex]
                    || !skinnedVertices[face.m_thirdVertex])
                {
                    continue;
                }

                const AZ::Vector3& first = definition.m_vertices[face.m_firstVertex].m_position;
                const AZ::Vector3& second = definition.m_vertices[face.m_secondVertex].m_position;
                const AZ::Vector3& third = definition.m_vertices[face.m_thirdVertex].m_position;
                const AZ::Vector3 faceNormal = (second - first).Cross(third - first).GetNormalizedSafe();
                normals[face.m_firstVertex] += faceNormal;
                normals[face.m_secondVertex] += faceNormal;
                normals[face.m_thirdVertex] += faceNormal;
            }

            for (AZ::Vector3& normal : normals)
            {
                normal.NormalizeSafe();
            }
        }

        bool GetJointPosition(
            const SoftBodyInverseBind& inverseBind,
            AZ::Vector3& jointPosition)
        {
            const float uniformScale = inverseBind.m_transform.GetUniformScale();
            if (!AZ::IsFiniteFloat(uniformScale)
                || AZ::IsClose(uniformScale, 0.0f))
            {
                return false;
            }

            jointPosition = inverseBind.m_transform.GetInverse().GetTranslation();
            return jointPosition.IsFinite();
        }

        void AddSphereToBounds(
            AZ::Aabb& bounds,
            const AZ::Vector3& center,
            const float radius)
        {
            const AZ::Vector3 extents(radius);
            bounds.AddPoint(center - extents);
            bounds.AddPoint(center + extents);
        }
    } // namespace

    SoftBodyComponent::SoftBodyComponent() = default;

    SoftBodyComponent::SoftBodyComponent(SoftBodyComponentConfiguration configuration)
        : m_configuration(AZStd::move(configuration))
    {
    }

    void SoftBodyComponent::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            SoftBodyComponentConfiguration::Reflect(serializeContext);

            serializeContext
                ->Class<SoftBodyComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Field("Configuration", &SoftBodyComponent::m_configuration);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<SoftBodyComponent>(
                        QT_TRANSLATE_NOOP("Jolt", "Jolt Soft Body"),
                        QT_TRANSLATE_NOOP("Jolt", "Authors deformable geometry and solver constraints."))
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Jolt")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &SoftBodyComponent::m_configuration,
                        QT_TRANSLATE_NOOP("Jolt", "Configuration"),
                        QT_TRANSLATE_NOOP("Jolt", "Geometry, constraints, materials, and simulation properties."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void SoftBodyComponent::GetProvidedServices(
        AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Jolt::SoftBodyComponent::GetProvidedServices(provided);
    }

    void SoftBodyComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Jolt::SoftBodyComponent::GetIncompatibleServices(incompatible);
    }

    void SoftBodyComponent::GetRequiredServices(
        AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Jolt::SoftBodyComponent::GetRequiredServices(required);
    }

    void SoftBodyComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
    }

    void SoftBodyComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void SoftBodyComponent::BuildGameEntity(
        AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<Jolt::SoftBodyComponent>(m_configuration);
    }

    void SoftBodyComponent::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        const AZ::Transform worldTransform = GetWorldTM();
        const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromTransform(worldTransform);
        const float uniformScale = AZStd::abs(worldTransform.GetUniformScale());
        const SoftBodyDefinitionConfiguration& definition = m_configuration.m_definition;

        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        for (const SoftBodyFace& face : definition.m_faces)
        {
            if (face.m_firstVertex >= definition.m_vertices.size()
                || face.m_secondVertex >= definition.m_vertices.size()
                || face.m_thirdVertex >= definition.m_vertices.size())
            {
                continue;
            }

            const AZ::Vector3 first = transform.TransformPoint(definition.m_vertices[face.m_firstVertex].m_position);
            const AZ::Vector3 second = transform.TransformPoint(definition.m_vertices[face.m_secondVertex].m_position);
            const AZ::Vector3 third = transform.TransformPoint(definition.m_vertices[face.m_thirdVertex].m_position);
            debugDisplay.DrawLine(first, second);
            debugDisplay.DrawLine(second, third);
            debugDisplay.DrawLine(third, first);
        }

        CalculateSkinNormals(definition, m_skinNormalScratch, m_skinnedVertexScratch);
        debugDisplay.SetColor(AZ::Colors::Orange);
        for (const SoftBodyInverseBind& inverseBind : definition.m_inverseBinds)
        {
            AZ::Vector3 jointPosition;
            if (GetJointPosition(inverseBind, jointPosition))
            {
                debugDisplay.DrawWireSphere(
                    transform.TransformPoint(jointPosition),
                    SkinJointRadius * uniformScale);
            }
        }

        for (const SoftBodySkinConstraint& constraint : definition.m_skinConstraints)
        {
            if (constraint.m_vertex >= definition.m_vertices.size())
            {
                continue;
            }

            const AZ::Vector3& vertex = definition.m_vertices[constraint.m_vertex].m_position;
            const AZ::Vector3 worldVertex = transform.TransformPoint(vertex);
            debugDisplay.SetColor(AZ::Colors::Cyan);
            for (const SoftBodySkinWeight& weight : constraint.m_weights)
            {
                if (weight.m_weight <= 0.0f
                    || weight.m_inverseBindIndex >= definition.m_inverseBinds.size())
                {
                    continue;
                }

                AZ::Vector3 jointPosition;
                if (GetJointPosition(definition.m_inverseBinds[weight.m_inverseBindIndex], jointPosition))
                {
                    debugDisplay.DrawLine(worldVertex, transform.TransformPoint(jointPosition));
                }
            }

            if (constraint.m_maximumDistance < AZStd::numeric_limits<float>::max())
            {
                debugDisplay.SetColor(AZ::Colors::Green);
                debugDisplay.DrawWireSphere(worldVertex, constraint.m_maximumDistance * uniformScale);
            }

            if (constraint.m_backstopDistance < constraint.m_maximumDistance
                && constraint.m_backstopRadius > 0.0f)
            {
                const AZ::Vector3 center =
                    vertex - m_skinNormalScratch[constraint.m_vertex]
                    * (constraint.m_backstopDistance + constraint.m_backstopRadius);
                debugDisplay.SetColor(AZ::Colors::Red);
                debugDisplay.DrawWireSphere(
                    transform.TransformPoint(center),
                    constraint.m_backstopRadius * uniformScale);
            }
        }
    }

    bool SoftBodyComponent::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb SoftBodyComponent::GetEditorSelectionBoundsViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        AZ::Aabb bounds = AZ::Aabb::CreateNull();
        const AZ::Transform worldTransform = GetWorldTM();
        const AZ::Matrix3x4 transform = AZ::Matrix3x4::CreateFromTransform(worldTransform);
        const float uniformScale = AZStd::abs(worldTransform.GetUniformScale());
        const SoftBodyDefinitionConfiguration& definition = m_configuration.m_definition;
        for (const SoftBodyVertex& vertex : definition.m_vertices)
        {
            bounds.AddPoint(transform.TransformPoint(vertex.m_position));
        }

        CalculateSkinNormals(definition, m_skinNormalScratch, m_skinnedVertexScratch);
        for (const SoftBodyInverseBind& inverseBind : definition.m_inverseBinds)
        {
            AZ::Vector3 jointPosition;
            if (GetJointPosition(inverseBind, jointPosition))
            {
                AddSphereToBounds(
                    bounds,
                    transform.TransformPoint(jointPosition),
                    SkinJointRadius * uniformScale);
            }
        }

        for (const SoftBodySkinConstraint& constraint : definition.m_skinConstraints)
        {
            if (constraint.m_vertex >= definition.m_vertices.size())
            {
                continue;
            }

            const AZ::Vector3& vertex = definition.m_vertices[constraint.m_vertex].m_position;
            const AZ::Vector3 worldVertex = transform.TransformPoint(vertex);
            if (constraint.m_maximumDistance < AZStd::numeric_limits<float>::max())
            {
                AddSphereToBounds(
                    bounds,
                    worldVertex,
                    constraint.m_maximumDistance * uniformScale);
            }

            if (constraint.m_backstopDistance < constraint.m_maximumDistance
                && constraint.m_backstopRadius > 0.0f)
            {
                const AZ::Vector3 center =
                    vertex - m_skinNormalScratch[constraint.m_vertex]
                    * (constraint.m_backstopDistance + constraint.m_backstopRadius);
                AddSphereToBounds(
                    bounds,
                    transform.TransformPoint(center),
                    constraint.m_backstopRadius * uniformScale);
            }
        }
        return bounds;
    }

    bool SoftBodyComponent::EditorSelectionIntersectRayViewport(
        const AzFramework::ViewportInfo& viewportInfo,
        const AZ::Vector3& rayStart,
        const AZ::Vector3& rayDirection,
        float& distance)
    {
        return IntersectEditorBounds(
            GetEditorSelectionBoundsViewport(viewportInfo),
            rayStart,
            rayDirection,
            distance);
    }
} // namespace Jolt::Editor
