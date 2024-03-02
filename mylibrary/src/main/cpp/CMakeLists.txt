cmake_minimum_required(VERSION 3.4.1)

set(FILE_DIR entry.cpp utils.h)

include_directories("${CMAKE_SOURCE_DIR}")
#aux_source_directory(${CMAKE_SOURCE_DIR} SRC_LIST)


add_library( # Specifies the name of the library.
        memoryescape

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        entry.cpp
        utils.h
        ${FILE_DIR}
        )
add_library(include SHARED IMPORTED)


find_library(
        log-lib
        log
)

find_package(shadowhook REQUIRED CONFIG)

target_link_libraries(memoryescape shadowhook::shadowhook)




#
target_link_libraries(
        memoryescape
        ${log-lib}
)


include_directories(

)