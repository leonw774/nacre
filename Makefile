FLAGS = -g -I include/
DEPS = src/*.c
TEST_DEPS = tests/*.c
TEST_TARGETS = tests/test_epsnfa

test: $(TEST_TARGETS)

tests/test_epsnfa: $(DEPS) $(TEST_DEPS)
	gcc $(FLAGS) -o $@ $^

clean:
	rm $(TEST_TARGETS)