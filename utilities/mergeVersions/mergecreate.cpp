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

#include "mergecreate.h"
#include <QDomNodeList>

mergeCreate::mergeCreate(QObject *parent) : QObject(parent)
{
    fatalError = false;
    idx = 1;
    create_lookup_rels.clear();
}

void mergeCreate::setInsertDiff(QList<TtableDiff> diff)
{
    insert_diff.append(diff);
}

QStringList mergeCreate::getInsertTablesUsed()
{
    return insertTablesUsed;
}

int mergeCreate::compare()
{
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

            rootA = docA.documentElement();
            rootB = docB.documentElement();
            if ((rootA.tagName() == "XMLSchemaStructure") && (rootB.tagName() == "XMLSchemaStructure"))
            {
                //Comparing lookup tables
                compareLKPTables(rootA.firstChild().firstChild(),docB);
                //Comparing tables
                compareTables(rootA.firstChild().nextSibling().firstChild(),docB);
                // Add foreign keys that were dropped earlier
                diff.append("\n");
                for (int pos = 0; pos < create_lookup_rels.count(); pos++)
                {                                            
                    diff.append("ALTER TABLE " + create_lookup_rels[pos].table_name + " ADD INDEX " + create_lookup_rels[pos].rel_name + " (" + create_lookup_rels[pos].field_name + ");\n");
                    diff.append("ALTER TABLE " + create_lookup_rels[pos].table_name + " ADD CONSTRAINT " + create_lookup_rels[pos].rel_name + " FOREIGN KEY (" + create_lookup_rels[pos].field_name + ") REFERENCES " + create_lookup_rels[pos].rel_table + " (" + create_lookup_rels[pos].rel_field + ") ON DELETE RESTRICT  ON UPDATE NO ACTION;\n\n");
                }

                //Process drops
                QDomNode lkpTables = rootB.firstChild();
                for (int pos = 0; pos < dropTables.count(); pos++)
                {
                    diff.append("DROP TABLE IF EXISTS " + dropTables[pos] + ";\n\n");
                    QDomNode a_table = rootB.firstChild().firstChild();
                    while (!a_table.isNull())
                    {
                        if (a_table.toElement().attribute("name") == dropTables[pos])
                            break;
                        a_table = a_table.nextSibling();
                    }
                    lkpTables.removeChild(a_table);
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

void mergeCreate::setFiles(QString createA, QString createB, QString createC, QString diffSQL, QString outputType)
{
    inputA = createA;
    inputB = createB;
    outputC = createC;
    outputD = diffSQL;
    this->outputType = outputType;
}

void mergeCreate::replace_lookup_relationships(QString table, QString field)
{
    QStringList affected_tables;


    QDomNodeList fields;
    fields = rootB.elementsByTagName("field");
    QString sql;
    for (int pos = 0; pos < fields.count(); pos++)
    {
        if ((fields.item(pos).toElement().attribute("rtable","") == table) && ((fields.item(pos).toElement().attribute("rfield","") == field)))
        {
            QString affected_table = fields.item(pos).parentNode().toElement().attribute("name");
            if (affected_tables.indexOf(affected_table) == -1)
                affected_tables.append(affected_table);

            dropped_rels.append(fields.item(pos).toElement().attribute("rname",""));
            sql = "ALTER TABLE " + fields.item(pos).parentNode().toElement().attribute("name") + " DROP FOREIGN KEY " + fields.item(pos).toElement().attribute("rname","") + ";\n";
            diff.append(sql);
            sql = "ALTER TABLE " + fields.item(pos).parentNode().toElement().attribute("name") + " DROP INDEX " + fields.item(pos).toElement().attribute("rname","") + ";\n";
            diff.append(sql);
        }
    }
    fields = rootA.elementsByTagName("field");
    for (int pos = 0; pos < fields.count(); pos++)
    {
        if ((fields.item(pos).toElement().attribute("rtable","") == table) && ((fields.item(pos).toElement().attribute("rfield","") == field)))
        {
            QString affected_table = fields.item(pos).parentNode().toElement().attribute("name");
            if (affected_tables.indexOf(affected_table) >= 0)
            {
                TreplaceRef a_replace;
                a_replace.table_name = fields.item(pos).parentNode().toElement().attribute("name");
                a_replace.rel_name = fields.item(pos).toElement().attribute("rname","");
                a_replace.field_name = fields.item(pos).toElement().attribute("name","");
                a_replace.rel_table = fields.item(pos).toElement().attribute("rtable","");
                a_replace.rel_field = fields.item(pos).toElement().attribute("rfield","");
                create_lookup_rels.append(a_replace);
            }
        }
    }
}

bool mergeCreate::relation_is_dropped(QString name)
{
    for (int pos = 0; pos < dropped_rels.count() ; pos++)
    {
        if (dropped_rels[pos] == name)
        {
            return true;
        }
    }
    return false;
}

void mergeCreate::addAlterFieldToDiff(QString table, QDomElement eField, int newSize, int newDec, bool islookup)
{
    QString sql;
    QString fieldName;
    fieldName = eField.attribute("name","");

    if (islookup && (eField.attribute("key","false") == "true"))
    {
        replace_lookup_relationships(table,fieldName);
    }

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

void mergeCreate::ddTableToDrop(QString name)
{
    bool found = false;
    if (dropTables.indexOf(name) >= 0)
        found = true;
    if (!found)
    {
        dropTables.append(name);
    }
}

void mergeCreate::changeLookupRelationship(QString table, QDomElement a, QDomElement b, bool islookup)
{
    QString sql;
    QString oldcntname;
    QString newcntname;
    oldcntname = b.attribute("rname");
    newcntname = a.attribute("rname");
    bool create_new_rel;
    create_new_rel = false;
    if (!relation_is_dropped(oldcntname))
    {
        create_new_rel = true;
        sql = "ALTER TABLE " + table + " DROP FOREIGN KEY " + oldcntname + ";\n";
        diff.append(sql);
        sql = "ALTER TABLE " + table + " DROP INDEX " + oldcntname + ";\n\n";
        diff.append(sql);
    }
    addAlterFieldToDiff(table,a,a.attribute("size","0").toInt(),0,islookup);
    if (create_new_rel)
    {
        sql = "ALTER TABLE " + table + " ADD INDEX " + newcntname + " (" + a.attribute("name") + ");\n";
        diff.append(sql);
    }
    for (int pos = 0; pos < insert_diff.count(); pos++)
    {
        if (insert_diff[pos].table == a.attribute("rtable"))
        {
            diff.append("\n");
            for (int pos2 = 0; pos2 < insert_diff[pos].diff.count(); pos2++)
            {
                diff.append(insert_diff[pos].diff[pos2] + "\n");
            }
            insertTablesUsed.append(a.attribute("rtable"));
            diff.append("\n");
        }
    }
    if (create_new_rel)
    {
        sql = "ALTER TABLE " + table + " ADD CONSTRAINT " + newcntname + " FOREIGN KEY (" + a.attribute("name") + ") REFERENCES " + a.attribute("rtable") + " (" + a.attribute("rfield") + ") ON DELETE RESTRICT  ON UPDATE NO ACTION;\n\n";
        diff.append(sql);
    }
    addTableToDrop(b.attribute("rtable"));
}

void mergeCreate::addTableToDrop(QString name)
{
    bool found = false;
    if (dropTables.indexOf(name) >= 0)
        found = true;
    if (!found)
    {
        dropTables.append(name);
    }
}

//This adds the modifications to a table to the diff list
void mergeCreate::addFieldToDiff(QString table, QDomElement eField)
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
        sql = "ALTER TABLE " + table + " ADD INDEX " + eField.attribute("rname","")  + " (" + eField.attribute("name","") + ");\n";
        diff.append(sql);
        sql = "ALTER TABLE " + table + " ADD CONSTRAINT " + eField.attribute("rname","") + " FOREIGN KEY (";
        sql = sql + eField.attribute("name","") + ") REFERENCES " + eField.attribute("rtable","") + "(";
        sql = sql + eField.attribute("rfield","") + ") ON DELETE RESTRICT ON UPDATE NO ACTION;\n" ;
        diff.append(sql);
        idx++;
    }
}

void mergeCreate::addFieldToRTables(QString parentTable, QString rTable, QString field, QString rField, QString rname, bool isLookUp)
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
        aField.rcode = rname;
        aField.isLookUp = isLookUp;
        rtables[tidx].rfields.append(aField);
    }
    else
    {
        TrtableDef table;
        table.parentTable = parentTable;
        table.name = rTable;
        table.isLookUp = isLookUp;
        TrfieldDef aField;
        aField.name = field;
        aField.rname = rField;
        aField.rcode = rname;
        aField.isLookUp = isLookUp;
        table.rfields.append(aField);
        rtables.append(table);
    }
}

//This adds a table to the diff list
void mergeCreate::addTableToSDiff(QDomNode table, bool lookUp)
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
                bool isLookup = false;
                if (field.attribute("rlookup","false") == "true")
                    isLookup = true;
                addFieldToRTables(eTable.attribute("name",""),field.attribute("rtable",""),field.attribute("name",""),field.attribute("rfield",""),field.attribute("rname",""),isLookup);
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

            if (!rtables[pos].isLookUp)
            {
                QUuid idxUUID=QUuid::createUuid();
                QString strIdxUUID=idxUUID.toString().replace("{","").replace("}","").right(12);
                sql = sql + "INDEX DIDX"  + strIdxUUID + "(";
                for (pos2 = 0; pos2 < rtables[pos].rfields.count();pos2++)
                {
                    sql = sql + rtables[pos].rfields[pos2].name + ",";
                }
                sql = sql.left(sql.length()-1) + "),\n";
            }
            else
            {
                for (pos2 = 0; pos2 < rtables[pos].rfields.count();pos2++)
                {
                    sql = sql + "INDEX "  + rtables[pos].rfields[pos2].rcode + " (";
                    sql = sql + rtables[pos].rfields[pos2].name + "),\n";
                }
            }                        
            idx++;

        }
    }
    //Add foreign keys
    for (pos = 0; pos < rtables.count();pos++)
    {
        if (rtables[pos].parentTable == eTable.attribute("name",""))
        {            
            if (!rtables[pos].isLookUp)
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
                sql = sql.left(sql.length()-1) + ")\nON DELETE CASCADE\nON UPDATE NO ACTION,\n";

            }
            else
            {
                for (pos2 = 0; pos2 < rtables[pos].rfields.count();pos2++)
                {
                    sql = sql + "CONSTRAINT "  + rtables[pos].rfields[pos2].rcode + "\n";
                    sql = sql + "FOREIGN KEY (";
                    sql = sql + rtables[pos].rfields[pos2].name + ")\n";
                    sql = sql + "REFERENCES " + rtables[pos].name + "(";
                    sql = sql + rtables[pos].rfields[pos2].rname + ")\nON DELETE RESTRICT\nON UPDATE NO ACTION,\n";
                }
            }

            idx++;
        }
    }
    if (hasRelatedTables)
        sql = sql.left(sql.length()-2);
    sql = sql + ")\n ENGINE = InnoDB CHARSET=utf8 COMMENT = \"" + eTable.attribute("desc","") + "\";\n";
    sql = sql + "CREATE UNIQUE INDEX DXROWUUID" + strRecordUUID + " ON " + eTable.attribute("name","") + "(rowuuid);\n\n";

    QUuid TriggerUUID=QUuid::createUuid();
    QString strTriggerUUID=TriggerUUID.toString().replace("{","").replace("}","").replace("-","_");

    sql = sql + "delimiter $$\n\n";
    sql = sql + "CREATE TRIGGER T" + strTriggerUUID + " BEFORE INSERT ON " + eTable.attribute("name","") + " FOR EACH ROW BEGIN IF (new.rowuuid IS NULL) THEN SET new.rowuuid = uuid(); ELSE IF (new.rowuuid NOT REGEXP '[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}') THEN SET new.rowuuid = uuid(); END IF; END IF; END;$$\n\n";
    sql = sql + "delimiter ;\n\n";

    diff.append(sql);
    for (int pos = 0; pos <= childTables.count()-1;pos++)
        addTableToSDiff(childTables[pos],lookUp);
}

void mergeCreate::log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toUtf8().data());
}

QList<TcompError> mergeCreate::getErrorList()
{
    return errorList;
}

void mergeCreate::fatal(QString message)
{
    fprintf(stderr, "\033[31m%s\033[0m \n", message.toUtf8().data());
    fatalError = true;
}

QDomNode mergeCreate::findField(QDomNode table,QString field)
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

QDomNode mergeCreate::findTable(QDomDocument docB,QString tableName)
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

QString mergeCreate::compareFields(QDomElement a, QDomElement b, int &newSize, int &newDec)
{
    newSize = a.attribute("size","0").toInt();
    newDec = a.attribute("decsize","0").toInt();
    if (a.attribute("key","") != b.attribute("key",""))
        return "KNS";
//    if (a.attribute("key","true") == "true")
//    {
//        if (a.attribute("type") != b.attribute("type"))
//            return "KNS";
//        if (a.attribute("size") != b.attribute("size"))
//            return "KNS";
//        if (a.attribute("rtable") != b.attribute("rtable"))
//            return "KNS";
//        if (a.attribute("rfield") != b.attribute("rfield"))
//            return "KNS";
//    }
    if ((a.attribute("rtable","") != b.attribute("rtable","")) || (a.attribute("rfield","") != b.attribute("rfield","")))
    {
        if (a.attribute("mergefine","false") == "true")
        {
            return "CHR";
        }
        else
            return "RNS";
    }
    if (a.attribute("type","") != b.attribute("type",""))
    {
        if ((b.attribute("type") == "int") && (a.attribute("type") == "varchar"))
        {
            if (a.attribute("size","0").toInt() >= b.attribute("size","0").toInt())
            {
                if (a.attribute("decsize","0").toInt() > b.attribute("decsize","0").toInt())
                {
                    newSize = a.attribute("size","0").toInt();
                    newDec = a.attribute("decsize","0").toInt();
                }
                return "FTC";
            }
            else
                return "FNS";
        }
        else
            return "FNS";
    }

    if (a.attribute("size","") != b.attribute("size",""))
    {
        if (a.attribute("size","0").toInt() > b.attribute("size","0").toInt())
        {
            if (a.attribute("type","") == "decimal")
            {
                if (a.attribute("decsize","0").toInt() != b.attribute("decsize","0").toInt())
                {
                    if (a.attribute("decsize","0").toInt() > b.attribute("decsize","0").toInt())
                    {
                        newSize = a.attribute("size","0").toInt();
                        newDec = a.attribute("decsize","0").toInt();
                        return "FIC";
                    }
                    else
                    {
                        if (a.attribute("decsize","0").toInt() < b.attribute("decsize","0").toInt())
                            return "FDC";
                    }
                }
                else
                {
                    if (a.attribute("size","0").toInt() < b.attribute("size","0").toInt())
                        return "FDC";
                }
            }
            else
            {
                newSize = a.attribute("size","0").toInt();
                return "FIC";
            }
        }
        else
        {
            if (a.attribute("size","0").toInt() < b.attribute("size","0").toInt())
                return "FDC";
        }
    }
    if (a.attribute("decsize","") != b.attribute("decsize",""))
    {
        if (a.attribute("decsize","0").toInt() > b.attribute("decsize","0").toInt())
        {
            newSize = a.attribute("size","0").toInt();
            newDec = a.attribute("decsize","0").toInt();
            return "FIC";
        }
        else
        {
            if (a.attribute("decsize","0").toInt() < b.attribute("decsize","0").toInt())
                return "FDC";
        }
    }

    return "";
}

QString mergeCreate::getFieldDefinition(QDomElement field)
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

void mergeCreate::checkField(QDomNode eTable, QDomElement a, QDomElement b, bool islookup = false)
{
    int newSize;
    int newDec;
    QString result;
    result = compareFields(a,b,newSize,newDec);
    if (result != "")
    {
        if (outputType != "h")
        {
            TcompError error;
            error.code = result;
            error.table = eTable.toElement().attribute("name","");
            error.field = a.attribute("name","");
            error.from = getFieldDefinition(b);
            error.to = getFieldDefinition(a);
            errorList.append(error);
        }
        if ((result == "FIC") || (result == "CHR") || (result == "FTC"))
        {
            if (result == "FIC")
            {
                if (outputType == "h")
                {
                    if (a.attribute("type","") != "decimal")
                        log("FIC: The size of field " + a.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " will be increased from " + b.attribute("size",0) + " to " + a.attribute("size","0"));
                    else
                        log("FIC: The size of field " + a.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " will be increased from " + b.attribute("size",0) + " to " + a.attribute("size","0") + ". Decimal size from " + b.attribute("decsize",0) + " to " + a.attribute("decsize","0"));
                }
                b.setAttribute("size",newSize);
                b.setAttribute("decsize",newDec);
                addAlterFieldToDiff(eTable.toElement().attribute("name",""),a,newSize,newDec,islookup);
            }
            else
            {
                if (result == "CHR")
                {
                    if (outputType == "h")
                    {
                        log("CHR: The relationship of field " + a.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " will be changed from " + b.attribute("rtable") + "." + b.attribute("rfield") + " to " + a.attribute("rtable") + "." + a.attribute("rfield"));
                    }
                    changeLookupRelationship(eTable.toElement().attribute("name",""),a,b,islookup);
                    b.setAttribute("rtable",a.attribute("rtable"));
                    b.setAttribute("rfield",a.attribute("rfield"));
                    b.setAttribute("type",a.attribute("type"));
                    b.setAttribute("size",a.attribute("size"));
                    b.setAttribute("rname",a.attribute("rname"));
                }
                else
                {
                    if (outputType == "h")
                    {
                        log("FTC: The field " + a.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " changed type from " + b.attribute("type") + " to " + a.attribute("type") + " which is allowed");                        
                    }
                    b.setAttribute("size",newSize);
                    b.setAttribute("decsize",newDec);
                    b.setAttribute("type",a.attribute("type"));
                    addAlterFieldToDiff(eTable.toElement().attribute("name",""),a,newSize,newDec,islookup);
                }
            }
        }
        else
        {
            if (outputType == "h")
            {
                if (result == "KNS")
                    fatal("KNS: Field " + a.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " changed key from B to A. ODK Tools cannot fix this. You need to fix it in the Excel file");
                if (result == "RNS")
                    fatal("RNS: The relationship for field " + a.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " changed from B to A. ODK Tools cannot fix this. You need to fix it in the Excel file");
                if (result == "FNS")
                    fatal("FNS: Field " + a.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " changed type from B to A. ODK Tools cannot fix this. You need to fix it in the Excel file");
                if (result == "FDC")
                    log("FDC: The field " + a.attribute("name","") + " in table " + eTable.toElement().attribute("name","") + " has descreased in size. This change will be ignored");
            }
            else
            {
                if (result != "FDC")
                    fatalError = true;
            }
        }
    }
}

QDomNode getLastField(QDomNode table)
{
    QDomNode child = table.firstChild();
    QDomNode res;
    while (!child.isNull())
    {
        res = child;
        child = child.nextSibling();
    }
    return res;
}

QDomNode findNodeByName(QDomNode table, QString name)
{
    QDomNode child = table.firstChild();
    while (!child.isNull())
    {
        if (child.toElement().attribute("name","") == name)
            return child;
        child = child.nextSibling();
    }
    return QDomNode();
}

void mergeCreate::compareLKPTables(QDomNode table,QDomDocument &docB)
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
               if (eField.tagName() == "table")
                   compareLKPTables(field,docB);
               else
               {
                   QDomNode fieldFound = findField(tablefound,eField.attribute("name",""));
                   if (!fieldFound.isNull())
                   {
                       checkField(tablefound,field.toElement(),fieldFound.toElement(),true);
                   }
                   else
                   {
                       if (outputType == "h")
                           log("FNF:Field " + eField.attribute("name","") + " in table " + tablefound.toElement().attribute("name","") + " from A is not found in B");
                       else
                       {
                           TcompError error;
                           error.code = "FNF";
                           error.table = tablefound.toElement().attribute("name","");
                           error.field = eField.attribute("name","");
                           error.from = "NULL";
                           error.to = getFieldDefinition(eField);
                           errorList.append(error);
                       }
                       addFieldToDiff(tablefound.toElement().attribute("name",""),eField);
                       QDomNode node_before = eField.previousSibling();
                       if (!node_before.isNull())
                       {
                           if (node_before.toElement().tagName() != "field")
                               node_before = QDomNode();
                       }
                       QDomNode node_after = eField.previousSibling();
                       if (!node_after.isNull())
                       {
                           if (node_after.toElement().tagName() != "field")
                               node_after = QDomNode();
                       }
                       QDomNode reference;
                       bool after = true;
                       if (node_before.isNull() && node_after.isNull())
                           reference = getLastField(tablefound);
                       else
                       {
                           if (!node_before.isNull())
                           {
                               reference = findNodeByName(tablefound,node_before.toElement().attribute("name"));
                               if (reference.isNull())
                                   reference = getLastField(tablefound);
                           }
                           else
                           {
                               reference = findNodeByName(tablefound,node_after.toElement().attribute("name"));
                               if (reference.isNull())
                               {
                                   reference = getLastField(tablefound);
                               }
                               else
                                   after = false;
                           }
                       }

                       if (after)
                           tablefound.insertAfter(eField.cloneNode(true),reference);
                       else
                           tablefound.insertBefore(eField.cloneNode(true),reference);
                   }
               }

               field = field.nextSibling();
           }
       }
       else
       {
           if (outputType == "h")
               log("TNF:Lookup table " + node.toElement().attribute("name","") + " from A not found in B");
           else
           {
               TcompError error;
               error.code = "TNF";
               error.table = "NA";
               error.field = "NA";
               error.from = "NULL";
               error.to = node.toElement().attribute("name","");
               errorList.append(error);
           }
           addTableToSDiff(node,true);
           //Now adds the lookup table
           docB.documentElement().firstChild().appendChild(node.cloneNode(true));
       }
       node = node.nextSibling();
   }
}

void mergeCreate::compareTables(QDomNode table,QDomDocument &docB)
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
                        checkField(tablefound,field.toElement(),fieldFound.toElement(),false);
                    }
                    else
                    {
                        if (outputType == "h")
                            log("FNF:Field " + eField.attribute("name","") + " in table " + tablefound.toElement().attribute("name","") + " from A is not found in B");
                        else
                        {
                            TcompError error;
                            error.code = "FNF";
                            error.table = tablefound.toElement().attribute("name","");
                            error.field = eField.attribute("name","");
                            error.from = "NULL";
                            error.to = getFieldDefinition(eField);
                            errorList.append(error);
                        }
                        addFieldToDiff(tablefound.toElement().attribute("name",""),eField);
                        QDomNode node_before = eField.previousSibling();
                        if (!node_before.isNull())
                        {
                            if (node_before.toElement().tagName() != "field")
                                node_before = QDomNode();
                        }
                        QDomNode node_after = eField.previousSibling();
                        if (!node_after.isNull())
                        {
                            if (node_after.toElement().tagName() != "field")
                                node_after = QDomNode();
                        }
                        QDomNode reference;
                        bool after = true;
                        if (node_before.isNull() && node_after.isNull())
                            reference = getLastField(tablefound);
                        else
                        {
                            if (!node_before.isNull())
                            {
                                reference = findNodeByName(tablefound,node_before.toElement().attribute("name"));
                                if (reference.isNull())
                                    reference = getLastField(tablefound);
                            }
                            else
                            {
                                reference = findNodeByName(tablefound,node_after.toElement().attribute("name"));
                                if (reference.isNull())
                                {
                                    reference = getLastField(tablefound);
                                }
                                else
                                    after = false;
                            }
                        }

                        if (after)
                            tablefound.insertAfter(eField.cloneNode(true),reference);
                        else
                            tablefound.insertBefore(eField.cloneNode(true),reference);

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
                error.table = eTable.toElement().attribute("name","");
                error.field = "NA";
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
            else
            {
                TcompError error;
                error.code = "TNF";
                error.table = "NA";
                error.field = "NA";
                error.from = "NULL";
                error.to = eTable.toElement().attribute("name","");
                errorList.append(error);
            }
            addTableToSDiff(eTable,false);
            parentfound.appendChild(table.cloneNode(true));
        }
        else
        {
            if (outputType == "h")
                fatal("TWP:Table " + eTable.toElement().attribute("name","") + " from A not found in B. Its parent in A is not found in B");
            else
            {
                TcompError error;
                error.code = "TWP";
                error.table = eTable.toElement().attribute("name","");
                error.field = "NA";
                error.from = parentTableName;
                error.to = "NULL";
                errorList.append(error);
            }
        }
    }
}
