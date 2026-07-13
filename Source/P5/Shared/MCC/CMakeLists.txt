# MCC / MRTC — engine runtime core (memory, threads, files, math, strings).
# Source list follows Lib_MCC.vcproj minus files that Mrtc.cpp #includes
# directly (unity-style) and minus Win32/Xbox/PS3-only translation units.
#
# Known exclusion: VPU/VPUWorkers.cpp assumes 32-bit pointers in the
# PPU<->SPU job ABI and needs a dedicated 64-bit port (see CLAUDE.md).

add_library(p5_mcc STATIC
    MCC_Dyn.cpp
    MDA_Hash.cpp
    MDataFile.cpp
    MFile.cpp
    MFile_AsyncCopy.cpp
    MFile_DiskUtil.cpp
    MFile_MegaFile.cpp
    MFile_Misc.cpp
    MFile_RAM.cpp
    MFile_Stream.cpp
    MFile_StreamMgr.cpp
    MFile_Stream_Compressed.cpp
    MFile_Stream_Disk.cpp
    MFile_Stream_MegaFile.cpp
    MFile_Stream_Memory.cpp
    MFile_Stream_RAM.cpp
    MFile_Stream_SubFile.cpp
    MFile_Stream_XDF.cpp
    MFile_XDF.cpp
    MFloat.cpp
    MMath.cpp
    MMath_SSE_M4.cpp
    MMath_SSE_V3.cpp
    MMath_fp2.cpp
    MMemMgrHeap.cpp
    MMemMgrPool.cpp
    MMisc.cpp
    MNetwork.cpp
    MRTC_Dyn.cpp
    MRTC_VPUManager.cpp
    MSIMD.cpp
    Mcomp.cpp
    Mda.cpp
    Mrtc.cpp
)

target_include_directories(p5_mcc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../Platform
)
target_include_directories(p5_mcc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/PCH)

target_compile_options(p5_mcc PRIVATE ${P5_ENGINE_CXX_FLAGS})
target_link_libraries(p5_mcc PUBLIC p5_zlib Threads::Threads)
