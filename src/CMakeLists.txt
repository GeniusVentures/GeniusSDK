add_proto_library_sdk(GeniusRPC proto/genius_rpc.proto)


file(GLOB ALL_HDRS
    "./*.h"
    "./*.hpp"
    )

file(GLOB ALL_SRCS
    "./*.cpp"
    "./*.c"
    )

add_library(GeniusSDK ${LIB_TYPE} ${ALL_SRCS} ${ALL_HDRS})
GENERATE_EXPORT_HEADER(GeniusSDK EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/GeniusSDKExport.h)
set_target_properties(GeniusSDK PROPERTIES PUBLIC_HEADER "${ALL_HDRS};${CMAKE_BINARY_DIR}/GeniusSDKExport.h")
target_link_libraries(GeniusSDK PUBLIC
        sgns::sgns_account
        GeniusRPC
        gRPC::grpc++
        
        gRPC::grpc
        )
geniussdk_install(GeniusSDK)
