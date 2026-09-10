/**
 * @file       GeniusSDK.hpp
 * @brief      C++ API extensions for the Genius SDK.
 *             Provides direct access to the underlying GeniusNode for
 *             C++ libraries building FFI interfaces on top of the SDK.
 * @date       2026-08-03
 */

#ifndef _GENIUSSDK_HPP
#define _GENIUSSDK_HPP

#include "GeniusSDK.h"

#include <memory>

#include "account/GeniusNode.hpp"

/**
 * @brief  Returns the initialized GeniusNode instance for direct C++ access.
 * @return Shared pointer to the sgns::GeniusNode, or nullptr if the SDK is not initialized.
 */
GNUS_VISIBILITY_DEFAULT std::shared_ptr<sgns::GeniusNode> GeniusSDKGetNode();

#endif
