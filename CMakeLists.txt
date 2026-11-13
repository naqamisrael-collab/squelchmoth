cmake_minimum_required(VERSION 3.22)

# SQUELCHMOTH — a Tracktion Engine powered DAW you can drive with Claude.
set (TARGET_NAME Squelchmoth)

# Runs on macOS Big Sur (11.0) and newer, Intel and Apple Silicon.
set (CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project (${TARGET_NAME} VERSION 1.14.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set (TE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tracktion_engine" CACHE PATH "Path to tracktion_engine checkout")

add_subdirectory (${TE_DIR}/modules/juce ./cmake_build_juce)
add_subdirectory (${TE_DIR}/modules ./cmake_build_tracktion)

juce_add_gui_app (${TARGET_NAME}
    PRODUCT_NAME "Squelchmoth"
    COMPANY_NAME "Jamar Stewart"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Jamar Stewart. All rights reserved."
    BUNDLE_ID "com.jamarstewart.squelchmoth"
    VERSION 1.14.0
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_1024.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Squelchmoth needs the microphone/audio input to record."
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE
    PLUGINHOST_AU TRUE
    DOCUMENT_EXTENSIONS squelch)

juce_generate_juce_header (${TARGET_NAME})

juce_add_binary_data (${TARGET_NAME}BinaryData SOURCES
    Resources/logo.svg
    Resources/welcome_banner.jpg
    Resources/fonts/ScoreFont.otf
    Resources/licence/keys3.bin
    Resources/plugart/pa_mothsynth.jpg
    Resources/plugart/pa_chrysalis.jpg
    Resources/plugart/pa_hivedrums.jpg
    Resources/plugart/pa_prismeq.jpg
    Resources/plugart/pa_scalpel.jpg
    Resources/plugart/pa_lanterntilt.jpg
    Resources/plugart/pa_antennaair.jpg
    Resources/plugart/pa_deepsub.jpg
    Resources/plugart/pa_veilfilter.jpg
    Resources/plugart/pa_capacitor.jpg
    Resources/plugart/pa_cocoon.jpg
    Resources/plugart/pa_mothpress.jpg
    Resources/plugart/pa_pupabuss.jpg
    Resources/plugart/pa_varimu.jpg
    Resources/plugart/pa_clamplimiter.jpg
    Resources/plugart/pa_veilgate.jpg
    Resources/plugart/pa_nectardeess.jpg
    Resources/plugart/pa_snaptransient.jpg
    Resources/plugart/pa_popknock.jpg
    Resources/plugart/pa_embertape.jpg
    Resources/plugart/pa_scorchdrive.jpg
    Resources/plugart/pa_lumenchannel.jpg
    Resources/plugart/pa_lumenbuss.jpg
    Resources/plugart/pa_ashendesk.jpg
    Resources/plugart/pa_furnacedensity.jpg
    Resources/plugart/pa_triodeglow.jpg
    Resources/plugart/pa_vinylpress.jpg
    Resources/plugart/pa_gritbits.jpg
    Resources/plugart/pa_nebulaverb.jpg
    Resources/plugart/pa_cavernir.jpg
    Resources/plugart/pa_pocketspaces.jpg
    Resources/plugart/pa_starfield.jpg
    Resources/plugart/pa_dustdelay.jpg
    Resources/plugart/pa_echowing.jpg
    Resources/plugart/pa_flutterchorus.jpg
    Resources/plugart/pa_swarmensemble.jpg
    Resources/plugart/pa_phasemoth.jpg
    Resources/plugart/pa_quivertremolo.jpg
    Resources/plugart/pa_helixpitch.jpg
    Resources/plugart/pa_doublewingadt.jpg
    Resources/plugart/pa_wingspan.jpg
    Resources/plugart/pa_monomaker.jpg
    Resources/plugart/pa_ditherdust.jpg
    Resources/plugart/pa_trimgain.jpg
    Resources/plugart/pa_midishaper.jpg
    Resources/plugart/pa_mothtune.jpg
    Resources/plugart/pa_mothtunefx.jpg
    Resources/plugart/pa_mothtunefx2.jpg
    Resources/plugart/pa_apexdyn.jpg
    Resources/plugart/pa_apexlim.jpg
    Resources/plugart/pa_apexmb.jpg
    Resources/plugart/pa_spectraeq.jpg
    Resources/plugart/pa_torquecomp.jpg
    Resources/plugart/pa_summitlim.jpg
    Resources/plugart/pa_atriumverb.jpg
    Resources/plugart/pa_apexmb2.jpg
    Resources/plugart/pa_sibyldeess.jpg
    Resources/plugart/pa_portcullis.jpg
    Resources/plugart/pa_kiln.jpg
    Resources/plugart/pa_apexsub.jpg
    Resources/plugart/pa_auroraverb.jpg
    Resources/plugart/pa_optoleveler.jpg
    Resources/plugart/pa_pulseeq.jpg
    Resources/plugart/pa_oilcan.jpg
    Resources/plugart/pa_silkladder.jpg
    Resources/plugart/pa_haloring.jpg
    Resources/plugart/pa_twelvephase.jpg
    Resources/plugart/pa_bucketbrigade.jpg
    Resources/plugart/pa_moth808.jpg
    Resources/plugart/pa_blossom.jpg
    Resources/plugart/pa_mothsynth2.jpg
    Resources/plugart/pa_venom.jpg
    Resources/plugart/pa_simmoth.jpg
    Resources/plugart/pa_minimoth.jpg
    Resources/plugart/pa_thump.jpg
    Resources/plugart/pa_dial10.jpg
    Resources/plugart/pa_hivepads.jpg
    Resources/plugart/pa_blossom2.jpg
    Resources/plugart/pa_rackchannel.jpg
    Resources/plugart/pa_hexwing.jpg
    Resources/plugart/pa_toxin.jpg
    Resources/plugart/pa_lantern.jpg
    Resources/plugart/pa_lanternexo.jpg
    Resources/plugart/pa_larvabass.jpg
    Resources/plugart/pa_thornbass.jpg
    Resources/plugart/pa_carapace.jpg
    Resources/plugart/pa_livedrumsynth.jpg
    Resources/plugart/pa_drumpadmodel.jpg
    Resources/plugart/pa_thumplive.jpg
    Resources/plugart/pa_timeweaver.jpg
    Resources/plugart/pa_larvafilter.jpg
    Resources/plugart/pa_apextriad.jpg
    Resources/plugart/pa_cutlab.jpg
    Resources/plugart/pa_chrysalis2.jpg
    Resources/plugart/pa_chrysalissf.jpg
    Resources/plugart/pa_mandible.jpg
    Resources/plugart/pa_mandiblelite.jpg
    Resources/plugart/pa_pollen.jpg
    Resources/plugart/pa_cinder.jpg
    Resources/plugart/pa_wingbeat.jpg
    Resources/plugart/pa_wingbeatjr.jpg
    Resources/plugart/pa_foilplate.jpg
    Resources/plugart/pa_foilplatelite.jpg
    Resources/plugart/pa_radiant.jpg
    Resources/plugart/pa_radiantpre.jpg
    Resources/plugart/pa_maw.jpg
    Resources/plugart/pa_mawlite.jpg
    Resources/plugart/pa_mothkey.jpg
    Resources/plugart/pa_sirencomp.jpg
    Resources/plugart/pa_sireneq.jpg
    Resources/plugart/pa_hushdeess2.jpg
    Resources/plugart/pa_vellum.jpg Resources/plugart/pa_tremor.jpg Resources/plugart/pa_orbitpan.jpg Resources/plugart/pa_mirrorshift.jpg
    Resources/plugart/pa_mirrorshiftlite.jpg Resources/plugart/pa_shutter.jpg Resources/plugart/pa_shutterlite.jpg
    Resources/plugart/pa_shard.jpg Resources/plugart/pa_murmuration.jpg Resources/plugart/pa_masquerade.jpg
    Resources/plugart/pa_larvafilter2.jpg
    Resources/plugart/pa_twelvephase2.jpg
    Resources/plugart/pa_hiverack.jpg
    Resources/plugart/pa_sirenverb.jpg
    Resources/plugart/pa_miccloset.jpg
    Resources/plugart/pa_larynx.jpg
    Resources/plugart/pa_exhale.jpg
    Resources/plugart/pa_hivechoir.jpg
    Resources/plugart/pa_doublewing2.jpg
    Resources/plugart/pa_mutant.jpg
    Resources/plugart/pa_voxmoth.jpg
    Resources/plugart/pa_talkwing.jpg
    Resources/plugart/pa_cleanroom.jpg)

target_sources (${TARGET_NAME} PRIVATE
    Source/Main.cpp)
if (APPLE)
    # Native helpers: Pro Tools-style floating panels (stay above the Edit window, hide when the app is in the background)
    target_sources (${TARGET_NAME} PRIVATE Source/MacWindowHelpers.mm)
endif()

target_include_directories (${TARGET_NAME} PRIVATE
    Source
    ThirdParty)

# Identical floating-point rounding on every platform (no fused multiply-add): DSP measures the same on Apple
# Silicon as on Intel and Windows (1.14: a feedback loop at exactly unity and a square root at exactly zero differed).
if (MSVC)
    target_compile_options (${TARGET_NAME} PRIVATE "/clang:-ffp-contract=off")
else()
    target_compile_options (${TARGET_NAME} PRIVATE "-ffp-contract=off")
endif()

target_compile_definitions (${TARGET_NAME} PRIVATE
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_LADSPA=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=0   # LGPL - removed in 1.7 (Signalsmith Stretch, MIT, does every elastic mode)
    TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH=1
    JUCE_USE_MP3AUDIOFORMAT=1                    # 1.12: MP3 import (the format's patents have expired); export is Squelchmoth's own encoder
    TRACKTION_AIR_WINDOWS=1)

target_link_libraries (${TARGET_NAME} PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_cryptography
    ${TARGET_NAME}BinaryData)

set_target_properties (${TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden)

if (APPLE)
    target_compile_options (${TARGET_NAME} PRIVATE "-fno-aligned-allocation")
endif()

# App name/version only matter to Squelchmoth's own code (keeps the JUCE/Tracktion objects cached between versions).
set_source_files_properties (Source/Main.cpp PROPERTIES COMPILE_DEFINITIONS
    "JUCE_APPLICATION_NAME_STRING=\"Squelchmoth\";JUCE_APPLICATION_VERSION_STRING=\"${PROJECT_VERSION}\"")

# Control-flow mistakes that silently break whole menus must never compile again (Squelchmoth's own code).
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties (Source/Main.cpp PROPERTIES COMPILE_OPTIONS "-Werror=dangling-else;-Werror=misleading-indentation")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package (Threads REQUIRED)
    target_link_libraries (${TARGET_NAME} PRIVATE "-latomic" Threads::Threads)
endif()

if (WIN32)
    target_link_libraries (${TARGET_NAME} PRIVATE ws2_32)
    target_link_options (${TARGET_NAME} PRIVATE "/STACK:8388608")   # 8 MB main-thread stack, like macOS and Linux
    target_sources (${TARGET_NAME} PRIVATE Source/WinWindowHelpers.cpp)   # secondary windows owned by the main one: one taskbar icon
endif()

if (APPLE AND CMAKE_CROSSCOMPILING)
    target_sources (${TARGET_NAME} PRIVATE Source/osxcross_compat.c)
endif()
