CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
AR       = ar

SRC = src/wire.cpp src/gate.cpp src/gates.cpp src/circuit.cpp
OBJ = $(SRC:.cpp=.o)
LIB = libpgates.a

EXAMPLES = examples/basic examples/repl
TESTS    = tests/truth_tables tests/bus_ops

.PHONY: all clean test

all: $(LIB) $(EXAMPLES) $(TESTS)

test: $(TESTS)
	@for t in $(TESTS); do echo "=== $$t ==="; ./$$t; done

$(LIB): $(OBJ)
	$(AR) rcs $@ $^

examples/%: examples/%.cpp $(LIB)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIB)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

tests/%: tests/%.cpp $(LIB)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIB)

clean:
	rm -f $(OBJ) $(LIB) $(EXAMPLES) $(TESTS)
