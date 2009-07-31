/*  FILE: yangcli_cmd.c

   NETCONF YANG-based CLI Tool

   See ./README for details

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
11-apr-09    abb      begun; started from yangcli.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libssh2.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include "libtecla.h"

#ifndef _H_procdefs
#include "procdefs.h"
#endif

#ifndef _H_cli
#include "cli.h"
#endif

#ifndef _H_conf
#include "conf.h"
#endif

#ifndef _H_help
#include "help.h"
#endif

#ifndef _H_log
#include "log.h"
#endif

#ifndef _H_mgr
#include "mgr.h"
#endif

#ifndef _H_mgr_io
#include "mgr_io.h"
#endif

#ifndef _H_mgr_not
#include "mgr_not.h"
#endif

#ifndef _H_mgr_rpc
#include "mgr_rpc.h"
#endif

#ifndef _H_mgr_ses
#include "mgr_ses.h"
#endif

#ifndef _H_ncx
#include "ncx.h"
#endif

#ifndef _H_ncxconst
#include "ncxconst.h"
#endif

#ifndef _H_ncxmod
#include "ncxmod.h"
#endif

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_obj_help
#include "obj_help.h"
#endif

#ifndef _H_op
#include "op.h"
#endif

#ifndef _H_rpc
#include "rpc.h"
#endif

#ifndef _H_runstack
#include "runstack.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_val_util
#include "val_util.h"
#endif

#ifndef _H_var
#include "var.h"
#endif

#ifndef _H_xmlns
#include "xmlns.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif

#ifndef _H_xml_val
#include "xml_val.h"
#endif

#ifndef _H_xml_wr
#include "xml_wr.h"
#endif

#ifndef _H_xpath
#include "xpath.h"
#endif

#ifndef _H_xpath1
#include "xpath1.h"
#endif

#ifndef _H_xpath_yang
#include "xpath_yang.h"
#endif

#ifndef _H_yangconst
#include "yangconst.h"
#endif

#ifndef _H_yangcli
#include "yangcli.h"
#endif

#ifndef _H_yangcli_cmd
#include "yangcli_cmd.h"
#endif

#ifndef _H_yangcli_tab
#include "yangcli_tab.h"
#endif

#ifndef _H_yangcli_util
#include "yangcli_util.h"
#endif


/********************************************************************
* FUNCTION parse_rpc_cli
* 
*  Call the cli_parse for an RPC input value set
* 
* INPUTS:
*   rpc == RPC to parse CLI for
*   line == input line to parse, starting with the parms to parse
*   res == pointer to status output
*
* OUTPUTS: 
*   *res == status
*
* RETURNS:
*    pointer to malloced value set or NULL if none created,
*    may have errors, check *res
*********************************************************************/
static val_value_t *
    parse_rpc_cli (const obj_template_t *rpc,
		   const xmlChar *args,
		   status_t  *res)
{
    const obj_template_t   *obj;
    const char             *myargv[2];

    /* construct an argv array, 
     * convert the CLI into a parmset 
     */
    obj = obj_find_child(rpc, NULL, YANG_K_INPUT);
    if (obj && obj_get_child_count(obj)) {
	myargv[0] = (const char *)obj_get_name(rpc);
	myargv[1] = (const char *)args;
	return cli_parse(2, 
			 myargv, 
			 obj, 
			 VALONLY, 
			 SCRIPTMODE,
			 get_autocomp(), 
                         CLI_MODE_COMMAND,
			 res);
    } else {
	*res = ERR_NCX_SKIPPED;
	return NULL;
    }
    /*NOTREACHED*/

}  /* parse_rpc_cli */


/********************************************************************
* FUNCTION get_prompt
* 
* Construct the CLI prompt for the current state
* 
* INPUTS:
*   buff == bufffer to hold prompt (zero-terminated string)
*   bufflen == length of buffer (max bufflen-1 chars can be printed
*
*********************************************************************/
static void
    get_prompt (agent_cb_t *agent_cb,
		xmlChar *buff,
		uint32 bufflen)
{
    xmlChar        *p;
    val_value_t    *parm;
    uint32          len;

    if (agent_cb->climore) {
	xml_strncpy(buff, MORE_PROMPT, bufflen);
	return;
    }

    switch (agent_cb->state) {
    case MGR_IO_ST_INIT:
    case MGR_IO_ST_IDLE:
    case MGR_IO_ST_CONNECT:
    case MGR_IO_ST_CONN_START:
    case MGR_IO_ST_SHUT:
	if (agent_cb->cli_fn) {
	    if ((xml_strlen(DEF_FN_PROMPT) 
		 + xml_strlen(agent_cb->cli_fn) + 2) < bufflen) {
		p = buff;
		p += xml_strcpy(p, DEF_FN_PROMPT);
		p += xml_strcpy(p, agent_cb->cli_fn);
		xml_strcpy(p, (const xmlChar *)"> ");
	    } else {
		xml_strncpy(buff, DEF_PROMPT, bufflen);
	    }
	} else {
	    xml_strncpy(buff, DEF_PROMPT, bufflen);
	}
	break;
    case MGR_IO_ST_CONN_IDLE:
    case MGR_IO_ST_CONN_RPYWAIT:
    case MGR_IO_ST_CONN_CANCELWAIT:
    case MGR_IO_ST_CONN_CLOSEWAIT:
    case MGR_IO_ST_CONN_SHUT:
	p = buff;
	len = xml_strncpy(p, (const xmlChar *)"yangcli ", bufflen);
	p += len;
	bufflen -= len;

	if (bufflen == 0) {
	    return;
	}

	parm = NULL;
	if (agent_cb->connect_valset) {
	    parm = val_find_child(agent_cb->connect_valset, 
				  YANGCLI_MOD, 
				  YANGCLI_USER);
	}

	if (!parm && get_connect_valset()) {
	    parm = val_find_child(get_connect_valset(), 
				  YANGCLI_MOD, 
				  YANGCLI_USER);
	}

	if (parm) {
	    len = xml_strncpy(p, VAL_STR(parm), bufflen);
	    p += len;
	    bufflen -= len;
	    if (bufflen == 0) {
		return;
	    }
	    *p++ = NCX_AT_CH;
	    --bufflen;
	    if (bufflen == 0) {
		return;
	    }
	}

	parm = NULL;
	if (agent_cb->connect_valset) {
	    parm = val_find_child(agent_cb->connect_valset, 
				  YANGCLI_MOD, 
				  YANGCLI_AGENT);
	}

	if (!parm && get_connect_valset()) {
	    parm= val_find_child(get_connect_valset(), 
				 YANGCLI_MOD, 
				 YANGCLI_AGENT);
	}

	if (parm) {
	    len = xml_strncpy(p, VAL_STR(parm), bufflen);
	    p += len;
	    bufflen -= len;
	    if (bufflen == 0) {
		return;
	    }
	}

	if (agent_cb->cli_fn && bufflen > 3) {
	    *p++ = ':';
	    len = xml_strncpy(p, agent_cb->cli_fn, --bufflen);
	    p += len;
	    bufflen -= len;
	}

	if (bufflen > 2) {
	    *p++ = '>';
	    *p++ = ' ';
	    *p = 0;
	    bufflen -= 2;
	}
	break;
    default:
	SET_ERROR(ERR_INTERNAL_VAL);
	xml_strncpy(buff, DEF_PROMPT, bufflen);	
	break;
    }

}  /* get_prompt */


/********************************************************************
* get_line
*
* Generate a prompt based on the program state and
* use the tecla library read function to handle
* user keyboard input
*
*
* Do not free this line after use, just discard it
* It will be freed by the tecla lib
*
* INPUTS:
*   agent_cb == agent control block to use
*
* RETURNS:
*   static line from tecla read line, else NULL if some error
*********************************************************************/
static xmlChar *
    get_line (agent_cb_t *agent_cb)
{
    xmlChar          *line;
    xmlChar           prompt[MAX_PROMPT_LEN];
    GlReturnStatus    returnstatus;

    line = NULL;

    get_prompt(agent_cb, prompt, MAX_PROMPT_LEN-1);

    if (!agent_cb->climore) {
	log_stdout("\n");
    }


    agent_cb->returncode = 0;

    if (agent_cb->history_line_active) {
        line = (xmlChar *)
            gl_get_line(agent_cb->cli_gl,
                        (const char *)prompt,
                        (const char *)agent_cb->history_line, 
                        -1);
        agent_cb->history_line_active = FALSE;
    } else {
        line = (xmlChar *)
            gl_get_line(agent_cb->cli_gl,
                        (const char *)prompt,
                        NULL, 
                        -1);
    }

    if (!line) {
	if (agent_cb->returncode == MGR_IO_RC_DROPPED ||
	    agent_cb->returncode == MGR_IO_RC_DROPPED_NOW) {
	    log_write("\nSession was dropped by the agent");
	    return NULL;
	}

	returnstatus = gl_return_status(agent_cb->cli_gl);

        /* treat signal as control-C warning; about to exit */
        if (returnstatus == GLR_SIGNAL) {
	    log_warn("\nWarning: Control-C exit\n\n");            
            return NULL;
        }

        /* skip ordinary line canceled code except if debug2 */
        if (returnstatus == GLR_TIMEOUT && !LOGDEBUG2) {
            return NULL;
        }

	log_error("\nError: GetLine returned ");
	switch (returnstatus) {
	case GLR_NEWLINE:
	    log_error("NEWLINE");
	    break;
	case GLR_BLOCKED:
	    log_error("BLOCKED");
	    break;
	case GLR_SIGNAL:
	    log_error("SIGNAL");
	    break;
	case GLR_TIMEOUT:
	    log_error("TIMEOUT");
	    break;
	case GLR_FDABORT:
	    log_error("FDABORT");
	    break;
	case GLR_EOF:
	    log_error("EOF");
	    break;
	case GLR_ERROR:
	    log_error("ERROR");
	    break;
	default:
	    log_error("<unknown>");
	}
	log_error(" (rt:%u errno:%u)", 
		  agent_cb->returncode,
		  agent_cb->errnocode);
    }

    return line;

} /* get_line */


/********************************************************************
* FUNCTION try_parse_def
* 
* Parse the possibly module-qualified definition (module:def)
* and find the template for the requested definition
*
* INPUTS:
*   agent_cb == agent control block to use
*   modname == module name to try
*   defname == definition name to try
*   dtyp == definition type 
*
* OUTPUTS:
*    *dtyp is set if it started as NONE
*
* RETURNS:
*   pointer to the found definition template or NULL if not found
*********************************************************************/
static void *
    try_parse_def (agent_cb_t *agent_cb,
		   const xmlChar *modname,
		   const xmlChar *defname,
		   ncx_node_t *dtyp)

{
    void          *def;
    ncx_module_t  *mod;

    mod = find_module(agent_cb, modname);
    if (!mod) {
	return NULL;
    }

    def = NULL;
    switch (*dtyp) {
    case NCX_NT_NONE:
	def = ncx_find_object(mod, defname);
	if (def) {
	    *dtyp = NCX_NT_OBJ;
	    break;
	}
	def = ncx_find_grouping(mod, defname);
	if (def) {
	    *dtyp = NCX_NT_GRP;
	    break;
	}
	def = ncx_find_type(mod, defname);
	if (def) {
	    *dtyp = NCX_NT_TYP;
	    break;
	}
	break;
    case NCX_NT_OBJ:
	def = ncx_find_object(mod, defname);
	break;
    case NCX_NT_GRP:
	def = ncx_find_grouping(mod, defname);
	break;
    case NCX_NT_TYP:
	def = ncx_find_type(mod, defname);
	break;
    default:
	def = NULL;
	SET_ERROR(ERR_INTERNAL_VAL);
    }

    return def;
    
} /* try_parse_def */


/********************************************************************
* FUNCTION get_yesno
* 
* Get the user-selected choice, yes or no
*
* INPUTS:
*   agent_cb == agent control block to use
*   prompt == prompt message
*   defcode == default answer code
*      0 == no def answer
*      1 == def answer is yes
*      2 == def answer is no
*   retcode == address of return code
*
* OUTPUTS:
*    *retcode is set if return NO_ERR
*       0 == cancel
*       1 == yes
*       2 == no
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    get_yesno (agent_cb_t *agent_cb,
	       const xmlChar *prompt,
	       uint32 defcode,
	       uint32 *retcode)
{
    xmlChar                 *myline, *str;
    status_t                 res;
    boolean                  done;

    done = FALSE;
    res = NO_ERR;

    set_completion_state(&agent_cb->completion_state,
			 NULL,
			 NULL,
			 CMD_STATE_YESNO);

    if (prompt) {
	log_stdout("\n%s", prompt);
    }
    log_stdout("\nEnter Y for yes, N for no, or C to cancel:");
    switch (defcode) {
    case YESNO_NODEF:
	break;
    case YESNO_YES:
	log_stdout(" [default: Y]");
	break;
    case YESNO_NO:
	log_stdout(" [default: N]");
	break;
    default:
	res = SET_ERROR(ERR_INTERNAL_VAL);
	done = TRUE;
    }


    while (!done) {

	/* get input from the user STDIN */
	myline = get_cmd_line(agent_cb, &res);
	if (!myline) {
	    done = TRUE;
	    continue;
	}

	/* strip leading whitespace */
	str = myline;
	while (*str && xml_isspace(*str)) {
	    str++;
	}

	/* convert to a number, check [ENTER] for default */
	if (*str) {
	    if (*str == 'Y' || *str == 'y') {
		*retcode = YESNO_YES;
		done = TRUE;
	    } else if (*str == 'N' || *str == 'n') {
		*retcode = YESNO_NO;
		done = TRUE;
	    } else if (*str == 'C' || *str == 'c') {
		*retcode = YESNO_CANCEL;
		done = TRUE;
	    }
	} else {
	    /* default accepted */
	    switch (defcode) {
	    case YESNO_NODEF:
		break;
	    case YESNO_YES:
		*retcode = YESNO_YES;
		done = TRUE;
		break;
	    case YESNO_NO:
		*retcode = YESNO_NO;
		done = TRUE;
		break;
	    default:
		res = SET_ERROR(ERR_INTERNAL_VAL);
		done = TRUE;
	    }
	}
	if (res == NO_ERR && !done) {
	    log_stdout("\nError: invalid value '%s'\n", str);
	}
    }

    return res;

}  /* get_yesno */


#if 0    /* in process of removal */
/********************************************************************
* FUNCTION get_complex_parm
* 
* Fill the specified parm, which is a complex type
* This function will block on readline to get input from the user
*
* INPUTS:
*   agent_cb == agent control block to use
*   parm == parm to get from the CLI
*   valset == value set being filled
*
* OUTPUTS:
*    new val_value_t node will be added to valset if NO_ERR
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    get_complex_parm (agent_cb_t *agent_cb,
		      const obj_template_t *parm,
		      val_value_t *valset)
{
    xmlChar          *line;
    val_value_t      *new_parm;
    status_t          res;

    res = NO_ERR;

    log_stdout("\nEnter parameter value %s (%s)", 
	       obj_get_name(parm), 
	       tk_get_btype_sym(obj_get_basetype(parm)));

    /* get a line of input from the user */
    line = get_cmd_line(agent_cb, &res);
    if (!line) {
	return res;
    }

    new_parm = val_new_value();
    if (!new_parm) {
	res = ERR_INTERNAL_MEM;
    } else {
	val_init_from_template(new_parm, parm);
	(void)var_get_script_val(parm,
				 new_parm,
				 line, 
                                 ISPARM, 
                                 &res);
	if (res == NO_ERR) {
	    /* add the parm to the parmset */
	    val_add_child(new_parm, valset);
	} else {
            val_free_value(new_parm);
        }
    }

    if (res != NO_ERR) {
	log_stdout("\nyangcli: Error in %s (%s)",
		   obj_get_name(parm), 
                   get_error_string(res));
    }

    return res;
    
} /* get_complex_parm */
#endif


/********************************************************************
* FUNCTION get_parm
* 
* Fill the specified parm
* Use the old value for the default if any
* This function will block on readline to get input from the user
*
* INPUTS:
*   agent_cb == agent control block to use
*   rpc == RPC method that is being called
*   parm == parm to get from the CLI
*   valset == value set being filled
*   oldvalset == last set of values (NULL if none)
*
* OUTPUTS:
*    new val_value_t node will be added to valset if NO_ERR
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    get_parm (agent_cb_t *agent_cb,
	      const obj_template_t *rpc,
	      const obj_template_t *parm,
	      val_value_t *valset,
	      val_value_t *oldvalset)
{
    const xmlChar    *def, *parmname, *str;
    const typ_def_t  *typdef;
    val_value_t      *oldparm, *newparm;
    xmlChar          *line, *start, *objbuff, *buff;
    xmlChar          *line2, *start2, *saveline;
    status_t          res;
    ncx_btype_t       btyp;
    boolean           done, ispassword, iscomplex;
    uint32            len;
    int               glstatus;

    if (!obj_is_mandatory(parm) && !agent_cb->get_optional) {
	return NO_ERR;
    }

    def = NULL;
    iscomplex = FALSE;
    btyp = obj_get_basetype(parm);
    res = NO_ERR;
    ispassword = obj_is_password(parm);

    if (obj_is_data_db(parm) && 
        parm->objtype != OBJ_TYP_RPCIO) {
	objbuff = NULL;
	res = obj_gen_object_id(parm, &objbuff);
	if (res != NO_ERR) {
	    log_error("\nError: generate object ID failed (%s)",
		      get_error_string(res));
	    return res;
	}

	/* let the user know about the new nest level */
	if (obj_is_key(parm)) {
	    str = YANG_K_KEY;
	} else if (ispassword) {
            str = YANGCLI_PASSWORD;
	} else if (obj_is_mandatory(parm)) {
	    str = YANG_K_MANDATORY;
	} else {
	    str = YANGCLI_OPTIONAL;
	}

	log_stdout("\nFilling %s %s %s:", 
		   str,
		   obj_get_typestr(parm), 
		   objbuff);
	    
	m__free(objbuff);
    }

    switch (btyp) {
    case NCX_BT_ANY:
    case NCX_BT_CONTAINER:
        iscomplex = TRUE;
        break;
	/* return get_complex_parm(agent_cb, parm, valset); */
    default:
	;
    }

    parmname = obj_get_name(parm);
    typdef = obj_get_ctypdef(parm);
    btyp = obj_get_basetype(parm);
    res = NO_ERR;
    oldparm = NULL;
  
    done = FALSE;
    while (!done) {
	if (btyp==NCX_BT_EMPTY) {
	    log_stdout("\nShould flag %s be set? [Y, N]", 
		       parmname);
	} else if (iscomplex) {
	    log_stdout("\nEnter value for %s <%s>",
		       obj_get_typestr(parm),
		       parmname);
        } else {
	    log_stdout("\nEnter %s value for %s <%s>",
		       (const xmlChar *)tk_get_btype_sym(btyp),
		       obj_get_typestr(parm),
		       parmname);
	}
	if (oldvalset) {
	    oldparm = val_find_child(oldvalset, 
				     obj_get_mod_name(parm),
				     parmname);
	}

	/* pick a default value, either old value or default clause */
	if (oldparm) {
	    /* use the old value for the default */
	    if (btyp==NCX_BT_EMPTY) {
		log_stdout(" [Y]");
            } else if (iscomplex) {
                log_stdout(" [%s contents not shown]",
                           obj_get_typestr(parm));
	    } else {
		res = val_sprintf_simval_nc(NULL, oldparm, &len);
		if (res != NO_ERR) {
		    return SET_ERROR(res);
		}
		buff = m__getMem(len+1);
		if (!buff) {
		    return ERR_INTERNAL_MEM;
		}
		res = val_sprintf_simval_nc(buff, oldparm, &len);
		if (res == NO_ERR) {
		    log_stdout(" [%s]", buff);
		}
		m__free(buff);
	    }
        } else {
            def = NULL;

	    /* try to get the defined default value */
	    if (btyp != NCX_BT_EMPTY && !iscomplex) {
		def = obj_get_default(parm);
		if (!def && (obj_get_nsid(rpc) == xmlns_nc_id() &&
			     (!xml_strcmp(parmname, NCX_EL_TARGET) ||
			      !xml_strcmp(parmname, NCX_EL_SOURCE)))) {
		    /* offer the default target for the NETCONF
		     * <source> and <target> parameters
		     */
		    def = agent_cb->default_target;
		}
	    }

	    if (def) {
		log_stdout(" [%s]\n", def);
	    } else if (btyp==NCX_BT_EMPTY) {
		log_stdout(" [N]\n");
	    }
	}

	set_completion_state_curparm(&agent_cb->completion_state,
				     parm);

	/* get a line of input from the user
         * but don't echo if this is a password parm
         */
        if (ispassword) {
            glstatus = gl_echo_mode(agent_cb->cli_gl, 0);
        }
                                
	line = get_cmd_line(agent_cb, &res);

        if (ispassword) {
            glstatus = gl_echo_mode(agent_cb->cli_gl, 1);
        }

	if (!line) {
	    return res;
	}

	/* skip whitespace */
	start = line;
	while (*start && xml_isspace(*start)) {
	    start++;
	}

	/* check for question-mark char sequences */
	if (*start == '?') {
	    if (start[1] == '?') {
		/* ?? == full help */
		obj_dump_template(parm, 
                                  HELP_MODE_FULL, 
                                  0,
				  NCX_DEF_INDENT);
	    } else if (start[1] == 'C' || start[1] == 'c') {
		/* ?c or ?C == cancel the operation */
		log_stdout("\n%s command canceled",
			   obj_get_name(rpc));
		return ERR_NCX_CANCELED;
	    } else if (start[1] == 'S' || start[1] == 's') {
		/* ?s or ?S == skip this parameter */
		log_stdout("\n%s parameter skipped",
			   obj_get_name(parm));
		return ERR_NCX_SKIPPED;
	    } else {
		/* ? == normal help mode */
		obj_dump_template(parm, 
                                  HELP_MODE_NORMAL, 
                                  4,
				  NCX_DEF_INDENT);
	    }
	    log_stdout("\n");
	    continue;
	} else {
	    /* top loop to get_parm is only executed once */
	    done = TRUE;
	}
    }

    /* check if any non-whitespace chars entered */
    if (!*start) {
	/* no input, use default or old value or EMPTY_STRING */
	if (def) {
	    /* use default */
	    res = cli_parse_parm_ex(valset, 
				    parm, 
				    def, 
				    SCRIPTMODE, 
				    get_baddata());
	} else if (oldparm) {
	    /* no default, try old value */
	    if (btyp==NCX_BT_EMPTY) {
		res = cli_parse_parm_ex(valset, 
					parm, 
					NULL,
					SCRIPTMODE, 
					get_baddata());
	    } else {
		/* use a copy of the last value */
		newparm = val_clone(oldparm);
		if (!newparm) {
		    res = ERR_INTERNAL_MEM;
		} else {
		    val_add_child(newparm, valset);
		}
	    }
	} else if (btyp == NCX_BT_EMPTY) {
	    res = cli_parse_parm_ex(valset, 
				    parm, 
				    NULL,
				    SCRIPTMODE, 
				    get_baddata());
	} else if (!iscomplex && 
                   (val_simval_ok(obj_get_ctypdef(parm), 
                                  EMPTY_STRING) == NO_ERR)) {
            res = cli_parse_parm_ex(valset, 
                                    parm, 
                                    EMPTY_STRING,
                                    SCRIPTMODE, 
                                    get_baddata());
        } else {
            /* data type requires some form of input */
            res = ERR_NCX_DATA_MISSING;
        }
    } else if (btyp==NCX_BT_EMPTY) {
	/* empty data type handled special Y: set, N: leave out */
	if (*start=='Y' || *start=='y') {
	    res = cli_parse_parm_ex(valset, 
				    parm, 
				    NULL, 
				    SCRIPTMODE, 
				    get_baddata());
	} else if (*start=='N' || *start=='n') {
	    ; /* skip; do not add the flag */
	} else if (oldparm) {
	    /* previous value was set, so add this flag */
	    res = cli_parse_parm_ex(valset, 
				    parm, 
				    NULL,
				    SCRIPTMODE, 
				    get_baddata());
	} else {
	    /* some value was entered, other than Y or N */
	    res = ERR_NCX_WRONG_VAL;
	}
    } else {
	/* normal case: input for regular data type */
	res = cli_parse_parm_ex(valset, 
				parm, 
				start,
				SCRIPTMODE, 
				get_baddata());
    }


    /* got some value, now check if it is OK
     * depending on the --bad-data CLI parameter,
     * the value may be entered over again, accepted,
     * or rejected with an invalid-value error
     */
    if (res != NO_ERR) {
	switch (get_baddata()) {
	case NCX_BAD_DATA_IGNORE:
	case NCX_BAD_DATA_WARN:
	    /* if these modes had error return status then the
	     * problem was not invalid value; maybe malloc error
	     */
	    break;
	case NCX_BAD_DATA_CHECK:
	    if (NEED_EXIT(res)) {
		break;
	    }

	    saveline = (start) ? xml_strdup(start) :
		xml_strdup(EMPTY_STRING);
	    if (!saveline) {
		res = ERR_INTERNAL_MEM;
		break;
	    }

	    done = FALSE;
	    while (!done) {
		log_stdout("\nError: parameter '%s' value '%s' is invalid"
			   "\nShould this value be used anyway? [Y, N]"
			   " [N]", 
			   obj_get_name(parm),
			   (start) ? start : EMPTY_STRING);

		/* save the previous value because it is about
		 * to get trashed by getting a new line
		 */

		/* get a line of input from the user */
		line2 = get_cmd_line(agent_cb, &res);
		if (!line2) {
		    m__free(saveline);
		    return res;
		}

		/* skip whitespace */
		start2 = line2;
		while (*start2 && xml_isspace(*start2)) {
		    start2++;
		}

		/* check for question-mark char sequences */
		if (!*start2) {
		    /* default N: try again for a different input */
		    m__free(saveline);
		    res = get_parm(agent_cb, rpc, parm, valset, oldvalset);
		    done = TRUE;
		} else if (*start2 == '?') {
		    if (start2[1] == '?') {
			/* ?? == full help */
			obj_dump_template(parm, 
					  HELP_MODE_FULL, 
					  0,
					  NCX_DEF_INDENT);
		    } else if (start2[1] == 'C' || start2[1] == 'c') {
			/* ?c or ?C == cancel the operation */
			log_stdout("\n%s command canceled",
				   obj_get_name(rpc));
			m__free(saveline);
			res = ERR_NCX_CANCELED;
			done = TRUE;
		    } else if (start2[1] == 'S' || start2[1] == 's') {
			/* ?s or ?S == skip this parameter */
			log_stdout("\n%s parameter skipped",
				   obj_get_name(parm));
			m__free(saveline);
			res = ERR_NCX_SKIPPED;
			done = TRUE;
		    } else {
			/* ? == normal help mode */
			obj_dump_template(parm, 
                                          HELP_MODE_NORMAL, 
                                          4,
					  NCX_DEF_INDENT);
		    }
		    log_stdout("\n");
		    continue;
		} else if (*start2 == 'Y' || *start2 == 'y') {
		    /* use the invalid value */
		    res = cli_parse_parm_ex(valset, 
					    parm, 
					    saveline, 
					    SCRIPTMODE, 
					    NCX_BAD_DATA_IGNORE);
		    m__free(saveline);
		    done = TRUE;
		} else if (*start2 == 'N' || *start2 == 'n') {
		    /* recurse: try again for a different input */
		    m__free(saveline);
		    res = get_parm(agent_cb, 
                                   rpc, 
                                   parm, 
                                   valset, 
                                   oldvalset);
		    done = TRUE;
		} else {
		    log_stdout("\nInvalid input.");
		}
	    }
	    break;
	case NCX_BAD_DATA_ERROR:
	    log_stdout("\nError: set parameter '%s' failed (%s)\n",
		       obj_get_name(parm),
		       get_error_string(res));
	    break;
	default:
	    SET_ERROR(ERR_INTERNAL_VAL);
	}
    }

    return res;
    
} /* get_parm */


/********************************************************************
* FUNCTION get_case
* 
* Get the user-selected case, which is not fully set
* Use values from the last set (if any) for defaults.
* This function will block on readline if mandatory parms
* are needed from the CLI
*
* INPUTS:
*   agent_cb == agent control block to use
*   rpc == RPC template in progress
*   cas == case object template header
*   valset == value set to fill
*   oldvalset == last set of values (or NULL if none)
*
* OUTPUTS:
*    new val_value_t nodes may be added to valset
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    get_case (agent_cb_t *agent_cb,
	      const obj_template_t *rpc,
	      const obj_template_t *cas,
	      val_value_t *valset,
	      val_value_t *oldvalset)
{
    const obj_template_t    *parm;
    val_value_t             *pval;
    xmlChar                 *objbuff;
    const xmlChar           *str;
    status_t                 res;
    boolean                  saveopt;

    /* make sure a case was selected or found */
    if (!obj_is_config(cas) || obj_is_abstract(cas)) {
	log_stdout("\nError: No writable objects to fill for this case");
	return ERR_NCX_SKIPPED;
    }

    saveopt = agent_cb->get_optional;

#if 0
    /* think this is not needed; user must set $$optional to true
     * in order to fill in the optional nodes 
     */
    agent_cb->get_optional = TRUE;
#endif

    res = NO_ERR;

    if (obj_is_data_db(cas)) {
        objbuff = NULL;
        res = obj_gen_object_id(cas, &objbuff);
        if (res != NO_ERR) {
            log_error("\nError: generate object ID failed (%s)",
                      get_error_string(res));
            return res;
        }

        /* let the user know about the new nest level */
        if (obj_is_mandatory(cas)) {
            str = YANG_K_MANDATORY;
        } else {
            str = (const xmlChar *)"optional";
        }

        log_stdout("\nFilling %s case %s:", 
                   str,
                   objbuff);
	    
        m__free(objbuff);
    }

    /* corner-case: user selected a case, and that case has
     * one empty leaf in it; 
     * e.g., <source> and <target> parms
     */
    if (obj_get_child_count(cas) == 1) {
	parm = obj_first_child(cas);
	if (parm && obj_get_basetype(parm)==NCX_BT_EMPTY) {
	    return cli_parse_parm(valset, parm, NULL, FALSE);
	}
    }

    /* finish the selected case */
    for (parm = obj_first_child(cas);
	 parm != NULL && res == NO_ERR;
	 parm = obj_next_child(parm)) {

	if (!obj_is_config(parm) || obj_is_abstract(parm)) {
	    continue;
	}

	pval = val_find_child(valset, 
			      obj_get_mod_name(parm),
			      obj_get_name(parm));
	if (pval) {
	    continue;
	}

	/* node is config and not already set */
	res = get_parm(agent_cb, rpc, parm, valset, oldvalset);

	if (res == ERR_NCX_SKIPPED) {
	    res = NO_ERR;
	}
    }

    agent_cb->get_optional = saveopt;
    return res;

} /* get_case */


/********************************************************************
* FUNCTION get_choice
* 
* Get the user-selected choice, which is not fully set
* Use values from the last set (if any) for defaults.
* This function will block on readline if mandatory parms
* are needed from the CLI
*
* INPUTS:
*   agent_cb == agent control block to use
*   rpc == RPC template in progress
*   choic == choice object template header
*   valset == value set to fill
*   oldvalset == last set of values (or NULL if none)
*
* OUTPUTS:
*    new val_value_t nodes may be added to valset
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    get_choice (agent_cb_t *agent_cb,
		const obj_template_t *rpc,
		const obj_template_t *choic,
		val_value_t *valset,
		val_value_t *oldvalset)
{
    const obj_template_t    *parm, *cas, *usecase;
    val_value_t             *pval;
    xmlChar                 *myline, *str, *objbuff;
    status_t                 res;
    int                      casenum, num;
    boolean                  first, done, usedef, redo, saveopt;

    if (!obj_is_config(choic) || obj_is_abstract(choic)) {
	log_stdout("\nError: choice '%s' has no configurable parameters",
		   obj_get_name(choic));
	return ERR_NCX_ACCESS_DENIED;
    }

    casenum = 0;
    res = NO_ERR;

    if (obj_is_data_db(choic)) {
	objbuff = NULL;
	res = obj_gen_object_id(choic, &objbuff);
	if (res != NO_ERR) {
	    log_error("\nError: generate object ID failed (%s)",
		      get_error_string(res));
	    return res;
	}

	/* let the user know about the new nest level */
	log_stdout("\nFilling choice %s:", objbuff);
	m__free(objbuff);
    }

    saveopt = agent_cb->get_optional;
    
#if 0
    /* now think this is not actually needed.
     * It will not ensure that some selection is made
     * for a mandatory choice
     */
    if (obj_is_mandatory(choic)) {
	agent_cb->get_optional = TRUE;
    }
#endif

    /* first check the partial block corner case */
    pval = val_get_choice_first_set(valset, choic);
    if (pval) {
	/* found something set from this choice, finish the case */
	log_stdout("\nEnter more parameters to complete the choice:");

	cas = pval->casobj;
	if (!cas) {
	    agent_cb->get_optional = saveopt;
	    return SET_ERROR(ERR_INTERNAL_VAL);
	}
	for (parm = obj_first_child(cas); 
	     parm != NULL;
	     parm = obj_next_child(parm)) {

	    if (!obj_is_config(parm) || obj_is_abstract(parm)) {
		continue;
	    }

	    pval = val_find_child(valset,
				  obj_get_mod_name(parm),
				  obj_get_name(parm));
	    if (pval) {
		continue;   /* node within case already set */
	    }

	    res = get_parm(agent_cb, rpc, parm, valset, oldvalset);
	    switch (res) {
	    case NO_ERR:
		break;
	    case ERR_NCX_SKIPPED:
		res = NO_ERR;
		break;
	    case ERR_NCX_CANCELED:
		agent_cb->get_optional = saveopt;
		return res;
	    default:
		agent_cb->get_optional = saveopt;
		return res;
	    }
	}
	agent_cb->get_optional = saveopt;
	return NO_ERR;
    }

    /* check corner-case -- choice with no cases defined */
    cas = obj_first_child(choic);
    if (!cas) {
	log_stdout("\nNo case nodes defined for choice %s\n",
		   obj_get_name(choic));
	agent_cb->get_optional = saveopt;
	return NO_ERR;
    }


    /* else not a partial block corner case but a normal
     * situation where no case has been selected at all
     */
    log_stdout("\nEnter a number of the selected case statement:\n");

    num = 1;
    usedef = FALSE;

    for (; cas != NULL;
         cas = obj_next_child(cas)) {

	first = TRUE;
	for (parm = obj_first_child(cas);
	     parm != NULL;
	     parm = obj_next_child(parm)) {

	    if (!obj_is_config(parm) || obj_is_abstract(parm)) {
		continue;
	    }

	    if (first) {
		log_stdout("\n  %d: case %s:", 
			   num++, obj_get_name(cas));
		first = FALSE;
	    }

	    log_stdout("\n       %s %s",
		       obj_get_typestr(parm),
		       obj_get_name(parm));
	}
    }

    done = FALSE;
    log_stdout("\n");
    while (!done) {

	redo = FALSE;

	/* Pick a prompt, depending on the choice default case */
	if (obj_get_default(choic)) {
	    log_stdout("\nEnter choice number [%d - %d], "
		       "[ENTER] for default (%s): ",
		       1, 
		       num-1,
		       obj_get_default(choic));
	} else {
	    log_stdout("\nEnter choice number [%d - %d]: ",
		       1, 
		       num-1);
	}

	/* get input from the user STDIN */
	myline = get_cmd_line(agent_cb, &res);
	if (!myline) {
	    agent_cb->get_optional = saveopt;
	    return res;
	}

	/* strip leading whitespace */
	str = myline;
	while (*str && xml_isspace(*str)) {
	    str++;
	}

	/* convert to a number, check [ENTER] for default */
	if (!*str) {
	    usedef = TRUE;
	} else if (*str == '?') {
	    redo = TRUE;
	    if (str[1] == '?') {
		obj_dump_template(choic, 
                                  HELP_MODE_FULL, 
                                  0,
				  NCX_DEF_INDENT);
	    } else if (str[1] == 'C' || str[1] == 'c') {
		log_stdout("\n%s command canceled\n",
			   obj_get_name(rpc));
		agent_cb->get_optional = saveopt;
		return ERR_NCX_CANCELED;
	    } else if (str[1] == 'S' || str[1] == 's') {
		log_stdout("\n%s choice skipped\n",
			   obj_get_name(choic));
		agent_cb->get_optional = saveopt;
		return ERR_NCX_SKIPPED;
	    } else {
		obj_dump_template(choic, 
                                  HELP_MODE_NORMAL,
                                  4,
				  NCX_DEF_INDENT);
	    }
	    log_stdout("\n");
	} else {
	    casenum = atoi((const char *)str);
	    usedef = FALSE;
	}

	if (redo) {
	    continue;
	}

	/* check if default requested */
	if (usedef) {
	    if (obj_get_default(choic)) {
		done = TRUE;
	    } else {
		log_stdout("\nError: Choice does not have "
                           "a default case\n");
		usedef = FALSE;
	    }
	} else if (casenum < 0 || casenum >= num) {
	    log_stdout("\nError: invalid value '%s'\n", str);
	} else {
	    done = TRUE;
	}
    }

    /* got a valid choice number or use the default case
     * now get the object template for the correct case 
     */
    if (usedef) {
	cas = obj_find_child(choic,
			     obj_get_mod_name(choic),
			     obj_get_default(choic));
    } else {

	num = 1;
	done = FALSE;
	usecase = NULL;

	for (cas = obj_first_child(choic); 
	     cas != NULL && !done;
	     cas = obj_next_child(cas)) {

	    if (!obj_is_config(cas) || obj_is_abstract(cas)) {
		continue;
	    }

	    if (casenum == num) {
		done = TRUE;
		usecase = cas;
	    } else {
		num++;
	    }
	}

	cas = usecase;
    }

    /* make sure a case was selected or found */
    if (!cas) {
	log_stdout("\nError: No case to fill for this choice");
	agent_cb->get_optional = saveopt;
	return ERR_NCX_SKIPPED;
    }

    res = get_case(agent_cb, rpc, cas, valset, oldvalset);
    switch (res) {
    case NO_ERR:
	break;
    case ERR_NCX_SKIPPED:
	res = NO_ERR;
	break;
    case ERR_NCX_CANCELED:
	break;
    default:
	;
    }

    agent_cb->get_optional = saveopt;

    return res;

} /* get_choice */


/********************************************************************
* FUNCTION fill_value
* 
* Malloc and fill the specified value
* Use the last value for the value if it is present and valid
* This function will block on readline for user input if no
* valid useval is given
*
* INPUTS:
*   agent_cb == agent control block to use
*   rpc == RPC method that is being called
*   parm == object for value to fill
*   useval == value to use (or NULL if none)
*   res == address of return status
*
* OUTPUTS:
*    *res == return status
*
* RETURNS:
*    malloced value, filled in or NULL if some error
*********************************************************************/
static val_value_t *
    fill_value (agent_cb_t *agent_cb,
		const obj_template_t *rpc,
		const obj_template_t *parm,
		val_value_t *useval,
		status_t  *res)
{
    const obj_template_t  *parentobj;
    val_value_t           *dummy, *newval;
    boolean                saveopt;

    switch (parm->objtype) {
    case OBJ_TYP_ANYXML:
    case OBJ_TYP_LEAF:
    case OBJ_TYP_LEAF_LIST:
	break;
    default:
	*res = SET_ERROR(ERR_INTERNAL_VAL);
	return NULL;
    }

    if (obj_is_abstract(parm)) {
	*res = ERR_NCX_NO_ACCESS_MAX;
	log_error("\nError: no access to abstract objects");
	return NULL;
    }

    /* just copy the useval if it is given */
    if (useval) {
        newval = val_clone(useval);
        if (!newval) {
	    log_error("\nError: malloc failed");
	    return NULL;
        }
        if (newval->obj != parm) {
            /* force the new value to have the QName of 'parm' */
            val_change_nsid(newval, obj_get_nsid(parm));
            val_set_name(newval, 
                         obj_get_name(parm),
                         xml_strlen(obj_get_name(parm)));
        }
	return newval;
    }	

    /* since the CLI and original NCX language were never designed
     * to support top-level leafs, a dummy container must be
     * created for the new and old leaf or leaf-list entries
     */
    if (parm->parent && !obj_is_root(parm->parent)) {
	parentobj = parm->parent;
    } else {
	parentobj = ncx_get_gen_container();
    }

    dummy = val_new_value();
    if (!dummy) {
	*res = ERR_INTERNAL_MEM;
	log_error("\nError: malloc failed");
	return NULL;
    }
    val_init_from_template(dummy, parentobj);

    agent_cb->cli_fn = obj_get_name(rpc);

    newval = NULL;
    saveopt = agent_cb->get_optional;
    agent_cb->get_optional = TRUE;

    set_completion_state(&agent_cb->completion_state,
			 rpc,
			 parm,
			 CMD_STATE_GETVAL);

    *res = get_parm(agent_cb, rpc, parm, dummy, NULL);
    agent_cb->get_optional = saveopt;

    if (*res == NO_ERR) {
	newval = val_get_first_child(dummy);
	if (newval) {
	    val_remove_child(newval);
	}
    }
    agent_cb->cli_fn = NULL;
    val_free_value(dummy);
    return newval;

} /* fill_value */


/********************************************************************
* FUNCTION fill_valset
* 
* Fill the specified value set with any missing parameters.
* Use values from the last set (if any) for defaults.
* This function will block on readline if mandatory parms
* are needed from the CLI
*
* INPUTS:
*   agent_cb == current agent control block (NULL if none)
*   rpc == RPC method that is being called
*   valset == value set to fill
*   oldvalset == last set of values (or NULL if none)
*
* OUTPUTS:
*    new val_value_t nodes may be added to valset
*
* RETURNS:
*    status,, valset may be partially filled if not NO_ERR
*********************************************************************/
static status_t
    fill_valset (agent_cb_t *agent_cb,
		 const obj_template_t *rpc,
		 val_value_t *valset,
		 val_value_t *oldvalset)
{
    const obj_template_t  *parm;
    val_value_t           *val, *oldval;
    xmlChar               *objbuff;
    status_t               res;
    boolean                done;
    uint32                 yesnocode;

    res = NO_ERR;
    agent_cb->cli_fn = obj_get_name(rpc);

    if (!(valset->obj->objtype == OBJ_TYP_RPC ||
          valset->obj->objtype == OBJ_TYP_RPCIO)) {
        objbuff = NULL;
        res = obj_gen_object_id(valset->obj, &objbuff);
        if (res != NO_ERR) {
            log_error("\nError: generate object ID failed (%s)",
                      get_error_string(res));
            return res;
        }

        /* let the user know about the new nest level */
        log_stdout("\nFilling %s %s:",
                   obj_get_typestr(valset->obj), 
                   objbuff);
        m__free(objbuff);
    }

    for (parm = obj_first_child(valset->obj);
         parm != NULL && res==NO_ERR;
         parm = obj_next_child(parm)) {

	if (!obj_is_config(parm) || obj_is_abstract(parm)) {
	    continue;
	}

	if (!agent_cb->get_optional && !obj_is_mandatory(parm)) {
	    continue;
	}

	set_completion_state(&agent_cb->completion_state,
			     rpc,
			     parm,
			     CMD_STATE_GETVAL);			     

        switch (parm->objtype) {
        case OBJ_TYP_CHOICE:
	    if (!val_choice_is_set(valset, parm)) {
		res = get_choice(agent_cb, 
                                 rpc, 
                                 parm, 
				 valset, 
                                 oldvalset);
		switch (res) {
		case NO_ERR:
		    break;
		case ERR_NCX_SKIPPED:
		    res = NO_ERR;
		    break;
		case ERR_NCX_CANCELED:
		    break;
		default:
		    ;
		}
	    }
            break;
        case OBJ_TYP_ANYXML:
        case OBJ_TYP_LEAF:
	    val = val_find_child(valset, 
				 obj_get_mod_name(parm),
				 obj_get_name(parm));
	    if (!val) {
		res = get_parm(agent_cb, rpc, parm, valset, oldvalset);
		switch (res) {
		case NO_ERR:
		    break;
		case ERR_NCX_SKIPPED:
		    res = NO_ERR;
		    break;
		case ERR_NCX_CANCELED:
		    break;
		default:
		    ;
		}
	    }
	    break;
	case OBJ_TYP_LEAF_LIST:
	    done = FALSE;
	    while (!done && res == NO_ERR) {
		res = get_parm(agent_cb, rpc, parm, valset, oldvalset);
		switch (res) {
		case NO_ERR:
		    /* prompt for more leaf-list objects */
		    res = get_yesno(agent_cb, YANGCLI_PR_LLIST,
				    YESNO_NO, &yesnocode);
		    if (res == NO_ERR) {
			switch (yesnocode) {
			case YESNO_CANCEL:
			    res = ERR_NCX_CANCELED;
			    break;
			case YESNO_YES:
			    break;
			case YESNO_NO:
			    done = TRUE;
			    break;
			default:
			    res = SET_ERROR(ERR_INTERNAL_VAL);
			}
		    }
		    break;
		case ERR_NCX_SKIPPED:
		    done = TRUE;
		    res = NO_ERR;
		    break;
		case ERR_NCX_CANCELED:
		    break;
		default:
		    ;
		}
	    }
	    break;
	case OBJ_TYP_CONTAINER:
	case OBJ_TYP_NOTIF:
	case OBJ_TYP_RPCIO:
	    /* if the parm is not already set and is not read-only
	     * then try to get a value from the user at the CLI
	     */
	    val = val_find_child(valset, 
				 obj_get_mod_name(parm),
				 obj_get_name(parm));
	    if (val) {
		break;
	    }
			
	    if (oldvalset) {
		oldval = val_find_child(oldvalset, 
					obj_get_mod_name(parm),
					obj_get_name(parm));
	    } else {
		oldval = NULL;
	    }

	    val = val_new_value();
	    if (!val) {
		res = ERR_INTERNAL_MEM;
		log_error("\nError: malloc of new value failed");
		break;
	    } else {
		val_init_from_template(val, parm);
		val_add_child(val, valset);
	    }

	    /* recurse with the child nodes */
	    res = fill_valset(agent_cb, rpc, val, oldval);

	    switch (res) {
	    case NO_ERR:
		break;
	    case ERR_NCX_SKIPPED:
		res = NO_ERR;
		break;
	    case ERR_NCX_CANCELED:
		break;
	    default:
		;
	    }
	    break;
	case OBJ_TYP_LIST:
	    done = FALSE;
	    while (!done && res == NO_ERR) {
		val = val_new_value();
		if (!val) {
		    res = ERR_INTERNAL_MEM;
		    log_error("\nError: malloc of new value failed");
		    continue;
		} else {
		    val_init_from_template(val, parm);
		    val_add_child(val, valset);
		}

		/* recurse with the child node -- NO OLD VALUE
		 * TBD: get keys, then look up old matching entry
		 */
		res = fill_valset(agent_cb, rpc, val, NULL);

		switch (res) {
		case NO_ERR:
		    /* prompt for more list entries */
		    res = get_yesno(agent_cb, YANGCLI_PR_LIST,
				    YESNO_NO, &yesnocode);
		    if (res == NO_ERR) {
			switch (yesnocode) {
			case YESNO_CANCEL:
			    res = ERR_NCX_CANCELED;
			    break;
			case YESNO_YES:
			    break;
			case YESNO_NO:
			    done = TRUE;
			    break;
			default:
			    res = SET_ERROR(ERR_INTERNAL_VAL);
			}
		    }
		    break;
		case ERR_NCX_SKIPPED:
		    res = NO_ERR;
		    break;
		case ERR_NCX_CANCELED:
		    break;
		default:
		    ;
		}
	    }
            break;
        default: 
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
    }

    agent_cb->cli_fn = NULL;
    return res;

} /* fill_valset */


/********************************************************************
 * FUNCTION get_valset
 * 
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the command being processed
 *    line == CLI input in progress
 *    res == address of status result
 *
 * OUTPUTS:
 *    *res is set to the status
 *
 * RETURNS:
 *   malloced valset filled in with the parameters for 
 *   the specified RPC
 *
 *********************************************************************/
static val_value_t *
    get_valset (agent_cb_t *agent_cb,
		const obj_template_t *rpc,
		const xmlChar *line,
		status_t  *res)
{
    const obj_template_t  *obj;
    val_value_t           *valset;
    uint32                 len;

    *res = NO_ERR;
    valset = NULL;
    len = 0;

    set_completion_state(&agent_cb->completion_state,
			 rpc,
			 NULL,
			 CMD_STATE_GETVAL);

    /* skip leading whitespace */
    while (line[len] && xml_isspace(line[len])) {
	len++;
    }

    /* check any non-whitespace entered after RPC method name */
    if (line[len]) {
	valset = parse_rpc_cli(rpc, &line[len], res);
	if (*res == ERR_NCX_SKIPPED) {
	    log_stdout("\nError: no parameters defined for RPC %s",
		       obj_get_name(rpc));
	} else if (*res != NO_ERR) {
	    log_stdout("\nError in the parameters for RPC %s (%s)",
		       obj_get_name(rpc), 
                       get_error_string(*res));
	}
    }

    obj = obj_find_child(rpc, NULL, YANG_K_INPUT);
    if (!obj || !obj_get_child_count(obj)) {
	*res = ERR_NCX_SKIPPED;
	if (valset) {
	    val_free_value(valset);
	}
	return NULL;
    }

    /* check no input from user, so start a parmset */
    if (*res == NO_ERR && !valset) {
	valset = val_new_value();
	if (!valset) {
	    *res = ERR_INTERNAL_MEM;
	} else {
	    val_init_from_template(valset, obj);
	}
    }

    /* fill in any missing parameters from the CLI */
    if (*res==NO_ERR && interactive_mode()) {
	*res = fill_valset(agent_cb, rpc, valset, NULL);
    }

    if (*res==NO_ERR) {
        *res = val_instance_check(valset, valset->obj);
    }
    return valset;

}  /* get_valset */


/********************************************************************
* FUNCTION create_session
* 
* Start a NETCONF session and change the program state
* Since this is called in sequence with readline, the STDIN IO
* handler may get called if the user enters keyboard text 
*
* The STDIN handler will not do anything with incoming chars
* while state == MGR_IO_ST_CONNECT
* 
* INPUTS:
*   agent_cb == agent control block to use
*
* OUTPUTS:
*   'agent_cb->mysid' is set to the output session ID, if NO_ERR
*   'agent_cb->state' is changed based on the success of the session setup
*
*********************************************************************/
static void
    create_session (agent_cb_t *agent_cb)
{
    const xmlChar *agent, *username, *password;
    modptr_t      *modptr;
    val_value_t   *val;
    status_t       res;
    uint16         port;

    if (LOGDEBUG) {
        log_debug("\nConnect attempt with following parameters:");
        val_dump_value(agent_cb->connect_valset, NCX_DEF_INDENT);
        log_debug("\n");
    }
    
    /* make sure session not already running */
    if (agent_cb->mysid) {
	if (mgr_ses_get_scb(agent_cb->mysid)) {
	    /* already connected; fn should not have been called */
	    SET_ERROR(ERR_INTERNAL_INIT_SEQ);
	    return;
	} else {
	    /* OK: reset session ID */
	    agent_cb->mysid = 0;
	}
    }

    /* make sure no stale modules in the control block */
    while (!dlq_empty(&agent_cb->modptrQ)) {
	modptr = (modptr_t *)dlq_deque(&agent_cb->modptrQ);
	free_modptr(modptr);
    }

    /* retrieving the parameters should not fail */
    val =  val_find_child(agent_cb->connect_valset, 
			  YANGCLI_MOD, 
			  YANGCLI_USER);
    if (val && val->res == NO_ERR) {
	username = VAL_STR(val);
    } else {
	SET_ERROR(ERR_INTERNAL_VAL);
	return;
    }

    val = val_find_child(agent_cb->connect_valset,
			 YANGCLI_MOD, 
			 YANGCLI_AGENT);
    if (val && val->res == NO_ERR) {
	agent = VAL_STR(val);
    } else {
	SET_ERROR(ERR_INTERNAL_VAL);
	return;
    }

    val = val_find_child(agent_cb->connect_valset,
			 YANGCLI_MOD, 
			 YANGCLI_PASSWORD);
    if (val && val->res == NO_ERR) {
	password = VAL_STR(val);
    } else {
	SET_ERROR(ERR_INTERNAL_VAL);
	return;
    }

    port = 0;
    val = val_find_child(agent_cb->connect_valset,
			 YANGCLI_MOD, 
			 YANGCLI_PORT);
    if (val && val->res == NO_ERR) {
	port = VAL_UINT16(val);
    }

    log_info("\nyangcli: Starting NETCONF session for %s on %s",
	     username, 
             agent);

    agent_cb->state = MGR_IO_ST_CONNECT;

    /* this function call will cause us to block while the
     * protocol layer connect messages are processed
     */
    res = mgr_ses_new_session(username, 
			      password, 
			      agent, 
			      port, 
			      &agent_cb->mysid);
    if (res == NO_ERR) {
	agent_cb->state = MGR_IO_ST_CONN_START;
	log_debug("\nyangcli: Start session %d OK for agent '%s'", 
		  agent_cb->mysid, 
                  agent_cb->name);
    } else {
	log_info("\nyangcli: Start session failed for user %s on "
		 "%s (%s)\n", 
                 username, 
                 agent, 
                 get_error_string(res));
	agent_cb->state = MGR_IO_ST_IDLE;
    }
    
} /* create_session */


/********************************************************************
* FUNCTION send_copy_config_to_agent
* 
* Send a <copy-config> operation to the agent to support
* the save operation
*
* INPUTS:
*    agent_cb == agent control block to use
*
* OUTPUTS:
*    state may be changed or other action taken
*    config_content is consumed -- freed or transfered
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    send_copy_config_to_agent (agent_cb_t *agent_cb)
{
    const obj_template_t  *rpc, *input, *child;
    mgr_rpc_req_t         *req;
    val_value_t           *reqdata, *parm, *target, *source;
    ses_cb_t              *scb;
    status_t               res;

    req = NULL;
    reqdata = NULL;
    res = NO_ERR;
    rpc = NULL;

    /* get the <copy-config> template */
    rpc = ncx_find_object(get_netconf_mod(), 
			  NCX_EL_COPY_CONFIG);
    if (!rpc) {
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    /* get the 'input' section container */
    input = obj_find_child(rpc, NULL, YANG_K_INPUT);
    if (!input) {
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    /* construct a method + parameter tree */
    reqdata = xml_val_new_struct(obj_get_name(rpc), 
				 obj_get_nsid(rpc));
    if (!reqdata) {
	log_error("\nError allocating a new RPC request");
	return ERR_INTERNAL_MEM;
    }

    /* set the edit-config/input/target node to the default_target */
    child = obj_find_child(input, NC_MODULE, NCX_EL_TARGET);
    parm = val_new_value();
    if (!parm) {
	val_free_value(reqdata);
	return ERR_INTERNAL_MEM;
    }
    val_init_from_template(parm, child);
    val_add_child(parm, reqdata);

    target = xml_val_new_flag((const xmlChar *)"startup",
			      obj_get_nsid(child));
    if (!target) {
	val_free_value(reqdata);
	return ERR_INTERNAL_MEM;
    }
    val_add_child(target, parm);

    /* set the edit-config/input/default-operation node to 'none' */
    child = obj_find_child(input, NC_MODULE, NCX_EL_SOURCE);
    parm = val_new_value();
    if (!parm) {
	val_free_value(reqdata);
	return ERR_INTERNAL_MEM;
    }
    val_init_from_template(parm, child);
    val_add_child(parm, reqdata);

    source = xml_val_new_flag((const xmlChar *)"running",
			      obj_get_nsid(child));
    if (!source) {
	val_free_value(reqdata);
	return ERR_INTERNAL_MEM;
    }
    val_add_child(source, parm);

    /* allocate an RPC request and send it */
    scb = mgr_ses_get_scb(agent_cb->mysid);
    if (!scb) {
	res = SET_ERROR(ERR_INTERNAL_PTR);
    } else {
	req = mgr_rpc_new_request(scb);
	if (!req) {
	    res = ERR_INTERNAL_MEM;
	    log_error("\nError allocating a new RPC request");
	} else {
	    req->data = reqdata;
	    req->rpc = rpc;
	    req->timeout = agent_cb->timeout;
	}
    }
	
    if (res == NO_ERR) {
	if (LOGDEBUG2) {
	    log_debug2("\nabout to send RPC request with reqdata:");
	    val_dump_value_ex(reqdata, 
                              NCX_DEF_INDENT,
                              agent_cb->display_mode);
	}

	/* the request will be stored if this returns NO_ERR */
	res = mgr_rpc_send_request(scb, req, yangcli_reply_handler);
    }

    if (res != NO_ERR) {
	if (req) {
	    mgr_rpc_free_request(req);
	} else if (reqdata) {
	    val_free_value(reqdata);
	}
    } else {
	agent_cb->state = MGR_IO_ST_CONN_RPYWAIT;
    }

    return res;

} /* send_copy_config_to_agent */


/********************************************************************
 * FUNCTION do_save
 * 
 * INPUTS:
 *    agent_cb == agent control block to use
 *
 * OUTPUTS:
 *   copy-config and/or commit operation will be sent to agent
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_save (agent_cb_t *agent_cb)
{
    const ses_cb_t   *scb;
    const mgr_scb_t  *mscb;
    xmlChar          *line;
    status_t          res;

    res = NO_ERR;

    /* get the session info */
    scb = mgr_ses_get_scb(agent_cb->mysid);
    if (!scb) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }
    mscb = (const mgr_scb_t *)scb->mgrcb;

    log_info("\nSaving configuration to non-volative storage");

    /* determine which commands to send */
    switch (mscb->targtyp) {
    case NCX_AGT_TARG_NONE:
	log_stdout("\nWarning: No writable targets supported on this agent");
	break;
    case NCX_AGT_TARG_CANDIDATE:
    case NCX_AGT_TARG_CAND_RUNNING:
	line = xml_strdup(NCX_EL_COMMIT);
	if (line) {
	    res = conn_command(agent_cb, line);
#ifdef NOT_YET
	    if (mscb->starttyp == NCX_AGT_START_DISTINCT) {
		log_stdout(" + copy-config <running> <startup>");
	    }
#endif
	    m__free(line);
	} else {
	    log_stdout("\nError: Malloc failed");
	}
	break;
    case NCX_AGT_TARG_RUNNING:
	if (mscb->starttyp == NCX_AGT_START_DISTINCT) {
	    res = send_copy_config_to_agent(agent_cb);
	    if (res != NO_ERR) {
		log_stdout("\nError: send copy-config failed (%s)",
			   get_error_string(res));
	    }
	} else {
	    log_stdout("\nWarning: No distinct save operation needed "
		       "for this agent");
	}
	break;
    case NCX_AGT_TARG_LOCAL:
	log_stdout("Error: Local URL target not supported");
	break;
    case NCX_AGT_TARG_REMOTE:
	log_stdout("Error: Local URL target not supported");
	break;
    default:
	log_stdout("Error: Internal target not set");
	res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    return res;

}  /* do_save */


/********************************************************************
 * FUNCTION do_mgrload (local RPC)
 * 
 * mgrload module=mod-name
 *
 * Get the module parameter and load the specified NCX module
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   specified module is loaded into the definition registry, if NO_ERR
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_mgrload (agent_cb_t *agent_cb,
		const obj_template_t *rpc,
		const xmlChar *line,
		uint32  len)
{
    val_value_t     *valset, *modval, *revval;
    ncx_module_t    *mod;
    modptr_t        *modptr;
    dlq_hdr_t       *mgrloadQ;
    logfn_t          logfn;
    status_t         res;

    modval = NULL;
    revval = NULL;
    res = NO_ERR;

    if (interactive_mode()) {
	logfn = log_stdout;
    } else {
	logfn = log_write;
    }
	
    valset = get_valset(agent_cb, 
			rpc, 
			&line[len], 
			&res);

    /* get the module name */
    if (res == NO_ERR) {
	modval = val_find_child(valset, 
                                YANGCLI_MOD, 
                                NCX_EL_MODULE);
	if (!modval) {
	    res = ERR_NCX_DEF_NOT_FOUND;
	} else if (modval->res != NO_ERR) {
	    res = modval->res;
	}
    }

    /* get the module revision */
    if (res == NO_ERR) {
	revval = val_find_child(valset, 
                                YANGCLI_MOD, 
                                NCX_EL_REVISION);
    }


    /* check if any version of the module is loaded already */
    if (res == NO_ERR) {
	mod = ncx_find_module(VAL_STR(modval), NULL);
	if (mod) {
	    if (mod->version) {
		(*logfn)("\nModule '%s' revision '%s' already loaded",
			 mod->name, 
			 mod->version);
	    } else {
		(*logfn)("\nModule '%s' already loaded",
			 mod->name);
	    }
	    if (valset) {
		val_free_value(valset);
	    }
	    return res;
	}
    }

    /* load the module */
    if (res == NO_ERR) {
	mod = NULL;
	res = ncxmod_load_module(VAL_STR(modval), 
                                 (revval) ? VAL_STR(revval) : NULL, 
                                 &mod);
	if (res == NO_ERR) {
	    /*** TBD: prompt user for features to enable/disable ***/
	    modptr = new_modptr(mod, NULL, NULL);
	    if (!modptr) {
		res = ERR_INTERNAL_MEM;
	    } else {
		mgrloadQ = get_mgrloadQ();
		dlq_enque(modptr, mgrloadQ);
	    }
	}
    }

    /* print the result to stdout */
    if (res == NO_ERR) {
        if (revval) {
            (*logfn)("\nLoad module '%s' revision '%s' OK", 
                     VAL_STR(modval),
                     VAL_STR(revval));
        } else {
            (*logfn)("\nLoad module '%s' OK", VAL_STR(modval));
        }
    } else {
        (*logfn)("\nError: Load module failed (%s)",
                 get_error_string(res));
    }
    (*logfn)("\n");

    if (valset) {
	val_free_value(valset);
    }

    return res;

}  /* do_mgrload */


/********************************************************************
 * FUNCTION show_user_var
 * 
 * generate the output for a global or local variable
 *
 * INPUTS:
 *   agent_cb == agent control block to use
 *   varname == variable name to show
 *   vartype == type of user variable
 *   val == value associated with this variable
 *   mode == help mode in use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    show_user_var (agent_cb_t *agent_cb,
                   const xmlChar *varname,
		   var_type_t vartype,
		   const val_value_t *val,
		   help_mode_t mode)
{
    xmlChar      *objbuff;
    logfn_t       logfn;
    boolean       imode;
    int32         doubleindent;
    status_t      res;

    res = NO_ERR;
    doubleindent = 1;

    imode = interactive_mode();
    if (imode) {
	logfn = log_stdout;
    } else {
	logfn = log_write;
    }

    switch (vartype) {
    case VAR_TYP_GLOBAL:
    case VAR_TYP_LOCAL:
    case VAR_TYP_SESSION:
	if (xml_strcmp(varname, val->name)) {
	    doubleindent = 2;

	    (*logfn)("\n   %s ", varname);

	    if (val->obj && obj_is_data_db(val->obj)) {
		res = obj_gen_object_id(val->obj, &objbuff);
		if (res != NO_ERR) {
		    (*logfn)("[no object id]");
		} else {
		    (*logfn)("[%s]", objbuff);
		    m__free(objbuff);
		}
	    }
	}
	break;
    default:
	;
    }

    if (!typ_is_simple(val->btyp) && mode == HELP_MODE_BRIEF) {
	if (doubleindent == 1) {
	    (*logfn)("\n   %s (%s)",
		     varname,
		     tk_get_btype_sym(val->btyp));
	} else {
	    (*logfn)("\n      (%s)", 
		     tk_get_btype_sym(val->btyp));
	}
    } else {
	if (imode) {
	    val_stdout_value_ex(val, 
                                NCX_DEF_INDENT * doubleindent,
                                agent_cb->display_mode);
	} else {
	    val_dump_value_ex(val, 
                              NCX_DEF_INDENT * doubleindent,
                              agent_cb->display_mode);
	}
    }

    return res;

}  /* show_user_var */


/********************************************************************
 * FUNCTION do_show_vars (sub-mode of local RPC)
 * 
 * show brief info for all user variables
 *
 * INPUTS:
 *  agent_cb == agent control block to use
 *  mode == help mode requested
 *  shortmode == TRUE if printing just global or local variables
 *               FALSE to print everything
 *  isglobal == TRUE if print just globals
 *              FALSE to print just locals
 *              Ignored unless shortmode==TRUE
 * isany == TRUE to choose global or local
 *          FALSE to use 'isglobal' valuse only
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_show_vars (agent_cb_t *agent_cb,
                  help_mode_t mode,
		  boolean shortmode,
		  boolean isglobal,
		  boolean isany)

{
    ncx_var_t    *var;
    val_value_t  *mgrset;
    dlq_hdr_t    *que;
    logfn_t       logfn;
    boolean       first, imode;

    imode = interactive_mode();
    if (imode) {
	logfn = log_stdout;
    } else {
	logfn = log_write;
    }

    mgrset = get_mgr_cli_valset();

    if (mode > HELP_MODE_BRIEF && !shortmode) {
	/* CLI Parameters */
	if (mgrset && val_child_cnt(mgrset)) {
	    (*logfn)("\nCLI Variables\n");
	    if (imode) {
		val_stdout_value_ex(mgrset, 
                                    NCX_DEF_INDENT,
                                    agent_cb->display_mode);
	    } else {
		val_dump_value_ex(mgrset, 
                                  NCX_DEF_INDENT,
                                  agent_cb->display_mode);
	    }
	    (*logfn)("\n");
	} else {
	    (*logfn)("\nNo CLI variables\n");
	}
    }

    /* System Script Variables */
    if (!shortmode) {
	que = runstack_get_que(ISGLOBAL);
	first = TRUE;
	for (var = (ncx_var_t *)dlq_firstEntry(que);
	     var != NULL;
	     var = (ncx_var_t *)dlq_nextEntry(var)) {

	    if (var->vartype != VAR_TYP_SYSTEM) {
		continue;
	    }

	    if (first) {
		(*logfn)("\nRead-only system variables\n");
		first = FALSE;
	    }
	    show_user_var(agent_cb,
                          var->name, 
			  var->vartype,
			  var->val,
			  mode);
	}
	if (first) {
	    (*logfn)("\nNo read-only system variables\n");
	}
	(*logfn)("\n");
    }

    /* System Config Variables */
    if (!shortmode) {
	que = runstack_get_que(ISGLOBAL);
	first = TRUE;
	for (var = (ncx_var_t *)dlq_firstEntry(que);
	     var != NULL;
	     var = (ncx_var_t *)dlq_nextEntry(var)) {

	    if (var->vartype != VAR_TYP_CONFIG) {
		continue;
	    }

	    if (first) {
		(*logfn)("\nRead-write system variables\n");
		first = FALSE;
	    }
	    show_user_var(agent_cb,
                          var->name,
			  var->vartype,
			  var->val,
			  mode);
	}
	if (first) {
	    (*logfn)("\nNo system config variables\n");
	}
	(*logfn)("\n");
    }

    /* Global Script Variables */
    if (!shortmode || isglobal) {
	que = runstack_get_que(ISGLOBAL);
	first = TRUE;
	for (var = (ncx_var_t *)dlq_firstEntry(que);
	     var != NULL;
	     var = (ncx_var_t *)dlq_nextEntry(var)) {

	    if (var->vartype != VAR_TYP_GLOBAL) {
		continue;
	    }

	    if (first) {
		(*logfn)("\nGlobal variables\n");
		first = FALSE;
	    }
	    show_user_var(agent_cb,
                          var->name,
			  var->vartype,
			  var->val,
			  mode);
	}
	if (first) {
	    (*logfn)("\nNo global variables\n");
	}
	(*logfn)("\n");
    }

    /* Local Script Variables */
    if (!shortmode || !isglobal || isany) {
	que = runstack_get_que(ISLOCAL);
	first = TRUE;
	for (var = (ncx_var_t *)dlq_firstEntry(que);
	     var != NULL;
	     var = (ncx_var_t *)dlq_nextEntry(var)) {
	    if (first) {
		(*logfn)("\nLocal variables\n");
		first = FALSE;
	    }
	    show_user_var(agent_cb,
                          var->name, 
			  var->vartype,
			  var->val,
			  mode);
	}
	if (first) {
	    (*logfn)("\nNo local variables\n");
	}
	(*logfn)("\n");
    }

    return NO_ERR;

} /* do_show_vars */


/********************************************************************
 * FUNCTION do_show_var (sub-mode of local RPC)
 * 
 * show full info for one user var
 *
 * INPUTS:
 *   agent_cb == agent control block to use
 *   name == variable name to find 
 *   isglobal == TRUE if global var, FALSE if local var
 *   isany == TRUE if don't care (global or local)
 *         == FALSE to force local or global with 'isglobal'
 *   mode == help mode requested
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_show_var (agent_cb_t *agent_cb,
                 const xmlChar *name,
		 var_type_t vartype,
		 boolean isany,
		 help_mode_t mode)
{
    const val_value_t *val;
    logfn_t            logfn;
    boolean            imode;

    imode = interactive_mode();
    if (imode) {
	logfn = log_stdout;
    } else {
	logfn = log_write;
    }

    if (isany) {
	/* skipping VAR_TYP_SESSION for now */
	val = var_get_local(name);
	if (val) {
	    vartype = VAR_TYP_LOCAL;
	} else {
	    val = var_get(name, VAR_TYP_GLOBAL);
	    if (val) {
		vartype = VAR_TYP_GLOBAL;
	    } else {
		val = var_get(name, VAR_TYP_CONFIG);
		if (val) {
		    vartype = VAR_TYP_CONFIG;
		} else {
		    val = var_get(name, VAR_TYP_SYSTEM);
		    if (val) {
			vartype = VAR_TYP_SYSTEM;
		    }
		}
	    }
	}
    } else {
	val = var_get(name, vartype);
    }

    if (val) {
	show_user_var(agent_cb, name, vartype, val, mode);
	(*logfn)("\n");
    } else {
	(*logfn)("\nVariable '%s' not found", name);
	return ERR_NCX_DEF_NOT_FOUND;
    }

    return NO_ERR;

} /* do_show_var */


/********************************************************************
 * FUNCTION do_show_module (sub-mode of local RPC)
 * 
 * show module=mod-name
 *
 * INPUTS:
 *    mod == module to show
 *    mode == requested help mode
 * 
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_show_module (const ncx_module_t *mod,
		    help_mode_t mode)
{
    help_data_module(mod, mode);
    return NO_ERR;

} /* do_show_module */


/********************************************************************
 * FUNCTION do_show_one_module (sub-mode of show modules RPC)
 * 
 * for 1 of N: show modules
 *
 * INPUTS:
 *    mod == module to show
 *    mode == requested help mode
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_show_one_module (ncx_module_t *mod,
			help_mode_t mode)
{
    boolean        anyout, imode;

    imode = interactive_mode();
    anyout = FALSE;

    if (mode == HELP_MODE_BRIEF) {
	if (imode) {
	    log_stdout("\n  %s", mod->name); 
	} else {
	    log_write("\n  %s", mod->name); 
	}
    } else if (mode == HELP_MODE_NORMAL) {
	if (imode) {
	    if (mod->version) {
		log_stdout("\n  %s:%s/%s", 
			   ncx_get_mod_xmlprefix(mod), 
			   mod->name, 
			   mod->version);
	    } else {
		log_stdout("\n  %s:%s", 
			   ncx_get_mod_xmlprefix(mod), 
			   mod->name);
	    }
	} else {
	    if (mod->version) {
		log_write("\n  %s/%s", 
			  mod->name, 
			  mod->version);
	    } else {
		log_write("\n  %s", 
			  mod->name);
	    }
	}
    } else {
	help_data_module(mod, HELP_MODE_BRIEF);
    }

    return NO_ERR;

}  /* do_show_one_module */


/********************************************************************
 * FUNCTION do_show_modules (sub-mode of local RPC)
 * 
 * show modules
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    mode == requested help mode
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_show_modules (agent_cb_t *agent_cb,
		     help_mode_t mode)
{
    ncx_module_t  *mod;
    modptr_t      *modptr;
    boolean        anyout, imode;
    status_t       res;

    imode = interactive_mode();
    anyout = FALSE;
    res = NO_ERR;

    if (use_agentcb(agent_cb)) {
	for (modptr = (modptr_t *)dlq_firstEntry(&agent_cb->modptrQ);
	     modptr != NULL && res == NO_ERR;
	     modptr = (modptr_t *)dlq_nextEntry(modptr)) {

	    res = do_show_one_module(modptr->mod, mode);
	    anyout = TRUE;
	}
	for (modptr = (modptr_t *)dlq_firstEntry(get_mgrloadQ());
	     modptr != NULL && res == NO_ERR;
	     modptr = (modptr_t *)dlq_nextEntry(modptr)) {

	    res = do_show_one_module(modptr->mod, mode);
	    anyout = TRUE;
	}
    } else {
	mod = ncx_get_first_module();
	while (mod && res == NO_ERR) {
	    res = do_show_one_module(mod, mode);
	    anyout = TRUE;
	    mod = ncx_get_next_module(mod);
	}
    }

    if (anyout) {
	if (imode) {
	    log_stdout("\n");
	} else {
	    log_write("\n");
	}
    } else {
	if (imode) {
	    log_stdout("\nyangcli: no modules loaded\n");
	} else {
	    log_error("\nyangcli: no modules loaded\n");
	}
    }

    return res;

} /* do_show_modules */


/********************************************************************
 * FUNCTION do_show_one_object (sub-mode of show objects local RPC)
 * 
 * show objects: 1 of N
 *
 * INPUTS:
 *    obj == object to show
 *    mode == requested help mode
 *    anyout == address of return anyout status
 *
 * OUTPUTS:
 *    *anyout set to TRUE only if any suitable objects found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_show_one_object (const obj_template_t *obj,
			help_mode_t mode,
			boolean *anyout)
{
    boolean               imode;

    imode = interactive_mode();

    if (obj_is_data_db(obj) && 
	obj_has_name(obj) &&
	!obj_is_hidden(obj) && !obj_is_abstract(obj)) {

	if (mode == HELP_MODE_BRIEF) {
	    if (imode) {
		log_stdout("\n%s:%s",
			   obj_get_mod_name(obj),
			   obj_get_name(obj));
	    } else {
		log_write("\n%s:%s",
			  obj_get_mod_name(obj),
			  obj_get_name(obj));
	    }
	} else {
	    obj_dump_template(obj, mode-1, 0, 0); 
	}
	*anyout = TRUE;
    }

    return NO_ERR;

} /* do_show_one_object */


/********************************************************************
 * FUNCTION do_show_objects (sub-mode of local RPC)
 * 
 * show objects
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    mode == requested help mode
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
    do_show_objects (agent_cb_t *agent_cb,
		     help_mode_t mode)
{
    const ncx_module_t   *mod;
    const obj_template_t *obj;
    const modptr_t       *modptr;
    boolean               anyout, imode;
    status_t              res;

    imode = interactive_mode();
    anyout = FALSE;
    res = NO_ERR;

    if (use_agentcb(agent_cb)) {
	for (modptr = (const modptr_t *)
		 dlq_firstEntry(&agent_cb->modptrQ);
	     modptr != NULL;
	     modptr = (const modptr_t *)dlq_nextEntry(modptr)) {

	    for (obj = ncx_get_first_object(modptr->mod);
		 obj != NULL && res == NO_ERR;
		 obj = ncx_get_next_object(modptr->mod, obj)) {

		res = do_show_one_object(obj, mode, &anyout);
	    }
	}

	for (modptr = (const modptr_t *)
		 dlq_firstEntry(get_mgrloadQ());
	     modptr != NULL;
	     modptr = (const modptr_t *)dlq_nextEntry(modptr)) {

	    for (obj = ncx_get_first_object(modptr->mod);
		 obj != NULL && res == NO_ERR;
		 obj = ncx_get_next_object(modptr->mod, obj)) {

		res = do_show_one_object(obj, mode, &anyout);
	    }
	}
    } else {
	mod = ncx_get_first_module();
	while (mod) {
	    for (obj = ncx_get_first_object(mod);
		 obj != NULL && res == NO_ERR;
		 obj = ncx_get_next_object(mod, obj)) {

		res = do_show_one_object(obj, mode, &anyout);
	    }
	    mod = (const ncx_module_t *)ncx_get_next_module(mod);
	}
    }
    if (anyout) {
	if (imode) {
	    log_stdout("\n");
	} else {
	    log_write("\n");
	}
    }

    return res;

} /* do_show_objects */


/********************************************************************
 * FUNCTION do_show (local RPC)
 * 
 * show module=mod-name
 *      modules
 *      def=def-nmae
 *
 * Get the specified parameter and show the internal info,
 * based on the parameter
 *
 * INPUTS:
 * agent_cb == agent control block to use
 *    rpc == RPC method for the show command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_show (agent_cb_t *agent_cb,
	     const obj_template_t *rpc,
	     const xmlChar *line,
	     uint32  len)
{
    val_value_t        *valset, *parm;
    const ncx_module_t *mod;
    status_t            res;
    boolean             imode, done;
    help_mode_t         mode;
    xmlChar             versionbuffer[NCX_VERSION_BUFFSIZE];

    res = NO_ERR;
    imode = interactive_mode();
    valset = get_valset(agent_cb, rpc, &line[len], &res);

    if (valset && res == NO_ERR) {
	mode = HELP_MODE_NORMAL;

	/* check if the 'brief' flag is set first */
	parm = val_find_child(valset, 
			      YANGCLI_MOD, 
			      YANGCLI_BRIEF);
	if (parm && parm->res == NO_ERR) {
	    mode = HELP_MODE_BRIEF;
	} else {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD, 
				  YANGCLI_FULL);
	    if (parm && parm->res == NO_ERR) {
		mode = HELP_MODE_FULL;
	    }
	}
	    
	/* get the 1 of N 'showtype' choice */
	done = FALSE;
	parm = val_find_child(valset, 
			      YANGCLI_MOD,
			      YANGCLI_LOCAL);
	if (parm) {
	    res = do_show_var(agent_cb,
                              VAL_STR(parm), 
			      VAR_TYP_LOCAL, 
			      FALSE, 
			      mode);
	    done = TRUE;
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD,
				  YANGCLI_LOCALS);
	    if (parm) {
		res = do_show_vars(agent_cb,
                                   mode, 
                                   TRUE, 
                                   FALSE, 
                                   FALSE);
		done = TRUE;
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD,
				  YANGCLI_OBJECTS);
	    if (parm) {
		res = do_show_objects(agent_cb, mode);
		done = TRUE;
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD,
				  YANGCLI_GLOBAL);
	    if (parm) {
		res = do_show_var(agent_cb,
                                  VAL_STR(parm), 
                                  VAR_TYP_GLOBAL, 
                                  FALSE, 
                                  mode);
		done = TRUE;
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD,
				  YANGCLI_GLOBALS);
	    if (parm) {
		res = do_show_vars(agent_cb,
                                   mode, 
                                   TRUE, 
                                   TRUE, 
                                   FALSE);
		done = TRUE;
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD,
				  YANGCLI_VAR);
	    if (parm) {
		res = do_show_var(agent_cb,
                                  VAL_STR(parm), 
                                  VAR_TYP_NONE, 
                                  TRUE, 
                                  mode);
		done = TRUE;
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD,
				  YANGCLI_VARS);
	    if (parm) {
		res = do_show_vars(agent_cb,
                                   mode, 
                                   FALSE, 
                                   FALSE, 
                                   TRUE);
		done = TRUE;
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD,
				  YANGCLI_MODULE);
	    if (parm) {
		mod = find_module(agent_cb, VAL_STR(parm));
		if (mod) {
		    res = do_show_module(mod, mode);
		} else {
		    if (imode) {
			log_stdout("\nyangcli: module (%s) not loaded",
				   VAL_STR(parm));
		    } else {
			log_error("\nyangcli: module (%s) not loaded",
				  VAL_STR(parm));
		    }
		}
		done = TRUE;
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD,
				  YANGCLI_MODULES);
	    if (parm) {
		res = do_show_modules(agent_cb, mode);
		done = TRUE;
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD,
				  NCX_EL_VERSION);
	    if (parm) {
                res = ncx_get_version(versionbuffer, 
                                      NCX_VERSION_BUFFSIZE);
                if (res == NO_ERR) {
                    if (imode) {
                        log_stdout("\nyangcli version %s\n", 
                                   versionbuffer);
                    } else {
                        log_write("\nyangcli version %s\n", 
                                  versionbuffer);
                    }
                }
		done = TRUE;
	    }
	}
    }

    if (valset) {
	val_free_value(valset);
    }

    return res;

}  /* do_show */


/********************************************************************
 * FUNCTION do_list_one_oid (sub-mode of list oids RPC)
 * 
 * list oids: 1 of N
 *
 * INPUTS:
 *    obj == the object to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_list_one_oid (const obj_template_t *obj)
{
    xmlChar      *buffer;
    boolean       imode;
    status_t      res;

    res = NO_ERR;

    if (obj_is_data_db(obj) && 
	obj_has_name(obj) &&
	!obj_is_hidden(obj) && 
	!obj_is_abstract(obj)) {

	imode = interactive_mode();
	buffer = NULL;
	res = obj_gen_object_id(obj, &buffer);
	if (res != NO_ERR) {
	    log_error("\nError: list OID failed (%s)",
		      get_error_string(res));
	} else {
	    if (imode) {
		log_stdout("\n   %s", buffer);
	    } else {
		log_write("\n   %s", buffer);
	    }
	}
	if (buffer) {
	    m__free(buffer);
	}
    }

    return res;

} /* do_list_one_oid */


/********************************************************************
 * FUNCTION do_list_oid (sub-mode of local RPC)
 * 
 * list oids
 *
 * INPUTS:
 *    obj == object to use
 *    nestlevel to stop at
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_list_oid (const obj_template_t *obj,
		 uint32 level)
{
    const obj_template_t  *chobj;
    status_t               res;

    res = NO_ERR;

    if (obj_get_level(obj) <= level) {
	res = do_list_one_oid(obj);
	for (chobj = obj_first_child(obj);
	     chobj != NULL && res != NO_ERR;
	     chobj = obj_next_child(chobj)) {
	    res = do_list_oid(chobj, level);
	}
    }

    return res;

} /* do_list_oid */


/********************************************************************
 * FUNCTION do_list_oids (sub-mode of local RPC)
 * 
 * list oids
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    mod == the 1 module to use
 *           NULL to use all available modules instead
 *    mode == requested help mode
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_list_oids (agent_cb_t *agent_cb,
		  const ncx_module_t *mod,
		  help_mode_t mode)
{
    const modptr_t        *modptr;
    const obj_template_t  *obj;
    boolean                imode;
    uint32                 level;
    status_t               res;

    res = NO_ERR;

    switch (mode) {
    case HELP_MODE_NONE:
	return res;
    case HELP_MODE_BRIEF:
	level = 1;
	break;
    case HELP_MODE_NORMAL:
	level = 5;
	break;
    case HELP_MODE_FULL:
	level = 999;
	break;
    default:
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    imode = interactive_mode();

    if (mod) {
	obj = ncx_get_first_object(mod);
	while (obj && res == NO_ERR) {
	    res = do_list_oid(obj, level);
	    obj = ncx_get_next_object(mod, obj);
	}
    } else if (use_agentcb(agent_cb)) {
	for (modptr = (const modptr_t *)
		 dlq_firstEntry(&agent_cb->modptrQ);
	     modptr != NULL;
	     modptr = (const modptr_t *)dlq_nextEntry(modptr)) {

	    obj = ncx_get_first_object(modptr->mod);
	    while (obj && res == NO_ERR) {
		res = do_list_oid(obj, level);
		obj = ncx_get_next_object(modptr->mod, obj);
	    }
	}
    } else {
	return res;
    }
    if (imode) {
	log_stdout("\n");
    } else {
	log_write("\n");
    }

    return res;

} /* do_list_oids */


/********************************************************************
 * FUNCTION do_list_one_command (sub-mode of list command RPC)
 * 
 * list commands: 1 of N
 *
 * INPUTS:
 *    obj == object template for the RPC command to use
 *    mode == requested help mode
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
    do_list_one_command (const obj_template_t *obj,
			 help_mode_t mode)
{
    if (interactive_mode()) {
	if (mode == HELP_MODE_BRIEF) {
	    log_stdout("\n   %s", obj_get_name(obj));
	} else if (mode == HELP_MODE_NORMAL) {
	    log_stdout("\n   %s:%s",
		       obj_get_mod_xmlprefix(obj),
		       obj_get_name(obj));
	} else {
	    log_stdout("\n   %s:%s",
		       obj_get_mod_name(obj),
		       obj_get_name(obj));
	}
    } else {
	if (mode == HELP_MODE_BRIEF) {
	    log_write("\n   %s", obj_get_name(obj));
	} else if (mode == HELP_MODE_NORMAL) {
	    log_write("\n   %s:%s",
		      obj_get_mod_xmlprefix(obj),
		      obj_get_name(obj));
	} else {
	    log_write("\n   %s:%s",
		      obj_get_mod_name(obj),
		      obj_get_name(obj));
	}
    }

    return NO_ERR;

} /* do_list_one_command */


/********************************************************************
 * FUNCTION do_list_objects (sub-mode of local RPC)
 * 
 * list objects
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    mod == the 1 module to use
 *           NULL to use all available modules instead
 *    mode == requested help mode
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_list_objects (agent_cb_t *agent_cb,
		     const ncx_module_t *mod,
		     help_mode_t mode)
{
    const modptr_t        *modptr;
    const obj_template_t  *obj;
    logfn_t                logfn;
    boolean                anyout;
    status_t               res;

    res = NO_ERR;
    anyout = FALSE;

    if (interactive_mode()) {
	logfn = log_stdout;
    } else {
	logfn = log_write;
    }

    if (mod) {
	obj = ncx_get_first_object(mod);
	while (obj && res == NO_ERR) {
	    if (obj_is_data_db(obj) && 
		obj_has_name(obj) &&
		!obj_is_hidden(obj) && 
		!obj_is_abstract(obj)) {
		anyout = TRUE;
		res = do_list_one_command(obj, mode);
	    }
	    obj = ncx_get_next_object(mod, obj);
	}
    } else {
	if (use_agentcb(agent_cb)) {
	    for (modptr = (const modptr_t *)
		     dlq_firstEntry(&agent_cb->modptrQ);
		 modptr != NULL;
		 modptr = (const modptr_t *)dlq_nextEntry(modptr)) {

		obj = ncx_get_first_object(modptr->mod);
		while (obj && res == NO_ERR) {
		    if (obj_is_data_db(obj) && 
			obj_has_name(obj) &&
			!obj_is_hidden(obj) && 
			!obj_is_abstract(obj)) {
			anyout = TRUE;			
			res = do_list_one_command(obj, mode);
		    }
		    obj = ncx_get_next_object(modptr->mod, obj);
		}
	    }
	}

	for (modptr = (const modptr_t *)
		 dlq_firstEntry(get_mgrloadQ());
	     modptr != NULL && res == NO_ERR;
	     modptr = (const modptr_t *)dlq_nextEntry(modptr)) {

	    obj = ncx_get_first_object(modptr->mod);
	    while (obj && res == NO_ERR) {
		if (obj_is_data_db(obj) && 
		    obj_has_name(obj) &&
		    !obj_is_hidden(obj) && 
		    !obj_is_abstract(obj)) {
		    anyout = TRUE;		    
		    res = do_list_one_command(obj, mode);
		}
		obj = ncx_get_next_object(modptr->mod, obj);
	    }
	}
    }

    if (!anyout) {
	(*logfn)("\nNo objects found to list");
    }

    (*logfn)("\n");

    return res;

} /* do_list_objects */


/********************************************************************
 * FUNCTION do_list_commands (sub-mode of local RPC)
 * 
 * list commands
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    mod == the 1 module to use
 *           NULL to use all available modules instead
 *    mode == requested help mode
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_list_commands (agent_cb_t *agent_cb,
		      const ncx_module_t *mod,
		      help_mode_t mode)
{
    const modptr_t        *modptr;
    const obj_template_t  *obj;
    logfn_t                logfn;
    boolean                imode, anyout;
    status_t               res;

    res = NO_ERR;
    imode = interactive_mode();
    if (imode) {
	logfn = log_stdout;
    } else {
	logfn = log_write;
    }

    if (mod) {
	anyout = FALSE;
	obj = ncx_get_first_object(mod);
	while (obj && res == NO_ERR) {
	    if (obj_is_rpc(obj)) {
		res = do_list_one_command(obj, mode);
		anyout = TRUE;
	    }
	    obj = ncx_get_next_object(mod, obj);
	}
	if (!anyout) {
	    (*logfn)("\nNo commands found in module '%s'",
		     mod->name);
	}
    } else {
	if (use_agentcb(agent_cb)) {
	    (*logfn)("\nAgent Commands:");
	
	    for (modptr = (const modptr_t *)
		     dlq_firstEntry(&agent_cb->modptrQ);
		 modptr != NULL && res == NO_ERR;
		 modptr = (const modptr_t *)dlq_nextEntry(modptr)) {

		obj = ncx_get_first_object(modptr->mod);
		while (obj) {
		    if (obj_is_rpc(obj)) {
			res = do_list_one_command(obj, mode);
		    }
		    obj = ncx_get_next_object(modptr->mod, obj);
		}
	    }
	}

	(*logfn)("\n\nLocal Commands:");

	obj = ncx_get_first_object(get_yangcli_mod());
	while (obj && res == NO_ERR) {
	    if (obj_is_rpc(obj)) {
		if (use_agentcb(agent_cb)) {
		    /* list a local command */
		    res = do_list_one_command(obj, mode);
		} else {
		    /* session not active so filter out
		     * all the commands except top command
		     */
		    if (is_top_command(obj_get_name(obj))) {
			res = do_list_one_command(obj, mode);
		    }
		}
	    }
	    obj = ncx_get_next_object(get_yangcli_mod(), obj);
	}
    }

    (*logfn)("\n");

    return res;

} /* do_list_commands */


/********************************************************************
 * FUNCTION do_list (local RPC)
 * 
 * list objects   [module=mod-name]
 *      ids
 *      commands
 *
 * List the specified information based on the parameters
 *
 * INPUTS:
 * agent_cb == agent control block to use
 *    rpc == RPC method for the show command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_list (agent_cb_t *agent_cb,
	     const obj_template_t *rpc,
	     const xmlChar *line,
	     uint32  len)
{
    val_value_t        *valset, *parm;
    const ncx_module_t *mod;
    status_t            res;
    boolean             imode, done;
    help_mode_t         mode;

    mod = NULL;
    done = FALSE;
    res = NO_ERR;
    imode = interactive_mode();

    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (valset && res == NO_ERR) {
	mode = HELP_MODE_NORMAL;

	/* check if the 'brief' flag is set first */
	parm = val_find_child(valset, 
			      YANGCLI_MOD, 
			      YANGCLI_BRIEF);
	if (parm && parm->res == NO_ERR) {
	    mode = HELP_MODE_BRIEF;
	} else {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD, 
				  YANGCLI_FULL);
	    if (parm && parm->res == NO_ERR) {
		mode = HELP_MODE_FULL;
	    }
	}

	parm = val_find_child(valset, 
			      YANGCLI_MOD, 
			      YANGCLI_MODULE);
	if (parm && parm->res == NO_ERR) {
	    mod = find_module(agent_cb, VAL_STR(parm));
	    if (!mod) {
		if (imode) {
		    log_stdout("\nError: no module found named '%s'",
			       VAL_STR(parm)); 
		} else {
		    log_write("\nError: no module found named '%s'",
			      VAL_STR(parm)); 
		}
		done = TRUE;
	    }
	}

	/* find the 1 of N choice */
	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD, 
				  YANGCLI_COMMANDS);
	    if (parm) {
		/* do list commands */
		res = do_list_commands(agent_cb, mod, mode);
		done = TRUE;
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD, 
				  YANGCLI_OBJECTS);
	    if (parm) {
		/* do list objects */
		res = do_list_objects(agent_cb, mod, mode);
		done = TRUE;
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD, 
				  YANGCLI_OIDS);
	    if (parm) {
		/* do list oids */
		res = do_list_oids(agent_cb, mod, mode);
		done = TRUE;
	    }
	}
    }

    if (valset) {
	val_free_value(valset);
    }

    return res;

}  /* do_list */


/********************************************************************
 * FUNCTION do_help_commands (sub-mode of local RPC)
 * 
 * help commands
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    mode == requested help mode
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_help_commands (agent_cb_t *agent_cb,
		      help_mode_t mode)
{
    const modptr_t        *modptr;
    const obj_template_t  *obj;
    boolean                anyout, imode;

    imode = interactive_mode();
    anyout = FALSE;

    if (use_agentcb(agent_cb)) {
	if (imode) {
	    log_stdout("\nAgent Commands:\n");
	} else {
	    log_write("\nAgent Commands:\n");
	}

	for (modptr = (const modptr_t *)
		 dlq_firstEntry(&agent_cb->modptrQ);
	     modptr != NULL;
	     modptr = (const modptr_t *)dlq_nextEntry(modptr)) {

	    obj = ncx_get_first_object(modptr->mod);
	    while (obj) {
		if (obj_is_rpc(obj) && !obj_is_hidden(obj)) {
		    if (mode == HELP_MODE_BRIEF) {
			obj_dump_template(obj, mode, 1, 0);
		    } else {
			obj_dump_template(obj, mode, 0, 0);
		    }
		    anyout = TRUE;
		}
		obj = ncx_get_next_object(modptr->mod, obj);
	    }
	}
    }

    if (imode) {
	log_stdout("\nLocal Commands:\n");
    } else {
	log_write("\nLocal Commands:\n");
    }

    obj = ncx_get_first_object(get_yangcli_mod());
    while (obj) {
	if (obj_is_rpc(obj) && !obj_is_hidden(obj)) {
	    if (mode == HELP_MODE_BRIEF) {
		obj_dump_template(obj, mode, 1, 0);
	    } else {
		obj_dump_template(obj, mode, 0, 0);
	    }
	    anyout = TRUE;
	}
	obj = ncx_get_next_object(get_yangcli_mod(), obj);
    }

    if (anyout) {
	if (imode) {
	    log_stdout("\n");
	} else {
	    log_write("\n");
	}
    }

    return NO_ERR;

} /* do_help_commands */


/********************************************************************
 * FUNCTION do_help
 * 
 * Print the general yangcli help text to STDOUT
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   log_stdout global help message
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_help (agent_cb_t *agent_cb,
	     const obj_template_t *rpc,
	     const xmlChar *line,
	     uint32  len)
{
    const typ_template_t *typ;
    const obj_template_t *obj;
    val_value_t          *valset, *parm;
    status_t              res;
    help_mode_t           mode;
    boolean               imode, done;
    ncx_node_t            dtyp;
    uint32                dlen;

    res = NO_ERR;
    imode = interactive_mode();
    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (res != NO_ERR) {
	if (valset) {
	    val_free_value(valset);
	}
	return res;
    }

    done = FALSE;
    mode = HELP_MODE_NORMAL;

    /* look for the 'brief' parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_BRIEF);
    if (parm && parm->res == NO_ERR) {
	mode = HELP_MODE_BRIEF;
    } else {
	/* look for the 'full' parameter */
	parm = val_find_child(valset, 
			      YANGCLI_MOD, 
			      YANGCLI_FULL);
	if (parm && parm->res == NO_ERR) {
	    mode = HELP_MODE_FULL;
	}
    }

    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_COMMAND);
    if (parm && parm->res == NO_ERR) {
	dtyp = NCX_NT_OBJ;
	obj = parse_def(agent_cb, &dtyp, VAL_STR(parm), &dlen);
	if (obj && 
            obj->objtype == OBJ_TYP_RPC && 
            !obj_is_hidden(obj)) {
	    help_object(obj, mode);
	} else {
	    res = ERR_NCX_DEF_NOT_FOUND;
	    if (imode) {
		log_stdout("\nyangcli: command (%s) not found",
			   VAL_STR(parm));
	    } else {
		log_error("\nyangcli: command (%s) not found",
			  VAL_STR(parm));
	    }
	}
	val_free_value(valset);
	return res;
    }

    /* look for the specific definition parameters */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_COMMANDS);
    if (parm && parm->res==NO_ERR) {
	res = do_help_commands(agent_cb, mode);
	val_free_value(valset);
	return res;
    }

    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  NCX_EL_TYPE);
    if (parm && parm->res==NO_ERR) {
	dtyp = NCX_NT_TYP;
	typ = parse_def(agent_cb, &dtyp, VAL_STR(parm), &dlen);
	if (typ) {
	    help_type(typ, mode);
	} else {
	    res = ERR_NCX_DEF_NOT_FOUND;
	    if (imode) {
		log_stdout("\nyangcli: type definition (%s) not found",
			   VAL_STR(parm));
	    } else {
		log_error("\nyangcli: type definition (%s) not found",
			  VAL_STR(parm));
	    }
	}
	val_free_value(valset);
	return res;
    }

    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  NCX_EL_OBJECT);
    if (parm && parm->res == NO_ERR) {
	dtyp = NCX_NT_OBJ;
	obj = parse_def(agent_cb, &dtyp, VAL_STR(parm), &dlen);
	if (obj && obj_is_data(obj) && !obj_is_hidden(obj)) {
	    help_object(obj, mode);
	} else {
	    res = ERR_NCX_DEF_NOT_FOUND;
	    if (imode) {
		log_stdout("\nyangcli: object definition (%s) not found",
			   VAL_STR(parm));
	    } else {
		log_error("\nyangcli: object definition (%s) not found",
			  VAL_STR(parm));
	    }
	}
	val_free_value(valset);
	return res;
    }


    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  NCX_EL_NOTIFICATION);
    if (parm && parm->res == NO_ERR) {
	dtyp = NCX_NT_OBJ;
	obj = parse_def(agent_cb, &dtyp, VAL_STR(parm), &dlen);
	if (obj && 
            obj->objtype == OBJ_TYP_NOTIF &&
            !obj_is_hidden(obj)) {
	    help_object(obj, mode);
	} else {
	    res = ERR_NCX_DEF_NOT_FOUND;
	    if (imode) {
		log_stdout("\nyangcli: notification definition (%s) not found",
			   VAL_STR(parm));
	    } else {
		log_error("\nyangcli: notification definition (%s) not found",
			  VAL_STR(parm));
	    }
	}
	val_free_value(valset);
	return res;
    }


    /* no parameters entered except maybe brief or full */
    switch (mode) {
    case HELP_MODE_BRIEF:
    case HELP_MODE_NORMAL:
	log_stdout("\n\nyangcli summary:");
	log_stdout("\n\n  Commands are defined with YANG rpc statements.");
	log_stdout("\n  Use 'help commands' to see current list of commands.");
	log_stdout("\n\n  Global variables are created with 2 dollar signs"
		   "\n  in assignment statements ($$foo = 7).");
	log_stdout("\n  Use 'show globals' to see current list "
		   "of global variables.");
	log_stdout("\n\n  Local variables (within a stack frame) are created"
		   "\n  with 1 dollar sign in assignment"
		   " statements ($foo = $bar).");
	log_stdout("\n  Use 'show locals' to see current list "
		   "of local variables.");
	log_stdout("\n\n  Use 'show vars' to see all program variables.\n");

	if (mode==HELP_MODE_BRIEF) {
	    break;
	}

	obj = ncx_find_object(get_yangcli_mod(), YANGCLI_HELP);
	if (obj && obj->objtype == OBJ_TYP_RPC) {
	    help_object(obj, HELP_MODE_FULL);
	} else {
	    SET_ERROR(ERR_INTERNAL_VAL);
	}
	break;
    case HELP_MODE_FULL:
	help_program_module(YANGCLI_MOD, 
			    YANGCLI_BOOT, 
			    HELP_MODE_FULL);
	break;
    default:
	SET_ERROR(ERR_INTERNAL_VAL);
    }

    val_free_value(valset);

    return res;

}  /* do_help */


/********************************************************************
 * FUNCTION do_start_script (sub-mode of run local RPC)
 * 
 * run script=script-filespec
 *
 * run the specified script
 *
 * INPUTS:
 *   agent_cb == agent control block to use
 *   source == file source
 *   valset == value set for the run script parameters
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_start_script (agent_cb_t *agent_cb,
		     const xmlChar *source,
		     val_value_t *valset)
{
    xmlChar       *str, *fspec;
    FILE          *fp;
    val_value_t   *parm;
    status_t       res;
    int            num;
    xmlChar        buff[4];

    res = NO_ERR;

    /* saerch for the script */
    fspec = ncxmod_find_script_file(source, &res);
    if (!fspec) {
	return res;
    }

    /* open a new runstack frame if the file exists */
    fp = fopen((const char *)fspec, "r");
    if (!fp) {
	m__free(fspec);
	return ERR_NCX_MISSING_FILE;
    } else {
	res = runstack_push(fspec, fp);
	m__free(fspec);
	if (res != NO_ERR) {
	    fclose(fp);
	    return res;
	}
    }
    
    /* setup the numeric script parameters */
    memset(buff, 0x0, 4);
    buff[0] = 'P';

    /* add the P1 through P9 parameters that are present */
    for (num=1; num<=YANGCLI_MAX_RUNPARMS; num++) {
	buff[1] = '0' + num;
	parm = (valset) ? val_find_child(valset, 
					 YANGCLI_MOD, 
					 buff) : NULL;
	if (parm) {
	    /* store P7 named as ASCII 7 */
	    res = var_set_str(buff+1, 1, parm, VAR_TYP_LOCAL);
	    if (res != NO_ERR) {
		runstack_pop();
		return res;
	    }
	}
    }

    /* execute the first command in the script */
    str = runstack_get_cmd(&res);
    if (str && res == NO_ERR) {
	/* execute the line as an RPC command */
	if (is_top(agent_cb->state)) {
	    res = top_command(agent_cb, str);
	} else {
	    res = conn_command(agent_cb, str);
	}
    }

    return res;

}  /* do_start_script */


/********************************************************************
 * FUNCTION do_run (local RPC)
 * 
 * run script=script-filespec
 *
 *
 * Get the specified parameter and run the specified script
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the show command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *    status
 *********************************************************************/
static status_t
    do_run (agent_cb_t *agent_cb,
	    const obj_template_t *rpc,
	    const xmlChar *line,
	    uint32  len)
{
    val_value_t  *valset, *parm;
    status_t      res;

    res = NO_ERR;

    /* get the 'script' parameter */
    valset = get_valset(agent_cb, rpc, &line[len], &res);

    if (valset && res == NO_ERR) {
	/* there is 1 parm */
	parm = val_find_child(valset, 
			      YANGCLI_MOD, 
			      NCX_EL_SCRIPT);
	if (!parm) {
	    res = ERR_NCX_DEF_NOT_FOUND;
	} else if (parm->res != NO_ERR) {
	    res = parm->res;
	} else {
	    /* the parm val is the script filespec */
	    res = do_start_script(agent_cb, VAL_STR(parm), valset);
	    if (res != NO_ERR) {
		log_write("\nError: start script %s failed (%s)",
			  obj_get_name(rpc),
			  get_error_string(res));
	    }
	}
    }

    if (valset) {
	val_free_value(valset);
    }

    return res;

}  /* do_run */


/********************************************************************
 * FUNCTION pwd
 * 
 * Print the current working directory
 *
 * INPUTS:
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   log_stdout global help message
 *
 *********************************************************************/
static void
    pwd (void)
{
    char             *buff;
    boolean           imode;

    imode = interactive_mode();

    buff = m__getMem(YANGCLI_BUFFLEN);
    if (!buff) {
	if (imode) {
	    log_stdout("\nMalloc failure\n");
	} else {
	    log_write("\nMalloc failure\n");
	}
	return;
    }

    if (!getcwd(buff, YANGCLI_BUFFLEN)) {
	if (imode) {
	    log_stdout("\nGet CWD failure\n");
	} else {
	    log_write("\nGet CWD failure\n");
	}
	m__free(buff);
	return;
    }

    if (imode) {
	log_stdout("\nCurrent working directory is %s\n", buff);
    } else {
	log_write("\nCurrent working directory is %s\n", buff);
    }

    m__free(buff);

}  /* pwd */


/********************************************************************
 * FUNCTION do_pwd
 * 
 * Print the current working directory
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   log_stdout global help message
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_pwd (agent_cb_t *agent_cb,
	    const obj_template_t *rpc,
	    const xmlChar *line,
	    uint32  len)
{
    val_value_t      *valset;
    status_t          res;
    boolean           imode;

    res = NO_ERR;
    imode = interactive_mode();
    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (res == NO_ERR || res == ERR_NCX_SKIPPED) {
	pwd();
    }
    if (valset) {
	val_free_value(valset);
    }
    return res;

}  /* do_pwd */


/********************************************************************
 * FUNCTION do_cd
 * 
 * Change the current working directory
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   log_stdout global help message
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_cd (agent_cb_t *agent_cb,
	   const obj_template_t *rpc,
	   const xmlChar *line,
	   uint32  len)
{
    val_value_t      *valset, *parm;
    xmlChar          *pathstr;
    status_t          res;
    int               ret;
    boolean           imode;

    res = NO_ERR;
    imode = interactive_mode();
    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (res != NO_ERR) {
	if (valset) {
	    val_free_value(valset);
	}
	return res;
    }

    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_DIR);
    if (!parm || parm->res != NO_ERR) {
	val_free_value(valset);
        log_error("\nError: 'dir' parameter is missing");
	return ERR_NCX_MISSING_PARM;
    }

    res = NO_ERR;
    pathstr = ncx_get_source(VAL_STR(parm), &res);
    if (!pathstr) {
	val_free_value(valset);
        log_error("\nError: get path string failed (%s)",
                  get_error_string(res));
        return res;
    }

    ret = chdir((const char *)pathstr);
    if (ret) {
	res = ERR_NCX_INVALID_VALUE;
	if (imode) {
	    log_stdout("\nChange CWD failure (%s)\n",
		       get_error_string(errno_to_status()));
	} else {
	    log_write("\nChange CWD failure (%s)\n",
		      get_error_string(errno_to_status()));
	}
    } else {
	pwd();
    }

    val_free_value(valset);
    m__free(pathstr);

    return res;

}  /* do_cd */


/********************************************************************
 * FUNCTION do_fill
 * 
 * Fill an object for use in a PDU
 *
 * INPUTS:
 *    agent_cb == agent control block to use (NULL if none)
 *    rpc == RPC method for the load command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   is usually part of an assignment statement
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_fill (agent_cb_t *agent_cb,
	     const obj_template_t *rpc,
	     const xmlChar *line,
	     uint32  len)
{
    val_value_t           *valset, *valroot, *parm, *newparm, *curparm;
    val_value_t           *targval;
    const obj_template_t  *targobj;
    const xmlChar         *target;
    status_t               res;
    boolean                imode, save_getopt;

    res = NO_ERR;
    valset = get_valset(agent_cb, 
			rpc, 
			&line[len], 
			&res);
    if (res != NO_ERR) {
	if (valset) {
	    val_free_value(valset);
	}
	return res;
    }

    target = NULL;
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  NCX_EL_TARGET);
    if (!parm || parm->res != NO_ERR) {
	val_free_value(valset);
	return ERR_NCX_MISSING_PARM;
    } else {
	target = VAL_STR(parm);
    }

    save_getopt = agent_cb->get_optional;

    imode = interactive_mode();
    newparm = NULL;
    curparm = NULL;
    targobj = NULL;
    targval = NULL;

    valroot = get_instanceid_parm(agent_cb,
				  target,
				  TRUE,
				  &targobj,
				  &targval,
				  &res);
    if (res != NO_ERR) {
	if (valroot) {
	    val_free_value(valroot);
	}
	val_free_value(valset);
	clear_result(agent_cb);
	return res;
    } else if (targval != valroot) {
	/* keep targval, toss valroot */
	val_remove_child(targval);
	val_free_value(valroot);
	valroot = NULL;
    }

    /* check if targval is valid if it is an empty string
     * this corner-case is what the get_instanceid_parm will
     * return if the last node was a leaf
     */
    if (targval && 
	targval->btyp == NCX_BT_STRING &&
	VAL_STR(targval) == NULL) {

	/* the NULL string was not entered;
	 * if a value was entered as '' it would be recorded
	 * as a zero-length string, not a NULL string
	 */
	val_free_value(targval);
	targval = NULL;
    }

    /* find the value to use as content value or template, if any */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_VALUE);
    if (parm && parm->res == NO_ERR) {
	curparm = parm;
    }

    /* find the --optional flag */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_OPTIONAL);
    if (parm && parm->res == NO_ERR) {
	agent_cb->get_optional = TRUE;
    }

    /* fill in the value based on all the parameters */
    switch (targobj->objtype) {
    case OBJ_TYP_ANYXML:
    case OBJ_TYP_LEAF:
    case OBJ_TYP_LEAF_LIST:
	/* make a new leaf, toss the targval if any */
	newparm = fill_value(agent_cb, 
			     rpc, 
			     targobj, 
			     curparm,
			     &res);
	if (targval) {
	    val_free_value(targval);
	    targval = NULL;
	}
	break;
    case OBJ_TYP_CHOICE:
	if (targval) {
	    newparm = targval;
	} else {
	    newparm = val_new_value();
	    if (!newparm) {
		log_error("\nError: malloc failure");
		res = ERR_INTERNAL_MEM;
	    } else {
		val_init_from_template(newparm, targobj);
	    }
	}
	if (res == NO_ERR) {
	    res = get_choice(agent_cb, 
			     rpc, 
			     targobj, 
			     newparm, 
			     curparm);
	    if (res == ERR_NCX_SKIPPED) {
		res = NO_ERR;
	    }
	}
	break;
    case OBJ_TYP_CASE:
	if (targval) {
	    newparm = targval;
	} else {
	    newparm = val_new_value();
	    if (!newparm) {
		log_error("\nError: malloc failure");
		res = ERR_INTERNAL_MEM;
	    } else {
		val_init_from_template(newparm, targobj);
	    }
	}
	res = get_case(agent_cb, 
		       rpc, 
		       targobj, 
		       newparm, 
		       curparm);
	if (res == ERR_NCX_SKIPPED) {
	    res = NO_ERR;
	}
	break;
    default:
	if (targval) {
	    newparm = targval;
	} else {
	    newparm = val_new_value();
	    if (!newparm) {
		log_error("\nError: malloc failure");
		res = ERR_INTERNAL_MEM;
	    } else {
		val_init_from_template(newparm, targobj);
	    }
	}
	res = fill_valset(agent_cb, 
			  rpc, 
			  newparm, 
			  curparm);
	if (res == ERR_NCX_SKIPPED) {
	    res = NO_ERR;
	}
    }

    /* check save result or clear it */
    if (res == NO_ERR) {
	if (agent_cb->result_name || 
	    agent_cb->result_filename) {
	    /* save the filled in value */
	    res = finish_result_assign(agent_cb, 
				       newparm, 
				       NULL);
	    newparm = NULL;
	}
    } else {
	clear_result(agent_cb);
    }

    /* cleanup */
    val_free_value(valset);
    if (newparm) {
	val_free_value(newparm);
    }
    agent_cb->get_optional = save_getopt;

    return res;

}  /* do_fill */


/********************************************************************
* FUNCTION add_content
* 
* Add the config nodes to the parent
*
* INPUTS:
*   agent_cb == agent control block to use
*   rpc == RPC method in progress
*   config_content == the node associated with the target
*             to be used as content nested within the 
*             <config> element
*   curobj == the current object node for config_content, going
*                 up the chain to the root object.
*                 First call should pass config_content->obj
*   dofill == TRUE if interactive mode and mandatory parms
*             need to be specified (they can still be skipped)
*             FALSE if no checking for mandatory parms
*             Just fill out the minimum path to root from
*             the 'config_content' node
*   curtop == address of steady-storage node to add the
*             next new level
*
* OUTPUTS:
*    config node is filled in with child nodes
*    curtop is set to the latest top value
*       It is not needed after the last call and should be ignored
*
* RETURNS:
*    status; config_content is NOT freed if returning an error
*********************************************************************/
static status_t
    add_content (agent_cb_t *agent_cb,
		 const obj_template_t *rpc,
		 val_value_t *config_content,
		 const obj_template_t *curobj,
		 boolean dofill,
		 val_value_t **curtop)
{

    const obj_key_t       *curkey;
    val_value_t           *newnode, *keyval, *lastkey;
    status_t               res;
    boolean                done, content_used;

    res = NO_ERR;
    newnode = NULL;

    set_completion_state(&agent_cb->completion_state,
			 rpc,
			 curobj,
			 CMD_STATE_GETVAL);


    /* add content based on the current node type */
    switch (curobj->objtype) {
    case OBJ_TYP_ANYXML:
    case OBJ_TYP_LEAF:
    case OBJ_TYP_LEAF_LIST:
	if (curobj != config_content->obj) {
	    return SET_ERROR(ERR_INTERNAL_VAL);
	}
	if (!obj_is_key(config_content->obj)) {
	    val_add_child(config_content, *curtop);
	    *curtop = config_content;
	}
	break;
    case OBJ_TYP_LIST:
	/* get all the key nodes for the current object,
	 * if they do not already exist
	 */
	if (curobj == config_content->obj) {
	    val_add_child(config_content, *curtop);
	    *curtop = config_content;
	    return NO_ERR;
	}

	/* else need to fill in the keys for this content layer */
	newnode = val_new_value();
	if (!newnode) {
	    return ERR_INTERNAL_MEM;
	}
	val_init_from_template(newnode, curobj);
	val_add_child(newnode, *curtop);
	*curtop = newnode;
	content_used = FALSE;

	lastkey = NULL;
	for (curkey = obj_first_ckey(curobj);
	     curkey != NULL;
	     curkey = obj_next_ckey(curkey)) {

	    keyval = val_find_child(*curtop,
				    obj_get_mod_name(curkey->keyobj),
				    obj_get_name(curkey->keyobj));
	    if (!keyval) {
		if (curkey->keyobj == config_content->obj) {
		    keyval = config_content;
		    val_insert_child(keyval, lastkey, *curtop);
		    content_used = TRUE;
		    lastkey = keyval;
		    res = NO_ERR;
		} else if (dofill) {
		    res = get_parm(agent_cb, 
				   rpc, 
				   curkey->keyobj, 
				   *curtop, 
				   NULL);
		    if (res == ERR_NCX_SKIPPED) {
			res = NO_ERR;
		    } else if (res != NO_ERR) {
			return res;
		    } else {
			keyval = val_find_child(*curtop,
						obj_get_mod_name
						(curkey->keyobj),
						obj_get_name
						(curkey->keyobj));
			if (!keyval) {
			    return SET_ERROR(ERR_INTERNAL_VAL);
			}
			val_remove_child(keyval);
			val_insert_child(keyval, lastkey, *curtop);
			lastkey = keyval;
		    } /* else skip this key (for debugging agent) */
		}  /* else --nofill; skip this node */
	    }
	}

	/* wait until all the key leafs are accounted for before
	 * changing the *curtop pointer
	 */
	if (content_used) {
	    *curtop = config_content;
	}
	break;
    case OBJ_TYP_CONTAINER:
	if (curobj == config_content->obj) {
	    val_add_child(config_content, *curtop);
	    *curtop = config_content;
	} else {
	    newnode = val_new_value();
	    if (!newnode) {
		return ERR_INTERNAL_MEM;
	    }
	    val_init_from_template(newnode, curobj);
	    val_add_child(newnode, *curtop);
	    *curtop = newnode;
	}
	break;
    case OBJ_TYP_CHOICE:
    case OBJ_TYP_CASE:
	if (curobj != config_content->obj) {
	    /* nothing to do for the choice level if the target is a case */
	    res = NO_ERR;
	    break;
	}
	done = FALSE;
	while (!done) {
	    newnode = val_get_first_child(config_content);
	    if (newnode) {
		val_remove_child(newnode);
		res = add_content(agent_cb, 
				  rpc, 
				  newnode, 
				  newnode->obj, 
				  dofill,
				  curtop);
		if (res != NO_ERR) {
		    val_free_value(newnode);
		    done = TRUE;
		}
	    } else {
		done = TRUE;
	    }
	}
	if (res == NO_ERR) {
	    val_free_value(config_content);
	}
	*curtop = newnode;
	break;
    default:
	/* any other object type is an error */
	res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    return res;

}  /* add_content */


/********************************************************************
* FUNCTION add_config_from_content_node
* 
* Add the config node content for the edit-config operation
* Build the <config> nodfe top-down, by recursing bottom-up
* from the node to be edited.
*
* INPUTS:
*   agent_cb == agent control block to use
*   rpc == RPC method in progress
*   config_content == the node associated with the target
*             to be used as content nested within the 
*             <config> element
*   curobj == the current object node for config_content, going
*                 up the chain to the root object.
*                 First call should pass config_content->obj
*   config == the starting <config> node to add the data into
*   curtop == address of stable storage for current add-to node
*            This pointer MUST be set to NULL upon first fn call
* OUTPUTS:
*    config node is filled in with child nodes
*
* RETURNS:
*    status; config_content is NOT freed if returning an error
*********************************************************************/
static status_t
    add_config_from_content_node (agent_cb_t *agent_cb,
				  const obj_template_t *rpc,
				  val_value_t *config_content,
				  const obj_template_t *curobj,
				  val_value_t *config,
				  val_value_t **curtop)
{
    const obj_template_t  *parent;
    status_t               res;

    /* get to the root of the object chain */
    parent = obj_get_cparent(curobj);
    if (parent && !obj_is_root(parent)) {
	res = add_config_from_content_node(agent_cb,
					   rpc,
					   config_content,
					   parent,
					   config,
					   curtop);
	if (res != NO_ERR) {
	    return res;
	}
    }

    /* set the current target, working down the stack
     * on the way back from the initial dive
     */
    if (!*curtop) {
	/* first time through to this point */
	*curtop = config;
    }

    res = add_content(agent_cb, 
		      rpc, 
		      config_content, 
		      curobj, 
		      TRUE,
		      curtop);

    return res;

}  /* add_config_from_content_node */


/********************************************************************
* FUNCTION complete_path_content
* 
* Use the valroot and content pointer to work up the already
* constructed value tree to find any missing mandatory
* nodes or key leafs
*
* INPUTS:
*   agent_cb == agent control block to use
*   rpc == RPC method in progress
*   valroot == value root of content
*           == NULL if not used (config_content used instead)
*   config_content == the node associated with the target
*                    within valroot (if valroot non-NULL)
*   dofill == TRUE if fill mode
*          == FALSE if --nofill is in effect
*
* OUTPUTS:
*    valroot tree is filled out as needed and requested
*
* RETURNS:
*    status; config_content is NOT freed if returning an error
*********************************************************************/
static status_t
    complete_path_content (agent_cb_t *agent_cb,
			   const obj_template_t *rpc,
			   val_value_t *valroot,
			   val_value_t *config_content,
			   boolean dofill)
{
    const obj_key_t   *curkey;
    val_value_t       *curnode, *keyval, *lastkey;
    status_t           res;
    boolean            done;

    res = NO_ERR;

    curnode = config_content;

    /* handle all the nodes from bottom to valroot */
    done = FALSE;
    while (!done) {
	if (curnode->btyp == NCX_BT_LIST) {
	    lastkey = NULL;
	    for (curkey = obj_first_ckey(curnode->obj);
		 curkey != NULL;
		 curkey = obj_next_ckey(curkey)) {

		keyval = val_find_child(curnode,
					obj_get_mod_name(curkey->keyobj),
					obj_get_name(curkey->keyobj));
		if (!keyval && dofill) {
		    res = get_parm(agent_cb, 
				   rpc, 
				   curkey->keyobj, 
				   curnode, 
				   NULL);
		    if (res == ERR_NCX_SKIPPED) {
			res = NO_ERR;
		    } else if (res != NO_ERR) {
			return res;
		    } else {
			keyval = val_find_child(curnode,
						obj_get_mod_name
						(curkey->keyobj),
						obj_get_name
						(curkey->keyobj));
			if (!keyval) {
			    return SET_ERROR(ERR_INTERNAL_VAL);
			}
			val_remove_child(keyval);
			val_insert_child(keyval, lastkey, curnode);
			lastkey = keyval;
		    } /* else skip this key (for debugging agent) */
		}  /* else --nofill; skip this node */
	    } /* for all the keys in the list */
	} /* else not list so skip this node */

	/* move up the tree */
	if (curnode != valroot && curnode->parent != NULL) {
	    curnode = curnode->parent;
	} else {
	    done = TRUE;
	}
    }

    return res;

}  /* complete_path_content */


/********************************************************************
* FUNCTION add_filter_from_content_node
* 
* Add the filter node content for the get or get-config operation
* Build the <filter> node top-down, by recursing bottom-up
* from the node to be edited.
*
* INPUTS:
*   agent_cb == agent control block to use
*   rpc == RPC method in progress
*   get_content == the node associated with the target
*             to be used as content nested within the 
*             <filter> element
*   curobj == the current object node for get_content, going
*                 up the chain to the root object.
*                 First call should pass get_content->obj
*   filter == the starting <filter> node to add the data into
*   dofill == TRUE for fill mode
*             FALSE to skip filling any nodes
*   curtop == address of stable storage for current add-to node
*            This pointer MUST be set to NULL upon first fn call
* OUTPUTS:
*    filter node is filled in with child nodes
*
* RETURNS:
*    status; get_content is NOT freed if returning an error
*********************************************************************/
static status_t
    add_filter_from_content_node (agent_cb_t *agent_cb,
				  const obj_template_t *rpc,
				  val_value_t *get_content,
				  const obj_template_t *curobj,
				  val_value_t *filter,
				  boolean dofill,
				  val_value_t **curtop)
{
    const obj_template_t  *parent;
    status_t               res;

    /* get to the root of the object chain */
    parent = obj_get_cparent(curobj);
    if (parent && !obj_is_root(parent)) {
	res = add_filter_from_content_node(agent_cb,
					   rpc,
					   get_content,
					   parent,
					   filter,
					   dofill,
					   curtop);
	if (res != NO_ERR) {
	    return res;
	}
    }

    /* set the current target, working down the stack
     * on the way back from the initial dive
     */
    if (!*curtop) {
	/* first time through to this point */
	*curtop = filter;
    }

    res = add_content(agent_cb, 
		      rpc, 
		      get_content, 
		      curobj, 
		      dofill,
		      curtop);
    return res;

}  /* add_filter_from_content_node */


/********************************************************************
* FUNCTION send_edit_config_to_agent
* 
* Send an <edit-config> operation to the agent
*
* Fills out the <config> node based on the config_target node
* Any missing key nodes will be collected (via CLI prompt)
* along the way.
*
* INPUTS:
*   agent_cb == agent control block to use
*   valroot == value tree root if used
*              (via get_content_from_choice)
*   config_content == the node associated with the target
*             to be used as content nested within the 
*             <config> element
*   timeoutval == timeout value to use
*
* OUTPUTS:
*    agent_cb->state may be changed or other action taken
*
*    !!! valroot is consumed id non-NULL
*    !!! config_content is consumed -- freed or transfered to a PDU
*    !!! that will be freed later
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    send_edit_config_to_agent (agent_cb_t *agent_cb,
			       val_value_t *valroot,
			       val_value_t *config_content,
			       uint32 timeoutval)
{
    const obj_template_t  *rpc, *input, *child;
    const xmlChar         *defopstr;
    mgr_rpc_req_t         *req;
    val_value_t           *reqdata, *parm, *target, *dummy_parm;
    ses_cb_t              *scb;
    status_t               res;
    boolean                freeroot, dofill;

    req = NULL;
    reqdata = NULL;
    res = NO_ERR;
    dofill = FALSE;

    /* either going to free valroot or config_content */
    if (valroot == NULL || valroot == config_content) {
	freeroot = FALSE;
    } else {
	freeroot = TRUE;
    }

    /* make sure there is an edit target on this agent */
    if (!agent_cb->default_target) {
	log_error("\nError: no <edit-config> target available on agent");
	if (freeroot) {
	    val_free_value(valroot);
	} else {
	    val_free_value(config_content);
	}
	return ERR_NCX_OPERATION_FAILED;
    }

    /* get the <edit-config> template */
    rpc = ncx_find_object(get_netconf_mod(), 
			  NCX_EL_EDIT_CONFIG);
    if (!rpc) {
	if (freeroot) {
	    val_free_value(valroot);
	} else {
	    val_free_value(config_content);
	}
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    /* get the 'input' section container */
    input = obj_find_child(rpc, NULL, YANG_K_INPUT);
    if (!input) {
	if (freeroot) {
	    val_free_value(valroot);
	} else {
	    val_free_value(config_content);
	}
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    /* construct a method + parameter tree */
    reqdata = xml_val_new_struct(obj_get_name(rpc), 
				 obj_get_nsid(rpc));
    if (!reqdata) {
	if (freeroot) {
	    val_free_value(valroot);
	} else {
	    val_free_value(config_content);
	}
	log_error("\nError allocating a new RPC request");
	return ERR_INTERNAL_MEM;
    }

    /* set the edit-config/input/target node to the default_target */
    child = obj_find_child(input, 
			   NC_MODULE,
			   NCX_EL_TARGET);
    parm = val_new_value();
    if (!parm) {
	if (freeroot) {
	    val_free_value(valroot);
	} else {
	    val_free_value(config_content);
	}
	val_free_value(reqdata);
	return ERR_INTERNAL_MEM;
    }
    val_init_from_template(parm, child);
    val_add_child(parm, reqdata);

    target = xml_val_new_flag(agent_cb->default_target,
			      obj_get_nsid(child));
    if (!target) {
	if (freeroot) {
	    val_free_value(valroot);
	} else {
	    val_free_value(config_content);
	}
	val_free_value(reqdata);
	return ERR_INTERNAL_MEM;
    }

    val_add_child(target, parm);

    /* set the edit-config/input/default-operation node */
    if (!(agent_cb->defop == OP_DEFOP_NOT_USED ||
          agent_cb->defop == OP_DEFOP_NOT_SET)) {          
        child = obj_find_child(input, 
                               NC_MODULE,
                               NCX_EL_DEFAULT_OPERATION);
        res = NO_ERR;
        defopstr = op_defop_name(agent_cb->defop);
        parm = val_make_simval_obj(child, defopstr, &res);
        if (!parm) {
            if (freeroot) {
                val_free_value(valroot);
            } else {
                val_free_value(config_content);
            }
            val_free_value(reqdata);
            return res;
        }
        val_add_child(parm, reqdata);
    }

    /* set the test-option to the user-configured or default value */
    if (agent_cb->testoption != OP_TESTOP_NONE) {
	/* set the edit-config/input/test-option node to
	 * the user-specified value
	 */
	child = obj_find_child(input, 
			       NC_MODULE,
			       NCX_EL_TEST_OPTION);
	res = NO_ERR;
	parm = val_make_simval_obj(child,
				   op_testop_name(agent_cb->testoption),
				   &res);
	if (!parm) {
	    if (freeroot) {
		val_free_value(valroot);
	    } else {
		val_free_value(config_content);
	    }
	    val_free_value(reqdata);
	    return res;
	}
    }

    /* set the error-option to the user-configured or default value */
    if (agent_cb->erroption != OP_ERROP_NONE) {
	/* set the edit-config/input/error-option node to
	 * the user-specified value
	 */
	child = obj_find_child(input, 
			       NC_MODULE,
			       NCX_EL_ERROR_OPTION);
	res = NO_ERR;
	parm = val_make_simval_obj(child,
				   op_errop_name(agent_cb->erroption),
				   &res);
	if (!parm) {
	    if (freeroot) {
		val_free_value(valroot);
	    } else {
		val_free_value(config_content);
	    }
	    val_free_value(reqdata);
	    return ERR_INTERNAL_MEM;
	}
    }

    /* create the <config> node */
    child = obj_find_child(input, 
			   NC_MODULE,
			   NCX_EL_CONFIG);
    parm = val_new_value();
    if (!parm) {
	if (freeroot) {
	    val_free_value(valroot);
	} else {
	    val_free_value(config_content);
	}
	val_free_value(reqdata);
	return ERR_INTERNAL_MEM;
    }
    val_init_from_template(parm, child);
    val_add_child(parm, reqdata);


    /* set the edit-config/input/config node to the
     * config_content, but after filling in any
     * missing nodes from the root to the target
     */
    if (valroot) {
	val_add_child(valroot, parm);
	res = complete_path_content(agent_cb,
				    rpc,
				    valroot,
				    config_content,
				    dofill);
	if (res != NO_ERR) {
	    val_free_value(valroot);
	    val_free_value(reqdata);
	    return res;
	}
    } else {
	dummy_parm = NULL;
	res = add_config_from_content_node(agent_cb,
					   rpc, 
					   config_content,
					   config_content->obj,
					   parm, 
					   &dummy_parm);
	if (res != NO_ERR) {
	    val_free_value(config_content);
	    val_free_value(reqdata);
	    return res;
	}
    }

    /* rearrange the nodes to canonical order if requested */
    if (agent_cb->fixorder) {
	/* must set the order of a root container seperately */
	val_set_canonical_order(parm);
    }

    /* !!! config_content consumed at this point !!!
     * allocate an RPC request
     */
    scb = mgr_ses_get_scb(agent_cb->mysid);
    if (!scb) {
	res = SET_ERROR(ERR_INTERNAL_PTR);
    } else {
	req = mgr_rpc_new_request(scb);
	if (!req) {
	    res = ERR_INTERNAL_MEM;
	    log_error("\nError allocating a new RPC request");
	} else {
	    req->data = reqdata;
	    req->rpc = rpc;
	    req->timeout = timeoutval;
	}
    }
	
    /* if all OK, send the RPC request */
    if (res == NO_ERR) {
	if (LOGDEBUG2) {
	    log_debug2("\nabout to send RPC request with reqdata:");
	    val_dump_value_ex(reqdata, 
                              NCX_DEF_INDENT,
                              agent_cb->display_mode);
	}

	/* the request will be stored if this returns NO_ERR */
	res = mgr_rpc_send_request(scb, req, yangcli_reply_handler);
    }

    /* cleanup and set next state */
    if (res != NO_ERR) {
	if (req) {
	    mgr_rpc_free_request(req);
	} else if (reqdata) {
	    val_free_value(reqdata);
	}
    } else {
	agent_cb->state = MGR_IO_ST_CONN_RPYWAIT;
    }

    return res;

} /* send_edit_config_to_agent */


/********************************************************************
 * FUNCTION add_filter_attrs
 * 
 * Add the type and possibly the select 
 * attribute to a value node
 *
 * INPUTS:
 *    val == value node to set
 *    selectval == select node to use, (type=xpath)
 *               == NULL for type = subtree
 *              !!! this memory is consumed here !!!
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_filter_attrs (val_value_t *val,
		      val_value_t *selectval)
{
    val_value_t          *metaval;

    /* create a value node for the meta-value */
    metaval = val_make_string(0, NCX_EL_TYPE,
			      (selectval) 
			      ?  NCX_EL_XPATH : NCX_EL_SUBTREE);
    if (!metaval) {
	return ERR_INTERNAL_MEM;
    }
    dlq_enque(metaval, &val->metaQ);

    if (selectval) {
        val_set_qname(selectval,
                      0,
                      NCX_EL_SELECT,
                      xml_strlen(NCX_EL_SELECT));
	dlq_enque(selectval, &val->metaQ);
    }

    return NO_ERR;

} /* add_filter_attrs */


/********************************************************************
* FUNCTION send_get_to_agent
* 
* Send an <get> operation to the specified agent
*
* Fills out the <filter> node based on the config_target node
* Any missing key nodes will be collected (via CLI prompt)
* along the way.
*
* INPUTS:
*   agent_cb == agent control block to use
*   valroot == root of get_data if set via
*              the CLI Xpath code; if so, then the
*              get_content is the target within this
*              subtree (may == get_content)
*           == NULL if not used and get_content is
*              the only parameter gathered from input
*   get_content == the node associated with the target
*             to be used as content nested within the 
*             <filter> element
*             == NULL to use selectval instead
*   selectval == value with select string in it
*             == NULL to use get_content instead
*   source == optional database source 
*             <candidate>, <running>
*   timeoutval == timeout value to use
*   dofill == TRUE if interactive mode and mandatory parms
*             need to be specified (they can still be skipped)
*             FALSE if no checking for mandatory parms
*             Just fill out the minimum path to root from
*             the 'get_content' node
*   withdef == the desired with-defaults parameter
*              It may be ignored or altered, depending on
*              whether the agent supports the capability or not
*
* OUTPUTS:
*    agent_cb->state may be changed or other action taken
*
*    !!! valroot is consumed -- freed or transfered to a PDU
*    !!! that will be freed later
*
*    !!! get_content is consumed -- freed or transfered to a PDU
*    !!! that will be freed later
*
*    !!! source is consumed -- freed or transfered to a PDU
*    !!! that will be freed later
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    send_get_to_agent (agent_cb_t *agent_cb,
		       val_value_t *valroot,
		       val_value_t *get_content,
		       val_value_t *selectval,
		       val_value_t *source,
		       uint32 timeoutval,
		       boolean dofill,
		       ncx_withdefaults_t withdef)
{
    const obj_template_t  *rpc, *input, *withdefobj;
    val_value_t           *reqdata, *filter;
    val_value_t           *withdefval, *dummy_parm;
    mgr_rpc_req_t         *req;
    ses_cb_t              *scb;
    mgr_scb_t             *mscb;
    status_t               res;
    boolean                freeroot;

    req = NULL;
    reqdata = NULL;
    res = NO_ERR;
    input = NULL;

    /* either going to free valroot or config_content */
    if (valroot == NULL || valroot == get_content) {
	freeroot = FALSE;
    } else {
	freeroot = TRUE;
    }

    /* get the <get> or <get-config> input template */
    rpc = ncx_find_object(get_netconf_mod(),
			  (source) ? 
			  NCX_EL_GET_CONFIG : NCX_EL_GET);
    if (rpc) {
	input = obj_find_child(rpc, NULL, YANG_K_INPUT);
    }

    if (!input) {
	res = SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    } else {
	/* construct a method + parameter tree */
	reqdata = xml_val_new_struct(obj_get_name(rpc), 
				     obj_get_nsid(rpc));
	if (!reqdata) {
	    log_error("\nError allocating a new RPC request");
	    res = ERR_INTERNAL_MEM;
	}
    }

    /* add /get-star/input/source */
    if (res == NO_ERR) {
	if (source) {
	    val_add_child(source, reqdata);
	}
    }

    /* add /get-star/input/filter */
    if (res == NO_ERR) {
	if (get_content) {
	    /* set the get/input/filter node to the
	     * get_content, but after filling in any
	     * missing nodes from the root to the target
	     */
	    filter = xml_val_new_struct(NCX_EL_FILTER, xmlns_nc_id());
	} else {
	    filter = xml_val_new_flag(NCX_EL_FILTER, xmlns_nc_id());
	}
	if (!filter) {
	    log_error("\nError: malloc failure");
	    res = ERR_INTERNAL_MEM;
	} else {
	    val_add_child(filter, reqdata);
	}
    }

    /* add the type and maybe select attributes */
    if (res == NO_ERR) {
	res = add_filter_attrs(filter, selectval);
    }

    /* check any errors so far */
    if (res != NO_ERR) {
	if (freeroot) {
	    val_free_value(valroot);
	} else if (get_content) {
	    val_free_value(get_content);
	} else if (selectval) {
            val_free_value(selectval);
        }
	val_free_value(reqdata);
	return res;
    }

    /* add the content to the filter element
     * building the path from the content node
     * to the root; fill if dofill is true
     */
    if (valroot) {
	val_add_child(valroot, filter);
	res = complete_path_content(agent_cb,
				    rpc,
				    valroot,
				    get_content,
				    dofill);
	if (res != NO_ERR) {
	    val_free_value(valroot);
	    val_free_value(reqdata);
	    return res;
	}
    } else if (get_content) {
	dummy_parm = NULL;
	res = add_filter_from_content_node(agent_cb,
					   rpc, 
					   get_content,
					   get_content->obj,
					   filter, 
					   dofill,
					   &dummy_parm);
	if (res != NO_ERR && res != ERR_NCX_SKIPPED) {
	    /*  val_free_value(get_content); already freed! */
	    val_free_value(reqdata);
	    return res;
	}
	res = NO_ERR;
    }

    /* get the session control block */
    scb = mgr_ses_get_scb(agent_cb->mysid);
    if (!scb) {
	res = SET_ERROR(ERR_INTERNAL_PTR);
    }

    /* !!! get_content consumed at this point !!!
     * check if the with-defaults parmaeter should be added
     */
    if (res == NO_ERR) {
	mscb = mgr_ses_get_mscb(scb);
	if (cap_std_set(&mscb->caplist, CAP_STDID_WITH_DEFAULTS)) {
	    switch (withdef) {
	    case NCX_WITHDEF_NONE:
		break;
	    case NCX_WITHDEF_TRIM:
	    case NCX_WITHDEF_EXPLICIT:
		/*** !!! NEED TO CHECK IF TRIM / EXPLICT 
		 *** !!! REALLY SUPPORTED IN THE caplist
		 ***/
		/* fall through */
	    case NCX_WITHDEF_REPORT_ALL:
		/* it is OK to send a with-defaults to this agent */
		withdefobj = obj_find_child(input, 
					    NULL,
					    NCX_EL_WITH_DEFAULTS);
		if (!withdefobj) {
		    SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
		} else {
		    withdefval = val_make_simval_obj
			(withdefobj,
			 ncx_get_withdefaults_string(withdef),
			 &res);
		    if (withdefval) {
			val_add_child(withdefval, reqdata);
		    }
		}
		break;
	    default:
		SET_ERROR(ERR_INTERNAL_VAL);
	    }
	} else {
	    log_warn("\nWarning: 'with-defaults' "
		     "capability not-supported so parameter ignored");
	}
    }

    if (res == NO_ERR) {
	/* allocate an RPC request and send it */
	req = mgr_rpc_new_request(scb);
	if (!req) {
	    res = ERR_INTERNAL_MEM;
	    log_error("\nError allocating a new RPC request");
	} else {
	    req->data = reqdata;
	    req->rpc = rpc;
	    req->timeout = timeoutval;
	}
    }
	
    if (res == NO_ERR) {
	if (LOGDEBUG2) {
	    log_debug2("\nabout to send RPC request with reqdata:");
	    val_dump_value_ex(reqdata, 
                              NCX_DEF_INDENT,
                              agent_cb->display_mode);
	}

	/* the request will be stored if this returns NO_ERR */
	res = mgr_rpc_send_request(scb, req, yangcli_reply_handler);
    }

    if (res != NO_ERR) {
	if (req) {
	    mgr_rpc_free_request(req);
	} else if (reqdata) {
	    val_free_value(reqdata);
	}
    } else {
	agent_cb->state = MGR_IO_ST_CONN_RPYWAIT;
    }

    return res;

} /* send_get_to_agent */


/********************************************************************
 * FUNCTION get_content_from_choice
 * 
 * Get the content input for the EditOps from choice
 *
 * If the choice is 'from-cli' and this is interactive mode
 * then the fill_valset will be called to get input
 * based on the 'target' parameter also in the valset
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the load command
 *    valset == parsed CLI valset
 *    getoptional == TRUE if optional nodes are desired
 *    isdelete == TRUE if this is for an edit-config delete
 *                FALSE for some other operation
 *    dofill == TRUE to fill the content,
 *              FALSE to skip fill phase
 *    iswrite == TRUE if write operation
 *               FALSE if a read operation
 *    valroot == address of return value root pointer
 *               used when the content is from the CLI XPath
 *
 * OUTPUTS:
 *    *valroot ==  pointer to malloced value tree content root. 
 *        This is the top of the tree that will be added
 *        as a child of the <config> node for the edit-config op
 *        If this is non-NULL, then the return value should
 *        not be freed. This pointer should be freed instead.
 *        == NULL if the from choice mode does not use
 *           an XPath schema-instance node as input
 *
 * RETURNS:
 *    result of the choice; this is the content
 *    that will be affected by the edit-config operation
 *    via create, merge, or replace
 *
 *   This is the specified target node in the
 *   content target, which may be the 
 *   same as the content root or will, if *valroot != NULL
 *   be a descendant of the content root 
 *
 *   If *valroot == NULL then this is a malloced pointer that
 *   must be freed by the caller
 *********************************************************************/
static val_value_t *
    get_content_from_choice (agent_cb_t *agent_cb,
			     const obj_template_t *rpc,
			     val_value_t *valset,
			     boolean getoptional,
			     boolean isdelete,
			     boolean dofill,
			     boolean iswrite,
			     val_value_t **valroot)
{
    val_value_t           *parm, *curparm, *newparm;
    const val_value_t     *userval;
    val_value_t           *targval;
    const obj_template_t  *targobj;
    const xmlChar         *fromstr, *target;
    var_type_t             vartype;
    boolean                iscli, isselect, saveopt;
    status_t               res;

    /* init locals */
    targobj = NULL;
    targval = NULL;
    newparm = NULL;
    fromstr = NULL;
    iscli = FALSE;
    isselect = FALSE;
    res = NO_ERR;
    vartype = VAR_TYP_NONE;

    *valroot = NULL;

    /* look for the 'from' parameter variant */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_VARREF);
    if (parm) {
	/* content = uservar or $varref */
	fromstr = VAL_STR(parm);
    } else {
	parm = val_find_child(valset, 
			      YANGCLI_MOD, 
			      NCX_EL_SELECT);
	if (parm) {
	    /* content == select string for xget or xget-config */
	    isselect = TRUE;
	    fromstr = VAL_STR(parm);
	} else {
	    /* last choice; content is from the CLI */
	    iscli = TRUE;
	}
    }

    if (iscli) {
	saveopt = agent_cb->get_optional;
	agent_cb->get_optional = getoptional;

	/* from CLI -- look for the 'target' parameter */
	parm = val_find_child(valset, 
			      YANGCLI_MOD, 
			      NCX_EL_TARGET);
	if (!parm) {
	    log_error("\nError: target parameter is missing");
	    agent_cb->get_optional = saveopt;
	    return NULL;
	}

	target = VAL_STR(parm);

	*valroot = get_instanceid_parm(agent_cb,
				       target,
				       TRUE,
				       &targobj,
				       &targval,
				       &res);
	if (res != NO_ERR) {
	    log_error("\nError: invalid target parameter (%s)",
		      get_error_string(res));
	    if (*valroot) {
		val_free_value(*valroot);
		*valroot = NULL;
	    }
	    agent_cb->get_optional = saveopt;
	    return NULL;
	}

	curparm = NULL;
	parm = val_find_child(valset, 
			      YANGCLI_MOD, 
			      YANGCLI_VALUE);
	if (parm && parm->res == NO_ERR) {
	    curparm = var_get_script_val(targobj, 
					 NULL, 
					 VAL_STR(parm),
					 ISPARM, 
					 &res);
	    if (!curparm || res != NO_ERR) {
		log_error("\nError: Script value '%s' invalid (%s)", 
			  VAL_STR(parm), 
			  get_error_string(res)); 
		agent_cb->get_optional = saveopt;
		val_free_value(*valroot);
		*valroot = NULL;
		if (curparm) {
		    val_free_value(curparm);
		}
		return NULL;
	    }
	    if (curparm->obj != targobj) {
		log_error("\nError: current value '%s' "
			  "object type is incorrect.",
			  VAL_STR(parm));
		agent_cb->get_optional = saveopt;
		val_free_value(*valroot);
		*valroot = NULL;
		val_free_value(curparm);
		return NULL;
	    }
	}

	switch (targobj->objtype) {
        case OBJ_TYP_ANYXML:
	case OBJ_TYP_LEAF:
	    if (isdelete) {
		dofill = FALSE;
	    } else if (!iswrite && 
		       obj_is_single_instance(targobj)) {
		dofill = FALSE;
	    } /* else fall through */
	case OBJ_TYP_LEAF_LIST:
	    if (dofill) {
		/* if targval is non-NULL then it is an empty
		 * value struct because it was the deepest node
		 * specified in the schema-instance string
		 */
		newparm = fill_value(agent_cb, 
				     rpc, 
				     targobj, 
				     curparm,
				     &res);

		if (newparm && res == NO_ERR) {
		    if (targval) {
			if (targval == *valroot) {
			    /* get a top-level leaf or leaf-list
			     * so get rid of the instance ID and
			     * just use the fill_value result
			     */
			    val_free_value(*valroot);
			    *valroot = NULL;
			} else {
			    /* got a leaf or leaf-list that is not
			     * a top-level node so just swap
			     * out the old leaf for the new leaf
			     */
			    val_swap_child(newparm, targval);
			    val_free_value(targval);
			    targval = NULL;
			}
		    }
		}
	    } else if (targval == NULL) {
		newparm = val_new_value();
		if (!newparm) {
		    log_error("\nError: malloc failure");
		    res = ERR_INTERNAL_MEM;
		} else {
		    val_init_from_template(newparm, targobj);
		}
		if (isdelete) {
		    val_reset_empty(newparm);
		}
	    } else if (isdelete) {
		val_reset_empty(targval);
	    }  /* else going to use targval, not newval */
	    break;
	case OBJ_TYP_CHOICE:
	    if (targval == NULL) {
		newparm = val_new_value();
		if (!newparm) {
		    log_error("\nError: malloc failure");
		    res = ERR_INTERNAL_MEM;
		} else {
		    val_init_from_template(newparm, targobj);
		}
	    }
	    if (res == NO_ERR && dofill) {
		res = get_choice(agent_cb, 
				 rpc, 
				 targobj,
				 (newparm) ? newparm : targval,
				 curparm);
	    }
	    break;
	case OBJ_TYP_CASE:
	    if (targval == NULL) {
		newparm = val_new_value();
		if (!newparm) {
		    log_error("\nError: malloc failure");
		    res = ERR_INTERNAL_MEM;
		} else {
		    val_init_from_template(newparm, targobj);
		}
	    }
	    if (res == NO_ERR && dofill) {
		res = get_case(agent_cb, 
			       rpc, 
			       targobj,
			       (newparm) ? newparm : targval, 
			       curparm);
	    }
	    break;
	default:
	    if (targval == NULL) {
		newparm = val_new_value();
		if (!newparm) {
		    log_error("\nError: malloc failure");
		    res = ERR_INTERNAL_MEM;
		} else {
		    val_init_from_template(newparm, targobj);
		}
	    }
	    if (res == NO_ERR && dofill) {
		res = fill_valset(agent_cb, 
				  rpc,
				  (newparm) ? newparm : targval,
				  curparm);
	    }
	}

	/* done with the current value */
	if (curparm) {
	    val_free_value(curparm);
	}

	agent_cb->get_optional = saveopt;
	if (res == ERR_NCX_SKIPPED) {
	    if (newparm) {
		val_free_value(newparm);
	    }
	    newparm = 
		xml_val_new_flag(obj_get_name(targobj),
				 obj_get_nsid(targobj));
	    if (!newparm) {
		res = ERR_INTERNAL_MEM;
		log_error("\nError: malloc failure");
	    } else {
		/* need to set the real object so the
		 * path to root will be built correctly
		 */
		newparm->obj = targobj;
	    }

	    if (res != NO_ERR) {
		val_free_value(*valroot);
		*valroot = NULL;
	    } /* else *valroot is active and in use */

	    return newparm;
	} else if (res != NO_ERR) {
	    if (newparm) {
		val_free_value(newparm);
	    }
	    val_free_value(*valroot);
	    *valroot = NULL;
	    return NULL;
	} else {
	    /* *valroot is active and in use */
	    return (newparm) ? newparm : targval;
	}
    } else if (isselect) {
        newparm = val_new_value();
        if (!newparm) {
	    log_error("\nError: malloc failed");
        } else {
            val_init_from_template(newparm, parm->obj);
            res = val_set_simval_obj(newparm,
                                     parm->obj,
                                     fromstr);
            if (res != NO_ERR) {
                log_error("\nError: set 'select' failed (%s)",
                          get_error_string(res));
                val_free_value(newparm);
                newparm = NULL;
            }
	}
	/*  *valroot == NULL and not in use */
	return newparm;
    } else {
	/* from global or local variable 
	 * *valroot == NULL and not used
	 */
	if (!fromstr) {
	    ;  /* should not be NULL */
	} else if (*fromstr == '$' && fromstr[1] == '$') {
	    /* $$foo */
	    vartype = VAR_TYP_GLOBAL;
	    fromstr += 2;
	} else if (*fromstr == '$') {
	    /* $foo */
	    vartype = VAR_TYP_LOCAL;
	    fromstr++;
	} else {
	    /* 'foo' : just assume local, not error */
	    vartype = VAR_TYP_LOCAL;
	}
	if (fromstr) {
	    userval = var_get(fromstr, vartype);
	    if (!userval) {
		log_error("\nError: variable '%s' not found", 
			  fromstr);
		return NULL;
	    } else {
		newparm = val_clone(userval);
		if (!newparm) {
		    log_error("\nError: valloc failed");
		}
		return newparm;
	    }
	}
    }
    return NULL;

}  /* get_content_from_choice */


/********************************************************************
 * FUNCTION add_one_operation_attr
 * 
 * Add the nc:operation attribute to a value node
 *
 * INPUTS:
 *    val == value node to set
 *    op == edit operation to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_one_operation_attr (val_value_t *val,
			    op_editop_t op)
{
    const obj_template_t *operobj;
    const xmlChar        *editopstr;
    val_value_t          *metaval;
    status_t              res;

    /* get the internal nc:operation object */
    operobj = ncx_find_object(get_netconf_mod(), 
			      NC_OPERATION_ATTR_NAME);
    if (!operobj) {
	return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    /* create a value node for the meta-value */
    metaval = val_new_value();
    if (!metaval) {
	return ERR_INTERNAL_MEM;
    }
    val_init_from_template(metaval, operobj);

    /* get the string value for the edit operation */
    editopstr = op_editop_name(op);
    if (!editopstr) {
	val_free_value(metaval);
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* set the meta variable value and other fields */
    res = val_set_simval(metaval,
			 obj_get_ctypdef(operobj),
			 obj_get_nsid(operobj),
			 obj_get_name(operobj),
			 editopstr);

    if (res != NO_ERR) {
	val_free_value(metaval);
	return res;
    }

    dlq_enque(metaval, &val->metaQ);
    return NO_ERR;

} /* add_one_operation_attr */


/********************************************************************
 * FUNCTION add_operation_attr
 * 
 * Add the nc:operation attribute to a value node
 *
 * INPUTS:
 *    val == value node to set
 *    op == edit operation to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_operation_attr (val_value_t *val,
			op_editop_t op)
{
    val_value_t          *childval;
    status_t              res;

    res = NO_ERR;

    switch (val->obj->objtype) {
    case OBJ_TYP_CHOICE:
    case OBJ_TYP_CASE:
	for (childval = val_get_first_child(val);
	     childval != NULL;
	     childval = val_get_next_child(childval)) {

	    res = add_one_operation_attr(childval, op);
	    if (res != NO_ERR) {
		return res;
	    }
	}
	break;
    default:
	res = add_one_operation_attr(val, op);
    }

    return res;

} /* add_operation_attr */


/********************************************************************
 * FUNCTION add_one_insert_attrs
 * 
 * Add the yang:insert attribute(s) to a value node
 *
 * INPUTS:
 *    val == value node to set
 *    insop == insert operation to use
 *    edit_target == edit target to use for key or value attr
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_one_insert_attrs (val_value_t *val,
			  op_insertop_t insop,
			  const xmlChar *edit_target)
{
    const obj_template_t *operobj;
    const xmlChar        *insopstr;
    val_value_t          *metaval;
    ncx_node_t            dtyp;
    status_t              res;
    xmlns_id_t            yangid;

    yangid = xmlns_yang_id();

    /* get the internal nc:operation object */
    dtyp = NCX_NT_OBJ;
    operobj = ncx_get_gen_string();
    if (!operobj) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    insopstr = op_insertop_name(insop);

    /* create a value node for the meta-value */
    metaval = val_new_value();
    if (!metaval) {
	return ERR_INTERNAL_MEM;
    }
    val_init_from_template(metaval, operobj);
    val_set_qname(metaval, yangid,
		  YANG_K_INSERT,
		  xml_strlen(YANG_K_INSERT));

    /* set the meta variable value and other fields */
    res = val_set_simval(metaval,
			 metaval->typdef,
			 yangid,
			 YANG_K_INSERT,
			 insopstr);
    if (res != NO_ERR) {
	val_free_value(metaval);
	return res;
    } else {
	dlq_enque(metaval, &val->metaQ);
    }

    if (insop == OP_INSOP_BEFORE || insop == OP_INSOP_AFTER) {
	/* create a value node for the meta-value */
	metaval = val_new_value();
	if (!metaval) {
	    return ERR_INTERNAL_MEM;
	}
	val_init_from_template(metaval, operobj);

	/* set the attribute name */
	if (val->obj->objtype==OBJ_TYP_LEAF_LIST) {
	    val_set_qname(metaval, yangid,
			  YANG_K_VALUE,
			  xml_strlen(YANG_K_VALUE));
	} else {
	    val_set_qname(metaval, yangid,
			  YANG_K_KEY,
			  xml_strlen(YANG_K_KEY));
	}

	/* set the meta variable value and other fields */
	res = val_set_simval(metaval,
			     metaval->typdef,
			     yangid, NULL,
			     edit_target);
	if (res != NO_ERR) {
	    val_free_value(metaval);
	    return res;
	} else {
	    dlq_enque(metaval, &val->metaQ);
	}
    }	  

    return NO_ERR;

} /* add_one_insert_attrs */


/********************************************************************
 * FUNCTION add_insert_attrs
 * 
 * Add the yang:insert attribute(s) to a value node
 *
 * INPUTS:
 *    val == value node to set
 *    insop == insert operation to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_insert_attrs (val_value_t *val,
		      op_insertop_t insop,
		      const xmlChar *edit_target)
{
    val_value_t          *childval;
    status_t              res;

    res = NO_ERR;

    switch (val->obj->objtype) {
    case OBJ_TYP_CHOICE:
    case OBJ_TYP_CASE:
	for (childval = val_get_first_child(val);
	     childval != NULL;
	     childval = val_get_next_child(childval)) {

	    res = add_one_insert_attrs(childval, insop,
				       edit_target);
	    if (res != NO_ERR) {
		return res;
	    }
	}
	break;
    default:
	res = add_one_insert_attrs(val, insop, edit_target);
    }

    return res;

} /* add_insert_attrs */


/********************************************************************
 * FUNCTION do_edit
 * 
 * Edit some database object on the agent
 * operation attribute:
 *   create/delete/merge/replace
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   an edit-config operation is sent to the agent
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_edit (agent_cb_t *agent_cb,
	     const obj_template_t *rpc,
	     const xmlChar *line,
	     uint32  len,
	     op_editop_t editop)
{
    val_value_t           *valset, *content, *parm, *valroot;
    status_t               res;
    uint32                 timeoutval;
    boolean                getoptional, dofill, isdelete;

    /* init locals */
    res = NO_ERR;
    content = NULL;
    valroot = NULL;
    dofill = TRUE;

    if (editop == OP_EDITOP_DELETE) {
	isdelete = TRUE;
    } else {
	isdelete = FALSE;
    }

    /* get the command line parameters for this command */
    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
	if (valset) {
	    val_free_value(valset);
	}
	return res;
    }

    /* get the timeout parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
	timeoutval = VAL_UINT(parm);
    } else {
	timeoutval = agent_cb->timeout;
    }

    /* get the 'fill-in optional parms' parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_OPTIONAL);
    if (parm && parm->res == NO_ERR) {
	getoptional = TRUE;
    } else {
	getoptional = agent_cb->get_optional;
    }

    /* get the --nofill parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_NOFILL);
    if (parm && parm->res == NO_ERR) {
	dofill = FALSE;
    }

    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(agent_cb, 
				      rpc, 
				      valset,
				      getoptional,
				      isdelete,
				      dofill,
				      TRUE,
				      &valroot);
    if (!content) {
	val_free_value(valset);
	return ERR_NCX_MISSING_PARM;
    }

    /* add nc:operation attribute to the value node */
    res = add_operation_attr(content, editop);
    if (res != NO_ERR) {
	log_error("\nError: Creation of nc:operation"
		  " attribute failed");
	val_free_value(valset);
	if (valroot) {
	    val_free_value(valroot);
	} else {
	    val_free_value(content);
	}
	return res;
    }

    /* construct an edit-config PDU with default parameters */
    res = send_edit_config_to_agent(agent_cb, 
				    valroot,
				    content, 
				    timeoutval);
    if (res != NO_ERR) {
	log_error("\nError: send %s operation failed (%s)",
		  op_editop_name(editop),
		  get_error_string(res));
    }

    val_free_value(valset);

    return res;

}  /* do_edit */


/********************************************************************
 * FUNCTION do_insert
 * 
 * Insert a database object on the agent
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   an edit-config operation is sent to the agent
 *
 * RETURNS:
 *  status
 *********************************************************************/
static status_t
    do_insert (agent_cb_t *agent_cb,
	       const obj_template_t *rpc,
	       const xmlChar *line,
	       uint32  len)
{
    val_value_t      *valset, *content, *tempval, *parm, *valroot;
    const xmlChar    *edit_target;
    op_editop_t       editop;
    op_insertop_t     insertop;
    status_t          res;
    uint32            timeoutval;
    boolean           getoptional, dofill;

    /* init locals */
    res = NO_ERR;
    content = NULL;
    valroot = NULL;
    dofill = TRUE;

    /* get the command line parameters for this command */
    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
	if (valset) {
	    val_free_value(valset);
	}
	return res;
    }

    /* get the 'fill-in optional parms' parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_OPTIONAL);
    if (parm && parm->res == NO_ERR) {
	getoptional = TRUE;
    } else {
	getoptional = agent_cb->get_optional;
    }

    /* get the --nofill parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_NOFILL);
    if (parm && parm->res == NO_ERR) {
	dofill = FALSE;
    }

    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(agent_cb, 
				      rpc, 
				      valset,
				      getoptional,
				      FALSE,
				      dofill,
				      TRUE,
				      &valroot);
    if (!content) {
	val_free_value(valset);
	return ERR_NCX_MISSING_PARM;
    }

    /* get the timeout parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
	timeoutval = VAL_UINT(parm);
    } else {
	timeoutval = agent_cb->timeout;
    }

    /* get the insert order */
    tempval = val_find_child(valset, 
			     YANGCLI_MOD,
			     YANGCLI_ORDER);
    if (tempval && tempval->res == NO_ERR) {
	insertop = op_insertop_id(VAL_ENUM_NAME(tempval));
    } else {
	insertop = OP_INSOP_LAST;
    }

    /* get the edit-config operation */
    tempval = val_find_child(valset, 
			     YANGCLI_MOD,
			     YANGCLI_OPERATION);
    if (tempval && tempval->res == NO_ERR) {
	editop = op_editop_id(VAL_ENUM_NAME(tempval));
    } else {
	editop = OP_EDITOP_MERGE;
    }

    /* get the edit-target parameter only if the
     * order is 'before' or 'after'; ignore otherwise
     */
    tempval = val_find_child(valset, 
			     YANGCLI_MOD,
			     YANGCLI_EDIT_TARGET);
    if (tempval && tempval->res == NO_ERR) {
	edit_target = VAL_STR(tempval);
    } else {
	edit_target = NULL;
    }

    /* check if the edit-target is needed */
    switch (insertop) {
    case OP_INSOP_BEFORE:
    case OP_INSOP_AFTER:
	if (!edit_target) {
	    log_error("\nError: edit-target parameter missing");
	    if (valroot) {
		val_free_value(valroot);
	    } else {
		val_free_value(content);
	    }
	    val_free_value(valset);
	    return ERR_NCX_MISSING_PARM;
	}
	break;
    default:
	;
    }

    /* add nc:operation attribute to the value node */
    res = add_operation_attr(content, editop);
    if (res != NO_ERR) {
	log_error("\nError: Creation of nc:operation"
		  " attribute failed");
    }

    /* add yang:insert attribute and possibly a key or value
     * attribute as well
     */
    if (res == NO_ERR) {
	res = add_insert_attrs(content, 
			       insertop,
			       edit_target);
	if (res != NO_ERR) {
	    log_error("\nError: Creation of yang:insert"
		  " attribute(s) failed");
	}
    }

    /* send the PDU, hand off the content node */
    if (res == NO_ERR) {
	/* construct an edit-config PDU with default parameters */
	res = send_edit_config_to_agent(agent_cb, 
					valroot,
					content, 
					timeoutval);
	if (res != NO_ERR) {
	    log_error("\nError: send create operation failed (%s)",
		      get_error_string(res));
	}
	valroot = NULL;
	content = NULL;
    }

    /* cleanup and exit */
    if (valroot) {
	val_free_value(valroot);
    } else if (content) {
	val_free_value(content);
    }

    val_free_value(valset);

    return res;

}  /* do_insert */


/********************************************************************
 * FUNCTION do_sget
 * 
 * Get some running config and/or state data with the <get> operation,
 * using an optional subtree
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   a <get> operation is sent to the agent
 *
 * RETURNS:
 *  status
 *********************************************************************/
static status_t
    do_sget (agent_cb_t *agent_cb,
	     const obj_template_t *rpc,
	     const xmlChar *line,
	     uint32  len)
{
    val_value_t           *valset, *content, *parm, *valroot;
    status_t               res;
    uint32                 timeoutval;
    boolean                dofill, getoptional;
    ncx_withdefaults_t     withdef;

    /* init locals */
    res = NO_ERR;
    content = NULL;
    valroot = NULL;
    dofill = TRUE;

    /* get the command line parameters for this command */
    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
	if (valset) {
	    val_free_value(valset);
	}
	return res;
    }

    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
	timeoutval = VAL_UINT(parm);
    } else {
	timeoutval = agent_cb->timeout;
    }

    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_NOFILL);
    if (parm && parm->res == NO_ERR) {
	dofill = FALSE;
    }

    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_OPTIONAL);
    if (parm && parm->res == NO_ERR) {
	getoptional = TRUE;
    } else {
	getoptional = agent_cb->get_optional;
    }

    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  NCX_EL_WITH_DEFAULTS);
    if (parm && parm->res == NO_ERR) {
	withdef = ncx_get_withdefaults_enum(VAL_STR(parm));
    } else {
	withdef = agent_cb->withdefaults;
    }
    
    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(agent_cb, 
				      rpc, 
				      valset,
				      getoptional,
				      FALSE,
				      dofill,
				      FALSE,
				      &valroot);
    if (!content) {
	val_free_value(valset);
	return ERR_NCX_MISSING_PARM;
    }

    /* construct a get PDU with the content as the filter */
    res = send_get_to_agent(agent_cb, 
			    valroot,
			    content, 
			    NULL, 
			    NULL, 
			    timeoutval, 
			    dofill,
			    withdef);
    if (res != NO_ERR) {
	log_error("\nError: send get operation failed (%s)",
		  get_error_string(res));
    }

    val_free_value(valset);

    return res;

}  /* do_sget */


/********************************************************************
 * FUNCTION do_sget_config
 * 
 * Get some config data with the <get-config> operation,
 * using an optional subtree
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   a <get-config> operation is sent to the agent
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_sget_config (agent_cb_t *agent_cb,
		    const obj_template_t *rpc,
		    const xmlChar *line,
		    uint32  len)
{
    val_value_t        *valset, *content, *source, *parm, *valroot;
    status_t            res;
    uint32              timeoutval;
    boolean             dofill, getoptional;
    ncx_withdefaults_t  withdef;

    dofill = TRUE;
    valroot = NULL;
    content = NULL;
    res = NO_ERR;

    /* get the command line parameters for this command */
    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
	if (valset) {
	    val_free_value(valset);
	}
	return res;
    }

    /* get the timeout parameter */
    parm = val_find_child(valset,
			  YANGCLI_MOD,
			  YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
	timeoutval = VAL_UINT(parm);
    } else {
	timeoutval = agent_cb->timeout;
    }

    /* get the'fill-in optional' parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_OPTIONAL);
    if (parm && parm->res == NO_ERR) {
	getoptional = TRUE;
    } else {
	getoptional = agent_cb->get_optional;
    }

    /* get the --nofill parameter */
    parm = val_find_child(valset,
			  YANGCLI_MOD,
			  YANGCLI_NOFILL);
    if (parm && parm->res == NO_ERR) {
	dofill = FALSE;
    }

    /* get the config source parameter */
    source = val_find_child(valset, NULL, NCX_EL_SOURCE);
    if (!source) {
	log_error("\nError: mandatory source parameter missing");
	val_free_value(valset);
	return ERR_NCX_MISSING_PARM;
    }

    /* get the with-defaults parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD,
			  NCX_EL_WITH_DEFAULTS);
    if (parm && parm->res == NO_ERR) {
	withdef = ncx_get_withdefaults_enum(VAL_STR(parm));
    } else {
	withdef = agent_cb->withdefaults;
    }

    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(agent_cb, 
				      rpc, 
				      valset,
				      getoptional,
				      FALSE,
				      dofill,
				      FALSE,
				      &valroot);
    if (!content) {
	val_free_value(valset);
	return ERR_NCX_MISSING_PARM;
    }

    /* hand off this malloced node to send_get_to_agent */
    val_remove_child(source);
    val_change_nsid(source, xmlns_nc_id());

    /* construct a get PDU with the content as the filter */
    res = send_get_to_agent(agent_cb, 
			    valroot,
			    content, 
			    NULL, 
			    source, 
			    timeoutval, 
			    dofill,
			    withdef);
    if (res != NO_ERR) {
	log_error("\nError: send get-config operation failed (%s)",
		  get_error_string(res));
    }

    val_free_value(valset);

    return res;

}  /* do_sget_config */


/********************************************************************
 * FUNCTION do_xget
 * 
 * Get some running config and/or state data with the <get> operation,
 * using an optional XPath filter
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   a <get> operation is sent to the agent
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_xget (agent_cb_t *agent_cb,
	     const obj_template_t *rpc,
	     const xmlChar *line,
	     uint32  len)
{
    const ses_cb_t      *scb;
    const mgr_scb_t     *mscb;
    val_value_t         *valset, *content, *parm, *valroot;
    const xmlChar       *str;
    status_t             res;
    uint32               retcode, timeoutval;
    ncx_withdefaults_t   withdef;

    /* init locals */
    res = NO_ERR;
    content = NULL;
    valroot = NULL;

    /* get the session info */
    scb = mgr_ses_get_scb(agent_cb->mysid);
    if (!scb) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }
    mscb = (const mgr_scb_t *)scb->mgrcb;

    /* get the command line parameters for this command */
    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
	if (valset) {
	    val_free_value(valset);
	}
	return res;
    }

    /* get the timeout parameter */
    parm = val_find_child(valset,
			  YANGCLI_MOD, 
			  YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
	timeoutval = VAL_UINT(parm);
    } else {
	timeoutval = agent_cb->timeout;
    }

    /* get the with-defaults parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  NCX_EL_WITH_DEFAULTS);
    if (parm && parm->res == NO_ERR) {
	withdef = ncx_get_withdefaults_enum(VAL_STR(parm));
    } else {
	withdef = agent_cb->withdefaults;
    }

    /* check if the agent supports :xpath */
    if (!cap_std_set(&mscb->caplist, CAP_STDID_XPATH)) {
	switch (agent_cb->baddata) {
	case NCX_BAD_DATA_IGNORE:
	    break;
	case NCX_BAD_DATA_WARN:
	    log_warn("\nWarning: agent does not have :xpath support");
	    break;
	case NCX_BAD_DATA_CHECK:
	    retcode = 0;
	    log_warn("\nWarning: agent does not have :xpath support");
	    res = get_yesno(agent_cb,
			    (const xmlChar *)"Send request anyway?",
			    YESNO_NO, &retcode);
	    if (res == NO_ERR) {
		switch (retcode) {
		case YESNO_CANCEL:
		case YESNO_NO:
		    res = ERR_NCX_CANCELED;
		    break;
		case YESNO_YES:
		    break;
		default:
		    res = SET_ERROR(ERR_INTERNAL_VAL);
		}
	    }
	    break;
	case NCX_BAD_DATA_ERROR:
	    log_error("\nError: agent does not have :xpath support");
	    res = ERR_NCX_OPERATION_NOT_SUPPORTED;
	    break;
	case NCX_BAD_DATA_NONE:
	default:
	    res = SET_ERROR(ERR_INTERNAL_VAL);
	}
    }

    /* check any error so far */
    if (res != NO_ERR) {
	if (valset) {
	    val_free_value(valset);
	}
	return res;
    }

    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(agent_cb, 
				      rpc, 
				      valset,
				      FALSE,
				      FALSE,
				      FALSE,
				      FALSE,
				      &valroot);
    if (content) {
	if (content->btyp == NCX_BT_STRING && VAL_STR(content)) {
	    str = VAL_STR(content);
	    while (*str && *str != '"') {
		str++;
	    }
	    if (*str) {
		log_error("\nError: select string cannot "
			  "contain a double quote");
	    } else {
		/* construct a get PDU with the content
		 * as the filter 
                 * !! passing off content memory even on error !!
		 */
		res = send_get_to_agent(agent_cb, 
					valroot,
					NULL, 
					content, 
					NULL, 
					timeoutval,
					FALSE,
					withdef);
                content = NULL;
		if (res != NO_ERR) {
		    log_error("\nError: send get operation"
			      " failed (%s)",
			      get_error_string(res));
		}
	    }
	} else {
	    log_error("\nError: xget content wrong type");
	}
	if (valroot) {
	    /* should not happen */
	    val_free_value(valroot);
	}
        if (content) {
	    val_free_value(content);
	}
    }

    val_free_value(valset);

    return res;

}  /* do_xget */


/********************************************************************
 * FUNCTION do_xget_config
 * 
 * Get some config data with the <get-config> operation,
 * using an optional XPath filter
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the create command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * OUTPUTS:
 *   the completed data node is output and
 *   a <get-config> operation is sent to the agent
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_xget_config (agent_cb_t *agent_cb,
		    const obj_template_t *rpc,
		    const xmlChar *line,
		    uint32  len)
{
    const ses_cb_t      *scb;
    const mgr_scb_t     *mscb;
    val_value_t         *valset, *content, *source, *parm, *valroot;
    const xmlChar       *str;
    status_t             res;
    uint32               retcode, timeoutval;
    ncx_withdefaults_t   withdef;

    /* init locals */
    res = NO_ERR;
    content = NULL;
    valroot = NULL;

    /* get the session info */
    scb = mgr_ses_get_scb(agent_cb->mysid);
    if (!scb) {
	return SET_ERROR(ERR_INTERNAL_VAL);
    }
    mscb = (const mgr_scb_t *)scb->mgrcb;
      
    /* get the command line parameters for this command */
    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (!valset || res != NO_ERR) {
	if (valset) {
	    val_free_value(valset);
	}
	return res;
    }

    /* get the source parameter */
    source = val_find_child(valset, NULL, NCX_EL_SOURCE);
    if (!source) {
	log_error("\nError: mandatory source parameter missing");
	val_free_value(valset);
	return ERR_NCX_MISSING_PARM;
    }

    /* get the with-defaults parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  NCX_EL_WITH_DEFAULTS);
    if (parm && parm->res == NO_ERR) {
	withdef = ncx_get_withdefaults_enum(VAL_STR(parm));
    } else {
	withdef = agent_cb->withdefaults;
    }

    /* get the timeout parameter */
    parm = val_find_child(valset, 
			  YANGCLI_MOD, 
			  YANGCLI_TIMEOUT);
    if (parm && parm->res == NO_ERR) {
	timeoutval = VAL_UINT(parm);
    } else {
	timeoutval = agent_cb->timeout;
    }

    /* check if the agent supports :xpath */
    if (!cap_std_set(&mscb->caplist, CAP_STDID_XPATH)) {
	switch (agent_cb->baddata) {
	case NCX_BAD_DATA_IGNORE:
	    break;
	case NCX_BAD_DATA_WARN:
	    log_warn("\nWarning: agent does not have :xpath support");
	    break;
	case NCX_BAD_DATA_CHECK:
	    retcode = 0;
	    log_warn("\nWarning: agent does not have :xpath support");
	    res = get_yesno(agent_cb,
			    (const xmlChar *)"Send request anyway?",
			    YESNO_NO, &retcode);
	    if (res == NO_ERR) {
		switch (retcode) {
		case YESNO_CANCEL:
		case YESNO_NO:
		    res = ERR_NCX_CANCELED;
		    break;
		case YESNO_YES:
		    break;
		default:
		    res = SET_ERROR(ERR_INTERNAL_VAL);
		}
	    }
	    break;
	case NCX_BAD_DATA_ERROR:
	    log_error("\nError: agent does not have :xpath support");
	    res = ERR_NCX_OPERATION_NOT_SUPPORTED;
	    break;
	case NCX_BAD_DATA_NONE:
	default:
	    res = SET_ERROR(ERR_INTERNAL_VAL);
	}
    }

    /* check any error so far */
    if (res != NO_ERR) {
	if (valset) {
	    val_free_value(valset);
	}
	return res;
    }

    /* get the contents specified in the 'from' choice */
    content = get_content_from_choice(agent_cb, 
				      rpc, 
				      valset,
				      FALSE,
				      FALSE,
				      FALSE,
				      FALSE,
				      &valroot);
    if (content) {
	if (content->btyp == NCX_BT_STRING && VAL_STR(content)) {
	    str = VAL_STR(content);
	    while (*str && *str != '"') {
		str++;
	    }
	    if (*str) {
		log_error("\nError: select string cannot "
			  "contain a double quote");
	    } else {
		/* hand off this malloced node to send_get_to_agent */
		val_remove_child(source);
		val_change_nsid(source, xmlns_nc_id());

		/* construct a get PDU with the content as the filter
                 * !! hand off content memory here, even on error !! 
                 */
		res = send_get_to_agent(agent_cb, 
					valroot,
					NULL, 
					content, 
					source,
					timeoutval,
					FALSE,
					withdef);
                content = NULL;
		if (res != NO_ERR) {
		    log_error("\nError: send get-config "
			      "operation failed (%s)",
			      get_error_string(res));
		}
	    }
	} else {
	    log_error("\nError: xget content wrong type");
	}
	if (valroot) {
	    /* should not happen */
	    val_free_value(valroot);
	}
        if (content) {
	    val_free_value(content);
	}
    }

    val_free_value(valset);

    return res;

}  /* do_xget_config */


/********************************************************************
 * FUNCTION do_history_show (sub-mode of history RPC)
 * 
 * history show [-1]
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    maxlines == max number of history entries to show
 *    mode == requested help mode
 * 
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_history_show (agent_cb_t *agent_cb,
                     int maxlines,
                     help_mode_t mode)
{
    FILE          *outputfile;
    const char    *format;
    int            glstatus;


    outputfile = log_get_logfile();
    if (!outputfile) {
        /* use STDOUT instead */
        outputfile = stdout;
    }
    
    if (mode == HELP_MODE_FULL) {
        format = "\n  [%N]\t%D %T %H";
    } else {
        format = "\n  [%N]\t%H";
    }

    glstatus = gl_show_history(agent_cb->cli_gl,
                               outputfile,
                               format,
                               1,
                               maxlines);

    fputc('\n', outputfile);

    return NO_ERR;

} /* do_history_show */


/********************************************************************
 * FUNCTION do_history_clear (sub-mode of history RPC)
 * 
 * history clear 
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 * 
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_history_clear (agent_cb_t *agent_cb)
{

    gl_clear_history(agent_cb->cli_gl, 1);
    return NO_ERR;

} /* do_history_clear */


/********************************************************************
 * FUNCTION do_history_load (sub-mode of history RPC)
 * 
 * history load
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    fname == filename parameter value
 *    if missing then try the previous history file name
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_history_load (agent_cb_t *agent_cb,
                     const xmlChar *fname)
{
    status_t   res;
    int        glstatus;

    res = NO_ERR;

    if (fname && *fname) {
        if (agent_cb->history_filename) {
            m__free(agent_cb->history_filename);
        }
        agent_cb->history_filename = xml_strdup(fname);
        if (!agent_cb->history_filename) {
            res = ERR_INTERNAL_MEM;
        }
    } else if (agent_cb->history_filename) {
        fname = agent_cb->history_filename;
    } else {
        res = ERR_NCX_INVALID_VALUE;
    }

    if (res == NO_ERR) {
        glstatus = gl_load_history(agent_cb->cli_gl,
                                   (const char *)fname,
                                   "#");   /* comment prefix */
        if (glstatus) {
            res = ERR_NCX_OPERATION_FAILED;
        }
    }

    return res;

} /* do_history_load */


/********************************************************************
 * FUNCTION do_history_save (sub-mode of history RPC)
 * 
 * history save
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    fname == filename parameter value
 *    if missing then try the previous history file name
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_history_save (agent_cb_t *agent_cb,
                     const xmlChar *fname)
{
    status_t   res;
    int        glstatus;

    res = NO_ERR;

    if (fname && *fname) {
        if (agent_cb->history_filename) {
            m__free(agent_cb->history_filename);
        }
        agent_cb->history_filename = xml_strdup(fname);
        if (!agent_cb->history_filename) {
            res = ERR_INTERNAL_MEM;
        }
    } else if (agent_cb->history_filename) {
        fname = agent_cb->history_filename;
    } else {
        res = ERR_NCX_INVALID_VALUE;
    }

    if (res == NO_ERR) {
        glstatus = gl_save_history(agent_cb->cli_gl,
                                   (const char *)fname,
                                   "#",   /* comment prefix */
                                   -1);    /* save all entries */
        if (glstatus) {
            res = ERR_NCX_OPERATION_FAILED;
        }
    }

    return res;

} /* do_history_save */


/********************************************************************
 * FUNCTION do_history (local RPC)
 * 
 * Do Command line history support operations
 *
 * history 
 *      show
 *      clear
 *      load
 *      save
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the history command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_history (agent_cb_t *agent_cb,
                const obj_template_t *rpc,
                const xmlChar *line,
                uint32  len)
{
    val_value_t        *valset, *parm;
    status_t            res;
    boolean             imode, done;
    help_mode_t         mode;

    done = FALSE;
    res = NO_ERR;
    imode = interactive_mode();

    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (valset && res == NO_ERR) {
	mode = HELP_MODE_NORMAL;

	/* check if the 'brief' flag is set first */
	parm = val_find_child(valset, 
			      YANGCLI_MOD, 
			      YANGCLI_BRIEF);
	if (parm && parm->res == NO_ERR) {
	    mode = HELP_MODE_BRIEF;
	} else {
            /* check if the 'full' flag is set first */
	    parm = val_find_child(valset, 
				  YANGCLI_MOD, 
				  YANGCLI_FULL);
	    if (parm && parm->res == NO_ERR) {
		mode = HELP_MODE_FULL;
	    }
	}

	/* find the 1 of N choice */
        parm = val_find_child(valset, 
                              YANGCLI_MOD, 
                              YANGCLI_SHOW);
        if (parm) {
            /* do show history */
            res = do_history_show(agent_cb,
                                  VAL_INT(parm), 
                                  mode);
            done = TRUE;
        }

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD, 
				  YANGCLI_CLEAR);
	    if (parm) {
		/* do clear history */
		res = do_history_clear(agent_cb);
		done = TRUE;
                if (res == NO_ERR) {
                    log_info("\nOK\n");
                } else {
                    log_error("\nError: clear history failed\n");
                }
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD, 
				  YANGCLI_LOAD);
	    if (parm) {
                if (!VAL_STR(parm) || !*VAL_STR(parm)) {
                    /* do history load buffer: default filename */
                    res = do_history_load(agent_cb,
                                          YANGCLI_DEF_HISTORY_FILE);
                } else {
                    /* do history load buffer */
                    res = do_history_load(agent_cb,
                                          VAL_STR(parm));
                }
		done = TRUE;
                if (res == NO_ERR) {
                    log_info("\nOK\n");
                } else {
                    log_error("\nError: load history failed\n");
                }
	    }
	}

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD, 
				  YANGCLI_SAVE);
	    if (parm) {
                if (!VAL_STR(parm) || !*VAL_STR(parm)) {
                    /* do history save buffer: default filename */
                    res = do_history_save(agent_cb,
                                          YANGCLI_DEF_HISTORY_FILE);
                } else {
                    /* do history save buffer */
                    res = do_history_save(agent_cb,
                                          VAL_STR(parm));
                }
		done = TRUE;
                if (res == NO_ERR) {
                    log_info("\nOK\n");
                } else {
                    log_error("\nError: save history failed\n");
                }
	    }
	}

        if (!done) {
            res = do_history_show(agent_cb, -1, mode);
        }
    }

    if (valset) {
	val_free_value(valset);
    }

    return res;

}  /* do_history */


/********************************************************************
 * FUNCTION do_line_recall (execute the recall local RPC)
 * 
 * recall n 
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    num == entry number of history entry entry to recall
 * 
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_line_recall (agent_cb_t *agent_cb,
                    unsigned long num)
{
    GlHistoryLine   history_line;
    int             glstatus;

    agent_cb->history_line_active = FALSE;
    memset(&history_line, 0x0, sizeof(GlHistoryLine));
    glstatus = gl_lookup_history(agent_cb->cli_gl,
                                 num,
                                 &history_line);

    if (glstatus == 0) {
        log_error("\nError: lookup command line history failed");
        return ERR_NCX_OPERATION_FAILED; 
    }

    if (agent_cb->history_line) {
        m__free(agent_cb->history_line);
    }

    /* save the line in the agent_cb for next call
     * to get_line
     */

    agent_cb->history_line = 
        xml_strdup((const xmlChar *)history_line.line);
    if (!agent_cb->history_line) {
        return ERR_INTERNAL_MEM;
    }
    agent_cb->history_line_active = TRUE;

    return NO_ERR;

} /* do_line_recall */


/********************************************************************
 * FUNCTION do_recall (local RPC)
 * 
 * Do Command line history support operations
 *
 * recall index=n
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the history command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_recall (agent_cb_t *agent_cb,
               const obj_template_t *rpc,
               const xmlChar *line,
               uint32  len)
{
    val_value_t        *valset, *parm;
    status_t            res;
    boolean             imode, done;
    help_mode_t         mode;

    done = FALSE;
    res = NO_ERR;
    imode = interactive_mode();

    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (valset && res == NO_ERR) {
	mode = HELP_MODE_NORMAL;

	/* find the mandatory index */
        parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_INDEX);
        if (parm) {
            /* do show history */
            res = do_line_recall(agent_cb, VAL_UINT(parm));
	} else {
            res = ERR_NCX_MISSING_PARM;
            log_error("\nError: missing index parameter");
        }
    }

    if (valset) {
	val_free_value(valset);
    }

    return res;

}  /* do_recall */


/********************************************************************
 * FUNCTION do_eventlog_show (sub-mode of eventlog RPC)
 * 
 * eventlog show=-1 start=0 full
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    maxevents == max number of eventlog entries to show
 *    startindex == start event index number to show
 *    mode == requested help mode
 * 
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_eventlog_show (agent_cb_t *agent_cb,
                      int32 maxevents,
                      uint32 startindex,
                      help_mode_t mode)
{
    mgr_not_msg_t  *notif;
    val_value_t    *sequence_id;
    logfn_t         logfn;
    boolean         done, imode;
    uint32          eventindex, maxindex, eventsdone;

    if (maxevents > 0) {
        maxindex = (uint32)maxevents;
    } else if (maxevents == -1) {
        maxindex = 0;
    } else {
        return ERR_NCX_INVALID_VALUE;
    }

    imode = interactive_mode();
    if (imode) {
	logfn = log_stdout;
    } else {
	logfn = log_write;
    }

    done = FALSE;
    eventindex = 0;
    eventsdone = 0;

    for (notif = (mgr_not_msg_t *)
             dlq_firstEntry(&agent_cb->notificationQ);
         notif != NULL && !done;
         notif = (mgr_not_msg_t *)dlq_nextEntry(notif)) {

        if (eventindex >= startindex) {

            sequence_id = val_find_child(notif->notification,
                                         NULL,
                                         NCX_EL_SEQUENCE_ID);


            (*logfn)("\n [%u]\t", eventindex);
            if (mode != HELP_MODE_BRIEF && notif->eventTime) {
                (*logfn)(" [%s] ", VAL_STR(notif->eventTime));
            }

            if (mode != HELP_MODE_BRIEF) {
                if (sequence_id) {
                    (*logfn)("(%u)\t", VAL_UINT(sequence_id));
                } else {
                    (*logfn)("(--)\t");
                }
            }

            /* print the eventType in the desired format */
            if (notif->eventType) {
                switch (agent_cb->display_mode) {
                case NCX_DISPLAY_MODE_PLAIN:
                    (*logfn)("<%s>", notif->eventType->name);
                    break;
                case NCX_DISPLAY_MODE_PREFIX:
                case NCX_DISPLAY_MODE_XML:
                    (*logfn)("<%s:%s>", 
                             val_get_mod_prefix(notif->eventType),
                             notif->eventType->name);
                    break;
                case NCX_DISPLAY_MODE_MODULE:
                    (*logfn)("<%s:%s>", 
                             val_get_mod_name(notif->eventType),
                             notif->eventType->name);
                    break;
                default:
                    SET_ERROR(ERR_INTERNAL_VAL);
                }
            } else {
                (*logfn)("<>");
            }

            if (mode == HELP_MODE_FULL) {
                if (imode) {
                    val_stdout_value_ex(notif->notification,
                                        NCX_DEF_INDENT,
                                        agent_cb->display_mode);
                } else {
                    val_dump_value_ex(notif->notification,
                                      NCX_DEF_INDENT,
                                      agent_cb->display_mode);
                }
                (*logfn)("\n");
            }
            eventsdone++;
        }

        if (maxindex && (eventsdone == maxindex)) {
            done = TRUE;
        }

        eventindex++;
    }

    if (eventsdone) {
        (*logfn)("\n");
    }

    return NO_ERR;

} /* do_eventlog_show */


/********************************************************************
 * FUNCTION do_eventlog_clear (sub-mode of eventlog RPC)
 * 
 * eventlog clear
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    maxevents == max number of events to clear
 *                 zero means clear all
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_eventlog_clear (agent_cb_t *agent_cb,
                       int32 maxevents)
{
    mgr_not_msg_t  *notif;
    int32           eventcount;

    if (maxevents > 0) {
        eventcount = 0;
        while (!dlq_empty(&agent_cb->notificationQ) &&
               eventcount < maxevents) {
            notif = (mgr_not_msg_t *)
                dlq_deque(&agent_cb->notificationQ);
            mgr_not_free_msg(notif);
            eventcount++;
        }
    } else if (maxevents == -1) {
        while (!dlq_empty(&agent_cb->notificationQ)) {
            notif = (mgr_not_msg_t *)
                dlq_deque(&agent_cb->notificationQ);
            mgr_not_free_msg(notif);
        }
    } else {
        return ERR_NCX_INVALID_VALUE;
    }

    return NO_ERR;

} /* do_eventlog_clear */


/********************************************************************
 * FUNCTION do_eventlog (local RPC)
 * 
 * Do Notification Event Log support operations
 *
 * eventlog 
 *      show
 *      clear
 *
 * INPUTS:
 *    agent_cb == agent control block to use
 *    rpc == RPC method for the history command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    do_eventlog (agent_cb_t *agent_cb,
                 const obj_template_t *rpc,
                 const xmlChar *line,
                 uint32  len)
{
    val_value_t        *valset, *parm, *start;
    status_t            res;
    boolean             imode, done;
    help_mode_t         mode;

    done = FALSE;
    res = NO_ERR;
    imode = interactive_mode();

    valset = get_valset(agent_cb, rpc, &line[len], &res);
    if (valset && res == NO_ERR) {
	mode = HELP_MODE_NORMAL;

	/* check if the 'brief' flag is set first */
	parm = val_find_child(valset, 
			      YANGCLI_MOD, 
			      YANGCLI_BRIEF);
	if (parm && parm->res == NO_ERR) {
	    mode = HELP_MODE_BRIEF;
	} else {
            /* check if the 'full' flag is set first */
	    parm = val_find_child(valset, 
				  YANGCLI_MOD, 
				  YANGCLI_FULL);
	    if (parm && parm->res == NO_ERR) {
		mode = HELP_MODE_FULL;
	    }
	}

        /* optional start position for show */
        start = val_find_child(valset, 
                               YANGCLI_MOD, 
                               YANGCLI_START);

	/* find the 1 of N choice */
        parm = val_find_child(valset, 
                              YANGCLI_MOD, 
                              YANGCLI_SHOW);
        if (parm) {
            /* do show eventlog  */
            res = do_eventlog_show(agent_cb,
                                   VAL_INT(parm),
                                   (start) ? VAL_UINT(start) : 0,
                                   mode);
            done = TRUE;
        }

	if (!done) {
	    parm = val_find_child(valset, 
				  YANGCLI_MOD, 
				  YANGCLI_CLEAR);
	    if (parm) {
		/* do clear event log */
		res = do_eventlog_clear(agent_cb,
                                        VAL_INT(parm));
		done = TRUE;
                if (res == NO_ERR) {
                    log_info("\nOK\n");
                } else {
                    log_error("\nError: clear event log failed\n");
                }
	    }
	}

        if (!done) {
            res = do_eventlog_show(agent_cb, 
                                   -1, 
                                   (start) ? VAL_UINT(start) : 0, 
                                   mode);
        }
    }

    if (valset) {
	val_free_value(valset);
    }

    return res;

}  /* do_eventlog */


/********************************************************************
* FUNCTION do_local_conn_command
* 
* Handle local connection mode RPC operations from yangcli.yang
*
* INPUTS:
*   agent_cb == agent control block to use
*   rpc == template for the local RPC
*   line == input command line from user
*   len == line length
*
* OUTPUTS:
*    agent_cb->state may be changed or other action taken
*    the line buffer is NOT consumed or freed by this function
*
* RETURNS:
*    NO_ERR if a RPC was executed OK
*    ERR_NCX_SKIPPED if no command was invoked
*    some other error if command execution failed
*********************************************************************/
static status_t
    do_local_conn_command (agent_cb_t *agent_cb,
			   const obj_template_t *rpc,
			   xmlChar *line,
			   uint32  len)
{
    const xmlChar *rpcname;
    status_t       res;

    res = NO_ERR;
    rpcname = obj_get_name(rpc);

    if (!xml_strcmp(rpcname, YANGCLI_CREATE)) {
	res = do_edit(agent_cb, rpc, line, len, OP_EDITOP_CREATE);
    } else if (!xml_strcmp(rpcname, YANGCLI_DELETE)) {
	res = do_edit(agent_cb, rpc, line, len, OP_EDITOP_DELETE);
    } else if (!xml_strcmp(rpcname, YANGCLI_INSERT)) {
	res = do_insert(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_MERGE)) {
	res = do_edit(agent_cb, rpc, line, len, OP_EDITOP_MERGE);
    } else if (!xml_strcmp(rpcname, YANGCLI_REPLACE)) {
	res = do_edit(agent_cb, rpc, line, len, OP_EDITOP_REPLACE);
    } else if (!xml_strcmp(rpcname, YANGCLI_SAVE)) {
	if (len < xml_strlen(line)) {
	    res = ERR_NCX_INVALID_VALUE;
	    log_error("\nError: Extra characters found (%s)",
		      &line[len]);
	} else {
	    res = do_save(agent_cb);
	}
    } else if (!xml_strcmp(rpcname, YANGCLI_SGET)) {
	res = do_sget(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_SGET_CONFIG)) {
	res = do_sget_config(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_XGET)) {
	res = do_xget(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_XGET_CONFIG)) {
	res = do_xget_config(agent_cb, rpc, line, len);
    } else {
	res = ERR_NCX_SKIPPED;
    }
    return res;

} /* do_local_conn_command */


/********************************************************************
* FUNCTION do_local_command
* 
* Handle local RPC operations from yangcli.yang
*
* INPUTS:
*   agent_cb == agent control block to use
*   rpc == template for the local RPC
*   line == input command line from user
*   len == length of line in bytes
*
* OUTPUTS:
*    state may be changed or other action taken
*    the line buffer is NOT consumed or freed by this function
*
* RETURNS:
*  status
*********************************************************************/
static status_t
    do_local_command (agent_cb_t *agent_cb,
		      const obj_template_t *rpc,
		      xmlChar *line,
		      uint32  len)
{
    const xmlChar *rpcname;
    status_t       res;

    res = NO_ERR;
    rpcname = obj_get_name(rpc);

    if (!xml_strcmp(rpcname, YANGCLI_CD)) {
	res = do_cd(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_CONNECT)) {
	res = do_connect(agent_cb, rpc, line, len, FALSE);
    } else if (!xml_strcmp(rpcname, YANGCLI_EVENTLOG)) {
	res = do_eventlog(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_FILL)) {
	res = do_fill(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_HELP)) {
	res = do_help(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_HISTORY)) {
	res = do_history(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_LIST)) {
	res = do_list(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_MGRLOAD)) {
	res = do_mgrload(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_PWD)) {
	res = do_pwd(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_QUIT)) {
	agent_cb->state = MGR_IO_ST_SHUT;
	mgr_request_shutdown();
    } else if (!xml_strcmp(rpcname, YANGCLI_RECALL)) {
	res = do_recall(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_RUN)) {
	res = do_run(agent_cb, rpc, line, len);
    } else if (!xml_strcmp(rpcname, YANGCLI_SHOW)) {
	res = do_show(agent_cb, rpc, line, len);
    } else {
	res = ERR_NCX_INVALID_VALUE;
	log_error("\nError: The %s command is not allowed in this mode",
		   rpcname);
    }

    return res;

} /* do_local_command */


/********************************************************************
* FUNCTION top_command
* 
* Top-level command handler
*
* INPUTS:
*   agent_cb == agent control block to use
*   line == input command line from user
*
* OUTPUTS:
*    state may be changed or other action taken
*    the line buffer is NOT consumed or freed by this function
*
* RETURNS:
*   status
*********************************************************************/
status_t
    top_command (agent_cb_t *agent_cb,
		 xmlChar *line)
{
    const obj_template_t  *rpc;
    uint32                 len;
    ncx_node_t             dtyp;
    status_t               res;

#ifdef DEBUG
    if (!agent_cb || !line) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    res = NO_ERR;

    if (!xml_strlen(line)) {
	return res;
    }

    dtyp = NCX_NT_OBJ;
    rpc = (const obj_template_t *)parse_def(agent_cb,
					    &dtyp, 
					    line, 
					    &len);
    if (!rpc) {
	if (agent_cb->result_name || agent_cb->result_filename) {
	    res = finish_result_assign(agent_cb, NULL, line);
	} else {
	    res = ERR_NCX_INVALID_VALUE;
	    /* this is an unknown command */
	    log_error("\nError: Unrecognized command");
	}
	return res;
    }

    /* check  handful of yangcli commands */
    if (is_yangcli_ns(obj_get_nsid(rpc))) {
	res = do_local_command(agent_cb, rpc, line, len);
    } else {
	res = ERR_NCX_OPERATION_FAILED;
	log_error("\nError: Not connected to agent."
		  "\nLocal commands only in this mode.");
    }

    return res;

} /* top_command */


/********************************************************************
* FUNCTION conn_command
* 
* Connection level command handler
*
* INPUTS:
*   agent_cb == agent control block to use
*   line == input command line from user
*
* OUTPUTS:
*    state may be changed or other action taken
*    the line buffer is NOT consumed or freed by this function
*
* RETURNS:
*   status
*********************************************************************/
status_t
    conn_command (agent_cb_t *agent_cb,
		  xmlChar *line)
{
    const obj_template_t  *rpc, *input;
    mgr_rpc_req_t         *req;
    val_value_t           *reqdata, *valset, *parm;
    ses_cb_t              *scb;
    uint32                 len, linelen;
    status_t               res;
    boolean                shut, load;
    ncx_node_t             dtyp;

#ifdef DEBUG
    if (!agent_cb || !line) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    req = NULL;
    reqdata = NULL;
    valset = NULL;
    res = NO_ERR;
    shut = FALSE;
    load = FALSE;

    /* make sure there is something to parse */
    linelen = xml_strlen(line);
    if (!linelen) {
	return res;
    }

    /* get the RPC method template */
    dtyp = NCX_NT_OBJ;
    rpc = (const obj_template_t *)parse_def(agent_cb,
					    &dtyp, 
                                            line, 
                                            &len);
    if (!rpc) {
	if (agent_cb->result_name || agent_cb->result_filename) {
	    res = finish_result_assign(agent_cb, NULL, line);
	} else {
	    res = ERR_NCX_DEF_NOT_FOUND;
	    /* this is an unknown command */
	    log_stdout("\nUnrecognized command");
	}
	return res;
    }

    /* check local commands */
    if (is_yangcli_ns(obj_get_nsid(rpc))) {
	if (!xml_strcmp(obj_get_name(rpc), YANGCLI_CONNECT)) {
	    res = ERR_NCX_OPERATION_FAILED;
	    log_stdout("\nError: Already connected");
	} else {
	    res = do_local_conn_command(agent_cb, rpc, line, len);
	    if (res == ERR_NCX_SKIPPED) {
		res = do_local_command(agent_cb, rpc, line, len);
	    }
	}
	return res;
    }

    /* else treat this as an RPC request going to the agent
     * first construct a method + parameter tree 
     */
    reqdata = xml_val_new_struct(obj_get_name(rpc), 
				 obj_get_nsid(rpc));
    if (!reqdata) {
	log_error("\nError allocating a new RPC request");
	res = ERR_INTERNAL_MEM;
    }

    /* should find an input node */
    input = obj_find_child(rpc, NULL, YANG_K_INPUT);

    /* check if any params are expected */
    if (res == NO_ERR && input && obj_get_child_count(input)) {
	while (line[len] && xml_isspace(line[len])) {
	    len++;
	}

	if (len < linelen) {
	    valset = parse_rpc_cli(rpc, &line[len], &res);
	    if (res != NO_ERR) {
		log_error("\nError in the parameters for RPC %s (%s)",
			  obj_get_name(rpc), get_error_string(res));
	    }
	}

	/* check no input from user, so start a parmset */
	if (res == NO_ERR && !valset) {
	    valset = val_new_value();
	    if (!valset) {
		res = ERR_INTERNAL_MEM;
	    } else {
		val_init_from_template(valset, input);
	    }
	}

	/* fill in any missing parameters from the CLI */
	if (res == NO_ERR) {
	    if (interactive_mode()) {
		res = fill_valset(agent_cb, rpc, valset, NULL);
		if (res == ERR_NCX_SKIPPED) {
		    res = NO_ERR;
		}
	    }
	}

	/* go through the parm list and move the values 
	 * to the reqdata struct. 
	 */
	if (res == NO_ERR) {
	    parm = val_get_first_child(valset);
	    while (parm) {
		val_remove_child(parm);
		val_add_child(parm, reqdata);
		parm = val_get_first_child(valset);
	    }
	}
    }

    /* check the close-session corner cases */
    if (res == NO_ERR && !xml_strcmp(obj_get_name(rpc), 
				     NCX_EL_CLOSE_SESSION)) {
	shut = TRUE;
    }
	    
    /* allocate an RPC request and send it */
    if (res == NO_ERR) {
	scb = mgr_ses_get_scb(agent_cb->mysid);
	if (!scb) {
	    res = SET_ERROR(ERR_INTERNAL_PTR);
	} else {
	    req = mgr_rpc_new_request(scb);
	    if (!req) {
		res = ERR_INTERNAL_MEM;
		log_error("\nError allocating a new RPC request");
	    } else {
		req->data = reqdata;
		req->rpc = rpc;
		req->timeout = agent_cb->timeout;
	    }
	}
	
	if (res == NO_ERR) {
	    if (LOGDEBUG2) {
		log_debug2("\nabout to send RPC request with reqdata:");
		val_dump_value_ex(reqdata, 
                                  NCX_DEF_INDENT,
                                  agent_cb->display_mode);
	    }

	    /* the request will be stored if this returns NO_ERR */
	    res = mgr_rpc_send_request(scb, req, yangcli_reply_handler);
	}
    }

    if (valset) {
	val_free_value(valset);
    }

    if (res != NO_ERR) {
	if (req) {
	    mgr_rpc_free_request(req);
	} else if (reqdata) {
	    val_free_value(reqdata);
	}
    } else if (shut) {
	agent_cb->state = MGR_IO_ST_CONN_CLOSEWAIT;
    } else {
	agent_cb->state = MGR_IO_ST_CONN_RPYWAIT;
    }

    return res;

} /* conn_command */


/********************************************************************
 * FUNCTION do_startup_script
 * 
 * Process run-script CLI parameter
 *
 * INPUTS:
 *   agent_cb == agent control block to use
 *   runscript == name of the script to run (could have path)
 *
 * SIDE EFFECTS:
 *   runstack start with the runscript script if no errors
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_startup_script (agent_cb_t *agent_cb,
                       const xmlChar *runscript)
{
    const obj_template_t *rpc;
    xmlChar              *line, *p;
    status_t              res;
    uint32                linelen;

#ifdef DEBUG
    if (!agent_cb || !runscript) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    if (!*runscript) {
	return ERR_NCX_INVALID_VALUE;
    }

    /* get the 'run' RPC method template */
    rpc = ncx_find_object(get_yangcli_mod(), YANGCLI_RUN);
    if (!rpc) {
	return ERR_NCX_DEF_NOT_FOUND;
    }

    /* create a dummy command line 'script <runscipt-text>' */
    linelen = xml_strlen(runscript) + xml_strlen(NCX_EL_SCRIPT) + 1;
    line = m__getMem(linelen+1);
    if (!line) {
	return ERR_INTERNAL_MEM;
    }
    p = line;
    p += xml_strcpy(p, NCX_EL_SCRIPT);
    *p++ = ' ';
    xml_strcpy(p, runscript);

    if (LOGDEBUG) {
        log_debug("\nBegin startup script '%s'",  runscript);
    }

    /* fill in the value set for the input parameters */
    res = do_run(agent_cb, rpc, line, 0);

    m__free(line);

    return res;

}  /* do_startup_script */


/********************************************************************
 * FUNCTION do_startup_command
 * 
 * Process run-command CLI parameter
 *
 * INPUTS:
 *   agent_cb == agent control block to use
 *   runcommand == command string to run
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_startup_command (agent_cb_t *agent_cb,
                        const xmlChar *runcommand)
{
    xmlChar        *copystring;
    status_t        res;

#ifdef DEBUG
    if (!agent_cb || !runcommand) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    if (!*runcommand) {
	return ERR_NCX_INVALID_VALUE;
    }

    if (xml_strlen(runcommand) > YANGCLI_LINELEN) {
        return ERR_NCX_RESOURCE_DENIED;
    }

    /* the top_command and conn_command functions
     * expect this buffer to be wriable
     */
    copystring = xml_strdup(runcommand);
    if (!copystring) {
        return ERR_INTERNAL_MEM;
    }

    if (LOGDEBUG) {
        log_debug("\nBegin startup command '%s'",  copystring);
    }

    /* only invoke the command in idle or connection idle states */
    switch (agent_cb->state) {
    case MGR_IO_ST_IDLE:
        res = top_command(agent_cb, copystring);
        break;
    case MGR_IO_ST_CONN_IDLE:
        res = conn_command(agent_cb, copystring);
        break;
    default:
        res = ERR_NCX_OPERATION_FAILED;
    }

    m__free(copystring);
    return res;

}  /* do_startup_command */


/********************************************************************
* FUNCTION get_cmd_line
* 
*  Read the current runstack context and construct
*  a command string for processing by do_run.
*    - Extended lines will be concatenated in the
*      buffer.  If a buffer overflow occurs due to this
*      concatenation, an error will be returned
* 
* INPUTS:
*   agent_cb == agent control block to use
*   res == address of status result
*
* OUTPUTS:
*   *res == function result status
*
* RETURNS:
*   pointer to the command line to process (should treat as CONST !!!)
*   NULL if some error
*********************************************************************/
xmlChar *
    get_cmd_line (agent_cb_t *agent_cb,
		  status_t *res)
{
    xmlChar        *start, *str, *clibuff;
    boolean         done;
    int             len, total, maxlen;

#ifdef DEBUG
    if (!agent_cb || !res) {
	SET_ERROR(ERR_INTERNAL_PTR);
	return NULL;
    }
#endif

    /* init locals */
    clibuff = agent_cb->clibuff;
    total = 0;
    str = NULL;
    maxlen = YANGCLI_BUFFLEN;
    done = FALSE;
    start = clibuff;

    /* get a command line, handling comment and continuation lines */
    while (!done) {

	/* read the next line from the user */
	str = get_line(agent_cb);
	if (!str) {
	    *res = ERR_NCX_READ_FAILED;
	    done = TRUE;
	    continue;
	}

	/* find end of string */
	len = xml_strlen(str);
	
	/* get rid of EOLN if present */
	if (len && str[len-1]=='\n') {
	    str[--len] = 0;
	}

	/* check line continuation */
	if (len && str[len-1]=='\\') {
	    /* get rid of the final backslash */
	    str[--len] = 0;
	    agent_cb->climore = TRUE;
	} else {
	    /* done getting lines */
	    *res = NO_ERR;
	    done = TRUE;
	}

	/* copy the string to the clibuff */
	if (total + len < maxlen) {
	    xml_strcpy(start, str);
	    start += len;
	    total += len;
	} else {
	    *res = ERR_BUFF_OVFL;
	    done = TRUE;
	}
	    
	str = NULL;
    }

    agent_cb->climore = FALSE;
    if (*res == NO_ERR) {
	/* trim all the trailing whitespace
	 * the user or the tab completion might
	 * add an extra space at the end of
	 * value, and this will cause an invalid-value
	 * error to be incorrectly generated
	 */
	len = xml_strlen(clibuff);
	if (len > 0) {
	    while (len > 0 && isspace(clibuff[len-1])) {
		len--;
	    }
	    clibuff[len] = 0;
	}
	return clibuff;
    } else {
	return NULL;
    }

}  /* get_cmd_line */


/********************************************************************
 * FUNCTION do_connect
 * 
 * INPUTS:
 *   agent_cb == agent control block to use
 *   rpc == rpc header for 'connect' command
 *   line == input text from readline call, not modified or freed here
 *   start == byte offset from 'line' where the parse RPC method
 *            left off.  This is eiother empty or contains some 
 *            parameters from the user
 *   climode == TRUE if starting from CLI and should try
 *              to connect right away if the mandatory parameters
 *              are present
 *
 * OUTPUTS:
 *   connect_valset parms may be set 
 *   create_session may be called
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_connect (agent_cb_t *agent_cb,
		const obj_template_t *rpc,
		const xmlChar *line,
		uint32 start,
                boolean climode)
{
    const obj_template_t  *obj;
    val_value_t           *connect_valset;
    val_value_t           *valset;
    status_t               res;
    boolean                s1, s2, s3;

#ifdef DEBUG
    if (!agent_cb) {
	return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    /* retrieve the 'connect' RPC template, if not done already */
    if (!rpc) {
	rpc = ncx_find_object(get_yangcli_mod(), 
			      YANGCLI_CONNECT);
	if (!rpc) {
            agent_cb->state = MGR_IO_ST_IDLE;
	    log_write("\nError finding the 'connect' RPC method");
	    return ERR_NCX_DEF_NOT_FOUND;
	}
    }	    

    obj = obj_find_child(rpc, NULL, YANG_K_INPUT);
    if (!obj) {
        agent_cb->state = MGR_IO_ST_IDLE;
	log_write("\nError finding the connect RPC 'input' node");	
	return SET_ERROR(ERR_INTERNAL_VAL);
    }

    res = NO_ERR;

    connect_valset = get_connect_valset();

    /* process any parameters entered on the command line */
    valset = NULL;
    if (line) {
	while (line[start] && xml_isspace(line[start])) {
	    start++;
	}
	if (line[start]) {
	    valset = parse_rpc_cli(rpc, &line[start], &res);
	    if (!valset || res != NO_ERR) {
                if (valset) {
                    val_free_value(valset);
                }
		log_write("\nError in the parameters for RPC %s (%s)",
			  obj_get_name(rpc), 
                          get_error_string(res));
                agent_cb->state = MGR_IO_ST_IDLE;
		return res;
	    }
	}
    }

    if (!valset) {
        if (climode) {
            /* just clone the connect valset to start with */
            valset = val_clone(connect_valset);
            if (!valset) {
                agent_cb->state = MGR_IO_ST_IDLE;
                log_write("\nError: malloc failed");
                return ERR_INTERNAL_MEM;
            }
        } else {
            valset = val_new_value();
            if (!valset) {
                log_write("\nError: malloc failed");
                agent_cb->state = MGR_IO_ST_IDLE;
                return ERR_INTERNAL_MEM;
            } else {
                val_init_from_template(valset, obj);
            }
        }
    }

    /* make sure the 3 required parms are set */
    s1 = val_find_child(valset,
                        YANGCLI_MOD, 
                        YANGCLI_AGENT) ? TRUE : FALSE;
    s2 = val_find_child(valset, 
                        YANGCLI_MOD,
                        YANGCLI_USER) ? TRUE : FALSE;
    s3 = val_find_child(valset, 
                        YANGCLI_MOD,
                        YANGCLI_PASSWORD) ? TRUE : FALSE;

    /* complete the connect valset if needed
     * and transfer it to the agent_cb version
     *
     * try to get any missing params in valset 
     */
    if (interactive_mode()) {
        if (climode && s1 && s2 && s3) {
            if (LOGDEBUG3) {
                log_debug3("\nyangcli: CLI direct connect mode");
            }
        } else {
            res = fill_valset(agent_cb, 
                              rpc, 
                              valset, 
                              connect_valset);
            if (res == ERR_NCX_SKIPPED) {
                res = NO_ERR;
            }
        }
    }

    /* check error or operation canceled */
    if (res != NO_ERR) {
	if (res != ERR_NCX_CANCELED) {
	    log_write("\nError: Connect failed (%s)", 
		      get_error_string(res));
	}
	agent_cb->state = MGR_IO_ST_IDLE;
        val_free_value(valset);
        return res;
    }


    /* passing off valset memory here */
    s1 = s2 = s3 = FALSE;
    if (valset) {
        /* save the malloced valset */
        if (agent_cb->connect_valset) {
            val_free_value(agent_cb->connect_valset);
        }
        agent_cb->connect_valset = valset;

        /* make sure the 3 required parms are set */
        s1 = val_find_child(agent_cb->connect_valset,
                            YANGCLI_MOD, 
                            YANGCLI_AGENT) ? TRUE : FALSE;
        s2 = val_find_child(agent_cb->connect_valset, 
                            YANGCLI_MOD,
                            YANGCLI_USER) ? TRUE : FALSE;
        s3 = val_find_child(agent_cb->connect_valset, 
                            YANGCLI_MOD,
                            YANGCLI_PASSWORD) ? TRUE : FALSE;
    }

    /* check if all params present yet */
    if (s1 && s2 && s3) {
        res = replace_connect_valset(agent_cb->connect_valset);
        if (res != NO_ERR) {
            log_warn("\nWarning: connection parameters could not be saved");
            res = NO_ERR;
        }
        create_session(agent_cb);
    } else {
        res = ERR_NCX_MISSING_PARM;
        log_write("\nError: Connect failed due to missing parameter(s)");
        agent_cb->state = MGR_IO_ST_IDLE;
    }

    return res;

}  /* do_connect */


/********************************************************************
* FUNCTION parse_def
* 
* Definitions have two forms:
*   def       (default module used)
*   module:def (explicit module name used)
*   prefix:def (if prefix-to-module found, explicit module name used)
*
* Parse the possibly module-qualified definition (module:def)
* and find the template for the requested definition
*
* INPUTS:
*   agent_cb == agent control block to use
*   dtyp == definition type 
*       (NCX_NT_OBJ or  NCX_NT_TYP)
*   line == input command line from user
*   len  == pointer to output var for number of bytes parsed
*
* OUTPUTS:
*    *dtyp is set if it started as NONE
*    *len == number of bytes parsed
*
* RETURNS:
*   pointer to the found definition template or NULL if not found
*********************************************************************/
void *
    parse_def (agent_cb_t *agent_cb,
	       ncx_node_t *dtyp,
	       xmlChar *line,
	       uint32 *len)
{
    void           *def;
    xmlChar        *start, *p, *q, oldp, oldq;
    const xmlChar  *prefix, *defname, *modname;
    ncx_module_t   *mod;
    obj_template_t *obj;
    modptr_t       *modptr;
    uint32          prelen;
    xmlns_id_t      nsid;
    
    def = NULL;
    q = NULL;
    oldq = 0;
    prelen = 0;
    *len = 0;
    start = line;

    /* skip any leading whitespace */
    while (*start && xml_isspace(*start)) {
	start++;
    }

    p = start;

    /* look for a colon or EOS or whitespace to end method name */
    while (*p && (*p != ':') && !xml_isspace(*p)) {
	p++;
    }

    /* make sure got something */
    if (p==start) {
	return NULL;
    }

    /* search for a module prefix if a separator was found */
    if (*p == ':') {

	/* use an explicit module prefix in YANG */
	prelen = p - start;
	q = p+1;
	while (*q && !xml_isspace(*q)) {
	    q++;
	}
	*len = q - line;

	oldq = *q;
	*q = 0;
	oldp = *p;
	*p = 0;

	prefix = start;
	defname = p+1;
    } else {
	/* no module prefix, use default module, if any */
	*len = p - line;

	oldp = *p;
	*p = 0;

	/* try the default module, which will be NULL
	 * unless set by the default-module CLI param
	 */
	prefix = NULL;
	defname = start;
    }

    /* look in the registry for the definition name 
     * first check if only the user supplied a module name
     */
    if (prefix) {
	modname = NULL;
	nsid = xmlns_find_ns_by_prefix(prefix);
	if (nsid) {
	    modname = xmlns_get_module(nsid);
	}
	if (modname) {
	    def = try_parse_def(agent_cb,
				modname, defname, dtyp);
	} else {
	    log_error("\nError: no module found for prefix '%s'", 
		      prefix);
	}
    } else {
	def = try_parse_def(agent_cb,
			    YANGCLI_MOD, 
			    defname, 
			    dtyp);

	if (!def && get_default_module()) {
	    def = try_parse_def(agent_cb,
				get_default_module(), 
				defname, 
				dtyp);
	}
	if (!def && (!get_default_module() ||
		     xml_strcmp(get_default_module(), 
				NC_MODULE))) {

	    def = try_parse_def(agent_cb,
				NC_MODULE, 
				defname, 
				dtyp);
	}

	/* if not found, try any module */
	if (!def) {
	    /* try any of the agent modules first */
	    if (use_agentcb(agent_cb)) {
		for (modptr = (modptr_t *)
			 dlq_firstEntry(&agent_cb->modptrQ);
		     modptr != NULL && !def;
		     modptr = (modptr_t *)dlq_nextEntry(modptr)) {

		    def = try_parse_def(agent_cb, 
					modptr->mod->name, 
					defname, 
					dtyp);
		}
	    }

	    /* try any of the manager-loaded modules */
	    for (mod = ncx_get_first_module();
		 mod != NULL && !def;
		 mod = ncx_get_next_module(mod)) {

		def = try_parse_def(agent_cb, 
				    mod->name, 
				    defname, 
				    dtyp);
	    }
	}

	/* if not found, try a partial RPC command name */
	if (!def && get_autocomp()) {
	    switch (*dtyp) {
	    case NCX_NT_NONE:
	    case NCX_NT_OBJ:
		if (use_agentcb(agent_cb)) {
		    for (modptr = (modptr_t *)
			     dlq_firstEntry(&agent_cb->modptrQ);
			 modptr != NULL && !def;
			 modptr = (modptr_t *)dlq_nextEntry(modptr)) {

			def = ncx_match_any_rpc(modptr->mod->name, 
						defname);
			if (def) {
			    *dtyp = NCX_NT_OBJ;
			}
		    }
		}
		if (!def) {
		    def = ncx_match_any_rpc(NULL, defname);
		    if (def) {
			obj = (obj_template_t *)def;
			if (obj_get_nsid(obj) == xmlns_nc_id()) {
			    /* matched a NETCONF RPC and not connected;
			     * would have matched in the use_agentcb()
			     * code above if this is a partial NC op
			     */
			    def = NULL;
			} else {
			    *dtyp = NCX_NT_OBJ;
			}
		    }
		}
		break;
	    default:
		;
	    }
	}
    }

    /* restore string as needed */
    *p = oldp;
    if (q) {
	*q = oldq;
    }

    return def;
    
} /* parse_def */


/********************************************************************
* FUNCTION send_keepalive_get
* 
* Send a <get> operation to the agent to keep the session
* from getting timed out; agent sent a keepalive request
* and SSH will drop the session unless data is sent
* within a configured time
*
* INPUTS:
*    agent_cb == agent control block to use
*
* OUTPUTS:
*    state may be changed or other action taken
*
* RETURNS:
*    status
*********************************************************************/
status_t
    send_keepalive_get (agent_cb_t *agent_cb)
{
    (void)agent_cb;
    return NO_ERR;
} /* send_keepalive_get */


/* END yangcli_cmd.c */
