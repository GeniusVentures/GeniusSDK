/**
 * @file       SDKIdleExample.cpp
 * @brief      
 * @date       2024-07-31
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

 #include <math.h>
#include <fstream>
#include <memory>
#include <iostream>
#include <cstdlib>
#include <cstdint>

#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/asio.hpp>
#include "local_secure_storage/impl/json/JSONSecureStorage.hpp"
#include "GeniusSDK.h"


int main( int argc, char *argv[] )
{
    const char* no_path = "./";
    GeniusSDKInit( no_path, "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef" );

    while ( true )
    {
    }
    return 0;
}
