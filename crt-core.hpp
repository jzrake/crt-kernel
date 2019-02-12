#include "crt-expr.hpp"




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
        expression func      (const expression& e);
        expression unparse   (const expression& e);

        void import(crt::kernel& k);
    }
}
