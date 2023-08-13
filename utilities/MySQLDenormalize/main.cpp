#include <QCoreApplication>
#include <tclap/CmdLine.h>
#include <QTimer>
#include "mainclass.h"
#include <QTime>
#include <QRandomGenerator>

/*
MySQLDenormalize

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

QString getRandomHex(const int &length)
{
    QString randomHex;
    QRandomGenerator gen;
    QTime time = QTime::currentTime();
    gen.seed((uint)time.msec());
    for(int i = 0; i < length; i++) {
        int n = gen.generate() % 16;
        randomHex.append(QString::number(n,16));
    }

    return randomHex;
}

void log_error(QString message)
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
    title = title + " * MySQLDenormalize                                                    * \n";
    title = title + " * This tool denormalize data from a MySQL Database into JSON          * \n";
    title = title + " * files starting from the main table. It relies on the Map XML files  * \n";
    title = title + " * created by JSONToMySQL. It is useful if you need to go back         * \n";
    title = title + " * to a JSON representation of relational data.                        * \n";
    title = title + " *********************************************************************** \n";

    TCLAP::CmdLine cmd(title.toUtf8().data(), ' ', "2.0");
    //Required arguments
    TCLAP::ValueArg<std::string> hostArg("H","host","MySQL host. Default localhost",false,"localhost","string");
    TCLAP::ValueArg<std::string> portArg("P","port","MySQL port. Default 3306.",false,"3306","string");
    TCLAP::ValueArg<std::string> userArg("u","user","User to connect to MySQL",true,"","string");
    TCLAP::ValueArg<std::string> passArg("p","password","Password to connect to MySQL",true,"","string");
    TCLAP::ValueArg<std::string> schemaArg("s","schema","Schema in MySQL",true,"","string");
    TCLAP::ValueArg<std::string> createArg("x","createxml","Input create XML file",true,"","string");    
    TCLAP::ValueArg<std::string> tmpArg("T","tempdir","Temporary directory (./tmp by default)",false,"./tmp","string");
    TCLAP::ValueArg<std::string> encryptArg("e","encrypt","32 char hex encryption key. Auto generate if empty",false,"","string");
    TCLAP::ValueArg<std::string> tableArg("t","maintable","Main table name",true,"","string");
    TCLAP::ValueArg<std::string> mapArg("m","mapdirectory","Directory containing the map XML files",true,"","string");
    TCLAP::ValueArg<std::string> outArg("o","output","Output directory to store the JSON result files",true,"","string");
    TCLAP::ValueArg<std::string> keyArg("k","key","Specific primary key to use",false,"","string");
    TCLAP::ValueArg<std::string> valueArg("v","value","Specific primary key value to use",false,"","string");
    TCLAP::ValueArg<std::string> separatorArg("S","separator","Separator to use in multi-selects. Pipe (|) is default",false,"|","string");
    TCLAP::ValueArg<std::string> resolveArg("r","resolve","Resolve lookup values: 1=Codes only (default), 2=Descriptions, 3=Codes and descriptions",false,"1","string");

    TCLAP::SwitchArg protectSwitch("c","protect","Protect sensitive fields. False by default", cmd, false);
    TCLAP::SwitchArg ODKFormatSwitch("f","odkformat","Format like ODK Collect. Keys will be the same as if data was collected by ODK Collect", cmd, false);


    cmd.add(hostArg);
    cmd.add(portArg);
    cmd.add(userArg);
    cmd.add(passArg);
    cmd.add(schemaArg);    
    cmd.add(tmpArg);
    cmd.add(createArg);        
    cmd.add(encryptArg);
    cmd.add(keyArg);
    cmd.add(valueArg);
    cmd.add(separatorArg);
    cmd.add(tableArg);
    cmd.add(mapArg);
    cmd.add(outArg);
    cmd.add(resolveArg);

    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    bool protectSensitive;
    protectSensitive = protectSwitch.getValue();

    bool likeODKCollect;
    likeODKCollect = ODKFormatSwitch.getValue();


    QString host = QString::fromUtf8(hostArg.getValue().c_str());
    QString port = QString::fromUtf8(portArg.getValue().c_str());
    QString user = QString::fromUtf8(userArg.getValue().c_str());
    QString pass = QString::fromUtf8(passArg.getValue().c_str());
    QString schema = QString::fromUtf8(schemaArg.getValue().c_str());
    QString key = QString::fromUtf8(keyArg.getValue().c_str());
    QString value = QString::fromUtf8(valueArg.getValue().c_str());
    QString separator = QString::fromUtf8(separatorArg.getValue().c_str());
    QString tmpDir = QString::fromUtf8(tmpArg.getValue().c_str());
    QString createXML = QString::fromUtf8(createArg.getValue().c_str());    
    QString resolve_type = QString::fromUtf8(resolveArg.getValue().c_str());
    QString encryption_key = QString::fromUtf8(encryptArg.getValue().c_str());
    if (encryption_key == "")
    {        
        encryption_key = getRandomHex(32);
    }

    QString mainTable = QString::fromUtf8(tableArg.getValue().c_str());
    QString mapDir = QString::fromUtf8(mapArg.getValue().c_str());
    QString outputDir = QString::fromUtf8(outArg.getValue().c_str());

    if (key != "" && value == "")
    {
        log_error("You need to specify key and value");
        exit(1);
    }


    if (key == "" && value != "")
    {
        log_error("You need to specify key and value");
        exit(1);
    }

    if (likeODKCollect && (key == ""))
    {
        log_error("You cannot use ODK format with more than one result. You need to specify key and value");
        exit(1);
    }
    if (likeODKCollect && resolve_type != "1")
    {
        log_error("You cannot use ODK format with resolving labels");
        exit(1);
    }

    mainClass *task = new mainClass(&app);
    task->setParameters(host,port,user,pass,schema,createXML,protectSensitive,tmpDir,encryption_key,mapDir,outputDir,mainTable, resolve_type, key, value, separator, likeODKCollect);
    QObject::connect(task, SIGNAL(finished()), &app, SLOT(quit()));
    QTimer::singleShot(0, task, SLOT(run()));
    app.exec();
    return task->returnCode;
}
