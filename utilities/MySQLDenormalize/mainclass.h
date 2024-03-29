#ifndef MAINCLASS_H
#define MAINCLASS_H

#include <QObject>
#include <QDomNode>
#ifndef Q_MOC_RUN
#include <mongocxx/collection.hpp>
#include <boost/property_tree/ptree.hpp>
#endif

namespace pt = boost::property_tree;

struct fieldDef
{
  QString name; //Field Name
  QString ODKname; //Field name in ODK
  QString type; //Variable type in MySQL
  QString desc; //Variable description
  int size; //Variable size
  int decSize; //Variable decimal size
  bool isMultiSelect = false;
  bool isKey = false;
  bool isLookUp = false;
  QString multiSelectTable;
  QString multiSelectField;
  QStringList multiSelectKeys;
  QString multiSelectRelTable;
  QString multiSelectRelField;
  QString lookupRelTable;
  QString lookupRelField;
  QString replace_value;
  QString value;
  bool sensitive;
  QString protection;
};
typedef fieldDef TfieldDef;

struct tableDef
{
  QString name;
  QString desc;
  QString ODKname;
  QList<TfieldDef> fields; //List of fields
  bool islookup; //Whether the table is a lookup table
};
typedef tableDef TtableDef;

struct UUIDFieldDef
{
    QString name;
    QString value;
};
typedef UUIDFieldDef TUUIDFieldDef;

struct UUIDDef
{
    QString UUID;
    QList<TUUIDFieldDef> fields;
};
typedef UUIDDef TUUIDDef;

struct linkedTable
{
    QString field;
    QString related_table;
    QString related_field;
};
typedef linkedTable TlinkedTable;

struct multiSelectTable
{
    QString field;
    QString multiSelectTable;
    QString multiSelectField;
    QString multiSelectRelTable;
    QString multiSelectRelField;
    QStringList multiSelectKeys;
};
typedef multiSelectTable TmultiSelectTable;

class mainClass : public QObject
{
    Q_OBJECT
public:
    explicit mainClass(QObject *parent = nullptr);
    void setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, bool protectSensitive, QString tempDir, QString encryption_key, QString mapDir, QString outputDir, QString mainTable, QString resolve_type, QString primaryKey, QString primaryKeyValue, QString separator, bool useODKFormat);
    int returnCode;
signals:
    void finished();
public slots:
    void run();
private:
    void log(QString message);
    int generateXLSX();            
    void loadTable(QDomNode node);
    void getMultiSelectInfo(QDomNode table, QString table_name, QString &multiSelect_field, QStringList &keys, QString &rel_table, QString &rel_field);
    QString host;
    QString port;
    QString user;
    QString pass;
    QString schema;    
    QString tempDir;
    QString createXML;
    QString encryption_key;
    bool protectSensitive;
    QList<TtableDef> tables;
    QList<TtableDef> mainTables;
    QStringList tableNames;           
    QStringList protectedKeys;
    void processMapFile(QString fileName);
    QString mapDir;
    QString outputDir;
    QString mainTable;
    int resolve_type;
    QString primaryKey;
    QString primaryKeyValue;
    bool useODKFormat;
    QString separator;
    mongocxx::collection mongo_collection;
    void getAllUUIDs(QDomNode node,QStringList &UUIDs);
    void parseMapFileWithBoost(QVector <TUUIDDef> dataList, QDomNode node, pt::ptree &json, pt::ptree &parent);
    QList<TUUIDFieldDef> getDataByRowUUID4(QVector<TUUIDDef> dataList, QString UUIDToSearch);
    void processSection(QStringList section);
};

#endif // MAINCLASS_H
