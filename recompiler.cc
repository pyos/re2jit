#include "re2/prog.h"
#include "recompiler.h"


using namespace re2jit;


Recompiler::Recompiler()
{
    // TODO init with an always-failing program
}


Recompiler::Recompiler(const re2::Prog& prog)
{
    *this = prog;
}


Recompiler&
Recompiler::operator = (const re2::Prog& prog)
{
    // TODO actually compile
    return *this;
}


Recompiler::~Recompiler()
{
}


Recompiler::Status
Recompiler::Run(const re2::StringPiece& text, RE2::Anchor anchor,
                      re2::StringPiece *match, int nmatch) const
{
    return Recompiler::ERR_NO_JIT;
}
