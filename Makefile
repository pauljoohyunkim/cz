INCLUDE=include
CC=clang
CXX=clang++
CFLAGS=-g -Wall -O0 -I$(INCLUDE) -MMD -MP $(shell llvm-config --cflags) $(CFLAGS_EXTRA)
CXXFLAGS=$(CFLAGS)

SRC=src
OBJ=obj
BIN=bin
TESTS=tests
SRCS=$(wildcard $(SRC)/cz_*.c)
OBJS=$(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))
DEPS=$(SRCS:.c=.d) $(SRC)/czc.d
UNITTEST_SRCS=$(wildcard $(TESTS)/*.cpp)
UNITTEST_OBJS=$(patsubst $(TESTS)/%.cpp,$(OBJ)/unittest_%.o,$(UNITTEST_SRCS))
UNITTEST_LDLIBS=$(shell pkg-config --libs gtest)
UNITTEST_DEPS=$(UNITTEST_SRCS:.cpp=.d)
LDLIBS=$(shell llvm-config --libs)

czc: $(BIN)/czc
unittest: $(BIN)/unittest

$(BIN)/czc: $(OBJS) $(OBJ)/czc.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(BIN)/unittest: $(OBJS) $(UNITTEST_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS) $(UNITTEST_LDLIBS)

$(OBJ)/unittest_%.o: $(TESTS)/%.cpp
	$(CXX) $(CXXFLAGS) -MF $(TESTS)/$*.d -c $< -o $@

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -MF $(SRC)/$*.d -c $< -o $@

-include $(DEPS) $(UNITTEST_DEPS)

clean:
	$(RM) -f $(OBJ)/* $(BIN)/* $(DEPS) $(UNITTEST_DEPS)

.PHONY: unittest clean
