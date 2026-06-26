OUTPUT = a.exe

CS = -Wall -Wextra -std=c++23 -Wshadow -Werror -Wno-unused-parameter -Wno-sign-compare

debug:
	clang++ $(CS) -g main.cpp -o $(OUTPUT)

release:
	clang++ $(CS) -s -O3 main.cpp -o $(OUTPUT)

clean:
	rm $(OUTPUT)
