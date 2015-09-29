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
