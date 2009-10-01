// -*- indent-tabs-mode:nil -*-
// vim: set ts=4 sts=4 sw=4 et:
/* This file is part of the KDE project
   Copyright (C) 2000 David Faure <faure@kde.org>
   Copyright (C) 2002-2003 Alexander Kellett <lypanov@kde.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License version 2 or at your option version 3 as published by
   the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "commands.h"
#include "bookmarkmodel.h"

#include "kinsertionsort.h"

#include "toplevel.h"

#include <assert.h>

#include <kdebug.h>
#include <klocale.h>
#include <kbookmarkmanager.h>
#include <kdesktopfile.h>

#include "treeitem_p.h" // TODO REMOVEME

class KBookmarkModelInsertSentry
{
public:
    KBookmarkModelInsertSentry(const KBookmark& parent, int first, int last)
    {
        QModelIndex mParent = CurrentMgr::self()->model()->indexForBookmark(parent);
        CurrentMgr::self()->model()->beginInsertRows( mParent, first, last);

        // TODO REMOVEME use of internal class TreeItem!
        mt = static_cast<TreeItem *>(mParent.internalPointer());
        mf = first;
        ml = last;
    }
    ~KBookmarkModelInsertSentry()
    {
        mt->insertChildren(mf, ml);
        CurrentMgr::self()->model()->endInsertRows();
    }
private:
    TreeItem * mt;
    int mf, ml;
};
class KBookmarkModelRemoveSentry
{
public:
    KBookmarkModelRemoveSentry(const KBookmark& parent, int first, int last)
    {
        QModelIndex mParent = CurrentMgr::self()->model()->indexForBookmark(parent);

        CurrentMgr::self()->model()->beginRemoveRows( mParent, first, last);

        mt = static_cast<TreeItem *>(mParent.internalPointer());
        mf = first;
        ml = last;
    }
    ~KBookmarkModelRemoveSentry()
    {
        mt->deleteChildren(mf, ml);
        CurrentMgr::self()->model()->endRemoveRows();
    }
private:
    TreeItem * mt;
    int mf, ml;
};

QString KEBMacroCommand::affectedBookmarks() const
{
    const QList<K3Command *> commandList = commands();
    QList<K3Command*>::const_iterator it = commandList.constBegin();
    if ( it == commandList.constEnd() )
        return QString();
    // Need to use dynamic_cast here due to going cross-hierarchy, but it should never return 0.
    QString affectBook = dynamic_cast<IKEBCommand *>(*it)->affectedBookmarks();
    ++it;
    for( ; it != commandList.constEnd() ; ++it )
    {
        affectBook = KBookmark::commonParent( affectBook, dynamic_cast<IKEBCommand *>(*it)->affectedBookmarks());
    }
    return affectBook;
}

DeleteManyCommand::DeleteManyCommand(const QString &name, const QList<KBookmark> & bookmarks)
    : KEBMacroCommand(name)
{
    QList<KBookmark>::const_iterator it, begin;
    begin = bookmarks.constBegin();
    it = bookmarks.constEnd();
    while(begin != it)
    {
        --it;
        DeleteCommand * dcmd = new DeleteCommand( (*it).address() );
        addCommand(dcmd);
    }
}


QString CreateCommand::name() const {
    if (m_separator) {
        return i18n("Insert Separator");
    } else if (m_group) {
        return i18n("Create Folder");
    } else if (!m_originalBookmark.isNull()) {
        return i18n("Copy %1", m_mytext);
    } else {
        return i18n("Create Bookmark");
    }
}

void CreateCommand::execute()
{
    QString parentAddress = KBookmark::parentAddress(m_to);
    KBookmarkGroup parentGroup =
        CurrentMgr::bookmarkAt(parentAddress).toGroup();

    QString previousSibling = KBookmark::previousAddress(m_to);

    // kDebug() << "CreateCommand::execute previousSibling="
    //           << previousSibling << endl;
    KBookmark prev = (previousSibling.isEmpty())
        ? KBookmark(QDomElement())
        : CurrentMgr::bookmarkAt(previousSibling);

    KBookmark bk = KBookmark(QDomElement());
    // TODO use m_to.positionInParent()
    KBookmarkModelInsertSentry guard(parentGroup, KBookmark::positionInParent(m_to), KBookmark::positionInParent(m_to));

    if (m_separator) {
        bk = parentGroup.createNewSeparator();

    } else if (m_group) {
        Q_ASSERT(!m_text.isEmpty());
        bk = parentGroup.createNewFolder(m_text);
        bk.internalElement().setAttribute("folded", (m_open ? "no" : "yes"));
        if (!m_iconPath.isEmpty()) {
            bk.setIcon(m_iconPath);
        }
    } else if(!m_originalBookmark.isNull()) {
        QDomElement element = m_originalBookmark.internalElement().cloneNode().toElement();
        bk = KBookmark(element);
        parentGroup.addBookmark(bk);
    } else {
        bk = parentGroup.addBookmark(m_text, m_url, m_iconPath);
    }

    // move to right position
    parentGroup.moveBookmark(bk, prev);
    if (!(name().isEmpty()) && !parentAddress.isEmpty() ) {
        // open the parent (useful if it was empty) - only for manual commands
        Q_ASSERT( parentGroup.internalElement().tagName() != "xbel" );
        parentGroup.internalElement().setAttribute("folded", "no");
    }

    Q_ASSERT(bk.address() == m_to);
}

QString CreateCommand::finalAddress() const {
    Q_ASSERT( !m_to.isEmpty() );
    return m_to;
}

void CreateCommand::unexecute() {
    KBookmark bk = CurrentMgr::bookmarkAt(m_to);
    Q_ASSERT(!bk.isNull() && !bk.parentGroup().isNull());

    // TODO use bk.positionInParent()
    KBookmarkModelRemoveSentry(bk.parentGroup(), KBookmark::positionInParent(bk.address()), KBookmark::positionInParent(bk.address()));
    bk.parentGroup().deleteBookmark(bk);
}

QString CreateCommand::affectedBookmarks() const
{
    return KBookmark::parentAddress(m_to);
}

/* -------------------------------------- */

EditCommand::EditCommand(const QString & address, int col, const QString & newValue)
      : K3Command(), mAddress(address), mCol(col)
{
    if(mCol == 1)
    {
        const KUrl u(newValue);
        if (!(u.isEmpty() && !newValue.isEmpty())) // prevent emptied line if the currently entered url is invalid
            mNewValue = u.url(KUrl::LeaveTrailingSlash);
        else
            mNewValue = newValue;
    }
    else
        mNewValue = newValue;
}

QString EditCommand::name() const
{
    if(mCol==-1)
        return i18n("%1 Change", i18n("Icon"));
    else if(mCol==0)
        return i18n("%1 Change", i18n("Title") );
    else if(mCol==1)
        return i18n("%1 Change", i18n("URL"));
    else if(mCol==2)
        return i18n("%1 Change", i18n("Comment"));
    //Never reached
    return QString("");
}

void EditCommand::execute()
{
    KBookmark bk = CurrentMgr::bookmarkAt(mAddress);
    if(mCol==-2)
    {
        mOldValue = bk.internalElement().attribute("toolbar");
        bk.internalElement().setAttribute("toolbar", mNewValue);
    }
    else if(mCol==-1)
    {
        mOldValue = bk.icon();
        bk.setIcon(mNewValue);
    }
    else if(mCol==0)
    {
        mOldValue = bk.fullText();
        bk.setFullText(mNewValue);
    }
    else if(mCol==1)
    {
        mOldValue = bk.url().prettyUrl();
        const KUrl newUrl(mNewValue);
        if (!(newUrl.isEmpty() && !mNewValue.isEmpty())) // prevent emptied line if the currently entered url is invalid
            bk.setUrl(newUrl);
    }
    else if(mCol==2)
    {
        mOldValue = getNodeText(bk, QStringList()<<"desc");
        setNodeText(bk, QStringList()<<"desc", mNewValue);
    }
    CurrentMgr::self()->model()->emitDataChanged(bk);
}

void EditCommand::unexecute()
{

    KBookmark bk = CurrentMgr::bookmarkAt(mAddress);
    if(mCol==-2)
    {
        bk.internalElement().setAttribute("toolbar", mOldValue);
    }
    else if(mCol==-1)
    {
        bk.setIcon(mOldValue);
    }
    else if(mCol==0)
    {
        bk.setFullText(mOldValue);
    }
    else if(mCol==1)
    {
        bk.setUrl(KUrl(mOldValue));
    }
    else if(mCol==2)
    {
        setNodeText(bk, QStringList()<<"desc", mOldValue);
    }
    CurrentMgr::self()->model()->emitDataChanged(bk);
}

void EditCommand::modify(const QString &newValue)
{
    if(mCol == 1)
    {
        const KUrl u(newValue);
        if (!(u.isEmpty() && !newValue.isEmpty())) // prevent emptied line if the currently entered url is invalid
            mNewValue = u.url(KUrl::LeaveTrailingSlash);
        else
            mNewValue = newValue;
    }
    else
        mNewValue = newValue;
}

QString EditCommand::getNodeText(const KBookmark& bk, const QStringList &nodehier)
{
    QDomNode subnode = bk.internalElement();
    for (QStringList::ConstIterator it = nodehier.begin();
            it != nodehier.end(); ++it)
    {
        subnode = subnode.namedItem((*it));
        if (subnode.isNull())
            return QString();
    }
    return (subnode.firstChild().isNull())
         ? QString()
         : subnode.firstChild().toText().data();
}

QString EditCommand::setNodeText(const KBookmark& bk, const QStringList &nodehier,
                                     const QString& newValue)
{
    QDomNode subnode = bk.internalElement();
    for (QStringList::ConstIterator it = nodehier.begin();
            it != nodehier.end(); ++it)
    {
        QDomNode parent = subnode;
        subnode = subnode.namedItem((*it));
        if (subnode.isNull()) {
            subnode = bk.internalElement().ownerDocument().createElement((*it));
            parent.appendChild(subnode);
        }
    }

    if (subnode.firstChild().isNull()) {
        QDomText domtext = subnode.ownerDocument().createTextNode("");
        subnode.appendChild(domtext);
    }

    QDomText domtext = subnode.firstChild().toText();

    QString oldText = domtext.data();
    domtext.setData(newValue);
    return oldText;
}

/* -------------------------------------- */

void DeleteCommand::execute() {
    KBookmark bk = CurrentMgr::bookmarkAt(m_from);
    Q_ASSERT(!bk.isNull());

    if (m_contentOnly) {
        QDomElement groupRoot = bk.internalElement();

        QDomNode n = groupRoot.firstChild();
        while (!n.isNull()) {
            QDomElement e = n.toElement();
            if (!e.isNull()) {
                // kDebug() << e.tagName();
            }
            QDomNode next = n.nextSibling();
            groupRoot.removeChild(n);
            n = next;
        }
        return;
    }

    // TODO - bug - unparsed xml is lost after undo,
    //              we must store it all therefore

//FIXME this removes the comments, that's bad!
    if (!m_cmd) {
        if (bk.isGroup()) {
            m_cmd = new CreateCommand(
                    m_from, bk.fullText(), bk.icon(),
                    bk.internalElement().attribute("folded") == "no");
            m_subCmd = deleteAll(bk.toGroup());
            m_subCmd->execute();

        } else {
            m_cmd = (bk.isSeparator())
                ? new CreateCommand(m_from)
                : new CreateCommand(m_from, bk.fullText(),
                        bk.icon(), bk.url());
        }
    }
    m_cmd->unexecute();
}

void DeleteCommand::unexecute() {
    // kDebug() << "DeleteCommand::unexecute " << m_from;

    if (m_contentOnly) {
        // TODO - recover saved metadata
        return;
    }

    m_cmd->execute();

    if (m_subCmd) {
        m_subCmd->unexecute();
    }
}

QString DeleteCommand::affectedBookmarks() const
{
    return KBookmark::parentAddress(m_from);
}

KEBMacroCommand* DeleteCommand::deleteAll(const KBookmarkGroup & parentGroup) {
    KEBMacroCommand *cmd = new KEBMacroCommand(QString());
    QStringList lstToDelete;
    // we need to delete from the end, to avoid index shifting
    for (KBookmark bk = parentGroup.first();
            !bk.isNull(); bk = parentGroup.next(bk))
        lstToDelete.prepend(bk.address());
    for (QStringList::const_iterator it = lstToDelete.constBegin();
            it != lstToDelete.constEnd(); ++it)
        cmd->addCommand(new DeleteCommand((*it)));
    return cmd;
}

/* -------------------------------------- */

QString MoveCommand::name() const {
    return i18n("Move %1", m_mytext);
}

void MoveCommand::execute() {
    // kDebug() << "MoveCommand::execute moving from=" << m_from
    //           << " to=" << m_to << endl;


    KBookmark fromBk = CurrentMgr::self()->mgr()->findByAddress( m_from );

    m_cc = new CreateCommand(m_to, fromBk, "");
    m_cc->execute();

    m_dc = new DeleteCommand( fromBk.address() );
    m_dc->execute();
}

QString MoveCommand::finalAddress() const {
    Q_ASSERT( !m_to.isEmpty() );
    return m_to;
}

void MoveCommand::unexecute() {

    m_dc->unexecute();
    m_cc->unexecute();
}

QString MoveCommand::affectedBookmarks() const
{
    return KBookmark::commonParent(KBookmark::parentAddress(m_from), KBookmark::parentAddress(m_to));
}

/* -------------------------------------- */

class SortItem {
    public:
        SortItem(const KBookmark & bk) : m_bk(bk) {}

        bool operator == (const SortItem & s) {
            return (m_bk.internalElement() == s.m_bk.internalElement()); }

        bool isNull() const {
            return m_bk.isNull(); }

        SortItem previousSibling() const {
            return m_bk.parentGroup().previous(m_bk); }

        SortItem nextSibling() const {
            return m_bk.parentGroup().next(m_bk); }

        const KBookmark& bookmark() const {
            return m_bk; }

    private:
        KBookmark m_bk;
};

class SortByName {
    public:
        static QString key(const SortItem &item) {
            return (item.bookmark().isGroup() ? "a" : "b")
                + (item.bookmark().fullText().toLower());
        }
};

/* -------------------------------------- */

void SortCommand::execute() {
    if (commands().isEmpty()) {
        KBookmarkGroup grp = CurrentMgr::bookmarkAt(m_groupAddress).toGroup();
        Q_ASSERT(!grp.isNull());
        SortItem firstChild(grp.first());
        // this will call moveAfter, which will add
        // the subcommands for moving the items
        kInsertionSort<SortItem, SortByName, QString, SortCommand>
            (firstChild, (*this));

    } else {
        // don't execute for second time on addCommand(cmd)
        KEBMacroCommand::execute();
    }
}

void SortCommand::moveAfter(const SortItem &moveMe,
        const SortItem &afterMe) {
    QString destAddress =
        afterMe.isNull()
        // move as first child
        ? KBookmark::parentAddress(moveMe.bookmark().address()) + "/0"
        // move after "afterMe"
        : KBookmark::nextAddress(afterMe.bookmark().address());

    MoveCommand *cmd = new MoveCommand(moveMe.bookmark().address(),
            destAddress);
    cmd->execute();
    this->addCommand(cmd);
}

void SortCommand::unexecute() {
    KEBMacroCommand::unexecute();
}

QString SortCommand::affectedBookmarks() const
{
    return m_groupAddress;
}

/* -------------------------------------- */

KEBMacroCommand* CmdGen::setAsToolbar(const KBookmark &bk)
{
    KEBMacroCommand *mcmd = new KEBMacroCommand(i18n("Set as Bookmark Toolbar"));

    KBookmarkGroup oldToolbar = CurrentMgr::self()->mgr()->toolbar();
    if (!oldToolbar.isNull())
    {
        mcmd->addCommand( new EditCommand(oldToolbar.address(), -2, "no")); //toolbar
        mcmd->addCommand( new EditCommand(oldToolbar.address(), -1, "")); //icon
    }

    mcmd->addCommand( new EditCommand(bk.address(), -2, "yes"));
    mcmd->addCommand( new EditCommand(bk.address(), -1, "bookmark-toolbar"));

    return mcmd;
}

KEBMacroCommand* CmdGen::insertMimeSource(const QString &cmdName, const QMimeData *data, const QString &addr)
{
    KEBMacroCommand *mcmd = new KEBMacroCommand(cmdName);
    QString currentAddress = addr;
    QDomDocument doc;
    const KBookmark::List bookmarks = KBookmark::List::fromMimeData(data, doc);
    KBookmark::List::const_iterator it, end;
    end = bookmarks.constEnd();

    for (it = bookmarks.constBegin(); it != end; ++it)
    {
        CreateCommand *cmd = new CreateCommand(currentAddress, (*it));
        cmd->execute();
        mcmd->addCommand(cmd);
        currentAddress = KBookmark::nextAddress(currentAddress);
    }
    return mcmd;
}


//FIXME copy=true needed? what is the difference with insertMimeSource
KEBMacroCommand* CmdGen::itemsMoved(const QList<KBookmark> & items,
        const QString &newAddress, bool copy) {
    KEBMacroCommand *mcmd = new KEBMacroCommand(copy ? i18n("Copy Items")
            : i18n("Move Items"));

    QList<KBookmark>::const_iterator it, end;
    it = items.constBegin();
    end = items.constEnd();

    QString bkInsertAddr = newAddress;
    for (; it != end; ++it) {
        if (copy) {
            CreateCommand *cmd;
            cmd = new CreateCommand(
                    bkInsertAddr,
                    KBookmark((*it).internalElement()
                    .cloneNode(true).toElement()),
                    (*it).text());

            cmd->execute();
            mcmd->addCommand(cmd);

            bkInsertAddr = cmd->finalAddress();

        } else /* if (move) */ {
            QString oldAddress = (*it).address();
            if (bkInsertAddr.startsWith(oldAddress))
                continue; // trying to insert a parent into one of its children, ignore :)

            MoveCommand *cmd = new MoveCommand(oldAddress, bkInsertAddr,
                    (*it).text());
            cmd->execute();
            mcmd->addCommand(cmd);

            bkInsertAddr = cmd->finalAddress();
        }

        bkInsertAddr = KBookmark::nextAddress(bkInsertAddr);
    }

    return mcmd;
}
