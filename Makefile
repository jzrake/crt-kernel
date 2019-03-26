CXXFLAGS = -std=c++14 -Wall -I../immer

default: test main

main.o    : crt-expr.hpp crt-core.hpp
test.o    : crt-expr2.hpp crt-core.hpp
crt-core.o: crt-expr.hpp crt-core.hpp

test: test.o catch.o
	$(CXX) -o $@ $(CXXFLAGS) $^

main: main.o crt-core.o
	$(CXX) -o $@ $(CXXFLAGS) $^

clean:
	$(RM) *.o test main
