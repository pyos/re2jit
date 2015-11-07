struct re2jit::native
{
    re2::Prog *_prog;

    native(re2::Prog *prog) : _prog(prog) {}

    bool match(const re2::StringPiece &text, int flags,
                     re2::StringPiece *groups, int ngroups)
    {
        // _prog->start() -- откуда начинать
        // _prog->size() -- сколько всего инструкций
        ssize_t i = _prog->start();

        // text.data() -- указатель на начало
        // text.size() -- ...

        // flags -- побитовое "или"
        //   RE2JIT_ANCHOR_START  --  совпадения должны быть в начале `text`
        //   RE2JIT_ANCHOR_END    --  совпадения должны быть в конце `text`
        // (соответственно, если выставлены оба, совпадением может быть только весь текст)

        // groups - куда записывать содержимое подгрупп, если совпадение нашлось
        // groups[i].set(указатель на начало группы, длина);
        // ngroups - размер `groups`.

        // нужно реализовать эту функцию.
        auto anchor = re2::Prog::kAnchored;
        auto kind   = re2::Prog::kFirstMatch;

        if (!(flags & RE2JIT_ANCHOR_START))
            anchor = re2::Prog::kUnanchored;
        else if (flags & RE2JIT_ANCHOR_END)
            kind = re2::Prog::kFullMatch;

        return _prog->SearchNFA(text, text, anchor, kind, groups, ngroups);;


        while (1) {
            re2::Prog::Inst *op = _prog->inst(i);

            switch (op->opcode())
            {
                case re2::kInstAltMatch:
                case re2::kInstAlt:
                    // ветвление на два состояния сразу
                    // op->out()  -- либо сюда
                    // op->out1() -- либо сюда, но out() лучше

                case re2::kInstByteRange:
                    // нужно прочитать байт и проверить, что он в опередленном диапазоне
                    // op->foldcase() -- если true, то A..Z нужно преобразить в a..z
                    // op->lo() -- нижняя граница
                    // op->hi() -- верхняя граница
                    // op->out() -- если символ заматчился, продолжать отсюда

                case re2::kInstCapture:
                    // нужно записать, что здесь начинается/заканчивается группа
                    // op->cap() -- 2 * n для n-й открывающей скобки
                    //              2 * n + 1 для закрывающей
                    // op->out() -- когда отметили, переходим сюда

                case re2::kInstEmptyWidth:
                    // нужно проверить флаги состояния
                    // op->empty() -- какие должны быть выставлены
                    //                это побитовое "или" констант:
                    //     re2::kEmptyBeginLine
                    //     re2::kEmptyEndLine
                    //     re2::kEmptyBeginText
                    //     re2::kEmptyEndText
                    //     re2::kEmptyWordBoundary
                    //     re2::kEmptyNonWordBoundary
                    // op->out() -- если все верно, идем сюда

                case re2::kInstNop:
                    // просто переход на op->out()

                case re2::kInstMatch:
                    // return true;

                case re2::kInstFail:
                    // не получилось, пробуем другую ветку
                    break;
            }
        }

        return 0;
    }
};
