/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/World.h>

#include <Jolt/Internal/HandleEncoding.h>
#include <Jolt/Profiler.h>
#include <Jolt/SystemInternal.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/std/typetraits/is_same.h>
#include <AzCore/std/typetraits/remove_cvref.h>
#include <AzCore/std/containers/variant.h>
#include <AzCore/std/limits.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>

#include <cmath>

namespace Jolt
{
    namespace
    {
        [[nodiscard]]
        JPH::Vec3 ToRagdollNativeVector(
            const AZ::Vector3& value)
        {
            return {value.GetX(), value.GetY(), value.GetZ()};
        }

        [[nodiscard]]
        JPH::Vec3 ToRagdollNativeVector(
            const WorldPosition& value)
        {
            return {
                static_cast<float>(value.m_x),
                static_cast<float>(value.m_y),
                static_cast<float>(value.m_z),
            };
        }

        [[nodiscard]]
        AZ::Vector3 FromRagdollNativeVector(
            const JPH::Vec3Arg value)
        {
            return {value.GetX(), value.GetY(), value.GetZ()};
        }

        [[nodiscard]]
        JPH::RVec3 ToRagdollNativePosition(
            const WorldPosition& position,
            const WorldPosition& origin)
        {
            return {
                static_cast<JPH::Real>(position.m_x - origin.m_x),
                static_cast<JPH::Real>(position.m_y - origin.m_y),
                static_cast<JPH::Real>(position.m_z - origin.m_z),
            };
        }

        [[nodiscard]]
        WorldPosition FromRagdollNativePosition(
            const JPH::RVec3Arg position,
            const WorldPosition& origin)
        {
            return {
                .m_x = static_cast<double>(position.GetX()) + origin.m_x,
                .m_y = static_cast<double>(position.GetY()) + origin.m_y,
                .m_z = static_cast<double>(position.GetZ()) + origin.m_z,
            };
        }

        [[nodiscard]]
        JPH::Quat ToRagdollNativeRotation(
            const AZ::Quaternion& rotation)
        {
            const AZ::Quaternion normalized = rotation.GetNormalized();
            return {normalized.GetX(), normalized.GetY(), normalized.GetZ(), normalized.GetW()};
        }

        [[nodiscard]]
        AZ::Quaternion FromRagdollNativeRotation(
            const JPH::QuatArg rotation)
        {
            return AZ::Quaternion(rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW());
        }

        [[nodiscard]]
        JPH::Mat44 ToRagdollNativeTransform(
            const AZ::Transform& transform)
        {
            JPH::Mat44 result = JPH::Mat44::sIdentity();
            result.SetColumn3(0, ToRagdollNativeVector(transform.GetBasisX()));
            result.SetColumn3(1, ToRagdollNativeVector(transform.GetBasisY()));
            result.SetColumn3(2, ToRagdollNativeVector(transform.GetBasisZ()));
            result.SetTranslation(ToRagdollNativeVector(transform.GetTranslation()));
            return result;
        }

        [[nodiscard]]
        AZ::Transform FromRagdollNativeTransform(
            JPH::Mat44Arg transform)
        {
            const JPH::Vec3 basisX = transform.GetColumn3(0);
            const JPH::Vec3 basisY = transform.GetColumn3(1);
            const JPH::Vec3 basisZ = transform.GetColumn3(2);
            return AZ::Transform::CreateFromMatrix3x3AndTranslation(
                AZ::Matrix3x3::CreateFromColumns(
                    FromRagdollNativeVector(basisX),
                    FromRagdollNativeVector(basisY),
                    FromRagdollNativeVector(basisZ)),
                FromRagdollNativeVector(transform.GetTranslation()));
        }

        [[nodiscard]]
        bool IsValidRagdollPoseTransform(
            const AZ::Transform& transform)
        {
            return transform.IsFinite()
                && transform.IsOrthogonal()
                && AZ::IsClose(transform.GetUniformScale(), 1.0f, AZ::Constants::Tolerance);
        }

        [[nodiscard]]
        bool AreValidRagdollFrameAxes(
            const AZ::Vector3& primaryAxis,
            const AZ::Vector3& normalAxis)
        {
            return primaryAxis.IsFinite()
                && normalAxis.IsFinite()
                && !primaryAxis.IsZero()
                && !normalAxis.IsZero()
                && AZ::IsClose(
                    primaryAxis.GetNormalized().Dot(normalAxis.GetNormalized()),
                    0.0f,
                    1.0e-3f);
        }

        [[nodiscard]]
        bool IsValidRagdollSpring(
            const SpringConfiguration& spring)
        {
            return spring.m_mode != SpringMode::None
                && AZ::IsFiniteFloat(spring.m_frequencyOrStiffness)
                && spring.m_frequencyOrStiffness >= 0.0f
                && AZ::IsFiniteFloat(spring.m_damping)
                && spring.m_damping >= 0.0f;
        }

        [[nodiscard]]
        bool IsValidRagdollMotor(
            const MotorConfiguration& motor)
        {
            return motor.m_state != MotorState::None
                && IsValidRagdollSpring(motor.m_spring)
                && AZ::IsFiniteFloat(motor.m_maximumForce)
                && AZ::IsFiniteFloat(motor.m_maximumTorque)
                && AZ::IsFiniteFloat(motor.m_minimumForce)
                && AZ::IsFiniteFloat(motor.m_minimumTorque)
                && motor.m_minimumForce <= motor.m_maximumForce
                && motor.m_minimumTorque <= motor.m_maximumTorque;
        }

        [[nodiscard]]
        JPH::EMotionType ToRagdollNativeMotionType(
            const MotionType motionType)
        {
            if (motionType == MotionType::Kinematic)
            {
                return JPH::EMotionType::Kinematic;
            }

            return JPH::EMotionType::Dynamic;
        }

        [[nodiscard]]
        JPH::EOverrideMassProperties ToRagdollNativeMassPropertiesMode(
            const MassPropertiesMode mode)
        {
            if (mode == MassPropertiesMode::CalculateInertia)
            {
                return JPH::EOverrideMassProperties::CalculateInertia;
            }
            if (mode == MassPropertiesMode::Provided)
            {
                return JPH::EOverrideMassProperties::MassAndInertiaProvided;
            }

            return JPH::EOverrideMassProperties::CalculateMassAndInertia;
        }

        [[nodiscard]]
        JPH::Mat44 ToRagdollNativeInertia(
            const AZ::Matrix3x3& inertia)
        {
            JPH::Mat44 nativeInertia = JPH::Mat44::sZero();
            nativeInertia.SetColumn3(0, ToRagdollNativeVector(inertia.GetColumn(0)));
            nativeInertia.SetColumn3(1, ToRagdollNativeVector(inertia.GetColumn(1)));
            nativeInertia.SetColumn3(2, ToRagdollNativeVector(inertia.GetColumn(2)));
            return nativeInertia;
        }

        [[nodiscard]]
        JPH::SpringSettings ToRagdollNativeSpring(
            const SpringConfiguration& spring)
        {
            JPH::ESpringMode mode = JPH::ESpringMode::FrequencyAndDamping;
            if (spring.m_mode == SpringMode::MassNormalizedStiffnessAndDamping)
            {
                mode = JPH::ESpringMode::MassNormalizedStiffnessAndDamping;
            }
            else if (spring.m_mode == SpringMode::StiffnessAndDamping)
            {
                mode = JPH::ESpringMode::StiffnessAndDamping;
            }

            return {mode, spring.m_frequencyOrStiffness, spring.m_damping};
        }

        [[nodiscard]]
        JPH::MotorSettings ToRagdollNativeMotor(
            const MotorConfiguration& motor)
        {
            JPH::MotorSettings settings;
            settings.mSpringSettings = ToRagdollNativeSpring(motor.m_spring);
            settings.mMaxForceLimit = motor.m_maximumForce;
            settings.mMaxTorqueLimit = motor.m_maximumTorque;
            settings.mMinForceLimit = motor.m_minimumForce;
            settings.mMinTorqueLimit = motor.m_minimumTorque;
            return settings;
        }

        [[nodiscard]]
        bool IsFiniteRagdollPosition(
            const WorldPosition& position)
        {
            return std::isfinite(position.m_x)
                && std::isfinite(position.m_y)
                && std::isfinite(position.m_z);
        }
    } // namespace

    RagdollDefinitionHandle World::CreateRagdollDefinition(
        const RagdollDefinitionConfiguration& configuration)
    {
        AZStd::lock_guard lock(m_mutex);
        if (!configuration.m_skeletonHandle
            || configuration.m_parts.empty()
            || !AZ::IsFiniteFloat(configuration.m_minimumCollisionSeparation)
            || configuration.m_minimumCollisionSeparation < 0.0f)
        {
            return {};
        }

        JPH::Ref<JPH::Skeleton> skeleton;
        if (!m_system.AcquireSkeletonDefinition(configuration.m_skeletonHandle, skeleton))
        {
            return {};
        }
        if (configuration.m_parts.size() != skeleton->GetJointCount())
        {
            m_system.ReleaseSkeletonDefinition(configuration.m_skeletonHandle);
            return {};
        }

        JPH::Ref<JPH::RagdollSettings> settings = new JPH::RagdollSettings();
        settings->mSkeleton = skeleton;
        settings->mParts.resize(configuration.m_parts.size());
        AZStd::vector<ShapeHandle> shapeHandles;
        AZStd::vector<AZ::Transform> neutralTransforms;
        shapeHandles.reserve(configuration.m_parts.size());
        neutralTransforms.reserve(configuration.m_parts.size());
        bool supportsMotorDrive = true;
        bool valid = true;

        const auto makeConstraintSettings =
            [&](const ConstraintGeometry& geometry) -> JPH::Ref<JPH::TwoBodyConstraintSettings>
            {
                JPH::Ref<JPH::TwoBodyConstraintSettings> result;
                AZStd::visit(
                    [&](const auto& value)
                    {
                        using Geometry = AZStd::remove_cvref_t<decltype(value)>;
                        if constexpr (AZStd::is_same_v<Geometry, ConeConstraintConfiguration>)
                        {
                            if (value.m_space != ConstraintSpace::LocalToCenterOfMass
                                || !IsFiniteRagdollPosition(value.m_firstPoint)
                                || !IsFiniteRagdollPosition(value.m_secondPoint)
                                || !value.m_firstTwistAxis.IsFinite()
                                || value.m_firstTwistAxis.IsZero()
                                || !value.m_secondTwistAxis.IsFinite()
                                || value.m_secondTwistAxis.IsZero()
                                || !AZ::IsFiniteFloat(value.m_halfConeAngle)
                                || value.m_halfConeAngle < 0.0f
                                || value.m_halfConeAngle > AZ::Constants::Pi)
                            {
                                return;
                            }

                            JPH::Ref<JPH::ConeConstraintSettings> native = new JPH::ConeConstraintSettings();
                            native->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                            native->mPoint1 = ToRagdollNativeVector(value.m_firstPoint);
                            native->mPoint2 = ToRagdollNativeVector(value.m_secondPoint);
                            native->mTwistAxis1 = ToRagdollNativeVector(value.m_firstTwistAxis.GetNormalized());
                            native->mTwistAxis2 = ToRagdollNativeVector(value.m_secondTwistAxis.GetNormalized());
                            native->mHalfConeAngle = value.m_halfConeAngle;
                            result = native;
                        }
                        else if constexpr (AZStd::is_same_v<Geometry, DistanceConstraintConfiguration>)
                        {
                            const bool usesAutomaticDistance =
                                value.m_minimumDistance < 0.0f
                                && value.m_maximumDistance < 0.0f;
                            const bool distancesAreValid = usesAutomaticDistance
                                || (AZ::IsFiniteFloat(value.m_minimumDistance)
                                    && AZ::IsFiniteFloat(value.m_maximumDistance)
                                    && value.m_minimumDistance >= 0.0f
                                    && value.m_maximumDistance >= value.m_minimumDistance);
                            if (value.m_space != ConstraintSpace::LocalToCenterOfMass
                                || !IsFiniteRagdollPosition(value.m_firstPoint)
                                || !IsFiniteRagdollPosition(value.m_secondPoint)
                                || !distancesAreValid
                                || !IsValidRagdollSpring(value.m_limitSpring))
                            {
                                return;
                            }

                            JPH::Ref<JPH::DistanceConstraintSettings> native =
                                new JPH::DistanceConstraintSettings();
                            native->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                            native->mPoint1 = ToRagdollNativeVector(value.m_firstPoint);
                            native->mPoint2 = ToRagdollNativeVector(value.m_secondPoint);
                            native->mMinDistance = value.m_minimumDistance;
                            native->mMaxDistance = value.m_maximumDistance;
                            native->mLimitsSpringSettings = ToRagdollNativeSpring(value.m_limitSpring);
                            result = native;
                        }
                        else if constexpr (AZStd::is_same_v<Geometry, FixedConstraintConfiguration>)
                        {
                            if (value.m_space != ConstraintSpace::LocalToCenterOfMass
                                || !IsFiniteRagdollPosition(value.m_firstPoint)
                                || !IsFiniteRagdollPosition(value.m_secondPoint)
                                || !AreValidRagdollFrameAxes(value.m_firstAxisX, value.m_firstAxisY)
                                || !AreValidRagdollFrameAxes(value.m_secondAxisX, value.m_secondAxisY))
                            {
                                return;
                            }

                            JPH::Ref<JPH::FixedConstraintSettings> native = new JPH::FixedConstraintSettings();
                            native->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                            native->mAutoDetectPoint = value.m_autoDetectPoint;
                            native->mPoint1 = ToRagdollNativeVector(value.m_firstPoint);
                            native->mPoint2 = ToRagdollNativeVector(value.m_secondPoint);
                            native->mAxisX1 = ToRagdollNativeVector(value.m_firstAxisX.GetNormalized());
                            native->mAxisY1 = ToRagdollNativeVector(value.m_firstAxisY.GetNormalized());
                            native->mAxisX2 = ToRagdollNativeVector(value.m_secondAxisX.GetNormalized());
                            native->mAxisY2 = ToRagdollNativeVector(value.m_secondAxisY.GetNormalized());
                            result = native;
                        }
                        else if constexpr (AZStd::is_same_v<Geometry, HingeConstraintConfiguration>)
                        {
                            if (value.m_space != ConstraintSpace::LocalToCenterOfMass
                                || !IsFiniteRagdollPosition(value.m_firstPoint)
                                || !IsFiniteRagdollPosition(value.m_secondPoint)
                                || !AreValidRagdollFrameAxes(value.m_firstHingeAxis, value.m_firstNormalAxis)
                                || !AreValidRagdollFrameAxes(value.m_secondHingeAxis, value.m_secondNormalAxis)
                                || !IsValidRagdollSpring(value.m_limitSpring)
                                || !IsValidRagdollMotor(value.m_motor)
                                || !AZ::IsFiniteFloat(value.m_maximumFrictionTorque)
                                || value.m_maximumFrictionTorque < 0.0f
                                || !AZ::IsFiniteFloat(value.m_minimumLimit)
                                || value.m_minimumLimit < -AZ::Constants::Pi
                                || value.m_minimumLimit > 0.0f
                                || !AZ::IsFiniteFloat(value.m_maximumLimit)
                                || value.m_maximumLimit < 0.0f
                                || value.m_maximumLimit > AZ::Constants::Pi)
                            {
                                return;
                            }

                            JPH::Ref<JPH::HingeConstraintSettings> native = new JPH::HingeConstraintSettings();
                            native->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                            native->mPoint1 = ToRagdollNativeVector(value.m_firstPoint);
                            native->mPoint2 = ToRagdollNativeVector(value.m_secondPoint);
                            native->mHingeAxis1 = ToRagdollNativeVector(value.m_firstHingeAxis.GetNormalized());
                            native->mNormalAxis1 = ToRagdollNativeVector(value.m_firstNormalAxis.GetNormalized());
                            native->mHingeAxis2 = ToRagdollNativeVector(value.m_secondHingeAxis.GetNormalized());
                            native->mNormalAxis2 = ToRagdollNativeVector(value.m_secondNormalAxis.GetNormalized());
                            native->mLimitsMin = value.m_minimumLimit;
                            native->mLimitsMax = value.m_maximumLimit;
                            native->mLimitsSpringSettings = ToRagdollNativeSpring(value.m_limitSpring);
                            native->mMaxFrictionTorque = value.m_maximumFrictionTorque;
                            native->mMotorSettings = ToRagdollNativeMotor(value.m_motor);
                            result = native;
                        }
                        else if constexpr (AZStd::is_same_v<Geometry, PointConstraintConfiguration>)
                        {
                            if (value.m_space != ConstraintSpace::LocalToCenterOfMass
                                || !IsFiniteRagdollPosition(value.m_firstPoint)
                                || !IsFiniteRagdollPosition(value.m_secondPoint))
                            {
                                return;
                            }

                            JPH::Ref<JPH::PointConstraintSettings> native = new JPH::PointConstraintSettings();
                            native->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                            native->mPoint1 = ToRagdollNativeVector(value.m_firstPoint);
                            native->mPoint2 = ToRagdollNativeVector(value.m_secondPoint);
                            result = native;
                        }
                        else if constexpr (AZStd::is_same_v<Geometry, SliderConstraintConfiguration>)
                        {
                            if (value.m_space != ConstraintSpace::LocalToCenterOfMass
                                || !IsFiniteRagdollPosition(value.m_firstPoint)
                                || !IsFiniteRagdollPosition(value.m_secondPoint)
                                || !AreValidRagdollFrameAxes(value.m_firstSliderAxis, value.m_firstNormalAxis)
                                || !AreValidRagdollFrameAxes(value.m_secondSliderAxis, value.m_secondNormalAxis)
                                || !IsValidRagdollSpring(value.m_limitSpring)
                                || !IsValidRagdollMotor(value.m_motor)
                                || !AZ::IsFiniteFloat(value.m_maximumFrictionForce)
                                || value.m_maximumFrictionForce < 0.0f
                                || !AZ::IsFiniteFloat(value.m_minimumLimit)
                                || value.m_minimumLimit > 0.0f
                                || !AZ::IsFiniteFloat(value.m_maximumLimit)
                                || value.m_maximumLimit < 0.0f)
                            {
                                return;
                            }

                            JPH::Ref<JPH::SliderConstraintSettings> native = new JPH::SliderConstraintSettings();
                            native->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                            native->mAutoDetectPoint = value.m_autoDetectPoint;
                            native->mPoint1 = ToRagdollNativeVector(value.m_firstPoint);
                            native->mPoint2 = ToRagdollNativeVector(value.m_secondPoint);
                            native->mSliderAxis1 = ToRagdollNativeVector(value.m_firstSliderAxis.GetNormalized());
                            native->mNormalAxis1 = ToRagdollNativeVector(value.m_firstNormalAxis.GetNormalized());
                            native->mSliderAxis2 = ToRagdollNativeVector(value.m_secondSliderAxis.GetNormalized());
                            native->mNormalAxis2 = ToRagdollNativeVector(value.m_secondNormalAxis.GetNormalized());
                            native->mLimitsMin = value.m_minimumLimit;
                            native->mLimitsMax = value.m_maximumLimit;
                            native->mLimitsSpringSettings = ToRagdollNativeSpring(value.m_limitSpring);
                            native->mMaxFrictionForce = value.m_maximumFrictionForce;
                            native->mMotorSettings = ToRagdollNativeMotor(value.m_motor);
                            result = native;
                        }
                        else if constexpr (AZStd::is_same_v<Geometry, SwingTwistConstraintConfiguration>)
                        {
                            if (value.m_space != ConstraintSpace::LocalToCenterOfMass
                                || value.m_swingType == SwingType::None
                                || !IsFiniteRagdollPosition(value.m_firstPoint)
                                || !IsFiniteRagdollPosition(value.m_secondPoint)
                                || !AreValidRagdollFrameAxes(value.m_firstTwistAxis, value.m_firstPlaneAxis)
                                || !AreValidRagdollFrameAxes(value.m_secondTwistAxis, value.m_secondPlaneAxis)
                                || !IsValidRagdollMotor(value.m_swingMotor)
                                || !IsValidRagdollMotor(value.m_twistMotor)
                                || !AZ::IsFiniteFloat(value.m_maximumFrictionTorque)
                                || value.m_maximumFrictionTorque < 0.0f
                                || !AZ::IsFiniteFloat(value.m_normalHalfConeAngle)
                                || value.m_normalHalfConeAngle < 0.0f
                                || value.m_normalHalfConeAngle > AZ::Constants::Pi
                                || !AZ::IsFiniteFloat(value.m_planeHalfConeAngle)
                                || value.m_planeHalfConeAngle < 0.0f
                                || value.m_planeHalfConeAngle > AZ::Constants::Pi
                                || !AZ::IsFiniteFloat(value.m_twistMinimumAngle)
                                || value.m_twistMinimumAngle < -AZ::Constants::Pi
                                || !AZ::IsFiniteFloat(value.m_twistMaximumAngle)
                                || value.m_twistMaximumAngle < value.m_twistMinimumAngle
                                || value.m_twistMaximumAngle > AZ::Constants::Pi)
                            {
                                return;
                            }

                            JPH::Ref<JPH::SwingTwistConstraintSettings> native =
                                new JPH::SwingTwistConstraintSettings();
                            native->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
                            native->mPosition1 = ToRagdollNativeVector(value.m_firstPoint);
                            native->mPosition2 = ToRagdollNativeVector(value.m_secondPoint);
                            native->mTwistAxis1 = ToRagdollNativeVector(value.m_firstTwistAxis.GetNormalized());
                            native->mPlaneAxis1 = ToRagdollNativeVector(value.m_firstPlaneAxis.GetNormalized());
                            native->mTwistAxis2 = ToRagdollNativeVector(value.m_secondTwistAxis.GetNormalized());
                            native->mPlaneAxis2 = ToRagdollNativeVector(value.m_secondPlaneAxis.GetNormalized());
                            if (value.m_swingType == SwingType::Pyramid)
                            {
                                native->mSwingType = JPH::ESwingType::Pyramid;
                            }
                            native->mNormalHalfConeAngle = value.m_normalHalfConeAngle;
                            native->mPlaneHalfConeAngle = value.m_planeHalfConeAngle;
                            native->mTwistMinAngle = value.m_twistMinimumAngle;
                            native->mTwistMaxAngle = value.m_twistMaximumAngle;
                            native->mMaxFrictionTorque = value.m_maximumFrictionTorque;
                            native->mSwingMotorSettings = ToRagdollNativeMotor(value.m_swingMotor);
                            native->mTwistMotorSettings = ToRagdollNativeMotor(value.m_twistMotor);
                            result = native;
                        }
                    },
                    geometry);
                return result;
            };

        for (size_t partIndex = 0; partIndex < configuration.m_parts.size(); ++partIndex)
        {
            const RagdollPartConfiguration& source = configuration.m_parts[partIndex];
            ShapeSlot* shapeSlot = FindShape(source.m_body.m_shapeHandle);
            const AZ::Transform neutralTransform = AZ::Transform::CreateFromQuaternionAndTranslation(
                source.m_body.m_transform.m_rotation,
                {
                    static_cast<float>(source.m_body.m_transform.m_position.m_x),
                    static_cast<float>(source.m_body.m_transform.m_position.m_y),
                    static_cast<float>(source.m_body.m_transform.m_position.m_z),
                });
            const bool isRoot = skeleton->GetJoint(aznumeric_cast<int>(partIndex)).mParentJointIndex < 0;
            if (!shapeSlot
                || shapeSlot->m_shape->MustBeStatic()
                || (source.m_body.m_motionType != MotionType::Dynamic
                    && source.m_body.m_motionType != MotionType::Kinematic)
                || source.m_body.m_collisionGroup.m_filterHandle
                || !source.m_body.m_objectLayer
                || source.m_body.m_objectLayer.GetValue() > m_configuration.m_objectLayers.size()
                || !IsFiniteRagdollPosition(source.m_body.m_transform.m_position)
                || !source.m_body.m_transform.m_rotation.IsFinite()
                || source.m_body.m_transform.m_rotation.IsZero()
                || !IsValidRagdollPoseTransform(neutralTransform)
                || (isRoot && source.m_toParent.has_value())
                || (!isRoot && !source.m_toParent.has_value()))
            {
                valid = false;
                break;
            }

            JPH::RagdollSettings::Part& part = settings->mParts[partIndex];
            part.SetShape(shapeSlot->m_shape);
            part.mPosition = {
                static_cast<JPH::Real>(source.m_body.m_transform.m_position.m_x),
                static_cast<JPH::Real>(source.m_body.m_transform.m_position.m_y),
                static_cast<JPH::Real>(source.m_body.m_transform.m_position.m_z),
            };
            part.mRotation = ToRagdollNativeRotation(source.m_body.m_transform.m_rotation);
            part.mMotionType = ToRagdollNativeMotionType(source.m_body.m_motionType);
            part.mObjectLayer = static_cast<JPH::ObjectLayer>(source.m_body.m_objectLayer.GetValue() - 1);
            part.mLinearVelocity = ToRagdollNativeVector(source.m_body.m_linearVelocity);
            part.mAngularVelocity = ToRagdollNativeVector(source.m_body.m_angularVelocity);
            part.mAllowedDOFs = static_cast<JPH::EAllowedDOFs>(static_cast<AZ::u8>(source.m_body.m_allowedDofs));
            part.mAllowDynamicOrKinematic = source.m_body.m_allowDynamicOrKinematic;
            part.mIsSensor = source.m_body.m_isSensor;
            part.mCollideKinematicVsNonDynamic = source.m_body.m_collideKinematicVsNonDynamic;
            part.mUseManifoldReduction = source.m_body.m_useManifoldReduction;
            part.mApplyGyroscopicForce = source.m_body.m_applyGyroscopicForce;
            if (source.m_body.m_motionQuality == MotionQuality::Continuous)
            {
                part.mMotionQuality = JPH::EMotionQuality::LinearCast;
            }
            part.mEnhancedInternalEdgeRemoval = source.m_body.m_enhancedInternalEdgeRemoval;
            part.mAllowSleeping = source.m_body.m_allowSleeping;
            part.mFriction = source.m_body.m_friction;
            part.mRestitution = source.m_body.m_restitution;
            part.mLinearDamping = source.m_body.m_linearDamping;
            part.mAngularDamping = source.m_body.m_angularDamping;
            part.mMaxLinearVelocity = source.m_body.m_maximumLinearVelocity;
            part.mMaxAngularVelocity = source.m_body.m_maximumAngularVelocity;
            part.mGravityFactor = source.m_body.m_gravityFactor;
            part.mNumVelocityStepsOverride = source.m_body.m_velocityStepCount;
            part.mNumPositionStepsOverride = source.m_body.m_positionStepCount;
            part.mOverrideMassProperties = ToRagdollNativeMassPropertiesMode(source.m_body.m_massProperties.m_mode);
            part.mInertiaMultiplier = source.m_body.m_massProperties.m_inertiaMultiplier;
            part.mMassPropertiesOverride.mMass = source.m_body.m_massProperties.m_mass;
            if (source.m_body.m_massProperties.m_mode == MassPropertiesMode::Provided)
            {
                part.mMassPropertiesOverride.mInertia =
                    ToRagdollNativeInertia(source.m_body.m_massProperties.m_inertia);
            }
            if (source.m_toParent.has_value())
            {
                part.mToParent = makeConstraintSettings(source.m_toParent.value());
                if (!part.mToParent)
                {
                    valid = false;
                    break;
                }
                if (!AZStd::holds_alternative<HingeConstraintConfiguration>(source.m_toParent.value())
                    && !AZStd::holds_alternative<SwingTwistConstraintConfiguration>(source.m_toParent.value()))
                {
                    supportsMotorDrive = false;
                }
            }
            shapeHandles.push_back(source.m_body.m_shapeHandle);
            neutralTransforms.push_back(neutralTransform);
        }

        if (valid)
        {
            settings->mAdditionalConstraints.reserve(configuration.m_additionalConstraints.size());
            for (const AdditionalRagdollConstraint& source : configuration.m_additionalConstraints)
            {
                JPH::Ref<JPH::TwoBodyConstraintSettings> constraint = makeConstraintSettings(source.m_geometry);
                if (!constraint
                    || source.m_firstPartIndex >= configuration.m_parts.size()
                    || source.m_secondPartIndex >= configuration.m_parts.size()
                    || source.m_firstPartIndex == source.m_secondPartIndex)
                {
                    valid = false;
                    break;
                }
                settings->mAdditionalConstraints.emplace_back(
                    aznumeric_cast<int>(source.m_firstPartIndex),
                    aznumeric_cast<int>(source.m_secondPartIndex),
                    constraint);
            }
        }
        if (!valid)
        {
            m_system.ReleaseSkeletonDefinition(configuration.m_skeletonHandle);
            return {};
        }

        settings->CalculateBodyIndexToConstraintIndex();
        settings->CalculateConstraintIndexToBodyIdxPair();
        if (configuration.m_calculateConstraintPriorities)
        {
            settings->CalculateConstraintPriorities(configuration.m_baseConstraintPriority);
        }
        if (configuration.m_disableSelfCollisions)
        {
            JPH::Array<JPH::Mat44> nativeNeutralTransforms;
            nativeNeutralTransforms.reserve(neutralTransforms.size());
            for (const AZ::Transform& transform : neutralTransforms)
            {
                nativeNeutralTransforms.push_back(ToRagdollNativeTransform(transform));
            }
            settings->DisableParentChildCollisions(
                nativeNeutralTransforms.data(),
                configuration.m_minimumCollisionSeparation);
        }
        if (configuration.m_stabilize && !settings->Stabilize())
        {
            m_system.ReleaseSkeletonDefinition(configuration.m_skeletonHandle);
            return {};
        }

        AZ::u32 definitionIndex = 0;
        if (!m_freeRagdollDefinitionSlots.empty())
        {
            definitionIndex = m_freeRagdollDefinitionSlots.back();
            m_freeRagdollDefinitionSlots.pop_back();
        }
        else
        {
            if (m_ragdollDefinitionSlots.size() > Internal::MaximumWorldMemberIndex)
            {
                m_system.ReleaseSkeletonDefinition(configuration.m_skeletonHandle);
                return {};
            }
            definitionIndex = aznumeric_cast<AZ::u32>(m_ragdollDefinitionSlots.size());
            m_ragdollDefinitionSlots.emplace_back();
        }

        RagdollDefinitionSlot& slot = m_ragdollDefinitionSlots[definitionIndex];
        slot.m_settings = settings;
        slot.m_skeletonHandle = configuration.m_skeletonHandle;
        slot.m_shapeHandles = AZStd::move(shapeHandles);
        slot.m_neutralModelTransforms = AZStd::move(neutralTransforms);
        slot.m_supportsMotorDrive = supportsMotorDrive;
        for (const ShapeHandle shapeHandle : slot.m_shapeHandles)
        {
            ++FindShape(shapeHandle)->m_ragdollDefinitionCount;
        }
        return Internal::MakeWorldMemberHandle<RagdollDefinitionHandle>(
            m_worldIndex,
            definitionIndex,
            slot.m_generation);
    }

    bool World::DestroyRagdollDefinition(
        const RagdollDefinitionHandle definitionHandle)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollDefinitionSlot* slot = FindRagdollDefinition(definitionHandle);
        if (!slot || slot->m_ragdollCount > 0)
        {
            return false;
        }

        Internal::WorldMemberHandleParts parts;
        if (!Internal::DecodeWorldMemberHandle(definitionHandle, parts))
        {
            return false;
        }
        for (const ShapeHandle shapeHandle : slot->m_shapeHandles)
        {
            ShapeSlot* shapeSlot = FindShape(shapeHandle);
            AZ_Assert(
                shapeSlot && shapeSlot->m_ragdollDefinitionCount > 0,
                "Ragdoll shape ownership is inconsistent.");
            if (shapeSlot && shapeSlot->m_ragdollDefinitionCount > 0)
            {
                --shapeSlot->m_ragdollDefinitionCount;
            }
        }
        m_system.ReleaseSkeletonDefinition(slot->m_skeletonHandle);
        slot->m_settings = nullptr;
        slot->m_skeletonHandle = {};
        slot->m_shapeHandles.clear();
        slot->m_neutralModelTransforms.clear();
        slot->m_supportsMotorDrive = false;
        if (Internal::AdvanceGeneration(slot->m_generation))
        {
            m_freeRagdollDefinitionSlots.push_back(parts.m_index);
        }
        return true;
    }

    bool World::IsValid(
        const RagdollDefinitionHandle definitionHandle) const
    {
        AZStd::lock_guard lock(m_mutex);
        return FindRagdollDefinition(definitionHandle);
    }

    QueryResult World::GetRagdollBodyConstraintIndices(
        const RagdollDefinitionHandle definitionHandle,
        const AZStd::span<AZ::s32> constraintIndices) const
    {
        AZStd::lock_guard lock(m_mutex);
        const RagdollDefinitionSlot* slot = FindRagdollDefinition(definitionHandle);
        if (!slot)
        {
            return {};
        }

        const JPH::Array<int>& nativeIndices = slot->m_settings->GetBodyIndexToConstraintIndex();
        const size_t copyCount = AZStd::min(constraintIndices.size(), nativeIndices.size());
        for (size_t bodyIndex = 0; bodyIndex < copyCount; ++bodyIndex)
        {
            constraintIndices[bodyIndex] = aznumeric_cast<AZ::s32>(nativeIndices[bodyIndex]);
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(nativeIndices.size()),
        };
    }

    QueryResult World::GetRagdollConstraintBodyPairs(
        const RagdollDefinitionHandle definitionHandle,
        const AZStd::span<RagdollConstraintBodyPair> bodyPairs) const
    {
        AZStd::lock_guard lock(m_mutex);
        const RagdollDefinitionSlot* slot = FindRagdollDefinition(definitionHandle);
        if (!slot)
        {
            return {};
        }

        const JPH::Array<JPH::RagdollSettings::BodyIdxPair>& nativePairs =
            slot->m_settings->GetConstraintIndexToBodyIdxPair();
        const size_t copyCount = AZStd::min(bodyPairs.size(), nativePairs.size());
        for (size_t constraintIndex = 0; constraintIndex < copyCount; ++constraintIndex)
        {
            bodyPairs[constraintIndex] = {
                .m_firstBodyIndex = aznumeric_cast<AZ::s32>(nativePairs[constraintIndex].first),
                .m_secondBodyIndex = aznumeric_cast<AZ::s32>(nativePairs[constraintIndex].second),
            };
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(nativePairs.size()),
        };
    }

    RagdollHandle World::CreateRagdoll(
        const RagdollConfiguration& configuration)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollDefinitionSlot* definition = FindRagdollDefinition(configuration.m_definitionHandle);
        if (!definition || !IsFiniteRagdollPosition(configuration.m_rootPosition))
        {
            return {};
        }

        AZ::u32 ragdollIndex = 0;
        if (!m_freeRagdollSlots.empty())
        {
            ragdollIndex = m_freeRagdollSlots.back();
            m_freeRagdollSlots.pop_back();
        }
        else
        {
            if (m_ragdollSlots.size() > Internal::MaximumWorldMemberIndex)
            {
                return {};
            }
            ragdollIndex = aznumeric_cast<AZ::u32>(m_ragdollSlots.size());
            m_ragdollSlots.emplace_back();
        }
        RagdollSlot& slot = m_ragdollSlots[ragdollIndex];
        const RagdollHandle ragdollHandle = Internal::MakeWorldMemberHandle<RagdollHandle>(
            m_worldIndex,
            ragdollIndex,
            slot.m_generation);

        AZ::u32 collisionGroupId = configuration.m_collisionGroupId;
        if (collisionGroupId == 0)
        {
            while (m_ragdollHandlesByGroupId.contains(m_nextRagdollGroupId))
            {
                if (m_nextRagdollGroupId == AZStd::numeric_limits<AZ::u32>::max())
                {
                    m_freeRagdollSlots.push_back(ragdollIndex);
                    return {};
                }
                ++m_nextRagdollGroupId;
            }
            collisionGroupId = m_nextRagdollGroupId;
            if (m_nextRagdollGroupId == AZStd::numeric_limits<AZ::u32>::max())
            {
                m_freeRagdollSlots.push_back(ragdollIndex);
                return {};
            }
            ++m_nextRagdollGroupId;
        }
        else if (m_ragdollHandlesByGroupId.contains(collisionGroupId))
        {
            m_freeRagdollSlots.push_back(ragdollIndex);
            return {};
        }
        JPH::Ref<JPH::Ragdoll> ragdoll = definition->m_settings->CreateRagdoll(
            collisionGroupId,
            0,
            &m_physicsSystem);
        if (!ragdoll)
        {
            m_freeRagdollSlots.push_back(ragdollIndex);
            return {};
        }

        slot.m_bodyHandles.clear();
        slot.m_bodyHandles.reserve(ragdoll->GetBodyCount());
        slot.m_constraintHandles.clear();
        slot.m_constraintHandles.reserve(ragdoll->GetConstraintCount());
        AZStd::vector<AZ::u32> bodyIndices;
        AZStd::vector<AZ::u32> constraintIndices;
        bodyIndices.reserve(ragdoll->GetBodyCount());
        constraintIndices.reserve(ragdoll->GetConstraintCount());
        for (size_t bodyOffset = 0; bodyOffset < ragdoll->GetBodyCount(); ++bodyOffset)
        {
            AZ::u32 bodyIndex = 0;
            if (!m_freeBodySlots.empty())
            {
                bodyIndex = m_freeBodySlots.back();
                m_freeBodySlots.pop_back();
            }
            else
            {
                if (m_bodySlots.size() > Internal::MaximumWorldMemberIndex)
                {
                    ragdoll = nullptr;
                    for (const AZ::u32 reservedIndex : bodyIndices)
                    {
                        m_freeBodySlots.push_back(reservedIndex);
                    }
                    m_freeRagdollSlots.push_back(ragdollIndex);
                    return {};
                }
                bodyIndex = aznumeric_cast<AZ::u32>(m_bodySlots.size());
                m_bodySlots.emplace_back();
            }
            bodyIndices.push_back(bodyIndex);
            const BodySlot& bodySlot = m_bodySlots[bodyIndex];
            slot.m_bodyHandles.push_back(Internal::MakeWorldMemberHandle<BodyHandle>(
                m_worldIndex,
                bodyIndex,
                bodySlot.m_generation));
        }
        for (size_t constraintOffset = 0; constraintOffset < ragdoll->GetConstraintCount(); ++constraintOffset)
        {
            AZ::u32 constraintIndex = 0;
            if (!m_freeConstraintSlots.empty())
            {
                constraintIndex = m_freeConstraintSlots.back();
                m_freeConstraintSlots.pop_back();
            }
            else
            {
                if (m_constraintSlots.size() > Internal::MaximumWorldMemberIndex)
                {
                    ragdoll = nullptr;
                    for (const AZ::u32 reservedIndex : bodyIndices)
                    {
                        m_freeBodySlots.push_back(reservedIndex);
                    }
                    for (const AZ::u32 reservedIndex : constraintIndices)
                    {
                        m_freeConstraintSlots.push_back(reservedIndex);
                    }
                    m_freeRagdollSlots.push_back(ragdollIndex);
                    return {};
                }
                constraintIndex = aznumeric_cast<AZ::u32>(m_constraintSlots.size());
                m_constraintSlots.emplace_back();
            }
            constraintIndices.push_back(constraintIndex);
            const ConstraintSlot& constraintSlot = m_constraintSlots[constraintIndex];
            slot.m_constraintHandles.push_back(Internal::MakeWorldMemberHandle<ConstraintHandle>(
                m_worldIndex,
                constraintIndex,
                constraintSlot.m_generation));
        }

        JPH::BodyInterface& bodyInterface = m_physicsSystem.GetBodyInterface();
        for (size_t bodyOffset = 0; bodyOffset < ragdoll->GetBodyCount(); ++bodyOffset)
        {
            BodySlot& bodySlot = m_bodySlots[bodyIndices[bodyOffset]];
            const BodyHandle bodyHandle = slot.m_bodyHandles[bodyOffset];
            bodySlot.m_bodyId = ragdoll->GetBodyID(aznumeric_cast<int>(bodyOffset));
            bodySlot.m_groupFilterHandle = {};
            bodySlot.m_shapeHandle = definition->m_shapeHandles[bodyOffset];
            bodySlot.m_softBodyDefinitionHandle = {};
            bodySlot.m_characterHandle = {};
            bodySlot.m_vehicleHandle = {};
            bodySlot.m_ragdollHandle = ragdollHandle;
            bodySlot.m_entityId = configuration.m_entityId;
            bodySlot.m_name = configuration.m_name;
            bodySlot.m_kind = BodyKind::Rigid;
            bodySlot.m_motionType = MotionType::Dynamic;
            if (bodyInterface.GetMotionType(bodySlot.m_bodyId) == JPH::EMotionType::Kinematic)
            {
                bodySlot.m_motionType = MotionType::Kinematic;
            }
            bodySlot.m_constraintCount = 0;
            bodyInterface.SetUserData(bodySlot.m_bodyId, Internal::HandleAccess::ToValue(bodyHandle));
            ++FindShape(bodySlot.m_shapeHandle)->m_bodyCount;
        }

        for (size_t constraintOffset = 0; constraintOffset < ragdoll->GetConstraintCount(); ++constraintOffset)
        {
            ConstraintSlot& constraintSlot = m_constraintSlots[constraintIndices[constraintOffset]];
            const ConstraintHandle constraintHandle = slot.m_constraintHandles[constraintOffset];
            JPH::TwoBodyConstraint* nativeConstraint = ragdoll->GetConstraint(aznumeric_cast<int>(constraintOffset));
            const JPH::RagdollSettings::BodyIdxPair bodyIndicesPair =
                definition->m_settings->GetBodyIndicesForConstraintIndex(aznumeric_cast<int>(constraintOffset));
            constraintSlot.m_constraint = nativeConstraint;
            constraintSlot.m_firstBodyHandle = slot.m_bodyHandles[bodyIndicesPair.first];
            constraintSlot.m_secondBodyHandle = slot.m_bodyHandles[bodyIndicesPair.second];
            constraintSlot.m_dependencyHandles = {};
            constraintSlot.m_pathHandle = {};
            constraintSlot.m_entityId = configuration.m_entityId;
            constraintSlot.m_name = configuration.m_name;
            constraintSlot.m_pathRotationConstraint = PathRotationConstraint::None;
            constraintSlot.m_parentCount = 0;
            constraintSlot.m_ragdollHandle = ragdollHandle;
            nativeConstraint->SetUserData(Internal::HandleAccess::ToValue(constraintHandle));
            ++FindBody(constraintSlot.m_firstBodyHandle)->m_constraintCount;
            ++FindBody(constraintSlot.m_secondBodyHandle)->m_constraintCount;
        }

        slot.m_pose.SetSkeleton(definition->m_settings->GetSkeleton());
        slot.m_previousPose.SetSkeleton(definition->m_settings->GetSkeleton());
        slot.m_pose.GetJointMatrices().resize(definition->m_neutralModelTransforms.size());
        for (size_t partIndex = 0; partIndex < definition->m_neutralModelTransforms.size(); ++partIndex)
        {
            slot.m_pose.GetJointMatrices()[partIndex] =
                ToRagdollNativeTransform(definition->m_neutralModelTransforms[partIndex]);
        }
        slot.m_pose.CalculateJointStates();
        ragdoll->SetPose(
            ToRagdollNativePosition(configuration.m_rootPosition, m_configuration.m_origin),
            slot.m_pose.GetJointMatrices().data());
        JPH::EActivation activation = JPH::EActivation::DontActivate;
        if (configuration.m_activate)
        {
            activation = JPH::EActivation::Activate;
        }
        ragdoll->AddToPhysicsSystem(activation);
        for (const ConstraintHandle constraintHandle : slot.m_constraintHandles)
        {
            FindConstraint(constraintHandle)->m_isInSimulation = true;
        }

        slot.m_ragdoll = ragdoll;
        slot.m_definitionHandle = configuration.m_definitionHandle;
        slot.m_entityId = configuration.m_entityId;
        slot.m_name = configuration.m_name;
        slot.m_collisionGroupId = collisionGroupId;
        slot.m_isInSimulation = true;
        m_ragdollHandlesByGroupId.emplace(collisionGroupId, ragdollHandle);
        ++definition->m_ragdollCount;
        return ragdollHandle;
    }

    bool World::AddRagdollToSimulation(
        const RagdollHandle ragdollHandle,
        const bool activate)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::World::AddRagdollToSimulation");

        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot
            || slot->m_isInSimulation
            || !AdvanceConfigurationRevision())
        {
            return false;
        }

        JPH::EActivation activation = JPH::EActivation::DontActivate;
        if (activate)
        {
            activation = JPH::EActivation::Activate;
        }
        slot->m_ragdoll->AddToPhysicsSystem(activation);
        for (const ConstraintHandle constraintHandle : slot->m_constraintHandles)
        {
            FindConstraint(constraintHandle)->m_isInSimulation = true;
        }
        slot->m_isInSimulation = true;
        return true;
    }

    bool World::RemoveRagdollFromSimulation(
        const RagdollHandle ragdollHandle)
    {
        JOLT_PROFILE_SCOPE(Physics, "Jolt::World::RemoveRagdollFromSimulation");

        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot
            || !slot->m_isInSimulation
            || !AdvanceConfigurationRevision())
        {
            return false;
        }

        JPH::BodyInterface& bodyInterface = m_physicsSystem.GetBodyInterface();
        slot->m_removedBodyMotionStates.resize_no_construct(slot->m_bodyHandles.size());
        for (size_t bodyIndex = 0; bodyIndex < slot->m_bodyHandles.size(); ++bodyIndex)
        {
            RagdollSlot::RemovedBodyMotionState& motionState = slot->m_removedBodyMotionStates[bodyIndex];
            bodyInterface.GetLinearAndAngularVelocity(
                slot->m_ragdoll->GetBodyID(aznumeric_cast<int>(bodyIndex)),
                motionState.m_linearVelocity,
                motionState.m_angularVelocity);
        }

        slot->m_ragdoll->RemoveFromPhysicsSystem();
        for (size_t bodyIndex = 0; bodyIndex < slot->m_bodyHandles.size(); ++bodyIndex)
        {
            const RagdollSlot::RemovedBodyMotionState& motionState = slot->m_removedBodyMotionStates[bodyIndex];
            bodyInterface.SetLinearAndAngularVelocity(
                slot->m_ragdoll->GetBodyID(aznumeric_cast<int>(bodyIndex)),
                motionState.m_linearVelocity,
                motionState.m_angularVelocity);
        }
        slot->m_removedBodyMotionStates.clear();

        MaintainBroadPhaseAfterBodyRemovals(aznumeric_cast<AZ::u32>(slot->m_bodyHandles.size()));
        for (const ConstraintHandle constraintHandle : slot->m_constraintHandles)
        {
            FindConstraint(constraintHandle)->m_isInSimulation = false;
        }
        slot->m_isInSimulation = false;
        return true;
    }

    bool World::DestroyRagdoll(
        const RagdollHandle ragdollHandle)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot)
        {
            return false;
        }
        Internal::WorldMemberHandleParts ragdollParts;
        if (!Internal::DecodeWorldMemberHandle(ragdollHandle, ragdollParts))
        {
            return false;
        }

        if (slot->m_isInSimulation)
        {
            slot->m_ragdoll->RemoveFromPhysicsSystem();
            MaintainBroadPhaseAfterBodyRemovals(aznumeric_cast<AZ::u32>(slot->m_bodyHandles.size()));
        }
        for (const ConstraintHandle constraintHandle : slot->m_constraintHandles)
        {
            Internal::WorldMemberHandleParts parts;
            ConstraintSlot* constraintSlot = FindConstraint(constraintHandle);
            if (!constraintSlot || !Internal::DecodeWorldMemberHandle(constraintHandle, parts))
            {
                continue;
            }
            BodySlot* firstBody = FindBody(constraintSlot->m_firstBodyHandle);
            BodySlot* secondBody = FindBody(constraintSlot->m_secondBodyHandle);
            if (firstBody && firstBody->m_constraintCount > 0)
            {
                --firstBody->m_constraintCount;
            }
            if (secondBody && secondBody->m_constraintCount > 0)
            {
                --secondBody->m_constraintCount;
            }
            constraintSlot->m_constraint = nullptr;
            constraintSlot->m_firstBodyHandle = {};
            constraintSlot->m_secondBodyHandle = {};
            constraintSlot->m_ragdollHandle = {};
            constraintSlot->m_entityId = AZ::EntityId();
            constraintSlot->m_name = AZ::Name();
            constraintSlot->m_isInSimulation = false;
            if (Internal::AdvanceGeneration(constraintSlot->m_generation))
            {
                m_freeConstraintSlots.push_back(parts.m_index);
            }
        }
        for (const BodyHandle bodyHandle : slot->m_bodyHandles)
        {
            Internal::WorldMemberHandleParts parts;
            BodySlot* bodySlot = FindBody(bodyHandle);
            if (!bodySlot || !Internal::DecodeWorldMemberHandle(bodyHandle, parts))
            {
                continue;
            }
            [[maybe_unused]] const bool moveEventsDisabled =
                SetBodyMoveEventsEnabled(bodyHandle, false);
            AZ_Assert(moveEventsDisabled, "A ragdoll body move subscription must be removable during destruction.");
            ShapeSlot* shapeSlot = FindShape(bodySlot->m_shapeHandle);
            if (shapeSlot && shapeSlot->m_bodyCount > 0)
            {
                --shapeSlot->m_bodyCount;
            }
            bodySlot->m_bodyId = JPH::BodyID();
            bodySlot->m_shapeHandle = {};
            bodySlot->m_ragdollHandle = {};
            bodySlot->m_entityId = AZ::EntityId();
            bodySlot->m_name = AZ::Name();
            bodySlot->m_kind = BodyKind::None;
            bodySlot->m_motionType = MotionType::None;
            bodySlot->m_constraintCount = 0;
            if (Internal::AdvanceGeneration(bodySlot->m_generation))
            {
                m_freeBodySlots.push_back(parts.m_index);
            }
        }

        RagdollDefinitionSlot* definition = FindRagdollDefinition(slot->m_definitionHandle);
        AZ_Assert(definition && definition->m_ragdollCount > 0, "Ragdoll definition ownership is inconsistent.");
        if (definition && definition->m_ragdollCount > 0)
        {
            --definition->m_ragdollCount;
        }
        m_ragdollHandlesByGroupId.erase(slot->m_collisionGroupId);
        slot->m_ragdoll = nullptr;
        slot->m_bodyHandles.clear();
        slot->m_constraintHandles.clear();
        slot->m_removedBodyMotionStates.clear();
        slot->m_pose = {};
        slot->m_previousPose = {};
        slot->m_definitionHandle = {};
        slot->m_entityId = AZ::EntityId();
        slot->m_name = AZ::Name();
        slot->m_collisionGroupId = 0;
        slot->m_isInSimulation = false;
        if (Internal::AdvanceGeneration(slot->m_generation))
        {
            m_freeRagdollSlots.push_back(ragdollParts.m_index);
        }
        return true;
    }

    bool World::IsValid(
        const RagdollHandle ragdollHandle) const
    {
        AZStd::lock_guard lock(m_mutex);
        return FindRagdoll(ragdollHandle);
    }

    bool World::IsRagdollInSimulation(
        const RagdollHandle ragdollHandle) const
    {
        AZStd::lock_guard lock(m_mutex);
        const RagdollSlot* slot = FindRagdoll(ragdollHandle);
        return slot && slot->m_isInSimulation;
    }

    bool World::GetRagdollState(
        const RagdollHandle ragdollHandle,
        RagdollState& state) const
    {
        AZStd::lock_guard lock(m_mutex);
        const RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot)
        {
            return false;
        }

        JPH::RVec3 rootPosition;
        JPH::Quat rootRotation;
        slot->m_ragdoll->GetRootTransform(rootPosition, rootRotation);
        const JPH::AABox bounds = slot->m_ragdoll->GetWorldSpaceBounds();
        state = {
            .m_rootTransform = {
                .m_position = FromRagdollNativePosition(rootPosition, m_configuration.m_origin),
                .m_rotation = FromRagdollNativeRotation(rootRotation),
            },
            .m_bounds = AZ::Aabb::CreateFromMinMax(
                FromRagdollNativeVector(bounds.mMin),
                FromRagdollNativeVector(bounds.mMax)),
            .m_entityId = slot->m_entityId,
            .m_name = slot->m_name,
            .m_definitionHandle = slot->m_definitionHandle,
            .m_bodyCount = aznumeric_cast<AZ::u32>(slot->m_bodyHandles.size()),
            .m_collisionGroupId = slot->m_collisionGroupId,
            .m_constraintCount = aznumeric_cast<AZ::u32>(slot->m_constraintHandles.size()),
            .m_isActive = slot->m_isInSimulation && slot->m_ragdoll->IsActive(),
            .m_isInSimulation = slot->m_isInSimulation,
        };
        const AZ::Vector3 origin(
            static_cast<float>(m_configuration.m_origin.m_x),
            static_cast<float>(m_configuration.m_origin.m_y),
            static_cast<float>(m_configuration.m_origin.m_z));
        state.m_bounds = AZ::Aabb::CreateFromMinMax(
            state.m_bounds.GetMin() + origin,
            state.m_bounds.GetMax() + origin);
        return true;
    }

    bool World::SetRagdollCollisionGroupId(
        const RagdollHandle ragdollHandle,
        AZ::u32 collisionGroupId)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot)
        {
            return false;
        }
        if (collisionGroupId == slot->m_collisionGroupId)
        {
            return true;
        }
        if (collisionGroupId == 0)
        {
            while (m_ragdollHandlesByGroupId.contains(m_nextRagdollGroupId))
            {
                if (m_nextRagdollGroupId == AZStd::numeric_limits<AZ::u32>::max())
                {
                    return false;
                }
                ++m_nextRagdollGroupId;
            }
            collisionGroupId = m_nextRagdollGroupId;
            if (m_nextRagdollGroupId == AZStd::numeric_limits<AZ::u32>::max())
            {
                return false;
            }
            ++m_nextRagdollGroupId;
        }
        else if (m_ragdollHandlesByGroupId.contains(collisionGroupId))
        {
            return false;
        }

        slot->m_ragdoll->SetGroupID(collisionGroupId);
        m_ragdollHandlesByGroupId.erase(slot->m_collisionGroupId);
        slot->m_collisionGroupId = collisionGroupId;
        m_ragdollHandlesByGroupId.emplace(collisionGroupId, ragdollHandle);
        return true;
    }

    QueryResult World::GetRagdollBodies(
        const RagdollHandle ragdollHandle,
        const AZStd::span<BodyHandle> bodyHandles) const
    {
        AZStd::lock_guard lock(m_mutex);
        const RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot)
        {
            return {};
        }
        const size_t copyCount = AZStd::min(bodyHandles.size(), slot->m_bodyHandles.size());
        for (size_t bodyIndex = 0; bodyIndex < copyCount; ++bodyIndex)
        {
            bodyHandles[bodyIndex] = slot->m_bodyHandles[bodyIndex];
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(slot->m_bodyHandles.size()),
        };
    }

    QueryResult World::GetRagdollConstraints(
        const RagdollHandle ragdollHandle,
        const AZStd::span<ConstraintHandle> constraintHandles) const
    {
        AZStd::lock_guard lock(m_mutex);
        const RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot)
        {
            return {};
        }
        const size_t copyCount = AZStd::min(constraintHandles.size(), slot->m_constraintHandles.size());
        for (size_t constraintIndex = 0; constraintIndex < copyCount; ++constraintIndex)
        {
            constraintHandles[constraintIndex] = slot->m_constraintHandles[constraintIndex];
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(slot->m_constraintHandles.size()),
        };
    }

    bool World::ActivateRagdoll(
        const RagdollHandle ragdollHandle)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot || !slot->m_isInSimulation)
        {
            return false;
        }
        slot->m_ragdoll->Activate();
        return true;
    }

    bool World::SetRagdollPose(
        const RagdollHandle ragdollHandle,
        const WorldPosition rootPosition,
        const AZStd::span<const AZ::Transform> modelTransforms)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot
            || !IsFiniteRagdollPosition(rootPosition)
            || modelTransforms.size() != slot->m_bodyHandles.size())
        {
            return false;
        }
        for (size_t jointIndex = 0; jointIndex < modelTransforms.size(); ++jointIndex)
        {
            if (!IsValidRagdollPoseTransform(modelTransforms[jointIndex]))
            {
                return false;
            }
            slot->m_pose.GetJointMatrices()[jointIndex] =
                ToRagdollNativeTransform(modelTransforms[jointIndex]);
        }
        slot->m_pose.CalculateJointStates();
        slot->m_ragdoll->SetPose(
            ToRagdollNativePosition(rootPosition, m_configuration.m_origin),
            slot->m_pose.GetJointMatrices().data());
        return true;
    }

    QueryResult World::GetRagdollPose(
        const RagdollHandle ragdollHandle,
        WorldPosition& rootPosition,
        const AZStd::span<AZ::Transform> modelTransforms) const
    {
        AZStd::lock_guard lock(m_mutex);
        const RagdollSlot* constSlot = FindRagdoll(ragdollHandle);
        if (!constSlot)
        {
            return {};
        }
        RagdollSlot& slot = const_cast<RagdollSlot&>(*constSlot);
        JPH::RVec3 nativeRootPosition;
        slot.m_ragdoll->GetPose(nativeRootPosition, slot.m_pose.GetJointMatrices().data());
        rootPosition = FromRagdollNativePosition(nativeRootPosition, m_configuration.m_origin);
        const size_t copyCount = AZStd::min(modelTransforms.size(), slot.m_bodyHandles.size());
        for (size_t jointIndex = 0; jointIndex < copyCount; ++jointIndex)
        {
            modelTransforms[jointIndex] =
                FromRagdollNativeTransform(slot.m_pose.GetJointMatrices()[jointIndex]);
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(slot.m_bodyHandles.size()),
        };
    }

    bool World::DriveRagdollKinematically(
        const RagdollHandle ragdollHandle,
        const WorldPosition rootPosition,
        const AZStd::span<const AZ::Transform> modelTransforms,
        const float deltaTime)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot
            || !IsFiniteRagdollPosition(rootPosition)
            || !AZ::IsFiniteFloat(deltaTime)
            || deltaTime <= 0.0f
            || modelTransforms.size() != slot->m_bodyHandles.size())
        {
            return false;
        }
        for (size_t jointIndex = 0; jointIndex < modelTransforms.size(); ++jointIndex)
        {
            if (!IsValidRagdollPoseTransform(modelTransforms[jointIndex]))
            {
                return false;
            }
            slot->m_pose.GetJointMatrices()[jointIndex] =
                ToRagdollNativeTransform(modelTransforms[jointIndex]);
        }
        slot->m_ragdoll->DriveToPoseUsingKinematics(
            ToRagdollNativePosition(rootPosition, m_configuration.m_origin),
            slot->m_pose.GetJointMatrices().data(),
            deltaTime);
        return true;
    }

    bool World::DriveRagdollMotors(
        const RagdollHandle ragdollHandle,
        const AZStd::span<const AZ::Transform> modelTransforms)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        const RagdollDefinitionSlot* definition = nullptr;
        if (slot)
        {
            definition = FindRagdollDefinition(slot->m_definitionHandle);
        }
        if (!slot
            || !definition
            || !definition->m_supportsMotorDrive
            || modelTransforms.size() != slot->m_bodyHandles.size())
        {
            return false;
        }
        for (size_t jointIndex = 0; jointIndex < modelTransforms.size(); ++jointIndex)
        {
            if (!IsValidRagdollPoseTransform(modelTransforms[jointIndex]))
            {
                return false;
            }
            slot->m_pose.GetJointMatrices()[jointIndex] =
                ToRagdollNativeTransform(modelTransforms[jointIndex]);
        }
        slot->m_pose.CalculateJointStates();
        slot->m_ragdoll->DriveToPoseUsingMotors(slot->m_pose);
        return true;
    }

    bool World::DriveRagdollMotors(
        const RagdollHandle ragdollHandle,
        const AZStd::span<const AZ::Transform> previousModelTransforms,
        const AZStd::span<const AZ::Transform> modelTransforms,
        const float deltaTime)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        const RagdollDefinitionSlot* definition = nullptr;
        if (slot)
        {
            definition = FindRagdollDefinition(slot->m_definitionHandle);
        }
        if (!slot
            || !definition
            || !definition->m_supportsMotorDrive
            || !AZ::IsFiniteFloat(deltaTime)
            || deltaTime <= 0.0f
            || previousModelTransforms.size() != slot->m_bodyHandles.size()
            || modelTransforms.size() != slot->m_bodyHandles.size())
        {
            return false;
        }
        for (size_t jointIndex = 0; jointIndex < modelTransforms.size(); ++jointIndex)
        {
            if (!IsValidRagdollPoseTransform(previousModelTransforms[jointIndex])
                || !IsValidRagdollPoseTransform(modelTransforms[jointIndex]))
            {
                return false;
            }
            slot->m_previousPose.GetJointMatrices()[jointIndex] =
                ToRagdollNativeTransform(previousModelTransforms[jointIndex]);
            slot->m_pose.GetJointMatrices()[jointIndex] =
                ToRagdollNativeTransform(modelTransforms[jointIndex]);
        }
        slot->m_previousPose.CalculateJointStates();
        slot->m_pose.CalculateJointStates();
        slot->m_ragdoll->DriveToPoseUsingMotors(
            slot->m_previousPose,
            slot->m_pose,
            deltaTime);
        return true;
    }

    bool World::ResetRagdollWarmStart(
        const RagdollHandle ragdollHandle)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot)
        {
            return false;
        }
        slot->m_ragdoll->ResetWarmStart();
        return true;
    }

    bool World::SetRagdollVelocity(
        const RagdollHandle ragdollHandle,
        const AZ::Vector3 linearVelocity,
        const AZ::Vector3 angularVelocity)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot || !linearVelocity.IsFinite() || !angularVelocity.IsFinite())
        {
            return false;
        }
        slot->m_ragdoll->SetLinearAndAngularVelocity(
            ToRagdollNativeVector(linearVelocity),
            ToRagdollNativeVector(angularVelocity));
        return true;
    }

    bool World::SetRagdollLinearVelocity(
        const RagdollHandle ragdollHandle,
        const AZ::Vector3 linearVelocity)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot || !linearVelocity.IsFinite())
        {
            return false;
        }
        slot->m_ragdoll->SetLinearVelocity(ToRagdollNativeVector(linearVelocity));
        return true;
    }

    bool World::AddRagdollLinearVelocity(
        const RagdollHandle ragdollHandle,
        const AZ::Vector3 linearVelocity)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot || !linearVelocity.IsFinite())
        {
            return false;
        }
        slot->m_ragdoll->AddLinearVelocity(ToRagdollNativeVector(linearVelocity));
        return true;
    }

    bool World::AddRagdollImpulse(
        const RagdollHandle ragdollHandle,
        const AZ::Vector3 impulse)
    {
        AZStd::lock_guard lock(m_mutex);
        RagdollSlot* slot = FindRagdoll(ragdollHandle);
        if (!slot || !impulse.IsFinite())
        {
            return false;
        }
        slot->m_ragdoll->AddImpulse(ToRagdollNativeVector(impulse));
        return true;
    }

    const World::RagdollDefinitionSlot* World::FindRagdollDefinition(
        const RagdollDefinitionHandle definitionHandle) const
    {
        Internal::WorldMemberHandleParts parts;
        if (!Internal::DecodeWorldMemberHandle(definitionHandle, parts)
            || parts.m_worldIndex != m_worldIndex
            || parts.m_index >= m_ragdollDefinitionSlots.size())
        {
            return nullptr;
        }
        const RagdollDefinitionSlot& slot = m_ragdollDefinitionSlots[parts.m_index];
        if (!slot.m_settings || slot.m_generation != parts.m_generation)
        {
            return nullptr;
        }
        return &slot;
    }

    World::RagdollDefinitionSlot* World::FindRagdollDefinition(
        const RagdollDefinitionHandle definitionHandle)
    {
        return const_cast<RagdollDefinitionSlot*>(
            static_cast<const World&>(*this).FindRagdollDefinition(definitionHandle));
    }

    const World::RagdollSlot* World::FindRagdoll(
        const RagdollHandle ragdollHandle) const
    {
        Internal::WorldMemberHandleParts parts;
        if (!Internal::DecodeWorldMemberHandle(ragdollHandle, parts)
            || parts.m_worldIndex != m_worldIndex
            || parts.m_index >= m_ragdollSlots.size())
        {
            return nullptr;
        }
        const RagdollSlot& slot = m_ragdollSlots[parts.m_index];
        if (!slot.m_ragdoll || slot.m_generation != parts.m_generation)
        {
            return nullptr;
        }
        return &slot;
    }

    World::RagdollSlot* World::FindRagdoll(
        const RagdollHandle ragdollHandle)
    {
        return const_cast<RagdollSlot*>(static_cast<const World&>(*this).FindRagdoll(ragdollHandle));
    }
} // namespace Jolt
