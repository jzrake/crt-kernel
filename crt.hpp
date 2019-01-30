#pragma once
#include "kernel.hpp"




//=============================================================================
namespace crt
{



    //=========================================================================

    /**
     * Check that the expression is actually a table, and that its value at
     * the specified index has type i32. Throws a runtime_error if these
     * conditions are not met. Note that the index is with respect to the list
     * part of the table, e.g. it ignores keyed parts.
     */
    int check_i32(const crt::expression& e, std::size_t index);


    /**
     * Check that the expression is actually a table, and that its value at
     * the specified index has type f64. Throws a runtime_error if these
     * conditions are not met. Note that the index is with respect to the list
     * part of the table, e.g. it ignores keyed parts.
     */
    double check_f64(const crt::expression& e, std::size_t index);


    /**
     * Check that the expression is actually a table, and that its value at
     * the specified index has type str. Throws a runtime_error if these
     * conditions are not met. Note that the index is with respect to the list
     * part of the table, e.g. it ignores keyed parts.
     */
    std::string check_str(const crt::expression& e, std::size_t index);


    /**
     * Check that the expression at the specified index within e is a table,
     * and return all of its parts if so. Otherwise throws a runtime_error.
     */
    std::vector<crt::expression> check_table(const crt::expression& e, std::size_t index);


    /**
     * Return e.list() if the expression at the specified index within e is a
     * table without any keyed entries. Otherwise throws a runtime_error.
     */
    std::vector<crt::expression> check_list(const crt::expression& e, std::size_t index);


    /**
     * Return e.dict() if the expression at the specified index within e is a
     * table with only keyed entries. Otherwise throws a runtime_error.
     */
    std::unordered_map<std::string, crt::expression> check_dict(const crt::expression& e, std::size_t index);


    /**
     * Return a const reference to the capsule value, if this expression has
     * type user_data, and the user_data instance casts successfully to a
     * capsule of the specified template parameter. Otherwise returns a
     * runtime_error.
     */
    template<typename T>
    const T& check_data(const crt::expression& e, std::size_t index)
    {
        auto arg = e.item(index);

        if (auto capsule = dynamic_cast<crt::capsule<T>*>(arg.get_data().get()))
        {
            return capsule->get();
        }
        throw std::runtime_error(what_wrong_type(crt::type_info<T>::type_name(), arg, index));
    }
}

