#include "jsonworker.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>

JSONWorker::JSONWorker(QObject *parent)
    : QThread{parent}
{

}

void JSONWorker::setParameters(QString host, QString port, QString user, QString pass, QString schema, QDir currDir, QString outputDirectory)
{
    this->host = host;
    this->port = port;
    this->user = user;
    this->pass = pass;
    this->schema = schema;
    this->currDir = currDir;
    this->outputDirectory = outputDirectory;
}

void JSONWorker::setName(QString name)
{
    this->name = name;
}

void JSONWorker::log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s", temp.toUtf8().data());
}

void JSONWorker::run()
{
    QProcess *mySQLDumpProcess = new QProcess();
    QStringList arguments;
    QString uri = user + ":" + pass + "@" + host + "/" + schema;

    int index = mutex->get_index();
    while (index >= 0)
    {
        //qDebug() << name + " - Processing index: " + QString::number(index);
        // Create the table
        arguments.clear();
        arguments.append("--host=" + this->host);
        arguments.append("--port=" + this->port);
        arguments.append("--password=" + this->pass);
        arguments.append("--user=" + this->user);
        arguments.append("--database=" + this->schema);

        mySQLDumpProcess->setStandardInputFile(task_list[index].create_sql);
        mySQLDumpProcess->setStandardOutputFile(QProcess::nullDevice());
        mySQLDumpProcess->start("mysql", arguments);
        mySQLDumpProcess->waitForFinished(-1);
        if (mySQLDumpProcess->exitCode() > 0)
        {
            QString serror = mySQLDumpProcess->readAllStandardError();
            log(serror);
            delete mySQLDumpProcess;
            this->status = 1;
            return;
        }

        // Apply the updates
        arguments.clear();
        arguments.append("--host=" + this->host);
        arguments.append("--port=" + this->port);
        arguments.append("--password=" + this->pass);
        arguments.append("--user=" + this->user);
        arguments.append("--database=" + this->schema);

        mySQLDumpProcess->setStandardInputFile(task_list[index].alter_sql);
        mySQLDumpProcess->setStandardOutputFile(QProcess::nullDevice());
        mySQLDumpProcess->start("mysql", arguments);
        mySQLDumpProcess->waitForFinished(-1);
        if (mySQLDumpProcess->exitCode() > 0)
        {
            QString serror = mySQLDumpProcess->readAllStandardError();
            log(serror);
            delete mySQLDumpProcess;
            this->status = 1;
            return;
        }

        // Extract the data as json
        arguments.clear();
        arguments << "--sql";
        arguments << "--result-format=json/array";
        arguments << "--uri=" + uri;
        mySQLDumpProcess->setStandardInputFile(task_list[index].query_sql);
        mySQLDumpProcess->setStandardOutputFile(task_list[index].json_file);
        mySQLDumpProcess->start("mysqlsh", arguments);
        mySQLDumpProcess->waitForFinished(-1);
        if ((mySQLDumpProcess->exitCode() > 0) || (mySQLDumpProcess->error() == QProcess::FailedToStart))
        {
            if (mySQLDumpProcess->error() == QProcess::FailedToStart)
            {
                log("Error: Command mysqlsh not found");
            }
            else
            {
                log("Running mysqlsh returned error");
                QString serror = mySQLDumpProcess->readAllStandardError();
                log(serror);
                log("Running paremeters:" + arguments.join(" "));
            }
            delete mySQLDumpProcess;
            this->status = 1;
            return;
        }
        // Removes the temporary table;
        arguments.clear();
        arguments.append("--host=" + this->host);
        arguments.append("--port=" + this->port);
        arguments.append("--password=" + this->pass);
        arguments.append("--user=" + this->user);
        arguments.append("--database=" + this->schema);
        arguments.append("--execute=DROP TABLE " + task_list[index].table );

        mySQLDumpProcess->setStandardInputFile(QProcess::nullDevice());
        mySQLDumpProcess->setStandardOutputFile(QProcess::nullDevice());
        mySQLDumpProcess->start("mysql", arguments);
        mySQLDumpProcess->waitForFinished(-1);
        if (mySQLDumpProcess->exitCode() > 0)
        {
            QString serror = mySQLDumpProcess->readAllStandardError();
            log(serror);
            delete mySQLDumpProcess;
            this->status = 1;
            return;
        }
        QString newpath = task_list[index].json_file;
        newpath.replace(currDir.absolutePath() + currDir.separator(), outputDirectory + currDir.separator());
        if (QFile::copy(task_list[index].json_file, newpath))
            QFile::remove(task_list[index].json_file);

        index = mutex->get_index();
    }

    delete mySQLDumpProcess;
    this->status = 0;
}

void JSONWorker::setTasks(QList< TtaskItem> task_list)
{
    this->task_list = task_list;
}

void JSONWorker::setMutex(ListMutex *mutex)
{
    this->mutex = mutex;
}
