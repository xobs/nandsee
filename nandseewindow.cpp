
#include <QFileDialog>
#include <QDebug>
#include <QFile>
#include <QString>
#include <QCoreApplication>
#include <QScrollBar>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include "histogramview.h"

#include "nandseewindow.h"
#include "eventitemmodel.h"
#include "hexwindow.h"
#include "ui_nandseewindow.h"
#include "tapboardprocessor.h"
#include "nand.h"

#define _USE_MATH_DEFINES
#include <math.h>

#define PI M_PI

#define	Z_MAX          6.0            /* maximum meaningful z value */
#define	LOG_SQRT_PI     0.5723649429247000870717135 /* log (sqrt (pi)) */
#define	I_SQRT_PI       0.5641895835477562869480795 /* 1 / sqrt (pi) */
#define	BIGX           20.0         /* max value to represent exp (x) */
#define	ex(x)             (((x) < -BIGX) ? 0.0 : exp(x))
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

    this->setWindowTitle("nandsee - " + fileName);

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

    connect(ui->optimizeXor, SIGNAL(clicked()),
            this, SLOT(optimizeXor()));

    connect(ui->lastAlignOffset, SIGNAL(valueChanged(int)),
            this, SLOT(updateAlign(int)));

    hideLabels();

    totalc = 0; // required init by entropy module
    lastAlignAt = 0;
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

/*
	QString rawPacketHex = "";
	for (unsigned int i=0; i<sizeof(evt_header); i++) {
		if (i>0)
			rawPacketHex += " ";
		rawPacketHex += QString("%1").arg(e.rawPacket().at(i)&0xff, 2, 16, QChar('0'));
	}
	ui->rawPacketHeader->setText(rawPacketHex);


	rawPacketHex = "";
	for (unsigned int i=sizeof(evt_header); i<(unsigned int)e.rawPacketSize(); i++) {
		if (i>sizeof(evt_header))
			rawPacketHex += " ";
		rawPacketHex += QString("%1").arg(e.rawPacket().at(i)&0xff, 2, 16, QChar('0'));
	}
	ui->rawPacketView->clear();
	ui->rawPacketView->appendPlainText(rawPacketHex);
	ui->rawPacketView->verticalScrollBar()->setValue(0);
*/

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

void NandSeeWindow::updateAlign(int value)
{
    this->lastAlignAt = value;
    updateHexView();
}

void NandSeeWindow::updateHexView()
{
	if (mostRecent.row() < 0)
		return;
	Event e = _eventItemModel->eventAt(mostRecent.row());
	const QModelIndexList indexes = _eventItemSelections->selectedIndexes();

    ui->lastAlignOffset->setValue(lastAlignAt);
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
        for (int i=0; i<currentData.size() - lastAlignAt; i++) {
            data[i + lastAlignAt] ^= _xorPattern.at(i%_xorPattern.size());
		}
	}
    ui->hexView->setData(currentData);

	if (currentData.size())
		ui->dataSizeLabel->setText(QString::number(currentData.size()));
	else
        ui->dataSizeLabel->setText("-");

    ui->histogramView->setData(currentData);

    // update entropy window
    this->initEntropy();
    for(int i = 0; i < currentData.size(); i++) {
        unsigned char c = currentData[i];
        this->updateEntropy(&c, 1);
    }
    this->finalizeEntropy();

    QString entropyStr = QString("Entropy = %1 bits/byte\n").arg(this->r_ent);
    entropyStr += QString("mean = %1\n").arg(this->r_mean);
    entropyStr += QString("monte carlo pi = %1, error %2\%\n").arg(this->r_montepicalc).arg(QString::number(100.0 * (fabs(PI - montepi) / PI),'f',2));
    entropyStr += QString("serial correlation = %1\n").arg(this->r_scc);
    entropyStr += QString("compresses by %1\%\n").arg(QString::number(100 * (8.0 - ent) / 8.0,'f',2));
    entropyStr += QString("chisq of %1; n=%2\n").arg(this->r_chisq).arg(this->totalc);
    if(this->r_chip < 0.01) {
        entropyStr += QString(" <0.01\% chance of TRNG\n");
    } else if( this->r_chip > 99.99 ) {
        entropyStr += QString(" >99.99\% chance of TRNG\n");
    } else {
        entropyStr += QString(" %3\% chance of TRNG\n").arg(this->r_chip);
    }
    entropyStr += QString("\nLast alignment: %1 inserts\n").arg(lastAlignAt);
    ui->extendedEntropy->clear();
    ui->extendedEntropy->appendPlainText(entropyStr);
   
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

void NandSeeWindow::initEntropy()
{
    /* Initialise for calculations */
    int i;

    ent = 0.0;		       /* Clear entropy accumulator */
    chisq = 0.0;	       /* Clear Chi-Square */
    datasum = 0.0;	       /* Clear sum of bytes for arithmetic mean */

    mp = 0;		       /* Reset Monte Carlo accumulator pointer */
    mcount = 0; 	       /* Clear Monte Carlo tries */
    inmont = 0; 	       /* Clear Monte Carlo inside count */
    incirc = 65535.0 * 65535.0;/* In-circle distance for Monte Carlo */

    sccfirst = 1;	       /* Mark first time for serial correlation */
    scct1 = scct2 = scct3 = 0.0; /* Clear serial correlation terms */

    incirc = pow(pow(256.0, (double) (MONTEN / 2)) - 1, 2.0);

    for (i = 0; i < 256; i++) {
        ccount[i] = 0;
    }
    totalc = 0;
}

void NandSeeWindow::updateEntropy(unsigned char *buf, int bufl)
{
    unsigned char bp = 0;
    unsigned char oc, c;

    while (bufl-- > 0) {
        oc = (buf[bp++] & 0xFF);
        c =   oc;
        ccount[c]++;		  /* Update counter for this bin */
        totalc++;

        /* Update inside / outside circle counts for Monte Carlo
         computation of PI */

        monte[mp++] = (double) oc;       /* Save character for Monte Carlo */
        if (mp >= MONTEN) {     /* Calculate every MONTEN character */
            int mj;

            mp = 0;
            mcount++;
            montex = montey = 0;
            for (mj = 0; mj < MONTEN / 2; mj++) {
                montex = (montex * 256.0) + monte[mj];
                montey = (montey * 256.0) + monte[(MONTEN / 2) + mj];
            }
            if ((montex * montex + montey *  montey) <= incirc) {
                inmont++;
            }
        }

        /* Update calculation of serial correlation coefficient */

        sccun = (double) c;
        if (sccfirst) {
            sccfirst = 0;
            scclast = 0;
            sccu0 = sccun;
        } else {
            scct1 = scct1 + scclast * sccun;
        }
        scct2 = scct2 + sccun;
        scct3 = scct3 + (sccun * sccun);
        scclast = sccun;
        oc <<= 1;
    }
}

void NandSeeWindow::finalizeEntropy()
{
    int i;

    /* Complete calculation of serial correlation coefficient */

    scct1 = scct1 + scclast * sccu0;
    scct2 = scct2 * scct2;
    scc = totalc * scct3 - scct2;
    if (scc == 0.0) {
       scc = -100000;
    } else {
       scc = (totalc * scct1 - scct2) / scc;
    }

    /* Scan bins and calculate probability for each bin and
       Chi-Square distribution.  The probability will be reused
       in the entropy calculation below.  While we're at it,
       we sum of all the data which will be used to compute the
       mean. */

    cexp = totalc / 256.0;  /* Expected count per bin */
    for (i = 0; i < 256; i++) {
       double a = ccount[i] - cexp;;

       prob[i] = ((double) ccount[i]) / totalc;
       chisq += (a * a) / cexp;
       datasum += ((double) i) * ccount[i];
    }

    /* Calculate entropy */

    for (i = 0; i < 256; i++) {
       if (prob[i] > 0.0) {
            ent += prob[i] * log2of10 * log10(1 / prob[i]);
       }
    }

    /* Calculate Monte Carlo value for PI from percentage of hits
       within the circle */

    montepi = 4.0 * (((double) inmont) / mcount);

    /* Return results through arguments */

    r_ent = ent;
    r_chisq = chisq;
    r_mean = datasum / totalc;
    r_montepicalc = montepi;
    r_scc = scc;

    r_chip = pochisq(chisq, 255) * 100;

}


/*

    Compute probability of measured Chi Square value.

    This code was developed by Gary Perlman of the Wang
    Institute (full citation below) and has been minimally
    modified for use in this program.

*/


/*HEADER
    Module:       z.c
    Purpose:      compute approximations to normal z distribution probabilities
    Programmer:   Gary Perlman
    Organization: Wang Institute, Tyngsboro, MA 01879
    Copyright:    none
    Tabstops:     4
*/

/*FUNCTION poz: probability of normal z value */
/*ALGORITHM
    Adapted from a polynomial approximation in:
        Ibbetson D, Algorithm 209
        Collected Algorithms of the CACM 1963 p. 616
    Note:
        This routine has six digit accuracy, so it is only useful for absolute
        z values < 6.  For z values >= to 6.0, poz() returns 0.0.
*/
        /*VAR returns cumulative probability from -oo to z */
double NandSeeWindow::poz(double z)  /*VAR normal z value */
{
    double y, x, w;

    if (z == 0.0) {
        x = 0.0;
    } else {
    y = 0.5 * fabs(z);
    if (y >= (Z_MAX * 0.5)) {
            x = 1.0;
    } else if (y < 1.0) {
       w = y * y;
       x = ((((((((0.000124818987 * w
           -0.001075204047) * w +0.005198775019) * w
           -0.019198292004) * w +0.059054035642) * w
           -0.151968751364) * w +0.319152932694) * w
           -0.531923007300) * w +0.797884560593) * y * 2.0;
    } else {
        y -= 2.0;
        x = (((((((((((((-0.000045255659 * y
            +0.000152529290) * y -0.000019538132) * y
            -0.000676904986) * y +0.001390604284) * y
            -0.000794620820) * y -0.002034254874) * y
            +0.006549791214) * y -0.010557625006) * y
            +0.011630447319) * y -0.009279453341) * y
            +0.005353579108) * y -0.002141268741) * y
            +0.000535310849) * y +0.999936657524;
        }
    }
    return (z > 0.0 ? ((x + 1.0) * 0.5) : ((1.0 - x) * 0.5));
}

/*
    Module:       chisq.c
    Purpose:      compute approximations to chisquare distribution probabilities
    Contents:     pochisq()
    Uses:         poz() in z.c (Algorithm 209)
    Programmer:   Gary Perlman
    Organization: Wang Institute, Tyngsboro, MA 01879
    Copyright:    none
    Tabstops:     4
*/

/*FUNCTION pochisq: probability of chi sqaure value */
/*ALGORITHM Compute probability of chi square value.
    Adapted from:
        Hill, I. D. and Pike, M. C.  Algorithm 299
        Collected Algorithms for the CACM 1967 p. 243
    Updated for rounding errors based on remark in
        ACM TOMS June 1985, page 185
*/

double NandSeeWindow::pochisq(
        double ax,    /* obtained chi-square value */
        int df	    /* degrees of freedom */
        )
{
    double x = ax;
    double a, y, s;
    double e, c, z;
    int even;	    	    /* true if df is an even number */

    if (x <= 0.0 || df < 1) {
        return 1.0;
    }

    a = 0.5 * x;
//    even = (2 * (df / 2)) == df;
    even = !(df & 0x1);
    if (df > 1) {
        y = ex(-a);
    }
    s = (even ? y : (2.0 * poz(-sqrt(x))));
    if (df > 2) {
    x = 0.5 * (df - 1.0);
    z = (even ? 1.0 : 0.5);
    if (a > BIGX) {
            e = (even ? 0.0 : LOG_SQRT_PI);
            c = log(a);
            while (z <= x) {
        e = log(z) + e;
        s += ex(c * z - a - e);
        z += 1.0;
            }
            return (s);
        } else {
        e = (even ? 1.0 : (I_SQRT_PI / sqrt(a)));
        c = 0.0;
        while (z <= x) {
            e = e * (a / z);
            c = c + e;
            z += 1.0;
            }
        return (c * y + s);
        }
    } else {
        return s;
    }
}

void NandSeeWindow::optimizeXor()
{
    double maxchisq = this->r_chisq;
    lastAlignAt = 0;
    int bestAlign = 0;
    QString hexChars = "00";
    for(int i=0;i<256;i++) {
        lastAlignAt = i;
        updateHexView(); // call this to update stats
        if( this->r_chisq > maxchisq ) {
            maxchisq = this->r_chisq;
            bestAlign = i;
        }
//        this->update();
        qApp->processEvents(QEventLoop::ExcludeSocketNotifiers);
    }
    lastAlignAt = bestAlign;
    updateHexView();
}

void NandSeeWindow::xorPatternChanged(const QString &text)
{
	QString tempString = text;

	tempString.remove("0x");
	tempString.remove(" ");
	tempString.remove(":");
	tempString.remove("-");
    tempString.remove("\n");
    tempString.remove("\r");
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
