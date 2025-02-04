/**
 * @file       SDKIdleExample.cpp
 * @brief      
 * @date       2024-07-31
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "GeniusSDK.h"

static void displayMenu();
static void getBalance();
static void getAddress();
static void getInTransactions();
static void getOutTransactions();
static void processSampleData();
static void getProcessingCost();
static void mintTokens();
static void transferTokens();
static void shutdownSDK();

static struct MenuOption
{
    int32_t     option;
    const char *description;
    void ( *function )();
} options[] = { { 1, "Get Balance", getBalance },
                { 2, "Get Address", getAddress },
                { 3, "Get Incoming Transactions", getInTransactions },
                { 4, "Get Outgoing Transactions", getOutTransactions },
                { 5, "Process Sample Data", processSampleData },
                { 6, "Get Processing Cost", getProcessingCost },
                { 7, "Mint Tokens", mintTokens },
                { 8, "Transfer Tokens", transferTokens },
                { 9, "Shutdown SDK", shutdownSDK },
                { 0, "Exit", NULL } };

static JsonData_t sampleData = R"(
                {
                "data": {
                    "type": "file",
                    "URL": "file://[basepath]../../../../test/src/processing_nodes/"
                },
                "model": {
                    "name": "mnnimage",
                    "file": "model.mnn"
                },
                "input": [
                    {
                        "image": "data/ballet.data",
                        "block_len": 4860000 ,
                        "block_line_stride": 5400,
                        "block_stride": 0,
                        "chunk_line_stride": 1080,
                        "chunk_offset": 0,
                        "chunk_stride": 4320,
                        "chunk_subchunk_height": 5,
                        "chunk_subchunk_width": 5,
                        "chunk_count": 25,
                        "channels": 4
                    },
                    {
                        "image": "data/frisbee3.data",
                        "block_len": 786432 ,
                        "block_line_stride": 1536,
                        "block_stride": 0,
                        "chunk_line_stride": 384,
                        "chunk_offset": 0,
                        "chunk_stride": 1152,
                        "chunk_subchunk_height": 4,
                        "chunk_subchunk_width": 4,
                        "chunk_count": 16,
                        "channels": 3
                    }
                ]
                }
               )";

int main()
{
    const char *base_path       = "./";
    const char *eth_private_key = "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
    int32_t     autodht         = 1;
    int32_t     process         = 1;
    uint16_t    baseport        = 40001;

    // Initialize SDK
    const char *init_result = GeniusSDKInit( base_path, eth_private_key, autodht, process, baseport );
    if ( !init_result )
    {
        printf( "Failed to initialize GeniusSDK.\n" );
        return -1;
    }
    printf( "GeniusSDK initialized successfully.\n" );

    int32_t choice;
    do
    {
        displayMenu();
        printf( "Enter choice: " );
        scanf( "%d", &choice );

        for ( int32_t i = 0; options[i].option != 0; i++ )
        {
            if ( options[i].option == choice )
            {
                if ( options[i].function )
                {
                    options[i].function();
                }
                else
                {
                    printf( "Exiting...\n" );
                }
                break;
            }
        }
    } while ( choice != 0 );

    return 0;
}

static void displayMenu()
{
    printf( "\nSelect an option:\n" );
    for ( int32_t i = 0; options[i].option != 0; i++ )
    {
        printf( "%d - %s\n", options[i].option, options[i].description );
    }
}

static void getBalance()
{
    uint64_t balance = GeniusSDKGetBalance();
    printf( "Current Balance: %llu\n", balance );
}

static void getAddress()
{
    GeniusAddress address = GeniusSDKGetAddress();
    printf( "Wallet Address: %s\n", address.address );
}

static void getInTransactions()
{
    GeniusMatrix inTransactions = GeniusSDKGetInTransactions();
    printf( "Incoming Transactions: %llu\n", inTransactions.size );
    GeniusSDKFreeTransactions( inTransactions );
}

static void getOutTransactions()
{
    GeniusMatrix outTransactions = GeniusSDKGetOutTransactions();
    printf( "Outgoing Transactions: %llu\n", outTransactions.size );
    GeniusSDKFreeTransactions( outTransactions );
}

static void processSampleData()
{
    GeniusSDKProcess( sampleData );
    printf( "Processed sample JSON data.\n" );
}

static void getProcessingCost()
{
    uint64_t cost = GeniusSDKGetCost( sampleData );
    printf( "Estimated processing cost: %llu\n", cost );
}

static void mintTokens()
{
    const char *amount           = "1000";
    const char *transaction_hash = "";
    const char *chain_id         = "";
    const char *token_id         = "";
    GeniusSDKMintTokens( amount, transaction_hash, chain_id, token_id );
    printf( "Minted tokens successfully.\n" );
}

static void transferTokens()
{
    GeniusAddress recipient;
    strncpy( recipient.address, "0xrecipient123456789", sizeof( recipient.address ) );
    int32_t transferSuccess = GeniusSDKTransferTokens( 500, &recipient );
    printf( "Token transfer %s.\n", transferSuccess ? "successful" : "failed" );
}

static void shutdownSDK()
{
    GeniusSDKShutdown();
    printf( "GeniusSDK shut down successfully.\n" );
}
