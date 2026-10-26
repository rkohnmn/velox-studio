CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O3 -Wall -Wextra -Werror -Wpedantic -Wno-restrict -pthread -Iinclude
ifeq ($(OS),Windows_NT)
    LDFLAGS ?= -lgdi32 -luser32
else
    LDFLAGS ?= -pthread -ldl
endif

CORE_SRCS := src/arena.cpp \
             src/sha256.cpp \
             src/csv_engine.cpp \
             src/viz_engine.cpp \
             src/schema_engine.cpp \
             src/profiler_engine.cpp \
             src/provenance_engine.cpp \
             src/studio_ui.cpp \
             src/native_gui.cpp \
             src/ux_research_engine.cpp

.PHONY: all test bench clean validate

all: velox velox_tests

velox: $(CORE_SRCS) src/main.cpp
	$(CXX) $(CXXFLAGS) $(CORE_SRCS) src/main.cpp -o $@ $(LDFLAGS)

velox_tests: $(CORE_SRCS) tests/test_main.cpp
	$(CXX) $(CXXFLAGS) $(CORE_SRCS) tests/test_main.cpp -o $@ $(LDFLAGS)

test: velox_tests
	./velox_tests

bench: velox
	./velox bench --rows 250000

validate:
	python3 scripts/validate_project.py .

clean:
	rm -f velox velox_tests
