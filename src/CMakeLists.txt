file(GLOB ALL_HDRS
    "./*.h"
    "./*.hpp"
    )

file(GLOB ALL_SRCS
    "./*.cpp"
    "./*.c"
    )

add_library(GeniusSDK SHARED ${ALL_SRCS} ${ALL_HDRS})
#set_property(TARGET GeniusSDK PROPERTY POSITION_INDEPENDENT_CODE ON)
GENERATE_EXPORT_HEADER(GeniusSDK EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/GeniusSDKExport.h)
set_target_properties(GeniusSDK PROPERTIES PUBLIC_HEADER "${ALL_HDRS};${CMAKE_BINARY_DIR}/GeniusSDKExport.h")
target_link_libraries(GeniusSDK
        sgns::crdt_datastore
        )
geniussdk_install(GeniusSDK)
