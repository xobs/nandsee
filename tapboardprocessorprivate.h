#ifndef TAPBOARDPROCESSORPRIVATE_H
#define TAPBOARDPROCESSORPRIVATE_H

#include <QObject>
#include <QTemporaryFile>

class TapboardProcessorPrivate : public QObject
{
    Q_OBJECT

public:
    explicit TapboardProcessorPrivate(QObject *parent = 0);
    ~TapboardProcessorPrivate();
    void setSourceFilename(QString &newSource);
    void setTargetFilename(QString &newTarget);

private:
    QTemporaryFile *joinedFile;
    QTemporaryFile *groupedFile;
    QFile *rawFile;
    QFile *sortedFile;
    QString sourceFilename;
    QString targetFilename;

public slots:
    int joinFile();
    int groupFile();
    int sortFile();

signals:
    void joinFinished();
    void groupFinished();
    void sortFinished();
};


#endif // TAPBOARDPROCESSORPRIVATE_H
