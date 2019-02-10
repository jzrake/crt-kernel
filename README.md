# `crt::kernel`

## A Lisp-inspired "concurrent runtime" in C++11

This library provides a few classes that work together to manage complex dependencies between data structures, as arise in e.g. task-based parallism, or in UI applications. The approach is analogous to the way a Makefile arranges rules and build products in an acyclic graph structure.

Rules are key-expression pairs, where the __key__ is a string, and the __expression__ is a data structure that converts losslessly to and from Lisp-style source strings.

An `expression` is a sum-type having one of the following types:
- `none`
- `i32`
- `f64`
- `str`
- `symbol`
- `data`
- `function`
- `table`

There is an intentional similarity to Lua in the above scheme, and indeed tables in `crt` function mostly the same as Lua tables. Also, `data` is like a Lua user data. Functions are mappings from one expression to another, and may either be C++ lambdas, or be defined in the language using the following syntax:
```
my-func: (do-something-with @1 @2 @kwarg1 @kwarg2)
```
This function is now callable as in `(my-func arg1 arg2 kwarg1='a' kwarg2='b')`.

The real power of the `crt` language derives from the `symbol` primitive type. Expressions containing symbols can be resolved by a `scope` to another expression. This may be done incrementally to resolve an expression through a hierarchy of scopes to obtain a concrete value.

A set of expressions may be assigned to distinct keys, and have symbols referencing one another. This is a declarative program like a `Makefile`, and just as in make, the order of the declarations does not matter. `crt` provides a class, the `crt::kernel`, which maintains an acyclic graph structure representing the data flow among a set of expressions. The kernel also provides facilities for propagating updates through the graph incrementally as new data is entered into it. These updates can be done on separate compute threads, making `crt` viable as a data concurrency language for parallel processing of e.g. software graphics pipelines or physics simulations.
