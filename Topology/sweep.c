/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2015-2020, Intel Corporation

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

** END_ICS_COPYRIGHT7   ****************************************/

/* [ICS VERSION STRING: unknown] */

#include "topology.h"
#include "topology_internal.h"
#include "stl_helper.h"
#include <limits.h>
#include <math.h>
#include <time.h>
#include "hpnmgt_snmp_priv.h"
#ifdef DBGPRINT
#undef DBGPRINT
#endif
#define DBGPRINT(format, args...) if (g_verbose_file) {fflush(stdout); fprintf(stderr, format, ##args); }

static FILE *g_verbose_file = NULL;	// file for verbose output

static FSTATUS ParseRoutePoint(struct hmgt_port *port,
							   EUI64 portGuid, 
							   FabricData_t *fabricp, 
							   char* arg, 
							   Point* pPoint, 
							   char **pp)
{
#if 1 // HPN_PORT_OPA_FAST_FABRIC
	// TODO: Implement route focus point 
	fprintf(stderr, "%s: Route focus point not supported yet.\n",
						g_Top_cmdname);
	return FERROR;
#else
	Point SrcPoint;
	Point DestPoint;
	FSTATUS status;

	ASSERT(! PointValid(pPoint));
	PointInit(&SrcPoint);
	PointInit(&DestPoint);

	if (arg == *pp) {
		fprintf(stderr, "%s: Invalid route format: '%s'\n", g_Top_cmdname, arg);
		return FINVALID_PARAMETER;
	}
	status = ParsePoint(fabricp, arg, &SrcPoint, FIND_FLAG_FABRIC, pp);
	if (FSUCCESS != status)
		return status;
	if (**pp != ':') {
		fprintf(stderr, "%s: Invalid route format: '%s'\n", g_Top_cmdname, arg);
		return FINVALID_PARAMETER;
	}
	(*pp)++;
	status = ParsePoint(fabricp, *pp, &DestPoint, FIND_FLAG_FABRIC, pp);
	if (FSUCCESS != status)
		return status;

	// now we have 2 valid points, add to pPoint all the Ports in all routes
	// between those points
	/* TBD - cleanup use of global */
	status = FindPointsTraceRoutes(port, portGuid, fabricp, &SrcPoint, &DestPoint, pPoint);
	PointDestroy(&SrcPoint);
	PointDestroy(&DestPoint);
	if (FSUCCESS != status)
		return status;
	if (! PointValid(pPoint)) {
		fprintf(stderr, "%s: Unable to resolve route: '%s'\n",
					   	g_Top_cmdname, arg);
		return FNOT_FOUND;
	}
	PointCompress(pPoint);
	return FSUCCESS;
#endif // HPN_PORT_OPA_FAST_FABRIC
}

// focus point syntax also allows route: format
FSTATUS ParseFocusPoint(EUI64 portGuid, 
						FabricData_t *fabricp, 
						char* arg, 
						Point* pPoint, 
						uint8 find_flag,
						char **pp, 
						boolean allow_route)
{
	char* param;
	struct hmgt_port *hmgt_port_session = NULL;
	FSTATUS fstatus = FSUCCESS;

	*pp = arg;
	PointInit(pPoint);
	if (NULL != (param = ComparePrefix(arg, "route:"))) {
		if (! allow_route || ! (find_flag & FIND_FLAG_FABRIC)) {
			fprintf(stderr, "%s: Format Not Allowed: '%s'\n", g_Top_cmdname, arg);
			fstatus = FINVALID_PARAMETER;
		} else {
			fstatus = hmgt_open_port_by_guid(&hmgt_port_session, portGuid, NULL);
			if (fstatus != FSUCCESS) {
				fprintf(stderr, "%s: Unable to open fabric interface.\n",
						g_Top_cmdname);
			} else {
				hmgt_set_timeout(hmgt_port_session, fabricp->ms_timeout);
				fstatus = ParseRoutePoint(hmgt_port_session, portGuid, fabricp, 
										  param, pPoint, pp);
				hmgt_close_port(hmgt_port_session);
			}
		}
	} else {
		fstatus = ParsePoint(fabricp, arg, pPoint, find_flag, pp);
	}

	return fstatus;
}

/* query all NodeInfo Records on fabric connected to given HFI port
 * and put results into fabricp->AllNodes
 */
static FSTATUS SweepInternal(struct hmgt_port *port, 
						   EUI64 portGuid, 
						   FabricData_t *fabricp, 
						   SweepFlags_t flags, 
						   int quiet)
{
	HMGT_QUERY query;
	FSTATUS status = HMGT_STATUS_SUCCESS;
	PHQUERY_RESULT_VALUES pQueryResults = NULL;

	memset(&query, 0, sizeof(query));	// initialize reserved fields
	query.InputType 	= InputTypeFabricDataPtr;
	query.OutputType 	= OutputTypeHpnSnmpFabricDataRecord;
	query.InputValue.FabricDataRecord.FabricDataPtr = fabricp;

	if (! quiet) ProgressPrint(TRUE, "Getting All Fabric Records...");
	//DBGPRINT("Query: Input=%s, Output=%s\n",
	//			   		iba_sd_query_input_type_msg(query.InputType),
	//				   	iba_sd_query_result_type_msg(query.OutputType));

	// this call is synchronous
	status = hmgt_query_fabric(port, &query, &pQueryResults);

	if (! pQueryResults)
	{
		fprintf(stderr, "%*sFabric record query Failed: %s\n", 0, "", iba_fstatus_msg(status));
		goto fail;
	} else if (pQueryResults->Status != FSUCCESS) {
		fprintf(stderr, "%*sFabric record query Failed: %s MadStatus 0x%x: %s\n", 0, "",
				iba_fstatus_msg(pQueryResults->Status),
			   	pQueryResults->PacketStatus, "No error text message available"/*iba_sd_mad_status_msg(pQueryResults->MadStatus)*/);
		goto fail;
	} else if (pQueryResults->ResultDataSize == 0) {
		fprintf(stderr, "%*sNo Fabric Record Returned\n", 0, "");
	}
	if (! quiet) ProgressPrint(TRUE, "Done Getting All Fabric Records");
	BuildFabricDataLists(fabricp);

done:
	// omgt_query_sa will have allocated a result buffer
	// we must free the buffer when we are done with it
	if (pQueryResults)
		hmgt_free_query_result_buffer(pQueryResults);
	return status;

fail:
	status = FERROR;
	goto done;
}

FSTATUS InitSweepVerbose(FILE *verbose_file)
{
		g_verbose_file = verbose_file;
		return FSUCCESS;
}

// only FF_LIDARRAY fflag is used, others ignored
FSTATUS Sweep(EUI64 portGuid, FabricData_t *fabricp, FabricFlags_t fflags,  SweepFlags_t flags, int quiet, int ms_timeout, void *cfpp)
{
	FSTATUS fstatus;
	struct hmgt_port *hmgt_port_session = NULL;
	struct hmgt_params port_params;

	if (FSUCCESS != InitFabricData(fabricp, fflags)) {
		fprintf(stderr, "%s: Unable to initialize fabric storage area\n",
			g_Top_cmdname);
		return FERROR;
	}

	fabricp->ms_timeout = ms_timeout;

	// set port parameters and open the port
	memset(&port_params, 0, sizeof(port_params));
	port_params.config_file_params = (fabric_config_t *)cfpp;

	if (FSUCCESS != (fstatus = hmgt_open_port_by_guid(&hmgt_port_session, portGuid, &port_params))) {
		fprintf(stderr, "%s: Unable to open fabric interface.\n",
			g_Top_cmdname);
		goto done;;
	}
	hmgt_set_timeout(hmgt_port_session, fabricp->ms_timeout);

	time(&fabricp->time);

	// get the data from the fabric
	fstatus = SweepInternal(hmgt_port_session, portGuid, fabricp, flags, quiet);

done:
	hmgt_close_port(hmgt_port_session);
	return fstatus;
}
