#ifndef NATIVES_H_INCLUDED
#define NATIVES_H_INCLUDED

#include "main.h"
#include "sdk/amx/amx.h"

#define AMX_DECLARE_NATIVE(Name) {#Name, Natives::Name}

int RegisterNatives(AMX *amx);

#endif
