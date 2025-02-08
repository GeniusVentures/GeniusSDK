/**
 * @file    SDKExample.cpp
 * @author  Luiz Guilherme Rizzatto Zucchi (luizgrz@gmail.com)
 * @brief   Example application for initializing and interacting with the GeniusSDK
 * @version 1.0
 * @date    2025-02-06
 */
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <fcntl.h>
#include "GeniusSDK.h"

static const int MAX_INPUT_SIZE = 256; ///< Maximum size for user input buffers.

static void initSDK();
static void getBalance();
static void getAddress();
static void getInTransactions();
static void getOutTransactions();
static void processSampleData();
static void getProcessingCost();
static void mintTokens();
static void transferTokens();
static void shutdownSDK();

static void     suppressSDKLogs();
static void     getSDKConfig( char *base_path, char *eth_private_key, int32_t *autodht, int32_t *process,
                              uint16_t *baseport );
static bool     loadJsonFromFile( const char *filename, JsonData_t jsonBuffer );
static void     promptString( const char *prompt, char *destination, size_t maxLen, const char *defaultValue );
static int      promptInt( const char *prompt, int defaultValue );
static uint16_t promptUShort( const char *prompt, uint16_t defaultValue );
static uint64_t promptUInt64( const char *prompt, uint64_t defaultValue );
static void     readLine( char *buffer, size_t bufferSize );
void            userPrint( const char *fmt, ... );

static int original_stdout_fd = -1; ///< To preserve original stdout when suppressing SDK logs.

int main()
{
    static const struct MenuOption
    {
        int32_t     option;      ///< Option number
        const char *description; ///< Option description
        void ( *function )();    ///< Function to execute when the option is selected
    } options[] = { { 1, "Initialize the SDK", initSDK },
                    { 2, "Get Balance", getBalance },
                    { 3, "Get Address", getAddress },
                    { 4, "Get Incoming Transactions", getInTransactions },
                    { 5, "Get Outgoing Transactions", getOutTransactions },
                    { 6, "Process Sample Data", processSampleData },
                    { 7, "Get Processing Cost", getProcessingCost },
                    { 8, "Mint Tokens", mintTokens },
                    { 9, "Transfer Tokens", transferTokens },
                    { 10, "Shutdown SDK", shutdownSDK },
                    { 0, "Exit", nullptr } };

    suppressSDKLogs();

    int choice = -1;
    do
    {
        userPrint( "\nSelect an option:\n" );
        for ( int32_t i = 0; options[i].option != 0; i++ )
        {
            userPrint( "%d - %s\n", options[i].option, options[i].description );
        }
        choice = promptInt( "Enter choice: ", -1 );

        bool validChoice = false;
        for ( int32_t i = 0; options[i].option != 0; i++ )
        {
            if ( options[i].option == choice )
            {
                validChoice = true;
                if ( options[i].function )
                {
                    options[i].function();
                }
                else
                {
                    userPrint( "Exiting...\n" );
                }
                break;
            }
        }

        if ( !validChoice && choice != 0 )
        {
            userPrint( "Invalid choice. Please select again.\n" );
        }
    } while ( choice != 0 );

    return 0;
}

/**
 * @brief Initializes the GeniusSDK
 */
static void initSDK()
{
    char     base_path[MAX_INPUT_SIZE]       = "./";
    char     eth_private_key[MAX_INPUT_SIZE] = "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
    int32_t  autodht                         = 1;
    int32_t  process                         = 1;
    uint16_t baseport                        = 40001;

    userPrint( "\n--- SDK Initialization ---\n" );
    getSDKConfig( base_path, eth_private_key, &autodht, &process, &baseport );

    const char *init_result = GeniusSDKInit( base_path, eth_private_key, autodht, process, baseport );
    if ( !init_result || strncmp( init_result, "Initialized", strlen( "Initialized" ) ) != 0 )
    {
        userPrint( "Failed to initialize GeniusSDK. Error: %s\n", init_result ? init_result : "No response" );
        return;
    }

    userPrint( "GeniusSDK initialized successfully.\n" );
}

/**
 * @brief Retrieves and prints the current balance.
 */
static void getBalance()
{
    uint64_t balance = GeniusSDKGetBalance();
    userPrint( "Current Balance: %llu\n", balance );
}

/**
 * @brief Retrieves and prints the wallet address.
 */
static void getAddress()
{
    GeniusAddress address = GeniusSDKGetAddress();
    userPrint( "Wallet Address: %s\n", address.address );
}

/**
 * @brief Retrieves and prints the number of incoming transactions.
 */
static void getInTransactions()
{
    GeniusMatrix inTransactions = GeniusSDKGetInTransactions();
    userPrint( "Incoming Transactions: %llu\n", inTransactions.size );
    GeniusSDKFreeTransactions( inTransactions );
}

/**
 * @brief Retrieves and prints the number of outgoing transactions.
 */
static void getOutTransactions()
{
    GeniusMatrix outTransactions = GeniusSDKGetOutTransactions();
    userPrint( "Outgoing Transactions: %llu\n", outTransactions.size );
    GeniusSDKFreeTransactions( outTransactions );
}

/**
 * @brief Processes a sample JSON data file using the GeniusSDK.
 */
static void processSampleData()
{
    char jsonFilePath[MAX_INPUT_SIZE] = "sample.json";
    promptString( "Enter JSON file path for sample data (Press Enter for default: sample.json): ", jsonFilePath,
                  MAX_INPUT_SIZE, "sample.json" );

    JsonData_t localSampleData;
    if ( !loadJsonFromFile( jsonFilePath, localSampleData ) )
    {
        userPrint( "Failed to load JSON file.\n" );
        return;
    }

    GeniusSDKProcess( localSampleData );
    userPrint( "Processed sample JSON data.\n" );
}

/**
 * @brief Retrieves and prints the estimated processing cost for the JSON data.
 */
static void getProcessingCost()
{
    char jsonFilePath[MAX_INPUT_SIZE] = "sample.json";
    promptString( "Enter JSON file path for sample data (Press Enter for default: sample.json): ", jsonFilePath,
                  MAX_INPUT_SIZE, "sample.json" );

    JsonData_t localSampleData;
    if ( !loadJsonFromFile( jsonFilePath, localSampleData ) )
    {
        userPrint( "Failed to load JSON file.\n" );
        return;
    }
    uint64_t cost = GeniusSDKGetCost( localSampleData );
    userPrint( "Estimated processing cost: %llu\n", cost );
}

/**
 * @brief Mints tokens by prompting the user for an amount.
 */
static void mintTokens()
{
    uint64_t amount = promptUInt64( "Enter the amount of Minion Tokens to mint: ", 0 );

    const char *transaction_hash = "";
    const char *chain_id         = "";
    const char *token_id         = "";

    GeniusSDKMintTokens( amount, transaction_hash, chain_id, token_id );
    userPrint( "Minted %llu Minion Tokens successfully.\n", amount );
}

/**
 * @brief Transfers tokens by prompting the user for an amount and a recipient address.
 */
static void transferTokens()
{
    uint64_t amount = promptUInt64( "Enter the amount of Minion Tokens to transfer: ", 0 );

    GeniusAddress recipient;
    userPrint( "Enter recipient wallet address: " );
    readLine( recipient.address, sizeof( recipient.address ) );

    bool transferSuccess = GeniusSDKTransferTokens( amount, &recipient );
    userPrint( "Token transfer %s.\n", transferSuccess ? "successful" : "failed" );
}

/**
 * @brief Shuts down the GeniusSDK.
 */
static void shutdownSDK()
{
    GeniusSDKShutdown();
    userPrint( "GeniusSDK shut down successfully.\n" );
}

/**
 * @brief Suppresses SDK logs by redirecting stdout to /dev/null.
 */
static void suppressSDKLogs()
{
    fflush( stdout );

#ifdef _WIN32
    original_stdout_fd = _dup( _fileno( stdout ) );

    int null_fd = _open( "NUL", _O_WRONLY );
    if ( null_fd != -1 )
    {
        _dup2( null_fd, _fileno( stdout ) );
        _close( null_fd );
    }
#else
    original_stdout_fd = dup( STDOUT_FILENO );

    int devnull_fd = open( "/dev/null", O_WRONLY );
    if ( devnull_fd != -1 )
    {
        dup2( devnull_fd, STDOUT_FILENO );
        close( devnull_fd );
    }
#endif
}

/**
 * @brief Retrieves configuration values for initializing the GeniusSDK.
 * @param[out] base_path Buffer to store the base path.
 * @param[out] eth_private_key Buffer to store the Ethereum private key.
 * @param[out] autodht Pointer to store the AutoDHT flag.
 * @param[out] process Pointer to store the processing flag.
 * @param[out] baseport Pointer to store the base port number.
 */
static void getSDKConfig( char *base_path, char *eth_private_key, int32_t *autodht, int32_t *process,
                          uint16_t *baseport )
{
    userPrint( "\nConfigure GeniusSDK Initialization:\n" );

    promptString( "Enter base path (Press Enter for default: ./): ", base_path, MAX_INPUT_SIZE, "./" );
    promptString( "Enter Ethereum private key (Press Enter for default): ", eth_private_key, MAX_INPUT_SIZE,
                  "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef" );
    *autodht  = promptInt( "Enable AutoDHT? (1 = Yes, 0 = No, Press Enter for default: 1): ", 1 );
    *process  = promptInt( "Enable processing? (1 = Yes, 0 = No, Press Enter for default: 1): ", 1 );
    *baseport = promptUShort( "Enter base port (Press Enter for default: 40001): ", 40001 );
}

/**
 * @brief Loads the contents of a file into a provided JSON buffer.
 * @param filename The path to the file to load.
 * @param jsonBuffer The buffer (of type JsonData_t) where file data will be stored.
 * @return true if the file was successfully loaded, false otherwise.
 */
static bool loadJsonFromFile( const char *filename, JsonData_t jsonBuffer )
{
    FILE *fp = fopen( filename, "rb" );
    if ( !fp )
    {
        userPrint( "Error: Could not open JSON file: %s\n", filename );
        return false;
    }
    size_t bytesRead      = fread( jsonBuffer, 1, sizeof( JsonData_t ) - 1, fp );
    jsonBuffer[bytesRead] = '\0';
    fclose( fp );
    return true;
}

/**
 * @brief Prompts the user for a string input.
 * @param[in] prompt The message displayed to the user.
 * @param[out] destination Buffer where the resulting string will be stored.
 * @param[in] maxLen Maximum length of the destination buffer.
 * @param[in] defaultValue Default string value if the user provides no input.
 */
static void promptString( const char *prompt, char *destination, size_t maxLen, const char *defaultValue )
{
    char buffer[MAX_INPUT_SIZE] = { 0 };
    userPrint( "%s", prompt );
    readLine( buffer, sizeof( buffer ) );
    if ( strlen( buffer ) > 0 )
    {
        strncpy( destination, buffer, maxLen );
        destination[maxLen - 1] = '\0';
    }
    else
    {
        strncpy( destination, defaultValue, maxLen );
        destination[maxLen - 1] = '\0';
    }
}

/**
 * @brief Prompts the user for an integer input.
 * @param[in] prompt The message displayed to the user.
 * @param[in] defaultValue The default integer value if the user provides no input.
 * @return The integer entered by the user or the default value.
 */
static int promptInt( const char *prompt, int defaultValue )
{
    char buffer[MAX_INPUT_SIZE] = { 0 };
    userPrint( "%s", prompt );
    readLine( buffer, sizeof( buffer ) );
    return ( strlen( buffer ) > 0 ) ? atoi( buffer ) : defaultValue;
}

/**
 * @brief Prompts the user for an unsigned short input.
 * @param[in] prompt The message displayed to the user.
 * @param[in] defaultValue The default unsigned short value if the user provides no input.
 * @return The unsigned short entered by the user or the default value.
 */
static uint16_t promptUShort( const char *prompt, uint16_t defaultValue )
{
    char buffer[MAX_INPUT_SIZE] = { 0 };
    userPrint( "%s", prompt );
    readLine( buffer, sizeof( buffer ) );
    return ( strlen( buffer ) > 0 ) ? static_cast<uint16_t>( atoi( buffer ) ) : defaultValue;
}

/**
 * @brief Prompts the user for a 64-bit unsigned integer input.
 * @param[in] prompt The message displayed to the user.
 * @param[in] defaultValue The default value if the user provides no input.
 * @return The 64-bit unsigned integer entered by the user or the default value.
 */
static uint64_t promptUInt64( const char *prompt, uint64_t defaultValue )
{
    char buffer[MAX_INPUT_SIZE] = { 0 };
    userPrint( "%s", prompt );
    readLine( buffer, sizeof( buffer ) );
    return ( strlen( buffer ) > 0 ) ? strtoull( buffer, nullptr, 10 ) : defaultValue;
}

/**
 * @brief Reads a line of text from standard input.
 * @param[out] buffer Pointer to the buffer where the input will be stored.
 * @param[in] bufferSize Maximum number of characters to read.
 */
static void readLine( char *buffer, size_t bufferSize )
{
    if ( fgets( buffer, bufferSize, stdin ) != nullptr )
    {
        buffer[strcspn( buffer, "\n" )] = '\0';
    }
}

/**
 * @brief Custom print function that always writes to the original stdout.
 * @param fmt Format string (printf-style).
 * @param ... Additional arguments.
 */
void userPrint( const char *fmt, ... )
{
    if ( original_stdout_fd == -1 )
    {
        va_list args;
        va_start( args, fmt );
        vprintf( fmt, args );
        va_end( args );
        return;
    }

#ifdef _WIN32
    int current_fd = _dup( _fileno( stdout ) );

    _dup2( original_stdout_fd, _fileno( stdout ) );

    va_list args;
    va_start( args, fmt );
    vprintf( fmt, args );
    va_end( args );

    fflush( stdout );

    _dup2( current_fd, _fileno( stdout ) );
    _close( current_fd );
#else
    int current_fd = dup( STDOUT_FILENO );
    dup2( original_stdout_fd, STDOUT_FILENO );

    va_list args;
    va_start( args, fmt );
    vprintf( fmt, args );
    va_end( args );
    fflush( stdout );

    dup2( current_fd, STDOUT_FILENO );
    close( current_fd );
#endif
}
