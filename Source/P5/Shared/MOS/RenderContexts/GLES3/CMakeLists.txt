# GLES3 + SDL2 render backend (skeleton, phases 3-4).

add_library(p5_rc_gles3 STATIC
    MDisplaySDL2.cpp
)

target_compile_options(p5_rc_gles3 PRIVATE ${P5_ENGINE_CXX_FLAGS})
target_include_directories(p5_rc_gles3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../MSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../../MSystem/PCH
)
target_link_libraries(p5_rc_gles3 PUBLIC p5_msystem SDL2::SDL2 GLESv2)
