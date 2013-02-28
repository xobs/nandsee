#ifndef TAPBOARDPROCESSOR_H
#define TAPBOARDPROCESSOR_H

#include <QObject>
#include <QProgressDialog>

class TapboardProcessor : public QObject
{
    Q_OBJECT
public:
    explicit TapboardProcessor(QObject *parent = 0);
    int processRawFile(QString &rawFileName);
    
private:
    QThread *backgroundThread;

    QProgressDialog *progressWindow;

signals:
    
public slots:
    void gotJoinFinished();
    void gotGroupFinished();
    void gotSortFinished();
};

#endif // TAPBOARDPROCESSOR_H
