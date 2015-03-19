/*
 * Copyright 2013 Jose Fonseca
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


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
