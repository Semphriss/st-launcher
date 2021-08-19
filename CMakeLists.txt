
project(stlauncher)
cmake_minimum_required(VERSION 3.0)

file(GLOB_RECURSE LAUNCHER_SOURCE src/*.cpp)
add_executable(stlauncher ${LAUNCHER_SOURCE})

set(HARBOR_BUILD_EXEC OFF CACHE BOOL "" FORCE)
set(HARBOR_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(HARBOR_USE_SCRIPTING OFF CACHE BOOL "" FORCE)
set(HARBOR_USE_VIDEO_OPENGL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/harbor)

find_package(CURL REQUIRED)
if(VCPKG_TARGET_TRIPLET)
  set(CURL_LIBRARIES CURL:libcurl)
endif()

target_link_libraries(stlauncher PUBLIC ${CURL_LIBRARIES} harbor_lib)
target_include_directories(stlauncher PUBLIC ${CURL_INCLUDE_DIRS}
                                             external/portable-file-dialogs)
