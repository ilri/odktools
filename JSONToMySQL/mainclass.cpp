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
#include <QThread>

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
        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL","repository");
        QSqlDatabase imported_db = QSqlDatabase::addDatabase("QSQLITE","submissions");
        db.setHostName(host);
        db.setPort(port.toInt());
        db.setDatabaseName(schema);
        db.setUserName(user);
        db.setPassword(password);
        //db.setConnectOptions("MYSQL_OPT_SSL_MODE=SSL_MODE_DISABLED");
        if (db.open())
        {            
            QFile UUIDFile(UUIDsFile);
            //The file is new so write from start
            if (!UUIDFile.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                log("Cannot create processing file");
                returnCode = 1;
                db.close();
                emit finished();
            }
            QTextStream UUIDout(&UUIDFile); //Stream to the processing file

            imported_db.setDatabaseName(input);
            if (imported_db.open())
            {
                QSqlQuery query(imported_db);
                QString sql;
                sql = "CREATE TABLE submissions (submission_id VARCHAR(64) PRIMARY KEY)";
                if (!query.exec(sql))
                {
                    if (query.lastError().databaseText().indexOf("already exists") < 0)
                    {
                        log("Cannot create processing table: " + query.lastError().databaseText());
                        returnCode = 1;
                        db.close();
                        emit finished();
                    }
                }
            }
            else
            {
                log("Cannot create processing sqlite file");
                returnCode = 1;
                db.close();
                emit finished();
            }

            QDir mapDir(mapOutputDir);
            if (!mapDir.exists())
            {
                if (mapDir.mkpath(mapOutputDir))
                {
                    log("Output map directory does not exist and cannot be created");
                    returnCode = 1;
                    db.close();
                    imported_db.close();
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
                        imported_db.close();
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
                        imported_db.close();
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
                imported_db.close();
                returnCode = 1;                
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
                    imported_db.close();
                    emit finished();
                }
                sqlStream.setDevice(&sqlFile);
            }

            SQLError = false;
            SQLErrorNumber = "";
            int processError;
            processError = processFile2(db,json,manifest,imported_db);
            if (processError == 2)
            {
                //This will happen if the file is already processes or an error ocurred
                db.close();
                imported_db.close();
                returnCode = 1;
                emit finished();
                return;
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
                            imported_db.close();
                            returnCode = 1;
                            emit finished();
                        }
                    }
                }

                if (!db.rollback())
                {
                    log("Error: Rolling back was not possible. Please check the database");
                    db.close();
                    imported_db.close();
                    returnCode = 1;
                    emit finished();
                }
            }
            else
            {
                for (int aUUID = 0; aUUID <= UUIDList.count()-1; aUUID++)
                {
                    UUIDout << UUIDList.at(aUUID) + "\n";
                }
                UUIDFile.close();

                QFileInfo fi(json);
                // Inserting into the submission database
                QString sql;
                QSqlQuery query(imported_db);                                
                bool inserted = false;
                int try_count = 0;
                while (!inserted)
                {
                    sql =  "INSERT INTO submissions VALUES ('" + fi.baseName() + "')";
                    if (!query.exec(sql))
                    {
                        if (try_count > 3)
                        {
                            log("Error: Cannot store the submission in the submission database. Rolling back");
                            log(query.lastError().databaseText());
                            if (!db.rollback())
                            {
                                log("Error: Cannot store the submission in the submission database. Rolling back was not possible. Please check the database");
                                db.close();
                                imported_db.close();
                                returnCode = 1;
                                inserted = true;
                                emit finished();
                            }
                            db.close();
                            imported_db.close();
                            inserted = true;
                        }
                        else
                        {
                            try_count++;
                            QThread::sleep(2);
                        }
                    }
                    else
                        inserted = true;
                }


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
                    imported_db.close();
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
            imported_db.close();
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

void mainClass::setParameters(bool voverwrite, QString vjson, QString vmanifest, QString vhost, QString vport, QString vuser, QString vpassword, QString vschema, QString voutput, QString vinput, QString vjavaScript, bool voputSQLSwitch, QString mapDirectory, QString outputType, QString uuidsFile, QStringList supportFiles)
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
    UUIDsFile = uuidsFile;
    for (int idx =0; idx < supportFiles.count(); idx++)
    {
        TOSMFileDef OSM_file;
        QFileInfo file_info(supportFiles[idx]);
        OSM_file.fileName = supportFiles[idx];
        OSM_file.baseName = file_info.fileName();
        OSMFiles.append(OSM_file);
    }
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
    printf("%s",temp.toLocal8Bit().data());
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

//This procedure creates log entries into the log file for OSM. It used the dictionary tables to retrive a more understanable message.
void mainClass::logLoopError(QString errorMessage, QString table, QString loopItem, QString execSQL)
{
    if (outputType == "h")
        logStream << fileID + "\t" + table + "\t" + loopItem + "\t\t" + errorMessage + "\t\t" + execSQL + "\n";
    else
    {
        QDomElement eError;
        eError = xmlLog.createElement("error");
        eError.setAttribute("FileUUID",fileID);
        eError.setAttribute("Table",table);
        eError.setAttribute("LoopItem",loopItem);
        eError.setAttribute("JSONVariable","");
        eError.setAttribute("Error",errorMessage);
        eError.setAttribute("Notes","");
        QDomText sqlExecuted;
        sqlExecuted = xmlLog.createTextNode(execSQL);
        eError.appendChild(sqlExecuted);
        eErrors.appendChild(eError);
    }
}

//This procedure creates log entries into the log file for OSM. It used the dictionary tables to retrive a more understanable message.
void mainClass::logOSMError(QString errorMessage, QString table, int nodeIndex, QString execSQL)
{
    if (outputType == "h")
        logStream << fileID + "\t" + table + "\t" + QString::number(nodeIndex) + "\t\t" + errorMessage + "\t\t" + execSQL + "\n";
    else
    {
        QDomElement eError;
        eError = xmlLog.createElement("error");
        eError.setAttribute("FileUUID",fileID);
        eError.setAttribute("Table",table);
        eError.setAttribute("Node",QString::number(nodeIndex));
        eError.setAttribute("JSONVariable","");
        eError.setAttribute("Error",errorMessage);
        eError.setAttribute("Notes","");
        QDomText sqlExecuted;
        sqlExecuted = xmlLog.createTextNode(execSQL);
        eError.appendChild(sqlExecuted);
        eErrors.appendChild(eError);
    }
}

//This procedure creates log entries into the log file. It used the dictionary tables to retrive a more understanable message.
void mainClass::logError(QString errorMessage, QString table, int rowNumber, QString execSQL)
{    
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

void mainClass::logErrorMSel(QString errorMessage, QString table, int rowNumber, QString execSQL)
{
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
    res = res.replace(";","|");
    res = res.replace("\n"," ");
    res = res.simplified();
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
            key.value = key.value.replace("'","`");
            if (fields[pos].type == "datetime")
            {
                if (key.value.indexOf(".") >= 0)
                {
                    QStringList parts;
                    parts = key.value.split(".");
                    key.value = parts[0];
                    key.value = key.value.replace("T"," ");
                    key.value = key.value.replace("Z","");
                    if (fields[pos].ODKType == "time")
                    {
                        if (key.value.indexOf("-") < 0)
                        {
                            //Treat time as datetime by fixing date a date
                            key.value = "2019-01-01 " + key.value;
                        }
                    }
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
            variantValue =  jsonData[fields[pos].xmlCode];
            fieldValue = fixString(QString::fromUtf8(variantValue.toByteArray()));

            if (fields[pos].name == "originid")
            {
                //sqlValues = sqlValues + "'ODKTOOLS',";
                fieldValue = "ODKTOOLS 2.0";
            }
            if (fields[pos].name == "surveyid")
            {
              //sqlValues = sqlValues + "'" + fileID + "',";
              fieldValue = fileID;
            }
            if (fields[pos].type == "datetime")
            {
                if (fieldValue.indexOf(".") >= 0)
                {
                    QStringList parts;
                    parts = fieldValue.split(".");
                    fieldValue = parts[0];
                    fieldValue = fieldValue.replace("T"," ");
                    fieldValue = fieldValue.replace("Z","");                    
                    if (fields[pos].ODKType == "time")
                    {
                        if (fieldValue.indexOf("-") < 0)
                        {
                            //Treat time as datetime by fixing date a date
                            fieldValue = "2019-01-01 " + fieldValue;
                        }
                    }
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

    bool hasSomethingToInsert;
    hasSomethingToInsert = false;
    for (pos = 0; pos < insertObject.count();pos++)
    {
        if (insertObject.itemName(pos) != "rowuuid")
        {
            sqlHeader = sqlHeader + insertObject.itemName(pos) + ",";
            sqlValues = sqlValues + "'" + insertObject.itemValue(pos) + "',";
            if (insertObject.itemIsKey(pos) == false)
                if (insertObject.itemValue(pos).simplified().trimmed() != "")
                    hasSomethingToInsert = true;
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
    sql = sql.replace("\\n"," ");
    sql = sql.replace("\n"," ");

    //Execute the SQL to the database

    if (outSQL)
    {
        sqlStream << sql + ";\n";
    }
    QSqlQuery query(db);
    hasSomethingToInsert = true;
    if (hasSomethingToInsert) //There are other values besides keys to insert
    {
        query.exec("SET @odktools_ignore_insert = 1");
        if (!query.exec(sql))
        {
            SQLError = true; //An error occurred. This will trigger a rollback
            if (SQLErrorNumber == "")
                SQLErrorNumber = query.lastError().nativeErrorCode() + "&"  + query.lastError().databaseText() + "@" + table;
            logError(query.lastError().databaseText(),table,tblIndex,sql); //Write the error to the log
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
            //Add the UUID to the list
            UUIDList.append(table + "," + strRecordUUID);
        }
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
                query.exec("SET @odktools_ignore_insert = 1");
                if (!query.exec(sql))
                {
                  SQLError = true; //An error occurred. This will trigger a rollback
                  if (SQLErrorNumber == "")
                    SQLErrorNumber = query.lastError().nativeErrorCode() + "&" + query.lastError().databaseText() + "@" + mSelectTableName;
                  logErrorMSel(query.lastError().databaseText(),mSelectTableName,tblIndex,sql); //Write the error to the log
                }
                else
                {
                    //Store the record in the map
                    storeRecord(table + "~" + strRecordUUID,mSelectTableName + "~" + uuidstr);
                    //Store the UUD
                    UUIDList.append(mSelectTableName + "," + uuidstr);
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

QString  mainClass::fixField(QString source)
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
    res = res.replace(":","");
    res = res.trimmed().simplified().toLower();
    return res;
}

void mainClass::insertOSMData(QString OSMField, QDomElement node, int nodeIndex, QDomNodeList tags, QList< TfieldDef> parentkeys, QSqlDatabase db)
{
    QUuid recordUUID=QUuid::createUuid();
    QString strRecordUUID=recordUUID.toString().replace("{","").replace("}","");

    QString sql;
    sql = "INSERT INTO " + OSMField  + " (";
    for (int ikey = 0; ikey < parentkeys.count(); ikey++)
    {
        sql = sql + parentkeys[ikey].name + ",";
    }
    sql = sql + OSMField + "_rowid,geopoint_lat,geopoint_lon,";
    for (int itag = 0; itag < tags.count(); itag++)
    {
        QDomElement ETag = tags.item(itag).toElement();
        QString column = ETag.attribute("k","NONE");
        if (column != "NONE")
        {
            sql = sql + fixField(column) + ",";
        }
    }
    sql = sql + "rowuuid) VALUES (";
    for (int ikey = 0; ikey < parentkeys.count(); ikey++)
    {
        sql = sql + "'" + parentkeys[ikey].value + "',";
    }
    sql = sql + QString::number(nodeIndex) + ",";
    sql = sql + "'" + node.attribute("lat","") + "',";
    sql = sql + "'" + node.attribute("lon","") + "',";
    for (int itag = 0; itag < tags.count(); itag++)
    {
        QDomElement ETag = tags.item(itag).toElement();
        QString column = ETag.attribute("k","NONE");
        QString value = ETag.attribute("v","");
        if (column != "NONE")
        {
            sql = sql + "'" + value + "',";
        }
    }
    sql = sql + "'" + strRecordUUID + "')";
    QSqlQuery query(db);
    query.exec("SET @odktools_ignore_insert = 1");    
    if (!query.exec(sql))
    {
        SQLError = true; //An error occurred. This will trigger a rollback
        if (SQLErrorNumber == "")
            SQLErrorNumber = query.lastError().nativeErrorCode() + "&"  + query.lastError().databaseText() + "@" + OSMField;
        logOSMError(query.lastError().databaseText(),OSMField,nodeIndex,sql);
    }
    else
    {
        //Add the UUID to the list
        UUIDList.append(OSMField + "," + strRecordUUID);
    }
}

void mainClass::processOSM(QString OSMField, QString OSMFile, QList< TfieldDef> parentkeys, QSqlDatabase db)
{    
    QString filePath = "";
    for (int idx=0; idx < OSMFiles.count(); idx++)
    {
        if (OSMFiles[idx].baseName == OSMFile)
        {
            filePath = OSMFiles[idx].fileName;
            break;
        }

    }
    if (filePath != "")
    {
        QDomDocument doc("OSMDocument");
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
        {
            log("Cannot open OSM file \"" + OSMFile + "\"");
            exit(2);
        }
        if (!doc.setContent(&file))
        {
            file.close();
            log("Cannot parse OSM file \"" + OSMFile + "\"");
            exit(2);
        }
        file.close();
        QDomNodeList nodes = doc.elementsByTagName("node");
        for (int inode=0; inode < nodes.count(); inode++)
        {
            QDomElement ENode = nodes.item(inode).toElement();
            QDomNodeList tags = ENode.elementsByTagName("tag");
            insertOSMData(OSMField, ENode, inode+1, tags, parentkeys, db);
        }
    }
    else
    {
        log("OSM file \"" + OSMFile + "\" was not attached. Attach it and run the tool again");
        exit(2);
    }
}

void mainClass::processLoop(QJsonObject jsonData, QString loopTable, QString loopXMLRoot, QStringList loopItems, QList< TfieldDef> fields, QList< TfieldDef> parentkeys, QSqlDatabase db)
{
    QSqlQuery query(db);
    for (int iItem = 0; iItem < loopItems.count(); iItem++)
    {
        QUuid recordUUID=QUuid::createUuid();
        QString strRecordUUID=recordUUID.toString().replace("{","").replace("}","");

        QString sql;
        sql = "INSERT INTO " + loopTable  + " (";
        for (int ikey = 0; ikey < parentkeys.count(); ikey++)
        {
            sql = sql + parentkeys[ikey].name + ",";
        }
        sql = sql + loopTable + ",";
        for (int iField = 0; iField < fields.count(); iField++)
        {
            sql = sql + fields[iField].name + ",";
        }
        sql = sql + "rowuuid) VALUES (";
        for (int ikey = 0; ikey < parentkeys.count(); ikey++)
        {
            sql = sql + "'" + parentkeys[ikey].value + "',";
        }
        sql = sql + "'" + loopItems[iItem] + "',";
        for (int iField = 0; iField < fields.count(); iField++)
        {
            QString fieldValue;
            QString xmlKey = loopXMLRoot + "/" + loopItems[iItem] + "/" + fields[iField].xmlCode;
            fieldValue = jsonData.value(xmlKey).toString("");
            if (fields[iField].type == "datetime")
            {
                if (fieldValue.indexOf(".") >= 0)
                {
                    QStringList parts;
                    parts = fieldValue.split(".");
                    fieldValue = parts[0];
                    fieldValue = fieldValue.replace("T"," ");
                    fieldValue = fieldValue.replace("Z","");
                    if (fields[iField].ODKType == "time")
                    {
                        if (fieldValue.indexOf("-") < 0)
                        {
                            //Treat time as datetime by fixing date a date
                            fieldValue = "2019-01-01 " + fieldValue;
                        }
                    }
                }
            }
            sql = sql + "'" + fieldValue + "',";
        }
        sql = sql + "'" + strRecordUUID + "')";
        query.exec("SET @odktools_ignore_insert = 1");
        if (!query.exec(sql))
        {
            SQLError = true; //An error occurred. This will trigger a rollback
            if (SQLErrorNumber == "")
                SQLErrorNumber = query.lastError().nativeErrorCode() + "&"  + query.lastError().databaseText() + "@" + loopTable;
            logLoopError(query.lastError().databaseText(),loopTable,loopItems[iItem],sql);
        }
        else
        {
            //Add the UUID to the list
            UUIDList.append(loopTable + "," + strRecordUUID);
            for (int pos = 0; pos < fields.count(); pos++)
            {
                if (fields[pos].multiSelect == true)
                {
                    QString fieldValue;
                    QString xmlKey = loopXMLRoot + "/" + loopItems[iItem] + "/" + fields[pos].xmlCode;
                    fieldValue = jsonData.value(xmlKey).toString("");
                    QStringList parts = fieldValue.split(" ",QString::SkipEmptyParts);
                    for (int ipart = 0; ipart < parts.count(); ipart++)
                    {
                        recordUUID=QUuid::createUuid();
                        strRecordUUID=recordUUID.toString().replace("{","").replace("}","");
                        sql = "INSERT INTO " + fields[pos].multiSelectTable + " (";
                        for (int ikey = 0; ikey < parentkeys.count(); ikey++)
                        {
                            sql = sql + parentkeys[ikey].name + ",";
                        }
                        sql = sql + loopTable + "," + fields[pos].name + ",rowuuid) VALUES (";
                        for (int ikey = 0; ikey < parentkeys.count(); ikey++)
                        {
                            sql = sql + "'" + parentkeys[ikey].value + "',";
                        }
                        sql = sql + "'" + loopItems[iItem] + "',";
                        sql = sql + "'" + parts[ipart] + "',";
                        sql = sql + "'" + strRecordUUID + "')";
                        query.exec("SET @odktools_ignore_insert = 1");
                        if (!query.exec(sql))
                        {
                            SQLError = true; //An error occurred. This will trigger a rollback
                            if (SQLErrorNumber == "")
                                SQLErrorNumber = query.lastError().nativeErrorCode() + "&"  + query.lastError().databaseText() + "@" + fields[pos].multiSelectTable;
                            logLoopError(query.lastError().databaseText(),fields[pos].multiSelectTable,loopItems[iItem] + "-" + parts[ipart],sql);
                        }
                    }
                }
            }
        }
    }
}

int mainClass::procTable2(QSqlDatabase db,QJsonObject jsonData, QDomNode table, QList< TfieldDef> parentkeys)
{
    QList< TfieldDef> keys;
    QList< TfieldDef> tkeys;
    QList< TtableKey> tableKeys;
    keys.append(parentkeys);
    QList< TfieldDef> fields;

    bool sqlCreated;
    sqlCreated = false;

    QString tableCode;
    tableCode = table.toElement().attribute("mysqlcode");

    int recordIndex;
    int tkindex;

    QString tableXMLCode;
    tableXMLCode = table.toElement().attribute("xmlcode");

    QString group;
    group = table.toElement().attribute("group","false");

    QString osm;
    osm = table.toElement().attribute("osm","false");

    QString loop;
    loop = table.toElement().attribute("loop","false");

    bool genSQL;
    genSQL = false;

    QDomNode child;
    child = table.firstChild();

    QVariantMap emptyMap;
    if (osm == "true")
    {        
        QString OSMODKField;
        OSMODKField = table.toElement().attribute("xmlcode","none");
        QString OSMFile = jsonData.value(OSMODKField).toString("");
        QString OSMField = table.toElement().attribute("mysqlcode","none");
        if (OSMFile != "")
        {
            processOSM(OSMField,OSMFile,keys,db);
            return 0;
        }
    }
    if (loop == "true")
    {
        QString loopXMLRoot;
        loopXMLRoot = table.toElement().attribute("xmlcode","none");
        QString loopTable;
        loopTable = table.toElement().attribute("mysqlcode","none");
        QStringList loopItems = table.toElement().attribute("loopitems","").split(QChar(743),QString::SkipEmptyParts);
        if (loopItems.count() > 0)
        {
            QDomNode fieldNode = table.firstChild();
            QList< TfieldDef> loopFields;
            while (!fieldNode.isNull())
            {
                if (fieldNode.toElement().attribute("xmlcode","NONE") != "NONE")
                {
                    TfieldDef aLoopField;
                    aLoopField.xmlCode = fieldNode.toElement().attribute("xmlcode","");
                    aLoopField.decSize = fieldNode.toElement().attribute("decsize","0").toInt();
                    aLoopField.key = false;
                    if (fieldNode.toElement().attribute("isMultiSelect","false") == "true")
                    {
                        aLoopField.multiSelect = true;
                        aLoopField.multiSelectTable = fieldNode.toElement().attribute("multiSelectTable","");
                    }
                    else
                        aLoopField.multiSelect = false;
                    aLoopField.name = fieldNode.toElement().attribute("mysqlcode","");
                    aLoopField.ODKType = fieldNode.toElement().attribute("odktype","");
                    aLoopField.size = fieldNode.toElement().attribute("size",0).toInt();
                    aLoopField.type = fieldNode.toElement().attribute("type","varchar");
                    loopFields.append(aLoopField);
                }
                fieldNode = fieldNode.nextSibling();
            }
            processLoop(jsonData,loopTable,loopXMLRoot,loopItems,loopFields,keys,db);
            return 0;
        }
    }

    if ((osm == "false") && (loop == "false"))
    {
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
                    field.type = child.toElement().attribute("type","varchar");
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
                if ((tableXMLCode == "main") || (group == "true") || (osm == "true") || (loop == "true"))
                {
                    if (genSQL == true)
                    {
                        keys.append(createSQL(db,jsonData.toVariantMap(),tableCode,fields,keys,emptyMap,true));    //Change the variant map to an object later on!
                        genSQL = false;
                    }
                    if ((osm == false) && (loop == false))
                    {
                        QJsonArray children = jsonData.value(child.toElement().attribute("xmlcode")).toArray();
                        for (int chld = 0; chld < children.count(); chld++)
                            procTable2(db,children.at(chld).toObject(),child,keys);
                    }
                    else
                    {
                        procTable2(db,jsonData,child,keys);
                    }
                }
                else
                {
                    QString tableXMLCode = table.toElement().attribute("xmlcode","");
                    if (tableXMLCode != "")
                    {
                        QJsonArray JSONArray;
                        JSONArray = jsonData.value(tableXMLCode).toArray();
                        recordIndex = 0;
                        for (int item = 0; item < JSONArray.count(); item++) //For each item in the array
                        {
                            QJsonObject JSONItem = JSONArray[item].toObject();
                            recordIndex++;
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
                                tkeys.append(createSQL(db,JSONItem.toVariantMap(),tableCode,fields,tkeys,emptyMap,false));
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
                            procTable2(db,JSONItem,child,tkeys);
                        }
                    }
                }
            }
            child = child.nextSibling();
        }
        if (!sqlCreated)
        {
            if ((tableXMLCode == "main") || (group == "true") || (osm == "true") || (loop == "true"))
                createSQL(db,jsonData.toVariantMap(),tableCode,fields,keys,emptyMap,true);   //Change the variant map later in
            else
            {
                QString tableXMLCode = table.toElement().attribute("xmlcode","");
                if (tableXMLCode != "")
                {
                    QJsonArray JSONArray;
                    JSONArray = jsonData.value(tableXMLCode).toArray();
                    for (int item = 0; item < JSONArray.count(); item++) //For each item in the array
                    {
                        QJsonObject JSONItem = JSONArray[item].toObject();
                        createSQL(db,JSONItem.toVariantMap(),tableCode,fields,keys,emptyMap,false);
                    }
                }
            }
        }
    }
    return 0;
}

int mainClass::processFile2(QSqlDatabase db, QString json, QString manifest, QSqlDatabase submissions_db)
{
    QFileInfo fi(json);
    //If the file hasn't been processed yet
    QSqlQuery query(submissions_db);
    QString sql;
    sql = "SELECT count(submission_id ) FROM submissions WHERE submission_id ='" + fi.baseName() + "'";
    if (query.exec(sql))
    {
        query.first();
        if (query.value(0).toInt() == 0)
        {
            fileID = fi.baseName();

            QFile JSONFile(json);
            if (!JSONFile.open(QIODevice::ReadOnly))
            {
                log("Cannot open" + json);
                return 1;
            }
            QByteArray JSONData = JSONFile.readAll();
            QJsonDocument JSONDocument;
            JSONDocument = QJsonDocument::fromJson(JSONData);
            QJsonObject firstObject = JSONDocument.object();
            if (!firstObject.isEmpty())
            {

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
                procTable2(db,firstObject,root,noParentKeys);
            }

        }
        else
        {
            log("File " + json + " has been already processed. Skipped");
            return 2;
        }
    }
    else
    {
        log("Error while quering for submission " + json + ". Skipping it");
        return 2;
    }
    return 0;
}
