CXX      ?= g++
CXXFLAGS ?= -O3 -Wall -Wextra -Werror

ENABLE_VM         ?= 0
ENABLE_PERF_TESTS ?= 0

ifeq ($(ENABLE_VM),1)
_options += -DRE2JIT_VM
endif

ifeq ($(ENABLE_PERF_TESTS),1)
_testopt += -DRE2JIT_DO_PERF_TESTS
endif

_require_vendor = \
	re2/obj/libre2.a


_require_headers = \
	re2/.git       \
	re2jit/it.h    \


_require_objects = \
	obj/it.o       \


_require_platform_code = \
	re2jit/asm64.h       \
	re2jit/it.x64.cc     \
	re2jit/it.vm.cc      \


_require_library = \
	obj/libre2jit.a


_require_test_run =     \
	test/10-literal     \
	test/11-anchoring   \
	test/12-branching   \
	test/13-exponential \
	test/20-submatching \
	test/30-long        \
	test/31-unicode     \
	test/32-markdownish


ARCHIVE = ar rcs
INSTALL = install -D
PYTHON3 = /usr/bin/python3
CCFLAGS = ./ccflags
DYNLINK = $(CC) -shared -o
COMPILE = $(CXX) $(CXXFLAGS) $(_options) -std=c++11 -I. -I./re2 -fPIC
CMPTEST = $(CXX) $(CXXFLAGS) $(_testopt) -std=c++11 -I. -I./re2 -L./obj -L./re2/obj -pthread -Wno-format-security


.PHONY: all clean test test/%
.PRECIOUS: \
	obj/%.o \
	obj/libre2jit.a \
	obj/libre2jit.so \
	obj/test/%


test: $(_require_test_run)
test/%: ./obj/test/%; ./$<


clean:
	rm -rf obj


re2/.git: .gitmodules
	git submodule update --init re2


re2/obj/libre2.a: re2/.git .git/modules/re2/refs/heads/master
	$(MAKE) -C re2 obj/libre2.a


obj/libre2jit.a: $(_require_objects)
	$(ARCHIVE) $@ $^


obj/libre2jit.so: $(_require_objects)
	$(DYNLINK) $@ $^


obj/it.o: re2jit/it.cc $(_require_headers) $(_require_platform_code)
	@mkdir -p $(dir $@)
	$(COMPILE) -c -o $@ $<


obj/%.o: re2jit/%.cc $(_require_headers)
	@mkdir -p $(dir $@)
	$(COMPILE) -c -o $@ $<


obj/test/%: test/%.cc test/%.h test/framework.cc $(_require_library) $(_require_vendor)
	@mkdir -p $(dir $@)
	$(CMPTEST) -DTEST=$< -DTESTH=$(basename $<).h -o $@ test/framework.cc `$(CCFLAGS) $(basename $<).h` -lre2jit -lre2
