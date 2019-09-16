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

namespace pt = boost::property_tree;

mainClass::mainClass(QObject *parent) : QObject(parent)
{
    returnCode = 0;
    letterIndex = 65;
}

void mainClass::log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s", temp.toUtf8().data());
}

void mainClass::setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, QString outputFile, bool includeProtected, QString tempDir, bool incLookups, bool incmsels, QString firstSheetName, QString insertXML, bool separate)
{
    this->host = host;
    this->port = port;
    this->user = user;
    this->pass = pass;
    this->schema = schema;
    this->outputFile = outputFile;
    this->includeSensitive = includeProtected;
    this->tempDir = tempDir;
    this->createXML = createXML;
    this->incLookups = incLookups;
    this->incmsels = incmsels;
    this->firstSheetName = firstSheetName;
    this->insertXML = insertXML;
    this->separateSelects = separate;
}

void mainClass::getFieldData(QString table, QString field, QString &desc, QString &valueType, int &size, int &decsize, bool &isMultiSelect, QString &multiSelectTable, QString &multiSelectField, QStringList &options, bool &isKey, QStringList &multiSelectKeys)
{
    for (int pos = 0; pos <= tables.count()-1;pos++)
    {
        if (tables[pos].name == table)
        {
            for (int pos2 = 0; pos2 <= tables[pos].fields.count()-1; pos2++)
            {
                if (tables[pos].fields[pos2].name == field)
                {
                    desc = tables[pos].fields[pos2].desc;
                    valueType = tables[pos].fields[pos2].type;
                    size = tables[pos].fields[pos2].size;
                    decsize = tables[pos].fields[pos2].decSize;
                    isMultiSelect = tables[pos].fields[pos2].isMultiSelect;
                    multiSelectTable = tables[pos].fields[pos2].multiSelectTable;
                    multiSelectField = tables[pos].fields[pos2].multiSelectField;
                    options.append(tables[pos].fields[pos2].multiSelectOptions);
                    multiSelectKeys.append(tables[pos].fields[pos2].multiSelectKeys);
                    isKey = tables[pos].fields[pos2].isKey;
                    return;
                }
            }
        }
    }
    desc = "NONE";
}

const char *mainClass::getSheetDescription(QString name)
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
        return truncated.toUtf8().constData();
    }
    else
    {
        truncated = truncated + "_" + QString::number(letterIndex);
        letterIndex++;
        tableNames.append(truncated);
        return truncated.toUtf8().constData();
    }
}

// Parses a multiselect table and get the value in each row
// This function must be thought a bit more because is not really efficient. Basically we parse with boost a whole multiselect table
// pulling those records matching a set of keys. Though boost is very fast we are passing thrrough the whole table for each record
// of a table
QStringList mainClass::getMultiSelectValues(QString multiSelectTable, QString multiSelectField, QStringList keys, QStringList multiSelectKeys)
{
    QDir currDir(tempDir);
    QStringList result;
    QString sourceFile;
    sourceFile = currDir.absolutePath() + currDir.separator() + multiSelectTable + ".xml";
    if (QFile::exists(sourceFile))
    {
        pt::ptree tree;
        pt::read_xml(sourceFile.toUtf8().constData(), tree);
        BOOST_FOREACH(boost::property_tree::ptree::value_type const&db, tree.get_child("mysqldump") )
        {
            const boost::property_tree::ptree & aDatabase = db.second; // value (or a subnode)
            BOOST_FOREACH(boost::property_tree::ptree::value_type const&ctable, aDatabase.get_child("") )
            {
                const std::string & key = ctable.first.data();
                if (key == "table_data")
                {
                    const boost::property_tree::ptree & aTable = ctable.second;
                    BOOST_FOREACH(boost::property_tree::ptree::value_type const&row, aTable.get_child("") )
                    {
                        QStringList Rowkeys;
                        QString rowLookUpValue;
                        const boost::property_tree::ptree & aRow = row.second;
                        BOOST_FOREACH(boost::property_tree::ptree::value_type const&field, aRow.get_child("") )
                        {
                            const std::string & fkey = field.first.data();
                            if (fkey == "field")
                            {
                                const boost::property_tree::ptree & aField = field.second;
                                std::string fname = aField.get<std::string>("<xmlattr>.name");
                                std::string fvalue = aField.data();
                                QString fieldName = QString::fromStdString(fname);
                                QString fieldValue = QString::fromStdString(fvalue);
                                if (multiSelectKeys.indexOf(fieldName) >= 0)
                                {
                                    Rowkeys.append(fieldName + "~@~" + fieldValue);
                                }
                                if (multiSelectField == fieldName)
                                {
                                    rowLookUpValue = fieldValue;
                                }
                            }
                        }
                        bool notFound = false;
                        for (int pos = 0; pos < Rowkeys.count(); pos++)
                        {
                            if (keys.indexOf(Rowkeys[pos]) < 0)
                                notFound = true;
                        }
                        if (notFound == false)
                            result.append(rowLookUpValue);
                    }
                }
            }
        }
    }
    return result;
}

int mainClass::parseDataToXLSX()
{
    QDir currDir(tempDir);

    lxw_workbook  *workbook  = workbook_new(outputFile.toUtf8().constData());

    //Parse all tables to the Excel file
    for (int pos = 0; pos <= tables.count()-1; pos++)
    {
        if (tables[pos].islookup == false)
        {
            if (tables[pos].name.indexOf("_msel_") < 0)
            {
                QString sourceFile;
                sourceFile = currDir.absolutePath() + currDir.separator() + tables[pos].name + ".xml";

                pt::ptree tree;
                pt::read_xml(sourceFile.toUtf8().constData(), tree);
                BOOST_FOREACH(boost::property_tree::ptree::value_type const&db, tree.get_child("mysqldump") )
                {
                    const boost::property_tree::ptree & aDatabase = db.second; // value (or a subnode)
                    BOOST_FOREACH(boost::property_tree::ptree::value_type const&ctable, aDatabase.get_child("") )
                    {
                        const std::string & key = ctable.first.data();
                        if (key == "table_data")
                        {
                            const boost::property_tree::ptree & aTable = ctable.second;

                            //Here we need to create the sheet
                            QString tableDesc;
                            tableDesc = tables[pos].desc;
                            if (tableDesc == "")
                                tableDesc = tables[pos].name;
                            lxw_worksheet *worksheet = workbook_add_worksheet(workbook,getSheetDescription(tables[pos].desc));
                            int rowNo = 1;
                            bool inserted = false;
                            BOOST_FOREACH(boost::property_tree::ptree::value_type const&row, aTable.get_child("") )
                            {
                                const boost::property_tree::ptree & aRow = row.second;

                                //Here we need to append a row
                                int colNo = 0;
                                QStringList keys;
                                BOOST_FOREACH(boost::property_tree::ptree::value_type const&field, aRow.get_child("") )
                                {
                                    const std::string & fkey = field.first.data();
                                    if (fkey == "field")
                                    {
                                        const boost::property_tree::ptree & aField = field.second;
                                        std::string fname = aField.get<std::string>("<xmlattr>.name");
                                        std::string fvalue = aField.data();
                                        QString desc;
                                        QString valueType;
                                        int size;
                                        int decSize;
                                        QString fieldName = QString::fromStdString(fname);
                                        QString fieldValue = QString::fromStdString(fvalue);
                                        bool isMultiSelect;
                                        QString multiSelectTable;
                                        QString multiSelectField;
                                        QStringList options;
                                        bool isKey;
                                        QStringList multiSelectKeys;
                                        getFieldData(tables[pos].name,fieldName,desc,valueType,size,decSize,isMultiSelect,multiSelectTable,multiSelectField,options,isKey,multiSelectKeys);
                                        if (desc != "NONE")
                                        {
                                            bool increase_colno = true;
                                            if (isKey)
                                            {
                                                keys.append(fieldName + "~@~" + fieldValue);
                                            }
                                            inserted = true;
                                            if (rowNo == 1)
                                            {
                                                if ((isMultiSelect) && (separateSelects) && (multiSelectTable != ""))
                                                {
                                                    int mselColno = colNo;
                                                    for (int opt = 0; opt < options.count(); opt++)
                                                    {
                                                        QString fieldWithOption = fieldName + "/" + options[opt];
                                                        worksheet_write_string(worksheet,0, mselColno, fieldWithOption.toUtf8().constData(), NULL);
                                                        mselColno++;
                                                    }
                                                }
                                                else
                                                    worksheet_write_string(worksheet,0, colNo, fieldName.toUtf8().constData(), NULL);
                                            }
                                            if ((isMultiSelect) && (separateSelects) && (multiSelectTable != ""))
                                            {
                                                QList <ToptionDef> lst_options;
                                                for (int opt = 0; opt < options.count(); opt++)
                                                {
                                                    ToptionDef an_option;
                                                    an_option.code = options[opt];
                                                    an_option.value = "0";
                                                    lst_options.append(an_option);
                                                }
                                                QStringList mselValues = getMultiSelectValues(multiSelectTable,multiSelectField,keys,multiSelectKeys);
                                                for (int mval=0; mval < mselValues.count(); mval++)
                                                {
                                                    for (int opt = 0; opt < lst_options.count(); opt++)
                                                    {
                                                        if (lst_options[opt].code == mselValues[mval])
                                                        {
                                                            lst_options[opt].value = "1";
                                                        }
                                                    }
                                                }
                                                for (int opt = 0; opt < lst_options.count(); opt++)
                                                {
                                                    worksheet_write_string(worksheet,rowNo, colNo, lst_options[opt].value.toUtf8().constData(), NULL);
                                                    colNo++;
                                                    increase_colno = false;
                                                }
                                            }
                                            else
                                            {
                                               if (!isMultiSelect)
                                               {
                                                   worksheet_write_string(worksheet,rowNo, colNo, fieldValue.toUtf8().constData(), NULL);
                                               }
                                               else
                                               {
//                                                   if (tables[pos].name == "maintable")
//                                                   {
//                                                       for (int tmp = 0; tmp < keys.count(); tmp++)
//                                                           log(keys[pos]);
//                                                   }
                                                   if (multiSelectTable != "")
                                                   {
                                                        QStringList mselValues = getMultiSelectValues(multiSelectTable,multiSelectField,keys,multiSelectKeys);
                                                        fieldValue = mselValues.join(" ");
                                                        worksheet_write_string(worksheet,rowNo, colNo, fieldValue.toUtf8().constData(), NULL);
                                                   }
                                                   else
                                                       worksheet_write_string(worksheet,rowNo, colNo, fieldValue.toUtf8().constData(), NULL);
                                               }
                                            }
                                            if (increase_colno)
                                                colNo++;
                                        }
                                    }
                                }
                                if (inserted)
                                    rowNo++;
                            }
                        }
                    }
                }
            }
        }
    }
    //Parse all lookup tables to the Excel file
    for (int pos = 0; pos <= tables.count()-1; pos++)
    {
        if (tables[pos].islookup == true)
        {
            QString sourceFile;
            sourceFile = currDir.absolutePath() + currDir.separator() + tables[pos].name + ".xml";

            pt::ptree tree;
            pt::read_xml(sourceFile.toUtf8().constData(), tree);
            BOOST_FOREACH(boost::property_tree::ptree::value_type const&db, tree.get_child("mysqldump") )
            {
                const boost::property_tree::ptree & aDatabase = db.second; // value (or a subnode)
                BOOST_FOREACH(boost::property_tree::ptree::value_type const&ctable, aDatabase.get_child("") )
                {
                    const std::string & key = ctable.first.data();
                    if (key == "table_data")
                    {
                        const boost::property_tree::ptree & aTable = ctable.second;

                        //Here we need to create the sheet
                        QString tableDesc;
                        tableDesc = tables[pos].desc;
                        if (tableDesc == "")
                            tableDesc = tables[pos].name;
                        lxw_worksheet *worksheet = workbook_add_worksheet(workbook,getSheetDescription(tables[pos].name));
                        int rowNo = 1;
                        bool inserted = false;
                        BOOST_FOREACH(boost::property_tree::ptree::value_type const&row, aTable.get_child("") )
                        {
                            const boost::property_tree::ptree & aRow = row.second;

                            //Here we need to append a row
                            int colNo = 0;
                            BOOST_FOREACH(boost::property_tree::ptree::value_type const&field, aRow.get_child("") )
                            {
                                const std::string & fkey = field.first.data();
                                if (fkey == "field")
                                {
                                    const boost::property_tree::ptree & aField = field.second;
                                    std::string fname = aField.get<std::string>("<xmlattr>.name");
                                    std::string fvalue = aField.data();
                                    QString desc;
                                    QString valueType;
                                    int size;
                                    int decSize;
                                    QString fieldName = QString::fromStdString(fname);
                                    QString fieldValue = QString::fromStdString(fvalue);
                                    bool isMultiSelect;
                                    QString multiSelectTable;
                                    QString multiSelectField;
                                    QStringList options;
                                    bool isKey;
                                    QStringList multiSelectKeys;
                                    getFieldData(tables[pos].name,fieldName,desc,valueType,size,decSize,isMultiSelect,multiSelectTable,multiSelectField,options,isKey, multiSelectKeys);
                                    if (desc != "NONE")
                                    {
                                        inserted = true;
                                        if (rowNo == 1)
                                            worksheet_write_string(worksheet,0, colNo, fieldName.toUtf8().constData(), NULL);
                                        worksheet_write_string(worksheet,rowNo, colNo, fieldValue.toUtf8().constData(), NULL);
                                        colNo++;
                                    }
                                }
                            }
                            if (inserted)
                                rowNo++;
                        }
                    }
                }
            }
        }
    }

    workbook_close(workbook);
    return 0;
}

void mainClass::getMultiSelectInfo(QDomNode table, QString table_name, QDomNode root_insert, QString &multiSelect_field, QStringList &options, QStringList &keys)
{
    QString lookupTableName;
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
                        lookupTableName = field.toElement().attribute("rtable");
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
    child = root_insert.firstChild();
    while (!child.isNull())
    {
        if (child.toElement().attribute("name") == lookupTableName)
        {
            QDomNode value = child.firstChild();
            while (!value.isNull())
            {
                options.append(value.toElement().attribute("code"));
                value = value.nextSibling();
            }
        }
        child = child.nextSibling();
    }
}

void mainClass::loadTable(QDomNode table, QDomNode insertRoot)
{
    QDomElement eTable;
    eTable = table.toElement();
    if ((eTable.attribute("sensitive","false") == "false") || (includeSensitive))
    {
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
                if ((eField.attribute("sensitive","false") == "false") || (includeSensitive))
                {
                    TfieldDef aField;
                    aField.name = eField.attribute("name","");
                    aField.desc = eField.attribute("name","");
                    aField.type = eField.attribute("type","");
                    aField.size = eField.attribute("size","").toInt();
                    aField.decSize = eField.attribute("decsize","").toInt();
                    if (eField.attribute("key","false") == "true")
                        aField.isKey = true;
                    // NOTE ON Rank. Rank is basically a multiselect with order and handled as a multiselect by ODK Tools. However
                    // we cannot pull the data from the database because the records may not be stored in the same order the user placed them in Collect
                    if ((eField.attribute("isMultiSelect","false") == "true") && (eField.attribute("odktype","") != "rank"))
                    {
                        aField.isMultiSelect = true;
                        aField.multiSelectTable = eField.attribute("multiSelectTable");
                        QString multiSelect_field;
                        QStringList options;
                        QStringList keys;
                        getMultiSelectInfo(table, aField.multiSelectTable, insertRoot, multiSelect_field, options, keys);
                        aField.multiSelectField = multiSelect_field;
                        aField.multiSelectOptions.append(options);
                        aField.multiSelectKeys.append(keys);
                    }
                    aTable.fields.append(aField);
                }
            }
            else
            {
                loadTable(field,insertRoot);
            }
            field = field.nextSibling();
        }
        mainTables.append(aTable);
    }
}

int mainClass::generateXLSX()
{
    QDomDocument insertDoc("input");
    QFile insertFile(insertXML);
    if (!insertFile.open(QIODevice::ReadOnly))
    {
        log("Cannot open input insert XML file");
        returnCode = 1;
        return returnCode;
    }
    if (!insertDoc.setContent(&insertFile))
    {
        log("Cannot parse input insert XML file");
        insertFile.close();
        returnCode = 1;
        return returnCode;
    }
    insertFile.close();
    QDomElement insertRoot = insertDoc.documentElement();

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
        if (this->incLookups)
        {
            QDomNode lkpTable = rootA.firstChild().firstChild();
            //Getting the fields to export from Lookup tables
            while (!lkpTable.isNull())
            {
                QDomElement eTable;
                eTable = lkpTable.toElement();
                if ((eTable.attribute("sensitive","false") == "false") || (includeSensitive))
                {
                    TtableDef aTable;
                    aTable.islookup = true;
                    aTable.name = eTable.attribute("name","");
                    aTable.desc = eTable.attribute("desc","");

                    QDomNode field = lkpTable.firstChild();
                    while (!field.isNull())
                    {
                        QDomElement eField;
                        eField = field.toElement();
                        if ((eField.attribute("sensitive","false") == "false") || (includeSensitive))
                        {
                            TfieldDef aField;
                            aField.name = eField.attribute("name","");
                            aField.desc = eField.attribute("desc","");
                            aField.type = eField.attribute("type","");
                            aField.size = eField.attribute("size","").toInt();
                            aField.decSize = eField.attribute("decsize","").toInt();
                            aTable.fields.append(aField);
                        }
                        field = field.nextSibling();
                    }
                    tables.append(aTable);
                }
                lkpTable = lkpTable.nextSibling();
            }
        }

        //Getting the fields to export from tables
        QDomNode table = rootA.firstChild().nextSibling().firstChild();

        //Load the data tables recursively
        loadTable(table,insertRoot);
        for (int nt =mainTables.count()-1; nt >= 0;nt--)
            tables.append(mainTables[nt]);
        if (firstSheetName != "")
            tables[0].desc = firstSheetName;
        //Export the tables as XML to the temp directory
        //Call MySQLDump to export each table as XML
        //We use MySQLDump because it very very fast
        QDir currDir(tempDir);
        QString program = "mysqldump";
        QStringList arguments;
        QProcess *mySQLDumpProcess = new QProcess();        
        QTime procTime;
        procTime.start();
        for (int pos = 0; pos <= tables.count()-1; pos++)
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
            arguments << tables[pos].name;
            mySQLDumpProcess->setStandardOutputFile(currDir.absolutePath() + currDir.separator() + tables[pos].name + ".xml");
            mySQLDumpProcess->start(program, arguments);
            mySQLDumpProcess->waitForFinished(-1);
            if ((mySQLDumpProcess->exitCode() > 0) || (mySQLDumpProcess->error() == QProcess::FailedToStart))
            {
                if (mySQLDumpProcess->error() == QProcess::FailedToStart)
                {
                    log("Error: Command " +  program + " not found");
                }
                else
                {
                    log("Running mysqldump returned error");
                    QString serror = mySQLDumpProcess->readAllStandardError();
                    log(serror);
                    log("Running paremeters:" + arguments.join(" "));
                }
                return 1;
            }
        }
        delete mySQLDumpProcess;
        returnCode = parseDataToXLSX();

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
