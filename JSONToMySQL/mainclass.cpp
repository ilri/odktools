/*
JSONToMySQL.

Copyright (C) 2015-2017 International Livestock Research Institute.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

JSONToMySQL is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

JSONToMySQL is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with JSONToMySQL.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include "mainclass.h"
#include <QUuid>
#include <QFileInfo>
#include <QDomText>

mainClass::mainClass(QObject *parent) : QObject(parent)
{
    returnCode = 0;
}

void mainClass::run()
{
    sqlStream.setCodec("UTF-8");

    recordMap = QDomDocument("ODKRecordMapFile");
    recordMapRoot = recordMap.createElement("ODKRecordMapXML");
    recordMapRoot.setAttribute("version", "1.0");
    recordMap.appendChild(recordMapRoot);

    callBeforeInsert = false;
    if (javaScript != "")
    {
        QFile scriptFile(javaScript);
        if (!scriptFile.open(QIODevice::ReadOnly))
        {
            log("Error: Script file defined but cannot be opened");
            returnCode = 1;

        }
        QJSValue JSCode = JSEngine.evaluate(scriptFile.readAll(), javaScript);
        /*if (JSCode.isError())
        {
            log("The JavaScript code has errors");
            scriptFile.close();
            returnCode = 1;
            emit finished();
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
                returnCode = 1;
                emit finished();
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
            returnCode = 1;
            emit finished();
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
                    returnCode = 1;
                    db.close();
                    emit finished();
                }
            }
            else
            {
                //The file exists so append at the end
                if (!file.open(QIODevice::Append | QIODevice::Text))
                {
                    log("Cannot create processing file");
                    returnCode = 1;
                    db.close();
                    emit finished();
                }
            }
            QTextStream out(&file); //Stream to the processing file
            QDir mapDir(mapOutputDir);
            if (!mapDir.exists())
            {
                if (mapDir.mkpath(mapOutputDir))
                {
                    log("Output map directory does not exist and cannot be created");
                    returnCode = 1;
                    db.close();
                    emit finished();
                }
            }

            //Loggin file
            if (outputType == "h")
            {
                logFile.setFileName(output);
                if (!QFile::exists(output) || overwrite)
                {
                    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text))
                    {
                        log("Cannot create log file");
                        returnCode = 1;
                        db.close();
                        emit finished();
                    }
                    logStream.setDevice(&logFile);
                    logStream << "FileUUID\tTable\tRowInJSON\tJSONVariable\tError\tNotes\tSQLExecuted\n";
                }
                else
                {
                    if (!logFile.open(QIODevice::Append | QIODevice::Text))
                    {
                        log("Cannot create log file");
                        returnCode = 1;
                        db.close();
                        emit finished();
                    }
                    logStream.setDevice(&logFile);
                }
            }
            else
            {
                xmlLog = QDomDocument("XMLErrorLog");
                QDomElement XMLRoot;
                XMLRoot = xmlLog.createElement("XMLErrorLog");
                xmlLog.appendChild(XMLRoot);
                eErrors = xmlLog.createElement("errors");
                XMLRoot.appendChild(eErrors);
            }

            if (!db.transaction())
            {
                log("Database does not support transactions");
                db.close();
                returnCode = 1;
                db.close();
                emit finished();
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
                    returnCode = 1;
                    db.close();
                    emit finished();
                }
                sqlStream.setDevice(&sqlFile);
            }

            SQLError = false;
            SQLErrorNumber = "";
            int processError;
            processError = processFile(db,json,manifest,procList);
            if (processError == 1)
            {
                //This will happen if the file is already processes or an error ocurred
                db.close();
                returnCode = 1;
                emit finished();
            }
            if ((SQLError) || (processError == 1))
            {
                returnCode = 2;
                if (outputType == "h")
                {
                    logFile.close();
                    QFileInfo fiLog(output);
                    if (fiLog.size() == 62)
                    {
                        logFile.remove();
                    }
                }
                else
                {
                    if (!eErrors.firstChild().isNull())
                    {
                        QFile XMLLogFile(output);
                        if (XMLLogFile.open(QIODevice::WriteOnly | QIODevice::Text))
                        {
                            QTextStream strXMLLog(&XMLLogFile);
                            strXMLLog.setCodec("UTF-8");
                            xmlLog.save(strXMLLog,1,QDomNode::EncodingFromTextStream);
                            XMLLogFile.close();
                        }
                        else
                        {
                            db.close();
                            returnCode = 1;
                            emit finished();
                        }
                    }
                }

                if (!db.rollback())
                {
                    db.close();
                    returnCode = 1;
                    emit finished();
                }
            }
            else
            {
                QFileInfo fi(json);
                out << fi.baseName() + "\n";
                file.close();
                if (outputType == "h")
                {
                    logFile.close();
                    QFileInfo fiLog(output);
                    if (fiLog.size() == 62)
                    {
                        logFile.remove();
                    }
                }
                //Commit the transaction
                if (!db.commit())
                {
                    log("Warning: Commit did not succed. Please check the database");
                    log(QString::number(db.lastError().number()));
                    db.close();
                    returnCode = 1;
                    emit finished();
                }
                //We store the map file

                QString mapFile;
                mapFile = mapDir.path() + mapDir.separator() + fileID + ".xml";
                QFile file(mapFile);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QTextStream out(&file);
                    out.setCodec("UTF-8");
                    recordMap.save(out,1,QDomNode::EncodingFromTextStream);
                    file.close();
                }
                else
                    log("Error: Cannot create xml manifest file");
            }

            db.close();
        }
        else
        {
            log("Cannot connect to database");
            log(db.lastError().databaseText());
            returnCode = 1;
        }
    }

    emit finished();
}

void mainClass::setParameters(bool voverwrite, QString vjson, QString vmanifest, QString vhost, QString vport, QString vuser, QString vpassword, QString vschema, QString voutput, QString vinput, QString vjavaScript, bool voputSQLSwitch, QString mapDirectory, QString outputType)
{
    overwrite = voverwrite;
    json = vjson;
    manifest = vmanifest;
    host = vhost;
    port = vport;
    user = vuser;
    password = vpassword;
    schema = vschema;
    output = voutput;
    input = vinput;
    javaScript = vjavaScript;
    outSQL = voputSQLSwitch;
    mapOutputDir = mapDirectory;
    this->outputType = outputType;
}


int mainClass::getLastIndex(QString table)
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

void mainClass::log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf(temp.toLocal8Bit().data());
}

//This function returns a the xmlFieldCode based on its MySQL name
QString mainClass::getXMLCodeFromField(QList< TfieldDef> fields, QString field)
{
    for (int pos = 0; pos <= fields.count()-1;pos++)
    {
        if (fields[pos].name == field)
            return fields[pos].xmlCode;
    }
    return "Unknown";
}

//This procedure creates log entries into the log file. It used the dictionary tables to retrive a more understanable message.
void mainClass::logError(QSqlDatabase db,QString errorMessage, QString table, int rowNumber,QVariantMap jsonData,QList< TfieldDef> fields, QString execSQL)
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
                if (outputType == "h")
                {
                    if (getXMLCodeFromField(fields,field) != "Unknown")
                        logStream << fileID + "\t" + table + "\t" + QString::number(rowNumber) + "\t" + getXMLCodeFromField(fields,field) + "\t" + errorMessage + "\tValue not found = " + jsonData[getXMLCodeFromField(fields,field)].toString() + "\t" + execSQL + "\n";
                    else
                        logStream << fileID + "\t" + table + "\t" + QString::number(rowNumber) + "\t\t" + errorMessage + "\t\t" + execSQL + "\n";
                }
                else
                {
                    QDomElement eError;
                    eError = xmlLog.createElement("error");
                    if (getXMLCodeFromField(fields,field) != "Unknown")
                    {
                        eError.setAttribute("FileUUID",fileID);
                        eError.setAttribute("Table",table);
                        eError.setAttribute("RowInJSON",QString::number(rowNumber));
                        eError.setAttribute("JSONVariable",getXMLCodeFromField(fields,field));
                        eError.setAttribute("Error",errorMessage);
                        eError.setAttribute("Notes","Value not found = " + jsonData[getXMLCodeFromField(fields,field)].toString());
                        QDomText sqlExecuted;
                        sqlExecuted = xmlLog.createTextNode(execSQL);
                        eError.appendChild(sqlExecuted);
                        eErrors.appendChild(eError);
                    }
                    else
                    {
                        eError.setAttribute("FileUUID",fileID);
                        eError.setAttribute("Table",table);
                        eError.setAttribute("RowInJSON",QString::number(rowNumber));
                        eError.setAttribute("JSONVariable","");
                        eError.setAttribute("Error",errorMessage);
                        eError.setAttribute("Notes","");
                        QDomText sqlExecuted;
                        sqlExecuted = xmlLog.createTextNode(execSQL);
                        eError.appendChild(sqlExecuted);
                        eErrors.appendChild(eError);
                    }
                }
                return;
            }
        }
    }
    if (outputType == "h")
        logStream << fileID + "\t" + table + "\t" + QString::number(rowNumber) + "\t\t" + errorMessage + "\t\t" + execSQL + "\n";
    else
    {
        QDomElement eError;
        eError = xmlLog.createElement("error");
        eError.setAttribute("FileUUID",fileID);
        eError.setAttribute("Table",table);
        eError.setAttribute("RowInJSON",QString::number(rowNumber));
        eError.setAttribute("JSONVariable","");
        eError.setAttribute("Error",errorMessage);
        eError.setAttribute("Notes","");
        QDomText sqlExecuted;
        sqlExecuted = xmlLog.createTextNode(execSQL);
        eError.appendChild(sqlExecuted);
        eErrors.appendChild(eError);
    }
}

void mainClass::logErrorMSel(QSqlDatabase db,QString errorMessage, QString table, int rowNumber,QString value, QString execSQL)
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
                if (outputType == "h")
                    logStream << fileID + "\t" + table + "\t" + QString::number(rowNumber) + "\t" + field + "\t" + errorMessage + "\tValue not found = " + value + "\t" + execSQL + "\n";
                else
                {
                    QDomElement eError;
                    eError = xmlLog.createElement("error");
                    eError.setAttribute("FileUUID",fileID);
                    eError.setAttribute("Table",table);
                    eError.setAttribute("RowInJSON",QString::number(rowNumber));
                    eError.setAttribute("JSONVariable",field);
                    eError.setAttribute("Error",errorMessage);
                    eError.setAttribute("Notes","Value not found = " + value);
                    QDomText sqlExecuted;
                    sqlExecuted = xmlLog.createTextNode(execSQL);
                    eError.appendChild(sqlExecuted);
                    eErrors.appendChild(eError);
                }
                return;
            }
        }
    }
    if (outputType == "h")
        logStream << fileID + "\t" + table + "\t" + QString::number(rowNumber) + "\t\t" + errorMessage + "\t\t" + execSQL + "\n";
    else
    {
        QDomElement eError;
        eError = xmlLog.createElement("error");
        eError.setAttribute("FileUUID",fileID);
        eError.setAttribute("Table",table);
        eError.setAttribute("RowInJSON",QString::number(rowNumber));
        eError.setAttribute("JSONVariable","");
        eError.setAttribute("Error",errorMessage);
        eError.setAttribute("Notes","");
        eError.setNodeValue(execSQL);
        eErrors.appendChild(eError);
    }
}

QString mainClass::fixString(QString source)
{
    QString res;
    res = source;
    res = res.replace("'","`");
    res = res.replace(";","");
    return res;
}

void mainClass::findElementsWithAttribute(const QDomElement& elem, const QString& attr, const QString& attvalue, QList<QDomElement> &foundElements)
{
    if (elem.attribute(attr,"") == attvalue)
        foundElements.append(elem);
    //  if( elem.attributes().contains(attr) )
//    foundElements.append(elem);

  QDomElement child = elem.firstChildElement();
  while( !child.isNull() ) {
    findElementsWithAttribute(child, attr, attvalue, foundElements);
    child = child.nextSiblingElement();
  }
}

void mainClass::storeRecord(QString parentUUID, QString recordUUID)
{
    QStringList split;
    split = parentUUID.split("~");
    QList<QDomElement> foundUUIDs;
    findElementsWithAttribute(recordMapRoot, "uuid", split[1], foundUUIDs);
    // If the parent is not found means that his insert
    // did not entered the database thus the map will not be
    // produded.
    if (foundUUIDs.count() > 0)
    {
        QDomElement parentRecord;
        parentRecord = foundUUIDs[0];
        split = recordUUID.split("~");
        QDomElement aRecord;
        aRecord = recordMap.createElement("record");
        aRecord.setAttribute("table",split[0]);
        aRecord.setAttribute("uuid",split[1]);
        parentRecord.appendChild(aRecord);
    }
}

//This function stores data in the record map
void mainClass::storeRecord(QStringList parentUUIDS, QString recordUUID)
{
    if (parentUUIDS.isEmpty())
    {
        QStringList split;
        split = recordUUID.split("~");
        QDomElement aRecord;
        aRecord = recordMap.createElement("record");
        aRecord.setAttribute("table",split[0]);
        aRecord.setAttribute("uuid",split[1]);
        recordMapRoot.appendChild(aRecord);
    }
    else
    {
        QStringList split;
        split = parentUUIDS[parentUUIDS.count()-1].split("~");
        QList<QDomElement> foundUUIDs;
        findElementsWithAttribute(recordMapRoot, "uuid", split[1], foundUUIDs);
        // If the parent is not found means that his insert
        // did not entered the database thus the map will not be
        // produded.
        if (foundUUIDs.count() > 0)
        {
            QDomElement parentRecord;
            parentRecord = foundUUIDs[0];
            split = recordUUID.split("~");
            QDomElement aRecord;
            aRecord = recordMap.createElement("record");
            aRecord.setAttribute("table",split[0]);
            aRecord.setAttribute("uuid",split[1]);
            parentRecord.appendChild(aRecord);
        }
    }
}

//This function construct an INSERT SQL and execute it againts the database.
QList<TfieldDef > mainClass::createSQL(QSqlDatabase db, QVariantMap jsonData, QString table, QList< TfieldDef> fields, QList< TfieldDef> parentkeys, QVariantMap jsonData2, bool mTable)
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
    tblIndex = getLastIndex(table);

    QUuid recordUUID=QUuid::createUuid();
    QString strRecordUUID=recordUUID.toString().replace("{","").replace("}","");

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
            if ((fields[pos].ODKType == "start") || (fields[pos].ODKType == "end") || (fields[pos].ODKType == "time") || (fields[pos].ODKType == "datetime"))
            {
                if (key.value.indexOf(".") >= 0)
                {
                    QStringList parts;
                    parts = key.value.split(".");
                    key.value = parts[0];
                    key.value = key.value.replace("T"," ");
                }
            }
            key.uuid = table + "~" + strRecordUUID;

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
                        //sqlValues = sqlValues + "'ODKTOOLS',";
                        fieldValue = "ODKTOOLS";
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
            if ((fields[pos].ODKType == "start") || (fields[pos].ODKType == "end") || (fields[pos].ODKType == "time") || (fields[pos].ODKType == "datetime"))
            {
                if (fieldValue.indexOf(".") >= 0)
                {
                    QStringList parts;
                    parts = fieldValue.split(".");
                    fieldValue = parts[0];
                    fieldValue = fieldValue.replace("T"," ");
                }
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
        if (insertObject.itemName(pos) != "rowuuid")
        {
            sqlHeader = sqlHeader + insertObject.itemName(pos) + ",";
            sqlValues = sqlValues + "'" + insertObject.itemValue(pos) + "',";
        }

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
    sqlHeader = sqlHeader.left(sqlHeader.length()-1) + ",rowuuid)";
    sqlValues = sqlValues.left(sqlValues.length()-1) + ",'" + strRecordUUID + "')";
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
      if (SQLErrorNumber == "")
        SQLErrorNumber = query.lastError().nativeErrorCode() + "&"  + query.lastError().databaseText() + "@" + table;
      logError(db,query.lastError().databaseText(),table,tblIndex,jsonData,fields,sql); //Write the error to the log
    }
    else
    {
        //Store the record in the map
        QStringList uuids;
        for (int nkey = 0; nkey <= parentkeys.count()-1;nkey++)
        {
            if (uuids.indexOf(parentkeys[nkey].uuid) == -1)
                uuids.append(parentkeys[nkey].uuid);
        }
        storeRecord(uuids,table + "~" + strRecordUUID);
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
                    if (multiSelectObject.itemName(nkey) != "rowuuid")
                    {
                        sqlHeader = sqlHeader + multiSelectObject.itemName(nkey) + ",";
                        sqlValues = sqlValues + "'" + multiSelectObject.itemValue(nkey) + "',";
                    }
                }
                sqlHeader = sqlHeader.left(sqlHeader.length()-1) + ",rowuuid)";
                QUuid uuid=QUuid::createUuid();
                QString uuidstr=uuid.toString().replace("{","").replace("}","");
                sqlValues = sqlValues.left(sqlValues.length()-1) + ",'" + uuidstr + "')";

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
                  if (SQLErrorNumber == "")
                    SQLErrorNumber = query.lastError().nativeErrorCode() + "&" + query.lastError().databaseText() + "@" + mSelectTableName;
                  logErrorMSel(db,query.lastError().databaseText(),mSelectTableName,tblIndex,mSelectValues[nvalue],sql); //Write the error to the log
                }
                else
                {
                    //Store the record in the map
                    storeRecord(table + "~" + strRecordUUID,mSelectTableName + "~" + uuidstr);
                }
            }
        }
    }

    return resKeys;
}

void mainClass::debugKeys(QString table, QList< TfieldDef> keys)
{
    qDebug() << "***" + table;
    qDebug() << "Total keys: " + QString::number(keys.count());
    for (int pos = 0; pos <= keys.count()-1;pos++)
    {
        qDebug() << "Field:" + keys[pos].name + ". Value: " + keys[pos].value;
    }

}

void mainClass::debugMap(QVariantMap jsonData)
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
int mainClass::procTable(QSqlDatabase db,QVariantMap jsonData, QDomNode table, QList< TfieldDef> parentkeys)
{
    QList< TfieldDef> keys;
    QList< TfieldDef> tkeys;
    QList< TtableKey> tableKeys;
    keys.append(parentkeys);

    QVariantMap emptyMap;

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
                field.type = child.toElement().attribute("xmlcode","varchar");
                field.ODKType = child.toElement().attribute("odktype","text");
                field.size = child.toElement().attribute("size","0").toInt();
                field.decSize = child.toElement().attribute("decsize","0").toInt();

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
                        keys.append(createSQL(db,jsonData,tableCode,fields,keys,emptyMap,true));
                    else
                    {
                        // if we are processing the first table and is not main, this means that a repeat of one was used
                        // to store the cover data. Therefore the insert SQL must use both the information on root (jsonData)
                        // and the information on the repeat of one (map)
                        QVariantList result = jsonData[tableXMLCode].toList();
                        foreach(QVariant record, result)
                        {
                            map = record.toMap();
                            keys.append(createSQL(db,jsonData,tableCode,fields,keys,map,true));
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
                        tkeys.append(createSQL(db,map,tableCode,fields,tkeys,emptyMap,false));
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
                createSQL(db,jsonData,tableCode,fields,keys,emptyMap,true);
            else
            {
                // if we are processing the first table and is not main, this means that a repeat of one was used
                // to store the cover data. Therefore the insert SQL must use both the information on root (jsonData)
                // and the information on the repeat of one (map)
                QVariantList result = jsonData[tableXMLCode].toList();
                foreach(QVariant record, result)
                {
                    QVariantMap map = record.toMap();
                    keys.append(createSQL(db,jsonData,tableCode,fields,keys,map,true));
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
                createSQL(db,map,tableCode,fields,keys,emptyMap,false);
            }
        }
    }
    return 0;
}

//This process load a JSON file to a VariantMap using QJSON library
int mainClass::processFile(QSqlDatabase db, QString json, QString manifest, QStringList procList)
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
