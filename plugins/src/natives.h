#ifndef NATIVES_H_INCLUDED
#define NATIVES_H_INCLUDED

#include "main.h"
#include "sdk/amx/amx.h"

#define optparam(idx, optvalue) (params[0] / sizeof(cell) < idx ? optvalue : params[idx])
#define amx_OptStrParam(amx,idx,result,default)                             \
    do {                                                                    \
      if (params[0] / sizeof(cell) < idx) { result = default; break; }      \
      cell *amx_cstr_; int amx_length_;                                     \
      amx_GetAddr((amx), (params[idx]), &amx_cstr_);                        \
      amx_StrLen(amx_cstr_, &amx_length_);                                  \
      if (amx_length_ > 0 &&                                                \
          ((result) = (char*)alloca((amx_length_ + 1) * sizeof(*(result)))) != NULL) \
        amx_GetString((char*)(result), amx_cstr_, sizeof(*(result))>1, amx_length_ + 1); \
      else (result) = NULL;                                                 \
    } while (0)

#define AMX_DECLARE_NATIVE(Name) {#Name, n_##Name}

int RegisterNatives(AMX *amx);

#endif
