if(NOT DEFINED DEPLOYMENT_TARGET)
    set(DEPLOYMENT_TARGET "16.3")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/ios.toolchain.cmake")
