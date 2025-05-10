SHARED_SRC = $(filter-out src/main.c, $(wildcard src/*.c))

MAIN_SRCS = src/main.c
MAIN_TARGET = nfaregex
MAIN_FLAGS = -O3 -I include/ -Wall -Wextra -Wno-unused-function

PROFILE_FLAGS = -Og -pg -fno-pie -no-pie -I include/ \
	-Wall -Wextra -Wno-unused-function

DEBUG_FLAGS = -g -I include/ \
	-Wall -Wextra -Wno-unused-function -D'IS_DEBUG' # -D'VERBOSE_MATCH'

TEST_TARGETS = $(patsubst tests/%.c, tests/%, $(wildcard tests/*.c))
TEST_FLAGS = -g -I include/ -Wall -Wextra -Wno-unused-function


release: $(SHARED_SRC) $(MAIN_SRCS)
	gcc $(MAIN_FLAGS) -o $(MAIN_TARGET) $^

profile: $(SHARED_SRC) $(MAIN_SRCS)
	gcc $(PROFILE_FLAGS) -o $(MAIN_TARGET) $^

debug: $(SHARED_SRC) $(MAIN_SRCS)
	gcc $(DEBUG_FLAGS) -o $(MAIN_TARGET) $^

tests: $(TEST_TARGETS)

tests/%: $(SHARED_SRC) tests/%.c
	gcc $(TEST_FLAGS) -o $@ $^

clean:
	rm $(TEST_TARGETS) $(MAIN_TARGET) || true
