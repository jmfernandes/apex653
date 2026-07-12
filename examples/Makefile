CXX := /opt/local/bin/g++-mp-15
# Preset whose build tree to link against, e.g. host-macos-gcc15-asan for a sanitized library.
PRESET := host-macos-gcc15
# Matching -fsanitize=... flags for that preset's build, e.g. "-fsanitize=address,undefined
# -fno-omit-frame-pointer". Must match what the PRESET's library was actually built with.
SANITIZE :=
CXXFLAGS := -std=c++26 -Wall -Wextra -Wpedantic $(SANITIZE)
INCLUDES := -I../components/apex653/include/h/apex
LIB := ../build/$(PRESET)/components/apex653/libapex653.a

main: main.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $(INCLUDES) main.cpp $(LIB) $(SANITIZE) -o main

clean:
	rm -f main

.PHONY: clean
