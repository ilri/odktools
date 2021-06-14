#include "mainclass.h"
#include <QDir>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QProcess>
#include <QChar>
#include <QTime>
#include <unistd.h>
#include <QDebug>
#include <QUuid>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>

#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/options/find.hpp>


mainClass::mainClass(QObject *parent) : QObject(parent)
{
    returnCode = 0;    
}

void mainClass::log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s", temp.toUtf8().data());
}

void mainClass::setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, bool protectSensitive, QString tempDir, QString encryption_key, QString mapDir, QString outputDir, QString mainTable, bool resolveMultiSelects, QString primaryKey)
{
    this->host = host;
    this->port = port;
    this->user = user;
    this->pass = pass;
    this->schema = schema;
    this->protectSensitive = protectSensitive;
    this->tempDir = tempDir;
    this->createXML = createXML;    
    this->encryption_key = encryption_key;
    this->mapDir = mapDir;
    this->outputDir = outputDir;
    this->mainTable = mainTable;
    this->resolveMultiSelects = resolveMultiSelects;
    this->primaryKey = primaryKey;
}

void mainClass::getMultiSelectInfo(QDomNode table, QString table_name, QString &multiSelect_field, QStringList &keys, QString &rel_table, QString &rel_field)
{
    QDomNode child = table.firstChild();
    while (!child.isNull())
    {
        if (child.toElement().tagName() == "table")
        {
            if (child.toElement().attribute("name") == table_name)
            {
                QDomNode field = child.firstChild();
                while (!field.isNull())
                {
                    if (field.toElement().attribute("rlookup","false") == "true")
                    {
                        multiSelect_field = field.toElement().attribute("name");
                        rel_table = field.toElement().attribute("rtable");
                        rel_field = field.toElement().attribute("rfield");
                    }
                    else
                    {
                        if (field.toElement().attribute("key","false") == "true")
                        {
                            keys.append(field.toElement().attribute("name"));
                        }
                    }
                    field = field.nextSibling();
                }
            }
        }
        child = child.nextSibling();
    }    
}

void mainClass::loadTable(QDomNode table)
{
    QDomElement eTable;
    eTable = table.toElement();

    TtableDef aTable;
    aTable.islookup = false;
    aTable.name = eTable.attribute("name","");
    aTable.desc = eTable.attribute("name","");

    QDomNode field = table.firstChild();
    while (!field.isNull())
    {
        QDomElement eField;
        eField = field.toElement();
        if (eField.tagName() == "field")
        {
            TfieldDef aField;
            aField.name = eField.attribute("name","");
            aField.desc = eField.attribute("desc","");
            aField.type = eField.attribute("type","");
            aField.size = eField.attribute("size","").toInt();
            aField.decSize = eField.attribute("decsize","").toInt();
            if (eField.attribute("sensitive","false") == "true")
            {
                aField.sensitive = true;
                aField.protection = eField.attribute("protection","exclude");
            }
            else
                aField.sensitive = false;
            if (eField.attribute("key","false") == "true")
            {
                TfieldDef keyField;
                keyField.name = aField.name;
                keyField.replace_value = "";
                aField.isKey = true;
                if (aField.sensitive == true)
                {
                    if (protectedKeys.indexOf(aField.name) < 0)
                        protectedKeys.append(aField.name);
                }
            }
            else
                aField.isKey = false;
            // NOTE ON Rank. Rank is basically a multiselect with order and handled as a multiselect by ODK Tools. However
            // we cannot pull the data from the database because the records may not be stored in the same order the user placed them in Collect
            if ((eField.attribute("isMultiSelect","false") == "true") && (eField.attribute("odktype","") != "rank"))
            {
                aField.isMultiSelect = true;
                aField.multiSelectTable = eField.attribute("multiSelectTable");
                QString multiSelect_field;
                QStringList keys;
                QString multiSelectRelTable;
                QString multiSelectRelField;
                getMultiSelectInfo(table, aField.multiSelectTable, multiSelect_field, keys, multiSelectRelTable, multiSelectRelField);
                aField.multiSelectField = multiSelect_field;
                aField.multiSelectRelTable = multiSelectRelTable;
                aField.multiSelectRelField = multiSelectRelField;
                aField.multiSelectKeys.append(keys);
            }
            aTable.fields.append(aField);
        }
        else
        {
            loadTable(field);
        }
        field = field.nextSibling();
    }
    mainTables.append(aTable);

}

int mainClass::generateXLSX()
{

    QDomDocument docA("input");
    QFile fileA(createXML);
    if (!fileA.open(QIODevice::ReadOnly))
    {
        log("Cannot open input create XML file");
        returnCode = 1;
        return returnCode;
    }
    if (!docA.setContent(&fileA))
    {
        log("Cannot parse input create XML file");
        fileA.close();
        returnCode = 1;
        return returnCode;
    }
    fileA.close();

    //Load the lookup tables if asked
    QDomElement rootA = docA.documentElement();

    if (rootA.tagName() == "XMLSchemaStructure")
    {

        //Getting the fields to export from tables
        QDomNode table = rootA.firstChild().nextSibling().firstChild();

        //Load the data tables recursively
        loadTable(table);
        for (int nt =mainTables.count()-1; nt >= 0;nt--)
        {
            if (mainTables[nt].name.indexOf("_msel_") < 0)
                tables.append(mainTables[nt]);
        }

        QDir currDir(tempDir);        
        QStringList arguments;
        QProcess *mySQLDumpProcess = new QProcess();        
        QTime procTime;
        procTime.start();
        QString sql;
        QStringList fields;
        QString uri = user + ":" + pass + "@" + host + "/" + schema;

        QStringList jsonFiles;
        QString leftjoin;
        QStringList leftjoins;                
        int lkpTblIndex;
        QString primaryKeyField;
        for (int fld = 0; fld < tables[0].fields.count(); fld++)
        {
            if (tables[0].fields[fld].isKey)
            {
                primaryKeyField = tables[0].fields[fld].name;
                break;
            }
        }

        for (int pos = 0; pos <= tables.count()-1; pos++)
        {                                                
            lkpTblIndex = 0;
            leftjoins.clear();            
            fields.clear();
            for (int fld = 0; fld < tables[pos].fields.count(); fld++)
            {
                lkpTblIndex++;
                if (this->protectSensitive)
                {
                    if (tables[pos].fields[fld].sensitive == false)
                    {
                        if (tables[pos].fields[fld].isMultiSelect == false)
                        {
                            if (tables[pos].fields[fld].isKey == false)
                                fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                            else
                            {
                                if (protectedKeys.indexOf(tables[pos].fields[fld].name) < 0)
                                    fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                                else
                                    fields.append("HEX(AES_ENCRYPT(" + tables[pos].name + "." + tables[pos].fields[fld].name + ",UNHEX('" + this->encryption_key + "'))) as " + tables[pos].fields[fld].name);
                            }
                        }
                        else
                        {
                            QString lkpdesc = tables[pos].fields[fld].multiSelectRelField;
                            lkpdesc = lkpdesc.replace("_cod","_des");
                            if (resolveMultiSelects)
                                fields.append("GROUP_CONCAT(DISTINCT T" + QString::number(lkpTblIndex) + "." + lkpdesc + ") AS " + tables[pos].fields[fld].name);
                            else
                                fields.append("GROUP_CONCAT(DISTINCT " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectField + " SEPARATOR ' ') AS " + tables[pos].fields[fld].name);
                            leftjoin = "LEFT JOIN " + tables[pos].fields[fld].multiSelectTable + " ON " + tables[pos].name + "." + tables[pos].fields[fld].multiSelectKeys[0] + " = " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectKeys[0];
                            for (int key = 1; key < tables[pos].fields[fld].multiSelectKeys.count(); key++)
                            {
                                leftjoin = leftjoin + " AND " + tables[pos].name + "." + tables[pos].fields[fld].multiSelectKeys[key] + " = " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectKeys[key];
                            }
                            leftjoins.append(leftjoin);
                            if (resolveMultiSelects)
                            {
                                leftjoin = "LEFT JOIN " + tables[pos].fields[fld].multiSelectRelTable + " AS T" + QString::number(lkpTblIndex) + " ON " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectField + " = T" + QString::number(lkpTblIndex) + "." + tables[pos].fields[fld].multiSelectRelField;
                                leftjoins.append(leftjoin);
                            }
                        }
                    }
                    else
                    {
                        if (tables[pos].fields[fld].protection != "exclude")
                            fields.append("HEX(AES_ENCRYPT(" + tables[pos].name + "." + tables[pos].fields[fld].name + ",UNHEX('" + this->encryption_key + "'))) as " + tables[pos].fields[fld].name);
                    }
                }
                else
                {
                    if (tables[pos].fields[fld].isMultiSelect == false)
                    {
                        fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                    }
                    else
                    {
                        QString lkpdesc = tables[pos].fields[fld].multiSelectRelField;
                        lkpdesc = lkpdesc.replace("_cod","_des");
                        if (resolveMultiSelects)
                            fields.append("GROUP_CONCAT(DISTINCT T" + QString::number(lkpTblIndex) + "." + lkpdesc + ") AS " + tables[pos].fields[fld].name);
                        else
                            fields.append("GROUP_CONCAT(DISTINCT " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectField + " SEPARATOR ' ') AS " + tables[pos].fields[fld].name);
                        leftjoin = "LEFT JOIN " + tables[pos].fields[fld].multiSelectTable + " ON " + tables[pos].name + "." + tables[pos].fields[fld].multiSelectKeys[0] + " = " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectKeys[0];
                        for (int key = 1; key < tables[pos].fields[fld].multiSelectKeys.count(); key++)
                        {
                            leftjoin = leftjoin + " AND " + tables[pos].name + "." + tables[pos].fields[fld].multiSelectKeys[key] + " = " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectKeys[key];
                        }
                        leftjoins.append(leftjoin);
                        if (resolveMultiSelects)
                        {
                            leftjoin = "LEFT JOIN " + tables[pos].fields[fld].multiSelectRelTable + " AS T" + QString::number(lkpTblIndex) + " ON " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectField + " = T" + QString::number(lkpTblIndex) + "." + tables[pos].fields[fld].multiSelectRelField;
                            leftjoins.append(leftjoin);
                        }
                    }
                }
            }

            sql = "SELECT " + fields.join(",") + " FROM " + tables[pos].name;

            if (leftjoins.length() > 0)
            {
                sql = "SET SQL_MODE = '';\n" + sql;
                sql = sql + " " + leftjoins.join(" ");
                QStringList grpKeys;
                for (int fld = 0; fld < tables[pos].fields.count(); fld++)
                    if (tables[pos].fields[fld].isKey)
                    {
                        if (tables[pos].fields[fld].sensitive == false)
                            grpKeys.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                        else
                        {
                            if (this->protectSensitive)
                                grpKeys.append(tables[pos].name + ".rowuuid");
                            else
                                grpKeys.append(tables[pos].fields[fld].name);
                        }
                    }                
                if (primaryKey != "")
                {
                    sql = sql + " WHERE " + tables[pos].name + "." + primaryKeyField + " = '" + primaryKey + "'";
                }
                sql = sql + " GROUP BY " + grpKeys.join(",");
            }
            else
            {
                if (primaryKey != "")
                {
                    sql = sql + " WHERE " + tables[pos].name + "." + primaryKeyField + " = '" + primaryKey + "'";
                }
            }
            sql = sql + ";\n";

            arguments.clear();
            arguments << "--sql";
            arguments << "--result-format=json/raw";
            arguments << "--uri=" + uri;
            QFile sqlfile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".sql");
            if (!sqlfile.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                delete mySQLDumpProcess;
                return 1;
            }
            QTextStream out(&sqlfile);
            out << sql;
            sqlfile.close();
            mySQLDumpProcess->setStandardInputFile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".sql");
            mySQLDumpProcess->setStandardOutputFile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".txt");
            mySQLDumpProcess->start("mysqlsh", arguments);
            mySQLDumpProcess->waitForFinished(-1);
            if ((mySQLDumpProcess->exitCode() > 0) || (mySQLDumpProcess->error() == QProcess::FailedToStart))
            {
                if (mySQLDumpProcess->error() == QProcess::FailedToStart)
                {
                    log("Error: Command mysqlsh not found");
                }
                else
                {
                    log("Running mysqlsh returned error");
                    QString serror = mySQLDumpProcess->readAllStandardError();
                    log(serror);
                    log("Running paremeters:" + arguments.join(" "));
                }
                delete mySQLDumpProcess;
                return 1;
            }
            jsonFiles.append(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".txt");
        }
        QString DataFile = currDir.absolutePath() + currDir.separator() + "data.txt";
        if (jsonFiles.count() > 1)
        {
            arguments.clear();
            for (int pos = 0; pos < jsonFiles.count(); pos++)
            {
                arguments.append(jsonFiles[pos]);
            }
            mySQLDumpProcess->setStandardOutputFile(DataFile);
            mySQLDumpProcess->setStandardInputFile(QProcess::nullDevice());
            mySQLDumpProcess->start("cat", arguments);
            mySQLDumpProcess->waitForFinished(-1);
            if ((mySQLDumpProcess->exitCode() > 0) || (mySQLDumpProcess->error() == QProcess::FailedToStart))
            {
                if (mySQLDumpProcess->error() == QProcess::FailedToStart)
                {
                    log("Error: Command xsv not found");
                }
                else
                {
                    log("Running xsv returned error");
                    QString serror = mySQLDumpProcess->readAllStandardError();
                    log(serror);
                    log("Running paremeters:" + arguments.join(" "));
                }
                delete mySQLDumpProcess;
                return 1;
            }
        }
        else
        {
            if (jsonFiles.count() > 0)
                QFile::rename(jsonFiles[0],DataFile);
        }

        QFileInfo dataInfo(DataFile);
        if (dataInfo.size() == 0)
        {
            qDebug() << "There is no data to process";
            delete mySQLDumpProcess;
            return 1;
        }

        QFileInfo outputDir(currDir.absolutePath() + currDir.separator() + "jsons");
        if (!outputDir.exists())
            currDir.mkdir(currDir.absolutePath() + currDir.separator() + "jsons");
        currDir.setPath(currDir.absolutePath() + currDir.separator() + "jsons");

        mongocxx::instance instance{}; // This should be done only once.
        mongocxx::uri mongo_uri("mongodb://localhost:27017");
        mongocxx::client client(mongo_uri);
        mongocxx::database mongoDB = client["mysqldenormalize"];
        //We create a collection based on the last 12 digits of a UUID
        QUuid collectionUUID=QUuid::createUuid();
        QString strCollectionUUID=collectionUUID.toString().replace("{","").replace("}","");
        strCollectionUUID = "C" + strCollectionUUID.right(12);
        mongocxx::collection coll = mongoDB[strCollectionUUID.toUtf8().constData()];

        auto index_specification = bsoncxx::builder::stream::document{} << "rowuuid" << 1 << bsoncxx::builder::stream::finalize;
        coll.create_index(std::move(index_specification));

        arguments.clear();
        arguments.append("--db");
        arguments.append("mysqldenormalize");
        arguments.append("--collection");
        arguments.append(strCollectionUUID);
        arguments.append("--file");
        arguments.append(DataFile);

        mySQLDumpProcess->setStandardOutputFile(QProcess::nullDevice());
        mySQLDumpProcess->setStandardInputFile(QProcess::nullDevice());
        mySQLDumpProcess->start("mongoimport", arguments);
        mySQLDumpProcess->waitForFinished(-1);
        if ((mySQLDumpProcess->exitCode() > 0) || (mySQLDumpProcess->error() == QProcess::FailedToStart))
        {
            if (mySQLDumpProcess->error() == QProcess::FailedToStart)
            {
                log("Error: Command mongoimport not found");
            }
            else
            {
                log("Running mongoimport returned error");
                QString serror = mySQLDumpProcess->readAllStandardError();
                log(serror);
                log("Running paremeters:" + arguments.join(" "));
            }
            delete mySQLDumpProcess;
            return 1;
        }

        delete mySQLDumpProcess;

        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
        db.setHostName(host);
        db.setPort(port.toInt());
        db.setDatabaseName(schema);
        db.setUserName(user);
        db.setPassword(pass);
        db.setConnectOptions("MYSQL_OPT_SSL_MODE=SSL_MODE_DISABLED");
        if (db.open())
        {
            mongo_collection = coll;
            sql = "SELECT surveyid FROM " + mainTable;
            QStringList lstIds;
            QSqlQuery qryIds(db);
            qryIds.exec(sql);
            while (qryIds.next())
                lstIds << qryIds.value(0).toString();

            log("Generating trees");
            for (int pos = 0; pos <= lstIds.count()-1; pos++)
            {
                processMapFile(lstIds[pos]);
            }
//            if (lstIds.count() >= 100000)
//            {
//                // If we are working with more than 1000 submissions
//                // then separate the creation of map files into 20 threads
//                int size = lstIds.count() / 2;
//                int pos = 0, arrsize = lstIds.size(), sizeInArray = size;
//                QList<QStringList > arrays;
//                while(pos<arrsize){
//                    QStringList arr = lstIds.mid(pos, sizeInArray);
//                    arrays << arr;
//                    pos+=arr.size();
//                }
//                qDebug() << "Total Arrays: " + QString::number(arrays.count());
//                QList< QFuture<void>> threads;
//                for( int i=0; i<arrays.count(); i++)
//                {
//                    threads.append(QtConcurrent::run(this, &mainClass::processSection,arrays[i]));
//                }
//                for( int i=0; i< threads.count(); i++ )
//                {
//                    threads[i].waitForFinished(); //Wait for the end of the thread
//                }

//            }
//            else
//            {
//                log("Generating trees");
//                for (int pos = 0; pos <= lstIds.count()-1; pos++)
//                {
//                    processMapFile(lstIds[pos]);
//                }
//            }
//            qDebug() << "Sleeping";
//            sleep(120);
            coll.drop();
            db.close();
        }
        else
        {
            coll.drop();
            log("Cannot connect to the database");
            return 1;
        }

        int Hours;
        int Minutes;
        int Seconds;
        int Milliseconds;

        if (returnCode == 0)
        {
            Milliseconds = procTime.elapsed();
            Hours = Milliseconds / (1000*60*60);
            Minutes = (Milliseconds % (1000*60*60)) / (1000*60);
            Seconds = ((Milliseconds % (1000*60*60)) % (1000*60)) / 1000;
            log("Finished in " + QString::number(Hours) + " Hours," + QString::number(Minutes) + " Minutes and " + QString::number(Seconds) + " Seconds.");
        }
        return returnCode;
    }
    else
    {
        log("The input create XML file is not valid");
        returnCode = 1;
        return returnCode;
    }
}

void mainClass::processSection(QStringList section)
{
    for (int pos = 0; pos <= section.count()-1; pos++)
    {
        processMapFile(section[pos]);
    }
}

void mainClass::run()
{
    if (QFile::exists(createXML))
    {
        QDir tDir;
        if (!tDir.exists(tempDir))
        {
            if (!tDir.mkdir(tempDir))
            {
                log("Cannot create temporary directory");
                returnCode = 1;
                emit finished();
            }
        }

        if (generateXLSX() == 0)
        {
            returnCode = 0;
            emit finished();
        }
        else
        {
            returnCode = 1;
            emit finished();
        }
    }
    else
    {
        log("The create XML file does not exists");
        returnCode = 1;
        emit finished();
    }
}

QList<TUUIDFieldDef> mainClass::getDataByRowUUID4(QVector<TUUIDDef> dataList, QString UUIDToSearch)
{
    QList<TUUIDFieldDef> records;

    for (int pos = 0; pos <= dataList.count()-1;pos++)
    {
        if ((dataList[pos].UUID == UUIDToSearch))
        {
            return dataList[pos].fields;
        }
    }

        log("Empty result for UUID " + UUIDToSearch);

    return records;
}


void mainClass::parseMapFileWithBoost(QVector <TUUIDDef> dataList, QDomNode node, pt::ptree &json, pt::ptree &parent)
{
    QDomElement elem;
    elem = node.toElement();
    QString tableName;
    QString UUID;
    tableName = elem.attribute("table");
    UUID = elem.attribute("uuid");
    //Get the data for a UUID in a table and add it to the JSON object
    QList<TUUIDFieldDef> records;
    if (tableName.indexOf("_msel_") == -1)
        records = getDataByRowUUID4(dataList,UUID);
    for (int pos = 0; pos <= records.count()-1; pos++)
    {
        json.put(records[pos].name.toStdString(), records[pos].value.toStdString());
    }
    //If the current node has a child record then process the child
    //by recursively call this process. The subtable is a JSON array
    if (!node.firstChild().isNull())
    {
        elem = node.firstChild().toElement();
        tableName = elem.attribute("table");

        pt::ptree childObject;
        parseMapFileWithBoost(dataList,node.firstChild(),childObject,json); //RECURSIVE!!!
        pt::ptree array;
        pt::ptree::const_assoc_iterator it;
        it = json.find(tableName.toStdString());
        if (it != json.not_found())
            array = json.get_child(tableName.toStdString());
        array.push_back(std::make_pair("", childObject));
        if (tableName.indexOf("_msel_") == -1)
            json.put_child(tableName.toStdString(), array);

    }
    //If the current node has siblings.
    if (!node.nextSibling().isNull())
    {
        QDomNode nextSibling;
        nextSibling = node.nextSibling();
        //Go trhough each sibbling
        while (!nextSibling.isNull())
        {
            elem = nextSibling.toElement();
            tableName = elem.attribute("table");
            UUID = elem.attribute("uuid");
            //New siblings usually refer to records in the same table
            //We only create an JSON Array if the table changes from
            //one sibling to another

            //Each sibling table is stored as a JSON Array
            pt::ptree array2;

            pt::ptree::const_assoc_iterator it2;
            it2= parent.find(tableName.toStdString());
            if (it2 != parent.not_found())
                array2 = parent.get_child(tableName.toStdString());
            pt::ptree childObject2;
            QList<TUUIDFieldDef> records2;
            if (tableName.indexOf("_msel_") == -1)
                records2 = getDataByRowUUID4(dataList,UUID);
            for (int pos = 0; pos <= records2.count()-1; pos++)
            {
                childObject2.put(records2[pos].name.toStdString(),records2[pos].value.toStdString());
            }
            //If the sibling has a child then recursively call
            //this function.
            if (!nextSibling.firstChild().isNull())
            {
                QString tableName2;
                QDomElement elem2;
                elem2 = nextSibling.firstChild().toElement();
                tableName2 = elem2.attribute("table");

                pt::ptree childObject3;
                parseMapFileWithBoost(dataList,nextSibling.firstChild(),childObject3,childObject2); //!!RECURSIVE
                pt::ptree array3;

                pt::ptree::const_assoc_iterator it3;
                it3 = childObject2.find(tableName2.toStdString());
                if (it3 != childObject2.not_found())
                    array3 = childObject2.get_child(tableName2.toStdString());
                array3.push_back(std::make_pair("", childObject3));
                if (tableName2.indexOf("_msel_") == -1)
                    childObject2.put_child(tableName2.toStdString(), array3);
            }
            array2.push_back(std::make_pair("", childObject2));
            if (tableName.indexOf("_msel_") == -1)
                parent.put_child(tableName.toStdString(), array2);

            nextSibling = nextSibling.nextSibling();
        }
    }
}


void mainClass::getAllUUIDs(QDomNode node,QStringList &UUIDs)
{
    QDomNode sibling;
    sibling = node;
    while (!sibling.isNull())
    {
        QDomElement eSibling;
        eSibling = sibling.toElement();
        if (eSibling.attribute("table","None") != "None" && eSibling.attribute("uuid","None") != "None")
        {
            QString table = eSibling.attribute("table","None");
            if (table.indexOf("_msel_") < 0)
            {
                UUIDs.append(eSibling.attribute("uuid","None"));
            }
        }
        if (!sibling.firstChild().isNull())
            getAllUUIDs(sibling.firstChild(),UUIDs);
        sibling = sibling.nextSibling();
    }
}

void mainClass::processMapFile(QString fileName)
{    
    QDir mapPath(mapDir);
    QString mapFile;
    mapFile = mapPath.absolutePath() + mapPath.separator() + fileName + ".xml";

    QDomDocument doc("mapfile");
    QFile file(mapFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        log("Map file " + mapFile + " not found");
        return;
    }
    if (!doc.setContent(&file))
    {
        file.close();
        log("Cannot parse map file " + mapFile);
        return;
    }
    file.close();
    QDomNode root;
    root = doc.firstChild().nextSibling().firstChild();

    QStringList UUIDs;
    getAllUUIDs(root,UUIDs);    
    QString mongoQry;
    mongoQry = "{ \"$or\" :[";
    for (int pos = 0; pos <= UUIDs.count()-1;pos++)
    {
        mongoQry = mongoQry + "{\"rowuuid\":\"" + UUIDs[pos] + "\"},";
    }
    mongoQry = mongoQry.left(mongoQry.length()-1);
    mongoQry = mongoQry + "]}";
    std::string utf8_text = mongoQry.toUtf8().constData();
    bsoncxx::string::view_or_value qry(utf8_text);
    bsoncxx::document::value bsondoc = bsoncxx::from_json(qry.view());
    mongocxx::cursor cursor = mongo_collection.find(bsondoc.view());

    QVector <TUUIDDef> dataList;
    for (bsoncxx::document::view doc : cursor)
    {
        TUUIDDef aRowUUID;
        for (bsoncxx::document::element ele : doc)
        {
            mongocxx::stdx::string_view field_key{ele.key()};
            QString key = QString::fromUtf8(field_key.data());
            QString value;
            if (key != "_id")
            {
                if (ele.type() == bsoncxx::type::k_utf8)
                {
                    value = QString::fromUtf8(ele.get_utf8().value.data());
                    if (key == "rowuuid")
                        aRowUUID.UUID = value;
                }
                if (ele.type() == bsoncxx::type::k_null)
                {
                    value = "";
                }
                if (ele.type() == bsoncxx::type::k_double)
                {
                    value = QString::number(ele.get_double());
                }
                if (ele.type() == bsoncxx::type::k_int32)
                {
                    value = QString::number(ele.get_int32());
                }
                if (ele.type() == bsoncxx::type::k_int64)
                {
                    value = QString::number(ele.get_int64());
                }
                TUUIDFieldDef aField;
                aField.name = key;
                aField.value = value;
                aRowUUID.fields.append(aField);
            }
        }
        dataList.append(aRowUUID);
    }

    QDomElement elem;
    elem = root.toElement();
    pt::ptree JSONRootBoost;
    parseMapFileWithBoost(dataList,root,JSONRootBoost,JSONRootBoost);
    QDir outputPath(outputDir);

    QString JSONFileBoost;
    JSONFileBoost = outputPath.absolutePath() + mapPath.separator() + fileName + ".json";
    pt::write_json(JSONFileBoost.toStdString(),JSONRootBoost);


}
