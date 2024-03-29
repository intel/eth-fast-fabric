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

#ifndef __IBA_STL_PM_H__
#define __IBA_STL_PM_H__

#include "iba/ib_generalServices.h"
#include "iba/stl_types.h"
#include "iba/stl_pa_types.h"
#include "iba/stl_helper.h"

#if defined (__cplusplus)
extern "C" {
#endif

#include "iba/public/ipackon.h"

/*
 * Defines 
 */

#define STL_PM_CLASS_VERSION					0x80 	/* Performance Management version */

#define STL_PM_ATTRIB_ID_CLASS_PORTINFO			0x01
#define STL_PM_ATTRIB_ID_PORT_STATUS			0x40
#define STL_PM_ATTRIB_ID_CLEAR_PORT_STATUS		0x41
#define STL_PM_ATTRIB_ID_DATA_PORT_COUNTERS		0x42
#define STL_PM_ATTRIB_ID_ERROR_PORT_COUNTERS	0x43
#define STL_PM_ATTRIB_ID_ERROR_INFO				0x44


/* if AllPortSelect capability, use this as PortNum to operate on all ports */
#define PM_ALL_PORT_SELECT 0xff

enum PM_VLs {
	PM_VL0 = 0,
	PM_VL1 = 1,
	PM_VL2 = 2,
	PM_VL3 = 3,
	PM_VL4 = 4,
	PM_VL5 = 5,
	PM_VL6 = 6,
	PM_VL7 = 7,
	PM_VL15 = 8,
	MAX_PM_VLS = 9
};
static __inline uint32 vl_to_idx(uint32 vl) {
	DEBUG_ASSERT(vl < PM_VL15 || vl == 15);
	if (vl >= PM_VL15) return (uint32)PM_VL15;
	return vl;
}
static __inline uint32 idx_to_vl(uint32 idx) {
	DEBUG_ASSERT(idx < MAX_PM_VLS);
	if (idx >= PM_VL15) return 15;
	return idx;
}

/* PortXmitData and PortRcvData are in units of flits (8 bytes) */
#define FLITS_PER_MiB ((uint64)1024*(uint64)1024/(uint64)8)
#define FLITS_PER_MB ((uint64)1000*(uint64)1000/(uint64)8)


/* MAD status codes for PM */
#define STL_PM_STATUS_REQUEST_TOO_LARGE			0x0100	/* response can't fit in a single MAD */
#define STL_PM_STATUS_NUMBLOCKS_INCONSISTENT	0x0200	/* attribute modifier number of blocks inconsistent with number of ports/blocks in MAD */
#define STL_PM_STATUS_OPERATION_FAILED			0x0300	/* an operation (such as clear counters) requested of the agent failed */


static __inline void
StlPmClassPortInfoCapMask(char buf[80], uint16 cmask)
{
	if (!cmask) {
		snprintf(buf, 80, "-");
	} else {

		snprintf(buf, 80, "%s%s%s",
			(cmask & STL_CLASS_PORT_CAPMASK_TRAP) ? "Trap " : "",
			(cmask & STL_CLASS_PORT_CAPMASK_NOTICE) ? "Notice " : "",
			(cmask & STL_CLASS_PORT_CAPMASK_CM2) ? "CapMask2 " : "");
	}
}
static __inline void
StlPmClassPortInfoCapMask2(char buf[80], uint32 cmask)
{
	if (!cmask) {
		snprintf(buf, 80, "-");
	} else {
		buf[0] = '\0';
	}
}
static __inline const char *
StlPmMadMethodToText(uint8 method) {
	switch (method) {
	case MMTHD_GET: return "Get";
	case MMTHD_SET: return "Set";
	case MMTHD_GET_RESP: return "GetResp";
	default: return "Unknown";
	}
}
static __inline const char *
StlPmMadAttributeToText(uint16 aid) {
	switch (aid) {
	case STL_PM_ATTRIB_ID_CLASS_PORTINFO: return "ClassPortInfo";
	case STL_PM_ATTRIB_ID_PORT_STATUS: return "PortStatus";
	case STL_PM_ATTRIB_ID_CLEAR_PORT_STATUS: return "ClearPortStatus";
	case STL_PM_ATTRIB_ID_DATA_PORT_COUNTERS: return "DataPortCounters";
	case STL_PM_ATTRIB_ID_ERROR_PORT_COUNTERS: return "ErrorportCounters";
	case STL_PM_ATTRIB_ID_ERROR_INFO: return "ErrorInfo";
	default: return "Unknown";
	}
}

/* MAD structure definitions */
typedef struct _STL_PERF_MAD {
	MAD_COMMON	common;				/* Generic MAD Header */

	uint8		PerfData[STL_GS_DATASIZE];	/* Performance Management Data */
} PACK_SUFFIX STL_PERF_MAD, *PSTL_PERF_MAD;


/* STL Port Counters - small request, large response */
typedef struct _STL_Port_Status_Req {
	uint8	PortNumber;
	uint8	Reserved[3];;
	uint32	VLSelectMask;
} PACK_SUFFIX STLPortStatusReq, STL_PORT_STATUS_REQ;

typedef struct _STL_Port_Status_Rsp {
	uint8	PortNumber;
	uint8	Reserved[3];
	uint32	VLSelectMask;
	uint64	PortXmitData;
	uint64	PortRcvData;
	uint64	PortXmitPkts;
	uint64	PortRcvPkts;
	uint64	PortMulticastXmitPkts;
	uint64	PortMulticastRcvPkts;
	uint64	PortXmitWait;
	uint64	SwPortCongestion;
	uint64	PortRcvFECN;
	uint64	PortRcvBECN;
	uint64	PortXmitTimeCong;
	uint64	PortXmitWastedBW;
	uint64	PortXmitWaitData;
		
	uint64	PortRcvBubble;
	uint64	PortMarkFECN;
	uint64	PortRcvConstraintErrors;
	uint64	PortRcvSwitchRelayErrors;
	uint64	PortXmitDiscards;
	uint64	PortXmitConstraintErrors;
	uint64	PortRcvRemotePhysicalErrors;
	uint64	LocalLinkIntegrityErrors;
	uint64	PortRcvErrors;
	uint64	ExcessiveBufferOverruns;
	uint64	FMConfigErrors;
	uint32	LinkErrorRecovery;
	uint32	LinkDowned;
	uint8	UncorrectableErrors;
	union {
		uint8	AsReg8;
		struct { IB_BITFIELD2(uint8,
				Reserved : 5,
				LinkQualityIndicator : 3)
		} PACK_SUFFIX s;
	} lq;
	uint8	Reserved2[6];
	struct _vls_pctrs {
		uint64	PortVLXmitData;
		uint64	PortVLRcvData;
		uint64	PortVLXmitPkts;
		uint64	PortVLRcvPkts;
		uint64	PortVLXmitWait;
		uint64	SwPortVLCongestion;
		uint64	PortVLRcvFECN;
		uint64	PortVLRcvBECN;
		uint64	PortVLXmitTimeCong;
		uint64	PortVLXmitWastedBW;
		uint64	PortVLXmitWaitData;
		uint64	PortVLRcvBubble;
		uint64	PortVLMarkFECN;
		uint64	PortVLXmitDiscards;
	} VLs[1]; /* n defined by number of bits in VLSelectmask */
} PACK_SUFFIX STLPortStatusRsp, STL_PORT_STATUS_RSP;

typedef struct _STL_Clear_Port_Status {
	uint64	PortSelectMask[4];
	CounterSelectMask_t CounterSelectMask;
} PACK_SUFFIX STLClearPortStatus, STL_CLEAR_PORT_STATUS;

/* The adders are added to the resolutions before performing the shift operation */
#define RES_ADDER_LLI 8
#define RES_ADDER_LER 2

/* STL Data Port Counters - small request, bigger response */
typedef struct _STL_Data_Port_Counters_Req {
	uint64	PortSelectMask[4];				/* signifies for which ports the PMA is to respond */
	uint32	VLSelectMask;					/* signifies for which VLs the PMA is to respond */
	union {
		uint32 AsReg32;
		struct { IB_BITFIELD3(uint32,
			Reserved : 24,
			LocalLinkIntegrityResolution : 4,	/* The PMA shall interpret the resolution fields as indicators of how many bits to right-shift */
			LinkErrorRecoveryResolution : 4)	/* the corresponding counters before adding them to the PortErrorCounterSummary */
		} PACK_SUFFIX s;
	} res;
} PACK_SUFFIX STLDataPortCountersReq, STL_DATA_PORT_COUNTERS_REQ;

struct _vls_dpctrs {
	uint64	PortVLXmitData;
	uint64	PortVLRcvData;
	uint64	PortVLXmitPkts;
	uint64	PortVLRcvPkts;
	uint64	PortVLXmitWait;
	uint64	SwPortVLCongestion;
	uint64	PortVLRcvFECN;
	uint64	PortVLRcvBECN;
	uint64	PortVLXmitTimeCong;
	uint64	PortVLXmitWastedBW;
	uint64	PortVLXmitWaitData;
	uint64	PortVLRcvBubble;
	uint64	PortVLMarkFECN;
};
 
struct _port_dpctrs {
	uint8	PortNumber;
	uint8	Reserved[3];
	union {
		uint32 AsReg32;
		struct { IB_BITFIELD2(uint32,
			Reserved : 29,
			LinkQualityIndicator : 3)
		} PACK_SUFFIX s;
	} lq;
	uint64	PortXmitData;
	uint64	PortRcvData;
	uint64	PortXmitPkts;
	uint64	PortRcvPkts;
	uint64	PortMulticastXmitPkts;
	uint64	PortMulticastRcvPkts;
	uint64	PortXmitWait;
	uint64	SwPortCongestion;
	uint64	PortRcvFECN;
	uint64	PortRcvBECN;
	uint64	PortXmitTimeCong;
	uint64	PortXmitWastedBW;
	uint64	PortXmitWaitData;
	uint64	PortRcvBubble;
	uint64	PortMarkFECN;
	uint64	PortErrorCounterSummary;		/* sum of all error counters for port */
	struct _vls_dpctrs VLs[1];							/* actual array size defined by number of bits in VLSelectmask */
};

typedef struct _STL_Data_Port_Counters_Rsp {
	uint64	PortSelectMask[4];
	uint32	VLSelectMask;
	union {
		uint32 AsReg32;
		struct { IB_BITFIELD3(uint32,
			Reserved : 24,
			LocalLinkIntegrityResolution : 4,
			LinkErrorRecoveryResolution : 4)
		} PACK_SUFFIX s;
	} res;
	struct _port_dpctrs Port[1];								/* actual array size defined by number of ports in attribute modifier */
} PACK_SUFFIX STLDataPortCountersRsp, STL_DATA_PORT_COUNTERS_RSP;

/* STL Error Port Counters - small request, bigger response */

typedef struct _STL_Error_Port_Counters_Req {
	uint64	PortSelectMask[4];				/* signifies for which ports the PMA is to respond */
	uint32	VLSelectMask;					/* signifies for which VLs the PMA is to respond */
	uint32	Reserved;
} PACK_SUFFIX STLErrorPortCountersReq, STL_ERROR_PORT_COUNTERS_REQ;

struct _vls_epctrs {
	uint64	PortVLXmitDiscards;
};
struct _port_epctrs {
	uint8	PortNumber;
	uint8	Reserved[7];
	uint64	PortRcvConstraintErrors;
	uint64	PortRcvSwitchRelayErrors;
	uint64	PortXmitDiscards;
	uint64	PortXmitConstraintErrors;
	uint64	PortRcvRemotePhysicalErrors;
	uint64	LocalLinkIntegrityErrors;
	uint64	PortRcvErrors;
	uint64	ExcessiveBufferOverruns;
	uint64	FMConfigErrors;
	uint32	LinkErrorRecovery;
	uint32	LinkDowned;
	uint8	UncorrectableErrors;
	uint8	Reserved2[7];
	struct _vls_epctrs VLs[1];
};
typedef struct _STL_Error_Port_Counters_Rsp {
	uint64	PortSelectMask[4];				/* echo from request */
	uint32	VLSelectMask;					/* echo from request */
	uint32	Reserved;
	struct _port_epctrs Port[1];				/* actual array size defined by number of ports in attribute modifier */
} PACK_SUFFIX STLErrorPortCountersRsp, STL_ERROR_PORT_COUNTERS_RSP;


struct _port_error_info {
	uint8	PortNumber;
	uint8	Reserved[7];

	/* PortRcvErrorInfo */
	struct {
		struct { IB_BITFIELD3(uint8,
			Status : 1,
			Reserved : 3,
			ErrorCode : 4)
		} PACK_SUFFIX s;
		union {
			uint8	AsReg8[17];			/* 136 bits of error info */
			struct {
				uint8	PacketFlit1[8];	/* first 64 bits of flit 1 */
				uint8	PacketFlit2[8];	/* first 64 bits of flit 2 */
				struct { IB_BITFIELD3(uint8,
					Flit1Bits : 1,		/* bit 64 of flit 1 */
					Flit2Bits : 1,		/* bit 64 of flit 2 */
					Reserved : 6)
				} PACK_SUFFIX s;
			} PACK_SUFFIX EI1to12;		/* error info for codes 1-12 */
			struct {
				uint8	PacketBytes[8];	/* first 64 bits of VL Marker flit */
				struct { IB_BITFIELD2(uint8,
					FlitBits : 1,		/* bit 64 of VL Marker flit */
					Reserved : 7)
				} PACK_SUFFIX s;
			} PACK_SUFFIX EI13;			/* error info for code 13 */
		} ErrorInfo;
		uint8 Reserved[6];
	} PACK_SUFFIX PortRcvErrorInfo;

	/* ExcessiveBufferOverrunInfo */
	struct {
		struct { IB_BITFIELD3(uint8,
			Status : 1,
			SC : 5,
			Reserved : 2)
		} PACK_SUFFIX s;
		uint8 Reserved[7];
	} PACK_SUFFIX ExcessiveBufferOverrunInfo;

	/* PortXmitConstraintErrorInfo */
	struct {
		struct { IB_BITFIELD2(uint8,
			Status : 1,
			Reserved : 7)
		} PACK_SUFFIX s;
		uint8	Reserved;
		uint16	P_Key;
		STL_LID	SLID;
	} PACK_SUFFIX PortXmitConstraintErrorInfo;

	/* PortRcvConstraintErrorInfo */
	struct {
		struct { IB_BITFIELD3(uint8,
			Status : 1,
			Reserved : 3,
			ErrorCode: 4)
		} PACK_SUFFIX s;
		uint8	Reserved;
		uint16	P_Key;
		STL_LID	SLID;
	} PACK_SUFFIX PortRcvConstraintErrorInfo;

	/* PortRcvSwitchRelayErrorInfo */
	struct {
		struct { IB_BITFIELD3(uint8,
			Status : 1,
			RC : 3,
			ErrorCode : 4)
		} PACK_SUFFIX s;
		uint8 SLID_23_16;
		uint16 SLID_15_0;
		union {
			uint32	AsReg32;
			struct { IB_BITFIELD2(uint32,
				SC: 8,
				DLID: 24)
			} EI0; 						/* error code 0 */
			struct {
				uint8 EgressPortNum;
				uint8 DLID_23_16;
				uint16 DLID_15_0;
			} EI2; 						/* error code 2 */
			struct {
				uint8	EgressPortNum;
				uint8	SC;
				uint16	Reserved;
			} EI3; 						/* error code 3 */
		} ErrorInfo;
	} PACK_SUFFIX PortRcvSwitchRelayErrorInfo;

	/* UncorrectableErrorInfo */
	struct {
		struct { IB_BITFIELD3(uint8,
			Status : 1,
			Reserved : 3,
			ErrorCode : 4)
		} PACK_SUFFIX s;
		uint8	Reserved;
	} PACK_SUFFIX UncorrectableErrorInfo;

	/* FMConfigErrorInfo */
	struct {
		struct { IB_BITFIELD3(uint8,
			Status : 1,
			Reserved : 3,
			ErrorCode : 4)
		} PACK_SUFFIX s;
		union {
			uint8	AsReg8;
			struct {
				uint8	Distance;
			} EI0to2_8;					/* error code 0-2,8 */
			struct {
				uint8	VL;
			} EI3to5; 					/* error code 3-5 */
			struct {
				uint8	BadFlitBits;	/* bits [63:56] of bad packet */
			} EI6; 						/* error code 6 */
			struct {
				uint8	SC;
			} EI7; 						/* error code 7 */
		} ErrorInfo;
	} PACK_SUFFIX FMConfigErrorInfo;
	uint32	Reserved2;
} PACK_SUFFIX;

typedef union _STL_Error_Info_Mask {
	uint32 AsReg32;
	struct { IB_BITFIELD8(uint32,
		PortRcvErrorInfo : 1,
		ExcessiveBufferOverrunInfo : 1,
		PortXmitConstraintErrorInfo : 1,
		PortRcvConstraintErrorInfo : 1,
		PortRcvSwitchRelayErrorInfo : 1,
		UncorrectableErrorInfo : 1,
		FMConfigErrorInfo : 1,
		Reserved : 25)
	} PACK_SUFFIX s;
} PACK_SUFFIX STLErrorInfoMask_t;

typedef struct _STL_Error_Info_Req {
	uint64	PortSelectMask[4];				/* signifies for which ports the PMA is to respond */
	STLErrorInfoMask_t ErrorInfoSelectMask;
	uint32	Reserved;
} PACK_SUFFIX STLErrorInfoReq, STL_ERROR_INFO_REQ;

typedef struct _STL_Error_Info_Rsp {
	uint64	PortSelectMask[4];				/* signifies for which ports the PMA is to respond */
	STLErrorInfoMask_t ErrorInfoSelectMask;
	uint32	Reserved;
	struct _port_error_info Port[1]; /* x defined by number of ports in attribute modifier */
} PACK_SUFFIX STLErrorInfoRsp, STL_ERROR_INFO_RSP;


#include "iba/public/ipackoff.h"

#if defined (__cplusplus)
};
#endif

#endif /* __IBA_STL_PM_H__ */
