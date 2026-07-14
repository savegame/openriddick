cmake_minimum_required(VERSION 3.20)
project(OpenRiddick CXX C)

# ---------------------------------------------------------------------------
# OpenRiddick — GLES3 + SDL2 port of the Starbreeze P5 engine.
# Targets: Linux x86_64, Linux ARM (aarch64/armhf).
# See CLAUDE.md for the porting plan and current status.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(ENGINE_MODULES "Build engine modules (MCC/MSystem/XR/...). OFF builds only in-tree SDK libraries until the port compiles." OFF)

set(P5 ${CMAKE_CURRENT_SOURCE_DIR}/Source/P5)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# The SDK snapshots are from ~2003-2005; silence their known warnings, keep
# warnings enabled for new port code.
set(SDK_C_FLAGS -w)

# ---------------------------------------------------------------------------
# SDK: zlib (in-tree snapshot, zlib 1.2.x)
# ---------------------------------------------------------------------------
add_library(p5_zlib STATIC
    ${P5}/SDK/ZLib/adler32.c
    ${P5}/SDK/ZLib/compress.c
    ${P5}/SDK/ZLib/crc32.c
    ${P5}/SDK/ZLib/deflate.c
    ${P5}/SDK/ZLib/gzio.c
    ${P5}/SDK/ZLib/infback.c
    ${P5}/SDK/ZLib/inffast.c
    ${P5}/SDK/ZLib/inflate.c
    ${P5}/SDK/ZLib/inftrees.c
    ${P5}/SDK/ZLib/trees.c
    ${P5}/SDK/ZLib/uncompr.c
    ${P5}/SDK/ZLib/zutil.c
)
target_include_directories(p5_zlib PUBLIC ${P5}/SDK/ZLib)
target_compile_options(p5_zlib PRIVATE ${SDK_C_FLAGS})

# ---------------------------------------------------------------------------
# SDK: libpng (in-tree snapshot; C-only paths, no MMX asm)
# ---------------------------------------------------------------------------
add_library(p5_png STATIC
    ${P5}/SDK/Png/png.c
    ${P5}/SDK/Png/pngerror.c
    ${P5}/SDK/Png/pngget.c
    ${P5}/SDK/Png/pngmem.c
    ${P5}/SDK/Png/pngpread.c
    ${P5}/SDK/Png/pngread.c
    ${P5}/SDK/Png/pngrio.c
    ${P5}/SDK/Png/pngrtran.c
    ${P5}/SDK/Png/pngrutil.c
    ${P5}/SDK/Png/pngset.c
    ${P5}/SDK/Png/pngtrans.c
    ${P5}/SDK/Png/pngwio.c
    ${P5}/SDK/Png/pngwrite.c
    ${P5}/SDK/Png/pngwtran.c
    ${P5}/SDK/Png/pngwutil.c
)
target_include_directories(p5_png PUBLIC ${P5}/SDK/Png)
target_compile_definitions(p5_png PRIVATE PNG_NO_MMX_CODE PNG_NO_ASSEMBLER_CODE)
target_compile_options(p5_png PRIVATE ${SDK_C_FLAGS})
target_link_libraries(p5_png PUBLIC p5_zlib)

# ---------------------------------------------------------------------------
# SDK: libogg + libvorbis (in-tree snapshot)
# ---------------------------------------------------------------------------
add_library(p5_ogg STATIC
    ${P5}/SDK/Vorbis/ogg/src/bitwise.c
    ${P5}/SDK/Vorbis/ogg/src/framing.c
)
target_include_directories(p5_ogg PUBLIC ${P5}/SDK/Vorbis/ogg/include)
target_compile_options(p5_ogg PRIVATE ${SDK_C_FLAGS})

add_library(p5_vorbis STATIC
    ${P5}/SDK/Vorbis/vorbis/lib/analysis.c
    ${P5}/SDK/Vorbis/vorbis/lib/bitrate.c
    ${P5}/SDK/Vorbis/vorbis/lib/block.c
    ${P5}/SDK/Vorbis/vorbis/lib/codebook.c
    ${P5}/SDK/Vorbis/vorbis/lib/envelope.c
    ${P5}/SDK/Vorbis/vorbis/lib/floor0.c
    ${P5}/SDK/Vorbis/vorbis/lib/floor1.c
    ${P5}/SDK/Vorbis/vorbis/lib/info.c
    ${P5}/SDK/Vorbis/vorbis/lib/lookup.c
    ${P5}/SDK/Vorbis/vorbis/lib/lpc.c
    ${P5}/SDK/Vorbis/vorbis/lib/lsp.c
    ${P5}/SDK/Vorbis/vorbis/lib/mapping0.c
    ${P5}/SDK/Vorbis/vorbis/lib/mdct.c
    ${P5}/SDK/Vorbis/vorbis/lib/psy.c
    ${P5}/SDK/Vorbis/vorbis/lib/registry.c
    ${P5}/SDK/Vorbis/vorbis/lib/res0.c
    ${P5}/SDK/Vorbis/vorbis/lib/sharedbook.c
    ${P5}/SDK/Vorbis/vorbis/lib/smallft.c
    ${P5}/SDK/Vorbis/vorbis/lib/synthesis.c
    ${P5}/SDK/Vorbis/vorbis/lib/window.c
    ${P5}/SDK/Vorbis/vorbis/lib/vorbisfile.c
)
target_include_directories(p5_vorbis
    PUBLIC ${P5}/SDK/Vorbis/vorbis/include
    PRIVATE ${P5}/SDK/Vorbis/vorbis/lib)
target_compile_options(p5_vorbis PRIVATE ${SDK_C_FLAGS})
target_link_libraries(p5_vorbis PUBLIC p5_ogg)

# ---------------------------------------------------------------------------
# SDK: NeuQuant (palette quantizer, used by MImageQuantize)
# ---------------------------------------------------------------------------
add_library(p5_neuquant STATIC ${P5}/SDK/NeuQuant/NEUQUANT.C)
set_source_files_properties(${P5}/SDK/NeuQuant/NEUQUANT.C PROPERTIES LANGUAGE C)
target_include_directories(p5_neuquant PUBLIC ${P5}/SDK/NeuQuant)
target_compile_options(p5_neuquant PRIVATE ${SDK_C_FLAGS})

# ---------------------------------------------------------------------------
# Engine modules (ported incrementally; see CLAUDE.md phase 2)
# ---------------------------------------------------------------------------
if(ENGINE_MODULES)
    find_package(SDL2 REQUIRED)
    find_package(Threads REQUIRED)

    # All engine modules build as the Linux/SDL2 target
    add_compile_definitions(TARGET_LINUX_SDL2)
    # Old-codebase compile flags:
    #  -flax-vector-conversions: the vec128 SSE code freely mixes __m128/__m128i
    set(P5_ENGINE_CXX_FLAGS -w -flax-vector-conversions)

    add_subdirectory(Source/P5/Shared)
    add_subdirectory(Source/P5/Projects/Main/GameWorld)
    add_subdirectory(Source/P5/Projects/Main/GameClasses)
    add_subdirectory(Source/P5/Projects/Main/Exe)
endif()
