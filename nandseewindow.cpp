
#include <QFileDialog>
#include <QDebug>
#include <QFile>
#include <QString>
#include <QCoreApplication>
#include <QScrollBar>

#include "nandseewindow.h"
#include "eventitemmodel.h"
#include "hexwindow.h"
#include "ui_nandseewindow.h"
#include "tapboardprocessor.h"
#include "nand.h"

NandSeeWindow::NandSeeWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::NandSeeWindow)
{
	ui->setupUi(this);

	QFileDialog selectFile(ui->centralWidget);
	selectFile.setFileMode(QFileDialog::ExistingFile);
    QStringList fileTypes;
	fileTypes << "Tap Board event files (*.tbevent)"
              << "Raw Tap Board capture files (*.tbraw)"
              ;
    selectFile.setNameFilters(fileTypes);
	if (!selectFile.exec()) {
		qDebug() << "No file selected";
		exit(0);
	}

	QString fileName;
	fileName = selectFile.selectedFiles()[0];

    // If we've got a raw tapboard file, process it first
    if (fileName.endsWith(".tbraw", Qt::CaseInsensitive)) {
        TapboardProcessor tp;
        if (tp.processRawFile(fileName)) {
            qDebug() << "Raw stream couldn't be processed";
            exit(0);
        }
    }

	_eventItemModel = new EventItemModel(this);
	if (_eventItemModel->loadFile(fileName)) {
		qDebug() << "Couldn't load file";
		exit(0);
	}
	_eventItemSelections = new QItemSelectionModel(_eventItemModel);
	ui->eventList->setModel(_eventItemModel);
	ui->eventList->setSelectionModel(_eventItemSelections);

    /* Wire everything up */
	connect(_eventItemSelections, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
			this, SLOT(eventSelectionChanged(QItemSelection, QItemSelection)));
	connect(_eventItemSelections, SIGNAL(currentChanged(QModelIndex,QModelIndex)),
			this, SLOT(changeLastSelected(QModelIndex,QModelIndex)));

	connect(ui->eventList, SIGNAL(doubleClicked(QModelIndex)),
			this, SLOT(openHexWindow(QModelIndex)));

    connect(ui->ignoreEventsAction, SIGNAL(triggered()),
            this, SLOT(ignoreEvents()));
    connect(ui->unignoreEventsAction, SIGNAL(triggered()),
            this, SLOT(unignoreEvents()));

	connect(ui->xorPattern, SIGNAL(textChanged(QString)),
			this, SLOT(xorPatternChanged(QString)));

	connect(ui->exportViewMenuItem, SIGNAL(triggered()),
			this, SLOT(exportCurrentView()));
	connect(ui->exportPageMenuItem, SIGNAL(triggered()),
			this, SLOT(exportCurrentPage()));

    connect(ui->actionHighlightMatches, SIGNAL(toggled(bool)),
            ui->hexView, SLOT(setHighlightSame(bool)));

    connect(ui->actionInvertValues, SIGNAL(toggled(bool)),
            ui->hexView, SLOT(setInvertValues(bool)));

	hideLabels();
}

NandSeeWindow::~NandSeeWindow()
{
	delete ui;
}

void NandSeeWindow::eventSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
	Q_UNUSED(selected);
	Q_UNUSED(deselected);
	updateHexView();

}

void NandSeeWindow::updateEventDetails()
{
	Event e = _eventItemModel->eventAt(mostRecent.row());

	QString temp;
	ui->startTimeLabel->setText(QString("%1.%2").arg(e.secondsStart()).arg(e.nanoSecondsStart(), 9, 10, QLatin1Char('0')));
	ui->endTimeLabel->setText(QString("%1.%2").arg(e.secondsEnd()).arg((uint)e.nanoSecondsEnd(), 9, 10, QLatin1Char('0')));
	int32_t duration_sec = e.secondsEnd() - e.secondsStart();
	int32_t duration_nsec = e.nanoSecondsEnd() - e.nanoSecondsStart();
	if (duration_nsec < 0) {
		duration_sec--;
		duration_nsec += 1000000000;
	}
	temp.sprintf("%d.%09d (", duration_sec, duration_nsec);
	if (duration_sec > 0)
		temp += QString("%1 sec, ").arg(QString::number(duration_sec));

	if (duration_nsec > 1000000)
		temp += QString("%1 msec").arg(QString::number((duration_nsec-duration_nsec%1000000)/1000000));
	else if(duration_nsec > 1000)
		temp += QString("%1 %2sec").arg(QString::number((duration_nsec-duration_nsec%1000)/1000)).arg(QString::fromUtf8("µ"));
	else
		temp += QString("%1 nsec").arg(QString::number(duration_nsec));
	temp += ")";
	ui->durationLabel->setText(temp);

	temp = e.eventTypeStr();
	temp.remove(0, 4);
	temp.replace("_", " ");
	temp.at(0).toTitleCase();
	QStringList tempList = temp.split(" ");
	for (int i=0; i<tempList.size(); i++) {
		QString thisString = tempList.at(i);
		QChar initial = thisString.at(0);
		thisString = thisString.toLower();
		thisString.replace(0, 1, initial);
		tempList.replace(i, thisString);
	}
	temp = tempList.join(" ");
	ui->eventTypeLabel->setText(temp);

	ui->entropyLabel->setText(QString::number(e.entropy()));
	ui->indexLabel->setText(QString::number(mostRecent.row()));

	hideLabels();

	QString rawPacketHex = "";
	for (unsigned int i=0; i<sizeof(evt_header); i++) {
		if (i>0)
			rawPacketHex += " ";
		rawPacketHex += QString("%1").arg(e.rawPacket().at(i)&0xff, 2, 16, QChar('0'));
	}
	ui->rawPacketHeader->setText(rawPacketHex);


	rawPacketHex = "";
    for (unsigned int i=sizeof(evt_header); i<e.rawPacketSize(); i++) {
		if (i>sizeof(evt_header))
			rawPacketHex += " ";
		rawPacketHex += QString("%1").arg(e.rawPacket().at(i)&0xff, 2, 16, QChar('0'));
	}
	ui->rawPacketView->clear();
	ui->rawPacketView->appendPlainText(rawPacketHex);
	ui->rawPacketView->verticalScrollBar()->setValue(0);

	if (e.eventType() == EVT_NAND_ID) {
		ui->attributeLine->setVisible(true);

		ui->attribute1Label->setText("NAND ID Address:");
		ui->attribute1Label->setVisible(true);
		ui->attribute1Value->setText(QString::number(e.nandIdAddr(), 16));
		ui->attribute1Value->setVisible(true);

		ui->attribute2Label->setText("NAND ID:");
		ui->attribute2Label->setVisible(true);
		ui->attribute2Value->setText(e.nandIdValue());
		ui->attribute2Value->setVisible(true);
	}
	else if (e.eventType() == EVT_NAND_SANDISK_VENDOR_PARAM) {
		ui->attributeLine->setVisible(true);
		ui->attribute1Label->setText("Sandisk Address:");
		ui->attribute1Label->setVisible(true);
		ui->attribute1Value->setText(QString::number(e.nandSakdiskParamAddr(), 16));
		ui->attribute1Value->setVisible(true);

		ui->attribute2Label->setText("Sandisk Value:");
		ui->attribute2Label->setVisible(true);
		ui->attribute2Value->setText(QString::number(e.nandSandiskParamData(), 16));
		ui->attribute2Value->setVisible(true);
	}
	else if (e.eventType() == EVT_NAND_STATUS) {
		ui->attributeLine->setVisible(true);
		ui->attribute1Label->setText("NAND Status:");
		ui->attribute1Label->setVisible(true);
		ui->attribute1Value->setText(QString::number(e.nandStatus(), 16));
		ui->attribute1Value->setVisible(true);
	}
	else if (e.eventType() == EVT_HELLO) {
		ui->attributeLine->setVisible(true);
		ui->attribute1Label->setText("Stream version:");
		ui->attribute1Label->setVisible(true);
		ui->attribute1Value->setText(QString::number(e.helloVersion()));
		ui->attribute1Value->setVisible(true);
	}
	else if (e.eventType() == EVT_NET_CMD) {
		ui->attributeLine->setVisible(true);
		ui->attribute1Label->setText("Network command:");
		ui->attribute1Label->setVisible(true);
		ui->attribute1Value->setText(e.netCmd());
		ui->attribute1Value->setVisible(true);

		ui->attribute2Label->setText("Argument:");
		ui->attribute2Label->setVisible(true);
		ui->attribute2Value->setText(QString::number(e.netArg()));
		ui->attribute2Value->setVisible(true);
	}
	if (e.eventType() == EVT_NAND_PARAMETER_READ) {
		ui->attributeLine->setVisible(true);
		ui->attribute1Label->setText("Param Addr:");
		ui->attribute1Label->setVisible(true);
		ui->attribute1Value->setText(QString::number(e.nandParameterAddr(), 16));
		ui->attribute1Value->setVisible(true);
	}
	if (e.eventType() == EVT_NAND_SANDISK_CHARGE1
		  || e.eventType() == EVT_NAND_SANDISK_CHARGE2) {
		ui->attributeLine->setVisible(true);
		ui->attribute1Label->setText("Charge Addr:");
		ui->attribute1Label->setVisible(true);
		ui->attribute1Value->setText(e.nandSandiskChargeAddr());
		ui->attribute1Value->setVisible(true);
	}
	if (e.eventType() == EVT_NAND_READ) {
		ui->attributeLine->setVisible(true);

        ui->attribute1Label->setText("Column:");
		ui->attribute1Label->setVisible(true);
        ui->attribute1Value->setText(e.nandReadColumnAddr());
		ui->attribute1Value->setVisible(true);

        ui->attribute2Label->setText("Row:");
        ui->attribute2Label->setVisible(true);
        ui->attribute2Value->setText(e.nandReadRowAddr());
        ui->attribute2Value->setVisible(true);
    }
	if (e.eventType() == EVT_NAND_CHANGE_READ_COLUMN) {
		ui->attributeLine->setVisible(true);

        ui->attribute1Label->setText("Column:");
        ui->attribute1Label->setVisible(true);
        ui->attribute1Value->setText(e.nandReadColumnAddr());
        ui->attribute1Value->setVisible(true);

        ui->attribute2Label->setText("Row:");
        ui->attribute2Label->setVisible(true);
        ui->attribute2Value->setText(e.nandReadRowAddr());
        ui->attribute2Value->setVisible(true);
    }
	if (e.eventType() == EVT_SD_CMD) {
		ui->attributeLine->setVisible(true);

		if (e.sdCmdIsACMD())
			ui->attribute1Label->setText(QString("ACMD%1 Args:").arg(e.sdCmdCMD()));
		else
			ui->attribute1Label->setText(QString("CMD%1 Args:").arg(e.sdCmdCMD()));
		ui->attribute1Label->setVisible(true);
		ui->attribute1Value->setText(e.sdCmdArgs());
		ui->attribute1Value->setVisible(true);

		if (e.data().size() == 1) {
			ui->attribute2Label->setText("Response:");
			ui->attribute2Label->setVisible(true);
			ui->attribute2Value->setText(QString::number(e.data().at(0), 16));
			ui->attribute2Value->setVisible(true);
		}
	}

	if (e.eventType() == EVT_NAND_UNKNOWN) {
		QStringList nandFlags;
		if (nand_ale(e.nandUnknownControl()))
			nandFlags.append("ALE");
		if (nand_cle(e.nandUnknownControl()))
			nandFlags.append("CLE");
		if (nand_we(e.nandUnknownControl()))
			nandFlags.append("WE");
		if (nand_re(e.nandUnknownControl()))
			nandFlags.append("RE");
		if (nand_rb(e.nandUnknownControl()))
			nandFlags.append("RB");
		ui->attributeLine->setVisible(true);
		ui->attribute1Label->setText("NAND Flags:");
		ui->attribute1Label->setVisible(true);
		ui->attribute1Value->setText(nandFlags.join(" "));
		ui->attribute1Value->setVisible(true);

		ui->attribute2Label->setText("NAND Data:");
		ui->attribute2Label->setVisible(true);
		ui->attribute2Value->setText(QString::number(e.nandUnknownData(), 16));
		ui->attribute2Value->setVisible(true);
	}
}

void NandSeeWindow::updateHexView()
{
	Event e = _eventItemModel->eventAt(mostRecent.row());
	const QModelIndexList indexes = _eventItemSelections->selectedIndexes();

	// Xor the data in the hex output
	currentData = 0;
	for (int i=0; i<indexes.count(); i++) {
		Event e = _eventItemModel->eventAt(indexes.at(i).row());
		const QByteArray currentArray = e.data();
		char *data = currentData.data();
		int position;

		// Resize the current data backing if necessary
		if (currentArray.size() > currentData.size()) {
			int oldSize = currentData.size();
			currentData.resize(currentArray.size());
			data = currentData.data();
			for (position=oldSize; position<currentArray.size(); position++)
				currentData[position] = 0;
		}

		// Xor the current data field onto the standing data backing
		for (position=0; position<currentData.size() && position<currentArray.size(); position++)
			data[position] ^= currentArray.at(position);
	}

	// Xor in the pattern, too
	if (_xorPattern.size() > 0) {
		char *data = currentData.data();
		for (int i=0; i<currentData.size(); i++) {
			data[i] ^= _xorPattern.at(i%_xorPattern.size());
		}
	}
    ui->hexView->setData(currentData);

	if (currentData.size())
		ui->dataSizeLabel->setText(QString::number(currentData.size()));
	else
		ui->dataSizeLabel->setText("-");
}

void NandSeeWindow::hideLabels()
{
	ui->attributeLine->setVisible(false);
	ui->attribute1Label->setVisible(false);
	ui->attribute1Value->setVisible(false);
	ui->attribute2Label->setVisible(false);
	ui->attribute2Value->setVisible(false);
    ui->attribute3Label->setVisible(false);
    ui->attribute3Value->setVisible(false);
}

void NandSeeWindow::changeLastSelected(const QModelIndex &index, const QModelIndex &old)
{
	Q_UNUSED(old);
	mostRecent = index;
	updateEventDetails();
    ui->ignoreEventsAction->setEnabled(true);
}

void NandSeeWindow::xorPatternChanged(const QString &text)
{
	QString tempString = text;

	tempString.remove("0x");
	tempString.remove(" ");
	tempString.remove(":");
	tempString.remove("-");
	_xorPattern = 0;

	for (int i=0; i<tempString.length(); i+=2) {
		QString hexChars;
		hexChars += tempString.at(i);
		if (i+1 < tempString.length())
			hexChars += tempString.at(i+1);
		else
			hexChars += "0";
		_xorPattern.append(hexChars.toInt(0, 16)&0xff);
	}
	updateHexView();
}

void NandSeeWindow::exportCurrentPage()
{
	QString suggestedName;
	QFileDialog selectFile(ui->centralWidget);
	selectFile.setFileMode(QFileDialog::AnyFile);
	selectFile.setAcceptMode(QFileDialog::AcceptSave);
	selectFile.setNameFilter("Page dump (*.bin)");
	suggestedName.sprintf("page-%d.bin", mostRecent.row());
	selectFile.selectFile(suggestedName);
	selectFile.selectNameFilter("bin");
	if (!selectFile.exec()) {
		qDebug() << "No file selected";
		return;
	}

	QString fileName;
	fileName = selectFile.selectedFiles()[0];

	QFile saveFile;
	saveFile.setFileName(fileName);
	if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qDebug() << "Couldn't open save file:" << saveFile.errorString();
		return;
	}

	Event e = _eventItemModel->eventAt(mostRecent.row());
	saveFile.write(e.data());
	saveFile.close();
	return;
}

void NandSeeWindow::exportCurrentView()
{
	QString suggestedName;
	QFileDialog selectFile(ui->centralWidget);
	selectFile.setFileMode(QFileDialog::AnyFile);
	selectFile.setAcceptMode(QFileDialog::AcceptSave);
	selectFile.setNameFilter("Page dump (*.bin)");
	suggestedName.sprintf("view-%d.bin", mostRecent.row());
	selectFile.selectFile(suggestedName);
	selectFile.selectNameFilter("bin");
	if (!selectFile.exec()) {
		qDebug() << "No file selected";
		return;
	}

	QString fileName;
	fileName = selectFile.selectedFiles()[0];

	QFile saveFile;
	saveFile.setFileName(fileName);
	if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		qDebug() << "Couldn't open save file:" << saveFile.errorString();
		return;
	}

	saveFile.write(currentData);
	saveFile.close();

	return;
}

void NandSeeWindow::openHexWindow(const QModelIndex &index)
{
	Event e = _eventItemModel->eventAt(index.row());
	HexWindow *newWindow;
	QString newWindowTitle = QString("Showing %1 @ %2").arg(e.eventTypeStr()).arg(QString::number(index.row()));
	newWindow = new HexWindow(this);
	newWindow->setData(e.data());
	newWindow->setWindowTitle(newWindowTitle);
	newWindow->show();
    connect(newWindow, SIGNAL(closeHexWindow(HexWindow*)),
            this, SLOT(closeHexWindow(HexWindow*)));
}

void NandSeeWindow::closeHexWindow(HexWindow *closingWindow)
{
    closingWindow->close();
    closingWindow->deleteLater();
}

void NandSeeWindow::ignoreEvents()
{
    _eventItemModel->ignoreEventsOfType(_eventItemModel->eventAt(mostRecent.row()).eventType());
    ui->eventList->reset();
    ui->unignoreEventsAction->setEnabled(true);
}

void NandSeeWindow::unignoreEvents()
{
    _eventItemModel->resetIgnoredEvents();
    ui->eventList->reset();
    ui->ignoreEventsAction->setEnabled(false);
}
