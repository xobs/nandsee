
#include <QDebug>
#include <QSize>
#include <QColor>
#include <QBrush>
#include <QPainter>
#include "eventitemmodel.h"
#include "nand.h"

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

QVariant EventItemModel::drawEntropyBackground(const Event &e) const
{
	QImage imageBar(500, 16, QImage::Format_RGB32);
	QPainter painter(&imageBar);
	painter.fillRect(0, 0, 2500, 16, QColor::fromRgb(255, 255, 255, 255));
	painter.fillRect(0, 0, (int)(10+(e.entropy()*200)), 8, QColor::fromRgb(255, 128, 128));
	painter.fillRect(0, 8, (int)(10+(e.data().size()/100.0)), 8, QColor::fromHsvF(.13, .34, .93));

	QBrush brush(imageBar);
	return brush;
}

QVariant EventItemModel::drawNandUnknownBackground(const Event &e) const
{
	QImage imageBar(500, 16, QImage::Format_RGB32);
	qreal onS = .95;
	qreal offS = .13;
	qreal onV = .83;
	qreal offV = .98;
	uint8_t (*nand_ctrl[])(uint8_t ctrl) = {
		nand_ale,
		nand_cle,
		nand_we,
		nand_re,
		nand_rb,
	};
	qreal colors[] = {
		.1,
		.4,
		.8,
		.6,
		.33,
	};

	int top = 4;
	int left = 120;
	int height = 8;
	int width = 14;

	QPainter painter(&imageBar);
	painter.fillRect(0, 0, 2500, 16, QColor::fromRgb(255, 255, 255, 255));

	painter.setPen(QColor::fromRgb(192,192,192));

	for (int i=0; i<5; i++) {
		painter.drawRect(left, top-1, width+1, height+1);
		left++;
		if (nand_ctrl[i](e.nandUnknownControl()))
			painter.fillRect(left, top, width, height, QColor::fromHsvF(colors[i], onS, onV));
		else
			painter.fillRect(left, top, width, height, QColor::fromHsvF(colors[i], offS, offV));
		left += 20;
	}

	QBrush brush(imageBar);
	return brush;
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
		if (e.eventType() == EVT_NAND_UNKNOWN)
			return drawNandUnknownBackground(e);
		if (e.entropy() < 1.0)
			return drawEntropyBackground(e);
		return QVariant();
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
