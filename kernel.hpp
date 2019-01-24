#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>




// ============================================================================
namespace crt
{
    template<typename ObjectType, typename CallAdapter>
    class kernel;
    class parser;
    class expression;

    enum class data_type { none, i32, f64, str, symbol, composite };

    class parser_error : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };
};




// ============================================================================
class crt::expression
{
public:
    struct none {};

    expression()                                        : type(data_type::none) {}
    expression(const none&)                             : type(data_type::none) {}
    expression(int vali32)                              : type(data_type::i32), vali32(vali32) {}
    expression(double valf64)                           : type(data_type::f64), valf64(valf64) {}
    expression(const std::string& valstr)               : type(data_type::str), valstr(valstr) {}
    expression(std::initializer_list<expression> parts) : expression(std::vector<expression>(parts)) {}
    expression(const std::vector<expression>& parts)    : type(data_type::composite), parts(parts)
    {
        if (expression::parts.empty())
        {
            type = data_type::none;
        }
    }

    static expression symbol(const std::string& v)
    {
        auto e = expression();
        e.type = data_type::symbol;
        e.valsym = v;
        return e;
    }

    static expression composite(const std::vector<expression>& v)
    {
        auto e = expression();
        e.type = v.empty() ? data_type::none : data_type::composite;
        e.parts = v;
        return e;
    }

    expression keyed(const std::string& kw) const
    {
        auto e = *this;
        e.keyword = kw;
        return e;
    }

    data_type dtype() const
    {
        return type;
    }

    std::string key() const
    {
        return keyword;
    }

    std::vector<expression> list() const
    {
        std::vector<expression> list_parts;

        for (const auto& part : parts)
        {
            if (part.keyword.empty())
            {
                list_parts.push_back (part);
            }
        }
        return list_parts;
    }

    std::unordered_map<std::string, expression> dict() const
    {
        std::unordered_map<std::string, expression> dict_parts;

        for (const auto& part : parts)
        {
            if (! part.keyword.empty())
            {
                dict_parts[part.keyword] = part;
            }
        }
        return dict_parts;
    }

    std::unordered_set<std::string> symbols() const
    {
        switch (type)
        {
            case data_type::none     : return {};
            case data_type::i32      : return {};
            case data_type::f64      : return {};
            case data_type::str      : return {};
            case data_type::symbol   : return {valsym};
            case data_type::composite:
            {
                std::unordered_set<std::string> res;

                for (const auto& part : parts)
                {
                    for (const auto& s : part.symbols())
                    {
                        res.insert(s);
                    }
                }
                return res;
            }
        }
    }

    expression relabeled(const std::string& from, const std::string& to) const
    {
        switch (type)
        {
            case data_type::none     : return *this;
            case data_type::i32      : return *this;
            case data_type::f64      : return *this;
            case data_type::str      : return *this;
            case data_type::symbol   : return {expression::symbol(valsym == from ? to : valsym).keyed(keyword)};
            case data_type::composite:
            {
                std::vector<expression> res;

                for (const auto& part : parts)
                {
                    res.push_back(part.relabeled(from, to));
                }
                return res;
            }
        }
    }

    const expression& at(std::size_t index) const
    {
        return parts.at(index);
    }

    std::size_t size() const
    {
        return parts.size();
    }

    bool empty() const
    {
        return
        (type == data_type::none) ||
        (type == data_type::composite && parts.empty());
    }

    auto begin() const
    {
        return parts.begin();
    }

    auto end() const
    {
        return parts.end();
    }

    const auto& front() const
    {
        return parts.front();
    }

    const auto& back() const
    {
        return parts.back();
    }

    int get_i32() const
    {
        switch(type)
        {
            case data_type::none     : return 0;
            case data_type::i32      : return vali32;
            case data_type::f64      : return valf64;
            case data_type::str      : return std::stoi(valstr);
            case data_type::symbol   : return 0;
            case data_type::composite: return 0;
        }
    }

    double get_f64() const
    {
        switch(type)
        {
            case data_type::none     : return 0.0;
            case data_type::i32      : return vali32;
            case data_type::f64      : return valf64;
            case data_type::str      : return std::stod(valstr);
            case data_type::symbol   : return 0.0;
            case data_type::composite: return 0.0;
        }
    }

    std::string repr() const
    {
        switch(type)
        {
            case data_type::none     : return "()";
            case data_type::i32      : return std::to_string(vali32);
            case data_type::f64      : return std::to_string(valf64);
            case data_type::str      : return valstr;
            case data_type::symbol   : return valsym;
            case data_type::composite: return str();
        }
    }

    std::string str() const
    {
        auto pre = keyword.empty() ? "" : keyword + "=";

        switch (type)
        {
            case data_type::none     : return pre + "()";
            case data_type::i32      : return pre + std::to_string(vali32);
            case data_type::f64      : return pre + std::to_string(valf64);
            case data_type::str      : return pre + "'" + valstr + "'";
            case data_type::symbol   : return pre + valsym;
            case data_type::composite:
            {
                std::string res;

                for (const auto& part : parts)
                {
                    res += " " + part.str();
                }
                return pre + "(" + res.substr(1) + ")";
            }
        }
    }

    std::string sym() const
    {
        return valsym;
    }

    template<typename ObjectType, typename CallAdapter, typename Mapping>
    ObjectType resolve(const Mapping& scope, const CallAdapter& adapter) const
    {
        switch (type)
        {
            case data_type::none     : return adapter.convert(none());
            case data_type::i32      : return adapter.convert(vali32);
            case data_type::f64      : return adapter.convert(valf64);
            case data_type::str      : return adapter.convert(valstr);
            case data_type::symbol   : return adapter.at(scope, valsym);
            case data_type::composite: return adapter.call(scope, *this);
        }
    }

    bool operator==(const expression& other) const
    {
        return
        type    == other.type    &&
        vali32  == other.vali32  &&
        valf64  == other.valf64  &&
        valstr  == other.valstr  &&
        valsym  == other.valsym  &&
        keyword == other.keyword &&
        parts   == other.parts;
    }

    bool operator!=(const expression& other) const
    {
        return ! operator==(other);
    }

    /**
     * Mutate this expression so that (1) it is composite if it wasn't before,
     * and (2) any of its parts with the given key are reset to value. If no
     * parts have that key, then a new part is appended.
     */
    void set(const char* key, const expression& value)
    {
        bool exists = false;
        ensure_composite();

        for (auto& part : parts)
        {
            if (part.keyword == key)
            {
                part = value.keyed (key);
                exists = true;
            }
        }
        if (! exists)
        {
            parts.push_back (value.keyed (key));
        }
    }

    /**
     * Mutate this expression if necessary so that it is composite. If it was
     * previously not composite, then it will become {*this};
     */
    void ensure_composite()
    {
        if (type != data_type::composite)
        {
            *this = {*this};
        }
    }

private:
    data_type               type   = data_type::none;
    int                     vali32 = 0;
    double                  valf64 = 0.0;
    std::string             valstr;
    std::string             valsym;
    std::string             keyword;
    std::vector<expression> parts;
    friend class parser;
};




// ============================================================================
template<typename ObjectType, typename CallAdapter>
class crt::kernel
{
public:


    // ========================================================================
    struct rule_t;
    using map_t = std::unordered_map<std::string, rule_t>;
    using set_t = std::unordered_set<std::string>;


    // ========================================================================
    struct rule_t
    {
        ObjectType value;
        expression expr;
        std::string error;
        set_t incoming;
        set_t outgoing;
        bool dirty;
        long flags;
    };


    /**
     * Add a rule to the graph.
     */
    set_t insert(const std::string& key, const expression& expr, long flags=0)
    {
        if (cyclic(key, expr))
        {
            throw std::invalid_argument("would create dependency cycle");
        }
        reconnect(key, expr);

        auto rule = rule_t();
        rule.expr = expr;
        rule.incoming = expr.symbols();
        rule.outgoing = outgoing(key);
        rule.flags = flags;

        for (const auto& i : rule.incoming)
        {
            if (contains(i))
            {
                rules.at(i).outgoing.insert(key);
            }
        }
        rules[key] = rule;
        return mark(downstream(key, true));
    }


    /**
     * Add a literal rule to the graph. The expression for this rule is empty,
     * and its value is always the one given.
     */
    set_t insert(const std::string& key, const ObjectType& value, long flags=0)
    {
        reconnect(key, {});

        auto rule = rule_t();
        rule.incoming = {};
        rule.outgoing = outgoing(key);
        rule.value = value;
        rule.flags = flags;
        rules[key] = rule;
        return mark(downstream(key));
    }


    /**
     * Set the error string for the rule at the given key. You might do this
     * if an expression with this name failed to parse, or if the expression
     * resolved successfully, but failed some validation.
     */
    void set_error(const std::string& key, const std::string& error)
    {
        rules.at(key).error = error;
    }


    /**
     * Do |= on the flags at the given key.
     */
    void enable(const std::string& key, long flags)
    {
        rules.at(key).flags |= flags;
    }


    /**
     * Do &= ~ on the flags at the given key.
     */
    void disable(const std::string& key, long flags)
    {
        rules.at(key).flags &= ~flags;
    }


    /**
     * Mark the given rule and its downstream rules as dirty.
     */
    set_t touch(const std::string& key)
    {
        if (! contains(key))
        {
            return {};
        }
        return mark(downstream(key, true));
    }


    /**
     * Remove the rule with the given key from the graph, and return the keys
     * of affected (downstream) rules.
     */
    set_t erase(const std::string& key)
    {
        if (! contains(key))
        {
            return {};
        }
        auto affected = downstream(key);
        reconnect(key, {});
        rules.erase(key);
        return mark(affected);
    }


    /**
     * Remove all rules from the kernel.
     */
    void clear()
    {
        rules.clear();
    }


    /**
     * Mark the rules at the given keys as being dirty. Return the same set of
     * keys.
    */
    set_t mark(const set_t& keys)
    {
        for (const auto& key : keys)
        {
            rules.at(key).dirty = true;
        }
        return keys;
    }


    /**
     * Return the value associated with the rule at the given key. This
     * function hrows if the key does not exist.
     */
    const ObjectType& at(const std::string& key) const
    {
        return rules.at(key).value;
    }


    /**
     * Return the expression associated with the rule at the given key. The
     * expression is empty if this is a literal rule.
     */
    const expression& expr_at(const std::string& key) const
    {
        return rules.at(key).expr;
    }


    /**
     * Return the user flags associated with the rule at the given key.
     */
    const long& flags_at(const std::string& key) const
    {
        return rules.at(key).flags;
    }


    /** Return the error string associated with the rule at the given key. */
    const std::string& error_at(const std::string& key) const
    {
        return rules.at(key).error;
    }


    /**
      * Return true if upstream rules have changed since this rule was last
      * resolved. Rules that are not in the graph are always up-to-date.
      */
    bool dirty(const std::string& key) const
    {
        return contains(key) ? rules.at(key).dirty : false;

    }

    /**
     * Return true if none of the given rules are dirty.
     */
    bool current(const set_t& keys) const
    {
        for (const auto& k : keys)
        {
            if (dirty(k))
            {
                return false;
            }
        }
        return true;
    }


    /**
     * Convenience method for current(incoming(key)).
     */
    bool eligible(const std::string& key) const
    {
        return current(incoming(key));
    }


    /**
     * Return all the rules that are dirty.
     */
    set_t dirty_rules() const
    {
        set_t res;

        for (const auto& rule : rules)
        {
            if (rule.second.dirty)
            {
                res.insert(rule.first);
            }
        }
        return res;
    }


    /**
     * Return all the rules that are dirty, and whose flags do not match
     * any bits in the `excluding` value.
     */
    set_t dirty_rules_excluding(long excluding) const
    {
        set_t res;

        for (const auto& rule : rules)
        {
            if (rule.second.dirty && (rule.second.flags & excluding) == 0)
            {
                res.insert(rule.first);
            }
        }
        return res;
    }


    /**
     * Return all the rules that are dirty, and whose flags match bits in
     * the `only` value.
     */
    set_t dirty_rules_only(long only) const
    {
        set_t res;

        for (const auto& rule : rules)
        {
            if (rule.second.dirty && (rule.second.flags & only) != 0)
            {
                res.insert(rule.first);
            }
        }
        return res;
    }


    /**
     * Return true if the graph contains a rule with the given key.
     */
    bool contains(const std::string& key) const
    {
        return rules.find(key) != rules.end();
    }


    /**
     * Return the number of rules in the graph.
     */
    std::size_t size() const
    {
        return rules.size();
    }


    /**
     * Return true if the graph is empty.
     */
    bool empty() const
    {
        return rules.empty();
    }


    /**
     * Return the begin iterator to the container of rules.
     */
    auto begin() const
    {
        return rules.begin();
    }


    /**
     * Return the end iterator to the container of rules.
     */
    auto end() const
    {
        return rules.end();
    }


    /**
     * Return the incoming edges for the given rule. An empty set is returned
     * if the key does not exist in the graph.
    */
    set_t incoming(const std::string& key) const
    {
        if (contains(key))
        {
            return rules.at(key).incoming;
        }
        return {};
    }


    /**
     * Return all rules that this rule depends on.
     */
    set_t upstream(const std::string& key) const
    {
        auto res = incoming(key);

        for (const auto& k : incoming(key))
        {
            for (const auto& m : upstream(k))
            {
                res.insert(m);
            }
        }
        return res;
    }


    /**
      * Return the outgoing edges for the given rule. Even if that rule does
      * not exist in the graph, it may have outgoing edges if other rules in
      * the graph name it as a dependency. If the rule does exist in the
      * graph, this is a fast operation because outgoing edges are cached and
      * kept up-to-date.
    */
    set_t outgoing(const std::string& key) const
    {
        if (contains(key))
        {
            return rules.at(key).outgoing;
        }

        /*
         Search the graph for rules naming key as a dependency, and add those rules
         to the list of outgoing edges.
         */
        set_t out;

        for (const auto& rule : rules)
        {
            if (incoming(rule.first).count(key))
            {
                out.insert(rule.first);
            }
        }
        return out;
    }


    /**
     * Return all rules that depend on the given rule. Note that adding and
     * removing rules from the kernel influences the downstream keys.
     */
    set_t downstream(const std::string& key, bool inclusive=false) const
    {
        auto res = outgoing(key);

        for (const auto& k : outgoing(key))
        {
            for (const auto& m : downstream(k))
            {
                res.insert(m);
            }
        }
        if (inclusive)
        {
            res.insert(key);
        }
        return res;
    }


    /**
     * Return true if addition of the given rule would create a dependency
     * cycle in the graph. This checks for whether any of the rules downstream
     * of the given key are symbols in expr.
     */
    bool cyclic(const std::string& key, const expression& expr)
    {
        auto dependents = downstream(key, true);

        for (const auto& k : expr.symbols())
        {
            if (dependents.count(k) != 0)
            {
                return true;
            }
        }
        return false;
    }


    /**
     * Evaluate the given rule, catching any exceptions that arise and
     * returning them in the error string.
     */
    ObjectType resolve(const std::string& key, std::string& error, const CallAdapter& adapter) const
    {
        try {
            error.clear();
            const auto rule = rules.at(key);

            if (rule.expr.empty())
            {
                return rule.value;
            }
            return rule.expr.template resolve<ObjectType>(*this, adapter);
        }
        catch (const std::out_of_range& e)
        {
            error = "unresolved symbol: " + key;
        }
        catch (const std::exception& e)
        {
            error = e.what();
        }
        return ObjectType();
    }

    ObjectType resolve(const std::string& key) const
    {
        std::string error;
        CallAdapter adapter;
        return resolve(key, error, adapter);
    }

    void unmark(const std::string& key)
    {
        rules.at(key).dirty = false;
    }

    /**
     * Set the value and error state of the given rule directly. This would be
     * useful if you are handling expression evaluations on your own, say on a
     * different thread, and you want to insert the result of that evaluation.
     * This method sets the rule as not dirty.
     */
    void update_directly(const std::string& key, const ObjectType& value, const std::string& error)
    {
        auto& rule = rules.at(key);
        rule.value = value;
        rule.error = error;
        rule.dirty = false;
    }

    bool update(const std::string& key, const CallAdapter& adapter)
    {
        auto& rule = rules.at(key);

        if (! current(rule.incoming))
        {
            return false;
        }
        if (rule.dirty)
        {
            rule.value = resolve(key, rule.error, adapter);
            rule.dirty = false;
        }
        return rule.error.empty();
    }

    bool update(const std::string& key)
    {
        CallAdapter adapter;
        return update(key, adapter);
    }

    void update_recurse(const std::string& key, const CallAdapter& adapter)
    {
        if (update(key, adapter))
        {
            for (const auto& k : outgoing(key))
            {
                update_recurse(k, adapter);
            }
        }
    }

    void update_recurse(const std::string& key)
    {
        CallAdapter adapter;
        update_recurse(adapter, adapter);
    }

    void update_all(const set_t& keys, const CallAdapter& adapter)
    {
        for (const auto& key : keys)
        {
            update_recurse(key, adapter);
        }
    }

    void update_all(const set_t& keys)
    {
        CallAdapter adapter;
        update_all(keys, adapter);
    }

    void relabel(const std::string& from, const std::string& to)
    {
        if (contains(to))
        {
            throw std::invalid_argument("cannot relabel rule to an existing key");
        }
        if (contains(from))
        {
            if (upstream(from).count(to))
            {
                throw std::invalid_argument("cannot relabel rule to an upstream symbol");
            }
            auto r = rules.at(from);
            rules.erase(from);
            rules[to] = r;
        }
        for (auto& rule : rules)
        {
            rule.second.expr = rule.second.expr.relabeled(from, to);
        }
    }


    /**
     * Make a copy of this kernel containing only the rules upstream of the
     * given key. This is useful if you want to do some evaluations on a
     * different thread. The result of asynchronous evaluations can be set
     * with the update_directly method.
     */
    // kernel subset_upstream_of(std::string& key)
    // {
    //     kernel other;

    //     for (const auto& k : upstream(key))
    //     {
    //         other.rules[k] = rules[k];
    //     }
    //     return other;
    // }


private:
    // ========================================================================
    void reconnect(const std::string& key, const expression& expr)
    {
        for (const auto& k : incoming(key))
        {
            if (contains(k))
            {
                rules.at(k).outgoing.erase(key);
            }
        }
        for (const auto& k : expr.symbols())
        {
            if (contains(k))
            {
                rules.at(k).outgoing.insert(key);
            }
        }
    }
    map_t rules;
};




// ============================================================================
class crt::parser
{

public:
    static expression parse(const char* expr)
    {
        return parse_part(expr);
    }

private:
    static bool is_symbol_character(char e)
    {
        return isalnum(e) || e == '_' || e == '-' || e == ':';
    }

    static bool is_number(const char* d)
    {
        if (isdigit(*d))
        {
            return true;
        }
        else if (*d == '.')
        {
            return isdigit(d[1]);
        }
        else if (*d == '+' || *d == '-')
        {
            return isdigit(d[1]) || (d[1] == '.' && isdigit(d[2]));
        }
        return false;
    }

    static const char* get_named_part(const char*& c)
    {
        const char* cc = c;
        const char* start = c;

        while (is_symbol_character(*cc++))
        {
            if (*cc == '=')
            {
                c = cc + 1;
                return start;
            }
        }
        return nullptr;
    }

    static const char* find_closing_parentheses(const char* c)
    {
        int level = 0;
        bool in_str = false;

        do
        {
            if (*c == '\0')
            {
                throw parser_error("unterminated expression");
            }
            else if (in_str)
            {
                if (*c == '\'') in_str = false;
            }
            else
            {
                if (*c == '\'') in_str = true;
                else if (*c == ')') --level;
                else if (*c == '(') ++level;
            }
            ++c;
        } while (level > 0);

        return c;
    }

    static expression parse_number(const char*& c)
    {
        const char* start = c;
        bool isdec = false;
        bool isexp = false;

        if (*c == '+' || *c == '-')
        {
            ++c;
        }

        while (isdigit(*c) || *c == '.' || *c == 'e' || *c == 'E')
        {
            if (*c == 'e' || *c == 'E')
            {
                if (isexp)
                {
                    throw parser_error("syntax error: bad numeric literal");
                }
                isexp = true;
            }
            if (*c == '.')
            {
                if (isdec || isexp)
                {
                    throw parser_error("syntax error: bad numeric literal");
                }
                isdec = true;
            }
            ++c;
        }

        if (! (isspace(*c) || *c == '\0' || *c == ')'))
        {
            throw parser_error("syntax error: bad numeric literal");
        }
        else if (isdec || isexp)
        {
            return expression(atof(std::string(start, c - start).data()));
        }
        else
        {
            return expression(atoi(std::string(start, c - start).data()));
        }
    }

    static expression parse_symbol(const char*& c)
    {
        const char* start = c;

        while (is_symbol_character(*c))
        {
            ++c;
        }
        return expression::symbol(std::string(start, c));
    }

    static expression parse_single_quoted_string(const char*& c)
    {
        const char* start = c++;

        while (*c != '\'')
        {
            if (*c == '\0')
            {
                throw parser_error("syntax error: unterminated string");
            }
            ++c;
        }

        ++c;

        if (! (isspace(*c) || *c == '\0' || *c == ')'))
        {
            throw parser_error("syntax error: non-whitespace character following single-quoted string");
        }
        return expression(std::string(start + 1, c - 1));
    }

    static expression parse_expression(const char*& c)
    {
        auto e = expression();
        auto end = find_closing_parentheses(c);
        ++c;

        while (c != end)
        {
            if (*c == '\0')
            {
                throw parser_error("syntax error: unterminated expression");
            }
            else if (isspace(*c) || *c == ')')
            {
                ++c;
            }
            else
            {
                e.parts.push_back(parse_part(c));
            }
        }
        if (! e.parts.empty())
        {
            e.type = data_type::composite;
        }
        return e;
    }

    static expression parse_part(const char*& c)
    {
        std::string kw;

        while (*c != '\0')
        {
            if (isspace(*c))
            {
                ++c;
            }
            else if (const char* kwstart = get_named_part(c))
            {
                kw = std::string(kwstart, c - 1);
            }
            else if (is_number(c))
            {
                return parse_number(c).keyed(kw);
            }
            else if (isalpha(*c))
            {
                return parse_symbol(c).keyed(kw);
            }
            else if (*c == '\'')
            {
                return parse_single_quoted_string(c).keyed(kw);
            }
            else if (*c == '(')
            {
                return parse_expression(c).keyed(kw);
            }
            else
            {
                throw parser_error("syntax error: unkown character '" + std::string(c, c + 1) + "'");
            }
        }
        return expression();
    }
};




// ============================================================================
#ifdef TEST_KERNEL
#include "catch.hpp"
#include "any.hpp"
using namespace crt;




// ============================================================================
class AnyCallAdapter
{
public:
    using ObjectType = linb::any;
    using list_t = std::vector<ObjectType>;
    using dict_t = std::unordered_map<std::string, ObjectType>;
    using func_t = std::function<ObjectType(list_t, dict_t)>;

    template<typename Mapping>
    ObjectType call(const Mapping& scope, const crt::expression& expr) const
    {
        auto head = std::string();
        auto args = std::vector<ObjectType>();
        auto kwar = std::unordered_map<std::string, ObjectType>();

        for (const auto& part : expr)
        {
            if (part == expr.front())
            {
                head = part.sym();
            }
            else if (part.key().empty())
            {
                args.push_back(expr.resolve<ObjectType>(scope, *this));
            }
            else
            {
                kwar[expr.key()] = expr.resolve<ObjectType>(scope, *this);
            }
        }
        return linb::any_cast<func_t>(scope.at(head))(args, kwar);
    }

    template<typename T>
    ObjectType convert(const T& value) const
    {
        return value;
    }

    template<typename Mapping>
    const ObjectType& at(const Mapping& scope, const std::string& key) const
    {
        return scope.at(key);
    }
};
using Kernel = kernel<linb::any, AnyCallAdapter>;




TEST_CASE("expression passes basic sanity tests", "[expression]")
{
    REQUIRE(expression{1, 2} == expression{1, 2});
    REQUIRE(expression{1, 2} != expression());
    REQUIRE(expression().empty());
    REQUIRE(expression().dtype() == data_type::none);
    REQUIRE(expression::composite({}).empty());
    REQUIRE(expression::composite({}).dtype() == data_type::none);
    REQUIRE(expression() == expression::none());
    REQUIRE(expression() == expression({}));
    REQUIRE(expression() == expression{});
    REQUIRE(expression() == expression::composite({}));
    REQUIRE(expression() != expression::composite({1, 2, 3}));
}




TEST_CASE("nested expression can be constructed by hand", "[expression]")
{
    expression e {
        1,
        2.3,
        std::string("sdf"),
        expression::symbol("a"),
        {1, expression::symbol("b"), expression::symbol("b")}};

    REQUIRE(e.dtype() == data_type::composite);
    REQUIRE(e.size() == 5);
    REQUIRE(e.list()[0].dtype() == data_type::i32);
    REQUIRE(e.list()[1].dtype() == data_type::f64);
    REQUIRE(e.list()[2].dtype() == data_type::str);
    REQUIRE(e.list()[3].dtype() == data_type::symbol);
    REQUIRE(e.list()[4].dtype() == data_type::composite);
    REQUIRE(e.symbols().size() == 2);
    REQUIRE(e == e);
}




TEST_CASE("expression can be converted to string", "[expression]")
{
    REQUIRE(expression().str() == "()");
    REQUIRE(expression({}).str() == "()");
    REQUIRE(expression({1, 2, 3}).str() == "(1 2 3)");
    REQUIRE(expression({1, 2, 3}).str() == "(1 2 3)");
}




TEST_CASE("basic strings can be parsed into expressions", "[parser]")
{
    REQUIRE(parser::parse("a").dtype() == data_type::symbol);
    REQUIRE(parser::parse("1").dtype() == data_type::i32);
    REQUIRE(parser::parse("1.0").dtype() == data_type::f64);
    REQUIRE(parser::parse("(a b c)").dtype() == data_type::composite);
    REQUIRE(parser::parse("(a b c)").size() == 3);
    REQUIRE(parser::parse("(a b b c 1 2 'ant')").symbols().size() == 3);
    REQUIRE(parser::parse("(1 2 3)") == expression{1, 2, 3});
    REQUIRE(parser::parse("(1.0 2.0 3.0)") == expression{1.0, 2.0, 3.0});
    REQUIRE(parser::parse("a=1").key() == "a");
    REQUIRE(parser::parse("('cat' 'moose' 'dragon')") == expression{
        std::string("cat"),
        std::string("moose"),
        std::string("dragon")});
    REQUIRE_THROWS(parser::parse("1.2.0"));
}




TEST_CASE("numeric constants parse correctly", "[parser]")
{
    REQUIRE(parser::parse("12").get_i32() == 12);
    REQUIRE(parser::parse("13").get_i32() == 13);
    REQUIRE(parser::parse("+12").get_i32() == 12);
    REQUIRE(parser::parse("-12").get_i32() ==-12);
    REQUIRE(parser::parse("13.5").get_f64() == 13.5);
    REQUIRE(parser::parse("+13.5").get_f64() == 13.5);
    REQUIRE(parser::parse("-13.5").get_f64() ==-13.5);
    REQUIRE(parser::parse("+13.5e2").get_f64() == 13.5e2);
    REQUIRE(parser::parse("-13.5e2").get_f64() ==-13.5e2);
    REQUIRE(parser::parse("+13e2").get_f64() == 13e2);
    REQUIRE(parser::parse("-13e2").get_f64() ==-13e2);
    REQUIRE(parser::parse("-.5").get_f64() == -.5);
    REQUIRE(parser::parse("+.5").get_f64() == +.5);
    REQUIRE(parser::parse(".5").get_f64() == +.5);
    REQUIRE_THROWS(parser::parse("-"));
    REQUIRE_THROWS(parser::parse("1e2e2"));
    REQUIRE_THROWS(parser::parse("1.2.2"));
    REQUIRE_THROWS(parser::parse("1e2.2"));
    REQUIRE_THROWS(parser::parse("13a"));
}




TEST_CASE("keyword expressions parse correctly", "[parser]")
{
    REQUIRE(parser::parse("a=1").dtype() == data_type::i32);
    REQUIRE(parser::parse("a=1").key() == "a");
    REQUIRE(parser::parse("cow='moo'").dtype() == data_type::str);
    REQUIRE(parser::parse("cow='moo'").key() == "cow");
    REQUIRE(parser::parse("deer=(0 1 2 3)").dtype() == data_type::composite);
    REQUIRE(parser::parse("deer=(0 1 2 3)").key() == "deer");
    REQUIRE(parser::parse("deer=(0 1 2 3)").size() == 4);
    REQUIRE(parser::parse("deer=(0 1 2 3)").at(0).get_i32() == 0);
    REQUIRE(parser::parse("deer=(0 1 2 3)").at(1).get_i32() == 1);
    REQUIRE(parser::parse("deer=(0 1 2 3)").at(2).get_i32() == 2);
    REQUIRE(parser::parse("deer=(0 1 2 3)").at(3).get_i32() == 3);
}




TEST_CASE("more complex expressions parse correctly", "[parser]")
{
    REQUIRE(parser::parse("(0 1 2 3 (0 1 2 3))").str() == "(0 1 2 3 (0 1 2 3))");
    REQUIRE(parser::parse("(a 1 2 3 (b 1 2 3 (c 1 2 3)))").str() == "(a 1 2 3 (b 1 2 3 (c 1 2 3)))");
    REQUIRE(parser::parse("(a a a)").size() == 3);
    REQUIRE(parser::parse("()").size() == 0);
    REQUIRE(parser::parse("(a)").size() == 1);
    REQUIRE(parser::parse("((a))").size() == 1);
    REQUIRE(parser::parse("((a) a)").size() == 2);
    REQUIRE(parser::parse("(a (a))").size() == 2);
    REQUIRE(parser::parse("((a) a a)").size() == 3);
    REQUIRE(parser::parse("(a (a) a)").size() == 3);
    REQUIRE(parser::parse("(a a (a))").size() == 3);
    REQUIRE(parser::parse("((a) a ('a') a (a))").size() == 5);
    REQUIRE(parser::parse("(a '(a) (a) (a')").size() == 2);
    REQUIRE(parser::parse("(a 'a) (a) (a)')").size() == 2);
    REQUIRE_THROWS(parser::parse("(a 'a) (a) (a))"));
}




TEST_CASE("kernel can be constructed", "[kernel]")
{
    Kernel k;

    k.insert("a", linb::any(1));
    k.insert("b", expression(1));

    REQUIRE(k.size() == 2);
}




TEST_CASE("kernel computes upstream/downstream nodes correctly", "[kernel]")
{
    Kernel k;

    k.insert("a", expression::symbol("b"));
    k.insert("b", expression::symbol("c"));

    REQUIRE(expression::symbol("a").symbols() == Kernel::set_t{"a"});
    REQUIRE(k.incoming("a") == Kernel::set_t{"b"});
    REQUIRE(k.upstream("a") == Kernel::set_t{"b", "c"});

    REQUIRE(k.downstream("c") == Kernel::set_t{"a", "b"});
    REQUIRE(k.downstream("b") == Kernel::set_t{"a"});

    k.erase("a");
    REQUIRE(k.downstream("c") == Kernel::set_t{"b"});
    REQUIRE(k.downstream("b") == Kernel::set_t{});

    k.insert("a", {expression::symbol("b"), expression::symbol("d")});
    REQUIRE(k.upstream("a") == Kernel::set_t{"b", "c", "d"});
    REQUIRE(k.cyclic("d", expression::symbol("a")));
    REQUIRE_FALSE(k.cyclic("d", expression::symbol("e")));
    REQUIRE_THROWS(k.insert("d", expression::symbol("a")));
}




TEST_CASE("kernel marks affected nodes correctly", "[kernel]")
{
    Kernel k;

    SECTION("graph with two rules")
    {
        k.insert("a", expression::symbol("b"));
        k.insert("b", expression::symbol("c"));

        REQUIRE(k.dirty("a"));
        REQUIRE(k.dirty("b"));
        REQUIRE_FALSE(k.dirty("c"));
    }

    SECTION("graph with two rules and a value")
    {
        k.insert("a", expression::symbol("b"));
        k.insert("b", expression::symbol("c"));
        k.insert("c", linb::any(12));

        SECTION("can be updated incrementally")
        {
            REQUIRE(k.dirty("a"));
            REQUIRE(k.dirty("b"));
            REQUIRE_FALSE(k.dirty("c"));
            REQUIRE_FALSE(k.update("a"));
            REQUIRE(k.update("b"));
            REQUIRE(k.update("a"));
            REQUIRE_FALSE(k.dirty("a"));
            REQUIRE_FALSE(k.dirty("b"));
        }

        SECTION("can be updated all at once")
        {
            REQUIRE(k.dirty_rules() == Kernel::set_t{"a", "b"});
            k.update_all(k.dirty_rules());
            REQUIRE(k.dirty_rules() == Kernel::set_t{});
        }
    }
}
#endif
