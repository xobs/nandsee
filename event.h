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

#if 0
    /* NAND Command/Data functions */
	uint8_t nandControl() const;
	uint8_t nandData() const;
	uint16_t nandUnknown() const;

    /* Error functions */
	const QString &errorStr() const;
	uint32_t errorCode() const;
	uint32_t errorArgument() const;
	uint32_t errorSubsystem() const;

    /* SD Command or Data functions */
	uint8_t sdRegister() const;
	uint8_t sdValue() const;

	/* SD response */
	uint8_t sdResponse() const;

	/* Command state */
	uint8_t commandState() const;
	const QString &commandName() const;
	uint32_t commandArg() const;

	/* Hello state */
	uint8_t helloVersion() const;

	/* Buffer drain started/stopped */
	uint8_t bufferDrainEvent() const;

	/* Reset event */
	uint8_t resetVersion() const;

	/* Get CSD */
	const QString &csd() const;

	/* Get CID */
	const QString &cid() const;
#endif
    /* NAND ID command */
    const QString &nandId() const;

	/* NAND Change Read Column or NAND Read */
	const QByteArray &data() const;

    /* Network command */
    const QString &netCmd() const;
    uint32_t netArg() const;

	/* If there's data, how random is it? */
	qreal entropy() const;

private:
    union evt evt;
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
