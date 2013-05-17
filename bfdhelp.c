/*
 * Copyright 2002-2011 Jose Fonseca
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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


#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <malloc.h>

#include <windows.h>
#include <tchar.h>
#include <psapi.h>

#include "misc.h"
#include "pehelp.h"
#include "bfdhelp.h"


#ifdef HAVE_BFD

/*
 * bfd.h will complain without this.
 */
#ifndef PACKAGE
#define PACKAGE
#endif
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION
#endif

#include <bfd.h>
#include <demangle.h>


struct bfdhelp_module
{
	struct bfdhelp_module *next;

	DWORD64 Base;

	IMAGEHLP_MODULE64 ModuleInfo;
	
	bfd_vma image_base_vma;

	bfd *abfd;
	asymbol **syms;
	long symcount;
};


struct bfdhelp_process
{
	struct bfdhelp_process *next;

	HANDLE hProcess;

	struct bfdhelp_module *modules;
};


struct bfdhelp_process *processes = NULL;


// Read in the symbol table.
static bfd_boolean
slurp_symtab (bfd *abfd, asymbol ***syms, long *symcount)
{
	unsigned int size;

	if ((bfd_get_file_flags (abfd) & HAS_SYMS) == 0)
		return FALSE;

	*symcount = bfd_read_minisymbols (abfd, FALSE, (void *) syms, &size);
	if (*symcount == 0)
		*symcount = bfd_read_minisymbols (abfd, TRUE /* dynamic */, (void *) syms, &size);

	if (*symcount < 0)
		return FALSE;

	return TRUE;
}


static struct bfdhelp_module *
bfdhelp_module_create(struct bfdhelp_process * process, DWORD64 Base)
{
	struct bfdhelp_module *module;

	module = calloc(1, sizeof *module);
	if(!module)
		return NULL;
	
	module->Base = Base;

	module->next = process->modules;
	process->modules = module;

	module->ModuleInfo.SizeOfStruct = sizeof module->ModuleInfo;
	if(!SymGetModuleInfo64(process->hProcess, Base, &module->ModuleInfo)) {
		OutputDebug("No module info");
		goto no_bfd;
	}

	module->abfd = bfd_openr(module->ModuleInfo.LoadedImageName, NULL);
	if(!module->abfd) {
		OutputDebug("%s: %s\n", module->ModuleInfo.LoadedImageName, "could not open");
		goto no_bfd;
	}

	if(!bfd_check_format(module->abfd, bfd_object))
	{
		OutputDebug("%s: %s\n", module->ModuleInfo.LoadedImageName, "bad format");
		goto bad_format;
	}

	if(!(bfd_get_file_flags(module->abfd) & HAS_SYMS))
	{
		OutputDebug("%s: %s\n", module->ModuleInfo.LoadedImageName, "no symbols");
		goto no_symbols;
	}

#if 0
	/* This requires access to BFD internal data structures */
	module->image_base_vma = pe_data (module->abfd)->pe_opthdr.ImageBase;
#else
	module->image_base_vma = (bfd_vma) PEGetImageBase(process->hProcess, Base);
#endif

	if(!slurp_symtab(module->abfd, &module->syms, &module->symcount))
		goto no_symbols;

	if(!module->symcount)
		goto no_symcount;

	return module;

no_symcount:
	free(module->syms);
	module->syms = NULL;
no_symbols:
bad_format:
	bfd_close(module->abfd);
	module->abfd = NULL;
no_bfd:

	return module;
}


static void
bfdhelp_module_destroy(struct bfdhelp_module * module)
{
	if(module->syms)
		free(module->syms);
		
	if(module->abfd)
		bfd_close(module->abfd);

	free(module);
}


static struct bfdhelp_module *
bfdhelp_module_lookup(struct bfdhelp_process * process, DWORD64 Base)
{
	struct bfdhelp_module *module;
	
	module = process->modules;
	while(module) {
		if(module->Base == Base)
			return module;

		module = module->next;
	}

	return bfdhelp_module_create(process, Base);
}


static struct bfdhelp_process *
bfdhelp_process_lookup(HANDLE hProcess)
{
	struct bfdhelp_process *process;
		
	process = processes;
	while(process) {
		if(process->hProcess == hProcess)
			return process;

		process = process->next;
	}

	process = calloc(1, sizeof *process);
	if(!process)
		return process;

	process->hProcess = hProcess;

	process->next = processes;
	processes = process;

	return process;
}


// This stucture is used to pass information between translate_addresses and find_address_in_section.
struct find_handle
{
	asymbol **syms;
	bfd_vma pc;
	const char *filename;
	const char *functionname;
	unsigned int line;
	bfd_boolean found;
};


// Look for an address in a section.  This is called via  bfd_map_over_sections. 
static void find_address_in_section (bfd *abfd, asection *section, void *data)
{
	struct find_handle *info = (struct find_handle *) data;
	bfd_vma vma;
	bfd_size_type size;

	if (info->found)
		return;

	if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0)
		return;
	
	vma = bfd_get_section_vma (abfd, section);
	size = bfd_get_section_size (section);

	if (0)
		OutputDebug("section: 0x%08" BFD_VMA_FMT "x - 0x%08" BFD_VMA_FMT "x (pc = 0x%08" BFD_VMA_FMT "x)\n",
			    vma, vma + size, info->pc);

	if (info->pc < vma)
		return;

	if (info->pc >= vma + size)
		return;

	info->found = bfd_find_nearest_line (abfd, section, info->syms, info->pc - vma, 
                                             &info->filename, &info->functionname, &info->line);
}


static BOOL bfdhelp_find_symbol(HANDLE hProcess, DWORD64 Address, struct find_handle *info)
{
	DWORD64 Base;
	struct bfdhelp_process *process;
	struct bfdhelp_module *module;
		
	process = bfdhelp_process_lookup(hProcess);
	if(!process) {
		return FALSE;
	}

	Base = SymGetModuleBase64(hProcess, Address);
	if(!Base) {
		return FALSE;
	}

	module = bfdhelp_module_lookup(process, Base);
	if(!module)
		return FALSE;

	if(!module->abfd)
		return FALSE;

	assert(bfd_get_file_flags(module->abfd) & HAS_SYMS);
	assert(module->symcount);

	memset(info, 0, sizeof *info);
	info->pc = module->image_base_vma + (bfd_vma)Address - (bfd_vma)module->Base;
	info->syms = module->syms;
	info->found = FALSE;

	bfd_map_over_sections(module->abfd, find_address_in_section, info);
	if (info->found == FALSE || info->line == 0) {
		return FALSE;
	}

	if(info->functionname == NULL || *info->functionname == '\0')
		return FALSE;		
	
	return TRUE;
}

#endif /* HAVE_BFD */


BOOL WINAPI BfdSymInitialize(HANDLE hProcess, PCSTR UserSearchPath, BOOL fInvadeProcess)
{
	BOOL ret;
	
	ret = SymInitialize(hProcess, UserSearchPath, fInvadeProcess);

#ifdef HAVE_BFD
	if(ret) {
		struct bfdhelp_process *process;

		process = calloc(1, sizeof *process);
		if(process) {
			process->hProcess = hProcess;

			process->next = processes;
			processes = process;
		}
	}
#endif /* HAVE_BFD */

	return ret;
}

DWORD WINAPI BfdSymSetOptions(DWORD SymOptions)
{
	return SymSetOptions(SymOptions);
}


BOOL WINAPI BfdSymFromAddr(HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol)
{
#ifdef HAVE_BFD
	struct find_handle info;
		
	if(bfdhelp_find_symbol(hProcess, Address, &info)) {
		strncpy(Symbol->Name, info.functionname, Symbol->MaxNameLen);

		if(Displacement) {
			/* TODO */
			*Displacement = 0;
		}

		return TRUE;
	}
#endif /* HAVE_BFD */

	return SymFromAddr(hProcess, Address, Displacement, Symbol);
}


BOOL WINAPI BfdSymGetLineFromAddr64(HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line)
{
#ifdef HAVE_BFD
	struct find_handle info;
		
	if(bfdhelp_find_symbol(hProcess, dwAddr, &info)) {
		Line->FileName = (char *)info.filename;
		Line->LineNumber = info.line;
		
		if(pdwDisplacement) {
			/* TODO */
			*pdwDisplacement = 0;
		}

		return TRUE;
	}
#endif /* HAVE_BFD */

	return SymGetLineFromAddr64(hProcess, dwAddr, pdwDisplacement, Line);
}


DWORD WINAPI BfdUnDecorateSymbolName(PCSTR DecoratedName, PSTR UnDecoratedName, DWORD UndecoratedLength, DWORD Flags)
{
#ifdef HAVE_BFD
	char *res;
	
	assert(DecoratedName != NULL);

	if((res = cplus_demangle(DecoratedName, 0)) != NULL)
	{
		strncpy(UnDecoratedName, res, UndecoratedLength);
		free(res);
		return strlen(UnDecoratedName);
	}
#endif

	return UnDecorateSymbolName(DecoratedName, UnDecoratedName, UndecoratedLength, Flags);
}


BOOL WINAPI BfdSymCleanup(HANDLE hProcess)
{
#ifdef HAVE_BFD
	struct bfdhelp_process **link;
	struct bfdhelp_process *process;
	struct bfdhelp_module *module;
		
	link = &processes;
	process = *link;
	while(process) {
		if(process->hProcess == hProcess) {
			module = process->modules;
			while(module) {
				struct bfdhelp_module *next = module->next;

				bfdhelp_module_destroy(module);

				module = next;
			}

			*link = process->next;
			free(process);
			break;
		}

		link = &process->next;
		process = *link;
	}
#endif /* HAVE_BFD */

	return SymCleanup(hProcess);
}


