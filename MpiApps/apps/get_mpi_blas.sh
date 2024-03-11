#!/bin/bash
# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015-2023, Intel Corporation
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

#[ICS VERSION STRING: unknown]
#!/bin/bash
#
# Sets the BLAS_TYPE variable to match the installed BLAS library.
#
# Usage:
#
# source get_mpi_blas.sh
#
# -- or --
#
# export MYVAR=`get_mpi_blas.sh`

if [[ -z "${BLAS_TYPE}" ]]; then

	openblasrpm="$(rpm -qa | grep -i openblas)"

	if [ -e "$MPICH_PREFIX/bin/tune" ] || [ -e $MPICH_PREFIX/bin/impi_info ] #IntelMPI
	then
		# If we are using Intel MPI, prefer the Intel MKL but
		# fallback to OpenBLAS if MKL is not found or if IMPI
		# is using an unsupported C compiler.
		#
		# By default, IMPI uses gcc, make that explicit.
		cc=${I_MPI_CC:-${MPICH_CC:-gcc}}

		if [[ -n "${MKLROOT}" && "${cc}" == "icc" ]]; then 
			export BLAS_TYPE="MKL-icc"
		elif [[ -n "${MKLROOT}" && "${cc}" == "gcc" ]]; then 
			export BLAS_TYPE="MKL-gcc"
		elif [[ -n "${openblasrpm}" ]]; then
			export BLAS_TYPE="OPENBLAS"
		else
			export BLAS_TYPE="UNKNOWN_MPI_CC"
		fi
	elif [[ -n "${openblasrpm}" ]]; then
		export BLAS_TYPE="OPENBLAS"
	else
		export BLAS_TYPE="UNKNOWN_MPI_CC"
	fi
fi

echo $BLAS_TYPE
