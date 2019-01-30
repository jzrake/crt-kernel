#include "crt.hpp"




//=============================================================================
static std::string what_wrong_type(const char* expected, const crt::expression& arg, std::size_t index)
{
    return "expected "
    + std::string(expected)
    +" at index + "
    + std::to_string(index)
    + ", got "
    + arg.type_name();
}




//=============================================================================
int crt::check_i32(const crt::expression& e, std::size_t index)
{
    auto arg = e.item(index);

    if (arg.has_type(crt::data_type::i32))
    {
        return arg.get_i32();
    }
    throw std::runtime_error(what_wrong_type("i32", arg, index));
}

double crt::check_f64(const crt::expression& e, std::size_t index)
{
    auto arg = e.item(index);

    if (arg.has_type(crt::data_type::f64))
    {
        return arg.get_f64();
    }
    throw std::runtime_error(what_wrong_type("f64", arg, index));
}

std::string crt::check_str(const crt::expression& e, std::size_t index)
{
    auto arg = e.item(index);

    if (arg.has_type(crt::data_type::str))
    {
        return arg.str();
    }
    throw std::runtime_error(what_wrong_type("str", arg, index));
}

std::vector<crt::expression> crt::check_table(const crt::expression& e, std::size_t index)
{
    auto arg = e.item(index);

    if (arg.has_type(crt::data_type::table))
    {
        return std::vector<crt::expression> (arg.begin(), arg.end());
    }
    throw std::runtime_error(what_wrong_type("table", arg, index));
}

std::vector<crt::expression> crt::check_list(const crt::expression& e, std::size_t index)
{
    auto arg = e.item(index);

    if (arg.has_type(crt::data_type::table))
    {
        auto list = arg.list();

        if (list.size() == arg.size())
        {
            return list;
        }
    }
    throw std::runtime_error(what_wrong_type("list", arg, index));
}

std::unordered_map<std::string, crt::expression> crt::check_dict(const crt::expression& e, std::size_t index)
{
    auto arg = e.item(index);

    if (arg.has_type(crt::data_type::table))
    {
        auto dict = arg.dict();

        if (dict.size() == arg.size())
        {
            return dict;
        }
    }
    throw std::runtime_error(what_wrong_type("dict", arg, index));
}
