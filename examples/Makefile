CXX := /opt/local/bin/g++-mp-15
CXXFLAGS := -std=c++26 -Wall -Wextra -Wpedantic
INCLUDES := -I../components/apex653/include/h/apex
LIB := ../build/host-macos-gcc15/components/apex653/libapex653.a

main: main.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $(INCLUDES) main.cpp $(LIB) -o main

clean:
	rm -f main

.PHONY: clean
