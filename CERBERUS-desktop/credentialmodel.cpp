#include "credentialmodel.h"
#include <qcolor.h>
#include <qfont.h>

CredentialModel::CredentialModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

int CredentialModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_cred.size());
}

int CredentialModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant CredentialModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return QVariant{};
    }

    switch (section) {
    case Col_Fav:
        return QString{};
    case Col_Site:
        return QString{"Site"};
    case Col_Accessed:
        return QString{"Last Accessed"};
    }
    return QVariant{};
}

QVariant CredentialModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (index.row() < 0 || index.row() >= m_cred.size())) {
        return QVariant{};
    }
    const credential &c = m_cred.at(index.row());
    const bool fav = (c.flags & FLAG_FAVORITE) != 0;

    switch (role) {
    case SlotIdxRole:
        return static_cast<int>(c.slot_idx);
    case AccessedRole:
        return c.time_accessed;
    case ModifiedRole:
        return c.time_modified;
    case CreatedRole:
        return c.time_created;
    case FlagsRole:
        return static_cast<int>(c.flags);
    case SiteRole:
        return c.site;
    case UrlRole:
        return c.url;
    case EmailRole:
        return c.email;
    case NotesRole:
        return c.notes;
    case Qt::DisplayRole: {
        switch (index.column()) {
        case Col_Fav:
            if (fav) { //check bit 2
                return QString{"★"};
            } else {
                return QString{"☆"};
            }
        case Col_Site:
            return c.site;
        case Col_Accessed:
            return c.time_accessed.toString("MMM dd");
        default:
            return QVariant{};
        }
    }
    case Qt::ForegroundRole:
        switch (index.column()) {
        case Col_Fav: {
            if (fav) { //check bit 2
                return QColor("#42ff85");
            } else {
                return QColor("#5a73a0");
            }
        }
        case Col_Site:
            return QColor("#5a73a0");
        case Col_Accessed:
            return QColor("#5a73a0");

        default:
            return QVariant{};
        }
    case Qt::FontRole:
        switch (index.column()) {
        case Col_Fav: {
            QFont f;
            f.setPixelSize(24);
            return f;
        }
        case Col_Site: {
            QFont s;
            s.setPixelSize(20);
            return s;
        }
        default:
            return QVariant{};
        }
    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case Col_Fav:
            return static_cast<int>(Qt::AlignCenter);

        case Col_Accessed:
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return QVariant{};
        }
    }

    return QVariant{};
}

void CredentialModel::append(const credential &c)
{
    const int row = rowCount();
    beginInsertRows(QModelIndex(), row, row); //updates the visuals too
    m_cred.append(c);
    endInsertRows();
}

void CredentialModel::clear()
{
    beginResetModel(); //updates visuals after clearing
    m_cred.clear();
    endResetModel();
}
