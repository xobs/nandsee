#include <QDebug>
#include <QPainter>
#include <QScrollBar>
#include "nandview.h"

NandView::NandView(QWidget *parent) :
	QAbstractScrollArea(parent)
{
}

NandView::~NandView()
{
}

//------------------------------------------------------------------------------
// Name: repaint
// Desc:
//------------------------------------------------------------------------------
void NandView::repaint() {
		viewport()->repaint();
}


//------------------------------------------------------------------------------
// Name: keyPressEvent
// Desc:
//------------------------------------------------------------------------------
void NandView::keyPressEvent(QKeyEvent *event) {
	qDebug() << "Key pressed:" << event;
}

//------------------------------------------------------------------------------
// Name: resizeEvent
//------------------------------------------------------------------------------
void NandView::resizeEvent(QResizeEvent *) {
		updateScrollbars();
}


//------------------------------------------------------------------------------
// Name: paintEvent
//------------------------------------------------------------------------------
void NandView::paintEvent(QPaintEvent *) {
	QPainter painter(viewport());
	int font_width_ = 12;
	int chars_per_row = 8;
	int origin_ = 0;
	painter.translate(-horizontalScrollBar()->value() * font_width_, 0);

	// current actual offset (in bytes)
	quint64 offset = (quint64)verticalScrollBar()->value() * chars_per_row;

	if(origin_ != 0) {
			if(offset > 0) {
					offset += origin_;
					offset -= chars_per_row;
			} else {
					origin_ = 0;
					updateScrollbars();
			}
	}
}

//------------------------------------------------------------------------------
// Name: updateScrollbars
// Desc: recalculates scrollbar maximum value base on number of NAND blocks
//------------------------------------------------------------------------------
void NandView::updateScrollbars() {
	/*
	const qint64 sz = dataSize();
	const int bpr = bytesPerRow();

	qint64 maxval = sz / bpr + ((sz % bpr) ? 1 : 0) - viewport()->height() / font_height_;
	static_cast<int>((line3() - viewport()->width()) / font_width_)));
	*/
	qint64 maxval = 200;
	verticalScrollBar()->setMaximum(qMax((qint64)0, maxval));
	horizontalScrollBar()->setMaximum(qMax(0, 500));
}
