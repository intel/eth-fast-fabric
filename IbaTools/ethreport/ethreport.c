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

#include "ethreport.h"
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <arpa/inet.h>
#include <stl_helper.h>
#include <hpnmgt.h>
//#include <umad.h>
#include <time.h>
#include <string.h>
//#include "stl_print.h"

// Used for expanding various enumarations into text equivalents
#define SHOW_BUF_SIZE 81

// what to output when g_noname set
char *g_name_marker = "xxxxxxxxxx";

// amount to subtract from threshold before compare
// 0-> Greater (report error if > threshold)
// 1-> Equal (report error if >= threshold)
uint32 g_threshold_compare = 0;

/* indicates overall set of reports for Slow Link reports
 * also used to indicate which portion of the report is being done
 */
typedef enum {
	LINK_EXPECTED_REPORT = 1,
	LINK_CONFIG_REPORT =2,
	LINK_CONN_REPORT =3
} LinkReport_t;

uint8           		g_verbose       = 0;
int				g_exitstatus	= 0;
int				g_persist		= 0;	// omit transient data like LIDs
int				g_hard			= 0;	// omit software configured items
int				g_noname		= 0;	// omit names
char*			g_snapshot_in_file	= NULL;	// input file being parsed
char*			g_topology_in_file	= NULL;	// input file being parsed
int				g_limitstats	= 0;	// limit stats to specific focus ports
STL_PORT_COUNTERS_DATA g_Thresholds;
EUI64			g_portGuid		= -1;	// local port to use to access fabric
IB_PORT_ATTRIBUTES	*g_portAttrib = NULL;// attributes for our local port
int				g_quietfocus	= 0;	// do not include focus desc in report
int				g_max_lft       = 0;	// Size of largest switch LFT
int				g_quiet         = 0;	// omit progress output
int		        g_use_scsc      = 0;    // should validatecreditloops use scsc tables
int				g_ms_timeout = OMGT_DEF_TIMEOUT_MS;

// All the information about HPN
char *g_hpnConfigFile = HPN_CONFIG_FILE;
char g_fabricId[HMGT_SHORT_STRING_SIZE] = "";
mgt_conf_t g_mgt_conf_params;
char confTopFile[HMGT_CONFIG_PARAMS_FILENAME_SIZE] = "";

// All the information about the fabric
FabricData_t g_Fabric;

void XmlPrintHex64(const char *tag, uint64 value, int indent)
{
	printf("%*s<%s>0x%016"PRIx64"</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintHex32(const char *tag, uint32 value, int indent)
{
	printf("%*s<%s>0x%08x</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintHex16(const char *tag, uint16 value, int indent)
{
	printf("%*s<%s>0x%04x</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintHex8(const char *tag, uint8 value, int indent)
{
	printf("%*s<%s>0x%02x</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintDec(const char *tag, unsigned value, int indent)
{
	printf("%*s<%s>%u</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintDec64(const char *tag, uint64 value, int indent)
{
	printf("%*s<%s>%"PRIu64"</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintHex(const char *tag, unsigned value, int indent)
{
	printf("%*s<%s>0x%x</%s>\n", indent, "",tag, value, tag);
}

void XmlPrintStrLen(const char *tag, const char* value, int len, int indent)
{
	printf("%*s<%s>", indent, "",tag);
	/* print string taking care to translate special XML characters */
	for (;len && *value; --len, ++value) {
		if (*value == '&')
			printf("&amp;");
		else if (*value == '<')
			printf("&lt;");
		else if (*value == '>')
			printf("&gt;");
		else if (*value == '\'')
			printf("&apos;");
		else if (*value == '"')
			printf("&quot;");
		else if (iscntrl(*value)) {
			//table in asciitab.h indiciates character codes permitted in XML strings
			//Only 3 control characters below 0x1f are permitted:
			//0x9 (BT_S), 0xa (BT_LRF), and 0xd (BT_CR)
			if ((unsigned char)*value <= 0x08
				|| ((unsigned char)*value >= 0x0b
						 && (unsigned char)*value <= 0x0c)
				|| ((unsigned char)*value >= 0x0e
						 && (unsigned char)*value <= 0x1f)) {
				// characters which XML does not permit in character fields
				printf("!");
			} else {
				printf("&#x%x;", (unsigned)(unsigned char)*value);
			}
		} else if ((unsigned char)*value > 0x7f)
			// permitted but generate 2 characters back after parsing, so omit
			printf("!");
		else
			putchar((int)(unsigned)(unsigned char)*value);
	}
	printf("</%s>\n", tag);
}

void XmlPrintStr(const char *tag, const char* value, int indent)
{
	XmlPrintStrLen(tag, value, IB_INT32_MAX, indent);
}

void XmlPrintOptionalStr(const char *tag, const char* value, int indent)
{
	if (value)
		XmlPrintStrLen(tag, value, IB_INT32_MAX, indent);
}

void XmlPrintBool(const char *tag, unsigned value, int indent)
{
	if (value)
		XmlPrintStr(tag, "True", indent);
	else
		XmlPrintStr(tag, "False", indent);
}

void XmlPrintPortIfID(const char *tag, STL_LID value, int indent)
{
	printf("%*s<%s>0x%.*x</%s>\n", indent, "", tag, (value <= IB_MAX_UCAST_LID ? 4:8), value, tag);
}

// TODO - cjking:   Implement
//void XmlPrintIP(const char *tag, STL_LID value, int indent)
//{
//	printf("%*s<%s>0x%.*x</%s>\n", indent, "", tag, (value <= IB_MAX_UCAST_LID ? 4:8), value, tag);
//}

void XmlPrintLID(const char *tag, STL_LID value, int indent)
{
	printf("%*s<%s>0x%.*x</%s>\n", indent, "", tag, (value <= IB_MAX_UCAST_LID ? 4:8), value, tag);
}

void XmlPrintGID(const char *tag, IB_GID value, int indent)
{
	printf("%*s<%s>0x%016"PRIx64":%016"PRIx64"</%s>\n",
				indent, "", tag,
				value.Type.Global.SubnetPrefix,
				value.Type.Global.InterfaceID, tag);
}

void XmlPrintNodeType(uint8 value, int indent)
{
	XmlPrintStr("NodeType", StlNodeTypeToText(value), indent);
	XmlPrintDec("NodeType_Int", value, indent);
}

void XmlPrintNodeDesc(const char *value, int indent)
{
	if (! g_noname)
		XmlPrintStrLen("NodeDesc", value, NODE_DESCRIPTION_ARRAY_SIZE, indent);
}

void XmlPrintRate(uint8 value, int indent)
{
	XmlPrintStr("Rate", EthStaticRateToText(value), indent);
	XmlPrintDec("Rate_Int", value, indent);
}

void XmlPrintLinkWidth(const char* tag_prefix, uint8 value, int indent)
{
	char buf[64];
	XmlPrintStr(tag_prefix, StlLinkWidthToText(value, buf, sizeof(buf)), indent);
	printf("%*s<%s_Int>%u</%s_Int>\n", indent, "",tag_prefix, value, tag_prefix);
}

void XmlPrintLinkSpeed(const char* tag_prefix, uint16 value, int indent)
{
	char buf[64];
	XmlPrintStr(tag_prefix, EthLinkSpeedToText(value, buf, sizeof(buf)), indent);
	printf("%*s<%s_Int>%u</%s_Int>\n", indent, "",tag_prefix, value, tag_prefix);
}

void XmlPrintPortLtpCrc(const char* tag_prefix, uint16 value, int indent)
{
	char buf[64];
	XmlPrintStr(tag_prefix, StlPortLtpCrcModeToText(value, buf, sizeof(buf)), indent);
	printf("%*s<%s_Int>%u</%s_Int>\n", indent, "", tag_prefix, value, tag_prefix);
}

// for predictable output order, should be called with the from port of the
// link record, with the exception of trace routes
void XmlPrintLinkStartTag(const char* tag, PortData *portp, int indent)
{
	//if (! portp->from)
	//	portp = portp->neighbor;

	printf("%*s<%s id=\"0x%016"PRIx64":%u\">\n", indent, "", tag,
				portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
}

void DisplaySeparator(void)
{
	printf("-------------------------------------------------------------------------------\n");

}

// header used before a series of links
void ShowLinkBriefSummaryHeader(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sRate IfAddr             Port PortId           Type Name\n", indent, "");
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

// show 1 port in a link in brief 1 line form
void ShowLinkPortBriefSummary(PortData *portp, const char *prefix,
			uint64 context, LinkPortSummaryDetailCallback_t *callback,
			Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%4s ", indent, "", prefix);
		
		printf("0x%016"PRIx64" %3u  %-*s %2s   %.*s\n",
			portp->nodep->NodeInfo.NodeGUID,
			portp->PortNum, TINY_STR_ARRAY_SIZE, portp->PortInfo.LocalPortId,
			StlNodeTypeToText(portp->nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)portp->nodep->NodeDesc.NodeString);
		if (portp->nodep->enodep && portp->nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", portp->nodep->enodep->details);
		}
		if (detail) {
			PortSelector* portselp = GetPortSelector(portp);
			if (portselp && portselp->details) {
				printf("%*sPortDetails: %s\n", indent+4, "", portselp->details);
			}
			
		}
		if (portp->pPortCounters && detail > 3 && ! g_persist && ! g_hard)
			ShowPortCounters(portp->pPortCounters, format, indent+4, detail-3);
		break;
	case FORMAT_XML:
		// MTU is output as part of LinkFrom directly in <Link> tag
		printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent, "",
				portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
		XmlPrintHex64("IfAddr",
				portp->nodep->NodeInfo.NodeGUID, indent+4);
		if (portp->PortGUID)
			XmlPrintHex64("MgmtIfAddr", portp->PortGUID, indent+4);
		XmlPrintDec("PortNum", portp->PortNum, indent+4);
		XmlPrintStr("PortId", (const char*) portp->PortInfo.LocalPortId, indent+4);
		XmlPrintNodeType(portp->nodep->NodeInfo.NodeType,
						indent+4);
		XmlPrintNodeDesc((char*)portp->nodep->NodeDesc.NodeString, indent+4);
		if (portp->nodep->enodep && portp->nodep->enodep->details) {
			XmlPrintOptionalStr("NodeDetails", portp->nodep->enodep->details, indent+4);
		}
		if (detail) {
			PortSelector* portselp = GetPortSelector(portp);
			if (portselp && portselp->details) {
				XmlPrintOptionalStr("PortDetails", portselp->details, indent+4);
			}
			
		}
		if (portp->pPortCounters && detail > 3 && ! g_persist && ! g_hard)
			ShowPortCounters(portp->pPortCounters, format, indent+4, detail-3);

		break;
	default:
		break;
	}
	if (callback && detail)
		(*callback)(context, portp, format, indent+4, detail-1);
	if (format == FORMAT_XML)
		printf("%*s</Port>\n", indent, "");
}

// show 1 port in a link in multi-line form with heading per field
void ShowLinkPortSummary(PortData *portp, const char *prefix,
			Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%4s Name: %.*s\n",
			indent, "", prefix,
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)portp->nodep->NodeDesc.NodeString);
		printf("%*sIfAddr: 0x%016"PRIx64" Type: %s PortNum: %3u PortId: %.*s\n",
			indent+4, "",
			portp->nodep->NodeInfo.NodeGUID,
			StlNodeTypeToText(portp->nodep->NodeInfo.NodeType),
			portp->PortNum, TINY_STR_ARRAY_SIZE, portp->PortInfo.LocalPortId);
		if (portp->nodep->enodep && portp->nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", portp->nodep->enodep->details);
		}
		if (detail) {
			PortSelector* portselp = GetPortSelector(portp);
			if (portselp && portselp->details) {
				printf("%*sPortDetails: %s\n", indent+4, "", portselp->details);
			}
		}
		break;
	case FORMAT_XML:
		ShowLinkPortBriefSummary(portp, prefix, 
			0, NULL, format, indent, detail);
		break;
	default:
		break;
	}
}

// show cable information for a link in brief summary format
void ShowExpectedLinkBriefSummary(ExpectedLink *elinkp,
			Format_t format, int indent, int detail)
{
	if (! elinkp)
		return;
	switch (format) {
	case FORMAT_TEXT:
		if (elinkp->details) {
			printf("%*sLinkDetails: %s\n", indent, "", elinkp->details);
		}
		break;
	case FORMAT_XML:
		indent-= 4;	// hack to fix indent level
		if (elinkp->details) {
			XmlPrintOptionalStr("LinkDetails", elinkp->details, indent);
		}
		break;
	default:
		break;
	}
}

// show cable information for a link in multi-line format with field headings
void ShowExpectedLinkSummary(ExpectedLink *elinkp,
			Format_t format, int indent, int detail)
{
	if (! elinkp)
		return;
	ASSERT(elinkp->portp1 && elinkp->portp1->neighbor == elinkp->portp2);
	switch (format) {
	case FORMAT_TEXT:
		if (elinkp->details) {
			printf("%*sLinkDetails: %s\n", indent, "", elinkp->details);
		}
		break;
	case FORMAT_XML:
		ShowExpectedLinkBriefSummary(elinkp, format, indent, detail);
		break;
	default:
		break;
	}
}

// show from side of a link, need to later call ShowLinkToBriefSummary
// useful when traversing trace route and don't have both sides of link handy
void ShowLinkFromBriefSummary(PortData *portp1,
			uint64 context, LinkPortSummaryDetailCallback_t *callback,
			Format_t format, int indent, int detail)
{
	if (format == FORMAT_XML) {
		XmlPrintLinkStartTag("Link", portp1, indent);
		indent+=4;
		XmlPrintRate(portp1->rate, indent);
		XmlPrintDec("Internal", isInternalLink(portp1)?1:0, indent);
		if (detail)
			ShowExpectedLinkBriefSummary(portp1->elinkp, format, indent+4, detail-1);
	}
	ShowLinkPortBriefSummary(portp1, EthStaticRateToText(portp1->rate), 
						context, callback, format, indent, detail);
}

// show to side of a link, need to call ShowLinkFromBriefSummary before this
// useful when traversing trace route and don't have both sides of link handy
// portp2 can be NULL to "close" the From Summary without additional
// port information and no cable information
// This is useful when reporting trace routes which stay within a single port
void ShowLinkToBriefSummary(PortData *portp2, const char* toprefix, boolean close_link,
			uint64 context, LinkPortSummaryDetailCallback_t *callback,
			Format_t format, int indent, int detail)
{
	if (format == FORMAT_XML)
		indent +=4;
	// portp2 should not be NULL, but code this defensively
	if (portp2) {
		ShowLinkPortBriefSummary(portp2, toprefix, 
							context, callback, format, indent, detail);
		DEBUG_ASSERT(portp2->elinkp == portp2->neighbor->elinkp);
		if (detail && format != FORMAT_XML)
			ShowExpectedLinkBriefSummary(portp2->elinkp, format, indent+4, detail-1);
	}
	if (format == FORMAT_XML && close_link)
		printf("%*s</Link>\n", indent-4, "");
}

// show both sides of a link, portp1 should be the "from" port
void ShowLinkBriefSummary(PortData *portp1, const char* toprefix, Format_t format, int indent, int detail)
{
	ShowLinkFromBriefSummary(portp1, 0, NULL, format, indent, detail);
	ShowLinkToBriefSummary(portp1->neighbor, toprefix, TRUE, 0, NULL, format, indent, detail);
}

void ShowPointNodeBriefSummary(const char* prefix, NodeData *nodep, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%s0x%016"PRIx64" %s %.*s\n",
			indent, "", prefix,
			nodep->NodeInfo.NodeGUID,
			StlNodeTypeToText(nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
				g_noname?g_name_marker:(char*)nodep->NodeDesc.NodeString);
		if (nodep->enodep && nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", nodep->enodep->details);
		}
		break;
	case FORMAT_XML:
		printf("%*s<Node id=\"0x%016"PRIx64"\">\n", indent, "",
					nodep->NodeInfo.NodeGUID);
		XmlPrintHex64("IfAddr", nodep->NodeInfo.NodeGUID,
						indent+4);
		XmlPrintNodeType(nodep->NodeInfo.NodeType, indent+4);
		XmlPrintNodeDesc(
				(char*)nodep->NodeDesc.NodeString, indent+4);
		if (nodep->enodep && nodep->enodep->details) {
			XmlPrintOptionalStr("NodeDetails", nodep->enodep->details, indent+4);
		}
		printf("%*s</Node>\n", indent, "");
		break;
	default:
		break;
	}
}

void ShowPointPortBriefSummary(const char* prefix, PortData *portp, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		if (portp->PortGUID)
			printf("%*s%s%3u 0x%016"PRIx64"\n",
				indent, "", prefix,
				portp->PortNum,
				portp->PortGUID);
		else
			printf("%*s%s%3u\n",
				indent, "", prefix,
				portp->PortNum);
		ShowPointNodeBriefSummary("in Node: ", portp->nodep, format,
								indent+4, detail);
		break;
	case FORMAT_XML:
		printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent, "",
				portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
		XmlPrintDec("PortNum", portp->PortNum, indent+4);
		if (portp->PortGUID)
			XmlPrintHex64("MgmtIfAddr", portp->PortGUID, indent+4);
		ShowPointNodeBriefSummary("in Node: ", portp->nodep, format,
								indent+4, detail);
		printf("%*s</Port>\n", indent, "");
		break;
	default:
		break;
	}
}

// show 1 port selector in link data in brief form
// designed to be called for side 1 then side 2
void ShowExpectedLinkPortSelBriefSummary(const char* prefix,
			ExpectedLink *elinkp, PortSelector *portselp,
			uint8 side, ExpectedLinkSummaryDetailCallback_t *callback,
			Format_t format, int indent, int detail)
{
	DEBUG_ASSERT(side == 1 || side == 2);
	switch (format) {
	case FORMAT_TEXT:
		{
		int prefix_len = strlen(prefix);
		if (side == 1) {
			printf("%*s%s%4s ", indent, "", prefix,
					elinkp->expected_rate?
						EthStaticRateToText(elinkp->expected_rate)
						:"    ");
		} else {
			printf("%*s%*s%4s ", indent, "", prefix_len, "",
					"<-> ");
		}
		if (side == 1 && elinkp->expected_mtu)
			printf("%5s ",
				IbMTUToText(elinkp->expected_mtu));
		else
			printf("      ");
		if (portselp) {
			if (portselp->NodeGUID)
				printf("0x%016"PRIx64, portselp->NodeGUID);
			else
				printf("                  ");
			if (portselp->gotPortNum)
				printf(" %3u            ",portselp->PortNum);
			else if (portselp->PortGUID)
				printf(" 0x%016"PRIx64, portselp->PortGUID);
			else
				printf("                ");
			PortData *portp = side == 1 ? elinkp->portp1 : elinkp->portp2;
			if (portp)
				printf(" %-*s", TINY_STR_ARRAY_SIZE, portp->PortInfo.LocalPortId);
			else
				printf("                 ");
			if (portselp->NodeType)
				printf(" %s",
					StlNodeTypeToText(portselp->NodeType));
			else
				printf("   ");
			if (portselp->NodeDesc)
				printf("   %.*s\n",
					NODE_DESCRIPTION_ARRAY_SIZE, g_noname?g_name_marker:portselp->NodeDesc);
			else
				printf("\n");
			if (detail) {
				if (portselp->details) {
					// TBD should g_noname suppress some of this?
					printf("%*sPortDetails: %s\n", indent+4+prefix_len, "", portselp->details);
				}
			}
		}
		}
		break;
	case FORMAT_XML:
		// MTU is output as part of LinkFrom directly in <Link> tag
		if (portselp) {
			printf("%*s<Port id=\"%u\">\n", indent, "", side);
			if (portselp->NodeGUID)
				XmlPrintHex64("IfAddr", portselp->NodeGUID, indent+4);
			if (portselp->gotPortNum)
				XmlPrintDec("PortNum", portselp->PortNum, indent+4);
			PortData *portp = side == 1 ? elinkp->portp1 : elinkp->portp2;
			if (portp)
				XmlPrintStr("PortId", (const char*) portp->PortInfo.LocalPortId, indent+4);
			if (portselp->PortGUID)
				XmlPrintHex64("MgmtIfAddr", portselp->PortGUID, indent+4);
			if (portselp->NodeType)
				XmlPrintNodeType(portselp->NodeType, indent+4);
			if (portselp->NodeDesc)
				XmlPrintNodeDesc(portselp->NodeDesc, indent+4);
			if (detail) {
				if (portselp->details) {
					// TBD should g_noname suppress some of this?
					XmlPrintOptionalStr("PortDetails", portselp->details, indent+4);
				}
			}
		}
		break;
	default:
		break;
	}
	if (callback && detail)
		(*callback)(elinkp, side, format, indent+4, detail-1);
	if (format == FORMAT_XML)
		printf("%*s</Port>\n", indent, "");
}

void ShowPointExpectedLinkBriefSummary(const char* prefix, ExpectedLink *elinkp, Format_t format, int indent, int detail)
{
	// top level information about link
	if (format == FORMAT_XML) {
		printf("%*s<InputLink id=\"0x%016"PRIx64"\">\n", indent, "", (uint64)(uintn)elinkp);
		indent+=4;
		if (elinkp->expected_rate)
	 		XmlPrintRate(elinkp->expected_rate, indent);
		if (elinkp->expected_mtu)
			XmlPrintDec("MTU",
				GetBytesFromMtu(elinkp->expected_mtu), indent);
		XmlPrintDec("Internal", elinkp->internal, indent);
		if (detail)
			ShowExpectedLinkBriefSummary(elinkp, format, indent+4, detail-1);
	}

	// From Side (Port 1)
	ShowExpectedLinkPortSelBriefSummary(prefix, elinkp, elinkp->portselp1,
					1, NULL, format, indent, detail);

	// To Side (Port 2)
	ShowExpectedLinkPortSelBriefSummary(prefix, elinkp, elinkp->portselp2,
					2, NULL, format, indent, detail);

	// Summary information about Link itself
	if (detail && format != FORMAT_XML)
		ShowExpectedLinkBriefSummary(elinkp, format, indent+4, detail-1);
	if (format == FORMAT_XML)
		printf("%*s</InputLink>\n", indent-4, "");
}

void ShowPointBriefSummary(Point* point, uint8 find_flag, Format_t format, int indent, int detail)
{
	ASSERT(PointValid(point));
	if (find_flag & FIND_FLAG_FABRIC) {
		switch (point->Type) {
		case POINT_TYPE_NONE:
			break;
		case POINT_TYPE_PORT:
			ShowPointPortBriefSummary("Port: ", point->u.portp, format, indent, detail);
			break;
		case POINT_TYPE_PORT_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pList = &point->u.portList;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%u Ports:\n",
						indent, "",
						ListCount(pList));
				break;
			case FORMAT_XML:
				printf("%*s<Ports>\n", indent, "");
				XmlPrintDec("Count", ListCount(pList), indent+4);
				break;
			default:
				break;
			}
			for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
				PortData *portp = (PortData*)ListObj(i);
				ShowPointPortBriefSummary("", portp, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</Ports>\n",
						indent, "");
			}
			break;
			}
		case POINT_TYPE_NODE:
			ShowPointNodeBriefSummary("Node: ", point->u.nodep, format, indent, detail);
			break;
		case POINT_TYPE_NODE_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pList = &point->u.nodeList;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%u Nodes:\n",
						indent, "",
						ListCount(pList));
				break;
			case FORMAT_XML:
				printf("%*s<Nodes>\n", indent, "");
				XmlPrintDec("Count", ListCount(pList), indent+4);
				break;
			default:
				break;
			}
			for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
				NodeData *nodep = (NodeData*)ListObj(i);
				ShowPointNodeBriefSummary("", nodep, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</Nodes>\n", indent, "");
			}
			break;
			}
		case POINT_TYPE_SYSTEM:
			{
			SystemData *systemp = point->u.systemp;
			cl_map_item_t *p;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*sSystem: 0x%016"PRIx64"\n",
					indent, "",
					systemp->SystemImageGUID);
				break;
			case FORMAT_XML:
				printf("%*s<System id=\"0x%016"PRIx64"\">\n", indent, "",
						systemp->SystemImageGUID?systemp->SystemImageGUID:
						PARENT_STRUCT(cl_qmap_head(&systemp->Nodes), NodeData, SystemNodesEntry)->NodeInfo.NodeGUID);
				XmlPrintHex64("ChassisID", systemp->SystemImageGUID, indent+4);
				break;
			default:
				break;
			}
			for (p=cl_qmap_head(&systemp->Nodes); p != cl_qmap_end(&systemp->Nodes); p = cl_qmap_next(p)) {
				NodeData *nodep = PARENT_STRUCT(p, NodeData, SystemNodesEntry);
				ShowPointNodeBriefSummary("Node: ", nodep, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</System>\n", indent, "");
			}
			break;
			}
		case POINT_TYPE_NODE_PAIR_LIST:
			{
			LIST_ITERATOR i, j;
			int noOfLeftNodes, noOfRightNodes;
			DLIST *pList1 = &point->u.nodePairList.nodePairList1;
			DLIST *pList2 = &point->u.nodePairList.nodePairList2;

			noOfLeftNodes = ListCount(pList1);
			noOfRightNodes = ListCount(pList2);

			if (noOfLeftNodes != noOfRightNodes) {
				fprintf(stderr, "Pairs are not complete \n");
				break;
			}

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%u Node Pairs:\n",
						indent, "",
						ListCount(pList1));
				break;
			case FORMAT_XML:
				printf("%*s<NodePairs>\n", indent, "");
				XmlPrintDec("Count", ListCount(pList1), indent+4);
				break;
			default:
				break;
			}
			for (i = ListHead(pList1), j = ListHead(pList2); (i != NULL && j != NULL);
				i = ListNext(pList1, i), j = ListNext(pList2, j) ) {
				NodeData *nodep1 = (NodeData*)ListObj(i);
				NodeData *nodep2 = (NodeData*)ListObj(j);
				ShowPointNodeBriefSummary("NodePair: ", nodep1, format, indent+4, detail);
				ShowPointNodeBriefSummary("          ", nodep2, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</NodePairs>\n", indent, "");
			}
			break;
			}
		default:
			break;
		}
	}
	if (find_flag & FIND_FLAG_ENODE) {
		switch (point->EnodeType) {
		case POINT_ENODE_TYPE_NONE:
			break;
		case POINT_ENODE_TYPE_NODE:
			ShowExpectedNodeBriefSummary("Input Node: ", point->u2.enodep, "InputNode", TRUE, format, indent, detail);
			break;
		case POINT_ENODE_TYPE_NODE_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pList = &point->u2.enodeList;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%u Input Nodes:\n",
						indent, "",
						ListCount(pList));
				break;
			case FORMAT_XML:
				printf("%*s<InputNodes>\n", indent, "");
				XmlPrintDec("Count", ListCount(pList), indent+4);
				break;
			default:
				break;
			}
			for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
				ExpectedNode *enodep = (ExpectedNode*)ListObj(i);
				ShowExpectedNodeBriefSummary("", enodep, "InputNode", TRUE, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</InputNodes>\n", indent, "");
			}
			break;
			}
		}
	}
	if (find_flag & FIND_FLAG_ELINK) {
		switch (point->ElinkType) {
		case POINT_ELINK_TYPE_NONE:
			break;
		case POINT_ELINK_TYPE_LINK:
			ShowPointExpectedLinkBriefSummary("Input Link: ", point->u4.elinkp, format, indent, detail);
			break;
		case POINT_ELINK_TYPE_LINK_LIST:
			{
			LIST_ITERATOR i;
			DLIST *pList = &point->u4.elinkList;

			switch (format) {
			case FORMAT_TEXT:
				printf("%*s%u Input Links:\n",
						indent, "",
						ListCount(pList));
				break;
			case FORMAT_XML:
				printf("%*s<InputLinks>\n", indent, "");
				XmlPrintDec("Count", ListCount(pList), indent+4);
				break;
			default:
				break;
			}
			for (i=ListHead(pList); i != NULL; i = ListNext(pList, i)) {
				ExpectedLink *elinkp = (ExpectedLink*)ListObj(i);
				ShowPointExpectedLinkBriefSummary("", elinkp, format, indent+4, detail);
			}
			if (format == FORMAT_XML) {
				printf("%*s</InputLinks>\n", indent, "");
			}
			break;
			}
		}
	}
}

void ShowPointFocus(Point* focus, uint8 find_flag, Format_t format, int indent, int detail)
{
	if (! focus || g_quietfocus)
		return;
	if (PointValid(focus)) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sFocused on:\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<Focus>\n", indent, "");
			break;
		default:
			break;
		}
		ShowPointBriefSummary(focus, find_flag, format, indent+4, detail);
		if (format == FORMAT_XML)
			printf("%*s</Focus>\n", indent, "");
	}
	if (format == FORMAT_TEXT)
		printf("\n");
}

// output verbose summary of PortCounters
void ShowPortCounters(STL_PORT_COUNTERS_DATA *pPortCounters, Format_t format, int indent, int detail)
{
	if (detail < 1) return;

	switch (format) {
	case FORMAT_TEXT:
		if (detail >= 3) {
			/*
			 * Performance: Transmit
			 */
			printf("%*sPerformance: Transmit\n", 
				indent, "");
			printf("%*s    If HC Out Octets                              %20"PRIu64" MB (%"PRIu64" Octets)\n",
				indent, "",
				pPortCounters->portXmitData/1000000,
				pPortCounters->portXmitData);
			printf("%*s    If HC Out Ucast Pkts                          %20"PRIu64"\n",
				indent, "",
				pPortCounters->portXmitPkts);
			printf("%*s    If HC Out Multicast Pkts                      %20"PRIu64"\n",
				indent, "",
				pPortCounters->portMulticastXmitPkts);

			/*
			 * Performance: Receive
			 */
			printf("%*sPerformance: Receive\n",
				indent, "");
			printf("%*s    If HC In Octets                               %20"PRIu64" MB (%"PRIu64" Octets)\n",
				indent, "",
				pPortCounters->portRcvData/1000000,
				pPortCounters->portRcvData);
			printf("%*s    If HC In Ucast Pkts                           %20"PRIu64"\n",
				indent, "",
				pPortCounters->portRcvPkts);
			printf("%*s    If HC In Multicast Pkts                       %20"PRIu64"\n",
				indent, "",
				pPortCounters->portMulticastRcvPkts);

			/*
			 * Errors: Packet Discards
			 */
			printf("%*sError: Packet Discards\n",
				indent, "");
			printf("%*s    If Out Discards                               %20"PRIu64"\n",
				indent, "",
				pPortCounters->portXmitDiscards);
			printf("%*s    If In Discards                                %20"PRIu64"\n",
				indent, "",
				pPortCounters->portRcvFECN);

			/*
			 * Errors: Signal Integrity
			 */
			printf("%*sErrors: Signal Integrity\n",
				indent, "");
			printf("%*s    Dot3 HC Stats Internal Mac Transmit Errors    %20"PRIu64"\n",
				indent, "",
				pPortCounters->dot3HCStatsInternalMacTransmitErrors);
			printf("%*s    Dot3 HC Stats Internal Mac Receive Errors     %20"PRIu64"\n",
				indent, "",
				pPortCounters->portRcvErrors);
			printf("%*s    Dot3 HC Stats Symbol Errors                   %20"PRIu64"\n",
				indent, "",
				pPortCounters->localLinkIntegrityErrors);

			/*
			 * Errors: Packet Integrity
			 */
			printf("%*sErrors: Packet Integrity\n",
				indent, "");
			printf("%*s    If Out Errors                                 %20u\n",
				indent, "",
				pPortCounters->ifOutErrors);
			printf("%*s    If In Errors                                  %20u\n",
				indent, "",
				pPortCounters->ifInErrors);
			printf("%*s    If In Unknown Protos                          %20u\n",
				indent, "",
				pPortCounters->ifInUnknownProtos);
			printf("%*s    Dot3 HC Stats Alignment Errors                %20"PRIu64"\n",
				indent, "",
				pPortCounters->dot3HCStatsAlignmentErrors);
			printf("%*s    Dot3 HC Stats FCS Errors                      %20"PRIu64"\n",
				indent, "",
				pPortCounters->dot3HCStatsFCSErrors);
			printf("%*s    Dot3 Stats Frame Too Longs                    %20"PRIu64"\n",
				indent, "",
				pPortCounters->excessiveBufferOverruns);

			/*
			 * Errors: Half-Duplex Detection
			 */
			printf("%*sErrors: Half-Duplex Detection\n",
				indent, "");
			printf("%*s    Dot3 Stats Carrier Sense Errors               %20u\n",
				indent, "",
				pPortCounters->dot3StatsCarrierSenseErrors);
			printf("%*s    Dot3 Stats Single Collision Frames            %20u\n",
				indent, "",
				pPortCounters->dot3StatsSingleCollisionFrames);
			printf("%*s    Dot3 Stats Multiple Collision Frames          %20u\n",
				indent, "",
				pPortCounters->dot3StatsMultipleCollisionFrames);
			printf("%*s    Dot3 Stats SQE Test Errors                    %20u\n",
				indent, "",
				pPortCounters->dot3StatsSQETestErrors);
			printf("%*s    Dot3 Stats Deferred Transmissions             %20u\n",
				indent, "",
				pPortCounters->dot3StatsDeferredTransmissions);
			printf("%*s    Dot3 Stats Late Collisions                    %20u\n",
				indent, "",
				pPortCounters->dot3StatsLateCollisions);
			printf("%*s    Dot3 Stats Excessive Collisions               %20u\n",
				indent, "",
				pPortCounters->dot3StatsExcessiveCollisions);
		} else if (detail == 2) {
			printf("%*sPortStatus:\n", indent, "");
			printf("%*sIf HC Out Octets          %20"PRIu64" MB | If HC Out Ucast Pkts             %20"PRIu64"\n", indent+4, "",
				 pPortCounters->portXmitData/FLITS_PER_MB, pPortCounters->portXmitPkts);
			printf("%*sIf HC In Octets           %20"PRIu64" MB | If HC In Ucast Pkts              %20"PRIu64"\n", indent+4, "",
				pPortCounters->portRcvData/FLITS_PER_MB, pPortCounters->portRcvPkts);
			if (pPortCounters->portMulticastXmitPkts || pPortCounters->portMulticastRcvPkts) {
			printf("%*sIf HC Out Multicast Pkts          %20"PRIu64" | If HC In Multicast Pkts                  %20"PRIu64"\n", indent+4, "",
				pPortCounters->portMulticastXmitPkts, pPortCounters->portMulticastRcvPkts);
			}
			boolean isLeft = TRUE, isAny = FALSE;
#define NON_ZERO_64(cntr, name) \
			if (cntr) { \
				printf("%*s%-22s%20"PRIu64"%s", isLeft ? indent+4 : 0, "", name, cntr, isLeft ? " | " : "\n"); \
				isLeft = !isLeft; isAny = TRUE;\
			}
#define NON_ZERO(cntr, name) \
			if (cntr) { \
				printf("%*s%-22s%20u%s", isLeft ? indent+4 : 0, "", name, cntr, isLeft ? " | " : "\n"); \
				isLeft = !isLeft; isAny = TRUE; \
			}
			NON_ZERO_64(pPortCounters->swPortCongestion,            "Congestion Discards");
			NON_ZERO_64(pPortCounters->portRcvFECN,                 "If In Discards");
			NON_ZERO_64(pPortCounters->portRcvBECN,                 "Rcv BECN");
			NON_ZERO_64(pPortCounters->portMarkFECN,                "Mark FECN");
			NON_ZERO_64(pPortCounters->portXmitTimeCong,            "Xmit Time Congestion");
			NON_ZERO_64(pPortCounters->portXmitWait,                "Xmit Wait");
			NON_ZERO_64(pPortCounters->portXmitWastedBW,            "Xmit Wasted BW");
			NON_ZERO_64(pPortCounters->portXmitWaitData,            "Xmit Wait Data");
			NON_ZERO_64(pPortCounters->portRcvBubble,               "Rcv Bubble");
			NON_ZERO(pPortCounters->uncorrectableErrors,            "Uncorrectable Errors");
			NON_ZERO(pPortCounters->linkDowned,                     "Link Downed");
			NON_ZERO_64(pPortCounters->portRcvErrors,               "Dot3 HC Stats Internal Mac Receive Errors");
			NON_ZERO_64(pPortCounters->excessiveBufferOverruns,     "Dot3 Stats Frame Too Longs");
			NON_ZERO_64(pPortCounters->fmConfigErrors,              "FM Config Errors");
			NON_ZERO(pPortCounters->linkErrorRecovery,              "Link Error Recovery");
			NON_ZERO_64(pPortCounters->localLinkIntegrityErrors,    "Dot3 HC Stats Symbol Errors");
			NON_ZERO_64(pPortCounters->portRcvRemotePhysicalErrors, "Rcv Rmt Phys Err");
			NON_ZERO_64(pPortCounters->portXmitConstraintErrors,    "Xmit Constraint");
			NON_ZERO_64(pPortCounters->portRcvConstraintErrors,     "Rcv Constraint");
			NON_ZERO_64(pPortCounters->portRcvSwitchRelayErrors,    "Rcv Sw Relay Err");
			NON_ZERO_64(pPortCounters->portXmitDiscards,            "If Out Discards");
	
			NON_ZERO_64(pPortCounters->dot3HCStatsInternalMacTransmitErrors,            "Dot3 HC Stats Internal Mac Transmit Errors");
			NON_ZERO_64(pPortCounters->dot3HCStatsAlignmentErrors,            "Dot3 HC Stats Alignment Errors");
			NON_ZERO(pPortCounters->ifOutErrors,                     "If Out Errors");
			NON_ZERO(pPortCounters->ifInErrors,                     "If In Errors");
			NON_ZERO(pPortCounters->ifInUnknownProtos,                     "If In Unknown Protos");
			NON_ZERO_64(pPortCounters->dot3HCStatsFCSErrors,            "Dot3 HC Stats FCS Errors");
			NON_ZERO(pPortCounters->dot3StatsCarrierSenseErrors,                     "dot3StatsCarrierSenseErrors");
			NON_ZERO(pPortCounters->dot3StatsSingleCollisionFrames,                     "dot3StatsSingleCollisionFrames");
			NON_ZERO(pPortCounters->dot3StatsMultipleCollisionFrames,                     "dot3StatsMultipleCollisionFrames");
			NON_ZERO(pPortCounters->dot3StatsSQETestErrors,                     "dot3StatsSQETestErrors");
			NON_ZERO(pPortCounters->dot3StatsDeferredTransmissions,                     "dot3StatsDeferredTransmissions");
			NON_ZERO(pPortCounters->dot3StatsLateCollisions,                     "dot3StatsLateCollisions");
			NON_ZERO(pPortCounters->dot3StatsExcessiveCollisions,                     "dot3StatsExcessiveCollisions");
			if (isAny && !isLeft) printf("\n");
#undef NON_ZERO_64
#undef NON_ZERO
		} else if (detail == 1) {
			printf("%*sPortStatus:\n", indent, "");
			printf("%*sIf HC Out Octets          %20"PRIu64" MB | If HC Out Ucast Pkts             %20"PRIu64"\n", indent+4, "",
				 pPortCounters->portXmitData/FLITS_PER_MB, pPortCounters->portXmitPkts);
			printf("%*sIf HC In Octets           %20"PRIu64" MB | If HC In Ucast Pkts              %20"PRIu64"\n", indent+4, "",
				pPortCounters->portRcvData/FLITS_PER_MB, pPortCounters->portRcvPkts);
		}
		break; 
	case FORMAT_XML:
		printf("%*s<Performance>\n", indent, "");
		// Data movement
		XmlPrintDec64("IfHCOutOctetsMB", pPortCounters->portXmitData/FLITS_PER_MB, indent+4);
		printf("%*s<IfHCOutOctets>%"PRIu64"</IfHCOutOctets> <!-- in Flits -->\n",
			indent+4, "", pPortCounters->portXmitData);
		XmlPrintDec64("IfHCInOctetsMB", pPortCounters->portRcvData/FLITS_PER_MB, indent+4);
		XmlPrintDec64("IfHCOutUcastPkts", pPortCounters->portXmitPkts, indent+4);
		printf("%*s<IfHCInOctets>%"PRIu64"</IfHCInOctets> <!-- in Flits -->\n",
			indent+4, "", pPortCounters->portRcvData);
		XmlPrintDec64("IfHCInUcastPkts", pPortCounters->portRcvPkts, indent+4);
		XmlPrintDec64("IfHCOutMulticastPkts", pPortCounters->portMulticastXmitPkts, indent+4);
		XmlPrintDec64("IfHCInMulticastPkts", pPortCounters->portMulticastRcvPkts, indent+4);
		// Signal Integrity and Node/Link Stability
		XmlPrintDec64("Dot3HCStatsInternalMacTransmitErrors", pPortCounters->dot3HCStatsInternalMacTransmitErrors, indent+4);
		XmlPrintDec64("Dot3HCStatsInternalMacReceiveErrors", pPortCounters->portRcvErrors, indent+4);
		XmlPrintDec64("Dot3HCStatsSymbolErrors", pPortCounters->localLinkIntegrityErrors, indent+4);
		// Packet Integrity
		XmlPrintDec("IfOutErrors", pPortCounters->ifOutErrors, indent+4);
		XmlPrintDec("IfInErrors", pPortCounters->ifInErrors, indent+4);
		XmlPrintDec("IfInUnknownProtos", pPortCounters->ifInUnknownProtos, indent+4);
		XmlPrintDec64("Dot3HCStatsAlignmentErrors", pPortCounters->dot3HCStatsAlignmentErrors, indent+4);
		XmlPrintDec64("Dot3HCStatsFCSErrors", pPortCounters->dot3HCStatsFCSErrors, indent+4);
		XmlPrintDec64("Dot3HCStatsFrameTooLongs", pPortCounters->excessiveBufferOverruns, indent+4);
		// Packet Discards
		XmlPrintDec64("IfOutDiscards", pPortCounters->portXmitDiscards, indent+4);
		XmlPrintDec64("IfInDiscards", pPortCounters->portRcvFECN, indent+4);
		// Half-Duplex Detection
		XmlPrintDec("Dot3StatsCarrierSenseErrors", pPortCounters->dot3StatsCarrierSenseErrors, indent+4);
		XmlPrintDec("Dot3StatsSingleCollisionFrames", pPortCounters->dot3StatsSingleCollisionFrames, indent+4);
		XmlPrintDec("Dot3StatsMultipleCollisionFrames", pPortCounters->dot3StatsMultipleCollisionFrames, indent+4);
		XmlPrintDec("Dot3StatsSQETestErrors", pPortCounters->dot3StatsSQETestErrors, indent+4);
		XmlPrintDec("Dot3StatsDeferredTransmissions", pPortCounters->dot3StatsDeferredTransmissions, indent+4);
		XmlPrintDec("Dot3StatsLateCollisions", pPortCounters->dot3StatsLateCollisions, indent+4);
		XmlPrintDec("Dot3StatsExcessiveCollisions", pPortCounters->dot3StatsExcessiveCollisions, indent+4);
		printf("%*s</Performance>\n", indent, "");
		break;
	default:
		break;
	}
}

// output verbose summary of an STL Port
void ShowPortSummary(PortData *portp, Format_t format, int indent, int detail)
{
	STL_PORT_INFO *pPortInfo = &portp->PortInfo;
	char buf1[SHOW_BUF_SIZE], buf2[SHOW_BUF_SIZE], buf3[SHOW_BUF_SIZE];

	switch (format) {
	case FORMAT_TEXT:
		if (portp->PortGUID)
			if (g_persist || g_hard)
				printf("%*sPortNum: %3u EndMgmtIfID: xxxxxxxxxx MgmtIfAddr: 0x%016"PRIx64"\n",
					indent, "", portp->PortNum, portp->PortGUID);
			else
				printf("%*sPortNum: %3u EndMgmtIfID: 0x%.*x MgmtIfAddr: 0x%016"PRIx64" LclMgmtIfID: %-3d\n",
					indent, "", portp->PortNum,
					(portp->EndPortLID <= IB_MAX_UCAST_LID ? 4:8),
					portp->EndPortLID, portp->PortGUID, pPortInfo->LocalPortNum);
		else
			printf("%*sPortNum: %3u    PortId: %.*s\n",
				indent, "", portp->PortNum, TINY_STR_ARRAY_SIZE, pPortInfo->LocalPortId);
		{
			PortSelector* portselp = GetPortSelector(portp);
			if (portselp && portselp->details) {
				printf("%*sPortDetails: %s\n", indent+4, "", portselp->details);
			}
		}
		if (portp->neighbor) {
			ShowLinkPortSummary(portp->neighbor, "Neighbor: ", format, indent+4, detail);
			if (detail-1)
				ShowExpectedLinkSummary(portp->neighbor->elinkp, format, indent+8, detail-1);
		}
		if (detail) {
			if (g_hard) {
				printf( "%*sLclMgmtIfID: %-3d PortState: xxxxxx           PhysState: xxxxxxxx\n",
					indent+4, "", pPortInfo->LocalPortNum);
			}
			else {
				uint8 ldr = 0;
				if (pPortInfo->LinkDownReason)
					ldr = pPortInfo->LinkDownReason;
				else if (pPortInfo->NeighborLinkDownReason)
					ldr = pPortInfo->NeighborLinkDownReason;

				if (pPortInfo->PortStates.s.PortState == ETH_PORT_DOWN && ldr && ! g_persist) {
					printf( "%*sLclMgmtIfID: %-3d PortState: %-6s (%-13s) PhysState: %-8s\n",
							indent+4, "",
							pPortInfo->LocalPortNum,
							EthPortStateToText(pPortInfo->PortStates.s.PortState),
							EthMauMediaAvailableToText(ldr),
							EthMauStatusToText(pPortInfo->PortStates.s.PortPhysicalState));
				} else {
					printf( "%*sLclMgmtIfID: %-3d PortState: %-6s                 PhysState: %-8s\n",
						indent+4, "",
						pPortInfo->LocalPortNum,
						EthPortStateToText(pPortInfo->PortStates.s.PortState),
						EthMauStatusToText(pPortInfo->PortStates.s.PortPhysicalState));
				}
				printf( "%*sIsAutoNegEnabled: %-5s\n",
						indent+4, "",
						pPortInfo->PortStates.s.PortPhysicalState ?
								(pPortInfo->PortStates.s.IsSMConfigurationStarted?"True":"False")
								: "False");
			}
			printf("%*sPortType: (%d) %-3s       IPAddr IPv4: %d.%d.%d.%d\n", indent+4, "", pPortInfo->PortPhysConfig.s.PortType,
					EthPortTypeToText(pPortInfo->PortPhysConfig.s.PortType), pPortInfo->IPAddrIPV4.addr[0],
					pPortInfo->IPAddrIPV4.addr[1],pPortInfo->IPAddrIPV4.addr[2],pPortInfo->IPAddrIPV4.addr[3]);
			if (g_hard) {
				printf( "%*sIfID:    xxxxxxxxxx\n",
					indent+4, "");
				printf( "%*sRespTimeout: xxxxxxx",
					indent+4, "");
			} else if (g_persist) {
				printf( "%*sIfID:    xxxxxxxxxx\n",
					indent+4, "");
				FormatTimeoutMult(buf1, pPortInfo->Resp.TimeValue);
				printf( "%*sRespTimeout: %s\n",
					indent+4, "", buf1);
			} else {
				printf( "%*sIfID:    0x%.*x\n",
					indent+4, "",
					(pPortInfo->LID <= IB_MAX_UCAST_LID ? 4:8),
					pPortInfo->LID);
				FormatTimeoutMult(buf1, pPortInfo->Resp.TimeValue);
				printf( "%*sRespTimeout: %s\n",
					indent+4, "", buf1);
			}
			printf("%*sMTU Supported: %d bytes\n",
					indent+4, "",
					pPortInfo->MTU2);
			if (g_hard)
				printf( "%*sLinkSpeed: Active: xxxxxxx  Supported: %10s  Enabled: xxxxxxxxxx\n",
					indent+4, "",
					EthLinkSpeedToText(pPortInfo->LinkSpeed.Supported, buf1, sizeof(buf1)));
			else {
				if (pPortInfo->LinkModeSupLen) {
					printf( "%*sLinkSpeed: Active: %7s (%d mbit/s)  Supported: %10s (%s)\n",
						indent+4, "",
						EthLinkSpeedToText(pPortInfo->LinkSpeed.Active, buf1, sizeof(buf1)),
						pPortInfo->IfSpeed,
						EthLinkSpeedToText(pPortInfo->LinkSpeed.Supported, buf2, sizeof(buf2)),
						EthSupportedLinkModeToText(pPortInfo->LinkModeSupported,
								pPortInfo->LinkModeSupLen, buf3, sizeof(buf3)));
				} else {
					printf( "%*sLinkSpeed: Active: %7s (%d mbit/s)  Supported: -\n",
							indent+4, "",
							EthLinkSpeedToText(pPortInfo->LinkSpeed.Active, buf1, sizeof(buf1)),
							pPortInfo->IfSpeed);
				}
			}

			printf("%*sIPAddr Prim/Sec: %s / %s\n",
				indent+4, "",
				inet_ntop(AF_INET6, pPortInfo->IPAddrIPV6.addr, buf1, sizeof(buf1)),
				inet_ntop(AF_INET, pPortInfo->IPAddrIPV4.addr, buf2, sizeof(buf2)));

			printf("%*sNeighborNodeType: %s\n",
				indent+4, "",
				OpaNeighborNodeTypeToText(pPortInfo->PortNeighborMode.NeighborNodeType));
			FormatEthCapabilityMask(buf1, pPortInfo->CapabilityMask);
			printf( "%*sCapability Supported 0x%08x: %s\n",
				indent+4, "",
				pPortInfo->CapabilityMask.AsReg32, buf1);
			FormatEthCapabilityMask3(buf1, pPortInfo->CapabilityMask3, sizeof(buf1));
			printf("%*sCapability Enabled 0x%04x: %s\n",
				indent+4, "",
				pPortInfo->CapabilityMask3.AsReg16, buf1);

			if (!g_hard) {
				if (pPortInfo->CapabilityMask3.s.IsMAXLIDSupported)
					printf("%*sMaxLID: %u\n",
						indent+4, "", pPortInfo->MaxLID);
			}

			if (portp->pPortCounters && detail > 2 && ! g_persist && ! g_hard) {
				ShowPortCounters(portp->pPortCounters, format, indent+4, detail-2);
			}
		} else {
			if (g_hard)
				printf("%*sWidth: xxxx Speed: xxxxxxx Downgraded? xxx\n",
					indent+4, "");
			else
				printf("%*sSpeed: %7s\n",
					indent+4, "",
					EthLinkSpeedToText(pPortInfo->LinkSpeed.Active, buf2, sizeof(buf2)));
		}
		break;
	case FORMAT_XML:
		printf("%*s<PortInfo id=\"0x%016"PRIx64":%u\">\n", indent, "",
				portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
		XmlPrintDec("PortNum", portp->PortNum, indent+4);
		if (portp->PortGUID) {
			if (! (g_persist || g_hard))
				XmlPrintPortIfID("EndMgmtIfID", portp->EndPortLID,
							indent+4);
			XmlPrintHex64("MgmtIfAddr", portp->PortGUID, indent+4);
		} else {
			XmlPrintStr("PortId", (const char*) pPortInfo->LocalPortId, indent+4);
		}
		{
			PortSelector* portselp = GetPortSelector(portp);
			if (portselp && portselp->details) {
				XmlPrintOptionalStr("PortDetails", portselp->details, indent+4);
			}
		}
		if (portp->neighbor) {
			printf("%*s<Neighbor>\n", indent+4, "");
			ShowLinkPortSummary(portp->neighbor, "Neighbor: ", format, indent+8, detail);
			if (detail-1)
				ShowExpectedLinkSummary(portp->neighbor->elinkp, format, indent+12, detail-1);
			printf("%*s</Neighbor>\n", indent+4, "");
		}
		if (detail) {
			if (g_hard) {
				// noop
				XmlPrintDec("LclMgmtIfID", pPortInfo->LocalPortNum, indent+4);
				XmlPrintStr("PortType", EthPortTypeToText(pPortInfo->PortPhysConfig.s.PortType), indent+4);
				XmlPrintHex("PortType_Int", pPortInfo->PortPhysConfig.s.PortType, indent+4);
			} else {
				XmlPrintDec("LclMgmtIfID", pPortInfo->LocalPortNum, indent+4);
				XmlPrintStr("PortState",
					EthPortStateToText(pPortInfo->PortStates.s.PortState), indent+4);
				XmlPrintDec("PortState_Int",
					pPortInfo->PortStates.s.PortState, indent+4);
				XmlPrintStr("PortType", EthPortTypeToText(pPortInfo->PortPhysConfig.s.PortType), indent+4);
				XmlPrintHex("PortType_Int", pPortInfo->PortPhysConfig.s.PortType, indent+4);

				XmlPrintStr("PhysState",
					EthMauStatusToText(pPortInfo->PortStates.s.PortPhysicalState),
					indent+4);
				XmlPrintDec("PhysState_Int",
					pPortInfo->PortStates.s.PortPhysicalState, indent+4);
				XmlPrintStr("IsSMConfigurationStarted", pPortInfo->PortStates.s.IsSMConfigurationStarted?"True":"False", indent+4);
			}
			if (g_hard) {
				// noop
			} else {
				if (! g_persist)
					XmlPrintPortIfID("IfID", pPortInfo->LID, indent+4);
				FormatTimeoutMult(buf1, pPortInfo->Resp.TimeValue);
				XmlPrintStr("RespTimeout", buf1, indent+4);
				XmlPrintDec("RespTimeout_Int", pPortInfo->Resp.TimeValue,
								indent+4);
			}

			XmlPrintDec("MTUSupported",
					pPortInfo->MTU2, indent+4);
			if (! g_hard) {
				XmlPrintLinkSpeed("LinkSpeedActive",
					pPortInfo->LinkSpeed.Active, indent+4);
			}
			if (pPortInfo->LinkModeSupLen) {
				XmlPrintLinkSpeed("LinkSpeedSupported",
						pPortInfo->LinkSpeed.Supported, indent+4);
			} else {
				XmlPrintStr("LinkSpeedSupported", "-", indent+4);
			}

			XmlPrintStr("IPV6", inet_ntop(AF_INET6, pPortInfo->IPAddrIPV6.addr, buf1, sizeof(buf1)), indent+4);
			XmlPrintStr("IPV4", inet_ntop(AF_INET, pPortInfo->IPAddrIPV4.addr, buf2, sizeof(buf2)), indent+4);

			XmlPrintStr("NeighborModeNeighborNodeType",
				OpaNeighborNodeTypeToText(pPortInfo->PortNeighborMode.NeighborNodeType),
				indent+4);

			FormatStlCapabilityMask(buf1, pPortInfo->CapabilityMask);
			XmlPrintHex32("CapabilityMask",
				pPortInfo->CapabilityMask.AsReg32,
				indent+4);
			XmlPrintStr("Capability", buf1, indent+4);
			XmlPrintHex16("CapabilityMask3", pPortInfo->CapabilityMask3.AsReg16, indent+4);
			FormatStlCapabilityMask3(buf1, pPortInfo->CapabilityMask3, sizeof(buf1));
			XmlPrintStr("Capability3", buf1, indent+4);
			if (! g_hard && ! g_persist) {
				XmlPrintHex8("LinkDownReason", pPortInfo->LinkDownReason, indent+4);
				XmlPrintHex8("NeighborLinkDownReason", pPortInfo->NeighborLinkDownReason, indent+4);
			}

			if (! g_hard ) {
				if(pPortInfo->CapabilityMask3.s.IsMAXLIDSupported)
					XmlPrintPortIfID("MaxIfID", pPortInfo->MaxLID, indent+4);
			}

			if (portp->pPortCounters && detail > 2 && ! g_persist && ! g_hard) {
				ShowPortCounters(portp->pPortCounters, format, indent+8, detail-2);
			}
		} else {
			if (! g_hard) {
				XmlPrintLinkSpeed("LinkSpeedActive",
					pPortInfo->LinkSpeed.Active, indent+4);
			}
		}
		printf("%*s</PortInfo>\n", indent, "");
		break;
	default:
		break;
	}
}	// End of ShowPortSummary()

// output verbose summary of a IB Node
void ShowNodeSummary(NodeData *nodep, Point *focus, Format_t format, int indent, int detail)
{
	cl_map_item_t *p;
	//char buf1[SHOW_BUF_SIZE], buf2[SHOW_BUF_SIZE];

	switch (format) {
	case FORMAT_TEXT:
		// omit fields which are port specific:
		// IfID, MgmtIfAddr, MgmtIfID
		printf("%*sName: %.*s\n",
					indent, "",
					STL_NODE_DESCRIPTION_ARRAY_SIZE,
					g_noname?g_name_marker:(char*)nodep->NodeDesc.NodeString);
		printf("%*sIfAddr: 0x%016"PRIx64" Type: %s\n",
					indent+4, "", nodep->NodeInfo.NodeGUID,
					StlNodeTypeToText(nodep->NodeInfo.NodeType));
		if (nodep->enodep && nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", nodep->enodep->details);
		}
		printf("%*sPorts: %d ChassisID: 0x%016"PRIx64"\n",
					indent+4, "", nodep->NodeInfo.NumPorts,
					nodep->NodeInfo.SystemImageGUID);
		printf("%*sVendorID: 0x%x MfgName: %.*s\n",
					indent+4, "",
					nodep->NodeInfo.u1.s.VendorID,
					SMALL_STR_ARRAY_SIZE, nodep->NodeInfo.MfgName);
		printf("%*sHardwareRev: %.*s FirmwareRev: %.*s\n",
					indent+4, "",
					SMALL_STR_ARRAY_SIZE, nodep->NodeInfo.HardwareRev,
					SMALL_STR_ARRAY_SIZE, nodep->NodeInfo.FirmwareRev);
		printf("%*sDevName: %.*s PartNum: %.*s SerialNum: %.*s\n",
					indent+4, "", SMALL_STR_ARRAY_SIZE, nodep->NodeInfo.DeviceName,
					SMALL_STR_ARRAY_SIZE, nodep->NodeInfo.PartNum,
					SMALL_STR_ARRAY_SIZE, nodep->NodeInfo.SerialNum);

		// TODO - cjking: SNMP data population of SwitchInfo Record not supported in
		// the first release
#if 0					
		if (nodep->pSwitchInfo) {
			STL_SWITCHINFO_RECORD *pSwitchInfoRecord = nodep->pSwitchInfo;
			STL_SWITCH_INFO *pSwitchInfo = &pSwitchInfoRecord->SwitchInfoData;
			if (g_persist || g_hard)
				printf("%*sLID: xxxxxxxxxx", indent+4, "");
			else
				printf("%*sLID: 0x%.*x", indent+4, "", (pSwitchInfoRecord->RID.LID <= IB_MAX_UCAST_LID ? 4:8),
					pSwitchInfoRecord->RID.LID);
			printf( " LinearFDBCap: %5u ",
				pSwitchInfo->LinearFDBCap );
			if (g_persist || g_hard)
				printf("LinearFDBTop: xxxxxxxxxx");
			else
				printf( "LinearFDBTop: 0x%08x",
					pSwitchInfo->LinearFDBTop );
			printf( " MCFDBCap: %5u\n",
				pSwitchInfo->MulticastFDBCap );
			printf("%*sPartEnfCap: %5u ", indent+4, "",	pSwitchInfo->PartitionEnforcementCap);
			if (g_persist || g_hard) {
				printf("U1: 0x%02x PortStateChange: x  SwitchLifeTime x\n",
					pSwitchInfo->u1.AsReg8);
				printf("%*sU2: 0x%02x: %s\n",
					indent+4, "",
					pSwitchInfo->u2.AsReg8,
					pSwitchInfo->u2.s.EnhancedPort0?"E0 ": "");
			} else { 
				printf("U1: 0x%02x PortStateChange: %1u  SwitchLifeTime %2u\n",
					pSwitchInfo->u1.AsReg8,
					pSwitchInfo->u1.s.PortStateChange, 
					pSwitchInfo->u1.s.LifeTimeValue);
				printf("%*sU2: 0x%02x: %s\n",
					indent+4, "",
					pSwitchInfo->u2.AsReg8,
					pSwitchInfo->u2.s.EnhancedPort0?"E0 ": "");
			}

			if (! (g_persist || g_hard)) {
				printf("%*sAR: 0x%04x: %s%s%s%sF%u T%u\n",
					indent+4, "",
					pSwitchInfo->AdaptiveRouting.AsReg16,
					pSwitchInfo->AdaptiveRouting.s.Enable?"On ": "",
					pSwitchInfo->AdaptiveRouting.s.Pause?"Pause ": "",
					pSwitchInfo->AdaptiveRouting.s.Algorithm?((pSwitchInfo->AdaptiveRouting.s.Algorithm==2)?"RGreedy ":"Greedy "): "Random ",
					pSwitchInfo->AdaptiveRouting.s.LostRoutesOnly?"LostOnly ": "",
					pSwitchInfo->AdaptiveRouting.s.Frequency,
					pSwitchInfo->AdaptiveRouting.s.Threshold);
			} else {
				printf("%*sAR: xxxxxx: x\n", indent+4, "");
			}
			printf("%*sCapabilityMask: 0x%04x: %s%s\n",
					indent+4, "",
					pSwitchInfo->CapabilityMask.AsReg16,
					pSwitchInfo->CapabilityMask.s.IsAddrRangeConfigSupported?"ARC ":"",
					pSwitchInfo->CapabilityMask.s.IsAdaptiveRoutingSupported?"AR ":"");

			if (! (g_persist || g_hard)) {
				printf("%*sRouting Mode: Supported: 0x%x Enabled 0x%x\n",
					indent+4, "",
					pSwitchInfo->RoutingMode.Supported,
					pSwitchInfo->RoutingMode.Enabled);
			} else {
				printf("%*sRouting Mode: Supported: 0x%x Enabled x\n", 
					indent+4, "",
					pSwitchInfo->RoutingMode.Supported);
			}
			{
				printf("%*sIPAddrIPV6:  %s IPAddrIPV4: %s\n",
					indent+4, "",
					inet_ntop(AF_INET6, 
						pSwitchInfo->IPAddrIPV6.addr, 
						buf1, sizeof(buf1)),
					inet_ntop(AF_INET, 
						pSwitchInfo->IPAddrIPV4.addr, 
						buf2, sizeof(buf2)));
			}
		}
#endif		
	
		printf("%*s%u Connected Ports%s\n", indent, "",
				CountInitializedPorts(&g_Fabric, nodep), detail?":":"");
		break;
	case FORMAT_XML:
		// omit fields which are port specific:
		// IfID, MgmtIfAddr, MgmtIfID
		printf("%*s<Node id=\"0x%016"PRIx64"\">\n", indent, "",
					nodep->NodeInfo.NodeGUID);
		XmlPrintNodeDesc((char*)nodep->NodeDesc.NodeString, indent+4);
		XmlPrintNodeType(nodep->NodeInfo.NodeType, indent+4);
		if (nodep->enodep && nodep->enodep->details) {
			XmlPrintOptionalStr("NodeDetails", nodep->enodep->details, indent+4);
		}
		XmlPrintDec("NumPorts", nodep->NodeInfo.NumPorts, indent+4);
		XmlPrintHex64("ChassisID", nodep->NodeInfo.SystemImageGUID, indent+4);
		XmlPrintHex64("IfAddr", nodep->NodeInfo.NodeGUID, indent+4);
		XmlPrintHex("VendorID", nodep->NodeInfo.u1.s.VendorID, indent+4);
		XmlPrintStr("MfgName", (const char*)nodep->NodeInfo.MfgName, indent+4);
		XmlPrintStr("HardwareRev", (const char*)nodep->NodeInfo.HardwareRev, indent+4);
		XmlPrintStr("FirmwareRev", (const char*)nodep->NodeInfo.FirmwareRev, indent+4);
		XmlPrintStr("DevName", (const char*)nodep->NodeInfo.DeviceName, indent+4);
		XmlPrintStr("PartNum", (const char*)nodep->NodeInfo.PartNum, indent+4);
		XmlPrintStr("SerialNum", (const char*)nodep->NodeInfo.SerialNum, indent+4);

		// TODO - cjking: SNMP data population of SwitchInfo Record not supported in
		// the first release
#if 0
		if (nodep->pSwitchInfo) {
			STL_SWITCHINFO_RECORD *pSwitchInfoRecord = nodep->pSwitchInfo;
			STL_SWITCH_INFO *pSwitchInfo = &pSwitchInfoRecord->SwitchInfoData;
			if (! (g_persist || g_hard))
				XmlPrintLID("LID", pSwitchInfoRecord->RID.LID, indent+4);
			XmlPrintDec("LinearFDBCap", pSwitchInfo->LinearFDBCap, indent+4);
			XmlPrintDec("MulticastFDBCap", pSwitchInfo->MulticastFDBCap, indent+4);
			if (! (g_persist || g_hard))
				XmlPrintDec("LinearFDBTop", pSwitchInfo->LinearFDBTop, indent+4);

			{
				XmlPrintStr("IPAddrIPV6",
					inet_ntop(AF_INET6, 
						pSwitchInfo->IPAddrIPV6.addr, 
						buf1, sizeof(buf1)),
					indent+4);
				XmlPrintStr("IPAddrIPV4",
					inet_ntop(AF_INET, 
						pSwitchInfo->IPAddrIPV4.addr, 
						buf1, sizeof(buf1)),
					indent+4);
			}
			XmlPrintHex8("U1",pSwitchInfo->u1.AsReg8,indent+4);
			if ( ! (g_persist || g_hard)) {
				XmlPrintDec("PortStateChange",
					pSwitchInfo->u1.s.PortStateChange,
					indent+4);
				XmlPrintDec("SwitchLifeTime",
					pSwitchInfo->u1.s.LifeTimeValue,
					indent+4);
			}
			XmlPrintDec("PartitionEnforcementCap",
					pSwitchInfo->PartitionEnforcementCap,
					indent+4);
			XmlPrintHex8("RoutingModeSupported",
					pSwitchInfo->RoutingMode.Supported, indent+4);
			if (!g_hard && !g_persist) {
				XmlPrintHex8("RoutingModeEnabled",
					pSwitchInfo->RoutingMode.Enabled, indent+4);
			}
			XmlPrintHex8("U2",
					pSwitchInfo->u2.AsReg8, indent+4);
			printf("%*s<Capability>%s</Capability>\n",
					indent+4, "",
					pSwitchInfo->u2.s.EnhancedPort0?"E0 ": "");
			if ( ! (g_persist || g_hard)) {
				XmlPrintHex8("AdaptiveRouting",
					pSwitchInfo->AdaptiveRouting.AsReg16, indent+4);
				XmlPrintDec("LostRoutesOnly",
					pSwitchInfo->AdaptiveRouting.s.LostRoutesOnly, indent+4);
				XmlPrintDec("Pause",
					pSwitchInfo->AdaptiveRouting.s.Pause, indent+4);
				XmlPrintDec("Enable",
					pSwitchInfo->AdaptiveRouting.s.Enable, indent+4);
				XmlPrintDec("Algorithm",
					pSwitchInfo->AdaptiveRouting.s.Algorithm, indent+4);
				XmlPrintDec("Frequency",
					pSwitchInfo->AdaptiveRouting.s.Frequency, indent+4);
				XmlPrintDec("Threshold",
					pSwitchInfo->AdaptiveRouting.s.Threshold, indent+4);
			}

			if (pSwitchInfo->CapabilityMask.AsReg16) {
				// only output if there are some vendor capabilities
				XmlPrintHex16("StlCapabilityMask",
					pSwitchInfo->CapabilityMask.AsReg16, indent+4);
				printf("%*s<StlCapability>%s</StlCapability>\n",
					indent+4, "",
					pSwitchInfo->CapabilityMask.s.IsAdaptiveRoutingSupported?"AR ": "");
			}
		}
#endif

		XmlPrintDec("ConnectedPorts", CountInitializedPorts(&g_Fabric, nodep),
						indent+4);
		break;
	default:
		break;
	}
	if (detail) {
		for (p=cl_qmap_head(&nodep->Ports); p != cl_qmap_end(&nodep->Ports); p = cl_qmap_next(p)) {
			PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
			if (! ComparePortPoint(portp, focus))
				continue;
			ShowPortSummary(portp, format, indent+4, detail-1);
		}
	}
	if (format == FORMAT_XML) {
		printf("%*s</Node>\n", indent, "");
	}
}	// End of ShowNodeSummary()

// output verbose summary of a IB System
void ShowSystemSummary(SystemData *systemp, Point *focus,
						Format_t format, int indent, int detail)
{
	cl_map_item_t *p;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sChassisID: 0x%016"PRIx64"\n", indent, "", systemp->SystemImageGUID);
		printf("%*s%u Connected Nodes%s\n", indent, "", (unsigned)cl_qmap_count(&systemp->Nodes), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<System id=\"0x%016"PRIx64"\">\n", indent, "",
					systemp->SystemImageGUID?systemp->SystemImageGUID:
					PARENT_STRUCT(cl_qmap_head(&systemp->Nodes), NodeData, SystemNodesEntry)->NodeInfo.NodeGUID);
		XmlPrintHex64("ChassisID", systemp->SystemImageGUID, indent+4);
		XmlPrintDec("ConnectedNodes", (unsigned)cl_qmap_count(&systemp->Nodes),
						indent+4);
		break;
	default:
		break;
	}
	if (detail) {
		for (p=cl_qmap_head(&systemp->Nodes); p != cl_qmap_end(&systemp->Nodes); p = cl_qmap_next(p)) {
			NodeData *nodep = PARENT_STRUCT(p, NodeData, SystemNodesEntry);
			if (! CompareNodePoint(nodep, focus))
				continue;
			ShowNodeSummary(nodep, focus, format, indent+4, detail-1);
		}
	}
	if (format == FORMAT_XML) {
		printf("%*s</System>\n", indent, "");
	}
}

// output verbose summary of all IB Components (Systems, Nodes, Ports)
void ShowComponentReport(Point *focus, Format_t format, int indent, int detail)
{
	cl_map_item_t *p;
	uint32 count = 0;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sComponent Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<ComponentSummary>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected Systems%s\n", indent, "", (unsigned)cl_qmap_count(&g_Fabric.AllSystems), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<Systems>\n", indent, "");
		XmlPrintDec("ConnectedSystems", (unsigned)cl_qmap_count(&g_Fabric.AllSystems),
						indent+4);
		break;
	default:
		break;
	}
	for (p=cl_qmap_head(&g_Fabric.AllSystems); p != cl_qmap_end(&g_Fabric.AllSystems); p = cl_qmap_next(p)) {
		SystemData *systemp = PARENT_STRUCT(p, SystemData, AllSystemsEntry);
		if (! CompareSystemPoint(systemp, focus))
			continue;
		if (detail)
			ShowSystemSummary(systemp, focus, format, indent+4, detail-1);
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Systems Found\n", indent, "", count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSystems", count, indent+4);
		printf("%*s</Systems>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</ComponentSummary>\n", indent, "");
		break;
	default:
		break;
	}
}

// output verbose summary of all IB Node Types
void ShowNodeTypeReport(Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 count;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sNode Type Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<NodeTypeSummary>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected NICs in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllFIs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<NICs>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedNICCount", (unsigned)QListCount(&g_Fabric.AllFIs), indent);
		break;
	default:
		break;
	}
	count = 0;
	for (p=QListHead(&g_Fabric.AllFIs); p != NULL; p = QListNext(&g_Fabric.AllFIs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		if (! CompareNodePoint(nodep, focus))
			continue;
		if (detail)
			ShowNodeSummary(nodep, focus, format, indent+4, detail-1);
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching NICs Found\n", indent, "", count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingNICs", count, indent+4);
		indent-=4;
		printf("%*s</NICs>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected Switches in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<Switches>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs),
						indent);
		break;
	default:
		break;
	}
	count = 0;
	for (p=QListHead(&g_Fabric.AllSWs); p != NULL; p = QListNext(&g_Fabric.AllSWs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		if (! CompareNodePoint(nodep, focus))
			continue;
		if (detail)
			ShowNodeSummary(nodep, focus, format, indent+4, detail-1);
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Switches Found\n", indent, "", count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSwitches", count, indent+4);
		indent-=4;
		printf("%*s</Switches>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</NodeTypeSummary>\n", indent, "");
		break;
	default:
		break;
	}
}

// ported from sa_FabricInfoRecord.c
void PopulateFabricinfo(STL_FABRICINFO_RECORD* rec)
{
	LIST_ITEM *p;

	rec->NumHFIs = (unsigned)QListCount(&g_Fabric.AllFIs);
	rec->NumSwitches = (unsigned)QListCount(&g_Fabric.AllSWs);

	for (p=QListHead(&g_Fabric.AllPorts); p != NULL; p = QListNext(&g_Fabric.AllPorts, p)) {
		boolean isl = FALSE;
		boolean internal = FALSE;
		boolean degraded = FALSE;
		boolean omitted = FALSE;
		PortData *portp = (PortData *)QListObj(p);
		PortData *nbrp = portp->neighbor;

		if (!nbrp) {
			continue;
		}
		// to avoid double counting, we only process when we have
		// nodep/portp as the lower NodeGuid and portNum of the link
		if (portp->nodep->NodeInfo.NodeGUID > nbrp->nodep->NodeInfo.NodeGUID) {
			continue;
		}
		if (portp->nodep->NodeInfo.NodeGUID == nbrp->nodep->NodeInfo.NodeGUID &&
				portp->PortNum > nbrp->PortNum) {
			continue;
		}

		if (portp->nodep->NodeInfo.NodeType == STL_NODE_SW &&
				nbrp->nodep->NodeInfo.NodeType == STL_NODE_SW) {
			isl = TRUE;
		}
		if (portp->nodep->NodeInfo.SystemImageGUID == nbrp->nodep->NodeInfo.SystemImageGUID) {
			internal = TRUE;
		}

		uint32 lse = EthExpectedLinkSpeed(portp->PortInfo.LinkSpeed.Supported,
				nbrp->PortInfo.LinkSpeed.Supported);
		if (
				/* Active speed should match highest speed supported on both ports */
				// treat as matched if no supported link speed
				(portp->PortInfo.LinkSpeed.Supported != ETH_LINK_SPEED_NOP &&
						nbrp->PortInfo.LinkSpeed.Supported != ETH_LINK_SPEED_NOP ) &&
				(portp->PortInfo.LinkSpeed.Active != lse || nbrp->PortInfo.LinkSpeed.Active != lse)) {
			degraded = TRUE;
		}

		if (portp->PortInfo.PortStates.s.PortState != ETH_PORT_UP) {
			omitted = TRUE;
			degraded = FALSE;
		}

		if (isl) {
			if (internal)
				rec->NumInternalISLs++;
			else
				rec->NumExternalISLs++;
		} else {
			if (internal)
				rec->NumInternalHFILinks++;
			else
				rec->NumExternalHFILinks++;
		}
		if (degraded) {
			if (isl)
				rec->NumDegradedISLs++;
			else
				rec->NumDegradedHFILinks++;
		}
		if (omitted) {
			if (isl)
				rec->NumOmittedISLs++;
			else
				rec->NumOmittedHFILinks++;
		}
	}

}


void ShowFabricinfoReport(int indent, int detail)
{
	STL_FABRICINFO_RECORD		myFI = { 0 };

	PopulateFabricinfo(&myFI);
	printf("%*sNumber of NICs: %u\n", indent, "", myFI.NumHFIs);
	printf("%*sNumber of Switches: %u\n", indent, "", myFI.NumSwitches);
	printf("%*sNumber of Links: %u\n", indent, "",
			myFI.NumInternalHFILinks +
			myFI.NumExternalHFILinks +
			myFI.NumInternalISLs +
			myFI.NumExternalISLs);
	printf("%*sNumber of NIC Links: %-7u        (Internal: %u   External: %u)\n", indent, "",
			myFI.NumInternalHFILinks + myFI.NumExternalHFILinks,
			myFI.NumInternalHFILinks, myFI.NumExternalHFILinks);
	printf("%*sNumber of ISLs Links: %-7u       (Internal: %u   External: %u)\n", indent, "",
			myFI.NumInternalISLs + myFI.NumExternalISLs,
			myFI.NumInternalISLs, myFI.NumExternalISLs);
	printf("%*sNumber of Slow Links: %-7u       (NIC Links: %u   ISLs: %u)\n", indent, "",
			myFI.NumDegradedHFILinks + myFI.NumDegradedISLs,
			myFI.NumDegradedHFILinks, myFI.NumDegradedISLs);
	printf("%*sNumber of Omitted Links: %-7u    (NIC Links: %u   ISLs: %u)\n", indent, "",
			myFI.NumOmittedHFILinks + myFI.NumOmittedISLs,
			myFI.NumOmittedHFILinks, myFI.NumOmittedISLs);
}

void ShowOtherPortSummaryHeader(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sIfAddr          Port Type Name\n", indent, "");
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

// show a non-connected port in a node
void ShowOtherPortSummary(NodeData *nodep, uint8 portNum,
			Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s0x%016"PRIx64" %3u %s %.*s\n",
			indent, "",
			nodep->NodeInfo.NodeGUID,
			portNum,
			StlNodeTypeToText(nodep->NodeInfo.NodeType),
			NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)nodep->NodeDesc.NodeString);
		if (nodep->enodep && nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", nodep->enodep->details);
		}
		break;
	case FORMAT_XML:
		printf("%*s<OtherPort id=\"0x%016"PRIx64":%u\">\n", indent, "",
				nodep->NodeInfo.NodeGUID, portNum);
		XmlPrintHex64("IfAddr",
				nodep->NodeInfo.NodeGUID, indent+4);
		XmlPrintDec("PortNum", portNum, indent+4);
		XmlPrintNodeType(nodep->NodeInfo.NodeType,
						indent+4);
		XmlPrintNodeDesc((char*)nodep->NodeDesc.NodeString, indent+4);
		if (nodep->enodep && nodep->enodep->details) {
			XmlPrintOptionalStr("NodeDetails", nodep->enodep->details, indent+4);
		}
		break;
	default:
		break;
	}
	if (format == FORMAT_XML)
		printf("%*s</OtherPort>\n", indent, "");
}

// show a non-connected port in a node
void ShowNodeOtherPortSummary(NodeData *nodep,
			Format_t format, int indent, int detail)
{
	cl_map_item_t *p;
	uint8 port;

	for (port=1, p=cl_qmap_head(&nodep->Ports); p != cl_qmap_end(&nodep->Ports);)
	{
		PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
		if (portp->PortNum == 0)
		{
			p = cl_qmap_next(p);
			continue;	/* skip switch port 0 */
		}
		if (portp->PortNum != port) {
			ShowOtherPortSummary(nodep, port, format, indent, detail);
		} else {
			if (! IsEthPortInitialized(portp->PortInfo.PortStates))
				ShowOtherPortSummary(nodep, port, format, indent, detail);
			p = cl_qmap_next(p);
		}
		port++;
	}
	/* output remaining ports after last connected port */
	for (; port <= nodep->NodeInfo.NumPorts; port++)
		ShowOtherPortSummary(nodep, port, format, indent, detail);
}

// output verbose summary of IB ports not connected to this fabric
void ShowOtherPortsReport(Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 node_count;
	uint32 port_count;
	uint32 other_port_count;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sOther Ports Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<OtherPortsSummary>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected NICs in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllFIs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<NICs>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedNICCount", (unsigned)QListCount(&g_Fabric.AllFIs), indent);
		break;
	default:
		break;
	}
	node_count = 0;
	port_count = 0;
	other_port_count = 0;
	for (p=QListHead(&g_Fabric.AllFIs); p != NULL; p = QListNext(&g_Fabric.AllFIs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		uint32 initialized_port_count;

		if (! CompareNodePoint(nodep, focus))
			continue;
		initialized_port_count = CountInitializedPorts(&g_Fabric, nodep);
		port_count += initialized_port_count;
		if (initialized_port_count >= nodep->NodeInfo.NumPorts)
			continue;
		// for NICs otherports will include ports connected to other fabrics
		if (node_count == 0 && detail)
			ShowOtherPortSummaryHeader(format, indent+4, detail-1);
		other_port_count += nodep->NodeInfo.NumPorts - initialized_port_count;
		if (detail)
			ShowNodeOtherPortSummary(nodep, format, indent+4, detail-1);
		node_count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching NICs Found\n", indent, "", node_count);
		printf("%*s%u Connected NIC Ports\n", indent, "", port_count);
		printf("%*s%u Other NIC Ports\n", indent, "", other_port_count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingNICs", node_count, indent+4);
		XmlPrintDec("ConnectedNICPorts", port_count, indent+4);
		XmlPrintDec("OtherNICPorts", other_port_count, indent+4);
		indent-=4;
		printf("%*s</NICs>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected Switches in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<Switches>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs),
						indent);
		break;
	default:
		break;
	}
	node_count = 0;
	port_count = 0;
	other_port_count = 0;
	for (p=QListHead(&g_Fabric.AllSWs); p != NULL; p = QListNext(&g_Fabric.AllSWs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		uint32 initialized_port_count;

		if (! CompareNodePoint(nodep, focus))
			continue;
		initialized_port_count = CountInitializedPorts(&g_Fabric, nodep);
		/* don't count switch port 0 */
		if (initialized_port_count) {
			PortData *portp = PARENT_STRUCT(cl_qmap_head(&nodep->Ports), PortData, NodePortsEntry);
			if (portp->PortNum == 0 && IsEthPortInitialized(portp->PortInfo.PortStates))
				initialized_port_count--;
		}
		port_count += initialized_port_count;
		if (initialized_port_count >= nodep->NodeInfo.NumPorts)
			continue;
		if (node_count == 0 && detail)
			ShowOtherPortSummaryHeader(format, indent+4, detail-1);
		other_port_count += nodep->NodeInfo.NumPorts - initialized_port_count;
		if (detail)
			ShowNodeOtherPortSummary(nodep, format, indent+4, detail-1);
		node_count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Switches Found\n", indent, "", node_count);
		printf("%*s%u Connected Switch Ports\n", indent, "", port_count);
		printf("%*s%u Other Switch Ports\n", indent, "", other_port_count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSwitches", node_count, indent+4);
		XmlPrintDec("ConnectedSwitchPorts", port_count, indent+4);
		XmlPrintDec("OtherSwitchPorts", other_port_count, indent+4);
		indent-=4;
		printf("%*s</Switches>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</OtherPortsSummary>\n", indent, "");
		break;
	default:
		break;
	}
}

// undocumented report on sizes
void ShowSizesReport(void)
{
	printf("sizeof(SystemData)=%u\n", (unsigned)sizeof(SystemData));
	printf("sizeof(NodeData)=%u\n", (unsigned)sizeof(NodeData));
	printf("sizeof(PortData)=%u\n", (unsigned)sizeof(PortData));
	printf("sizeof(STL_PORT_COUNTERS_DATA)=%u (up to 1 per port)\n", (unsigned)sizeof(STL_PORT_COUNTERS_DATA));
	//printf("sizeof(STL_SWITCHINFO_RECORD)=%u (up to 1 per switch)\n", (unsigned)sizeof(STL_SMINFO_RECORD));
	printf("sizeof(STL_NODE_RECORD)=%u\n", (unsigned)sizeof(STL_NODE_RECORD));
	printf("sizeof(STL_PORTINFO_RECORD)=%u\n", (unsigned)sizeof(STL_PORTINFO_RECORD));
	printf("sizeof(STL_LINK_RECORD)=%u\n", (unsigned)sizeof(STL_LINK_RECORD));
}

// output brief summary of a IB Port
void ShowPortBriefSummary(PortData *portp, Format_t format, int indent, int detail)
{
	char buf1[SHOW_BUF_SIZE];

	switch (format) {
	case FORMAT_TEXT:
		if (portp->PortGUID)
			if (g_hard || g_persist)
				printf("%*s%3u xxxxxx %-*s 0x%016"PRIx64,
					indent, "", portp->PortNum,
					TINY_STR_ARRAY_SIZE, portp->PortInfo.LocalPortId,
					portp->PortGUID);
			else
				printf("%*s%3u 0x%04x %-*s 0x%016"PRIx64,
					indent, "", portp->PortNum, portp->EndPortLID,
					TINY_STR_ARRAY_SIZE, portp->PortInfo.LocalPortId,
					portp->PortGUID);
		else
			printf("%*s%3u          %-*s                   ",
				indent, "", portp->PortNum, TINY_STR_ARRAY_SIZE,
				portp->PortInfo.LocalPortId);
		if (g_hard)
			printf(" xxxx xxxxxxx\n");
		else
			printf(" %7s\n",
				EthLinkSpeedToText(portp->PortInfo.LinkSpeed.Active, buf1, sizeof(buf1)));
		break;
	case FORMAT_XML:
		printf("%*s<Port id=\"0x%016"PRIx64":%u\">\n", indent, "",
				portp->nodep->NodeInfo.NodeGUID, portp->PortNum);
		XmlPrintDec("PortNum", portp->PortNum, indent+4);
		if (portp->PortGUID) {
			if (! (g_hard || g_persist)) {
				XmlPrintPortIfID("EndMgmtIfID",
					portp->EndPortLID, indent+4);
            }
			XmlPrintHex64("MgmtIfAddr", portp->PortGUID, indent+4);
		}
		XmlPrintStr("PortId", (const char*) portp->PortInfo.LocalPortId, indent+4);
		if (g_hard) {
			// noop
		} else {
			XmlPrintLinkSpeed("LinkSpeedActive",
				portp->PortInfo.LinkSpeed.Active, indent+4);
		}
		printf("%*s</Port>\n", indent, "");
		break;
	default:
		break;
	}
}

void ShowPortBriefSummaryHeadings(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sPort IfID      PortId         MgmtIfAddr          Speed\n", indent, "");
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

static void PrintXmlNodeSummaryBrief(NodeData *nodep, int indent)
{
	printf("%*s<Node id=\"0x%016"PRIx64"\">\n", indent, "",	nodep->NodeInfo.NodeGUID);
	XmlPrintHex64("IfAddr",nodep->NodeInfo.NodeGUID, indent+4);
	XmlPrintNodeType(nodep->NodeInfo.NodeType, indent+4);
	XmlPrintNodeDesc((char*)nodep->NodeDesc.NodeString, indent+4);
	if (nodep->enodep && nodep->enodep->details) {
		XmlPrintOptionalStr("NodeDetails", nodep->enodep->details, indent+4);
	}
}

static void PrintBriefNodePorts(NodeData *nodep, Point *focus, Format_t format,
				int indent, int detail)
{
	cl_map_item_t *p;

	for (p=cl_qmap_head(&nodep->Ports); p != cl_qmap_end(&nodep->Ports); p = cl_qmap_next(p)) {
		PortData *portp = PARENT_STRUCT(p, PortData, NodePortsEntry);
		if (! ComparePortPoint(portp, focus))
			continue;
		ShowPortBriefSummary(portp, format, indent+4, detail-1);
	}
}
// output brief summary of a IB Node
void ShowNodeBriefSummary(NodeData *nodep, Point *focus,
				boolean close_node, Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s0x%016"PRIx64" %s %.*s\n",
				indent, "", nodep->NodeInfo.NodeGUID,
				StlNodeTypeToText(nodep->NodeInfo.NodeType),
				NODE_DESCRIPTION_ARRAY_SIZE,
				g_noname?g_name_marker:(char*)nodep->NodeDesc.NodeString);
		if (nodep->enodep && nodep->enodep->details) {
			printf("%*sNodeDetails: %s\n", indent+4, "", nodep->enodep->details);
		}
		break;
	case FORMAT_XML:
		PrintXmlNodeSummaryBrief(nodep, indent);
		break;
	default:
		break;
	}
	if (detail)
		PrintBriefNodePorts(nodep, focus, format, indent, detail);

	if (close_node && format == FORMAT_XML) {
		printf("%*s</Node>\n", indent, "");
	}
}

void ShowTopologyNodeBriefSummary(NodeData *nodep, Point *focus, int indent, int detail)
{
	PrintXmlNodeSummaryBrief(nodep, indent);
	// Print node/port xml tag only for ethreport option -d3 or greater detail level
	if (detail >= 2)
		PrintBriefNodePorts(nodep, focus, FORMAT_XML, indent, detail);
	printf("%*s</Node>\n", indent, "");
}

// output brief summary of an expected IB Node
void ShowExpectedNodeBriefSummary(const char *prefix, ExpectedNode *enodep,
				const char *xml_tag, boolean close_node, Format_t format,
				int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%s", indent, "", prefix);
		if (enodep->NodeGUID)
			printf("0x%016"PRIx64, enodep->NodeGUID);
		else
			printf("%*s", 18, "");
		if (enodep->NodeType)
			printf(" %s", StlNodeTypeToText(enodep->NodeType));
		else
			printf("   ");
		if (enodep->NodeDesc)
			printf(" %.*s\n", NODE_DESCRIPTION_ARRAY_SIZE,
						g_noname?g_name_marker:enodep->NodeDesc);
		else
			printf("\n");
		if (enodep->details)
			printf("%*sNodeDetails: %s\n", indent+4, "", enodep->details);
		break;
	case FORMAT_XML:
		printf("%*s<%s id=\"0x%016"PRIx64"\">\n", indent, "",
					xml_tag, (uint64)(uintn)enodep);
		if (enodep->NodeGUID)
			XmlPrintHex64("IfAddr", enodep->NodeGUID, indent+4);
		if (enodep->NodeType)
			XmlPrintNodeType(enodep->NodeType, indent+4);
		if (enodep->NodeDesc)
			XmlPrintNodeDesc(enodep->NodeDesc, indent+4);
		if (enodep->details)
			XmlPrintOptionalStr("NodeDetails", enodep->details, indent+4);
		break;
	default:
		break;
	}
	if (close_node && format == FORMAT_XML) {
		printf("%*s</%s>\n", indent, "", xml_tag);
	}
}

void ShowNodeBriefSummaryHeadings(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sIfAddr           Type Name\n", indent, "");
		if (detail)
			ShowPortBriefSummaryHeadings(format, indent+4, detail-1);
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

// output brief summary of a IB System
void ShowSystemBriefSummary(SystemData *systemp, Point *focus,
						Format_t format, int indent, int detail)
{
	cl_map_item_t *p;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s0x%016"PRIx64"\n", indent, "", systemp->SystemImageGUID);
		break;
	case FORMAT_XML:
		printf("%*s<BriefSystem id=\"0x%016"PRIx64"\">\n", indent, "",
					systemp->SystemImageGUID?systemp->SystemImageGUID:
					PARENT_STRUCT(cl_qmap_head(&systemp->Nodes), NodeData, SystemNodesEntry)->NodeInfo.NodeGUID);
		XmlPrintHex64("ChassisID", systemp->SystemImageGUID, indent+4);
		break;
	default:
		break;
	}
	if (detail) {
		for (p=cl_qmap_head(&systemp->Nodes); p != cl_qmap_end(&systemp->Nodes); p = cl_qmap_next(p)) {
			NodeData *nodep = PARENT_STRUCT(p, NodeData, SystemNodesEntry);
			if (! CompareNodePoint(nodep, focus))
				continue;
			ShowNodeBriefSummary(nodep, focus, TRUE, format, indent+4, detail-1);
		}
	}
	if (format == FORMAT_XML) {
		printf("%*s</BriefSystem>\n", indent, "");
	}
}

void ShowSystemBriefSummaryHeadings(Format_t format, int indent, int detail)
{
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sChassisID\n", indent, "");
		if (detail)
			ShowNodeBriefSummaryHeadings(format, indent+4, detail-1);
		break;
	case FORMAT_XML:
		break;
	default:
		break;
	}
}

// output verbose summary of all IB Components (Systems, Nodes, Ports)
void ShowComponentBriefReport(Point *focus, Format_t format, int indent, int detail)
{
	cl_map_item_t *p;
	uint32 count = 0;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sComponent Brief Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<Components>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected Systems in Fabric%s\n", indent, "", (unsigned)cl_qmap_count(&g_Fabric.AllSystems), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<Systems>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedSystemCount", (unsigned)cl_qmap_count(&g_Fabric.AllSystems), indent);
		break;
	default:
		break;
	}
	for (p=cl_qmap_head(&g_Fabric.AllSystems); p != cl_qmap_end(&g_Fabric.AllSystems); p = cl_qmap_next(p)) {
		SystemData *systemp = PARENT_STRUCT(p, SystemData, AllSystemsEntry);
		if (! CompareSystemPoint(systemp, focus))
			continue;
		if (detail) {
			if (! count)
				ShowSystemBriefSummaryHeadings(format, indent, detail-1);
			ShowSystemBriefSummary(systemp, focus, format, indent, detail-1);
		}
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Systems Found\n", indent, "", count);
		printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSystems", count, indent);
		indent-=4;
		printf("%*s</Systems>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</Components>\n", indent, "");
		break;
	default:
		break;
	}
}

// output brief summary of all IB Node Types
void ShowNodeTypeBriefReport(Point *focus, Format_t format, report_t report, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 count;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sNode Type Brief Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<Nodes>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected NICs in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllFIs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<NICs>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedNICCount", (unsigned)QListCount(&g_Fabric.AllFIs), indent);
		break;
	default:
		break;
	}
	count = 0;
	for (p=QListHead(&g_Fabric.AllFIs); p != NULL; p = QListNext(&g_Fabric.AllFIs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		if (! CompareNodePoint(nodep, focus))
			continue;
		if (detail) {
			if (! count)
				ShowNodeBriefSummaryHeadings(format, indent, detail-1);
			if (report == REPORT_TOPOLOGY)
				ShowTopologyNodeBriefSummary(nodep, focus, indent, detail-1);
			else
				ShowNodeBriefSummary(nodep, focus, TRUE, format, indent, detail-1);
		}
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching NIC Found\n", indent, "", count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingNICs", count, indent);
		indent-=4;
		printf("%*s</NICs>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Connected Switches in Fabric%s\n", indent, "", (unsigned)QListCount(&g_Fabric.AllSWs), detail?":":"");
		break;
	case FORMAT_XML:
		printf("%*s<Switches>\n", indent, "");
		indent+=4;
		XmlPrintDec("ConnectedSwitchCount", (unsigned)QListCount(&g_Fabric.AllSWs), indent);
		break;
	default:
		break;
	}
	count = 0;
	for (p=QListHead(&g_Fabric.AllSWs); p != NULL; p = QListNext(&g_Fabric.AllSWs, p)) {
		NodeData *nodep = (NodeData *)QListObj(p);
		if (! CompareNodePoint(nodep, focus))
			continue;
		if (detail) {
			if (! count)
				ShowNodeBriefSummaryHeadings(format, indent, detail-1);
			if (report == REPORT_TOPOLOGY)
				ShowTopologyNodeBriefSummary(nodep, focus, indent, detail-1);
			else
				ShowNodeBriefSummary(nodep, focus, TRUE, format, indent, detail-1);
		}
		count++;
	}
	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Switches Found\n", indent, "", count);
		if (detail)
			printf("\n");
		break;
	case FORMAT_XML:
		if (PointValid(focus))
			XmlPrintDec("MatchingSwitches", count, indent);
		indent-=4;
		printf("%*s</Switches>\n", indent, "");
		break;
	default:
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</Nodes>\n", indent, "");
		break;
	default:
		break;
	}
}

static _inline
boolean PortCounterBelowThreshold(uint32 value, uint32 threshold)
{
	return (threshold && value < threshold);
}

static _inline
boolean PortCounterExceedsThreshold(uint32 value, uint32 threshold)
{
	return (threshold && value > threshold - g_threshold_compare);
}

static _inline
boolean PortCounterExceedsThreshold64(uint64 value, uint64 threshold)
{
	return (threshold && value > threshold - g_threshold_compare);
}

// check the last port counters against the new vs threshold
// returns: TRUE - one or more counters exceed threshold
//			FALSE - all counters below threshold
static boolean PortCountersExceedThreshold(PortData *portp)
{
	STL_PORT_COUNTERS_DATA *pPortCounters = portp->pPortCounters;

	if (! pPortCounters)
		return FALSE;

#define EXCEEDS_THRESHOLD(field) \
			PortCounterExceedsThreshold(pPortCounters->field, g_Thresholds.field)
#define EXCEEDS_THRESHOLD64(field) \
			PortCounterExceedsThreshold64(pPortCounters->field, g_Thresholds.field)
#define BELOW_THRESHOLD_LQI(field) \
			PortCounterBelowThreshold(pPortCounters->lq.s.field, g_Thresholds.lq.s.field)
#define EXCEEDS_THRESHOLD_NLD(field) \
			PortCounterExceedsThreshold(pPortCounters->lq.s.field, g_Thresholds.lq.s.field)

			// Data movement
	return EXCEEDS_THRESHOLD64(portXmitData)
			|| EXCEEDS_THRESHOLD64(portRcvData)
			|| EXCEEDS_THRESHOLD64(portXmitPkts)
			|| EXCEEDS_THRESHOLD64(portRcvPkts)
			|| EXCEEDS_THRESHOLD64(portMulticastXmitPkts)
			|| EXCEEDS_THRESHOLD64(portMulticastRcvPkts)
			// Signal Integrity and Node/Link Stability
			|| EXCEEDS_THRESHOLD64(dot3HCStatsInternalMacTransmitErrors)
			|| EXCEEDS_THRESHOLD64(portRcvErrors)
			|| EXCEEDS_THRESHOLD64(localLinkIntegrityErrors)
			// Packet Integrity
			|| EXCEEDS_THRESHOLD(ifOutErrors)
			|| EXCEEDS_THRESHOLD(ifInErrors)
			|| EXCEEDS_THRESHOLD(ifInUnknownProtos)
			|| EXCEEDS_THRESHOLD64(dot3HCStatsAlignmentErrors)
			|| EXCEEDS_THRESHOLD64(dot3HCStatsFCSErrors)
			|| EXCEEDS_THRESHOLD64(excessiveBufferOverruns)
			// Packet Discards
			|| EXCEEDS_THRESHOLD64(portXmitDiscards)
			|| EXCEEDS_THRESHOLD64(portRcvFECN)
			// Half-Duplex Detection
			|| EXCEEDS_THRESHOLD(dot3StatsCarrierSenseErrors)
			|| EXCEEDS_THRESHOLD(dot3StatsSingleCollisionFrames)
			|| EXCEEDS_THRESHOLD(dot3StatsMultipleCollisionFrames)
			|| EXCEEDS_THRESHOLD(dot3StatsSQETestErrors)
			|| EXCEEDS_THRESHOLD(dot3StatsDeferredTransmissions)
			|| EXCEEDS_THRESHOLD(dot3StatsLateCollisions)
			|| EXCEEDS_THRESHOLD(dot3StatsExcessiveCollisions);
#undef EXCEEDS_THRESHOLD
#undef EXCEEDS_THRESHOLD64
#undef BELOW_THRESHOLD_LQI
#undef EXCEEDS_THRESHOLD_NLD
}

void ShowPortCounterBelowThreshold(const char* field, uint32 value, uint32 threshold, Format_t format, int indent, int detail)
{
	if (PortCounterBelowThreshold(value, threshold))
	{
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%s: %u Below Threshold: %u\n",
				indent, "", field, value, threshold);
			break;
		case FORMAT_XML:
			// old format
			printf("%*s<%s>\n", indent, "", field);
			XmlPrintDec("Value", value, indent+4);
			XmlPrintDec("LowerThreshold", threshold, indent+4);
			printf("%*s</%s>\n", indent, "", field);
			// new format
			printf("%*s<%sValue>%u</%sValue>\n", indent, "", field, value, field);
			printf("%*s<%sThreshold>%u</%sThreshold>\n", indent, "", field, threshold, field);
			break;
		default:
			break;
		}
	}
}

void ShowPortCounterExceedingThreshold(const char* field, uint32 value, uint32 threshold, Format_t format, int indent, int detail)
{
	if (PortCounterExceedsThreshold(value, threshold))
	{
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%s: %u Exceeds Threshold: %u\n",
				indent, "", field, value, threshold);
			break;
		case FORMAT_XML:
			// old format
			printf("%*s<%s>\n", indent, "", field);
			XmlPrintDec("Value", value, indent+4);
			XmlPrintDec("Threshold", threshold, indent+4);
			printf("%*s</%s>\n", indent, "", field);
			// new format
			printf("%*s<%sValue>%u</%sValue>\n", indent, "", field, value, field);
			printf("%*s<%sThreshold>%u</%sThreshold>\n", indent, "", field, threshold, field);
			break;
		default:
			break;
		}
	}
}

void ShowPortCounterExceedingThreshold64(const char* field, uint64 value, uint64 threshold, Format_t format, int indent, int detail)
{
	if (PortCounterExceedsThreshold64(value, threshold))
	{
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%s: %"PRIu64" Exceeds Threshold: %"PRIu64"\n",
				indent, "", field, value, threshold);
			break;
		case FORMAT_XML:
			// old format
			printf("%*s<%s>\n", indent, "", field);
			XmlPrintDec64("Value", value, indent+4);
			XmlPrintDec64("Threshold", threshold, indent+4);
			printf("%*s</%s>\n", indent, "", field);
			// new format
			printf("%*s<%sValue>%"PRIu64"</%sValue>\n", indent, "", field, value, field);
			printf("%*s<%sThreshold>%"PRIu64"</%sThreshold>\n", indent, "", field, threshold, field);
			break;
		default:
			break;
		}
	}
}

void ShowPortCounterExceedingMbThreshold64(const char* field, uint64 value, uint64 threshold, Format_t format, int indent, int detail)
{
	if (PortCounterExceedsThreshold(value, threshold))
	{
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%s: %"PRIu64" MB Exceeds Threshold: %u MB\n",
				indent, "", field, value/FLITS_PER_MB, (unsigned int)(threshold/FLITS_PER_MB));
			break;
		case FORMAT_XML:
			// old format
			printf("%*s<%s>\n", indent, "", field);
			XmlPrintDec64("ValueMB", value/FLITS_PER_MB, indent+4);
			XmlPrintDec64("ThresholdMB", threshold/FLITS_PER_MB, indent+4);
			printf("%*s</%s>\n", indent, "", field);
			// new format
			printf("%*s<%sValueMB>%"PRIu64"</%sValueMB>\n", indent, "", field, value/FLITS_PER_MB, field);
			printf("%*s<%sThresholdMB>%u</%sThresholdMB>\n", indent, "", field, (unsigned int)(threshold/FLITS_PER_MB), field);
			break;
		default:
			break;
		}
	}
}

void ShowLinkPortErrorSummary(PortData *portp, Format_t format, int indent, int detail)
{
	STL_PORT_COUNTERS_DATA *pPortCounters = portp->pPortCounters;

	if (! pPortCounters)
		return;

#define SHOW_BELOW_LQI_THRESHOLD(field, name) \
			ShowPortCounterBelowThreshold(#name, pPortCounters->lq.s.field, g_Thresholds.lq.s.field, format, indent, detail)
#define SHOW_EXCEEDING_THRESHOLD(field, name) \
			ShowPortCounterExceedingThreshold(#name, pPortCounters->field, g_Thresholds.field, format, indent, detail)
#define SHOW_EXCEEDING_THRESHOLD64(field, name) \
			ShowPortCounterExceedingThreshold64(#name, pPortCounters->field, g_Thresholds.field, format, indent, detail)
#define SHOW_EXCEEDING_MB_THRESHOLD(field, name) \
			ShowPortCounterExceedingMbThreshold64(#name, pPortCounters->field, g_Thresholds.field, format, indent, detail)
#define SHOW_EXCEEDING_NLD_THRESHOLD(field, name) \
			ShowPortCounterExceedingThreshold(#name, pPortCounters->lq.s.field, g_Thresholds.lq.s.field, format, indent, detail)
	// Data movement
	SHOW_EXCEEDING_MB_THRESHOLD(portXmitData, IfHCOutOctets);
	SHOW_EXCEEDING_MB_THRESHOLD(portRcvData, IfHCInOctets);
	SHOW_EXCEEDING_THRESHOLD64(portXmitPkts, IfHCOutUcastPkts);
	SHOW_EXCEEDING_THRESHOLD64(portRcvPkts, IfHCInUcastPkts);
	SHOW_EXCEEDING_THRESHOLD64(portMulticastXmitPkts, IfHCOutMulticastPkts);
	SHOW_EXCEEDING_THRESHOLD64(portMulticastRcvPkts, IfHCInMulticastPkts);
	// Signal Integrity and Node/Link Stability
	SHOW_EXCEEDING_THRESHOLD64(dot3HCStatsInternalMacTransmitErrors, Dot3HCStatsInternalMacTransmitErrors);
	SHOW_EXCEEDING_THRESHOLD64(portRcvErrors, Dot3HCStatsInternalMacReceiveErrors);
	SHOW_EXCEEDING_THRESHOLD64(localLinkIntegrityErrors, Dot3HCStatsSymbolErrors);
	// Packet Integrity
	SHOW_EXCEEDING_THRESHOLD(ifOutErrors, IfOutErrors);
	SHOW_EXCEEDING_THRESHOLD(ifInErrors, IfInErrors);
	SHOW_EXCEEDING_THRESHOLD(ifInUnknownProtos, IfInUnknownProtos);
	SHOW_EXCEEDING_THRESHOLD64(dot3HCStatsAlignmentErrors, Dot3HCStatsAlignmentErrors);
	SHOW_EXCEEDING_THRESHOLD64(dot3HCStatsFCSErrors, Dot3HCStatsFCSErrors);
	SHOW_EXCEEDING_THRESHOLD64(excessiveBufferOverruns, Dot3HCStatsFrameTooLongs);
	// Packet Discards
	SHOW_EXCEEDING_THRESHOLD64(portXmitDiscards, IfOutDiscards);
	SHOW_EXCEEDING_THRESHOLD64(portRcvFECN, IfInDiscards);
	// Half-Duplex Detectio
	SHOW_EXCEEDING_THRESHOLD(dot3StatsCarrierSenseErrors, Dot3StatsCarrierSenseErrors);
	SHOW_EXCEEDING_THRESHOLD(dot3StatsSingleCollisionFrames, Dot3StatsSingleCollisionFrames);
	SHOW_EXCEEDING_THRESHOLD(dot3StatsMultipleCollisionFrames, Dot3StatsMultipleCollisionFrames);
	SHOW_EXCEEDING_THRESHOLD(dot3StatsSQETestErrors, Dot3StatsSQETestErrors);
	SHOW_EXCEEDING_THRESHOLD(dot3StatsDeferredTransmissions, Dot3StatsDeferredTransmissions);
	SHOW_EXCEEDING_THRESHOLD(dot3StatsLateCollisions, Dot3StatsLateCollisions);
	SHOW_EXCEEDING_THRESHOLD(dot3StatsExcessiveCollisions, Dot3StatsExcessiveCollisions);
#undef SHOW_BELOW_LQI_THRESHOLD
#undef SHOW_EXCEEDING_THRESHOLD
#undef SHOW_EXCEEDING_THRESHOLD64
#undef SHOW_EXCEEDING_MB_THRESHOLD
#undef SHOW_EXCEEDING_NLD_THRESHOLD
}

// returns TRUE if thresholds are configured
boolean ShowThresholds(Format_t format, int indent, int detail)
{
	boolean didoutput = FALSE;
	switch (format) {
	case FORMAT_TEXT:
		printf("%*sConfigured Thresholds:\n",indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<ConfiguredThresholds>\n",indent, "");
		break;
	default:
		break;
	}
#define SHOW_THRESHOLD(field, name) \
	do { if (g_Thresholds.field) { switch (format) { \
		case FORMAT_TEXT: printf("%*s%-30s %lu\n", indent+4, "", #name, (uint64)g_Thresholds.field); break; \
		case FORMAT_XML: printf("%*s<%s>%lu</%s>\n", indent+4, "", #name, (uint64)g_Thresholds.field, #name); break; \
		default: break; } didoutput = TRUE; } }  while (0)

#define SHOW_THRESHOLD_LQI_NLD(field, name) \
	do { if (g_Thresholds.lq.s.field) { switch (format) { \
		case FORMAT_TEXT: printf("%*s%-30s %lu\n", indent+4, "", #name, (uint64)g_Thresholds.lq.s.field); break; \
		case FORMAT_XML: printf("%*s<%s>%lu</%s>\n", indent+4, "", #name, (uint64)g_Thresholds.lq.s.field, #name); break; \
		default: break; } didoutput = TRUE; } }  while (0)

#define SHOW_MB_THRESHOLD(field, name) \
	do { if (g_Thresholds.field) { switch (format) { \
		case FORMAT_TEXT: printf("%*s%-30s %lu MB\n", indent+4, "", #name, (uint64)g_Thresholds.field/FLITS_PER_MB); break; \
		case FORMAT_XML: printf("%*s<%sMB>%lu</%sMB>\n", indent+4, "", #name, (uint64)g_Thresholds.field/FLITS_PER_MB, #name); break; \
		default: break; } didoutput = TRUE; } }  while (0)

	/*
	 * Data movement
	 */
	SHOW_MB_THRESHOLD(portXmitData, IfHCOutOctets);
	SHOW_MB_THRESHOLD(portRcvData, IfHCInOctets);
	SHOW_THRESHOLD(portXmitPkts, IfHCOutUcastPkts);
	SHOW_THRESHOLD(portRcvPkts, IfHCInUcastPkts);
	SHOW_THRESHOLD(portMulticastXmitPkts, IfHCOutMulticastPkts);
	SHOW_THRESHOLD(portMulticastRcvPkts, IfHCInMulticastPkts);

	/*
	 * Signal Integrity and Node/Link Stability
	 */
	SHOW_THRESHOLD(dot3HCStatsInternalMacTransmitErrors, Dot3HCStatsInternalMacTransmitErrors);
	SHOW_THRESHOLD(portRcvErrors, Dot3HCStatsInternalMacReceiveErrors);
	SHOW_THRESHOLD(localLinkIntegrityErrors, Dot3HCStatsSymbolErrors);

	/*
	 * Packet Integrity
	 */
	SHOW_THRESHOLD(ifOutErrors, IfOutErrors);
	SHOW_THRESHOLD(ifInErrors, IfInErrors);
	SHOW_THRESHOLD(ifInUnknownProtos, IfInUnknownProtos);
	SHOW_THRESHOLD(dot3HCStatsAlignmentErrors, Dot3HCStatsAlignmentErrors);
	SHOW_THRESHOLD(dot3HCStatsFCSErrors, Dot3HCStatsFCSErrors);
	SHOW_THRESHOLD(excessiveBufferOverruns, Dot3HCStatsFrameTooLongs);

	// Packet Discards
	SHOW_THRESHOLD(portXmitDiscards, IfOutDiscards);
	SHOW_THRESHOLD(portRcvFECN, IfInDiscards);

	/*
	 * Half-Duplex Detection
	 */
	SHOW_THRESHOLD(dot3StatsCarrierSenseErrors, Dot3StatsCarrierSenseErrors);
	SHOW_THRESHOLD(dot3StatsSingleCollisionFrames, Dot3StatsSingleCollisionFrames);
	SHOW_THRESHOLD(dot3StatsMultipleCollisionFrames, Dot3StatsMultipleCollisionFrames);
	SHOW_THRESHOLD(dot3StatsSQETestErrors, Dot3StatsSQETestErrors);
	SHOW_THRESHOLD(dot3StatsDeferredTransmissions, Dot3StatsDeferredTransmissions);
	SHOW_THRESHOLD(dot3StatsLateCollisions, Dot3StatsLateCollisions);
	SHOW_THRESHOLD(dot3StatsExcessiveCollisions, Dot3StatsExcessiveCollisions);

	switch (format) {
	case FORMAT_TEXT:
		if (! didoutput)
			printf("%*sNone\n", indent+4, "");
		break;
	case FORMAT_XML:
		printf("%*s</ConfiguredThresholds>\n",indent, "");
		break;
	default:
		break;
	}

#undef SHOW_THRESHOLD
#undef SHOW_MB_THRESHOLD
#undef SHOW_THRESHOLD_LQI_NLD
	return didoutput;
}

void ShowSlowLinkPortSummaryHeader(LinkReport_t report, Format_t format, int indent, int detail)
{
	ShowLinkBriefSummaryHeader(format, indent, detail);
	if (detail) {
		switch (format) {
		case FORMAT_TEXT:
		 	switch (report) {
			case LINK_EXPECTED_REPORT:
				printf("%*s Active\n", indent+4, "");
				printf("%*s Rate\n", indent+4, "");
			break;
			case LINK_CONFIG_REPORT:
				printf("%*s Supported\n", indent+4, "");
				printf("%*s Rates\n", indent+4, "");
				break;
			case LINK_CONN_REPORT:
				printf("%*s Supported\n", indent+4, "");
				printf("%*s Rates\n", indent+4, "");
				break;
			}
			DisplaySeparator();
			break;
		case FORMAT_XML:
			break;
		default:
			break;
		}
	}
}

void ShowSlowLinkReasonSummary(LinkReport_t report, PortData *portp, Format_t format, int indent, int detail)
{
	char buf1[SHOW_BUF_SIZE], buf2[SHOW_BUF_SIZE];

	switch (format) {
	case FORMAT_TEXT:
		switch (report) {
		case LINK_EXPECTED_REPORT:
			printf("%*s %-8s\n", indent, "",
				EthLinkSpeedToText(portp->PortInfo.LinkSpeed.Active, buf2, sizeof(buf2)));
			break;
		case LINK_CONFIG_REPORT:
			printf("%*s %-12s\n", indent, "",
				EthLinkSpeedToText(portp->PortInfo.LinkSpeed.Supported, buf1, sizeof(buf1)));
			break;
		case LINK_CONN_REPORT:
			printf("%*s %-12s\n", indent, "",
				EthLinkSpeedToText(portp->PortInfo.LinkSpeed.Supported, buf2, sizeof(buf2)));
			break;
		}
		break;
	case FORMAT_XML:
		switch (report) {
		case LINK_EXPECTED_REPORT:
			XmlPrintLinkSpeed("LinkSpeedActive",
				portp->PortInfo.LinkSpeed.Active, indent);
			break;
		case LINK_CONFIG_REPORT:
			XmlPrintLinkSpeed("LinkSpeedSupported",
				portp->PortInfo.LinkSpeed.Supported, indent);
			break;
		case LINK_CONN_REPORT:
			XmlPrintLinkSpeed("LinkSpeedSupported",
				portp->PortInfo.LinkSpeed.Supported, indent);
			break;
		}
		break;
	default:
		break;
	}
}

void ShowSlowLinkReasonSummaryCallback(uint64 context, PortData *portp,
									Format_t format, int indent, int detail)
{
	ShowSlowLinkReasonSummary((LinkReport_t)context, portp, format, indent, detail);
}

void ShowSlowLinkSummary(LinkReport_t report, PortData *portp1, Format_t format, int indent, int detail)
{
	ShowLinkFromBriefSummary(portp1,
					(uint64)(uintn)report, ShowSlowLinkReasonSummaryCallback,
					format, indent, detail);
	ShowLinkToBriefSummary(portp1->neighbor, "<-> ", TRUE,
					(uint64)(uintn)report, ShowSlowLinkReasonSummaryCallback,
					format, indent, detail);
}

void ShowLinkPortErrorSummaryCallback(uint64 context, PortData *portp,
									Format_t format, int indent, int detail)
{
	ShowLinkPortErrorSummary(portp, format, indent, detail);
}

// show link errors from portp1 to its neighbor
void ShowLinkErrorSummary(PortData *portp1, Format_t format, int indent, int detail)
{
	ShowLinkFromBriefSummary(portp1, 0, ShowLinkPortErrorSummaryCallback, format, indent, detail);

	ShowLinkToBriefSummary(portp1->neighbor, "<-> ", TRUE, 0, ShowLinkPortErrorSummaryCallback, format, indent, detail);
}

// output summary of Links for given report
void ShowLinksReport(Point *focus, report_t report, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 count = 0;
	char *xml_prefix = "";
	char *prefix = "";

	switch (report) {
	default:	// should not happen, but just in case
		ASSERT(0);
	case REPORT_LINKS:
		xml_prefix = "";
		prefix = "";
		break;
	case REPORT_EXTLINKS:
		xml_prefix = "Ext";
		prefix = "External ";
		break;
	case REPORT_FILINKS:
		xml_prefix = "NIC";
		prefix = "NIC ";
		break;
	case REPORT_ISLINKS:
		xml_prefix = "IS";
		prefix = "Inter-Switch ";
		break;
	case REPORT_EXTISLINKS:
		xml_prefix = "ExtIS";
		prefix = "External Inter-Switch ";
		break;
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%sLink Summary\n", indent, "", prefix);
		break;
	case FORMAT_XML:
		if (g_Fabric.LinkCount) {
			printf("%*s<%sLinkSummary>\n", indent, "", xml_prefix);
			indent+=4;
		}
		break;
	default:
		break;
	}

	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		switch (report) {
		default:	// should not happen, but just in case
		case REPORT_LINKS:
			printf("%*s%u Links in Fabric%s\n", indent, "", g_Fabric.LinkCount, detail?":":""); break;
		case REPORT_EXTLINKS:
			printf("%*s%u External Links in Fabric%s\n", indent, "", g_Fabric.ExtLinkCount, detail?":":""); break;
		case REPORT_FILINKS:
			printf("%*s%u NIC Links in Fabric%s\n", indent, "", g_Fabric.FILinkCount, detail?":":""); break;
		case REPORT_ISLINKS:
			printf("%*s%u Inter-Switch Links in Fabric%s\n", indent, "", g_Fabric.ISLinkCount, detail?":":""); break;
		case REPORT_EXTISLINKS:
			printf("%*s%u External Inter-Switch Links in Fabric%s\n", indent, "", g_Fabric.ExtISLinkCount, detail?":":""); break;
		}
		break;
	case FORMAT_XML:
		if (g_Fabric.LinkCount) {
			switch (report) {
			default:	// should not happen, but just in case
			case REPORT_LINKS:
				XmlPrintDec("LinkCount", g_Fabric.LinkCount, indent); break;
			case REPORT_EXTLINKS:
				XmlPrintDec("ExternalLinkCount", g_Fabric.ExtLinkCount, indent); break;
			case REPORT_FILINKS:
				XmlPrintDec("NICLinkCount", g_Fabric.FILinkCount, indent); break;
			case REPORT_ISLINKS:
				XmlPrintDec("ISLinkCount", g_Fabric.ISLinkCount, indent); break;
			case REPORT_EXTISLINKS:
				XmlPrintDec("ExternalISLinkCount", g_Fabric.ExtISLinkCount, indent); break;
			}
		}
		break;
	default:
		break;
	}

	if (detail)
		ShowLinkBriefSummaryHeader(format, indent, detail-1);
	for (p=QListHead(&g_Fabric.AllPorts); p != NULL; p = QListNext(&g_Fabric.AllPorts, p)) {
		PortData *portp1 = (PortData *)QListObj(p);
		// to avoid duplicated processing, only process "from" ports in link
		if (! portp1->from)
			continue;

		switch (report) {
		default:	// should not happen, but just in case
		case REPORT_LINKS:
			break;	// always show
		case REPORT_EXTLINKS:
			if (isInternalLink(portp1))
				continue;
			break;
		case REPORT_FILINKS:
			if (! isFILink(portp1))
				continue;
			break;
		case REPORT_ISLINKS:
			if (! isISLink(portp1))
				continue;
			break;
		case REPORT_EXTISLINKS:
			if (isInternalLink(portp1))
				continue;
			if (! isISLink(portp1))
				continue;
			break;
		}

		if (! ComparePortPoint(portp1, focus) && ! ComparePortPoint(portp1->neighbor, focus))
			continue;
		count++;
		if (detail)
			ShowLinkBriefSummary(portp1, "<-> ", format, indent, detail-1);
	}

	switch (format) {
	case FORMAT_TEXT:
		if (PointValid(focus))
			printf("%*s%u Matching Links Found\n", indent, "", count);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		if (g_Fabric.LinkCount) {
			if (PointValid(focus))
				XmlPrintDec("MatchingLinks", count, indent);
			indent-=4;
			printf("%*s</%sLinkSummary>\n", indent, "", xml_prefix);
		}
		break;
	default:
		break;
	}
}

// output summary of all slow IB Links
// detail = 0,1 -> link running < best enabled speed/width
// detail = 2 -> links running < best supported speed/width
// detail = >2 -> links running < max supported speed/width
// one_report indicates if only a single vs stacked reports
//	(stacked means separate sections for each previous part of report)
void ShowSlowLinkReport(LinkReport_t report, boolean one_report, Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	int loops;
	int firstloop = 1;
	int loop;
	char *xmltag="";

	switch (report) {
	default:
	case LINK_EXPECTED_REPORT:
		loops = 1;
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sLinks running slower than expected Summary\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<LinksExpected> <!-- Links running slower than expected Summary -->\n", indent, "");
			xmltag="LinksExpected";
			break;
		default:
			break;
		}
		break;
	case LINK_CONFIG_REPORT:
		loops = 2;
		if (one_report) {
			firstloop = 2;
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks configured slower than supported Summary\n", indent, "");
				break;
			case FORMAT_XML:
				printf("%*s<LinksConfig> <!-- Links configured slower than supported Summary -->\n", indent, "");
				xmltag="LinksConfig";
				break;
			default:
				break;
			}
		} else {
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks running slower than supported Summary\n", indent, "");
				break;
			case FORMAT_XML:
				printf("%*s<LinksRunningSupported> <!-- Links running slower than supported Summary -->\n", indent, "");
				xmltag="LinksRunningSupported";
				break;
			default:
				break;
			}
		}
		break;
	case LINK_CONN_REPORT:
		loops = 3;
		if (one_report) {
			firstloop = 3;
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks connected with mismatched supported speeds Summary\n", indent, "");
				break;
			case FORMAT_XML:
				printf("%*s<LinksMismatched> <!-- Links connected with mismatched supported speeds Summary -->\n", indent, "");
				xmltag="LinksMismatched";
				break;
			default:
				break;
			}
		} else {
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks running slower than faster port Summary\n", indent, "");
				break;
			case FORMAT_XML:
				printf("%*s<LinksRunningSlower> <!-- Links running slower than faster port Summary -->\n", indent, "");
				xmltag="LinksRunningSlower";
				break;
			default:
				break;
			}
		}
		break;
	}
	if (format == FORMAT_XML)
		indent+=4;
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	if (format == FORMAT_XML)
		indent-=4;

	for (loop = firstloop; loop <= loops; ++loop) {
		uint32 badcount = 0;
		uint32 checked = 0;
		LinkReport_t loop_report;

		switch (loop) {
		case 1:
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks running slower than expected:\n", indent, "");
				break;
			case FORMAT_XML:
				/* already output above */
				break;
			default:
				break;
			}
			loop_report = LINK_EXPECTED_REPORT;
			break;
		case 2:
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks configured to run slower than supported:\n", indent, "");
				break;
			case FORMAT_XML:
				if (loop != firstloop) {
					printf("</%s>\n", xmltag);
					printf("%*s<LinksConfig> <!-- Links configured slower than supported Summary -->\n", indent, "");
					xmltag="LinksConfig";
				}
				break;
			default:
				break;
			}
			loop_report = LINK_CONFIG_REPORT;
			if (g_hard) {
				switch (format) {
				case FORMAT_TEXT:
					printf("%*sReport skipped: -H option specified\n", indent, "");
					break;
				case FORMAT_XML:
					printf("%*s<!-- Report skipped: -H option specified -->\n", indent, "");
					break;
				default:
					break;
				}
				continue;
			}
			break;
		case 3:
			switch (format) {
			case FORMAT_TEXT:
				printf("%*sLinks connected with mismatched speed potential:\n", indent, "");
				break;
			case FORMAT_XML:
				if (loop != firstloop) {
					printf("</%s>\n", xmltag);
					printf("%*s<LinksMismatched> <!-- Links connected with mismatched supported speeds Summary -->\n", indent, "");
					xmltag="LinksMismatched";
				}
				break;
			default:
				break;
			}
			loop_report = LINK_CONN_REPORT;
			break;
		default:
			ASSERT(0);
			continue;
			break;
		}
		if (format == FORMAT_XML)
			indent+=4;
		for (p=QListHead(&g_Fabric.AllPorts); p != NULL; p = QListNext(&g_Fabric.AllPorts, p)) {
			PortData *portp1, *portp2;
			STL_PORT_INFO *pi1, *pi2;

			portp1 = (PortData *)QListObj(p);
			// to avoid duplicated processing, only process "from" ports in link
			if (! portp1->from)
				continue;
			portp2 = portp1->neighbor;
			if (! ComparePortPoint(portp1, focus) && ! ComparePortPoint(portp2, focus))
				continue;
			checked++;
			pi1 = &portp1->PortInfo;
			pi2 = &portp2->PortInfo;

			// If the data is invalid, declare the test failed
			if ( /* Active speed, width, widthDowngrade should all be a single bit value */
				 /* - else is invalid data */				 
				 (pi1->LinkSpeed.Active != EthBestLinkSpeed(pi1->LinkSpeed.Active)) ||
				 (pi2->LinkSpeed.Active != EthBestLinkSpeed(pi2->LinkSpeed.Active)) ) {
				printf(" The speed value retrieved is not valid. \n");
				goto show;
			}
			// Links running slower than expected (not at highest supported speed)
			if (firstloop <= 1) {
				if ( 
					 /* Active speed should match highest speed supported on both ports */
					 // treat as matched if no supported link speed
					 (pi1->LinkSpeed.Supported == ETH_LINK_SPEED_NOP ||
						pi2->LinkSpeed.Supported == ETH_LINK_SPEED_NOP ) || (
					 (pi1->LinkSpeed.Active == EthExpectedLinkSpeed(
						pi1->LinkSpeed.Supported, pi2->LinkSpeed.Supported)) &&
					 (pi2->LinkSpeed.Active == EthExpectedLinkSpeed(
						pi1->LinkSpeed.Supported, pi2->LinkSpeed.Supported)))
					 ) {
					/* active matches the best supported, cable is good */
					if (loop == 1)
						continue;
				} else {
					/* bad cable, active doesn't match best supported */
					if (loop > 1)
						continue;	/* already reported on loop 1 */
					else
						goto show;
				}
			}

			// links configured to run slower than expected (not configured to highest speed supported)
			if (firstloop <= 2) {
				/*
				 * TODO - cjking: Skip LinkSpeed.Enabled not supported in the first release.
				 */
#if 1	// HPN_PORT_OPA_FAST_FABRIC
					/* configured matches the best supported, config is good */
					if (loop == 2)
						continue;
#else
				if (
					/* The highest supported speed should be what is configured as enabled */
					(EthBestLinkSpeed(pi1->LinkSpeed.Enabled) == EthExpectedLinkSpeed( 
					   pi1->LinkSpeed.Supported, pi2->LinkSpeed.Supported)) && 
					(EthBestLinkSpeed(pi2->LinkSpeed.Enabled) == EthExpectedLinkSpeed( 
					   pi1->LinkSpeed.Supported, pi2->LinkSpeed.Supported))
					) {

					/* configured matches the best supported, config is good */
					if (loop == 2)
						continue;
				} else {
					/* bad config, active doesn't match best supported */
					if (loop > 2)
						continue;	/* already reported on loop 2 */
					else
						goto show;
				}
#endif	// HPN_PORT_OPA_FAST_FABRIC				
			}

			// Link connected with mismatched speed potential (bi-directional link not symetric)
			if (firstloop <= 3) {
				if ( 
					/* Bidirectional speed and width should match */
					(EthBestLinkSpeed(pi1->LinkSpeed.Supported) == EthBestLinkSpeed(pi2->LinkSpeed.Supported))
				) {

					/* match, connection choice is good */
					if (loop == 3)
						continue;
				} else {
					/* bad config, active doesn't match best supported */
					if (loop > 3)
						continue;	/* already reported on loop 3 */
					else
						goto show;
				}
			}

			/* bad connection choice, active doesn't match best supported */
show:
			if (detail) {
				if (! badcount)
					ShowSlowLinkPortSummaryHeader(loop_report, format, indent, detail-1);
				ShowSlowLinkSummary(loop_report, portp1, format, indent, detail-1);
			}
			badcount++;
		}
		switch (format) {
		case FORMAT_TEXT:
			printf("%*s%u of %u Links Checked, %u Errors found\n", indent, "",
						checked, g_Fabric.LinkCount, badcount);
			break;
		case FORMAT_XML:
			XmlPrintDec("LinksChecked", checked, indent);
			XmlPrintDec("TotalLinks", g_Fabric.LinkCount, indent);
			XmlPrintDec("LinksWithErrors", badcount, indent);
			break;
		default:
			break;
		}
		if (format == FORMAT_XML)
			indent-=4;
	}
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		printf("</%s>\n", xmltag);
		break;
	default:
		break;
	}
}

void ShowLinkInfoReport(Point *focus, Format_t format, int indent, int detail)
{
	cl_map_item_t *p;
	PortData *portp1, *portp2;
	NodeData *nodep;
	LIST_ITEM *q;

	printf("%*sLinkInfo Summary\n", indent, "");
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	printf( "%*s%u Links in Fabric%s\n", indent, "",g_Fabric.LinkCount, detail?":":"" );
	if (detail) {
		printf("%*s   IfAddr         Type MgmtIfID       Name          \n", indent, "");
		printf("%*sEgress EgressId       LinkSpeed  Type      IfAddr        Port PortId           MgmtIfID      Name       \n", indent, "");
	}

	// First the switches
	for (q=QListHead(&g_Fabric.AllSWs); q != NULL; q = QListNext(&g_Fabric.AllSWs, q)) {
		nodep = (NodeData *)QListObj(q);
		portp1 = FindNodePort(nodep, 0);

		if (!portp1)
			continue;
		if (!ComparePortPoint(portp1, focus))
			continue;

		printf("%*s0x%016"PRIx64" %s  %5u   %.*s \n", indent, "",
			portp1->nodep->NodeInfo.NodeGUID,
			StlNodeTypeToText(portp1->nodep->NodeInfo.NodeType),
			portp1->EndPortLID, NODE_DESCRIPTION_ARRAY_SIZE,
			g_noname?g_name_marker:(char*)portp1->nodep->NodeDesc.NodeString);

		for ( p=cl_qmap_head(&nodep->Ports);
				p != cl_qmap_end(&nodep->Ports);
				p = cl_qmap_next(p) ){
			portp2 = PARENT_STRUCT(p, PortData, NodePortsEntry);
			if (!portp2)
				continue;
			if (portp2->neighbor) {
				printf("%3u   %-*s %s      %s  0x%016"PRIx64" %3u   %-*s %5u  %.*s \n", portp2->PortNum,
					TINY_STR_ARRAY_SIZE, portp2->PortInfo.LocalPortId,
					EthStaticRateToText(portp2->rate), StlNodeTypeToText(portp2->neighbor->nodep->NodeInfo.NodeType),
					portp2->neighbor->nodep->NodeInfo.NodeGUID, portp2->neighbor->PortNum,
					TINY_STR_ARRAY_SIZE, portp2->neighbor->PortInfo.LocalPortId,
					portp2->neighbor->EndPortLID, NODE_DESCRIPTION_ARRAY_SIZE,
					g_noname?g_name_marker:(char*)portp2->neighbor->nodep->NodeDesc.NodeString);
			}

		} // End of ( p=cl_qmap_head(&nodep->Ports)

	} // End of for (q=QListHead(&g_Fabric.AllSWs);

	// now the NICs
	for (q=QListHead(&g_Fabric.AllFIs); q != NULL; q = QListNext(&g_Fabric.AllFIs, q)) {
		nodep = (NodeData *)QListObj(q);

		for ( p=cl_qmap_head(&nodep->Ports);
			p != cl_qmap_end(&nodep->Ports);
				p = cl_qmap_next(p) ){
			portp1 = PARENT_STRUCT(p, PortData, NodePortsEntry);
			if (!portp1)
				continue;
			if (! ComparePortPoint(portp1, focus))
				continue;

			printf("%*s0x%016"PRIx64" %s  %5u   %.*s\n", indent, "",
				portp1->nodep->NodeInfo.NodeGUID,
				StlNodeTypeToText(portp1->nodep->NodeInfo.NodeType),
				portp1->EndPortLID, NODE_DESCRIPTION_ARRAY_SIZE,
				g_noname?g_name_marker:(char*)portp1->nodep->NodeDesc.NodeString);

			if (portp1->neighbor){
				printf("%3u   %-*s %s      %s  0x%016"PRIx64" %3u   %-*s %5u  %.*s\n", portp1->PortNum,
					TINY_STR_ARRAY_SIZE, portp1->PortInfo.LocalPortId,
					EthStaticRateToText(portp1->rate), StlNodeTypeToText(portp1->neighbor->nodep->NodeInfo.NodeType),
					portp1->neighbor->nodep->NodeInfo.NodeGUID, portp1->neighbor->PortNum,
					TINY_STR_ARRAY_SIZE, portp1->neighbor->PortInfo.LocalPortId,
					portp1->neighbor->EndPortLID, NODE_DESCRIPTION_ARRAY_SIZE,
					g_noname?g_name_marker:(char*)portp1->neighbor->nodep->NodeDesc.NodeString);
			}

		} // End of ( p=cl_qmap_head(&nodep->Ports)

	} // End of for (q=QListHead(&g_Fabric.AllFIs);

}

// output summary of all IB Links with errors > threshold
void ShowLinkErrorReport(Point *focus, Format_t format, int indent, int detail)
{
	LIST_ITEM *p;
	uint32 count = 0;
	uint32 checked = 0;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sLinks with errors %s threshold Summary\n", indent, "",
				g_threshold_compare?">=":">");
		break;
	case FORMAT_XML:
		printf("%*s<LinkErrors> <!-- Links with errors %s threshold Summary -->\n", indent, "",
				g_threshold_compare?">=":">");
		indent+=4;
		break;
	default:
		break;
	}
	if (g_hard || g_persist) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: -H or -P option specified\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: -H or -P option specified -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}

	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	if (g_limitstats) {
		switch (format) {
		case FORMAT_TEXT:
			printf("Check limited, will not check neighbor ports\n");
			break;
		case FORMAT_XML:
			printf("<!-- Check limited, will not check neighbor ports -->\n");
			break;
		default:
			break;
		}
	}
	if (! ShowThresholds(format, indent, detail)) {
		switch (format) {
		case FORMAT_TEXT:
			printf("%*sReport skipped: no thresholds configured\n", indent, "");
			break;
		case FORMAT_XML:
			printf("%*s<!-- Report skipped: no thresholds configured -->\n", indent, "");
			break;
		default:
			break;
		}
		goto done;
	}

	for (p=QListHead(&g_Fabric.AllPorts); p != NULL; p = QListNext(&g_Fabric.AllPorts, p)) {
		PortData *portp1, *portp2;

		portp1 = (PortData *)QListObj(p);
		// to avoid duplicated processing, only process "from" ports in link
		if (! portp1->from)
			continue;
		portp2 = portp1->neighbor;
		// if g_limitstats is set, we will not have PortCounters for
		// ports outside our focus, so we will not report nor check them
		if (! ComparePortPoint(portp1, focus) && ! ComparePortPoint(portp2, focus))
			continue;
		checked++;
		if (PortCountersExceedThreshold(portp1) || PortCountersExceedThreshold(portp2))
		{
			if (detail) {
				if (count) {
					if (format == FORMAT_TEXT) {
						printf("\n");	// blank line between links
					}
				} else {
					ShowLinkBriefSummaryHeader(format, indent, detail-1);
				}
				ShowLinkErrorSummary(portp1, format, indent, detail-1);
			}
			count++;
		}
	}

	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u of %u Links Checked, %u Errors found\n", indent, "",
					checked, g_Fabric.LinkCount, count);
		break;
	case FORMAT_XML:
		XmlPrintDec("LinksChecked", checked, indent);
		XmlPrintDec("TotalLinks", g_Fabric.LinkCount, indent);
		XmlPrintDec("LinksWithErrors", count, indent);
		break;
	default:
		break;
	}

done:
	switch (format) {
	case FORMAT_TEXT:
		DisplaySeparator();
		break;
	case FORMAT_XML:
		indent-=4;
		printf("%*s</LinkErrors>\n", indent, "");
		break;
	default:
		break;
	}
}

// output summary of all LIDs
void ShowAllIPReport(Point *focus, Format_t format, int indent, int detail)
{
	cl_map_item_t *pMap;
	NodeData *nodep;
	PortData *portp;
	PortSelector* portselp;
	uint32 ct_lid;

	switch (format) {
	case FORMAT_TEXT:
		printf("%*sIfID Summary\n", indent, "");
		break;
	case FORMAT_XML:
		printf("%*s<IfIDSummary>\n", indent, "");
		indent+=4;
		break;
	default:
		break;
	}
	ShowPointFocus(focus, FIND_FLAG_FABRIC, format, indent, detail);
	switch (format) {
	case FORMAT_TEXT:
		printf( "%*s%u IfID(s) in Fabric%s\n", indent, "",g_Fabric.lidCount, detail?":":"" );
		break;
	case FORMAT_XML:
		XmlPrintDec("FabricIfIDCount", g_Fabric.lidCount, indent);
		break;
	default:
		break;
	}
	// Report LID Summary
	ct_lid = 0;
	for ( pMap=cl_qmap_head(&g_Fabric.u.AllLids);
			pMap != cl_qmap_end(&g_Fabric.u.AllLids);
			pMap = cl_qmap_next(pMap) ) {
		portp = PARENT_STRUCT(pMap, PortData, AllLidsEntry);
		portselp = GetPortSelector(portp);
		nodep = portp->nodep;

		if (!ComparePortPoint(portp, focus))
			continue;

		if (detail) {
			if (!ct_lid) {
				switch (format) {
				case FORMAT_TEXT:
					printf("%*s   IfID           IfAddr            Port Type Name\n", indent, "");
					if (g_topology_in_file)
						printf("%*s              NodeDetails          PortDetails\n", indent, "");
					break;
				case FORMAT_XML:
					break;
				default:
					break;
				}
			}
			switch (format) {
			case FORMAT_TEXT:
				printf( "%*s0x%.*x", indent, "", (portp->EndPortLID <= IB_MAX_UCAST_LID ? 4:8),
					portp->EndPortLID);
				printf("       ");
				printf( " 0x%016"PRIx64" %3u %s %s\n",
					nodep->NodeInfo.NodeGUID, portp->PortNum,
					StlNodeTypeToText(nodep->NodeInfo.NodeType),
					g_noname?g_name_marker:(char*)nodep->NodeDesc.NodeString );
				if (nodep->enodep && nodep->enodep->details)
					printf( "%*s              %s\n",
						indent, "", nodep->enodep->details );
				if (portselp && portselp->details)
					printf( "%*s                                   %s\n",
						indent, "", portselp->details );
				break;
			case FORMAT_XML:
				if (!ct_lid)
					printf("%*s<IfIDs>\n", indent, "");
				printf( "%*s<Value IfID=\"0x%.*x\">\n", indent+4, "",
					(portp->EndPortLID <= IB_MAX_UCAST_LID ? 4:8),
					portp->EndPortLID );
				XmlPrintHex64( "IfAddr",
						nodep->NodeInfo.NodeGUID, indent+8 );
				XmlPrintDec("PortNum", portp->PortNum, indent+8);
				if (portselp && portselp->details)
					XmlPrintStr("PortDetails", portselp->details, indent+8);
				XmlPrintStr( "NodeType",
					StlNodeTypeToText(nodep->NodeInfo.NodeType),
					indent+8 );
				XmlPrintNodeDesc((char*)nodep->NodeDesc.NodeString, indent+8);
				if (nodep->enodep && nodep->enodep->details)
					XmlPrintStr("NodeDetails", nodep->enodep->details, indent+8);
				printf("%*s</Value>\n", indent+4, "");
				break;
			default:
				break;
			}
		}	// End of if (detail)
		ct_lid = ct_lid + 1;
	}	// End of for ( pMap=cl_qmap_head(&g_Fabric.AllLids)


	switch (format) {
	case FORMAT_TEXT:
		printf("%*s%u Reported IfID(s)\n", indent, "", ct_lid);
		DisplaySeparator();
		break;
	case FORMAT_XML:
		if (detail && ct_lid)
			printf("%*s</IfIDs>\n", indent, "");
		XmlPrintDec("ReportedIfIDCount", ct_lid, indent);
		indent-=4;
		printf("%*s</IfIDSummary>\n", indent, "");
		break;
	default:
		break;
	}

}	// End of ShowAllIPReport()


// command line options, each has a short and long flag name
struct option options[] = {
		// basic controls
		{ "verbose", no_argument, NULL, 'v' },
		{ "quiet", no_argument, NULL, 'q' },
		{ "output", required_argument, NULL, 'o' },
		{ "detail", required_argument, NULL, 'd' },
		{ "noname", no_argument, NULL, 'N' },
		{ "limit", no_argument, NULL, 'L' },
		{ "config", required_argument, NULL, 'c' },
		{ "focus", required_argument, NULL, 'F' },
		{ "xml", no_argument, NULL, 'x' },
		{ "infile", required_argument, NULL, 'X' },
		{ "topology", required_argument, NULL, 'T' },
		{ "quietfocus", no_argument, NULL, 'Q' },
		{ "allports", no_argument, NULL, 'A' },
		{ "rc", required_argument, NULL, 'z' },
		{ "timeout", required_argument, NULL, '!' },
		{ "ethconfig", required_argument, NULL, 'E' },
		{ "plane", required_argument, NULL, 'p' },
		{ "help", no_argument, NULL, '$' },	// use an invalid option character

		{ 0 }
};

void Usage_full(void)
{
	fprintf(stderr, "Usage: ethreport [-v][-q] [-o report] [-d detail]\n"
	                "                    [-N] [-x] [-X snapshot_input] [-T topology_input]\n"
	                "                    [-A] [-c file] [-L] [-F point]\n"
	                "                    [-Q] [-E file] [-p plane]\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       ethreport --help\n");
	fprintf(stderr, "    --help - produce full help text\n");
	fprintf(stderr, "    -v/--verbose              - verbose output\n");
	fprintf(stderr, "    -q/--quiet                - disable progress reports\n");
	fprintf(stderr, "    --timeout                 - timeout(wait time for response) in ms, default is 1000ms\n");
	fprintf(stderr, "    -o/--output report        - report type for output\n");
	fprintf(stderr, "    -d/--detail level         - level of detail 0-n for output, default is 2\n");
	fprintf(stderr, "    -N/--noname               - omit node\n");
	fprintf(stderr, "    -x/--xml                  - output in xml\n");
	fprintf(stderr, "    -X/--infile snapshot_input\n");
	fprintf(stderr, "                              - generate report using data in snapshot_input\n");
	fprintf(stderr, "                                snapshot_input must have been generated via\n");
	fprintf(stderr, "                                previous -o snapshot run.\n");
	fprintf(stderr, "                                '-' may be used to specify stdin\n");
	fprintf(stderr, "    -T/--topology topology_input\n");
	fprintf(stderr, "                              - use topology_input file to augment and\n");
	fprintf(stderr, "                                verify fabric information.  When used various\n");
	fprintf(stderr, "                                reports can be augmented with information\n");
	fprintf(stderr, "                                not available electronically.\n");
	fprintf(stderr, "                                '-' may be used to specify stdin\n");
	fprintf(stderr, "    -A/--allports             - also get PortInfo for down switch ports.\n");
	fprintf(stderr, "    -c/--config file          - error thresholds config file, default is\n");
	fprintf(stderr, "                                %s\n", CONFIG_FILE);
	fprintf(stderr, "    -E/--ethconfig file       - Ethernet Mgt config file, default is\n");
	fprintf(stderr, "                                %s\n", HPN_CONFIG_FILE);
	fprintf(stderr, "    -p/--plane plane          - Name of the enabled plane defined in Mgt config file,\n");
	fprintf(stderr, "                                default is the first enabled plane\n");
	fprintf(stderr, "    -L/--limit                - For port error counters check (-o errors)\n");
	fprintf(stderr, "                                with -F limit operation to exact specified\n");
	fprintf(stderr, "                                focus.\n");
	fprintf(stderr, "                                Normally the neighbor of each selected\n");
	fprintf(stderr, "                                port would also be checked\n");
	fprintf(stderr, "                                Does not affect other reports\n");
	fprintf(stderr, "    -F/--focus point          - focus area for report\n");
	fprintf(stderr, "                                Limits output to reflect a subsection of\n");
	fprintf(stderr, "                                the fabric. May not work with all reports.\n");
	fprintf(stderr, "                                (For example, the verify*\n");
	fprintf(stderr, "                                reports may ignore the option or not generate\n");
	fprintf(stderr, "                                useful results.)\n");
	fprintf(stderr, "    -Q/--quietfocus           - do not include focus description in report\n");
	fprintf(stderr, "Report Types:\n");
	fprintf(stderr, "    comps                     - summary of all systems in fabric\n");
	fprintf(stderr, "    brcomps                   - brief summary of all systems in fabric\n");
	fprintf(stderr, "    nodes                     - summary of all node types in fabric\n");
	fprintf(stderr, "    brnodes                   - brief summary of all node types in fabric\n");
	fprintf(stderr, "    ifids                     - summary of all ifids in fabric\n");
	fprintf(stderr, "    linkinfo                  - summary of all links with ifids in fabric\n");
	fprintf(stderr, "    links                     - summary of all links\n");
	fprintf(stderr, "    extlinks                  - summary of links external to systems\n");
	fprintf(stderr, "    niclinks                   - summary of links to NICs\n");
	fprintf(stderr, "    islinks                   - summary of inter-switch links\n");
	fprintf(stderr, "    extislinks                - summary of inter-switch links external to systems\n");
	fprintf(stderr, "    slowlinks                 - summary of links running slower than expected\n");
	fprintf(stderr, "    slowconfiglinks           - summary of links configured to run slower than\n");
	fprintf(stderr, "                                supported includes slowlinks\n");
	fprintf(stderr, "    slowconnlinks             - summary of links connected with mismatched speed\n");
	fprintf(stderr, "                                potential includes slowconfiglinks\n");
	fprintf(stderr, "    misconfiglinks            - summary of links configured to run slower than\n");
	fprintf(stderr, "                                supported\n");
	fprintf(stderr, "    misconnlinks              - summary of links connected with mismatched speed\n");
	fprintf(stderr, "                                potential\n");
	fprintf(stderr, "    errors                    - summary of links whose errors exceed counts in\n");
    fprintf(stderr, "                                config file\n");
	fprintf(stderr, "    otherports                - summary of ports not connected to this fabric\n");
	fprintf(stderr, "    verifynics                 - compare fabric (or snapshot) NICs to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "    verifysws                 - compare fabric (or snapshot) Switches to\n");
	fprintf(stderr, "                                supplied topology and identify differences and\n");
	fprintf(stderr, "                                omissions\n");
	fprintf(stderr, "    verifynodes               - verifynics and verifysws reports\n");
	fprintf(stderr, "    verifylinks               - compare fabric (or snapshot) links to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "    verifyextlinks            - compare fabric (or snapshot) links to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "                                limit analysis to links external to systems\n");
	fprintf(stderr, "    verifyniclinks             - compare fabric (or snapshot) links to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "                                limit analysis to links to NICs\n");
	fprintf(stderr, "    verifyislinks             - compare fabric (or snapshot) links to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "                                limit analysis to inter-switch links\n");
	fprintf(stderr, "    verifyextislinks          - compare fabric (or snapshot) links to supplied\n");
	fprintf(stderr, "                                topology and identify differences and omissions\n");
	fprintf(stderr, "                                limit analysis to inter-switch links external to\n");
	fprintf(stderr, "                                systems\n");
	fprintf(stderr, "    verifyall                 - verifynics, verifysws, and verifylinks reports\n");
	fprintf(stderr, "    all                       - comp, nodes, links, extlinks,\n");
	fprintf(stderr, "                                slowconnlinks, and errors reports\n");
	fprintf(stderr, "    snapshot                  - output snapshot of fabric state for later use as\n");
	fprintf(stderr, "                                snapshot_input implies -x.  May not be combined\n");
	fprintf(stderr, "                                with other reports. When selected, -F, -N options\n");
	fprintf(stderr, "                                are ignored\n");
	fprintf(stderr, "    topology                  - output the topology of the fabric for later use\n");
	fprintf(stderr, "                                as topology_input implies -x.  May not be\n");
	fprintf(stderr, "                                combined with other reports.\n");
	fprintf(stderr, "                                Use with detail level 3 or more to get Port element\n");
	fprintf(stderr, "                                under Node in output xml\n");
	fprintf(stderr, "    fabricinfo                - fabric information\n");
	fprintf(stderr, "    none                      - no report\n");
	fprintf(stderr, "Point Syntax:\n");
	fprintf(stderr, "   ifid:value                 - value is numeric ifid\n");
	fprintf(stderr, "   ifid:value:node            - value is numeric ifid, selects node with given ifid\n");
	fprintf(stderr, "   ifid:value:port:value2     - value is numeric ifid of node, value2 is port #\n");
	fprintf(stderr, "   mgmtifaddr:value           - value is numeric port mgmtifaddr\n");
	fprintf(stderr, "   ifaddr:value               - value is numeric node ifaddr\n");
	fprintf(stderr, "   ifaddr:value1:port:value2\n");
    fprintf(stderr, "                              - value1 is numeric node ifaddr, value2 is port #\n");
	fprintf(stderr, "   chassisid:value            - value is numeric chassisid\n");
	fprintf(stderr, "   chassisid:value1:port:value2\n");
    fprintf(stderr, "                              - value1 is numeric chassisid value2 is port #\n");
	fprintf(stderr, "   node:value                 - value is node description (node name)\n");
	fprintf(stderr, "   node:value1:port:value2    - value1 is node description (node name) value2 is port #\n");
	fprintf(stderr, "   nodepat:value              - value is glob pattern for node description (node name)\n");
	fprintf(stderr, "   nodepat:value1:port:value2 - value1 is glob pattern for node description\n");
	fprintf(stderr, "                                (node name), value2 is port #\n");
	fprintf(stderr, "   nodedetpat:value           - value is glob pattern for node details\n");
	fprintf(stderr, "   nodedetpat:value1:port:value2\n");
    fprintf(stderr, "                              - value1 is glob pattern for node details,\n");
	fprintf(stderr, "                                value2 is port #\n");
	fprintf(stderr, "   nodetype:value             - value is node type (SW or NIC)\n");
	fprintf(stderr, "   nodetype:value1:port:value2\n");
    fprintf(stderr, "                              - value1 is node type (SW or NIC) value2 is port #\n");
	fprintf(stderr, "   rate:value                 - value is string for rate (25g, 50g, 75g, 100g, 150g, 200g)\n");
	fprintf(stderr, "                                omits switch mgmt port 0\n");
	fprintf(stderr, "   portstate:value            - value is string for state (up, down, testing, unknown,\n");
	fprintf(stderr, "                                dormant, notactive)\n");
	fprintf(stderr, "   portphysstate:value        - value is string for phys state (other, unknown,\n");
	fprintf(stderr, "                                operational, standby, shutdown, reset)\n");
	fprintf(stderr, "   mtucap:value               - value is MTU size (maximum size %d)\n", IB_UINT16_MAX);
	fprintf(stderr, "                                omits switch mgmt port 0\n");
	fprintf(stderr, "   linkdetpat:value           - value is glob pattern for link details\n");
	fprintf(stderr, "   portdetpat:value           - value is glob pattern for port details\n");
	fprintf(stderr, "   nodepatfile:FILENAME       - name of file with list of nodes\n");
	fprintf(stderr, "   nodepairpatfile:FILENAME   - name of file with list of node pairs separated by colon\n");
	fprintf(stderr, "   ldr                        - ports with a non-zero link down reason or neighbor\n");
	fprintf(stderr, "                                link down reason\n");
	fprintf(stderr, "   ldr:value                  - ports with a link down reason or neighbor link down\n");
	fprintf(stderr, "                                reason equal to value\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "   ethreport -o comps -d 3\n");
	fprintf(stderr, "   ethreport -o errors -o slowlinks\n");
	fprintf(stderr, "   ethreport -o nodes -F mgmtifaddr:0x00066a00a000447b\n");
	fprintf(stderr, "   ethreport -o nodes -F ifaddr:0x001175019800447b:port:1\n");
	fprintf(stderr, "   ethreport -o nodes -F ifaddr:0x001175019800447b\n");
	fprintf(stderr, "   ethreport -o nodes -F 'node:duster-eth2'\n");
	fprintf(stderr, "   ethreport -o nodes -F 'node:duster-eth2:port:1'\n");
	fprintf(stderr, "   ethreport -o nodes -F 'nodepat:d*'\n");
	fprintf(stderr, "   ethreport -o nodes -F 'nodepat:d*:port:1'\n");
	fprintf(stderr, "   ethreport -o nodes -F 'nodedetpat:compute*'\n");
	fprintf(stderr, "   ethreport -o nodes -F 'nodedetpat:compute*:port:1'\n");
	fprintf(stderr, "   ethreport -o nodes -F nodetype:NIC\n");
	fprintf(stderr, "   ethreport -o nodes -F nodetype:NIC:port:1\n");
	fprintf(stderr, "   ethreport -o nodes -F ifid:1\n");
	fprintf(stderr, "   ethreport -o nodes -F ifid:1:node\n");
	fprintf(stderr, "   ethreport -o nodes -F ifid:1:port:2\n");
	fprintf(stderr, "   ethreport -o nodes -F chassisid:0x001175019800447b\n");
	fprintf(stderr, "   ethreport -o nodes -F chassisid:0x001175019800447b:port:1\n");
	fprintf(stderr, "   ethreport -o extlinks -F rate:100g\n");
	fprintf(stderr, "   ethreport -o extlinks -F portstate:up\n");
	fprintf(stderr, "   ethreport -o extlinks -F portphysstate:operational\n");
	fprintf(stderr, "   ethreport -o extlinks -F 'portdetpat:*mgmt*'\n");
	fprintf(stderr, "   ethreport -o links -F mtucap:2048\n");
	fprintf(stderr, "   ethreport -o snapshot > file\n");
	fprintf(stderr, "   ethreport -o topology > topology.xml\n");
	fprintf(stderr, "   ethreport -o errors -X file\n");
	exit(0);
}

void Usage(void)
{
	fprintf(stderr, "Usage: ethreport [-v][-q] [-o report] [-d detail] [-x]\n");
	fprintf(stderr, "              or\n");
	fprintf(stderr, "       ethreport --help\n");
	fprintf(stderr, "    --help - produce full help text\n");
	fprintf(stderr, "    -v/--verbose              - verbose output\n");
	fprintf(stderr, "    -q/--quiet                - disable progress reports\n");
	fprintf(stderr, "    -o/--output report        - report type for output\n");
	fprintf(stderr, "    -d/--detail level         - level of detail 0-n for output, default is 2\n");
	fprintf(stderr, "    -x/--xml                  - output in xml\n");
	fprintf(stderr, "Report Types (abridged):\n");
	fprintf(stderr, "    comps                     - summary of all systems in fabric\n");
	fprintf(stderr, "    brcomps                   - brief summary of all systems in fabric\n");
	fprintf(stderr, "    nodes                     - summary of all node types in fabric\n");
	fprintf(stderr, "    brnodes                   - brief summary of all node types in fabric\n");
	fprintf(stderr, "    ifids                     - summary of all IfIDs in fabric\n");
	fprintf(stderr, "    links                     - summary of all links\n");
	fprintf(stderr, "    extlinks                  - summary of links external to systems\n");
	fprintf(stderr, "    slowlinks                 - summary of links running slower than expected\n");
	fprintf(stderr, "    slowconfiglinks           - summary of links configured to run slower than\n");
	fprintf(stderr, "                                supported includes slowlinks\n");
	fprintf(stderr, "    slowconnlinks             - summary of links connected with mismatched speed\n");
	fprintf(stderr, "                                potential includes slowconfiglinks\n");
	fprintf(stderr, "    misconfiglinks            - summary of links configured to run slower than\n");
	fprintf(stderr, "                                supported\n");
	fprintf(stderr, "    misconnlinks              - summary of links connected with mismatched speed\n");
	fprintf(stderr, "                                potential\n");
	fprintf(stderr, "    errors                    - summary of links whose errors exceed counts in\n");
	fprintf(stderr, "                                config file\n");
	fprintf(stderr, "    otherports                - summary of ports not connected to this fabric\n");
	fprintf(stderr, "    all                       - comp, nodes, links, extlinks,\n");
	fprintf(stderr, "                                slowconnlinks, and error reports\n");
	fprintf(stderr, "    fabricinfo                - fabric information\n");
	fprintf(stderr, "    none                      - no report\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "   ethreport -o comps -d 3\n");
	fprintf(stderr, "   ethreport -o errors -o slowlinks\n");
	exit(2);
}

int parse(const char* filename)
{
	FILE *fp = NULL;
	char param[80];
	unsigned long long threshold;
	int ret;
	char buffer[81];
	int skipping = 0;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr, "ethreport: Can't open %s: %s\n", filename, strerror(errno));
		return -1;
	}
	while (NULL != fgets(buffer, sizeof(buffer), fp))
	{
		// just ignore long lines
		if (buffer[strlen(buffer)-1] != '\n') {
			skipping=1;
			continue;
		}
		if (skipping) {
			skipping = 0;
			continue;
		}
		ret = sscanf(buffer, "%70s\n", param);
		if (ret != 1)
			continue;	// blank line
		if ( param[0]=='#')
			continue; // ignore comments
		if (strcmp(param, "Threshold") == 0) {
			char compare[21];
			ret = sscanf(buffer,"%70s %20s\n", param, compare);
			if (ret != 2) {
				fprintf(stderr, "ethreport: Invalid Config Line: %s, ignoring\n", buffer);
				continue;
			}
			if (strcmp(compare, "Greater") == 0) {
				g_threshold_compare = 0;
			} else if (strcmp(compare, "Equal") == 0) {
				g_threshold_compare = 1;
			} else {
				fprintf(stderr, "ethreport: Invalid Threshold: %s, ignoring\n", compare);
			}
			continue;
		}
		ret = sscanf(buffer, "%70s %llu\n", param, &threshold);
		if (ret == 2) {
			if (param[0]=='#') {
				// ignore comments
			} else if (strcmp(param,"NumLanesDown") == 0) {
				if (threshold > 4) {
					fprintf(stderr, "ethreport: NumLanesDown max threshold setting is 4, ignoring: %llu\n", threshold);
				} else {
					g_Thresholds.lq.s.numLanesDown = threshold;
				}
#define PARSE_THRESHOLD(field, sel, name, max) \
	if (strcmp(param, #name) == 0) { \
		if (threshold > (max)) { \
			fprintf(stderr, "ethreport: " #name " max threshold is %u, ignoring: %llu\n", (max), threshold); \
		} else { \
			g_Thresholds.field = threshold; \
		} \
	}
#define PARSE_THRESHOLD64(field, sel, name) \
	if (strcmp(param, #name) == 0) { \
		if (threshold >= UINT64_MAX) { \
			fprintf(stderr, "ethreport: " #name " max threshold is %llu, ignoring: %llu\n",(unsigned long long)UINT64_MAX-1, threshold); \
		} else { \
			g_Thresholds.field = threshold; \
		} \
	}
#define PARSE_MB_THRESHOLD(field, sel, name) \
	if (strcmp(param, #name) == 0) { \
		if (threshold > ((1ULL << 63)-1)/(FLITS_PER_MB/2)) { \
			fprintf(stderr, "ethreport: " #name " max threshold is %llu, ignoring: %llu\n", ((1ULL << 63)-1)/(FLITS_PER_MB/2), threshold); \
		} else { \
			threshold = threshold * FLITS_PER_MB; \
			g_Thresholds.field = threshold; \
		} \
	}
			// Data movement
			} else PARSE_MB_THRESHOLD(portXmitData, PortXmitData, IfHCOutOctets)
			else PARSE_MB_THRESHOLD(portRcvData, PortRcvData, IfHCInOctets)
			else PARSE_THRESHOLD64(portXmitPkts, PortXmitPkts, IfHCOutUcastPkts)
			else PARSE_THRESHOLD64(portRcvPkts, PortRcvPkts, IfHCInUcastPkts)
			else PARSE_THRESHOLD64(portMulticastXmitPkts, PortMulticastXmitPkts, IfHCOutMulticastPkts)
			else PARSE_THRESHOLD64(portMulticastRcvPkts, PortMulticastRcvPkts, IfHCInMulticastPkts)
			// Signal Integrity and Node/Link Stability
			else PARSE_THRESHOLD64(dot3HCStatsInternalMacTransmitErrors, Dot3HCStatsInternalMacTransmitErrors, Dot3HCStatsInternalMacTransmitErrors)
			else PARSE_THRESHOLD64(portRcvErrors, PortRcvErrors, Dot3HCStatsInternalMacReceiveErrors)
			else PARSE_THRESHOLD64(localLinkIntegrityErrors, LocalLinkIntegrityErrors, Dot3HCStatsSymbolErrors)
			// Packet Integrity
			else PARSE_THRESHOLD(ifOutErrors, IfOutErrors, IfOutErrors, UINT_MAX)
			else PARSE_THRESHOLD(ifInErrors, IfInErrors, IfInErrors, UINT_MAX)
			else PARSE_THRESHOLD(ifInUnknownProtos, IfInUnknownProtos, IfInUnknownProtos, UINT_MAX)
			else PARSE_THRESHOLD64(dot3HCStatsAlignmentErrors, Dot3HCStatsAlignmentErrors, Dot3HCStatsAlignmentErrors)
			else PARSE_THRESHOLD64(dot3HCStatsFCSErrors, Dot3HCStatsFCSErrors, Dot3HCStatsFCSErrors)
			else PARSE_THRESHOLD64(excessiveBufferOverruns, ExcessiveBufferOverruns, Dot3HCStatsFrameTooLongs)
			// Packet Discards
			else PARSE_THRESHOLD64(portXmitDiscards, PortXmitDiscards, IfOutDiscards)
			else PARSE_THRESHOLD64(portRcvFECN, PortRcvFECN, IfInDiscards)
			// Half-Duplex Detection
			else PARSE_THRESHOLD(dot3StatsCarrierSenseErrors, Dot3StatsCarrierSenseErrors, Dot3StatsCarrierSenseErrors, UINT_MAX)
			else PARSE_THRESHOLD(dot3StatsSingleCollisionFrames, Dot3StatsSingleCollisionFrames, Dot3StatsSingleCollisionFrames, UINT_MAX)
			else PARSE_THRESHOLD(dot3StatsMultipleCollisionFrames, Dot3StatsMultipleCollisionFrames, Dot3StatsMultipleCollisionFrames, UINT_MAX)
			else PARSE_THRESHOLD(dot3StatsSQETestErrors, Dot3StatsSQETestErrors, Dot3StatsSQETestErrors, UINT_MAX)
			else PARSE_THRESHOLD(dot3StatsDeferredTransmissions, Dot3StatsDeferredTransmissions, Dot3StatsDeferredTransmissions, UINT_MAX)
			else PARSE_THRESHOLD(dot3StatsLateCollisions, Dot3StatsLateCollisions, Dot3StatsLateCollisions, UINT_MAX)
			else PARSE_THRESHOLD(dot3StatsExcessiveCollisions, Dot3StatsExcessiveCollisions, Dot3StatsExcessiveCollisions, UINT_MAX)
#undef PARSE_THRESHOLD
#undef PARSE_MB_THRESHOLD
			else {
				fprintf(stderr, "ethreport: Invalid parameter: %s, ignoring\n", param);
			}
		} else {
			fprintf(stderr, "ethreport: Invalid Config Line: %s, ignoring\n", buffer);
		}
	}
	fclose(fp);
	return 0;
}

// convert a output type argument to the proper constant
report_t checkOutputType(const char* name)
{
	if (0 == strcmp(optarg, "comps")) {
		return REPORT_COMP;
	} else if (0 == strcmp(optarg, "brcomps")) {
		return REPORT_BRCOMP;
	} else if (0 == strcmp(optarg, "nodes")) {
		return REPORT_NODES;
	} else if (0 == strcmp(optarg, "brnodes")) {
		return REPORT_BRNODES;
	} else if (0 == strcmp(optarg, "linkinfo")) {
		return REPORT_LINKINFO;
	} else if (0 == strcmp(optarg, "links")) {
		return REPORT_LINKS;
	} else if (0 == strcmp(optarg, "extlinks")) {
		return REPORT_EXTLINKS;
	} else if (0 == strcmp(optarg, "niclinks")) {
		return REPORT_FILINKS;
	} else if (0 == strcmp(optarg, "islinks")) {
		return REPORT_ISLINKS;
	} else if (0 == strcmp(optarg, "extislinks")) {
		return REPORT_EXTISLINKS;
	} else if (0 == strcmp(optarg, "slowlinks")) {
		return REPORT_SLOWLINKS;
	} else if (0 == strcmp(optarg, "slowconfiglinks")) {
		return REPORT_SLOWCONFIGLINKS;
	} else if (0 == strcmp(optarg, "slowconnlinks")) {
		return REPORT_SLOWCONNLINKS;
	} else if (0 == strcmp(optarg, "misconfiglinks")) {
		return REPORT_MISCONFIGLINKS;
	} else if (0 == strcmp(optarg, "misconnlinks")) {
		return REPORT_MISCONNLINKS;
	} else if (0 == strcmp(optarg, "errors")) {
		return REPORT_ERRORS;
	} else if (0 == strcmp(optarg, "otherports")) {
		return REPORT_OTHERPORTS;
	} else if (0 == strcmp(optarg, "verifylinks")) {
		return REPORT_VERIFYLINKS;
	} else if (0 == strcmp(optarg, "verifyextlinks")) {
		return REPORT_VERIFYEXTLINKS;
	} else if (0 == strcmp(optarg, "verifyniclinks")) {
		return REPORT_VERIFYNICLINKS;
	} else if (0 == strcmp(optarg, "verifyislinks")) {
		return REPORT_VERIFYISLINKS;
	} else if (0 == strcmp(optarg, "verifyextislinks")) {
		return REPORT_VERIFYEXTISLINKS;
	} else if (0 == strcmp(optarg, "verifynodes")) {
		return REPORT_VERIFYNICS|REPORT_VERIFYSWS;
	} else if (0 == strcmp(optarg, "verifynics")) {
		return REPORT_VERIFYNICS;
	} else if (0 == strcmp(optarg, "verifysws")) {
		return REPORT_VERIFYSWS;
	} else if (0 == strcmp(optarg, "verifyall")) {
		/* verifylinks is a superset of verifyextlinks, verifyniclinks, */
		/* verifyislinks, verifyextislinks */
		return REPORT_VERIFYNICS|REPORT_VERIFYSWS|REPORT_VERIFYLINKS;
	} else if (0 == strcmp(optarg, "none")) {
		return REPORT_SKIP;
	} else if (0 == strcmp(optarg, "sizes")) {
		return REPORT_SIZES;
	} else if (0 == strcmp(optarg, "snapshot")) {
		return REPORT_SNAPSHOT;
	} else if (0 == strcmp(optarg, "ifids")) {
		return REPORT_LIDS;
	} else if (0 == strcmp(optarg, "topology")) {
		return REPORT_TOPOLOGY;
	} else if (0 == strcmp(optarg, "all")) {
		/* note we omit brcomp and brnodes since comp and nodes is superset */
		/* similarly links is a suprset of filinks, islinks, extislinks */
		/* omit snapshot */
		return REPORT_COMP|REPORT_NODES|REPORT_LINKS
				|REPORT_EXTLINKS|REPORT_SLOWCONNLINKS|REPORT_ERRORS;
	} else if (0 == strcmp(optarg, "fabricinfo")) {
		return REPORT_FABRICINFO;
	} else {
		fprintf(stderr, "ethreport: Invalid Output Type: %s\n", name);
		Usage();
		// NOTREACHED
		return 0;
	}
}

int main(int argc, char ** argv)
{
	FSTATUS             fstatus;
	int                 c;
//    uint8               hfi         = 0;
//    uint8               port        = 0;
//	boolean				gothfi=FALSE, gotport=FALSE;
	Format_t			format = FORMAT_TEXT;
        int				detail = 2;
	int				index;
	report_t			report = REPORT_NONE;
	char *config_file = CONFIG_FILE;
	char *focus_arg = NULL;
	Point				focus;
	uint32 temp;
	FabricFlags_t		sweepFlags = FF_NONE;
	uint8				find_flag = FIND_FLAG_FABRIC;	// always check fabric
	boolean				has_mgt_conf;

	Top_setcmdname("ethreport");
	PointInit(&focus);
	g_quiet = ! isatty(2);	// disable progress if stderr is not tty
	memset(&g_mgt_conf_params, 0, sizeof(g_mgt_conf_params));
	QListInitState(&g_mgt_conf_params.fabric_confs);
	if (! QListInit(&g_mgt_conf_params.fabric_confs)) {
		fprintf(stderr, "ethreport: Unable to initialize List\n");
		goto done;
	}

	// process command line arguments
	while (-1 != (c = getopt_long(argc,argv, "vVAqo:d:NLc:F:xX:T:E:p:Q", options, &index)))
	{
                switch (c)
		{
			case '$':
				Usage_full();
				// NOTREACHED
				break;
                        case 'v':
				g_verbose++;
				//if (g_verbose > 3) umad_debug(g_verbose-2);
				break;
			case 'q':
				g_quiet = 1;
				break;
			case '!':
				if (FSUCCESS != StringToInt32(&g_ms_timeout, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "ethreport: Invalid timeout value: %s\n", optarg);
					Usage();
				}
				break;
			case 'o':	// select output record desired
				report = (report_t) report | checkOutputType(optarg);
				if (report & (REPORT_VERIFYNICS|REPORT_VERIFYSWS))
					find_flag |= FIND_FLAG_ENODE;
				if (report & (REPORT_VERIFYLINKS|REPORT_VERIFYEXTLINKS
							  |REPORT_VERIFYNICLINKS|REPORT_VERIFYISLINKS
							  |REPORT_VERIFYEXTISLINKS))
					find_flag |= FIND_FLAG_ELINK;
				break;
			case 'd':	// detail level
				if (FSUCCESS != StringToUint32(&temp, optarg, NULL, 0, TRUE)) {
					fprintf(stderr, "ethreport: Invalid Detail Level: %s\n", optarg);
					Usage();
					// NOTREACHED
				}
				detail = (int)temp;
				break;
			case 'N':	// omit names
				g_noname = 1;
				break;
			case 'A':	// get PortInfo for all switch ports, including down ones
				sweepFlags |= FF_DOWNPORTINFO;
				break;
			case 'L':	// limit to specific ports
				g_limitstats = 1;
				break;
			case 'c':	// config file for thresholds in errors report
				config_file = optarg;
				break;
			case 'F':	// focus for report
				if (focus_arg) {
					fprintf(stderr, "ethreport: -F option may only be specified once\n");
					Usage();
					// NOTREACHED
				}
				focus_arg = optarg;
				break;
			case 'x':	// output in xml
				format = FORMAT_XML;
				break;
			case 'X':	// snapshot_input in xml
				g_snapshot_in_file = optarg;
				break;
			case 'T':	// topology_input in xml
				g_topology_in_file = optarg;
				break;
			case 'Q':	// do not include focus description in report
				g_quietfocus = 1;
				break;
			case 'E':	// ethernet config file
				g_hpnConfigFile = optarg;
				break;
			case 'p':	// fabric plane
				// in our code a fabric is actually a fabric plane
				snprintf(g_fabricId, HMGT_SHORT_STRING_SIZE, "%s", optarg);
				break;
			default:
				fprintf(stderr, "ethreport: Invalid option -%c\n", c);
				Usage();
				// NOTREACHED
				break;
		}
	} /* end while */

	if (optind < argc)
	{
		Usage();
		// NOTREACHED
	}

	// check for incompatible reports
	if (report & REPORT_TOPOLOGY) {
		if((report != REPORT_TOPOLOGY)){
			fprintf(stderr, "ethreport: -o topology cannot be run with other reports\n");
			Usage();
			// NOTREACHED
		} else {
			format = FORMAT_XML;
		}
	}
	if ((report & REPORT_SNAPSHOT) && (report != REPORT_SNAPSHOT)) {
		fprintf(stderr, "ethreport: -o snapshot cannot be run with other reports\n");
		Usage();
		// NOTREACHED
	}
	has_mgt_conf = hmgt_parse_config_file(g_hpnConfigFile, g_quiet, g_verbose, &g_mgt_conf_params) == FSUCCESS;
	if (!has_mgt_conf && !g_snapshot_in_file) {
		fprintf(stderr, "ethreport: Must provide a valid configuration file: %s\n", g_hpnConfigFile);
		Usage();
	}

	// check for incompatible arguments
	if ((report & REPORT_LINKINFO) && (format == FORMAT_XML)) {
		fprintf(stderr, "ethreport: -o linkinfo option does not support XML output\n");
		Usage();
		// NOTREACHED
	}
	if ((report & REPORT_FABRICINFO) && (format == FORMAT_XML)) {
		fprintf(stderr, "ethreport: -o fabricinfo option does not support XML output\n");
		Usage();
		// NOTREACHED
	}

	// Warn for extraneous arguments and ignore them
	if (focus_arg) {
		char *name = "report";
		int suppress = 0;
		if (report & REPORT_SNAPSHOT) { suppress = 1; name = "snapshot"; }
		if (report & REPORT_SKIP) { suppress = 1; name = "none"; }
		if (report & REPORT_FABRICINFO) { suppress = 1; name = "fabricinfo"; }

		if (suppress) {
			fprintf(stderr,"ethreport: %s does not support -F option.\n", name);
			fprintf(stderr,"           -F ignored for all reports.\n");
			focus_arg = NULL;
		}
	}

	if (report == REPORT_SNAPSHOT && g_limitstats) {
		fprintf(stderr, "ethreport: -L ignored for -o snapshot\n");
		g_limitstats = 0;
	}

	if ((report & REPORT_FABRICINFO) && (g_limitstats || g_noname || (sweepFlags & FF_DOWNPORTINFO))) {
		fprintf(stderr, "ethreport: -L, -N, -A ignored for -o fabricinfo\n");
		g_noname = 0;
	}

	if (g_limitstats && ! focus_arg) {
		fprintf(stderr, "ethreport: -L ignored when -F not specified\n");
		g_limitstats = 0;
	}

	if ((report & REPORT_SNAPSHOT) && g_noname) {
		fprintf(stderr, "ethreport: -N ignored for -o snapshot\n");
		g_noname = 0;
	}

	if (g_fabricId[0] && strchr(g_fabricId, ' ')) {
		fprintf(stderr, "ethreport: -p option only supports one plane\n");
		Usage();
	}

	if (g_snapshot_in_file) {
		if (sweepFlags & FF_DOWNPORTINFO)
			fprintf(stderr, "ethreport: -A ignored for -X\n");
		sweepFlags &= ~(FF_DOWNPORTINFO);
	}
	if (g_limitstats && ! (report & (REPORT_ERRORS|REPORT_SNAPSHOT))) {
		fprintf(stderr, "ethreport: -L ignored without -o errors nor -o snapshot\n");
		g_limitstats = 0;
	}

	if (report == REPORT_NONE)
		report = REPORT_BRNODES;

	// Initialize Sweep Verbose option, for -X still used for Focus processing
	fstatus = InitSweepVerbose(g_verbose?stderr:NULL);
	if (fstatus != FSUCCESS) {
		fprintf(stderr, "ethreport: Initialize Verbose option (status=0x%x): %s\n", fstatus, iba_fstatus_msg(fstatus));
		g_exitstatus = 1;
		goto done;
	}

	if (g_verbose) {
		setTopologySnmpVerbose(stderr, g_verbose);
	}

	if (g_quiet) {
		setTopologySnmpQuiet(g_quiet);
	}

	// get thresholds config file
	if (report & REPORT_ERRORS) {
		if (0 != parse(config_file)) {
			g_exitstatus = 1;
			goto done;
		}
	}

	// get the fabric snapshot data and set fabric plane based on the snapshot
	if (g_snapshot_in_file) {
		if (FSUCCESS != Xml2ParseSnapshot(g_snapshot_in_file, g_quiet, &g_Fabric, FF_NONE, 0)) {
			g_exitstatus = 1;
			goto done;
		}
		if (g_fabricId[0])
			fprintf(stderr, "ethreport: -p ignored for -X\n");
		snprintf(g_fabricId, HMGT_SHORT_STRING_SIZE, "%s", g_Fabric.name);
	}

	// check plane name defined in topology file and use it if no -X
	if (g_topology_in_file && strcmp(g_topology_in_file, "-")) {
		char buff[HMGT_SHORT_STRING_SIZE] = "";
		if (FSUCCESS != Xml2GetTopologyPlane(g_topology_in_file, g_quiet, buff)) {
			g_exitstatus = 1;
			goto done;
		}
		if (!g_snapshot_in_file) {
			if (g_fabricId[0])
                		fprintf(stderr, "ethreport: -p ignored for -T\n");
			snprintf(g_fabricId, HMGT_SHORT_STRING_SIZE, "%s", buff);
		} else if (strncmp(g_fabricId, buff, HMGT_SHORT_STRING_SIZE)) {
			// print out warning message. and continue use this topology file.
			fprintf(stderr, "WARNING - mismatched plane name found in topology file: expect %s, found %s\n",
			                g_fabricId, buff);
		}
	}

	// get plane config if conf file was parsed successfully
	fabric_config_t *port_conf = NULL;
	if (has_mgt_conf) {
		QUICK_LIST *fabs = &g_mgt_conf_params.fabric_confs;
		LIST_ITEM *lip;

		for (lip = QListHead(fabs); lip != NULL; lip = QListNext(fabs, lip)) {
			fabric_config_t *tmp_fab_conf = QListObj(lip);
			if (!g_fabricId[0] || !strncmp(tmp_fab_conf->name, g_fabricId, HMGT_SHORT_STRING_SIZE)) {
				port_conf = tmp_fab_conf;
				if (!g_fabricId[0])
					snprintf(g_fabricId, HMGT_SHORT_STRING_SIZE, "%s", tmp_fab_conf->name);
				break;
			}
		}

		if (port_conf && !g_quiet) {
			ProgressPrint(TRUE, "Use Plane '%s'...", port_conf->name);
		}
	}

	// get live fabric data that requires port_conf
	if (!g_snapshot_in_file) {
		if (!port_conf) {
			if (g_fabricId[0])
				fprintf(stderr, "ethreport: Couldn't find plane '%s' in config file %s\n", g_fabricId, g_hpnConfigFile);
			else /* shouldn't happen */
				fprintf(stderr, "ethreport: No plane defined in config file %s\n", g_hpnConfigFile);
			g_exitstatus = 1;
			goto done;
		}
		if (FSUCCESS != Sweep(g_portGuid, &g_Fabric, sweepFlags, SWEEP_ALL, g_quiet, g_ms_timeout, port_conf)) {
			g_exitstatus = 1;
			goto done;
		}
	}

	// if topology file not specified yet, use what defined in conf file
	if (!g_topology_in_file && port_conf && port_conf->topology_file[0]) {
		if (access(port_conf->topology_file, F_OK)) {
			if (port_conf->directory[0] && port_conf->topology_file[0] != '/') {
				if (strlen(port_conf->directory) == 0 ||
					port_conf->directory[strlen(port_conf->directory)-1] == '/')
					snprintf(confTopFile, sizeof(confTopFile), "%s%s", port_conf->directory,
					         port_conf->topology_file);
				else
					snprintf(confTopFile, sizeof(confTopFile), "%s/%s", port_conf->directory,
					         port_conf->topology_file);
				if (!access(confTopFile, F_OK))
					g_topology_in_file = confTopFile;
			}
			if (!g_topology_in_file)
				fprintf(stderr, "WARNING - Couldn't find topology file %s\n", port_conf->topology_file);
		} else {
			g_topology_in_file = port_conf->topology_file;
		}
		if (!g_quiet && g_topology_in_file)
			ProgressPrint(TRUE, "Use Topology file '%s' defined in conf file...", g_topology_in_file);
	}
	// parse topology input file and cross reference to fabric data
	if (g_topology_in_file) {
		if (FSUCCESS != Xml2ParseTopology(g_topology_in_file, g_quiet, &g_Fabric, TOPOVAL_NONE)) {
			g_exitstatus = 1;
			goto done_fabric;
		}
		//if (g_verbose)
		//	Xml2PrintTopology(stdout, &g_Fabric);	// for debug
	}

	// we can't do a linkqual focus until after the port counters have been collected
	// nor can we do route focus with -m until FDBs are collected. So will handle route focus with and without -m later.
	if (focus_arg) {
		char *p;
		FSTATUS status;

		status = ParseFocusPoint(g_snapshot_in_file?0:g_portGuid,
						&g_Fabric, focus_arg, &focus, find_flag, &p, TRUE);
		if (FINVALID_PARAMETER == status || (FSUCCESS == status && *p != '\0')) {
			fprintf(stderr, "ethreport: Invalid Point Syntax: '%s'\n", focus_arg);
			fprintf(stderr, "ethreport:                        %*s^\n", (int)(p-focus_arg), "");
			PointDestroy(&focus);
			Usage_full();
			// NOTREACHED
		}
		if (FSUCCESS != status) {
			fprintf(stderr, "ethreport: Unable to resolve Point: '%s': %s\n", focus_arg, iba_fstatus_msg(status));
			g_exitstatus = 1;
			goto done_fabric;
		}
	}

	// get other optional fabric data
	// now that the port counters have been collected, we can do the link quality focus
	if (format == FORMAT_XML && ! (report & REPORT_SNAPSHOT)) {
		// TBD - use IXml functions for XML output
		char datestr[80] = "";
		int i;

		printf("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");
		Top_formattime(datestr, sizeof(datestr), g_Fabric.time);
		printf("<Report plane=\"%s\" date=\"%s\" unixtime=\"%ld\" options=\"", g_Fabric.name, datestr, g_Fabric.time);
		for (i=1; i<argc; i++)
			printf("%s%s", i>1?" ":"", argv[i]);
		printf("\">\n");
	}
	if (report & REPORT_COMP)
		ShowComponentReport(&focus, format, 0, detail);
	if (report & REPORT_BRCOMP)
		ShowComponentBriefReport(&focus, format, 0, detail);
	if (report & REPORT_NODES)
		ShowNodeTypeReport(&focus, format, 0, detail);
	if (report & REPORT_BRNODES)
		ShowNodeTypeBriefReport(&focus, format, REPORT_BRNODES, 0, detail);
	if (report & REPORT_LINKS)
		ShowLinksReport(&focus, REPORT_LINKS, format, 0, detail);
	if (report & REPORT_EXTLINKS)
		ShowLinksReport(&focus, REPORT_EXTLINKS, format, 0, detail);
	if (report & REPORT_FILINKS)
		ShowLinksReport(&focus, REPORT_FILINKS, format, 0, detail);
	if (report & REPORT_ISLINKS)
		ShowLinksReport(&focus, REPORT_ISLINKS, format, 0, detail);
	if (report & REPORT_EXTISLINKS)
		ShowLinksReport(&focus, REPORT_EXTISLINKS, format, 0, detail);
	if (report & REPORT_SLOWLINKS)
		ShowSlowLinkReport(LINK_EXPECTED_REPORT, FALSE, &focus, format, 0, detail);
	if (report & REPORT_SLOWCONFIGLINKS)
		ShowSlowLinkReport(LINK_CONFIG_REPORT, FALSE, &focus, format, 0, detail);
	if (report & REPORT_SLOWCONNLINKS)
		ShowSlowLinkReport(LINK_CONN_REPORT, FALSE, &focus, format, 0, detail);
	if (report & REPORT_MISCONFIGLINKS)
		ShowSlowLinkReport(LINK_CONFIG_REPORT, TRUE, &focus, format, 0, detail);
	if (report & REPORT_MISCONNLINKS)
		ShowSlowLinkReport(LINK_CONN_REPORT, TRUE, &focus, format, 0, detail);
	if (report & REPORT_ERRORS)
		ShowLinkErrorReport(&focus, format, 0, detail);
	if (report & REPORT_OTHERPORTS)
		ShowOtherPortsReport(&focus, format, 0, detail);
	if (report & REPORT_VERIFYNICS)
		ShowVerifyNodesReport(&focus, STL_NODE_FI, format, 0, detail);
	if (report & REPORT_VERIFYSWS)
		ShowVerifyNodesReport(&focus, STL_NODE_SW, format, 0, detail);
	if (report & REPORT_VERIFYLINKS)
		ShowVerifyLinksReport(&focus, REPORT_VERIFYLINKS, format, 0, detail);
	if (report & REPORT_VERIFYEXTLINKS)
		ShowVerifyLinksReport(&focus, REPORT_VERIFYEXTLINKS, format, 0, detail);
	if (report & REPORT_VERIFYNICLINKS)
		ShowVerifyLinksReport(&focus, REPORT_VERIFYNICLINKS, format, 0, detail);
	if (report & REPORT_VERIFYISLINKS)
		ShowVerifyLinksReport(&focus, REPORT_VERIFYISLINKS, format, 0, detail);
	if (report & REPORT_VERIFYEXTISLINKS)
		ShowVerifyLinksReport(&focus, REPORT_VERIFYEXTISLINKS, format, 0, detail);
	if (report & REPORT_TOPOLOGY) {
		ShowNodeTypeBriefReport(&focus, format, REPORT_TOPOLOGY, 0, detail);
		ShowLinksReport(&focus, REPORT_LINKS, format, 0, detail);
	}
	if (report & REPORT_SIZES)
		ShowSizesReport();
	if (report == REPORT_SNAPSHOT) {
		SnapshotOutputInfo_t info;

		info.fabricp = &g_Fabric;
		info.argc = argc;
		info.argv = argv;

		Xml2PrintSnapshot(stdout, &info);
	}

	if (report & REPORT_LIDS)
		ShowAllIPReport(&focus, format, 0, detail);

	if (report & REPORT_LINKINFO)
		ShowLinkInfoReport(&focus, format, 0, detail);

	if (report & REPORT_FABRICINFO)
		ShowFabricinfoReport(0, detail);

	if (format == FORMAT_XML && ! (report & REPORT_SNAPSHOT)) {
		printf("</Report>\n");
	}

done_fabric:
	DestroyFabricData(&g_Fabric);
done:
	PointDestroy(&focus);
	if (g_exitstatus == 2) {
		Usage();
		// NOTREACHED
	}

	return g_exitstatus;
}
