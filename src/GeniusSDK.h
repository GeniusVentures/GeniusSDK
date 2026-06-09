/**
 * @brief      Genius SDK for managing and interacting with Genius Tokens (GNUS).
 * @date       2024-05-25
 *
 * This SDK provides functions to initialize and shut down a Genius node,
 * retrieve balances, transfer tokens, and process data.
 *
 * - **Minion Tokens**: The smallest sub-unit of Genius Tokens (1 Minion = 1e-6 GNUS).
 *   Functions that accept or return a `uint64_t` amount refer to Minion Tokens.
 * - **Genius Tokens (GNUS)**: The standard unit of the token system. Functions that
 *   accept or return a `GeniusTokenValue` (string format) refer to Genius Tokens.
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


#define GENIUS_SDK_ADDRESS_SIZE (2 + 128) ///< 2 for prefix (0x) + 128 hex characters

typedef struct
{
    /// A string prepended with `0x` followed by 128 hex characters,
    /// including a null-terminating char just for safety.
    char address[GENIUS_SDK_ADDRESS_SIZE + 1];
} GeniusAddress;

/**
 * @brief Represents a Genius token value in fixed-point format as a string.
 *
 * This structure provides enough space to hold the largest possible Genius token
 * string value ("18446744073709.551615") plus the null terminator.
 */
typedef struct
{
    char value[22]; ///< String representation of the Genius token value
} GeniusTokenValue;

/**
 * @brief A binary identifier for a Genius-compatible token.
 */
typedef struct
{
    unsigned char data[32]; ///< 32-byte raw token ID used internally
} GeniusTokenID;

typedef char     JsonData_t[2048]; ///< ID/Path of the image to be processed
typedef uint64_t PayAmount_t;      ///< Amount to be paid for the processing
typedef int32_t  GeniusNodeReturnValue_t;
typedef int32_t  GeniusNodeState_t;
typedef int32_t  GeniusTransactionManagerState_t;
typedef int32_t  GeniusTransactionStatus_t;
typedef int32_t  GeniusProcessingStatus_t;

typedef enum
{
    GENIUS_NODE_RET_OK = 0,
    GENIUS_NODE_ERROR_NOT_INITIALIZED,
    GENIUS_NODE_ERROR_PROCESS_IMAGE,
    GENIUS_NODE_ERROR_MINT,
    GENIUS_NODE_INVALID_ARGUMENT,
    GENIUS_NODE_ERROR_TRANSFER,
    GENIUS_NODE_ERROR_PAY_DEV
} GeniusNodeReturnValue;

/**
 * @brief Node state enumeration (maps to GeniusNode::NodeState)
 * Values must match GeniusNode::NodeState enum values
 */
typedef enum
{
    GENIUS_NODE_CREATING = 0,
    GENIUS_NODE_MIGRATING_DATABASE,
    GENIUS_NODE_INITIALIZING_DATABASE,
    GENIUS_NODE_INITIALIZING_PROCESSING,
    GENIUS_NODE_INITIALIZING_BLOCKCHAIN,
    GENIUS_NODE_INITIALIZING_TRANSACTIONS,
    GENIUS_NODE_INITIALIZING_DHT,
    GENIUS_NODE_READY,
} GeniusNodeState;

typedef enum
{
    GENIUS_TM_STATE_CREATING     = 0, ///< Creating the object
    GENIUS_TM_STATE_INITIALIZING = 1, ///< Initializing the object
    GENIUS_TM_STATE_SYNCHING     = 2, ///< Synching the transactions
    GENIUS_TM_STATE_READY        = 3  ///< Ready to process transactions
} GeniusTransactionManagerState;

/**
 * @brief Transaction Status enumeration (maps to TransactionManager::TransactionStatus)
 * Values must match TransactionManager::TransactionStatus enum values
 */
typedef enum
{
    GENIUS_TX_STATUS_CREATED   = 0, ///< Transaction created but not yet sent
    GENIUS_TX_STATUS_SENDING   = 1, ///< Transaction is being sent
    GENIUS_TX_STATUS_CONFIRMED = 2, ///< Transaction confirmed
    GENIUS_TX_STATUS_VERIFYING = 3, ///< Transaction being verified
    GENIUS_TX_STATUS_FAILED    = 4, ///< Transaction failed
    GENIUS_TX_STATUS_INVALID   = 5  ///< Invalid transaction
} GeniusTransactionStatus;

typedef enum
{
    GENIUS_PR_STATUS_DISABLED   = 0, ///< Processing was disabled
    GENIUS_PR_STATUS_IDLE       = 1, ///< Not processing at the moment
    GENIUS_PR_STATUS_PROCESSING = 2, ///< Currently processing a job s
} GeniusProcessingStatus;

/**
 * @brief Represents the current processing status with progress information.
 */
typedef struct
{
    GeniusProcessingStatus_t status;     ///< Current processing state
    float                    percentage; ///< Progress percentage from 0.0 to 100.0
} GeniusProcessingStatusInfo;

typedef struct
{
    const char *email;    ///< Null-terminated email
    const char *password; ///< Null-terminated password
} GeniusCredentials;

/**
 * @brief Inits the SDK with saved settings (no private key — uses existing wallet).
 * @param[in] base_path    Base path for node data storage. Must contain a `dev_config.json` file.
 * @param[in] autodht      Whether to auto-discover DHT peers.
 * @param[in] process      Whether to enable processing.
 * @param[in] baseport     Base network port for the node.
 * @param[in] is_full_node Whether to run as a full node.
 * @returns Initialization path in case of success, null on failure.
 */
GNUS_VISIBILITY_DEFAULT const char *GeniusSDKInit( const char *base_path,
                                                   bool        autodht,
                                                   bool        process,
                                                   uint16_t    baseport,
                                                   bool        is_full_node );
/**
 * @brief Inits the SDK with an ethereum private key.
 * @param[in] base_path       Base path for node data storage. Must contain a `dev_config.json` file.
 * @param[in] eth_private_key Valid HEX ethereum key, supports '0x' prefix.
 * @param[in] autodht         Whether to auto-discover DHT peers.
 * @param[in] process         Whether to enable processing.
 * @param[in] baseport        Base network port for the node.
 * @param[in] is_full_node    Whether to run as a full node.
 * @returns Initialization path in case of success, null on failure.
 */
GNUS_VISIBILITY_DEFAULT const char *GeniusSDKInitWithKey( const char *base_path,
                                                          const char *eth_private_key,
                                                          bool        autodht,
                                                          bool        process,
                                                          uint16_t    baseport,
                                                          bool        is_full_node );

/**
 * @brief Inits the SDK with an explicit developer config JSON string and an ethereum private key.
 * @param[in] base_path       Base path for node data storage.
 * @param[in] dev_config      Developer configuration as a JSON string (overrides dev_config.json).
 * @param[in] eth_private_key Valid HEX ethereum key, supports '0x' prefix.
 * @param[in] autodht         Whether to auto-discover DHT peers.
 * @param[in] process         Whether to enable processing.
 * @param[in] baseport        Base network port for the node.
 * @param[in] is_full_node    Whether to run as a full node.
 * @returns Initialization path in case of success, null on failure.
 */
GNUS_VISIBILITY_DEFAULT const char *GeniusSDKInitWithKeyAndDevConfig( const char *base_path,
                                                                      const char *dev_config,
                                                                      const char *eth_private_key,
                                                                      bool        autodht,
                                                                      bool        process,
                                                                      uint16_t    baseport,
                                                                      bool        is_full_node );

/**
 * @brief Inits the SDK with minimal configuration (convenience wrapper).
 * @details Equivalent to calling GeniusSDKInitWithKey() with autodht=true, process=true, is_full_node=false.
 * @param[in] base_path       Base path for node data storage.
 * @param[in] eth_private_key Valid HEX ethereum key, supports '0x' prefix.
 * @param[in] baseport        Base network port for the node.
 * @returns Initialization path in case of success, null on failure.
 */
GNUS_VISIBILITY_DEFAULT const char *GeniusSDKInitMinimal( const char *base_path,
                                                          const char *eth_private_key,
                                                          uint16_t    baseport );

/**
 * @brief Shuts down the SDK and releases all node resources.
 * @returns @ref GENIUS_NODE_RET_OK on success.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKShutdown();

/**
 * @brief Retrieves a list of available Genius accounts.
 * @return A null-terminated string containing newline-separated hex addresses,
 *         or null if the SDK is not initialized. The caller must free the
 *         returned string with free().
 */
GNUS_VISIBILITY_DEFAULT const char *GeniusSDKGetAvailableAccounts();

/**
 * @brief Adds a new account using an Ethereum private key.
 * @param[in] private_key Null-terminated string representing the private key in hex format (0x prefix optional).
 * @return @ref GENIUS_NODE_RET_OK on success, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED if the SDK is not initialized,
 *         or @ref GENIUS_NODE_INVALID_ARGUMENT on failure.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKAddAccountWithPrivateKey( const char *private_key );

/**
 * @brief Adds a new account using a mnemonic phrase.
 * @param[in] mnemonic Null-terminated string representing the mnemonic recovery phrase.
 * @return @ref GENIUS_NODE_RET_OK on success, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED if the SDK is not initialized,
 *         or @ref GENIUS_NODE_INVALID_ARGUMENT on failure.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKAddAccountWithMnemonic( const char *mnemonic );

/**
 * @brief Selects the active account for subsequent SDK operations.
 * @param[in] public_address Null-terminated string representing the account's public address.
 * @return @ref GENIUS_NODE_RET_OK on success, @ref GENIUS_NODE_ERROR_CREATING if not initialized,
 *         or @ref GENIUS_NODE_INVALID_ARGUMENT on failure.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKSelectGeniusAccount( const char *public_address );

/**
 * @brief Transfers an account to a different address.
 * @param[in] public_address Null-terminated string representing the target public address.
 * @return @ref GENIUS_NODE_RET_OK on success, @ref GENIUS_NODE_ERROR_CREATING if not initialized,
 *         or @ref GENIUS_NODE_INVALID_ARGUMENT on failure.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKTransferGeniusAccount( const char *public_address );

/**
 * @brief Merges an external account into the node's wallet.
 * @param[in] public_address Null-terminated string representing the account's public address to merge.
 * @return @ref GENIUS_NODE_RET_OK on success, @ref GENIUS_NODE_ERROR_CREATING if not initialized,
 *         or @ref GENIUS_NODE_INVALID_ARGUMENT on failure.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKMergeGeniusAccount( const char *public_address );

/**
 * @brief Deletes the account.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKDeleteAccount( const char *public_address );

/**
 * @brief Sets the payout address for processing rewards.
 * @param[in] public_address Null-terminated string representing the payout public address.
 * @return @ref GENIUS_NODE_RET_OK on success, @ref GENIUS_NODE_ERROR_CREATING if not initialized,
 *         or @ref GENIUS_NODE_INVALID_ARGUMENT on failure.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKSetPayoutAddress( const char *public_address );

/**
 * @brief Retrieves the current balance for a specific token.
 * @param[in] token_id  Token identifier to query.
 * @return The balance amount as a `uint64_t` value (in Minion Tokens).
 */
GNUS_VISIBILITY_DEFAULT uint64_t GeniusSDKGetBalance( GeniusTokenID token_id );
/**
 * @brief Retrieves the current balance in **Genius Tokens** as a formatted string.
 * @return The balance as a `GeniusTokenValue` struct, containing a GNUS value in string format.
 */
GNUS_VISIBILITY_DEFAULT GeniusTokenValue GeniusSDKGetBalanceGNUS();

/**
 * @brief Retrieves the current balance in **Genius Tokens** as a formatted string.
 * @return A pointer to a null-terminated UTF-8 string representing the current balance in GNUS.
 */
GNUS_VISIBILITY_DEFAULT const char *GeniusSDKGetBalanceGNUSString();

/**
 * @brief Retrieves the current USD price of gnus
 * @return The price as a `double` value in USD.
 */
GNUS_VISIBILITY_DEFAULT double GeniusSDKGetGNUSPrice();

/**
 * @brief       Retrieves the SDK version string.
 * @return      A pointer to a null-terminated UTF-8 string representing the SDK version.
 */
GNUS_VISIBILITY_DEFAULT const char *GeniusSDKGetVersion();

/**
 * @brief Returns the public address of the currently selected account.
 * @return Address filled or zeroized if SDK wasn't initialized.
 */
GNUS_VISIBILITY_DEFAULT GeniusAddress GeniusSDKGetAddress();

/**
 * @brief Retrieves all incoming transactions.
 * @return A @ref GeniusMatrix containing the incoming transactions.
 *         Must be freed with @ref GeniusSDKFreeTransactions().
 */
GNUS_VISIBILITY_DEFAULT GeniusMatrix GeniusSDKGetInTransactions();

/**
 * @brief Retrieves all outgoing transactions.
 * @return A @ref GeniusMatrix containing the outgoing transactions.
 *         Must be freed with @ref GeniusSDKFreeTransactions().
 */
GNUS_VISIBILITY_DEFAULT GeniusMatrix GeniusSDKGetOutTransactions();

/**
 * @brief Frees a @ref GeniusMatrix previously obtained from GetInTransactions() or GetOutTransactions().
 * @param[in] matrix The matrix to free.
 */
GNUS_VISIBILITY_DEFAULT void GeniusSDKFreeTransactions( GeniusMatrix matrix );

/**
 * @brief     Mints new tokens specified in **Minion Tokens**.
 * @param[in] amount           The amount to be minted in Minion Tokens.
 * @param[in] transaction_hash A null-terminated string representing the transaction hash.
 * @param[in] chain_id         A null-terminated string representing the blockchain chain ID.
 * @param[in] token_id         Token identifier.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKMint( uint64_t      amount,
                                                               const char   *transaction_hash,
                                                               const char   *chain_id,
                                                               GeniusTokenID token_id );

/**
 * @brief     Mints new tokens using a **Genius Token** string format.
 * @param[in] amount           Pointer to a `GeniusTokenValue` struct representing the amount in GNUS.
 * @param[in] transaction_hash A null-terminated string representing the transaction hash.
 * @param[in] chain_id         A null-terminated string representing the blockchain chain ID.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKMintGNUS( const GeniusTokenValue *amount,
                                                                   const char             *transaction_hash,
                                                                   const char             *chain_id );

/**
 * @brief     Transfers tokens in **Minion Tokens** to another address.
 * @param[in] amount    The amount to transfer in Minion Tokens.
 * @param[in] dest      Pointer to a `GeniusAddress` struct representing the recipient's address.
 * @param[in] token_id  Token identifier.
 * @return @ref GENIUS_NODE_RET_OK on success, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED if the SDK is not initialized,
 *         or @ref GENIUS_NODE_ERROR_TRANSFER on failure.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKTransfer( uint64_t       amount,
                                                                   GeniusAddress *dest,
                                                                   GeniusTokenID  token_id );

/**
 * @brief     Transfers tokens using a **Genius Token** string representation.
 * @param[in] amount Pointer to a `GeniusTokenValue` struct representing the amount in GNUS.
 * @param[in] dest   Pointer to a `GeniusAddress` struct representing the recipient's address.
 * @return @ref GENIUS_NODE_RET_OK on success, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED if the SDK is not initialized,
 *         @ref GENIUS_NODE_INVALID_ARGUMENT if amount or dest is null,
 *         or @ref GENIUS_NODE_ERROR_TRANSFER on failure.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKTransferGNUS( const GeniusTokenValue *amount,
                                                                       GeniusAddress          *dest );

/**
 * @brief     Pays the developer for in-game transactions.
 * @param[in] amount   The amount to transfer in Minion Tokens.
 * @param[in] token_id Token identifier.
 * @return @ref GENIUS_NODE_RET_OK on success, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED if the SDK is not initialized,
 *         or @ref GENIUS_NODE_ERROR_PAY_DEV on failure.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKPayDev( uint64_t amount, GeniusTokenID token_id );

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
/**
 * @brief Submits data for processing based on the given JSON data.
 * @param[in] jsondata The JSON data to be processed.
 * @return A `GeniusNodeReturnValue_t` indicating the result of the operation.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKProcess( const JsonData_t jsondata );
/**
 * @brief Checks the validity of a job based on the given JSON data.
 * @param[in] jsondata The JSON data to be processed.
 * @return `true` if the job is valid, `false` otherwise.
 */
GNUS_VISIBILITY_DEFAULT bool GeniusSDKCheckJobValidity( const JsonData_t jsondata );
/**
 * @brief       Retrieves the current state of the Transaction Manager.
 * @return      The current state as a @ref GeniusTransactionManagerState enum value.
 */
GNUS_VISIBILITY_DEFAULT GeniusTransactionManagerState_t GeniusSDKGetTransactionManagerState();

/**
 * @brief       Retrieves the current state of the Genius Node.
 * @return      The current state as a @ref GeniusNodeState enum value.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeState_t GeniusSDKGetNodeState();

/**
 * @brief       Retrieves the status of a specific transaction.
 * @param[in]   tx_id A null-terminated string representing the transaction ID.
 * @return      The transaction status as a @ref GeniusTransactionStatus enum value.
 */
GNUS_VISIBILITY_DEFAULT GeniusTransactionStatus_t GeniusSDKGetTransactionStatus( const char *tx_id );

/**
 * @brief       Retrieves the current processing status with progress information.
 * @return      A @ref GeniusProcessingStatusInfo struct containing the processing status and percentage.
 */
GNUS_VISIBILITY_DEFAULT GeniusProcessingStatusInfo GeniusSDKGetProcessingStatus();

GNUS_EXPORT_END

#endif
