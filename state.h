#ifndef __STATE_H__
#define __STATE_H__

#include <stdint.h>

struct pkt;

class QFile;
struct state {
	QFile *fdh;
	QFile *out_fdh;
    int st;

    int skip_counter;
    int is_logging;
    int commands;
    int search_limit;
    int buffer_offset;

    /* Fudge packets, for when the counter is entirely worng */
    int last_sec, last_nsec, last_sec_adjust;

    /* When we start a new syncpoint or NAND run, save the offset */
    off_t last_run_offset;

    int sec_dif, nsec_dif;

    /* When joining, these contain the values for the previous run */
    int last_sec_dif, last_nsec_dif;

    int join_buffer_capacity;

    /* For group-joining, a list of open items */
    struct evt_header *events[128];

	/* The last-known NAND address */
	uint8_t addr[5];
};

int packet_get_next(struct state *st, struct pkt *pkt);
int packet_get_next_raw(struct state *st, struct pkt *pkt);
int packet_unget(struct state *st, struct pkt *pkt);
int packet_write(struct state *st, struct pkt *pkt);


#endif // __STATE_H__
