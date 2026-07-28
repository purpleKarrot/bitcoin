// Copyright (c) 2026 Daniel Pfeifer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COIN_INDEX_H
#define BITCOIN_COIN_INDEX_H

#include <coins.h>
#include <primitives/transaction.h>

#include <concepts>
#include <functional>
#include <optional>

template <typename T>
concept CoinIndex = requires(const T& m, COutPoint p) {
    { m.lookup(p) } -> std::convertible_to<std::optional<Coin>>;
};

class CoinIndexRef
{
public:
    template <CoinIndex T>
        requires(!std::same_as<std::remove_cvref_t<T>, CoinIndexRef>)
    constexpr CoinIndexRef(const T& index) noexcept
        : _object(std::addressof(index)), _lookup([](void* object, const COutPoint& p) {
              return static_cast<const T*>(object)->lookup(p);
          })
    {
    }

    [[nodiscard]] auto lookup(const COutPoint& p) const
    {
        return _lookup(_object, p);
    }

private:
    void const* _object;
    std::optional<Coin> (*_lookup)(void*, const COutPoint&);
};


static_assert(CoinIndex<CoinIndexRef>);

#endif
