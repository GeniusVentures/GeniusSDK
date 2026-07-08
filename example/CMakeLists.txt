add_executable(SDKIdleExample
    SDKIdleExample.cpp
)
TARGET_LINK_LIBRARIES_WHOLE_ARCHIVE(SDKIdleExample GeniusSDK)

add_executable(SDKExample
    SDKExample.cpp
)
TARGET_LINK_LIBRARIES_WHOLE_ARCHIVE(SDKExample GeniusSDK)

add_custom_command(TARGET SDKExample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/dev_config.json"
        "$<TARGET_FILE_DIR:SDKExample>/dev_config.json"
        COMMENT "Copying dev_config.json data"
)
add_custom_command(TARGET SDKExample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/network_config.json"
        "$<TARGET_FILE_DIR:SDKExample>/network_config.json"
        COMMENT "Copying network_config.json data"
)
add_custom_command(TARGET SDKExample POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/sgns_config.json"
        "$<TARGET_FILE_DIR:SDKExample>/sgns_config.json"
        COMMENT "Copying sgns_config.json data"
)
