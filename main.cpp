#include <cassert>
#include <iostream>
#include "kernel.hpp"



crt::expression item (const crt::expression& e)
{
	return {};
}

crt::expression take (const crt::expression& e)
{
	return {1, 2, 3};
}


// ============================================================================
int main()
{
	crt::expression e (item);
	crt::expression f (take);


	std::cout << int(e.get_func().target<crt::func_t>() == e.get_func().target<crt::func_t>()) << std::endl;
	std::cout << ptrdiff_t(e.get_func().target<crt::func_t>()) << std::endl;

	std::cout << e.get_func().target_type().name() << std::endl;
	std::cout << f.get_func().target_type().name() << std::endl;

    return 0;
}
