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

QString mainTag;

void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf(temp.toUtf8().data());
}

QJsonArray processNode(QStringList repeatArray,bool group,bool table,QDomNode node,QJsonObject &json)
{
    QDomNode start;
    start = node.firstChild();
    QStringList tagsArray;
    QJsonArray groupArray;
    QJsonArray tableArray;

    while (!start.isNull())
    {
        //This will get the parent nodes
        if (tagsArray.length() == 0)
        {
            QDomNode parent;
            parent= start.parentNode();
            while (!parent.isNull())
            {
                if ((parent.toElement().tagName() != "") && (mainTag != parent.toElement().tagName()))
                    tagsArray.prepend(parent.toElement().tagName());
                parent = parent.parentNode();
            }
        }

        if (start.firstChildElement().isNull() == true)
        {
            if (!start.firstChild().nodeValue().isNull())
            {
            QString varName;
            if (tagsArray.length() >= 1)
                varName = tagsArray.join("/") + "/" + start.toElement().tagName();
            else
                varName = start.toElement().tagName();
            if (group == false)
                json[varName] = start.firstChild().nodeValue();
            else
            {
                if (group == true)
                {
                    QJsonObject ajvar;
                    ajvar[varName] = start.firstChild().nodeValue();
                    groupArray.append(ajvar);
                }
                if (table == true)
                {
                    QJsonObject ajvar;
                    ajvar[varName] = start.firstChild().nodeValue();
                    tableArray.append(ajvar);
                }
            }}

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
                //QJsonObject temp;
                QJsonObject repContent;
                processNode(repeatArray,false,true,start,repContent);
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
                QJsonArray elems;
                elems = processNode(repeatArray,true,false,start,json);
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
    if (table)
        return QJsonArray();
    return QJsonArray();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * XMLToJSON                                                         * \n";
    title = title + " * This tool converts a ODK XML data file into JSON.                 * \n";
    title = title + " * The JSON input files are usually generated from 'mongotojson      * \n";
    title = title + " * however sometimes is convenient to convert the raw XML inteself   * \n";
    title = title + " * just in case the date in formshare get lost and only the XMLs     * \n";
    title = title + " * in the device are backed up.                                      * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is part of ODK Tools (c) Bioversity Costa Rica, 2017    * \n";
    title = title + " * Author: Carlos Quiros (c.f.quiros@cgiar.org / cquiros@qlands.com) * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.0");

    TCLAP::ValueArg<std::string> xmlArg("i","xml","Input XML File",true,"","string");
    TCLAP::ValueArg<std::string> jsonArg("o","json","Input JSON File",true,"","string");
    TCLAP::ValueArg<std::string> formArg("x","xform","Input XML Form File",true,"","string");

    cmd.add(xmlArg);
    cmd.add(jsonArg);
    cmd.add(formArg);
    cmd.parse( argc, argv );


    //Getting the variables from the command

    QString xmlFile = QString::fromUtf8(xmlArg.getValue().c_str());
    QString formFile = QString::fromUtf8(formArg.getValue().c_str());

    QString jsonFile = QString::fromUtf8(jsonArg.getValue().c_str());

    QDomDocument xForm("xform");
    QFile xFormFile(formFile);
    if (!xFormFile.open(QIODevice::ReadOnly))
    {
        log("Cannot open XML form file");
        return 1;
    }
    if (!xForm.setContent(&xFormFile))
    {
        log("Cannot parse input file");
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
        log("Cannot open input file");
        return 1;
    }
    if (!inputXML.setContent(&inputFile))
    {
        log("Cannot parse input file");
        inputFile.close();
        return 1;
    }
    inputFile.close();
    QDomElement root = inputXML.documentElement();
    mainTag = root.tagName();
    //QString empty;
    QJsonObject JSONRoot;
    processNode(repeatArray,false,false,root,JSONRoot);
    JSONRoot["_xform_id_string"] = root.attribute("id","");

    QFile saveFile(jsonFile);

    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning("Couldn't open save file.");
        return 1;
    }

    QJsonDocument saveDoc(JSONRoot);
    saveFile.write(saveDoc.toJson());
    saveFile.close();

    return 0;

}
