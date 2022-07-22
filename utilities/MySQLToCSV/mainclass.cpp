#include "mainclass.h"
#include <QDir>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QProcess>
#include <QChar>
#include <QElapsedTimer>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <xlsxwriter.h>
#include <QDebug>
#include <QUuid>
#include "jsonworker.h"
#include "listmutex.h"
#include <QSqlQuery>

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

void mainClass::setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, QString outputDir, bool protectSensitive, QString tempDir, bool incLookups, bool incmsels, QString encryption_key, QString resolve_type, int num_workers)
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
    this->encryption_key = encryption_key;
    this->resolve_type = resolve_type.toInt();
    this->db = QSqlDatabase::addDatabase("QMYSQL","repository");
    db.setHostName(host);
    db.setPort(port.toInt());
    db.setDatabaseName(schema);
    db.setUserName(user);
    db.setPassword(pass);
    if (!db.open())
    {
        log("Error while conneting to MySQL");
        exit(1);
    }
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
    if (aTable.name.indexOf("_msel_") >= 0)
        aTable.ismultiselect = true;
    else
        aTable.ismultiselect = false;

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

int  mainClass::processTasks(QDir currDir)
{
    ListMutex *mutex = new ListMutex(this);

    QList< JSONWorker*> workers;
    for (int w=1; w <= num_workers; w++)
    {
        JSONWorker *a_worker = new JSONWorker(this);
        a_worker->setName("Worker" + QString::number(w));
        a_worker->setParameters(host, port, user, pass, schema, currDir, outputDirectory);
        workers.append(a_worker);
    }

    //qDebug() << "Working on separation: " + QString::number(separate_task_list.count());
    //Work on separation
    mutex->set_total(separate_task_list.count());
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->setTasks(separate_task_list);
        workers[w]->setMutex(mutex);
    }
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->start();
    }
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->wait();
    }
    for (int w=0; w < workers.count(); w++)
    {
        if (workers[w]->status != 0)
            return -1;
    }

    //qDebug() << "Working on applying updates: " + QString::number(update_task_list.count());
    //Work on the updates
    mutex->set_total(update_task_list.count());
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->setTasks(update_task_list);
        workers[w]->setMutex(mutex);
    }
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->start();
    }
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->wait();
    }
    for (int w=0; w < workers.count(); w++)
    {
        if (workers[w]->status != 0)
            return -1;
    }

    //qDebug() << "Working on generating JSON files";
    //Work on the JSON extract
    mutex->set_total(json_task_list.count());
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->setTasks(json_task_list);
        workers[w]->setMutex(mutex);
    }
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->start();
    }
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->wait();
    }
    for (int w=0; w < workers.count(); w++)
    {
        if (workers[w]->status != 0)
            return -1;
    }

    //qDebug() << "Working on merging the final files";
    //Work in the merging
    mutex->set_total(merge_task_list.count());
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->setTasks(merge_task_list);
        workers[w]->setMutex(mutex);
    }
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->start();
    }
    for (int w=0; w < workers.count(); w++)
    {
        workers[w]->wait();
    }
    for (int w=0; w < workers.count(); w++)
    {
        if (workers[w]->status != 0)
            return -1;
    }
    return 0;
}

QStringList mainClass::get_parts(int total, int parts)
{
    QStringList res;
    int div = total / parts;
    int start = 1;
    int end = div;
    if (parts >= total)
    {
        res.append("1|" + QString::number(total));
        return res;
    }
    if (parts == 1)
    {
        res.append("1|" + QString::number(total));
        return res;
    }
    if (total >= 500)
    {
        for (int pos = 1; pos < parts; pos++)
        {
            res.append(QString::number(start) + "|" + QString::number(end));
            start = end + 1 ;
            end = start + div;
        }
        res.append(QString::number(start) + "|" + QString::number(total));
    }
    else
    {
        res.append("1|" + QString::number(total));
    }
    return res;
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
                lookupTables.append(aTable);

                lkpTable = lkpTable.nextSibling();
            }
        }

        QDir currDir(tempDir);

        QElapsedTimer procTime;
        procTime.start();
        QString sql;
        QStringList fields;
        QStringList fields_for_select;
        QStringList descfields;
        QStringList fields_for_create;

        QVector <TlinkedTable> linked_tables;
        QVector <TmultiSelectTable> multiSelectTables;
        QStringList drops;
        if (resolve_type > 1 || (resolve_type == 1 && incmsels == false))
        {
            for (int pos = 0; pos <= tables.count()-1; pos++)
            {
                linked_tables.clear();
                fields.clear();
                fields_for_select.clear();
                fields_for_create.clear();
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
                                    {
                                        fields.append(tables[pos].fields[fld].name);
                                        fields_for_select.append(tables[pos].fields[fld].name);
                                        fields_for_create.append(tables[pos].fields[fld].name);
                                    }
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

                                        fields.append(tables[pos].fields[fld].name);
                                        fields_for_select.append(tables[pos].fields[fld].name);
                                        fields_for_create.append(tables[pos].fields[fld].name);
                                        fields.append("'' as '" + tables[pos].fields[fld].name + "-desc'");
                                        fields_for_select.append(tables[pos].fields[fld].name + "-desc");
                                        fields_for_create.append(tables[pos].fields[fld].name + "-desc");
                                        descfields.append(tables[pos].fields[fld].name + "-desc");

                                    }
                                }
                                else
                                {
                                    if (protectedKeys.indexOf(tables[pos].fields[fld].name) < 0)
                                    {

                                        fields.append(tables[pos].fields[fld].name);
                                        fields_for_select.append(tables[pos].fields[fld].name);
                                        fields_for_create.append(tables[pos].fields[fld].name);
                                    }
                                    else
                                    {
                                        fields.append("HEX(AES_ENCRYPT(" + tables[pos].fields[fld].name + ",UNHEX('" + this->encryption_key + "'))) as " + tables[pos].fields[fld].name);
                                        fields_for_select.append("HEX(AES_ENCRYPT(" + tables[pos].fields[fld].name + ",UNHEX('" + this->encryption_key + "'))) as " + tables[pos].fields[fld].name);
                                        fields_for_create.append(tables[pos].fields[fld].name);
                                    }
                                }
                            }
                            else
                            {
                                fields.append(tables[pos].fields[fld].name);
                                fields_for_select.append(tables[pos].fields[fld].name);
                                fields_for_create.append(tables[pos].fields[fld].name);
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
                                fields_for_select.append(tables[pos].fields[fld].name + "-desc");
                                fields_for_create.append(tables[pos].fields[fld].name + "-desc");
                                descfields.append(tables[pos].fields[fld].name + "-desc");
                            }
                        }
                        else
                        {
                            if (tables[pos].fields[fld].protection != "exclude")
                            {
                                fields.append("HEX(AES_ENCRYPT(" + tables[pos].fields[fld].name + ",UNHEX('" + this->encryption_key + "'))) as " + tables[pos].fields[fld].name);
                                fields_for_select.append("HEX(AES_ENCRYPT(" + tables[pos].fields[fld].name + ",UNHEX('" + this->encryption_key + "'))) as " + tables[pos].fields[fld].name);
                                fields_for_create.append(tables[pos].fields[fld].name);
                            }
                        }
                    }
                    else
                    {
                        if (tables[pos].fields[fld].isMultiSelect == false)
                        {
                            if (tables[pos].fields[fld].isLookUp == false)
                            {
                                fields.append(tables[pos].fields[fld].name);
                                fields_for_select.append(tables[pos].fields[fld].name);
                                fields_for_create.append(tables[pos].fields[fld].name);
                            }
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

                                fields.append(tables[pos].fields[fld].name);
                                fields_for_select.append(tables[pos].fields[fld].name);
                                fields_for_create.append(tables[pos].fields[fld].name);
                                fields.append("'' as '" + tables[pos].fields[fld].name + "-desc'");
                                fields_for_select.append(tables[pos].fields[fld].name + "-desc");
                                fields_for_create.append(tables[pos].fields[fld].name + "-desc");
                                descfields.append(tables[pos].fields[fld].name + "-desc");

                            }
                        }
                        else
                        {
                            fields.append(tables[pos].fields[fld].name);
                            fields_for_select.append(tables[pos].fields[fld].name);
                            fields_for_create.append(tables[pos].fields[fld].name);
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
                            fields_for_select.append(tables[pos].fields[fld].name + "-desc");
                            fields_for_create.append(tables[pos].fields[fld].name + "-desc");
                            descfields.append(tables[pos].fields[fld].name + "-desc");
                        }
                    }
                }

                QString temp_table;

                QUuid recordUUID=QUuid::createUuid();
                temp_table = "TMP_" + recordUUID.toString().replace("{","").replace("}","").replace("-","_");
                drops.append("DROP TABLE " + temp_table);
                //qDebug() << temp_table;
                QSqlQuery qry(db);
                if (!qry.exec("CREATE TABLE " + temp_table + " ENGINE=MyISAM DEFAULT CHARACTER SET utf8mb4 DEFAULT COLLATE utf8mb4_unicode_ci AS SELECT " + fields.join(",") + " FROM " + tables[pos].name + " LIMIT 0"))
                {
                    log("Cannot create temporary table: " + temp_table);
                    exit(1);
                }
                if (!qry.exec("ALTER TABLE " + temp_table + " ADD COLUMN `record-index` int(10) UNSIGNED PRIMARY KEY AUTO_INCREMENT"))
                {
                    log("Cannot add record index to table: " + temp_table);
                    exit(1);
                }

                QStringList select_fields;
                for (int i=0; i < fields.count(); i++)
                {
                    if (fields[i].indexOf("-desc") < 0)
                        select_fields.append(fields[i]);

                }
                QStringList insert_fields;
                for (int i=0; i < fields_for_create.count(); i++)
                {
                    if (fields_for_create[i].indexOf("-desc") < 0)
                        insert_fields.append(fields_for_create[i]);

                }
                if (!qry.exec("INSERT INTO " + temp_table + " (" + insert_fields.join(",") + ") SELECT " + select_fields.join(",") + " FROM " + tables[pos].name + ";"))
                {
                    log("Cannot insert data into: " + temp_table);
                    log("INSERT INTO " + temp_table + " (" + insert_fields.join(",") + ") SELECT " + select_fields.join(",") + " FROM " + tables[pos].name + ";");
                    exit(1);
                }

                if (!qry.exec("SELECT count(`record-index`) FROM " + temp_table))
                {
                    log("Cannot count records in " + temp_table);
                    exit(1);
                }
                qry.first();
                int tot_records = qry.value(0).toInt();

                QStringList parts;
                parts = get_parts(tot_records,num_workers*num_workers);

                //qDebug() <<"Creating files";

                for (int p = 0; p < parts.count(); p++)
                {
                    QStringList sections = parts[p].split("|");
                    TtaskItem a_separation_task;
                    a_separation_task.task_type = 1;
                    a_separation_task.table = temp_table;
                    a_separation_task.sql_file = currDir.absolutePath() + currDir.separator() + tables[pos].name + "_" + QString::number(p+1) + "_sep.sql";
                    QFile tempfile(currDir.absolutePath() + currDir.separator() + tables[pos].name + "_" + QString::number(p+1) + "_sep.sql");
                    if (!tempfile.open(QIODevice::WriteOnly | QIODevice::Text))
                    {
                        return 1;
                    }
                    QTextStream temout(&tempfile);

                    temout << "CREATE TABLE " + temp_table + "_" + QString::number(p+1) + " ENGINE=MyISAM DEFAULT CHARACTER SET utf8mb4 DEFAULT COLLATE utf8mb4_unicode_ci AS SELECT * FROM " + temp_table + " LIMIT 0;\n";;
                    drops.append("DROP TABLE " + temp_table + "_" + QString::number(p+1));
                    //Alter primary keys
                    for (int fld = 0; fld < tables[pos].fields.count(); fld++)
                    {
                        if (tables[pos].fields[fld].isKey)
                            temout << "ALTER TABLE " + temp_table + "_" + QString::number(p+1) + " MODIFY COLUMN " + tables[pos].fields[fld].name + " VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n";
                    }

                    //Alter for multiselects
                    if (multiSelectTables.count() == 0 && linked_tables.count() == 0)
                    {
                        if (descfields.count() > 0)
                        {
                            QStringList modifies;
                            for (int i = 0; i < descfields.count(); i++)
                            {
                                modifies.append("MODIFY COLUMN `" + descfields[i] + "` TEXT");
                            }
                            temout << "ALTER TABLE " + temp_table + "_" + QString::number(p+1) + " " + modifies.join(",") + ";\n";

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
                        temout << "ALTER TABLE " + temp_table + "_" + QString::number(p+1) + " " + modifies.join(",") + ";\n";
                    }
                    //Alter for lookups
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
                        temout << "ALTER TABLE " + temp_table + "_" + QString::number(p+1) + " " + modifies.join(",") + ";\n";
                    }
                    temout << "INSERT INTO " + temp_table + "_" + QString::number(p+1) + " SELECT * FROM " + temp_table + " WHERE `record-index` BETWEEN " + sections[0] + " AND " + sections[1] + ";\n";

                    tempfile.close();
                    separate_task_list.append(a_separation_task);
                }

                for (int p = 0; p < parts.count(); p++)
                {
                    TtaskItem a_update_task;
                    a_update_task.task_type = 1;
                    a_update_task.table = temp_table;
                    a_update_task.sql_file = currDir.absolutePath() + currDir.separator() + tables[pos].name + "_" + QString::number(p+1) + "_update.sql";
                    QFile tempfile(currDir.absolutePath() + currDir.separator() + tables[pos].name + "_" + QString::number(p+1) + "_update.sql");
                    if (!tempfile.open(QIODevice::WriteOnly | QIODevice::Text))
                    {
                        return 1;
                    }
                    QTextStream temout(&tempfile);

                    temout << "SET SQL_MODE = '';\n";

                    //Update the lookup tables
                    if (linked_tables.count() > 0)
                    {
                        for (int i_table=0; i_table < linked_tables.count(); i_table++)
                        {
                            sql = "UPDATE " + temp_table + "_" + QString::number(p+1) + "," + linked_tables[i_table].related_table + " AS T" + QString::number(i_table) + " SET ";
                            QString relfield = linked_tables[i_table].related_field;
                            if (this->resolve_type == 2)
                                sql = sql + temp_table + "_" + QString::number(p+1) + "." + linked_tables[i_table].field + " = T" + QString::number(i_table) + "." + relfield.replace("_cod","_des");
                            else
                                sql = sql + temp_table + "_" + QString::number(p+1) + ".`" + linked_tables[i_table].field + "-desc` = T" + QString::number(i_table) + "." + relfield.replace("_cod","_des");
                            sql = sql + " WHERE " + temp_table + "_" + QString::number(p+1) + "." + linked_tables[i_table].field + " = T" + QString::number(i_table) + "." + linked_tables[i_table].related_field + ";\n";
                            temout << sql;
                        }
                    }
                    //Update multiselects
                    if (multiSelectTables.count() > 0 && this->incmsels == false)
                    {
                        for (int i_table=0; i_table < multiSelectTables.count(); i_table++)
                        {
                            if (this->resolve_type == 1 || this->resolve_type == 3)
                            {
                                sql = "UPDATE " + temp_table + "_" + QString::number(p+1) + " AS TA SET TA." + multiSelectTables[i_table].field + " = (SELECT GROUP_CONCAT(TB." + multiSelectTables[i_table].multiSelectField + " SEPARATOR ',') FROM " + multiSelectTables[i_table].multiSelectTable + " as TB";
                                QStringList wheres;
                                QStringList groups;
                                for (int a_key = 0; a_key < multiSelectTables[i_table].multiSelectKeys.count(); a_key++)
                                {
                                    wheres.append("TA." + multiSelectTables[i_table].multiSelectKeys[a_key] + " = TB." + multiSelectTables[i_table].multiSelectKeys[a_key]);
                                    groups.append("TB." + multiSelectTables[i_table].multiSelectKeys[a_key]);
                                }
                                sql = sql + " WHERE " + wheres.join(" AND ");
                                sql = sql + " GROUP BY " + groups.join(",") + ");\n";
                                temout << sql;
                            }

                            if (this->resolve_type == 2)
                            {
                                QString desc_field = multiSelectTables[i_table].multiSelectRelField;
                                desc_field = desc_field.replace("_cod","_des");
                                sql = "UPDATE " + temp_table + "_" + QString::number(p+1) + " AS TA SET TA." + multiSelectTables[i_table].field + " = (SELECT GROUP_CONCAT(TC." + desc_field + " SEPARATOR ',') FROM " + multiSelectTables[i_table].multiSelectTable + " as TB," +  multiSelectTables[i_table].multiSelectRelTable + " as TC";
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
                                temout << sql;
                            }

                            if (this->resolve_type == 3)
                            {
                                QString desc_field = multiSelectTables[i_table].multiSelectRelField;
                                desc_field = desc_field.replace("_cod","_des");
                                sql = "UPDATE " + temp_table + "_" + QString::number(p+1) + " AS TA SET TA.`" + multiSelectTables[i_table].field + "-desc` = (SELECT GROUP_CONCAT(TC." + desc_field + " SEPARATOR ',') FROM " + multiSelectTables[i_table].multiSelectTable + " as TB," +  multiSelectTables[i_table].multiSelectRelTable + " as TC";
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
                                temout << sql;
                            }
                        }
                    }
                    tempfile.close();
                    update_task_list.append(a_update_task);
                }

                QDir finalDir(outputDirectory);
                TtaskItem a_merge_task;
                a_merge_task.task_type = 3;
                a_merge_task.table = temp_table;
                a_merge_task.final_file = finalDir.absolutePath() + currDir.separator() + tables[pos].name + ".json";


                for (int p = 0; p < parts.count(); p++)
                {
                    TtaskItem a_json_task;
                    a_json_task.task_type = 2;
                    a_json_task.table = temp_table;

                    a_json_task.sql_file = currDir.absolutePath() + currDir.separator() + tables[pos].name + "_" + QString::number(p+1) + "_query.sql";
                    a_json_task.json_file = currDir.absolutePath() + currDir.separator() + tables[pos].name + "_" + QString::number(p+1) + ".json";

                    a_merge_task.json_files.append(currDir.absolutePath() + currDir.separator() + tables[pos].name + "_" + QString::number(p+1) + ".json");

                    QStringList fields_to_select;
                    for (int fld =0; fld < fields_for_select.count(); fld++)
                    {
                        if (fields_for_select[fld].indexOf(" as ") < 0)
                            fields_to_select.append("ifnull(`" + fields_for_select[fld] + "`,'') as `" + fields_for_select[fld] + "`");
                        else
                            fields_to_select.append(fields_for_select[fld]);
                    }

                    sql = "SELECT " + fields_to_select.join(",") + " FROM " + temp_table + "_" + QString::number(p+1) + ";";
                    QFile sqlfile(currDir.absolutePath() + currDir.separator() + tables[pos].name + "_" + QString::number(p+1) + "_query.sql");
                    if (!sqlfile.open(QIODevice::WriteOnly | QIODevice::Text))
                    {
                        return 1;
                    }
                    QTextStream out(&sqlfile);
                    out << sql;
                    sqlfile.close();

                    json_task_list.append(a_json_task);

                }
                merge_task_list.append(a_merge_task);


                linked_tables.clear();
                multiSelectTables.clear();

            }
            QDir finalDir(outputDirectory);
            for (int lkp = 0; lkp < lookupTables.count(); lkp++)
            {
                TtaskItem a_merge_task;
                a_merge_task.task_type = 4;
                a_merge_task.table = lookupTables[lkp].name;
                a_merge_task.final_file = finalDir.absolutePath() + currDir.separator() + lookupTables[lkp].name + ".json";


                TtaskItem a_json_task;
                a_json_task.task_type = 2;
                a_json_task.table = lookupTables[lkp].name;

                a_json_task.sql_file = currDir.absolutePath() + currDir.separator() + lookupTables[lkp].name + "_query.sql";
                a_json_task.json_file = currDir.absolutePath() + currDir.separator() + lookupTables[lkp].name + ".json";

                a_merge_task.json_files.append(currDir.absolutePath() + currDir.separator() + lookupTables[lkp].name + ".json");

                fields.clear();
                descfields.clear();
                for (int fld = 0; fld < lookupTables[lkp].fields.count(); fld++)
                {
                    if (this->protectSensitive)
                    {
                        if (lookupTables[lkp].fields[fld].sensitive == false)
                        {
                            fields.append(lookupTables[lkp].fields[fld].name);
                        }
                        else
                        {
                            if (lookupTables[lkp].fields[fld].protection != "exclude")
                                fields.append("HEX(AES_ENCRYPT(" + lookupTables[lkp].name + "." + lookupTables[lkp].fields[fld].name + ",UNHEX('" + this->encryption_key + "'))) as " + lookupTables[lkp].fields[fld].name);
                        }
                    }
                    else
                        fields.append(lookupTables[lkp].fields[fld].name);
                }

                QStringList fields_to_select;
                for (int fld =0; fld < fields.count(); fld++)
                {
                    if (fields[fld].indexOf(" as ") < 0)
                        fields_to_select.append("ifnull(`" + fields[fld] + "`,'') as `" + fields[fld] + "`");
                    else
                        fields_to_select.append(fields[fld]);
                }

                sql = "SELECT " + fields_to_select.join(",") + " FROM " + lookupTables[lkp].name + ";\n";
                QFile sqlfile(currDir.absolutePath() + currDir.separator() + lookupTables[lkp].name + "_query.sql");
                if (!sqlfile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    return 1;
                }
                QTextStream out(&sqlfile);
                out << sql;
                sqlfile.close();

                json_task_list.append(a_json_task);
                merge_task_list.append(a_merge_task);
            }
        }
        else
        {
            for (int pos = 0; pos < lookupTables.count(); pos++)
                tables.append(lookupTables[pos]);

            QDir finalDir(outputDirectory);
            for (int pos = 0; pos < tables.count(); pos++)
            {
                TtaskItem a_merge_task;
                a_merge_task.task_type = 4;
                a_merge_task.table = tables[pos].name;
                a_merge_task.final_file = finalDir.absolutePath() + currDir.separator() + tables[pos].name + ".json";


                TtaskItem a_json_task;
                a_json_task.task_type = 2;
                a_json_task.table = tables[pos].name;

                a_json_task.sql_file = currDir.absolutePath() + currDir.separator() + tables[pos].name + "_query.sql";
                a_json_task.json_file = currDir.absolutePath() + currDir.separator() + tables[pos].name + ".json";

                a_merge_task.json_files.append(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".json");


                fields.clear();
                descfields.clear();
                for (int fld = 0; fld < tables[pos].fields.count(); fld++)
                {
                    if (this->protectSensitive)
                    {
                        if (tables[pos].fields[fld].sensitive == false)
                        {
                            fields.append(tables[pos].fields[fld].name);
                        }
                        else
                        {
                            if (tables[pos].fields[fld].protection != "exclude")
                                fields.append("HEX(AES_ENCRYPT(" + tables[pos].name + "." + tables[pos].fields[fld].name + ",UNHEX('" + this->encryption_key + "'))) as " + tables[pos].fields[fld].name);
                        }
                    }
                    else
                        fields.append(tables[pos].fields[fld].name);
                }

                QStringList fields_to_select;
                for (int fld =0; fld < fields.count(); fld++)
                {
                    if (fields[fld].indexOf(" as ") < 0)
                        fields_to_select.append("ifnull(`" + fields[fld] + "`,'') as `" + fields[fld] + "`");
                    else
                        fields_to_select.append(fields[fld]);
                }

                sql = "SELECT " + fields_to_select.join(",") + " FROM " + tables[pos].name + ";\n";
                QFile sqlfile(currDir.absolutePath() + currDir.separator() + tables[pos].name + "_query.sql");
                if (!sqlfile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    return 1;
                }
                QTextStream out(&sqlfile);
                out << sql;
                sqlfile.close();

                json_task_list.append(a_json_task);
                merge_task_list.append(a_merge_task);

            }
        }

        //exit(1);

        int result = processTasks(currDir);
        QSqlQuery drop(db);
        for (int d=0; d < drops.count(); d++)
        {
            drop.exec(drops[d]);
        }
        db.close();
        QDir tdir(tempDir);
        tdir.removeRecursively();

        if (result != 0)
        {
            returnCode = 1;
            return returnCode;
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
