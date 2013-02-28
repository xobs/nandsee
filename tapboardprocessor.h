#ifndef TAPBOARDPROCESSOR_H
#define TAPBOARDPROCESSOR_H

#include <QObject>

class TapboardProcessor : public QObject
{
    Q_OBJECT
public:
    explicit TapboardProcessor(QObject *parent = 0);
    int processRawFile(QString &rawFileName);
    
signals:
    
public slots:
    void joinFile();
    void groupFile();
    void sortFile();
};

#endif // TAPBOARDPROCESSOR_H
