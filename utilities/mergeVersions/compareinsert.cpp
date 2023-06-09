/*
Merge Versions

Copyright (C) 2019 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com)

Merge Versions is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

Merge Versions is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with Merge Versions.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include "compareinsert.h"
#include <iostream>

compareInsert::compareInsert(QObject *parent) : QObject(parent)
{
    fatalError = false;
}

void compareInsert::setFiles(QString insertA, QString insertB, QString insertC, QString diffSQL, QString outputType, QList<TignoreTableValues> toIgnore)
{
    inputA = insertA;
    inputB = insertB;
    outputC = insertC;
    outputD = diffSQL;    
    this->outputType = outputType;
    valuesToIgnore = toIgnore;
}

QList<TtableDiff> compareInsert::getDiffs()
{
    QList<TtableDiff> res;
    res.append(diff);
    return res;
}

void compareInsert::addDiffToTable(QString table, QString sql)
{
    bool found;
    found = false;
    for (int pos = 0; pos < diff.count(); pos++)
    {
        if (diff[pos].table == table)
        {
            diff[pos].diff.append(sql);
            found = true;
            break;
        }
    }
    if (!found)
    {
        TtableDiff a_table;
        a_table.table = table;
        a_table.diff.append(sql);
        diff.append(a_table);
    }
}

void compareInsert::setAsParsed(QString table)
{
    for (int pos = 0; pos < diff.count(); pos++)
    {
        if (diff[pos].table == table)
        {
            diff[pos].parsed = true;
        }
    }
}

int compareInsert::createCFile()
{
    //Create the insert XML file. If exist it get overwriten
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
    {
        log("Error: Cannot create XML combined file");
        return 1;
    }
    return 0;
}

int compareInsert::createDiffFile()
{
    if (!fatalError)
    {
        if (QFile::exists(outputD))
            QFile::remove(outputD);
        QFile dfile(outputD);
        if (dfile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream outD(&dfile);
            outD.setCodec("UTF-8");
            for (int tpos = 0; tpos < diff.count();tpos++)
            {
                if (diff[tpos].parsed == false)
                {
                    diff[tpos].parsed = true;
                    for (int dpos = 0; dpos < diff[tpos].diff.count(); dpos++)
                    {
                        outD << diff[tpos].diff[dpos] + "\n";
                    }
                }
            }
            dfile.close();
            return 0;
        }
        else
        {
            log("Error: Cannot create Diff file");
            return 1;
        }
    }
    else
        return 1;
}

QList<TcompError> compareInsert::getErrorList()
{
    return errorList;
}

int compareInsert::compare()
{
    fatalError = false;
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
                compareLKPTables(rootA.firstChild(),docB);                
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
    if (!fatalError)
        return 0;
    else
        return 1;
}

//This logs messages to the terminal. We use printf because qDebug does not log in relase
void compareInsert::log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toUtf8().data());
}

void compareInsert::fatal(QString message)
{
    fatalError = true;
    fprintf(stderr, "\033[31m%s\033[0m \n", message.toUtf8().data());
}

QDomNode compareInsert::findTable(QDomDocument docB,QString tableName)
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

QDomNode compareInsert::findValue(QDomNode table,QString code)
{
    QDomNode node;
    node = table.firstChild();
    while (!node.isNull())
    {
        if (node.toElement().attribute("code","").toUpper() == code.toUpper())
            return node;
        node = node.nextSibling();
    }
    QDomNode null;
    return null;
}

void compareInsert::addValueToDiff(QDomElement table, QDomElement field)
{
    QString sql;
    sql = "INSERT INTO " + table.attribute("name","") + " (";
    sql = sql + table.attribute("clmcode","") + ",";
    sql = sql + table.attribute("clmdesc","") + ",";
    QStringList properties = table.attribute("properties","").split(",", Qt::SkipEmptyParts);
    if (properties.length() > 0)
    {
        for (int p=0; p < properties.length(); p++)
        {
            sql = sql + properties[p] + ",";
        }
    }
    sql = sql.left(sql.length()-1) + ") VALUES (";
    sql = sql + "\"" + field.attribute("code","").replace("\"","") + "\",";
    sql = sql + "\"" + field.attribute("description","").replace("\"","") + "\",";
    if (properties.length() > 0)
    {
        for (int p=0; p < properties.length(); p++)
        {
            sql = sql + "\"" + field.attribute(properties[p],"").replace("\"","") + "\",";
        }
    }
    sql = sql.left(sql.length()-1) + ");";
    addDiffToTable(table.attribute("name",""),sql);
}

void compareInsert::UpdateValue(QDomElement table, QDomElement field)
{
   QString sql;
   sql = "UPDATE " + table.attribute("name","") + " SET ";
   sql = sql + table.attribute("clmdesc","") + " = \"";
   sql = sql + field.attribute("description","") + "\" WHERE ";
   sql = sql + table.attribute("clmcode","") + " = '";
   sql = sql + field.attribute("code","") + "';";
   addDiffToTable(table.attribute("name",""),sql);
}

void compareInsert::addTableToDiff(QDomElement table)
{
    QDomNode field;
    field = table.firstChild();
    while (!field.isNull())
    {
        addValueToDiff(table,field.toElement());
        field = field.nextSibling();
    }
}

void compareInsert::changeValueInC(QDomNode table, QString code, QString newDescription)
{
    QDomNode field;
    field = table.firstChild();
    while (!field.isNull())
    {
        QDomElement efield;
        efield = field.toElement();
        if (efield.attribute("code","") == code)
        {
            efield.setAttribute("description",newDescription);
            return;
        }
        field = field.nextSibling();
    }
}

bool compareInsert::ignoreChange(QString table, QString value)
{
    for (int pos =0; pos < valuesToIgnore.count(); pos++)
    {
        if (valuesToIgnore[pos].table == table)
        {
            for (int pos2 =0; pos2 < valuesToIgnore[pos].values.count(); pos2++)
            {
                if (valuesToIgnore[pos].values[pos2] == value)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

void compareInsert::compareLKPTables(QDomNode table,QDomDocument &docB)
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
                    {
                        if (!ignoreChange(node.toElement().attribute("name",""),field.toElement().attribute("code","")))
                        {
                            if (outputType == "h")
                                fatal("VNS:Value " + field.toElement().attribute("code","") + " of lookup table " + node.toElement().attribute("name","") + " has changed from \"" + fieldFound.toElement().attribute("description","") + "\" to \"" + field.toElement().attribute("description","") + "\"");
                            else
                            {
                                TcompError error;
                                error.code = "VNS";
                                error.desc = "Value " + field.toElement().attribute("code","") + " of lookup table " + node.toElement().attribute("name","") + " from A not the same in B";
                                error.table = node.toElement().attribute("name","");
                                error.value = field.toElement().attribute("code","");
                                error.from = fieldFound.toElement().attribute("description","");
                                error.to = field.toElement().attribute("description","");
                                errorList.append(error);
                                fatalError = true;
                            }
                        }
                        else
                        {
                            UpdateValue(node.toElement(),field.toElement());
                            changeValueInC(tableFound,field.toElement().attribute("code",""),field.toElement().attribute("description",""));
                        }
                    }
                }
                else
                {
                    if (outputType == "h")
                        log("VNF:Value " + field.toElement().attribute("code","") + "(" + field.toElement().attribute("description","") + ") will be included in table " + node.toElement().attribute("name",""));
                    tableFound.appendChild(field.cloneNode(true));
                    addValueToDiff(node.toElement(),field.toElement());
                }
                field = field.nextSibling();
            }
        }
        else
        {
            if (outputType == "h")
                log("TNF:The lookup table " + node.toElement().attribute("name","") + " will be included in the database.");
            //Now adds the lookup table
            addTableToDiff(node.toElement());
            docB.documentElement().appendChild(node.cloneNode(true));
        }
        node = node.nextSibling();
    }
}
