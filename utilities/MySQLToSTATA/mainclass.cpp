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
#include <QDebug>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>

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

void mainClass::setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, QString outputDir, bool protectSensitive, QString tempDir, bool incLookups, bool incmsels, QString firstSheetName, QString encryption_key, QString resolve_type)
{
    this->host = host;
    this->port = port;
    this->user = user;
    this->pass = pass;
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
        QStringList arguments;
        QProcess *mySQLDumpProcess = new QProcess();        
        QTime procTime;
        procTime.start();
        QString sql;
        QStringList fields;
        QString uri = user + ":" + pass + "@" + host + "/" + schema;
        QStringList sheets;
        QStringList csvs;

        QVector <TlinkedTable> linked_tables;
        QVector <TmultiSelectTable> multiSelectTables;

        for (int pos = 0; pos <= tables.count()-1; pos++)
        {                                                
            qDebug() << "Creating temp table " + tables[pos].name;
            linked_tables.clear();
            sheets.append(getSheetDescription(tables[pos].desc));
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
            sql = "SET SQL_MODE = '';\nSTART TRANSACTION;\n";

            QUuid recordUUID=QUuid::createUuid();
            temp_table = "TMP_" + recordUUID.toString().replace("{","").replace("}","").replace("-","_");
            sql = sql + "CREATE TABLE " + temp_table + " ENGINE=MyISAM DEFAULT CHARACTER SET utf8mb4 DEFAULT COLLATE utf8mb4_unicode_ci AS SELECT " + fields.join(",") + " FROM " + tables[pos].name + ";\nCOMMIT;\n";

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
            sqls.append("START TRANSACTION;\n");

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
            sqls.append("COMMIT;\n");
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

            // Export the structure of the temporary table JSON format
            arguments.clear();
            arguments << "--sql";
            arguments << "--result-format=json/array";
            arguments << "--uri=" + uri;
            arguments << "--execute=desc " + temp_table;
            mySQLDumpProcess->setStandardInputFile(QProcess::nullDevice());
            mySQLDumpProcess->setStandardOutputFile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".json");

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

            QFile loadFile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".json");

            if (!loadFile.open(QIODevice::ReadOnly)) {
                log("Couldn't open json structure file.");
                delete mySQLDumpProcess;
                return 1;
            }
            QByteArray jsonData = loadFile.readAll();
            QJsonDocument loadDoc(QJsonDocument::fromJson(jsonData));
            QJsonArray fields = loadDoc.array();
            QStringList fields_to_select;
            for (auto a_field : fields)
            {
                QJsonObject element = a_field.toObject();
                fields_to_select << "ifnull(`" + element["Field"].toString() + "`,'') as `" + element["Field"].toString() + "`";
            }


            linked_tables.clear();
            multiSelectTables.clear();
            qDebug() << "Parsing table " + tables[pos].name + " to TAB format";
            sql = "SELECT " + fields_to_select.join(",") + " FROM " + temp_table + ";";

            arguments.clear();
            arguments << "--sql";
            arguments << "--result-format=tabbed";
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
            mySQLDumpProcess->setStandardOutputFile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".raw");
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


            QString do_file = currDir.absolutePath() + currDir.separator() + tables[pos].name + ".do";

            QFile dofile(do_file);
            if (!dofile.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                log("Cannot open DCF file for writing");
                delete mySQLDumpProcess;
                return 1;
            }
            QTextStream do_out(&dofile);
            do_out << "/* This script will generate a STATA " + tables[pos].name + ".dta file from a " + tables[pos].name + ".raw tab delimited file*/\n";
            do_out << "/* You only need to run this once.*/\n";
            do_out << "/* Change c:\\my_working_dir to the directory holding this file.*/\n";
            do_out << "cd \"c:\\my_working_dir\"\n";
            do_out << "import delimited \"" + tables[pos].name + ".raw\", delimiter(tab) varnames(1) encoding(UTF-8) stringcols(_all)\n";



            for (auto a_field : fields)
            {
                QString line_data;
                QJsonObject element = a_field.toObject();
                if (element["Type"].toString() == "double")
                    line_data = "destring " + element["Field"].toString() + ", replace float force\n";
                if (element["Type"].toString().indexOf("decimal") >= 0)
                    line_data = "destring " + element["Field"].toString() + ", replace float force\n";
                if (element["Type"].toString().indexOf("int") >= 0)
                    line_data =  "destring " + element["Field"].toString() + ", replace force\n";
                if (element["Type"].toString().indexOf("varchar") >= 0)
                {
                    QRegExp exp("\\(([^()]*)\\)");
                    exp.indexIn(element["Type"].toString());
                    QString zize = exp.cap(1);
                    line_data =  "recast str"+ zize + " " + element["Field"].toString() + ", force\n";
                }
                if (line_data != "")
                    do_out << line_data;

            }
            do_out << "save " + tables[pos].name + "\n";
            do_out << "clear";
            dofile.close();
            loadFile.close();

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

            QString tab_file = currDir.absolutePath() + currDir.separator() + tables[pos].name + ".raw";

            csvs.append(tab_file);
            csvs.append(do_file);
        }


        for (int pos = 0; pos < csvs.count(); pos++)
        {
            QString newpath;
            newpath = csvs[pos];
            newpath.replace(currDir.absolutePath() + currDir.separator(), outputDirectory + currDir.separator());
            if (QFile::copy(csvs[pos], newpath))
                QFile::remove(csvs[pos]);
        }

        delete mySQLDumpProcess;

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
