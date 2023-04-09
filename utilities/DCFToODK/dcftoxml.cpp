#include "dcftoxml.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

QDomElement root;
QDomElement dict;
QDomElement level;
QDomElement iditems;
QDomElement items;
QDomElement item;
QDomElement records;
QDomElement record;
QDomElement valueSet;
QDomElement valueSetValues;

bool inDict;
bool inLevel;
bool inIdItems;
bool inItem;
bool inRecord;
bool inValueSet;
bool createValues;
bool createItems;
bool createRecords;

void DCFToXML::appendValue(QDomElement parent, QString data)
{
    // This function appends the value of the value set.
    // It replaces the ; with |

    int pos;
    pos = data.indexOf("=");
    QString temp;
    temp = data.right(data.length()-pos-1);
    temp = temp.replace(";","|");

    QDomElement varname;
    QDomText varvalue;

    varname = this->xmlDoc.createElement("Value");
    parent.appendChild(varname);
    varvalue = this->xmlDoc.createTextNode(temp);
    varname.appendChild(varvalue);

}

void DCFToXML::appendVar(QDomElement parent, QString data)
{
    //This appends a variable to a parent
    //In CSPro varieables are separated by  =
    // We remove the = and the node name is the variable
    // while the value becomes the value of the node

    int pos;
    pos = data.indexOf("=");
    QString var;
    QString value;
    var = data.left(pos);
    value = data.right(data.length()-pos-1);

    QDomElement varname;
    QDomText varvalue;

    varname = this->xmlDoc.createElement(var);
    parent.appendChild(varname);
    varvalue = this->xmlDoc.createTextNode(value);
    varname.appendChild(varvalue);

}

void DCFToXML::addToXML(QString data)
{
    /*
        This fuction will parse the data from the CSPro
        into XML nodes. There are several controling bool
        variables to knows where in the CSPro data data is
        and how is its parent
    */
    bool section;
    section = false;

    QString currData;
    currData = data;

    if (data == "[Dictionary]")
    {
        section = true;
        inDict = true;
        inLevel = false;
        inIdItems = false;
        inItem = false;
        inRecord = false;
        inValueSet = false;
        createRecords = true;
        dict = this->xmlDoc.createElement("Dictionary");
        root.appendChild(dict);
    }
    if (data == "[Level]")
    {
        section = true;
        inDict = false;
        inLevel = true;
        inIdItems = false;
        inRecord = false;
        inItem = false;
        inValueSet = false;
        level = this->xmlDoc.createElement("Level");
        dict.appendChild(level);
    }
    if (data == "[IdItems]")
    {
        section = true;
        inDict = false;
        inLevel = false;
        inIdItems = true;
        inRecord = false;
        inItem = false;
        inValueSet = false;
        createItems = true;
        iditems = this->xmlDoc.createElement("IdItems");
        dict.appendChild(iditems);
    }
    if (data == "[Item]")
    {
        section = true;
        inDict = false;
        inLevel = false;
        inItem = true;
        inValueSet = false;
        if (createItems)
        {
            if (inIdItems)
            {
                items = this->xmlDoc.createElement("Items");
                iditems.appendChild(items);
            }
            if (inRecord)
            {
                items = this->xmlDoc.createElement("Items");
                record.appendChild(items);
            }
            createItems = false;
        }

        item = this->xmlDoc.createElement("Item");
        items.appendChild(item);
    }
    if (data == "[Record]")
    {
        section = true;
        inDict = false;
        inLevel = false;
        inIdItems = false;
        inItem = false;
        inRecord = true;
        inValueSet = false;
        createItems = true;
        if (createRecords)
        {
            records = this->xmlDoc.createElement("Records");
            dict.appendChild(records);
            createRecords = false;
        }
        record = this->xmlDoc.createElement("Record");
        records.appendChild(record);
    }
    if (data == "[ValueSet]")
    {
        section = true;
        inDict = false;
        inLevel = false;
        inItem = false;
        inValueSet = true;
        valueSet = this->xmlDoc.createElement("ValueSet");
        item.appendChild(valueSet);
        createValues = true;
    }
    if (section == false)
    {
        if (!inValueSet)
        {
            if (inDict)
            {
                appendVar(dict,data);
            }
            if (inLevel)
            {
                appendVar(level,data);
            }
            if ((inIdItems) && (!inItem))
            {
                appendVar(iditems,data);
            }
            if ((inIdItems) && (inItem))
            {
                appendVar(item,data);
            }
            if ((inRecord) && (!inItem))
            {
                appendVar(record,data);
            }
            if ((inRecord) && (inItem))
            {
                appendVar(item,data);
            }
        }
        else
        {
            if (!data.contains("Value="))
                appendVar(valueSet,data);
            else
            {
                if (createValues)
                {
                    valueSetValues = this->xmlDoc.createElement("ValueSetValues");
                    valueSet.appendChild(valueSetValues);
                    appendValue(valueSetValues,data);
                    createValues = false;
                }
                else
                    appendValue(valueSetValues,data);
            }
        }
    }
}

DCFToXML::DCFToXML()
{

}

void DCFToXML::convertToXML(QString dcfFile)
{
    this->xmlDoc = QDomDocument("CSProXMLFile");
    root = this->xmlDoc.createElement("CSProXML");
    root.setAttribute("version", "1.0");
    this->xmlDoc.appendChild(root);

    QFile file(dcfFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Cannot open DCF file";
        exit(1);
    }
    QTextStream in(&file);
    int pos;
    pos = 1;
    while (!in.atEnd())
    {
        QString line = in.readLine();
        if (!line.isEmpty())
            addToXML(line); //Parse the line
        pos++;
    }
}
