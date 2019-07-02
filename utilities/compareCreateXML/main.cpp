/*
CompareCreateXML

Copyright (C) 2015-2017 International Livestock Research Institute.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

CompareCreateXML is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

CompareCreateXML is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with CompareCreateXML.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <tclap/CmdLine.h>
#include <QtCore>
#include <QDomElement>
#include <QList>
#include <QUuid>

bool fatalError;
QStringList diff;
QString outputType;
int idx;

struct compError
{
  QString code;
  QString desc;
  QString table;
  QString field;
  QString from;
  QString to;
};
typedef compError TcompError;

QList<TcompError> errorList;

struct rfieldDef
{
  QString name;
  QString rname;
};
typedef rfieldDef TrfieldDef;

struct rtableDef
{
  QString parentTable;
  QString name;
  QList<TrfieldDef> rfields;
};
typedef rtableDef TrtableDef;

QList<TrtableDef> rtables;


void addAlterFieldToDiff(QString table, QDomElement eField, int newSize, int newDec)
{
    QString sql;
    QString fieldName;
    fieldName = eField.attribute("name","");
    if (eField.attribute("type","") == "varchar")
    {
        sql = "ALTER TABLE " + table + " MODIFY " + fieldName + " varchar(" + QString::number(newSize) + ");\n";
        diff.append(sql);
    }
    if (eField.attribute("type","") == "int")
    {
        sql = "ALTER TABLE " + table + " MODIFY " + fieldName + " int(" + QString::number(newSize) + ");\n";
        diff.append(sql);
    }
    if (eField.attribute("type","") == "decimal")
    {
        sql = "ALTER TABLE " + table + " MODIFY " + fieldName + " decimal(" + QString::number(newSize) + "," + QString::number(newDec) + ");\n";
        diff.append(sql);
    }
}

//This adds the modifications to a table to the diff list
void addFieldToDiff(QString table, QDomElement eField)
{
    QString sql;
    sql = "ALTER TABLE " + table + " ADD COLUMN " + eField.attribute("name","");
    if (eField.attribute("type","") == "decimal")
        sql = sql + " " + eField.attribute("type","") + " (" + eField.attribute("size","0") + "," + eField.attribute("decsize","0") + ");\n";
    else
    {
        if (eField.attribute("type","") != "text")
            sql = sql + " " + eField.attribute("type","") + " (" + eField.attribute("size","0") + ");\n";
        else
            sql = sql + " " + eField.attribute("type","") + ";\n";
    }
    diff.append(sql);
    if (eField.attribute("rtable","") != "")
    {
        QUuid recordUUID=QUuid::createUuid();
        QString strRecordUUID=recordUUID.toString().replace("{","").replace("}","").right(12);

        sql = "ALTER TABLE " + table + " ADD INDEX DIDX" + strRecordUUID  + " (" + eField.attribute("name","") + ");\n";
        diff.append(sql);
        sql = "ALTER TABLE " + table + " ADD CONSTRAINT DFK" + strRecordUUID + " FOREIGN KEY (";
        sql = sql + eField.attribute("name","") + ") REFERENCES " + eField.attribute("rtable","") + "(";
        sql = sql + eField.attribute("rfield","") + ") ON DELETE RESTRICT ON UPDATE NO ACTION;\n" ;
        diff.append(sql);
        idx++;
    }
}

void addFieldToRTables(QString parentTable, QString rTable, QString field, QString rField)
{
    int tidx;
    tidx = -1;
    int pos;
    for (pos = 0; pos < rtables.count(); pos++)
    {
        if ((rtables[pos].parentTable == parentTable) && (rtables[pos].name == rTable))
            tidx = pos;
    }
    if (tidx != -1)
    {
        TrfieldDef aField;
        aField.name = field;
        aField.rname = rField;
        rtables[tidx].rfields.append(aField);
    }
    else
    {
        TrtableDef table;
        table.parentTable = parentTable;
        table.name = rTable;
        TrfieldDef aField;
        aField.name = field;
        aField.rname = rField;
        table.rfields.append(aField);
        rtables.append(table);
    }
}

//This adds a table to the diff list
void addTableToSDiff(QDomNode table, bool lookUp)
{
    QUuid recordUUID=QUuid::createUuid();
    QString strRecordUUID=recordUUID.toString().replace("{","").replace("}","").right(12);

    QDomElement eTable = table.toElement();
    QString sql;
    sql = "CREATE TABLE IF NOT EXISTS " + eTable.attribute("name","") + "(\n";
    QDomNode node = table.firstChild();
    QStringList keys;
    QList<QDomNode> childTables;
    while (!node.isNull())
    {
        QDomElement field = node.toElement();
        if (field.tagName() == "field")
        {
            sql = sql + field.attribute("name","");

            if (field.attribute("type","") == "decimal")
                sql = sql + " " + field.attribute("type","") + " (" + field.attribute("size","0") + "," + field.attribute("decsize","0") + ")";
            else
            {
                if (field.attribute("type","") != "text")
                    sql = sql + " " + field.attribute("type","") + " (" + field.attribute("size","0") + ")";
                else
                    sql = sql + " " + field.attribute("type","");
            }
            if (field.attribute("key","") == "true")
            {
                sql = sql + "NOT NULL COMMENT \"" + field.attribute("desc","Without description") + "\",\n";
                keys.append(field.attribute("name",""));
            }
            else
                sql = sql + " COMMENT \"" + field.attribute("desc","Without description") + "\",\n";
            if (field.attribute("rtable","") != "")
            {
                addFieldToRTables(eTable.attribute("name",""),field.attribute("rtable",""),field.attribute("name",""),field.attribute("rfield",""));
            }
        }
        else
        {
            childTables.append(node);
        }
        node = node.nextSibling();
    }
    //sql = sql + "rowuuid varchar(80) COMMENT \"Unique Row Identifier (UUID)\",\n";
    int pos;
    int pos2;
    //Add the keys
    sql = sql + "PRIMARY KEY (";
    for (pos = 0; pos < keys.count();pos++)
    {
        sql = sql + keys[pos] + ",";
    }
    bool hasRelatedTables;
    hasRelatedTables = false;
    for (pos = 0; pos < rtables.count();pos++)
    {
        if (rtables[pos].parentTable == eTable.attribute("name",""))
        {
            hasRelatedTables = true;
            break;
        }
    }
    if (hasRelatedTables)
        sql = sql.left(sql.length()-1) + "),\n";
    else
        sql = sql.left(sql.length()-1) + ")";
    //Add the indexes
    for (pos = 0; pos < rtables.count();pos++)
    {
        if (rtables[pos].parentTable == eTable.attribute("name",""))
        {
            QUuid idxUUID=QUuid::createUuid();
            QString strIdxUUID=idxUUID.toString().replace("{","").replace("}","").right(12);

            sql = sql + "INDEX DIDX"  + strIdxUUID + "(";
            for (pos2 = 0; pos2 < rtables[pos].rfields.count();pos2++)
            {
                sql = sql + rtables[pos].rfields[pos2].name + ",";
            }
            sql = sql.left(sql.length()-1) + "),\n";
            idx++;
        }
    }
    //Add foreign keys
    for (pos = 0; pos < rtables.count();pos++)
    {
        if (rtables[pos].parentTable == eTable.attribute("name",""))
        {
            QUuid cntUUID=QUuid::createUuid();
            QString strCntUUID=cntUUID.toString().replace("{","").replace("}","").right(12);

            sql = sql + "CONSTRAINT DFK"  + strCntUUID + "\n";
            sql = sql + "FOREIGN KEY (";
            for (pos2 = 0; pos2 < rtables[pos].rfields.count();pos2++)
            {
                sql = sql + rtables[pos].rfields[pos2].name + ",";
            }
            sql = sql.left(sql.length()-1) + ")\n";
            sql = sql + "REFERENCES " + rtables[pos].name + "(";
            for (pos2 = 0; pos2 < rtables[pos].rfields.count();pos2++)
            {
                sql = sql + rtables[pos].rfields[pos2].rname + ",";
            }
            sql = sql.left(sql.length()-1) + ")\nON DELETE RESTRICT\nON UPDATE NO ACTION,\n";
            idx++;
        }
    }
    if (hasRelatedTables)
        sql = sql.left(sql.length()-2);
    sql = sql + ")\n ENGINE = InnoDB CHARSET=utf8 COMMENT = \"" + eTable.attribute("desc","") + "\";\n";
    sql = sql + "CREATE UNIQUE INDEX DXROWUUID" + strRecordUUID + " ON " + eTable.attribute("name","") + "(rowuuid);\n";
    if (lookUp)
        sql = sql + "CREATE TRIGGER uudi_" + eTable.attribute("name","") + " BEFORE INSERT ON " + eTable.attribute("name","") + " FOR EACH ROW SET new.rowuuid = uuid();\n\n";
    else
        sql = sql + "\n";
    diff.append(sql);
    for (int pos = 0; pos <= childTables.count()-1;pos++)
        addTableToSDiff(childTables[pos],lookUp);
}

//This logs messages to the terminal. We use printf because qDebug does not log in relase
void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toLocal8Bit().data());
}

void fatal(QString message)
{
    fprintf(stderr, "\033[31m%s\033[0m \n", message.toUtf8().data());
    fatalError = true;
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

bool compareFields(QDomElement a, QDomElement b, bool &increased, int &newSize, int &newDec, bool &critical)
{
    bool same;
    increased = false;
    newSize = a.attribute("size","0").toInt();
    newDec = a.attribute("decsize","0").toInt();
    same = true;
    critical = true;
    bool isKey;
    isKey = false;
    if (a.attribute("key","false") == "true")
        isKey = true;

    if (a.attribute("key","") != b.attribute("key",""))
        return false;

    if (a.attribute("rtable","") != b.attribute("rtable",""))
        return false;
    if (a.attribute("rfield","") != b.attribute("rfield",""))
        return false;

    if (a.attribute("type","") != b.attribute("type",""))
        return false;

    if (a.attribute("size","") != b.attribute("size",""))
    {
        critical = false;
        if (a.attribute("size","0").toInt() >= b.attribute("size","0").toInt())
        {
            if (a.attribute("type","") == "decimal")
            {
                if (a.attribute("decsize","0").toInt() >= b.attribute("decsize","0").toInt())
                {
                    if ((a.attribute("size","0").toInt() - a.attribute("decsize","0").toInt()) >= (b.attribute("size","0").toInt() - b.attribute("decsize","0").toInt()))
                    {
                        increased = true;
                        newSize = a.attribute("size","0").toInt();
                        newDec = a.attribute("decsize","0").toInt();
                        if (isKey == false)
                            same = true;
                        else
                            same = false;
                    }
                    else
                    {
                        same = false;
                    }
                }
                else
                {
                    same = false;
                }
            }
            else
            {
                increased = true;
                newSize = a.attribute("size","0").toInt();
                if (isKey == false)
                    same = true;
                else
                    same = false;
            }

        }
        else
            same = false;
    }
    if (a.attribute("decsize","") != b.attribute("decsize",""))
    {
        critical = false;
        if (a.attribute("decsize","0").toInt() >= b.attribute("decsize","0").toInt())
        {
            if ((a.attribute("size","0").toInt() - a.attribute("decsize","0").toInt()) >= (b.attribute("size","0").toInt() - b.attribute("decsize","0").toInt()))
            {
                increased = true;
                newSize = a.attribute("size","0").toInt();
                newDec = a.attribute("decsize","0").toInt();
                if (isKey == false)
                    same = true;
                else
                    same = false;
            }
            else
                same = false;
        }
        else
            same = false;
    }

    return same;
}

QString getFieldDefinition(QDomElement field)
{
    QString result;
    result = "[Key=" + field.attribute("key","false") + ",";
    result = result + "Type=" + field.attribute("type","") + ",";
    result = result + "Size=" + field.attribute("size","0") + ",";
    result = result + "DecimalSize=" + field.attribute("decsize","0") + ",";
    result = result + "RelatedTable=" + field.attribute("rtable","None") + ",";
    result = result + "RelatedField=" + field.attribute("rfield","None") + "]";
    return result;
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
                   bool increased;
                   int newSize;
                   int newDec;
                   bool critical;
                   if (compareFields(eField,fieldFound.toElement(),increased,newSize,newDec,critical) == false)
                   {
                       if (outputType == "h")
                       {
                           if (critical)
                               fatal("FNS:Field " + eField.attribute("name","") + " in lookup table " + node.toElement().attribute("name","") + " from A is not the same in B. You need to fix it in the XLSX file");
                       }
                           else
                       {
                           if (critical)
                           {
                               TcompError error;
                               error.code = "FNS";
                               error.table = node.toElement().attribute("name","");
                               error.field = eField.attribute("name","");
                               error.desc = "Field " + eField.attribute("name","") + " in lookup table " + node.toElement().attribute("name","") + " from A is not the same in B";
                               error.from = getFieldDefinition(fieldFound.toElement());
                               error.to = getFieldDefinition(eField);
                               errorList.append(error);
                           }
                       }
                   }
                   else
                   {
                       if (increased)
                       {
                           if (outputType == "h")
                           {
                               if (eField.attribute("type","") != "decimal")
                                   log("FIC: Field " + eField.attribute("name","") + " in lookup table " + node.toElement().attribute("name","") + " will be increased from " + fieldFound.toElement().attribute("size",0) + " to " + eField.toElement().attribute("size","0"));
                               else
                                   log("FIC: Field " + eField.attribute("name","") + " in lookup table " + node.toElement().attribute("name","") + " will be increased from " + fieldFound.toElement().attribute("size",0) + " to " + eField.toElement().attribute("size","0") + ". Decimal size from " + fieldFound.toElement().attribute("decsize",0) + " to " + eField.toElement().attribute("decsize","0"));
                           }
                           addAlterFieldToDiff(node.toElement().attribute("name",""),eField,newSize,newDec);
                       }
                   }
               }
               else
               {
                   if (outputType == "h")
                       log("FNF:Field " + eField.attribute("name","") + " in lookup table " + node.toElement().attribute("name","") + " from A is not found in B");
                   addFieldToDiff(node.toElement().attribute("name",""),eField);
               }
               field = field.nextSibling();
           }
       }
       else
       {
           if (outputType == "h")
               log("TNF:Lookup table " + node.toElement().attribute("name","") + " from A not found in B");
           addTableToSDiff(node,true);
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
    //log(eTable.toElement().attribute("name",""));
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
                        bool increased;
                        int newSize;
                        int newDec;
                        bool critical;
                        if (compareFields(eField,fieldFound.toElement(),increased,newSize,newDec,critical) == false)
                        {
                            if (outputType == "h")
                            {
                                if (critical)
                                    fatal("FNS:Field " + eField.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " from A is not the same in B. You need to fix it in the XLSX file");
                            }
                            else
                            {
                                if (critical)
                                {
                                    TcompError error;
                                    error.code = "FNS";
                                    error.table = eTable.toElement().attribute("name","");
                                    error.field = eField.attribute("name","");
                                    error.desc = "Field " + eField.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " from A is not the same in B";
                                    error.from = getFieldDefinition(fieldFound.toElement());
                                    error.to = getFieldDefinition(eField);
                                    errorList.append(error);
                                }
                            }
                        }
                        else
                        {
                            if (increased)
                            {
                                if (outputType == "h")
                                {
                                    if (eField.attribute("type","") != "decimal")
                                        log("FIC: Field " + eField.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " will be increased from " + fieldFound.toElement().attribute("size",0) + " to " + eField.toElement().attribute("size","0"));
                                    else
                                        log("FIC: Field " + eField.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " will be increased from " + fieldFound.toElement().attribute("size",0) + " to " + eField.toElement().attribute("size","0") + ". Decimal size from " + fieldFound.toElement().attribute("decsize",0) + " to " + eField.toElement().attribute("decsize","0"));
                                }
                                addAlterFieldToDiff(eTable.attribute("name",""),eField,newSize,newDec);
                            }
                        }
                    }
                    else
                    {
                        if (outputType == "h")
                            log("FNF:Field " + eField.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " from A is not found in B");
                        addFieldToDiff( eTable.toElement().attribute("name",""),eField);
                        tablefound.insertBefore(eField.cloneNode(true),tablefound.firstChild());
                    }
                }
                field = field.nextSibling();
            }
        }
        else
        {
            if (outputType == "h")
                fatal("TNS:Table " + eTable.toElement().attribute("name","") + " from A does not have the same parent in B");
            else
            {
                TcompError error;
                error.code = "TNS";
                error.table = "NA";
                error.field = "NA";
                error.desc = "Table " + eTable.toElement().attribute("name","") + " from A does not have the same parent in B";
                error.from = tablefound.parentNode().toElement().attribute("name","");
                error.to = table.parentNode().toElement().attribute("name","");
                errorList.append(error);
            }
        }
    }
    else
    {
        //Now adds the table to doc2
        QDomNode parentfound;
        QString parentTableName;
        parentTableName = table.parentNode().toElement().attribute("name","");
        parentfound = findTable(docB,parentTableName);
        if (!parentfound.isNull())
        {
            if (outputType == "h")
                log("TNF:Table " + eTable.toElement().attribute("name","") + " from A not found in B");
            addTableToSDiff(eTable,false);
            parentfound.appendChild(table.cloneNode(true));
        }
        else
        {
            if (outputType == "h")
                fatal("TNF:Table " + eTable.toElement().attribute("name","") + " from A not found in B. Its parent in A is not found in B");
            else
            {
                TcompError error;
                error.code = "TNF";
                error.table = "NA";
                error.field = "NA";
                error.desc = "Table " + eTable.toElement().attribute("name","") + " from A not found in B. Its parent in A is not found in B";
                error.from = "NULL";
                error.to = parentTableName;
                errorList.append(error);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * Compare Create XML                                            * \n";
    title = title + " * This tool compares two create XML files (A and B) for incremental * \n";
    title = title + " * changes. A is consider an incremental version of B .              * \n";
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
    title = title + " * If a TNS nor a FNS are NOT encounter then this means that B can   * \n";
    title = title + " * merge into A and a diff SQL script is issued.                     * \n";
    title = title + " *                                                                   * \n";
    title = title + " * Decrimental changes are not taken into account because this means * \n";
    title = title + " * losing data between versions.                                     * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.1");

    TCLAP::ValueArg<std::string> aArg("a","inputa","Input create XML file A (later)",true,"","string");
    TCLAP::ValueArg<std::string> bArg("b","inputb","Input create XML file B (former)",true,"","string");
    TCLAP::ValueArg<std::string> cArg("c","outputc","Output create XML file C",false,"./combined-create.xml","string");
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
    idx = 1;

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
                compareLKPTables(rootA.firstChild().firstChild(),docB);
                //Comparing tables
                compareTables(rootA.firstChild().nextSibling().firstChild(),docB);

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
                        anError.setAttribute("field",errorList[pos].field);
                        anError.setAttribute("code",errorList[pos].code);
                        anError.setAttribute("desc",errorList[pos].desc);
                        anError.setAttribute("from",errorList[pos].from);
                        anError.setAttribute("to",errorList[pos].to);
                        eErrors.appendChild(anError);
                    }
                    log(XMLResult.toString());
                }

                //Do not create C and the Diff SQL if the merge cannot be done
                if (fatalError == false)
                {
                    //Create the output file. If exist it get overwriten
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
                            outD << diff[dpos];
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
                    return 1;
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
    if (!fatalError)
        return 0;
    else
        return 1;
}
