#pragma once

#include <array>
#include <bit>
#include <climits>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace integra
{

/// A buffer element: std::uint8_t or std::byte, either of them const or not.
template<typename Byte>
concept ByteLike =
    std::same_as<std::remove_cv_t<Byte>, std::uint8_t> || std::same_as<std::remove_cv_t<Byte>, std::byte>;

/// A value that goes on the wire as its own bytes: an integer other than bool, an enum,
/// or a floating-point type of 1, 2, 4 or 8 bytes. bool is left out because a byte other
/// than 0 or 1 has no bool to become.
template<typename T>
concept Codable =
    ((std::is_integral_v<T> && !std::same_as<T, bool>) || std::is_enum_v<T> ||
     std::is_floating_point_v<T>)&&(sizeof(T) == 1U || sizeof(T) == 2U || sizeof(T) == 4U || sizeof(T) == 8U);

/// Big- or little-endian. std::endian::native is accepted too: on every platform this
/// builds for it equals one of the two.
template<std::endian ORDER>
concept ByteOrder = ORDER == std::endian::big || ORDER == std::endian::little;

namespace detail
{

template<std::size_t SIZE>
struct UnsignedOfSize;

template<>
struct UnsignedOfSize<1U>
{
    using Type = std::uint8_t;
};

template<>
struct UnsignedOfSize<2U>
{
    using Type = std::uint16_t;
};

template<>
struct UnsignedOfSize<4U>
{
    using Type = std::uint32_t;
};

template<>
struct UnsignedOfSize<8U>
{
    using Type = std::uint64_t;
};

/// Every value is shifted as this type, never as the int a byte promotes to — the
/// ported code did the latter, which made each byte above the fourth undefined.
template<typename T>
using Bits = typename UnsignedOfSize<sizeof(T)>::Type;

/// How far the byte at `index` sits from the least significant end.
template<std::endian ORDER, typename T>
[[nodiscard]] constexpr std::size_t ShiftOf(std::size_t index) noexcept
{
    const std::size_t significance = ORDER == std::endian::big ? sizeof(T) - 1U - index : index;
    return significance * CHAR_BIT;
}

template<typename T>
struct IsCodableArray : std::false_type
{};

template<Codable T, std::size_t SIZE>
struct IsCodableArray<std::array<T, SIZE>> : std::true_type
{};

} // namespace detail

/// A std::array of Codable elements, read element by element.
template<typename T>
concept CodableArray = detail::IsCodableArray<T>::value;

/// `value` as sizeof(T) bytes in the given order.
template<std::endian ORDER, Codable T, ByteLike Byte = std::uint8_t>
    requires ByteOrder<ORDER>
[[nodiscard]] constexpr std::array<Byte, sizeof(T)> Store(T value) noexcept
{
    const auto bits = std::bit_cast<detail::Bits<T>>(value);
    std::array<Byte, sizeof(T)> bytes{};
    for (std::size_t index = 0U; index < sizeof(T); ++index)
    {
        bytes[index] = static_cast<Byte>(static_cast<std::uint8_t>(bits >> detail::ShiftOf<ORDER, T>(index)));
    }
    return bytes;
}

/// The T held in exactly sizeof(T) bytes in the given order. The extent is part of the
/// type, so there is no short read to report.
template<std::endian ORDER, Codable T, ByteLike Byte>
    requires ByteOrder<ORDER>
[[nodiscard]] constexpr T Load(std::span<Byte, sizeof(T)> bytes) noexcept
{
    using Bits = detail::Bits<T>;
    Bits bits{0U};
    for (std::size_t index = 0U; index < sizeof(T); ++index)
    {
        const auto byte = static_cast<Bits>(static_cast<std::uint8_t>(bytes[index]));
        bits            = static_cast<Bits>(bits | static_cast<Bits>(byte << detail::ShiftOf<ORDER, T>(index)));
    }
    return std::bit_cast<T>(bits);
}

/// Reads values one after another from a borrowed buffer, which must outlive the reader.
///
/// A read that does not fit leaves its target and the cursor as they were and marks the
/// reader failed; every read after that is a no-op, so a chain of reads needs one check
/// at its end:
///
///     ByteReader<std::endian::big> reader{payload};
///     reader >> vendor >> name >> version;
///     if (reader.Failed()) { ... }
template<std::endian ORDER, ByteLike Byte = std::uint8_t>
    requires ByteOrder<ORDER>
class ByteReader
{
public:
    constexpr explicit ByteReader(std::span<const Byte> data) noexcept
        : m_data{data}
    {}

    constexpr ByteReader(const ByteReader&)            = default;
    constexpr ByteReader& operator=(const ByteReader&) = default;
    constexpr ByteReader(ByteReader&&)                 = default;
    constexpr ByteReader& operator=(ByteReader&&)      = default;
    constexpr ~ByteReader()                            = default;

    template<Codable T>
    constexpr ByteReader& operator>>(T& value) noexcept
    {
        if (Reserve(sizeof(T)))
        {
            value = ReadUnchecked<T>();
        }
        return *this;
    }

    /// All elements or none: a short buffer leaves the whole array untouched.
    template<CodableArray T>
    constexpr ByteReader& operator>>(T& values) noexcept
    {
        using Element = typename T::value_type;
        if (Reserve(sizeof(Element) * values.size()))
        {
            for (auto& value : values)
            {
                value = ReadUnchecked<Element>();
            }
        }
        return *this;
    }

    [[nodiscard]] constexpr bool Failed() const noexcept
    {
        return m_failed;
    }

    [[nodiscard]] constexpr std::size_t Consumed() const noexcept
    {
        return m_offset;
    }

    [[nodiscard]] constexpr std::span<const Byte> Remaining() const noexcept
    {
        return m_data.subspan(m_offset);
    }

private:
    /// Whether `size` more bytes may be read; if not, the reader becomes failed.
    [[nodiscard]] constexpr bool Reserve(std::size_t size) noexcept
    {
        if (m_failed || size > m_data.size() - m_offset)
        {
            m_failed = true;
            return false;
        }
        return true;
    }

    template<Codable T>
    [[nodiscard]] constexpr T ReadUnchecked() noexcept
    {
        const T value  = Load<ORDER, T>(m_data.subspan(m_offset).template first<sizeof(T)>());
        m_offset      += sizeof(T);
        return value;
    }

    std::span<const Byte> m_data;
    std::size_t m_offset{0U};
    bool m_failed{false};
};

} // namespace integra
