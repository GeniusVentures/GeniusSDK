#pragma once

#include "proto/genius_rpc.grpc.pb.h"
#include <memory>

class GeniusNodeAPIServiceImpl : public genius::GeniusNodeAPI::Service
{
public:
    grpc::Status GetBalance( grpc::ServerContext *context, const genius::Empty *request,
                             genius::BalanceReply *response ) override;

    grpc::Status GetGNUSPrice( grpc::ServerContext *context, const genius::Empty *request,
                               genius::GNUSPriceReply *response ) override;

    grpc::Status Transfer( grpc::ServerContext *context, const genius::TransferRequest *request,
                           genius::TransferReply *response ) override;

    grpc::Status GetAddress( grpc::ServerContext *context, const genius::Empty *request,
                             genius::AddressReply *response ) override;

    grpc::Status Shutdown( grpc::ServerContext *context, const genius::Empty *request,
                           genius::Empty *response ) override;

    grpc::Status ToMinions( grpc::ServerContext *, const genius::GeniusTokenValue *request,
                            genius::MinionReply *response ) override;

    grpc::Status ToGenius( grpc::ServerContext *, const genius::MinionRequest *request,
                           genius::GeniusTokenValue *response ) override;

    grpc::Status PayDev( grpc::ServerContext *, const genius::MinionRequest *request,
                         genius::TransferReply *response ) override;

    grpc::Status GetCost( grpc::ServerContext *, const genius::JsonData *request,
                          genius::MinionReply *response ) override;

    grpc::Status Process( grpc::ServerContext *, const genius::JsonData *request, genius::Empty * ) override;

    grpc::Status Mint( grpc::ServerContext *, const genius::MintRequest *request, genius::Empty * ) override;

    grpc::Status GetInTransactions( grpc::ServerContext *, const genius::Empty *,
                                    genius::TransactionMatrix *response ) override;

    grpc::Status GetOutTransactions( grpc::ServerContext *, const genius::Empty *,
                                     genius::TransactionMatrix *response ) override;
};
