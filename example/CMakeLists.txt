add_executable(SDKIdleExample
    SDKIdleExample.cpp
)
target_link_libraries(SDKIdleExample PRIVATE "-Wl,--whole-archive"
    GeniusSDK "-Wl,--no-whole-archive"

)
