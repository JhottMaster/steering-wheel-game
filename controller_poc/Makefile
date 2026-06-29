CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2
CPPFLAGS ?=

SRC = src/main.cpp
TEST_SRC = src/game_logic_tests.cpp
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

ifeq ($(OS),Windows_NT)
SHELL := cmd
.SHELLFLAGS := /C
EXE_EXT = .exe
OUTPUT = $(BIN_DIR)/steering_wheel_poc$(EXE_EXT)
TEST_OUTPUT = $(BIN_DIR)/game_logic_tests$(EXE_EXT)
RAYLIB_DIR ?= ..\raylib
RAYLIB_LIB = $(RAYLIB_DIR)\src\libraylib.a
INCLUDES = -I"$(RAYLIB_DIR)\src"
LIBS = "$(RAYLIB_LIB)" -lopengl32 -lgdi32 -lwinmm -lws2_32 -static-libgcc -static-libstdc++

define ENSURE_DIR
	@if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
endef

define REMOVE_DIR
	@if exist "$(subst /,\,$1)" rmdir /s /q "$(subst /,\,$1)"
endef

define RUN_OUTPUT
	@if exist "$(subst /,\,$(OUTPUT))" "$(subst /,\,$(OUTPUT))"
endef

else
UNAME_S := $(shell uname -s)
OUTPUT = $(BIN_DIR)/steering_wheel_poc
TEST_OUTPUT = $(BIN_DIR)/game_logic_tests
SYSTEM_RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
SYSTEM_RAYLIB_LIBS := $(shell pkg-config --libs raylib 2>/dev/null)
INCLUDES = $(SYSTEM_RAYLIB_CFLAGS)
LIBS = $(SYSTEM_RAYLIB_LIBS)

define ENSURE_DIR
	@mkdir -p $1
endef

define REMOVE_DIR
	@rm -rf $1
endef

define RUN_OUTPUT
	@if [ -f "./$(OUTPUT)" ]; then ./$(OUTPUT); fi
endef
endif

.PHONY: all buildonly run test clean rebuild

all: $(OUTPUT)

buildonly: all

test: $(TEST_OUTPUT)
	"$(TEST_OUTPUT)"

run: $(OUTPUT)
	$(RUN_OUTPUT)

$(OUTPUT): $(SRC)
	$(call ENSURE_DIR,$(BIN_DIR))
	$(call ENSURE_DIR,$(OBJ_DIR))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) $(SRC) $(LIBS) -o $(OUTPUT)

$(TEST_OUTPUT): $(TEST_SRC) src/game_logic.h
	$(call ENSURE_DIR,$(BIN_DIR))
	$(call ENSURE_DIR,$(OBJ_DIR))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TEST_SRC) -o $(TEST_OUTPUT)

clean:
	$(call REMOVE_DIR,$(BUILD_DIR))

rebuild: clean all
