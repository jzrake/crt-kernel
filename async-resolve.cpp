#include <rxcpp/rx.hpp>
#include "crt-expr.hpp"
#include "crt-context.hpp"
#include "crt-algorithm.hpp"




//=============================================================================
// int resolve_source_sync(std::string source)
// {
//     auto rules = crt::context::parse(source);
//     auto prods = crt::resolve_full(rules);

//     std::printf("%s\n", prods.expr().unparse().c_str());

//     return 0;
// }




/**
 * Returns a function, that when passed to observable::create, yields an
 * observable of resolutions (products) of the given set of rules. If products
 * is non-empty, then its entries should be up-to-date with the rules. The
 * function calls the subscriber with the products, as they mature, once per
 * resolve cycle. On each generation it checks to see if it's subscribed, and
 * if not it returns (no need to complete in that case). The observable
 * completes when the context is fully resolved.
 */
auto resolution_of(crt::context rules, crt::context prods={})
{
    return [rules, p=prods] (auto s)
    {
        std::cout << "subscribed on " << std::this_thread::get_id() << std::endl;

        auto prods = p;

        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));

            auto new_prods = crt::resolve_once(rules, prods);

            if (new_prods.size() == prods.size())
            {
                break;
            }
            if (! s.is_subscribed())
            {
                std::cout << "unsubscribed!" << std::endl;

                return;
            }
            s.on_next(prods = std::move(new_prods));
        }
        s.on_completed();
    };
}

auto make_source(int i)
{
   return "(a=b b=c c=d d=e e=f f=g g=h h=i i=j j=" + std::to_string(i) + ")";
};




//=============================================================================
int main(int argc, const char* argv[])
{
    using namespace rxcpp;


    std::cout << "main thread: " << std::this_thread::get_id() << std::endl;


    auto sched = schedulers::make_event_loop();
    auto worker = sched.create_worker();
    auto coordination = identity_same_worker(schedulers::make_event_loop().create_worker());


    observable<>::interval(std::chrono::milliseconds(30))
    .take(10)
    .map([] (auto i) { return crt::context::parse(make_source(i)); })
    .map([coordination] (auto r)
    {
        std::cout << "interval thread: " << std::this_thread::get_id() << std::endl;
        return observable<>::create<crt::context>(resolution_of(r)).subscribe_on(coordination);
    })
    .switch_on_next()
    .as_blocking()
    .subscribe([] (auto p) { std::cout << p.expr().unparse() << std::endl; });


    return 0;
}
