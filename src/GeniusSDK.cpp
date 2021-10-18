#include "GeniusSDK.h"
#include "crdt/crdt_datastore.hpp"
#include "crdt/hierarchical_key.hpp"
#include <ed25519/ed25519.h>
#include "base/blob.hpp"
#include <storage/rocksdb/rocksdb.hpp>
#include <outcome/outcome.hpp>
#include <ipfs_lite/ipfs/merkledag/impl/merkledag_service_impl.hpp>
#include <ipfs_lite/ipfs/impl/in_memory_datastore.hpp>
#include <boost/algorithm/string/join.hpp>
#include "crypto/bip39/impl/bip39_provider_impl.hpp"
#include "crypto/bip39/mnemonic.hpp"
#include "crypto/pbkdf2/impl/pbkdf2_provider_impl.hpp"
#include "crypto/sr25519_types.hpp"
#include "crypto/sr25519/sr25519_provider_impl.hpp"
#include "crypto/random_generator/boost_generator.hpp"

using sgns::base::Buffer;
using sgns::ipfs_lite::ipld::IPLDNode;
using sgns::ipfs_lite::ipfs::IpfsDatastore;
using sgns::ipfs_lite::ipfs::InMemoryDatastore;
using sgns::crdt::CrdtDatastore;
using sgns::crdt::CrdtOptions;
using sgns::crdt::HierarchicalKey;
using CrdtBuffer = CrdtDatastore::Buffer;
using Delta = CrdtDatastore::Delta;
using sgns::crypto::Pbkdf2ProviderImpl;
using sgns::crypto::Bip39ProviderImpl;
using sgns::crypto::SR25519Provider;

namespace sgns
{

class CustomDagSyncer: public crdt::DAGSyncer
{
public:
  using IpfsDatastore = ipfs_lite::ipfs::IpfsDatastore;
  using MerkleDagServiceImpl = ipfs_lite::ipfs::merkledag::MerkleDagServiceImpl;
  using Leaf = ipfs_lite::ipfs::merkledag::Leaf;

  CustomDagSyncer(std::shared_ptr<IpfsDatastore> service)
      : dagService_(service)
  {
  }

  outcome::result<bool> HasBlock(const CID &cid) const override
  {
    auto getNodeResult = dagService_.getNode(cid);
    return getNodeResult.has_value();
  }

  outcome::result<void> addNode(std::shared_ptr<const IPLDNode> node) override
  {
    return dagService_.addNode(node);
  }

  outcome::result<std::shared_ptr<IPLDNode>> getNode(const CID &cid) const override
  {
    return dagService_.getNode(cid);
  }

  outcome::result<void> removeNode(const CID &cid) override
  {
    return dagService_.removeNode(cid);
  }

  outcome::result<size_t> select(
      gsl::span<const uint8_t> root_cid,
      gsl::span<const uint8_t> selector,
      std::function<bool(std::shared_ptr<const IPLDNode> node)> handler) const override
  {
    return dagService_.select(root_cid, selector, handler);
  }

  outcome::result<std::shared_ptr<Leaf>> fetchGraph(const CID &cid) const override
  {
    return dagService_.fetchGraph(cid);
  }

  outcome::result<std::shared_ptr<Leaf>> fetchGraphOnDepth(const CID &cid, uint64_t depth) const override
  {
    return dagService_.fetchGraphOnDepth(cid, depth);
  }

  MerkleDagServiceImpl dagService_;
};

class CustomBroadcaster: public crdt::Broadcaster
{
public:
  /**
  * Send {@param buff} payload to other replicas.
  * @return outcome::success on success or outcome::failure on error
  */
  virtual outcome::result<void> Broadcast(const base::Buffer &buff) override
  {
    if (!buff.empty())
    {
      const std::string bCastData(buff.toString());
      listOfBroadcasts_.push(bCastData);
    }
    return outcome::success();
  }

  /**
  * Obtain the next {@return} payload received from the network.
  * @return buffer value or outcome::failure on error
  */
  virtual outcome::result<base::Buffer> Next() override
  {
    if (listOfBroadcasts_.empty())
    {
      //Broadcaster::ErrorCode::ErrNoMoreBroadcast
      return outcome::failure(boost::system::error_code{});
    }

    std::string strBuffer = listOfBroadcasts_.front();
    listOfBroadcasts_.pop();

    base::Buffer buffer;
    buffer.put(strBuffer);
    return buffer;
  }

  std::queue<std::string> listOfBroadcasts_;
};

GeniusSDK::GeniusSDK()
{

}

int GeniusSDK::GetSeed(const std::vector<std::string> &mnemonic, const std::string &passphrase, std::string &seed)
{
  auto pbkdf2_provider = std::make_shared<Pbkdf2ProviderImpl>();
  auto bip39_provider = std::make_shared<Bip39ProviderImpl>(pbkdf2_provider);

  auto entropyResult = bip39_provider->calculateEntropy(mnemonic);
  if (entropyResult.has_failure())
  {
    return 1;
  }

  auto bip39SeedResult = bip39_provider->makeSeed(entropyResult.value(), passphrase);
  if (bip39SeedResult.has_failure())
  {
    return 1;
  }

  seed = bip39SeedResult.value().subbuffer(0, SR25519_SEED_SIZE).toHex();
  return 0;
}

int GeniusSDK::GetPublicKey(const std::vector<std::string> &mnemonic,
                            const std::string &passphrase,
                            std::string &publicKey)
{
  std::string seedHex;
  if(GeniusSDK::GetSeed(mnemonic, passphrase, seedHex) != 0)
  {
    return 1;
  }

  auto random_generator =
      std::make_shared<sgns::crypto::BoostRandomGenerator>();
  auto sr25519_provider =
      std::make_shared<sgns::crypto::SR25519ProviderImpl>(random_generator);

  auto seedResult = sgns::crypto::SR25519Seed::fromHex(seedHex);
  if(seedResult.has_failure())
  {
    return 1;
  }

  auto keyPair = sr25519_provider->generateKeypair(seedResult.value());
  publicKey = keyPair.public_key.toHex();
  return 0;
}

int GeniusSDK::GetPrivateKey(const std::vector<std::string> &mnemonic,
                             const std::string &passphrase,
                             std::string &privateKey)
{
  std::string seedHex;
  if(GeniusSDK::GetSeed(mnemonic, passphrase, seedHex) != 0)
  {
    return 1;
  }

  auto random_generator =
      std::make_shared<sgns::crypto::BoostRandomGenerator>();
  auto sr25519_provider =
      std::make_shared<sgns::crypto::SR25519ProviderImpl>(random_generator);

  auto seedResult = sgns::crypto::SR25519Seed::fromHex(seedHex);
  if(seedResult.has_failure())
  {
    return 1;
  }

  auto keyPair = sr25519_provider->generateKeypair(seedResult.value());
  privateKey = keyPair.secret_key.toHex();
  return 0;
}

  void GeniusSDK::SetDatastorePath(const std::string &aPath)
{
  if (this->crdtDatastore_ == nullptr)
  {
    // Remove leftover database
    std::string databasePath = "supergenius_crdt_datastore_test";

    // Create new database
    ::CrdtDatastore::DataStore::Options options;
    options.create_if_missing = true;  // intentionally
    auto dataStoreResult = ::CrdtDatastore::DataStore::create(databasePath, options);
    auto dataStore = dataStoreResult.value();

    // Create new DAGSyncer
    auto ipfsDataStore = std::make_shared<InMemoryDatastore>();
    auto dagSyncer = std::make_shared<CustomDagSyncer>(ipfsDataStore);

    // Create new Broadcaster
    auto broadcaster = std::make_shared<CustomBroadcaster>();

    // Define test values
    const std::string strNamespace = "/namespace";
    HierarchicalKey namespaceKey(strNamespace);

    // Create crdtDatastore
    crdtDatastore_ = std::make_shared<::CrdtDatastore>(dataStore,
                                                       namespaceKey,
                                                       dagSyncer,
                                                       broadcaster,
                                                       CrdtOptions::DefaultOptions());
  }
}

}

