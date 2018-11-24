#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "any.hpp"




// ============================================================================
namespace crt
{
    class expression;
    class kernel;
    class parser;

    enum class data_type { none, i32, f64, str, symbol, composite };
    using list_t = std::vector<linb::any>;
    using dict_t = std::unordered_map<std::string, linb::any>;
    using func_t = std::function<linb::any(list_t, dict_t)>;
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
    expression(std::initializer_list<expression> parts) : type(data_type::composite), parts(parts)
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

    double get_i32() const
    {
        switch(type)
        {
            case data_type::none     : return 0.0;
            case data_type::i32      : return vali32;
            case data_type::f64      : return valf64;
            case data_type::str      : return std::atoi(valstr.data());
            case data_type::symbol   : return 0.0;
            case data_type::composite: return 0.0;
        }
    }

    double get_f64() const
    {
        switch(type)
        {
            case data_type::none     : return 0.0;
            case data_type::i32      : return vali32;
            case data_type::f64      : return valf64;
            case data_type::str      : return std::atof(valstr.data());
            case data_type::symbol   : return 0.0;
            case data_type::composite: return 0.0;
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

    template<typename Mapping>
    linb::any resolve(const Mapping& scope) const
    {
        switch (type)
        {
            case data_type::none     : return none();
            case data_type::i32      : return vali32;
            case data_type::f64      : return valf64;
            case data_type::str      : return valstr;
            case data_type::symbol   : return scope.at(valsym);
            case data_type::composite:
            {
                list_t args;
                dict_t kwar;

                for (const auto& a : list())
                {
                    args.push_back(a.resolve(scope));
                }
                for (const auto& p : dict())
                {
                    kwar.emplace(p.first, p.second.resolve(scope));
                }
                auto head = linb::any_cast<func_t>(scope.at(parts.at(0).valsym));
                return head(args, kwar);
            }
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
        expression expr;
        linb::any value;
        std::string key;
        std::string error;
        set_t incoming;
        set_t outgoing;
        bool dirty;
    };


    /** Add a rule to the graph.
     */
    set_t insert(const std::string& key, const expression& expr)
    {
        if (cyclic(key, expr))
        {
            throw std::invalid_argument("would create cyclic dependency");
        }
        reconnect(key, expr);

        auto rule = rule_t();
        rule.expr = expr;
        rule.incoming = expr.symbols();
        rule.outgoing = outgoing(key);

        for (const auto& i : rule.incoming)
        {
            if (contains(i))
            {
                rules.at(i).outgoing.insert(key);
            }
        }
        rules.emplace(key, rule);
        return mark(downstream(key, true));
    }

    /** Add a literal rule to the graph. The expression for this rule is empty, and
        its value is always the one given.
     */
    set_t insert(const std::string& key, const linb::any& value)
    {
        reconnect(key, {});

        auto rule = rule_t();
        rule.value = value;
        rule.incoming = {};
        rule.outgoing = outgoing(key);
        rules.emplace(key, rule);
        return mark(downstream(key, true));
    }

    /** Remove the rule with the given key from the graph, and return the keys of
        affected (downstream) rules.
    */
    set_t erase(const std::string& key)
    {
        reconnect(key, {});

        if (! contains(key))
        {
            return {};
        }

        auto affected = downstream(key);
        rules.erase(key);
        return mark(affected);
    }

    /** Mark the rules at the given keys as being dirty. Return the same set of
        keys.
    */
    set_t mark(const set_t& keys)
    {
        for (const auto& key : keys)
        {
            rules.at(key).dirty = true;
        }
        return keys;
    }

    /** Return the expression associated with the rule at the given key. The expression is
        empty if this is a literal rule.
    */
    const expression& expr_at(const std::string& key) const
    {
        return rules.at(key).expr;
    }

    /** Return the value associated with the rule at the given key. This function
        throws if the key does not exist.
    */
    const linb::any& at(const std::string& key) const
    {
        return rules.at(key).value;
    }

    /** Return true if upstream rules have changed since this rule was last
        resolved. Rules that are not in the graph are always up-to-date.
    */
    bool dirty(const std::string& key) const
    {
        return contains(key) ? rules.at(key).dirty : false;
    }

    /** Return true if the graph contains a rule with the given key. */
    bool contains(const std::string& key) const
    {
        return rules.find(key) != rules.end();
    }

    /** Return the number of rules in the graph.*/
    std::size_t size() const
    {
        return rules.size();
    }

    /** Return true if the graph is empty.*/
    bool empty() const
    {
        return rules.empty();
    }

    /** Return the begin iterator to the container of rules.*/
    auto begin() const
    {
        return rules.begin();
    }

    /** Return the end iterator to the container of rules.*/
    auto end() const
    {
        return rules.end();
    }

    /** Return the incoming edges for the given rule. An empty set is returned if
        the key does not exist in the graph.
    */
    set_t incoming(const std::string& key) const
    {
        if (contains(key))
        {
            return rules.at(key).incoming;
        }
        return {};
    }

    /** Return all rules that this rule depends on. */
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

    /** Return the outgoing edges for the given rule. Even if that rule does not exist
        in the graph, it may have outgoing edges if other rules in the graph name it
        as a dependency. If the rule does exist in the graph, this is a fast operation
        because outgoing edges are cached and kept up-to-date.
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


    /** Return all rules that depend on the given rule. Note that adding and
        removing rules from the kernel influences the downstream keys.
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

    /** Return true if addition of the given rule would create a dependency
        cycle in the graph. This checks for whether any of the rules downstream
        of the given key are symbols in expr.
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

    /** Return the value of the given rule, catching any exceptions that arise
        and returning them in the error string.
    */
    linb::any resolve(const std::string& key, std::string& error) const
    {
        try {
            error.clear();
            auto rule = rules.at(key);
            return rule.expr.empty() ? rule.value : rule.expr.resolve(*this);
        }
        catch (std::exception& e)
        {
            error = e.what();
            return linb::any();
        }
    }

    void update(const std::string& key)
    {
        auto& rule = rules.at(key);

        for (const auto& k : rule.incoming)
        {
            if (dirty(k))
            {
                throw std::invalid_argument("cannot update rule with unresolved dependencies");
            }
        }
        rule.value = resolve(key, rule.error);
    }

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
                    throw std::runtime_error("syntax error: bad numeric literal");   
                }
                isexp = true;
            }
            if (*c == '.')
            {
                if (isdec || isexp)
                {
                    throw std::runtime_error("syntax error: bad numeric literal");   
                }
                isdec = true;
            }
            ++c;
        }

        if (! (isspace(*c) || *c == '\0' || *c == ')'))
        {
            throw std::runtime_error("syntax error: bad numeric literal");
        }
        else if (isdec || isexp)
        {
            return expression(atof(std::string (start, c - start).data()));
        }
        else
        {
            return expression(atoi(std::string (start, c - start).data()));
        }
    }

    static expression parse_symbol(const char*& c)
    {
        const char* start = c;

        while (is_symbol_character (*c))
        {
            ++c;
        }
        return expression::symbol(std::string(start, c));
    }

    static expression parse_single_quoted_string(const char*& c)
    {
        assert(*c == '\'');
        const char* start = c++;

        while (*c != '\'')
        {
            if (*c == '\0')
            {
                throw std::runtime_error("syntax error: unterminated string");
            }
            ++c;
        }

        ++c;

        if (! (isspace (*c) || *c == '\0' || *c == ')'))
        {
            throw std::runtime_error("syntax error: non-whitespace character following single-quoted string");
        }
        return expression(std::string(start + 1, c - 1));
    }

    static expression parse_expression(const char*& c)
    {
        assert(*c == '(');
        const char* start = c++;
        auto e = expression();

        while (*c != ')')
        {
            if (*c == '\0')
            {
                throw std::runtime_error("syntax error: unterminated expression");
            }
            else if (isspace(*c))
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
                throw std::runtime_error("syntax error: unkown character");
            }
        }
        return expression();
    }
};




// ============================================================================
#ifdef TEST_KERNEL
#include "catch.hpp"
using namespace crt;




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




TEST_CASE("kernel can be constructed", "[kernel]")
{
    kernel k;

    k.insert("a", linb::any(1));
    k.insert("b", expression(1));

    REQUIRE(k.size() == 2);
}




TEST_CASE("kernel computes upstream/downstream nodes correctly", "[kernel]")
{
    kernel k;

    k.insert("a", expression::symbol("b"));
    k.insert("b", expression::symbol("c"));

    REQUIRE(expression::symbol("a").symbols() == kernel::set_t{"a"});
    REQUIRE(k.incoming("a") == kernel::set_t{"b"});
    REQUIRE(k.upstream("a") == kernel::set_t{"b", "c"});

    REQUIRE(k.downstream("c") == kernel::set_t{"a", "b"});
    REQUIRE(k.downstream("b") == kernel::set_t{"a"});

    k.erase("a");
    REQUIRE(k.downstream("c") == kernel::set_t{"b"});
    REQUIRE(k.downstream("b") == kernel::set_t{});

    k.insert("a", {expression::symbol("b"), expression::symbol("d")});
    REQUIRE(k.upstream("a") == kernel::set_t{"b", "c", "d"});
    REQUIRE(k.cyclic("d", expression::symbol("a")));
    REQUIRE_FALSE(k.cyclic("d", expression::symbol("e")));
    REQUIRE_THROWS(k.insert("d", expression::symbol("a")));
}




TEST_CASE("kernel marks affected nodes correctly", "[kernel]")
{
    kernel k;

    k.insert("a", expression::symbol("b"));
    k.insert("b", expression::symbol("c"));

    REQUIRE(k.dirty("a"));
    REQUIRE(k.dirty("b"));
    REQUIRE_FALSE(k.dirty("c"));
}
#endif
