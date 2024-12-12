/**
 * @file       GeniusSDK.h
 * @brief      New GeniusSDK node
 * @date       2024-05-25
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */
#ifndef _GENIUSSDK_H
#define _GENIUSSDK_H

#include <stdint.h>
#include <stdbool.h>

#ifndef GNUS_EXPORT_BEGIN
#if defined( __cplusplus )
#define GNUS_EXPORT_BEGIN                                                                                              \
    extern "C"                                                                                                         \
    {
#define GNUS_EXPORT_END }
#else
#define GNUS_EXPORT_BEGIN
#define GNUS_EXPORT_END
#endif
#endif

#ifdef _WIN32
#define GNUS_VISIBILITY_DEFAULT __declspec( dllexport )
#else
#define GNUS_VISIBILITY_DEFAULT __attribute__( ( visibility( "default" ) ) )
#endif

GNUS_EXPORT_BEGIN

typedef struct
{
    uint64_t size;
    uint8_t *ptr;
} GeniusArray; ///< Struct to interop C++ vectors with C

typedef struct
{
    uint64_t     size;
    GeniusArray *ptr;
} GeniusMatrix; ///< Struct to interop a matrix of C++ vectors in C

typedef struct
{
    /// A string prepended with `0x` followed by 64 hex characters,
    /// including a null-terminating char just for safety.
    char address[2 + 256 / 4 + 1];
} GeniusAddress;

typedef char     JsonData_t[1024]; ///< ID/Path of the image to be processed
typedef uint64_t PayAmount_t;      ///< Amount to be paid for the processing

GNUS_VISIBILITY_DEFAULT const char  *GeniusSDKInit( const char *base_path, const char *eth_private_key, bool autodht,
                                                    bool process );
GNUS_VISIBILITY_DEFAULT void         GeniusSDKProcess( const JsonData_t jsondata );
GNUS_VISIBILITY_DEFAULT uint64_t     GeniusSDKGetBalance();
GNUS_VISIBILITY_DEFAULT GeniusMatrix GeniusSDKGetOutTransactions();
GNUS_VISIBILITY_DEFAULT GeniusMatrix GeniusSDKGetInTransactions();
GNUS_VISIBILITY_DEFAULT void         GeniusSDKFreeTransactions( GeniusMatrix matrix );
GNUS_VISIBILITY_DEFAULT void GeniusSDKMintTokens( uint64_t amount, const char *transaction_hash, const char *chain_id,
                                                  const char *token_id );
GNUS_VISIBILITY_DEFAULT GeniusAddress GeniusSDKGetAddress();
GNUS_VISIBILITY_DEFAULT bool          GeniusSDKTransferTokens( uint64_t amount, GeniusAddress *dest );
GNUS_VISIBILITY_DEFAULT uint64_t      GeniusSDKGetCost( const JsonData_t jsondata );

GNUS_EXPORT_END

#endif
