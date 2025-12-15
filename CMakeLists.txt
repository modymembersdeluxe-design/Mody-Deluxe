cmake_minimum_required(VERSION 3.10)
project(ModyPlusDeluxe VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
)
FetchContent_MakeAvailable(json)

add_executable(modyplus_deluxe
  src/main.cpp
  src/FFmpegCommandBuilder.cpp
  src/RemixRuleEngine.cpp
  src/PreviewPlayer.cpp
  src/MediaManager.cpp
  src/Utils.cpp
)

target_include_directories(modyplus_deluxe PRIVATE ${json_SOURCE_DIR})

if (MSVC)
  target_compile_definitions(modyplus_deluxe PRIVATE NOMINMAX)
endif()