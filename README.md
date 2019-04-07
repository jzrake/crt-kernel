# `crt::kernel`
_A lisp-inspired "concurrent runtime" in C++11_

This library provides a few classes that work together to manage data flow in a [dependency graph](https://en.wikipedia.org/wiki/Dependency_graph). Originally, the need arose from an application for scientific data visualization - where bits of data to be displayed are defined in terms of one another, the way formulas in a spreadsheet can reference the values of other cells, which can themselves be formulas. This pattern also arises in physics simulations, where each stage of an update scheme depends on the completion of preceding stages.

The code uses a purely [functional](https://en.wikipedia.org/wiki/Functional_programming) programming style, where the data structures are immutable. This approach trivializes concurrent execution, but would naively introduce lots of overhead due to the high volume of data copies. This overhead is removed by the use of [persistent data structures](https://en.wikipedia.org/wiki/Persistent_data_structure) from the excellent [immer](https://github.com/arximboldi/immer) library.

I have written a few dependency graph resolution schemes (others can easily be added), which use [incremental computing](https://en.wikipedia.org/wiki/Incremental_computing) to eliminate redundant computations when only subsets of the graph have been invalidated. These schemes can be executed asynchronously - generating an observable stream of increasingly resolved "build products". You can get this behavior by (optionally) including [Kirk Shoop's](https://github.com/ReactiveX/RxCpp/commits?author=kirkshoop) excellent [Reactive Extensions](http://reactivex.io) implementation for C++, [`RxCpp`](https://github.com/ReactiveX/RxCpp).


## Data structures
`crt` is based around three basic facilities:
- `expression`: the basic object model
- `context`: a mapping of `(string -> expression)` that maintains a dependency graph
- `parser`: a very basic lisp-style parser to define user expressions at runtime


### `crt::expression`
An `expression` is a [sum-type](https://en.wikipedia.org/wiki/Tagged_union):
- `none`
- `i32`
- `f64`
- `str`
- `symbol`
- `data`
- `function`
- `table`

There is an intentional similarity to Lua in the above scheme, and indeed tables in `crt` function mostly the same as Lua tables, and `data` is like a Lua user data. Functions are mappings from one expression to another and are defined as `std::function<expression(expression)>`.

What separates the `crt::expression` from the JS and Lua object models is the `symbol` primitive type. It allows expressions to serve as templates for other expressions. The `expression::resolve` method takes a scope object (`string -> expression`), and returns a "product" expression, whose parts are resolved recusively. Expressions can be of higher order, by resolving into other expressions which themselves contain symbols.


### `crt::context`
The context is a mapping (`string -> expression`), which maintains ingoing and outgoing edges representing dependencies between different expressions. It ensures the graph remains acyclic, and effectively provides [topological sort](https://en.wikipedia.org/wiki/Topological_sorting) operations to optimize the resolution of a context to its build products. `crt::context` is immutable, returing cheap copies of itself on insert/erase operations (it's based on the `immer::map` and `immer::set` containers). Algorithms for context resolution are provided in `crt-algorithm.hpp`. 
