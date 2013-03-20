#include <stdio.h>
#include <QDebug>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <QFile>
#include "packet-struct.h"
#include "event-struct.h"
#include "state.h"
#include "byteswap.h"
#include "nand.h"

#define SKIP_AMOUNT 80
#define SEARCH_LIMIT 20

enum prog_state {
    ST_UNINITIALIZED,
    ST_DONE,
    ST_SCANNING,
    ST_GROUPING,
};


static int st_uninitialized(struct state *st);
static int st_scanning(struct state *st);
static int st_grouping(struct state *st);
static int st_done(struct state *st);

static int (*st_funcs[])(struct state *st) = {
	st_uninitialized,
	st_done,
	st_scanning,
	st_grouping,
};


static int evt_fill_header(void *arg, uint32_t sec_start, uint32_t nsec_start,
                    uint32_t size, uint8_t type) {
	struct evt_header *hdr = (struct evt_header *)arg;
    memset(hdr, 0, sizeof(*hdr));
    hdr->sec_start = _htonl(sec_start);
    hdr->nsec_start = _htonl(nsec_start);
    hdr->size = _htonl(size);
    hdr->type = type;
    return 0;
}

static int evt_fill_end(void *arg, uint32_t sec_end, uint32_t nsec_end) {
	struct evt_header *hdr = (struct evt_header *)arg;
	hdr->sec_end = _htonl(sec_end);
    hdr->nsec_end = _htonl(nsec_end);
    return 0;
}


static int evt_write_hello(struct state *st, struct pkt *pkt) {
    struct evt_hello evt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_HELLO);
    evt.version = pkt->data.hello.version;
    evt.magic1 = _htonl(EVENT_MAGIC_1);
    evt.magic2 = _htonl(EVENT_MAGIC_2);
    evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}


static int evt_write_reset(struct state *st, struct pkt *pkt) {
    struct evt_reset evt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_RESET);
    evt.version = pkt->data.reset.version;
    evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}

static int evt_write_nand_unk(struct state *st, struct pkt *pkt) {
    struct evt_nand_unk evt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_UNKNOWN);
    evt.data = pkt->data.nand_cycle.data;
    evt.ctrl = pkt->data.nand_cycle.control;
    evt.unknown = pkt->data.nand_cycle.unknown;
    evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}

static int evt_write_id(struct state *st, struct pkt *pkt) {
    struct evt_nand_id evt;

    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_ID);

    // Grab the "address" byte.
    packet_get_next(st, pkt);
    if (!nand_ale(pkt->data.nand_cycle.control)
     || !nand_we(pkt->data.nand_cycle.control))
        fprintf(stderr, "Warning: ALE/WE not set for 'Read ID'\n");
    evt.addr = pkt->data.nand_cycle.data;

    // Read the actual ID
    evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
    packet_get_next(st, pkt);
    for (evt.size=0;
         evt.size<sizeof(evt.id) && nand_re(pkt->data.nand_cycle.control);
         evt.size++) {
        evt.id[evt.size] = pkt->data.nand_cycle.data;
        evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
        packet_get_next(st, pkt);
    }

    if (!nand_re(pkt->data.nand_cycle.control))
        packet_unget(st, pkt);

    evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}

static int evt_write_sandisk_set(struct state *st, struct pkt *pkt) {
    struct evt_nand_unk_sandisk_code evt;
    struct pkt second_pkt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_SANDISK_VENDOR_START);

    // Make sure the subsequent packet is 0xc5
    packet_get_next(st, &second_pkt);
    if (!nand_cle(second_pkt.data.nand_cycle.control)
     || second_pkt.data.nand_cycle.data != 0xc5) {
        fprintf(stderr, "Not a Sandisk packet!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        return 0;
    }

    evt_fill_end(&evt, second_pkt.header.sec, second_pkt.header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}

static int evt_write_sandisk_param(struct state *st, struct pkt *pkt) {
    struct evt_nand_unk_sandisk_param evt;
    struct pkt second_pkt, third_pkt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_SANDISK_VENDOR_PARAM);

    // Make sure the subsequent packet is an address
    packet_get_next(st, &second_pkt);
    if (!nand_ale(second_pkt.data.nand_cycle.control)
     && !nand_we(second_pkt.data.nand_cycle.control)) {
        fprintf(stderr, "Not a Sandisk param packet!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        return 0;
    }

    packet_get_next(st, &third_pkt);
    if (nand_ale(third_pkt.data.nand_cycle.control)
     || nand_cle(third_pkt.data.nand_cycle.control)
     || nand_re(third_pkt.data.nand_cycle.control)) {
        fprintf(stderr, "Not a Sandisk param packet!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        evt_write_nand_unk(st, &third_pkt);
        return 0;
    }

    evt.addr = second_pkt.data.nand_cycle.data;
    evt.data = third_pkt.data.nand_cycle.data;

    evt_fill_end(&evt, third_pkt.header.sec, third_pkt.header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}


static int evt_write_sandisk_charge1(struct state *st, struct pkt *pkt) {
    struct evt_nand_sandisk_charge1 evt;
    struct pkt second_pkt, third_pkt, fourth_pkt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_SANDISK_CHARGE1);

    // Make sure the subsequent packet is an address
    packet_get_next(st, &second_pkt);
    if (!nand_ale(second_pkt.data.nand_cycle.control)
     || nand_cle(second_pkt.data.nand_cycle.control)
     || !nand_we(second_pkt.data.nand_cycle.control)) {
        fprintf(stderr, "Not a Sandisk charge(?) packet!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        return 0;
    }

    packet_get_next(st, &third_pkt);
    if (!nand_ale(third_pkt.data.nand_cycle.control)
     || nand_cle(third_pkt.data.nand_cycle.control)
     || !nand_we(third_pkt.data.nand_cycle.control)) {
        fprintf(stderr, "Not a Sandisk charge(?) packet!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        evt_write_nand_unk(st, &third_pkt);
        return 0;
    }

    packet_get_next(st, &fourth_pkt);
    if (!nand_ale(fourth_pkt.data.nand_cycle.control)
     || nand_cle(fourth_pkt.data.nand_cycle.control)
     || !nand_we(fourth_pkt.data.nand_cycle.control)) {
        fprintf(stderr, "Not a Sandisk charge(?) packet!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        evt_write_nand_unk(st, &third_pkt);
        evt_write_nand_unk(st, &fourth_pkt);
        return 0;
    }

    evt.addr[0] = second_pkt.data.nand_cycle.data;
    evt.addr[1] = third_pkt.data.nand_cycle.data;
    evt.addr[2] = fourth_pkt.data.nand_cycle.data;

    evt_fill_end(&evt, fourth_pkt.header.sec, fourth_pkt.header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}


static int evt_write_sandisk_charge2(struct state *st, struct pkt *pkt) {
    struct evt_nand_sandisk_charge2 evt;
    struct pkt second_pkt, third_pkt, fourth_pkt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_SANDISK_CHARGE1);

    // Make sure the subsequent packet is an address
    packet_get_next(st, &second_pkt);
    if (!nand_ale(second_pkt.data.nand_cycle.control)
     || nand_cle(second_pkt.data.nand_cycle.control)
     || !nand_we(second_pkt.data.nand_cycle.control)) {
        fprintf(stderr, "Not a Sandisk charge2(?) packet!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        return 0;
    }

    packet_get_next(st, &third_pkt);
    if (!nand_ale(third_pkt.data.nand_cycle.control)
     || nand_cle(third_pkt.data.nand_cycle.control)
     || !nand_we(third_pkt.data.nand_cycle.control)) {
        fprintf(stderr, "Not a Sandisk charge2(?) packet!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        evt_write_nand_unk(st, &third_pkt);
        return 0;
    }

    packet_get_next(st, &fourth_pkt);
    if (!nand_ale(fourth_pkt.data.nand_cycle.control)
     || nand_cle(fourth_pkt.data.nand_cycle.control)
     || !nand_we(fourth_pkt.data.nand_cycle.control)) {
        fprintf(stderr, "Not a Sandisk charge2(?) packet!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        evt_write_nand_unk(st, &third_pkt);
        evt_write_nand_unk(st, &fourth_pkt);
        return 0;
    }

    evt.addr[0] = second_pkt.data.nand_cycle.data;
    evt.addr[1] = third_pkt.data.nand_cycle.data;
    evt.addr[2] = fourth_pkt.data.nand_cycle.data;

    evt_fill_end(&evt, fourth_pkt.header.sec, fourth_pkt.header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}


static int evt_write_nand_reset(struct state *st, struct pkt *pkt) {
    struct evt_nand_reset evt;
    struct pkt second_pkt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_RESET);

    // Make sure the subsequent packet is 0xc5
    packet_get_next(st, &second_pkt);
    if (!nand_cle(second_pkt.data.nand_cycle.control)
     || second_pkt.data.nand_cycle.data != 0x00) {
        fprintf(stderr, "Not a reset packet!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        return 0;
    }

    evt_fill_end(&evt, second_pkt.header.sec, second_pkt.header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}


static int evt_write_nand_cache1(struct state *st, struct pkt *pkt) {
    struct evt_nand_cache1 evt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_CACHE1);
    evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}


static int evt_write_nand_cache2(struct state *st, struct pkt *pkt) {
    struct evt_nand_cache2 evt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_CACHE2);
    evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}


static int evt_write_nand_cache3(struct state *st, struct pkt *pkt) {
    struct evt_nand_cache3 evt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_CACHE3);
    evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}


static int evt_write_nand_cache4(struct state *st, struct pkt *pkt) {
    struct evt_nand_cache4 evt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_CACHE4);
    evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}


static int evt_write_nand_status(struct state *st, struct pkt *pkt) {
    struct evt_nand_status evt;
    struct pkt second_pkt;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_STATUS);

    // Make sure the subsequent packet is a read of status
    packet_get_next(st, &second_pkt);
    if (nand_ale(second_pkt.data.nand_cycle.control)
     || nand_cle(second_pkt.data.nand_cycle.control)
     || nand_we(second_pkt.data.nand_cycle.control)) {
        fprintf(stderr, "Not a NAND status packet!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        return 0;
    }

    evt.status = second_pkt.data.nand_cycle.data;

    evt_fill_end(&evt, second_pkt.header.sec, second_pkt.header.nsec);
	st->out_fdh->write((char *)&evt, sizeof(evt));
	return 0;
}


static int evt_write_nand_parameter_page(struct state *st, struct pkt *pkt) {
    struct evt_nand_parameter_read evt;
    struct pkt second_pkt;

    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_PARAMETER_READ);

    // Make sure the subsequent packet is a read of status
    packet_get_next(st, &second_pkt);
    if (!nand_ale(second_pkt.data.nand_cycle.control)
     || nand_cle(second_pkt.data.nand_cycle.control)
     || !nand_we(second_pkt.data.nand_cycle.control)) {
        fprintf(stderr, "Not a NAND parameter read!\n");
        evt_write_nand_unk(st, pkt);
        evt_write_nand_unk(st, &second_pkt);
        return 0;
    }

    evt.count = 0;
    evt.addr = second_pkt.data.nand_cycle.data;
    memset(evt.data, 0, sizeof(evt.data));

    evt.count = 0;

    evt_fill_end(&evt, second_pkt.header.sec, second_pkt.header.nsec);
    packet_get_next(st, pkt);
    while (nand_re(pkt->data.nand_cycle.control)) {
        evt.data[evt.count++] = pkt->data.nand_cycle.data;

        evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
        packet_get_next(st, pkt);
    }
    packet_unget(st, pkt);
    evt.count = _htons(evt.count);

    evt.hdr.size = sizeof(evt.hdr)
                 + sizeof(evt.addr)
                 + sizeof(evt.count)
                 + _htons(evt.count);
    evt.hdr.size = _htonl(evt.hdr.size);
	st->out_fdh->write((char *)&evt, _ntohl(evt.hdr.size));
	return 0;
}


static int evt_write_nand_change_read_column(struct state *st, struct pkt *pkt) {
    struct evt_nand_change_read_column evt;
    struct pkt pkts[6];
    int counter;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_CHANGE_READ_COLUMN);


    for (counter=0; counter<5; counter++) {
        // Make sure the subsequent packet is an address
        packet_get_next(st, &pkts[counter]);
        if (!nand_ale(pkts[counter].data.nand_cycle.control)
         || nand_cle(pkts[counter].data.nand_cycle.control)
         || !nand_we(pkts[counter].data.nand_cycle.control)) {
            int countdown;
            fprintf(stderr, "Not a page_select packet\n");
            evt_write_nand_unk(st, pkt);
            for (countdown=0; countdown<=counter; countdown++)
                evt_write_nand_unk(st, &pkts[countdown]);
            return 0;
        }
    }

    // Next one should be a command, with type 0xe0
    packet_get_next(st, &pkts[counter]);
    if (nand_ale(pkts[counter].data.nand_cycle.control)
     || !nand_cle(pkts[counter].data.nand_cycle.control)
     || !nand_we(pkts[counter].data.nand_cycle.control)
     || pkts[counter].data.nand_cycle.data != 0xe0) {
        int countdown;
        fprintf(stderr, "Not a page_select packet (last packet wrong)\n");
        evt_write_nand_unk(st, pkt);
        for (countdown=0; countdown<=counter; countdown++)
            evt_write_nand_unk(st, &pkts[countdown]);
        return 0;
    }

    evt.addr[0] = pkts[0].data.nand_cycle.data;
    evt.addr[1] = pkts[1].data.nand_cycle.data;
    evt.addr[2] = pkts[2].data.nand_cycle.data;
    evt.addr[3] = pkts[3].data.nand_cycle.data;
    evt.addr[4] = pkts[4].data.nand_cycle.data;
    evt.addr[5] = pkts[5].data.nand_cycle.data;

    evt.count = 0;
    evt_fill_end(&evt, pkts[6].header.sec, pkts[6].header.nsec);
    memcpy(evt.unknown, &pkt->data.nand_cycle.unknown, sizeof(evt.unknown));
    packet_get_next(st, pkt);
    while (nand_re(pkt->data.nand_cycle.control)) {
        evt.data[evt.count++] = pkt->data.nand_cycle.data;

        evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
        memcpy(evt.unknown, &pkt->data.nand_cycle.unknown, sizeof(evt.unknown));
        packet_get_next(st, pkt);
    }
    packet_unget(st, pkt);

    evt.hdr.size = sizeof(evt.hdr)
                 + sizeof(evt.addr)
                 + sizeof(evt.count)
                 + sizeof(evt.unknown)
                 + evt.count;
    evt.hdr.size = _htonl(evt.hdr.size);

    evt.count = _htonl(evt.count);
	st->out_fdh->write((char *)&evt, _ntohl(evt.hdr.size));
	return 0;
}


static int evt_write_nand_read(struct state *st, struct pkt *pkt) {
    struct evt_nand_read evt;
    struct pkt pkts[6];
    int counter;
    evt_fill_header(&evt, pkt->header.sec, pkt->header.nsec,
                    sizeof(evt), EVT_NAND_READ);


    for (counter=0; counter<5; counter++) {
        // Make sure the subsequent packet is an address
        packet_get_next(st, &pkts[counter]);
        if (!nand_ale(pkts[counter].data.nand_cycle.control)
         || nand_cle(pkts[counter].data.nand_cycle.control)
         || !nand_we(pkts[counter].data.nand_cycle.control)) {
            int countdown;
            fprintf(stderr, "Not a nand_read packet (counter %d)\n", counter);
            evt_write_nand_unk(st, pkt);
            for (countdown=0; countdown<=counter; countdown++)
                evt_write_nand_unk(st, &pkts[countdown]);
            return 0;
        }
    }

    // Next one should be a command, with type 0xe0
    packet_get_next(st, &pkts[counter]);
    if (nand_ale(pkts[counter].data.nand_cycle.control)
     || !nand_cle(pkts[counter].data.nand_cycle.control)
     || !nand_we(pkts[counter].data.nand_cycle.control)
     || pkts[counter].data.nand_cycle.data != 0x30) {
        int countdown;
        fprintf(stderr, "Not a nand_read packet (last packet wrong)\n");
        evt_write_nand_unk(st, pkt);
        for (countdown=0; countdown<=counter; countdown++)
            evt_write_nand_unk(st, &pkts[countdown]);
        return 0;
    }

    evt.addr[0] = pkts[0].data.nand_cycle.data;
    evt.addr[1] = pkts[1].data.nand_cycle.data;
    evt.addr[2] = pkts[2].data.nand_cycle.data;
    evt.addr[3] = pkts[3].data.nand_cycle.data;
    evt.addr[4] = pkts[4].data.nand_cycle.data;
    evt.addr[5] = pkts[5].data.nand_cycle.data;

    evt.count = 0;
    evt_fill_end(&evt, pkts[5].header.sec, pkts[5].header.nsec);
    memcpy(evt.unknown, &pkt->data.nand_cycle.unknown, sizeof(evt.unknown));
    packet_get_next(st, pkt);
    while (nand_re(pkt->data.nand_cycle.control)) {
        evt.data[evt.count++] = pkt->data.nand_cycle.data;

        evt_fill_end(&evt, pkt->header.sec, pkt->header.nsec);
        memcpy(evt.unknown, &pkt->data.nand_cycle.unknown, sizeof(evt.unknown));
        packet_get_next(st, pkt);
    }
    packet_unget(st, pkt);

    evt.hdr.size = sizeof(evt.hdr)
                 + sizeof(evt.addr)
                 + sizeof(evt.count)
                 + sizeof(evt.unknown)
                 + evt.count;
    evt.hdr.size = _htonl(evt.hdr.size);

    evt.count = _htonl(evt.count);

	st->out_fdh->write((char *)&evt, _ntohl(evt.hdr.size));
	return 0;
}



static int write_nand_cmd(struct state *st, struct pkt *pkt) {
    struct pkt_nand_cycle *nand = &pkt->data.nand_cycle;

    // If it's not a command, we're lost
    if (!nand_cle(nand->control)) {
        evt_write_nand_unk(st, pkt);
        return 0;
    }

    // "Get ID" command
    if (nand->data == 0x90) {
        evt_write_id(st, pkt);
    }
    else if (nand->data == 0x5c) {
        evt_write_sandisk_set(st, pkt);
    }
    else if (nand->data == 0xff) {
        evt_write_nand_reset(st, pkt);
    }
    else if (nand->data == 0x55) {
        evt_write_sandisk_param(st, pkt);
    }
    else if (nand->data == 0x70) {
        evt_write_nand_status(st, pkt);
    }
    else if (nand->data == 0xec) {
        evt_write_nand_parameter_page(st, pkt);
    }
    else if (nand->data == 0x60) {
        evt_write_sandisk_charge2(st, pkt);
    }
    else if (nand->data == 0x65) {
        evt_write_sandisk_charge1(st, pkt);
    }
    else if (nand->data == 0x05) {
        evt_write_nand_change_read_column(st, pkt);
    }
    else if (nand->data == 0x00) {
        evt_write_nand_read(st, pkt);
    }
    else if (nand->data == 0x30) {
        evt_write_nand_cache1(st, pkt);
    }
    else if (nand->data == 0xa2) {
        evt_write_nand_cache2(st, pkt);
    }
    else if (nand->data == 0x69) {
        evt_write_nand_cache3(st, pkt);
    }
    else if (nand->data == 0xfd) {
        evt_write_nand_cache4(st, pkt);
    }
    else {
        evt_write_nand_unk(st, pkt);
    }
    return 0;
}


// Initialize the "group" state machine
struct state *gstate_init() {
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

int gstate_free(struct state **st) {
    free(*st);
    *st = NULL;
    return 0;
}

int gstate_state(struct state *st) {
    return st->st;
}

int gstate_run(struct state *st) {
    return st_funcs[st->st](st);
}

void *evt_take(struct state *st, int type) {
    unsigned int i;
    for (i=0; i<(sizeof(st->events)/sizeof(st->events[0])); i++) {
        if (st->events[i] && st->events[i]->type == type) {
            void *val = st->events[i];
            st->events[i] = NULL;
            return val;
        }
    }
    return NULL;
}

int evt_put(struct state *st, void *v) {
	struct evt_header *val = (struct evt_header *)v;
    unsigned int i;
    for (i=0; i<(sizeof(st->events)/sizeof(st->events[0])); i++) {
        if (!st->events[i]) {
            st->events[i] = val;
            return 0;
        }
    }
    return 1;
}



// Dummy state that should never be reached
static int st_uninitialized(struct state *st) {
    Q_UNUSED(st);
    printf("state error: should not be in this state\n");
    return -1;
}

// Searching for either a NAND block or a sync point
static int st_scanning(struct state *st) {
    struct pkt pkt;
    int ret;
    while ((ret = packet_get_next(st, &pkt)) == 0) {

        if (pkt.header.type == PACKET_HELLO) {
            evt_write_hello(st, &pkt);
        }

        else if (pkt.header.type == PACKET_RESET) {
            evt_write_reset(st, &pkt);
        }

        else if (pkt.header.type == PACKET_NAND_CYCLE) {
            write_nand_cmd(st, &pkt);
        }

        else if (pkt.header.type == PACKET_COMMAND) {
            if (pkt.data.command.start_stop == CMD_STOP) {
				struct evt_net_cmd *net = (struct evt_net_cmd *)evt_take(st, EVT_NET_CMD);
                if (!net) {
                    struct evt_net_cmd evt;
                    fprintf(stderr, "NET_CMD end without begin\n");
                    evt_fill_header(&evt, pkt.header.sec, pkt.header.nsec,
                                    sizeof(evt), EVT_NET_CMD);
                    evt.cmd[0] = pkt.data.command.cmd[0];
                    evt.cmd[1] = pkt.data.command.cmd[1];
                    evt.arg = pkt.data.command.arg;
                    evt_fill_end(&evt, pkt.header.sec, pkt.header.nsec);
                    evt.arg = _htonl(evt.arg);
					st->out_fdh->write((char *)&evt, sizeof(evt));
				}
                else {
                    evt_fill_end(net, pkt.header.sec, pkt.header.nsec);
                    net->arg = _htonl(net->arg);
					st->out_fdh->write((char *)net, sizeof(*net));
					free(net);
                }
            }
            else {
				struct evt_net_cmd *net = (struct evt_net_cmd *)evt_take(st, EVT_NET_CMD);
                if (net) {
                    fprintf(stderr, "Multiple NET_CMDs going at once\n");
                    free(net);
                }

				net = (struct evt_net_cmd *)malloc(sizeof(struct evt_net_cmd));
                evt_fill_header(net, pkt.header.sec, pkt.header.nsec,
                                sizeof(*net), EVT_NET_CMD);
                net->cmd[0] = pkt.data.command.cmd[0];
                net->cmd[1] = pkt.data.command.cmd[1];
                net->arg = pkt.data.command.arg;
                evt_put(st, net);
            }
        }

        else if (pkt.header.type == PACKET_BUFFER_DRAIN) {
            if (pkt.data.buffer_drain.start_stop == PKT_BUFFER_DRAIN_STOP) {
				struct evt_buffer_drain *evt = (struct evt_buffer_drain *)evt_take(st, EVT_BUFFER_DRAIN);
                if (!evt) {
                    struct evt_buffer_drain evt;
                    fprintf(stderr, "BUFFER_DRAIN end without begin\n");
                    evt_fill_header(&evt, pkt.header.sec, pkt.header.nsec,
                                    sizeof(evt), EVT_BUFFER_DRAIN);
                    evt_fill_end(&evt, pkt.header.sec, pkt.header.nsec);
					st->out_fdh->write((char *)&evt, sizeof(evt));
                }
                else {
                    evt_fill_end(evt, pkt.header.sec, pkt.header.nsec);
					st->out_fdh->write((char *)evt, sizeof(*evt));
                    free(evt);
                }
            }
            else {
				struct evt_buffer_drain *evt = (struct evt_buffer_drain *)evt_take(st, EVT_BUFFER_DRAIN);
                if (evt) {
                    fprintf(stderr, "Multiple BUFFER_DRAINs going at once\n");
                    free(evt);
                }

				evt = (struct evt_buffer_drain *)malloc(sizeof(struct evt_buffer_drain));
                evt_fill_header(evt, pkt.header.sec, pkt.header.nsec,
                                sizeof(*evt), EVT_BUFFER_DRAIN);
                evt_put(st, evt);
            }
        }

        else if (pkt.header.type == PACKET_SD_CMD_ARG) {
			struct evt_sd_cmd *evt = (struct evt_sd_cmd *)evt_take(st, EVT_SD_CMD);
            struct pkt_sd_cmd_arg *sd = &pkt.data.sd_cmd_arg;
            if (!evt) {
				evt = (struct evt_sd_cmd *)malloc(sizeof(struct evt_sd_cmd));
                memset(evt, 0, sizeof(*evt));
                evt_fill_header(evt, pkt.header.sec, pkt.header.nsec,
                                sizeof(*evt), EVT_SD_CMD);
            }

            // Ignore args for CMD55
            if ((evt->num_args || sd->reg>0) && evt->cmd != (55|0x80)) {
                evt->args[evt->num_args++] = sd->val;
            }

            // Register 0 implies this is a CMD.
            else if (sd->reg == 0) {
                if ((sd->val&0x3f) == 55 || evt->cmd&0x80) {
                    evt->cmd = 0x80 | (0x3f & sd->val);
                }
                else {
                    evt->cmd = 0x3f & sd->val;
                }
            }
            evt_put(st, evt);
        }
        else if (pkt.header.type == PACKET_SD_RESPONSE) {
			struct evt_sd_cmd *evt = (struct evt_sd_cmd *)evt_take(st, EVT_SD_CMD);
            // Ignore CMD17, as we'll pick it up on the PACKET_SD_DATA packet
            // Also ignore CMD55, as it'll become an ACMD later on
            if (evt->cmd == 17 || evt->cmd == (55|0x80)) {
                evt_put(st, evt);
            }
            else {
                struct pkt_sd_response *sd = &pkt.data.response;
                if (!evt) {
                    fprintf(stderr, "Couldn't find old EVT_SD_CMD in SD_RESPONSE\n");
                    continue;
                }

                evt->result[evt->num_results++] = sd->byte;
                evt->num_results = _htonl(evt->num_results);
                evt->num_args = _htonl(evt->num_args);

                evt_fill_end(evt, pkt.header.sec, pkt.header.nsec);
				st->out_fdh->write((char *)evt, _ntohl(evt->hdr.size));
                free(evt);
            }
        }

        else if (pkt.header.type == PACKET_SD_DATA) {
			struct evt_sd_cmd *evt = (struct evt_sd_cmd *)evt_take(st, EVT_SD_CMD);
            struct pkt_sd_data *sd = &pkt.data.sd_data;
            unsigned int offset;
            if (!evt) {
                fprintf(stderr, "Couldn't find old SD_EVT_CMD in SD_DATA\n");
                continue;
            }

            for (offset=0; offset<sizeof(sd->data); offset++)
                evt->result[evt->num_results++] = sd->data[offset];

            evt->num_results = _htonl(evt->num_results);
            evt->num_args = _htonl(evt->num_args);
            evt_fill_end(evt, pkt.header.sec, pkt.header.nsec);
            st->out_fdh->write((char *)evt, _ntohl(evt->hdr.size));
            free(evt);
        }

        else {
			printf("Unknown packet type: %d\n", pkt.header.type);
        }
    }

    return ret;
}

static int st_grouping(struct state *st) {
    Q_UNUSED(st);
    return 0;
}

static int st_done(struct state *st) {
    Q_UNUSED(st);
    printf("Done.\n");
    return 0;
}

