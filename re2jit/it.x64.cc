#include <vector>
#include <sys/mman.h>

#include "it.x64.asm.h"


static const struct rejit_threadset_t *NFA    = NULL;  // used for offset calculation
static const struct rejit_thread_t    *THREAD = NULL;

struct re2jit::native
{
    void * entry;
    void * state;
    size_t _size;

    native(re2::Prog *prog) : entry(NULL), state(NULL), _size(0)
    {
        size_t i;
        size_t n = prog->size();

        as code;
        std::vector<as::label> labels(n);

        // NFA library will call this code with itself (%rdi) and a state (%rsi).
        // In our case, the state is a pointer to actual code to execute.
        code.jmp(as::rsi);

        as::label fail;     // jump here to return 0
        as::label succeed;  // jump here to return 1 (if a matching state is reachable)
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

        emitted[*stptr++ = prog->start()]++;

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
                code.mov  (as::mem(as::rdi + &NFA->visited), as::rsi)
                    .test (as::i8(1 << (i % 8)), as::mem(as::rsi + i / 8)).jmp(fail, as::not_zero)
                    .or_  (as::i8(1 << (i % 8)), as::mem(as::rsi + i / 8));

            if (vec.size()) {
                for (auto &op : vec) {
                    // Instead of returning, a failed pseudo-inst should jump
                    // to the next inst in the alternation.
                    as::label fail;

                    switch (op.opcode()) {
                        case re2jit::inst::kUnicodeType:
                            // utf8_chr = rejit_read_utf8(nfa->input, nfa->length);
                            code.push (as::rdi)
                                .mov  (as::mem(as::rdi + &NFA->length), as::rsi)
                                .mov  (as::mem(as::rdi + &NFA->input),  as::rdi)
                                .call (&rejit_read_utf8)
                                .pop  (as::rdi)
                            // if ((utf8_length = utf8_chr >> 32) == 0) return;
                                .mov  (as::rax, as::rdx)
                                .shr  (32,      as::rdx).jmp(fail, as::zero)
                            // if ((rejit_unicode_category(utf8_chr) & UNICODE_CATEGORY_GENERAL) != arg) return;
                                .push (as::rdi)
                                .push (as::rdx)
                                .mov  (as::eax, as::edi)
                                .call (&rejit_unicode_category)  // inlining is hard.
                                .pop  (as::rdx)
                                .pop  (as::rdi)
                                .and_ (UNICODE_CATEGORY_GENERAL, as::al)
                                .cmp  (op.arg(), as::al).jmp(fail, as::not_equal)
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
                    code.cmp(as::i32(seq.size()), as::mem(as::rdi + &NFA->length)).jmp(fail, as::less_u)
                        .mov(as::mem(as::rdi + &NFA->input), as::rsi);

                    for (auto op : seq) {
                        code.mov(as::mem(as::rsi), as::al)
                            .add(as::i8(1), as::rsi);

                        if (op->foldcase())
                            // if ('A' <= al && al <= 'Z') al = al - 'A' + 'a';
                            code.mov(as::rax - 'A', as::ecx)
                                .mov(as::rcx + 'a', as::edx)
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
                    code.cmp  (as::i32(op->cap()), as::mem(as::rdi + &NFA->groups))
                        .jmp  (labels[op->out()], as::less_equal_u)
                    // esi = nfa->running->groups[cap]; nfa->running->groups[cap] = nfa->offset;
                        .mov  (as::mem(as::rdi + &NFA->running), as::rcx)
                        .mov  (as::mem(as::rdi + &NFA->offset),  as::eax)
                        .mov  (as::mem(as::rcx + &THREAD->groups[op->cap()]), as::esi)
                        .mov  (as::eax, as::mem(as::rcx + &THREAD->groups[op->cap()]))
                    // eax = out(nfa);
                        .push (as::rsi)
                        .push (as::rcx)
                        .call (labels[op->out()])
                        .pop  (as::rcx)
                        .pop  (as::rsi)
                    // nfa->running->groups[cap] = esi; return eax;
                        .mov  (as::esi, as::mem(as::rcx + &THREAD->groups[op->cap()]))
                        .ret  ();
                    EMIT_NEXT(op->out());
                    break;

                case re2::kInstEmptyWidth:
                    // if (~nfa->empty & empty) return;
                    code.mov  (as::mem(as::rdi + &NFA->empty), as::eax)
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

        void *target = mmap(NULL, code.size(), PROT_READ | PROT_WRITE,
                                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (target == (void *) -1)
            return;

        if (!code.write(target) || mprotect(target, code.size(), PROT_READ | PROT_EXEC) == -1) {
            munmap(target, code.size());
            return;
        }

        entry = target;
        state = code.dereference(target, labels[prog->start()]);
        _size = code.size();
    }

   ~native()
    {
        munmap(entry, _size);
    }
};
