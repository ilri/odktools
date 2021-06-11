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

void mainClass::setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, QString outputFile, bool protectSensitive, QString tempDir, bool incLookups, bool incmsels, QString firstSheetName, QString encryption_key)
{
    this->host = host;
    this->port = port;
    this->user = user;
    this->pass = pass;
    this->schema = schema;
    this->outputFile = outputFile;
    this->protectSensitive = protectSensitive;
    this->tempDir = tempDir;
    this->createXML = createXML;
    this->incLookups = incLookups;
    this->incmsels = incmsels;
    this->firstSheetName = firstSheetName;    
    this->encryption_key = encryption_key;
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
        QString leftjoin;
        QStringList leftjoins;                
        int lkpTblIndex;
        for (int pos = 0; pos <= tables.count()-1; pos++)
        {                                                
            lkpTblIndex = 0;
            leftjoins.clear();
            sheets.append(getSheetDescription(tables[pos].desc));
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
                            fields.append("GROUP_CONCAT(DISTINCT T" + QString::number(lkpTblIndex) + "." + lkpdesc + ") AS " + tables[pos].fields[fld].name);
                            leftjoin = "LEFT JOIN " + tables[pos].fields[fld].multiSelectTable + " ON " + tables[pos].name + "." + tables[pos].fields[fld].multiSelectKeys[0] + " = " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectKeys[0];
                            for (int key = 1; key < tables[pos].fields[fld].multiSelectKeys.count(); key++)
                            {
                                leftjoin = leftjoin + " AND " + tables[pos].name + "." + tables[pos].fields[fld].multiSelectKeys[key] + " = " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectKeys[key];
                            }
                            leftjoins.append(leftjoin);
                            leftjoin = "LEFT JOIN " + tables[pos].fields[fld].multiSelectRelTable + " AS T" + QString::number(lkpTblIndex) + " ON " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectField + " = T" + QString::number(lkpTblIndex) + "." + tables[pos].fields[fld].multiSelectRelField;
                            leftjoins.append(leftjoin);
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
                        fields.append("GROUP_CONCAT(DISTINCT T" + QString::number(lkpTblIndex) + "." + lkpdesc + ") AS " + tables[pos].fields[fld].name);
                        leftjoin = "LEFT JOIN " + tables[pos].fields[fld].multiSelectTable + " ON " + tables[pos].name + "." + tables[pos].fields[fld].multiSelectKeys[0] + " = " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectKeys[0];
                        for (int key = 1; key < tables[pos].fields[fld].multiSelectKeys.count(); key++)
                        {
                            leftjoin = leftjoin + " AND " + tables[pos].name + "." + tables[pos].fields[fld].multiSelectKeys[key] + " = " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectKeys[key];
                        }
                        leftjoins.append(leftjoin);
                        leftjoin = "LEFT JOIN " + tables[pos].fields[fld].multiSelectRelTable + " AS T" + QString::number(lkpTblIndex) + " ON " + tables[pos].fields[fld].multiSelectTable + "." + tables[pos].fields[fld].multiSelectField + " = T" + QString::number(lkpTblIndex) + "." + tables[pos].fields[fld].multiSelectRelField;
                        leftjoins.append(leftjoin);
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
                sql = sql + " GROUP BY " + grpKeys.join(",");
            }
            sql = sql + ";\n";

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
            // Convert the tab delimited file to CSV
            arguments.clear();
            arguments << "input";
            arguments << "-d";
            arguments << "\\t";
            arguments << currDir.absolutePath() + currDir.separator() + tables[pos].name + ".txt";
            mySQLDumpProcess->setStandardInputFile(QProcess::nullDevice());
            mySQLDumpProcess->setStandardOutputFile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".csv");
            mySQLDumpProcess->start("xsv", arguments);
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
            csvs.append(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".csv");
        }

        // Finally create the Excel file using the CSVs
        arguments.clear();
        for (int pos = 0; pos < sheets.count(); pos++)
        {
            arguments.append("-s");
            arguments.append(sheets[pos]);
        }
        arguments.append("--output");
        arguments.append(outputFile);
        for (int pos = 0; pos < csvs.count(); pos++)
        {
            arguments.append(csvs[pos]);
        }
        mySQLDumpProcess->setStandardInputFile(QProcess::nullDevice());
        mySQLDumpProcess->setStandardOutputFile(QProcess::nullDevice());        
        mySQLDumpProcess->start("csv2xlsx", arguments);

        mySQLDumpProcess->waitForFinished(-1);
        if ((mySQLDumpProcess->exitCode() > 0) || (mySQLDumpProcess->error() == QProcess::FailedToStart))
        {
            if (mySQLDumpProcess->error() == QProcess::FailedToStart)
            {
                log("Error: Command csv2xlsx not found");
            }
            else
            {
                log("Running csv2xlsx returned error");
                QString serror = mySQLDumpProcess->readAllStandardError();
                log(serror);
                log("Running paremeters:" + arguments.join(" "));
            }
            delete mySQLDumpProcess;
            return 1;
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
