/* debug.c -- Handle generic debugging information.
   Copyright (C) 1995, 1996, 1997, 1998, 1999 Free Software Foundation, Inc.
   Written by Ian Lance Taylor <ian@cygnus.com>.

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* This file implements a generic debugging format.  We may eventually
   have readers which convert different formats into this generic
   format, and writers which write it out.  The initial impetus for
   this was writing a convertor from stabs to HP IEEE-695 debugging
   format.  */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "log.h"
#include "symbols.h"

#include "bfd.h"
#include "demangle.h"
#include "libiberty.h"
#include "debug.h"

#define MAX_ARRAY_SIZE	20
#define MAX_STRING_SIZE	512

/* Global information we keep for debugging.  A pointer to this
   structure is the debugging handle passed to all the routines.  */

struct debug_handle
{
	/* A linked list of compilation units.  */
	struct debug_unit *units;
	/* The current compilation unit.  */
	struct debug_unit *current_unit;
	/* The current source file.  */
	struct debug_file *current_file;
	/* The current function.  */
	struct debug_function *current_function;
	/* The current block.  */
	struct debug_block *current_block;
	/* The current line number information for the current unit.  */
	struct debug_lineno *current_lineno;
	/* Mark.  This is used by debug_write.  */
	unsigned int mark;
	/* A struct/class ID used by debug_write.  */
	unsigned int class_id;
	/* The base for class_id for this call to debug_write.  */
	unsigned int base_id;
	/* The current line number in debug_write.  */
	struct debug_lineno *current_write_lineno;
	unsigned int current_write_lineno_index;
	/* A list of classes which have assigned ID's during debug_write.
	   This is linked through the next_id field of debug_class_type.  */
	struct debug_class_id *id_list;
	/* A list used to avoid recursion during debug_type_samep.  */
	struct debug_type_compare_list *compare_list;
};

/* Information we keep for a single compilation unit.  */

struct debug_unit
{
	/* The next compilation unit.  */
	struct debug_unit *next;
	/* A list of files included in this compilation unit.  The first
	   file is always the main one, and that is where the main file name
	   is stored.  */
	struct debug_file *files;
	/* Line number information for this compilation unit.  This is not
	   stored by function, because assembler code may have line number
	   information without function information.  */
	struct debug_lineno *linenos;
};

/* Information kept for a single source file.  */

struct debug_file
{
	/* The next source file in this compilation unit.  */
	struct debug_file *next;
	/* The name of the source file.  */
	const char *filename;
	/* Global functions, variables, types, etc.  */
	struct debug_namespace *globals;
};

/* A type.  */

struct debug_type
{
	/* Kind of type.  */
	enum debug_type_kind kind;
	/* Size of type (0 if not known).  */
	unsigned int size;
	/* Type which is a pointer to this type.  */
	debug_type pointer;
	/* Tagged union with additional information about the type.  */
	union
	{
		/* DEBUG_KIND_INDIRECT.  */
		struct debug_indirect_type *kindirect;
		/* DEBUG_KIND_INT.  */
		/* Whether the integer is unsigned.  */
		bfd_boolean kint;
		/* DEBUG_KIND_STRUCT, DEBUG_KIND_UNION, DEBUG_KIND_CLASS,
		   DEBUG_KIND_UNION_CLASS.  */
		struct debug_class_type *kclass;
		/* DEBUG_KIND_ENUM.  */
		struct debug_enum_type *kenum;
		/* DEBUG_KIND_POINTER.  */
		struct debug_type *kpointer;
		/* DEBUG_KIND_FUNCTION.  */
		struct debug_function_type *kfunction;
		/* DEBUG_KIND_REFERENCE.  */
		struct debug_type *kreference;
		/* DEBUG_KIND_RANGE.  */
		struct debug_range_type *krange;
		/* DEBUG_KIND_ARRAY.  */
		struct debug_array_type *karray;
		/* DEBUG_KIND_SET.  */
		struct debug_set_type *kset;
		/* DEBUG_KIND_OFFSET.  */
		struct debug_offset_type *koffset;
		/* DEBUG_KIND_METHOD.  */
		struct debug_method_type *kmethod;
		/* DEBUG_KIND_CONST.  */
		struct debug_type *kconst;
		/* DEBUG_KIND_VOLATILE.  */
		struct debug_type *kvolatile;
		/* DEBUG_KIND_NAMED, DEBUG_KIND_TAGGED.  */
		struct debug_named_type *knamed;
	} u;
};

/* Information kept for an indirect type.  */

struct debug_indirect_type
{
	/* Slot where the final type will appear.  */
	debug_type *slot;
	/* Tag.  */
	const char *tag;
};

/* Information kept for a struct, union, or class.  */

struct debug_class_type
{
	/* NULL terminated array of fields.  */
	debug_field *fields;
	/* A mark field which indicates whether the struct has already been
	   printed.  */
	unsigned int mark;
	/* This is used to uniquely identify unnamed structs when printing.  */
	unsigned int id;
	/* The remaining fields are only used for DEBUG_KIND_CLASS and
	   DEBUG_KIND_UNION_CLASS.  */
	/* NULL terminated array of base classes.  */
	debug_baseclass *baseclasses;
	/* NULL terminated array of methods.  */
	debug_method *methods;
	/* The type of the class providing the virtual function table for
	   this class.  This may point to the type itself.  */
	debug_type vptrbase;
};

/* Information kept for an enum.  */

struct debug_enum_type
{
	/* NULL terminated array of names.  */
	const char **names;
	/* Array of corresponding values.  */
	bfd_signed_vma *values;
};

/* Information kept for a function.  FIXME: We should be able to
   record the parameter types.  */

struct debug_function_type
{
	/* Return type.  */
	debug_type return_type;
	/* NULL terminated array of argument types.  */
	debug_type *arg_types;
	/* Whether the function takes a variable number of arguments.  */
	bfd_boolean varargs;
};

/* Information kept for a range.  */

struct debug_range_type
{
	/* Range base type.  */
	debug_type type;
	/* Lower bound.  */
	bfd_signed_vma lower;
	/* Upper bound.  */
	bfd_signed_vma upper;
};

/* Information kept for an array.  */

struct debug_array_type
{
	/* Element type.  */
	debug_type element_type;
	/* Range type.  */
	debug_type range_type;
	/* Lower bound.  */
	bfd_signed_vma lower;
	/* Upper bound.  */
	bfd_signed_vma upper;
	/* Whether this array is really a string.  */
	bfd_boolean stringp;
};

/* Information kept for a set.  */

struct debug_set_type
{
	/* Base type.  */
	debug_type type;
	/* Whether this set is really a bitstring.  */
	bfd_boolean bitstringp;
};

/* Information kept for an offset type (a based pointer).  */

struct debug_offset_type
{
	/* The type the pointer is an offset from.  */
	debug_type base_type;
	/* The type the pointer points to.  */
	debug_type target_type;
};

/* Information kept for a method type.  */

struct debug_method_type
{
	/* The return type.  */
	debug_type return_type;
	/* The object type which this method is for.  */
	debug_type domain_type;
	/* A NULL terminated array of argument types.  */
	debug_type *arg_types;
	/* Whether the method takes a variable number of arguments.  */
	bfd_boolean varargs;
};

/* Information kept for a named type.  */

struct debug_named_type
{
	/* Name.  */
	struct debug_name *name;
	/* Real type.  */
	debug_type type;
};

/* A field in a struct or union.  */

struct debug_field
{
	/* Name of the field.  */
	const char *name;
	/* Type of the field.  */
	struct debug_type *type;
	/* Visibility of the field.  */
	enum debug_visibility visibility;
	/* Whether this is a static member.  */
	bfd_boolean static_member;
	union
	{
		/* If static_member is FALSE.  */
		struct
		{
			/* Bit position of the field in the struct.  */
			unsigned int bitpos;
			/* Size of the field in bits.  */
			unsigned int bitsize;
		} f;
		/* If static_member is TRUE.  */
		struct
		{
			const char *physname;
		} s;
	} u;
};

/* A base class for an object.  */

struct debug_baseclass
{
	/* Type of the base class.  */
	struct debug_type *type;
	/* Bit position of the base class in the object.  */
	unsigned int bitpos;
	/* Whether the base class is virtual.  */
	bfd_boolean virtual;
	/* Visibility of the base class.  */
	enum debug_visibility visibility;
};

/* A method of an object.  */

struct debug_method
{
	/* The name of the method.  */
	const char *name;
	/* A NULL terminated array of different types of variants.  */
	struct debug_method_variant **variants;
};

/* The variants of a method function of an object.  These indicate
   which method to run.  */

struct debug_method_variant
{
	/* The physical name of the function.  */
	const char *physname;
	/* The type of the function.  */
	struct debug_type *type;
	/* The visibility of the function.  */
	enum debug_visibility visibility;
	/* Whether the function is const.  */
	bfd_boolean constp;
	/* Whether the function is volatile.  */
	bfd_boolean volatilep;
	/* The offset to the function in the virtual function table.  */
	bfd_vma voffset;
	/* If voffset is VOFFSET_STATIC_METHOD, this is a static method.  */
#define VOFFSET_STATIC_METHOD ((bfd_vma) -1)
	/* Context of a virtual method function.  */
	struct debug_type *context;
};

/* A variable.  This is the information we keep for a variable object.
   This has no name; a name is associated with a variable in a
   debug_name structure.  */

struct debug_variable
{
	/* Kind of variable.  */
	enum debug_var_kind kind;
	/* Type.  */
	debug_type type;
	/* Value.  The interpretation of the value depends upon kind.  */
	bfd_vma val;
};

/* A function.  This has no name; a name is associated with a function
   in a debug_name structure.  */

struct debug_function
{
	/* Return type.  */
	debug_type return_type;
	/* Parameter information.  */
	struct debug_parameter *parameters;
	/* Block information.  The first structure on the list is the main
	   block of the function, and describes function local variables.  */
	struct debug_block *blocks;
};

/* A function parameter.  */

struct debug_parameter
{
	/* Next parameter.  */
	struct debug_parameter *next;
	/* Name.  */
	const char *name;
	/* Type.  */
	debug_type type;
	/* Kind.  */
	enum debug_parm_kind kind;
	/* Value (meaning depends upon kind).  */
	bfd_vma val;
};

/* A typed constant.  */

struct debug_typed_constant
{
	/* Type.  */
	debug_type type;
	/* Value.  FIXME: We may eventually need to support non-integral
	   values.  */
	bfd_vma val;
};

/* Information about a block within a function.  */

struct debug_block
{
	/* Next block with the same parent.  */
	struct debug_block *next;
	/* Parent block.  */
	struct debug_block *parent;
	/* List of child blocks.  */
	struct debug_block *children;
	/* Start address of the block.  */
	bfd_vma start;
	/* End address of the block.  */
	bfd_vma end;
	/* Local variables.  */
	struct debug_namespace *locals;
};

/* Line number information we keep for a compilation unit.  FIXME:
   This structure is easy to create, but can be very space
   inefficient.  */

struct debug_lineno
{
	/* More line number information for this block.  */
	struct debug_lineno *next;
	/* Source file.  */
	struct debug_file *file;
	/* Line numbers, terminated by a -1 or the end of the array.  */
#define DEBUG_LINENO_COUNT 10
	unsigned long linenos[DEBUG_LINENO_COUNT];
	/* Addresses for the line numbers.  */
	bfd_vma addrs[DEBUG_LINENO_COUNT];
};

/* A namespace.  This is a mapping from names to objects.  FIXME: This
   should be implemented as a hash table.  */

struct debug_namespace
{
	/* List of items in this namespace.  */
	struct debug_name *list;
	/* Pointer to where the next item in this namespace should go.  */
	struct debug_name **tail;
};

/* Kinds of objects that appear in a namespace.  */

enum debug_object_kind
{
	/* A type.  */
	DEBUG_OBJECT_TYPE,
	/* A tagged type (really a different sort of namespace).  */
	DEBUG_OBJECT_TAG,
	/* A variable.  */
	DEBUG_OBJECT_VARIABLE,
	/* A function.  */
	DEBUG_OBJECT_FUNCTION,
	/* An integer constant.  */
	DEBUG_OBJECT_INT_CONSTANT,
	/* A floating point constant.  */
	DEBUG_OBJECT_FLOAT_CONSTANT,
	/* A typed constant.  */
	DEBUG_OBJECT_TYPED_CONSTANT
};

/* Linkage of an object that appears in a namespace.  */

enum debug_object_linkage
{
	/* Local variable.  */
	DEBUG_LINKAGE_AUTOMATIC,
	/* Static--either file static or function static, depending upon the
	   namespace is.  */
	DEBUG_LINKAGE_STATIC,
	/* Global.  */
	DEBUG_LINKAGE_GLOBAL,
	/* No linkage.  */
	DEBUG_LINKAGE_NONE
};

/* A name in a namespace.  */

struct debug_name
{
	/* Next name in this namespace.  */
	struct debug_name *next;
	/* Name.  */
	const char *name;
	/* Mark.  This is used by debug_write.  */
	unsigned int mark;
	/* Kind of object.  */
	enum debug_object_kind kind;
	/* Linkage of object.  */
	enum debug_object_linkage linkage;
	/* Tagged union with additional information about the object.  */
	union
	{
		/* DEBUG_OBJECT_TYPE.  */
		struct debug_type *type;
		/* DEBUG_OBJECT_TAG.  */
		struct debug_type *tag;
		/* DEBUG_OBJECT_VARIABLE.  */
		struct debug_variable *variable;
		/* DEBUG_OBJECT_FUNCTION.  */
		struct debug_function *function;
		/* DEBUG_OBJECT_INT_CONSTANT.  */
		bfd_vma int_constant;
		/* DEBUG_OBJECT_FLOAT_CONSTANT.  */
		double float_constant;
		/* DEBUG_OBJECT_TYPED_CONSTANT.  */
		struct debug_typed_constant *typed_constant;
	} u;
};

/* During debug_write, a linked list of these structures is used to
   keep track of ID numbers that have been assigned to classes.  */

struct debug_class_id
{
	/* Next ID number.  */
	struct debug_class_id *next;
	/* The type with the ID.  */
	struct debug_type *type;
	/* The tag; NULL if no tag.  */
	const char *tag;
};

/* During debug_type_samep, a linked list of these structures is kept
   on the stack to avoid infinite recursion.  */

struct debug_type_compare_list
{
	/* Next type on list.  */
	struct debug_type_compare_list *next;
	/* The types we are comparing.  */
	struct debug_type *t1;
	struct debug_type *t2;
};

/* During debug_get_real_type, a linked list of these structures is
   kept on the stack to avoid infinite recursion.  */

struct debug_type_real_list
{
	/* Next type on list.  */
	struct debug_type_real_list *next;
	/* The type we are checking.  */
	struct debug_type *t;
};

/* Local functions.  */

static void debug_error (const char *);
static struct debug_type *debug_get_real_type (PTR, debug_type, struct debug_type_real_list *);
static bfd_boolean print_type (struct debug_handle *info, struct debug_type *type, struct debug_name *name);
static bfd_boolean print_value (bfd *abfd, asymbol **syms, long symcount, struct debug_handle *info, struct debug_type *type, struct debug_name *name, HANDLE hprocess, DWORD addr, int level, int indent);

/* Issue an error message.  */

static void
debug_error (message)
	   const char *message;
{
	fprintf (stderr, "%s\n", message);
}

/* Get a base type.  We build a linked list on the stack to avoid
   crashing if the type is defined circularly.  */

static struct debug_type * debug_get_real_type (PTR handle, debug_type type, struct debug_type_real_list *list)
{
	struct debug_type_real_list *l;
	struct debug_type_real_list rl;

	switch (type->kind)
	{
		default:
			return type;

		case DEBUG_KIND_INDIRECT:
		case DEBUG_KIND_NAMED:
		case DEBUG_KIND_TAGGED:
			break;
	}

	for (l = list; l != NULL; l = l->next)
	{
		if (l->t == type)
		{
			fprintf (stderr, "debug_get_real_type: circular debug information for %s\n", debug_get_type_name (handle, type));
			return NULL;
		}
	}

	rl.next = list;
	rl.t = type;

	switch (type->kind)
	{
		default:
			/* The default case is just here to avoid warnings.  */
		case DEBUG_KIND_INDIRECT:
			if (*type->u.kindirect->slot != NULL)
				return debug_get_real_type (handle, *type->u.kindirect->slot, &rl);
			return type;
		case DEBUG_KIND_NAMED:
		case DEBUG_KIND_TAGGED:
			return debug_get_real_type (handle, type->u.knamed->type, &rl);
	}
	/* NOTREACHED */
}

bfd_boolean
get_line_from_addr (bfd *abfd, asymbol **syms, long symcount, PTR dhandle, bfd_vma address, char *filename, unsigned int nsize, unsigned int *lineno)
{
	struct debug_handle *info = (struct debug_handle *) dhandle;
	struct debug_unit *u;
	struct debug_lineno *closest_lineno = NULL;
	unsigned int closest_lineno_index = 0;

	for (u = info->units; u != NULL; u = u->next)
	{
		struct debug_lineno *current_lineno;
		unsigned int current_lineno_index;
		
		current_lineno = u->linenos;
		current_lineno_index = 0;

		while (current_lineno != NULL)
		{
			while (current_lineno_index < DEBUG_LINENO_COUNT)
			{
				if (current_lineno->linenos[current_lineno_index] == (unsigned long) -1)
					break;
			
				
				if ((!closest_lineno || current_lineno->addrs[current_lineno_index] > closest_lineno->addrs[closest_lineno_index]) && current_lineno->addrs[current_lineno_index] <= address)
				{
					closest_lineno = current_lineno;
					closest_lineno_index = current_lineno_index;
				}

				++current_lineno_index;
			}
			
			current_lineno = current_lineno->next;
			current_lineno_index = 0;
		}
	}
	
	if(closest_lineno)
	{
		lstrcpyn(filename, closest_lineno->file->filename, nsize);
		*lineno = closest_lineno->linenos[closest_lineno_index];
		return TRUE;
	}
	else
		return FALSE;
}

static void 
print_indent(unsigned indent)
{
	unsigned i;
	
	lprintf("\r\n");
	for(i = indent; i; --i)
		lprintf("\t");
}


/* Write out the debugging information.  This is given a handle to
   debugging information, and a set of function pointers to call.  */

const char *parameter_name = NULL;

bfd_boolean
print_function_info (bfd *abfd, asymbol **syms, long symcount, PTR dhandle, HANDLE hprocess, const char *function_name, DWORD framepointer)
{
	struct debug_handle *info = (struct debug_handle *) dhandle;
	struct debug_unit *u;
	int indent = 0;

	for (u = info->units; u != NULL; u = u->next)
	{
		struct debug_file *f;

		for (f = u->files; f != NULL; f = f->next)
		{
			struct debug_name *n;

			if (f->globals != NULL)
			{
				for (n = f->globals->list; n != NULL; n = n->next)
				{
					if(n->kind == DEBUG_OBJECT_FUNCTION && !strcmp(n->name, function_name))
					{
						struct debug_function *function;
						struct debug_parameter *p;
						struct debug_block *b;
						char *res;
					
						function = n->u.function;
						
						if( n->linkage == DEBUG_LINKAGE_STATIC)
							lprintf("static ");
						
						if (! print_type (info, function->return_type, (struct debug_name *) NULL))
							return FALSE;
					
						if((res = cplus_demangle(n->name, DMGL_ANSI)))
						{
							lprintf(" %s", res);
							free (res);							
						}
						else
							lprintf(" %s", n->name);
							
						lprintf("(");
						print_indent(++indent);
					
						for (p = function->parameters; p != NULL; p = p->next)
						{
							parameter_name = n->name;
							
							
							if (! print_type (info, p->type, (struct debug_name *) NULL))
								return FALSE;
							
							lprintf(" %s = ", p->name);
							//lprintf(" %s /* %li */ = ", p->name, p->val);

							if (! print_value (abfd, syms, symcount, info, p->type, (struct debug_name *) NULL, hprocess, p->kind == DEBUG_PARM_STACK ? framepointer + p->val : 0, 1, indent))
								return FALSE;

							if(p->next)
							{
								lprintf(",");
								print_indent(indent);
							}
						}
						
						print_indent(--indent);
						lprintf(")");

						for (b = function->blocks; b != NULL; b = b->next)
						{
							//if (! print_block (info, b))
							//	return FALSE;
						}
					}
				}
			}
		}
	}

	return TRUE;
}

/* Write out a class type.  */
#if 0
static bfd_boolean
print_class_type (struct debug_handle *info, struct debug_type *type, const char *tag)
{
	unsigned int i;
	unsigned int id;
	struct debug_type *vptrbase;

	if (type->u.kclass == NULL)
	{
		id = 0;
		vptrbase = NULL;
	}
	else
	{
		/*
		if (type->u.kclass->id <= info->base_id)
		{
			if (! debug_set_class_id (info, tag, type))
				return FALSE;
		}
		*/
		/*
		if (info->mark == type->u.kclass->mark)
		{
			// We are currently outputting this class, or we have
			// already output it.  This can happen when there are
			// methods for an anonymous class.  
			assert (type->u.kclass->id > info->base_id);
			return (*fns->tag_type) (fhandle, tag, type->u.kclass->id, type->kind);
		}
		type->u.kclass->mark = info->mark;
		id = type->u.kclass->id;*/

		vptrbase = type->u.kclass->vptrbase;
		if (vptrbase != NULL && vptrbase != type)
		{
			if (! print_type (info, vptrbase, (struct debug_name *) NULL))
				return FALSE;
		}
	}

	//if (! (*fns->start_class_type) (fhandle, tag, id, type->kind == DEBUG_KIND_CLASS, type->size, vptrbase != NULL, vptrbase == type))
	//	return FALSE;
	lprintf(type->kind == DEBUG_KIND_CLASS ? "class" : "union");

	if (type->u.kclass != NULL)
	{
		if (type->u.kclass->fields != NULL)
		{
			for (i = 0; type->u.kclass->fields[i] != NULL; i++)
			{
				struct debug_field *f;

				f = type->u.kclass->fields[i];
				if (! print_type (info, f->type, (struct debug_name *) NULL))
					return FALSE;
				if (f->static_member)
				{
					
					if (! (*fns->class_static_member) (fhandle, f->name, f->u.s.physname, f->visibility))
						return FALSE;
				}
				else
				{
					if (! (*fns->struct_field) (fhandle, f->name, f->u.f.bitpos, f->u.f.bitsize, f->visibility))
						return FALSE;
				}
			}
		}

		if (type->u.kclass->baseclasses != NULL)
		{
			for (i = 0; type->u.kclass->baseclasses[i] != NULL; i++)
			{
				struct debug_baseclass *b;

				b = type->u.kclass->baseclasses[i];
				if (! debug_write_type (info, b->type, (struct debug_name *) NULL))
					return FALSE;
				if (! (*fns->class_baseclass) (fhandle, b->bitpos, b->virtual, b->visibility))
					return FALSE;
			}
		}

		if (type->u.kclass->methods != NULL)
		{
			for (i = 0; type->u.kclass->methods[i] != NULL; i++)
			{
				struct debug_method *m;
				unsigned int j;

				m = type->u.kclass->methods[i];
				if (! (*fns->class_start_method) (fhandle, m->name))
					return FALSE;
				for (j = 0; m->variants[j] != NULL; j++)
				{
					struct debug_method_variant *v;

					v = m->variants[j];
					if (v->context != NULL)
					{
						if (! print_type (info, v->context, (struct debug_name *) NULL))
							return FALSE;
					}
					if (! print_type (info, v->type, (struct debug_name *) NULL))
						return FALSE;
					if (v->voffset != VOFFSET_STATIC_METHOD)
					{
						if (! (*fns->class_method_variant) (fhandle, v->physname, v->visibility, v->constp, v->volatilep, v->voffset, v->context != NULL))
							return FALSE;
					}
					else
					{
						if (! (*fns->class_static_method_variant) (fhandle, v->physname, v->visibility, v->constp, v->volatilep))
							return FALSE;
					}
				}
				if (! (*fns->class_end_method) (fhandle))
					return FALSE;
			}
		}
	}

	//return (*fns->end_class_type) (fhandle);
	lprintf("}");
	return TRUE;
}
#endif

/* Write out a type.  If the type is DEBUG_KIND_NAMED or
   DEBUG_KIND_TAGGED, then the name argument is the name for which we
   are about to call typedef or tag.  If the type is anything else,
   then the name argument is a tag from a DEBUG_KIND_TAGGED type which
   points to this one.  */

static bfd_boolean
print_type (struct debug_handle *info, struct debug_type *type, struct debug_name *name)
{
	unsigned int i;
	int is;
	const char *tag = NULL;

	// If we have a name for this type, just output it.
	// We output type tags whenever we are not actually defining them.
	if ((type->kind == DEBUG_KIND_NAMED || type->kind == DEBUG_KIND_TAGGED)	&& (type->u.knamed->name->mark == info->mark || (type->kind == DEBUG_KIND_TAGGED && type->u.knamed->name != name)))
	{
		if (type->kind == DEBUG_KIND_NAMED)
		{
			//return (*fns->typedef_type) (fhandle, type->u.knamed->name->name);
			lprintf("%s", type->u.knamed->name->name);
			return TRUE;
		}
		else
		{
			struct debug_type *real;

			real = debug_get_real_type ((PTR) info, type, NULL);
			if (real == NULL)
			{
				//return (*fns->empty_type) (fhandle);
				return TRUE;
			}

			//return (*fns->tag_type) (fhandle, type->u.knamed->name->name, id, real->kind);
			switch (real->kind)
			{
				case DEBUG_KIND_STRUCT:
					lprintf("struct ");
					break;
				case DEBUG_KIND_UNION:
					lprintf("union ");
					break;
				case DEBUG_KIND_ENUM:
					lprintf("enum ");
					break;
				case DEBUG_KIND_CLASS:
					lprintf("class ");
					break;
				case DEBUG_KIND_UNION_CLASS:
					lprintf("union class ");
					break;
				default:
					assert(0);
					return FALSE;
			}
			if(type->u.knamed->name->name)
				lprintf("%s", type->u.knamed->name->name);
			return TRUE;
		}
	}

	if (name != NULL && type->kind != DEBUG_KIND_NAMED && type->kind != DEBUG_KIND_TAGGED)
	{
		assert (name->kind == DEBUG_OBJECT_TAG);
		tag = name->name;
	}

	switch (type->kind)
	{
		case DEBUG_KIND_ILLEGAL:
			debug_error ("print_type: illegal type encountered");
			return FALSE;
		case DEBUG_KIND_INDIRECT:
			if (*type->u.kindirect->slot == DEBUG_TYPE_NULL)
			{
				//return (*fns->empty_type) (fhandle);
				return TRUE;
			}
			return print_type (info, *type->u.kindirect->slot, name);
		case DEBUG_KIND_VOID:
			//return (*fns->void_type) (fhandle);
			lprintf("void");			
			return TRUE;
		case DEBUG_KIND_INT:
			//return (*fns->int_type) (fhandle, type->size, type->u.kint);
  			lprintf ("%sint%d", type->u.kint ? "u" : "", type->size * 8);
			return TRUE;
		case DEBUG_KIND_FLOAT:
			//return (*fns->float_type) (fhandle, type->size);
			if (type->size == 4)
				lprintf ("float");
			else if (type->size == 8)
				lprintf ("double");
			else
				lprintf ("float%d", type->size * 8);
			return TRUE;
		case DEBUG_KIND_COMPLEX:
			//return (*fns->complex_type) (fhandle, type->size);
			lprintf("complex ");	
			if (type->size == 4)
				lprintf ("float");
			else if (type->size == 8)
				lprintf ("double");
			else
				lprintf ("float%d", type->size * 8);
			return TRUE;
		case DEBUG_KIND_BOOL:
			//return (*fns->bool_type) (fhandle, type->size);
			lprintf ("bool%d", type->size * 8);
			return TRUE;
		case DEBUG_KIND_STRUCT:
		case DEBUG_KIND_UNION:
			//(*fns->start_struct_type) (fhandle, tag, (type->u.kclass != NULL ? type->u.kclass->id : 0), type->kind == DEBUG_KIND_STRUCT, type->size)
			lprintf(type->kind == DEBUG_KIND_STRUCT ? "struct " : "union ");
			lprintf("{ ");
			if (type->u.kclass != NULL && type->u.kclass->fields != NULL)
			{
				for (i = 0; type->u.kclass->fields[i] != NULL; i++)
				{
					struct debug_field *f;

					if(i)
						lprintf(" ");

					f = type->u.kclass->fields[i];
					if (!print_type (info, f->type, (struct debug_name *) NULL))
						return FALSE;

					//(*fns->struct_field) (fhandle, f->name, f->u.f.bitpos, f->u.f.bitsize, f->visibility)
					lprintf(" %s;", f->name);
				}
			}
			//return (*fns->end_struct_type) (fhandle);
			lprintf(" }");
			return TRUE;
		case DEBUG_KIND_CLASS:
		case DEBUG_KIND_UNION_CLASS:
			//return debug_write_class_type (info, fns, fhandle, type, tag);
			return TRUE;
		case DEBUG_KIND_ENUM:
			//if (type->u.kenum == NULL)
			//	return (*fns->enum_type) (fhandle, tag, (const char **) NULL,	(bfd_signed_vma *) NULL);
			//return (*fns->enum_type) (fhandle, tag, type->u.kenum->names, type->u.kenum->values);
			lprintf("enum ");
			if(tag != NULL)
				lprintf("%s", tag);
			else
			{
				lprintf("{ ");
				if(type->u.kenum == NULL)
					lprintf("/* undefined */");
				else
				{
					int i;
					bfd_signed_vma val = 0;

					for (i = 0; type->u.kenum->names[i] != NULL; i++)
					{
						if (i)
							lprintf(", ");
						lprintf("%s", type->u.kenum->names[i]);
						if (type->u.kenum->values[i] != val)
						{
							lprintf(" = %li", (long) val);
							val = type->u.kenum->values[i];
						}
						++val;
					}
				}
				lprintf(" }");
			}
			return TRUE;
		case DEBUG_KIND_POINTER:
			if (! print_type (info, type->u.kpointer, (struct debug_name *) NULL))
				return FALSE;
			lprintf(" *");
			//return (*fns->pointer_type) (fhandle);
			return TRUE;
		case DEBUG_KIND_FUNCTION:
			//return (*fns->function_type) (fhandle, is, type->u.kfunction->varargs);
			{
				int i = 0;

				if (! print_type (info,	type->u.kfunction->return_type,	(struct debug_name *) NULL))
					return FALSE;
				lprintf(" ()(");
				if (type->u.kfunction->arg_types != NULL)
					for (; type->u.kfunction->arg_types[i] != NULL; i++)
					{
						if(i)
							lprintf(", ");
						if (! print_type (info,	type->u.kfunction->arg_types[i], (struct debug_name *) NULL))
							return FALSE;
					}
				if(!i)
					lprintf("void");
				lprintf(")");
				return TRUE;
			}
		case DEBUG_KIND_REFERENCE:
			if (! print_type (info, type->u.kreference, (struct debug_name *) NULL))
				return FALSE;
			//return (*fns->reference_type) (fhandle);
			lprintf(" &");
			return TRUE;
		case DEBUG_KIND_RANGE:
			if (! print_type (info, type->u.krange->type, (struct debug_name *) NULL))
				return FALSE;
			//return (*fns->range_type) (fhandle, type->u.krange->lower, type->u.krange->upper);
			lprintf("[]");
			return TRUE;
		case DEBUG_KIND_ARRAY:
			if (! print_type (info, type->u.karray->element_type, (struct debug_name *) NULL) || ! print_type (info, type->u.karray->range_type, (struct debug_name *) NULL))
				return FALSE;
			//return (*fns->array_type) (fhandle, type->u.karray->lower, type->u.karray->upper, type->u.karray->stringp);
			lprintf("[]");
			return TRUE;
		case DEBUG_KIND_SET:
			lprintf("set");
			if (! print_type (info, type->u.kset->type,	(struct debug_name *) NULL))
				return FALSE;
			//return (*fns->set_type) (fhandle, type->u.kset->bitstringp);
			return TRUE;
		case DEBUG_KIND_OFFSET:
			if (
				!print_type (info, type->u.koffset->base_type, (struct debug_name *) NULL)
				|| !print_type (info, type->u.koffset->target_type, (struct debug_name *) NULL)
			)
				return FALSE;
			//return (*fns->offset_type) (fhandle);
			lprintf("offset");
			return TRUE;
		case DEBUG_KIND_METHOD:
			if (! print_type (info, type->u.kmethod->return_type, (struct debug_name *) NULL))
				return FALSE;
			if (type->u.kmethod->arg_types == NULL)
				is = -1;
			else
			{
				for (is = 0; type->u.kmethod->arg_types[is] != NULL; is++)
					if (! print_type (info, 
																	type->u.kmethod->arg_types[is],
																	(struct debug_name *) NULL))
						return FALSE;
			}
			if (type->u.kmethod->domain_type != NULL)
			{
				if (! print_type (info, type->u.kmethod->domain_type, (struct debug_name *) NULL))
					return FALSE;
			}
			//return (*fns->method_type) (fhandle, type->u.kmethod->domain_type != NULL, is, type->u.kmethod->varargs);
			lprintf("method");
			return TRUE;
		case DEBUG_KIND_CONST:
			lprintf("const ");
			if (! print_type (info, type->u.kconst, (struct debug_name *) NULL))
				return FALSE;
			//return (*fns->const_type) (fhandle);
			return TRUE;
		case DEBUG_KIND_VOLATILE:
			lprintf("volatile ");
			if (! print_type (info, type->u.kvolatile, (struct debug_name *) NULL))
				return FALSE;
			//return (*fns->volatile_type) (fhandle);
			return TRUE;
		case DEBUG_KIND_NAMED:
			return print_type (info, type->u.knamed->type, (struct debug_name *) NULL);
		case DEBUG_KIND_TAGGED:
			return print_type (info, type->u.knamed->type, type->u.knamed->name);
		default:
			assert(0);
			return FALSE;
	}
}

static bfd_boolean
print_string (HANDLE hprocess, DWORD addr)
{
	char szString[MAX_STRING_SIZE];
	int i = 0;

	while(1)
	{
		char c;
		
		if(!ReadProcessMemory(hprocess, (LPVOID) (addr++), &c, sizeof(char), NULL))
			return FALSE;
		
		szString[i++] = c;
		
		if(!c)
		{
			lprintf("\"%s\"", szString);
			return TRUE;
		}
		
		if(!isprint(c))
			return FALSE;
		
		if(i == MAX_STRING_SIZE - 1)
		{
			szString[i] = 0;
			lprintf("\"%s\"...", szString);
			return TRUE;
		}
	}
}

/* Write out a class type.  */
#if 0
static bfd_boolean
debug_write_class_value (struct debug_handle *info, struct debug_type *type, const char *tag)
{
	unsigned int i;
	unsigned int id;
	struct debug_type *vptrbase;

	if (type->u.kclass == NULL)
	{
		id = 0;
		vptrbase = NULL;
	}
	else
	{
		if (type->u.kclass->id <= info->base_id)
		{
			if (! debug_set_class_id (info, tag, type))
				return FALSE;
		}

		if (info->mark == type->u.kclass->mark)
		{
			/* We are currently outputting this class, or we have
			   already output it.  This can happen when there are
			   methods for an anonymous class.  */
			assert (type->u.kclass->id > info->base_id);
			return (*fns->tag_type) (fhandle, tag, type->u.kclass->id, type->kind);
		}
		type->u.kclass->mark = info->mark;
		id = type->u.kclass->id;

		vptrbase = type->u.kclass->vptrbase;
		if (vptrbase != NULL && vptrbase != type)
		{
			if (! debug_write_type (info, fns, fhandle, vptrbase, (struct debug_name *) NULL))
				return FALSE;
		}
	}

	if (! (*fns->start_class_type) (fhandle, tag, id, type->kind == DEBUG_KIND_CLASS, type->size, vptrbase != NULL, vptrbase == type))
		return FALSE;

	if (type->u.kclass != NULL)
	{
		if (type->u.kclass->fields != NULL)
		{
			for (i = 0; type->u.kclass->fields[i] != NULL; i++)
			{
				struct debug_field *f;

				f = type->u.kclass->fields[i];
				if (! debug_write_type (info, fns, fhandle, f->type, (struct debug_name *) NULL))
					return FALSE;
				if (f->static_member)
				{
					if (! (*fns->class_static_member) (fhandle, f->name, f->u.s.physname, f->visibility))
						return FALSE;
				}
				else
				{
					if (! (*fns->struct_field) (fhandle, f->name, f->u.f.bitpos, f->u.f.bitsize, f->visibility))
						return FALSE;
				}
			}
		}

		if (type->u.kclass->baseclasses != NULL)
		{
			for (i = 0; type->u.kclass->baseclasses[i] != NULL; i++)
			{
				struct debug_baseclass *b;

				b = type->u.kclass->baseclasses[i];
				if (! debug_write_type (info, fns, fhandle, b->type, (struct debug_name *) NULL))
					return FALSE;
				if (! (*fns->class_baseclass) (fhandle, b->bitpos, b->virtual, b->visibility))
					return FALSE;
			}
		}

		if (type->u.kclass->methods != NULL)
		{
			for (i = 0; type->u.kclass->methods[i] != NULL; i++)
			{
				struct debug_method *m;
				unsigned int j;

				m = type->u.kclass->methods[i];
				if (! (*fns->class_start_method) (fhandle, m->name))
					return FALSE;
				for (j = 0; m->variants[j] != NULL; j++)
				{
					struct debug_method_variant *v;

					v = m->variants[j];
					if (v->context != NULL)
					{
						if (! debug_write_type (info, fns, fhandle, v->context, (struct debug_name *) NULL))
							return FALSE;
					}
					if (! debug_write_type (info, fns, fhandle, v->type, (struct debug_name *) NULL))
						return FALSE;
					if (v->voffset != VOFFSET_STATIC_METHOD)
					{
						if (! (*fns->class_method_variant) (fhandle, v->physname, v->visibility, v->constp, v->volatilep, v->voffset, v->context != NULL))
							return FALSE;
					}
					else
					{
						if (! (*fns->class_static_method_variant) (fhandle, v->physname, v->visibility, v->constp, v->volatilep))
							return FALSE;
					}
				}
				if (! (*fns->class_end_method) (fhandle))
					return FALSE;
			}
		}
	}

	return (*fns->end_class_type) (fhandle);
}
#endif

/* Write out a type.  If the type is DEBUG_KIND_NAMED or
   DEBUG_KIND_TAGGED, then the name argument is the name for which we
   are about to call typedef or tag.  If the type is anything else,
   then the name argument is a tag from a DEBUG_KIND_TAGGED type which
   points to this one.  */

static bfd_boolean
print_value (bfd *abfd, asymbol **syms, long symcount, struct debug_handle *info, struct debug_type *type, struct debug_name *name, HANDLE hprocess, DWORD addr, int level, int indent)
{
	unsigned int i;

	switch (type->kind)
	{
	case DEBUG_KIND_ILLEGAL:
		debug_error ("print_value: illegal type encountered");
		return FALSE;
	case DEBUG_KIND_INDIRECT:
		if (*type->u.kindirect->slot == DEBUG_TYPE_NULL)
		{
			//return (*fns->empty_type) (fhandle);
			lprintf("<undefined>");
			return TRUE;
		}
		//return print_value (info, *type->u.kindirect->slot, name, NULL, 0, level - 1);
		lprintf("(indirect)");
		return TRUE;
	case DEBUG_KIND_VOID:
		//return (*fns->void_type) (fhandle);
		lprintf ("(void)");
		return TRUE;
	case DEBUG_KIND_INT:
		//return (*fns->int_type) (fhandle, type->size, type->u.kint);
		{
			char buffer[8];

			assert(type->size <= sizeof(buffer));
			
			if(ReadProcessMemory(hprocess, (LPVOID) addr, buffer, type->size, NULL))
				switch(type->size)
				{
					case 1:
						if(isprint(*((char *) buffer)))
							lprintf("'%c'", *((char *) buffer));
						else
						{
							if(type->u.kint)
								lprintf("%u", (unsigned) *((unsigned char *) buffer));
							else
								lprintf("%i", (int) *((char *) buffer));
						}
						break;
	
					case 2:
						if(type->u.kint)
							lprintf("%u", (unsigned) *((unsigned short *) buffer));
						else
							lprintf("%i", (int) *((short *) buffer));
						break;
	
					case 4:
						if(type->u.kint)
							lprintf("%lu", *((unsigned long *) buffer));
						else
							lprintf("%li", *((long *) buffer));
						break;
		
					default:
						return FALSE;
				}
		}
		return TRUE;
	case DEBUG_KIND_FLOAT:
		//return (*fns->float_type) (fhandle, type->size);
		{
			char buffer[8];

			assert(type->size <= sizeof(buffer));
			
			if(ReadProcessMemory(hprocess, (LPVOID) addr, buffer, type->size, NULL))
				switch(type->size)
				{
					case 4:
						lprintf("%f", *((float *) buffer));
						break;
	
					case 8:
						lprintf("%f", *((double *) buffer));
						break;
		
					default:
						lprintf("%i", type->size);
						return FALSE;
				}
		}
		return TRUE;
	case DEBUG_KIND_COMPLEX:
		//return (*fns->complex_type) (fhandle, type->size);
		lprintf("(complex)");			
		return TRUE;
	case DEBUG_KIND_BOOL:
		//return (*fns->bool_type) (fhandle, type->size);
		lprintf("(bool)");
		return TRUE;
	case DEBUG_KIND_STRUCT:
	case DEBUG_KIND_UNION:
		//(*fns->start_struct_type) (fhandle, tag, (type->u.kclass != NULL ? type->u.kclass->id : 0), type->kind == DEBUG_KIND_STRUCT, type->size)
		lprintf("{");
		print_indent(++indent);
		if (type->u.kclass != NULL && type->u.kclass->fields != NULL)
		{
			for (i = 0; type->u.kclass->fields[i] != NULL; i++)
			{
				struct debug_field *f;

				if(i)
				{
					lprintf(",");
					print_indent(indent);
				}
				
				f = type->u.kclass->fields[i];
				if (!print_type (info, f->type, (struct debug_name *) NULL))
					return FALSE;
				
				lprintf(" %s", f->name);

				if((f->u.f.bitpos & 0x7) == 0 && (f->u.f.bitsize & 0x7) == 0)
				{
					lprintf(" = ", f->name);

					//(*fns->struct_field) (fhandle, f->name, f->u.f.bitpos, f->u.f.bitsize, f->visibility)
					if (! print_value (abfd, syms, symcount, info, f->type, (struct debug_name *) NULL, hprocess, addr + (f->u.f.bitpos >> 3), level, indent))
						return FALSE;
				}
			}
		}
		print_indent(--indent);
		lprintf("}");
		//return (*fns->end_struct_type) (fhandle);
		return TRUE;
	case DEBUG_KIND_CLASS:
	case DEBUG_KIND_UNION_CLASS:
		//return debug_write_class_type (info, fns, fhandle, type, tag);
		lprintf("(class)");
		return TRUE;
	case DEBUG_KIND_ENUM:
		//if (type->u.kenum == NULL)
		//	return (*fns->enum_type) (fhandle, tag, (const char **) NULL,	(bfd_signed_vma *) NULL);
		//return (*fns->enum_type) (fhandle, tag, type->u.kenum->names, type->u.kenum->values);
		{
			bfd_signed_vma value;
			
			if(!ReadProcessMemory(hprocess, (LPVOID) addr, &value, sizeof(value), NULL))
				return TRUE;
			
			if (type->u.kenum == NULL)
			{
				lprintf("%li", (long) value);
				return TRUE;
			}
			else
			{
				int i;
				
				for (i = 0; type->u.kenum->names[i] != NULL; i++)
				{
					if(type->u.kenum->values[i] == value)
					{
						lprintf("%s", type->u.kenum->names[i]);
						return TRUE;
					}
				}

				lprintf("%li", (long) value);
				return TRUE;
			}
		}
		return TRUE;
	case DEBUG_KIND_POINTER:
		{
			DWORD nextaddr;
			
			if(ReadProcessMemory(hprocess, (LPVOID) addr, &nextaddr, sizeof(DWORD), NULL))
			{				
				if(level)
				{
					if(
						!(
							(
								(type->u.kpointer->kind == DEBUG_KIND_INT && type->u.kpointer->size == 1) ||
								(
									(type->u.kpointer->kind == DEBUG_KIND_NAMED || type->u.kpointer->kind == DEBUG_KIND_TAGGED) &&
									type->u.kpointer->u.knamed->type->kind == DEBUG_KIND_INT && type->u.kpointer->u.knamed->type->size == 1
								)
							) &&
							print_string(hprocess, nextaddr)
						) &&
						!(
							type->u.kpointer->kind == DEBUG_KIND_VOID ||
							(
								(type->u.kpointer->kind == DEBUG_KIND_NAMED || type->u.kpointer->kind == DEBUG_KIND_TAGGED) &&
								type->u.kpointer->u.knamed->type->kind == DEBUG_KIND_VOID
							)
						)
					)
					{
						lprintf("&");
						if (! print_value (abfd, syms, symcount, info, type->u.kpointer, (struct debug_name *) NULL, hprocess, nextaddr, level - 1, indent))
							return FALSE;
					}
				}
				else
					lprintf("0x%08lx", nextaddr);
			}
			//return (*fns->pointer_type) (fhandle);
			return TRUE;
		}
	case DEBUG_KIND_FUNCTION:
		//return (*fns->function_type) (fhandle, is, type->u.kfunction->varargs);
		{
			char szSymName[MAX_SYM_NAME_SIZE];
			
			if(BfdGetSymFromAddr(abfd, syms, symcount, hprocess, addr, szSymName, MAX_SYM_NAME_SIZE))
				lprintf("%s", szSymName);
			else
				lprintf("0x%08lx", addr);

			return TRUE;
		}
	case DEBUG_KIND_REFERENCE:
		{
			DWORD nextaddr;
			
			if(ReadProcessMemory(hprocess, (LPVOID) addr, &nextaddr, sizeof(DWORD), NULL))
			{				
				if(level)
				{
					if (! print_value (abfd, syms, symcount, info, type->u.kreference, (struct debug_name *) NULL, hprocess, nextaddr, level - 1, indent))
						return FALSE;
				}
				else
					lprintf("*0x%08lx", nextaddr);
			}
			//return (*fns->reference_type) (fhandle);
			return TRUE;
		}
	case DEBUG_KIND_RANGE:
		lprintf("(range)");
		/*
		if (! print_value (info, type->u.krange->type, (struct debug_name *) NULL, NULL, 0, level - 1))
			return FALSE;
		*/
		//return (*fns->range_type) (fhandle, type->u.krange->lower, type->u.krange->upper);
		return TRUE;
	case DEBUG_KIND_ARRAY:
		lprintf("(array)");
		/*
		if (! print_value (info, type->u.karray->element_type, (struct debug_name *) NULL, NULL, 0, level - 1)
				|| ! print_value (info, type->u.karray->range_type, (struct debug_name *) NULL, NULL, 0, level - 1))
			return FALSE;
		*/
		//return (*fns->array_type) (fhandle, type->u.karray->lower, type->u.karray->upper, type->u.karray->stringp);
		return TRUE;
	case DEBUG_KIND_SET:
		lprintf("(set)");
		/*
		if (! print_value (info, type->u.kset->type, (struct debug_name *) NULL, NULL, 0, level - 1))
			return FALSE;
		*/
		//return (*fns->set_type) (fhandle, type->u.kset->bitstringp);
		return TRUE;
	case DEBUG_KIND_OFFSET:
		lprintf("(offset)");
		/*
		if (! print_value (info, type->u.koffset->base_type, (struct debug_name *) NULL, NULL, 0, level - 1)
				|| ! print_value (info, type->u.koffset->target_type, (struct debug_name *) NULL, NULL, 0, level - 1))
			return FALSE;
		*/
		//return (*fns->offset_type) (fhandle);
		lprintf("offset ");
		return TRUE;
	case DEBUG_KIND_METHOD:
		lprintf("(method)");
		/*
		if (! print_value (info, type->u.kmethod->return_type, (struct debug_name *) NULL, NULL, 0, level - 1))
			return FALSE;
		if (type->u.kmethod->arg_types == NULL)
			is = -1;
		else
		{
			for (is = 0; type->u.kmethod->arg_types[is] != NULL; is++)
				if (! print_value (info, type->u.kmethod->arg_types[is], (struct debug_name *) NULL, NULL, 0, level - 1))
					return FALSE;
		}
		if (type->u.kmethod->domain_type != NULL)
		{
			if (! print_value (info, type->u.kmethod->domain_type, (struct debug_name *) NULL, NULL, 0, level - 1))
				return FALSE;
		}
		*/
		//return (*fns->method_type) (fhandle, type->u.kmethod->domain_type != NULL, is, type->u.kmethod->varargs);
		return TRUE;
	case DEBUG_KIND_CONST:
		//return (*fns->const_type) (fhandle);
		if (! print_value (abfd, syms, symcount, info, type->u.kconst, (struct debug_name *) NULL, hprocess, addr, level, indent))
			return FALSE;
		return TRUE;
	case DEBUG_KIND_VOLATILE:
		//return (*fns->volatile_type) (fhandle);
		if (! print_value (abfd, syms, symcount, info, type->u.kvolatile, (struct debug_name *) NULL, hprocess, addr, level, indent))
			return FALSE;
		return TRUE;
	case DEBUG_KIND_NAMED:
		return print_value (abfd, syms, symcount, info, type->u.knamed->type, (struct debug_name *) NULL, hprocess, addr, level, indent);
	case DEBUG_KIND_TAGGED:
		return print_value (abfd, syms, symcount, info, type->u.knamed->type, type->u.knamed->name, hprocess, addr, level, indent);
	default:
		abort ();
		return FALSE;
	}
}

