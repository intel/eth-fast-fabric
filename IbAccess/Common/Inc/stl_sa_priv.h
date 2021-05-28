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

#ifndef __STL_SA_PRIV_H__
#define __STL_SA_PRIV_H__

#include "iba/stl_mad_priv.h"
#include "iba/stl_sa_types.h"
#include "iba/ib_sa_records_priv.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * If STL LID falls within the IB unicast LID range,
 * just return the LID. If it falls outside the range
 * return the last 14 bits of the LID
 */
#define STL_TO_IB_UCAST_LID(x) ((uint16) (((x) > IB_MAX_UCAST_LID) ?\
					  ((x) & 0x3FFF) : (x)))
#define MAKE_OPA_GID_IID(lid) (((uint64)OUI_TRUESCALE << 40) | (uint64)(lid))

#if defined (__cplusplus)
}
#endif
#endif //__STL_SA_PRIV_H__

