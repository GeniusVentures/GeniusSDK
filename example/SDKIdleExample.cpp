/**
 * @file       SDKIdleExample.cpp
 * @brief      
 * @date       2024-07-31
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "GeniusSDK.h"

#include <fstream>
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

    GeniusSDKInitWithKey( no_path, dev_config.c_str(),
                          "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef" );

    while ( true )
    {
    }
    return 0;
}
