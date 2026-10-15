/**
 * @file       ServiceLifetime.hpp
 * @brief      Platform contract for the service runner's shutdown wait: arm the
 *             termination capture before the SDK boots, then park the main thread
 *             (no busy-wait) until one termination event cleanly stops the node.
 * @date       2026-10-15
 * @author     Henrique A. Klein (hklein@gnus.ai)
 */

#ifndef SGNS_SERVICES_SERVICE_LIFETIME_HPP
#define SGNS_SERVICES_SERVICE_LIFETIME_HPP

namespace sgns::service_lifetime
{
    /**
     * @brief  Arm the shutdown wait BEFORE any SDK call.
     *
     * POSIX blocks SIGTERM/SIGINT in the calling thread so every node thread created
     * afterwards inherits the mask; Windows registers the console-ctrl handler that
     * captures CTRL+C / close / logoff / shutdown. Fail-fast: terminates the process
     * if the platform mechanism cannot be armed.
     */
    void ArmShutdownWait();

    /**
     * @brief  Park the calling thread until one termination event.
     * @return The POSIX signal number of the received termination (SIGINT=2,
     *         SIGTERM=15) on both platforms; 0 if the platform wait itself failed
     *         (the caller still runs its shutdown path, mirroring sigwait).
     */
    int WaitForShutdownSignal();
} // namespace sgns::service_lifetime

#endif // SGNS_SERVICES_SERVICE_LIFETIME_HPP
