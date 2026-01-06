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
#include <boost/algorithm/hex.hpp>
#include <memory>
#include <processing/processing_service.hpp>
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
    explicit JsonError( std::string msg ) : message( std::move( msg ) ) {}

    const char *what() const noexcept
    {
        return message.c_str();
    }

private:
    std::string message;
};

namespace
{
    outcome::result<sgns::TokenID, JsonError> ParseTokenID( const rapidjson::Value &v )
    {
        if ( !v.IsString() )
        {
            return outcome::failure( JsonError( "Invalid TokenID field: expected hex string" ) );
        }

        std::string s = v.GetString();
        std::string hex;

        if ( s.size() >= 2 && ( s.rfind( "0x", 0 ) == 0 || s.rfind( "0X", 0 ) == 0 ) )
        {
            hex = s.substr( 2 );
        }
        else
        {
            hex = s;
        }

        if ( hex.size() != 64 )
        {
            return outcome::failure( JsonError( "Invalid TokenID: must be exactly 64 hex digits" ) );
        }

        std::array<uint8_t, 32> buf;
        try
        {
            boost::algorithm::unhex( hex.begin(), hex.end(), buf.begin() );
        }
        catch ( ... )
        {
            return outcome::failure( JsonError( "TokenID contains invalid hex digits" ) );
        }

        return outcome::success( sgns::TokenID::FromBytes( buf.data(), buf.size() ) );
    }

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
        if ( !document.HasMember( "TokenValue" ) || !document["TokenValue"].IsString() )
        {
            return outcome::failure( JsonError( "Missing or invalid 'TokenValue'" ) );
        }
        if ( !document.HasMember( "TokenID" ) || !document["TokenID"].IsString() )
        {
            return outcome::failure( JsonError( "Missing or invalid 'TokenID'" ) );
        }
        auto tidRes = ParseTokenID( document["TokenID"] );
        if ( !tidRes )
        {
            return outcome::failure( JsonError( std::string( "Failed to parse TokenID: " ) + tidRes.error().what() ) );
        }

        strncpy( config_from_file.Addr, document["Address"].GetString(), document["Address"].GetStringLength() );
        config_from_file.Cut              = document["Cut"].GetString();
        config_from_file.TokenValueInGNUS = document["TokenValue"].GetString();
        config_from_file.TokenID          = tidRes.value();
        strncpy( config_from_file.BaseWritePath, base_path.data(), base_path.size() );

        return outcome::success( config_from_file );
    }

    outcome::result<DevConfig_st, JsonError> ReadDevConfigFromJSONStr( const std::string &base_path,
                                                                       const std::string &jsonStr )
    {
        DevConfig_st           config_from_file = {};
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
        if ( !document.HasMember( "TokenValue" ) || !document["TokenValue"].IsString() )
        {
            return outcome::failure( JsonError( "Missing or invalid 'TokenValue'" ) );
        }
        if ( !document.HasMember( "TokenID" ) || !document["TokenID"].IsString() )
        {
            return outcome::failure( JsonError( "Missing or invalid 'TokenID'" ) );
        }
        auto tidRes = ParseTokenID( document["TokenID"] );
        if ( !tidRes )
        {
            return outcome::failure( JsonError( std::string( "Failed to parse TokenID: " ) + tidRes.error().what() ) );
        }

        strncpy( config_from_file.Addr, document["Address"].GetString(), document["Address"].GetStringLength() );
        config_from_file.Cut              = document["Cut"].GetString();
        config_from_file.TokenValueInGNUS = document["TokenValue"].GetString();
        config_from_file.TokenID          = tidRes.value();
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

const char *GeniusSDKInit( const char *base_path,
                           const char *eth_private_key,
                           bool        autodht,
                           bool        process,
                           uint16_t    baseport,
                           bool        is_full_node )
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
        GeniusNodeInstance = std::shared_ptr<sgns::GeniusNode>( sgns::GeniusNode::New( load_config_ret.value(),
                                                                                       eth_private_key,
                                                                                       autodht,
                                                                                       process,
                                                                                       baseport,
                                                                                       is_full_node,
                                                                                       true ) );
        ret_val.append( load_config_ret.value().BaseWritePath );
    }
    else
    {
        ret_val.assign( load_config_ret.error().what() );
        std::cout << load_config_ret.error().what() << std::endl;
    }

    return ret_val.c_str();
}

const char *GeniusSDKInitSecure( const char *base_path,
                                 const char *dev_config,
                                 const char *eth_private_key,
                                 bool        autodht,
                                 bool        process,
                                 uint16_t    baseport,
                                 bool        is_full_node )
{
    if ( base_path == nullptr )
    {
        std::cerr << "base_path should not be empty!" << std::endl;
        return nullptr;
    }
    if ( dev_config == nullptr )
    {
        std::cerr << "dev_config should not be empty!" << std::endl;
        return nullptr;
    }
    auto               load_config_ret = ReadDevConfigFromJSONStr( base_path, dev_config );
    static std::string ret_val         = "Initialized on ";

    if ( load_config_ret )
    {
        GeniusNodeInstance = std::shared_ptr<sgns::GeniusNode>( sgns::GeniusNode::New( load_config_ret.value(),
                                                                                       eth_private_key,
                                                                                       autodht,
                                                                                       process,
                                                                                       baseport,
                                                                                       is_full_node,
                                                                                       true ) );
        ret_val.append( load_config_ret.value().BaseWritePath );
    }
    else
    {
        ret_val.assign( load_config_ret.error().what() );
        std::cout << load_config_ret.error().what() << std::endl;
    }

    return ret_val.c_str();
}

const char *GeniusSDKInitMinimal( const char *base_path, const char *eth_private_key, uint16_t baseport )
{
    return GeniusSDKInit( base_path, eth_private_key, true, true, baseport, false );
}

GeniusNodeReturnValue_t GeniusSDKProcess( const JsonData_t jsondata )
{
    GeniusNodeReturnValue ret = GENIUS_NODE_ERROR_NOT_INITIALIZED;
    do
    {
        if ( !GeniusNodeInstance )
        {
            break;
        }
        auto result = GeniusNodeInstance->ProcessImage( std::string{ jsondata } );

        if ( !result.has_value() )
        {
            ret = GENIUS_NODE_ERROR_PROCESS_IMAGE;
            std::cerr << "Error processing image: " << result.error() << std::endl;
            break;
        }
        ret = GENIUS_NODE_RET_OK;
    } while ( 0 );

    return ret;
}

bool GeniusSDKCheckJobValidity( const JsonData_t jsondata )
{
    if ( !GeniusNodeInstance )
    {
        return false;
    }
    auto procmgr       = sgns::sgprocessing::ProcessingManager::Create( jsondata );
    if(!procmgr)
    {
        return false;
    }
    return true;
}

double GeniusSDKGetGNUSPrice()
{
    auto result = GeniusNodeInstance->GetGNUSPrice();

    if ( !result.has_value() )
    {
        std::cerr << "Error getting gnus price: " << result.error() << std::endl;
        return 0;
    }
    return result.value();
}

const char *GeniusSDKGetVersion()
{
    static std::string version = GeniusNodeInstance->GetVersion();
    return version.c_str();
}

uint64_t GeniusSDKGetBalance( GeniusTokenID token_id )
{
    return GeniusNodeInstance->GetBalance( sgns::TokenID::FromBytes( token_id.data, sizeof( token_id.data ) ) );
}

GeniusTokenValue GeniusSDKGetBalanceGNUS()
{
    uint64_t raw = GeniusNodeInstance->GetBalance( sgns::TokenID::FromBytes( { 0x00 } ) );

    GeniusTokenValue tv;
    auto             fmt = GeniusNodeInstance->FormatTokens( raw, sgns::TokenID::FromBytes( { 0x00 } ) );
    if ( fmt.has_value() )
    {
        std::strncpy( tv.value, fmt.value().c_str(), sizeof( tv.value ) - 1 );
    }
    else
    {
        std::strncpy( tv.value, "0", sizeof( tv.value ) - 1 );
    }
    tv.value[sizeof( tv.value ) - 1] = '\0';
    return tv;
}

const char *GeniusSDKGetBalanceGNUSString()
{
    uint64_t balance = GeniusNodeInstance->GetBalance( sgns::TokenID::FromBytes( { 0x00 } ) );

    // Use a static buffer to store the string (not thread-safe but should work for your needs)
    static char buffer[64];

    auto fmtRes = GeniusNodeInstance->FormatTokens( balance, sgns::TokenID::FromBytes( { 0x00 } ) );

    std::string formatted;
    if ( fmtRes.has_value() )
    {
        formatted = fmtRes.value();
    }
    else
    {
        formatted = std::string{};
    }

    std::strncpy( buffer, formatted.c_str(), sizeof( buffer ) - 1 );
    buffer[sizeof( buffer ) - 1] = '\0';
    return buffer;
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

GeniusNodeReturnValue_t GeniusSDKMint( uint64_t      amount,
                                     const char   *transaction_hash,
                                     const char   *chain_id,
                                     GeniusTokenID token_id )
{
    GeniusNodeReturnValue ret = GENIUS_NODE_ERROR_NOT_INITIALIZED;
    do
    {
        if ( !GeniusNodeInstance )
        {
            break;
        }
        auto result = GeniusNodeInstance->MintTokens(
            amount,
            std::string( transaction_hash ),
            std::string( chain_id ),
            sgns::TokenID::FromBytes( token_id.data, sizeof( token_id.data ) ) );

        if ( !result.has_value() )
        {
            ret = GENIUS_NODE_ERROR_MINT;
            std::cerr << "Error minting tokens: " << result.error() << std::endl;
            break;
        }
        ret = GENIUS_NODE_RET_OK;
    } while ( 0 );

    return ret;
}

GeniusNodeReturnValue_t GeniusSDKMintGNUS( const GeniusTokenValue *amount,
                                         const char             *transaction_hash,
                                         const char             *chain_id )
{
    GeniusNodeReturnValue ret = GENIUS_NODE_ERROR_NOT_INITIALIZED;
    do
    {
        if ( !GeniusNodeInstance )
        {
            break;
        }
        auto parseRes = GeniusNodeInstance->ParseTokens( std::string( amount->value ),
                                                         sgns::TokenID::FromBytes( { 0x00 } ) );
        if ( !parseRes.has_value() )
        {
            ret = GENIUS_NODE_INVALID_ARGUMENT;
            std::cerr << "Error minting tokens, invalid argument: " << parseRes.error() << std::endl;
            break;
        }
        GeniusTokenID gnus_id;
        memset(gnus_id.data, 0, sizeof(gnus_id.data));
        ret = static_cast<GeniusNodeReturnValue>(GeniusSDKMint( parseRes.value(),
                             transaction_hash ,
                             chain_id ,
                             gnus_id ));
    } while ( 0 );

    return ret;
}

GeniusAddress GeniusSDKGetAddress()
{
    GeniusAddress ret;
    if ( GeniusNodeInstance )
    {
        auto address = GeniusNodeInstance->GetAddress();

        std::copy( address.cbegin(), address.cend(), ret.address );
    }

    return ret;
}

GeniusNodeReturnValue_t GeniusSDKTransfer( uint64_t amount, GeniusAddress *dest, GeniusTokenID token_id )
{
    GeniusNodeReturnValue ret = GENIUS_NODE_ERROR_NOT_INITIALIZED;
    do
    {
        if ( !GeniusNodeInstance )
        {
            break;
        }
        std::string destination( dest->address );
        auto        result = GeniusNodeInstance->TransferFunds(
            amount,
            destination,
            sgns::TokenID::FromBytes( token_id.data, sizeof( token_id.data ) ) );
        if ( !result.has_value() )
        {
            ret = GENIUS_NODE_ERROR_TRANSFER;
            break;
        }
        ret = GENIUS_NODE_RET_OK;
    } while ( 0 );

    return ret;
}

GeniusNodeReturnValue_t GeniusSDKTransferGNUS( const GeniusTokenValue *amount, GeniusAddress *dest )
{
    GeniusNodeReturnValue ret = GENIUS_NODE_ERROR_NOT_INITIALIZED;
    do
    {
        if ( !GeniusNodeInstance )
        {
            break;
        }
        if ( !amount )
        {
            ret = GENIUS_NODE_INVALID_ARGUMENT;
            break;
        }
        if ( !dest )
        {
            ret = GENIUS_NODE_INVALID_ARGUMENT;
            break;
        }
        auto parseRes = GeniusNodeInstance->ParseTokens( std::string( amount->value ),
                                                         sgns::TokenID::FromBytes( { 0x00 } ) );
        if ( !parseRes.has_value() )
        {
            ret = GENIUS_NODE_INVALID_ARGUMENT;
            break;
        }
        GeniusTokenID gnus_id;
        memset(gnus_id.data, 0, sizeof(gnus_id.data));
        ret = static_cast<GeniusNodeReturnValue>(GeniusSDKTransfer( parseRes.value(), dest, gnus_id ));
    } while ( 0 );

    return ret;
}

GeniusNodeReturnValue_t GeniusSDKPayDev( uint64_t amount, GeniusTokenID token_id )
{
    GeniusNodeReturnValue ret = GENIUS_NODE_ERROR_NOT_INITIALIZED;
    do
    {
        if ( !GeniusNodeInstance )
        {
            break;
        }
        auto result = GeniusNodeInstance->PayDev( amount,
                                                  sgns::TokenID::FromBytes( token_id.data, sizeof( token_id.data ) ) );
        if ( !result.has_value() )
        {
            ret = GENIUS_NODE_ERROR_PAY_DEV;
            break;
        }
        ret = GENIUS_NODE_RET_OK;
    } while ( 0 );

    return ret;
}

uint64_t GeniusSDKGetCost( const JsonData_t jsondata )
{
    auto procmgr       = sgns::sgprocessing::ProcessingManager::Create( jsondata );
    if(!procmgr)
    {
        return 0;
    }
    return GeniusNodeInstance->GetProcessCost( procmgr.value() );
}

GeniusTokenValue GeniusSDKGetCostGNUS( const JsonData_t jsondata )
{
    GeniusTokenValue tv;
    if ( GeniusNodeInstance )
    {
        auto procmgr       = sgns::sgprocessing::ProcessingManager::Create( jsondata );
        if(!procmgr)
        {
            std::strncpy( tv.value, "0", sizeof( tv.value ) - 1 );
            tv.value[sizeof( tv.value ) - 1] = '\0';
            return tv;
        }
        uint64_t rawCost = GeniusNodeInstance->GetProcessCost( procmgr.value() );

        auto fmt = GeniusNodeInstance->FormatTokens( rawCost, sgns::TokenID::FromBytes( { 0x00 } ) );
        if ( fmt.has_value() )
        {
            std::strncpy( tv.value, fmt.value().c_str(), sizeof( tv.value ) - 1 );
        }
        else
        {
            std::strncpy( tv.value, "0", sizeof( tv.value ) - 1 );
        }
        tv.value[sizeof( tv.value ) - 1] = '\0';
    }
    return tv;
}

GeniusNodeReturnValue_t GeniusSDKShutdown()
{
    GeniusNodeReturnValue ret = GENIUS_NODE_RET_OK;
    if ( GeniusNodeInstance )
    {
        GeniusNodeInstance.reset(); // Explicitly destroy the shared_ptr
        std::cout << "GeniusNodeInstance has been shut down." << std::endl;
    }
    return ret;
}

GeniusTransactionManagerState_t GeniusSDKGetTransactionManagerState()
{
    if ( !GeniusNodeInstance )
    {
        return GENIUS_TM_STATE_CREATING;
    }
    return static_cast<GeniusTransactionManagerState>( GeniusNodeInstance->GetTransactionManagerState() );
}

GeniusNodeState_t GeniusSDKGetNodeState()
{
    if ( !GeniusNodeInstance )
    {
        return GENIUS_NODE_CREATING;
    }
    return static_cast<GeniusNodeState>( GeniusNodeInstance->GetState() );
}

GeniusTransactionStatus_t GeniusSDKGetTransactionStatus( const char *tx_id )
{
    if ( !GeniusNodeInstance || !tx_id )
    {
        return GENIUS_TX_STATUS_INVALID;
    }

    auto status = GeniusNodeInstance->GetTransactionStatus( std::string( tx_id ) );

    return static_cast<GeniusTransactionStatus>( static_cast<int>( status ) );
}

GeniusProcessingStatus_t GeniusSDKGetProcessingStatus()
{
    using Status = sgns::processing::ProcessingServiceImpl::Status;

    if ( !GeniusNodeInstance )
    {
        return GENIUS_PR_STATUS_DISABLED;
    }

    auto status = GeniusNodeInstance->GetProcessingStatus();

    switch ( status )
    {
        case Status::DISABLED:
            return GeniusProcessingStatus::GENIUS_PR_STATUS_DISABLED;
        case Status::PROCESSING:
            return GeniusProcessingStatus::GENIUS_PR_STATUS_PROCESSING;
        case Status::IDLE:
            return GeniusProcessingStatus::GENIUS_PR_STATUS_IDLE;
        default:
            return GeniusProcessingStatus::GENIUS_PR_STATUS_DISABLED;
    }
}
