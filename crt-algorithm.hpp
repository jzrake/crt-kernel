#pragma once
#include "crt-expr.hpp"
#include "crt-context.hpp"




//=============================================================================
namespace crt {

    template <typename Range, typename T, typename Fn>
    T accumulate(Range&& r, T init, Fn fn);

    template <typename Map, typename Set>
    bool contains(const Map& A, const Set& B);

    inline auto insert_invalidate(expression e, context rules, context prods);
    inline auto resolution_of(crt::context rules, crt::context prods={});
    inline context resolve_only(expression e, context prods);
    inline context resolve_once(context rules, context prods={});
    inline context resolve_full(context rules, crt::context prods={});
}




//=============================================================================
template <typename Range, typename T, typename Fn>
T crt::accumulate(Range&& r, T init, Fn fn)
{
    for (const auto& x : r)
    {
        init = fn(std::move(init), x);
    }
    return init;
}

template <typename Map, typename Set>
bool crt::contains(const Map& A, const Set& B)
{
    for (const auto& b : B)
    {
        if (! A.count(b))
        {
            return false;
        }
    }
    return true;
}




//=============================================================================
/**
 * Return a pair <context, map_t> with the given expression (e) inserted into
 * the context (rules), and deleting from the map (prods) any of that
 * context's rules that reference e.
 */
auto crt::insert_invalidate(crt::expression e, crt::context rules, crt::context prods)
{
    return std::make_pair(
        rules.insert(e),
        prods.erase(rules.referencing(e.key())));
}

/**
 * Returns a function, that when passed to observable::create, yields an
 * observable of resolutions (products) of the given set of rules. If products
 * is non-empty, then its entries should be up-to-date with the rules. The
 * function calls the subscriber with the products, as they mature, once per
 * resolve cycle. On each generation it checks to see if it's subscribed, and
 * if not it returns (no need to complete in that case). The observable
 * completes when the context is fully resolved.
 */
auto crt::resolution_of(crt::context rules, crt::context prods)
{
    return [rules, p=prods] (auto s)
    {
        auto prods = p;

        while (s.is_subscribed())
        {
            auto new_prods = crt::resolve_once(rules, prods);

            if (new_prods.size() == prods.size())
            {
                break;
            }
            s.on_next(prods = std::move(new_prods));
        }
        s.on_completed();
    };
}

crt::context crt::resolve_full(crt::context rules, crt::context prods)
{
    while (true)
    {
        auto new_prods = resolve_once(rules, prods);

        if (new_prods.size() == prods.size())
        {
            break;
        }
        prods = new_prods;
    }
    return prods;
}

crt::context crt::resolve_only(crt::expression e, crt::context prods)
{
    if (! prods.count(e.key()))
    {
        if (e.symbols().size() == 0)
        {
            return prods.insert(e);
        }
        else if (contains(prods, e.symbols()))
        {
            return prods.insert(e.resolve(prods, crt::call_adapter()));
        }
    }
    return prods;
}

crt::context crt::resolve_once(crt::context rules, crt::context prods)
{
    auto trans = [] (auto p, auto i)
    {
        return resolve_only(i.second, p);
    };
    return crt::accumulate(rules, prods, trans);
}
