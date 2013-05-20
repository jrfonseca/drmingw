
#include <cxxabi.h>

#include <string.h>
#include <stdlib.h>

#include "demangle.h"


char *
demangle(const char * mangled_name)
{
	char * output_buffer;
	int status = 0;
	output_buffer = abi::__cxa_demangle(mangled_name, NULL, NULL, &status);
	output_buffer = (char *)malloc(strlen(mangled_name) + 1);
	strcpy(output_buffer, mangled_name);
	return output_buffer;
}
