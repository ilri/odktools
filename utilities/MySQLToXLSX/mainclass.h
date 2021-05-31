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
  QStringList multiSelectKeys;
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
    void setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, QString outputFile, bool protectSensitive, QString tempDir, bool incLookups, bool incmsels, QString firstSheetName, QString encryption_key);
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
    void getMultiSelectInfo(QDomNode table, QString table_name, QString &multiSelect_field, QStringList &keys);
    QString host;
    QString port;
    QString user;
    QString pass;
    QString schema;
    QString outputFile;
    QString tempDir;
    QString createXML;
    QString encryption_key;
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
