#include <QCoreApplication>
#include <tclap/CmdLine.h>
#include <QTimer>
#include "mainclass.h"
#include <QTime>

/*
MySQLToJSON

Copyright (C) 2022 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

MySQLToXLSX is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

MySQLToXLSX is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with MySQLToXLSX.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

QString getRandomHex(const int &length)
{
    QString randomHex;

    for(int i = 0; i < length; i++) {
        int n = qrand() % 16;
        randomHex.append(QString::number(n,16));
    }

    return randomHex;
}

void log_out(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s", temp.toUtf8().data());
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString title;
    title = title + " *********************************************************************** \n";
    title = title + " * MySQLToJSON                                                         * \n";
    title = title + " * This tool extracts data from a MySQL Database into JSON files.      * \n";
    title = title + " * Each table will create a new JSON file.                             * \n";
    title = title + " * The tool requires the create.xml file created by JXFormToMySQL to   * \n";
    title = title + " * determine the type of data and whether a field or a table should be * \n";
    title = title + " * exported due to the sensitivity of its information.                 * \n";
    title = title + " *********************************************************************** \n";

    TCLAP::CmdLine cmd(title.toUtf8().data(), ' ', "2.0");
    //Required arguments
    TCLAP::ValueArg<std::string> hostArg("H","host","MySQL host. Default localhost",false,"localhost","string");
    TCLAP::ValueArg<std::string> portArg("P","port","MySQL port. Default 3306.",false,"3306","string");
    TCLAP::ValueArg<std::string> userArg("u","user","User to connect to MySQL",true,"","string");
    TCLAP::ValueArg<std::string> passArg("p","password","Password to connect to MySQL",true,"","string");
    TCLAP::ValueArg<std::string> schemaArg("s","schema","Schema in MySQL",true,"","string");
    TCLAP::ValueArg<std::string> createArg("x","createxml","Input create XML file",true,"","string");
    TCLAP::ValueArg<std::string> outArg("o","output","Output directory for the JSON files",true,"","string");
    TCLAP::ValueArg<std::string> tmpArg("T","tempdir","Temporary directory (./tmp by default)",false,"./tmp","string");
    TCLAP::ValueArg<std::string> firstArg("f","firstsheetname","Name for the first sheet",false,"","string");
    TCLAP::ValueArg<std::string> encryptArg("e","encrypt","32 char hex encryption key. Auto generate if empty",false,"","string");
    TCLAP::ValueArg<std::string> resolveArg("r","resolve","Resolve lookup values: 1=Codes only (default), 2=Descriptions, 3=Codes and descriptions",false,"1","string");
    TCLAP::SwitchArg lookupSwitch("l","includelookups","Include lookup tables. False by default", cmd, false);
    TCLAP::SwitchArg mselSwitch("m","includemultiselects","Include multi-select tables as sheets. False by default", cmd, false);    
    TCLAP::SwitchArg protectSwitch("c","protect","Protect sensitive fields. False by default", cmd, false);
    TCLAP::ValueArg<std::string> numWorkers("w","workers","Number of workers. 1 by default",false,"1","string");

    cmd.add(hostArg);
    cmd.add(portArg);
    cmd.add(numWorkers);
    cmd.add(userArg);
    cmd.add(passArg);
    cmd.add(schemaArg);
    cmd.add(outArg);
    cmd.add(tmpArg);
    cmd.add(createArg);    
    cmd.add(firstArg);
    cmd.add(encryptArg);
    cmd.add(resolveArg);
    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    bool protectSensitive;
    protectSensitive = protectSwitch.getValue();

    bool includeLookUps;
    includeLookUps = lookupSwitch.getValue();

    bool includeMSels;
    includeMSels = mselSwitch.getValue();


    QString host = QString::fromUtf8(hostArg.getValue().c_str());
    QString port = QString::fromUtf8(portArg.getValue().c_str());
    QString user = QString::fromUtf8(userArg.getValue().c_str());
    QString pass = QString::fromUtf8(passArg.getValue().c_str());
    QString schema = QString::fromUtf8(schemaArg.getValue().c_str());
    QString outputFile = QString::fromUtf8(outArg.getValue().c_str());
    QString tmpDir = QString::fromUtf8(tmpArg.getValue().c_str());
    QString createXML = QString::fromUtf8(createArg.getValue().c_str());    
    QString firstSheetName = QString::fromUtf8(firstArg.getValue().c_str());
    QString encryption_key = QString::fromUtf8(encryptArg.getValue().c_str());
    QString resolve_type = QString::fromUtf8(resolveArg.getValue().c_str());
    bool ok;

    if (resolve_type.toInt() > 1)
    {
        if (includeMSels || includeLookUps)
        {
            log_out("Resolving codes cannot include multiselect or lookup tables");
            exit(1);
        }
    }

    int workers = QString::fromUtf8(numWorkers.getValue().c_str()).toInt(&ok);
    if (!ok)
        workers = 1;
    if (encryption_key == "")
    {
        QTime time = QTime::currentTime();
        qsrand((uint)time.msec());
        encryption_key = getRandomHex(32);
    }

    mainClass *task = new mainClass(&app);
    task->setParameters(host,port,user,pass,schema,createXML,outputFile,protectSensitive,tmpDir, includeLookUps, includeMSels, firstSheetName, encryption_key, resolve_type, workers);
    QObject::connect(task, SIGNAL(finished()), &app, SLOT(quit()));
    QTimer::singleShot(0, task, SLOT(run()));
    app.exec();
    return task->returnCode;
}
