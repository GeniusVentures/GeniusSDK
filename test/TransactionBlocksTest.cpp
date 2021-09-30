#include <gtest/gtest.h>
#include "TransactionBlocks.h"
namespace sgns
{

TEST(TransactionBlocksTest, TestClass)
{
  std::string sourceAddress_1 = "0xb60e8dd61c5d32be8058bb8eb970870f07233155";
  std::string destinationAddress_1 = "0xd46e8dd67c5d32be8058bb8eb970870f07244567";
  std::string data_1 = "0xd46e8dd67c5d32be8d46e8dd67c5d32be8058bb8eb970870f072445675058bb8eb970870f072445675";
  double amount_1 = 0.000003;
  TransactionData tdata_1;
  tdata_1.SetSourceAddress(sourceAddress_1);
  tdata_1.SetDestinationAddress(destinationAddress_1);
  tdata_1.SetData(data_1);
  tdata_1.SetAmount(amount_1);

  std::string sourceAddress_2 = "0xd46e8dd67c5d32be8058bb8eb970870f07244567";
  std::string destinationAddress_2 = "0xb25e4568aa5d32be8058bb8eb970870f07244567";
  std::string data_2 = "";
  double amount_2 = 0.000453;
  TransactionData tdata_2;
  tdata_2.SetSourceAddress(sourceAddress_2);
  tdata_2.SetDestinationAddress(destinationAddress_2);
  tdata_2.SetData(data_2);
  tdata_2.SetAmount(amount_2);

  int blockNumber = 12442;
  std::string hash = "0x1d59ff54b1eb26b013ce3cb5fc9dab3705b415a67127a003c3e61eb445bb8df2";
  std::string parentHash = "0xe99e022112df268087ea7eafaf4790497fd21dbeeb6bd7a1721df161a6657a54";
  std::string nonce = "0x689056015818adbe";
  std::string sha3Uncles = "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347";
  std::string transactionsRoot = "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421";
  std::string stateRoot = "0xddc8b0234c2e0cad087c8b389aa7ef01f7d79b2570bccb77ce48648aa61c904d";
  std::string receiptsRoot = "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421";
  uint64_t timestamp = 1438271100;
  std::vector<TransactionData> tDataList;
  tDataList.push_back(tdata_1);
  tDataList.push_back(tdata_2);

  TransactionBlocks tBlocks;

  EXPECT_NE(blockNumber, tBlocks.GetBlockNumber());
  tBlocks.SetBlockNumber(blockNumber);
  EXPECT_EQ(blockNumber, tBlocks.GetBlockNumber());

  EXPECT_NE(hash, tBlocks.GetHash());
  tBlocks.SetHash(hash);
  EXPECT_EQ(hash, tBlocks.GetHash());

  EXPECT_NE(parentHash, tBlocks.GetParentHash());
  tBlocks.SetParentHash(parentHash);
  EXPECT_EQ(parentHash, tBlocks.GetParentHash());

  EXPECT_NE(nonce, tBlocks.GetNonce());
  tBlocks.SetNonce(nonce);
  EXPECT_EQ(nonce, tBlocks.GetNonce());

  EXPECT_NE(sha3Uncles, tBlocks.GetSha3Uncles());
  tBlocks.SetSha3Uncles(sha3Uncles);
  EXPECT_EQ(sha3Uncles, tBlocks.GetSha3Uncles());

  EXPECT_NE(transactionsRoot, tBlocks.GetTransactionsRoot());
  tBlocks.SetTransactionsRoot(transactionsRoot);
  EXPECT_EQ(transactionsRoot, tBlocks.GetTransactionsRoot());

  EXPECT_NE(stateRoot, tBlocks.GetStateRoot());
  tBlocks.SetStateRoot(stateRoot);
  EXPECT_EQ(stateRoot, tBlocks.GetStateRoot());

  EXPECT_NE(receiptsRoot, tBlocks.GetReceiptsRoot());
  tBlocks.SetReceiptsRoot(receiptsRoot);
  EXPECT_EQ(receiptsRoot, tBlocks.GetReceiptsRoot());

  EXPECT_NE(timestamp, tBlocks.GetTimestamp());
  tBlocks.SetTimestamp(timestamp);
  EXPECT_EQ(timestamp, tBlocks.GetTimestamp());

  bool isEqual = std::equal(tDataList.begin(), tDataList.end(), tBlocks.GetTransactionDataList().begin());
  EXPECT_FALSE(isEqual);
  tBlocks.SetTransactionDataList(tDataList);
  isEqual = std::equal(tDataList.begin(), tDataList.end(), tBlocks.GetTransactionDataList().begin());
  EXPECT_TRUE(isEqual);

}
}