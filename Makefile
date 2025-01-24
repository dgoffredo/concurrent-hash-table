test: test.cpp *.h
	clang++ -DITERATIONS=$(ITERATIONS) --stdlib=libc++ --std=c++20 -Wall -Wextra -pedantic -Werror -g -Og -fsanitize=undefined -fsanitize=thread -o $@ $<

.PHONY: format
format:
	clang-format -i --style='{BasedOnStyle: Google, Language: Cpp, ColumnLimit: 80}' *.h *.cpp

.PHONY: clean
clean:
	rm -f test
