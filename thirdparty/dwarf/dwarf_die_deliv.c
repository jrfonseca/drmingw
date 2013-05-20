/*

  Copyright (C) 2000-2006 Silicon Graphics, Inc.  All Rights Reserved.
  Portions Copyright (C) 2007-2011 David Anderson. All Rights Reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2.1 of the GNU Lesser General Public License 
  as published by the Free Software Foundation.

  This program is distributed in the hope that it would be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  Further, this software is distributed without any warranty that it is
  free of the rightful claim of any third person regarding infringement 
  or the like.  Any license provided herein, whether implied or 
  otherwise, applies only to this software file.  Patent licenses, if
  any, provided herein do not apply to combinations of this program with 
  other software, or any other product whatsoever.  

  You should have received a copy of the GNU Lesser General Public 
  License along with this program; if not, write the Free Software 
  Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston MA 02110-1301,
  USA.

  Contact information:  Silicon Graphics, Inc., 1500 Crittenden Lane,
  Mountain View, CA 94043, or:

  http://www.sgi.com

  For further information regarding this notice, see:

  http://oss.sgi.com/projects/GenInfo/NoticeExplan

*/
/* The address of the Free Software Foundation is
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, 
   Boston, MA 02110-1301, USA.
   SGI has moved from the Crittenden Lane address.
*/




#include "config.h"
#include "dwarf_incl.h"
#ifdef HAVE_ELF_H
#include <elf.h>
#endif
#include <stdio.h>
#include "dwarf_die_deliv.h"


static int
dwarf_next_cu_header_internal(Dwarf_Debug dbg,
    Dwarf_Bool is_info,
    Dwarf_Unsigned * cu_header_length,
    Dwarf_Half * version_stamp,
    Dwarf_Unsigned * abbrev_offset,
    Dwarf_Half * address_size,
    Dwarf_Half * offset_size,
    Dwarf_Half * extension_size,
    Dwarf_Sig8 * signature,
    Dwarf_Unsigned *typeoffset,
    Dwarf_Unsigned * next_cu_offset,
    Dwarf_Error * error);

/*  New October 2011.  Enables client code to know if
    it is a debug_info or debug_types context. */
Dwarf_Bool
dwarf_get_die_infotypes_flag(Dwarf_Die die)
{
    return die->di_is_info;
}

/*
    For a given Dwarf_Debug dbg, this function checks 
    if a CU that includes the given offset has been read 
    or not.  If yes, it returns the Dwarf_CU_Context 
    for the CU.  Otherwise it returns NULL.  Being an 
    internal routine, it is assumed that a valid dbg 
    is passed.

    **This is a sequential search.  May be too slow.

    If debug_info and debug_abbrev not loaded, this will
    wind up returning NULL. So no need to load before calling
    this.
*/
static Dwarf_CU_Context
_dwarf_find_CU_Context(Dwarf_Debug dbg, Dwarf_Off offset,Dwarf_Bool is_info)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Debug_InfoTypes dis = is_info? &dbg->de_info_reading: 
        &dbg->de_types_reading;

    if (offset >= dis->de_last_offset)
        return (NULL);

    if (dis->de_cu_context != NULL &&
        dis->de_cu_context->cc_next != NULL &&
        dis->de_cu_context->cc_next->cc_debug_offset == offset) {

        return (dis->de_cu_context->cc_next);
    }

    if (dis->de_cu_context != NULL &&
        dis->de_cu_context->cc_debug_offset <= offset) {

        for (cu_context = dis->de_cu_context;
            cu_context != NULL; cu_context = cu_context->cc_next) {

            if (offset >= cu_context->cc_debug_offset &&
                offset < cu_context->cc_debug_offset +
                cu_context->cc_length + cu_context->cc_length_size
                + cu_context->cc_extension_size) {

                return (cu_context);
            }
        }
    }

    for (cu_context = dis->de_cu_context_list;
        cu_context != NULL; cu_context = cu_context->cc_next) {

        if (offset >= cu_context->cc_debug_offset &&
            offset < cu_context->cc_debug_offset +
            cu_context->cc_length + cu_context->cc_length_size
            + cu_context->cc_extension_size) {

            return (cu_context);
        }
    }

    return (NULL);
}


/*  This routine checks the dwarf_offdie() list of 
    CU contexts for the right CU context.  */
static Dwarf_CU_Context
_dwarf_find_offdie_CU_Context(Dwarf_Debug dbg, Dwarf_Off offset,
    Dwarf_Bool is_info)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Debug_InfoTypes dis = is_info? &dbg->de_info_reading: 
        &dbg->de_types_reading; 

    for (cu_context = dis->de_offdie_cu_context;
        cu_context != NULL; cu_context = cu_context->cc_next)

        if (offset >= cu_context->cc_debug_offset &&
            offset < cu_context->cc_debug_offset +
            cu_context->cc_length + cu_context->cc_length_size
            + cu_context->cc_extension_size)

            return (cu_context);

    return (NULL);
}


/*  This function is used to create a CU Context for
    a compilation-unit that begins at offset in 
    .debug_info.  The CU Context is attached to the
    list of CU Contexts for this dbg.  It is assumed
    that the CU at offset has not been read before,
    and so do not call this routine before making
    sure of this with _dwarf_find_CU_Context().
    Returns NULL on error.  As always, being an
    internal routine, assumes a good dbg.

    This function must always set a dwarf error code
    before returning NULL. Always.  */
static Dwarf_CU_Context
_dwarf_make_CU_Context(Dwarf_Debug dbg,
    Dwarf_Off offset,Dwarf_Bool is_info,Dwarf_Error * error)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Unsigned length = 0;
    Dwarf_Signed abbrev_offset = 0;
    Dwarf_Unsigned typeoffset = 0;
    Dwarf_Sig8 signaturedata;
    Dwarf_Unsigned types_extra_len = 0;
    Dwarf_Byte_Ptr cu_ptr = 0;
    int local_extension_size = 0;
    int local_length_size = 0;
    Dwarf_Debug_InfoTypes dis = is_info? &dbg->de_info_reading: 
        &dbg->de_types_reading; 
    Dwarf_Unsigned section_size = is_info? dbg->de_debug_info.dss_size:
        dbg->de_debug_types.dss_size;

    cu_context =
        (Dwarf_CU_Context) _dwarf_get_alloc(dbg, DW_DLA_CU_CONTEXT, 1);
    if (cu_context == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return (NULL);
    }
    cu_context->cc_dbg = dbg;
    cu_context->cc_is_info = is_info;

    {
        Dwarf_Small *dataptr = is_info? dbg->de_debug_info.dss_data:
            dbg->de_debug_types.dss_data;
        cu_ptr = (Dwarf_Byte_Ptr) (dataptr+offset);
    }

    /* READ_AREA_LENGTH updates cu_ptr for consumed bytes */
    READ_AREA_LENGTH(dbg, length, Dwarf_Unsigned,
        cu_ptr, local_length_size, local_extension_size);
    cu_context->cc_length_size = local_length_size;
    cu_context->cc_extension_size = local_extension_size;


    cu_context->cc_length = (Dwarf_Word) length;

    READ_UNALIGNED(dbg, cu_context->cc_version_stamp, Dwarf_Half,
        cu_ptr, sizeof(Dwarf_Half));
    cu_ptr += sizeof(Dwarf_Half);

    READ_UNALIGNED(dbg, abbrev_offset, Dwarf_Signed,
        cu_ptr, local_length_size);
    cu_ptr += local_length_size;
    cu_context->cc_abbrev_offset = (Dwarf_Sword) abbrev_offset;

    cu_context->cc_address_size = *(Dwarf_Small *) cu_ptr;
    ++cu_ptr;
    
    

    if(cu_context->cc_address_size  > sizeof(Dwarf_Addr)) {
        _dwarf_error(dbg, error, DW_DLE_CU_ADDRESS_SIZE_BAD);
        return (NULL);
    }
    if(!is_info) {
        /* debug_types CU headers have extra header bytes. */
        types_extra_len = sizeof(signaturedata) + local_length_size;
    }
    if ((length < (CU_VERSION_STAMP_SIZE + local_length_size + 
        CU_ADDRESS_SIZE_SIZE + types_extra_len)) ||
        ((offset + length + local_length_size + local_extension_size) > 
            section_size)) {

        dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
        _dwarf_error(dbg, error, DW_DLE_CU_LENGTH_ERROR);
        return (NULL);
    }

    if (cu_context->cc_version_stamp != CURRENT_VERSION_STAMP
        && cu_context->cc_version_stamp != CURRENT_VERSION_STAMP3
        && cu_context->cc_version_stamp != CURRENT_VERSION_STAMP4) {
        dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
        _dwarf_error(dbg, error, DW_DLE_VERSION_STAMP_ERROR);
        return (NULL);
    }
    if (!is_info) {
        if (cu_context->cc_version_stamp != CURRENT_VERSION_STAMP4) {
            dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
            _dwarf_error(dbg, error, DW_DLE_DEBUG_TYPES_ONLY_DWARF4);
            return (NULL);
        }
        /*  Now read the debug_types extra header fields of
            the signature (8 bytes) and the typeoffset. */
        memcpy(&signaturedata,cu_ptr,sizeof(signaturedata));
        cu_ptr += sizeof(signaturedata);
        READ_UNALIGNED(dbg, typeoffset, Dwarf_Unsigned,
            cu_ptr, local_length_size);
        cu_context->cc_typeoffset = typeoffset;
        cu_context->cc_signature = signaturedata;
        {
            Dwarf_Unsigned cu_len = length - (local_length_size + 
                local_extension_size);
            if(typeoffset >= cu_len) {
                dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
                _dwarf_error(dbg, error, DW_DLE_DEBUG_TYPEOFFSET_BAD);
                return (NULL);
            }
        }
    }

    if (abbrev_offset >= dbg->de_debug_abbrev.dss_size) {
        dwarf_dealloc(dbg, cu_context, DW_DLA_CU_CONTEXT);
        _dwarf_error(dbg, error, DW_DLE_ABBREV_OFFSET_ERROR);
        return (NULL);
    }

    cu_context->cc_abbrev_hash_table =
        (Dwarf_Hash_Table) _dwarf_get_alloc(dbg, DW_DLA_HASH_TABLE, 1);
    if (cu_context->cc_abbrev_hash_table == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return (NULL);
    }

    cu_context->cc_debug_offset = (Dwarf_Word) offset;

    dis->de_last_offset = (Dwarf_Word) (offset + length +
        local_extension_size + local_length_size);

    if (dis->de_cu_context_list == NULL) {
        dis->de_cu_context_list = cu_context;
        dis->de_cu_context_list_end = cu_context;
    } else {
        dis->de_cu_context_list_end->cc_next = cu_context;
        dis->de_cu_context_list_end = cu_context;
    }

    return (cu_context);
}

static int
reloc_incomplete(Dwarf_Error err)
{
    int e = dwarf_errno(err);
    if( e == DW_DLE_RELOC_MISMATCH_INDEX       ||
        e == DW_DLE_RELOC_MISMATCH_RELOC_INDEX  ||
        e == DW_DLE_RELOC_MISMATCH_STRTAB_INDEX ||
        e == DW_DLE_RELOC_SECTION_MISMATCH      ||
        e == DW_DLE_RELOC_SECTION_MISSING_INDEX  ||
        e == DW_DLE_RELOC_SECTION_LENGTH_ODD     ||
        e == DW_DLE_RELOC_SECTION_PTR_NULL        ||
        e == DW_DLE_RELOC_SECTION_MALLOC_FAIL      ||
        e == DW_DLE_RELOC_SECTION_SYMBOL_INDEX_BAD  ) {
        return 1;
    }
    return 0;
}



/*  Returns offset of next compilation-unit thru next_cu_offset
    pointer.
    It sequentially moves from one
    cu to the next.  The current cu is recorded
    internally by libdwarf.

    The _b form is new for DWARF4 adding new returned fields.  */
int
dwarf_next_cu_header(Dwarf_Debug dbg,
    Dwarf_Unsigned * cu_header_length,
    Dwarf_Half * version_stamp,
    Dwarf_Unsigned * abbrev_offset,
    Dwarf_Half * address_size,
    Dwarf_Unsigned * next_cu_offset,
    Dwarf_Error * error)
{
    Dwarf_Bool is_info = true;
    return dwarf_next_cu_header_internal(dbg,
        is_info,
        cu_header_length,
        version_stamp,
        abbrev_offset,
        address_size,
        0,0,0,0,
        next_cu_offset,
        error);
}
int
dwarf_next_cu_header_b(Dwarf_Debug dbg,
    Dwarf_Unsigned * cu_header_length,
    Dwarf_Half * version_stamp,
    Dwarf_Unsigned * abbrev_offset,
    Dwarf_Half * address_size,
    Dwarf_Half * offset_size,
    Dwarf_Half * extension_size,
    Dwarf_Unsigned * next_cu_offset,
    Dwarf_Error * error)
{
    Dwarf_Bool is_info = true;
    return dwarf_next_cu_header_internal(dbg,
        is_info,
        cu_header_length,
        version_stamp,
        abbrev_offset,
        address_size,
        offset_size,extension_size,
        0,0,
        next_cu_offset,
        error);
}

int
dwarf_next_cu_header_c(Dwarf_Debug dbg,
    Dwarf_Bool is_info,
    Dwarf_Unsigned * cu_header_length,
    Dwarf_Half * version_stamp,
    Dwarf_Unsigned * abbrev_offset,
    Dwarf_Half * address_size,
    Dwarf_Half * offset_size,
    Dwarf_Half * extension_size,
    Dwarf_Sig8 * signature,
    Dwarf_Unsigned * typeoffset,
    Dwarf_Unsigned * next_cu_offset,
    Dwarf_Error * error)
{
    return dwarf_next_cu_header_internal(dbg,
        is_info,
        cu_header_length,
        version_stamp,
        abbrev_offset,
        address_size,
        offset_size,
        extension_size,
        signature,
        typeoffset,
        next_cu_offset,
        error);
}
static int
dwarf_next_cu_header_internal(Dwarf_Debug dbg,
    Dwarf_Bool is_info,
    Dwarf_Unsigned * cu_header_length,
    Dwarf_Half * version_stamp,
    Dwarf_Unsigned * abbrev_offset,
    Dwarf_Half * address_size,
    Dwarf_Half * offset_size,
    Dwarf_Half * extension_size,
    Dwarf_Sig8 * signature,
    Dwarf_Unsigned *typeoffset,
    Dwarf_Unsigned * next_cu_offset,
    Dwarf_Error * error)
{
    /* Offset for current and new CU. */
    Dwarf_Unsigned new_offset = 0;

    /* CU Context for current CU. */
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Debug_InfoTypes dis = 0;
    Dwarf_Unsigned section_size =  0;



    /* ***** BEGIN CODE ***** */

    if (dbg == NULL) {
        _dwarf_error(NULL, error, DW_DLE_DBG_NULL);
        return (DW_DLV_ERROR);
    }
    dis = is_info? &dbg->de_info_reading: &dbg->de_types_reading;
    /*  Get offset into .debug_info of next CU. If dbg has no context,
        this has to be the first one. */
    if (dis->de_cu_context == NULL) {
        Dwarf_Small *dataptr = is_info? dbg->de_debug_info.dss_data:
            dbg->de_debug_types.dss_data;
        new_offset = 0;
        if (!dataptr) {
            Dwarf_Error err2= 0;
            int res = is_info?_dwarf_load_debug_info(dbg, &err2):
                _dwarf_load_debug_types(dbg,&err2);

            if (res != DW_DLV_OK) {
                if(reloc_incomplete(err2)) {
                    /*  We will assume all is ok, though it is not. 
                        Relocation errors need not be fatal.  */
                    char msg_buf[200];
                    snprintf(msg_buf,sizeof(msg_buf),
                        "Relocations did not complete successfully, but we are "
                        " ignoring error: %s",dwarf_errmsg(err2));
                    dwarf_insert_harmless_error(dbg,msg_buf);
                    res = DW_DLV_OK;
                } else {
                    if( error) {
                        *error = err2;
                    }
                    return res;
                }
              
            }
        }

    } else {
        new_offset = dis->de_cu_context->cc_debug_offset +
            dis->de_cu_context->cc_length +
            dis->de_cu_context->cc_length_size +
            dis->de_cu_context->cc_extension_size;
    }

    /*  Check that there is room in .debug_info beyond the new offset
        for at least a new cu header. If not, return 0 to indicate end
        of debug_info section, and reset de_cu_debug_info_offset to
        enable looping back through the cu's. */
    section_size = is_info? dbg->de_debug_info.dss_size:
        dbg->de_debug_types.dss_size;
    if ((new_offset + _dwarf_length_of_cu_header_simple(dbg,is_info)) >= 
        section_size) {
        dis->de_cu_context = NULL;
        return (DW_DLV_NO_ENTRY);
    }

    /* Check if this CU has been read before. */
    cu_context = _dwarf_find_CU_Context(dbg, new_offset,is_info);

    /* If not, make CU Context for it. */
    if (cu_context == NULL) {
        cu_context = _dwarf_make_CU_Context(dbg, new_offset,is_info, error);
        if (cu_context == NULL) {
            /*  Error if CU Context could not be made. Since
                _dwarf_make_CU_Context has already registered an error
                we do not do that here: we let the lower error pass
                thru. */
            return (DW_DLV_ERROR);
        }
    }

    dis->de_cu_context = cu_context;

    if (cu_header_length != NULL) {
        *cu_header_length = cu_context->cc_length;
    }

    if (version_stamp != NULL) {
        *version_stamp = cu_context->cc_version_stamp;
    }
    if (abbrev_offset != NULL) {
        *abbrev_offset = cu_context->cc_abbrev_offset;
    }

    if (address_size != NULL) {
        *address_size = cu_context->cc_address_size;
    }
    if (offset_size != NULL) {
        *offset_size = cu_context->cc_length_size;
    }
    if (extension_size != NULL) {
        *extension_size = cu_context->cc_extension_size;
    }
    if(!is_info) {  
        if(signature) {
            *signature = cu_context->cc_signature;
        }
        if(typeoffset) {
            *typeoffset = cu_context->cc_typeoffset;
        }
    }

    new_offset = new_offset + cu_context->cc_length +
        cu_context->cc_length_size + cu_context->cc_extension_size;
    *next_cu_offset = new_offset;
    return (DW_DLV_OK);
}

static int 
dwarf_ptr_CU_offset(Dwarf_CU_Context cu_context,
    Dwarf_Byte_Ptr di_ptr,
    Dwarf_Bool is_info,
    Dwarf_Off * cu_off)
{
    Dwarf_Debug dbg = cu_context->cc_dbg;
    Dwarf_Small *dataptr = is_info? dbg->de_debug_info.dss_data:
        dbg->de_debug_types.dss_data;
    *cu_off = (di_ptr - dataptr);
    return DW_DLV_OK;
}
#if 0
/* Just for debug purposes */
void print_sib_offset(Dwarf_Die sibling)
{
    Dwarf_Off sib_off;
    Dwarf_Error error;
    dwarf_dieoffset(sibling,&sib_off,&error);
    fprintf(stderr," SIB OFF = 0x%" DW_PR_XZEROS DW_PR_DUx,sib_off);
}
void print_ptr_offset(Dwarf_CU_Context cu_context,Dwarf_Byte_Ptr di_ptr)
{
    Dwarf_Off ptr_off;
    dwarf_ptr_CU_offset(cu_context,di_ptr,&ptr_off);
    fprintf(stderr," PTR OFF = 0x%" DW_PR_XZEROS DW_PR_DUx,ptr_off);
}
#endif


/*  Validate the sibling DIE. This only makes sense to call
    if the sibling's DIEs have been travsersed and 
    dwarf_child() called on each,
    so that the last DIE dwarf_child saw was the last. 
    Essentially ensuring that (after such traversal) that we
    are in the same place a sibling attribute would identify.
    In case we return DW_DLV_ERROR, the global offset of the last
    DIE traversed by dwarf_child is returned through *offset 

    It is essentially guaranteed that  dbg->de_last_die 
    is a stale DIE pointer of a deallocated DIE when we get here. 
    It must not be used as a DIE pointer here,
    just as a sort of anonymous pointer that we just check against
    NULL.

    There is a (subtle?) dependence on the fact that when we call this
    the last dwarf_child() call would have been for this sibling.
    Meaning that this works in a depth-first traversal even though there
    is no stack of 'de_last_die' values.

    The check for dbg->de_last_die just ensures sanity.

    If one is switching between normal debug_frame and eh_frame
    (traversing them in tandem, let us say) in a single
    Dwarf_Debug this validator makes no sense. 
    It works if one processes a .debug_frame (entirely) and
    then an eh_frame (or vice versa) though.
    Use caution.
*/
int 
dwarf_validate_die_sibling(Dwarf_Die sibling,Dwarf_Off *offset)
{
    Dwarf_Debug dbg = 0;
    Dwarf_Error *error = 0;
    Dwarf_Debug_InfoTypes dis = 0;
    CHECK_DIE(sibling, DW_DLV_ERROR);
    dbg = sibling->di_cu_context->cc_dbg;

    dis = sibling->di_is_info? &dbg->de_info_reading: &dbg->de_types_reading;
    
    *offset = 0;
    if (dis->de_last_die && dis->de_last_di_ptr) {
        if (sibling->di_debug_ptr == dis->de_last_di_ptr) {
            return (DW_DLV_OK);
        }
    }
    /* Calculate global offset used for error reporting */
    dwarf_ptr_CU_offset(sibling->di_cu_context,
        dis->de_last_di_ptr,sibling->di_is_info,offset);
    return (DW_DLV_ERROR);
}

/*  This function does two slightly different things
    depending on the input flag want_AT_sibling.  If
    this flag is true, it checks if the input die has
    a DW_AT_sibling attribute.  If it does it returns
    a pointer to the start of the sibling die in the
    .debug_info section.  Otherwise it behaves the 
    same as the want_AT_sibling false case.

    If the want_AT_sibling flag is false, it returns
    a pointer to the immediately adjacent die in the 
    .debug_info section.

    Die_info_end points to the end of the .debug_info 
    portion for the cu the die belongs to.  It is used 
    to check that the search for the next die does not 
    cross the end of the current cu.  Cu_info_start points 
    to the start of the .debug_info portion for the 
    current cu, and is used to add to the offset for 
    DW_AT_sibling attributes.  Finally, has_die_child 
    is a pointer to a Dwarf_Bool that is set true if 
    the present die has children, false otherwise.  
    However, in case want_AT_child is true and the die 
    has a DW_AT_sibling attribute *has_die_child is set 
    false to indicate that the children are being skipped.

    die_info_end  points to the last byte+1 of the cu.  */
static Dwarf_Byte_Ptr
_dwarf_next_die_info_ptr(Dwarf_Byte_Ptr die_info_ptr,
    Dwarf_CU_Context cu_context,
    Dwarf_Byte_Ptr die_info_end,
    Dwarf_Byte_Ptr cu_info_start,
    Dwarf_Bool want_AT_sibling,
    Dwarf_Bool * has_die_child)
{
    Dwarf_Byte_Ptr info_ptr = 0;
    Dwarf_Byte_Ptr abbrev_ptr = 0;
    Dwarf_Word abbrev_code = 0;
    Dwarf_Abbrev_List abbrev_list;
    Dwarf_Half attr = 0;
    Dwarf_Half attr_form = 0;
    Dwarf_Unsigned offset = 0;
    Dwarf_Word leb128_length = 0;
    Dwarf_Unsigned utmp = 0;
    Dwarf_Debug dbg = 0;

    info_ptr = die_info_ptr;
    DECODE_LEB128_UWORD(info_ptr, utmp);
    abbrev_code = (Dwarf_Word) utmp;
    if (abbrev_code == 0) {
        return NULL;
    }


    abbrev_list = _dwarf_get_abbrev_for_code(cu_context, abbrev_code);
    if (abbrev_list == NULL) {
        return (NULL);
    }
    dbg = cu_context->cc_dbg;

    *has_die_child = abbrev_list->ab_has_child;

    abbrev_ptr = abbrev_list->ab_abbrev_ptr;
    do {
        Dwarf_Unsigned utmp2;

        DECODE_LEB128_UWORD(abbrev_ptr, utmp2);
        attr = (Dwarf_Half) utmp2;
        DECODE_LEB128_UWORD(abbrev_ptr, utmp2);
        attr_form = (Dwarf_Half) utmp2;
        if (attr_form == DW_FORM_indirect) {
            Dwarf_Unsigned utmp6;

            /* DECODE_LEB128_UWORD updates info_ptr */
            DECODE_LEB128_UWORD(info_ptr, utmp6);
            attr_form = (Dwarf_Half) utmp6;

        }

        if (want_AT_sibling && attr == DW_AT_sibling) {
            switch (attr_form) {
            case DW_FORM_ref1:
                offset = *(Dwarf_Small *) info_ptr;
                break;
            case DW_FORM_ref2:
                /* READ_UNALIGNED does not update info_ptr */
                READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
                    info_ptr, sizeof(Dwarf_Half));
                break;
            case DW_FORM_ref4:
                READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
                    info_ptr, sizeof(Dwarf_ufixed));
                break;
            case DW_FORM_ref8:
                READ_UNALIGNED(dbg, offset, Dwarf_Unsigned,
                    info_ptr, sizeof(Dwarf_Unsigned));
                break;
            case DW_FORM_ref_udata:
                offset =
                    _dwarf_decode_u_leb128(info_ptr, &leb128_length);
                break;
            case DW_FORM_ref_addr:
                /*  Very unusual.  The FORM is intended to refer to
                    a different CU, but a different CU cannot
                    be a sibling, can it? 
                    We could ignore this and treat as if no DW_AT_sibling
                    present.   Or derive the offset from it and if
                    it is in the same CU use it directly. 
                    The offset here is *supposed* to be a global offset,
                    so adding cu_info_start is wrong  to any offset
                    we find here unless cu_info_start
                    is zero! Lets pretend there is no DW_AT_sibling
                    attribute.  */
                goto no_sibling_attr;
            default:
                return (NULL);
            }

            /*  Reset *has_die_child to indicate children skipped.  */
            *has_die_child = false;

            /*  A value beyond die_info_end indicates an error. Exactly
                at die_info_end means 1-past-cu-end and simply means we
                are at the end, do not return NULL. Higher level code
                will detect that we are at the end. */
            if (cu_info_start + offset > die_info_end) {
                /* Error case, bad DWARF. */
                return (NULL);
            }
            /* At or before end-of-cu */
            return (cu_info_start + offset);
        }

        no_sibling_attr:
        if (attr_form != 0) {
            info_ptr += _dwarf_get_size_of_val(cu_context->cc_dbg,
                attr_form, 
                cu_context->cc_address_size,
                info_ptr,
                cu_context->cc_length_size);
            /*  It is ok for info_ptr == die_info_end, as we will test
                later before using a too-large info_ptr */
            if (info_ptr > die_info_end) {
                /*  More than one-past-end indicates a bug somewhere,
                    likely bad dwarf generation. */
                return (NULL);
            }
        }
    } while (attr != 0 || attr_form != 0);
    return (info_ptr);
}

/*  Multiple TAGs are in fact compile units.
    Allow them all. 
    Return non-zero if a CU tag.
    Else return 0.
*/
static int
is_cu_tag(int t)
{
    if(t == DW_TAG_compile_unit ||
        t == DW_TAG_partial_unit ||
        t == DW_TAG_imported_unit ||
        t == DW_TAG_type_unit) {
        return 1;
    }
    return 0;
}

/*  Given a Dwarf_Debug dbg, and a Dwarf_Die die, it returns 
    a Dwarf_Die for the sibling of die.  In case die is NULL, 
    it returns (thru ptr) a Dwarf_Die for the first die in the current 
    cu in dbg.  Returns DW_DLV_ERROR on error.

    It is assumed that every sibling chain including those with 
    only one element is terminated with a NULL die, except a 
    chain with only a NULL die.

    The algorithm moves from one die to the adjacent one.  It 
    returns when the depth of children it sees equals the number 
    of sibling chain terminations.  A single count, child_depth 
    is used to track the depth of children and sibling terminations 
    encountered.  Child_depth is incremented when a die has the 
    Has-Child flag set unless the child happens to be a NULL die.  
    Child_depth is decremented when a die has Has-Child false, 
    and the adjacent die is NULL.  Algorithm returns when 
    child_depth is 0.

    **NOTE: Do not modify input die, since it is used at the end.  */
int
dwarf_siblingof(Dwarf_Debug dbg,
    Dwarf_Die die,
    Dwarf_Die * caller_ret_die, Dwarf_Error * error)
{
    Dwarf_Bool is_info = true;
    return dwarf_siblingof_b(dbg,die,is_info,caller_ret_die,error);
}
/*  This is the new form, October 2011.  On calling with 'die' NULL,
    we cannot tell if this is debug_info or debug_types, so 
    we must be informed!. */ 
int
dwarf_siblingof_b(Dwarf_Debug dbg,
    Dwarf_Die die,
    Dwarf_Bool is_info,
    Dwarf_Die * caller_ret_die, Dwarf_Error * error)
{
    Dwarf_Die ret_die = 0;
    Dwarf_Byte_Ptr die_info_ptr = 0;
    Dwarf_Byte_Ptr cu_info_start = 0;

    /* die_info_end points 1-past end of die (once set) */
    Dwarf_Byte_Ptr die_info_end = 0;
    Dwarf_Word abbrev_code = 0;
    Dwarf_Unsigned utmp = 0;
    /* Since die may be NULL, we rely on the input argument. */
    Dwarf_Debug_InfoTypes dis = is_info? &dbg->de_info_reading: 
        &dbg->de_types_reading; 
    Dwarf_Small *dataptr = is_info? dbg->de_debug_info.dss_data:
        dbg->de_debug_types.dss_data;

   

    if (dbg == NULL) {
        _dwarf_error(NULL, error, DW_DLE_DBG_NULL);
        return (DW_DLV_ERROR);
    }

    if (die == NULL) {
        /*  Find root die of cu */
        /*  die_info_end is untouched here, need not be set in this
            branch. */
        Dwarf_Off off2;
        Dwarf_CU_Context context=0;

        /*  If we've not loaded debug_info, de_cu_context will be NULL,
            so no need to laod */

        context = dis->de_cu_context;
        if (context == NULL) {
            _dwarf_error(dbg, error, DW_DLE_DBG_NO_CU_CONTEXT);
            return (DW_DLV_ERROR);
        }

        off2 = context->cc_debug_offset;
        cu_info_start = dataptr + off2;
        die_info_ptr = cu_info_start +
            _dwarf_length_of_cu_header(dbg, off2,is_info);
        die_info_end = cu_info_start + context->cc_length +
            context->cc_length_size +
            context->cc_extension_size;
    } else {
        /* Find sibling die. */
        Dwarf_Bool has_child = false;
        Dwarf_Sword child_depth = 0;
        Dwarf_CU_Context context=0;

        /*  We cannot have a legal die unless debug_info was loaded, so
            no need to load debug_info here. */
        CHECK_DIE(die, DW_DLV_ERROR);

        die_info_ptr = die->di_debug_ptr;
        if (*die_info_ptr == 0) {
            return (DW_DLV_NO_ENTRY);
        }
        context = die->di_cu_context;
        cu_info_start = dataptr+ context->cc_debug_offset;
        die_info_end = cu_info_start + context->cc_length +
            context->cc_length_size +
            context->cc_extension_size;

        if ((*die_info_ptr) == 0) {
            return (DW_DLV_NO_ENTRY);
        }
        child_depth = 0;
        do {
            die_info_ptr = _dwarf_next_die_info_ptr(die_info_ptr,
                die->di_cu_context, die_info_end,
                cu_info_start, true, &has_child);
            if (die_info_ptr == NULL) {
                _dwarf_error(dbg, error, DW_DLE_NEXT_DIE_PTR_NULL);
                return (DW_DLV_ERROR);
            }

            /*  die_info_end is one past end. Do not read it!  
                A test for ``!= die_info_end''  would work as well,
                but perhaps < reads more like the meaning. */
            if(die_info_ptr < die_info_end) { 
                if ((*die_info_ptr) == 0 && has_child) {
                    die_info_ptr++;
                    has_child = false;
                }
            }

            /* die_info_ptr can be one-past-end. */
            if ((die_info_ptr == die_info_end) ||
                ((*die_info_ptr) == 0)) {
                for (; child_depth > 0 && *die_info_ptr == 0;
                    child_depth--, die_info_ptr++);
            } else {
                child_depth = has_child ? child_depth + 1 : child_depth;
            }

        } while (child_depth != 0);
    }

    /*  die_info_ptr > die_info_end is really a bug (possibly in dwarf
        generation)(but we are past end, no more DIEs here), whereas
        die_info_ptr == die_info_end means 'one past end, no more DIEs
        here'. */
    if (die_info_ptr >= die_info_end) {
        return (DW_DLV_NO_ENTRY);
    }

    if ((*die_info_ptr) == 0) {
        return (DW_DLV_NO_ENTRY);
    }

    ret_die = (Dwarf_Die) _dwarf_get_alloc(dbg, DW_DLA_DIE, 1);
    if (ret_die == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return (DW_DLV_ERROR);
    }

    ret_die->di_is_info = is_info;
    ret_die->di_debug_ptr = die_info_ptr;
    ret_die->di_cu_context =
        die == NULL ? dis->de_cu_context : die->di_cu_context;

    DECODE_LEB128_UWORD(die_info_ptr, utmp);
    if (die_info_ptr > die_info_end) {
        /*  We managed to go past the end of the CU!. 
            Something is badly wrong. */
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        _dwarf_error(dbg, error, DW_DLE_ABBREV_DECODE_ERROR);
        return (DW_DLV_ERROR);
    }
    abbrev_code = (Dwarf_Word) utmp;
    if (abbrev_code == 0) {
        /* Zero means a null DIE */
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        return (DW_DLV_NO_ENTRY);
    }
    ret_die->di_abbrev_code = abbrev_code;
    ret_die->di_abbrev_list =
        _dwarf_get_abbrev_for_code(ret_die->di_cu_context, abbrev_code);
    if (ret_die->di_abbrev_list == NULL ) {
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        _dwarf_error(dbg, error, DW_DLE_DIE_ABBREV_LIST_NULL);
        return (DW_DLV_ERROR);
    }
    if (die == NULL && !is_cu_tag(ret_die->di_abbrev_list->ab_tag)) { 
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        _dwarf_error(dbg, error, DW_DLE_FIRST_DIE_NOT_CU);
        return (DW_DLV_ERROR);
    }

    *caller_ret_die = ret_die;
    return (DW_DLV_OK);
}


int
dwarf_child(Dwarf_Die die,
    Dwarf_Die * caller_ret_die, Dwarf_Error * error)
{
    Dwarf_Byte_Ptr die_info_ptr = 0;

    /* die_info_end points one-past-end of die area. */
    Dwarf_Byte_Ptr die_info_end = 0;
    Dwarf_Die ret_die = 0;
    Dwarf_Bool has_die_child = 0;
    Dwarf_Debug dbg;
    Dwarf_Word abbrev_code = 0;
    Dwarf_Unsigned utmp = 0;
    Dwarf_Small *dataptr = 0;
    Dwarf_Debug_InfoTypes dis = 0;

    CHECK_DIE(die, DW_DLV_ERROR);
    dbg = die->di_cu_context->cc_dbg;
    dis = die->di_is_info? &dbg->de_info_reading: 
        &dbg->de_types_reading; 
    die_info_ptr = die->di_debug_ptr;

    dataptr = die->di_is_info? dbg->de_debug_info.dss_data:
        dbg->de_debug_types.dss_data;


    /*  We are saving a DIE pointer here, but the pointer
        will not be presumed live later, when it is tested. */
    dis->de_last_die = die;
    dis->de_last_di_ptr = die_info_ptr;

    /* NULL die has no child. */
    if ((*die_info_ptr) == 0)
        return (DW_DLV_NO_ENTRY);

    die_info_end = dataptr +
        die->di_cu_context->cc_debug_offset +
        die->di_cu_context->cc_length +
        die->di_cu_context->cc_length_size +
        die->di_cu_context->cc_extension_size;

    die_info_ptr =
        _dwarf_next_die_info_ptr(die_info_ptr, die->di_cu_context,
            die_info_end, NULL, false,
            &has_die_child);
    if (die_info_ptr == NULL) {
        _dwarf_error(dbg, error, DW_DLE_NEXT_DIE_PTR_NULL);
        return (DW_DLV_ERROR);
    }

    dis->de_last_di_ptr = die_info_ptr;

    if (!has_die_child) {
        /* Look for end of sibling chain. */
        while ( dis->de_last_di_ptr < die_info_end) {
            if (*dis->de_last_di_ptr) {
                break;
            }
            ++dis->de_last_di_ptr;
        }
        return (DW_DLV_NO_ENTRY);
    }

    ret_die = (Dwarf_Die) _dwarf_get_alloc(dbg, DW_DLA_DIE, 1);
    if (ret_die == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return (DW_DLV_ERROR);
    }
    ret_die->di_debug_ptr = die_info_ptr;
    ret_die->di_cu_context = die->di_cu_context;
    ret_die->di_is_info = die->di_is_info;

    DECODE_LEB128_UWORD(die_info_ptr, utmp);
    abbrev_code = (Dwarf_Word) utmp;

    dis->de_last_di_ptr = die_info_ptr;

    if (abbrev_code == 0) {
        /* Look for end of sibling chain */
        while ( dis->de_last_di_ptr < die_info_end) {
            if (*dis->de_last_di_ptr) {
                break;
            }
            ++dis->de_last_di_ptr;
        }

        /*  We have arrived at a null DIE, at the end of a CU or the end 
            of a list of siblings. */
        *caller_ret_die = 0;
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        return DW_DLV_NO_ENTRY;
    }
    ret_die->di_abbrev_code = abbrev_code;
    ret_die->di_abbrev_list =
        _dwarf_get_abbrev_for_code(die->di_cu_context, abbrev_code);
    if (ret_die->di_abbrev_list == NULL) {
        dwarf_dealloc(dbg, ret_die, DW_DLA_DIE);
        _dwarf_error(dbg, error, DW_DLE_DIE_BAD);
        return (DW_DLV_ERROR);
    }

    *caller_ret_die = ret_die;
    return (DW_DLV_OK);
}

/*  Given a (global, not cu_relative) die offset, this returns
    a pointer to a DIE thru *new_die.
    It is up to the caller to do a
    dwarf_dealloc(dbg,*new_die,DW_DLE_DIE); 
    The old form only works with debug_info.
    The new _b form works with debug_info or debug_types.
    */
int
dwarf_offdie(Dwarf_Debug dbg,
    Dwarf_Off offset, Dwarf_Die * new_die, Dwarf_Error * error)
{
    Dwarf_Bool is_info = true;
    return dwarf_offdie_b(dbg,offset,is_info,new_die,error);
}

int
dwarf_offdie_b(Dwarf_Debug dbg,
    Dwarf_Off offset, Dwarf_Bool is_info, 
    Dwarf_Die * new_die, Dwarf_Error * error)
{
    Dwarf_CU_Context cu_context = 0;
    Dwarf_Off new_cu_offset = 0;
    Dwarf_Die die = 0;
    Dwarf_Byte_Ptr info_ptr = 0;
    Dwarf_Unsigned abbrev_code = 0;
    Dwarf_Unsigned utmp = 0;
    Dwarf_Debug_InfoTypes dis = 0;


    if (dbg == NULL) {
        _dwarf_error(NULL, error, DW_DLE_DBG_NULL);
        return (DW_DLV_ERROR);
    }
    dis = is_info? &dbg->de_info_reading:
        &dbg->de_types_reading;

    cu_context = _dwarf_find_CU_Context(dbg, offset,is_info);
    if (cu_context == NULL)
        cu_context = _dwarf_find_offdie_CU_Context(dbg, offset,is_info);

    if (cu_context == NULL) {
        Dwarf_Unsigned section_size = is_info? dbg->de_debug_info.dss_size:
            dbg->de_debug_types.dss_size;
        int res = is_info?_dwarf_load_debug_info(dbg, error):
            _dwarf_load_debug_types(dbg,error);

        if (res != DW_DLV_OK) {
            return res;
        }

        if (dis->de_offdie_cu_context_end != NULL) {
            Dwarf_CU_Context lcu_context =
                dis->de_offdie_cu_context_end;
            new_cu_offset =
                lcu_context->cc_debug_offset +
                lcu_context->cc_length +
                lcu_context->cc_length_size +
                lcu_context->cc_extension_size;
        }


        do {
            if ((new_cu_offset +
                _dwarf_length_of_cu_header_simple(dbg,is_info)) >= section_size) {
                _dwarf_error(dbg, error, DW_DLE_OFFSET_BAD);
                return (DW_DLV_ERROR);
            }

            cu_context =
                _dwarf_make_CU_Context(dbg, new_cu_offset,is_info, error);
            if (cu_context == NULL) {
                /*  Error if CU Context could not be made. Since
                    _dwarf_make_CU_Context has already registered an
                    error we do not do that here: we let the lower error
                    pass thru. */

                return (DW_DLV_ERROR);
            }

            if (dis->de_offdie_cu_context == NULL) {
                dis->de_offdie_cu_context = cu_context;
                dis->de_offdie_cu_context_end = cu_context;
            } else {
                dis->de_offdie_cu_context_end->cc_next = cu_context;
                dis->de_offdie_cu_context_end = cu_context;
            }

            new_cu_offset = new_cu_offset + cu_context->cc_length +
                cu_context->cc_length_size;

        } while (offset >= new_cu_offset);
    }

    die = (Dwarf_Die) _dwarf_get_alloc(dbg, DW_DLA_DIE, 1);
    if (die == NULL) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return (DW_DLV_ERROR);
    }
    die->di_cu_context = cu_context;
    die->di_is_info = is_info;

    {
        Dwarf_Small *dataptr = is_info? dbg->de_debug_info.dss_data:
            dbg->de_debug_types.dss_data;
        info_ptr = dataptr + offset;
    }
    die->di_debug_ptr = info_ptr;
    DECODE_LEB128_UWORD(info_ptr, utmp);
    abbrev_code = utmp;
    if (abbrev_code == 0) {
        /* we are at a null DIE (or there is a bug). */
        *new_die = 0;
        dwarf_dealloc(dbg, die, DW_DLA_DIE);
        return DW_DLV_NO_ENTRY;
    }
    die->di_abbrev_code = abbrev_code;
    die->di_abbrev_list =
        _dwarf_get_abbrev_for_code(cu_context, abbrev_code);
    if (die->di_abbrev_list == NULL) {
        dwarf_dealloc(dbg, die, DW_DLA_DIE);
        _dwarf_error(dbg, error, DW_DLE_DIE_ABBREV_LIST_NULL);
        return (DW_DLV_ERROR);
    }

    *new_die = die;
    return (DW_DLV_OK);
}
