/**
 * @brief      Genius SDK for managing and interacting with Genius Tokens (GNUS).
 * @date       2024-05-25
 *
 * This SDK provides functions to initialize and shut down a Genius node,
 * retrieve balances, transfer tokens, and process data. It uses two units:
 *
 * - **Minion Tokens**: The smallest sub-unit of Genius Tokens (1 Minion = 1e-9 GNUS).
 *   Functions that accept or return a `uint64_t` amount refer to Minion Tokens.
 * - **Genius Tokens (GNUS)**: The standard unit of the token system. Functions that
 *   accept or return a `GeniusTokenValue` (string format) refer to Genius Tokens.
 *
 * When working with amounts, ensure you use the correct unit. You can convert
 * between Genius Tokens and Minion Tokens using `GeniusSDKToMinions` and
 * `GeniusSDKToGenius`.
 *
 * @author     Henrique A. Klein
 * @author     Luiz Guilherme Rizzatto Zucchi
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

/**
 * @brief Represents a Genius token value in fixed-point format as a string.
 *
 * This structure provides enough space to hold the largest possible Genius token
 * string value ("18446744073.709551615") plus the null terminator.
 */
typedef struct
{
    char value[22]; ///< String representation of the Genius token value
} GeniusTokenValue;

typedef char     JsonData_t[2048]; ///< ID/Path of the image to be processed
typedef uint64_t PayAmount_t;      ///< Amount to be paid for the processing

GNUS_VISIBILITY_DEFAULT const char *GeniusSDKInit( const char *base_path, const char *eth_private_key, bool autodht,
                                                   bool process, uint16_t baseport );
GNUS_VISIBILITY_DEFAULT void        GeniusSDKShutdown();

/**
 * @brief Retrieves the current balance in **Minion Tokens** (1e-9 GNUS).
 * @return The balance amount as a `uint64_t` value representing Minion Tokens.
 */
GNUS_VISIBILITY_DEFAULT uint64_t GeniusSDKGetBalance();

/**
 * @brief Retrieves the current balance in **Genius Tokens** as a formatted string.
 * @return The balance as a `GeniusTokenValue` struct, containing a GNUS value in string format.
 */
GNUS_VISIBILITY_DEFAULT GeniusTokenValue GeniusSDKGetBalanceGNUS();
GNUS_VISIBILITY_DEFAULT GeniusAddress    GeniusSDKGetAddress();

GNUS_VISIBILITY_DEFAULT GeniusMatrix GeniusSDKGetInTransactions();
GNUS_VISIBILITY_DEFAULT GeniusMatrix GeniusSDKGetOutTransactions();
GNUS_VISIBILITY_DEFAULT void         GeniusSDKFreeTransactions( GeniusMatrix matrix );

/**
 * @brief     Mints new tokens specified in **Minion Tokens** (1e-9 GNUS).
 * @param[in] amount           The amount to be minted in Minion Tokens.
 * @param[in] transaction_hash A null-terminated string representing the transaction hash.
 * @param[in] chain_id         A null-terminated string representing the blockchain chain ID.
 * @param[in] token_id         A null-terminated string representing the token ID.
 * @note If you have a Genius Token amount, use `GeniusSDKToMinions` to convert
 *       GNUS to Minion Tokens before calling this function.
 */
GNUS_VISIBILITY_DEFAULT void GeniusSDKMint( uint64_t amount, const char *transaction_hash, const char *chain_id,
                                            const char *token_id );

/**
 * @brief     Mints new tokens using a **Genius Token** string format.
 * @param[in] gnus             Pointer to a `GeniusTokenValue` struct representing the amount in GNUS.
 * @param[in] transaction_hash A null-terminated string representing the transaction hash.
 * @param[in] chain_id         A null-terminated string representing the blockchain chain ID.
 * @param[in] token_id         A null-terminated string representing the token ID.
 */
GNUS_VISIBILITY_DEFAULT void GeniusSDKMintGNUS( const GeniusTokenValue *gnus, const char *transaction_hash,
                                                const char *chain_id, const char *token_id );

/**
 * @brief     Transfers tokens in **Minion Tokens** (1e-9 GNUS) to another address.
 * @param[in] amount The amount to transfer in Minion Tokens.
 * @param[in] dest   Pointer to a `GeniusAddress` struct representing the recipient's address.
 * @return `true` if the transfer is successful, `false` otherwise.
 * @note If you have a Genius Token amount, use `GeniusSDKToMinions` to convert
 *       GNUS to Minion Tokens before calling this function.
 */
GNUS_VISIBILITY_DEFAULT bool GeniusSDKTransfer( uint64_t amount, GeniusAddress *dest );

/**
 * @brief     Transfers tokens using a **Genius Token** string representation.
 * @param[in] gnus Pointer to a `GeniusTokenValue` struct representing the amount in GNUS.
 * @param[in] dest Pointer to a `GeniusAddress` struct representing the recipient's address.
 * @return `true` if the transfer is successful, `false` otherwise.
 */
GNUS_VISIBILITY_DEFAULT bool GeniusSDKTransferGNUS( const GeniusTokenValue *gnus, GeniusAddress *dest );

/**
 * @brief Computes the cost of an operation based on the given JSON data (in **Minion Tokens**).
 * @param[in] jsondata The JSON data to be processed.
 * @return A `uint64_t` representing the cost in Minion Tokens.
 */
GNUS_VISIBILITY_DEFAULT uint64_t GeniusSDKGetCost( const JsonData_t jsondata );

/**
 * @brief Computes the cost of an operation based on the given JSON data (in **Genius Tokens**).
 * @param[in] jsondata The JSON data to be processed.
 * @return A `GeniusTokenValue` struct representing the cost in Genius Tokens.
 */
GNUS_VISIBILITY_DEFAULT GeniusTokenValue GeniusSDKGetCostGNUS( const JsonData_t jsondata );
GNUS_VISIBILITY_DEFAULT void             GeniusSDKProcess( const JsonData_t jsondata );

/**
 * @brief     Converts an amount in **Genius Tokens** (string) to **Minion Tokens** (1e-9 GNUS).
 * @param[in] gnus Pointer to a `GeniusTokenValue` struct containing the GNUS amount as a string.
 * @return    A `uint64_t` value representing the equivalent amount in Minion Tokens.
 */
GNUS_VISIBILITY_DEFAULT uint64_t GeniusSDKToMinions( const GeniusTokenValue *gnus );

/**
 * @brief     Converts an amount in **Minion Tokens** (1e-9 GNUS) to a **Genius Token** string.
 * @param[in] minions The amount in Minion Tokens.
 * @return    A `GeniusTokenValue` struct containing the amount in Genius Tokens.
 */
GNUS_VISIBILITY_DEFAULT GeniusTokenValue GeniusSDKToGenius( uint64_t minions );

GNUS_EXPORT_END

#endif
