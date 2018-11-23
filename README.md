# `crt::kernel`

## A Lisp-inspired "concurrent runtime" in C++11

This library provides a few classes that work together to manage complex dependencies between data structures, as arise in e.g. task-based parallism, or in UI applications. The approach is analogous to the way a Makefile arranges rules and build products in an acyclic graph structure.

Rules are key-expression pairs, where the __key__ is a string, and the __expression__ is a data structure that converts losslessly to and from Lisp-style source strings.
