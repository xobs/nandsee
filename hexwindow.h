#ifndef HEXWINDOW_H
#define HEXWINDOW_H

#include <QMainWindow>

namespace Ui {
class HexWindow;
}

class HexWindow : public QMainWindow
{
	Q_OBJECT
	
public:
	explicit HexWindow(QWidget *parent = 0);
	~HexWindow();
	void setData(const QByteArray &data);

public slots:
	void closeWindow();

private:
	QByteArray _data;
	
private:
	Ui::HexWindow *ui;
};

#endif // HEXWINDOW_H
