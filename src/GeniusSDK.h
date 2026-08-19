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

// NOLINTBEGIN(modernize-use-using, modernize-deprecated-headers, cppcoreguidelines-avoid-c-arrays, performance-enum-size)

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

#define GENIUS_SDK_ADDRESS_SIZE ( 2 + 128 ) ///< 2 for prefix (0x) + 128 hex characters

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

#define GENIUS_SDK_MAX_METADATA_STRING_SIZE 128 ///< Bound for opaque caller-supplied registration metadata strings

/**
 * @brief Caller-supplied registration metadata for a child wallet, mirroring the
 *        SGTransaction::RegistrationMetadata proto fields.
 */
typedef struct
{
    char     game_id[GENIUS_SDK_MAX_METADATA_STRING_SIZE];      ///< Opaque game identifier string
    char     publisher_id[GENIUS_SDK_MAX_METADATA_STRING_SIZE]; ///< Opaque publisher identifier string
    char     dev_wallet[GENIUS_SDK_MAX_METADATA_STRING_SIZE];   ///< Opaque developer wallet identifier bytes/string
    uint64_t peers_cut;                                         ///< Peers' cut value
} GeniusRegistrationMetadata;

/**
 * @brief A single child-wallet registration discovered via GeniusSDKGetRegistrationsForMain.
 */
typedef struct
{
    GeniusAddress              child_address; ///< Registered child wallet's public address
    GeniusAddress              main_address;  ///< Main wallet public address the child is registered under
    uint64_t                   sequence;      ///< Registration sequence number
    GeniusRegistrationMetadata metadata;      ///< Registration metadata submitted at registration time
} GeniusRegistrationDiscoveryEntry;

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
    GENIUS_NODE_ERROR_PAY_DEV,
    GENIUS_NODE_ERROR_REGISTRATION ///< GeniusSDKRegisterChild submission failure, GeniusSDKGetRegistrationsForMain
                                    ///< discovery-query failure, or GeniusSDKDetachChild/GeniusSDKReplaceMain/
                                    ///< GeniusSDKRevokeChild lifecycle-change submission failure
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
    float percentage;
    char *message;
} GeniusStatusInfo;

#define GENIUS_SDK_MAX_MNEMONIC_SIZE                                                                                   \
    ( 24 * 8 + 23 + 1 ) // 24 words (with max of 8 characters), 23 space separators, 1 null termination

typedef struct
{
    char mnemonic[GENIUS_SDK_MAX_MNEMONIC_SIZE];
} GeniusMnemonic;

/**
 * @brief Return type for account creation that includes both a status code and
 *        the generated mnemonic phrase.
 */
typedef struct
{
    GeniusProcessingStatus_t
         status; ///< Return status (@ref GENIUS_NODE_RET_OK or @ref GENIUS_NODE_ERROR_NOT_INITIALIZED)
    char mnemonic[GENIUS_SDK_MAX_MNEMONIC_SIZE]; ///< Null terminated string
} GeniusMnemonicAndStatus;

/**
 * @brief Return type for SDK initialization with a random mnemonic, bundling
 *        the initialization path and the generated recovery phrase.
 */
typedef struct
{
    const char *initialization_path; ///< Statically allocated, do not call free
    char        mnemonic[GENIUS_SDK_MAX_MNEMONIC_SIZE];
} GeniusMnemonicAndInitPath;

/**
 * @brief Inits the SDK with saved settings (no private key - uses existing wallet).
 * If no account exists, creates with a random mnemonic.
 * @param[in] base_path  Base path for node data storage.
 * @param[in] dev_config Developer configuration JSON string (Address, Cut, TokenValue, TokenID).
 * @returns Initialization path in case of success, null on failure.
 */
GNUS_VISIBILITY_DEFAULT const char *GeniusSDKInit( const char *base_path, const char *dev_config );

/**
 * @brief Inits the SDK with an ethereum private key.
 * @param[in] base_path       Base path for node data storage.
 * @param[in] dev_config      Developer configuration JSON string.
 * @param[in] eth_private_key Valid HEX ethereum key, supports '0x' prefix.
 * @returns Initialization path in case of success, null on failure.
 */
GNUS_VISIBILITY_DEFAULT const char *GeniusSDKInitWithKey( const char *base_path,
                                                          const char *dev_config,
                                                          const char *eth_private_key );

/**
 * @brief Inits the SDK with a BIP39 mnemonic recovery phrase.
 * @param[in] base_path  Base path for node data storage.
 * @param[in] dev_config Developer configuration JSON string.
 * @param[in] mnemonic   BIP39 mnemonic recovery phrase.
 * @returns Initialization path in case of success, null on failure.
 */
GNUS_VISIBILITY_DEFAULT const char *GeniusSDKInitWithMnemonic( const char *base_path,
                                                               const char *dev_config,
                                                               const char *mnemonic );

/**
 * @brief Shuts down the SDK and releases all node resources.
 * @returns @ref GENIUS_NODE_RET_OK on success.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKShutdown();

/**
 * @brief Reloads log level overrides from log_config.json at runtime.
 */
GNUS_VISIBILITY_DEFAULT void GeniusSDKLoadLogConfig();

/**
 * @brief Frees memory allocated by the SDK.
 * @param[in] ptr Pointer to the memory block to free. May be null (no-op).
 */
GNUS_VISIBILITY_DEFAULT void GeniusSDKFree( void *ptr );

/**
 * @brief Retrieves the current SDK initialization progress.
 * @return A @ref GeniusStatusInfo struct containing:
 *         - `percentage`: Initialization progress from 0.0 to 1.0
 *         - `message`: A null-terminated string describing the current initialization step,
 *           or null if the SDK is not initialized. The caller must free `message` with
 *           @ref GeniusSDKFree().
 */
GNUS_VISIBILITY_DEFAULT GeniusStatusInfo GeniusSDKGetInitializationStatus();

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
 * @brief Adds a new account using a randomly generated mnemonic phrase.
 * @return A @ref GeniusMnemonicAndStatus struct. On success, `status` is
 *         @ref GENIUS_NODE_RET_OK and `mnemonic` contains the generated phrase
 *         (must be freed with @ref GeniusSDKFree). On failure, `status` is
 *         @ref GENIUS_NODE_ERROR_NOT_INITIALIZED and `mnemonic` is null.
 */
GNUS_VISIBILITY_DEFAULT GeniusMnemonicAndStatus GeniusSDKAddAccountWithRandomMnemonic();

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
 * @brief Returns the mnemonic of the current active account or `nullptr`
 *        if the account was not created from a mnemonic.
 * @return A heap-allocated null-terminated mnemonic string, or `nullptr` if
 *         the SDK is not initialized or the account has no mnemonic.
 *         Must be freed with @ref GeniusSDKFree.
 */
GNUS_VISIBILITY_DEFAULT GeniusMnemonic GeniusSDKGetMnemonic();

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
 * @brief       Retrieves an opaque handle to the running node's shared PubSub instance.
 * @return      Opaque handle to the node's `sgns::ipfs_pubsub::GossipPubSub`, or `nullptr`
 *              if the SDK has not been initialized yet (no active node) or the node has
 *              not started its PubSub service. Ownership remains with the SDK's internal
 *              node instance — the handle must NOT be passed to GeniusSDKFree() and is
 *              only valid for as long as the initialized node remains alive.
 */
GNUS_VISIBILITY_DEFAULT void *GeniusSDKGetPubSub();

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

/**
 * @brief       Returns the task IDs of jobs submitted by the active account.
 * @param[in]   limit  Maximum number of task IDs to return.
 * @param[in]   offset Number of task IDs to skip from the end of the list.
 * @return      A null-terminated string containing newline-separated task IDs,
 *              or null if the SDK is not initialized. Must be freed with GeniusSDKFree().
 */
GNUS_VISIBILITY_DEFAULT const char *GeniusSDKGetMyTaskIds( uint64_t limit, uint64_t offset );

/**
 * @brief       Retrieves the completed result for a specific task.
 * @param[in]   task_id A null-terminated string representing the task ID (ipfs_block_id).
 * @return      A GeniusArray containing the serialized protobuf bytes of TaskResult,
 *              or {0, nullptr} if the task is not found or SDK is not initialized.
 *              On success, the ptr field must be freed with GeniusSDKFree().
 */
GNUS_VISIBILITY_DEFAULT GeniusArray GeniusSDKGetTaskResult( const char *task_id );

/* --- Child Wallet Interfaces (v2.2) --- */

/**
 * @brief     Registers this node as a child wallet under a main wallet address.
 *            Wraps the auto-derived-sequence overload of `GeniusNode::RegisterChild` —
 *            the registration sequence number is derived automatically from this node's
 *            existing reg/ CRDT record; no sequence value is accepted by this function.
 * @param[in] main_address Null-terminated string representing the main wallet's public address.
 * @param[in] metadata     Registration metadata (game_id, publisher_id, dev_wallet, peers_cut).
 * @return @ref GENIUS_NODE_RET_OK on success, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED if the SDK
 *         is not initialized, @ref GENIUS_NODE_INVALID_ARGUMENT if `main_address` is null/empty,
 *         or @ref GENIUS_NODE_ERROR_REGISTRATION if registration submission failed.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKRegisterChild( const char                 *main_address,
                                                                        GeniusRegistrationMetadata  metadata );

/**
 * @brief      Enumerates child wallets registered under a given main wallet address.
 *             Wraps `GeniusNode::GetRegistrationsForMain`.
 * @param[in]  main_address Null-terminated string representing the main wallet's public address.
 * @param[out] out_entries  On success, set to a heap-allocated array of discovered registrations
 *                          (caller must free with @ref GeniusSDKFree); set to null on failure or
 *                          when there are zero registrations.
 * @param[out] out_count    On success, set to the number of entries in `*out_entries` (may be 0,
 *                          which is a valid empty result, not a failure); set to 0 on failure.
 * @return @ref GENIUS_NODE_RET_OK on success (including the zero-registrations case),
 *         @ref GENIUS_NODE_ERROR_NOT_INITIALIZED if the SDK is not initialized,
 *         @ref GENIUS_NODE_INVALID_ARGUMENT if `main_address`, `out_entries`, or `out_count`
 *         is null, or @ref GENIUS_NODE_ERROR_REGISTRATION if the discovery query failed.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKGetRegistrationsForMain(
    const char                        *main_address,
    GeniusRegistrationDiscoveryEntry **out_entries,
    uint64_t                          *out_count );

/**
 * @brief Retrieves a child wallet's balance for a specific token, read from the locally-synced
 *        CRDT UTXO view. Wraps the token-filtered overload of `GeniusNode::GetChildBalance`.
 * @param[in] child_address Null-terminated string representing the child wallet's public address.
 * @param[in] token_id      Token identifier to filter by.
 * @return The balance amount as a `uint64_t` value (in Minion Tokens), or 0 if the SDK is not
 *         initialized or `child_address` is null. As with @ref GeniusSDKGetBalance, 0 is
 *         inherently ambiguous (no balance vs. not-yet-synced).
 */
GNUS_VISIBILITY_DEFAULT uint64_t GeniusSDKGetChildBalance( const char *child_address, GeniusTokenID token_id );

/**
 * @brief Retrieves a child wallet's total balance across all tokens, read from the locally-synced
 *        CRDT UTXO view. Wraps the all-tokens overload of `GeniusNode::GetChildBalance`.
 * @param[in] child_address Null-terminated string representing the child wallet's public address.
 * @return The total balance amount as a `uint64_t` value (in Minion Tokens), or 0 if the SDK is
 *         not initialized or `child_address` is null. Same 0-ambiguity caveat as
 *         @ref GeniusSDKGetChildBalance applies.
 */
GNUS_VISIBILITY_DEFAULT uint64_t GeniusSDKGetChildBalanceAll( const char *child_address );

/* --- Child Wallet Transfers (v2.3) --- */

/**
 * @brief     Funds a registered child wallet by transferring tokens to it (in **Minion Tokens**).
 *            Wraps the fire-and-forget overload of `GeniusNode::TransferFunds` — this is an
 *            ordinary transfer to the child's address, no new consensus mechanics involved.
 * @param[in] amount        The amount to transfer in Minion Tokens.
 * @param[in] child_address Null-terminated string representing the child wallet's public address.
 * @param[in] token_id      Token identifier.
 * @return @ref GENIUS_NODE_RET_OK on successful submission (submitted, not confirmed by
 *         consensus — see @ref GeniusSDKRecoverFromChild's @note for the related caveat),
 *         @ref GENIUS_NODE_ERROR_NOT_INITIALIZED if the SDK is not initialized,
 *         @ref GENIUS_NODE_INVALID_ARGUMENT if `child_address` is null/empty,
 *         or @ref GENIUS_NODE_ERROR_TRANSFER on submission failure.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKFundChild( uint64_t       amount,
                                                                     const char    *child_address,
                                                                     GeniusTokenID  token_id );

/**
 * @brief     Funds a registered child wallet using a **Genius Token** string representation.
 *            Parses `amount` via `GeniusNode::ParseTokens` then delegates to
 *            @ref GeniusSDKFundChild, mirroring how `GeniusSDKTransferGNUS` delegates to
 *            `GeniusSDKTransfer`.
 * @param[in] amount        Pointer to a `GeniusTokenValue` struct representing the amount in GNUS.
 * @param[in] child_address Null-terminated string representing the child wallet's public address.
 * @return @ref GENIUS_NODE_RET_OK on successful submission, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED
 *         if the SDK is not initialized, @ref GENIUS_NODE_INVALID_ARGUMENT if `amount` is null or
 *         `child_address` is null/empty, or @ref GENIUS_NODE_ERROR_TRANSFER on submission failure.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKFundChildGNUS( const GeniusTokenValue *amount,
                                                                         const char             *child_address );

/**
 * @brief     Recovers funds from a registered child wallet back to this node's address
 *            (in **Minion Tokens**). Wraps the fire-and-forget overload of
 *            `GeniusNode::RecoverFromChild`. This wrapper's own parameter order
 *            (`amount`, `child_address`, `token_id`) mirrors @ref GeniusSDKFundChild for API
 *            symmetry, even though `GeniusNode::RecoverFromChild`'s actual signature takes
 *            `child_address` first.
 * @param[in] amount        The amount to recover in Minion Tokens.
 * @param[in] child_address Null-terminated string representing the child wallet's public address.
 * @param[in] token_id      Token identifier.
 * @return @ref GENIUS_NODE_RET_OK on successful submission, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED
 *         if the SDK is not initialized, @ref GENIUS_NODE_INVALID_ARGUMENT if `child_address` is
 *         null/empty, or @ref GENIUS_NODE_ERROR_TRANSFER on submission failure (the same code
 *         @ref GeniusSDKFundChild uses — no dedicated recovery error value).
 * @note This call is fire-and-forget and reports submission-time status only; the
 *       destination-mismatch rejection performed by `CheckParentChildAuthority` is only
 *       evaluated at consensus finalization and cannot be observed by this synchronous
 *       wrapper — @ref GENIUS_NODE_RET_OK means "submitted," not "confirmed by consensus."
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKRecoverFromChild( uint64_t      amount,
                                                                            const char   *child_address,
                                                                            GeniusTokenID token_id );

/**
 * @brief     Recovers funds from a registered child wallet using a **Genius Token** string
 *            representation. Parses `amount` via `GeniusNode::ParseTokens` then delegates to
 *            @ref GeniusSDKRecoverFromChild, mirroring how `GeniusSDKTransferGNUS` delegates to
 *            `GeniusSDKTransfer`.
 * @param[in] amount        Pointer to a `GeniusTokenValue` struct representing the amount in GNUS.
 * @param[in] child_address Null-terminated string representing the child wallet's public address.
 * @return @ref GENIUS_NODE_RET_OK on successful submission, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED
 *         if the SDK is not initialized, @ref GENIUS_NODE_INVALID_ARGUMENT if `amount` is null or
 *         `child_address` is null/empty, or @ref GENIUS_NODE_ERROR_TRANSFER on submission failure.
 * @note Same fire-and-forget/D-21-observability limitation as @ref GeniusSDKRecoverFromChild
 *       applies — see that function's @note.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKRecoverFromChildGNUS( const GeniusTokenValue *amount,
                                                                                const char             *child_address );

/* --- Child Wallet Lifecycle (v2.4) --- */

/**
 * @brief     Creates a child-initiated Detach transaction, ending this node's own child-wallet
 *            registration under its current main (D-35). Wraps the auto-derived-sequence
 *            overload of `GeniusNode::DetachChild` — the registration sequence number is derived
 *            automatically from this node's existing reg/ CRDT record; no address parameter is
 *            required since this node acts as the child on its own registration.
 * @param[in] metadata Registration metadata carried forward on the lifecycle-change transaction.
 * @return @ref GENIUS_NODE_RET_OK on successful submission, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED
 *         if the SDK is not initialized, or @ref GENIUS_NODE_ERROR_REGISTRATION if submission
 *         failed — including the case where no prior reg/ record exists for this node (the
 *         auto-derive overload's own fail-closed behavior, not a wrapper-level argument check).
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKDetachChild( GeniusRegistrationMetadata metadata );

/**
 * @brief     Creates a child-initiated Replace-Main transaction, changing this node's registered
 *            main wallet address (D-37). Wraps the auto-derived-sequence overload of
 *            `GeniusNode::ReplaceMain`.
 * @param[in] new_main_address Null-terminated string representing the new main wallet's public
 *                             address (128-hex).
 * @param[in] metadata         Registration metadata carried forward on the lifecycle-change
 *                             transaction.
 * @return @ref GENIUS_NODE_RET_OK on successful submission, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED
 *         if the SDK is not initialized, @ref GENIUS_NODE_INVALID_ARGUMENT if `new_main_address`
 *         is null/empty, or @ref GENIUS_NODE_ERROR_REGISTRATION if submission failed — including
 *         the no-prior-reg/-record fail-closed case, same caveat as @ref GeniusSDKDetachChild.
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKReplaceMain( const char *new_main_address, GeniusRegistrationMetadata metadata );

/**
 * @brief     Creates a main-initiated Revoke transaction against a registered child wallet
 *            (D-36). Wraps the fire-and-forget overload of `GeniusNode::RevokeChild` — mirrors
 *            @ref GeniusSDKRecoverFromChild's choice of the non-timeout overload.
 * @param[in] child_address Null-terminated string representing the registered child wallet's
 *                          public address being revoked.
 * @return @ref GENIUS_NODE_RET_OK on successful submission, @ref GENIUS_NODE_ERROR_NOT_INITIALIZED
 *         if the SDK is not initialized, @ref GENIUS_NODE_INVALID_ARGUMENT if `child_address` is
 *         null/empty, or @ref GENIUS_NODE_ERROR_REGISTRATION on submission failure.
 * @note This call is fire-and-forget and reports submission-time status only; unauthorized-revoke
 *       rejection (non-main caller, sequence mismatch — enforced by the `CheckParentChildAuthority`
 *       revoke branch and `FilterRegistration` gate 3b) is only evaluated at consensus finalization
 *       and cannot be observed by this synchronous wrapper — @ref GENIUS_NODE_RET_OK means
 *       "submitted," not "confirmed by consensus."
 */
GNUS_VISIBILITY_DEFAULT GeniusNodeReturnValue_t GeniusSDKRevokeChild( const char *child_address );

GNUS_EXPORT_END

#endif

// NOLINTEND(modernize-use-using, modernize-deprecated-headers, cppcoreguidelines-avoid-c-arrays, performance-enum-size)
