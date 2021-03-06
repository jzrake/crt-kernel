#pragma once
#include "crt-expr.hpp"
#include "immer/map.hpp"




//=============================================================================
namespace crt {
    class context;
}




//=============================================================================
/**
 * This class extends an immutable map<std::string, crt::expression> to serve
 * as a dependency graph, providing methods to quickly retrieve dependencies
 * between the contained expressions. In particular, it makes the
 * determination of downstream rules (the ones directly or indirectly
 * referencing another) fast by maintaining outgoing edges. This is
 * just-as-good as computing the topological sort on each insertion. Worst
 * case, the insertion of a new rule is O(N+S) in the number N of existing
 * items, and the number of symbols (incoming edge count) S immediately
 * referenced by the inserted item. Replacement of existing rules is O(S).
 */
class crt::context
{
public:


    //=========================================================================
    using map_t = immer::map<std::string, crt::expression>;
    using set_t = immer::set<std::string>;
    using dag_t = immer::map<std::string, set_t>;


    /**
     * Factory method to load from a context from a source string.
     */
    static context parse(std::string source)
    {
        auto c = context();

        for (const auto& e : crt::parse(source))
        {
            if (! e->key().empty())
            {
                c = std::move(c).insert(e);
            }
        }
        return c;
    }


    /**
     * Default constructor
     */
    context() {}


    bool operator==(const context& other) const
    {
        return items == other.items;
    }


    bool operator!=(const context& other) const
    {
        return items != other.items;
    }


    /**
     * Insert the given expression into the context, using its keyword as the
     * name. std::invalid_argument is thrown if the addition would form a
     * dependency cycle.
     */
    context insert(crt::expression e) const
    {
        if (cyclic(e))
        {
            throw std::invalid_argument("would create dependency cycle");
        }

        auto k = e.key();

        return {
            items.set(k, e),
            incoming.set(k, e.symbols()),
            add_through(remove_through(outgoing, get(k)), e).set(k, get_outgoing(k)),
        };
    }


    /**
     * Erase the item with the given key, if it exists.
     */
    context erase(std::string k) const
    {
        return {
            items.erase(k),
            incoming.erase(k),
            remove_through(outgoing, get(k)).erase(k),
        };
    }


    /**
     * Erase any item whose key is in the given set.
     */
    context erase(const set_t& keys) const
    {
        auto result = *this;

        for (auto key : keys)
        {
            result = std::move(result).erase(key);
        }
        return result;
    }


    /**
     * Return the incoming edges for the given rule. An empty set is returned
     * if the key does not exist in the graph.
     */
    set_t get_incoming(std::string key) const
    {
        if (items.count(key))
        {
            return incoming.at(key);
        }
        return {};
    }


    /**
     * Return the outgoing edges for the given rule. Even if that rule does
     * not exist in the graph, it may have outgoing edges if other rules in
     * the graph name it as a dependency. If it does exist in the graph, this
     * is a fast operation because the outgoing edges are kept up-to-date.
     */
    set_t get_outgoing(std::string key) const
    {
        if (items.count(key))
        {
            return outgoing.at(key);
        }

        set_t out;

        for (const auto& item : items)
        {
            if (get_incoming(item.first).count(key))
            {
                out = std::move(out).insert(item.first);
            }
        }
        return out;
    }


    /** Return 1 if the given key is in the map. */
    auto count(std::string key) const
    {
        return items.count(key);
    }

    bool empty() const
    {
        return items.size() == 0;
    }

    /**
     * Return true if addition of the given rule would create a dependency
     * cycle in the graph. This checks for whether any of the expression's
     * symbols are downstream of its key.
     */
    bool cyclic(expression e) const
    {
        auto dependents = referencing(e.key());

        for (const auto& s : e.symbols())
        {
            if (dependents.count(s))
            {
                return true;
            }
        }
        return false;
    }


    /**
     * Return the names of items in this context that reference (directly or
     * indirectly) the given key.
     */
    set_t referencing(std::string key) const
    {
        auto result = get_outgoing(key);

        for (const auto& k : get_outgoing(key))
        {
            for (const auto& m : referencing(k))
            {
                result = std::move(result).insert(m);
            }
        }
        return result.insert(key);
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
     * Return a const reference to the expression at the given key. Throw
     * std::out_of_range if it does not exist at all.
     */
    const crt::expression& at(std::string k) const
    {
        return items.at(k);
    }


    /**
     * Return the expression at the given key if it exists, or an empty one
     * with that key otherwise.
     */
    crt::expression get(std::string k) const
    {
        return items.count(k) ? items.at(k) : crt::expression().keyed(k);
    }


    /**
     * Return the key at the given linear index. The items are not sorted, but
     * if you are displaying this context somewhere using its internal
     * ordering, this method will find the key at the displayed index. Note
     * the search is O(N) in the context size. Returns an empty string if the
     * index is larger than or equal to the number of items.
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
     * Return an expression built from the items in this context.
     */
    expression expr() const
    {
        auto e = crt::expression();

        for (auto item : items)
        {
            e = std::move(e).append(item.second);
        }
        return e;
    }


    //=========================================================================
    /**
     * Internal constructor
     */
    context(map_t items, dag_t incoming, dag_t outgoing)
    : items(items)
    , incoming(incoming)
    , outgoing(outgoing)
    {
    }


    /**
     * o[s] -= e.key for s in e.symbols if s in o
     */
    static dag_t remove_through(dag_t o, expression e)
    {
        for (const auto& s : e.symbols())
        {
            if (o.count(s))
            {
                o = std::move(o).set(s, o.at(s).erase(e.key()));
            }
        }
        return o;
    }


    /**
     * o[s] += e.key for s in e.symbols if s in o
     */
    static dag_t add_through(dag_t o, expression e)
    {
        for (const auto& s : e.symbols())
        {
            if (o.count(s))
            {
                o = std::move(o).set(s, o.at(s).insert(e.key()));
            }
        }
        return o;
    }


    map_t items;
    dag_t incoming;
    dag_t outgoing;
};




//=============================================================================
#ifdef TEST_CONTEXT
#include "catch.hpp"
using namespace crt;




//=============================================================================
TEST_CASE("context maintains DAG correctly", "[context]")
{
    SECTION("for a linear graph (A=B B=C)")
    {
        auto c = context()
        .insert(symbol("B").keyed("A"))
        .insert(symbol("C").keyed("B"));

        REQUIRE(c.size() == 2);
        REQUIRE(c.get_incoming("A") == context::set_t().insert("B"));
        REQUIRE(c.get_incoming("B") == context::set_t().insert("C"));
        REQUIRE(c.get_outgoing("B") == context::set_t().insert("A"));
        REQUIRE(c.get_outgoing("C") == context::set_t().insert("B"));

        REQUIRE(c.erase("A").get_incoming("A") == context::set_t());
        REQUIRE(c.erase("B").get_incoming("A") == context::set_t().insert("B"));
        REQUIRE(c.erase("B").get_outgoing("B") == context::set_t().insert("A"));

        REQUIRE(c.referencing("C") == context::set_t().insert("A").insert("B").insert("C"));
    }
    SECTION("for a branching graph A=(B C)")
    {
        auto c = context().insert(expression({symbol("B"), symbol("C")}).keyed("A"));
        REQUIRE(c.size() == 1);
        REQUIRE(c.get_incoming("A") == context::set_t().insert("B").insert("C"));
        REQUIRE(c.get_outgoing("B") == context::set_t().insert("A"));
        REQUIRE(c.get_outgoing("C") == context::set_t().insert("A"));

        REQUIRE(c.referencing("C") == context::set_t().insert("A").insert("C"));
        REQUIRE(c.referencing("B") == context::set_t().insert("A").insert("B"));
    }
    SECTION("same behavior when loading from expression source")
    {
        auto c = context::parse("(D=E C=D B=C A=B)");
        REQUIRE(c.get_incoming("A") == context::set_t().insert("B"));
        REQUIRE(c.get_incoming("B") == context::set_t().insert("C"));
        REQUIRE(c.get_incoming("C") == context::set_t().insert("D"));
        REQUIRE(c.get_incoming("D") == context::set_t().insert("E"));
        REQUIRE(c.get_outgoing("B") == context::set_t().insert("A"));
        REQUIRE(c.get_outgoing("C") == context::set_t().insert("B"));
        REQUIRE(c.get_outgoing("D") == context::set_t().insert("C"));
        REQUIRE(c.get_outgoing("E") == context::set_t().insert("D"));
    }
}



#endif // TEST_CONTEXT
