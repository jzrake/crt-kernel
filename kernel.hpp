#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include "any.hpp"




// ============================================================================
namespace crt
{
    class expression;
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
    expression(int vali32)                              : type(data_type::i32), vali32(vali32) {}
    expression(double valf64)                           : type(data_type::f64), valf64(valf64) {}
    expression(const std::string& valstr)               : type(data_type::str), valstr(valstr) {}
    expression(std::initializer_list<expression> parts) : type(data_type::composite), parts(parts) {}

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
        e.type = data_type::composite;
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

    std::size_t size() const
    {
        return parts.size();
    }

    bool empty() const
    {
        return type == data_type::none;
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

    std::set<std::string> symbols() const
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
                std::set<std::string> res;

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

    linb::any evaluate(const dict_t& scope) const
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
                    args.push_back(a.evaluate(scope));
                }
                for (const auto& p : dict())
                {
                    kwar.emplace(p.first, p.second.evaluate(scope));
                }
                auto head = linb::any_cast<func_t>(scope.at(parts.at(0).valsym));
                return head(args, kwar);
            }
        }
    }

    bool operator==(const expression& other) const
    {
        return (true
        && type    == other.type
        && vali32  == other.vali32
        && valf64  == other.valf64
        && valstr  == other.valstr
        && valsym  == other.valsym
        && keyword == other.keyword
        && parts   == other.parts);
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
class crt::parser
{
public:

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
        auto e = expression::composite({});

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
        return e;
    }

    static expression parse_part(const char*& c)
    {
        const char* kw = nullptr;
        std::size_t kwlen = 0;

        while (*c != '\0')
        {
            if (isspace(*c))
            {
                ++c;
            }
            else if (const char* kwstart = get_named_part(c))
            {
                kwlen = c - (kw = kwstart) - 1;
            }
            else if (is_number(c))
            {
                return parse_number(c).keyed(std::string(kw, kw + kwlen));
            }
            else if (isalpha(*c))
            {
                return parse_symbol(c).keyed(std::string(kw, kw + kwlen));
            }
            else if (*c == '\'')
            {
                return parse_single_quoted_string(c).keyed(std::string(kw, kw + kwlen));
            }
            else if (*c == '(')
            {
                return parse_expression(c).keyed(std::string(kw, kw + kwlen));
            }
            else
            {
                throw std::runtime_error("syntax error: unkown character");
            }
        }
        return expression();
    }

    static expression parse(const char* expr)
    {
        return parse_part(expr);
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




TEST_CASE("parser works", "[parser]")
{
    REQUIRE(parser::parse("a").dtype() == data_type::symbol);
    REQUIRE(parser::parse("1").dtype() == data_type::i32);
    REQUIRE(parser::parse("1.0").dtype() == data_type::f64);
    REQUIRE(parser::parse("(a b c)").dtype() == data_type::composite);
    REQUIRE(parser::parse("(a b c)").size() == 3);
    REQUIRE(parser::parse("(1 2 3)") == expression{1, 2, 3});
    REQUIRE(parser::parse("(1.0 2.0 3.0)") == expression{1.0, 2.0, 3.0});
    REQUIRE(parser::parse("a=1").key() == "a");
    REQUIRE(parser::parse("('cat' 'moose' 'dragon')") == expression{
        std::string("cat"),
        std::string("moose"),
        std::string("dragon")});
    REQUIRE_THROWS(parser::parse("1.2.0"));
}
#endif
