#ifndef LISTMUTEX_H
#define LISTMUTEX_H

#include <QObject>
#include <QMutex>

class ListMutex : public QObject
{
    Q_OBJECT
public:
    explicit ListMutex(QObject *parent = nullptr);
    void set_total(int total);
    int get_index();
private:
    int total;
    QMutex mutex;
};

#endif // LISTMUTEX_H
