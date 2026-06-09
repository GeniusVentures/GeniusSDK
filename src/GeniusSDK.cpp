/**
 * @file       GeniusSDK.cpp
 * @brief      
 * @date       2024-05-26
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "GeniusSDK.h"

#include <algorithm>
#include <memory>
#include <string>
#include <cstring>
#include <fstream>

#include "account/GeniusNode.hpp"

#include <boost/algorithm/hex.hpp>
#include <boost/exception/all.hpp>
#include <boost/multiprecision/cpp_int/import_export.hpp>
#include <boost/outcome.hpp>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/writer.h>

#include <base/buffer.hpp>
#include <processing/processing_service.hpp>

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

    // Helper function to parse JSON string into DevConfig_st
    outcome::result<DevConfig_st, JsonError> ParseDevConfig( const std::string &jsonStr, const std::string &base_path )
    {
        DevConfig_st           config_from_file = {};
        rapidjson::Document    document;
        rapidjson::ParseResult parseResult = document.Parse( jsonStr.c_str() );
        if ( parseResult == nullptr )
        {
            return outcome::failure( JsonError( "Parse error" ) );
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

        config_from_file.Addr             = std::string( document["Address"].GetString(), document["Address"].GetStringLength() );
        config_from_file.Cut              = document["Cut"].GetString();
        config_from_file.TokenValueInGNUS = document["TokenValue"].GetString();
        config_from_file.TokenID          = tidRes.value();
        config_from_file.BaseWritePath    = base_path;

        return outcome::success( config_from_file );
    }

    outcome::result<DevConfig_st, JsonError> ReadDevConfigFromJSONStr( const std::string &base_path,
                                                                       const std::string &jsonStr )
    {
        return ParseDevConfig( jsonStr, base_path );
    }

    outcome::result<DevConfig_st, JsonError> ReadDevConfigFromJSON( const std::string &base_path )
    {
        std::ifstream file( base_path + "dev_config.json" );
        if ( !file.is_open() )
        {
            return outcome::failure( JsonError( "Configuration file \"dev_config.json\" not found on " + base_path ) );
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonStr = buffer.str();
        return ParseDevConfig( jsonStr, base_path );
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

    template <typename Creator>
    const char *SDKInitHelper( const char *base_path, Creator create_node )
    {
        static std::string ret_val = "Initialized on ";

        if ( base_path == nullptr )
        {
            std::cerr << "GeniusSDK: base_path should not be empty!\n";
            return nullptr;
        }

        auto load_config_ret = ReadDevConfigFromJSON( base_path );

        if ( !load_config_ret )
        {
            ret_val.assign( load_config_ret.error().what() );
            std::cerr << load_config_ret.error().what() << std::endl;
            return nullptr;
        }

        GeniusNodeInstance = create_node( load_config_ret.value() );

        if ( GeniusNodeInstance == nullptr )
        {
            return nullptr;
        }

        ret_val.append( load_config_ret.value().BaseWritePath );
        return ret_val.c_str();
    }
}

const char *GeniusSDKInit( const char *base_path, bool autodht, bool process, uint16_t baseport, bool is_full_node )
{
    return SDKInitHelper( base_path,
                          [&]( const auto &config )
                          {
                              return std::shared_ptr<sgns::GeniusNode>(
                                  sgns::GeniusNode::New( config, autodht, process, baseport, is_full_node, true ) );
                          } );
}

const char *GeniusSDKInitWithKey( const char *base_path,
                                  const char *eth_private_key,
                                  bool        autodht,
                                  bool        process,
                                  uint16_t    baseport,
                                  bool        is_full_node )
{
    return SDKInitHelper(
        base_path,
        [&]( const auto &config )
        {
            return std::shared_ptr<sgns::GeniusNode>(
                sgns::GeniusNode::New( config, eth_private_key, autodht, process, baseport, is_full_node, true ) );
        } );
}

const char *GeniusSDKInitWithCredentials( const char              *base_path,
                                          const GeniusCredentials *credentials,
                                          bool                     autodht,
                                          bool                     process,
                                          uint16_t                 baseport,
                                          bool                     is_full_node )
{
    return SDKInitHelper( base_path,
                          [&]( const auto &config )
                          {
                              return std::shared_ptr<sgns::GeniusNode>(
                                  sgns::GeniusNode::New( config,
                                                         credentials->password,
                                                         autodht,
                                                         process,
                                                         baseport,
                                                         is_full_node,
                                                         true ) );
                          } );
}

const char *GeniusSDKInitWithKeyAndDevConfig( const char *base_path,
                                              const char *dev_config,
                                              const char *eth_private_key,
                                              bool        autodht,
                                              bool        process,
                                              uint16_t    baseport,
                                              bool        is_full_node )
{
    static std::string ret_val = "Initialized on ";

    if ( base_path == nullptr )
    {
        std::cerr << "base_path should not be empty!\n";
        return nullptr;
    }

    if ( dev_config == nullptr )
    {
        std::cerr << "dev_config should not be empty!\n";
        return nullptr;
    }

    auto load_config_ret = ReadDevConfigFromJSONStr( base_path, dev_config );

    if ( !load_config_ret )
    {
        ret_val.assign( load_config_ret.error().what() );
        std::cerr << load_config_ret.error().what() << std::endl;
        return nullptr;
    }

    GeniusNodeInstance = std::shared_ptr<sgns::GeniusNode>( sgns::GeniusNode::New( load_config_ret.value(),
                                                                                   eth_private_key,
                                                                                   autodht,
                                                                                   process,
                                                                                   baseport,
                                                                                   is_full_node,
                                                                                   true ) );
    ret_val.append( load_config_ret.value().BaseWritePath );

    return ret_val.c_str();
}

const char *GeniusSDKInitMinimal( const char *base_path, const char *eth_private_key, uint16_t baseport )
{
    return GeniusSDKInitWithKey( base_path, eth_private_key, true, true, baseport, false );
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
    auto procmgr = sgns::sgprocessing::ProcessingManager::Create( jsondata );

    return procmgr.has_value();
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
        memset( gnus_id.data, 0, sizeof( gnus_id.data ) );
        ret = static_cast<GeniusNodeReturnValue>(
            GeniusSDKMint( parseRes.value(), transaction_hash, chain_id, gnus_id ) );
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
        if ( amount == nullptr )
        {
            ret = GENIUS_NODE_INVALID_ARGUMENT;
            break;
        }
        if ( dest == nullptr )
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
        memset( gnus_id.data, 0, sizeof( gnus_id.data ) );
        ret = static_cast<GeniusNodeReturnValue>( GeniusSDKTransfer( parseRes.value(), dest, gnus_id ) );
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
    auto procmgr = sgns::sgprocessing::ProcessingManager::Create( jsondata );
    if ( !procmgr )
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
        auto procmgr = sgns::sgprocessing::ProcessingManager::Create( jsondata );
        if ( !procmgr )
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
        std::cout << "GeniusSDK: GeniusNodeInstance has been shut down\n";
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
    if ( !GeniusNodeInstance || tx_id == nullptr )
    {
        return GENIUS_TX_STATUS_INVALID;
    }

    auto status = GeniusNodeInstance->GetTransactionStatus( std::string( tx_id ) );

    return static_cast<GeniusTransactionStatus>( static_cast<int>( status ) );
}

GeniusProcessingStatusInfo GeniusSDKGetProcessingStatus()
{
    using Status = sgns::processing::ProcessingServiceImpl::Status;

    GeniusProcessingStatusInfo result;
    result.percentage = 0.0f;

    if ( !GeniusNodeInstance )
    {
        result.status = GENIUS_PR_STATUS_DISABLED;
        return result;
    }

    auto status_info = GeniusNodeInstance->GetProcessingStatus();

    switch ( status_info.status )
    {
        case Status::DISABLED:
            result.status = GeniusProcessingStatus::GENIUS_PR_STATUS_DISABLED;
            break;
        case Status::PROCESSING:
            result.status = GeniusProcessingStatus::GENIUS_PR_STATUS_PROCESSING;
            break;
        case Status::IDLE:
            result.status = GeniusProcessingStatus::GENIUS_PR_STATUS_IDLE;
            break;
        default:
            result.status = GeniusProcessingStatus::GENIUS_PR_STATUS_DISABLED;
            break;
    }

    result.percentage = status_info.percentage;
    return result;
}
