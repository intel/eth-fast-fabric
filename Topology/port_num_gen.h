/* BEGIN_ICS_COPYRIGHT2 ****************************************

Copyright (c) 2021, Intel Corporation

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

#ifndef __PORT_NUM_GEN_H__
#define __PORT_NUM_GEN_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A port number generator that creates a unique port number from a given port
 * name. It figures out how to convert a name to number from the registered
 * port names. The current implementation is a count based approach that
 * assumes each port name includes port number and the number is unique in each
 * port group. It splits each name into segments where each segment contains
 * either all alphabets or all digits. For a given name, its port number is the
 * digit segment with the lest occurrence. If there are multiple digit segments
 * with the same count (this is very rare if we register all port names), it
 * picks the last segment. Port number confliction may happen when a switch has
 * multiple port groups. The implementation applies a simple conflication
 * resolve approach based on port groups. For ech port name, it treats all the
 * segments except the port number segment as a "group name", and figures out
 * each port group's occurrence and port number range (min, max) from the
 * registered port names. When port number conflication happens, it then tries
 * to align the port groups by occurrence. The group with largest occurrence
 * will be the first group. It's then followed by a group with less count. If
 * two groups have the same count, the group with smaller start port number
 * will be in front of another. The first group is the majority group that no
 * need port number adjustment unless its number starts from ZERO that will
 * introduce an offset of 1 to ensure all port numbers are positive (port ZERO
 * has special meaning in our code, so we must avoid it). Then for the followed
 * group, if it overlaps with previous group, we adjust its port numbers with
 * an offset to continue the port number from previous group. The conflication
 * resolve approach is a very simple solution that works for our switches. If
 * we encounter a case that doesn't work, we can further improve the solution.
 *
 * To use it, please create a pn_gen_t data model, call pn_gen_init to
 * initialize it, then call pn_gen_register to register ALL port names. After
 * that, pn_gen_get_port will return a port name for a given port name. Finally,
 * please call pn_gen_cleanup to release used resources.
 */

#include <iba/ipublic.h>

#define PNG_OK 0
#define PNG_NO_MEMORY 1
#define PNG_ALREADY_PROCESSED 2
#define PNG_INVALID_PORT_NAME 3

/**
 * The data model for port number generation.
 */
typedef struct {
	// segment map contains segment occurrence count
	cl_qmap_t seg_map;
	// port map  contains all ports
	cl_qmap_t port_map;
	// indicate whether the data was processed or not
	boolean processed;
} pn_gen_t;

/**
 * Initialize pn_gen_t that is the data model for port number generation
 */
void pn_gen_init(pn_gen_t* const model);

/**
 * Register a port name into a port number generator described by pn_gen_t
 *
 * Return value indicates states. A value of ZERO means success, otherwise
 * it's an error code that can be
 *  PNG_NO_MEMORY - memory allocation failure
 *  PNG_ALREADY_PROCESSED - registration failed because data was already
 *                          processed, so the model is locked.
 *  PNG_INVALID_PORT_NAME - invalid port name, such as no number in a name
 */
int pn_gen_register(pn_gen_t* const model, char* const port_name);

/**
 * Return port number for a given port name from a port number generator
 * described by pn_gen_t. The port name shall be registered before.
 * Must call this function after all port names are registered. Unexpected port
 * number may return if only partial port names are registered.
 *
 * Return value shall be a positive number. Value ZERO indicates errors, such
 * as a port name never registered.
 */
uint16 pn_gen_get_port(pn_gen_t* const model, char* const port_name);

/**
 * Cleanup a port number generator described by pn_gen_t
 */
void pn_gen_cleanup(pn_gen_t* const model);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_NUM_GEN_H__ */