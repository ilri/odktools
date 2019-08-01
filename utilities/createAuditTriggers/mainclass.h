#ifndef MAINCLASS_H
#define MAINCLASS_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>

class mainClass : public QObject
{
    Q_OBJECT
public:
    explicit mainClass(QObject *parent = nullptr);
    void setParameters(QString host, QString port, QString user, QString pass, QString schema, QString outputDirectory);
    int returnCode;
signals:
    void finished();
public slots:
    void run();
private:
    QString host;
    QString port;
    QString user;
    QString pass;
    QString schema;
    QString outputDirectory;
    void log(QString message);
    int createAudit(QSqlDatabase mydb, QString auditDir, QStringList ignoreTables);

public slots:
};

#endif // MAINCLASS_H
