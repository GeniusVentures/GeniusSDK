/**
 * @file       GeniusRpcServer.hpp
 * @brief      
 * @date       2025-05-26
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */
#pragma once

#include <cstdint>

namespace genius {

/// Starts the gRPC server in a background thread.
/// Should only be called after GeniusNodeInstance is initialized.
void GeniusSDKStartRpcServer(uint16_t port);

/// Gracefully stops the gRPC server.
/// Should be called before shutting down GeniusNodeInstance.
void GeniusSDKStopRpcServer();

}
