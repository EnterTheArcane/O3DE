/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#if !defined(_RELEASE)
#include <AnimKey.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/UnitTest/UnitTest.h>
#include <AzTest/AzTest.h>
#include <Cinematics/AnimSplineTrack.h>
#include <Cinematics/AnimTrack.h>
#include <Maestro/Types/AssetBlendKey.h>

namespace Maestro
{

    namespace AnimTrackTest
    {
        struct ITestKey : public IKey
        {
        public:
            AZ_RTTI(ITestKey, "{0C84DBF1-C88E-4C76-8282-601EE30D1AFF}", IKey);

            ITestKey()
            {
            }

            ITestKey(bool isSelected, bool isSortMarker)
            {
                if (isSelected)
                {
                    flags |= AKEY_SELECTED;
                }

                if (isSortMarker)
                {
                    flags |= AKEY_SORT_MARKER;
                }
            }
        };

        class CTestTrack : public TAnimTrack<ITestKey>
        {
        public:
            AZ_RTTI(CTestTrack, "{78041EDD-872C-4AB9-81DD-605413810C7E}", IAnimTrack);

            CTestTrack()
            {
            }

            void GetKeyInfo([[maybe_unused]] int index, [[maybe_unused]] const char*& description, [[maybe_unused]] float& duration) const
            {
            }
            void SerializeKey([[maybe_unused]] ITestKey& key, [[maybe_unused]] XmlNodeRef& keyNode, [[maybe_unused]] bool bLoading)
            {
            }
        };

        class TAnimTrackTest : public ::testing::Test
        {
        public:
            TAnimTrackTest()
            {
            }

            void SetUp() override
            {
                // Init Test Track A
                int keyIndex = 0;

                m_testKeyA_0.time = 1.0f;
                m_testTrackA.CreateKey(m_testKeyA_0.time);
                m_testTrackA.SetKey(keyIndex++, &m_testKeyA_0);

                m_testKeyA_1 = ITestKey(/*is selected*/ true, /*is sort marker*/ false);
                m_testKeyA_1.time = 2.0f;
                m_testTrackA.CreateKey(m_testKeyA_1.time);
                m_testTrackA.SetKey(keyIndex++, &m_testKeyA_1);

                m_testKeyA_2 = ITestKey(/*is selected*/ false, /*is sort marker*/ true);
                m_testKeyA_2.time = 5.0f;
                m_testTrackA.CreateKey(m_testKeyA_2.time);
                m_testTrackA.SetKey(keyIndex++, &m_testKeyA_2);
            }

            CTestTrack m_emptyTrack;

            CTestTrack m_testTrackA;
            ITestKey m_testKeyA_0;
            ITestKey m_testKeyA_1;
            ITestKey m_testKeyA_2;
        };

        // Unit Tests

        TEST_F(TAnimTrackTest, IsKeySelected_InvalidKey_ExpectAssert)
        {
            AZ_TEST_START_TRACE_SUPPRESSION;
            m_testTrackA.IsKeySelected(-1);
            AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        }

        TEST_F(TAnimTrackTest, IsKeySelected_UnselectedKey_ExpectFalse)
        {
            bool result = true;
            result = m_testTrackA.IsKeySelected(2);
            EXPECT_FALSE(result);
        }

        TEST_F(TAnimTrackTest, IsKeySelected_SelectedKey_ExpectTrue)
        {
            bool result = true;
            result = m_testTrackA.IsKeySelected(1);
            EXPECT_TRUE(result);
        }

        TEST_F(TAnimTrackTest, SelectKey_InvalidKey)
        {
            AZ_TEST_START_TRACE_SUPPRESSION;
            m_testTrackA.SelectKey(-1, true);
            AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        }

        TEST_F(TAnimTrackTest, SelectKey_UnselectedKey_Select)
        {
            bool result = false;

            m_testTrackA.SelectKey(0, true);
            result = m_testTrackA.IsKeySelected(0);

            EXPECT_TRUE(result);
        }

        TEST_F(TAnimTrackTest, SelectKey_SelectedKey_Select)
        {
            bool result = false;

            m_testTrackA.SelectKey(1, true);
            result = m_testTrackA.IsKeySelected(1);

            EXPECT_TRUE(result);
        }

        TEST_F(TAnimTrackTest, SelectKey_UnselectedKey_UnSelect)
        {
            bool result = true;

            m_testTrackA.SelectKey(0, false);
            result = m_testTrackA.IsKeySelected(0);

            EXPECT_FALSE(result);
        }

        TEST_F(TAnimTrackTest, SelectKey_SelectedKey_UnSelect)
        {
            bool result = true;

            m_testTrackA.SelectKey(1, false);
            result = m_testTrackA.IsKeySelected(1);

            EXPECT_FALSE(result);
        }

        TEST_F(TAnimTrackTest, IsSortMarkerKey_InvalidKey)
        {
            AZ_TEST_START_TRACE_SUPPRESSION;
            m_testTrackA.IsSortMarkerKey(5);
            AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        }

        TEST_F(TAnimTrackTest, IsSortMarkerKey_UnmarkedKey_ExpectFalse)
        {
            bool result = true;
            result = m_testTrackA.IsSortMarkerKey(0);
            EXPECT_FALSE(result);
        }

        TEST_F(TAnimTrackTest, IsSortMarkerKey_MarkedKey_ExpectTrue)
        {
            bool result = false;
            result = m_testTrackA.IsSortMarkerKey(2);
            EXPECT_TRUE(result);
        }

        TEST_F(TAnimTrackTest, SetSortMarkerKey_InvalidKey)
        {
            AZ_TEST_START_TRACE_SUPPRESSION;
            m_testTrackA.SetSortMarkerKey(5, true);
            AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        }

        TEST_F(TAnimTrackTest, SetSortMarkerKey_UnsetKey_Set)
        {
            m_testTrackA.SetSortMarkerKey(0, true);
            EXPECT_TRUE(m_testTrackA.IsSortMarkerKey(0));
        }

        TEST_F(TAnimTrackTest, SetSortMarkerKey_UnsetKey_Unset)
        {
            m_testTrackA.SetSortMarkerKey(0, false);
            EXPECT_FALSE(m_testTrackA.IsSortMarkerKey(0));
        }

        TEST_F(TAnimTrackTest, SetSortMarkerKey_SetKey_Set)
        {
            m_testTrackA.SetSortMarkerKey(2, true);
            EXPECT_TRUE(m_testTrackA.IsSortMarkerKey(2));
        }

        TEST_F(TAnimTrackTest, SetSortMarkerKey_SetKey_Unset)
        {
            m_testTrackA.SetSortMarkerKey(2, false);
            EXPECT_FALSE(m_testTrackA.IsSortMarkerKey(2));
        }

        TEST_F(TAnimTrackTest, RemoveKey_RemoveFirstKey)
        {
            m_testTrackA.RemoveKey(0);

            IKey result;
            m_testTrackA.GetKey(0, &result);

            EXPECT_EQ(m_testTrackA.GetNumKeys(), 2);
            EXPECT_EQ(result.time, 2.0f);
        }

        TEST_F(TAnimTrackTest, RemoveKey_RemoveMiddleKey)
        {
            m_testTrackA.RemoveKey(1);

            EXPECT_EQ(m_testTrackA.GetNumKeys(), 2);

            IKey result;
            m_testTrackA.GetKey(0, &result);
            EXPECT_EQ(result.time, 1.0f);

            m_testTrackA.GetKey(1, &result);
            EXPECT_EQ(result.time, 5.0f);
        }

        TEST_F(TAnimTrackTest, RemoveKey_RemoveLastKey)
        {
            m_testTrackA.RemoveKey(2);
            EXPECT_EQ(m_testTrackA.GetNumKeys(), 2);
        }

        TEST_F(TAnimTrackTest, GetKey_InvalidIndex_ExpectAssert)
        {
            IKey result;
            AZ_TEST_START_TRACE_SUPPRESSION;
            m_testTrackA.GetKey(-1, &result);
            AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        }

        TEST_F(TAnimTrackTest, GetKey_ValidInputs_ExpectSuccess)
        {
            IKey result;
            m_testTrackA.GetKey(0, &result);
            EXPECT_EQ(result.time, m_testKeyA_0.time);
        }

        TEST_F(TAnimTrackTest, SetKey_InvalidIndex_ExpectAssert)
        {
            ITestKey testKey;
            AZ_TEST_START_TRACE_SUPPRESSION;
            m_testTrackA.SetKey(-1, &testKey);
            AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        }

        TEST_F(TAnimTrackTest, SetKey_ValidKey_ExpectSuccess)
        {
            ITestKey testKey;
            testKey.time = 3.0f;
            m_testTrackA.SetKey(2, &testKey);
            EXPECT_EQ(m_testTrackA.GetKeyTime(2), 3.0f);
            EXPECT_EQ(m_testTrackA.GetNumKeys(), 3);
            EXPECT_FALSE(m_testTrackA.IsSortMarkerKey(2));
        }

        TEST_F(TAnimTrackTest, GetKeyTime_InvalidIndex_ExpectAssert)
        {
            AZ_TEST_START_TRACE_SUPPRESSION;
            m_testTrackA.GetKeyTime(-1);
            AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        }

        TEST_F(TAnimTrackTest, GetKeyTime_ValidInputs_ExpectSuccess)
        {
            EXPECT_EQ(m_testTrackA.GetKeyTime(0), m_testKeyA_0.time);
            EXPECT_EQ(m_testTrackA.GetKeyTime(1), m_testKeyA_1.time);
            EXPECT_EQ(m_testTrackA.GetKeyTime(2), m_testKeyA_2.time);
        }

        TEST_F(TAnimTrackTest, SetKeyTime_InvalidIndex_ExpectAssert)
        {
            AZ_TEST_START_TRACE_SUPPRESSION;
            m_testTrackA.SetKeyTime(-1, 5.0f);
            AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        }

        TEST_F(TAnimTrackTest, SetKeyTime)
        {
            m_testTrackA.SetKeyTime(2, 6.0f);
            EXPECT_EQ(m_testTrackA.GetKeyTime(0), m_testKeyA_0.time);
            EXPECT_EQ(m_testTrackA.GetKeyTime(1), m_testKeyA_1.time);
            EXPECT_EQ(m_testTrackA.GetKeyTime(2), 6.0f);
        }

        TEST_F(TAnimTrackTest, FindKey_IncorrectInput_NoKeysFound)
        {
            int result = m_testTrackA.FindKey(4.0f);
            EXPECT_EQ(result, -1);
        }

        TEST_F(TAnimTrackTest, FindKey_ExactInputs_ExpectKeysFound)
        {
            int result = m_testTrackA.FindKey(1.0f);
            EXPECT_EQ(m_testTrackA.GetKeyTime(result), m_testKeyA_0.time);
            result = m_testTrackA.FindKey(2.0f);
            EXPECT_EQ(m_testTrackA.GetKeyTime(result), m_testKeyA_1.time);
            result = m_testTrackA.FindKey(5.0f);
            EXPECT_EQ(m_testTrackA.GetKeyTime(result), m_testKeyA_2.time);
        }

        TEST_F(TAnimTrackTest, CreateKey)
        {
            m_testTrackA.CreateKey(7.0f);
            EXPECT_EQ(m_testTrackA.GetNumKeys(), 4);
            EXPECT_EQ(m_testTrackA.GetKeyTime(3), 7.0f);
        }

        TEST_F(TAnimTrackTest, CloneKey)
        {
            constexpr const float timeOffset = -0.2f;
            const int keyIndex = m_testTrackA.CloneKey(2, timeOffset);
            EXPECT_EQ(m_testTrackA.GetNumKeys(), 4);
            EXPECT_EQ(m_testTrackA.GetKeyTime(keyIndex), m_testKeyA_2.time + timeOffset);
        }

        TEST_F(TAnimTrackTest, CopyKey)
        {
            m_emptyTrack.CopyKey(&m_testTrackA, 1);
            EXPECT_EQ(m_emptyTrack.GetNumKeys(), 1);
            EXPECT_EQ(m_emptyTrack.GetKeyTime(0), m_testKeyA_1.time);
        }

        TEST_F(TAnimTrackTest, GetActiveKey_NullKey)
        {
            int i = m_testTrackA.GetActiveKey(0.0f, nullptr);
            EXPECT_EQ(i, -1);
        }

        TEST_F(TAnimTrackTest, GetActiveKey_EmptyTrack)
        {
            ITestKey tempKey;
            int i = m_emptyTrack.GetActiveKey(5.0f, &tempKey);
            EXPECT_EQ(i, -1);
        }

        TEST_F(TAnimTrackTest, GetActiveKey_TimeIsBeforeFirstKey_RegularTrack_ExpectInvalid)
        {
            ITestKey tempKey;
            int i = m_testTrackA.GetActiveKey(0.5f, &tempKey);
            EXPECT_EQ(i, -1);
        }

        TEST_F(TAnimTrackTest, GetActiveKey_TimeIsBeforeCurrentKey_ExpectValid)
        {
            ITestKey tempKey;
            int i = m_testTrackA.GetActiveKey(3.0f, &tempKey);
            EXPECT_EQ(i, 1);

            i = m_testTrackA.GetActiveKey(1.5f, &tempKey);
            EXPECT_EQ(i, 0);
        }

        TEST_F(TAnimTrackTest, GetActiveKey_TimeIsAfterCurrentKey_ExpectValid)
        {
            ITestKey tempKey;
            int i = m_testTrackA.GetActiveKey(1.5f, &tempKey);
            EXPECT_EQ(i, 0);
        }

        TEST_F(TAnimTrackTest, GetActiveKey_TimeIsAfterLastKey_ExpectValid)
        {
            ITestKey tempKey;
            int i = m_testTrackA.GetActiveKey(6.0f, &tempKey);
            EXPECT_EQ(i, 2);
        }
    } // namespace AnimTrackTest

    TEST(MaestroLegacySerializationTest, Vec2TrackAndVec3FieldConvertersAreRegisteredWithoutLyShine)
    {
        AZ::SerializeContext context;
        struct ModuleGenericCleanup
        {
            ~ModuleGenericCleanup()
            {
                // The generic class info instances created by this test contain
                // code from the Maestro test module. Destroy them while the
                // module is still loaded rather than leaving them for process
                // shutdown after AzTestRunner has unloaded the module.
                AZ::GetGlobalSerializeContextModule().Cleanup();
            }

        } moduleGenericCleanup;

        IAnimTrack::Reflect(&context);
        TAnimSplineTrack<AZ::Vector2>::Reflect(&context);

        const AZ::Uuid legacyTrackTypeId("{665B2599-32E9-560F-A434-2E69C8A25117}");
        const AZ::SerializeContext::ClassData* legacyTrackClass = context.FindClassData(legacyTrackTypeId);
        ASSERT_NE(legacyTrackClass, nullptr);
        ASSERT_TRUE(legacyTrackClass->m_converter);

        AZ::SerializeContext::DataElementNode trackNode;
        trackNode.GetRawDataElement().m_id = legacyTrackTypeId;
        trackNode.SetVersion(4);
        ASSERT_NE(trackNode.AddElementWithData(context, "Flags", 37), -1);
        ASSERT_TRUE(legacyTrackClass->m_converter(context, trackNode));
        EXPECT_EQ(trackNode.GetId(), azrtti_typeid<TAnimSplineTrack<AZ::Vector2>>());
        EXPECT_EQ(trackNode.GetVersion(), 5);
        int flags = 0;
        EXPECT_TRUE(trackNode.GetChildData(AZ_CRC_CE("Flags"), flags));
        EXPECT_EQ(flags, 37);

        const AZ::Uuid legacyVec3TypeId("{DFA993FB-4E92-4A13-BDB3-4E9285A5346F}");
        const AZ::SerializeContext::ClassData* legacyVec3Class = context.FindClassData(legacyVec3TypeId);
        ASSERT_NE(legacyVec3Class, nullptr);
        ASSERT_TRUE(legacyVec3Class->m_converter);

        AZ::SerializeContext::DataElementNode vec3Node;
        vec3Node.GetRawDataElement().m_id = legacyVec3TypeId;
        ASSERT_NE(vec3Node.AddElementWithData(context, "x", 1.0f), -1);
        ASSERT_NE(vec3Node.AddElementWithData(context, "y", -2.0f), -1);
        ASSERT_NE(vec3Node.AddElementWithData(context, "z", 3.5f), -1);
        ASSERT_TRUE(legacyVec3Class->m_converter(context, vec3Node));
        AZ::Vector3 color;
        ASSERT_TRUE(vec3Node.GetData(color));
        EXPECT_EQ(color, AZ::Vector3(1.0f, -2.0f, 3.5f));
    }

} // namespace Maestro

#endif
