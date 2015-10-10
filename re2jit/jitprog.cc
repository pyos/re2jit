#include <re2/prog.h>
#include <re2jit/debug.h>
#include <re2jit/jitprog.h>


using namespace re2jit;


JITProg::JITProg(re2::Prog* prog) : jit_function(
    *(_context = new jit_context),
    signature_helper(
        jit_type_int,
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
