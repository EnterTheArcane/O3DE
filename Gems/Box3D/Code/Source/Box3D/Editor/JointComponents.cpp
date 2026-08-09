/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Box3D/Editor/JointComponents.h>

#include <Box3D/Editor/DebugDraw.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/array.h>
#include <AzFramework/Translation/TranslationDef.h>
#include <AzFramework/Viewport/ViewportColors.h>
#include <AzToolsFramework/ComponentMode/EditorBaseComponentMode.h>
#include <AzToolsFramework/Manipulators/AngularManipulator.h>
#include <AzToolsFramework/Manipulators/ManipulatorManager.h>
#include <AzToolsFramework/Manipulators/ManipulatorView.h>
#include <AzToolsFramework/Manipulators/TranslationManipulators.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#include <AzToolsFramework/ViewportUi/ViewportUiRequestBus.h>
#include <Box3D/JointComponent.h>

namespace Box3D::Editor
{
    namespace ComponentModes
    {
        class JointFrameComponentMode final
            : public AzToolsFramework::ComponentModeFramework::EditorBaseComponentMode
        {
        public:
            AZ_CLASS_ALLOCATOR(JointFrameComponentMode, AZ::SystemAllocator);
            AZ_RTTI(
                JointFrameComponentMode,
                "{A398A96D-952F-4D79-B35D-00ED402647DF}",
                AzToolsFramework::ComponentModeFramework::EditorBaseComponentMode);

            JointFrameComponentMode(const AZ::EntityComponentIdPair& entityComponentIdPair, const AZ::Uuid componentType)
                : EditorBaseComponentMode(entityComponentIdPair, componentType)
                , m_translationManipulators(
                      AzToolsFramework::TranslationManipulators::Dimensions::Three,
                      AZ::Transform::CreateIdentity(),
                      AZ::Vector3::CreateOne())
            {
                AzToolsFramework::ViewportUi::ViewportUiRequestBus::EventResult(
                    m_clusterId,
                    AzToolsFramework::ViewportUi::DefaultViewportId,
                    &AzToolsFramework::ViewportUi::ViewportUiRequestBus::Events::CreateCluster,
                    AzToolsFramework::ViewportUi::Alignment::TopLeft);

                const auto createButton = [this](const char* tooltip)
                {
                    AzToolsFramework::ViewportUi::ButtonId buttonId;
                    AzToolsFramework::ViewportUi::ViewportUiRequestBus::EventResult(
                        buttonId,
                        AzToolsFramework::ViewportUi::DefaultViewportId,
                        &AzToolsFramework::ViewportUi::ViewportUiRequestBus::Events::CreateClusterButton,
                        m_clusterId,
                        AZStd::string(":/stylesheet/img/UI20/toolbar/Move.svg"));
                    AzToolsFramework::ViewportUi::ViewportUiRequestBus::Event(
                        AzToolsFramework::ViewportUi::DefaultViewportId,
                        &AzToolsFramework::ViewportUi::ViewportUiRequestBus::Events::SetClusterButtonTooltip,
                        m_clusterId,
                        buttonId,
                        tooltip);
                    return buttonId;
                };
                m_frameButtons[static_cast<AZ::u8>(JointFrame::Parent)] = createButton("Edit the parent joint frame");
                m_frameButtons[static_cast<AZ::u8>(JointFrame::Child)] = createButton("Edit the child joint frame");
                m_frameSelectionHandler = AZ::Event<AzToolsFramework::ViewportUi::ButtonId>::Handler(
                    [this](const AzToolsFramework::ViewportUi::ButtonId buttonId)
                    {
                        if (buttonId == m_frameButtons[static_cast<AZ::u8>(JointFrame::Parent)])
                        {
                            SetFrame(JointFrame::Parent);
                        }
                        else if (buttonId == m_frameButtons[static_cast<AZ::u8>(JointFrame::Child)])
                        {
                            SetFrame(JointFrame::Child);
                        }
                    });
                AzToolsFramework::ViewportUi::ViewportUiRequestBus::Event(
                    AzToolsFramework::ViewportUi::DefaultViewportId,
                    &AzToolsFramework::ViewportUi::ViewportUiRequestBus::Events::RegisterClusterEventHandler,
                    m_clusterId,
                    m_frameSelectionHandler);
                SetFrame(JointFrame::Child);
            }

            ~JointFrameComponentMode() override
            {
                TeardownManipulators();
                if (m_clusterId != AzToolsFramework::ViewportUi::InvalidClusterId)
                {
                    AzToolsFramework::ViewportUi::ViewportUiRequestBus::Event(
                        AzToolsFramework::ViewportUi::DefaultViewportId,
                        &AzToolsFramework::ViewportUi::ViewportUiRequestBus::Events::RemoveCluster,
                        m_clusterId);
                }
            }

            void Refresh() override
            {
                AZ::Transform localFrame = AZ::Transform::CreateIdentity();
                AZ::Transform frameSpace = AZ::Transform::CreateIdentity();
                JointManipulatorRequestBus::EventResult(
                    localFrame, GetEntityComponentIdPair(), &JointManipulatorRequestBus::Events::GetLocalFrame, m_frame);
                JointManipulatorRequestBus::EventResult(
                    frameSpace, GetEntityComponentIdPair(), &JointManipulatorRequestBus::Events::GetFrameSpace, m_frame);
                m_translationManipulators.SetSpace(frameSpace);
                m_translationManipulators.SetLocalTransform(localFrame);
                m_translationManipulators.SetBoundsDirty();
                for (const AZStd::shared_ptr<AzToolsFramework::AngularManipulator>& manipulator : m_rotationManipulators)
                {
                    manipulator->SetSpace(frameSpace);
                    manipulator->SetLocalTransform(localFrame);
                    manipulator->SetBoundsDirty();
                }
            }

            bool HandleMouseInteraction(
                [[maybe_unused]] const AzToolsFramework::ViewportInteraction::MouseInteractionEvent& mouseInteraction) override
            {
                return false;
            }

            AZStd::vector<AzToolsFramework::ViewportUi::ClusterId> PopulateViewportUiImpl() override
            {
                return { m_clusterId };
            }

            AZStd::string GetComponentModeName() const override
            {
                return "Joint Frame Edit Mode";
            }

            AZ::Uuid GetComponentModeType() const override
            {
                return azrtti_typeid<JointFrameComponentMode>();
            }

        private:
            void SetFrame(const JointFrame frame)
            {
                TeardownManipulators();
                m_frame = frame;
                SetupManipulators();
                AzToolsFramework::ViewportUi::ViewportUiRequestBus::Event(
                    AzToolsFramework::ViewportUi::DefaultViewportId,
                    &AzToolsFramework::ViewportUi::ViewportUiRequestBus::Events::SetClusterActiveButton,
                    m_clusterId,
                    m_frameButtons[static_cast<AZ::u8>(m_frame)]);
            }

            void SetupManipulators()
            {
                const AZ::EntityComponentIdPair pair = GetEntityComponentIdPair();
                AZ::Transform localFrame = AZ::Transform::CreateIdentity();
                AZ::Transform frameSpace = AZ::Transform::CreateIdentity();
                JointManipulatorRequestBus::EventResult(localFrame, pair, &JointManipulatorRequestBus::Events::GetLocalFrame, m_frame);
                JointManipulatorRequestBus::EventResult(frameSpace, pair, &JointManipulatorRequestBus::Events::GetFrameSpace, m_frame);

                m_translationManipulators.SetSpace(frameSpace);
                m_translationManipulators.SetLocalTransform(localFrame);
                m_translationManipulators.AddEntityComponentIdPair(pair);
                m_translationManipulators.Register(AzToolsFramework::GetMainManipulatorManagerId());
                AzToolsFramework::ConfigureTranslationManipulatorAppearance3d(&m_translationManipulators);
                const auto translate = [this, pair](const auto& action)
                {
                    AZ::Transform updatedFrame = AZ::Transform::CreateIdentity();
                    JointManipulatorRequestBus::EventResult(
                        updatedFrame, pair, &JointManipulatorRequestBus::Events::GetLocalFrame, m_frame);
                    updatedFrame.SetTranslation(action.LocalPosition());
                    JointManipulatorRequestBus::Event(pair, &JointManipulatorRequestBus::Events::SetLocalFrame, m_frame, updatedFrame);
                };
                m_translationManipulators.InstallLinearManipulatorMouseMoveCallback(translate);
                m_translationManipulators.InstallPlanarManipulatorMouseMoveCallback(translate);
                m_translationManipulators.InstallSurfaceManipulatorMouseMoveCallback(translate);

                const AZStd::array axes{ AZ::Vector3::CreateAxisX(), AZ::Vector3::CreateAxisY(), AZ::Vector3::CreateAxisZ() };
                const AZStd::array colors{ AZ::Colors::Red, AZ::Colors::Green, AZ::Colors::Blue };
                for (AZ::u32 index = 0; index < m_rotationManipulators.size(); ++index)
                {
                    AZStd::shared_ptr<AzToolsFramework::AngularManipulator>& manipulator = m_rotationManipulators[index];
                    manipulator = AzToolsFramework::AngularManipulator::MakeShared(frameSpace);
                    manipulator->AddEntityComponentIdPair(pair);
                    manipulator->SetAxis(axes[index]);
                    manipulator->SetLocalTransform(localFrame);
                    manipulator->SetView(
                        AzToolsFramework::CreateManipulatorViewCircle(
                            *manipulator,
                            colors[index],
                            2.0f,
                            AzToolsFramework::ManipulatorCicleBoundWidth(),
                            AzToolsFramework::DrawHalfDottedCircle));
                    manipulator->InstallLeftMouseDownCallback(
                        [this, pair]([[maybe_unused]] const AzToolsFramework::AngularManipulator::Action& action)
                        {
                            JointManipulatorRequestBus::EventResult(
                                m_rotationStart, pair, &JointManipulatorRequestBus::Events::GetLocalFrame, m_frame);
                        });
                    manipulator->InstallMouseMoveCallback(
                        [this, pair](const AzToolsFramework::AngularManipulator::Action& action)
                        {
                            const AZ::Transform updatedFrame =
                                m_rotationStart * AZ::Transform::CreateFromQuaternion(action.m_current.m_delta);
                            JointManipulatorRequestBus::Event(
                                pair, &JointManipulatorRequestBus::Events::SetLocalFrame, m_frame, updatedFrame);
                        });
                    manipulator->Register(AzToolsFramework::GetMainManipulatorManagerId());
                }
            }

            void TeardownManipulators()
            {
                if (m_translationManipulators.Registered())
                {
                    m_translationManipulators.RemoveEntityComponentIdPair(GetEntityComponentIdPair());
                    m_translationManipulators.Unregister();
                }
                for (AZStd::shared_ptr<AzToolsFramework::AngularManipulator>& manipulator : m_rotationManipulators)
                {
                    if (manipulator)
                    {
                        manipulator->RemoveEntityComponentIdPair(GetEntityComponentIdPair());
                        manipulator->Unregister();
                        manipulator.reset();
                    }
                }
            }

            JointFrame m_frame = JointFrame::Child;
            AzToolsFramework::TranslationManipulators m_translationManipulators;
            AZStd::array<AZStd::shared_ptr<AzToolsFramework::AngularManipulator>, 3> m_rotationManipulators;
            AZ::Transform m_rotationStart = AZ::Transform::CreateIdentity();
            AzToolsFramework::ViewportUi::ClusterId m_clusterId = AzToolsFramework::ViewportUi::InvalidClusterId;
            AZStd::array<AzToolsFramework::ViewportUi::ButtonId, 2> m_frameButtons;
            AZ::Event<AzToolsFramework::ViewportUi::ButtonId>::Handler m_frameSelectionHandler;
        };
    } // namespace ComponentModes

    void JointComponentBase::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
        const AZ::EntityComponentIdPair pair(GetEntityId(), GetId());
        JointManipulatorRequestBus::Handler::BusConnect(pair);

        const auto connect = [this, pair]<class Component>()
        {
            m_componentModeDelegate.Connect<Component>(pair, this);
        };
        if (azrtti_istypeof<ParallelJointComponent>(this))
        {
            connect.template operator()<ParallelJointComponent>();
        }
        else if (azrtti_istypeof<DistanceJointComponent>(this))
        {
            connect.template operator()<DistanceJointComponent>();
        }
        else if (azrtti_istypeof<FilterJointComponent>(this))
        {
            connect.template operator()<FilterJointComponent>();
        }
        else if (azrtti_istypeof<MotorJointComponent>(this))
        {
            connect.template operator()<MotorJointComponent>();
        }
        else if (azrtti_istypeof<PrismaticJointComponent>(this))
        {
            connect.template operator()<PrismaticJointComponent>();
        }
        else if (azrtti_istypeof<RevoluteJointComponent>(this))
        {
            connect.template operator()<RevoluteJointComponent>();
        }
        else if (azrtti_istypeof<SphericalJointComponent>(this))
        {
            connect.template operator()<SphericalJointComponent>();
        }
        else if (azrtti_istypeof<WeldJointComponent>(this))
        {
            connect.template operator()<WeldJointComponent>();
        }
        else if (azrtti_istypeof<WheelJointComponent>(this))
        {
            connect.template operator()<WheelJointComponent>();
        }

        const AZ::TypeId componentType = RTTI_GetType();
        m_componentModeDelegate.SetAddComponentModeCallback(
            [componentType](const AZ::EntityComponentIdPair& entityComponentIdPair)
            {
                const AzToolsFramework::ComponentModeFramework::ComponentModeBuilder builder(
                    entityComponentIdPair.GetComponentId(),
                    componentType,
                    [entityComponentIdPair, componentType]
                    {
                        return AZStd::make_unique<ComponentModes::JointFrameComponentMode>(entityComponentIdPair, componentType);
                    });
                AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
                    &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::AddComponentModes,
                    AzToolsFramework::ComponentModeFramework::EntityAndComponentModeBuilders(entityComponentIdPair.GetEntityId(), builder));
            });
    }

    void JointComponentBase::Deactivate()
    {
        m_componentModeDelegate.Disconnect();
        JointManipulatorRequestBus::Handler::BusDisconnect();
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    void JointComponentBase::DisplayEntityViewport(
        [[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo, AzFramework::DebugDisplayRequests& debugDisplay)
    {
        const JointCommonConfiguration& configuration = GetCommonConfiguration();
        const AZ::Transform childFrame = GetWorldTM() * configuration.m_childLocalFrame;
        AZ::Transform parentWorld = AZ::Transform::CreateIdentity();
        if (m_parentEntity.IsValid())
        {
            AZ::TransformBus::EventResult(parentWorld, m_parentEntity, &AZ::TransformInterface::GetWorldTM);
        }
        const AZ::Transform parentFrame = parentWorld * configuration.m_parentLocalFrame;
        const AZ::Vector3 parentPosition = parentFrame.GetTranslation();
        const AZ::Vector3 childPosition = childFrame.GetTranslation();
        const float drawScale = configuration.m_drawScale;

        debugDisplay.SetColor(AzFramework::ViewportColors::WireColor);
        debugDisplay.DrawLine(parentPosition, childPosition);
        debugDisplay.SetColor(AZ::Colors::Red);
        debugDisplay.DrawLine(childPosition, childPosition + drawScale * childFrame.GetBasisX());
        debugDisplay.SetColor(AZ::Colors::Green);
        debugDisplay.DrawLine(childPosition, childPosition + drawScale * childFrame.GetBasisY());
        debugDisplay.SetColor(AZ::Colors::Blue);
        debugDisplay.DrawLine(childPosition, childPosition + drawScale * childFrame.GetBasisZ());
    }

    bool JointComponentBase::SupportsEditorRayIntersect()
    {
        return true;
    }

    AZ::Aabb JointComponentBase::GetEditorSelectionBoundsViewport([[maybe_unused]] const AzFramework::ViewportInfo& viewportInfo)
    {
        const JointCommonConfiguration& configuration = GetCommonConfiguration();
        const AZ::Transform childFrame = GetWorldTM() * configuration.m_childLocalFrame;
        AZ::Transform parentWorld = AZ::Transform::CreateIdentity();
        if (m_parentEntity.IsValid())
        {
            AZ::TransformBus::EventResult(parentWorld, m_parentEntity, &AZ::TransformInterface::GetWorldTM);
        }

        AZ::Aabb bounds = AZ::Aabb::CreateNull();
        bounds.AddPoint((parentWorld * configuration.m_parentLocalFrame).GetTranslation());
        const AZ::Vector3 childPosition = childFrame.GetTranslation();
        bounds.AddPoint(childPosition);
        bounds.AddPoint(childPosition + configuration.m_drawScale * childFrame.GetBasisX());
        bounds.AddPoint(childPosition + configuration.m_drawScale * childFrame.GetBasisY());
        bounds.AddPoint(childPosition + configuration.m_drawScale * childFrame.GetBasisZ());
        bounds.Expand(AZ::Vector3(0.1f));
        return bounds;
    }

    bool JointComponentBase::EditorSelectionIntersectRayViewport(
        const AzFramework::ViewportInfo& viewportInfo, const AZ::Vector3& rayStart, const AZ::Vector3& rayDirection, float& distance)
    {
        return IntersectEditorBounds(GetEditorSelectionBoundsViewport(viewportInfo), rayStart, rayDirection, distance);
    }

    void JointComponentBase::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<JointComponentBase, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(2)
                ->Field("ParentEntity", &JointComponentBase::m_parentEntity)
                ->Field("ComponentMode", &JointComponentBase::m_componentModeDelegate);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<JointCommonConfiguration>("Joint", "Settings shared by every joint type.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &JointCommonConfiguration::m_parentLocalFrame,
                        QT_TRANSLATE_NOOP("Box3D", "Parent local frame"),
                        QT_TRANSLATE_NOOP("Box3D", "Joint frame relative to the parent body origin."))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &JointCommonConfiguration::m_childLocalFrame,
                        QT_TRANSLATE_NOOP("Box3D", "Child local frame"),
                        QT_TRANSLATE_NOOP("Box3D", "Joint frame relative to this body origin."))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &JointCommonConfiguration::m_forceThreshold,
                        QT_TRANSLATE_NOOP("Box3D", "Force threshold"),
                        QT_TRANSLATE_NOOP("Box3D", "Constraint force that raises the threshold event."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &JointCommonConfiguration::m_torqueThreshold,
                        QT_TRANSLATE_NOOP("Box3D", "Torque threshold"),
                        QT_TRANSLATE_NOOP("Box3D", "Constraint torque that raises the threshold event."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &JointCommonConfiguration::m_constraintHertz,
                        QT_TRANSLATE_NOOP("Box3D", "Constraint frequency"),
                        QT_TRANSLATE_NOOP("Box3D", "Frequency used to resolve constraint error."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &JointCommonConfiguration::m_constraintDampingRatio,
                        QT_TRANSLATE_NOOP("Box3D", "Constraint damping ratio"),
                        QT_TRANSLATE_NOOP("Box3D", "Damping applied while resolving constraint error."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &JointCommonConfiguration::m_drawScale,
                        QT_TRANSLATE_NOOP("Box3D", "Draw scale"),
                        QT_TRANSLATE_NOOP("Box3D", "Scale of the joint debug visualization."))
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &JointCommonConfiguration::m_collideConnected,
                        QT_TRANSLATE_NOOP("Box3D", "Collide connected"),
                        QT_TRANSLATE_NOOP("Box3D", "Allow contact generation between the connected bodies."));

                editContext->Class<JointComponentBase>("Joint", "Connects this rigid body to another rigid body.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &JointComponentBase::m_parentEntity,
                        QT_TRANSLATE_NOOP("Box3D", "Parent entity"),
                        QT_TRANSLATE_NOOP("Box3D", "Entity containing the other rigid body."))
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &JointComponentBase::m_componentModeDelegate,
                        QT_TRANSLATE_NOOP("Box3D", "Component mode"),
                        QT_TRANSLATE_NOOP("Box3D", "Edit the parent and child joint frames in the viewport."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void JointComponentBase::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        Box3D::JointComponent::GetProvidedServices(provided);
    }

    void JointComponentBase::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        Box3D::JointComponent::GetIncompatibleServices(incompatible);
    }

    void JointComponentBase::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        Box3D::JointComponent::GetRequiredServices(required);
    }

    void JointComponentBase::AddJointToGameEntity(AZ::Entity* gameEntity, const JointConfiguration& configuration) const
    {
        gameEntity->CreateComponent<Box3D::JointComponent>(configuration, m_parentEntity);
    }

    AZ::Transform JointComponentBase::GetLocalFrame(const JointFrame frame) const
    {
        const JointCommonConfiguration& configuration = GetCommonConfiguration();
        return frame == JointFrame::Parent ? configuration.m_parentLocalFrame : configuration.m_childLocalFrame;
    }

    void JointComponentBase::SetLocalFrame(const JointFrame frame, const AZ::Transform& localFrame)
    {
        if (!localFrame.IsFinite())
        {
            return;
        }
        JointCommonConfiguration& configuration = GetCommonConfiguration();
        (frame == JointFrame::Parent ? configuration.m_parentLocalFrame : configuration.m_childLocalFrame) = localFrame;
        SetDirty();
        AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequestBus::Broadcast(
            &AzToolsFramework::ComponentModeFramework::ComponentModeSystemRequests::Refresh,
            AZ::EntityComponentIdPair(GetEntityId(), GetId()));
        AzToolsFramework::ToolsApplicationNotificationBus::Broadcast(
            &AzToolsFramework::ToolsApplicationNotificationBus::Events::InvalidatePropertyDisplayForComponent,
            AZ::EntityComponentIdPair(GetEntityId(), GetId()),
            AzToolsFramework::Refresh_Values);
    }

    AZ::Transform JointComponentBase::GetFrameSpace(const JointFrame frame) const
    {
        AZ::Transform frameSpace = AZ::Transform::CreateIdentity();
        if (frame == JointFrame::Child)
        {
            frameSpace = GetWorldTM();
        }
        else if (m_parentEntity.IsValid())
        {
            AZ::TransformBus::EventResult(frameSpace, m_parentEntity, &AZ::TransformInterface::GetWorldTM);
        }
        frameSpace.ExtractUniformScale();
        return frameSpace;
    }

    void ReflectConfiguration(AZ::EditContext& editContext, ParallelJointConfiguration*)
    {
        editContext.Class<ParallelJointConfiguration>("Parallel Joint Configuration", "Parallel frame constraint settings.")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &ParallelJointConfiguration::m_common, "Joint", "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &ParallelJointConfiguration::m_hertz,
                QT_TRANSLATE_NOOP("Box3D", "Spring frequency"),
                QT_TRANSLATE_NOOP("Box3D", "Frequency used to align the joint frames."))
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &ParallelJointConfiguration::m_dampingRatio,
                QT_TRANSLATE_NOOP("Box3D", "Damping ratio"),
                QT_TRANSLATE_NOOP("Box3D", "Damping applied while aligning the frames."))
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &ParallelJointConfiguration::m_maxTorque,
                QT_TRANSLATE_NOOP("Box3D", "Maximum torque"),
                QT_TRANSLATE_NOOP("Box3D", "Maximum torque used to align the frames."))
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f);
    }

    void ReflectConfiguration(AZ::EditContext& editContext, DistanceJointConfiguration*)
    {
        editContext.Class<DistanceJointConfiguration>("Distance Joint Configuration", "Distance constraint settings.")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &DistanceJointConfiguration::m_common, "Joint", "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(AZ::Edit::UIHandlers::Default, &DistanceJointConfiguration::m_length, "Length", "Target anchor distance.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &DistanceJointConfiguration::m_enableSpring,
                "Enable spring",
                "Use a spring to reach the target length.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &DistanceJointConfiguration::m_lowerSpringForce,
                "Lower spring force",
                "Minimum spring force.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &DistanceJointConfiguration::m_upperSpringForce,
                "Upper spring force",
                "Maximum spring force.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &DistanceJointConfiguration::m_hertz, "Spring frequency", "Spring response frequency.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &DistanceJointConfiguration::m_dampingRatio, "Spring damping ratio", "Spring damping.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &DistanceJointConfiguration::m_enableLimit, "Enable limit", "Restrict the anchor distance.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &DistanceJointConfiguration::m_minLength,
                "Minimum length",
                "Shortest allowed anchor distance.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &DistanceJointConfiguration::m_maxLength,
                "Maximum length",
                "Longest allowed anchor distance.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &DistanceJointConfiguration::m_enableMotor,
                "Enable motor",
                "Drive the distance at the requested speed.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &DistanceJointConfiguration::m_maxMotorForce,
                "Maximum motor force",
                "Maximum force applied by the motor.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &DistanceJointConfiguration::m_motorSpeed,
                "Motor speed",
                "Requested change in distance per second.");
    }

    void ReflectConfiguration(AZ::EditContext& editContext, FilterJointConfiguration*)
    {
        editContext.Class<FilterJointConfiguration>("Filter Joint Configuration", "Collision filtering between connected bodies.")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &FilterJointConfiguration::m_common, "Joint", "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
    }

    void ReflectConfiguration(AZ::EditContext& editContext, MotorJointConfiguration*)
    {
        editContext.Class<MotorJointConfiguration>("Motor Joint Configuration", "Relative velocity and spring settings.")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &MotorJointConfiguration::m_common, "Joint", "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &MotorJointConfiguration::m_linearVelocity,
                "Linear velocity",
                "Target relative linear velocity.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &MotorJointConfiguration::m_angularVelocity,
                "Angular velocity",
                "Target relative angular velocity in radians per second.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &MotorJointConfiguration::m_maxVelocityForce,
                "Maximum velocity force",
                "Maximum force used to reach linear velocity.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &MotorJointConfiguration::m_maxVelocityTorque,
                "Maximum velocity torque",
                "Maximum torque used to reach angular velocity.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &MotorJointConfiguration::m_linearHertz,
                "Linear spring frequency",
                "Linear position spring frequency.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &MotorJointConfiguration::m_linearDampingRatio,
                "Linear damping ratio",
                "Linear position spring damping.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &MotorJointConfiguration::m_maxSpringForce,
                "Maximum spring force",
                "Maximum linear spring force.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &MotorJointConfiguration::m_angularHertz,
                "Angular spring frequency",
                "Angular position spring frequency.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &MotorJointConfiguration::m_angularDampingRatio,
                "Angular damping ratio",
                "Angular position spring damping.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &MotorJointConfiguration::m_maxSpringTorque,
                "Maximum spring torque",
                "Maximum angular spring torque.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f);
    }

    void ReflectConfiguration(AZ::EditContext& editContext, PrismaticJointConfiguration*)
    {
        editContext.Class<PrismaticJointConfiguration>("Prismatic Joint Configuration", "Single-axis translation settings.")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &PrismaticJointConfiguration::m_common, "Joint", "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &PrismaticJointConfiguration::m_enableSpring,
                "Enable spring",
                "Drive toward the target translation with a spring.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &PrismaticJointConfiguration::m_targetTranslation,
                "Target translation",
                "Spring target along the joint axis.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &PrismaticJointConfiguration::m_hertz, "Spring frequency", "Translation spring frequency.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &PrismaticJointConfiguration::m_dampingRatio,
                "Spring damping ratio",
                "Translation spring damping.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &PrismaticJointConfiguration::m_enableLimit,
                "Enable limit",
                "Restrict translation along the joint axis.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &PrismaticJointConfiguration::m_lowerTranslation,
                "Lower translation",
                "Minimum translation along the joint axis.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &PrismaticJointConfiguration::m_upperTranslation,
                "Upper translation",
                "Maximum translation along the joint axis.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &PrismaticJointConfiguration::m_enableMotor,
                "Enable motor",
                "Drive translation along the joint axis.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &PrismaticJointConfiguration::m_maxMotorForce,
                "Maximum motor force",
                "Maximum force applied by the motor.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &PrismaticJointConfiguration::m_motorSpeed, "Motor speed", "Target translation speed.");
    }

    void ReflectConfiguration(AZ::EditContext& editContext, RevoluteJointConfiguration*)
    {
        editContext.Class<RevoluteJointConfiguration>("Revolute Joint Configuration", "Single-axis rotation settings.")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &RevoluteJointConfiguration::m_common, "Joint", "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &RevoluteJointConfiguration::m_enableSpring,
                "Enable spring",
                "Drive toward the target angle with a spring.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &RevoluteJointConfiguration::m_targetAngle,
                "Target angle",
                "Spring target angle in radians.")
            ->Attribute(AZ::Edit::Attributes::Suffix, " rad")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &RevoluteJointConfiguration::m_hertz, "Spring frequency", "Angular spring frequency.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &RevoluteJointConfiguration::m_dampingRatio,
                "Spring damping ratio",
                "Angular spring damping.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &RevoluteJointConfiguration::m_enableLimit,
                "Enable limit",
                "Restrict rotation around the joint axis.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &RevoluteJointConfiguration::m_lowerAngle, "Lower angle", "Minimum angle in radians.")
            ->Attribute(AZ::Edit::Attributes::Suffix, " rad")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &RevoluteJointConfiguration::m_upperAngle, "Upper angle", "Maximum angle in radians.")
            ->Attribute(AZ::Edit::Attributes::Suffix, " rad")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &RevoluteJointConfiguration::m_enableMotor,
                "Enable motor",
                "Drive rotation around the joint axis.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &RevoluteJointConfiguration::m_maxMotorTorque,
                "Maximum motor torque",
                "Maximum torque applied by the motor.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &RevoluteJointConfiguration::m_motorSpeed,
                "Motor speed",
                "Target angular speed in radians per second.")
            ->Attribute(AZ::Edit::Attributes::Suffix, " rad/s");
    }

    void ReflectConfiguration(AZ::EditContext& editContext, SphericalJointConfiguration*)
    {
        editContext.Class<SphericalJointConfiguration>("Spherical Joint Configuration", "Swing, twist, spring, and motor settings.")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &SphericalJointConfiguration::m_common, "Joint", "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &SphericalJointConfiguration::m_enableSpring,
                "Enable spring",
                "Drive toward the target rotation with a spring.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &SphericalJointConfiguration::m_targetRotation,
                "Target rotation",
                "Spring target relative rotation.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &SphericalJointConfiguration::m_hertz, "Spring frequency", "Rotation spring frequency.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &SphericalJointConfiguration::m_dampingRatio,
                "Spring damping ratio",
                "Rotation spring damping.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &SphericalJointConfiguration::m_enableConeLimit,
                "Enable cone limit",
                "Restrict swing away from the joint axis.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &SphericalJointConfiguration::m_coneAngle, "Cone angle", "Maximum swing angle in radians.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Max, SphericalJointConfiguration::MaximumConeAngle)
            ->Attribute(AZ::Edit::Attributes::Suffix, " rad")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &SphericalJointConfiguration::m_enableTwistLimit,
                "Enable twist limit",
                "Restrict rotation around the joint axis.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &SphericalJointConfiguration::m_lowerTwistAngle,
                "Lower twist angle",
                "Minimum twist angle in radians.")
            ->Attribute(AZ::Edit::Attributes::Suffix, " rad")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &SphericalJointConfiguration::m_upperTwistAngle,
                "Upper twist angle",
                "Maximum twist angle in radians.")
            ->Attribute(AZ::Edit::Attributes::Suffix, " rad")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &SphericalJointConfiguration::m_enableMotor,
                "Enable motor",
                "Drive relative angular velocity.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &SphericalJointConfiguration::m_motorVelocity,
                "Motor velocity",
                "Target angular velocity in radians per second.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &SphericalJointConfiguration::m_maxMotorTorque,
                "Maximum motor torque",
                "Maximum torque applied by the motor.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f);
    }

    void ReflectConfiguration(AZ::EditContext& editContext, WeldJointConfiguration*)
    {
        editContext.Class<WeldJointConfiguration>("Weld Joint Configuration", "Linear and angular weld settings.")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &WeldJointConfiguration::m_common, "Joint", "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &WeldJointConfiguration::m_linearHertz, "Linear frequency", "Linear correction frequency.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WeldJointConfiguration::m_linearDampingRatio,
                "Linear damping ratio",
                "Linear correction damping.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WeldJointConfiguration::m_angularHertz,
                "Angular frequency",
                "Angular correction frequency.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WeldJointConfiguration::m_angularDampingRatio,
                "Angular damping ratio",
                "Angular correction damping.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f);
    }

    void ReflectConfiguration(AZ::EditContext& editContext, WheelJointConfiguration*)
    {
        editContext.Class<WheelJointConfiguration>("Wheel Joint Configuration", "Suspension, wheel spin, and steering settings.")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &WheelJointConfiguration::m_common, "Joint", "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_enableSuspensionSpring,
                "Enable suspension spring",
                "Apply spring forces along the suspension axis.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_suspensionHertz,
                "Suspension frequency",
                "Suspension spring frequency.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_suspensionDampingRatio,
                "Suspension damping ratio",
                "Suspension spring damping.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_enableSuspensionLimit,
                "Enable suspension limit",
                "Restrict suspension travel.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_lowerSuspensionLimit,
                "Lower suspension limit",
                "Minimum suspension translation.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_upperSuspensionLimit,
                "Upper suspension limit",
                "Maximum suspension translation.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &WheelJointConfiguration::m_enableSpinMotor, "Enable spin motor", "Drive wheel spin.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_spinSpeed,
                "Spin speed",
                "Target wheel speed in radians per second.")
            ->Attribute(AZ::Edit::Attributes::Suffix, " rad/s")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_maxSpinTorque,
                "Maximum spin torque",
                "Maximum wheel motor torque.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &WheelJointConfiguration::m_enableSteering, "Enable steering", "Drive wheel steering.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_targetSteeringAngle,
                "Target steering angle",
                "Target steering angle in radians.")
            ->Attribute(AZ::Edit::Attributes::Suffix, " rad")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_steeringHertz,
                "Steering frequency",
                "Steering spring frequency.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->Attribute(AZ::Edit::Attributes::Suffix, " Hz")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_steeringDampingRatio,
                "Steering damping ratio",
                "Steering spring damping.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_maxSteeringTorque,
                "Maximum steering torque",
                "Maximum steering motor torque.")
            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_enableSteeringLimit,
                "Enable steering limit",
                "Restrict steering angle.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_lowerSteeringLimit,
                "Lower steering limit",
                "Minimum steering angle in radians.")
            ->Attribute(AZ::Edit::Attributes::Suffix, " rad")
            ->DataElement(
                AZ::Edit::UIHandlers::Default,
                &WheelJointConfiguration::m_upperSteeringLimit,
                "Upper steering limit",
                "Maximum steering angle in radians.")
            ->Attribute(AZ::Edit::Attributes::Suffix, " rad");
    }

    template<class Component, class Configuration>
    void ReflectJointComponent(
        AZ::ReflectContext* context, Configuration Component::* configuration, const char* name, const char* description)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Component, JointComponentBase>()->Version(1)->Field("Configuration", configuration);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                ReflectConfiguration(*editContext, static_cast<Configuration*>(nullptr));
                editContext->Class<Component>(name, description)
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Box3D")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        configuration,
                        QT_TRANSLATE_NOOP("Box3D", "Configuration"),
                        QT_TRANSLATE_NOOP("Box3D", "Constraint, limits, springs, and motors."))
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void ParallelJointComponent::Reflect(AZ::ReflectContext* context)
    {
        JointComponentBase::Reflect(context);
        ReflectJointComponent(
            context,
            &ParallelJointComponent::m_configuration,
            QT_TRANSLATE_NOOP("Box3D", "Box3D Parallel Joint"),
            QT_TRANSLATE_NOOP("Box3D", "Keeps the bodies' joint frames parallel."));
    }

    void ParallelJointComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        AddJointToGameEntity(gameEntity, m_configuration);
    }

    JointCommonConfiguration& ParallelJointComponent::GetCommonConfiguration()
    {
        return m_configuration.m_common;
    }

    const JointCommonConfiguration& ParallelJointComponent::GetCommonConfiguration() const
    {
        return m_configuration.m_common;
    }

    void DistanceJointComponent::Reflect(AZ::ReflectContext* context)
    {
        ReflectJointComponent(
            context,
            &DistanceJointComponent::m_configuration,
            QT_TRANSLATE_NOOP("Box3D", "Box3D Distance Joint"),
            QT_TRANSLATE_NOOP("Box3D", "Constrains the distance between two joint anchors."));
    }

    void DistanceJointComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        AddJointToGameEntity(gameEntity, m_configuration);
    }

    JointCommonConfiguration& DistanceJointComponent::GetCommonConfiguration()
    {
        return m_configuration.m_common;
    }

    const JointCommonConfiguration& DistanceJointComponent::GetCommonConfiguration() const
    {
        return m_configuration.m_common;
    }

    void FilterJointComponent::Reflect(AZ::ReflectContext* context)
    {
        ReflectJointComponent(
            context,
            &FilterJointComponent::m_configuration,
            QT_TRANSLATE_NOOP("Box3D", "Box3D Filter Joint"),
            QT_TRANSLATE_NOOP("Box3D", "Suppresses collision between the connected bodies."));
    }

    void FilterJointComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        AddJointToGameEntity(gameEntity, m_configuration);
    }

    JointCommonConfiguration& FilterJointComponent::GetCommonConfiguration()
    {
        return m_configuration.m_common;
    }

    const JointCommonConfiguration& FilterJointComponent::GetCommonConfiguration() const
    {
        return m_configuration.m_common;
    }

    void MotorJointComponent::Reflect(AZ::ReflectContext* context)
    {
        ReflectJointComponent(
            context,
            &MotorJointComponent::m_configuration,
            QT_TRANSLATE_NOOP("Box3D", "Box3D Motor Joint"),
            QT_TRANSLATE_NOOP("Box3D", "Drives relative linear and angular motion."));
    }

    void MotorJointComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        AddJointToGameEntity(gameEntity, m_configuration);
    }

    JointCommonConfiguration& MotorJointComponent::GetCommonConfiguration()
    {
        return m_configuration.m_common;
    }

    const JointCommonConfiguration& MotorJointComponent::GetCommonConfiguration() const
    {
        return m_configuration.m_common;
    }

    void PrismaticJointComponent::Reflect(AZ::ReflectContext* context)
    {
        ReflectJointComponent(
            context,
            &PrismaticJointComponent::m_configuration,
            QT_TRANSLATE_NOOP("Box3D", "Box3D Prismatic Joint"),
            QT_TRANSLATE_NOOP("Box3D", "Allows translation along one axis."));
    }

    void PrismaticJointComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        AddJointToGameEntity(gameEntity, m_configuration);
    }

    JointCommonConfiguration& PrismaticJointComponent::GetCommonConfiguration()
    {
        return m_configuration.m_common;
    }

    const JointCommonConfiguration& PrismaticJointComponent::GetCommonConfiguration() const
    {
        return m_configuration.m_common;
    }

    void RevoluteJointComponent::Reflect(AZ::ReflectContext* context)
    {
        ReflectJointComponent(
            context,
            &RevoluteJointComponent::m_configuration,
            QT_TRANSLATE_NOOP("Box3D", "Box3D Revolute Joint"),
            QT_TRANSLATE_NOOP("Box3D", "Allows rotation around one axis."));
    }

    void RevoluteJointComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        AddJointToGameEntity(gameEntity, m_configuration);
    }

    JointCommonConfiguration& RevoluteJointComponent::GetCommonConfiguration()
    {
        return m_configuration.m_common;
    }

    const JointCommonConfiguration& RevoluteJointComponent::GetCommonConfiguration() const
    {
        return m_configuration.m_common;
    }

    void SphericalJointComponent::Reflect(AZ::ReflectContext* context)
    {
        ReflectJointComponent(
            context,
            &SphericalJointComponent::m_configuration,
            QT_TRANSLATE_NOOP("Box3D", "Box3D Spherical Joint"),
            QT_TRANSLATE_NOOP("Box3D", "Allows constrained swing and twist rotation."));
    }

    void SphericalJointComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        AddJointToGameEntity(gameEntity, m_configuration);
    }

    JointCommonConfiguration& SphericalJointComponent::GetCommonConfiguration()
    {
        return m_configuration.m_common;
    }

    const JointCommonConfiguration& SphericalJointComponent::GetCommonConfiguration() const
    {
        return m_configuration.m_common;
    }

    void WeldJointComponent::Reflect(AZ::ReflectContext* context)
    {
        ReflectJointComponent(
            context,
            &WeldJointComponent::m_configuration,
            QT_TRANSLATE_NOOP("Box3D", "Box3D Weld Joint"),
            QT_TRANSLATE_NOOP("Box3D", "Constrains all relative translation and rotation."));
    }

    void WeldJointComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        AddJointToGameEntity(gameEntity, m_configuration);
    }

    JointCommonConfiguration& WeldJointComponent::GetCommonConfiguration()
    {
        return m_configuration.m_common;
    }

    const JointCommonConfiguration& WeldJointComponent::GetCommonConfiguration() const
    {
        return m_configuration.m_common;
    }

    void WheelJointComponent::Reflect(AZ::ReflectContext* context)
    {
        ReflectJointComponent(
            context,
            &WheelJointComponent::m_configuration,
            QT_TRANSLATE_NOOP("Box3D", "Box3D Wheel Joint"),
            QT_TRANSLATE_NOOP("Box3D", "Combines suspension, wheel spin, and steering."));
    }

    void WheelJointComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        AddJointToGameEntity(gameEntity, m_configuration);
    }

    JointCommonConfiguration& WheelJointComponent::GetCommonConfiguration()
    {
        return m_configuration.m_common;
    }

    const JointCommonConfiguration& WheelJointComponent::GetCommonConfiguration() const
    {
        return m_configuration.m_common;
    }
} // namespace Box3D::Editor
