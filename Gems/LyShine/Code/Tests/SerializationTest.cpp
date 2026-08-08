/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "LyShineTest.h"
#include <Animation/AnimSplineTrack.h>
#include <LyShine/Animation/IUiAnimation.h>
#include <LyShine/UiRenderFormats.h>
#include <UiCanvasComponent.h>
#include <UiCanvasFileObject.h>
#include <UiSerialize.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/Math/MathReflection.h>
#include <AzCore/Serialization/DataPatch.h>
#include <AzCore/Utils/Utils.h>
#include <AzFramework/Asset/SimpleAsset.h>
#include <LmbrCentral/Rendering/TextureAsset.h>
#include <Range.h>

#include <array>
#include <cstring>

namespace UnitTest
{
    namespace
    {
        AZ::u32 FloatBits(float value)
        {
            AZ::u32 bits = 0;
            static_assert(sizeof(bits) == sizeof(value));
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        }

        void AppendTrackManifest(AZStd::vector<AZStd::string>& manifest, IUiAnimTrack* track, AZStd::string_view path)
        {
            ASSERT_NE(track, nullptr);
            const CUiAnimParamType& parameterType = track->GetParameterType();
            const UiAnimParamData& parameterData = track->GetParamData();
            // UiCompoundSplineTrack does not serialize or initialize its m_valueType in the
            // serialization constructor. Its child tracks carry the meaningful value types, so
            // avoid reading that indeterminate parent field in the evidence manifest.
            const AZStd::string valueType = track->GetSubTrackCount() == 0
                ? AZStd::string::format("%u", aznumeric_cast<unsigned int>(track->GetValueType()))
                : "compound";
            manifest.push_back(
                AZStd::string::format(
                    "%.*s|track|curve=%u|value=%s|flags=%d|param=%d:%s|component=%llu|type=%s|field=%s|offset=%zu|keys=%d|sub=%d",
                    aznumeric_cast<int>(path.size()),
                    path.data(),
                    aznumeric_cast<unsigned int>(track->GetCurveType()),
                    valueType.c_str(),
                    track->GetFlags(),
                    aznumeric_cast<int>(parameterType.GetType()),
                    parameterType.GetName(),
                    aznumeric_cast<unsigned long long>(parameterData.GetComponentId()),
                    parameterData.GetTypeId().ToString<AZStd::string>().c_str(),
                    parameterData.GetName(),
                    parameterData.GetOffset(),
                    track->GetNumKeys(),
                    track->GetSubTrackCount()));

            if (ISplineInterpolator* spline = track->GetSpline())
            {
                const int dimensionCount = spline->GetNumDimensions();
                manifest.push_back(
                    AZStd::string::format(
                        "%.*s|spline|dimensions=%d|keys=%d",
                        aznumeric_cast<int>(path.size()),
                        path.data(),
                        dimensionCount,
                        spline->GetKeyCount()));
                for (int splineKeyIndex = 0; splineKeyIndex < spline->GetKeyCount(); ++splineKeyIndex)
                {
                    ISplineInterpolator::ValueType value = {};
                    ISplineInterpolator::ValueType inTangent = {};
                    ISplineInterpolator::ValueType outTangent = {};
                    ASSERT_TRUE(spline->GetKeyValue(splineKeyIndex, value));
                    ASSERT_TRUE(spline->GetKeyTangents(splineKeyIndex, inTangent, outTangent));
                    manifest.push_back(
                        AZStd::string::format(
                            "%.*s|spline-key=%d|time=%08x|flags=%d|value=%08x,%08x,%08x,%08x|in=%08x,%08x,%08x,%08x|out=%08x,%08x,%08x,%08x",
                            aznumeric_cast<int>(path.size()),
                            path.data(),
                            splineKeyIndex,
                            FloatBits(spline->GetKeyTime(splineKeyIndex)),
                            spline->GetKeyFlags(splineKeyIndex),
                            FloatBits(value[0]),
                            FloatBits(value[1]),
                            FloatBits(value[2]),
                            FloatBits(value[3]),
                            FloatBits(inTangent[0]),
                            FloatBits(inTangent[1]),
                            FloatBits(inTangent[2]),
                            FloatBits(inTangent[3]),
                            FloatBits(outTangent[0]),
                            FloatBits(outTangent[1]),
                            FloatBits(outTangent[2]),
                            FloatBits(outTangent[3])));

                    // The Vector2 spline intentionally exposes only its Y component through the
                    // one-dimensional ISplineInterpolator API. Record the underlying serialized
                    // X/Y values and tangents as well; X participates in time-warp interpolation.
                    if (auto* vector2Spline = dynamic_cast<UiSpline::TrackSplineInterpolator<AZ::Vector2>*>(spline))
                    {
                        const UiSpline::SplineKeyEx<AZ::Vector2>& rawKey = vector2Spline->key(splineKeyIndex);
                        manifest.push_back(
                            AZStd::string::format(
                                "%.*s|spline-key-raw-v2=%d|value=%08x,%08x|in=%08x,%08x|out=%08x,%08x",
                                aznumeric_cast<int>(path.size()),
                                path.data(),
                                splineKeyIndex,
                                FloatBits(rawKey.value.GetX()),
                                FloatBits(rawKey.value.GetY()),
                                FloatBits(rawKey.ds.GetX()),
                                FloatBits(rawKey.ds.GetY()),
                                FloatBits(rawKey.dd.GetX()),
                                FloatBits(rawKey.dd.GetY())));
                    }
                }

                // Interpolation can lazily sort keys and derive tangents. Sample only after the
                // complete raw snapshot above, and restore a spline backup after every sample so
                // evidence gathering cannot alter later records or the subsequent serialization.
                for (int splineKeyIndex = 0; splineKeyIndex + 1 < spline->GetKeyCount(); ++splineKeyIndex)
                {
                    const float startTime = spline->GetKeyTime(splineKeyIndex);
                    const float endTime = spline->GetKeyTime(splineKeyIndex + 1);
                    if (endTime > startTime)
                    {
                        const float sampleTime = startTime + ((endTime - startTime) * 0.5f);
                        ISplineInterpolator::ValueType sample = {};
                        ISplineBackup* backup = spline->Backup();
                        ASSERT_NE(backup, nullptr);
                        backup->AddRef();
                        spline->Interpolate(sampleTime, sample);
                        spline->Restore(backup);
                        backup->Release();
                        manifest.push_back(
                            AZStd::string::format(
                                "%.*s|spline-sample=%d|time=%08x|value=%08x,%08x,%08x,%08x",
                                aznumeric_cast<int>(path.size()),
                                path.data(),
                                splineKeyIndex,
                                FloatBits(sampleTime),
                                FloatBits(sample[0]),
                                FloatBits(sample[1]),
                                FloatBits(sample[2]),
                                FloatBits(sample[3])));
                    }
                }
            }

            // Compound tracks expose their child-key count but intentionally do not implement the
            // raw key accessors. Their child tracks are recorded recursively below.
            for (int keyIndex = 0; track->GetSubTrackCount() == 0 && keyIndex < track->GetNumKeys(); ++keyIndex)
            {
                const float time = track->GetKeyTime(keyIndex);
                AZStd::string keyManifest = AZStd::string::format(
                    "%.*s|key=%d|time=%08x|flags=%d",
                    aznumeric_cast<int>(path.size()),
                    path.data(),
                    keyIndex,
                    FloatBits(time),
                    track->GetKeyFlags(keyIndex));

                switch (track->GetValueType())
                {
                case eUiAnimValue_Float:
                case eUiAnimValue_DiscreteFloat:
                    {
                        float value = 0.0f;
                        track->GetValue(time, value);
                        keyManifest += AZStd::string::format("|f=%08x", FloatBits(value));
                        break;
                    }
                case eUiAnimValue_Bool:
                    {
                        bool value = false;
                        track->GetValue(time, value);
                        keyManifest += AZStd::string::format("|b=%d", value ? 1 : 0);
                        break;
                    }
                case eUiAnimValue_Vector2:
                    {
                        AZ::Vector2 value = AZ::Vector2::CreateZero();
                        track->GetValue(time, value);
                        keyManifest += AZStd::string::format("|v2=%08x,%08x", FloatBits(value.GetX()), FloatBits(value.GetY()));
                        break;
                    }
                case eUiAnimValue_Vector:
                case eUiAnimValue_Vector3:
                    {
                        AZ::Vector3 value = AZ::Vector3::CreateZero();
                        track->GetValue(time, value);
                        keyManifest += AZStd::string::format(
                            "|v3=%08x,%08x,%08x", FloatBits(value.GetX()), FloatBits(value.GetY()), FloatBits(value.GetZ()));
                        break;
                    }
                case eUiAnimValue_Vector4:
                    {
                        AZ::Vector4 value = AZ::Vector4::CreateZero();
                        track->GetValue(time, value);
                        keyManifest += AZStd::string::format(
                            "|v4=%08x,%08x,%08x,%08x",
                            FloatBits(value.GetX()),
                            FloatBits(value.GetY()),
                            FloatBits(value.GetZ()),
                            FloatBits(value.GetW()));
                        break;
                    }
                case eUiAnimValue_Quat:
                    {
                        AZ::Quaternion value = AZ::Quaternion::CreateIdentity();
                        track->GetValue(time, value);
                        keyManifest += AZStd::string::format(
                            "|q=%08x,%08x,%08x,%08x",
                            FloatBits(value.GetX()),
                            FloatBits(value.GetY()),
                            FloatBits(value.GetZ()),
                            FloatBits(value.GetW()));
                        break;
                    }
                case eUiAnimValue_RGB:
                    {
                        AZ::Color value = AZ::Colors::Black;
                        track->GetValue(time, value);
                        keyManifest += AZStd::string::format(
                            "|c=%08x,%08x,%08x,%08x",
                            FloatBits(value.GetR()),
                            FloatBits(value.GetG()),
                            FloatBits(value.GetB()),
                            FloatBits(value.GetA()));
                        break;
                    }
                default:
                    break;
                }
                manifest.push_back(AZStd::move(keyManifest));
            }

            for (int subTrackIndex = 0; subTrackIndex < track->GetSubTrackCount(); ++subTrackIndex)
            {
                AppendTrackManifest(
                    manifest,
                    track->GetSubTrack(subTrackIndex),
                    AZStd::string::format("%.*s/sub%d", aznumeric_cast<int>(path.size()), path.data(), subTrackIndex));
            }
        }

        AZStd::vector<AZStd::string> BuildAnimationManifest(UiCanvasFileObject* canvas)
        {
            AZStd::vector<AZStd::string> manifest;
            UiCanvasComponent* canvasComponent = canvas->m_canvasEntity->FindComponent<UiCanvasComponent>();
            EXPECT_NE(canvasComponent, nullptr);
            if (!canvasComponent)
            {
                return manifest;
            }

            IUiAnimationSystem* animationSystem = canvasComponent->GetAnimationSystem();
            EXPECT_NE(animationSystem, nullptr);
            if (!animationSystem)
            {
                return manifest;
            }

            manifest.push_back(AZStd::string::format("sequences=%d", animationSystem->GetNumSequences()));
            for (int sequenceIndex = 0; sequenceIndex < animationSystem->GetNumSequences(); ++sequenceIndex)
            {
                IUiAnimSequence* sequence = animationSystem->GetSequence(sequenceIndex);
                EXPECT_NE(sequence, nullptr);
                if (!sequence)
                {
                    continue;
                }
                const Range sequenceTimeRange = sequence->GetTimeRange();
                manifest.push_back(
                    AZStd::string::format(
                        "sequence=%d|name=%s|id=%u|flags=%d|start=%08x|end=%08x|nodes=%d|events=%d",
                        sequenceIndex,
                        sequence->GetName(),
                        sequence->GetId(),
                        sequence->GetFlags(),
                        FloatBits(sequenceTimeRange.start),
                        FloatBits(sequenceTimeRange.end),
                        sequence->GetNodeCount(),
                        sequence->GetTrackEventsCount()));
                for (int eventIndex = 0; eventIndex < sequence->GetTrackEventsCount(); ++eventIndex)
                {
                    manifest.push_back(
                        AZStd::string::format(
                            "sequence=%d|event=%d|name=%s", sequenceIndex, eventIndex, sequence->GetTrackEvent(eventIndex)));
                }
                for (int nodeIndex = 0; nodeIndex < sequence->GetNodeCount(); ++nodeIndex)
                {
                    IUiAnimNode* node = sequence->GetNode(nodeIndex);
                    EXPECT_NE(node, nullptr);
                    if (!node)
                    {
                        continue;
                    }
                    manifest.push_back(
                        AZStd::string::format(
                            "sequence=%d|node=%d|name=%s|type=%d|flags=%d|tracks=%d",
                            sequenceIndex,
                            nodeIndex,
                            node->GetName().c_str(),
                            aznumeric_cast<int>(node->GetType()),
                            node->GetFlags(),
                            node->GetTrackCount()));
                    for (int trackIndex = 0; trackIndex < node->GetTrackCount(); ++trackIndex)
                    {
                        AppendTrackManifest(
                            manifest,
                            node->GetTrackByIndex(trackIndex),
                            AZStd::string::format("sequence=%d/node=%d/track=%d", sequenceIndex, nodeIndex, trackIndex));
                    }
                }
            }
            return manifest;
        }
    } // namespace

    class LyShineSerializationTest : public LyShineTest
    {
    protected:
        void SetupApplication() override
        {
            if (!AZ::Data::AssetManager::IsReady())
            {
                AZ::Data::AssetManager::Descriptor descriptor;
                AZ::Data::AssetManager::Create(descriptor);
                m_createdAssetManager = true;
            }

            AZ::ComponentApplication::Descriptor appDesc;
            appDesc.m_memoryBlocksByteSize = 10 * 1024 * 1024;
            appDesc.m_recordingMode = AZ::Debug::AllocationRecords::Mode::RECORD_FULL;

            AZ::ComponentApplication::StartupParameters appStartup;
            appStartup.m_createStaticModulesCallback = [](AZStd::vector<AZ::Module*>& modules)
            {
                modules.emplace_back(new LyShine::LyShineModule);
            };

            m_application = aznew AZ::ComponentApplication();
            m_systemEntity = m_application->Create(appDesc, appStartup);
            m_systemEntity->Init();
            m_systemEntity->Activate();

            AZ::SerializeContext& context = *m_application->GetSerializeContext();
            if (!context.FindClassData(azrtti_typeid<AZ::DataPatch>()))
            {
                AZ::DataPatch::Reflect(&context);
            }
            if (!context.FindClassData(azrtti_typeid<AzFramework::SimpleAssetReferenceBase>()))
            {
                AzFramework::SimpleAssetReferenceBase::Reflect(&context);
            }
            if (!context.FindClassData(azrtti_typeid<Range>()))
            {
                context.Class<Range>()->Field("Start", &Range::start)->Field("End", &Range::end);
            }
            using TextureAssetReference = AzFramework::SimpleAssetReference<LmbrCentral::TextureAsset>;
            if (!context.FindClassData(azrtti_typeid<TextureAssetReference>()))
            {
                TextureAssetReference::Register(context);
            }
            const AZ::Uuid legacyTextureAssetReferenceTypeId("{68E92460-5C0C-4031-9620-6F1A08763243}");
            if (!context.FindClassData(legacyTextureAssetReferenceTypeId))
            {
                context.ClassDeprecate(
                    "SimpleAssetReference_TextureAsset",
                    legacyTextureAssetReferenceTypeId,
                    [](AZ::SerializeContext& serializeContext, AZ::SerializeContext::DataElementNode& rootElement)
                    {
                        AZStd::vector<AZ::SerializeContext::DataElementNode> children;
                        for (int index = 0; index < rootElement.GetNumSubElements(); ++index)
                        {
                            children.push_back(rootElement.GetSubElement(index));
                        }
                        if (!rootElement.Convert<TextureAssetReference>(serializeContext))
                        {
                            return false;
                        }
                        for (AZ::SerializeContext::DataElementNode& child : children)
                        {
                            rootElement.AddElement(AZStd::move(child));
                        }
                        return true;
                    });
            }

            // Runtime canvas loading intentionally strips this editor-only marker. Register its
            // known id as deprecated so strict fixture loading still rejects every unexpected id.
            const AZ::Uuid editorOnlyEntityComponentTypeId("{22A16F1D-6D49-422D-AAE9-91AE45B5D3E7}");
            if (!context.FindClassData(editorOnlyEntityComponentTypeId))
            {
                context.ClassDeprecate("EditorOnlyEntityComponent", editorOnlyEntityComponentTypeId);
            }
            const AZ::Uuid scriptEditorComponentTypeId("{B5FC8679-FA2A-4C7C-AC42-DCC279EA613A}");
            if (!context.FindClassData(scriptEditorComponentTypeId))
            {
                context.ClassDeprecate("ScriptEditorComponent", scriptEditorComponentTypeId);
            }

            using TextureAssetReferenceList = AZStd::vector<TextureAssetReference>;
            const AZ::Uuid legacyTextureAssetReferenceListTypeId("{44AD8E77-27DB-55B1-9263-5C8973E5F692}");
            if (!context.FindClassData(legacyTextureAssetReferenceListTypeId))
            {
                context.ClassDeprecate(
                    "AZStd::vector<SimpleAssetReference_TextureAsset>",
                    legacyTextureAssetReferenceListTypeId,
                    [](AZ::SerializeContext& serializeContext, AZ::SerializeContext::DataElementNode& rootElement)
                    {
                        AZStd::vector<AZ::SerializeContext::DataElementNode> children;
                        for (int index = 0; index < rootElement.GetNumSubElements(); ++index)
                        {
                            children.push_back(rootElement.GetSubElement(index));
                        }
                        if (!rootElement.Convert<TextureAssetReferenceList>(serializeContext))
                        {
                            return false;
                        }
                        for (AZ::SerializeContext::DataElementNode& child : children)
                        {
                            rootElement.AddElement(AZStd::move(child));
                        }
                        return true;
                    });
            }
        }

        void SetupEnvironment() override
        {
            LyShineTest::SetupEnvironment();
        }

        void TearDown() override
        {
            LyShineTest::TearDown();
            if (m_createdAssetManager)
            {
                AZ::Data::AssetManager::Destroy();
                m_createdAssetManager = false;
            }
        }

        bool m_createdAssetManager = false;
    };

    TEST_F(LyShineSerializationTest, Serialization_LayoutErrorsOnNullptr_FT)
    {
        AZ_TEST_START_TRACE_SUPPRESSION;

        UiSerialize::SetAnchorLeft(nullptr, .0f);
        UiSerialize::SetAnchorTop(nullptr, .0f);
        UiSerialize::SetAnchorRight(nullptr, .0f);
        UiSerialize::SetAnchorBottom(nullptr, .0f);
        UiSerialize::SetAnchors(nullptr, .0f, .0f, .0f, .0f);

        UiSerialize::SetOffsetLeft(nullptr, .0f);
        UiSerialize::SetOffsetTop(nullptr, .0f);
        UiSerialize::SetOffsetRight(nullptr, .0f);
        UiSerialize::SetOffsetBottom(nullptr, .0f);
        UiSerialize::SetOffsets(nullptr, .0f, .0f, .0f, .0f);

        UiSerialize::SetPaddingLeft(nullptr, 0);
        UiSerialize::SetPaddingTop(nullptr, 0);
        UiSerialize::SetPaddingRight(nullptr, 0);
        UiSerialize::SetPaddingBottom(nullptr, 0);
        UiSerialize::SetPadding(nullptr, 0, 0, 0, 0);

        AZ_TEST_STOP_TRACE_SUPPRESSION(15);
    }

    TEST_F(LyShineSerializationTest, LegacyVec2Converter_PreservesComponents)
    {
        AZ::SerializeContext& context = *m_application->GetSerializeContext();
        AZ::SerializeContext::DataElementNode node;
        const AZ::Uuid legacyTypeId("{844131BA-9565-42F3-8482-6F65A6D5FC59}");
        node.GetRawDataElement().m_id = legacyTypeId;
        ASSERT_NE(node.AddElementWithData(context, "x", 1.25f), -1);
        ASSERT_NE(node.AddElementWithData(context, "y", -3.5f), -1);

        const AZ::SerializeContext::ClassData* legacyClass = context.FindClassData(legacyTypeId);
        ASSERT_NE(legacyClass, nullptr);
        ASSERT_TRUE(legacyClass->m_converter);
        ASSERT_TRUE(legacyClass->m_converter(context, node));
        AZ::Vector2 converted;
        ASSERT_TRUE(node.GetData(converted));
        EXPECT_FLOAT_EQ(converted.GetX(), 1.25f);
        EXPECT_FLOAT_EQ(converted.GetY(), -3.5f);
    }

    TEST_F(LyShineSerializationTest, LegacyVectorConverters_AreRemovedWithMathReflection)
    {
        AZ::SerializeContext context;
        const AZ::Uuid legacyVec2TypeId("{844131BA-9565-42F3-8482-6F65A6D5FC59}");
        const AZ::Uuid legacyVec3TypeId("{DFA993FB-4E92-4A13-BDB3-4E9285A5346F}");
        const AZ::Uuid legacyVec4TypeId("{CAC9510C-8C00-41D4-BC4D-2C6A8136EB30}");

        EXPECT_NE(context.FindClassData(legacyVec2TypeId), nullptr);
        EXPECT_NE(context.FindClassData(legacyVec3TypeId), nullptr);
        EXPECT_NE(context.FindClassData(legacyVec4TypeId), nullptr);

        context.EnableRemoveReflection();
        AZ::MathReflect(&context);
        context.DisableRemoveReflection();
        EXPECT_EQ(context.FindClassData(legacyVec2TypeId), nullptr);
        EXPECT_EQ(context.FindClassData(legacyVec3TypeId), nullptr);
        EXPECT_EQ(context.FindClassData(legacyVec4TypeId), nullptr);
        EXPECT_TRUE(context.FindClassId(AZ::Crc32("Vec2")).empty());
        EXPECT_TRUE(context.FindClassId(AZ::Crc32("Vec3")).empty());
        EXPECT_TRUE(context.FindClassId(AZ::Crc32("Vec4")).empty());

        AZ::MathReflect(&context);
        EXPECT_EQ(context.FindClassId(AZ::Crc32("Vec2")), AZStd::vector<AZ::Uuid>{ legacyVec2TypeId });
        EXPECT_EQ(context.FindClassId(AZ::Crc32("Vec3")), AZStd::vector<AZ::Uuid>{ legacyVec3TypeId });
        EXPECT_EQ(context.FindClassId(AZ::Crc32("Vec4")), AZStd::vector<AZ::Uuid>{ legacyVec4TypeId });
    }

    TEST_F(LyShineSerializationTest, LegacyVec3Converter_PreservesComponents)
    {
        AZ::SerializeContext& context = *m_application->GetSerializeContext();
        AZ::SerializeContext::DataElementNode node;
        const AZ::Uuid legacyTypeId("{DFA993FB-4E92-4A13-BDB3-4E9285A5346F}");
        node.GetRawDataElement().m_id = legacyTypeId;
        ASSERT_NE(node.AddElementWithData(context, "x", 1.25f), -1);
        ASSERT_NE(node.AddElementWithData(context, "y", -3.5f), -1);
        ASSERT_NE(node.AddElementWithData(context, "z", 7.75f), -1);

        const AZ::SerializeContext::ClassData* legacyClass = context.FindClassData(legacyTypeId);
        ASSERT_NE(legacyClass, nullptr);
        ASSERT_TRUE(legacyClass->m_converter);
        ASSERT_TRUE(legacyClass->m_converter(context, node));
        AZ::Vector3 converted;
        ASSERT_TRUE(node.GetData(converted));
        EXPECT_FLOAT_EQ(converted.GetX(), 1.25f);
        EXPECT_FLOAT_EQ(converted.GetY(), -3.5f);
        EXPECT_FLOAT_EQ(converted.GetZ(), 7.75f);
    }

    TEST_F(LyShineSerializationTest, LegacyVec4Converter_PreservesComponents)
    {
        AZ::SerializeContext& context = *m_application->GetSerializeContext();
        AZ::SerializeContext::DataElementNode node;
        const AZ::Uuid legacyTypeId("{CAC9510C-8C00-41D4-BC4D-2C6A8136EB30}");
        node.GetRawDataElement().m_id = legacyTypeId;
        ASSERT_NE(node.AddElementWithData(context, "x", 1.25f), -1);
        ASSERT_NE(node.AddElementWithData(context, "y", -3.5f), -1);
        ASSERT_NE(node.AddElementWithData(context, "z", 7.75f), -1);
        ASSERT_NE(node.AddElementWithData(context, "w", -9.0f), -1);

        const AZ::SerializeContext::ClassData* legacyClass = context.FindClassData(legacyTypeId);
        ASSERT_NE(legacyClass, nullptr);
        ASSERT_TRUE(legacyClass->m_converter);
        ASSERT_TRUE(legacyClass->m_converter(context, node));
        AZ::Vector4 converted;
        ASSERT_TRUE(node.GetData(converted));
        EXPECT_FLOAT_EQ(converted.GetX(), 1.25f);
        EXPECT_FLOAT_EQ(converted.GetY(), -3.5f);
        EXPECT_FLOAT_EQ(converted.GetZ(), 7.75f);
        EXPECT_FLOAT_EQ(converted.GetW(), -9.0f);
    }

    TEST_F(LyShineSerializationTest, LegacyVectorConverters_RejectMissingComponents)
    {
        AZ::SerializeContext& context = *m_application->GetSerializeContext();
        AZ::SerializeContext::DataElementNode node;
        const AZ::Uuid legacyTypeId("{844131BA-9565-42F3-8482-6F65A6D5FC59}");
        node.GetRawDataElement().m_id = legacyTypeId;
        ASSERT_NE(node.AddElementWithData(context, "x", 1.25f), -1);

        AZ_TEST_START_TRACE_SUPPRESSION;
        const AZ::SerializeContext::ClassData* legacyClass = context.FindClassData(legacyTypeId);
        ASSERT_NE(legacyClass, nullptr);
        ASSERT_TRUE(legacyClass->m_converter);
        EXPECT_FALSE(legacyClass->m_converter(context, node));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
    }

    TEST_F(LyShineSerializationTest, LegacyVec2SplineTrackConverter_PreservesOuterFields)
    {
        AZ::SerializeContext& context = *m_application->GetSerializeContext();
        const AZ::Uuid legacyTrackTypeId("{1E34E372-3A0E-5246-A4B2-70E75B5AA11F}");
        const AZ::SerializeContext::ClassData* legacyTrackClass = context.FindClassData(legacyTrackTypeId);
        ASSERT_NE(legacyTrackClass, nullptr);
        ASSERT_TRUE(legacyTrackClass->m_converter);

        AZ::SerializeContext::DataElementNode node;
        node.GetRawDataElement().m_id = legacyTrackTypeId;
        node.SetVersion(2);
        ASSERT_NE(node.AddElementWithData(context, "Flags", 42), -1);
        const AZ::Vector2 expectedDefaultValue(7.25f, -3.5f);
        AZ::SerializeContext::DataElementNode legacyDefaultValueNode;
        legacyDefaultValueNode.GetRawDataElement().m_id = AZ::Uuid("{844131BA-9565-42F3-8482-6F65A6D5FC59}");
        legacyDefaultValueNode.SetName("DefaultValue");
        ASSERT_NE(legacyDefaultValueNode.AddElementWithData(context, "x", expectedDefaultValue.GetX()), -1);
        ASSERT_NE(legacyDefaultValueNode.AddElementWithData(context, "y", expectedDefaultValue.GetY()), -1);
        ASSERT_NE(node.AddElement(legacyDefaultValueNode), -1);

        ASSERT_TRUE(legacyTrackClass->m_converter(context, node));
        EXPECT_EQ(node.GetId(), azrtti_typeid<TUiAnimSplineTrack<AZ::Vector2>>());
        EXPECT_EQ(node.GetVersion(), 2);
        ASSERT_EQ(node.GetNumSubElements(), 2);
        int flags = 0;
        EXPECT_TRUE(node.GetChildData(AZ_CRC_CE("Flags"), flags));
        EXPECT_EQ(flags, 42);

        AZ::SerializeContext::DataElementNode* defaultValueNode = node.FindSubElement(AZ_CRC_CE("DefaultValue"));
        ASSERT_NE(defaultValueNode, nullptr);
        EXPECT_EQ(defaultValueNode->GetId(), AZ::Uuid("{844131BA-9565-42F3-8482-6F65A6D5FC59}"));
        const AZ::SerializeContext::ClassData* legacyVectorClass = context.FindClassData(defaultValueNode->GetId());
        ASSERT_NE(legacyVectorClass, nullptr);
        ASSERT_TRUE(legacyVectorClass->m_converter);
        ASSERT_TRUE(legacyVectorClass->m_converter(context, *defaultValueNode));

        AZ::Vector2 defaultValue = AZ::Vector2::CreateZero();
        EXPECT_TRUE(node.GetChildData(AZ_CRC_CE("DefaultValue"), defaultValue));
        EXPECT_EQ(defaultValue, expectedDefaultValue);

        TUiAnimSplineTrack<AZ::Vector2> convertedTrack;
        ASSERT_TRUE(node.GetData(convertedTrack));
        float evaluatedDefault = 0.0f;
        convertedTrack.GetValue(0.0f, evaluatedDefault);
        EXPECT_FLOAT_EQ(evaluatedDefault, expectedDefaultValue.GetY());
    }

    TEST_F(LyShineSerializationTest, Vector2SplineValueConversionSupportsFloatAlignedStorage)
    {
        struct alignas(AZ::Vector2) FloatAlignedValueStorage
        {
            float m_padding;
            ISplineInterpolator::ValueType m_value;
        } storage{};

        static_assert(offsetof(FloatAlignedValueStorage, m_value) == sizeof(float));
        if constexpr (alignof(AZ::Vector2) > alignof(float))
        {
            EXPECT_NE(reinterpret_cast<uintptr_t>(storage.m_value) % alignof(AZ::Vector2), 0);
        }

        UiSpline::TrackSplineInterpolator<AZ::Vector2> spline;
        const AZ::Vector2 expected(1.25f, -7.5f);
        spline.ToValueType(expected, storage.m_value);
        EXPECT_FLOAT_EQ(storage.m_value[0], expected.GetX());
        EXPECT_FLOAT_EQ(storage.m_value[1], expected.GetY());
        EXPECT_FLOAT_EQ(storage.m_value[2], 0.0f);
        EXPECT_FLOAT_EQ(storage.m_value[3], 0.0f);

        AZ::Vector2 roundTrip = AZ::Vector2::CreateZero();
        spline.FromValueType(storage.m_value, roundTrip);
        EXPECT_EQ(roundTrip, expected);
    }

    TEST_F(LyShineSerializationTest, UiPrimitiveVertexBytesMatchDeclaredGpuLayout)
    {
        LyShine::UiPrimitiveVertex vertex{};
        vertex.xy = AZ::PackedVector2f(1.0f, 2.0f);
        vertex.color.dcolor = 0x44332211;
        vertex.st = AZ::PackedVector2f(3.0f, 4.0f);
        vertex.texIndex = 5;
        vertex.texHasColorChannel = 6;
        vertex.texIndex2 = 7;
        vertex.pad = 8;

        std::array<unsigned char, sizeof(vertex)> bytes{};
        std::memcpy(bytes.data(), &vertex, sizeof(vertex));

        float position[2]{};
        std::memcpy(position, bytes.data(), sizeof(position));
        EXPECT_FLOAT_EQ(position[0], 1.0f);
        EXPECT_FLOAT_EQ(position[1], 2.0f);

        AZ::u32 color = 0;
        std::memcpy(&color, bytes.data() + 8, sizeof(color));
        EXPECT_EQ(color, 0x44332211);

        float uv[2]{};
        std::memcpy(uv, bytes.data() + 12, sizeof(uv));
        EXPECT_FLOAT_EQ(uv[0], 3.0f);
        EXPECT_FLOAT_EQ(uv[1], 4.0f);
        EXPECT_EQ(bytes[20], 5);
        EXPECT_EQ(bytes[21], 6);
        EXPECT_EQ(bytes[22], 7);
        EXPECT_EQ(bytes[23], 8);
    }

    TEST_F(LyShineSerializationTest, CheckedInLegacyCanvases_LoadAndSemanticallyReserialize)
    {
        constexpr const char* legacyCanvasPaths[] = {
            "AutomatedTesting/Gem/PythonTests/assetpipeline/asset_processor_tests/assets/C1568831/uicanvas_to_delete.uicanvas",
            "Gems/GameStateSamples/Assets/UI/Canvases/DefaultLevelLoadingScreen.uicanvas",
            "Gems/LyShineExamples/Assets/UI/Canvases/LyShineExamples/Animation/MultipleSequences.uicanvas",
            "Gems/LyShineExamples/Assets/UI/Canvases/LyShineExamples/Animation/SequenceStates.uicanvas",
            "Gems/LyShineExamples/Assets/UI/Canvases/LyShineExamples/Animation/SimpleSequence.uicanvas",
            "Gems/LyShineExamples/Assets/UI/Canvases/LyShineExamples/Comp/Flipbook/Flipbook.uicanvas",
            "Gems/LyShineExamples/Assets/UI/Canvases/LyShineExamples/Comp/Mask/ChildMaskElement.uicanvas",
            "Gems/LyShineExamples/Assets/UI/Canvases/LyShineExamples/Comp/ParticleEmitter/ParticleEmitterSparks.uicanvas",
            "Gems/LyShineExamples/Assets/UI/Canvases/LyShineExamples/Comp/ParticleEmitter/ParticleEmitterTrails.uicanvas",
            "Gems/LyShineExamples/Assets/UI/Canvases/LyShineExamples/Comp/Text/Spacing.uicanvas",
            "Gems/LyShineExamples/Assets/UI/Canvases/LyShineExamples/Comp/Tooltips/Styles.uicanvas",
            "Gems/LyShineExamples/Assets/UI/Canvases/LyShineExamples/Performance/DrawCallsControl.uicanvas",
            "Gems/LyShineExamples/Assets/UI/Canvases/LyShineExamples/Performance/DrawCallsExample.uicanvas",
        };

        const AZ::IO::FixedMaxPath engineRoot(AZ::Utils::GetEnginePath());
        ASSERT_FALSE(engineRoot.empty());
        const AZ::ObjectStream::FilterDescriptor filter(
            [](const AZ::Data::AssetFilterInfo&)
            {
                return false;
            });

        for (const char* relativePath : legacyCanvasPaths)
        {
            const AZ::IO::FixedMaxPath canvasPath = engineRoot / relativePath;
            AZ::IO::SystemFileStream inputStream(canvasPath.c_str(), AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeBinary);
            ASSERT_TRUE(inputStream.IsOpen()) << relativePath;

            UiCanvasFileObject* canvas = UiCanvasFileObject::LoadCanvasFromStream(inputStream, filter);
            ASSERT_NE(canvas, nullptr) << relativePath;
            ASSERT_NE(canvas->m_canvasEntity, nullptr) << relativePath;
            ASSERT_NE(canvas->m_rootSliceEntity, nullptr) << relativePath;
            const AZStd::vector<AZStd::string> originalAnimationManifest = BuildAnimationManifest(canvas);

            AZStd::vector<AZ::u8> serializedBytes;
            AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> outputStream(&serializedBytes);
            UiCanvasFileObject::SaveCanvasToStream(outputStream, canvas);
            EXPECT_FALSE(serializedBytes.empty()) << relativePath;

            AZ::IO::MemoryStream reserializedStream(serializedBytes.data(), serializedBytes.size());
            UiCanvasFileObject* reloadedCanvas = UiCanvasFileObject::LoadCanvasFromStream(reserializedStream, filter);
            ASSERT_NE(reloadedCanvas, nullptr) << relativePath;
            ASSERT_NE(reloadedCanvas->m_canvasEntity, nullptr) << relativePath;
            ASSERT_NE(reloadedCanvas->m_rootSliceEntity, nullptr) << relativePath;
            EXPECT_EQ(reloadedCanvas->m_canvasEntity->GetId(), canvas->m_canvasEntity->GetId()) << relativePath;
            EXPECT_EQ(reloadedCanvas->m_canvasEntity->GetName(), canvas->m_canvasEntity->GetName()) << relativePath;
            const AZStd::vector<AZStd::string> reloadedAnimationManifest = BuildAnimationManifest(reloadedCanvas);
            ASSERT_EQ(reloadedAnimationManifest.size(), originalAnimationManifest.size()) << relativePath;
            for (size_t manifestIndex = 0; manifestIndex < originalAnimationManifest.size(); ++manifestIndex)
            {
                if (reloadedAnimationManifest[manifestIndex] != originalAnimationManifest[manifestIndex])
                {
                    EXPECT_EQ(reloadedAnimationManifest[manifestIndex], originalAnimationManifest[manifestIndex])
                        << relativePath << " manifest index " << manifestIndex;
                    break;
                }
            }

            delete canvas->m_canvasEntity;
            delete canvas->m_rootSliceEntity;
            canvas->m_canvasEntity = nullptr;
            canvas->m_rootSliceEntity = nullptr;
            delete canvas;

            delete reloadedCanvas->m_canvasEntity;
            delete reloadedCanvas->m_rootSliceEntity;
            reloadedCanvas->m_canvasEntity = nullptr;
            reloadedCanvas->m_rootSliceEntity = nullptr;
            delete reloadedCanvas;
        }
    }
} // namespace UnitTest
