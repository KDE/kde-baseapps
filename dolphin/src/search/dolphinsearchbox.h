/***************************************************************************
 *   Copyright (C) 2010 by Peter Penz <peter.penz19@gmail.com>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#ifndef DOLPHINSEARCHBOX_H
#define DOLPHINSEARCHBOX_H

#include <KUrl>
#include <QList>
#include <QWidget>

class DolphinFacetsWidget;
class KLineEdit;
class KSeparator;
class QToolButton;
class QScrollArea;
class QLabel;
class QVBoxLayout;

/**
 * @brief Input box for searching files with or without Baloo.
 *
 * The widget allows to specify:
 * - Where to search: Everywhere or below the current directory
 * - What to search: Filenames or content
 *
 * If Baloo is available and the current folder is indexed, further
 * options are offered.
 */
class DolphinSearchBox : public QWidget {
    Q_OBJECT

public:
    explicit DolphinSearchBox(QWidget* parent = 0);
    virtual ~DolphinSearchBox();

    /**
     * Sets the text that should be used as input for
     * searching.
     */
    void setText(const QString& text);

    /**
     * Returns the text that should be used as input
     * for searching.
     */
    QString text() const;

    /**
     * Sets the current path that is used as root for
     * searching files, if "From Here" has been selected.
     */
    void setSearchPath(const KUrl& url);
    KUrl searchPath() const;

    /** @return URL that will start the searching of files. */
    KUrl urlForSearching() const;

    /**
     * Selects the whole text of the search box.
     */
    void selectAll();

    /**
     * @param readOnly If set to true the searchbox cannot be modified
     *                 by the user and acts as visual indicator for
     *                 an externally triggered search query.
     * @param query    If readOnly is true this URL will be used
     *                 to show a human readable information about the
     *                 query.
     */
    void setReadOnly(bool readOnly, const KUrl& query = KUrl());
    bool isReadOnly() const;

    /**
     * Set the search box to the active mode, if \a active
     * is true. The active mode is default. The inactive mode only differs
     * visually from the active mode, no change of the behavior is given.
     *
     * Using the search box in the inactive mode is useful when having split views,
     * where the inactive view is indicated by an search box visually.
     */
    void setActive(bool active);

    /**
     * @return True, if the search box is in the active mode.
     * @see    DolphinSearchBox::setActive()
     */
    bool isActive() const;

protected:
    virtual bool event(QEvent* event);
    virtual void showEvent(QShowEvent* event);
    virtual void keyReleaseEvent(QKeyEvent* event);
    virtual bool eventFilter(QObject* obj, QEvent* event);

signals:
    /**
     * Is emitted when a searching should be triggered.
     */
    void searchRequest();

    /**
     * Is emitted when the user has changed a character of
     * the text that should be used as input for searching.
     */
    void searchTextChanged(const QString& text);

    void returnPressed(const QString& text);

    /**
     * Emitted as soon as the search box should get closed.
     */
    void closeRequest();

    /**
     * Is emitted, if the searchbox has been activated by
     * an user interaction
     * @see DolphinSearchBox::setActive()
     */
    void activated();

private slots:
    void emitSearchRequest();
    void emitCloseRequest();
    void slotConfigurationChanged();
    void slotSearchTextChanged(const QString& text);
    void slotReturnPressed(const QString& text);
    void slotFacetsButtonToggled();
    void slotFacetChanged();

private:
    void initButton(QToolButton* button);
    void loadSettings();
    void saveSettings();
    void init();

    /**
     * @return URL that represents the Baloo query for starting the search.
     */
    KUrl balooUrlForSearching() const;

    void applyReadOnlyState();

    void updateFacetsToggleButton();
private:
    bool m_startedSearching;
    bool m_readOnly;
    bool m_active;

    QVBoxLayout* m_topLayout;

    QLabel* m_searchLabel;
    KLineEdit* m_searchInput;
    QScrollArea* m_optionsScrollArea;
    QToolButton* m_fileNameButton;
    QToolButton* m_contentButton;
    KSeparator* m_separator;
    QToolButton* m_fromHereButton;
    QToolButton* m_everywhereButton;
    QToolButton* m_facetsToggleButton;
    DolphinFacetsWidget* m_facetsWidget;

    KUrl m_searchPath;
    KUrl m_readOnlyQuery;

    QTimer* m_startSearchTimer;
};

#endif
