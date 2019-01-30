#include <cassert>
#include <iostream>
#include "kernel.hpp"
#include "crt.hpp"




//=============================================================================
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
    return e.first().item(e.second().get_i32());
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

crt::expression zip(const crt::expression& e)
{
    return e.zip();
}




//=============================================================================
int main()
{
    crt::kernel kern;
    kern.insert_literal("list", crt::expression(list));
    kern.insert_literal("dict", crt::expression(dict));
    kern.insert_literal("item", crt::expression(item));
    kern.insert_literal("attr", crt::expression(attr));
    kern.insert_literal("range", crt::expression(range));
    kern.insert_literal("concat", crt::expression(concat));
    kern.insert_literal("join", crt::expression(join));
    kern.insert_literal("zip", crt::expression(zip));

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
