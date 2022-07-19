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
    //qDebug() << name + " - Processing index: " + QString::number(index);
    while (index >= 0)
    {
        //qDebug() << name + " - Processing index: " + QString::number(index);
        // Create the table
        if (task_list[index].task_type == 1)
        {
            //qDebug() << name + " - Processing file: " + task_list[index].sql_file;
            arguments.clear();
            arguments.append("--host=" + this->host);
            arguments.append("--port=" + this->port);
            arguments.append("--password=" + this->pass);
            arguments.append("--user=" + this->user);
            arguments.append("--database=" + this->schema);

            mySQLDumpProcess->setStandardInputFile(task_list[index].sql_file);
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
        }
        if (task_list[index].task_type == 2)
        {
            // Extract the data as json
            //qDebug() << name + " - Exporting file: " + task_list[index].sql_file;
            arguments.clear();
            arguments << "--sql";
            arguments << "--result-format=json/array";
            arguments << "--uri=" + uri;
            mySQLDumpProcess->setStandardInputFile(task_list[index].sql_file);
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
            else
            {
                this->msleep(500);
            }
        }
        if (task_list[index].task_type == 3)
        {
            //qDebug() << name + " - Creating final file: " + task_list[index].final_file;
            arguments.clear();
            arguments << "-s";
            arguments << "reduce .[] as $x ([]; . + $x)";
            for (int f=0; f < task_list[index].json_files.count(); f++)
            {
                arguments << task_list[index].json_files[f];
            }
            mySQLDumpProcess->setStandardInputFile(QProcess::nullDevice());
            mySQLDumpProcess->setStandardOutputFile(task_list[index].final_file);

            mySQLDumpProcess->start("jq", arguments);
            mySQLDumpProcess->waitForFinished(-1);
            if ((mySQLDumpProcess->exitCode() > 0) || (mySQLDumpProcess->error() == QProcess::FailedToStart))
            {
                if (mySQLDumpProcess->error() == QProcess::FailedToStart)
                {
                    log("Error: Command jq not found");
                }
                else
                {
                    log("Running jq returned error");
                    QString serror = mySQLDumpProcess->readAllStandardError();
                    log(serror);
                    log("Running paremeters:" + arguments.join(" "));
                }
                delete mySQLDumpProcess;
                this->status = 1;
                return;
            }

            QFileInfo info(task_list[index].final_file);
            if (info.size() > 0)
            {
                QString final_csv_file = task_list[index].final_file;
                final_csv_file = final_csv_file.replace(".json",".csv");
                arguments.clear();
                arguments << "-i";
                arguments << task_list[index].final_file;
                arguments << "-o";
                arguments << final_csv_file;

                mySQLDumpProcess->setStandardInputFile(QProcess::nullDevice());
                mySQLDumpProcess->setStandardOutputFile(QProcess::nullDevice());
                mySQLDumpProcess->start("json2csv", arguments);
                mySQLDumpProcess->waitForFinished(-1);
                if ((mySQLDumpProcess->exitCode() > 0) || (mySQLDumpProcess->error() == QProcess::FailedToStart))
                {
                    if (mySQLDumpProcess->error() == QProcess::FailedToStart)
                    {
                        log("Error: Command jq not found");
                    }
                    else
                    {
                        log("Running jq returned error");
                        QString serror = mySQLDumpProcess->readAllStandardError();
                        log(serror);
                        log("Running paremeters:" + arguments.join(" "));
                    }
                    delete mySQLDumpProcess;
                    this->status = 1;
                    return;
                }
            }
            QFile::remove(task_list[index].final_file);            
        }
        if (task_list[index].task_type == 4)
        {
            QFileInfo info(task_list[index].json_files[0]);
            if (info.size() > 0)
            {
                QString final_csv_file = task_list[index].final_file;
                final_csv_file = final_csv_file.replace(".json",".csv");
                arguments.clear();
                arguments << "-i";
                arguments << task_list[index].json_files[0];
                arguments << "-o";
                arguments << final_csv_file;

                mySQLDumpProcess->setStandardInputFile(QProcess::nullDevice());
                mySQLDumpProcess->setStandardOutputFile(QProcess::nullDevice());
                mySQLDumpProcess->start("json2csv", arguments);
                mySQLDumpProcess->waitForFinished(-1);
                if ((mySQLDumpProcess->exitCode() > 0) || (mySQLDumpProcess->error() == QProcess::FailedToStart))
                {
                    if (mySQLDumpProcess->error() == QProcess::FailedToStart)
                    {
                        log("Error: Command json2csv not found");
                    }
                    else
                    {
                        log("Running json2csv returned error");
                        QString serror = mySQLDumpProcess->readAllStandardError();
                        log(serror);
                        log("Running paremeters:" + arguments.join(" "));
                    }
                    delete mySQLDumpProcess;
                    this->status = 1;
                    return;
                }
            }
            QFile::remove(task_list[index].final_file);
        }
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
