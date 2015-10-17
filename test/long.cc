#define GROUP_TEST(regex, anchor, input, n) \
    test_case("group equality") { \
        re2::StringPiece g2[n]; \
        re2::StringPiece gj[n]; \
        if ( _RE2_RUN(_RE2(regex), input, anchor, g2, n) \
          != _R2J_RUN(_R2J(regex), input, anchor, gj, n)) return Result::Fail("invalid answer"); \
        for (size_t i = 0; i < n; i++) { \
            if (g2[i] != gj[i]) { \
                return Result::Fail( \
                    "group %zu incorrect\n" \
                    "    expected [%d] '%.*s'\n" \
                    "    matched  [%d] '%.*s'", i, g2[i].size(), 0, g2[i].data() + g2[i].size() - 20, \
                                                   gj[i].size(), 20, gj[i].data() + gj[i].size() - 20); \
            } \
        } \
    }


#define PERF_TEST(n, regex, anchor, input, ngroups) \
  test_case("re2    ") { _RE2 r(regex); re2::StringPiece s[ngroups]; \
      MEASURE(n, _RE2_RUN(r, input, anchor, s, ngroups)); } \
  test_case("re2jit ") { _R2J r(regex); re2::StringPiece s[ngroups]; \
      MEASURE(n, _R2J_RUN(r, input, anchor, s, ngroups)); }


GROUP_TEST("(.|ab|cd)+"
         , ANCHOR_BOTH
         , "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbccccccccccccccccccccccccccccccccddddddddddddddddddddddddddddddddddeeeeeeeeeeeeeeeeeeeeeeeeeeeeffffffffffffffffffffffffffffggggggggggggggggggggggggggggghhhhhhhhhhhhhhhhhhhiiiiiiiiiiiiiiiiiijjjjjjjjjjjjjjjjjjjjjkkkkkkkkkkkkkkkkkkk"
         , 2);

PERF_TEST(10000, "(.|ab|cd)+"
         , ANCHOR_BOTH
         , "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbccccccccccccccccccccccccccccccccddddddddddddddddddddddddddddddddddeeeeeeeeeeeeeeeeeeeeeeeeeeeeffffffffffffffffffffffffffffggggggggggggggggggggggggggggghhhhhhhhhhhhhhhhhhhiiiiiiiiiiiiiiiiiijjjjjjjjjjjjjjjjjjjjjkkkkkkkkkkkkkkkkkkk"
         , 2);

GROUP_TEST("(?is)(?:(?P<skip>[^\\S\\n]+|\\s*\\#(?::(?P<docstr>[^\\n]*)|[^\\n]*))|(?P<number>[+-]?(?:(?P<isbin>0b)[01]+|(?P<isoct>0o)[0-7]+|(?P<ishex>0x)[0-9a-f]+|[0-9]+(?P<isfloat>(?:\\.[0-9]+)?(?:e[+-]?[0-9]+)?)(?P<isimag>j)?))|(?P<string>(?:br|r?b?)(?:'{3}(?:[^\\\\]|\\\\.)*?'{3}|\"{3}(?:[^\\\\]|\\\\.)*?\"{3}|'(?:[^\\\\]|\\\\.)*?'|\"(?:[^\\\\]|\\\\.)*?\"))|(?P<name>[\\p{L}\\p{N}_]+'*|\\*+:)|(?P<infix>[!$%&*+\\--/:<-@\\\\^|~;]+|,)|(?P<eol>\\s*\\n(?P<indent>[\\ \\t]*))|(?P<block>[\\(\\[])|(?P<end>[\\)\\]]|$)|(?P<iname>`(?P<iname_>\\w+'*)`))+"
         , ANCHOR_BOTH
         , "Юникод тоже проверить надо, наверное.  # should match"
         , 30);

PERF_TEST(200
         , "(?is)(?:(?P<skip>[^\\S\\n]+|\\s*\\#(?::(?P<docstr>[^\\n]*)|[^\\n]*))|(?P<number>[+-]?(?:(?P<isbin>0b)[01]+|(?P<isoct>0o)[0-7]+|(?P<ishex>0x)[0-9a-f]+|[0-9]+(?P<isfloat>(?:\\.[0-9]+)?(?:e[+-]?[0-9]+)?)(?P<isimag>j)?))|(?P<string>(?:br|r?b?)(?:'{3}(?:[^\\\\]|\\\\.)*?'{3}|\"{3}(?:[^\\\\]|\\\\.)*?\"{3}|'(?:[^\\\\]|\\\\.)*?'|\"(?:[^\\\\]|\\\\.)*?\"))|(?P<name>[\\p{L}\\p{N}_]+'*|\\*+:)|(?P<infix>[!$%&*+\\--/:<-@\\\\^|~;]+|,)|(?P<eol>\\s*\\n(?P<indent>[\\ \\t]*))|(?P<block>[\\(\\[])|(?P<end>[\\)\\]]|$)|(?P<iname>`(?P<iname_>\\w+'*)`))+"
         , ANCHOR_BOTH
         , "Юникод тоже проверить надо, наверное.  # should match"
         , 30);

GROUP_TEST("(?is)(?:(?P<skip>[^\\S\\n]+|\\s*\\#(?::(?P<docstr>[^\\n]*)|[^\\n]*))|(?P<number>[+-]?(?:(?P<isbin>0b)[01]+|(?P<isoct>0o)[0-7]+|(?P<ishex>0x)[0-9a-f]+|[0-9]+(?P<isfloat>(?:\\.[0-9]+)?(?:e[+-]?[0-9]+)?)(?P<isimag>j)?))|(?P<string>(?:br|r?b?)(?:'{3}(?:[^\\\\]|\\\\.)*?'{3}|\"{3}(?:[^\\\\]|\\\\.)*?\"{3}|'(?:[^\\\\]|\\\\.)*?'|\"(?:[^\\\\]|\\\\.)*?\"))|(?P<name>[\\p{L}\\p{N}_]+'*|\\*+:)|(?P<infix>[!$%&*+\\--/:<-@\\\\^|~;]+|,)|(?P<eol>\\s*\\n(?P<indent>[\\ \\t]*))|(?P<block>[\\(\\[])|(?P<end>[\\)\\]]|$)|(?P<iname>`(?P<iname_>\\w+'*)`))+"
         , ANCHOR_BOTH
         , "import '/numpy/array'\nimport '/numpy/dot'; .* = dot\narray [[0, 1], [1, 0]] .* array [[0, 1], [1, 0]] |> print  # @ не нужен"
         , 30);

PERF_TEST(100
         , "(?is)(?:(?P<skip>[^\\S\\n]+|\\s*\\#(?::(?P<docstr>[^\\n]*)|[^\\n]*))|(?P<number>[+-]?(?:(?P<isbin>0b)[01]+|(?P<isoct>0o)[0-7]+|(?P<ishex>0x)[0-9a-f]+|[0-9]+(?P<isfloat>(?:\\.[0-9]+)?(?:e[+-]?[0-9]+)?)(?P<isimag>j)?))|(?P<string>(?:br|r?b?)(?:'{3}(?:[^\\\\]|\\\\.)*?'{3}|\"{3}(?:[^\\\\]|\\\\.)*?\"{3}|'(?:[^\\\\]|\\\\.)*?'|\"(?:[^\\\\]|\\\\.)*?\"))|(?P<name>[\\p{L}\\p{N}_]+'*|\\*+:)|(?P<infix>[!$%&*+\\--/:<-@\\\\^|~;]+|,)|(?P<eol>\\s*\\n(?P<indent>[\\ \\t]*))|(?P<block>[\\(\\[])|(?P<end>[\\)\\]]|$)|(?P<iname>`(?P<iname_>\\w+'*)`))+"
         , ANCHOR_BOTH
         , "import '/numpy/array'\nimport '/numpy/dot'; .* = dot\narray [[0, 1], [1, 0]] .* array [[0, 1], [1, 0]] |> print  # @ не нужен"
         , 30);

GROUP_TEST("(?is)(?:(?P<skip>[^\\S\\n]+|\\s*\\#(?::(?P<docstr>[^\\n]*)|[^\\n]*))|(?P<number>[+-]?(?:(?P<isbin>0b)[01]+|(?P<isoct>0o)[0-7]+|(?P<ishex>0x)[0-9a-f]+|[0-9]+(?P<isfloat>(?:\\.[0-9]+)?(?:e[+-]?[0-9]+)?)(?P<isimag>j)?))|(?P<string>(?:br|r?b?)(?:'{3}(?:[^\\\\]|\\\\.)*?'{3}|\"{3}(?:[^\\\\]|\\\\.)*?\"{3}|'(?:[^\\\\]|\\\\.)*?'|\"(?:[^\\\\]|\\\\.)*?\"))|(?P<name>[\\p{L}\\p{N}_]+'*|\\*+:)|(?P<infix>[!$%&*+\\--/:<-@\\\\^|~;]+|,)|(?P<eol>\\s*\\n(?P<indent>[\\ \\t]*))|(?P<block>[\\(\\[])|(?P<end>[\\)\\]]|$)|(?P<iname>`(?P<iname_>\\w+'*)`))+"
         , ANCHOR_BOTH
         , "import '/types'\nimport '/opcode'\nimport '/struct'\nimport '/collections'\n\n# Allow cross-compiling for CPython 3.5 on CPython 3.4:\nopcode.opmap.setdefault 'WITH_CLEANUP_START'  81\nopcode.opmap.setdefault 'WITH_CLEANUP_FINISH' 82\n\n\n#: Calculate the length of a bytecode sequence given (opcode, argument) pairs, in bytes.\n#:\n#: codelen :: [(int, int)] -> int\n#:\ncodelen = seq -> sum\n  where for (c, v) in seq => yield $ if\n    c < opcode.HAVE_ARGUMENT => 1\n    otherwise                => 3 * (1 + abs (v.bit_length! - 1) // 16)\n\n\nJump = subclass object where\n  #: An argument to a jump opcode.\n  #:\n  #: code     :: CodeType  -- bytecode to insert a jump into.\n  #: start    :: int       -- offset at which the jump object was created.\n  #: op       :: str       -- instruction to insert.\n  #: relative :: bool      -- whether to start counting from `start`.\n  #: reverse  :: bool      -- ask questions first, insert later. Implies `absolute`.\n  #:\n  __init__ = @code @reverse @op delta ~>\n    @relative = @op == 'JUMP_FORWARD' or @op == 'FOR_ITER' or @op.startswith 'SETUP'\n    @start    = len @code.bytecode\n    @value    = None\n    @code.depth delta\n\n    if @reverse  => @relative => raise $ SystemError 'cannot make reverse relative jumps'\n       otherwise => @code.append @op 0\n    None\n\n  __enter__ = self        -> self\n  __exit__  = self t v tv -> @set => False\n\n  #: Set the target of a forward jump. Insert the opcode of a reverse jump.\n  #:\n  #: set :: a\n  #:\n  set = ~>\n    @value is None =>\n      @value = i = 0\n      not @relative => @value += codelen $ take  @start      @code.bytecode\n      not @reverse  => @value += codelen $ drop (@start + 1) @code.bytecode\n      not @reverse and not @relative =>\n        # This jump needs to account for itself.\n        while @value >> i => @value, i = @value + 3, i + 16\n\n    if @reverse  => @code.append @op @value\n       otherwise => @code.bytecode !! @start = opcode.opmap !! @op, @value\n\n\nCodeType = subclass object where\n  #: A mutable version of `types.CodeType`.\n  #:\n  #: cell      :: Maybe CodeType -- a parent code object.\n  #: argc      :: int\n  #: kwargc    :: int\n  #: varargs   :: bool -- accepts more than `argc` arguments.\n  #: varkws    :: bool -- accepts keyword arguments not in `varnames[argc:][:kwargc]`.\n  #: function  :: bool -- is a function, not a module.\n  #: generator :: bool -- is a function with `yield`.\n  #: name      :: str\n  #: qualname  :: str\n  #: docstring :: str\n  #:\n  #: var        :: Maybe str -- a string representing the innermost assignment.\n  #: fastlocals :: dict (dict int int) -- maps names from `varnames` to opcode locations.\n  #: consts     :: dict (object, type) int\n  #: varnames   :: dict str int -- array-stored arguments.\n  #: names      :: dict str int -- attributes, globals & module names.\n  #: cellvars   :: dict str int -- local variables used by closures.\n  #: freevars   :: dict str int -- non-local variables.\n  #: enclosed   :: set str -- names that may be added to `freevars`.\n  #:\n  #: bytecode  :: [(int, int)] -- (opcode, argument) pairs.\n  #: stacksize :: int -- minimum stack depth required for evaluation.\n  #: currstack :: int -- approx. stack depth at this point.\n  #:\n  #: filename :: str\n  #: lineno   :: int\n  #: lnotab   :: bytes\n  #: lineoff  :: int -- `lineno` last time `lnotab` was updated.\n  #: byteoff  :: int -- `len bytecode` at the same point.\n  #:\n  __init__ = name a: tuple! kw: tuple! va: tuple! vkw: tuple! cell: None function: False doc: None ~>\n    @cell      = cell\n    @argc      = len a\n    @kwargc    = len kw\n    @varargs   = bool va\n    @varkws    = bool vkw\n    @function  = bool function\n    @generator = False  # only becomes known during generation\n    @coroutine = False\n    @name      = str name\n    @docstring = doc\n    @qualname  = ''\n    cell => cell.qualname => @qualname += cell.qualname + '.'\n    cell => cell.function => @qualname += '<locals>.'\n    function => @qualname += name\n\n    @var        = None\n    @fastlocals = collections.defaultdict dict\n    @consts     = collections.defaultdict $ -> len @consts\n    @varnames   = collections.defaultdict $ -> len @varnames\n    @names      = collections.defaultdict $ -> len @names\n    @cellvars   = collections.defaultdict $ -> len @cellvars\n    @freevars   = collections.defaultdict $ -> -1 - len @freevars\n    @enclosed   = if\n      cell      => dict.keys cell.varnames | cell.cellvars | cell.enclosed\n      otherwise => set!\n    @globals    = if\n      cell      => cell.globals\n      otherwise => set!\n    for v in itertools.chain a kw va vkw => @varnames !! v\n    # First constant in a code object is always its docstring.\n    # Except if this is a class/module, in which case an additional manual\n    # assignment to `__doc__` is necessary.\n    @consts !! (doc, type doc)\n\n    @bytecode  = []\n    @stacksize = 0\n    @currstack = 0\n\n    @filename = '<generated>'\n    @lineno   = 1\n    @lnotab   = b''\n    @lineoff  = -1\n    @byteoff  = 0\n    None\n\n  #: These constants, unless redefined, be loaded with LOAD_CONST, not LOAD_GLOBAL.\n  constnames = dict True: True False: False None: None otherwise: True (...): Ellipsis\n\n  #: Make the bytecode slightly faster. Only works on CPython, because it has\n  #: a built-in peephole optimizer and writing a new one is hard. PyPy uses\n  #: an AST-based optimizer instead, and we can't use that for obvious reasons.\n  #: The arguments are: bytecode, constants, names, lnotab.\n  #: Constants are passed as a list to allow further additions.\n  #:\n  #: optimize :: Maybe (bytes list tuple bytes -> bytes)\n  #:\n  optimize = if PY_TAG.startswith 'cpython-' => fn where\n    import '/ctypes/pythonapi'\n    import '/ctypes/py_object'\n    fn = pythonapi.PyCode_Optimize\n    fn.restype  = py_object\n    fn.argtypes = py_object, py_object, py_object, py_object\n\n  #: Calculated value of CodeType.co_flags.\n  #:\n  #: flags :: int\n  #:\n  flags = ~>\n    f = 0\n    # 0x1 = CO_OPTIMIZED -- do not create `locals()` at all, use an array instead\n    # 0x2 = CO_NEWLOCALS -- do not set `locals()` to the same value as `globals()`\n    @function => f |= 0x3\n    @varargs  => f |= 0x4\n    @varkws   => f |= 0x8\n    # 0x10 = CO_NESTED -- set iff @freevars not empty; an obsolete `__future__` flag.\n    not $ @cellvars or @freevars => f |= 0x40\n    @generator => f |= 0x20\n    @coroutine => f |= 0xA0  # every coroutine is a generator\n    # 0x100 = CO_ITERATORCOROUTINE; set by `asyncio.coroutine`.\n    # Flags >= 0x1000 are reserved for `__future__` imports. We don't have those.\n    f\n\n  #: Generate a sequence of bytes for an opcode with an argument.\n  #:\n  #: code :: (int, int) -> bytes\n  #:\n  code = (op, arg) ~> if\n    op  < opcode.HAVE_ARGUMENT => struct.pack '<B'  op\n    arg < 0                    => @code (op, len @cellvars - arg - 1)\n    arg < 0x10000              => struct.pack '<BH' op arg\n    otherwise                  => @code (opcode.opmap !! 'EXTENDED_ARG', arg >> 16) +\n                                  struct.pack '<BH' op (arg & 0xffff)\n\n  #: Convert this object into an immutable version actually suitable for use with `eval`.\n  #:\n  #: frozen :: types.CodeType\n  #:\n  frozen = ~>\n    code     = b''.join $ map @code @bytecode\n    consts   = list  $ map fst $ sorted @consts key: @consts.__getitem__\n    names    = tuple $ sorted @names    key: @names.__getitem__\n    varnames = tuple $ sorted @varnames key: @varnames.__getitem__\n    cellvars = tuple $ sorted @cellvars key: @cellvars.__getitem__\n    freevars = tuple $ sorted @freevars key: @freevars.__getitem__ reverse: True\n\n    if @optimize =>\n      # Most of the functions are unaffected by the first run, but some\n      # may benefit from two. `PyCode_Optimize` is fast, so why not?\n      code = @optimize code consts names @lnotab\n      code = @optimize code consts names @lnotab\n\n    types.CodeType @argc @kwargc (len varnames) @stacksize @flags code (tuple consts) names\n      varnames\n      @filename\n      @name\n      @lineno\n      @lnotab\n      freevars\n      cellvars\n\n  #: Append a new opcode to the sequence.\n  #:\n  #: append :: str (Optional int) (Optional int) -> a\n  #:\n  append = name arg: 0 delta: 0 ~>\n    @depth delta\n    # These indices are used to quickly change all references\n    # to an array slot into references to a cell.\n    name == 'LOAD_FAST'  => @fastlocals !! arg !! len @bytecode = opcode.opmap !! 'LOAD_DEREF'\n    name == 'STORE_FAST' => @fastlocals !! arg !! len @bytecode = opcode.opmap !! 'STORE_DEREF'\n    @bytecode.append (opcode.opmap !! name, arg)\n\n  #: Request a permanent change in stack size.\n  #:\n  #: depth :: int -> ()\n  #:\n  depth = x ~>\n    # Python calculates the stack depth by scanning bytecode.\n    # We'll opt for traversing the AST instead.\n    @currstack += x\n    @currstack > @stacksize => @stacksize = @currstack\n\n  #: Push `x` onto the value stack.\n  #:\n  #: Technically, `x` can be anything, but most types would make\n  #: the code object unmarshallable.\n  #:\n  #: pushconst :: object -> a\n  #:\n  pushconst = x ~> @append 'LOAD_CONST' delta: +1 $ @consts !! (x, type x)\n\n  #: Push the value assigned to some name onto the value stack.\n  #:\n  #: pushname :: str -> a\n  #:\n  pushname = v ~> if\n    v in @cellvars   => @append 'LOAD_DEREF'  delta: +1 $ @cellvars !! v\n    v in @varnames   => @append 'LOAD_FAST'   delta: +1 $ @varnames !! v\n    v in @enclosed   => @append 'LOAD_DEREF'  delta: +1 $ @freevars !! v\n    v in @globals    => @append 'LOAD_GLOBAL' delta: +1 $ @names !! v\n    v in @constnames => @pushconst $ @constnames !! v\n    otherwise        => @append 'LOAD_GLOBAL' delta: +1 $ @names !! v\n\n  #: Pop the value from the top of the stack, assign it to a name.\n  #:\n  #: popname :: str -> a\n  #:\n  popname = v ~> if\n    v in @cellvars => @append 'STORE_DEREF'  delta: -1 $ @cellvars !! v\n    v in @varnames => @append 'STORE_FAST'   delta: -1 $ @varnames !! v\n    v in @enclosed => @append 'STORE_DEREF'  delta: -1 $ @freevars !! v\n    otherwise      => @append 'STORE_GLOBAL' delta: -1 $ @names    !! v\n\n  #: Load cell objects referencing some names. Used to create closures.\n  #:\n  #: pushcells :: [str] -> a\n  #:\n  pushcells = vs ~> for v in vs =>\n    if v in @varnames => for i in @fastlocals !! (@varnames !! v) =>\n      # All previously inserted `*_FAST` references to that name should be\n      # changed to `*_DEREF` to keep the cell contents up-to-date.\n      @bytecode !! i = @fastlocals !! (@varnames !! v) !! i, @cellvars !! v\n\n    @append 'LOAD_CLOSURE' delta: +1 $ if\n      v in @cellvars => @cellvars !! v\n      v in @varnames => @cellvars !! v\n      otherwise      => @freevars !! v\n\n  #: Insert a jump clause.\n  #:\n  #: jump :: str (Optional bool) (Optional int) -> Jump\n  #:\n  jump = opname reverse: False delta: 0 ~> Jump self reverse opname delta\n\n  #: Make a child code object.\n  #:\n  #: spawn :: str * ** -> CodeType\n  #:\n  spawn = name *: args **: kwargs ~> @__class__ cell: self function: True *: args **: kwargs $ if\n    @var is None       => name\n    @var.isidentifier! => @var\n    otherwise          => '(' + @var + ')'\n"
         , 30);

PERF_TEST(5
         , "(?is)(?:(?P<skip>[^\\S\\n]+|\\s*\\#(?::(?P<docstr>[^\\n]*)|[^\\n]*))|(?P<number>[+-]?(?:(?P<isbin>0b)[01]+|(?P<isoct>0o)[0-7]+|(?P<ishex>0x)[0-9a-f]+|[0-9]+(?P<isfloat>(?:\\.[0-9]+)?(?:e[+-]?[0-9]+)?)(?P<isimag>j)?))|(?P<string>(?:br|r?b?)(?:'{3}(?:[^\\\\]|\\\\.)*?'{3}|\"{3}(?:[^\\\\]|\\\\.)*?\"{3}|'(?:[^\\\\]|\\\\.)*?'|\"(?:[^\\\\]|\\\\.)*?\"))|(?P<name>[\\p{L}\\p{N}_]+'*|\\*+:)|(?P<infix>[!$%&*+\\--/:<-@\\\\^|~;]+|,)|(?P<eol>\\s*\\n(?P<indent>[\\ \\t]*))|(?P<block>[\\(\\[])|(?P<end>[\\)\\]]|$)|(?P<iname>`(?P<iname_>\\w+'*)`))+"
         , ANCHOR_BOTH
         , "import '/types'\nimport '/opcode'\nimport '/struct'\nimport '/collections'\n\n# Allow cross-compiling for CPython 3.5 on CPython 3.4:\nopcode.opmap.setdefault 'WITH_CLEANUP_START'  81\nopcode.opmap.setdefault 'WITH_CLEANUP_FINISH' 82\n\n\n#: Calculate the length of a bytecode sequence given (opcode, argument) pairs, in bytes.\n#:\n#: codelen :: [(int, int)] -> int\n#:\ncodelen = seq -> sum\n  where for (c, v) in seq => yield $ if\n    c < opcode.HAVE_ARGUMENT => 1\n    otherwise                => 3 * (1 + abs (v.bit_length! - 1) // 16)\n\n\nJump = subclass object where\n  #: An argument to a jump opcode.\n  #:\n  #: code     :: CodeType  -- bytecode to insert a jump into.\n  #: start    :: int       -- offset at which the jump object was created.\n  #: op       :: str       -- instruction to insert.\n  #: relative :: bool      -- whether to start counting from `start`.\n  #: reverse  :: bool      -- ask questions first, insert later. Implies `absolute`.\n  #:\n  __init__ = @code @reverse @op delta ~>\n    @relative = @op == 'JUMP_FORWARD' or @op == 'FOR_ITER' or @op.startswith 'SETUP'\n    @start    = len @code.bytecode\n    @value    = None\n    @code.depth delta\n\n    if @reverse  => @relative => raise $ SystemError 'cannot make reverse relative jumps'\n       otherwise => @code.append @op 0\n    None\n\n  __enter__ = self        -> self\n  __exit__  = self t v tv -> @set => False\n\n  #: Set the target of a forward jump. Insert the opcode of a reverse jump.\n  #:\n  #: set :: a\n  #:\n  set = ~>\n    @value is None =>\n      @value = i = 0\n      not @relative => @value += codelen $ take  @start      @code.bytecode\n      not @reverse  => @value += codelen $ drop (@start + 1) @code.bytecode\n      not @reverse and not @relative =>\n        # This jump needs to account for itself.\n        while @value >> i => @value, i = @value + 3, i + 16\n\n    if @reverse  => @code.append @op @value\n       otherwise => @code.bytecode !! @start = opcode.opmap !! @op, @value\n\n\nCodeType = subclass object where\n  #: A mutable version of `types.CodeType`.\n  #:\n  #: cell      :: Maybe CodeType -- a parent code object.\n  #: argc      :: int\n  #: kwargc    :: int\n  #: varargs   :: bool -- accepts more than `argc` arguments.\n  #: varkws    :: bool -- accepts keyword arguments not in `varnames[argc:][:kwargc]`.\n  #: function  :: bool -- is a function, not a module.\n  #: generator :: bool -- is a function with `yield`.\n  #: name      :: str\n  #: qualname  :: str\n  #: docstring :: str\n  #:\n  #: var        :: Maybe str -- a string representing the innermost assignment.\n  #: fastlocals :: dict (dict int int) -- maps names from `varnames` to opcode locations.\n  #: consts     :: dict (object, type) int\n  #: varnames   :: dict str int -- array-stored arguments.\n  #: names      :: dict str int -- attributes, globals & module names.\n  #: cellvars   :: dict str int -- local variables used by closures.\n  #: freevars   :: dict str int -- non-local variables.\n  #: enclosed   :: set str -- names that may be added to `freevars`.\n  #:\n  #: bytecode  :: [(int, int)] -- (opcode, argument) pairs.\n  #: stacksize :: int -- minimum stack depth required for evaluation.\n  #: currstack :: int -- approx. stack depth at this point.\n  #:\n  #: filename :: str\n  #: lineno   :: int\n  #: lnotab   :: bytes\n  #: lineoff  :: int -- `lineno` last time `lnotab` was updated.\n  #: byteoff  :: int -- `len bytecode` at the same point.\n  #:\n  __init__ = name a: tuple! kw: tuple! va: tuple! vkw: tuple! cell: None function: False doc: None ~>\n    @cell      = cell\n    @argc      = len a\n    @kwargc    = len kw\n    @varargs   = bool va\n    @varkws    = bool vkw\n    @function  = bool function\n    @generator = False  # only becomes known during generation\n    @coroutine = False\n    @name      = str name\n    @docstring = doc\n    @qualname  = ''\n    cell => cell.qualname => @qualname += cell.qualname + '.'\n    cell => cell.function => @qualname += '<locals>.'\n    function => @qualname += name\n\n    @var        = None\n    @fastlocals = collections.defaultdict dict\n    @consts     = collections.defaultdict $ -> len @consts\n    @varnames   = collections.defaultdict $ -> len @varnames\n    @names      = collections.defaultdict $ -> len @names\n    @cellvars   = collections.defaultdict $ -> len @cellvars\n    @freevars   = collections.defaultdict $ -> -1 - len @freevars\n    @enclosed   = if\n      cell      => dict.keys cell.varnames | cell.cellvars | cell.enclosed\n      otherwise => set!\n    @globals    = if\n      cell      => cell.globals\n      otherwise => set!\n    for v in itertools.chain a kw va vkw => @varnames !! v\n    # First constant in a code object is always its docstring.\n    # Except if this is a class/module, in which case an additional manual\n    # assignment to `__doc__` is necessary.\n    @consts !! (doc, type doc)\n\n    @bytecode  = []\n    @stacksize = 0\n    @currstack = 0\n\n    @filename = '<generated>'\n    @lineno   = 1\n    @lnotab   = b''\n    @lineoff  = -1\n    @byteoff  = 0\n    None\n\n  #: These constants, unless redefined, be loaded with LOAD_CONST, not LOAD_GLOBAL.\n  constnames = dict True: True False: False None: None otherwise: True (...): Ellipsis\n\n  #: Make the bytecode slightly faster. Only works on CPython, because it has\n  #: a built-in peephole optimizer and writing a new one is hard. PyPy uses\n  #: an AST-based optimizer instead, and we can't use that for obvious reasons.\n  #: The arguments are: bytecode, constants, names, lnotab.\n  #: Constants are passed as a list to allow further additions.\n  #:\n  #: optimize :: Maybe (bytes list tuple bytes -> bytes)\n  #:\n  optimize = if PY_TAG.startswith 'cpython-' => fn where\n    import '/ctypes/pythonapi'\n    import '/ctypes/py_object'\n    fn = pythonapi.PyCode_Optimize\n    fn.restype  = py_object\n    fn.argtypes = py_object, py_object, py_object, py_object\n\n  #: Calculated value of CodeType.co_flags.\n  #:\n  #: flags :: int\n  #:\n  flags = ~>\n    f = 0\n    # 0x1 = CO_OPTIMIZED -- do not create `locals()` at all, use an array instead\n    # 0x2 = CO_NEWLOCALS -- do not set `locals()` to the same value as `globals()`\n    @function => f |= 0x3\n    @varargs  => f |= 0x4\n    @varkws   => f |= 0x8\n    # 0x10 = CO_NESTED -- set iff @freevars not empty; an obsolete `__future__` flag.\n    not $ @cellvars or @freevars => f |= 0x40\n    @generator => f |= 0x20\n    @coroutine => f |= 0xA0  # every coroutine is a generator\n    # 0x100 = CO_ITERATORCOROUTINE; set by `asyncio.coroutine`.\n    # Flags >= 0x1000 are reserved for `__future__` imports. We don't have those.\n    f\n\n  #: Generate a sequence of bytes for an opcode with an argument.\n  #:\n  #: code :: (int, int) -> bytes\n  #:\n  code = (op, arg) ~> if\n    op  < opcode.HAVE_ARGUMENT => struct.pack '<B'  op\n    arg < 0                    => @code (op, len @cellvars - arg - 1)\n    arg < 0x10000              => struct.pack '<BH' op arg\n    otherwise                  => @code (opcode.opmap !! 'EXTENDED_ARG', arg >> 16) +\n                                  struct.pack '<BH' op (arg & 0xffff)\n\n  #: Convert this object into an immutable version actually suitable for use with `eval`.\n  #:\n  #: frozen :: types.CodeType\n  #:\n  frozen = ~>\n    code     = b''.join $ map @code @bytecode\n    consts   = list  $ map fst $ sorted @consts key: @consts.__getitem__\n    names    = tuple $ sorted @names    key: @names.__getitem__\n    varnames = tuple $ sorted @varnames key: @varnames.__getitem__\n    cellvars = tuple $ sorted @cellvars key: @cellvars.__getitem__\n    freevars = tuple $ sorted @freevars key: @freevars.__getitem__ reverse: True\n\n    if @optimize =>\n      # Most of the functions are unaffected by the first run, but some\n      # may benefit from two. `PyCode_Optimize` is fast, so why not?\n      code = @optimize code consts names @lnotab\n      code = @optimize code consts names @lnotab\n\n    types.CodeType @argc @kwargc (len varnames) @stacksize @flags code (tuple consts) names\n      varnames\n      @filename\n      @name\n      @lineno\n      @lnotab\n      freevars\n      cellvars\n\n  #: Append a new opcode to the sequence.\n  #:\n  #: append :: str (Optional int) (Optional int) -> a\n  #:\n  append = name arg: 0 delta: 0 ~>\n    @depth delta\n    # These indices are used to quickly change all references\n    # to an array slot into references to a cell.\n    name == 'LOAD_FAST'  => @fastlocals !! arg !! len @bytecode = opcode.opmap !! 'LOAD_DEREF'\n    name == 'STORE_FAST' => @fastlocals !! arg !! len @bytecode = opcode.opmap !! 'STORE_DEREF'\n    @bytecode.append (opcode.opmap !! name, arg)\n\n  #: Request a permanent change in stack size.\n  #:\n  #: depth :: int -> ()\n  #:\n  depth = x ~>\n    # Python calculates the stack depth by scanning bytecode.\n    # We'll opt for traversing the AST instead.\n    @currstack += x\n    @currstack > @stacksize => @stacksize = @currstack\n\n  #: Push `x` onto the value stack.\n  #:\n  #: Technically, `x` can be anything, but most types would make\n  #: the code object unmarshallable.\n  #:\n  #: pushconst :: object -> a\n  #:\n  pushconst = x ~> @append 'LOAD_CONST' delta: +1 $ @consts !! (x, type x)\n\n  #: Push the value assigned to some name onto the value stack.\n  #:\n  #: pushname :: str -> a\n  #:\n  pushname = v ~> if\n    v in @cellvars   => @append 'LOAD_DEREF'  delta: +1 $ @cellvars !! v\n    v in @varnames   => @append 'LOAD_FAST'   delta: +1 $ @varnames !! v\n    v in @enclosed   => @append 'LOAD_DEREF'  delta: +1 $ @freevars !! v\n    v in @globals    => @append 'LOAD_GLOBAL' delta: +1 $ @names !! v\n    v in @constnames => @pushconst $ @constnames !! v\n    otherwise        => @append 'LOAD_GLOBAL' delta: +1 $ @names !! v\n\n  #: Pop the value from the top of the stack, assign it to a name.\n  #:\n  #: popname :: str -> a\n  #:\n  popname = v ~> if\n    v in @cellvars => @append 'STORE_DEREF'  delta: -1 $ @cellvars !! v\n    v in @varnames => @append 'STORE_FAST'   delta: -1 $ @varnames !! v\n    v in @enclosed => @append 'STORE_DEREF'  delta: -1 $ @freevars !! v\n    otherwise      => @append 'STORE_GLOBAL' delta: -1 $ @names    !! v\n\n  #: Load cell objects referencing some names. Used to create closures.\n  #:\n  #: pushcells :: [str] -> a\n  #:\n  pushcells = vs ~> for v in vs =>\n    if v in @varnames => for i in @fastlocals !! (@varnames !! v) =>\n      # All previously inserted `*_FAST` references to that name should be\n      # changed to `*_DEREF` to keep the cell contents up-to-date.\n      @bytecode !! i = @fastlocals !! (@varnames !! v) !! i, @cellvars !! v\n\n    @append 'LOAD_CLOSURE' delta: +1 $ if\n      v in @cellvars => @cellvars !! v\n      v in @varnames => @cellvars !! v\n      otherwise      => @freevars !! v\n\n  #: Insert a jump clause.\n  #:\n  #: jump :: str (Optional bool) (Optional int) -> Jump\n  #:\n  jump = opname reverse: False delta: 0 ~> Jump self reverse opname delta\n\n  #: Make a child code object.\n  #:\n  #: spawn :: str * ** -> CodeType\n  #:\n  spawn = name *: args **: kwargs ~> @__class__ cell: self function: True *: args **: kwargs $ if\n    @var is None       => name\n    @var.isidentifier! => @var\n    otherwise          => '(' + @var + ')'\n"
         , 30);
