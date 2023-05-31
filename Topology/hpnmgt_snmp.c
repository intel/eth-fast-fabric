/* BEGIN_ICS_COPYRIGHT7 ****************************************

Copyright (c) 2020-2021, Intel Corporation

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
#include <stdarg.h>
#include "iba/stl_sa_types.h"
#include "iba/stl_sd.h"
#include "hpnmgt_snmp_priv.h"
#include "hpnmgt_snmp.h"
#include "port_num_gen.h"

// turn on DISPLAY_TIMESTAMP to trace time spent on tasks
#define DISPLAY_TIMESTAMP 0
static int SNMP_INITED = 0;

#define SNMPTAG MAKE_MEM_TAG('S','n', 'm', 'p')

#define ENTITY_TYPE_MODULE 9
#define ENTITY_TYPE_CHASSIS 3
#define ADDR_SUBTYPE_IPV4 1
#define PORTID_SUBTYPE_MAC 3
#define SNMP_BULK_SIZE 10
#define SNMP_RESULT_BLOCK 1024

typedef struct {
	STL_NODE_RECORD *node;
	int ifIndex; /* the mgmt interface index. used for host. */
	NodeData *nodeData; /* a pointer to the NodeData in a fabric data */
	QUICK_LIST *ports; /* a list of STL_PORTINFO_RECORD */
	cl_qmap_t portIdMap; /* a map between port Id and portData */
} RAW_NODE;

boolean TRACE = FALSE;
uint8 verbose_level = 0;
FILE *verbose_file = NULL;	// file for verbose output

#define DBGPRINT(format, args...) if (verbose_file) { fprintf(verbose_file, format, ##args); }
#define TRACEPRINT(format, args...) if (TRACE) { fprintf(verbose_file?verbose_file:stderr, format, ##args); }
#define PRINT_NOSUCHOBJECT(oid, host) \
	fprintf(stderr, "No OID: %s available on %s\n", oid, host);
#define PRINT_NOSUCHINSTANCE(oid, host) \
	fprintf(stderr, "No Such Instance currently exists on %s at OID: %s\n", host, oid);

void setTopologySnmpVerbose(FILE* file, uint8 level) {
	verbose_file = file;
	verbose_level = level;
	if (! TRACE) {
		TRACE = verbose_file && verbose_level>=4;
	}
}

//--------- utility functions ------------------//

void print_timestamp(FILE *out) {
	struct timeval curTime;
	struct timezone timeZone;
	struct tm *tm;

	gettimeofday(&curTime, &timeZone);
	tm = localtime(&curTime.tv_sec);
	if (tm) {
		fprintf(out, "%.2d:%.2d:%.2d.%.6d - ", tm->tm_hour, tm->tm_min, tm->tm_sec,
				(int) curTime.tv_usec);
	}
}

// used for performance test
static inline void time_print(const char *format, ...) {
	if (DISPLAY_TIMESTAMP) {
		print_timestamp(verbose_file?verbose_file:stderr);
		va_list args;
		va_start(args, format);
		vfprintf(verbose_file?verbose_file:stderr, format, args);
		va_end(args);
	}
}

#define ETH_IF_SPEED_100G			100000	// in Mbps
#define ETH_IF_SPEED_200G			200000  // in Mbps
#define ETH_IF_SPEED_400G   		300000  // in Mbps
#define ETH_IF_SPEED_RANGE_FACTOR	25000	// in bits

#define VALID_IFSPEED_LINKSPEED(ifspeed, linkspeed) \
	(ifspeed <= (linkspeed + ETH_IF_SPEED_RANGE_FACTOR)) && \
	(ifspeed >= (linkspeed - ETH_IF_SPEED_RANGE_FACTOR))

#define COUNTER64_TO_UINT64(pCounter) \
	((uint64)pCounter->high << 32) | pCounter->low

/*
 * @brief internal helper function that converts the ifSpeed object Mbps value
 *        to a generic LinkSpeed mask value to be used by ETH applications
 */
static __inline uint32
ifspeed_to_active_linkspeed(uint32 ifspeed)
{
	if ( VALID_IFSPEED_LINKSPEED(ifspeed, ETH_IF_SPEED_400G) ) {
		 return ETH_LINK_SPEED_400G;
	} else if ( VALID_IFSPEED_LINKSPEED(ifspeed, ETH_IF_SPEED_200G) ) {
		return ETH_LINK_SPEED_200G;
	} else if ( VALID_IFSPEED_LINKSPEED(ifspeed, ETH_IF_SPEED_100G) ) {
		return ETH_LINK_SPEED_100G;
	} else if (ifspeed) {
		return ETH_LINK_SPEED_LT_100G;
	}
	return 0;
}


/*
 * @brief Check whether oid2 matches oid1, i.e. oid2 is the same as oid1 or
 *        oid2 starts with oid1
 * @param oild1	the oid that is used as the comparison reference
 * @param oid1Len	length of the first oid
 * @param oid2	the oid that we would like to check
 * @param oid2Len	length of the second oid
 * @return TRUE if oid2 is the same as oid1, or starts with oid1
 */
boolean match_oid(oid *oid1, size_t oid1Len, oid *oid2, size_t oid2Len) {
	if (oid2Len < oid1Len) {
		return FALSE;
	}

	return snmp_oid_compare(oid1, oid1Len, oid2, oid1Len) == 0;
}

/*
 * @brief check if the SNMP data come from the specified OID
 * @param res	the SNMP data to check
 * @param oid	the OID to check
 * @return TRUE if the SNMP data matches the specified OID
 */
boolean is_oid(const SNMPResult* res, const SNMPOid* oid) {
	if (res->oidLen < oid->oidLen) {
		return FALSE;
	}

	return snmp_oid_compare(oid->oid, oid->oidLen, res->oid, oid->oidLen) == 0;
}

/*
 * @brief get the value of an oid segment at specified offset from SNMPResult
 * @param res 	The SNMPResult where we want to get oid segment value
 * @param offset	the offset
 * @return int	the value of desired oid segment
 */
int get_oid_num(SNMPResult* res, int offset) {
	if (offset > res->oidLen) {
		fprintf(stderr,
				"ERROR - OID index overflow. OID len is %ld, fetch offset=%d.\n",
				res->oidLen, offset);
		return -1;
	}

	int val = *(res->oid + offset);
	return val;
}

/*
 * @brief transfer MauTypeListBits to uint32 that represents 100G+ speed
 * @param bytes	byte array of MauTypeListBits
 * @param len	length of the byte array
 * @return uint32 of the speeds that is >= 100Gb
 */
uint16 get_link_speed_supported(u_char* bytes, size_t len) {
	int i = 0;
	int j = 0;
	uint16 base = 0;
	uint8  mask = 0;
	uint16 speed = 0;
	uint16 res = 0;

	for (i = 0; i < len; i++, base += 8) {
		if (!bytes[i]) {
			continue;
		}
		mask = 0x80;
		for (j = 0; j < 8; j++) {
			if (mask & bytes[i]) {
				speed = EthLinkModeToSpeed(base + j);
				if (speed != ETH_LINK_SPEED_NOP) {
					res = res | speed;
				}
			}
			mask = mask >> 1;
		}
	}
	return res;
}

boolean is_all_zeros(u_char* bytes, size_t len) {
	int i = 0;
	for(; i<len; i++) {
		if (bytes[i]) {
			return FALSE;
		}
	}
	return TRUE;
}

/*
 * @brief transfer IPV4 address to LID from an oid stored in SNMPResult
 * @param res	the SNMPResult that contains oid
 * @param offset	the IPv4 address starts position in oid
 * @return STL_LID
 */
STL_LID get_lid_from_oid(SNMPResult* res, int offset) {
	STL_LID lid = 0;
	int i;
	for (i = 0; i < 4; i++) {
		lid = (lid << 8) + get_oid_num(res, offset + i);
	}
	// fabric data only supports up to 24 bits
	lid = lid & 0xffffff;
	return lid;
}

/*
 * @brief figure out node type based on Capability byte array. See table 9-4 in
 *        8021AB-2005.pdf for capability details
 * @param cap the capability byte array
 * @param len	length of the capability byte array
 * @return node type, either STL_NODE_SW or STL_NODE_FI
 */
uint8 get_node_type(u_char* cap, size_t len) {
	// bridge (0x20) or router (0x08)
	if (len >= 1 && ((cap[0] & 0x20) == 0x20 || (cap[0] & 0x08) == 0x08)) {
		return STL_NODE_SW;
	} else {
		return STL_NODE_FI;
	}
}

/*
 * @brief transfer capability byte array to a uint32 integer number
 * @param cap the capability byte array
 * @param len	length of the capability byte array
 * @return a uint32 number that represent the capabilities
 */
uint32 get_capability(u_char* cap, size_t len) {
	uint32 res = 0;
	int i = 0;
	for (; i < len; i++) {
		res = (res << 8) + cap[len - i - 1];
	}
	return res;
}

/*
 * @brief Calculate hash value for a string using djb2 algorithm.
 * @param str	the string to be evaluated
 * @param len	string length
 * @return hash value
 */
uint64 get_hash(u_char* str, size_t len) {
	uint64 res = 5381;
	int i = 0;
	for (; str[i] && i < len; i++) {
		res = ((res << 5) + res) + str[i];
	}
	return res;
}

char get_hex(char c) {
	if (c >= '0' && c <='9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

/*
 * @brief transfer a byte array to GUID. Only first 8 elements will be used.
 * @param bytes	the byte array
 * @param len	length of the byte array
 * @return a uint64 GUID value
 */
uint64 get_guid(u_char* bytes, size_t len) {
	uint64 res = 0;

	boolean hex = FALSE;
	int i = 0;
	for (; i < len; i++) {
		if (!isprint(bytes[i]) && !isspace(bytes[i])) {
			hex = TRUE;
			break;
		}
	}

	if (hex) {
		for (i = 0; i < len && i < 8; i++) {
			res = (res << 8) + bytes[i];
		}
	} else {
		u_char* pos = bytes;
		if (len > 2 && bytes[0] == '0' && (bytes[1] == 'x' || bytes[1] == 'X')) {
			// skip "0x" or "0X"
			pos += 2;
		}
		// try to be aggresive to tolerant string in any possible format
		for (i = 0; i < 16 && (pos - bytes) < len && *pos; pos++) {
			char val = get_hex(*pos);
			if (val != -1) {
				res = (res<<4) + val;
				i++;
			}
		}
	}
	return res;
}

/*
 * @brief copy string value in SNMPResult to specified destination
 * @param res	the SNMP query result
 * @param dest	the destination to copy the string
 * @param maxLen	the maximum length allowed at destination
 * @return void
 */
void copy_snmp_string(SNMPResult* res, char* dest, size_t maxLen, boolean hex_only) {
	if (res->type != ASN_OCTET_STR) {
		fprintf(stderr, "ERROR - SNMP value is not string.\n");
		return;
	}
	// figure out whether the value is bit string or not
	boolean hex = FALSE;
	int i = 0;
	u_char *sPos = res->val.string;
	for (; i < res->valLen; i++, sPos++) {
		if (!isprint(*sPos) && !isspace(*sPos)) {
			hex = TRUE;
			break;
		}
	}
	if (hex) {
		sPos = res->val.string;
		size_t len = 0;
		for (i=0; i < res->valLen && (len + 2) < maxLen; i++, sPos++) {
			snprintf(dest + len, 3, "%02x", *sPos);
			len += 2;
		}
	} else if (hex_only) {
		sPos = res->val.string;
		if (res->valLen > 2 && sPos[0] == '0' && (sPos[1] == 'x' || sPos[1] == 'X')) {
			// skip "0x" or "0X"
			sPos += 2;
		}
		for (i=0; i < maxLen-1 && (sPos - res->val.string) < res->valLen && *sPos; sPos++) {
			if (isxdigit(*sPos)) {
				dest[i] = *sPos;
				i++;
			}
		}
		dest[i] = '\0';
	} else {
		size_t len = res->valLen + 1 < maxLen ? res->valLen + 1 : maxLen;
		snprintf(dest, len, "%s", res->val.string);
	}
}

boolean is_supported_interface(SNMPHost *host, char* ifName, size_t len) {
	if (host->numInterface == 0) {
		return TRUE;
	}

	int i = 0;
	for (;i < host->numInterface; i++) {
		if (strncmp(host->interfaces[i], ifName, len) == 0 && host->interfaces[i][len] == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * @brief get next SNMPResult. If unavailable, create SNMP_RESULT_BLOCK linked SNMPResult
 *        and return the first one.
 */
SNMPResult * get_next_snmp_result(SNMPResult *res) {
	if (res && res->next) {
		return res->next;
	}

	SNMPResult* tmp = MemoryAllocate2AndClear(sizeof(SNMPResult) * SNMP_RESULT_BLOCK, IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
	if (tmp == NULL) {
		fprintf(stderr, "ERROR - couldn't allocate memory for SNMPResult.\n");
		return NULL;
	}
	TRACEPRINT("Allocated SNMPResult BLOCK\n");

	int i;
	SNMPResult *rp = res ? res : tmp;
	for (i = 0; i < SNMP_RESULT_BLOCK; i++) {
		tmp[i].freeable = i == 0;
		rp->next = &tmp[i];
		rp = rp->next;
	}
	return tmp;
}

/*
 * @brief Trace the SNMPResult chain and free each element
 * @param res	the SNMPResult to free
 * @return void
 */
void free_snmp_result(SNMPResult *res) {
	if (res) {
		// just pick one of the union, so the compiler will not complain
		if (res->val.string && (u_char *)res->val.string != res->data)
			MemoryDeallocate(res->val.string);
		if (res->next) {
			free_snmp_result(res->next);
		}
		if (res->freeable) {
			MemoryDeallocate(res);
		}
	}
}

/*
 * @brief Make a SNMPResult object from the content in variable_list. And then
 *        either fill in or attach to the provided current SNMPResult based on
 *        indicator, fillFirst. This function also returns this created object.
 * @param res	current SNMPResult
 * @param varLst	the variable_list that contains snmp query result
 * @param fillFirst	indicates whether fill in res or create a new one and then
 *                 	link to it
 * @return the created SNMPResult
 */
SNMPResult * add_snmp_result(SNMPResult *res, struct variable_list *vars,
		boolean fillFirst) {
	SNMPResult *newRes = NULL;
	char buf[1024];
	if (fillFirst) {
		newRes = res;
	} else {
		newRes = get_next_snmp_result(res);
	}
	if (!newRes) {
		fprintf(stderr, "ERROR - no SNMPResult\n");
		return NULL;
	}

	ASSERT(vars->name_length <= FF_MAX_OID_LEN);
	int size = sizeof(oid) * MIN(vars->name_length, FF_MAX_OID_LEN);
	memcpy(newRes->oid, vars->name, size);
	newRes->oidLen = vars->name_length;
	newRes->type = vars->type;
	void * val = NULL;
	if (vars->val_len <= FF_SNMP_VAL_LEN) {
		val = newRes->data;
	} else {
		val = MemoryAllocate2AndClear(vars->val_len, IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
		if (!val) {
			fprintf(stderr, "ERROR - couldn't allocate memory.\n");
			if (!fillFirst) {
				free_snmp_result(newRes);
			}
			return NULL;
		}
		TRACEPRINT("Allocated SNMP VAL len=%ld\n", vars->val_len);
	}
	memcpy(val, vars->val.string, vars->val_len);
	// just pick one of the union to do the cast, so the compiler will not complain
	newRes->val = (netsnmp_vardata) (u_char *) val;

	newRes->valLen = vars->val_len;

	snprint_objid(buf, sizeof(buf), newRes->oid, newRes->oidLen);
	TRACEPRINT("Added %s\n", buf);
	return newRes;
}

//-------- End of utility functions -------------//


//--------- Data Processing Functions --------------//

cl_map_obj_t *create_map_obj(const void *obj) {
	cl_map_obj_t *res = MemoryAllocate2AndClear(sizeof(cl_map_obj_t), IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
	if (res == NULL) {
		fprintf(stderr, "ERROR - cannot allocate memory for map obj\n");
		return NULL;
	}
	res->p_object = obj;
	return res;
}

/*
 * @brief fetch the object wrapped in cl_map_obj_t from a map with the provided
 *        key
 */
void *get_map_obj(cl_qmap_t *map, uint64 key) {
	cl_map_item_t *mItem = cl_qmap_get(map, key);
	if (mItem == cl_qmap_end(map)) {
		return NULL;
	}
	return cl_qmap_obj(PARENT_STRUCT(mItem, cl_map_obj_t, item));
}

/*
 * @brief internal helper function that fetches the node record obj that was
 *        stored in RAW_NODE, and then wrapped in LIST_ITEM for QUICK_LIST and
 *        then cl_map_obj_t for quick map.
 */
STL_NODE_RECORD *_get_node_rec(cl_qmap_t *map, uint64 key) {
	LIST_ITEM *item = get_map_obj(map, key);
	if (item) {
		RAW_NODE *rawNode = item->pObject;
		if (rawNode) {
			return rawNode->node;
		}
	}
	return NULL;
}

/**
 * @brief trace each element in a quick map and free the holder of the element,
 *        i.e. cl_map_obj_t. This is an internal function used for free resources
 */
void _free_map_item(const cl_map_item_t *mItem, const cl_map_item_t *end) {
	if (mItem != end) {
		cl_map_item_t *next = cl_qmap_next(mItem);
		_free_map_item(next, end);
		cl_map_obj_t *parent = PARENT_STRUCT(mItem, cl_map_obj_t, item);
		if (parent) {
			MemoryDeallocate(parent);
		}
	}
}

void cleanup_map(cl_qmap_t *map) {
	const cl_map_item_t *head = cl_qmap_head(map);
	const cl_map_item_t *end = cl_qmap_end(map);
	_free_map_item(head, end);
	cl_qmap_remove_all(map);
}

void _free_list_item(QUICK_LIST *list, LIST_ITEM *item) {
	if (item == NULL) {
		return;
	}

	LIST_ITEM *next = QListNext(list, item);
	_free_list_item(list, next);
	if (item->pObject) {
		MemoryDeallocate(item->pObject);
	}
	MemoryDeallocate(item);
}

void free_list(QUICK_LIST *list) {
	if (list == NULL) {
		return;
	}

	LIST_ITEM *portItem = QListHead(list);
	_free_list_item(list, portItem);
	QListDeallocate(list);
}

/*
 * @brief free node and port records. Call this when we do not need them,
 *        e.g. after we have add them into fabric data.
 * @param nodeList	the nodeList we want to partially free
 */
void free_records(QUICK_LIST *nodeList) {
	if (nodeList == NULL) {
		return;
	}

	LIST_ITEM *nodeItem;
	RAW_NODE *node;
	for (nodeItem = QListHead(nodeList); nodeItem != NULL;
			nodeItem = QListNext(nodeList, nodeItem)) {
		node = nodeItem->pObject;
		if (node == NULL) {
			continue;
		}
		if (node->node) {
			MemoryDeallocate(node->node);
			node->node = NULL;
		}

		if (node->ports) {
			free_list(node->ports);
			node->ports = NULL;
		}
	}
}

/*
 * @brief totally free a node list
 * @param nodeList	the node list to free
 */
void free_node_list(QUICK_LIST *nodeList) {
	if (nodeList == NULL) {
		return;
	}

	LIST_ITEM *nodeItem;
	RAW_NODE *node;
	for (nodeItem = QListHead(nodeList); nodeItem != NULL;
			nodeItem = QListNext(nodeList, nodeItem)) {
		node = nodeItem->pObject;
		if (node == NULL) {
			continue;
		}

		if (node->node) {
			MemoryDeallocate(node->node);
			node->node = NULL;
		}

		if (node->ports) {
			free_list(node->ports);
			node->ports = NULL;
		}

		cleanup_map(&node->portIdMap);
	}
	free_list(nodeList);
}

LIST_ITEM *create_node_item() {
	STL_NODE_RECORD *node = MemoryAllocate2AndClear(sizeof(STL_NODE_RECORD), IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
	if (node == NULL) {
		fprintf(stderr, "ERROR - couldn't allocate memory for Node record!\n");
		return NULL;
	}

	RAW_NODE *rawNode = MemoryAllocate2AndClear(sizeof(RAW_NODE), IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
	if (rawNode == NULL) {
		fprintf(stderr, "ERROR - couldn't allocate memory for Raw Node!\n");
		MemoryDeallocate(node);
		return NULL;
	}
	rawNode->node = node;

	LIST_ITEM *item = MemoryAllocate2AndClear(sizeof(LIST_ITEM), IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
	if (item == NULL) {
		fprintf(stderr, "ERROR - couldn't allocate memory for list item!\n");
		MemoryDeallocate(node);
		MemoryDeallocate(rawNode);
		return NULL;
	}
	item->pObject = rawNode;

	return item;
}

void free_node_item(LIST_ITEM *item) {
	if (!item) {
		return;
	}

	RAW_NODE *rawNode = item->pObject;
	if (rawNode) {
		STL_NODE_RECORD *node = rawNode->node;
		if (node) {
			MemoryDeallocate(node);
		}
		MemoryDeallocate(rawNode);
	}
	MemoryDeallocate(item);
}

STL_NODE_RECORD *get_node_record(LIST_ITEM **array, size_t size, int portNum) {
	if (portNum >= size) {
		return NULL;
	}

	LIST_ITEM *item = array[portNum];
	if (item && item->pObject) {
		RAW_NODE *rawNode = item->pObject;
		return rawNode->node;
	}
	return NULL;
}

LIST_ITEM *create_port_item() {
	STL_PORTINFO_RECORD *port = MemoryAllocate2AndClear(sizeof(STL_PORTINFO_RECORD), IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
	if (port == NULL) {
		fprintf(stderr, "ERROR - couldn't allocate memory for Port record!\n");
		return NULL;
	}

	LIST_ITEM *item = MemoryAllocate2AndClear(sizeof(LIST_ITEM), IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
	if (item == NULL) {
		fprintf(stderr, "ERROR - couldn't allocate memory for list item!\n");
		MemoryDeallocate(port);
		return NULL;
	}
	item->pObject = port;

	return item;
}

STL_PORT_COUNTERS_DATA *get_port_counters(cl_qmap_t *map, SNMPResult *res,
		const SNMPOid* oid) {
	int id = get_oid_num(res, oid->oidLen);
	PortData *pData = get_map_obj(map, id);
	if (pData) {
		return pData->pPortCounters;
	}
	return NULL;
}

HMGT_STATUS_T populate_switch_node_record(SNMPHost *host, SNMPResult *res,
		QUICK_LIST *nodeList) {
	LIST_ITEM *item = create_node_item();
	if (item == NULL) {
		fprintf(stderr, "ERROR - couldn't create a list item for node!\n");
		return HMGT_STATUS_INSUFFICIENT_MEMORY;
	}
	QListInsertTail(nodeList, item);
	RAW_NODE *rawNode = item->pObject;
	STL_NODE_RECORD *node = rawNode->node;
	node->NodeInfo.NodeType = STL_NODE_SW;

	HMGT_STATUS_T fstatus = HMGT_STATUS_SUCCESS;
	char buf[1024];
	SNMPResult *rp = res;
	int i = 0;
	boolean manAddrProcessed = FALSE;
	int modulePhyId = -1;
	while (rp && rp->oidLen) {
		if (TRACE) {
			snprint_objid(buf, sizeof(buf), rp->oid, rp->oidLen);
			TRACEPRINT("Processing %s type=%d str=", buf, rp->type);
			for (i=0; i < rp->valLen; i++) {
				TRACEPRINT("%02X ", rp->val.string[i]);
			}
			TRACEPRINT("\n");
		}

		if (is_oid(rp, &lldpLocChassisId)) {
			TRACEPRINT("..lldpLocChassisId\n");
			uint64 guid = get_guid(rp->val.string, rp->valLen);
			node->NodeInfo.SystemImageGUID = guid;
			//TODO: extend to support director switch
			node->NodeInfo.NodeGUID = guid;
		} else if (is_oid(rp, &lldpLocManAddrIfId)) {
			TRACEPRINT("..lldpLocManAddrIfId\n");
			if (manAddrProcessed) {
				// only pick the first entry
				goto next_loop;
			}
			// oid format is lldpLocManAddrIfId + subType + len + IP
			if (rp->oidLen >= lldpLocManAddrIfId.oidLen + 1 + 1 + 4) {
				int subtype = get_oid_num(rp, lldpLocManAddrIfId.oidLen);
				int ipLen = get_oid_num(rp, lldpLocManAddrIfId.oidLen + 1);
				if (subtype == ADDR_SUBTYPE_IPV4 && ipLen == 4) {
					node->RID.LID = get_lid_from_oid(rp,
							lldpLocManAddrIfId.oidLen + 2);
					node->NodeInfo.u1.s.LocalPortNum =
							(uint16) *(rp->val.integer);
					manAddrProcessed = TRUE;
					TRACEPRINT("....LID=0x%x LocalPortNum=%d\n", node->RID.LID,
							node->NodeInfo.u1.s.LocalPortNum);
				} else {
					fprintf(stderr,
							"ERROR - unsupported ManAddrSubtype. expected=4, actual=%d!\n",
							subtype);
					fstatus = HMGT_STATUS_ERROR;
					// break because we have no LID. Doesn't make sense to continue;
					break;
				}
			} else {
				// shouldn't happen
				fprintf(stderr,
						"ERROR - invalid OID. Expected length=18, actual=%ld!\n",
						rp->oidLen);
				fstatus = HMGT_STATUS_ERROR;
				// break because we have no LID. Doesn't make sense to continue;
				break;
			}
		} else if (is_oid(rp, &sysObjectID)) {
			TRACEPRINT("..sysObjectID\n");
			node->NodeInfo.u1.s.VendorID = (uint32) *(rp->val.objid + 6);
		} else if (is_oid(rp, &sysName)) {
			TRACEPRINT("..sysName\n");
			size_t len =
					rp->valLen + 1 < STL_NODE_DESCRIPTION_ARRAY_SIZE ?
							rp->valLen + 1 : STL_NODE_DESCRIPTION_ARRAY_SIZE;
			snprintf((char*) node->NodeDesc.NodeString, len, "%s",
					rp->val.string);
		} else if (is_oid(rp, &ifNumber)) {
			TRACEPRINT("..ifNumber\n");
			node->NodeInfo.NumPorts = (uint16) *(rp->val.integer);
		} else if (is_oid(rp, &ifPhysAddress)) {
			TRACEPRINT("..ifPhysAddress\n");
			int ifId = get_oid_num(rp, ifPhysAddress.oidLen);
			if (ifId == node->NodeInfo.u1.s.LocalPortNum) {
				uint64 guid = get_guid(rp->val.string, rp->valLen);
				node->NodeInfo.PortGUID = guid;
			}
		} else if (is_oid(rp, &entPhysicalClass)) {
			TRACEPRINT("..entPhysicalClass\n");
			// Note: other ent data processing depends on modulePhyId, we shall
			// ensure we query entPhysicalClass before other ent queries.
			if (modulePhyId == -1) {
				if (*(rp->val.integer) == ENTITY_TYPE_MODULE ||
						*(rp->val.integer) == ENTITY_TYPE_CHASSIS) {
					modulePhyId = get_oid_num(rp, entPhysicalClass.oidLen);
				}
				TRACEPRINT("....modulePhyId=%d\n", modulePhyId);
			}
		} else if (is_oid(rp, &entPhysicalDescr)) {
			TRACEPRINT("..entPhysicalDescr\n");
			int phyId = get_oid_num(rp, entPhysicalDescr.oidLen);
			if (phyId == modulePhyId) {
				copy_snmp_string(rp, (char*) node->NodeInfo.DeviceName,
						SMALL_STR_ARRAY_SIZE, FALSE);
			}
		} else if (is_oid(rp, &entPhysicalHardwareRev)) {
			TRACEPRINT("..entPhysicalHardwareRev\n");
			int phyId = get_oid_num(rp, entPhysicalHardwareRev.oidLen);
			if (phyId == modulePhyId) {
				copy_snmp_string(rp, (char*) node->NodeInfo.HardwareRev,
						SMALL_STR_ARRAY_SIZE, FALSE);
			}
		} else if (is_oid(rp, &entPhysicalFirmwareRev)) {
			TRACEPRINT("..entPhysicalFirmwareRev\n");
			int phyId = get_oid_num(rp, entPhysicalFirmwareRev.oidLen);
			if (phyId == modulePhyId) {
				copy_snmp_string(rp, (char*) node->NodeInfo.FirmwareRev,
						SMALL_STR_ARRAY_SIZE, FALSE);
			}
		} else if (is_oid(rp, &entPhysicalSerialNum)) {
			TRACEPRINT("..entPhysicalSerialNum\n");
			int phyId = get_oid_num(rp, entPhysicalSerialNum.oidLen);
			if (phyId == modulePhyId) {
				copy_snmp_string(rp, (char*) node->NodeInfo.SerialNum,
						SMALL_STR_ARRAY_SIZE, FALSE);
			}
		} else if (is_oid(rp, &entPhysicalMfgName)) {
			TRACEPRINT("..entPhysicalMfgName\n");
			int phyId = get_oid_num(rp, entPhysicalMfgName.oidLen);
			if (phyId == modulePhyId) {
				copy_snmp_string(rp, (char*) node->NodeInfo.MfgName,
						SMALL_STR_ARRAY_SIZE, FALSE);
			}
		} else if (is_oid(rp, &entPhysicalModelName)) {
			TRACEPRINT("..entPhysicalModelName\n");
			int phyId = get_oid_num(rp, entPhysicalModelName.oidLen);
			if (phyId == modulePhyId) {
				copy_snmp_string(rp, (char*) node->NodeInfo.PartNum,
						SMALL_STR_ARRAY_SIZE, FALSE);
			}
		}
next_loop:
		rp = rp->next;
	}

	return fstatus;
}

HMGT_STATUS_T populate_host_node_record(SNMPHost *host, SNMPResult *res,
		QUICK_LIST *nodeList) {
	HMGT_STATUS_T fstatus = HMGT_STATUS_SUCCESS;
	SNMPResult *rp = res;
	char buf[1024];
	uint64 systemImgGuid = 0;
	int i = 0;
	SNMPResult *sysNameRes = NULL;
	cl_qmap_t nodeMap;
	cl_qmap_init(&nodeMap, NULL);
	while (rp && rp->oidLen) {
		if (TRACE) {
			snprint_objid(buf, sizeof(buf), rp->oid, rp->oidLen);
			TRACEPRINT("Processing %s type=%d str=", buf, rp->type);
			for (i=0; i < rp->valLen; i++) {
				TRACEPRINT("%02X ", rp->val.string[i]);
			}
			TRACEPRINT("\n");
		}

		if (is_oid(rp, &sysName)) {
			TRACEPRINT("..sysName\n");
			sysNameRes = rp;
		} else if (is_oid(rp, &ifName)) {
			TRACEPRINT("..ifName\n");
			if (!is_supported_interface(host, (char*)rp->val.string, rp->valLen)) {
				goto next_loop;
			}
			int portId = get_oid_num(rp, ifName.oidLen);
			LIST_ITEM *item = create_node_item();
			if (item) {
				RAW_NODE *rawNode = item->pObject;
				STL_NODE_RECORD *node = rawNode->node;
				node->NodeInfo.NodeType = STL_NODE_FI;
				// TODO: support NIC with multiple ports
				node->NodeInfo.NumPorts = 1;
				node->NodeInfo.u1.s.LocalPortNum = 1;
				node->NodeInfo.SystemImageGUID = systemImgGuid;
				rawNode->ifIndex = portId;
				QListInsertTail(nodeList, item);
				cl_map_obj_t *mapObj = create_map_obj(item);
				if (mapObj) {
					cl_qmap_insert(&nodeMap, portId, &(mapObj->item));
				} else {
					fprintf(stderr, "ERROR - failed to create map obj.\n");
					fstatus = HMGT_STATUS_INSUFFICIENT_MEMORY;
					break;
				}

				// rp->val.string doesn't guarantee it's null terminated
				size_t len = 0;
				if (sysNameRes) {
					len =
							sysNameRes->valLen + 1 < STL_NODE_DESCRIPTION_ARRAY_SIZE ?
									sysNameRes->valLen + 1 : STL_NODE_DESCRIPTION_ARRAY_SIZE;
					if (len + rp->valLen + 1 > STL_NODE_DESCRIPTION_ARRAY_SIZE) {
						len = STL_NODE_DESCRIPTION_ARRAY_SIZE - rp->valLen - 1;
						if (len < 5) {
							len = 5;
						}
					}
					snprintf((char*) node->NodeDesc.NodeString, len, "%s",
							sysNameRes->val.string);
				} else {
					len = snprintf((char*) node->NodeDesc.NodeString, 8, "%s",
							"Unknown");
				}
				char *pos = (char*) (node->NodeDesc.NodeString) + len - 1;
				len = rp->valLen + 2 < STL_NODE_DESCRIPTION_ARRAY_SIZE - len + 1 ?
						rp->valLen + 2 : STL_NODE_DESCRIPTION_ARRAY_SIZE - len + 1;
				size_t delta = rp->valLen + 2 - len;
				snprintf(pos, len, "-%s", rp->val.string + delta);
			}
		} else if (is_oid(rp, &ifType)) {
			TRACEPRINT("..ifType\n");
			uint8 type = (uint8) *(rp->val.integer);
			// ignore software loopback
			if (type == ETH_PORT_TYPE_24) {
				int portId = get_oid_num(rp, ifType.oidLen);
				cl_map_item_t *mItem = cl_qmap_remove(&nodeMap, portId);
				if (mItem != cl_qmap_end(&nodeMap)) {
					cl_map_obj_t *mapObj = PARENT_STRUCT(mItem, cl_map_obj_t, item);
					LIST_ITEM *lItem = (LIST_ITEM *) mapObj->p_object;
					QListRemoveItem(nodeList, lItem);
					free_node_item(lItem);
					MemoryDeallocate(mapObj);
				}
			}
		} else if (is_oid(rp, &ifPhysAddress)) {
			TRACEPRINT("..ifPhysAddress\n");
			int portNum = get_oid_num(rp, ifPhysAddress.oidLen);
			STL_NODE_RECORD *node = _get_node_rec(&nodeMap, portNum);
			if (node) {
				if (systemImgGuid == 0) {
					// use the first physical card's mac addr as systemImgGuid
					// same logic as lldpd when no specify -C
					systemImgGuid = get_guid(rp->val.string, rp->valLen);
				}
				uint64 guid = get_guid(rp->val.string, rp->valLen);
				node->NodeInfo.NodeGUID = guid;
				node->NodeInfo.PortGUID = guid;
				node->NodeInfo.SystemImageGUID = systemImgGuid;
			}
		} else if (is_oid(rp, &ipAdEntIfIndex)) {
			TRACEPRINT("..ipAdEntIfIndex\n");
			int portNum = *(rp->val.integer);
			STL_NODE_RECORD *node = _get_node_rec(&nodeMap, portNum);
			if (node) {
				node->RID.LID = get_lid_from_oid(rp, ipAdEntIfIndex.oidLen);
			}
		}
		// TODO: net-snmp agentx doesn't provide entity mib data. We may need
		// to customize it to provide related info so we can fill in other
		// fields in node record
next_loop:
		rp = rp->next;
	}
	cleanup_map(&nodeMap);
	return fstatus;
}

/*
 * @brief create port records for each switch node and put port list back to RAW_NODE
 */
HMGT_STATUS_T populate_switch_node_port_records(SNMPResult *res,
		QUICK_LIST *nodeList, int downportinfo) {
	HMGT_STATUS_T fstatus = HMGT_STATUS_SUCCESS;

	if (nodeList == NULL) {
		return HMGT_STATUS_INVALID_PARAMETER;
	}
	// shall only have one item
	LIST_ITEM *head = QListHead(nodeList);
	if (!head) {
		// shouldn't happen
		return HMGT_STATUS_UNAVAILABLE;
	}
	RAW_NODE *rawNode = head->pObject;
	if (rawNode == NULL) {
		// shouldn't happen
		return HMGT_STATUS_UNAVAILABLE;
	}

	STL_NODE_RECORD *nodeRec = rawNode->node;
	if (nodeRec == NULL) {
		// shouldn't happen
		return HMGT_STATUS_UNAVAILABLE;
	}
	rawNode->ports = QListAllocateAndInit(FALSE, SNMPTAG);
	if (rawNode->ports == NULL) {
		fprintf(stderr, "ERROR - Failed to allocate memory for Port List!\n");
		return HMGT_STATUS_INSUFFICIENT_MEMORY;
	}

	SNMPResult *rp = res;
	char buf[1024];
	int i = 0;
	boolean manAddrProcessed = FALSE;
	cl_qmap_t portNumMap;
	cl_qmap_init(&portNumMap, NULL);
	cl_qmap_t portIdMap;
	cl_qmap_init(&portIdMap, NULL);

	pn_gen_t png_model;
	pn_gen_init(&png_model);

	// switch port zero
	LIST_ITEM *item = create_port_item();
	if (item == NULL) {
		// create_port_item only print out err msg
		return HMGT_STATUS_INSUFFICIENT_MEMORY;
	}
	QListInsertTail(rawNode->ports, item);

	STL_PORTINFO_RECORD *portZeroRec = item->pObject;
	portZeroRec->RID.EndPortLID = nodeRec->RID.LID;
	portZeroRec->PortInfo.PortStates.s.PortState = ETH_PORT_UP;


	while (rp && rp->oidLen) {
		if (TRACE) {
			snprint_objid(buf, sizeof(buf), rp->oid, rp->oidLen);
			TRACEPRINT("Processing %s type=%d str=", buf, rp->type);
			for (i=0; i < rp->valLen; i++) {
				TRACEPRINT("%02X ", rp->val.string[i]);
			}
			TRACEPRINT("\n");
		}

		if (is_oid(rp, &lldpLocSysCapSupported)) {
			TRACEPRINT("..lldpLocSysCapSupported\n");
			portZeroRec->PortInfo.CapabilityMask.AsReg32 = get_capability(
					rp->val.string, rp->valLen);
		} else if (is_oid(rp, &lldpLocSysCapEnabled)) {
			TRACEPRINT("..lldpLocSysCapEnabled\n");
			portZeroRec->PortInfo.CapabilityMask3.AsReg16 = get_capability(
					rp->val.string, rp->valLen);
		} else if (is_oid(rp, &lldpLocManAddrIfId)) {
			TRACEPRINT("..lldpLocManAddrIfId\n");
			// only pick the first one
			if (manAddrProcessed) {
				goto next_loop;
			}

			portZeroRec->PortInfo.LID = (uint32) *(rp->val.integer);
			portZeroRec->PortInfo.LocalPortNum = (uint32) *(rp->val.integer);
			// oid format is lldpLocManAddrIfId + subType + len + IP
			if (rp->oidLen >= lldpLocManAddrIfId.oidLen + 1 + 1 + 4) {
				int subtype = get_oid_num(rp, lldpLocManAddrIfId.oidLen);
				int ipLen = get_oid_num(rp, lldpLocManAddrIfId.oidLen + 1);
				if (subtype == ADDR_SUBTYPE_IPV4 && ipLen == 4) {
					for (i = 0; i < 4; i++) {
						portZeroRec->PortInfo.IPAddrIPV4.addr[i] = get_oid_num(
								rp, lldpLocManAddrIfId.oidLen + 2 + i);
					}
				} else {
					fprintf(stderr,
							"ERROR - unsupported ManAddrSubtype. expected=4, actual=%d!\n",
							subtype);
					fstatus = HMGT_STATUS_ERROR;
				}
			} else {
				// shouldn't happen
				fprintf(stderr,
						"ERROR - invalid OID. Expected length=18, actual=%ld!\n",
						rp->oidLen);
				fstatus = HMGT_STATUS_ERROR;
			}
			manAddrProcessed = TRUE;
		} else if (is_oid(rp, &lldpRemChassisId)) {
			TRACEPRINT("..lldpRemChassisId\n");
			LIST_ITEM *item = create_port_item();
			if (item == NULL) {
				// create_port_item already print out err msg
				goto next_loop;
			}
			QListInsertTail(rawNode->ports, item);

			STL_PORTINFO_RECORD *portRec = item->pObject;
			portRec->RID.EndPortLID = nodeRec->RID.LID;
			int portNum = get_oid_num(rp, lldpRemChassisId.oidLen + 1);
			//portRec->RID.PortNum = portNum;
			uint64 guid = get_guid(rp->val.string, rp->valLen);
			portRec->PortInfo.NeighborNodeGUID = guid;
			portRec->PortInfo.CapabilityMask.AsReg32 =
					portZeroRec->PortInfo.CapabilityMask.AsReg32;
			portRec->PortInfo.CapabilityMask3.AsReg16 =
					portZeroRec->PortInfo.CapabilityMask3.AsReg16;

			cl_map_obj_t *mapObj = create_map_obj(item);
			if (mapObj) {
				cl_qmap_insert(&portNumMap, portNum, &(mapObj->item));
			} else {
				fprintf(stderr, "ERROR - failed to create map obj.\n");
				fstatus = HMGT_STATUS_INSUFFICIENT_MEMORY;
				break;
			}
		} else if (is_oid(rp, &lldpRemPortIdSubtype)) {
			TRACEPRINT("..lldpRemPortIdSubtype\n");
			int portNum = get_oid_num(rp, lldpRemPortIdSubtype.oidLen + 1);
			LIST_ITEM *lItem = get_map_obj(&portNumMap, portNum);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				portRec->PortInfo.NeighborPortIdSubtype = (uint8) *(rp->val.integer);
			}
		} else if (is_oid(rp, &lldpRemPortId)) {
			TRACEPRINT("..lldpRemPortId\n");
			int portNum = get_oid_num(rp, lldpRemPortId.oidLen + 1);
			LIST_ITEM *lItem = get_map_obj(&portNumMap, portNum);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				copy_snmp_string(rp, (char*) portRec->PortInfo.NeighborPortId,
						TINY_STR_ARRAY_SIZE,
						portRec->PortInfo.NeighborPortIdSubtype == PORTID_SUBTYPE_MAC);
			}
		} else if (is_oid(rp, &lldpRemSysCapEnabled)) {
			TRACEPRINT("..lldpRemSysCapEnabled\n");
			int portNum = get_oid_num(rp, lldpRemSysCapEnabled.oidLen + 1);
			LIST_ITEM *lItem = get_map_obj(&portNumMap, portNum);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				uint8 type = get_node_type(rp->val.string, rp->valLen);
				if (type == STL_NODE_SW) {
					portRec->PortInfo.PortNeighborMode.NeighborNodeType =
							STL_NEIGH_NODE_TYPE_SW;
				} else if (type == STL_NODE_FI) {
					portRec->PortInfo.PortNeighborMode.NeighborNodeType =
							STL_NEIGH_NODE_TYPE_HFI;
				}
			}
		} else if (is_oid(rp, &lldpLocPortIdSubtype)) {
			TRACEPRINT("..lldpLocPortIdSubtype\n");
			STL_PORTINFO_RECORD *portRec = NULL;
			int portNum = get_oid_num(rp, lldpLocPortIdSubtype.oidLen);
			uint8 type = *(rp->val.integer);
			LIST_ITEM *lItem = get_map_obj(&portNumMap, portNum);
			if (lItem) {
				portRec = lItem->pObject;
				if (type != 5 && type != 7) {
				// we expect subtype 5 (interface name) or 7 (local) for switch ports
					fprintf(stderr,
							"ERROR - LLDP Port ID is not interface name.\n");
					// cannot continue, have to break
					break;
				}
			} else if (downportinfo) {
				// inactive ports
				LIST_ITEM *item = create_port_item();
				if (item == NULL) {
					// create_port_item already print out err msg
					goto next_loop;
				}
				QListInsertTail(rawNode->ports, item);
				portRec = item->pObject;
				portRec->RID.EndPortLID = nodeRec->RID.LID;
				portRec->PortInfo.CapabilityMask.AsReg32 =
						portZeroRec->PortInfo.CapabilityMask.AsReg32;
				portRec->PortInfo.CapabilityMask3.AsReg16 =
						portZeroRec->PortInfo.CapabilityMask3.AsReg16;

				cl_map_obj_t *mapObj = create_map_obj(item);
				if (mapObj) {
					cl_qmap_insert(&portNumMap, portNum, &(mapObj->item));
				} else {
					fprintf(stderr, "ERROR - failed to create map obj.\n");
					fstatus = HMGT_STATUS_INSUFFICIENT_MEMORY;
					break;
				}
			}
			if (portRec) {
				portRec->PortInfo.LocalPortIdSubtype = type;
			}
		} else if (is_oid(rp, &lldpLocPortId)) {
			TRACEPRINT("..lldpLocPortId\n");
			int portNum = get_oid_num(rp, lldpLocPortId.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portNumMap, portNum);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				copy_snmp_string(rp, (char*) portRec->PortInfo.LocalPortId,
						TINY_STR_ARRAY_SIZE, FALSE);
			}
		} else if (is_oid(rp, &ifName)) {
			TRACEPRINT("..ifName\n");
			// transfer from port number map to interface index map
			char* portName = (char*) rp->val.string;
			portName[rp->valLen] = 0;
			pn_gen_register(&png_model, portName);

			cl_map_item_t *mItem = cl_qmap_head(&portNumMap);
			cl_map_obj_t *mapObj = NULL;
			LIST_ITEM *lItem = NULL;
			STL_PORTINFO_RECORD *portRec = NULL;
			for (; mItem != cl_qmap_end(&portNumMap);
					mItem = cl_qmap_next(mItem)) {
				mapObj = PARENT_STRUCT(mItem, cl_map_obj_t, item);
				lItem = (LIST_ITEM *) mapObj->p_object;
				portRec = lItem->pObject;
				// when lldpLocPortIdSubtype is 5 (interfaceName) or 7 (local),
				// the lldpLocPortId shall match ifName. Please note that some
				// switches have mismatched names. E.g. switch hdlaexa3421 has
				// lldpLocPortId='9'and the corresponding ifName is "1:9". This
				// is a vendor issue. Please see below for another example and
				// related discussion
				// https://knowledge.broadcom.com/external/article?articleId=15995
				// https://github.com/netdisco/snmp-info/issues/140
				// Maybe in the future we allow user provided regex to do
				// advanced match
				if (strncmp((char*) portRec->PortInfo.LocalPortId, portName, rp->valLen)) {
					portRec = NULL;
				} else {
					break;
				}
			}
			if (!portRec) {
				goto next_loop;
			}

			int portId = get_oid_num(rp, ifName.oidLen);
			portRec->PortInfo.LID = portId;
			TRACEPRINT("Map %s to IfIndex=%d\n", portRec->PortInfo.LocalPortId, portId);
			cl_qmap_remove_item(&portNumMap, mItem);
			MemoryDeallocate(mapObj);

			mapObj = create_map_obj(lItem);
			if (mapObj) {
				cl_qmap_insert(&portIdMap, portId, &(mapObj->item));
			} else {
				fprintf(stderr, "ERROR - failed to create map obj.\n");
				fstatus = HMGT_STATUS_INSUFFICIENT_MEMORY;
				break;
			}
		} else if (is_oid(rp, &ifType)) {
			TRACEPRINT("..ifType\n");
			int portId = get_oid_num(rp, ifOperStatus.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				portRec->PortInfo.PortPhysConfig.s.PortType =
						(uint8) *(rp->val.integer);
			}
		} else if (is_oid(rp, &ifMTU)) {
			TRACEPRINT("..ifMTU\n");
			int portId = get_oid_num(rp, ifOperStatus.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				portRec->PortInfo.MTU2 = (uint16) *(rp->val.integer);
			}
		} else if (is_oid(rp, &ifSpeed)) {
			TRACEPRINT("..ifSpeed\n");
			int portId = get_oid_num(rp, ifOperStatus.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem) {
				// transfer from bit/s to mbit/s to be consistent with ifHighSpeed
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				portRec->PortInfo.IfSpeed =
						(uint32) (*(rp->val.integer) / 1000000);
				portRec->PortInfo.LinkSpeed.Active =
						ifspeed_to_active_linkspeed(portRec->PortInfo.IfSpeed);
			}
		} else if (is_oid(rp, &ifOperStatus)) {
			TRACEPRINT("..ifOperStatus\n");
			int portId = get_oid_num(rp, ifOperStatus.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				int state = *(rp->val.integer);
				portRec->PortInfo.PortStates.s.PortState = state;
			}
		} else if (is_oid(rp, &ifMauStatus)) {
			TRACEPRINT("..ifMauStatus\n");
			int portId = get_oid_num(rp, ifMauStatus.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				uint8 state = (uint8) *(rp->val.integer);
				portRec->PortInfo.PortStates.s.PortPhysicalState = state;
			}
		} else if (is_oid(rp, &ifMauMediaAvailable)) {
			TRACEPRINT("..ifMauMediaAvailable\n");
			int portId = get_oid_num(rp, ifMauMediaAvailable.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				uint8 state = (uint8) *(rp->val.integer);
				portRec->PortInfo.LinkDownReason = state;
			}
		} else if (is_oid(rp, &ifMauTypeListBits)) {
			TRACEPRINT("..ifMauTypeListBits\n");
			int portId = get_oid_num(rp, ifMauTypeListBits.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem && !is_all_zeros(rp->val.bitstring, rp->valLen)) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				memcpy(portRec->PortInfo.LinkModeSupported, rp->val.string,
						rp->valLen > LINK_MODES_SIZE ? LINK_MODES_SIZE : rp->valLen);
				portRec->PortInfo.LinkSpeed.Supported =
						get_link_speed_supported(rp->val.bitstring, rp->valLen);
				portRec->PortInfo.LinkModeSupLen = (uint16) rp->valLen;
			}
		} else if (is_oid(rp, &ifMauAutoNegAdminStatus)) {
			TRACEPRINT("..ifMauAutoNegAdminStatus\n");
			int portId = get_oid_num(rp, ifMauAutoNegAdminStatus.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				uint8 state = (uint8) *(rp->val.integer);
				portRec->PortInfo.PortStates.s.IsSMConfigurationStarted =
						state == 1 ? 1 : 0;
			}
		} else if (is_oid(rp, &ifHighSpeed)) {
			TRACEPRINT("..ifHighSpeed\n");
			// query ifHighSpeed later than ifSpeed. If it's available, we
			// replace the value we get from ifSpeed.
			int portId = get_oid_num(rp, ifHighSpeed.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				portRec->PortInfo.IfSpeed =
						(uint32) *(rp->val.integer);
				portRec->PortInfo.LinkSpeed.Active =
						ifspeed_to_active_linkspeed(portRec->PortInfo.IfSpeed);
			}
		}
next_loop:
		rp = rp->next;
	}

	STL_PORTINFO_RECORD *portRec;
	for (item = QListHead(rawNode->ports); item!=NULL;
			item = QListNext(rawNode->ports, item)) {
		portRec = item->pObject;
		if (*portRec->PortInfo.LocalPortId) {
			portRec->RID.PortNum = pn_gen_get_port(&png_model, (char* const)portRec->PortInfo.LocalPortId);
		}
	}
	pn_gen_cleanup(&png_model);

	// portNumMap shall be empty and all related cl_map_obj_t shall be already
	// freed. We call cleanup_map here just to play safe.
	cleanup_map(&portNumMap);
	cleanup_map(&portIdMap);
	return fstatus;
}

/*
 * @brief create port records for each host node and put port list back to RAW_NODE
 */
HMGT_STATUS_T populate_host_node_port_records(SNMPResult *res,
		QUICK_LIST *nodeList) {
	HMGT_STATUS_T fstatus = HMGT_STATUS_SUCCESS;
	SNMPResult *rp = res;
	char buf[1024];
	int i = 0;
	cl_qmap_t portIdMap;
	cl_qmap_init(&portIdMap, NULL);
	uint32 capSupported = 0x39;
	uint32 capEnabled = 0x01;
	while (rp && rp->oidLen) {
		if (TRACE) {
			snprint_objid(buf, sizeof(buf), rp->oid, rp->oidLen);
			TRACEPRINT("Processing %s type=%d str=", buf, rp->type);
			for (i=0; i < rp->valLen; i++) {
				TRACEPRINT("%02X ", rp->val.string[i]);
			}
			TRACEPRINT("\n");
		}

		if (is_oid(rp, &ifType)) {
			TRACEPRINT("..ifType\n");
			uint8 type = (uint8) *(rp->val.integer);
			// ignore software loop back
			if (type != ETH_PORT_TYPE_24) {
				int portId = get_oid_num(rp, ifOperStatus.oidLen);
				STL_PORTINFO_RECORD *port = MemoryAllocate2AndClear(sizeof(STL_PORTINFO_RECORD),
						IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
				if (port == NULL) {
					fprintf(stderr,
							"ERROR - couldn't allocate memory for Port record!\n");
					fstatus = HMGT_STATUS_INSUFFICIENT_MEMORY;
					break;
				}

				// TODO: support NIC with multiple ports
				port->RID.PortNum = 1;
				port->PortInfo.LocalPortNum = 1;
				port->PortInfo.CapabilityMask.AsReg32 = capSupported;
				port->PortInfo.CapabilityMask3.AsReg16 = capEnabled;
				port->PortInfo.LID = portId;
				port->PortInfo.PortPhysConfig.s.PortType = type;
				// TODO: support setting of port PhysicalState
				port->PortInfo.PortStates.s.PortPhysicalState = ETH_PORT_PHYS_OPERATIONAL;
				cl_map_obj_t *mapObj = create_map_obj(port);
				if (mapObj) {
					cl_qmap_insert(&portIdMap, portId, &(mapObj->item));
				} else {
					fprintf(stderr, "ERROR - failed to create map obj.\n");
					fstatus = HMGT_STATUS_INSUFFICIENT_MEMORY;
					break;
				}
			}
		} else if (is_oid(rp, &ifMTU)) {
			TRACEPRINT("..ifMTU\n");
			int portId = get_oid_num(rp, ifMTU.oidLen);
			STL_PORTINFO_RECORD *portRec = get_map_obj(&portIdMap, portId);
			if (portRec) {
				portRec->PortInfo.MTU2 = (uint16) *(rp->val.integer);
			}
		} else if (is_oid(rp, &ifSpeed)) {
			TRACEPRINT("..ifSpeed\n");
			int portId = get_oid_num(rp, ifSpeed.oidLen);
			STL_PORTINFO_RECORD *portRec = get_map_obj(&portIdMap, portId);
			if (portRec) {
				// transfer from bit/s to mbit/s to be consistent with ifHighSpeed
				portRec->PortInfo.IfSpeed =
						(uint32) (*(rp->val.integer) / 1000000);
				portRec->PortInfo.LinkSpeed.Active =
						ifspeed_to_active_linkspeed(portRec->PortInfo.IfSpeed);
			}
		} else if (is_oid(rp, &ifPhysAddress)) {
			TRACEPRINT("..ifPhysAddress\n");
			int portId = get_oid_num(rp, ifPhysAddress.oidLen);
			STL_PORTINFO_RECORD *portRec = get_map_obj(&portIdMap, portId);
			if (portRec) {
				copy_snmp_string(rp, (char*) portRec->PortInfo.LocalPortId,
					TINY_STR_ARRAY_SIZE, TRUE);
				portRec->PortInfo.LocalPortIdSubtype = PORTID_SUBTYPE_MAC;
			}
		} else if (is_oid(rp, &ifOperStatus)) {
			TRACEPRINT("..ifOperStatus\n");
			int portId = get_oid_num(rp, ifOperStatus.oidLen);
			STL_PORTINFO_RECORD *portRec = get_map_obj(&portIdMap, portId);
			if (portRec) {
				int state = *(rp->val.integer);
				portRec->PortInfo.PortStates.s.PortState = state;
			}
		} else if (is_oid(rp, &ifMauStatus)) {
			TRACEPRINT("..ifMauStatus\n");
			int portId = get_oid_num(rp, ifMauStatus.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				uint8 state = (uint8) *(rp->val.integer);
				portRec->PortInfo.PortStates.s.PortPhysicalState = state;
			}
		} else if (is_oid(rp, &ifMauMediaAvailable)) {
			TRACEPRINT("..ifMauMediaAvailable\n");
			int portId = get_oid_num(rp, ifMauMediaAvailable.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				uint8 state = (uint8) *(rp->val.integer);
				portRec->PortInfo.LinkDownReason = state;
			}
		} else if (is_oid(rp, &ifMauTypeListBits)) {
			TRACEPRINT("..ifMauTypeListBits\n");
			int portId = get_oid_num(rp, ifMauTypeListBits.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem && !is_all_zeros(rp->val.bitstring, rp->valLen)) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				memcpy(portRec->PortInfo.LinkModeSupported, rp->val.string,
						rp->valLen > LINK_MODES_SIZE ? LINK_MODES_SIZE : rp->valLen);
				portRec->PortInfo.LinkSpeed.Supported =
						get_link_speed_supported(rp->val.bitstring, rp->valLen);
				portRec->PortInfo.LinkModeSupLen = (uint16) rp->valLen;
			}
		} else if (is_oid(rp, &ifMauAutoNegAdminStatus)) {
			TRACEPRINT("..ifMauAutoNegAdminStatus\n");
			int portId = get_oid_num(rp, ifMauAutoNegAdminStatus.oidLen);
			LIST_ITEM *lItem = get_map_obj(&portIdMap, portId);
			if (lItem) {
				STL_PORTINFO_RECORD *portRec = lItem->pObject;
				uint8 state = (uint8) *(rp->val.integer);
				portRec->PortInfo.PortStates.s.IsSMConfigurationStarted =
						state == 1 ? 1 : 0;
			}
		} else if (is_oid(rp, &ifHighSpeed)) {
			TRACEPRINT("..ifHighSpeed\n");
			// query ifHighSpeed later than ifSpeed. If it's available, we
			// replace the value we get from ifSpeed.
			int portId = get_oid_num(rp, ifHighSpeed.oidLen);
			STL_PORTINFO_RECORD *portRec = get_map_obj(&portIdMap, portId);
			if (portRec) {
				portRec->PortInfo.IfSpeed =
						(uint32) *(rp->val.integer);
				portRec->PortInfo.LinkSpeed.Active =
						ifspeed_to_active_linkspeed(portRec->PortInfo.IfSpeed);
			}
		} else if (is_oid(rp, &ipAdEntIfIndex)) {
			TRACEPRINT("..ipAdEntIfIndex\n");
			int portId = *(rp->val.integer);
			STL_PORTINFO_RECORD *port = get_map_obj(&portIdMap, portId);
			if (port) {
				port->RID.EndPortLID = get_lid_from_oid(rp, ipAdEntIfIndex.oidLen);
				for (i = 0; i < 4; i++) {
					uint8 val = (uint8) get_oid_num(rp,
							ipAdEntIfIndex.oidLen + i);
					port->PortInfo.IPAddrIPV4.addr[i] = val;
				}
			}
		}
		rp = rp->next;
	}

	LIST_ITEM *item = NULL;
	for (item = QListHead(nodeList); item != NULL;
			item = QListNext(nodeList, item)) {
		RAW_NODE *rawNode = item->pObject;
		rawNode->ports = QListAllocateAndInit(FALSE, SNMPTAG);
		if (rawNode->ports == NULL) {
			fprintf(stderr,
					"ERROR - Failed to allocate memory for Port List!\n");
			fstatus = HMGT_STATUS_INSUFFICIENT_MEMORY;
			break;
		}

		cl_map_item_t *mItem = NULL;
		for (mItem = cl_qmap_head(&portIdMap); mItem != cl_qmap_end(&portIdMap);
				mItem = cl_qmap_next(mItem)) {
			STL_PORTINFO_RECORD *port = cl_qmap_obj(
					PARENT_STRUCT(mItem, cl_map_obj_t, item));
			if (port->PortInfo.LID == rawNode->ifIndex) {
				LIST_ITEM *pItem = MemoryAllocate2AndClear(sizeof(LIST_ITEM), IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
				if (pItem == NULL) {
					fprintf(stderr,
							"ERROR - couldn't allocate memory for list item!\n");
					fstatus = HMGT_STATUS_INSUFFICIENT_MEMORY;
					break;
				}
				pItem->pObject = port;
				QListInsertTail(rawNode->ports, pItem);
				break;
			}
		}
	}

	cleanup_map(&portIdMap);
	return fstatus;
}

HMGT_STATUS_T populate_port_counters(SNMPResult *res,
		cl_qmap_t *ifIndexMap) {
	HMGT_STATUS_T fstatus = HMGT_STATUS_SUCCESS;
	SNMPResult *rp = res;
	char buf[1024];
	int i = 0;
	while (rp && rp->oidLen) {
		if (TRACE) {
			snprint_objid(buf, sizeof(buf), rp->oid, rp->oidLen);
			TRACEPRINT("Processing %s type=%d str=", buf, rp->type);
			for (i=0; i < rp->valLen; i++) {
				TRACEPRINT("%02X ", rp->val.string[i]);
			}
			TRACEPRINT("\n");
		}

		if (is_oid(rp, &ifInDiscards)) {
			TRACEPRINT("..ifInDiscards\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &ifInDiscards);
			if (pCounters) {
				pCounters->portRcvBECN = *rp->val.integer;
			}
		} else if (is_oid(rp, &ifInErrors)) {
			TRACEPRINT("..ifInErrors\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &ifInErrors);
			if (pCounters) {
				pCounters->ifInErrors = *rp->val.integer;
			}
		} else if (is_oid(rp, &ifInUnknownProtos)) {
			TRACEPRINT("..ifInUnknownProtos\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(
					ifIndexMap,
					rp, &ifInUnknownProtos);
			if (pCounters) {
				pCounters->ifInUnknownProtos = *rp->val.integer;
			}
		} else if (is_oid(rp, &ifOutDiscards)) {
			TRACEPRINT("..ifOutDiscards\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(
					ifIndexMap,
					rp, &ifOutDiscards);
			if (pCounters) {
				pCounters->portXmitDiscards = *rp->val.integer;
			}
		} else if (is_oid(rp, &ifOutErrors)) {
			TRACEPRINT("..ifOutErrors\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &ifOutErrors);
			if (pCounters) {
				pCounters->ifOutErrors = *rp->val.integer;
			}
		} else if (is_oid(rp, &dot3StatsSingleCollisionFrames)) {
			TRACEPRINT("..dot3StatsSingleCollisionFrames\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3StatsSingleCollisionFrames);
			if (pCounters) {
				pCounters->dot3StatsSingleCollisionFrames = *rp->val.integer;
			}
		} else if (is_oid(rp, &dot3StatsMultipleCollisionFrames)) {
			TRACEPRINT("..dot3StatsMultipleCollisionFrames\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3StatsMultipleCollisionFrames);
			if (pCounters) {
				pCounters->dot3StatsMultipleCollisionFrames = *rp->val.integer;
			}
		} else if (is_oid(rp, &dot3StatsSQETestErrors)) {
			TRACEPRINT("..dot3StatsSQETestErrors\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3StatsSQETestErrors);
			if (pCounters) {
				pCounters->dot3StatsSQETestErrors = *rp->val.integer;
			}
		} else if (is_oid(rp, &dot3StatsDeferredTransmissions)) {
			TRACEPRINT("..dot3StatsDeferredTransmissions\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3StatsDeferredTransmissions);
			if (pCounters) {
				pCounters->dot3StatsDeferredTransmissions = *rp->val.integer;
			}
		} else if (is_oid(rp, &dot3StatsLateCollisions)) {
			TRACEPRINT("..dot3StatsLateCollisions\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3StatsLateCollisions);
			if (pCounters) {
				pCounters->dot3StatsLateCollisions = *rp->val.integer;
			}
		} else if (is_oid(rp, &dot3StatsExcessiveCollisions)) {
			TRACEPRINT("..dot3StatsExcessiveCollisions\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3StatsExcessiveCollisions);
			if (pCounters) {
				pCounters->dot3StatsExcessiveCollisions = *rp->val.integer;
			}
		} else if (is_oid(rp, &dot3StatsCarrierSenseErrors)) {
			TRACEPRINT("..dot3StatsCarrierSenseErrors\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3StatsCarrierSenseErrors);
			if (pCounters) {
				pCounters->dot3StatsCarrierSenseErrors = *rp->val.integer;
			}
		} else if (is_oid(rp, &dot3HCStatsAlignmentErrors)) {
			TRACEPRINT("..dot3HCStatsAlignmentErrors\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3HCStatsAlignmentErrors);
			if (pCounters) {
				pCounters->dot3HCStatsAlignmentErrors =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		} else if (is_oid(rp, &dot3HCStatsFCSErrors)) {
			TRACEPRINT("..dot3HCStatsFCSErrors\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3HCStatsFCSErrors);
			if (pCounters) {
				pCounters->dot3HCStatsFCSErrors =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		} else if (is_oid(rp, &dot3HCStatsInternalMacTransmitErrors)) {
			TRACEPRINT("..dot3HCStatsInternalMacTransmitErrors\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3HCStatsInternalMacTransmitErrors);
			if (pCounters) {
				pCounters->dot3HCStatsInternalMacTransmitErrors =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		} else if (is_oid(rp, &dot3HCStatsFrameTooLongs)) {
			TRACEPRINT("..dot3HCStatsFrameTooLongs\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3HCStatsFrameTooLongs);
			if (pCounters) {
				pCounters->excessiveBufferOverruns =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		} else if (is_oid(rp, &dot3HCStatsInternalMacReceiveErrors)) {
			TRACEPRINT("..dot3HCStatsInternalMacReceiveErrors\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3HCStatsInternalMacReceiveErrors);
			if (pCounters) {
				pCounters->portRcvErrors =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		} else if (is_oid(rp, &dot3HCStatsSymbolErrors)) {
			TRACEPRINT("..dot3HCStatsSymbolErrors\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &dot3HCStatsSymbolErrors);
			if (pCounters) {
				pCounters->localLinkIntegrityErrors =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		} else if (is_oid(rp, &ifHCInOctets)) {
			TRACEPRINT("..ifHCInOctets\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &ifHCInOctets);
			if (pCounters) {
				pCounters->portRcvData =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		} else if (is_oid(rp, &ifHCInUcastPkts)) {
			TRACEPRINT("..ifHCInUcastPkts\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &ifHCInUcastPkts);
			if (pCounters) {
				pCounters->portRcvPkts =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		} else if (is_oid(rp, &ifHCInMulticastPkts)) {
			TRACEPRINT("..ifHCInMulticastPkts\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &ifHCInMulticastPkts);
			if (pCounters) {
				pCounters->portMulticastRcvPkts =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		} else if (is_oid(rp, &ifHCOutOctets)) {
			TRACEPRINT("..ifHCOutOctets\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &ifHCOutOctets);
			if (pCounters) {
				pCounters->portXmitData =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		} else if (is_oid(rp, &ifHCOutUcastPkts)) {
			TRACEPRINT("..ifHCOutUcastPkts\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &ifHCOutUcastPkts);
			if (pCounters) {
				pCounters->portXmitPkts =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		} else if (is_oid(rp, &ifHCOutMulticastPkts)) {
			TRACEPRINT("..ifHCOutMulticastPkts\n");
			STL_PORT_COUNTERS_DATA *pCounters = get_port_counters(ifIndexMap,
					rp, &ifHCOutMulticastPkts);
			if (pCounters) {
				pCounters->portMulticastXmitPkts =
						COUNTER64_TO_UINT64(rp->val.counter64);
			}
		}
		rp = rp->next;
	}
	return fstatus;
}
//--------- End of Data Processing Functions --------------//


/*
 * initialize
 */
static boolean init_oids(SNMPOid **oids) {
	char buf[1024];
	SNMPOid **opp = oids;
	SNMPOid *op;
	while ((op = *opp)) {
		op->oidLen = sizeof(op->oid) / sizeof(op->oid[0]);
		TRACEPRINT("Prepare OID: %s Len:%zu\n", op->name, op->oidLen);
		if (!snmp_parse_oid(op->name, op->oid, &op->oidLen)) {
			snmp_perror("read_objid");
			return FALSE;
		}
		snprint_objid(buf, sizeof(buf), op->oid, op->oidLen);
		TRACEPRINT("  ==> ID: %s (%s)\n", op->name, buf);
		opp++;
	}
	return TRUE;
}

static HMGT_STATUS_T init_hosts(FabricData_t *pFabric, SNMPHost **hosts, uint32_t *entries) 
{
	int memSize;
	uint16_t i = 0, j = 0;
	uint32_t nodes;
	HMGT_STATUS_T status = HMGT_STATUS_SUCCESS; 
	LIST_ITEM *dscNodeLstp;
	SNMPHost *pHosts;


	nodes = QListCount(&pFabric->SnmpDiscoverHosts) + QListCount(&pFabric->SnmpDiscoverSwitches);
	if (nodes == 0) {
		status = HMGT_STATUS_NOT_FOUND;
		fprintf(stderr, "ERROR - no hosts configured for fabric discovery.\n");
		goto done;
	}

	memSize = sizeof(SNMPHost) * nodes;
	pHosts = MemoryAllocate2AndClear(memSize, IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
	if (!pHosts) {
		fprintf(stderr, "ERROR - failed to allocate memory for hosts.\n");
		status = HMGT_STATUS_INSUFFICIENT_MEMORY;
		goto done;
	}

	// Populate hosts array entries from hosts configuration file.
	for (dscNodeLstp = QListHead(&pFabric->SnmpDiscoverHosts);
		(dscNodeLstp != NULL) && (i < nodes);
		i++, dscNodeLstp = QListNext(&pFabric->SnmpDiscoverHosts, dscNodeLstp)) {
		snmpNodeConfigData_t *snmpNodeConfp = (snmpNodeConfigData_t *)QListObj(dscNodeLstp);
		pHosts[i].type = STL_NODE_FI;
		pHosts[i].numInterface = QListCount(&snmpNodeConfp->InterfaceNames);
		if (pHosts[i].numInterface == 0) {
			pHosts[i].interfaces = NULL;
		} else {
			pHosts[i].interfaces = MemoryAllocate2AndClear(pHosts[i].numInterface * sizeof (char*),
				IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
			if (!pHosts[i].interfaces) {
				fprintf(stderr, "ERROR - failed to allocate memory for host interfaces.\n");
				status = HMGT_STATUS_INSUFFICIENT_MEMORY;
				MemoryDeallocate(pHosts);
				goto done;
			}
			LIST_ITEM *item = NULL;
			for (j = 0, item = QListHead(&snmpNodeConfp->InterfaceNames);
					(item != NULL) && (j < pHosts[i].numInterface);
					j++, item = QListNext(&snmpNodeConfp->InterfaceNames, item)) {
				pHosts[i].interfaces[j] = (char*)QListObj(item);
			}
		}

		pHosts[i].name = (char *)snmpNodeConfp->NodeDesc.NodeString;
	}

	// Populate hosts array entries from switches configuration file.
	for (dscNodeLstp = QListHead(&pFabric->SnmpDiscoverSwitches);
		(dscNodeLstp != NULL) && (i < nodes);
		i++, dscNodeLstp = QListNext(&pFabric->SnmpDiscoverSwitches, dscNodeLstp)) {
		snmpNodeConfigData_t *snmpNodeConfp = (snmpNodeConfigData_t *)QListObj(dscNodeLstp);
		pHosts[i].type = STL_NODE_SW;
		pHosts[i].name = (char *)snmpNodeConfp->NodeDesc.NodeString;
	}
	
	*hosts = pHosts;
	*entries = nodes;
done:
	return status;
}

int hmgt_snmp_init(void)
{
	int rc = 0;

	if (!SNMP_INITED) {
		/* initialize library */
		init_snmp("hpn_snmp_api");

		SNMP_INITED = 1;
		DBGPRINT("NET-SNMP Initialized!\n");
	}
	if (init_oids(LLDPOids)) {
		rc = 1;
	} else {
		char buf[1024];
		SNMPOid **opp = LLDPOids;
		while (*opp) {
			TRACEPRINT("OID = %s Len = %zu\n", (*opp)->name, (*opp)->oidLen);
			snprint_objid(buf, sizeof(buf), (*opp)->oid, (*opp)->oidLen);
			TRACEPRINT("  ==> OID: %s (%s)\n", (*opp)->name, buf);
			opp++;
		}
	}

	return rc;
}


//TODO: This function needs to be rewritten; comes from asyncapp.c demo
/*
 * simple printing of returned data
 */
int print_result(FILE *out, int status, struct snmp_session *session,
		struct snmp_pdu *pdu) {
	char buf[1024];
	struct variable_list *varList = NULL;
	int i = 0;

	print_timestamp(out);
	switch (status) {
	case STAT_SUCCESS:
		varList = pdu->variables;
		if (pdu->errstat == SNMP_ERR_NOERROR) {
			while (varList) {
				snprint_variable(buf, sizeof(buf), varList->name,
						varList->name_length, varList);
				fprintf(out, "%s: %s\n", session->peername, buf);
				varList = varList->next_variable;
				if (varList) {
					print_timestamp(out);
				}
			}
		} else {
			for (i = 1; varList && i != pdu->errindex;
					varList = varList->next_variable, i++)
				;
			if (varList) {
				snprint_objid(buf, sizeof(buf), varList->name,
						varList->name_length);
			} else {
				strcpy(buf, "(none)");
			}
			fprintf(out, "%s: %s: %s\n", session->peername, buf,
					snmp_errstring(pdu->errstat));
		}
		return 1;
	case STAT_TIMEOUT:
		fprintf(out, "%s: Timeout\n", session->peername);
		return 0;
	case STAT_ERROR:
		snmp_perror(session->peername);
		return 0;
	}
	return 0;
}

/*
 * prepare next SNMP query based on OID type
 */
struct snmp_pdu * prepare_snmp_query(struct context_s *context) {
	struct snmp_pdu *res = NULL;
	if (!context->current_oid->name) {
		return res;
	}
	if (context->current_oid->type == SNMP_MSG_GET) {
		res = snmp_pdu_create(SNMP_MSG_GET);
		TRACEPRINT("Query GET %s\n", context->current_oid->name);
	} else {
		res = snmp_pdu_create(SNMP_MSG_GETBULK);
		res->non_repeaters = 0;
		// Entity MIB returns all entities in a device that can be a big number.
		// We only care Module/Chassis that supposed to be in the first 5. Usually
		// it's the first or second (if it has virtual root) one.
		if (match_oid(entPhysical.oid, entPhysical.oidLen,
			context->current_oid->oid, context->current_oid->oidLen)) {
			res->max_repetitions = 5;
		} else {
			res->max_repetitions = context->ifNumber ? context->ifNumber + 1 : SNMP_BULK_SIZE;
		}
		TRACEPRINT("Query GETBULK %s max_repetitions=%ld\n",
			context->current_oid->name, res->max_repetitions);
	}
	snmp_add_null_var(res, context->current_oid->oid, context->current_oid->oidLen);
	return res;
}

//TODO: improve to store query result in a map with key=oid, value=SNMPResult
//      that is a linked list with all results for an oid.
/*
 * response handler
 */
int asynch_mixed_response(int operation, struct snmp_session *sp, int reqid,
		struct snmp_pdu *pdu, void *magic) {
	struct context_s *context = (struct context_s *) magic;
	struct snmp_pdu *req = NULL;
	struct variable_list *vars;
	QueryState state = Q_NONE;
	SNMPOid *next_oid;

	TRACEPRINT("Get response for %s, magic=%p from %s\n",
			context->current_oid->name, magic, sp->peername);
	if (operation == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE) {
		if (pdu->errstat == SNMP_ERR_NOERROR) {
			if (TRACE) {
				print_result(verbose_file, STAT_SUCCESS, context->sess, pdu);
			}
			vars = pdu->variables;
			while (vars) {
				if (!match_oid(context->current_oid->oid,
						context->current_oid->oidLen, vars->name,
						vars->name_length)) {
					TRACEPRINT("  No OID match\n");
					if ((! context->resultTail) ||
					    (! is_oid(context->resultTail, context->current_oid)))
						PRINT_NOSUCHOBJECT(context->current_oid->name, sp->peername);

					next_oid = context->current_oid + 1;
					if (!next_oid->name ||
					    !match_oid(next_oid->oid, next_oid->oidLen,
						vars->name, vars->name_length)) {
						state = Q_END_NEXT;
						break;
					} else {
						context->current_oid += 1;
						state = Q_NEXT;
						continue;
					}
				}
				if (vars->type == SNMP_ENDOFMIBVIEW ||
				    vars->type == SNMP_NOSUCHOBJECT ||
				    vars->type == SNMP_NOSUCHINSTANCE) {
					PRINT_NOSUCHOBJECT(context->current_oid->name,
							   sp->peername);
					state = Q_WARN;
					break;
				}
				if (!context->ifNumber &&
				    match_oid(ifNumber.oid, ifNumber.oidLen, vars->name, vars->name_length)) {
				    context->ifNumber = (uint16) *(vars->val.integer);
				    TRACEPRINT("ifNumber=%d\n", context->ifNumber);
				}
				state = context->current_oid->type == SNMP_MSG_GET ? Q_END_NEXT : Q_NEXT;
				boolean fillFirst = FALSE;
				if (!context->result) {
					context->result = context->resultTail = get_next_snmp_result(NULL);
					fillFirst = TRUE;
				}
				SNMPResult * newRes = add_snmp_result(context->resultTail,
						vars, fillFirst);
				context->resultTail = newRes;
				if (!newRes) {
					fprintf(stderr,
							"ERROR - Couldn't allocate memory for SNMPResult.\n");
					// set state to Q_ERROR, so we stop the query.
					state = Q_ERROR;
				}
				if (vars->next_variable) {
					vars = vars->next_variable;
				} else {
					break;
				}
			}
			// Entity MIB data can be big. We only care the first couple entities,
			// so no continue query for it.
			if (state == Q_NEXT &&
				!match_oid(entPhysical.oid, entPhysical.oidLen, vars->name, vars->name_length)) {
				req = snmp_pdu_create(SNMP_MSG_GETBULK);
				req->non_repeaters = 0;
				req->max_repetitions = context->ifNumber ? context->ifNumber + 1 : SNMP_BULK_SIZE;
				if (TRACE) {
					char buf[512];
					snprint_variable(buf, sizeof(buf), vars->name,
							vars->name_length, vars);
					TRACEPRINT("Continue Query \n    %s\n", buf);
				}
				snmp_add_null_var(req, vars->name, vars->name_length);
			} else if (state != Q_ERROR) {
				context->current_oid += 1;
				req = prepare_snmp_query(context);
			}

			if (req) {
				if (snmp_send(context->sess, req)) {
					if (TRACE) {
						print_timestamp(verbose_file?verbose_file:stderr);
					}
					TRACEPRINT("Send Query to %s\n", context->sess->peername);
					return 1;
				} else {
					snmp_perror("snmp_send");
					snmp_free_pdu(req);
					state = Q_ERROR;
				}
			}
		} else {
			print_result(stderr, STAT_SUCCESS, context->sess, pdu);
			state = Q_ERROR;
		}
	} else {
		print_result(stderr, STAT_TIMEOUT, context->sess, pdu);
		state = Q_ERROR;
	}

	/* something went wrong or end of variables
	 * this host not active any more
	 */
	if (state != Q_ERROR) {
		TRACEPRINT("Process data\n");
		time_print("[%s] Data collected\n", context->host->name);
		context->populated_data = context->processor(context->host,
				context->result, context->fabric);
	}

	free_snmp_result(context->result);
	active_hosts--;
	TRACEPRINT("Decrease - ActiveHosts=%d\n", active_hosts);
	return 1; 
}

/*
 * @brief the entry function that collects data simultaneously from given hosts
 *        and then processes returned data with given callback functions in
 *        single thread. This function blocks until all data got back and are
 *        processed.
 * @param hosts	a list of hosts defines where we want to collect data. A host
 *             	can be any device that provide SNMP services
 * @param sw_oids	a list of oids that defines which data we want to collect
 *                      from switches
 * @param nic_oids	a list of oids that defines which data we want to collect
 *                      from nic hosts
 * @param numHosts	the number of hosts
 * @param dev_data_processor	the callback function that processes data from
 *                          	individual host, i.e. phase 1 data process at
 *                          	device level
 * @param fab_data_processor	the callback function that processes data from
 *                          	all hosts, i.e. phase 2 data process at fabric
 *                          	level
 * @param dev_cleanup_processor	the callback function that cleans up resources
 *                             	created by dev_data_processor
 * @param fabric	the FabricData_t that stores data for a fabric. callback
 *              	functions suppose to process data and fill result into it.
 */
HMGT_STATUS_T collect_data(SNMPHost *hosts, SNMPOid *sw_oids, SNMPOid *nic_oids, int numHosts,
		snmp_device_data_process dev_data_processor,
		snmp_fabric_data_process fab_data_processor,
		snmp_device_data_cleanup dev_cleanup_processor, FabricData_t *fabric)
{
	// Prepare OIDs
	struct context_s contexts[numHosts];
	struct context_s *cs;

	//Convert user specified SNMP security parameters
	int version = 0;
	int secLevel = 0;
	oid *authProtocol = NULL;
	oid *encrypProtocol = NULL;
	size_t authProtocolLength = 0;
	size_t encrypProtocolLength = 0;
	int configParseError = 0;

	HMGT_STATUS_T fstatus = HMGT_STATUS_SUCCESS;

	//SNMP Version
	if (strcmp(fabric->SnmpVersion, "SNMP_VERSION_3") == 0) {
        	version = SNMP_VERSION_3;
		DBGPRINT("Running SNMP_VERSION_3\n");
        } else {
        	version = SNMP_VERSION_2c;
		DBGPRINT("Running SNMP_VERSION_2c\n");
        }

	//Security Level
        if (strcmp(fabric->SnmpSecurityLevel, "NOAUTH") == 0) {
                secLevel = SNMP_SEC_LEVEL_NOAUTH;
                DBGPRINT("Running with no authentication or encryption\n");
        } else if (strcmp(fabric->SnmpSecurityLevel, "AUTHNOPRIV") == 0) {
                secLevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
                DBGPRINT("Running with authentication, but no encryption\n");
        } else if (strcmp(fabric->SnmpSecurityLevel, "AUTHPRIV") == 0) {
                secLevel = SNMP_SEC_LEVEL_AUTHPRIV;
                DBGPRINT("Running with both authentication and encryption\n");
        } else {
                fprintf(stderr, "%s: Error parsing SnmpSecurityLevel\n", __func__);
		configParseError = 1;
	}

	//Authentication Protocol
	if ( (secLevel == SNMP_SEC_LEVEL_AUTHNOPRIV) || (secLevel == SNMP_SEC_LEVEL_AUTHPRIV) ) {
		if (strcmp(fabric->SnmpAuthenticationProtocol, "MD5") == 0) {
			authProtocol = usmHMACMD5AuthProtocol;
			authProtocolLength = USM_AUTH_PROTO_MD5_LEN;
			DBGPRINT("Running MD5 authentication \n");
		} else if (strcmp(fabric->SnmpAuthenticationProtocol, "SHA") == 0) {
			authProtocol = usmHMACSHA1AuthProtocol;
			authProtocolLength = USM_AUTH_PROTO_SHA_LEN;
			DBGPRINT("Running SHA authentication\n");
		} else {
			fprintf(stderr, "%s: Error parsing SnmpAuthenticationProtocol\n", __func__);
			configParseError = 1;
		}
	}
	
	//Encryption Protocol
	if (secLevel == SNMP_SEC_LEVEL_AUTHPRIV) {
		if (strcmp(fabric->SnmpEncryptionProtocol, "AES") == 0) {
				encrypProtocol = usmAESPrivProtocol;
				encrypProtocolLength = USM_PRIV_PROTO_AES_LEN;
				DBGPRINT("Running AES encryption\n");
		} else {
				DBGPRINT("Error parsing SnmpEncryptionProtocol\n");
				configParseError = 1;
		}
	}
		
	//Additional information we may want to enable for debugging
	//TRACEPRINT("SNMP Community String: %s\n", &fabric->SnmpCommunityString[0]);
	//TRACEPRINT("SNMP Security Name: %s\n", &fabric->SnmpSecurityName[0]);
	//TRACEPRINT("SNMP Auth Passphrase: %s\n", &fabric->SnmpAuthenticationPassphrase[0]);
	//TRACEPRINT("SNMP Encryp Passphrase: %s\n", &fabric->SnmpEncryptionPassphrase[0]);

	if (configParseError) {
		return HMGT_STATUS_INVALID_SETTING;
	}

	time_print("Start data collection...\n");

	int count;
	for (cs = contexts, count = 0; count < numHosts; count++, cs++) {

		struct snmp_pdu *req;
		struct snmp_session sess = {0};

		if (!hosts[count].name) {
			fprintf(stderr, "WARNING - no host name defined. Skip.\n");
			continue;
		}

		cs->host = &hosts[count];
		if (hosts[count].type == STL_NODE_FI) {
			cs->current_oid = nic_oids;
		} else {
                	cs->current_oid = sw_oids;
                }
		cs->processor = dev_data_processor;
		cs->fabric = fabric;
		cs->result = NULL;
		cs->resultTail = NULL;
		cs->populated_data = NULL;
		cs->ifNumber = 0;

		DBGPRINT("Init SNMP Session for %s\n", hosts[count].name);

		snmp_sess_init(&sess); /* initialize session */
		char* peer_name = MemoryAllocate2AndClear(STL_NODE_DESCRIPTION_ARRAY_SIZE+8, IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
		if (!peer_name) {
			fprintf(stderr, "ERROR - Couldn't allocate memory for peer name\n");
			continue;
		}
		snprintf(peer_name, STL_NODE_DESCRIPTION_ARRAY_SIZE+8, "%s:%d",
				hosts[count].name, fabric->SnmpPort);

		if (version == SNMP_VERSION_3) {
			sess.version = SNMP_VERSION_3;
			sess.peername = peer_name;

			sess.securityName = strdup(&fabric->SnmpSecurityName[0]);
			if (!sess.securityName) {
				fprintf(stderr, "ERROR - Couldn't allocate memory for securityName\n");
				continue;
			}
			sess.securityNameLen = strlen(sess.securityName);
			
			sess.securityLevel = secLevel;

			if (authProtocol) {
				sess.securityAuthProto = authProtocol;
				sess.securityAuthProtoLen = authProtocolLength;
				sess.securityAuthKeyLen = USM_AUTH_KU_LEN;
			}

			if (encrypProtocol) {
				sess.securityPrivProto = encrypProtocol;
				sess.securityPrivProtoLen = encrypProtocolLength;
				sess.securityPrivKeyLen = USM_PRIV_KU_LEN;
			}

			//Generate authentication key
			if (generate_Ku(sess.securityAuthProto, sess.securityAuthProtoLen,
					(u_char*) fabric->SnmpAuthenticationPassphrase,
					strlen(&fabric->SnmpAuthenticationPassphrase[0]),
					sess.securityAuthKey,
					&sess.securityAuthKeyLen) != SNMPERR_SUCCESS) {
				fprintf(stderr, "%s: ERROR generating authentication key\n", __func__);
				return HMGT_STATUS_INVALID_SETTING;
			}

			//Generate encryption key
			if (generate_Ku(sess.securityPrivProto,
					sess.securityPrivProtoLen,
					(u_char*)fabric->SnmpEncryptionPassphrase,
					strlen(&fabric->SnmpEncryptionPassphrase[0]),
					sess.securityPrivKey,
					&sess.securityPrivKeyLen) != SNMPERR_SUCCESS) {
				fprintf(stderr, "%s: ERROR generating encryption key\n", __func__);
				return HMGT_STATUS_INVALID_SETTING;
			}
	
			TRACEPRINT("Set up snmp_session for %s\n", &fabric->SnmpSecurityName[0]);	

		} else {
			sess.version = SNMP_VERSION_2c;
			sess.peername = peer_name;
			if ( fabric->SnmpCommunityString[0] != '\0' ) {
				sess.community = (u_char *) strdup(fabric->SnmpCommunityString);
				if (!sess.community) {
					fprintf(stderr, "%s: ERROR - Couldn't allocate memory for Community string\n", __func__);
					return HMGT_STATUS_INSUFFICIENT_MEMORY;
				}
			} else {
				fprintf(stderr, "%s: ERROR - unspecified community string when configuring SNMP V2\n", __func__);
				return HMGT_STATUS_INVALID_SETTING;
			}
			sess.community_len = strlen((char*) sess.community);
		}

		sess.callback = asynch_mixed_response; /* default callback */
		sess.callback_magic = cs;
		cs->sess = snmp_open(&sess);
		if (sess.peername) {
			MemoryDeallocate(sess.peername);
		}
		if (sess.securityName) {
			MemoryDeallocate(sess.securityName);
		}
		if (sess.community) {
			MemoryDeallocate(sess.community);
		}
		if (!cs->sess) {
			snmp_perror("snmp_open");
			fprintf(stderr, "ERROR - Couldn't open SNMP for %s:%d. Skip this host.\n",
					hosts[count].name, fabric->SnmpPort);
			continue;
		}

		req = prepare_snmp_query(cs);

		if (snmp_send(cs->sess, req)) {
			active_hosts++;
			if (TRACE) {
				print_timestamp(verbose_file?verbose_file:stderr);
			}
			TRACEPRINT("Send Query to %s\nIncrease - ActiveHosts=%d\n",
				cs->sess->peername, active_hosts);
		} else {
			snmp_perror("snmp_send");
			snmp_free_pdu(req);
		}
	}

	//TODO: need to consider rewrite here; this loop is the same as asyncapp.c::asynchronous
	while (active_hosts) {
		int fds = 0, block = 1;
		fd_set fdset;
		struct timeval timeout;

		FD_ZERO(&fdset);
		snmp_select_info(&fds, &fdset, &timeout, &block);
		fds = select(fds, &fdset, NULL, NULL, block ? NULL : &timeout);
		if (fds < 0) {
			perror("select failed");
			exit(1);
		}
		if (fds) {
			snmp_read(&fdset);
		} else {
			snmp_timeout();
		}
	}
	//End block that needs rewrite

	/* close snmp */
	for (count = 0; count < numHosts; count++) {
		if (contexts[count].sess) {
			DBGPRINT("Close SNMP Session for %s\n", contexts[count].host->name);
			snmp_close(contexts[count].sess);
		}
	}
	time_print("All data collected and pre-processed.\n");

	time_print("Start phase 2 data processing...\n");
	/* second phase data process */
	void *intermediate_data[numHosts];
	int i = 0;
	for (i=0; i < numHosts; i++) {
		if (contexts[i].sess) 
			intermediate_data[i] = contexts[i].populated_data;
		else
			intermediate_data[i] = NULL;
	}
	HMGT_STATUS_T fstatus2 = fab_data_processor(intermediate_data, numHosts, fabric);

	time_print("Phased 2 data processing finished\n");

	time_print("Start resources cleanup...\n");
	/* cleanup intermediate data */

	HMGT_STATUS_T fstatus3 = HMGT_STATUS_SUCCESS;

        for (i=0; i < numHosts; i++) {
		if (contexts[i].populated_data) {
			HMGT_STATUS_T tmp_status = dev_cleanup_processor(
					contexts[i].populated_data);
			if (tmp_status != HMGT_STATUS_SUCCESS) {
				fstatus3 = tmp_status;
			}
		}
	}
	time_print("Finished resources cleanup...\n");

	//Generate overall status for collect_data
	if (fstatus2 != HMGT_STATUS_SUCCESS) {
		if (fstatus2 == HMGT_STATUS_PARTIALLY_PROCESSED) {
			fprintf(stderr, "fab_data_processor returned partial success\n");
			//TODO - do we want to just report success or report partial success and let caller deal with it?
			//fstatus = HMGT_STATUS_PARTIALLY_PROCESSED;
			fstatus = HMGT_STATUS_SUCCESS;
		}
		else
			fprintf(stderr, "ERROR - function fab_data_processor returned status %d\n", fstatus2);

		if (fstatus2 == HMGT_STATUS_ERROR)
			fstatus = fstatus2;
	}

	if ( (fstatus != HMGT_STATUS_ERROR) && (fstatus3 != HMGT_STATUS_SUCCESS) ) {
		if (fstatus3 == HMGT_STATUS_PARTIALLY_PROCESSED) {
			fprintf(stderr, "dev_cleanup_processor returned partial success\n");
			//TODO - do we want to just report success or report partial success and let caller deal with it?
			//fstatus = HMGT_STATUS_PARTIALLY_PROCESSED;
			fstatus = HMGT_STATUS_SUCCESS;
		}
		else
			fprintf(stderr, "ERROR - function dev_cleanup_processor returned status %d\n", fstatus3);
	}

	return fstatus;
}

/*
 * This routine
 * 1) creates Node Record from SNMP data
 * 2) creates PortInfo records from SNMP data
 * 3) adds both Node and PortInfo records into fabric data
 * 4) creates a map between port id and PortData that will help us figuring out
 *    links in a fabric
 *
 */
QUICK_LIST* process_dev_data(SNMPHost *host, SNMPResult *res,
		FabricData_t *pFabric) {
	time_print("[%s] Start data process...\n", host->name);
	HMGT_STATUS_T fstatus = HMGT_STATUS_SUCCESS;
	QUICK_LIST *nodeList = NULL;
	RAW_NODE *rawNode;
	if (!res) {
		fprintf(stderr, "ERROR - No SNMP data to process!\n");
		return NULL;
	}

	if (!pFabric) {
		fprintf(stderr, "ERROR - No Fabric data!\n");
		return NULL;
	}

	TRACEPRINT("NodeType=%d\n", host->type);
	if (host->type != STL_NODE_SW && host->type != STL_NODE_FI) {
		fprintf(stderr, "ERROR - Failed to get node type!\n");
		return NULL;
	}

	nodeList = QListAllocateAndInit(FALSE, SNMPTAG);
	if (nodeList == NULL) {
		fprintf(stderr, "ERROR - Failed to allocate memory for Node List!\n");
		return NULL;
	}

	if (host->type == STL_NODE_SW) {
		fstatus = populate_switch_node_record(host, res, nodeList);
		if (fstatus != HMGT_STATUS_SUCCESS) {
			goto done;
		}
		fstatus = populate_switch_node_port_records(res, nodeList, pFabric->flags & FF_DOWNPORTINFO);
		if (fstatus != HMGT_STATUS_SUCCESS) {
			goto done;
		}
	} else {
		fstatus = populate_host_node_record(host, res, nodeList);
		if (fstatus != HMGT_STATUS_SUCCESS) {
			goto done;
		}
		fstatus = populate_host_node_port_records(res, nodeList);
		if (fstatus != HMGT_STATUS_SUCCESS) {
			goto done;
		}
	}
	DBGPRINT("Created %d NodeRecords\n", QListCount(nodeList));

	cl_qmap_t ifIndexMap;
	cl_qmap_init(&ifIndexMap, NULL);
	// add node records and ports records into fabric data
	LIST_ITEM *nodeItem;
	STL_NODE_RECORD *nodeRec = NULL;
	for (nodeItem = QListHead(nodeList); nodeItem != NULL;
			nodeItem = QListNext(nodeList, nodeItem)) {
		rawNode = nodeItem->pObject;
		cl_qmap_t *portIdMap = &rawNode->portIdMap;
		cl_qmap_init(portIdMap, NULL);
		nodeRec = rawNode->node;
		TRACEPRINT("FabricDataAddNode Node=%s IfID=0x%04x IfAddr=0x%016"PRIx64"\n",
				nodeRec->NodeDesc.NodeString, nodeRec->RID.LID,
				nodeRec->NodeInfo.NodeGUID);
		if (!(pFabric->flags & FF_DOWNPORTINFO) && nodeRec->RID.LID == 0) {
			// invalid node. Happens on the host interface that has no IP addr
			continue;
		}
		boolean newNode;
		NodeData *pNodeData = FabricDataAddNode(pFabric, nodeRec, &newNode);
		TRACEPRINT("  newNode=%d pNodeData=%p\n", newNode, pNodeData);
		if (!pNodeData) {
			fstatus = HMGT_STATUS_ERROR;
			fprintf(stderr, "ERROR - failed to add node to Fabric Data Ptr\n");
			continue;
		}
		rawNode->nodeData = pNodeData;

		if (rawNode->ports) {
			DBGPRINT("  Found %d Ports on Node %s\n", QListCount(rawNode->ports), nodeRec->NodeDesc.NodeString);
			LIST_ITEM *portItem = NULL;
			STL_PORTINFO_RECORD *portRec;
			for (portItem = QListHead(rawNode->ports); portItem!=NULL;
					portItem = QListNext(rawNode->ports, portItem)) {
				portRec = portItem->pObject;
				TRACEPRINT("AddPort Node=%s PortNum=%u PortId=%s\n",
					nodeRec->NodeDesc.NodeString, portRec->RID.PortNum, portRec->PortInfo.LocalPortId);
				if (portRec->PortInfo.LocalPortId[0] && !portRec->RID.PortNum) {
					// special case - mgmt port that has no dot1BasePort
					continue;
				}
				PortData *pPortData = NodeDataAddPort(pFabric, pNodeData,
						nodeRec->NodeInfo.NodeGUID, portRec);
				if (!pPortData) {
					fstatus = HMGT_STATUS_ERROR;
					fprintf(stderr, "ERROR - failed to add port data to Fabric Data Ptr\n");
					continue;
				}

				if (nodeRec->NodeInfo.NodeType == STL_NODE_SW
						&& portRec->RID.PortNum == 0) {
					// do not map switch port zero because it doesn't connect to other
					// devices
					continue;
				}

				uint64 key = get_hash(portRec->PortInfo.LocalPortId,
						TINY_STR_ARRAY_SIZE);
				cl_map_obj_t *mapObj = create_map_obj(pPortData);
				if (mapObj) {
					cl_qmap_insert(portIdMap, key, &(mapObj->item));
				} else {
					fprintf(stderr, "ERROR - failed to create map obj.\n");
					fstatus = HMGT_STATUS_INSUFFICIENT_MEMORY;
					//break;
				}

				mapObj = create_map_obj(pPortData);
				if (mapObj) {
					cl_qmap_insert(&ifIndexMap, portRec->PortInfo.LID,
							&(mapObj->item));
				} else {
					fprintf(stderr, "ERROR - failed to create map obj.\n");
					fstatus = HMGT_STATUS_INSUFFICIENT_MEMORY;
					//break;
				}
			}
		}
	}
	if (pFabric->flags & FF_STATS) {
		fstatus = populate_port_counters(res, &ifIndexMap);
	}
	cleanup_map(&ifIndexMap);

done:
	if (fstatus != HMGT_STATUS_SUCCESS) {
		fprintf(stderr,
				"ERROR - processing snmp data for %s failed! status=%d\n",
				host->name, fstatus);
		// free all data
		free_node_list(nodeList);
		nodeList = NULL;
	} else {
		// free node and port records that we no longer need
		free_records(nodeList);
	}
	time_print("[%s] Finished data process\n", host->name);

	return nodeList;
}

/*
 * This routine
 * 1) creates a map between NodeGUID and RawData from given node lists
 * 2) walks through each node list, and for each node, iterates its port map
 *    to find the neighbor of each port with benefits from the node and port
 *    maps. And then add the link into fabric.
 */
HMGT_STATUS_T process_fab_data(QUICK_LIST **allNodes, int numHosts,
		FabricData_t *pFabric) {
	TRACEPRINT("process_fab_data\n");
	HMGT_STATUS_T fstatus = HMGT_STATUS_SUCCESS;
	QUICK_LIST *nodeList = NULL;
	LIST_ITEM *nodeItem = NULL;
	RAW_NODE *rawNode = NULL;
	RAW_NODE *nbrRawNode = NULL;
	NodeData *nodeData = NULL;
	cl_qmap_t *portMap = NULL;
	cl_map_item_t *mItem = NULL;
	PortData *portData = NULL;
	PortData *nbrPortData = NULL;
	int i = 0;

	// create a map between NodeGUID and RAW_NODE
	cl_qmap_t nodeMap;
	cl_qmap_init(&nodeMap, NULL);
	for (i = 0; i < numHosts; i++) {
		nodeList = allNodes[i];
		if (nodeList == NULL) {
			continue;
		}
		for (nodeItem = QListHead(nodeList); nodeItem != NULL; nodeItem =
				QListNext(nodeList, nodeItem)) {
			rawNode = nodeItem->pObject;
			nodeData = rawNode->nodeData;
			if (!rawNode->nodeData) {
				// skip invalid nodes
				continue;
			}
			TRACEPRINT("Create Node Map - Add node with IfAddr 0x%016"PRIx64"\n",
					nodeData->NodeInfo.NodeGUID);
			cl_map_obj_t *mapObj = create_map_obj(rawNode);
			if (mapObj) {
				cl_qmap_insert(&nodeMap, nodeData->NodeInfo.NodeGUID,
						&(mapObj->item));
			} else {
				fprintf(stderr, "ERROR - failed to create map obj.\n");
				fstatus = HMGT_STATUS_INSUFFICIENT_MEMORY;
				//break;
			}
		}
	}


	// figure out links and add them into fabric data
	for (i = 0; i < numHosts; i++) {
		nodeList = allNodes[i];
		if (nodeList == NULL) {
			continue;
		}
		for (nodeItem = QListHead(nodeList); nodeItem != NULL; nodeItem =
				QListNext(nodeList, nodeItem)) {
			rawNode = nodeItem->pObject;
			nodeData = rawNode->nodeData;
			if (!nodeData || nodeData->NodeInfo.NodeType == STL_NODE_FI) {
				// invalid node or HFI node that has no neighbor info, skip.
				continue;
			}
			portMap = &rawNode->portIdMap;
			TRACEPRINT("Processing Node 0x%016"PRIx64"\n",
					nodeData->NodeInfo.NodeGUID);
			for (mItem = cl_qmap_head(portMap); mItem != cl_qmap_end(portMap);
					mItem = cl_qmap_next(mItem)) {
				portData = cl_qmap_obj(
						PARENT_STRUCT(mItem, cl_map_obj_t, item));
				if (nodeData->NodeInfo.NodeType == STL_NODE_SW
						&& portData->PortNum == 0) {
					// skip switch port zero
					continue;
				}
				if (portData == NULL) {
					// shouldn't happen
					fprintf(stderr, "ERROR - port data is null\n");
					fstatus = HMGT_STATUS_PARTIALLY_PROCESSED;
					continue;
				}

				uint64 nbrGuid = portData->PortInfo.NeighborNodeGUID;
				TRACEPRINT(
						"  PortId=%s NeighborNodeGUID=0x%016"PRIx64" NeighborPortId=%s\n",
						portData->PortInfo.LocalPortId, nbrGuid,
						portData->PortInfo.NeighborPortId);
				nbrRawNode = get_map_obj(&nodeMap, nbrGuid);
				if (nbrRawNode == NULL) {
					if (portData->PortInfo.NeighborNodeGUID)
						fprintf(stderr,
							"WARNING - Couldn't find neighbor node nodeGuid=0x%016"PRIx64", portId=%s\n",
							nbrGuid, portData->PortInfo.NeighborPortId);
					fstatus = HMGT_STATUS_PARTIALLY_PROCESSED;
					continue;
				}

				uint64 portKey = get_hash(portData->PortInfo.NeighborPortId,
						TINY_STR_ARRAY_SIZE);
				nbrPortData = get_map_obj(&nbrRawNode->portIdMap, portKey);
				if (nbrPortData == NULL) {
					fprintf(stderr,
							"ERROR - Couldn't find PortData for PortId %s from Node with NodeGuid=0x%016"PRIx64"\n",
							portData->PortInfo.NeighborPortId, nbrGuid);
					fstatus = HMGT_STATUS_PARTIALLY_PROCESSED;
					continue;
				}

				if (portData->PortInfo.NeighborPortIdSubtype != nbrPortData->PortInfo.LocalPortIdSubtype) {
					fprintf(stderr,
							"ERROR - Unmacthed PortID Subtype. Expected %d, got %d\n",
							portData->PortInfo.NeighborPortIdSubtype,
							nbrPortData->PortInfo.LocalPortIdSubtype);
				}

				// set neighbor info
				if (nbrRawNode->nodeData->NodeInfo.NodeType == STL_NODE_FI) {
					nbrPortData->PortInfo.PortNeighborMode.NeighborNodeType =
							STL_NEIGH_NODE_TYPE_SW;;
					nbrPortData->PortInfo.NeighborNodeGUID =
							nodeData->NodeInfo.NodeGUID;
					snprintf((char *)nbrPortData->PortInfo.NeighborPortId,
							TINY_STR_ARRAY_SIZE, "%s",
							(char *)portData->PortInfo.LocalPortId);
					nbrPortData->PortInfo.NeighborPortNum = portData->PortNum;
				}
				portData->PortInfo.NeighborPortNum = nbrPortData->PortNum;

				TRACEPRINT("  Add Link\n");
				// add "new" link to fabric data
				if (portData->neighbor != nbrPortData || nbrPortData->neighbor != portData) {
					fstatus = FabricDataAddLink(pFabric, portData, nbrPortData);
				}
			}
		}
	}

	return fstatus;
}

void cleanup_dev_data(QUICK_LIST *nodeList) {
	free_node_list(nodeList);
}

HMGT_STATUS_T hmgt_snmp_get_fabric_data(struct hmgt_port *port,
		HMGT_QUERY *pQuery, struct _HQUERY_RESULT_VALUES **ppQR)
{
	HMGT_STATUS_T status = HMGT_STATUS_SUCCESS;
	uint32_t memSize, recSize, hostEntries;
	FabricData_t *pFabric = pQuery->InputValue.FabricDataRecord.FabricDataPtr;

	if (!pFabric)
		return HMGT_STATUS_INVALID_PARAMETER;

	// Populate the hosts array with nodes specified in the hosts and switches configuration
	// files.
	SNMPHost *hosts = NULL;
	if ((status = init_hosts(pFabric, &hosts, &hostEntries)))
		goto done;

	// Note: the order of the OIDs matters. How to process data of a later OID
	// may depend on the data processing of a previous OID. E.g. we figure out
	// device type based on lldpLocSysCapEnabled and then process data
	// differently. Changing OID order may impact performance or even break the
	// code.
	// TODO: improve to maintain query results in a map rather than list. This
	//       will break the dependency mentioned above. And may slightly improve
	//       performance as well.
	SNMPOid sw_oids_full[] = { ifNumber, lldpLocSysCapEnabled, lldpLocSysCapSupported,
			lldpLocChassisId, lldpLocManAddrIfId, lldpRemChassisId,
			lldpRemPortIdSubtype, lldpRemPortId, lldpRemSysName,
			lldpRemSysCapEnabled, lldpLocPortIdSubtype, lldpLocPortId,
			sysObjectID, sysName,
			ifIndex, ifName, ifType, ifMTU, ifSpeed,
			ifPhysAddress, ifOperStatus, ifInDiscards, ifInErrors,
			ifInUnknownProtos, ifOutDiscards, ifOutErrors,
			ipAdEntIfIndex,
			dot3StatsSingleCollisionFrames, dot3StatsMultipleCollisionFrames,
			dot3StatsSQETestErrors, dot3StatsDeferredTransmissions,
			dot3StatsLateCollisions, dot3StatsExcessiveCollisions,
			dot3StatsCarrierSenseErrors, dot3HCStatsAlignmentErrors,
			dot3HCStatsFCSErrors, dot3HCStatsInternalMacTransmitErrors,
			dot3HCStatsFrameTooLongs, dot3HCStatsInternalMacReceiveErrors,
			dot3HCStatsSymbolErrors,
			ifMauStatus, ifMauMediaAvailable, ifMauTypeListBits,
			ifMauAutoNegAdminStatus,
			ifHCInOctets, ifHCInUcastPkts, ifHCInMulticastPkts, ifHCOutOctets,
			ifHCOutUcastPkts, ifHCOutMulticastPkts, ifHighSpeed,
			entPhysicalClass, entPhysicalDescr,
			entPhysicalHardwareRev, entPhysicalFirmwareRev,
			entPhysicalSerialNum, entPhysicalMfgName, entPhysicalModelName,
			{ NULL } };
	SNMPOid sw_oids_basic[] = { ifNumber, lldpLocSysCapEnabled, lldpLocSysCapSupported,
			lldpLocChassisId, lldpLocManAddrIfId, lldpRemChassisId,
			lldpRemPortIdSubtype, lldpRemPortId, lldpRemSysName,
			lldpRemSysCapEnabled, lldpLocPortIdSubtype, lldpLocPortId,
			sysObjectID, sysName,
			ifIndex, ifName, ifType, ifMTU, ifSpeed,
			ifPhysAddress, ifOperStatus, ipAdEntIfIndex,
			ifMauStatus, ifMauMediaAvailable, ifMauTypeListBits,
			ifMauAutoNegAdminStatus, ifHighSpeed,
			entPhysicalClass, entPhysicalDescr,
			entPhysicalHardwareRev, entPhysicalFirmwareRev,
			entPhysicalSerialNum, entPhysicalMfgName, entPhysicalModelName,
			{ NULL } };
	// net-snmp on host doesn't support LLDP, EtherLike, MAU and Entity
	SNMPOid nic_oids_full[] = {
//	                lldpLocSysCapEnabled, lldpLocSysCapSupported,
//			lldpLocChassisId, lldpLocManAddrIfId, lldpRemChassisId,
//			lldpRemPortIdSubtype, lldpRemPortId, lldpRemSysName,
//			lldpRemSysCapEnabled, lldpLocPortIdSubtype, lldpLocPortId,
			ifNumber, sysObjectID, sysName,
			ifIndex, ifName, ifType, ifMTU, ifSpeed,
			ifPhysAddress, ifOperStatus, ifInDiscards, ifInErrors,
			ifInUnknownProtos, ifOutDiscards, ifOutErrors,
			ipAdEntIfIndex,
//			dot3StatsSingleCollisionFrames, dot3StatsMultipleCollisionFrames,
//			dot3StatsSQETestErrors,
			dot3StatsDeferredTransmissions,
//			dot3StatsLateCollisions, dot3StatsExcessiveCollisions,
			dot3StatsCarrierSenseErrors,
//			dot3HCStatsAlignmentErrors,
//			dot3HCStatsFCSErrors, dot3HCStatsInternalMacTransmitErrors,
//			dot3HCStatsFrameTooLongs, dot3HCStatsInternalMacReceiveErrors,
//			dot3HCStatsSymbolErrors,
//			ifMauStatus, ifMauMediaAvailable, ifMauTypeListBits,
//			ifMauAutoNegAdminStatus,
			ifHCInOctets, ifHCInUcastPkts, ifHCInMulticastPkts, ifHCOutOctets,
			ifHCOutUcastPkts, ifHCOutMulticastPkts, ifHighSpeed,
//			entPhysicalClass, entPhysicalDescr,
//			entPhysicalHardwareRev, entPhysicalFirmwareRev,
//			entPhysicalSerialNum, entPhysicalMfgName, entPhysicalModelName,
			{ NULL } };
	SNMPOid nic_oids_basic[] = {
//	                lldpLocSysCapEnabled, lldpLocSysCapSupported,
//			lldpLocChassisId, lldpLocManAddrIfId, lldpRemChassisId,
//			lldpRemPortIdSubtype, lldpRemPortId, lldpRemSysName,
//			lldpRemSysCapEnabled, lldpLocPortIdSubtype, lldpLocPortId,
			ifNumber, sysObjectID, sysName,
			ifIndex, ifName, ifType, ifMTU, ifSpeed,
			ifPhysAddress, ifOperStatus, ipAdEntIfIndex,
//			ifMauStatus, ifMauMediaAvailable, ifMauTypeListBits,
//			ifMauAutoNegAdminStatus,
			ifHighSpeed,
//			entPhysicalClass, entPhysicalDescr,
//			entPhysicalHardwareRev, entPhysicalFirmwareRev,
//			entPhysicalSerialNum, entPhysicalMfgName, entPhysicalModelName,
			{ NULL } };
	if (pFabric->flags & FF_STATS) {
		status = collect_data(hosts, sw_oids_full, nic_oids_full, hostEntries,
				(void* (*)(SNMPHost*, SNMPResult*, FabricData_t*)) process_dev_data,
				(HMGT_STATUS_T (*)(void**, int, FabricData_t*)) process_fab_data,
				(HMGT_STATUS_T (*)(void*)) cleanup_dev_data,
				pFabric);
	} else {
		status = collect_data(hosts, sw_oids_basic, nic_oids_basic, hostEntries,
				(void* (*)(SNMPHost*, SNMPResult*, FabricData_t*)) process_dev_data,
				(HMGT_STATUS_T (*)(void**, int, FabricData_t*)) process_fab_data,
				(HMGT_STATUS_T (*)(void*)) cleanup_dev_data,
				pFabric);
	}

	recSize = sizeof(HPN_FABRICDATA_RECORD);
	memSize = recSize;
	memSize += sizeof (uint32_t); 
	memSize += sizeof (QUERY_RESULT_VALUES);

	// ResultDataSize should be 0 when status is not successful and no data is returned
	*ppQR = MemoryAllocate2AndClear(memSize, IBA_MEM_FLAG_PREMPTABLE, SNMPTAG);
	if (!(*ppQR)) {
		status = HMGT_STATUS_INSUFFICIENT_MEMORY;
		TRACEPRINT("SNMP fabric query failed to allocate result: %d\n", status);
		goto done;
	}

	(*ppQR)->Status = status;
	(*ppQR)->PacketStatus = 0;
	(*ppQR)->ResultDataSize = recSize;
	*((uint32_t*)((*ppQR)->QueryResult)) = 1;
	HPN_FABRICDATA_RECORD_RESULT *pFDR =
			(HPN_FABRICDATA_RECORD_RESULT *) (*ppQR)->QueryResult;
	pFDR->FabricDataRecord.FabricData = pFabric;

done:
	return status;
}
