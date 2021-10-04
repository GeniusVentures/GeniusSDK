#include "TransactionBlocks.h"

namespace sgns
{

TransactionBlocks::TransactionBlocks(const TransactionBlocks &tBlocks)
{
  *this = tBlocks;
}

TransactionBlocks &TransactionBlocks::operator=(const TransactionBlocks &tBlocks)
{
  if (this != &tBlocks)
  {
    this->blockNumber_ = tBlocks.blockNumber_;
    this->hash_ = tBlocks.hash_;
    this->parentHash_ = tBlocks.parentHash_;
    this->nonce_ = tBlocks.nonce_;
    this->sha3Uncles_ = tBlocks.sha3Uncles_;
    this->transactionsRoot_ = tBlocks.transactionsRoot_;
    this->stateRoot_ = tBlocks.stateRoot_;
    this->receiptsRoot_ = tBlocks.receiptsRoot_;
    this->timestamp_ = tBlocks.timestamp_;
    this->transactionDataList_ = tBlocks.transactionDataList_;
    this->uncles_ = tBlocks.uncles_;
  }
  return *this;
}

bool TransactionBlocks::operator==(const TransactionBlocks &tBlocks) const
{
  bool returnEqual = true;
  returnEqual &= this->blockNumber_ == tBlocks.blockNumber_;
  returnEqual &= this->hash_ == tBlocks.hash_;
  returnEqual &= this->parentHash_ == tBlocks.parentHash_;
  returnEqual &= this->nonce_ == tBlocks.nonce_;
  returnEqual &= this->sha3Uncles_ == tBlocks.sha3Uncles_;
  returnEqual &= this->transactionsRoot_ == tBlocks.transactionsRoot_;
  returnEqual &= this->stateRoot_ == tBlocks.stateRoot_;
  returnEqual &= this->receiptsRoot_ == tBlocks.receiptsRoot_;
  returnEqual &= this->timestamp_ == tBlocks.timestamp_;
  returnEqual &= std::equal(this->transactionDataList_.begin(), this->transactionDataList_.end(), tBlocks.transactionDataList_.begin());
  returnEqual &= std::equal(this->uncles_.begin(), this->uncles_.end(), tBlocks.uncles_.begin());
  return returnEqual;
}

bool TransactionBlocks::operator!=(const TransactionBlocks &tBlocks) const
{
  return !(*this == tBlocks);
}

void TransactionBlocks::SetBlockNumber(const int &blockNumber)
{
  this->blockNumber_ = blockNumber;
}

int TransactionBlocks::GetBlockNumber() const
{
  return this->blockNumber_;
}

void TransactionBlocks::SetHash(const std::string &hash)
{
  this->hash_ = hash;
}

std::string TransactionBlocks::GetHash() const
{
  return this->hash_;
}

void TransactionBlocks::SetParentHash(const std::string &hash)
{
  this->parentHash_ = hash;
}

std::string TransactionBlocks::GetParentHash() const
{
  return this->parentHash_;
}

void TransactionBlocks::SetNonce(const std::string &nonce)
{
  this->nonce_ = nonce;
}

std::string TransactionBlocks::GetNonce() const
{
  return this->nonce_;
}

void TransactionBlocks::SetSha3Uncles(const std::string &sha3Uncles)
{
  this->sha3Uncles_ = sha3Uncles;
}

std::string TransactionBlocks::GetSha3Uncles() const
{
  return this->sha3Uncles_;
}

void TransactionBlocks::SetTransactionsRoot(const std::string &transactionsRoot)
{
  this->transactionsRoot_ = transactionsRoot;
}

std::string TransactionBlocks::GetTransactionsRoot() const
{
  return this->transactionsRoot_;
}

void TransactionBlocks::SetStateRoot(const std::string &stateRoot)
{
  this->stateRoot_ = stateRoot;
}

std::string TransactionBlocks::GetStateRoot() const
{
  return this->stateRoot_;
}

void TransactionBlocks::SetReceiptsRoot(const std::string &receiptsRoot)
{
  this->receiptsRoot_ = receiptsRoot;
}

std::string TransactionBlocks::GetReceiptsRoot() const
{
  return this->receiptsRoot_;
}

void TransactionBlocks::SetTimestamp(const uint64_t &ts)
{
  this->timestamp_ = ts;
}

uint64_t TransactionBlocks::GetTimestamp() const
{
  return this->timestamp_;
}

void TransactionBlocks::SetTransactionDataList(const std::vector<TransactionData>& tDataList)
{
  this->transactionDataList_ = tDataList;
}

std::vector<TransactionData> TransactionBlocks::GetTransactionDataList() const
{
  return this->transactionDataList_;
}

void TransactionBlocks::SetUncleHashesList(const std::vector<std::string> &uhList)
{
  this->uncles_ = uhList;
}

std::vector<std::string> TransactionBlocks::GetUncleHashesList() const
{
  return this->uncles_;
}

}