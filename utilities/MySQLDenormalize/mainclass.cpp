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
#ifndef Q_MOC_RUN
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/options/find.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#endif


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

void mainClass::setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, bool protectSensitive, QString tempDir, QString encryption_key, QString mapDir, QString outputDir, QString mainTable, QString resolve_type, QString primaryKey, QString primaryKeyValue, QString separator, bool useODKFormat)
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
    this->resolve_type = resolve_type.toInt();
    this->primaryKey = primaryKey;
    this->primaryKeyValue = primaryKeyValue;
    this->separator = separator;
    this->useODKFormat = useODKFormat;
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
    aTable.ODKname = eTable.attribute("xmlcode","NONE");
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
            aField.ODKname = eField.attribute("xmlcode","NONE");
            aField.desc = eField.attribute("desc","");
            aField.type = eField.attribute("type","");
            aField.size = eField.attribute("size","").toInt();
            aField.decSize = eField.attribute("decsize","").toInt();

            if (eField.attribute("rlookup","false") == "true")
            {
                aField.isLookUp = true;
                aField.lookupRelTable = eField.attribute("rtable");
                aField.lookupRelField = eField.attribute("rfield");
            }

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

        QVector <TlinkedTable> linked_tables;
        QVector <TmultiSelectTable> multiSelectTables;
        for (int pos = 0; pos <= tables.count()-1; pos++)
        {                                                
            qDebug() << "Creating temp table " + tables[pos].name;
            linked_tables.clear();

            fields.clear();
            for (int fld = 0; fld < tables[pos].fields.count(); fld++)
            {
                if (this->protectSensitive)
                {
                    if (tables[pos].fields[fld].sensitive == false)
                    {
                        if (tables[pos].fields[fld].isMultiSelect == false)
                        {
                            if (tables[pos].fields[fld].isKey == false)
                            {
                                if (tables[pos].fields[fld].isLookUp == false)
                                    fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                                else
                                {
                                    if (this->resolve_type != 1)
                                    {
                                        TlinkedTable a_linked_table;
                                        a_linked_table.field = tables[pos].fields[fld].name;
                                        a_linked_table.related_table = tables[pos].fields[fld].lookupRelTable;
                                        a_linked_table.related_field = tables[pos].fields[fld].lookupRelField;
                                        linked_tables.append(a_linked_table);
                                    }
                                    if (this->resolve_type == 3)
                                    {
                                        fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                                        fields.append("'' as '" + tables[pos].fields[fld].name + "-desc'");
                                    }
                                    else
                                        fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                                }
                            }
                            else
                            {
                                if (protectedKeys.indexOf(tables[pos].fields[fld].name) < 0)
                                {

                                    fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                                }
                                else
                                    fields.append("HEX(AES_ENCRYPT(" + tables[pos].name + "." + tables[pos].fields[fld].name + ",UNHEX('" + this->encryption_key + "'))) as " + tables[pos].fields[fld].name);
                            }
                        }
                        else
                        {
                            fields.append(tables[pos].fields[fld].name);
                            TmultiSelectTable a_multiSelectTable;
                            a_multiSelectTable.field = tables[pos].fields[fld].name;
                            a_multiSelectTable.multiSelectTable = tables[pos].fields[fld].multiSelectTable;
                            a_multiSelectTable.multiSelectField = tables[pos].fields[fld].multiSelectField;
                            a_multiSelectTable.multiSelectRelTable = tables[pos].fields[fld].multiSelectRelTable;
                            a_multiSelectTable.multiSelectRelField = tables[pos].fields[fld].multiSelectRelField;
                            a_multiSelectTable.multiSelectKeys.append(tables[pos].fields[fld].multiSelectKeys);
                            multiSelectTables.append(a_multiSelectTable);
                            if (this->resolve_type == 3)
                                fields.append("'' as '" + tables[pos].fields[fld].name + "-desc'");
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
                        if (tables[pos].fields[fld].isLookUp == false)
                            fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                        else
                        {
                            if (this->resolve_type != 1)
                            {
                                TlinkedTable a_linked_table;
                                a_linked_table.field = tables[pos].fields[fld].name;
                                a_linked_table.related_table = tables[pos].fields[fld].lookupRelTable;
                                a_linked_table.related_field = tables[pos].fields[fld].lookupRelField;
                                linked_tables.append(a_linked_table);
                            }
                            if (this->resolve_type == 3)
                            {
                                fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                                fields.append("'' as '" + tables[pos].fields[fld].name + "-desc'");
                            }
                            else
                                fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                        }
                    }
                    else
                    {
                        fields.append(tables[pos].fields[fld].name);
                        TmultiSelectTable a_multiSelectTable;
                        a_multiSelectTable.field = tables[pos].fields[fld].name;
                        a_multiSelectTable.multiSelectTable = tables[pos].fields[fld].multiSelectTable;
                        a_multiSelectTable.multiSelectField = tables[pos].fields[fld].multiSelectField;
                        a_multiSelectTable.multiSelectRelTable = tables[pos].fields[fld].multiSelectRelTable;
                        a_multiSelectTable.multiSelectRelField = tables[pos].fields[fld].multiSelectRelField;
                        a_multiSelectTable.multiSelectKeys.append(tables[pos].fields[fld].multiSelectKeys);
                        multiSelectTables.append(a_multiSelectTable);
                        if (this->resolve_type == 3)
                            fields.append("'' as '" + tables[pos].fields[fld].name + "-desc'");
                    }
                }
            }
            QString temp_table;
            sql = "SET SQL_MODE = '';\n";

            QUuid recordUUID=QUuid::createUuid();
            temp_table = "TMP_" + recordUUID.toString().replace("{","").replace("}","").replace("-","_");
            if (primaryKey == "")
                sql = sql + "CREATE TABLE " + temp_table + " ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci AS SELECT " + fields.join(",") + " FROM " + tables[pos].name + ";";
            else
            {
                if (primaryKey != "" && primaryKeyValue != "")
                    sql = sql + "CREATE TABLE " + temp_table + " ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci AS SELECT " + fields.join(",") + " FROM " + tables[pos].name + " WHERE " + primaryKey + " = '" + primaryKeyValue + "';";
                else
                    sql = sql + "CREATE TABLE " + temp_table + " ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci AS SELECT " + fields.join(",") + " FROM " + tables[pos].name + ";";
            }

            arguments.clear();
            arguments.append("--host=" + this->host);
            arguments.append("--port=" + this->port);
            arguments.append("--password=" + this->pass);
            arguments.append("--user=" + this->user);
            arguments.append("--database=" + this->schema);

            QFile tempfile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".sql");
            if (!tempfile.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                delete mySQLDumpProcess;
                return 1;
            }
            QTextStream temout(&tempfile);
            temout << sql;
            tempfile.close();

            mySQLDumpProcess->setStandardInputFile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".sql");
            mySQLDumpProcess->setStandardOutputFile(QProcess::nullDevice());
            mySQLDumpProcess->start("mysql", arguments);
            mySQLDumpProcess->waitForFinished(-1);
            if (mySQLDumpProcess->exitCode() > 0)
            {
                QString serror = mySQLDumpProcess->readAllStandardError();
                log(serror);
                delete mySQLDumpProcess;
                return 1;
            }
            arguments.clear();

            QStringList sqls;
            qDebug() << "Performing Alters on temp table";
            for (int fld = 0; fld < tables[pos].fields.count(); fld++)
            {
                if (tables[pos].fields[fld].isKey)
                    sqls.append("ALTER TABLE " + temp_table + " MODIFY COLUMN " + tables[pos].fields[fld].name + " VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n");
            }

            if (multiSelectTables.count() > 0)
            {
                QStringList modifies;
                for (int i_table=0; i_table < multiSelectTables.count(); i_table++)
                {
                    modifies.append("MODIFY COLUMN " + multiSelectTables[i_table].field + " TEXT");
                    if (this->resolve_type == 3)
                        modifies.append("MODIFY COLUMN `" + multiSelectTables[i_table].field + "-desc` TEXT");
                }
                sql = "ALTER TABLE " + temp_table + " " + modifies.join(",") + ";\n";
                sqls.append(sql);
            }

            if (linked_tables.count() > 0)
            {
                QStringList modifies;
                for (int i_table=0; i_table < linked_tables.count(); i_table++)
                {
                    sql = "MODIFY COLUMN ";
                    if (this->resolve_type == 2)
                        sql = sql +  linked_tables[i_table].field;
                    else
                        sql = sql + "`" + linked_tables[i_table].field + "-desc`";
                    sql = sql + " TEXT";
                    modifies.append(sql);
                }
                sql = "ALTER TABLE " + temp_table + " " + modifies.join(",") + ";\n";
                sqls.append(sql);


                for (int i_table=0; i_table < linked_tables.count(); i_table++)
                {
                    sql = "UPDATE " + temp_table + "," + linked_tables[i_table].related_table + " AS T" + QString::number(i_table) + " SET ";
                    QString relfield = linked_tables[i_table].related_field;
                    if (this->resolve_type == 2)
                        sql = sql + temp_table + "." + linked_tables[i_table].field + " = T" + QString::number(i_table) + "." + relfield.replace("_cod","_des");
                    else
                        sql = sql + temp_table + ".`" + linked_tables[i_table].field + "-desc` = T" + QString::number(i_table) + "." + relfield.replace("_cod","_des");
                    sql = sql + " WHERE " + temp_table + "." + linked_tables[i_table].field + " = T" + QString::number(i_table) + "." + linked_tables[i_table].related_field + ";\n";
                    sqls.append(sql);
                }
            }
            if (multiSelectTables.count() > 0)
            {
                for (int i_table=0; i_table < multiSelectTables.count(); i_table++)
                {
                    if (this->resolve_type == 1 || this->resolve_type == 3)
                    {
                        sql = "UPDATE " + temp_table + " AS TA SET TA." + multiSelectTables[i_table].field + " = (SELECT GROUP_CONCAT(TB." + multiSelectTables[i_table].multiSelectField + " SEPARATOR '" + separator + "') FROM " + multiSelectTables[i_table].multiSelectTable + " as TB";
                        QStringList wheres;
                        QStringList groups;
                        for (int a_key = 0; a_key < multiSelectTables[i_table].multiSelectKeys.count(); a_key++)
                        {
                            wheres.append("TA." + multiSelectTables[i_table].multiSelectKeys[a_key] + " = TB." + multiSelectTables[i_table].multiSelectKeys[a_key]);
                            groups.append("TB." + multiSelectTables[i_table].multiSelectKeys[a_key]);
                        }
                        sql = sql + " WHERE " + wheres.join(" AND ");
                        sql = sql + " GROUP BY " + groups.join(",") + ");\n";
                        sqls.append(sql);
                    }
                    if (this->resolve_type == 2)
                    {
                        QString desc_field = multiSelectTables[i_table].multiSelectRelField;
                        desc_field = desc_field.replace("_cod","_des");
                        sql = "UPDATE " + temp_table + " AS TA SET TA." + multiSelectTables[i_table].field + " = (SELECT GROUP_CONCAT(TC." + desc_field + " SEPARATOR '" + separator + "') FROM " + multiSelectTables[i_table].multiSelectTable + " as TB," +  multiSelectTables[i_table].multiSelectRelTable + " as TC";
                        sql = sql + " WHERE TB." + multiSelectTables[i_table].multiSelectField + " = TC." + multiSelectTables[i_table].multiSelectRelField;
                        QStringList wheres;
                        QStringList groups;
                        for (int a_key = 0; a_key < multiSelectTables[i_table].multiSelectKeys.count(); a_key++)
                        {
                            wheres.append("TA." + multiSelectTables[i_table].multiSelectKeys[a_key] + " = TB." + multiSelectTables[i_table].multiSelectKeys[a_key]);
                            groups.append("TB." + multiSelectTables[i_table].multiSelectKeys[a_key]);
                        }
                        sql = sql + " AND " + wheres.join(" AND ");
                        sql = sql + " GROUP BY " + groups.join(",") + ");\n";
                        sqls.append(sql);
                    }
                    if (this->resolve_type == 3)
                    {
                        QString desc_field = multiSelectTables[i_table].multiSelectRelField;
                        desc_field = desc_field.replace("_cod","_des");
                        sql = "UPDATE " + temp_table + " AS TA SET TA.`" + multiSelectTables[i_table].field + "-desc` = (SELECT GROUP_CONCAT(TC." + desc_field + " SEPARATOR '" + separator + "') FROM " + multiSelectTables[i_table].multiSelectTable + " as TB," +  multiSelectTables[i_table].multiSelectRelTable + " as TC";
                        sql = sql + " WHERE TB." + multiSelectTables[i_table].multiSelectField + " = TC." + multiSelectTables[i_table].multiSelectRelField;
                        QStringList wheres;
                        QStringList groups;
                        for (int a_key = 0; a_key < multiSelectTables[i_table].multiSelectKeys.count(); a_key++)
                        {
                            wheres.append("TA." + multiSelectTables[i_table].multiSelectKeys[a_key] + " = TB." + multiSelectTables[i_table].multiSelectKeys[a_key]);
                            groups.append("TB." + multiSelectTables[i_table].multiSelectKeys[a_key]);
                        }
                        sql = sql + " AND " + wheres.join(" AND ");
                        sql = sql + " GROUP BY " + groups.join(",") + ");\n";
                        sqls.append(sql);
                    }
                }
            }
//            qDebug() << "*****************" + tables[pos].name + "**********************" ;
//            for (int p=0; p < sqls.count(); p++)
//                qDebug() << sqls[p];

            if (sqls.count() > 0)
            {
                QFile modfile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".sql");
                if (!modfile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    delete mySQLDumpProcess;
                    return 1;
                }
                QTextStream modOut(&modfile);
                for (int isql=0; isql < sqls.count(); isql++)
                    modOut << sqls[isql];
                modfile.close();

                arguments.clear();
                arguments.append("--host=" + this->host);
                arguments.append("--port=" + this->port);
                arguments.append("--password=" + this->pass);
                arguments.append("--user=" + this->user);
                arguments.append("--database=" + this->schema);

                mySQLDumpProcess->setStandardInputFile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".sql");
                mySQLDumpProcess->setStandardOutputFile(QProcess::nullDevice());
                mySQLDumpProcess->start("mysql", arguments);
                mySQLDumpProcess->waitForFinished(-1);
                if (mySQLDumpProcess->exitCode() > 0)
                {
                    QString serror = mySQLDumpProcess->readAllStandardError();
                    log(serror);
                    delete mySQLDumpProcess;
                    return 1;
                }
            }
            linked_tables.clear();
            multiSelectTables.clear();
            qDebug() << "Quering table " + tables[pos].name;
            if (primaryKey == "")
                sql = "SELECT * FROM " + temp_table + ";";
            else
            {
                if (primaryKey != "" && primaryKeyValue != "")
                {
                    sql = "SELECT * FROM " + temp_table + " WHERE " + primaryKey + " = '" + primaryKeyValue + "';";
                }
                else
                    sql = "SELECT * FROM " + temp_table + ";";
            }
            qDebug() << sql;
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

            arguments.clear();
            arguments.append("--host=" + this->host);
            arguments.append("--port=" + this->port);
            arguments.append("--password=" + this->pass);
            arguments.append("--user=" + this->user);
            arguments.append("--database=" + this->schema);
            arguments.append("--execute=DROP TABLE " + temp_table );

            mySQLDumpProcess->setStandardInputFile(QProcess::nullDevice());
            mySQLDumpProcess->setStandardOutputFile(QProcess::nullDevice());
            mySQLDumpProcess->start("mysql", arguments);
            mySQLDumpProcess->waitForFinished(-1);
            if (mySQLDumpProcess->exitCode() > 0)
            {
                QString serror = mySQLDumpProcess->readAllStandardError();
                log(serror);
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
        //db.setConnectOptions("MYSQL_OPT_SSL_MODE=SSL_MODE_DISABLED");
        if (db.open())
        {
            mongo_collection = coll;
            if (primaryKey == "")
                sql = "SELECT surveyid FROM " + mainTable;
            else
            {
                if (primaryKey != "" && primaryKeyValue != "")
                    sql = "SELECT surveyid FROM " + mainTable + " WHERE " + primaryKey + " = '" + primaryKeyValue + "'";
                else
                    sql = "SELECT surveyid FROM " + mainTable;
            }
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

    if (useODKFormat)
    {
        QString BKFile;
        BKFile = outputPath.absolutePath() + mapPath.separator() + fileName + ".bk";

        QProcess *mySQLDumpProcess = new QProcess();

        //Rename the primary key
        mySQLDumpProcess->start("bash", QStringList() << "-c" << "jq 'with_entries(if .key == \"" + primaryKey + "\" then .key = \"PRIMARY\" else . end)' " + JSONFileBoost + " > " + BKFile + " && mv " + BKFile + " " + JSONFileBoost);
        mySQLDumpProcess->waitForFinished(-1);
        for (int t=0; t < tables.count(); t++)
        {
            if (tables[t].ODKname != "main" && tables[t].ODKname != "NONE")
            {
                mySQLDumpProcess->start("bash", QStringList() << "-c" << "jq '(.. | select(has(\"" + tables[t].name + "\")?)) |= with_entries(if .key == \"" + tables[t].name + "\" then .key = \"" + tables[t].ODKname + "\" else . end)' " + JSONFileBoost + " > " + BKFile + " && mv " + BKFile + " " + JSONFileBoost);
                mySQLDumpProcess->waitForFinished(-1);
            }
            for (int f=0; f < tables[t].fields.count(); f++)
            {
                if (tables[t].fields[f].ODKname != "NONE")
                {
                    if (tables[t].fields[f].isKey == false)
                    {
                        mySQLDumpProcess->start("bash", QStringList() << "-c" << "jq '(.. | select(has(\"" + tables[t].fields[f].name + "\")?)) |= with_entries(if .key == \"" + tables[t].fields[f].name + "\" then .key = \"" + tables[t].fields[f].ODKname + "\" else . end)' " + JSONFileBoost + " > " + BKFile + " && mv " + BKFile + " " + JSONFileBoost);
                        mySQLDumpProcess->waitForFinished(-1);
                    }
                    else
                    {
                        // Remove all keys
                        mySQLDumpProcess->start("bash", QStringList() << "-c" << "jq 'walk(if type == \"object\" then del(." + tables[t].fields[f].name + ") else . end)' " + JSONFileBoost + " > " + BKFile + " && mv " + BKFile + " " + JSONFileBoost);
                        mySQLDumpProcess->waitForFinished(-1);
                    }
                }
                else
                {
                    // Remove all interal columns like rowuuid
                    mySQLDumpProcess->start("bash", QStringList() << "-c" << "jq 'walk(if type == \"object\" then del(." + tables[t].fields[f].name + ") else . end)' " + JSONFileBoost + " > " + BKFile + " && mv " + BKFile + " " + JSONFileBoost);
                    mySQLDumpProcess->waitForFinished(-1);
                }
            }
        }
        // Rename the primary key back
        mySQLDumpProcess->start("bash", QStringList() << "-c" << "jq 'with_entries(if .key == \"PRIMARY\" then .key = \"" + primaryKey + "\" else . end)' " + JSONFileBoost + " > " + BKFile + " && mv " + BKFile + " " + JSONFileBoost);
        mySQLDumpProcess->waitForFinished(-1);

        for (int t=0; t < tables.count(); t++)
        {
            for (int f=0; f < tables[t].fields.count(); f++)
            {
                if (tables[t].fields[f].ODKname != "NONE")
                {
                    if (tables[t].fields[f].isKey == true)
                    {
                        mySQLDumpProcess->start("bash", QStringList() << "-c" << "jq '(.. | select(has(\"" + tables[t].fields[f].name + "\")?)) |= with_entries(if .key == \"" + tables[t].fields[f].name + "\" then .key = \"" + tables[t].fields[f].ODKname + "\" else . end)' " + JSONFileBoost + " > " + BKFile + " && mv " + BKFile + " " + JSONFileBoost);
                        mySQLDumpProcess->waitForFinished(-1);
                    }
                }
            }
        }

        delete mySQLDumpProcess;
    }

}
