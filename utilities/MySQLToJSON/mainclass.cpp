#include "mainclass.h"
#include <QDir>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QProcess>
#include <QChar>
#include <QTime>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <xlsxwriter.h>
#include <QDebug>
#include <QUuid>
#include "jsonworker.h"
#include "listmutex.h"

namespace pt = boost::property_tree;

mainClass::mainClass(QObject *parent) : QObject(parent)
{
    returnCode = 0;
    letterIndex = 1;
}

void mainClass::log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s", temp.toUtf8().data());
}

void mainClass::setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, QString outputDir, bool protectSensitive, QString tempDir, bool incLookups, bool incmsels, QString firstSheetName, QString encryption_key, QString resolve_type, int num_workers)
{
    this->host = host;
    this->port = port;
    this->user = user;
    this->pass = pass;
    this->num_workers = num_workers;
    this->schema = schema;
    this->outputDirectory = outputDir;
    this->protectSensitive = protectSensitive;
    this->tempDir = tempDir;
    this->createXML = createXML;
    this->incLookups = incLookups;
    this->incmsels = incmsels;
    this->firstSheetName = firstSheetName;    
    this->encryption_key = encryption_key;
    this->resolve_type = resolve_type.toInt();
}

QString mainClass::getSheetDescription(QString name)
{
    QString truncated;
    truncated = name.left(25);
    truncated = truncated.replace("[","");
    truncated = truncated.replace("]","");
    truncated = truncated.replace(":","");
    truncated = truncated.replace("*","");
    truncated = truncated.replace("?","");
    truncated = truncated.replace("/","");
    truncated = truncated.replace("\\","");
    if (tableNames.indexOf(truncated) == -1)
    {
        tableNames.append(truncated);
        return truncated;
    }
    else
    {
        truncated = truncated + "_" + QString::number(letterIndex);
        letterIndex++;
        tableNames.append(truncated);
        return truncated;
    }
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

void mainClass::processTasks(QDir currDir)
{
    ListMutex *mutex = new ListMutex(this);
    mutex->set_total(task_list.count());

    QList< JSONWorker*> workers;
    for (int w=1; w <= num_workers; w++)
    {
        JSONWorker *a_worker = new JSONWorker(this);
        a_worker->setName("Worker" + QString::number(w));
        a_worker->setMutex(mutex);
        a_worker->setParameters(host, port, user, pass, schema, currDir, outputDirectory);
        a_worker->setTasks(task_list);
        workers.append(a_worker);
    }
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->start();
    }
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->wait();
    }
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

    //Getting the fields to export from tables
    QDomNode table = rootA.firstChild().nextSibling().firstChild();

    //Load the data tables recursively
    loadTable(table);
    for (int nt =mainTables.count()-1; nt >= 0;nt--)
    {
        if (mainTables[nt].name.indexOf("_msel_") < 0)
            tables.append(mainTables[nt]);
        else
        {
            if (incmsels)
                tables.append(mainTables[nt]);
        }
    }
    if (firstSheetName != "")
        tables[0].desc = firstSheetName;


    if (rootA.tagName() == "XMLSchemaStructure")
    {
        if (this->incLookups)
        {
            QDomNode lkpTable = rootA.firstChild().firstChild();
            //Getting the fields to export from Lookup tables
            while (!lkpTable.isNull())
            {
                QDomElement eTable;
                eTable = lkpTable.toElement();

                TtableDef aTable;
                aTable.islookup = true;
                aTable.name = eTable.attribute("name","");
                aTable.desc = eTable.attribute("name","");

                QDomNode field = lkpTable.firstChild();
                while (!field.isNull())
                {
                    QDomElement eField;
                    eField = field.toElement();

                    TfieldDef aField;
                    aField.name = eField.attribute("name","");
                    aField.desc = eField.attribute("desc","");
                    aField.type = eField.attribute("type","");
                    if (eField.attribute("sensitive","false") == "true")
                    {
                        aField.sensitive = true;
                        aField.protection = eField.attribute("protection","exclude");
                    }
                    else
                        aField.sensitive = false;
                    aField.size = eField.attribute("size","").toInt();
                    aField.decSize = eField.attribute("decsize","").toInt();
                    aTable.fields.append(aField);

                    field = field.nextSibling();
                }
                tables.append(aTable);

                lkpTable = lkpTable.nextSibling();
            }
        }

        QDir currDir(tempDir);        

        QTime procTime;
        procTime.start();
        QString sql;
        QStringList fields;
        QStringList descfields;
        QString uri = user + ":" + pass + "@" + host + "/" + schema;
        QStringList sheets;


        QVector <TlinkedTable> linked_tables;
        QVector <TmultiSelectTable> multiSelectTables;

        for (int pos = 0; pos <= tables.count()-1; pos++)
        {                                                            
            linked_tables.clear();
            sheets.append(getSheetDescription(tables[pos].desc));
            fields.clear();
            descfields.clear();
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

                                    fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                                    fields.append("'' as '" + tables[pos].fields[fld].name + "-desc'");
                                    descfields.append(tables[pos].fields[fld].name + "-desc");

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
                            if (this->incmsels == false)
                            {
                                TmultiSelectTable a_multiSelectTable;
                                a_multiSelectTable.field = tables[pos].fields[fld].name;
                                a_multiSelectTable.multiSelectTable = tables[pos].fields[fld].multiSelectTable;
                                a_multiSelectTable.multiSelectField = tables[pos].fields[fld].multiSelectField;
                                a_multiSelectTable.multiSelectRelTable = tables[pos].fields[fld].multiSelectRelTable;
                                a_multiSelectTable.multiSelectRelField = tables[pos].fields[fld].multiSelectRelField;
                                a_multiSelectTable.multiSelectKeys.append(tables[pos].fields[fld].multiSelectKeys);
                                multiSelectTables.append(a_multiSelectTable);
                            }
                            fields.append("'' as '" + tables[pos].fields[fld].name + "-desc'");
                            descfields.append(tables[pos].fields[fld].name + "-desc");
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

                            fields.append(tables[pos].name + "." + tables[pos].fields[fld].name);
                            fields.append("'' as '" + tables[pos].fields[fld].name + "-desc'");
                            descfields.append(tables[pos].fields[fld].name + "-desc'");

                        }
                    }
                    else
                    {                       
                        fields.append(tables[pos].fields[fld].name);
                        if (this->incmsels == false)
                        {
                            TmultiSelectTable a_multiSelectTable;
                            a_multiSelectTable.field = tables[pos].fields[fld].name;
                            a_multiSelectTable.multiSelectTable = tables[pos].fields[fld].multiSelectTable;
                            a_multiSelectTable.multiSelectField = tables[pos].fields[fld].multiSelectField;
                            a_multiSelectTable.multiSelectRelTable = tables[pos].fields[fld].multiSelectRelTable;
                            a_multiSelectTable.multiSelectRelField = tables[pos].fields[fld].multiSelectRelField;
                            a_multiSelectTable.multiSelectKeys.append(tables[pos].fields[fld].multiSelectKeys);
                            multiSelectTables.append(a_multiSelectTable);
                        }
                        fields.append("'' as '" + tables[pos].fields[fld].name + "-desc'");
                        descfields.append(tables[pos].fields[fld].name + "-desc'");
                    }
                }
            }
            QString temp_table;
            sql = "SET SQL_MODE = '';\n";

            QUuid recordUUID=QUuid::createUuid();
            temp_table = "TMP_" + recordUUID.toString().replace("{","").replace("}","").replace("-","_");
            sql = sql + "CREATE TABLE " + temp_table + " ENGINE=MyISAM DEFAULT CHARACTER SET utf8mb4 DEFAULT COLLATE utf8mb4_unicode_ci AS SELECT " + fields.join(",") + " FROM " + tables[pos].name + ";";

            TtaskItem a_task;
            a_task.table = temp_table;

            QFile tempfile(currDir.absolutePath() + currDir.separator() + tables[pos].name + "_create.sql");
            if (!tempfile.open(QIODevice::WriteOnly | QIODevice::Text))
            {                
                return 1;
            }
            QTextStream temout(&tempfile);
            temout << sql;
            tempfile.close();

            a_task.create_sql = currDir.absolutePath() + currDir.separator() + tables[pos].name + "_create.sql";


            QStringList sqls;            
            sqls.append("START TRANSACTION;\n");
            for (int fld = 0; fld < tables[pos].fields.count(); fld++)
            {
                if (tables[pos].fields[fld].isKey)
                    sqls.append("ALTER TABLE " + temp_table + " MODIFY COLUMN " + tables[pos].fields[fld].name + " VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n");
            }

            if (multiSelectTables.count() == 0 && linked_tables.count() == 0)
            {
                if (descfields.count() > 0)
                {
                    QStringList modifies;
                    for (int i = 0; i < descfields.count(); i++)
                    {
                        modifies.append("MODIFY COLUMN `" + descfields[i] + "` TEXT");
                    }
                    sql = "ALTER TABLE " + temp_table + " " + modifies.join(",") + ";\n";
                    sqls.append(sql);
                }
            }

            if (multiSelectTables.count() > 0 && this->incmsels == false)
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
            if (multiSelectTables.count() > 0 && this->incmsels == false)
            {
                for (int i_table=0; i_table < multiSelectTables.count(); i_table++)
                {
                    if (this->resolve_type == 1 || this->resolve_type == 3)
                    {
                        sql = "UPDATE " + temp_table + " AS TA SET TA." + multiSelectTables[i_table].field + " = (SELECT GROUP_CONCAT(TB." + multiSelectTables[i_table].multiSelectField + " SEPARATOR ',') FROM " + multiSelectTables[i_table].multiSelectTable + " as TB";
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
                        sql = "UPDATE " + temp_table + " AS TA SET TA." + multiSelectTables[i_table].field + " = (SELECT GROUP_CONCAT(TC." + desc_field + " SEPARATOR ',') FROM " + multiSelectTables[i_table].multiSelectTable + " as TB," +  multiSelectTables[i_table].multiSelectRelTable + " as TC";
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
                        sql = "UPDATE " + temp_table + " AS TA SET TA.`" + multiSelectTables[i_table].field + "-desc` = (SELECT GROUP_CONCAT(TC." + desc_field + " SEPARATOR ',') FROM " + multiSelectTables[i_table].multiSelectTable + " as TB," +  multiSelectTables[i_table].multiSelectRelTable + " as TC";
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
//            for (int p=0; p < sqls.count(); p++)
//                qDebug() << sqls[p];
            sqls.append( "COMMIT;\n");

            QFile modfile(currDir.absolutePath() + currDir.separator() + tables[pos].name + "_alter.sql");
            if (!modfile.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                return 1;
            }
            QTextStream modOut(&modfile);
            for (int isql=0; isql < sqls.count(); isql++)
                modOut << sqls[isql];
            modfile.close();
            a_task.alter_sql = currDir.absolutePath() + currDir.separator() + tables[pos].name + "_alter.sql";

            linked_tables.clear();
            multiSelectTables.clear();

            sql = "SELECT * FROM " + temp_table + ";";

            QFile sqlfile(currDir.absolutePath() + currDir.separator() + tables[pos].name + "_query.sql");
            if (!sqlfile.open(QIODevice::WriteOnly | QIODevice::Text))
            {                
                return 1;
            }
            QTextStream out(&sqlfile);
            out << sql;
            sqlfile.close();

            a_task.query_sql = currDir.absolutePath() + currDir.separator() + tables[pos].name + "_query.sql";
            a_task.json_file = currDir.absolutePath() + currDir.separator() + tables[pos].name + ".json";

            task_list.append(a_task);
        }

        processTasks(currDir);
        //qDebug() << "Finish workers";

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
