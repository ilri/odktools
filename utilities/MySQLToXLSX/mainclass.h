#ifndef MAINCLASS_H
#define MAINCLASS_H

#include <QObject>
#include <QDomNode>

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
};
typedef tableDef TtableDef;

struct linkedTable
{
    QString field;
    QString related_table;
    QString related_field;
};
typedef linkedTable TlinkedTable;

class mainClass : public QObject
{
    Q_OBJECT
public:
    explicit mainClass(QObject *parent = nullptr);
    void setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, QString outputFile, bool protectSensitive, QString tempDir, bool incLookups, bool incmsels, QString firstSheetName, QString encryption_key, QString resolve_type);
    int returnCode;
signals:
    void finished();
public slots:
    void run();
private:
    void log(QString message);
    int generateXLSX();        
    QString getSheetDescription(QString name);
    void loadTable(QDomNode node);
    void getMultiSelectInfo(QDomNode table, QString table_name, QString &multiSelect_field, QStringList &keys, QString &rel_table, QString &rel_field);
    QString host;
    QString port;
    QString user;
    QString pass;
    QString schema;
    QString outputFile;
    QString tempDir;
    QString createXML;
    QString encryption_key;
    int resolve_type = 1;
    bool protectSensitive;
    QList<TtableDef> tables;
    QList<TtableDef> mainTables;
    QStringList tableNames;
    int letterIndex;
    bool incLookups;
    bool incmsels;
    QString firstSheetName;
    QStringList protectedKeys;
};

#endif // MAINCLASS_H
