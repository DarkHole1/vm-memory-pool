CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2

SRC_DIR  := src
LIB_DIR  := $(SRC_DIR)/lib
DIST_DIR := dist

SRCS        := $(wildcard $(SRC_DIR)/*.cpp)
TARGETS     := $(patsubst $(SRC_DIR)/%.cpp,$(DIST_DIR)/%,$(SRCS))
COMMONS_SRC := $(LIB_DIR)/commons.cpp
COMMONS_OBJ := $(DIST_DIR)/lib/commons.o

.PHONY: all clean

all: $(TARGETS)

$(COMMONS_OBJ): $(COMMONS_SRC)
	@mkdir -p $(DIST_DIR)/lib
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(DIST_DIR)/%: $(SRC_DIR)/%.cpp $(COMMONS_OBJ)
	@mkdir -p $(DIST_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(DIST_DIR)