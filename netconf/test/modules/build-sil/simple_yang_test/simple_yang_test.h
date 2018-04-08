
#ifndef _H_simple_yang_test
#define _H_simple_yang_test
/* 

 * Copyright (c) 2009 - 2011, Andy Bierman
 * All Rights Reserved.
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *

*** Generated by yangdump 2.1.1630

    Combined SIL header
    module simple_yang_test
    revision 2011-11-21
    namespace http://netconfcentral.org/ns/simple_yang_test

 */

#include <libxml/xmlstring.h>

#include "dlq.h"
#include "ncxtypes.h"
#include "op.h"
#include "status.h"
#include "val.h"

#ifdef __cplusplus
extern "C" {
#endif

#define y_simple_yang_test_M_simple_yang_test (const xmlChar *)"simple_yang_test"
#define y_simple_yang_test_R_simple_yang_test (const xmlChar *)"2011-11-21"

#define y_simple_yang_test_N_ifMTU (const xmlChar *)"ifMTU"
#define y_simple_yang_test_N_ifType (const xmlChar *)"ifType"
#define y_simple_yang_test_N_interface (const xmlChar *)"interface"
#define y_simple_yang_test_N_name (const xmlChar *)"name"
#define y_simple_yang_test_N_protocol (const xmlChar *)"protocol"
#define y_simple_yang_test_N_tcp (const xmlChar *)"tcp"
#define y_simple_yang_test_N_udp (const xmlChar *)"udp"

/* case /protocol/name/udp */
typedef struct y_simple_yang_test_T_protocol_name_udp_ {
    boolean udp;
} y_simple_yang_test_T_protocol_name_udp;

/* case /protocol/name/tcp */
typedef struct y_simple_yang_test_T_protocol_name_tcp_ {
    boolean tcp;
} y_simple_yang_test_T_protocol_name_tcp;

/* choice /protocol/name */
typedef union y_simple_yang_test_T_protocol_name_ {
    y_simple_yang_test_T_protocol_name_udp udp;
    y_simple_yang_test_T_protocol_name_tcp tcp;
} y_simple_yang_test_T_protocol_name;

/* container /protocol */
typedef struct y_simple_yang_test_T_protocol_ {
    y_simple_yang_test_T_protocol_name name;
} y_simple_yang_test_T_protocol;

/* container /interface */
typedef struct y_simple_yang_test_T_interface_ {
    xmlChar *ifType;
    uint32 ifMTU;
} y_simple_yang_test_T_interface;

/********************************************************************
* FUNCTION y_simple_yang_test_init
* 
* initialize the simple_yang_test server instrumentation library
* 
* INPUTS:
*    modname == requested module name
*    revision == requested version (NULL for any)
* 
* RETURNS:
*     error status
********************************************************************/
extern status_t y_simple_yang_test_init (
    const xmlChar *modname,
    const xmlChar *revision);


/********************************************************************
* FUNCTION y_simple_yang_test_init2
* 
* SIL init phase 2: non-config data structures
* Called after running config is loaded
* 
* RETURNS:
*     error status
********************************************************************/
extern status_t y_simple_yang_test_init2 (void);


/********************************************************************
* FUNCTION y_simple_yang_test_cleanup
*    cleanup the server instrumentation library
* 
********************************************************************/
extern void y_simple_yang_test_cleanup (void);

#ifdef __cplusplus
} /* end extern 'C' */
#endif

#endif