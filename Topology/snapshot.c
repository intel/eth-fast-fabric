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
#include "hpnmgt.h"

/* this file supports fabric snapshot generation and parsing */

IXML_FIELD PortStatusDataFields[] = {
	{ tag:"IfHCOutOctets", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, portXmitData) },
	{ tag:"IfHCInOctets", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, portRcvData) },
	{ tag:"IfHCOutUcastPkts", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, portXmitPkts) },
	{ tag:"IfHCInUcastPkts", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, portRcvPkts) },
	{ tag:"IfHCOutMulticastPkts", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, portMulticastXmitPkts) },
	{ tag:"IfHCInMulticastPkts", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, portMulticastRcvPkts) },
	{ tag:"Dot3HCStatsInternalMacTransmitErrors", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, dot3HCStatsInternalMacTransmitErrors) },
	{ tag:"Dot3HCStatsInternalMacReceiveErrors", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, portRcvErrors) },
	{ tag:"Dot3HCStatsSymbolErrors", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, localLinkIntegrityErrors) },
	{ tag:"IfOutErrors", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, ifOutErrors) },
	{ tag:"IfInErrors", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, ifInErrors) },
	{ tag:"IfInUnknownProtos", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, ifInUnknownProtos) },
	{ tag:"Dot3HCStatsAlignmentErrors", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, dot3HCStatsAlignmentErrors) },
	{ tag:"Dot3HCStatsFCSErrors", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, dot3HCStatsFCSErrors) },
	{ tag:"Dot3HCStatsFrameTooLongs", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, excessiveBufferOverruns) },
	{ tag:"IfOutDiscards", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, portXmitDiscards) },
	{ tag:"IfInDiscards", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, portRcvFECN) },
	{ tag:"Dot3StatsCarrierSenseErrors", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, dot3StatsCarrierSenseErrors) },
	{ tag:"Dot3StatsSingleCollisionFrames", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, dot3StatsSingleCollisionFrames) },
	{ tag:"Dot3StatsMultipleCollisionFrames", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, dot3StatsMultipleCollisionFrames) },
	{ tag:"Dot3StatsSQETestErrors", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, dot3StatsSQETestErrors) },
	{ tag:"Dot3StatsDeferredTransmissions", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, dot3StatsDeferredTransmissions) },
	{ tag:"Dot3StatsLateCollisions", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, dot3StatsLateCollisions) },
	{ tag:"Dot3StatsExcessiveCollisions", format:'U', IXML_FIELD_INFO(STL_PORT_COUNTERS_DATA, dot3StatsExcessiveCollisions) },
	{ NULL }
};

void PortStatusDataXmlOutput(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputStruct(state, tag, (STL_PORT_COUNTERS_DATA *)data, NULL, PortStatusDataFields);
}

// only output if value != NULL
void PortStatusDataXmlOutputOptional(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputOptionalStruct(state, tag, (STL_PORT_COUNTERS_DATA *)data, NULL, PortStatusDataFields);
}

static void PortStatusDataXmlParserEnd(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	STL_PORT_COUNTERS_DATA *pPortCountersData = (STL_PORT_COUNTERS_DATA *)object;
	PortData *portp = (PortData*)parent;

	if (! valid)	// missing mandatory fields
		goto failvalidate;

	if (portp->pPortCounters) {
		IXmlParserPrintError(state, "More than 1 PortStatus for Port");
		goto failinsert;
	}
	portp->pPortCounters = pPortCountersData;
	// NumLanesDown is set in PortDataXmlParserEnd()

	return;

failinsert:
failvalidate:
	MemoryDeallocate(pPortCountersData);
}

/****************************************************************************/
/* PortData Input/Output functions */

static void PortDataXmlFormatAttr(IXmlOutputState_t *state, void *data)
{
	PortData *portp = (PortData *)data;

	IXmlOutputPrint(state, " id=\"0x%016"PRIx64":%u\"", portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
}

/* bitfields needs special handling: LID */
static void PortDataXmlOutputEndPortLID(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputLIDValue(state, tag, ((PortData *)data)->EndPortLID);
}

static void PortDataXmlParserEndEndPortLID(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	uint32 value;
	
	if (IXmlParseUint32(state, content, len, &value))
		((PortData *)object)->EndPortLID = value;
}

/* bitfields needs special handling: PortState */
static void PortDataXmlOutputEthPortState(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputEthPortStateValue(state, tag, ((PortData *)data)->PortInfo.PortStates.s.PortState);
}

static void PortDataXmlParserEndPortState(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	uint8 value;
	
	if (IXmlParseUint8(state, content, len, &value))
		((PortData *)object)->PortInfo.PortStates.s.PortState = value;
}

/* bitfields needs special handling: InitReason */
static void PortDataXmlOutputInitReason(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputInitReasonValue(state, tag, ((PortData *)data)->PortInfo.s3.LinkInitReason);
}

static void PortDataXmlParserEndInitReason(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	uint8 value;
	
	if (IXmlParseUint8(state, content, len, &value))
		((PortData *)object)->PortInfo.s3.LinkInitReason = value;
}


/* bitfields needs special handling: PortPhysicalState */
static void PortDataXmlOutputPhysState(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputPortPhysStateValue(state, tag, ((PortData *)data)->PortInfo.PortStates.s.PortPhysicalState);
}

static void PortDataXmlOutputPortPhysConfig(IXmlOutputState_t *state, const char *tag, void *data)
{
	const uint8_t pt = ((PortData *)data)->PortInfo.PortPhysConfig.s.PortType;
	IXmlOutputStr(state, tag, StlPortTypeToText(pt));
}

static void PortDataXmlParserEndPhysState(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	uint8 value;
	
	if (IXmlParseUint8(state, content, len, &value))
		((PortData *)object)->PortInfo.PortStates.s.PortPhysicalState = value;
}

/* bitfields needs special handling: LinkSpeedSupported */
static void PortDataXmlOutputLinkSpeedSupported(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputLinkSpeedValue( state, tag, ((PortData*)data)->PortInfo.LinkSpeed.Supported);
}

static void PortDataXmlParserEndLinkSpeedSupported(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	uint16 value;
	
	if (IXmlParseUint16(state, content, len, &value))
		((PortData*)object)->PortInfo.LinkSpeed.Supported = value;
}

/* bitfields needs special handling: LinkSpeedActive */
static void PortDataXmlOutputLinkSpeedActive(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputLinkSpeedValue( state, tag, ((PortData*)data)->PortInfo.LinkSpeed.Active);
}

static void PortDataXmlParserEndLinkSpeedActive(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	uint16 value;
	
	if (IXmlParseUint16(state, content, len, &value))
		((PortData*)object)->PortInfo.LinkSpeed.Active = value;
}

static void PortDataXmlOutputLinkModeSupported(IXmlOutputState_t *state, const char *tag, void *data)
{
	size_t len = ((PortData*)data)->PortInfo.LinkModeSupLen;
	if (len) {
		char buffer[len*3+1];
		char *p = buffer;
		int i = 0;
		for (; i<len; i++) {
			sprintf(p, "%02x ", ((PortData*)data)->PortInfo.LinkModeSupported[i]);
			p += 3;
		}
		*p = 0;

		IXmlOutputStr(state, tag, buffer);
	}
}

static void PortDataXmlParserEndLinkModeSupported(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	int i = 0;
	char *start = content;
	char *end;
	uint32 temp;
	for (; i < len/3 && i < LINK_MODES_SIZE; i++) {
		errno = 0;
		temp = strtoul(start, &end, 16);
		if ((temp == IB_UINT32_MAX && errno) || (end == start)) {
			if (errno == ERANGE) {
				IXmlParserPrintError(state, "Value out of range");
			} else {
				IXmlParserPrintError(state, "Invalid contents");
			}
			break;
		}
		((PortData*)object)->PortInfo.LinkModeSupported[i] = (uint8)temp;
		start = end;
	}
}

/* bitfields needs special handling: MTUSupported */
static void PortDataXmlOutputMTUSupported(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputEthMtuValue(state, tag,
		((PortData*)data)->PortInfo.MTU2);
}

static void PortDataXmlParserEndMTUSupported(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	uint16 value;
	
	if (IXmlParseUint16(state, content, len, &value))
		((PortData*)object)->PortInfo.MTU2 = value;
}

/* bitfields needs special handling: RespTimeout */
static void PortDataXmlOutputRespTimeout(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputTimeoutMultValue(state, tag,
		((PortData*)data)->PortInfo.Resp.TimeValue);
}

static void PortDataXmlParserEndRespTimeout(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	uint8 value;
	
	if (IXmlParseUint8(state, content, len, &value))
		((PortData*)object)->PortInfo.Resp.TimeValue = value;
}

static void PortDataXmlOutputPortStatusData(IXmlOutputState_t *state, const char *tag, void *data)
{
	PortData *portp = (PortData*)data;

	PortStatusDataXmlOutputOptional(state, "PortStatus", portp->pPortCounters);
}

static void PortDataXmlOutputDownReason(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputDownReasonValue(state, tag, ((STL_LINKDOWN_REASON *)data)->LinkDownReason);
}
static void PortDataXmlParserEndDownReason(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	uint8 value;

	if (IXmlParseUint8(state, content, len, &value))
		((STL_LINKDOWN_REASON *)object)->LinkDownReason = value;
}
static void PortDataXmlOutputNeighborDownReason(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputDownReasonValue(state, tag, ((STL_LINKDOWN_REASON *)data)->NeighborLinkDownReason);
}
static void PortDataXmlParserEndNeighborDownReason(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	uint8 value;

	if (IXmlParseUint8(state, content, len, &value))
		((STL_LINKDOWN_REASON *)object)->NeighborLinkDownReason = value;
}

// LinkDownReasonLog
static IXML_FIELD LDRLogFields[] = {
	{ tag:"LinkDownReason", format:'k', format_func:PortDataXmlOutputDownReason, end_func:IXmlParserEndNoop },
	{ tag:"LinkDownReason_Int", format:'K', format_func:IXmlOutputNoop, end_func:PortDataXmlParserEndDownReason },
	{ tag:"NeighborLinkDownReason", format:'k', format_func:PortDataXmlOutputNeighborDownReason, end_func:IXmlParserEndNoop },
	{ tag:"NeighborLinkDownReason_Int", format:'K', format_func:IXmlOutputNoop, end_func:PortDataXmlParserEndNeighborDownReason },
	{ tag:"Timestamp", format:'U', IXML_FIELD_INFO(STL_LINKDOWN_REASON, Timestamp) },
	{ NULL }
};

static void LDRLogEntryXmlFormatAttr(IXmlOutputState_t *state, void *data)
{
	IXmlOutputPrint(state, " idx=\"%d\"", *(int *)data);
}
static void PortDataXmlOutputLDRLog(IXmlOutputState_t *state, const char *tag, void *data)
{
	PortData *portp = (PortData *)data;	// data points to PortData

	int i;
	for (i = 0; i < STL_NUM_LINKDOWN_REASONS; ++i) {
		STL_LINKDOWN_REASON *ldr = &portp->LinkDownReasons[i];

		if (ldr->Timestamp) {
			IXmlOutputStartAttrTag(state, tag, &i, LDRLogEntryXmlFormatAttr);

			IXmlOutputStrUint(state, "LinkDownReason",
				StlLinkDownReasonToText(ldr->LinkDownReason), ldr->LinkDownReason);
			IXmlOutputStrUint(state, "NeighborLinkDownReason",
				StlLinkDownReasonToText(ldr->NeighborLinkDownReason), ldr->NeighborLinkDownReason);

			IXmlOutputUint64(state, "Timestamp", ldr->Timestamp);

			IXmlOutputEndTag(state, tag);
		}
	}
}

static void *LDRLogXmlParserStart(IXmlParserState_t *state, void *parent, const char **attr)
{
	PortData *pdata = (PortData *)parent;
	int idx = -1;

	if (attr == NULL) {
		IXmlParserPrintError(state, "Failed to parse idx Attribute");
		return NULL;
	}

	int i = 0;
	while (attr[i]) {
		if (!strcmp("idx", attr[i]) && attr[i+1] != NULL) {
			if (attr[i + 1][1] == '\0') {
				idx = attr[i + 1][0] - '0';
				if (idx >= 0 && idx < STL_NUM_LINKDOWN_REASONS) {
					return &pdata->LinkDownReasons[idx];
				}
			}
		}
		i++;
	}

	IXmlParserPrintError(state, "Failed to parse idx Attribute: %d", idx);
	return NULL;
}
static void LDRLogXmlParserEnd(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	return;
}
/** =========================================================================
 * PortData definitions
 */
static IXML_FIELD PortDataFields[] = {
	{ tag:"PortNum", format:'U', IXML_FIELD_INFO(PortData, PortNum) },
	{ tag:"PortId", format:'C', IXML_FIELD_INFO(PortData, PortInfo.LocalPortId) },
	{ tag:"MgmtIfAddr", format:'H', IXML_FIELD_INFO(PortData, PortGUID) },
	{ tag:"EndMgmtIfID", format:'K', format_func:PortDataXmlOutputEndPortLID, end_func:PortDataXmlParserEndEndPortLID }, // bitfield
//	{ tag:"SubnetPrefix", format:'H', IXML_FIELD_INFO(PortData, PortInfo.SubnetPrefix) },
	{ tag:"IfID", format:'H', IXML_FIELD_INFO(PortData, PortInfo.LID) },
//#ifdef STL_CONFIG_MAXIMUMLID_ENABLE
	{ tag:"MaxIfID", format:'h', IXML_FIELD_INFO(PortData, PortInfo.MaxLID) },
//#endif /* STL_CONFIG_MAXIMUMLID_ENABLE */
	{ tag:"IPAddrIPV6", format:'k', format_func:IXmlOutputIPAddrIPV6, IXML_FIELD_INFO(PortData, PortInfo.IPAddrIPV6.addr), end_func:IXmlParserEndIPAddrIPV6},
	{ tag:"IPAddrIPV4", format:'k', format_func:IXmlOutputIPAddrIPV4, IXML_FIELD_INFO(PortData, PortInfo.IPAddrIPV4.addr), end_func:IXmlParserEndIPAddrIPV4},
	{ tag:"CapabilityMask", format:'H', IXML_FIELD_INFO(PortData, PortInfo.CapabilityMask.AsReg32) },
	{ tag:"CapabilityMask3", format:'H', IXML_FIELD_INFO(PortData, PortInfo.CapabilityMask3.AsReg16) },
	{ tag:"PortState", format:'k', format_func: PortDataXmlOutputEthPortState, end_func:IXmlParserEndNoop }, // output only bitfield
	{ tag:"PortState_Int", format:'K', format_func: IXmlOutputNoop, end_func:PortDataXmlParserEndPortState }, // input only bitfield
	{ tag:"InitReason", format:'k', format_func: PortDataXmlOutputInitReason, end_func:IXmlParserEndNoop },
	{ tag:"InitReason_Int", format:'K', format_func: IXmlOutputNoop, end_func:PortDataXmlParserEndInitReason },
	{ tag:"PhysState", format:'k', format_func: PortDataXmlOutputPhysState, end_func:IXmlParserEndNoop }, // output only bitfield
	{ tag:"PhysState_Int", format:'K', format_func: IXmlOutputNoop, end_func:PortDataXmlParserEndPhysState }, // input only bitfield
	{ tag:"PortType", format:'k', format_func:PortDataXmlOutputPortPhysConfig, end_func:IXmlParserEndNoop }, 
	{ tag:"PortType_Int", format:'H', IXML_FIELD_INFO(PortData, PortInfo.PortPhysConfig.AsReg8)},
	{ tag:"LinkSpeedSupported", format:'k', format_func: PortDataXmlOutputLinkSpeedSupported, end_func:IXmlParserEndNoop }, // output only bitfield
	{ tag:"LinkSpeedSupported_Int", format:'K', format_func: IXmlOutputNoop, end_func:PortDataXmlParserEndLinkSpeedSupported }, // input only bitfield
	{ tag:"LinkSpeedActive", format:'k', format_func: PortDataXmlOutputLinkSpeedActive, end_func:IXmlParserEndNoop }, // output only bitfield
	{ tag:"LinkSpeedActive_Int", format:'K', format_func: IXmlOutputNoop, end_func:PortDataXmlParserEndLinkSpeedActive }, // input only bitfield
	{ tag:"LinkModeSupported", format:'k', format_func: PortDataXmlOutputLinkModeSupported, end_func:PortDataXmlParserEndLinkModeSupported },
	{ tag:"LinkModeSupLen", format:'u', IXML_FIELD_INFO(PortData, PortInfo.LinkModeSupLen) },
	{ tag:"IfSpeed", format:'u', IXML_FIELD_INFO(PortData, PortInfo.IfSpeed) },
	{ tag:"MTUSupported", format:'K', format_func:PortDataXmlOutputMTUSupported, end_func:PortDataXmlParserEndMTUSupported }, // bitfield
	{ tag:"RespTimeout", format:'k', format_func: PortDataXmlOutputRespTimeout, end_func:IXmlParserEndNoop }, // output only bitfield
	{ tag:"RespTimeout_Int", format:'K', format_func: IXmlOutputNoop, end_func:PortDataXmlParserEndRespTimeout }, // input only bitfield
	{ tag:"PortStatus", format:'k', size:sizeof(STL_PORT_COUNTERS_DATA), format_func:PortDataXmlOutputPortStatusData, subfields:PortStatusDataFields, start_func:IXmlParserStartStruct, end_func:PortStatusDataXmlParserEnd }, // structure
	{ tag:"LclMgmtIfID", format:'u', IXML_FIELD_INFO(PortData, PortInfo.LocalPortNum) },
	{ tag:"PortStates", format:'h', IXML_FIELD_INFO(PortData, PortInfo.PortStates.AsReg32) },
	{ tag:"NeighborNodeIfAddr", format:'h', IXML_FIELD_INFO(PortData, PortInfo.NeighborNodeGUID) },
	// TODO cjking:  Handle Neighbor Supported MTU
	// { tag:"NeighborMTU", format:'k', format_func:PortDataXmlOutputNeighborMTU, start_func:PortDataXmlParserStartNeighborMtu, end_func:PortDataXmlParserEndNeighborMtu },
	{ tag:"NeighborPortNum", format:'u', IXML_FIELD_INFO(PortData, PortInfo.NeighborPortNum) },
	{ tag:"LinkDownReason_Int", format:'u', IXML_FIELD_INFO(PortData, PortInfo.LinkDownReason) },
	{ tag:"NeighborLinkDownReason_Int", format:'u', IXML_FIELD_INFO(PortData, PortInfo.NeighborLinkDownReason) },
	{ tag:"LinkDownReasonLog", format:'k', format_func:PortDataXmlOutputLDRLog, subfields:LDRLogFields, start_func:LDRLogXmlParserStart, end_func:LDRLogXmlParserEnd }, // structure
	{ NULL }
};

static void PortDataXmlOutput(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputStruct(state, tag, (PortData*)data, PortDataXmlFormatAttr, PortDataFields);
}

static void *PortDataXmlParserStart(IXmlParserState_t *state, void *parent, const char **attr)
{
	PortData *portp = (PortData*)MemoryAllocate2AndClear(sizeof(PortData), IBA_MEM_FLAG_PREMPTABLE, MYTAG);

	if (! portp) {
		IXmlParserPrintError(state, "Unable to allocate memory");
		return NULL;
	}

	ListItemInitState(&portp->AllPortsEntry);
	QListSetObj(&portp->AllPortsEntry, portp);
	portp->nodep = (NodeData *)parent;
	//printf("portp=%p, nodep=%p\n", portp, portp->nodep);

	return portp;
}

static int Snapshot_PortDataComplete(IXmlParserState_t * state, void * object, void * parent);
static ParseCompleteFn portDataCompleteFn = Snapshot_PortDataComplete;

void SetPortDataComplete(ParseCompleteFn fn)
{
	portDataCompleteFn = fn;
}

static ParseCompleteFn GetPortDataComplete()
{
	return portDataCompleteFn;
}

/**
	"Destructors" for some topology structs.

	They do not free the targets, only their members.
*/
void Snapshot_PortDataFree(PortData * portp, FabricData_t * fabricp);
void Snapshot_NodeDataFree(NodeData * nodep, FabricData_t * fabricp);

/**
	Completion handler for PortData inside a snapshot.
*/
static int Snapshot_PortDataComplete(IXmlParserState_t * state, void * object, void * parent)
{
	const IXML_FIELD *p;
	unsigned int i;
	PortData *portp = (PortData*)object;
	NodeData *nodep = portp->nodep;
	FabricData_t *fabricp = IXmlParserGetContext(state);

	ASSERT(nodep == (NodeData*)parent);

	// technically <Port> could preceed <NodeType> within <Node>
	if (nodep->NodeInfo.NodeType == STL_NODE_SW) {
		// a switch only gets 1 port Guid, we save it for switch
		// port 0 (the "virtual management port")
		// should not have PortGUID if PortNum != 0
		if (portp->PortGUID && portp->PortNum != 0) {
			IXmlParserPrintError(state, "Invalid fields in Port: PortGUID not allowed for Switch port!=0");
			return FERROR;
		}
	} else {
		portp->PortInfo.LocalPortNum = portp->PortNum ;
		// should have PortGUID
		if (portp->PortGUID == 0 || portp->PortNum == 0) {
			IXmlParserPrintError(state, "Invalid/missing fields in Port, PortGUID required for non-Switch");
			return FERROR;
		}
	}

	if (cl_qmap_insert(&nodep->Ports, portp->PortNum, &portp->NodePortsEntry) != &portp->NodePortsEntry)
	{
		IXmlParserPrintError(state, "Duplicate PortNum: %u", portp->PortNum);
		goto failinsert2;
	}
	if (FSUCCESS != AllLidsAdd(fabricp, portp, FALSE))
	{
		IXmlParserPrintError(state, "Duplicate LIDs found in portRecords: LID 0x%x Port %u Node: %.*s\n",
					portp->EndPortLID,
					portp->PortNum, STL_NODE_DESCRIPTION_ARRAY_SIZE,
					(char*)nodep->NodeDesc.NodeString);
		goto failinsert2;
	}

	// Handling to deal with fields not defined in STL Gen 1, but required in Gen 2. Allows forward compatability
	// of snapshots.
	for (p=state->current.subfields,i=0; p->tag != NULL; ++i,++p) {
		if (strcmp(p->tag, "PortPacketFormatsSupported") == 0) {
			if (! (state->current.fields_found & ((uint64_t)1)<<i) ) {
				portp->PortInfo.PortPacketFormats.Supported = STL_PORT_PACKET_FORMAT_9B;
				portp->PortInfo.PortPacketFormats.Enabled = STL_PORT_PACKET_FORMAT_9B;
			}
		} else if (strcmp(p->tag, "MaxLID") == 0) {
			if (! (state->current.fields_found & ((uint64_t)1)<<i) )
				if (portp->PortInfo.CapabilityMask3.s.IsMAXLIDSupported) {
					IXmlParserPrintError(state, "Missing fields in Port, MaxLID required for Gen2 HFI");
					return FERROR;
				}
		}
	}

	return FSUCCESS;

failinsert2:
	cl_qmap_remove_item(&nodep->Ports, &portp->NodePortsEntry);
	return FERROR;
}

static void PortDataXmlParserEnd(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	PortData *portp = (PortData*)object;
	FabricData_t *fabricp = IXmlParserGetContext(state);

	ParseCompleteFn parseCompleteFn = GetPortDataComplete();

	if (! valid)	// missing mandatory fields
		goto failvalidate;

	portp->rate = EthIfSpeedToStaticRate(portp->PortInfo.IfSpeed);

	if (portp->pPortCounters) {
		portp->pPortCounters->lq.s.numLanesDown = StlGetNumLanesDown(&portp->PortInfo);
	}

	if (parseCompleteFn) {
		if (parseCompleteFn(state, object, parent) != FSUCCESS) {
			goto failvalidate;
		}
	}

	QListInsertTail(&fabricp->AllPorts, &portp->AllPortsEntry);

	return;

failvalidate:
	Snapshot_PortDataFree(portp, fabricp);
	MemoryDeallocate(portp);
}

/**
	Destructor for @c PortData inside parser.

	Frees members but not port data itself.
*/
void Snapshot_PortDataFree(PortData * portp, FabricData_t * fabricp)
{
	if (portp->pPortCounters)
		MemoryDeallocate(portp->pPortCounters);
	PortDataFreeQOSData(fabricp, portp);
	PortDataFreePartitionTable(fabricp, portp);
}

/****************************************************************************/
/* NodeData Input/Output functions */

static void NodeDataXmlOutputPorts(IXmlOutputState_t *state, const char *tag, void *data)
{
	NodeData *nodep = (NodeData*)data;
	cl_map_item_t *p;

	for (p=cl_qmap_head(&nodep->Ports); p != cl_qmap_end(&nodep->Ports); p = cl_qmap_next(p)) {
		PortDataXmlOutput(state, "PortInfo", PARENT_STRUCT(p, PortData, NodePortsEntry));
	}
}

/* bitfields needs special handling: VendorID */
static void NodeDataXmlOutputVendorID(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputUint(state, tag, ((NodeData *)data)->NodeInfo.u1.s.VendorID);
}

static void NodeDataXmlParserEndVendorID(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	uint32 value;
	
	if (IXmlParseUint32(state, content, len, &value))
		((NodeData *)object)->NodeInfo.u1.s.VendorID = value;
}

const IXML_FIELD NodeDataFields[] = {
	{ tag:"NodeDesc", format:'C', IXML_FIELD_INFO(NodeData, NodeDesc.NodeString) },
	{ tag:"IfAddr", format:'H', IXML_FIELD_INFO(NodeData, NodeInfo.NodeGUID) },
	{ tag:"NodeType", format:'k', IXML_FIELD_INFO(NodeData, NodeInfo.NodeType), format_func:IXmlOutputNodeType, end_func:IXmlParserEndNoop },	// output only
	{ tag:"NodeType_Int", format:'U', IXML_FIELD_INFO(NodeData, NodeInfo.NodeType), format_func:IXmlOutputNoop },	// input only
	{ tag:"BaseVersion", format:'U', IXML_FIELD_INFO(NodeData, NodeInfo.BaseVersion) },
	{ tag:"NumPorts", format:'U', IXML_FIELD_INFO(NodeData, NodeInfo.NumPorts) },
	{ tag:"ChassisID", format:'H', IXML_FIELD_INFO(NodeData, NodeInfo.SystemImageGUID) },
	{ tag:"DeviceID", format:'H', IXML_FIELD_INFO(NodeData, NodeInfo.DeviceID) },
	{ tag:"Revision", format:'H', IXML_FIELD_INFO(NodeData, NodeInfo.Revision) },
	// NodeData.NodeInfo.u1.s.LocalPortNum is not used
	{ tag:"VendorID", format:'H', format_func:NodeDataXmlOutputVendorID, end_func:NodeDataXmlParserEndVendorID },
	{ tag:"PortInfo", format:'k', format_func:NodeDataXmlOutputPorts, subfields:PortDataFields, start_func:PortDataXmlParserStart, end_func:PortDataXmlParserEnd }, // structures
	{ NULL }
};

static void NodeDataXmlFormatAttr(IXmlOutputState_t *state, void *data)
{
	IXmlOutputPrint(state, " id=\"0x%016"PRIx64"\"", ((NodeData*)data)->NodeInfo.NodeGUID);
}

static void NodeDataXmlOutput(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputStruct(state, tag, (NodeData*)data, NodeDataXmlFormatAttr, NodeDataFields);
}

static void *NodeDataXmlParserStart(IXmlParserState_t *state, void *parent, const char **attr)
{
	NodeData *nodep = (NodeData*)MemoryAllocate2AndClear(sizeof(NodeData), IBA_MEM_FLAG_PREMPTABLE, MYTAG);

	// TBD - if enable then need quiet arg in a static global
	//if (i%PROGRESS_FREQ == 0)
		//ProgressPrint(FALSE, "Processed %6d of %6d Nodes...", i, p->NumNodeRecords);
	if (! nodep) {
		IXmlParserPrintError(state, "Unable to allocate memory");
		return NULL;
	}

	cl_qmap_init(&nodep->Ports, NULL);
	ListItemInitState(&nodep->AllTypesEntry);
	QListSetObj(&nodep->AllTypesEntry, nodep);

	return nodep;
}

static void NodeDataXmlParserEnd(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	cl_map_item_t *mi;
	NodeData *nodep = (NodeData*)object;
	FabricData_t *fabricp = IXmlParserGetContext(state);
	//FSTATUS status;

	if (! valid)	// missing mandatory fields
		goto failvalidate;

	// TODO should this enforce if NodeType == NI_TYPE_SWITCH that 
	// SwitchData has been parsed (nodep->switchp != NULL)?

	mi = cl_qmap_insert(&fabricp->AllNodes, nodep->NodeInfo.NodeGUID, &nodep->AllNodesEntry);
	if (mi != &nodep->AllNodesEntry)
	{
		IXmlParserPrintError(state, "Duplicate NodeGuid: 0x%"PRIx64"\n", nodep->NodeInfo.NodeGUID);
		goto failinsert;
	}

	//printf("processed NodeRecord GUID: 0x%"PRIx64"\n", nodep->NodeInfo.NodeGUID);
	if (FSUCCESS != AddSystemNode(fabricp, nodep)) {
		IXmlParserPrintError(state, "Unable to track systems for NodeGuid: 0x%"PRIx64"\n", nodep->NodeInfo.NodeGUID);
		goto failsystem;
	}
	return;

failsystem:
	cl_qmap_remove_item(&fabricp->AllNodes, &nodep->AllNodesEntry);
failinsert:
failvalidate:
	MemoryDeallocate(nodep);
}

static IXML_FIELD NodesFields[] = {
	{ tag:"Node", format:'K', subfields:(IXML_FIELD*)NodeDataFields, start_func:NodeDataXmlParserStart, end_func:NodeDataXmlParserEnd }, // structure
	{ NULL }
};

void Snapshot_NodeDataFree(NodeData * nodep, FabricData_t * fabricp)
{
	NodeDataFreePorts(fabricp, nodep);
	if (nodep->pSwitchInfo)
		MemoryDeallocate(nodep->pSwitchInfo);
	NodeDataFreeSwitchData(fabricp, nodep);
}

/****************************************************************************/
/* Link Input/Output functions */
static void LinkXmlOutputNodeGUID(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputHexPad64(state, tag, ((PortData*)data)->nodep->NodeInfo.NodeGUID);
}

/* Output fields for PortData to include in To and From Link */
static IXML_FIELD LinkPortFields[] = {
	{ tag:"IfAddr", format:'K', format_func:LinkXmlOutputNodeGUID },
	{ tag:"PortNum", format:'U', IXML_FIELD_INFO(PortData, PortNum) },
	{ NULL }
};

#ifndef JFORMAT
static void LinkXmlOutputFrom(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputStruct(state, tag, (PortData*)data, NULL /*PortDataXmlFormatAttr*/, LinkPortFields);
}
#endif

static void LinkXmlOutputTo(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputStruct(state, tag, ((PortData*)data)->neighbor, NULL /*PortDataXmlFormatAttr*/, LinkPortFields);
}

struct portref {
	EUI64 NodeGUID;
	uint8 PortNum;
};

typedef struct TempLinkData {
	struct portref from;
	struct portref to;
} TempLinkData_t;

/* Input fields for TempLinkData */
static IXML_FIELD TempLinkDataFromFields[] = {
	{ tag:"IfAddr", format:'H', IXML_FIELD_INFO(TempLinkData_t, from.NodeGUID)},
	{ tag:"PortNum", format:'U', IXML_FIELD_INFO(TempLinkData_t, from.PortNum) },
	{ NULL }
};
static IXML_FIELD TempLinkDataToFields[] = {
	{ tag:"IfAddr", format:'H', IXML_FIELD_INFO(TempLinkData_t, to.NodeGUID)},
	{ tag:"PortNum", format:'U', IXML_FIELD_INFO(TempLinkData_t, to.PortNum) },
	{ NULL }
};

static void *LinkFromXmlParserStart(IXmlParserState_t *state, void *parent, const char **attr)
{
	return parent;
}

static void *LinkToXmlParserStart(IXmlParserState_t *state, void *parent, const char **attr)
{
	return parent;
}


/* <Link> description */
static IXML_FIELD LinkFields[] = {
#ifdef JFORMAT
	{ tag:"From", format:'J', subfields: LinkPortFields, format_attr:PortDataXmlFormatAttr },
	{ tag:"To", format:'K', subfields: LinkPortFields, format_func:LinkXmlOutputTo },
#else
	{ tag:"From", format:'K', subfields: TempLinkDataFromFields, format_func:LinkXmlOutputFrom, start_func:LinkFromXmlParserStart, end_func:IXmlParserEndNoop }, // special handling
	{ tag:"To", format:'K', subfields: TempLinkDataToFields, format_func:LinkXmlOutputTo, start_func:LinkToXmlParserStart, end_func:IXmlParserEndNoop }, // special handling
#endif
	{ NULL }
};



static void LinkXmlOutput(IXmlOutputState_t *state, const char *tag, void *data)
{
	IXmlOutputStruct(state, tag,  (PortData*)data, PortDataXmlFormatAttr, LinkFields);
}


static void *LinkXmlParserStart(IXmlParserState_t *state, void *parent, const char **attr)
{
	/* since links don't nest, we can get away with a single static */
	/* if we ever use this multi-threaded, will need to allocate */
	static TempLinkData_t link;

	MemoryClear(&link, sizeof(link));
	return &link;
}

static void LinkXmlParserEnd(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	TempLinkData_t *link = (TempLinkData_t*)object;
	PortData *p1, *p2;
	FabricData_t *fabricp = IXmlParserGetContext(state);

	if (! valid)
		goto invalid;
	p1 = FindNodeGuidPort(fabricp, link->from.NodeGUID, link->from.PortNum);
	if (! p1) {
		IXmlParserPrintError(state, "Port not found: 0x%016"PRIx64":%u\n",
								link->from.NodeGUID, link->from.PortNum);
		goto badport;
	}
	if (p1->neighbor) {
		IXmlParserPrintError(state, "Duplicate Port found: 0x%016"PRIx64":%u\n",
								link->from.NodeGUID, link->from.PortNum);
		goto badport;
	}
	p2 = FindNodeGuidPort(fabricp, link->to.NodeGUID, link->to.PortNum);
	if (! p2) {
		IXmlParserPrintError(state, "Port not found: 0x%016"PRIx64":%u\n",
								link->to.NodeGUID, link->to.PortNum);
		goto badport;
	}
	if (p2->neighbor) {
		IXmlParserPrintError(state, "Duplicate Port found: 0x%016"PRIx64":%u\n",
								link->to.NodeGUID, link->to.PortNum);
		goto badport;
	}
	p1->neighbor = p2;
	p2->neighbor = p1;
	p1->from = 1;
	if (p1->rate != p2->rate) {
		fprintf(stderr, "%s: Warning: Ignoring Inconsistent Active Speed/Width for link between:\n", g_Top_cmdname);
		fprintf(stderr, "  %4s 0x%016"PRIx64" %3u %s %.*s\n",
				StlStaticRateToText(p1->rate),
				p1->nodep->NodeInfo.NodeGUID,
				p1->PortNum,
				StlNodeTypeToText(p1->nodep->NodeInfo.NodeType),
				STL_NODE_DESCRIPTION_ARRAY_SIZE,
				(char*)p1->nodep->NodeDesc.NodeString);
		fprintf(stderr, "  %4s 0x%016"PRIx64" %3u %s %.*s\n",
				StlStaticRateToText(p2->rate),
				p2->nodep->NodeInfo.NodeGUID,
				p2->PortNum,
				StlNodeTypeToText(p2->nodep->NodeInfo.NodeType),
				STL_NODE_DESCRIPTION_ARRAY_SIZE,
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

badport:
invalid:
	/* nothing to free */
	return;
}


static IXML_FIELD LinksFields[] = {
	{ tag:"Link", format:'K', subfields:LinkFields, start_func:LinkXmlParserStart, end_func:LinkXmlParserEnd }, // structure
	{ NULL }
};


/****************************************************************************/
/* Overall Serialization Input/Output functions */

/* only used for input parsing */
static IXML_FIELD SnapshotFields[] = {
	{ tag:"Nodes", format:'K', subfields:NodesFields }, // list
	{ tag:"Links", format:'K', subfields:LinksFields }, // list
	{ NULL }
};

static void *SnapshotXmlParserStart(IXmlParserState_t *state, void *parent, const char **attr)
{
	int i;
	boolean gottime = FALSE;
	boolean gotstats = FALSE;
	boolean gotplane = FALSE;
	FabricData_t *fabricp = IXmlParserGetContext(state);
	uint64 temp;

	// process unixtime attribute and update fabricp->time
	for (i = 0; attr[i]; i += 2) {
		if (strcmp(attr[i], "unixtime") == 0) {
			gottime = TRUE;
			// typically time_t is 32 bits, but allow 64 bits
			if (FSUCCESS != StringToUint64(&temp, attr[i+1], NULL, 0, TRUE))
				IXmlParserPrintError(state, "Invalid unixtime attribute: %s", attr[i+1]);
			else
				fabricp->time = (time_t)temp;
		} else if (strcmp(attr[i], "stats") == 0) {
			gotstats = TRUE;
			if (FSUCCESS != StringToUint64(&temp, attr[i+1], NULL, 0, TRUE))
				IXmlParserPrintError(state, "Invalid stats attribute: %s", attr[i+1]);
			else
				fabricp->flags |= temp?FF_STATS:FF_NONE;
		} else if (strcmp(attr[i], "routes") == 0) {
			if (FSUCCESS != StringToUint64(&temp, attr[i+1], NULL, 0, TRUE))
				IXmlParserPrintError(state, "Invalid routes attribute: %s", attr[i+1]);
			else
				fabricp->flags |= temp?FF_ROUTES:FF_NONE;
		} else if (strcmp(attr[i], "qosdata") == 0) {
			if (FSUCCESS != StringToUint64(&temp, attr[i+1], NULL, 0, TRUE))
				IXmlParserPrintError(state, "Invalid qosdata attribute: %s", attr[i+1]);
			else
				fabricp->flags |= temp?FF_QOSDATA:FF_NONE;
		} else if (strcmp(attr[i], "bfrctrl") == 0) {
			if (FSUCCESS != StringToUint64(&temp, attr[i+1], NULL, 0, TRUE))
				IXmlParserPrintError(state, "Invalid bfrctrl attribute: %s", attr[i+1]);
			else
				fabricp->flags |= temp?FF_BUFCTRLTABLE:FF_NONE;
		} else if (strcmp(attr[i], "downports") == 0) {
			if (FSUCCESS != StringToUint64(&temp, attr[i+1], NULL, 0, TRUE))
				IXmlParserPrintError(state, "Invalid downports attribute: %s", attr[i+1]);
			else
				fabricp->flags |= temp?FF_DOWNPORTINFO:FF_NONE;
		} else if (strcmp(attr[i], "plane") == 0) {
			gotplane  =TRUE;
			snprintf(fabricp->name, HPN_NODE_COMMUNITY_ARRAY_SIZE, "%s", attr[i+1]);
		}
	}
	if (! gottime) {
		IXmlParserPrintError(state, "Missing unixtime attribute");
	}
	if (! gotstats) {
		IXmlParserPrintError(state, "Missing stats attribute");
	}
	if (! gotplane) {
		IXmlParserPrintError(state, "Missing plane attribute");
	}
	fabricp->NumOfMcGroups = 0;
	return NULL;
}

static void SnapshotXmlParserEnd(IXmlParserState_t *state, const IXML_FIELD *field, void *object, void *parent, XML_Char *content, unsigned len, boolean valid)
{
	FabricData_t *fabricp = IXmlParserGetContext(state);

	if (! valid) {
		// This free's everything we built while parsing, leaving empty lists
		NodeDataFreeAll(fabricp);
		fabricp->LinkCount = 0;
		fabricp->ExtLinkCount = 0;
	}
}

static IXML_FIELD TopLevelFields[] = {
	{ tag:"Snapshot", format:'K', subfields:SnapshotFields, start_func:SnapshotXmlParserStart, end_func:SnapshotXmlParserEnd }, // structure
	{ NULL }
};
#if 0
static IXML_FIELD TopLevelFields[] = {
	{ tag:"Report", format:'K', subfields:ReportFields }, // structure
	{ NULL }
};
#endif

static void SnapshotInfoXmlFormatAttr(IXmlOutputState_t *state, void *data)
{
	SnapshotOutputInfo_t *info = (SnapshotOutputInfo_t *)IXmlOutputGetContext(state);
	char datestr[80] = "";
	int i;

	Top_formattime(datestr, sizeof(datestr), info->fabricp->time);
	IXmlOutputPrint( state, " plane=\"%s\" date=\"%s\" unixtime=\"%ld\" stats=\"%d\""
		" downports=\"%d\" options=\"",
		info->fabricp->name, datestr, info->fabricp->time, (info->fabricp->flags & FF_STATS) ? 1:0,
		(info->fabricp->flags & FF_DOWNPORTINFO) ? 1:0 );
	for (i=1; i<info->argc; i++)
		IXmlOutputPrint(state, "%s%s", i>1?" ":"", info->argv[i]);
	IXmlOutputPrint(state, "\"");
}

static void Xml2PrintAll(IXmlOutputState_t *state, const char *tag, void *data)
{
	SnapshotOutputInfo_t *info = (SnapshotOutputInfo_t *)IXmlOutputGetContext(state);
	FabricData_t *fabricp = info->fabricp;

	IXmlOutputStartAttrTag(state, tag, NULL, SnapshotInfoXmlFormatAttr);

	{
		cl_map_item_t *p;

		IXmlOutputStartAttrTag(state, "Nodes", NULL, NULL);
		for (p=cl_qmap_head(&fabricp->AllNodes); p != cl_qmap_end(&fabricp->AllNodes); p = cl_qmap_next(p)) {
			NodeData *nodep = PARENT_STRUCT(p, NodeData, AllNodesEntry);
#if 0
			if (! CompareNodePoint(nodep, info->focus))
				continue;
#endif
			NodeDataXmlOutput(state, "Node", nodep);
		}
		IXmlOutputEndTag(state, "Nodes");
	}

	{
		LIST_ITEM *p;

		IXmlOutputStartAttrTag(state, "Links", NULL, NULL);
		for (p=QListHead(&fabricp->AllPorts); p != NULL; p = QListNext(&fabricp->AllPorts, p)) {
			PortData *portp = (PortData *)QListObj(p);
			// to avoid duplicated processing, only process "from" ports in link
			if (! portp->from)
				continue;
			LinkXmlOutput(state, "Link", portp);
		}
		IXmlOutputEndTag(state, "Links");
	}


	IXmlOutputEndTag(state, tag);
}

void Xml2PrintSnapshot(FILE *file, SnapshotOutputInfo_t *info)
{
	IXmlOutputState_t state;

	/* using SERIALIZE with no indent makes output less pretty but 1/2 the size */
	if (FSUCCESS != IXmlOutputInit(&state, file, 0, IXML_OUTPUT_FLAG_SERIALIZE, info))
	//if (FSUCCESS != IXmlOutputInit(&state, file, 4, IXML_OUTPUT_FLAG_NONE, info))
		goto fail;
	
	//IXmlOutputStartAttrTag(&state, "Report", NULL, NULL);
	Xml2PrintAll(&state, "Snapshot", NULL);
	//IXmlOutputEndTag(&state, "Report");

	IXmlOutputDestroy(&state);

	return;

fail:
	return;
}



#ifndef __VXWORKS__
FSTATUS Xml2ParseSnapshot(const char *input_file, int quiet, FabricData_t *fabricp, FabricFlags_t flags, boolean allocFull)
{
	unsigned tags_found, fields_found;
	const char *filename=input_file;

	if (FSUCCESS != InitFabricData(fabricp, flags)) {
		fprintf(stderr, "%s: Unable to initialize fabric data memory\n", g_Top_cmdname);
		return FERROR;
	}
	if (strcmp(input_file, "-") == 0) {
		filename="stdin";
		if (! quiet) ProgressPrint(TRUE, "Parsing stdin...");
		if (FSUCCESS != IXmlParseFile(stdin, "stdin", IXML_PARSER_FLAG_NONE, TopLevelFields, NULL, fabricp, NULL, NULL, &tags_found, &fields_found)) {
			return FERROR;
		}
	} else {
		if (! quiet) ProgressPrint(TRUE, "Parsing %s...", Top_truncate_str(input_file));
		if (FSUCCESS != IXmlParseInputFile(input_file, IXML_PARSER_FLAG_NONE, TopLevelFields, NULL, fabricp, NULL, NULL, &tags_found, &fields_found)) {
			return FERROR;
		}
	}
	if (tags_found != 1 || fields_found != 1) {
		fprintf(stderr, "Warning: potentially inaccurate input '%s': found %u recognized top level tags, expected 1\n", filename, tags_found);
	}
	BuildFabricDataLists(fabricp);

	/*
		Resize the switch FDB tables to their full capacity.
	*/
	if (allocFull) {
		LIST_ITEM * n;
		for (n = QListHead(&fabricp->AllSWs); n != NULL; n = QListNext(&fabricp->AllSWs, n)) {
			NodeData * node = (NodeData*)QListObj(n);
			STL_SWITCH_INFO * swInfo = &node->pSwitchInfo->SwitchInfoData;
			
			// The snapshot may not have SwitchData in it. Tables will have to be provided by application (e.g.
			// fabric_sim).
			if (!node->switchp) continue;
			assert(NodeDataSwitchResizeFDB(node, swInfo->LinearFDBCap, swInfo->MulticastFDBCap) == FSUCCESS);
		}
	}

	return FSUCCESS;
}
#else
FSTATUS Xml2ParseSnapshot(const char *input_file, int quiet, FabricData_t *fabricp, FabricFlags_t flags, boolean allocFull, XML_Memory_Handling_Suite* memsuite)
{
	unsigned tags_found, fields_found;
	const char *filename=input_file;

	if (FSUCCESS != InitFabricData(fabricp, flags)) {
		fprintf(stderr, "%s: Unable to initialize fabric data memory\n", g_Top_cmdname);
		return FERROR;
	}
	if (strcmp(input_file, "-") == 0) {
		filename="stdin";
		if (! quiet) ProgressPrint(TRUE, "Parsing stdin...");
		if (FSUCCESS != IXmlParseFile(stdin, "stdin", IXML_PARSER_FLAG_NONE, TopLevelFields, NULL, fabricp, NULL, NULL, &tags_found, &fields_found, memsuite)) {
			return FERROR;
		}
	} else {
		if (! quiet) ProgressPrint(TRUE, "Parsing %s...", Top_truncate_str(input_file));
		if (FSUCCESS != IXmlParseInputFile(input_file, IXML_PARSER_FLAG_NONE, TopLevelFields, NULL, fabricp, NULL, NULL, &tags_found, &fields_found, memsuite)) {
			return FERROR;
		}
	}
	if (tags_found != 1 || fields_found != 1) {
		fprintf(stderr, "Warning: potentially inaccurate input '%s': found %u recognized top level tags, expected 1\n", filename, tags_found);
	}
	BuildFabricDataLists(fabricp);

	/*
		Resize the switch FDB tables to their full capacity.
	*/
	if (allocFull) {
		LIST_ITEM * n;
		for (n = QListHead(&fabricp->AllSWs); n != NULL; n = QListNext(&fabricp->AllSWs, n)) {
			NodeData * node = (NodeData*)QListObj(n);
			STL_SWITCH_INFO * swInfo = &node->pSwitchInfo->SwitchInfoData;
			
			// The snapshot may not have SwitchData in it. Tables will have to be provided by application (e.g.
			// fabric_sim).
			if (!node->switchp) continue;
			assert(NodeDataSwitchResizeFDB(node, swInfo->LinearFDBCap, swInfo->MulticastFDBCap) == FSUCCESS);
		}
	}

	return FSUCCESS;
}
#endif
