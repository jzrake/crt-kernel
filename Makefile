CXXFLAGS = -std=c++14 -Wall -I../immer

default: test main crt-workers

main.o    : crt-expr.hpp
test.o    : crt-expr.hpp
crt-core.o: crt-expr.hpp
crt-workers.o: crt-workers.cpp

test: test.o catch.o
	$(CXX) -o $@ $(CXXFLAGS) $^

main: main.o
	$(CXX) -o $@ $(CXXFLAGS) $^ -lcurses

crt-workers: crt-workers.o
	$(CXX) -o $@ $(CXXFLAGS) $^ -lcurses

clean:
	$(RM) *.o test main crt-workers
