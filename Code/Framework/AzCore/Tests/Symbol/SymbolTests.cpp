/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/Memory/OSAllocator.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/Serialization/Internal/TextConversion.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Symbol/Internal/SymbolTable.h>
#include <AzCore/Symbol/Internal/SymbolStorage.h>
#include <AzCore/Symbol/Internal/SymbolValidation.h>
#include <AzCore/Symbol/Symbol.h>
#include <AzCore/Symbol/SymbolLiteral.h>
#include <AzCore/Symbol/SymbolSerializer.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/thread.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/typetraits/is_destructible.h>
#include <AzCore/std/typetraits/is_trivially_copyable.h>

#include <Tests/Symbol/SymbolTestSupport.h>

#include <cstring>
#include <limits>
#include <type_traits>
#include <xxhash.h>

namespace UnitTest
{
    namespace
    {
        class FailingSymbolAllocator final
            : public AZ::IAllocator
        {
        public:
            explicit FailingSymbolAllocator(const size_t failAfterAllocationCount)
                : m_failAfterAllocationCount{failAfterAllocationCount}
            {
            }

            AllocateAddress allocate(const size_type byteSize, const align_type alignment) override
            {
                if (m_allocationCount++ >= m_failAfterAllocationCount)
                {
                    return {};
                }

                void* address = AZ_OS_MALLOC(byteSize, alignment);
                size_type allocatedByteSize = 0;
                if (address)
                {
                    allocatedByteSize = byteSize;
                }
                return AllocateAddress{address, allocatedByteSize};
            }

            size_type deallocate(pointer address, size_type, align_type) override
            {
                AZ_OS_FREE(address);
                return 0;
            }

            AllocateAddress reallocate(pointer, size_type, align_type) override
            {
                return {};
            }

            size_type get_allocated_size(pointer, align_type) const override
            {
                return 0;
            }

            void FailNextAllocation()
            {
                m_failAfterAllocationCount = m_allocationCount;
            }

        private:
            size_t m_failAfterAllocationCount;
            size_t m_allocationCount = 0;
        };

        struct SymbolContainer final
        {
            AZ_TYPE_INFO(SymbolContainer, "{18F94552-6261-4F27-8AE8-6B78499B7737}");

            static void Reflect(AZ::SerializeContext& serializeContext)
            {
                serializeContext.Class<SymbolContainer>()
                    ->Version(1)
                    ->Field("Symbols", &SymbolContainer::m_symbols);
            }

            AZStd::vector<AZ::Symbol> m_symbols;
        };
    } // namespace

    class SymbolTests
        : public LeakDetectionFixture
    {
    };

    TEST_F(SymbolTests, TraitsArePointerSizedAndTrivial)
    {
        static_assert(AZ::Symbol::IsValid(""));
        static_assert(AZ::Symbol::IsValid("a"));
        static_assert(sizeof(AZ::Symbol) == sizeof(void*));
        static_assert(std::is_standard_layout_v<AZ::Symbol>);
        static_assert(AZStd::is_trivially_copyable_v<AZ::Symbol>);
        static_assert(AZStd::is_trivially_destructible_v<AZ::Symbol>);
        static_assert(!std::is_convertible_v<AZStd::string_view, AZ::Symbol>);
        static_assert(!std::is_convertible_v<AZ::Symbol, AZStd::string_view>);
        SUCCEED();
    }

    TEST_F(SymbolTests, VendoredXxh3VersionAndDispatchBoundaryVectorsAreStable)
    {
        struct HashVector final
        {
            size_t m_length;
            XXH64_hash_t m_unseeded;
            XXH64_hash_t m_seeded;
        };

        constexpr XXH64_hash_t Seed = 0x0123456789ABCDEFull;
        constexpr HashVector Vectors[] = {
            {0, 0x2D06800538D394C2ull, 0xCC1CA35A1B089C5Cull},
            {1, 0x4A4139CAF4136257ull, 0xFA04F0E2621BCC23ull},
            {3, 0x505118C313121C0Eull, 0x6174298C86E7040Dull},
            {4, 0x3F6889EF166B4AD1ull, 0x1672EA86470577D2ull},
            {8, 0x93F1200F9BE82671ull, 0x6A6BD4A711F78D8Aull},
            {9, 0x05B2FD4D97EDCDAEull, 0x9863DC0DDA1A4468ull},
            {16, 0x2E4359E2A04A5E32ull, 0xAF54D11A43A78036ull},
            {17, 0xF60C53684BBC489Dull, 0x2D8679ABCF68E8BBull},
            {128, 0x5C6AB7B5B2360539ull, 0xA40C5F87DDA0A045ull},
            {129, 0x45FABBD183DE7649ull, 0x0F2558E3A3D42811ull},
            {240, 0xC0796428068CD12Eull, 0x1B4ED5223C8FFA57ull},
            {241, 0x859DC8AB6DD85C7Cull, 0x889EA88F951BD31Eull},
            {512, 0xD02220E4B3BFACE7ull, 0x3D499708C462D379ull},
            {1023, 0xF35D4C7D308F7A40ull, 0xC8EFAE76BA62C45Aull},
        };

        AZStd::array<AZ::u8, 1024> input{};
        for (size_t index = 0; index < input.size(); ++index)
        {
            input[index] = static_cast<AZ::u8>(index * 37 + 11);
        }

        EXPECT_EQ(XXH_VERSION_NUMBER, 803);
        EXPECT_EQ(XXH_versionNumber(), 803);
        EXPECT_EQ(XXH64(nullptr, 0, 0), 17241709254077376921ull);
        for (const HashVector& vector : Vectors)
        {
            EXPECT_EQ(XXH3_64bits(input.data(), vector.m_length), vector.m_unseeded) << vector.m_length;
            EXPECT_EQ(XXH3_64bits_withSeed(input.data(), vector.m_length, Seed), vector.m_seeded) << vector.m_length;
        }
    }

    TEST_F(SymbolTests, VendoredXxh3StreamingMatchesOneShotHash)
    {
        AZStd::array<AZ::u8, 1024> input{};
        for (size_t index = 0; index < input.size(); ++index)
        {
            input[index] = static_cast<AZ::u8>(index * 37 + 11);
        }

        XXH3_state_t* state = XXH3_createState();
        ASSERT_NE(state, nullptr);
        ASSERT_EQ(XXH3_64bits_reset(state), XXH_OK);
        ASSERT_EQ(XXH3_64bits_update(state, input.data(), 17), XXH_OK);
        ASSERT_EQ(XXH3_64bits_update(state, input.data() + 17, 224), XXH_OK);
        ASSERT_EQ(XXH3_64bits_update(state, input.data() + 241, input.size() - 241), XXH_OK);
        EXPECT_EQ(XXH3_64bits_digest(state), XXH3_64bits(input.data(), input.size()));
        EXPECT_EQ(XXH3_freeState(state), XXH_OK);
    }

    TEST_F(SymbolTests, EmptySymbolHasStableNullState)
    {
        const AZ::Symbol symbol;

        EXPECT_TRUE(symbol.IsEmpty());
        EXPECT_TRUE(symbol.GetStringView().empty());
        EXPECT_STREQ(symbol.GetCStr(), "");
        EXPECT_EQ(AZ::SymbolHash{}(symbol), 0);
    }

    TEST_F(SymbolTests, EmptyValueHasIdenticalConstructionAndOptionalSemantics)
    {
        const AZ::Symbol defaultValue;
        const AZ::Symbol constructed{AZStd::string_view{}};
        const AZ::Symbol created = AZ::Symbol::Create({});
        const AZStd::optional<AZ::Symbol> tried = AZ::Symbol::TryCreate({});
        const AZStd::optional<AZ::Symbol> found = AZ::Symbol::Find({});

        ASSERT_TRUE(tried.has_value());
        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(defaultValue, constructed);
        EXPECT_EQ(defaultValue, created);
        EXPECT_EQ(defaultValue, *tried);
        EXPECT_EQ(defaultValue, *found);
    }

    TEST_F(SymbolTests, ExplicitCreationHelpersMatchConstructorAndFindDoesNotAdmit)
    {
        const AZStd::string unknownValue = AZStd::string::format("UnknownSymbol_%p", this);
        EXPECT_FALSE(AZ::Symbol::Find(unknownValue).has_value());
        EXPECT_FALSE(AZ::Symbol::Find(unknownValue).has_value());

        const AZStd::optional<AZ::Symbol> tried = AZ::Symbol::TryCreate(unknownValue);
        ASSERT_TRUE(tried.has_value());
        const AZ::Symbol constructed{unknownValue};
        const AZ::Symbol created = AZ::Symbol::Create(unknownValue);
        const AZStd::optional<AZ::Symbol> found = AZ::Symbol::Find(unknownValue);

        ASSERT_TRUE(found.has_value());
        EXPECT_EQ(*tried, constructed);
        EXPECT_EQ(*tried, created);
        EXPECT_EQ(*tried, *found);
    }

    TEST_F(SymbolTests, TryCreateRejectsInvalidValuesWithoutProducingAnEmptySymbol)
    {
        const AZStd::string oversized(AZ::Symbol::MaxStringBufferSize, 'x');
        constexpr char nullAtStart[] = {'\0', 'a'};
        constexpr char nullInMiddle[] = {'a', '\0', 'b'};
        constexpr char nullAtEnd[] = {'a', '\0'};
        constexpr char overlong[] = {static_cast<char>(0xC0), static_cast<char>(0x80)};
        constexpr char surrogate[] = {static_cast<char>(0xED), static_cast<char>(0xA0), static_cast<char>(0x80)};
        constexpr char aboveUnicode[] = {
            static_cast<char>(0xF4),
            static_cast<char>(0x90),
            static_cast<char>(0x80),
            static_cast<char>(0x80),
        };

        EXPECT_FALSE(AZ::Symbol::TryCreate(oversized).has_value());
        EXPECT_FALSE(AZ::Symbol::TryCreate(AZStd::string_view{nullAtStart, sizeof(nullAtStart)}).has_value());
        EXPECT_FALSE(AZ::Symbol::TryCreate(AZStd::string_view{nullInMiddle, sizeof(nullInMiddle)}).has_value());
        EXPECT_FALSE(AZ::Symbol::TryCreate(AZStd::string_view{nullAtEnd, sizeof(nullAtEnd)}).has_value());
        EXPECT_FALSE(AZ::Symbol::TryCreate(AZStd::string_view{overlong, sizeof(overlong)}).has_value());
        EXPECT_FALSE(AZ::Symbol::TryCreate(AZStd::string_view{surrogate, sizeof(surrogate)}).has_value());
        EXPECT_FALSE(AZ::Symbol::TryCreate(AZStd::string_view{aboveUnicode, sizeof(aboveUnicode)}).has_value());
        EXPECT_FALSE(AZ::Symbol::Find(AZStd::string_view{overlong, sizeof(overlong)}).has_value());
    }

    TEST_F(SymbolTests, Utf8ValidationRejectsEveryMalformedSequenceCategory)
    {
        const AZStd::vector<AZStd::vector<char>> invalidValues{
            {static_cast<char>(0x80)},
            {static_cast<char>(0xBF)},
            {static_cast<char>(0xC0), static_cast<char>(0x80)},
            {static_cast<char>(0xC1), static_cast<char>(0xBF)},
            {static_cast<char>(0xC2)},
            {static_cast<char>(0xC2), 'a'},
            {static_cast<char>(0xE0), static_cast<char>(0x9F), static_cast<char>(0xBF)},
            {static_cast<char>(0xE0), static_cast<char>(0xA0)},
            {static_cast<char>(0xE1), 'a', static_cast<char>(0x80)},
            {static_cast<char>(0xE1), static_cast<char>(0x80), 'a'},
            {static_cast<char>(0xED), static_cast<char>(0xA0), static_cast<char>(0x80)},
            {static_cast<char>(0xF0), static_cast<char>(0x8F), static_cast<char>(0xBF), static_cast<char>(0xBF)},
            {static_cast<char>(0xF0), static_cast<char>(0x90), static_cast<char>(0x80)},
            {static_cast<char>(0xF1), 'a', static_cast<char>(0x80), static_cast<char>(0x80)},
            {static_cast<char>(0xF1), static_cast<char>(0x80), 'a', static_cast<char>(0x80)},
            {static_cast<char>(0xF1), static_cast<char>(0x80), static_cast<char>(0x80), 'a'},
            {static_cast<char>(0xF4), static_cast<char>(0x90), static_cast<char>(0x80), static_cast<char>(0x80)},
            {static_cast<char>(0xF5), static_cast<char>(0x80), static_cast<char>(0x80), static_cast<char>(0x80)},
            {static_cast<char>(0xFF)},
        };

        for (const AZStd::vector<char>& value : invalidValues)
        {
            const AZStd::string_view valueView{value.data(), value.size()};
            EXPECT_FALSE(AZ::Symbol::IsValid(valueView));
            EXPECT_FALSE(AZ::Symbol::TryCreate(valueView).has_value());
            EXPECT_FALSE(AZ::Symbol::Find(valueView).has_value());
        }
    }

    TEST_F(SymbolTests, ControlsAndUnicodeNoncharactersRemainValidSymbolValues)
    {
        constexpr char value[] = {
            '\x01',
            '\n',
            static_cast<char>(0xEF),
            static_cast<char>(0xBF),
            static_cast<char>(0xBE),
        };
        const AZStd::string_view valueView{value, sizeof(value)};

        EXPECT_TRUE(AZ::Symbol::IsValid(valueView));
        const AZStd::optional<AZ::Symbol> symbol = AZ::Symbol::TryCreate(valueView);
        ASSERT_TRUE(symbol.has_value());
        EXPECT_EQ(symbol->GetStringView(), valueView);
        EXPECT_FALSE(AZ::Internal::IsValidXmlText(valueView));
    }

    TEST_F(SymbolTests, XmlObjectStreamRejectsUnrepresentableValueButJsonRoundTripsIt)
    {
        constexpr char value[] = {'a', '\x01', 'b'};
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        const AZ::Symbol input{AZStd::string_view{value, sizeof(value)}};

        AZStd::vector<char> xmlBuffer;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> xmlStream(&xmlBuffer);
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(AZ::Utils::SaveObjectToStream(
            xmlStream,
            AZ::ObjectStream::ST_XML,
            &input,
            &serializeContext));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        AZStd::vector<char> jsonBuffer;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> jsonOutputStream(&jsonBuffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(
            jsonOutputStream,
            AZ::ObjectStream::ST_JSON,
            &input,
            &serializeContext));

        AZ::Symbol output;
        AZ::IO::MemoryStream jsonInputStream(jsonBuffer.data(), jsonBuffer.size());
        EXPECT_TRUE(AZ::Utils::LoadObjectFromStreamInPlace(jsonInputStream, output, &serializeContext));
        EXPECT_EQ(output, input);
    }

    TEST_F(SymbolTests, EqualValuesHaveCanonicalPointerIdentity)
    {
        using namespace AZ::Literals;

        const AZ::Symbol first{AZStd::string_view{"CanonicalSymbol"}};
        const AZ::Symbol second{AZStd::string_view{"CanonicalSymbol"}};
        const AZ::Symbol literal = "CanonicalSymbol"_sym;

        EXPECT_EQ(first, second);
        EXPECT_EQ(first, literal);
        EXPECT_EQ(AZ::SymbolHash{}(first), AZ::SymbolHash{}(literal));
        EXPECT_EQ(first.GetStringView(), "CanonicalSymbol");
        EXPECT_STREQ(first.GetCStr(), "CanonicalSymbol");
    }

    TEST_F(SymbolTests, IdentityIsCaseSensitive)
    {
        const AZ::Symbol lower{AZStd::string_view{"player"}};
        const AZ::Symbol upper{AZStd::string_view{"Player"}};

        EXPECT_NE(lower, upper);
        EXPECT_TRUE(AZ::SymbolEqual{}(lower, lower));
        EXPECT_FALSE(AZ::SymbolEqual{}(lower, upper));
    }

    TEST_F(SymbolTests, LiteralIsCanonicalAcrossTranslationUnits)
    {
        using namespace AZ::Literals;

        EXPECT_EQ("CrossTranslationUnitSymbol"_sym, GetCrossTranslationUnitSymbol());
    }

    TEST_F(SymbolTests, MaximumValueIsAcceptedAndNullTerminated)
    {
        const AZStd::string value(AZ::Symbol::MaxStringSize, 'm');
        const AZ::Symbol symbol{value};

        EXPECT_FALSE(symbol.IsEmpty());
        EXPECT_EQ(symbol.GetStringView(), value);
        EXPECT_EQ(symbol.GetCStr()[AZ::Symbol::MaxStringSize], '\0');
    }

    TEST_F(SymbolTests, InvalidDirectConstructionTerminates)
    {
        const AZStd::string oversized(AZ::Symbol::MaxStringSize + 1, 'x');
        constexpr char embeddedNull[] = {'a', '\0', 'b'};
        constexpr char malformed[] = {static_cast<char>(0xC0), static_cast<char>(0xAF)};
        const AZStd::string_view embeddedNullView{embeddedNull, sizeof(embeddedNull)};
        const AZStd::string_view malformedView{malformed, sizeof(malformed)};

        EXPECT_DEATH((void)AZ::Symbol{oversized}, "");
        EXPECT_DEATH((void)AZ::Symbol{embeddedNullView}, "");
        EXPECT_DEATH((void)AZ::Symbol{malformedView}, "");
    }

    TEST_F(SymbolTests, Utf8ValidationAcceptsValidBoundarySequences)
    {
        constexpr char valid[] = {
            static_cast<char>(0xC2), static_cast<char>(0x80),
            static_cast<char>(0xE0), static_cast<char>(0xA0), static_cast<char>(0x80),
            static_cast<char>(0xF4), static_cast<char>(0x8F), static_cast<char>(0xBF), static_cast<char>(0xBF),
        };

        EXPECT_EQ(
            AZ::Internal::ValidateSymbolValue(AZStd::string_view{valid, sizeof(valid)}, AZ::Symbol::MaxStringSize),
            AZ::Internal::SymbolValidationError::None);
        const AZ::Symbol symbol{AZStd::string_view{valid, sizeof(valid)}};
        EXPECT_FALSE(symbol.IsEmpty());
    }

    TEST_F(SymbolTests, FindDoesNotCreateMissingSymbol)
    {
        constexpr AZStd::string_view value{"FindWithoutInterning"};
        AZ::Symbol found;

        EXPECT_FALSE(AZ::Internal::FindSymbol(found, value));
        const AZ::Symbol inserted{value};
        EXPECT_TRUE(AZ::Internal::FindSymbol(found, value));
        EXPECT_EQ(found, inserted);
    }

    TEST_F(SymbolTests, FindDoesNotAllocateOrInitializeAShard)
    {
        FailingSymbolAllocator allocator{0};
        AZ::Internal::SymbolTable table{allocator};

        EXPECT_EQ(table.Find("UnknownValue", 17), nullptr);
        EXPECT_EQ(table.GetStorageBytes(), 0);
    }

    TEST_F(SymbolTests, HashCollisionStillUsesExactBytes)
    {
        AZ::Internal::SymbolTable table;
        constexpr AZ::u64 hash = 0xDEADBEEF;
        constexpr size_t ValueCount = 40;
        AZStd::array<AZStd::string, ValueCount> values;
        AZStd::array<const AZ::Internal::SymbolEntry*, ValueCount> entries{};

        for (size_t index = 0; index < ValueCount; ++index)
        {
            values[index] = AZStd::string::format("CollisionValue%zu", index);
            entries[index] = table.Intern(values[index], hash);
            ASSERT_NE(entries[index], nullptr);
        }

        for (size_t index = 0; index < ValueCount; ++index)
        {
            EXPECT_EQ(table.Intern(values[index], hash), entries[index]);
            EXPECT_EQ(table.Find(values[index], hash), entries[index]);
        }
    }

    TEST_F(SymbolTests, StorageBudgetRejectsArithmeticOverflowWithoutMutation)
    {
        AZ::Internal::SymbolStorageBudget budget{(std::numeric_limits<size_t>::max)()};
        const size_t reservation = (std::numeric_limits<size_t>::max)() - 1;

        ASSERT_TRUE(budget.TryReserve(reservation));
        EXPECT_FALSE(budget.TryReserve(2));
        EXPECT_EQ(budget.GetUsed(), reservation);
        budget.Release(reservation);
        EXPECT_EQ(budget.GetUsed(), 0);
    }

    TEST_F(SymbolTests, AllocationFailureRollsBackStorageAndTableMutation)
    {
        {
            FailingSymbolAllocator allocator{0};
            AZ::Internal::SymbolTable table{allocator};
            EXPECT_EQ(table.TryIntern("InitialStorageFailure", 1), nullptr);
            EXPECT_EQ(table.GetStorageBytes(), 0);
            EXPECT_EQ(table.Find("InitialStorageFailure", 1), nullptr);
        }

        {
            FailingSymbolAllocator allocator{1};
            AZ::Internal::SymbolTable table{allocator};
            EXPECT_EQ(table.TryIntern("PartialStorageFailure", 1), nullptr);
            EXPECT_EQ(table.GetStorageBytes(), 0);
            EXPECT_EQ(table.Find("PartialStorageFailure", 1), nullptr);
        }

        {
            FailingSymbolAllocator allocator{2};
            AZ::Internal::SymbolTable table{allocator};
            EXPECT_EQ(table.TryIntern("PartialTableStorageFailure", 1), nullptr);
            EXPECT_EQ(table.GetStorageBytes(), 0);
            EXPECT_EQ(table.Find("PartialTableStorageFailure", 1), nullptr);
        }

        {
            FailingSymbolAllocator allocator{3};
            AZ::Internal::SymbolTable table{allocator};
            for (size_t index = 0; index < 14; ++index)
            {
                const AZStd::string value = AZStd::string::format("ResizeFailure%zu", index);
                ASSERT_NE(table.TryIntern(value, 1), nullptr);
            }
            const size_t storageBytes = table.GetStorageBytes();
            EXPECT_EQ(table.TryIntern("ResizeFailureFinal", 1), nullptr);
            EXPECT_EQ(table.GetStorageBytes(), storageBytes);
            EXPECT_EQ(table.Find("ResizeFailureFinal", 1), nullptr);
            for (size_t index = 0; index < 14; ++index)
            {
                const AZStd::string value = AZStd::string::format("ResizeFailure%zu", index);
                EXPECT_NE(table.Find(value, 1), nullptr);
            }
        }
    }

    TEST_F(SymbolTests, BudgetExhaustionBeforeFirstInsertionLeavesNoStorage)
    {
        FailingSymbolAllocator allocator{(std::numeric_limits<size_t>::max)()};
        AZ::Internal::SymbolTable table{allocator, 128};

        EXPECT_EQ(table.TryIntern("BudgetFailure", 1), nullptr);
        EXPECT_EQ(table.GetStorageBytes(), 0);
        EXPECT_EQ(table.Find("BudgetFailure", 1), nullptr);
    }

    TEST_F(SymbolTests, ArenaGrowthFailureRollsBackThePendingEntry)
    {
        FailingSymbolAllocator allocator{(std::numeric_limits<size_t>::max)()};
        AZ::Internal::SymbolTable table{allocator};
        constexpr AZ::u64 Hash = 7;
        AZStd::array<AZStd::string, 4> values;
        for (size_t index = 0; index < values.size(); ++index)
        {
            values[index].assign(AZ::Symbol::MaxStringSize, static_cast<char>('a' + index));
        }

        for (size_t index = 0; index < 3; ++index)
        {
            ASSERT_NE(table.TryIntern(values[index], Hash), nullptr);
        }
        const size_t storageBytes = table.GetStorageBytes();
        allocator.FailNextAllocation();

        EXPECT_EQ(table.TryIntern(values.back(), Hash), nullptr);
        EXPECT_EQ(table.GetStorageBytes(), storageBytes);
        EXPECT_EQ(table.Find(values.back(), Hash), nullptr);
        for (size_t index = 0; index < 3; ++index)
        {
            EXPECT_NE(table.Find(values[index], Hash), nullptr);
        }
    }

    TEST_F(SymbolTests, BudgetExhaustionRejectsNewStorageButPreservesExistingValues)
    {
        FailingSymbolAllocator allocator{(std::numeric_limits<size_t>::max)()};
        AZ::Internal::SymbolTable table{allocator, 4300};
        const AZ::Internal::SymbolEntry* existing = table.TryIntern("BudgetKnown", 0);
        ASSERT_NE(existing, nullptr);

        EXPECT_EQ(table.TryIntern("BudgetUnknown", 1), nullptr);
        EXPECT_EQ(table.Find("BudgetUnknown", 1), nullptr);
        EXPECT_EQ(table.TryIntern("BudgetKnown", 0), existing);
        EXPECT_EQ(table.Find("BudgetKnown", 0), existing);
    }

    TEST_F(SymbolTests, ConcurrentInterningReturnsOneCanonicalIdentity)
    {
        constexpr size_t ThreadCount = 16;
        AZStd::vector<AZ::Symbol> results(ThreadCount);
        AZStd::vector<AZStd::thread> threads;
        threads.reserve(ThreadCount);

        for (size_t index = 0; index < ThreadCount; ++index)
        {
            threads.emplace_back([index, &results]()
            {
                results[index] = AZ::Symbol{AZStd::string_view{"ConcurrentCanonicalSymbol"}};
            });
        }
        for (AZStd::thread& thread : threads)
        {
            thread.join();
        }

        for (const AZ::Symbol symbol : results)
        {
            EXPECT_EQ(symbol, results.front());
        }
    }

    TEST_F(SymbolTests, ConcurrentDistinctValuesRemainDiscoverableAndCanonical)
    {
        constexpr size_t ThreadCount = 16;
        AZStd::array<AZStd::string, ThreadCount> values;
        AZStd::array<AZ::Symbol, ThreadCount> results;
        AZStd::vector<AZStd::thread> threads;
        threads.reserve(ThreadCount);

        for (size_t index = 0; index < ThreadCount; ++index)
        {
            values[index] = AZStd::string::format("ConcurrentDistinctSymbol%zu_%p", index, this);
            threads.emplace_back([index, &results, &values]()
            {
                results[index] = AZ::Symbol{values[index]};
            });
        }
        for (AZStd::thread& thread : threads)
        {
            thread.join();
        }

        for (size_t index = 0; index < ThreadCount; ++index)
        {
            const AZStd::optional<AZ::Symbol> found = AZ::Symbol::Find(values[index]);
            ASSERT_TRUE(found.has_value());
            EXPECT_EQ(*found, results[index]);
        }
    }

    TEST_F(SymbolTests, ConcurrentFirstLiteralUseReturnsOneCanonicalIdentity)
    {
        using namespace AZ::Literals;

        constexpr size_t ThreadCount = 16;
        AZStd::vector<AZ::Symbol> results(ThreadCount);
        AZStd::vector<AZStd::thread> threads;
        threads.reserve(ThreadCount);

        for (size_t index = 0; index < ThreadCount; ++index)
        {
            threads.emplace_back([index, &results]()
            {
                results[index] = "ConcurrentFirstUseLiteral"_sym;
            });
        }
        for (AZStd::thread& thread : threads)
        {
            thread.join();
        }

        for (const AZ::Symbol symbol : results)
        {
            EXPECT_EQ(symbol, results.front());
        }
    }

    TEST_F(SymbolTests, BinaryEncodingAlwaysIncludesOneMarker)
    {
        AZ::SymbolSerializer serializer;
        const AZ::Symbol empty;
        const AZ::Symbol value{AZStd::string_view{"PersistentSymbol"}};
        AZStd::vector<AZ::u8> buffer;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> stream(&buffer);

        EXPECT_EQ(serializer.Save(&empty, stream, false), 1);
        ASSERT_EQ(buffer.size(), 1);
        EXPECT_EQ(buffer[0], 0);

        buffer.clear();
        stream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
        EXPECT_EQ(serializer.Save(&value, stream, false), value.GetStringView().size() + 1);
        ASSERT_EQ(buffer.size(), value.GetStringView().size() + 1);
        EXPECT_EQ(buffer.back(), 0);
    }

    TEST_F(SymbolTests, BinaryLoadIsTransactionalAndRequiresExactMarker)
    {
        AZ::SymbolSerializer serializer;
        const AZ::Symbol existing{AZStd::string_view{"ExistingSymbol"}};
        AZ::Symbol output = existing;

        const AZStd::vector<AZStd::vector<AZ::u8>> invalidValues{
            {},
            {'n', 'o', 'm', 'a', 'r', 'k', 'e', 'r'},
            {'a', 0, 'b', 0},
            {static_cast<AZ::u8>(0xC0), static_cast<AZ::u8>(0xAF), 0},
        };

        for (const AZStd::vector<AZ::u8>& bytes : invalidValues)
        {
            AZ::IO::MemoryStream stream(bytes.data(), bytes.size());
            EXPECT_FALSE(serializer.Load(&output, stream, 0, false));
            EXPECT_EQ(output, existing);
        }
    }

    TEST_F(SymbolTests, MaximumBinaryEncodingIsExactlyOneKiB)
    {
        AZ::SymbolSerializer serializer;
        const AZStd::string value(AZ::Symbol::MaxStringSize, 'p');
        const AZ::Symbol input{value};
        AZStd::vector<AZ::u8> buffer;
        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> outputStream(&buffer);

        EXPECT_EQ(serializer.Save(&input, outputStream, false), 1024);
        ASSERT_EQ(buffer.size(), 1024);

        AZ::Symbol output;
        AZ::IO::MemoryStream inputStream(buffer.data(), buffer.size());
        EXPECT_TRUE(serializer.Load(&output, inputStream, 0, false));
        EXPECT_EQ(output, input);
    }

    TEST_F(SymbolTests, ObjectStreamRoundTripsEveryFormat)
    {
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        const AZ::Symbol input{AZStd::string_view{"ObjectStreamSymbol"}};

        for (const AZ::ObjectStream::StreamType streamType :
            {AZ::ObjectStream::ST_BINARY, AZ::ObjectStream::ST_XML, AZ::ObjectStream::ST_JSON})
        {
            AZStd::vector<char> buffer;
            AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
            ASSERT_TRUE(AZ::Utils::SaveObjectToStream(outputStream, streamType, &input, &serializeContext));

            AZ::Symbol output;
            AZ::IO::MemoryStream inputStream(buffer.data(), buffer.size());
            EXPECT_TRUE(AZ::Utils::LoadObjectFromStreamInPlace(inputStream, output, &serializeContext));
            EXPECT_EQ(output, input);
        }
    }

    TEST_F(SymbolTests, BinaryObjectStreamRejectsEveryPayloadTruncationBoundaryWithoutMutation)
    {
        constexpr AZStd::string_view Value{"TruncationBoundarySymbol"};
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        const AZ::Symbol input{Value};
        const AZ::Symbol existing{AZStd::string_view{"ExistingTruncationValue"}};
        AZStd::vector<char> buffer;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(
            outputStream,
            AZ::ObjectStream::ST_BINARY,
            &input,
            &serializeContext));

        const auto valuePosition = AZStd::search(buffer.begin(), buffer.end(), Value.begin(), Value.end());
        ASSERT_NE(valuePosition, buffer.end());
        const size_t requiredPayloadSize = static_cast<size_t>(valuePosition - buffer.begin()) + Value.size() + 1;

        AZ_TEST_START_TRACE_SUPPRESSION;
        for (size_t truncatedSize = 0; truncatedSize < requiredPayloadSize; ++truncatedSize)
        {
            AZ::Symbol output = existing;
            AZ::IO::MemoryStream inputStream(buffer.data(), truncatedSize);
            EXPECT_FALSE(AZ::Utils::LoadObjectFromStreamInPlace(inputStream, output, &serializeContext))
                << "Truncated size " << truncatedSize;
            EXPECT_EQ(output, existing) << "Truncated size " << truncatedSize;
        }
        AZ_TEST_STOP_TRACE_SUPPRESSION_NO_COUNT;
    }

    TEST_F(SymbolTests, ObjectStreamRejectsInvalidValuesWithoutMutation)
    {
        constexpr AZStd::string_view Value{"PersistentSymbol"};
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        const AZ::Symbol input{Value};
        const AZ::Symbol existing{AZStd::string_view{"ExistingSymbol"}};

        for (const AZ::ObjectStream::StreamType streamType :
            {AZ::ObjectStream::ST_BINARY, AZ::ObjectStream::ST_XML, AZ::ObjectStream::ST_JSON})
        {
            AZStd::vector<char> buffer;
            AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
            ASSERT_TRUE(AZ::Utils::SaveObjectToStream(outputStream, streamType, &input, &serializeContext));

            const auto valuePosition = AZStd::search(buffer.begin(), buffer.end(), Value.begin(), Value.end());
            ASSERT_NE(valuePosition, buffer.end());
            if (streamType == AZ::ObjectStream::ST_BINARY)
            {
                ASSERT_NE(valuePosition + Value.size(), buffer.end());
                valuePosition[Value.size()] = 'x';
            }
            else
            {
                AZStd::string serialized(buffer.begin(), buffer.end());
                const size_t position = serialized.find(Value);
                ASSERT_NE(position, AZStd::string::npos);
                AZStd::string invalidText{"Bad"};
                invalidText.push_back(static_cast<char>(0xC0));
                invalidText.push_back(static_cast<char>(0xAF));
                invalidText.append("Symbol");
                serialized.replace(position, Value.size(), invalidText);
                buffer.assign(serialized.begin(), serialized.end());
            }

            AZ::Symbol output = existing;
            AZ::IO::MemoryStream inputStream(buffer.data(), buffer.size());
            AZ_TEST_START_TRACE_SUPPRESSION;
            EXPECT_FALSE(AZ::Utils::LoadObjectFromStreamInPlace(inputStream, output, &serializeContext));
            AZ_TEST_STOP_TRACE_SUPPRESSION(1);
            EXPECT_EQ(output, existing);
        }
    }

    TEST_F(SymbolTests, JsonObjectStreamRejectsEmbeddedNullWithoutMutation)
    {
        constexpr AZStd::string_view Value{"PersistentSymbol"};
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        const AZ::Symbol input{Value};
        const AZ::Symbol existing{AZStd::string_view{"ExistingSymbol"}};

        AZStd::vector<char> buffer;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(
            outputStream,
            AZ::ObjectStream::ST_JSON,
            &input,
            &serializeContext));

        AZStd::string serialized(buffer.begin(), buffer.end());
        const size_t position = serialized.find(Value);
        ASSERT_NE(position, AZStd::string::npos);
        serialized.replace(position, Value.size(), "Persistent\\u0000Symbol");
        buffer.assign(serialized.begin(), serialized.end());

        AZ::Symbol output = existing;
        AZ::IO::MemoryStream inputStream(buffer.data(), buffer.size());
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(AZ::Utils::LoadObjectFromStreamInPlace(inputStream, output, &serializeContext));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_EQ(output, existing);
    }

    TEST_F(SymbolTests, ObjectStreamRemovesReservedContainerElementWhenNestedSymbolFails)
    {
        constexpr AZStd::string_view Value{"NestedSymbol"};
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        SymbolContainer::Reflect(serializeContext);

        SymbolContainer input;
        input.m_symbols.emplace_back(Value);
        AZStd::vector<char> buffer;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(outputStream, AZ::ObjectStream::ST_BINARY, &input, &serializeContext));

        const auto valuePosition = AZStd::search(buffer.begin(), buffer.end(), Value.begin(), Value.end());
        ASSERT_NE(valuePosition, buffer.end());
        ASSERT_NE(valuePosition + Value.size(), buffer.end());
        valuePosition[Value.size()] = 'x';

        SymbolContainer output;
        AZ::IO::MemoryStream inputStream(buffer.data(), buffer.size());
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(AZ::Utils::LoadObjectFromStreamInPlace(inputStream, output, &serializeContext));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_TRUE(output.m_symbols.empty());
    }

} // namespace UnitTest
