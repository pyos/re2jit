#include <vector>
#include <sys/mman.h>

#include "it.x64.asm.h"


static const struct rejit_threadset_t *NFA    = NULL;  // used for offset calculation
static const struct rejit_thread_t    *THREAD = NULL;

struct re2jit::native
{
    void *_code;
    void *_entry;
    size_t _size;

    native(re2::Prog *prog) : _code(NULL), _entry(NULL), _size(0)
    {
        size_t i;
        size_t n = prog->size();

        as code;
        std::vector<as::label> labels(n);

        as::label fail;  // return 0, meaning did not enter an accepting state
        as::label succeed;  // return 1, meaning there was a match somewhere
        code.mark(fail).xor_(as::eax, as::eax).mark(succeed).ret();

        // How many transitions have the i-th opcode as a target.
        // Opcodes with indegree 1 don't need to be tracked in the bit vector
        // (as there is only one way to reach them and we've already checked that).
        // Opcodes with indegree 0 are completely unreachable, no need to compile those.
        std::vector< unsigned > indegree(n);
        std::vector< unsigned > emitted(n);

        ssize_t *stack = new ssize_t[prog->size()];
        ssize_t *stptr = stack;
        indegree[*stptr++ = prog->start()]++;

        while (stptr != stack) {
            auto op  = prog->inst(*--stptr);
            auto vec = re2jit::is_extcode(prog, op);

            for (auto& op : vec)
                if (!indegree[op.out()]++)
                    *stptr++ = op.out();

            if (!vec.size())
                switch (op->opcode()) {
                    case re2::kInstAlt:
                    case re2::kInstAltMatch:
                        if (!indegree[op->out1()]++)
                            *stptr++ = op->out1();

                    default:
                        if (!indegree[op->out()]++)
                            *stptr++ = op->out();

                    case re2::kInstFail:
                    case re2::kInstMatch:
                        break;

                    case re2::kInstByteRange:
                        if (!indegree[op->out()])
                            *stptr++ = op->out();
                        // subsequent byte ranges are concatenated into one block of code
                        if (prog->inst(op->out())->opcode() != re2::kInstByteRange)
                            indegree[op->out()]++;
                }
        }

        *stptr++ = prog->start();

        while (stptr != stack) {
            #define EMIT_NEXT(i) if (!emitted[i]++) *stptr++ = i
            #define EMIT_JUMP(i) EMIT_NEXT(i); else code.jmp(labels[i])

            code.mark(labels[i = *--stptr]);
            // Each opcode should conform to the System V ABI calling convention.
            //   argument 1: %rdi = struct rejit_threadset_t *nfa
            //   return reg: %rax = 1 iff found a match somewhere
            auto op  = prog->inst(i);
            auto vec = re2jit::is_extcode(prog, op);

            // kInstFail will do `ret` anyway.
            if (op->opcode() != re2::kInstFail && indegree[i] > 1)
                // if (bit(nfa->visited, i) == 1) return; bit(nfa->visited, i) = 1;
                code.mov  (as::mem(as::rdi) + &NFA->visited, as::rsi)
                    .test (as::i8(1 << (i % 8)), as::mem(as::rsi) + i / 8).jmp(fail, as::not_zero)
                    .or_  (as::i8(1 << (i % 8)), as::mem(as::rsi) + i / 8);

            if (vec.size()) {
                for (auto &op : vec) {
                    as::label fail;

                    switch (op.opcode()) {
                        case re2jit::inst::kUnicodeType:
                            // utf8_chr = rejit_read_utf8(nfa->input, nfa->length);
                            code.push (as::rdi)
                                .mov  (as::mem(as::rdi) + &NFA->length, as::rsi)
                                .mov  (as::mem(as::rdi) + &NFA->input,  as::rdi)
                                .call (&rejit_read_utf8)
                                .pop  (as::rdi)
                            // if ((utf8_length = utf8_chr >> 32) == 0) return;
                                .mov  (as::rax, as::rdx)
                                .shr  (32,      as::rdx).jmp(fail, as::zero)
                            // if ((UNICODE_CODEPOINT_TYPE[utf8_chr] & UNICODE_GENERAL) != arg) return;
                                .mov  (as::i64(UNICODE_CODEPOINT_TYPE), as::rsi)
                                .mov  (as::eax, as::eax)  // zero upper 32 bits
                                .add  (as::rsi, as::rax)  // TODO mov (%rsi, %rax, 1), %cl
                                .mov  (as::mem(as::rax), as::cl)
                                .and_ (UNICODE_GENERAL,  as::cl)
                                .cmp  (op.arg(),         as::cl).jmp(fail, as::not_equal)
                            // rejit_thread_wait(nfa, &out, utf8_length);
                                .mov  (labels[op.out()], as::rsi)
                                .push (as::rdi)
                                .call (&rejit_thread_wait)
                                .pop  (as::rdi);
                            EMIT_NEXT(op.out());
                            break;
                    }

                    code.test(as::eax, as::eax).jmp(succeed, as::not_zero).mark(fail);
                }

                code.jmp(fail);
            } else switch (op->opcode()) {
                case re2::kInstAltMatch:
                case re2::kInstAlt:
                    // if (out(nfa)) return 1;
                    code.push  (as::rdi)
                        .call  (labels[op->out()])
                        .pop   (as::rdi)
                        .test  (as::eax, as::eax).jmp(succeed, as::not_zero);

                    EMIT_NEXT(op->out());
                    EMIT_JUMP(op->out1());
                    break;

                case re2::kInstByteRange: {
                    std::vector<re2::Prog::Inst *> seq{ op };

                    while ((op = prog->inst(op->out()))->opcode() == re2::kInstByteRange)
                        seq.push_back(op);

                    // if (nfa->length < len) return; else rsi = nfa->input;
                    code.cmp(as::i32(seq.size()), as::mem(as::rdi) + &NFA->length).jmp(fail, as::less_u)
                        .mov(as::mem(as::rdi) + &NFA->input, as::rsi);

                    for (auto op : seq) {
                        code.mov(as::mem(as::rsi), as::al)
                            .add(as::i8(1), as::rsi);

                        if (op->foldcase())
                            // if ('A' <= al && al <= 'Z') al = al - 'A' + 'a';
                            code.lea(as::mem(as::rax) - 'A', as::ecx)
                                .lea(as::mem(as::rcx) + 'a', as::edx)
                                .cmp('Z' - 'A', as::cl)
                                .mov(as::edx, as::eax, as::less_equal_u);

                        if (op->hi() == op->lo())
                            // if (al != lo) return;
                            code.cmp(op->lo(), as::al).jmp(fail, as::not_equal);
                        else
                            // if (al < lo || hi < al) return;
                            code.sub(op->lo(),            as::al)
                                .cmp(op->hi() - op->lo(), as::al).jmp(fail, as::more_u);
                    }

                    // return rejit_thread_wait(nfa, &out, len);
                    code.mov(labels[seq.back()->out()], as::rsi)
                        .mov(seq.size(), as::edx)
                        .jmp(&rejit_thread_wait);
                    EMIT_NEXT(seq.back()->out());
                    break;
                }

                case re2::kInstCapture:
                    // if (nfa->groups <= cap) goto out;
                    code.cmp  (as::i32(op->cap()), as::mem(as::rdi) + &NFA->groups)
                        .jmp  (labels[op->out()], as::less_equal_u)
                    // esi = nfa->running->groups[cap]; nfa->running->groups[cap] = nfa->offset;
                        .mov  (as::mem(as::rdi) + &NFA->running, as::rcx)
                        .mov  (as::mem(as::rdi) + &NFA->offset,  as::eax)
                        .mov  (as::mem(as::rcx) + &THREAD->groups[op->cap()], as::esi)
                        .mov  (as::eax, as::mem(as::rcx) + &THREAD->groups[op->cap()])
                    // eax = out(nfa);
                        .push (as::rsi)
                        .push (as::rcx)
                        .call (labels[op->out()])
                        .pop  (as::rcx)
                        .pop  (as::rsi)
                    // nfa->running->groups[cap] = esi; return eax;
                        .mov  (as::esi, as::mem(as::rcx) + &THREAD->groups[op->cap()])
                        .ret  ();
                    EMIT_NEXT(op->out());
                    break;

                case re2::kInstEmptyWidth:
                    // if (~nfa->empty & empty) return;
                    code.mov  (as::mem(as::rdi) + &NFA->empty, as::eax)
                        .not_ (as::eax)
                        .test (op->empty(), as::eax).jmp(fail, as::not_zero);
                    EMIT_JUMP(op->out());
                    break;

                case re2::kInstNop:
                    EMIT_JUMP(op->out());
                    break;

                case re2::kInstMatch:
                    // return rejit_thread_match(nfa);
                    code.jmp(&rejit_thread_match);
                    break;

                case re2::kInstFail:
                    code.ret();
                    break;
            }
        }

        delete[] stack;

        as::i8 *target = (as::i8 *) mmap(NULL, code.size(), PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (target == (as::i8 *) -1)
            return;

        if (!code.write(target) || mprotect(target, code.size(), PROT_READ | PROT_EXEC) == -1) {
            munmap(target, code.size());
            return;
        }

        _code  = target;
        _entry = target + labels[prog->start()]->offset;
        _size  = code.size();
    }

   ~native()
    {
        munmap(_code, _size);
    }

    rejit_entry_t entry() const
    {
        return (rejit_entry_t) _entry;
    }

    void run(struct rejit_threadset_t *nfa) const
    {
        rejit_thread_dispatch(nfa);
    }
};
