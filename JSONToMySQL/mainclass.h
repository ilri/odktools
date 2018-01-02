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
    void setParameters(bool voverwrite, QString vjson, QString vmanifest, QString vhost, QString vport, QString vuser, QString vpassword, QString vschema, QString voutput, QString vinput, QString vjavaScript, bool voputSQLSwitch, QString mapDirectory);
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

    QFile logFile;
    QTextStream logStream;

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
};

#endif // MAINCLASS_H
