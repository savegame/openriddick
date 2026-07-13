# XRClasses — the remaining XR translation units from Lib_XRClasses.vcproj
# (physics, solids, shaders, block navigation, AnimGraph2). Paths are
# repo-root relative; XRShader_*.cpp variants that XRShader.cpp includes
# unity-style are not listed.

add_library(p5_xrclasses STATIC
    ../../XR/Phys/WPhysCapsule.cpp
    ../../XR/Phys/WPhysCollision.cpp
    ../../XR/Phys/WPhysCollisionObsolete.cpp
    ../../XR/Phys/WPhysColllisionPolyOBB.cpp
    ../../XR/Phys/WPhysOBB.cpp
    ../../XR/Phys/WPhysPCS.cpp
    ../../XR/Phys/WPhysPolyOBB_fp4.cpp
    ../../XR/Phys/WPhysTT.cpp
    ../../XR/Phys/WPhysTriLine.cpp
    ../../XR/Phys/WPhysTriOBB_fp4.cpp
    ../../XR/Phys/WPhysTriSphere.cpp
    ../../XR/Solids/MCSolid.cpp
    ../../XR/Solids/XRIndexedSolid.cpp
    ../../XR/Solids/XRThinSolid.cpp
    ../../XR/Solids/XWSolid.cpp
    ../../XR/XRBlockNav.cpp
    ../../XR/XRBlockNavGrid.cpp
    ../../XR/XRBlockNavResult.cpp
    ../../XR/XRClass.cpp
    ../../XR/XRLight.cpp
    ../../XR/XRMediumDesc.cpp
    ../../XR/XRNavGraph.cpp
    ../../XR/XRNavGraphBuilder.cpp
    ../../XR/XRShader.cpp
    ../../XR/XRUtil.cpp
    ../../XR/XRUtilRS.cpp
    ../../XR/XRUtilSubdivSurf.cpp
    ../../XR/XRVBContainer.cpp
    ../../XR/XRVBEContext.cpp
    ../../XR/XRVBMHelper.cpp
    ../../XR/XRVertexBuffer.cpp
)

target_compile_options(p5_xrclasses PRIVATE ${P5_ENGINE_CXX_FLAGS})
target_include_directories(p5_xrclasses PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../XR/PCH)
target_link_libraries(p5_xrclasses PUBLIC p5_xr)
