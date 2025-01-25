test: test.cpp *.h
	# clang++ --stdlib=libc++ --std=c++20 -Wall -Wextra -pedantic -Werror -g -Og -fsanitize=undefined -fsanitize=thread -o $@ $<
	# clang++ --stdlib=libc++ --std=c++20 -Wall -Wextra -pedantic -Werror -Ofast -o $@ $<
	# g++ --std=c++20 -Wall -Wextra -pedantic -Werror -Og -g -pg -o $@ $<
	g++ --std=c++20 -Wall -Wextra -pedantic -Werror -Ofast -g -pg -o $@ $<

optimized.asm: test.cpp *.h
	g++ --std=c++20 -Wall -Wextra -pedantic -Werror -Ofast -march=native -fno-asynchronous-unwind-tables -S -o $@ $<
	sed -i '/^\s*\.[a-z]/d' $@
	c++filt <$@ | stdfilt >$@.tmp
	mv $@.tmp $@

.PHONY: format
format:
	clang-format -i --style='{BasedOnStyle: Google, Language: Cpp, ColumnLimit: 80}' *.h *.cpp

.PHONY: clean
clean:
	rm -f test optimized.asm
