
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QDebug>
#include <QPlainTextEdit>
#include <QMouseEvent>
#include "histogramview.h"

static bool numericalLessThan(QPair<int,int> i1, QPair<int,int> i2)
{
	if (i1.second < i2.second)
		return true;
	if (i1.second > i2.second)
		return false;
	if (i1.first < i2.first)
		return true;
	return false;
}

HistogramView::HistogramView(QWidget *parent) :
    QWidget(parent)
{
	statsOutput = NULL;
}


void HistogramView::setData(const QByteArray &newData)
{
	unsigned int index;

	data = newData;
	setMouseTracking(true);

	buckets.clear();
	for (index=0; index<MAX_VALUE; index++)
		buckets.append(QPair<int,int>(index, 0));

	for(index = 0; index < (unsigned int) newData.size(); index++) {
		buckets[newData.at(index) & 0xff].second++;
	}

	sortedBuckets = buckets;
	qSort(sortedBuckets.begin(), sortedBuckets.end(), numericalLessThan);

	/////// text box update
	if (statsOutput) {
		QString histVals = "";
		for(index=0; index<MAX_VALUE; index++) {
			histVals += QString("0x%1 occurs %2 times\n")
						.arg(sortedBuckets[index].first, 2, 16)
						.arg(sortedBuckets[index].second);
		}
		statsOutput->clear();
		statsOutput->appendPlainText(histVals);
	}

    update();
}

void HistogramView::setStatsOutput(QPlainTextEdit *newStatsOutput)
{
	statsOutput = newStatsOutput;
}

void HistogramView::mouseMoveEvent(QMouseEvent *event)
{
	qreal percentage = ((qreal)MAX_VALUE)/((qreal)width());
	mouseY = percentage * event->x();
	setToolTip(QString("Value at %1: %2").arg((long)mouseY,2,16).arg(buckets[mouseY].second));
	setStatusTip(QString("Value at %1: %2").arg((long)mouseY,2,16).arg(buckets[mouseY].second));
	update();
}

void HistogramView::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	unsigned int index;
	QPainter painter(this);
	QPen pen(Qt::black, 2, Qt::SolidLine);
	QPen bluePen(Qt::yellow, 2, Qt::SolidLine);
	qreal width = this->width();
	qreal height = this->height();

	qreal lineWidth = width/MAX_VALUE/2;
	qreal lineHeight = height;

	if (data.size() > 0) {

		float yscale = 0.0;
		for(index = 0; index < buckets.count(); index++) {
			if (yscale < buckets[index].second) {
				yscale = buckets[index].second;
            }
        }

		painter.setPen(pen);
		for(index = 0; index < buckets.count(); index++) {
			if (mouseY == index)
				painter.setPen(bluePen);
			else
				painter.setPen(pen);
			painter.drawLine(
						index*2*lineWidth,
						lineHeight - (buckets[index].second * lineHeight / yscale) + 3.0,
						index*2*lineWidth,
						lineHeight + 3.0);
		}

    }
}
