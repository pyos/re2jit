## re2jit

A compiler for [re2](https://github.com/google/re2/) regexps.

### What?

Do you know what Thompson's NFA construction is? Have you read the original paper?
It compiled regexps into asm code for an IBM mainframe, which is the sort of thing
[people in the 60's did](http://www.diku.dk/hjemmesider/ansatte/henglein/papers/thompson1968.pdf).
Now, a modern implementation of the same technique (re2) adopts a
[virtual machine approach](https://swtch.com/~rsc/regexp/regexp2.html) instead. Which is,
obviously, slower. Slower than Python on some regexps! We can do better than that.

### So...how fast is this thing?

Depends.

**For a simple unanchored search** it is exactly as fast as re2. Unanchored searches
are done using the standard re2 DFA, as only the NFA is compiled to native code,
and it is slower than the DFA. This is only true, however, if you discard the contents
of all groups, as the DFA has no way to report their boundaries.

**If you do want to know where each group matched**, it should be 20-30% faster than re2.
Try running `make test/32-markdownish ENABLE_PERF_TESTS=1`, for example. (This test is
a converter of a small subset of Markdown to HTML that uses two regexps to do
the heavy lifting.)

**If you need to detect word boundaries**, that is, if you use `\b` or `\B`, it won't
work at all. (That's because they're not implemented.)

**If your regexp has to match whole Unicode classes**, i.e. contains `\pN` or `\p{Lu}`
or something similar (note that `\w`, `\b`, etc. are ASCII-only in re2, so Python's `\w`
is equivalent to `[\pL\pN_]`), it may be up to 100x faster than re2 because re2jit's
implementation of `\p{X}` matches a whole code point using a lookup table instead of
unwrapping, for example, `\pL` into `[enumeration of all letters in Unicode]` as re2
itself does. Just run `make test/30-long ENABLE_PERF_TESTS=1` to see how good it is
[at tokenizing dg](https://github.com/pyos/dg/blob/master/core/3.parser.dg#L35)
(the regexp for which contains the aforementioned Pythonic `\w`.)

**Other than that**, hard to say. Just roll some benchmarks of your own, will you?

### About backreferences...

Backreferences are supported. And no, don't go all "hey, I read on Stack Overflow
that backreferences require backtracking".
[Here's Russ Cox's opinion on the matter](https://swtch.com/~rsc/regexp/regexp2.html#real):

>Backreferences are trivial in backtracking implementations. **In Pike's VM, they can be
>accommodated, except that the argument about discarding threads with duplicate PCs no
>longer holds**: two threads with the same PC might have different capture sets, and now
>the capture sets can influence future execution, so an implementation has to keep both
>threads, a potentially exponential blowup in state.

So what do we do? Simple - we only discard threads with duplicate program counters
if we're 100% sure they matched all the groups at the same positions in the input string.
And while that does not guarantee polynomial time in worst case...

 * **If your regexp is backreference-free**, it will match in linear time. Guaranteed.

 * **In general, if no backreferenced group can match multiple times** - for example,
   a regexp like ``(`).*?\1`` satisfies that rule, while `(x*)*\1` does not -
   it is guaranteed to run in O(n^(2k + 1)) time, where k is the number of backreferenced
   groups (1 in the above example). This is, coincidentally, exactly the amount of time
   required to match the regex by an exhaustive search on its state space. (A regexp with
   m states and k backreferenced groups has O(m \* n^(2k + 1)) possible unique combinations
   of a state, a position in an input string, and offsets for the start and end of each
   relevant group.) Note that this is the worst case - that regexp with backticks is
   actually O(n + n * m^2) where m is the number of backticks the input contains. So O(n)
   on average. I mean, how many of these an average text contains? (0. The answer is 0.)

 * **If, however, a backreferenced group is repeated**, all bets are off.
   The regexp `(x*)*\1` requires exponential time to match. An equivalent regexp
   `(x*)*` (it is equivalent because the last match of `x*` is always an empty string)
   does not, as it contains no backreferences. Likewise, ``(`).*?\1(x*)*`` is O(n^3)
   because only ``(`)`` is backreferenced, not `(x*)`, while `(x*)\1` is also O(n^3)
   as `(x*)` only matches once this time. Or O(n^2) if it is anchored at the beginning
   of the input.

Note, however, that since re2 does not support backreferences, it's not possible
to use its DFA to run unanchored searches with regexps that contain any. So an unanchored
search with backreferences is likely to be slow. Not because of some algorithmic complexity
thing, but because of a good old huge constant behind that O.

### Enough theory, time for some practice

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

#### Oh no, `make` returned a bunch of errors!

Make sure you're using one of the supported platforms. Which is to say, the only one.
This thing was tested on Linux. It should also work on Mac OS X or any other 64-bit
UNIX/UNIX-like OS. Either way, your CPU should be x86-64.

If you only have a PowerPC or a MIPS or whatever, tough luck. Your only option is to
run an interpreter (`make ENABLE_VM=1`), which is basically a slower version of re2's
NFA. At least, most of the time it is slower. Sometimes it's a lot faster for some reason,
which is pretty weird. It might also be faster if you're neck-deep in that Unicode charclass
mess I talked about above. Other than that, better not use it.

### In fact,

don't use this thing either way.
