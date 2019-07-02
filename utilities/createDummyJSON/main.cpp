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
#include <QList>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace pt = boost::property_tree;

bool keysForRepo;

struct arraySizeItem
{
    QString name;
    int size;
};
typedef arraySizeItem TarraySizeItem;

QList<TarraySizeItem > arraySizes;

void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toLocal8Bit().data());
}

int getArraySize(QString name)
{
    for (int pos = 0; pos < arraySizes.count(); pos++)
    {
        if (arraySizes[pos].name == name)
            return arraySizes[pos].size;
    }
    return 1;
}

void parseManifest(QDomNode node, pt::ptree &json)
{
    while (!node.isNull())
    {
        QString xmlCode;
        if (!keysForRepo)
            xmlCode = node.toElement().attribute("xmlcode","NONE");
        else
            xmlCode = node.toElement().attribute("mysqlcode","NONE");
        if (node.toElement().tagName() == "field")
        {
            if ((xmlCode != "NONE") && (xmlCode != "dummy") && (xmlCode != "rowuuid"))
            {
                json.put(xmlCode.toStdString(),"dummy");
            }
        }
        else
        {
            pt::ptree childObject;
            parseManifest(node.firstChild(), childObject);
            pt::ptree repeatArray;
            int arraySize;
            arraySize = getArraySize(xmlCode);
            for (int pos = 1 ; pos <= arraySize; pos++)
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
    title = title + " * Create Dummy JSON                                                 * \n";
    title = title + " * This tool creates a dummy data file in JSON format based on a     * \n";
    title = title + " * manifest file.                                                    * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is usefull when flatting JSONs into a CSV format while  * \n";
    title = title + " * conserving a proper order of the variables.                       * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.1");

    TCLAP::ValueArg<std::string> inputArg("i","input","Input manifest XML file",true,"","string");
    TCLAP::ValueArg<std::string> outputArg("o","output","Output JSON file",false,"./output.json","string");
    TCLAP::ValueArg<std::string> arraysArg("a","arrays","Array sizes as defined as name:size,name:size",false,"","string");
    TCLAP::SwitchArg repoSwitch("r","repository","Generate keys for repository", cmd, false);

    cmd.add(inputArg);
    cmd.add(outputArg);
    cmd.add(arraysArg);

    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    QString input = QString::fromUtf8(inputArg.getValue().c_str());
    QString output = QString::fromUtf8(outputArg.getValue().c_str());
    QString sarrays = QString::fromUtf8(arraysArg.getValue().c_str());
    keysForRepo = repoSwitch.getValue();

    if (sarrays != "")
    {
        QStringList items = sarrays.split(",",QString::SkipEmptyParts);
        for (int pos = 0; pos < items.count(); pos++)
        {
            QStringList parts = items[pos].split(":",QString::SkipEmptyParts);
            if (parts.length() == 2)
            {
                TarraySizeItem item;
                item.name = parts[0];
                item.size = parts[1].toInt();
                arraySizes.append(item);
            }
        }
    }

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
                if (!keysForRepo)
                {
                    JSONRoot.put("_xform_id_string","dummy");
                    JSONRoot.put("_submissionid","dummy");
                    JSONRoot.put("meta/instanceID","dummy");
                }
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
