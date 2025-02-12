/**
 * @file       GeniusSDK.cpp
 * @brief      
 * @date       2024-05-26
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "GeniusSDK.h"

#include "account/GeniusNode.hpp"
#include <algorithm>
#include <base/buffer.hpp>
#include <boost/multiprecision/cpp_int/import_export.hpp>
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

namespace
{
    outcome::result<DevConfig_st, JsonError> ReadDevConfigFromJSON( const std::string &base_path )
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
        if ( parseResult == nullptr )
        {
            return outcome::failure( JsonError( "Parse error " ) );
        }

        if ( !document.HasMember( "Address" ) || !document["Address"].IsString() )
        {
            return outcome::failure( JsonError( "Missing or invalid 'Address'" ) );
        }
        if ( !document.HasMember( "Cut" ) || !document["Cut"].IsString() )
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
        config_from_file.Cut              = document["Cut"].GetString();
        config_from_file.TokenValueInGNUS = document["TokenValue"].GetDouble();
        config_from_file.TokenID          = document["TokenID"].GetInt();
        strncpy( config_from_file.BaseWritePath, base_path.data(), base_path.size() );

        return outcome::success( config_from_file );
    }


    GeniusMatrix matrix_from_vector_of_vector( const std::vector<std::vector<uint8_t>> &vec )
    {
        uint64_t size = vec.size();

        GeniusMatrix matrix = { size, reinterpret_cast<GeniusArray *>( malloc( size * sizeof( GeniusArray ) ) ) };

        for ( uint64_t i = 0; i < size; i++ )
        {
            matrix.ptr[i] = GeniusArray{ vec[i].size(),
                                         reinterpret_cast<uint8_t *>( malloc( vec[i].size() * sizeof( uint8_t ) ) ) };
            memcpy( matrix.ptr[i].ptr, vec[i].data(), vec[i].size() * sizeof( uint8_t ) );
        }

        return matrix;
    }

    GeniusMatrix matrix_from_buffer( const std::vector<sgns::base::Buffer> &vec )
    {
        uint64_t size = vec.size();

        GeniusMatrix matrix = { size, reinterpret_cast<GeniusArray *>( malloc( size * sizeof( GeniusArray ) ) ) };

        for ( uint64_t i = 0; i < size; i++ )
        {
            matrix.ptr[i] = GeniusArray{ vec[i].size(),
                                         reinterpret_cast<uint8_t *>( malloc( vec[i].size() * sizeof( uint8_t ) ) ) };
            memcpy( matrix.ptr[i].ptr, vec[i].data(), vec[i].size() * sizeof( uint8_t ) );
        }

        return matrix;
    }

    std::shared_ptr<sgns::GeniusNode> GeniusNodeInstance;
}

const char *GeniusSDKInit( const char *base_path, const char *eth_private_key, bool autodht, bool process,
                           uint16_t baseport )
{
    if ( base_path == nullptr )
    {
        std::cerr << "base_path should not be empty!" << std::endl;
        return nullptr;
    }

    auto               load_config_ret = ReadDevConfigFromJSON( base_path );
    static std::string ret_val         = "Initialized on ";

    if ( load_config_ret )
    {
        GeniusNodeInstance =
            std::make_shared<sgns::GeniusNode>( load_config_ret.value(), eth_private_key, autodht, process, baseport );
        ret_val.append( load_config_ret.value().BaseWritePath );
    }
    else
    {
        ret_val.assign( load_config_ret.error().what() );
        std::cout << load_config_ret.error().what() << std::endl;
    }

    return ret_val.c_str();
}

void GeniusSDKProcess( const JsonData_t jsondata )
{
    GeniusNodeInstance->ProcessImage( std::string{ jsondata } );
}

uint64_t GeniusSDKGetBalance()
{
    return GeniusNodeInstance->GetBalance();
}

GeniusTokenValue GeniusSDKGetBalanceGNUS()
{
    return GeniusSDKToGenius( GeniusNodeInstance->GetBalance() );
}

GeniusMatrix GeniusSDKGetOutTransactions()
{
    return matrix_from_vector_of_vector( GeniusNodeInstance->GetOutTransactions() );
}

GeniusMatrix GeniusSDKGetInTransactions()
{
    return matrix_from_vector_of_vector( GeniusNodeInstance->GetInTransactions() );
}

void GeniusSDKFreeTransactions( GeniusMatrix matrix )
{
    for ( uint64_t i = 0; i < matrix.size; ++i )
    {
        free( matrix.ptr[i].ptr );
    }
    free( matrix.ptr );
}

void GeniusSDKMint( uint64_t amount, const char *transaction_hash, const char *chain_id, const char *token_id )
{
    GeniusNodeInstance->MintTokens( amount, std::string( transaction_hash ), std::string( chain_id ),
                                    std::string( token_id ) );
}

void GeniusSDKMintGNUS( const GeniusTokenValue *gnus, const char *transaction_hash, const char *chain_id,
                                    const char *token_id )
{
    GeniusSDKMint( GeniusSDKToMinions( gnus ), transaction_hash, chain_id, token_id );
}

GeniusAddress GeniusSDKGetAddress()
{
    auto address = GeniusNodeInstance->GetAddress();

    GeniusAddress ret;

    std::copy( address.cbegin(), address.cend(), ret.address );

    return ret;
}

bool GeniusSDKTransfer( uint64_t amount, GeniusAddress *dest )
{
    std::string destination( dest->address );
    return GeniusNodeInstance->TransferFunds( amount, destination );
}

bool GeniusSDKTransferGNUS( const GeniusTokenValue *gnus, GeniusAddress *dest )
{
    return GeniusSDKTransfer( GeniusSDKToMinions( gnus ), dest );
}

uint64_t GeniusSDKGetCost( const JsonData_t jsondata )
{
    return GeniusNodeInstance->GetProcessCost( jsondata );
}

GeniusTokenValue GeniusSDKGetCostGNUS( const JsonData_t jsondata )
{
    return GeniusSDKToGenius( GeniusNodeInstance->GetProcessCost( jsondata ) );
}

void GeniusSDKShutdown()
{
    if ( GeniusNodeInstance )
    {
        GeniusNodeInstance.reset(); // Explicitly destroy the shared_ptr
        std::cout << "GeniusNodeInstance has been shut down." << std::endl;
    }
}

uint64_t GeniusSDKToMinions( const GeniusTokenValue *gnus )
{
    auto result = GeniusNodeInstance->ParseTokens( std::string( gnus->value ) );
    if ( result.has_value() )
    {
        return result.value();
    }
    return 0;
}

GeniusTokenValue GeniusSDKToGenius( uint64_t minions )
{
    GeniusTokenValue tokenValue;
    std::string      formatted = GeniusNodeInstance->FormatTokens( minions );
    std::strncpy( tokenValue.value, formatted.c_str(), sizeof( tokenValue.value ) - 1 );
    tokenValue.value[sizeof( tokenValue.value ) - 1] = '\0';

    return tokenValue;
}
