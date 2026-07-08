/**
 * @file       SDKIdleExample.cpp
 * @brief      
 * @date       2024-07-31
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "GeniusSDK.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

int main( int argc, char *argv[] )
{
    const char *no_path = "./";

    // Load dev_config.json from the current directory
    std::ifstream cfg_file( std::string( no_path ) + "dev_config.json" );
    if ( !cfg_file.is_open() ) return 1;
    std::stringstream buf;
    buf << cfg_file.rdbuf();
    std::string dev_config = buf.str();
    if ( dev_config.empty() ) return 1;

    const char *init_result = GeniusSDKInitWithKey( no_path, dev_config.c_str(),
                          "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef" );
    if ( !init_result || strncmp( init_result, "Initialized", strlen( "Initialized" ) ) != 0 )
    {
        std::cerr << "Error: GeniusSDK init failed\n";
        return 1;
    }

    while ( true )
    {
    }
    return 0;
}
