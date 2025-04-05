# 设置编译器路径
if(USE_GCC)
  set(CMAKE_C_COMPILER gcc)
  set(CMAKE_CXX_COMPILER g++)
elseif(USE_CLANG)
  set(CMAKE_C_COMPILER clang)
  set(CMAKE_CXX_COMPILER clang++)
elseif(USE_VS2019)
  set(CMAKE_C_COMPILER ${VS_PATH}/cl.exe)
  set(CMAKE_CXX_COMPILER ${VS_PATH}/cl.exe)
endif()

# -Wall:开启所有警告 -fmessage-length=0:禁止警告信息换行
# -ffunction-sections&&-fdata-sections:函数和数据放在单独的节(section)上
# -fno-exceptions:g++专用编译选项,不会对异常进行处理,直接打印错误信息 -mwindows:隐藏控制台 set(COMMON_FLAG
# "-fmessage-length=0 -ffunction-sections -fdata-sections -fno-exceptions")
if(USE_GCC)
  set(CMAKE_C_FLAGS "-std=c99 -fmessage-length=0 -ffunction-sections -fdata-sections")
  set(CMAKE_C_FLAGS_RELEASE "-Os -mwindows")
  set(CMAKE_C_FLAGS_DEBUG "-O0 -g -Wall")
  set(CMAKE_C_FLAGS_MINSIZEREL "-Os -mwindows")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-Os -g")

  set(CMAKE_CXX_FLAGS "-std=c++17 -fmessage-length=0 -ffunction-sections -fdata-sections")
  set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
  set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
  set(CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL})
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO})
elseif(USE_CLANG)
  set(CMAKE_C_FLAGS "-std=c99 ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "-std=c++17 ${CMAKE_CXX_FLAGS}")
elseif(USE_VS2019)
  set(CMAKE_C_FLAGS "/DWIN32 /D_WINDOWS /W3 /utf-8 /std:c11")
  set(CMAKE_C_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG")
  set(CMAKE_C_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /RTC1")
  set(CMAKE_C_FLAGS_MINSIZEREL "/MD /O1 /Ob1 /DNDEBUG")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MD /Zi /O2 /Ob1 /DNDEBUG")

  set(CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /W3 /GR /EHsc /utf-8 /std:c++17")
  set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
  set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
  set(CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL})
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO})
endif()

message(STATUS "**${CMAKE_BUILD_TYPE}**")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
  message(STATUS "**C:${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELEASE}**\n-- **C++:${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}**")

elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  message(STATUS "**C:${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}**\n-- **C++:${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}**")

elseif("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
  message(STATUS "**C:${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELWITHDEBINFO}**\n-- **C++:${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}**")

elseif("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
  message(STATUS "**C:${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_MINSIZEREL}**\n-- **C++:${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_MINSIZEREL}**")
endif()
