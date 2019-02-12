#include <iostream>
#include "crt-expr.hpp"
#include "crt-core.hpp"




//=============================================================================
int main()
{
    crt::kernel kern;
    crt::core::import(kern);

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
