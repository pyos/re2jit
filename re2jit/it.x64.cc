#include <set>
#include <vector>
#include <sys/mman.h>

#include "asm64.h"

// `&NFA->input` -- like offsetof, but shorter and with 100% more undefined behavior.
static constexpr const struct rejit_threadset_t *NFA    = NULL;
static constexpr const struct rejit_thread_t    *THREAD = NULL;

struct re2jit::native
{
    const void *state = NULL;
    size_t space = 0;  // = 1 bit for each state reachable through multiple paths
    size_t _size = 0;

    native(re2::Prog *prog)
    {
        std::vector<unsigned> stack(prog->size());
        unsigned *it;
        unsigned *visited;
        #define DFS(count) it      = &stack[0]; \
                           visited = &count[0]; \
                           visited[*it++ = prog->start()]++; \
                           while (it-- != &stack[0])
        #define VISIT(i) if (!visited[i]++) *it++ = i

        // dead code elimination pass:
        //   1. ignore all instructions with indegree = 0
        //   2. do not emit bitmap checks for those with indegree = 1
        //   3. find out changing which groups would require resetting the state bitmap
        std::vector<unsigned> indegree(prog->size());
        std::set   <unsigned> backrefs;

        DFS(indegree) {
            auto op  = prog->inst(*it);
            auto ext = re2jit::get_extcode(prog, op);

            for (auto& op : ext) switch (op.opcode) {
                case re2jit::kBackreference:
                    backrefs.insert(op.arg);

                default:
                    VISIT(op.out);
            }

            if (!ext.size()) switch (op->opcode()) {
                case re2::kInstAlt:
                case re2::kInstAltMatch:
                    VISIT(op->out1());

                default:
                    VISIT(op->out());

                case re2::kInstFail:
                case re2::kInstMatch:
                    break;

                case re2::kInstByteRange:
                    do  // non-extcode byte ranges are concatenated into one block of code
                        // => intermediate insts are unreachable by themselves
                        op = prog->inst(op->out());
                    while (op->opcode() == re2::kInstByteRange && !re2jit::is_extcode(prog, op));

                    VISIT(op->id(prog));
            }
        }

        // compiler pass:
        //   emitted code is a series of opcodes, each `int(struct rejit_threadset_t* rdi)`.
        //   return value is 1 iff a matching state is reachable through epsilon transitions.
        //   first emitted opcode is the regexp's entry point.
        as::code  code;
        as::label fail, succeed;
        std::vector<as::label> labels(prog->size());
        std::vector<unsigned> emitted(prog->size());

        DFS(emitted) {
            auto op  = prog->inst(*it);
            auto ext = re2jit::get_extcode(prog, op);
            as::label next_extcode;

            code.mark(labels[*it]);

            // kInstFail will do `ret` anyway.
            if (op->opcode() != re2::kInstFail && indegree[*it] > 1) {
                // if (bit(nfa->bitmap, *it) == 1) return; bit(nfa->bitmap, *it) = 1;
                code.mov  (as::mem(as::rdi + &NFA->bitmap), as::rsi)
                    .test (as::i8(1 << (space % 8)), as::mem(as::rsi + space / 8)).jmp(fail, as::not_zero)
                    .or_  (as::i8(1 << (space % 8)), as::mem(as::rsi + space / 8));
                space++;
            }

            for (auto op = ext.crbegin(); op != ext.crend(); ++op) {
                code.mark(next_extcode);

                if (op != ext.crend() - 1)
                    code.push(as::rdi)
                        .call(next_extcode = as::label())
                        .pop (as::rdi);

                switch (op->opcode) {
                    case re2jit::kUnicodeTypeGeneral:
                    case re2jit::kUnicodeTypeSpecific:
                        // rax = rejit_read_utf8(nfa->input, nfa->length);
                        code.push (as::rdi)
                            .mov  (as::mem(as::rdi + &NFA->length), as::esi)
                            .mov  (as::mem(as::rdi + &NFA->input),  as::rdi)
                            .call (&rejit_read_utf8)
                            .pop  (as::rdi)
                        // if ((edx = rax >> 24) == 0) return;
                            .mov  (as::rax, as::rdx)
                            .shr  (24,      as::rdx).jmp(fail, as::zero)
                        // inlined: eax = rejit_unicode_category(eax) @ unicode.h
                            .movzb(as::al,    as::ecx)
                            .and_ (0x1FFFFFu, as::eax)
                            .shr  (8,         as::eax)
                            .add  (as::mem(as::p32(UNICODE_CATEGORY_1) + as::rax * 4), as::ecx)
                            .movzb(as::mem(as::p32(UNICODE_CATEGORY_2) + as::rcx),     as::eax);

                        if (op->opcode == re2jit::kUnicodeTypeGeneral)
                            code.and_(UNICODE_CATEGORY_GENERAL, as::eax);

                        code.cmp  (as::i8(op->arg), as::eax)
                            .jmp  (fail, as::not_equal)
                        // return rejit_thread_wait(nfa, &out, edx);
                            .mov  (labels[op->out], as::rsi)
                            .jmp  (&rejit_thread_wait);
                        VISIT(op->out);
                        break;

                    case re2jit::kBackreference:
                        // if (nfa->groups <= arg * 2) return;
                        code.cmp(as::i32(op->arg * 2), as::mem(as::rdi + &NFA->groups))
                            .jmp(fail, as::less_equal_u)
                        // if (end == -1 || end < start) return; if (end == start) return out();
                            .mov (as::mem(as::rdi + &NFA->running), as::rsi)
                            .mov (as::mem(as::rsi + &THREAD->groups[op->arg * 2 + 1]), as::ecx)
                            .mov (as::mem(as::rsi + &THREAD->groups[op->arg * 2]),     as::esi)
                            .cmp (as::i32(-1), as::ecx).jmp(fail, as::equal)
                            .sub (as::esi,     as::ecx).jmp(fail, as::less_u)
                            .jmp (labels[op->out], as::equal)
                        // if (nfa->length < end - start) return;
                            .cmp(as::ecx, as::mem(as::rdi + &NFA->length)).jmp(fail, as::less_u)
                        // if (memcmp(nfa->input, nfa->input + start - nfa->offset, end - start)) return;
                            .push(as::rdi)
                            .sub (as::mem(as::rdi + &NFA->offset), as::esi).movsl(as::esi, as::rsi)
                            .mov (as::mem(as::rdi + &NFA->input),  as::rdi)
                            .add (as::rdi, as::rsi)
                            .mov (as::ecx, as::edx)
                            .repz().cmpsb().pop(as::rdi).jmp(fail, as::not_equal)
                        // return rejit_thread_wait(nfa, &out, end - start);
                            .mov (labels[op->out], as::rsi)
                            .jmp (&rejit_thread_wait);
                        VISIT(op->out);
                        break;
                }
            }

            if (!ext.size()) switch (op->opcode()) {
                case re2::kInstAltMatch:
                case re2::kInstAlt:
                    // if (out(nfa)) return 1;
                    code.push  (as::rdi)
                        .call  (labels[op->out()])
                        .pop   (as::rdi)
                        .test  (as::eax, as::eax).jmp(succeed, as::not_zero);
                    VISIT(op->out());
                    VISIT(op->out1()); else code.jmp(labels[op->out1()]);
                    break;

                case re2::kInstByteRange: {
                    auto i = 0, len = 0, r = 0;
                    auto end = op;

                    do
                        len++, end = prog->inst(r = end->out());
                    while (end->opcode() == re2::kInstByteRange && !re2jit::is_extcode(prog, end));

                    // if (nfa->length < len) return; else rsi = nfa->input;
                    code.cmp(as::i32(len), as::mem(as::rdi + &NFA->length)).jmp(fail, as::less_u)
                        .mov(as::mem(as::rdi + &NFA->input), as::rsi);

                    do {
                        // al = rsi[i];
                        code.movzb(as::mem(as::rsi + i++), as::eax);

                        if (op->foldcase())
                            // if ('A' <= al && al <= 'Z') al = al - 'A' + 'a';
                            code.mov(as::rax - 'A' + 'a', as::edx)
                                .mov(as::rax - 'A',       as::ecx)
                                .cmp('Z' - 'A', as::cl)
                                .mov(as::edx, as::eax, as::less_equal_u);

                        if (op->hi() == op->lo())
                            // if (al != lo) return;
                            code.cmp(op->lo(), as::al).jmp(fail, as::not_equal);
                        else
                            // if (al < lo || hi < al) return;
                            code.sub(op->lo(),            as::al)
                                .cmp(op->hi() - op->lo(), as::al).jmp(fail, as::more_u);

                        op = prog->inst(op->out());
                    } while (op != end);

                    // return rejit_thread_wait(nfa, &out, len);
                    code.mov(labels[r], as::rsi)
                        .mov(len,       as::edx)
                        .jmp(&rejit_thread_wait);
                    VISIT(r);
                    break;
                }

                case re2::kInstCapture:
                    // if (nfa->groups <= cap) goto out;
                    code.cmp  (as::i32(op->cap()), as::mem(as::rdi + &NFA->groups))
                        .jmp  (labels[op->out()], as::less_equal_u)
                    // if ((edx = nfa->running->groups[cap]) == nfa->offset) goto out;
                        .mov  (as::mem(as::rdi + &NFA->running), as::rcx)
                        .mov  (as::mem(as::rdi + &NFA->offset),  as::eax)
                        .mov  (as::mem(as::rcx + &THREAD->groups[op->cap()]), as::edx)
                        .cmp  (as::eax, as::edx).jmp(labels[op->out()], as::equal)
                    // nfa->running->groups[cap] = nfa->offset;
                        .push (as::rdx)
                        .push (as::rcx)
                        .mov  (as::eax, as::mem(as::rcx + &THREAD->groups[op->cap()]));

                    if (backrefs.find(op->cap() / 2) != backrefs.end())
                        code.push(as::rdi).call(&rejit_thread_bitmap_save)   .pop(as::rdi)
                            .push(as::rdi).call(labels[op->out()])           .pop(as::rdi)
                            .push(as::rax).call(&rejit_thread_bitmap_restore).pop(as::rax);
                    else
                        code.call(labels[op->out()]);

                    // nfa->running->groups[cap] = edx; return eax;
                    code.pop(as::rcx)
                        .pop(as::rdx)
                        .mov(as::edx, as::mem(as::rcx + &THREAD->groups[op->cap()]))
                        .ret();
                    VISIT(op->out());
                    break;

                case re2::kInstEmptyWidth:
                    // if (!rejit_thread_satisfies(nfa, empty)) return;
                    code.push(as::rdi)
                        .mov (as::i32(op->empty()), as::esi)
                        .call(&rejit_thread_satisfies)
                        .pop (as::rdi)
                        .test(as::eax, as::eax)
                        .jmp (fail, as::zero);

                case re2::kInstNop:
                    VISIT(op->out()); else code.jmp(labels[op->out()]);
                    break;

                case re2::kInstMatch:
                    code.jmp(&rejit_thread_match);
                    break;

                case re2::kInstFail:
                    code.ret();
                    break;
            }
        }

        code.mark(fail).xor_(as::eax, as::eax).mark(succeed).ret();
        #undef VISIT
        #undef DFS

        void *m = mmap(NULL, code.size(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (m == MAP_FAILED)
            return;

        _size = code.size();

        if (!code.write(m) || mprotect(m, _size, PROT_READ | PROT_EXEC) == -1) {
            munmap(m, _size);
            return;
        }

        state = m;
        space = (space + 7) / 8;  // bits -> bytes
    }

   ~native()
    {
        if (state) munmap((void *) state, _size);
    }

    static void entry(struct rejit_threadset_t *nfa, const void *f)
    {
        return ((void (*)(struct rejit_threadset_t *)) f)(nfa);
    }
};
