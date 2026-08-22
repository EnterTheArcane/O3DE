/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Jolt/SystemInternal.h>

#include <Jolt/BehaviorReflection.h>
#include <Jolt/FloatEnvironment.h>
#include <Jolt/HandleEncoding.h>
#include <Jolt/Reflection.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/limits.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/string/string.h>

#include <Jolt/Math/Mat44.h>
#include <Jolt/Skeleton/SkeletalAnimation.h>
#include <Jolt/Skeleton/SkeletonMapper.h>

#include <cmath>

namespace Jolt
{
    namespace
    {
        [[nodiscard]]
        bool IsValidSkeletonTransform(
            const AZ::Transform& transform)
        {
            if (!transform.IsFinite() || !transform.IsOrthogonal())
            {
                return false;
            }

            const float scaleX = transform.GetBasisX().GetLength();
            const float scaleY = transform.GetBasisY().GetLength();
            const float scaleZ = transform.GetBasisZ().GetLength();
            return scaleX > 0.0f
                && AZ::IsClose(scaleX, scaleY)
                && AZ::IsClose(scaleX, scaleZ);
        }

        [[nodiscard]]
        bool IsValidRigidSkeletonTransform(
            const AZ::Transform& transform)
        {
            return IsValidSkeletonTransform(transform)
                && AZ::IsClose(transform.GetUniformScale(), 1.0f, AZ::Constants::Tolerance);
        }

        [[nodiscard]]
        JPH::Mat44 ToNativeSkeletonTransform(
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
        AZ::Transform FromNativeSkeletonTransform(
            JPH::Mat44Arg transform)
        {
            const JPH::Vec3 basisX = transform.GetColumn3(0);
            const JPH::Vec3 basisY = transform.GetColumn3(1);
            const JPH::Vec3 basisZ = transform.GetColumn3(2);
            const JPH::Vec3 translation = transform.GetTranslation();
            return AZ::Transform::CreateFromMatrix3x3AndTranslation(
                AZ::Matrix3x3::CreateFromColumns(
                    {basisX.GetX(), basisX.GetY(), basisX.GetZ()},
                    {basisY.GetX(), basisY.GetY(), basisY.GetZ()},
                    {basisZ.GetX(), basisZ.GetY(), basisZ.GetZ()}),
                {translation.GetX(), translation.GetY(), translation.GetZ()});
        }

        [[nodiscard]]
        QueryResult CopySkeletonJointIndices(
            const JPH::Array<int>& source,
            const AZStd::span<AZ::u32> destination)
        {
            const size_t copyCount = AZStd::min(source.size(), destination.size());
            for (size_t jointIndex = 0; jointIndex < copyCount; ++jointIndex)
            {
                destination[jointIndex] = aznumeric_cast<AZ::u32>(source[jointIndex]);
            }
            return {
                .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
                .m_requiredHitCount = aznumeric_cast<AZ::u32>(source.size()),
            };
        }

        [[nodiscard]]
        bool BuildSkeletalAnimation(
            const SkeletalAnimationConfiguration& configuration,
            JPH::Ref<JPH::SkeletalAnimation>& animation,
            AZStd::vector<AZ::Name>& jointNames)
        {
            JPH::Ref<JPH::SkeletalAnimation> result = new JPH::SkeletalAnimation();
            result->SetIsLooping(configuration.m_isLooping);
            JPH::SkeletalAnimation::AnimatedJointVector& nativeJoints = result->GetAnimatedJoints();
            nativeJoints.reserve(configuration.m_joints.size());

            AZStd::vector<AZ::Name> resultNames;
            resultNames.reserve(configuration.m_joints.size());
            for (const SkeletalAnimatedJoint& joint : configuration.m_joints)
            {
                if (joint.m_name.IsEmpty()
                    || joint.m_keyframes.empty()
                    || AZStd::find(resultNames.begin(), resultNames.end(), joint.m_name) != resultNames.end())
                {
                    return false;
                }

                JPH::SkeletalAnimation::AnimatedJoint nativeJoint;
                nativeJoint.mJointName = joint.m_name.GetCStr();
                nativeJoint.mKeyframes.reserve(joint.m_keyframes.size());
                float previousTime = -1.0f;
                for (const SkeletalAnimationKeyframe& keyframe : joint.m_keyframes)
                {
                    const float rotationLengthSq = keyframe.m_rotation.GetLengthSq();
                    if (!keyframe.m_rotation.IsFinite()
                        || !AZ::IsFiniteFloat(rotationLengthSq)
                        || rotationLengthSq <= 0.0f
                        || !keyframe.m_translation.IsFinite()
                        || !AZ::IsFiniteFloat(keyframe.m_time)
                        || keyframe.m_time < 0.0f
                        || keyframe.m_time <= previousTime)
                    {
                        return false;
                    }

                    const AZ::Quaternion rotation = keyframe.m_rotation.GetNormalized();
                    JPH::SkeletalAnimation::Keyframe nativeKeyframe;
                    nativeKeyframe.mRotation = {
                        rotation.GetX(),
                        rotation.GetY(),
                        rotation.GetZ(),
                        rotation.GetW(),
                    };
                    nativeKeyframe.mTranslation = {
                        keyframe.m_translation.GetX(),
                        keyframe.m_translation.GetY(),
                        keyframe.m_translation.GetZ(),
                    };
                    nativeKeyframe.mTime = keyframe.m_time;
                    nativeJoint.mKeyframes.push_back(nativeKeyframe);
                    previousTime = keyframe.m_time;
                }
                nativeJoints.push_back(AZStd::move(nativeJoint));
                resultNames.push_back(joint.m_name);
            }

            animation = AZStd::move(result);
            jointNames = AZStd::move(resultNames);
            return true;
        }

        template<class Value>
        [[nodiscard]]
        AZStd::string GetScriptName(
            const Value* value)
        {
            return AZStd::string(value->m_name.GetStringView());
        }

        template<class Value>
        void SetScriptName(
            Value* value,
            AZStd::string name)
        {
            value->m_name = name;
        }
    } // namespace

    void SkeletonDefinitionConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SkeletonDefinitionConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<SkeletonJoint>()
                ->Field("Name", &SkeletonJoint::m_name)
                ->Field("ParentIndex", &SkeletonJoint::m_parentIndex);

            serializeContext
                ->Class<SkeletonDefinitionConfiguration>()
                ->Field("Joints", &SkeletonDefinitionConfiguration::m_joints);

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext
                    ->Class<SkeletonJoint>("Joint", "A stable joint name and parent index.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkeletonJoint::m_name, "Name", "")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkeletonJoint::m_parentIndex, "Parent index", "");

                editContext
                    ->Class<SkeletonDefinitionConfiguration>("Skeleton", "Parent-before-child joint topology.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SkeletonDefinitionConfiguration::m_joints, "Joints", "");
            }
        }
    }

    void SkeletalAnimationConfiguration::Reflect(
        AZ::ReflectContext* context)
    {
        SkeletonDefinitionConfiguration::Reflect(context);
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            if (!ShouldReflect<SkeletalAnimationConfiguration>(*serializeContext))
            {
                return;
            }

            serializeContext
                ->Class<SkeletonJointMapping>()
                ->Field("SourceJoint", &SkeletonJointMapping::m_sourceJoint)
                ->Field("TargetJoint", &SkeletonJointMapping::m_targetJoint);

            serializeContext
                ->Class<SkeletonMapperConfiguration>()
                ->Field("SourceSkeletonHandle", &SkeletonMapperConfiguration::m_sourceSkeletonHandle)
                ->Field("TargetSkeletonHandle", &SkeletonMapperConfiguration::m_targetSkeletonHandle)
                ->Field("SourceNeutralModelTransforms", &SkeletonMapperConfiguration::m_sourceNeutralModelTransforms)
                ->Field("TargetNeutralModelTransforms", &SkeletonMapperConfiguration::m_targetNeutralModelTransforms)
                ->Field("JointMappings", &SkeletonMapperConfiguration::m_jointMappings)
                ->Field("LockedTargetTranslations", &SkeletonMapperConfiguration::m_lockedTargetTranslations)
                ->Field("LockAllTargetTranslations", &SkeletonMapperConfiguration::m_lockAllTargetTranslations);

            serializeContext
                ->Class<SkeletonMapperState>()
                ->Field("SourceSkeletonHandle", &SkeletonMapperState::m_sourceSkeletonHandle)
                ->Field("TargetSkeletonHandle", &SkeletonMapperState::m_targetSkeletonHandle)
                ->Field("ChainCount", &SkeletonMapperState::m_chainCount)
                ->Field("LockedTranslationCount", &SkeletonMapperState::m_lockedTranslationCount)
                ->Field("MappingCount", &SkeletonMapperState::m_mappingCount)
                ->Field("SourceJointCount", &SkeletonMapperState::m_sourceJointCount)
                ->Field("TargetJointCount", &SkeletonMapperState::m_targetJointCount)
                ->Field("UnmappedJointCount", &SkeletonMapperState::m_unmappedJointCount);

            serializeContext
                ->Class<SkeletonMapperMappingState>()
                ->Field("SourceToTarget", &SkeletonMapperMappingState::m_sourceToTarget)
                ->Field("TargetToSource", &SkeletonMapperMappingState::m_targetToSource)
                ->Field("SourceJointIndex", &SkeletonMapperMappingState::m_sourceJointIndex)
                ->Field("TargetJointIndex", &SkeletonMapperMappingState::m_targetJointIndex);

            serializeContext
                ->Class<SkeletonMapperChainState>()
                ->Field("SourceJointCount", &SkeletonMapperChainState::m_sourceJointCount)
                ->Field("TargetJointCount", &SkeletonMapperChainState::m_targetJointCount);

            serializeContext
                ->Class<SkeletonMapperUnmappedJoint>()
                ->Field("JointIndex", &SkeletonMapperUnmappedJoint::m_jointIndex)
                ->Field("ParentJointIndex", &SkeletonMapperUnmappedJoint::m_parentJointIndex);

            serializeContext
                ->Class<SkeletonMapperLockedTranslation>()
                ->Field("Translation", &SkeletonMapperLockedTranslation::m_translation)
                ->Field("JointIndex", &SkeletonMapperLockedTranslation::m_jointIndex)
                ->Field("ParentJointIndex", &SkeletonMapperLockedTranslation::m_parentJointIndex);

            serializeContext
                ->Class<SkeletonDefinitionArchive>()
                ->Field("BinaryState", &SkeletonDefinitionArchive::m_binaryState)
                ->Field("BuildFingerprint", &SkeletonDefinitionArchive::m_buildFingerprint)
                ->Field("ContentHash", &SkeletonDefinitionArchive::m_contentHash)
                ->Field("FormatVersion", &SkeletonDefinitionArchive::m_formatVersion)
                ->Field("JointCount", &SkeletonDefinitionArchive::m_jointCount);

            serializeContext
                ->Class<SkeletalAnimationKeyframe>()
                ->Field("Rotation", &SkeletalAnimationKeyframe::m_rotation)
                ->Field("Translation", &SkeletalAnimationKeyframe::m_translation)
                ->Field("Time", &SkeletalAnimationKeyframe::m_time);

            serializeContext
                ->Class<SkeletalAnimatedJoint>()
                ->Field("Name", &SkeletalAnimatedJoint::m_name)
                ->Field("Keyframes", &SkeletalAnimatedJoint::m_keyframes);

            serializeContext
                ->Class<SkeletalAnimationConfiguration>()
                ->Field("Joints", &SkeletalAnimationConfiguration::m_joints)
                ->Field("IsLooping", &SkeletalAnimationConfiguration::m_isLooping);

            serializeContext
                ->Class<SkeletalAnimationArchive>()
                ->Field("BinaryState", &SkeletalAnimationArchive::m_binaryState)
                ->Field("BuildFingerprint", &SkeletalAnimationArchive::m_buildFingerprint)
                ->Field("ContentHash", &SkeletalAnimationArchive::m_contentHash)
                ->Field("FormatVersion", &SkeletalAnimationArchive::m_formatVersion)
                ->Field("JointCount", &SkeletalAnimationArchive::m_jointCount);

            serializeContext
                ->Class<SkeletalAnimationState>()
                ->Field("Duration", &SkeletalAnimationState::m_duration)
                ->Field("JointCount", &SkeletalAnimationState::m_jointCount)
                ->Field("IsLooping", &SkeletalAnimationState::m_isLooping);

            serializeContext
                ->Class<SkeletonPoseState>()
                ->Field("RootOffset", &SkeletonPoseState::m_rootOffset)
                ->Field("SkeletonHandle", &SkeletonPoseState::m_skeletonHandle)
                ->Field("JointCount", &SkeletonPoseState::m_jointCount);
        }

        if (auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->Class<SkeletonJoint>("SkeletonJoint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "name",
                    &GetScriptName<SkeletonJoint>,
                    &SetScriptName<SkeletonJoint>)
                ->Property("parentIndex", JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletonJoint::m_parentIndex));

            behaviorContext->Class<SkeletonDefinitionConfiguration>("SkeletonDefinitionConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("joints", JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletonDefinitionConfiguration::m_joints));

            behaviorContext->Class<SkeletonJointMapping>("SkeletonJointMapping")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("sourceJoint", JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletonJointMapping::m_sourceJoint))
                ->Property("targetJoint", JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletonJointMapping::m_targetJoint));

            behaviorContext->Class<SkeletonMapperConfiguration>("SkeletonMapperConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "sourceSkeletonHandle",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletonMapperConfiguration::m_sourceSkeletonHandle))
                ->Property(
                    "targetSkeletonHandle",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletonMapperConfiguration::m_targetSkeletonHandle))
                ->Property(
                    "sourceNeutralModelTransforms",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletonMapperConfiguration::m_sourceNeutralModelTransforms))
                ->Property(
                    "targetNeutralModelTransforms",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletonMapperConfiguration::m_targetNeutralModelTransforms))
                ->Property("jointMappings", JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletonMapperConfiguration::m_jointMappings))
                ->Property(
                    "lockedTargetTranslations",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletonMapperConfiguration::m_lockedTargetTranslations))
                ->Property(
                    "lockAllTargetTranslations",
                    JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletonMapperConfiguration::m_lockAllTargetTranslations));

            behaviorContext->Class<SkeletonMapperState>("SkeletonMapperState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "sourceSkeletonHandle",
                    BehaviorValueGetter(&SkeletonMapperState::m_sourceSkeletonHandle),
                    nullptr)
                ->Property(
                    "targetSkeletonHandle",
                    BehaviorValueGetter(&SkeletonMapperState::m_targetSkeletonHandle),
                    nullptr)
                ->Property("chainCount", BehaviorValueGetter(&SkeletonMapperState::m_chainCount), nullptr)
                ->Property(
                    "lockedTranslationCount",
                    BehaviorValueGetter(&SkeletonMapperState::m_lockedTranslationCount),
                    nullptr)
                ->Property("mappingCount", BehaviorValueGetter(&SkeletonMapperState::m_mappingCount), nullptr)
                ->Property("sourceJointCount", BehaviorValueGetter(&SkeletonMapperState::m_sourceJointCount), nullptr)
                ->Property("targetJointCount", BehaviorValueGetter(&SkeletonMapperState::m_targetJointCount), nullptr)
                ->Property(
                    "unmappedJointCount",
                    BehaviorValueGetter(&SkeletonMapperState::m_unmappedJointCount),
                    nullptr);

            behaviorContext->Class<SkeletonMapperMappingState>("SkeletonMapperMappingState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "sourceToTarget",
                    BehaviorValueGetter(&SkeletonMapperMappingState::m_sourceToTarget),
                    nullptr)
                ->Property(
                    "targetToSource",
                    BehaviorValueGetter(&SkeletonMapperMappingState::m_targetToSource),
                    nullptr)
                ->Property(
                    "sourceJointIndex",
                    BehaviorValueGetter(&SkeletonMapperMappingState::m_sourceJointIndex),
                    nullptr)
                ->Property(
                    "targetJointIndex",
                    BehaviorValueGetter(&SkeletonMapperMappingState::m_targetJointIndex),
                    nullptr);

            behaviorContext->Class<SkeletonMapperChainState>("SkeletonMapperChainState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "sourceJointCount",
                    BehaviorValueGetter(&SkeletonMapperChainState::m_sourceJointCount),
                    nullptr)
                ->Property(
                    "targetJointCount",
                    BehaviorValueGetter(&SkeletonMapperChainState::m_targetJointCount),
                    nullptr);

            behaviorContext->Class<SkeletonMapperUnmappedJoint>("SkeletonMapperUnmappedJoint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "jointIndex",
                    BehaviorValueGetter(&SkeletonMapperUnmappedJoint::m_jointIndex),
                    nullptr)
                ->Property(
                    "parentJointIndex",
                    BehaviorValueGetter(&SkeletonMapperUnmappedJoint::m_parentJointIndex),
                    nullptr);

            behaviorContext->Class<SkeletonMapperLockedTranslation>("SkeletonMapperLockedTranslation")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "translation",
                    BehaviorValueGetter(&SkeletonMapperLockedTranslation::m_translation),
                    nullptr)
                ->Property(
                    "jointIndex",
                    BehaviorValueGetter(&SkeletonMapperLockedTranslation::m_jointIndex),
                    nullptr)
                ->Property(
                    "parentJointIndex",
                    BehaviorValueGetter(&SkeletonMapperLockedTranslation::m_parentJointIndex),
                    nullptr);

            behaviorContext->Class<SkeletalAnimationKeyframe>("SkeletalAnimationKeyframe")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("rotation", JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletalAnimationKeyframe::m_rotation))
                ->Property("translation", JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletalAnimationKeyframe::m_translation))
                ->Property("time", JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletalAnimationKeyframe::m_time));

            behaviorContext->Class<SkeletalAnimatedJoint>("SkeletalAnimatedJoint")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property(
                    "name",
                    &GetScriptName<SkeletalAnimatedJoint>,
                    &SetScriptName<SkeletalAnimatedJoint>)
                ->Property("keyframes", JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletalAnimatedJoint::m_keyframes));

            behaviorContext->Class<SkeletalAnimationConfiguration>("SkeletalAnimationConfiguration")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("joints", JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletalAnimationConfiguration::m_joints))
                ->Property("isLooping", JOLT_BEHAVIOR_VALUE_PROPERTY(&SkeletalAnimationConfiguration::m_isLooping));

            behaviorContext->Class<SkeletalAnimationState>("SkeletalAnimationState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("duration", BehaviorValueGetter(&SkeletalAnimationState::m_duration), nullptr)
                ->Property("jointCount", BehaviorValueGetter(&SkeletalAnimationState::m_jointCount), nullptr)
                ->Property("isLooping", BehaviorValueGetter(&SkeletalAnimationState::m_isLooping), nullptr);

            behaviorContext->Class<SkeletonPoseState>("SkeletonPoseState")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Module, "jolt")
                ->Constructor<>()
                ->Property("rootOffset", BehaviorValueGetter(&SkeletonPoseState::m_rootOffset), nullptr)
                ->Property("skeletonHandle", BehaviorValueGetter(&SkeletonPoseState::m_skeletonHandle), nullptr)
                ->Property("jointCount", BehaviorValueGetter(&SkeletonPoseState::m_jointCount), nullptr);
        }
    }

    SkeletonDefinitionHandle RuntimeImplementation::CreateSkeletonDefinition(
        const SkeletonDefinitionConfiguration& configuration)
    {
        if (configuration.m_joints.empty()
            || configuration.m_joints.size() >= Internal::HandlePayloadMask)
        {
            return {};
        }

        for (size_t jointIndex = 0; jointIndex < configuration.m_joints.size(); ++jointIndex)
        {
            const SkeletonJoint& joint = configuration.m_joints[jointIndex];
            if (joint.m_name.IsEmpty()
                || joint.m_parentIndex >= aznumeric_cast<AZ::s32>(jointIndex)
                || joint.m_parentIndex < -1)
            {
                return {};
            }
            for (size_t previousIndex = 0; previousIndex < jointIndex; ++previousIndex)
            {
                if (configuration.m_joints[previousIndex].m_name == joint.m_name)
                {
                    return {};
                }
            }
        }

        JPH::Ref<JPH::Skeleton> skeleton = new JPH::Skeleton();
        skeleton->GetJoints().reserve(configuration.m_joints.size());
        for (const SkeletonJoint& joint : configuration.m_joints)
        {
            skeleton->AddJoint(joint.m_name.GetCStr(), joint.m_parentIndex);
        }
        if (!skeleton->AreJointsCorrectlyOrdered())
        {
            return {};
        }

        return StoreSkeletonDefinition(
            AZStd::move(skeleton),
            {configuration.m_joints.begin(), configuration.m_joints.end()});
    }

    SkeletonDefinitionHandle RuntimeImplementation::StoreSkeletonDefinition(
        JPH::Ref<JPH::Skeleton> skeleton,
        AZStd::vector<SkeletonJoint> joints)
    {
        AZStd::lock_guard lock(m_skeletonMutex);
        AZ::u32 skeletonIndex = 0;
        if (!m_freeSkeletonDefinitionSlots.empty())
        {
            skeletonIndex = m_freeSkeletonDefinitionSlots.back();
            m_freeSkeletonDefinitionSlots.pop_back();
        }
        else
        {
            if (m_skeletonDefinitionSlots.size() >= Internal::HandlePayloadMask)
            {
                return {};
            }
            skeletonIndex = aznumeric_cast<AZ::u32>(m_skeletonDefinitionSlots.size());
            m_skeletonDefinitionSlots.emplace_back();
        }

        SkeletonDefinitionSlot& slot = m_skeletonDefinitionSlots[skeletonIndex];
        slot.m_skeleton = AZStd::move(skeleton);
        slot.m_joints = AZStd::move(joints);
        slot.m_jointIndices.clear();
        slot.m_jointIndices.reserve(slot.m_joints.size());
        for (size_t jointIndex = 0; jointIndex < slot.m_joints.size(); ++jointIndex)
        {
            slot.m_jointIndices.emplace(slot.m_joints[jointIndex].m_name, aznumeric_cast<AZ::u32>(jointIndex));
        }
        return Internal::MakeResourceHandle<SkeletonDefinitionHandle>(skeletonIndex, slot.m_generation);
    }

    bool RuntimeImplementation::DestroySkeletonDefinition(
        const SkeletonDefinitionHandle skeletonHandle)
    {
        AZStd::lock_guard lock(m_skeletonMutex);
        SkeletonDefinitionSlot* slot = FindSkeletonDefinitionUnlocked(skeletonHandle);
        if (!slot
            || slot->m_mapperCount > 0
            || slot->m_poseCount > 0
            || slot->m_ragdollDefinitionCount > 0)
        {
            return false;
        }

        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(skeletonHandle, parts))
        {
            return false;
        }
        slot->m_skeleton = nullptr;
        slot->m_jointIndices.clear();
        slot->m_joints.clear();
        if (Internal::AdvanceGeneration(slot->m_generation))
        {
            m_freeSkeletonDefinitionSlots.push_back(parts.m_index);
        }
        return true;
    }

    bool RuntimeImplementation::IsValid(
        const SkeletonDefinitionHandle skeletonHandle) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        return FindSkeletonDefinitionUnlocked(skeletonHandle);
    }

    QueryResult RuntimeImplementation::GetSkeletonJoints(
        const SkeletonDefinitionHandle skeletonHandle,
        AZStd::span<SkeletonJoint> joints) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonDefinitionSlot* slot = FindSkeletonDefinitionUnlocked(skeletonHandle);
        if (!slot)
        {
            return {};
        }

        const size_t copiedJointCount = AZStd::min(joints.size(), slot->m_joints.size());
        for (size_t jointIndex = 0; jointIndex < copiedJointCount; ++jointIndex)
        {
            joints[jointIndex] = slot->m_joints[jointIndex];
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copiedJointCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(slot->m_joints.size()),
        };
    }

    bool RuntimeImplementation::FindSkeletonJoint(
        const SkeletonDefinitionHandle skeletonHandle,
        const AZ::Name jointName,
        AZ::u32& jointIndex) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonDefinitionSlot* slot = FindSkeletonDefinitionUnlocked(skeletonHandle);
        if (!slot)
        {
            return false;
        }

        const auto jointIterator = slot->m_jointIndices.find(jointName);
        if (jointIterator == slot->m_jointIndices.end())
        {
            return false;
        }

        jointIndex = jointIterator->second;
        return true;
    }

    SkeletalAnimationHandle RuntimeImplementation::CreateSkeletalAnimation(
        const SkeletalAnimationConfiguration& configuration)
    {
        const DeterministicFloatScope floatScope;
        JPH::Ref<JPH::SkeletalAnimation> animation;
        AZStd::vector<AZ::Name> jointNames;
        if (!BuildSkeletalAnimation(configuration, animation, jointNames))
        {
            return {};
        }

        return StoreSkeletalAnimation(
            AZStd::move(animation),
            AZStd::move(jointNames));
    }

    SkeletalAnimationHandle RuntimeImplementation::StoreSkeletalAnimation(
        JPH::Ref<JPH::SkeletalAnimation> animation,
        AZStd::vector<AZ::Name> jointNames)
    {
        AZStd::lock_guard lock(m_skeletonMutex);
        AZ::u32 animationIndex = 0;
        if (!m_freeSkeletalAnimationSlots.empty())
        {
            animationIndex = m_freeSkeletalAnimationSlots.back();
            m_freeSkeletalAnimationSlots.pop_back();
        }
        else
        {
            if (m_skeletalAnimationSlots.size() >= Internal::HandlePayloadMask)
            {
                return {};
            }
            animationIndex = aznumeric_cast<AZ::u32>(m_skeletalAnimationSlots.size());
            m_skeletalAnimationSlots.emplace_back();
        }

        SkeletalAnimationSlot& slot = m_skeletalAnimationSlots[animationIndex];
        slot.m_animation = AZStd::move(animation);
        slot.m_jointNames = AZStd::move(jointNames);
        return Internal::MakeResourceHandle<SkeletalAnimationHandle>(animationIndex, slot.m_generation);
    }

    bool RuntimeImplementation::UpdateSkeletalAnimation(
        const SkeletalAnimationHandle animationHandle,
        const SkeletalAnimationConfiguration& configuration)
    {
        const DeterministicFloatScope floatScope;
        JPH::Ref<JPH::SkeletalAnimation> animation;
        AZStd::vector<AZ::Name> jointNames;
        if (!BuildSkeletalAnimation(configuration, animation, jointNames))
        {
            return false;
        }

        AZStd::lock_guard lock(m_skeletonMutex);
        SkeletalAnimationSlot* slot = FindSkeletalAnimationUnlocked(animationHandle);
        if (!slot)
        {
            return false;
        }

        slot->m_animation = AZStd::move(animation);
        slot->m_jointNames = AZStd::move(jointNames);
        ++slot->m_revision;
        if (slot->m_revision == 0)
        {
            slot->m_revision = 1;
        }
        return true;
    }

    bool RuntimeImplementation::DestroySkeletalAnimation(
        const SkeletalAnimationHandle animationHandle)
    {
        AZStd::lock_guard lock(m_skeletonMutex);
        SkeletalAnimationSlot* slot = FindSkeletalAnimationUnlocked(animationHandle);
        if (!slot)
        {
            return false;
        }

        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(animationHandle, parts))
        {
            return false;
        }
        slot->m_animation = nullptr;
        slot->m_jointNames.clear();
        if (Internal::AdvanceGeneration(slot->m_generation))
        {
            m_freeSkeletalAnimationSlots.push_back(parts.m_index);
        }
        return true;
    }

    bool RuntimeImplementation::DestroySkeletonResources(
        const SkeletonDefinitionHandle skeletonHandle,
        const AZStd::span<const SkeletalAnimationHandle> animationHandles)
    {
        AZStd::lock_guard lock(m_skeletonMutex);
        SkeletonDefinitionSlot* skeletonSlot = FindSkeletonDefinitionUnlocked(skeletonHandle);
        Internal::ResourceHandleParts skeletonParts;
        if (!skeletonSlot
            || skeletonSlot->m_mapperCount > 0
            || skeletonSlot->m_poseCount > 0
            || skeletonSlot->m_ragdollDefinitionCount > 0
            || !Internal::DecodeResourceHandle(skeletonHandle, skeletonParts))
        {
            return false;
        }

        AZStd::vector<Internal::ResourceHandleParts> animationParts;
        animationParts.reserve(animationHandles.size());
        for (const SkeletalAnimationHandle animationHandle : animationHandles)
        {
            Internal::ResourceHandleParts parts;
            if (!FindSkeletalAnimationUnlocked(animationHandle)
                || !Internal::DecodeResourceHandle(animationHandle, parts)
                || AZStd::find_if(
                    animationParts.begin(),
                    animationParts.end(),
                    [&](const Internal::ResourceHandleParts& existing)
                    {
                        return existing.m_index == parts.m_index;
                    }) != animationParts.end())
            {
                return false;
            }
            animationParts.push_back(parts);
        }

        for (const Internal::ResourceHandleParts& parts : animationParts)
        {
            SkeletalAnimationSlot& animationSlot = m_skeletalAnimationSlots[parts.m_index];
            animationSlot.m_animation = nullptr;
            animationSlot.m_jointNames.clear();
            if (Internal::AdvanceGeneration(animationSlot.m_generation))
            {
                m_freeSkeletalAnimationSlots.push_back(parts.m_index);
            }
        }

        skeletonSlot->m_skeleton = nullptr;
        skeletonSlot->m_jointIndices.clear();
        skeletonSlot->m_joints.clear();
        if (Internal::AdvanceGeneration(skeletonSlot->m_generation))
        {
            m_freeSkeletonDefinitionSlots.push_back(skeletonParts.m_index);
        }
        return true;
    }

    bool RuntimeImplementation::IsValid(
        const SkeletalAnimationHandle animationHandle) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        return FindSkeletalAnimationUnlocked(animationHandle);
    }

    bool RuntimeImplementation::GetSkeletalAnimationState(
        const SkeletalAnimationHandle animationHandle,
        SkeletalAnimationState& state) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletalAnimationSlot* slot = FindSkeletalAnimationUnlocked(animationHandle);
        if (!slot)
        {
            return false;
        }

        state = {
            .m_duration = slot->m_animation->GetDuration(),
            .m_jointCount = aznumeric_cast<AZ::u32>(slot->m_jointNames.size()),
            .m_isLooping = slot->m_animation->IsLooping(),
        };
        return true;
    }

    bool RuntimeImplementation::GetSkeletalAnimatedJointName(
        const SkeletalAnimationHandle animationHandle,
        const AZ::u32 jointIndex,
        AZ::Name& jointName) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletalAnimationSlot* slot = FindSkeletalAnimationUnlocked(animationHandle);
        if (!slot || jointIndex >= slot->m_jointNames.size())
        {
            return false;
        }

        jointName = slot->m_jointNames[jointIndex];
        return true;
    }

    QueryResult RuntimeImplementation::GetSkeletalAnimationKeyframes(
        const SkeletalAnimationHandle animationHandle,
        const AZ::u32 jointIndex,
        const AZStd::span<SkeletalAnimationKeyframe> keyframes) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletalAnimationSlot* slot = FindSkeletalAnimationUnlocked(animationHandle);
        if (!slot)
        {
            return {};
        }

        const JPH::SkeletalAnimation::AnimatedJointVector& nativeJoints =
            slot->m_animation->GetAnimatedJoints();
        if (jointIndex >= nativeJoints.size())
        {
            return {};
        }

        const JPH::SkeletalAnimation::KeyframeVector& nativeKeyframes =
            nativeJoints[jointIndex].mKeyframes;
        const size_t copyCount = AZStd::min(keyframes.size(), nativeKeyframes.size());
        for (size_t keyframeIndex = 0; keyframeIndex < copyCount; ++keyframeIndex)
        {
            const JPH::SkeletalAnimation::Keyframe& nativeKeyframe = nativeKeyframes[keyframeIndex];
            keyframes[keyframeIndex] = {
                .m_rotation = AZ::Quaternion(
                    nativeKeyframe.mRotation.GetX(),
                    nativeKeyframe.mRotation.GetY(),
                    nativeKeyframe.mRotation.GetZ(),
                    nativeKeyframe.mRotation.GetW()),
                .m_translation = AZ::Vector3(
                    nativeKeyframe.mTranslation.GetX(),
                    nativeKeyframe.mTranslation.GetY(),
                    nativeKeyframe.mTranslation.GetZ()),
                .m_time = nativeKeyframe.mTime,
            };
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(nativeKeyframes.size()),
        };
    }

    bool RuntimeImplementation::SetSkeletalAnimationLooping(
        const SkeletalAnimationHandle animationHandle,
        const bool isLooping)
    {
        AZStd::lock_guard lock(m_skeletonMutex);
        SkeletalAnimationSlot* slot = FindSkeletalAnimationUnlocked(animationHandle);
        if (!slot)
        {
            return false;
        }

        slot->m_animation->SetIsLooping(isLooping);
        return true;
    }

    bool RuntimeImplementation::ScaleSkeletalAnimation(
        const SkeletalAnimationHandle animationHandle,
        const float scale)
    {
        const DeterministicFloatScope floatScope;
        if (!AZ::IsFiniteFloat(scale) || scale <= 0.0f)
        {
            return false;
        }

        AZStd::lock_guard lock(m_skeletonMutex);
        SkeletalAnimationSlot* slot = FindSkeletalAnimationUnlocked(animationHandle);
        if (!slot)
        {
            return false;
        }

        slot->m_animation->ScaleJoints(scale);
        return true;
    }

    SkeletonPoseHandle RuntimeImplementation::CreateSkeletonPose(
        const SkeletonDefinitionHandle skeletonHandle)
    {
        const DeterministicFloatScope floatScope;
        AZStd::lock_guard lock(m_skeletonMutex);
        SkeletonDefinitionSlot* skeletonSlot = FindSkeletonDefinitionUnlocked(skeletonHandle);
        if (!skeletonSlot)
        {
            return {};
        }

        AZ::u32 poseIndex = 0;
        if (!m_freeSkeletonPoseSlots.empty())
        {
            poseIndex = m_freeSkeletonPoseSlots.back();
            m_freeSkeletonPoseSlots.pop_back();
        }
        else
        {
            if (m_skeletonPoseSlots.size() >= Internal::HandlePayloadMask)
            {
                return {};
            }
            poseIndex = aznumeric_cast<AZ::u32>(m_skeletonPoseSlots.size());
            m_skeletonPoseSlots.emplace_back();
        }

        SkeletonPoseSlot& slot = m_skeletonPoseSlots[poseIndex];
        slot.m_scratch = AZStd::make_shared<SkeletonPoseScratch>();
        slot.m_scratch->m_pose.SetSkeleton(skeletonSlot->m_skeleton);
        slot.m_scratch->m_pose.CalculateJointMatrices();
        slot.m_scratch->m_localTransforms.resize(skeletonSlot->m_joints.size());
        slot.m_skeletonHandle = skeletonHandle;
        ++skeletonSlot->m_poseCount;
        return Internal::MakeResourceHandle<SkeletonPoseHandle>(poseIndex, slot.m_generation);
    }

    bool RuntimeImplementation::DestroySkeletonPose(
        const SkeletonPoseHandle poseHandle)
    {
        AZStd::lock_guard lock(m_skeletonMutex);
        SkeletonPoseSlot* slot = FindSkeletonPoseUnlocked(poseHandle);
        if (!slot)
        {
            return false;
        }

        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(poseHandle, parts))
        {
            return false;
        }
        SkeletonDefinitionSlot* skeletonSlot = FindSkeletonDefinitionUnlocked(slot->m_skeletonHandle);
        AZ_Assert(skeletonSlot && skeletonSlot->m_poseCount > 0, "Skeleton pose ownership is inconsistent.");
        if (skeletonSlot && skeletonSlot->m_poseCount > 0)
        {
            --skeletonSlot->m_poseCount;
        }
        slot->m_scratch.reset();
        slot->m_skeletonHandle = SkeletonDefinitionHandle::Invalid;
        if (Internal::AdvanceGeneration(slot->m_generation))
        {
            m_freeSkeletonPoseSlots.push_back(parts.m_index);
        }
        return true;
    }

    bool RuntimeImplementation::IsValid(
        const SkeletonPoseHandle poseHandle) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        return FindSkeletonPoseUnlocked(poseHandle);
    }

    bool RuntimeImplementation::GetSkeletonPoseState(
        const SkeletonPoseHandle poseHandle,
        SkeletonPoseState& state) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonPoseSlot* slot = FindSkeletonPoseUnlocked(poseHandle);
        if (!slot)
        {
            return false;
        }

        SkeletonPoseScratch& scratch = *slot->m_scratch;
        AZStd::lock_guard scratchLock(scratch.m_mutex);
        const JPH::RVec3 rootOffset = scratch.m_pose.GetRootOffset();
        state = {
            .m_rootOffset = {
                .m_x = static_cast<double>(rootOffset.GetX()),
                .m_y = static_cast<double>(rootOffset.GetY()),
                .m_z = static_cast<double>(rootOffset.GetZ()),
            },
            .m_skeletonHandle = slot->m_skeletonHandle,
            .m_jointCount = scratch.m_pose.GetJointCount(),
        };
        return true;
    }

    bool RuntimeImplementation::SetSkeletonPoseRootOffset(
        const SkeletonPoseHandle poseHandle,
        const WorldPosition& rootOffset)
    {
        if (!std::isfinite(rootOffset.m_x)
            || !std::isfinite(rootOffset.m_y)
            || !std::isfinite(rootOffset.m_z))
        {
            return false;
        }

        AZStd::shared_lock lock(m_skeletonMutex);
        SkeletonPoseSlot* slot = FindSkeletonPoseUnlocked(poseHandle);
        if (!slot)
        {
            return false;
        }

        SkeletonPoseScratch& scratch = *slot->m_scratch;
        AZStd::lock_guard scratchLock(scratch.m_mutex);
        scratch.m_pose.SetRootOffset({
            static_cast<JPH::Real>(rootOffset.m_x),
            static_cast<JPH::Real>(rootOffset.m_y),
            static_cast<JPH::Real>(rootOffset.m_z),
        });
        return true;
    }

    bool RuntimeImplementation::SetSkeletonPoseLocalTransforms(
        const SkeletonPoseHandle poseHandle,
        const AZStd::span<const AZ::Transform> localTransforms)
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_skeletonMutex);
        SkeletonPoseSlot* slot = FindSkeletonPoseUnlocked(poseHandle);
        if (!slot || localTransforms.size() != slot->m_scratch->m_pose.GetJointCount())
        {
            return false;
        }

        SkeletonPoseScratch& scratch = *slot->m_scratch;
        AZStd::lock_guard scratchLock(scratch.m_mutex);
        for (const AZ::Transform& transform : localTransforms)
        {
            if (!IsValidRigidSkeletonTransform(transform))
            {
                return false;
            }
        }
        for (size_t jointIndex = 0; jointIndex < localTransforms.size(); ++jointIndex)
        {
            scratch.m_pose.GetJoint(aznumeric_cast<int>(jointIndex)).FromMatrix(
                ToNativeSkeletonTransform(localTransforms[jointIndex]));
        }
        scratch.m_pose.CalculateJointMatrices();
        return true;
    }

    bool RuntimeImplementation::SetSkeletonPoseModelTransforms(
        const SkeletonPoseHandle poseHandle,
        const AZStd::span<const AZ::Transform> modelTransforms)
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_skeletonMutex);
        SkeletonPoseSlot* slot = FindSkeletonPoseUnlocked(poseHandle);
        if (!slot || modelTransforms.size() != slot->m_scratch->m_pose.GetJointCount())
        {
            return false;
        }

        SkeletonPoseScratch& scratch = *slot->m_scratch;
        AZStd::lock_guard scratchLock(scratch.m_mutex);
        for (const AZ::Transform& transform : modelTransforms)
        {
            if (!IsValidRigidSkeletonTransform(transform))
            {
                return false;
            }
        }
        for (size_t jointIndex = 0; jointIndex < modelTransforms.size(); ++jointIndex)
        {
            scratch.m_pose.GetJointMatrix(aznumeric_cast<int>(jointIndex)) =
                ToNativeSkeletonTransform(modelTransforms[jointIndex]);
        }
        scratch.m_pose.CalculateJointStates();
        return true;
    }

    QueryResult RuntimeImplementation::GetSkeletonPoseLocalTransforms(
        const SkeletonPoseHandle poseHandle,
        const AZStd::span<AZ::Transform> localTransforms) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonPoseSlot* slot = FindSkeletonPoseUnlocked(poseHandle);
        if (!slot)
        {
            return {};
        }

        SkeletonPoseScratch& scratch = *slot->m_scratch;
        AZStd::lock_guard scratchLock(scratch.m_mutex);
        scratch.m_pose.CalculateLocalSpaceJointMatrices(scratch.m_localTransforms.data());
        const size_t copiedCount = AZStd::min(localTransforms.size(), scratch.m_localTransforms.size());
        for (size_t jointIndex = 0; jointIndex < copiedCount; ++jointIndex)
        {
            localTransforms[jointIndex] = FromNativeSkeletonTransform(scratch.m_localTransforms[jointIndex]);
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copiedCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(scratch.m_localTransforms.size()),
        };
    }

    QueryResult RuntimeImplementation::GetSkeletonPoseModelTransforms(
        const SkeletonPoseHandle poseHandle,
        const AZStd::span<AZ::Transform> modelTransforms) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonPoseSlot* slot = FindSkeletonPoseUnlocked(poseHandle);
        if (!slot)
        {
            return {};
        }

        SkeletonPoseScratch& scratch = *slot->m_scratch;
        AZStd::lock_guard scratchLock(scratch.m_mutex);
        const JPH::SkeletonPose::Mat44Vector& nativeTransforms = scratch.m_pose.GetJointMatrices();
        const size_t copiedCount = AZStd::min(modelTransforms.size(), nativeTransforms.size());
        for (size_t jointIndex = 0; jointIndex < copiedCount; ++jointIndex)
        {
            modelTransforms[jointIndex] = FromNativeSkeletonTransform(nativeTransforms[jointIndex]);
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copiedCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(nativeTransforms.size()),
        };
    }

    bool RuntimeImplementation::SampleSkeletalAnimation(
        const SkeletalAnimationHandle animationHandle,
        const SkeletonPoseHandle poseHandle,
        const float time)
    {
        const DeterministicFloatScope floatScope;
        if (!AZ::IsFiniteFloat(time) || time < 0.0f)
        {
            return false;
        }

        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletalAnimationSlot* animationSlot = FindSkeletalAnimationUnlocked(animationHandle);
        SkeletonPoseSlot* poseSlot = FindSkeletonPoseUnlocked(poseHandle);
        if (!animationSlot || !poseSlot)
        {
            return false;
        }

        SkeletonPoseScratch& scratch = *poseSlot->m_scratch;
        AZStd::lock_guard scratchLock(scratch.m_mutex);
        const SkeletonDefinitionSlot* skeletonSlot = FindSkeletonDefinitionUnlocked(poseSlot->m_skeletonHandle);
        AZ_Assert(skeletonSlot, "Skeleton pose ownership is inconsistent.");
        if (!skeletonSlot)
        {
            return false;
        }
        if (scratch.m_cachedAnimationHandle != animationHandle
            || scratch.m_cachedAnimationRevision != animationSlot->m_revision)
        {
            scratch.m_cachedAnimationHandle = SkeletalAnimationHandle::Invalid;
            scratch.m_cachedAnimationRevision = 0;
            scratch.m_animationJointIndices.clear();
            scratch.m_animationJointIndices.reserve(animationSlot->m_jointNames.size());
            for (const AZ::Name& jointName : animationSlot->m_jointNames)
            {
                const auto jointIterator = skeletonSlot->m_jointIndices.find(jointName);
                if (jointIterator == skeletonSlot->m_jointIndices.end())
                {
                    scratch.m_animationJointIndices.clear();
                    return false;
                }
                scratch.m_animationJointIndices.push_back(jointIterator->second);
            }
            scratch.m_cachedAnimationHandle = animationHandle;
            scratch.m_cachedAnimationRevision = animationSlot->m_revision;
        }

        float sampleTime = time;
        const float duration = animationSlot->m_animation->GetDuration();
        if (duration > 0.0f && animationSlot->m_animation->IsLooping())
        {
            sampleTime = std::fmod(sampleTime, duration);
        }
        const JPH::SkeletalAnimation::AnimatedJointVector& animatedJoints =
            animationSlot->m_animation->GetAnimatedJoints();
        for (size_t animatedJointIndex = 0; animatedJointIndex < animatedJoints.size(); ++animatedJointIndex)
        {
            const JPH::SkeletalAnimation::KeyframeVector& keyframes =
                animatedJoints[animatedJointIndex].mKeyframes;
            const auto upperKeyframe = AZStd::lower_bound(
                keyframes.begin(),
                keyframes.end(),
                sampleTime,
                [](const JPH::SkeletalAnimation::Keyframe& keyframe, const float targetTime)
                {
                    return keyframe.mTime < targetTime;
                });

            JPH::SkeletalAnimation::JointState sampledState;
            if (upperKeyframe == keyframes.begin())
            {
                sampledState = keyframes.front();
            }
            else if (upperKeyframe == keyframes.end())
            {
                sampledState = keyframes.back();
            }
            else
            {
                const JPH::SkeletalAnimation::Keyframe& secondKeyframe = *upperKeyframe;
                const JPH::SkeletalAnimation::Keyframe& firstKeyframe = *(upperKeyframe - 1);
                const float fraction =
                    (sampleTime - firstKeyframe.mTime) / (secondKeyframe.mTime - firstKeyframe.mTime);
                sampledState.mTranslation =
                    (1.0f - fraction) * firstKeyframe.mTranslation
                    + fraction * secondKeyframe.mTranslation;
                sampledState.mRotation = firstKeyframe.mRotation.SLERP(secondKeyframe.mRotation, fraction);
            }

            const AZ::u32 poseJointIndex = scratch.m_animationJointIndices[animatedJointIndex];
            scratch.m_pose.GetJoint(aznumeric_cast<int>(poseJointIndex)) = sampledState;
        }
        scratch.m_pose.CalculateJointMatrices();
        return true;
    }

    SkeletonMapperHandle RuntimeImplementation::CreateSkeletonMapper(
        const SkeletonMapperConfiguration& configuration)
    {
        const DeterministicFloatScope floatScope;
        AZStd::lock_guard lock(m_skeletonMutex);
        SkeletonDefinitionSlot* sourceSlot =
            FindSkeletonDefinitionUnlocked(configuration.m_sourceSkeletonHandle);
        SkeletonDefinitionSlot* targetSlot =
            FindSkeletonDefinitionUnlocked(configuration.m_targetSkeletonHandle);
        if (!sourceSlot
            || !targetSlot
            || sourceSlot->m_joints.size() > targetSlot->m_joints.size()
            || configuration.m_sourceNeutralModelTransforms.size() != sourceSlot->m_joints.size()
            || configuration.m_targetNeutralModelTransforms.size() != targetSlot->m_joints.size()
            || (!configuration.m_lockedTargetTranslations.empty()
                && configuration.m_lockedTargetTranslations.size() != targetSlot->m_joints.size()))
        {
            return {};
        }

        for (const AZ::Transform& transform : configuration.m_sourceNeutralModelTransforms)
        {
            if (!IsValidSkeletonTransform(transform))
            {
                return {};
            }
        }
        for (const AZ::Transform& transform : configuration.m_targetNeutralModelTransforms)
        {
            if (!IsValidSkeletonTransform(transform))
            {
                return {};
            }
        }
        for (size_t jointIndex = 0; jointIndex < configuration.m_lockedTargetTranslations.size(); ++jointIndex)
        {
            const AZ::u8 locked = configuration.m_lockedTargetTranslations[jointIndex];
            if (locked > 1
                || (locked == 1 && targetSlot->m_joints[jointIndex].m_parentIndex < 0))
            {
                return {};
            }
        }

        constexpr AZ::u32 InvalidJointIndex = AZStd::numeric_limits<AZ::u32>::max();
        AZStd::vector<AZ::u32> targetJointBySource(sourceSlot->m_joints.size(), InvalidJointIndex);
        AZStd::vector<AZ::s32> sourceJointByTarget(targetSlot->m_joints.size(), -1);
        if (configuration.m_jointMappings.empty())
        {
            for (size_t sourceJoint = 0; sourceJoint < sourceSlot->m_joints.size(); ++sourceJoint)
            {
                const auto targetJoint = targetSlot->m_jointIndices.find(sourceSlot->m_joints[sourceJoint].m_name);
                if (targetJoint == targetSlot->m_jointIndices.end())
                {
                    return {};
                }
                targetJointBySource[sourceJoint] = targetJoint->second;
            }
        }
        else
        {
            if (configuration.m_jointMappings.size() != sourceSlot->m_joints.size())
            {
                return {};
            }
            for (size_t mappingIndex = 0; mappingIndex < configuration.m_jointMappings.size(); ++mappingIndex)
            {
                const SkeletonJointMapping& mapping = configuration.m_jointMappings[mappingIndex];
                if (mapping.m_sourceJoint >= sourceSlot->m_joints.size()
                    || mapping.m_targetJoint >= targetSlot->m_joints.size())
                {
                    return {};
                }

                if (targetJointBySource[mapping.m_sourceJoint] != InvalidJointIndex)
                {
                    return {};
                }
                targetJointBySource[mapping.m_sourceJoint] = mapping.m_targetJoint;
            }
        }

        for (size_t sourceJoint = 0; sourceJoint < targetJointBySource.size(); ++sourceJoint)
        {
            const AZ::u32 targetJoint = targetJointBySource[sourceJoint];
            if (targetJoint == InvalidJointIndex || sourceJointByTarget[targetJoint] >= 0)
            {
                return {};
            }
            sourceJointByTarget[targetJoint] = aznumeric_cast<AZ::s32>(sourceJoint);
        }

        for (size_t sourceJoint = 0; sourceJoint < sourceSlot->m_joints.size(); ++sourceJoint)
        {
            AZ::s32 mappedSourceParent = -1;
            AZ::s32 targetParent = targetSlot->m_joints[targetJointBySource[sourceJoint]].m_parentIndex;
            while (targetParent >= 0)
            {
                mappedSourceParent = sourceJointByTarget[aznumeric_cast<size_t>(targetParent)];
                if (mappedSourceParent >= 0)
                {
                    break;
                }
                targetParent = targetSlot->m_joints[aznumeric_cast<size_t>(targetParent)].m_parentIndex;
            }
            if (mappedSourceParent != sourceSlot->m_joints[sourceJoint].m_parentIndex)
            {
                return {};
            }
        }

        JPH::Array<JPH::Mat44> sourceNeutralPose;
        sourceNeutralPose.reserve(configuration.m_sourceNeutralModelTransforms.size());
        for (const AZ::Transform& transform : configuration.m_sourceNeutralModelTransforms)
        {
            sourceNeutralPose.push_back(ToNativeSkeletonTransform(transform));
        }
        JPH::Array<JPH::Mat44> targetNeutralPose;
        targetNeutralPose.reserve(configuration.m_targetNeutralModelTransforms.size());
        for (const AZ::Transform& transform : configuration.m_targetNeutralModelTransforms)
        {
            targetNeutralPose.push_back(ToNativeSkeletonTransform(transform));
        }

        JPH::Ref<JPH::SkeletonMapper> mapper = new JPH::SkeletonMapper();
        mapper->Initialize(
            sourceSlot->m_skeleton,
            sourceNeutralPose.data(),
            targetSlot->m_skeleton,
            targetNeutralPose.data(),
            [&targetJointBySource](
                const JPH::Skeleton*,
                const int sourceJoint,
                const JPH::Skeleton*,
                const int targetJoint)
            {
                return sourceJoint >= 0
                    && aznumeric_cast<size_t>(sourceJoint) < targetJointBySource.size()
                    && targetJoint >= 0
                    && targetJointBySource[aznumeric_cast<size_t>(sourceJoint)]
                        == aznumeric_cast<AZ::u32>(targetJoint);
            });
        if (mapper->GetMappings().size() != sourceSlot->m_joints.size())
        {
            return {};
        }

        if (configuration.m_lockAllTargetTranslations)
        {
            mapper->LockAllTranslations(targetSlot->m_skeleton, targetNeutralPose.data());
        }
        else if (!configuration.m_lockedTargetTranslations.empty())
        {
            AZStd::unique_ptr<bool[]> lockedTranslations =
                AZStd::make_unique<bool[]>(configuration.m_lockedTargetTranslations.size());
            for (size_t jointIndex = 0; jointIndex < configuration.m_lockedTargetTranslations.size(); ++jointIndex)
            {
                lockedTranslations[jointIndex] = configuration.m_lockedTargetTranslations[jointIndex] != 0;
            }
            mapper->LockTranslations(
                targetSlot->m_skeleton,
                lockedTranslations.get(),
                targetNeutralPose.data());
        }

        AZ::u32 mapperIndex = 0;
        if (!m_freeSkeletonMapperSlots.empty())
        {
            mapperIndex = m_freeSkeletonMapperSlots.back();
            m_freeSkeletonMapperSlots.pop_back();
        }
        else
        {
            if (m_skeletonMapperSlots.size() >= Internal::HandlePayloadMask)
            {
                return {};
            }
            mapperIndex = aznumeric_cast<AZ::u32>(m_skeletonMapperSlots.size());
            m_skeletonMapperSlots.emplace_back();
        }

        SkeletonMapperSlot& slot = m_skeletonMapperSlots[mapperIndex];
        slot.m_mapper = mapper;
        slot.m_sourceSkeletonHandle = configuration.m_sourceSkeletonHandle;
        slot.m_targetSkeletonHandle = configuration.m_targetSkeletonHandle;
        slot.m_sourceJointCount = aznumeric_cast<AZ::u32>(sourceSlot->m_joints.size());
        slot.m_targetJointCount = aznumeric_cast<AZ::u32>(targetSlot->m_joints.size());
        slot.m_scratch = AZStd::make_shared<SkeletonMapperScratch>();
        slot.m_scratch->m_sourceTransforms.resize(slot.m_sourceJointCount);
        slot.m_scratch->m_targetLocalTransforms.resize(slot.m_targetJointCount);
        slot.m_scratch->m_targetModelTransforms.resize(slot.m_targetJointCount);
        ++sourceSlot->m_mapperCount;
        ++targetSlot->m_mapperCount;
        return Internal::MakeResourceHandle<SkeletonMapperHandle>(mapperIndex, slot.m_generation);
    }

    bool RuntimeImplementation::DestroySkeletonMapper(
        const SkeletonMapperHandle mapperHandle)
    {
        AZStd::lock_guard lock(m_skeletonMutex);
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(mapperHandle, parts)
            || parts.m_index >= m_skeletonMapperSlots.size())
        {
            return false;
        }

        SkeletonMapperSlot& slot = m_skeletonMapperSlots[parts.m_index];
        if (!slot.m_mapper || slot.m_generation != parts.m_generation)
        {
            return false;
        }

        --FindSkeletonDefinitionUnlocked(slot.m_sourceSkeletonHandle)->m_mapperCount;
        --FindSkeletonDefinitionUnlocked(slot.m_targetSkeletonHandle)->m_mapperCount;
        slot.m_mapper = nullptr;
        slot.m_scratch.reset();
        slot.m_sourceSkeletonHandle = {};
        slot.m_targetSkeletonHandle = {};
        slot.m_sourceJointCount = 0;
        slot.m_targetJointCount = 0;
        if (Internal::AdvanceGeneration(slot.m_generation))
        {
            m_freeSkeletonMapperSlots.push_back(parts.m_index);
        }
        return true;
    }

    bool RuntimeImplementation::IsValid(
        const SkeletonMapperHandle mapperHandle) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        return FindSkeletonMapperUnlocked(mapperHandle);
    }

    bool RuntimeImplementation::GetSkeletonMapperState(
        const SkeletonMapperHandle mapperHandle,
        SkeletonMapperState& state) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonMapperSlot* slot = FindSkeletonMapperUnlocked(mapperHandle);
        if (!slot)
        {
            return false;
        }

        state = {
            .m_sourceSkeletonHandle = slot->m_sourceSkeletonHandle,
            .m_targetSkeletonHandle = slot->m_targetSkeletonHandle,
            .m_chainCount = aznumeric_cast<AZ::u32>(slot->m_mapper->GetChains().size()),
            .m_lockedTranslationCount =
                aznumeric_cast<AZ::u32>(slot->m_mapper->GetLockedTranslations().size()),
            .m_mappingCount = aznumeric_cast<AZ::u32>(slot->m_mapper->GetMappings().size()),
            .m_sourceJointCount = slot->m_sourceJointCount,
            .m_targetJointCount = slot->m_targetJointCount,
            .m_unmappedJointCount = aznumeric_cast<AZ::u32>(slot->m_mapper->GetUnmapped().size()),
        };
        return true;
    }

    QueryResult RuntimeImplementation::GetSkeletonMapperMappings(
        const SkeletonMapperHandle mapperHandle,
        const AZStd::span<SkeletonMapperMappingState> mappings) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonMapperSlot* slot = FindSkeletonMapperUnlocked(mapperHandle);
        if (!slot)
        {
            return {};
        }

        const JPH::SkeletonMapper::MappingVector& nativeMappings = slot->m_mapper->GetMappings();
        const size_t copyCount = AZStd::min(nativeMappings.size(), mappings.size());
        for (size_t mappingIndex = 0; mappingIndex < copyCount; ++mappingIndex)
        {
            const JPH::SkeletonMapper::Mapping& nativeMapping = nativeMappings[mappingIndex];
            mappings[mappingIndex] = {
                .m_sourceToTarget = FromNativeSkeletonTransform(nativeMapping.mJoint1To2),
                .m_targetToSource = FromNativeSkeletonTransform(nativeMapping.mJoint2To1),
                .m_sourceJointIndex = aznumeric_cast<AZ::u32>(nativeMapping.mJointIdx1),
                .m_targetJointIndex = aznumeric_cast<AZ::u32>(nativeMapping.mJointIdx2),
            };
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(nativeMappings.size()),
        };
    }

    bool RuntimeImplementation::GetSkeletonMapperChainState(
        const SkeletonMapperHandle mapperHandle,
        const AZ::u32 chainIndex,
        SkeletonMapperChainState& state) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonMapperSlot* slot = FindSkeletonMapperUnlocked(mapperHandle);
        if (!slot || chainIndex >= slot->m_mapper->GetChains().size())
        {
            return false;
        }

        const JPH::SkeletonMapper::Chain& chain = slot->m_mapper->GetChains()[chainIndex];
        state = {
            .m_sourceJointCount = aznumeric_cast<AZ::u32>(chain.mJointIndices1.size()),
            .m_targetJointCount = aznumeric_cast<AZ::u32>(chain.mJointIndices2.size()),
        };
        return true;
    }

    QueryResult RuntimeImplementation::GetSkeletonMapperSourceChain(
        const SkeletonMapperHandle mapperHandle,
        const AZ::u32 chainIndex,
        const AZStd::span<AZ::u32> jointIndices) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonMapperSlot* slot = FindSkeletonMapperUnlocked(mapperHandle);
        if (!slot || chainIndex >= slot->m_mapper->GetChains().size())
        {
            return {};
        }

        return CopySkeletonJointIndices(
            slot->m_mapper->GetChains()[chainIndex].mJointIndices1,
            jointIndices);
    }

    QueryResult RuntimeImplementation::GetSkeletonMapperTargetChain(
        const SkeletonMapperHandle mapperHandle,
        const AZ::u32 chainIndex,
        const AZStd::span<AZ::u32> jointIndices) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonMapperSlot* slot = FindSkeletonMapperUnlocked(mapperHandle);
        if (!slot || chainIndex >= slot->m_mapper->GetChains().size())
        {
            return {};
        }

        return CopySkeletonJointIndices(
            slot->m_mapper->GetChains()[chainIndex].mJointIndices2,
            jointIndices);
    }

    QueryResult RuntimeImplementation::GetSkeletonMapperUnmappedJoints(
        const SkeletonMapperHandle mapperHandle,
        const AZStd::span<SkeletonMapperUnmappedJoint> joints) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonMapperSlot* slot = FindSkeletonMapperUnlocked(mapperHandle);
        if (!slot)
        {
            return {};
        }

        const JPH::SkeletonMapper::UnmappedVector& nativeJoints = slot->m_mapper->GetUnmapped();
        const size_t copyCount = AZStd::min(nativeJoints.size(), joints.size());
        for (size_t jointIndex = 0; jointIndex < copyCount; ++jointIndex)
        {
            joints[jointIndex] = {
                .m_jointIndex = aznumeric_cast<AZ::s32>(nativeJoints[jointIndex].mJointIdx),
                .m_parentJointIndex = aznumeric_cast<AZ::s32>(nativeJoints[jointIndex].mParentJointIdx),
            };
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(nativeJoints.size()),
        };
    }

    QueryResult RuntimeImplementation::GetSkeletonMapperLockedTranslations(
        const SkeletonMapperHandle mapperHandle,
        const AZStd::span<SkeletonMapperLockedTranslation> translations) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonMapperSlot* slot = FindSkeletonMapperUnlocked(mapperHandle);
        if (!slot)
        {
            return {};
        }

        const JPH::SkeletonMapper::LockedVector& nativeTranslations =
            slot->m_mapper->GetLockedTranslations();
        const size_t copyCount = AZStd::min(nativeTranslations.size(), translations.size());
        for (size_t translationIndex = 0; translationIndex < copyCount; ++translationIndex)
        {
            const JPH::SkeletonMapper::Locked& nativeTranslation = nativeTranslations[translationIndex];
            translations[translationIndex] = {
                .m_translation = AZ::Vector3(
                    nativeTranslation.mTranslation.GetX(),
                    nativeTranslation.mTranslation.GetY(),
                    nativeTranslation.mTranslation.GetZ()),
                .m_jointIndex = aznumeric_cast<AZ::s32>(nativeTranslation.mJointIdx),
                .m_parentJointIndex = aznumeric_cast<AZ::s32>(nativeTranslation.mParentJointIdx),
            };
        }
        return {
            .m_hitCount = aznumeric_cast<AZ::u32>(copyCount),
            .m_requiredHitCount = aznumeric_cast<AZ::u32>(nativeTranslations.size()),
        };
    }

    bool RuntimeImplementation::GetMappedSkeletonJoint(
        const SkeletonMapperHandle mapperHandle,
        const AZ::u32 sourceJointIndex,
        AZ::u32& targetJointIndex) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonMapperSlot* slot = FindSkeletonMapperUnlocked(mapperHandle);
        if (!slot || sourceJointIndex >= slot->m_sourceJointCount)
        {
            return false;
        }

        const int nativeTargetIndex = slot->m_mapper->GetMappedJointIdx(
            aznumeric_cast<int>(sourceJointIndex));
        if (nativeTargetIndex < 0)
        {
            return false;
        }

        targetJointIndex = aznumeric_cast<AZ::u32>(nativeTargetIndex);
        return true;
    }

    bool RuntimeImplementation::IsSkeletonJointTranslationLocked(
        const SkeletonMapperHandle mapperHandle,
        const AZ::u32 targetJointIndex,
        bool& locked) const
    {
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonMapperSlot* slot = FindSkeletonMapperUnlocked(mapperHandle);
        if (!slot || targetJointIndex >= slot->m_targetJointCount)
        {
            return false;
        }

        locked = slot->m_mapper->IsJointTranslationLocked(
            aznumeric_cast<int>(targetJointIndex));
        return true;
    }

    bool RuntimeImplementation::MapSkeletonPose(
        const SkeletonMapperHandle mapperHandle,
        const AZStd::span<const AZ::Transform> sourceModelTransforms,
        const AZStd::span<const AZ::Transform> targetLocalTransforms,
        AZStd::span<AZ::Transform> targetModelTransforms) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonMapperSlot* constSlot = FindSkeletonMapperUnlocked(mapperHandle);
        if (!constSlot
            || sourceModelTransforms.size() != constSlot->m_sourceJointCount
            || targetLocalTransforms.size() != constSlot->m_targetJointCount
            || targetModelTransforms.size() < constSlot->m_targetJointCount)
        {
            return false;
        }

        SkeletonMapperScratch& scratch = *constSlot->m_scratch;
        AZStd::lock_guard scratchLock(scratch.m_mutex);
        for (size_t jointIndex = 0; jointIndex < sourceModelTransforms.size(); ++jointIndex)
        {
            if (!IsValidSkeletonTransform(sourceModelTransforms[jointIndex]))
            {
                return false;
            }
            scratch.m_sourceTransforms[jointIndex] = ToNativeSkeletonTransform(sourceModelTransforms[jointIndex]);
        }
        for (size_t jointIndex = 0; jointIndex < targetLocalTransforms.size(); ++jointIndex)
        {
            if (!IsValidSkeletonTransform(targetLocalTransforms[jointIndex]))
            {
                return false;
            }
            scratch.m_targetLocalTransforms[jointIndex] =
                ToNativeSkeletonTransform(targetLocalTransforms[jointIndex]);
        }

        constSlot->m_mapper->Map(
            scratch.m_sourceTransforms.data(),
            scratch.m_targetLocalTransforms.data(),
            scratch.m_targetModelTransforms.data());
        for (size_t jointIndex = 0; jointIndex < constSlot->m_targetJointCount; ++jointIndex)
        {
            targetModelTransforms[jointIndex] =
                FromNativeSkeletonTransform(scratch.m_targetModelTransforms[jointIndex]);
        }
        return true;
    }

    bool RuntimeImplementation::MapSkeletonPoseReverse(
        const SkeletonMapperHandle mapperHandle,
        const AZStd::span<const AZ::Transform> targetModelTransforms,
        AZStd::span<AZ::Transform> sourceModelTransforms) const
    {
        const DeterministicFloatScope floatScope;
        AZStd::shared_lock lock(m_skeletonMutex);
        const SkeletonMapperSlot* constSlot = FindSkeletonMapperUnlocked(mapperHandle);
        if (!constSlot
            || targetModelTransforms.size() != constSlot->m_targetJointCount
            || sourceModelTransforms.size() < constSlot->m_sourceJointCount)
        {
            return false;
        }

        SkeletonMapperScratch& scratch = *constSlot->m_scratch;
        AZStd::lock_guard scratchLock(scratch.m_mutex);
        for (size_t jointIndex = 0; jointIndex < targetModelTransforms.size(); ++jointIndex)
        {
            if (!IsValidSkeletonTransform(targetModelTransforms[jointIndex]))
            {
                return false;
            }
            scratch.m_targetModelTransforms[jointIndex] =
                ToNativeSkeletonTransform(targetModelTransforms[jointIndex]);
        }

        constSlot->m_mapper->MapReverse(
            scratch.m_targetModelTransforms.data(),
            scratch.m_sourceTransforms.data());
        for (size_t jointIndex = 0; jointIndex < constSlot->m_sourceJointCount; ++jointIndex)
        {
            sourceModelTransforms[jointIndex] =
                FromNativeSkeletonTransform(scratch.m_sourceTransforms[jointIndex]);
        }
        return true;
    }

    RuntimeImplementation::SkeletonDefinitionSlot* RuntimeImplementation::FindSkeletonDefinitionUnlocked(
        const SkeletonDefinitionHandle skeletonHandle)
    {
        return const_cast<SkeletonDefinitionSlot*>(
            static_cast<const RuntimeImplementation*>(this)->FindSkeletonDefinitionUnlocked(skeletonHandle));
    }

    const RuntimeImplementation::SkeletonDefinitionSlot* RuntimeImplementation::FindSkeletonDefinitionUnlocked(
        const SkeletonDefinitionHandle skeletonHandle) const
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(skeletonHandle, parts)
            || parts.m_index >= m_skeletonDefinitionSlots.size())
        {
            return nullptr;
        }

        const SkeletonDefinitionSlot& slot = m_skeletonDefinitionSlots[parts.m_index];
        if (!slot.m_skeleton || slot.m_generation != parts.m_generation)
        {
            return nullptr;
        }
        return &slot;
    }

    bool RuntimeImplementation::AcquireSkeletonDefinition(
        const SkeletonDefinitionHandle skeletonHandle,
        JPH::Ref<JPH::Skeleton>& skeleton)
    {
        AZStd::lock_guard lock(m_skeletonMutex);
        SkeletonDefinitionSlot* slot = FindSkeletonDefinitionUnlocked(skeletonHandle);
        if (!slot)
        {
            return false;
        }

        ++slot->m_ragdollDefinitionCount;
        skeleton = slot->m_skeleton;
        return true;
    }

    void RuntimeImplementation::ReleaseSkeletonDefinition(
        const SkeletonDefinitionHandle skeletonHandle)
    {
        AZStd::lock_guard lock(m_skeletonMutex);
        SkeletonDefinitionSlot* slot = FindSkeletonDefinitionUnlocked(skeletonHandle);
        AZ_Assert(
            slot && slot->m_ragdollDefinitionCount > 0,
            "Ragdoll skeleton ownership is inconsistent.");
        if (slot && slot->m_ragdollDefinitionCount > 0)
        {
            --slot->m_ragdollDefinitionCount;
        }
    }

    const RuntimeImplementation::SkeletonMapperSlot* RuntimeImplementation::FindSkeletonMapperUnlocked(
        const SkeletonMapperHandle mapperHandle) const
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(mapperHandle, parts)
            || parts.m_index >= m_skeletonMapperSlots.size())
        {
            return nullptr;
        }

        const SkeletonMapperSlot& slot = m_skeletonMapperSlots[parts.m_index];
        if (!slot.m_mapper || slot.m_generation != parts.m_generation)
        {
            return nullptr;
        }
        return &slot;
    }

    RuntimeImplementation::SkeletalAnimationSlot* RuntimeImplementation::FindSkeletalAnimationUnlocked(
        const SkeletalAnimationHandle animationHandle)
    {
        return const_cast<SkeletalAnimationSlot*>(
            static_cast<const RuntimeImplementation&>(*this).FindSkeletalAnimationUnlocked(animationHandle));
    }

    const RuntimeImplementation::SkeletalAnimationSlot* RuntimeImplementation::FindSkeletalAnimationUnlocked(
        const SkeletalAnimationHandle animationHandle) const
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(animationHandle, parts)
            || parts.m_index >= m_skeletalAnimationSlots.size())
        {
            return nullptr;
        }

        const SkeletalAnimationSlot& slot = m_skeletalAnimationSlots[parts.m_index];
        if (!slot.m_animation || slot.m_generation != parts.m_generation)
        {
            return nullptr;
        }
        return &slot;
    }

    RuntimeImplementation::SkeletonPoseSlot* RuntimeImplementation::FindSkeletonPoseUnlocked(
        const SkeletonPoseHandle poseHandle)
    {
        return const_cast<SkeletonPoseSlot*>(
            static_cast<const RuntimeImplementation&>(*this).FindSkeletonPoseUnlocked(poseHandle));
    }

    const RuntimeImplementation::SkeletonPoseSlot* RuntimeImplementation::FindSkeletonPoseUnlocked(
        const SkeletonPoseHandle poseHandle) const
    {
        Internal::ResourceHandleParts parts;
        if (!Internal::DecodeResourceHandle(poseHandle, parts)
            || parts.m_index >= m_skeletonPoseSlots.size())
        {
            return nullptr;
        }

        const SkeletonPoseSlot& slot = m_skeletonPoseSlots[parts.m_index];
        if (!slot.m_scratch || slot.m_generation != parts.m_generation)
        {
            return nullptr;
        }
        return &slot;
    }
} // namespace Jolt
