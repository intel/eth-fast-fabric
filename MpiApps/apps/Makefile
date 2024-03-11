# BEGIN_ICS_COPYRIGHT8 ****************************************
#
# Copyright (c) 2015-2022, Intel Corporation
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
# 	full    - build all targets.
#	eth-base- build applications provivded in eth-tools-fastfabric package
# 	quick   - build commonly used apps. (superset of eth-base)
# 	all     - synonym for quick
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
#	STRESS (mpi_stress)
#	IMB (v2021.3)
#	OSU (v5.9)
#	XHPL2 (hpl v2)
#
ifndef OSU_VER
export OSU_VER="5.9"
endif

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
ifeq "$(MPI_APPS_CUDA)" "y"
ifndef CUDA_DIR
CUDA_DIR=/usr/local/cuda
endif
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
ifeq "$(MPI_APPS_CUDA)" "y"
	cd mpi_stress;\
	$(MPICC) -DMPI_STRESS_CUDA -I$(MPI_INCLUDE_DIR) -I$(CUDA_DIR)/include -L$(CUDA_DIR)/lib64 -o mpi_stress mpi_stress.c -lcuda -lcudart -lz;
else ifeq "$(MPI_APPS_ONEAPI)" "y"
	cd mpi_stress;\
	$(MPICC) -DMPI_STRESS_ONEAPI -I$(MPI_INCLUDE_DIR) -o mpi_stress mpi_stress.c -lze_loader -lz;
else
	cd mpi_stress;\
	$(MPICC) -I$(MPI_INCLUDE_DIR) -o mpi_stress mpi_stress.c -lz;
endif
	@echo
.PHONY: STRESS

ifeq "$(MPI_APPS_CUDA)" "y"
OSU_CONFIG=--enable-cuda --with-cuda=$(CUDA_DIR)
OSU_PATH="$(PATH):$(CUDA_DIR)/bin"
else
OSU_CONFIG=
OSU_PATH="$(PATH)"
endif
OSU: SHOWMPI
	@echo "Building OSU $(OSU_VER) benchmarks..."
	cd osu-micro-benchmarks-$(OSU_VER); \
		find . -name aclocal.m4 |xargs -r touch; \
		find . -name Makefile.in |xargs -r touch; \
		find . -name configure |xargs -r touch
	cd osu-micro-benchmarks-$(OSU_VER); \
	export PATH=$(OSU_PATH); \
	if [[ "$(REAL_MPICC)" == *"mpiicc"* || "$(I_MPI_CC)" == "icc" || "$(MPICH_CC)" == "icc" || "$(MPICH_PREFIX)" == *"intel/openmpi"* ]]; then \
		# intel compiler \
		./configure $(OSU_CONFIG) CC=$(REAL_MPICC) CXX=$(REAL_MPICXX) LIBS="-lirc -lsvml"; make; \
	else \
		# gcc compiler \
		./configure $(OSU_CONFIG) CC=$(REAL_MPICC) CXX=$(REAL_MPICXX); make; \
	fi
	@echo
.PHONY: OSU

IMB: SHOWMPI
	@echo "Building Intel Micro Benchmarks..."
ifeq "$(MPI_APPS_ONEAPI)" "y"
	cd imb; make CC=$(MPICC) CXX=$(MPICXX) all IMB-MPI1-GPU
else
	cd imb; make CC=$(MPICC) CXX=$(MPICXX) all
endif
	@echo

.PHONY: IMB

HPL_VER=hpl-2.3
XHPL2: SHOWMPI
	export BLAS_TYPE=`MPICH_PREFIX=$(MPICH_PREFIX) ./get_mpi_blas.sh`; \
	echo "Using BLAS_TYPE=$$BLAS_TYPE"; \
	if [[ $$BLAS_TYPE == UNKNOWN_MPI_CC ]]; then \
		echo ; \
		echo "WARNING: No BLAS library found. Skipping HPL."; \
	else \
		echo "Building HPL (Linpack)..."; \
		export HPL_HOME_DIR=$(PWD)/$(HPL_VER); \
		cd $(HPL_VER); \
		if [[ "$(MPICH_PREFIX)" == *"oneapi"* ]]; then \
			# Intel MPI \
			if [[ "$(REAL_MPICC)" == *"mpiicc"* || "$(I_MPI_CC)" == "icc" || "$(MPICH_CC)" == "icc" ]]; then \
				# IntelMPI & icc \
				make HPL_OPTS="-lirc" arch=ICS.`uname -s`.$$BLAS_TYPE; \
			else \
				# IntelMPI & gcc \
				make arch=ICS.`uname -s`.$$BLAS_TYPE; \
			fi \
		elif [[ "$(MPICH_PREFIX)" == *"intel/openmpi"* ]]; then \
			# intel compiler with OpenMPI \
			make HPL_OPTS="-lirc" LINKFLAGS=-nofor_main arch=ICS.`uname -s`.$$BLAS_TYPE; \
		else \
			# OpenMPI with gcc compiler \
			make arch=ICS.`uname -s`.$$BLAS_TYPE; \
		fi \
	fi
	@echo
.PHONY: XHPL2

cleandepend clean:: clobber
.PHONY: cleandepend clean

clean_hpl:
	if [ -d $(HPL_VER) ]; then find $(HPL_VER) -name ICS.`uname -s`.* | xargs rm -rf; rm -rf $(HPL_VER)/bin $(HPL_VER)/lib; fi
	if [ -d $(HPL_VER) ]; then find $(HPL_VER) -name xerbla.o | xargs rm -rf; fi
	if [ -d pgfile_test ]; then make -C pgfile_test clean; fi

clean_osu:
	if [ -f osu-micro-benchmarks-$(OSU_VER)/Makefile ]; then make -C osu-micro-benchmarks-$(OSU_VER) clean; rm -f osu-micro-benchmarks-$(OSU_VER)/Makefile; fi

clobber:: clean_hpl clean_osu
	rm -f latency/*.o latency/*.d latency/latency
	rm -f bandwidth/*.o bandwidth/*.d bandwidth/bw
	if [ -d "imb" ]; then cd imb; make clean; fi
	make -C mpicheck clean;
	make -C deviation clean;
	make -C groupstress clean;
	rm -f mpi_multibw/mpi_multibw mpi_multibw/*.o
	rm -f mpi_stress/mpi_stress mpi_stress/*.o
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

quick:: eth-base BW LAT MULTIBW OSU IMB STRESS XHPL2
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
