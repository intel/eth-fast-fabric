# BEGIN_ICS_COPYRIGHT8 ****************************************
#
# Copyright (c) 2015-2024, Intel Corporation
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
# Makefile for LINUX

# Include Make Control Settings
include $(TL_DIR)/$(PROJ_FILE_DIR)/Makesettings.project

#=============================================================================#
# Definitions:
#-----------------------------------------------------------------------------#

# Name of SubProjects
DS_SUBPROJECTS	= 
# name of executable or downloadable image
EXECUTABLE		= #LINUX$(EXE_SUFFIX)
# list of sub directories to build
DIRS			= 
# C files (.c)
CFILES			= \
				# Add more c files here
# C++ files (.cpp)
CCFILES			= \
				# Add more cpp files here
# lex files (.lex)
LFILES			= \
				# Add more lex files here
# archive library files (basename, $ARFILES will add MOD_LIB_DIR/prefix and suffix)
LIBFILES = 
# Windows Resource Files (.rc)
RSCFILES		=
# Windows IDL File (.idl)
IDLFILE			=
# Windows Linker Module Definitions (.def) file for dll's
DEFFILE			=
# targets to build during INCLUDES phase (add public includes here)
INCLUDE_TARGETS	= \
				# Add more h hpp files here
# Non-compiled files
MISC_FILES		=  
				# Add more files here
# all source files
SOURCES			= $(CFILES) $(CCFILES) $(LFILES) $(RSCFILES) $(IDLFILE)

ifeq "$(BUILD_TARGET_OS_VENDOR)" "ubuntu"
	UTIL_PKG = $(TL_DIR)/CommonInstall/util_deb.pl
else
	UTIL_PKG = $(TL_DIR)/CommonInstall/util_rpm.pl
endif
INSTALL_SOURCES= \
				 $(TL_DIR)/CommonInstall/util_init.pl \
				 $(TL_DIR)/CommonInstall/util_basicio.pl \
				 $(TL_DIR)/CommonInstall/util_basicfile.pl \
				 $(TL_DIR)/CommonInstall/util_configfile.pl \
				 $(TL_DIR)/CommonInstall/util_startup.pl \
				 $(TL_DIR)/CommonInstall/util_driver.pl \
				 $(TL_DIR)/CommonInstall/util_components.pl \
				 $(TL_DIR)/CommonInstall/util_blacklist.pl \
				 $(TL_DIR)/CommonInstall/util_ifcfg.pl \
				 $(TL_DIR)/CommonInstall/util_linuxconfig.pl \
				 $(TL_DIR)/CommonInstall/util_hosttools.pl \
                                 $(UTIL_PKG) \
				 $(TL_DIR)/CommonInstall/util_build.pl \
				 $(TL_DIR)/CommonInstall/ofed_firmware.pl \
				 $(TL_DIR)/CommonInstall/comp_oftools.pl \
				 $(TL_DIR)/CommonInstall/comp_fastfabric.pl \
				 $(TL_DIR)/CommonInstall/overrides_ff.pl \
				 $(TL_DIR)/CommonInstall/main_omnipathwrap_delta.pl

MAJOR = $(shell echo ${BUILD_PLATFORM_OS_VENDOR_VERSION_MAJOR} | cut -c 3-)
ifeq "$(BUILD_PLATFORM_OS_VENDOR)" "redhat"
	PREREQ_SOURCE = $(TL_DIR)/CommonInstall/comp_ff_of_prereq_RHEL$(MAJOR).pl
else ifeq "$(BUILD_PLATFORM_OS_VENDOR)" "SuSE"
	PREREQ_SOURCE = $(TL_DIR)/CommonInstall/comp_ff_of_prereq_SLES$(MAJOR).pl
else ifeq "$(BUILD_PLATFORM_OS_VENDOR)" "ubuntu"
	PREREQ_SOURCE = $(TL_DIR)/CommonInstall/comp_ff_of_prereq_UBUNTU$(MAJOR).pl
else
	$(error BUILD_PLATFORM_OS_VENDOR not valid: $(BUILD_PLATFORM_OS_VENDOR))
endif

# ignore prereq if the file specified in PREREQ_SOURCE doesn't exist.
# This is mainly for Fedora that we treat it as redhat, but it has
# totally different version number.
# TBD, find a better solution for this case. Maybe we shall get rid of
# prereq files.
ifeq (,$(wildcard ${PREREQ_SOURCE}))
	PREREQ_SOURCE =
endif

COMP_SOURCES= \
				 $(TL_DIR)/CommonInstall/comp_oftools.pl \
				 $(TL_DIR)/CommonInstall/comp_fastfabric.pl
# Source files to include in DSP File
DSP_SOURCES		= $(INCLUDE_TARGETS) $(SOURCES) $(MISC_FILES) \
				  $(RSCFILES) $(DEFFILE) $(MAKEFILE) 
# all object files
OBJECTS			= $(CFILES:.c=$(OBJ_SUFFIX)) $(CCFILES:.cpp=$(OBJ_SUFFIX)) \
				  $(LFILES:.lex=$(OBJ_SUFFIX))
RSCOBJECTS		= $(RSCFILES:.rc=$(RES_SUFFIX))
# targets to build during LIBS phase
LIB_TARGETS_IMPLIB	=
LIB_TARGETS_ARLIB	= # $(LIB_PREFIX)LINUX$(ARLIB_SUFFIX)
LIB_TARGETS_EXP		= $(LIB_TARGETS_IMPLIB:$(ARLIB_SUFFIX)=$(EXP_SUFFIX))
LIB_TARGETS_MISC	= 
# targets to build during CMDS phase
CMD_TARGETS_SHLIB	= 
CMD_TARGETS_EXE		= $(EXECUTABLE)
CMD_TARGETS_MISC	= ethcapture INSTALL.pl comp.pl
# files to remove during clean phase
CLEAN_TARGETS_MISC	=  
CLEAN_TARGETS		= $(OBJECTS) $(RSCOBJECTS) $(IDL_TARGETS) $(CLEAN_TARGETS_MISC)
# other files to remove during clobber phase
CLOBBER_TARGETS_MISC= ethcapture.sh
# sub-directory to install to within bin
BIN_SUBDIR		= 
# sub-directory to install to within include
INCLUDE_SUBDIR		=

# Additional Settings
#CLOCALDEBUG	= User defined C debugging compilation flags [Empty]
#CCLOCALDEBUG	= User defined C++ debugging compilation flags [Empty]
#CLOCAL	= User defined C flags for compiling [Empty]
#CCLOCAL	= User defined C++ flags for compiling [Empty]
#BSCLOCAL	= User flags for Browse File Builder [Empty]
#DEPENDLOCAL	= user defined makedepend flags [Empty]
#LINTLOCAL	= User defined lint flags [Empty]
#LOCAL_INCLUDE_DIRS	= User include directories to search for C/C++ headers [Empty]
#LDLOCAL	= User defined C flags for linking [Empty]
#IMPLIBLOCAL	= User flags for Object Lirary Manager [Empty]
#MIDLLOCAL	= User flags for IDL compiler [Empty]
#RSCLOCAL	= User flags for resource compiler [Empty]
#LOCALDEPLIBS	= User libraries to include in dependencies [Empty]
#LOCALLIBS		= User libraries to use when linking [Empty]
#				(in addition to LOCALDEPLIBS)
#LOCAL_LIB_DIRS	= User library directories for libpaths [Empty]

LOCALDEPLIBS = 

# Include Make Rules definitions and rules
include $(TL_DIR)/$(PROJ_FILE_DIR)/Makerules.project

#=============================================================================#
# Overrides:
#-----------------------------------------------------------------------------#
#CCOPT			=	# C++ optimization flags, default lets build config decide
#COPT			=	# C optimization flags, default lets build config decide
#SUBSYSTEM = Subsystem to build for (none, console or windows) [none]
#					 (Windows Only)
#USEMFC	= How Windows MFC should be used (none, static, shared, no_mfc) [none]
#				(Windows Only)
#=============================================================================#

#=============================================================================#
# Rules:
#-----------------------------------------------------------------------------#
# process Sub-directories
include $(TL_DIR)/Makerules/Maketargets.toplevel

# build cmds and libs
include $(TL_DIR)/Makerules/Maketargets.build

# install for includes, libs and cmds phases
include $(TL_DIR)/Makerules/Maketargets.install

# to avoid conflicts with the INSTALL pseudo target in make system build as .pl
# and rename during STAGE
INSTALL.pl: $(INSTALL_SOURCES)
	cat $(INSTALL_SOURCES) > INSTALL.pl
	chmod +x INSTALL.pl

comp.pl: $(PREREQ_SOURCE)
	cat $(PREREQ_SOURCE) $(COMP_SOURCES) > comp.pl
	chmod +x comp.pl

# install for stage phase
#include $(TL_DIR)/Makerules/Maketargets.stage
STAGE:
	$(VS)$(STAGE_INSTALL) $(STAGE_INSTALL_DIR_OPT) $(PROJ_STAGE_DIR) INSTALL.pl
	cd $(PROJ_STAGE_DIR) ; mv INSTALL.pl INSTALL
	$(VS)$(STAGE_INSTALL) $(STAGE_INSTALL_DIR_OPT) $(PROJ_STAGE_DIR) comp.pl
	$(VS)$(STAGE_INSTALL) $(STAGE_INSTALL_DIR_OPT) $(PROJ_STAGE_BIN_DIR) ethcapture
	
# TBD - should build an rpm here
# can't use STAGE_SUB_DIR since RELEASE_TAG may not be set and it is used to
# put version suffix on STAGE_SUB_DIR
# TBD - make this a script, the wildcards for IntelEth-Tools.* are a problem
# packaging should be filelist based, especially for release media which will
# be a subset of things found in stage
# TBD - make some of these "make variables" so this can be a simple sequence
# of commands and make will test exit codes for us


# TBD - make some of these "make variables" so this can be a simple sequence
# of commands and make will test exit codes for us

DIR_ARCH := $(shell echo $(BUILD_TARGET) | tr '[A-Z]' '[a-z]')

ifeq "$(BUILD_TARGET_OS_VENDOR)" "ubuntu"
	RPM_ARCH := $(subst ia32,i386,$(subst em64t,amd64,$(subst x86_64,amd64,$(DIR_ARCH))))
	RPMDIR := $(TL_DIR)/debbuild
	RPM_BUILD_DIR := .
	SRPMSDIR := $(RPM_BUILD_DIR)
	RPMSDIR := $(RPM_BUILD_DIR)
	DEVEL := dev
	SEPARATOR := _
	DEBUGINFO := dbgsym
	SRPM_EXT := {.tar.gz,.dsc,_$(RPM_ARCH).changes}
	RPM_EXT := _$(RPM_ARCH).deb
	DEBUG_EXT := _$(RPM_ARCH).ddeb
	EXT := deb
	pkg_arch = _$(DEB_TARGET_ARCH)
	pkg_ver = _$${MKRPM_VER}
	BIN_OUTPUT_DIR := DEBS
	SRC_OUTPUT_DIR := SDEBS
else
	RPM_ARCH := $(DIR_ARCH)
	RPMDIR := $(TL_DIR)/rpmbuild
	RPM_BUILD_DIR := BUILD
	SRPMSDIR := SRPMS
	RPMSDIR := RPMS/$(RPM_ARCH)
	DEVEL := devel
	SEPARATOR := -
	DEBUGINFO := debuginfo
	SRPM_EXT := .src.rpm
	RPM_EXT := .$(RPM_ARCH).rpm
	DEBUG_EXT := $(RPM_EXT)
	EXT := rpm
	pkg_arch = .`echo $(BUILD_TARGET) | tr '[A-Z]' '[a-z]'`
	pkg_ver = -$${MKRPM_VER}
	BIN_OUTPUT_DIR := RPMS
	SRC_OUTPUT_DIR := SRPMS
endif

package:
	mkdir -p $(RELEASE_DIR)
	rm -f packaged_files dist_files
	cd $(STAGE_DIR) ; \
	dir=`echo IntelEth-Tools-FF.*`; \
	ver=`expr IntelEth-Tools-FF.* : 'IntelEth-Tools-FF.\(.*\)$$'`; \
	arch=$(pkg_arch); \
	vers=$(pkg_ver); \
	rm -rf $(RELEASE_DIR)/IntelEth-Tools-FF.$$ver ; \
	mkdir -p $(RELEASE_DIR)/IntelEth-Tools-FF.$$ver ; \
	cp -r $$dir/arch $$dir/distro $$dir/distro_version $$dir/os_id $(RELEASE_DIR)/IntelEth-Tools-FF.$$ver 2>/dev/null; \
	cp -r $$dir/INSTALL $$dir/comp.pl $$dir/README $$dir/version $(RELEASE_DIR)/IntelEth-Tools-FF.$$ver ; \
	echo "$(RELEASE_DIR)/IntelEth-Tools-FF.$$ver.tgz" >> $(TL_DIR)/packaged_files; \
	echo "$(RELEASE_DIR)/IntelEth-Tools-FF.$$ver.tgz" >> $(TL_DIR)/dist_files; \
	echo "$(RELEASE_DIR)/IntelEth-Tools.$$ver.tgz" >> $(TL_DIR)/packaged_files; \
	echo "$(RELEASE_DIR)/IntelEth-Tools.$$ver.tgz" >> $(TL_DIR)/dist_files; \
	if [ "$(OPA_FEATURE_SET)" != "opa10" ]; then \
		echo "$(RELEASE_DIR)/opa-snapconfig$${vers}-$${MKRPM_REL}$${arch}.$(EXT)" >> $(TL_DIR)/packaged_files; \
		echo "$(RELEASE_DIR)/opa-snapconfig$${vers}-$${MKRPM_REL}$${arch}.$(EXT)" >> $(TL_DIR)/dist_files; \
	fi; \

# when build in Fedora38, EXPORT_ALL_VARIABLES can hang our build
# so we change to directly export the vars we need
#.EXPORT_ALL_VARIABLES:
final_package:
	RELEASE_DIR="$(RELEASE_DIR)" \
	RPMDIR="$(RPMDIR)" \
	RPM_BUILD_DIR="$(RPM_BUILD_DIR)" \
	MKRPM_VER="$(MKRPM_VER)" \
	BIN_OUTPUT_DIR="$(BIN_OUTPUT_DIR)" \
	DIR_ARCH="$(DIR_ARCH)" \
	SRC_OUTPUT_DIR="$(SRC_OUTPUT_DIR)" \
	SRPMSDIR="$(SRPMSDIR)" \
	SEPARATOR="$(SEPARATOR)" \
	MKRPM_REL="$(MKRPM_REL)" \
	SRPM_EXT="$(SRPM_EXT)" \
	RPMSDIR="$(RPMSDIR)" \
	RPM_EXT="$(RPM_EXT)" \
	DEBUGINFO="$(DEBUGINFO)" \
	DEBUG_EXT="$(DEBUG_EXT)" \
	BUILD_TARGET_OS_VENDOR="$(BUILD_TARGET_OS_VENDOR)" \
	GZIP="$(GZIP)" \
	./final_package.sh
# Unit test execution
#include $(TL_DIR)/Makerules/Maketargets.runtest

# Unit test execution
#include $(TL_DIR)/Makerules/Maketargets.runtest

#=============================================================================#

#=============================================================================#
# DO NOT DELETE THIS LINE -- make depend depends on it.
#=============================================================================#
