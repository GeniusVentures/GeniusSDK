#ifndef TRANSACTIONDATA_H
#define TRANSACTIONDATA_H

#include "GeniusSDKExport.h"
#include <string>

namespace sgns
{
/** @brief
 *
 */
class GENIUSSDK_EXPORT TransactionData
    {
    public:

      /** Constructor
       */
      TransactionData() = default;

      /** Copy constructor
      */
      TransactionData(const TransactionData&);

      /** Destructor
       */
      virtual ~TransactionData() = default;

      /** Equality operator
       * @return true if equal otherwise, it returns false.
       */
      bool operator==(const TransactionData&) const;

      /** Equality operator
      * @return true if NOT equal otherwise, it returns false.
      */
      bool operator!=(const TransactionData&) const;

      /** Assignment operator
      */
      TransactionData& operator=(const TransactionData&);

      /**
       * Set source account address
       * @param address Address of the source account
       */
      void SetSourceAddress(const std::string& address);
      /**
       * Get source account address
       * @return source address
       */
      std::string GetSourceAddress() const;

      /**
       * Set destination address
       * @param address Address of the destination account
       */
      void SetDestinationAddress(const std::string& address);
      /**
       * Get destination address
       * @return destination address
       */
      std::string GetDestinationAddress() const;

      /**
       * Set transaction amount
       * @param amount Transaction amount
       */
      void SetAmount(const double& amount);
      /**
       * Get transaction amount
       * @return transaction amount
       */
      double GetAmount() const;

      /**
       * Set custom data.
       * The compiled code of a contract OR the hash of the
       * invoked method signature and encoded parameters
       * @param data custom data
       */
      void SetData(const std::string& data);
      /**
       * Get custom data
       * The compiled code of a contract OR the hash of the
       * invoked method signature and encoded parameters
       * @return custom data
       */
      std::string GetData() const;

    protected:
      std::string sourceAddress_;
      std::string destinationAddress_;
      double amount_ = 0;
      std::string data_;
    };
}

#endif //TRANSACTIONDATA_H
