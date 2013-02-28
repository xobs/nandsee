#include <QFileDialog>
#include <QDebug>
#include <QTemporaryFile>
#include <QProgressDialog>
#include <QThread>
#include <QApplication>
#include "tapboardprocessor.h"
#include "tapboardprocessorprivate.h"


TapboardProcessor::TapboardProcessor(QObject *parent) :
    QObject(parent)
{
}

int TapboardProcessor::processRawFile(QString &rawFileName)
{
    QFileDialog selectFile;

    selectFile.setFileMode(QFileDialog::AnyFile);
    selectFile.setAcceptMode(QFileDialog::AcceptSave);

    QString suggestedName = rawFileName;
    selectFile.setNameFilter("Tap Board event file (*.tbevent)");
    suggestedName.replace(".tbraw", ".tbevent");
    selectFile.selectFile(suggestedName);

    if (!selectFile.exec()) {
        qDebug() << "No file selected";
        return -1;
    }

    QString fileName;
    fileName = selectFile.selectedFiles()[0];
    if (!fileName.endsWith(".tbevent"))
        fileName += ".tbevent";

    progressWindow = new QProgressDialog(QString::fromUtf8("Joining events…"), "Cancel", 0, 4);
    progressWindow->setMinimumDuration(0);
    progressWindow->setValue(1);
    progressWindow->setWindowTitle("Importing trace file");

    TapboardProcessorPrivate *tpp = new TapboardProcessorPrivate;
    backgroundThread = new QThread(this);

    connect(backgroundThread, SIGNAL(started()),
            tpp, SLOT(joinFile()));
    tpp->moveToThread(backgroundThread);

    connect(tpp, SIGNAL(joinFinished()),
            this, SLOT(gotJoinFinished()));
    connect(tpp, SIGNAL(groupFinished()),
            this, SLOT(gotGroupFinished()));
    connect(tpp, SIGNAL(sortFinished()),
            this, SLOT(gotSortFinished()));

    // Still don't know why this is required, I thought it quit on its own
    connect(tpp, SIGNAL(sortFinished()),
            backgroundThread, SLOT(quit()));

    tpp->setSourceFilename(rawFileName);
    tpp->setTargetFilename(fileName);

    backgroundThread->start();
    progressWindow->setLabelText(QString::fromUtf8("Joining events…"));
    progressWindow->setValue(0);
    while (backgroundThread->isRunning())
        qApp->processEvents();

    delete backgroundThread;
    delete progressWindow;

    rawFileName = fileName;
    qDebug() << "Returning...";
    return 0;
}

void TapboardProcessor::gotJoinFinished()
{
    progressWindow->setLabelText("Grouping events...");
    progressWindow->setValue(2);
}

void TapboardProcessor::gotGroupFinished()
{
    progressWindow->setLabelText("Sorting events...");
    progressWindow->setValue(3);
}

void TapboardProcessor::gotSortFinished()
{
    progressWindow->setValue(4);
}
