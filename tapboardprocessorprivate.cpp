
#include <QDebug>
#include <QThread>
#include "tapboardprocessorprivate.h"
#include "state.h"
#include "packet-struct.h"
#include "event-struct.h"

struct state;
extern "C" {
    struct state *jstate_init();
    int jstate_state(struct state *st);
    int jstate_run(struct state *st);
    int jstate_free(struct state **st);

    struct state *gstate_init();
    int gstate_state(struct state *st);
    int gstate_run(struct state *st);
    int gstate_free(struct state **st);

    struct state *sstate_init();
    int sstate_state(struct state *st);
    int sstate_run(struct state *st);
    int sstate_free(struct state **st);

    int packet_get_next_raw(struct state *st, struct pkt *pkt) {
        int ret;

        ret = read(st->fd, &pkt->header, sizeof(pkt->header));
        if (ret < 0)
            return -1;

        if (ret == 0)
            return -2;
        pkt->header.sec = ntohl(pkt->header.sec);
        pkt->header.nsec = ntohl(pkt->header.nsec);
        pkt->header.size = ntohs(pkt->header.size);

        ret = read(st->fd, &pkt->data, pkt->header.size-sizeof(pkt->header));
        if (ret < 0)
            return -1;

        if (ret == 0)
            return -2;

        return 0;
    }

    int packet_get_next(struct state *st, struct pkt *pkt) {
        int ret;
        ret = packet_get_next_raw(st, pkt);
        if (ret)
            return ret;
        if (pkt->header.type == PACKET_NAND_CYCLE)
            pkt->data.nand_cycle.data = nand_unscramble_byte(pkt->data.nand_cycle.data);
        return ret;
    }

    int packet_unget(struct state *st, struct pkt *pkt) {
        return lseek(st->fd, -pkt->header.size, SEEK_CUR);
    }

    int packet_write(struct state *st, struct pkt *pkt) {
        struct pkt cp;
        memcpy(&cp, pkt, sizeof(cp));
        cp.header.sec = htonl(pkt->header.sec);
        cp.header.nsec = htonl(pkt->header.nsec);
        cp.header.size = htons(pkt->header.size);

        return write(st->out_fd, &cp, ntohs(cp.header.size));
    }

    int event_get_next(struct state *st, union evt *evt) {
        int ret;
        int bytes_to_read;

        ret = read(st->fd, &evt->header, sizeof(evt->header));
        if (ret < 0) {
            perror("Couldn't read header");
            return -1;
        }

        if (ret == 0) {
            perror("End of file for header");
            return -2;
        }
        evt->header.sec_start = ntohl(evt->header.sec_start);
        evt->header.nsec_start = ntohl(evt->header.nsec_start);
        evt->header.sec_end = ntohl(evt->header.sec_end);
        evt->header.nsec_end = ntohl(evt->header.nsec_end);
        evt->header.size = ntohl(evt->header.size);

        bytes_to_read = evt->header.size - sizeof(evt->header);
        ret = read(st->fd,
                   ((char *)&(evt->header)) + sizeof(evt->header),
                   bytes_to_read);

        if (ret < 0) {
            perror("Couldn't read");
            return -1;
        }

        if (ret == 0 && bytes_to_read > 0) {
            perror("End of file");
            return -2;
        }

        return 0;
    }

    int event_unget(struct state *st, union evt *evt) {
        return lseek(st->fd, -evt->header.size, SEEK_CUR);
    }

    int event_write(struct state *st, union evt *evt) {
        int ret;
        evt->header.sec_start = htonl(evt->header.sec_start);
        evt->header.nsec_start = htonl(evt->header.nsec_start);
        evt->header.sec_end = htonl(evt->header.sec_end);
        evt->header.nsec_end = htonl(evt->header.nsec_end);
        evt->header.size = htonl(evt->header.size);
        ret = write(st->out_fd, evt, ntohl(evt->header.size));

        evt->header.sec_start = ntohl(evt->header.sec_start);
        evt->header.nsec_start = ntohl(evt->header.nsec_start);
        evt->header.sec_end = ntohl(evt->header.sec_end);
        evt->header.nsec_end = ntohl(evt->header.nsec_end);
        evt->header.size = ntohl(evt->header.size);

        return ret;
    }
};

TapboardProcessorPrivate::TapboardProcessorPrivate(QObject *parent)
    : QObject(parent)
{
    rawFile = new QFile;
    sortedFile = new QFile;
    joinedFile = new QTemporaryFile;
    groupedFile = new QTemporaryFile;
}

TapboardProcessorPrivate::~TapboardProcessorPrivate()
{
    delete rawFile;
    delete sortedFile;
    delete joinedFile;
    delete groupedFile;
}

void TapboardProcessorPrivate::setSourceFilename(QString &newSource)
{
    sourceFilename = newSource;
}

void TapboardProcessorPrivate::setTargetFilename(QString &newTarget)
{
    targetFilename = newTarget;
}

int TapboardProcessorPrivate::joinFile()
{
    qDebug() << "Starting join";

    int ret = 0;

    // Open the raw input file.
    rawFile->setFileName(sourceFilename);
    if (!rawFile->open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open raw file:" << rawFile->errorString();
        return -1;
    }

    // Join up the files
    if (!joinedFile->open()) {
        qDebug() << "Unable to open joined temporary file:" << joinedFile->errorString();
        return -1;
    }
    struct state *js = jstate_init();
    js->fd = rawFile->handle();
    js->out_fd = joinedFile->handle();
    while (jstate_state(js) != 1 && !ret)
        ret = jstate_run(js);
    jstate_free(&js);
    joinedFile->seek(0);

    qDebug() << "Done joining";
    emit joinFinished();
    return groupFile();
}

int TapboardProcessorPrivate::groupFile()
{
    qDebug() << "Starting group";

    int ret = 0;
    // Group files together
    if (!groupedFile->open()) {
        qDebug() << "Unable to open grouped temporary file:" << groupedFile->errorString();
        return -1;
    }
    struct state *gs = gstate_init();
    gs->fd = joinedFile->handle();
    gs->out_fd = groupedFile->handle();
    while (gstate_state(gs) != 1 && !ret)
        ret = gstate_run(gs);
    gstate_free(&gs);
    groupedFile->seek(0);

    qDebug() << "Group done";
    emit groupFinished();
    return sortFile();
}

int TapboardProcessorPrivate::sortFile()
{
    qDebug() << "Starting sort";

    int ret = 0;
    // Sort the output stream
    sortedFile->setFileName(targetFilename);
    if (!sortedFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "Unable to open sorted output file:" << sortedFile->errorString();
        return -1;
    }
    struct state *ss = sstate_init();
    ss->fd = groupedFile->handle();
    ss->out_fd = sortedFile->handle();
    while (sstate_state(ss) != 1 && !ret)
        ret = sstate_run(ss);
    sstate_free(&ss);
    sortedFile->close();

    qDebug() << "Done sort";
    emit sortFinished();
    QThread::currentThread()->exit();
    return 0;
}
