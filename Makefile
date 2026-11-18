CXX      = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -Iinclude
SRC      = $(wildcard src/*.cpp)
OBJ      = $(SRC:src/%.cpp=build/%.o)
HDR      = $(wildcard include/*.h)

all: minios

minios: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

build/%.o: src/%.cpp $(HDR)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: minios
	./minios

# run every script in demos/ and save the transcripts in results/
demos: minios
	@mkdir -p results
	@for d in demos/*.txt; do \
		n=$$(basename $$d .txt); \
		./minios < $$d > results/$$n.out; \
		cp kernel.log results/$$n.kernel.log; \
		echo "ran $$d -> results/$$n.out"; \
	done

# compare fresh runs against the saved expected output (regression tests)
test: minios
	@bash tests/run_tests.sh

# build with AddressSanitizer + UBSan and run every script (finds memory bugs)
asan:
	$(CXX) -std=c++11 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -o build/minios_asan src/*.cpp
	@for d in demos/*.txt tests/cases/*.txt; do \
		./build/minios_asan < $$d > build/asan.out 2>&1; \
		if grep -q 'runtime error\|AddressSanitizer' build/asan.out; then echo "FAIL $$d"; exit 1; else echo "clean $$d"; fi; \
	done

clean:
	rm -rf build minios kernel.log

.PHONY: all run demos test asan clean
