/*
ODKToMySQL

Copyright (C) 2015-2017 International Livestock Research Institute.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

ODKToMySQL is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

ODKToMySQL is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with ODKToMySQL.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <tclap/CmdLine.h>
#include <QtXml>
#include <QFile>
#include <QDir>
#include "xlsxdocument.h"
#include "xlsxworkbook.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"
#include "xlsxcellreference.h"
#include "xlsxcell.h"
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

bool debug;
QString command;
QString outputType;

//This logs messages to the terminal. We use printf because qDebug does not log in relase
void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf(temp.toUtf8().data());
}

int CSVRowNumber;
bool CSVColumError;
QStringList CSVvalues;
QStringList CSVSQLs;

void cb1 (void *s, size_t, void *)
{
    char* charData;
    charData = (char*)s;
    CSVvalues.append(QString::fromUtf8(charData));
}

QString fixColumnName(QString column)
{
    QString res;
    res = column;
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

int numColumns;
int numColumnsInData;
void cb2 (int , void *)
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
        return 14;
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

QString strYes; //Yes string for comparing Yes/No lookup tables
QString strNo; //No string for comparing Yes/No lookup tables
QStringList variableStack; //This is a stack of groups or repeats for a variable. Used to get /xxx/xxx/xxx structures
QStringList repeatStack; //This is a stack of repeats. So we know in which repeat we are
QString prefix; //Table prefix
int tableIndex; //Global index of a table. Used later on to sort them
QStringList supportFiles;

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
  QString rTable; //Related table
  QString rField; //Related field
  bool key; //Whether the field is key
  QString xmlCode; //The field XML code /xx/xx/xx/xx
  bool isMultiSelect; //Whether the field if multiselect
  QString multiSelectTable; //Multiselect table
  QString selectSource; //The source of the select. Internal, External or Search
  QString selectListName; //The list name of the select
};
typedef fieldDef TfieldDef;

//Language structure
struct langDef
{
  QString code;
  QString desc;
  bool deflang; //Wether the language is default
  int idxInSurvey; //Column index in the survey sheet
  int idxInChoices; //Column index in the choices sheet
};
typedef langDef TlangDef;

//List of languages
QList <TlangDef> languages;

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
  QString parentTable; //The parent of the table
  QDomElement tableElement; //Each table is an Dom Element for building the manifest XML file
  QDomElement tableCreteElement; //Each table is a second Dom Element for building the XML Create file
};
typedef tableDef TtableDef;

QList<TtableDef> tables; //List of tables

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
void genSQL(QString ddlFile,QString insFile, QString metaFile, QString xmlFile, QString transFile, QString XMLCreate, QString insertXML, QString dropSQL)
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

//    QFile UUIDTriggersFile(UUIDFile);
//    if (!UUIDTriggersFile.open(QIODevice::WriteOnly | QIODevice::Text))
//             return;
//    QTextStream UUIDStrm(&UUIDTriggersFile);
//    UUIDStrm.setCodec("UTF-8");

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

    sqlCreateStrm << "-- Code generated by ODKToMySQL" << "\n";
    sqlCreateStrm << "-- " + command << "\n";
    sqlCreateStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    sqlCreateStrm << "-- by: ODKToMySQL Version 1.0" << "\n";
    sqlCreateStrm << "-- WARNING! All changes made in this file might be lost when running ODKToMySQL again" << "\n\n";

    sqlDropStrm << "-- Code generated by ODKToMySQL" << "\n";
    sqlDropStrm << "-- " + command << "\n";
    sqlDropStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    sqlDropStrm << "-- by: ODKToMySQL Version 1.0" << "\n";
    sqlDropStrm << "-- WARNING! All changes made in this file might be lost when running ODKToMySQL again" << "\n\n";

    sqlInsertStrm << "-- Code generated by ODKToMySQL" << "\n";
    sqlInsertStrm << "-- " + command << "\n";
    sqlInsertStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    sqlInsertStrm << "-- by: ODKToMySQL Version 1.0" << "\n";
    sqlInsertStrm << "-- WARNING! All changes made in this file might be lost when running ODKToMySQL again" << "\n\n";
    sqlInsertStrm << "START TRANSACTION;" << "\n\n";

    iso639Strm << "-- Code generated by ODKToMySQL" << "\n";
    iso639Strm << "-- " + command << "\n";
    iso639Strm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    iso639Strm << "-- by: ODKToMySQL Version 1.0" << "\n";
    iso639Strm << "-- WARNING! All changes made in this file might be lost when running ODKToMySQL again" << "\n\n";

//    UUIDStrm << "-- Code generated by ODKToMySQL" << "\n";
//    UUIDStrm << "-- " + command << "\n";
//    UUIDStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
//    UUIDStrm << "-- by: ODKToMySQL Version 1.0" << "\n";
//    UUIDStrm << "-- WARNING! All changes made in this file might be lost when running ODKToMySQL again" << "\n\n";

    sqlUpdateStrm << "-- Code generated by ODKToMySQL" << "\n";
    sqlUpdateStrm << "-- " + command << "\n";
    sqlUpdateStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    sqlUpdateStrm << "-- by: ODKToMySQL Version 1.0" << "\n";
    sqlUpdateStrm << "-- WARNING! All changes made in this file might be lost when running ODKToMySQL again" << "\n\n";

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
    result.size = 60;
    result.decSize = 0;
    if (ODKFieldType == "start")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "today")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "end")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "deviceid")
    {
        result.type = "varchar";
        result.size = 60;
    }
    if (ODKFieldType == "subscriberid")
    {
        result.type = "varchar";
        result.size = 60;
    }
    if (ODKFieldType == "simserial")
    {
        result.type = "varchar";
        result.size = 60;
    }
    if (ODKFieldType == "phonenumber")
    {
        result.type = "varchar";
        result.size = 60;
    }
    if (ODKFieldType == "note")
    {
        result.type = "text";
    }
    if (ODKFieldType == "text")
    {
        result.type = "text";
    }
    if (ODKFieldType == "decimal")
    {
        result.type = "decimal";
        result.size = 10;
        result.decSize = 3;
    }
    if (ODKFieldType == "integer")
    {
        result.type = "int";
        result.size = 9;
    }        
    if (ODKFieldType == "date")
    {
        result.type = "date";
    }
    if (ODKFieldType == "time")
    {
        result.type = "time";
    }
    if (ODKFieldType == "datetime")
    {
        result.type = "datetime";
    }
    if (ODKFieldType == "geopoint")
    {
        result.type = "varchar";
        result.size = 60;
    }
    if (ODKFieldType == "calculate")
    {
        result.type = "varchar";
        result.size = 255;
    }
    return result; //Otherwise treat it as varchar
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
        for (lng = 0; lng < languages.count();lng++)
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
void addToStack(QString groupOrRepeat)
{
    variableStack.append(groupOrRepeat);
}

//Fixes table and field name by removing invalid MySQL characters
QString fixField(QString source)
{
    QString res;
    res = source;
    res = res.replace("'","");
    res = res.replace('\"',"");
    res = res.replace(";","");
    res = res.replace("-","_");
    res = res.replace(",","");
    res = res.replace(" ","");
    res = res.replace(".","_");
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
QString getVariableStack()
{
    QString res;
    for (int pos = 0; pos < variableStack.count();pos++)
    {
        res = res + variableStack[pos] + "/";
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
    if (variableType.toLower().trimmed().indexOf("or_other") >= 0)
        return true;
    else
        return false;
}

//This return the values of a select from an external CSV.
QList<TlkpValue> getSelectValuesFromCSV(QString searchExpresion, QXlsx::Worksheet *choicesSheet,QString listName,int listNameIdx,int nameIdx, bool hasOrOther, int &result, QDir dir, QSqlDatabase database)
{
    QList<TlkpValue> res;
    QXlsx::CellReference ref;
    QXlsx::Cell *cell;
    QXlsx::Cell *cell2;

    // First we get column for code and the different columns for descriptions
    // like for select_one household_region   search('regions', 'matches', 'cnty_cod', ${hh_country}) :
    // list_name            name        label::English      label::Spanish
    // household_region     reg_cod     reg_name_en         reg_name_es

    // codeColum is "reg_cod" and the descColumns are "reg_name_en" and "reg_name_es"
    QString codeColumn;
    codeColumn = "";
    QList<TlngLkpDesc> descColumns;
    for (int nrow = 2; nrow <= choicesSheet->dimension().lastRow(); nrow++)
    {
        ref.setRow(nrow);
        ref.setColumn(listNameIdx);
        cell = choicesSheet->cellAt(ref);
        if (cell != 0)
        {
            if (cell->value().toString().toLower().trimmed() == listName.toLower().trimmed())
            {
                ref.setColumn(nameIdx);
                codeColumn = choicesSheet->cellAt(ref)->value().toString().trimmed();
                for (int lng = 0; lng < languages.count(); lng++)
                {
                    TlngLkpDesc desc;
                    desc.langCode = languages[lng].code;
                    ref.setColumn(languages[lng].idxInChoices);
                    cell2 = choicesSheet->cellAt(ref);
                    if (cell2 != 0)
                    {
                        QString columDesc;
                        columDesc = cell2->value().toString().trimmed();
                        columDesc = fixColumnName(columDesc);
                        if (isColumnValid(columDesc) == false)
                        {
                            log("The XLSX file has an CSV option with an invalid label: \"" + columDesc + "\". Only : and _ are allowed.");
                            exit(15);
                        }
                        desc.desc = columDesc;
                    }
                    else
                        desc.desc = "NONE";
                    descColumns.append(desc);
                }

            }
        }
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
                        res.append(value);
                    }
                    if (hasOrOther)
                    {
                        TlkpValue value;
                        value.code = "other";
                        for (int lng = 0; lng < languages.count(); lng++)
                        {
                            TlngLkpDesc desc;
                            desc.langCode = languages[lng].code;
                            ref.setColumn(languages[lng].idxInChoices);
                            desc.desc = "Other";
                            value.desc.append(desc);
                        }
                        res.append(value);
                    }
                    result = 0;
                    return res;
                }
                else
                {
                    log("Unable to retreive data for search \"" + file + "\". Reason: " + query.lastError().databaseText() + ". Maybe the \"name column\" or any of the \"labels columns\" do not exist in the CSV?");
                    result = 12;
                    return res;
                }
            }
            else
            {
                log("Cannot create SQLite database " + sqliteFile);
                result = 1;
                return res;
            }
        }
        else
        {
            log("There is no SQLite file for search \"" + file + "\". Did you add it as CSV when you ran ODKToMySQL?");            
            result = 13;
            return res;
        }
    }
    else
    {
        if (codeColumn != "")
            log("Cannot locate the code column for the search select \"" + listName + "\"");
        if (descColumns.count() == 0)
            log("Cannot locate a description column for the search select \"" + listName + "\"");
        result = 10;
        return res;
    }

    result = 0;
    return res;
}

//This return the values of a select in different languages. If the select is a reference then it return empty
QList<TlkpValue> getSelectValues(QXlsx::Worksheet *choicesSheet,QString listName,int listNameIdx,int nameIdx, bool hasOrOther)
{
    QList<TlkpValue> res;
    QXlsx::CellReference ref;
    QXlsx::Cell *cell;
    QXlsx::Cell *cell2;
    bool referenceLookup;
    referenceLookup = false;
    for (int nrow = 2; nrow <= choicesSheet->dimension().lastRow(); nrow++)
    {
        /*if (debug)
        {
            qDebug() << "Choices: " + QString::number(nrow);
            if (nrow == 48)
                qDebug() << "Choices here";
        }*/
        ref.setRow(nrow);
        ref.setColumn(listNameIdx);
        cell = choicesSheet->cellAt(ref);
        if (cell != 0)
        {
            if (cell->value().toString().toLower().trimmed() == listName.toLower().trimmed())
            {
                ref.setColumn(nameIdx);
                TlkpValue value;
                value.code = choicesSheet->cellAt(ref)->value().toString().trimmed();
                for (int lng = 0; lng < languages.count(); lng++)
                {
                    TlngLkpDesc desc;
                    desc.langCode = languages[lng].code;
                    ref.setColumn(languages[lng].idxInChoices);
                    cell2 = choicesSheet->cellAt(ref);
                    if (cell2 != 0)
                        desc.desc = cell2->value().toString().trimmed();
                    else
                        desc.desc = "Empty description for value " + value.code + " in language " + languages[lng].desc;
                    if (desc.desc.indexOf("${") >= 0) //We dont treat reference lists as lookup tables
                        referenceLookup = true;
                    value.desc.append(desc);
                }
                res.append(value);
            }
        }
    }
    if (!referenceLookup)
    {
        if (hasOrOther)
        {
            TlkpValue value;
            value.code = "other";
            for (int lng = 0; lng < languages.count(); lng++)
            {
                TlngLkpDesc desc;
                desc.langCode = languages[lng].code;
                ref.setColumn(languages[lng].idxInChoices);
                desc.desc = "Other";
                value.desc.append(desc);
            }
            res.append(value);
        }
        return res;
    }
    else
    {
        QList<TlkpValue> empty;
        return empty; //Return an empty list when the select is a reference
    }
}

/*Main process.
    Process the ODK Form XLSX File
    Basically it does the following:
        1- Check is the xlsx file is an ODK file
        2- Extracts the languages and points each language to the right columns
        3- Sequentially goes thorough the survey structure. begin groups/repeats are added to the stacks
           end groups/repeats are removed from the stacks. Begin repeats create new tables.
        4- Creates lookup tables for each select or multiselect
        5- Creates a multiselect table to store each value as a independent row
        6- Add tables to the list called tables
*/

bool variableIsControl(QString variableType)
{
    if ((variableType == "begin group") || (variableType == "begin_group"))
        return true;
    if ((variableType == "end group") || (variableType == "end_group"))
        return true;
    if ((variableType == "begin repeat") || (variableType == "begin_repeat"))
        return true;
    if ((variableType == "end repeat") || (variableType == "end_repeat"))
        return true;
    return false;
}

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
            if (tables[pos].fields[pos2].name == variable)
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

//This is the main process. It parses the XLSX file and store all information in the array "tables"
int processXLSX(QString inputFile, QString mainTable, QString mainField, QDir dir, QSqlDatabase database)
{
    bool hasSurveySheet;
    bool hasChoicesSheet;
    bool hasExternalChoicesSheet;
    bool hasSettingsSheet;
    hasSurveySheet = false;
    hasChoicesSheet = false;
    hasSettingsSheet = false;
    hasExternalChoicesSheet = false;
    QXlsx::Document xlsx(inputFile);
    QStringList sheets = xlsx.sheetNames();
    for (int nsheet = 0; nsheet <= sheets.count()-1;nsheet++)
    {
        if (sheets[nsheet].toLower() == "survey")
            hasSurveySheet = true;
        if (sheets[nsheet].toLower() == "choices")
            hasChoicesSheet = true;
        if (sheets[nsheet].toLower() == "settings")
            hasSettingsSheet = true;
        if (sheets[nsheet].toLower() == "external_choices")
            hasExternalChoicesSheet = true;
    }
    if (!hasSurveySheet || !hasChoicesSheet || !hasSettingsSheet)
    {
        log("The excel file does not seems to be an ODK XLSX file");
        return 1;
    }
    else
    {
        //Processing languages
        QStringList ODKLanguages;
        int ncols;
        QXlsx::CellReference ref;
        QXlsx::CellReference langRef;
        QXlsx::Cell *cell;
        QXlsx::Worksheet *excelSheet = (QXlsx::Worksheet*)xlsx.sheet("settings");
        bool hasDefaultLanguage = false;
        for (ncols = 1; ncols <= excelSheet->dimension().lastColumn(); ncols++)
        {
            ref.setRow(1);
            ref.setColumn(ncols);
            cell = excelSheet->cellAt(ref);
            if (cell != 0)

                if (cell->value().toString().toLower() == "default_language")
                {
                    ref.setRow(2);
                    ref.setColumn(ncols);
                    ODKLanguages.append(excelSheet->cellAt(ref)->value().toString());
                    hasDefaultLanguage = true;
                }
        }

        excelSheet = (QXlsx::Worksheet*)xlsx.sheet("survey");
        for (ncols = 1; ncols <= excelSheet->dimension().lastColumn(); ncols++)
        {            
            ref.setRow(1);
            ref.setColumn(ncols);
            cell = excelSheet->cellAt(ref);
            if (cell != 0)
            {
                if (cell->value().toString().indexOf("label") >= 0)
                {
                    QString label;
                    label = cell->value().toString();
                    if (label.indexOf(":") >= 0)
                    {
                        int langIdx = label.lastIndexOf(":");
                        QString language;
                        language = label.right(label.length()-langIdx-1);
                        if (ODKLanguages.indexOf(language) < 0)
                            ODKLanguages.append(language);
                    }
                    else
                    {
                        if (!hasDefaultLanguage)
                        {
                            if (ODKLanguages.indexOf("English") < 0)
                                ODKLanguages.append("English");
                        }
                    }
                }
            }
        }
        if (ODKLanguages.count() == 0)
            ODKLanguages.append("English");

        if ((ODKLanguages.count() > 1) && (languages.count() == 1))
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
            return 3;
        }

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
            if (outputType == "m")
                log(XMLResult.toString());
            return 4;
        }

        //Allocating survey labels column indexes to languages
        for (ncols = 1; ncols <= excelSheet->dimension().lastColumn(); ncols++)
        {
            ref.setRow(1);
            ref.setColumn(ncols);
            cell = excelSheet->cellAt(ref);
            if (cell != 0)
            {
                if (cell->value().toString().toLower().indexOf("label") >= 0)
                {
                    QString label;
                    label = cell->value().toString();
                    if (label.indexOf(":") >= 0)
                    {
                        int langIdx = label.lastIndexOf(":");
                        QString language;
                        language = label.right(label.length()-langIdx-1);
                        langIdx = genLangIndexByName(language);
                        languages[langIdx].idxInSurvey = ncols;
                    }
                    else
                    {
                        int langIdx = genLangIndexByName(ODKLanguages[0]);
                        languages[langIdx].idxInSurvey = ncols;
                    }
                }
            }
        }

        //Allocating choices labels column indexes to languages
        excelSheet = (QXlsx::Worksheet*)xlsx.sheet("choices");
        for (ncols = 1; ncols <= excelSheet->dimension().lastColumn(); ncols++)
        {
            ref.setRow(1);
            ref.setColumn(ncols);
            cell = excelSheet->cellAt(ref);
            if (cell != 0)
            {
                if (cell->value().toString().toLower().indexOf("label") >= 0)
                {
                    QString label;
                    label = cell->value().toString();
                    if (label.indexOf(":") >= 0)
                    {
                        int langIdx = label.lastIndexOf(":");
                        QString language;
                        language = label.right(label.length()-langIdx-1);
                        langIdx = genLangIndexByName(language);
                        languages[langIdx].idxInChoices = ncols;
                    }
                    else
                    {
                        int langIdx = genLangIndexByName(ODKLanguages[0]);
                        languages[langIdx].idxInChoices = ncols;
                    }
                }
            }
        }
        for (ncols = 0; ncols < languages.count();ncols++)
        {
            if ((languages[ncols].idxInChoices == -1) || (languages[ncols].idxInSurvey == -1))
            {
                log("Language " + languages[ncols].desc + " is not present in the labels of the sheets choices or Survey");
                return 5;
            }
        }
        //Processing survey structure
        excelSheet = (QXlsx::Worksheet*)xlsx.sheet("survey");
        //Getting the Survey column
        int columnType;
        columnType = -1;
        int columnName;
        columnName = -1;
        int columnAppearance;
        columnAppearance = -1;
        int columnCalculation;
        columnCalculation = -1;

        for (ncols = 1; ncols <= excelSheet->dimension().lastColumn(); ncols++)
        {
            ref.setRow(1);
            ref.setColumn(ncols);
            cell = excelSheet->cellAt(ref);
            if (cell != 0)
            {
                if (cell->value().toString().toLower().indexOf("type") >= 0)
                {
                    columnType = ncols;
                }
                if (cell->value().toString().toLower().indexOf("name") >= 0)
                {
                    columnName = ncols;
                }
                if (cell->value().toString().toLower().indexOf("appearance") >= 0)
                {
                    columnAppearance = ncols;
                }
                if (cell->value().toString().toLower().indexOf("calculation") >= 0)
                {
                    columnCalculation = ncols;
                }
            }
        }
        if ((columnType == -1) || (columnName == -1))
        {
            log("Cannot find type or name column in the ODK. Is this an ODK file?");
            return 1;
        }
        QXlsx::Worksheet *choicesSheet = (QXlsx::Worksheet*)xlsx.sheet("choices");
        int columnListName;
        columnListName = -1;
        int columnChoiceName;
        columnChoiceName = -1;
        for (ncols = 1; ncols <= choicesSheet->dimension().lastColumn(); ncols++)
        {
            ref.setRow(1);
            ref.setColumn(ncols);
            cell =  choicesSheet->cellAt(ref);
            if (cell != 0)
            {
                if (cell->value().toString().toLower().indexOf("list_name") >= 0)
                {
                    columnListName = ncols;
                }
                if (cell->value().toString().toLower().indexOf("name") >= 0)
                {
                    columnChoiceName = ncols;
                }
            }
        }
        if ((columnListName == -1) || (columnChoiceName == -1))
        {
            log("Cannot find list_name or name in choices in the ODK. Is this an ODK file?");
            return 1;
        }
        //Look for external choices
        int externalColumnListName;
        externalColumnListName = -1;
        int externalColumnChoiceName;
        externalColumnChoiceName = -1;
        QXlsx::Worksheet *externalChoicesSheet = (QXlsx::Worksheet*)xlsx.sheet("external_choices");
        if (hasExternalChoicesSheet)
        {
            for (ncols = 1; ncols <= externalChoicesSheet->dimension().lastColumn(); ncols++)
            {
                ref.setRow(1);
                ref.setColumn(ncols);
                cell =  externalChoicesSheet->cellAt(ref);
                if (cell != 0)
                {
                    if (cell->value().toString().toLower().indexOf("list_name") >= 0)
                    {
                        externalColumnListName = ncols;
                    }
                    if (cell->value().toString().toLower().indexOf("name") >= 0)
                    {
                        externalColumnChoiceName = ncols;
                    }
                }
            }
            if ((externalColumnListName == -1) || (externalColumnChoiceName == -1))
            {
                log("Cannot find list_name or name in external choices in the ODK. Is this an ODK file?");
                return 1;
            }
        }


        int lang;
        //Creating the main table
        tableIndex = tableIndex + 1;
        TtableDef maintable;
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
        maintable.fields.append(foriginID);

        tables.append(maintable);
        addToRepeat(mainTable);

        //Processing variables
        QString variableType;
        QString variableName;
        QString variableApperance;
        QString variableCalculation;
        QString tableName;
        int tblIndex;
        int nrow;

        bool mainFieldFound;
        mainFieldFound = false;

        bool mainFieldinMainTable;
        mainFieldinMainTable = false;

        for (nrow = 2; nrow <= excelSheet->dimension().lastRow(); nrow++)
        {
            /*qDebug() << nrow;
            if (nrow == 39)
            {
                qDebug() << "Here";
                debug = true;
            }*/

            ref.setRow(nrow);
            ref.setColumn(columnType);
            cell = excelSheet->cellAt(ref);
            if (cell != 0)
                variableType = cell->value().toString().toLower().trimmed();
            else
                variableType = "note";
            ref.setColumn(columnName);
            cell = excelSheet->cellAt(ref);
            if (cell != 0)            
                variableName = cell->value().toString().trimmed();
            else
                variableName = "";
            //Read the apperance value
            ref.setColumn(columnAppearance);
            cell = excelSheet->cellAt(ref);
            if (cell != 0)
                variableApperance = cell->value().toString().trimmed().toLower();
            else
                variableApperance = "";
            //Read the calculation value
            ref.setColumn(columnCalculation);
            cell = excelSheet->cellAt(ref);
            if (cell != 0)
                variableCalculation = cell->value().toString().trimmed().toLower();
            else
                variableCalculation = "";

            //if (variableName == "secd_d4_exotic_breed")
            //{
            //    qDebug() << "!!!!!!!!!!!!!secd_d4_exotic_breed!!!!!!!!!!!!";
            //}

            if ((variableType == "begin group") || (variableType == "begin_group"))
                addToStack(variableName);
            if ((variableType == "end group") || (variableType == "end_group"))
                removeFromStack();
            if ((variableType == "begin repeat") || (variableType == "begin_repeat"))
            {
                if (variableName.toLower() != mainTable.toLower())
                {
                    //Find the father of this table
                    tableName = getTopRepeat();
                    //Creates the new tables
                    tableIndex = tableIndex + 1;
                    TtableDef aTable;
                    aTable.name = fixField(variableName.toLower());

                    for (int lng = 0; lng < languages.count();lng++)
                    {
                        langRef.setRow(nrow);
                        langRef.setColumn(languages[lng].idxInSurvey);
                        cell = excelSheet->cellAt(langRef);
                        if (cell != 0)
                        {
                            TlngLkpDesc fieldDesc;
                            fieldDesc.langCode = languages[lng].code;
                            fieldDesc.desc = cell->value().toString().trimmed();
                            aTable.desc.append(fieldDesc);
                        }
                    }

                    aTable.pos = tableIndex;
                    aTable.islookup = false;
                    aTable.isSeparated = false;
                    if (getVariableStack() == "")
                        aTable.xmlCode = variableName;
                    else
                        aTable.xmlCode = getVariableStack() + "/" + variableName;
                    aTable.parentTable = tableName;
                    //Add the father key fields to the table as related fields
                    TtableDef parentTable = getTable(tableName);
                    for (int field = 0; field < parentTable.fields.count()-1;field++)
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
                            relField.selectSource = "NONE";
                            relField.selectListName = "NONE";
                            relField.isMultiSelect = false;
                            relField.multiSelectTable = "";
                            aTable.fields.append(relField);
                        }
                    }
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
                    keyField.selectSource = "NONE";
                    keyField.selectListName = "NONE";
                    keyField.isMultiSelect = false;
                    keyField.multiSelectTable = "";
                    aTable.fields.append(keyField);
                    tables.append(aTable);
                    addToRepeat(variableName);
                    addToStack(variableName);
                }
                else
                {
                    int mainTableIndex;
                    mainTableIndex = getTableIndex(variableName.toLower());
                    if (getVariableStack() == "")
                        tables[mainTableIndex].xmlCode = variableName;
                    else
                        tables[mainTableIndex].xmlCode = getVariableStack() + "/" + variableName;
                    addToStack(variableName);
                }
            }
            if ((variableType == "end repeat") || (variableType == "end_repeat"))
            {
                if (variableName.trimmed().toLower() == "")
                    variableName = getTopRepeat();

                if (variableName.toLower() != mainTable.toLower())
                {
                    removeRepeat();
                    removeFromStack();
                }
                else
                {
                    removeFromStack();
                }
            }
            if (!variableIsControl(variableType))
            {
                tableName = getTopRepeat();

                //if (tableName == "rpt_lrq1_labouract")
                //    qDebug() << "rpt_lrq1_labouract";

                if (tableName == "")
                    tableName = mainTable;
                tblIndex = getTableIndex(tableName);
                if (variableType.trimmed() != "") //Not process empty cells
                {
                    if ((variableType.indexOf("select_one") == -1) && (variableType.indexOf("select_multiple") == -1) && (variableType.indexOf("note") == -1) && (variableType.indexOf("select_one_external") == -1))
                    {

                        TfieldDef aField;
                        aField.name = fixField(variableName.toLower());
                        TfieldMap vartype = mapODKFieldTypeToMySQL(variableType);
                        aField.selectSource = "NONE";
                        aField.selectListName = "NONE";
                        aField.odktype = variableType; //variableType
                        aField.type = vartype.type;
                        aField.size = vartype.size;
                        aField.decSize = vartype.decSize;
                        if (fixField(variableName.toLower()) == fixField(mainField.toLower()))
                        {
                            mainFieldFound = true;

                            if (fixField(tables[tblIndex].name.trimmed().toLower()) == fixField(mainTable.toLower()))
                                mainFieldinMainTable = true;

                            aField.key = true;
                            if (aField.type == "text")
                            {
                                aField.type = "varchar";
                                aField.size = 60;
                            }
                        }
                        else
                            aField.key = false;
                        if (variableType != "calculate")
                        {
                            aField.rTable = "";
                            aField.rField = "";
                        }
                        else
                        {
                            if (variableCalculation.indexOf("selected-at(") >=0 )
                            {
                                getReferenceForSelectAt(variableCalculation,aField.type,aField.size,aField.decSize,aField.rTable,aField.rField);
                            }
                            else
                            {
                                aField.rTable = "";
                                aField.rField = "";
                            }

                        }
                        if (getVariableStack() != "")
                            aField.xmlCode = getVariableStack() + "/" + variableName;
                        else
                            aField.xmlCode = variableName;                        
                        aField.isMultiSelect = false;
                        aField.multiSelectTable = "";
                        for (int lng = 0; lng < languages.count();lng++)
                        {
                            ref.setRow(nrow);
                            ref.setColumn(languages[lng].idxInSurvey);
                            cell = excelSheet->cellAt(ref);
                            if (cell != 0)
                            {
                                TlngLkpDesc fieldDesc;
                                fieldDesc.langCode = languages[lng].code;
                                fieldDesc.desc = cell->value().toString().trimmed();
                                aField.desc.append(fieldDesc);
                            }
                        }
                        tables[tblIndex].fields.append(aField);
                    }
                    else
                    {
                        //Processing selects                        
                        if ((variableType.indexOf("select_one") >= 0) || ((variableType.indexOf("select_one_external") >= 0) && (hasExternalChoicesSheet)))
                        {
                            QList<TlkpValue> values;
                            QString listName;
                            listName = "";
                            if (variableType.indexOf(" ") >= 0)
                                listName = variableType.split(" ",QString::SkipEmptyParts)[1].trimmed();
                            if (variableType.indexOf(" ") >= 0)
                                listName = variableType.split(" ",QString::SkipEmptyParts)[1].trimmed();
                            if (listName != "")
                            {
                                if (variableType.indexOf("select_one_external") >= 0)
                                    values.append(getSelectValues(externalChoicesSheet,listName,externalColumnListName,externalColumnChoiceName,selectHasOrOther(variableType)));
                                else
                                {
                                    if (variableApperance.indexOf("search(") == -1)
                                        values.append(getSelectValues(choicesSheet,listName,columnListName,columnChoiceName,selectHasOrOther(variableType)));
                                    else
                                    {
                                        int result;
                                        values.append(getSelectValuesFromCSV(variableApperance,choicesSheet,listName,columnListName,columnChoiceName,selectHasOrOther(variableType),result,dir,database));
                                        if (result != 0)
                                            return result;
                                    }
                                }
                            }
                            //Processing field
                            TfieldDef aField;
                            if (variableType.indexOf("select_one_external") >= 0)
                                aField.selectSource = "EXTERNAL";
                            else
                            {
                                if (variableApperance.indexOf("search(") == -1)
                                    aField.selectSource = "INTERNAL";
                                else
                                    aField.selectSource = "SEARCH";
                            }

                            aField.name = fixField(variableName.toLower());
                            aField.odktype = variableType;
                            aField.selectListName = listName;
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
                            if (fixField(variableName.toLower()) == fixField(mainField.toLower()))
                            {
                                aField.key = true;

                                mainFieldFound = true;

                                if (fixField(tables[tblIndex].name.trimmed().toLower()) == fixField(mainTable.toLower()))
                                    mainFieldinMainTable = true;
                            }
                            else
                                aField.key = false;
                            aField.isMultiSelect = false;
                            aField.multiSelectTable = "";
                            if (getVariableStack() != "")
                                aField.xmlCode = getVariableStack() + "/" + variableName;
                            else
                                aField.xmlCode = variableName;
                            for (int lng = 0; lng < languages.count();lng++)
                            {
                                ref.setRow(nrow);
                                ref.setColumn(languages[lng].idxInSurvey);
                                cell = excelSheet->cellAt(ref);
                                if (cell != 0)
                                {
                                    TlngLkpDesc fieldDesc;
                                    fieldDesc.langCode = languages[lng].code;
                                    fieldDesc.desc = cell->value().toString().trimmed();
                                    aField.desc.append(fieldDesc);
                                }
                            }
                            if (values.count() > 0) //If the lookup table has values
                            {
                                //Creating the lookp table if its neccesary
                                if (isLookUpYesNo(values) == false) //Do not process yes/no values as lookup tables
                                {
                                    TtableDef lkpTable = getDuplicatedLkpTable(values);
                                    if (lkpTable.name == "EMPTY")
                                    {
                                        lkpTable.name = "lkp" + fixField(variableName.toLower());
                                        for (lang = 0; lang < aField.desc.count(); lang++)
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
                                        for (lang = 0; lang <= languages.count()-1;lang++)
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
                                oField.key = false;
                                oField.multiSelectTable = "";
                                oField.rField = "";
                                oField.rTable = "";
                                oField.size = 120;
                                oField.type = "varchar";
                                tables[tblIndex].fields.append(oField);
                            }
                        }                        
                        //Select multiple
                        if (variableType.indexOf("select_multiple") >= 0)
                        {
                            if (fixField(variableName.toLower()) == fixField(mainField.toLower()))
                            {
                                log("Error: Primary ID : " + mainField + " cannot be a multi-select variable");
                                return 1;
                            }
                            //Processing multiselects
                            QString listName;
                            listName = "";
                            if (variableType.indexOf(" ") >= 0)
                                listName = variableType.split(" ",QString::SkipEmptyParts)[1].trimmed();
                            if (variableType.indexOf(" ") >= 0)
                                listName = variableType.split(" ",QString::SkipEmptyParts)[1].trimmed();
                            QList<TlkpValue> values;
                            if (listName != "")
                            {
                                if (variableApperance.indexOf("search(") == -1)
                                    values.append(getSelectValues(choicesSheet,listName,columnListName,columnChoiceName,selectHasOrOther(variableType)));
                                else
                                {
                                    int result;
                                    values.append(getSelectValuesFromCSV(variableApperance,choicesSheet,listName,columnListName,columnChoiceName,selectHasOrOther(variableType),result,dir,database));
                                    if (result != 0)
                                        return result;
                                }
                            }

                            //Processing  the field
                            TfieldDef aField;
                            aField.name = fixField(variableName.toLower());
                            if (variableApperance.indexOf("search(") == -1)
                                aField.selectSource = "INTERNAL";
                            else
                                aField.selectSource = "SEARCH";
                            aField.selectListName = listName;
                            aField.type = "varchar";
                            aField.size = getMaxMSelValueLength(values);
                            aField.decSize = 0;
                            aField.key = false;
                            aField.isMultiSelect = true;
                            aField.multiSelectTable = fixField(tables[tblIndex].name) + "_msel_" + fixField(variableName.toLower());
                            if (getVariableStack() != "")
                                aField.xmlCode = getVariableStack() + "/" + variableName;
                            else
                                aField.xmlCode = variableName;
                            for (int lng = 0; lng < languages.count();lng++)
                            {
                                ref.setRow(nrow);
                                ref.setColumn(languages[lng].idxInSurvey);
                                cell = excelSheet->cellAt(ref);
                                if (cell != 0)
                                {
                                    TlngLkpDesc fieldDesc;
                                    fieldDesc.langCode = languages[lng].code;
                                    fieldDesc.desc = cell->value().toString().trimmed();
                                    aField.desc.append(fieldDesc);
                                }
                            }
                            if (values.count() > 0) //If the lookup table has values
                            {
                                //Creating the multiselect table
                                TtableDef mselTable;
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
                                        relField.multiSelectTable = "";
                                        mselTable.fields.append(relField);
                                    }
                                }
                                mselTable.name =  fixField(tables[tblIndex].name) + + "_msel_" + fixField(variableName.toLower());

                                for (lang = 0; lang < aField.desc.count(); lang++)
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
                                if (lkpTable.name == "EMPTY")
                                {
                                    lkpTable.name = "lkp" + fixField(variableName.toLower());

                                    for (lang = 0; lang < aField.desc.count(); lang++)
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
            }
        }
        if (!mainFieldFound)
        {
            log("ERROR!: The main variable \"" + mainField + "\" was not found in the ODK. Please indicate a correct main variable");
            return 10;
        }
        if (!mainFieldinMainTable)
        {
            log("ERROR!: The main variable \"" + mainField + "\" is not in the main table \"" + mainTable + "\". Please indicate a correct main variable.");
            return 11;
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

// We put a cap of 100 fiels in a table.
// Basically it is ridiculous to have a table with more the 100 variable and consider it normalized.
// We force the user to think and come with common (normalized) groups of data

// This is one reason fo creating a manifest file.
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
        //if (temp.childNodes().count() > 100)
            //log("WARNING!: " + grpName + " has more than 200 fields. This might end up in further separation");
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
            if ((table.fields[pos].name == "surveyid") || (table.fields[pos].name == "originid") || (table.fields[pos].name == "rowuuid"))
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

//This function return the list names referencing a lookup table
QStringList getReferencingLists(QString table)
{
    QStringList res;
    for (int pos = 0; pos <= tables.count()-1; pos++)
    {
        for (int pos2 = 0; pos2 <= tables[pos].fields.count()-1; pos2++)
        {
            if (tables[pos].fields[pos2].rTable == table)
                res.append(tables[pos].fields[pos2].xmlCode + "~~" + tables[pos].fields[pos2].selectListName);
        }
    }
    return res;
}

//This function checks the lookup tables to see if a value appears more than once
int checkLookupTables()
{
    QDomDocument XMLResult;
    XMLResult = QDomDocument("XMLResult");
    QDomElement XMLRoot;
    XMLRoot = XMLResult.createElement("XMLResult");
    XMLResult.appendChild(XMLRoot);

    bool repeatedValues;
    repeatedValues = false;
    for (int pos = 0; pos <= tables.count()-1; pos++)
    {
        QDomElement eList;
        eList = XMLResult.createElement("list");
        eList.setAttribute("name",tables[pos].name);

        QDomElement eValues;
        eValues = XMLResult.createElement("values");
        eList.appendChild(eValues);
        QDomElement eReferences;
        eReferences = XMLResult.createElement("references");
        eList.appendChild(eReferences);

        if (tables[pos].islookup)
        {                        
            QSet<QString> valueSet;
            QStringList repeated;
            for (int pos2 = 0; pos2 <= tables[pos].lkpValues.count()-1;pos2++)
            {
                if (valueSet.contains(tables[pos].lkpValues[pos2].code))
                {
                    if (repeated.indexOf(tables[pos].lkpValues[pos2].code) == -1)
                        repeated.append(tables[pos].lkpValues[pos2].code);
                }
                else
                    valueSet << tables[pos].lkpValues[pos2].code;
            }
            if (repeated.count() > 0)
            {
                if (outputType == "h")
                    log("Error: Duplicated options. Variables:");
                repeatedValues = true;
                QStringList references;
                references = getReferencingLists(tables[pos].name);
                for (int pos2 = 0; pos2 <= references.count()-1; pos2++)
                {
                    QDomElement eReference;
                    eReference = XMLResult.createElement("reference");
                    QStringList refCode;
                    refCode = references[pos2].split("~~");
                    eReference.setAttribute("variable",refCode[0]);
                    eReference.setAttribute("option",refCode[1]);
                    eReferences.appendChild(eReference);
                    if (outputType == "h")
                        log(refCode[0] + " with option " + refCode[1]);
                }
                if (outputType == "h")
                    log("Have duplicated values: ");
                for (int pos2 = 0; pos2 <= repeated.count()-1; pos2++)
                {
                    QDomElement eValue;
                    eValue = XMLResult.createElement("value");
                    QDomText vValue;
                    vValue = XMLResult.createTextNode(repeated[pos2]);
                    eValue.appendChild(vValue);
                    eValues.appendChild(eValue);
                    if (outputType == "h")
                        log(repeated[pos2]);
                }
                if (outputType == "h")
                    log("----------------------------");
                XMLRoot.appendChild(eList);
            }
        }
    }
    if (repeatedValues)
    {
        if (outputType == "m")
            log(XMLResult.toString());
        return 9;
    }
    return 0;
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
        return 2;
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
        language.code = code;
        language.desc = name;
        language.deflang = defLang;
        language.idxInChoices = -1;
        language.idxInSurvey = -1;
        languages.append(language);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * ODK To MySQL                                                      * \n";
    title = title + " * This tool generates a MySQL schema from a ODK XLSX survey file    * \n";
    title = title + " * Though ODK Aggregate can store data in MySQL such repository      * \n";
    title = title + " * is for storage only. The resulting schema from Aggregate is not   * \n";
    title = title + " * relational or easy for analysis.                                  * \n";
    title = title + " * ODKtoMySQL generates a full relational MySQL database from a      * \n";
    title = title + " * ODK XLSX file.                                                    * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.0");

    TCLAP::ValueArg<std::string> inputArg("x","inputXLSX","Input ODK XLSX survey file",true,"","string");
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
            return 6;
    
    if (isDefaultLanguage("English") == false)
    {
        if (yesNoString == "")
        {
            log("English is not the default language. You need to specify Yes and No values with the -y parameter in " + getDefLanguage());
            return 7;
        }
        if (yesNoString.indexOf("|") == -1)
        {
            log("Malformed yes|no language string1");
            return 8;
        }
        if (yesNoString.length() == 1)
        {
            log("Malformed yes|no language string2");
            return 8;
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
    returnValue = processXLSX(input,mTable.trimmed(),mainVar.trimmed(),dir,dblite);
    if (returnValue == 0)
    {
        //dumpTablesForDebug();
        if (sepFile != "") //If we have a separation file
        {
            //log("Separating tables");
            QFile file(sepFile);
            if (file.exists())
            {
                if (separeteTables(sepFile) == 1) //Separate the tables using the file  //TODO: If separation happens then we need to move multi-select tables to the end of the list
                    return 1;
            }
        }
        appendUUIDs();
        //log("Checking tables");
        if (checkTables(sepOutFile) != 0) //Check the tables to see if they have less than 60 related fields. If so a separation file is created
        {
            return 2; //If a table has more than 60 related field then exit
        }
        if (checkLookupTables() != 0)
            return 9;
        //log("Generating SQL scripts");
        genSQL(ddl,insert,metadata,xmlFile,transFile,xmlCreateFile,insertXML,drop);
    }
    else
        return returnValue;
    if (outputType == "h")
        log("Done without errors");
    return 0;
}
