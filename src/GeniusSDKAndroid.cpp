#include "GeniusSDKAndroid.hpp"
#include "android/background_config.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>

#include <android/log.h>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "GeniusSDK.h"

#define LOG_TAG "GeniusSDKBackground"
#define LOGI( ... ) __android_log_print( ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__ )
#define LOGE( ... ) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

// ============================================================================
// Anonymous namespace — global JVM pointer and cached JNI references
// Pattern: Android.cpp lines 17-19
// ============================================================================
namespace
{
    JavaVM *g_jvm = nullptr;

    // Cached global references for BackgroundServiceManager
    jclass     g_background_manager_class = nullptr;
    jmethodID  g_request_foreground_method = nullptr;
    jmethodID  g_request_stop_foreground_method = nullptr;

    // Cached global reference for GeniusForegroundService
    jclass     g_foreground_service_class = nullptr;
    jmethodID  g_get_processing_status_method = nullptr;

    // Background configuration — loaded once via nativeInit
    // Mitigation T-01-07: default mode is "on_demand" (safe); "always" requires
    // explicit config file edit. Never crash on missing/malformed config.
    BackgroundConfig g_background_config;
    bool             g_background_config_loaded = false;
}

// ============================================================================
// JNI_OnLoad — library entry point on Android
// Pattern: Android.cpp lines 112-127
// ============================================================================
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad( JavaVM *vm, void *_reserved )
{
    LOGI( "GeniusSDK Android background bridge initializing" );

    g_jvm = vm;

    JNIEnv *env = nullptr;
    if ( vm->GetEnv( reinterpret_cast<void **>( &env ), JNI_VERSION_1_6 ) != JNI_OK )
    {
        LOGE( "Failed to get JNI environment in JNI_OnLoad" );
        return JNI_ERR;
    }

    LOGI( "GeniusSDK Android background bridge initialized successfully" );
    return JNI_VERSION_1_6;
}

// ============================================================================
// Namespace sgns — JNI helper functions
// ============================================================================
namespace sgns
{

    // ========================================================================
    // GetJNIEnv — thread-safe JNIEnv retrieval
    // Pattern: Android.cpp lines 266-287
    // Handles C++ IO threads that are NOT JNI threads by default (T-01-01)
    // ========================================================================
    JNIEnv *GetJNIEnv()
    {
        JNIEnv *env    = nullptr;
        jint    result = g_jvm->GetEnv( reinterpret_cast<void **>( &env ), JNI_VERSION_1_6 );

        if ( result == JNI_EDETACHED )
        {
            result = g_jvm->AttachCurrentThread( &env, nullptr );
            if ( result != JNI_OK )
            {
                LOGE( "Failed to attach current thread to JVM" );
                return nullptr;
            }
        }
        else if ( result != JNI_OK )
        {
            LOGE( "Failed to get JNI environment" );
            return nullptr;
        }

        return env;
    }

    // ========================================================================
    // AndroidRequestForegroundService — JNI upcall to start foreground service
    // Pattern: Android.cpp lines 179-196 (CallStaticBooleanMethod + ExceptionCheck)
    // Mitigation T-01-04: strncpy-safe string handling
    // ========================================================================
    bool AndroidRequestForegroundService( const char *title, const char *text )
    {
        JNIEnv *env = GetJNIEnv();
        if ( env == nullptr )
        {
            LOGE( "AndroidRequestForegroundService: GetJNIEnv returned null" );
            return false;
        }

        // Cache class and method references on first call
        // Pattern: Android.cpp lines 88-107 (FindClass + NewGlobalRef)
        if ( g_background_manager_class == nullptr )
        {
            jclass local_class = env->FindClass( "ai/gnus/sdk/BackgroundServiceManager" );
            if ( local_class == nullptr )
            {
                LOGE( "Failed to find BackgroundServiceManager class" );
                if ( env->ExceptionCheck() != 0U )
                {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }
                return false;
            }

            g_background_manager_class =
                static_cast<jclass>( env->NewGlobalRef( local_class ) );
            env->DeleteLocalRef( local_class );

            if ( g_background_manager_class == nullptr )
            {
                LOGE( "Failed to create global reference for BackgroundServiceManager" );
                return false;
            }

            // Get static method: boolean requestForegroundService(String, String)
            g_request_foreground_method =
                env->GetStaticMethodID( g_background_manager_class,
                                        "requestForegroundService",
                                        "(Ljava/lang/String;Ljava/lang/String;)Z" );

            if ( g_request_foreground_method == nullptr )
            {
                LOGE( "Failed to find requestForegroundService method" );
                if ( env->ExceptionCheck() != 0U )
                {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }
                return false;
            }

            LOGI( "BackgroundServiceManager class and methods cached" );
        }

        // Create JNI strings from C strings
        // Mitigation T-01-04: ensure valid null-terminated C strings
        jstring jtitle = nullptr;
        jstring jtext  = nullptr;

        if ( title != nullptr )
        {
            jtitle = env->NewStringUTF( title );
        }
        if ( text != nullptr )
        {
            jtext = env->NewStringUTF( text );
        }

        if ( jtitle == nullptr || jtext == nullptr )
        {
            LOGE( "Failed to create JNI strings for foreground service request" );
            if ( jtitle != nullptr ) env->DeleteLocalRef( jtitle );
            if ( jtext != nullptr ) env->DeleteLocalRef( jtext );
            return false;
        }

        // Call static method on BackgroundServiceManager
        // Pattern: Android.cpp lines 244-245 (CallStaticBooleanMethod)
        jboolean result = env->CallStaticBooleanMethod( g_background_manager_class,
                                                         g_request_foreground_method,
                                                         jtitle, jtext );

        // Check for Java exceptions
        // Pattern: Android.cpp lines 189-195
        if ( env->ExceptionCheck() != 0U )
        {
            LOGE( "Java exception in requestForegroundService" );
            env->ExceptionDescribe();
            env->ExceptionClear();
            env->DeleteLocalRef( jtitle );
            env->DeleteLocalRef( jtext );
            return false;
        }

        // Cleanup local references
        env->DeleteLocalRef( jtitle );
        env->DeleteLocalRef( jtext );

        LOGI( "requestForegroundService result: %s",
              ( result == JNI_TRUE ) ? "true" : "false" );
        return ( result == JNI_TRUE );
    }

    // ========================================================================
    // AndroidRequestStopForegroundService — JNI upcall to stop foreground svc
    // ========================================================================
    bool AndroidRequestStopForegroundService()
    {
        JNIEnv *env = GetJNIEnv();
        if ( env == nullptr )
        {
            LOGE( "AndroidRequestStopForegroundService: GetJNIEnv returned null" );
            return false;
        }

        if ( g_background_manager_class == nullptr )
        {
            LOGE( "BackgroundServiceManager class not cached — cannot stop service" );
            return false;
        }

        // Get stop method ID on first use
        if ( g_request_stop_foreground_method == nullptr )
        {
            g_request_stop_foreground_method =
                env->GetStaticMethodID( g_background_manager_class,
                                        "requestStopForegroundService",
                                        "()V" );

            if ( g_request_stop_foreground_method == nullptr )
            {
                LOGE( "Failed to find requestStopForegroundService method" );
                if ( env->ExceptionCheck() != 0U )
                {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                }
                return false;
            }
        }

        env->CallStaticVoidMethod( g_background_manager_class,
                                    g_request_stop_foreground_method );

        if ( env->ExceptionCheck() != 0U )
        {
            LOGE( "Java exception in requestStopForegroundService" );
            env->ExceptionDescribe();
            env->ExceptionClear();
            return false;
        }

        LOGI( "requestStopForegroundService dispatched" );
        return true;
    }

} // namespace sgns

// ============================================================================
// Helper: Extract app files directory from Android Context via JNI
// Used by nativeInit to locate background_config.json
// ============================================================================
static std::string GetBasePathFromAndroidContext( JNIEnv *env, jobject context )
{
    if ( env == nullptr || context == nullptr )
    {
        return "";
    }

    // Call context.getFilesDir() → returns java.io.File
    jclass    context_class = env->GetObjectClass( context );
    jmethodID get_files_dir = env->GetMethodID( context_class, "getFilesDir",
                                                 "()Ljava/io/File;" );
    if ( get_files_dir == nullptr )
    {
        env->DeleteLocalRef( context_class );
        return "";
    }

    jobject files_dir_obj = env->CallObjectMethod( context, get_files_dir );
    env->DeleteLocalRef( context_class );

    if ( files_dir_obj == nullptr )
    {
        return "";
    }

    // Call File.getAbsolutePath() → returns String
    jclass    file_class    = env->GetObjectClass( files_dir_obj );
    jmethodID get_abs_path  = env->GetMethodID( file_class, "getAbsolutePath",
                                                 "()Ljava/lang/String;" );
    if ( get_abs_path == nullptr )
    {
        env->DeleteLocalRef( file_class );
        env->DeleteLocalRef( files_dir_obj );
        return "";
    }

    jstring abs_path_str = static_cast<jstring>(
        env->CallObjectMethod( files_dir_obj, get_abs_path ) );
    env->DeleteLocalRef( file_class );
    env->DeleteLocalRef( files_dir_obj );

    if ( abs_path_str == nullptr )
    {
        return "";
    }

    const char *cstr = env->GetStringUTFChars( abs_path_str, nullptr );
    std::string result( cstr != nullptr ? cstr : "" );
    if ( cstr != nullptr )
    {
        env->ReleaseStringUTFChars( abs_path_str, cstr );
    }
    env->DeleteLocalRef( abs_path_str );

    return result;
}

// ============================================================================
// JNI exported functions — called FROM Kotlin/Java
// ============================================================================

/**
 * @brief Initialize the native background bridge with Android context.
 *
 * Called by BackgroundServiceManager.initialize().
 * Extracts the app's files directory path from the Android Context,
 * loads background_config.json via LoadBackgroundConfig(), and
 * stores the parsed config for subsequent wake-up decisions.
 *
 * JNI signature: Java_ai_gnus_sdk_BackgroundServiceManager_nativeInit
 *
 * Mitigation T-01-06: malformed JSON → LoadBackgroundConfig uses safe defaults.
 * Mitigation T-01-07: default mode is "on_demand" — safe by default.
 */
extern "C" JNIEXPORT void JNICALL
Java_ai_gnus_sdk_BackgroundServiceManager_nativeInit(
    JNIEnv *env, jclass /* clazz */, jobject context )
{
    LOGI( "nativeInit: loading background configuration" );

    std::string base_path = GetBasePathFromAndroidContext( env, context );

    if ( base_path.empty() )
    {
        LOGI( "nativeInit: could not determine base path — using defaults" );
        g_background_config        = BackgroundConfig{};
        g_background_config_loaded = true;
        return;
    }

    LOGI( "nativeInit: base_path=%s", base_path.c_str() );

    // Load and parse background_config.json
    // LoadBackgroundConfig follows LoadCrdtConfig pattern:
    // defaults if file absent, safe defaults if parse error
    LoadBackgroundConfig( base_path, g_background_config );
    g_background_config_loaded = true;

    LOGI( "nativeInit: config loaded — mode=%s, interval=%u min, "
          "network=%s, battery_not_low=%s, idle_only=%s",
          g_background_config.mode.c_str(),
          g_background_config.wakeup_interval_minutes,
          g_background_config.network_required ? "true" : "false",
          g_background_config.battery_not_low ? "true" : "false",
          g_background_config.idle_only ? "true" : "false" );
}

/**
 * @brief Return the current background configuration as a JSON string.
 *
 * Called by BackgroundServiceManager.initialize() after nativeInit
 * so the Kotlin layer can read config values without re-parsing the file.
 *
 * JNI signature: Java_ai_gnus_sdk_BackgroundServiceManager_nativeGetConfigJson
 *
 * @return JSON string with config values, or empty string if not yet loaded
 */
extern "C" JNIEXPORT jstring JNICALL
Java_ai_gnus_sdk_BackgroundServiceManager_nativeGetConfigJson(
    JNIEnv *env, jclass /* clazz */ )
{
    if ( !g_background_config_loaded )
    {
        LOGE( "nativeGetConfigJson: config not loaded — returning null" );
        return nullptr;
    }

    // Serialize BackgroundConfig to JSON using rapidjson::Writer
    // Pattern: Android.cpp lines 6-7 (rapidjson/stringbuffer.h, rapidjson/writer.h)
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer( sb );

    writer.StartObject();
    writer.Key( "mode" );
    writer.String( g_background_config.mode.c_str() );
    writer.Key( "wakeupIntervalMinutes" );
    writer.Uint( g_background_config.wakeup_interval_minutes );
    writer.Key( "networkRequired" );
    writer.Bool( g_background_config.network_required );
    writer.Key( "batteryNotLow" );
    writer.Bool( g_background_config.battery_not_low );
    writer.Key( "idleOnly" );
    writer.Bool( g_background_config.idle_only );
    writer.EndObject();

    return env->NewStringUTF( sb.GetString() );
}

/**
 * @brief WorkManager wake-up handler — config-aware.
 *
 * Called by GeniusBackgroundWorker.doWork() via JNI when a WorkManager
 * periodic task fires. Checks the GeniusNodeInstance processing status
 * and decides whether to start/stop the foreground service based on
 * the configured background mode.
 *
 * JNI signature: Java_ai_gnus_sdk_GeniusBackgroundWorker_nativeOnWorkManagerWakeUp
 *
 * Mode behavior (per D-07, T-01-07):
 *   "disabled" → return false immediately — no foreground service
 *   "always"   → always request foreground service regardless of status
 *   "on_demand" (default) → status-driven: PROCESSING → start, IDLE → stop
 *
 * Guard: if config not loaded, defaults to "on_demand" with safe behavior.
 * Per D-10: on wake-up, re-checks GeniusNodeInstance (CRDT re-sync on init).
 * Per D-12: assumes node availability — no polling of GetNodeState().
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_ai_gnus_sdk_GeniusBackgroundWorker_nativeOnWorkManagerWakeUp(
    JNIEnv *env, jclass /* clazz */ )
{
    LOGI( "WorkManager wake-up triggered" );

    // Determine effective mode (safe default if config not loaded)
    std::string effective_mode = "on_demand";
    if ( g_background_config_loaded )
    {
        effective_mode = g_background_config.mode;
    }

    // Mode "disabled" — skip all foreground service actions
    if ( effective_mode == "disabled" )
    {
        LOGI( "Background processing disabled by config — no action" );
        return JNI_FALSE;
    }

    // Access the global GeniusNodeInstance singleton
    // Pattern: GeniusSDK.cpp line 203
    extern std::shared_ptr<sgns::GeniusNode> GeniusNodeInstance;

    if ( !GeniusNodeInstance )
    {
        LOGI( "GeniusNodeInstance is null — node not initialized, skipping wake-up" );
        return JNI_FALSE;
    }

    // Check processing status via existing C API
    // Pattern: GeniusSDK.cpp lines 642-655
    GeniusProcessingStatusInfo status_info = GeniusSDKGetProcessingStatus();

    LOGI( "Processing status: %d, percentage: %.1f%%, mode: %s",
          static_cast<int>( status_info.status ), status_info.percentage,
          effective_mode.c_str() );

    // Mode "always" — request foreground service unconditionally
    if ( effective_mode == "always" )
    {
        LOGI( "Mode is always — requesting foreground service unconditionally" );

        char status_text[64];
        if ( status_info.status == GENIUS_PR_STATUS_PROCESSING )
        {
            std::snprintf( status_text, sizeof( status_text ),
                           "PROCESSING: %.0f%%", status_info.percentage );
        }
        else
        {
            std::snprintf( status_text, sizeof( status_text ), "Standing by" );
        }

        bool started = sgns::AndroidRequestForegroundService(
            "SuperGenius Processing", status_text );

        LOGI( "Foreground service %s", started ? "started" : "failed to start" );
        return started ? JNI_TRUE : JNI_FALSE;
    }

    // Mode "on_demand" (default) — status-driven behavior
    if ( status_info.status == GENIUS_PR_STATUS_PROCESSING )
    {
        // Node has pending CRDT work — request foreground service
        LOGI( "Node is PROCESSING — requesting foreground service" );

        char status_text[64];
        std::snprintf( status_text, sizeof( status_text ),
                       "PROCESSING: %.0f%%", status_info.percentage );

        bool started = sgns::AndroidRequestForegroundService(
            "SuperGenius Processing", status_text );

        return started ? JNI_TRUE : JNI_FALSE;
    }
    else if ( status_info.status == GENIUS_PR_STATUS_IDLE )
    {
        LOGI( "Node is IDLE — requesting foreground service stop" );
        sgns::AndroidRequestStopForegroundService();
        return JNI_FALSE;
    }

    // DISABLED or other — no action needed
    LOGI( "Node is DISABLED — no foreground service action" );
    return JNI_FALSE;
}

/**
 * @brief Poll processing status for foreground service notification updates.
 *
 * Called by GeniusForegroundService coroutine every 1000ms to update
 * the notification with live processing status from the C++ node.
 *
 * JNI signature: Java_ai_gnus_sdk_GeniusForegroundService_nativeGetProcessingStatus
 *
 * Per D-09: returns DISABLED/IDLE/PROCESSING + percentage.
 * Notification shows ONLY these values — never internal data.
 */
extern "C" JNIEXPORT jobject JNICALL
Java_ai_gnus_sdk_GeniusForegroundService_nativeGetProcessingStatus(
    JNIEnv *env, jclass /* clazz */ )
{
    // Get processing status from the GeniusNodeInstance
    GeniusProcessingStatusInfo status_info;
    status_info.status     = GENIUS_PR_STATUS_DISABLED;
    status_info.percentage = 0.0f;

    extern std::shared_ptr<sgns::GeniusNode> GeniusNodeInstance;

    if ( GeniusNodeInstance )
    {
        status_info = GeniusSDKGetProcessingStatus();
    }

    // Return a ProcessingStatusInfo object to Kotlin
    // The Kotlin companion object expects: data class ProcessingStatusInfo(status: Int, percentage: Float)
    jclass status_class = env->FindClass( "ai/gnus/sdk/GeniusForegroundService$ProcessingStatusInfo" );
    if ( status_class == nullptr )
    {
        LOGE( "Failed to find ProcessingStatusInfo class" );
        if ( env->ExceptionCheck() != 0U )
        {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return nullptr;
    }

    // Get constructor: ProcessingStatusInfo(int status, float percentage)
    jmethodID constructor = env->GetMethodID( status_class, "<init>", "(IF)V" );
    if ( constructor == nullptr )
    {
        LOGE( "Failed to find ProcessingStatusInfo constructor" );
        env->DeleteLocalRef( status_class );
        return nullptr;
    }

    jobject result = env->NewObject( status_class, constructor,
                                      static_cast<jint>( status_info.status ),
                                      static_cast<jfloat>( status_info.percentage ) );

    env->DeleteLocalRef( status_class );
    return result;
}
