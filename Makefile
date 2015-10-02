CC     = g++
CFLAGS = -std=c++11 -Wall -Wextra -Werror -Wno-unused-parameter -fPIC -I. -L.

HDRS = re2jit.h
OBJS = re2jit.o

.PHONY: all clean
.PRECIOUS: %.o

all: libre2jit.so

clean:
	rm -f $(OBJS) examples/*.bin tests/*.bin libre2jit.a libre2jit.so

libre2jit.a: $(OBJS)
	ar rcs $@ $^

libre2jit.so: $(OBJS)
	$(CC) -shared -o $@ $^

%.o: %.cc $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

examples/%.bin: examples/%.cc libre2jit.a
	$(CC) $(CFLAGS) -o $@ $< -lre2jit -lre2

tests/%.bin: tests/%.cc tests/0-template.cc tests/0-template-footer.cc libre2jit.a
	$(CC) $(CFLAGS) -I./tests -o $@ $< -lre2jit -lre2

test: tests/1-basic.bin
	./tests/1-basic.bin
