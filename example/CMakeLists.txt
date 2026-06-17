add_executable(SDKIdleExample
    SDKIdleExample.cpp
)
target_link_libraries(SDKIdleExample PRIVATE GeniusSDK)

if(APPLE)
    target_link_options(SDKIdleExample PRIVATE -force_load "$<TARGET_FILE:GeniusSDK>")
elseif(WIN32)
    target_link_options(SDKIdleExample PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:GeniusSDK>")
elseif(UNIX)
    target_link_options(SDKIdleExample PRIVATE "-Wl,--whole-archive" "$<TARGET_FILE:GeniusSDK>" "-Wl,--no-whole-archive")
endif()

add_executable(SDKExample
    SDKExample.cpp
)
target_link_libraries(SDKExample PRIVATE GeniusSDK)

if(APPLE)
    target_link_options(SDKExample PRIVATE -force_load "$<TARGET_FILE:GeniusSDK>")
elseif(WIN32)
    target_link_options(SDKExample PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:GeniusSDK>")
elseif(UNIX)
    target_link_options(SDKExample PRIVATE "-Wl,--whole-archive" "$<TARGET_FILE:GeniusSDK>" "-Wl,--no-whole-archive")
endif()

add_custom_command(TARGET SDKExample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/dev_config.json"
        "$<TARGET_FILE_DIR:SDKExample>/dev_config.json"
        COMMENT "Copying dev_config.json data"
)

# add_executable(SDKExampleCredentials
#     SDKExampleCredentials.cpp
# )
# target_link_libraries(SDKExampleCredentials PRIVATE GeniusSDK)

# if(APPLE)
#     target_link_options(SDKExampleCredentials PRIVATE -force_load "$<TARGET_FILE:GeniusSDK>")
# elseif(WIN32)
#     target_link_options(SDKExampleCredentials PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:GeniusSDK>")
# elseif(UNIX)
#     target_link_options(SDKExampleCredentials PRIVATE "-Wl,--whole-archive" "$<TARGET_FILE:GeniusSDK>" "-Wl,--no-whole-archive")
# endif()
