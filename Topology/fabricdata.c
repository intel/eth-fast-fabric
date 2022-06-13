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
#include <stl_helper.h>



#define	VTIMER_1S		    1000000ull
#define VTIMER_1_MILLISEC           1000ull
#define VTIMER_10_MILLISEC          10000ull
#define VTIMER_100_MILLISEC         100000ull
#define VTIMER_200_MILLISEC         200000ull
#define VTIMER_500_MILLISEC         500000ull
#define RESP_WAIT_TIME		    1000

#define CL_MAX_THREADS              4

typedef struct clListSearchData_s {
   cl_map_item_t      AllListEntry; // key is numeric id
} clListSearchData_t; 

#ifndef __VXWORKS__
typedef struct clThreadContext_s {
   FabricData_t *fabricp; 
   clGraphData_t *graphp; 
   uint32 srcHcaListLength; 
   LIST_ITEM *srcHcaList;
   uint32 threadId; 
   int verbose;
   int quiet; 
   uint32 numVertices; 
   uint8 threadExit; 
   uint64_t sTime, eTime; 
   FSTATUS threadStatus;
   ValidateCLTimeGetCallback_t timeGetCallback;
   uint32 usedSLs;
} clThreadContext_t; 

pthread_mutex_t g_cl_lock; 

#endif
PortData *GetMapEntry(FabricData_t *fabricp, STL_LID lid)
{
	if (lid > TOPLM_LID_MAX) return NULL;
	if (!fabricp->u.LidMap.LidBlocks[TOPLM_BLOCK_NUM(lid)]) return NULL;
	return fabricp->u.LidMap.LidBlocks[TOPLM_BLOCK_NUM(lid)][TOPLM_ENTRY_NUM(lid)];
}

FSTATUS SetMapEntry(FabricData_t *fabricp, STL_LID lid, PortData *pd)
{
	if (lid > TOPLM_LID_MAX) return FERROR;
	if (!fabricp->u.LidMap.LidBlocks[TOPLM_BLOCK_NUM(lid)]) {
		if (!pd) return FSUCCESS;

		fabricp->u.LidMap.LidBlocks[TOPLM_BLOCK_NUM(lid)] =
			(PortData **)MemoryAllocate2AndClear(sizeof(PortData *)*(TOPLM_ENTRIES), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
		if (!fabricp->u.LidMap.LidBlocks[TOPLM_BLOCK_NUM(lid)]) {
			// Failed to allocate
			return FERROR;
		}
	}
	fabricp->u.LidMap.LidBlocks[TOPLM_BLOCK_NUM(lid)][TOPLM_ENTRY_NUM(lid)] = pd;
	return FSUCCESS;
}

void FreeLidMap(FabricData_t *fabricp)
{
	int i;
	for (i = 0; i < TOPLM_BLOCKS; i++) {
		if (fabricp->u.LidMap.LidBlocks[i]) {
			MemoryDeallocate(fabricp->u.LidMap.LidBlocks[i]);
			fabricp->u.LidMap.LidBlocks[i] = NULL;
		}
	}
}

// only FF_LIDARRAY,FF_PMADIRECT,FF_SMADIRECT,FF_DOWNPORTINFO,FF_CABLELOWPAGE
// flags are used, others ignored
FSTATUS InitFabricData(FabricData_t *fabricp, FabricFlags_t flags)
{
	MemoryClear(fabricp, sizeof(*fabricp));
	cl_qmap_init(&fabricp->AllNodes, NULL);
	if (!(flags & FF_LIDARRAY)) {
		cl_qmap_init(&fabricp->u.AllLids, NULL);
	}
	fabricp->ms_timeout = RESP_WAIT_TIME;
	fabricp->flags = flags & (FF_LIDARRAY|FF_PMADIRECT|FF_SMADIRECT|FF_DOWNPORTINFO|FF_CABLELOWPAGE);
	cl_qmap_init(&fabricp->AllSystems, NULL);
	cl_qmap_init(&fabricp->ExpectedNodeGuidMap, NULL);
	QListInitState(&fabricp->AllPorts);
	if (! QListInit(&fabricp->AllPorts)) {
		fprintf(stderr, "%s: Unable to initialize List\n", g_Top_cmdname);
		goto fail;
	}
	QListInitState(&fabricp->ExpectedLinks);
	if (! QListInit(&fabricp->ExpectedLinks)) {
		fprintf(stderr, "%s: Unable to initialize List\n", g_Top_cmdname);
		goto fail;
	}
	QListInitState(&fabricp->AllFIs);
	if (! QListInit(&fabricp->AllFIs)) {
		fprintf(stderr, "%s: Unable to initialize List\n", g_Top_cmdname);
		goto fail;
	}
	QListInitState(&fabricp->AllSWs);
	if (! QListInit(&fabricp->AllSWs)) {
		fprintf(stderr, "%s: Unable to initialize List\n", g_Top_cmdname);
		goto fail;
	}
	QListInitState(&fabricp->ExpectedFIs);
	if (! QListInit(&fabricp->ExpectedFIs)) {
		fprintf(stderr, "%s: Unable to initialize List\n", g_Top_cmdname);
		goto fail;
	}
	QListInitState(&fabricp->ExpectedSWs);
	if (! QListInit(&fabricp->ExpectedSWs)) {
		fprintf(stderr, "%s: Unable to initialize List\n", g_Top_cmdname);
		goto fail;
	}
	QListInitState(&fabricp->ExpectedSMs);
	if (! QListInit(&fabricp->ExpectedSMs)) {
		fprintf(stderr, "%s: Unable to initialize List\n", g_Top_cmdname);
		goto fail;
	}

	// MC routes related structures
	QListInitState(&fabricp->AllMcGroups);
	if (!QListInit(&fabricp->AllMcGroups)) {
		fprintf(stderr, "%s: Unable to initialize List of mcast Members\n", g_Top_cmdname);
		goto fail;
	}

        // credit-loop related lists
        cl_qmap_init(&fabricp->map_guid_to_ib_device, NULL); 
        QListInitState(&fabricp->FIs); 
        if (!QListInit(&fabricp->FIs)) {
           fprintf(stderr, "%s: Unable to initialize List\n", g_Top_cmdname); 
           goto fail;
        }
        QListInitState(&fabricp->Switches); 
        if (!QListInit(&fabricp->Switches)) {
           fprintf(stderr, "%s: Unable to initialize List\n", g_Top_cmdname); 
           goto fail;
        }
	// SNMP related lists
        cl_qmap_init(&fabricp->map_snmp_desc_to_node, NULL); 
        QListInitState(&fabricp->SnmpDiscoverHosts); 
        if (!QListInit(&fabricp->SnmpDiscoverHosts)) {
           fprintf(stderr, "%s: Unable to initialize List\n", g_Top_cmdname); 
           goto fail;
        }
        QListInitState(&fabricp->SnmpDiscoverSwitches); 
        if (!QListInit(&fabricp->SnmpDiscoverSwitches)) {
           fprintf(stderr, "%s: Unable to initialize List\n", g_Top_cmdname); 
           goto fail;
        }
	return FSUCCESS;

fail:
	if (flags & FF_LIDARRAY) {
		FreeLidMap(fabricp);
	}
	return FERROR;
}

// create SystemData as needed
// add this Node to the appropriate System
// This should only be invoked once per node (eg. not per NodeRecord)
FSTATUS AddSystemNode(FabricData_t *fabricp, NodeData *nodep)
{
	SystemData* systemp;
	cl_map_item_t *mi;
	FSTATUS status;
	uint64 key;

	systemp = (SystemData *)MemoryAllocate2AndClear(sizeof(SystemData), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! systemp) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		goto fail;
	}

	cl_qmap_init(&systemp->Nodes, NULL);
	systemp->SystemImageGUID = nodep->NodeInfo.SystemImageGUID;
	key = systemp->SystemImageGUID;
	if (! key)
		key = nodep->NodeInfo.NodeGUID;

	// There can be more than 1 node per system, only create 1 SystemData
	mi = cl_qmap_insert(&fabricp->AllSystems, key, &systemp->AllSystemsEntry);
	if (mi != &systemp->AllSystemsEntry)
	{
		MemoryDeallocate(systemp);
		systemp = PARENT_STRUCT(mi, SystemData, AllSystemsEntry);
	}

	nodep->systemp = systemp;
	if (cl_qmap_insert(&systemp->Nodes, nodep->NodeInfo.NodeGUID, &nodep->SystemNodesEntry) != &nodep->SystemNodesEntry)
	{
		fprintf(stderr, "%s: Duplicate NodeGuids found in nodeRecords: 0x%"PRIx64"\n",
					   g_Top_cmdname, nodep->NodeInfo.NodeGUID);
		goto fail;
	}
	status = FSUCCESS;
done:
	return status;

fail:
	status = FERROR;
	goto done;
}

/* build the fabricp->AllPorts, ALLFIs, and AllSWs lists such that
 * AllPorts is sorted by NodeGUID, PortNum
 * AllFIs, ALLSWs, AllIOUs is sorted by NodeGUID
 */
void BuildFabricDataLists(FabricData_t *fabricp)
{
	cl_map_item_t *p;

	// this list may already be filled when parsing Port definitions
	boolean AllPortsIsEmpty = QListIsEmpty(&fabricp->AllPorts);

	for (p=cl_qmap_head(&fabricp->AllNodes); p != cl_qmap_end(&fabricp->AllNodes); p = cl_qmap_next(p)) {
		NodeData *nodep = PARENT_STRUCT(p, NodeData, AllNodesEntry);
		cl_map_item_t *q;

		switch (nodep->NodeInfo.NodeType) {
		case STL_NODE_FI:
			QListInsertTail(&fabricp->AllFIs, &nodep->AllTypesEntry);
			break;
		case STL_NODE_SW:
			QListInsertTail(&fabricp->AllSWs, &nodep->AllTypesEntry);
			break;
		default:
			fprintf(stderr, "%s: Ignoring Unknown node type: 0x%x\n",
					g_Top_cmdname, nodep->NodeInfo.NodeType);
			break;
		}
#if !defined(VXWORKS) || defined(BUILD_DMC)
		if (nodep->ioup) {
			QListInsertTail(&fabricp->AllIOUs, &nodep->ioup->AllIOUsEntry);
		}
#endif

		if (AllPortsIsEmpty) {
			for (q=cl_qmap_head(&nodep->Ports); q != cl_qmap_end(&nodep->Ports); q = cl_qmap_next(q)) {
				PortData *portp = PARENT_STRUCT(q, PortData, NodePortsEntry);
				QListInsertTail(&fabricp->AllPorts, &portp->AllPortsEntry);
			}
		}
	}
}

PortSelector* GetPortSelector(PortData *portp)
{
	ExpectedLink *elinkp = portp->elinkp;
	if (elinkp) {
		if (elinkp->portp1 == portp)
			return elinkp->portselp1;
		if (elinkp->portp2 == portp)
			return elinkp->portselp2;
	}
	return NULL;
}

// Determine if portp and its neighbor are an internal link within a single
// system.  Note that some 3rd party products report SystemImageGuid of 0 in
// which case we can only consider links between the same node as internal links
boolean isInternalLink(PortData *portp)
{
	PortData *neighbor = portp->neighbor;

	return (neighbor
			&& portp->nodep->NodeInfo.SystemImageGUID
				== neighbor->nodep->NodeInfo.SystemImageGUID
			&& (portp->nodep->NodeInfo.SystemImageGUID
				|| portp->nodep->NodeInfo.NodeGUID
					== neighbor->nodep->NodeInfo.NodeGUID));
}

// Determine if portp and its neighbor are an Inter-switch Link
boolean isISLink(PortData *portp)
{
	PortData *neighbor = portp->neighbor;

	return (neighbor
			&& portp->nodep->NodeInfo.NodeType == STL_NODE_SW
			&& neighbor->nodep->NodeInfo.NodeType == STL_NODE_SW);
}

// Determine if portp or its neighbor is an NIC
boolean isFILink(PortData *portp)
{
	PortData *neighbor = portp->neighbor;

	return (neighbor
			&& (portp->nodep->NodeInfo.NodeType == STL_NODE_FI
				|| neighbor->nodep->NodeInfo.NodeType == STL_NODE_FI));
}

// The routines below are primarily for use when classifying ExpectedLinks
// which did not resolve properly.  So we want them to be a little loose
// and select a link which is expected to be of the given type based on input
// or where the port resolved for either side is connected to a real link of
// the given type.

// Due to this more inclusive definition, oriented toward verification,
// isExternal is not the same as ! isInternal
// Given this definition both isExternal and isInternal could be true for
// the same resolved or vaguely defined link, such as when the elinkp
// has not specified if its internal or external (default is ! internal)
// and the elink is resolved to two disconnected ports where one is an internal
// link and the other is an external link.
// Note: Currently there is no use case for an isInternalExpectedLink function
// and given the defaulting behavior of elinkp->internal such a function would
// not be implementable under this inclusive definition.
//
// Similarly isISExpectedLink is not the same as ! isFIExpectedLink
// isIS will include a link where expected link or either resolved real link
// have at least 1 switch to switch link, even if other expected or real links
// are NIC links.  Similarly isFIExpectedLink will match any link where an NIC is
// expected on either side or is found on either side of a resolved real link.
// As such the case of an expected link which is SW-SW which matches two
// unrelated ports where one port is part of a NIC-SW link and the other is
// part of a SW-SW link, will return true for both functions.
// Note portselp*->NodeType is optional (default NONE) and can resolve a port
// without matching NodeType.  So it is even possible for an expected link
// of SW-SW to resolve to a NIC-SW link (verify reports will report the
// incorrect node types as part of fully verifying the link)

// Determine if elinkp is an external link within a single system.
boolean isExternalExpectedLink(ExpectedLink *elinkp)
{
	return (
			// elinkp was specified as external (or Internal not specified)
			(! elinkp->internal)
			// either real port resolved to the elinkp is an external link
			|| (elinkp->portp1 && ! isInternalLink(elinkp->portp1))
			|| (elinkp->portp2 && ! isInternalLink(elinkp->portp2))
		);
}

// Determine if elinkp is an inter-switch link
boolean isISExpectedLink(ExpectedLink *elinkp)
{
	return (
			// elinkp was expicitly specified as an ISL
			((elinkp->portselp1 && elinkp->portselp1->NodeType == STL_NODE_SW)
			 && (elinkp->portselp2 && elinkp->portselp2->NodeType == STL_NODE_SW))
			// either real port resolved to the elinkp is an ISL link
			|| (elinkp->portp1 && isISLink(elinkp->portp1))
			|| (elinkp->portp2 && isISLink(elinkp->portp2))
		);
}

// Determine if elinkp is an NIC link
boolean isFIExpectedLink(ExpectedLink *elinkp)
{
	return (
			// either side of elinkp was specified as an NIC
			(elinkp->portselp1 && elinkp->portselp1->NodeType == STL_NODE_FI)
			|| (elinkp->portselp2 && elinkp->portselp2->NodeType == STL_NODE_FI)
			// either real port resolved to a link including an NIC
			|| (elinkp->portp1 && isFILink(elinkp->portp1))
			|| (elinkp->portp2 && isFILink(elinkp->portp2))
		);
}

// Lookup PKey
// ignores the Full/Limited bit, only checks low 15 bits for a match
// returns index of pkey in overall table or -1 if not found
// if the Partition Table for the port is not available, returns -1
int FindPKey(PortData *portp, uint16 pkey)
{
	uint16 ix, ix_capacity;
	STL_PKEY_ELEMENT *pPartitionTable = portp->pPartitionTable;

	if (! pPartitionTable)
		return -1;
	ix_capacity = PortPartitionTableSize(portp);
	for (ix = 0; ix < ix_capacity; ix++)
	{
		if ((pPartitionTable[ix].AsReg16 & 0x7FFF) == (pkey & 0x7FFF))
			return ix;
	}
	return -1;
}

// Determine if the given port is a member of the given vFabric
// We don't really have all the right data here, especially if the FM has
// combined multiple vFabrics into the same SL and PKey
// but for most cases, we can safely conclude that if the port has the SL and
// PKey configured it is a member of the vFabric.
// This function will return FALSE if QoS data or SL2SCMap is not available
// or if the port is not Armed/Active.
// Given the current FM implementation (and the dependency on SL2SCMap), this
// routine will return FALSE if invoked for non-endpoints
boolean isVFMember(PortData *portp, VFData_t *pVFData) 
{
	STL_VFINFO_RECORD *pR = &pVFData->record;
	uint8 sl = pR->s1.slBase;
	uint8 slResp = (pR->slResponseSpecified? pR->slResponse: sl);
	uint8 slMcast = (pR->slMulticastSpecified? pR->slMulticast: sl);

	// VF only valid if port initialized
	if (! IsEthPortInitialized(portp->PortInfo.PortStates))
		return FALSE;
	// there is no saquery to find out if a port is a member of a vfabric
	// so a port is assumed to be a member of a vfabric if:
	//	for the base SL of the vfabric, that port has an SC assigned (SC!=15)
	//	and the port has the VF's pkey

	if (! portp->pQOS || ! portp->pQOS->SL2SCMap)
		return FALSE;

	if (portp->pQOS->SL2SCMap->SLSCMap[sl].SC == 15 &&
		portp->pQOS->SL2SCMap->SLSCMap[slResp].SC == 15 &&
		portp->pQOS->SL2SCMap->SLSCMap[slMcast].SC == 15)
		return FALSE;

	return (-1 != FindPKey(portp, pR->pKey));
}

// count the number of armed/active links in the node
uint32 CountInitializedPorts(FabricData_t *fabricp, NodeData *nodep)
{
	cl_map_item_t *p;
	uint32 count = 0;
	for (p=cl_qmap_head(&nodep->Ports); p != cl_qmap_end(&nodep->Ports); p = cl_qmap_next(p))
	{
		PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
		if (IsEthPortInitialized(portp->PortInfo.PortStates))
			count++;
	}
	return count;
}

void PortDataFreeQOSData(FabricData_t *fabricp, PortData *portp)
{
	if (portp->pQOS) {
		QOSData *pQOS = portp->pQOS;

		LIST_ITEM *p;
		int i;
		for (i=0; i< SC2SCMAPLIST_MAX; i++) {
			while (!QListIsEmpty(&pQOS->SC2SCMapList[i])) {
				p = QListTail(&pQOS->SC2SCMapList[i]);
				PortMaskSC2SCMap *pSC2SC = (PortMaskSC2SCMap *)QListObj(p);
				QListRemoveTail(&pQOS->SC2SCMapList[i]);
				MemoryDeallocate(pSC2SC->SC2SCMap);
				pSC2SC->SC2SCMap = NULL;
				MemoryDeallocate(pSC2SC);
			}
		}

		if (pQOS->SL2SCMap)
			MemoryDeallocate(pQOS->SL2SCMap);
		if (pQOS->SC2SLMap)
			MemoryDeallocate(pQOS->SC2SLMap);

		MemoryDeallocate(pQOS);
	}
	portp->pQOS = NULL;
}

void PortDataFreeBufCtrlTable(FabricData_t *fabricp, PortData *portp)
{
	if (portp->pBufCtrlTable) {
		MemoryDeallocate(portp->pBufCtrlTable);
	}
	portp->pBufCtrlTable = NULL;
}

void PortDataFreePartitionTable(FabricData_t *fabricp, PortData *portp)
{
	if (portp->pPartitionTable) {
		MemoryDeallocate(portp->pPartitionTable);
	}
	portp->pPartitionTable = NULL;
}


void PortDataFreeCableInfoData(FabricData_t *fabricp, PortData *portp)
{
	if (portp->pCableInfoData) {
		MemoryDeallocate(portp->pCableInfoData);
	}
	portp->pCableInfoData = NULL;
}

void PortDataFreeCongestionControlTableEntries(FabricData_t *fabricp, PortData *portp)
{
	if (portp->pCongestionControlTableEntries) {
		MemoryDeallocate(portp->pCongestionControlTableEntries);
	}
	portp->pCongestionControlTableEntries = NULL;
}

void AllLidsRemove(FabricData_t *fabricp, PortData *portp)
{
	if (! portp->PortGUID)
		return;	// quietly ignore
	switch (portp->PortInfo.PortStates.s.PortState) {
	default:
	case ETH_PORT_DOWN:
		// may be in AllLids
		if (FindLid(fabricp, portp->EndPortLID) != portp)
			return;
		break;
	case ETH_PORT_DORMANT:
	case ETH_PORT_UP:
		// should be in AllLids
		break;
	}
	if (fabricp->flags & FF_LIDARRAY) {
		STL_LID first_lid = portp->EndPortLID;
		STL_LID last_lid = portp->EndPortLID |
					((1<<portp->PortInfo.s1.LMC)-1);
		while (first_lid <= last_lid) {
			SetMapEntry(fabricp, first_lid++, NULL);
		}
	} else {
		cl_qmap_remove_item(&fabricp->u.AllLids, &portp->AllLidsEntry);
	}
	fabricp->lidCount -= (1<<portp->PortInfo.s1.LMC);
}

void PartialLidsRemove(FabricData_t *fabricp,  PortData *portp, uint32 count)
{
	STL_LID first_lid = portp->EndPortLID;
	STL_LID last_lid = portp->EndPortLID + count;
	while (first_lid < last_lid) {
		SetMapEntry(fabricp, first_lid++, NULL);
		fabricp->lidCount--;
	}
}

// add the LID for a non-down port to the LID lists
// does nothing for ports in DOWN or INIT state
FSTATUS AllLidsAdd(FabricData_t *fabricp, PortData *portp, boolean force)
{
	cl_map_item_t *mi;

	if (!portp->PortGUID)
		return FSUCCESS;	// quietly ignore

	// we are less strict about switch port 0.  We have seen odd state's
	// reported and unfortunately SM just passes them along.
	// Since Port 0 is in SM DB it must have a trustable LID (note that
	// for simulator, PortDataStateChanged will only call this for Armed/Active)
	if (ETH_PORT_UP != portp->PortInfo.PortStates.s.PortState
		&& ETH_PORT_DORMANT != portp->PortInfo.PortStates.s.PortState
		&& ! (portp->nodep->NodeInfo.NodeType == STL_NODE_SW
				&& 0 == portp->PortNum
					&& ETH_PORT_NOP == portp->PortInfo.PortStates.s.PortState))
		return FSUCCESS;	// quietly ignore
	if (portp->EndPortLID > TOPLM_LID_MAX)
		return FERROR;
	if (fabricp->flags & FF_LIDARRAY) {
		STL_LID perm_first_lid = portp->EndPortLID;
		STL_LID first_lid = portp->EndPortLID;
		STL_LID last_lid = portp->EndPortLID |
					((1<<portp->PortInfo.s1.LMC)-1);
		FSTATUS status = FSUCCESS;

		for(;first_lid <= last_lid;first_lid++) {
			PortData *testportp = GetMapEntry(fabricp, first_lid);
			if(testportp != portp) {
				if(testportp != NULL) {
					status = FDUPLICATE;
					if (! force) {
						PartialLidsRemove(fabricp, portp, first_lid - perm_first_lid);
						return status;
					} else {
						AllLidsRemove(fabricp, testportp);
					}
				}
				fabricp->lidCount++;
				SetMapEntry(fabricp, first_lid, portp);
			}
		}
		return status;
	} else {
		// TBD does not verify potential overlap of lid+(1<<lmc)-1 range
		// lidCount will produce incorrect results in case of such overlap
		mi = cl_qmap_insert(&fabricp->u.AllLids, portp->EndPortLID, &portp->AllLidsEntry);
		if (mi != &portp->AllLidsEntry) {
			if (force) {
				AllLidsRemove(fabricp, PARENT_STRUCT(mi, PortData, AllLidsEntry));
				mi = cl_qmap_insert(&fabricp->u.AllLids, portp->EndPortLID, &portp->AllLidsEntry);
				ASSERT(mi == &portp->AllLidsEntry);
				fabricp->lidCount += (1<<portp->PortInfo.s1.LMC);
			}
			return FDUPLICATE;
		} else {
			fabricp->lidCount += (1<<portp->PortInfo.s1.LMC);
			return FSUCCESS;
		}
	}
}

STL_SCSCMAP * QOSDataLookupSCSCMap(PortData *portp, uint8_t outport, int extended) {
	LIST_ITEM *p;
	PortMaskSC2SCMap *pSC2SC;
	QOSData *pQOS = portp->pQOS;

	if (!pQOS)
		return NULL;
	if ((extended >= SC2SCMAPLIST_MAX) || (extended < 0))
		return NULL;
 
	for (p = QListHead(&pQOS->SC2SCMapList[extended]); p != NULL; p = QListNext(&pQOS->SC2SCMapList[extended], p)) {
		pSC2SC = (PortMaskSC2SCMap *)QListObj(p);

		if (StlIsPortInPortMask(pSC2SC->outports, outport))
			return (pSC2SC->SC2SCMap);
	}
	return NULL;
}

// Add a SCSC table to QOS data
// If a matching SCSC table already exists, add egress to its port mask
// Otherwise, create a new entry in the SCSCMap list for this table
void QOSDataAddSCSCMap(PortData *portp, uint8_t outport, int extended, const STL_SCSCMAP *pSCSC) {
	LIST_ITEM *p;
	QOSData *pQOS = portp->pQOS;
	PortMaskSC2SCMap *pSC2SC2;
	PortMaskSC2SCMap *pEmptySC2SC2 = NULL;
	int entryFound = 0;

	if (!pQOS)
		return;
	if ((extended >= SC2SCMAPLIST_MAX) || (extended < 0))
		return;

	for (p = QListHead(&pQOS->SC2SCMapList[extended]); p != NULL; p = QListNext(&pQOS->SC2SCMapList[extended], p)) {
		pSC2SC2 = (PortMaskSC2SCMap *)QListObj(p);

		if (!memcmp(pSC2SC2->SC2SCMap, pSCSC, sizeof(STL_SCSCMAP))) {
			StlAddPortToPortMask(pSC2SC2->outports, outport);
			entryFound = 1;
		} else if (StlIsPortInPortMask(pSC2SC2->outports, outport)) {
			// the maps don't match but the port does, remove & replace with new map
			StlClearPortInPortMask(pSC2SC2->outports, outport);
			if (StlNumPortsSetInPortMask(pSC2SC2->outports, portp->nodep->NodeInfo.NumPorts) ==0) {
				pEmptySC2SC2 = pSC2SC2;
			}
		}
	}

	if (entryFound) {
		if(pEmptySC2SC2) {
			QListRemoveItem(&pQOS->SC2SCMapList[extended], &pEmptySC2SC2->SC2SCMapListEntry);
		}
		return;
	}

	pSC2SC2 = pEmptySC2SC2;
	if (!pSC2SC2) {
		// never found a matching map, create a new one
		pSC2SC2 = (PortMaskSC2SCMap *)MemoryAllocate2AndClear(sizeof(PortMaskSC2SCMap), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
		if (!pSC2SC2) {
			// memory error
			fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
			return;
		}

		ListItemInitState(&pSC2SC2->SC2SCMapListEntry);
		QListSetObj(&pSC2SC2->SC2SCMapListEntry, pSC2SC2);

		pSC2SC2->SC2SCMap = (STL_SCSCMAP *)MemoryAllocate2AndClear(sizeof(STL_SCSCMAP), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
		if (!pSC2SC2->SC2SCMap) {
			// memory error
			fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
			MemoryDeallocate(pSC2SC2);
			return;
		}
		QListInsertTail(&pQOS->SC2SCMapList[extended], &pSC2SC2->SC2SCMapListEntry);
	}

	memcpy(pSC2SC2->SC2SCMap, pSCSC, sizeof(STL_SCSCMAP));
	StlAddPortToPortMask(pSC2SC2->outports, outport);
}

// Set new Port Info based on a Set(PortInfo).  For use by fabric simulator
// assumes pInfo already validated and any Noop fields filled in with correct
// values.
// MODIFIED FOR STL
void PortDataStateChanged(FabricData_t *fabricp, PortData *portp)
{
	if (ETH_PORT_UP == portp->PortInfo.PortStates.s.PortState
		|| ETH_PORT_DORMANT == portp->PortInfo.PortStates.s.PortState) {
		if (FSUCCESS != AllLidsAdd(fabricp, portp, TRUE)) {
			fprintf(stderr, "%s: Overwrote Duplicate LID found in portRecords: PortGUID 0x%016"PRIx64" LID 0x%x Port %u Node %.*s\n",
				g_Top_cmdname,
				portp->PortGUID, portp->EndPortLID,
				portp->PortNum, STL_NODE_DESCRIPTION_ARRAY_SIZE,
				(char*)portp->nodep->NodeDesc.NodeString);
		}
	} else {
		AllLidsRemove(fabricp, portp);
	}

}

// Set new Port Info based on a Set(PortInfo).  For use by fabric simulator
// assumes pInfo already validated and any Noop fields filled in with correct
// values.
// MODIFIED FOR STL
void PortDataSetPortInfo(FabricData_t *fabricp, PortData *portp, STL_PORT_INFO *pInfo)
{
	portp->PortInfo = *pInfo;
	if (portp->PortGUID) {
		portp->EndPortLID = pInfo->LID;
		/* Only port 0 in a switch has a GUID.  It's LID is the switch's LID */
		if (portp->nodep->pSwitchInfo) {
			cl_map_item_t *q;
			NodeData *nodep = portp->nodep;

			nodep->pSwitchInfo->RID.LID = pInfo->LID;
			/* also set EndPortLID in all of switch's PortInfoRecords */
			for (q=cl_qmap_head(&nodep->Ports); q != cl_qmap_end(&nodep->Ports); q = cl_qmap_next(q)) {
				PortData *p = PARENT_STRUCT(q, PortData, NodePortsEntry);
				p->EndPortLID = pInfo->LID;
			}
		}
	}
	PortDataStateChanged(fabricp, portp);
}

// remove Port from lists and free it
// Only removes from AllLids and NodeData.Ports, caller must remove from
// any other lists if called after BuildFabricLists
void PortDataFree(FabricData_t *fabricp, PortData *portp)
{
	NodeData *nodep = portp->nodep;

	if (portp->context && g_Top_FreeCallbacks.pPortDataFreeCallback)
		(*g_Top_FreeCallbacks.pPortDataFreeCallback)(fabricp, portp);

	if (portp->PortGUID)
		AllLidsRemove(fabricp, portp);
	cl_qmap_remove_item(&nodep->Ports, &portp->NodePortsEntry);
	if (portp->pPortCounters)
		MemoryDeallocate(portp->pPortCounters);
	PortDataFreeQOSData(fabricp, portp);
	PortDataFreeBufCtrlTable(fabricp, portp);
	PortDataFreePartitionTable(fabricp, portp);


	PortDataFreeCableInfoData(fabricp, portp);
	PortDataFreeCongestionControlTableEntries(fabricp, portp);
	MemoryDeallocate(portp);
}

FSTATUS PortDataAllocateQOSData(FabricData_t *fabricp, PortData *portp)
{
	QOSData *pQOS;
	int i;

	ASSERT(! portp->pQOS);	// or could free if present
	portp->pQOS = (QOSData *)MemoryAllocate2AndClear(sizeof(QOSData), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! portp->pQOS) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		goto fail;
	}
	pQOS = portp->pQOS;
	if (portp->nodep->NodeInfo.NodeType == STL_NODE_SW && portp->PortNum) {
		for (i=0; i<SC2SCMAPLIST_MAX; i++) {
		// external switch ports get SC2SC map
			QListInitState(&pQOS->SC2SCMapList[i]);
			if (!QListInit(&pQOS->SC2SCMapList[i])) {
				fprintf(stderr, "%s: Unable to initialize SC2SCMaps member list\n", g_Top_cmdname);
				goto fail;
			}
		}
	} else {
		// HFI and Switch Port 0 get SL2SC and SC2SL
		pQOS->SL2SCMap = (STL_SLSCMAP *)MemoryAllocate2AndClear(sizeof(STL_SLSCMAP), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
		if (! pQOS->SL2SCMap) {
			fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
			goto fail;
		}

		pQOS->SC2SLMap = (STL_SCSLMAP *)MemoryAllocate2AndClear(sizeof(STL_SCSLMAP), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
		if (!pQOS->SC2SLMap) {
			goto fail;
		}
	}

	return FSUCCESS;
fail:
	PortDataFreeQOSData(fabricp, portp);
	return FINSUFFICIENT_MEMORY;
}

FSTATUS PortDataAllocateAllQOSData(FabricData_t *fabricp)
{
	LIST_ITEM *p;
	FSTATUS status = FSUCCESS;

	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);
		FSTATUS s;
		s = PortDataAllocateQOSData(fabricp, portp);
		if (FSUCCESS != s)
			status = s;
	}
	return status;
}

FSTATUS PortDataAllocateBufCtrlTable(FabricData_t *fabricp, PortData *portp)
{
	ASSERT(! portp->pBufCtrlTable);	// or could free if present
	portp->pBufCtrlTable = MemoryAllocate2AndClear(sizeof(STL_BUFFER_CONTROL_TABLE), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! portp->pBufCtrlTable) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		goto fail;
	}

	return FSUCCESS;
fail:
	//PortDataFreeBufCtrlTable(fabricp, portp);
	return FINSUFFICIENT_MEMORY;
}

FSTATUS PortDataAllocateAllBufCtrlTable(FabricData_t *fabricp)
{
	LIST_ITEM *p;
	FSTATUS status = FSUCCESS;

	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);
		FSTATUS s;
		s = PortDataAllocateBufCtrlTable(fabricp, portp);
		if (FSUCCESS != s)
			status = s;
	}
	return status;
}

uint16 PortPartitionTableSize(PortData *portp)
{
	NodeData *nodep = portp->nodep;
	if (nodep->NodeInfo.NodeType == STL_NODE_SW && portp->PortNum) {
		// Switch External Ports table size is defined in SwitchInfo
		if (! nodep->pSwitchInfo) {
			// guess the limits, we don't have SwitchInfo
			// or we haven't yet read it from the snapshot while parsing
			return nodep->NodeInfo.PartitionCap;
		} else {
			return nodep->pSwitchInfo->SwitchInfoData.PartitionEnforcementCap;
		}
	} else {
		// Switch Port 0 and NIC table size is defined by NodeInfo
		return nodep->NodeInfo.PartitionCap;
	}
}

FSTATUS PortDataAllocatePartitionTable(FabricData_t *fabricp, PortData *portp)
{
	uint16 size;

	ASSERT(! portp->pPartitionTable);	// or could free if present
	size = PortPartitionTableSize(portp);
	portp->pPartitionTable = (STL_PKEY_ELEMENT *)MemoryAllocate2AndClear(sizeof(STL_PKEY_ELEMENT)*size, IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! portp->pPartitionTable) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		goto fail;
	}

	return FSUCCESS;
fail:
	//PortDataFreePartitionTable(fabricp, portp);
	return FINSUFFICIENT_MEMORY;
}

FSTATUS PortDataAllocateAllPartitionTable(FabricData_t *fabricp)
{
	LIST_ITEM *p;
	FSTATUS status = FSUCCESS;

	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);
		FSTATUS s;
		s = PortDataAllocatePartitionTable(fabricp, portp);
		if (FSUCCESS != s)
			status = s;
	}
	return status;
}


FSTATUS PortDataAllocateCableInfoData(FabricData_t *fabricp, PortData *portp)
{
	uint16 size = STL_CIB_STD_LEN;
	ASSERT(! portp->pCableInfoData);	// or could free if present

	//Data in Low address space of Cable info is also accesed for Cable Health Report
	if (fabricp) {
		if (fabricp->flags & FF_CABLELOWPAGE)
			size = STL_CABLE_INFO_DATA_SIZE * 4;    // 2 blocks of lower page 0 and 2 blocks of upper page 0
	}

	portp->pCableInfoData = MemoryAllocate2AndClear(size, IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! portp->pCableInfoData) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		goto fail;
	}

	return FSUCCESS;
fail:
	//PortDataFreeCableInfoData(fabricp, portp);
	return FINSUFFICIENT_MEMORY;
}

FSTATUS PortDataAllocateAllCableInfo(FabricData_t *fabricp)
{
	LIST_ITEM *p;
	FSTATUS status = FSUCCESS;

	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);
		FSTATUS s;
		// skip switch port 0
		if (! portp->PortNum)
			continue;
		s = PortDataAllocateCableInfoData(fabricp, portp);
		if (FSUCCESS != s)
			status = s;
	}
	return status;
}

FSTATUS PortDataAllocateCongestionControlTableEntries(FabricData_t *fabricp, PortData *portp)
{
	ASSERT(! portp->pCongestionControlTableEntries);	// or could free if present
	if (! portp->nodep->CongestionInfo.ControlTableCap)
		return FSUCCESS;
	portp->pCongestionControlTableEntries = MemoryAllocate2AndClear(
							sizeof(STL_HFI_CONGESTION_CONTROL_TABLE_ENTRY)
								* STL_NUM_CONGESTION_CONTROL_ELEMENTS_BLOCK_ENTRIES
								* portp->nodep->CongestionInfo.ControlTableCap,
							IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! portp->pCongestionControlTableEntries) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		goto fail;
	}

	return FSUCCESS;
fail:
	//PortDataFreeCongestionControlTableEntries(fabricp, portp);
	return FINSUFFICIENT_MEMORY;
}

FSTATUS PortDataAllocateAllCongestionControlTableEntries(FabricData_t *fabricp)
{
	LIST_ITEM *p;
	FSTATUS status = FSUCCESS;

	for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
		PortData *portp = (PortData *)QListObj(p);
		FSTATUS s;
		// only applicable to HFIs and enhanced switch port 0
		if (portp->nodep->NodeInfo.NodeType == STL_NODE_SW
			&& portp->PortNum)
			continue;
		if (! portp->nodep->CongestionInfo.ControlTableCap)
			continue;
		s = PortDataAllocateCongestionControlTableEntries(fabricp, portp);
		if (FSUCCESS != s)
			status = s;
	}
	return status;
}

// guid is the PortGUID as found in corresponding NodeRecord
// we adjust as needed to account for Switch Ports (only switch port 0 has guid)
PortData* NodeDataAddPort(FabricData_t *fabricp, NodeData *nodep, EUI64 guid, STL_PORTINFO_RECORD *pPortInfo)
{
	PortData *portp;

	portp = (PortData*)MemoryAllocate2AndClear(sizeof(PortData), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! portp) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		goto fail;
	}

	portp->pPortCounters = (STL_PORT_COUNTERS_DATA *)MemoryAllocate2AndClear(sizeof(STL_PORT_COUNTERS_DATA),
		IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! portp->pPortCounters) {
		fprintf(stderr, "%s: Unable to allocate memory for Port Counters\n", g_Top_cmdname);
		MemoryDeallocate(portp);
		goto fail;
	}

	ListItemInitState(&portp->AllPortsEntry);
	QListSetObj(&portp->AllPortsEntry, portp);
	portp->PortInfo = pPortInfo->PortInfo;
	portp->EndPortLID = pPortInfo->RID.EndPortLID;
	memcpy(portp->LinkDownReasons, pPortInfo->LinkDownReasons, STL_NUM_LINKDOWN_REASONS * sizeof(STL_LINKDOWN_REASON));

	if (nodep->NodeInfo.NodeType == STL_NODE_SW) {
		// a switch only gets 1 port Guid, we save it for switch
		// port 0 (the "virtual management port")
		portp->PortNum = pPortInfo->RID.PortNum;
		portp->PortGUID = portp->PortNum?0:guid;
	} else {
		portp->PortNum = pPortInfo->PortInfo.LocalPortNum;
		portp->PortGUID = guid;
	}

	portp->rate = EthIfSpeedToStaticRate(portp->PortInfo.IfSpeed);
	portp->nodep = nodep;
	//DisplayPortInfoRecord(&pPortInfoRecords[i], 0);
	//printf("process PortNumber: %u\n", portp->PortNum);

	if (cl_qmap_insert(&nodep->Ports, portp->PortNum, &portp->NodePortsEntry) != &portp->NodePortsEntry)
	{
		fprintf(stderr, "%s: Duplicate PortNums found in portRecords: LID 0x%x Port %u Node: %.*s\n",
					   	g_Top_cmdname,
					   	portp->EndPortLID,
					   	portp->PortNum, STL_NODE_DESCRIPTION_ARRAY_SIZE,
						(char*)nodep->NodeDesc.NodeString);
		MemoryDeallocate(portp);
		goto fail;
	}
	if (FSUCCESS != AllLidsAdd(fabricp, portp, FALSE))
	{
		fprintf(stderr, "%s: Duplicate LIDs found in portRecords: PortGUID 0x%016"PRIx64" LID 0x%x Port %u Node %.*s\n",
			   	g_Top_cmdname,
			   	portp->PortGUID, portp->EndPortLID,
			   	portp->PortNum, STL_NODE_DESCRIPTION_ARRAY_SIZE,
				(char*)nodep->NodeDesc.NodeString);
		cl_qmap_remove_item(&nodep->Ports, &portp->NodePortsEntry);
		MemoryDeallocate(portp);
		goto fail;
	}
	//DisplayPortInfoRecord(pPortInfo, 0);

	return portp;

fail:
	return NULL;
}

FSTATUS NodeDataSetSwitchInfo(NodeData *nodep, STL_SWITCHINFO_RECORD *pSwitchInfo)
{
	ASSERT(! nodep->pSwitchInfo);
	nodep->pSwitchInfo = (STL_SWITCHINFO_RECORD*)MemoryAllocate2AndClear(sizeof(STL_SWITCHINFO_RECORD), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! nodep->pSwitchInfo) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		return FERROR;
	}
	// leave the extra vendor specific fields zeroed
	nodep->pSwitchInfo->RID.LID = pSwitchInfo->RID.LID;
	nodep->pSwitchInfo->SwitchInfoData = pSwitchInfo->SwitchInfoData;
	return FSUCCESS;
}

// TBD - routines to add/set IOC and IOU information

NodeData *FabricDataAddNode(FabricData_t *fabricp, STL_NODE_RECORD *pNodeRecord, boolean *new_nodep)
{
	NodeData *nodep = (NodeData*)MemoryAllocate2AndClear(sizeof(NodeData), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	cl_map_item_t *mi;
	boolean new_node = TRUE;

	if (! nodep) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		goto fail;
	}
	//printf("process NodeRecord LID: 0x%x\n", pNodeRecords->RID.s.LID);
	//DisplayNodeRecord(pNodeRecord, 0);

	cl_qmap_init(&nodep->Ports, NULL);

        nodep->NodeInfo = pNodeRecord->NodeInfo;

	nodep->NodeDesc = pNodeRecord->NodeDesc;

	// zero out port specific fields, the PortData will handle these
	nodep->NodeInfo.PortGUID=0;
	nodep->NodeInfo.u1.s.LocalPortNum=0;
	ListItemInitState(&nodep->AllTypesEntry);
	QListSetObj(&nodep->AllTypesEntry, nodep);

	// when sweeping we get 1 NodeRecord per port on a node, so only save
	// 1 NodeData structure per node and discard the duplicates
	mi = cl_qmap_insert(&fabricp->AllNodes, nodep->NodeInfo.NodeGUID, &nodep->AllNodesEntry);
	if (mi != &nodep->AllNodesEntry)
	{
		MemoryDeallocate(nodep);
		nodep = PARENT_STRUCT(mi, NodeData, AllNodesEntry);
		new_node = FALSE;
	}

	if (new_node) {
		if (FSUCCESS != AddSystemNode(fabricp, nodep)) {
			cl_qmap_remove_item(&fabricp->AllNodes, &nodep->AllNodesEntry);
			MemoryDeallocate(nodep);
			goto fail;
		}
	}

	if (new_nodep)
		*new_nodep = new_node;
	return nodep;

fail:
	return NULL;
}


FSTATUS AddEdgeSwitchToGroup(FabricData_t *fabricp, McGroupData *mcgroupp, NodeData *groupswitch, uint8 SWentryport)
{
	LIST_ITEM *p;
	boolean found;
	FSTATUS status;

	// this linear insertion needs to be optimized
	found = FALSE;
	p=QListHead(&mcgroupp->EdgeSwitchesInGroup);
	while (!found && (p != NULL)) {
		McEdgeSwitchData *pSW = (McEdgeSwitchData *)QListObj(p);
		if	(pSW->NodeGUID == groupswitch->NodeInfo.NodeGUID)
			found = TRUE;
		else
			p = QListNext(&mcgroupp->EdgeSwitchesInGroup, p);
	}
	if (!found) {
			McEdgeSwitchData *mcsw = (McEdgeSwitchData*)MemoryAllocate2AndClear(sizeof(McEdgeSwitchData), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
			if (! mcsw) {
				status = FINSUFFICIENT_MEMORY;
				return status;
			}
			mcsw->NodeGUID = groupswitch->NodeInfo.NodeGUID;
			mcsw->pPort = FindPortGuid(fabricp, groupswitch->NodeInfo.NodeGUID );
			mcsw->EntryPort = SWentryport;
			QListSetObj(&mcsw->McEdgeSwitchEntry, mcsw);
			QListInsertTail(&mcgroupp->EdgeSwitchesInGroup, &mcsw->McEdgeSwitchEntry);
	}
	return FSUCCESS;
}

FSTATUS FabricDataAddLink(FabricData_t *fabricp, PortData *p1, PortData *p2)
{
	// The Link Records will be reported in both directions
	// so discard duplicates
	if (p1->neighbor == p2 && p2->neighbor == p1)
		goto fail;
	if (p1->neighbor) {
		fprintf(stderr, "%s: Skipping Duplicate Link Record reference to port: LID 0x%x Port %u Node %.*s\n",
			   	g_Top_cmdname,
			   	p1->EndPortLID,
			   	p1->PortNum, NODE_DESCRIPTION_ARRAY_SIZE,
				(char*)p1->nodep->NodeDesc.NodeString);
		goto fail;
	}
	if (p2->neighbor) {
		fprintf(stderr, "%s: Skipping Duplicate Link Record reference to port: LID 0x%x Port %u Node %.*s\n",
			   	g_Top_cmdname,
			   	p2->EndPortLID,
			   	p2->PortNum, NODE_DESCRIPTION_ARRAY_SIZE,
				(char*)p2->nodep->NodeDesc.NodeString);
		goto fail;
	}
	//DisplayLinkRecord(pLinkRecord, 0);
	p1->neighbor = p2;
	p2->neighbor = p1;
	// for consistency between runs, we always report the "from" port
	// as the one with lower numbered NodeGUID/PortNum
	if (p1->nodep->NodeInfo.NodeGUID != p2->nodep->NodeInfo.NodeGUID) {
		if (p1->nodep->NodeInfo.NodeGUID < p2->nodep->NodeInfo.NodeGUID) {
			p1->from = 1;
			p2->from = 0;
		} else {
			p1->from = 0;
			p2->from = 1;
		}
	} else {
		if (p1->PortNum < p2->PortNum) {
			p1->from = 1;
			p2->from = 0;
		} else {
			p1->from = 0;
			p2->from = 1;
		}
	}
	if (p1->rate != p2->rate) {
		fprintf(stderr, "\n%s: Warning: Ignoring Inconsistent Active Speed/Width for link between:\n", g_Top_cmdname);
		fprintf(stderr, "  %4s 0x%016"PRIx64" %3u %s %.*s\n",
				StlStaticRateToText(p1->rate),
				p1->nodep->NodeInfo.NodeGUID,
				p1->PortNum,
				StlNodeTypeToText(p1->nodep->NodeInfo.NodeType),
				NODE_DESCRIPTION_ARRAY_SIZE,
				(char*)p1->nodep->NodeDesc.NodeString);
		fprintf(stderr, "  %4s 0x%016"PRIx64" %3u %s %.*s\n",
				StlStaticRateToText(p2->rate),
				p2->nodep->NodeInfo.NodeGUID,
				p2->PortNum,
				StlNodeTypeToText(p2->nodep->NodeInfo.NodeType),
				NODE_DESCRIPTION_ARRAY_SIZE,
				(char*)p2->nodep->NodeDesc.NodeString);
	}
	++(fabricp->LinkCount);
	if (! isInternalLink(p1))
		++(fabricp->ExtLinkCount);
	if (isFILink(p1))
		++(fabricp->FILinkCount);
	if (isISLink(p1))
		++(fabricp->ISLinkCount);
	if (! isInternalLink(p1)&& isISLink(p1))
		++(fabricp->ExtISLinkCount);

	return FSUCCESS;

fail:
	return FERROR;
}

FSTATUS FabricDataAddLinkRecord(FabricData_t *fabricp, STL_LINK_RECORD *pLinkRecord)
{
	PortData *p1, *p2;

	//printf("process LinkRecord LID: 0x%x\n", pLinkRecord->RID.s.LID);
	p1 = FindLidPort(fabricp, pLinkRecord->RID.FromLID, pLinkRecord->RID.FromPort);
	if (! p1) {
		fprintf(stderr, "%s: Can't find \"From\" Lid 0x%x Port %u: Skipping\n",
				g_Top_cmdname,
				pLinkRecord->RID.FromLID, pLinkRecord->RID.FromPort);
		return FERROR;
	}
	p2 = FindLidPort(fabricp, pLinkRecord->ToLID, pLinkRecord->ToPort);
	if (! p2) {
		fprintf(stderr, "%s: Can't find \"To\" Lid 0x%x Port %u: Skipping\n",
				g_Top_cmdname,
				pLinkRecord->ToLID, pLinkRecord->ToPort);
		return FERROR;
	}
	return FabricDataAddLink(fabricp, p1, p2);
}

// This allows a link to be removed, its mainly a helper for manual
// construction of topologies which need to "undo links" they created
FSTATUS FabricDataRemoveLink(FabricData_t *fabricp, PortData *p1)
{
	PortData *p2 = p1->neighbor;

	if (! p1->neighbor)
		goto fail;
	if (p1 == p2) {
		fprintf(stderr, "%s: Corrupted Topology, port linked to self: LID 0x%x Port %u Node %.*s\n",
			   	g_Top_cmdname,
			   	p1->EndPortLID,
			   	p1->PortNum, NODE_DESCRIPTION_ARRAY_SIZE,
				(char*)p1->nodep->NodeDesc.NodeString);
		goto fail;
	}
	if (p2->neighbor != p1) {
		fprintf(stderr, "%s: Corrupted Topology, port linked inconsistently: LID 0x%x Port %u Node %.*s\n",
			   	g_Top_cmdname,
			   	p1->EndPortLID,
			   	p1->PortNum, NODE_DESCRIPTION_ARRAY_SIZE,
				(char*)p1->nodep->NodeDesc.NodeString);
		goto fail;
	}
	--(fabricp->LinkCount);
	if (! isInternalLink(p1))
		--(fabricp->ExtLinkCount);
	if (isFILink(p1))
		--(fabricp->FILinkCount);
	if (isISLink(p1))
		--(fabricp->ISLinkCount);
	if (! isInternalLink(p1)&& isISLink(p1))
		--(fabricp->ExtISLinkCount);
	p1->rate = 0;
	p1->neighbor = NULL;
	p1->from = 0;
	p2->rate = 0;
	p2->neighbor = NULL;
	p2->from = 0;

	return FSUCCESS;

fail:
	return FERROR;
}
#if !defined(VXWORKS) || defined(BUILD_DMC)
// remove Ioc from lists and free it
// Only removes from AllIOCs and IouData.Iocs, caller must remove from
// any other lists if called after BuildFabricLists
void IocDataFree(FabricData_t *fabricp, IocData *iocp)
{
	IouData *ioup = iocp->ioup;

	if (iocp->context && g_Top_FreeCallbacks.pIocDataFreeCallback)
		(*g_Top_FreeCallbacks.pIocDataFreeCallback)(fabricp, iocp);

	QListRemoveItem(&ioup->Iocs, &iocp->IouIocsEntry);
	cl_qmap_remove_item(&fabricp->AllIOCs, &iocp->AllIOCsEntry);
	if (iocp->Services)
		MemoryDeallocate(iocp->Services);
	MemoryDeallocate(iocp);
}

// Free all parsed Iocs for this Iou
// Only removes Iocs from AllIOCs and IouData.Iocs, caller must remove from
// any other lists if called after BuildFabricLists
void IouDataFreeIocs(FabricData_t *fabricp, IouData *ioup)
{
	LIST_ITEM *p;

	// this list is sorted/keyed by NodeGUID
	for (p=QListHead(&ioup->Iocs); p != NULL; p = QListHead(&ioup->Iocs)) {
		IocDataFree(fabricp, (IocData*)QListObj(p));
	}
}

// remove Iou from NodeData.ioup and free it and its IOCs
// Only removes Iocs from AllIOCs and IouData.Iocs, caller must remove from
// any other lists if called after BuildFabricLists
void IouDataFree(FabricData_t *fabricp, IouData *ioup)
{
	if (ioup->context && g_Top_FreeCallbacks.pIouDataFreeCallback)
		(*g_Top_FreeCallbacks.pIouDataFreeCallback)(fabricp, ioup);

	IouDataFreeIocs(fabricp, ioup);
	if (ioup->nodep && ioup->nodep->ioup)
		ioup->nodep->ioup = NULL;
	MemoryDeallocate(ioup);
}
#endif
// Free all ports for this node
// Only removes ports from AllLids and NodeData.Ports, caller must remove from
// any other lists if called after BuildFabricLists
void NodeDataFreePorts(FabricData_t *fabricp, NodeData *nodep)
{
	cl_map_item_t *p;

	// this list is sorted/keyed by NodeGUID
	for (p=cl_qmap_head(&nodep->Ports); p != cl_qmap_end(&nodep->Ports); p = cl_qmap_head(&nodep->Ports)) {
		PortDataFree(fabricp, PARENT_STRUCT(p, PortData, NodePortsEntry));
	}
}

void NodeDataFreeSwitchData(FabricData_t *fabricp, NodeData *nodep)
{
	uint8_t i;

	if (nodep->switchp) {
		SwitchData *switchp = nodep->switchp;

		if (switchp->LinearFDB)
			MemoryDeallocate(switchp->LinearFDB);
		if (switchp->PortGroupFDB)
			MemoryDeallocate(switchp->PortGroupFDB);
		if (switchp->PortGroupElements)
			MemoryDeallocate(switchp->PortGroupElements);
		for (i = 0; i < STL_NUM_MFT_POSITIONS_MASK; ++i)
			if (switchp->MulticastFDB[i])
				MemoryDeallocate(switchp->MulticastFDB[i]);

		MemoryDeallocate(switchp);
	}
	nodep->switchp = NULL;
}

FSTATUS NodeDataAllocateFDB(FabricData_t *fabricp, NodeData *nodep,
							uint32 LinearFDBSize) {

	if (NodeDataSwitchResizeLinearFDB(nodep, LinearFDBSize) != FSUCCESS) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		return FINSUFFICIENT_MEMORY;
	}

	return FSUCCESS;
}

FSTATUS NodeDataAllocateSwitchData(FabricData_t *fabricp, NodeData *nodep,
				uint32 LinearFDBSize, uint32 MulticastFDBSize)
{
	SwitchData *switchp;

	if (nodep->switchp)	{
		// Free prior switch data
		NodeDataFreeSwitchData(fabricp, nodep);
	}

	nodep->switchp = (SwitchData *)MemoryAllocate2AndClear(sizeof(SwitchData), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! nodep->switchp) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		goto fail;
	}

	switchp = nodep->switchp;

	if (NodeDataAllocateFDB(fabricp, nodep, LinearFDBSize) != FSUCCESS) {
		goto fail;
	}

	if (NodeDataSwitchResizeMcastFDB(nodep, MulticastFDBSize) != FSUCCESS) {
		goto fail;
	}

	// These are for the new STL PG Adaptative routing. Note that we allocate
	// in multiples of full "blocks" which simplifies sending PG MADs to the
	// switches and supports up to 64 ports per port group. Supporting more
	// than 64 ports will require a different memory management technique.
	switchp->PortGroupSize = ROUNDUP(nodep->pSwitchInfo->SwitchInfoData.PortGroupCap, NUM_PGT_ELEMENTS_BLOCK);
	switchp->PortGroupElements = 
		MemoryAllocate2AndClear(switchp->PortGroupSize * 
			sizeof(STL_PORTMASK), IBA_MEM_FLAG_PREMPTABLE, MYTAG);


	return FSUCCESS;
fail:
	NodeDataFreeSwitchData(fabricp, nodep);
	return FINSUFFICIENT_MEMORY;
}


//----------------------------------------------------------------------------

FSTATUS NodeDataSwitchResizeMcastFDB(NodeData * nodep, uint32 newMfdbSize)
{
	int errStatus = FINSUFFICIENT_MEMORY;
	int i;
	STL_PORTMASK * newMfdb[STL_NUM_MFT_POSITIONS_MASK] = {NULL};
	assert(nodep->NodeInfo.NodeType == STL_NODE_SW && nodep->switchp && nodep->pSwitchInfo);

	STL_SWITCH_INFO * switchInfo = &nodep->pSwitchInfo->SwitchInfoData;

	STL_LID newMtop = switchInfo->MulticastFDBTop;

	if (newMfdbSize > 0) {
		newMfdbSize = MIN(newMfdbSize, switchInfo->MulticastFDBCap);

		for (i = 0; i < STL_NUM_MFT_POSITIONS_MASK; ++i) {
			newMfdb[i] = (STL_PORTMASK*) MemoryAllocate2AndClear(newMfdbSize * sizeof(STL_PORTMASK),
				IBA_MEM_FLAG_PREMPTABLE, MYTAG);
			if (!newMfdb[i]) goto fail;
		}

		if (switchInfo->MulticastFDBTop >= STL_LID_MULTICAST_BEGIN) {
			size_t size = MIN(newMfdbSize, switchInfo->MulticastFDBTop - STL_LID_MULTICAST_BEGIN + 1);

			for (i = 0; i < STL_NUM_MFT_POSITIONS_MASK; ++i) {
				if (!nodep->switchp->MulticastFDB[i])
					continue;

				memcpy(newMfdb[i], nodep->switchp->MulticastFDB[i], size * sizeof(STL_PORTMASK));
				newMtop = MIN((size - 1) + STL_LID_MULTICAST_BEGIN, newMtop);
			}
		}
	}

	if (newMfdbSize) {
		for (i = 0; i < STL_NUM_MFT_POSITIONS_MASK; ++i) {
			if (nodep->switchp->MulticastFDB[i]) {
				MemoryDeallocate(nodep->switchp->MulticastFDB[i]);
				nodep->switchp->MulticastFDB[i] = NULL;
			}
		}

		memcpy(nodep->switchp->MulticastFDB, newMfdb, sizeof(newMfdb));
		switchInfo->MulticastFDBTop = newMtop;

		if (newMtop >= STL_LID_MULTICAST_BEGIN)
			nodep->switchp->MulticastFDBSize = newMtop + 1;
		else
			nodep->switchp->MulticastFDBSize = newMfdbSize + STL_LID_MULTICAST_BEGIN;

	}

	return FSUCCESS;
fail:
	for (i = 0; i < STL_NUM_MFT_POSITIONS_MASK; ++i) {
		if (newMfdb[i]) {
			MemoryDeallocate(newMfdb[i]);
			newMfdb[i] = NULL;
		}
	}

	return errStatus;
}

FSTATUS NodeDataSwitchResizeLinearFDB(NodeData * nodep, uint32 newLfdbSize)
{
	uint32 newPgdbSize;
	STL_LINEAR_FORWARDING_TABLE * newLfdb = NULL;
	STL_PORT_GROUP_FORWARDING_TABLE * newPgdb = NULL;
	DEBUG_ASSERT(nodep->NodeInfo.NodeType == STL_NODE_SW && nodep->switchp && nodep->pSwitchInfo);

	STL_SWITCH_INFO * switchInfo = &nodep->pSwitchInfo->SwitchInfoData;

	STL_LID newLtop = switchInfo->LinearFDBTop;

	if (newLfdbSize > 0 &&
		switchInfo->RoutingMode.Enabled == STL_ROUTE_LINEAR) {
		newLfdbSize = MIN(newLfdbSize, switchInfo->LinearFDBCap);
		newLfdb = (STL_LINEAR_FORWARDING_TABLE*) MemoryAllocate2AndClear(
			ROUNDUP(newLfdbSize, MAX_LFT_ELEMENTS_BLOCK), IBA_MEM_FLAG_PREMPTABLE, MYTAG);

		if (!newLfdb) goto fail;

		memset(newLfdb, 0xff, ROUNDUP(newLfdbSize, MAX_LFT_ELEMENTS_BLOCK));
		if (nodep->switchp->LinearFDB) {
			size_t size = MIN(newLfdbSize, switchInfo->LinearFDBTop + 1);
			memcpy(newLfdb, nodep->switchp->LinearFDB, size * sizeof(PORT));
			newLtop = MIN(size - 1, newLtop);
		}

		// Port Group Forwarding table same as Linear FDB but capped
		// at PortGroupFDBCap (for early STL1 HW hardcoded cap of 8kb)
		newPgdbSize = MIN(newLfdbSize,
					  switchInfo->PortGroupFDBCap ? switchInfo->PortGroupFDBCap : DEFAULT_MAX_PGFT_LID+1);
		newPgdb = (STL_PORT_GROUP_FORWARDING_TABLE*) MemoryAllocate2AndClear(
			ROUNDUP(newPgdbSize, NUM_PGFT_ELEMENTS_BLOCK), IBA_MEM_FLAG_PREMPTABLE, MYTAG);

		if (!newPgdb) goto fail;

		memset(newPgdb, 0xff, ROUNDUP(newPgdbSize, NUM_PGFT_ELEMENTS_BLOCK));
		if (nodep->switchp->PortGroupFDB) {
			size_t size = MIN(newPgdbSize, switchInfo->LinearFDBTop + 1);
			memcpy(newPgdb, nodep->switchp->PortGroupFDB, size * sizeof(PORT));
		}
	}

	if (newLfdb) {
		STL_LINEAR_FORWARDING_TABLE * oldLfdb = nodep->switchp->LinearFDB;
		nodep->switchp->LinearFDB = newLfdb;
		nodep->switchp->LinearFDBSize = newLfdbSize;
		switchInfo->LinearFDBTop = newLtop;
		MemoryDeallocate(oldLfdb);
	}

	if (newPgdb) {
		STL_PORT_GROUP_FORWARDING_TABLE * oldPgdb = nodep->switchp->PortGroupFDB;
		nodep->switchp->PortGroupFDB = newPgdb;
		nodep->switchp->PortGroupFDBSize = newPgdbSize;
		MemoryDeallocate(oldPgdb);
	}

	return FSUCCESS;
fail:
	// MemoryDeallocate perfectly handles NULL input.
	MemoryDeallocate(newLfdb);
	MemoryDeallocate(newPgdb);
	return FINSUFFICIENT_MEMORY;
}

FSTATUS NodeDataSwitchResizeFDB(NodeData * nodep, uint32 newLfdbSize, uint32 newMfdbSize)
{
	if ((NodeDataSwitchResizeLinearFDB(nodep, newLfdbSize) != FSUCCESS) ||
		(NodeDataSwitchResizeMcastFDB(nodep, newMfdbSize) != FSUCCESS)) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		return FINSUFFICIENT_MEMORY;
	}
	return FSUCCESS;
}

// remove Node from lists and free it
// Only removes node from AllNodes and SystemData.Nodes
// Only removes ports from AllLids and NodeData.Ports
// Only removes Iou from NodeData.ioup
// Only removes Iocs from AllIOCs and IouData.Iocs, caller must remove from
// any other lists if called after BuildFabricLists
void NodeDataFree(FabricData_t *fabricp, NodeData *nodep)
{
	if (nodep->context && g_Top_FreeCallbacks.pNodeDataFreeCallback)
		(*g_Top_FreeCallbacks.pNodeDataFreeCallback)(fabricp, nodep);

	cl_qmap_remove_item(&nodep->systemp->Nodes, &nodep->SystemNodesEntry);
	if (cl_qmap_count(&nodep->systemp->Nodes) == 0) {
		if (nodep->systemp->context && g_Top_FreeCallbacks.pSystemDataFreeCallback)
			(*g_Top_FreeCallbacks.pSystemDataFreeCallback)(fabricp, nodep->systemp);

		cl_qmap_remove_item(&fabricp->AllSystems, &nodep->systemp->AllSystemsEntry);
		MemoryDeallocate(nodep->systemp);
	}
	cl_qmap_remove_item(&fabricp->AllNodes, &nodep->AllNodesEntry);
	NodeDataFreePorts(fabricp, nodep);
#if !defined(VXWORKS) || defined(BUILD_DMC)
	if (nodep->ioup)
		IouDataFree(fabricp, nodep->ioup);
#endif
	if (nodep->pSwitchInfo)
		MemoryDeallocate(nodep->pSwitchInfo);
	NodeDataFreeSwitchData(fabricp, nodep);
	MemoryDeallocate(nodep);
}

// remove all Nodes from lists and free them
// Only removes nodes from AllNodes and SystemData.Nodes
// Only removes ports from AllLids and NodeData.Ports
// Only removes Iou from NodeData.ioup
// Only removes Iocs from AllIOCs and IouData.Iocs, caller must remove from
// any other lists if called after BuildFabricLists
void NodeDataFreeAll(FabricData_t *fabricp)
{
	cl_map_item_t *p;

	// this list is sorted/keyed by NodeGUID
	for (p=cl_qmap_head(&fabricp->AllNodes); p != cl_qmap_end(&fabricp->AllNodes); p = cl_qmap_head(&fabricp->AllNodes)) {
		NodeDataFree(fabricp, PARENT_STRUCT(p, NodeData, AllNodesEntry));
	}
}

// remove SM from lists and free it
// Only removes SM from AllSMs, caller must remove from
// any other lists if called after BuildFabricLists
void SMDataFree(FabricData_t *fabricp, SMData *smp)
{
	if (smp->context && g_Top_FreeCallbacks.pSMDataFreeCallback)
		(*g_Top_FreeCallbacks.pSMDataFreeCallback)(fabricp, smp);

	cl_qmap_remove_item(&fabricp->AllSMs, &smp->AllSMsEntry);
	MemoryDeallocate(smp);
}

// remove all SMs from lists and free them
// Only removes SMs from AllSMs, caller must remove from
// any other lists if called after BuildFabricLists
void SMDataFreeAll(FabricData_t *fabricp)
{
	cl_map_item_t *p;

	for (p=cl_qmap_head(&fabricp->AllSMs); p != cl_qmap_end(&fabricp->AllSMs); p = cl_qmap_head(&fabricp->AllSMs)) {
		SMDataFree(fabricp, PARENT_STRUCT(p, SMData, AllSMsEntry));
	}
}

void MCGroupFree(FabricData_t *fabricp, McGroupData *mcgroupp)
{
	LIST_ITEM *p;

	while (!QListIsEmpty(&mcgroupp->AllMcGroupMembers)) {
		p = QListTail(&mcgroupp->AllMcGroupMembers);
		McGroupData *group = (McGroupData *)QListObj(p);
		QListRemoveTail(&mcgroupp->AllMcGroupMembers);
		MemoryDeallocate(group);
	}

	QListRemoveItem(&fabricp->AllMcGroups, &mcgroupp->AllMcGMembersEntry);
	MemoryDeallocate(mcgroupp);
}

void MCDataFreeAll(FabricData_t *fabricp)
{
	LIST_ITEM *p;
	while (!QListIsEmpty(&fabricp->AllMcGroups)) {
		p = QListTail(&fabricp->AllMcGroups);
		MCGroupFree(fabricp, (McGroupData *)QListObj(p));
	}

}

void VFDataFreeAll(FabricData_t *fabricp)
{
	LIST_ITEM *i;
	for (i = QListHead(&fabricp->AllVFs); i;) {
		LIST_ITEM *next = QListNext(&fabricp->AllVFs, i);
		MemoryDeallocate(QListObj(i));
		i = next;
	}
}

void CableDataFree(CableData *cablep)
{
	if (cablep->length)
		MemoryDeallocate(cablep->length);
	if (cablep->label)
		MemoryDeallocate(cablep->label);
	if (cablep->details)
		MemoryDeallocate(cablep->details);
	cablep->length = cablep->label = cablep->details = NULL;
}

void PortSelectorFree(PortSelector *portselp)
{
	if (portselp->NodeDesc)
		MemoryDeallocate(portselp->NodeDesc);
	if (portselp->details)
		MemoryDeallocate(portselp->details);
	MemoryDeallocate(portselp);
}

// remove Link from lists and free it
void ExpectedLinkFree(FabricData_t *fabricp, ExpectedLink *elinkp)
{
	if (elinkp->portp1 && elinkp->portp1->elinkp == elinkp)
		elinkp->portp1->elinkp = NULL;
	if (elinkp->portp2 && elinkp->portp2->elinkp == elinkp)
		elinkp->portp2->elinkp = NULL;
	if (ListItemIsInAList(&elinkp->ExpectedLinksEntry))
		QListRemoveItem(&fabricp->ExpectedLinks, &elinkp->ExpectedLinksEntry);
	if (elinkp->portselp1)
		PortSelectorFree(elinkp->portselp1);
	if (elinkp->portselp2)
		PortSelectorFree(elinkp->portselp2);
	if (elinkp->details)
		MemoryDeallocate(elinkp->details);
	CableDataFree(&elinkp->CableData);
	MemoryDeallocate(elinkp);
}

// remove all Links from lists and free them
void ExpectedLinkFreeAll(FabricData_t *fabricp)
{
	LIST_ITEM *p;

	// free all link data
	for (p=QListHead(&fabricp->ExpectedLinks); p != NULL;) {
		LIST_ITEM *nextp = QListNext(&fabricp->ExpectedLinks, p);
		ExpectedLinkFree(fabricp, (ExpectedLink *)QListObj(p));
		p = nextp;
	}
}

// remove Expected Node from lists and free it
void ExpectedNodeFree(FabricData_t *fabricp, ExpectedNode *enodep, QUICK_LIST *listp)
{
	if (enodep->nodep && enodep->nodep->enodep == enodep)
		enodep->nodep->enodep = NULL;
	if (ListItemIsInAList(&enodep->ExpectedNodesEntry))
		QListRemoveItem(listp, &enodep->ExpectedNodesEntry);
	if (enodep->NodeGUID)
		cl_qmap_remove(&fabricp->ExpectedNodeGuidMap, enodep->NodeGUID);
	if (enodep->NodeDesc)
		MemoryDeallocate(enodep->NodeDesc);
	if (enodep->details)
		MemoryDeallocate(enodep->details);
	if (enodep->ports) {
		int i;
		for (i = 0; i < enodep->portsSize; ++i) {
			if (enodep->ports[i])
				MemoryDeallocate(enodep->ports[i]);
		}
		MemoryDeallocate(enodep->ports);
	}
	MemoryDeallocate(enodep);
}

// remove all Expected Nodes from lists and maps and free them
void ExpectedNodesFreeAll(FabricData_t *fabricp, QUICK_LIST *listp)
{
	LIST_ITEM *p;

	// free all link data
	for (p=QListHead(listp); p != NULL;) {
		LIST_ITEM *nextp = QListNext(listp, p);
		ExpectedNodeFree(fabricp, (ExpectedNode *)QListObj(p), listp);
		p = nextp;
	}
}

void DestroyFabricData(FabricData_t *fabricp)
{
	if (fabricp->context && g_Top_FreeCallbacks.pFabricDataFreeCallback)
		(*g_Top_FreeCallbacks.pFabricDataFreeCallback)(fabricp);

	ExpectedLinkFreeAll(fabricp);	// ExpectedLinks
	ExpectedNodesFreeAll(fabricp, &fabricp->ExpectedFIs);	// ExpectedFIs
	ExpectedNodesFreeAll(fabricp, &fabricp->ExpectedSWs);	// ExpectedSWs

	NodeDataFreeAll(fabricp);	// Nodes, Ports, IOUs, Systems

	if (fabricp->flags & FF_LIDARRAY)
		FreeLidMap(fabricp);

	// make sure no stale pointers in lists, etc
	// also clear counters and flags
	MemoryClear(fabricp, sizeof(*fabricp));
}

static uint64_t convertNodeName2Guid(char* idStr, size_t len) {
	int hash = 0;
	int i = 0;
	for (; i < len; i++) {
		hash = hash * 31 + idStr[i];
	}
	return hash;
}

SnmpNodeConfigParamData_t* snmpDataAddNodeConfig(FabricData_t *fabricp, SnmpNodeConfigParamData_t *nodeConfParmp, int verbose, int quiet)
{
	cl_map_item_t *mi;
	snmpNodeConfigData_t *snmpNodeConfp;

	if (!nodeConfParmp) {
		return NULL;
	}

	//mi = cl_qmap_get(&fabricp->map_guid_to_ib_device, nodep->NodeInfo.NodeGUID);
	mi = cl_qmap_get(&fabricp->map_snmp_desc_to_node, convertNodeName2Guid(nodeConfParmp->NodeDesc, strlen(nodeConfParmp->NodeDesc)));
	if (mi != cl_qmap_end(&fabricp->map_snmp_desc_to_node)) {
		snmpNodeConfp = PARENT_STRUCT(mi, snmpNodeConfigData_t, AllSnmpNodesEntry);
		if (verbose >= 5)
			fprintf(stderr, "%s: WARNING - duplicate node name found in SNMP node config parameter data: %s\n",
			    g_Top_cmdname, nodeConfParmp->NodeDesc);

		return nodeConfParmp;
	}

	snmpNodeConfp = (snmpNodeConfigData_t *)MemoryAllocate2AndClear(sizeof(snmpNodeConfigData_t), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (!snmpNodeConfp) {
		fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
		return NULL;
	}
	MemoryFill(snmpNodeConfp, 0, sizeof(snmpNodeConfigData_t));

	strncpy((char *)snmpNodeConfp->NodeDesc.NodeString, nodeConfParmp->NodeDesc,
		sizeof(snmpNodeConfp->NodeDesc.NodeString)-1);

	//cl_qmap_init(&snmpNodeConfp->map_dlid_to_route, NULL);
	mi = cl_qmap_insert(&fabricp->map_snmp_desc_to_node, convertNodeName2Guid(nodeConfParmp->NodeDesc, strlen(nodeConfParmp->NodeDesc)), &snmpNodeConfp->AllSnmpNodesEntry);
	if (mi != &snmpNodeConfp->AllSnmpNodesEntry) {
		MemoryDeallocate(snmpNodeConfp);
		snmpNodeConfp = PARENT_STRUCT(mi, snmpNodeConfigData_t, AllSnmpNodesEntry);
		fprintf(stderr, "ERROR - failed to add SNMP config data of %s %s\n",
			 StlNodeTypeToText(nodeConfParmp->NodeType),
			 nodeConfParmp->NodeDesc);
		return NULL;
	} else {
		QListInitState(&snmpNodeConfp->InterfaceNames);
		if (!QListInit(&snmpNodeConfp->InterfaceNames)) {
			fprintf(stderr, "%s: Unable to initialize list\n", g_Top_cmdname);
			goto fail;
		}
		if (nodeConfParmp->InterfaceDesc) {
			char* org_if_desc = strdup(nodeConfParmp->InterfaceDesc);
			if (org_if_desc == NULL) {
				fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
				goto fail;
			}
			int org_if_len = strlen(org_if_desc);
			char* interface = strtok(nodeConfParmp->InterfaceDesc, " ,\t\r\n");
			int if_len = 0;
			while (interface != NULL) {
				LIST_ITEM *item = (LIST_ITEM *)MemoryAllocate2AndClear(
						sizeof(LIST_ITEM), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
				if (item == NULL) {
					fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
					free(org_if_desc);
					goto fail;
				}
				if (if_len)
					if_len += 1; // count ','
				if_len += strlen(interface);
				char *dupInterface = strdup(interface);
				if (dupInterface == NULL) {
					fprintf(stderr, "%s: Unable to allocate memory\n", g_Top_cmdname);
					free(org_if_desc);
					MemoryDeallocate(item);
					goto fail;
				}
				QListSetObj(item, dupInterface);
				QListInsertTail(&snmpNodeConfp->InterfaceNames, item);
				interface = strtok(NULL, " ,\t\r\n");
			}
			if (org_if_len != if_len) {
				fprintf(stderr, "%s: WARNING - empty string and/or space(s) ignored in interface names '%s'\n",
						g_Top_cmdname, org_if_desc);
			}
			free(org_if_desc);
		}

		ListItemInitState(&snmpNodeConfp->AllSnmpNodeConfigsEntry);
		QListSetObj(&snmpNodeConfp->AllSnmpNodeConfigsEntry, snmpNodeConfp);

		if (nodeConfParmp->NodeType == STL_NODE_FI) {
			//fprintf(stderr, "%s: [debug] Adding host to discovery list:  nodename=<%s>\n",
			//	__func__,
			//	 snmpNodeConfp->NodeDesc.NodeString);

			QListInsertTail(&fabricp->SnmpDiscoverHosts, &snmpNodeConfp->AllSnmpNodeConfigsEntry);
		} else {
			//fprintf(stderr, "%s: [debug] Adding switch to discovery list:  nodename=<%s>\n",
			//	__func__,
			//	 snmpNodeConfp->NodeDesc.NodeString);
			QListInsertTail(&fabricp->SnmpDiscoverSwitches, &snmpNodeConfp->AllSnmpNodeConfigsEntry);
		}
		if (verbose >= 4 && !quiet) {
			ProgressPrint(TRUE, "Add %s %s",
				StlNodeTypeToText(nodeConfParmp->NodeType),
				nodeConfParmp->NodeDesc);
		}
	}
	return nodeConfParmp;
fail:
	cl_qmap_remove_item(&fabricp->map_snmp_desc_to_node, mi);
	MemoryDeallocate(snmpNodeConfp);
	return NULL;
}
