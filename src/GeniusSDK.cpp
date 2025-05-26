/**
 * @file       GeniusSDK.cpp
 * @brief      
 * @date       2024-05-26
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "GeniusSDK.h"
#include "GeniusRpcServer.hpp"
#include "proto/genius_rpc.pb.h"

#include "proto/genius_rpc.grpc.pb.h"

#include "account/GeniusNode.hpp"
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/client_context.h>
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

std::unique_ptr<genius::GeniusNodeAPI::Stub> RpcClientStub;
std::shared_ptr<sgns::GeniusNode>            GeniusNodeInstance;
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
    bool                                         is_server = false;


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

    void InitRpcClient( uint16_t port )
    {
        auto channel = grpc::CreateChannel( "127.0.0.1:" + std::to_string( port ), grpc::InsecureChannelCredentials() );
        RpcClientStub = genius::GeniusNodeAPI::NewStub( channel );
    }

    bool TryStartAsServer( const std::string &base_path, const std::string &dev_config, const std::string &eth_key,
                           bool autodht, bool process, uint16_t baseport )
    {
        auto config = ReadDevConfigFromJSONStr( base_path, dev_config );
        if ( !config )
        {
            return false;
        }
        GeniusNodeInstance =
            std::make_shared<sgns::GeniusNode>( config.value(), eth_key.c_str(), autodht, process, baseport );
        genius::GeniusSDKStartRpcServer( baseport );
        is_server = true;
        return true;
    }

    std::string TryInitClient( uint16_t baseport )
    {
        try
        {
            InitRpcClient( baseport );
            grpc::ClientContext  ctx;
            genius::Empty        req;
            genius::BalanceReply reply;
            auto                 status = RpcClientStub->GetBalance( &ctx, req, &reply );
            if ( !status.ok() )
            {
                throw std::runtime_error( "Connection failed" );
            }
            return "Connected to GeniusNode RPC server";
        }
        catch ( ... )
        {
            return "Failed to connect to GeniusNode server";
        }
    }

    uint64_t RpcGetBalance()
    {
        grpc::ClientContext  ctx;
        genius::Empty        req;
        genius::BalanceReply reply;
        auto                 status = RpcClientStub->GetBalance( &ctx, req, &reply );
        if ( status.ok() )
        {
            return reply.balance();
        }
        std::cerr << "[gRPC] GetBalance failed: " << status.error_message() << std::endl;
        return 0;
    }

    double RpcGetGNUSPrice()
    {
        grpc::ClientContext    ctx;
        genius::Empty          req;
        genius::GNUSPriceReply reply;
        auto                   status = RpcClientStub->GetGNUSPrice( &ctx, req, &reply );
        if ( status.ok() )
        {
            return reply.price();
        }
        std::cerr << "[gRPC] GetGNUSPrice failed: " << status.error_message() << std::endl;
        return 0.0;
    }

    bool RpcTransfer( uint64_t amount, const std::string &dest )
    {
        grpc::ClientContext     ctx;
        genius::TransferRequest req;
        genius::TransferReply   reply;
        req.set_amount( amount );
        req.set_destination( dest );
        auto status = RpcClientStub->Transfer( &ctx, req, &reply );
        return status.ok() && reply.success();
    }

    std::string RpcGetAddress()
    {
        grpc::ClientContext  ctx;
        genius::Empty        req;
        genius::AddressReply reply;
        auto                 status = RpcClientStub->GetAddress( &ctx, req, &reply );
        return status.ok() ? reply.address() : "";
    }

    void RpcShutdown()
    {
        grpc::ClientContext ctx;
        genius::Empty       req, resp;
        RpcClientStub->Shutdown( &ctx, req, &resp );
    }

    uint64_t RpcToMinions( const GeniusTokenValue *gnus )
    {
        grpc::ClientContext      ctx;
        genius::MinionReply      reply;
        genius::GeniusTokenValue pb_token;
        pb_token.set_value( gnus->value );
        auto status = RpcClientStub->ToMinions( &ctx, pb_token, &reply );
        return status.ok() ? reply.minions() : 0;
    }

    GeniusTokenValue RpcToGenius( uint64_t minions )
    {
        grpc::ClientContext   ctx;
        genius::MinionRequest req;
        req.set_minions( minions );
        genius::GeniusTokenValue pb_reply;
        RpcClientStub->ToGenius( &ctx, req, &pb_reply );
        GeniusTokenValue reply{};
        std::strncpy( reply.value, pb_reply.value().c_str(), sizeof( reply.value ) - 1 );

        return reply;
    }

    bool RpcPayDev( uint64_t amount )
    {
        grpc::ClientContext   ctx;
        genius::MinionRequest req;
        req.set_minions( amount );
        genius::TransferReply reply;
        auto                  status = RpcClientStub->PayDev( &ctx, req, &reply );
        return status.ok() && reply.success();
    }

    void RpcProcess( const JsonData_t jsondata )
    {
        grpc::ClientContext ctx;
        genius::JsonData    req;
        req.set_json( jsondata );
        genius::Empty dummy;
        RpcClientStub->Process( &ctx, req, &dummy );
    }

    void RpcMint( uint64_t amount, const char *tx_hash, const char *chain_id, const char *token_id )
    {
        grpc::ClientContext ctx;
        genius::MintRequest req;
        req.set_amount( amount );
        req.set_transaction_hash( tx_hash );
        req.set_chain_id( chain_id );
        req.set_token_id( token_id );
        genius::Empty dummy;
        RpcClientStub->Mint( &ctx, req, &dummy );
    }

    uint64_t RpcGetCost( const JsonData_t jsondata )
    {
        grpc::ClientContext ctx;
        genius::JsonData    req;
        req.set_json( jsondata );
        genius::MinionReply reply;
        auto                status = RpcClientStub->GetCost( &ctx, req, &reply );
        return status.ok() ? reply.minions() : 0;
    }

    GeniusMatrix RpcGetTransactions( bool inbound )
    {
        grpc::ClientContext       ctx;
        genius::Empty             req;
        genius::TransactionMatrix reply;
        auto                      status = inbound ? RpcClientStub->GetInTransactions( &ctx, req, &reply )
                                                   : RpcClientStub->GetOutTransactions( &ctx, req, &reply );
        if ( !status.ok() )
        {
            return { 0, nullptr };
        }

        auto         count  = reply.transactions_size();
        GeniusMatrix matrix = { static_cast<uint64_t>( count ), new GeniusArray[count] };
        for ( int i = 0; i < count; ++i )
        {
            const std::string &entry = reply.transactions( i );
            matrix.ptr[i].size       = entry.size();
            matrix.ptr[i].ptr        = new uint8_t[entry.size()];
            std::memcpy( matrix.ptr[i].ptr, entry.data(), entry.size() );
        }
        return matrix;
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

}

const char *GeniusSDKInitSecure( const char *base_path, const char *dev_config, const char *eth_private_key,
                                 bool autodht, bool process, uint16_t baseport )
{
    if ( !base_path || !eth_private_key || !dev_config )
    {
        std::cerr << "Invalid parameters!" << std::endl;
        return nullptr;
    }

    std::string path( base_path );
    std::string key( eth_private_key );
    std::string cfg( dev_config );

    static std::string ret_val;

    if ( TryStartAsServer( path, cfg, key, autodht, process, baseport ) )
    {
        ret_val = "Initialized server on " + path;
        return ret_val.c_str();
    }

    ret_val = TryInitClient( baseport );
    return ret_val.c_str();
}

void GeniusSDKProcess( const JsonData_t jsondata )
{
    if ( is_server )
    {
        auto result = GeniusNodeInstance->ProcessImage( std::string{ jsondata } );
        if ( !result.has_value() )
        {
            std::cerr << "Error processing image: " << result.error() << std::endl;
        }
    }
    else
    {
        RpcProcess( jsondata );
    }
}

double GeniusSDKGetGNUSPrice()
{
    if ( is_server )
    {
        auto result = GeniusNodeInstance->GetGNUSPrice();
        return result.has_value() ? result.value() : 0.0;
    }
    return RpcGetGNUSPrice();
}

uint64_t GeniusSDKGetBalance()
{
    if ( is_server )
    {
        return GeniusNodeInstance->GetBalance();
    }
    return RpcGetBalance();
}

GeniusTokenValue GeniusSDKGetBalanceGNUS()
{
    return GeniusSDKToGenius( GeniusNodeInstance->GetBalance() );
}

const char *GeniusSDKGetBalanceGNUSString()
{
    uint64_t balance = GeniusSDKGetBalance();

    // Use a static buffer to store the string (not thread-safe but should work for your needs)
    static char buffer[64];

    std::string formatted = GeniusNodeInstance->FormatTokens( balance );
    strncpy( buffer, formatted.c_str(), sizeof( buffer ) - 1 );
    buffer[sizeof( buffer ) - 1] = '\0';

    return buffer;
}

GeniusMatrix GeniusSDKGetOutTransactions()
{
    return is_server ? matrix_from_vector_of_vector( GeniusNodeInstance->GetOutTransactions() )
                     : RpcGetTransactions( true );
}

GeniusMatrix GeniusSDKGetInTransactions()
{
    return is_server ? matrix_from_vector_of_vector( GeniusNodeInstance->GetInTransactions() )
                     : RpcGetTransactions( false );
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
    if ( is_server )
    {
        GeniusNodeInstance->MintTokens( amount, std::string( transaction_hash ), std::string( chain_id ),
                                        std::string( token_id ) );
    }
    else
    {
        RpcMint( amount, transaction_hash, chain_id, token_id );
    }
    auto result = GeniusNodeInstance->MintTokens( amount, std::string( transaction_hash ), std::string( chain_id ),
                                                  std::string( token_id ) );
}

void GeniusSDKMintGNUS( const GeniusTokenValue *gnus, const char *transaction_hash, const char *chain_id,
                        const char *token_id )
{
    GeniusSDKMint( GeniusSDKToMinions( gnus ), transaction_hash, chain_id, token_id );
}

GeniusAddress GeniusSDKGetAddress()
{
    GeniusAddress ret{};
    if ( is_server )
    {
        auto addr = GeniusNodeInstance->GetAddress();
        std::copy( addr.begin(), addr.end(), ret.address );
    }
    else
    {
        auto str = RpcGetAddress();
        std::strncpy( ret.address, str.c_str(), sizeof( ret.address ) - 1 );
        ret.address[sizeof( ret.address ) - 1] = '\0';
    }
    return ret;
}

bool GeniusSDKTransfer( uint64_t amount, GeniusAddress *dest )
{
    std::string destination( dest->address );
    if ( is_server )
    {
        auto result = GeniusNodeInstance->TransferFunds( amount, destination );
        return result.has_value();
    }
    return RpcTransfer( amount, destination );
}

bool GeniusSDKTransferGNUS( const GeniusTokenValue *gnus, GeniusAddress *dest )
{
    return GeniusSDKTransfer( GeniusSDKToMinions( gnus ), dest );
}

bool GeniusSDKPayDev( uint64_t amount )
{
    return is_server ? GeniusNodeInstance->PayDev( amount ).has_value() : RpcPayDev( amount );
}

uint64_t GeniusSDKGetCost( const JsonData_t jsondata )
{
    return is_server ? GeniusNodeInstance->GetProcessCost( jsondata ) : RpcGetCost( jsondata );
}

GeniusTokenValue GeniusSDKGetCostGNUS( const JsonData_t jsondata )
{
    return GeniusSDKToGenius( GeniusNodeInstance->GetProcessCost( jsondata ) );
}

void GeniusSDKShutdown()
{
    if ( is_server )
    {
        GeniusNodeInstance.reset();
        genius::GeniusSDKStopRpcServer();
        std::cout << "GeniusNode Server has been shut down." << std::endl;
    }
    else
    {
        RpcShutdown();
        std::cout << "GeniusNode Client has been shut down." << std::endl;
    }
}

uint64_t GeniusSDKToMinions( const GeniusTokenValue *gnus )
{
    if ( is_server )
    {
        auto result = GeniusNodeInstance->ParseTokens( std::string( gnus->value ) );
        return result.has_value() ? result.value() : 0;
    }
    else
    {
        return RpcToMinions( gnus );
    }
}

GeniusTokenValue GeniusSDKToGenius( uint64_t minions )
{
    if ( is_server )
    {
        GeniusTokenValue tokenValue;
        std::string      formatted = GeniusNodeInstance->FormatTokens( minions );
        std::strncpy( tokenValue.value, formatted.c_str(), sizeof( tokenValue.value ) - 1 );
        tokenValue.value[sizeof( tokenValue.value ) - 1] = '\0';
        return tokenValue;
    }
    return RpcToGenius( minions );
}
