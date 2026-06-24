add_executable(GeniusSDKService service.cpp)
TARGET_LINK_LIBRARIES_WHOLE_ARCHIVE(GeniusSDKService GeniusSDK)
geniussdk_install(GeniusSDKService)

install(FILES
    genius-archive-node.service
    genius-full-node.service
    genius-job-poster.service
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
