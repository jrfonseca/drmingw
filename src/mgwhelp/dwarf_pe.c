/*
 * Copyright 2012 Jose Fonseca
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


#include <stdlib.h>

#include <windows.h>

#include "config.h"
#include "dwarf_incl.h"
#include "dwarf_pe.h"

#include "outdbg.h"
#include "paths.h"


typedef struct {
    HANDLE hFile;
    HANDLE hFileMapping;
    union {
        PBYTE lpFileBase;
        PIMAGE_DOS_HEADER pDosHeader;
    };
    PIMAGE_NT_HEADERS pNtHeaders;
    PIMAGE_SECTION_HEADER Sections;
    PIMAGE_SYMBOL pSymbolTable;
    PSTR pStringTable;
} pe_access_object_t;


static int
pe_get_section_info(void *obj,
                    Dwarf_Half section_index,
                    Dwarf_Obj_Access_Section *return_section,
                    int *error)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)obj;

    return_section->addr = 0;
    if (section_index == 0) {
        /* Non-elf object formats must honor elf convention that pSection index
         * is always empty. */
        return_section->size = 0;
        return_section->name = "";
    } else {
        PIMAGE_SECTION_HEADER pSection = pe_obj->Sections + section_index - 1;
        if (pSection->Misc.VirtualSize < pSection->SizeOfRawData) {
            return_section->size = pSection->Misc.VirtualSize;
        } else {
            return_section->size = pSection->SizeOfRawData;
        }
        return_section->name = (const char *)pSection->Name;
        if (return_section->name[0] == '/') {
            return_section->name = &pe_obj->pStringTable[atoi(&return_section->name[1])];
        }
    }
    return_section->link = 0;
    return_section->entrysize = 0;

    return DW_DLV_OK;
}


static Dwarf_Endianness
pe_get_byte_order(void *obj)
{
    return DW_OBJECT_LSB;
}


static Dwarf_Small
pe_get_length_pointer_size(void *obj)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)obj;
    PIMAGE_OPTIONAL_HEADER pOptionalHeader = &pe_obj->pNtHeaders->OptionalHeader;

    switch (pOptionalHeader->Magic) {
    case IMAGE_NT_OPTIONAL_HDR32_MAGIC:
        return 4;
    case IMAGE_NT_OPTIONAL_HDR64_MAGIC:
        return 8;
    default:
        return 0;
    }
}


static Dwarf_Unsigned
pe_get_section_count(void *obj)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)obj;
    PIMAGE_FILE_HEADER pFileHeader = &pe_obj->pNtHeaders->FileHeader;
    return pFileHeader->NumberOfSections + 1;
}


static int
pe_load_section(void *obj,
                Dwarf_Half section_index,
                Dwarf_Small **return_data,
                int *error)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)obj;
    if (section_index == 0) {
        return DW_DLV_NO_ENTRY;
    } else {
        PIMAGE_SECTION_HEADER pSection = pe_obj->Sections + section_index - 1;
        *return_data = pe_obj->lpFileBase + pSection->PointerToRawData;
        return DW_DLV_OK;
    }
}


static const Dwarf_Obj_Access_Methods
pe_methods = {
    pe_get_section_info,
    pe_get_byte_order,
    pe_get_length_pointer_size,
    pe_get_length_pointer_size,
    pe_get_section_count,
    pe_load_section
};


int
dwarf_pe_init(const char *image,
              Dwarf_Handler errhand,
              Dwarf_Ptr errarg,
              Dwarf_Debug *ret_dbg,
              Dwarf_Error *error)
{
    int res = DW_DLV_ERROR;
    pe_access_object_t *pe_obj = 0;
    Dwarf_Obj_Access_Interface *intfc = 0;

    /* Initialize the internal struct */
    pe_obj = (pe_access_object_t *)calloc(1, sizeof *pe_obj);
    if (!pe_obj) {
        goto no_internals;
    }

    pe_obj->hFile = CreateFile(image, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (pe_obj->hFile == INVALID_HANDLE_VALUE) {
        OutputDebug("MGWHELP: %s - file not found\n", image);
        goto no_file;
    }

    pe_obj->hFileMapping = CreateFileMapping(pe_obj->hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!pe_obj->hFileMapping) {
        goto no_file_mapping;
    }

    pe_obj->lpFileBase = (PBYTE)MapViewOfFile(pe_obj->hFileMapping, FILE_MAP_READ, 0, 0, 0);
    if (!pe_obj->lpFileBase) {
        goto no_view_of_file;
    }

    pe_obj->pNtHeaders = (PIMAGE_NT_HEADERS) (
        pe_obj->lpFileBase + 
        pe_obj->pDosHeader->e_lfanew
    );
    pe_obj->Sections = (PIMAGE_SECTION_HEADER) (
        (PBYTE)pe_obj->pNtHeaders + 
        sizeof(DWORD) + 
        sizeof(IMAGE_FILE_HEADER) + 
        pe_obj->pNtHeaders->FileHeader.SizeOfOptionalHeader
    );
    pe_obj->pSymbolTable = (PIMAGE_SYMBOL) (
        pe_obj->lpFileBase + 
        pe_obj->pNtHeaders->FileHeader.PointerToSymbolTable
    );
    pe_obj->pStringTable = (PSTR) 
        &pe_obj->pSymbolTable[pe_obj->pNtHeaders->FileHeader.NumberOfSymbols];
    
    /* Initialize the interface struct */
    intfc = (Dwarf_Obj_Access_Interface *)calloc(1, sizeof *intfc);
    if (!intfc) {
        goto no_intfc;
    }
    intfc->object = pe_obj;
    intfc->methods = &pe_methods;

    res = dwarf_object_init(intfc, errhand, errarg, ret_dbg, error);
    if (res == DW_DLV_OK) {
        return res;
    }

    // https://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html
    Dwarf_Unsigned section_count = pe_get_section_count(pe_obj);
    int section_index;
    for (section_index = 0; section_index < section_count; ++section_index) {
        Dwarf_Obj_Access_Section doas;
        memset(&doas, 0, sizeof doas);
        int err = 0;
        pe_get_section_info(pe_obj, section_index, &doas, &err);
        if (!doas.size) {
            continue;
        }

        if (strcmp(doas.name, ".gnu_debuglink") == 0) {
            Dwarf_Small *data;
            pe_load_section(pe_obj, section_index, &data, &err);
            const char *debuglink = (const char *)data;

            const char *pSeparator = getSeparator(image);
            char *debugImage;
            if (pSeparator) {
                size_t cbDir = pSeparator - image;
                size_t cbFile = strlen(debuglink);
                debugImage = malloc(cbDir + cbFile + 1);
                memcpy(debugImage, image, cbDir);
                memcpy(debugImage + cbDir, debuglink, cbFile + 1);
            } else {
                debugImage = strdup(debuglink);
            }

            res = dwarf_pe_init(debugImage, errhand, errarg, ret_dbg, error);

            free(debugImage);

            break;
        }
    }


    free(intfc);
no_intfc:
    ;
no_view_of_file:
    CloseHandle(pe_obj->hFileMapping);
no_file_mapping:
    CloseHandle(pe_obj->hFile);
no_file:
    free(pe_obj);
no_internals:
    return res;
}


int
dwarf_pe_finish(Dwarf_Debug dbg,
                Dwarf_Error *error)
{
    pe_access_object_t *pe_obj = (pe_access_object_t *)dbg->de_obj_file->object;
    CloseHandle(pe_obj->hFileMapping);
    CloseHandle(pe_obj->hFile);
    free(pe_obj);
    return dwarf_object_finish(dbg, error);
}
