#include <re2/prog.h>
#include <re2jit/debug.h>
#include <re2jit/jitprog.h>


using namespace re2jit;


JITProg::JITProg(re2::Prog* prog) : jit_function(
    *(_context = new jit_context),
    signature_helper(
        jit_type_int,  // const char *input, int *groups, int ngroups -> int matched
        jit_type_void_ptr, jit_type_void_ptr, jit_type_int, end_params)), _prog(prog), _ptr(NULL)
{
    if (prog == NULL) {
        DEBUG("NULL prog\n");
        return;
    }

    jit_value input   = get_param(0);
    jit_value groups  = get_param(1);
    jit_value ngroups = get_param(2);

    jit_value __true  = new_constant(1, jit_type_int);
    jit_value __false = new_constant(0, jit_type_int);

    jit_label __entry;
    insn_branch(__entry);

    ssize_t i;
    ssize_t e = prog->start();
    ssize_t n = prog->size();

    /* So, the idea:

        1. Create a linked list of "threads", separated into two parts.
        2. Each thread has an entry point, which is a pointer to somewhere
           within the compiled function, as well as a copy of the `groups` array.
        3. Initially, there is one thread in the first part of that list, pointing to
           `prog->start()` with a zero-initialized array.
        4. When a thread encounters a `kInstAlt` instruction, it forks off
           a new thread with its instruction counter pointing to `out1`, appends
           that thread to the first part of the list, then jumps to `out`.
        5. When a thread encounters a `kInstByteRange`, it either fails to match
           or succeeds and moves itself to the second part of the list, then resumes
           execution at the next thread of the first list.
        6. If the first list becomes empty, all threads from the second list
           are moved into the first, the input string is advanced one byte,
           and execution continues at the next thread.
        7. If both lists are empty, the regex did not match.
        8. Execution continues until either all threads die (=> no match),
           or first `N - 1` threads die and the `N`th encounters a `kInstMatch`
           (=> best match found.)

        (Point 8 assumes `kInstAlt` orders alternatives in order of preference, e.g.
        a greedy `x*x` regex compiles to something like `loop: ... kInstAlt(loop, try_x) ...`
        while the non-greedy `x*?x` version is `loop: ... kInstAlt(try_x, loop) ...`.)

        TODO:

          * implement all that;
          * figure out what the hell `kInstAltMatch` is;
          * use `Inst`'s static methods to check whether `kInstEmptyWidth` matches;
          * come up with some way to optimize Unicode lookups -- unwrapping `\N{whatev}`
            into `[all characters which have class whatev]` is a bad idea if these
            characters include every single one of Chinese hieroglyphs.

    */
    for (i = 0; i < n; i++) {
        if (i == e) {
            insn_label(__entry);
        }

        re2::Prog::Inst *op = prog->inst(i);
        // op->[out, out1, lo, hi, cap, empty, match_id]()

        switch (op->opcode()) {
            case re2::kInstAlt:
                // jump to either out_ or out1_
                DEBUG("not jitting: kInstAlt\n");
                return;

            case re2::kInstAltMatch:
                // ??? [out_, out1_]
                DEBUG("not jitting: kInstAltMatch\n");
                return;

            case re2::kInstByteRange:
                // char in [lo_; hi_]
                DEBUG("not jitting: kInstByteRange\n");
                return;

            case re2::kInstCapture:
                // write string offset to group cap_: n-th group is [2n;2n+1)
                DEBUG("not jitting: kInstCapture\n");
                return;

            case re2::kInstEmptyWidth:
                // empty-width state check in empty_
                // kEmptyBeginLine -> check for beginning of line
                // kEmptyEndLine
                // kEmptyBeginText
                // kEmptyEndText
                // kEmptyWordBoundary -> \b
                // kEmptyNonWordBoundary -> \B
                DEBUG("not jitting: kInstEmptyWidth\n");
                return;

            case re2::kInstMatch:
                insn_return(__true);
                break;

            case re2::kInstNop:
                break;

            case re2::kInstFail:
                insn_return(__false);
                break;

            default:
                DEBUG("unknown opcode %d\n", op->opcode());
                return;
        }
    }

    if (compile()) {
        _ptr = (Closure *) closure();
    } else {
        DEBUG("libjit error\n");
    }
}


JITProg::~JITProg()
{
    delete _context;
    delete _prog;
}


JITProg::Status
JITProg::operator()(const re2::StringPiece& text, RE2::Anchor anchor,
                          re2::StringPiece *match, int nmatch) const
{
    if (_ptr == NULL) {
        return NOT_JITTED;
    }

    // TODO something with the anchor?
    int *m = new int[nmatch * 2];
    int  r = _ptr(text.begin(), m, nmatch * 2);

    if (r) {
        for (int i = 0; i < nmatch; i++) {
            match[i].set(text.begin() + m[2 * i], m[2 * i + 1] - m[2 * i]);
        }
    }

    delete[] m;
    return r ? MATCHED : NOT_MATCHED;
}
