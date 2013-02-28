#ifndef EVENTITEMMODEL_H
#define EVENTITEMMODEL_H

#include <QAbstractItemModel>
#include "eventstream.h"

class EventItemModel : public QAbstractItemModel
{
	Q_OBJECT
public:
	explicit EventItemModel(QObject *parent = 0);
	int loadFile(QString &filename);
	int rowCount(const QModelIndex &parent = QModelIndex()) const ;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
	QModelIndex index(int row, int column, const QModelIndex &parent) const;
	QModelIndex parent(const QModelIndex &child) const;

	const Event &eventAt(int index);

    void ignoreEventsOfType(int type);
    void resetIgnoredEvents();

private:
	EventStream _events;
    EventStream _currentEvents;

signals:
	
public slots:
	
};

#endif // EVENTITEMMODEL_H
