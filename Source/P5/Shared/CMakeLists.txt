# Engine modules for the GLES3+SDL2 port. Built with -DENGINE_MODULES=ON.
# Modules are added here as they start to compile (see CLAUDE.md).

add_compile_definitions(TARGET_LINUX_SDL2)

# Smoke test: the target/platform header must compile standalone and define
# the fundamental types with the sizes the engine assumes.
add_executable(platform_check Platform/PlatformCheck.cpp)
