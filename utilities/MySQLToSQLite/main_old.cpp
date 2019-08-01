#include <tclap/CmdLine.h>
#include <QSqlDatabase>
#include <stdlib.h>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QVariant>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSqlField>
#include <QStringList>
#include <QProcess>
#include <QByteArray>
#include <QThread>
#include <QDir>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <QDomDocument>
#include <QDomDocument>
#include <QDomElement>

namespace pt = boost::property_tree;

struct tblDef
{
    QString create;
    QStringList inserts;
};
typedef tblDef TtblDef;

void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toLocal8Bit().data());
}

int parseXML(QSqlDatabase db, QString xml_create_file, QString xml_data_file)
{
    QFile fileA(xml_create_file);
    if (!fileA.open(QIODevice::ReadOnly))
    {
        log("Cannot open input file");
        return 1;
    }
    if (!docA.setContent(&fileA))
    {
        log("Cannot parse input file");
        fileA.close();
        return 1;
    }
    fileA.close();

    QDomElement rootA = docA.documentElement();

    if (rootA.tagName() == "XMLSchemaStructure")
    {
        QDomNode lkptable = rootA.firstChild().firstChild();
        while (!lkptable.isNull())
        {
            QString tableName = lkptable.toElement().attribute("name");
            QDomNode a_filed = rootA.firstChild();
            while (!a_filed.isNull())
            {
                QString field_name = a_filed.toElement().attribute("name");
                QString field_key = a_filed.toElement().attribute("key","false");
                QString field_type = a_filed.toElement().attribute("type","");

                a_filed = a_filed.nextSibling();
            }

            lkptable = lkptable.nextSibling();
        }
    }


    pt::ptree tree;
    pt::read_xml(xml_data_file.toUtf8().constData(), tree);
    BOOST_FOREACH(boost::property_tree::ptree::value_type const&db, tree.get_child("mysqldump") )
    {
        const boost::property_tree::ptree & aDatabase = db.second; // value (or a subnode)
        BOOST_FOREACH(boost::property_tree::ptree::value_type const&ctable, aDatabase.get_child("") )
        {
            const std::string & key = ctable.first.data();
            if (key == "table_data")
            {
                const boost::property_tree::ptree & aTable = ctable.second;
                std::string fname = aTable.get<std::string>("<xmlattr>.name");
                BOOST_FOREACH(boost::property_tree::ptree::value_type const&row, aTable.get_child("") )
                {
                    const boost::property_tree::ptree & aRow = row.second;
                    BOOST_FOREACH(boost::property_tree::ptree::value_type const&field, aRow.get_child("") )
                    {
                        const std::string & fkey = field.first.data();
                        if (fkey == "field")
                        {
                            const boost::property_tree::ptree & aField = field.second;
                            std::string fname = aField.get<std::string>("<xmlattr>.name");
                            std::string fvalue = aField.data();
                            doc.append(kvp(fname,fvalue));
                        }
                        //QJsonDocument doc(JSONRow);
                        //QString JSONString(doc.toJson(QJsonDocument::Compact));
                    }
                }
            }
        }
    }
}

int genSnapHost(QSqlDatabase db, QSqlDatabase dblite, QString auditFile, QString host, QString user, QString pass, QString schema, QString tempDir, QString createXML)
{
    QSqlQuery tables(db);
    QString sql;
    QStringList ignoreTables;
    int pos;


    ignoreTables << "audit_log";    
    ignoreTables << "dict_iso639";
    ignoreTables << "alembic_version";

    if (!QFile::exists(createXML))
    {
        log("The XML create file does not exists");
        return 1;
    }

    //Retrives any views in the schema and add them to the list of ignored tables
    sql = "show full tables where Table_Type = 'VIEW'";
    if (tables.exec(sql))
    {
        while (tables.next())
        {
            ignoreTables << tables.value(0).toString();
        }
    }
    else
    {
        log("Error reading the tables. MySQL Error:" + tables.lastError().databaseText());
        return 1;
    }

    QString program = "mysqldump";
    QStringList arguments;
    arguments << "--single-transaction";
    arguments << "-h" << host;
    arguments << "-u" << user;
    arguments << "--password=" + pass;
    arguments << "--skip-triggers";
    arguments << "--xml";
    arguments << "--no-create-info";
    for (pos = 0; pos <= ignoreTables.count()-1; pos++)
        arguments << "--ignore-table=" + schema + "." + ignoreTables[pos];
    arguments << schema;

    QDir temporary_dir(tempDir);
    if (!temporary_dir.exists())
        temporary_dir.mkdir(tempDir);

    QProcess *myProcess = new QProcess();
    log("Running MySQLDump please wait....");
    QString temp_xml_file = temporary_dir.absolutePath() + QDir::separator()  + "tempDump.xml";
    if (QFile::exists(temp_xml_file))
        if (!QFile::remove(temp_xml_file))
        {
            log("Error removing temporary dump file");
            return 1;
        }

    myProcess->setStandardOutputFile(temp_xml_file);
    myProcess->start(program, arguments);
    myProcess->waitForFinished(-1);
    if ((myProcess->exitCode() > 0) || (myProcess->error() == QProcess::FailedToStart))
    {
        if (myProcess->error() == QProcess::FailedToStart)
        {
            log("Error: Command " +  program + " not found");
        }
        else
        {
            log("Running MySQLDump returned error");
            QString serror = myProcess->readAllStandardError();
            log(serror);
            log("Running paremeters:" + arguments.join(" "));
        }
        return 1;
    }
    log("Dump XML file created. Generating SQLite database");
    parseXML(dblite, createXML, temp_xml_file);

    log("Appending audit to snapshot");

    program = "sqlite3";
    arguments.clear();
    arguments << "-bail";
    arguments << dblite.databaseName();

    myProcess->setStandardInputFile(auditFile);
    myProcess->start(program, arguments);
    myProcess->waitForFinished(-1);
    if ((myProcess->exitCode() > 0) || (myProcess->error() == QProcess::FailedToStart))
    {
        if (myProcess->error() == QProcess::FailedToStart)
        {
            log("Error: Command " +  program + " not found");
        }
        else
        {
            log("Running Sqlite3 returned error");
            QString serror = myProcess->readAllStandardError();
            log(serror);
            log("Running paremeters:" + arguments.join(" "));
        }
        return 1;
    }

    log("Snapshot created successfully");

    return 0;
}

int main(int argc, char *argv[])
{
    QString title;
    title = title + "****************************************************************** \n";
    title = title + " * MySQLToSQLite 2.0                                              * \n";
    title = title + " * This tool generates a SQLite file from a MySQL schema.         * \n";
    title = title + " * The tool relies on MySQLDump, sqlite3 to convert a MySQL       * \n";
    title = title + " * XML dump file into a sqlite database.                          * \n";
    title = title + " * (c) QLands, 2019                                               * \n";
    title = title + " ****************************************************************** \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "2.0");
    //Required arguments
    TCLAP::ValueArg<std::string> hostArg("H","host","MySQL Host. Default localhost",false,"localhost","string");
    TCLAP::ValueArg<std::string> portArg("P","port","MySQL Port. Default 3306",false,"3306","string");
    TCLAP::ValueArg<std::string> userArg("u","user","MySQL user",true,"","string");
    TCLAP::ValueArg<std::string> passArg("p","password","MySQL password",true,"","string");
    TCLAP::ValueArg<std::string> schemaArg("s","schema","MySQL schema",true,"","string");
    TCLAP::ValueArg<std::string> auditArg("a","audit","Input audit file",true,"","string");
    TCLAP::ValueArg<std::string> tempArg("t","temp","Temporary directory",false,".","string");
    TCLAP::ValueArg<std::string> createArg("c","create","Create XML file",true,"","string");
    TCLAP::ValueArg<std::string> outputArg("o","output","Ooutput snapshot file",true,"","string");


    cmd.add(hostArg);
    cmd.add(portArg);
    cmd.add(userArg);
    cmd.add(passArg);
    cmd.add(schemaArg);
    cmd.add(auditArg);
    cmd.add(outputArg);
    cmd.add(tempArg);
    cmd.add(createArg);
    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command


    QString host = QString::fromUtf8(hostArg.getValue().c_str());
    QString port = QString::fromUtf8(portArg.getValue().c_str());
    QString user = QString::fromUtf8(userArg.getValue().c_str());
    QString pass = QString::fromUtf8(passArg.getValue().c_str());
    QString schema = QString::fromUtf8(schemaArg.getValue().c_str());
    QString auditFile = QString::fromUtf8(auditArg.getValue().c_str());
    QString outputFile = QString::fromUtf8(outputArg.getValue().c_str());
    QString tempDir = QString::fromUtf8(tempArg.getValue().c_str());
    QString createXML = QString::fromUtf8(createArg.getValue().c_str());

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL","MyDB");
        QSqlDatabase dblite = QSqlDatabase::addDatabase("QSQLITE","DBLite");
        db.setHostName(host);
        db.setPort(port.toInt());
        db.setDatabaseName(schema);
        db.setUserName(user);
        db.setPassword(pass);
        if (db.open())
        {
            if (!QFile::exists(outputFile))
            {
                dblite.setDatabaseName(outputFile);
                if (dblite.open())
                {
                    if (genSnapHost(db,dblite,auditFile,host,user,pass,schema,tempDir,createXML) > 0)
                    {
                        db.close();
                        dblite.close();
                        return 1;
                    }
                }
                else
                {
                    db.close();
                    log("Cannot create snapshot file");
                    log(dblite.lastError().databaseText());
                    return 1;
                }
            }
            else
            {
                db.close();
                log("The sqlite file already exists");
                return 1;
            }
        }
        else
        {
            log("Cannot connect to database");
            log(db.lastError().databaseText());
            return 1;
        }
    }

    return 0;
}
