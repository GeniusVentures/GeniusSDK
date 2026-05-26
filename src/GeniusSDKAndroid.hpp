#pragma once

#include <jni.h>
#include <string>

namespace sgns
{
    // Forward declare BackgroundConfig (defined in android/background_config.h)
    struct BackgroundConfig;

    /**
     * @brief Request Android to start the GeniusForegroundService.
     *
     * Called from C++ IO threads when the node detects pending CRDT work.
     * Uses JNI upcall to BackgroundServiceManager.requestForegroundService().
     *
     * @param title Notification title (e.g. "SuperGenius Processing")
     * @param text  Notification content text (e.g. "PROCESSING: 42%")
     * @return true if the foreground service was started successfully
     */
    bool AndroidRequestForegroundService( const char *title, const char *text );

    /**
     * @brief Request Android to stop the GeniusForegroundService.
     *
     * Called when the node transitions to idle and no longer needs
     * the process to stay alive in the foreground.
     *
     * @return true if the stop request was dispatched successfully
     */
    bool AndroidRequestStopForegroundService();

    /**
     * @brief Thread-safe JNIEnv retrieval.
     *
     * Follows the exact pattern from AndroidSecureStorage::GetJNIEnv().
     * On C++ IO threads (which are NOT JNI threads by default),
     * calls AttachCurrentThread before returning the JNIEnv.
     *
     * @return JNIEnv pointer, or nullptr if the thread could not be attached
     */
    JNIEnv *GetJNIEnv();
}
