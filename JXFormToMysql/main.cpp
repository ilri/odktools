/*
JXFormToMySQL

Copyright (C) 2019 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com)

JXFormToMySQL is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

JXFormToMySQL is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with JXFormToMySQL.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <tclap/CmdLine.h>
#include <QtXml>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QDomComment>
#include <QDirIterator>
#include <quazip5/quazip.h>
#include <quazip5/quazipfile.h>
#include <QDomDocument>
#include <csv.h>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

//*******************************************Global variables***********************************************
bool debug;
QString command;
QString outputType;
QString default_language;
QString strYes; //Yes string for comparing Yes/No lookup tables
QString strNo; //No string for comparing Yes/No lookup tables
QStringList variableStack; //This is a stack of groups or repeats for a variable. Used to get /xxx/xxx/xxx structures
QStringList repeatStack; //This is a stack of repeats. So we know in which repeat we are
QString prefix; //Table prefix
int tableIndex; //Global index of a table. Used later on to sort them
QStringList supportFiles;
bool primaryKeyAdded;
int CSVRowNumber;
bool CSVColumError;
QStringList CSVvalues;
QStringList CSVSQLs;
int numColumns;
int numColumnsInData;
QStringList duplicatedTables;
bool includeGroups;
bool justCheck;
QStringList requiredFiles;


//********************************************Global structures****************************************

//Structure that holds the description of each lkpvalue separated by language
struct lngDesc
{
    QString langCode;
    QString desc;
};
typedef lngDesc TlngLkpDesc;

struct sepSection
{
    QString name;
    QString desc;
};
typedef sepSection TsepSection;

//Variable mapping structure between ODK and MySQL
struct fieldMap
{
    QString type;
    int size;
    int decSize;
};
typedef fieldMap TfieldMap;

//Field Definition structure
struct fieldDef
{
  QString name; //Field Name
  QList<TlngLkpDesc > desc; //List of field descriptions in different languages
  QString type; //Variable type in MySQL
  QString odktype; //Variable type in MySQL
  int size; //Variable size
  int decSize; //Variable decimal size
  bool calculateWithSelect;
  QString formula;
  QString rTable; //Related table
  QString rField; //Related field
  bool key; //Whether the field is key
  QString xmlCode; //The field XML code /xx/xx/xx/xx
  QString xmlFullPath; //The field XML code /xx/xx/xx/xx with groups
  bool isMultiSelect; //Whether the field if multiselect
  QString multiSelectTable; //Multiselect table
  QString selectSource; //The source of the select. Internal, External or Search
  QString selectListName; //The list name of the select
};
typedef fieldDef TfieldDef;

struct ODKGroupingDef
{
  QString varibleType; //Type
  QString varibleName; //Name
  bool isRepeat;
};
typedef ODKGroupingDef TODKGroupingDef;

//Language structure
struct langDef
{
  QString code;
  QString desc;
  bool deflang; //Wether the language is default
};
typedef langDef TlangDef;

//List of languages
QList <TlangDef> languages;

//Sirvey Variables in ODK
struct surveyVariableDef
{
  QString name;
  QString fullName;
  QString type;
  QJsonObject object;
};
typedef surveyVariableDef TsurveyVariableDef;

//Lookup value structure
struct lkpValue
{
  QString code;
  QList<TlngLkpDesc > desc; //List of lookup values in different languages
};
typedef lkpValue TlkpValue;

//Table structure. Hold information about each table in terms of name, xmlCode, fields
//and, if its a lookuptable, the lookup values
struct tableDef
{
  QString name;
  QList<TlngLkpDesc > desc; //List of table descriptions in different languages
  QList<TfieldDef> fields; //List of fields
  QList<TlkpValue> lkpValues; //List of lookup values
  int pos; //Global position of the table
  bool islookup; //Whether the table is a lookup table
  bool isSeparated; //Whether the table has been separated
  QString xmlCode; //The table XML code /xx/xx/xx/xx
  QString xmlFullPath; //The table XML code /xx/xx/xx/xx
  QString parentTable; //The parent of the table
  QDomElement tableElement; //Each table is an Dom Element for building the manifest XML file
  QDomElement tableCreteElement; //Each table is a second Dom Element for building the XML Create file
  QStringList loopItems;
  bool isLoop;
  bool isOSM;
  bool isGroup;
};
typedef tableDef TtableDef;

QList<TtableDef> tables; //List of tables

struct duplicatedSelectValue
{
    QString variableName;
    QString selectValue;
};
typedef duplicatedSelectValue TduplicatedSelectValue;
QList<TduplicatedSelectValue> duplicatedSelectValues;

struct duplicatedField
{
    QString table;
    QStringList fields;
};
typedef duplicatedField TduplicatedField;
QList<TduplicatedField> duplicatedFields;

//***************************************Processes********************************************

void addRequiredFile(QString fileName)
{
    bool found = false;
    for (int pos = 0; pos < requiredFiles.count(); pos++)
    {
        if (requiredFiles[pos] == fileName)
        {
            found = true;
            break;
        }
    }
    if (!found)
        requiredFiles.append(fileName.toLower().simplified());
}

//Checks wether a field already exist in a table
void checkFieldName(TtableDef table, QString fieldName)
{
    for (int pos = 0; pos < table.fields.count(); pos++)
    {
        if (table.fields[pos].name == fieldName)
        {
            int idx;
            idx = -1;
            for (int pos2 = 0; pos2 < duplicatedFields.count(); pos2++)
            {
                if (duplicatedFields[pos2].table == table.name)
                {
                    idx = pos2;
                    break;
                }
            }
            if (idx == -1)
            {
                TduplicatedField duplicated;
                duplicated.table = table.name;
                duplicated.fields.append(fieldName);
                duplicatedFields.append(duplicated);
            }
            else
            {
                duplicatedFields[idx].fields.append(fieldName);
            }
        }
    }
}

//Checks wether the table aready exits.
void checkTableName(QString tableName)
{
    for (int pos = 0; pos < tables.count(); pos++)
    {
        if (tables[pos].name == tableName)
        {
            duplicatedTables.append(tableName);
        }
    }
}

int isSelect(QString variableType)
{
    QString varType;
    varType = variableType.toLower().trimmed();
    varType = varType.replace(" or specify other","");
    if (varType == "select one")
        return 1;
    if (varType == "select one external")
        return 2;
    if (varType == "select all that apply")
        return 3;
    if (varType == "rank")
        return 4;
    return 0;
}

bool isNote(QString variableType)
{
    QString varType;
    varType = variableType.toLower().trimmed();
    if (varType == "add note prompt")
        return true;
    if (varType == "note")
        return true;
    if (varType == "q note")
        return true;
    return false;
}

bool isCalculate(QString variableType)
{
    QString varType;
    varType = variableType.toLower().trimmed();
    if (varType == "add calculate prompt")
        return true;
    if (varType == "calculate")
        return true;
    if (varType == "q calculate")
        return true;
    return false;
}

//This logs messages to the terminal. We use printf because qDebug does not log in relase
void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s", temp.toUtf8().data());
}

void cb1(void *s, size_t, void *)
{
    char* charData;
    charData = (char*)s;
    CSVvalues.append(QString::fromUtf8(charData));
}

QString fixColumnName(QString column)
{
    QString res;
    res = column;
    res = res.toLower().trimmed();
    res = res.replace(":","_");
    res = res.replace("-","_");
    return res;
}

bool isColumnValid(QString column)
{
    QRegExp re("^[a-zA-Z0-9_]*$");
    if (re.exactMatch(column))
    {
        return true;
    }
    else
    {
        return false;
    }
}

void cb2(int , void *)
{
    QString sql;
    if (CSVRowNumber == 1)
    {
        sql = "CREATE TABLE data (";
        numColumns = 0;
        for (int pos = 0; pos <= CSVvalues.count()-1;pos++)
        {
            numColumns++;
            QString columnName;
            columnName = fixColumnName(CSVvalues[pos]);
            if (isColumnValid(columnName) == false)
                CSVColumError = true;
            sql = sql + columnName + " TEXT,";
        }
        sql = sql.left(sql.length()-1) + ");";
        CSVSQLs.append(sql);
    }
    else
    {
        sql = "INSERT INTO data VALUES (";
        numColumnsInData = 0;
        //Using numColumns so avoids more columns than the heading
        for (int pos = 0; pos <= numColumns-1;pos++)
        {
            numColumnsInData++;
            sql = sql + "\"" + CSVvalues[pos].replace("\"","") + "\",";
        }
        //This will fix if a row has less columns than the heading
        for (int pos =1; pos <= numColumns-numColumnsInData;pos++)
            sql = sql + "\"\"";
        sql = sql.left(sql.length()-1) + ");";
        CSVSQLs.append(sql);
    }
    CSVvalues.clear();
    CSVRowNumber++;
}

int convertCSVToSQLite(QString fileName, QDir tempDirectory, QSqlDatabase database)
{
    FILE *fp;
    struct csv_parser p;
    char buf[4096];
    size_t bytes_read;
    size_t retval;
    unsigned char options = 0;

    if (csv_init(&p, CSV_STRICT) != 0)
    {
        log("Failed to initialize csv parser");
        return 1;
    }
    fp = fopen(fileName.toUtf8().constData(), "rb");
    if (!fp)
    {
        log("Failed to open CSV file " + fileName);
        return 0;
    }
    options = CSV_APPEND_NULL;
    csv_set_opts(&p, options);
    CSVColumError = false;
    CSVRowNumber = 1;
    CSVvalues.clear();
    CSVSQLs.clear();
    while ((bytes_read=fread(buf, 1, 4096, fp)) > 0)
    {
        if ((retval = csv_parse(&p, buf, bytes_read, cb1, cb2, NULL)) != bytes_read)
        {
            if (csv_error(&p) == CSV_EPARSE)
            {
                log("Malformed data at byte " + QString::number((unsigned long)retval + 1) + " in file " + fileName);
                return 1;
            }
            else
            {
                log("Error \"" + QString::fromUtf8(csv_strerror(csv_error(&p))) + "\" in file " + fileName);
                return 1;
            }
        }
    }
    fclose(fp);
    csv_fini(&p, cb1, cb2, NULL);
    csv_free(&p);

    if (CSVColumError)
    {
        log("The CSV \"" + fileName + "\" has invalid characters. Only : and _ are allowed");
        exit(14);
    }

    QFileInfo fi(fileName);
    QString sqlLiteFile;
    sqlLiteFile = fi.baseName();
    sqlLiteFile = tempDirectory.absolutePath() + tempDirectory.separator() + sqlLiteFile + ".sqlite";
    if (QFile::exists(sqlLiteFile))
        if (!tempDirectory.remove(sqlLiteFile))
        {
            log("Cannot remove previous temporary file " + sqlLiteFile);
            return 1;
        }
    if (outputType == "h")
    {
        log("Converting " + fileName + " into SQLite");
    }

    database.setDatabaseName(sqlLiteFile);
    if (database.open())
    {
        QSqlQuery query(database);
        query.exec("BEGIN TRANSACTION");
        for (int pos = 0; pos <= CSVSQLs.count()-1;pos++)
        {
            if (!query.exec(CSVSQLs[pos]))
            {
                log("Cannot insert data for row: " + QString::number(pos+2) + " in file: " + sqlLiteFile + " reason: " + query.lastError().databaseText());
                query.exec("ROLLBACK TRANSACTION");
                return 1;
            }
        }
        query.exec("COMMIT TRANSACTION");
        database.close();
    }
    else
    {
        log("Cannot create SQLite database " + sqlLiteFile);
        return 1;
    }
    return 0;
}

// This function return the XML create element of a table.
// Used to produce the XML create file so a table can be a child of another table
QDomElement getTableCreateElement(QString table)
{
    QDomElement res;
    for (int pos = 0; pos <= tables.count()-1; pos++)
    {
        if (tables[pos].name.trimmed().toLower() == table.trimmed().toLower())
            return tables[pos].tableCreteElement;
    }
    return res;
}

// This function return the XML element of a table.
// Used to produce the import manifest so a table can be a child of another tbles
QDomElement getTableElement(QString table)
{
    QDomElement res;
    for (int pos = 0; pos <= tables.count()-1; pos++)
    {
        if (tables[pos].name.trimmed().toLower() == table.trimmed().toLower())
            return tables[pos].tableElement;
    }
    return res;
}

//This function sort values in lookuptable by code
bool lkpComp(TlkpValue left, TlkpValue right)
{
  return left.code < right.code;
}

//This function sort the tables by its position.
//The order is ascending so parent table are created before child tables.
//Lookup tables have a fix position of -1
bool tblComp(TtableDef left, TtableDef right)
{
    return left.pos < right.pos;
}

//This returns the description of a language
QString getDescForLanguage(QList<TlngLkpDesc > lkpList, QString langCode)
{
    for (int pos = 0; pos <= lkpList.count()-1;pos++)
    {
        if (lkpList[pos].langCode == langCode)
            return lkpList[pos].desc;
    }
    return "";
}

//This function returns the default language
QString getDefLanguage()
{
    for (int pos = 0; pos <= languages.count()-1; pos++)
    {
        if (languages[pos].deflang == true)
            return languages[pos].desc;
    }
    return "";
}

//This function returns the language code for a given name
QString getLanguageCode(QString languageName)
{
    for (int pos = 0; pos <= languages.count()-1; pos++)
    {
        if (languages[pos].desc == languageName)
            return languages[pos].code;
    }
    return "";
}

//This fuction checkd wheter a lookup table is duplicated.
//If there is a match then returns such table
TtableDef getDuplicatedLkpTable(QList<TlkpValue> thisValues)
{
    int pos2;
    bool same;
    TtableDef empty;
    empty.name = "EMPTY";
    empty.isLoop = false;
    empty.isOSM = false;
    empty.isGroup = false;
    QList<TlkpValue> currentValues;
    QString thisDesc;
    QString currenDesc;

    //Move the new list of values to a new list and sort it by code    
    qSort(thisValues.begin(),thisValues.end(),lkpComp);

    QString defLangCode;
    defLangCode = getLanguageCode(getDefLanguage());

    for (int pos = 0; pos <= tables.count()-1; pos++)
    {
        if (tables[pos].islookup)
        {
            //Move the current list of values to a new list and sort it by code
            currentValues.clear();
            currentValues.append(tables[pos].lkpValues);
            qSort(currentValues.begin(),currentValues.end(),lkpComp);

            if (currentValues.count() == thisValues.count()) //Same number of values
            {                
                same = true;
                for (pos2 = 0; pos2 <= tables[pos].lkpValues.count()-1;pos2++)
                {
                    //Compares if an item in the list dont have same code or same description
                    thisDesc = getDescForLanguage(thisValues[pos2].desc,defLangCode);
                    currenDesc = getDescForLanguage(currentValues[pos2].desc,defLangCode);

                    if ((currentValues[pos2].code.simplified().toLower() != thisValues[pos2].code.simplified().toLower()) ||
                            (currenDesc.simplified().toLower() != thisDesc.simplified().toLower()))
                    {                        
                        same = false;
                        break;
                    }
                }
                if (same)
                {
                    return tables[pos];
                }
            }
        }
    }
    return empty;
}

//This function return the key of a table using its name
QString getKeyField(QString table)
{
    int pos2;
    QString res;
    for (int pos = 0; pos <= tables.count()-1; pos++)
    {
        if (tables[pos].name == table)
        {
            for (pos2 = 0; pos2 <= tables[pos].fields.count()-1;pos2++)
            {
                if (tables[pos].fields[pos2].key == true)
                {
                    res = tables[pos].fields[pos2].name;
                    return res;
                }
            }
        }
    }
    return res;
}

//This function retrieves a coma separated string of the fields
// that are related to a table
QString getForeignColumns(TtableDef table, QString relatedTable)
{
    QString res;
    int pos;
    for (pos = 0; pos <= table.fields.count()-1; pos++)
    {
        if (table.fields[pos].rTable == relatedTable)
        {
            res = res + table.fields[pos].name + ",";
        }
    }
    res = res.left(res.length()-1);
    return res;
}

//This function retrieves a coma separated string of the related fields
// that are related to a table
QString getReferencedColumns(TtableDef table, QString relatedTable)
{
    QString res;
    int pos;
    for (pos = 0; pos <= table.fields.count()-1; pos++)
    {
        if (table.fields[pos].rTable == relatedTable)
        {
            res = res + table.fields[pos].rField + ",";
        }
    }
    res = res.left(res.length()-1);
    return res;
}

//This function returns wether or not a related table is a lookup table
bool isRelatedTableLookUp(QString relatedTable)
{
    int pos;
    for (pos = 0; pos <= tables.count()-1; pos++)
    {
        if (tables[pos].name.toLower() == relatedTable.toLower())
            return tables[pos].islookup;
    }
    return false;
}

QString fixString(QString source)
{
    QString res;
    int start;
    int finish;
    start = source.indexOf('\"');
    finish = source.lastIndexOf('\"');
    QString begin;
    QString end;
    QString middle;

    if (start != finish) //There are chars in between ""
    {
        begin = source.left(start);
        end = source.right(source.length()-finish-1);
        middle = source.mid(start+1,finish-start-1);
        res = begin + "“" + fixString(middle) + "”" + end; //Recursive
        res = res.replace('\n'," "); //Replace carry return for a space
        return res;
    }
    else
    {
        if ((start == -1) && (finish == -1)) //There are no " character
        {
            res = source;
            res = res.replace('\n'," "); //Replace carry return for a space
            return res;
        }
        else
        {
            if (start >= 0) //There is only one " character
            {
                res = source.replace('\"',"“");
                res = res.replace('\n'," "); //Replace carry return for a space
                return res;
            }
            else
            {
                res = source;
                res = res.replace('\n'," "); //Replace carry return for a space
                return res;
            }
        }
    }
}

//This is the main process that generates the DDL, DML and metadata SQLs.
//This process also generated the Import manifest file.
void generateOutputFiles(QString ddlFile,QString insFile, QString metaFile, QString xmlFile, QString transFile, QString XMLCreate, QString insertXML, QString dropSQL)
{
    QStringList fields;
    QStringList indexes;
    QStringList keys;
    QStringList rels;
    int clm;
    QString sql;
    QString keysql;
    QString insertSQL;
    QString field;
    int idx;
    idx = 0;

    QDomDocument outputdoc;
    outputdoc = QDomDocument("ODKImportFile");
    QDomElement root;
    root = outputdoc.createElement("ODKImportXML");
    root.setAttribute("version", "1.0");
    outputdoc.appendChild(root);

    //This is the XML representation of the schema
    QDomDocument XMLSchemaStructure;
    XMLSchemaStructure = QDomDocument("XMLSchemaStructure");
    QDomElement XMLRoot;
    XMLRoot = XMLSchemaStructure.createElement("XMLSchemaStructure");
    XMLRoot.setAttribute("version", "1.0");
    XMLSchemaStructure.appendChild(XMLRoot);

    QDomElement XMLLKPTables;
    XMLLKPTables = XMLSchemaStructure.createElement("lkptables");
    XMLRoot.appendChild(XMLLKPTables);

    QDomElement XMLTables;
    XMLTables = XMLSchemaStructure.createElement("tables");
    XMLRoot.appendChild(XMLTables);

    //This is the XML representation lookup values
    QDomDocument insertValuesXML;
    insertValuesXML = QDomDocument("insertValuesXML");
    QDomElement XMLInsertRoot;
    XMLInsertRoot = insertValuesXML.createElement("insertValuesXML");
    XMLInsertRoot.setAttribute("version", "1.0");
    insertValuesXML.appendChild(XMLInsertRoot);


    QString defLangCode;
    int lng;
    defLangCode = getLanguageCode(getDefLanguage());

    QDomElement fieldNode;

    QString index;
    QString constraint;

    //Create all the files and streams
    QFile sqlCreateFile(ddlFile);
    if (!sqlCreateFile.open(QIODevice::WriteOnly | QIODevice::Text))
             return;
    QTextStream sqlCreateStrm(&sqlCreateFile);

    QFile sqlInsertFile(insFile);
    if (!sqlInsertFile.open(QIODevice::WriteOnly | QIODevice::Text))
             return;
    QTextStream sqlInsertStrm(&sqlInsertFile);
    sqlInsertStrm.setCodec("UTF-8");

    QFile iso639File(transFile);
    if (!iso639File.open(QIODevice::WriteOnly | QIODevice::Text))
             return;
    QTextStream iso639Strm(&iso639File);
    iso639Strm.setCodec("UTF-8");

    QFile sqlUpdateFile(metaFile);
    if (!sqlUpdateFile.open(QIODevice::WriteOnly | QIODevice::Text))
             return;

    QTextStream sqlUpdateStrm(&sqlUpdateFile);
    sqlUpdateStrm.setCodec("UTF-8");


    QFile sqlDropFile(dropSQL);
    if (!sqlDropFile.open(QIODevice::WriteOnly | QIODevice::Text))
             return;

    QTextStream sqlDropStrm(&sqlDropFile);
    sqlDropStrm.setCodec("UTF-8");

    //Start creating the header or each file.
    QDateTime date;
    date = QDateTime::currentDateTime();

    QStringList rTables;

    sqlCreateStrm << "-- Code generated by JXFormToMySQL" << "\n";
    sqlCreateStrm << "-- " + command << "\n";
    sqlCreateStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    sqlCreateStrm << "-- by: JXFormToMySQL Version 1.0" << "\n";
    sqlCreateStrm << "-- WARNING! All changes made in this file might be lost when running JXFormToMySQL again" << "\n\n";

    sqlDropStrm << "-- Code generated by JXFormToMySQL" << "\n";
    sqlDropStrm << "-- " + command << "\n";
    sqlDropStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    sqlDropStrm << "-- by: JXFormToMySQL Version 1.0" << "\n";
    sqlDropStrm << "-- WARNING! All changes made in this file might be lost when running JXFormToMySQL again" << "\n\n";

    sqlInsertStrm << "-- Code generated by JXFormToMySQL" << "\n";
    sqlInsertStrm << "-- " + command << "\n";
    sqlInsertStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    sqlInsertStrm << "-- by: JXFormToMySQL Version 1.0" << "\n";
    sqlInsertStrm << "-- WARNING! All changes made in this file might be lost when running JXFormToMySQL again" << "\n\n";
    sqlInsertStrm << "START TRANSACTION;" << "\n\n";

    iso639Strm << "-- Code generated by JXFormToMySQL" << "\n";
    iso639Strm << "-- " + command << "\n";
    iso639Strm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    iso639Strm << "-- by: JXFormToMySQL Version 1.0" << "\n";
    iso639Strm << "-- WARNING! All changes made in this file might be lost when running JXFormToMySQL again" << "\n\n";

    sqlUpdateStrm << "-- Code generated by JXFormToMySQL" << "\n";
    sqlUpdateStrm << "-- " + command << "\n";
    sqlUpdateStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    sqlUpdateStrm << "-- by: JXFormToMySQL Version 1.0" << "\n";
    sqlUpdateStrm << "-- WARNING! All changes made in this file might be lost when running JXFormToMySQL again" << "\n\n";

    for (int pos = 0; pos <= languages.count()-1;pos++)
    {
        if (languages[pos].code != "")
        {
            insertSQL = "INSERT INTO dict_iso639 (lang_cod,lang_des,lang_def) VALUES (";
            insertSQL = insertSQL + "'" + languages[pos].code + "',";
            insertSQL = insertSQL + "\"" + fixString(languages[pos].desc) + "\",";
            insertSQL = insertSQL + QString::number(languages[pos].deflang) + ");\n";
            iso639Strm << insertSQL;
        }
    }
    iso639Strm << "\n";

    qSort(tables.begin(),tables.end(),tblComp);

    for (int pos = tables.count()-1; pos >=0;pos--)
    {
        sqlDropStrm << "DROP TABLE IF EXISTS " + prefix + tables[pos].name.toLower() + ";\n";
    }

    for (int pos = 0; pos <= tables.count()-1;pos++)
    {
        //Set some XML attributes in the manifest file
        if (tables[pos].islookup == false)
        {
            if (tables[pos].xmlCode != "NONE")
            {
                //For the manifest XML
                tables[pos].tableElement = outputdoc.createElement("table");
                tables[pos].tableElement.setAttribute("mysqlcode",prefix + tables[pos].name.toLower());
                tables[pos].tableElement.setAttribute("xmlcode",tables[pos].xmlCode);
                tables[pos].tableElement.setAttribute("parent",tables[pos].parentTable);
                if (tables[pos].isSeparated == true)
                    tables[pos].tableElement.setAttribute("separated","true");
                if (tables[pos].isLoop)
                {
                    tables[pos].tableElement.setAttribute("loop","true");
                    QString loopItems;
                    loopItems = tables[pos].loopItems.join(QChar(743));
                    tables[pos].tableElement.setAttribute("loopitems",loopItems);
                }
                if (tables[pos].isOSM)
                    tables[pos].tableElement.setAttribute("osm","true");
                if (tables[pos].isGroup)
                    tables[pos].tableElement.setAttribute("group","true");


                //For the create XML
                tables[pos].tableCreteElement = XMLSchemaStructure.createElement("table");
                tables[pos].tableCreteElement.setAttribute("name",prefix + tables[pos].name.toLower());
                tables[pos].tableCreteElement.setAttribute("desc",fixString(getDescForLanguage(tables[pos].desc,getLanguageCode(getDefLanguage()))));
                tables[pos].tableCreteElement.setAttribute("sensitive","false");
            }
            else
            {
                //For the create XML
                tables[pos].tableCreteElement = XMLSchemaStructure.createElement("table");
                tables[pos].tableCreteElement.setAttribute("name",prefix + tables[pos].name.toLower());
                tables[pos].tableCreteElement.setAttribute("desc",fixString(getDescForLanguage(tables[pos].desc,getLanguageCode(getDefLanguage()))));
                tables[pos].tableCreteElement.setAttribute("sensitive","false");
            }
        }
        else
        {
            tables[pos].tableCreteElement = XMLSchemaStructure.createElement("table");
            tables[pos].tableCreteElement.setAttribute("name",prefix + tables[pos].name.toLower());
            tables[pos].tableCreteElement.setAttribute("desc",fixString(getDescForLanguage(tables[pos].desc,getLanguageCode(getDefLanguage()))));
            tables[pos].tableCreteElement.setAttribute("sensitive","false");

            //Append the values to the XML insert
            QDomElement lkptable = insertValuesXML.createElement("table");
            lkptable.setAttribute("name",prefix + tables[pos].name.toLower());
            lkptable.setAttribute("clmcode",tables[pos].fields[0].name);
            lkptable.setAttribute("clmdesc",tables[pos].fields[1].name);
            for (int nlkp = 0; nlkp < tables[pos].lkpValues.count();nlkp++)
            {
                QDomElement aLKPValue = insertValuesXML.createElement("value");
                aLKPValue.setAttribute("code",tables[pos].lkpValues[nlkp].code);
                aLKPValue.setAttribute("description",fixString(getDescForLanguage(tables[pos].lkpValues[nlkp].desc,defLangCode)));
                lkptable.appendChild(aLKPValue);
            }
            XMLInsertRoot.appendChild(lkptable);
        }

        //Update the dictionary tables to set the table description
        sqlUpdateStrm << "UPDATE dict_tblinfo SET tbl_des = \"" + fixString(getDescForLanguage(tables[pos].desc,getLanguageCode(getDefLanguage()))) + "\" WHERE tbl_cod = '" + prefix + tables[pos].name.toLower() + "';\n";

        //Insert the translation of the table into the dictionary translation table
        for (lng = 0; lng <= languages.count()-1;lng++)
        {
            if (languages[lng].deflang == false)
            {
                insertSQL = "INSERT INTO dict_dctiso639 (lang_cod,trans_des,tblinfo_cod) VALUES (";
                insertSQL = insertSQL + "'" + languages[lng].code + "',";
                insertSQL = insertSQL + "\"" + fixString(getDescForLanguage(tables[pos].desc,languages[lng].code)) + "\",";
                insertSQL = insertSQL + "'" + prefix + tables[pos].name.toLower() + "');\n";
                iso639Strm << insertSQL;
            }
        }

        //Clear the controlling lists
        fields.clear();
        indexes.clear();
        keys.clear();
        rels.clear();
        sql = "";
        keysql = "";

        //Start of a create script for each table
        fields << "CREATE TABLE IF NOT EXISTS " + prefix + tables[pos].name.toLower() + "(" << "\n";

        keys << "PRIMARY KEY (";
        for (clm = 0; clm <= tables[pos].fields.count()-1; clm++)
        {

            //Append the fields and child tables to the manifest file
            if (tables[pos].islookup == false)
            {
                if (tables[pos].xmlCode != "NONE")
                {
                    if ((tables[pos].fields[clm].xmlCode != "main"))
                    {
                        //For the manifest XML
                        fieldNode = outputdoc.createElement("field");
                        fieldNode.setAttribute("mysqlcode",tables[pos].fields[clm].name.toLower());
                        fieldNode.setAttribute("xmlcode",tables[pos].fields[clm].xmlCode);
                        fieldNode.setAttribute("type",tables[pos].fields[clm].type);
                        fieldNode.setAttribute("odktype",tables[pos].fields[clm].odktype);
                        fieldNode.setAttribute("size",tables[pos].fields[clm].size);
                        fieldNode.setAttribute("decsize",tables[pos].fields[clm].decSize);
                        if (tables[pos].fields[clm].isMultiSelect == true)
                        {
                            fieldNode.setAttribute("isMultiSelect","true");
                            fieldNode.setAttribute("multiSelectTable",prefix + tables[pos].fields[clm].multiSelectTable);
                        }
                        if (tables[pos].fields[clm].key)
                        {
                            fieldNode.setAttribute("key","true");
                            if (tables[pos].fields[clm].rTable != "" && tables[pos].fields[clm].rTable.left(3) != "lkp")
                                fieldNode.setAttribute("reference","true");
                            else
                                fieldNode.setAttribute("reference","false");
                        }
                        else
                        {
                            fieldNode.setAttribute("key","false");
                            fieldNode.setAttribute("reference","false");
                        }
                        tables[pos].tableElement.appendChild(fieldNode);

                        //For the create XML
                        QDomElement createFieldNode;
                        createFieldNode = XMLSchemaStructure.createElement("field");
                        createFieldNode.setAttribute("name",tables[pos].fields[clm].name.toLower());
                        createFieldNode.setAttribute("desc",fixString(getDescForLanguage(tables[pos].fields[clm].desc,defLangCode)));
                        createFieldNode.setAttribute("type",tables[pos].fields[clm].type);
                        createFieldNode.setAttribute("odktype",tables[pos].fields[clm].odktype);
                        createFieldNode.setAttribute("size",tables[pos].fields[clm].size);
                        createFieldNode.setAttribute("decsize",tables[pos].fields[clm].decSize);
                        createFieldNode.setAttribute("sensitive","false");

                        if (tables[pos].fields[clm].isMultiSelect == true)
                        {
                            createFieldNode.setAttribute("isMultiSelect","true");
                            createFieldNode.setAttribute("multiSelectTable",prefix + tables[pos].fields[clm].multiSelectTable);
                        }

                        if (tables[pos].fields[clm].key)
                            createFieldNode.setAttribute("key","true");
                        if (tables[pos].fields[clm].rTable != "")
                        {
                            createFieldNode.setAttribute("rtable",prefix + tables[pos].fields[clm].rTable);
                            createFieldNode.setAttribute("rfield",tables[pos].fields[clm].rField);

                            if (isRelatedTableLookUp(tables[pos].fields[clm].rTable))
                            {
                                createFieldNode.setAttribute("rlookup","true");
                            }

                        }
                        tables[pos].tableCreteElement.appendChild(createFieldNode);
                    }
                    else
                    {
                        QDomElement createFieldNode;
                        createFieldNode = XMLSchemaStructure.createElement("field");
                        createFieldNode.setAttribute("name",tables[pos].fields[clm].name.toLower());
                        createFieldNode.setAttribute("type",tables[pos].fields[clm].type);
                        createFieldNode.setAttribute("odktype",tables[pos].fields[clm].odktype);
                        createFieldNode.setAttribute("size",tables[pos].fields[clm].size);
                        createFieldNode.setAttribute("decsize",tables[pos].fields[clm].decSize);
                        createFieldNode.setAttribute("sensitive","false");
                        if (tables[pos].fields[clm].key)
                            createFieldNode.setAttribute("key","true");
                        if (tables[pos].fields[clm].rTable != "")
                        {
                            createFieldNode.setAttribute("rtable",prefix + tables[pos].fields[clm].rTable);
                            createFieldNode.setAttribute("rfield",tables[pos].fields[clm].rField);                            
                            if (isRelatedTableLookUp(tables[pos].fields[clm].rTable))
                            {
                                createFieldNode.setAttribute("rlookup","true");
                            }
                        }
                        tables[pos].tableCreteElement.appendChild(createFieldNode);
                    }
                }
                else
                {
                    QDomElement createFieldNode;
                    createFieldNode = XMLSchemaStructure.createElement("field");
                    createFieldNode.setAttribute("name",tables[pos].fields[clm].name.toLower());
                    createFieldNode.setAttribute("desc",fixString(getDescForLanguage(tables[pos].fields[clm].desc,defLangCode)));
                    createFieldNode.setAttribute("type",tables[pos].fields[clm].type);
                    createFieldNode.setAttribute("odktype",tables[pos].fields[clm].odktype);
                    createFieldNode.setAttribute("size",tables[pos].fields[clm].size);
                    createFieldNode.setAttribute("decsize",tables[pos].fields[clm].decSize);
                    createFieldNode.setAttribute("sensitive","false");
                    if (tables[pos].fields[clm].key)
                        createFieldNode.setAttribute("key","true");
                    if (tables[pos].fields[clm].rTable != "")
                    {
                        createFieldNode.setAttribute("rtable",prefix + tables[pos].fields[clm].rTable);
                        createFieldNode.setAttribute("rfield",tables[pos].fields[clm].rField);
                        if (isRelatedTableLookUp(tables[pos].fields[clm].rTable))
                        {
                            createFieldNode.setAttribute("rlookup","true");
                        }
                    }
                    tables[pos].tableCreteElement.appendChild(createFieldNode);
                }
            }
            else
            {
                QDomElement createFieldNode;
                createFieldNode = XMLSchemaStructure.createElement("field");
                createFieldNode.setAttribute("name",tables[pos].fields[clm].name.toLower());
                createFieldNode.setAttribute("desc",fixString(getDescForLanguage(tables[pos].fields[clm].desc,defLangCode)));
                createFieldNode.setAttribute("type",tables[pos].fields[clm].type);
                createFieldNode.setAttribute("odktype",tables[pos].fields[clm].odktype);
                createFieldNode.setAttribute("size",tables[pos].fields[clm].size);
                createFieldNode.setAttribute("decsize",tables[pos].fields[clm].decSize);
                createFieldNode.setAttribute("sensitive","false");
                if (tables[pos].fields[clm].key)
                    createFieldNode.setAttribute("key","true");
                if (tables[pos].fields[clm].rTable != "")
                {
                    createFieldNode.setAttribute("rtable",prefix + tables[pos].fields[clm].rTable);
                    createFieldNode.setAttribute("rfield",tables[pos].fields[clm].rField);                    
                }
                tables[pos].tableCreteElement.appendChild(createFieldNode);
            }

            //Update the dictionary tables to the set column description
            sqlUpdateStrm << "UPDATE dict_clminfo SET clm_des = \"" + fixString(getDescForLanguage(tables[pos].fields[clm].desc,defLangCode)) + "\" WHERE tbl_cod = '" + prefix + tables[pos].name.toLower() + "' AND clm_cod = '" + tables[pos].fields[clm].name + "';\n";

            //Insert the translation of the column into the dictionary translation table
            for (lng = 0; lng <= languages.count()-1;lng++)
            {
                if (languages[lng].deflang == false)
                {
                    insertSQL = "INSERT INTO dict_dctiso639 (lang_cod,trans_des,tbl_cod,clm_cod) VALUES (";
                    insertSQL = insertSQL + "'" + languages[lng].code + "',";
                    insertSQL = insertSQL + "\"" + fixString(getDescForLanguage(tables[pos].fields[clm].desc,languages[lng].code)) + "\",";
                    insertSQL = insertSQL + "'" + prefix + tables[pos].name.toLower() + "',";
                    insertSQL = insertSQL + "'" + tables[pos].fields[clm].name + "');\n";
                    iso639Strm << insertSQL;
                }
            }
            //Work out the mySQL column types
            field = "";
            if ((tables[pos].fields[clm].type == "varchar") || (tables[pos].fields[clm].type == "int"))
                field = tables[pos].fields[clm].name.toLower() + " " + tables[pos].fields[clm].type + "(" + QString::number(tables[pos].fields[clm].size) + ")";
            else
                if (tables[pos].fields[clm].type == "decimal")
                    field = tables[pos].fields[clm].name.toLower() + " " + tables[pos].fields[clm].type + "(" + QString::number(tables[pos].fields[clm].size) + "," + QString::number(tables[pos].fields[clm].decSize) + ")";
                else
                    field = tables[pos].fields[clm].name.toLower() + " " + tables[pos].fields[clm].type;

            if (tables[pos].fields[clm].key == true)
                field = field + " NOT NULL COMMENT \"" + fixString(getDescForLanguage(tables[pos].fields[clm].desc,defLangCode)) + "\", ";
            else
                field = field + " COMMENT \"" + fixString(getDescForLanguage(tables[pos].fields[clm].desc,defLangCode)) + "\", ";

            fields << field << "\n";

            if (tables[pos].fields[clm].key == true)
                keys << tables[pos].fields[clm].name + " , ";

            //Here we create the indexes and constraints for lookup tables using RESTRICT

            if (!tables[pos].fields[clm].rTable.isEmpty())
            {
                if (isRelatedTableLookUp(tables[pos].fields[clm].rTable))
                {
                    idx++;
                    index = "INDEX fk" + QString::number(idx) + "_" + prefix + tables[pos].name.toLower() + "_" + prefix + tables[pos].fields[clm].rTable.toLower() ;
                    indexes << index.left(64) + " (" + tables[pos].fields[clm].name.toLower() + ") , " << "\n";

                    constraint = "CONSTRAINT fk" + QString::number(idx) + "_" + prefix + tables[pos].name.toLower() + "_" + prefix + tables[pos].fields[clm].rTable.toLower();
                    rels << constraint.left(64) << "\n";
                    rels << "FOREIGN KEY (" + tables[pos].fields[clm].name.toLower() + ")" << "\n";
                    rels << "REFERENCES " + prefix + tables[pos].fields[clm].rTable.toLower() + " (" + tables[pos].fields[clm].rField.toLower() + ")" << "\n";

                    rels << "ON DELETE RESTRICT " << "\n";
                    rels << "ON UPDATE NO ACTION," << "\n";
                }
            }
        }
        //Append each child table to its parent in the manifest file. This
        //is only for no lookup tables.
        if (tables[pos].islookup == false)
        {
            //qDebug() << tables[pos].name;
            //qDebug() << tables[pos].parentTable;
            if (tables[pos].xmlCode != "NONE")
            {
                if (tables[pos].parentTable == "NULL")
                {
                    root.appendChild(tables[pos].tableElement);
                    XMLTables.appendChild(tables[pos].tableCreteElement);
                }
                else
                {
                    QDomElement parentElement;
                    parentElement = getTableElement(tables[pos].parentTable);
                    parentElement.appendChild(tables[pos].tableElement);

                    QDomElement parentCreateElement;
                    parentCreateElement = getTableCreateElement(tables[pos].parentTable);
                    parentCreateElement.appendChild(tables[pos].tableCreteElement);
                }
            }
            else
            {
                if (tables[pos].parentTable == "NULL")
                {
                    XMLTables.appendChild(tables[pos].tableCreteElement);
                }
                else
                {
                    QDomElement parentCreateElement;
                    parentCreateElement = getTableCreateElement(tables[pos].parentTable);
                    parentCreateElement.appendChild(tables[pos].tableCreteElement);
                }
            }
        }
        else
        {
            XMLLKPTables.appendChild(tables[pos].tableCreteElement);
        }
        rTables.clear();
        //Extract all related tables into rTables that are not lookups
        for (clm = 0; clm <= tables[pos].fields.count()-1; clm++)
        {
            if (!tables[pos].fields[clm].rTable.isEmpty())
            {
                if (!isRelatedTableLookUp(tables[pos].fields[clm].rTable))
                    if (rTables.indexOf(tables[pos].fields[clm].rTable) < 0)
                        rTables.append(tables[pos].fields[clm].rTable);
            }
        }

        //Creating indexes and references for those tables the are not lookup tables using CASCADE";

        for (clm = 0; clm <= rTables.count()-1; clm++)
        {
            idx++;
            index = "INDEX fk" + QString::number(idx) + "_" + prefix + tables[pos].name.toLower() + "_" + prefix + rTables[clm].toLower() ;
            indexes << index.left(64) + " (" + getForeignColumns(tables[pos],rTables[clm]) + ") , " << "\n";

            constraint = "CONSTRAINT fk" + QString::number(idx) + "_" + prefix + tables[pos].name.toLower() + "_" + prefix + rTables[clm].toLower();
            rels << constraint.left(64) << "\n";
            rels << "FOREIGN KEY (" + getForeignColumns(tables[pos],rTables[clm]) + ")" << "\n";
            rels << "REFERENCES " + prefix + rTables[clm].toLower() + " (" + getReferencedColumns(tables[pos],rTables[clm]) + ")" << "\n";
            if (!isRelatedTableLookUp(tables[pos].name))
                rels << "ON DELETE CASCADE " << "\n";
            else
                rels << "ON DELETE RESTRICT " << "\n";
            rels << "ON UPDATE NO ACTION," << "\n";
        }

        //Contatenate al different pieces of the create script into one SQL
        for (clm = 0; clm <= fields.count() -1;clm++)
        {
            sql = sql + fields[clm];
        }
        for (clm = 0; clm <= keys.count() -1;clm++)
        {
            keysql = keysql + keys[clm];
        }
        clm = keysql.lastIndexOf(",");
        keysql = keysql.left(clm) + ") , \n";

        sql = sql + keysql;

        for (clm = 0; clm <= indexes.count() -1;clm++)
        {
            sql = sql + indexes[clm];
        }
        for (clm = 0; clm <= rels.count() -1;clm++)
        {
            sql = sql + rels[clm];
        }
        clm = sql.lastIndexOf(",");
        sql = sql.left(clm);
        sql = sql + ")" + "\n ENGINE = InnoDB CHARSET=utf8 COMMENT = \"" + fixString(getDescForLanguage(tables[pos].desc,getLanguageCode(getDefLanguage()))) + "\"; \n";
        idx++;
        sql = sql + "CREATE UNIQUE INDEX rowuuid" + QString::number(idx) + " ON " + prefix + tables[pos].name.toLower() + "(rowuuid);\n";

        // Append UUIDs triggers to the file but only for those
        // That are lookups. The other tables will have an UUID
        // when data in inserted using JSONToMySQL
        if (tables[pos].islookup == true)
            sql = sql + "CREATE TRIGGER uudi_" + prefix+ tables[pos].name.toLower() + " BEFORE INSERT ON " + prefix + tables[pos].name.toLower() + " FOR EACH ROW SET new.rowuuid = uuid();\n\n";
        else
            sql = sql + "\n";

        sqlCreateStrm << sql; //Add the triggers to the SQL DDL file

        //Create the inserts of the lookup tables values into the insert SQL
        if (tables[pos].lkpValues.count() > 0)
        {
            for (clm = 0; clm <= tables[pos].lkpValues.count()-1;clm++)
            {
                insertSQL = "INSERT INTO " + prefix + tables[pos].name.toLower() + " (";
                for (int pos2 = 0; pos2 <= tables[pos].fields.count()-2;pos2++)
                {
                    insertSQL = insertSQL + tables[pos].fields[pos2].name + ",";
                }
                insertSQL = insertSQL.left(insertSQL.length()-1) + ")  VALUES ('";

                insertSQL = insertSQL + tables[pos].lkpValues[clm].code.replace("'","`") + "',\"";
                insertSQL = insertSQL + fixString(getDescForLanguage(tables[pos].lkpValues[clm].desc,defLangCode)) + "\");";
                sqlInsertStrm << insertSQL << "\n";

                for (lng = 0; lng <= tables[pos].lkpValues[clm].desc.count() -1; lng++)
                {
                    if (tables[pos].lkpValues[clm].desc[lng].langCode != defLangCode)
                    {
                        insertSQL = "INSERT INTO dict_lkpiso639 (tbl_cod,lang_cod,lkp_value,lkp_desc) VALUES (";
                        insertSQL = insertSQL + "'" + prefix + tables[pos].name.toLower() + "',";
                        insertSQL = insertSQL + "'" + tables[pos].lkpValues[clm].desc[lng].langCode + "',";
                        insertSQL = insertSQL + "'" + tables[pos].lkpValues[clm].code + "',";
                        insertSQL = insertSQL + "\"" + fixString(tables[pos].lkpValues[clm].desc[lng].desc) + "\");";
                        iso639Strm << insertSQL << "\n";
                    }
                }
            }
        }
    }

    sqlInsertStrm << "COMMIT;\n";
    //Create the manifext file. If exist it get overwriten
    if (QFile::exists(xmlFile))
        QFile::remove(xmlFile);
    QFile file(xmlFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        out.setCodec("UTF-8");        
        outputdoc.save(out,1,QDomNode::EncodingFromTextStream);
        file.close();
    }
    else
        log("Error: Cannot create xml manifest file");

    //Create the XMLCreare file. If exist it get overwriten
    if (QFile::exists(XMLCreate))
        QFile::remove(XMLCreate);
    QFile XMLCreateFile(XMLCreate);
    if (XMLCreateFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream outXMLCreate(&XMLCreateFile);
        outXMLCreate.setCodec("UTF-8");
        XMLSchemaStructure.save(outXMLCreate,1,QDomNode::EncodingFromTextStream);
        XMLCreateFile.close();
    }
    else
        log("Error: Cannot create xml create file");

    //Create the XML Insert file. If exist it get overwriten
    if (QFile::exists(insertXML))
        QFile::remove(insertXML);
    QFile XMLInsertFile(insertXML);
    if (XMLInsertFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream outXMLInsert(&XMLInsertFile);
        outXMLInsert.setCodec("UTF-8");
        insertValuesXML.save(outXMLInsert,1,QDomNode::EncodingFromTextStream);
        XMLInsertFile.close();
    }
    else
        log("Error: Cannot create xml insert file");
}

//This function maps ODK XML Form data types to MySQL data types
TfieldMap mapODKFieldTypeToMySQL(QString ODKFieldType)
{
    TfieldMap result;
    result.type = "varchar";
    result.size = 255;
    result.decSize = 0;
    if (ODKFieldType == "acknowledge")
    {
        result.size = 2;
    }
    if (ODKFieldType == "add acknowledge prompt")
    {
        result.size = 2;
    }
    if (ODKFieldType == "add date prompt")
    {
        result.type = "date";
    }
    if (ODKFieldType == "add date time prompt")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "add dateTime prompt")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "add decimal prompt")
    {
        result.type = "decimal";
        result.size = 10;
        result.decSize = 3;
    }
    if (ODKFieldType == "add integer prompt")
    {
        result.type = "int";
        result.size = 9;
    }
    if (ODKFieldType == "add location prompt")
    {
        result.size = 120;
    }
    if (ODKFieldType == "audio")
    {
        result.size = 120;
    }
    if (ODKFieldType == "audit")
    {
        result.size = 120;
    }
    if (ODKFieldType == "date")
    {
        result.type = "date";
    }
    if (ODKFieldType == "date time")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "dateTime")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "datetime")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "decimal")
    {
        result.type = "decimal";
        result.size = 10;
        result.decSize = 3;
    }
    if (ODKFieldType == "device id")
    {
        result.size = 120;
    }
    if (ODKFieldType == "deviceid")
    {
        result.size = 120;
    }
    if (ODKFieldType == "email")
    {
        result.size = 120;
    }
    if (ODKFieldType == "end")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "end time")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "file")
    {
        result.size = 120;
    }
    if (ODKFieldType == "geopoint")
    {
        result.size = 120;
    }
    if (ODKFieldType == "get device id")
    {
        result.size = 120;
    }
    if (ODKFieldType == "get end time")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "get phone number")
    {
        result.size = 120;
    }
    if (ODKFieldType == "get sim id")
    {
        result.size = 120;
    }
    if (ODKFieldType == "get start time")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "get subscriber id")
    {
        result.size = 120;
    }
    if (ODKFieldType == "get today")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "gps")
    {
        result.size = 120;
    }
    if (ODKFieldType == "int")
    {
        result.type = "int";
        result.size = 9;
    }
    if (ODKFieldType == "integer")
    {
        result.type = "int";
        result.size = 9;
    }
    if (ODKFieldType == "location")
    {
        result.size = 120;
    }
    if (ODKFieldType == "number of days in last month")
    {
        result.type = "int";
        result.size = 9;
    }
    if (ODKFieldType == "number of days in last six months")
    {
        result.type = "int";
        result.size = 9;
    }
    if (ODKFieldType == "number of days in last year")
    {
        result.type = "int";
        result.size = 9;
    }
    if (ODKFieldType == "percentage")
    {
        result.type = "int";
        result.size = 9;
    }
    if (ODKFieldType == "phone number")
    {
        result.size = 120;
    }
    if (ODKFieldType == "phonenumber")
    {
        result.size = 120;
    }
    if (ODKFieldType == "photo")
    {
        result.size = 120;
    }
    if (ODKFieldType == "q acknowledge")
    {
        result.size = 2;
    }
    if (ODKFieldType == "q audio")
    {
        result.size = 120;
    }
    if (ODKFieldType == "q date")
    {
        result.type = "date";
    }
    if (ODKFieldType == "q date time")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "q dateTime")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "q decimal")
    {
        result.type = "decimal";
        result.size = 10;
        result.decSize = 3;
    }
    if (ODKFieldType == "q geopoint")
    {
        result.size = 120;
    }
    if (ODKFieldType == "q image")
    {
        result.size = 120;
    }
    if (ODKFieldType == "q int")
    {
        result.type = "int";
        result.size = 9;
    }
    if (ODKFieldType == "q location")
    {
        result.size = 120;
    }
    if (ODKFieldType == "q picture")
    {
        result.size = 120;
    }
    if (ODKFieldType == "q string")
    {
        result.size = 120;
    }
    if (ODKFieldType == "q video")
    {
        result.size = 120;
    }
    if (ODKFieldType == "range")
    {
        result.type = "decimal";
        result.size = 10;
        result.decSize = 3;
    }
    if (ODKFieldType == "sim id")
    {
        result.size = 120;
    }
    if (ODKFieldType == "simserial")
    {
        result.size = 120;
    }
    if (ODKFieldType == "start")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "start time")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "string")
    {
       result.size = 120;
    }
    if (ODKFieldType == "subscriber id")
    {
        result.size = 120;
    }
    if (ODKFieldType == "subscriberid")
    {
        result.size = 120;
    }
    if (ODKFieldType == "time")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "today")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "trigger")
    {
        result.size = 2;
    }
    if (ODKFieldType == "uri:deviceid")
    {
        result.size = 120;
    }
    if (ODKFieldType == "uri:email")
    {
        result.size = 120;
    }
    if (ODKFieldType == "uri:phonenumber")
    {
        result.size = 120;
    }
    if (ODKFieldType == "uri:simserial")
    {
        result.size = 120;
    }
    if (ODKFieldType == "uri:subscriberid")
    {
        result.size = 120;
    }
    if (ODKFieldType == "uri:username")
    {
        result.size = 120;
    }
    if (ODKFieldType == "username")
    {
        result.size = 120;
    }
    if (ODKFieldType == "video")
    {
        result.size = 120;
    }
    if (ODKFieldType == "xml-external")
    {
        result.size = 120;
    }
    if ((ODKFieldType == "geoshape") || (ODKFieldType == "q geoshape"))
    {
        result.type = "text";
    }
    if ((ODKFieldType == "geotrace") || (ODKFieldType == "q geotrace"))
    {
        result.type = "text";
    }
    return result;
}

//Return whether the values of a lookup table are numbers or strings. Used to determine the type of variables in the lookup tables
bool areValuesStrings(QList<TlkpValue> values)
{
    bool ok;
    int pos;
    for (pos = 0; pos <= values.count()-1;pos++)
    {
        values[pos].code.toInt(&ok,10);
        if (!ok)
        {
            return true;
        }
    }
    return false;
}

int getMaxMSelValueLength(QList<TlkpValue> values)
{
    QString res;
    for (int pos = 0; pos <= values.count()-1;pos++)
    {
        res = res + values[pos].code.trimmed() + " ";
    }
    res = res.left(res.length()-1);
    return res.length();
}

//Return the maximum lenght of the values in a lookup table so the size is not excesive for primary keys
int getMaxValueLength(QList<TlkpValue> values)
{
    int res;
    res = 0;
    int pos;
    for (pos = 0; pos <= values.count()-1;pos++)
    {
        if (values[pos].code.length() >= res)
            res = values[pos].code.length();
    }
    return res;
}

int getMaxDescLength(QList<TlkpValue> values)
{
    int res;
    res = 0;
    int pos;
    int lng;
    for (pos = 0; pos <= values.count()-1;pos++)
    {
        for (lng = 0; lng < values[pos].desc.count();lng++)
        {
            if (values[pos].desc[lng].desc.length() >= res)
                res = values[pos].desc[lng].desc.length();
        }
    }
    return res;
}


//Returns wheter or not a the values of a lookup table are yes/no values. Used in combination to yes/no lookup values to
//create or not a lookup table of yes/no.
// NOTE this is a very crude function and only catches a couple of cases...
bool isLookUpYesNo(QList<TlkpValue> values)
{
    bool hasNo;
    bool hasYes;
    int lkp;
    QString lkpValue;
    QString defLang;
    defLang = getLanguageCode(getDefLanguage());
    if (values.count() == 2 )
    {
        hasNo = false;
        hasYes = false;
        for (lkp = 0; lkp <= values.count()-1;lkp++)
        {
            lkpValue = getDescForLanguage(values[lkp].desc,defLang);

            //Evaluate 0 = No, 1 = yes
            if ((values[lkp].code.toInt() == 0) && ((lkpValue.trimmed().toLower() == strNo.toLower()) || (lkpValue.trimmed().toLower() == "0-" + strNo.toLower()) || (lkpValue.trimmed().toLower() == "0- " + strNo.toLower())))
                hasNo = true;
            if ((values[lkp].code.toInt() == 1) && ((lkpValue.trimmed().toLower() == strYes.toLower()) || (lkpValue.trimmed().toLower() == "1-" + strYes.toLower()) || (lkpValue.trimmed().toLower() == "1- " + strYes.toLower())))
                hasYes = true;

            //Evalues 1 = yes, 2 = no
            if ((values[lkp].code.toInt() == 1) && ((lkpValue.trimmed().toLower() == strYes.toLower()) || (lkpValue.trimmed().toLower() == "1-"+ strYes.toLower()) || (lkpValue.trimmed().toLower() == "1- "+ strYes.toLower())))
                hasNo = true;
            if ((values[lkp].code.toInt() == 2) && ((lkpValue.trimmed().toLower() == strNo.toLower()) || (lkpValue.trimmed().toLower() == "2-" + strNo.toLower()) || (lkpValue.trimmed().toLower() == "2- " + strNo.toLower())))
                hasYes = true;

            //Evalues 1 = no, 2 = Yes
            if ((values[lkp].code.toInt() == 2) && ((lkpValue.trimmed().toLower() == strYes.toLower()) || (lkpValue.trimmed().toLower() == "2-"+ strYes.toLower()) || (lkpValue.trimmed().toLower() == "2- "+ strYes.toLower())))
                hasNo = true;
            if ((values[lkp].code.toInt() == 1) && ((lkpValue.trimmed().toLower() == strNo.toLower()) || (lkpValue.trimmed().toLower() == "1-" + strNo.toLower()) || (lkpValue.trimmed().toLower() == "1- " + strNo.toLower())))
                hasYes = true;
        }
        if (hasYes && hasNo)
            return true;
        else
            return false;
    }
    else
        return false;
}

//Return the index of table in the list using its name
int getTableIndex(QString name)
{
    for (int pos = 0; pos <= tables.count()-1;pos++)
    {
        if (!tables[pos].islookup)
        {
            if (tables[pos].name.trimmed().toLower() == name.trimmed().toLower())
                return pos;
        }
    }
    return -1;
}

//Append the UniqueIDS to each table
void appendUUIDs()
{
    int pos;
    int lang;
    for (pos = 0; pos <= tables.count()-1;pos++)
    {
        TfieldDef UUIDField;
        UUIDField.name = "rowuuid";        

        for (lang = 0; lang <= languages.count()-1;lang++)
        {
            TlngLkpDesc langDesc;
            langDesc.langCode = languages[lang].code;
            langDesc.desc = "Unique Row Identifier (UUID)";
            UUIDField.desc.append(langDesc);
        }
        UUIDField.key = false;
        UUIDField.type = "varchar";
        UUIDField.size = 80;
        UUIDField.decSize = 0;
        UUIDField.rTable = "";
        UUIDField.rField = "";
        UUIDField.xmlCode = "NONE";
        UUIDField.isMultiSelect = false;
        UUIDField.formula = "";
        UUIDField.calculateWithSelect = false;
        UUIDField.selectSource = "NONE";
        UUIDField.selectListName = "NONE";
        tables[pos].fields.append(UUIDField);
    }
}

//Checks whether a language is the default language
bool isDefaultLanguage(QString language)
{
    for (int pos=0; pos < languages.count();pos++)
    {
        if (languages[pos].desc.toLower().trimmed() == language.toLower().trimmed())
            if (languages[pos].deflang == true)
                return true;
    }
    return false;
}

//Get an language index by its name
int genLangIndexByName(QString language)
{    
    for (int pos=0; pos < languages.count();pos++)
    {                
        if (languages[pos].desc.toLower().trimmed() == language.toLower().trimmed())
            return pos;
    }
    return -1;    
}

//Adds a group or repeat to the stack
void addToStack(QString groupOrRepeat, QString type)
{
    variableStack.append(groupOrRepeat + "@" + type);
}

//Fixes table and field name by removing invalid MySQL characters
QString fixField(QString source)
{
    QString res;
    res = source;    
    res = res.replace("-","_");
    QRegularExpression re("[^.a-zA-Z0-9\\$\\_]");
    res = res.replace(re,"");
    res = res.trimmed().simplified().toLower();
    return res;
}

//Adds a repeat to the repeat stack
void addToRepeat(QString repeat)
{
    repeatStack.append(fixField(repeat.trimmed().toLower()));
}

//Removes one group or repeat from the variable stack
bool removeFromStack()
{
    if (variableStack.count() > 0)
    {
        variableStack.removeLast();
        return true;
    }
    return false;
}

//Removes one repeat from the stack
bool removeRepeat()
{
    if (repeatStack.count() > 0)
    {
        repeatStack.removeLast();
        return true;
    }
    return false;
}

//This return the stack for a variable in format /xxx/xxx/xxx/
QString getVariableStack(bool full)
{
    QString res;
    for (int pos = 0; pos < variableStack.count();pos++)
    {
        QStringList parts = variableStack[pos].split("@",QString::SkipEmptyParts);
        if (full)
            res = res + parts[0] + "/";
        else
        {
            if (parts[1] != "group")
                res = res + parts[0] + "/";
        }
    }
    if (res.length() > 0)
        res = res.left(res.length()-1);
    return res;
}

//This gets the active repeat from the stack
QString getTopRepeat()
{
    if (repeatStack.count() > 0)
        return repeatStack.last();
    else
        return "";
}

//Return an item in the list of tables using its name
TtableDef getTable(QString name)
{
    TtableDef res;
    res.name = "";
    res.isLoop = false;
    res.isOSM = false;
    res.isGroup = false;
    for (int pos = 0; pos <= tables.count()-1;pos++)
    {
        if (!tables[pos].islookup)
        {
            if (tables[pos].name.trimmed().toLower() == name.trimmed().toLower())
                return tables[pos];
        }
    }
    return res;
}

bool selectHasOrOther(QString variableType)
{
    if (variableType.toLower().trimmed().indexOf("or specify other") >= 0)
        return true;
    else
        return false;
}

//This return the labels of any variable in different languages
QList <TlngLkpDesc > getLabels(QJsonValue labelValue)
{
    QList <TlngLkpDesc > labels;
    if (!labelValue.isObject())
    {
        int languageIndex;
        languageIndex = 0;
        for (int lng = 0; lng < languages.count();lng++)
        {
            if (languages[lng].deflang == true)
            {
                languageIndex = lng;
                break;
            }
        }
        TlngLkpDesc fieldDesc;
        fieldDesc.langCode = languages[languageIndex].code;
        fieldDesc.desc = labelValue.toString("Without label");
        labels.append(fieldDesc);
    }
    else
    {
        QJsonObject labelObject;
        labelObject = labelValue.toObject();
        for (int lbl = 0; lbl < labelObject.keys().count(); lbl++)
        {
            TlngLkpDesc fieldDesc;
            fieldDesc.langCode = getLanguageCode(labelObject.keys()[lbl].toLower().trimmed());
            fieldDesc.desc = labelObject.value(labelObject.keys()[lbl]).toString();
            labels.append(fieldDesc);
        }
    }
    return labels;
}

bool checkSelectValue(QString variableName, QList<TlkpValue> values, QString value, bool report=true)
{
    for (int idx = 0; idx < values.count(); idx++)
    {
        if (values[idx].code.toLower().trimmed() == value.toLower().trimmed())
        {
            if (report)
            {
                TduplicatedSelectValue duplicated;
                duplicated.variableName = variableName;
                duplicated.selectValue = value.toLower().trimmed();
                duplicatedSelectValues.append(duplicated);
            }
            return true;
        }
    }
    return false;
}

// This return the values of a select that uses an external xml file.
// e.g., "select one from file a_file.xml" and "select multiple from file a_file.xml"
QList<TlkpValue> getSelectValuesFromXML(QString variableName, QString fileName, bool hasOrOther, int &result, QDir dir)
{
    QList<TlkpValue> res;
    QString codeColumn;
    codeColumn = "name";
    QStringList descColumns;
    result = 0;
    descColumns << "label";
    descColumns << "label::" + getDefLanguage().toLower().trimmed();
    for (int lng = 0; lng < languages.count(); lng++)
    {
        if (descColumns.indexOf("label::" + languages[lng].desc) < 0)
            descColumns.append("label::" + languages[lng].desc.toLower().trimmed());
    }
    QString xmlFile;
    xmlFile = dir.absolutePath() + dir.separator() + fileName;
    addRequiredFile(fileName);
    if (QFile::exists(xmlFile))
    {
        QDomDocument xmlDocument;
        QFile file(xmlFile);
        if (!file.open(QIODevice::ReadOnly))
        {
            log("Cannot open XML resource file: "+ xmlFile);
            result = 21;
        }

        if (!xmlDocument.setContent(&file))
        {
            log("Cannot parse XML resource file: "+ xmlFile);
            file.close();
            result = 21;
        }
        file.close();
        QDomNodeList items;
        items = xmlDocument.elementsByTagName("item");
        if (items.count() > 0)
        {
            for (int nitem = 0; nitem < items.count(); nitem++)
            {
                QDomElement Eitem = items.item(nitem).toElement();
                QDomNodeList columns;
                columns = Eitem.elementsByTagName(codeColumn);
                if (columns.count() > 0)
                {
                    TlkpValue value;
                    value.code = columns.item(0).firstChild().nodeValue();
                    for (int pos = 0; pos <= descColumns.count()-1; pos++)
                    {
                        columns = Eitem.elementsByTagName(descColumns[pos]);
                        if (columns.count() > 0)
                        {
                            if (descColumns[pos].indexOf("::") >= 0)
                            {
                                QStringList parts;
                                parts = descColumns[pos].split("::",QString::SkipEmptyParts);
                                QString langCode;
                                langCode = getLanguageCode(parts[1]);
                                if (langCode != "")
                                {
                                    TlngLkpDesc desc;
                                    desc.desc = columns.item(0).firstChild().nodeValue();
                                    desc.langCode = langCode;
                                    value.desc.append(desc);
                                }
                                else
                                {
                                    if (!justCheck)
                                        log("Language " + parts[1] + " was not found in the parameters. Please indicate it as default language (-d) or as other lannguage (-l)");
                                    result = 4;
                                }
                            }
                            else
                            {
                                TlngLkpDesc desc;
                                desc.desc = columns.item(0).firstChild().nodeValue();
                                desc.langCode = getLanguageCode(getDefLanguage());
                                value.desc.append(desc);
                            }
                        }
                    }
                    checkSelectValue(variableName,res,value.code);
                    res.append(value);
                }
                else
                {
                    if (!justCheck)
                        log("Unable to find the column called name in the XML file \"" + fileName + "\". Is this a valid XML resource");
                    result = 12;
                }
            }
            if (hasOrOther)
            {
                bool duplicated = checkSelectValue(variableName,res,"other",false);
                if (!duplicated)
                {
                    TlkpValue value;
                    value.code = "other";
                    for (int lng = 0; lng < languages.count(); lng++)
                    {
                        TlngLkpDesc desc;
                        desc.langCode = languages[lng].code;
                        desc.desc = "Other";
                        value.desc.append(desc);
                    }
                    res.append(value);
                }
            }
        }
        else
        {
            if (!justCheck)
                log("Unable to retreive items from the XML file \"" + fileName + "\". Is this a valid XML resource");
            result = 12;
        }
    }
    else
    {        
        if (!justCheck)
            log("There is no XML file for \"" + fileName + "\". Did you add it when you ran JXFormToMySQL?");
        result = 13;
    }
    return res;
}

// This return the values of a select that uses an external CSV file.
// e.g., "select one from file a_file.csv","select multiple from file a_file.csv","select one external"
QList<TlkpValue> getSelectValuesFromCSV2(QString variableName, QString fileName, bool hasOrOther, int &result, QDir dir, QSqlDatabase database, QString queryValue)
{
    QList<TlkpValue> res;
    QString codeColumn;
    codeColumn = "name";
    QStringList descColumns;
    result = 0;
    descColumns << "label";
    descColumns << "label::" + getDefLanguage().toLower().trimmed();
    for (int lng = 0; lng < languages.count(); lng++)
    {
        if (descColumns.indexOf("label::" + languages[lng].desc) < 0)
            descColumns.append("label::" + languages[lng].desc.toLower().trimmed());
    }

    QString sqliteFile;
    //There should be an sqlite version of such file in the temporary directory
    addRequiredFile(fileName);
    sqliteFile = dir.absolutePath() + dir.separator() + fileName.replace(".csv","") + ".sqlite";
    if (QFile::exists(sqliteFile))
    {
        database.setDatabaseName(sqliteFile);
        if (database.open())
        {
            //Contruct a query from using "codeColumn" and the list "descColumns"
            QSqlQuery query(database);
            QString sql;
            sql = "SELECT * FROM data";
            if (queryValue != "")
            {
                sql = sql + " WHERE list_name = '" + queryValue + "'";
            }
            if (query.exec(sql))
            {
                //Appends each value as a select value
                while (query.next())
                {
                    QVariant queryValue;
                    queryValue = query.value(fixColumnName(codeColumn));
                    if (queryValue.isValid())
                    {
                        TlkpValue value;
                        value.code = query.value(fixColumnName(codeColumn)).toString();
                        for (int pos = 0; pos <= descColumns.count()-1; pos++)
                        {
                            queryValue = query.value(fixColumnName(descColumns[pos]));
                            if (queryValue.isValid())
                            {
                                if (descColumns[pos].indexOf("::") >= 0)
                                {
                                    QStringList parts;
                                    parts = descColumns[pos].split("::",QString::SkipEmptyParts);
                                    QString langCode;
                                    langCode = getLanguageCode(parts[1]);
                                    if (langCode != "")
                                    {
                                        TlngLkpDesc desc;
                                        desc.desc = queryValue.toString();
                                        desc.langCode = langCode;
                                        value.desc.append(desc);
                                    }
                                    else
                                    {
                                        if (!justCheck)
                                            log("Language " + parts[1] + " was not found in the parameters. Please indicate it as default language (-d) or as other lannguage (-l)");
                                        result = 4;
                                    }
                                }
                                else
                                {
                                    TlngLkpDesc desc;
                                    desc.desc = queryValue.toString();
                                    desc.langCode = getLanguageCode(getDefLanguage());
                                    value.desc.append(desc);
                                }
                            }
                        }
                        checkSelectValue(variableName,res,value.code);
                        res.append(value);
                    }
                    else
                    {
                        if (!justCheck)
                            log("The file " + fileName + "is not a valid ODK resource file");
                        result = 20;
                    }
                }
                if (hasOrOther)
                {
                    bool duplicated = checkSelectValue(variableName,res,"other",false);
                    if (!duplicated)
                    {
                        TlkpValue value;
                        value.code = "other";
                        for (int lng = 0; lng < languages.count(); lng++)
                        {
                            TlngLkpDesc desc;
                            desc.langCode = languages[lng].code;
                            desc.desc = "Other";
                            value.desc.append(desc);
                        }
                        res.append(value);
                    }
                }
                result = 0;
                return res;
            }
            else
            {
                if (!justCheck)
                    log("Unable to retreive data for search \"" + fileName + "\". Reason: " + query.lastError().databaseText() + ". Maybe the \"name column\" or any of the \"labels columns\" do not exist in the CSV?");
                result = 12;
            }
        }
        else
        {
            if (!justCheck)
                log("Cannot create SQLite database " + sqliteFile);
            result = 1;
        }
    }
    else
    {        
        if (!justCheck)
            log("There is no SQLite file for file \"" + fileName + "\". Did you add it as CSV when you ran JXFormToMySQL?");
        result = 13;
    }
    return res;
}

// This return the values of a select that uses an external CSV file through a search expresion in apperance.
// e.g.:
//       type: select one canton
//       appearance: search('cantones', 'matches', 'a_column', ${a_variable})
QList<TlkpValue> getSelectValuesFromCSV(QString searchExpresion, QJsonArray choices,QString variableName, bool hasOrOther, int &result, QDir dir, QSqlDatabase database)
{    
    QList<TlkpValue> res;
    QString codeColumn;
    codeColumn = "";
    result = 0;
    QList<TlngLkpDesc> descColumns;    
    for (int nrow = 0; nrow < choices.count(); nrow++)
    {
        QJsonValue JSONValue = choices.at(nrow);
        codeColumn = JSONValue.toObject().value("name").toString();
        QJsonValue JSONlabel = JSONValue.toObject().value("label");
        descColumns.append(getLabels(JSONlabel));
    }

    if ((codeColumn != "") && (descColumns.count() > 0))
    {
        int pos;
        //Extract the file from the expression
        pos = searchExpresion.indexOf("'");
        QString file = searchExpresion.right(searchExpresion.length()-(pos+1));
        pos = file.indexOf("'");
        file = file.left(pos);
        QString sqliteFile;
        //There should be an sqlite version of such file in the temporary directory
        addRequiredFile(file + ".csv");
        sqliteFile = dir.absolutePath() + dir.separator() + file + ".sqlite";
        if (QFile::exists(sqliteFile))
        {
            database.setDatabaseName(sqliteFile);
            if (database.open())
            {
                //Contruct a query from using "codeColumn" and the list "descColumns"
                QSqlQuery query(database);
                QString sql;
                sql = "SELECT " + fixColumnName(codeColumn) + ",";
                for (int pos = 0; pos <= descColumns.count()-1; pos++)
                {
                    if (descColumns[pos].desc != "NONE")
                        sql = sql + fixColumnName(descColumns[pos].desc) + ",";
                }
                sql = sql.left(sql.length()-1) + " FROM data";
                if (query.exec(sql))
                {
                    //Appends each value as a select value
                    while (query.next())
                    {
                        TlkpValue value;
                        value.code = query.value(fixColumnName(codeColumn)).toString();
                        for (int pos = 0; pos <= descColumns.count()-1; pos++)
                        {
                            if (descColumns[pos].desc != "NONE")
                            {
                                TlngLkpDesc desc;
                                desc.langCode = descColumns[pos].langCode;
                                desc.desc = query.value(fixColumnName(descColumns[pos].desc)).toString();
                                value.desc.append(desc);
                            }
                            else
                            {
                                TlngLkpDesc desc;
                                desc.langCode = descColumns[pos].langCode;
                                desc.desc = "";
                                value.desc.append(desc);
                            }
                        }
                        checkSelectValue(variableName,res,value.code);
                        res.append(value);
                    }
                    if (hasOrOther)
                    {
                        bool duplicated = checkSelectValue(variableName,res,"other",false);
                        if (!duplicated)
                        {
                            TlkpValue value;
                            value.code = "other";
                            for (int lng = 0; lng < languages.count(); lng++)
                            {
                                TlngLkpDesc desc;
                                desc.langCode = languages[lng].code;
                                desc.desc = "Other";
                                value.desc.append(desc);
                            }
                            res.append(value);
                        }
                    }
                    result = 0;
                }
                else
                {
                    if (!justCheck)
                        log("Unable to retreive data for search \"" + file + "\". Reason: " + query.lastError().databaseText() + ". Maybe the \"name column\" or any of the \"labels columns\" do not exist in the CSV?");
                    result = 12;
                }
            }
            else
            {
                if (!justCheck)
                    log("Cannot create SQLite database " + sqliteFile);
                result = 1;
            }
        }
        else
        {            
            if (!justCheck)
                log("There is no SQLite file for search \"" + file + "\". Did you add it as CSV when you ran JXFormToMySQL?");
            result = 13;
        }
    }
    else
    {
        if (codeColumn != "")
            log("Cannot locate the code column for the search select \"" + variableName + "\"");
        if (descColumns.count() == 0)
            log("Cannot locate a description column for the search select \"" + variableName + "\"");
        result = 16;
    }
    return res;
}

//This return the values of a simple select or select multiple
QList<TlkpValue> getSelectValues(QString variableName, QJsonArray choices, bool hasOther)
{
    QList<TlkpValue> res;
    for (int nrow = 0; nrow < choices.count(); nrow++)
    {
        QJsonValue JSONValue = choices.at(nrow);
        TlkpValue value;
        value.code = JSONValue.toObject().value("name").toString();
        QJsonValue JSONlabel = JSONValue.toObject().value("label");
        value.desc = getLabels(JSONlabel);
        checkSelectValue(variableName,res,value.code);
        res.append(value);
    }
    if (hasOther)
    {
        bool duplicated = checkSelectValue(variableName,res,"other",false);
        if (!duplicated)
        {
            TlkpValue value;
            value.code = "other";
            for (int lng = 0; lng < languages.count(); lng++)
            {
                TlngLkpDesc desc;
                desc.langCode = languages[lng].code;
                desc.desc = "Other";
                value.desc.append(desc);
            }
            res.append(value);
        }
    }

    return res;

}

// This checks the expresion of a calculate to see if it maps a select multiple
// e.g.,:
//        type: calculate
//        calculation: selected-at(${variable_that_is_select_multiple}, position(..)-1)
void getReferenceForSelectAt(QString calculateExpresion,QString &fieldType, int &fieldSize, int &fieldDecSize, QString &fieldRTable, QString &fieldRField)
{
    int pos;
    //Extract the file from the expression
    pos = calculateExpresion.indexOf("{");
    QString variable = calculateExpresion.right(calculateExpresion.length()-(pos+1));
    pos = variable.indexOf("}");
    variable = variable.left(pos);
    variable = fixField(variable);

    QString multiSelectTable;
    multiSelectTable = "";
    bool found;
    found = false;
    for (int pos = 0; pos <= tables.count()-1; pos++)
    {
        for (int pos2 = 0; pos2 <= tables[pos].fields.count()-1; pos2++)
        {
            if (tables[pos].fields[pos2].name.toLower().trimmed() == variable.toLower().trimmed())
            {
                multiSelectTable = tables[pos].fields[pos2].multiSelectTable;
                found = true;
                break;
            }
        }
        if (found)
            break;
    }
    if (multiSelectTable != "")
    {
        for (int pos = 0; pos <= tables.count()-1; pos++)
        {
            if (tables[pos].name == multiSelectTable)
            {
                for (int pos2 = 0; pos2 <= tables[pos].fields.count()-1; pos2++)
                {
                    if (tables[pos].fields[pos2].name == variable)
                    {
                        fieldType = tables[pos].fields[pos2].type;
                        fieldSize = tables[pos].fields[pos2].size;
                        fieldDecSize = tables[pos].fields[pos2].decSize;
                        fieldRTable = tables[pos].fields[pos2].rTable;
                        fieldRField = tables[pos].fields[pos2].rField;                        
                        return;
                    }
                }
            }
        }
    }
}

//Adds OSM tags as columns to a OSM table
void parseOSMField(TtableDef &OSMTable, QJsonObject fieldObject)
{
    QString variableName;
    variableName = fieldObject.value("name").toString();
    variableName = variableName.toLower();
    if (fieldObject.keys().indexOf("choices") >= 0)
    {
        QList<TlkpValue> values;
        values.append(getSelectValues(variableName,fieldObject.value("choices").toArray(),false));

        TfieldDef aField;
        aField.selectSource = "NONE";
        aField.name = fixField(variableName.toLower());
        checkFieldName(OSMTable,aField.name);
        aField.odktype = "NONE";
        aField.selectListName = "NONE";
        aField.calculateWithSelect = false;
        aField.formula = "";
        aField.type = "varchar";
        aField.size = getMaxValueLength(values);
        aField.decSize = 0;
        aField.key = false;
        aField.isMultiSelect = false;
        aField.multiSelectTable = "";
        aField.formula = "";
        aField.calculateWithSelect = false;
        aField.xmlCode = variableName;
        aField.xmlFullPath = "NONE";
        QJsonValue labelValue = fieldObject.value("label");
        aField.desc.append(getLabels(labelValue));
        if (values.count() > 0) //If the lookup table has values
        {
            //Creating the lookp table if its neccesary
            if (isLookUpYesNo(values) == false) //Do not process yes/no values as lookup tables
            {
                TtableDef lkpTable = getDuplicatedLkpTable(values);
                lkpTable.isLoop = false;
                lkpTable.isOSM = false;
                lkpTable.isGroup = false;
                if (lkpTable.name == "EMPTY")
                {
                    lkpTable.name = "lkp" + fixField(variableName.toLower());
                    for (int lang = 0; lang < aField.desc.count(); lang++)
                    {
                        TlngLkpDesc fieldDesc;
                        fieldDesc.langCode = aField.desc[lang].langCode;
                        fieldDesc.desc = "Lookup table (" + aField.desc[lang].desc + ")";
                        lkpTable.desc.append(fieldDesc);
                    }
                    lkpTable.pos = -1;
                    lkpTable.islookup = true;
                    lkpTable.isSeparated = false;
                    lkpTable.lkpValues.append(values);
                    //Creates the field for code in the lookup
                    TfieldDef lkpCode;
                    lkpCode.name = fixField(variableName.toLower()) + "_cod";
                    lkpCode.selectSource = "NONE";
                    lkpCode.selectListName = "NONE";
                    for (int lang = 0; lang <= languages.count()-1;lang++)
                    {
                        TlngLkpDesc langDesc;
                        langDesc.langCode = languages[lang].code;
                        langDesc.desc = "Code";
                        lkpCode.desc.append(langDesc);
                    }
                    lkpCode.key = true;
                    lkpCode.type = aField.type;
                    lkpCode.size = aField.size;
                    lkpCode.decSize = aField.decSize;
                    lkpTable.fields.append(lkpCode);
                    //Creates the field for description in the lookup
                    TfieldDef lkpDesc;
                    lkpDesc.name = fixField(variableName.toLower()) + "_des";
                    lkpDesc.selectSource = "NONE";
                    lkpDesc.selectListName = "NONE";
                    for (int lang = 0; lang <= languages.count()-1;lang++)
                    {
                        TlngLkpDesc langDesc;
                        langDesc.langCode = languages[lang].code;
                        langDesc.desc = "Description";
                        lkpDesc.desc.append(langDesc);
                    }
                    lkpDesc.key = false;
                    lkpDesc.type = "varchar";
                    lkpDesc.size = getMaxDescLength(values);
                    lkpDesc.decSize = 0;
                    lkpTable.fields.append(lkpDesc);
                    aField.rTable = lkpTable.name;
                    aField.rField = lkpCode.name;
                    tables.append(lkpTable);
                }
                else
                {
                    if (outputType == "h")
                        log("Lookup table for field " + variableName + " is the same as " + lkpTable.name + ". Using " + lkpTable.name);
                    aField.rTable = lkpTable.name;
                    aField.rField = getKeyField(lkpTable.name);
                }
            }
        }
        else
        {
            aField.rTable = "";
            aField.rField = "";
        }
        OSMTable.fields.append(aField);
    }
    else
    {
        TfieldDef aField;
        aField.name = fixField(variableName.toLower());
        aField.xmlCode = variableName;
        aField.selectSource = "NONE";
        aField.selectListName = "NONE";
        aField.odktype = "NONE";
        aField.type = "varchar";
        aField.size = 255;
        aField.decSize = 0;
        aField.calculateWithSelect = false;
        aField.isMultiSelect = false;
        aField.formula = "";
        aField.rTable = "";
        aField.rField = "";
        QJsonValue labelValue = fieldObject.value("label");
        aField.desc.append(getLabels(labelValue));
        OSMTable.fields.append(aField);
    }
}

// Adds a ODK table as a field to a table
void parseField(QJsonObject fieldObject, QString mainTable, QString mainField, QDir dir, QSqlDatabase database, QString varXMLCode ="")
{
    QString variableApperance;
    QString variableCalculation;
    QString tableName;
    int tblIndex;
    tableName = getTopRepeat();
    if (tableName == "")
        tableName = mainTable;
    tblIndex = getTableIndex(tableName);
    QString variableType;
    variableType = fieldObject.value("type").toString("text");
    QString variableName;
    variableName = fieldObject.value("name").toString();    
    //log("\tProcessing field:" + variableName);
    QString tableType = "NotLoop";
    if (tables[tblIndex].isLoop)
        tableType = "loop";

    if (isNote(variableType))
        return; //Don't store notes

    if (isSelect(variableType) == 0)
    {
        TfieldDef aField;
        aField.name = fixField(variableName.toLower());        
        TfieldMap vartype = mapODKFieldTypeToMySQL(variableType);
        aField.selectSource = "NONE";
        aField.selectListName = "NONE";
        aField.odktype = variableType;
        aField.type = vartype.type;
        aField.size = vartype.size;
        aField.decSize = vartype.decSize;
        aField.calculateWithSelect = false;
        aField.formula = "";
        if (fixField(variableName) == fixField(mainField.toLower()))
        {
            if (!primaryKeyAdded)
                aField.key = true;
            else
                return;
        }
        else
            aField.key = false;

        if (isCalculate(variableType))
        {
            if (fieldObject.keys().indexOf("bind") >= 0)
            {
                QString calculation;
                calculation = fieldObject.value("bind").toObject().value("calculate").toString();
                if (!calculation.isNull())
                {
                    calculation = calculation.toLower().simplified();
                    if (calculation.indexOf("pulldata") >= 0)
                    {
                        int temp;
                        temp = calculation.indexOf(",");
                        calculation = calculation.left(temp);
                        temp = calculation.indexOf("(");
                        calculation = calculation.right(calculation.length()-temp-1);
                        calculation.replace("'","");
                        calculation = calculation + ".csv";
                        addRequiredFile(calculation);
                    }
                }

            }
        }

        checkFieldName(tables[tblIndex],aField.name);

        if (!isCalculate(variableType))
        {
            aField.rTable = "";
            aField.rField = "";
        }
        else
        {
            variableCalculation = fieldObject.value("bind").toObject().value("calculate").toString().toLower();
            if (variableCalculation.indexOf("selected-at(") >=0 )
            {
                aField.rTable = "";
                aField.rField = "";
                aField.calculateWithSelect = true;
                aField.formula = variableCalculation;                
            }
            else
            {
                aField.rTable = "";
                aField.rField = "";
            }

        }
        if (getVariableStack(true) != "")
        {
            if (tableType != "loop")
            {
                aField.xmlCode = getVariableStack(true) + "/" + variableName;
                aField.xmlFullPath = getVariableStack(true) + "/" + variableName;
            }
            else
            {
                aField.xmlCode = variableName;
                aField.xmlFullPath = variableName;
            }
        }
        else
        {
            aField.xmlCode = variableName;
            aField.xmlFullPath = variableName;
        }
        if (varXMLCode != "")
            aField.xmlCode = varXMLCode;

        aField.isMultiSelect = false;
        aField.multiSelectTable = "";

        QJsonValue labelValue = fieldObject.value("label");
        aField.desc.append(getLabels(labelValue));
        tables[tblIndex].fields.append(aField);
    }
    else
    {
        //Gather the lookup values
        QList<TlkpValue> values;
        if ((isSelect(variableType) == 1) || (isSelect(variableType) == 3) || (isSelect(variableType) == 4))
        {
            bool fromSearchCSV;
            fromSearchCSV = false;
            if (fieldObject.keys().indexOf("control") >= 0)
            {               
                variableApperance = fieldObject.value("control").toObject().value("appearance").toString("").toLower().trimmed();
                if (variableApperance != "")
                {
                    if (variableApperance.indexOf("search(") >= 0)
                        fromSearchCSV = true;
                }
            }

            if ((fieldObject.value("itemset").toString("").toLower().trimmed().indexOf(".csv") > 0) || (fieldObject.value("itemset").toString("").toLower().trimmed().indexOf(".xml") > 0))
            {
                if (fieldObject.value("itemset").toString("").toLower().trimmed().indexOf(".csv") > 0)
                {
                    QString fileName;
                    fileName = fieldObject.value("itemset").toString("").toLower().trimmed();
                    int result;
                    values.append(getSelectValuesFromCSV2(fixField(variableName),fileName,selectHasOrOther(variableType),result,dir,database,""));
                    if (result != 0)
                    {                        
                        if (!justCheck)
                            exit(result);
                    }
                }
                else                    
                {
                    QString fileName;
                    fileName = fieldObject.value("itemset").toString("").toLower().trimmed();
                    int result;
                    values.append(getSelectValuesFromXML(fixField(variableName),fileName,selectHasOrOther(variableType),result,dir));
                    if (result != 0)
                    {
                        if (!justCheck)
                            exit(result);
                    }
                }
            }
            else
            {
                if (!fromSearchCSV)
                    values.append(getSelectValues(fixField(variableName),fieldObject.value("choices").toArray(),selectHasOrOther(variableType)));
                else
                {
                    int result;
                    values.append(getSelectValuesFromCSV(variableApperance,fieldObject.value("choices").toArray(),fixField(variableName),selectHasOrOther(variableType),result,dir,database));
                    if (result != 0)
                    {
                        if (result == 16)
                            exit(result);
                        if (!justCheck)
                            exit(result);
                    }
                }
            }
        }
        if ((isSelect(variableType) == 2))
        {
            QString queryField;
            queryField = fieldObject.value("query").toString("");
            if (queryField != "")
            {
                int result;
                values.append(getSelectValuesFromCSV2(fixField(variableName),"itemsets.csv",selectHasOrOther(variableType),result,dir,database,queryField));
                if (result != 0)
                    exit(19);
            }
        }

        //Creating a select one field
        if ((isSelect(variableType) == 1) || (isSelect(variableType) == 2))
        {
            TfieldDef aField;
            aField.selectSource = "NONE";
            aField.name = fixField(variableName.toLower());

            aField.odktype = variableType;
            aField.selectListName = "NONE";
            aField.calculateWithSelect = false;
            aField.formula = "";
            if (!selectHasOrOther(variableType))
            {
                if (areValuesStrings(values))
                    aField.type = "varchar";
                else
                    aField.type = "int";
            }
            else
                aField.type = "varchar";
            aField.size = getMaxValueLength(values);
            aField.decSize = 0;
            if (fixField(variableName) == fixField(mainField.toLower()))
            {
                if (!primaryKeyAdded)
                    aField.key = true;
                else
                    return;
            }
            else
                aField.key = false;

            checkFieldName(tables[tblIndex],aField.name);

            aField.isMultiSelect = false;
            aField.multiSelectTable = "";
            aField.formula = "";
            aField.calculateWithSelect = false;
            if (getVariableStack(true) != "")
            {
                if (tableType != "loop")
                {
                    aField.xmlCode = getVariableStack(true) + "/" + variableName;
                    aField.xmlFullPath = getVariableStack(true) + "/" + variableName;
                }
                else
                {
                    aField.xmlCode = variableName;
                    aField.xmlFullPath = variableName;
                }
            }
            else
            {
                aField.xmlCode = variableName;
                aField.xmlFullPath = variableName;
            }
            if (varXMLCode != "")
                aField.xmlCode = varXMLCode;
            QJsonValue labelValue = fieldObject.value("label");
            aField.desc.append(getLabels(labelValue));
            if (values.count() > 0) //If the lookup table has values
            {
                //Creating the lookp table if its neccesary
                if (isLookUpYesNo(values) == false) //Do not process yes/no values as lookup tables
                {
                    TtableDef lkpTable = getDuplicatedLkpTable(values);
                    lkpTable.isLoop = false;
                    lkpTable.isOSM = false;
                    lkpTable.isGroup = false;
                    if (lkpTable.name == "EMPTY")
                    {
                        lkpTable.name = "lkp" + fixField(variableName.toLower());
                        for (int lang = 0; lang < aField.desc.count(); lang++)
                        {
                            TlngLkpDesc fieldDesc;
                            fieldDesc.langCode = aField.desc[lang].langCode;
                            fieldDesc.desc = "Lookup table (" + aField.desc[lang].desc + ")";
                            lkpTable.desc.append(fieldDesc);
                        }
                        lkpTable.pos = -1;
                        lkpTable.islookup = true;
                        lkpTable.isSeparated = false;
                        lkpTable.lkpValues.append(values);
                        //Creates the field for code in the lookup
                        TfieldDef lkpCode;
                        lkpCode.name = fixField(variableName.toLower()) + "_cod";
                        lkpCode.selectSource = "NONE";
                        lkpCode.selectListName = "NONE";
                        for (int lang = 0; lang <= languages.count()-1;lang++)
                        {
                            TlngLkpDesc langDesc;
                            langDesc.langCode = languages[lang].code;
                            langDesc.desc = "Code";
                            lkpCode.desc.append(langDesc);
                        }
                        lkpCode.key = true;
                        lkpCode.type = aField.type;
                        lkpCode.size = aField.size;
                        lkpCode.decSize = aField.decSize;
                        lkpTable.fields.append(lkpCode);
                        //Creates the field for description in the lookup
                        TfieldDef lkpDesc;
                        lkpDesc.name = fixField(variableName.toLower()) + "_des";
                        lkpDesc.selectSource = "NONE";
                        lkpDesc.selectListName = "NONE";
                        for (int lang = 0; lang <= languages.count()-1;lang++)
                        {
                            TlngLkpDesc langDesc;
                            langDesc.langCode = languages[lang].code;
                            langDesc.desc = "Description";
                            lkpDesc.desc.append(langDesc);
                        }
                        lkpDesc.key = false;
                        lkpDesc.type = "varchar";
                        lkpDesc.size = getMaxDescLength(values);
                        lkpDesc.decSize = 0;
                        lkpTable.fields.append(lkpDesc);
                        aField.rTable = lkpTable.name;
                        aField.rField = lkpCode.name;
                        tables.append(lkpTable);
                    }
                    else
                    {
                        if (outputType == "h")
                            log("Lookup table for field " + variableName + " is the same as " + lkpTable.name + ". Using " + lkpTable.name);                        
                        aField.rTable = lkpTable.name;
                        aField.rField = getKeyField(lkpTable.name);                        
                    }
                }
            }
            else
            {
                aField.rTable = "";
                aField.rField = "";
            }
            tables[tblIndex].fields.append(aField);

            //We add here the has other field
            if (selectHasOrOther(variableType))
            {
                TfieldDef oField;
                oField.name = aField.name + "_other";
                oField.selectSource = "NONE";
                oField.selectListName = "NONE";
                oField.xmlCode = aField.xmlCode + "_other";
                oField.decSize = 0;
                for (int lng = 0; lng < languages.count();lng++)
                {
                    TlngLkpDesc fieldDesc;
                    fieldDesc.langCode = languages[lng].code;
                    fieldDesc.desc = "Other";
                    oField.desc.append(fieldDesc);
                }
                oField.isMultiSelect = false;
                aField.formula = "";
                aField.calculateWithSelect = false;
                oField.key = false;
                oField.multiSelectTable = "";
                oField.rField = "";
                oField.rTable = "";
                oField.size = 120;
                oField.type = "varchar";
                tables[tblIndex].fields.append(oField);
            }
        }
        else
        {
            //Creating a multiSelect
            TfieldDef aField;
            aField.name = fixField(variableName.toLower());
            checkFieldName(tables[tblIndex],aField.name);
            aField.selectSource = "NONE";
            aField.selectListName = "NONE";
            aField.type = "varchar";
            aField.size = getMaxMSelValueLength(values);
            aField.decSize = 0;
            aField.key = false;
            aField.isMultiSelect = true;
            aField.formula = "";
            aField.calculateWithSelect = false;
            aField.multiSelectTable = fixField(tables[tblIndex].name) + "_msel_" + fixField(variableName.toLower());
            if (getVariableStack(true) != "")
            {
                if (tableType != "loop")
                {
                    aField.xmlCode = getVariableStack(true) + "/" + variableName;
                    aField.xmlFullPath = getVariableStack(true) + "/" + variableName;
                }
                else
                {
                    aField.xmlCode = variableName;
                    aField.xmlFullPath = variableName;
                }
            }
            else
            {
                aField.xmlCode = variableName;
                aField.xmlFullPath = variableName;
            }
            if (varXMLCode != "")
                aField.xmlCode = varXMLCode;
            QJsonValue labelValue = fieldObject.value("label");
            aField.desc.append(getLabels(labelValue));
            if (values.count() > 0) //If the lookup table has values
            {
                //Creating the multiselect table
                TtableDef mselTable;
                mselTable.isLoop = false;
                mselTable.isOSM = false;
                mselTable.isGroup = false;
                //The multiselect table is a child of the current table thus pass the keyfields
                for (int field = 0; field < tables[tblIndex].fields.count();field++)
                {
                    if (tables[tblIndex].fields[field].key == true)
                    {
                        TfieldDef relField;
                        relField.selectSource = "NONE";
                        relField.selectListName = "NONE";
                        relField.name = tables[tblIndex].fields[field].name;
                        relField.desc.append(tables[tblIndex].fields[field].desc);
                        relField.type = tables[tblIndex].fields[field].type;
                        relField.size = tables[tblIndex].fields[field].size;
                        relField.decSize = tables[tblIndex].fields[field].decSize;
                        relField.key = true;
                        relField.rTable = tables[tblIndex].name;
                        relField.rField = tables[tblIndex].fields[field].name;
                        relField.xmlCode = "NONE";
                        relField.isMultiSelect = false;
                        relField.formula = "";
                        relField.calculateWithSelect = false;
                        relField.multiSelectTable = "";
                        mselTable.fields.append(relField);
                    }
                }
                mselTable.name =  fixField(tables[tblIndex].name) + + "_msel_" + fixField(variableName.toLower());

                for (int lang = 0; lang < aField.desc.count(); lang++)
                {
                    TlngLkpDesc fieldDesc;
                    fieldDesc.langCode = aField.desc[lang].langCode;
                    fieldDesc.desc = "Table for multiple select of field " + variableName + " in table " + tables[tblIndex].name;
                    mselTable.desc.append(fieldDesc);
                }

                mselTable.islookup = false;
                tableIndex++;
                mselTable.pos = tableIndex;
                mselTable.parentTable = tables[tblIndex].name;
                mselTable.xmlCode = "NONE";
                //Creating the extra field to the multiselect table that will be linked to the lookuptable
                TfieldDef mselKeyField;
                mselKeyField.name = aField.name;
                mselKeyField.selectSource = "NONE";
                mselKeyField.selectListName = "NONE";
                mselKeyField.desc.append(aField.desc);
                mselKeyField.key = true;

                if (!selectHasOrOther(variableType))
                {
                    if (areValuesStrings(values))
                        mselKeyField.type = "varchar";
                    else
                        mselKeyField.type = "int";
                }
                else
                    mselKeyField.type = "varchar";

                mselKeyField.size = getMaxValueLength(values);
                mselKeyField.decSize = 0;
                //Processing the lookup table if neccesary
                TtableDef lkpTable = getDuplicatedLkpTable(values);
                lkpTable.isLoop = false;
                lkpTable.isOSM = false;
                lkpTable.isGroup = false;
                if (lkpTable.name == "EMPTY")
                {
                    lkpTable.name = "lkp" + fixField(variableName.toLower());

                    for (int lang = 0; lang < aField.desc.count(); lang++)
                    {
                        TlngLkpDesc fieldDesc;
                        fieldDesc.langCode = aField.desc[lang].langCode;
                        fieldDesc.desc = "Lookup table (" + aField.desc[lang].desc + ")";
                        lkpTable.desc.append(fieldDesc);
                    }
                    lkpTable.pos = -1;
                    lkpTable.islookup = true;
                    lkpTable.isSeparated = false;                    
                    //Creates the field for code in the lookup
                    TfieldDef lkpCode;
                    lkpCode.name = fixField(variableName.toLower()) + "_cod";
                    lkpCode.selectSource = "NONE";
                    lkpCode.selectListName = "NONE";
                    int lang;
                    for (lang = 0; lang <= languages.count()-1;lang++)
                    {
                        TlngLkpDesc langDesc;
                        langDesc.langCode = languages[lang].code;
                        langDesc.desc = "Code";
                        lkpCode.desc.append(langDesc);
                    }
                    lkpCode.key = true;
                    lkpCode.type = mselKeyField.type;
                    lkpCode.size = mselKeyField.size;
                    lkpCode.decSize = mselKeyField.decSize;
                    lkpTable.fields.append(lkpCode);
                    //Creates the field for description in the lookup
                    TfieldDef lkpDesc;
                    lkpDesc.name = fixField(variableName.toLower()) + "_des";
                    lkpDesc.selectSource = "NONE";
                    lkpDesc.selectListName = "NONE";
                    for (lang = 0; lang <= languages.count()-1;lang++)
                    {
                        TlngLkpDesc langDesc;
                        langDesc.langCode = languages[lang].code;
                        langDesc.desc = "Description";
                        lkpDesc.desc.append(langDesc);
                    }

                    lkpDesc.key = false;
                    lkpDesc.type = "varchar";
                    lkpDesc.size = getMaxDescLength(values);
                    lkpDesc.decSize = 0;                    
                    lkpTable.fields.append(lkpDesc);                    
                    //Linking the multiselect field to this lookup table
                    mselKeyField.rTable = lkpTable.name;
                    mselKeyField.rField = lkpCode.name;
                    lkpTable.lkpValues.append(values);
                    tables.append(lkpTable); //Append the lookup table to the list of tables
                }
                else
                {
                    if (outputType == "h")
                        log("Lookup table for field " + variableName + " is the same as " + lkpTable.name + ". Using " + lkpTable.name);
                    //Linkin the multiselect table to the existing lookup table
                    mselKeyField.rTable = lkpTable.name;
                    mselKeyField.rField = getKeyField(lkpTable.name);
                }
                mselTable.fields.append(mselKeyField); //Add the multiselect key now linked to the looktable to the multiselect table
                tables.append(mselTable); //Append the multiselect to the list of tables
            }
            else
            {
                aField.rTable = "";
                aField.rField = "";
                aField.isMultiSelect = false;
                aField.multiSelectTable = "";
                aField.formula = "";
                aField.calculateWithSelect = false;
            }
            tables[tblIndex].fields.append(aField); //Appends the multi select varchar field to the list

            //We add here the has other field
            if (selectHasOrOther(variableType))
            {
                TfieldDef oField;
                oField.name = aField.name + "_other";
                oField.selectSource = "NONE";
                oField.selectListName = "NONE";
                oField.xmlCode = aField.xmlCode + "_other";
                oField.decSize = 0;
                for (int lng = 0; lng < languages.count();lng++)
                {
                    TlngLkpDesc fieldDesc;
                    fieldDesc.langCode = languages[lng].code;
                    fieldDesc.desc = "Other";
                    oField.desc.append(fieldDesc);
                }
                oField.isMultiSelect = false;
                oField.formula = "";
                oField.calculateWithSelect = false;
                oField.key = false;
                oField.multiSelectTable = "";
                oField.rField = "";
                oField.rTable = "";
                oField.size = 120;
                oField.type = "varchar";
                tables[tblIndex].fields.append(oField);
            }
        }
    }
}

// Get the root variables of the survey
void getJSONRootVariables(QJsonObject JSONObject, QList <TsurveyVariableDef> &surveyVariables)
{
    if (JSONObject.keys().indexOf("type") >= 0)
    {
        bool hasChildren = false;
        if (JSONObject.value("type").toString() == "survey")
        {
            hasChildren = true;
        }
        if (JSONObject.value("type").toString() == "repeat")
        {
            hasChildren = true;
        }
        if (JSONObject.value("type").toString() == "group")
        {
            hasChildren = true;
            addToStack(JSONObject.value("name").toString(),"group");
        }
        if (JSONObject.value("type").toString() == "loop")
        {
            hasChildren = true;
        }
        if (!hasChildren)
        {
            QString variableName = JSONObject.value("name").toString();
            QString root = getVariableStack(false);
            if (root != "")
                variableName = root + "/" + variableName;
            TsurveyVariableDef aVariable;
            aVariable.name = variableName;
            root = getVariableStack(true);
            if (root != "")
                variableName = root + "/" + variableName;
            aVariable.fullName = variableName;
            aVariable.type = JSONObject.value("type").toString();
            aVariable.object = JSONObject;
            surveyVariables.append(aVariable);
        }
        else
        {
            if ((JSONObject.value("type").toString() == "group") || (JSONObject.value("type").toString() == "survey"))
            {
                QJsonValue value;
                value = JSONObject.value("children");
                QJsonArray JSONArray = value.toArray();
                for (int nitem = 0; nitem < JSONArray.count(); nitem++)
                {
                    QJsonValue value = JSONArray.at(nitem);
                    if (value.isObject())
                    {
                        getJSONRootVariables(value.toObject(),surveyVariables);
                    }
                }
                removeFromStack();
            }
        }
    }
}

//Coverts group, repeat, loop and osm types into tables
void parseTable(QJsonObject tableObject, QString tableType)
{
    QString tableName;
    tableName = getTopRepeat();
    //Creates the new tables
    tableIndex = tableIndex + 1;
    TtableDef aTable;
    aTable.isLoop = false;
    aTable.isOSM = false;
    aTable.isGroup = false;
    QString variableName;
    variableName = tableObject.value("name").toString();
    aTable.name = fixField(variableName.toLower());
    checkTableName(aTable.name);
    QJsonValue labelValue = tableObject.value("label");
    aTable.desc.append(getLabels(labelValue));
    aTable.pos = tableIndex;
    aTable.islookup = false;
    aTable.isSeparated = false;
    if (getVariableStack(true) == "")
    {
        aTable.xmlCode = variableName;
        aTable.xmlFullPath = variableName;
    }
    else
    {
        aTable.xmlCode = getVariableStack(true) + "/" + variableName;
        aTable.xmlFullPath = getVariableStack(true) + "/" + variableName;
    }
    aTable.parentTable = tableName;
    //Add the father key fields to the table as related fields
    TtableDef parentTable = getTable(tableName);
    for (int field = 0; field < parentTable.fields.count();field++)
    {
        if (parentTable.fields[field].key == true)
        {
            TfieldDef relField;
            relField.name = parentTable.fields[field].name;
            relField.desc.append(parentTable.fields[field].desc);
            relField.type = parentTable.fields[field].type;
            relField.size = parentTable.fields[field].size;
            relField.decSize = parentTable.fields[field].decSize;
            relField.key = true;
            relField.rTable = parentTable.name;
            relField.rField = parentTable.fields[field].name;
            relField.xmlCode = "NONE";
            relField.xmlFullPath = "NONE";
            relField.selectSource = "NONE";
            relField.selectListName = "NONE";
            relField.isMultiSelect = false;
            relField.formula = "";
            relField.calculateWithSelect = false;
            relField.multiSelectTable = "";
            aTable.fields.append(relField);
        }
    }
    if (tableType == "group")
        aTable.isGroup = true;

    if (tableType == "repeat")
    {
        //Add the extra row ID Key field to this table
        TfieldDef keyField;
        keyField.name = fixField(variableName.toLower()) + "_rowid";
        for (int lng = 0; lng < languages.count(); lng++)
        {
            TlngLkpDesc fieldDesc;
            fieldDesc.langCode = languages[lng].code;
            fieldDesc.desc = "Unique Row ID";
            keyField.desc.append(fieldDesc);
        }
        keyField.type = "int";
        keyField.size = 3;
        keyField.decSize = 0;
        keyField.key = true;
        keyField.rTable = "";
        keyField.rField = "";
        keyField.xmlCode = "NONE";
        keyField.xmlFullPath = "NONE";
        keyField.selectSource = "NONE";
        keyField.selectListName = "NONE";
        keyField.isMultiSelect = false;
        keyField.formula = "";
        keyField.calculateWithSelect = false;
        keyField.multiSelectTable = "";
        aTable.fields.append(keyField);
    }
    if (tableType == "loop")
    {
        aTable.isLoop = true;
        QList<TlkpValue> values;
        values.append(getSelectValues(aTable.name,tableObject.value("columns").toArray(),false));
        for (int litem = 0; litem < values.count(); litem++)
        {
            aTable.loopItems.append(values[litem].code);
        }

        // Since the loop items comes from a select. Add add a primary key column to the loop table linked to the lookup os such select
        TfieldDef aField;
        aField.selectSource = "NONE";
        aField.name = aTable.name;
        aField.odktype = "select one";
        aField.selectListName = "NONE";
        aField.calculateWithSelect = false;
        aField.formula = "";
        if (areValuesStrings(values))
            aField.type = "varchar";
        else
            aField.type = "int";
        aField.size = getMaxValueLength(values);
        aField.decSize = 0;
        aField.key = true;
        aField.isMultiSelect = false;
        aField.multiSelectTable = "";
        aField.formula = "";
        aField.calculateWithSelect = false;
        aField.xmlCode = "NONE";
        aField.xmlFullPath = "NONE";
        for (int lang = 0; lang < aField.desc.count(); lang++)
        {
            TlngLkpDesc fieldDesc;
            fieldDesc.langCode = aField.desc[lang].langCode;
            fieldDesc.desc = "Loop elements";
            aField.desc.append(fieldDesc);
        }
        if (values.count() > 0) //If the lookup table has values
        {
            //Creating the lookp table if its neccesary
            if (isLookUpYesNo(values) == false) //Do not process yes/no values as lookup tables
            {
                TtableDef lkpTable = getDuplicatedLkpTable(values);
                lkpTable.isLoop = false;
                lkpTable.isOSM = false;
                lkpTable.isGroup = false;
                if (lkpTable.name == "EMPTY")
                {
                    lkpTable.name = "lkp" + fixField(variableName.toLower());
                    for (int lang = 0; lang < aField.desc.count(); lang++)
                    {
                        TlngLkpDesc fieldDesc;
                        fieldDesc.langCode = aField.desc[lang].langCode;
                        fieldDesc.desc = "Lookup table (" + aField.desc[lang].desc + ")";
                        lkpTable.desc.append(fieldDesc);
                    }
                    lkpTable.pos = -1;
                    lkpTable.islookup = true;
                    lkpTable.isSeparated = false;
                    lkpTable.lkpValues.append(values);
                    //Creates the field for code in the lookup
                    TfieldDef lkpCode;
                    lkpCode.name = fixField(variableName.toLower()) + "_cod";
                    lkpCode.selectSource = "NONE";
                    lkpCode.selectListName = "NONE";
                    for (int lang = 0; lang <= languages.count()-1;lang++)
                    {
                        TlngLkpDesc langDesc;
                        langDesc.langCode = languages[lang].code;
                        langDesc.desc = "Code";
                        lkpCode.desc.append(langDesc);
                    }
                    lkpCode.key = true;
                    lkpCode.type = aField.type;
                    lkpCode.size = aField.size;
                    lkpCode.decSize = aField.decSize;
                    lkpTable.fields.append(lkpCode);
                    //Creates the field for description in the lookup
                    TfieldDef lkpDesc;
                    lkpDesc.name = fixField(variableName.toLower()) + "_des";
                    lkpDesc.selectSource = "NONE";
                    lkpDesc.selectListName = "NONE";
                    for (int lang = 0; lang <= languages.count()-1;lang++)
                    {
                        TlngLkpDesc langDesc;
                        langDesc.langCode = languages[lang].code;
                        langDesc.desc = "Description";
                        lkpDesc.desc.append(langDesc);
                    }
                    lkpDesc.key = false;
                    lkpDesc.type = "varchar";
                    lkpDesc.size = getMaxDescLength(values);
                    lkpDesc.decSize = 0;
                    lkpTable.fields.append(lkpDesc);
                    aField.rTable = lkpTable.name;
                    aField.rField = lkpCode.name;
                    tables.append(lkpTable);
                }
                else
                {
                    if (outputType == "h")
                        log("Lookup table for field " + variableName + " is the same as " + lkpTable.name + ". Using " + lkpTable.name);
                    aField.rTable = lkpTable.name;
                    aField.rField = getKeyField(lkpTable.name);
                }
            }
        }
        else
        {
            aField.rTable = "";
            aField.rField = "";
        }
        aTable.fields.append(aField);
    }
    if (tableType == "osm")
    {
        if (getVariableStack(true) != "")
        {
            aTable.xmlCode = getVariableStack(true) + "/" + variableName;
        }
        else
        {
            aTable.xmlCode = variableName;
        }

        aTable.isOSM = true;
        //Add the extra row ID Key field to this table
        TfieldDef keyField;
        keyField.name = fixField(variableName.toLower()) + "_rowid";
        for (int lng = 0; lng < languages.count(); lng++)
        {
            TlngLkpDesc fieldDesc;
            fieldDesc.langCode = languages[lng].code;
            fieldDesc.desc = "Unique Row ID";
            keyField.desc.append(fieldDesc);
        }
        keyField.type = "int";
        keyField.size = 3;
        keyField.decSize = 0;
        keyField.key = true;
        keyField.rTable = "";
        keyField.rField = "";
        keyField.xmlCode = "NONE";
        keyField.xmlFullPath = "NONE";
        keyField.selectSource = "NONE";
        keyField.selectListName = "NONE";
        keyField.isMultiSelect = false;
        keyField.formula = "";
        keyField.calculateWithSelect = false;
        keyField.multiSelectTable = "";
        aTable.fields.append(keyField);

        //Add the lat field
        TfieldDef f_geopoint_lat;
        f_geopoint_lat.name = "geopoint_lat";
        for (int lang = 0; lang <= languages.count()-1;lang++)
        {
            TlngLkpDesc langDesc;
            langDesc.langCode = languages[lang].code;
            langDesc.desc = "Latitude point";
            f_geopoint_lat.desc.append(langDesc);
        }
        f_geopoint_lat.type = "varchar";
        f_geopoint_lat.size = 40;
        f_geopoint_lat.decSize = 0;
        f_geopoint_lat.rField = "";
        f_geopoint_lat.rTable = "";
        f_geopoint_lat.key = false;
        f_geopoint_lat.isMultiSelect = false;
        f_geopoint_lat.xmlCode = "NONE";
        f_geopoint_lat.odktype = "NONE";
        f_geopoint_lat.calculateWithSelect = false;
        f_geopoint_lat.formula = "";
        f_geopoint_lat.selectSource = "NONE";
        f_geopoint_lat.selectListName = "NONE";
        aTable.fields.append(f_geopoint_lat);

        //Add the lon field
        TfieldDef f_geopoint_lon;
        f_geopoint_lon.name = "geopoint_lon";
        for (int lang = 0; lang <= languages.count()-1;lang++)
        {
            TlngLkpDesc langDesc;
            langDesc.langCode = languages[lang].code;
            langDesc.desc = "Longitude point";
            f_geopoint_lon.desc.append(langDesc);
        }
        f_geopoint_lon.type = "varchar";
        f_geopoint_lon.size = 40;
        f_geopoint_lon.decSize = 0;
        f_geopoint_lon.rField = "";
        f_geopoint_lon.rTable = "";
        f_geopoint_lon.key = false;
        f_geopoint_lon.isMultiSelect = false;
        f_geopoint_lon.xmlCode = "NONE";
        f_geopoint_lon.odktype = "NONE";
        f_geopoint_lon.calculateWithSelect = false;
        f_geopoint_lon.formula = "";
        f_geopoint_lon.selectSource = "NONE";
        f_geopoint_lon.selectListName = "NONE";
        aTable.fields.append(f_geopoint_lon);

        //Parse the tags as columns
        QJsonArray OSMtags;
        OSMtags = tableObject.value("tags").toArray();
        for (int tag = 0; tag < OSMtags.count(); tag++)
        {
            parseOSMField(aTable,OSMtags[tag].toObject());
        }
    }    
    tables.append(aTable);
    if (tableType != "osm")
        addToRepeat(variableName);
}

//Checks whether a group has variables or is just a control group (a group that only contains other groups or repeats)
bool checkIgnoreGroup(QJsonArray children)
{
    bool ignore = true;
    for (int child =0; child < children.count(); child++ )
    {
        if (children[child].isObject())
        {
            QJsonObject a_child = children[child].toObject();
            if ((a_child.value("type").toString("") != "repeat") && (a_child.value("type").toString("") != "group") && (a_child.value("type").toString("") != "loop"))
                ignore = false;
        }
    }
    return ignore;
}

//Reads a JSON object and converts it according to its type
void parseJSONObject(QJsonObject JSONObject, QString mainTable, QString mainField, QDir dir, QSqlDatabase database)
{
//    if (JSONObject.value("name") == "maininfo")
//        qDebug() << JSONObject.value("name");
    if (JSONObject.keys().indexOf("type") >= 0)
    {
        bool hasChildren = false;
        bool isOSM = false;
        bool ignoreGroup = false;
        if (JSONObject.value("type").toString() == "survey")
        {
//            log("*1* Begin processing survey: " + JSONObject.value("name").toString() + "***");
            hasChildren = true;
        }
        if (JSONObject.value("type").toString() == "repeat")
        {
//            log("*2* Begin processing repeat: " + JSONObject.value("name").toString() + "***");
            hasChildren = true;
            parseTable(JSONObject,"repeat");
            addToStack(JSONObject.value("name").toString(),"repeat");

        }
        if (JSONObject.value("type").toString() == "group")
        {
//            log("*3* Begin processing group: " + JSONObject.value("name").toString() + "***");
            hasChildren = true;            
            ignoreGroup = checkIgnoreGroup(JSONObject.value("children").toArray());
            if (!ignoreGroup && includeGroups)
                parseTable(JSONObject,"group");
            addToStack(JSONObject.value("name").toString(),"group");
        }
        if (JSONObject.value("type").toString() == "loop")
        {
//            log("*4* Begin processing loop: " + JSONObject.value("name").toString() + "***");
            hasChildren = true;
            parseTable(JSONObject,"loop");
            addToStack(JSONObject.value("name").toString(),"loop");
        }
        if (JSONObject.value("type").toString() == "osm")
        {
            isOSM = true;
        }
        if (!hasChildren)
        {
            if (!isOSM)
                parseField(JSONObject,mainTable,mainField,dir,database);
            else
            {
                parseTable(JSONObject,"osm");
            }
        }
        else
        {
            QJsonValue value;
            value = JSONObject.value("children");
            QJsonArray JSONArray = value.toArray();
            for (int nitem = 0; nitem < JSONArray.count(); nitem++)
            {
                QJsonValue value = JSONArray.at(nitem);
                if (value.isObject())
                {
                    parseJSONObject(value.toObject(),mainTable,mainField,dir,database);
                }
            }
            if (JSONObject.value("type").toString() == "survey")
            {
//                log("*-1* End processing survey: " + JSONObject.value("name").toString() + "***");
            }
            if (JSONObject.value("type") == "repeat")
            {
                removeRepeat();
                removeFromStack();
//                log("*-2* End processing repeat: " + JSONObject.value("name").toString() + "***");
            }
            if (JSONObject.value("type") == "group")
            {
                if (!ignoreGroup && includeGroups)
                    removeRepeat();
                removeFromStack();
//                log("*-3* End processing group: " + JSONObject.value("name").toString() + "***");
            }
            if (JSONObject.value("type") == "loop")
            {
                removeRepeat();
                removeFromStack();
//                log("*-4* End processing loop: " + JSONObject.value("name").toString() + "***");
            }
        }
    }
}

void getLanguages(QJsonObject JSONObject, QStringList &languageList)
{
    for (int nkey = 0; nkey < JSONObject.keys().count(); nkey++)
    {
        QString key = JSONObject.keys()[nkey];
        if (key != "label")
        {
            QJsonValue value;
            value = JSONObject.value(key);
            if (!value.isArray())
            {
                if (value.isObject())
                    getLanguages(value.toObject(), languageList);
            }
            else
            {
                QJsonArray JSONArray = value.toArray();
                for (int nitem = 0; nitem < JSONArray.count(); nitem++)
                {
                    QJsonValue value = JSONArray.at(nitem);
                    if (value.isObject())
                    {
                        getLanguages(value.toObject(), languageList);
                    }
                }
            }
        }
        else
        {
            QJsonValue value;
            value = JSONObject.value(key);
            if (value.isObject())
            {
                QJsonObject labelObject = value.toObject();
                for (int nitem = 0; nitem < labelObject.keys().count(); nitem++)
                {
                    QString language;
                    language = labelObject.keys()[nitem].toLower();
                    if (language == "default")
                    {
                        if (default_language == "default")
                            language = "english";
                        else
                            language = default_language;
                    }                                        
                    if (languageList.indexOf(language) < 0)
                        languageList.append(language);
                }
            }
        }
    }
}

void reportDuplicatedFields()
{
    QDomDocument XMLResult;
    XMLResult = QDomDocument("XMLResult");
    QDomElement XMLRoot;
    XMLRoot = XMLResult.createElement("XMLResult");
    XMLResult.appendChild(XMLRoot);
    if (outputType != "m")
    {
        log("The following tables have duplicated fields: ");
    }
    for (int pos = 0; pos < duplicatedFields.count(); pos++)
    {
        if (outputType != "m")
        {
            log("\tTable: " + duplicatedFields[pos].table);
        }
        QDomElement eDuplicatedTable;
        eDuplicatedTable = XMLResult.createElement("duplicatedTable");
        eDuplicatedTable.setAttribute("tableName",duplicatedFields[pos].table);
        for (int pos2 = 0; pos2 < duplicatedFields[pos].fields.count(); pos2++)
        {
            if (outputType != "m")
            {
                log("\t\tField: " + duplicatedFields[pos].fields[pos2]);
            }
            QDomElement eDuplicatedField;
            eDuplicatedField = XMLResult.createElement("duplicatedField");
            eDuplicatedField.setAttribute("fieldName",duplicatedFields[pos].fields[pos2]);
            eDuplicatedTable.appendChild(eDuplicatedField);
        }
        XMLRoot.appendChild(eDuplicatedTable);
    }
    if (outputType == "m")
        log(XMLResult.toString());
}

void reportDuplicatedTables()
{
    QDomDocument XMLResult;
    XMLResult = QDomDocument("XMLResult");
    QDomElement XMLRoot;
    XMLRoot = XMLResult.createElement("XMLResult");
    XMLResult.appendChild(XMLRoot);
    if (outputType != "m")
    {
        log("The following tables have the same name: ");
    }
    for (int pos = 0; pos < duplicatedTables.count(); pos++)
    {
        if (outputType != "m")
        {
            log("\tTable: " + duplicatedTables[pos]);
        }
        QDomElement eDuplicatedItem;
        eDuplicatedItem = XMLResult.createElement("duplicatedItem");
        eDuplicatedItem.setAttribute("tableName",duplicatedTables[pos]);
        XMLRoot.appendChild(eDuplicatedItem);
    }
    if (outputType == "m")
        log(XMLResult.toString());
}

void reportSelectDuplicates()
{
    QDomDocument XMLResult;
    XMLResult = QDomDocument("XMLResult");
    QDomElement XMLRoot;
    XMLRoot = XMLResult.createElement("XMLResult");
    XMLResult.appendChild(XMLRoot);
    if (outputType != "m")
    {
        log("The following variables have duplicated options");
    }
    for (int pos = 0; pos <= duplicatedSelectValues.count()-1; pos++)
    {
        if (outputType != "m")
        {
            log("\tVariable: " + duplicatedSelectValues[pos].variableName + " - Option: " + duplicatedSelectValues[pos].selectValue);
        }
        QDomElement eDuplicatedItem;
        eDuplicatedItem = XMLResult.createElement("duplicatedItem");
        eDuplicatedItem.setAttribute("variableName",duplicatedSelectValues[pos].variableName);
        eDuplicatedItem.setAttribute("duplicatedValue",duplicatedSelectValues[pos].selectValue);
        XMLRoot.appendChild(eDuplicatedItem);
    }
    if (outputType == "m")
        log(XMLResult.toString());
}

//Reads the input JSON file and converts it to a MySQL database
int processJSON(QString inputFile, QString mainTable, QString mainField, QDir dir, QSqlDatabase database)
{
    primaryKeyAdded = false;
    QFile JSONFile(inputFile);
    if (!JSONFile.open(QIODevice::ReadOnly))
    {
        log("Cannot open" + inputFile);
        return 1;
    }
    QByteArray JSONData = JSONFile.readAll();
    QJsonDocument JSONDocument;
    JSONDocument = QJsonDocument::fromJson(JSONData);
    QJsonObject firstObject = JSONDocument.object();
    if (!firstObject.isEmpty())
    {
        default_language = firstObject.value("default_language").toString("default");
        //Get the root variables to then check if the primary key exists and is valid
        QList <TsurveyVariableDef> surveyVariables;
        getJSONRootVariables(firstObject, surveyVariables);
        int mainFieldIndex =  -1;
        for (int ivar=0; ivar < surveyVariables.count(); ivar++)
        {
            if (surveyVariables[ivar].name.toLower().simplified().trimmed() == mainField.toLower().simplified().trimmed())
            {
                mainFieldIndex = ivar;
                break;
            }
        }
        if (!justCheck)
        {
            if (mainFieldIndex == -1)
            {
                log("The primary key field does not exists or is inside a repeat");
                exit(10);
            }
        }
        QStringList invalidKeyTypes;
        invalidKeyTypes << "acknowledge";
        invalidKeyTypes << "add acknowledge prompt";
        invalidKeyTypes << "add note prompt";
        invalidKeyTypes << "audio";
        invalidKeyTypes << "audit";
        invalidKeyTypes << "device id";
        invalidKeyTypes << "deviceid";
        invalidKeyTypes << "geoshape";
        invalidKeyTypes << "geotrace";
        invalidKeyTypes << "get device id";
        invalidKeyTypes << "get sim id";
        invalidKeyTypes << "get subscriber id";
        invalidKeyTypes << "hidden";
        invalidKeyTypes << "note";
        invalidKeyTypes << "osm";
        invalidKeyTypes << "photo";
        invalidKeyTypes << "q acknowledge";
        invalidKeyTypes << "q audio";
        invalidKeyTypes << "q geoshape";
        invalidKeyTypes << "q geotrace";
        invalidKeyTypes << "q image";
        invalidKeyTypes << "q note";
        invalidKeyTypes << "q picture";
        invalidKeyTypes << "q video";
        invalidKeyTypes << "range";
        invalidKeyTypes << "rank";
        invalidKeyTypes << "select all that apply";
        invalidKeyTypes << "sim id";
        invalidKeyTypes << "simserial";
        invalidKeyTypes << "subscriber id";
        invalidKeyTypes << "subscriberid";
        invalidKeyTypes << "trigger";
        invalidKeyTypes << "uri:deviceid";
        invalidKeyTypes << "uri:simserial";
        invalidKeyTypes << "uri:subscriberid";
        invalidKeyTypes << "uri:username";
        invalidKeyTypes << "uri:phonenumber";
        invalidKeyTypes << "username";
        invalidKeyTypes << "video";
        invalidKeyTypes << "Xml-external";

        if (!justCheck)
        {
            if (invalidKeyTypes.indexOf(surveyVariables[mainFieldIndex].object.value("type").toString()) >= 0)
            {
                log("The type of the primary key field is invalid");
                exit(18);
            }
        }

        QStringList ODKLanguages;
        getLanguages(firstObject, ODKLanguages);
        //qDebug() << ODKLanguages;
        //Check if we have indicated the proper amount of languages
        if ((ODKLanguages.count() > 1) && (languages.count() == 1))
        {
            if (!justCheck)
            {
                if (outputType == "h")
                    log("This ODK has multiple languages but not other languages where specified with the -l parameter.");
                else
                {
                    QDomDocument XMLResult;
                    XMLResult = QDomDocument("XMLResult");
                    QDomElement XMLRoot;
                    XMLRoot = XMLResult.createElement("XMLResult");
                    XMLResult.appendChild(XMLRoot);
                    QDomElement eLanguages;
                    eLanguages = XMLResult.createElement("languages");
                    for (int pos = 0; pos <= ODKLanguages.count()-1;pos++)
                    {
                        QDomElement eLanguage;
                        eLanguage = XMLResult.createElement("language");
                        QDomText vSepFile;
                        vSepFile = XMLResult.createTextNode(ODKLanguages[pos]);
                        eLanguage.appendChild(vSepFile);
                        eLanguages.appendChild(eLanguage);
                    }
                    XMLRoot.appendChild(eLanguages);
                    log(XMLResult.toString());
                }
                exit(3);
            }
        }
        //Check that each language is properly coded
        bool languageNotFound;
        languageNotFound = false;
        QDomDocument XMLResult;
        XMLResult = QDomDocument("XMLResult");
        QDomElement XMLRoot;
        XMLRoot = XMLResult.createElement("XMLResult");
        XMLResult.appendChild(XMLRoot);
        QDomElement eLanguages;
        eLanguages = XMLResult.createElement("languages");
        for (int lng = 0; lng < ODKLanguages.count();lng++)
            if (genLangIndexByName(ODKLanguages[lng]) == -1)
            {
                if (outputType == "h")
                {
                    languageNotFound = true;
                    if (!justCheck)
                        log("Language " + ODKLanguages[lng] + " was not found in the parameters. Please indicate it as default language (-d) or as other lannguage (-l)");
                }
                else
                {
                    languageNotFound = true;
                    QDomElement eLanguage;
                    eLanguage = XMLResult.createElement("language");
                    QDomText vSepFile;
                    vSepFile = XMLResult.createTextNode(ODKLanguages[lng]);
                    eLanguage.appendChild(vSepFile);
                    eLanguages.appendChild(eLanguage);
                }
            }
        XMLRoot.appendChild(eLanguages);
        if (languageNotFound)
        {
            if (!justCheck)
            {
                if (outputType == "m")
                    log(XMLResult.toString());
                exit(4);
            }
        }
        int lang;
        //Creating the main table
        tableIndex = tableIndex + 1;
        TtableDef maintable;
        maintable.isLoop = false;
        maintable.isOSM = false;
        maintable.isGroup = false;
        maintable.name = fixField(mainTable.toLower());

        for (lang = 0; lang <= languages.count()-1;lang++)
        {
            TlngLkpDesc langDesc;
            langDesc.langCode = languages[lang].code;
            langDesc.desc = "Main table - Set a description";
            maintable.desc.append(langDesc);
        }

        maintable.pos = tableIndex;
        maintable.islookup = false;
        maintable.isSeparated = false;
        maintable.xmlCode = "main";
        maintable.parentTable = "NULL";

        //Append the SurveyID field to the main table
        TfieldDef fsurveyID;
        fsurveyID.name = "surveyid";
        for (lang = 0; lang <= languages.count()-1;lang++)
        {
            TlngLkpDesc langDesc;
            langDesc.langCode = languages[lang].code;
            langDesc.desc = "Submission ID";
            fsurveyID.desc.append(langDesc);
        }
        fsurveyID.type = "varchar";
        fsurveyID.size = 80;
        fsurveyID.decSize = 0;
        fsurveyID.rField = "";
        fsurveyID.rTable = "";
        fsurveyID.key = false;
        fsurveyID.isMultiSelect = false;
        fsurveyID.formula = "";
        fsurveyID.calculateWithSelect = false;
        fsurveyID.xmlCode = "NONE";
        fsurveyID.selectSource = "NONE";
        fsurveyID.selectListName = "NONE";
        maintable.fields.append(fsurveyID);

        //Append the origin ID to the main table
        TfieldDef foriginID;
        foriginID.name = "originid";

        for (lang = 0; lang <= languages.count()-1;lang++)
        {
            TlngLkpDesc langDesc;
            langDesc.langCode = languages[lang].code;
            langDesc.desc = "Origin";
            foriginID.desc.append(langDesc);
        }
        foriginID.type = "varchar";
        foriginID.size = 15;
        foriginID.decSize = 0;
        foriginID.rField = "";
        foriginID.rTable = "";
        foriginID.key = false;
        foriginID.xmlCode = "NONE";
        foriginID.selectSource = "NONE";
        foriginID.selectListName = "NONE";
        foriginID.isMultiSelect = false;
        foriginID.formula = "";
        foriginID.calculateWithSelect = false;
        maintable.fields.append(foriginID);

        //Append the Submmited by field to the main table
        TfieldDef f_submitted_by;
        f_submitted_by.name = "_submitted_by";
        for (lang = 0; lang <= languages.count()-1;lang++)
        {
            TlngLkpDesc langDesc;
            langDesc.langCode = languages[lang].code;
            langDesc.desc = "Submitted by";
            f_submitted_by.desc.append(langDesc);
        }
        f_submitted_by.type = "varchar";
        f_submitted_by.size = 120;
        f_submitted_by.decSize = 0;
        f_submitted_by.rField = "";
        f_submitted_by.rTable = "";
        f_submitted_by.key = false;
        f_submitted_by.isMultiSelect = false;
        f_submitted_by.xmlCode = "_submitted_by";
        f_submitted_by.odktype = "text";
        f_submitted_by.calculateWithSelect = false;
        f_submitted_by.formula = "";
        f_submitted_by.selectSource = "NONE";
        f_submitted_by.selectListName = "NONE";
        maintable.fields.append(f_submitted_by);

        //Append the Submmited date field to the main table
        TfieldDef f_submitted_date;
        f_submitted_date.name = "_submitted_date";
        for (lang = 0; lang <= languages.count()-1;lang++)
        {
            TlngLkpDesc langDesc;
            langDesc.langCode = languages[lang].code;
            langDesc.desc = "Submitted date";
            f_submitted_date.desc.append(langDesc);
        }
        f_submitted_date.type = "datetime";
        f_submitted_date.size = 120;
        f_submitted_date.decSize = 0;
        f_submitted_date.rField = "";
        f_submitted_date.rTable = "";
        f_submitted_date.key = false;
        f_submitted_date.isMultiSelect = false;
        f_submitted_date.xmlCode = "_submitted_date";
        f_submitted_date.odktype = "datetime";
        f_submitted_date.calculateWithSelect = false;
        f_submitted_date.formula = "";
        f_submitted_date.selectSource = "NONE";
        f_submitted_date.selectListName = "NONE";
        maintable.fields.append(f_submitted_date);

        //Append the generic GPS field to the main table
        TfieldDef f_geopoint;
        f_geopoint.name = "_geopoint";
        for (lang = 0; lang <= languages.count()-1;lang++)
        {
            TlngLkpDesc langDesc;
            langDesc.langCode = languages[lang].code;
            langDesc.desc = "GPS point";
            f_geopoint.desc.append(langDesc);
        }
        f_geopoint.type = "varchar";
        f_geopoint.size = 120;
        f_geopoint.decSize = 0;
        f_geopoint.rField = "";
        f_geopoint.rTable = "";
        f_geopoint.key = false;
        f_geopoint.isMultiSelect = false;
        f_geopoint.xmlCode = "_geopoint";
        f_geopoint.odktype = "geopoint";
        f_geopoint.calculateWithSelect = false;
        f_geopoint.formula = "";
        f_geopoint.selectSource = "NONE";
        f_geopoint.selectListName = "NONE";
        maintable.fields.append(f_geopoint);

        tables.append(maintable);
        addToRepeat(mainTable);
        if (!justCheck)
        {
            QString primaryKeyXMLCode = surveyVariables[mainFieldIndex].fullName;
            QJsonObject primaryKeyObject = surveyVariables[mainFieldIndex].object;
            parseField(primaryKeyObject,mainTable, mainField, dir, database,primaryKeyXMLCode);
        }
        else
        {
            QJsonObject testing_object;
            testing_object.insert("type",QJsonValue("text"));
            testing_object.insert("name",QJsonValue("_dummy"));
            if (ODKLanguages.count() == 1)
                testing_object.insert("label",QJsonValue("Dummy"));
            else
            {
                QJsonObject language_object;
                for (int lng = 0; lng < ODKLanguages.count(); lng++)
                {
                    language_object.insert(ODKLanguages[lng],QJsonValue("Dummy"));
                }
                testing_object.insert("label",language_object);
            }
            parseField(testing_object,mainTable, mainField, dir, database,"none");
        }
        primaryKeyAdded = true;        
        parseJSONObject(firstObject, mainTable, mainField, dir, database);       
        if (duplicatedTables.count() > 0)
        {
            reportDuplicatedTables();
            exit(22);
        }
        if (duplicatedFields.count() > 0)
        {
            reportDuplicatedFields();
            exit(23);
        }
        if (duplicatedSelectValues.count() > 0)
        {
            reportSelectDuplicates();
            exit(9);
        }

        for (int itable = 0; itable < tables.count(); itable++)
        {
            if (tables[itable].islookup == false)
            {
                for (int ifield = 0; ifield < tables[itable].fields.count(); ifield++)
                {
                    if (tables[itable].fields[ifield].calculateWithSelect == true)
                    {
                        TfieldDef aField = tables[itable].fields[ifield];
                        getReferenceForSelectAt(aField.formula,aField.type,aField.size,aField.decSize,aField.rTable,aField.rField);
                    }
                }
            }
        }

    }
    return 0;
}

//Return the index of a field in a table using it name
int getFieldIndex(TtableDef table, QString fieldName)
{
    for (int pos = 0; pos <= table.fields.count()-1;pos++)
    {
        if (table.fields[pos].name == fieldName)
            return pos;
    }
    return -1;
}

void moveMultiSelectToTable(QString msTable, QString fromTable, QString toTable)
{
    for (int pos = 0; pos < tables.count(); pos++)
    {
        if (tables[pos].name == msTable)
        {
            for (int clm = 0; clm < tables[pos].fields.count(); clm++)
            {
                if (tables[pos].fields[clm].rTable == fromTable)
                    tables[pos].fields[clm].rTable = toTable;
            }
        }
    }
}

bool separationSectionFound(QList<sepSection> sections, QString name)
{
    for (int pos = 0; pos <= sections.count()-1; pos++)
    {
        if (sections[pos].name == name)
            return true;
    }
    return false;
}

// Separate a table into different subtables.
// This because MySQL cant handle a maximum of 64 reference tables or a row structure of more than 65,535 bytes.

// We put a cap of 60 fiels in a table.
// Basically it is ridiculous to have a table with more the 60 columns and consider it normalized.
// We force the user to think and come with common (normalized) groups of data
int separateTable(TtableDef &table, QDomNode groups)
{
    //log("separateTable");
    QList<TsepSection> sections;
    int pos;
    int pos2;
    QString grpName;
    QString grpDesc;
    QDomNode temp;
    QDomNode field;
    temp = groups;
    //Get all the different sections
    bool mainFound;
    mainFound = false;

    while (!temp.isNull())
    {
        grpName = temp.toElement().attribute("name","unknown");
        grpDesc = temp.toElement().attribute("description","unknown");
        if (grpName == "main")
            mainFound = true;        
        if (separationSectionFound(sections,grpName) == false)
        {
            TsepSection aSection;
            aSection.name = grpName;
            aSection.desc = grpDesc;
            sections.append(aSection);
        }
            else
        {
            log("ERROR!: " + grpName + " is repated. Cannot continue with separation");
            return 1;
        }
        temp = temp.nextSibling();
    }
    if (!mainFound)
    {
        log("ERROR!: Main group for table " + table.name + " is not present. Cannot continue");
        return 1;
    }
    temp = groups;
    int fieldIndex;
    QString fieldName;
    QList <TfieldDef> keys;
    //Add the keys of a table to a list
    for (pos2 = 0; pos2 <= table.fields.count()-1;pos2++)
    {
        if (table.fields[pos2].key == true)
        {
            TfieldDef keyfield;
            keyfield.name = table.fields[pos2].name;
            keyfield.selectSource = "NONE";
            keyfield.selectListName = "NONE";
            keyfield.desc.append(table.fields[pos2].desc);
            keyfield.key =true;
            keyfield.type = table.fields[pos2].type;
            keyfield.size = table.fields[pos2].size;
            keyfield.decSize = table.fields[pos2].decSize;
            keyfield.rTable = "";
            keyfield.rField = "";
            keys.append(keyfield);
        }
    }
    for (pos = 0; pos <= sections.count()-1;pos++)
    {
        if (sections[pos].name != "main")
        {
            tableIndex++;
            TtableDef ntable;
            ntable.isLoop = false;
            ntable.isOSM = false;
            ntable.isGroup = false;
            ntable.name = table.name + "_" + sections[pos].name;

            for (int lang = 0; lang < languages.count()-1; lang++)
            {
                TlngLkpDesc langDesc;
                langDesc.langCode = languages[lang].code;
                if (sections[pos].desc == "unknown")
                    langDesc.desc = "Subsection " + sections[pos].name + " of " + table.name + " - Edit this description";
                else
                    langDesc.desc = sections[pos].desc;
                ntable.desc.append(langDesc);
            }
            ntable.xmlCode = table.xmlCode;
            ntable.pos = tableIndex;
            ntable.islookup = false;
            ntable.isSeparated = true;
            ntable.parentTable = table.name;
            for (pos2 = 0; pos2 <= keys.count()-1;pos2++)
            {
                TfieldDef keyfield;
                keyfield = keys[pos2];
                //A secondary group will be linked to the main
                keyfield.selectSource = "NONE";
                keyfield.selectListName = "NONE";
                keyfield.isMultiSelect = false;
                keyfield.rTable = table.name;
                keyfield.rField = keys[pos2].name;
                ntable.fields.append(keyfield);
            }

            //Then add the fields of the section
            field = temp.firstChild();
            while (!field.isNull())
            {
                fieldName = field.toElement().attribute("name","unknown");
                fieldIndex = getFieldIndex(table,fieldName);
                if (fieldIndex >= 0)
                {
                    if (table.fields[fieldIndex].key == false)
                    {
                        ntable.fields.append(table.fields[fieldIndex]); //Add the field to the new table
                        //If the field that we are moving is a isMultiSelect then that multiselect table needs to link this new table
                        if (table.fields[fieldIndex].isMultiSelect == true)
                        {
                            moveMultiSelectToTable(table.fields[fieldIndex].multiSelectTable,table.name,ntable.name);
                        }
                        table.fields.removeAt(fieldIndex); //Removes the field from main
                    }
                }
                field = field.nextSibling();
            }
            tables.append(ntable);
        }
        temp = temp.nextSibling();
    }

    //We move here all multi-select tables to be last in position thus no referencia problem would occur;
    int maxTableCount;
    maxTableCount = tables.count() + 1;
    for (pos = 0; pos < tables.count();pos++)
    {
        if (tables[pos].name.indexOf("_msel_") >= 0)
        {
            tables[pos].pos = maxTableCount;
            maxTableCount++;
        }
    }

    return 0;
}

//Checks which table has more than 60 references or more than 100 fields. Those with more than 60 or 100 fields gets separated
int separeteTables(QString sepFile)
{    
    QDomDocument doc("mydocument");
    QFile xmlfile(sepFile);
    if (!xmlfile.open(QIODevice::ReadOnly))
    {
        log("Error reading separation file");
        return 1;
    }
    if (!doc.setContent(&xmlfile))
    {
        log("Error reading separation file");
        xmlfile.close();
        return 1;
    }
    xmlfile.close();
    QDomNode table;
    QDomNode groups;
    QString tableName;
    table = doc.firstChild().nextSibling().firstChild();
    int tblIndex;
    while (!table.isNull())
    {
        tableName = table.toElement().attribute("name","!unknow");
        tblIndex = getTableIndex(tableName);
        if (tblIndex >= 0)
        {
            groups = table.firstChild();
            //log("Separating table: " + tables[tblIndex].name);
            if (separateTable(tables[tblIndex],groups) == 1)
            {
                return 1;
            }            
        }
        table = table.nextSibling();
    }
    return 0;
}

// This function construct a XML <table> node with all fields in one group called allfields.
// the node then is appended to the XML Separation file
QDomElement getTableXML(QDomDocument doc, TtableDef table)
{
    QDomElement tnode;
    QDomElement gnode;
    tnode = doc.createElement("table");
    tnode.setAttribute("name",table.name);
    gnode = doc.createElement("group");
    gnode.setAttribute("name","main");
    gnode.setAttribute("description",fixString(getDescForLanguage(table.desc,getLanguageCode(getDefLanguage()))));
    bool notmove;
    for (int pos = 0; pos <= table.fields.count()-1;pos++)
    {
        notmove = false;
        if (table.fields[pos].key == false)
        {
            if ((table.fields[pos].name == "surveyid") || (table.fields[pos].name == "originid") || (table.fields[pos].name == "rowuuid") || (table.fields[pos].name == "_submitted_by") || (table.fields[pos].name == "_submitted_date") || (table.fields[pos].name == "_geopoint"))
                notmove = true;
            if (table.fields[pos].key == true)
                notmove = true;
            QDomElement fnode;
            fnode = doc.createElement("field");
            fnode.setAttribute("name",table.fields[pos].name);
            fnode.setAttribute("xmlcode",table.fields[pos].xmlCode);
            fnode.setAttribute("description",fixString(getDescForLanguage(table.fields[pos].desc,getLanguageCode(getDefLanguage()))));
            if (notmove)
                fnode.setAttribute("notmove","true");
            else
                fnode.setAttribute("notmove","false");
            gnode.appendChild(fnode);
        }
    }
    tnode.appendChild(gnode);
    return tnode;
}

// This function check the tables. If the table has more than 60 relationships creates a separation file using UUID as file name
// The separation file then can be used as an input parameter to separate the tables into sections
int checkTables(QString sepOutFile)
{
    int pos;
    int pos2;
    int rfcount;
    int tmax;
    tmax = tables.count();
    bool error;
    error = false;
    QDomDocument outputdoc;
    outputdoc = QDomDocument("ODKSeparationFile");
    QDomElement root;
    root = outputdoc.createElement("ODKSeparationXML");
    root.setAttribute("version", "1.0");
    outputdoc.appendChild(root);
    int count;
    count = 0;
    int fcount;
    for (pos = 0; pos <= tmax-1;pos++)
    {
        rfcount = 0;
        fcount = 0;
        for (pos2 = 0; pos2 <= tables[pos].fields.count()-1;pos2++)
        {
            if (!tables[pos].fields[pos2].rTable.isEmpty())
                rfcount++;
            fcount++;
        }
        if ((rfcount > 60) || (fcount >= 100))
        {
            root.appendChild(getTableXML(outputdoc,tables[pos]));
            count++;
            error = true;
        }
    }
    if (error)
    {
        QUuid id;
        id = QUuid::createUuid();
        QString fname;
        fname = id.toString();
        fname = fname.replace("{","");
        fname = fname.replace("}","");
        fname = fname.right(12);
        QFile file;
        if (sepOutFile == "")
            file.setFileName(fname + ".xml");
        else
            file.setFileName(sepOutFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&file);
            out.setCodec("UTF-8");
            outputdoc.save(out,1,QDomNode::EncodingFromTextStream);
            file.close();
        }
        else
        {
            log("Cannot create separation file");
            return 1;
        }

        QString sepFileName;
        if (sepOutFile == "")
        {
            QDir dir(".");
            sepFileName = dir.absolutePath() + dir.separator() + fname + ".xml";
        }
        else
        {
            QDir dir;
            sepFileName = dir.absoluteFilePath(sepOutFile);
        }
        if (outputType == "h")
            log("The separation file: " + sepFileName + " has been created use. Edit it and use it as an input.");
        else
        {
            QDomDocument XMLResult;
            XMLResult = QDomDocument("XMLResult");
            QDomElement XMLRoot;
            XMLRoot = XMLResult.createElement("XMLResult");
            XMLResult.appendChild(XMLRoot);
            QDomElement eSepFile;
            eSepFile = XMLResult.createElement("sepfile");
            QDomText vSepFile;
            vSepFile = XMLResult.createTextNode(sepFileName);
            eSepFile.appendChild(vSepFile);
            XMLRoot.appendChild(eSepFile);
            log(XMLResult.toString());
        }
        exit(2);
    }
    else
        return 0;
}

//Returns the index of a language by its code
int genLangIndex(QString langCode)
{    
    for (int pos=0; pos < languages.count();pos++)
    {
        if (languages[pos].code == langCode)
            return pos;
    }
    return -1;
}

//Adds a language to the list and check if the each language structure is ok
int addLanguage(QString langCode, bool defLang)
{
    QRegExp reEngLetterOnly("\\(([a-zA-Z]{2})\\)(.*)");
    if (reEngLetterOnly.indexIn(langCode) == -1)
    {
        log("Malformed language code " + langCode + ". Indicate a language like (iso639 Code)Language_Name. For example (en)English");
        return 1;
    }
    QString code = reEngLetterOnly.cap(1);
    if (genLangIndex(code) == -1)
    {
        QString name = reEngLetterOnly.cap(2);
        name = name;
        TlangDef language;
        language.code = code.toLower();
        language.desc = name.toLower();
        language.deflang = defLang;        
        languages.append(language);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * JSON XForm To MySQL                                               * \n";
    title = title + " * This tool generates a MySQL schema from a PyXForm JSON file.      * \n";
    title = title + " * JXFormToMySQL generates full relational MySQL databases.          * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "2.0");

    TCLAP::ValueArg<std::string> inputArg("j","inputJSON","Input PyXForm JSON survey file",true,"","string");
    TCLAP::ValueArg<std::string> tableArg("t","mainTable","Name of the master table for the target schema. ODK surveys do not have a master table however this is neccesary to store ODK variables that are not inside a repeat. Please give a name for the master table for maintable, mainmodule, coverinformation, etc.",true,"","string");
    TCLAP::ValueArg<std::string> mainVarArg("v","mainVariable","Code of the main variable of the ODK survey. For example HH_ID",true,"","string");
    TCLAP::ValueArg<std::string> ddlArg("c","outputdml","Output DDL file. Default ./create.sql",false,"./create.sql","string");
    TCLAP::ValueArg<std::string> XMLCreateArg("C","xmlschema","Output XML schema file. Default ./create.xml",false,"./create.xml","string");
    TCLAP::ValueArg<std::string> insertArg("i","outputinsert","Output insert file. Default ./insert.sql",false,"./insert.sql","string");
    TCLAP::ValueArg<std::string> dropArg("D","outputdrop","Output drop table file. Default ./drop.sql",false,"./drop.sql","string");
    TCLAP::ValueArg<std::string> insertXMLArg("I","xmlinsert","Output lookup values in XML format. Default ./insert.xml",false,"./insert.xml","string");
    TCLAP::ValueArg<std::string> metadataArg("m","outputmetadata","Output metadata file. Default ./metadata.sql",false,"./metadata.sql","string");
    TCLAP::ValueArg<std::string> impxmlArg("f","outputxml","Output xml manifest file. Default ./manifest.xml",false,"./manifest.xml","string");
    TCLAP::ValueArg<std::string> prefixArg("p","prefix","Prefix for each table. _ is added to the prefix. Default no prefix",false,"","string");
    TCLAP::ValueArg<std::string> sepArg("s","separationfile","Separation file to use",false,"","string");
    TCLAP::ValueArg<std::string> sepOutputArg("S","separationoutputfile","Separation file to writen if required. By default a auto generated file will be created",false,"","string");
    TCLAP::ValueArg<std::string> langArg("l","otherlanguages","Other languages. For example: (en)English,(es)Español. Required if ODK form has multiple languages",false,"","string");
    TCLAP::ValueArg<std::string> defLangArg("d","deflanguage","Default language. For example: (en)English. If not indicated then English will be asumed",false,"(en)English","string");
    TCLAP::ValueArg<std::string> transFileArg("T","translationfile","Output translation file",false,"./iso639.sql","string");
    TCLAP::ValueArg<std::string> yesNoStringArg("y","yesnostring","Yes and No strings in the default language in the format \"String|String\". This will allow the tool to identify Yes/No lookup tables and exclude them. This is not case sensitive. For example, if the default language is Spanish this value should be indicated as \"Si|No\". If empty English \"Yes|No\" will be assumed",false,"Yes|No","string");
    TCLAP::ValueArg<std::string> tempDirArg("e","tempdirectory","Temporary directory. ./tmp by default",false,"./tmp","string");
    TCLAP::ValueArg<std::string> outputTypeArg("o","outputtype","Output type: (h)uman or (m)achine readble. Machine readble by default",false,"m","string");
    TCLAP::SwitchArg groupsSwitch("G","includegroups","Include groups as tables", cmd, false);
    TCLAP::SwitchArg justCheckSwitch("K","justCheck","Just check of main inconsistencies and report back", cmd, false);
    TCLAP::UnlabeledMultiArg<std::string> suppFiles("supportFile", "support files", false, "string");

    debug = false;

    for (int i = 1; i < argc; i++)
    {
        command = command + argv[i] + " ";
    }

    cmd.add(inputArg);
    cmd.add(XMLCreateArg);
    cmd.add(tableArg);
    cmd.add(mainVarArg);
    cmd.add(ddlArg);
    cmd.add(insertArg);
    cmd.add(dropArg);
    cmd.add(insertXMLArg);
    cmd.add(metadataArg);
    cmd.add(impxmlArg);
    cmd.add(prefixArg);
    cmd.add(sepArg);
    cmd.add(langArg);
    cmd.add(defLangArg);
    cmd.add(transFileArg);
    cmd.add(yesNoStringArg);
    cmd.add(tempDirArg);
    cmd.add(outputTypeArg);
    cmd.add(sepOutputArg);
    cmd.add(suppFiles);

    //Parsing the command lines
    cmd.parse( argc, argv );

    //Get the support files
    std::vector<std::string> v = suppFiles.getValue();
    for (int i = 0; static_cast<unsigned int>(i) < v.size(); i++)
    {
        QString file = QString::fromStdString(v[i]);
        if (QFile::exists(file))
            supportFiles.append(file);
        else
        {
            log("Support file \"" + file + "\" does not exist.");
            return 1;
        }
    }

    includeGroups = groupsSwitch.getValue();
    justCheck = justCheckSwitch.getValue();
    //Getting the variables from the command
    QString input = QString::fromUtf8(inputArg.getValue().c_str());
    QString ddl = QString::fromUtf8(ddlArg.getValue().c_str());
    QString insert = QString::fromUtf8(insertArg.getValue().c_str());
    QString drop = QString::fromUtf8(dropArg.getValue().c_str());
    QString insertXML = QString::fromUtf8(insertXMLArg.getValue().c_str());
    QString metadata = QString::fromUtf8(metadataArg.getValue().c_str());
    QString xmlFile = QString::fromUtf8(impxmlArg.getValue().c_str());
    QString xmlCreateFile = QString::fromUtf8(XMLCreateArg.getValue().c_str());
    QString mTable = QString::fromUtf8(tableArg.getValue().c_str());
    QString mainVar = QString::fromUtf8(mainVarArg.getValue().c_str());
    QString sepFile = QString::fromUtf8(sepArg.getValue().c_str());
    QString sepOutFile = QString::fromUtf8(sepOutputArg.getValue().c_str());
    QString lang = QString::fromUtf8(langArg.getValue().c_str());
    QString defLang = QString::fromUtf8(defLangArg.getValue().c_str());
    QString transFile = QString::fromUtf8(transFileArg.getValue().c_str());
    QString yesNoString = QString::fromUtf8(yesNoStringArg.getValue().c_str());
    QString tempDirectory = QString::fromUtf8(tempDirArg.getValue().c_str());
    outputType = QString::fromUtf8(outputTypeArg.getValue().c_str());
    mainVar = mainVar.trimmed().toLower();
    QDir currdir(".");
    QDir dir;
    if (!dir.exists(tempDirectory))
    {
        if (!dir.mkdir(tempDirectory))
        {
            log("Cannot create temporary directory");
            return 1;
        }
    }
    else
    {
        dir.setPath(tempDirectory);
        if (currdir.absolutePath() == dir.absolutePath())
        {
            log("Temporary directory cannot be the same as this path");
            return 1;
        }
        if (!dir.removeRecursively())
        {
            log("Cannot remove existing temporary directory");
            return 1;
        }
        QDir dir2;
        if (!dir2.mkdir(tempDirectory))
        {
            log("Cannot create temporary directory");
            return 1;
        }
    }
    dir.setPath(tempDirectory);

    //Unzip any zip files in the temporary directory
    QStringList zipFiles;
    for (int pos = 0; pos <= supportFiles.count()-1; pos++)
    {        
        QFile file(supportFiles[pos]);
        if (file.open(QIODevice::ReadOnly))
        {
            QDataStream in(&file);
            quint32 header;
            in >> header;
            file.close();
            if (header == 0x504b0304)
            {
                QuaZip zip(supportFiles[pos]);
                if (!zip.open(QuaZip::mdUnzip))
                {
                    log("Cannot open zip file:" + supportFiles[pos]);
                    return 1;
                }
                QuaZipFile file(&zip);
                QString zipFileName;
                QFile file2;
                for(bool more=zip.goToFirstFile(); more; more=zip.goToNextFile()) {

                    zipFileName = zip.getCurrentFileName();
                    if (zipFileName.right(1) == "/")
                    {
                        dir.mkpath(zipFileName.left(zipFileName.length()-1));
                    }
                    else
                    {
                        file.open(QIODevice::ReadOnly);
                        file2.setFileName(dir.absolutePath() + dir.separator() + zipFileName);
                        file2.open(QIODevice::WriteOnly);
                        QByteArray fileData;
                        fileData = file.readAll();
                        file2.write(fileData);
                        file.close(); // do not forget to close!
                        file2.close();
                    }
                }
                //gbtLog(QObject::tr("Closing file"));
                zip.close();
                zipFiles.append(supportFiles[pos]);
            }
        }
    }
    //Remove any zip files from the list of support files
    for (int pos =0; pos <= zipFiles.count()-1; pos++)
    {
        int idx;
        idx = supportFiles.indexOf(zipFiles[pos]);
        if (idx >= 0)
        {
            supportFiles.removeAt(idx);
        }
    }
    //Add all files in the temporary directory as support files
    QDirIterator it(dir.absolutePath(), QStringList() << "*.*", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        supportFiles.append(it.next());

    QSqlDatabase dblite = QSqlDatabase::addDatabase("QSQLITE","DBLite");
    int CSVError;
    CSVError = 0;
    for (int pos = 0; pos <= supportFiles.count()-1;pos++)
    {
        if (supportFiles[pos].right(3).toLower() == "csv")
        {
            CSVError = convertCSVToSQLite(supportFiles[pos],dir,dblite);
            if (CSVError != 0)
                return CSVError;
        }
        if (supportFiles[pos].right(3).toLower() == "xml")
        {
            QFileInfo supportFile(supportFiles[pos]);
            QFile::copy(supportFiles[pos],dir.absolutePath() + QDir::separator() + supportFile.fileName());
        }
    }


    prefix = QString::fromUtf8(prefixArg.getValue().c_str());
    prefix = prefix + "_";

    strYes = "Yes";
    strNo = "No";

    if (prefix == "_")
        prefix = "";

    tableIndex = 0;

    if (addLanguage(defLang.replace("'",""),true) != 0)
        return 1;
    
    QStringList othLanguages;
    othLanguages = lang.split(",",QString::SkipEmptyParts);
    for (int lng = 0; lng < othLanguages.count(); lng++)
        if (addLanguage(othLanguages[lng].replace("'",""),false) != 0)
            exit(6);
    
    if (isDefaultLanguage("english") == false)
    {
        if (yesNoString == "")
        {
            log("English is not the default language. You need to specify Yes and No values with the -y parameter in " + getDefLanguage());
            exit(7);
        }
        if (yesNoString.indexOf("|") == -1)
        {
            log("Malformed yes|no language string1");
            exit(8);
        }
        if (yesNoString.length() == 1)
        {
            log("Malformed yes|no language string2");
            exit(8);
        }
        strYes = yesNoString.split("|",QString::SkipEmptyParts)[0];
        strNo = yesNoString.split("|",QString::SkipEmptyParts)[1];
    }
    else
    {
        strYes = yesNoString.split("|",QString::SkipEmptyParts)[0];
        strNo = yesNoString.split("|",QString::SkipEmptyParts)[1];
    }

    int returnValue;
    returnValue = processJSON(input,mTable.trimmed(),mainVar.trimmed(),dir,dblite);
    if (returnValue == 0)
    {
        //dumpTablesForDebug();
        if (sepFile != "") //If we have a separation file
        {
            //log("Separating tables");
            QFile file(sepFile);
            if (file.exists())
            {
                if (separeteTables(sepFile) == 1) //Separate the tables using the file
                    return 1;
            }
        }
        appendUUIDs();
        //log("Checking tables");
        if (!justCheck)
        {
            if (checkTables(sepOutFile) != 0) //Check the tables to see if they have less than 60 related fields. If so a separation file is created
            {
                exit(2); //If a table has more than 60 related field then exit
            }
        }

        //log("Generating output files");
        if (!justCheck)
            generateOutputFiles(ddl,insert,metadata,xmlFile,transFile,xmlCreateFile,insertXML,drop);
    }
    else
        return returnValue;
    if (outputType == "h")
        log("Done without errors");
    if (justCheck)
    {
        if (requiredFiles.count() > 0)
        {
            if (outputType == "h")
            {
                log("Required files:" + requiredFiles.join(","));
            }
            else
            {
                QDomDocument XMLResult;
                XMLResult = QDomDocument("XMLResult");
                QDomElement XMLRoot;
                XMLRoot = XMLResult.createElement("XMLResult");
                XMLResult.appendChild(XMLRoot);
                for (int item = 0; item < requiredFiles.count(); item++)
                {
                    QDomElement eDuplicatedItem;
                    eDuplicatedItem = XMLResult.createElement("missingFile");
                    eDuplicatedItem.setAttribute("fileName",requiredFiles[item]);
                    XMLRoot.appendChild(eDuplicatedItem);
                }
                log(XMLResult.toString());
            }
        }
    }
    return 0;
}
