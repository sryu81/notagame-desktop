cmake_minimum_required(VERSION 3.22)
project(not_a_game_desktop CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(NAG_SANITIZE "Build with ASan + UBSan" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
pkg_check_modules(GLESV2 REQUIRED IMPORTED_TARGET glesv2)
find_package(Threads REQUIRED)

file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/engine/*.cpp)
add_library(not-a-game-engine STATIC ${ENGINE_SOURCES})
target_include_directories(not-a-game-engine PUBLIC ${CMAKE_SOURCE_DIR}/src/engine)
target_include_directories(not-a-game-engine SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(not-a-game-engine PUBLIC PkgConfig::GLESV2 Threads::Threads ${CMAKE_DL_LIBS} m)

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/game/*.cpp)
file(GLOB DESKTOP_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/desktop/*.cpp)
add_executable(not_a_game ${DESKTOP_SOURCES} ${GAME_SOURCES})
target_include_directories(not_a_game PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/game)
target_link_libraries(not_a_game PRIVATE not-a-game-engine PkgConfig::GLFW3)

foreach(t not-a-game-engine not_a_game)
    target_compile_options(${t} PRIVATE -fno-omit-frame-pointer -UNDEBUG -Wall -Wno-unused-variable)
    if(NAG_SANITIZE)
        target_compile_options(${t} PRIVATE -fsanitize=address,undefined)
        target_link_options(${t} PRIVATE -fsanitize=address,undefined)
    endif()
endforeach()

add_custom_command(TARGET not_a_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:not_a_game>/assets)
