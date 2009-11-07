/*  FILE: c_util.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
24-oct-09    abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xmlstring.h>

#ifndef _H_procdefs
#include  "procdefs.h"
#endif

#ifndef _H_c_util
#include  "c_util.h"
#endif

#ifndef _H_log
#include  "log.h"
#endif

#ifndef _H_ncx
#include  "ncx.h"
#endif

#ifndef _H_ncxmod
#include  "ncxmod.h"
#endif

#ifndef _H_ncxtypes
#include "ncxtypes.h"
#endif

#ifndef _H_ses
#include "ses.h"
#endif

#ifndef _H_status
#include  "status.h"
#endif

#ifndef _H_yangconst
#include "yangconst.h"
#endif

#ifndef _H_yangdump
#include "yangdump.h"
#endif

#ifndef _H_yangdump_util
#include "yangdump_util.h"
#endif


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
* FUNCTION new_c_define
* 
* Allocate and fill in a constructed identifier to string binding
* for an object struct of some kind
*
* INPUTS:
*   modname == module name to use
*   defname == definition name to use
*   cdefmode == c definition mode
*********************************************************************/
static c_define_t *
    new_c_define (const xmlChar *modname,
                  const xmlChar *defname,
                  c_mode_t cmode)
{
    c_define_t  *cdef;
    xmlChar     *buffer, *value, *p;
    uint32       len;

    /* get the idstr length */
    len = 0;

    if (cmode != C_MODE_CALLBACK) {
        len += xml_strlen(Y_PREFIX);
        len += xml_strlen(modname);
    }

    switch (cmode) {
    case C_MODE_OID:
        len += 3;  /* _N_ */
        break;
    case C_MODE_TYPEDEF:
        len += 2;  /* _T */
        break;
    case C_MODE_CALLBACK:
        len += 1;
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return NULL;
    }

    len += xml_strlen(defname);

    buffer = m__getMem(len+1);
    if (buffer == NULL) {
        return NULL;
    }

    value = xml_strdup(defname);
    if (value == NULL) {
        m__free(buffer);
        return NULL;
    }

    cdef = m__getObj(c_define_t);
    if (cdef == NULL) {
        m__free(buffer);
        m__free(value);
        return NULL;
    }
    memset(cdef, 0x0, sizeof(c_define_t));

    /* fill in the idstr buffer */
    p = buffer;
    if (cmode != C_MODE_CALLBACK) {
        p += xml_strcpy(p, Y_PREFIX);
        p += copy_c_safe_str(p, modname);
    }

    switch (cmode) {
    case C_MODE_OID:
        p += xml_strcpy(p, (const xmlChar *)"_N_");
        break;
    case C_MODE_TYPEDEF:
        p += xml_strcpy(p, (const xmlChar *)"_T");
        break;
    case C_MODE_CALLBACK:
        *p++ = 'd';
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }
    p += copy_c_safe_str(p, defname);    

    cdef->idstr = buffer;     /* transfer buffer memory here */
    cdef->valstr = value;     /* transfer value memory here */    
    return cdef;

}  /* new_c_define */


/********************************************************************
* FUNCTION free_c_define
* 
* Free an identifier to string binding
*
* INPUTS:
*   cdef == struct to free
*********************************************************************/
static void
    free_c_define (c_define_t *cdef)
{
    if (cdef->idstr != NULL) {
        m__free(cdef->idstr);
    }
    if (cdef->valstr != NULL) {
        m__free(cdef->valstr);
    }
    m__free(cdef);

}  /* free_c_define */


/********************************************************************
* FUNCTION need_rpc_includes
* 
* Check if the include-stmts for RPC methods are needed
*
* INPUTS:
*   mod == module in progress
*   cp == conversion parameters to use
*
* RETURNS:
*  TRUE if RPCs found
*  FALSE if no RPCs found
*********************************************************************/
boolean
    need_rpc_includes (const ncx_module_t *mod,
                       const yangdump_cvtparms_t *cp)
{
    const ncx_include_t *inc;

    if (obj_any_rpcs(&mod->datadefQ)) {
        return TRUE;
    }

    if (cp->unified) {
        for (inc = (const ncx_include_t *)
                 dlq_firstEntry(&mod->includeQ);
             inc != NULL;
             inc = (const ncx_include_t *)dlq_nextEntry(inc)) {

            if (inc->submod && obj_any_rpcs(&inc->submod->datadefQ)) {
                return TRUE;
            }
        }
    }

    return FALSE;

} /* need_rpc_includes */


/********************************************************************
* FUNCTION need_notif_includes
* 
* Check if the include-stmts for notifications are needed
*
* INPUTS:
*   mod == module in progress
*   cp == conversion parameters to use
*
* RETURNS:
*   TRUE if notifcations found
*   FALSE if no notifications found
*********************************************************************/
boolean
    need_notif_includes (const ncx_module_t *mod,
                         const yangdump_cvtparms_t *cp)
{
    const ncx_include_t *inc;

    if (obj_any_notifs(&mod->datadefQ)) {
        return TRUE;
    }

    if (cp->unified) {
        for (inc = (const ncx_include_t *)
                 dlq_firstEntry(&mod->includeQ);
             inc != NULL;
             inc = (const ncx_include_t *)dlq_nextEntry(inc)) {

            if (inc->submod && obj_any_notifs(&inc->submod->datadefQ)) {
                return TRUE;
            }
        }
    }

    return FALSE;

} /* need_notif_includes */


/********************************************************************
* FUNCTION write_c_safe_str
* 
* Generate a string token at the current line location
*
* INPUTS:
*   scb == session control block to use for writing
*   strval == string value
*********************************************************************/
void
    write_c_safe_str (ses_cb_t *scb,
                      const xmlChar *strval)
{
    const xmlChar *s;

    s = strval;
    while (*s) {
        if (*s == '.' || *s == '-' || *s == NCXMOD_PSCHAR) {
            ses_putchar(scb, '_');
        } else {
            ses_putchar(scb, *s);
        }
        s++;
    }

}  /* write_c_safe_str */


/********************************************************************
* FUNCTION copy_c_safe_str
* 
* Generate a string token and copy to buffer
*
* INPUTS:
*   buffer == buffer to write into
*   strval == string value to copy
*
* RETURNS
*   number of chars copied
*********************************************************************/
uint32
    copy_c_safe_str (xmlChar *buffer,
                     const xmlChar *strval)
{
    const xmlChar *s;
    uint32         count;

    count = 0;
    s = strval;

    while (*s) {
        if (*s == '.' || *s == '-' || *s == NCXMOD_PSCHAR) {
            *buffer++ = '_';
        } else {
            *buffer++ = *s;
        }
        s++;
        count++;
    }
    *buffer = 0;

    return count;

}  /* copy_c_safe_str */


/********************************************************************
* FUNCTION write_c_str
* 
* Generate a string token at the current line location
*
* INPUTS:
*   scb == session control block to use for writing
*   strval == string value
*   quotes == quotes style (0, 1, 2)
*********************************************************************/
void
    write_c_str (ses_cb_t *scb,
                 const xmlChar *strval,
                 uint32 quotes)
{
    switch (quotes) {
    case 1:
        ses_putchar(scb, '\'');
        break;
    case 2:
        ses_putchar(scb, '"');
        break;
    default:
        ;
    }

    ses_putstr(scb, strval);

    switch (quotes) {
    case 1:
        ses_putchar(scb, '\'');
        break;
    case 2:
        ses_putchar(scb, '"');
        break;
    default:
        ;
    }

}  /* write_c_str */


/********************************************************************
* FUNCTION write_c_simple_str
* 
* Generate a simple clause on 1 line
*
* INPUTS:
*   scb == session control block to use for writing
*   kwname == keyword name
*   strval == string value
*   indent == indent count to use
*   quotes == quotes style (0, 1, 2)
*********************************************************************/
void
    write_c_simple_str (ses_cb_t *scb,
                        const xmlChar *kwname,
                        const xmlChar *strval,
                        int32 indent,
                        uint32 quotes)
{
    ses_putstr_indent(scb, kwname, indent);
    if (strval) {
        ses_putchar(scb, ' ');
        write_c_str(scb, strval, quotes);
    }

}  /* write_c_simple_str */


/********************************************************************
*
* FUNCTION write_identifier
* 
* Generate an identifier
*
*  #module_DEFTYPE_idname
*
* INPUTS:
*   scb == session control block to use for writing
*   modname == module name start-string to use
*   defpart == internal string for deftype part
*   idname == identifier name
*
*********************************************************************/
void
    write_identifier (ses_cb_t *scb,
                      const xmlChar *modname,
                      const xmlChar *defpart,
                      const xmlChar *idname)
{
    ses_putstr(scb, Y_PREFIX);
    write_c_safe_str(scb, modname);
    ses_putchar(scb, '_');
    if (defpart != NULL) {
        ses_putstr(scb, defpart);
        ses_putchar(scb, '_');
    }
    write_c_safe_str(scb, idname);

}  /* write_identifier */


/********************************************************************
* FUNCTION write_ext_include
* 
* Generate an include statement for an external file
*
*  #include <foo,h>
*
* INPUTS:
*   scb == session control block to use for writing
*   hfile == H file name == file name to include (foo.h)
*
*********************************************************************/
void
    write_ext_include (ses_cb_t *scb,
                       const xmlChar *hfile)
{
    ses_putstr(scb, POUND_INCLUDE);
    ses_putstr(scb, (const xmlChar *)"<");
    ses_putstr(scb, hfile);
    ses_putstr(scb, (const xmlChar *)">\n");

}  /* write_ext_include */


/********************************************************************
* FUNCTION write_ncx_include
* 
* Generate an include statement for an NCX file
*
*  #ifndef _H_foo
*  #include "foo,h"
*
* INPUTS:
*   scb == session control block to use for writing
*   modname == module name to include (foo)
*
*********************************************************************/
void
    write_ncx_include (ses_cb_t *scb,
                       const xmlChar *modname)
{
    ses_putstr(scb, POUND_IFNDEF);
    ses_putstr(scb, BAR_H);
    ses_putstr(scb, modname);
    ses_putstr(scb, POUND_INCLUDE);
    ses_putchar(scb, '"');
    ses_putstr(scb, modname);
    ses_putstr(scb, (const xmlChar *)".h\"");
    ses_putstr(scb, POUND_ENDIF);
    ses_putchar(scb, '\n');

}  /* write_ncx_include */


/********************************************************************
* FUNCTION save_oid_cdefine
* 
* Generate a #define binding for a definition and save it in the
* specified Q of c_define_t structs
*
* INPUTS:
*   cdefineQ == Q of c_define_t structs to use
*   modname == module name to use
*   defname == object definition name to use
*
* OUTPUTS:
*   a new c_define_t is allocated and added to the cdefineQ
*   if returning NO_ERR;
*
* RETURNS:
*   status; duplicate C identifiers not supported yet
*      foo-1 -->  foo_1
*      foo.1 -->  foo_1
*   An error message will be generated if this type of error occurs
*********************************************************************/
status_t
    save_oid_cdefine (dlq_hdr_t *cdefineQ,
                      const xmlChar *modname,
                      const xmlChar *defname)
{
    c_define_t    *newcdef, *testcdef;
    status_t       res;
    int32          retval;

#ifdef DEBUG
    if (cdefineQ == NULL || modname == NULL || defname == NULL) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    newcdef = new_c_define(modname, defname, C_MODE_OID);
    if (newcdef == NULL) {
        return ERR_INTERNAL_MEM;
    }

    /* keep the cdefineQ sorted by the idstr value */
    res = NO_ERR;
    for (testcdef = (c_define_t *)dlq_firstEntry(cdefineQ);
         testcdef != NULL;
         testcdef = (c_define_t *)dlq_nextEntry(testcdef)) {

        retval = xml_strcmp(newcdef->idstr, testcdef->idstr);
        if (retval == 0) {
            if (xml_strcmp(newcdef->valstr, testcdef->valstr)) {
                /* error - duplicate ID with different original value */
                res = ERR_NCX_INVALID_VALUE;
                log_error("\nError: C idenitifer conflict between "
                          "'%s' and '%s'",
                          newcdef->valstr,
                          testcdef->valstr);
            } /* else duplicate is not a problem */
            free_c_define(newcdef);
            return res;
        } else if (retval < 0) {
            dlq_insertAhead(newcdef, testcdef);
            return NO_ERR;
        } /* else try next entry */
    }

    /* new last entry */
    dlq_enque(newcdef, cdefineQ);
    return NO_ERR;

}  /* save_oid_cdefine */


/********************************************************************
* FUNCTION save_path_cdefine
* 
* Generate a #define binding for a definition and save it in the
* specified Q of c_define_t structs
*
* INPUTS:
*   cdefineQ == Q of c_define_t structs to use
*   modname == base module name to use
*   obj == object struct to use to generate path
*   cmode == mode to use
*
* OUTPUTS:
*   a new c_define_t is allocated and added to the cdefineQ
*   if returning NO_ERR;
*
* RETURNS:
*   status; duplicate C identifiers not supported yet
*      foo-1/a/b -->  foo_1_a_b
*      foo.1/a.2 -->  foo_1_a_b
*   An error message will be generated if this type of error occurs
*********************************************************************/
status_t
    save_path_cdefine (dlq_hdr_t *cdefineQ,
                       const xmlChar *modname,
                       obj_template_t *obj,
                       c_mode_t cmode)
{
    c_define_t    *newcdef, *testcdef;
    xmlChar       *buffer;
    status_t       res;
    int32          retval;

#ifdef DEBUG
    if (cdefineQ == NULL || modname == NULL || obj == NULL) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    buffer = NULL;
    res = obj_gen_object_id(obj, &buffer);
    if (res != NO_ERR) {
        return res;
    }

    newcdef = new_c_define(modname, buffer, cmode);
    m__free(buffer);
    buffer = NULL;
    if (newcdef == NULL) {
        return ERR_INTERNAL_MEM;
    }
    newcdef->obj = obj;

    /* keep the cdefineQ sorted by the idstr value */
    res = NO_ERR;
    for (testcdef = (c_define_t *)dlq_firstEntry(cdefineQ);
         testcdef != NULL;
         testcdef = (c_define_t *)dlq_nextEntry(testcdef)) {

        retval = xml_strcmp(newcdef->idstr, testcdef->idstr);
        if (retval == 0) {
            if (xml_strcmp(newcdef->valstr, testcdef->valstr)) {
                /* error - duplicate ID with different original value */
                res = ERR_NCX_INVALID_VALUE;
                log_error("\nError: C idenitifer conflict between "
                          "'%s' and '%s'",
                          newcdef->valstr,
                          testcdef->valstr);
            } /* else duplicate is not a problem */
            free_c_define(newcdef);
            return res;
        } else if (retval < 0) {
            dlq_insertAhead(newcdef, testcdef);
            return NO_ERR;
        } /* else try next entry */
    }

    /* new last entry */
    dlq_enque(newcdef, cdefineQ);
    return NO_ERR;

}  /* save_path_cdefine */


/********************************************************************
* FUNCTION find_path_cdefine
* 
* Find a #define binding for a definition in the
* specified Q of c_define_t structs
*
* INPUTS:
*   cdefineQ == Q of c_define_t structs to use
*   obj == object struct to find
*
* RETURNS:
*   pointer to found entry
*   NULL if not found
*********************************************************************/
c_define_t *
    find_path_cdefine (dlq_hdr_t *cdefineQ,
                       obj_template_t *obj)
{
    c_define_t    *testcdef;

#ifdef DEBUG
    if (cdefineQ == NULL || obj == NULL) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    /* find the cdefineQ sorted by the object struct back-ptr */
    for (testcdef = (c_define_t *)dlq_firstEntry(cdefineQ);
         testcdef != NULL;
         testcdef = (c_define_t *)dlq_nextEntry(testcdef)) {

        if (testcdef->obj == obj) {
            return testcdef;
        }
    }

    return NULL;

}  /* find_path_cdefine */


/********************************************************************
* FUNCTION clean_cdefineQ
* 
* Clean a Q of c_define_t structs
*
* INPUTS:
*   cdefineQ == Q of c_define_t structs to use
*********************************************************************/
void
    clean_cdefineQ (dlq_hdr_t *cdefineQ)
{
    c_define_t    *cdef;

#ifdef DEBUG
    if (cdefineQ == NULL) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    while (!dlq_empty(cdefineQ)) {
        cdef = (c_define_t *)dlq_deque(cdefineQ);
        free_c_define(cdef);
    }

}  /* clean_cdefineQ */


/********************************************************************
* FUNCTION write_c_header
* 
* Write the C file header
*
* INPUTS:
*   scb == session control block to use for writing
*   mod == module in progress
*   cp == conversion parameters to use
*
*********************************************************************/
void
    write_c_header (ses_cb_t *scb,
                    const ncx_module_t *mod,
                    const yangdump_cvtparms_t *cp)
{
    int32                     indent;

    indent = cp->indent;

    /* banner comments */
    ses_putstr(scb, START_COMMENT);    
    ses_putchar(scb, '\n');

    /* generater tag */
    write_banner_session(scb);

    /* copyright section
     * ses_putstr(scb, (const xmlChar *)
     *        "\n  *** Put copyright info here ***\n");
     */

    /* module name */
    if (mod->ismod) {
        ses_putstr_indent(scb, YANG_K_MODULE, indent);
    } else {
        ses_putstr_indent(scb, YANG_K_SUBMODULE, indent);
    }
    ses_putchar(scb, ' ');
    ses_putstr(scb, mod->name);

    /* version */
    if (mod->version) {
        write_c_simple_str(scb, 
                           YANG_K_REVISION,
                           mod->version, 
                           indent,
                           0);
        ses_putchar(scb, '\n');
    }

    /* namespace or belongs-to */
    if (mod->ismod) {
        write_c_simple_str(scb, 
                           YANG_K_NAMESPACE, 
                           mod->ns,
                           indent,
                           0);
    } else {
        write_c_simple_str(scb, 
                           YANG_K_BELONGS_TO, 
                           mod->belongs,
                           indent,
                           0);
    }

    /* organization */
    if (mod->organization) {
        write_c_simple_str(scb, 
                           YANG_K_ORGANIZATION,
                           mod->organization, 
                           indent,
                           0);
    }

    ses_putchar(scb, '\n');
    ses_putchar(scb, '\n');
    ses_putstr(scb, END_COMMENT);
    ses_putchar(scb, '\n');

} /* write_c_header */


/********************************************************************
* FUNCTION write_c_footer
* 
* Write the C file footer
*
* INPUTS:
*   scb == session control block to use for writing
*   mod == module in progress
*
*********************************************************************/
void
    write_c_footer (ses_cb_t *scb,
                    const ncx_module_t *mod)
{
    ses_putstr(scb, (const xmlChar *)"\n/* END ");
    write_c_safe_str(scb, ncx_get_modname(mod));
    ses_putstr(scb, (const xmlChar *)".c */\n");

} /* write_c_footer */


/*******************************************************************
* FUNCTION write_c_objtype
* 
* Generate the C data type for the NCX data type
*
* INPUTS:
*   scb == session control block to use for writing
*   obj == object template to check
*
**********************************************************************/
void
    write_c_objtype (ses_cb_t *scb,
                     const obj_template_t *obj)
{
    write_c_objtype_ex(scb, obj, ';');

}  /* write_c_objtype */


/*******************************************************************
* FUNCTION write_c_objtype_ex
* 
* Generate the C data type for the NCX data type
*
* INPUTS:
*   scb == session control block to use for writing
*   obj == object template to check
*   endchar == char to use at end (semi-colon, comma, right-paren)
**********************************************************************/
void
    write_c_objtype_ex (ses_cb_t *scb,
                        const obj_template_t *obj,
                        xmlChar endchar)
{
    boolean        needspace;
    ncx_btype_t    btyp;

    needspace = TRUE;
    btyp = obj_get_basetype(obj);

    switch (btyp) {
    case NCX_BT_BOOLEAN:
        ses_putstr(scb, BOOLEAN);
        break;
    case NCX_BT_INT8:
        ses_putstr(scb, INT8);
        break;
    case NCX_BT_INT16:
        ses_putstr(scb, INT16);
        break;
    case NCX_BT_INT32:
        ses_putstr(scb, INT32);
        break;
    case NCX_BT_INT64:
        ses_putstr(scb, INT64);
        break;
    case NCX_BT_UINT8:
        ses_putstr(scb, UINT8);
        break;
    case NCX_BT_UINT16:
        ses_putstr(scb, UINT16);
        break;
    case NCX_BT_UINT32:
        ses_putstr(scb, UINT32);
        break;
    case NCX_BT_UINT64:
        ses_putstr(scb, UINT64);
        break;
    case NCX_BT_DECIMAL64:
        ses_putstr(scb, INT64);
        break;
    case NCX_BT_FLOAT64:
        ses_putstr(scb, DOUBLE);
        break;
    case NCX_BT_ENUM:
    case NCX_BT_STRING:
    case NCX_BT_BINARY:
    case NCX_BT_INSTANCE_ID:
    case NCX_BT_LEAFREF:
    case NCX_BT_SLIST:
        ses_putstr(scb, STRING);
        needspace = FALSE;
        break;
    case NCX_BT_IDREF:
        ses_putstr(scb, IDREF);
        needspace = FALSE;
        break;
    case NCX_BT_LIST:
        ses_putstr(scb, QUEUE);
        break;
    default:
        /* assume complex type */
        write_identifier(scb,
                         obj_get_mod_name(obj),
                         DEF_TYPE,
                         obj_get_name(obj));
    }

    if (needspace) {
        ses_putchar(scb, ' ');
    }

    write_c_safe_str(scb, obj_get_name(obj));
    ses_putchar(scb, endchar);

}  /* write_c_objtype_ex */


/*******************************************************************
* FUNCTION write_c_val_macro_name
* 
* Generate the C VAL_FOO macro name for the data type
*
* INPUTS:
*   scb == session control block to use for writing
*   obj == object template to check
*
**********************************************************************/
void
    write_c_val_macro_type (ses_cb_t *scb,
                            const obj_template_t *obj)
{
    ncx_btype_t    btyp;

    btyp = obj_get_basetype(obj);

    switch (btyp) {
    case NCX_BT_EMPTY:
        ses_putstr(scb, (const xmlChar *)"VAL_EMPTY");
        break;
    case NCX_BT_BOOLEAN:
        ses_putstr(scb, (const xmlChar *)"VAL_BOOL");
        break;
    case NCX_BT_INT8:
        ses_putstr(scb, (const xmlChar *)"VAL_INT8");
        break;
    case NCX_BT_INT16:
        ses_putstr(scb, (const xmlChar *)"VAL_INT16");
        break;
    case NCX_BT_INT32:
        ses_putstr(scb, (const xmlChar *)"VAL_INT");
        break;
    case NCX_BT_INT64:
        ses_putstr(scb, (const xmlChar *)"VAL_LONG");
        break;
    case NCX_BT_UINT8:
        ses_putstr(scb, (const xmlChar *)"VAL_UINT8");
        break;
    case NCX_BT_UINT16:
        ses_putstr(scb, (const xmlChar *)"VAL_UINT16");
        break;
    case NCX_BT_UINT32:
        ses_putstr(scb, (const xmlChar *)"VAL_UINT");
        break;
    case NCX_BT_UINT64:
        ses_putstr(scb, (const xmlChar *)"VAL_ULONG");
        break;
    case NCX_BT_DECIMAL64:
        ses_putstr(scb, (const xmlChar *)"VAL_DEC64");
        break;
    case NCX_BT_FLOAT64:
        ses_putstr(scb, (const xmlChar *)"VAL_DOUBLE");
        break;
    case NCX_BT_ENUM:
        ses_putstr(scb, (const xmlChar *)"VAL_ENUM_NAME");
        break;
    case NCX_BT_STRING:
        ses_putstr(scb, (const xmlChar *)"VAL_STRING");
        break;
    case NCX_BT_BINARY:
        ses_putstr(scb, (const xmlChar *)"VAL_BINARY");
        break;
    case NCX_BT_INSTANCE_ID:
        ses_putstr(scb, (const xmlChar *)"VAL_INSTANCE_ID");
        break;
    case NCX_BT_LEAFREF:
        ses_putstr(scb, (const xmlChar *)"VAL_STRING");
        break;
    case NCX_BT_IDREF:
        ses_putstr(scb, (const xmlChar *)"VAL_IDREF");
        break;
    case NCX_BT_SLIST:
        ses_putstr(scb, (const xmlChar *)"VAL_LIST");
        break;
    case NCX_BT_LIST:
        ses_putstr(scb, QUEUE);
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

}  /* write_c_val_macro_type */


/*******************************************************************
* FUNCTION write_c_oid_comment
* 
* Generate the object OID as a comment line
*
* INPUTS:
*   scb == session control block to use for writing
*   obj == object template to check
*
**********************************************************************/
void
    write_c_oid_comment (ses_cb_t *scb,
                         const obj_template_t *obj)
{
    xmlChar    *buffer;
    status_t    res;

    /* generate the YOID as a comment */
    res = obj_gen_object_id(obj, &buffer);
    if (res != NO_ERR) {
        SET_ERROR(res);
        return;
    }

    ses_putstr(scb, START_COMMENT);
    ses_putstr(scb, obj_get_typestr(obj));
    ses_putchar(scb, ' ');
    ses_putstr(scb, buffer);
    ses_putstr(scb, END_COMMENT);
    m__free(buffer);

} /* write_c_oid_comment */


/********************************************************************
* FUNCTION save_c_objects
* 
* save the path name bindings for C typdefs
*
* INPUTS:
*   mod == module in progress
*   datadefQ == que of obj_template_t to use
*   savecdefQ == Q of c_define_t structs to use
*   cmode == C code generating mode to use
*
* OUTPUTS:
*   savecdefQ may get new structs added
*
* RETURNS:
*  status
*********************************************************************/
status_t
    save_c_objects (ncx_module_t *mod,
                    dlq_hdr_t *datadefQ,
                    dlq_hdr_t *savecdefQ,
                    c_mode_t cmode)
{
    obj_template_t    *obj;
    dlq_hdr_t         *childdatadefQ;
    status_t           res;

    if (dlq_empty(datadefQ)) {
        return NO_ERR;
    }

    for (obj = (obj_template_t *)dlq_firstEntry(datadefQ);
         obj != NULL;
         obj = (obj_template_t *)dlq_nextEntry(obj)) {

        if (!obj_has_name(obj) || 
            obj_is_cli(obj) ||
            !obj_is_enabled(obj) ||
            obj_is_abstract(obj)) {
            continue;
        }

        res = save_path_cdefine(savecdefQ,
                                ncx_get_modname(mod), 
                                obj,
                                cmode);
        if (res != NO_ERR) {
            return res;
        }

        childdatadefQ = obj_get_datadefQ(obj);
        if (childdatadefQ) {
            res = save_c_objects(mod,
                                 childdatadefQ,
                                 savecdefQ,
                                 cmode);
            if (res != NO_ERR) {
                return res;
            }
        }
    }

    return NO_ERR;

}  /* save_c_objects */

/* END c_util.c */



