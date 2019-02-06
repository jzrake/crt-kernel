#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>




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
         * probably be a table, but in principle anything other than another
         * user_data is OK, including none. Returning a user_data here would
         * cause the unparse method to recurse forever.
         */
        virtual expression to_table() const = 0;
    };


    //=========================================================================
    using func_t = std::function<expression(expression)>;
    using data_t = std::shared_ptr<user_data>;


    //=========================================================================
    template<typename T> struct capsule;
    template<typename T> struct type_info;
    template<typename T> static std::shared_ptr<user_data> make_data(const T&);
    template<typename T> static crt::func_t init();
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
     * Return a symbol expression.
     */
    static expression symbol(const std::string& v)
    {
        auto e = expression();
        e.type = data_type::symbol;
        e.valsym = v;
        return e;
    }


    /**
     * Default constructor.
     */
    expression()                                        : type(data_type::none) {}
    expression(const none&)                             : type(data_type::none) {}
    expression(int vali32)                              : type(data_type::i32), vali32(vali32) {}
    expression(float valf64)                            : type(data_type::f64), valf64(valf64) {}
    expression(double valf64)                           : type(data_type::f64), valf64(valf64) {}
    expression(const char* valstr)                      : type(data_type::str), valstr(valstr) {}
    expression(const std::string& valstr)               : type(data_type::str), valstr(valstr) {}
    expression(data_t valdata)                          : type(data_type::data), valdata(valdata) {}
    expression(func_t valfunc)                          : type(data_type::function), valfunc(valfunc) {}
    expression(const std::vector<expression>& parts)    : type(data_type::table), parts(parts)
    {
        if (expression::parts.empty())
        {
            type = data_type::none;
        }
    }
    expression(std::initializer_list<expression> parts) :
    expression(std::vector<expression>(parts)) {}


    /**
     * Construct an expression from a pair of iterators.
     */
    template<typename IteratorType>
    expression(IteratorType first, IteratorType second) :
    expression(std::vector<expression>(first, second))
    {
    }


    /**
     * Return the raw data members themselves.
     */
    const auto& get_i32()  const { return vali32; }
    const auto& get_f64()  const { return valf64; }
    const auto& get_str()  const { return valstr; }
    const auto& get_sym()  const { return valsym; }
    const auto& get_func() const { return valfunc; }
    const auto& get_data() const { return valdata; }
    auto key()                 const { return keyword; }
    auto dtype()               const { return type; }
    auto has_type(data_type t) const { return type == t; }
    auto begin()        const { return parts.begin(); }
    auto end()          const { return parts.end(); }
    auto rbegin()       const { return parts.rbegin(); }
    auto rend()         const { return parts.rend(); }
    expression first()  const { return parts.size() > 0 ? parts[0] : none(); }
    expression second() const { return parts.size() > 1 ? parts[1] : none(); }
    expression third()  const { return parts.size() > 2 ? parts[2] : none(); }
    expression rest()   const { return parts.size() > 1 ? expression(begin() + 1, end()) : none(); }
    expression last()   const { return parts.size() > 0 ? parts.back() : none(); }


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
            return index < valstr.size() ? expression(valstr.substr(index, 1)) : none();
        }
        std::size_t n = 0;

        for (const auto& part : parts)
        {
            if (part.keyword.empty())
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
     * Return the first part whose key matches the one specified, or none if
     * none exists (including if this is not a table). This function removes
     * the keyword part of the resulting expression.
     */
    expression attr(const std::string& key) const
    {
        for (const auto& part : parts)
        {
            if (part.keyword == key)
            {
                return part.keyed(std::string());
            }
        }
        return {};    
    }


    /**
     * Return the unkeyed subset of this expression's parts.
     */
    std::vector<expression> list() const
    {
        std::vector<expression> list_parts;

        for (const auto& part : parts)
        {
            if (part.keyword.empty())
            {
                list_parts.push_back(part);
            }
        }
        return list_parts;
    }


    /**
     * Return the keyed subset of this expression's parts.
     */
    std::vector<expression> dict() const
    {
        std::vector<expression> dict_parts;

        for (const auto& part : parts)
        {
            if (! part.keyword.empty())
            {
                dict_parts.push_back(part);
            }
        }
        return dict_parts;
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
     * Returns this expression with its outermost two layers transposed:
     * 
     *          ((a b c) (1 2 3)) -> ((a 1) (b 2) (b 3)) .
     *
     * If this expression is not a table, then an empty expression is
     * returned. If any of its parts is not a table, that value is broadcast
     * to a list, duplicating its scalar value. The size of the result has the
     * size of the smallest part that is a table.
     */
    expression zip() const
    {
        std::size_t len = 0;

        for (const auto& part : parts)
        {
            if (part.has_type(data_type::table))
            {
                if (len == 0 || len > part.size())
                {
                    len = part.size();
                }
            }
        }

        std::vector<expression> result(len);

        for (std::size_t n = 0; n < len; ++n)
        {
            result[n].type = data_type::table;

            for (const auto& part : parts)
            {
                if (part.has_type(data_type::table))
                {
                    result[n].parts.push_back(part.parts[n].keyed(part.keyword));
                }
                else
                {
                    result[n].parts.push_back(part);
                }
            }
        }
        return result;
    }


    expression inc() const
    {
        switch (type)
        {
            case data_type::none: return +1;
            case data_type::i32: return vali32 + 1;
            case data_type::f64: return valf64 + 1.0;
            default: return *this;
        }
    }


    expression dec() const
    {
        switch (type)
        {
            case data_type::none: return -1;
            case data_type::i32: return vali32 - 1;
            case data_type::f64: return valf64 - 1.0;
            default: return *this;
        }
    }


    expression toggle() const
    {
        switch (type)
        {
            case data_type::none: return 1;
            default: return {};
        }
    }


    /**
     * Return an expression built from the parts of this one and the parts of
     * the one specified.
     */
    expression concat(const expression& more) const
    {
        auto result = parts;
        result.insert(result.end(), more.begin(), more.end());
        return result;
    }


    /**
     * Comparison operator: works like a dictionary for strings and tables.
     * Different types compare the integer equivalent of their type enums.
     */
    bool operator<(const expression& other) const
    {
        if (type == other.type)
        {
            switch (type)
            {
                case data_type::none     : return false;
                case data_type::i32      : return vali32 < other.vali32;
                case data_type::f64      : return valf64 < other.valf64;
                case data_type::str      : return valstr < other.valstr;
                case data_type::symbol   : return valsym < other.valsym;
                case data_type::data     : return valdata < other.valdata;
                case data_type::function : return false;
                case data_type::table    :
                {
                    for (int n = 0; n < std::min(size(), other.size()); ++n)
                    {
                        if (parts[n] != other.parts[n])
                        {
                            return parts[n] < other.parts[n];
                        }
                    }
                    return size() < other.size() || keyword < other.keyword;
                }
            }
        }
        return type < other.type;
    }


    /**
     * Return a sorted version of this expression.
     */
    expression sort() const
    {
        auto result = parts;
        std::sort(result.begin(), result.end());
        return result;
    }


    /**
     * Return a set of all symbols referenced at any level in this expression.
     */
    std::unordered_set<std::string> symbols() const
    {
        switch (type)
        {
            case data_type::symbol:
            {
                return {valsym};
            }
            case data_type::table:
            {
                std::unordered_set<std::string> result;

                for (const auto& part : parts)
                {
                    for (const auto& s : part.symbols())
                    {
                        result.insert(s);
                    }
                }
                return result;
            }
            default: return {};
        }
    }


    /**
     * Return a copy of this expression with a different key.
     */
    expression keyed(const std::string& kw) const
    {
        auto e = *this;
        e.keyword = kw;
        return e;
    }


    /**
     * Convenience method to return a default value for this expression, if it
     * evaluates empty.
     */
    expression otherwise (const expression& e) const
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
           case data_type::str      : return ! valstr.empty();
           case data_type::symbol   : return ! valsym.empty();
           case data_type::data     : return valdata != nullptr;
           case data_type::function : return valfunc != nullptr;
           case data_type::table    : return ! parts.empty();
       }
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
    }


    operator bool()        const { return as_boolean(); }
    operator int()         const { return as_i32(); }
    operator float()       const { return as_f64(); }
    operator double()      const { return as_f64(); }
    operator std::string() const { return as_str(); }


    /**
     * Try to return a string interpretation of this expression that can be
     * restored by the parser. The only cases where this can fail are if the
     * expression is a function, or if it is a user_data that has not
     * implemented the unparse method.
     */
    std::string unparse() const
    {
        auto pre = keyword.empty() ? "" : keyword + "=";

        switch (type)
        {
            case data_type::none     : return pre + "()";
            case data_type::i32      : return pre + std::to_string(vali32);
            case data_type::f64      : return pre + std::to_string(valf64);
            case data_type::str      : return pre + "'" + valstr + "'";
            case data_type::symbol   : return pre + valsym;
            case data_type::data     : return pre + valdata->to_table().unparse();
            case data_type::function : return pre + "<func>";
            case data_type::table:
            {
                std::string res;

                for (const auto& part : parts)
                {
                    res += " " + part.unparse();
                }
                return pre + "(" + res.substr(1) + ")";
            }
        }
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
    };


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
                    // throw std::runtime_error("unresolved symbol: " + valsym);
                }
            }
            default: return *this;
        }
    }


    /**
     * Returns this expression with all of its symbols having the name `from`
     * renamed to `to`.
     */
    expression relabel(const std::string& from, const std::string& to) const
    {
        switch (type)
        {
            case data_type::symbol:
            {
                return {expression::symbol(valsym == from ? to : valsym).keyed(keyword)};
            }
            case data_type::table:
            {
                std::vector<expression> result;

                for (const auto& part : parts)
                {
                    result.push_back(part.relabel(from, to));
                }
                return result;
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
            case data_type::symbol   : return valsym == symbol ? e.keyed(keyword) : *this;
            case data_type::table:
            {
                std::vector<expression> result;

                for (const auto& part : parts)
                {
                    result.push_back(part.replace(symbol, e));
                }
                return result;
            }
            default: return *this;
        }
    }


    /**
     * Test for equality between two expressions. Equality means exact
     * equivalence of type and value. Functions are always unequal, even to
     * themselves.
     */
    bool operator==(const expression& other) const
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
        keyword == other.keyword &&
        parts   == other.parts;
    }


    /**
     * Test for inequality.
     */
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
    crt::data_t             valdata;
    crt::func_t             valfunc;
    std::vector<expression> parts;
    std::string             keyword;
    friend class parser;
};




//=========================================================================
template<typename T>
static std::shared_ptr<crt::user_data> crt::make_data(const T& v)
{
    return std::dynamic_pointer_cast<user_data>(std::make_shared<capsule<T>>(v));
}

template<typename T>
crt::func_t crt::init()
{
    return [] (const crt::expression& e) -> crt::expression
    {
        return crt::make_data(e.to<T>());
    };
}

template<typename T>
struct crt::capsule : public crt::user_data
{
    capsule() {}
    capsule(const T& value) : value(value) {}
    const char* type_name() const override { return type_info<T>::name(); }
    expression to_table() const override { return type_info<T>::to_table(value); }
    T value;
};




//=============================================================================
class crt::kernel
{
public:


    //=========================================================================
    struct rule_t;
    using map_t = std::unordered_map<std::string, rule_t>;
    using set_t = std::unordered_set<std::string>;


    //=========================================================================
    struct rule_t
    {
        expression expr;
        expression value;
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

    set_t insert(const expression& expr)
    {
        return insert (expr.key(), expr, 0);
    }

    set_t define(const std::string& key, func_t func)
    {
        return insert_literal(key, func);
    }


    /**
     * Add a literal rule to the graph. The expression for this rule is empty,
     * and its value is always the one given.
     */
    set_t insert_literal(const std::string& key, const expression& value, long flags=0)
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
     * function throws if the key does not exist.
     */
    const expression& at(const std::string& key) const
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
     * Return the value at the given key if it exists, and none otherwise.
     */
    const expression& attr(const std::string& key) const
    {
        static expression empty = expression::none();

        if (contains(key))
        {
            return at(key);
        }
        return empty;
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
    template<typename CallAdapter>
    expression resolve(const std::string& key, std::string& error, const CallAdapter& adapter) const
    {
        try {
            error.clear();
            const auto rule = rules.at(key);

            if (rule.expr.empty())
            {
                return rule.value;
            }
            return rule.expr.resolve(*this, adapter);
        }
        catch (const std::out_of_range& e)
        {
            error = "unresolved symbol: " + key;
        }
        catch (const std::exception& e)
        {
            error = e.what();
        }
        return expression();
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
    void update_directly(const std::string& key, const expression& value, const std::string& error)
    {
        auto& rule = rules.at(key);
        rule.value = value;
        rule.error = error;
        rule.dirty = false;
    }

    template<typename CallAdapter>
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

    template<typename CallAdapter>
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

    template<typename CallAdapter>
    set_t update_all(const set_t& keys, const CallAdapter& adapter)
    {
        for (const auto& key : keys)
        {
            update_recurse(key, adapter);
        }
        return keys;
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
            rule.second.expr = rule.second.expr.relabel(from, to);
        }
    }


private:
    //=========================================================================
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


#include <iostream>

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

        std::vector<expression> parts;
        auto c = source;

        while (*c != '\0')
        {
            parts.push_back(parse_part(c));
        }
        return parts.size() == 1 ? parts.front() : parts;
    }


private:


    //=========================================================================
    static bool is_symbol_character(char e)
    {
        return std::isalnum(e) || e == '_' || e == '-' || e == ':' || e == '@';
    }

    static bool is_leading_symbol_character(char e)
    {
        return std::isalpha(e) || e == '_' || e == '-' || e == ':' || e == '@';
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

        if (! (std::isspace(*c) || *c == '\0' || *c == ')'))
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
            else if (std::isspace(*c) || *c == ')')
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
            e.type = data_type::table;
        }
        return e;
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
class crt::call_adapter
{
public:
    template<typename Mapping>
    crt::expression call(const Mapping& scope, const crt::expression& expr) const
    {
        auto head = expr.first().resolve(scope, *this);
        auto args = std::vector<expression>();

        for (const auto& part : expr.rest())
        {
            args.push_back(part.resolve(scope, *this));
        }

        if (head.has_type(crt::data_type::function))
        {
            return head.call(args);
        }
        return head.nest().concat(args);
    }
};




//=============================================================================
#ifdef TEST_KERNEL
#include "catch.hpp"
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
        expression::symbol("a"),
        {1, expression::symbol("b"), expression::symbol("b")}};

    REQUIRE(e.dtype() == data_type::table);
    REQUIRE(e.size() == 5);
    REQUIRE(e.list()[0].dtype() == data_type::i32);
    REQUIRE(e.list()[1].dtype() == data_type::f64);
    REQUIRE(e.list()[2].dtype() == data_type::str);
    REQUIRE(e.list()[3].dtype() == data_type::symbol);
    REQUIRE(e.list()[4].dtype() == data_type::table);
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




TEST_CASE("kernel can be constructed", "[kernel]")
{
    kernel k;

    k.insert("a", std::string("thing"));
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
        k.insert_literal("c", expression(12));

        SECTION("can be updated incrementally")
        {
            REQUIRE(k.dirty("a"));
            REQUIRE(k.dirty("b"));
            REQUIRE_FALSE(k.dirty("c"));
            REQUIRE_FALSE(k.update("a", crt::call_adapter()));
            REQUIRE(k.update("b", crt::call_adapter()));
            REQUIRE(k.update("a", crt::call_adapter()));
            REQUIRE_FALSE(k.dirty("a"));
            REQUIRE_FALSE(k.dirty("b"));
        }

        SECTION("can be updated all at once")
        {
            REQUIRE(k.dirty_rules() == kernel::set_t{"a", "b"});
            k.update_all(k.dirty_rules(), crt::call_adapter());
            REQUIRE(k.dirty_rules() == kernel::set_t{});
        }
    }
}
#endif
