#include <QCoreApplication>
#include <tclap/CmdLine.h>
#include <QtXml>
#include <QFile>
#include <QDebug>

QString vMysqlHost;
QString vUserName;
QString vPassword;
QString vSchema;
QString vManifestMainTable;
QString vManifestMainVariable;
QString vManifestXML;
QString vDatafileXML;
QString vDatafileBaseNode;
QString vSqlInsertsFile;
QString vLogFile;
QString vPrefix;

int count;
QDomDocument manifestDoc("ODKManifest");
QDomDocument dataDoc("ODKManifest");
QDomNodeList tableNodesList;
QDomNodeList dataNodesList;
QString str;
int nodeDepth = 0;

QStringList recordKeys;

void createSQLStatement(QString tableName, QString fieldsStatement, QString valuesStatement) {
    QString nullHandler;

    //Replaces anything which is '' (space) with NULL before creating the script
    nullHandler = "insert into " + vPrefix + tableName + "(" + fieldsStatement.left(fieldsStatement.length() - 2) +
            ")\nvalues(" + valuesStatement.left(valuesStatement.length() - 2) + ");\n\n";
    nullHandler.replace("''", "NULL");

    str = str + nullHandler;
}

void processTableData(QDomNode tableNode, QDomNode recordNode, QString tableName, QString fieldsStatement, QString recordId) {
    QString valuesStatement;
    QDomNode childTableNodeData;
    QString variableName;
    QDomNode variableNode;
    QString variableValue;
    int fieldCounter = 0;

    valuesStatement = "";

    //Pass through the fields in the table so as to extract their value from the record node
    childTableNodeData = tableNode.firstChild();

    while (!childTableNodeData.isNull()) {
        fieldCounter = fieldCounter + 1;

        if (childTableNodeData.toElement().tagName() == "field") {
            variableName = childTableNodeData.toElement().attribute("mysqlcode", "None");
            variableNode = recordNode.toElement().elementsByTagName(variableName).item(0);

            if(variableNode.toElement().text() == "") {
                if (fieldCounter == nodeDepth) {
                    //Handles generation of primary key Id and adds to the recordKeys list
                    variableValue = recordId;

                    if (recordKeys.size() < nodeDepth) {
                        recordKeys.append(variableValue);
                    }
                    else {
                        recordKeys.replace(nodeDepth - 1, variableValue);
                    }
                }
                else if (fieldCounter < nodeDepth) {
                    //Referenced key get it from the recordKeys list
                    variableValue = recordKeys.at(fieldCounter - 1);
                }
                else {
                    //Current field is not among the primary key fields
                    variableValue = "";
                }
            }
            else {
                //Check if the main table (vManifestMainTable) and the key field (vManifestMainVariable) then add the key field in keys list
                if (tableName == vManifestMainTable and variableName == vManifestMainVariable) {
                    if (recordKeys.size() < nodeDepth) {
                        recordKeys.append(variableValue);
                    }
                    else {
                        recordKeys.replace(nodeDepth - 1, variableNode.toElement().text());
                    }
                }

                variableValue = variableNode.toElement().text();
            }

            valuesStatement = valuesStatement + "'" + variableValue + "', ";
        }

        //Get the next child table node for the data
        childTableNodeData = childTableNodeData.nextSibling();
    }

    createSQLStatement(tableName, fieldsStatement, valuesStatement);
}

int processTables(QDomNode tableNodeToProcess, QDomNode dataNodeToProcess) {
    QString tableName;
    QDomNodeList recordsNodesList;
    QDomNode recordNode;
    QDomNode tableNode;
    QString fieldsStatement;
    QDomNode childTableNode;
    QString fieldName;
    bool processedData = false;

    //Get the table name and set how deep the node is in the tree
    tableName = tableNodeToProcess.toElement().attribute("mysqlcode", "None");
    nodeDepth = nodeDepth + 1;

    //Get the records from the data using the table name
    //First check if the main table has been processed and pick its name correctly
    //This is caused by the fact that the main table in the manifest and the data might not have the same name
    if (tableName == vManifestMainTable) {
        recordsNodesList = dataDoc.elementsByTagName(vDatafileBaseNode);
    }
    else {
        if (tableName.startsWith(vManifestMainTable)) {
            recordsNodesList = dataNodeToProcess.toElement().elementsByTagName(tableName.mid(vManifestMainTable.length() + 1));
        }
        else {
            recordsNodesList = dataNodeToProcess.toElement().elementsByTagName(tableName);
        }
    }

    if (recordsNodesList.count() > 0) {
        for (int recCount = 0; recCount <= recordsNodesList.count() - 1; recCount++) {
            processedData = false;
            recordNode  = recordsNodesList.item(recCount);

            //Get the current table node from manifest
            for(int tcount = 0; tableNodesList.count(); tcount++) {
                if(tableNodesList.item(tcount).toElement().attribute("mysqlcode", "None") == tableName) {
                    tableNode = tableNodesList.item(tcount);
                    break;
                }
            }

            //Pass through the table child nodes to get the field names
            fieldsStatement = "";

            childTableNode = tableNode.firstChild();

            while (!childTableNode.isNull()) {
                if (childTableNode.toElement().tagName() == "field") {
                    //Get the field name and use in creating the fields statement
                    fieldName = childTableNode.toElement().attribute("mysqlcode", "None");
                    fieldsStatement = fieldsStatement + fieldName + ", ";
                }
                else {
                    //Encountered a table get the record values under this parent record and do process again
                    if (processedData == false) {
                        processTableData(tableNode, recordNode, tableName, fieldsStatement, QString::number(recCount + 1));
                        processedData = true;
                    }

                    processTables(childTableNode, recordNode);
                }

                //Get the next child table node
                childTableNode = childTableNode.nextSibling();
            }

            //If no table was encountered within the table being processed
            if (processedData == false) {
                processTableData(tableNode, recordNode, tableName, fieldsStatement, QString::number(recCount + 1));
            }
        }
    }

    //Current table finished reduce nodeDepth
    nodeDepth = nodeDepth - 1;

    return 0;
}

int processManifestXML() {
    QFile xmlManifest(vManifestXML);
    QFile xmlData(vDatafileXML);
    QFile sqlInsertsFile(vSqlInsertsFile);

    //Open the manifest file and put the contents in a DOM document
    if (!xmlManifest.open(QIODevice::ReadOnly))
        return 1;

    if (!manifestDoc.setContent(&xmlManifest)) {
        xmlManifest.close();
        return 1;
    }

    xmlManifest.close();

    //Open the xml data file and put the contents in a DOM document
    if (!xmlData.open(QIODevice::ReadOnly))
        return 1;

    if (!dataDoc.setContent(&xmlData)) {
        xmlData.close();
        return 1;
    }

    xmlData.close();

    //Open text file for writing
    if (!sqlInsertsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return 1;
    }
    QTextStream out(&sqlInsertsFile);

    //Get the list of table nodes from the manifest
    tableNodesList = manifestDoc.elementsByTagName("table");

    //Get nodes list for the data
    dataNodesList = dataDoc.elementsByTagName(vDatafileBaseNode);

    //Start the data processing
    processTables(tableNodesList.item(0), dataNodesList.item(0));

    //Write the string to the file and close the file
    out << str;
    sqlInsertsFile.close();

    return 0;

}

int main(int argc, char *argv[]) {
    QString title;
    title = title + " ****************************************************************** \n";
    title = title + " * odkdatatomysql 1.0                                             * \n";
    title = title + " * This tool converts ODK data into MySQL.                        * \n";
    title = title + " * The tool uses manifest xml file and ODK xml data file          * \n";
    title = title + " * Outputs data into a MySQL database and generates a log file    * \n";
    title = title + " * This tool is part of ODK Tools (c) ILRI, 2014                  * \n";
    title = title + " ****************************************************************** \n";

    TCLAP::CmdLine cmd(title.toLatin1().constData(), ' ', "1.0 (Beta 1)");

    //Required arguments
    TCLAP::ValueArg<std::string> mysqlHostArg("H","mysqlHost","MySQL server host",true,"","string");
    TCLAP::ValueArg<std::string> userNameArg("u","userName","MySQL server user name",true,"","string");
    TCLAP::ValueArg<std::string> passwordArg("p","password","MySQL server user password",true,"","string");
    TCLAP::ValueArg<std::string> schemaArg("s","schema","Schema name in MySQL host",true,"","string");
    TCLAP::ValueArg<std::string> manifestMainTableArg("t","manifestMainTable","Manifest main table within the schema",true,"","string");
    TCLAP::ValueArg<std::string> manifestMainVariableArg("v","manifestMainVariable","Manifest main variable in main table. Identifies unique record",true,"","string");
    TCLAP::ValueArg<std::string> manifestXMLArg("f","manifestXML","ODK manifest XML file",true,"","string");
    TCLAP::ValueArg<std::string> datafileXMLArg("d","datafileXML","ODK data XML file",true,"","string");
    TCLAP::ValueArg<std::string> datafileBaseNodeArg("b","datafileBaseNode","ODK data file XML base node. Node just after the <?xml version=1.0?>",true,"","string");
    TCLAP::ValueArg<std::string> sqlInsertsFileArg("q","sqlInsertsFile","SQL output file. Default ./sqlInsertFile.sql",true,"./sqlInsertFile.sql","string");
    TCLAP::ValueArg<std::string> logFileArg("l","logFile","Log file for errors. Default ./logfile.csv",true,"./logFile.csv","string");
    TCLAP::ValueArg<std::string> prefixArg("r","prefix","Prefix for each table. _ is added to the prefix. Default no prefix",false,"","string");

    cmd.add(mysqlHostArg);
    cmd.add(userNameArg);
    cmd.add(passwordArg);
    cmd.add(schemaArg);
    cmd.add(manifestMainTableArg);
    cmd.add(manifestMainVariableArg);
    cmd.add(manifestXMLArg);
    cmd.add(datafileXMLArg);
    cmd.add(datafileBaseNodeArg);
    cmd.add(sqlInsertsFileArg);
    cmd.add(logFileArg);
    cmd.add(prefixArg);

    //Parsing the command lines
    cmd.parse( argc, argv );

    vMysqlHost = QString::fromUtf8(mysqlHostArg.getValue().c_str());
    vUserName = QString::fromUtf8(userNameArg.getValue().c_str());
    vPassword = QString::fromUtf8(passwordArg.getValue().c_str());
    vSchema = QString::fromUtf8(schemaArg.getValue().c_str());
    vManifestMainTable = QString::fromUtf8(manifestMainTableArg.getValue().c_str());
    vManifestMainVariable = QString::fromUtf8(manifestMainVariableArg.getValue().c_str());
    vManifestXML = QString::fromUtf8(manifestXMLArg.getValue().c_str());
    vDatafileXML = QString::fromUtf8(datafileXMLArg.getValue().c_str());
    vDatafileBaseNode = QString::fromUtf8(datafileBaseNodeArg.getValue().c_str());
    vSqlInsertsFile = QString::fromUtf8(sqlInsertsFileArg.getValue().c_str());
    vLogFile = QString::fromUtf8(logFileArg.getValue().c_str());
    vPrefix = QString::fromUtf8(prefixArg.getValue().c_str());

    //Check if any prefix specified and if not clear prefix
    vPrefix = vPrefix + "_";

    if (vPrefix == "_")
        vPrefix = "";

    count = 0;
    //Call the processing of the manifest file
    processManifestXML();

    return 0;

}
