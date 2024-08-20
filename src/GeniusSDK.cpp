/**
 * @file       GeniusSDK.cpp
 * @brief      
 * @date       2024-05-26
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "GeniusSDK.h"
#include "account/GeniusNode.hpp"
#include <memory>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <boost/outcome.hpp>
#include <boost/exception/all.hpp>
#include <string>
#include <cstring>
#include <fstream>

class JsonError : public boost::exception
{
public:
    explicit JsonError( std::string msg ) : message( std::move( msg ) )
    {
    }

    const char *what() const noexcept
    {
        return message.c_str();
    }

private:
    std::string message;
};

static outcome::result<DevConfig_st, JsonError> ReadDevConfigFromJSON( const std::string &base_path )
{
    std::ifstream file( base_path + "dev_config.json" );
    if ( !file.is_open() )
    {
        return outcome::failure( JsonError( "Configuration file \"dev_config.json\" not found on " + base_path ) );
    }
    DevConfig_st      config_from_file = {};
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string jsonStr = buffer.str();

    rapidjson::Document    document;
    rapidjson::ParseResult parseResult = document.Parse( jsonStr.c_str() );
    if ( !parseResult )
    {
        return outcome::failure( JsonError( "Parse error " ) );
    }

    if ( !document.HasMember( "Address" ) || !document["Address"].IsString() )
    {
        return outcome::failure( JsonError( "Missing or invalid 'Address'" ) );
    }
    if ( !document.HasMember( "Cut" ) || !document["Cut"].IsDouble() )
    {
        return outcome::failure( JsonError( "Missing or invalid 'Cut'" ) );
    }
    if ( !document.HasMember( "TokenValue" ) || !document["TokenValue"].IsDouble() )
    {
        return outcome::failure( JsonError( "Missing or invalid 'TokenValue'" ) );
    }
    if ( !document.HasMember( "TokenID" ) || !document["TokenID"].IsInt() )
    {
        return outcome::failure( JsonError( "Missing or invalid 'TokenID'" ) );
    }

    strncpy( config_from_file.Addr, document["Address"].GetString(), document["Address"].GetStringLength() );
    config_from_file.Cut              = document["Cut"].GetDouble();
    config_from_file.TokenValueInGNUS = document["TokenValue"].GetDouble();
    config_from_file.TokenID          = document["TokenID"].GetInt();
    strncpy( config_from_file.BaseWritePath, base_path.data(), base_path.size() );

    return outcome::success( config_from_file );
}

std::shared_ptr<sgns::GeniusNode> GeniusNodeInstance;

const char *GeniusSDKInit( const char *base_path )
{
    std::string path = "";
    if ( base_path != nullptr )
    {
        path.assign( base_path );
    }
    auto               load_config_ret = ReadDevConfigFromJSON( path );
    static std::string ret_val         = "Initialized on ";
    if ( load_config_ret )
    {
        GeniusNodeInstance = std::make_shared<sgns::GeniusNode>( load_config_ret.value() );
        ret_val.append( load_config_ret.value().BaseWritePath );
    }
    else
    {
        ret_val.assign( load_config_ret.error().what() );
        std::cout << load_config_ret.error().what() << std::endl;
    }

    return ret_val.c_str();
}

void GeniusSDKProcess( const ImagePath_t path, const PayAmount_t amount )
{
    GeniusNodeInstance->ProcessImage( std::string{ path }, amount );
}
