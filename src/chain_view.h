// Copyright (c) 2026 Daniel Pfeifer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAIN_VIEW_H
#define BITCOIN_CHAIN_VIEW_H

#include <primitives/block.h>

#include <cassert>
#include <cstddef>
#include <ranges>
#include <type_traits>
#include <utility>

template <typename T>
concept ChainView = std::ranges::view<T> && std::ranges::sized_range<T> && std::ranges::random_access_range<T> && std::convertible_to<std::ranges::range_reference_t<T>, CBlockHeader>;

class AnyChainView : public std::ranges::view_interface<AnyChainView>
{
public:
    class iterator
    {
    public:
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::random_access_iterator_tag;
        using value_type = CBlockHeader;

        iterator() = default;
        iterator(AnyChainView& impl, difference_type index) : _impl{&impl}, _index{index} {}

        [[nodiscard]] auto operator*() const -> value_type
        {
            assert(_impl != nullptr);
            return (*_impl)[_index];
        }

        [[nodiscard]] auto operator[](difference_type n) const -> value_type
        {
            return *(*this + n);
        }

        auto operator++() -> iterator&
        {
            ++_index;
            return *this;
        }

        auto operator++(int) -> iterator
        {
            iterator tmp = *this;
            ++_index;
            return tmp;
        }

        auto operator--() -> iterator&
        {
            --_index;
            return *this;
        }

        auto operator--(int) -> iterator
        {
            iterator tmp = *this;
            --_index;
            return tmp;
        }

        auto operator+=(difference_type n) -> iterator&
        {
            _index += n;
            return *this;
        }

        auto operator-=(difference_type n) -> iterator&
        {
            _index -= n;
            return *this;
        }

        friend auto operator+(iterator it, difference_type n) -> iterator
        {
            return it += n;
        }

        friend auto operator+(difference_type n, iterator it) -> iterator
        {
            return it += n;
        }

        friend auto operator-(iterator it, difference_type n) -> iterator
        {
            return it + (-n);
        }

        friend auto operator-(iterator lhs, iterator rhs) -> difference_type
        {
            assert(lhs._impl == rhs._impl);
            return lhs._index - rhs._index;
        }

        friend constexpr bool operator==(iterator lhs, iterator rhs) noexcept
        {
            assert(lhs._impl == rhs._impl);
            return lhs._index == rhs._index;
        }

        friend constexpr auto operator<=>(iterator lhs, iterator rhs) noexcept
        {
            assert(lhs._impl == rhs._impl);
            return lhs._index <=> rhs._index;
        }

    private:
        AnyChainView* _impl = nullptr;
        difference_type _index = 0;
    };

    AnyChainView() = default;
    AnyChainView(AnyChainView const& other) : _vtable{other._vtable} { _vtable->copy(other.storage(), storage()); }
    AnyChainView(AnyChainView&& other) noexcept : _vtable{std::exchange(other._vtable, std::addressof(empty_vtable))} { _vtable->move(other.storage(), storage()); }

    auto operator=(AnyChainView const& other) -> AnyChainView&
    {
        if (this != std::addressof(other)) {
            _vtable->destroy(storage());
            _vtable = other._vtable;
            _vtable->copy(other.storage(), storage());
        }
        return *this;
    }

    auto operator=(AnyChainView&& other) noexcept -> AnyChainView&
    {
        if (this != std::addressof(other)) {
            _vtable->destroy(storage());
            _vtable = std::exchange(other._vtable, std::addressof(empty_vtable));
            _vtable->move(other.storage(), storage());
        }
        return *this;
    }

    ~AnyChainView() { _vtable->destroy(storage()); }

    template <typename T>
        requires(!std::same_as<std::remove_cvref_t<T>, AnyChainView> && ChainView<std::remove_cvref_t<T>>)
    explicit AnyChainView(T&& object)
    {
        if (std::ranges::empty(object)) {
            return;
        }

        using value_type = std::remove_cvref_t<T>;
        construct<value_type>(std::forward<T>(object), vtable_for<value_type>);
    }

    [[nodiscard]] std::size_t size() { return table().size(storage()); }
    [[nodiscard]] CBlockHeader operator[](std::size_t index) { return table().get(storage(), index); }

    [[nodiscard]] iterator begin() { return iterator(*this, 0); }
    [[nodiscard]] iterator end() { return iterator(*this, size()); }

private:
    struct vtable {
        void (*copy)(void const* src, void* dst);
        void (*move)(void* src, void* dst) noexcept;
        void (*destroy)(void* storage) noexcept;
        std::size_t (*size)(void* storage);
        CBlockHeader (*get)(void* storage, std::size_t index);
    };

    static inline auto const empty_vtable = vtable{
        .copy = [](void const*, void*) {},
        .move = [](void*, void*) noexcept {},
        .destroy = [](void*) noexcept {},
        .size = [](void*) -> std::size_t { return 0; },
        .get = nullptr,
    };

    static constexpr std::size_t storage_size = 3 * sizeof(void*);
    static_assert(storage_size >= sizeof(std::shared_ptr<void>));

    static constexpr std::size_t storage_align = alignof(std::max_align_t);
    static_assert(storage_align >= alignof(std::shared_ptr<void>));

    template <typename T>
    static constexpr bool stores_inline = (sizeof(T) <= storage_size) && (alignof(T) <= storage_align) && std::copy_constructible<T> && std::is_nothrow_move_constructible_v<T>;

    template <typename T>
    using storage_t = std::conditional_t<stores_inline<T>, T, std::shared_ptr<T>>;

    template <typename T>
    [[nodiscard]] static auto raw_storage(void* p) noexcept -> storage_t<T>*
    {
        return reinterpret_cast<storage_t<T>*>(p);
    }

    template <typename T>
    [[nodiscard]] static auto raw_storage(void const* p) noexcept -> storage_t<T> const*
    {
        return reinterpret_cast<storage_t<T> const*>(p);
    }

    template <typename T>
    [[nodiscard]] static auto storage(void* p) noexcept -> storage_t<T>*
    {
        return std::launder(raw_storage<T>(p));
    }

    template <typename T>
    [[nodiscard]] static auto storage(void const* p) noexcept -> storage_t<T> const*
    {
        return std::launder(raw_storage<T>(p));
    }

    template <typename T>
    [[nodiscard]] static auto object(void* p) noexcept -> T&
    {
        if constexpr (stores_inline<T>) {
            return *storage<T>(p);
        } else {
            return **storage<T>(p);
        }
    }

    template <typename T, typename U>
    static void construct_storage(void* p, U&& value)
    {
        if constexpr (stores_inline<T>) {
            std::construct_at(raw_storage<T>(p), std::forward<U>(value));
        } else {
            std::construct_at(raw_storage<T>(p), std::make_shared<T>(std::forward<U>(value)));
        }
    }

    template <typename T, typename U>
    void construct(U&& value, vtable const& table)
    {
        construct_storage<T>(storage(), std::forward<U>(value));
        _vtable = std::addressof(table);
    }

    [[nodiscard]] auto storage() noexcept -> void* { return _storage.data(); }
    [[nodiscard]] auto storage() const noexcept -> void const* { return _storage.data(); }

    [[nodiscard]] auto table() const noexcept -> vtable const& { return *_vtable; }

    template <typename T>
    static inline auto const vtable_for = vtable{
        .copy = [](void const* src, void* dst) { std::construct_at(raw_storage<T>(dst), *storage<T>(src)); },
        .move = [](void* src, void* dst) noexcept { std::construct_at(raw_storage<T>(dst), std::move(*storage<T>(src))); std::destroy_at(storage<T>(src)); },
        .destroy = [](void* p) noexcept { std::destroy_at(storage<T>(p)); },
        .size = [](void* p) -> std::size_t { return std::ranges::size(object<T>(p)); },
        .get = [](void* p, std::size_t i) -> CBlockHeader { return std::ranges::begin(object<T>(p))[i]; },
    };

    alignas(storage_align) std::array<std::byte, storage_size> _storage{};
    vtable const* _vtable = std::addressof(empty_vtable);
};

static_assert(ChainView<AnyChainView>);

#endif // BITCOIN_CHAIN_VIEW_H
