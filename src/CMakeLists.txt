include_directories(
        ../include
)

file(GLOB ALL_HDRS
    "../include/*.h"
    "../include/*.hpp"
    )

file(GLOB ALL_SRCS
    "../src/*.cpp"
    "../src/*.c"
    )

add_library(GeniusSDK SHARED ${ALL_SRCS} ${ALL_HDRS})
set_target_properties(GeniusSDK PROPERTIES PUBLIC_HEADER "${ALL_HDRS}")
geniussdk_install(GeniusSDK)
