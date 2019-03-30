#include <queue>
#include <rxcpp/rx.hpp>
#include "crt-expr.hpp"
#include "crt-context.hpp"
#include "crt-algorithm.hpp"




//=============================================================================
int resolve_source_sync(std::string source)
{
    auto rules = crt::context::parse(source);
    auto prods = crt::resolve_full(rules);

    std::printf("%s\n", prods.expr().unparse().c_str());

    return 0;
}




//=============================================================================
int main(int argc, const char* argv[])
{
    // for (int n = 0; n < (argc > 1 ? std::atoi(argv[1]) : 1); ++n)
    // {
    //     resolve_source_sync("(a=b b=c c=d d=e e=f f=g g=h h=i i=j j=1)");
    //     resolve_source_sync("(a=(b c) b=(d e) c=(f g) d=(h i) e=(j k) f=(l m) g=(n o) h=1 i=2 j=3 k=4 l=5 m=6 n=7 o=8)");
    // }


    /**
     * This observable is created with a context, and the (possibly
     * incomplete) product map as a capture. It then begins resolving the
     * product map, and emitting it (perhaps once per generation). On each
     * generation it checks to see if it's subscribed, and if not it returns
     * (no need to complete in that case). Once the context is fully resolved,
     * and is still subscribed, the observable completes.
     */
    auto rules = crt::context::parse("(a=b b=c c=d d=e e=f f=g g=h h=i i=j j=1)");
    auto prods = crt::context();

    auto resolver = [rules, p=prods] (auto s)
    {
        auto prods = p;

        while (true)
        {
            auto new_prods = crt::resolve_once(rules, prods);

            if (new_prods.size() == prods.size())
            {
                break;
            }
            s.on_next(prods = std::move(new_prods));
        }
        s.on_completed();
    };

    auto products = rxcpp::observable<>::create<crt::context>(resolver);
    products.subscribe([] (auto p) { std::printf("%s\n", p.expr().unparse().data()); });

    return 0;
}
