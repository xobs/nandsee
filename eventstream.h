#ifndef EVENTSTREAM_H
#define EVENTSTREAM_H

#include <QObject>
#include <QVector>
#include <QIODevice>
#include "event.h"

class EventStream : public QObject
{
    Q_OBJECT
public:
    explicit EventStream(QObject *parent = 0);
    int load(QIODevice &source);
	const Event &eventAt(int offset) const;
	int count() const;
    int ignoreEventsOfType(int type);
    int resetIgnoredEvents();

private:
    QList<Event> _events;
    QList<Event> _currentEvents;
    QList<uint32_t> _offsets;

signals:
    
public slots:
    
};

#endif // EVENTSTREAM_H
