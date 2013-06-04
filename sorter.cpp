#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <QFile>
#include <QDebug>
#include "packet-struct.h"
#include "event-struct.h"
#include "state.h"
#include "byteswap.h"

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

struct small_hdr {
    uint32_t sec;
    uint32_t nsec;
    int pos;
};

static struct small_hdr *hdrs;
static int hdr_count;


int compare_event_addrs(const void *a1, const void *a2) {
    const struct small_hdr *o1 = (struct small_hdr *)a1;
    const struct small_hdr *o2 = (struct small_hdr *)a2;

    if (o1->sec < o2->sec)
        return -1;
    if (o1->sec > o2->sec)
        return 1;
    if (o1->nsec < o2->nsec)
        return -1;
    if (o1->nsec > o2->nsec)
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
    hdrs = NULL;
    return st;
}

int sstate_free(struct state **st) {
    free(*st);
    *st = NULL;
    free(hdrs);
    hdrs = NULL;
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
    Q_UNUSED(st);
    printf("state error: should not be in this state\n");
    return -1;
}

// Searching for either a NAND block or a sync point
static int st_scanning(struct state *st) {
    int ret;
    union evt evt;

    hdr_count = 0;
	st->fdh->seek(0);
    qDebug() << "Counting headers...\n";
    while(1) {
        int s = st->fdh->pos();
        if (s == -1) {
            perror("Couldn't seek");
            return 1;
        }
        ret = event_get_next(st, &evt);
        if (ret < 0)
            break;
        hdr_count++;
        hdrs = (struct small_hdr *)realloc(hdrs, hdr_count*sizeof(struct small_hdr));
        hdrs[hdr_count-1].sec = evt.header.sec_start;
        hdrs[hdr_count-1].nsec = evt.header.nsec_start;
        hdrs[hdr_count-1].pos = s;
    }
    qDebug() << "Found" << hdr_count << "headers to sort";

    sstate_set(st, ST_GROUPING);
    return 0;
}

static int st_grouping(struct state *st) {
    qsort(hdrs, hdr_count, sizeof(*hdrs), compare_event_addrs);
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
    struct evt_file_header file_header;
    uint32_t offset;

    qDebug() << "Writing out...";
	st->out_fdh->seek(0);
    offset = 0;

    // Write out the file header
    memset(&file_header, 0, sizeof(file_header));
    memcpy(file_header.magic1, EVENT_HDR_1, strlen(EVENT_HDR_1));
    file_header.version = _ntohl(1);
    file_header.count = _htonl(hdr_count);
	offset += st->out_fdh->write((char *)&file_header, sizeof(file_header));

    // Advance the offset past the jump table
    offset += hdr_count*sizeof(offset);

    // Read in the jump table entries
	st->fdh->seek(0);
    for (jump_offset=0; jump_offset<hdr_count; jump_offset++) {
        union evt evt;
        uint32_t offset_swab = _htonl(offset);
		st->out_fdh->write((char *)&offset_swab, sizeof(offset_swab));

        st->fdh->seek(hdrs[jump_offset].pos);
        memset(&evt, 0, sizeof(evt));
        event_get_next(st, &evt);
        if (evt.header.size > 32768)
            return 1;
        offset += evt.header.size;
    }

	offset += st->out_fdh->write(EVENT_HDR_2, 4);

    // Now copy over the exact events
    for (jump_offset=0; jump_offset<hdr_count; jump_offset++) {
        union evt evt;
        st->fdh->seek(hdrs[jump_offset].pos);
        event_get_next(st, &evt);
        event_write(st, &evt);
    }

    sstate_set(st, ST_DONE);
    return 1;
}

static int st_done(struct state *st) {
    Q_UNUSED(st);
    return 1;
}
