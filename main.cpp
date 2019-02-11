#include <iostream>
#include "kernel.hpp"




//=============================================================================
namespace crt {
namespace core {
    expression table     (const expression& e);
    expression list      (const expression& e);
    expression dict      (const expression& e);
    expression switch_   (const expression& e);
    expression item      (const expression& e);
    expression attr      (const expression& e);
    expression range     (const expression& e);
    expression slice     (const expression& e);
    expression concat    (const expression& e);
    expression join      (const expression& e);
    expression apply     (const expression& e);
    expression zip       (const expression& e);
    expression map       (const expression& e);
    expression merge_key (const expression& e);
    expression nest      (const expression& e);
    expression first     (const expression& e);
    expression second    (const expression& e);
    expression rest      (const expression& e);
    expression last      (const expression& e);
    expression len       (const expression& e);
    expression sort      (const expression& e);
    expression reverse   (const expression& e);
    expression type      (const expression& e);
    expression eval      (const expression& e);
    expression unparse   (const expression& e);
    expression func      (const expression& e);
}
}




//=============================================================================
crt::expression crt::core::table(const crt::expression& e)
{
    return e;
}

crt::expression crt::core::list(const crt::expression& e)
{
    return e.list();
}

crt::expression crt::core::dict(const crt::expression& e)
{
    return e.dict();
}

crt::expression crt::core::item(const crt::expression& e)
{
    auto arg = e.first();
    auto ind = e.second();

    if (ind.has_type(crt::data_type::i32))
    {
        return arg.item(int(ind));
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

crt::expression crt::core::attr(const crt::expression& e)
{
    return e.first().attr(e.second());
}

crt::expression crt::core::range(const crt::expression& e)
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

crt::expression crt::core::slice(const crt::expression& e)
{
    return item({e.first(), range(e.rest())});
}

crt::expression crt::core::concat(const crt::expression& e)
{
    std::vector<crt::expression> result;

    for (const auto& part : e)
    {
        result.insert(result.end(), part.begin(), part.end());
    }
    return result;
}

crt::expression crt::core::join(const crt::expression& e)
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

crt::expression crt::core::apply(const crt::expression& e)
{
    return e.first().call(e.second());
}

crt::expression crt::core::zip(const crt::expression& e)
{
    return e.zip();
}

crt::expression crt::core::map(const crt::expression& e)
{
    std::vector<crt::expression> result;

    for (const auto& argset : e.rest().zip())
    {
        result.push_back(e.first().call(argset));
    }
    return result;
}

crt::expression crt::core::merge_key(const crt::expression& e)
{
    return e.rest().merge_key(e.first());
}

crt::expression crt::core::first(const crt::expression& e)
{
    return e.first().first();
}

crt::expression crt::core::second(const crt::expression& e)
{
    return e.first().second();
}

crt::expression crt::core::rest(const crt::expression& e)
{
    return e.first().rest();
}

crt::expression crt::core::last(const crt::expression& e)
{
    return e.first().last();
}

crt::expression crt::core::len(const crt::expression& e)
{
    return int(e.first().size());
}

crt::expression crt::core::switch_(const crt::expression& e)
{
    return e.first() ? e.second() : e.third();
}

crt::expression crt::core::sort(const crt::expression& e)
{
    return e.first().sort();
}

crt::expression crt::core::reverse(const crt::expression& e)
{
    auto arg = e.first();
    return crt::expression(arg.rbegin(), arg.rend());
}

crt::expression crt::core::nest(const crt::expression& e)
{
    return e.first().nest();
}

crt::expression crt::core::type(const crt::expression& e)
{
    return std::string(e.first().type_name());
}

crt::expression crt::core::eval(const crt::expression& e)
{
    return crt::parse(e.first());
}

crt::expression crt::core::unparse(const crt::expression& e)
{
    return e.first().unparse();
}

crt::expression crt::core::func(const crt::expression& e)
{
    auto locals = std::unordered_set<std::string>();
    auto localized = e.first();

    for (const auto& sym : e.symbols())
    {
        if (! sym.empty() && sym[0] == '@')
        {
            auto local_sym = sym.substr(1);
            localized = localized.relabel(sym, local_sym);
            locals.insert(local_sym);
        }
    }

    auto result_func = [localized, locals] (const crt::expression& args)
    {
        auto result = localized;

        for (auto var : locals)
        {
            if (var.empty())
            {
                result = result.replace(var, args.first());
            }
            else if (std::isdigit(var[0]))
            {
                result = result.replace(var, args.item(var[0] - '0' - 1));
            }
            else
            {
                result = result.replace(var, args.attr(var));
            }
        }
        return result;
    };
    return crt::func_t(result_func);
}




//=============================================================================
int main()
{
    using namespace crt::core;

    crt::kernel kern;
    kern.define("apply",     apply);
    kern.define("attr",      attr);
    kern.define("concat",    concat);
    kern.define("dict",      dict);
    kern.define("eval",      eval);
    kern.define("first",     first);
    kern.define("func",      func);
    kern.define("item",      item);
    kern.define("join",      join);
    kern.define("last",      last);
    kern.define("len",       len);
    kern.define("list",      list);
    kern.define("map",       map);
    kern.define("merge-key", merge_key);
    kern.define("nest",      nest);
    kern.define("range",     range);
    kern.define("rest",      rest);
    kern.define("reverse",   reverse);
    kern.define("second",    second);
    kern.define("slice",     slice);
    kern.define("sort",      sort);
    kern.define("switch",    switch_);
    kern.define("table",     table);
    kern.define("type",      type);
    kern.define("unparse",   unparse);
    kern.define("zip",       zip);

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
            std::cerr << e.what() << std::endl;
        }
    }
    return 0;
}
