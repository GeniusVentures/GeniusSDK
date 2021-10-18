#ifndef GENIUSSDK_H
#define GENIUSSDK_H

#include "TransactionBlocks.h"
#include <memory>
namespace sgns
{

namespace crdt
{
class CrdtDatastore;
}

/**
 * @brief Interface library for
 */
class GeniusSDK
{
public:

  /** Constructor
   */
  GENIUSSDK_EXPORT GeniusSDK();

  /** Destructor
   */
  GENIUSSDK_EXPORT virtual ~GeniusSDK() = default;

  void SetDatastorePath(const std::string &aPath);

  /**
   * Adding TransactionBlocks to database
   * @param tBlocks TransactionBlocks to add to database
   * @return 0 on success
   */
  GENIUSSDK_EXPORT int AddTransactionBlocks(const TransactionBlocks &tBlocks);

  /**
   * Get seed from mnemonic phrase
   * @param mnemonic List of words
   * @param passphrase Passphrase used for generating seed
   * @param seed Output result of generated seed as HEX
   * @return 0 on success
   */
  GENIUSSDK_EXPORT static int
  GetSeed(const std::vector<std::string> &mnemonic, const std::string &passphrase, std::string &seed);

  /**
   * Get public key from mnemonic phrase
   * @param mnemonic List of words
   * @param passphrase Passphrase used for generating public key
   * @param publicKey
   * @return 0 on success
   */
  GENIUSSDK_EXPORT static int
  GetPublicKey(const std::vector<std::string> &mnemonic, const std::string &passphrase, std::string &publicKey);

  /**
 * Get public key from mnemonic phrase
 * @param mnemonic List of words
 * @param passphrase Passphrase used for generating public key
 * @param privateKey
 * @return 0 on success
 */
  GENIUSSDK_EXPORT static int
  GetPrivateKey(const std::vector<std::string> &mnemonic, const std::string &passphrase, std::string &privateKey);

protected:
  /** Copy constructor
  */
  GeniusSDK(const GeniusSDK &) = default;

  std::shared_ptr<sgns::crdt::CrdtDatastore> crdtDatastore_ = nullptr;
};
}

#endif //GENIUSSDK_H
