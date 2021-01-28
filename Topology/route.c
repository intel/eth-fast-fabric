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

// functions to perform routing analysis using LFT tables in FabricData
#include <opamgt_sa_priv.h>

#include "topology.h"
#include "topology_internal.h"

//////////////////////////////////////////////////////
// multicast routes functions                       //
//////////////////////////////////////////////////////

static STL_PORTMASK *LookupMFT(PortData *portp, STL_LID mlid, uint8 pos)
{
	STL_PORTMASK *PortMask;
	uint32 entry;
	uint32 MCFDBSize;

	DEBUG_ASSERT(portp->nodep->switchp && portp->nodep->switchp->MulticastFDB[0]);	//caller checks

	// Get index independent of mlid format (16-bit or 32-bit)
	entry = GetMulticastOffset((uint32)mlid);
	MCFDBSize = portp->nodep->switchp->MulticastFDBSize & MULTICAST_LID_OFFSET_MASK;
	if (entry >=MCFDBSize)
		return NULL;

	if (pos >= STL_NUM_MFT_POSITIONS_MASK)
		return NULL;

	PortMask= GetMulticastFDBEntry(portp->nodep, entry, pos);

	return PortMask;
}

// copying the route with problems to main list for later display.
static FSTATUS CopyAndInsertMcLoopInc(FabricData_t *fabricp, MCROUTESTATUS MCRouteStatus, McLoopInc *pMcLoopInc)
{
	LIST_ITEM *p;

	McLoopInc *pMcLoopIncR = (McLoopInc*)MemoryAllocate2AndClear(sizeof(McLoopInc), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
	if (! pMcLoopIncR) {
		fprintf(stderr, "Unable to allocate memory to init a list of MC loop and incomplete routes\n");
		return FINSUFFICIENT_MEMORY;
	}
	//copying contents of pMcLoopInc to the new record.
	pMcLoopIncR->status = MCRouteStatus;
	pMcLoopIncR->mlid = pMcLoopInc->mlid;


	QListInitState(&pMcLoopIncR->AllMcNodeLoopIncR);
	if (!QListInit(&pMcLoopIncR->AllMcNodeLoopIncR)) {
		fprintf(stderr, "Unable to initialize List of nodes with not found MC routes\n");
		//deallocate mem
		MemoryDeallocate(pMcLoopIncR);
		return FINSUFFICIENT_MEMORY;
	}

	// copying list of nodes
	for (p = QListHead(&pMcLoopInc->AllMcNodeLoopIncR); p!= NULL; p = QListNext( &pMcLoopInc->AllMcNodeLoopIncR,p) ){
		// retrieve current node
		McNodeLoopInc *pmcnode = (McNodeLoopInc *) QListObj(p);
		// create new node
		McNodeLoopInc *pMcNodeLoopIncR = (McNodeLoopInc*)MemoryAllocate2AndClear(sizeof(McNodeLoopInc), IBA_MEM_FLAG_PREMPTABLE, MYTAG);
		if (! pMcNodeLoopIncR) {
			fprintf(stderr, "Unable to allocate memory to init a list of MC loop and incomplete routes\n");
			MemoryDeallocate(pMcLoopIncR);
			return FINSUFFICIENT_MEMORY;
		}
		// copy node to node
		pMcNodeLoopIncR->pPort = pmcnode->pPort;
		pMcNodeLoopIncR->entryPort = pmcnode->entryPort;
		pMcNodeLoopIncR->exitPort = pmcnode->exitPort;
		//insert new node
		QListSetObj(&pMcNodeLoopIncR->McNodeEntry, pMcNodeLoopIncR);
		QListInsertTail(&pMcLoopIncR->AllMcNodeLoopIncR , &pMcNodeLoopIncR->McNodeEntry);
	} // end for


	//When all nodes were copied add route item to the main list
	QListSetObj(&pMcLoopIncR->LoopIncEntry, pMcLoopIncR);
	QListInsertTail(&fabricp->AllMcLoopIncRoutes[MCRouteStatus].AllMcRouteStatus, &pMcLoopIncR->LoopIncEntry);

	return FSUCCESS;
}

static FSTATUS IsMemberMcGroup(McGroupData *mcgroupp, PortData *portp)
{
	LIST_ITEM *p;

	for (p=QListHead(&mcgroupp->AllMcGroupMembers); p!= NULL; p = QListNext(&mcgroupp->AllMcGroupMembers,p)) {
		McMemberData *mcmp = (McMemberData *)QListObj(p);
		if (mcmp->MemberInfo.RID.PortGID.AsReg64s.L == portp->nodep->NodeInfo.NodeGUID) {
			/*if (!mcmp->MemberInfo.JoinFullMember)
				return FNOT_FOUND;
			else */return FSUCCESS;
		}
	}
	return FNOT_FOUND;
}

// Walk MC route from portp for mlid group.
//
// returns status of MC route (if not FSUCCESS)
// FUNAVAILABLE - no routing tables in FabricData given
// FNOT_FOUND - unable to find starting port
// FNOT_DONE - unable to trace route, mlid is a dead end
// FDUPLICATE - loop detected in mc routes

FSTATUS WalkMCRoute(FabricData_t *fabricp, McGroupData *mcgroupp, PortData *portp, int hop,
	uint8 EntryPort, McLoopInc *pMcLoopInc, uint32 *pathCount)
{
	PortData *portp2, *portn;	// next port in route
	STL_PORTMASK *pp;
	FSTATUS status=FSUCCESS;
	uint64 Port_res;
	SwitchData *switchp;
	MCROUTESTATUS mcroutestat;
	McNodeLoopInc McNodeLoopIncR, *pMcNodeLoopIncR;

	pMcNodeLoopIncR = &McNodeLoopIncR;

	ListItemInitState(&pMcNodeLoopIncR->McNodeEntry);
	pMcNodeLoopIncR->pPort = portp;
	pMcNodeLoopIncR->entryPort = EntryPort;
	pMcNodeLoopIncR->exitPort = 0;

	QListSetObj(&pMcNodeLoopIncR->McNodeEntry, pMcNodeLoopIncR);
	QListInsertTail(&pMcLoopInc->AllMcNodeLoopIncR , &pMcNodeLoopIncR->McNodeEntry);

	if ((hop) >= 64) {
		// add list to NOT_DONE
		mcroutestat=MC_NO_TRACE;
		status = CopyAndInsertMcLoopInc(fabricp, mcroutestat, pMcLoopInc);
		if (status== FINSUFFICIENT_MEMORY) {
			fprintf(stderr, "Unable to allocate memory\n");
			return FERROR;
		}
		QListRemoveTail(&pMcLoopInc->AllMcNodeLoopIncR);
		(*pathCount)++;
		return FNOT_DONE; // too long a path
	}
// if port is NIC them check membership, if OK then reach end-of-route
// if port is SW; Port0 then check membership, if OK then reach end-of-route
	if (( (hop > 1) && (portp->nodep->NodeInfo.NodeType == STL_NODE_SW)
		&& (EntryPort == 0) && portp->nodep->pSwitchInfo->SwitchInfoData.u2.s.EnhancedPort0) // this is a enhanced Port0;
		|| (portp->nodep->NodeInfo.NodeType == STL_NODE_FI)) { // these are end-nodes
		status = IsMemberMcGroup(mcgroupp, portp);
		(*pathCount)++;
		if (status != FSUCCESS) {
			mcroutestat=MC_NOGROUP;
			status = CopyAndInsertMcLoopInc(fabricp, mcroutestat, pMcLoopInc);
			if (status == FINSUFFICIENT_MEMORY) {
				fprintf(stderr, "Unable to allocate memory\n");
				return FERROR;
			}
			QListRemoveTail(&pMcLoopInc->AllMcNodeLoopIncR);
			return FUNAVAILABLE; // not a member
		}
		else {
			QListRemoveTail(&pMcLoopInc->AllMcNodeLoopIncR);
		}
		return FSUCCESS;
	}
	if ((EntryPort == 0) && (hop > 1)){ // Port 0 cannot be switch external port
		mcroutestat=MC_NOGROUP;
		status = CopyAndInsertMcLoopInc(fabricp,mcroutestat, pMcLoopInc);
		if (status== FINSUFFICIENT_MEMORY) {
			fprintf(stderr, "Unable to allocate memory\n");
			return FERROR;
		}
		QListRemoveTail(&pMcLoopInc->AllMcNodeLoopIncR);
		return FNOT_DONE;
	}
// if we are here, port can be a switch
 	switchp = portp->nodep->switchp;

// 	//test if there is a switch
	if (!switchp) {
 		mcroutestat=MC_NO_TRACE;
 		status = CopyAndInsertMcLoopInc(fabricp,mcroutestat, pMcLoopInc);
 		if (status== FINSUFFICIENT_MEMORY) {
 			fprintf(stderr, "Unable to allocate memory\n");
 			return FERROR;
 		}
		QListRemoveTail(&pMcLoopInc->AllMcNodeLoopIncR);
		(*pathCount)++;
		return FNOT_DONE;
	}

//test if there is a routing table
	if (!switchp->MulticastFDB) {
		mcroutestat=MC_UNAVAILABLE;
		status = CopyAndInsertMcLoopInc(fabricp,mcroutestat, pMcLoopInc);
		if (status== FINSUFFICIENT_MEMORY) {
			fprintf(stderr, "Unable to allocate memory\n");
			return FERROR;
		}
		QListRemoveTail(&pMcLoopInc->AllMcNodeLoopIncR);
		(*pathCount)++;
		return FUNAVAILABLE;
	}

// check if the size of the table is enough
// check if MulticastFDBTop <= MulticastFDBCap
 	uint32 MCTableSize;
// lower 14 bits of top
	MCTableSize = portp->nodep->pSwitchInfo->SwitchInfoData.MulticastFDBTop & MULTICAST_LID_OFFSET_MASK;
	if (MCTableSize > portp->nodep->pSwitchInfo->SwitchInfoData.MulticastFDBCap){
		mcroutestat=MC_UNAVAILABLE;
 		status = CopyAndInsertMcLoopInc(fabricp,mcroutestat, pMcLoopInc);
 		if (status== FINSUFFICIENT_MEMORY) {
 			fprintf(stderr, "Unable to allocate memory\n");
 			return FERROR;
 		}
 		QListRemoveTail(&pMcLoopInc->AllMcNodeLoopIncR);
 		(*pathCount)++;
		return FUNAVAILABLE;
	}

// get index to retrieve routing ports for MC from the MC table
	int ix_lid = GetMulticastOffset((uint32)pMcLoopInc->mlid);
	//verify that index lies within the table size
 	if ( ix_lid > MCTableSize) {
 		mcroutestat=MC_UNAVAILABLE;
 		status = CopyAndInsertMcLoopInc(fabricp,mcroutestat, pMcLoopInc);
 		if (status== FINSUFFICIENT_MEMORY) {
 			fprintf(stderr, "Unable to allocate memory\n");
 			return FERROR;
 		}
 		QListRemoveTail(&pMcLoopInc->AllMcNodeLoopIncR);
 		(*pathCount)++;
 		return FUNAVAILABLE;
 	}


	// is this my first visit here?
	if (!portp->nodep->switchp->HasBeenVisited)
		portp->nodep->switchp->HasBeenVisited = 1;
	else {   //if not... there is a loop.
		mcroutestat=MC_LOOP;
		status = CopyAndInsertMcLoopInc(fabricp,mcroutestat, pMcLoopInc);
		if (status== FINSUFFICIENT_MEMORY) {
			fprintf(stderr, "Unable to allocate memory\n");
			return FERROR;
		}
		QListRemoveTail(&pMcLoopInc->AllMcNodeLoopIncR);
		(*pathCount)++;
		return FDUPLICATE;
	}

	// getting information of number of ports for the current switch to run the loop only for those values
	// limiting max. num of ports to 256
	uint8 swmaxnumport = portp->nodep->NodeInfo.NumPorts;
	if (swmaxnumport > STL_MAX_PORTS) {
		fprintf(stderr, "Cannot handle more than %d different ports\n", STL_MAX_PORTS);
		return FERROR;
	}

	uint8 pos=0;
	int ix_port;
	for (ix_port=0; ix_port <= swmaxnumport; ix_port ++) {
		if (EntryPort != ix_port) {
			//select correct mask to test given 256 max ports
			pos = ix_port / STL_PORT_MASK_WIDTH;
			pp = LookupMFT(portp, (uint32)pMcLoopInc->mlid, pos);
			if (pp==NULL) {
				mcroutestat=MC_UNAVAILABLE;
				status = CopyAndInsertMcLoopInc(fabricp,mcroutestat, pMcLoopInc);
				if (status== FINSUFFICIENT_MEMORY) {
					fprintf(stderr, "Unable to allocate memory\n");
					return FERROR;
				}
				QListRemoveTail(&pMcLoopInc->AllMcNodeLoopIncR);
				(*pathCount)++;
				return FUNAVAILABLE;
			}
			Port_res = ((uint64_t)1<< (ix_port % STL_PORT_MASK_WIDTH)) & *pp;/// this must be compatible with 4 columns of 64 bits each
			if (Port_res != 0) { //matching port
				pMcNodeLoopIncR->exitPort = ix_port; // save exit port
				if (ix_port == 0) {
					status = WalkMCRoute( fabricp, mcgroupp, portp, (hop+1), 0, pMcLoopInc, pathCount);
					if (status == FERROR)
						return FERROR;
				}
				else {
					portp2 = FindNodePort(portp->nodep, ix_port);
					if (! portp2 || ! IsEthPortInitialized(portp2->PortInfo.PortStates)
						|| (!portp2->neighbor)) {
						mcroutestat=MC_NO_TRACE;
						status = CopyAndInsertMcLoopInc(fabricp,mcroutestat, pMcLoopInc);
						if (status== FINSUFFICIENT_MEMORY) {
							fprintf(stderr, "Unable to allocate memory\n");
							return FERROR;
						}
						(*pathCount)++;
						status = FNOT_DONE;
					} // Non viable route
					else {
						portn = portp2->neighbor; // mist be the entry port of the next switch
						status = WalkMCRoute( fabricp, mcgroupp, portn, (hop+1), portn->PortNum, pMcLoopInc, pathCount);
						if (status == FERROR)
						// just return all the way
							return FERROR;
					} //end else
				} //end else
			} // end of there is a match
		}// end of entry port != exit port
	} // end for looking for next port
	QListRemoveTail(&pMcLoopInc->AllMcNodeLoopIncR);

	return status;
}

// report all MC routes
FSTATUS ValidateMCRoutes(FabricData_t *fabricp, McGroupData *mcgroupp,
			McEdgeSwitchData *swp, uint32 *pathCount)
{
	FSTATUS status;

	SwitchData *switchp;
	PortData *portp1;
	LIST_ITEM *n1;
	portp1 = swp->pPort;

	switchp = portp1->nodep->switchp;
	if (!switchp) {
		printf("No switch connected to HFI \n");
		return FNOT_DONE;
	}

	McLoopInc mcLoop;
	QListInitState(&mcLoop.AllMcNodeLoopIncR);
	if (!QListInit(&mcLoop.AllMcNodeLoopIncR)) {
		fprintf(stderr, "Unable to initialize List of nodes for loops and incomplete MC routes\n");
		return FERROR;
	}

	mcLoop.mlid = mcgroupp->MLID; // identifies the route to analyze

	/* clear visited flag set in previous call to WalkMCRoute for every node */
	for (n1 = QListHead(&fabricp->AllSWs ); n1 != NULL; n1= QListNext(&fabricp->AllSWs, n1)) {
		NodeData * node = (NodeData*)QListObj(n1);
		node->switchp->HasBeenVisited=0;
	}
	status = WalkMCRoute( fabricp, mcgroupp, portp1, 1, swp->EntryPort, &mcLoop, pathCount);

	return status;
}

FSTATUS InitListofLoopAndIncMCRoutes(FabricData_t *fabricp)

{	int i;

	// init list of loops and incomplete MC routes

	fabricp->AllMcLoopIncRoutes[0].status=MC_NO_TRACE;
	fabricp->AllMcLoopIncRoutes[1].status=MC_NOT_FOUND;
	fabricp->AllMcLoopIncRoutes[2].status=MC_UNAVAILABLE;
	fabricp->AllMcLoopIncRoutes[3].status=MC_LOOP;
	fabricp->AllMcLoopIncRoutes[4].status=MC_NOGROUP;


	for (i=0; i<MAXMCROUTESTATUS; i++) {
		QListInitState(&fabricp->AllMcLoopIncRoutes[i].AllMcRouteStatus);
		if (!QListInit(&fabricp->AllMcLoopIncRoutes[i].AllMcRouteStatus)) {
			fprintf(stderr, "Unable to initialize List of MC loops and incomplete routes\n");
			return FERROR;
		}
	}
	return FSUCCESS;
}

FSTATUS ValidateAllMCRoutes(FabricData_t *fabricp, uint32 *totalPaths )

{	LIST_ITEM *n1, *p1, *q1;
	FSTATUS status;
	McMemberData *pMCM1;
	uint32 pathCount;

	*totalPaths = 0;

	status = InitListofLoopAndIncMCRoutes(fabricp);
	if (status!=FSUCCESS)
		return FERROR;

	// init all switches as never been visited
	for (n1 = QListHead(&fabricp->AllSWs ); n1 != NULL; n1= QListNext(&fabricp->AllSWs, n1)) {
		NodeData * node = (NodeData*)QListObj(n1);
		node->switchp->HasBeenVisited=0;
	}

	for (n1 = QListHead(&fabricp->AllMcGroups); n1 != NULL; n1= QListNext(&fabricp->AllMcGroups, n1)) {
		McGroupData *pmcgmember = (McGroupData *)QListObj(n1);
		//for this group get all member information
			p1 = QListHead(&pmcgmember->AllMcGroupMembers);
			pMCM1 = (McMemberData *)QListObj(p1);
			// do not validate routes empty groups or groups with 1 member
			if ((pMCM1->MemberInfo.RID.PortGID.AsReg64s.H != 0) || (pMCM1->MemberInfo.RID.PortGID.AsReg64s.L!=0)) {
				if (pmcgmember->NumOfMembers > 1) {
					for (q1 = QListHead(&pmcgmember->EdgeSwitchesInGroup); q1 != NULL; q1= QListNext(&pmcgmember->EdgeSwitchesInGroup, q1)) {
						pathCount=0;
						McEdgeSwitchData *swp = (McEdgeSwitchData *)QListObj(q1);
						status = ValidateMCRoutes(fabricp, pmcgmember, swp, &pathCount);
						if (status ==FERROR) {
							fprintf(stderr, "Unable to validate MC routes\n");
							return FERROR;
						}
						(*totalPaths) += pathCount;
					}
				} // end if num members >1
//			}// end evaluating groups with non-zero members*/
		} // end evaluating one MC group
	} // end for all MC groups

	return FSUCCESS;
} // End of ValidateAllMCRoutes

void FreeValidateMCRoutes(FabricData_t *fabricp)
{ int i;
  LIST_ITEM *p, *q;

	for (i=0;i<MAXMCROUTESTATUS;i++){
		//if the list is empty there is nothing to release
		while (!QListIsEmpty(&fabricp->AllMcLoopIncRoutes[i].AllMcRouteStatus) ){
			p = QListTail(&fabricp->AllMcLoopIncRoutes[i].AllMcRouteStatus);
			McLoopInc *pmcloop = (McLoopInc *) QListObj(p);
				//remove members in each mc route
			while (!QListIsEmpty(&pmcloop->AllMcNodeLoopIncR)) {
				q = QListTail(&pmcloop->AllMcNodeLoopIncR);
				McNodeLoopInc *pmcnodeloop = (McNodeLoopInc *) QListObj(q);
				QListRemoveTail(&pmcloop->AllMcNodeLoopIncR);
				MemoryDeallocate (pmcnodeloop);
			} // end while q
			QListRemoveTail(&fabricp->AllMcLoopIncRoutes[i].AllMcRouteStatus);
			MemoryDeallocate (pmcloop);
		}// end  while p
	}// end for 0..4
	return;

}

////////// /////////// end of MC-related functions
