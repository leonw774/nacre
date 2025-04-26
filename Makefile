SHARED_SRC = $(filter-out src/main.c, $(wildcard src/*.c))

MAIN_SRCS = src/main.c
MAIN_TARGET = nfaregex
MAIN_FLAGS = -O2 -I include/ -Wall -Wextra -Wno-unused-function

DEBUG_FLAGS = -g -I include/ -Wall -Wextra -Wno-unused-function -D'IS_DEBUG'

TEST_TARGETS = $(patsubst tests/%.c, tests/%, $(wildcard tests/*.c))
TEST_FLAGS = -g -I include/ -Wall -Wextra -Wno-unused-function


$(MAIN_TARGET): $(SHARED_SRC) $(MAIN_SRCS)
	gcc $(MAIN_FLAGS) -o $@ $^

debug: $(SHARED_SRC) $(MAIN_SRCS)
	gcc $(DEBUG_FLAGS) -o $(MAIN_TARGET) $^

tests: $(TEST_TARGETS)

tests/%: $(SHARED_SRC) tests/%.c
	gcc $(TEST_FLAGS) -o $@ $^

clean:
	rm $(TEST_TARGETS) $(MAIN_TARGET) || true