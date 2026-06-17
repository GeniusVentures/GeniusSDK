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
#include <inttypes.h>
#include "GeniusSDK.h"

static const int MAX_INPUT_SIZE = 256; ///< Maximum size for user input buffers.

static void initSDK();

static void ExampleGetAddress();
static void ExampleGetBalance();
static void ExampleGetBalanceGNUS();
static void ExampleGetBalanceGNUSString();
static void ExampleGetInTransactions();
static void ExampleGetOutTransactions();
static void processSampleData();
static void getProcessingCost();
static void getProcessingCostInString();
static void ExampleMint();
static void ExampleMintGNUS();
static void ExampleTransfer();
static void ExampleTransferGNUS();
static void shutdownSDK();

#if ( SUPPRESS_OUTPUT == 1 )
static void suppressSDKLogs();
#endif
static void     getSDKConfig( char     *base_path,
                              char     *eth_private_key,
                              int32_t  *autodht,
                              int32_t  *process,
                              uint16_t *baseport );
static bool     loadJsonFromFile( const char *filename, JsonData_t jsonBuffer );
static bool     parseGeniusTokenID( const char *hex, GeniusTokenID *out );
static void     promptString( const char *prompt, char *destination, size_t maxLen, const char *defaultValue );
static int      promptInt( const char *prompt, int defaultValue );
static uint16_t promptUShort( const char *prompt, uint16_t defaultValue );
static uint64_t promptUInt64( const char *prompt, uint64_t defaultValue );
static void     readLine( char *buffer, size_t bufferSize );

#define SUPPRESS_OUTPUT 0
#if ( SUPPRESS_OUTPUT == 0 )
#define userPrint printf
#else
static int original_stdout_fd = -1; ///< To preserve original stdout when suppressing SDK logs.
void       userPrint( const char *fmt, ... );
#endif

typedef void ( *MenuFunc )();

struct MenuOption
{
    int         option;
    const char *description;
    MenuFunc    function; // nullptr means “Back” or “Exit”
};

// Generic submenu runner
static void runSubMenu( const char *title, const MenuOption *menu )
{
    while ( true )
    {
        userPrint( "\n--- %s ---\n", title );
        for ( const MenuOption *opt = menu; opt->description; ++opt )
        {
            userPrint( "%2d - %s\n", opt->option, opt->description );
        }
        userPrint( " 0 - Back\n" );

        int  choice  = promptInt( "Enter choice: ", -1 );
        bool handled = false;
        for ( const MenuOption *opt = menu; opt->description; ++opt )
        {
            if ( opt->option == choice )
            {
                handled = true;
                opt->function(); // call routine
                return;          // back to main menu
            }
        }
        if ( choice == 0 )
        {
            return; // Back
        }
        if ( !handled )
        {
            userPrint( "Invalid choice. Please select again.\n" );
        }
    }
}

int main()
{
#if ( SUPPRESS_OUTPUT == 1 )
    suppressSDKLogs();
#endif

    static const MenuOption sdkMenu[] = {
        { 1, "Initialize SDK", initSDK },
        { 2, "Shutdown SDK", shutdownSDK },
        { 0, nullptr, nullptr },
    };
    static const MenuOption walletMenu[] = {
        { 1, "GetAddress", ExampleGetAddress },
        { 2, "GetBalance", ExampleGetBalance },
        { 3, "GetBalanceGNUS", ExampleGetBalanceGNUS },
        { 4, "GetBalanceGNUSString", ExampleGetBalanceGNUSString },
        { 0, nullptr, nullptr },
    };
    static const MenuOption txMenu[] = {
        { 1, "GetInTransactions", ExampleGetInTransactions },
        { 2, "GetOutTransactions", ExampleGetOutTransactions },
        { 3, "Mint", ExampleMint },
        { 4, "MintGNUS", ExampleMintGNUS },
        { 5, "Transfer", ExampleTransfer },
        { 6, "TransferGNUS", ExampleTransferGNUS },
        { 0, nullptr, nullptr },
    };
    static const MenuOption procMenu[] = {
        { 1, "Process Sample JSON", processSampleData },
        { 2, "Get Cost (raw)", getProcessingCost },
        { 3, "Get Cost (formatted)", getProcessingCostInString },
        { 0, nullptr, nullptr },
    };

    while ( true )
    {
        struct MenuOption
        {
            int         option;
            const char *description;
            void ( *function )();
        };

        static const MenuOption mainMenu[] = { { 1, "SDK", []() { runSubMenu( "SDK", sdkMenu ); } },
                                               { 2, "Wallet", []() { runSubMenu( "Wallet", walletMenu ); } },
                                               { 3, "Transactions", []() { runSubMenu( "Transactions", txMenu ); } },
                                               { 4, "Processing", []() { runSubMenu( "Processing", procMenu ); } },
                                               { 0, "Exit", nullptr } };

        userPrint( "\n=== Main Menu ===\n" );
        for ( const auto &opt : mainMenu )
        {
            userPrint( "%2d - %s\n", opt.option, opt.description );
        }

        int  choice  = promptInt( "Enter choice: ", -1 );
        bool handled = false;

        for ( const auto &opt : mainMenu )
        {
            if ( opt.option == choice )
            {
                handled = true;
                if ( opt.function )
                {
                    opt.function();
                }
                else
                {
                    userPrint( "Exiting application.\n" );
                    return 0;
                }
                break;
            }
        }

        if ( !handled )
        {
            userPrint( "Invalid choice. Please select again.\n" );
        }
    }

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

    const char *init_result = GeniusSDKInitWithKey( base_path, eth_private_key, autodht, process, baseport, false );
    if ( !init_result || strncmp( init_result, "Initialized", strlen( "Initialized" ) ) != 0 )
    {
        userPrint( "Failed to initialize GeniusSDK. Error: %s\n", init_result ? init_result : "No response" );
        return;
    }

    userPrint( "GeniusSDK initialized successfully.\n" );
}

/**
 * @brief Retrieves and prints the wallet address.
 */
static void ExampleGetAddress()
{
    GeniusAddress address = GeniusSDKGetAddress();
    userPrint( "Wallet Address: %s\n", address.address );
}

static void ExampleGetBalance()
{
    char tokenId[MAX_INPUT_SIZE] = { 0 };
    promptString( "Enter child-token ID (hex, or press enter for default): ", tokenId, MAX_INPUT_SIZE, "" );

    GeniusTokenID tid = {};
    if ( strlen( tokenId ) > 0 )
    {
        if ( !parseGeniusTokenID( tokenId, &tid ) )
        {
            printf( "Invalid token ID\n" );
            return;
        }
    }

    uint64_t balance = GeniusSDKGetBalance( tid );
    printf( "Balance (minions): %llu\n", (unsigned long long)balance );
}

static void ExampleGetBalanceGNUS()
{
    GeniusTokenValue balance = GeniusSDKGetBalanceGNUS();
    printf( "Balance (GNUS value): %s\n", balance.value );
}

static void ExampleGetBalanceGNUSString()
{
    const char *balanceStr = GeniusSDKGetBalanceGNUSString();
    printf( "Balance (GNUS string): %s\n", balanceStr );
}

/**
 * @brief Mints tokens by prompting the user for an amount.
 */
static void ExampleMint()
{
    uint64_t amount = promptUInt64( "Enter the amount to mint: ", 0 );

    char txHash[MAX_INPUT_SIZE]  = "";
    char chainId[MAX_INPUT_SIZE] = "";
    char tokenId[MAX_INPUT_SIZE] = "";
    promptString( "Transaction hash (optional): ", txHash, MAX_INPUT_SIZE, "" );
    promptString( "Chain ID (optional): ", chainId, MAX_INPUT_SIZE, "" );
    promptString( "Token ID: ", tokenId, MAX_INPUT_SIZE, "" );

    GeniusTokenID tid;
    if ( !parseGeniusTokenID( tokenId, &tid ) )
    {
        userPrint( "Invalid token ID\n" );
        return;
    }

    GeniusSDKMint( amount, txHash, chainId, tid );
    userPrint( "Minted %llu tokens of “%s” successfully.\n",
               (unsigned long long)amount,
               tokenId[0] ? tokenId : "<default>" );
}

/**
 * @brief Mints tokens using the string-based function.
 */
static void ExampleMintGNUS()
{
    GeniusTokenValue amount;
    userPrint( "Enter amount to mint: " );
    scanf( "%s", amount.value );

    char txHash[MAX_INPUT_SIZE]  = "";
    char chainId[MAX_INPUT_SIZE] = "";
    promptString( "Transaction hash: ", txHash, MAX_INPUT_SIZE, "" );
    promptString( "Chain ID: ", chainId, MAX_INPUT_SIZE, "" );

    GeniusSDKMintGNUS( &amount, txHash, chainId );
    userPrint( "Minted %s.\n", amount.value );
}

/**
 * @brief Transfers tokens by prompting the user for an amount and a recipient address.
 */
static void ExampleTransfer()
{
    uint64_t      amount                  = promptUInt64( "Enter the amount of Minion Tokens to transfer: ", 0 );
    char          tokenId[MAX_INPUT_SIZE] = "";
    GeniusAddress recipient;
    GeniusTokenID tid;

    promptString( "Enter Token ID (leave empty for default): ", tokenId, MAX_INPUT_SIZE, "" );

    userPrint( "Enter recipient wallet address: " );
    readLine( recipient.address, sizeof( recipient.address ) );

    if ( !parseGeniusTokenID( tokenId, &tid ) )
    {
        userPrint( "Invalid token ID\n" );
        return;
    }

    auto transferSuccess = GeniusSDKTransfer( amount, &recipient, tid );
    userPrint( "Token transfer %s.\n", transferSuccess == GENIUS_NODE_RET_OK ? "successful" : "failed" );
}

/**
 * @brief Transfers tokens using the string-based function.
 */
static void ExampleTransferGNUS()
{
    GeniusTokenValue amount;
    GeniusAddress    recipient;

    userPrint( "Enter amount to transfer: " );
    scanf( "%s", amount.value );
    userPrint( "Enter recipient address: " );
    scanf( "%s", recipient.address );

    GeniusSDKTransferGNUS( &amount, &recipient );
    userPrint( "Transferred %s tokens to %s.\n", amount.value, recipient.address );
}

/**
 * @brief Gets processing cost as a string.
 */
static void getProcessingCostInString()
{
    JsonData_t       jsonData = "sample.json";
    GeniusTokenValue cost     = GeniusSDKGetCostGNUS( jsonData );
    userPrint( "Processing cost: %s\n", cost.value );
}

/**
 * @brief Retrieves and prints the number of incoming transactions.
 */
static void ExampleGetInTransactions()
{
    GeniusMatrix inTransactions = GeniusSDKGetInTransactions();
    userPrint( "Incoming Transactions: %" PRIu64 "\n", inTransactions.size );
    GeniusSDKFreeTransactions( inTransactions );
}

/**
 * @brief Retrieves and prints the number of outgoing transactions.
 */
static void ExampleGetOutTransactions()
{
    GeniusMatrix outTransactions = GeniusSDKGetOutTransactions();
    userPrint( "Outgoing Transactions: %llu\n", static_cast<unsigned long long>( outTransactions.size ) );
    GeniusSDKFreeTransactions( outTransactions );
}

/**
 * @brief Processes a sample JSON data file using the GeniusSDK.
 */
static void processSampleData()
{
    char jsonFilePath[MAX_INPUT_SIZE] = "sample.json";
    promptString( "Enter JSON file path for sample data (Press Enter for default: sample.json): ",
                  jsonFilePath,
                  MAX_INPUT_SIZE,
                  "sample.json" );

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
    promptString( "Enter JSON file path for sample data (Press Enter for default: sample.json): ",
                  jsonFilePath,
                  MAX_INPUT_SIZE,
                  "sample.json" );

    JsonData_t localSampleData;
    if ( !loadJsonFromFile( jsonFilePath, localSampleData ) )
    {
        userPrint( "Failed to load JSON file.\n" );
        return;
    }
    uint64_t cost = GeniusSDKGetCost( localSampleData );
    userPrint( "Estimated processing cost: %llu\n", static_cast<unsigned long long>( cost ) );
}

/**
 * @brief Shuts down the GeniusSDK.
 */
static void shutdownSDK()
{
    GeniusSDKShutdown();
    userPrint( "GeniusSDK shut down successfully.\n" );
}

#if ( SUPPRESS_OUTPUT == 1 )
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
#endif

/**
 * @brief Retrieves configuration values for initializing the GeniusSDK.
 * @param[out] base_path Buffer to store the base path.
 * @param[out] eth_private_key Buffer to store the Ethereum private key.
 * @param[out] autodht Pointer to store the AutoDHT flag.
 * @param[out] process Pointer to store the processing flag.
 * @param[out] baseport Pointer to store the base port number.
 */
static void getSDKConfig( char     *base_path,
                          char     *eth_private_key,
                          int32_t  *autodht,
                          int32_t  *process,
                          uint16_t *baseport )
{
    userPrint( "\nConfigure GeniusSDK Initialization:\n" );

    promptString( "Enter base path (Press Enter for default: ./): ", base_path, MAX_INPUT_SIZE, "./" );
    promptString( "Enter Ethereum private key (Press Enter for default): ",
                  eth_private_key,
                  MAX_INPUT_SIZE,
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
 * @brief Parse a hex string (with optional "0x" prefix) into a 32-byte TokenID.
 * @param hex   Null-terminated hex string (upper/lower case). May be shorter or longer than 64 digits.
 * @param out   Pointer to GeniusTokenID to fill.
 * @return      true on success (out is filled), false on invalid hex.
 */
static bool parseGeniusTokenID( const char *hex, GeniusTokenID *out )
{
    size_t i;

    if ( out == NULL )
    {
        return false;
    }

    memset( out->data, 0, sizeof( out->data ) );

    if ( hex == NULL || *hex == '\0' )
    {
        return true;
    }

    if ( hex[0] == '0' && ( hex[1] == 'x' || hex[1] == 'X' ) )
    {
        hex += 2;
    }

    size_t      len = strlen( hex );
    const char *s   = hex;
    if ( len > 64 )
    {
        s   = hex + ( len - 64 );
        len = 64;
    }

    char   tmp[65];
    size_t pad = 64 - len;
    for ( i = 0; i < pad; ++i )
    {
        tmp[i] = '0';
    }
    memcpy( tmp + pad, s, len );
    tmp[64] = '\0';

    for ( i = 0; i < 32; ++i )
    {
        char c1 = tmp[2 * i];
        char c2 = tmp[2 * i + 1];
        int  hi, lo;

        if ( c1 >= '0' && c1 <= '9' )
        {
            hi = c1 - '0';
        }
        else if ( c1 >= 'a' && c1 <= 'f' )
        {
            hi = c1 - 'a' + 10;
        }
        else if ( c1 >= 'A' && c1 <= 'F' )
        {
            hi = c1 - 'A' + 10;
        }
        else
        {
            return false;
        }

        if ( c2 >= '0' && c2 <= '9' )
        {
            lo = c2 - '0';
        }
        else if ( c2 >= 'a' && c2 <= 'f' )
        {
            lo = c2 - 'a' + 10;
        }
        else if ( c2 >= 'A' && c2 <= 'F' )
        {
            lo = c2 - 'A' + 10;
        }
        else
        {
            return false;
        }

        out->data[i] = (uint8_t)( ( hi << 4 ) | lo );
    }

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

#if ( SUPPRESS_OUTPUT == 1 )
/**
 * @brief Custom print function that always writes to the original stdout.
 * @param fmt Format string (printf-style).
 * @param ... Additional arguments.
 */
void userPrint( const char *fmt, ... )
{
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
#endif
