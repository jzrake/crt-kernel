#pragma once
#include "crt-expr.hpp"
#include "crt-context.hpp"




//=============================================================================
namespace crt {
    template <typename Range, typename T, typename Fn>
    T accumulate(Range&& r, T init, Fn fn);

    inline auto insert_invalidate(expression e, context rules, context prods);
    inline context resolve_once(expression e, context prods);
    inline context resolve_all_once(context rules, context prods={});
    inline context resolve_all(context rules, crt::context prods={});
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

crt::context crt::resolve_once(crt::expression e, crt::context prods)
{
    if (! prods.contains(e.key()))
    {
        if (e.symbols().size() == 0)
        {
            return prods.insert(e);
        }
        else if (prods.contains(e.symbols()))
        {
            return prods.insert(e.resolve(prods, crt::call_adapter()));
        }
    }
    return prods;
}

crt::context crt::resolve_all_once(crt::context rules, crt::context prods)
{
    auto trans = [] (auto p, auto i)
    {
        return resolve_once(i.second, p);
    };
    return crt::accumulate(rules, prods, trans);
}

crt::context crt::resolve_all(crt::context rules, crt::context prods)
{
    while (true)
    {
        auto new_prods = resolve_all_once(rules, prods);

        if (new_prods.size() == prods.size())
        {
            break;
        }
        prods = new_prods;
    }
    return prods;
}
