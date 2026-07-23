/**
 * @file       GeniusSDK.cpp
 * @brief
 * @date       2024-05-26
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "GeniusSDK.h"

#include <account/GeniusAccount.hpp>
#include <algorithm>
#include <blockchain/Blockchain.hpp>
#include <cstdint>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
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
#include <processing/proto/SGProcessing.pb.h>

namespace
{

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

    // Helper function to parse JSON string into GeniusNodeConfig
    outcome::result<GeniusNodeConfig, JsonError> ParseDevConfig( const std::string &jsonStr, const std::string &base_path )
    {
        GeniusNodeConfig       config_from_file = {};
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

        config_from_file.Addr = std::string( document["Address"].GetString(), document["Address"].GetStringLength() );
        config_from_file.Cut  = document["Cut"].GetString();
        config_from_file.TokenValueInGNUS = document["TokenValue"].GetString();
        config_from_file.TokenID          = tidRes.value();
        config_from_file.BaseWritePath    = base_path;

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
    // ponytail: serialize SDK calls with one lock; if this becomes a bottleneck, split node lifetime from read/write API locks.
    std::recursive_mutex GeniusSDKMutex;

    const char *MakeInitReturnValue( const char *base_path )
    {
        thread_local std::string ret_val;
        ret_val.assign( "Initialized on " );
        ret_val.append( base_path );
        return ret_val.c_str();
    }
}

const char *GeniusSDKInit( const char *base_path, const char *dev_config )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !base_path || !dev_config || dev_config[0] == '\0' )
    {
        SPDLOG_ERROR( "base_path and dev_config must not be empty!" );
        return nullptr;
    }
    auto cfg = ParseDevConfig( std::string( dev_config ), std::string( base_path ) );
    if ( !cfg )
    {
        SPDLOG_ERROR( "{}", cfg.error().what() );
        return nullptr;
    }
    GeniusNodeInstance = sgns::GeniusNode::New( cfg.value(), sgns::AccountSource{ sgns::NewAccount{} } );
    if ( !GeniusNodeInstance )
    {
        return nullptr;
    }
    return MakeInitReturnValue( base_path );
}

const char *GeniusSDKInitWithKey( const char *base_path, const char *dev_config, const char *eth_private_key )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !base_path || !dev_config || dev_config[0] == '\0' )
    {
        SPDLOG_ERROR( "base_path and dev_config must not be empty!" );
        return nullptr;
    }
    if ( !eth_private_key || eth_private_key[0] == '\0' )
    {
        SPDLOG_ERROR( "eth_private_key must not be empty!" );
        return nullptr;
    }
    auto cfg = ParseDevConfig( std::string( dev_config ), std::string( base_path ) );
    if ( !cfg )
    {
        SPDLOG_ERROR( "{}", cfg.error().what() );
        return nullptr;
    }
    std::string key_copy( eth_private_key );
    GeniusNodeInstance = sgns::GeniusNode::New( cfg.value(), sgns::AccountSource{ sgns::FromPrivateKey{ key_copy } } );
    if ( !GeniusNodeInstance )
    {
        return nullptr;
    }
    return MakeInitReturnValue( base_path );
}

const char *GeniusSDKInitWithMnemonic( const char *base_path, const char *dev_config, const char *mnemonic )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !base_path || !dev_config || dev_config[0] == '\0' )
    {
        SPDLOG_ERROR( "base_path and dev_config must not be empty!" );
        return nullptr;
    }
    if ( !mnemonic || mnemonic[0] == '\0' )
    {
        SPDLOG_ERROR( "mnemonic must not be empty!" );
        return nullptr;
    }
    auto cfg = ParseDevConfig( std::string( dev_config ), std::string( base_path ) );
    if ( !cfg )
    {
        SPDLOG_ERROR( "{}", cfg.error().what() );
        return nullptr;
    }
    std::string mnemonic_copy( mnemonic );
    GeniusNodeInstance = sgns::GeniusNode::New( cfg.value(),
                                                sgns::AccountSource{ sgns::FromMnemonic{ mnemonic_copy } } );
    if ( !GeniusNodeInstance )
    {
        return nullptr;
    }
    return MakeInitReturnValue( base_path );
}

GeniusNodeReturnValue_t GeniusSDKProcess( const JsonData_t jsondata )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    GeniusNodeReturnValue ret = GENIUS_NODE_ERROR_NOT_INITIALIZED;
    do
    {
        if ( !GeniusNodeInstance )
        {
            break;
        }
        if ( jsondata == nullptr )
        {
            ret = GENIUS_NODE_INVALID_ARGUMENT;
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
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return false;
    }
    if ( jsondata == nullptr )
    {
        return false;
    }
    auto procmgr = sgns::sgprocessing::ProcessingManager::Create( jsondata );

    return procmgr.has_value();
}

double GeniusSDKGetGNUSPrice()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return 0;
    }

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
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return nullptr;
    }

    thread_local std::string version;
    version = GeniusNodeInstance->GetVersion();
    return version.c_str();
}

uint64_t GeniusSDKGetBalance( GeniusTokenID token_id )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return 0;
    }

    return GeniusNodeInstance->GetBalance( sgns::TokenID::FromBytes( token_id.data, sizeof( token_id.data ) ) );
}

GeniusTokenValue GeniusSDKGetBalanceGNUS()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    GeniusTokenValue tv = {};
    if ( !GeniusNodeInstance )
    {
        std::strncpy( tv.value, "0", sizeof( tv.value ) - 1 );
        return tv;
    }

    uint64_t raw = GeniusNodeInstance->GetBalance( sgns::TokenID::FromBytes( { 0x00 } ) );

    auto fmt = GeniusNodeInstance->FormatTokens( raw, sgns::TokenID::FromBytes( { 0x00 } ) );
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
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return nullptr;
    }

    uint64_t balance = GeniusNodeInstance->GetBalance( sgns::TokenID::FromBytes( { 0x00 } ) );

    thread_local char buffer[64];

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
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return { 0, nullptr };
    }

    return matrix_from_vector_of_vector( GeniusNodeInstance->GetOutTransactions() );
}

GeniusMatrix GeniusSDKGetInTransactions()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return { 0, nullptr };
    }

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
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    GeniusNodeReturnValue ret = GENIUS_NODE_ERROR_NOT_INITIALIZED;
    do
    {
        if ( !GeniusNodeInstance )
        {
            break;
        }
        if ( transaction_hash == nullptr || chain_id == nullptr )
        {
            ret = GENIUS_NODE_INVALID_ARGUMENT;
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
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

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
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    GeniusAddress ret = {};

    auto node = GeniusNodeInstance;
    if ( node )
    {
        auto address   = node->GetAddress();
        ret.address[0] = '0';
        ret.address[1] = 'x';
        std::copy( address.cbegin(), address.cend(), &ret.address[2] );
    }

    return ret;
}

GeniusMnemonic GeniusSDKGetMnemonic()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return {};
    }

    auto mnemonic = GeniusNodeInstance->GetMnemonicOfActiveAccount();

    if ( !mnemonic )
    {
        return {};
    }

    GeniusMnemonic ret = {};

    std::copy( mnemonic->cbegin(), mnemonic->cend(), ret.mnemonic );

    return ret;
}

GeniusNodeReturnValue_t GeniusSDKTransfer( uint64_t amount, GeniusAddress *dest, GeniusTokenID token_id )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    GeniusNodeReturnValue ret = GENIUS_NODE_ERROR_NOT_INITIALIZED;
    do
    {
        if ( !GeniusNodeInstance )
        {
            break;
        }
        if ( dest == nullptr )
        {
            ret = GENIUS_NODE_INVALID_ARGUMENT;
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
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

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
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

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
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return 0;
    }
    if ( jsondata == nullptr )
    {
        return 0;
    }

    auto procmgr = sgns::sgprocessing::ProcessingManager::Create( jsondata );
    if ( !procmgr )
    {
        return 0;
    }
    return GeniusNodeInstance->GetProcessCost( procmgr.value() );
}

GeniusTokenValue GeniusSDKGetCostGNUS( const JsonData_t jsondata )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    GeniusTokenValue tv = {};
    if ( !GeniusNodeInstance || jsondata == nullptr )
    {
        std::strncpy( tv.value, "0", sizeof( tv.value ) - 1 );
        return tv;
    }

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
    return tv;
}

GeniusNodeReturnValue_t GeniusSDKShutdown()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    GeniusNodeReturnValue ret = GENIUS_NODE_RET_OK;
    if ( GeniusNodeInstance )
    {
        GeniusNodeInstance.reset(); // Explicitly destroy the shared_ptr
        std::cout << "GeniusSDK: GeniusNodeInstance has been shut down\n";
    }
    return ret;
}

void GeniusSDKLoadLogConfig()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( GeniusNodeInstance )
    {
        GeniusNodeInstance->LoadLogConfig();
    }
}

void GeniusSDKFree( void *ptr )
{
    free( ptr );
}

GeniusStatusInfo GeniusSDKGetInitializationStatus()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return {};
    }

    auto status = GeniusNodeInstance->GetInitializationStatus();

    return { status.first, strdup( status.second.c_str() ) };
}

const char *GeniusSDKGetAvailableAccounts()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    // +1 for separator
    constexpr uint64_t SECTION_SIZE = GENIUS_SDK_ADDRESS_SIZE + 1;

    if ( !GeniusNodeInstance )
    {
        return nullptr;
    }

    auto accounts = GeniusNodeInstance->GetAvailableAccounts();

    char *ret = reinterpret_cast<char *>( malloc( std::max( accounts.size() * SECTION_SIZE, UINT64_C( 1 ) ) ) );

    for ( size_t i = 0; i < accounts.size(); ++i )
    {
        ret[i * SECTION_SIZE]     = '0';
        ret[i * SECTION_SIZE + 1] = 'x';
        memcpy( &ret[i * SECTION_SIZE + 2], accounts[i].data(), GENIUS_SDK_ADDRESS_SIZE - 2 );
        ret[( i + 1 ) * SECTION_SIZE - 1] = '\n';
    }

    if ( accounts.size() > 0 )
    {
        ret[accounts.size() * SECTION_SIZE - 1] = '\0';
    }
    else
    {
        ret[0] = '\0';
    }

    return ret;
}

GeniusNodeReturnValue_t GeniusSDKAddAccountWithPrivateKey( const char *private_key )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return GENIUS_NODE_ERROR_NOT_INITIALIZED;
    }
    if ( private_key == nullptr )
    {
        return GENIUS_NODE_INVALID_ARGUMENT;
    }
    if ( GeniusNodeInstance->AddAccountWithKey( private_key ).has_value() )
    {
        return GENIUS_NODE_RET_OK;
    }
    return GENIUS_NODE_INVALID_ARGUMENT;
}

GeniusNodeReturnValue_t GeniusSDKAddAccountWithMnemonic( const char *mnemonic )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return GENIUS_NODE_ERROR_NOT_INITIALIZED;
    }
    if ( mnemonic == nullptr )
    {
        return GENIUS_NODE_INVALID_ARGUMENT;
    }
    if ( GeniusNodeInstance->AddAccountWithMnemonic( std::string( mnemonic ) ).has_value() )
    {
        return GENIUS_NODE_RET_OK;
    }
    return GENIUS_NODE_INVALID_ARGUMENT;
}

GeniusMnemonicAndStatus GeniusSDKAddAccountWithRandomMnemonic()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return { GENIUS_NODE_ERROR_NOT_INITIALIZED, {} };
    }
    auto reply = GeniusNodeInstance->AddAccountWithRandomMnemonic();
    if ( reply.has_error() )
    {
        return { GENIUS_NODE_ERROR_NOT_INITIALIZED, {} };
    }
    GeniusMnemonicAndStatus ret = { GENIUS_NODE_RET_OK, {} };
    std::copy( reply.value().cbegin(), reply.value().cend(), ret.mnemonic );
    return ret;
}

GeniusNodeReturnValue_t GeniusSDKSelectGeniusAccount( const char *public_address )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return GENIUS_NODE_ERROR_NOT_INITIALIZED;
    }
    if ( public_address == nullptr )
    {
        return GENIUS_NODE_INVALID_ARGUMENT;
    }

    if ( GeniusNodeInstance->SelectAccount( public_address ).has_value() )
    {
        return GENIUS_NODE_RET_OK;
    }
    return GENIUS_NODE_INVALID_ARGUMENT;
}

GeniusNodeReturnValue_t GeniusSDKTransferGeniusAccount( const char *public_address )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return GENIUS_NODE_ERROR_NOT_INITIALIZED;
    }
    if ( public_address == nullptr )
    {
        return GENIUS_NODE_INVALID_ARGUMENT;
    }

    if ( GeniusNodeInstance->TransferAccount( public_address ).has_value() )
    {
        return GENIUS_NODE_RET_OK;
    }
    return GENIUS_NODE_INVALID_ARGUMENT;
}

GeniusNodeReturnValue_t GeniusSDKMergeGeniusAccount( const char *public_address )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return GENIUS_NODE_ERROR_NOT_INITIALIZED;
    }
    if ( public_address == nullptr )
    {
        return GENIUS_NODE_INVALID_ARGUMENT;
    }

    if ( GeniusNodeInstance->MergeAccount( public_address ).has_value() )
    {
        return GENIUS_NODE_RET_OK;
    }
    return GENIUS_NODE_INVALID_ARGUMENT;
}

GeniusNodeReturnValue_t GeniusSDKDeleteAccount( const char *public_address )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return GENIUS_NODE_ERROR_NOT_INITIALIZED;
    }
    if ( public_address == nullptr )
    {
        return GENIUS_NODE_INVALID_ARGUMENT;
    }

    if ( GeniusNodeInstance->DeleteAccount( public_address ).has_value() )
    {
        return GENIUS_NODE_RET_OK;
    }
    return GENIUS_NODE_INVALID_ARGUMENT;
}

GeniusNodeReturnValue_t GeniusSDKSetPayoutAddress( const char *public_address )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return GENIUS_NODE_ERROR_NOT_INITIALIZED;
    }
    if ( public_address == nullptr )
    {
        return GENIUS_NODE_INVALID_ARGUMENT;
    }

    if ( GeniusNodeInstance->SetPayoutAddress( public_address ).has_value() )
    {
        return GENIUS_NODE_RET_OK;
    }
    return GENIUS_NODE_INVALID_ARGUMENT;
}

GeniusTransactionManagerState_t GeniusSDKGetTransactionManagerState()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return GENIUS_TM_STATE_CREATING;
    }
    return static_cast<GeniusTransactionManagerState>( GeniusNodeInstance->GetTransactionManagerState() );
}

GeniusNodeState_t GeniusSDKGetNodeState()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return GENIUS_NODE_CREATING;
    }
    return static_cast<GeniusNodeState>( GeniusNodeInstance->GetState() );
}

GeniusTransactionStatus_t GeniusSDKGetTransactionStatus( const char *tx_id )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance || tx_id == nullptr )
    {
        return GENIUS_TX_STATUS_INVALID;
    }

    auto status = GeniusNodeInstance->GetTransactionStatus( std::string( tx_id ) );

    return static_cast<GeniusTransactionStatus>( static_cast<int>( status ) );
}

GeniusProcessingStatusInfo GeniusSDKGetProcessingStatus()
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

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

const char *GeniusSDKGetMyTaskIds( uint64_t limit, uint64_t offset )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance )
    {
        return nullptr;
    }

    auto task_ids = GeniusNodeInstance->GetMyTaskIds( limit, offset );

    if ( task_ids.empty() )
    {
        char *ret = reinterpret_cast<char *>( malloc( 1 ) );
        ret[0]    = '\0';
        return ret;
    }

    // Calculate total size: each ID + '\n' separator, last entry gets '\0' instead
    size_t total_size = 0;
    for ( const auto &id : task_ids )
    {
        total_size += id.size() + 1; // id + separator
    }

    char *ret  = reinterpret_cast<char *>( malloc( total_size ) );
    char *dest = ret;
    for ( size_t i = 0; i < task_ids.size(); ++i )
    {
        memcpy( dest, task_ids[i].data(), task_ids[i].size() );
        dest    += task_ids[i].size();
        *dest++  = ( i + 1 < task_ids.size() ) ? '\n' : '\0';
    }

    return ret;
}

GeniusArray GeniusSDKGetTaskResult( const char *task_id )
{
    const std::lock_guard<std::recursive_mutex> lock( GeniusSDKMutex );

    if ( !GeniusNodeInstance || task_id == nullptr )
    {
        return { 0, nullptr };
    }

    auto result = GeniusNodeInstance->GetTaskResult( std::string( task_id ) );
    if ( !result.has_value() )
    {
        return { 0, nullptr };
    }

    std::string serialized;
    if ( !result.value().SerializeToString( &serialized ) )
    {
        return { 0, nullptr };
    }

    uint8_t *buf = reinterpret_cast<uint8_t *>( malloc( serialized.size() ) );
    memcpy( buf, serialized.data(), serialized.size() );

    return { serialized.size(), buf };
}
