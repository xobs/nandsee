
#include <QDebug>
#include <QSize>
#include <QColor>
#include <QBrush>
#include <QPainter>
#include "eventitemmodel.h"

EventItemModel::EventItemModel(QObject *parent) :
	QAbstractItemModel(parent)
{
}

int EventItemModel::loadFile(QString &filename)
{
	QFile input;
	input.setFileName(filename);
	if (!input.open(QIODevice::ReadOnly)) {
		qDebug() << "Couldn't open event stream file: " << input.errorString();
		return -1;
	}
	return _events.load(input);
}

QModelIndex EventItemModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	if (!parent.isValid())
		return createIndex(row, column, row);

	// Child view.  Does not exist.
	return QModelIndex();
}

QModelIndex EventItemModel::parent(const QModelIndex &child) const
{
	Q_UNUSED(child);
	return QModelIndex();
}

int EventItemModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return _events.count();
}

int EventItemModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return 1;
}

QVariant EventItemModel::data(const QModelIndex &index, int role) const
{
	const Event e = _events.eventAt(index.row());

	if (role == Qt::SizeHintRole) {
		return QVariant(QSize(200,16));
	}

	else if (role == Qt::DisplayRole) {
		QString temp;
		temp = e.eventTypeStr();
		temp.remove(0, 4);
		return QVariant(temp);
	}

	else if (role == Qt::DecorationRole) {
		return QVariant();
	}

	else if (role == Qt::ToolTipRole) {
		return QVariant();
	}

	else if (role == Qt::FontRole)
		return QVariant();

	else if (role == Qt::TextAlignmentRole)
		return QVariant();

	else if (role == Qt::BackgroundColorRole) {
		if (e.entropy() >= 1.0)
			return QVariant();
		QImage imageBar(500, 16, QImage::Format_RGB32);
		QPainter painter(&imageBar);
		painter.fillRect(0, 0, 500, 16, QColor::fromRgb(255, 255, 255, 255));
		painter.fillRect(0, 0, (int)(10+(e.entropy()*200)), 8, QColor::fromRgb(255, 128, 128));
		painter.fillRect(0, 8, (int)(10+(e.data().size()/100.0)), 8, QColor::fromHsvF(.13, .34, .93));

		QBrush brush(imageBar);
		return brush;
	}

	else if (role == Qt::TextColorRole)
		return QVariant();

	else if (role == Qt::CheckStateRole)
		return QVariant();

	else if (role == Qt::StatusTipRole)
		return QVariant();

	qDebug() << "Unknown item role" << role;
	return QVariant();
}

const Event &EventItemModel::eventAt(int index)
{
	return _events.eventAt(index);
}

void EventItemModel::ignoreEventsOfType(int type)
{
    _events.ignoreEventsOfType(type);
}

void EventItemModel::resetIgnoredEvents()
{
    _events.resetIgnoredEvents();
}
