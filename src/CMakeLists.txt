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
target_link_libraries(GeniusSDK PUBLIC sgns::genius_node)
geniussdk_install(GeniusSDK)

add_library(GeniusSDK_shared SHARED ${ALL_SRCS} ${ALL_HDRS})
GENERATE_EXPORT_HEADER(GeniusSDK_shared EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/GeniusSDKExport.h)


set_target_properties(GeniusSDK_shared PROPERTIES
    OUTPUT_NAME GeniusSDK_shared  
    PUBLIC_HEADER "${ALL_HDRS};${CMAKE_BINARY_DIR}/GeniusSDKExport.h"
)


target_link_libraries(GeniusSDK_shared PUBLIC sgns::genius_node)
if(SGNS_ENABLE_RELEASE_SYMBOLS)
    if(MSVC)
        # Shared library should emit standalone PDBs for symbolication/debugging.
        target_compile_options(GeniusSDK_shared PRIVATE
            "$<$<CONFIG:Release>:/Zi>"
            "$<$<CONFIG:RelWithDebInfo>:/Zi>"
        )
        target_link_options(GeniusSDK_shared PRIVATE
            "$<$<CONFIG:Release>:/DEBUG:FULL>"
            "$<$<CONFIG:RelWithDebInfo>:/DEBUG:FULL>"
        )
        install(FILES $<TARGET_PDB_FILE:GeniusSDK_shared>
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            OPTIONAL
        )
    endif()

    if(APPLE)
        find_program(DSYMUTIL_EXECUTABLE dsymutil REQUIRED)
        add_custom_command(TARGET GeniusSDK_shared POST_BUILD
        COMMAND ${DSYMUTIL_EXECUTABLE} $<TARGET_FILE:GeniusSDK_shared> -o $<TARGET_FILE:GeniusSDK_shared>.dSYM
        COMMENT "Generating dSYM for GeniusSDK_shared"
        )
        install(DIRECTORY $<TARGET_FILE:GeniusSDK_shared>.dSYM
            DESTINATION ${CMAKE_INSTALL_LIBDIR}
            OPTIONAL
        )
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_OBJCOPY)
            add_custom_command(TARGET GeniusSDK_shared POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:GeniusSDK_shared> $<TARGET_FILE:GeniusSDK_shared>.debug
            COMMENT "Generating debug symbols for GeniusSDK_shared"
            )
            install(FILES $<TARGET_FILE:GeniusSDK_shared>.debug
                DESTINATION ${CMAKE_INSTALL_LIBDIR}
                OPTIONAL
            )
        endif()
    endif()
endif()
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
        target_link_libraries(GeniusSDK_framework PUBLIC sgns::genius_node "-framework Security")
        find_program(DSYMUTIL_EXECUTABLE dsymutil REQUIRED)
        add_custom_command(TARGET GeniusSDK_framework POST_BUILD
        COMMAND ${DSYMUTIL_EXECUTABLE} $<TARGET_FILE:GeniusSDK_framework> -o $<TARGET_FILE:GeniusSDK_framework>.dSYM
        COMMENT "Generating dSYM for GeniusSDK_framework"
        )
        geniussdk_install(GeniusSDK_framework)
        if(TARGET GeniusSDK_shared)
            target_link_libraries(GeniusSDK_shared
            PUBLIC
            "-framework Security"
            )
        endif()        
    else()
        # macOS Bundle
        add_library(GeniusSDK_bundle MODULE ${ALL_SRCS} ${ALL_HDRS})
        set_target_properties(GeniusSDK_bundle PROPERTIES
            BUNDLE TRUE
            OUTPUT_NAME GeniusSDK
            INSTALL_NAME_DIR "@rpath"
            PUBLIC_HEADER "${ALL_HDRS};${CMAKE_BINARY_DIR}/GeniusSDKExport.h"
        )
        target_link_libraries(GeniusSDK_bundle PUBLIC sgns::genius_node)
        find_program(DSYMUTIL_EXECUTABLE dsymutil REQUIRED)
        add_custom_command(TARGET GeniusSDK_bundle POST_BUILD
        COMMAND ${DSYMUTIL_EXECUTABLE} $<TARGET_FILE:GeniusSDK_bundle> -o $<TARGET_FILE:GeniusSDK_bundle>.dSYM
        COMMENT "Generating dSYM for GeniusSDK_bundle"
        )
        geniussdk_install(GeniusSDK_bundle)
    endif()
endif()