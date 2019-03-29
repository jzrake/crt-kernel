CXXFLAGS = -std=c++14 -Wall -I../immer

default   : test main async-resolve
main.o    : crt-expr.hpp crt-workers.hpp crt-context.hpp 
test.o    : crt-expr.hpp crt-workers.hpp crt-context.hpp
async-resolve.o: crt-expr.hpp crt-workers.hpp crt-context.hpp
crt-core.o: crt-expr.hpp

test: test.o catch.o
	$(CXX) -o $@ $(CXXFLAGS) $^

main: main.o
	$(CXX) -o $@ $(CXXFLAGS) $< -lcurses

async-resolve: async-resolve.o
	$(CXX) -o $@ $(CXXFLAGS) $< -lcurses

clean:
	$(RM) *.o test main async-resolve
