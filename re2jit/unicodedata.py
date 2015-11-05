import os
import textwrap
import unicodedata


def writeinto(file, data, *args, **kwargs):
    with open(file, 'w') as fd:
        print(textwrap.dedent(data).format(*args, **kwargs), file=fd)


SPACE_SIZE = 0x110000  # max. code point + 1
BLOCK_SIZE = 8  # data = table_2[table_1[ch >> block_size] + ch % (1 << block_size)]
assert SPACE_SIZE % (1 << BLOCK_SIZE) == 0

TABLE_CATEGORY_N = {}  # category -> id
TABLE_CATEGORY_1 = []  # character / block -> ref
TABLE_CATEGORY_2 = []  # character % block + ref -> id


def gen_category_table():
    cats  = {}
    blks  = {}
    block = []

    for uid in range(SPACE_SIZE):
        gen, _ = spc = unicodedata.category(chr(uid))
        # Categories are two letters: general category and subcategory.
        # Example: Lu = Letter, uppercase.
        #          Nd = Number, decimal digit.
        # Most significant 4 bits will identify the general category.
        try:
            gid = TABLE_CATEGORY_N[gen]
        except KeyError:
            gid = TABLE_CATEGORY_N[gen] = len(cats) << 4

        try:
            sid = TABLE_CATEGORY_N[spc]
        except KeyError:
            sid = TABLE_CATEGORY_N[spc] = gid | cats.setdefault(gen, 0)
            cats[gen] += 1

        assert (gid &  0x0F) == 0,   'category name collision'
        assert (gid & ~0xFF) == 0,   'category limit exceeded'
        assert (sid &  0xF0) == gid, 'category mismatch or overflow'
        block.append(sid)

        if len(block) == 1 << BLOCK_SIZE:
            block = tuple(block)
            if block not in blks:
                blks[block] = len(TABLE_CATEGORY_2)
                TABLE_CATEGORY_2.extend(block)
            TABLE_CATEGORY_1.append(blks[block])
            block = []


gen_category_table()
print("total  blocks: ", len(TABLE_CATEGORY_1))
print("unique blocks: ", len(TABLE_CATEGORY_2) >> BLOCK_SIZE)


writeinto(os.path.join(os.path.dirname(__file__), 'unicodedata.h'),
    '''
        // make re2jit/unicodedata.h
        #include <stdint.h>
        typedef uint8_t  rejit_uni_type_t;
        typedef uint16_t rejit_bmp_char_t;
        typedef uint32_t rejit_uni_char_t;

        static const rejit_uni_char_t UNICODE_SPACE_SIZE = {};
        static const rejit_uni_char_t UNICODE_BLOCK_SIZE = {};

        extern const rejit_uni_char_t UNICODE_CATEGORY_1[];
        extern const rejit_uni_type_t UNICODE_CATEGORY_2[];
        static const rejit_uni_type_t UNICODE_CATEGORY_GENERAL = 0xF0;
        {}
    ''',
    SPACE_SIZE,
    BLOCK_SIZE,
    '\n'.join('static const rejit_uni_type_t UNICODE_TYPE_{} = {};'.format(k, v)
              for k, v in TABLE_CATEGORY_N.items())
)


writeinto(os.path.join(os.path.dirname(__file__), 'unicodedata.cc'),
    '''
    #include "unicodedata.h"

    extern const rejit_uni_char_t UNICODE_CATEGORY_1[] = {{ {} }};
    extern const rejit_uni_type_t UNICODE_CATEGORY_2[] = {{ {} }};
    ''',
    ','.join(str(x) for x in TABLE_CATEGORY_1),
    ','.join(str(x) for x in TABLE_CATEGORY_2),
)
