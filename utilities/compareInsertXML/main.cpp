/*
CompareInsertXML

Copyright (C) 2015-2017 International Livestock Research Institute.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

CompareInsertXML is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

CompareInsertXML is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with CompareInsertXML.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <tclap/CmdLine.h>
#include <QtCore>
#include <QDomElement>

QString outputType;
bool fatalError;
QStringList diff;

struct compError
{
  QString code;
  QString desc;
  QString table;
  QString value;
  QString from;
  QString to;
};
typedef compError TcompError;

QList<TcompError> errorList;

//This logs messages to the terminal. We use printf because qDebug does not log in relase
void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toLocal8Bit().data());
}

void fatal(QString message)
{
    fatalError = true;
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

void addValueToDiff(QDomElement table, QDomElement field)
{
    QString sql;
    sql = "INSERT INTO " + table.attribute("name","") + " (";
    sql = sql + table.attribute("clmcode","") + ",";
    sql = sql + table.attribute("clmdesc","") + ") VALUES ('";
    sql = sql + field.attribute("code","") + "','";
    sql = sql + field.attribute("description","") + "');";
    diff.append(sql);
}

void UpdateValue(QDomElement table, QDomElement field)
{
   QString sql;
   sql = "UPDATE " + table.attribute("name","") + " SET ";
   sql = sql + table.attribute("clmdesc","") + " = '";
   sql = sql + field.attribute("description","") + "' WHERE ";
   sql = sql + table.attribute("clmcode","") + " = '";
   sql = sql + field.attribute("code","") + "';";
   diff.append(sql);

}

void addTableToDiff(QDomElement table)
{
    QDomNode field;
    field = table.firstChild();
    while (!field.isNull())
    {
        addValueToDiff(table,field.toElement());
        field = field.nextSibling();
    }
}

void changeValueInC(QDomNode table, QString code, QString newDescription)
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
                    {
                        if (outputType == "h")
                        {
                            fatal("VNS:Value " + field.toElement().attribute("code","") + " of lookup table " + node.toElement().attribute("name","") + " has changed from \"" + fieldFound.toElement().attribute("description","") + "\" to \"" + field.toElement().attribute("description","") + "\"");
                            log("Do you want to change this value in the database? Y/N");
                            std::string line;
                            std::getline(std::cin, line);
                            QString result = QString::fromStdString(line);
                            if (std::cin.eof() || result.toLower() == "y")
                            {
                                fatalError = false;
                                UpdateValue(node.toElement(),field.toElement());
                                changeValueInC(tableFound,field.toElement().attribute("code",""),field.toElement().attribute("description",""));
                            }
                            else
                            {
                                fatalError = true;
                            }
                        }
                        {
                            TcompError error;
                            error.code = "VNS";
                            error.desc = "Value " + field.toElement().attribute("code","") + " of lookup table " + node.toElement().attribute("name","") + " from A not the same in B";
                            error.table = node.toElement().attribute("name","");
                            error.value = field.toElement().attribute("code","");
                            error.from = fieldFound.toElement().attribute("description","");
                            error.to = field.toElement().attribute("description","");
                            errorList.append(error);
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

int main(int argc, char *argv[])
{
    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * Compare Insert XML                                                * \n";
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
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.1");

    TCLAP::ValueArg<std::string> aArg("a","inputa","Input insert XML file A",true,"","string");
    TCLAP::ValueArg<std::string> bArg("b","inputb","Input insert XML file B",true,"","string");
    TCLAP::ValueArg<std::string> cArg("c","outputc","Output insert XML file C",false,"./combined-insert.xml","string");
    TCLAP::ValueArg<std::string> dArg("d","diff","Output diff SQL script",false,"./diff.sql","string");
    TCLAP::ValueArg<std::string> oArg("o","outputype","Output type: (h)uman readble or (m)achine readble",false,"m","string");

    cmd.add(aArg);
    cmd.add(bArg);
    cmd.add(cArg);
    cmd.add(dArg);
    cmd.add(oArg);


    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    QString inputA = QString::fromUtf8(aArg.getValue().c_str());
    QString inputB = QString::fromUtf8(bArg.getValue().c_str());
    QString outputC = QString::fromUtf8(cArg.getValue().c_str());
    QString outputD = QString::fromUtf8(dArg.getValue().c_str());
    outputType = QString::fromUtf8(oArg.getValue().c_str());

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
                compareLKPTables(rootA.firstChild(),docB);

                if (outputType == "m")
                {
                    QDomDocument XMLResult;
                    XMLResult = QDomDocument("XMLResult");
                    QDomElement XMLRoot;
                    XMLRoot = XMLResult.createElement("XMLResult");
                    XMLResult.appendChild(XMLRoot);
                    QDomElement eErrors;
                    eErrors = XMLResult.createElement("errors");
                    XMLRoot.appendChild(eErrors);
                    if (errorList.count() > 0)
                        fatalError = true;
                    for (int pos = 0; pos <= errorList.count()-1; pos++)
                    {
                        QDomElement anError;
                        anError = XMLResult.createElement("error");
                        anError.setAttribute("table",errorList[pos].table);
                        anError.setAttribute("value",errorList[pos].value);
                        anError.setAttribute("code",errorList[pos].code);
                        anError.setAttribute("desc",errorList[pos].desc);
                        anError.setAttribute("from",errorList[pos].from);
                        anError.setAttribute("to",errorList[pos].to);
                        eErrors.appendChild(anError);
                    }
                    log(XMLResult.toString());
                }


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
                {
                    log("Error: Cannot create XML combined file");
                    return 1;
                }

                if (QFile::exists(outputD))
                    QFile::remove(outputD);
                QFile dfile(outputD);
                if (dfile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QTextStream outD(&dfile);
                    outD.setCodec("UTF-8");
                    for (int dpos = 0; dpos < diff.count();dpos++)
                    {
                        outD << diff[dpos] + "\n";
                    }
                    file.close();
                }
                else
                {
                    log("Error: Cannot create Diff file");
                    return 1;
                }
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
