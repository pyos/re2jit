CXX    ?= g++
CFLAGS ?= -Wall -Wextra -Werror -Wno-unused-parameter
_CFLAGS = $(CFLAGS) -std=c++11 -fPIC -I. -I./re2 -L. -L./re2/obj

HDRS = re2jit.h recompiler.h
OBJS = re2jit.o recompiler.o

.PHONY: all clean re2
.PRECIOUS: %.o

all: libre2jit.so

clean:
	rm -f $(OBJS) examples/*.bin tests/*.bin libre2jit.a libre2jit.so

re2/obj/libre2.a: .gitmodules
	$(MAKE) -C re2 obj/libre2.a

libre2jit.a: $(OBJS)
	ar rcs $@ $^

libre2jit.so: $(OBJS)
	$(CXX) -shared -o $@ $^

%.o: %.cc $(HDRS)
	$(CXX) $(_CFLAGS) -c -o $@ $<

examples/%.bin: examples/%.cc libre2jit.a
	$(CXX) $(_CFLAGS) -pthread -o $@ $< -lre2jit -lre2

tests/%.bin: tests/%.cc tests/0-template.cc tests/0-template-footer.cc libre2jit.a re2/obj/libre2.a
	$(CXX) $(_CFLAGS) -pthread -I./tests -o $@ $< -lre2jit -lre2

test: tests/1-basic.bin
	./tests/1-basic.bin
