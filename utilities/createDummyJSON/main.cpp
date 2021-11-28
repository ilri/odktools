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
int resolve_type;

struct arraySizeItem
{
    QString name;
    int size;
};
typedef arraySizeItem TarraySizeItem;

QList<TarraySizeItem > arraySizes;

QDomDocument xmlCreateDocument;
QDomDocument xmlInsertDocument;

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

QStringList getLookUpValues(QString table)
{
    QStringList result;
    QDomElement rootInsert = xmlInsertDocument.documentElement();
    QDomNode insertNode = rootInsert.firstChild();
    while (!insertNode.isNull())
    {
        if (insertNode.toElement().attribute("name") == table)
        {
            QDomNode valueNode = insertNode.firstChild();
            while (!valueNode.isNull())
            {
                result.append(valueNode.toElement().attribute("code"));
                valueNode = valueNode.nextSibling();
            }
        }

        insertNode = insertNode.nextSibling();
    }
    return result;
}

QStringList separateMultiSelect(QString multiSelectTable, QString variableName)
{
    QStringList result;
    QDomElement rootCreate = xmlCreateDocument.documentElement();
    QDomNode tablesNode = rootCreate.firstChild().nextSibling();
    QDomNodeList tables = tablesNode.toElement().elementsByTagName("table");
    QDomNode createNode;
    for (int tbl = 0; tbl < tables.count(); tbl++)
    {
        if (tables.item(tbl).toElement().attribute("name") == multiSelectTable)
        {
            createNode = tables.item(tbl);
            break;
        }
    }
    if (!createNode.isNull())
    {
        QDomNode fieldNode = createNode.firstChild();
        while (!fieldNode.isNull())
        {
            if (fieldNode.toElement().attribute("rlookup","false") == "true")
            {
                QString lookupTableName = fieldNode.toElement().attribute("rtable");
                result = getLookUpValues(lookupTableName);
            }

            fieldNode = fieldNode.nextSibling();
        }
    }

    for (int pos = 0; pos < result.count(); pos++)
    {
        result[pos] = variableName + "/" + result[pos];
    }

    return result;
}

void parseCreateFile(QDomNode node, pt::ptree &json)
{
    while (!node.isNull())
    {
        QString xmlCode;
        if (!keysForRepo)
            xmlCode = node.toElement().attribute("xmlcode","NONE");
        else
            xmlCode = node.toElement().attribute("name","NONE");
        if (node.toElement().tagName() == "field")
        {            
            bool addkey = true;
            if (!keysForRepo)
            {
                if ((node.toElement().attribute("key","false") == "true") && ((node.toElement().attribute("rtable","") != "") || (node.toElement().attribute("rlookup","false") == "true")))
                    addkey = false;
            }
            if ((xmlCode.toUpper() != "NONE") && (xmlCode != "_dummy") && (addkey))
            {
                if ((node.toElement().attribute("isMultiSelect","false") == "true") && (node.toElement().attribute("multiSelectTable") != ""))
                {
                    if (resolve_type == 1 || resolve_type == 2)
                        json.put(xmlCode.toStdString(),"dummy");
                    else
                    {
                        if (keysForRepo)
                        {
                            json.put(xmlCode.toStdString(),"dummy");
                            json.put(xmlCode.toStdString() + "-desc","dummy");
                        }
                        else
                            json.put(xmlCode.toStdString(),"dummy");
                    }
                }
                else
                {
                    if (node.toElement().attribute("rlookup","false") == "true")
                    {
                        if (resolve_type == 1 || resolve_type == 2)
                            json.put(xmlCode.toStdString(),"dummy");
                        else
                        {
                            if (keysForRepo)
                            {
                                json.put(xmlCode.toStdString(),"dummy");
                                json.put(xmlCode.toStdString() + "-desc","dummy");
                            }
                            else
                                json.put(xmlCode.toStdString(),"dummy");
                        }
                    }
                    else
                        json.put(xmlCode.toStdString(),"dummy");
                }
            }
        }
        else
        {
            if (node.toElement().attribute("name").indexOf("_msel_") < 0)
            {
                if (!keysForRepo)
                {
                    json.put(xmlCode.toStdString() + "_count","dummy");
                }
                pt::ptree childObject;
                parseCreateFile(node.firstChild(), childObject);
                pt::ptree repeatArray;
                int arraySize;
                arraySize = getArraySize(xmlCode);
                for (int pos = 1 ; pos <= arraySize; pos++)
                    repeatArray.push_front(std::make_pair("", childObject));
                json.put_child(xmlCode.toStdString(),repeatArray);
            }
        }
        node = node.nextSibling();
    }
}

int main(int argc, char *argv[])
{
    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * Create Dummy JSON                                                 * \n";
    title = title + " * This tool creates a dummy data file in JSON format based on the   * \n";
    title = title + " * create and insert XML files.                                      * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is useful when flatting JSONs into a CSV format while   * \n";
    title = title + " * conserving a proper order of the variables.                       * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "2.0");

    TCLAP::ValueArg<std::string> createArg("c","create","Input create XML file",false,"","string");    
    TCLAP::ValueArg<std::string> outputArg("o","output","Output JSON file",false,"./output.json","string");
    TCLAP::ValueArg<std::string> arraysArg("a","arrays","Array sizes as defined as name:size,name:size",false,"","string");
    TCLAP::ValueArg<std::string> resolveArg("l","resolve","Resolve lookup values: 1=Codes only (default), 2=Descriptions, 3=Codes and descriptions",false,"1","string");
    TCLAP::SwitchArg repoSwitch("r","repository","Generate keys for repository", cmd, false);    

    cmd.add(createArg);    
    cmd.add(outputArg);
    cmd.add(arraysArg);
    cmd.add(resolveArg);

    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command    

    QString xmlCreate = QString::fromUtf8(createArg.getValue().c_str());

    QString output = QString::fromUtf8(outputArg.getValue().c_str());
    QString sarrays = QString::fromUtf8(arraysArg.getValue().c_str());
    QString tresolve_type = QString::fromUtf8(resolveArg.getValue().c_str());
    resolve_type = tresolve_type.toInt();
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

    if (xmlCreate != output)
    {
        if (QFile::exists(xmlCreate))
        {            
            //Openning and parsing the Create XML file
            QFile fileCreate(xmlCreate);
            if (!fileCreate.open(QIODevice::ReadOnly))
            {
                log("Cannot open create xml file");
                return 1;
            }
            if (!xmlCreateDocument.setContent(&fileCreate))
            {
                log("Cannot parse create xml file");
                fileCreate.close();
                return 1;
            }
            fileCreate.close();


            QDomElement rootA = xmlCreateDocument.documentElement();

            if (rootA.tagName() == "XMLSchemaStructure")
            {
                QDomNode start = rootA.firstChild().nextSibling().firstChild().firstChild();
                pt::ptree JSONRoot;
                if (!keysForRepo)
                {
                    JSONRoot.put("_xform_id_string","dummy");
                    JSONRoot.put("_submission_id","dummy");
                    JSONRoot.put("meta/instanceID","dummy");
                    JSONRoot.put("_project_code","dummy");
                    JSONRoot.put("_user_id","dummy");
                }
                parseCreateFile(start, JSONRoot);
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
