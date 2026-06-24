#include "GeniusSDK.h"

#include <cstdlib>
#include <iostream>

int main( int argc, char *argv[] )
{
    if ( argc < 2 )
    {
        std::cerr << "Usage: " << argv[0] << " <base_path> [autodht] [process] [baseport] [is_full_node]\n"
                  << "  base_path    - Base path for node data storage (required)\n"
                  << "  autodht      - Auto-discover DHT peers (1/0, true/false, default: true)\n"
                  << "  process      - Enable processing (1/0, true/false, default: true)\n"
                  << "  baseport     - Base network port (uint16_t, default: 40001)\n"
                  << "  is_full_node - Run as full node (1/0, true/false, default: false)\n";
        return 1;
    }

    auto parse_bool = []( const char *arg, bool default_val ) -> bool
    {
        if ( !arg )
        {
            return default_val;
        }
        return arg[0] == '1' || arg[0] == 't' || arg[0] == 'T';
    };

    const char    *base_path    = argv[1];
    const bool     autodht      = ( argc > 2 ) ? parse_bool( argv[2], true ) : true;
    const bool     process      = ( argc > 3 ) ? parse_bool( argv[3], true ) : true;
    const uint16_t baseport     = ( argc > 4 ) ? static_cast<uint16_t>( std::atoi( argv[4] ) ) : 40001;
    const bool     is_full_node = ( argc > 5 ) ? parse_bool( argv[5], false ) : false;

    GeniusSDKInit( base_path, autodht, process, baseport, is_full_node );

    while ( true )
    {
    }
}
