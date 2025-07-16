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
    OUTPUT_NAME GeniusSDK_shared  
    PUBLIC_HEADER "${ALL_HDRS};${CMAKE_BINARY_DIR}/GeniusSDKExport.h"
)


target_link_libraries(GeniusSDK_shared PUBLIC sgns::sgns_account)

geniussdk_install(GeniusSDK)        
geniussdk_install(GeniusSDK_shared)  

if(APPLE)
    if(PLATFORM STREQUAL "OS64")
        # iOS Framework
        add_library(GeniusSDK_framework SHARED ${ALL_SRCS} ${ALL_HDRS})
        set_target_properties(GeniusSDK_framework PROPERTIES
            FRAMEWORK TRUE
            OUTPUT_NAME GeniusSDK
            FRAMEWORK_VERSION A
            MACOSX_FRAMEWORK_IDENTIFIER ai.gnus.GeniusSDK
            INSTALL_NAME_DIR "@rpath"
            MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/../cmake/GeniusSDK_Info_ios.plist"
            PUBLIC_HEADER "${ALL_HDRS};${CMAKE_BINARY_DIR}/GeniusSDKExport.h"
        )
        target_link_libraries(GeniusSDK_framework PUBLIC sgns::sgns_account "-framework Security")
        geniussdk_install(GeniusSDK_framework)
        target_link_libraries(GeniusSDK_shared
        PUBLIC
        "-framework Security"
        )

    else()
        # macOS Bundle
        add_library(GeniusSDK_bundle MODULE ${ALL_SRCS} ${ALL_HDRS})
        set_target_properties(GeniusSDK_bundle PROPERTIES
            BUNDLE TRUE
            OUTPUT_NAME GeniusSDK
            INSTALL_NAME_DIR "@rpath"
            PUBLIC_HEADER "${ALL_HDRS};${CMAKE_BINARY_DIR}/GeniusSDKExport.h"
        )
        target_link_libraries(GeniusSDK_bundle PUBLIC sgns::sgns_account)
        geniussdk_install(GeniusSDK_bundle)
    endif()
endif()