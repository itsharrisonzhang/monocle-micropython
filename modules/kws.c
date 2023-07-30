#include "kws.h"

STATIC MP_DEFINE_CONST_FUN_OBJ_0(run_obj, run);

STATIC const mp_rom_map_elem_t module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_kws) },
    { MP_ROM_QSTR(MP_QSTR_run), MP_ROM_PTR(&run_obj) },
};

STATIC MP_DEFINE_CONST_DICT(module_globals, module_globals_table);

const mp_obj_module_t kws_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_kws, kws_user_cmodule);