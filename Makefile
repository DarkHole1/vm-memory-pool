CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

SRC_DIR  := src
DIST_DIR := dist

SRCS     := $(wildcard $(SRC_DIR)/*.cpp)
TARGETS  := $(patsubst $(SRC_DIR)/%.cpp,$(DIST_DIR)/%,$(SRCS))

.PHONY: all clean

all: $(TARGETS)

$(DIST_DIR)/%: $(SRC_DIR)/%.cpp
	@mkdir -p $(DIST_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf $(DIST_DIR)