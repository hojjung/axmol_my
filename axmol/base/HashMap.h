/****************************************************************************
Copyright (c) 2019-present Axmol Engine contributors (see AUTHORS.md).

https://axmol.dev/

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <entt/container/dense_map.hpp>
#include <entt/container/dense_set.hpp>

namespace ax
{

namespace detail
{

constexpr std::size_t mixPointerHash(std::size_t value) noexcept
{
    if constexpr (sizeof(std::size_t) == 8)
    {
        value ^= value >> 30;
        value *= static_cast<std::size_t>(0xbf58476d1ce4e5b9ULL);
        value ^= value >> 27;
        value *= static_cast<std::size_t>(0x94d049bb133111ebULL);
        value ^= value >> 31;
    }
    else
    {
        value ^= value >> 16;
        value *= static_cast<std::size_t>(0x85ebca6bU);
        value ^= value >> 13;
        value *= static_cast<std::size_t>(0xc2b2ae35U);
        value ^= value >> 16;
    }

    return value;
}

}  // namespace detail

template <class Pointer>
struct PointerHash
{
    static_assert(std::is_pointer_v<Pointer>);

    std::size_t operator()(Pointer value) const noexcept { return detail::mixPointerHash(std::hash<Pointer>{}(value)); }
};

template <class Value, bool = std::is_pointer_v<Value>>
struct DefaultHash : std::hash<Value>
{};

template <class Value>
struct DefaultHash<Value, true> : PointerHash<Value>
{};

template <class Key,
          class Value,
          class Hash      = DefaultHash<Key>,
          class KeyEqual  = std::equal_to<>,
          class Allocator = std::allocator<std::pair<const Key, Value>>>
using HashMap = entt::dense_map<Key, Value, Hash, KeyEqual, Allocator>;

template <class Value,
          class Hash      = DefaultHash<Value>,
          class KeyEqual  = std::equal_to<>,
          class Allocator = std::allocator<Value>>
using HashSet = entt::dense_set<Value, Hash, KeyEqual, Allocator>;

struct StringHash
{
    using is_transparent = void;

    std::size_t operator()(std::string_view value) const noexcept { return std::hash<std::string_view>{}(value); }
};

template <class Value, class Allocator = std::allocator<std::pair<const std::string, Value>>>
using StringHashMap = HashMap<std::string, Value, StringHash, std::equal_to<>, Allocator>;

}  // namespace ax
