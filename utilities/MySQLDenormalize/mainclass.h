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

struct UUIDDef
{
    QString table;
    QString UUID;
    QList<TfieldDef> fields;
};
typedef UUIDDef TUUIDDef;

class mainClass : public QObject
{
    Q_OBJECT
public:
    explicit mainClass(QObject *parent = nullptr);
    void setParameters(QString host, QString port, QString user, QString pass, QString schema, QString table, QString mapDir, QString output, bool includeProtected, QString tempDir);
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
    void processMapFile(mongocxx::collection collection, QString fileName);
    void parseMapFile(QList<TUUIDDef> dataList, QDomNode node, QJsonObject &json, QString currentTable, QJsonObject &parent);
    void parseDataToMongo(mongocxx::collection collection, QString table, QString fileName);
    QList<TfieldDef> getDataByRowUUID2(mongocxx::collection collection, QString tableToSearch, QString UUIDToSearch);
    //QList<TfieldDef> getDataByRowUUID3(QSqlDatabase db, QString tableToSearch, QString UUIDToSearch);
    QList<TfieldDef> getDataByRowUUID4(QList<TUUIDDef> dataList, QString tableToSearch, QString UUIDToSearch);
    void getAllUUIDs(QDomNode node, QStringList &UUIDs);
    QString host;
    QString port;
    QString user;
    QString pass;
    QString schema;
    QString table;
    QString mapDir;
    QString output;
    QString nullValue;
    QString tempDir;
    bool includeProtected;
};

#endif // MAINCLASS_H
