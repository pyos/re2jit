//! library: jitplus
//! library: jit
#include <new>
#include <jit/jit-plus.h>


static jit_context *context;
static jit_function *mul_add_jit;


typedef int (*mul_add_type)(int, int, int);


int mul_add(int x, int y, int z)
{
    return x * y + z;
}


int build_mul_add(jit_context *context)
{
    jit_type_t params[3] = { jit_type_int, jit_type_int, jit_type_int };
    jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_int, params, 3, 1);

    mul_add_jit = new jit_function(*context, signature);
    mul_add_jit->build_start();
    jit_value x = mul_add_jit->get_param(0);
    jit_value y = mul_add_jit->get_param(1);
    jit_value z = mul_add_jit->get_param(2);
    jit_value a = mul_add_jit->insn_mul(x, y);
    jit_value b = mul_add_jit->insn_add(a, z);
    mul_add_jit->insn_return(b);
    int r = mul_add_jit->compile();
    mul_add_jit->build_end();

    if (!r) {
        delete mul_add_jit;
        mul_add_jit = NULL;
    }

    return r;
}
