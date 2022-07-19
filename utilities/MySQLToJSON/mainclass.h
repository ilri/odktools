#ifndef MAINCLASS_H
#define MAINCLASS_H

#include <QObject>
#include <QDomNode>
#include <QDir>
#include <QSqlDatabase>

struct fieldDef
{
  QString name; //Field Name
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
  QList<TfieldDef> fields; //List of fields
  bool islookup; //Whether the table is a lookup table
  bool ismultiselect; //Whether the table is a lookup table
};
typedef tableDef TtableDef;

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

struct taskItem
{
    QString table;
    int task_type;
    QString sql_file;
    QString json_file;
    QStringList json_files;
    QString final_file;
};
typedef taskItem TtaskItem;

class mainClass : public QObject
{
    Q_OBJECT
public:
    explicit mainClass(QObject *parent = nullptr);
    void setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, QString outputDir, bool protectSensitive, QString tempDir, bool incLookups, bool incmsels, QString encryption_key, QString resolve_type, int num_workers);
    int returnCode;
signals:
    void finished();
public slots:
    void run();
private:
    void log(QString message);
    int generateXLSX();
    int processTasks(QDir currDir);
    QString getSheetDescription(QString name);
    void loadTable(QDomNode node);
    void getMultiSelectInfo(QDomNode table, QString table_name, QString &multiSelect_field, QStringList &keys, QString &rel_table, QString &rel_field);
    QStringList get_parts(int total, int parts);
    QString host;
    QString port;
    QString user;
    QString pass;
    QString schema;
    QString outputDirectory;
    QString tempDir;
    QString createXML;
    QString encryption_key;
    int resolve_type = 1;
    bool protectSensitive;
    QList<TtableDef> tables;
    QList<TtableDef> mainTables;
    QList<TtableDef> lookupTables;
    QStringList tableNames;
    int num_workers;
    int letterIndex;
    bool incLookups;
    bool incmsels;    
    QStringList protectedKeys;    
    QList< TtaskItem> separate_task_list;
    QList< TtaskItem> update_task_list;
    QList< TtaskItem> json_task_list;
    QList< TtaskItem> merge_task_list;
    QSqlDatabase db;
};

#endif // MAINCLASS_H
