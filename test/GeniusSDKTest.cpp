#include <gtest/gtest.h>
#include "GeniusSDK.h"
namespace sgns
{

TEST(KeyGenerationTest, TestClass)
{
  const std::string expected_seed = "fbcc5229ade0c0ff018cb7a329c5459f91876e4dde2a97ddf03c832eab7f2612";

  const std::string expected_privateKey = "102c14f85195d15ecc07bd81bf99093a02f4b31e38dd32a4c867e1cf4d861955"
                                          "e60d95beabd0c2b3876cb72e4bf0ceafacf5e1abbd61b78d4c19468986630b4c";

  const std::string expected_publicKey = "38db45d0b3c3f82b2e7fd4f80c47923f788a45efdae0aca1e8d90e35b587585f";

  const std::string passphrase = "Substrate";
  std::vector<std::string> mnemonic =
      std::vector<std::string>({"gravity", "machine", "north", "sort", "system", "female", "filter", "attitude",
                                "volume", "fold", "club", "stay", "feature", "office", "ecology", "stable", "narrow",
                                "fog"});
  EXPECT_EQ(mnemonic.size(), 18);

  std::string seed;
  EXPECT_EQ(GeniusSDK::GetSeed(mnemonic, passphrase, seed), 0);
  EXPECT_EQ(expected_seed, seed);

  std::string privateKey;
  EXPECT_EQ(GeniusSDK::GetPrivateKey(mnemonic, passphrase, privateKey), 0);
  EXPECT_EQ(expected_privateKey, privateKey);

  std::string publicKey;
  EXPECT_EQ(GeniusSDK::GetPublicKey(mnemonic, passphrase, publicKey), 0);
  EXPECT_EQ(expected_publicKey, publicKey);
}

}
