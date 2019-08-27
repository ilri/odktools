/*
Merge Versions

Copyright (C) 2019 QLands Technology Consultants.
Author: Carlos Quiros (cquiros_at_qlands.com)

Merge Versions is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

Merge Versions is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with Merge Versions.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#include <QCoreApplication>
#include <QTimer>
#include <tclap/CmdLine.h>
#include "mainclass.h"
#include "generic.h"
#include <QList>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QString title;
    title = title + "************************************************************************ ";
    title = title + " * Merge Versions. Version 2.0                                          * ";
    title = title + " * This tool merges two versions of an ODK (A & B) using the XML        * ";
    title = title + " * files created by JXFormToMySQL (create.xml and insert.xml).          * ";
    title = title + " *                                                                      * ";
    title = title + " * A is considered an incremental version of B. The tool informs        * ";
    title = title + " * of tables, variables and values in A that are not in B and adds them * ";
    title = title + " * in a file called C that will have all of B + all of A.               * ";
    title = title + " *                                                                      * ";
    title = title + " * Nomenclature:                                                        * ";
    title = title + " *   Changes allowed from B to A:                                       * ";
    title = title + " *   TNF: Table in A not found in B. It will be added to C.             * ";
    title = title + " *   FNF: Field in A not found in B. It will be addded to C.            * ";
    title = title + " *   VNF: Lookup value in A not found in B. It will be addded to C.     * ";
    title = title + " *   FIC: The field size in A is bigger than in B. It will be increased * ";
    title = title + " *        in C to match the size of in A.                               * ";
    title = title + " *   FDC: The field in A is smaller than in B. This is a decrimental    * ";
    title = title + " *        change thus will be ignored.                                  * ";
    title = title + " *   CHR: The referenced lookup table+field in A is different than in B.* ";
    title = title + " *        however the new looup table in A has the same values or more  * ";
    title = title + " *        than the previous one in B. The referenced lookup table+field * ";
    title = title + " *        will be changed in C.                                         * ";
    title = title + " *   FTC: Field type changed from int in B to varchar A. This is        * ";
    title = title + " *        allowed beause a varchar can hold numeric characters. The     * ";
    title = title + " *        field will be changed to varchar in C.                        * ";
    title = title + " *                                                                      * ";
    title = title + " *   Changes NOT allowed from B to A:                                   * ";
    title = title + " *   TNS: The table does not have the same parent table.                * ";
    title = title + " *   TWP: The parent table is not found in B.                           * ";
    title = title + " *   FNS: The field is not the same and such change will generate       * ";
    title = title + " *        iconsistencies in the data. An example is to change a         * ";
    title = title + " *        variable from categorical to continuous.                      * ";
    title = title + " *   VNS: The description of a lookup value changed from B to A. For    * ";
    title = title + " *        example 1-Male in B changed to 1-Female in A.                 * ";
    title = title + " *                                                                      * ";
    title = title + " * This tool is usefull when dealing with multiple versions of an       * ";
    title = title + " * ODK survey that must be combined in one common database.             * ";
    title = title + " *                                                                      * ";
    title = title + " * If all changes are allowed, this means that A can merge into B and   * ";
    title = title + " * a C file is created. Also a diff SQL script is issued.               * ";
    title = title + " *                                                                      * ";
    title = title + " * Decrimental changes are not taken into account because this means    * ";
    title = title + " * losing data between versions.                                        * ";
    title = title + " ************************************************************************ ";

    TCLAP::CmdLine cmd(title.toUtf8().constData(), ' ', "2.0");

    TCLAP::ValueArg<std::string> aArg("a","inputa","Input create XML file A (later)",true,"","string");
    TCLAP::ValueArg<std::string> bArg("b","inputb","Input create XML file B (former)",true,"","string");
    TCLAP::ValueArg<std::string> AArg("A","inputA","Input insert XML file A (later)",true,"","string");
    TCLAP::ValueArg<std::string> BArg("B","inputB","Input insert XML file B (former)",true,"","string");
    TCLAP::ValueArg<std::string> cArg("c","outputc","Output create XML file C",false,"./combined-create.xml","string");
    TCLAP::ValueArg<std::string> CArg("C","outputC","Output insert XML file C",false,"./combined-insert.xml","string");
    TCLAP::ValueArg<std::string> dArg("d","diffc","Output diff SQL script for create",false,"./diff-create.sql","string");
    TCLAP::ValueArg<std::string> DArg("D","diffC","Output diff SQL script for insert",false,"./diff-insert.sql","string");
    TCLAP::ValueArg<std::string> tArg("t","outputype","Output type: (h)uman readble or (m)achine readble",false,"m","string");
    TCLAP::ValueArg<std::string> oArg("o","erroroutput","Error file ",false,"./error.xml","string");
    TCLAP::ValueArg<std::string> iArg("i","ignore","Ignore changes in value descriptions. Indicated like Table1:value1,value2,...;Table2:value1,value2,..;..",false,"","string");
    TCLAP::SwitchArg saveErrorSwitch("s","saveerror","Save errors to file", cmd, false);

    cmd.add(aArg);
    cmd.add(bArg);
    cmd.add(AArg);
    cmd.add(BArg);
    cmd.add(cArg);
    cmd.add(CArg);
    cmd.add(dArg);
    cmd.add(DArg);
    cmd.add(tArg);
    cmd.add(iArg);
    cmd.add(oArg);


    //Parsing the command lines
    cmd.parse( argc, argv );

    //Getting the variables from the command
    QString a_createXML = QString::fromUtf8(aArg.getValue().c_str());
    QString b_createXML = QString::fromUtf8(bArg.getValue().c_str());
    QString a_insertXML = QString::fromUtf8(AArg.getValue().c_str());
    QString b_insertXML = QString::fromUtf8(BArg.getValue().c_str());

    QString c_createXML = QString::fromUtf8(cArg.getValue().c_str());
    QString c_insertXML = QString::fromUtf8(CArg.getValue().c_str());
    QString outputd = QString::fromUtf8(dArg.getValue().c_str());
    QString outputD = QString::fromUtf8(DArg.getValue().c_str());
    QString outputType = QString::fromUtf8(tArg.getValue().c_str());
    QString ignoreString = QString::fromUtf8(iArg.getValue().c_str());
    QString errorFile = QString::fromUtf8(oArg.getValue().c_str());

    QStringList ignoreTables;
    QList<TignoreTableValues> valuesToIgnore;
    if (ignoreString != "")
    {
        ignoreTables = ignoreString.split(";",QString::SkipEmptyParts);
        for (int pos =0; pos < ignoreTables.count(); pos++)
        {
            if (ignoreTables[pos].indexOf(":") > 0)
            {
                QStringList parts = ignoreTables[pos].split(":",QString::SkipEmptyParts);
                if (parts.count() == 2)
                {
                    int index;
                    index = -1;
                    for (int tbl = 0; tbl < valuesToIgnore.count(); tbl++)
                    {
                        if (valuesToIgnore[tbl].table == parts[0])
                        {
                            index = tbl;
                            break;
                        }
                    }
                    if (index == -1)
                    {
                        TignoreTableValues aIgnoreValue;
                        aIgnoreValue.table = parts[0];
                        aIgnoreValue.values = parts[1].split(",",QString::SkipEmptyParts);
                        valuesToIgnore.append(aIgnoreValue);
                    }
                    else
                    {
                        valuesToIgnore[index].values.append(parts[1].split(",",QString::SkipEmptyParts));
                    }
                }
            }
        }
    }
    bool saveToFile = saveErrorSwitch.getValue();


    mainClass *task = new mainClass(&a);
    task->setParameters(a_createXML,b_createXML,a_insertXML,b_insertXML,c_createXML,c_insertXML,outputd,outputD,outputType,valuesToIgnore,saveToFile,errorFile);
    QObject::connect(task, SIGNAL(finished()), &a, SLOT(quit()));

    QTimer::singleShot(0, task, SLOT(run()));

    a.exec();
    return task->returnCode;
}
