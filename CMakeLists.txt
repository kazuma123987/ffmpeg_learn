cmake_minimum_required(VERSION 3.12)
project(
  main
  VERSION 0.0.1
  LANGUAGES C CXX)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
include_directories(
  ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/ffmpeg
  ${CMAKE_SOURCE_DIR}/include/utils)
link_directories(${CMAKE_SOURCE_DIR}/lib ${CMAKE_SOURCE_DIR}/lib/ffmpeg
                 ${CMAKE_SOURCE_DIR}/build)
set(GLAD "${CMAKE_SOURCE_DIR}/src/glad/glad.c")
set(UTIL ${CMAKE_SOURCE_DIR}/src/utils/util.c)
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/media_player/*.cpp")
file(GLOB FFPLAY_SRC "${CMAKE_SOURCE_DIR}/src/ffplay/*.c")
link_libraries(avcodec.dll avformat.dll swscale.dll avfilter.dll
               avutil.dll avdevice.dll swresample.dll)
add_library(util ${UTIL})
add_executable(media_player ${SOURCES} ${GLAD})
add_executable(ffplay ${FFPLAY_SRC})
target_link_libraries(media_player PRIVATE glfw3 SDL2.dll fmod_vc.lib
                                           util)
target_link_libraries(ffplay PRIVATE glfw3 SDL2.dll util )
