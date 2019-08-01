/*
CreateAuditTriggers

Copyright (C) 2019 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com)

CreateAuditTriggers is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

CreateAuditTriggers is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with CreateFromXML.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <QCoreApplication>
#include <tclap/CmdLine.h>
#include "mainclass.h"
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString title;
    title = title + "********************************************************************* \n";
    title = title + " * CreateAuditTriggers                                               * \n";
    title = title + " * This tool creates the audit triggers based on a MySQL schemata.   * \n";
    title = title + " * The tool generates both MySQL and SQLite triggers.                * \n";
    title = title + " * (c) QLands, 2019                                                  * \n";
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.1");
    TCLAP::ValueArg<std::string> hostArg("H","host","MySQL Host. Default: localhost",false,"localhost","string");
    TCLAP::ValueArg<std::string> portArg("P","port","MySQL port. Default: 3306",false,"3306","string");
    TCLAP::ValueArg<std::string> userArg("u","user","MySQL User",true,"","string");
    TCLAP::ValueArg<std::string> passArg("p","password","MySQL Password",true,"","string");
    TCLAP::ValueArg<std::string> schemaArg("s","schema","MySQL Schema",true,"","string");
    TCLAP::ValueArg<std::string> outputArg("o","output","Output directory",false,".","string");

    cmd.add(hostArg);
    cmd.add(portArg);
    cmd.add(userArg);
    cmd.add(passArg);
    cmd.add(schemaArg);
    cmd.add(outputArg);

    //Parsing the command lines
    cmd.parse( argc, argv );


    //Getting the variables from the command
    QString host = QString::fromUtf8(hostArg.getValue().c_str());
    QString port = QString::fromUtf8(portArg.getValue().c_str());
    QString user = QString::fromUtf8(userArg.getValue().c_str());
    QString password = QString::fromUtf8(passArg.getValue().c_str());
    QString schema = QString::fromUtf8(schemaArg.getValue().c_str());
    QString output = QString::fromUtf8(outputArg.getValue().c_str());

    mainClass *task = new mainClass(&app);

    task->setParameters(host,port,user,password,schema,output);

    QObject::connect(task, SIGNAL(finished()), &app, SLOT(quit()));

    QTimer::singleShot(0, task, SLOT(run()));

    app.exec();
    return task->returnCode;
}
