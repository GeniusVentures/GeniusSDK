#include "GeniusSDK.h"

int main( int argc, char *argv[] )
{
    const char *no_path = "./";

    const GeniusCredentials credentials = { "kenneth.hurley@gnus.ai", "123456789" };

    GeniusSDKInitWithCredentials( no_path, &credentials, true, true, 40001, false );

    while ( true )
    {
    }
    return 0;
}
