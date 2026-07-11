cmake_minimum_required(VERSION 3.20)
project(flappyship)

include(FetchContent)

set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        6.0
    GIT_SHALLOW    TRUE
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(HARDCODER_SCRIPT "${CMAKE_SOURCE_DIR}/tools/hardcoder")
set(RESOURCES_DIR    "${CMAKE_SOURCE_DIR}/resources")
set(GENERATED_DIR    "${CMAKE_BINARY_DIR}/generated")

file(MAKE_DIRECTORY ${GENERATED_DIR})

file(GLOB_RECURSE RESOURCE_FILES RELATIVE ${RESOURCES_DIR} "${RESOURCES_DIR}/*")

set(GENERATED_HEADERS "")

foreach(RES_REL_PATH ${RESOURCE_FILES})
  string(REPLACE "/" "_" RES_FLAT_NAME "${RES_REL_PATH}")
  set(OUT_HEADER "${GENERATED_DIR}/${RES_FLAT_NAME}.h")
  set(IN_FILE    "${RESOURCES_DIR}/${RES_REL_PATH}")

  get_filename_component(RES_SUBDIR "${RES_REL_PATH}" DIRECTORY)

  set(HARDCODER_ARGS c ${IN_FILE} ${OUT_HEADER})
  if(RES_SUBDIR)
    list(APPEND HARDCODER_ARGS --prefix ${RES_SUBDIR})
  endif()

  add_custom_command(
    OUTPUT  ${OUT_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${HARDCODER_SCRIPT} ${HARDCODER_ARGS}
    DEPENDS ${IN_FILE} ${HARDCODER_SCRIPT}
    COMMENT "hardcoder: ${RES_REL_PATH} -> generated/${RES_FLAT_NAME}.h"
    VERBATIM
  )

  list(APPEND GENERATED_HEADERS ${OUT_HEADER})
endforeach()

add_custom_target(generate_resources DEPENDS ${GENERATED_HEADERS})

add_executable(${PROJECT_NAME} main.c)

add_dependencies(${PROJECT_NAME} generate_resources)

target_include_directories(${PROJECT_NAME} PRIVATE ${GENERATED_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE winmm gdi32 user32 shell32)
elseif(UNIX AND NOT APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE m pthread dl rt X11)
endif()
