#include <cassert>
#include <iostream>
#include "kernel.hpp"
#include "crt.hpp"




//=============================================================================
int main()
{
    crt::kernel kern;
    std::string line;


    while (std::cin >> line)
    {

        std::cout << line << std::endl;
    }

    return 0;
}
