file(GLOB ALL_HDRS
    "./*.h"
    "./*.hpp"
    )

file(GLOB ALL_SRCS
    "./*.cpp"
    "./*.c"
    )

add_library(GeniusSDK SHARED ${ALL_SRCS} ${ALL_HDRS})
GENERATE_EXPORT_HEADER(GeniusSDK EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/GeniusSDKExport.h)
set_target_properties(GeniusSDK PROPERTIES PUBLIC_HEADER "${ALL_HDRS};${CMAKE_BINARY_DIR}/GeniusSDKExport.h")
target_link_libraries(GeniusSDK
        sgns::crdt_datastore
        sgns::sr25519_provider
        sgns::bip39_provider
        sgns::pbkdf2_provider
        )
geniussdk_install(GeniusSDK)
