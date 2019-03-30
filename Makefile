CXXFLAGS = -std=c++14 -Wall -I../immer -I../RxCpp/Rx/v2/src
HEADERS = \
	crt-algorithm.hpp \
    crt-context.hpp \
    crt-expr.hpp \
    crt-workers.hpp \

default        : test main async-resolve
main.o         : $(HEADERS)
test.o         : $(HEADERS)
async-resolve.o: $(HEADERS)

test: test.o catch.o
	$(CXX) -o $@ $(CXXFLAGS) $^

main: main.o
	$(CXX) -o $@ $(CXXFLAGS) $< -lcurses

async-resolve: async-resolve.o
	$(CXX) -o $@ $(CXXFLAGS) $<

clean:
	$(RM) *.o test main async-resolve
