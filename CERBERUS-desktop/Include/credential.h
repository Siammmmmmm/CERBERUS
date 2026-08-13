#ifndef CREDENTIAL_H
#define CREDENTIAL_H

#include <QString>
#include <QtTypes>
#include <qdatetime.h>
struct credential
{
    quint16 slot_idx = 0;
    QDate time_modified;
    QDate time_created;
    quint8 flags = 0;
    QString site;
    QString url;
    QString email;
    QString notes;
};

struct password{
    quint8 len = 0;
    QString pass;
};

#endif // CREDENTIAL_H
