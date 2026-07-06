#include "GeniusSDK.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

int main( int argc, char *argv[] )
{
    if ( argc < 2 )
    {
        std::cerr << "Usage: " << argv[0] << " <base_path>\n"
                  << "  base_path - Base path for node data storage (must contain dev_config.json)\n";
        return 1;
    }

    const char *base_path = argv[1];

    // Load dev_config.json from base_path
    std::ifstream cfg_file( std::string( base_path ) + "dev_config.json" );
    if ( !cfg_file.is_open() )
    {
        std::cerr << "Error: dev_config.json not found at " << base_path << "\n";
        return 1;
    }
    std::stringstream buf;
    buf << cfg_file.rdbuf();
    std::string dev_config = buf.str();
    if ( dev_config.empty() )
    {
        std::cerr << "Error: dev_config.json is empty\n";
        return 1;
    }

    GeniusSDKInit( base_path, dev_config.c_str() );

    while ( true )
    {
    }
    return 0;
}
