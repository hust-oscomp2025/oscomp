project(lwext4 C)
cmake_minimum_required(VERSION 3.4)


include_directories(include)
include_directories(${PROJECT_BINARY_DIR}/include)




include_directories(blockdev/filedev)
include_directories(blockdev/filedev_win)
# 我们需要把这个blockdev换成riscv的

set(BLOCKDEV_TYPE  none)

add_definitions(-DCONFIG_USE_DEFAULT_CONFIG=0)
add_definitions(-DVERSION="${VERSION}")
# ==============================================================================================

add_subdirectory(blockdev)

#Library build
add_subdirectory(src)
#Detect all possible warnings for lwext4 target
if (NOT CMAKE_COMPILER_IS_GNUCC)
    set_target_properties(lwext4 PROPERTIES COMPILE_FLAGS "")
else()
    set_target_properties(lwext4 PROPERTIES COMPILE_FLAGS "-Wall -Wextra -pedantic")
endif()

#DISTRIBUTION
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES
"/build" ".git")
include(CPack)


add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)
