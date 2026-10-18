set(BELLWOOD_AUDIO_NEOS_SOURCES
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

set(BELLWOOD_AUDIO_GAME_SOURCES
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

set(BELLWOOD_AUDIO_GLUE_SOURCES
    src/port/audio/NeosGlue.c
)

set(BELLWOOD_AUDIO_PORT_SOURCES
    src/port/audio/ACAudio.cpp
    src/port/audio/AiCtrl.cpp
    src/port/audio/ModSound.cpp
    src/port/audio/NeosResources.cpp
    src/port/audio/importers/NeosAudioResources.cpp
)

target_sources(acgame PRIVATE ${BELLWOOD_AUDIO_NEOS_SOURCES} ${BELLWOOD_AUDIO_GAME_SOURCES} ${BELLWOOD_AUDIO_GLUE_SOURCES})
if (NOT MSVC)
    set_source_files_properties(${BELLWOOD_AUDIO_NEOS_SOURCES} PROPERTIES COMPILE_OPTIONS "-fsigned-char")
endif()

add_library(acaudio OBJECT ${BELLWOOD_AUDIO_PORT_SOURCES})
target_include_directories(acaudio BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/port/audio
    ${CMAKE_SOURCE_DIR}/libultraship/include
    ${CMAKE_SOURCE_DIR}/libultraship/include/libultraship
)
target_link_libraries(acaudio PRIVATE libultraship)
target_compile_definitions(acaudio PRIVATE F3DEX_GBI_2 TARGET_PC VERSION=VER_GAFE01_00 SPDLOG_ACTIVE_LEVEL=0)
if (MSVC)
    target_compile_options(acaudio PRIVATE /w)
else()
    target_compile_options(acaudio PRIVATE -Wno-zero-length-array -Wno-c99-extensions)
    if (BELLWOOD_ASAN)
        target_compile_options(acaudio PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    endif()
endif()

target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:acaudio>)
add_dependencies(${PROJECT_NAME} acaudio)
