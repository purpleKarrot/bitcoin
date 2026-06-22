#include "observers.h"

OutpointObservers::OutpointObservers(
  clang::StringRef CheckName, clang::tidy::ClangTidyContext* Context)
  : ObserversBase(CheckName, Context)
{
  ClassName = "COutPoint";
  MemberToAccessor["n"] = "GetIndex()";
  MemberToAccessor["hash"] = "GetTxid()";
}

TxObservers::TxObservers(
  clang::StringRef CheckName, clang::tidy::ClangTidyContext* Context)
  : ObserversBase(CheckName, Context)
{
  ClassName = "CTransaction";
  MemberToAccessor["vin"] = "GetInputs()";
  MemberToAccessor["vout"] = "GetOutputs()";
  MemberToAccessor["version"] = "GetVersion()";
  MemberToAccessor["nLockTime"] = "GetLockTime()";
}
