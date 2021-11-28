#include <QCoreApplication>
#include <tclap/CmdLine.h>
#include <QTimer>
#include "mainclass.h"
#include <QTime>

/*
createTemporaryTable

Copyright (C) 2021 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

createTemporaryTable is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

createTemporaryTable is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with createTemporaryTable.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString title;
    title = title + " *********************************************************************** \n";
    title = title + " * createTemporaryTable                                                * \n";
    title = title + " * This tool creates a temporary table based on a select of another.   * \n";
    title = title + " * The tools resolves the lookup values of selects and multi-selects.  * \n";
    title = title + " * The tool requires the create.xml file created by JXFormToMySQL to   * \n";
    title = title + " * determine the type of data to be exported.                          * \n";
    title = title + " *********************************************************************** \n";

    TCLAP::CmdLine cmd(title.toUtf8().data(), ' ', "2.0");
    //Required arguments
    TCLAP::ValueArg<std::string> hostArg("H","host","MySQL host. Default localhost",false,"localhost","string");
    TCLAP::ValueArg<std::string> portArg("P","port","MySQL port. Default 3306.",false,"3306","string");
    TCLAP::ValueArg<std::string> userArg("u","user","User to connect to MySQL",true,"","string");
    TCLAP::ValueArg<std::string> passArg("p","password","Password to connect to MySQL",true,"","string");
    TCLAP::ValueArg<std::string> schemaArg("s","schema","Schema in MySQL",true,"","string");
    TCLAP::ValueArg<std::string> createArg("x","createxml","Input create XML file",true,"","string");    
    TCLAP::ValueArg<std::string> resolveArg("r","resolve","Resolve lookup values: 1=Codes only (default), 2=Descriptions, 3=Codes and descriptions",false,"1","string");
    TCLAP::ValueArg<std::string> tableArg("t","table","Table to export",true,"","string");
    TCLAP::ValueArg<std::string> fieldsArg("f","fields","Fields to export separated by |. Like: field|field|field",true,"","string");
    TCLAP::ValueArg<std::string> nameArg("n","name","Name of the temporary table",true,"","string");
    TCLAP::ValueArg<std::string> tmpArg("T","tempdir","Temporary directory (./tmp by default)",false,"./tmp","string");

    cmd.add(hostArg);
    cmd.add(portArg);
    cmd.add(userArg);
    cmd.add(passArg);
    cmd.add(schemaArg);
    cmd.add(tableArg);
    cmd.add(fieldsArg);
    cmd.add(createArg);    
    cmd.add(nameArg);
    cmd.add(resolveArg);
    cmd.add(tmpArg);
    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command


    QString host = QString::fromUtf8(hostArg.getValue().c_str());
    QString port = QString::fromUtf8(portArg.getValue().c_str());
    QString user = QString::fromUtf8(userArg.getValue().c_str());
    QString pass = QString::fromUtf8(passArg.getValue().c_str());
    QString schema = QString::fromUtf8(schemaArg.getValue().c_str());
    QString table = QString::fromUtf8(tableArg.getValue().c_str());
    QString fields = QString::fromUtf8(fieldsArg.getValue().c_str());
    QString createXML = QString::fromUtf8(createArg.getValue().c_str());    
    QString name = QString::fromUtf8(nameArg.getValue().c_str());
    QString resolve_type = QString::fromUtf8(resolveArg.getValue().c_str());
    QString tmpDir = QString::fromUtf8(tmpArg.getValue().c_str());

    fields =  fields.toLower();
    mainClass *task = new mainClass(&app);
    task->setParameters(host,port,user,pass,schema,createXML,table,fields,name,resolve_type,tmpDir);
    QObject::connect(task, SIGNAL(finished()), &app, SLOT(quit()));
    QTimer::singleShot(0, task, SLOT(run()));
    app.exec();
    return task->returnCode;
}
