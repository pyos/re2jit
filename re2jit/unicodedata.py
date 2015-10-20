import os
import textwrap
import unicodedata


def writeinto(file, data, *args, **kwargs):
    with open(file, 'w') as fd:
        print(textwrap.dedent(data.format(*args, **kwargs)), file=fd)


UNICODE_CODEPOINT_MAX = 0x10FFFF
UNICODE_CODEPOINT_TYPE = []
UNICODE_CLASSREFS = {}


next_genclass = 0
next_spcclass = {}


for uid in range(UNICODE_CODEPOINT_MAX + 1):
    general, _ = cls = unicodedata.category(chr(uid))
    # Classes are two letters: general class and subclass.
    # Example: Lu = Letter, uppercase.
    #          Nd = Number, decimal digit.
    # Most significant 4 bits will identify the general class.
    gid = UNICODE_CLASSREFS.get(general)

    if gid is None:
        gid = next_genclass << 4
        next_genclass += 1
        assert next_genclass < 16, 'should fit in 4 bits'
        UNICODE_CLASSREFS[general] = gid

    sid = UNICODE_CLASSREFS.get(cls, None)

    if sid is None:
        sid = next_spcclass.get(gid, gid)
        next_spcclass[gid] = sid + 1
        UNICODE_CLASSREFS[cls] = sid

    assert (sid & 0xF0) == gid, 'general class id mismatch'
    UNICODE_CODEPOINT_TYPE.append(sid)


writeinto(os.path.join(os.path.dirname(__file__), 'unicodedata.h'),
    '''
        // make re2jit/unicodedata.h
        #include <stdint.h>
        typedef uint8_t  rejit_uni_type_t;
        typedef uint16_t rejit_bmp_char_t;
        typedef uint32_t rejit_uni_char_t;

        static const rejit_uni_char_t UNICODE_CODEPOINT_MAX = {};
        extern const rejit_uni_type_t UNICODE_CODEPOINT_TYPE[{}];
        static const rejit_uni_type_t UNICODE_GENERAL = 0xF0;
        {}
    ''',
    UNICODE_CODEPOINT_MAX,
    UNICODE_CODEPOINT_MAX + 1,
    '\n'.join(
        'static const rejit_uni_type_t UNICODE_TYPE_{} = {};'.format(k, v)
        for k, v in UNICODE_CLASSREFS.items()
    )
)


writeinto(os.path.join(os.path.dirname(__file__), 'unicodedata.cc'),
    '''
    #include "unicodedata.h"

    extern const rejit_uni_type_t UNICODE_CODEPOINT_TYPE[] = {{ {} }};
    ''',
    ','.join(map(str, UNICODE_CODEPOINT_TYPE)),
)
