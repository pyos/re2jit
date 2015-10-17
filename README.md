## re2jit

JIT compiler for [re2](https://github.com/google/re2/) regexps.

### Usage

First, `make obj/libre2jit.a` or `make obj/libre2jit.so`.

Second,

```c++
#include <re2jit/it.h>

...

re2jit::it regexp("[Hh]ello,? ([^'!\\.]+)(?:!|\\.|)");
// First (zeroth?) entry is the entire match.
//   subgroups[i].data() -- non-null-terminated string (const char *)
//   subgroups[i].size() -- its length
re2::StringPiece subgroups[2];

bool matched = regexp.match(
    "And then it said, 'Hello, World!'",
    // Restrict where the matched string may appear in the input.
    //   RE2::UNANCHORED -- anywhere
    //   RE2::ANCHOR_START -- at the beginning
    //   RE2::ANCHOR_BOTH  -- at the beginning, spanning the whole input
    // Default = RE2::ANCHOR_START
    RE2::UNANCHORED,
    // Array to store the subgroups in, and its length.
    // Default = NULL, 0
    subgroups, 2);
```

Third, build with `-lre2jit -lre2 -pthread`. (Don't forget to add appropriate `-I` & `-L`.)

### So...how fast is it?

Not really. I mean, it's an NFA. What do you expect?

1. If you do not require subgroup extraction, re2 will use a DFA. Thus, re2 will be faster.
2. If your regexp is constructed in such a way that whenever there is a branch,
   only one path can be chosen based *only* on the next input byte, then re2 will use a special
   kind of NFA constructed with that in mind. Thus, re2 will be faster.
3. If your input is short and the expression is even shorter, re2 will trade some memory
   for speed. Thus, re2 will be faster.

But, if none of the above hold (i.e. you have a reasonably complex regexp and a good deal
of input *and* you want to know where some group begins and ends), then *maybe*, just maybe,
this thing will be faster than re2. I don't know. Run some benchmarks.

(And never trust someone when they tell you their code is fast without proof. re2 was supposed
to be, and it fails horribly if you try to write something equivalent to Python's `\w`.
All because Python reads the next character and looks at its class in the Unicode database,
while re2 happily unwraps `[\p{L}\p{N}_]` into `[list of all letters and digits in Unicode]`.
Ugh.)

### Supported platforms

 * x86-64 on \*nix (System V calling convention **required**)
 * Everything else - only through an interpreter, which is a slightly slower version of re2's
   NFA. At least, most of the time it *is* slightly slower. Sometimes it is a lot faster
   for some reason, which is pretty weird. (For an example see
   [tests/long.cc](https://github.com/pyos/re2jit/blob/master/test/long.cc#L36).)
   Point is, don't use it.

### In fact,

don't use this thing either way.
