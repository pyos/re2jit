#include <re2/prog.h>
#include <re2jit/recompiler.h>


using namespace re2jit;


Recompiler::Recompiler()
{
    *this = NULL;
}


Recompiler::Recompiler(re2::Prog& prog)
{
    *this = prog;
}


Recompiler&
Recompiler::operator = (re2::Prog* prog)
{
    if (prog == NULL) {
        _prog  = NULL;
        _entry = NULL;
        return *this;
    }

    return *this = *prog;
}


Recompiler&
Recompiler::operator = (re2::Prog& prog)
{
    ssize_t i;
  //ssize_t e = prog.start();  // index of entry point instruction
    ssize_t n = prog.size();

    for (i = 0; i < n; i++) {
        re2::Prog::Inst *op = prog.inst(i);
        // op->[out, out1, lo, hi, cap, empty, match_id]()

        switch (op->opcode()) {
            case re2::kInstAlt:
                // jump to either out_ or out1_
                goto cant_compile;

            case re2::kInstAltMatch:
                // ??? [out_, out1_]
                goto cant_compile;

            case re2::kInstByteRange:
                // char in [lo_; hi_]
                goto cant_compile;

            case re2::kInstCapture:
                // write string offset to group cap_: n-th group is [2n;2n+1)
                goto cant_compile;

            case re2::kInstEmptyWidth:
                // empty-width state check in empty_
                // kEmptyBeginLine -> check for beginning of line
                // kEmptyEndLine
                // kEmptyBeginText
                // kEmptyEndText
                // kEmptyWordBoundary -> \b
                // kEmptyNonWordBoundary -> \B
                goto cant_compile;

            case re2::kInstMatch:
                // return true
                goto cant_compile;

            case re2::kInstNop:
                // nop
                goto cant_compile;

            case re2::kInstFail:
                // return false
                goto cant_compile;

        }
    }

    return *this;

cant_compile:
    return *this = NULL;
}


Recompiler::~Recompiler()
{
}


Recompiler::Status
Recompiler::Run(const re2::StringPiece& text, RE2::Anchor anchor,
                      re2::StringPiece *match, int nmatch) const
{
    if (_prog == NULL) {
        return Recompiler::ERR_NO_JIT;
    }

    return Recompiler::ERR_NO_JIT;
}
