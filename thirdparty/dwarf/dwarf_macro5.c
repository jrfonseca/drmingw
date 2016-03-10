/*
  Copyright (C) 2015-2015 David Anderson. All Rights Reserved.

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

*/

#include "config.h"
#include "dwarf_incl.h"
#include <stdio.h>
#include <limits.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#include "dwarf_macro5.h"

# if 0
#define LEFTPAREN '('
#define RIGHTPAREN ')'
#define SPACE ' '
#endif
#define TRUE 1
#define FALSE 0

/*  Section 6.3: Macro Information:
    Each macro unit ends with an entry
    containing an opcode of 0. */

static const Dwarf_Small dwarf_udata_string_form[]  = {DW_FORM_udata,DW_FORM_string};
static const Dwarf_Small dwarf_udata_udata_form[]   = {DW_FORM_udata,DW_FORM_udata};
static const Dwarf_Small dwarf_udata_strp_form[]    = {DW_FORM_udata,DW_FORM_strp};
static const Dwarf_Small dwarf_udata_strp_sup_form[] = {DW_FORM_udata,DW_FORM_strp_sup};
static const Dwarf_Small dwarf_secoffset_form[]     = {DW_FORM_sec_offset};
static const Dwarf_Small dwarf_udata_strx_form[]    = {DW_FORM_udata,DW_FORM_strx};

#if 0
/* We do not presently use this here. It's a view of DW2 macros. */
struct Dwarf_Macro_Forms_s dw2formsarray[] = {
    {0,0,0},
    {DW_MACINFO_define,2,dwarf_udata_string_form},
    {DW_MACINFO_undef,2,dwarf_udata_string_form},
    {DW_MACINFO_start_file,2,dwarf_udata_udata_form},
    {DW_MACINFO_end_file,0,0},
    {DW_MACINFO_vendor_ext,2,dwarf_udata_string_form},
};

/* Represents original DWARF 2,3,4 macro info */
static const struct Dwarf_Macro_OperationsList_s dwarf_default_macinfo_opslist= {
6, dw2formsarray
};
#endif

struct Dwarf_Macro_Forms_s dw5formsarray[] = {
    {0,0,0},
    {DW_MACRO_define,2,dwarf_udata_string_form},
    {DW_MACRO_undef,2,dwarf_udata_string_form},
    {DW_MACRO_start_file,2,dwarf_udata_udata_form},
    {DW_MACRO_end_file,0,0},

    {DW_MACRO_define_strp,2,dwarf_udata_strp_form},
    {DW_MACRO_undef_strp,2,dwarf_udata_strp_form},
    {DW_MACRO_import,1,dwarf_secoffset_form},

    {DW_MACRO_define_sup,2,dwarf_udata_strp_sup_form},
    {DW_MACRO_undef_sup,2,dwarf_udata_strp_sup_form},
    {DW_MACRO_import_sup,1,dwarf_secoffset_form},

    {DW_MACRO_define_strx,2,dwarf_udata_strx_form},
    {DW_MACRO_undef_strx,2,dwarf_udata_strx_form},
};



/* Represents DWARF 5 macro info */
/* .debug_macro predefined, in order by value  */
static const struct Dwarf_Macro_OperationsList_s dwarf_default_macro_opslist = {
13, dw5formsarray
};


int _dwarf_internal_macro_context_by_offset(Dwarf_Debug dbg,
    Dwarf_Unsigned offset,
    Dwarf_Unsigned  * version_out,
    Dwarf_Macro_Context * macro_context_out,
    Dwarf_Unsigned *macro_ops_count_out,
    Dwarf_Unsigned *macro_ops_data_length,
    char **srcfiles,
    Dwarf_Signed srcfilescount,
    const char *comp_dir,
    const char *comp_name,
    Dwarf_Error * error);

int _dwarf_internal_macro_context(Dwarf_Die die,
    Dwarf_Bool offset_specified,
    Dwarf_Unsigned offset,
    Dwarf_Unsigned  * version_out,
    Dwarf_Macro_Context * macro_context_out,
    Dwarf_Unsigned *macro_unit_offset_out,
    Dwarf_Unsigned *macro_ops_count_out,
    Dwarf_Unsigned *macro_ops_data_length,
    Dwarf_Error * error);

static int
is_std_moperator(Dwarf_Small op)
{
    if (op >= 1 && op <= DW_MACRO_undef_strx) {
        return TRUE;
    }
    return FALSE;
}

static int
_dwarf_skim_forms(Dwarf_Debug dbg,
    Dwarf_Macro_Context mcontext,
    Dwarf_Small *mdata_start,
    unsigned formcount,
    const Dwarf_Small *forms,
    Dwarf_Small *section_end,
    Dwarf_Unsigned *forms_length,
    Dwarf_Error *error)
{
    unsigned i = 0;
    Dwarf_Small curform = 0 ;
    Dwarf_Unsigned totallen = 0;
    Dwarf_Unsigned v = 0;
    Dwarf_Unsigned ret_value = 0;
    Dwarf_Unsigned length;
    Dwarf_Small *mdata = mdata_start;
    Dwarf_Word leb128_length = 0;

    for( ; i < formcount; ++i) {
        curform = forms[i];
        if (mdata >= section_end) {
            _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
            return DW_DLV_ERROR;
        }
        switch(curform) {
        default:
            _dwarf_error(dbg,error,DW_DLE_DEBUG_FORM_HANDLING_INCOMPLETE);
            return DW_DLV_ERROR;
        case DW_FORM_block1:
            v =  *(Dwarf_Small *) mdata;
            totallen += v+1;
            mdata += v+1;
            break;
        case DW_FORM_block2:
            READ_UNALIGNED(dbg, ret_value, Dwarf_Unsigned,
                mdata, sizeof(Dwarf_Half));
            v = ret_value + sizeof(Dwarf_Half);
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_block4:
            READ_UNALIGNED(dbg, ret_value, Dwarf_Unsigned,
                mdata, sizeof(Dwarf_ufixed));
            v = ret_value + sizeof(Dwarf_ufixed);
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_data1:
            v = 1;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_data2:
            v = 2;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_data4:
            v = 4;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_data8:
            v = 8;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_data16:
            v = 8;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_string:
            v = strlen((char *) mdata) + 1;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_block:
            length = _dwarf_decode_u_leb128(mdata, &leb128_length);
            v = length + leb128_length;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_flag:
            v = 1;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_sec_offset:
            /* If 32bit dwarf, is 4. Else is 64bit dwarf and is 8. */
            v = mcontext->mc_offset_size;
            totallen += v;
            mdata += v;
            break;
        case DW_FORM_sdata:
            /*  Discard the decoded value, we just want the length
                of the value. */
            _dwarf_decode_s_leb128(mdata, &leb128_length);
            v = leb128_length;
            mdata += v;
            totallen += v;
            break;
        case DW_FORM_strx:
            _dwarf_decode_u_leb128(mdata, &leb128_length);
            v = leb128_length;
            mdata += v;
            totallen += v;
            break;
        case DW_FORM_strp:
            v = mcontext->mc_offset_size;
            mdata += v;
            totallen += v;
            break;
        case DW_FORM_udata:
            /*  Discard the decoded value, we just want the length
                of the value. */
            _dwarf_decode_u_leb128(mdata, &leb128_length);
            v = leb128_length;
            mdata += v;
            totallen += v;
            break;
        }
    }
    if (mdata > section_end) {
        _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
        return DW_DLV_ERROR;
    }
    *forms_length = totallen;
    return DW_DLV_OK;
}

#if 0
static void
dump_bytes(Dwarf_Small * start, long len)
{
    Dwarf_Small *end = start + len;
    Dwarf_Small *cur = start;
    unsigned pos = 0;

    printf("dump %ld bytes, start at 0x%lx\n",len,(unsigned long)start);
    printf("0x");
    for (; cur < end;pos++, cur++) {
        if (!(pos %4)) {
            printf(" ");
        }
        printf("%02x",*cur);
    }
    printf("\n");
}
#endif


/*  On first call (for this macro_context),
    build_ops_array is FALSE. On second,
    it is TRUE and we know the count so we allocate and fill in
    the ops array. */
int
_dwarf_get_macro_ops_count_internal(Dwarf_Macro_Context macro_context,
    Dwarf_Bool build_ops_array,
    Dwarf_Error *error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_Small *mdata = 0;
    Dwarf_Small *section_end = 0;
    Dwarf_Small *section_base = 0;
    Dwarf_Unsigned opcount = 0;
    Dwarf_Unsigned known_ops_count = 0;
    struct Dwarf_Macro_Operator_s *opsarray = 0;
    struct Dwarf_Macro_Operator_s *curopsentry = 0;
    int res = 0;

    dbg = macro_context->mc_dbg;
    if (build_ops_array) {
        known_ops_count = macro_context->mc_macro_ops_count;
        opsarray = (struct Dwarf_Macro_Operator_s *)
            calloc(known_ops_count,sizeof(struct Dwarf_Macro_Operator_s));
        if(!opsarray) {
            _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
            return DW_DLV_ERROR;
        }
        curopsentry = opsarray;
        macro_context->mc_ops = opsarray;
    }
    section_base = dbg->de_debug_macro.dss_data;
    section_end = section_base + dbg->de_debug_macro.dss_size;
    mdata = macro_context->mc_macro_ops;

    while (mdata < section_end) {
        Dwarf_Small op = 0;

        if (mdata >= section_end)  {
            _dwarf_error(dbg, error, DW_DLE_MACRO_PAST_END);
            return DW_DLV_ERROR;
        }
        op = *mdata;
        ++opcount;
        ++mdata;
        /*  Here so we would set it for the zero op,
            though that is kind of redundant since
            the curopsentry starts out zero-d.
            if (build_ops_array)
                curopsentry->mo_opcode = op;  */
        if (!op) {
            Dwarf_Unsigned opslen = 0;
            /*  End of ops, this is terminator, count the ending 0
                as an operator so dwarfdump can print it.  */
            opslen = mdata - macro_context->mc_macro_ops;
            macro_context->mc_macro_ops_count = opcount;
            macro_context->mc_ops_data_length = opslen;
            macro_context->mc_total_length = opslen +
                macro_context->mc_macro_header_length;
            return DW_DLV_OK;
        }
        if (is_std_moperator(op)) {
            struct Dwarf_Macro_Forms_s * ourform =
                dw5formsarray + op;
            /* ASSERT: op == ourform->mf_code */
            unsigned formcount = ourform->mf_formcount;
            const Dwarf_Small *forms = ourform->mf_formbytes;
            Dwarf_Unsigned forms_length = 0;

            res = _dwarf_skim_forms(dbg,macro_context,mdata,
                formcount,forms,
                section_end,
                &forms_length,error);
            if ( res != DW_DLV_OK) {
                return res;
            }
            if(build_ops_array) {
                curopsentry->mo_opcode = op;
                curopsentry->mo_form = ourform;
                curopsentry->mo_data = mdata;
            }
            mdata += forms_length;
        } else {
            /* FIXME Add support for user defined ops. */
            _dwarf_error(dbg, error, DW_DLE_MACRO_OP_UNHANDLED);
            return DW_DLV_ERROR;
        }
        if (mdata > section_end)  {
            _dwarf_error(dbg, error, DW_DLE_MACRO_PAST_END);
            return DW_DLV_ERROR;
        }
        curopsentry++;
    }
    _dwarf_error(dbg, error, DW_DLE_MACRO_PAST_END);
    return DW_DLV_ERROR;
}

int
dwarf_get_macro_op(Dwarf_Macro_Context macro_context,
    Dwarf_Unsigned op_number,
    Dwarf_Unsigned * op_start_section_offset,
    Dwarf_Half    * macro_operator,
    Dwarf_Half    * forms_count,
    const Dwarf_Small **   formcode_array,
    Dwarf_Error *error)
{
    struct Dwarf_Macro_Operator_s *curop = 0;
    Dwarf_Debug dbg = 0;
    if (!macro_context || macro_context->mc_sentinel != 0xada) {
        if(macro_context) {
            dbg = macro_context->mc_dbg;
        }
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_HEADER_POINTER);
        return DW_DLV_ERROR;
    }
    dbg = macro_context->mc_dbg;
    if (op_number >= macro_context->mc_macro_ops_count) {
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_INDEX);
        return DW_DLV_ERROR;
    }
    curop = macro_context->mc_ops + op_number;

    /*  ASSERT: *op_start_section_offset ==
        (curop->mo_data -1) - dbg->de_debug_macro.dss_data  */
    *op_start_section_offset =
        ((curop->mo_data -1) - macro_context->mc_macro_header) +
        macro_context->mc_section_offset;
    *macro_operator = curop->mo_opcode;
    if (curop->mo_form) {
        *forms_count  = curop->mo_form->mf_formcount;
        *formcode_array = curop->mo_form->mf_formbytes;
    } else {
        /* ASSERT: macro_operator == 0 */
        *forms_count  = 0;
        *formcode_array = 0;
    }
    return DW_DLV_OK;
}

#if 0
Dwarf_Bool
is_defundef(unsigned op)
{
    switch(op){
    case DW_MACRO_define:
    case DW_MACRO_undef:
    case DW_MACRO_define_strp:
    case DW_MACRO_undef_strp:
    case DW_MACRO_define_strx:
    case DW_MACRO_undef_strx:
    case DW_MACRO_define_sup:
    case DW_MACRO_undef_sup:
        return TRUE;
    }
    return FALSE;
}
#endif

/*  Here a DW_DLV_NO_ENTRY return means the macro operator
    is not a def/undef operator. */
int
dwarf_get_macro_defundef(Dwarf_Macro_Context macro_context,
    Dwarf_Unsigned op_number,
    Dwarf_Unsigned * line_number,
    Dwarf_Unsigned * index,
    Dwarf_Unsigned * offset,
    Dwarf_Half     * forms_count,
    const char     ** macro_string,
    Dwarf_Error *error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_Small *mdata = 0;
    int res = 0;
    Dwarf_Small *startptr = 0;
    Dwarf_Small *endptr = 0;
    Dwarf_Half lformscount = 0;
    struct Dwarf_Macro_Operator_s *curop = 0;
    unsigned macop = 0;

    if (!macro_context || macro_context->mc_sentinel != 0xada) {
        if(macro_context) {
            dbg = macro_context->mc_dbg;
        }
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_HEADER_POINTER);
        return DW_DLV_ERROR;
    }
    dbg = macro_context->mc_dbg;
    if (op_number >= macro_context->mc_macro_ops_count) {
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_INDEX);
        return DW_DLV_ERROR;
    }
    curop = macro_context->mc_ops + op_number;
    macop = curop->mo_opcode;
    startptr = macro_context->mc_macro_header;
    endptr = startptr + macro_context->mc_total_length;
    mdata = curop->mo_data;
    lformscount = curop->mo_form->mf_formcount;
    if (lformscount != 2) {
        /*_dwarf_error(dbg, error,DW_DLE_MACRO_OPCODE_FORM_BAD);*/
        return DW_DLV_NO_ENTRY;
    }
    switch(macop){
    case DW_MACRO_define:
    case DW_MACRO_undef: {
        Dwarf_Unsigned linenum = 0;
        Dwarf_Word uleblen = 0;
        const char * content = 0;

        linenum = _dwarf_decode_u_leb128(mdata,
            &uleblen);
        mdata += uleblen;
        content = (const char *)mdata;
        res = _dwarf_check_string_valid(dbg,
            startptr,mdata, endptr, error);
        if(res != DW_DLV_OK) {
            return res;
        }
        *line_number = linenum;
        *index = 0;
        *offset = 0;
        *forms_count = lformscount;
        *macro_string = content;
        }
        return DW_DLV_OK;
    case DW_MACRO_define_strp:
    case DW_MACRO_undef_strp: {
        Dwarf_Unsigned linenum = 0;
        Dwarf_Word uleblen = 0;
        Dwarf_Unsigned stringoffset = 0;
        Dwarf_Small form1 =  curop->mo_form->mf_formbytes[1];
        char * localstr = 0;


        linenum = _dwarf_decode_u_leb128(mdata,
            &uleblen);
        mdata += uleblen;

        READ_UNALIGNED(dbg,stringoffset,Dwarf_Unsigned,
            mdata,macro_context->mc_offset_size);
        mdata += macro_context->mc_offset_size;

        res = _dwarf_extract_local_debug_str_string_given_offset(dbg,
            form1,
            stringoffset,
            &localstr,
            error);
        *index = 0;
        *line_number = linenum;
        *offset = stringoffset;
        *forms_count = lformscount;
        if (res == DW_DLV_ERROR) {
            return res;
            *macro_string = "<Error: getting local .debug_str>";
        } else if (res == DW_DLV_NO_ENTRY) {
            *macro_string = "<Error: NO_ENTRY on .debug_string (strp)>";
        } else {
            *macro_string = (const char *)localstr;
        }
        }
        return DW_DLV_OK;
    case DW_MACRO_define_strx:
    case DW_MACRO_undef_strx: {
        Dwarf_Unsigned linenum = 0;
        Dwarf_Word uleblen = 0;
        Dwarf_Unsigned stringindex = 0;
        Dwarf_Unsigned offsettostr= 0;
        int res = 0;
        Dwarf_Small form1 =  curop->mo_form->mf_formbytes[1];

        linenum = _dwarf_decode_u_leb128(mdata,
            &uleblen);
        mdata += uleblen;

        *line_number = linenum;
        stringindex = _dwarf_decode_u_leb128(mdata,
            &uleblen);
        /* leave mdata uchanged for call below */


        *index = stringindex;
        *forms_count = lformscount;

        /* FIXME */
        /* Redoes the index-getting. Gets offset. */
        res = _dwarf_extract_string_offset_via_str_offsets(dbg,
            mdata,
            DW_AT_macros, /*arbitrary, unused by called routine. */
            form1,
            macro_context->mc_cu_context,
            &offsettostr,
            error);
        if (res  == DW_DLV_ERROR) {
            return res;
        }
        if (res == DW_DLV_OK) {
            *index = stringindex;
            *offset = offsettostr;
            char *localstr = 0;
            res = _dwarf_extract_local_debug_str_string_given_offset(dbg,
                form1,
                offsettostr,
                &localstr,
                error);
            if(res == DW_DLV_ERROR) {
                return res;
            } else if (res == DW_DLV_NO_ENTRY){
                *macro_string = "<:No string available>";
            } else {
                *macro_string = (const char *)localstr;
                /* All is ok. */
            }
        } else {
            *index = stringindex;
            *offset = 0;
            *macro_string = "<.debug_str_offsets not available>";
        }
        }
        return DW_DLV_OK;
    case DW_MACRO_define_sup:
    case DW_MACRO_undef_sup: {
        Dwarf_Unsigned linenum = 0;
        Dwarf_Word uleblen = 0;
        Dwarf_Unsigned supoffset = 0;
        char *localstring = 0;
        int res = 0;

        linenum = _dwarf_decode_u_leb128(mdata,
            &uleblen);
        mdata += uleblen;

        READ_UNALIGNED(dbg,supoffset,Dwarf_Unsigned,
            mdata,macro_context->mc_offset_size);
        mdata += macro_context->mc_offset_size;
        *line_number = linenum;
        *index = 0;
        *offset = supoffset;
        *forms_count = lformscount;
        res = _dwarf_get_string_from_tied(dbg, supoffset,
            &localstring, error);
        if (res != DW_DLV_OK) {
            if (res == DW_DLV_ERROR) {
                if(dwarf_errno(*error) == DW_DLE_NO_TIED_FILE_AVAILABLE) {
                    *macro_string =
                        (char *)"<DW_FORM_str_sup-no-tied_file>";
                } else {
                    *macro_string =
                        (char *)"<Error: DW_FORM_str_sup-got-error>";
                }
                dwarf_dealloc(dbg,*error,DW_DLA_ERROR);
            } else {
                *macro_string = "<DW_FORM_str_sup-no-entry>";
            }
        } else {
            *macro_string = (const char *)localstring;
        }
        return DW_DLV_OK;
        }
    }
    return DW_DLV_NO_ENTRY;
}

/*  ASSERT: we elsewhere guarantee room to copy into.
    If trimtarg ==1, trim trailing slash in targ.
    Caller should not pass in 'src'
    with leading /  */
static void
specialcat(char *targ,char *src,int trimtarg)
{
    char *last = 0;

    while( *targ) {
        last = targ;
        targ++;
    }
    /* TARG now points at terminating NUL */
    /* LAST points at final character in targ. */
    if (trimtarg ) {
        if(last && *last == '/') {
            /* Truncate. */
            *last = 0;
            targ = last;
            /* TARG again points at terminating NUL */
        }
    }
    while (*src) {
        *targ = *src;
        targ++;
        src++;
    }
    *targ = 0;
}

/* If returns NULL caller must handle it. */
const char *
construct_from_dir_and_name(const char *dir,
   const char *name)
{
    int truelen = 0;
    char *final = 0;

    /* Allow for NUL char and added /  */
    truelen = strlen(dir) + strlen(name) + 1 +1;
    final = (char *)malloc(truelen);
    if(!final) {
        return NULL;
    }
    final[0] = 0;
    specialcat(final,(char *)dir,1);
    strcat(final,"/");
    specialcat(final,(char *)name,0);
    return final;
}

/* If returns NULL caller must handle it. */
const char *
construct_at_path_from_parts(Dwarf_Macro_Context mc)
{
    if (mc->mc_file_path) {
        return mc->mc_file_path;
    }
    if(!mc->mc_at_comp_dir || !mc->mc_at_comp_dir[0]) {
        return mc->mc_at_name;
    }
    if(_dwarf_file_name_is_full_path((Dwarf_Small *)mc->mc_at_name)) {
        return mc->mc_at_name;
    }
    if (!mc->mc_at_name || !mc->mc_at_name[0]) {
        return NULL;
    }
    /* Dwarf_Macro_Context destructor will free this. */
    mc->mc_file_path = construct_from_dir_and_name(
        mc->mc_at_comp_dir,mc->mc_at_name);
    return mc->mc_file_path;
}


int
dwarf_get_macro_startend_file(Dwarf_Macro_Context macro_context,
    Dwarf_Unsigned op_number,
    Dwarf_Unsigned * line_number,
    Dwarf_Unsigned * name_index_to_line_tab,
    const char     ** src_file_name,
    Dwarf_Error *error)
{
    Dwarf_Debug dbg = 0;
    Dwarf_Small *mdata = 0;
    unsigned macop = 0;
    struct Dwarf_Macro_Operator_s *curop = 0;

    if (!macro_context || macro_context->mc_sentinel != 0xada) {
        if(macro_context) {
            dbg = macro_context->mc_dbg;
        }
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_HEADER_POINTER);
        return DW_DLV_ERROR;
    }
    dbg = macro_context->mc_dbg;
    if (op_number >= macro_context->mc_macro_ops_count) {
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_INDEX);
        return DW_DLV_ERROR;
    }
    curop = macro_context->mc_ops + op_number;
    macop = curop->mo_opcode;
    mdata = curop->mo_data;
    if (macop != DW_MACRO_start_file && macop != DW_MACRO_end_file) {
        return DW_DLV_NO_ENTRY;
    }
    if (macop == DW_MACRO_start_file) {
        Dwarf_Word uleblen = 0;
        Dwarf_Unsigned linenum = 0;
        Dwarf_Unsigned srcindex = 0;
        Dwarf_Signed trueindex = 0;

        linenum = _dwarf_decode_u_leb128(mdata,
            &uleblen);
        mdata += uleblen;

        srcindex = _dwarf_decode_u_leb128(mdata,
            &uleblen);
        mdata += uleblen;
        *line_number = linenum;
        *name_index_to_line_tab = srcindex;;
        /*  For DWARF 2,3,4, decrement by 1.
            FOR DWARF 5 do not decrement. */
        if(macro_context->mc_version_number >= 5) {
            trueindex = srcindex;
            if (trueindex >= 0 &&
                trueindex < macro_context->mc_srcfiles_count) {
                *src_file_name = macro_context->mc_srcfiles[trueindex];
            } else {
                *src_file_name = "<no-source-file-name-available>";
            }
        } else {
            trueindex = srcindex-1;
            if (srcindex > 0 &&
                trueindex < macro_context->mc_srcfiles_count) {
                *src_file_name = macro_context->mc_srcfiles[trueindex];
            } else {
                const char *mcatcomp = construct_at_path_from_parts(
                    macro_context);
                if(mcatcomp) {
                    *src_file_name = mcatcomp;
                } else {
                    *src_file_name = "<no-source-file-name-available>";
                }
            }
        }
    } else {
        /* DW_MACRO_end_file. No operands. */
    }
    return DW_DLV_OK;
}

/*  Target_offset is the offset in a .debug_macro section,
    of a macro unit header.
    Returns DW_DLV_NO_ENTRY if the macro operator is not
    one of the import operators.  */
int
dwarf_get_macro_import(Dwarf_Macro_Context macro_context,
    Dwarf_Unsigned   op_number,
    Dwarf_Unsigned * target_offset,
    Dwarf_Error *error)
{
    Dwarf_Unsigned supoffset = 0;
    Dwarf_Debug dbg = 0;
    unsigned macop = 0;
    struct Dwarf_Macro_Operator_s *curop = 0;
    Dwarf_Small *mdata = 0;

    if (!macro_context || macro_context->mc_sentinel != 0xada) {
        if(macro_context) {
            dbg = macro_context->mc_dbg;
        }
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_HEADER_POINTER);
        return DW_DLV_ERROR;
    }
    dbg = macro_context->mc_dbg;
    if (op_number >= macro_context->mc_macro_ops_count) {
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_INDEX);
        return DW_DLV_ERROR;
    }
    curop = macro_context->mc_ops + op_number;
    macop = curop->mo_opcode;
    mdata = curop->mo_data;
    if (macop != DW_MACRO_import && macop != DW_MACRO_import_sup) {
        return DW_DLV_NO_ENTRY;
    }
    READ_UNALIGNED(dbg,supoffset,Dwarf_Unsigned,
        mdata,macro_context->mc_offset_size);
    *target_offset = supoffset;
    return DW_DLV_OK;
}

/* */
static int
valid_macro_form(Dwarf_Half form)
{
    switch(form) {
    case DW_FORM_block:
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
    case DW_FORM_data1:
    case DW_FORM_data2:
    case DW_FORM_data4:
    case DW_FORM_data8:
    case DW_FORM_data16:
    case DW_FORM_sdata:
    case DW_FORM_udata:
    case DW_FORM_flag:
    case DW_FORM_sec_offset:
    case DW_FORM_string:
    case DW_FORM_strp:
    case DW_FORM_strx:
        return TRUE;
    }
    return FALSE;
}

static int
validate_opcode(Dwarf_Debug dbg,
   struct Dwarf_Macro_Forms_s *curform,
   Dwarf_Error * error)
{
    unsigned i = 0;
    struct Dwarf_Macro_Forms_s *stdfptr = 0;
    if (curform->mf_code >= DW_MACRO_lo_user) {
        /* Nothing to check. user level. */
        return DW_DLV_OK;
    }
    if (curform->mf_code > DW_MACRO_undef_strx) {
        _dwarf_error(dbg, error, DW_DLE_MACRO_OPCODE_BAD);
        return (DW_DLV_ERROR);
    }
    if (!curform->mf_code){
        _dwarf_error(dbg, error, DW_DLE_MACRO_OPCODE_BAD);
        return (DW_DLV_ERROR);
    }
    stdfptr = &dwarf_default_macro_opslist.mol_data[curform->mf_code];

    if (curform->mf_formcount != stdfptr->mf_formcount) {
        _dwarf_error(dbg, error, DW_DLE_MACRO_OPCODE_FORM_BAD);
        return (DW_DLV_ERROR);
    }
    for(i = 0; i < curform->mf_formcount; ++i) {
        if (curform->mf_formbytes[i] != stdfptr->mf_formbytes[1]) {
            _dwarf_error(dbg, error, DW_DLE_MACRO_OPCODE_FORM_BAD);
            return (DW_DLV_ERROR);
        }
    }
    return DW_DLV_OK;
}

static int
read_operands_table(Dwarf_Macro_Context macro_context,
    Dwarf_Small * macro_header,
    Dwarf_Small * macro_data,
    Dwarf_Small * section_base,
    Dwarf_Unsigned section_size,
    Dwarf_Unsigned *table_size_out,
    Dwarf_Error *error)
{
    Dwarf_Small* table_data_start = macro_data;
    Dwarf_Unsigned local_size = 0;
    Dwarf_Unsigned cur_offset = 0;
    Dwarf_Small operand_table_count = 0;
    unsigned i = 0;
    struct Dwarf_Macro_Forms_s *curformentry = 0;
    Dwarf_Debug dbg = 0;

    if (!macro_context || macro_context->mc_sentinel != 0xada) {
        if(macro_context) {
            dbg = macro_context->mc_dbg;
        }
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_HEADER_POINTER);
        return DW_DLV_ERROR;
    }

    dbg = macro_context->mc_dbg;
    cur_offset = (1+ macro_data) - macro_header;
    if (cur_offset >= section_size) {
        _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
        return (DW_DLV_ERROR);
    }
    READ_UNALIGNED(dbg,operand_table_count,Dwarf_Small,
        macro_data,sizeof(Dwarf_Small));
    macro_data += sizeof(Dwarf_Small);
    /* Estimating minimum size */
    local_size = operand_table_count * 4;

    cur_offset = (local_size+ macro_data) - section_base;
    if (cur_offset >= section_size) {
        _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
        return (DW_DLV_ERROR);
    }
    /* first, get size of table. */
    table_data_start = macro_data;
    for (i = 0; i < operand_table_count; ++i) {
        /*  Compiler warning about unused opcode_number
            variable should be ignored. */
        Dwarf_Small opcode_number = 0;
        Dwarf_Unsigned formcount = 0;
        Dwarf_Word uleblen = 0;
        READ_UNALIGNED(dbg,opcode_number,Dwarf_Small,
            macro_data,sizeof(Dwarf_Small));
        macro_data += sizeof(Dwarf_Small);

        formcount = _dwarf_decode_u_leb128(macro_data,
            &uleblen);
        macro_data += uleblen;
        cur_offset = (formcount+ macro_data) - section_base;
        if (cur_offset >= section_size) {
            _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
            return (DW_DLV_ERROR);
        }
        /* The 1 ubyte forms follow. Step past them. */
        macro_data += formcount;
    }
    /* reset for reread. */
    macro_data = table_data_start;
    /* allocate table */
    macro_context->mc_opcode_forms =  (struct Dwarf_Macro_Forms_s *)
        calloc(operand_table_count,
            sizeof(struct Dwarf_Macro_Forms_s));
    macro_context->mc_opcode_count = operand_table_count;
    if(!macro_context->mc_opcode_forms) {
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return DW_DLV_ERROR;
    }

    curformentry = macro_context->mc_opcode_forms;
    for (i = 0; i < operand_table_count; ++i,++curformentry) {
        Dwarf_Small opcode_number = 0;
        Dwarf_Unsigned formcount = 0;
        Dwarf_Word uleblen = 0;

        cur_offset = (2 + macro_data) - section_base;
        if (cur_offset >= section_size) {
            _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
            return (DW_DLV_ERROR);
        }
        READ_UNALIGNED(dbg,opcode_number,Dwarf_Small,
            macro_data,sizeof(Dwarf_Small));
        macro_data += sizeof(Dwarf_Small);
        formcount = _dwarf_decode_u_leb128(macro_data,
            &uleblen);
        curformentry->mf_code = opcode_number;
        curformentry->mf_formcount = formcount;
        macro_data += uleblen;
        cur_offset = (formcount+ macro_data) - section_base;
        if (cur_offset >= section_size) {
            _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
            return (DW_DLV_ERROR);
        }
        curformentry->mf_formbytes = macro_data;
        macro_data += formcount;
        if (opcode_number  > DW_MACRO_undef_strx ) {
            Dwarf_Half k = 0;
            for(k = 0; k < formcount; ++k) {
                if (!valid_macro_form(curformentry->mf_formbytes[k])) {
                    _dwarf_error(dbg, error, DW_DLE_MACRO_OP_UNHANDLED);
                    return (DW_DLV_ERROR);
                }
            }
        }
        int res = validate_opcode(macro_context->mc_dbg,curformentry, error);
        if(res != DW_DLV_OK) {
            return res;
        }
    }
    *table_size_out = macro_data - table_data_start;
    return DW_DLV_OK;
}

static void
dealloc_srcfiles(Dwarf_Debug dbg,
    char ** srcfiles,
    Dwarf_Signed srcfiles_count)
{
    Dwarf_Signed i = 0;
    if (!dbg || !srcfiles_count) {
        return;
    }
    for (i = 0; i < srcfiles_count; ++i) {
        dwarf_dealloc(dbg, srcfiles[i], DW_DLA_STRING);
    }
    dwarf_dealloc(dbg, srcfiles, DW_DLA_LIST);
}

int
_dwarf_internal_macro_context(Dwarf_Die die,
    Dwarf_Bool        offset_specified,
    Dwarf_Unsigned    offset_in,
    Dwarf_Unsigned  * version_out,
    Dwarf_Macro_Context * macro_context_out,
    Dwarf_Unsigned      * macro_unit_offset_out,
    Dwarf_Unsigned      * macro_ops_count_out,
    Dwarf_Unsigned      * macro_ops_data_length,
    Dwarf_Error * error)
{
    Dwarf_CU_Context   cu_context = 0;

    /*  The Dwarf_Debug this die belongs to. */
    Dwarf_Debug dbg = 0;
    int resattr = DW_DLV_ERROR;
    int lres = DW_DLV_ERROR;
    int res = DW_DLV_ERROR;
    Dwarf_Unsigned macro_offset = 0;
    Dwarf_Attribute macro_attr = 0;
    Dwarf_Signed srcfiles_count = 0;
    char ** srcfiles = 0;
    const char *comp_dir = 0;
    const char *comp_name = 0;

    /*  ***** BEGIN CODE ***** */
    if (error != NULL) {
        *error = NULL;
    }

    CHECK_DIE(die, DW_DLV_ERROR);
    cu_context = die->di_cu_context;
    dbg = cu_context->cc_dbg;

    /*  Doing the load here results in duplication of the
        section-load call  (in the by_offset
        interface below) but detects the missing section
        quickly. */
    res = _dwarf_load_section(dbg, &dbg->de_debug_macro,error);
    if (res != DW_DLV_OK) {
        return res;
    }
    if (!dbg->de_debug_macro.dss_size) {
        return (DW_DLV_NO_ENTRY);
    }
    resattr = dwarf_attr(die, DW_AT_macros, &macro_attr, error);
    if (resattr == DW_DLV_NO_ENTRY) {
        resattr = dwarf_attr(die, DW_AT_GNU_macros, &macro_attr, error);
    }
    if (resattr != DW_DLV_OK) {
        return resattr;
    }
    if (!offset_specified) {
        lres = dwarf_global_formref(macro_attr, &macro_offset, error);
        if (lres != DW_DLV_OK) {
            return lres;
        }
    } else {
        macro_offset = offset_in;
    }
    lres = dwarf_srcfiles(die,&srcfiles,&srcfiles_count, error);
    if (lres == DW_DLV_ERROR) {
        return lres;
    }
    lres = _dwarf_internal_get_die_comp_dir(die, &comp_dir,
        &comp_name,error);
    if (resattr == DW_DLV_ERROR) {
        return lres;
    }
    *macro_unit_offset_out = macro_offset;

    /*  NO ENTRY or OK we accept, though NO ENTRY means there
        are no source files available. */
    lres = _dwarf_internal_macro_context_by_offset(dbg,
        macro_offset,version_out,macro_context_out,
        macro_ops_count_out,
        macro_ops_data_length,
        srcfiles,srcfiles_count,
        comp_dir,
        comp_name,
        error);
    return lres;
}

int
_dwarf_internal_macro_context_by_offset(Dwarf_Debug dbg,
    Dwarf_Unsigned offset,
    Dwarf_Unsigned  * version_out,
    Dwarf_Macro_Context * macro_context_out,
    Dwarf_Unsigned      * macro_ops_count_out,
    Dwarf_Unsigned      * macro_ops_data_length,
    char **srcfiles,
    Dwarf_Signed srcfilescount,
    const char *comp_dir,
    const char *comp_name,
    Dwarf_Error * error)
{
    Dwarf_Unsigned line_table_offset = 0;
    Dwarf_Small * macro_header = 0;
    Dwarf_Small * macro_data = 0;
    Dwarf_Half version = 0;
    Dwarf_Small flags = 0;
    Dwarf_Small offset_size = 4;
    Dwarf_Unsigned cur_offset = 0;
    Dwarf_Unsigned section_size = 0;
    Dwarf_Small *section_base = 0;
    Dwarf_Unsigned optablesize = 0;
    Dwarf_Unsigned macro_offset = offset;
    int res = 0;
    Dwarf_Macro_Context macro_context = 0;
    Dwarf_Bool build_ops_array = FALSE;

    res = _dwarf_load_section(dbg, &dbg->de_debug_macro,error);
    if (res != DW_DLV_OK) {
        dealloc_srcfiles(dbg, srcfiles, srcfilescount);
        return res;
    }
    if (!dbg->de_debug_macro.dss_size) {
        dealloc_srcfiles(dbg, srcfiles, srcfilescount);
        return (DW_DLV_NO_ENTRY);
    }

    section_base = dbg->de_debug_macro.dss_data;
    section_size = dbg->de_debug_macro.dss_size;
    /*  The '3'  ensures the header initial bytes present too. */
    if ((3+macro_offset) >= section_size) {
        dealloc_srcfiles(dbg, srcfiles, srcfilescount);
        _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
        return (DW_DLV_ERROR);
    }
    macro_header = macro_offset + section_base;
    macro_data = macro_header;


    macro_context = (Dwarf_Macro_Context)
        _dwarf_get_alloc(dbg,DW_DLA_MACRO_CONTEXT,1);
    if (!macro_context) {
        dealloc_srcfiles(dbg, srcfiles, srcfilescount);
        _dwarf_error(dbg, error, DW_DLE_ALLOC_FAIL);
        return DW_DLV_ERROR;
    }

    READ_UNALIGNED(dbg,version, Dwarf_Half,
        macro_data,sizeof(Dwarf_Half));
    macro_data += sizeof(Dwarf_Half);
    READ_UNALIGNED(dbg,flags, Dwarf_Small,
        macro_data,sizeof(Dwarf_Small));
    macro_data += sizeof(Dwarf_Small);

    macro_context->mc_at_comp_dir = comp_dir;
    macro_context->mc_at_name = comp_name;
    macro_context->mc_srcfiles = srcfiles;
    macro_context->mc_srcfiles_count = srcfilescount;
    macro_context->mc_macro_header = macro_header;
    macro_context->mc_section_offset = macro_offset;
    macro_context->mc_version_number = version;
    macro_context->mc_flags = flags;
    macro_context->mc_dbg = dbg;
    macro_context->mc_offset_size_flag =
        flags& MACRO_OFFSET_SIZE_FLAG?TRUE:FALSE;
    macro_context->mc_debug_line_offset_flag =
        flags& MACRO_LINE_OFFSET_FLAG?TRUE:FALSE;
    macro_context->mc_operands_table_flag =
        flags& MACRO_OP_TABLE_FLAG?TRUE:FALSE;
    offset_size = macro_context->mc_offset_size_flag?8:4;
    macro_context->mc_offset_size = offset_size;
    if (macro_context->mc_debug_line_offset_flag) {
        cur_offset = (offset_size+ macro_data) - section_base;
        if (cur_offset >= section_size) {
            dwarf_dealloc_macro_context(macro_context);
            _dwarf_error(dbg, error, DW_DLE_MACRO_OFFSET_BAD);
            return (DW_DLV_ERROR);
        }
        READ_UNALIGNED(dbg,line_table_offset,Dwarf_Unsigned,
            macro_data,offset_size);
        macro_data += offset_size;
        macro_context->mc_debug_line_offset = line_table_offset;
    }
    if (macro_context->mc_operands_table_flag) {
        res = read_operands_table(macro_context,
            macro_header,
            macro_data,
            section_base,
            section_size,
            &optablesize,
            error);
        if (res != DW_DLV_OK) {
            dwarf_dealloc_macro_context(macro_context);
            return res;
        }
    }

    macro_data += optablesize;
    macro_context->mc_macro_ops = macro_data;
    macro_context->mc_macro_header_length =macro_data - macro_header;

    build_ops_array = FALSE;
    res = _dwarf_get_macro_ops_count_internal(macro_context,
        build_ops_array,
        error);
    if (res != DW_DLV_OK) {
        dwarf_dealloc_macro_context(macro_context);
        return res;
    }
    build_ops_array = TRUE;
    res = _dwarf_get_macro_ops_count_internal(macro_context,
        build_ops_array,
        error);
    if (res != DW_DLV_OK) {
        dwarf_dealloc_macro_context(macro_context);
        return res;
    }
    *macro_ops_count_out = macro_context->mc_macro_ops_count;
    *macro_ops_data_length = macro_context->mc_ops_data_length;
    *version_out = version;
    *macro_context_out = macro_context;
    return DW_DLV_OK;
}

int dwarf_macro_context_head(Dwarf_Macro_Context head,
    Dwarf_Half     * version,
    Dwarf_Unsigned * mac_offset,
    Dwarf_Unsigned * mac_len,
    Dwarf_Unsigned * mac_header_len,
    unsigned *       flags,
    Dwarf_Bool *     has_line_offset,
    Dwarf_Unsigned * line_offset,
    Dwarf_Bool *     has_offset_size_64,
    Dwarf_Bool *     has_operands_table,
    Dwarf_Half *     opcode_count,
    Dwarf_Error *error)
{
    if (!head || head->mc_sentinel != 0xada) {
        Dwarf_Debug dbg = 0;
        if(head) {
            dbg = head->mc_dbg;
        }
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_HEADER_POINTER);
        return DW_DLV_ERROR;
    }
    *version = head->mc_version_number;
    *mac_offset = head->mc_section_offset;
    *mac_len    = head->mc_total_length;
    *mac_header_len = head->mc_macro_header_length;
    *flags = head->mc_flags;
    *line_offset = head->mc_debug_line_offset;
    *has_line_offset = head->mc_debug_line_offset_flag;
    *has_offset_size_64 = head->mc_offset_size_flag;
    *has_operands_table = head->mc_operands_table_flag;
    *opcode_count = head->mc_opcode_count;
    return DW_DLV_OK;
}
int dwarf_macro_operands_table(Dwarf_Macro_Context head,
    Dwarf_Half  index, /* 0 to opcode_count -1 */
    Dwarf_Half  *opcode_number,
    Dwarf_Half  *operand_count,
    const Dwarf_Small **operand_array,
    Dwarf_Error *error)
{
    struct Dwarf_Macro_Forms_s * ops = 0;
    Dwarf_Debug dbg = 0;
    if (!head || head->mc_sentinel != 0xada) {
        if(head) {
            dbg = head->mc_dbg;
        }
        _dwarf_error(dbg, error,DW_DLE_BAD_MACRO_HEADER_POINTER);
        return DW_DLV_ERROR;
    }
    dbg = head->mc_dbg;
    if (index < 0 || index >= head->mc_opcode_count) {
        _dwarf_error(dbg, error, DW_DLE_BAD_MACRO_INDEX);
        return DW_DLV_ERROR;
    }
    ops = head->mc_opcode_forms + index;
    *opcode_number = ops->mf_code;
    *operand_count = ops->mf_formcount;
    *operand_array = ops->mf_formbytes;
    return DW_DLV_OK;
}

/*  The base interface to the .debug_macro section data
    for a specific CU.

    The version number passed back by *version_out
    may be 4 (a gnu extension of DWARF)  or 5. */
int
dwarf_get_macro_context(Dwarf_Die cu_die,
    Dwarf_Unsigned      * version_out,
    Dwarf_Macro_Context * macro_context,
    Dwarf_Unsigned      * macro_unit_offset_out,
    Dwarf_Unsigned      * macro_ops_count_out,
    Dwarf_Unsigned      * macro_ops_data_length,
    Dwarf_Error * error)
{
    int res = 0;
    Dwarf_Bool offset_specified = FALSE;
    Dwarf_Unsigned offset = 0;

    res =  _dwarf_internal_macro_context(cu_die,
        offset_specified,
        offset,
        version_out,
        macro_context,
        macro_unit_offset_out,
        macro_ops_count_out,
        macro_ops_data_length,
        error);
    return res;
}

/*  Like  dwarf_get_macro_context but
    here we use a specfied offset  instead  of
    the offset in the cu_die. */
int
dwarf_get_macro_context_by_offset(Dwarf_Die cu_die,
    Dwarf_Unsigned offset,
    Dwarf_Unsigned      * version_out,
    Dwarf_Macro_Context * macro_context,
    Dwarf_Unsigned      * macro_ops_count_out,
    Dwarf_Unsigned      * macro_ops_data_length,
    Dwarf_Error * error)
{
    int res = 0;
    Dwarf_Bool offset_specified = TRUE;
    Dwarf_Unsigned macro_unit_offset_out = 0;

    res = _dwarf_internal_macro_context(cu_die,
        offset_specified,
        offset,
        version_out,
        macro_context,
        &macro_unit_offset_out,
        macro_ops_count_out,
        macro_ops_data_length,
        error);
    return res;
}

int dwarf_get_macro_section_name(Dwarf_Debug dbg,
   const char **sec_name_out,
   Dwarf_Error *error)
{
    struct Dwarf_Section_s *sec = 0;

    sec = &dbg->de_debug_macro;
    if (sec->dss_size == 0) {
        /* We don't have such a  section at all. */
        return DW_DLV_NO_ENTRY;
    }
    *sec_name_out = sec->dss_name;
    return DW_DLV_OK;
}

void
dwarf_dealloc_macro_context(Dwarf_Macro_Context mc)
{
    Dwarf_Debug dbg = mc->mc_dbg;
    dwarf_dealloc(dbg,mc,DW_DLA_MACRO_CONTEXT);
}

int
_dwarf_macro_constructor(Dwarf_Debug dbg, void *m)
{
    /* Nothing to do */
    Dwarf_Macro_Context mc= (Dwarf_Macro_Context)m;
    /* Arbitrary sentinel. For debugging. */
    mc->mc_sentinel = 0xada;
    mc->mc_dbg = dbg;
    return DW_DLV_OK;
}
void
_dwarf_macro_destructor(void *m)
{
    Dwarf_Macro_Context mc= (Dwarf_Macro_Context)m;
    Dwarf_Debug dbg = 0;

    dbg = mc->mc_dbg;
    dealloc_srcfiles(dbg, mc->mc_srcfiles, mc->mc_srcfiles_count);
    mc->mc_srcfiles = 0;
    mc->mc_srcfiles_count = 0;
    free((void *)mc->mc_file_path);
    mc->mc_file_path = 0;
    free(mc->mc_ops);
    mc->mc_ops = 0;
    free(mc->mc_opcode_forms);
    mc->mc_opcode_forms = 0;
    memset(mc,0,sizeof(*mc));
    /* Just a recognizable sentinel. For debugging.  No real meaning. */
    mc->mc_sentinel = 0xdeadbeef;
}
