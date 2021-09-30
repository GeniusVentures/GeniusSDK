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

  void SetDatastorePath(const std::string& aPath);

  /**
   * Adding TransactionBlocks to database
   * @param tBlocks TransactionBlocks to add to database
   * @return 0 on success
   */
  GENIUSSDK_EXPORT int AddTransactionBlocks(const TransactionBlocks& tBlocks);

protected:
  /** Copy constructor
  */
  GeniusSDK(const GeniusSDK &) = default;

  std::shared_ptr<sgns::crdt::CrdtDatastore> crdtDatastore_ = nullptr;
};
}

#endif //GENIUSSDK_H
