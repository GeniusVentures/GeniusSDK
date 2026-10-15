/**
 * @file       ServiceLifetimePosix.cpp
 * @brief      POSIX arm of the service runner lifetime: block SIGTERM/SIGINT before
 *             the SDK boots, then park in sigwait until one of them stops the node.
 * @date       2026-10-15
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "ServiceLifetime.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

namespace sgns::service_lifetime
{
    namespace
    {
        // One home for the captured set - sigset_t is a scalar on Darwin, so the set is
        // filled through an out-parameter instead of returned by value.
        void FillShutdownSignalSet( sigset_t &shutdown_signals )
        {
            sigemptyset( &shutdown_signals );
            sigaddset( &shutdown_signals, SIGTERM );
            sigaddset( &shutdown_signals, SIGINT );
        }

    } // namespace

    void ArmShutdownWait()
    {
        sigset_t shutdown_signals{};
        FillShutdownSignalSet( shutdown_signals );
        if ( pthread_sigmask( SIG_BLOCK, &shutdown_signals, nullptr ) != 0 )
        {
            static_cast<void>( std::fputs( "Error: could not arm the signal shutdown wait\n", stderr ) );
            std::exit( 1 ); // NOLINT(concurrency-mt-unsafe): boot-time fail-fast, no thread exists yet
        }
    }

    int WaitForShutdownSignal()
    {
        sigset_t shutdown_signals{};
        int      received_signal = 0;
        FillShutdownSignalSet( shutdown_signals );
        sigwait( &shutdown_signals, &received_signal );
        return received_signal;
    }
} // namespace sgns::service_lifetime
