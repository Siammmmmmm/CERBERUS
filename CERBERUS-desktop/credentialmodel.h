#ifndef CREDENTIALMODEL_H
#define CREDENTIALMODEL_H
#include <QAbstractTableModel>
#include <QDate>
#include <QObject>
#include <QVector>
#include "credential.h"
#include "protocol.h"

class CredentialModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { Col_Fav = 0, Col_Site, Col_Modified, ColumnCount };
    enum Role {
        SlotIdxRole = Qt::UserRole + 1,
        ModifiedRole,
        CreatedRole,
        FlagsRole,
        SiteRole,
        UrlRole,
        EmailRole,
        NotesRole
    };
    explicit CredentialModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant extracted() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void append(const credential &metadata);
    void clear();

private:
    QVector<credential> m_cred;
};
#endif // CREDENTIALMODEL_H