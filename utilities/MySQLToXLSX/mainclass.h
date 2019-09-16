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
  QString multiSelectTable;
  QString multiSelectField;
  QStringList multiSelectOptions;
  QStringList multiSelectKeys;
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

struct optionDef
{
    QString code;
    QString value;
};
typedef optionDef ToptionDef;

class mainClass : public QObject
{
    Q_OBJECT
public:
    explicit mainClass(QObject *parent = nullptr);
    void setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, QString outputFile, bool includeProtected, QString tempDir, bool incLookups, bool incmsels, QString firstSheetName, QString insertXML, bool separate);
    int returnCode;
signals:
    void finished();
public slots:
    void run();
private:
    void log(QString message);
    int generateXLSX();
    int parseDataToXLSX();
    void getFieldData(QString table, QString field, QString &desc, QString &valueType, int &size, int &decsize, bool &isMultiSelect, QString &multiSelectTable, QString &multiSelectField, QStringList &options, bool &isKey, QStringList &multiSelectKeys);
    const char *getSheetDescription(QString name);
    void loadTable(QDomNode node, QDomNode insertRoot);
    void getMultiSelectInfo(QDomNode table, QString table_name, QDomNode root_insert, QString &multiSelect_field, QStringList &options, QStringList &keys);
    QStringList getMultiSelectValues(QString multiSelectTable, QString multiSelectField, QStringList keys, QStringList multiSelectKeys);
    QString host;
    QString port;
    QString user;
    QString pass;
    QString schema;
    QString outputFile;
    QString tempDir;
    QString createXML;
    QString insertXML;
    bool includeSensitive;
    QList<TtableDef> tables;
    QList<TtableDef> mainTables;
    QStringList tableNames;
    int letterIndex;
    bool incLookups;
    bool incmsels;
    QString firstSheetName;
    bool separateSelects;
};

#endif // MAINCLASS_H
