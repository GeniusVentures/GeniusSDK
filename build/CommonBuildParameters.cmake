# --------------------------------------------------------
# define third party directory
if (NOT DEFINED THIRDPARTY_DIR)
  print("Setting default third party directory")
  set(THIRDPARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty")
  ## get absolute path
  cmake_path(SET THIRDPARTY_DIR NORMALIZE "${THIRDPARTY_DIR}")
endif()

if (NOT DEFINED THIRDPARTY_BUILD_DIR)
  print("Setting third party build directory default")
  set(THIRDPARTY_BUILD_DIR "${THIRDPARTY_DIR}/build/${CMAKE_SYSTEM_NAME}/${CMAKE_BUILD_TYPE}")
endif()

print("THIRDPARTY BUILD DIR: ${THIRDPARTY_BUILD_DIR}")
print("THIRDPARTY SRC DIR: ${THIRDPARTY_DIR}")

# --------------------------------------------------------
# Set config of GTest
set(GTest_DIR "${THIRDPARTY_BUILD_DIR}/GTest/lib/cmake/GTest")
set(GTest_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/GTest/include")
find_package(GTest CONFIG REQUIRED)
include_directories(${GTest_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of fmt
set(fmt_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/fmt/include")
set(fmt_DIR "${THIRDPARTY_BUILD_DIR}/fmt/lib/cmake/fmt")
find_package(fmt CONFIG REQUIRED)
include_directories(${fmt_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of soralog
set(soralog_DIR "${THIRDPARTY_BUILD_DIR}/soralog/lib/cmake/soralog")
set(soralog_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/soralog/include")
find_package(soralog CONFIG REQUIRED)
include_directories(${soralog_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of cares
set(c-ares_DIR "${THIRDPARTY_BUILD_DIR}/cares/lib/cmake/c-ares")
set(c-ares_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/cares/include")
find_package(c-ares CONFIG REQUIRED)
include_directories(${c-ares_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of yaml-cpp
set(yaml-cpp_DIR "${THIRDPARTY_BUILD_DIR}/yaml-cpp/share/cmake/yaml-cpp")
set(yaml-cpp_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/yaml-cpp/include")
find_package(yaml-cpp CONFIG REQUIRED)
include_directories(${yaml-cpp_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of protobuf project
set(Protobuf_DIR "${THIRDPARTY_BUILD_DIR}/grpc/lib/cmake/protobuf")
set(Protobuf_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/grpc/include/google/protobuf")
if (${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
  set(Protobuf_DIR  "${iOS_Protobuf_DIR}")
  set(Protobuf_INCLUDE_DIR "${iOS_Protobuf_INCLUDE_DIR}")
  set(Protobuf_LIBRARIES "${iOS_Protobuf_LIBRARIES}")
endif()
if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  # TODO: Check grpc install on Windows
  set(Protobuf_DIR  "${THIRDPARTY_BUILD_DIR}/grpc/build/third_party/protobuf/cmake")
  set(Protobuf_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/grpc/build/third_party/protobuf/include/google/protobuf")
endif()

find_package(Protobuf CONFIG REQUIRED )
include_directories(${Protobuf_INCLUDE_DIR})

if (NOT DEFINED PROTOC_EXECUTABLE)
  set(PROTOC_EXECUTABLE "${THIRDPARTY_BUILD_DIR}/grpc/bin/protoc${CMAKE_EXECUTABLE_SUFFIX}")
endif()

set(Protobuf_PROTOC_EXECUTABLE ${PROTOC_EXECUTABLE} CACHE PATH "Initial cache" FORCE)
if(NOT TARGET protobuf::protoc)
  add_executable(protobuf::protoc IMPORTED)
endif()
if(EXISTS "${Protobuf_PROTOC_EXECUTABLE}")
  set_target_properties(protobuf::protoc PROPERTIES
          IMPORTED_LOCATION ${Protobuf_PROTOC_EXECUTABLE})
endif()

# protoc definition #####################################################################################
get_target_property(PROTOC_LOCATION protobuf::protoc IMPORTED_LOCATION)
print("PROTOC_LOCATION: ${PROTOC_LOCATION}")
if ( Protobuf_FOUND )
  message( STATUS "Protobuf version : ${Protobuf_VERSION}" )
  message( STATUS "Protobuf compiler : ${Protobuf_PROTOC_EXECUTABLE}")
endif()
include(${PROJECT_ROOT}/cmake/functions.cmake)


# --------------------------------------------------------
# Set config of openssl project
set(OPENSSL_DIR "${THIRDPARTY_BUILD_DIR}/openssl/build/${CMAKE_SYSTEM_NAME}${ABI_SUBFOLDER_NAME}")
set(OPENSSL_USE_STATIC_LIBS ON)
set(OPENSSL_MSVC_STATIC_RT ON)
set(OPENSSL_ROOT_DIR "${OPENSSL_DIR}")
set(OPENSSL_INCLUDE_DIR "${OPENSSL_DIR}/include")
set(OPENSSL_LIBRARIES "${OPENSSL_DIR}/lib")
set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_LIBRARIES}/libcrypto${CMAKE_STATIC_LIBRARY_SUFFIX})
set(OPENSSL_SSL_LIBRARY ${OPENSSL_LIBRARIES}/libssl${CMAKE_STATIC_LIBRARY_SUFFIX})

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of rocksdb
set(RocksDB_DIR "${THIRDPARTY_BUILD_DIR}/rocksdb/lib/cmake/rocksdb")
set(RocksDB_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/rocksdb/include")
find_package(RocksDB CONFIG REQUIRED)
include_directories(${RocksDB_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of Microsoft.GSL
set(GSL_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/GSL/include")
include_directories(${GSL_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of fmt
set(fmt_DIR "${THIRDPARTY_BUILD_DIR}/fmt/lib/cmake/fmt")
set(fmt_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/fmt/include")
find_package(fmt CONFIG REQUIRED)
include_directories(${fmt_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of spdlog v1.4.2
set(spdlog_DIR "${THIRDPARTY_BUILD_DIR}/spdlog/lib/cmake/spdlog")
set(spdlog_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/spdlog/include")
find_package(spdlog CONFIG REQUIRED)
include_directories(${spdlog_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of soralog
set(soralog_DIR "${THIRDPARTY_BUILD_DIR}/soralog/lib/cmake/soralog")
set(soralog_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/soralog/include")
find_package(soralog CONFIG REQUIRED)
include_directories(${soralog_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of yaml-cpp
set(yaml-cpp_DIR "${THIRDPARTY_BUILD_DIR}/yaml-cpp/share/cmake/yaml-cpp")
set(yaml-cpp_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/yaml-cpp/include")
find_package(yaml-cpp CONFIG REQUIRED)
include_directories(${yaml-cpp_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of  tsl_hat_trie
set(tsl_hat_trie_DIR "${THIRDPARTY_BUILD_DIR}/hat-trie/lib/cmake/tsl_hat_trie")
set(tsl_hat_trie_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/hat-trie/include")
find_package(tsl_hat_trie CONFIG REQUIRED)
include_directories(${tsl_hat_trie_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of libp2p
set(libp2p_DIR "${THIRDPARTY_BUILD_DIR}/libp2p/lib/cmake/libp2p")
set(libp2p_LIBRARY_DIR "${THIRDPARTY_BUILD_DIR}/libp2p/lib")
set(libp2p_INCLUDE_DIR    "${THIRDPARTY_BUILD_DIR}/libp2p/include")
find_package(libp2p CONFIG REQUIRED)
include_directories(${libp2p_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of ipfs-lite-cpp
set(ipfs-lite-cpp_DIR "${THIRDPARTY_BUILD_DIR}/ipfs-lite-cpp/cmake/ipfs-lite-cpp")
set(ipfs-lite-cpp_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/ipfs-lite-cpp/include")
set(ipfs-lite-cpp_LIB_DIR "${THIRDPARTY_BUILD_DIR}/ipfs-lite-cpp/lib")
set(CBOR_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/ipfs-lite-cpp/include/deps/tinycbor/src")
find_package(ipfs-lite-cpp CONFIG REQUIRED)
include_directories(${ipfs-lite-cpp_INCLUDE_DIR} ${CBOR_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of ipfs-pubsub
set(ipfs-pubsub_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/ipfs-pubsub/include")
set(ipfs-pubsub_DIR "${THIRDPARTY_BUILD_DIR}/ipfs-pubsub/lib/cmake/ipfs-pubsub")
find_package(ipfs-pubsub CONFIG REQUIRED)
include_directories(${ipfs-pubsub_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of Boost.DI
set(Boost.DI_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/Boost.DI/include")
set(Boost.DI_DIR "${THIRDPARTY_BUILD_DIR}/Boost.DI/lib/cmake/Boost.DI")
find_package(Boost.DI CONFIG REQUIRED)
include_directories(${Boost.DI_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of Boost project
set(_BOOST_ROOT "${THIRDPARTY_BUILD_DIR}/boost/build/${CMAKE_SYSTEM_NAME}${ABI_SUBFOLDER_NAME}")
set(Boost_LIB_DIR "${_BOOST_ROOT}/lib")
set(Boost_INCLUDE_DIR "${_BOOST_ROOT}/include/boost-1_72")
set(Boost_DIR "${Boost_LIB_DIR}/cmake/Boost-1.72.0")
set(boost_headers_DIR "${Boost_LIB_DIR}/cmake/boost_headers-1.72.0")
set(boost_random_DIR "${Boost_LIB_DIR}/cmake/boost_random-1.72.0")
set(boost_system_DIR "${Boost_LIB_DIR}/cmake/boost_system-1.72.0")
set(boost_filesystem_DIR "${Boost_LIB_DIR}/cmake/boost_filesystem-1.72.0")
set(boost_program_options_DIR "${Boost_LIB_DIR}/cmake/boost_program_options-1.72.0")
set(boost_date_time_DIR "${Boost_LIB_DIR}/cmake/boost_date_time-1.72.0")
set(boost_regex_DIR "${Boost_LIB_DIR}/cmake/boost_regex-1.72.0")
set(boost_atomic_DIR "${Boost_LIB_DIR}/cmake/boost_atomic-1.72.0")
set(boost_chrono_DIR "${Boost_LIB_DIR}/cmake/boost_chrono-1.72.0")
set(boost_log_DIR "${Boost_LIB_DIR}/cmake/boost_log-1.72.0")
set(boost_log_setup_DIR "${Boost_LIB_DIR}/cmake/boost_log_setup-1.72.0")
set(boost_thread_DIR "${Boost_LIB_DIR}/cmake/boost_thread-1.72.0")
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_NO_SYSTEM_PATHS ON)
set(Boost_USE_STATIC_RUNTIME ON)
if (${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
  set(Boost_USE_STATIC_RUNTIME OFF)
endif()

option (SGNS_STACKTRACE_BACKTRACE "Use BOOST_STACKTRACE_USE_BACKTRACE in stacktraces, for POSIX" OFF)
if (SGNS_STACKTRACE_BACKTRACE)
  add_definitions(-DSGNS_STACKTRACE_BACKTRACE=1)
  if (BACKTRACE_INCLUDE)
    add_definitions(-DBOOST_STACKTRACE_BACKTRACE_INCLUDE_FILE=${BACKTRACE_INCLUDE})
  endif()
endif ()

# header only libraries must not be added here
find_package(Boost REQUIRED COMPONENTS date_time filesystem random regex system thread log log_setup program_options)
include_directories(${Boost_INCLUDE_DIRS})

# --------------------------------------------------------
# Set config of ed25519
set(ed25519_DIR "${THIRDPARTY_BUILD_DIR}/ed25519/lib/cmake/ed25519")
set(ed25519_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/ed25519/include")
find_package(ed25519 CONFIG REQUIRED)
include_directories(${ed25519_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of sr25519-donna
set(sr25519-donna_DIR "${THIRDPARTY_BUILD_DIR}/sr25519-donna/lib/cmake/sr25519-donna")
set(sr25519-donna_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/sr25519-donna/include")
find_package(sr25519-donna CONFIG REQUIRED)
include_directories(${sr25519-donna_INCLUDE_DIR})

# --------------------------------------------------------
# Set RapidJSON config path
set(RapidJSON_DIR "${THIRDPARTY_BUILD_DIR}/rapidjson/lib/cmake/RapidJSON")
set(RapidJSON_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/rapidjson/include")
find_package(RapidJSON CONFIG REQUIRED)
include_directories(${RapidJSON_INCLUDE_DIR})

# --------------------------------------------------------
# Set jsonrpc-lean include path
set(jsonrpc_lean_INCLUDE_DIR "${THIRDPARTY_DIR}/jsonrpc-lean/include")
include_directories(${jsonrpc_lean_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of secp256k1
set(libsecp256k1_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/libsecp256k1/include")
set(libsecp256k1_LIBRARY_DIR "${THIRDPARTY_BUILD_DIR}/libsecp256k1/lib")
set(libsecp256k1_DIR "${THIRDPARTY_BUILD_DIR}/libsecp256k1/lib/cmake/libsecp256k1")
find_package(libsecp256k1 CONFIG REQUIRED)
include_directories(${libsecp256k1_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of xxhash
set(xxhash_INCLUDE_DIR "${THIRDPARTY_BUILD_DIR}/xxhash/include")
set(xxhash_LIBRARY_DIR "${THIRDPARTY_BUILD_DIR}/xxhash/lib")
set(xxhash_DIR "${THIRDPARTY_BUILD_DIR}/xxhash/lib/cmake/xxhash")
find_package(xxhash CONFIG REQUIRED)
include_directories(${xxhash_INCLUDE_DIR})

# --------------------------------------------------------
# Set config of SuperGenius project
if (NOT DEFINED SuperGenius_DIR)
  print("Setting SuperGenius build directory default")
  set(SUPERGENIUS_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/../../SuperGenius")
  set(SuperGenius_DIR "${SUPERGENIUS_SRC_DIR}/build/${CMAKE_SYSTEM_NAME}/${CMAKE_BUILD_TYPE}/SuperGenius/lib/cmake/SuperGenius/")
endif()
print("SuperGenius_DIR: ${SuperGenius_DIR}")

find_package(SuperGenius CONFIG REQUIRED)
include_directories(${SuperGenius_INCLUDE_DIR})

# --------------------------------------------------------
include_directories(
        ${PROJECT_ROOT}/include
        ${PROJECT_ROOT}/src
        ${CMAKE_BINARY_DIR}
)

# --------------------------------------------------------
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(TESTING "Build tests" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)

add_subdirectory(${PROJECT_ROOT}/src ${CMAKE_BINARY_DIR}/src)

# --------------------------------------------------------
if (TESTING)
  # Set config of GTest project
  find_package(GTest CONFIG REQUIRED)
  include_directories(${GTest_INCLUDE_DIR})

  enable_testing()
  add_subdirectory(${PROJECT_ROOT}/test ${CMAKE_BINARY_DIR}/test)
endif ()

# --------------------------------------------------------
# Build examples

if (BUILD_EXAMPLES)
    add_subdirectory(${PROJECT_ROOT}/example ${CMAKE_BINARY_DIR}/example)
endif ()

# --------------------------------------------------------
# Install targets

install(
  EXPORT GeniusSDKTargets
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GeniusSDK
  NAMESPACE sgns::
)

# generate the config file that is includes the exports
configure_package_config_file(${PROJECT_ROOT}/cmake/config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/GeniusSDKConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GeniusSDK
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# generate the version file for the config file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/GeniusSDKConfigVersion.cmake"
  VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}"
  COMPATIBILITY AnyNewerVersion
)

# install the configuration file
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/GeniusSDKConfig.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GeniusSDK
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/GeniusSDKConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GeniusSDK
)
