#include "crt-core.hpp"




//=============================================================================
void crt::core::import(crt::kernel& k)
{
    k.define("apply",     apply);
    k.define("attr",      attr);
    k.define("call",      call);
    k.define("concat",    concat);
    k.define("dict",      dict);
    k.define("eval",      eval);
    k.define("first",     first);
    k.define("eq",        eq);
    k.define("func",      func);
    k.define("ge",        ge);
    k.define("gt",        gt);
    k.define("index",     index);
    k.define("item",      item);
    k.define("join",      join);
    k.define("last",      last);
    k.define("len",       len);
    k.define("le",        le);
    k.define("lt",        lt);
    k.define("list",      list);
    k.define("map",       map);
    k.define("merge-key", merge_key);
    k.define("nest",      nest);
    k.define("ne",        ne);
    k.define("range",     range);
    k.define("rest",      rest);
    k.define("reverse",   reverse);
    k.define("second",    second);
    k.define("slice",     slice);
    k.define("sort",      sort);
    k.define("switch",    switch_);
    k.define("table",     table);
    k.define("type",      type);
    k.define("unparse",   unparse);
    k.define("with",      with);
    k.define("zip",       zip);
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

crt::expression crt::core::with(const crt::expression& e)
{
    return e.first().substitute_in(e.rest());
}

crt::expression crt::core::call(const crt::expression& e)
{
    auto scope = std::unordered_map<std::string, expression>();

    for (const auto& part : e.rest())
    {
        scope[part.key()] = part;
    }
    return e.first().resolve(scope, call_adapter());
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

        for (const auto& var : locals)
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
    std::string sep = e.attr("sep").otherwise("");

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

crt::expression crt::core::eq(const crt::expression& e)
{
    return e.first() == e.second();
}

crt::expression crt::core::ne(const crt::expression& e)
{
    return e.first() != e.second();
}

crt::expression crt::core::gt(const crt::expression& e)
{
    return e.first() > e.second();
}

crt::expression crt::core::ge(const crt::expression& e)
{
    return e.first() >= e.second();
}

crt::expression crt::core::lt(const crt::expression& e)
{
    return e.first() < e.second();
}

crt::expression crt::core::le(const crt::expression& e)
{
    return e.first() <= e.second();
}

crt::expression crt::core::index(const crt::expression& e)
{
    auto container = e.first();
    auto item = e.second();
    auto it = std::find(container.begin(), container.end(), item);

    if (it != container.end())
    {
        return int(it - container.begin());
    }
    return {};
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
