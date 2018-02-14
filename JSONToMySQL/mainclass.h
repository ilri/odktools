/*
JSONToMySQL.

Copyright (C) 2015-2017 International Livestock Research Institute.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

JSONToMySQL is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

JSONToMySQL is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with JSONToMySQL.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#ifndef MAINCLASS_H
#define MAINCLASS_H

#include <QObject>
#include <qjson-qt5/parser.h>
#include <QSqlDatabase>
#include <QSqlError>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QFileInfo>
#include <QVariantMap>
#include <QVariantList>
#include <QVariant>
#include <QtXml>
#include <QList>
#include <QSqlQuery>
#include <QJSEngine>
#include <QJSValue>
#include <QJSValueList>
#include "insertvalues.h"
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>

struct tblIndexDef
{
    QString table;
    int index;
};
typedef tblIndexDef TtblIndexDef;

struct fieldDef
{
  QString name;
  QString xmlCode;
  QString value;
  bool key;
  bool multiSelect;
  QString multiSelectTable;
  QString uuid;
  QString type;
  QString size;
  QString decSize;
  QString ODKType;
};
typedef fieldDef TfieldDef;

struct tableKey
{
    int index;
    QList< TfieldDef> keys;
};
typedef tableKey TtableKey;

class mainClass : public QObject
{
    Q_OBJECT
public:
    explicit mainClass(QObject *parent = 0);
    int returnCode;
signals:
    void finished();
    void finishedWithError(int error);
public slots:
    void run();
    void setParameters(bool voverwrite, QString vjson, QString vmanifest, QString vhost, QString vport, QString vuser, QString vpassword, QString vschema, QString voutput, QString vinput, QString vjavaScript, bool voputSQLSwitch, QString mapDirectory, QString outputType);
private:

    int getLastIndex(QString table);
    void log(QString message);
    QString getXMLCodeFromField(QList< TfieldDef> fields, QString field);
    void logError(QSqlDatabase db,QString errorMessage, QString table, int rowNumber,QVariantMap jsonData,QList< TfieldDef> fields, QString execSQL);
    void logErrorMSel(QSqlDatabase db,QString errorMessage, QString table, int rowNumber,QString value, QString execSQL);
    QString fixString(QString source);
    QList<TfieldDef > createSQL(QSqlDatabase db, QVariantMap jsonData, QString table, QList< TfieldDef> fields, QList< TfieldDef> parentkeys, QVariantMap jsonData2, bool mTable);
    void debugKeys(QString table, QList< TfieldDef> keys);
    void debugMap(QVariantMap jsonData);
    int procTable(QSqlDatabase db, QVariantMap jsonData, QDomNode table, QList< TfieldDef> parentkeys);
    int processFile(QSqlDatabase db, QString json, QString manifest, QStringList procList);
    void storeRecord(QStringList parentUUIDS, QString recordUUID);
    void storeRecord(QString parentUUID, QString recordUUID);
    void findElementsWithAttribute(const QDomElement& elem, const QString& attr, const QString& attvalue, QList<QDomElement> &foundElements);

    QJSValue beforeInsertFunction;
    bool callBeforeInsert;
    QList <TtblIndexDef > lstTblIndex;

    bool SQLError;
    QString SQLErrorNumber;
    QString fileID;
    QString mainTable;

    //Huma logging
    QFile logFile;
    QTextStream logStream;
    //Machine loggin
    QDomDocument xmlLog;
    QDomElement eErrors;

    bool outSQL;
    QFile sqlFile;
    QTextStream sqlStream;
    QDomDocument recordMap;
    QDomElement recordMapRoot;

    QJSEngine JSEngine;

    //Run parameters
    bool overwrite;
    QString json;
    QString manifest;
    QString host;
    QString port;
    QString user;
    QString password;
    QString schema;
    QString output;
    QString input;
    QString javaScript;
    QString mapOutputDir;
    QString outputType;
};

#endif // MAINCLASS_H
