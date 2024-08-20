/**
 * @file       GeniusSDK.h
 * @brief      New GeniusSDK node
 * @date       2024-05-25
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */
#ifndef _GENIUSSDK_H
#define _GENIUSSDK_H

#include <memory>

#ifndef GNUS_EXPORT_BEGIN
    #if defined(__cplusplus)
        #define GNUS_EXPORT_BEGIN extern "C" {
        #define GNUS_EXPORT_END }
    #endif
#endif

#ifdef _WIN32
#define GNUS_VISIBILITY_DEFAULT __declspec( dllexport )
#else
#define GNUS_VISIBILITY_DEFAULT __attribute__( ( visibility( "default" ) ) )
#endif

GNUS_EXPORT_BEGIN

    typedef char     ImagePath_t[1024]; ///< ID/Path of the image to be processed
    typedef uint64_t PayAmount_t;       ///< Amount to be paid for the processing

GNUS_VISIBILITY_DEFAULT  const char *GeniusSDKInit( const char *base_path );
GNUS_VISIBILITY_DEFAULT  void        GeniusSDKProcess( const ImagePath_t path, const PayAmount_t amount );

GNUS_EXPORT_END

#endif //GENIUSSDK_H
