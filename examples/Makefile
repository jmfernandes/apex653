CXX := /opt/local/bin/g++-mp-15
# Preset whose build tree to link against, e.g. host-macos-gcc15-asan for a sanitized library.
PRESET := host-macos-gcc15
# Matching -fsanitize=... flags for that preset's build, e.g. "-fsanitize=address,undefined
# -fno-omit-frame-pointer". Must match what the PRESET's library was actually built with.
SANITIZE :=
# -fcontracts: both macOS presets (host-macos-gcc15, host-macos-gcc15-asan) are built with this
# unconditionally (see docs/DESIGN.md), and a contracts-compiled libapex653.a requires the final
# linking translation unit to also use -fcontracts -- same requirement as SANITIZE below, just
# not optional for these presets.
CXXFLAGS := -std=c++26 -Wall -Wextra -Wpedantic -Wno-unused-parameter -fcontracts $(SANITIZE)
INCLUDES := -I../components/apex653/include/h/apex
LIB := ../build/$(PRESET)/components/apex653/libapex653.a

main: main.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $(INCLUDES) main.cpp $(LIB) $(SANITIZE) -o main

clean:
	rm -f main

.PHONY: clean
