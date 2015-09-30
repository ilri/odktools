/*
This file is part of ODKTools.

Copyright (C) 2015 International Livestock Research Institute.
Author: Carlos Quiros (cquiros_at_qlands.com / c.f.quiros_at_cgiar.org)

ODKTools is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

ODKTools is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with ODKTools.  If not, see <http://www.gnu.org/licenses/lgpl-3.0.html>.
*/

#ifndef INSERTVALUES_H
#define INSERTVALUES_H

#include <QObject>

struct insertValueDef
{
  QString name;
  QString xmlCode;
  QString value;
  bool key;
  bool insert;
  bool multiSelect;
  QString multiSelectTable;
};
typedef insertValueDef TinsertValueDef;


class insertValues : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count)
public:
    explicit insertValues(QObject *parent = 0);
    ~insertValues();
    void insertValue(TinsertValueDef value);
    TinsertValueDef getValue(int index);
    QList<TinsertValueDef> getKeys();
    int count();

    //Callable write functions from JavaScript
    Q_INVOKABLE void insertValue(QString name, QString xmlCode, QString value, bool isKey, bool isMultiSelect = false, QString multiSelectTable = QString());
    Q_INVOKABLE void setItemName(int index, QString name);
    Q_INVOKABLE void setItemXMLCode(int index, QString xmlCode);
    Q_INVOKABLE void setItemValue(int index, QString value);
    Q_INVOKABLE void setItemIsKey(int index, bool isKey);
    Q_INVOKABLE void setItemIsMultiSelect(int index, bool isMultiSelect);
    Q_INVOKABLE void setItemMultiSelectTable(int index, QString table);
    Q_INVOKABLE void setItemToInsert(int index, bool toInsert);
    //Callable read functions from JavaScript
    Q_INVOKABLE QString itemName(int index);
    Q_INVOKABLE QString itemXMLCode(int index);
    Q_INVOKABLE QString itemValue(int index);
    Q_INVOKABLE bool itemIsKey(int index);
    Q_INVOKABLE bool itemToInsert(int index);
    Q_INVOKABLE bool itemIsMultiSelect(int index);
    Q_INVOKABLE QString itemMultiSelectTable(int index);
    //Callable read ultility functions from JavaScript
    Q_INVOKABLE bool valueIsNumber(int index);
    Q_INVOKABLE int getIndexByColumnName(QString name);

private:
    QList<TinsertValueDef> m_insertList;
};



#endif // INSERTVALUES_H
