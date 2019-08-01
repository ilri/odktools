/*
MySQLToSQlite

Copyright (C) 2019 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com)

CreateFromXML is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

CreateFromXML is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with CreateFromXML.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <tclap/CmdLine.h>
#include <QtCore>
#include <QDomDocument>
#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QStringList>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>

namespace pt = boost::property_tree;

struct tblDef
{
    QString name;
    QString create;
    QStringList indexes;
};
typedef tblDef TtblDef;

QList <TtblDef> lst_tables;

struct relatedField
{
    QString name;
    QString rname;
};
typedef relatedField TrelatedField;

struct relatedTable
{
    QString name;
    QList< TrelatedField> fields;
};
typedef relatedTable TrelatedTable;

void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toUtf8().data());
}

int relTableFound(QList <TrelatedTable> relTables, QString tableName)
{
    for (int pos = 0; pos < relTables.count();pos++)
    {
        if (relTables[pos].name == tableName)
            return pos;
    }
    return -1;
}

QString getFields(TrelatedTable table)
{
    QString res;
    for (int pos = 0; pos < table.fields.count();pos++)
    {
        res = res + table.fields[pos].name + ",";
    }
    if (res.length() > 0)
        res = res.left(res.length()-1);
    return res;
}

QString getRelatedFields(TrelatedTable table)
{
    QString res;
    for (int pos = 0; pos < table.fields.count();pos++)
    {
        res = res + table.fields[pos].rname + ",";
    }
    if (res.length() > 0)
        res = res.left(res.length()-1);
    return res;
}

void createTable(QString tableName,QList<QDomNode> fields)
{
    QStringList sfields;
    QStringList indexes;
    QStringList keys;
    QStringList rels;

    QString sql;
    QString keysql;
    QString field;

    QString index;
    QString constraint;
    int pos;

    QList <TrelatedTable> relTables;

    sfields << "CREATE TABLE IF NOT EXISTS " + tableName + "(" << "\n";

    for (int fld = 0; fld < fields.count(); fld++)
    {
        QDomElement efield = fields[fld].toElement();

        field = "";
        if ((efield.attribute("type","") == "varchar") || (efield.attribute("type","") == "int"))
            field = efield.attribute("name","") + " " + efield.attribute("type","") + "(" + efield.attribute("size","") + ")";
        else
            if (efield.attribute("type","") == "decimal")
                field = efield.attribute("name","") + " " + efield.attribute("type","") + "(" + efield.attribute("size","") + "," + efield.attribute("decsize","") + ")";
            else
                field = efield.attribute("name","") + " " + efield.attribute("type","");

        if (efield.attribute("key","false") == "true")
        {
            field = field + " NOT NULL, ";
            keys << efield.attribute("name","");
        }
        else
        {
            if (efield.attribute("name","") == "rowuuid")
                field = field + " DEFAULT (lower(hex(randomblob(4))) || '-' || lower(hex(randomblob(2))) || '-4' || substr(lower(hex(randomblob(2))),2) || '-' || substr('89ab',abs(random()) % 4 + 1, 1) || substr(lower(hex(randomblob(2))),2) || '-' || lower(hex(randomblob(6)))) , ";
            else
                field = field + " , ";
        }

        sfields << field << "\n";

        if (efield.attribute("rtable","") != "")
        {
            if (efield.attribute("rlookup","") == "true")
            {
                QUuid triggerUUID=QUuid::createUuid();
                QString strTriggerUUID=triggerUUID.toString().replace("{","").replace("}","").replace("-","_");

                index = "CREATE INDEX idx_" + strTriggerUUID + " ON " + tableName;
                indexes << index + " (" + efield.attribute("name","") + ");\n";

                constraint = "CONSTRAINT fk_" + strTriggerUUID;
                rels << constraint << "\n";
                rels << "FOREIGN KEY (" + efield.attribute("name","") + ")" << "\n";
                rels << "REFERENCES " + efield.attribute("rtable","") + " (" + efield.attribute("rfield","") + ")" << "\n";

                rels << "ON DELETE RESTRICT " << "\n";
                rels << "ON UPDATE NO ACTION," << "\n";
            }
            else
            {
                pos = relTableFound(relTables,efield.attribute("rtable",""));
                if (pos >= 0)
                {
                    TrelatedField relfield;
                    relfield.name = efield.attribute("name","");
                    relfield.rname = efield.attribute("rfield","");
                    relTables[pos].fields.append(relfield);
                }
                else
                {
                    TrelatedTable relTable;
                    relTable.name = efield.attribute("rtable","");

                    TrelatedField relfield;
                    relfield.name = efield.attribute("name","");
                    relfield.rname = efield.attribute("rfield","");
                    relTable.fields.append(relfield);
                    relTables.append(relTable);
                }
            }
        }
    }
    //Process combined foreign keys
    for (pos = 0; pos < relTables.count();pos++)
    {
        QUuid triggerUUID=QUuid::createUuid();
        QString strTriggerUUID=triggerUUID.toString().replace("{","").replace("}","").replace("-","_");
        index = "CREATE INDEX idx_" + strTriggerUUID + " ON " + tableName;
        indexes << index + " (" + getFields(relTables[pos]) + ") ;\n";

        constraint = "CONSTRAINT fk_" + strTriggerUUID;
        rels << constraint << "\n";
        rels << "FOREIGN KEY (" + getFields(relTables[pos]) + ")" << "\n";
        rels << "REFERENCES " + relTables[pos].name + " (" + getRelatedFields(relTables[pos]) + ")" << "\n";

        rels << "ON DELETE CASCADE " << "\n";
        rels << "ON UPDATE NO ACTION," << "\n";

    }
    int clm;
    //Contatenate al different pieces of the create script into one SQL
    for (clm = 0; clm <= sfields.count() -1;clm++)
    {
        sql = sql + sfields[clm];
    }
    keysql = "PRIMARY KEY (" + keys.join(",") + ") , UNIQUE(rowuuid), \n";
    sql = sql + keysql;

    for (clm = 0; clm <= rels.count() -1;clm++)
    {
        sql = sql + rels[clm];
    }
    clm = sql.lastIndexOf(",");
    sql = sql.left(clm);
    sql = sql + ");\n";
    TtblDef a_table;
    a_table.name = tableName;
    a_table.create = sql;
    for (clm = 0; clm <= indexes.count() -1;clm++)
    {
        a_table.indexes.append(indexes[clm]);
    }
    lst_tables.append(a_table);
}

void procLKPTables(QDomNode start)
{
    QDomNode node = start;
    QList<QDomNode> fields;
    while (!node.isNull())
    {
        fields.clear();
        QDomNode field = node.firstChild();
        while (!field.isNull())
        {
            fields.append(field);
            field = field.nextSibling();
        }
        createTable(node.toElement().attribute("name",""),fields);
        //Here we create the insert with the table definition
        node = node.nextSibling();
    }
}

void procTables(QDomNode start)
{
    QString tableName;
    tableName = start.toElement().attribute("name");
    //qDebug() << "Creating table:" + tableName;
    QDomNode node = start.firstChild();
    QList<QDomNode> fields;
    bool proc;
    proc = false;
    while (!node.isNull())
    {
        if (node.toElement().tagName() == "field")
            fields.append(node);
        if (node.toElement().tagName() == "table")
        {
            if (proc == false)
            {
                createTable(tableName,fields); //Create the current table
                fields.clear(); //Clear the fields
                proc = true;
            }
            procTables(node); //Recursive process the subtable
        }
        node = node.nextSibling();
    }
    if (fields.count() > 0)
    {
        createTable(tableName,fields);
    }
}

void parseData(QString fileName, QString tableName, QTextStream &out_file)
{
    pt::ptree tree;
    pt::read_xml(fileName.toUtf8().constData(), tree);
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
                    const boost::property_tree::ptree & aRow = row.second;
                    QStringList lst_fields;
                    QStringList lst_values;
                    BOOST_FOREACH(boost::property_tree::ptree::value_type const&field, aRow.get_child("") )
                    {
                        const std::string & fkey = field.first.data();
                        if (fkey == "field")
                        {
                            const boost::property_tree::ptree & aField = field.second;
                            std::string fname = aField.get<std::string>("<xmlattr>.name");
                            std::string fvalue = aField.data();
                            lst_fields.append(QString::fromStdString(fname));
                            QString value;
                            value = QString::fromStdString(fvalue);
                            value.replace("'","");
                            lst_values.append("'" + value + "'");
                        }
                    }
                    if (lst_fields.length() > 0)
                    {
                        QString sql;
                        sql = "INSERT INTO " + tableName + " (" + lst_fields.join(",") + ") VALUES (" + lst_values.join(",") + ");\n";
                        sql = sql.replace("''","null");
                        out_file << sql;
                    }
                }
            }
        }
    }
}

int procData(QString tempDir, QString host, QString user, QString pass, QString schema, QTextStream &out_file, QString tableName)
{
    QStringList ignoreTables;
    ignoreTables << "audit_log";
    ignoreTables << "dict_iso639";
    ignoreTables << "alembic_version";

    QString program = "mysqldump";
    QStringList arguments;
    arguments << "--single-transaction";
    arguments << "-h" << host;
    arguments << "-u" << user;
    arguments << "--password=" + pass;
    arguments << "--skip-triggers";
    arguments << "--no-create-info";
    arguments << "--xml";
    for (int pos = 0; pos < ignoreTables.count(); pos++)
        arguments << "--ignore-table=" + schema + "." + ignoreTables[pos];
    arguments << schema;
    arguments << tableName;

    QDir temp_dir(tempDir);
    if (!temp_dir.exists())
    {
        log("Temporary directory does not exists");
        return 1;
    }

    QString temp_dump_file = temp_dir.absolutePath() + QDir::separator() + "tempDump.xml";
    QProcess *myProcess = new QProcess();
    if (QFile::exists(temp_dump_file))
    {
        if (!QFile::remove(temp_dump_file))
        {
            log("Error removing temporary dump file");
            return 1;
        }
    }
    myProcess->setStandardOutputFile(temp_dump_file);
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
    //log("Processing: " + tableName);
    parseData(temp_dump_file, tableName, out_file);

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
                    QDomDocument docA("input");
                    QFile fileA(createXML);
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
                        QDomNode lkpTables = docA.documentElement().firstChild();
                        QDomNode tables = docA.documentElement().firstChild().nextSibling();
                        if (!lkpTables.isNull())
                        {
                            procLKPTables(lkpTables.firstChild());
                        }
                        if (!tables.isNull())
                        {
                            procTables(tables.firstChild());
                        }

                        QDir temp_dir(tempDir);
                        if (!temp_dir.exists())
                        {
                            if (!temp_dir.mkdir(temp_dir.absolutePath()))
                            {
                                log("Error creating temporary directory");
                                return 1;
                            }
                        }
                        QString out_sqlite_file;
                        out_sqlite_file = temp_dir.absolutePath() + QDir::separator() + "sqlite.sql";
                        QFile file(out_sqlite_file);
                        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                            return  1;
                        QTextStream out(&file);
                        out.setCodec("UTF-8");
                        out << "BEGIN;\n";
                        for (int pos = 0; pos < lst_tables.count(); pos++)
                        {
                            out << lst_tables[pos].create + "\n";
                            for (int idx=0; idx < lst_tables[pos].indexes.count(); idx++)
                            {
                                out << lst_tables[pos].indexes[idx] + "\n";
                            }
                        }
                        out << "\n";
                        log("Processing data....");
                        for (int pos = 0; pos < lst_tables.count(); pos++)
                        {
                            procData(tempDir, host, user, pass, schema, out, lst_tables[pos].name);
                        }
                        out << "COMMIT;\n";
                        file.close();

                        QString program;
                        QStringList arguments;
                        program = "sqlite3";
                        arguments << "-bail";
                        arguments << dblite.databaseName();
                        dblite.close();
                        QProcess *myProcess = new QProcess();
                        myProcess->setStandardInputFile(out_sqlite_file);
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

                        log("Loading audit triggers");
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

                        log("SQLite created successfully");

                    }
                    else
                    {
                        log("Input document is not a XML create file");
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
