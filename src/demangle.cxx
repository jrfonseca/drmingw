
#include <cxxabi.h>

#include <string.h>
#include <stdlib.h>

#include "demangle.h"
#include "misc.h"


/**
 * See http://gcc.gnu.org/onlinedocs/libstdc++/manual/ext_demangling.html
 */
char *
demangle(const char * mangled_name)
{
    int status = 0;
    char * output_buffer;
    output_buffer = abi::__cxa_demangle(mangled_name, 0, 0, &status);
    if (status != 0) {
        OutputDebug("error: __cxa_demangle failed with status %i\n", status);
    }
    return output_buffer;
}
