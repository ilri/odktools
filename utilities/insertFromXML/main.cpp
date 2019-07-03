/*
InsertFromXML

Copyright (C) 2015-2017 International Livestock Research Institute.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

InsertFromXML is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

InsertFromXML is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with InsertFromXML.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <tclap/CmdLine.h>
#include <QtCore>
#include <QDomDocument>

void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toLocal8Bit().data());
}

QString fixString(QString source)
{
    QString res;
    int start;
    int finish;
    start = source.indexOf('\"');
    finish = source.lastIndexOf('\"');
    QString begin;
    QString end;
    QString middle;

    if (start != finish) //There are chars in between ""
    {
        begin = source.left(start);
        end = source.right(source.length()-finish-1);
        middle = source.mid(start+1,finish-start-1);
        res = begin + "“" + fixString(middle) + "”" + end; //Recursive
        res = res.replace('\n'," "); //Replace carry return for a space
        return res;
    }
    else
    {
        if ((start == -1) && (finish == -1)) //There are no " character
        {
            res = source;
            res = res.replace('\n'," "); //Replace carry return for a space
            return res;
        }
        else
        {
            if (start >= 0) //There is only one " character
            {
                res = source.replace('\"',"“");
                res = res.replace('\n'," "); //Replace carry return for a space
                return res;
            }
            else
            {
                res = source;
                res = res.replace('\n'," "); //Replace carry return for a space
                return res;
            }
        }
    }
}

void genInsert(QDomNode node, QTextStream &out)
{
    QString insertSQL;
    QDomNode value = node.firstChild();
    while (!value.isNull())
    {
        insertSQL = "INSERT INTO " + node.toElement().attribute("name","") + " (";
        insertSQL = insertSQL + node.toElement().attribute("clmcode","") + ",";
        insertSQL = insertSQL + node.toElement().attribute("clmdesc","");
        insertSQL = insertSQL + ")  VALUES ('";
        insertSQL = insertSQL + value.toElement().attribute("code","").replace("'","`") + "',\"";
        insertSQL = insertSQL + fixString(value.toElement().attribute("description","")) + "\");";
        out << insertSQL << "\n";

        value = value.nextSibling();
    }
}


int main(int argc, char *argv[])
{
    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * Insert from XML                                                   * \n";
    title = title + " * This tool create a SQL DML script file from a XML insert file     * \n";
    title = title + " * created by ODKToMySQL.                                            * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is usefull when dealing with multiple versions of an    * \n";
    title = title + " * ODK survey that were combined into a common XML schema using      * \n";
    title = title + " * compareInsertXML.                                                 * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.1");

    TCLAP::ValueArg<std::string> inputArg("i","input","Input insert XML file",true,"","string");
    TCLAP::ValueArg<std::string> outputArg("o","output","Output SQL file",false,"./insert.sql","string");


    cmd.add(inputArg);
    cmd.add(outputArg);

    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    QString input = QString::fromUtf8(inputArg.getValue().c_str());
    QString output = QString::fromUtf8(outputArg.getValue().c_str());


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

            if (rootA.tagName() == "insertValuesXML")
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

                out << "-- Code generated by insertFromXML" << "\n";
                out << "-- Created: " + date.toString("ddd MMMM d yyyy h:m:s ap")  << "\n";
                out << "-- by: insertFromXML Version 1.0" << "\n";
                out << "-- WARNING! All changes made in this file might be lost when running insertFromXML again" << "\n\n";

                QDomNode table = docA.documentElement().firstChild();
                while (!table.isNull())
                {
                    genInsert(table,out);
                    table = table.nextSibling();
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
