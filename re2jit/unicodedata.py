import os
import textwrap
import itertools
import subprocess
import unicodedata


def writeinto(file, data, *args, **kwargs):
    with open(file, 'w') as fd:
        print(textwrap.dedent(data).format(*args, **kwargs), file=fd)


def gperf(xs, name, struct, initial):
    return subprocess.check_output(
        ['gperf', '-N', name, '-H', name + '_hash', '-F', initial, '-ctCEIL', 'ANSI-C', '--null-strings'],
        input='struct {}_t {{ {} }};\n%%\n'.format(name, struct).encode('utf-8') +
              '\n'.join(','.join(map(str, x)) for x in xs).encode('utf-8')).decode('utf-8')


SPACE_SIZE = 0x110000  # max. code point + 1
BLOCK_SIZE = 8  # data = table_2[table_1[ch >> block_size] + ch % (1 << block_size)]


def make_2stage_table(xs):
    blocks = {}
    table1 = []
    table2 = []

    for block in zip(*[iter(xs)] * (1 << BLOCK_SIZE)):
        if block not in blocks:
            blocks[block] = len(table2)
            table2.extend(block)
        table1.append(blocks[block])

    return table1, table2


def make_string_table(xs, bits_per_char):
    return dict(make_string_table_rec(sorted(xs), bits_per_char, 0, 0))


def make_string_table_rec(xs, bits_per_char, i, suffix):
    # `xs` assumed to be sorted.
    it = itertools.groupby((x for x in xs if len(x) > i), lambda x: x[:i + 1])

    for digit, (key, group) in enumerate(it):
        assert digit < (1 << bits_per_char), 'overflow'
        # basically, the result is a table that maps each string to its encoding as a
        # base-N number, and it is guaranteed that if `x` is in that table, then each
        # prefix of `x` is also there; and if `x` is a prefix of `y`, then `enc(x)`
        # is a suffix of `enc(y)`.
        new_suffix = digit << (bits_per_char * i) | suffix
        yield key, new_suffix
        yield from make_string_table_rec(group, bits_per_char, i + 1, new_suffix)


TABLE_CATEGORY_1, \
TABLE_CATEGORY_2 = make_2stage_table(unicodedata.category(chr(c)) for c in range(SPACE_SIZE))
TABLE_CATEGORY_N = make_string_table(set(TABLE_CATEGORY_2), bits_per_char=4)

print("total  blocks: ", len(TABLE_CATEGORY_1))
print("unique blocks: ", len(TABLE_CATEGORY_2) >> BLOCK_SIZE)


writeinto(os.path.join(os.path.dirname(__file__), 'unicodedata.h'),
    '''
    // make re2jit/unicodedata.h
    #include <stdint.h>
    #define UNICODE_2STAGE_GET(t, c) t##_2[(c) % (1 << {1}) + t##_1[(c) >> {1}]]

    static const uint32_t UNICODE_SPACE_SIZE = {0};
    static const uint32_t UNICODE_BLOCK_SIZE = {1};

    extern const uint32_t UNICODE_CATEGORY_1[];
    extern const uint8_t  UNICODE_CATEGORY_2[];
    static const uint8_t  UNICODE_CATEGORY_GENERAL = 0x0F;
    {2}
    ''',
    SPACE_SIZE,
    BLOCK_SIZE,
    gperf(TABLE_CATEGORY_N.items(), '_rejit_uni_cat_id', 'const char *name; uint8_t id;', ', 0'),
)


writeinto(os.path.join(os.path.dirname(__file__), 'unicodedata.cc'),
    '''
    #include "unicodedata.h"

    extern const uint32_t UNICODE_CATEGORY_1[] = {{ {} }};
    extern const uint8_t  UNICODE_CATEGORY_2[] = {{ {} }};
    ''',
    ','.join(str(x)                   for x in TABLE_CATEGORY_1),
    ','.join(str(TABLE_CATEGORY_N[x]) for x in TABLE_CATEGORY_2),
)
