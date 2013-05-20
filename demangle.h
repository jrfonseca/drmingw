#ifndef DEMANGLE_H
#define DEMANGLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char *
demangle(const char *mangled_name);

#ifdef __cplusplus
}
#endif

#endif // DEMANGLE_H
