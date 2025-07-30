file(GLOB SOURCES "*/*.cpp")

add_library(libexchange STATIC ${SOURCES})

option(BUILD_WITH_BITMAP "Build the bitmap version of the OrderQueue" OFF)

if(BUILD_WITH_BITMAP)
    target_compile_definitions(libexchange PUBLIC USE_BITMAP_QUEUE)
endif()