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
    title = title + " ********************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.0");
    TCLAP::ValueArg<std::string> hostArg("H","host","MySQL Host. Default: localhost",false,"localhost","string");
    TCLAP::ValueArg<std::string> portArg("P","port","MySQL port. Default: 3306",false,"3306","string");
    TCLAP::ValueArg<std::string> userArg("u","user","MySQL User",true,"","string");
    TCLAP::ValueArg<std::string> passArg("p","password","MySQL Password",true,"","string");
    TCLAP::ValueArg<std::string> schemaArg("s","schema","MySQL Schema",true,"","string");
    TCLAP::ValueArg<std::string> outputArg("o","output","Output directory",false,".","string");

    //These two parameters should be removed once the external script code works

    //TCLAP::SwitchArg ignoreSwitch("g","ignore","Ignore insert in main table", cmd, false);
    //TCLAP::SwitchArg extractSwitch("e","extract","Extract number from primary key", cmd, false);

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
