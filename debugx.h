#include <bfd.h>

extern int get_line_from_addr(bfd *abfd, asymbol **syms, long symcount, PTR dhandle, bfd_vma address, char *filename, unsigned int nsize, unsigned int *lineno);
extern int print_function_info(bfd *abfd, asymbol **syms, long symcount, PTR dhandle, HANDLE hprocess, const char *function_name, DWORD framepointer);
