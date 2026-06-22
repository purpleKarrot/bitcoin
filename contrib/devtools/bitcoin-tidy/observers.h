#pragma once

#include "observers-base.h"

class OutpointObservers : public ObserversBase
{
public:
  OutpointObservers(
    clang::StringRef CheckName, clang::tidy::ClangTidyContext* Context);
};

class TxObservers : public ObserversBase
{
public:
  TxObservers(
    clang::StringRef CheckName, clang::tidy::ClangTidyContext* Context);
};
