# MSystem — engine system layer (display/render abstraction, textures,
# images, input, sound, script, registry, console).
# Source list follows Lib_MSystem.vcproj minus platform TUs that are not
# used on Linux (Win32/Xenon/PS3, ASIO, FaceFX/Bink/XMV SDK wrappers) and
# minus MMain* (built by the executable). The SDL2 platform implementation
# (MSystem_Linux.cpp) arrives in phase 3.

add_library(p5_msystem STATIC
    ../Classes/Miscellaneous/MMd5.cpp
    Input/MInput.cpp
    Input/MInputEnvelope.cpp
    Input/MInputScankey.cpp
    Input/MInput_SDL2.cpp
    MSystem_Core.cpp
    Misc/MConsole.cpp
    Misc/MLocalizer.cpp
    Misc/MPerfGraph.cpp
    Misc/MRTC_Task.cpp
    Misc/MRegistry.cpp
    Misc/MRegistry_Compiled.cpp
    Misc/MRegistry_Dynamic.cpp
    Raster/JPGLib/jcapimin.cpp
    Raster/JPGLib/jcapistd.cpp
    Raster/JPGLib/jccoefct.cpp
    Raster/JPGLib/jccolor.cpp
    Raster/JPGLib/jcdctmgr.cpp
    Raster/JPGLib/jcfileread.cpp
    Raster/JPGLib/jchuff.cpp
    Raster/JPGLib/jcinit.cpp
    Raster/JPGLib/jcmainct.cpp
    Raster/JPGLib/jcmarker.cpp
    Raster/JPGLib/jcmaster.cpp
    Raster/JPGLib/jcomapi.cpp
    Raster/JPGLib/jcparam.cpp
    Raster/JPGLib/jcphuff.cpp
    Raster/JPGLib/jcprepct.cpp
    Raster/JPGLib/jcsample.cpp
    Raster/JPGLib/jctrans.cpp
    Raster/JPGLib/jdapimin.cpp
    Raster/JPGLib/jdapistd.cpp
    Raster/JPGLib/jdatadst.cpp
    Raster/JPGLib/jdatasrc.cpp
    Raster/JPGLib/jdcoefct.cpp
    Raster/JPGLib/jdcolor.cpp
    Raster/JPGLib/jddctmgr.cpp
    Raster/JPGLib/jdhuff.cpp
    Raster/JPGLib/jdinput.cpp
    Raster/JPGLib/jdmainct.cpp
    Raster/JPGLib/jdmarker.cpp
    Raster/JPGLib/jdmaster.cpp
    Raster/JPGLib/jdmerge.cpp
    Raster/JPGLib/jdphuff.cpp
    Raster/JPGLib/jdpostct.cpp
    Raster/JPGLib/jdsample.cpp
    Raster/JPGLib/jdtrans.cpp
    Raster/JPGLib/jerror.cpp
    Raster/JPGLib/jfdctflt.cpp
    Raster/JPGLib/jfdctfst.cpp
    Raster/JPGLib/jfdctint.cpp
    Raster/JPGLib/jidctflt.cpp
    Raster/JPGLib/jidctfst.cpp
    Raster/JPGLib/jidctint.cpp
    Raster/JPGLib/jidctred.cpp
    Raster/JPGLib/jmemmgr.cpp
    Raster/JPGLib/jmemnobs.cpp
    Raster/JPGLib/jquant1.cpp
    Raster/JPGLib/jquant2.cpp
    Raster/JPGLib/jutils.cpp
    Raster/MDisplay.cpp
    Raster/MDisplayPresent.cpp
    Raster/MImage.cpp
    Raster/MImageCompress3DC.cpp
    Raster/MImageCompressCTX.cpp
    Raster/MImageCompressJPG.cpp
    Raster/MImageCompressS3TC.cpp
    Raster/MImageCompressSound.cpp
    Raster/MImageCompressVQ.cpp
    Raster/MImageCubemapSampler.cpp
    Raster/MImageIO.cpp
    Raster/MImagePalette.cpp
    Raster/MImageQuantize.cpp
    Raster/MImageScaler.cpp
    Raster/MRender.cpp
    Raster/MTexture.cpp
    Raster/MTextureContainerBink.cpp
    Raster/MTextureContainerBinkDummy.cpp
    Raster/MTextureContainerMPEG.cpp
    Raster/MTextureContainerTheora.cpp
    Raster/MTextureContainerWMV.cpp
    Raster/MTextureContainerXTC2.cpp
    Raster/MTextureContainers.cpp
    Raster/MTextureContainersPostFilter.cpp
    Script/MScript.cpp
    Script/MScript_ConstantParsers.cpp
    Script/MScript_Convert.cpp
    Script/MScript_Language.cpp
    Script/MScript_MultiType.cpp
    Script/MScript_RegisterContext.cpp
    Script/MScript_Syntax.cpp
    Sound/LipSync/LipSyncAnno.cpp
    Sound/MSound.cpp
    Sound/MSound_Codec.cpp
    Sound/MSound_Core.cpp
    Sound/MSound_Mixer.cpp
    Sound/MSound_Mixer_DSPEffects.cpp
    Sound/MSound_Mixer_Worker.cpp
    Sound/MSound_Mixer_Worker_CPUVPUAccess.cpp
    Sound/MSound_SFXDesc.cpp
    Sound/MSound_WaveContainer_Plain.cpp
    Sound/MSound_Waveform.cpp
    Sound/SCMixer/MSound_SCMixer.cpp
    Sound/SDL2/MSound_SDL2.cpp
    Sound/Codecs/MSound_Codec_Vorbis.cpp
    Sound/Vorbis/MSound_Vorbis.cpp
    Sound/LipSync/LipSync.cpp
    Sound/MSound_CoreDualStream.cpp
)

target_include_directories(p5_msystem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_include_directories(p5_msystem PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/PCH)

target_compile_options(p5_msystem PRIVATE ${P5_ENGINE_CXX_FLAGS})
target_link_libraries(p5_msystem PUBLIC p5_mcc p5_png p5_vorbis SDL2::SDL2)
