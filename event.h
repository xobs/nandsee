#ifndef EVENT_H
#define EVENT_H

#include <QObject>
#include <QIODevice>
#include <QDateTime>
#include <stdint.h>
#include <QFile>

#include "event-struct.h"

#define MAX_BUFFER_SIZE 4096

QList<QString> &EventTypes();

class Event : public QObject
{
    Q_OBJECT
public:
    explicit Event(QObject *parent = 0);
	Event(QIODevice &source, QObject *parent = 0);
	Event(QByteArray &data, QObject *parent = 0);

    Event() {}
    Event(const Event &other, QObject *parent = 0);
    Event &operator=(const Event &other);
    bool operator<(const Event &other) const;
	void decodeEvent();

	/* Used for writing data out */
	qint64 write(QIODevice &device);

	uint32_t nanoSecondsStart() const;
	uint32_t nanoSecondsEnd() const;
	uint32_t secondsStart() const;
	uint32_t secondsEnd() const;
	uint32_t eventSize() const;

	enum evt_type eventType() const;
	const QString &eventTypeStr() const;
	int index();
	int setIndex(int index);

    /* Hello Stream functions */
    uint8_t helloVersion() const;

	/* NAND ID command */
	uint8_t nandIdAddr() const;
	const QString &nandIdValue() const;

	/* NAND Change Read Column or NAND Read */
	const QByteArray &data() const;

    /* Network command */
    const QString &netCmd() const;
    uint32_t netArg() const;

	/* If there's data, how random is it? */
	qreal entropy() const;

	/* Sandisk vendor param */
	uint8_t nandSakdiskParamAddr() const;
	uint8_t nandSandiskParamData() const;

	/* NAND Status command */
	uint8_t nandStatus() const;

	/* Useful for debugging packet parsing */
	int rawPacketSize() const;
	const QByteArray &rawPacket() const;

private:
    union evt evt;
	QByteArray _dataAsByteArray;
	int eventIndex;
    QString nandIdString;
	QByteArray _data;
    QString _netCmd;
	double _entropy;

signals:
    
public slots:
    
};

QDebug operator<<(QDebug dbg, const Event &p);

#endif // EVENT_H
