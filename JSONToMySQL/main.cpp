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

#include <QCoreApplication>
#include <qjson/parser.h>
#include <tclap/CmdLine.h>
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

QJSEngine JSEngine;
QJSValue beforeInsertFunction;
bool callBeforeInsert;

struct tblIndexDef
{
    QString table;
    int index;
};
typedef tblIndexDef TtblIndexDef;

QList <TtblIndexDef > lstTblIndex;

int getLastIndex(QString table)
{
    int pos;

    int idx;
    idx = -1;
    //Look for the table in the list
    for (pos = 0; pos <= lstTblIndex.count()-1;pos++)
    {
        if (lstTblIndex[pos].table == table)
        {
            idx = pos;
            break;
        }
    }

    if (idx == -1) //The table does not exists then add it to the list and return 1
    {
        TtblIndexDef newTable;
        newTable.table = table;
        newTable.index = 1;
        lstTblIndex.append(newTable);
        return 1;
    }
    else
    {
        //Increase the index of the table and return it
        lstTblIndex[idx].index++;
        return lstTblIndex[idx].index;
    }
}

struct fieldDef
{
  QString name;
  QString xmlCode;
  QString value;
  bool key;
  bool multiSelect;
  QString multiSelectTable;
};
typedef fieldDef TfieldDef;



struct tableKey
{
    int index;
    QList< TfieldDef> keys;
};
typedef tableKey TtableKey;

void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf(temp.toLocal8Bit().data());
}


bool SQLError;
QString fileID;
QString mainTable;

QFile logFile;
QTextStream logStream;

bool outSQL;
QFile sqlFile;
QTextStream sqlStream;

//This function returns a the xmlFieldCode based on its MySQL name
QString getXMLCodeFromField(QList< TfieldDef> fields, QString field)
{
    for (int pos = 0; pos <= fields.count()-1;pos++)
    {
        if (fields[pos].name == field)
            return fields[pos].xmlCode;
    }
    return "Unknown";
}

//This procedure creates log entries into the log file. It used the dictionary tables to retrive a more understanable message.
void logError(QSqlDatabase db,QString errorMessage, QString table, int rowNumber,QVariantMap jsonData,QList< TfieldDef> fields, QString execSQL)
{
    int idx;
    idx = errorMessage.indexOf("CONSTRAINT");
    if (idx >= 0)
    {
        int idx2;
        idx2 = errorMessage.indexOf("FOREIGN KEY");
        QString cntr;
        cntr = errorMessage.mid(idx+11,idx2-(idx+11));
        cntr = cntr.replace("`","");
        cntr = cntr.simplified();

        QSqlQuery query(db);
        QString sql;
        QString field;
        sql = "SELECT error_msg,error_notes,clm_cod FROM dict_relinfo WHERE cnt_name = '" + cntr + "'";
        if (query.exec(sql))
        {
            if (query.first())
            {
                errorMessage = query.value(0).toString();
                field = query.value(2).toString();
                if (getXMLCodeFromField(fields,field) != "Unknown")
                    logStream << fileID + "\t" + table + "\t" + QString::number(rowNumber) + "\t" + getXMLCodeFromField(fields,field) + "\t" + errorMessage + "\tValue not found = " + jsonData[getXMLCodeFromField(fields,field)].toString() + "\t" + execSQL + "\n";
                else
                    logStream << fileID + "\t" + table + "\t" + QString::number(rowNumber) + "\t\t" + errorMessage + "\t\t" + execSQL + "\n";
                return;
            }
        }
    }
    logStream << fileID + "\t" + table + "\t" + QString::number(rowNumber) + "\t\t" + errorMessage + "\t\t" + execSQL + "\n";
}

void logErrorMSel(QSqlDatabase db,QString errorMessage, QString table, int rowNumber,QString value, QString execSQL)
{
    int idx;
    idx = errorMessage.indexOf("CONSTRAINT");
    if (idx >= 0)
    {
        int idx2;
        idx2 = errorMessage.indexOf("FOREIGN KEY");
        QString cntr;
        cntr = errorMessage.mid(idx+11,idx2-(idx+11));
        cntr = cntr.replace("`","");
        cntr = cntr.simplified();

        QSqlQuery query(db);
        QString sql;
        QString field;
        sql = "SELECT error_msg,error_notes,clm_cod FROM dict_relinfo WHERE cnt_name = '" + cntr + "'";
        if (query.exec(sql))
        {
            if (query.first())
            {
                errorMessage = query.value(0).toString();
                field = query.value(2).toString();
                logStream << fileID + "\t" + table + "\t" + QString::number(rowNumber) + "\t" + field + "\t" + errorMessage + "\tValue not found = " + value + "\t" + execSQL + "\n";
                return;
            }
        }
    }
    logStream << fileID + "\t" + table + "\t" + QString::number(rowNumber) + "\t\t" + errorMessage + "\t\t" + execSQL + "\n";
}


QString fixString(QString source)
{
    QString res;
    res = source;
    res = res.replace("'","`");
    res = res.replace(";","");
    return res;
}

QVariantMap emptyMap;

//This function construct an INSERT SQL and execute it againts the database.
QList<TfieldDef > createSQL(QSqlDatabase db,QVariantMap jsonData,QString table,QList< TfieldDef> fields,QList< TfieldDef> parentkeys,bool mTable = false, QVariantMap jsonData2 = emptyMap)
{
    QString sqlHeader;
    QString sqlValues;
    QString sql;
    QVariant variantValue;
    int pos;

    QList<TfieldDef > resKeys;

    QString fieldValue;
    insertValues insertObject;


    int tblIndex;

    //log("Table:" + table);

    tblIndex = getLastIndex(table);

    sqlHeader = "INSERT INTO " + table + " (";
    sqlValues = " VALUES (";
    for (pos = 0; pos <= parentkeys.count()-1;pos++)
    {
        //sqlHeader = sqlHeader + parentkeys[pos].name + ",";
        //sqlValues = sqlValues + "'" + parentkeys[pos].value + "',";
        TinsertValueDef insValue;
        insValue.key = true;
        insValue.name = parentkeys[pos].name;
        insValue.value = parentkeys[pos].value;
        insValue.xmlCode = parentkeys[pos].xmlCode;
        insValue.multiSelect = parentkeys[pos].multiSelect;
        insValue.multiSelectTable = parentkeys[pos].multiSelectTable;
        insValue.insert = true;
        insertObject.insertValue(insValue);
    }
    for (pos = 0; pos <= fields.count()-1;pos++)
    {
        // If a new key is found in the list of fields
        // then we added to the result of keys that will be passes to any possible child table
        if (fields[pos].key == true)
        {
            TfieldDef key;
            key.key = true;
            key.name = fields[pos].name;
            key.xmlCode = fields[pos].xmlCode;
            key.multiSelect = fields[pos].multiSelect;
            key.multiSelectTable = fields[pos].multiSelectTable;
            key.value = jsonData[fields[pos].xmlCode].toString();
            key.value = key.value.simplified();

            // If its empty. The try to find it in jsonData2
            // This happens when the cover information is stored in a repeat of one
            // so part of the information for the main table must be searched in jsonData
            // and part in jsonData2
            if (key.value.isEmpty())
            {
                key.value = jsonData2[fields[pos].xmlCode].toString();
                key.value = key.value.simplified();
            }

            //If the key is empty (Normal as in the JSON such key does not exist) set the key value to tblIndex
            if (key.value.isEmpty())
            {                                
                if (!mTable)
                {
                    key.value = QString::number(tblIndex);
                    //sqlHeader = sqlHeader + key.name + ",";
                    //sqlValues = sqlValues + "'" + QString::number(tblIndex) + "',";
                }
                else
                {
                    key.value = "";
                    //sqlHeader = sqlHeader + key.name + ",";
                    //sqlValues = sqlValues + "'',";
                }
            }
            else
            {                
                //sqlHeader = sqlHeader + key.name + ",";
                //sqlValues = sqlValues + "'" + fixString(key.value) + "',";
            }
            //Append the key to the list of returned keys
            //key.value = key.value;

            resKeys.append(key);
            insertObject.insertValue(key.name,key.xmlCode,key.value,key.key,false,key.multiSelectTable);
        }
        else
        {
            TinsertValueDef insValue;
            insValue.key = false;
            insValue.name = fields[pos].name;
            insValue.xmlCode = fields[pos].xmlCode;
            insValue.insert = true;
            insValue.multiSelect = fields[pos].multiSelect;
            insValue.multiSelectTable = fields[pos].multiSelectTable;

            //sqlHeader = sqlHeader + fields[pos].name + ",";
            if (mainTable == table)
            {
                if (fields[pos].name == "surveyid")
                {
                  //sqlValues = sqlValues + "'" + fileID + "',";
                  fieldValue = fileID;
                }
                else
                {
                    if (fields[pos].name == "originid")
                    {
                        //sqlValues = sqlValues + "'FORMHUB-JSON',";
                        fieldValue = "FORMHUB-JSON";
                    }
                    else
                    {
                        fieldValue = fixString(jsonData[fields[pos].xmlCode].toString());
                        if (fieldValue.isEmpty())
                        {
                            // This happens when the cover information is stored in a repeat of one
                            // so part of the information for the main table must be searched in jsonData
                            // and part in jsonData2                            
                            fieldValue = fixString(jsonData2[fields[pos].xmlCode].toString());
                        }
                        //sqlValues = sqlValues + "'" + fieldValue + "',";
                    }
                }
            }
            else
            {                                
                variantValue =  jsonData[fields[pos].xmlCode];                
                fieldValue = fixString(QString::fromUtf8(variantValue.toByteArray()));
                if (fieldValue.isEmpty())
                {
                    // This happens when the cover information is stored in a repeat of one
                    // so part of the information for the main table must be searched in jsonData
                    // and part in jsonData2
                    variantValue = jsonData2[fields[pos].xmlCode];                    
                    fieldValue = fixString(QString::fromUtf8(variantValue.toByteArray()));
                }

                //sqlValues = sqlValues + "'" + fixString(fieldValue) + "',";
            }
            insValue.value = fieldValue;            
            insertObject.insertValue(insValue);
        }
    }

    //We have all the insert values, Now we pass such values along with the table name to the external javaScript if its defined to allow custom modifications
    //to the values before we insert them into the mysql database
    if (callBeforeInsert)
    {
        QJSValue insertListObj = JSEngine.newQObject(&insertObject);
        QJSValue result = beforeInsertFunction.call(QJSValueList() << table << insertListObj);
        if (result.isError())
        {
            log("Error calling BeforInsert JS function with table " + table + ". The insert may end up in error. The script will not be loaded again.");
            callBeforeInsert = false;
        }
    }

    for (pos = 0; pos < insertObject.count();pos++)
    {
        sqlHeader = sqlHeader + insertObject.itemName(pos) + ",";
        sqlValues = sqlValues + "'" + insertObject.itemValue(pos) + "',";

        // Key values could have changed by the external JavaScript so
        // update resKeys with the current values
        if (insertObject.itemIsKey(pos) == true)
        {
            for (int rkey = 0; rkey < resKeys.count(); rkey++)
            {
                if (resKeys[rkey].name.toLower().simplified() == insertObject.itemName(pos).toLower().simplified())
                {
                    resKeys[rkey].value = insertObject.itemValue(pos);
                }
            }
        }
    }
    //Removing final , and appending )
    sqlHeader = sqlHeader.left(sqlHeader.length()-1) + ")";
    sqlValues = sqlValues.left(sqlValues.length()-1) + ")";
    //Create the final sql
    sql = sqlHeader + " " + sqlValues;
    //Change all empty valued to NULL. This minimize foreign key errors in skips
    sql = sql.replace("''","NULL");

    //Execute the SQL to the database

    if (outSQL)
    {
        sqlStream << sql + ";\n";
    }
    QSqlQuery query(db);

    if (!query.exec(sql))
    {
      SQLError = true; //An error occurred. This will trigger a rollback
      logError(db,query.lastError().databaseText(),table,tblIndex,jsonData,fields,sql); //Write the error to the log
    }


    //Now we process the MultiSelects
    QList<TinsertValueDef> currentKeys;
    currentKeys.append(insertObject.getKeys());

    QString mSelectTableName;
    QStringList mSelectValues;
    int nkey;
    int nvalue;
    for (pos = 0; pos < insertObject.count();pos++)
    {
        if (insertObject.itemIsMultiSelect(pos) == true)
        {
            mSelectTableName = insertObject.itemMultiSelectTable(pos);
            //qDebug() << "MSelField: " + insertObject.itemName(pos);
            //qDebug() << "MSelTable: " + mSelectTableName;

            mSelectValues.clear();
            //Split the values into a stringlist
            mSelectValues.append(insertObject.itemValue(pos).simplified().split(" ",QString::SkipEmptyParts));
            //Process each value
            for (nvalue = 0; nvalue < mSelectValues.count();nvalue++)
            {
                insertValues multiSelectObject; //Each value creates a new multiSelectObject
                //Insert the current keys to the multiSelectObject object
                for (nkey = 0; nkey < currentKeys.count();nkey++)
                {
                    multiSelectObject.insertValue(currentKeys[nkey]);
                }
                //Insert the value to the multiSelectObject
                multiSelectObject.insertValue(insertObject.itemName(pos),"",mSelectValues[nvalue],true);

                //Here we should pass the value to the JavaScript function

                if (callBeforeInsert)
                {
                    QJSValue insertListObj = JSEngine.newQObject(&multiSelectObject);
                    QJSValue result = beforeInsertFunction.call(QJSValueList() << mSelectTableName << insertListObj);
                    if (result.isError())
                    {
                        log("Error calling BeforInsert JS function with table " + mSelectTableName + ". The insert may end up in error. The script will not be loaded again.");
                        callBeforeInsert = false;
                    }
                }

                //Create the insert
                sqlHeader = "INSERT INTO " + mSelectTableName + "(";
                sqlValues = " VALUES (";

                for (nkey = 0; nkey < multiSelectObject.count();nkey++)
                {
                    sqlHeader = sqlHeader + multiSelectObject.itemName(nkey) + ",";
                    sqlValues = sqlValues + "'" + multiSelectObject.itemValue(nkey) + "',";
                }
                sqlHeader = sqlHeader.left(sqlHeader.length()-1) + ")";
                sqlValues = sqlValues.left(sqlValues.length()-1) + ")";

                //Create the final sql
                sql = sqlHeader + " " + sqlValues;
                //Change all empty valued to NULL. This minimize foreign key errors in skips
                sql = sql.replace("''","NULL");

                if (outSQL)
                {
                    sqlStream << sql + ";\n";
                }

                if (!query.exec(sql))
                {
                  SQLError = true; //An error occurred. This will trigger a rollback
                  logErrorMSel(db,query.lastError().databaseText(),mSelectTableName,tblIndex,mSelectValues[nvalue],sql); //Write the error to the log
                }
            }
        }
    }

    return resKeys;
}

void debugKeys(QString table, QList< TfieldDef> keys)
{
    qDebug() << "***" + table;
    qDebug() << "Total keys: " + QString::number(keys.count());
    for (int pos = 0; pos <= keys.count()-1;pos++)
    {
        qDebug() << "Field:" + keys[pos].name + ". Value: " + keys[pos].value;
    }

}

void debugMap(QVariantMap jsonData)
{

    QList<QString > keys;
    keys = jsonData.keys();
    qDebug() << "-----------------------------------------";
    for (int pos = 0; pos <= keys.count()-1;pos++)
    {
        qDebug() << keys[pos] << " : " << jsonData.value(keys[pos]).toString();
    }
}



//Process a table in the manifest. This fuction is recursive
int procTable(QSqlDatabase db,QVariantMap jsonData, QDomNode table, QList< TfieldDef> parentkeys)
{
    QList< TfieldDef> keys;
    QList< TfieldDef> tkeys;
    QList< TtableKey> tableKeys;
    keys.append(parentkeys);

    QList< TfieldDef> fields;

    bool sqlCreated;
    sqlCreated = false;

    QString tableCode;
    bool tableSeparated;
    tableCode = table.toElement().attribute("mysqlcode");

    int recordIndex;
    int tkindex;

    QString tableXMLCode;
    tableXMLCode = table.toElement().attribute("xmlcode");

    QString parentTag;
    parentTag = table.parentNode().toElement().tagName();

    bool genSQL;
    genSQL = false;

    QDomNode child;
    child = table.firstChild();

    while (!child.isNull())
    {
        if (child.toElement().nodeName() == "field")
        {
            //We not process referenced fields because they come as part of the key
            if (child.toElement().attribute("reference") == "false")
            {
                TfieldDef field;
                field.name = child.toElement().attribute("mysqlcode");
                field.xmlCode = child.toElement().attribute("xmlcode");

                if (child.toElement().attribute("key").toStdString() == "true")
                    field.key = true;
                else
                    field.key = false;
                if (child.toElement().attribute("isMultiSelect","false") == "true")
                {
                    field.multiSelect = true;
                    field.multiSelectTable = child.toElement().attribute("multiSelectTable","");
                }
                else
                    field.multiSelect = false;

                fields.append(field); //Append the field to the list of fields
            }
            genSQL = true;
        }
        else
        {
            sqlCreated = true; //To control more than one child table
            if ((tableXMLCode == "main") || (parentTag == "ODKImportXML"))
            {
                mainTable = tableCode;
                QVariantMap map;
                if (genSQL == true)
                {
                    if (tableXMLCode == "main")
                        keys.append(createSQL(db,jsonData,tableCode,fields,keys,true));
                    else
                    {
                        // if we are processing the first table and is not main, this means that a repeat of one was used
                        // to store the cover data. Therefore the insert SQL must use both the information on root (jsonData)
                        // and the information on the repeat of one (map)
                        QVariantList result = jsonData[tableXMLCode].toList();
                        foreach(QVariant record, result)
                        {
                            map = record.toMap();
                            keys.append(createSQL(db,jsonData,tableCode,fields,keys,true,map));
                        }
                    }
                    genSQL = false;
                }
                else
                {
                    if (tableXMLCode != "main") //If we are processing a sibling table and the main table is a repeat then we need to get map
                    {
                        QVariantList result = jsonData[tableXMLCode].toList();
                        foreach(QVariant record, result)
                        {
                            map = record.toMap();
                        }
                    }
                }
                if (tableXMLCode == "main")
                    procTable(db,jsonData,child,keys); //Recursive call of a table from main using a main table as root
                else
                    procTable(db,map,child,keys); // //Recursive call of a table from main using a main repeat as root
            }
            else
            {
                if (child.toElement().attribute("separated","false") == "true")
                    tableSeparated = true;
                else
                    tableSeparated = false;

                //If its not main then we need to get the json list of items

                QVariantList result = jsonData[tableXMLCode].toList();
                //We need to generate an SQL for each item
                //tblIndex = 0;
                recordIndex = 0;
                foreach(QVariant record, result)
                {
                    recordIndex++;

                    QVariantMap map = record.toMap();
                    tkeys.clear();                    
                    tkeys.append(keys);

                    //Tries to find if this record already has a key allocated
                    tkindex = -1;
                    for (int pos = 0; pos <= tableKeys.count()-1;pos++)
                    {
                        if (tableKeys[pos].index == recordIndex)
                            tkindex = pos;
                    }

                    if (tkindex == -1) //If no key allocated then insert it ad store the key for the record
                    {
                        tkeys.append(createSQL(db,map,tableCode,fields,tkeys,false));
                        TtableKey tableKey;
                        tableKey.index = recordIndex;
                        tableKey.keys.append(tkeys);
                        tableKeys.append(tableKey);
                    }
                    else //If found then
                    {
                        tkeys.clear();
                        tkeys.append(tableKeys[tkindex].keys);
                    }

                    if (tableSeparated == false)
                        procTable(db,map,child,tkeys); //Recursive call of a table from other table than main that is a real child and not a separation table
                    else
                        procTable(db,jsonData,child,tkeys); //Recursive call of a table from other table than main that is a separation table
                }
            }
        }
        child = child.nextSibling();
    }
    //If the table does not have any subtables then insert the table
    if (!sqlCreated)
    {
        if ((tableXMLCode == "main") || (table.parentNode().toElement().tagName() == "ODKImportXML"))
        {
            mainTable = tableCode;
            if (tableXMLCode == "main")
                createSQL(db,jsonData,tableCode,fields,keys,true);
            else
            {
                // if we are processing the first table and is not main, this means that a repeat of one was used
                // to store the cover data. Therefore the insert SQL must use both the information on root (jsonData)
                // and the information on the repeat of one (map)
                QVariantList result = jsonData[tableXMLCode].toList();
                foreach(QVariant record, result)
                {
                    QVariantMap map = record.toMap();
                    keys.append(createSQL(db,jsonData,tableCode,fields,keys,true,map));
                }
            }
        }
        else
        {
            //If its not main then we need to get the json list of items
            QVariantList result = jsonData[tableXMLCode].toList();
            //We need to generate an SQL for each item
            //tblIndex = 0; //Set to zero so each item will start in 1
            foreach(QVariant record, result)
            {
                QVariantMap map = record.toMap();
                createSQL(db,map,tableCode,fields,keys,false);
            }
        }
    }
    return 0;
}

//This process load a JSON file to a VariantMap using QJSON library
int processFile(QSqlDatabase db, QString json, QString manifest, QStringList procList)
{
    QFileInfo fi(json);
    //If the file hasn't been processed yet
    if (procList.indexOf(fi.baseName()) < 0)
    {
        fileID = fi.baseName();

        QFile ijson(json);
        if (!ijson.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            log("Cannot open JSON file: " + json);
            return 1;
        }

        QJson::Parser parser;
        bool ok;
        //Parse the JSON File into a QVariantMap
        QVariantMap jsonData = parser.parse(&ijson, &ok).toMap();
        if (!ok)
        {
            log("Error parsing JSON:" + json);
            return 1;
        }

        //Opens the Manifest File
        QDomDocument doc("mydocument");
        QFile xmlfile(manifest);
        if (!xmlfile.open(QIODevice::ReadOnly))
        {
            log("Error reading manifest file");
            return 1;
        }
        if (!doc.setContent(&xmlfile))
        {
            log("Error reading manifest file");
            xmlfile.close();
            return 1;
        }
        xmlfile.close();

        //Gets the first table of the file
        QDomNode root;
        root = doc.firstChild().nextSibling().firstChild();

        //Process the table with no parent Keys
        QList< TfieldDef> noParentKeys;
        procTable(db,jsonData,root,noParentKeys);


        /*QVariantList result = jsonData["BM/rpt_secg1_sheep"].toList();
        foreach(QVariant record, result)
        {
            QVariantMap map = record.toMap();
            log(map["BM/rpt_secg1_sheep/g1shp"].toString());
        }*/

    }
    else
    {
        log("File " + json + " has been already processed. Skipped");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * JSON to MySQL 1.5                                                 * \n";
    title = title + " * This tool imports JSON data from FormHub into MySQL.              * \n";
    title = title + " * The JSON input files are generated from 'mongotojson'.            * \n";
    title = title + " * The tool uses the import manifest file generated by 'odktomysql'  * \n";
    title = title + " * to import the JSON data into the neccesary MySQL tables.          * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is part of ODK Tools (c) ILRI-RMG, 2014                 * \n";
    title = title + " * Author: Carlos Quiros (c.f.quiros@cgiar.org / cquiros@qlands.com) * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.5");

    TCLAP::ValueArg<std::string> jsonArg("j","json","Input JSON File",true,"","string");
    TCLAP::ValueArg<std::string> manifestArg("m","manifest","Input manifest XML file",true,"","string");
    TCLAP::ValueArg<std::string> hostArg("H","host","MySQL Host. Default: localhost",false,"localhost","string");
    TCLAP::ValueArg<std::string> portArg("P","port","MySQL port. Default: 3306",false,"3306","string");
    TCLAP::ValueArg<std::string> userArg("u","user","MySQL User",true,"","string");
    TCLAP::ValueArg<std::string> passArg("p","password","MySQL Password",true,"","string");
    TCLAP::ValueArg<std::string> schemaArg("s","schema","MySQL Schema",true,"","string");
    TCLAP::ValueArg<std::string> outputArg("o","output","Output Log file",false,"./output.csv","string");
    TCLAP::ValueArg<std::string> inputArg("i","imported","Imported file. Store the files names properly imported. Also used to skip repeated files",false,"./imported.log","string");
    TCLAP::ValueArg<std::string> JSArg("J","javascript","Custom Before Insert JavaScript",false,"","string");
    TCLAP::SwitchArg overwriteSwitch("w","overwrite","Overwrite the log file", cmd, false);
    TCLAP::SwitchArg oputSQLSwitch("S","outputSQL","Output each insert SQL to ./inputfile.json.sql", cmd, false);

    //These two parameters should be removed once the external script code works

    //TCLAP::SwitchArg ignoreSwitch("g","ignore","Ignore insert in main table", cmd, false);
    //TCLAP::SwitchArg extractSwitch("e","extract","Extract number from primary key", cmd, false);

    cmd.add(jsonArg);
    cmd.add(manifestArg);
    cmd.add(hostArg);
    cmd.add(portArg);
    cmd.add(userArg);
    cmd.add(passArg);
    cmd.add(schemaArg);
    cmd.add(outputArg);
    cmd.add(inputArg);
    cmd.add(JSArg);

    //Parsing the command lines
    cmd.parse( argc, argv );

    sqlStream.setCodec("UTF-8");

    //Getting the variables from the command
    bool overwrite = overwriteSwitch.getValue();
    outSQL = oputSQLSwitch.getValue();
    QString json = QString::fromUtf8(jsonArg.getValue().c_str());
    QString manifest = QString::fromUtf8(manifestArg.getValue().c_str());
    QString host = QString::fromUtf8(hostArg.getValue().c_str());
    QString port = QString::fromUtf8(portArg.getValue().c_str());
    QString user = QString::fromUtf8(userArg.getValue().c_str());
    QString password = QString::fromUtf8(passArg.getValue().c_str());
    QString schema = QString::fromUtf8(schemaArg.getValue().c_str());
    QString output = QString::fromUtf8(outputArg.getValue().c_str());
    QString input = QString::fromUtf8(inputArg.getValue().c_str());
    QString javaScript = QString::fromUtf8(JSArg.getValue().c_str());

    callBeforeInsert = false;
    if (javaScript != "")
    {
        QFile scriptFile(javaScript);
        if (!scriptFile.open(QIODevice::ReadOnly))
        {
            log("Error: Script file defined but cannot be opened");
            return 1;
        }
        QJSValue JSCode = JSEngine.evaluate(scriptFile.readAll(), javaScript);
        /*if (JSCode.isError())
        {
            log("The JavaScript code has errors");
            scriptFile.close();
            return 1;
        }*/
        scriptFile.close();

        insertValues insertObject;

        TinsertValueDef tfield;
        tfield.key = false;
        tfield.name = "tmpfield";
        tfield.xmlCode = "tmpCode";
        tfield.value = "tmpValue";
        tfield.insert = true;
        insertObject.insertValue(tfield);
        QString error;

        beforeInsertFunction = JSEngine.evaluate("beforeInsert",error);

        if (!beforeInsertFunction.isError())
        {
            QJSValue insertListObj = JSEngine.newQObject(&insertObject);
            QJSValue result = beforeInsertFunction.call(QJSValueList() << "tmpTable" << insertListObj);
            if (result.isError())
            {
                log("Error calling BeforInsert JS function.");
                return 1;
            }
            else
            {
                log("JavaScript BeforInsert function is seems to be ok");
                callBeforeInsert = true;
            }
        }
        else
        {
            log("Error evaluating BeforInsert JS function. [" + error + "]");
            return 1;
        }
    }

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
        db.setHostName(host);
        db.setPort(port.toInt());
        db.setDatabaseName(schema);
        db.setUserName(user);
        db.setPassword(password);
        if (db.open())
        {
            QStringList procList;
            QFile file(input);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            {

                QTextStream in(&file);
                while (!in.atEnd())
                {
                    QString line = in.readLine();
                    procList.append(line);
                }
                file.close();
            }
            if (!QFile(input).exists())
            {
                //The file is new so write from start
                if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    log("Cannot create processing file");
                    return 1;
                }
            }
            else
            {
                //The file exists so append at the end
                if (!file.open(QIODevice::Append | QIODevice::Text))
                {
                    log("Cannot create processing file");
                    return 1;
                }
            }
            QTextStream out(&file); //Stream to the processing file

            //Loggin file
            logFile.setFileName(output);
            if (!QFile::exists(output) || overwrite)
            {
                if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    log("Cannot create log file");
                    return 1;
                }
                logStream.setDevice(&logFile);

                logStream << "FileUUID\tTable\tRowInJSON\tJSONVariable\tError\tNotes\tSQLExecuted\n";
            }
            else
            {
                if (!logFile.open(QIODevice::Append | QIODevice::Text))
                {
                    log("Cannot create log file");
                    return 1;
                }
                logStream.setDevice(&logFile);
            }

            if (!db.transaction())
            {
                log("Database does not support transactions");
                db.close();
                return 1;
            }

            if (outSQL)
            {
                QFileInfo fi(json);

                QString outsqlFile;
                outsqlFile = "./" + fi.fileName() + ".sql";

                sqlFile.setFileName(outsqlFile);
                if (!sqlFile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    log("Cannot create sqlFile file");
                    return 1;
                }
                sqlStream.setDevice(&sqlFile);
            }

            SQLError = false;
            if (processFile(db,json,manifest,procList))
            {
                //This will happen if the file is already processes or an error ocurred
                db.close();
                return 1;
            }
            if (SQLError)
            {
                if (!db.rollback())
                {
                    log("Errors were encountered but Rollback did not succed. The processs might have ended in incomplete data.");
                    db.close();
                    return 1;
                }
            }
            else
            {
                //If no errors were found then write the id to the processing file
                QFileInfo fi(json);
                out << fi.baseName() + "\n";
                file.close();
                //Commit the transaction
                if (!db.commit())
                {
                    log("Warning: Commit did not succed. Please check the database");
                    db.close();
                    return 1;
                }
            }

            db.close();
        }
        else
        {
            log("Cannot connect to database");
            log(db.lastError().databaseText());
            return 1;
        }
    }

    return 0;

}
