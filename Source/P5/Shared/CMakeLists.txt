# Engine modules for the GLES3+SDL2 port. Built with -DENGINE_MODULES=ON.
# Modules are added here as they start to compile (see CLAUDE.md).

add_compile_definitions(TARGET_LINUX_SDL2)

find_package(Threads REQUIRED)

# Old-codebase compile flags shared by all engine modules:
#  -flax-vector-conversions: the vec128 SSE code freely mixes __m128/__m128i
set(P5_ENGINE_CXX_FLAGS -w -flax-vector-conversions)

add_subdirectory(MCC)

# Smoke test: the target/platform header must compile standalone and define
# the fundamental types with the sizes the engine assumes.
add_executable(platform_check Platform/PlatformCheck.cpp)
