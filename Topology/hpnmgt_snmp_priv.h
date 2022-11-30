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

#ifndef __HPNMGT_PRIV_SNMP_H__
#define __HPNMGT_PRIV_SNMP_H__

#include "hpnmgt_priv.h"
#include <iba/public/ilist.h>
#include <iba/stl_sd.h>
#include "topology.h"
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
#define FF_MAX_OID_LEN 24
#define FF_SNMP_VAL_LEN 64

typedef struct SNMPHost_s {
	uint8 type;
	char *name;
	char *community;
	int numInterface;
	char **interfaces;
} SNMPHost;

typedef struct SNMPOid_s {
	char *name;
	int type;
	oid oid[MAX_OID_LEN];
	size_t oidLen;
} SNMPOid;

typedef struct SNMPResult_s {
	oid oid[FF_MAX_OID_LEN];
	size_t oidLen;
	u_char type;
	u_char data[FF_SNMP_VAL_LEN];
	netsnmp_vardata val;
	size_t valLen;
	struct SNMPResult_s *next;
	boolean freeable;
} SNMPResult;

//-------- SNMP data process interface --------//
/*
 * @brief process SNMP data from a device. Can fill data into fabric data and/or
 *        return intermediate data for next phase data processing.
 * @param host	information about the host we are querying that may help us
 *            	figure out how to process data, e.g. NICs in the fabric
 * @param res	SNMP query results
 * @param fabric	fabric data
 * @return any intermediate data that will pass to the next phase data processing
 */
typedef void* (*snmp_device_data_process)(SNMPHost *host, SNMPResult *res, FabricData_t *fabric);

/*
 * @brief second phase data processing that intends to handle data at fabric level.
 *        E.g. figuring out link/topology in a fabric.
 * @param data	an array of intermediate data generated for individual devices by
 *            	the phase 1 data processors.
 * @param numHosts number of hosts
 * @param fabric	fabric data
 * @return FSTATUS	execution status
 */
typedef HMGT_STATUS_T (*snmp_fabric_data_process)(void** data, int numHosts,
		FabricData_t *fabric);

/*
 * @brief a device data processor shall cleanup resources within its routine. However
 *        for the generated intermediate data that is used outside the routine, we
 *        call this function to do clean up after the data is consumed.
 * @param data	intermediate data we want to cleanup
 * @return FSTATUS	execution status
 */
typedef HMGT_STATUS_T (*snmp_device_data_cleanup)(void* data);

//-------- End of SNMP data process interface --------//

int hmgt_snmp_init(void);
HMGT_STATUS_T hmgt_snmp_get_fabric_data(struct hmgt_port *port,
		HMGT_QUERY *pQuery, struct _HQUERY_RESULT_VALUES **ppQR);

#ifdef __cplusplus
}
;
#endif
#endif // __HPNMGT_PRIV_SNMP_H__

