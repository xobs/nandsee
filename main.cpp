#include <QtGui/QApplication>
#include "nandseewindow.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	NandSeeWindow w;
	w.show();
	
	return a.exec();
}
