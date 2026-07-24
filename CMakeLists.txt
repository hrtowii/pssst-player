cmake_minimum_required(VERSION 3.16)

project(pssst-player C CXX ASM)

file(GLOB PROJECT_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/lib
)

target_link_libraries(${PROJECT_NAME}
    pspgu
    pspgum
    pspaudio
    pspctrl
    pspdisplay
    pspge
    pspdebug
    m
    # me-core            # uncomment once you wire in the Media Engine library
)

create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE "pssst-player"
)
