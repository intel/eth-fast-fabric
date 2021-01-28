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

# Makefile for MPI application/benchmark source for end user system
# targets:
# 	full 	- build all targets.
#   basic   - build all applications provivded in opa-fastfabric package
# 	quick 	- build commonly used apps.
# 	all		- synonym for quick
# 	clobber - remove all built files.
#
# 	The following targets build the matching applications. If the
# 	application name differs from the target name, it is listed
# 	in parens.
#
# 	GROUPSTRESS	
# 	DEVIATION
# 	LAT (latency)
#	BW (bandwidth)
#	MULTIBW (multi-pair bandwidth)
#	IMB
#	OSU563 (version 5.6.3 of the OSU benchmarks)
#	XHPL2 (hpl v2)
#

ifndef MPICH_PREFIX
export MPICH_PREFIX=$(shell . ./select_mpi 2>/dev/null; echo $$MPICH_PREFIX)
endif

# set MPICC to our own mpicc wrapper that will change the real mpicc to use RPATH
# so that our MPI Apps will stick with the mpi we selected during build.
# if you want the default RUNPATH, you can set MPICC before calling the Makefile
ifndef MPICC
export MPICC=$(PWD)/mpicc
endif
export REAL_MPICC=$(MPICH_PREFIX)/bin/mpicc
ifndef MPIF77
export MPIF77=$(PWD)/mpif77
endif
export REAL_MPIF77=$(MPICH_PREFIX)/bin/mpif77
ifndef MPICXX
export MPICXX=$(PWD)/mpicxx
endif
export REAL_MPICXX=$(MPICH_PREFIX)/bin/mpicxx
ifndef MPI_INCLUDE_DIR
export MPI_INCLUDE_DIR=$(MPICH_PREFIX)/include
endif
ifndef MPI_LIB_DIR
export MPI_LIB_DIR=/lib
endif

all:: INCLUDES LIBS CMDS
.PHONY: all

CHECK: SHOWMPI
	@echo "Building MPI Check..."
	export HOME_DIR=$(PWD); \
	make -C mpicheck
	@echo
.PHONY: CHECK

GROUPSTRESS: SHOWMPI
	@echo "Building groupstress..."
	export HOME_DIR=$(PWD); \
	make -C groupstress
	@echo
.PHONY: GROUPSTRESS
DEVIATION: SHOWMPI
	@echo "Building deviation..."
	export HOME_DIR=$(PWD); \
	make -C deviation
	@echo
.PHONY: DEVIATION

LAT: SHOWMPI
	@echo "Building OSU latency..."
	cd latency;\
	$(MPICC) -I$(MPI_INCLUDE_DIR) -o latency latency.c;
	@echo
.PHONY: LAT

BW: SHOWMPI
	@echo "Building OSU bandwidth..."
	cd bandwidth;\
	$(MPICC) -I$(MPI_INCLUDE_DIR) -o bw bw.c;
	@echo
.PHONY: BW

MULTIBW: SHOWMPI
	@echo "Building multi bandwidth..."
	cd mpi_multibw;\
	$(MPICC) -I$(MPI_INCLUDE_DIR) -o mpi_multibw mpi_multibw.c;
	@echo
.PHONY: MUTLIBW

STRESS: SHOWMPI
	@echo "Building mpi_stress..."
	cd mpi_stress;\
	$(MPICC) -I$(MPI_INCLUDE_DIR) -o mpi_stress mpi_stress.c -lz;
	@echo
.PHONY: STRESS

OSU563: SHOWMPI
	@echo "Building OSU 5.6.3 benchmarks..."
	cd osu-micro-benchmarks-5.6.3; \
		find . -name aclocal.m4 |xargs -r touch; \
		find . -name Makefile.in |xargs -r touch
	cd osu-micro-benchmarks-5.6.3; \
	if [[ "$(REAL_MPICC)" == *"mpiicc"* || "$(I_MPI_CC)" == "icc" || "$(MPICH_CC)" == "icc" || "$(MPICH_PREFIX)" == *"intel/openmpi"* ]]; then \
		# intel compiler \
		./configure CC=$(REAL_MPICC) CXX=$(REAL_MPICXX) LIBS="-lirc"; make; \
	else \
		# gcc compiler \
		./configure CC=$(REAL_MPICC) CXX=$(REAL_MPICXX); make; \
	fi
	@echo
.PHONY: OSU563

IMB: SHOWMPI
	@echo "Building Intel Micro Benchmarks..."
	cd imb; make CC=$(MPICC) CXX=$(MPICXX)
	@echo
.PHONY: IMB

HPL_VER=hpl-2.3
XHPL2: SHOWMPI
	@echo "Building HPL (Linpack)..."
	export HPL_HOME_DIR=$(PWD)/$(HPL_VER); \
	cd $(HPL_VER); \
	if [[ "$(MPICH_PREFIX)" == *"intel/openmpi"* ]]; then \
		# intel compiler with OpenMPI \
		make MPICC=$(MPICC) HPL_OPTS="-lirc" LINKFLAGS=-nofor_main arch=ICS.`uname -s`.`../get_mpi_blas.sh`; \
	elif [[ "$(MPICC)" == *"mpiicc"* || "$(I_MPI_CC)" == "icc" || "$(MPICH_CC)" == "icc" ]]; then \
		# intel compiler with IntelMPI \
		make MPICC=$(MPICC) HPL_OPTS="-lirc" arch=ICS.`uname -s`.`../get_mpi_blas.sh`; \
	else \
		# gcc compiler \
		make MPICC=$(MPICC) arch=ICS.`uname -s`.`../get_mpi_blas.sh`; \
	fi
	@echo
.PHONY: XHPL2

cleandepend clean:: clobber
.PHONY: cleandepend clean

clobber::
	rm -f latency/*.o latency/*.d latency/latency
	rm -f bandwidth/*.o bandwidth/*.d bandwidth/bw
	if [ -d "imb" ]; then cd imb; make clean; fi
	if [ -d $(HPL_VER) ]; then find $(HPL_VER) -name ICS.`uname -s`.* | xargs rm -rf; rm -rf $(HPL_VER)/bin $(HPL_VER)/lib; fi
	if [ -d $(HPL_VER) ]; then find $(HPL_VER) -name xerbla.o | xargs rm -rf; fi
	if [ -d pgfile_test ]; then make -C pgfile_test clean; fi
	if [ -d tachyon ]; then rm -rf tachyon/compile/*; fi
	if [ -d mandel ]; then make -C mandel clean; rm -f mandel/*.d; fi
	make -C mpicheck clean;
	make -C deviation clean;
	make -C groupstress clean;
	rm -f mpi_multibw/mpi_multibw mpi_multibw/*.o
	rm -f mpi_stress/mpi_stress mpi_stress/*.o
	if [ -f osu-micro-benchmarks-5.6.3/Makefile ]; then make -C osu-micro-benchmarks-5.6.3 clean; rm -f osu-micro-benchmarks-5.6.3/Makefile; fi
	rm -f .prefix
.PHONY: clobber

SHOWMPI:
	@echo "Using MPICH_PREFIX=$(MPICH_PREFIX)"
	echo "$(MPICH_PREFIX)" > .prefix
.PHONY: SHOWMPI

depend::
force::
INCLUDES::
# do the real work during LIBS
LIBS:: 
.PHONY: depend force INCLUDES LIBS

eth-base:: DEVIATION CHECK GROUPSTRESS
	@echo "build base sample applications"
.PHONY: eth-base

quick:: BW LAT MULTIBW OSU563 IMB XHPL2 STRESS eth-base
	@echo "Built subset of sample applications"
.PHONY: quick

CMDS:: quick
	@echo "Built sample applications"
.PHONY: CMDS

full:: CMDS
	@echo "Built full set of sample applications"
.PHONY: full

STAGE::
INSTALL::

RUNTEST::
build_dsfiles::
clobber_dsfiles::
lint::
cflow::

.PHONY: STAGE INSTALL RUNTEST build_dsfiles clobber_dsfiles lint cflow
