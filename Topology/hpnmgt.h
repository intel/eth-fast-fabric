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

#ifndef __HPNMGT_H__
#define __HPNMGT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <iba/stl_types.h>
#include <iba/public/ilist.h>

typedef uint32_t HMGT_STATUS_T;

/* OMGT Service State Values */
#define OMGT_SERVICE_STATE_UNKNOWN         0
#define OMGT_SERVICE_STATE_OPERATIONAL     1
#define OMGT_SERVICE_STATE_DOWN            (-1)
#define OMGT_SERVICE_STATE_UNAVAILABLE     (-2)

#define OMGT_DEF_TIMEOUT_MS 1000

#define HMGT_STATUS_SUCCESS                 0x00
#define HMGT_STATUS_ERROR                   0x01
#define HMGT_STATUS_INVALID_STATE           0x02
#define HMGT_STATUS_INVALID_OPERATION       0x03
#define HMGT_STATUS_INVALID_SETTING         0x04
#define HMGT_STATUS_INVALID_PARAMETER       0x05
#define HMGT_STATUS_INSUFFICIENT_RESOURCES  0x06
#define HMGT_STATUS_INSUFFICIENT_MEMORY     0x07
#define HMGT_STATUS_COMPLETED               0x08
#define HMGT_STATUS_NOT_DONE                0x09
#define HMGT_STATUS_PENDING                 0x0A
#define HMGT_STATUS_TIMEOUT                 0x0B
#define HMGT_STATUS_CANCELED                0x0C
#define HMGT_STATUS_REJECT                  0x0D
#define HMGT_STATUS_OVERRUN                 0x0E
#define HMGT_STATUS_PROTECTION              0x0F
#define HMGT_STATUS_NOT_FOUND               0x10
#define HMGT_STATUS_UNAVAILABLE             0x11
#define HMGT_STATUS_BUSY                    0x12
#define HMGT_STATUS_DISCONNECT              0x13
#define HMGT_STATUS_DUPLICATE               0x14
#define HMGT_STATUS_POLL_NEEDED             0x15
#define HMGT_STATUS_PARTIALLY_PROCESSED     0x16

#define HMGT_STATUS_COUNT                   0x17 /* should be the last value */


/* opaque data defined internally */
struct hmgt_port;

#define HMGT_DBG_FILE_SYSLOG ((FILE *)-1)
#define HMGT_DEF_TIMEOUT_MS 1000
#define HMGT_DEF_RETRY_CNT 3

#define HMGT_SHORT_STRING_SIZE 64
#define HMGT_MAX_STRING_SIZE 256
#define HMGT_CONFIG_PARAMS_DIR_SIZE 256
#define HMGT_CONFIG_PARAMS_FILENAME_SIZE 256
#define HMGT_CONFIG_PARAMS_PATH_SIZE (HMGT_CONFIG_PARAMS_DIR_SIZE + HMGT_CONFIG_PARAMS_FILENAME_SIZE)
/**
 * @brief fabric related configuration file parameter values.
 *
 * A structure to contain the SNMP API related parameter values used during runtime.
 */
typedef struct fabric_config_s {
        LIST_ITEM list_item; /* internal use for QLIST */
	// SNMP API related configuration parameters
	char name[HMGT_SHORT_STRING_SIZE]; /* Fabric Name */
	uint8 enable; /* indicate whether the fabric is enabled */
	char directory[HMGT_CONFIG_PARAMS_DIR_SIZE]; /* Directory location of related files */
	char hosts_file[HMGT_CONFIG_PARAMS_FILENAME_SIZE]; /* Hosts file for a Fabric */
	char switches_file[HMGT_CONFIG_PARAMS_FILENAME_SIZE]; /* Switches file for a Fabric */
	char topology_file[HMGT_CONFIG_PARAMS_FILENAME_SIZE]; /* Topology file for a Fabric  */
	char snmp_version[HMGT_MAX_STRING_SIZE]; /* Specifies version of SNMP to use */
	int  snmp_port; /* Specifies snmp port number to use */
	char snmp_community_string[HMGT_MAX_STRING_SIZE]; /* Specifies community string for SNMP V2 */
	char snmp_security_name[HMGT_MAX_STRING_SIZE]; /* Specifies a user name for SNMP session */
	char snmp_security_level[HMGT_MAX_STRING_SIZE]; /* Specifies the security level to configure for SNMP session */
	char snmp_auth_protocol[HMGT_MAX_STRING_SIZE]; /* Specifies the authentication protocol for SNMP session */
	char snmp_auth_passphrase[HMGT_MAX_STRING_SIZE]; /* Specifies the authentication passphrase for SNMP session */
	char snmp_encryp_protocol[HMGT_MAX_STRING_SIZE]; /* Specifies the encryption protocol for SNMP session */
	char snmp_encryp_passphrase[HMGT_MAX_STRING_SIZE]; /* Specifies the encryption passphrase for SNMP session */
} fabric_config_t;

/**
 * @brief Configuration settings used when opening an hmgt_port
 *
 * Optional advanced configuration options than can be passed to hmgt_port open
 * functions.
 *
 * Current default values when NULL pointer is passed to open functions is NULL,
 * meaning error and debug logging are disabled.
 *
 * error_file and debug_file can be specified as either an open linux FILE, or
 * can use the following special values:
 *   NULL: Disable output of this class of log messages
 *   HMGT_DBG_FILE_SYSLOG: Send this class of messages to syslog.
 *   Note: It is advisable, but not required, to call "openlog" prior to
 *     setting this option
 *
 * @note Enabling debug_file logging will generate a lot of data and may
 * overwhelm syslog. error_file logging with syslog will use the LOG_ERR
 * facility. debug_file logging with syslog will use the LOG_INFO facility.
 *
 */
struct hmgt_params {
	FILE            *error_file; /**File to send ERROR log messages to. */
	FILE            *debug_file; /**File to send DEBUG log messages to. */
	fabric_config_t *config_file_params; /* fabric configuration. */
};

typedef struct mgt_conf_s {
        fabric_config_t common; // the common config data
        boolean         common_processed; // indicate whether we have processed common data
        QUICK_LIST      fabric_confs; // fabrics config data
} mgt_conf_t;

/**
 * @brief Open an in-band hpamgt port by port GUID
 *
 * This function will allocate and initilize a connection to the local HFI using
 * the HFI device's Port GUID. Additionally, per port object logging can be
 * setup using session_params.
 *
 * @param port              port object is allocated and returned
 * @param port_guid         port GUID of the port
 * @param session_params    Optional advanced parameters to open port with (e.g.
 *  						Logging streams).
 *
 * @return HMGT_STATUS_T
 *
 * @see hmgt_params session_params
 * @see hmgt_close_port
 */
HMGT_STATUS_T hmgt_open_port_by_guid(struct hmgt_port **port, uint64_t port_guid, struct hmgt_params *session_params);

/**
 * @brief Close and free port object
 *
 * This function will close, disconnect, and free any previously allocated and
 * opened connections for both in-band and out-of-band port objects.
 *
 * @param port       port object to close and free.
 *
 * @see hmgt_open_port
 * @see hmgt_open_port_by_num
 * @see hmgt_open_port_by_guid
 * @see hmgt_oob_connect
 */
void hmgt_close_port(struct hmgt_port *port);


/** ============================================================================
 * hmgt_port accessor functions for use while in either in-band or out-of-band
 * mode
 */

/**
 * @brief Set debug logging output for an opamgt port
 *
 * Allows Dynamic modification of the debug log target file. Log Settings are
 * initially configured during port open and can be changed at any time with
 * this function. Target file can either be a standard linux flat FILE, NULL to
 * disable, or HMGT_DBG_FILE_SYSLOG to send debug logging to syslog.
 *
 * @param port   port instance to modify logging configuration
 * @param file   Target file for debug logging output
 *
 * @see hmgt_params
 */
void hmgt_set_dbg(struct hmgt_port *port, FILE *file);

/**
 * @brief Set error logging output for an opamgt port
 *
 * Allows Dynamic modification of the error log target file. Log Settings are
 * initially configured during port open and can be changed at any time with
 * this function. Target file can either be a standard Linux flat FILE, NULL to
 * disable, or HMGT_DBG_FILE_SYSLOG to send error logging to syslog.
 *
 * @param port   port instance to modify logging configuration
 * @param file   Target file for error logging
 *
 * @see hmgt_params
 */
void hmgt_set_err(struct hmgt_port *port, FILE *file);

/**
 * @brief Set query timeout for an opamgt port
 *
 * Allows Dynamic modification of the query timeout value. Timeout value is
 * initially set to HMGT_DEF_TIMEOUT_MS during port open and can be changed
 * at any time with this function.
 *
 * @param port        port instance to modify configuration.
 * @param ms_timeout  timeout value in milliseconds (ms). An invalid timeout
 *  				  value will reset timeout to default. Default timeout is
 *  				  1000 ms or 1 second.
 *
 * @see hmgt_params
 * @see HMGT_DEF_TIMEOUT_MS
 */
void hmgt_set_timeout(struct hmgt_port *port, int ms_timeout);

/**
 * @brief Set query retry count for an opamgt port
 *
 * Allows Dynamic modification of the query retry value. Retry value is
 * initially set to HMGT_DEF_RETRY_CNT during port open and can be changed
 * at any time with this function.
 *
 * @param port        port instance to modify configuration.
 * @param retry_count Number of times to retry query. An invalid retry count
 *  				  will reset to default. Default is 3.
 *
 * @see hmgt_params
 * @see HMGT_DEF_RETRY_CNT
 */
void hmgt_set_retry_count(struct hmgt_port *port, int retry_count);


/* OMGT Service State Values */
#define HMGT_SERVICE_STATE_UNKNOWN         0
#define HMGT_SERVICE_STATE_OPERATIONAL     1
#define HMGT_SERVICE_STATE_DOWN            (-1)
#define HMGT_SERVICE_STATE_UNAVAILABLE     (-2)

/* OMGT refresh values for how to get or update a service's state */
#define HMGT_REFRESH_SERVICE_NOP             0x00000000 /* Do not refresh */
#define HMGT_REFRESH_SERVICE_BAD_STATE       0x00000001 /* Only Refresh if state is not Operational */
#define HMGT_REFRESH_SERVICE_ANY_STATE       0x00000002 /* Refresh on any state */


/**
 * Pose a query to the fabric, expect a response.
 *
 * @param port           port opened by omgt_open_port_*
 * @param pQuery         pointer to the query structure
 * @param ppQueryResult  pointer where the response will go
 *
 * @return          0 if success, else error code
 */
HMGT_STATUS_T hmgt_query_fabric(struct hmgt_port *port,
					 HMGT_QUERY *pQuery,
					 HQUERY_RESULT_VALUES **ppQueryResult);

void hmgt_free_query_result_buffer(IN void * pQueryResult);
HMGT_STATUS_T hmgt_parse_config_file(const char *input_file, int quiet, int verbose, mgt_conf_t *params);

#ifdef __cplusplus
}
#endif

#endif /* __HPNMGT_H__ */
