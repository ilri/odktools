/*
CreateFromXML

Copyright (C) 2015-2017 International Livestock Research Institute.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

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

QString command;

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
    printf("%s",temp.toLocal8Bit().data());
}

int idx;

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

void createTable(QString tableName,QList<QDomNode> fields,QTextStream &outstrm, QString tableDesc, bool isLookUp)
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

    qDebug() << "Creating table: " + tableName;

    sfields << "CREATE TABLE IF NOT EXISTS " + tableName + "(" << "\n";
    keys << "PRIMARY KEY (";

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
            field = field + " NOT NULL COMMENT \"" + efield.attribute("desc","") + "\", ";
            keys << efield.attribute("name","") + " , ";
        }
        else
            field = field + " COMMENT \"" + efield.attribute("desc","") + "\", ";

        sfields << field << "\n";

        if (efield.attribute("rtable","") != "")
        {
            if (efield.attribute("rlookup","") == "true")
            {
                idx++;
                index = "INDEX fk" + QString::number(idx) + "_" + tableName + "_" + efield.attribute("rtable","") ;
                indexes << index.left(64) + " (" + efield.attribute("name","") + ") , " << "\n";

                constraint = "CONSTRAINT fk" + QString::number(idx) + "_" + tableName + "_" + efield.attribute("rtable","");
                rels << constraint.left(64) << "\n";
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
        idx++;
        index = "INDEX fk" + QString::number(idx) + "_" + tableName + "_" + relTables[pos].name ;
        indexes << index.left(64) + " (" + getFields(relTables[pos]) + ") , " << "\n";

        constraint = "CONSTRAINT fk" + QString::number(idx) + "_" + tableName + "_" + relTables[pos].name;
        rels << constraint.left(64) << "\n";
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
    for (clm = 0; clm <= keys.count() -1;clm++)
    {
        keysql = keysql + keys[clm];
    }
    clm = keysql.lastIndexOf(",");
    keysql = keysql.left(clm) + ") , \n";

    sql = sql + keysql;

    for (clm = 0; clm <= indexes.count() -1;clm++)
    {
        sql = sql + indexes[clm];
    }
    for (clm = 0; clm <= rels.count() -1;clm++)
    {
        sql = sql + rels[clm];
    }
    clm = sql.lastIndexOf(",");
    sql = sql.left(clm);
    sql = sql + ")" + "\n ENGINE = InnoDB CHARSET=utf8 COMMENT = \"" + tableDesc + "\"; \n";
    idx++;
    sql = sql + "CREATE UNIQUE INDEX rowuuid" + QString::number(idx) + " ON " + tableName + "(rowuuid);\n";
    if (isLookUp)
        sql = sql + "CREATE TRIGGER uudi_" + tableName + " BEFORE INSERT ON " + tableName + " FOR EACH ROW SET new.rowuuid = uuid();\n\n";
    else
        sql = sql + "\n";
    outstrm << sql;
}

void procLKPTables(QDomNode start, QTextStream &outstrm)
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
        createTable(node.toElement().attribute("name",""),fields,outstrm,node.toElement().attribute("desc",""),true);
        //Here we create the insert with the table definition
        node = node.nextSibling();
    }
}

void procTables(QDomNode start, QTextStream &outstrm)
{
    QString tableName;
    tableName = start.toElement().attribute("name");
    QString tabledesc;
    tabledesc = start.toElement().attribute("desc");
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
                createTable(tableName,fields,outstrm,tabledesc,false); //Create the current table
                fields.clear(); //Clear the fields
                proc = true;
            }
            procTables(node,outstrm); //Recursive process the subtable
        }
        node = node.nextSibling();
    }
    if (fields.count() > 0)
    {
        createTable(tableName,fields,outstrm,tabledesc,false);
    }
}

int main(int argc, char *argv[])
{
    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * Create from XML                                                   * \n";
    title = title + " * This tool create a SQL DDL script file from a XML schema file     * \n";
    title = title + " * created by ODKToMySQL.                                            * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is usefull when dealing with multiple versions of an    * \n";
    title = title + " * ODK survey that were combined into a common XML schema using      * \n";
    title = title + " * compareCreateXML.                                                 * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.1");

    TCLAP::ValueArg<std::string> inputArg("i","input","Input create XML file",true,"","string");
    TCLAP::ValueArg<std::string> outputArg("o","output","Output SQL file",false,"./create.sql","string");

    for (int i = 1; i < argc; i++)
    {
        command = command + argv[i] + " ";
    }

    cmd.add(inputArg);
    cmd.add(outputArg);

    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    QString input = QString::fromUtf8(inputArg.getValue().c_str());
    QString output = QString::fromUtf8(outputArg.getValue().c_str());
    idx = 0;

    if (input != output)
    {
        if (QFile::exists(input))
        {
            //Openning and parsing input file A
            QDomDocument docA("input");
            QFile fileA(input);
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
                QFile file(output);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    log("Cannot create output file");
                    return 1;
                }

                QDateTime date;
                date = QDateTime::currentDateTime();

                QTextStream out(&file);

                out << "-- Code generated by createFromXML" << "\n";
                out << "-- " + command << "\n";
                out << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
                out << "-- by: createFromXML Version 1.0" << "\n";
                out << "-- WARNING! All changes made in this file might be lost when running createFromXML again" << "\n\n";

                QDomNode lkpTables = docA.documentElement().firstChild();
                QDomNode tables = docA.documentElement().firstChild().nextSibling();
                if (!lkpTables.isNull())
                {
                    procLKPTables(lkpTables.firstChild(),out);
                }
                if (!tables.isNull())
                {
                    procTables(tables.firstChild(),out);
                }
            }
            else
            {
                log("Input document is not a XML create file");
                return 1;
            }
        }
        else
        {
            log("Input file does not exists");
            return 1;
        }
    }
    else
    {
        log("Fatal: Input files and output file are the same.");
        return 1;
    }

    return 0;
}
