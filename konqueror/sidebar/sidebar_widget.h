/***************************************************************************
                               sidebar_widget.h
                             -------------------
    begin                : Sat June 2 16:25:27 CEST 2001
    copyright            : (C) 2001 Joseph Wenninger
    email                : jowenn@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef _SIDEBAR_WIDGET_
#define _SIDEBAR_WIDGET_

#include <QtCore/QTimer>

#include <QtCore/QPointer>

#include <kurl.h>
#include <ktoolbar.h>
#include <kparts/part.h>
#include <kmultitabbar.h>

#include "konqsidebarplugin.h"
#include "konqsidebariface_p.h"

#ifndef KONQSIDEBAR_EXPORT
# if defined(MAKE_KONQ_SIDEBAR_LIB)
    /* We are building this library */
#  define KONQSIDEBAR_EXPORT KDE_EXPORT
# else
   /* We are using this library */
#  define KONQSIDEBAR_EXPORT KDE_IMPORT
# endif
#endif

class QHBoxLayout;
class QSplitter;
class QStringList;
class KMenu;

class ButtonInfo: public QObject, public KonqSidebarIface
{
	Q_OBJECT
public:
	ButtonInfo(const QString& file_, class KonqSidebarIface *part, QWidget *dock_,
			const QString &url_,const QString &lib,
			const QString &dispName_, const QString &iconName_,
			QObject *parent)
		: QObject(parent), file(file_), dock(dock_), URL(url_),
		libName(lib), displayName(dispName_), iconName(iconName_), m_part(part)
		{
		copy = cut = paste = trash = del = rename =false;
		}

	~ButtonInfo() {}

	QString file;
	QPointer<QWidget> dock;
	KonqSidebarPlugin *module;
	QString URL;
	QString libName;
	QString displayName;
	QString iconName;
	bool copy;
	bool cut;
	bool paste;
	bool trash;
	bool del;
        bool rename;
        KonqSidebarIface *m_part;
	virtual bool universalMode() {return m_part->universalMode();}
};


class addBackEnd: public QObject
{
	Q_OBJECT
public:
	addBackEnd(QWidget *parent,class QMenu *addmenu, bool univeral, const QString &currentProfile, const char *name=0);
	~addBackEnd(){;}
protected Q_SLOTS:
	void aboutToShowAddMenu();
	void triggeredAddMenu(QAction* action);
	void doRollBack();

Q_SIGNALS:
	void updateNeeded();
	void initialCopyNeeded();
private:
	QPointer<class QMenu> menu;
	bool m_universal;
	QString m_currentProfile;
	QWidget *m_parent;
};

class KONQSIDEBAR_EXPORT Sidebar_Widget: public QWidget
{
	Q_OBJECT
public:
	friend class ButtonInfo;
public:
	Sidebar_Widget(QWidget *parent, KParts::ReadOnlyPart *par,
						bool universalMode,
						const QString &currentProfile);
	~Sidebar_Widget();
	bool openUrl(const class KUrl &url);
	void stdAction(const char *handlestd);
	//virtual KParts::ReadOnlyPart *getPart();
	KParts::BrowserExtension *getExtension();
        virtual QSize sizeHint() const;

public Q_SLOTS:
	void addWebSideBar(const KUrl& url, const QString& name);

protected:
	void customEvent(QEvent* ev);
    //void resizeEvent(QResizeEvent* ev);
	virtual bool eventFilter(QObject*,QEvent*);
	virtual void mousePressEvent(QMouseEvent*);

protected Q_SLOTS:
	void showHidePage(int value);
	void createButtons();
	void updateButtons();
	void finishRollBack();
	void aboutToShowConfigMenu();
	void saveConfig();

	void slotMultipleViews();
	void slotShowTabsLeft();
	void slotShowConfigurationButton();

	void slotSetName();
	void slotSetURL();
	void slotSetIcon();
	void slotRemove();

Q_SIGNALS:
	void started(KIO::Job *);
	void completed();
	void fileSelection(const KFileItemList& iems);
	void fileMouseOver(const KFileItem& item);

public:
	/* interface KonqSidebar_PluginInterface*/
	const KComponentData &getInstance();
//        virtual void showError(QString &);      for later extension
//        virtual void showMessage(QString &);    for later extension
	/* end of interface implementation */


 /* The following public slots are wrappers for browserextension fields */
public Q_SLOTS:
	void openUrlRequest( const KUrl &url, const KParts::OpenUrlArguments& args, const KParts::BrowserArguments& browserArgs );
	/* @internal
	 * ### KDE4 remove me
	 */
	void submitFormRequest(const char*,const QString&,const QByteArray&,const QString&,const QString&,const QString&);
	void createNewWindow( const KUrl &url, const KParts::OpenUrlArguments& args, const KParts::BrowserArguments& browserArgs,
             const KParts::WindowArgs &windowArgs, KParts::ReadOnlyPart **part );

  	void popupMenu( const QPoint &global, const KFileItemList &items );
	void popupMenu( const QPoint &global, const KUrl &url,
		const QString &mimeType, mode_t mode = (mode_t)-1 );
	void enableAction( const char * name, bool enabled );
    //void userMovedSplitter();

private:
    //QSplitter *splitter() const;
	bool addButton(const QString &desktoppath,int pos=-1);
	bool createView(ButtonInfo *data);
	KonqSidebarPlugin *loadModule(QWidget *par,QString &desktopName,const QString &lib_name,ButtonInfo *bi);
	void readConfig();
	void initialCopy();
	void doLayout();
	void connectModule(QObject *mod);
	void collapseExpandSidebar();
	bool doEnableActions();
	bool m_universalMode;
    //bool m_userMovedSplitter;
private:
	KParts::ReadOnlyPart *m_partParent;
	QSplitter *m_area;

	KMultiTabBar *m_buttonBar;
	QVector<ButtonInfo*> m_buttons;
	QHBoxLayout *m_layout;
	KMenu *m_buttonPopup;
	QAction* m_buttonPopupTitle, *m_showTabLeft;
	QMenu *m_menu;
	QPointer<ButtonInfo> m_activeModule;
	QPointer<ButtonInfo> m_currentButton;

	KConfigGroup *m_config;
	QTimer m_configTimer;

	KUrl m_storedUrl;
	int m_savedWidth;
	int m_latestViewed;

	bool m_hasStoredUrl;
	bool m_singleWidgetMode;
	bool m_showTabsLeft;
	bool m_hideTabs;
	bool m_showExtraButtons;
	bool m_somethingVisible;
	bool m_noUpdate;
	bool m_initial;
        
        QAction *m_multiViews;
        QAction *m_showConfigButton;

	QString m_path;
	QString m_relPath;
	QString m_currentProfile;
	QStringList m_visibleViews; // The views that are actually open
	QStringList m_openViews; // The views that should be opened

Q_SIGNALS:
	void panelHasBeenExpanded(bool);
};

#endif
