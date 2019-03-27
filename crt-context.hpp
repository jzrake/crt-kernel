#pragma once
#include "crt-expr.hpp"
#include "immer/map.hpp"




//=============================================================================
namespace crt {
    class context;
}




//=============================================================================
/**
 * This class extends an immutable map<std::string, crt::expression> with
 * methods to resolve expressions by treating itself as a scope.
 */
class crt::context
{
public:


    using map_t = immer::map<std::string, crt::expression>;
    using set_t = immer::set<std::string>;


    /** Default constructor */
    context() {}


    /** Construct a context from a map of items. */
    context(immer::map<std::string, crt::expression> items) : items(items) {}


    /**
     * Insert the given expression into the context, using its keyword as the
     * name.
     */
    context insert(crt::expression e) const
    {
        return items.set(e.key(), e);
    }


    /**
     * Erase the item with the given key, if it exists.
     */
    context erase(std::string key) const
    {
        return items.erase(key);
    }


    /**
     * Erase any item whose key is in the given set.
     */
    context erase(const set_t& keys) const
    {
        auto result = items;

        for (auto key : keys)
        {
            result = std::move(result).erase(key);
        }
        return result;
    }


    /** Return true if the given key is in the map. */
    bool contains(std::string key) const
    {
        return items.count(key);
    }


    /** Return true if all of the given keys are in the map. */
    bool contains(const set_t& keys) const
    {
        for (const auto& key : keys)
        {
            if (! items.count(key))
            {
                return false;
            }
        }
        return true;
    }


    /**
     * Return the names of items in this context that match the specified
     * key, or include it as a symbol.
     */
    set_t referencing(std::string key) const
    {
        auto result = set_t().insert(key);

        for (const auto& item : items)
        {
            if (item.second.symbols().count(key))
            {
                result = std::move(result).insert(item.first);
            }
        }
        return result;
    }


    /** Return an iterator to the beginning of the map. */
    auto begin() const
    {
        return items.begin();
    }


    /** Return an iterator to the end of the map. */
    auto end() const
    {
        return items.end();
    }


    /** Return the number of items in the map. */
    std::size_t size() const
    {
        return items.size();
    }


    /**
     * Return a const reference to the expression at the given key, or
     * throw std::out_of_range if it does not exist.
     */
    const crt::expression& at(std::string key) const
    {
        return items.at(key);
    }


    /**
     * Return the key at the given linear index. The items are not sorted,
     * but if you are displaying this context somewhere using its internal
     * ordering, this method will find the kay at the displayed index.
     * Note the search is O(N) in the context size. Returns an empty
     * string if the index is larger than or equal to the number of items.
     */
    std::string nth_key(std::size_t index) const
    {
        for (const auto& item : items)
        {
            if (! index--)
            {
                return item.first;
            }
        }
        return std::string();
    }


    /**
     * Return a context containing the resolution of all the items in this
     * one. If the key already exists in the given cache, that value is
     * used rather than resolving it.
     */
    context resolve(context cache={})
    {
        int num_resolved;

        do
        {
            num_resolved = 0;

            for (const auto& item : items)
            {
                if (cache.contains(item.first))
                {
                    continue;
                }
                else if (item.second.symbols().size() == 0)
                {
                    cache = cache.insert(item.second);
                    ++num_resolved;
                }
                else if (cache.contains(item.second.symbols()))
                {
                    cache = cache.insert(item.second.resolve(cache, call_adapter()));
                    ++num_resolved;
                }
            }
        } while (num_resolved > 0);

        return cache;
    }

private:
    map_t items;
};
