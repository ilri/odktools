/*
This file is part of ODKTools.

Copyright (C) 2015 International Livestock Research Institute.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

ODKTools is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

ODKTools is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with ODKTools.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <tclap/CmdLine.h>
#include <QtCore>
#include <QDomElement>

//This logs messages to the terminal. We use printf because qDebug does not log in relase
void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf(temp.toUtf8().data());
}

void fatal(QString message)
{
    fprintf(stderr, "\033[31m%s\033[0m \n", message.toUtf8().data());
}

QDomNode findTable(QDomDocument docB,QString tableName)
{
    QDomNodeList tables;
    tables = docB.elementsByTagName("table");
    for (int pos = 0; pos < tables.count();pos++)
    {
        if (tables.item(pos).toElement().attribute("name","") == tableName)
            return tables.item(pos);
    }
    QDomNode null;
    return null;
}

QDomNode findValue(QDomNode table,QString code)
{
    QDomNode node;
    node = table.firstChild();
    while (!node.isNull())
    {
        if (node.toElement().attribute("code","") == code)
            return node;
        node = node.nextSibling();
    }
    QDomNode null;
    return null;
}

void compareLKPTables(QDomNode table,QDomDocument &docB)
{
    QDomNode node;
    node = table;
    while (!node.isNull())
    {
        QDomNode tableFound = findTable(docB,node.toElement().attribute("name",""));
        if (!tableFound.isNull())
        {
            QDomNode field = node.firstChild();
            while (!field.isNull())
            {
                QDomNode fieldFound = findValue(tableFound,field.toElement().attribute("code",""));
                if (!fieldFound.isNull())
                {
                    if (field.toElement().attribute("description","") != fieldFound.toElement().attribute("description",""))
                        fatal("VNS:Value " + field.toElement().attribute("code","") + " of lookup table " + node.toElement().attribute("name","") + " from A not the same in B");
                }
                else
                {
                    log("VNF:Value " + field.toElement().attribute("code","") + " of lookup table " + node.toElement().attribute("name","") + " from A not found in B");
                    tableFound.appendChild(field.cloneNode(true));
                }
                field = field.nextSibling();
            }
        }
        else
        {
            log("TNF:Lookup table " + node.toElement().attribute("name","") + " from A not found in B");
            //Now adds the lookup table
            docB.documentElement().appendChild(node.cloneNode(true));
        }
        node = node.nextSibling();
    }
}

int main(int argc, char *argv[])
{
    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * Compare Insert XML 1.0                                            * \n";
    title = title + " * This tool compares two insert XML files (A and B) for incremental * \n";
    title = title + " * changes.                                                          * \n";
    title = title + " *                                                                   * \n";
    title = title + " * The tool informs of lookup values in A that are not in B.         * \n";
    title = title + " * The tool can also create a combined file C that appends           * \n";
    title = title + " * not found values.                                                 * \n";
    title = title + " *                                                                   * \n";
    title = title + " * Nomenclature:                                                     * \n";
    title = title + " *   TNF: Lookup table not found.                                    * \n";
    title = title + " *   VNF: Value not found.                                           * \n";
    title = title + " *   VNS: The values is not the same.                                * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is usefull when dealing with multiple versions of an    * \n";
    title = title + " * ODK survey that must be combined in one common database.          * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is part of ODK Tools (c) ILRI-RMG, 2015                 * \n";
    title = title + " * Author: Carlos Quiros (c.f.quiros@cgiar.org / cquiros@qlands.com) * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.0");

    TCLAP::ValueArg<std::string> aArg("a","inputa","Input insert XML file A",true,"","string");
    TCLAP::ValueArg<std::string> bArg("b","inputb","Input insert XML file B",true,"","string");
    TCLAP::ValueArg<std::string> cArg("c","outputc","Output insert XML file C",false,"./combined-insert.xml","string");

    cmd.add(aArg);
    cmd.add(bArg);
    cmd.add(cArg);


    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    QString inputA = QString::fromUtf8(aArg.getValue().c_str());
    QString inputB = QString::fromUtf8(bArg.getValue().c_str());
    QString outputC = QString::fromUtf8(cArg.getValue().c_str());

    if (inputA != inputB)
    {
        if ((QFile::exists(inputA)) && (QFile::exists(inputB)))
        {
            //Openning and parsing input file A
            QDomDocument docA("inputA");
            QFile fileA(inputA);
            if (!fileA.open(QIODevice::ReadOnly))
            {
                log("Cannot open input file A");
                return 1;
            }
            if (!docA.setContent(&fileA))
            {
                log("Cannot parse document for input file A");
                fileA.close();
                return 1;
            }
            fileA.close();

            //Openning and parsing input file B
            QDomDocument docB("inputB");
            QFile fileB(inputB);
            if (!fileB.open(QIODevice::ReadOnly))
            {
                log("Cannot open input file B");
                return 1;
            }
            if (!docB.setContent(&fileB))
            {
                log("Cannot parse document for input file B");
                fileB.close();
                return 1;
            }
            fileB.close();

            QDomElement rootA = docA.documentElement();
            QDomElement rootB = docB.documentElement();
            if ((rootA.tagName() == "insertValuesXML") && (rootB.tagName() == "insertValuesXML"))
            {
                //Comparing lookup tables
                if ((!rootA.firstChild().isNull()) && (!rootB.firstChild().isNull()))
                    qDebug() << "Comparing lookup tables";
                compareLKPTables(rootA.firstChild(),docB);

                //Create the manifext file. If exist it get overwriten
                if (QFile::exists(outputC))
                    QFile::remove(outputC);
                QFile file(outputC);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QTextStream out(&file);
                    out.setCodec("UTF-8");
                    docB.save(out,1,QDomNode::EncodingFromTextStream);
                    file.close();
                }
                else
                    log("Error: Cannot create XML combined file");

            }
            else
            {
                if (!(rootA.tagName() == "ODKImportXML"))
                {
                    log("Input document A is not a insert XML file");
                    return 1;
                }
                if (!(rootB.tagName() == "ODKImportXML"))
                {
                    log("Input document B is not a insert XML file");
                    return 1;
                }
            }

        }
        else
        {
            if (!QFile::exists(inputA))
            {
                log("Input file A does not exists");
                return 1;
            }
            if (!QFile::exists(inputB))
            {
                log("Input file B does not exists");
                return 1;
            }
        }
    }
    else
    {
        log("Input files A and B are the same. No point in comparing them.");
        return 1;
    }

    return 0;
}
