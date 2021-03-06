#pragma once
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#include <immer/set.hpp>
#include <immer/box.hpp>




//=============================================================================
namespace crt
{


    //=========================================================================
    class kernel;
    class parser;
    class expression;
    class call_adapter;
    enum class data_type { none, i32, f64, str, symbol, data, function, table };


    //=========================================================================
    struct user_data
    {
        virtual ~user_data() {}


        /**
         * Return a type name for the user data.
         */
        virtual const char* type_name() const = 0;


        /**
         * Convert this user_data to an expression. The return value should
         * probably be a table, but anything that's not a user_data of the
         * same type is safe. Returning a user_data of the same type would
         * cause the unparse method to recurse forever.
         */
        virtual expression to_table() const = 0;
    };


    //=========================================================================
    using func_t = std::function<expression(expression)>;
    using data_t = std::shared_ptr<user_data>;
    using cont_t = immer::flex_vector<immer::box<expression>>;


    //=========================================================================
    template<typename T> struct capsule;
    template<typename T> struct type_info;
    template<typename T> static data_t make_data(const T&);
    template<typename T> static func_t init();
    static inline expression symbol(const std::string&);
    static inline expression parse(const std::string&);
    static inline expression parse(const char*);


    //=========================================================================
    class parser_error : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };
};




//=============================================================================
class crt::expression
{
public:
    struct none {};


    /**
     * Return an expression converted from a custom data type. You must
     * specialize the type_info struct, and implement the to_table method for
     * this to compile.
     */
    template<typename T>
    static expression from(const T& val) { return type_info<T>::to_table(val); }


    /**
     * Return this expression converted to a custom data type. You must
     * specialize the type_info struct, and implement the from_expr method for
     * this to compile.
     */
    template<typename T>
    T to() const { return type_info<T>::from_expr(*this); }


    /**
     * Construct an expression from a pair of iterators.
     */
    template<typename IteratorType>
    expression(IteratorType first, IteratorType second)
    {
        auto transient_parts = parts.transient();

        while (first != second)
        {
            transient_parts.push_back(*first);
            ++first;
        }
        parts = transient_parts.persistent();
        type = parts.empty() ? data_type::none : data_type::table;
    }


    /**
     * Initializer list constructor
     */
    expression(std::initializer_list<expression> i) : expression(i.begin(), i.end()) {}


    /**
     * Default constructor
     */
    expression()                          : type(data_type::none) {}
    expression(const none&)               : type(data_type::none) {}
    expression(int vali32)                : type(data_type::i32), vali32(vali32) {}
    expression(float valf64)              : type(data_type::f64), valf64(valf64) {}
    expression(double valf64)             : type(data_type::f64), valf64(valf64) {}
    expression(const char* valstr)        : type(data_type::str), valstr(valstr) {}
    expression(const std::string& valstr) : type(data_type::str), valstr(valstr) {}
    expression(data_t valdata)            : type(data_type::data), valdata(valdata) {}
    expression(func_t valfunc)            : type(data_type::function), valfunc(valfunc) {}
    expression(cont_t parts)              : parts(parts)
    {
        type = parts.empty() ? data_type::none : data_type::table;
    }


    const auto& get_i32()      const { return vali32; }
    const auto& get_f64()      const { return valf64; }
    const auto& get_str()      const { return valstr.get(); }
    const auto& get_sym()      const { return valsym.get(); }
    const auto& get_func()     const { return valfunc; }
    const auto& get_data()     const { return valdata; }
    const auto& key()          const { return keyword.get(); }
    auto dtype()               const { return type; }
    auto has_type(data_type t) const { return type == t; }
    auto begin()               const { return parts.begin(); }
    auto end()                 const { return parts.end(); }
    auto rbegin()              const { return parts.rbegin(); }
    auto rend()                const { return parts.rend(); }
    expression first()         const { return parts.size() > 0 ? parts[0] : none(); }
    expression second()        const { return parts.size() > 1 ? parts[1] : none(); }
    expression third()         const { return parts.size() > 2 ? parts[2] : none(); }
    expression rest()          const { return parts.size() > 1 ? expression(begin() + 1, end()) : none(); }
    expression last()          const { return parts.size() > 0 ? parts.back() : none(); }
    operator bool()            const { return as_boolean(); }
    operator int()             const { return as_i32(); }
    operator float()           const { return as_f64(); }
    operator double()          const { return as_f64(); }
    operator std::string()     const { return as_str(); }
    operator std::size_t()     const { return as_i32(); }


    /**
     * Return a copy of this expression with a different key.
     */
    expression keyed(immer::box<std::string> kw) const
    {
        auto e = *this;
        e.keyword = kw;
        return e;
    }


    /**
     * Convenience method to return a default value for this expression, if it
     * evaluates empty.
     */
    expression otherwise(const expression& e) const
    {
        return ! empty() ? *this : e;
    }


    /**
     * Return the expression part at the specified index. Throws an
     * out_of_range exception if the index is invalid.
     */
    const expression& at(std::size_t index) const
    {
        return parts.at(index);
    }


    /**
     * Return the number of parts if this is a table, or zero otherwise.
     */
    std::size_t size() const
    {
        return parts.size();
    }


    /**
     * Return true if this is none or an empty table (the two are supposed to
     * be equivalent).
     */
    bool empty() const
    {
        return
        (type == data_type::none) ||
        (type == data_type::table && parts.empty());
    }


    bool as_boolean() const
    {
       switch(type)
       {
           case data_type::none     : return false;
           case data_type::i32      : return vali32;
           case data_type::f64      : return valf64;
           case data_type::str      : return ! valstr->empty();
           case data_type::symbol   : return ! valsym->empty();
           case data_type::data     : return valdata != nullptr;
           case data_type::function : return valfunc != nullptr;
           case data_type::table    : return ! parts.empty();
       }
       return false;
    }


    /**
     * Return the best-guess integer equivalent for this expression: floats
     * are truncated and strings are converted via std::stoi.
     */
    int as_i32() const
    {
        switch(type)
        {
            case data_type::i32: return vali32;
            case data_type::f64: return valf64;
            case data_type::str: try { return std::stoi(valstr); } catch (...) { return 0; }
            default: return 0;
        }
    }


    /**
     * Return the best-guess double equivalent for this expression: ints are
     * promoted and strings are converted via std::stod.
     */
    double as_f64() const
    {
        switch(type)
        {
            case data_type::i32: return vali32;
            case data_type::f64: return valf64;
            case data_type::str: try { return std::stod(valstr); } catch (...) { return 0.0; }
            default: return 0;
        }
    }


    /**
     * Return the best-guess string equivalent for this expression. This returns
     * an un-quoted string for both data_type::str and data_type::symbol, but it
     * unparses tables.
     */
    std::string as_str() const
    {
        switch(type)
        {
            case data_type::none     : return "()";
            case data_type::i32      : return std::to_string(vali32);
            case data_type::f64      : return std::to_string(valf64);
            case data_type::str      : return valstr;
            case data_type::symbol   : return valsym;
            case data_type::data     : return valdata ? "()" : valdata->type_name();
            case data_type::function : return "<func>";
            case data_type::table    : return unparse();
        }
        return std::string();
    }


    /**
     * Try to return a string interpretation of this expression that can be
     * restored by the parser. The only cases where this can fail are if the
     * expression is a function, or if it is a user_data that has not
     * implemented the unparse method.
     */
    std::string unparse() const
    {
        auto pre = keyword->empty() ? "" : *keyword + "=";

        switch (type)
        {
            case data_type::none     : return pre + "()";
            case data_type::i32      : return pre + std::to_string(vali32);
            case data_type::f64      : return pre + std::to_string(valf64);
            case data_type::str      : return pre + "'" + *valstr + "'";
            case data_type::symbol   : return pre + *valsym;
            case data_type::data     : return pre + valdata->to_table().unparse();
            case data_type::function : return pre + "<func>";
            case data_type::table:
            {
                std::string res;

                for (const auto& part : parts)
                {
                    res += " " + part->unparse();
                }
                return pre + "(" + res.substr(1) + ")";
            }
        }
        return std::string();
    }


    /**
     * Return the name of the data type.
     */
    const char* type_name() const
    {
        switch (type)
        {
            case data_type::none:      return "none";
            case data_type::i32:       return "i32";
            case data_type::f64:       return "f64";
            case data_type::str:       return "str";
            case data_type::symbol:    return "symbol";
            case data_type::data:      return valdata->type_name();
            case data_type::function:  return "function";
            case data_type::table:     return "table";
        }
        return nullptr;
    };


    /**
     * Return true if this expression is a user data of the template parameter
     * type.
     */
    template<typename T>
    auto has_type() const
    {
        return type_name() == crt::type_info<T>::name();
    }


    /**
     * If this is a user data of the given type, return a reference to
     * the underlying value. Otherwise, throws a runtime_error.
     */
    template<typename T>
    const T& check_data() const
    {
        if (has_type(data_type::data))
        {
            if (auto data = std::dynamic_pointer_cast<capsule<T>>(valdata))
            {
                return data->value;
            }
        }
        throw std::runtime_error(std::string("wrong type: expected ")
            + type_info<T>::name() + ", got "
            + type_name());
    }


    /**
     * Return a set of all symbols referenced at any level in this expression.
     */
    immer::set<std::string> symbols() const
    {
        switch (type)
        {
            case data_type::symbol:
            {
                return immer::set<std::string>().insert(valsym);
            }
            case data_type::table:
            {
                auto result = immer::set<std::string>();

                for (const auto& part : parts)
                {
                    for (const auto& s : part->symbols())
                    {
                        result = std::move(result).insert(s);
                    }
                }
                return result;
            }
            default: return {};
        }
    }


    /**
     * Convert this expression to one whose truth value is the opposite of
     * this.
     */
    expression toggle() const
    {
        return as_boolean() ? none() : expression(1);
    }


    /**
     * Return an expression built from the parts of this, appending the additional
     * part provided.
     */
    expression append(const expression& e) const
    {
        return parts.push_back(e);
    }


    /**
     * Return an expression built from the parts of this one and the parts of the
     * one specified.
     */
    expression concat(const expression& more) const
    {
        return parts + more.parts;
    }


    /**
     * Insert the parts of another expression at the given index.
     */
    expression splice(std::size_t index, const expression& e) const
    {
        return parts.insert(index, e.parts);
    }


    /**
     * Insert another expression at the front.
     */
    expression prepend(const expression& e) const
    {
        return parts.push_front(e);
    }


    /**
     * Insert another expression at the given index.
     */
    expression insert(std::size_t index, const expression& e) const
    {
        return parts.insert(index, e);
    }


    /**
     * Return an expression with the final num elements removed.
     */
    expression pop_back(std::size_t num=1) const
    {
        return parts.take(parts.size() - num);
    }


    /**
     * Return an expression with the first num elements removed.
     */
    expression pop_front(std::size_t num=1) const
    {
        return parts.drop(num);
    }


    /**
     * Return an expression with only the first num parts.
     */
    expression take(std::size_t num) const
    {
        return parts.take(num);
    }


    /**
     * Return an expression with the part at the given index erased.
     */
    expression erase(std::size_t index) const
    {
        return parts.erase(index);
    }


    /**
     * Return an expression with the part at the given index erased.
     */
    expression erase(std::size_t first_index, std::size_t final_index) const
    {
        return parts.erase(first_index, final_index);
    }


    /**
     * Return this expression as the sole element of a new table:
     * 
     *                   key=val -> (key=val) .
     *
     */
    expression nest() const
    {
        return crt::expression({*this});
    }


    /**
     * If this is a table, return the unkeyed part at the given index
     * (equivalent to e.items()[index], except that it returns none if
     * out-of-bounds. This may be more or less efficient than first getting
     * the result of items() depending on the context. If this is a string,
     * then return the character at the specified index if it's within range,
     * and none otherwise.
     */
    expression item(std::size_t index) const
    {
        if (type == data_type::str)
        {
            return index < valstr->size() ? expression(valstr->substr(index, 1)) : none();
        }
        std::size_t n = 0;

        for (const auto& part : parts)
        {
            if (part->keyword->empty())
            {
                if (n == index)
                {
                    return part;
                }
                ++n;
            }
        }
        return {};
    }


    /**
     * Return the last part whose key matches the one specified, or none if
     * none exists (including if this is not a table). This function removes
     * the keyword part of the resulting expression.
     */
    expression attr(const std::string& key) const
    {
        auto part = parts.rbegin();

        while (part != parts.rend())
        {
            if (*(*part)->keyword == key)
            {
                return (*part)->keyed(std::string());
            }
            ++part;
        }
        return {};    
    }


    /**
     * Return the expression part at the given linear index, or none if it's
     * out-of-range.
     */
    expression part(std::size_t index) const
    {
        if (index < parts.size())
        {
            return parts.at(index);
        }
        return {};
    }


    /**
     * Evaluate this expression using the specified scope and call adapter.
     * Symbols are resolved in the given scope. Tables are interpreted by the
     * call adapter, which should find the first table element is a function.
     * The remaining arguments should be resolved recursively, and passed as
     * arguments to that function. If the first argument is not a function,
     * the call adapter may interpret the expression as a table.
     */
    template<typename Mapping, typename CallAdapter>
    expression resolve(const Mapping& scope, const CallAdapter& adapter) const
    {
        switch (type)
        {
            case data_type::table:
            {
                return adapter.call(scope, *this).keyed(keyword);
            }
            case data_type::symbol:
            {
                try {
                    return scope.at(valsym).keyed(keyword);
                }
                catch (const std::out_of_range& e)
                {
                    return *this;
                }
            }
            default: return *this;
        }
    }


    /**
     * Return this expression with all of its symbols (at any depth) having
     * the name `from` renamed to `to`.
     */
    expression relabel(const std::string& from, const std::string& to) const
    {
        switch (type)
        {
            case data_type::symbol:
            {
                return symbol(*valsym == from ? to : *valsym).keyed(keyword);
            }
            case data_type::table:
            {
                auto result = parts.transient();
                std::size_t n = 0;

                for (const auto& part : parts)
                {
                    if (part->has_type(data_type::symbol) ||
                        part->has_type(data_type::table))
                    {
                        result.set(n, part->relabel(from, to));
                    }
                    ++n;
                }
                return expression(result.persistent()).keyed(keyword);
            }
            default: return *this;
        }
    }


    /**
     * Replace all instances of a symbol with the given expression. Recurses
     * if this is a table.
     */
    expression replace(const std::string& symbol, const crt::expression& e) const
    {
        switch (type)
        {
            case data_type::symbol:
            {
                return *valsym == symbol ? e.keyed(keyword) : *this;
            }
            case data_type::table:
            {
                auto result = parts.transient();
                std::size_t n = 0;

                for (const auto& part : parts)
                {
                    if (part->has_type(data_type::symbol) ||
                        part->has_type(data_type::table))
                    {
                        result.set(n, part->replace(symbol, e));
                    }
                    ++n;
                }
                return expression(result.persistent()).keyed(keyword);
            }
            default: return *this;
        }
    }


    /**
     * Replace all expression *values* equaling the first one given with the
     * second. Recurse if this is a table (table values are not tested to see
     * if they match). The key part of both parameters is disregarded, and the
     * key of the swapped values is kept as this one, e.g.
     *
     *          (a=1 b=2).substitute(1, 2) -> (a=2 b=2)
     *
     */
    expression substitute(const crt::expression& value, const crt::expression& new_value) const
    {
        switch (type)
        {
            case data_type::table:
            {
                auto result = parts.transient();
                std::size_t n = 0;

                for (const auto& part : parts)
                {
                    if (part->has_type(data_type::table))
                    {
                        result.set(n, part->substitute(value, new_value));
                    }
                    ++n;
                }
                return expression(result.persistent()).keyed(keyword);
            }
            default: return has_same_value(value) ? new_value.keyed(keyword) : *this;
        }
    }


    /**
     * Call substitute on each of the key-values pairs in the given
     * expression.
     */
    expression substitute_in(const crt::expression& lookup) const
    {
        auto result = *this;

        for (const auto& part : lookup)
        {
            result = result.substitute(*part->keyword, part);
        }
        return result;
    }


    /**
     * Replace all parts having the specified key, with the value part of the
     * given expression (its key is disregarded). Does not recurse.
     */
    expression with_attr(const std::string& key, const crt::expression& e) const
    {
        auto result = parts.transient();
        std::size_t n = 0;

        for (const auto& part : parts)
        {
            if (*part->keyword == key)
            {
                result.set(n, e.keyed(part->keyword));
            }
            ++n;
        }
        return expression(result.persistent());
    }


    /**
     * Replace the part at the given index with a new expression. This method
     * is not named 'with_item', because the index is linear in the raw
     * container of parts, not the 'i-th' unkeyed part.
     */
    expression with_part(std::size_t index, const crt::expression& e) const
    {
        if (index < size())
        {
            return expression(parts.set(index, e)).keyed(keyword);
        }
        return *this;
    }


    /**
     * Return this expression, without any of its parts that have the given
     * key.
     */
    expression without_attr(const std::string& key) const
    {
        auto result = parts;
        std::size_t n = 0;

        for (const auto& part : parts)
        {
            if (*part->keyword == key)
            {
                result = std::move(result).erase(n);
            }
            else
            {
                ++n;
            }
        }
        return expression(result).keyed(keyword);
    }


    /**
     * Return an expression with the part at the given index (keyed or
     * unkeyed) removed. If the index is not in range, then this expression is
     * returned.
     */
    expression without_part(std::size_t index) const
    {
        if (index < size())
        {
            return expression(parts.erase(index)).keyed(keyword);
        }
        return *this;
    }


    /**
     * A generalization of the with_attr and with_part methods. The address
     * expression is interpreted as a sequence of either attribute keys or
     * part indexes. Keys in the address are disregarded.
     */
    expression with(const expression& address, const expression& e) const
    {
        auto front = address.first();

        if (front.has_type(data_type::str))
        {
            return with_attr(front, attr(front.get_str()).with(address.rest(), e));
        }
        if (front.has_type(data_type::i32))
        {
            return with_part(front, part(front.get_i32()).with(address.rest(), e));
        }
        return e;
    }


    /**
     * Return this expression, with the item at the given address removed.
     *
     */
    expression without(const expression& address) const
    {
        if (! has_type(data_type::table))
        {
            return *this;
        }
        if (address.size() <= 1)
        {
            auto front = address.first().otherwise(address);

            if (front.has_type(data_type::str))
            {
                return without_attr(front.get_str());
            }
            if (front.has_type(data_type::i32))
            {
                return without_part(front.get_i32());
            }            
        }

        auto result = parts.transient();
        std::size_t n = 0;

        for (const auto& part : parts)
        {
            result.set(n, part->without(address.rest()));
            ++n;
        }
        return expression(result.persistent()).keyed(keyword);
    }


    /**
     * Return a nested item, using the given expression as a sequence of keys
     * or indexes.
     */
    expression address(const expression& address) const
    {
        auto front = address.first();

        if (front.has_type(data_type::str))
        {
            return attr(front.get_str()).address(address.rest());
        }
        if (front.has_type(data_type::i32))
        {
            return part(front.get_i32()).address(address.rest());
        }
        return *this;
    }


    /**
     * Call an expression that is a function with the given args, if it is a
     * function. Otherwise this method throws a runtime_error.
     */
    expression call(const expression& args) const
    {
        if (! has_type(crt::data_type::function) || ! valfunc)
        {
            throw std::runtime_error("expression is not a function");
        }
        return valfunc(args).keyed(keyword);
    }


    /**
     * Test for equivalence of only the type and value.
     */
    bool has_same_value(const crt::expression& other) const
    {
        return
        type       != data_type::function && // no equality testing for function types
        other.type != data_type::function &&
        type    == other.type    &&
        vali32  == other.vali32  &&
        valf64  == other.valf64  &&
        valstr  == other.valstr  &&
        valsym  == other.valsym  &&
        valdata == other.valdata &&
        parts   == other.parts;
    }


    /**
     * Test for equality between two expressions. Equality means exact
     * equivalence of key, type, and value. Functions are always unequal, even
     * to themselves.
     */
    bool operator==(const expression& other) const
    {
        return has_same_value(other) && keyword == other.keyword;
    }


    /**
     * Test for inequality.
     */
    bool operator!=(const expression& other) const
    {
        return ! operator==(other);
    }


private:
    data_type               type = data_type::none;
    immer::box<std::string> keyword;
    int                     vali32 = 0;
    double                  valf64 = 0.0;
    immer::box<std::string> valstr;
    immer::box<std::string> valsym;
    crt::data_t             valdata;
    crt::func_t             valfunc;
    cont_t                  parts;
    friend expression symbol(const std::string&);
    friend class parser;
};




//=========================================================================
/**
 * Return a symbol expression.
 */
crt::expression crt::symbol(const std::string& v)
{
    auto e = expression();
    e.type = data_type::symbol;
    e.valsym = v;
    return e;
}


/**
 * Create a user_data from the given value. You'll get a compile error if
 * there is no class definition for crt::type_info<T>.
 */
template<typename T>
crt::data_t crt::make_data(const T& v)
{
    return std::dynamic_pointer_cast<user_data>(std::make_shared<capsule<T>>(v));
}


/**
 * Return a function expression that constructs a user_data of the given type.
 * This is useful in concisely defining API's.
 */
template<typename T>
crt::func_t crt::init()
{
    return [] (const crt::expression& e) -> crt::expression
    {
        return crt::make_data(e.to<T>());
    };
}


/**
 * Holder for user_data values. Client code probably does not need to use
 * this.
 */
template<typename T>
struct crt::capsule : public crt::user_data
{
    capsule() {}
    capsule(const T& value) : value(value) {}
    const char* type_name() const override { return type_info<T>::name(); }
    expression to_table() const override { return type_info<T>::to_table(value); }
    T value;
};


/**
 * This is a good general purpose call_adapter to be used with
 * expression::resolve. You can write your own, of course!
 */
class crt::call_adapter
{
public:
    template<typename Mapping>
    crt::expression call(const Mapping& scope, const crt::expression& expr) const
    {
        auto head = expr.first().resolve(scope, *this);
        auto args = cont_t().transient();

        for (const auto& part : expr.rest())
        {
            args.push_back(part->resolve(scope, *this));
        }

        if (head.has_type(crt::data_type::function))
        {
            return head.call(args.persistent());
        }
        return head.nest().concat(args.persistent());
    }
};




//=============================================================================
class crt::parser
{

public:


    //=========================================================================
    static expression parse(const char* source)
    {
        if (std::strlen(source) > 0 && source[0] == '(')
        {
            return parse_part(source);
        }

        auto parts = cont_t().transient();
        auto c = source;

        while (*c != '\0')
        {
            parts.push_back(parse_part(c));
        }
        return parts.size() == 1 ? parts[0] : parts.persistent();
    }


private:


    //=========================================================================
    static bool is_symbol_character(char e)
    {
        return std::isalnum(e) || e == '_' || e == '-' || e == '+' || e == ':' || e == '@';
    }

    static bool is_leading_symbol_character(char e)
    {
        return std::isalpha(e) || e == '_' || e == '-' || e == '+' || e == ':' || e == '@';
    }

    static bool is_number(const char* d)
    {
        if (std::isdigit(*d))
        {
            return true;
        }
        else if (*d == '.')
        {
            return std::isdigit(d[1]);
        }
        else if (*d == '+' || *d == '-')
        {
            return std::isdigit(d[1]) || (d[1] == '.' && std::isdigit(d[2]));
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

        while (std::isdigit(*c) || *c == '.' || *c == 'e' || *c == 'E')
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

        if (! (std::isspace(*c) || *c == '\0' || *c == ')'))
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
        return symbol(std::string(start, c));
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

        if (! (std::isspace(*c) || *c == '\0' || *c == ')'))
        {
            throw parser_error("syntax error: non-whitespace character following single-quoted string");
        }
        return expression(std::string(start + 1, c - 1));
    }

    static expression parse_expression(const char*& c)
    {
        auto e = cont_t().transient();
        auto end = find_closing_parentheses(c);
        ++c;

        while (c != end)
        {
            if (*c == '\0')
            {
                throw parser_error("syntax error: unterminated expression");
            }
            else if (std::isspace(*c) || *c == ')')
            {
                ++c;
            }
            else
            {
                e.push_back(parse_part(c));
            }
        }
        return e.persistent();
    }

    static expression parse_part(const char*& c)
    {
        std::string kw;

        while (*c != '\0')
        {
            if (std::isspace(*c))
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
            else if (is_leading_symbol_character(*c))
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
                throw parser_error("syntax error: unknown character '" + std::string(c, c + 1) + "'");
            }
        }
        return expression();
    }
};




//=============================================================================
crt::expression crt::parse(const std::string& source)
{
    return parser::parse(source.data());
}

crt::expression crt::parse(const char* source)
{
    return parser::parse(source);
}




//=============================================================================
#ifdef TEST_EXPRESSION
#include "catch.hpp"
#include "immer/map.hpp"
using namespace crt;




//=============================================================================
TEST_CASE("expression passes basic sanity tests", "[expression]")
{
    REQUIRE(expression{1, 2} == expression{1, 2});
    REQUIRE(expression{1, 2} != expression());
    REQUIRE(expression().empty());
    REQUIRE(expression().dtype() == data_type::none);
    REQUIRE(expression({}).empty());
    REQUIRE(expression({}).dtype() == data_type::none);
    REQUIRE(expression() == expression::none());
    REQUIRE(expression() == expression({}));
    REQUIRE(expression() == expression{});
    REQUIRE(expression() == expression({}));
    REQUIRE(expression() != expression({1, 2, 3}));
}




TEST_CASE("nested expression can be constructed by hand", "[expression]")
{
    expression e {
        1,
        2.3,
        std::string("sdf"),
        symbol("a"),
        {1, symbol("b"), symbol("b")}};

    REQUIRE(e.dtype() == data_type::table);
    REQUIRE(e.size() == 5);
    REQUIRE(e.item(0).dtype() == data_type::i32);
    REQUIRE(e.item(1).dtype() == data_type::f64);
    REQUIRE(e.item(2).dtype() == data_type::str);
    REQUIRE(e.item(3).dtype() == data_type::symbol);
    REQUIRE(e.item(4).dtype() == data_type::table);
    REQUIRE(e.symbols().size() == 2);
    REQUIRE(e == e);
}




TEST_CASE("expression can be converted to string", "[expression]")
{
    REQUIRE(expression().unparse() == "()");
    REQUIRE(expression({}).unparse() == "()");
    REQUIRE(expression({1, 2, 3}).unparse() == "(1 2 3)");
    REQUIRE(expression({1, 2, 3}).unparse() == "(1 2 3)");
}




TEST_CASE("expression::with* methods correctly", "[expression]")
{
    SECTION("with_part and with_attr work correctly")
    {
        expression e = {1, 2, 3, 4, expression(10).keyed("ten")};
        REQUIRE(e.with_part(0, 5).part(0).get_i32() == 5);
        REQUIRE(e.with_attr("ten", "9+1").attr("ten").get_str() == "9+1");
        REQUIRE(e.with_attr("nine", "9") == e);
    }
    SECTION("with on a flat expression works correctly")
    {
        expression e = {10, 20};
        expression f = {expression(10).keyed("ten"), expression(20).keyed("twenty")};
        expression g = {
            expression(0).keyed("A"),
            expression(1).keyed("B"),
            expression(2).keyed("C"),
            expression(3).keyed("B") };
        REQUIRE(e.with({0}, 50) == expression {50, 20});
        REQUIRE(e.with({1}, 50) == expression {10, 50});
        REQUIRE(f.with({"ten"}, "9+1").attr("ten").get_str() == "9+1");
        REQUIRE(f.with({"twenty"}, "18+2").attr("twenty").get_str() == "18+2");
        REQUIRE(g.without_attr("A").part(0).get_i32() == 1);
        REQUIRE(g.without_attr("A").part(1).get_i32() == 2);
        REQUIRE(g.without_attr("A").part(2).get_i32() == 3);
        REQUIRE(g.without_attr("B").part(0).get_i32() == 0);
        REQUIRE(g.without_attr("B").part(1).get_i32() == 2);
        REQUIRE(e.without_part(0).part(0).get_i32() == 20);
        REQUIRE(e.without_part(1).part(0).get_i32() == 10);
    }
    SECTION("with on a nested expression works correctly")
    {
        expression e = {{10, 20}, {30, 40}};
        REQUIRE(e.with({0, 0}, 50) == expression {{50, 20}, {30, 40}});
        REQUIRE(e.with({1, 1}, 50) == expression {{10, 20}, {30, 50}});
        REQUIRE(e.with({2, 2}, 50) == e);
        REQUIRE(e.address({0, 0}).get_i32() == 10);
        REQUIRE(e.address({1, 1}).get_i32() == 40);
        REQUIRE(e.without({1, 1}).size() == 2);
        REQUIRE(e.without({1, 1}).part(1).size() == 1);
        REQUIRE(e.without({1, 1}).part(1).part(0).get_i32() == 30);
    }
}




TEST_CASE("expression::relabel works correctly", "[expression]")
{
    expression e = {symbol("a"), symbol("b"), symbol("c"), symbol("a")};
    REQUIRE(e.relabel("a", "A").size() == e.size());
    REQUIRE(e.relabel("a", "A").part(0).get_sym() == "A");
    REQUIRE(e.relabel("a", "A").part(3).get_sym() == "A");
    REQUIRE(e.relabel("b", "B").part(1).get_sym() == "B");
    REQUIRE(e.relabel("c", "C").part(2).get_sym() == "C");
}




TEST_CASE("expression::resolve works correctly with the basic call adapter", "[expression]")
{
    SECTION("for simple symbol resolution")
    {
        auto e = expression{symbol("a"), symbol("b"), symbol("c"), symbol("a")};
        auto f = expression{"A", "B", symbol("c"), "A"};
        auto a = call_adapter();
        auto s = immer::map<std::string, expression>()
        .set("a", "A")
        .set("b", "B");
        REQUIRE(e.resolve(s, a) == f);
    }
    SECTION("for function evaluations")
    {
        auto e = expression{expression([] (auto e) { return int(e.first()) + int(e.second()); }),
                            symbol("a"),
                            symbol("b")};
        auto f = expression{"A", "B", symbol("c"), "A"};
        auto a = call_adapter();
        auto s = immer::map<std::string, expression>()
        .set("a", 1)
        .set("b", 2);
        REQUIRE(int(e.resolve(s, a)) == 3);
    }
}




TEST_CASE("basic strings can be parsed into expressions", "[parser]")
{
    REQUIRE(parser::parse("a").dtype() == data_type::symbol);
    REQUIRE(parser::parse("1").dtype() == data_type::i32);
    REQUIRE(parser::parse("1.0").dtype() == data_type::f64);
    REQUIRE(parser::parse("(a b c)").dtype() == data_type::table);
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
    // REQUIRE_THROWS(parser::parse("-"));
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
    REQUIRE(parser::parse("deer=(0 1 2 3)").dtype() == data_type::table);
    REQUIRE(parser::parse("deer=(0 1 2 3)").key() == "deer");
    REQUIRE(parser::parse("deer=(0 1 2 3)").size() == 4);
    REQUIRE(parser::parse("deer=(0 1 2 3)").at(0).get_i32() == 0);
    REQUIRE(parser::parse("deer=(0 1 2 3)").at(1).get_i32() == 1);
    REQUIRE(parser::parse("deer=(0 1 2 3)").at(2).get_i32() == 2);
    REQUIRE(parser::parse("deer=(0 1 2 3)").at(3).get_i32() == 3);
}




TEST_CASE("more complex expressions parse correctly", "[parser]")
{
    REQUIRE(parser::parse("(0 1 2 3 (0 1 2 3))").unparse() == "(0 1 2 3 (0 1 2 3))");
    REQUIRE(parser::parse("(a 1 2 3 (b 1 2 3 (c 1 2 3)))").unparse() == "(a 1 2 3 (b 1 2 3 (c 1 2 3)))");
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



#endif // TEST_EXPRESSION
