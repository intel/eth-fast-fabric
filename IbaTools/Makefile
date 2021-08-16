# BEGIN_ICS_COPYRIGHT8 ****************************************
#
# Copyright (c) 2015-2020, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Intel Corporation nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# END_ICS_COPYRIGHT8   ****************************************
# Makefile for IbaTools

# Include Make Control Settings
include $(TL_DIR)/$(PROJ_FILE_DIR)/Makesettings.project

DS_SUBPROJECTS	= 
DSP_SOURCES = Makefile

DIRS			= \
				$(shell ls -d disable_ports 2>/dev/null) \
				$(shell ls -d FastFabric 2>/dev/null) \
				$(shell ls -d fabric_info 2>/dev/null) \
				$(shell ls -d setup_self_ssh 2>/dev/null) \
				$(shell ls -d stream 2>/dev/null) \
				$(shell ls -d ethipcalc 2>/dev/null) \
				$(shell ls -d ethgetipaddrtype 2>/dev/null) \
				$(shell ls -d ethmon 2>/dev/null) \
				$(shell ls -d ethreport 2>/dev/null) \
				$(shell ls -d eth2rm 2>/dev/null) \
				$(shell ls -d ethxmlextract 2>/dev/null) \
				$(shell ls -d ethxmlfilter 2>/dev/null) \
				$(shell ls -d ethxmlgenerate 2>/dev/null) \
				$(shell ls -d ethxlattopology 2>/dev/null) \
				$(shell ls -d ethfindgood 2>/dev/null) \
				$(shell ls -d ethlinkanalysis 2>/dev/null) \
				$(shell ls -d ethcabletest 2>/dev/null) \
				$(shell ls -d ethcheckload 2>/dev/null) \
				$(shell ls -d nodeverify 2>/dev/null) \
				$(shell ls -d usemem 2>/dev/null) \
				$(shell ls -d man 2>/dev/null)
# 				$(shell ls -d chassis_setup 2>/dev/null) \
# 				$(shell ls -d switch_setup 2>/dev/null) \
# ifeq "$(IB_STACK)" "OPENIB"
# DIRS			+= \
# 				$(shell ls -d opafirmware 2>/dev/null)  \
# 				$(shell ls -d opahfirev 2>/dev/null) \
# 				$(shell ls -d opagenswitches 2>/dev/null) \
# 				$(shell ls -d opagenesmchassis 2>/dev/null) \
# 				$(shell ls -d opasmaquery 2>/dev/null) \
# 				$(shell ls -d opapaextract 2>/dev/null) \
# 				$(shell ls -d opapacketcapture 2>/dev/null) \
# 				$(shell ls -d opasnapconfig 2>/dev/null) \
# 				$(shell ls -d opatmmtool 2>/dev/null) \
# 				$(shell ls -d opatop 2>/dev/null) \
# 				$(shell ls -d portdown 2>/dev/null) \
# 				$(shell ls -d saquery 2>/dev/null) \
# 				$(shell ls -d opainfo 2>/dev/null) \
# 				$(shell ls -d opasw 2>/dev/null) \
# 				$(shell ls -d opapaquery 2>/dev/null) \
# 				$(shell ls -d oparesolvehfiport 2>/dev/null) \
# 				$(shell ls -d opagenchassis 2>/dev/null)
# endif # OPENIB

include $(TL_DIR)/IbaTools/Makerules.module

include $(TL_DIR)/Makerules/Maketargets.toplevel

# install for includes, libs and cmds phases
include $(TL_DIR)/Makerules/Maketargets.install

# install for stage phase - nothing to stage, internal tools
#include $(TL_DIR)/Makerules/Maketargets.stage
STAGE::

clobber::
	rm -rf modules builtlibs
	
