CXX      = g++
CXXFLAGS = -Wall -Wextra -Werror -O2 -std=c++17

wish: wish.cpp
	$(CXX) $(CXXFLAGS) -o wish wish.cpp

test: wish
	./test-wish.sh -c

clean:
	rm -rf wish tests-out

.PHONY: test clean
