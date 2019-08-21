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

#ifndef COMPAREINSERT_H
#define COMPAREINSERT_H

#include <QObject>
#include <QtCore>
#include <QDomElement>
#include "generic.h"
#include <QDomDocument>

class compareInsert : public QObject
{    
public:
    explicit compareInsert(QObject *parent = nullptr);
    void setFiles(QString insertA, QString insertB, QString insertC, QString diffSQL, QString outputType, QList<TignoreTableValues> toIgnore);
    int compare();
    int createCFile();
    int createDiffFile();
    void setAsParsed(QString table);
    QList<TtableDiff> getDiffs();
    QList<TcompError> getErrorList();
private:
    QString inputA;
    QString inputB;
    QString outputC;
    QString outputD;
    QString outputType;
    QList<TignoreTableValues> valuesToIgnore;
    bool fatalError;
    QDomDocument docB;
    QList<TtableDiff> diff;
    QList<TcompError> errorList;
    void log(QString message);
    void fatal(QString message);
    QDomNode findTable(QDomDocument docB,QString tableName);
    QDomNode findValue(QDomNode table,QString code);
    void addValueToDiff(QDomElement table, QDomElement field);
    void UpdateValue(QDomElement table, QDomElement field);
    void addTableToDiff(QDomElement table);
    void changeValueInC(QDomNode table, QString code, QString newDescription);
    void compareLKPTables(QDomNode table,QDomDocument &docB);
    void addDiffToTable(QString table, QString sql);
    bool ignoreChange(QString table, QString value);
};

#endif // COMPAREINSERT_H
