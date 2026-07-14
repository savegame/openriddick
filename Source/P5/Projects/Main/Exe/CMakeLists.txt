# The game executable. XRApp.cpp defines the application class and
# MACRO_MAIN provides main() -> Linux_Main (MMain_Linux.cpp).

add_executable(openriddick
    WGameContextMain.cpp
    WGameContext_P6.cpp
    WGameMultiplayerHandler.cpp
    XRApp.cpp
    ../GameClasses/GameClasses_Dyn.cpp
    ../GameWorld/GameWorld_Dyn.cpp
    ../../../Shared/MCC/MCC_Dyn.cpp
    ../../../Shared/MCC/MRTC_Dyn.cpp
    ../../../Shared/MOS/MSystem/MSystem_Dyn.cpp
    ../../../Shared/MOS/XR/XR_Dyn.cpp
    ../../../Shared/MOS/Classes/Registry/MRegistryNavigator.cpp
    ../../../Shared/MOS/Classes/Render/MRenderCapture.cpp
    ../../../Shared/MOS/MMain.cpp
    ../../../Shared/MOS/MSystem/MSystem_Linux.cpp
)

target_compile_options(openriddick PRIVATE ${P5_ENGINE_CXX_FLAGS} -fpermissive)
target_include_directories(openriddick PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/PCH
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../Shared/MOS/MSystem/PCH
)
target_link_libraries(openriddick PRIVATE
    -Wl,--start-group
    p5_gameclasses p5_gameworld p5_xrclasses p5_xr p5_msystem p5_mcc p5_neuquant
    -Wl,--end-group
)
