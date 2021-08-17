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

#ifndef __VXWORKS__
#include <strings.h>
#include <fnmatch.h>
#else
#include "icsBspUtil.h"
#endif

#define FILENAME				256 //Length of file name
#define MIN_LIST_ITEMS				100 // minimum items for ListInit to allocate for
#define NODE_PAIR_DELIMITER_SIZE		1 //Length of delimiter while parsing for node pairs



// Functions to Parse Focus arguments and build POINTs for matches
typedef boolean (PointPortElinkCompareCallback_t)(ExpectedLink *elinkp, void *nodecmp, uint8 portnum, char* portid);
static FSTATUS ParsePointPort(FabricData_t *fabricp, char *arg, boolean is_portnunm, Point *pPoint, uint8 find_flag, PointPortElinkCompareCallback_t *callback, void *nodecmp, char **pp);

/* check arg to see if 1st characters match prefix, if so return pointer
 * into arg just after prefix, otherwise return NULL
 */
char* ComparePrefix(char *arg, const char *prefix)
{
	int len = strlen(prefix);
	if (strncmp(arg, prefix, len) == 0)
		return arg+len;
	else
		return NULL;
}

static FSTATUS ParseIfIDPoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	STL_LID lid;
	PortData *portp = NULL;
	char *param;
	
	ASSERT(! PointValid(pPoint));
	if (FSUCCESS != StringToUint32(&lid, arg, pp, 0, TRUE))  {
		fprintf(stderr, "%s: Invalid IfID format: '%s'\n", g_Top_cmdname, arg);
		return FINVALID_PARAMETER;
	}
	if (! lid) {
		fprintf(stderr, "%s: Invalid IfID: 0x%x\n", g_Top_cmdname, lid);
		return FINVALID_PARAMETER;
	}

	if (0 == (find_flag & FIND_FLAG_FABRIC))
		return FINVALID_OPERATION;
	if (find_flag & FIND_FLAG_FABRIC) {
		portp = FindEndLid(fabricp, lid);
		if (portp) {
			pPoint->u.portp = portp;
			pPoint->Type = POINT_TYPE_PORT;
		}
	}
	// N/A for FIND_FLAG_ENODE, FIND_FLAG_ESM and FIND_FLAG_ELINK
	if (PointValid(pPoint)) {
		ASSERT(portp);	// since we only handle FIND_FLAG_FABRIC
		if (NULL != (param = ComparePrefix(*pp, ":node"))) {
			*pp = param;
			pPoint->u.nodep = portp->nodep;
			pPoint->Type = POINT_TYPE_NODE;
		} else if (NULL != (param = ComparePrefix(*pp, ":port:"))) {
			pPoint->u.nodep = portp->nodep;
			pPoint->Type = POINT_TYPE_NODE;
			return ParsePointPort(fabricp, param, TRUE, pPoint, find_flag, NULL, NULL, pp);
		} else if (NULL != (param = ComparePrefix(*pp, ":portid:"))) {
			pPoint->u.nodep = portp->nodep;
			pPoint->Type = POINT_TYPE_NODE;
			return ParsePointPort(fabricp, param, FALSE, pPoint, find_flag, NULL, NULL, pp);
		}
		return FSUCCESS;
	} else {
		fprintf(stderr, "%s: IfID Not Found: 0x%x\n", g_Top_cmdname, lid);
		return FNOT_FOUND;
	}
}

static FSTATUS ParseMgmtIfAddrPoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	EUI64 guid;
	
	ASSERT(! PointValid(pPoint));
	if (FSUCCESS != StringToUint64(&guid, arg, pp, 0, TRUE))  {
		fprintf(stderr, "%s: Invalid Port Mgmt IfAddr format: '%s'\n",
						g_Top_cmdname, arg);
		return FINVALID_PARAMETER;
	}
	if (! guid) {
		fprintf(stderr, "%s: Invalid Port Mgmt IfAddr: 0x%016"PRIx64"\n",
						g_Top_cmdname, guid);
		return FINVALID_PARAMETER;
	}
	return FindPortGuidPoint(fabricp, guid, pPoint, find_flag, 0);
}

/* Parse a :port:# suffix, this will limit the Point to the list of ports
 * with the given number
 */
static FSTATUS ParsePointPort(FabricData_t *fabricp, char *arg, boolean is_portnunm, Point *pPoint, uint8 find_flag, PointPortElinkCompareCallback_t *callback, void *nodecmp, char **pp)
{
	uint8 portnum = 0;
	char portid[TINY_STR_ARRAY_SIZE+1] = "";
	FSTATUS status;
	Point newPoint;
	boolean fabric_fail = FALSE;

	ASSERT(PointValid(pPoint));
	PointInit(&newPoint);

	if (is_portnunm) {
		if (FSUCCESS != StringToUint8(&portnum, arg, pp, 0, TRUE))  {
			fprintf(stderr, "%s: Invalid Port Number format: '%s'\n",
							g_Top_cmdname, arg);
			status = FINVALID_PARAMETER;
			goto fail;
		}
	} else {
		StringCopy(portid, arg, sizeof(portid));
		*pp = arg + strlen(portid);
		if (!portid[0]) {
			fprintf(stderr, "%s: empty port id\n", g_Top_cmdname);
			status = FINVALID_PARAMETER;
			goto fail;
		}
	}

	// for fabric we try to provide a more detailed message using the
	// fabric_fail flag and leaving some of the information in the pPoint
	if (find_flag & FIND_FLAG_FABRIC) {
		switch (pPoint->Type) {
		case POINT_TYPE_NONE:
			break;
		case POINT_TYPE_PORT:
			ASSERT(0);	// should not be called for this type of point
			status = FINVALID_PARAMETER;
			goto fail;
		case POINT_TYPE_PORT_LIST:
			ASSERT(0);	// should not be called for this type of point
			status = FINVALID_PARAMETER;
			goto fail;
		case POINT_TYPE_NODE_PAIR_LIST:
			ASSERT(0);	// should not be called for this type of point
			status = FINVALID_PARAMETER;
			goto fail;
		case POINT_TYPE_NODE:
			{
			PortData *portp = is_portnunm ?
			                  FindNodePort(pPoint->u.nodep, portnum) :
			                  FindNodePortId(pPoint->u.nodep, portid);
			if (! portp) {
				fabric_fail = TRUE;
			} else {
				pPoint->Type = POINT_TYPE_PORT;
				pPoint->u.portp = portp;
			}
			}
			break;
		case POINT_TYPE_NODE_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pNodeList = &pPoint->u.nodeList;

			for (i=ListHead(pNodeList); i != NULL; i = ListNext(pNodeList, i)) {
				NodeData *nodep = (NodeData*)ListObj(i);
				PortData *portp = is_portnunm ?
				                  FindNodePort(nodep, portnum) :
				                  FindNodePortId(nodep, portid);
				if (portp) {
					status = PointListAppend(&newPoint, POINT_TYPE_PORT_LIST, portp);
					if (FSUCCESS != status)
						goto fail;
				}
			}
			if (! PointValid(&newPoint)) {
				fabric_fail = TRUE;
			} else {
				PointFabricCompress(&newPoint);
				status = PointFabricCopy(pPoint, &newPoint);
				PointDestroy(&newPoint);
				if (FSUCCESS != status)
					goto fail;
			}
			}
			break;
#if !defined(VXWORKS) || defined(BUILD_DMC)
		case POINT_TYPE_IOC:
			{
			PortData *portp = is_portnunm ?
			                  FindNodePort(pPoint->u.iocp->ioup->nodep, portnum) :
			                  FindNodePortId(pPoint->u.iocp->ioup->nodep, portid);
			if (! portp) {
				fabric_fail = TRUE;
			} else {
				pPoint->Type = POINT_TYPE_PORT;
				pPoint->u.portp = portp;
			}
			}
			break;
		case POINT_TYPE_IOC_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pIocList = &pPoint->u.iocList;

			for (i=ListHead(pIocList); i != NULL; i = ListNext(pIocList, i)) {
				IocData *iocp = (IocData*)ListObj(i);
				PortData *portp = is_portnunm ?
				                  FindNodePort(iocp->ioup->nodep, portnum) :
				                  FindNodePortId(iocp->ioup->nodep, portid);
				if (portp) {
					status = PointListAppend(&newPoint, POINT_TYPE_PORT_LIST, portp);
					if (FSUCCESS != status)
						goto fail;
				}
			}
			if (! PointValid(&newPoint)) {
				fabric_fail = TRUE;
			} else {
				PointFabricCompress(&newPoint);
				status = PointFabricCopy(pPoint, &newPoint);
				PointDestroy(&newPoint);
				if (FSUCCESS != status)
					goto fail;
			}
			}
			break;
#endif
		case POINT_TYPE_SYSTEM:
			{
			cl_map_item_t *p;
			SystemData *systemp = pPoint->u.systemp;

			for (p=cl_qmap_head(&systemp->Nodes); p != cl_qmap_end(&systemp->Nodes); p = cl_qmap_next(p)) {
				NodeData *nodep = PARENT_STRUCT(p, NodeData, SystemNodesEntry);
				PortData *portp = is_portnunm ?
				                  FindNodePort(nodep, portnum) :
				                  FindNodePortId(nodep, portid);
				if (portp) {
					status = PointListAppend(&newPoint, POINT_TYPE_PORT_LIST, portp);
					if (FSUCCESS != status)
						goto fail;
				}
			}
			if (! PointValid(&newPoint)) {
				fabric_fail = TRUE;
			} else {
				PointFabricCompress(&newPoint);
				status = PointFabricCopy(pPoint, &newPoint);
				PointDestroy(&newPoint);
				if (FSUCCESS != status)
					goto fail;
			}
			}
			break;
		}
	}

	// for FIND_FLAG_ENODE leave any selected ExpectedNodes as is
	
	if ((find_flag & FIND_FLAG_ELINK) && callback) {
		// rather than retain extra information to indicate which side of
		// the ELINK matched the original search criteria (nodeguid, nodedesc,
		// nodedescpat, nodetype) we use a callback to
		// recompare with the original criteria and also check against portnum/portid
		// N/A search critieria: nodedetpat, iocname, ioctype, iocnamepat
		switch (pPoint->ElinkType) {
		case POINT_ELINK_TYPE_NONE:
			break;
		case POINT_ELINK_TYPE_LINK:
			if (! (*callback)(pPoint->u4.elinkp, nodecmp, portnum, portid)){
				PointElinkDestroy(pPoint);
			}
			break;
		case POINT_ELINK_TYPE_LINK_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pElinkList = &pPoint->u4.elinkList;

			for (i=ListHead(pElinkList); i != NULL; i = ListNext(pElinkList, i)) {
				ExpectedLink *elinkp = (ExpectedLink*)ListObj(i);
				if ((*callback)(elinkp, nodecmp, portnum, portid)){
					status = PointElinkListAppend(&newPoint, POINT_ELINK_TYPE_LINK_LIST, elinkp);
					if (FSUCCESS != status)
						goto fail;
				}
			}
			if (! PointValid(&newPoint)) {
				PointElinkDestroy(pPoint);
			} else {
				PointElinkCompress(&newPoint);
				status = PointElinkCopy(pPoint, &newPoint);
				PointDestroy(&newPoint);
				if (FSUCCESS != status)
					goto fail;
			}
			}
			break;
		}
	} else {
		DEBUG_ASSERT(pPoint->ElinkType == POINT_ELINK_TYPE_NONE);
	}

	if (fabric_fail && pPoint->EnodeType == POINT_ENODE_TYPE_NONE
			&& pPoint->EsmType == POINT_ESM_TYPE_NONE
			&& pPoint->ElinkType == POINT_ELINK_TYPE_NONE) {
		// we failed fabric and had no enode, esm, nor elink, so output a good
		// message specific to the fabric point we must have started with
		switch (pPoint->Type) {
		case POINT_TYPE_NONE:
		case POINT_TYPE_PORT:
		case POINT_TYPE_PORT_LIST:
		case POINT_TYPE_NODE_PAIR_LIST:
			ASSERT(0);	// should not get here
			break;
		case POINT_TYPE_NODE:
			fprintf(stderr, "%s: Node %.*s GUID 0x%016"PRIx64" ",
				g_Top_cmdname, STL_NODE_DESCRIPTION_ARRAY_SIZE,
				(char*)pPoint->u.nodep->NodeDesc.NodeString,
				pPoint->u.nodep->NodeInfo.NodeGUID);
			if (is_portnunm) {
				fprintf(stderr, "Port %u Not Found\n", portnum);
			} else {
				fprintf(stderr, "PortId %s Not Found\n", portid);
			}
			break;
		case POINT_TYPE_NODE_LIST:
#if !defined(VXWORKS) || defined(BUILD_DMC)
		case POINT_TYPE_IOC_LIST:
#endif
		case POINT_TYPE_SYSTEM:
			if (is_portnunm) {
				fprintf(stderr, "%s: Port %u Not Found\n", g_Top_cmdname, portnum);
			} else {
				fprintf(stderr, "%s: PortId %s Not Found\n", g_Top_cmdname, portid);
			}
			break;
#if !defined(VXWORKS) || defined(BUILD_DMC)
		case POINT_TYPE_IOC:
			fprintf(stderr, "%s: IOC %.*s GUID 0x%016"PRIx64" ",
					g_Top_cmdname, IOC_IDSTRING_SIZE,
					(char*)pPoint->u.iocp->IocProfile.IDString,
					pPoint->u.iocp->IocProfile.IocGUID);
			if (is_portnunm) {
				fprintf(stderr, "Port %u Not Found\n", portnum);
			} else {
				fprintf(stderr, "PortId %s Not Found\n", portid);
			}
			break;
#endif
		}
		status = FNOT_FOUND;
	} else if (! PointValid(pPoint)) {
		if (is_portnunm) {
			fprintf(stderr, "%s: Port %u Not Found\n", g_Top_cmdname, portnum);
		} else {
			fprintf(stderr, "%s: PortId %s Not Found\n", g_Top_cmdname, portid);
		}
		status = FNOT_FOUND;
	} else {
		return FSUCCESS;
	}

fail:
	PointDestroy(&newPoint);
	PointDestroy(pPoint);
	return status;
}

static boolean PointPortCompare(PortSelector *portselp, uint8 portnum, char* portid)
{
	if (portid && portid[0]) {
		return portselp && portselp->PortId
		       && (strncmp(portselp->PortId, portid, TINY_STR_ARRAY_SIZE) == 0);
	} else {
		return portselp && portselp->gotPortNum && (portselp->PortNum == portnum);
	}
}

static boolean PointPortElinkCompareNodeGuid(ExpectedLink *elinkp, void *nodecmp, uint8 portnum, char* portid)
{
	EUI64 guid = *(EUI64*)nodecmp;

	return ((elinkp->portselp1 && elinkp->portselp1->NodeGUID == guid
				 && PointPortCompare(elinkp->portselp1, portnum, portid))
			|| (elinkp->portselp2 && elinkp->portselp2->NodeGUID == guid
				 && PointPortCompare(elinkp->portselp2, portnum, portid)));
}

static FSTATUS ParseIfAddrPoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	char *param;
	EUI64 guid;
	FSTATUS status;
	
	ASSERT(! PointValid(pPoint));
	if (FSUCCESS != StringToUint64(&guid, arg, pp, 0, TRUE))  {
		fprintf(stderr, "%s: Invalid Node IfAddr format: '%s'\n",
						g_Top_cmdname, arg);
		return FINVALID_PARAMETER;
	}
	if (! guid) {
		fprintf(stderr, "%s: Invalid Node IfAddr: 0x%016"PRIx64"\n",
						g_Top_cmdname, guid);
		return FINVALID_PARAMETER;
	}
	status = FindNodeGuidPoint(fabricp, guid, pPoint, find_flag, 0);
	if (FSUCCESS != status)
		return status;

	if (NULL != (param = ComparePrefix(*pp, ":port:"))) {
		return ParsePointPort(fabricp, param, TRUE, pPoint, find_flag,
								PointPortElinkCompareNodeGuid, &guid, pp);
	} else if (NULL != (param = ComparePrefix(*pp, ":portid:"))) {
		return ParsePointPort(fabricp, param, FALSE, pPoint, find_flag,
								PointPortElinkCompareNodeGuid, &guid, pp);
	} else {
		return FSUCCESS;
	}
}

static FSTATUS ParseChassisIDPoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	EUI64 guid;
	char *param;

	ASSERT(! PointValid(pPoint));
	if (FSUCCESS != StringToUint64(&guid, arg, pp, 0, TRUE))  {
		fprintf(stderr, "%s: Invalid Chassis ID format: '%s'\n",
						g_Top_cmdname, arg);
		return FINVALID_PARAMETER;
	}
	if (! guid) {
		fprintf(stderr, "%s: Invalid Chassis ID: 0x%016"PRIx64"\n",
						g_Top_cmdname, guid);
		return FINVALID_PARAMETER;
	}
	if (0 == (find_flag & FIND_FLAG_FABRIC))
		return FINVALID_OPERATION;
	if (find_flag & FIND_FLAG_FABRIC) {
		pPoint->u.systemp = FindSystemGuid(fabricp, guid);
		if (pPoint->u.systemp)
			pPoint->Type = POINT_TYPE_SYSTEM;
	}
	// N/A for FIND_FLAG_ENODE, FIND_FLAG_ESM and FIND_FLAG_ELINK
	if (PointValid(pPoint)) {
		if (NULL != (param = ComparePrefix(*pp, ":port:"))) {
			return ParsePointPort(fabricp, param, TRUE, pPoint, find_flag, NULL, NULL, pp);
		} else if (NULL != (param = ComparePrefix(*pp, ":portid:"))) {
			return ParsePointPort(fabricp, param, FALSE, pPoint, find_flag, NULL, NULL, pp);
		}
		return FSUCCESS;
	} else {
		fprintf(stderr, "%s: Chassis ID Not Found: 0x%016"PRIx64"\n",
						g_Top_cmdname, guid);
		return FNOT_FOUND;
	}
}

static boolean PointPortElinkCompareNodeName(ExpectedLink *elinkp, void *nodecmp, uint8 portnum, char* portid)
{
	char *name = (char*)nodecmp;

	return ((elinkp->portselp1 && elinkp->portselp1->NodeDesc
				&& PointPortCompare(elinkp->portselp1, portnum, portid)
				&& 0 == strncmp(elinkp->portselp1->NodeDesc,
								name, STL_NODE_DESCRIPTION_ARRAY_SIZE))
			|| (elinkp->portselp2 && elinkp->portselp2->NodeDesc
				&& PointPortCompare(elinkp->portselp2, portnum, portid)
				&& 0 == strncmp(elinkp->portselp2->NodeDesc,
								name, STL_NODE_DESCRIPTION_ARRAY_SIZE)));
}

static FSTATUS ParseNodeNamePoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	char *p;
	char Name[STL_NODE_DESCRIPTION_ARRAY_SIZE+1];
	char *param;
	FSTATUS status;

	ASSERT(! PointValid(pPoint));
	p = strchr(arg, ':');
	if (p) {
		if (p == arg) {
			fprintf(stderr, "%s: Invalid Node name format: '%s'\n",
							g_Top_cmdname, arg);
			return FINVALID_PARAMETER;
		}
		if (p - arg > STL_NODE_DESCRIPTION_ARRAY_SIZE) {
			fprintf(stderr, "%s: Node name Not Found (too long): %.*s\n",
							g_Top_cmdname, (int)(p-arg), arg);
			return FINVALID_PARAMETER;
		}
		StringCopy(Name, arg, sizeof(Name));
		Name[p-arg] = '\0';
		*pp = p;
		arg = Name;
	} else {
		*pp = arg + strlen(arg);
	}

	status = FindNodeNamePoint(fabricp, arg, pPoint, find_flag, 0);
	if (FSUCCESS != status)
		return status;

	if (NULL != (param = ComparePrefix(*pp, ":port:"))) {
		return ParsePointPort(fabricp, param, TRUE, pPoint, find_flag,
								PointPortElinkCompareNodeName, arg, pp);
	} else if (NULL != (param = ComparePrefix(*pp, ":portid:"))) {
		return ParsePointPort(fabricp, param, FALSE, pPoint, find_flag,
								PointPortElinkCompareNodeName, arg, pp);
	} else {
		return FSUCCESS;
	}
}

#ifndef __VXWORKS__
static boolean PointPortElinkCompareNodeNamePat(ExpectedLink *elinkp, void *nodecmp, uint8 portnum, char* portid)
{
	char *pattern = (char*)nodecmp;

	return ( (elinkp->portselp1 && elinkp->portselp1->NodeDesc
				&& PointPortCompare(elinkp->portselp1, portnum, portid)
				&& 0 == fnmatch(pattern, elinkp->portselp1->NodeDesc, 0))
			|| (elinkp->portselp2 && elinkp->portselp2->NodeDesc
				&& PointPortCompare(elinkp->portselp2, portnum, portid)
				&& 0 == fnmatch(pattern, elinkp->portselp2->NodeDesc, 0)));
}

/* Initialize the nodepatPairs list*/
static FSTATUS InitNodePatPairs(NodePairList_t *nodePatPairs)
{
	//Initialize the left side of the list here.
	ListInitState(&nodePatPairs->nodePairList1);
	if (! ListInit(&nodePatPairs->nodePairList1, MIN_LIST_ITEMS)) {
		fprintf(stderr, "%s: unable to allocate memory\n", g_Top_cmdname);
		return FINSUFFICIENT_MEMORY;
	}

	//Initialize the right side of the list here.
	ListInitState(&nodePatPairs->nodePairList2);
	if (! ListInit(&nodePatPairs->nodePairList2, MIN_LIST_ITEMS)) {
		fprintf(stderr, "%s: unable to allocate memory\n", g_Top_cmdname);
		if(&nodePatPairs->nodePairList1)
			ListDestroy(&nodePatPairs->nodePairList1);
		return FINSUFFICIENT_MEMORY;
	}
	return FSUCCESS;
}

/* Delete the nodepatpairs list*/
FSTATUS DeleteNodePatPairs(NodePairList_t *nodePatPairs)
{
	if(nodePatPairs) {
		//delete the left side of the list.
		ListDestroy(&nodePatPairs->nodePairList1);
		//delete the right side of the list.
		ListDestroy(&nodePatPairs->nodePairList2);
	}
	return FSUCCESS;
}

/* Parse the node pairs/nodes file */
static FSTATUS ParseNodePairPatFilePoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, uint8 pair_flag, char **pp)
{
	char *p, *pEol;
	char patternLine[STL_NODE_DESCRIPTION_ARRAY_SIZE*2+NODE_PAIR_DELIMITER_SIZE+1];
	char pattern[STL_NODE_DESCRIPTION_ARRAY_SIZE+1];
	FSTATUS status;
	char nodePatFileName[FILENAME] = {0};
	struct stat fileStat;
	FILE *fp;
	NodePairList_t nodePatPairs;

	ASSERT(PointIsInInit(pPoint));

	if (0 == pair_flag)
		return FINVALID_OPERATION;

	if (NULL == arg) {
		fprintf(stderr, "%s: Node pattern Filename missing\n", g_Top_cmdname);
		return FINVALID_PARAMETER;
	}

	if (strlen(arg) > FILENAME - 1) {
		fprintf(stderr, "%s: Node pattern Filename too long: %.*s\n",
						g_Top_cmdname, (int)strlen(arg), arg);
		return FINVALID_PARAMETER;
	}

	if ((PAIR_FLAG_NODE != pair_flag) && (PAIR_FLAG_NONE != pair_flag) ) {
		fprintf(stderr, "%s: Pair flag argument is invalid: %d\n",
						g_Top_cmdname, pair_flag);
		return FINVALID_PARAMETER;
	}
	//Get file name
	StringCopy(nodePatFileName, arg, (int)sizeof(nodePatFileName));
	nodePatFileName[strlen(arg)] = '\0';

	//There are no further focus to evaluate for node pair list
	*pp = arg + strlen(arg);

	//Open file
	if ((fp = fopen(nodePatFileName, "r")) == NULL) {
		fprintf(stderr, "Error opening file %s for input: %s\n", nodePatFileName, strerror(errno));
		return FINVALID_PARAMETER;
	}

	// Check if file is present
	if (fstat(fileno(fp), &fileStat) < 0) {
		fprintf(stderr, "Error taking stat of file {%s}: %s\n",
			nodePatFileName, strerror(errno));
		status = FINVALID_PARAMETER;
		goto fail;
	}

	memset(patternLine, 0, sizeof(patternLine));

	//Get one line at a time
	while(fgets(patternLine, sizeof(patternLine), fp) != NULL){
		//remove newline
		if ((pEol = strrchr(patternLine, '\n')) != NULL) {
			*pEol= '\0';
		}
		//When node pairs are given
		if (PAIR_FLAG_NODE == pair_flag ) {
			p = strchr(patternLine, ':');
			if (p) {
				memset(pattern, 0, sizeof(pattern));
				//just log the error meesage
				if (p - patternLine > STL_NODE_DESCRIPTION_ARRAY_SIZE) {
					fprintf(stderr, "%s: Left side node name Not Found (too long): %.*s\n",
						g_Top_cmdname, (int)(p - patternLine), patternLine);
				}
				// copy the Left side of node pair. +1 for '\0'
				StringCopy(pattern, patternLine, (p - patternLine + 1));
				pattern[(p - patternLine) + 1] = '\0';
				status = InitNodePatPairs(&nodePatPairs);
				if(FSUCCESS != status){
					fprintf(stderr, "%s: Insufficient Memory\n",g_Top_cmdname );
					goto fail;
				}
				// Find all the nodes that match the pattern for the left side
				status = FindNodePatPairs(fabricp, pattern, &nodePatPairs, find_flag, LSIDE_PAIR);
				// When there is invalid entry in the Left side of each line, the entire line is skipped
				if ((FSUCCESS != status)){
					DeleteNodePatPairs(&nodePatPairs);
					continue;
				}
				memset(pattern, 0, sizeof(pattern));
				//When there is invalid entry in the right side don't mark error as corresponding left side entry could be a Switch
				if (pEol - p <= STL_NODE_DESCRIPTION_ARRAY_SIZE) {
					// copy the Right side of node pair. +1 is for ':'
					StringCopy(pattern, patternLine + (p - patternLine + 1), (pEol - p + 1));
					// Find all the nodes that match the pattern for the right side
					status = FindNodePatPairs(fabricp, pattern, &nodePatPairs, find_flag, RSIDE_PAIR);
					// Only when there is insufficient memory error mark as error and return
					if ((FSUCCESS != status) && (FINVALID_OPERATION != status) &&
						(FNOT_FOUND != status)){
						DeleteNodePatPairs(&nodePatPairs);
						goto fail;
					}
				} else {
					//just log the error meesage
					fprintf(stderr, "%s: Right side node name (too long): %.*s\n",
						g_Top_cmdname, (int)(pEol - p), p);
				}
			}else {
				//just log error message
				fprintf(stderr, "%s: Node pair is missing: %.*s\n",
					g_Top_cmdname, (int)sizeof(patternLine), patternLine);
			}
			// The complete node pair List N*M  is populated for a single line in file
			status = PointPopulateNodePairList(pPoint, &nodePatPairs);
			if (FSUCCESS != status) {
				//Log error and return if it fails to fom N*M list
				fprintf(stderr, "%s: Error creating node pairs\n", g_Top_cmdname);
				DeleteNodePatPairs(&nodePatPairs);
				goto fail;
			}
			// Now the line is parsed and nadepat pairs are created, so free nodePatPairs
			DeleteNodePatPairs(&nodePatPairs);
		//When only one node is given
		} else {
			if (strlen(patternLine) <= STL_NODE_DESCRIPTION_ARRAY_SIZE) {
				// Only when there is insufficient memory error or invalid operation error mark as error and return
				// else proceed with next node in list
				status = FindNodeNamePatPointUncompress(fabricp, patternLine, pPoint, find_flag);
				if ((FSUCCESS != status) && (FNOT_FOUND != status))
					goto fail;
			} else {
				//just log the error message and parse next line
				fprintf(stderr, "%s: Node name (too long): %.*s\n",
					g_Top_cmdname, (int)sizeof(patternLine), patternLine);
			}
		}
		memset(patternLine, 0, sizeof(patternLine));
	}
	PointCompress(pPoint);
	fclose(fp);
	return FSUCCESS;

fail:
	fclose(fp);
	return status;
}

static FSTATUS ParseNodeNamePatPoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	char *p;
	char Pattern[STL_NODE_DESCRIPTION_ARRAY_SIZE*5+1];
	char *param;
	FSTATUS status;

	ASSERT(! PointValid(pPoint));
	p = strchr(arg, ':');
	if (p) {
		if (p == arg) {
			fprintf(stderr, "%s: Invalid Node name pattern format: '%s'\n",
							g_Top_cmdname, arg);
			return FINVALID_PARAMETER;
		}
		if (p - arg > sizeof(Pattern)-1) {
			fprintf(stderr, "%s: Node name pattern too long: %.*s\n",
							g_Top_cmdname, (int)(p-arg), arg);
			return FINVALID_PARAMETER;
		}
		StringCopy(Pattern, arg, sizeof(Pattern));
		Pattern[p-arg] = '\0';
		*pp = p;
		arg = Pattern;
	} else {
		*pp = arg + strlen(arg);
	}

	status = FindNodeNamePatPoint(fabricp, arg, pPoint, find_flag);
	if (FSUCCESS != status)
		return status;

	if (NULL != (param = ComparePrefix(*pp, ":port:"))) {
		return ParsePointPort(fabricp, param, TRUE, pPoint, find_flag,
								PointPortElinkCompareNodeNamePat, arg, pp);
	} else if (NULL != (param = ComparePrefix(*pp, ":portid:"))) {
		return ParsePointPort(fabricp, param, FALSE, pPoint, find_flag,
								PointPortElinkCompareNodeNamePat, arg, pp);
	} else {
		return FSUCCESS;
	}
}

static FSTATUS ParseNodeDetPatPoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	char *p;
	char Pattern[NODE_DETAILS_STRLEN*5+1];
	char *param;
	FSTATUS status;

	ASSERT(! PointValid(pPoint));
	p = strchr(arg, ':');
	if (p) {
		if (p == arg) {
			fprintf(stderr, "%s: Invalid Node Details pattern format: '%s'\n",
							g_Top_cmdname, arg);
			return FINVALID_PARAMETER;
		}
		if (p - arg > sizeof(Pattern)-1) {
			fprintf(stderr, "%s: Node Details pattern too long: %.*s\n",
							g_Top_cmdname, (int)(p-arg), arg);
			return FINVALID_PARAMETER;
		}
		StringCopy(Pattern, arg, sizeof(Pattern));
		Pattern[p-arg] = '\0';
		*pp = p;
		arg = Pattern;
	} else {
		*pp = arg + strlen(arg);
	}

	if (0 == (find_flag & (FIND_FLAG_FABRIC|FIND_FLAG_ENODE))
		&& ! QListCount(&fabricp->ExpectedFIs)
		&& ! QListCount(&fabricp->ExpectedSWs))
		fprintf(stderr, "%s: Warning: No Node Details supplied via topology_input\n", g_Top_cmdname);
	status = FindNodeDetailsPatPoint(fabricp, arg, pPoint, find_flag);
	if (FSUCCESS != status)
		return status;

	if (NULL != (param = ComparePrefix(*pp, ":port:"))) {
		return ParsePointPort(fabricp, param, TRUE, pPoint, find_flag, NULL, NULL, pp);
	} else if (NULL != (param = ComparePrefix(*pp, ":portid:"))) {
		return ParsePointPort(fabricp, param, FALSE, pPoint, find_flag, NULL, NULL, pp);
	} else {
		return FSUCCESS;
	}
}
#endif /* __VXWORKS__ */

static boolean PointPortElinkCompareNodeType(ExpectedLink *elinkp, void *nodecmp, uint8 portnum, char* portid)
{
	NODE_TYPE type = *(NODE_TYPE*)nodecmp;

	return ((elinkp->portselp1 && elinkp->portselp1->NodeType == type
				 && PointPortCompare(elinkp->portselp1, portnum, portid) )
			|| (elinkp->portselp2 && elinkp->portselp2->NodeType == type
				 && PointPortCompare(elinkp->portselp2, portnum, portid)));
}

static FSTATUS ParseNodeTypePoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	char *p;
	char *param;
	FSTATUS status;
	NODE_TYPE type;
	char* type_name = NULL;

	ASSERT(! PointValid(pPoint));
	p = strchr(arg, ':');
	if (p) {
		if (p == arg) {
			fprintf(stderr, "%s: Invalid Node type format: '%s'\n",
							g_Top_cmdname, arg);
			return FINVALID_PARAMETER;
		}
		if (p - arg > UNREPORTED_LEN) {
			fprintf(stderr, "%s: Invalid Node type: %.*s\n",
							g_Top_cmdname, (int)(p-arg), arg);
			return FINVALID_PARAMETER;
		}
		*pp = p;
	} else {
		if (strlen(arg) > UNREPORTED_LEN) {
			fprintf(stderr, "%s: Invalid Node type: %s\n", g_Top_cmdname, arg);
			return FINVALID_PARAMETER;
		}
		*pp = arg + strlen(arg);
	}
	if (strncasecmp(arg, "NIC", 3) == 0)
		type = STL_NODE_FI;
	else if (strncasecmp(arg, "SW", 2) == 0)
		type = STL_NODE_SW;
	else if (strncasecmp(arg, UNREPORTED, UNREPORTED_LEN) == 0) {
	        type = STL_NODE_FI; // unnecessary, but can make klocwork happy
		type_name = UNREPORTED;
	} else {
		fprintf(stderr, "%s: Invalid Node type: %.*s\n", g_Top_cmdname, 2, arg);
		*pp = arg; /* back up to start of type for syntax error */
		return FINVALID_PARAMETER;
	}

	status = FindNodeTypePoint(fabricp, type, type_name, pPoint, find_flag);
	if (FSUCCESS != status)
		return status;

	if (NULL != (param = ComparePrefix(*pp, ":port:"))) {
		return ParsePointPort(fabricp, param, TRUE, pPoint, find_flag,
								PointPortElinkCompareNodeType, &type, pp);
	} else if (NULL != (param = ComparePrefix(*pp, ":portid:"))) {
		return ParsePointPort(fabricp, param, FALSE, pPoint, find_flag,
								PointPortElinkCompareNodeType, &type, pp);
	} else {
		return FSUCCESS;
	}
}

static FSTATUS ParseRatePoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	char *p;
	char Rate[UNREPORTED_LEN+1];	// UNREPORTED is largest possible rate name
	FSTATUS status;
	int len;
	uint32 rate;
	char* rate_name = NULL;

	ASSERT(! PointValid(pPoint));
	p = strchr(arg, ':');
	if (p) {
		if (p == arg) {
			fprintf(stderr, "%s: Invalid Rate format: '%s'\n",
							g_Top_cmdname, arg);
			return FINVALID_PARAMETER;
		}
		if (p - arg > sizeof(Rate)-1) {
			fprintf(stderr, "%s: Invalid Rate: %.*s\n",
							g_Top_cmdname, (int)(p-arg), arg);
			return FINVALID_PARAMETER;
		}
		len = (int)(p-arg);
		StringCopy(Rate, arg, sizeof(Rate));
		Rate[len] = '\0';
		*pp = p;
		arg = Rate;
	} else {
		len = strlen(arg);
		*pp = arg + len;
	}
	if (strncmp(arg, "12.5g", len) == 0)
		rate = IB_STATIC_RATE_14G;
	else if (strncmp(arg, "25g", len) == 0)
		rate = IB_STATIC_RATE_25G;
	else if (strncmp(arg, "37.5", len) == 0)
		rate = IB_STATIC_RATE_40G;
	else if (strncmp(arg, "50g", len) == 0)
		rate = IB_STATIC_RATE_56G;
	else if (strncmp(arg, "75g", len) == 0)
		rate = IB_STATIC_RATE_80G;
	else if (strncmp(arg, "100g", len) == 0)
		rate = IB_STATIC_RATE_100G;
	else if (strncmp(arg, "150g", len) == 0)
		rate = IB_STATIC_RATE_168G;
	else if (strncmp(arg, "200g", len) == 0)
		rate = IB_STATIC_RATE_200G;
	else if (strncmp(arg, "400g", len) == 0)
		rate = IB_STATIC_RATE_400G;
	else if (strncmp(arg, UNREPORTED, len) == 0) {
	        rate = IB_STATIC_RATE_DONTCARE; // unnecessary, but can make klocwork happy
		rate_name = UNREPORTED;
	} else {
		fprintf(stderr, "%s: Invalid Rate: %.*s\n", g_Top_cmdname, len, arg);
		*pp -= len;	/* back up for syntax error report */
		return FINVALID_PARAMETER;
	}

	status = FindRatePoint(fabricp, rate, rate_name, pPoint, find_flag);
	return status;
}

static FSTATUS ParsePortStatePoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	char *p;
	char State[UNREPORTED_LEN+1];	// UNREPORTED is largest possible state name
	FSTATUS status;
	int len;
	ETH_PORT_STATE state;
	char* state_name = NULL;

	ASSERT(! PointValid(pPoint));
	p = strchr(arg, ':');
	if (p) {
		if (p == arg) {
			fprintf(stderr, "%s: Invalid Port State format: '%s'\n",
							g_Top_cmdname, arg);
			return FINVALID_PARAMETER;
		}
		if (p - arg > sizeof(State)-1) {
			fprintf(stderr, "%s: Invalid Port State: %.*s\n",
							g_Top_cmdname, (int)(p-arg), arg);
			return FINVALID_PARAMETER;
		}
		len = (int)(p-arg);
		StringCopy(State, arg, sizeof(State));
		State[len] = '\0';
		*pp = p;
		arg = State;
	} else {
		len = strlen(arg);
		*pp = arg + len;
	}

	if (strncasecmp(arg, "down", len) == 0)
		state = ETH_PORT_DOWN;
	else if (strncasecmp(arg, "up", len) == 0)
		state = ETH_PORT_UP;
	else if (strncasecmp(arg, "testing", len) == 0)
		state = ETH_PORT_TESTING;
	else if (strncasecmp(arg, "unknown", len) == 0)
		state = ETH_PORT_UNKNOWN;
	else if (strncasecmp(arg, "dormant", len) == 0)
		state = ETH_PORT_DORMANT;
	else if (strncasecmp(arg, "notpresent", len) == 0)
		state = ETH_PORT_NOT_PRESENT;
	else if (strncasecmp(arg, "lowerlayerdown", len) == 0)
		state = ETH_PORT_LOWER_LAYER_DOWN;
	else if (strncasecmp(arg, "notactive", len) == 0)
		state = PORT_STATE_SEARCH_NOTACTIVE;
	else if (strncasecmp(arg, UNREPORTED, len) == 0) {
	        state = ETH_PORT_UNKNOWN; // unnecessary, but can make klocwork happy
		state_name = UNREPORTED;
	} else {
		fprintf(stderr, "%s: Invalid Port State: %.*s\n", g_Top_cmdname, len, arg);
		*pp -= len;	/* back up for syntax error report */
		return FINVALID_PARAMETER;
	}

	status = FindPortStatePoint(fabricp, state, state_name, pPoint, find_flag);
	return status;
}

static FSTATUS ParsePortPhysStatePoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	char *p;
	char PhysState[UNREPORTED_LEN+1];	// UNREPORTED is largest possible state name
	FSTATUS status;
	int len;
	ETH_PORT_PHYS_STATE physstate;
	char* physstate_name = NULL;

	ASSERT(! PointValid(pPoint));
	p = strchr(arg, ':');
	if (p) {
		if (p == arg) {
			fprintf(stderr, "%s: Invalid Port Phys State format: '%s'\n",
							g_Top_cmdname, arg);
			return FINVALID_PARAMETER;
		}
		if (p - arg > sizeof(PhysState)-1) {
			fprintf(stderr, "%s: Invalid Port Phys State: %.*s\n",
							g_Top_cmdname, (int)(p-arg), arg);
			return FINVALID_PARAMETER;
		}
		len = (int)(p-arg);
		StringCopy(PhysState, arg, sizeof(PhysState));
		PhysState[len] = '\0';
		*pp = p;
		arg = PhysState;
	} else {
		len = strlen(arg);
		*pp = arg + len;
	}

	if (strncasecmp(arg, "other", len) == 0)
		physstate = ETH_PORT_PHYS_OTHER;
	else if (strncasecmp(arg, "unknown", len) == 0)
		physstate = ETH_PORT_PHYS_UNKNOWN;
	else if (strncasecmp(arg, "operational", len) == 0)
		physstate = ETH_PORT_PHYS_OPERATIONAL;
	else if (strncasecmp(arg, "standby", len) == 0)
		physstate = ETH_PORT_PHYS_STANDBY;
	else if (strncasecmp(arg, "shutdown", len) == 0)
		physstate = ETH_PORT_PHYS_SHUTDOWN;
	else if (strncasecmp(arg, "reset", len) == 0)
		physstate = ETH_PORT_PHYS_RESET;
	else if (strncasecmp(arg, UNREPORTED, len) == 0) {
	        // only do searching by name for UNREPORTED. Others will be searching by value
	        // that is more efficient
	        physstate = ETH_PORT_PHYS_OTHER;// unnecessary, but can make klocwork happy
	        physstate_name = UNREPORTED;
	} else {
		fprintf(stderr, "%s: Invalid Port Phys State: %.*s\n", g_Top_cmdname, len, arg);
		*pp -= len;	/* back up for syntax error report */
		return FINVALID_PARAMETER;
	}

	status = FindPortPhysStatePoint(fabricp, physstate, physstate_name, pPoint, find_flag);
	return status;
}

static FSTATUS ParseMtuPoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	uint16 mtu;

	ASSERT(! PointValid(pPoint));
	if (FSUCCESS != StringToUint16(&mtu, arg, pp, 0, TRUE))  {
		fprintf(stderr, "%s: Invalid MTU format: '%s'\n", g_Top_cmdname, arg);
		return FINVALID_PARAMETER;
	}
	if (! mtu) {
		fprintf(stderr, "%s: Invalid MTU: %u\n",
						g_Top_cmdname, (unsigned)mtu);
		return FINVALID_PARAMETER;
	}

	return FindEthMtuPoint(fabricp, mtu, pPoint, find_flag);
}

static FSTATUS ParseLinkDetailsPatPoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	char *p;
	char Pattern[LINK_DETAILS_STRLEN*5+1];

	ASSERT(! PointValid(pPoint));
	p = strchr(arg, ':');
	if (p) {
		if (p == arg) {
			fprintf(stderr, "%s: Invalid Link Details pattern format: '%s'\n",
							g_Top_cmdname, arg);
			return FINVALID_PARAMETER;
		}
		if (p - arg > sizeof(Pattern)-1) {
			fprintf(stderr, "%s: Link Details pattern too long: %.*s\n",
							g_Top_cmdname, (int)(p-arg), arg);
			return FINVALID_PARAMETER;
		}
		StringCopy(Pattern, arg, sizeof(Pattern));
		Pattern[p-arg] = '\0';
		*pp = p;
		arg = Pattern;
	} else {
		*pp = arg + strlen(arg);
	}

	if (0 == (find_flag & (FIND_FLAG_FABRIC|FIND_FLAG_ELINK))
		&& ! QListCount(&fabricp->ExpectedLinks))
		fprintf(stderr, "%s: Warning: No Link Details supplied via topology_input\n", g_Top_cmdname);
	return FindLinkDetailsPatPoint(fabricp, arg, pPoint, find_flag);
}

static FSTATUS ParsePortDetailsPatPoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	char *p;
	char Pattern[PORT_DETAILS_STRLEN*5+1];

	ASSERT(! PointValid(pPoint));
	p = strchr(arg, ':');
	if (p) {
		if (p == arg) {
			fprintf(stderr, "%s: Invalid Port Details pattern format: '%s'\n",
							g_Top_cmdname, arg);
			return FINVALID_PARAMETER;
		}
		if (p - arg > sizeof(Pattern)-1) {
			fprintf(stderr, "%s: Port Details pattern too long: %.*s\n",
							g_Top_cmdname, (int)(p-arg), arg);
			return FINVALID_PARAMETER;
		}
		StringCopy(Pattern, arg, sizeof(Pattern));
		Pattern[p-arg] = '\0';
		*pp = p;
		arg = Pattern;
	} else {
		*pp = arg + strlen(arg);
	}

	if (0 == (find_flag & (FIND_FLAG_FABRIC|FIND_FLAG_ELINK))
		&& ! QListCount(&fabricp->ExpectedLinks))
		fprintf(stderr, "%s: Warning: No Port Details supplied via topology_input\n", g_Top_cmdname);
	return FindPortDetailsPatPoint(fabricp, arg, pPoint, find_flag);
}

static FSTATUS ParseLinkDownReasonPoint(FabricData_t *fabricp, char *arg, Point *pPoint, uint8 find_flag, char **pp)
{
	uint8 ldr = IB_UINT8_MAX;
	char *ldrp;
	
	ASSERT(! PointValid(pPoint));
	*pp = arg;
	if (NULL != (ldrp = ComparePrefix(arg, ":"))) {
		*pp = ldrp;
		if (FSUCCESS != StringToUint8(&ldr, ldrp, pp, 0, TRUE)) {
			fprintf(stderr, "%s: Invalid Link Down Reason format: '%s'\n", g_Top_cmdname, arg);
			return FINVALID_PARAMETER;
		}
	}

	return FindLinkDownReasonPoint(fabricp, ldr, pPoint, find_flag);
}

/* parse the arg string and find the mentioned Point
 * arg string formats:
 * 	gid:subnet:guid
 *	lid:lid
 *	lid:lid:node
 *	lid:lid:port:#
 *	lid:lid:veswport:#
 *	portguid:guid
 *	nodeguid:guid
 *	nodeguid:guid:port:#
 *	iocguid:guid
 *	iocguid:guid:port:#
 *	systemguid:guid
 *	systemguid:guid:port:#
 *	node:node name
 *	node:node name:port:#
 *	nodepat:node name pattern
 *	nodepat:node name pattern:port:#
 *	nodedetpat:node details pattern
 *	nodedetpat:node details pattern:port:#
 *	nodetype:node type
 *	nodetype:node type:port:#
 *	ioc:ioc name
 *	ioc:ioc name:port:#
 *	iocpat:ioc name pattern
 *	iocpat:ioc name pattern:port:#
 *	ioctype:ioc type
 *	ioctype:ioc type:port:#
 *	rate:rate string
 *	portstate:state string
 *	portphysstate:phys state string
 *	mtu:#
 *	cableinftype:cable type
 *	labelpat:cable label pattern
 *	led:on/off
 *	lengthpat:cable length pattern
 *	cabledetpat:cable details pattern
 *	cabinflenpat:cable info cable length pattern
 *	cabinfvendnamepat:cable info vendor name pattern
 *	cabinfvendpnpat:cable info vendor part number pattern
 *	cabinfvendrevpat:cable info vendor rev pattern
 *	cabinfvendsnpat:cable info vendor serial number pattern
 *	linkdetpat:cable details pattern
 *	portdetpat:port details pattern
 *	sm
 *  smdetpat:sm details pattern
 *  linkqual:link quality level
 *  linkqualLE:link quality level
 *  linkqualGE:link quality level
 */
FSTATUS ParsePoint(FabricData_t *fabricp, char* arg, Point* pPoint, uint8 find_flag, char **pp)
{
	char* param;
	FSTATUS status;

	*pp = arg;
	PointInit(pPoint);
	if (NULL != (param = ComparePrefix(arg, "ifid:"))) {
		status = ParseIfIDPoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "mgmtifaddr:"))) {
		status = ParseMgmtIfAddrPoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "ifaddr:"))) {
		status = ParseIfAddrPoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "chassisid:"))) {
		status = ParseChassisIDPoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "node:"))) {
		status = ParseNodeNamePoint(fabricp, param, pPoint, find_flag, pp);
#ifndef __VXWORKS__
	} else if (NULL != (param = ComparePrefix(arg, "nodepat:"))) {
		status = ParseNodeNamePatPoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "nodedetpat:"))) {
		status = ParseNodeDetPatPoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "nodepatfile:"))) {
		status = ParseNodePairPatFilePoint(fabricp, param, pPoint, find_flag, PAIR_FLAG_NONE, pp);
	} else if (NULL != (param = ComparePrefix(arg, "nodepairpatfile:"))) {
		status = ParseNodePairPatFilePoint(fabricp, param, pPoint, find_flag, PAIR_FLAG_NODE, pp);
#endif
	} else if (NULL != (param = ComparePrefix(arg, "nodetype:"))) {
		status = ParseNodeTypePoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "rate:"))) {
		status = ParseRatePoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "portstate:"))) {
		status = ParsePortStatePoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "portphysstate:"))) {
		status = ParsePortPhysStatePoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "mtucap:"))) {
		status = ParseMtuPoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "linkdetpat:"))) {
		status = ParseLinkDetailsPatPoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "portdetpat:"))) {
		status = ParsePortDetailsPatPoint(fabricp, param, pPoint, find_flag, pp);
	} else if (NULL != (param = ComparePrefix(arg, "ldr"))) {
		status = ParseLinkDownReasonPoint(fabricp, param, pPoint, find_flag, pp);
	} else {
		fprintf(stderr, "%s: Invalid format: '%s'\n", g_Top_cmdname, arg);
		return FINVALID_PARAMETER;
	}
	if (status == FINVALID_OPERATION) {
		fprintf(stderr, "%s: Format Not Allowed: '%s'\n", g_Top_cmdname, arg);
		return FINVALID_PARAMETER;
	}
	return status;
}

///////////////////////////////////////////////////////////////////////////////
// FIPortIteratorHead
//
// Description:
//	Finds the first non-SW port for each type in the point.
//
// Inputs:
//	pFIPortIterator	- Pointer to FIPortIterator object
//	pFocus	- Pointer to user's compare function
//
// Outputs:
//	None
//
// Returns:
//	Pointer to port data.
//
///////////////////////////////////////////////////////////////////////////////
PortData *FIPortIteratorHead(FIPortIterator *pFIPortIterator, Point *pFocus)
{
	pFIPortIterator->pPoint = pFocus;

	switch(pFocus->Type)
	{
		case POINT_TYPE_PORT:
		{
			//return NULL if port belongs to a switch else return the port
			if( STL_NODE_SW !=  pFocus->u.portp->nodep->NodeInfo.NodeType) {
				//Flag to indicate the port is the only port and it is already returned
				pFIPortIterator->u.PortIter.lastPortFlag = TRUE;
				return pFocus->u.portp;
			}
			return NULL;
		}
		break;
		case POINT_TYPE_PORT_LIST:
		{
			LIST_ITERATOR i;
			//Go through the port list to find the first NIC port. Skip all SW ports
			for (i = ListHead(&pFocus->u.portList);
				i != NULL; i = ListNext(&pFocus->u.portList, i)) {
				PortData *portp = (PortData *)ListObj(i);
				if(STL_NODE_SW == portp->nodep->NodeInfo.NodeType)
					continue;
				else {
					//Store where in the point the current iterator is.
					pFIPortIterator->u.PortListIter.currentPort = i;
					return portp;
				}
			}
			return NULL;
		}
		break;
		case POINT_TYPE_NODE:
		{
			NodeData *nodep = pFocus->u.nodep;
			// Get the first port only if it is a NIC node. Return NULL for switch nodes.
			if(STL_NODE_SW != nodep->NodeInfo.NodeType) {
				cl_map_item_t * p = cl_qmap_head(&nodep->Ports);
				if(p != cl_qmap_end(&nodep->Ports)) {
					PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
					//Store where in the point the current iterator is.
					pFIPortIterator->u.NodeIter.pCurrentPort = p;
					pFIPortIterator->u.NodeIter.lastNodeFlag = TRUE;
					return portp;
				}
			}
			return NULL;
		}
		break;
		case POINT_TYPE_NODE_LIST:
		{
			LIST_ITERATOR i;
			//Iterate over the node list to get the first NIC node.
			for (i = ListHead(&pFocus->u.nodeList);
				i != NULL; i = ListNext(&pFocus->u.nodeList, i)) {
				NodeData *nodep = (NodeData*)ListObj(i);

				//skip switches
				if(nodep->NodeInfo.NodeType == STL_NODE_SW)
					continue;
				else {
					cl_map_item_t *p = cl_qmap_head(&nodep->Ports);
					if(p != cl_qmap_end(&nodep->Ports)){
						PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
						//Store where in the point the current iterator is.
						pFIPortIterator->u.NodeListIter.currentNode = i;
						pFIPortIterator->u.NodeListIter.pCurrentPort = p;
						return portp;
					}
				}
			}
			return NULL;
		}
		break;
#if !defined(VXWORKS) || defined(BUILD_DMC)
		case POINT_TYPE_IOC:
		{
			NodeData *nodep = pFocus->u.iocp->ioup->nodep;
			//Get the first port for a NIC node.
			if(STL_NODE_SW != nodep->NodeInfo.NodeType) {
				cl_map_item_t *p = cl_qmap_head(&nodep->Ports);
				if(p != cl_qmap_end(&nodep->Ports))
				{
					PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
					//Store where in the point the current iterator is.
					pFIPortIterator->u.IocIter.pCurrentPort = p;
					pFIPortIterator->u.IocIter.lastIocFlag = TRUE;
					return portp;
				}
			}
			return NULL;
		}
		break;
		case POINT_TYPE_IOC_LIST:
		{
			LIST_ITERATOR i;
			//Iterate over the Ioc list to get the first NIC node.
			for (i = ListHead(&pFocus->u.iocList);
				i != NULL; i = ListNext(&pFocus->u.iocList, i)) {
				IocData *iocp = (IocData*)ListObj(i);
				NodeData *nodep = iocp->ioup->nodep;
				//skip switches
				if(nodep->NodeInfo.NodeType == STL_NODE_SW)
					continue;
				else {
					cl_map_item_t *p = cl_qmap_head(&nodep->Ports);
					if (p != cl_qmap_end(&nodep->Ports))
					{
						PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
						//Store where in the point the current iterator is.
						pFIPortIterator->u.IocListIter.currentNode = i;
						pFIPortIterator->u.IocListIter.pCurrentPort = p;
						return portp;
					}
				}
			}
			return NULL;
		}
		break;
#endif
		case POINT_TYPE_SYSTEM:
		{
			cl_map_item_t *s;
			//Iterate over the nodes in the system to get the first NIC node.
			for (s = cl_qmap_head(&pFocus->u.systemp->Nodes);
				s != cl_qmap_end(&pFocus->u.systemp->Nodes); s = cl_qmap_next(s)){
				NodeData *nodep = PARENT_STRUCT(s, NodeData, SystemNodesEntry);
				if(nodep->NodeInfo.NodeType == STL_NODE_SW)
					continue;
				else {
					cl_map_item_t *p = cl_qmap_head(&nodep->Ports);
					if (p != cl_qmap_end(&nodep->Ports))
					{
						PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
						//Store where in the point the current iterator is.
						pFIPortIterator->u.SystemIter.pCurrentNode = s;
						pFIPortIterator->u.SystemIter.pCurrentPort = p;
						pFIPortIterator->u.SystemIter.lastSystemFlag = TRUE;
						return portp;
					}
				}
			}
			return NULL;
		}
		break;
		case POINT_TYPE_NONE:
		case POINT_TYPE_NODE_PAIR_LIST:
		default:
			return NULL;
		break;
	}
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// FIPortIteratorNext
//
// Description:
//	Finds the next non-SW port for each type in the point
//
// Inputs:
//	pFIPortIterator	- Pointer to FIPortIterator object
//
// Outputs:
//	None
//
// Returns:
//	Pointer to port data.
//
///////////////////////////////////////////////////////////////////////////////
PortData *FIPortIteratorNext(FIPortIterator *pFIPortIterator)
{
	Point *pFocus = pFIPortIterator->pPoint;

	switch(pFocus->Type)
	{
		case POINT_TYPE_PORT:
		{
			//Check if port is iterated over
			if(pFIPortIterator->u.PortIter.lastPortFlag)
				pFIPortIterator->u.PortIter.lastPortFlag = FALSE;
			return NULL;
		}
		break;
		case POINT_TYPE_PORT_LIST:
		{
			LIST_ITERATOR i;
			//Get the next NIC port using the stored position of the iterator in the point
			for (i = ListNext(&pFocus->u.portList, pFIPortIterator->u.PortListIter.currentPort);
				i != NULL; i = ListNext(&pFocus->u.portList,i)) {
				PortData *portp = (PortData *)ListObj(i);
				if(STL_NODE_SW == portp->nodep->NodeInfo.NodeType)
					continue;
				else {
					//Store where in the point the current iterator is.
					pFIPortIterator->u.PortListIter.currentPort = i;
					return portp;
				}
			}
			return NULL;
		}
		break;
		case POINT_TYPE_NODE:
		{
			if (pFIPortIterator->u.NodeIter.lastNodeFlag){
				//Get the next port using the stored position of the iterator in the point. It is an NIC port.
				cl_map_item_t *p = cl_qmap_next(pFIPortIterator->u.NodeIter.pCurrentPort);
				if(p != cl_qmap_end(&pFocus->u.nodep->Ports)){
					PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
					//Store where in the point the current iterator is.
					pFIPortIterator->u.NodeIter.pCurrentPort = p;
					return portp;
				} else
					pFIPortIterator->u.NodeIter.lastNodeFlag = FALSE;
			}
			return NULL;
		}
		break;
		case POINT_TYPE_NODE_LIST:
		{
			LIST_ITERATOR i;
			//Get the next port of the node using the stored position of the iterator in the point. It is an NIC port.
			NodeData *nodep = (NodeData*)ListObj(pFIPortIterator->u.NodeListIter.currentNode);
			cl_map_item_t *p = cl_qmap_next(pFIPortIterator->u.NodeListIter.pCurrentPort);
			if(p != cl_qmap_end(&nodep->Ports)){
				PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
				//Store where in the point the current iterator is.
				pFIPortIterator->u.NodeIter.pCurrentPort = p;
				return portp;
			} else {
				//Iterate over the next node of the list
				for (i = ListNext(&pFocus->u.nodeList, pFIPortIterator->u.NodeListIter.currentNode);
					i != NULL; i = ListNext(&pFocus->u.nodeList, i)) {
					nodep = (NodeData*)ListObj(i);
					//skip switches
					if(nodep->NodeInfo.NodeType == STL_NODE_SW)
						continue;
					else {
						p = cl_qmap_head(&nodep->Ports);
						if(p != cl_qmap_end(&nodep->Ports)){
							PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
							//Store where in the point the current iterator is.
							pFIPortIterator->u.NodeListIter.currentNode = i;
							pFIPortIterator->u.NodeListIter.pCurrentPort = p;
							return portp;
						}
					}
				}
			}
			return NULL;
		}
		break;
#if !defined(VXWORKS) || defined(BUILD_DMC)
		case POINT_TYPE_IOC:
		{
			if (pFIPortIterator->u.IocIter.lastIocFlag){
				//Get the next port using the stored position of the iterator in the point. It is an NIC port.
				cl_map_item_t *p = cl_qmap_next(pFIPortIterator->u.IocIter.pCurrentPort);
				NodeData *nodep = pFocus->u.iocp->ioup->nodep;
				if(p != cl_qmap_end(&nodep->Ports)){
					PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
					//Store where in the point the current iterator is.
					pFIPortIterator->u.IocIter.pCurrentPort = p;
					return portp;
				} else
					pFIPortIterator->u.IocIter.lastIocFlag = FALSE;
			}
			return NULL;
		}
		break;
		case POINT_TYPE_IOC_LIST:
		{
			LIST_ITERATOR i;
			//Get the next port of the node using the stored position of the iterator in the point. It is an NIC port.
			IocData *iocp = (IocData*)ListObj(pFIPortIterator->u.IocListIter.currentNode);
			NodeData *nodep = iocp->ioup->nodep;
			cl_map_item_t *p = cl_qmap_next(pFIPortIterator->u.IocListIter.pCurrentPort);
			if(p != cl_qmap_end(&nodep->Ports)){
				PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
				//Store where in the point the current iterator is.
				pFIPortIterator->u.IocListIter.pCurrentPort = p;
				return portp;
			} else {
				//Iterate over the next Ioc of the list
				for (i = ListNext(&pFocus->u.iocList, pFIPortIterator->u.IocListIter.currentNode);
					i != NULL; i = ListNext(&pFocus->u.iocList, i)) {
					iocp = (IocData*)ListObj(i);
					nodep = iocp->ioup->nodep;
					//skip switches
					if(nodep->NodeInfo.NodeType == STL_NODE_SW)
						continue;
					else {
						p = cl_qmap_head(&nodep->Ports);
						if(p != cl_qmap_end(&nodep->Ports)){
							PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
							//Store where in the point the current iterator is.
							pFIPortIterator->u.IocListIter.currentNode = i;
							pFIPortIterator->u.IocListIter.pCurrentPort = p;
							return portp;
						}
					}
				}
			}
			return NULL;
		}
		break;
#endif
		case POINT_TYPE_SYSTEM:
		{
			cl_map_item_t *s;
			if (pFIPortIterator->u.SystemIter.lastSystemFlag){
				//Get the next port using the stored position of the iterator in the point. It is an NIC port.
				NodeData *nodep = PARENT_STRUCT(pFIPortIterator->u.SystemIter.pCurrentNode, NodeData, SystemNodesEntry);
				cl_map_item_t *p = cl_qmap_next(pFIPortIterator->u.SystemIter.pCurrentPort);
				if(p != cl_qmap_end(&nodep->Ports)){
					PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
					//Store where in the point the current iterator is.
					pFIPortIterator->u.SystemIter.pCurrentPort = p;
					return portp;
				} else {
					//Iterate over the next node of the list
					for (s = cl_qmap_next(pFIPortIterator->u.SystemIter.pCurrentNode);
						s != cl_qmap_end(&pFocus->u.systemp->Nodes); s = cl_qmap_next(s)) {
						nodep = PARENT_STRUCT(s, NodeData, SystemNodesEntry);
						//skip switches
						if(nodep->NodeInfo.NodeType == STL_NODE_SW)
							continue;
						else {
							p = cl_qmap_head(&nodep->Ports);
							if(p != cl_qmap_end(&nodep->Ports)){
								PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
								//Store where in the point the current iterator is.
								pFIPortIterator->u.SystemIter.pCurrentNode = s;
								pFIPortIterator->u.SystemIter.pCurrentPort = p;
								return portp;
							}
						}
					}
					pFIPortIterator->u.SystemIter.lastSystemFlag = FALSE;
				}
			}
			return NULL;
		}
		break;
		case POINT_TYPE_NONE:
		case POINT_TYPE_NODE_PAIR_LIST:
		default:
			return NULL;
		break;
	}
	return NULL;
}
