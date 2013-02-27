#ifndef NANDSEEWINDOW_H
#define NANDSEEWINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QItemSelectionModel>

namespace Ui {
class NandSeeWindow;
}

class EventItemModel;

class NandSeeWindow : public QMainWindow
{
	Q_OBJECT
	
public:
	explicit NandSeeWindow(QWidget *parent = 0);
	~NandSeeWindow();
	
public slots:
	void eventSelectionChanged(const QItemSelection &index, const QItemSelection &old);
	void changeLastSelected(const QModelIndex &index, const QModelIndex &old);
	void xorPatternChanged(const QString &text);

	void exportCurrentView();
	void exportCurrentPage();

private:
	Ui::NandSeeWindow *ui;
	EventItemModel *_eventItemModel;
	QItemSelectionModel *_eventItemSelections;
	QByteArray currentData;
	QModelIndex mostRecent;
	QByteArray _xorPattern;

	void updateEventDetails();
	void updateHexView();
};

#endif // NANDSEEWINDOW_H
