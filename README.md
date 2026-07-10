# apex653

A modern C++26 implementation of the APEX interface defined by the ARINC 653 standard, built as
a single static library (`libapex653.a`) with a C-linkage public ABI, per service.

Work in progress — currently implementing the **filesystem** service (ARINC 653 Part 2).

## Building

Each preset configures its own build tree under `build/<preset>/`, so multiple presets can
coexist without clobbering each other.

### macOS (local development)

Output: `build/host-macos-gcc15/components/apex653/libapex653.a`

### Linux

```sh
cmake --preset linux-x86_64-gcc
cmake --build --preset linux-x86_64-gcc-debug
```

Available configure presets:

| Preset                    | Target                          | Runnable on this host? |
|---------------------------|----------------------------------|-------------------------|
| `host-macos-gcc15`        | native macOS (dev only)          | yes |
| `linux-x86_64-gcc`        | Linux x86_64, GCC                | yes, on Linux |
| `linux-x86_64-clang`      | Linux x86_64, Clang              | yes, on Linux |
| `linux-x86_32-gcc`        | Linux x86_32 (gcc `-m32`)        | yes, on Linux with multilib |
| `linux-arm64-gnu`         | Linux AArch64 (cross)            | build-only, needs target/QEMU to run |
| `linux-arm32-gnueabihf`   | Linux ARMv7 hard-float (cross)   | build-only, needs target/QEMU to run |

Swap the preset name in either command above (e.g. `linux-x86_64-clang`) to build for a
different target. The Linux presets assume `CMAKE_SYSTEM_NAME Linux` — this is a portable,
host-testable implementation validated across architectures on Linux; adapting it to a specific
ARINC 653 partitioning OS/RTOS target is a separate, later toolchain concern.

### Cleaning

Remove every preset's build tree:

```sh
rm -rf build/
```

Remove just one preset's build tree instead:

```sh
rm -rf build/host-macos-gcc15
```

`build/` is gitignored and holds only the CMake cache and generated Ninja files, so deleting it
is always safe. `cmake --build --preset <preset>` only drives an *existing* build tree — after a
clean, reconfigure before building again:

```sh
cmake --preset host-macos-gcc15
cmake --build --preset host-macos-gcc15-debug
```

## Layout

- `components/apex653/include/h/apex/` — public C-linkage headers implementing the standard's
  fixed C ABI (e.g. `apexFileSystem.h`).
- `components/apex653/src/` — implementation, mirrors `include/h/apex/`.
- `components/apex653/CMakeLists.txt` — builds the single `apex653` static library target
  (alias `apex653::apex653`).
- `cmake/` — toolchain files and build helpers.
