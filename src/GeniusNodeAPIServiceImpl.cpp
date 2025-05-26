#include "GeniusNodeAPIServiceImpl.hpp"
#include "account/GeniusNode.hpp"

extern std::shared_ptr<sgns::GeniusNode> GeniusNodeInstance;

grpc::Status GeniusNodeAPIServiceImpl::GetBalance( grpc::ServerContext *, const genius::Empty *,
                                                   genius::BalanceReply *response )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    response->set_balance( GeniusNodeInstance->GetBalance() );
    return grpc::Status::OK;
}

grpc::Status GeniusNodeAPIServiceImpl::GetGNUSPrice( grpc::ServerContext *, const genius::Empty *,
                                                     genius::GNUSPriceReply *response )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    auto result = GeniusNodeInstance->GetGNUSPrice();
    if ( !result.has_value() )
    {
        return grpc::Status( grpc::StatusCode::INTERNAL, result.error().message() );
    }
    response->set_price( result.value() );
    return grpc::Status::OK;
}

grpc::Status GeniusNodeAPIServiceImpl::Transfer( grpc::ServerContext *, const genius::TransferRequest *request,
                                                 genius::TransferReply *response )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    auto result = GeniusNodeInstance->TransferFunds( request->amount(), request->destination() );
    response->set_success( result.has_value() );
    if ( !result.has_value() )
    {
        return grpc::Status( grpc::StatusCode::INTERNAL, result.error().message() );
    }
    return grpc::Status::OK;
}

grpc::Status GeniusNodeAPIServiceImpl::GetAddress( grpc::ServerContext *, const genius::Empty *,
                                                   genius::AddressReply *response )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    auto        address = GeniusNodeInstance->GetAddress();
    std::string addr_str( address.begin(), address.end() );
    response->set_address( addr_str );
    return grpc::Status::OK;
}

grpc::Status GeniusNodeAPIServiceImpl::Shutdown( grpc::ServerContext *, const genius::Empty *, genius::Empty * )
{
    if ( GeniusNodeInstance )
    {
        GeniusNodeInstance.reset();
        std::cout << "[gRPC] GeniusNode shut down." << std::endl;
    }
    return grpc::Status::OK;
}

extern std::shared_ptr<sgns::GeniusNode> GeniusNodeInstance;

grpc::Status GeniusNodeAPIServiceImpl::ToMinions( grpc::ServerContext *, const genius::GeniusTokenValue *request,
                                                  genius::MinionReply *response )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    auto result = GeniusNodeInstance->ParseTokens( request->value() );
    response->set_minions( result.has_value() ? result.value() : 0);
    return grpc::Status::OK;
}

grpc::Status GeniusNodeAPIServiceImpl::ToGenius( grpc::ServerContext *, const genius::MinionRequest *request,
                                                 genius::GeniusTokenValue *response )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    std::string formatted = GeniusNodeInstance->FormatTokens( request->minions() );
    response->set_value( formatted );
    return grpc::Status::OK;
}

grpc::Status GeniusNodeAPIServiceImpl::PayDev( grpc::ServerContext *, const genius::MinionRequest *request,
                                               genius::TransferReply *response )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    auto result = GeniusNodeInstance->PayDev( request->minions() );
    response->set_success( result.has_value() );
    return grpc::Status::OK;
}

grpc::Status GeniusNodeAPIServiceImpl::GetCost( grpc::ServerContext *, const genius::JsonData *request,
                                                genius::MinionReply *response )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    auto cost = GeniusNodeInstance->GetProcessCost( request->json() );
    response->set_minions( cost );
    return grpc::Status::OK;
}

grpc::Status GeniusNodeAPIServiceImpl::Process( grpc::ServerContext *, const genius::JsonData *request,
                                                genius::Empty * )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    auto result = GeniusNodeInstance->ProcessImage( request->json() );
    return result.has_value() ? grpc::Status::OK : grpc::Status( grpc::StatusCode::INTERNAL, result.error().message() );
}

grpc::Status GeniusNodeAPIServiceImpl::Mint( grpc::ServerContext *, const genius::MintRequest *request,
                                             genius::Empty * )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    GeniusNodeInstance->MintTokens( request->amount(), request->transaction_hash(), request->chain_id(),
                                    request->token_id() );
    return grpc::Status::OK;
}

grpc::Status GeniusNodeAPIServiceImpl::GetInTransactions( grpc::ServerContext *, const genius::Empty *,
                                                          genius::TransactionMatrix *response )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    auto txs = GeniusNodeInstance->GetInTransactions();
    for ( const auto &v : txs )
    {
        response->add_transactions( std::string(reinterpret_cast<const char*>(v.data()), v.size()) );
    }
    return grpc::Status::OK;
}

grpc::Status GeniusNodeAPIServiceImpl::GetOutTransactions( grpc::ServerContext *, const genius::Empty *,
                                                           genius::TransactionMatrix *response )
{
    if ( !GeniusNodeInstance )
    {
        return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Precondition failed");
    }
    auto txs = GeniusNodeInstance->GetOutTransactions();
    for ( const auto &v : txs )
    {
        response->add_transactions( std::string(reinterpret_cast<const char*>(v.data()), v.size()) );
    }
    return grpc::Status::OK;
}
