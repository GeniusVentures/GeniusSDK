#include <gtest/gtest.h>
#include "TransactionData.h"
namespace sgns
{

TEST(TransactionDataTest, TestClass)
{
  std::string sourceAddress = "0xb60e8dd61c5d32be8058bb8eb970870f07233155";
  std::string destinationAddress = "0xd46e8dd67c5d32be8058bb8eb970870f07244567";
  std::string data = "0xd46e8dd67c5d32be8d46e8dd67c5d32be8058bb8eb970870f072445675058bb8eb970870f072445675";
  double amount = 0.000003;

  TransactionData tdata_1;
  TransactionData tdata_2;

  EXPECT_TRUE(tdata_1 == tdata_2);

  EXPECT_NE(sourceAddress, tdata_1.GetSourceAddress());
  tdata_1.SetSourceAddress(sourceAddress);
  EXPECT_EQ(sourceAddress, tdata_1.GetSourceAddress());

  EXPECT_NE(destinationAddress, tdata_1.GetDestinationAddress());
  tdata_1.SetDestinationAddress(destinationAddress);
  EXPECT_EQ(destinationAddress, tdata_1.GetDestinationAddress());

  EXPECT_NE(amount, tdata_1.GetAmount());
  tdata_1.SetAmount(amount);
  EXPECT_EQ(amount, tdata_1.GetAmount());

  EXPECT_NE(data, tdata_1.GetData());
  tdata_1.SetData(data);
  EXPECT_EQ(data, tdata_1.GetData());

  EXPECT_TRUE(tdata_1 != tdata_2);
  tdata_2 = tdata_1;
  EXPECT_TRUE(tdata_1 == tdata_2);
  EXPECT_TRUE(tdata_1 == TransactionData(tdata_1));

  tdata_2.SetAmount(amount + 0.00000000001);
  EXPECT_TRUE(tdata_1 != tdata_2);

}
}