/* BEGIN_ICS_COPYRIGHT2 ****************************************

Copyright (c) 2015-2017, Intel Corporation

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

#ifndef __HPNMGT_PRIV_H__
#define __HPNMGT_PRIV_H__

#include <syslog.h>
/* Needed for getpid() */
#include <sys/types.h>
#include <unistd.h>
#include "hpnmgt.h"

#include "iba/ib_types.h"

#define HMGT_STL_OUI    (0x66A)

#include "iba/ib_generalServices.h"

#ifndef HMGT_OUTPUT_ERROR
#define HMGT_OUTPUT_ERROR(port, format, args...) \
	do { \
		FILE *current_log_file = port ? port->error_file : stderr; \
		if (port && current_log_file) { \
			if (current_log_file == HMGT_DBG_FILE_SYSLOG) { \
				syslog(LOG_ERR, "hpnmgt ERROR: [%d] %s: " format, \
					(int)getpid(), __func__, ##args); \
			} else { \
				fprintf(current_log_file, "hpnmgt ERROR: [%d] %s: " format, \
					(int)getpid(), __func__, ##args); \
			} \
		} \
	} while(0)
#endif

/* NOTE we keep this at LOG_INFO and reserve LOG_DEBUG for packet dump */
#ifndef HMGT_DBGPRINT
#define HMGT_DBGPRINT(port, format, args...) \
	do { \
		FILE *current_log_file = port ? port->dbg_file : NULL; \
		if (port && current_log_file) { \
			if (current_log_file == HMGT_DBG_FILE_SYSLOG) { \
				syslog(LOG_INFO, "hpnmgt: [%d] %s: " format, \
				    (int)getpid(), __func__, ##args); \
			} else { \
				fflush(current_log_file); fprintf(current_log_file, "hpnmgt: [%d] %s: " format, \
				    (int)getpid(), __func__, ##args); \
			} \
		} \
	} while(0)
#endif

#ifndef HMGT_OUTPUT_INFO
#define HMGT_OUTPUT_INFO(port, format, args...) \
	do { \
		FILE *current_log_file = port ? port->dbg_file : NULL; \
		if (port && current_log_file) { \
			if (current_log_file == HMGT_DBG_FILE_SYSLOG) { \
				syslog(LOG_INFO, "hpnmgt: [%d] %s: " format, \
				    (int)getpid(), __func__, ##args); \
			} else { \
				fprintf(current_log_file, "hpnmgt: [%d] %s: " format, \
				    (int)getpid(), __func__, ##args); \
			} \
		} \
	} while(0)
#endif


#ifndef DBG_ENTER_FUNC
#define DBG_ENTER_FUNC(port)  HMGT_DBGPRINT(port, "Entering %s\n",__func__)
#endif

#ifndef DBG_EXIT_FUNC
#define DBG_EXIT_FUNC(port)  HMGT_DBGPRINT(port, "Exiting %s\n",__func__)
#endif

struct hmgt_port {
//	int                  hfi_num;
//	char                 hfi_name[IBV_SYSFS_NAME_MAX];
//	uint8_t              hfi_port_num;
//	int                  umad_fd;
//	int                  umad_agents[OMGT_MAX_CLASS_VERSION][OMGT_MAX_CLASS];
//	struct ibv_context  *verbs_ctx;
//
//	/* from omgt_sa_registry_t object */
//	struct ibv_sa_event_channel *channel;
//	omgt_sa_registration_t       *regs_list;
//	SEMAPHORE                    lock;
//
//	/* cache of "port details" */
//	SEMAPHORE   umad_port_cache_lock;
//	umad_port_t umad_port_cache;
//	int         umad_port_cache_valid;
//	pthread_t   umad_port_thread;
//	int         umad_port_sv[2];

	/* Logging */
	FILE *dbg_file;
	FILE *error_file;

	/* Timeout & Retries */
	int ms_timeout;
	int retry_count;

//	/* SA interaction for userspace Notice registration */
//	struct ibv_comp_channel *sa_qp_comp_channel;
//	struct ibv_cq           *sa_qp_cq;
//	struct ibv_pd           *sa_qp_pd;
//	struct ibv_qp           *sa_qp;
//	struct ibv_ah           *sa_ah;
//	uint32_t                 next_tid; /* 32 bits only */
//	int                      run_sa_cq_poll;
//	int                      poll_timeout_ms;
//
//	int                      num_userspace_recv_buf;
//	int                      num_userspace_send_buf;
//	int                      outstanding_sends_cnt;
//	struct omgt_sa_msg       pending_reg_msg_head;
//	struct omgt_sa_msg      *recv_bufs;
//
//	/* For SA Client interface */
//	uint16_t                 sa_mad_status;
	int                      sa_service_state;
//	uint32_t                 sa_capmask2;
//
//	/* For PA/EA client interface */
//	IB_GID              local_gid;
//	FILE               *verbose_file;
//	int                 pa_verbose;
	int                 pa_service_state;
//	STL_LID             primary_pm_lid;
//	uint8_t             primary_pm_sl;
//	uint16_t            pa_mad_status;
//
//	int                 ea_service_state;
//	STL_LID             primary_em_lid;
//	uint8_t             primary_em_sl;
//
//	/* For OOB client interface */
//	boolean                is_oob_enabled; /* Is port in FE OOB mode */
//	struct net_connection *conn;
//	struct omgt_oob_input  oob_input;
//	/* For OOB Notice interface */
//	boolean                is_oob_notice_setup;
//	struct net_connection *notice_conn;
//	/* For OOB SSL */
//	boolean             is_ssl_enabled; /* Is SSL enabled */
//	boolean             is_ssl_initialized;
//	void               *ssl_context;
//	const SSL_METHOD   *ssl_client_method;
//	boolean             is_x509_store_initialized;
//	X509_STORE         *x509_store;
//	boolean             is_dh_params_initialized;
//	DH                 *dh_params;
	void 		   *config_file_params;
};

#endif /* __HPNMGT_PRIV_H__ */
