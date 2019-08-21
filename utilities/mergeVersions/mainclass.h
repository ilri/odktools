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

#ifndef MAINCLASS_H
#define MAINCLASS_H

#include <QObject>
#include "generic.h"

class mainClass : public QObject
{
    Q_OBJECT
public:
    explicit mainClass(QObject *parent = nullptr);
    int returnCode;
signals:
    void finished();
public slots:
    void run();
    void setParameters(QString createA, QString createB, QString insertA, QString insertB, QString createC, QString insertC, QString diffCreate, QString diffInsert, QString outputType, QList<TignoreTableValues> toIgnore);
private:
    QString a_createXML;
    QString b_createXML ;
    QString a_insertXML;
    QString b_insertXML;
    QString c_createXML;
    QString c_insertXML ;
    QString d_createSQL;
    QString d_insertSQL;
    QString output_type;
    void log(QString message);
    QList<TignoreTableValues> valuesToIgnore;
};

#endif // MAINCLASS_H
