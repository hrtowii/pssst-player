cmake_minimum_required(VERSION 3.16)

project(pssst-player C CXX ASM)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
target_compile_options(${PROJECT_NAME} PRIVATE
    -O3 -march=allegrex -mtune=allegrex -fomit-frame-pointer -G0
)
set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON")

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

find_library(OGG_LIBRARY ogg
    HINTS
        $ENV{LIBOGG_LIBDIR}
        ${CMAKE_SOURCE_DIR}/lib
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

find_path(OGG_INCLUDE_DIR ogg/ogg.h
    HINTS
        $ENV{LIBOGG_INCDIR}
        ${CMAKE_SOURCE_DIR}/lib
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

find_library(FLAC_LIBRARY FLAC
    HINTS
        $ENV{LIBFLAC_LIBDIR}
        ${CMAKE_SOURCE_DIR}/lib
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

find_path(FLAC_INCLUDE_DIR FLAC/stream_decoder.h
    HINTS
        $ENV{LIBFLAC_INCDIR}
        ${CMAKE_SOURCE_DIR}/lib
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

find_library(OPUS_LIBRARY opus
    HINTS
        $ENV{LIBOPUS_LIBDIR}
        ${CMAKE_SOURCE_DIR}/lib
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

find_path(OPUS_INCLUDE_DIR opus/opus.h
    HINTS
        $ENV{LIBOPUS_INCDIR}
        ${CMAKE_SOURCE_DIR}/lib
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

find_library(OPUSFILE_LIBRARY opusfile
    HINTS
        $ENV{LIBOPUSFILE_LIBDIR}
        ${CMAKE_SOURCE_DIR}/lib
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

find_path(OPUSFILE_INCLUDE_DIR opus/opusfile.h
    HINTS
        $ENV{LIBOPUSFILE_INCDIR}
        ${CMAKE_SOURCE_DIR}/lib
    NO_CMAKE_FIND_ROOT_PATH
    REQUIRED
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/lib
    ${MAD_INCLUDE_DIR}
    ${OGG_INCLUDE_DIR}
    ${FLAC_INCLUDE_DIR}
    ${OPUS_INCLUDE_DIR}
    ${OPUS_INCLUDE_DIR}/opus
    ${OPUSFILE_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    pspgu
    pspgum
    pspaudio
    pspctrl
    pspdisplay
    pspge
    pspdebug
    # psppower
    m
    ${MAD_LIBRARY}
    ${FLAC_LIBRARY}
    ${OPUS_LIBRARY}
    ${OPUSFILE_LIBRARY}
    ${OGG_LIBRARY}
    # me-core
)

create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE "pssst-player"
)
