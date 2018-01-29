#include "mainclass.h"
#include <QDir>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QProcess>
#include <QTime>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>

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

void mainClass::setParameters(QString host, QString port, QString user, QString pass, QString schema, QString createXML, QString outputFile, bool includeProtected, QString tempDir)
{
    this->host = host;
    this->port = port;
    this->user = user;
    this->pass = pass;
    this->schema = schema;
    this->outputFile = outputFile;
    this->includeProtected = includeProtected;
    this->tempDir = tempDir;
    this->createXML = createXML;
}

int mainClass::parseDataToXLSX()
{
    QDir currDir(tempDir);
    for (int pos = 0; pos <= tables.count()-1; pos++)
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

                    BOOST_FOREACH(boost::property_tree::ptree::value_type const&row, aTable.get_child("") )
                    {
                        const boost::property_tree::ptree & aRow = row.second;

                        //Here we need to append a row

                        BOOST_FOREACH(boost::property_tree::ptree::value_type const&field, aRow.get_child("") )
                        {
                            const std::string & fkey = field.first.data();
                            if (fkey == "field")
                            {
                                const boost::property_tree::ptree & aField = field.second;
                                std::string fname = aField.get<std::string>("<xmlattr>.name");
                                std::string fvalue = aField.data();

                                //Here we need to append the column

                            }
                            //QJsonDocument doc(JSONRow);
                            //QString JSONString(doc.toJson(QJsonDocument::Compact));
                        }
                    }
                }
            }
        }
    }
    return 0;
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

    QDomElement rootA = docA.documentElement();
    if (rootA.tagName() == "XMLSchemaStructure")
    {
        QDomNode lkpTable = docA.documentElement().firstChild();

        //Getting the fields to export from Lookup tables
        while (!lkpTable.nextSibling().isNull())
        {
            QDomElement eTable;
            eTable = lkpTable.toElement();
            if (eTable.attribute("sensitive","false") == "false")
            {
                TtableDef aTable;
                aTable.islookup = true;
                aTable.name = eTable.attribute("name","");
                aTable.desc = eTable.attribute("desc","");

                QDomNode field = lkpTable.firstChild();
                while (!field.nextSibling().isNull())
                {
                    QDomElement eField;
                    eField = field.toElement();
                    if (eField.attribute("sensitive","false") == "false")
                    {
                        if (eField.attribute("name","") != "rowuuid")
                        {
                            TfieldDef aField;
                            aField.name = eField.attribute("name","");
                            aField.desc = eField.attribute("desc","");
                            aField.type = eField.attribute("type","");
                            aField.size = eField.attribute("size","").toInt();
                            aField.decSize = eField.attribute("decsize","").toInt();
                            aTable.fields.append(aField);
                        }
                    }
                    field = field.nextSibling();
                }
                tables.append(aTable);
            }
            lkpTable = lkpTable.nextSibling();
        }

        //Getting the fields to export from tables
        QDomNode table = docA.documentElement().firstChild().nextSibling();
        while (!table.nextSibling().isNull())
        {
            QDomElement eTable;
            eTable = table.toElement();
            if (eTable.attribute("sensitive","false") == "false")
            {
                TtableDef aTable;
                aTable.islookup = true;
                aTable.name = eTable.attribute("name","");
                aTable.desc = eTable.attribute("desc","");

                QDomNode field = lkpTable.firstChild();
                while (!field.nextSibling().isNull())
                {
                    QDomElement eField;
                    eField = field.toElement();
                    if (eField.attribute("sensitive","false") == "false")
                    {
                        if (eField.attribute("name","") != "rowuuid")
                        {
                            TfieldDef aField;
                            aField.name = eField.attribute("name","");
                            aField.desc = eField.attribute("desc","");
                            aField.type = eField.attribute("type","");
                            aField.size = eField.attribute("size","").toInt();
                            aField.decSize = eField.attribute("decsize","").toInt();
                            aTable.fields.append(aField);
                        }
                    }
                    field = field.nextSibling();
                }
                tables.append(aTable);
            }
            table = table.nextSibling();
        }
        //Export the tables as XML to the temp directory
        //Call MySQLDump to export each table as XML
        //We use MySQLDump because it very very fast
        QDir currDir(tempDir);
        QString program = "mysqldump";
        QStringList arguments;
        QProcess *mySQLDumpProcess = new QProcess();
        log("Exporting data to XML");
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
