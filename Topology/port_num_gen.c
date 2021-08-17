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

#include "port_num_gen.h"
#include <ctype.h>
#include <inttypes.h>

#define DEBUG 0
#define DBGPRINT(format, args...) if (DEBUG) { fprintf(stderr, format, ##args); }

#define PNGTAG MAKE_MEM_TAG('p','n', 'g', 'e')
#define MAX_SEGS 8
#define MAX_PORT_NUM 256

// segment data structure
typedef struct {
	// segment key calculated from seg index and contents
	uint64 key;
	// value of the segment. For alphabet seg, it doesn't apply and
	//  the value is always -1
	int num;
} pn_seg;

// port data structure
typedef struct {
	// segments in a port name
	pn_seg segments[MAX_SEGS];
	// number of segments
	int seg_count;
	// group key
	uint64 group_key;
	// port number adjustment
	int offset;
	// port number
	uint8 port_num;
} pn_port_item;

// double linked group data structure
typedef struct pn_group_s {
	// group key
	uint64 key;
	// the min port number in a group
	uint8 min;
	// the max port number in a group
	uint8 max;
	// occurrence of the group
	int count;
	// port number adjustment
	int offset;
	// previous pn_group_item
	struct pn_group_s* prev;
	// next pn_group_item
	struct pn_group_s* next;
} pn_group_item;

cl_map_obj_t *create_port_obj() {
	pn_port_item* port = MemoryAllocate2AndClear(sizeof(pn_port_item), IBA_MEM_FLAG_PREMPTABLE, PNGTAG);
	if (port == NULL) {
        	fprintf(stderr, "ERROR - couldn't allocate memory for port item!\n");
        	return NULL;
        }

	cl_map_obj_t *item = MemoryAllocate2AndClear(sizeof(cl_map_obj_t), IBA_MEM_FLAG_PREMPTABLE, PNGTAG);
	if (item == NULL) {
		fprintf(stderr, "ERROR - couldn't allocate memory for map obj!\n");
		MemoryDeallocate(port);
		return NULL;
	}
	item->p_object = port;

	return item;
}

cl_map_obj_t *create_count_obj() {
	uint8* count = MemoryAllocate2AndClear(sizeof(uint8), IBA_MEM_FLAG_PREMPTABLE, PNGTAG);
	if (count == NULL) {
		fprintf(stderr, "ERROR - couldn't allocate memory for count!\n");
		return NULL;
        }

	cl_map_obj_t *res = MemoryAllocate2AndClear(sizeof(cl_map_obj_t), IBA_MEM_FLAG_PREMPTABLE, PNGTAG);
	if (res == NULL) {
		fprintf(stderr, "ERROR - couldn't allocate memory for map obj\n");
		MemoryDeallocate(count);
		return NULL;
	}
	res->p_object = count;
	return res;
}

void _free_pn_map_item(const cl_map_item_t *mItem, const cl_map_item_t *end) {
	if (mItem != end) {
		cl_map_item_t *next = cl_qmap_next(mItem);
		_free_pn_map_item(next, end);
		cl_map_obj_t *parent = PARENT_STRUCT(mItem, cl_map_obj_t, item);
		if (parent) {
			if (parent->p_object) {
				MemoryDeallocate((void*)parent->p_object);
			}
			MemoryDeallocate(parent);
		}
	}
}

pn_group_item* create_group_item() {
	pn_group_item* res = MemoryAllocate2AndClear(sizeof(pn_group_item), IBA_MEM_FLAG_PREMPTABLE, PNGTAG);
	if (res == NULL) {
		fprintf(stderr, "ERROR - couldn't allocate memory for group item!\n");
		return NULL;
	}
	return res;
}

void free_group_item(pn_group_item* head) {
	pn_group_item* tmp = head;
	while(tmp) {
		if (tmp->prev) {
			MemoryDeallocate(tmp->prev);
		}
		if (!tmp->next) {
			// the tail
			MemoryDeallocate(tmp);
			break;
		}
		tmp = tmp->next;
	}
}

void pn_gen_init(pn_gen_t* const model) {
	cl_qmap_init(&(model->seg_map), NULL);
	cl_qmap_init(&(model->port_map), NULL);
	model->processed = FALSE;
}

uint64 cal_str_key(int index, char* const start, char* const end) {
	uint64 res = 5381 + index;
	char* p = start;
	for (; *p && (end == NULL || p < end); p++) {
		res = ((res << 5) + res) + *p;
	}
	return res;
}

int parse_port_name(char* const port_name, pn_port_item* port_item) {
	if (port_name == NULL) {
		fprintf(stderr, "ERROR - port name is NULL!\n");
		return PNG_INVALID_PORT_NAME;
	}
	if (! *port_name) {
		fprintf(stderr, "ERROR - empty port name!\n");
		return PNG_INVALID_PORT_NAME;
	}

	char* p = port_name;
	int index = 0;
	char* seg_start = port_name;
	boolean has_num = FALSE;
	boolean is_num = FALSE;
	boolean to_close_seg = FALSE;
	int value = 0;
	//boolean
	// state:
	//  0 - NONE
	//  1 - start a segment
	//  2 - in a segment
	//  3 - end of segment
	int state = 0;
	for (; *p; p++) {
		if (isdigit(*p) ) {
			if (state == 2 && is_num) {
				// continue a digit segment
				value = value * 10 + (*p - '0');
			} else {
				// change to a new digit segment
				is_num = TRUE;
				if (!has_num) {
					has_num = TRUE;
				}
				// if state is 2, a segment directly changes from alphabet seg
				// to digit seg. We need to close alphabet seg and then start
				// a new digit seg.
				to_close_seg = state == 2;
				state = 1;
			}
		} else if (isalpha(*p)) {
			if (state != 2 || is_num) {
				// change to a new alphabet segment
				is_num = FALSE;
				// if state is 2, a segment directly changes from digit seg
				// to alphabet seg. We need to close digit seg and then start
				// a new alphabet seg.
				to_close_seg = state == 2;
				state = 1;
			}
		} else if (state == 2) {
			state = 3;
		}

		if (state == 3 || to_close_seg) {
			// close a segment
			pn_seg* seg = &(port_item->segments[index]);
			seg->key = cal_str_key(index+1, seg_start, p);
			seg->num = value;
			to_close_seg = FALSE;
			DBGPRINT("  Seg%d: start=%ld end=%ld num=%d\n",
				index, seg_start - port_name, p - port_name, seg->num);
			index += 1;
		}
		if (state == 1) {
			// start a new segment
			if (index == MAX_SEGS) {
				fprintf(stderr, "ERROR - too much segments in port name!\n");
				return PNG_INVALID_PORT_NAME;
			}
			seg_start = p;
			if (is_num) {
				value = *p - '0';
			} else {
				value = -1;
			}
			state = 2;
		}
	}
	if (!has_num) {
		fprintf(stderr, "ERROR - no number in port name!\n");
                return PNG_INVALID_PORT_NAME;
	}

	if (state == 2) {
		// process last segment
		pn_seg* seg = &(port_item->segments[index]);
		seg->key = cal_str_key(index+1, seg_start, p);
		seg->num = value;
		DBGPRINT("  Seg%d: start=%ld end=%ld num=%d\n",
			index, seg_start - port_name, p - port_name, seg->num);
		index += 1;
	}
	port_item->seg_count = index;
	return PNG_OK;
}

uint8 update_count(cl_qmap_t* map, uint64 key) {
	uint8* count;
	cl_map_item_t *mItem = cl_qmap_get(map, key);
	if (mItem == cl_qmap_end(map)) {
		cl_map_obj_t *mapObj = create_count_obj();
		if (mapObj == NULL) {
			return 0;
		}
		cl_qmap_insert(map, key, &(mapObj->item));
		count = (uint8 *)mapObj->p_object;
	} else {
		count = cl_qmap_obj(PARENT_STRUCT(mItem, cl_map_obj_t, item));
	}
	(*count) +=  1;
	return *count;
}

int pn_gen_register(pn_gen_t* const model, char* const port_name) {
	if (model->processed) {
		fprintf(stderr, "ERROR - couldn't register because model was already processed and locked!\n");
		return PNG_ALREADY_PROCESSED;
	}

	cl_map_obj_t *mapObj = create_port_obj();
	if (mapObj == NULL) {
		return PNG_NO_MEMORY;
	}
	uint64 key = cal_str_key(0, port_name, NULL);
	cl_qmap_insert(&(model->port_map), key, &(mapObj->item));
	DBGPRINT("Added <%s> with key:%"PRIu64"\n", port_name, key);
	pn_port_item* ppitem = (pn_port_item*)mapObj->p_object;

	DBGPRINT("Parse <%s>\n", port_name);
	int ret = parse_port_name(port_name, ppitem);
	if (ret) {
		return ret;
	}
	DBGPRINT("  seg_count=%d\n", ppitem->seg_count);
	int i = 0;
	for (; i < ppitem->seg_count; i++) {
		update_count(&(model->seg_map), ppitem->segments[i].key);
	}
	return PNG_OK;
}

/**
 * Compare two groups, g1 and g2.
 *
 * When order groups, if g1 has larger occurrence than g2, g1 will be before
 * g2, i.e. smaller than g2. If they have the same occurrence, g1 is before
 * g2 if it has smaller start port number. Please note the followed key
 * comparison if g1 and g2 have the same start port number has no any physical
 * meaning. The intention is to ensure repeatable sort order.
 */
int compare_groups(pn_group_item* g1, pn_group_item* g2) {
	if (g1->count > g2->count) {
		return -1;
	} else if (g1->count < g2->count) {
		return 1;
	} else if (g1->min < g2->min) {
		return -1;
	} else if (g1->min > g2->min) {
		return 1;
	} else if (g1->key < g2->key) {
		return -1;
	} else if (g1->key > g2->key) {
        	return 1;
        } else {
        	return 0;
        }
}

/**
 * Sort groups in ascending order with insertion sort algorithm. We will only
 * have couple groups, this algorithm shall be efficient enough for us.
 */
void sort_groups(pn_group_item** head) {
	if (head == NULL || *head==NULL) {
		return;
	}
	pn_group_item* pitem;
	int comp = 0;
	for (pitem = (*head)->next; pitem; pitem = pitem->next) {
		comp = compare_groups(pitem, pitem->prev);
		if (comp >= 0) {
			continue;
		}
		// adjust item order
		// remove pitem from linked list
		pn_group_item* new_pitem = pitem->prev;
		new_pitem->next = pitem->next;
		if (pitem->next) {
			pitem->next->prev = new_pitem;
		}
		// search backward to find the first item, ritem, that is "smaller" than pitem
		pn_group_item* ritem;
		for (ritem = pitem->prev->prev; ritem; ritem = ritem->prev) {
			comp = compare_groups(pitem, ritem);
			if (comp > 0) {
				break;
			}
		}
		if (ritem) {
			// insert pitem after ritem
			pn_group_item* tmp = ritem->next;
			ritem->next = pitem;
			pitem->next = tmp;
			tmp->prev = pitem;
			pitem->prev = ritem;
		} else {
			// insert pitem as head
			pn_group_item* tmp = *head;
			*head = pitem;
			pitem->prev = NULL;
			pitem->next = tmp;
			tmp->prev = pitem;
		}
		pitem = new_pitem;
	}
}

/*
 * Process registered port names
 */
int pn_gen_process(pn_gen_t* const model) {
	int i = 0;
	int count = 0;
	int min_count = 256;
	int pn_index = MAX_SEGS;
	cl_map_item_t* port_item = cl_qmap_head(&(model->port_map));
	const cl_map_item_t* port_end_item = cl_qmap_end(&(model->port_map));
	pn_port_item*  port = NULL;
	cl_map_item_t* seg_item = NULL;
	const cl_map_item_t* seg_end_item = cl_qmap_end(&(model->seg_map));
	boolean has_confliction = FALSE;

	// Port array stores port items from port number 0 to 255. We support
	// up to 255 ports since we are using unit8 as port number. This array
	// is used to help us figure out whether there is port number conflication
	pn_port_item** full_ports =
		MemoryAllocate2AndClear(sizeof(pn_port_item*) * MAX_PORT_NUM, IBA_MEM_FLAG_PREMPTABLE, PNGTAG);
	if (full_ports == NULL) {
		fprintf(stderr, "ERROR - couldn't allocate memory for port item pointers!\n");
		return PNG_NO_MEMORY;
	}

	pn_group_item* group_head = NULL;
	pn_group_item* group_tail = NULL;

	for (; port_item != port_end_item; port_item = cl_qmap_next(port_item)) {
		// check each port
        	port = cl_qmap_obj(PARENT_STRUCT(port_item, cl_map_obj_t, item));
		min_count = 256;
		pn_index = MAX_SEGS;
        	for (i = 0; i < port->seg_count; i++) {
			// check each segment in a port name
        		if (port->segments[i].num == -1) {
	        		// ignore alphabet segment
        			continue;
        		}
        		// get the seg with least count
        		seg_item = cl_qmap_get(&(model->seg_map), port->segments[i].key);
        		if (seg_item != seg_end_item) {
        			count = *((uint8*)cl_qmap_obj(PARENT_STRUCT(seg_item, cl_map_obj_t, item)));
        			// in favor of larger index
        			if (count <= min_count) {
        				pn_index = i;
        				min_count = count;
        			}
        		} else {
        			// shouldn't happen
        			fprintf(stderr, "ERROR - no segment count found!\n");
        		}
        	}
        	if (pn_index < MAX_SEGS) {
        		port->port_num = (uint8)(port->segments[pn_index].num);
        		// check port confliction
        		if (port->port_num < MAX_PORT_NUM) {
        			if (full_ports[port->port_num] || port->port_num == 0) {
        				has_confliction = TRUE;
        			} else {
					full_ports[port->port_num] = port;
        			}
        		} else {
        			// shouldn't happen
        			fprintf(stderr, "ERROR - port number %d is out of range (0, %d)\n",
        				port->port_num, MAX_PORT_NUM);
        			MemoryDeallocate(full_ports);
				if (group_head) {
					free_group_item(group_head);
				}
        			return PNG_INVALID_PORT_NAME;
        		}
        		DBGPRINT("  port<%"PRIu64">: index=%d portNum=%d\n", port_item->key, pn_index, port->port_num);
        		// calculate group key. Please note the key is based on
        		// seg key that already includes seg index info.
			uint64 g_key = 0;
			for (i = 0; i < port->seg_count; i++) {
				if (i != pn_index) {
					g_key = 31 * g_key + port->segments[i].key;
				}
			}
			port->group_key = g_key;

			// update group port range if it already exists
			pn_group_item* pitem = group_head;
			while (pitem) {
				if (pitem->key == g_key) {
					if (port->port_num > pitem->max) {
						pitem->max = port->port_num;
					}
					if (port->port_num < pitem->min) {
						pitem->min = port->port_num;
					}
					pitem->count += 1;
					break;
				}
				pitem = pitem->next;
			}
			if (!pitem) {
				// new group
				pn_group_item* g_item = create_group_item();
				if (!g_item) {
	        			MemoryDeallocate(full_ports);
					if (group_head) {
						free_group_item(group_head);
					}
        				return PNG_NO_MEMORY;
				}
				g_item->key = g_key;
				g_item->min = g_item->max = port->port_num;
				g_item->count = 1;
				g_item->offset = 0;
				g_item->next = NULL;
				g_item->prev = NULL;
				// append to the group list
				if (group_tail) {
					group_tail->next = g_item;
					g_item->prev = group_tail;
					group_tail = g_item;
				} else {
					group_head = group_tail = g_item;
				}
			}
        	} else {
        		// shouldn't happen
        		fprintf(stderr, "ERROR - no min count found!\n");
        	}
    	}

    	// simple conflication resolve
    	if (has_confliction) {
    		DBGPRINT("Try to resolve confliction...\n");
    		// sort groups
    		sort_groups(&group_head);
		pn_group_item* item;
		// adjust port ZERO if it exists
		if (group_head->min == 0) {
			group_head->offset = 1;
		}
		// adjust port number if a group's port range overlaps with previous
		DBGPRINT("  Group %"PRIu64" min=%d max=%d count=%d offset=%d\n",
			group_head->key, group_head->min, group_head->max, group_head->count, group_head->offset);
		for (item = group_head->next; item; item = item->next) {
			if (item->min <= (item->prev->max + item->prev->offset)) {
				item->offset = item->prev->max + item->prev->offset - item->min + 1;
			}
			DBGPRINT("  Group %"PRIu64" min=%d max=%d count=%d offset=%d\n",
				item->key, item->min, item->max, item->count, item->offset);
		}
		// update each port's port offset based on group key
		for (port_item = cl_qmap_head(&(model->port_map)); port_item != port_end_item;
				port_item = cl_qmap_next(port_item)) {
			port = cl_qmap_obj(PARENT_STRUCT(port_item, cl_map_obj_t, item));
			for (item = group_head; item && item->key != port->group_key; item = item->next);
			if (item) {
				port->offset = item->offset;
				DBGPRINT("Adjusted port<%"PRIu64">: group=%"PRIu64" port_num=%d offset=%d\n",
					port_item->key, port->group_key, port->port_num, port->offset);
			} else {
				// shouldn't happen
				fprintf(stderr, "ERROR - couldn't find group of port %"PRIu64"\n", port->group_key);
			}
		}
    	}

	model->processed = TRUE;
	MemoryDeallocate(full_ports);
	free_group_item(group_head);
	return PNG_OK;
}

uint8 pn_gen_get_port(pn_gen_t* const model, char* const port_name) {
	if (!model->processed) {
		DBGPRINT("Process data\n");
		pn_gen_process(model);
	}

	cl_qmap_t* map = &(model->port_map);
	uint64 key = cal_str_key(0, port_name, NULL);
	cl_map_item_t *mItem = cl_qmap_get(map, key);
	if (mItem != cl_qmap_end(map)) {
		pn_port_item* port = cl_qmap_obj(PARENT_STRUCT(mItem, cl_map_obj_t, item));
		int port_num = port->port_num + port->offset;
		if (port_num >= MAX_PORT_NUM) {
			// shouldn't happen. check just in case.
			fprintf(stderr, "WARNING - port number %d overflow\n", port_num);
		}
		return (uint8)port_num;
	} else {
		fprintf(stderr, "ERROR - couldn't find port '%s'\n", port_name);
		return 0;
	}
}

void pn_gen_cleanup(pn_gen_t* const model) {
	cl_qmap_t* map = &(model->seg_map);
	const cl_map_item_t *head = cl_qmap_head(map);
	const cl_map_item_t *end = cl_qmap_end(map);
	_free_pn_map_item(head, end);
	cl_qmap_remove_all(map);

	map = &(model->port_map);
	head = cl_qmap_head(map);
	end = cl_qmap_end(map);
	_free_pn_map_item(head, end);
	cl_qmap_remove_all(map);
}

#if 0
//------- unit test code ---------//

boolean test_group_sort() {
	boolean res = TRUE;
	printf("\n-- test_group_sort --\n");
	pn_group_item g1 = {.key=1, .min=1, .max=5};
	pn_group_item g2 = {.key=2, .min=6, .max=8};
	pn_group_item g3 = {.key=3, .min=9, .max=11};
	pn_group_item g4 = {.key=4, .min=9, .max=11};

	pn_group_item* head = &g3;
	g3.prev = NULL;
	g3.next = &g1;
	g1.prev = &g3;
	g1.next = &g2;
	g2.prev = &g1;
	g2.next = &g4;
	g4.prev = &g2;
	g4.next = NULL;

	pn_group_item* item;
	printf("Before sort\n");
	for(item = head; item; item = item->next) {
		printf("  group %"PRIu64"\n", item->key);
	}
	sort_groups(&head);
	printf("After sort\n");
	for(item = head; item; item = item->next) {
		printf("  group %"PRIu64"\n", item->key);
	}
	pn_group_item* g = head;
	if (g->key != 1) res = FALSE;
	g = g->next;
	if (g->key != 2) res = FALSE;
	g = g->next;
	if (g->key != 3) res = FALSE;

	if (res) {
		printf("test_group_sort passed.\n");
	} else {
		printf("test_group_sort failed.\n");
	}

	return res;
}

#define TST_MAX_NAME 65
#define TST_MAX_PORT 128

boolean test_port_num(char* test_name, char names[][TST_MAX_NAME], int* ports, int size) {
	printf("\n-- test_port_num: %s--\n", test_name);
	boolean res = TRUE;
	int i;

	pn_gen_t model;
	pn_gen_init(&model);

	for (i = 0; i< size; i++) {
		pn_gen_register(&model, names[i]);
	}

	for (i = 0; i< size; i++) {
		uint8 port_num = pn_gen_get_port(&model, names[i]);
		printf("  '%s' -> %d\t- ", names[i], port_num);
		if (port_num != ports[i]) {
			printf("Failed: expect %d\n", ports[i]);
			res = FALSE;
		} else {
			printf("Passed\n");
		}
	}

	pn_gen_cleanup(&model);

	if (res) {
		printf("%s test passed.\n", test_name);
	} else {
		printf("%s test failed.\n", test_name);
	}

	return res;
}

int main(int argc, char** argv) {
	test_group_sort();

	int i;
	char names[TST_MAX_PORT][TST_MAX_NAME] = {0};
	int ports[TST_MAX_PORT] = {0};

	for (i = 0; i < 32; i++) {
		snprintf(names[i], TST_MAX_NAME-1, "Eth1/%d", i + 1);
		ports[i] = i + 1;
	}
	test_port_num("Mellanox SN2700 switch", names, ports, 32);

	memset(names, 0, TST_MAX_PORT * TST_MAX_NAME);
	memset(ports, 0, TST_MAX_PORT);
	sprintf(names[0], "mgmt0");
	ports[0] = 37;
	for (int i = 1; i <= 36; i++) {
		snprintf(names[i], TST_MAX_NAME-1, "Ethernet1/%d", i);
                ports[i] = i;
	}
	test_port_num("Cisco 9236C switch", names, ports, 37);

	memset(names, 0, TST_MAX_PORT * TST_MAX_NAME);
	memset(ports, 0, TST_MAX_PORT);
	sprintf(names[0], "Management1");
	ports[0] = 87;
	for (int i = 1; i <= 32; i++) {
		snprintf(names[i], TST_MAX_NAME-1, "Ethernet%d/1", i);
                ports[i] = i;
	}
	for (int i = 0; i < 2; i++) {
		snprintf(names[33 + i], TST_MAX_NAME-1, "Ethernet%d", 33 + i);
                ports[33 + i] = 85 + i;
	}
	sprintf(names[35], "Port-Channel1");
	ports[35] = 33;
	for (int i = 0; i < 4; i++) {
		snprintf(names[36 + i], TST_MAX_NAME-1, "Port-Channel%d", 23 + i);
                ports[36 + i] = 55 + i;
	}
	sprintf(names[40], "Port-Channel52");
	ports[40] = 84;
	test_port_num("Arista 7060CX switch", names, ports, 41);

	memset(names, 0, TST_MAX_PORT * TST_MAX_NAME);
	memset(ports, 0, TST_MAX_PORT);
	for (int i = 0; i < 64; i++) {
		snprintf(names[i], TST_MAX_NAME-1, "Ethernet%d/1", i + 1);
                ports[i] = i + 1;
	}
	sprintf(names[64], "Ethernet65");
	ports[64] = 65;
	sprintf(names[65], "Ethernet66");
	ports[65] = 66;
	sprintf(names[66], "Management1");
	ports[66] = 67;
	test_port_num("Arista c720/c731 switch", names, ports, 67);

	memset(names, 0, TST_MAX_PORT * TST_MAX_NAME);
	memset(ports, 0, TST_MAX_PORT);
	// the following names are guessed from
	// https://support.edge-core.com/hc/en-us/articles/900000200743--Edgecore-SONiC-Switch-Port-Attributes
	for (int i = 0; i < 56; i++) {
		snprintf(names[i], TST_MAX_NAME-1, "Ethernet%d", i + 1);
                ports[i] = i + 1;
	}
	test_port_num("Edgecore AS7326-56X switch", names, ports, 56);

	memset(names, 0, TST_MAX_PORT * TST_MAX_NAME);
	memset(ports, 0, TST_MAX_PORT);
	for (int i = 0; i < 24; i++) {
        	snprintf(names[i], TST_MAX_NAME-1, "Gi1/0/%d", i + 1);
                ports[i] = i + 1;
        }
	for (int i = 0; i < 4; i++) {
        	snprintf(names[24 + i], TST_MAX_NAME-1, "Gi2/0/%d", i + 1);
                ports[24 + i] = 24 + i + 1;
        }
        test_port_num("Csico 3750x switch", names, ports, 28);

	memset(names, 0, TST_MAX_PORT * TST_MAX_NAME);
	memset(ports, 0, TST_MAX_PORT);
	for (int i = 0; i < 24; i++) {
        	snprintf(names[i], TST_MAX_NAME-1, "Gi1/0/%d", i);
                ports[i] = i + 1;
        }
	for (int i = 0; i < 24; i++) {
        	snprintf(names[24+i], TST_MAX_NAME-1, "Gi1/1/%d", i);
                ports[24+i] = 25 + i;
        }
	for (int i = 0; i < 4; i++) {
        	snprintf(names[48+i], TST_MAX_NAME-1, "Gi2/0/%d", 49 + i);
                ports[48+i] = 49 + i;
        }
	for (int i = 0; i < 2; i++) {
        	snprintf(names[52+i], TST_MAX_NAME-1, "LMgmt%d", i);
                ports[52+i] = 53 + i;
        }
	for (int i = 0; i < 2; i++) {
        	snprintf(names[54+i], TST_MAX_NAME-1, "RMgmt%d", i);
                ports[54+i] = 55 + i;
        }
        test_port_num("Corner case", names, ports, 56);
}

//------ End of unit test --------//
#endif