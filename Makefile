CXX=clang++
CXXFLAGS=-std=c++14 -O0
INCLUDES=-I/usr/local/include/lua5.1
LIBS=-l lua5.1

.PHONY: all
all: main

main: main.o
	$(CXX) $< -o $@ $(LIBS) $(INCLUDES)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -c $(INCLUDES)

.PHONY: clean
clean:
	rm -f main
	rm -f *.o
