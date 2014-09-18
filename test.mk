

CXXFLAGS := -std=c++0x -DTWEEN_UNIT_TEST -pthread -g -O2
LDFLAGS := -pthread
LD := $(CXX)

tweentest : tween.o catch.o
	$(LD) $(LDFLAGS) -o$@ $^
