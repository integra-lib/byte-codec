#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <integra/byte_codec.hpp>
#include <limits>
#include <span>

namespace
{

using integra::ByteReader;
using integra::Load;
using integra::Store;

constexpr auto BIG    = std::endian::big;
constexpr auto LITTLE = std::endian::little;

enum class Mode : std::uint16_t
{
    eIdle   = 0x0102U,
    eActive = 0xA0B0U,
};

template<std::endian ORDER, typename T>
[[nodiscard]] constexpr T RoundTrip(T value) noexcept
{
    const auto bytes = Store<ORDER>(value);
    return Load<ORDER, T>(std::span{bytes});
}

TEST(ByteCodecConceptTest, AcceptsOnlyTypesWithAFixedWireForm)
{
    static_assert(integra::Codable<char>);
    static_assert(integra::Codable<Mode>);
    static_assert(integra::Codable<double>);
    static_assert(!integra::Codable<bool>);
    static_assert(!integra::Codable<const std::uint8_t*>);
    static_assert(integra::CodableArray<std::array<std::int16_t, 3>>);
    static_assert(!integra::CodableArray<std::array<bool, 3>>);
    static_assert(!integra::ByteLike<char>);
    SUCCEED();
}

TEST(ByteCodecStoreTest, WritesMostSignificantByteFirstForBigEndian)
{
    EXPECT_EQ(Store<BIG>(std::uint32_t{0x12345678U}), (std::array<std::uint8_t, 4>{0x12U, 0x34U, 0x56U, 0x78U}));
}

TEST(ByteCodecStoreTest, WritesLeastSignificantByteFirstForLittleEndian)
{
    EXPECT_EQ(Store<LITTLE>(std::uint32_t{0x12345678U}), (std::array<std::uint8_t, 4>{0x78U, 0x56U, 0x34U, 0x12U}));
}

TEST(ByteCodecStoreTest, WritesStdByte)
{
    EXPECT_EQ((Store<BIG, std::uint16_t, std::byte>(0xABCDU)),
              (std::array<std::byte, 2>{std::byte{0xABU}, std::byte{0xCDU}}));
}

TEST(ByteCodecLoadTest, ReadsSixtyFourBitsWithTheTopByteSet)
{
    // The ported code shifted each byte as an int, so every byte above the fourth
    // was undefined behaviour and came back wrong.
    constexpr std::array<std::uint8_t, 8> BYTES{0xF1U, 0xE2U, 0xD3U, 0xC4U, 0xB5U, 0xA6U, 0x97U, 0x88U};
    EXPECT_EQ((Load<BIG, std::uint64_t>(std::span{BYTES})), 0xF1E2D3C4B5A69788U);
    EXPECT_EQ((Load<LITTLE, std::uint64_t>(std::span{BYTES})), 0x8897A6B5C4D3E2F1U);
}

TEST(ByteCodecLoadTest, ReadsStdByte)
{
    constexpr std::array<std::byte, 2> BYTES{std::byte{0x01U}, std::byte{0x02U}};
    EXPECT_EQ((Load<BIG, std::uint16_t>(std::span{BYTES})), 0x0102U);
    EXPECT_EQ((Load<LITTLE, std::uint16_t>(std::span{BYTES})), 0x0201U);
}

TEST(ByteCodecRoundTripTest, KeepsUnsignedExtremesOfEveryWidth)
{
    EXPECT_EQ(RoundTrip<BIG>(std::numeric_limits<std::uint8_t>::max()), std::numeric_limits<std::uint8_t>::max());
    EXPECT_EQ(RoundTrip<BIG>(std::numeric_limits<std::uint16_t>::max()), std::numeric_limits<std::uint16_t>::max());
    EXPECT_EQ(RoundTrip<LITTLE>(std::numeric_limits<std::uint32_t>::max()), std::numeric_limits<std::uint32_t>::max());
    EXPECT_EQ(RoundTrip<LITTLE>(std::numeric_limits<std::uint64_t>::max()), std::numeric_limits<std::uint64_t>::max());
}

TEST(ByteCodecRoundTripTest, KeepsSignedExtremesOfEveryWidth)
{
    EXPECT_EQ(RoundTrip<BIG>(std::numeric_limits<std::int8_t>::min()), std::numeric_limits<std::int8_t>::min());
    EXPECT_EQ(RoundTrip<BIG>(std::numeric_limits<std::int16_t>::min()), std::numeric_limits<std::int16_t>::min());
    EXPECT_EQ(RoundTrip<BIG>(std::numeric_limits<std::int32_t>::min()), std::numeric_limits<std::int32_t>::min());
    EXPECT_EQ(RoundTrip<LITTLE>(std::numeric_limits<std::int64_t>::min()), std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(RoundTrip<LITTLE>(std::int32_t{-2}), -2);
}

TEST(ByteCodecRoundTripTest, KeepsEnumsAndFloatingPoint)
{
    EXPECT_EQ(RoundTrip<BIG>(Mode::eActive), Mode::eActive);
    EXPECT_EQ(RoundTrip<LITTLE>(-1.5F), -1.5F);
    EXPECT_EQ(RoundTrip<BIG>(3.25), 3.25);
    EXPECT_EQ(Store<BIG>(1.0F), (std::array<std::uint8_t, 4>{0x3FU, 0x80U, 0x00U, 0x00U}));
}

TEST(ByteCodecRoundTripTest, IsUsableAtCompileTime)
{
    static_assert(RoundTrip<BIG>(std::uint64_t{0x0123456789ABCDEFU}) == 0x0123456789ABCDEFU);
    static_assert(Store<LITTLE>(std::uint16_t{0x1234U})[0] == 0x34U);
    SUCCEED();
}

TEST(ByteReaderTest, ReadsFieldsInSequence)
{
    constexpr std::array<std::uint8_t, 7> BYTES{0x01U, 0x02U, 0x03U, 0xA0U, 0xB0U, 0x04U, 0x05U};
    ByteReader<BIG> reader{BYTES};

    std::uint16_t first{};
    std::uint8_t second{};
    Mode mode{};
    reader >> first >> second >> mode;

    EXPECT_FALSE(reader.Failed());
    EXPECT_EQ(first, 0x0102U);
    EXPECT_EQ(second, 0x03U);
    EXPECT_EQ(mode, Mode::eActive);
    EXPECT_EQ(reader.Consumed(), 5U);
    EXPECT_EQ(reader.Remaining().size(), 2U);
    EXPECT_EQ(reader.Remaining()[0], 0x04U);
}

TEST(ByteReaderTest, ReadsArraysElementByElementInItsByteOrder)
{
    constexpr std::array<std::uint8_t, 4> BYTES{0x01U, 0x02U, 0x03U, 0x04U};
    ByteReader<LITTLE> reader{BYTES};

    std::array<std::uint16_t, 2> words{};
    reader >> words;

    EXPECT_FALSE(reader.Failed());
    EXPECT_EQ(words, (std::array<std::uint16_t, 2>{0x0201U, 0x0403U}));
}

TEST(ByteReaderTest, FailsAShortReadWithoutTouchingTheOutputOrTheCursor)
{
    constexpr std::array<std::uint8_t, 3> BYTES{0x01U, 0x02U, 0x03U};
    ByteReader<BIG> reader{BYTES};

    // The second field fits the buffer but not what is left of it: the check must be
    // against the remainder, not the whole.
    std::uint16_t head{};
    std::uint16_t tooWide{0xDEADU};
    reader >> head >> tooWide;

    EXPECT_TRUE(reader.Failed());
    EXPECT_EQ(head, 0x0102U);
    EXPECT_EQ(tooWide, 0xDEADU);
    EXPECT_EQ(reader.Consumed(), 2U);
}

TEST(ByteReaderTest, StaysFailedEvenWhenTheNextFieldWouldFit)
{
    constexpr std::array<std::uint8_t, 3> BYTES{0x01U, 0x02U, 0x03U};
    ByteReader<BIG> reader{BYTES};

    std::uint32_t tooWide{};
    std::uint8_t fits{0x55U};
    reader >> tooWide >> fits;

    EXPECT_TRUE(reader.Failed());
    EXPECT_EQ(fits, 0x55U);
    EXPECT_EQ(reader.Consumed(), 0U);
}

TEST(ByteReaderTest, ReadsAnArrayWholeOrNotAtAll)
{
    constexpr std::array<std::uint8_t, 3> BYTES{0x01U, 0x02U, 0x03U};
    ByteReader<BIG> reader{BYTES};

    std::array<std::uint16_t, 2> words{0xAAAAU, 0xBBBBU};
    reader >> words;

    EXPECT_TRUE(reader.Failed());
    EXPECT_EQ(words, (std::array<std::uint16_t, 2>{0xAAAAU, 0xBBBBU}));
    EXPECT_EQ(reader.Consumed(), 0U);
}

TEST(ByteReaderTest, ReadsAnExactlySizedBufferToTheEnd)
{
    constexpr std::array<std::uint8_t, 2> BYTES{0x12U, 0x34U};
    ByteReader<BIG> reader{BYTES};

    std::uint16_t value{};
    reader >> value;

    EXPECT_FALSE(reader.Failed());
    EXPECT_EQ(value, 0x1234U);
    EXPECT_TRUE(reader.Remaining().empty());
}

// The eBUS identification reply that a169-boiler-remote-controller parses: vendor,
// a five-character name, then software and hardware versions. The ported code read
// the versions big-endian; its error list it read big-endian too and then swapped
// every word, which a little-endian reader does directly.
TEST(ByteReaderTest, ParsesAnEbusIdentificationReply)
{
    constexpr std::array<std::byte, 10> REPLY{
        std::byte{0xB5U}, std::byte{'B'},   std::byte{'A'},   std::byte{'I'},   std::byte{'0'},
        std::byte{'0'},   std::byte{0x01U}, std::byte{0x02U}, std::byte{0x74U}, std::byte{0x03U}};
    ByteReader<BIG, std::byte> reader{REPLY};

    std::uint8_t vendor{};
    std::array<char, 5> name{};
    std::uint16_t softwareVersion{};
    std::uint16_t hardwareVersion{};
    reader >> vendor >> name >> softwareVersion >> hardwareVersion;

    EXPECT_FALSE(reader.Failed());
    EXPECT_EQ(vendor, 0xB5U);
    EXPECT_EQ(name, (std::array<char, 5>{'B', 'A', 'I', '0', '0'}));
    EXPECT_EQ(softwareVersion, 0x0102U);
    EXPECT_EQ(hardwareVersion, 0x7403U);
    EXPECT_TRUE(reader.Remaining().empty());
}

TEST(ByteReaderTest, IsUsableAtCompileTime)
{
    constexpr auto READ = [] {
        constexpr std::array<std::uint8_t, 4> BYTES{0x00U, 0x00U, 0x01U, 0x00U};
        ByteReader<BIG> reader{BYTES};
        std::uint32_t value{};
        reader >> value;
        return value;
    }();
    static_assert(READ == 0x100U);
    SUCCEED();
}

} // namespace
