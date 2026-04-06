CPP_FILES		:= $(wildcard src/*.cpp)
OBJ_FILES		:= $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))
CC_FLAGS		:= --std=c++23 -I./src -I../lib/PEGTL/include -I../lib -I../util -g3 -DDEBUG -pedantic -pedantic-errors -Werror=pedantic
CC				:= g++
PL_CLASS        := IR
COMPILER 		:= bin/$(PL_CLASS)
TEST_DIR		:= tests
OUT_DIR			:= out
BUILD_DIR		:= build
ROOT_DIR		:= ..
RUNTIME			:= $(ROOT_DIR)/lib/runtime.c

build: dirs $(COMPILER)

all: dirs $(COMPILER)

dirs: obj bin

obj:
	mkdir -p $@

bin:
	mkdir -p $@

$(COMPILER): $(OBJ_FILES)
	$(CC) $(LD_FLAGS) -o $@ $^

obj/%.o: src/%.cpp
	$(CC) $(CC_FLAGS) -c -o $@ $<

compile:
	python3 ../scripts/compile.py $(ROOT_DIR) $(PL_CLASS) $(FILE) $(BUILD_DIR) $(RUNTIME)

run:
	python3 ../scripts/compile_and_run.py $(ROOT_DIR) $(PL_CLASS) $(FILE) $(BUILD_DIR) $(RUNTIME)

test:
	python3 ../scripts/test_compiler.py $(ROOT_DIR) $(PL_CLASS) $(TEST_DIR) $(BUILD_DIR) $(RUNTIME) --out $(OUT_DIR)

copy_instructor_bin:
	mkdir -p bin ;
	cp .bin/* bin/ ;

clean:
	rm -fr bin obj *.out *.o core.* build/ out/

.PHONY: compiler all dirs bin $(COMPILER) test test_fast copy_instructor_bin clean
