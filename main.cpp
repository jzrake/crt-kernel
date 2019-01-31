#include <cassert>
#include <iostream>
#include "kernel.hpp"
#include "crt.hpp"




//=============================================================================
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

crt::expression reverse(const crt::expression& e)
{
    auto arg = e.first();
    return crt::expression(arg.rbegin(), arg.rend());
}

crt::expression type(const crt::expression& e)
{
    return std::string(e.first().type_name());
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
    static expression to_expr(const my_struct& val)
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
    kern.insert_literal("table", crt::expression(table));
    kern.insert_literal("list", crt::expression(list));
    kern.insert_literal("item", crt::expression(item));
    kern.insert_literal("dict", crt::expression(dict));
    kern.insert_literal("attr", crt::expression(attr));
    kern.insert_literal("range", crt::expression(range));
    kern.insert_literal("slice", crt::expression(slice));
    kern.insert_literal("len", crt::expression(len));
    kern.insert_literal("concat", crt::expression(concat));
    kern.insert_literal("join", crt::expression(join));
    kern.insert_literal("zip", crt::expression(zip));
    kern.insert_literal("map", crt::expression(map));
    kern.insert_literal("apply", crt::expression(apply));
    kern.insert_literal("first", crt::expression(first));
    kern.insert_literal("second", crt::expression(second));
    kern.insert_literal("rest", crt::expression(rest));
    kern.insert_literal("last", crt::expression(last));
    kern.insert_literal("reverse", crt::expression(reverse));
    kern.insert_literal("type", crt::expression(type));
    kern.insert_literal("my-struct", crt::init<my_struct>());


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
