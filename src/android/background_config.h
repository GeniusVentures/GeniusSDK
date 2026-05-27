#pragma once

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include <rapidjson/document.h>

/**
 * @brief Configuration for Android background processing.
 *
 * Follows the exact struct pattern from GeniusNode::CrdtBackupConfig
 * (GeniusNode.hpp lines 369-377) with default values in brace initialization.
 *
 * Read from background_config.json at GeniusSDKInit() time.
 * If the file is absent or malformed, safe defaults are used.
 */
struct BackgroundConfig
{
    std::string mode{ "on_demand" };
    uint32_t    wakeup_interval_minutes{ 15 };
    bool        network_required{ true };
    bool        battery_not_low{ true };
    bool        idle_only{ false };
};

/**
 * @brief Load background configuration from JSON file.
 *
 * Follows the exact rapidjson parse pattern from GeniusNode::LoadCrdtConfig()
 * (GeniusNode.cpp lines 195-251):
 *  1. Open ifstream, check .good() — use defaults if absent
 *  2. Parse with rapidjson::Document::Parse()
 *  3. Check HasParseError() and IsObject() — use defaults if invalid
 *  4. For each member: HasMember() + type check BEFORE value access
 *  5. Apply non-zero defaults as safety net
 *
 * @param base_path Path to the directory containing background_config.json
 * @param cfg       [out] BackgroundConfig populated with parsed values or defaults
 */
inline void LoadBackgroundConfig( const std::string &base_path, BackgroundConfig &cfg )
{
    // Reset to defaults
    cfg = BackgroundConfig{};

    const std::string config_path = base_path + "/background_config.json";
    std::ifstream     config_file( config_path );
    if ( !config_file.good() )
    {
        // File not found — use defaults
        return;
    }

    std::stringstream buffer;
    buffer << config_file.rdbuf();

    // Parse JSON configuration — mirrors LoadCrdtConfig pattern exactly
    // (GeniusNode.cpp lines 195-251): HasParseError guard, HasMember + type check
    // before every value access, safe defaults on any failure.
    //
    // Mitigation T-01-06: rapidjson Parse() + HasParseError() check
    // → fall back to safe defaults on malformed JSON (DoS prevention).
    rapidjson::Document config_json;
    config_json.Parse( buffer.str().c_str() );
    if ( config_json.HasParseError() || !config_json.IsObject() )
    {
        return; // use safe defaults
    }

    if ( config_json.HasMember( "mode" ) && config_json["mode"].IsString() )
    {
        cfg.mode = config_json["mode"].GetString();
    }
    if ( config_json.HasMember( "wakeup_interval_minutes" ) &&
         config_json["wakeup_interval_minutes"].IsUint() )
    {
        cfg.wakeup_interval_minutes = config_json["wakeup_interval_minutes"].GetUint();
    }
    if ( config_json.HasMember( "network_required" ) && config_json["network_required"].IsBool() )
    {
        cfg.network_required = config_json["network_required"].GetBool();
    }
    if ( config_json.HasMember( "battery_not_low" ) && config_json["battery_not_low"].IsBool() )
    {
        cfg.battery_not_low = config_json["battery_not_low"].GetBool();
    }
    if ( config_json.HasMember( "idle_only" ) && config_json["idle_only"].IsBool() )
    {
        cfg.idle_only = config_json["idle_only"].GetBool();
    }

    // Apply non-zero defaults for fields that should not be zero
    if ( cfg.wakeup_interval_minutes == 0 )
    {
        cfg.wakeup_interval_minutes = 15;
    }
}
