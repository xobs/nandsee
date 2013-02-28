#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "packet-struct.h"
#include "event-struct.h"
#include "state.h"

static const char *EVENT_HDR_1 = "TBEv";
static const char *EVENT_HDR_2 = "MaDa";

#define SKIP_AMOUNT 80
#define SEARCH_LIMIT 20

enum prog_state {
    ST_UNINITIALIZED,
    ST_DONE,
    ST_SCANNING,
    ST_GROUPING,
    ST_WRITE,
};


static int st_uninitialized(struct state *st);
static int st_scanning(struct state *st);
static int st_grouping(struct state *st);
static int st_done(struct state *st);
static int st_write(struct state *st);

static int (*st_funcs[])(struct state *st) = {
	st_uninitialized,
	st_done,
	st_scanning,
	st_grouping,
	st_write,
};

static uint32_t hdrs[16777216];
static int hdr_count;


int compare_event_addrs(void *thunk, const void *a1, const void *a2) {
	struct state *st = (struct state *)thunk;
	const uint32_t *o1 = (uint32_t *)a1;
	const uint32_t *o2 = (uint32_t *)a2;
	union evt e1, e2;

    lseek(st->fd, *o1, SEEK_SET);
    event_get_next(st, &e1);

    lseek(st->fd, *o2, SEEK_SET);
    event_get_next(st, &e2);

    if (e1.header.sec_start < e2.header.sec_start)
        return -1;
    if (e1.header.sec_start > e2.header.sec_start)
        return 1;
    if (e1.header.nsec_start < e2.header.nsec_start)
        return -1;
    if (e1.header.nsec_start > e2.header.nsec_start)
        return 1;
    return 0;
}


// Initialize the "joiner" state machine
struct state *sstate_init(void) {
	struct state *st = (struct state *)malloc(sizeof(struct state));

    memset(st, 0, sizeof(*st));
    st->is_logging = 0;
    st->st = ST_SCANNING;
    st->last_run_offset = 0;
    st->join_buffer_capacity = 0;
    st->buffer_offset = -1;
    st->search_limit = 0;
    return st;
}

int sstate_free(struct state **st) {
    free(*st);
    *st = NULL;
    return 0;
}

int sstate_state(struct state *st) {
    return st->st;
}

int sstate_run(struct state *st) {
    return st_funcs[st->st](st);
}

static int sstate_set(struct state *st, enum prog_state new_state) {
    st->st = new_state;
    return 0;
}



// Dummy state that should never be reached
static int st_uninitialized(struct state *st) {
    printf("state error: should not be in this state\n");
    return -1;
}

// Searching for either a NAND block or a sync point
static int st_scanning(struct state *st) {
    int ret;
    union evt evt;

    hdr_count = 0;
    lseek(st->fd, 0, SEEK_SET);
    do {
        int s = lseek(st->fd, 0, SEEK_CUR);
        if (s == -1) {
            perror("Couldn't seek");
            return 1;
        }
        hdrs[hdr_count++] = s;
    } while ((ret = event_get_next(st, &evt)) == 0);
    hdr_count--;
    printf("Working on %d events, last ret was %d...\n", hdr_count, ret);

    sstate_set(st, ST_GROUPING);
    return 0;
}

static int st_grouping(struct state *st) {
    qsort_r(hdrs, hdr_count, sizeof(*hdrs), st, compare_event_addrs);
    sstate_set(st, ST_WRITE);
    return 0;
}


/* We're all done sorting.  Write out the logfile.
 * Format:
 *   Magic number 0x43 0x9f 0x22 0x53
 *   Number of elements
 *   Array of absolute offsets from the start of the file
 *   Magic number 0xa4 0xc3 0x2d 0xe5
 *   Array of events
 */
static int st_write(struct state *st) {
    int jump_offset;
    uint32_t word;
    uint32_t offset;

    printf("Writing out...\n");
    lseek(st->out_fd, 0, SEEK_SET);
    offset = 0;

    // Write out magic
    write(st->out_fd, EVENT_HDR_1, 4);
    offset += sizeof(4);

    // Write out how many header items there are
    word = htonl(hdr_count);
    write(st->out_fd, &word, sizeof(word));
    offset += sizeof(word);

    // Advance the offset past the jump table
    offset += hdr_count*sizeof(offset);

    // Read in the jump table entries
    lseek(st->fd, 0, SEEK_SET);
    for (jump_offset=0; jump_offset<hdr_count; jump_offset++) {
        union evt evt;
        uint32_t offset_swab = htonl(offset);
        write(st->out_fd, &offset_swab, sizeof(offset_swab));

        lseek(st->fd, hdrs[jump_offset], SEEK_SET);
        memset(&evt, 0, sizeof(evt));
        event_get_next(st, &evt);
        if (evt.header.size > 32768)
            return 1;
        offset += evt.header.size;
    }

    write(st->out_fd, EVENT_HDR_2, 4);
    offset += sizeof(4);

    // Now copy over the exact events
    lseek(st->fd, 0, SEEK_SET);
    for (jump_offset=0; jump_offset<hdr_count; jump_offset++) {
        union evt evt;
        lseek(st->fd, hdrs[jump_offset], SEEK_SET);
        event_get_next(st, &evt);
        event_write(st, &evt);
    }

    sstate_set(st, ST_DONE);
    return 1;
}

static int st_done(struct state *st) {
    return 1;
}
