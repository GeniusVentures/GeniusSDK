add_executable(GeniusSDKService service.cpp)
if(WIN32)
    target_sources(GeniusSDKService PRIVATE ServiceLifetimeWindows.cpp)
else()
    target_sources(GeniusSDKService PRIVATE ServiceLifetimePosix.cpp)
endif()
TARGET_LINK_LIBRARIES_WHOLE_ARCHIVE(GeniusSDKService GeniusSDK)
geniussdk_install(GeniusSDKService)

install(FILES
    genius-archive-node.service
    genius-full-node.service
    genius-job-poster.service
    DESTINATION ${CMAKE_INSTALL_BINDIR}/services
)
