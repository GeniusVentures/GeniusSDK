#include "TransactionData.h"

namespace sgns
{

TransactionData::TransactionData(const TransactionData &tData)
{
  *this = tData;
}

TransactionData &TransactionData::operator=(const TransactionData &tData)
{
  if (this != &tData)
  {
    this->sourceAddress_ = tData.sourceAddress_;
    this->destinationAddress_ = tData.destinationAddress_;
    this->amount_ = tData.amount_;
    this->data_ = tData.data_;
  }
  return *this;
}

bool TransactionData::operator==(const TransactionData &tData) const
{
  bool returnEqual = true;
  returnEqual &= this->sourceAddress_ == tData.sourceAddress_;
  returnEqual &= this->destinationAddress_ == tData.destinationAddress_;
  returnEqual &= this->amount_ == tData.amount_;
  returnEqual &= this->data_ == tData.data_;
  return returnEqual;
}

bool TransactionData::operator!=(const TransactionData &tData) const
{
  return !(*this == tData);
}

void TransactionData::SetSourceAddress(const std::string& address)
{
  this->sourceAddress_ = address;
}

std::string TransactionData::GetSourceAddress() const
{
  return this->sourceAddress_;
}

void TransactionData::SetDestinationAddress(const std::string& address)
{
  this->destinationAddress_ = address;
}

std::string TransactionData::GetDestinationAddress() const
{
  return this->destinationAddress_;
}

void TransactionData::SetAmount(const double &amount)
{
  this->amount_ = amount;
}

double TransactionData::GetAmount() const
{
  return this->amount_;
}

void TransactionData::SetData(const std::string &data)
{
  this->data_ = data;
}

std::string TransactionData::GetData() const
{
  return this->data_;
}

}