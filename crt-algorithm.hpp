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
    inline auto resolution_of(context rules, context prods={}, unsigned int delay_ms=0);
    inline context resolve_only(expression e, context prods={});
    inline context resolve_once(context rules, context prods={});
    inline context resolve_full(context rules, context prods={});
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
auto crt::insert_invalidate(expression e, context rules, context prods)
{
    return std::make_pair(
        rules.insert(e),
        prods.erase(rules.referencing(e.key())));
}

/**
 * Returns a function, that when passed to observable::create, yields an
 * observable of resolutions (prods) of the given set of rules. If prods is
 * non-empty, then its entries should be up-to-date with the rules. The
 * function calls the subscriber with the products, as they mature, once per
 * resolve cycle. On each generation it checks to see if it's subscribed, and
 * if not it returns. The observable completes when the context is fully
 * resolved.
 */
auto crt::resolution_of(context rules, context prods, unsigned int delay_ms)
{
    return [rules, p=prods, delay_ms] (auto s)
    {
        auto prods = p;

        while (s.is_subscribed())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            auto new_prods = resolve_once(rules, prods);

            if (new_prods.size() == prods.size())
            {
                break;
            }
            s.on_next(prods = std::move(new_prods));
        }
        s.on_completed();
    };
}

crt::context crt::resolve_full(context rules, context prods)
{
    while (true)
    {
        auto new_prods = resolve_once(rules, prods);

        if (new_prods.size() == prods.size())
        {
            break;
        }
        prods = std::move(new_prods);
    }
    return prods;
}

crt::context crt::resolve_once(context rules, context prods)
{
    auto trans = [] (auto p, auto i)
    {
        return resolve_only(i.second, p);
    };
    return accumulate(rules, prods, trans);
}

crt::context crt::resolve_only(expression e, context prods)
{
    if (! prods.count(e.key()))
    {
        if (e.symbols().size() == 0)
        {
            return prods.insert(e);
        }
        else if (contains(prods, e.symbols()))
        {
            return prods.insert(e.resolve(prods, call_adapter()));
        }
    }
    return prods;
}
