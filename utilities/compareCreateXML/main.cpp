#include <tclap/CmdLine.h>
#include <QtCore>
#include <QDomElement>

//This logs messages to the terminal. We use printf because qDebug does not log in relase
void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf(temp.toUtf8().data());
}

void fatal(QString message)
{
    fprintf(stderr, "\033[31m%s\033[0m \n", message.toUtf8().data());
}

QDomNode findField(QDomNode table,QString field)
{
    QDomNode node;
    node = table.firstChild();
    while (!node.isNull())
    {
        if (node.toElement().attribute("name","") == field)
            return node;
        node = node.nextSibling();
    }
    QDomNode null;
    return null;
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

bool compareFields(QDomElement a, QDomElement b)
{
    bool same;
    same = true;
    if (a.attribute("key","") != b.attribute("key",""))
        same = false;
    if (a.attribute("type","") != b.attribute("type",""))
        same = false;
    if (a.attribute("size","") != b.attribute("size",""))
        same = false;
    if (a.attribute("decsize","") != b.attribute("decsize",""))
        same = false;
    if (a.attribute("rtable","") != b.attribute("rtable",""))
        same = false;
    if (a.attribute("rfield","") != b.attribute("rfield",""))
        same = false;

    return same;
}

void compareLKPTables(QDomNode table,QDomDocument &docB)
{
   QDomNode node;
   node = table;
   while (!node.isNull())
   {
       QDomNode tablefound;
       tablefound = findTable(docB,node.toElement().attribute("name",""));
       if (!tablefound.isNull())
       {
           QDomNode field = node.firstChild();
           while (!field.isNull())
           {
               QDomElement eField = field.toElement();
               QDomNode fieldFound = findField(tablefound,eField.attribute("name",""));
               if (!fieldFound.isNull())
               {
                   if (compareFields(eField,fieldFound.toElement()) == false)
                       fatal("FNS:Field " + eField.attribute("name","") + " in lookup table " + node.toElement().attribute("name","") + " from A is not the same in B");
               }
               else
               {
                   log("FNF:Field " + eField.attribute("name","") + " in lookup table " + node.toElement().attribute("name","") + " from A is not found in B");
               }
               field = field.nextSibling();
           }
       }
       else
       {
           log("TNF:Lookup table " + node.toElement().attribute("name","") + " from A not found in B");
           //Now adds the lookup table
           docB.documentElement().firstChild().appendChild(node.cloneNode(true));
       }
       node = node.nextSibling();
   }
}

void compareTables(QDomNode table,QDomDocument &docB)
{
    QDomElement eTable = table.toElement();
    QDomNode tablefound;
    tablefound = findTable(docB,eTable.toElement().attribute("name",""));
    if (!tablefound.isNull())
    {        
        if (table.parentNode().toElement().attribute("name","") ==
                tablefound.parentNode().toElement().attribute("name",""))
        {
            QDomNode field = table.firstChild();
            while (!field.isNull())
            {
                QDomElement eField = field.toElement();
                if (eField.tagName() == "table")
                    compareTables(field,docB);
                else
                {
                    QDomNode fieldFound = findField(tablefound,eField.attribute("name",""));
                    if (!fieldFound.isNull())
                    {
                        if (compareFields(eField,fieldFound.toElement()) == false)
                            fatal("FNS:Field " + eField.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " from A is not the same in B");
                    }
                    else
                    {
                        log("FNF:Field " + eField.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " from A is not found in B");
                        tablefound.insertBefore(eField.cloneNode(true),tablefound.firstChild());
                    }
                }
                field = field.nextSibling();
            }
        }
        else
        {
            fatal("TNS:Table " + eTable.toElement().attribute("name","") + " from A does not have the same parent in B");
        }
    }
    else
    {
        //Now adds the table to doc2
        QDomNode parentfound;
        parentfound = findTable(docB,table.parentNode().toElement().attribute("name",""));
        if (!parentfound.isNull())
        {
            log("TNF:Table " + eTable.toElement().attribute("name","") + " from A not found in B");
            parentfound.appendChild(table.cloneNode(true));
        }
        else
        {
            fatal("TNF:Table " + eTable.toElement().attribute("name","") + " from A not found in B. Its parent in A is not found in B");
        }
    }
}

int main(int argc, char *argv[])
{
    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * Compare Create XML 1.0                                            * \n";
    title = title + " * This tool compares two create XML files (A and B) for incremental * \n";
    title = title + " * changes.                                                          * \n";
    title = title + " *                                                                   * \n";
    title = title + " * The tool informs of tables and variables in A that are not in B.  * \n";
    title = title + " * The tool can also create a combined file C that appends           * \n";
    title = title + " * not found tables and variables with the following conditions:     * \n";
    title = title + " *   1) If a table in A is not found in B then it will be added to B * \n";
    title = title + " *      only if its parent exists in B.                              * \n";
    title = title + " *                                                                   * \n";
    title = title + " * C will have all of B plus all of A.                               * \n";
    title = title + " *                                                                   * \n";
    title = title + " * The tool WILL NOT fix the following:                              * \n";
    title = title + " *   1) Inconsistencies in field definition like size, type,         * \n";
    title = title + " *      parent table and parent field.                               * \n";
    title = title + " *   2) Tables that do not share the same parent.                    * \n";
    title = title + " *                                                                   * \n";
    title = title + " * Nomenclature:                                                     * \n";
    title = title + " *   TNF: Table not found.                                           * \n";
    title = title + " *   TNS: The table does not have the same parent table.             * \n";
    title = title + " *   FNF: Field not found.                                           * \n";
    title = title + " *   FNS: The field is not the same.                                 * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is usefull when dealing with multiple versions of an    * \n";
    title = title + " * ODK survey that must be combined in one common database.          * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is part of ODK Tools (c) ILRI-RMG, 2015                 * \n";
    title = title + " * Author: Carlos Quiros (c.f.quiros@cgiar.org / cquiros@qlands.com) * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.0");

    TCLAP::ValueArg<std::string> aArg("a","inputa","Input create XML file A",true,"","string");
    TCLAP::ValueArg<std::string> bArg("b","inputb","Input create XML file B",true,"","string");
    TCLAP::ValueArg<std::string> cArg("c","outputc","Output create XML file C",false,"./combined-create.xml","string");

    cmd.add(aArg);
    cmd.add(bArg);
    cmd.add(cArg);


    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    QString inputA = QString::fromUtf8(aArg.getValue().c_str());
    QString inputB = QString::fromUtf8(bArg.getValue().c_str());
    QString outputC = QString::fromUtf8(cArg.getValue().c_str());

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
            if ((rootA.tagName() == "XMLSchemaStructure") && (rootB.tagName() == "XMLSchemaStructure"))
            {
                //Comparing lookup tables
                if ((!rootA.firstChild().firstChild().isNull()) && (!rootB.firstChild().firstChild().isNull()))
                    qDebug() << "Comparing lookup tables";
                compareLKPTables(rootA.firstChild().firstChild(),docB);

                //Comparing tables
                if ((!rootA.firstChild().nextSibling().firstChild().isNull()) && (!rootB.firstChild().nextSibling().firstChild().isNull()))
                    qDebug() << "Comparing tables";
                compareTables(rootA.firstChild().nextSibling().firstChild(),docB);

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
                    log("Error: Cannot create XML combined file");

            }
            else
            {
                if (!(rootA.tagName() == "ODKImportXML"))
                {
                    log("Input document A is not a create XML file");
                    return 1;
                }
                if (!(rootB.tagName() == "ODKImportXML"))
                {
                    log("Input document B is not a create XML file");
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

    return 0;
}
