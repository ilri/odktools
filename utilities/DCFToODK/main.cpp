#include <tclap/CmdLine.h>
#include <QtCore>
#include "dcftoxml.h"
#include "xmltoyml.h"

void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toUtf8().data());
}

int main(int argc, char *argv[])
{
    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * DCF to ODK                                                        * \n";
    title = title + " * This tool creates a YML represention of an ODK Excel Form based   * \n";
    title = title + " * on a CSPro dictionary file (dcf).                                 * \n";
    title = title + " *                                                                   * \n";
    title = title + " * This tool is usefull when migrating from CSPro to ODK. It         * \n";
    title = title + " * creates a basic ODK structure without logic but could accelarate  * \n";
    title = title + " * a migration when dealing with big CSPro surveys.                  * \n";
    title = title + " * This tools requires yq (https://mikefarah.gitbook.io/yq/v/v3.x/). * \n";
    title = title + " *                                                                   * \n";
    title = title + " * The YAML file can be converted to an Excel workbook with yxf      * \n";
    title = title + " * https://github.com/Sjlver/yxf                                     * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "2.0");

    TCLAP::ValueArg<std::string> inputArg("i","input","Input DCF file",true,"","string");
    TCLAP::ValueArg<std::string> outputArg("o","output","Output YAML file",true,"","string");
    TCLAP::ValueArg<std::string> mainRecordArg("r","mainrecord","Main record",true,"","string");
    TCLAP::ValueArg<std::string> temDirArg("t","tempdir","Temporary directory",true,"","string");


    cmd.add(inputArg);
    cmd.add(outputArg);
    cmd.add(mainRecordArg);
    cmd.add(temDirArg);

    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    QString input = QString::fromUtf8(inputArg.getValue().c_str());
    QString output = QString::fromUtf8(outputArg.getValue().c_str());
    QString mainRecord = QString::fromUtf8(mainRecordArg.getValue().c_str());
    QString tempDir = QString::fromUtf8(temDirArg.getValue().c_str());


    if (input != output)
    {
        if (QFile::exists(input))
        {
            QDir dir(tempDir);
            if (dir.exists())
            {
                DCFToXML toXML;
                toXML.convertToXML(input);
                XMLToYML toYML;
                toYML.setXMLDocument(toXML.xmlDoc);
                toYML.generateYML(output,mainRecord,tempDir);
                exit(0);
            }
            else
                log("The temporary directory does not exist");
        }
        else
            log("The DFC file does not exist");
    }
    else
        log("The DCF file and the YML file cannot be the same");
    exit(1);
}
