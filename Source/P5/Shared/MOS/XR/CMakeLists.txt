# XR — 3D engine core (scene, shaders-over-attribs, vertex buffers,
# animation, navigation, physics) plus the XRModels model types that
# Lib_XR.vcproj includes directly.

add_library(p5_xr STATIC
    ../Classes/BitString/MBitString.cpp
    ../Classes/Render/MRenderVPGen.cpp
    ../XRModels/Model_BSP/WBSPFog.cpp
    ../XRModels/Model_BSP/WBSPLight.cpp
    ../XRModels/Model_BSP/WBSPLoader.cpp
    ../XRModels/Model_BSP/WBSPMisc.cpp
    ../XRModels/Model_BSP/WBSPModel.cpp
    ../XRModels/Model_BSP/WBSPPhys.cpp
    ../XRModels/Model_BSP/WBSPPortal.cpp
    ../XRModels/Model_BSP/WBSPSceneGraph.cpp
    ../XRModels/Model_BSP/WBSPSpline.cpp
    ../XRModels/Model_BSP/WBSPWallmark.cpp
    ../XRModels/Model_BSP/XWCommon.cpp
    ../XRModels/Model_BSP/XWCommon_OctAABB.cpp
    ../XRModels/Model_BSP2/WBSP2Fog.cpp
    ../XRModels/Model_BSP2/WBSP2Light.cpp
    ../XRModels/Model_BSP2/WBSP2Loader.cpp
    ../XRModels/Model_BSP2/WBSP2Misc.cpp
    ../XRModels/Model_BSP2/WBSP2Model.cpp
    ../XRModels/Model_BSP2/WBSP2Phys.cpp
    ../XRModels/Model_BSP2/WBSP2Portal.cpp
    ../XRModels/Model_BSP2/WBSP2SceneGraph.cpp
    ../XRModels/Model_BSP2/WBSP2Wallmark.cpp
    ../XRModels/Model_BSP2/XW2Common.cpp
    ../XRModels/Model_BSP3/WBSP3Fog.cpp
    ../XRModels/Model_BSP3/WBSP3Light.cpp
    ../XRModels/Model_BSP3/WBSP3Loader.cpp
    ../XRModels/Model_BSP3/WBSP3Misc.cpp
    ../XRModels/Model_BSP3/WBSP3Model.cpp
    ../XRModels/Model_BSP3/WBSP3Phys.cpp
    ../XRModels/Model_BSP3/WBSP3Portal.cpp
    ../XRModels/Model_BSP3/WBSP3SceneGraph.cpp
    ../XRModels/Model_BSP3/WBSP3Wallmark.cpp
    ../XRModels/Model_BSP3/XW3Common.cpp
    ../XRModels/Model_BSP4/WBSP4Loader.cpp
    ../XRModels/Model_BSP4/WBSP4Misc.cpp
    ../XRModels/Model_BSP4/WBSP4Model.cpp
    ../XRModels/Model_BSP4/WBSP4Phys.cpp
    ../XRModels/Model_BSP4/XW4Common.cpp
    ../XRModels/Model_BSP4Glass/WBSP4Glass.cpp
    ../XRModels/Model_BSP4Glass/WBSP4GlassCommon.cpp
    ../XRModels/Model_BSP4Glass/WBSP4GlassCrush.cpp
    ../XRModels/Model_BSP4Glass/WBSP4GlassDelaunay2D.cpp
    ../XRModels/Model_BSP4Glass/WBSP4GlassLoader.cpp
    ../XRModels/Model_BSP4Glass/WBSP4GlassPhys_Box.cpp
    ../XRModels/Model_BSP4Glass/WBSP4GlassPhys_Line.cpp
    ../XRModels/Model_BSP4Glass/WBSP4GlassPhys_Sphere.cpp
    ../XRModels/Model_BSP4Glass/wBSP4GlassPhys.cpp
    ../XRModels/Model_Flare/WModel_Flare.cpp
    ../XRModels/Model_Multi/WModel_Multi.cpp
    ../XRModels/Model_MultiTriMesh/WModel_MultiTriMesh.cpp
    ../XRModels/Model_Sky/WSky.cpp
    ../XRModels/Model_SolidPhys/XRModel_SolidPhys.cpp
    ../XRModels/Model_SolidPhys/XRModel_SolidPhysLoader.cpp
    ../XRModels/Model_TriMesh/WTriMesh.cpp
    ../XRModels/Model_TriMesh/WTriMeshDC.cpp
    ../XRModels/Model_TriMesh/XMDCommn.cpp
    WBSPCommon.cpp
    XRAnim.cpp
    XRAnimCompressed.cpp
    XRAnimDataKey.cpp
    XRAnimGraph2/AGCompiler2/AnimGraph2Compiler.cpp
    XRAnimGraph2/AGCompiler2/AnimGraph2Compiler_GraphBlock.cpp
    XRAnimGraph2/AGCompiler2/AnimGraph2Compiler_Reactions.cpp
    XRAnimGraph2/AGCompiler2/AnimGraph2Compiler_State.cpp
    XRAnimGraph2/AnimGraph2.cpp
    XRAnimGraph2/AnimGraph2_Action.cpp
    XRAnimGraph2/AnimGraph2_AnimLayer.cpp
    XRAnimGraph2/AnimGraph2_CCheck.cpp
    XRAnimGraph2/AnimGraph2_GraphBlock.cpp
    XRAnimGraph2/AnimGraph2_IO.cpp
    XRAnimGraph2/AnimGraph2_LogDump.cpp
    XRAnimGraph2/AnimGraph2_Reaction.cpp
    XRAnimGraph2/AnimGraph2_State.cpp
    XRAnimOptimizer.cpp
    XRCloth.cpp
    XREngine.cpp
    XRFog.cpp
    XRPContainer.cpp
    XRSkeleton.cpp
    XRSurf.cpp
    XRSurfaceContext.cpp
    XRVBContext.cpp
    XRVBManager.cpp
    XRVBOperators.cpp
    XRVBOperators_NVidia.cpp
    XRVBOperators_GC.cpp
)

target_include_directories(p5_xr PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(p5_xr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/PCH)

target_compile_options(p5_xr PRIVATE ${P5_ENGINE_CXX_FLAGS})
target_link_libraries(p5_xr PUBLIC p5_msystem)
