/**
 * @file       GeniusSDK.h
 * @brief      New GeniusSDK node
 * @date       2024-05-25
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */
#ifndef _GENIUSSDK_H
#define _GENIUSSDK_H

#include <memory>
#include "account/GeniusNode.hpp"

#ifndef GNUS_EXPORT
#define GNUS_EXPORT extern "C"
#endif

#ifdef _WIN32
#define GNUS_VISIBILITY_DEFAULT __declspec( dllexport )
#else
#define GNUS_VISIBILITY_DEFAULT __attribute__( ( visibility( "default" ) ) )
#endif

#ifndef __cplusplus
extern "C"
{
#endif

    typedef char     ImagePath_t[1024]; ///< ID/Path of the image to be processed
    typedef uint64_t PayAmount_t;       ///< Amount to be paid for the processing

#ifndef __cplusplus
}
#endif

GNUS_VISIBILITY_DEFAULT GNUS_EXPORT void GeniusSDKInit();
GNUS_VISIBILITY_DEFAULT GNUS_EXPORT void GeniusSDKProcess( const ImagePath_t path, const PayAmount_t amount );

#endif //GENIUSSDK_H
