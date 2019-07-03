/*
JSONToCSV

Copyright (C) 2018 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

JSONToCSV is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

JSONToCSV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with JSONToCSV.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <tclap/CmdLine.h>
#include <QDirIterator>
#include <QDir>
#include <QStringList>
#include <QUuid>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QTime>

void log(QString message)
{
    QString temp;
    temp = message + "\n";
    printf("%s",temp.toLocal8Bit().data());
}

QStringList chuckFiles;

int createCSV(QString CSVFileName, QString tmpDirectory)
{
    QFile file;
    QStringList lines;
    for (int pos = 0; pos <= chuckFiles.count()-1; pos++)
    {
        file.setFileName(chuckFiles[pos]);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&file);
            in.setCodec("UTF-8");
            bool first;
            first = true;
            while (!in.atEnd())
            {
                //Dont include the first [
                if (!first)
                {
                    QString line = in.readLine();
                    lines.append(line);
                }
                else
                {
                    in.readLine();
                    first = false;
                }
            }
            lines.removeAt(lines.count()-1); //Remove the last ]
            lines.append(",");
            file.close();
        }
    }
    QDir currDir(tmpDirectory);
    QString chuckfile;
    QUuid chuckUUID=QUuid::createUuid();
    QString strChuckUUID=chuckUUID.toString().replace("{","").replace("}","").right(12);
    chuckfile = currDir.absolutePath() + currDir.separator() + strChuckUUID + ".json";

    QFile outfile(chuckfile);
    QTextStream out(&outfile);
    out.setCodec("UTF-8");
    if (outfile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        out << "[" << "\n";
        for (int pos = 0; pos <= lines.count()-2;pos++)
        {
            out << lines[pos] << "\n";
        }
        out << "]\n";
    }
    outfile.close();
    QString program;
    QStringList arguments;
    program = "json2csv";
    arguments << chuckfile;
    arguments << CSVFileName;
    QProcess *myProcess2 = new QProcess();
    myProcess2->start(program, arguments);
    myProcess2->waitForFinished(-1);
    if ((myProcess2->exitCode() > 0) || (myProcess2->error() == QProcess::FailedToStart))
    {
        if (myProcess2->error() == QProcess::FailedToStart)
        {
            log("Error: Command " +  program + " not found");
        }
        else
        {
            log("Running JSON2CSV returned error");
            QString serror = myProcess2->readAllStandardError() + myProcess2->readAllStandardOutput();
            log(serror);
            log("Running paremeters:" + arguments.join(" "));
        }
        return 1;
    }
    return 0;
}

int processChuck(QStringList chunck, QString tmpDirectory)
{
    QDir currDir(tmpDirectory);
    QString chuckfile;
    QUuid chuckUUID=QUuid::createUuid();
    QString strChuckUUID=chuckUUID.toString().replace("{","").replace("}","").right(12);
    chuckfile = currDir.absolutePath() + currDir.separator() + strChuckUUID + ".json";
    QString program = "jq";
    QStringList arguments;
    arguments << "-s";
    arguments << ".";
    for (int pos = 0; pos <= chunck.count()-1; pos++)
        arguments << chunck[pos];
    //log("Running paremeters:" + arguments.join(" "));
    QProcess *myProcess = new QProcess();
    myProcess->setStandardOutputFile(chuckfile);
    myProcess->start(program, arguments);
    myProcess->waitForFinished(-1);
    if ((myProcess->exitCode() > 0) || (myProcess->error() == QProcess::FailedToStart))
    {
        if (myProcess->error() == QProcess::FailedToStart)
        {
            log("Error: Command " +  program + " not found");
        }
        else
        {
            log("Running jQ returned error");
            QString serror = myProcess->readAllStandardError();
            log(serror);
            log("Running paremeters:" + arguments.join(" "));
        }
        return 1;
    }
    else
    {
        chuckFiles.append(chuckfile);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    QString title;
    title = title + "************************************************************************************** \n";
    title = title + " * JSON to CSV                                                                       * \n";
    title = title + " * This tool creates a CSV file based a JSON file by flattening the JSON structure   * \n";
    title = title + " * as described in                                                                   * \n";
    title = title + " * https://sunlightfoundation.com/2014/03/11/making-json-as-simple-as-a-spreadsheet/ * \n";
    title = title + " *                                                                                   * \n";
    title = title + " * This tool requires at run time:                                                   * \n";
    title = title + " * jq (https://stedolan.github.io/jq/)                                               * \n";
    title = title + " * json2csv-cpp (https://github.com/once-ler/json2csv-cpp)                           * \n";
    title = title + " *                                                                                   * \n";
    title = title + " * Though it is possible to use jq and json2csv-cpp on a command line to produce     * \n";
    title = title + " * the same result, this tool handles big numbers of files by aggregating them in    * \n";
    title = title + " * groups of 10,000 items.                                                           * \n";
    title = title + " ************************************************************************************* \n";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "1.1");

    TCLAP::ValueArg<std::string> inputArg("i","input","Input directory containing JSON files",true,"","string");
    TCLAP::ValueArg<std::string> outputArg("o","output","Output CSV file (./output.csv by default)",false,"./output.csv","string");
    TCLAP::ValueArg<std::string> tempDirArg("t","tempdir","Temporary directory (./tmp by default)",false,"./tmp","string");

    cmd.add(inputArg);
    cmd.add(outputArg);
    cmd.add(tempDirArg);

    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    QString input = QString::fromUtf8(inputArg.getValue().c_str());
    QString output = QString::fromUtf8(outputArg.getValue().c_str());
    QString tmpDirectory = QString::fromUtf8(tempDirArg.getValue().c_str());

    QDir tmpDir;
    if (!tmpDir.exists(tmpDirectory))
        tmpDir.mkdir(tmpDirectory);

    QDir inputDirectory;
    if (inputDirectory.exists(input))
    {
        inputDirectory.setPath(input);
        QDirIterator it(inputDirectory.absolutePath(), QStringList() << "*.json", QDir::Files, QDirIterator::NoIteratorFlags);
        int count;
        count = 1;
        QStringList chucks;
        QTime procTime;
        procTime.start();
        bool error;
        error = false;
        while (it.hasNext())
        {
            if (count <= 10000)
            {
                chucks << it.next();
            }
            else
            {
                count = 1;
                if (processChuck(chucks,tmpDirectory) != 0)
                    error = true;
                chucks.clear();
                chucks << it.next();
            }
            count++;
        }
        if (chucks.count() > 0)
        {
            if (processChuck(chucks,tmpDirectory) != 0)
                error = true;
        }
        if (error == false)
        {
            if (createCSV(output,tmpDirectory) == 0)
            {
                int Hours;
                int Minutes;
                int Seconds;
                int Milliseconds;

                Milliseconds = procTime.elapsed();
                Hours = Milliseconds / (1000*60*60);
                Minutes = (Milliseconds % (1000*60*60)) / (1000*60);
                Seconds = ((Milliseconds % (1000*60*60)) % (1000*60)) / 1000;
                log("Finished in " + QString::number(Hours) + " Hours," + QString::number(Minutes) + " Minutes and " + QString::number(Seconds) + " Seconds.");
            }
        }
        else
            return 1;
    }
    else
    {
        log("Input directory does not exists");
        return 1;
    }

    return 0;
}
