cmake_minimum_required(VERSION 3.5)
#set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/../esp-idf-lib/components)
set(PROJECT_VER "0.1")
include(FetchContent)
FetchContent_Declare(
  espidflib
  GIT_REPOSITORY https://github.com/UncleRus/esp-idf-lib.git
)
FetchContent_MakeAvailable(espidflib)
set(EXTRA_COMPONENT_DIRS ${espidflib_SOURCE_DIR}/components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-demo)
