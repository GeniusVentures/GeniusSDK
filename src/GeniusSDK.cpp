/**
 * @file       GeniusSDK.cpp
 * @brief      
 * @date       2024-05-26
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "GeniusSDK.h"
#include "account/GeniusNode.hpp"
#include <memory>

std::shared_ptr<sgns::GeniusNode> GeniusNodeInstance;
DevConfig_st                      DEV_CONFIG{ "0xdeadbeef", 0.7, 1.0, 0, "writable/path/" };

GNUS_VISIBILITY_DEFAULT GNUS_EXPORT void GeniusSDKInit()
{
    GeniusNodeInstance = std::make_shared<sgns::GeniusNode>( DEV_CONFIG );
}

GNUS_VISIBILITY_DEFAULT GNUS_EXPORT void GeniusSDKProcess( const ImagePath_t path, const PayAmount_t amount )
{
    GeniusNodeInstance->ProcessImage( std::string{ path }, amount );
}
