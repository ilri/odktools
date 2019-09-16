/*
MySQLDenormalize

Copyright (C) 2018 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

MySQLDenormalize is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

MySQLDenormalize is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with MySQLDenormalize.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

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
#include <boost/property_tree/ptree.hpp>

namespace pt = boost::property_tree;

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
    void setParameters(QString host, QString port, QString user, QString pass, QString schema, QString table, QString mapDir, QString output, bool separate, QString tempDir, QString createFileName);
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
    //void parseMapFile(QList<TUUIDDef> dataList, QDomNode node, QJsonObject &json, QString currentTable, QJsonObject &parent);
    void parseMapFileWithBoost(QList <TUUIDDef> dataList, QDomNode node, pt::ptree &json, QString currentTable, pt::ptree &parent);
    void parseDataToMongo(mongocxx::collection collection, QString table, QString fileName);
    QList<TfieldDef> getDataByRowUUID2(mongocxx::collection collection, QString tableToSearch, QString UUIDToSearch);
    //QList<TfieldDef> getDataByRowUUID3(QSqlDatabase db, QString tableToSearch, QString UUIDToSearch);
    QList<TfieldDef> getDataByRowUUID4(QList<TUUIDDef> dataList, QString tableToSearch, QString UUIDToSearch, QDomNode current_node);
    void getAllUUIDs(QDomNode node, QStringList &UUIDs);
    void remove_msels(QDomNode node);
    bool isFieldMultiSelect(QString table, QString field, QString &msel_table, QString &lookup_field);
    QString getMultiSelectValuesAsString(QList<TUUIDDef> dataList, QString msel_table, QString lookup_field, QDomNode current_node);
    QStringList getMultiSelectValuesAsArray(QList<TUUIDDef> dataList, QString msel_table, QString lookup_field, QDomNode current_node, QString field_name);

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
    QString createFile;
    bool separateSelects;
    QDomNodeList tables_in_create;
    QStringList alreadyPulled;
    QStringList alreadyPulled2;
};

#endif // MAINCLASS_H
