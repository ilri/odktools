/*
XMLToJSON

Copyright (C) 2018 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

XMLToJSON is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

XMLToJSON is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with XMLToJSON.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

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
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/stream_translator.hpp>
#include <boost/foreach.hpp>

namespace pt = boost::property_tree;

QString mainTag;

void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toUtf8().data());
}

pt::ptree processNodeBoost(QStringList repeatArray,bool group,QDomNode node,pt::ptree &json)
{
    QDomNode start;
    start = node.firstChild();
    QStringList tagsArray;
    pt::ptree groupArray; //QJsonArray groupArray;

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
                    json.put(varName.toStdString(),start.firstChild().nodeValue().toStdString()); //json[varName] = start.firstChild().nodeValue();
                else
                {
                    pt::ptree ajvar; //QJsonObject ajvar;
                    ajvar.put(varName.toStdString(),start.firstChild().nodeValue().toStdString()); //ajvar[varName] = start.firstChild().nodeValue();
                    groupArray.push_front(std::make_pair("", ajvar));//groupArray.append(ajvar);
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
                pt::ptree repContent;//QJsonObject repContent;
                processNodeBoost(repeatArray,false,start,repContent);
                //If the repeat tag is not in the JSON then create the key as an array and append the content
                //otherwise then just append the content
                pt::ptree::const_assoc_iterator it;
                it = json.find(varName.toStdString());
                if (it == json.not_found())//if (json[varName].isNull())
                {
                    if (!repContent.empty())
                    {
                        pt::ptree repArray;//QJsonArray repArray;
                        repArray.push_front(std::make_pair("", repContent));//repArray.append(repContent);
                        json.put_child(varName.toStdString(),repArray);//json[varName] = repArray;
                    }
                }
                else
                {
                    if (!repContent.empty())
                    {
                        pt::ptree repArray;//QJsonArray repArray;
                        repArray = json.get_child(varName.toStdString());//repArray = json[varName].toArray();
                        repArray.push_front(std::make_pair("", repContent));//repArray.append(repContent);
                        json.put_child(varName.toStdString(),repArray); //json[varName] = repArray;
                    }
                }
            }
            else
            {
                //If its a group then we need to extract the elements recursively because they don't generate an array
                pt::ptree elems;//QJsonArray elems;
                elems = processNodeBoost(repeatArray,true,start,json);
                int count;
                count = 0;
                BOOST_FOREACH(boost::property_tree::ptree::value_type const&v, elems.get_child(""))
                {
                    count++;
                    const boost::property_tree::ptree &subtree = v.second;
                    BOOST_FOREACH(  boost::property_tree::ptree::value_type const&v2, subtree )
                    {
                        const std::string & key = v2.first;
                        const std::string & value = v2.second.data();                        
                        json.put(key,value);
                    }
                }
            }            
        }
        start = start.nextSibling();
    }
    if (group)
        return groupArray;
    pt::ptree empty;
    return empty;
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
    title = title + " * XML To JSON                                                       * \n";
    title = title + " * This tool converts an ODK XML data file into JSON.                * \n";
    title = title + " * JSON files are usually generated from 'formhubtojson' however     * \n";
    title = title + " * sometimes it is convenient to convert raw XML files. For example, * \n";
    title = title + " * in case that the data in Formshare get lost and only the XML      * \n";
    title = title + " * files from devices exists. It is also useful if you dont want     * \n";
    title = title + " * to use software like FormShare or FormHub.                        * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "2.0");

    TCLAP::ValueArg<std::string> xmlArg("i","xml","Input XML File",true,"","string");
    TCLAP::ValueArg<std::string> jsonArg("o","json","Input JSON File",false,"","string");
    TCLAP::ValueArg<std::string> formArg("x","xform","Input XML Form File",true,"","string");

    cmd.add(xmlArg);
    cmd.add(jsonArg);
    cmd.add(formArg);
    cmd.parse( argc, argv );

    bool withStdIn;
    withStdIn = false;
    QJsonObject extRoot;
    if (!isatty(fileno(stdin)))
    {
        QTextStream qtin(stdin);
        QString stdindata = qtin.readAll();
        QByteArray ba;
        ba = stdindata.toUtf8();
        QJsonDocument exJSONDoc(QJsonDocument::fromJson(ba));
        if (!exJSONDoc.isNull())
        {
            extRoot = exJSONDoc.object();
            withStdIn = true;
        }
        else
        {
            withStdIn = false;
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

    //Extract all repeats from manifest file
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

//    QJsonObject JSONRoot;
//    JSONRoot["_xform_id_string"] = root.attribute("id","");
//    processNode(repeatArray,false,root,JSONRoot);


    pt::ptree JSONRoot;
    JSONRoot.put("_xform_id_string",root.attribute("id","").toStdString());
    processNodeBoost(repeatArray,false,root,JSONRoot);


    if (withStdIn)
    {
        //Append any keys comming from stdin
        QStringList keys;
        keys = extRoot.keys();
        for (int n=0; n <= keys.count()-1; n++)
        {
            //JSONRoot[keys[n]] = extRoot.value(keys[n]).toString();
            JSONRoot.put(keys[n].toStdString(),extRoot.value(keys[n]).toString().toStdString());
        }
    }
    //QJsonDocument saveDoc(JSONRoot);

    //If no output file was given the print to stdout
    if (!jsonFile.isEmpty())
    {
        pt::write_json(jsonFile.toStdString(),JSONRoot);
//        QFile saveFile(jsonFile);
//        if (!saveFile.open(QIODevice::WriteOnly)) {
//            log("Couldn't open output JSON file.");
//            return 1;
//        }
//        saveFile.write(saveDoc.toJson());
//        saveFile.close();
    }
    else
    {
        QTextStream out(stdout);
        out << QString::fromStdString(JSONRoot.data());
        //out << saveDoc.toJson();
    }

    return 0;
}
