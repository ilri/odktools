#include <QCoreApplication>
#include <tclap/CmdLine.h>
#include <QTimer>
#include "mainclass.h"
/*
MySQLToXLSX

Copyright (C) 2018 QLands Technology Consultants.
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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString title;
    title = title + " *********************************************************************** \n";
    title = title + " * MySQLToXLSX                                                         * \n";
    title = title + " * This tool extracts data from a MySQL Database into XLSX files.      * \n";
    title = title + " * Each table will create a new sheet in the excel file.               * \n";
    title = title + " * The tool requires the create.xml file created by ODKToMySQL to      * \n";
    title = title + " * determine the type of data and whether a field or a table should be * \n";
    title = title + " * exported due to the sensitivity of its information.                 * \n";
    title = title + " *********************************************************************** \n";

    TCLAP::CmdLine cmd(title.toUtf8().data(), ' ', "1.1");
    //Required arguments
    TCLAP::ValueArg<std::string> hostArg("H","host","MySQL host. Default localhost",false,"localhost","string");
    TCLAP::ValueArg<std::string> portArg("P","port","MySQL port. Default 3306.",false,"3306","string");
    TCLAP::ValueArg<std::string> userArg("u","user","User to connect to MySQL",true,"","string");
    TCLAP::ValueArg<std::string> passArg("p","password","Password to connect to MySQL",true,"","string");
    TCLAP::ValueArg<std::string> schemaArg("s","schema","Schema in MySQL",true,"","string");
    TCLAP::ValueArg<std::string> createArg("x","createxml","Input create XML file",true,"","string");
    TCLAP::ValueArg<std::string> outArg("o","output","Output XLSX file",true,"","string");
    TCLAP::ValueArg<std::string> tmpArg("T","tempdir","Temporary directory (./tmp by default)",false,"./tmp","string");
    TCLAP::ValueArg<std::string> firstArg("f","firstsheetname","Name for the first sheet",false,"","string");
    TCLAP::SwitchArg remoteSwitch("i","includesensitive","Include sensitive information. False by default", cmd, false);
    TCLAP::SwitchArg lookupSwitch("l","includelookups","Include lookup tables. False by default", cmd, false);
    TCLAP::SwitchArg mselSwitch("m","includemultiselects","Include multi-select tables. False by default", cmd, false);



    cmd.add(hostArg);
    cmd.add(portArg);
    cmd.add(userArg);
    cmd.add(passArg);
    cmd.add(schemaArg);
    cmd.add(outArg);
    cmd.add(tmpArg);
    cmd.add(createArg);
    cmd.add(firstArg);
    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    bool includeSensitive;
    includeSensitive = remoteSwitch.getValue();

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

    mainClass *task = new mainClass(&app);
    task->setParameters(host,port,user,pass,schema,createXML,outputFile,includeSensitive,tmpDir, includeLookUps, includeMSels, firstSheetName);
    QObject::connect(task, SIGNAL(finished()), &app, SLOT(quit()));
    QTimer::singleShot(0, task, SLOT(run()));
    app.exec();
    return task->returnCode;
}
