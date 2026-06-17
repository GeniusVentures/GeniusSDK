/**
 * @file       SDKIdleExample.cpp
 * @brief      
 * @date       2024-07-31
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "GeniusSDK.h"

int main( int argc, char *argv[] )
{
    const char *no_path = "./";

    GeniusSDKInitWithKey( no_path,
                          "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef",
                          true,
                          true,
                          40001,
                          false );

    while ( true )
    {
    }
    return 0;
}
