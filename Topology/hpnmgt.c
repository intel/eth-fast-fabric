/* BEGIN_ICS_COPYRIGHT2 ****************************************

Copyright (c) 2015-2017, Intel Corporation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

** END_ICS_COPYRIGHT2   ****************************************/

/* [ICS VERSION STRING: unknown] */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "iba/stl_sd.h"
#include "hpnmgt_snmp_priv.h"
#include "topology_internal.h"

#define INCLUDE "include"
#define INCLUDE_LEN 7

extern SnmpNodeConfigParamData_t* snmpDataAddNodeConfig(FabricData_t *fabricp, SnmpNodeConfigParamData_t *nodeConfParmp, int verbose, int quiet);

#define HPN_CONF_IS_VALID_FN(d, f, b)   ((strlen(d) + strlen(f) + 1) < (sizeof(b) - 1))
#define HPN_CONF_GET_FN(d, f, b) { \
    if (*f == '/' ) \
        snprintf(b, sizeof(b), "%s", f); \
    else if (strlen(d) == 0 || d[strlen(d)-1] == '/') \
        snprintf(b, sizeof(b), "%s%s", d, f); \
    else \
        snprintf(b, sizeof(b), "%s/%s", d, f); \
}

uint8 mgt_verbose_level = 0;

/** ========================================================================= */
void hmgt_set_dbg(struct hmgt_port *port, FILE *file)
{
	if (port) port->dbg_file = file;
}
/** ========================================================================= */
void hmgt_set_err(struct hmgt_port *port, FILE *file)
{
	if (port) port->error_file = file;
}
/** ========================================================================= */
void hmgt_set_timeout(struct hmgt_port *port, int ms_timeout)
{
	if (port) {
		port->ms_timeout = (ms_timeout > 0 ? ms_timeout : HMGT_DEF_TIMEOUT_MS);
	}
}
/** ========================================================================= */
void hmgt_set_retry_count(struct hmgt_port *port, int retry_count)
{
	if (port) {
		port->retry_count = (retry_count >= 0 ? retry_count : HMGT_DEF_RETRY_CNT);
	}
}
/** ========================================================================= */
const char* hmgt_status_totext(HMGT_STATUS_T status)
{
	switch (status) {
	case HMGT_STATUS_SUCCESS:                return "Success";
	case HMGT_STATUS_ERROR:                  return "Error";
	case HMGT_STATUS_INVALID_STATE:          return "Invalid State";
	case HMGT_STATUS_INVALID_OPERATION:      return "Invalid Operation";
	case HMGT_STATUS_INVALID_SETTING:        return "Invalid Setting";
	case HMGT_STATUS_INVALID_PARAMETER:      return "Invalid Parameter";
	case HMGT_STATUS_INSUFFICIENT_RESOURCES: return "Insufficient Resources";
	case HMGT_STATUS_INSUFFICIENT_MEMORY:    return "Insufficient Memory";
	case HMGT_STATUS_COMPLETED:              return "Completed";
	case HMGT_STATUS_NOT_DONE:               return "Not Done";
	case HMGT_STATUS_PENDING:                return "Pending";
	case HMGT_STATUS_TIMEOUT:                return "Timeout";
	case HMGT_STATUS_CANCELED:               return "Canceled";
	case HMGT_STATUS_REJECT:                 return "Reject";
	case HMGT_STATUS_OVERRUN:                return "Overrun";
	case HMGT_STATUS_PROTECTION:             return "Protection";
	case HMGT_STATUS_NOT_FOUND:              return "Not Found";
	case HMGT_STATUS_UNAVAILABLE:            return "Unavailable";
	case HMGT_STATUS_BUSY:                   return "Busy";
	case HMGT_STATUS_DISCONNECT:             return "Disconnect";
	case HMGT_STATUS_DUPLICATE:              return "Duplicate";
	case HMGT_STATUS_POLL_NEEDED:            return "Poll Needed";
	default: return "Unknown";
	}
}
/** ========================================================================= */
const char* hmgt_service_state_totext(int service_state)
{
	switch (service_state) {
	case OMGT_SERVICE_STATE_OPERATIONAL: return "Operational";
	case OMGT_SERVICE_STATE_DOWN:        return "Down";
	case OMGT_SERVICE_STATE_UNAVAILABLE: return "Unavailable";
	case OMGT_SERVICE_STATE_UNKNOWN:
	default:
		return "Unknown";
	}
}

/** =========================================================================
 * Init sub libraries like umad here
 * */
static int init_sub_lib(struct hmgt_port *port _UNUSED_)
{
	static int done = 0;

	if (done)
		return 0;

	if (hmgt_snmp_init())
		done = 1;

	return (done) ? 0 : 1;
}

static boolean is_valid_file(const char* filename)
{
	struct stat res;
	char buffer[PATH_MAX];
	char *p;

	if (!realpath(filename, buffer)) {
		fprintf(stderr, "%s: ERROR - failed to get real path '%s': %s\n",
		                g_Top_cmdname, filename, strerror(errno));
		return FALSE;
	}

	if (lstat(buffer, &res)) {
		fprintf(stderr, "%s: ERROR - failed to get stat of '%s': %s\n",
		                g_Top_cmdname, buffer, strerror(errno));
		return FALSE;
	}

	if ((res.st_mode & S_IXUSR) | (res.st_mode & S_IXGRP) | (res.st_mode & S_IXOTH)) {
		fprintf(stderr, "%s: ERROR - '%s' is an executable file\n", g_Top_cmdname, filename);
		return FALSE;
	}

	if ((res.st_mode & S_IWGRP) | (res.st_mode & S_IWOTH)) {
		fprintf(stderr, "%s: ERROR - '%s' is writable by group or other\n", g_Top_cmdname, filename);
		return FALSE;
	}

	uid_t f_uid = res.st_uid;
	gid_t f_gid = res.st_gid;

	p = strrchr(buffer, '/');
	if (p) {
		*p = '\0';
		if (stat(buffer, &res)) {
			// shouldn't happen
			fprintf(stderr, "%s: ERROR - failed to get stat of '%s': %s\n",
			                g_Top_cmdname, buffer, strerror(errno));
			return FALSE;
		}

		if ((res.st_mode & S_IWGRP) | (res.st_mode & S_IWOTH)) {
			fprintf(stderr, "%s: ERROR - parent '%s' is writable by group or other\n",
			                g_Top_cmdname, buffer);
			return FALSE;
		}
	}

	gid_t p_gid = getegid();
	if (f_uid && (f_gid != p_gid)) {
		fprintf(stderr, "%s: ERROR - intend to open unsafe file %s owned by another group\n",
		                g_Top_cmdname, filename);
		return FALSE;
	}

	return TRUE;
}

static int parse_nodes_file(FabricData_t *fabricp, const char* filename, const char* dirname,
		uint8_t nodetype, boolean supportInclude)
{
	FILE *fp = NULL;
	char * nodenamep, * interfacesp;
	int skipping = 0;
	char buffer[CONF_LINE_SIZE];
	int line_num = 0;
	size_t line_len = 0;
	boolean has_error = FALSE;
	char filepath[HMGT_CONFIG_PARAMS_PATH_SIZE];

	HPN_CONF_GET_FN("", filename, filepath);
	if (is_valid_file(filepath)) {
                fp = fopen(filepath, "r");
	}

	if (fp == NULL) {
                fprintf(stderr, "%s: WARNING - failed to open '%s': %s. ",
                                g_Top_cmdname, filepath, strerror(errno));
                HPN_CONF_GET_FN(dirname, filename, filepath);
                fprintf(stderr, "Try '%s'\n", filepath);
                if (is_valid_file(filepath)) {
                        fp = fopen(filepath, "r");
                }
                if (fp == NULL) {
                        fprintf(stderr, "%s: WARNING - failed to open '%s': %s\n",
                                        g_Top_cmdname, filepath, strerror(errno));
                        return -1;
                }
	}
	while (NULL != fgets(buffer, sizeof(buffer), fp)) {
		line_num += 1;
		// Ignore long lines
		line_len = strlen(buffer);
		if (line_len == 0)
			continue;
		if (buffer[line_len - 1] != '\n') {
			skipping=1;
			continue;
		}
		if (skipping) {
			skipping = 0;
			continue;
		}
		if (buffer[0]=='#' || buffer[0]=='\n') {
			continue; // ignore comments or empty lines
		}
		// remove '\n'
		if (buffer[line_len - 1] == '\n')
			buffer[line_len - 1] = '\0';
		if (strncasecmp(buffer, INCLUDE, INCLUDE_LEN) == 0 &&
				isspace((unsigned char)*(buffer+INCLUDE_LEN))) {
			if (supportInclude) {
				char* incFile = strtok(buffer + INCLUDE_LEN + 1, " \t\r\n");
				// to avoid cycling include, we only support it at 1 level
				if (incFile) {
					if (parse_nodes_file(fabricp, incFile, dirname, nodetype, FALSE)) {
						has_error = TRUE;
					}
				} else {
					// rare case shouldn't happen
					fprintf(stderr, "%s: WARNING - invalid include file at line %d:'%s' in file '%s'\n",
					                g_Top_cmdname, line_num, buffer, filepath);
				}
			} else {
				fprintf(stderr, "%s: WARNING - only one level include supported. Ignored line %d:'%s' in file '%s'\n",
				                g_Top_cmdname, line_num, buffer, filepath);
			}
			continue;
		}

		char* org_buff = strdup(buffer);
		// Get Host Name or IP address
		interfacesp = strchr(buffer, ':');
		if (interfacesp) {
			*interfacesp='\0';
			interfacesp += 1;
		}
		if ((nodenamep = strtok(buffer, " \t\r\n"))) {
			SnmpNodeConfigParamData_t ncp;

			// Add node to discovery list
			ncp.NodeType = nodetype;
			ncp.NodeDesc = nodenamep;
			ncp.InterfaceDesc = interfacesp;

			snmpDataAddNodeConfig(fabricp, &ncp, 5, 1);
		} else {
			if (org_buff)
				fprintf(stderr, "%s: WARNING - invalid host/switch name. Ignored line %d:'%s' in file '%s'\n",
						g_Top_cmdname, line_num, org_buff, filepath);
			else
				fprintf(stderr, "%s: WARNING - invalid host/switch name. Ignored line %d in file '%s'\n",
						g_Top_cmdname, line_num, filepath);
		}
		if (org_buff)
			free(org_buff);
	}
	fclose(fp);
	if (has_error) {
		return -1;
	} else {
		return 0;
	}
}

static HMGT_STATUS_T open_port_internal(struct hmgt_port *port, char *hfi_name _UNUSED_, uint8_t port_num _UNUSED_)
{
	HMGT_STATUS_T err = HMGT_STATUS_SUCCESS;

	if (init_sub_lib(port)) {
		err = HMGT_STATUS_UNAVAILABLE;
		goto free_port;
	}

	/* Set Timeout and retry to default values */
	port->ms_timeout = HMGT_DEF_TIMEOUT_MS;
	port->retry_count = HMGT_DEF_RETRY_CNT;

free_port:
	return (err);
}


/** ========================================================================= */
HMGT_STATUS_T hmgt_open_port_by_guid(struct hmgt_port **port, uint64_t port_guid _UNUSED_, struct hmgt_params *session_params)
{
	HMGT_STATUS_T status = HMGT_STATUS_SUCCESS;
	char name[HMGT_SHORT_STRING_SIZE];
	int num = -1;
	struct hmgt_port *rc;

	if ((rc = calloc(1, sizeof(*rc))) == NULL)
		return (HMGT_STATUS_INSUFFICIENT_MEMORY);

	if (session_params) {
		rc->dbg_file = session_params->debug_file;
		rc->error_file = session_params->error_file;
		rc->config_file_params = session_params->config_file_params;
	} else {
		rc->dbg_file = NULL;
		rc->error_file = NULL;
		rc->config_file_params = NULL;
	}

	status = open_port_internal(rc, name, num);

//done:

	if (status == HMGT_STATUS_SUCCESS) {
		//rc->is_oob_enabled = FALSE;
		*port = rc;
	} else {
		free(rc);
		rc = NULL;
	}
	return status;
}


/** ========================================================================= */
void hmgt_close_port(struct hmgt_port *port)
{
	if (port) {
		free(port);
	}
}

/**
 * Pose a query to the fabric, expect a response.
 *
 * @param port           port opened by omgt_open_port_*
 * @param pQuery         pointer to the query structure
 * @param ppQueryResult  pointer where the response will go
 *
 * @return          0 if success, else error code
 */
static HMGT_STATUS_T query_fabric_internal(struct hmgt_port *port, HMGT_QUERY *pQuery,
	struct _HQUERY_RESULT_VALUES **ppQueryResult)
{
	HMGT_STATUS_T status = HMGT_STATUS_ERROR;
	fabric_config_t * cfpp = (fabric_config_t *)port->config_file_params;

	DBG_ENTER_FUNC(port);

	switch ((int)pQuery->OutputType) {
	case OutputTypeHpnSnmpFabricDataRecord:
		{
			FabricData_t *fabricp = pQuery->InputValue.FabricDataRecord.FabricDataPtr;
			char snmpHostsFn[HMGT_CONFIG_PARAMS_PATH_SIZE], snmpSwitchesFn[HMGT_CONFIG_PARAMS_PATH_SIZE];

			switch (pQuery->InputType) {
			case InputTypeFabricDataPtr:
				break;
			default:
				//HMGT_OUTPUT_ERROR(port,"Query not supported by opamgt: Input=%s, Output=%s\n",
				//		iba_sd_query_input_type_msg(pQuery->InputType),
				//		iba_sd_query_result_type_msg(pQuery->OutputType));
				status = HMGT_STATUS_INVALID_PARAMETER; goto done;
			}
			/*
			 * Process SNMP related configuration parameters
			 */
			if (!fabricp || !cfpp) {
				status = HMGT_STATUS_INVALID_PARAMETER;
			} else {
				fabricp->SnmpPort = cfpp->snmp_port;
				snprintf(fabricp->name, HPN_NODE_COMMUNITY_ARRAY_SIZE, "%s", cfpp->name);
				snprintf(fabricp->SnmpVersion, HPN_NODE_COMMUNITY_ARRAY_SIZE, "%s", cfpp->snmp_version);
				snprintf(fabricp->SnmpCommunityString, HPN_NODE_COMMUNITY_ARRAY_SIZE, "%s", cfpp->snmp_community_string);
				snprintf(fabricp->SnmpSecurityName, HPN_NODE_COMMUNITY_ARRAY_SIZE, "%s", cfpp->snmp_security_name);
				snprintf(fabricp->SnmpSecurityLevel, HPN_NODE_COMMUNITY_ARRAY_SIZE, "%s", cfpp->snmp_security_level);
				snprintf(fabricp->SnmpAuthenticationProtocol, HPN_NODE_COMMUNITY_ARRAY_SIZE, "%s", cfpp->snmp_auth_protocol);
				snprintf(fabricp->SnmpAuthenticationPassphrase, HPN_NODE_COMMUNITY_ARRAY_SIZE, "%s", cfpp->snmp_auth_passphrase);
				snprintf(fabricp->SnmpEncryptionProtocol, HPN_NODE_COMMUNITY_ARRAY_SIZE, "%s", cfpp->snmp_encryp_protocol);
				snprintf(fabricp->SnmpEncryptionPassphrase, HPN_NODE_COMMUNITY_ARRAY_SIZE, "%s", cfpp->snmp_encryp_passphrase);

				// Process the hosts configuration file.
				HPN_CONF_GET_FN(cfpp->directory, cfpp->hosts_file, snmpHostsFn);
				if (0 != parse_nodes_file(fabricp, snmpHostsFn, cfpp->directory, STL_NODE_FI, TRUE)) {
					fprintf(stderr, "%s: WARNING - failed to process SNMP hosts from in file: '%s'\n", __func__, snmpHostsFn);
					status = HMGT_STATUS_INVALID_PARAMETER;
					break;
				}

				// Process the switches configuration file.
				HPN_CONF_GET_FN(cfpp->directory, cfpp->switches_file, snmpSwitchesFn);
				if (0 != parse_nodes_file(fabricp, snmpSwitchesFn, cfpp->directory, STL_NODE_SW, TRUE)) {
					fprintf(stderr, "%s: WARNING - failed to process SNMP switches in file: '%s'\n", __func__, snmpSwitchesFn);
					status = HMGT_STATUS_INVALID_PARAMETER;
					break;
				}

				status = hmgt_snmp_get_fabric_data(port, pQuery, ppQueryResult);
			}

			if (status != HMGT_STATUS_SUCCESS) break;
		}
		break;
	default:
		//HMGT_OUTPUT_ERROR(port, "Query not supported by opamgt: Input=%s, Output=%s\n",
		//		iba_sd_query_input_type_msg(pQuery->InputType),
		//		iba_sd_query_result_type_msg(pQuery->OutputType));
		status = HMGT_STATUS_INVALID_PARAMETER; goto done;
		break;
	}

done:
	//*ppQueryResult = pQR;

	DBG_EXIT_FUNC(port);

	return status;
}

HMGT_STATUS_T hmgt_query_fabric(struct hmgt_port *port, HMGT_QUERY *pQuery,
	struct _HQUERY_RESULT_VALUES **ppQueryResult)
{
	HMGT_STATUS_T status = HMGT_STATUS_SUCCESS;

	if (port == NULL)
		return HMGT_STATUS_INVALID_PARAMETER;

	if (pQuery != NULL && ppQueryResult != NULL) {
		status = query_fabric_internal(port, pQuery, ppQueryResult);
		if (status == HMGT_STATUS_TIMEOUT || status ==  HMGT_STATUS_NOT_DONE) {
			HMGT_OUTPUT_ERROR(port, "Query Failed on response: %s.\n", hmgt_status_totext(status));
			port->sa_service_state = HMGT_SERVICE_STATE_DOWN;
			/* If SA is down assume PA is down */
			port->pa_service_state = HMGT_SERVICE_STATE_DOWN;
		} else if (status != HMGT_STATUS_SUCCESS) {
			HMGT_OUTPUT_ERROR(port, "Query Failed: %s. \n", hmgt_status_totext(status));
		}
	}

	return status;
}
/**
 * Free the memory used in the query result
 *
 * @param pQueryResult    pointer to the SA query result buffer
 *
 * @return          none
 */
void hmgt_free_query_result_buffer(IN void * pQueryResult)
{
	MemoryDeallocate(pQueryResult);
}

/*
 * HPN Configuration Parameters XML parse code
 * */

void freeFabricConfs(QUICK_LIST *listp)
{
	LIST_ITEM *p;

	for (p = QListHead(listp); p != NULL; p = QListNext(listp, p)) {
	        fabric_config_t *fabric_conf = QListObj(p);
	        QListRemoveItem(listp, &fabric_conf->list_item);
		MemoryDeallocate(fabric_conf);
	}
}

static IXML_FIELD snmpConfigFields[] = {
	{ tag:"Name", format:'s', IXML_FIELD_INFO(fabric_config_t, name) },
	{ tag:"Enable", format:'u', IXML_FIELD_INFO(fabric_config_t, enable) },
	{ tag:"ConfigDir", format:'s', IXML_FIELD_INFO(fabric_config_t, directory) },
	{ tag:"HostsFile", format:'s', IXML_FIELD_INFO(fabric_config_t, hosts_file) },
	{ tag:"SwitchesFile", format:'s', IXML_FIELD_INFO(fabric_config_t, switches_file) },
	{ tag:"TopologyFile", format:'s', IXML_FIELD_INFO(fabric_config_t, topology_file) },
	{ tag:"SnmpVersion", format:'s', IXML_FIELD_INFO(fabric_config_t, snmp_version) },
	{ tag:"SnmpPort", format:'u', IXML_FIELD_INFO(fabric_config_t, snmp_port) },
	{ tag:"SnmpCommunityString", format:'s', IXML_FIELD_INFO(fabric_config_t, snmp_community_string) },
	{ tag:"SnmpSecurityName", format:'s', IXML_FIELD_INFO(fabric_config_t, snmp_security_name) },
	{ tag:"SnmpSecurityLevel", format:'s', IXML_FIELD_INFO(fabric_config_t, snmp_security_level) },
	{ tag:"SnmpAuthenticationProtocol", format:'s', IXML_FIELD_INFO(fabric_config_t, snmp_auth_protocol) },
	{ tag:"SnmpAuthPassphrase", format:'s', IXML_FIELD_INFO(fabric_config_t, snmp_auth_passphrase) },
	{ tag:"SnmpEncryptionProtocol", format:'s', IXML_FIELD_INFO(fabric_config_t, snmp_encryp_protocol) },
	{ tag:"SnmpEncrypPassphrase", format:'s', IXML_FIELD_INFO(fabric_config_t, snmp_encryp_passphrase) },
	{ NULL }
};

static void *hmgtConfXmlCommonParserStart(IXmlParserState_t *state, void *parent _UNUSED_, const char **attr _UNUSED_)
{
        mgt_conf_t *mgt_conf = IXmlParserGetContext(state);

        if (mgt_conf->common_processed) {
                IXmlParserPrintError(state, "Only one <Common> can be supplied\n");
                return NULL;
        }

        return &mgt_conf->common;
}

static void hmgtConfXmlCommonParserEnd(IXmlParserState_t *state, const IXML_FIELD *field _UNUSED_, void *object _UNUSED_,
                                       void *parent _UNUSED_, XML_Char *content _UNUSED_, unsigned len _UNUSED_, boolean valid)
{
        mgt_conf_t *mgt_conf = IXmlParserGetContext(state);

        if (!valid) {
                return;
        }

        mgt_conf->common_processed = TRUE;
}

static void *hmgtConfXmlFabricParserStart(IXmlParserState_t *state, void *parent _UNUSED_, const char **attr _UNUSED_)
{
        mgt_conf_t *mgt_conf = IXmlParserGetContext(state);

	fabric_config_t *fabric_conf = (fabric_config_t*)MemoryAllocate2AndClear(sizeof(fabric_config_t),
	                                                                         IBA_MEM_FLAG_PREMPTABLE, MYTAG);

	if (! fabric_conf) {
		IXmlParserPrintError(state, "Unable to allocate memory");
		return NULL;
	}

//	if (!mgt_conf->common_processed) {
//		IXmlParserPrintError(state, "No valid <Common> supplied");
//                return NULL;
//	}

	if (mgt_conf->common_processed)
        	memcpy(fabric_conf, &mgt_conf->common, sizeof(fabric_config_t));
	ListItemInitState(&fabric_conf->list_item);
	QListSetObj(&fabric_conf->list_item, fabric_conf);
        return fabric_conf;
}

static void hmgtConfXmlFabricParserEnd(IXmlParserState_t *state, const IXML_FIELD *field _UNUSED_, void *object,
                                       void *parent _UNUSED_, XML_Char *content _UNUSED_, unsigned len _UNUSED_, boolean valid)
{
        mgt_conf_t *mgt_conf = IXmlParserGetContext(state);
        fabric_config_t *fabric_conf = (fabric_config_t *)object;

        if (!fabric_conf->name[0]) {
                IXmlParserPrintError(state, "No Plane Name specified!");
                goto invalid;
        }

        if (strcmp(fabric_conf->name, "ALL") == 0) {
                IXmlParserPrintError(state, "Plane Name 'ALL' is reserved!");
                goto invalid;
        }

        // plane name may be used as part of a file name, and we support multiple planes in
        // command line with plane names separated by space, so '/' and space are not allowed.
        char *np = fabric_conf->name;
        while (*np) {
        	if (*np == '/' || isspace(*np)) {
			IXmlParserPrintError(state, "'/' and space are not allowed in Plane Name!");
			goto invalid;
        	}
        	np += 1;
        }

        if (!valid)
                goto invalid;

	LIST_ITEM *p = NULL;
	QUICK_LIST *listp = &mgt_conf->fabric_confs;
	for (p = QListHead(listp); p != NULL; p = QListNext(listp, p)) {
		fabric_config_t *tmp_conf = QListObj(p);
		if (strncmp(tmp_conf->name, fabric_conf->name, HMGT_SHORT_STRING_SIZE) == 0) {
			IXmlParserPrintError(state, "Duplicate Plane Name '%s' specified!", fabric_conf->name);
			goto invalid;
		}
	}

	if (!fabric_conf->enable)
		return;

	if (!fabric_conf->directory[0]) {
		strcpy(fabric_conf->directory, "/etc/eth-tools");
		if (mgt_verbose_level)
			fprintf(stderr, "Warning: Plane '%s' Config Directory is empty. Use default value - %s\n",
			                fabric_conf->name, fabric_conf->directory);
	}
	if (!fabric_conf->hosts_file[0]) {
		strcpy(fabric_conf->hosts_file, "allhosts");
		if (mgt_verbose_level)
			fprintf(stderr, "Warning: Plane '%s' Hosts File is empty. Use default value - %s\n",
			                fabric_conf->name, fabric_conf->hosts_file);
	}
	if (!fabric_conf->switches_file[0]) {
		strcpy(fabric_conf->switches_file, "switches");
		if (mgt_verbose_level)
			fprintf(stderr, "Warning: Plane '%s' Switches File is empty. Use default value - %s\n",
			                fabric_conf->name, fabric_conf->switches_file);
	}
	if (!fabric_conf->snmp_version[0]) {
		strcpy(fabric_conf->snmp_version, "SNMP_VERSION_2c");
		if (mgt_verbose_level)
			fprintf(stderr, "Warning: Plane '%s' SNMP Version is empty. Use default value - %s\n",
			                fabric_conf->name, fabric_conf->snmp_version);
	}
	if (!fabric_conf->snmp_port) {
		fabric_conf->snmp_port = 161;
		if (mgt_verbose_level)
			fprintf(stderr, "Warning: Plane '%s' SNMP Port is empty. Use default value - %d\n",
			                fabric_conf->name, fabric_conf->snmp_port);
	}
	if (!fabric_conf->snmp_community_string[0]) {
		strcpy(fabric_conf->snmp_community_string, "public");
		if (mgt_verbose_level)
			fprintf(stderr, "Warning: Plane '%s' SNMP Community String is empty. Use default value - %s\n",
			                fabric_conf->name, fabric_conf->snmp_community_string);
	}
	if (!fabric_conf->snmp_security_level[0]) {
		strcpy(fabric_conf->snmp_security_level, "NOAUTH");
		if (mgt_verbose_level)
			fprintf(stderr, "Warning: Plane '%s' SNMP Security Level is empty. Use default value - %s\n",
			                fabric_conf->name, fabric_conf->snmp_security_level);
	}

	QListInsertTail(&mgt_conf->fabric_confs, &fabric_conf->list_item);
	return;

invalid:
	if (ListItemIsInAList(&fabric_conf->list_item))
		QListRemoveItem(&mgt_conf->fabric_confs, &fabric_conf->list_item);
	MemoryDeallocate(fabric_conf);
}

static IXML_FIELD hmgtConfigMgtFields[] = {
	{ tag:"Common", format:'k', subfields:snmpConfigFields, start_func:hmgtConfXmlCommonParserStart,
	                end_func:hmgtConfXmlCommonParserEnd }, // structure
	{ tag:"Plane", format:'K', subfields:snmpConfigFields, start_func:hmgtConfXmlFabricParserStart,
	                end_func:hmgtConfXmlFabricParserEnd }, // structure
	{ NULL }
};

static void *hmgtConfXmlParserStart(IXmlParserState_t *state _UNUSED_, void *parent _UNUSED_, const char **attr _UNUSED_)
{
	return NULL;
}

static void hmgtConfXmlParserEnd(IXmlParserState_t *state, const IXML_FIELD *field _UNUSED_, void *object _UNUSED_,
                                 void *parent _UNUSED_, XML_Char *content _UNUSED_, unsigned len _UNUSED_, boolean valid)
{
	mgt_conf_t *mgt_conf = IXmlParserGetContext(state);

	if (! valid)
		goto invalid;

	if (! QListCount(&mgt_conf->fabric_confs)) {
		IXmlParserPrintError(state, "No enabled Plane.");
		goto invalid;
	}

	return;

invalid:
        freeFabricConfs(&mgt_conf->fabric_confs);
}

static IXML_FIELD hmgtConfigTopLevelFields[] = {
	{ tag:"Config", format:'K', subfields:hmgtConfigMgtFields, start_func:hmgtConfXmlParserStart,
	                end_func:hmgtConfXmlParserEnd }, // structure
	{ NULL }
};

FSTATUS  Xml2ParseHpnConfigFile(const char *input_file, int quiet, int verbose, mgt_conf_t *params)
{
	unsigned tags_found, fields_found;
	const char *filename=input_file;

	mgt_verbose_level = verbose;
	if (! quiet)
		ProgressPrint(TRUE, "Parsing %s...", Top_truncate_str(input_file));
	if (FSUCCESS != IXmlParseInputFile(input_file, IXML_PARSER_FLAG_NONE, hmgtConfigTopLevelFields,
					   NULL, params, NULL, NULL, &tags_found, &fields_found)) {
		return FERROR;
	}

	if (tags_found != 1 || fields_found != 1) {
		fprintf(stderr, "Warning: potentially inaccurate input '%s': found %u recognized top level tags, expected 1\n",
		                filename, tags_found);
	}

        if (verbose) {
                QUICK_LIST *fabs = &params->fabric_confs;
                LIST_ITEM *p;

                for (p=QListHead(fabs); p != NULL;) {
                        fabric_config_t *fabric_conf = QListObj(p);
                        fprintf(stderr, "Name=%s\n", fabric_conf->name);
                        fprintf(stderr, "  ConfDir=%s\n", fabric_conf->directory);
                        fprintf(stderr, "  HostsFile=%s\n", fabric_conf->hosts_file);
                        fprintf(stderr, "  SwitchesFile=%s\n", fabric_conf->switches_file);
                        fprintf(stderr, "  SnmpPort=%d\n", fabric_conf->snmp_port);
                        fprintf(stderr, "  SnmpVersion=%s\n", fabric_conf->snmp_version);
                        fprintf(stderr, "  SnmpSecurityLevel=%s\n", fabric_conf->snmp_security_level);
                        fprintf(stderr, "  SnmpCommunityString=%s\n", fabric_conf->snmp_community_string);
                        LIST_ITEM *nextp = QListNext(fabs, p);
                        p = nextp;
                }
        }
	return FSUCCESS;
}

FSTATUS hmgt_parse_config_file(const char *input_file, int quiet, int verbose, mgt_conf_t *params)
{
	return Xml2ParseHpnConfigFile(input_file, quiet, verbose, params);
}
/* END SNMP Paramters XML parse code */
