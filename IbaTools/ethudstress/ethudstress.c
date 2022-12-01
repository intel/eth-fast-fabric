/* BEGIN_ICS_COPYRIGHT7 ****************************************

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

** END_ICS_COPYRIGHT7   ****************************************/

/* [ICS VERSION STRING: unknown] */

//
// to compile: gcc -lpthread -lrdmacm -libverbs ethudstress.c -o ethudstress
//
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/time.h>

#include <rdma/rdma_cma.h>

#define DEBUG 0
#define BASENAME "ethudstress"
#define PRINTHDR(out) {                                        \
	struct timeval curTime;                                \
	struct timezone timeZone;                              \
	struct tm *tm;                                         \
	gettimeofday(&curTime, &timeZone);                     \
	tm = localtime(&curTime.tv_sec);                       \
	if (tm) {                                              \
		fprintf(out, "%s[%s] %.2d:%.2d:%.2d.%.6d ",    \
			BASENAME, src_addr ? src_addr : "-",   \
			tm->tm_hour, tm->tm_min, tm->tm_sec,   \
			(int) curTime.tv_usec);                \
	} else {                                               \
		fprintf(out, "%s[%s] ", BASENAME,              \
			src_addr ? src_addr : "-");            \
	}                                                      \
}
#define PSERROR(str) { PRINTHDR(stderr); fprintf(stderr, "- Error: %s - %s\n", str, strerror(errno)); }
#define PFERROR(format, args...) { PRINTHDR(stderr); fprintf(stderr, "- Error: "); fprintf(stderr, format, ##args); }
#define PFWARN(format, args...) { PRINTHDR(stderr); fprintf(stderr, "- Warning: "); fprintf(stderr, format, ##args); }
#define FPRINT(format, args...) { PRINTHDR(stdout); printf("- "); printf(format, ##args); }
#define DBGPRINT(format, args...) if (DEBUG) { PRINTHDR(stderr); fprintf(stderr, format, ##args); }

#define OP_TIMEOUT_MS 2000 // operation timeout in ms

typedef struct {
	int			id;
	struct rdma_cm_id	*cma_id;
	bool			connected;
	struct ibv_pd		*pd;
	struct ibv_cq		*cq;
	struct ibv_mr		*mr;
	struct ibv_ah		*ah;
	uint32_t		remote_qpn;
	uint32_t		remote_qkey;
	void			*mem;
	// for statistics
	int			rx_pkts;
} connection;

typedef struct {
	struct rdma_event_channel	*channel;
	connection			*conns;
	int				conn_index;
	int				conn_left;
	struct rdma_addrinfo		*addr_info;
} job;

typedef struct {
	 pthread_t	thread;
	 connection	*conn;
	 int		send_flags;
	 int		msg_count;
	 int		ret;
} send_param;

typedef struct {
	pthread_t	thread;
	connection	*conn;
	int		msg_count;
	int		ret;
} poll_param;

typedef struct {
	uint64_t start_us;
	uint64_t end_us;
	long tx_pkts;
	long rx_pkts;
	long lost_rx_pkts;
} perf;

static char *dst_addr;
static char *src_addr;
static char *port = "15234";
static bool set_tos = false;
static uint8_t tos;
static bool srv_reply = false;
static int num_conns = 1;
static int msg_size = 100;
static int msg_count = 10;
static int timeout_s = 30;
static uint64_t stop_time_us;
static uint64_t start_us;
static uint64_t end_us;
static perf receive_perf, reply_perf;

static struct rdma_addrinfo hint;

void usage() {
	fprintf(stderr, "Usage: %s [-b bind_addr] [-B port] [-r] [-c connections]\n", BASENAME);
	fprintf(stderr, "		[-C msg_count] [-S msg_size] [-T timeout_sec]\n");
	fprintf(stderr, "Usage: %s -s server_addr [-b bind_addr] [-B port] [-r] [-t tos]\n", BASENAME);
	fprintf(stderr, "		[-c connections] [-S msg_size] [-T timeout_sec]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Description: Send UD messages from multiple senders to a receiver.\n");
	fprintf(stderr, "    Run this tool as a server on receiver machine, and run this tool as client\n");
	fprintf(stderr, "    on sender machines. The total number of connections from all senders shall\n");
	fprintf(stderr, "    equal the number of connections on a receiver. Both receiver and senders\n");
	fprintf(stderr, "    must have the same message count and message size.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "    -b bind_addr   bind IP address\n");
	fprintf(stderr, "    -B port        port service name/number (default '15234')\n");
	fprintf(stderr, "    -s server_addr server/receiver address\n");
	fprintf(stderr, "    -r             sender sends reply messages\n");
	fprintf(stderr, "    -t tos         type of service (ToS) for this tool. The value can be 0, 8,\n");
	fprintf(stderr, "                   16 or 24 that corresponds to priority 0, 2, 6, 4 respectively.\n");
	fprintf(stderr, "                   See `man tc-prio` for more details.\n");
	fprintf(stderr, "    -c connections number of connections. Default 1.\n");
	fprintf(stderr, "    -C msg_count   number of messages a sender will send out from a connection\n");
	fprintf(stderr, "                   number of messages a receiver will expect from a connection\n");
	fprintf(stderr, "                   Default 10 messages\n");
	fprintf(stderr, "    -S msg_size    message size in bytes. Default 100 bytes\n");
	fprintf(stderr, "    -T timeout_sec execution time out in seconds. Default 30 seconds\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "    One sender sends messages to a receiver with address 192.168.101.1\n");
	fprintf(stderr, "    On receiver: %s\n", BASENAME);
	fprintf(stderr, "    On sender: %s -s 192.168.101.1\n", BASENAME);
	fprintf(stderr, "\n");
	fprintf(stderr, "    Two senders send messages to a receiver with address 192.168.101.1\n");
	fprintf(stderr, "    On receiver: %s -b 192.168.101.1 -c 32 -C 1000 -S 1024\n", BASENAME);
	fprintf(stderr, "    On sender 1: %s -s 192.168.101.1 -c 16 -C 1000 -S 1024\n", BASENAME);
	fprintf(stderr, "    On sender 2: %s -s 192.168.101.1 -c 16 -C 1000 -S 1024\n", BASENAME);
}

uint64_t get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
}

void update_perf(job *the_job, perf *the_perf, long tx_pkts, long expected_rx_pkts) {
	int i;
	the_perf->start_us = start_us;
	the_perf->end_us = end_us;
	the_perf->tx_pkts = tx_pkts;
	the_perf->rx_pkts = 0;
	for (i = 0; i < num_conns; i++) {
		the_perf->rx_pkts += the_job->conns[i].rx_pkts;
		the_job->conns[i].rx_pkts = 0;
	}
	the_perf->lost_rx_pkts = expected_rx_pkts - the_perf->rx_pkts;
}

void print_perf(char* name, perf the_perf) {
	if (!the_perf.start_us) {
		// empty perf. do nothing.
		return;
	}
	uint64_t delta = the_perf.end_us - the_perf.start_us;
	printf("\n---- %s: %s ----\n", src_addr, name);
	printf("Time: %ld ms\n", delta/1000);
	printf("Pkt Size: %d Bytes\n", msg_size);
	printf("Rx: %ld Pkts, Lost: %ld Pkts\n", the_perf.rx_pkts, the_perf.lost_rx_pkts);
	printf("Rx: %ld Bytes\n", the_perf.rx_pkts * msg_size);
	if (the_perf.lost_rx_pkts) {
		printf("Rx Speed: N/A Gb/Sec\n");
		printf("Speed: N/A Gb/Sec\n");
	} else {
		printf("Rx Speed: %5.2f Gb/Sec\n",
			the_perf.rx_pkts * msg_size * 8.0 / (delta * 1000.0));
		printf("Speed: %5.2f Gb/Sec\n",
			(the_perf.rx_pkts + the_perf.tx_pkts) * msg_size * 8.0 / (delta * 1000.0));
	}
	printf("--------------\n\n");
}

connection* create_conns(int num_conns, struct rdma_event_channel *channel) {
	connection* res = calloc(sizeof(connection), num_conns);
	if (!res) {
		PFERROR("couldn't allocate memory for connections.\n");
		return NULL;
	}

	int ret;
	int i;
	for (i=0; i<num_conns; i++) {
		res[i].id = i;
		res[i].connected = false;
		if (dst_addr) {
                	ret = rdma_create_id(channel, &res[i].cma_id,  &res[i], hint.ai_port_space);
                	if (ret) {
                		PSERROR("failed to create id");
                		goto err;
                	}
                }
	}
	return res;
err:
	while(--i >= 0) {
		rdma_destroy_id(res[i].cma_id);
	}
	free(res);
	return NULL;
}

static void clean_conns(connection* conns, int num_conns) {
	int i;

	for (i = 0; i < num_conns; i++) {
		if (!conns[i].cma_id) {
			// empty conn
			continue;
		}

		if (conns[i].ah) {
			ibv_destroy_ah(conns[i].ah);
		}

		if (conns[i].cma_id->qp) {
			rdma_destroy_qp(conns[i].cma_id);
		}

		if (conns[i].cq) {
			ibv_destroy_cq(conns[i].cq);
		}

		if (conns[i].mem) {
			ibv_dereg_mr(conns[i].mr);
			free(conns[i].mem);
		}

		if (conns[i].pd) {
                	ibv_dealloc_pd(conns[i].pd);
                }

                rdma_destroy_id(conns[i].cma_id);
	}
	free(conns);
}

job* create_job() {
	job *res = malloc(sizeof(job));
	if (!res) {
		PSERROR("couldn't allocation memory for a job");
		return NULL;
	}

	res->channel = rdma_create_event_channel();
	if (!res->channel) {
		PSERROR("failed to create event channel");
		free(res);
		return NULL;
	}

	res->conns = create_conns(num_conns, res->channel);
	if (!res->conns) {
		rdma_destroy_event_channel(res->channel);
		free(res);
		return NULL;
	}
	res->conn_index = 0;
	res->conn_left = num_conns;
	res->addr_info = NULL;

	return res;
}

void clean_job(job* the_job) {
	clean_conns(the_job->conns, num_conns);
	rdma_destroy_event_channel(the_job->channel);
	if (the_job->addr_info)
		rdma_freeaddrinfo(the_job->addr_info);
	free(the_job);
}

int create_message(connection *conn) {
	if (!msg_size) {
		return 0;
	}

	int total_size = msg_size + sizeof(struct ibv_grh);
	conn->mem = malloc(total_size);
	if (!conn->mem) {
		PFERROR("failed to allocate memory for message\n");
		return -ENOMEM;
	}
	conn->mr = ibv_reg_mr(conn->pd, conn->mem, total_size, IBV_ACCESS_LOCAL_WRITE);
	if (!conn->mr) {
		PFERROR("failed to register MR\n");
		free(conn->mem);
		return -1;
	}
	return 0;
}

int check_conn(connection *conn) {
	struct ibv_port_attr attr;

	int ret = ibv_query_port(conn->cma_id->verbs, conn->cma_id->port_num,
	                         &attr);
	if (ret) {
		return ret;
	}

	int max_size = 1 << (attr.active_mtu + 7);
	if (msg_count && msg_size > max_size) {
		PFERROR("message size %d exceeds active mtu %d\n", msg_size, max_size);
		return -EINVAL;
	}

	return 0;
}

int init_conn(connection *conn) {
	struct ibv_qp_init_attr attr = {0};
	int ret;
	conn->pd = ibv_alloc_pd(conn->cma_id->verbs);
	if (!conn->pd) {
		PFERROR("couldn't allocate PD\n");
		return -ENOMEM;
	}

	int cqe = msg_count ? msg_count * 2: 2;
	conn->cq = ibv_create_cq(conn->cma_id->verbs, cqe, conn, 0, 0);
	if (!conn->cq) {
		PFERROR("failed to create CQ\n");
		return  -ENOMEM;
	}

	attr.cap.max_send_wr = msg_count ? msg_count + 1 : 1;
	attr.cap.max_recv_wr = msg_count ? msg_count + 1 : 1;
	attr.cap.max_send_sge = 1;
	attr.cap.max_recv_sge = 1;
	attr.qp_context = conn;
	attr.sq_sig_all = 0;
	attr.qp_type = IBV_QPT_UD;
	attr.send_cq = conn->cq;
	attr.recv_cq = conn->cq;
	ret = rdma_create_qp(conn->cma_id, conn->pd, &attr);
	if (ret) {
		PSERROR("failed to create QP");
		return ret;
	}

	ret = create_message(conn);
	return ret;
}

int post_recvs(connection *conn) {
	struct ibv_recv_wr wr, *bad_wr;
	struct ibv_sge sge;
	int i, ret = 0;

	if (!msg_count) {
		return 0;
	}

	wr.next = NULL;
	wr.sg_list = &sge;
	wr.num_sge = 1;
	wr.wr_id = (uintptr_t) conn;

	sge.length = msg_size + sizeof(struct ibv_grh);
	sge.lkey = conn->mr->lkey;
	sge.addr = (uintptr_t) conn->mem;

	for (i = 0; i < (msg_count + 1) && !ret; i++ ) {
		ret = ibv_post_recv(conn->cma_id->qp, &wr, &bad_wr);
		if (ret) {
			PSERROR("failed to post receives");
			break;
		}
	}
	return ret;
}

void *do_post_sends(void *arg) {
	send_param *param = (send_param *)arg;
	connection *conn = param->conn;
	struct ibv_send_wr wr, *bad_wr;
	struct ibv_sge sge;
	int i, ret = 0;
	DBGPRINT("  id=%d connected=%d msg_count=%d\n", conn->id, conn->connected, param->msg_count);
	if (!conn->connected || !msg_count) {
		param->ret = 0;
		return NULL;
	}

	wr.next = NULL;
	wr.sg_list = &sge;
	wr.num_sge = 1;
	wr.opcode = IBV_WR_SEND_WITH_IMM;
	wr.send_flags = param->send_flags;
	wr.wr_id = (unsigned long)conn;
	wr.imm_data = htonl(conn->cma_id->qp->qp_num);

	wr.wr.ud.ah = conn->ah;
	wr.wr.ud.remote_qpn = conn->remote_qpn;
	wr.wr.ud.remote_qkey = conn->remote_qkey;

	sge.length = msg_size;
	sge.lkey = conn->mr->lkey;
	sge.addr = (uintptr_t) conn->mem;

	for (i = 0; i < param->msg_count && !ret; i++) {
		ret = ibv_post_send(conn->cma_id->qp, &wr, &bad_wr);
		if (ret) {
			PFERROR("failed to post sends: ret=%d\n", ret);
		}
	}
	param->ret = ret;
	return NULL;
}

int post_sends(job *the_job, int send_flags, int msg_count) {
	int i, ret = 0;
	DBGPRINT("post sends\n");
	send_param *params = malloc(sizeof(send_param) * num_conns);
	if (!params) {
		PFERROR("failed to allocate memory for send parameters\n");
                return -ENOMEM;
	}
	for (i = 0; i < num_conns; i++) {
		params[i].conn = &the_job->conns[i];
        	params[i].msg_count = msg_count;
        	params[i].send_flags = send_flags;
        	params[i].ret = -1;
        	pthread_create(&params[i].thread, NULL, do_post_sends, &params[i]);
        }
        for (i = 0; i < num_conns; i++) {
        	pthread_join(params[i].thread, NULL);
        }
        for (i = 0; i < num_conns; i++) {
        	if (params[i].ret) {
        		ret = params[i].ret;
        		break;
        	}
        }
        free(params);
        return ret;
}

void *do_poll_cqs(void *arg) {
	poll_param *param = (poll_param *)arg;
	struct ibv_wc wc[8];
	struct ibv_qp_attr attr;
	struct ibv_qp_init_attr init_attr;
	int count = 0, ret;
	uint64_t progress_time = get_timestamp() + 1000000;

	connection *conn = param->conn;
	int msg_count = param->msg_count;
	DBGPRINT("  conn id=%d connected=%d expected msg_count=%d\n", conn->id, conn->connected, msg_count);
	if (!conn->connected) {
		param->ret = 0;
		goto out;
	}

	for (count = 0; count < msg_count; count += ret) {
		ret = ibv_poll_cq(conn->cq, msg_count >8 ? 8 : msg_count, wc);
		if (!dst_addr && get_timestamp() >= progress_time) {
			// only print progress on receiver side
			DBGPRINT("connection %2d - progress: qpn=%d expected msg_count=%d got=%d poll ret=%d\n",
				conn->id, conn->cma_id->qp->qp_num, msg_count, count, ret);
			progress_time += 1000000;
		}
		if (ret > 0 && (msg_count - count < 10)) {
			DBGPRINT("    id=%d count=%d get=%d\n", conn->id, count, ret);
		}
		if (ret < 0) {
			PFERROR("failed to poll CQ: ret=%d\n", ret);
			param->ret = ret;
			goto out;
		}
		if (!ret && get_timestamp() >= stop_time_us) {
			PFWARN("connection %2d - timeout: qpn=%d expected msg_count=%d got=%d\n",
				conn->id, conn->cma_id->qp->qp_num, msg_count, count);
			param->ret = 0;
			goto out;
		}

		if (ret && !conn->ah) {
			DBGPRINT("    create ah\n");
			conn->ah = ibv_create_ah_from_wc(conn->pd, wc, conn->mem,
							 conn->cma_id->port_num);
			conn->remote_qpn = ntohl(wc->imm_data);

			ibv_query_qp(conn->cma_id->qp, &attr, IBV_QP_QKEY, &init_attr);
			conn->remote_qkey = attr.qkey;
		}
	}
	DBGPRINT("  conn id=%d count=%d last_get=%d\n", conn->id, count, ret);
	param->ret = 0;
out:
	if (start_us) {
		conn->rx_pkts += count;
	}
	return NULL;
}

int poll_cqs(job *the_job, int msg_count) {
	int i, ret = 0;
	DBGPRINT("poll cqs\n");
	poll_param *params = malloc(sizeof(poll_param) * num_conns);
	if (!params) {
		PFERROR("failed to allocate memory for poll parameters\n");
                return -ENOMEM;
	}
	for (i = 0; i < num_conns; i++) {
		params[i].conn = &the_job->conns[i];
        	params[i].msg_count = msg_count;
        	params[i].ret = -1;
        	pthread_create(&params[i].thread, NULL, do_poll_cqs, &params[i]);
        }
        for (i = 0; i < num_conns; i++) {
        	pthread_join(params[i].thread, NULL);
        }
        for (i = 0; i < num_conns; i++) {
        	if (params[i].ret) {
        		ret = params[i].ret;
        		break;
        	}
        }
        free(params);
        return ret;
}

int on_addr_event(job *the_job, struct rdma_cm_event *event) {
	int ret = 0;

	connection *conn = event->id->context;
	if (set_tos) {
		ret = rdma_set_option(conn->cma_id, RDMA_OPTION_ID,
		                      RDMA_OPTION_ID_TOS, &tos, sizeof(tos));
		if (ret) {
			PSERROR("failed set TOS");
			return ret;
		}
	}

	ret = rdma_resolve_route(conn->cma_id, OP_TIMEOUT_MS);
	if (ret) {
		PSERROR("failed resolve route");
		the_job->conn_left -= 1;
	}
	return ret;
}

int on_route_event(job *the_job, struct rdma_cm_event *event) {
	connection *conn = event->id->context;
	int ret = check_conn(conn);
	if (ret) {
		goto err;
	}

	ret = init_conn(conn);
	if (ret) {
		goto err;
	}
	ret = post_recvs(conn);
	if (ret) {
		goto err;
	}

	struct rdma_conn_param param = {0};
	param.private_data = the_job->addr_info->ai_connect;
	param.private_data_len = the_job->addr_info->ai_connect_len;
	ret = rdma_connect(conn->cma_id, &param);
	if (ret) {
		PSERROR("failed to connect");
		goto err;
	}
	return 0;
err:
	the_job->conn_left -= 1;
	return ret;
}

int on_connect_event(job *the_job, struct rdma_cm_event *event) {
	struct rdma_cm_id *cma_id = event->id;
	int ret;

	if (the_job->conn_index == num_conns) {
		ret = -ENOMEM;
		goto err1;
	}
	connection *next = &the_job->conns[the_job->conn_index++];

	next->cma_id = cma_id;
	cma_id->context = next;

	ret = check_conn(next);
	if (ret) {
		goto err2;
	}

	ret = init_conn(next);
	if (ret) {
		goto err2;
	}

	ret = post_recvs(next);
	if (ret) {
		goto err2;
	}

	struct rdma_conn_param param = {0};
	param.qp_num = next->cma_id->qp->qp_num;
	ret = rdma_accept(next->cma_id, &param);
	if (ret) {
		PSERROR("failed to accept");
		goto err2;
	}
	next->connected = true;
	the_job->conn_left -= 1;
	FPRINT("Accept connection: id=%2d qp_num=%d\n", next->id, param.qp_num);
	return 0;

err2:
	next->cma_id = NULL;
	the_job->conn_left -= 1;
err1:
	PFERROR("failure on connection request\n");
	rdma_reject(cma_id, NULL, 0);
	return ret;
}

int on_established_event(job* the_job, struct rdma_cm_event *event) {
	int ret = 0;
	connection *conn = event->id->context;
	conn->remote_qpn = event->param.ud.qp_num;
	conn->remote_qkey = event->param.ud.qkey;
	conn->ah = ibv_create_ah(conn->pd, &event->param.ud.ah_attr);
	if (!conn->ah) {
		PFERROR("failed to create address handle\n");
		ret = -1;
	} else {
		conn->connected = true;
		ret = 0;
	}

	the_job->conn_left -= 1;
	FPRINT("Connection established: id=%3d qp_num=%3d remote_qpn=%3d\n",
		conn->id, conn->cma_id->qp->qp_num, conn->remote_qpn);
	return ret;
}

int process_events(job* the_job) {
	struct rdma_cm_event *event;
	int ret = 0;

	while (the_job->conn_left && !ret) {
		ret = rdma_get_cm_event(the_job->channel, &event);
		if (ret) {
			break;
		}
		switch (event->event) {
			case RDMA_CM_EVENT_ADDR_RESOLVED:
				// client side
				DBGPRINT("RDMA_CM_EVENT_ADDR_RESOLVED\n");
				ret = on_addr_event(the_job, event);
				break;
			case RDMA_CM_EVENT_ROUTE_RESOLVED:
				// client side
				DBGPRINT("RDMA_CM_EVENT_ROUTE_RESOLVED\n");
				ret = on_route_event(the_job, event);
				break;
			case RDMA_CM_EVENT_CONNECT_REQUEST:
				// server side
				DBGPRINT("RDMA_CM_EVENT_CONNECT_REQUEST\n");
				ret = on_connect_event(the_job, event);
				break;
			case RDMA_CM_EVENT_ESTABLISHED:
				// client side
				DBGPRINT("RDMA_CM_EVENT_ESTABLISHED\n");
				ret = on_established_event(the_job, event);
				break;
			case RDMA_CM_EVENT_ADDR_ERROR:
			case RDMA_CM_EVENT_ROUTE_ERROR:
			case RDMA_CM_EVENT_CONNECT_ERROR:
			case RDMA_CM_EVENT_UNREACHABLE:
			case RDMA_CM_EVENT_REJECTED:
				PFERROR("event: %s, error: %d\n",
				        rdma_event_str(event->event), event->status);
				the_job->conn_left -= 1;
				ret = event->status;
				break;
			case RDMA_CM_EVENT_DEVICE_REMOVAL:
				PFERROR("Device removed.\n");
				break;
			default:
				break;
		}
		rdma_ack_cm_event(event);
	}
	return ret;
}

int launch_server(job* the_job)
{
	struct rdma_cm_id *listen_id;
	int ret;

	FPRINT("Starting server\n");
	ret = rdma_create_id(the_job->channel, &listen_id, the_job, hint.ai_port_space);
	if (ret) {
		PSERROR("failed create id");
		return ret;
	}

	ret = rdma_getaddrinfo(src_addr, port, &hint, &the_job->addr_info);
	if (ret) {
		PFERROR("couldn't get address : %s\n", gai_strerror(ret));
		goto out;
	}

	ret = rdma_bind_addr(listen_id, the_job->addr_info->ai_src_addr);
	if (ret) {
		PSERROR("failed bind address");
		goto out;
	}

	ret = rdma_listen(listen_id, 0);
	if (ret) {
		PSERROR("failed listen to incoming connections");
		goto out;
	}

	ret = process_events(the_job);
	if (ret) {
		goto out;
	}

	stop_time_us = get_timestamp() + timeout_s * 1000000;
	FPRINT("Waiting clients\n");
	ret = poll_cqs(the_job, 1);
	if (ret) {
		goto out;
	}
	FPRINT("Starting test\n");
	start_us = get_timestamp();
	ret = post_sends(the_job, IBV_SEND_SIGNALED, 1);
	if (ret) {
		goto out;
	}
	ret = poll_cqs(the_job, 1);
	if (ret) {
		goto out;
	}
	if (msg_count) {
		FPRINT("Receiving data\n");
		ret = poll_cqs(the_job, msg_count);
		end_us = get_timestamp();
		update_perf(the_job, &receive_perf, num_conns, (long)num_conns * (msg_count + 1));
		// prepare for reply perf
		start_us = end_us = 0;
		if (ret) {
			goto out;
		}

		if (srv_reply) {
			FPRINT("Sending replies\n");
			start_us = get_timestamp();
		} else {
			FPRINT("Sending test end\n");
		}

		ret = post_sends(the_job, IBV_SEND_SIGNALED, srv_reply ? msg_count : 1);
		if (ret) {
			goto out;
		}

		ret = poll_cqs(the_job, srv_reply ? msg_count : 1);
		if (srv_reply) {
			end_us = get_timestamp();
			update_perf(the_job, &reply_perf, (long)num_conns * msg_count, (long)num_conns * msg_count);
		}
		if (ret) {
			goto out;
		}
		FPRINT("Communication complete\n");
	}
out:
	rdma_destroy_id(listen_id);
	return ret;
}

int launch_client(job *the_job) {
	int i, ret;
	struct rdma_addrinfo *src_ai = NULL;

	FPRINT("Starting client\n");

	if (src_addr) {
		hint.ai_flags |= RAI_PASSIVE;
		ret = rdma_getaddrinfo(src_addr, NULL, &hint, &src_ai);
		if (ret) {
			PFERROR("failed to get source address info: %s\n", gai_strerror(ret));
			return ret;
		}
		hint.ai_src_addr = src_ai->ai_src_addr;
		hint.ai_src_len = src_ai->ai_src_len;
		hint.ai_flags &= ~RAI_PASSIVE;
	}
	ret = rdma_getaddrinfo(dst_addr, port, &hint, &the_job->addr_info);
	if (ret) {
		PFERROR("failed to get address info: %s\n", gai_strerror(ret));
		return ret;
	}
	if (src_ai) {
		rdma_freeaddrinfo(src_ai);
	}

	FPRINT("Connecting to server\n");
	for (i = 0; i < num_conns; i++) {
		ret = rdma_resolve_addr(the_job->conns[i].cma_id, the_job->addr_info->ai_src_addr,
					the_job->addr_info->ai_dst_addr, OP_TIMEOUT_MS);
		if (ret) {
			PFERROR("failed to get addr: ret=%d\n", ret);
			the_job->conn_left -= 1;
			return ret;
		}
	}

	ret = process_events(the_job);
	if (ret) {
		return ret;
	}

	stop_time_us = get_timestamp() + timeout_s * 1000000;
	FPRINT("Notifying server\n");
	ret = post_sends(the_job, 0, 1);
	if (ret) {
		the_job->conn_left -= 1;
		return ret;
	}
	FPRINT("Waiting test start\n");
	ret = poll_cqs(the_job, 1);
	if (ret) {
		return ret;
	}
	if (msg_count) {
		FPRINT("Sending data\n");
		start_us = get_timestamp();
		ret = post_sends(the_job, 0, msg_count);
		if (ret) {
			the_job->conn_left -= 1;
			return ret;
		}
		if (srv_reply) {
			FPRINT("Receiving reply\n");
		} else {
			FPRINT("Waiting test end\n");
		}
		stop_time_us += 5000000; // plus 5 seconds, so the poll has a chance to run if timeout happened
		ret = poll_cqs(the_job, srv_reply ? msg_count : 1);
		end_us = get_timestamp();
		update_perf(the_job, &receive_perf, (long)num_conns * msg_count, num_conns * (srv_reply ? msg_count : 1));
		if (ret) {
			return ret;
		}

		FPRINT("Communication complete\n");
	}
	return ret;
}

int main(int argc, char **argv) {
	int op, ret;

	while ((op = getopt(argc, argv, "s:b:B:rc:C:S:t:T:")) != -1) {
		switch (op) {
		case 's':
			dst_addr = optarg;
			break;
		case 'b':
			src_addr = optarg;
			break;
		case 'B':
			port = optarg;
			break;
		case 'r':
			srv_reply = true;
			break;
		case 'c':
			num_conns = atoi(optarg);
			if (num_conns <= 0) {
				num_conns = 1;
			}
			break;
		case 'C':
			msg_count = atoi(optarg);
			if (msg_count < 0) {
				msg_count = 0;
			}
			break;
		case 'S':
			msg_size = atoi(optarg);
			if (msg_size <= 0) {
				msg_size = 1;
			}
			break;
		case 't':
			set_tos = true;
			tos = (uint8_t) strtoul(optarg, NULL, 0);
			break;
		case 'T':
			timeout_s = atoi(optarg);
			break;
		default:
			usage();
			exit(1);
		}
	}

	hint.ai_port_space = RDMA_PS_UDP;
	job *cur_job = create_job();
	if (!cur_job) {
		exit(1);
	}

	if (dst_addr) {
		ret = launch_client(cur_job);
	} else {
		hint.ai_flags |= RAI_PASSIVE;
		ret = launch_server(cur_job);
	}

	FPRINT("Test complete\n");
	print_perf("Receive Perf.", receive_perf);
	print_perf("Reply Perf.", reply_perf);
	clean_job(cur_job);

	return ret;
}
