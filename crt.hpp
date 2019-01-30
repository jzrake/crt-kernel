#pragma once
#include "kernel.hpp"




//=============================================================================
namespace crt
{



    //=========================================================================
    int check_i32(const crt::expression& e, std::size_t index);
    double check_f64(const crt::expression& e, std::size_t index);
    std::string check_str(const crt::expression& e, std::size_t index);
    std::vector<crt::expression> check_table(const crt::expression& e, std::size_t index);
    std::vector<crt::expression> check_list(const crt::expression& e, std::size_t index);
    std::unordered_map<std::string, crt::expression> check_dict(const crt::expression& e, std::size_t index);

    template<typename T>
    T& check_data(const crt::expression& e, std::size_t index)
    {
        auto arg = e.item(index);

        if (auto capsule = dynamic_cast<crt::capsule<T>*>(arg.get_data().get()))
        {
            return capsule->get();
        }
        throw std::runtime_error(what_wrong_type(crt::type_info<T>::type_name(), arg, index));
    }
}

