/* BEGIN_ICS_COPYRIGHT2 ****************************************

Copyright (c) 2015-2018, Intel Corporation

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

#ifndef __HPNMGT_SNMP_H__
#define __HPNMGT_SNMP_H__

#ifdef __cplusplus
extern "C" {
#endif

//opamgt includes
#include "hpnmgt.h"
#include <iba/stl_sd.h>

struct context_s {
	struct snmp_session *sess; /* SNMP session data */
	SNMPHost *host; /* host information */
	SNMPOid *current_oid; /* How far in our poll are we */
	SNMPResult *result; /* A chain of query results in this session */
	SNMPResult *resultTail; /* last element in the result chain */
	int ifNumber; /* Number of interfaces */
	snmp_device_data_process processor; /* function that processors the SNMPResult data */
	void *populated_data; /* data generated from processor */
	FabricData_t *fabric; /* the fabric data */
};
int active_hosts; /* hosts that we have not completed */
typedef enum {
	Q_NONE, Q_NEXT, Q_END_NEXT, Q_WARN, Q_ERROR
} QueryState;
typedef enum {
	P_UNKNOWN_ENTRY, P_SUCCESS, P_ERROR
} ProcessState;

SNMPOid lldpLocChassisId = { ".1.0.8802.1.1.2.1.3.2.0", SNMP_MSG_GET, {0}, 0 };
SNMPOid lldpLocSysName = { ".1.0.8802.1.1.2.1.3.3.0", SNMP_MSG_GET, {0}, 0 };
SNMPOid lldpLocSysCapSupported = { ".1.0.8802.1.1.2.1.3.5.0", SNMP_MSG_GET, {0}, 0 };
SNMPOid lldpLocSysCapEnabled = { ".1.0.8802.1.1.2.1.3.6.0", SNMP_MSG_GET, {0}, 0 };

SNMPOid lldpLocPortEntry = { ".1.0.8802.1.1.2.1.3.7.1", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid lldpLocPortIdSubtype = {".1.0.8802.1.1.2.1.3.7.1.2", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid lldpLocPortId = { ".1.0.8802.1.1.2.1.3.7.1.3", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid lldpLocPortDesc = { ".1.0.8802.1.1.2.1.3.7.1.4", SNMP_MSG_GETNEXT, {0}, 0 };

SNMPOid lldpLocManAddrIfId = { ".1.0.8802.1.1.2.1.3.8.1.5", SNMP_MSG_GETNEXT, {0}, 0 };

SNMPOid lldpRemEntry = { ".1.0.8802.1.1.2.1.4.1.1", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid lldpRemChassisId = { ".1.0.8802.1.1.2.1.4.1.1.5", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid lldpRemPortIdSubtype = { ".1.0.8802.1.1.2.1.4.1.1.6", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid lldpRemPortId = { ".1.0.8802.1.1.2.1.4.1.1.7", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid lldpRemSysName = { ".1.0.8802.1.1.2.1.4.1.1.9", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid lldpRemSysCapEnabled = { ".1.0.8802.1.1.2.1.4.1.1.12", SNMP_MSG_GETNEXT, {0}, 0 };

SNMPOid sysObjectID = { ".1.3.6.1.2.1.1.2.0", SNMP_MSG_GET, {0}, 0 };
SNMPOid sysName = { ".1.3.6.1.2.1.1.5.0", SNMP_MSG_GET, {0}, 0 };
SNMPOid ifNumber = { ".1.3.6.1.2.1.2.1.0", SNMP_MSG_GET, {0}, 0 };
SNMPOid ifIndex = { ".1.3.6.1.2.1.2.2.1.1", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid ifDescr = { ".1.3.6.1.2.1.2.2.1.2", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid ifType = { ".1.3.6.1.2.1.2.2.1.3", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid ifMTU = { ".1.3.6.1.2.1.2.2.1.4", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid ifSpeed = { ".1.3.6.1.2.1.2.2.1.5", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid ifPhysAddress = { ".1.3.6.1.2.1.2.2.1.6", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid ifOperStatus = { ".1.3.6.1.2.1.2.2.1.8", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid ifInDiscards = {".1.3.6.1.2.1.2.2.1.13", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid ifInErrors = {".1.3.6.1.2.1.2.2.1.14", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid ifInUnknownProtos = {".1.3.6.1.2.1.2.2.1.15", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid ifOutDiscards = {".1.3.6.1.2.1.2.2.1.19", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid ifOutErrors = {".1.3.6.1.2.1.2.2.1.20", SNMP_MSG_GETNEXT, {0}, 0};

SNMPOid ipAdEntIfIndex = { ".1.3.6.1.2.1.4.20.1.2", SNMP_MSG_GETNEXT, {0}, 0 };

SNMPOid dot3StatsSingleCollisionFrames = { ".1.3.6.1.2.1.10.7.2.1.4", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid dot3StatsMultipleCollisionFrames = { ".1.3.6.1.2.1.10.7.2.1.5", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid dot3StatsSQETestErrors = { ".1.3.6.1.2.1.10.7.2.1.6", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid dot3StatsDeferredTransmissions = { ".1.3.6.1.2.1.10.7.2.1.7", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid dot3StatsLateCollisions = { ".1.3.6.1.2.1.10.7.2.1.8", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid dot3StatsExcessiveCollisions = { ".1.3.6.1.2.1.10.7.2.1.9", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid dot3StatsCarrierSenseErrors = { ".1.3.6.1.2.1.10.7.2.1.11", SNMP_MSG_GETNEXT, {0}, 0 };

SNMPOid dot3HCStatsAlignmentErrors = { ".1.3.6.1.2.1.10.7.11.1.1", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid dot3HCStatsFCSErrors = { ".1.3.6.1.2.1.10.7.11.1.2", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid dot3HCStatsInternalMacTransmitErrors = { ".1.3.6.1.2.1.10.7.11.1.3", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid dot3HCStatsFrameTooLongs = { ".1.3.6.1.2.1.10.7.11.1.4", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid dot3HCStatsInternalMacReceiveErrors = { ".1.3.6.1.2.1.10.7.11.1.5", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid dot3HCStatsSymbolErrors = { ".1.3.6.1.2.1.10.7.11.1.6", SNMP_MSG_GETNEXT, {0}, 0 };

//SNMPOid dot1dBasePortIfIndex = { ".1.3.6.1.2.1.17.1.4.1.2", SNMP_MSG_GETNEXT, {0}, 0 };

SNMPOid ifMauStatus = { ".1.3.6.1.2.1.26.2.1.1.4", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid ifMauMediaAvailable = { ".1.3.6.1.2.1.26.2.1.1.5", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid ifMauTypeListBits = { ".1.3.6.1.2.1.26.2.1.1.13", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid ifMauAutoNegAdminStatus = { ".1.3.6.1.2.1.26.5.1.1.1", SNMP_MSG_GETNEXT, {0}, 0 };

SNMPOid ifName = {".1.3.6.1.2.1.31.1.1.1.1", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid ifHCInOctets = {".1.3.6.1.2.1.31.1.1.1.6", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid ifHCInUcastPkts = {".1.3.6.1.2.1.31.1.1.1.7", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid ifHCInMulticastPkts = {".1.3.6.1.2.1.31.1.1.1.8", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid ifHCOutOctets = {".1.3.6.1.2.1.31.1.1.1.10", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid ifHCOutUcastPkts = {".1.3.6.1.2.1.31.1.1.1.11", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid ifHCOutMulticastPkts = {".1.3.6.1.2.1.31.1.1.1.12", SNMP_MSG_GETNEXT, {0}, 0};
SNMPOid ifHighSpeed = { ".1.3.6.1.2.1.31.1.1.1.15", SNMP_MSG_GETNEXT, {0}, 0 };

SNMPOid entPhysical = { ".1.3.6.1.2.1.47.1.1", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid entPhysicalDescr = { ".1.3.6.1.2.1.47.1.1.1.1.2", SNMP_MSG_GETNEXT, {0}, 0 };
//SNMPOid entPhysicalVendorType =
//		{ ".1.3.6.1.2.1.47.1.1.1.1.3", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid entPhysicalClass = { ".1.3.6.1.2.1.47.1.1.1.1.5", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid entPhysicalHardwareRev =
		{ ".1.3.6.1.2.1.47.1.1.1.1.8", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid entPhysicalFirmwareRev =
		{ ".1.3.6.1.2.1.47.1.1.1.1.9", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid entPhysicalSerialNum =
		{ ".1.3.6.1.2.1.47.1.1.1.1.11", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid entPhysicalMfgName = { ".1.3.6.1.2.1.47.1.1.1.1.12", SNMP_MSG_GETNEXT, {0}, 0 };
SNMPOid entPhysicalModelName =
		{ ".1.3.6.1.2.1.47.1.1.1.1.13", SNMP_MSG_GETNEXT, {0}, 0 };

SNMPOid *LLDPOids[] = { &lldpLocChassisId, &lldpLocSysName,
	&lldpLocSysCapSupported, &lldpLocSysCapEnabled,
	&lldpLocPortEntry, &lldpLocPortIdSubtype,
	&lldpLocPortId, &lldpLocPortDesc, &lldpLocManAddrIfId, &lldpRemEntry,
	&lldpRemChassisId, &lldpRemPortIdSubtype, &lldpRemPortId, &lldpRemSysName,
	&lldpRemSysCapEnabled, &sysObjectID, &sysName, &ifNumber, &ifIndex, &ifDescr, &ifType,
	&ifMTU, &ifSpeed, &ifPhysAddress, &ifOperStatus, &ifInDiscards, &ifInErrors,
	&ifInUnknownProtos, &ifOutDiscards, &ifOutErrors, &ipAdEntIfIndex,
	&dot3StatsSingleCollisionFrames, &dot3StatsMultipleCollisionFrames,
	&dot3StatsSQETestErrors, &dot3StatsDeferredTransmissions,
	&dot3StatsLateCollisions, &dot3StatsExcessiveCollisions,
	&dot3StatsCarrierSenseErrors, &dot3HCStatsAlignmentErrors,
	&dot3HCStatsFCSErrors, &dot3HCStatsInternalMacTransmitErrors,
	&dot3HCStatsFrameTooLongs, &dot3HCStatsInternalMacReceiveErrors,
	&dot3HCStatsSymbolErrors,
	&ifMauStatus, &ifMauMediaAvailable, &ifMauTypeListBits,
	&ifMauAutoNegAdminStatus, &ifName, &ifHCInOctets, &ifHCInUcastPkts,
	&ifHCInMulticastPkts, &ifHCOutOctets, &ifHCOutUcastPkts,
	&ifHCOutMulticastPkts, &ifHighSpeed, &entPhysical,
	&entPhysicalClass, &entPhysicalDescr, &entPhysicalHardwareRev,
	&entPhysicalFirmwareRev, &entPhysicalSerialNum,
	&entPhysicalMfgName, &entPhysicalModelName,
	NULL};

#ifdef __cplusplus
}
#endif

#endif /* __HPNMGT_SNMP_H__ */
