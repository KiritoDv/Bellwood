set(ACPORT_AUDIO_NEOS_SOURCES
    src/static/jaudio_NES/internal/astest.c
    src/static/jaudio_NES/internal/audioconst.c
    src/static/jaudio_NES/internal/audiotable.c
    src/static/jaudio_NES/internal/audiowork.c
    src/static/jaudio_NES/internal/channel.c
    src/static/jaudio_NES/internal/driver.c
    src/static/jaudio_NES/internal/effect.c
    src/static/jaudio_NES/internal/memory.c
    src/static/jaudio_NES/internal/os.c
    src/static/jaudio_NES/internal/rate.c
    src/static/jaudio_NES/internal/rspsim.c
    src/static/jaudio_NES/internal/sample.c
    src/static/jaudio_NES/internal/sub_sys.c
    src/static/jaudio_NES/internal/system.c
    src/static/jaudio_NES/internal/track.c
)

set(ACPORT_AUDIO_GAME_SOURCES
    src/static/jaudio_NES/game/audioheaders.c
    src/static/jaudio_NES/game/dummyprobe.c
    src/static/jaudio_NES/game/emusound.c
    src/static/jaudio_NES/game/game64.c
    src/static/jaudio_NES/game/kappa.c
    src/static/jaudio_NES/game/melody.c
    src/static/jaudio_NES/game/radio.c
    src/static/jaudio_NES/game/rhythm.c
    src/static/jaudio_NES/game/staff.c
)

set(ACPORT_AUDIO_GLUE_SOURCES
    src/port/audio/NeosGlue.c
)

set(ACPORT_AUDIO_PORT_SOURCES
    src/port/audio/ACAudio.cpp
    src/port/audio/AiCtrl.cpp
    src/port/audio/NeosResources.cpp
    src/port/audio/importers/NeosAudioResources.cpp
)

get_target_property(ACPORT_AUDIO_EXISTING_SOURCES ${PROJECT_NAME} SOURCES)
list(FILTER ACPORT_AUDIO_EXISTING_SOURCES EXCLUDE REGEX "(^|/)src/port/audio/.*\\.(c|cpp)$")
set_property(TARGET ${PROJECT_NAME} PROPERTY SOURCES ${ACPORT_AUDIO_EXISTING_SOURCES})

get_target_property(ACPORT_AUDIO_GAME_EXISTING_SOURCES acgame SOURCES)
list(FILTER ACPORT_AUDIO_GAME_EXISTING_SOURCES EXCLUDE REGEX "(^|/)src/port/stubs/AiMixStub\\.c$")
set_property(TARGET acgame PROPERTY SOURCES ${ACPORT_AUDIO_GAME_EXISTING_SOURCES})

target_sources(acgame PRIVATE ${ACPORT_AUDIO_NEOS_SOURCES} ${ACPORT_AUDIO_GAME_SOURCES} ${ACPORT_AUDIO_GLUE_SOURCES})
target_compile_definitions(acgame PRIVATE ACPORT_AUDIO=1)
if (NOT MSVC)
    set_source_files_properties(${ACPORT_AUDIO_NEOS_SOURCES} PROPERTIES COMPILE_OPTIONS "-fsigned-char")
endif()

add_library(acaudio OBJECT ${ACPORT_AUDIO_PORT_SOURCES})
target_include_directories(acaudio BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/port/audio
    ${CMAKE_SOURCE_DIR}/libultraship/include
    ${CMAKE_SOURCE_DIR}/libultraship/include/libultraship
)
target_link_libraries(acaudio PRIVATE libultraship)
target_compile_definitions(acaudio PRIVATE ACPORT_AUDIO=1 F3DEX_GBI_2 TARGET_PC VERSION=VER_GAFE01_00 SPDLOG_ACTIVE_LEVEL=0)
if (MSVC)
    target_compile_options(acaudio PRIVATE /w)
else()
    target_compile_options(acaudio PRIVATE -Wno-zero-length-array -Wno-c99-extensions)
    if (ACPORT_ASAN)
        target_compile_options(acaudio PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    endif()
endif()

target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:acaudio>)
target_compile_definitions(${PROJECT_NAME} PRIVATE ACPORT_AUDIO=1)
add_dependencies(${PROJECT_NAME} acaudio)

option(ACPORT_AUDIO_TEST "Build the headless Neos render test" OFF)
if (ACPORT_AUDIO_TEST)
    set(ACPORT_AUDIO_TEST_C_SOURCES
        ${ACPORT_AUDIO_NEOS_SOURCES}
        src/static/jaudio_NES/game/audioheaders.c
        src/static/jaudio_NES/game/emusound.c
        src/port/audio/NeosGlue.c
        src/port/audio/test/NeosTestPlatform.c
    )
    add_library(acaudio_test_c OBJECT ${ACPORT_AUDIO_TEST_C_SOURCES})
    target_include_directories(acaudio_test_c BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libultraship/include
        ${CMAKE_SOURCE_DIR}/libultraship/include/libultraship
    )
    target_compile_definitions(acaudio_test_c PRIVATE
        ACPORT_AUDIO=1 ACPORT_AUDIO_TEST=1 F3DEX_GBI_2 TARGET_PC VERSION=VER_GAFE01_00 DEBUG=0 NDEBUG)
    set_target_properties(acaudio_test_c PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
    if (NOT MSVC)
        target_compile_options(acaudio_test_c PRIVATE -funsigned-char -fwrapv -fno-strict-aliasing -w)
    endif()

    add_executable(NeosRenderTest
        src/port/audio/AiCtrl.cpp
        src/port/audio/NeosResources.cpp
        src/port/audio/importers/NeosAudioResources.cpp
        src/port/audio/test/NeosRenderTest.cpp
        $<TARGET_OBJECTS:acaudio_test_c>
    )
    target_include_directories(NeosRenderTest BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/port/audio
        ${CMAKE_SOURCE_DIR}/libultraship/include
        ${CMAKE_SOURCE_DIR}/libultraship/include/libultraship
    )
    target_compile_definitions(NeosRenderTest PRIVATE
        ACPORT_AUDIO=1 ACPORT_AUDIO_TEST=1 F3DEX_GBI_2 TARGET_PC VERSION=VER_GAFE01_00 SPDLOG_ACTIVE_LEVEL=0)
    target_link_libraries(NeosRenderTest PRIVATE libultraship)
    if (ACPORT_ASAN AND NOT MSVC)
        target_compile_options(acaudio_test_c PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_compile_options(NeosRenderTest PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(NeosRenderTest PRIVATE -fsanitize=address)
    endif()
endif()
