#include <stdio.h>

#include <re2/prog.h>
#include <re2jit/debug.h>
#include <re2jit/jitprog.h>
#include <re2jit/util/stackbound.h>


using namespace re2jit;


static inline void _print_bytecode(re2::Prog *prog)
{
    ssize_t i = 0;
    ssize_t n = prog->size();
    printf("\n");

    for (i = 0; i < n; i++) {
        re2::Prog::Inst *op = prog->inst(i);

        if (i == prog->start()) {
            printf("--> start\n");
        }

        switch (op->opcode()) {
            case re2::kInstAlt:
                printf("alt -> %d or %d\n", op->out(), op->out1());
                break;

            case re2::kInstAltMatch:
                printf("altmatch [%d, %d] -> %d or %d???\n", op->lo(), op->hi(), op->out(), op->out1());
                break;

            case re2::kInstByteRange:
                printf("byterange [%d, %d] -> %d\n", op->lo(), op->hi(), op->out());
                break;

            case re2::kInstCapture:
                printf("capture %d -> %d\n", op->cap(), op->out());
                break;

            case re2::kInstEmptyWidth:
                printf("emptywidth %d -> %d\n", op->empty(), op->out());
                break;

            case re2::kInstNop:
                printf("nop -> %d\n", op->out());
                break;

            case re2::kInstMatch:
                printf("match\n");
                break;

            case re2::kInstFail:
                printf("fail\n");
                break;

            default:
                printf("unknown\n");
                break;
        }
    }
}


JITProg::JITProg(re2::Prog* prog) : _prog(prog)
{
    Debug::Write("re2::Prog * = %p\n", prog);

    if (prog == NULL) {
        return;
    }

    // ???
}


JITProg::~JITProg()
{
    delete _prog;
}


#ifdef RE2JIT_INTERPRET
    #include "re2jit/jitprog_interpreted.cc"
#else
    JITProg::Status
    JITProg::operator()(const re2::StringPiece& text, RE2::Anchor anchor,
                              re2::StringPiece *match, int nmatch) const
    {
        return NOT_JITTED;
    }
#endif
