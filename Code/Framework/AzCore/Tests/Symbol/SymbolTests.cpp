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
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Symbol/Internal/SymbolTable.h>
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

    TEST_F(SymbolTests, EmptySymbolHasStableNullState)
    {
        const AZ::Symbol symbol;

        EXPECT_TRUE(symbol.IsEmpty());
        EXPECT_FALSE(symbol);
        EXPECT_TRUE(symbol.GetStringView().empty());
        EXPECT_STREQ(symbol.GetCStr(), "");
        EXPECT_EQ(AZ::SymbolHash{}(symbol), 0);
    }

    TEST_F(SymbolTests, EqualSpellingsHaveCanonicalPointerIdentity)
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

    TEST_F(SymbolTests, MaximumSpellingIsAcceptedAndNullTerminated)
    {
        const AZStd::string spelling(AZ::Symbol::MaxLength, 'm');
        const AZ::Symbol symbol{spelling};

        ASSERT_TRUE(symbol);
        EXPECT_EQ(symbol.GetStringView(), spelling);
        EXPECT_EQ(symbol.GetCStr()[AZ::Symbol::MaxLength], '\0');
    }

    TEST_F(SymbolTests, InvalidDirectConstructionTerminates)
    {
        const AZStd::string oversized(AZ::Symbol::MaxLength + 1, 'x');
        constexpr char embeddedNull[] = {'a', '\0', 'b'};
        constexpr char control[] = {'a', '\n', 'b'};
        constexpr char malformed[] = {static_cast<char>(0xC0), static_cast<char>(0xAF)};
        constexpr char unsupportedXml[] = {static_cast<char>(0xEF), static_cast<char>(0xBF), static_cast<char>(0xBE)};
        const AZStd::string_view embeddedNullView{embeddedNull, sizeof(embeddedNull)};
        const AZStd::string_view controlView{control, sizeof(control)};
        const AZStd::string_view malformedView{malformed, sizeof(malformed)};
        const AZStd::string_view unsupportedXmlView{unsupportedXml, sizeof(unsupportedXml)};

        EXPECT_DEATH((void)AZ::Symbol{oversized}, "");
        EXPECT_DEATH((void)AZ::Symbol{embeddedNullView}, "");
        EXPECT_DEATH((void)AZ::Symbol{controlView}, "");
        EXPECT_DEATH((void)AZ::Symbol{malformedView}, "");
        EXPECT_DEATH((void)AZ::Symbol{unsupportedXmlView}, "");
    }

    TEST_F(SymbolTests, Utf8ValidationAcceptsValidBoundarySequences)
    {
        constexpr char valid[] = {
            static_cast<char>(0xC2), static_cast<char>(0x80),
            static_cast<char>(0xE0), static_cast<char>(0xA0), static_cast<char>(0x80),
            static_cast<char>(0xF4), static_cast<char>(0x8F), static_cast<char>(0xBF), static_cast<char>(0xBF),
        };

        EXPECT_EQ(
            AZ::Internal::ValidateSymbolSpelling(AZStd::string_view{valid, sizeof(valid)}),
            AZ::Internal::SymbolValidationError::None);
        const AZ::Symbol symbol{AZStd::string_view{valid, sizeof(valid)}};
        EXPECT_TRUE(symbol);
    }

    TEST_F(SymbolTests, FindDoesNotCreateMissingSymbol)
    {
        constexpr AZStd::string_view spelling{"FindWithoutInterning"};
        AZ::Symbol found;

        EXPECT_FALSE(AZ::Internal::FindSymbol(found, spelling));
        const AZ::Symbol inserted{spelling};
        EXPECT_TRUE(AZ::Internal::FindSymbol(found, spelling));
        EXPECT_EQ(found, inserted);
    }

    TEST_F(SymbolTests, HashCollisionStillUsesExactBytes)
    {
        AZ::Internal::SymbolTable table;
        constexpr AZ::u64 hash = 0xDEADBEEF;

        const AZ::Internal::SymbolEntry* first = table.Intern("CollisionOne", hash);
        const AZ::Internal::SymbolEntry* second = table.Intern("CollisionTwo", hash);

        EXPECT_NE(first, second);
        EXPECT_EQ(table.Intern("CollisionOne", hash), first);
        EXPECT_EQ(table.Intern("CollisionTwo", hash), second);
    }

    TEST_F(SymbolTests, AllocationFailureTerminatesInsteadOfReturningEmpty)
    {
        EXPECT_DEATH(
            {
                FailingSymbolAllocator allocator{0};
                AZ::Internal::SymbolTable table{allocator};
                (void)table.Intern("InitialStorageFailure", 1);
            },
            "");

        EXPECT_DEATH(
            {
                FailingSymbolAllocator allocator{1};
                AZ::Internal::SymbolTable table{allocator};
                (void)table.Intern("PartialStorageFailure", 1);
            },
            "");

        EXPECT_DEATH(
            {
                FailingSymbolAllocator allocator{2};
                AZ::Internal::SymbolTable table{allocator};
                (void)table.Intern("ArenaFailure", 1);
            },
            "");

        EXPECT_DEATH(
            {
                FailingSymbolAllocator allocator{3};
                AZ::Internal::SymbolTable table{allocator};
                for (size_t index = 0; index < 15; ++index)
                {
                    const AZStd::string spelling = AZStd::string::format("ResizeFailure%zu", index);
                    (void)table.Intern(spelling, 1);
                }
            },
            "");
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
        const AZStd::string spelling(AZ::Symbol::MaxLength, 'p');
        const AZ::Symbol input{spelling};
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

    TEST_F(SymbolTests, ObjectStreamRejectsInvalidValuesWithoutMutation)
    {
        constexpr AZStd::string_view Spelling{"PersistentSymbol"};
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        const AZ::Symbol input{Spelling};
        const AZ::Symbol existing{AZStd::string_view{"ExistingSymbol"}};

        for (const AZ::ObjectStream::StreamType streamType :
            {AZ::ObjectStream::ST_BINARY, AZ::ObjectStream::ST_XML, AZ::ObjectStream::ST_JSON})
        {
            AZStd::vector<char> buffer;
            AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
            ASSERT_TRUE(AZ::Utils::SaveObjectToStream(outputStream, streamType, &input, &serializeContext));

            const auto spellingPosition = AZStd::search(buffer.begin(), buffer.end(), Spelling.begin(), Spelling.end());
            ASSERT_NE(spellingPosition, buffer.end());
            if (streamType == AZ::ObjectStream::ST_BINARY)
            {
                ASSERT_NE(spellingPosition + Spelling.size(), buffer.end());
                spellingPosition[Spelling.size()] = 'x';
            }
            else
            {
                AZStd::string serialized(buffer.begin(), buffer.end());
                const size_t position = serialized.find(Spelling);
                ASSERT_NE(position, AZStd::string::npos);
                AZStd::string_view invalidText{"Bad\\nSymbol"};
                if (streamType == AZ::ObjectStream::ST_XML)
                {
                    invalidText = "Bad&#xA;Symbol";
                }
                serialized.replace(position, Spelling.size(), invalidText);
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

    TEST_F(SymbolTests, ObjectStreamRemovesReservedContainerElementWhenNestedSymbolFails)
    {
        constexpr AZStd::string_view Spelling{"NestedSymbol"};
        AZ::SerializeContext serializeContext;
        AZ::Symbol::Reflect(&serializeContext);
        SymbolContainer::Reflect(serializeContext);

        SymbolContainer input;
        input.m_symbols.emplace_back(Spelling);
        AZStd::vector<char> buffer;
        AZ::IO::ByteContainerStream<AZStd::vector<char>> outputStream(&buffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(outputStream, AZ::ObjectStream::ST_BINARY, &input, &serializeContext));

        const auto spellingPosition = AZStd::search(buffer.begin(), buffer.end(), Spelling.begin(), Spelling.end());
        ASSERT_NE(spellingPosition, buffer.end());
        ASSERT_NE(spellingPosition + Spelling.size(), buffer.end());
        spellingPosition[Spelling.size()] = 'x';

        SymbolContainer output;
        AZ::IO::MemoryStream inputStream(buffer.data(), buffer.size());
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(AZ::Utils::LoadObjectFromStreamInPlace(inputStream, output, &serializeContext));
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);
        EXPECT_TRUE(output.m_symbols.empty());
    }

} // namespace UnitTest
