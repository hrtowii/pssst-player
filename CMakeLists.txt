cmake_minimum_required(VERSION 3.16)

project(pssst-player C CXX ASM)

file(GLOB PROJECT_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# the shit i do for libraries
find_library(MAD_LIBRARY mad
    HINTS
        $ENV{LIBMAD_LIBDIR}
        ${CMAKE_SOURCE_DIR}/lib
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)
find_path(MAD_INCLUDE_DIR mad.h
    HINTS
        $ENV{LIBMAD_INCDIR}
        ${CMAKE_SOURCE_DIR}/lib
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/lib
    ${MAD_INCLUDE_DIR}
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
    ${MAD_LIBRARY}
    # me-core
)

create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE "pssst-player"
)
