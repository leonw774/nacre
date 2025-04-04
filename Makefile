FLAGS = -g -I include/ -Wall -Wextra -Wno-unused-function
SRCS = src/*.c
TEST_SRCS = $(wildcard tests/*.c)
TEST_TARGETS = $(patsubst tests/%.c, tests/%, $(TEST_SRCS))

test: $(TEST_TARGETS)

tests/%: $(SRCS) tests/%.c
	gcc $(FLAGS) -o $@ $^

clean:
	rm $(TEST_TARGETS)