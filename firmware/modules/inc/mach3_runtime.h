#ifndef SN_MACH3_RUNTIME_H
#define SN_MACH3_RUNTIME_H
#include <stdbool.h>
void sn_mach3_runtime_init(void);
void sn_mach3_runtime_poll(void);
bool sn_mach3_runtime_armed(void);
#endif
