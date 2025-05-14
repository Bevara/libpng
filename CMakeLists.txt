cmake_minimum_required(VERSION 3.7)
project(Filters)

include(filters.cmake)

add_definitions(-fpic)

SET(LIBPNG_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/img.c
        ${CMAKE_CURRENT_SOURCE_DIR}/enc_png.c
        ${CMAKE_CURRENT_SOURCE_DIR}/dec_png.c
        ${CMAKE_CURRENT_SOURCE_DIR}/reframe_png.c
        ${CMAKE_CURRENT_SOURCE_DIR}/convert.c
)

SET(LIBPNG_LIBS
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libpng16.a
)

SET(PNG_INC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_filter(libpng
        "${LIBPNG_SRC}"
        "${LIBPNG_LIBS}"
        []
        GPAC_HAS_PNG
        "${PNG_INC}"
        "-s USE_ZLIB=1 -lz"
        "1")
