/*****************************************************************************
 * selector.hpp : Playlist source selector
 ****************************************************************************
 * Copyright (C) 2000-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _PLSEL_H_
#define _PLSEL_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QStyledItemDelegate>

#include <vlc_playlist.h>

#include "qt4.hpp"

class PlaylistWidget;

enum {
    PL_TYPE,
    ML_TYPE,
    SD_TYPE,
};

enum {
    TYPE_ROLE = Qt::UserRole,
    PPL_ITEM_ROLE,
    NAME_ROLE,
    LONGNAME_ROLE,
};

class PLSelectorDelegate : public QStyledItemDelegate
{
  private:
    QSize sizeHint ( const QStyleOptionViewItem& option, const QModelIndex& index ) const
    {
      QSize sz = QStyledItemDelegate::sizeHint( option, index );
      if( sz.height() < 25 ) sz.setHeight(25);
      return sz;
    }
};

Q_DECLARE_METATYPE( playlist_item_t *);
class PLSelector: public QTreeWidget
{
    Q_OBJECT;
public:
    PLSelector( QWidget *p, intf_thread_t *_p_intf );
    virtual ~PLSelector();
protected:
    friend class PlaylistWidget;
private:
    QStringList mimeTypes () const;
    void makeStandardItem( QTreeWidgetItem*, const QString& );
    bool dropMimeData ( QTreeWidgetItem * parent, int index, const QMimeData * data, Qt::DropAction action );
    void createItems();
    intf_thread_t *p_intf;
private slots:
    void setSource( QTreeWidgetItem *item );
signals:
    void activated( playlist_item_t * );
};

#endif
