#ifndef JSONWORKER_H
#define JSONWORKER_H

#include <QThread>
#include <QDir>
#include "listmutex.h"
#include "mainclass.h"

//struct taskItem
//{
//    QString table;
//    QString create_sql;
//    QString alter_sql;
//    QString query_sql;
//    QString json_file;
//};
//typedef taskItem TtaskItem;

class JSONWorker : public QThread
{
    Q_OBJECT
public:
    explicit JSONWorker(QObject *parent = nullptr);
    void run();
    void setTasks(QList< TtaskItem> task_list);
    void setMutex(ListMutex *mutex);
    void setName(QString name);
    void setParameters(QString host, QString port, QString user, QString pass, QString schema, QDir currDir, QString outputDirectory);
    int status;
private:
    void log(QString message);
    QList< TtaskItem> task_list;
    ListMutex *mutex;
    QString host;
    QString port;
    QString pass;
    QString user;
    QString schema;
    QDir currDir;
    QString outputDirectory;
    QString name;
};

#endif // JSONWORKER_H
