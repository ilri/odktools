/*
This file is part of ODKTools.

Copyright (C) 2015 International Livestock Research Institute.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

ODKTools is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

ODKTools is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with ODKTools.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <tclap/CmdLine.h>
#include <QtXml>
#include <QFile>
#include "xlsxdocument.h"
#include "xlsxworkbook.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"
#include "xlsxcellreference.h"
#include "xlsxcell.h"

bool debug;

//This logs messages to the terminal. We use printf because qDebug does not log in relase
void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf(temp.toUtf8().data());
}

QString strYes; //Yes string for comparing Yes/No lookup tables
QString strNo; //No string for comparing Yes/No lookup tables
QStringList variableStack; //This is a stack of groups or repeats for a variable. Used to get /xxx/xxx/xxx structures
QStringList repeatStack; //This is a stack of repeats. So we know in which repeat we are
QString prefix; //Table prefix
int tableIndex; //Global index of a table. Used later on to sort them

//Structure that holds the description of each lkpvalue separated by language
struct lngDesc
{
    QString langCode;
    QString desc;
};
typedef lngDesc TlngLkpDesc;

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
  int size; //Variable size
  int decSize; //Variable decimal size
  QString rTable; //Related table
  QString rField; //Related field
  bool key; //Whether the field is key
  QString xmlCode; //The field XML code /xx/xx/xx/xx
  bool isMultiSelect; //Whether the field if multiselect
  QString multiSelectTable; //Multiselect table
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
void genSQL(QString ddlFile,QString insFile, QString metaFile, QString xmlFile, QString transFile, QString UUIDFile, QString XMLCreate, QString insertXML)
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

    QFile UUIDTriggersFile(UUIDFile);
    if (!UUIDTriggersFile.open(QIODevice::WriteOnly | QIODevice::Text))
             return;
    QTextStream UUIDStrm(&UUIDTriggersFile);
    UUIDStrm.setCodec("UTF-8");

    QFile sqlUpdateFile(metaFile);
    if (!sqlUpdateFile.open(QIODevice::WriteOnly | QIODevice::Text))
             return;

    QTextStream sqlUpdateStrm(&sqlUpdateFile);
    sqlUpdateStrm.setCodec("UTF-8");

    //Start creating the header or each file.
    QDateTime date;
    date = QDateTime::currentDateTime();

    QStringList rTables;

    sqlCreateStrm << "-- Code generated by ODKToMySQL" << "\n";
    sqlCreateStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    sqlCreateStrm << "-- by: ODKToMySQL Version 1.0" << "\n";
    sqlCreateStrm << "-- WARNING! All changes made in this file might be lost when running ODKToMySQL again" << "\n\n";

    sqlInsertStrm << "-- Code generated by ODKToMySQL" << "\n";
    sqlInsertStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    sqlInsertStrm << "-- by: ODKToMySQL Version 1.0" << "\n";
    sqlInsertStrm << "-- WARNING! All changes made in this file might be lost when running ODKToMySQL again" << "\n\n";

    iso639Strm << "-- Code generated by ODKToMySQL" << "\n";
    iso639Strm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    iso639Strm << "-- by: ODKToMySQL Version 1.0" << "\n";
    iso639Strm << "-- WARNING! All changes made in this file might be lost when running ODKToMySQL again" << "\n\n";

    UUIDStrm << "-- Code generated by ODKToMySQL" << "\n";
    UUIDStrm << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
    UUIDStrm << "-- by: ODKToMySQL Version 1.0" << "\n";
    UUIDStrm << "-- WARNING! All changes made in this file might be lost when running ODKToMySQL again" << "\n\n";

    sqlUpdateStrm << "-- Code generated by ODKToMySQL" << "\n";
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
            }
            else
            {
                //For the create XML
                tables[pos].tableCreteElement = XMLSchemaStructure.createElement("table");
                tables[pos].tableCreteElement.setAttribute("name",prefix + tables[pos].name.toLower());
            }
        }
        else
        {
            tables[pos].tableCreteElement = XMLSchemaStructure.createElement("table");
            tables[pos].tableCreteElement.setAttribute("name",prefix + tables[pos].name.toLower());

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
                        if (tables[pos].fields[clm].isMultiSelect == true)
                        {
                            fieldNode.setAttribute("isMultiSelect","true");
                            fieldNode.setAttribute("multiSelectTable",prefix + tables[pos].fields[clm].multiSelectTable);
                        }
                        if (tables[pos].fields[clm].key)
                        {
                            fieldNode.setAttribute("key","true");
                            if (tables[pos].fields[clm].rTable != "")
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
                        createFieldNode.setAttribute("type",tables[pos].fields[clm].type);
                        createFieldNode.setAttribute("size",tables[pos].fields[clm].size);
                        createFieldNode.setAttribute("decsize",tables[pos].fields[clm].decSize);
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
                        createFieldNode.setAttribute("size",tables[pos].fields[clm].size);
                        createFieldNode.setAttribute("decsize",tables[pos].fields[clm].decSize);
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
                    createFieldNode.setAttribute("type",tables[pos].fields[clm].type);
                    createFieldNode.setAttribute("size",tables[pos].fields[clm].size);
                    createFieldNode.setAttribute("decsize",tables[pos].fields[clm].decSize);
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
                createFieldNode.setAttribute("type",tables[pos].fields[clm].type);
                createFieldNode.setAttribute("size",tables[pos].fields[clm].size);
                createFieldNode.setAttribute("decsize",tables[pos].fields[clm].decSize);
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
                field = field + " NOT NULL , ";
            else
                field = field + " , ";

            fields << field << "\n";

            if (tables[pos].fields[clm].key == true)
                keys << tables[pos].fields[clm].name + " , ";

            //Here we create the indexes and constraints for lookup tables using RESTRICT

            if (!tables[pos].fields[clm].rTable.isEmpty())
            {
                if (isRelatedTableLookUp(tables[pos].fields[clm].rTable))
                {
                    idx++;
                    index = "INDEX fk" + QString::number(idx) + "_" + prefix + tables[pos].name.toLower() + "_" + tables[pos].fields[clm].rTable.toLower() ;
                    indexes << index.left(64) + " (" + tables[pos].fields[clm].name.toLower() + ") , " << "\n";

                    constraint = "CONSTRAINT fk" + QString::number(idx) + "_" + prefix + tables[pos].name.toLower() + "_" + tables[pos].fields[clm].rTable.toLower();
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
            index = "INDEX fk" + QString::number(idx) + "_" + prefix + tables[pos].name.toLower() + "_" + rTables[clm].toLower() ;
            indexes << index.left(64) + " (" + getForeignColumns(tables[pos],rTables[clm]) + ") , " << "\n";

            constraint = "CONSTRAINT fk" + QString::number(idx) + "_" + prefix + tables[pos].name.toLower() + "_" + rTables[clm].toLower();
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
        sql = sql + ")" + "\n ENGINE = InnoDB CHARSET=utf8; \n\n";

        //Append UUIDs triggers to the the
        UUIDStrm << "CREATE TRIGGER uudi_" + prefix+ tables[pos].name.toLower() + " BEFORE INSERT ON " + prefix + tables[pos].name.toLower() + " FOR EACH ROW SET new.rowuuid = uuid();\n";

        sqlCreateStrm << sql; //Add the final SQL to the DDL file

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

//This return the values of a select in different languages. If the select is a reference then it return empty
QList<TlkpValue> getSelectValues(QXlsx::Worksheet *choicesSheet,QString listName,int listNameIdx,int nameIdx)
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
        return res;
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

int processXLSX(QString inputFile, QString mainTable, QString mainField)
{
    bool hasSurveySheet;
    bool hasChoicesSheet;
    bool hasSettingsSheet;
    hasSurveySheet = false;
    hasChoicesSheet = false;
    hasSettingsSheet = false;
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
                }
            }
        }
        if (ODKLanguages.count() == 0)
            ODKLanguages.append("English");

        if ((ODKLanguages.count() > 1) && (languages.count() == 1))
        {
            log("This ODK has multiple languages but not other languages where specified with the -l parameter.");
            return 1;
        }

        for (int lng = 0; lng < ODKLanguages.count();lng++)
            if (genLangIndexByName(ODKLanguages[lng]) == -1)
            {
                log("ODK language " + ODKLanguages[lng] + " was not found. Please indicate it as default language (-d) or as other lannguage (-l)");
                return 1;
            }

        //Allocating survey labels column indexes to languages
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
                if (cell->value().toString().indexOf("label") >= 0)
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
                log("Language " + languages[ncols].desc + " has not Choices or Survey column");
                return 1;
            }
        }
        //Processing survey structure
        excelSheet = (QXlsx::Worksheet*)xlsx.sheet("survey");
        //Getting the Survey column
        int columnType;
        columnType = -1;
        int columnName;
        columnName = -1;
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
            langDesc.desc = "Survey ID (UUID)";
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
        maintable.fields.append(fsurveyID);

        //Append the origin ID to the main table
        TfieldDef foriginID;
        foriginID.name = "originid";

        for (lang = 0; lang <= languages.count()-1;lang++)
        {
            TlngLkpDesc langDesc;
            langDesc.langCode = languages[lang].code;
            langDesc.desc = "Origin ID: formhub or aggregate";
            foriginID.desc.append(langDesc);
        }
        foriginID.type = "varchar";
        foriginID.size = 15;
        foriginID.decSize = 0;        
        foriginID.rField = "";
        foriginID.rTable = "";
        foriginID.key = false;
        foriginID.xmlCode = "NONE";
        foriginID.isMultiSelect = false;
        maintable.fields.append(foriginID);

        tables.append(maintable);
        addToRepeat(mainTable);

        //Processing variables
        QString variableType;
        QString variableName;
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

            if (variableType == "begin group")
                addToStack(variableName);
            if (variableType == "end group")
                removeFromStack();
            if (variableType == "begin repeat")
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
            if (variableType == "end repeat")
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
            if ((variableType.indexOf("group") == -1) && (variableType.indexOf("repeat") == -1))
            {
                tableName = getTopRepeat();
                if (tableName == "")
                    tableName = mainTable;
                tblIndex = getTableIndex(tableName);
                if (variableType.trimmed() != "") //Not process empty cells
                {
                    if ((variableType.indexOf("select_one") == -1) && (variableType.indexOf("select_multiple") == -1) && (variableType.indexOf("note") == -1))
                    {

                        TfieldDef aField;
                        aField.name = fixField(variableName.toLower());
                        TfieldMap vartype = mapODKFieldTypeToMySQL(variableType);
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
                        aField.rTable = "";
                        aField.rField = "";
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
                        if (variableType.indexOf("select_one") >= 0)
                        {
                            QList<TlkpValue> values;
                            QString listName;
                            listName = "";
                            if (variableType.indexOf(" ") >= 0)
                                listName = variableType.split(" ",QString::SkipEmptyParts)[1].trimmed();
                            if (variableType.indexOf(" ") >= 0)
                                listName = variableType.split(" ",QString::SkipEmptyParts)[1].trimmed();
                            if (listName != "")
                                values.append(getSelectValues(choicesSheet,
                                                          listName,
                                          columnListName,columnChoiceName));
                            //Processing field
                            TfieldDef aField;
                            aField.name = fixField(variableName.toLower());
                            if (areValuesStrings(values))
                                aField.type = "varchar";
                            else
                                aField.type = "int";
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
                        }
                        if (variableType.indexOf("select_multiple") >= 0)
                        {
                            if (fixField(variableName.toLower()) == fixField(mainField.toLower()))
                            {
                                log("Error: Primary ID : " + mainField + " cannot be a multi-select variable");
                                return 1;
                            }

                            //Processing multiselects
                            QList<TlkpValue> values;
                            values.append(getSelectValues(choicesSheet,
                                                          variableType.split(" ",QString::SkipEmptyParts)[1].trimmed(),
                                          columnListName,columnChoiceName));

                            //Processing  the field
                            TfieldDef aField;
                            aField.name = fixField(variableName.toLower());
                            aField.type = "varchar";
                            aField.size = getMaxMSelValueLength(values);
                            aField.decSize = 0;
                            aField.key = false;
                            aField.isMultiSelect = true;
                            aField.multiSelectTable = fixField(tables[tblIndex].name) + + "_msel_" + fixField(variableName.toLower());
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
                                for (int field = 0; field < tables[tblIndex].fields.count()-1;field++)
                                {
                                    if (tables[tblIndex].fields[field].key == true)
                                    {
                                        TfieldDef relField;
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
                                mselKeyField.desc.append(aField.desc);
                                mselKeyField.key = true;
                                if (areValuesStrings(values))
                                    mselKeyField.type = "varchar";
                                else
                                    mselKeyField.type = "int";
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
                        }
                    }
                }
            }
        }
        if (!mainFieldFound)
        {
            log("ERROR!: The main variable \"" + mainField + "\" was not found in the ODK. Please indicate a correct main variable");
            return 1;
        }
        if (!mainFieldinMainTable)
        {
            log("ERROR!: The main variable \"" + mainField + "\" is not in the main table \"" + mainTable + "\". Please indicate a correct main variable.");
            return 1;
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

// Separate a table into different subtables.
// This because MySQL cant handle a maximum of 64 reference tables or a row structure of more than 65,535 bytes.

// We put a cap of 100 fiels in a table.
// Basically it is ridiculous to have a table with more the 100 variable and consider it normalized.
// We force the user to think and come with common (normalized) groups of data

// This is one reason fo creating a manifest file.
int separateTable(TtableDef &table, QDomNode groups)
{
    //log("separateTable");
    QStringList sections;
    int pos;
    int pos2;
    QString grpName;
    QDomNode temp;
    QDomNode field;
    temp = groups;
    //Get all the different sections
    bool mainFound;
    mainFound = false;

    while (!temp.isNull())
    {
        grpName = temp.toElement().attribute("name","unknown");
        if (grpName == "main")
            mainFound = true;
        if (temp.childNodes().count() > 100)
            log("WARNING!: " + grpName + " has more than 200 fields. This might end up in further separation");
        if (sections.indexOf(grpName) < 0)
            sections.append(grpName);
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
        if (sections[pos] != "main")
        {
            tableIndex++;
            TtableDef ntable;
            ntable.name = table.name + "_" + sections[pos];

            for (int lang = 0; lang < languages.count()-1; lang++)
            {
                TlngLkpDesc langDesc;
                langDesc.langCode = languages[lang].code;
                langDesc.desc = "Subsection " + sections[pos] + " of " + table.name + " - Edit this description";
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
                        table.fields.removeAt(fieldIndex); //Removes the field from main
                    }
                }
                field = field.nextSibling();
            }
            tables.append(ntable);
        }
        temp = temp.nextSibling();
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
            log("Separating table: " + tables[tblIndex].name);
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
    for (int pos = 0; pos <= table.fields.count()-1;pos++)
    {
        if (table.fields[pos].key == false)
        {
            QDomElement fnode;
            fnode = doc.createElement("field");
            fnode.setAttribute("name",table.fields[pos].name);
            fnode.setAttribute("xmlcode",table.fields[pos].xmlCode);
            gnode.appendChild(fnode);
        }
    }
    tnode.appendChild(gnode);
    return tnode;
}

// This function check the tables. If the table has more than 60 relationships creates a separation file using UUID as file name
// The separation file then can be used as an input parameter to separate the tables into sections
int checkTables()
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
            if (rfcount > 60)
                log(tables[pos].name + " has more than 60 relationships");
            if (fcount > 100)
                log(tables[pos].name + " has more than 100 fields.");
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
        QFile file(fname + ".xml");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&file);
            out.setCodec("UTF-8");
            outputdoc.save(out,1,QDomNode::EncodingFromTextStream);
            file.close();
        }
        QString msg;
        msg = QString::number(count) + " tables have more than 60 relationships or 100 fields. A separation file (";
        msg = msg + fname + ".xml) was created. Edit this file and separate the fields in logical groups of data that share something in common.";
        msg = msg + "For example if such table contains variables about crops, livestock, household members, health,etc create a group for each set of variables";
        msg = msg + "in the separation file";
        log(msg);
        msg = "Once you are done run the same command with -s '/where/my/separation/file/is/separationFile.xml' to separate the table(s) in sections.";
        log(msg);
        return 1;
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
    title = title + " * ODKToMySQL 1.0                                                    * \n";
    title = title + " * This tool generates a MySQL schema from a ODK XLSX survey file    * \n";
    title = title + " * Though ODK Aggregate can store data in MySQL such repository      * \n";
    title = title + " * is for storage only. The resulting schema from Aggregate is not   * \n";
    title = title + " * relational or easy for analysis.                                  * \n";
    title = title + " * ODKtoMySQL generates a full relational MySQL database from a      * \n";
    title = title + " * ODK XLSX file.                                                    * \n";
    title = title + " * This tool is part of ODK Tools (c) ILRI-RMG, 2014-15              * \n";
    title = title + " * Author: Carlos Quiros (c.f.quiros@cgiar.org / cquiros@qlands.com) * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.0");

    TCLAP::ValueArg<std::string> inputArg("x","inputXLSX","Input ODK XLSX survey file",true,"","string");
    TCLAP::ValueArg<std::string> tableArg("t","mainTable","Name of the master table for the target schema. ODK surveys do not have a master table however this is neccesary to store ODK variables that are not inside a repeat. Please give a name for the master table for maintable, mainmodule, coverinformation, etc.",true,"","string");
    TCLAP::ValueArg<std::string> mainVarArg("v","mainVariable","Code of the main variable of the ODK survey. For example HH_ID",true,"","string");
    TCLAP::ValueArg<std::string> ddlArg("c","outputdml","Output DDL file. Default ./create.sql",false,"./create.sql","string");
    TCLAP::ValueArg<std::string> XMLCreateArg("C","xmlschema","Output XML schema file. Default ./create.xml",false,"./create.xml","string");
    TCLAP::ValueArg<std::string> insertArg("i","outputinsert","Output insert file. Default ./insert.sql",false,"./insert.sql","string");
    TCLAP::ValueArg<std::string> insertXMLArg("I","xmlinsert","Output lookup values in XML format. Default ./insert.xml",false,"./insert.xml","string");
    TCLAP::ValueArg<std::string> metadataArg("m","outputmetadata","Output metadata file. Default ./metadata.sql",false,"./metadata.sql","string");
    TCLAP::ValueArg<std::string> impxmlArg("f","outputxml","Output xml manifest file. Default ./manifest.xml",false,"./manifest.xml","string");
    TCLAP::ValueArg<std::string> prefixArg("p","prefix","Prefix for each table. _ is added to the prefix. Default no prefix",false,"","string");
    TCLAP::ValueArg<std::string> sepArg("s","separationfile","Separation file to use",false,"","string");
    TCLAP::ValueArg<std::string> langArg("l","otherlanguages","Other languages. For example: (en)English,(es)Español. Required if ODK form has multiple languages",false,"","string");
    TCLAP::ValueArg<std::string> defLangArg("d","deflanguage","Default language. For example: (en)English. If not indicated then English will be asumed",false,"(en)English","string");
    TCLAP::ValueArg<std::string> transFileArg("T","translationfile","Output translation file",false,"./iso639.sql","string");
    TCLAP::ValueArg<std::string> uuidFileArg("u","uuidfile","Output UUID trigger file",false,"./uuid-triggers.sql","string");
    TCLAP::ValueArg<std::string> yesNoStringArg("y","yesnostring","Yes and No strings in the default language in the format \"String|String\". This will allow the tool to identify Yes/No lookup tables and exclude them. This is not case sensitive. For example, if the default language is Spanish this value should be indicated as \"Si|No\". If empty English \"Yes|No\" will be assumed",false,"Yes|No","string");

    debug = false;

    cmd.add(inputArg);
    cmd.add(XMLCreateArg);
    cmd.add(tableArg);
    cmd.add(mainVarArg);
    cmd.add(ddlArg);
    cmd.add(insertArg);
    cmd.add(insertXMLArg);
    cmd.add(metadataArg);
    cmd.add(impxmlArg);
    cmd.add(prefixArg);
    cmd.add(sepArg);
    cmd.add(langArg);
    cmd.add(defLangArg);
    cmd.add(transFileArg);
    cmd.add(uuidFileArg);
    cmd.add(yesNoStringArg);

    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    QString input = QString::fromUtf8(inputArg.getValue().c_str());
    QString ddl = QString::fromUtf8(ddlArg.getValue().c_str());
    QString insert = QString::fromUtf8(insertArg.getValue().c_str());
    QString insertXML = QString::fromUtf8(insertXMLArg.getValue().c_str());
    QString metadata = QString::fromUtf8(metadataArg.getValue().c_str());
    QString xmlFile = QString::fromUtf8(impxmlArg.getValue().c_str());
    QString xmlCreateFile = QString::fromUtf8(XMLCreateArg.getValue().c_str());
    QString mTable = QString::fromUtf8(tableArg.getValue().c_str());
    QString mainVar = QString::fromUtf8(mainVarArg.getValue().c_str());
    QString sepFile = QString::fromUtf8(sepArg.getValue().c_str());
    QString lang = QString::fromUtf8(langArg.getValue().c_str());
    QString defLang = QString::fromUtf8(defLangArg.getValue().c_str());
    QString transFile = QString::fromUtf8(transFileArg.getValue().c_str());
    QString UUIDFile = QString::fromUtf8(uuidFileArg.getValue().c_str());
    QString yesNoString = QString::fromUtf8(yesNoStringArg.getValue().c_str());
    prefix = QString::fromUtf8(prefixArg.getValue().c_str());
    prefix = prefix + "_";

    strYes = "Yes";
    strNo = "No";

    if (prefix == "_")
        prefix = "";

    tableIndex = 0;

    if (addLanguage(defLang,true) != 0)
        return 1;
    
    QStringList othLanguages;
    othLanguages = lang.split(",",QString::SkipEmptyParts);
    for (int lng = 0; lng < othLanguages.count(); lng++)
        if (addLanguage(othLanguages[lng],false) != 0)
            return 1; 
    
    if (isDefaultLanguage("English") == false)
    {
        if (yesNoString == "")
        {
            log("English is not the default language. You need to specify Yes and No values with the -y parameter in " + getDefLanguage());
            return 1;
        }
        if (yesNoString.indexOf("|") == -1)
        {
            log("Malformed yes|no language string1");
            return 1;
        }
        if (yesNoString.length() == 1)
        {
            log("Malformed yes|no language string2");
            return 1;
        }
        strYes = yesNoString.split("|",QString::SkipEmptyParts)[0];
        strNo = yesNoString.split("|",QString::SkipEmptyParts)[1];
    }
    else
    {
        strYes = yesNoString.split("|",QString::SkipEmptyParts)[0];
        strNo = yesNoString.split("|",QString::SkipEmptyParts)[1];
    }

    if (processXLSX(input,mTable.trimmed(),mainVar.trimmed()) == 0)
    {
        //dumpTablesForDebug();
        if (sepFile != "") //If we have a separation file
        {
            log("Separating tables");
            QFile file(sepFile);
            if (file.exists())
            {
                if (separeteTables(sepFile) == 1) //Separate the tables using the file
                    return 1;
            }
        }
        appendUUIDs();
        log("Checking tables");
        if (checkTables() == 1) //Check the tables to see if they have less than 60 related fields. If so a separation file is created
        {
            return 1; //If a table has more than 60 related field then exit
        }
        log("Generating SQL scripts");
        genSQL(ddl,insert,metadata,xmlFile,transFile,UUIDFile,xmlCreateFile,insertXML);
    }
    else
        return 1;
    log("Done");
    return 0;
}
