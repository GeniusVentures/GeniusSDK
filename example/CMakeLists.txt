add_executable(SDKIdleExample
    SDKIdleExample.cpp
)
target_link_libraries(SDKIdleExample PRIVATE GeniusSDK)

# Apply whole-archive selectively for GeniusSDK based on platform
if(APPLE)
    target_link_options(SDKIdleExample PRIVATE -force_load "$<TARGET_FILE:GeniusSDK>")
elseif(WIN32 AND NOT SHARED_LIB_BUILD)
    target_link_options(SDKIdleExample PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:GeniusSDK>")
elseif(UNIX)
    target_link_options(SDKIdleExample PRIVATE "-Wl,--whole-archive" "$<TARGET_FILE:GeniusSDK>" "-Wl,--no-whole-archive")
endif()

add_executable(SDKExample
    SDKExample.cpp
)
target_link_libraries(SDKExample PRIVATE GeniusSDK)

# Apply whole-archive selectively for GeniusSDK based on platform
if(APPLE)
    target_link_options(SDKExample PRIVATE -force_load "$<TARGET_FILE:GeniusSDK>")
elseif(WIN32 AND NOT SHARED_LIB_BUILD)
    target_link_options(SDKExample PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:GeniusSDK>")
elseif(UNIX)
    target_link_options(SDKExample PRIVATE "-Wl,--whole-archive" "$<TARGET_FILE:GeniusSDK>" "-Wl,--no-whole-archive")
endif()

