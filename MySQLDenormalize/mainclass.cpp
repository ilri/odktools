#include "mainclass.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFileInfoList>
#include <QVariant>
#include <QProcess>
#include <QXmlQuery>
#include <QSqlRecord>
#include <QXmlSerializer>
#include <QList>
#include <QTime>
#include <QDomNodeList>
#include <QDomNode>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>

namespace pt = boost::property_tree;

mainClass::mainClass(QObject *parent) : QObject(parent)
{
    returnCode = 0;
}

void mainClass::log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf(temp.toLocal8Bit().data());
}

void mainClass::setParameters(QString host, QString port, QString user, QString pass, QString schema, QString table, QString mapDir, QString output, bool includeProtected)
{
    this->host = host;
    this->port = port;
    this->user = user;
    this->pass = pass;
    this->schema = schema;
    this->table = table;
    this->mapDir = mapDir;
    this->output = output;
    this->includeProtected = includeProtected;
}

//This function goes trough a series of tables and their childs
//to obtain the child tables from the main table (start table)
void mainClass::processChilds(QDomDocument doc, QDomElement &parent, QString table, QList <TtableDef> tables, QStringList &tablesUsed)
{
    tablesUsed.append(table);
    QDomElement etable;
    etable = doc.createElement(table);
    parent.appendChild(etable);
    for (int pos = 0; pos <= tables.count()-1;pos++)
    {
        if (tables[pos].parent == table)
            processChilds(doc,etable,tables[pos].name,tables,tablesUsed);
    }
}

QList<TfieldDef> mainClass::getDataByRowUUID(QSqlDatabase db, QString tableToSearch, QString UUIDToSearch)
{
    QList<TfieldDef> records;
    QSqlQuery qry(db);
    QString sql;
    sql = "SELECT * FROM " + tableToSearch + " WHERE rowuuid = '" + UUIDToSearch + "'";
    qry.exec(sql);
    if (qry.first())
    {
        QSqlRecord record = qry.record();
        for (int pos = 0; pos <= record.count()-1; pos++)
        {
            QString fieldName;
            fieldName = record.fieldName(pos);
            TfieldDef aRecord;
            aRecord.name = fieldName;
            aRecord.value = qry.value(fieldName).toString();
            records.append(aRecord);
        }
    }
    if (records.count() == 0)
    {
        log("Empty result for " + tableToSearch + " with UUID " + UUIDToSearch);
    }
    return records;
}

//This function uses mongo to search for the data associated with a UUID in a table.
//It has been replaced by directly searching the database using an index on rowuuid
//This function return a list of fields (Field name, Field value)
//QList<TfieldDef> mainClass::getDataByRowUUID2(mongocxx::collection collection, QString tableToSearch, QString UUIDToSearch)
//{
//    QList<TfieldDef> records;
//    bsoncxx::stdx::optional<bsoncxx::document::value> query_result =
//    collection.find_one(bsoncxx::builder::stream::document{} << "_tablename" << tableToSearch.toUtf8().constData() << "rowuuid" << UUIDToSearch.toUtf8().constData() << bsoncxx::builder::stream::finalize);
//    if(query_result)
//    {
//        std::string res = bsoncxx::to_json(*query_result);
//        QString result = QString::fromStdString(res);
//        QJsonDocument doc = QJsonDocument::fromJson(result.toUtf8());
//        QJsonObject obj = doc.object();
//        QStringList keys = obj.keys();
//        for (int pos = 0; pos <= keys.count()-1; pos++)
//        {
//            QJsonValue value = obj.value(keys[pos]);
//            TfieldDef aValue;
//            aValue.name = keys[pos];
//            aValue.value = value.toString();
//            records.append(aValue);
//        }
//    }
//    if (records.count() == 0)
//    {
//        log("Empty result for " + tableToSearch + " with UUID " + UUIDToSearch);
//    }
//    return records;
//}


//This function uses xmllint to search for the data associated with a UUID in a table.
//It has been replaced by directly searching the database using an index on rowuuid
//This function return a list of fields (Field name, Field value)
//QList<TfieldDef> mainClass::getDataByRowUUID(QString tableToSearch, QString UUIDToSearch)
//{
//    QList<TfieldDef> records;
//    QString program = "xmllint";
//    QStringList arguments;
//    arguments << "--xpath";
//    arguments << "//table_data[@name='" + tableToSearch + "']//row[./field[@name='rowuuid' and . = '" + UUIDToSearch + "']]";
//    arguments << "./tmp/" + tableToSearch + ".xml";

//    //log("Running xmllint please wait....");
//    QProcess *myProcess = new QProcess();
//    myProcess->start(program, arguments);
//    myProcess->waitForFinished(-1);
//    QString result = myProcess->readAllStandardOutput();
//    if (result != "XPath set is empty")
//    {
//        QDomDocument doc("dataSection");
//        doc.setContent(result);
//        QDomNodeList fields;
//        fields = doc.elementsByTagName("field");
//        for (int pos = 0; pos <= fields.count()-1;pos++)
//        {
//            QString fieldName;
//            fieldName = fields.item(pos).toElement().attribute("name");
//            QString fieldValue;
//            fieldValue = fields.item(pos).firstChild().nodeValue();
//            //if (fieldName != "rowuuid")
//            //{
//            TfieldDef aRecord;
//            aRecord.name = fieldName;
//            aRecord.value = fieldValue;
//            records.append(aRecord);
//            //}
//        }
//    }
//    if (records.count() == 0)
//    {
//        log("Empty result for " + tableToSearch + " with UUID " + UUIDToSearch);
//    }
//    return records;
//}

//This function parse a Map File and returs the data tree in JSON form
//This is a recursive fuction
//mongocxx::collection collection
void mainClass::parseMapFile(QSqlDatabase db, QDomNode node, QJsonObject &json, QString currentTable, QJsonObject &parent)
{
    QDomElement elem;
    elem = node.toElement();
    QString tableName;
    QString UUID;
    tableName = elem.attribute("table");
    UUID = elem.attribute("uuid");

    //Get the data for a UUID in a table and add it to the JSON object
    QList<TfieldDef> records;
    records = getDataByRowUUID(db,tableName,UUID);
    for (int pos = 0; pos <= records.count()-1; pos++)
    {
        json[records[pos].name] = records[pos].value;
    }
    //If the current node has a child record then process the child
    //by recursively call this process. The subtable is a JSON array
    if (!node.firstChild().isNull())
    {
        elem = node.firstChild().toElement();
        tableName = elem.attribute("table");
        json[tableName] = QJsonArray();
        QJsonObject childObject;
        parseMapFile(db,node.firstChild(),childObject,tableName,json);
        QJsonArray array = json[tableName].toArray();
        array.append(childObject);
        json[tableName] = array;
    }
    //If the current node has siblings.
    if (!node.nextSibling().isNull())
    {
        QDomNode nextSibling;
        nextSibling = node.nextSibling();
        QString currTable;
        currTable = currentTable;
        //Go trhough each sibbling
        while (!nextSibling.isNull())
        {
            elem = nextSibling.toElement();
            tableName = elem.attribute("table");
            UUID = elem.attribute("uuid");
            //New siblings usually refer to records in the same table
            //We only create an JSON Array if the table changes from
            //one sibling to another
            if (currTable != tableName)
            {
                //Each sibling table is stored as a JSON Array
                QJsonArray array2 = parent[tableName].toArray();
                QJsonObject childObject2;
                QList<TfieldDef> records2;
                records2 = getDataByRowUUID(db,tableName,UUID);
                for (int pos = 0; pos <= records2.count()-1; pos++)
                {
                    childObject2[records2[pos].name] = records2[pos].value;
                }
                //If the sibling has a child then recursively call
                //this function.
                if (!nextSibling.firstChild().isNull())
                {
                    QString tableName2;
                    QDomElement elem2;
                    elem2 = nextSibling.firstChild().toElement();
                    tableName2 = elem2.attribute("table");
                    childObject2[tableName2] = QJsonArray();
                    QJsonObject childObject3;
                    parseMapFile(db,nextSibling.firstChild(),childObject3,currTable,childObject2);
                    QJsonArray array3;
                    array3 = childObject2[tableName2].toArray();
                    array3.append(childObject3);
                    childObject2[tableName2] = array3;
                }
                array2.append(childObject2);
                parent[tableName] = array2;
                currTable = tableName;
            }
            else
            {
                //This happend when the sibling has the same table which
                //means is a sibling record in a table
                QJsonObject childObject2;
                QList<TfieldDef> records2;
                records2 = getDataByRowUUID(db,tableName,UUID);
                for (int pos = 0; pos <= records2.count()-1; pos++)
                {
                    childObject2[records2[pos].name] = records2[pos].value;
                }
                QJsonArray currArray = parent[tableName].toArray();
                //If the sibling has a child then recursively call
                //this function.
                if (!nextSibling.firstChild().isNull())
                {
                    QString tableName2;
                    QDomElement elem2;
                    elem2 = nextSibling.firstChild().toElement();
                    tableName2 = elem2.attribute("table");
                    childObject2[tableName2] = QJsonArray();
                    QJsonObject childObject3;
                    parseMapFile(db,nextSibling.firstChild(),childObject3,currTable,childObject2);
                    QJsonArray array3;
                    array3 = childObject2[tableName2].toArray();
                    array3.append(childObject3);
                    childObject2[tableName2] = array3;
                }
                currArray.append(childObject2);
                parent[tableName] = currArray;
            }

            nextSibling = nextSibling.nextSibling();
        }
    }
}

//This function receives a Map File and construct a JSON file with the data
//The JSON file is stored in the ouput directy and has the same name
//of the Map File
void mainClass::processMapFile(QSqlDatabase db, QString fileName)
{
    QDir mapPath(mapDir);
    QString mapFile;
    mapFile = mapPath.absolutePath() + mapPath.separator() + fileName + ".xml";
    QDomDocument doc("mapfile");
    QFile file(mapFile);
    if (!file.open(QIODevice::ReadOnly))
        return;
    if (!doc.setContent(&file)) {
        file.close();
        return;
    }
    file.close();
    QDomNode root;
    root = doc.firstChild().nextSibling().firstChild();

    QDomElement elem;
    elem = root.toElement();
    QString tableName;
    tableName = elem.attribute("table");
    QJsonObject JSONRoot;
    parseMapFile(db,root,JSONRoot,tableName,JSONRoot);

    QJsonDocument saveDoc(JSONRoot);
    QDir outputPath(output);
    QString JSONFile;
    JSONFile = outputPath.absolutePath() + mapPath.separator() + fileName + ".json";

    QFile saveFile(JSONFile);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        log("Couldn't open output JSON file.");
        return;
    }
    saveFile.write(saveDoc.toJson());
    saveFile.close();
}

//This function was another attempt to minimize the time
//Now is not neeed but commented until we are sure we don't need to go back
//void mainClass::parseDataToMongo(mongocxx::collection collection, QString table, QString fileName)
//{

//    pt::ptree tree;
//    pt::read_xml(fileName.toUtf8().constData(), tree);

//    BOOST_FOREACH(boost::property_tree::ptree::value_type const&db, tree.get_child("mysqldump") )
//    {
//        const boost::property_tree::ptree & aDatabase = db.second; // value (or a subnode)
//        BOOST_FOREACH(boost::property_tree::ptree::value_type const&ctable, aDatabase.get_child("") )
//        {
//            const std::string & key = ctable.first.data();
//            if (key == "table_data")
//            {
//                const boost::property_tree::ptree & aTable = ctable.second;
//                BOOST_FOREACH(boost::property_tree::ptree::value_type const&row, aTable.get_child("") )
//                {
//                    const boost::property_tree::ptree & aRow = row.second;
//                    auto doc = bsoncxx::builder::basic::document{};
//                    using bsoncxx::builder::basic::kvp;
//                    doc.append(kvp("_tablename", table.toUtf8().constData()));
//                    BOOST_FOREACH(boost::property_tree::ptree::value_type const&field, aRow.get_child("") )
//                    {
//                        const std::string & fkey = field.first.data();
//                        if (fkey == "field")
//                        {
//                            const boost::property_tree::ptree & aField = field.second;
//                            std::string fname = aField.get<std::string>("<xmlattr>.name");
//                            std::string fvalue = aField.data();
//                            doc.append(kvp(fname,fvalue));
//                        }
//                        //QJsonDocument doc(JSONRow);
//                        //QString JSONString(doc.toJson(QJsonDocument::Compact));
//                    }
//                    collection.insert_one(doc.view());
//                }

//            }
//        }
//    }
//}

//This is the starter function. It queries the database for the relationships
//and get the child tables of the starting table (main table)
//then each table is exported in XML form using mysqldump into a temporary directory
//then for each surveyid in the main table process the respective Map File.
int mainClass::generateJSONs(QSqlDatabase db)
{
    QString sql;
    sql = "SELECT table_name,column_name,REFERENCED_TABLE_NAME,REFERENCED_COLUMN_NAME FROM information_schema.KEY_COLUMN_USAGE WHERE table_schema = '" + db.databaseName().toLower() + "' GROUP BY table_name,REFERENCED_TABLE_NAME";

    //Here we get the tables and their relationships from the schema
    QList <TtableDef> tables;
    QSqlQuery rels(db);
    rels.exec(sql);
    while (rels.next())
    {
        TtableDef aTable;
        aTable.name = rels.value("table_name").toString();
        aTable.parent = rels.value("REFERENCED_TABLE_NAME").toString();
        tables.append(aTable);
    }
    //We create an XML document to store the relationships
    QDomDocument outputdoc;
    outputdoc = QDomDocument("DBStructFile");
    QDomElement root;
    root = outputdoc.createElement("tables");
    root.setAttribute("version", "1.0");
    outputdoc.appendChild(root);
    //We get the tables used from the starting table
    QStringList tablesUsed;
    processChilds(outputdoc,root,table,tables,tablesUsed);

    /*
    This code has been replaced to directly get the data from MySQL using an index on rowuuid
    We leave it here for a while until we are sure we will not need it.
    //Connects to MONGO
    mongocxx::instance instance{}; // This should be done only once.
    mongocxx::uri uri("mongodb://localhost:27017");
    mongocxx::client client(uri);
    mongocxx::database mongoDB = client["mysqldenormalize"];
    //We create a collection based on the last 12 digits of a UUID
    QUuid collectionUUID=QUuid::createUuid();
    QString strCollectionUUID=collectionUUID.toString().replace("{","").replace("}","");
    strCollectionUUID = "C" + strCollectionUUID.right(12);
    mongocxx::collection coll = mongoDB[strCollectionUUID.toUtf8().constData()];

    auto index_specification = bsoncxx::builder::stream::document{} << "_tablename" << 1 << "rowuuid" << 1 << bsoncxx::builder::stream::finalize;
    coll.create_index(std::move(index_specification));
    //Call MySQLDump to export each table as XML
    //We use MySQLDump because it very very fast
    QDir currDir(".");
    currDir.mkdir("tmp");
    QString program = "mysqldump";
    QStringList arguments;
    QProcess *mySQLDumpProcess = new QProcess();
    for (int pos = 0; pos <= tablesUsed.count()-1; pos++)
    {
        arguments.clear();
        arguments << "--single-transaction";
        arguments << "-h" << host;
        arguments << "-u" << user;
        arguments << "--password=" + pass;
        arguments << "--skip-triggers";
        arguments << "--xml";
        arguments << "--no-create-info";
        arguments << schema;
        arguments << tablesUsed[pos];
        mySQLDumpProcess->setStandardOutputFile("./tmp/" + tablesUsed[pos] + ".xml");
        mySQLDumpProcess->start(program, arguments);
        log("Exporting " + tablesUsed[pos] + " to Mongo");
        mySQLDumpProcess->waitForFinished(-1);
        parseDataToMongo(coll,tablesUsed[pos],"./tmp/" + tablesUsed[pos] + ".xml");
    }
    */

    //Select the Survey IDs that will be exported
    sql = "SELECT surveyid FROM " + table;
    QStringList lstIds;
    QSqlQuery qryIds(db);
    qryIds.exec(sql);
    while (qryIds.next())
        lstIds << qryIds.value(0).toString();

    QTime procTime;
    procTime.start();
    for (int pos = 0; pos <= lstIds.count()-1; pos++)
    {
        processMapFile(db,lstIds[pos]);
    }
    int Hours;
    int Minutes;
    int Seconds;
    int Milliseconds;

    Milliseconds = procTime.elapsed();
    Hours = Milliseconds / (1000*60*60);
    Minutes = (Milliseconds % (1000*60*60)) / (1000*60);
    Seconds = ((Milliseconds % (1000*60*60)) % (1000*60)) / 1000;
    log("Finished in " + QString::number(Hours) + " Hours," + QString::number(Minutes) + " Minutes and " + QString::number(Seconds) + " Seconds.");

    return 0;
}

void mainClass::run()
{
    QDir dir(mapDir);
    if (dir.exists(mapDir))
    {
        QFileInfoList lstFiles;
        QStringList filters;
        filters << "*.xml";
        lstFiles = dir.entryInfoList(filters);
        if (lstFiles.count() > 0)
        {
            QDir outDir(output);
            if (outDir.exists())
            {
                QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
                db.setHostName(host);
                db.setPort(port.toInt());
                db.setDatabaseName(schema);
                db.setUserName(user);
                db.setPassword(pass);
                if (db.open())
                {
                    if (generateJSONs(db) == 0)
                    {
                        db.close();
                        emit finished();
                    }
                    else
                    {
                        db.close();
                        emit finished();
                    }
                }
            }
            else
            {
                log("Output directory does not exists");
                returnCode = -1;
                emit finished();
            }
        }
        else
        {
            log("Map directory does not contains XML files");
            returnCode = -1;
            emit finished();
        }
    }
    else
    {
        log("Map directory does not exists");
        returnCode = -1;
        emit finished();
    }
}
