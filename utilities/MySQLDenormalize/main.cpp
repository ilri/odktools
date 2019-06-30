/*
MySQLDenormalize

Copyright (C) 2018 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

MySQLDenormalize is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

MySQLDenormalize is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with MySQLDenormalize.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <QCoreApplication>
#include "mainclass.h"
#include <tclap/CmdLine.h>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString title;
    title = title + " *********************************************************************** \n";
    title = title + " * MySQLDenormalize                                                    * \n";
    title = title + " * This tool denormalize data from a MySQL Database into JSON          * \n";
    title = title + " * files starting from the main table. It relies on the Map XML files  * \n";
    title = title + " * created by JSONToMySQL. It is useful when you need                  * \n";
    title = title + " * to generate a CSV of the data but where one line is one record      * \n";
    title = title + " * of the starting table and from there on the columns of the CSV      * \n";
    title = title + " * are the columns of many different tables in the database thus       * \n";
    title = title + " * the output is only one CSV file with many columns and not           * \n";
    title = title + " * many CSVs as tables in the database.                                * \n";
    title = title + " *********************************************************************** \n";

    TCLAP::CmdLine cmd(title.toUtf8().data(), ' ', "1.1");
    //Required arguments
    TCLAP::ValueArg<std::string> hostArg("H","host","MySQL host. Default localhost",false,"localhost","string");
    TCLAP::ValueArg<std::string> portArg("P","port","MySQL port. Default 3306.",false,"3306","string");
    TCLAP::ValueArg<std::string> userArg("u","user","User to connect to MySQL",true,"","string");
    TCLAP::ValueArg<std::string> passArg("p","password","Password to connect to MySQL",true,"","string");
    TCLAP::ValueArg<std::string> schemaArg("s","schema","Schema in MySQL",true,"","string");
    TCLAP::ValueArg<std::string> tableArg("t","maintable","Main table to denormalize from",true,"","string");
    TCLAP::ValueArg<std::string> mapArg("m","mapdirectory","Directory containing the map XML files",true,"","string");
    TCLAP::ValueArg<std::string> outArg("o","output","Output directory to store the JSON result files",false,"CSV","string");
    TCLAP::ValueArg<std::string> tmpArg("T","tempdir","Temporary directory (./tmp by default)",false,"./tmp","string");
    TCLAP::SwitchArg remoteSwitch("i","includeprotected","Include protected", cmd, false);


    cmd.add(hostArg);
    cmd.add(portArg);
    cmd.add(userArg);
    cmd.add(passArg);
    cmd.add(schemaArg);
    cmd.add(tableArg);
    cmd.add(mapArg);
    cmd.add(outArg);
    cmd.add(tmpArg);
    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    bool includeProtected;
    includeProtected = remoteSwitch.getValue();

    QString host = QString::fromUtf8(hostArg.getValue().c_str());
    QString port = QString::fromUtf8(portArg.getValue().c_str());
    QString user = QString::fromUtf8(userArg.getValue().c_str());
    QString pass = QString::fromUtf8(passArg.getValue().c_str());
    QString schema = QString::fromUtf8(schemaArg.getValue().c_str());
    QString table = QString::fromUtf8(tableArg.getValue().c_str());
    QString mapDir = QString::fromUtf8(mapArg.getValue().c_str());
    QString output = QString::fromUtf8(outArg.getValue().c_str());
    QString tmpDir = QString::fromUtf8(tmpArg.getValue().c_str());

    mainClass *task = new mainClass(&app);
    task->setParameters(host,port,user,pass,schema,table,mapDir,output,includeProtected,tmpDir);
    QObject::connect(task, SIGNAL(finished()), &app, SLOT(quit()));
    QTimer::singleShot(0, task, SLOT(run()));
    app.exec();
    return task->returnCode;
}
