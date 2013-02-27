#ifndef NANDVIEW_H
#define NANDVIEW_H

#include <QAbstractScrollArea>

class EventItemModel;
class NandView : public QAbstractScrollArea
{
	Q_OBJECT
	
public:
	explicit NandView(QWidget *parent = 0);
	virtual ~NandView();
	
protected:
	virtual void paintEvent(QPaintEvent *event);
	virtual void resizeEvent(QResizeEvent *event);
	virtual void keyPressEvent(QKeyEvent *event);

public Q_SLOTS:
	void repaint();

private:
	void updateScrollbars();
};

#endif // NANDVIEW_H
