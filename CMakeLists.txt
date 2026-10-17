cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(PORT_NAME "bellwood")
set(PORT_ARCHIVE "${PORT_NAME}.o2r")
set(GAME_ARCHIVE "ac.o2r")

# Set the project version and language
project(${PORT_NAME} VERSION 0.1.0 LANGUAGES C CXX)
include(FetchContent)

include(cmake/bellwood-cvars.cmake)
include(cmake/lus-cvars.cmake)

set(NATO_PHONETIC_ALPHABET
  "Alfa" "Bravo" "Charlie" "Delta" "Echo" "Foxtrot" "Golf" "Hotel"
  "India" "Juliett" "Kilo" "Lima" "Mike" "November" "Oscar" "Papa"
  "Quebec" "Romeo" "Sierra" "Tango" "Uniform" "Victor" "Whiskey"
  "Xray" "Yankee" "Zulu"
)

# Get the patch version number from the project version
math(EXPR PATCH_INDEX "${PROJECT_VERSION_PATCH}")

# Use the patch number to select the correct word
list(GET NATO_PHONETIC_ALPHABET ${PATCH_INDEX} PROJECT_PATCH_WORD)

set(PROJECT_BUILD_NAME "${PORT_NAME} ${PROJECT_PATCH_WORD}" CACHE STRING "" FORCE)
set(PROJECT_TEAM "${PORT_NAME}" CACHE STRING "" FORCE)

# Get Git Information
execute_process(
    COMMAND git branch --show-current
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(CMAKE_PROJECT_GIT_BRANCH "${GIT_BRANCH}" CACHE STRING "Git branch" FORCE)

execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get only the first 7 characters of the hash
string(SUBSTRING "${GIT_COMMIT_HASH}" 0 7 SHORT_COMMIT_HASH)

set(CMAKE_PROJECT_GIT_COMMIT_HASH "${SHORT_COMMIT_HASH}" CACHE STRING "Git commit hash" FORCE)

execute_process(
    COMMAND git describe --tags --abbrev=0 --exact-match HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_COMMIT_TAG)
    set(GIT_COMMIT_TAG "" CACHE STRING "Git commit tag" FORCE)
endif()

set(CMAKE_PROJECT_GIT_COMMIT_TAG "${GIT_COMMIT_TAG}" CACHE STRING "Git commit tag" FORCE)

if(APPLE)
  enable_language(OBJCXX)
endif()

# Set the minimum version of CMake and the deployment target for macOS
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")

# Set the C++ standard and enable the MSVC parallel build option
set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")
set(CMAKE_C_STANDARD 11 CACHE STRING "The C standard to use")
# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

# Add a custom module path to locate additional CMake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(YAML_CPP_STATIC_DEFINE ON)
add_compile_definitions(YAML_CPP_STATIC_DEFINE)

if (WIN32)
include(cmake/automate-vcpkg.cmake)
set(VCPKG_TRIPLET x64-windows-static)
set(VCPKG_TARGET_TRIPLET x64-windows-static)
vcpkg_bootstrap()
vcpkg_install_packages(zlib bzip2 libzip libpng sdl2[vulkan] glew glfw3 nlohmann-json tinyxml2 spdlog)

add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if (MSVC)
set(CPP "${CMAKE_C_COMPILER}" "/EP")
else()
set(CPP "${CMAKE_C_COMPILER}" "-E" "-P" "-Wno-trigraphs" "-x" "c")
endif()

################################################################################
# Sources
################################################################################
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/port/build.c.in ${CMAKE_CURRENT_BINARY_DIR}/src/port/build.c @ONLY)

################################################################################
# Set target arch type if empty. Visual studio solution generator provides it.
################################################################################
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(NOT CMAKE_VS_PLATFORM_NAME)
        set(CMAKE_VS_PLATFORM_NAME "x64")
    endif()
    message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

    if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64"
        OR "${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32"))
        message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
    endif()
endif()

################################################################################
# Use solution folders feature
################################################################################
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

################################################################################
# Global configuration types
################################################################################
if (NOT MSVC)
set(CMAKE_C_FLAGS_RELEASE "-O1 -fno-strict-aliasing -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O1 -DNDEBUG")
set(CMAKE_OBJCXX_FLAGS_RELEASE "-O1 -DNDEBUG")
endif()

add_compile_definitions(
    _LANGUAGE_C
    _USE_MATH_DEFINES
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_compile_definitions(ENABLE_OPENGL=1)
endif()

# Find necessary libraries
if (UNIX AND NOT APPLE)
find_package(OpenGL REQUIRED)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/properties.h.in ${CMAKE_CURRENT_BINARY_DIR}/properties.h @ONLY)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

file(GLOB_RECURSE ALL_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
    "src/port/*.h"
    "src/port/*.hpp"
    "src/port/*.c"
    "src/port/*.cpp"
)
file(GLOB_RECURSE PORT_GAME_C_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
    "src/port/boot/*.c"
    "src/port/platform/*.c"
    "src/port/input/*.c"
    "src/port/stubs/*.c"
    "src/port/famicom/*.c"
)
list(REMOVE_ITEM ALL_FILES ${PORT_GAME_C_FILES})
include(src/port/importer/ac/sources.cmake OPTIONAL)
if (AC_IMPORTER_SOURCES)
    list(FILTER ALL_FILES EXCLUDE REGEX "^src/port/importer/ac/")
    list(APPEND ALL_FILES ${AC_IMPORTER_SOURCES})
endif()

# Create source groups that match the real file directory paths
foreach(source IN LISTS ALL_FILES)
    get_filename_component(source_path "${source}" PATH)
    string(REPLACE "/" "\\" source_path_adjusted "${source_path}")
    source_group("${source_path_adjusted}" FILES "${source}")
endforeach()

list(APPEND ALL_FILES ${CMAKE_CURRENT_BINARY_DIR}/src/port/build.c)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND ALL_FILES Resource.rc)
endif()

add_executable(${PROJECT_NAME} ${ALL_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    F3DEX_GBI_2
    TARGET_PC
    VERSION=VER_GAFE01_00
    AVOID_UB=1
    PORT_NAME="${PORT_NAME}"
    PORT_ARCHIVE="${PORT_ARCHIVE}"
    GAME_ARCHIVE="${GAME_ARCHIVE}"
)

################################################################################
# MSVC runtime library
################################################################################
if (MSVC)
  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	  get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
	  if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
		  string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
			  $<$<CONFIG:Debug>:
                MultiThreadedDebug
			  >
			  $<$<CONFIG:Release>:
                MultiThreaded
			  >
			  $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
		  )
	  elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
		  string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
			  $<$<CONFIG:Debug>:
				  MultiThreadedDebug
			  >
			  $<$<CONFIG:Release>:
				  MultiThreaded
			  >
			  $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
		  )
	  endif()
	  set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})
  endif()
endif()

#==============================================================================#
# Libultraship Integration                                                     #
#==============================================================================#

# Removes MPQ/OTR support
set(EXCLUDE_MPQ_SUPPORT TRUE CACHE BOOL "")
set(ENABLE_EXP_AUTO_CONFIGURE_CONTROLLERS ON CACHE BOOL "")
add_compile_definitions(EXCLUDE_MPQ_SUPPORT)

set(GBI_UCODE F3DEX_GBI_2 CACHE STRING "GBI ucode version used to build libultraship" FORCE)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/libultraship/include
  ${CMAKE_CURRENT_SOURCE_DIR}/libultraship/include/libultraship
  ${SDL2_INCLUDE_DIRS}
  ${GLEW_INCLUDE_DIRS}
)

add_subdirectory(libultraship ${CMAKE_BINARY_DIR}/libultraship)
add_dependencies(${PROJECT_NAME} libultraship)
target_link_libraries(${PROJECT_NAME} PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,libultraship>")

option(BELLWOOD_ASAN "Build the port and the game with AddressSanitizer" OFF)

include(cmake/bellwood-game.cmake)
target_sources(acgame PRIVATE ${PORT_GAME_C_FILES})
include(cmake/bellwood-audio.cmake)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
if (BELLWOOD_ASAN AND NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES ENABLE_EXPORTS TRUE)
set_target_properties(${PROJECT_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "_DEBUG;"
            "_CRT_SECURE_NO_WARNINGS;"
            "ENABLE_DX11;"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG"
        ">"
        "INCLUDE_GAME_PRINTF;"
        "UNICODE;"
        "_UNICODE;"
        "_CRT_SECURE_NO_WARNINGS;"
        "_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS;"
    )
    add_compile_definitions(STORMLIB_NO_AUTO_LINK)
	set(STORMLIB_NO_AUTO_LINK ON)
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU|Clang|AppleClang")
	target_compile_definitions(${PROJECT_NAME} PRIVATE
		"$<$<CONFIG:Debug>:"
			"_DEBUG"
		">"
		"$<$<CONFIG:Release>:"
			"NDEBUG"
		">"
		"SPDLOG_ACTIVE_LEVEL=0;"
		"_CONSOLE;"
		"_CRT_SECURE_NO_WARNINGS;"
		"UNICODE;"
		"_UNICODE"
	)
endif()

set(ADDITIONAL_LIBRARY_DEPENDENCIES)

#==============================================================================#
# Torch Integration                                                            #
#==============================================================================#

option(USE_STANDALONE "Build as a standalone executable" OFF)
option(BUILD_STORMLIB "Build with StormLib support" OFF)

option(BUILD_SM64 "Build with Super Mario 64 support" OFF)
option(BUILD_MK64 "Build with Mario Kart 64 support" OFF)
option(BUILD_SF64 "Build with Star Fox 64 support" OFF)
option(BUILD_PM64 "Build with Paper Mario support" OFF)
option(BUILD_FZERO "Build with F-Zero X support" OFF)
option(BUILD_BK64 "Build with Banjo Kazooie support" OFF)
option(BUILD_MARIO_ARTIST "Build with Mario Artist support" OFF)
option(BUILD_OOT "Build with Ocarina of Time support" OFF)
option(BUILD_NAUDIO "Build with NAudio support" ON)
set(BUILD_GAMECUBE ON CACHE BOOL "Build with GameCube disc input support")
set(BUILD_AC ON CACHE BOOL "Build with Animal Crossing support")
add_subdirectory(Torch)
list(APPEND ADDITIONAL_LIBRARY_DEPENDENCIES torch)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/Torch/CMakeLists.txt" TORCH_HAS_GAMECUBE_INPUT REGEX "GAMECUBE_SUPPORT")
if(BUILD_GAMECUBE AND TORCH_HAS_GAMECUBE_INPUT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TORCH_GAMECUBE_SUPPORT=1)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")
################################################################################
# Compile and link options
################################################################################
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /w;
            /Od
        >
        $<$<CONFIG:Release>:
            /Oi;
            /Gy;
            /W3
        >

        /wd4024 #different types for formal and actual parameter [x]. Usually seen along side 4047.
        /wd4047 #'uintptr_t' differs in levels of indirection from 'Gfx *'. Usually caused by passing a uintptr_t into a function expecting a pointer-types
        /wd4090 #'initializing': different 'const' qualifiers. Caused when initializing a non const struct member with a const char*.
        /wd4101 # unreferenced local variable. Things like stack pads.
        /wd4133 #incompatible types. Usually caused by passing a c string into a function that takes a pointer.
        /wd4244 #conversion from [a] to [b]. Used to warn when copying to smaller type without a cast.
        $<$<COMPILE_LANGUAGE:C>:/J>
        /sdl-;
        /permissive-;
        /MP;
        ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
        ${DEFAULT_CXX_EXCEPTION_HANDLING}
    )
  target_compile_options(${PROJECT_NAME} PRIVATE  $<$<CONFIG:Debug>:/ZI;>)
  target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /INCREMENTAL
        >
        $<$<CONFIG:Release>:
            /OPT:REF;
            /OPT:ICF;
            /INCREMENTAL:NO;
            /FORCE:MULTIPLE
        >
        /MANIFEST:NO;
        /DEBUG;
        /SUBSYSTEM:WINDOWS
    )

  # Remove /RTC from msvc flags
  foreach (fentry
    CMAKE_C_FLAGS
    CMAKE_CXX_FLAGS
  )
    string (REGEX REPLACE "/RTC(su|[1su])" "" ${fentry} "${${fentry}}")
  endforeach(fentry)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_compile_options(${PROJECT_NAME} PRIVATE
            -Wall -Wextra -Wno-error
            -Wno-return-type
            -Wno-unused-parameter
            -Wno-unused-function
            -Wno-unused-variable
            -Wno-missing-field-initializers
            -Wno-parentheses
            -Wno-narrowing
            -Wno-missing-braces
            -Wno-int-conversion
            $<$<COMPILE_LANGUAGE:C>:
                -Werror-implicit-function-declaration
                -Wno-incompatible-pointer-types
                -fno-strict-aliasing
                -fwrapv
                -funsigned-char
            >
            $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
            $<$<COMPILE_LANGUAGE:CXX>:
                -Wno-c++11-narrowing
                -Wno-deprecated-enum-enum-conversion
            >
            -pthread
        )

        target_link_options(${PROJECT_NAME} PRIVATE
            -pthread
        )
    else()
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        set(CPU_OPTION -msse2 -mfpmath=sse)
      elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(CPU_OPTION -mcpu=native)
      endif()

      target_compile_options(${PROJECT_NAME} PRIVATE
          -Wall -Wextra -Wno-error
          -Wno-unused-parameter
          -Wno-unused-function
          -Wno-unused-variable
          -Wno-missing-field-initializers
          -Wno-parentheses
          -Wno-narrowing
          -Wno-missing-braces
          -Wno-int-conversion
          $<$<COMPILE_LANGUAGE:C>:
              -Werror-implicit-function-declaration
              -Wno-incompatible-pointer-types
              -fno-strict-aliasing
              -fwrapv
              -funsigned-char
          >
          $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
          $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
          -pthread
    ${CPU_OPTION}
      )

      target_link_options(${PROJECT_NAME} PRIVATE
          -pthread
          -Wl,-export-dynamic
      )
  endif()
endif()

add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
	COMMENT "Copying asset yamls..."
	COMMAND ${CMAKE_COMMAND} -DSRC_DIR=${CMAKE_SOURCE_DIR} -DDST_DIR=$<TARGET_FILE_DIR:${PROJECT_NAME}>
	        -P ${CMAKE_SOURCE_DIR}/cmake/copy-extractor-data.cmake
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying runtime DLL dependencies next to ${PROJECT_NAME}.exe..."
  )
  find_file(PORT_SHADERC_DLL NAMES shaderc_shared.dll
            HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
  if (PORT_SHADERC_DLL)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${PORT_SHADERC_DLL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
      COMMENT "Copying shaderc_shared.dll next to ${PROJECT_NAME}.exe..."
    )
  endif()
endif()

include(ExternalProject)
ExternalProject_Add(TorchExternal
	PREFIX TorchExternal
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/Torch
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/TorchExternal/install
        -DCMAKE_BUILD_TYPE=Release
        -DBUILD_SM64=OFF
        -DBUILD_MK64=OFF
        -DBUILD_SF64=OFF
        -DBUILD_PM64=OFF
        -DBUILD_FZERO=OFF
        -DBUILD_BK64=OFF
        -DBUILD_MARIO_ARTIST=OFF
        -DBUILD_OOT=OFF
        -DBUILD_NAUDIO=ON
        -DBUILD_GAMECUBE=${BUILD_GAMECUBE}
        -DBUILD_AC=${BUILD_AC}
    INSTALL_COMMAND ""
    BUILD_ALWAYS TRUE
    EXCLUDE_FROM_ALL TRUE
)
ExternalProject_Get_Property(TorchExternal binary_dir)
get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (_is_multi_config)
  set(TORCH_EXECUTABLE ${binary_dir}/$<CONFIG>/torch)
else()
  set(TORCH_EXECUTABLE ${binary_dir}/torch)
endif()

set(PORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

set(AC_DISC_PATH "" CACHE FILEPATH "GameCube disc image (.iso/.gcm/.ciso/.gcz) or extracted disc folder used by ExtractAssets")
if (AC_DISC_PATH)
  add_custom_target(
      ExtractAssets
      DEPENDS TorchExternal
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMAND ${TORCH_EXECUTABLE} o2r ${AC_DISC_PATH} -u ${PORT_VERSION_STRING}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/${GAME_ARCHIVE}" "${CMAKE_BINARY_DIR}/${GAME_ARCHIVE}"
      VERBATIM
  )
else()
  add_custom_target(
      ExtractAssets
      COMMAND ${CMAKE_COMMAND} -E echo "ExtractAssets needs -DAC_DISC_PATH=/path/to/disc (image or extracted folder)"
      COMMAND ${CMAKE_COMMAND} -E false
      VERBATIM
  )
endif()

set(PORT_O2R_STAGING ${CMAKE_BINARY_DIR}/port-o2r)
file(GLOB_RECURSE PORT_O2R_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/port/*"
    "${CMAKE_SOURCE_DIR}/libultraship/src/fast/shaders/*"
)
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/${PORT_ARCHIVE}"
    DEPENDS TorchExternal ${PORT_O2R_INPUTS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${PORT_O2R_STAGING}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/libultraship/src/fast/shaders" "${PORT_O2R_STAGING}/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/port" "${PORT_O2R_STAGING}"
    COMMAND ${TORCH_EXECUTABLE} pack "${PORT_O2R_STAGING}" "${CMAKE_BINARY_DIR}/${PORT_ARCHIVE}" o2r -u ${PORT_VERSION_STRING}
    VERBATIM
)
add_custom_target(GeneratePortO2R DEPENDS "${CMAKE_BINARY_DIR}/${PORT_ARCHIVE}")
add_dependencies(${PROJECT_NAME} GeneratePortO2R)

find_program(CURL NAMES curl DOC "Path to the curl program.  Used to download files.")
execute_process(COMMAND ${CURL} -sSfL https://raw.githubusercontent.com/gabomdq/SDL_GameControllerDB/master/gamecontrollerdb.txt -o ${CMAKE_BINARY_DIR}/gamecontrollerdb.txt OUTPUT_VARIABLE RESULT)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  install(FILES "${CMAKE_BINARY_DIR}/${PORT_ARCHIVE}" DESTINATION . COMPONENT ${PROJECT_NAME})
  install(DIRECTORY "${CMAKE_BINARY_DIR}/assets/" DESTINATION ./assets COMPONENT ${PROJECT_NAME} OPTIONAL)
  install(FILES "${CMAKE_BINARY_DIR}/config.yml" DESTINATION . COMPONENT ${PROJECT_NAME} OPTIONAL)
endif()

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION ./debug COMPONENT ${PROJECT_NAME})
  install(FILES "${CMAKE_BINARY_DIR}/${PORT_ARCHIVE}" DESTINATION . COMPONENT ${PROJECT_NAME})
  install(FILES "$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>" DESTINATION . COMPONENT ${PROJECT_NAME})
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
add_custom_target(CreateOSXIcons
    COMMAND mkdir -p ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset
    COMMAND sips -z 16 16     logo.png --out ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset/icon_16x16.png
    COMMAND sips -z 32 32     logo.png --out ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset/icon_16x16@2x.png
    COMMAND sips -z 32 32     logo.png --out ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset/icon_32x32.png
    COMMAND sips -z 64 64     logo.png --out ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset/icon_32x32@2x.png
    COMMAND sips -z 128 128   logo.png --out ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset/icon_128x128.png
    COMMAND sips -z 256 256   logo.png --out ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset/icon_128x128@2x.png
    COMMAND sips -z 256 256   logo.png --out ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset/icon_256x256.png
    COMMAND sips -z 512 512   logo.png --out ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset/icon_256x256@2x.png
    COMMAND sips -z 512 512   logo.png --out ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset/icon_512x512.png
    COMMAND cp                logo.png ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset/icon_512x512@2x.png
    COMMAND iconutil -c icns -o ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.icns ${CMAKE_BINARY_DIR}/macosx/${PORT_NAME}.iconset
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Creating OSX icons ..."
    )
    add_dependencies(${PROJECT_NAME} CreateOSXIcons)
    configure_file("${CMAKE_SOURCE_DIR}/Info.plist" "${CMAKE_BINARY_DIR}/Info.plist" @ONLY)
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ../MacOS COMPONENT ${PROJECT_NAME})
INSTALL(FILES ${CMAKE_BINARY_DIR}/gamecontrollerdb.txt DESTINATION ../MacOS COMPONENT ${PROJECT_NAME})
INSTALL(FILES ${CMAKE_BINARY_DIR}/${PORT_ARCHIVE} DESTINATION ../Resources COMPONENT ${PROJECT_NAME})
INSTALL(FILES ${CMAKE_BINARY_DIR}/config.yml DESTINATION ../Resources COMPONENT ${PROJECT_NAME} OPTIONAL)
INSTALL(DIRECTORY ${CMAKE_BINARY_DIR}/assets/ DESTINATION ../Resources/assets COMPONENT ${PROJECT_NAME} OPTIONAL)

# Fix bundle to include and relink all dependencies
install(CODE "
    include(BundleUtilities)
    fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/../MacOS/${PORT_NAME}\" \"\" \"${dirs}\")
")
# Homebrew's sdl2 is sdl2-compat, a shim that dlopens the real SDL3 at runtime;
# fixup_bundle cannot follow a dlopen, so the app aborts at launch with
# "Failed loading SDL3 library." unless SDL3 sits next to the shim in Frameworks
# (sdl2-compat checks @loader_path). No-op when building against real SDL2.
find_program(BREW_EXECUTABLE brew)
if (BREW_EXECUTABLE)
    execute_process(COMMAND ${BREW_EXECUTABLE} --prefix sdl3
                    OUTPUT_VARIABLE SDL3_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET)
    if (SDL3_PREFIX AND EXISTS "${SDL3_PREFIX}/lib/libSDL3.0.dylib")
        install(PROGRAMS "${SDL3_PREFIX}/lib/libSDL3.0.dylib"
                DESTINATION ../Frameworks RENAME libSDL3.dylib COMPONENT ${PROJECT_NAME})
    endif()
endif()
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY APPIMAGE_DESKTOP_FILE_TERMINAL YES)
set_property(TARGET ${PROJECT_NAME} PROPERTY APPIMAGE_DESKTOP_FILE "${CMAKE_SOURCE_DIR}/${PROJECT_NAME}.desktop")
set_property(TARGET ${PROJECT_NAME} PROPERTY APPIMAGE_ICON_FILE "${CMAKE_SOURCE_DIR}/logo.png")

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CPACK_GENERATOR "External")
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(CPACK_GENERATOR "ZIP")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CPACK_GENERATOR "Bundle")
endif()

set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_BINARY_DIR}/configure-packaging.cmake)
configure_file(${CMAKE_SOURCE_DIR}/cmake/configure-packaging.cmake ${CPACK_PROJECT_CONFIG_FILE} @ONLY)
include(cmake/packaging.cmake)
