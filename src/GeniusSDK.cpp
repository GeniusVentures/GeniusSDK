/**
 * @file       GeniusSDK.cpp
 * @brief      
 * @date       2024-05-26
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "GeniusSDK.h"
#include <memory>

std::shared_ptr<sgns::GeniusNode> GeniusNodeInstance;
AccountKey                        key = "0xfeed";
DevConfig_st                      local_config{ "0xdeadbeef", 0.7f };

GNUS_VISIBILITY_DEFAULT GNUS_EXPORT void GeniusSDKInit()
{
    GeniusNodeInstance = std::make_shared<sgns::GeniusNode>( key, local_config );
}

GNUS_VISIBILITY_DEFAULT GNUS_EXPORT void GeniusSDKProcess( const ImagePath_t path, const PayAmount_t amount )
{
    GeniusNodeInstance->ProcessImage( std::string{ path }, amount );
}
