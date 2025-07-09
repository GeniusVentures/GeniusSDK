file(GLOB ALL_HDRS
    "./*.h"
    "./*.hpp"
    )

file(GLOB ALL_SRCS
    "./*.cpp"
    "./*.c"
    )

add_library(GeniusSDK STATIC ${ALL_SRCS} ${ALL_HDRS})
set_target_properties(GeniusSDK PROPERTIES
    OUTPUT_NAME GeniusSDK
    PUBLIC_HEADER "${ALL_HDRS}"
)
target_link_libraries(GeniusSDK PUBLIC sgns::sgns_account)

add_library(GeniusSDK_shared SHARED ${ALL_SRCS} ${ALL_HDRS})
GENERATE_EXPORT_HEADER(GeniusSDK_shared EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/GeniusSDKExport.h)
set_target_properties(GeniusSDK_shared PROPERTIES
    OUTPUT_NAME GeniusSDK  
    PUBLIC_HEADER "${ALL_HDRS};${CMAKE_BINARY_DIR}/GeniusSDKExport.h"
)
target_link_libraries(GeniusSDK_shared PUBLIC sgns::sgns_account)

geniussdk_install(GeniusSDK)        
geniussdk_install(GeniusSDK_shared)  