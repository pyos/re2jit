CXX      ?= g++
CXXFLAGS ?= -O3 -Wall -Wextra -Werror

ENABLE_FALLBACK    ?= 1
ENABLE_NEW_OPCODES ?= 0

ifeq ($(FORCE_VM),1)
_options += -DRE2JIT_VM
endif

ifeq ($(ENABLE_DEBUG),1)
_options += -DRE2JIT_DEBUG
endif

ifneq ($(ENABLE_FALLBACK),1)
_options += -DRE2JIT_NO_FALLBACK
endif

ifneq ($(ENABLE_NEW_OPCODES),1)
_options += -DRE2JIT_NO_EXTCODES
endif

ifeq ($(ENABLE_PERF_TESTS),1)
_options += -DRE2JIT_DO_PERF_TESTS
endif

_require_vendor = \
	re2/obj/libre2.a


_require_headers = \
	re2jit/it.h \
	re2jit/list.h \
	re2jit/debug.h \
	re2jit/threads.h \
	re2jit/rewriter.h


_require_objects = \
	obj/it.o \
	obj/debug.o \
	obj/threads.o \
	obj/rewriter.o


_require_platform_code = \
	re2jit/it.x64.cc \
	re2jit/it.vm.cc


_require_library = \
	obj/libre2jit.a


_require_test_run = \
	test/01-invalid \
	test/10-literal \
	test/11-anchoring \
	test/12-branching \
	test/13-exponential \
	test/20-submatching \
	test/30-long \
	test/31-unicode


ARCHIVE = ar rcs
INSTALL = install -D
CCFLAGS = ./ccflags
DYNLINK = $(CC) -shared -o
COMPILE = $(CXX) $(CXXFLAGS) $(_options) -std=c++11 -fPIC -I. -I./re2 -L./obj -L./re2/obj


.PHONY: all clean test test/%
.PRECIOUS: \
	obj/%.o \
	obj/libre2.a \
	obj/libre2.so \
	obj/test/%


test: $(_require_test_run)
test/%: ./obj/test/%; ./$<


clean:
	rm -rf obj


re2: .gitmodules
	git submodule update --init re2


re2/obj/libre2.a: re2 .git/modules/re2/refs/heads/master
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
	$(COMPILE) -DTEST=$< -DTESTH=$(basename $<).h -pthread -o $@ test/framework.cc `$(CCFLAGS) $(basename $<).h` -lre2jit -lre2
