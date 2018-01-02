#ifndef MAINCLASS_H
#define MAINCLASS_H

#include <QObject>
#include <QSqlDatabase>
#include <QDomDocument>
#include <QDomElement>
#include <QProcess>
#include <QJsonObject>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>


struct tableDef
{
    QString name;
    QString parent;
};
typedef tableDef TtableDef;

struct fieldDef
{
    QString name;
    QString value;
};
typedef fieldDef TfieldDef;

class mainClass : public QObject
{
    Q_OBJECT
public:
    explicit mainClass(QObject *parent = nullptr);
    void setParameters(QString host, QString port, QString user, QString pass, QString schema, QString table, QString mapDir, QString output, bool includeProtected);
    int returnCode;
signals:
    void finished();
public slots:
    void run();
private:
    void log(QString message);
    int generateJSONs(QSqlDatabase db);
    void processChilds(QDomDocument doc, QDomElement &parent, QString table, QList <TtableDef> tables, QStringList &tablesUsed);
    //QList<TfieldDef> getDataByRowUUID(QString tableToSearch, QString UUIDToSearch);
    void processMapFile(QSqlDatabase db, QString fileName);
    void parseMapFile(QSqlDatabase db, QDomNode node, QJsonObject &json, QString currentTable, QJsonObject &parent);
    void parseDataToMongo(mongocxx::collection collection, QString table, QString fileName);
    //QList<TfieldDef> getDataByRowUUID2(mongocxx::collection collection, QString tableToSearch, QString UUIDToSearch);
    QList<TfieldDef> getDataByRowUUID(QSqlDatabase db, QString tableToSearch, QString UUIDToSearch);
    QString host;
    QString port;
    QString user;
    QString pass;
    QString schema;
    QString table;
    QString mapDir;
    QString output;
    QString nullValue;
    bool includeProtected;
};

#endif // MAINCLASS_H
