FLAGS = -g -I include/
DEPS = src/*.c
TEST_DEPS = $(wildcard tests/*.c)
TEST_TARGETS = $(patsubst tests/%.c, tests/%, $(TEST_DEPS))

test: $(TEST_TARGETS)

tests/%: $(DEPS) tests/%.c
	gcc $(FLAGS) -o $@ $^

clean:
	rm $(TEST_TARGETS)