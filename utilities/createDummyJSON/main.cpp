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

#include <QCoreApplication>
#include <tclap/CmdLine.h>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QFile>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace pt = boost::property_tree;

void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf(temp.toUtf8().data());
}

void parseManifest(QDomNode node, pt::ptree &json)
{
    while (!node.isNull())
    {
        QString xmlCode;
        xmlCode = node.toElement().attribute("xmlcode","NONE");
        if (node.toElement().tagName() == "field")
        {
            if ((xmlCode != "NONE") && (xmlCode != "dummy"))
            {
                json.put(xmlCode.toStdString(),"dummy");
            }
        }
        else
        {
            pt::ptree childObject;
            parseManifest(node.firstChild(), childObject);
            pt::ptree repeatArray;
            repeatArray.push_front(std::make_pair("", childObject));
            json.put_child(xmlCode.toStdString(),repeatArray);
        }
        node = node.nextSibling();
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

    TCLAP::ValueArg<std::string> inputArg("i","input","Input manifest XML file",true,"","string");
    TCLAP::ValueArg<std::string> outputArg("o","output","Output JSON file",false,"./output.json","string");

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

            if (rootA.tagName() == "ODKImportXML")
            {
                QDomNode start = rootA.firstChild().firstChild();
                pt::ptree JSONRoot;
                JSONRoot.put("_xform_id_string","dummy");
                JSONRoot.put("_submissionid","dummy");
                JSONRoot.put("meta/instanceID","dummy");
                parseManifest(start, JSONRoot);
                pt::write_json(output.toStdString(),JSONRoot);
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
