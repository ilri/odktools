#include "xmltoyml.h"
#include <QFileInfo>
#include <unistd.h>
#include <stdio.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <iostream>
#include <QDomNodeList>
#include <QProcess>
#include <QUuid>
#include <QDir>


namespace pt = boost::property_tree;

XMLToYML::XMLToYML(QObject *parent)
    : QObject{parent}
{

}

void XMLToYML::log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toUtf8().data());
}

void XMLToYML::setXMLDocument(QDomDocument doc)
{
    this->xmlDoc = doc;
}

void XMLToYML::addValueToList(QList<TlkpValue > &list, QString code, QString desc)
{
    bool found = false;
    for (int v=0; v < list.count(); v++)
    {
        if (list[v].code.simplified() == code.replace("'","").simplified())
            found = true;
    }
    if (!found)
    {
        TlkpValue new_value;
        new_value.code = code.replace("'","").simplified();
        new_value.desc = desc.simplified();
        list.append(new_value);
    }
}

QString XMLToYML::getListByLink(QString link)
{
    for (int l=0; l < item_link_list.count(); l++)
    {
        if (item_link_list[l].link == link)
            return item_link_list[l].code;
    }
    return "";
}


void XMLToYML::generateYML(QString file, QString mainRecord, QString tempDir)
{
    QUuid json_id=QUuid::createUuid();
    QString json_file=json_id.toString().replace("{","").replace("}","");
    json_file = json_file + ".json";

    QDir temp_dir(tempDir);
    json_file = temp_dir.canonicalPath() + temp_dir.separator() + json_file;

    pt::ptree JSONRoot;

    pt::ptree choices_sheet;
    pt::ptree survey_sheet;

    pt::ptree hash_object;
    hash_object.put("#","Converted by yxf. Edit the YAML file instead of the Excel file.");
    survey_sheet.push_back(std::make_pair("", hash_object));


    QDomNodeList tmp;
    tmp = this->xmlDoc.elementsByTagName("IdItems");
    QDomNodeList IdItems = tmp.item(0).toElement().elementsByTagName("Item");
    QString IdItemLabel = IdItems.item(0).firstChild().toElement().text();
    QString IdItemName = IdItems.item(0).firstChild().nextSibling().toElement().text();

    QDomNodeList records = this->xmlDoc.elementsByTagName("Record");
    for (int r=0; r < records.count(); r++)
    {
        QDomNodeList items = records.item(r).toElement().elementsByTagName("Item");
        //Ignore records without items
        if (items.count() > 0)
        {
            QString recordLabel = records.item(r).firstChild().toElement().text();
            QString recordName = records.item(r).firstChild().nextSibling().toElement().text();
            if (recordName == mainRecord)
            {
                pt::ptree beginGroupObject;
                beginGroupObject.put("type","begin group");
                beginGroupObject.put("name","grp_cover");
                beginGroupObject.put("label","Cover information");
                beginGroupObject.put("repeat_count","");
                beginGroupObject.put("appearance","field-list");
                survey_sheet.push_back(std::make_pair("", beginGroupObject));

                pt::ptree IdItemObject;
                IdItemObject.put("type","text");
                IdItemObject.put("name",IdItemName.toStdString());
                IdItemObject.put("label",IdItemLabel.toStdString());
                IdItemObject.put("repeat_count","");
                IdItemObject.put("appearance","");
                survey_sheet.push_back(std::make_pair("", IdItemObject));

                for (int i=0; i < items.count(); i++)
                {
                    QString itemLabel = items.item(i).firstChild().toElement().text();
                    QString itemName = items.item(i).firstChild().nextSibling().toElement().text();
                    pt::ptree ItemObject;
                    QString itemType = "integer";
                    if (items.item(i).toElement().elementsByTagName("Decimal").count() > 0)
                        itemType = "decimal";
                    if (items.item(i).toElement().elementsByTagName("DataType").count() > 0)
                        itemType = "text";
                    QDomNodeList itemValues = items.item(i).toElement().elementsByTagName("Value");
                    if (itemValues.count() > 0)
                    {
                        tmp = items.item(i).toElement().elementsByTagName("Link");
                        if (tmp.count() > 0)
                        {
                            TlkpLink a_link;
                            a_link.code = itemName;
                            a_link.link = tmp.item(0).toElement().text();
                            item_link_list.append(a_link);
                        }

                        itemType = "select_one " +  itemName;
                        QList<TlkpValue > item_value_list;
                        for (int v=0; v < itemValues.count(); v++)
                        {
                            QStringList parts = itemValues.item(v).toElement().text().split("|");
                            addValueToList(item_value_list,parts[0],parts[1]);
                        }
                        for (int v=0; v < item_value_list.count(); v++)
                        {
                            pt::ptree ValueObject;
                            ValueObject.put("list_name",itemName.toStdString());
                            ValueObject.put("name",item_value_list[v].code.toStdString());
                            ValueObject.put("label",item_value_list[v].desc.toStdString());
                            choices_sheet.push_back(std::make_pair("", ValueObject));
                        }
                    }
                    else
                    {
                        tmp = items.item(i).toElement().elementsByTagName("Link");
                        if (tmp.count() > 0)
                        {
                            QString link = tmp.item(0).toElement().text();
                            QString listCode = getListByLink(link);
                            if (listCode != "")
                            {
                                itemType = "select_one " +  listCode;
                            }
                        }
                    }

                    ItemObject.put("type",itemType.toStdString());
                    ItemObject.put("name",itemName.toStdString());
                    ItemObject.put("label",itemLabel.toStdString());
                    ItemObject.put("repeat_count","");
                    ItemObject.put("appearance","");
                    survey_sheet.push_back(std::make_pair("", ItemObject));

                }

                pt::ptree endGroupObject;
                endGroupObject.put("type","end group");
                endGroupObject.put("name","grp_cover");
                endGroupObject.put("label","");
                endGroupObject.put("repeat_count","");
                endGroupObject.put("appearance","");
                survey_sheet.push_back(std::make_pair("", endGroupObject));
            }
            else
            {
                pt::ptree beginRepeatObject;
                beginRepeatObject.put("type","begin repeat");
                beginRepeatObject.put("name",recordName.toStdString());
                beginRepeatObject.put("label",recordLabel.toStdString());
                beginRepeatObject.put("repeat_count","");
                beginRepeatObject.put("appearance","");
                survey_sheet.push_back(std::make_pair("", beginRepeatObject));

                pt::ptree beginGroupObject;
                beginGroupObject.put("type","begin group");
                beginGroupObject.put("name","grp_" + recordName.toStdString());
                beginGroupObject.put("label",recordLabel.toStdString());
                beginGroupObject.put("repeat_count","");
                beginGroupObject.put("appearance","field-list");
                survey_sheet.push_back(std::make_pair("", beginGroupObject));

                for (int i=0; i < items.count(); i++)
                {
                    QString itemLabel = items.item(i).firstChild().toElement().text();
                    QString itemName = items.item(i).firstChild().nextSibling().toElement().text();
                    pt::ptree ItemObject;
                    QString itemType = "integer";
                    if (items.item(i).toElement().elementsByTagName("Decimal").count() > 0)
                        itemType = "decimal";
                    if (items.item(i).toElement().elementsByTagName("DataType").count() > 0)
                        itemType = "text";
                    QDomNodeList itemValues = items.item(i).toElement().elementsByTagName("Value");
                    if (itemValues.count() > 0)
                    {
                        tmp = items.item(i).toElement().elementsByTagName("Link");
                        if (tmp.count() > 0)
                        {
                            TlkpLink a_link;
                            a_link.code = itemName;
                            a_link.link = tmp.item(0).toElement().text();
                            item_link_list.append(a_link);
                        }

                        itemType = "select_one " +  itemName;
                        QList<TlkpValue > item_value_list;
                        for (int v=0; v < itemValues.count(); v++)
                        {
                            QStringList parts = itemValues.item(v).toElement().text().split("|");
                            addValueToList(item_value_list,parts[0],parts[1]);
                        }
                        for (int v=0; v < item_value_list.count(); v++)
                        {
                            pt::ptree ValueObject;
                            ValueObject.put("list_name",itemName.toStdString());
                            ValueObject.put("name",item_value_list[v].code.toStdString());
                            ValueObject.put("label",item_value_list[v].desc.toStdString());
                            choices_sheet.push_back(std::make_pair("", ValueObject));
                        }
                    }
                    else
                    {
                        tmp = items.item(i).toElement().elementsByTagName("Link");
                        if (tmp.count() > 0)
                        {
                            QString link = tmp.item(0).toElement().text();
                            QString listCode = getListByLink(link);
                            if (listCode != "")
                            {
                                itemType = "select_one " +  listCode;
                            }
                        }
                    }

                    ItemObject.put("type",itemType.toStdString());
                    ItemObject.put("name",itemName.toStdString());
                    ItemObject.put("label",itemLabel.toStdString());
                    ItemObject.put("repeat_count","");
                    ItemObject.put("appearance","");
                    survey_sheet.push_back(std::make_pair("", ItemObject));

                }

                pt::ptree endGroupObject;
                endGroupObject.put("type","end group");
                endGroupObject.put("name","grp_" + recordName.toStdString());
                endGroupObject.put("label","");
                endGroupObject.put("repeat_count","");
                endGroupObject.put("appearance","");
                survey_sheet.push_back(std::make_pair("", endGroupObject));

                pt::ptree endRepeatObject;
                endRepeatObject.put("type","end repeat");
                endRepeatObject.put("name",recordName.toStdString());
                endRepeatObject.put("label","");
                endRepeatObject.put("repeat_count","");
                endRepeatObject.put("appearance","");
                survey_sheet.push_back(std::make_pair("", endRepeatObject));
            }
        }
    }

    JSONRoot.add_child("survey",survey_sheet);
    JSONRoot.add_child("choices",choices_sheet);

    //Settings sheet
    QFileInfo info(file);
    QString fileName = info.baseName();
    pt::ptree settings_sheet;
    pt::ptree settings_object;
    settings_object.put("form_title",fileName.toStdString());
    settings_object.put("form_id",fileName.remove(QRegExp("[^a-zA-Z\\d\\s]")).toStdString());
    settings_sheet.push_back(std::make_pair("", settings_object));
    JSONRoot.add_child("settings",settings_sheet);


    // Metadata
    pt::ptree yxf;
    pt::ptree headers;
    pt::ptree survey;
    pt::ptree choices;
    pt::ptree settings;

    pt::ptree type, name, label, list_name, hash_tag, form_title, form_id, repeat_count, appearance;
    type.put("","type");
    name.put("","name");
    label.put("","label");
    repeat_count.put("","repeat_count");
    appearance.put("","appearance");
    list_name.put("","list_name");
    hash_tag.put("","#");
    form_title.put("","form_title");
    form_id.put("","form_id");

    survey.push_back(std::make_pair("", hash_tag));
    survey.push_back(std::make_pair("", type));
    survey.push_back(std::make_pair("", name));
    survey.push_back(std::make_pair("", label));
    survey.push_back(std::make_pair("", repeat_count));
    survey.push_back(std::make_pair("", appearance));

    choices.push_back(std::make_pair("", list_name));
    choices.push_back(std::make_pair("", name));
    choices.push_back(std::make_pair("", label));

    settings.push_back(std::make_pair("", form_title));
    settings.push_back(std::make_pair("", form_id));

    headers.add_child("survey", survey);
    headers.add_child("choices", choices);
    headers.add_child("settings", settings);

    yxf.add_child("headers", headers);
    JSONRoot.add_child("yxf",yxf);


    pt::write_json(json_file.toStdString(),JSONRoot);

    QProcess *mySQLDumpProcess = new QProcess();
    QStringList arguments;
    arguments << "-p";
    arguments << "json";
    arguments << "-o";
    arguments << "yaml";
    arguments << json_file;
    mySQLDumpProcess->setStandardInputFile(QProcess::nullDevice());
    mySQLDumpProcess->setStandardOutputFile(file);

    mySQLDumpProcess->start("yq", arguments);
    mySQLDumpProcess->waitForFinished(-1);
    if ((mySQLDumpProcess->exitCode() > 0) || (mySQLDumpProcess->error() == QProcess::FailedToStart))
    {
        if (mySQLDumpProcess->error() == QProcess::FailedToStart)
        {
            log("Error: Command yq not found");
        }
        else
        {
            log("Running yq returned error");
            QString serror = mySQLDumpProcess->readAllStandardError();
            log(serror);
            log("Running paremeters:" + arguments.join(" "));
        }
        temp_dir.remove(json_file);
        delete mySQLDumpProcess;
        exit(1);
    }
    temp_dir.remove(json_file);
    delete mySQLDumpProcess;
}
