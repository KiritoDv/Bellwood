include(${CMAKE_CURRENT_LIST_DIR}/bellwood-game-sources.cmake)

add_library(acgame OBJECT
    ${BELLWOOD_GAME_SOURCES}
    ${BELLWOOD_DATA_SOURCES}
    src/assets/asset_paths.c
)

target_include_directories(acgame BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libultraship/include
    ${CMAKE_SOURCE_DIR}/libultraship/include/libultraship
    ${CMAKE_BINARY_DIR}
)

target_compile_definitions(acgame PRIVATE
    F3DEX_GBI_2
    TARGET_PC
    VERSION=VER_GAFE01_00
    DEBUG=0
    NDEBUG
)

set_target_properties(acgame PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)

if (MSVC)
    target_compile_options(acgame PRIVATE /J /w)
else()
    target_compile_options(acgame PRIVATE
        -funsigned-char
        -fwrapv
        -fno-strict-aliasing
        -Werror=implicit-function-declaration
        -Wno-incompatible-pointer-types
        -Wno-incompatible-pointer-types-discards-qualifiers
        -Wno-int-conversion
        -Wno-unused-value
        -Wno-constant-conversion
        -Wno-parentheses
        -Wno-missing-braces
        -Wno-tautological-constant-out-of-range-compare
        -Wno-pointer-sign
        -Wno-unknown-pragmas
        -Wno-switch
        -Wno-dangling-else
        -Wno-shift-negative-value
        -Wno-unsequenced
        -Wno-implicit-const-int-float-conversion
        -Wno-extern-initializer
        -Wno-invalid-source-encoding
    )
endif()

if (BELLWOOD_ASAN AND NOT MSVC)
    target_compile_options(acgame PRIVATE -fsanitize=address -fno-omit-frame-pointer)
endif()

target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:acgame>)
add_dependencies(${PROJECT_NAME} acgame)
