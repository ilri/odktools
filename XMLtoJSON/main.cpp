#include <QCoreApplication>
#include <tclap/CmdLine.h>
#include <QDomNode>
#include <QDomElement>
#include <QDomDocument>
#include <QDomNodeList>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QStringList>
#include <unistd.h>
#include <stdio.h>


QString mainTag;

void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf(temp.toUtf8().data());
}

QJsonArray processNode(QStringList repeatArray,bool group,QDomNode node,QJsonObject &json)
{
    QDomNode start;
    start = node.firstChild();
    QStringList tagsArray;
    QJsonArray groupArray;

    while (!start.isNull())
    {
        //This will get the parent tags of the item to them form the name no the key in the JSON
        if (tagsArray.length() == 0)
        {
            QDomNode parent;
            parent= start.parentNode();
            while (!parent.isNull())
            {
                if ((parent.toElement().tagName() != "") && (mainTag != parent.toElement().tagName()))
                    tagsArray.prepend(parent.toElement().tagName()); //Preend them so we have the correct order
                parent = parent.parentNode();
            }
        }

        //If this tag does not have children (is not a repeat or a group)
        if (start.firstChildElement().isNull() == true)
        {
            if (!start.firstChild().nodeValue().isNull())
            {
                //Forms the key name using / as separator
                QString varName;
                if (tagsArray.length() >= 1)
                    varName = tagsArray.join("/") + "/" + start.toElement().tagName();
                else
                    varName = start.toElement().tagName();
                //If we are not processing a group then add the single var otherwise create an array for the group
                if (group == false)
                    json[varName] = start.firstChild().nodeValue();
                else
                {
                    QJsonObject ajvar;
                    ajvar[varName] = start.firstChild().nodeValue();
                    groupArray.append(ajvar);
                }
            }
        }
        else
        {            
            //This node has child. So is a group or a repeat
            QString varName;
            if (tagsArray.length() >= 1)
                varName = tagsArray.join("/") + "/" + start.toElement().tagName();
            else
                varName = start.toElement().tagName();

            if (repeatArray.indexOf(start.toElement().tagName()) >= 0)
            {
                //If its a repeat then enter into its content and store it in repContent
                QJsonObject repContent;
                processNode(repeatArray,false,start,repContent);
                //If the repeat tag is not in the JSON then create the key as an array and append the content
                //otherwise then just append the content
                if (json[varName].isNull())
                {
                    QJsonArray repArray;
                    repArray.append(repContent);
                    json[varName] = repArray;
                }
                else
                {
                    QJsonArray repArray;
                    repArray = json[varName].toArray();
                    repArray.append(repContent);
                    json[varName] = repArray;
                }
            }
            else
            {
                //If its a group then we need to extract the elements recursively because they don't generate an array
                QJsonArray elems;
                elems = processNode(repeatArray,true,start,json);
                for (int pos = 0; pos <= elems.count()-1;pos++)
                {
                    QString key;
                    QStringList keys;
                    keys = elems[pos].toObject().keys();
                    key = keys[0];
                    json[key] = elems[pos].toObject().value(key).toString();
                }
            }
        }
        start = start.nextSibling();
    }
    if (group)
        return groupArray;

    return QJsonArray();
}

int main(int argc, char *argv[])
{
    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * XMLToJSON                                                         * \n";
    title = title + " * This tool converts an ODK XML data file into JSON.                * \n";
    title = title + " * JSON files are usually generated from 'formhubtojson' however     * \n";
    title = title + " * sometimes it is convenient to convert raw XML files. For example, * \n";
    title = title + " * in case that the data in Formshare get lost and only the XML      * \n";
    title = title + " * files from devices exists. It is also useful if you want to       * \n";
    title = title + " * create your own FormShare ;-)                                     * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is part of ODK Tools (c) Bioversity Costa Rica, 2017    * \n";
    title = title + " * Author: Carlos Quiros (c.f.quiros@cgiar.org / cquiros@qlands.com) * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.0");

    TCLAP::ValueArg<std::string> xmlArg("i","xml","Input XML File",true,"","string");
    TCLAP::ValueArg<std::string> jsonArg("o","json","Input JSON File",false,"","string");
    TCLAP::ValueArg<std::string> formArg("x","xform","Input XML Form File",true,"","string");

    cmd.add(xmlArg);
    cmd.add(jsonArg);
    cmd.add(formArg);
    cmd.parse( argc, argv );

    QJsonObject extRoot;
    if (!isatty(fileno(stdin)))
    {
        QTextStream qtin(stdin);
        QString stdindata = qtin.readAll();
        QByteArray ba;
        ba = stdindata.toUtf8();
        QJsonDocument exJSONDoc(QJsonDocument::fromJson(ba));
        if (!exJSONDoc.isNull())
            extRoot = exJSONDoc.object();
        else
        {
            log("Error parsing stdin JSON content");
            return 1;
        }
    }

    //Getting the variables from the command

    QString xmlFile = QString::fromUtf8(xmlArg.getValue().c_str());
    QString formFile = QString::fromUtf8(formArg.getValue().c_str());
    QString jsonFile = QString::fromUtf8(jsonArg.getValue().c_str());

    QDomDocument xForm("xform");
    QFile xFormFile(formFile);
    if (!xFormFile.open(QIODevice::ReadOnly))
    {
        log("Couldn't open XML form file");
        return 1;
    }
    if (!xForm.setContent(&xFormFile))
    {
        log("Couldn't parse XML form file");
        xFormFile.close();
        return 1;
    }
    xFormFile.close();
    QStringList repeatArray;
    QDomNodeList repeats;
    repeats = xForm.elementsByTagName("repeat");
    for (int pos = 0; pos <= repeats.count()-1;pos++)
    {
        QString nodeset;
        QStringList nodeArray;
        nodeset = repeats.item(pos).toElement().attribute("nodeset");
        nodeArray = nodeset.split("/",QString::SkipEmptyParts);
        if (nodeArray.length() > 0)
            repeatArray.append(nodeArray[nodeArray.length()-1]);
    }

    QDomDocument inputXML("xml");
    QFile inputFile(xmlFile);
    if (!inputFile.open(QIODevice::ReadOnly))
    {
        log("Couldn't open input XML data file");
        return 1;
    }
    if (!inputXML.setContent(&inputFile))
    {
        log("Couldn't parse input XML data file");
        inputFile.close();
        return 1;
    }
    inputFile.close();
    QDomElement root = inputXML.documentElement();
    mainTag = root.tagName();
    //QString empty;
    QJsonObject JSONRoot;
    processNode(repeatArray,false,root,JSONRoot);
    JSONRoot["_xform_id_string"] = root.attribute("id","");

    //Append any keys comming from stdin
    QStringList keys;
    keys = extRoot.keys();
    for (int n=0; n <= keys.count()-1; n++)
    {
        JSONRoot[keys[n]] = extRoot.value(keys[n]).toString();
    }
    QJsonDocument saveDoc(JSONRoot);

    //If no output file was given the print to stdout
    if (!jsonFile.isEmpty())
    {
        QFile saveFile(jsonFile);
        if (!saveFile.open(QIODevice::WriteOnly)) {
            log("Couldn't open output JSON file.");
            return 1;
        }
        saveFile.write(saveDoc.toJson());
        saveFile.close();
    }
    else
    {
        QTextStream out(stdout);
        out << saveDoc.toJson();
    }

    return 0;
}
