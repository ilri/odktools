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
  bool sensitive;
  bool key;
  QString replace_value;
  QString value;
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

class mainClass : public QObject
{
    Q_OBJECT
public:
    explicit mainClass(QObject *parent = nullptr);
    void setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, QString outputFile, QString tempDir, bool incLookups, bool incmsels, QString firstSheetName, bool protectSensitive);
    int returnCode;
signals:
    void finished();
public slots:
    void run();
private:
    void log(QString message);
    int generateXLSX();
    int parseDataToXLSX();
    void getFieldData(QString table, QString field, QString &desc, QString &valueType, int &size, int &decsize);
    const char *getSheetDescription(QString name);
    void loadTable(QDomNode node);
    QString protect_field(QString table_name, QString field_name, QString field_value);
    QString host;
    QString port;
    QString user;
    QString pass;
    QString schema;
    QString outputFile;
    QString tempDir;
    QString createXML;    
    QList<TtableDef> tables;
    QList<TtableDef> mainTables;
    QStringList tableNames;
    int letterIndex;
    bool incLookups;
    bool incmsels;
    QString firstSheetName;
    bool protectSensitive;
    QList<TfieldDef> keys;
    QList<TfieldDef> replace_values;
};

#endif // MAINCLASS_H
