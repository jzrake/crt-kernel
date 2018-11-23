CXXFLAGS = -std=c++14


default: test main

main.o: kernel.hpp

test.o: kernel.hpp

test: test.o catch.o
	$(CXX) -o $@ $(CXXFLAGS) $^

main: main.o
	$(CXX) -o $@ $(CXXFLAGS) $^

clean:
	$(RM) *.o test main
