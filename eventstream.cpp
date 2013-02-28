#include <QDebug>
#include "eventstream.h"
#include "byteswap.h"

static const char *EVENT_HDR_1 = "TBEv";
static const char *EVENT_HDR_2 = "MaDa";



EventStream::EventStream(QObject *parent) :
    QObject(parent)
{
}

int EventStream::load(QIODevice &source)
{
    uint8_t sig[4];
    int32_t size;
    int offset;

    memset(sig, 0, sizeof(sig));
    if (source.read((char *)sig, sizeof(sig)) != sizeof(sig)) {
        qDebug() << "Unable to read stream: " << source.errorString();
        return -1;
    }

    if (memcmp(sig, EVENT_HDR_1, sizeof(sig))) {
        qDebug() << "Error: File signature doesn't match";
        qDebug("%x%x%x%x vs %x%x%x%x\n", sig[0], sig[1], sig[2], sig[3],
                EVENT_HDR_1[0], EVENT_HDR_1[1], EVENT_HDR_1[2], EVENT_HDR_1[3]);
        return -1;
    }

    if (source.read((char *)&size, sizeof(size)) != sizeof(size)) {
        qDebug() << "Unable to read element count: " << source.errorString();
        return -1;
    }
    size = _ntohl(size);

    for (offset=0; offset<size; offset++) {
        uint32_t addr;
        if (source.read((char *)&addr, sizeof(addr)) != sizeof(addr)) {
            qDebug() << "Unable to read addr table: " << source.errorString();
            return -1;
        }
        addr = _ntohl(addr);
        _offsets.append(addr);
    }
    
    if (source.read((char *)sig, sizeof(sig)) != sizeof(sig)) {
        qDebug() << "Unable to read stream: " << source.errorString();
        return -1;
    }

    if (memcmp(sig, EVENT_HDR_2, sizeof(sig))) {
        qDebug() << "Error: Main signature doesn't match";
        qDebug("%x%x%x%x vs %x%x%x%x\n", sig[0], sig[1], sig[2], sig[3],
                EVENT_HDR_2[0], EVENT_HDR_2[1], EVENT_HDR_2[2], EVENT_HDR_2[3]);
        return -1;
    }

    for (offset=0; offset<size; offset++) {
        Event e(source, this);
        _events.append(e);
    }

    _currentEvents = _events;

    return 0;
}

const Event &EventStream::eventAt(int offset) const
{
    return _currentEvents.at(offset);
}

int EventStream::count() const
{
    return _currentEvents.count();
}

int EventStream::ignoreEventsOfType(int type)
{
    QList<Event> tempList;
    int i;
    for (i=0; i<_currentEvents.count(); i++)
        if (_currentEvents.at(i).eventType() != type)
            tempList.append(_currentEvents.at(i));
    _currentEvents = tempList;
    return 0;
}

int EventStream::resetIgnoredEvents()
{
    _currentEvents = _events;
    return 0;
}
