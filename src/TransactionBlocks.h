#ifndef TRANSACTIONBLOCKS_H
#define TRANSACTIONBLOCKS_H

#include <vector>
#include "GeniusSDKExport.h"
#include "TransactionData.h"

namespace sgns
{
class GENIUSSDK_EXPORT TransactionBlocks
{
public:

  /** Constructor
   */
  TransactionBlocks() = default;

  /** Copy constructor
  */
  TransactionBlocks(const TransactionBlocks &);

  /** Destructor
   */
  virtual ~TransactionBlocks() = default;

  /** Equality operator
   * @return true if equal otherwise, it returns false.
   */
  bool operator==(const TransactionBlocks &) const;

  /** Equality operator
  * @return true if NOT equal otherwise, it returns false.
  */
  bool operator!=(const TransactionBlocks &) const;

  /** Assignment operator
  */
  TransactionBlocks &operator=(const TransactionBlocks &);

  /**
  * Set block number. -1 when its pending block.
  * @param blockNumber the block number
  */
  void SetBlockNumber(const int &blockNumber);
  /**
   * Get block number
   * @return block number, -1 when its pending block.
   */
  int GetBlockNumber() const;

  /**
  * Set hash of the block
  * @param hash hash of the block
  */
  void SetHash(const std::string &hash);
  /**
   * Get hash of the block
   * @return hash of the block
   */
  std::string GetHash() const;

  /**
  * Set hash of the parent block
  * @param hash hash of the parent block
  */
  void SetParentHash(const std::string &hash);
  /**
   * Get hash of the parent block
   * @return hash of the parent block
   */
  std::string GetParentHash() const;

  /**
  * Set hash of the generated proof-of-work.
   * A hash that, when combined with the hash, proves that the block has gone through proof of work
   * A nonce is an arbitrary number used only once in a cryptographic communication, in the spirit of
   * a nonce word. They are often random or pseudo-random numbers
  * @param nonce hash of the generated proof-of-work
  */
  void SetNonce(const std::string &nonce);
  /**
   * Get hash of the generated proof-of-work.
   * @return nonce hash of the generated proof-of-work
   */
  std::string GetNonce() const;

  /**
  * Set SHA3 of the uncles data in the block
  * @param sha3Uncles SHA3 of the uncles data in the block
  */
  void SetSha3Uncles(const std::string &sha3Uncles);
  /**
   * Get SHA3 of the uncles data in the block
   * @return SHA3 of the uncles data in the block
   */
  std::string GetSha3Uncles() const;

  /**
  * Set the root of the transaction trie of the block
  * @param transactionsRoot the root of the transaction trie of the block
  */
  void SetTransactionsRoot(const std::string &transactionsRoot);
  /**
   * Get the root of the transaction trie of the block
   * @return root of the transaction trie of the block
   */
  std::string GetTransactionsRoot() const;

  /**
  * Set the root of the final state trie of the block
  * @param stateRoot the root of the final state trie of the block
  */
  void SetStateRoot(const std::string &stateRoot);
  /**
   * Get the root of the final state trie of the block
   * @return root of the final state trie of the block
   */
  std::string GetStateRoot() const;

  /**
  * Set the root of the receipts trie of the block
  * @param receiptsRoot the root of the receipts trie of the block
  */
  void SetReceiptsRoot(const std::string &receiptsRoot);
  /**
   * Get the root of the receipts trie of the block
   * @return the root of the receipts trie of the block
   */
  std::string GetReceiptsRoot() const;

  /**
  * Set unix timestamp for when the block was collated
  * @param ts unix timestamp for when the block was collated
  */
  void SetTimestamp(const uint64_t& ts);
  /**
   * Get the unix timestamp for when the block was collated
   * @return unix timestamp for when the block was collated
   */
  uint64_t GetTimestamp() const;

  /**
  * Set list of transaction data
  * @param tdl list of transaction data
  */
  void SetTransactionDataList(const std::vector<TransactionData>& tDataList);
  /**
   * Get list of transaction data
   * @return transaction data list
   */
  std::vector<TransactionData> GetTransactionDataList() const;

  /**
  * Set list of uncle hashes
  * @param uhList list of uncle hashes
  */
  void SetUncleHashesList(const std::vector<std::string>& uhList);
  /**
   * Get list of uncle hashes
   * @return list of uncle hashes
   */
  std::vector<std::string> GetUncleHashesList() const;

protected:

  /** The block number. -1 when its pending block. */
  int blockNumber_ = -1;

  /** Hash of the block */
  std::string hash_;

  /** hash of the parent block */
  std::string parentHash_;

  /** hash of the generated proof-of-work */
  std::string nonce_;

  /** SHA3 of the uncles data in the block. */
  std::string sha3Uncles_;

  /** the root of the transaction trie of the block */
  std::string transactionsRoot_;

  /** the root of the final state trie of the block */
  std::string stateRoot_;

  /** the root of the receipts trie of the block */
  std::string receiptsRoot_;

  /** the unix timestamp for when the block was collated */
  uint64_t timestamp_ = 0;

  /** List of transactions */
  std::vector<TransactionData> transactionDataList_;

  /** Vector of uncle hashes */
  std::vector<std::string> uncles_;
};
}

#endif //TRANSACTIONBLOCKS_H
