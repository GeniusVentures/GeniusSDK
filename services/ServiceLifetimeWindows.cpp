/**
 * @file       ServiceLifetimeWindows.cpp
 * @brief      Windows arm of the service runner lifetime: a console-ctrl handler
 *             captures CTRL+C / close / logoff / shutdown; main parks on the event
 *             until one of them stops the node.
 * @date       2026-10-15
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#include "ServiceLifetime.hpp"

#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>

namespace sgns::service_lifetime
{
    namespace
    {
        HANDLE           g_shutdown_event = nullptr;
        std::atomic<int> g_received_signal{ 0 };

        // Map console-ctrl events onto their POSIX signal equivalents so the runner's
        // shutdown log line keeps one cross-platform contract (SIGINT=2, SIGTERM=15).
        BOOL WINAPI ShutdownHandler( DWORD ctrl_type )
        {
            switch ( ctrl_type )
            {
                case CTRL_C_EVENT:
                case CTRL_BREAK_EVENT:
                    g_received_signal.store( SIGINT );
                    break;
                default:
                    g_received_signal.store( SIGTERM );
                    break;
            }
            SetEvent( g_shutdown_event );
            return TRUE;
        }

    } // namespace

    void ArmShutdownWait()
    {
        g_shutdown_event = CreateEventA( nullptr, TRUE, FALSE, nullptr );
        if ( g_shutdown_event == nullptr || !SetConsoleCtrlHandler( &ShutdownHandler, TRUE ) )
        {
            std::fputs( "Error: could not arm the console shutdown wait\n", stderr );
            std::exit( 1 );
        }
    }

    int WaitForShutdownSignal()
    {
        // Mirrors the POSIX arm's degenerate contract: a failed wait returns 0 and the
        // caller still runs its shutdown path.
        if ( WaitForSingleObject( g_shutdown_event, INFINITE ) != WAIT_OBJECT_0 )
        {
            return 0;
        }
        return g_received_signal.load();
    }
} // namespace sgns::service_lifetime
