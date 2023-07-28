#include "experimental.h"

STATIC mp_obj_t factorial(mp_obj_t x_obj) {
    int x = mp_obj_get_int(x_obj);
    // if (x > 10) {
    //     return mp_obj_new_int(0);
    // }
    // if (x == 0 || x == 1) {
    //     return mp_obj_new_int(1);
    // }
    // mp_obj_t new_x_obj = mp_obj_new_int(x - 1);
    // return mp_obj_new_int(x * mp_obj_get_int(factorial(new_x_obj)));
    return mp_obj_new_int(_factorial(x));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(factorial_obj, factorial);

STATIC mp_obj_t square(mp_obj_t x_obj) {
    int x = mp_obj_get_int(x_obj);
    // if (x > 1000) {
    //     return mp_obj_new_int(0);
    // }
    // return mp_obj_new_int(x * x);
    return mp_obj_new_int(_square(x));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(square_obj, square);

STATIC const mp_rom_map_elem_t module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),  MP_ROM_QSTR(MP_QSTR_experimental) },
    { MP_ROM_QSTR(MP_QSTR_factorial), MP_ROM_PTR(&factorial_obj) },
    { MP_ROM_QSTR(MP_QSTR_square),    MP_ROM_PTR(&square_obj) },
};

STATIC MP_DEFINE_CONST_DICT(module_globals, module_globals_table);

const mp_obj_module_t experimental_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_experimental, experimental_user_cmodule);