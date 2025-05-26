#include "GeniusNodeAPIServiceImpl.hpp"
#include "GeniusRpcServer.hpp"
#include "account/GeniusNode.hpp"
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/security/server_credentials.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

extern std::shared_ptr<sgns::GeniusNode>            GeniusNodeInstance;
extern std::unique_ptr<genius::GeniusNodeAPI::Stub> RpcClientStub;

namespace genius
{

    static std::unique_ptr<grpc::Server> grpc_server;
    static std::thread                   grpc_thread;
    static std::atomic<bool>             server_running{ false };

    void RunGeniusRpcServer( uint16_t port )
    {
        if ( server_running.exchange( true ) )
        {
            std::cerr << "[gRPC] Server already running." << std::endl;
            return;
        }

        grpc_thread = std::thread(
            [port]()
            {
                std::string              server_address = "127.0.0.1:" + std::to_string( port );
                GeniusNodeAPIServiceImpl service;

                grpc::ServerBuilder builder;
                builder.AddListeningPort( server_address, grpc::InsecureServerCredentials() );
                builder.RegisterService( &service );

                grpc_server = builder.BuildAndStart();
                if ( grpc_server )
                {
                    std::cout << "[gRPC] Server started at " << server_address << std::endl;
                    grpc_server->Wait();
                }
                else
                {
                    std::cerr << "[gRPC] Failed to start server on " << server_address << std::endl;
                }

                server_running = false;
            } );
    }

    void StopGeniusRpcServer()
    {
        if ( !server_running.exchange( false ) )
        {
            return;
        }
        if ( grpc_server )
        {
            grpc_server->Shutdown();
            grpc_server.reset();
        }
        if ( grpc_thread.joinable() )
        {
            grpc_thread.join();
        }
    }

    void GeniusSDKStartRpcServer( uint16_t port )
    {
        if ( GeniusNodeInstance )
        {
            RunGeniusRpcServer( port );
        }
    }

    void GeniusSDKStopRpcServer()
    {
        StopGeniusRpcServer();
    }
}
