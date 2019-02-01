#include <iostream>
#include "kernel.hpp"




//=============================================================================
namespace builtin
{

crt::expression table(const crt::expression& e)
{
    return e;
}

crt::expression list(const crt::expression& e)
{
    return e.list();
}

crt::expression dict(const crt::expression& e)
{
    return e.dict();
}

crt::expression item(const crt::expression& e)
{
    auto arg = e.first();
    auto ind = e.second();

    if (e.second().has_type(crt::data_type::i32))
    {
        return arg.item(ind.get_i32());
    }
    if (ind.has_type(crt::data_type::table))
    {
        std::vector<crt::expression> result;
        
        for (const auto& i : ind)
        {
            result.push_back(arg.item(int(i)).keyed(i.key()));
        }
        return result;
    }
    return {};
}

crt::expression attr(const crt::expression& e)
{
    return e.first().attr(e.second());
}

crt::expression range(const crt::expression& e)
{
    int start = 0;
    int final = 0;
    int steps = 1;

    if (e.size() == 1)
    {
        final = e.item(0);
    }
    else if (e.size() == 2)
    {
        start = e.item(0);
        final = e.item(1);
    }
    else if (e.size() == 3)
    {
        start = e.item(0);
        final = e.item(1);
        steps = e.item(2);
    }

    std::vector<crt::expression> result;

    if (start < final && steps > 0)
        for (int n = start; n < final; n += steps)
            result.push_back(n);

    if (start > final && steps < 0)
        for (int n = start; n > final; n += steps)
            result.push_back(n);

    return result;
}

crt::expression slice(const crt::expression& e)
{
    return item({e.first(), range(e.rest())});
}

crt::expression concat(const crt::expression& e)
{
    std::vector<crt::expression> result;

    for (const auto& part : e)
    {
        result.insert(result.end(), part.begin(), part.end());
    }
    return result;
}

crt::expression join(const crt::expression& e)
{
    std::string result;
    std::string sep = e.attr("sep");

    bool first = true;

    for (const auto& part : e.list())
    {
        result += (first ? "" : sep) + std::string(part);
        first = false;
    }
    return result;
}

crt::expression apply(const crt::expression& e)
{
    return e.first().call(e.second());
}

crt::expression zip(const crt::expression& e)
{
    return e.zip();
}

crt::expression map(const crt::expression& e)
{
    std::vector<crt::expression> result;

    for (const auto& argset : e.rest().zip())
    {
        result.push_back(e.first().call(argset));
    }
    return result;
}

crt::expression first(const crt::expression& e)
{
    return e.first().first();
}

crt::expression second(const crt::expression& e)
{
    return e.first().second();
}

crt::expression rest(const crt::expression& e)
{
    return e.first().rest();
}

crt::expression last(const crt::expression& e)
{
    return e.first().last();
}

crt::expression len(const crt::expression& e)
{
    return int(e.first().size());
}

crt::expression sort(const crt::expression& e)
{
    return e.first().sort();
}

crt::expression reverse(const crt::expression& e)
{
    auto arg = e.first();
    return crt::expression(arg.rbegin(), arg.rend());
}

crt::expression nest(const crt::expression& e)
{
    return e.first().nest();
}

crt::expression type(const crt::expression& e)
{
    return std::string(e.first().type_name());
}

crt::expression eval(const crt::expression& e)
{
    return crt::parse(e.first());
}

crt::expression unparse(const crt::expression& e)
{
    return e.first().unparse();
}

}





//=============================================================================
struct my_struct
{
    int a, b;
};




//=============================================================================
template<>
struct crt::type_info<my_struct>
{
    static const char* name()
    {
        return "my-struct";
    }
    static expression to_table(const my_struct& val)
    {
        return {
            expression(val.a).keyed("a"),
            expression(val.b).keyed("b"),
        };
    }
    static my_struct from_expr(const expression& expr)
    {
        return {
            expr.attr("a"),
            expr.attr("b"),
        };
    }
};




//=============================================================================
int main()
{
    crt::kernel kern;
    kern.define("apply",     builtin::apply);
    kern.define("attr",      builtin::attr);
    kern.define("concat",    builtin::concat);
    kern.define("dict",      builtin::dict);
    kern.define("eval",      builtin::eval);
    kern.define("first",     builtin::first);
    kern.define("item",      builtin::item);
    kern.define("join",      builtin::join);
    kern.define("last",      builtin::last);
    kern.define("len",       builtin::len);
    kern.define("list",      builtin::list);
    kern.define("map",       builtin::map);
    kern.define("nest",      builtin::nest);
    kern.define("range",     builtin::range);
    kern.define("rest",      builtin::rest);
    kern.define("reverse",   builtin::reverse);
    kern.define("second",    builtin::second);
    kern.define("slice",     builtin::slice);
    kern.define("sort",      builtin::sort);
    kern.define("table",     builtin::table);
    kern.define("type",      builtin::type);
    kern.define("unparse",   builtin::unparse);
    kern.define("zip",       builtin::zip);
    kern.define("my-struct", crt::init<my_struct>());


    while (! std::cin.eof())
    {
        std::string line;
        std::getline(std::cin, line);

        try {
            auto expr = crt::parse(line);
            std::cout << expr.resolve(kern, crt::call_adapter()).unparse() << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
    return 0;
}
