file(GLOB ALL_HDRS
    "./*.h"
    "./*.hpp"
    )

file(GLOB ALL_SRCS
    "./*.cpp"
    "./*.c"
    )

add_library(GeniusSDK STATIC ${ALL_SRCS} ${ALL_HDRS})
GENERATE_EXPORT_HEADER(GeniusSDK EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/GeniusSDKExport.h)
set_target_properties(GeniusSDK PROPERTIES PUBLIC_HEADER "${ALL_HDRS};${CMAKE_BINARY_DIR}/GeniusSDKExport.h")
target_link_libraries(GeniusSDK PUBLIC
        sgns::sgns_account
        )
geniussdk_install(GeniusSDK)
