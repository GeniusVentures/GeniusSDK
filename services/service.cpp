/**
 * @file       service.cpp
 * @brief      GeniusSDKService runner: boots one node through the public SDK C API
 *             (random account, or an identity read from a policy-checked key file),
 *             emits a flushed STATUS line every 10 s, and parks in sigwait until one
 *             termination signal cleanly stops the node.
 * @date       2026-10-06
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "GeniusSDK.h"

#include <pthread.h>
#include <openssl/crypto.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include "trustedpeer/genesis_tool/GenesisCeremony.hpp"
#include "trustedpeer/genesis_tool/GenesisCeremonyPlatform.hpp"

namespace
{
    using sgns::trustedpeer::GenesisCeremony;
    namespace genesis_platform = sgns::trustedpeer::genesis_ceremony_platform;

    // Indices are GeniusNode::NodeState values - keep in sync with the NodeState enum in
    // account/GeniusNode.hpp (fixed order, exactly 10 entries).
    constexpr std::array<const char *, 10> NODE_STATE_NAMES{
        "CREATING",
        "MIGRATING_DATABASE",
        "INITIALIZING_DATABASE",
        "INITIALIZING_BLOCKCHAIN",
        "INITIALIZING_TRANSACTIONS",
        "WAITING_FOR_TRUST_GENESIS",
        "WAITING_FOR_BURN_GENESIS",
        "FATAL_TRUST_MISMATCH",
        "INITIALIZING_PROCESSING",
        "READY",
    };

    constexpr auto STATUS_INTERVAL = std::chrono::seconds( 10 );

    void PrintUsage( std::ostream &out, const char *program )
    {
        out << "Usage: " << program
            << " <base_path> [--key-file <path>]\n"
               "  base_path  - base path for node data storage; MUST end in '/' (dev_config.json\n"
               "               is looked up by direct concatenation, no separator is added)\n"
               "  --key-file - optional node identity: a regular, non-symlink, user-owned 0600\n"
               "               file holding exactly 64 hex characters. Without it the node\n"
               "               boots with a random account.\n";
    }

    struct Arguments
    {
        std::string base_path;
        std::string key_file; ///< Empty = boot with a random account.
    };

    std::optional<Arguments> ParseArguments( int argc, char *argv[], std::ostream &errors )
    {
        if ( argc < 2 || std::string( argv[1] ).rfind( "--", 0 ) == 0 )
        {
            errors << "Error: missing <base_path>\n";
            PrintUsage( errors, argv[0] );
            return std::nullopt;
        }
        Arguments parsed;
        parsed.base_path = argv[1];
        for ( int i = 2; i < argc; ++i )
        {
            const std::string option = argv[i];
            if ( option != "--key-file" )
            {
                errors << "Error: unknown option: " << option << "\n";
                PrintUsage( errors, argv[0] );
                return std::nullopt;
            }
            if ( !parsed.key_file.empty() )
            {
                errors << "Error: duplicate option: " << option << "\n";
                return std::nullopt;
            }
            if ( i + 1 >= argc || std::string( argv[i + 1] ).rfind( "--", 0 ) == 0 )
            {
                errors << "Error: missing value for option: " << option << "\n";
                return std::nullopt;
            }
            parsed.key_file = argv[++i];
        }
        return parsed;
    }

    bool LoadDevConfig( const std::string &base_path, std::string &dev_config, std::ostream &errors )
    {
        // Direct concatenation on purpose - base_path must already end in '/'.
        std::ifstream cfg_file( base_path + "dev_config.json" );
        if ( !cfg_file.is_open() )
        {
            errors << "Error: dev_config.json not found at " << base_path << "\n";
            return false;
        }
        std::stringstream buf;
        buf << cfg_file.rdbuf();
        dev_config = buf.str();
        if ( dev_config.empty() )
        {
            errors << "Error: dev_config.json is empty\n";
            return false;
        }
        return true;
    }

    // Display names for the key-file subset of GenesisCeremony::Error - the only values
    // KeyFileStatusProblem returns. The human-readable prose lives with the outcome
    // category in GenesisCeremony.cpp; only the typed value name is repeated here.
    const char *KeyFileErrorName( GenesisCeremony::Error error_value )
    {
        using Error = GenesisCeremony::Error;
        switch ( error_value )
        {
            case Error::KEY_FILE_SYMLINK:
                return "KEY_FILE_SYMLINK";
            case Error::KEY_FILE_NOT_REGULAR:
                return "KEY_FILE_NOT_REGULAR";
            case Error::KEY_FILE_OWNER:
                return "KEY_FILE_OWNER";
            case Error::KEY_FILE_MODE:
                return "KEY_FILE_MODE";
            case Error::KEY_FILE_IO:
            default:
                return "KEY_FILE_IO";
        }
    }

    std::optional<std::string> ReadIdentityKey( const std::string &key_file_path, std::ostream &errors )
    {
        // Key-file policy is owned by the trustedpeer domain - reuse its checks, never
        // re-implement stat/mode/owner rules here.
        const auto problem = GenesisCeremony::KeyFileStatusProblem( genesis_platform::InspectKeyFile( key_file_path ) );
        if ( problem )
        {
            errors << "Error: key file rejected (" << KeyFileErrorName( *problem ) << "): " << key_file_path << "\n";
            return std::nullopt;
        }
        auto key = genesis_platform::ReadKeyFile( key_file_path );
        if ( key.has_error() )
        {
            errors << "Error: key file could not be read (KEY_FILE_IO): " << key_file_path << "\n";
            return std::nullopt;
        }
        // Edge validation only (GenesisCeremony's IsPrivateKeyHex is file-local): a usable
        // identity is exactly 64 hex characters. The key itself is never printed.
        const std::string private_key        = std::move( key.value() );
        const bool        is_private_key_hex = private_key.size() == 64 &&
                                        std::all_of( private_key.begin(),
                                                     private_key.end(),
                                                     []( unsigned char c ) { return std::isxdigit( c ) != 0; } );
        if ( !is_private_key_hex )
        {
            errors << "Error: key file must hold exactly 64 hex characters: " << key_file_path << "\n";
            return std::nullopt;
        }
        return private_key;
    }

    // One parseable progress line on stdout, force-flushed - the harness reads this
    // through a pipe. Node state and init progress come via the public C API only.
    void PrintStatusLine()
    {
        const GeniusNodeState_t node_state = GeniusSDKGetNodeState();
        std::string             state_name;
        if ( node_state >= 0 && static_cast<size_t>( node_state ) < NODE_STATE_NAMES.size() )
        {
            state_name = NODE_STATE_NAMES[static_cast<size_t>( node_state )];
        }
        else
        {
            state_name = "UNKNOWN(" + std::to_string( node_state ) + ")";
        }
        const GeniusStatusInfo status = GeniusSDKGetInitializationStatus();
        std::cout << "STATUS node_state=" << state_name << " init=" << std::fixed << std::setprecision( 2 )
                  << status.percentage << std::endl;
        if ( status.message != nullptr )
        {
            GeniusSDKFree( status.message ); // Contract: free the status message when non-null.
        }
    }
} // namespace

int main( int argc, char *argv[] )
{
    // Block termination signals BEFORE any SDK call so every node thread inherits the
    // mask; main then parks in sigwait (no busy-wait) and one signal cleanly stops the node.
    sigset_t shutdown_signals;
    sigemptyset( &shutdown_signals );
    sigaddset( &shutdown_signals, SIGTERM );
    sigaddset( &shutdown_signals, SIGINT );
    pthread_sigmask( SIG_BLOCK, &shutdown_signals, nullptr );

    const auto arguments = ParseArguments( argc, argv, std::cerr );
    if ( !arguments )
    {
        return 1;
    }

    std::string dev_config;
    if ( !LoadDevConfig( arguments->base_path, dev_config, std::cerr ) )
    {
        return 1;
    }

    std::string identity_key;
    if ( !arguments->key_file.empty() )
    {
        auto key = ReadIdentityKey( arguments->key_file, std::cerr );
        if ( !key )
        {
            return 1;
        }
        identity_key = std::move( *key );
    }

    const char *init_result = nullptr;
    if ( identity_key.empty() )
    {
        init_result = GeniusSDKInit( arguments->base_path.c_str(), dev_config.c_str() );
    }
    else
    {
        init_result = GeniusSDKInitWithKey( arguments->base_path.c_str(), dev_config.c_str(), identity_key.c_str() );
        // The secret must never outlive its use - scrub the local copy (GenesisCeremony
        // cleanse discipline) before parking in sigwait.
        OPENSSL_cleanse( identity_key.data(), identity_key.size() );
        identity_key.clear();
    }
    if ( !init_result || std::strncmp( init_result, "Initialized", std::strlen( "Initialized" ) ) != 0 )
    {
        std::cerr << "Error: GeniusSDK initialization failed: " << ( init_result ? init_result : "No response" )
                  << "\n";
        return 1;
    }

    // Staying resident is part of the contract: exiting destroys the process's
    // pubsub/GraphSync transports. The sigwait below is this runner's equivalent of the
    // genesis tool's ServeBeforeExit window.
    std::atomic<bool>       stop_status_printer{ false };
    std::mutex              wakeup_mutex;
    std::condition_variable wakeup;
    std::thread             status_printer(
        [&stop_status_printer, &wakeup_mutex, &wakeup]
        {
            for ( ;; )
            {
                PrintStatusLine();
                std::unique_lock<std::mutex> lock( wakeup_mutex );
                wakeup.wait_for( lock, STATUS_INTERVAL, [&stop_status_printer] { return stop_status_printer.load(); } );
                if ( stop_status_printer.load() )
                {
                    return;
                }
            }
        } );

    int received_signal = 0;
    sigwait( &shutdown_signals, &received_signal );

    {
        const std::lock_guard<std::mutex> lock( wakeup_mutex );
        stop_status_printer.store( true );
    }
    wakeup.notify_all();
    status_printer.join();

    std::cout << "received signal " << received_signal << ", shutting down" << std::endl;
    GeniusSDKShutdown();
    return 0;
}
