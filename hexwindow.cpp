#include "hexwindow.h"
#include "ui_hexwindow.h"

HexWindow::HexWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::HexWindow)
{
	ui->setupUi(this);
	connect(ui->actionClose, SIGNAL(triggered()),
			this, SLOT(closeWindow()));
}

HexWindow::~HexWindow()
{
	delete ui;
}

void HexWindow::setData(const QByteArray &data)
{
	_data = data;
	ui->hexView->setData(_data);
}

void HexWindow::closeWindow()
{
	close();
    emit closeHexWindow(this);
}
