# byte-codec

Integers, enums and floats to and from big- or little-endian bytes, plus a cursor
that reads a packet field by field. All constexpr.

Part of [hwlib](https://github.com/integra-lib) — architecture-independent C++20
components shared between firmware projects. Header-only,
no exceptions, no RTTI.

```cpp
#include <hwlib/utilities/byte_codec.hpp>

// One value, fixed size: nothing can be short.
const auto bytes = hwlib::utilities::Store<std::endian::big>(std::uint32_t{0x12345678U}); // {0x12, 0x34, 0x56, 0x78}
const auto value = hwlib::utilities::Load<std::endian::big, std::uint32_t>(std::span{bytes});

// A packet: read the fields in order, check once at the end.
hwlib::utilities::ByteReader<std::endian::little, std::byte> reader{payload};
reader >> vendor >> name >> version; // name is a std::array, read whole or not at all
if (reader.Failed())
{
    // too short: every field from the first that did not fit is untouched
}
```

A value is an integer other than `bool`, an enum or a floating-point type of 1, 2, 4
or 8 bytes. A buffer is `std::uint8_t` or `std::byte`. The reader borrows its buffer.

## Use it

```bash
git submodule add git@github.com:integra-lib/byte-codec.git external/hwlib/byte-codec
```

```cmake
add_subdirectory(external/hwlib/byte-codec)
target_link_libraries(app PRIVATE Hwlib::byte_codec)
```

```cpp
#include <hwlib/utilities/byte_codec.hpp>
```

Each component carries its own include directory, so this header stays unreachable
until the component is linked: a forgotten dependency is a compile error rather than
a build that happens to work.

## Versioning

Every component is released on its own, tagged `vX.Y.Z`. Pre-1.0, a minor release may
break the API, which is why dependants accept a single minor.

```bash
git -C external/hwlib/byte-codec fetch --tags
git -C external/hwlib/byte-codec checkout v0.2.0
git add external/hwlib/byte-codec && git commit -m "build: bump byte-codec to v0.2.0"
```

## In a consumer's CI

The component is an ordinary submodule, so the build needs it checked out. On GitLab
that means `GIT_SUBMODULE_STRATEGY: normal` (or `recursive`) on every job that builds —
not only on the ones that run unit tests.

## Develop it

```bash
git submodule update --init          # ci-shared, needed by pre-commit
cmake -S . -B build && cmake --build build -j && ctest --test-dir build
```

Tests are built only when this repository is the top-level project, so a consumer
never builds them and never fetches GoogleTest.

The style configs are symlinks into the `ci-shared` submodule, and the pipeline comes
from the same place. On GitHub this repository carries a self-contained build-and-test
workflow instead: a workflow token cannot read another private repository, so neither
a shared workflow nor the submodule is reachable there. The shared setup is what
GitLab will use.
