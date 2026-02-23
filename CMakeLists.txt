cmake_minimum_required(VERSION 3.12)
project(
  main
  VERSION 0.0.1
  LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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
file(GLOB VIDEO_DEMO "${CMAKE_SOURCE_DIR}/src/video_demo/*.cpp")
# glfw源码
add_subdirectory(src/glfw3)
# ffmpeg库路径
set(FFMPEG_LIB_PATH "${CMAKE_SOURCE_DIR}/lib/ffmpeg")
find_library(AVCODEC_LIB libavcodec.dll.a PATHS ${FFMPEG_LIB_PATH} NO_DEFAULT_PATH)
find_library(AVFORMAT_LIB libavformat.dll.a PATHS ${FFMPEG_LIB_PATH} NO_DEFAULT_PATH)
find_library(SWSCALE_LIB libswscale.dll.a PATHS ${FFMPEG_LIB_PATH} NO_DEFAULT_PATH)
find_library(AVFILTER_LIB libavfilter.dll.a PATHS ${FFMPEG_LIB_PATH} NO_DEFAULT_PATH)
find_library(AVUTIL_LIB libavutil.dll.a PATHS ${FFMPEG_LIB_PATH} NO_DEFAULT_PATH)
find_library(AVDEVICE_LIB libavdevice.dll.a PATHS ${FFMPEG_LIB_PATH} NO_DEFAULT_PATH)
find_library(SWRESAMPLE_LIB libswresample.dll.a PATHS ${FFMPEG_LIB_PATH} NO_DEFAULT_PATH)
# 其他库路径
find_library(SDL2_LIB libSDL2.dll.a PATHS ${CMAKE_SOURCE_DIR}/lib NO_DEFAULT_PATH)
find_library(FMOD_LIB fmod_vc.lib PATHS ${CMAKE_SOURCE_DIR}/lib NO_DEFAULT_PATH)

link_libraries(${AVCODEC_LIB} ${AVFORMAT_LIB} ${SWSCALE_LIB} ${AVFILTER_LIB}
               ${AVUTIL_LIB} ${AVDEVICE_LIB} ${SWRESAMPLE_LIB})
               
add_library(util ${UTIL})
add_executable(media_player ${SOURCES} ${GLAD})
add_executable(ffplay ${FFPLAY_SRC})
add_executable(video_demo WIN32 ${VIDEO_DEMO} ${GLAD})
target_link_libraries(media_player PRIVATE glfw ${SDL2_LIB} ${FMOD_LIB} util)
target_link_libraries(ffplay PRIVATE glfw ${SDL2_LIB} util )

target_link_libraries(video_demo PRIVATE glfw ${SDL2_LIB} util d3d11 dxgi d3dcompiler)
