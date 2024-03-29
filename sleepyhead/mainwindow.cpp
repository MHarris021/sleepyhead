/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * MainWindow Implementation
 *
 * Copyright (c) 2011-2014 Mark Watkins <jedimark@users.sourceforge.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the Linux
 * distribution for more details. */

#include <QGLFormat>
#include <QFileDialog>
#include <QMessageBox>
#include <QResource>
#include <QProgressBar>
#include <QWebHistory>
#include <QWebFrame>
#include <QNetworkRequest>
#include <QDesktopServices>
#include <QNetworkReply>
#include <QTimer>
#include <QSettings>
#include <QPixmap>
#include <QDesktopWidget>
#include <QListView>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QProcess>
#include <QFontMetrics>
#include <QTextDocument>
#include <QTranslator>
#include <cmath>

// Custom loaders that don't autoscan..
#include <SleepLib/loader_plugins/zeo_loader.h>

#ifndef REMSTAR_M_SUPPORT
#include <SleepLib/loader_plugins/mseries_loader.h>
#endif

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "newprofile.h"
#include "exportcsv.h"
#include "SleepLib/schema.h"
#include "Graphs/glcommon.h"
#include "UpdaterWindow.h"
#include "SleepLib/calcs.h"
#include "version.h"

#include "reports.h"
#include "summary.h"

QProgressBar *qprogress;
QLabel *qstatus;
QLabel *qstatus2;
QStatusBar *qstatusbar;

extern Profile * profile;

void MainWindow::Log(QString s)
{

    if (!strlock.tryLock())
        return;

//  strlock.lock();
    QString tmp=QString("%1: %2").arg(logtime.elapsed(),5,10,QChar('0')).arg(s);

    logbuffer.append(tmp); //QStringList appears not to be threadsafe
    strlock.unlock();

    strlock.lock();
    // only do this in the main thread?
    for (int i=0;i<logbuffer.size();i++) {
        ui->logText->appendPlainText(logbuffer[i]);
        fprintf(stderr,"%s\n",logbuffer[i].toLocal8Bit().constData());
    }
    logbuffer.clear();
    strlock.unlock();

    //loglock.unlock();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    Q_ASSERT(p_profile!=NULL);

    logtime.start();
    ui->setupUi(this);

    QString version=FullVersionString;
    if (QString(GIT_BRANCH)!="master") version+=QString(" ")+QString(GIT_BRANCH);
    this->setWindowTitle(STR_TR_SleepyHead+QString(" v%1 ("+tr("Profile")+": %2)").arg(version).arg(PREF[STR_GEN_Profile].toString()));
    //ui->tabWidget->setCurrentIndex(1);

#ifdef Q_OS_MAC
#if(QT_VERSION<QT_VERSION_CHECK(5,0,0))
    // Disable Screenshot on Mac Platform,as it doesn't work in Qt4, and the system provides this functionality anyway.
   ui->action_Screenshot->setEnabled(false);
#endif
#endif

    overview=NULL;
    daily=NULL;
    oximetry=NULL;
    prefdialog=NULL;

    m_inRecalculation=false;
    m_restartRequired=false;
    // Initialize Status Bar objects
    qstatusbar=ui->statusbar;
    qprogress=new QProgressBar(this);
    qprogress->setMaximum(100);
    qstatus2=new QLabel(tr("Welcome"),this);
    qstatus2->setFrameStyle(QFrame::Raised);
    qstatus2->setFrameShadow(QFrame::Sunken);
    qstatus2->setFrameShape(QFrame::Box);
    //qstatus2->setMinimumWidth(100);
    qstatus2->setMaximumWidth(100);
    qstatus2->setAlignment(Qt::AlignRight |Qt::AlignVCenter);
    qstatus=new QLabel("",this);
    qprogress->hide();
    ui->statusbar->setMinimumWidth(200);
    ui->statusbar->addPermanentWidget(qstatus,0);
    ui->statusbar->addPermanentWidget(qprogress,1);
    ui->statusbar->addPermanentWidget(qstatus2,0);

    ui->actionDebug->setChecked(PROFILE.general->showDebug());

    if (!PROFILE.general->showDebug()) {
        ui->logText->hide();
    }

#ifdef Q_OS_MAC
    PROFILE.appearance->setAntiAliasing(false);
#endif
    ui->action_Link_Graph_Groups->setChecked(PROFILE.general->linkGroups());

    first_load=true;

    // Using the dirty registry here. :(
    QSettings settings(getDeveloperName(), getAppName());

    // Load previous Window geometry
    this->restoreGeometry(settings.value("MainWindow/geometry").toByteArray());

    daily=new Daily(ui->tabWidget,NULL);
    ui->tabWidget->insertTab(1,daily,STR_TR_Daily);


    // Start with the Summary Tab
    ui->tabWidget->setCurrentWidget(ui->summaryTab); // setting this to daily shows the cube during loading..

    // Nifty Notification popups in System Tray (uses Growl on Mac)
    if (QSystemTrayIcon::isSystemTrayAvailable() && QSystemTrayIcon::supportsMessages()) {
        systray=new QSystemTrayIcon(QIcon(":/icons/bob-v3.0.png"),this);
        systray->show();
        systraymenu=new QMenu(this);
        systray->setContextMenu(systraymenu);
        QAction *a=systraymenu->addAction(STR_TR_SleepyHead+" v"+VersionString);
        a->setEnabled(false);
        systraymenu->addSeparator();
        systraymenu->addAction(tr("&About"),this,SLOT(on_action_About_triggered()));
        systraymenu->addAction(tr("Check for &Updates"),this,SLOT(on_actionCheck_for_Updates_triggered()));
        systraymenu->addSeparator();
        systraymenu->addAction(tr("E&xit"),this,SLOT(close()));
    } else { // if not available, the messages will popup in the taskbar
        systray=NULL;
        systraymenu=NULL;
    }
    ui->toolBox->setCurrentIndex(0);
    daily->graphView()->redraw();

    if (PROFILE.cpap->AHIWindow() < 30.0) {
        PROFILE.cpap->setAHIWindow(60.0);
    }

    ui->recordsBox->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    ui->summaryView->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    ui->webView->page()->setLinkDelegationPolicy(QWebPage::DelegateExternalLinks);
    ui->bookmarkView->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);

    QString loadingtxt="<HTML><body style='text-align: center; vertical-align: center'><table width='100%' height='100%'><tr><td align=center><h1>"+tr("Loading...")+"</h1></td></tr></table></body></HTML>";
    ui->summaryView->setHtml(loadingtxt);
    on_tabWidget_currentChanged(0);

#ifndef REMSTAR_M_SUPPORT
    ui->actionImport_RemStar_MSeries_Data->setVisible(false);
#endif
}
extern MainWindow *mainwin;
MainWindow::~MainWindow()
{
    if (systraymenu) delete systraymenu;
    if (systray) delete systray;

    // Save current window position
    QSettings settings(getDeveloperName(), getAppName());
    settings.setValue("MainWindow/geometry", saveGeometry());

    if (daily) {
        daily->close();
        delete daily;
    }
    if (overview) {
        overview->close();
        delete overview;
    }
    if (oximetry) {
        oximetry->close();
        delete oximetry;
    }

    // Trash anything allocated by the Graph objects
    DoneGraphs();

    // Shutdown and Save the current User profile
    Profiles::Done();

    mainwin=NULL;
    delete ui;
}


void MainWindow::Notify(QString s,QString title,int ms)
{
    if (systray) {
        systray->showMessage(title,s,QSystemTrayIcon::Information,ms);
    } else {
        ui->statusbar->showMessage(s,ms);
    }
}

void MainWindow::Startup()
{
    qDebug() << STR_TR_SleepyHeadVersion.toLocal8Bit().data() << "built with Qt"<< QT_VERSION_STR << "on" << __DATE__ << __TIME__;
    qstatus->setText(tr("Loading Data"));
    qprogress->show();
    //qstatusbar->showMessage(tr("Loading Data"),0);

    // profile is a global variable set in main after login
    PROFILE.LoadMachineData();

    SnapshotGraph=new gGraphView(this,daily->graphView());

    // the following are platform overides for the UsePixmapCache preference settings
#ifdef Q_OS_MAC
    //Mac needs this to be able to offscreen render
    SnapshotGraph->setUsePixmapCache(true);
#else
    //Windows & Linux barfs when offscreen rendering with pixmap cached text
    SnapshotGraph->setUsePixmapCache(false);
#endif

    SnapshotGraph->setFormat(daily->graphView()->format());
    //SnapshotGraph->setMaximumSize(1024,512);
    //SnapshotGraph->setMinimumSize(1024,512);
    SnapshotGraph->hide();

    overview=new Overview(ui->tabWidget,daily->graphView());
    ui->tabWidget->insertTab(2,overview,STR_TR_Overview);
    if (PROFILE.oxi->oximetryEnabled()) {
        oximetry=new Oximetry(ui->tabWidget,daily->graphView());
        ui->tabWidget->insertTab(3,oximetry,STR_TR_Oximetry);
    }


    ui->tabWidget->setCurrentWidget(ui->summaryTab);
    if (daily) daily->ReloadGraphs();
    if (overview) overview->ReloadGraphs();
    qprogress->hide();
    qstatus->setText("");
    on_summaryButton_clicked();

}

void MainWindow::on_action_Import_Data_triggered()
{
    if (m_inRecalculation) {
        Notify(tr("Access to Import has been blocked while recalculations are in progress."));
        return;

    }

    QStringList importLocations;
    {
        QString filename=PROFILE.Get("{DataFolder}/ImportLocations.txt");
        QFile file(filename);
        file.open(QFile::ReadOnly);
        QTextStream textStream(&file);
        while (1) {
            QString line = textStream.readLine();
             if (line.isNull())
                 break;
             else if (line.isEmpty())
                 continue;
             else {
                 importLocations.append(line);
             }
        }
        file.close();
    }

    //bool addnew=false;
    QString newdir;

    bool asknew=false;
    if (importLocations.size()==0) {
        asknew=true;
    } else {
        int res=QMessageBox::question(this,tr("Import from where?"),tr("Do you just want to Import from the usual (remembered) locations?\n"),tr("The Usual"),tr("New Location"),tr("Cancel"),0,2);
        if (res==1) {
            asknew=true;
        }
        if (res==2) return;
    }
    if (asknew) {
        //mainwin->Notify("Please remember to point the importer at the root folder or drive letter of your data-card, and not a subfolder.","Import Reminder",8000);
    }

    QStringList importFrom;

    if (asknew) {
        QFileDialog w;
        w.setFileMode(QFileDialog::Directory);
        w.setOption(QFileDialog::ShowDirsOnly, true);

#if defined(Q_OS_MAC) && (QT_VERSION_CHECK(4,8,0) > QT_VERSION)
        // Fix for tetragon, 10.6 barfs up Qt's custom dialog
        w.setOption(QFileDialog::DontUseNativeDialog,true);
#else
        w.setOption(QFileDialog::DontUseNativeDialog,false);

        QListView *l = w.findChild<QListView*>("listView");
        if (l) {
            l->setSelectionMode(QAbstractItemView::MultiSelection);
        }
        QTreeView *t = w.findChild<QTreeView*>();
        if (t) {
            t->setSelectionMode(QAbstractItemView::MultiSelection);
        }
#endif
        if (w.exec()!=QDialog::Accepted) {
            return;
        }
        for (int i=0;i<w.selectedFiles().size();i++) {
            QString newdir=w.selectedFiles().at(i);
            if (!importFrom.contains(newdir)) {
                importFrom.append(newdir);
                //addnew=true;
            }
        }
    } else importFrom=importLocations;

    int successful=false;

    QStringList goodlocations;
    for (int i=0;i<importFrom.size();i++) {
        QString dir=importFrom[i];
        if (!dir.isEmpty()) {
            qprogress->setValue(0);
            qprogress->show();
            qstatus->setText(tr("Importing Data"));
            int c=PROFILE.Import(dir);
            qDebug() << "Finished Importing data" << c;
            if (c) {
                if (!importLocations.contains(dir))
                    goodlocations.push_back(dir);
                successful=true;
            }
            qstatus->setText("");
            qprogress->hide();
        }
    }
    if (successful) {
        PROFILE.Save();
        if (overview) overview->ReloadGraphs();
        on_summaryButton_clicked();
        if (daily) daily->ReloadGraphs();
        if ((goodlocations.size()>0) && (QMessageBox::question(this,tr("Remember this Location?"),tr("Would you like to remember this import location for next time?")+"\n"+newdir,QMessageBox::Yes,QMessageBox::No)==QMessageBox::Yes)) {
            for (int i=0;i<goodlocations.size();i++) {
                importLocations.push_back(goodlocations[i]);
            }
            QString filename=PROFILE.Get("{DataFolder}/ImportLocations.txt");
            QFile file(filename);
            file.open(QFile::WriteOnly);
            QTextStream ts(&file);
            for (int i=0;i<importLocations.size();i++) {
                ts << importLocations[i] << endl;
               //file.write(importLocations[i].toUtf8());
            }
            file.close();
        }
    } else {
        mainwin->Notify(tr("Import Problem\n\nCouldn't find any new Machine Data at the locations given"));
    }
}
QMenu * MainWindow::CreateMenu(QString title)
{
    QMenu *menu=new QMenu(title,ui->menubar);
    ui->menubar->insertMenu(ui->menu_Help->menuAction(),menu);
    return menu;
}

void MainWindow::on_action_Fullscreen_triggered()
{
    if (ui->action_Fullscreen->isChecked())
        this->showFullScreen();
    else
        this->showNormal();
}

void MainWindow::setRecBoxHTML(QString html)
{
    ui->recordsBox->setHtml(html);
}

class MyStatsPage:public QWebPage
{
public:
    MyStatsPage(QObject *parent);
    virtual ~MyStatsPage();
protected:
    //virtual void javaScriptConsoleMessage(const QString & message, int lineNumber, const QString & sourceID);
    virtual void javaScriptAlert ( QWebFrame * frame, const QString & msg );
};
MyStatsPage::MyStatsPage(QObject *parent)
:QWebPage(parent)
{
}
MyStatsPage::~MyStatsPage()
{
}

void MyStatsPage::javaScriptAlert(QWebFrame * frame, const QString & msg)
{
    Q_UNUSED(frame);
    mainwin->sendStatsUrl(msg);
}

void MainWindow::on_homeButton_clicked()
{
    ui->webView->setUrl(QUrl("qrc:/docs/index.html"));
}


void MainWindow::on_summaryButton_clicked()
{
    QString html=Summary::GenerateHTML();

    updateFavourites();

    //QWebFrame *frame=ui->summaryView->page()->currentFrame();
    //frame->addToJavaScriptWindowObject("mainwin",this);
    //ui->summaryView->setHtml(html);
    MyStatsPage *page=new MyStatsPage(this);
    page->currentFrame()->setHtml(html);
    ui->summaryView->setPage(page);
//    connect(ui->summaryView->page()->currentFrame(),SIGNAL(javaScriptWindowObjectCleared())
//    QString file="qrc:/docs/index.html";
//    QUrl url(file);
//    ui->webView->setUrl(url);
}


void MainWindow::updateFavourites()
{
    QDate date=PROFILE.LastDay(MT_JOURNAL);
    if (!date.isValid())
        return;

    QString html="<html><head><style type='text/css'>"
        "p,a,td,body { font-family: '"+QApplication::font().family()+"'; }"
        "p,a,td,body { font-size: "+QString::number(QApplication::font().pointSize() + 2)+"px; }"
        "a:link,a:visited { color: inherit; text-decoration: none; }" //font-weight: normal;
        "a:hover { background-color: inherit; color: white; text-decoration:none; font-weight: bold; }"
        "</style></head><body>"
        "<table width=100% cellpadding=2 cellspacing=0>";

    do {
        Day * journal=PROFILE.GetDay(date,MT_JOURNAL);
        if (journal) {
            if (journal->size()>0) {
                Session *sess=(*journal)[0];
                QString tmp;
                bool filtered=!bookmarkFilter.isEmpty();
                bool found=!filtered;
                if (sess->settings.contains(Bookmark_Start)) {
                    //QVariantList start=sess->settings[Bookmark_Start].toList();
                    //QVariantList end=sess->settings[Bookmark_End].toList();
                    QStringList notes=sess->settings[Bookmark_Notes].toStringList();
                    if (notes.size()>0) {
                        tmp+=QString("<tr><td><b><a href='daily=%1'>%2</a></b><br/>")
                                .arg(date.toString(Qt::ISODate))
                                .arg(date.toString());

                        tmp+="<list>";

                        for (int i=0;i<notes.size();i++) {
                            //QDate d=start[i].toDate();
                            QString note=notes[i];
                            if (filtered && note.contains(bookmarkFilter,Qt::CaseInsensitive))
                                found=true;
                            tmp+="<li>"+note+"</li>";
                        }
                        tmp+="</list></td>";
                    }
                }
                if (found) html+=tmp;
            }
        }

        date=date.addDays(-1);
    } while (date>=PROFILE.FirstDay(MT_JOURNAL));
    html+="</table></body></html>";
    ui->bookmarkView->setHtml(html);
}

void MainWindow::on_backButton_clicked()
{
    ui->webView->back();
}

void MainWindow::on_forwardButton_clicked()
{
    ui->webView->forward();
}

void MainWindow::on_webView_urlChanged(const QUrl &arg1)
{
    ui->urlBar->setEditText(arg1.toString());
}

void MainWindow::on_urlBar_activated(const QString &arg1)
{
    QUrl url(arg1);
    ui->webView->setUrl(url);
}

void MainWindow::on_dailyButton_clicked()
{
    ui->tabWidget->setCurrentWidget(daily);
    daily->RedrawGraphs();
    qstatus2->setText(STR_TR_Daily);
}
void MainWindow::JumpDaily()
{
    on_dailyButton_clicked();
}

void MainWindow::on_overviewButton_clicked()
{
    ui->tabWidget->setCurrentWidget(overview);
    qstatus2->setText(STR_TR_Overview);
}

void MainWindow::on_webView_loadFinished(bool arg1)
{
    Q_UNUSED(arg1);
    qprogress->hide();
    if (first_load) {
        QTimer::singleShot(0,this,SLOT(Startup()));
        first_load=false;
    } else {
        qstatus->setText("");
    }
    ui->backButton->setEnabled(ui->webView->history()->canGoBack());
    ui->forwardButton->setEnabled(ui->webView->history()->canGoForward());

    connect(ui->webView->page(),SIGNAL(linkHovered(QString,QString,QString)),this,SLOT(LinkHovered(QString,QString,QString)));
}

void MainWindow::on_webView_loadStarted()
{
    disconnect(ui->webView->page(),SIGNAL(linkHovered(QString,QString,QString)),this,SLOT(LinkHovered(QString,QString,QString)));
    if (!first_load) {
        qstatus->setText(tr("Loading"));
        qprogress->reset();
        qprogress->show();
    }
}

void MainWindow::on_webView_loadProgress(int progress)
{
    qprogress->setValue(progress);
}

void MainWindow::aboutBoxLinkClicked(const QUrl & url)
{
    QDesktopServices::openUrl(url);
}

void MainWindow::on_action_About_triggered()
{

    QString gitrev=QString(GIT_REVISION);
    if (!gitrev.isEmpty()) gitrev="Revision: "+gitrev+" ("+QString(GIT_BRANCH)+" branch)";

//    "<style type=\"text/css\">body { margin:0; padding:0; } html, body, #bg { height:100%; width:100% } #bg { position: absolute; left:0; right:0; bottom:0; top:0; overflow:hidden; z-index:1; } #bg img { width:100%; min-width:100%; min-height:100%; } #content { z-index:0; }</style><body><div id=\"bg\"> <img style=\"display:block;\" src=\"qrc:/icons/Bob Strikes Back.png\"></div><div id=\"content\">"
    QString msg=QString(
"<span style=\"color:#000000; font-weight:600; vertical-align:middle;\">"
"<table width=100%><tr><td>"
"<p><h1>"+STR_TR_SleepyHead+" v%1.%2.%3-%4 (%8)</h1></p><font color=black><p>"+tr("Build Date")+": %5 %6<br/>%7<br/>"+tr("Data Folder Location")+": %9<hr/>"+
tr("Copyright")+" &copy;2013 Mark Watkins (jedimark) <br/> \n"+
tr("This software is released under the GNU Public License v3.0<br/>")+
"<hr><p>"+tr("SleepyHead Project Page")+": <a href=\"http://sourceforge.net/projects/sleepyhead\">http://sourceforge.net/projects/sleepyhead</a><br/>"+
tr("SleepyHead Wiki")+": <a href=\"http://sleepyhead.sourceforge.net\">http://sleepyhead.sourceforge.net</a><br/>"+
tr("Authors Twitter Feed")+": <a href=\"http://twitter.com/jedimark64\">http://twitter.com/jedimark64</a></p>"+
"</td><td><img src=\"qrc:/icons/Bob Strikes Back.png\" width=150px height=150px></td></tr><tr colspan><td colspan=2>"+
tr("<p>The author wishes to express thanks to James Marshall and Rich Freeman for their assistance with this project.</p>")+
"<hr><p><i>"+tr("This software comes with absolutely no warranty, either express of implied.")+" "+
tr("It comes with no guarantee of fitness for any particular purpose.")+" "+
tr("No guarantees are made regarding the accuracy of any data this program displays.")+"</i></p>"
"<p><i>"+tr("This is NOT medical software, it is merely a research tool that provides a visual interpretation of data recorded by supported devices.")+
tr("This software is NOT suitable for medical diagnosis, CPAP complaince reporting and other similar purposes.")+"</i></p>"
"<p><i>"+tr("The author and any associates of his accept NO responsibilty for damages, issues or non-issues resulting from the use or mis-use of this software.")+"<br/>"+
tr("Use this software entirely at your own risk.")+"</i></p>"
"<hr><p><font color=\"blue\">"+tr("If you find this free software to be of use, please consider supporting the development efforts by making a paypal donation to the Author")+"</font></p>"
"</font></td></tr></table></span>"
).arg(major_version).arg(minor_version).arg(revision_number).arg(release_number).arg(__DATE__).arg(__TIME__).arg(gitrev).arg(ReleaseStatus).arg(QDir::toNativeSeparators(GetAppRoot()));
    //"</div></body>"

     QDialog aboutbox;
     aboutbox.setWindowTitle(QObject::tr("About SleepyHead"));


     QVBoxLayout layout(&aboutbox);

     QWebView webview(&aboutbox);

     webview.setHtml(msg);
     webview.page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
     connect(&webview,SIGNAL(linkClicked(const QUrl &)),SLOT(aboutBoxLinkClicked(const QUrl &)));

     layout.insertWidget(0,&webview,1);

     QHBoxLayout layout2(&aboutbox);
     layout.insertLayout(1,&layout2,1);
     QPushButton okbtn(QObject::tr("&Close"),&aboutbox);
     aboutbox.connect(&okbtn,SIGNAL(clicked()),SLOT(reject()));
     layout2.insertWidget(1,&okbtn,1);

     QPushButton donatebtn(QObject::tr("&Donate"),&aboutbox);
     aboutbox.connect(&donatebtn,SIGNAL(clicked()),SLOT(accept())); //hack this button to use the accepted slot, so clicking x closes like it shouldß
     layout2.insertWidget(1,&donatebtn,1);

     QApplication::processEvents(); // MW: Needed on Mac, as the html has to finish loading


     if (aboutbox.exec()==QDialog::Accepted) {
         QDesktopServices::openUrl(QUrl("http://sourceforge.net/p/sleepyhead/donate"));
        //spawn browser with paypal site.
     }

     disconnect(&webview,SIGNAL(linkClicked(const QUrl &)),this,SLOT(aboutBoxLinkClicked(const QUrl &)));
}

void MainWindow::on_actionDebug_toggled(bool checked)
{
    PROFILE.general->setShowDebug(checked);
    if (checked) {
        ui->logText->show();
    } else {
        ui->logText->hide();
    }
}

void MainWindow::on_action_Reset_Graph_Layout_triggered()
{
    if (daily && (ui->tabWidget->currentWidget()==daily)) daily->ResetGraphLayout();
    if (overview && (ui->tabWidget->currentWidget()==overview)) overview->ResetGraphLayout();
}

void MainWindow::on_action_Preferences_triggered()
{
    //MW: TODO: This will crash if attempted to enter while still loading..

    if (m_inRecalculation) {
        mainwin->Notify(tr("Access to Preferences has been blocked until recalculation completes."));
        return;
    }
    PreferencesDialog pd(this,p_profile);
    prefdialog=&pd;
    if (pd.exec()==PreferencesDialog::Accepted) {
        qDebug() << "Preferences Accepted";
        //pd.Save();
        if (daily) {
            //daily->ReloadGraphs();
            daily->RedrawGraphs();
        }
        if (overview) {
            overview->ReloadGraphs();
            overview->RedrawGraphs();
        }
    }
    prefdialog=NULL;
}
void MainWindow::selectOximetryTab()
{
    on_oximetryButton_clicked();
}

void MainWindow::on_oximetryButton_clicked()
{
    bool first=false;
    if (!oximetry) {
        if (!PROFILE.oxi->oximetryEnabled()) {
            if (QMessageBox::question(this,tr("Question"),tr("Do you have a CMS50[x] Oximeter?\nOne is required to use this section."),QMessageBox::Yes,QMessageBox::No)==QMessageBox::No) return;
            PROFILE.oxi->setOximetryEnabled(true);
        }
        oximetry=new Oximetry(ui->tabWidget,daily->graphView());
        ui->tabWidget->insertTab(3,oximetry,STR_TR_Oximetry);
        first=true;
    }

    // MW: Instead, how about starting a direct import?
    oximetry->serialImport();

    ui->tabWidget->setCurrentWidget(oximetry);
    if (!first) oximetry->RedrawGraphs();
    qstatus2->setText(STR_TR_Oximetry);
}

void MainWindow::CheckForUpdates()
{
    on_actionCheck_for_Updates_triggered();
}

void MainWindow::on_actionCheck_for_Updates_triggered()
{
    UpdaterWindow *w=new UpdaterWindow(this);
    w->checkForUpdates();
}

void MainWindow::on_action_Screenshot_triggered()
{
    QTimer::singleShot(250,this,SLOT(DelayedScreenshot()));
}
void MainWindow::DelayedScreenshot()
{
    int w=width();
    int h=height();

    // Scale for high resolution displays (like Retina)
#if(QT_VERSION>=QT_VERSION_CHECK(5,0,0))
    qreal pr=devicePixelRatio();
    w/=pr;
    h/=pr;
#endif

    QPixmap pixmap=QPixmap::grabWindow(this->winId(),x(),y(),w,h);

    QString a=PREF.Get("{home}/Screenshots");
    QDir dir(a);
    if (!dir.exists()){
        dir.mkdir(a);
    }

    a+="/screenshot-"+QDateTime::currentDateTime().toString(Qt::ISODate)+".png";

    qDebug() << "Saving screenshot to" << a;
    if (!pixmap.save(a)) {
        Notify(tr("There was an error saving screenshot to file \"%1\"").arg(QDir::toNativeSeparators(a)));
    } else {
        Notify(tr("Screenshot saved to file \"%1\"").arg(QDir::toNativeSeparators(a)));
    }
}

void MainWindow::on_actionView_O_ximetry_triggered()
{
    on_oximetryButton_clicked();
}
void MainWindow::updatestatusBarMessage (const QString & text)
{
    ui->statusbar->showMessage(text,1000);
}

void MainWindow::on_actionPrint_Report_triggered()
{
#ifdef Q_WS_MAC
 #if ((QT_VERSION <= QT_VERSION_CHECK(4, 8, 4)))
    QMessageBox::information(this,tr("Printing Disabled"),tr("Please rebuild SleepyHead with Qt 4.8.5 or greater, as printing causes a crash with this version of Qt"),QMessageBox::Ok);
    return;
 #endif
#endif
    Report report;
    if (ui->tabWidget->currentWidget()==overview) {
        Report::PrintReport(overview->graphView(),STR_TR_Overview);
    } else if (ui->tabWidget->currentWidget()==daily) {
        Report::PrintReport(daily->graphView(),STR_TR_Daily,daily->getDate());
    } else if (ui->tabWidget->currentWidget()==oximetry) {
        if (oximetry)
            Report::PrintReport(oximetry->graphView(),STR_TR_Oximetry);
    } else {
        QPrinter printer;
#ifdef Q_WS_X11
        printer.setPrinterName("Print to File (PDF)");
        printer.setOutputFormat(QPrinter::PdfFormat);
        QString name;
        QString datestr;
        if (ui->tabWidget->currentWidget()==ui->summaryTab) {
            name="Summary";
            datestr=QDate::currentDate().toString(Qt::ISODate);
        } else if (ui->tabWidget->currentWidget()==ui->helpTab) {
            name="Help";
            datestr=QDateTime::currentDateTime().toString(Qt::ISODate);
        } else name="Unknown";

        QString filename=PREF.Get("{home}/"+name+"_"+PROFILE.user->userName()+"_"+datestr+".pdf");

        printer.setOutputFileName(filename);
#endif
        printer.setPrintRange(QPrinter::AllPages);
        printer.setOrientation(QPrinter::Portrait);
        printer.setFullPage(false); // This has nothing to do with scaling
        printer.setNumCopies(1);
        printer.setPageMargins(10,10,10,10,QPrinter::Millimeter);

        QPrintDialog pdlg(&printer,this);
        if (pdlg.exec()==QPrintDialog::Accepted) {

            if (ui->tabWidget->currentWidget()==ui->summaryTab) {
                ui->summaryView->print(&printer);
            } else if (ui->tabWidget->currentWidget()==ui->helpTab) {
                ui->webView->print(&printer);
            }

        }
        //QMessageBox::information(this,tr("Not supported Yet"),tr("Sorry, printing from this page is not supported yet"),QMessageBox::Ok);
    }
}

void MainWindow::on_action_Edit_Profile_triggered()
{
    NewProfile newprof(this);
    newprof.edit(PREF[STR_GEN_Profile].toString());
    newprof.exec();

}

void MainWindow::on_action_Link_Graph_Groups_toggled(bool arg1)
{
    PROFILE.general->setLinkGroups(arg1);
    if (daily) daily->RedrawGraphs();
}

void MainWindow::on_action_CycleTabs_triggered()
{
    int i;
    qDebug() << "Switching Tabs";
    i=ui->tabWidget->currentIndex()+1;
    if (i >= ui->tabWidget->count())
        i=0;
    ui->tabWidget->setCurrentIndex(i);
}

void MainWindow::on_actionExp_ort_triggered()
{
    ExportCSV ex(this);
    if (ex.exec()==ExportCSV::Accepted) {
    }
}

void MainWindow::on_actionOnline_Users_Guide_triggered()
{
    ui->webView->load(QUrl("http://sourceforge.net/apps/mediawiki/sleepyhead/index.php?title=SleepyHead_Users_Guide"));
    ui->tabWidget->setCurrentWidget(ui->helpTab);
}

void MainWindow::on_action_Frequently_Asked_Questions_triggered()
{
    ui->webView->load(QUrl("http://sourceforge.net/apps/mediawiki/sleepyhead/index.php?title=Frequently_Asked_Questions"));
    ui->tabWidget->setCurrentWidget(ui->helpTab);
}

void packEventList(EventList *el, EventDataType minval=0)
{
    if (el->count()<2) return;
    EventList nel(EVL_Waveform);
    EventDataType t=0,lastt=0; //el->data(0);
    qint64 ti=0;//=el->time(0);
    //nel.AddEvent(ti,lastt);
    bool f=false;
    qint64 lasttime=0;
    EventDataType min=999,max=0;


    for (quint32 i=0;i<el->count();i++) {
        t=el->data(i);
        ti=el->time(i);
        f=false;
        if (t>minval) {
            if (t!=lastt) {
                if (!lasttime) {
                    nel.setFirst(ti);
                }
                nel.AddEvent(ti,t);
                if (t < min) min=t;
                if (t > max) max=t;
                lasttime=ti;
                f=true;
            }
        } else {
            if (lastt>minval) {
                nel.AddEvent(ti,lastt);
                lasttime=ti;
                f=true;
            }
        }
        lastt=t;
    }
    if (!f) {
        if (t>minval) {
            nel.AddEvent(ti,t);
            lasttime=ti;
        }
    }
    el->setFirst(nel.first());
    el->setLast(nel.last());
    el->setMin(min);
    el->setMax(max);

    el->getData().clear();
    el->getTime().clear();
    el->setCount(nel.count());

    el->getData()=nel.getData();
    el->getTime()=nel.getTime();
}

void MainWindow::on_action_Rebuild_Oximetry_Index_triggered()
{
    QVector<ChannelID> valid;
    valid.push_back(OXI_Pulse);
    valid.push_back(OXI_SPO2);
    valid.push_back(OXI_Plethy);
    //valid.push_back(OXI_PulseChange); // Delete these and recalculate..
    //valid.push_back(OXI_SPO2Drop);

    QVector<ChannelID> invalid;

    QList<Machine *> machines=PROFILE.GetMachines(MT_OXIMETER);

    qint64 f=0,l=0;

    int discard_threshold=PROFILE.oxi->oxiDiscardThreshold();
    Machine *m;
    for (int z=0;z<machines.size();z++) {
        m=machines.at(z);
        //m->sessionlist.erase(m->sessionlist.find(0));

        // For each Session
        for (QHash<SessionID,Session *>::iterator s=m->sessionlist.begin();s!=m->sessionlist.end();s++) {
            Session *sess=s.value();
            if (!sess) continue;
            sess->OpenEvents();

            // For each EventList contained in session
            invalid.clear();
            f=0;
            l=0;
            for (QHash<ChannelID,QVector<EventList *> >::iterator e=sess->eventlist.begin();e!=sess->eventlist.end();e++) {

                // Discard any non data events.
                if (!valid.contains(e.key())) {
                    // delete and push aside for later to clean up
                    for (int i=0;i<e.value().size();i++)  {
                        delete e.value()[i];
                    }
                    e.value().clear();
                    invalid.push_back(e.key());
                } else {
                    // Valid event


//                    // Clean up outliers at start of eventlist chunks
//                    EventDataType baseline=sess->wavg(OXI_SPO2);
//                    if (e.key()==OXI_SPO2) {
//                        const int o2start_threshold=10000; // seconds since start of event

//                        EventDataType zz;
//                        int ii;

//                        // Trash suspect outliers in the first o2start_threshold milliseconds
//                        for (int j=0;j<e.value().size();j++) {
//                            EventList *ev=e.value()[j];
//                            if ((ev->count() <= (unsigned)discard_threshold))
//                                continue;

//                            qint64 ti=ev->time(0);

//                            zz=-1;
//                            // Peek o2start_threshold ms ahead and grab the value
//                            for (ii=0;ii<ev->count();ii++) {
//                                if (((ev->time(ii)-ti) > o2start_threshold)) {
//                                    zz=ev->data(ii);
//                                    break;
//                                }
//                            }
//                            if (zz<0)
//                                continue;
//                            // Trash any suspect outliers
//                            for (int i=0;i<ii;i++) {
//                                if (ev->data(i) < baseline) { //(zz-10)) {

//                                    ev->getData()[i]=0;
//                                }
//                            }
//                        }
//                    }
                    QVector<EventList *> newlist;
                    for (int i=0;i<e.value().size();i++)  {
                        if (e.value()[i]->count() > (unsigned)discard_threshold) {
                            newlist.push_back(e.value()[i]);
                        } else {
                            delete e.value()[i];
                        }
                    }
                    for (int i=0;i<newlist.size();i++) {
                        packEventList(newlist[i],8);

                        EventList * el=newlist[i];
                        if (!f || f > el->first()) f=el->first();
                        if (!l || l < el->last()) l=el->last();
                    }
                    e.value()=newlist;
                }
            }
            for (int i=0;i<invalid.size();i++) {
                sess->eventlist.erase(sess->eventlist.find(invalid[i]));
            }
            if (f) sess->really_set_first(f);
            if (l) sess->really_set_last(l);
            sess->m_cnt.clear();
            sess->m_sum.clear();
            sess->m_min.clear();
            sess->m_max.clear();
            sess->m_cph.clear();
            sess->m_sph.clear();
            sess->m_avg.clear();
            sess->m_wavg.clear();
            sess->m_valuesummary.clear();
            sess->m_timesummary.clear();
            sess->m_firstchan.clear();
            sess->m_lastchan.clear();
            sess->SetChanged(true);
        }

    }
    for (int i=0;i<machines.size();i++) {
        Machine *m=machines[i];
        m->Save();
    }
    getDaily()->LoadDate(getDaily()->getDate());
    //getDaily()->ReloadGraphs();
    getOverview()->ReloadGraphs();
}

void MainWindow::RestartApplication(bool force_login,bool change_datafolder)
{
    QString apppath;
#ifdef Q_OS_MAC
        // In Mac OS the full path of aplication binary is:
        //    <base-path>/myApp.app/Contents/MacOS/myApp

        // prune the extra bits to just get the app bundle path
        apppath=QApplication::instance()->applicationDirPath().section("/",0,-3);

        QStringList args;
        args << "-n"; // -n option is important, as it opens a new process
        args << apppath;

        args << "--args"; // SleepyHead binary options after this
        args << "-p"; // -p starts with 1 second delay, to give this process time to save..


        if (force_login) args << "-l";
        if (change_datafolder) args << "-d";

        if (QProcess::startDetached("/usr/bin/open",args)) {
            QApplication::instance()->exit();
        } else QMessageBox::warning(NULL,tr("Gah!"),tr("If you can read this, the restart command didn't work. Your going to have to do it yourself manually."),QMessageBox::Ok);

#else
        apppath=QApplication::instance()->applicationFilePath();

        // If this doesn't work on windoze, try uncommenting this method
        // Technically should be the same thing..

        //if (QDesktopServices::openUrl(apppath)) {
        //    QApplication::instance()->exit();
        //} else
        QStringList args;
        args << "-p";
        if (force_login) args << "-l";
        if (change_datafolder) args << "-d";
        if (QProcess::startDetached(apppath,args)) {
            ::exit(0);
            //QApplication::instance()->exit();
        } else QMessageBox::warning(NULL,tr("Gah!"),tr("If you can read this, the restart command didn't work. Your going to have to do it yourself manually."),QMessageBox::Ok);
#endif
}

void MainWindow::on_actionChange_User_triggered()
{
    PROFILE.Save();
    PREF.Save();
    RestartApplication(true);
}

void MainWindow::on_actionPurge_Current_Day_triggered()
{
    QDate date=getDaily()->getDate();
    Day *day=PROFILE.GetDay(date,MT_CPAP);
    Machine *m;
    if (day) {
        m=day->machine;
        QString path=PROFILE.Get("{"+STR_GEN_DataFolder+"}/")+m->GetClass()+"_"+m->properties[STR_PROP_Serial]+"/";

        QList<Session *>::iterator s;

        QList<Session *> list;
        for (s=day->begin();s!=day->end();++s) {
            SessionID id=(*s)->session();
            QString filename0=path+QString().sprintf("%08lx.000",id);
            QString filename1=path+QString().sprintf("%08lx.001",id);
            qDebug() << "Removing" << filename0;
            qDebug() << "Removing" << filename1;
            QFile::remove(filename0);
            QFile::remove(filename1);

            list.push_back(*s);
            m->sessionlist.erase(m->sessionlist.find(id)); // remove from machines session list
        }
        m->day.erase(m->day.find(date));
        for (int i=0;i<list.size();i++) {
            Session * sess=list.at(i);
            day->removeSession(sess);
            delete sess;
        }



        QList<Day *> & dl=PROFILE.daylist[date];
        QList<Day *>::iterator it;//=dl.begin();
        for (it=dl.begin();it!=dl.end();it++) {
            if ((*it)==day) break;
        }
        if (it!=dl.end()) {
            dl.erase(it);
            //PROFILE.daylist[date].  // ??
            delete day;
        }
    }
    getDaily()->clearLastDay();
    getDaily()->LoadDate(date);
}

void MainWindow::on_actionAll_Data_for_current_CPAP_machine_triggered()
{
    QDate date=getDaily()->getDate();
    Day *day=PROFILE.GetDay(date,MT_CPAP);
    Machine *m;
    if (day) {
        m=day->machine;
        if (!m) {
            qDebug() << "Gah!! no machine to purge";
            return;
        }
        if (QMessageBox::question(this,tr("Are you sure?"),tr("Are you sure you want to purge all CPAP data for the following machine:\n")+m->properties[STR_PROP_Brand]+" "+m->properties[STR_PROP_Model]+" "+m->properties[STR_PROP_ModelNumber]+" ("+m->properties[STR_PROP_Serial]+")",QMessageBox::Yes,QMessageBox::No)==QMessageBox::Yes) {
            m->Purge(3478216);
            PROFILE.machlist.erase(PROFILE.machlist.find(m->id()));
            // delete or not to delete.. this needs to delete later.. :/
            //delete m;
            RestartApplication();
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent * event)
{
    Q_UNUSED(event)
    //qDebug() << "Keypress:" << event->key();
}

void MainWindow::on_summaryButton_2_clicked()
{
    ui->tabWidget->setCurrentWidget(ui->summaryTab);
    on_summaryButton_clicked();
}

void MainWindow::on_action_Sidebar_Toggle_toggled(bool visible)
{
    ui->toolBox->setVisible(visible);
}

void MainWindow::on_recordsBox_linkClicked(const QUrl &linkurl)
{
    QString link=linkurl.toString().section("=",0,0).toLower();
    QString datestr=linkurl.toString().section("=",1).toLower();
    qDebug() << linkurl.toString() << link << datestr;
    if (link=="daily") {
        QDate date=QDate::fromString(datestr,Qt::ISODate);
        daily->LoadDate(date);
        ui->tabWidget->setCurrentWidget(daily);
    } else if (link=="overview") {
        QString date1=datestr.section(",",0,0);
        QString date2=datestr.section(",",1);

        QDate d1=QDate::fromString(date1,Qt::ISODate);
        QDate d2=QDate::fromString(date2,Qt::ISODate);
        overview->setRange(d1,d2);
        ui->tabWidget->setCurrentWidget(overview);
    }

}

void MainWindow::on_helpButton_clicked()
{
    ui->tabWidget->setCurrentWidget(ui->helpTab);
}

void MainWindow::on_actionView_S_ummary_triggered()
{
    ui->tabWidget->setCurrentWidget(ui->summaryTab);
}

void MainWindow::on_webView_linkClicked(const QUrl &url)
{
    QString s=url.toString();
    qDebug() << "Link Clicked" << url;
    if (s.toLower().startsWith("https:")) {
        QDesktopServices().openUrl(url);
    } else {
        ui->webView->setUrl(url);
    }
}

void MainWindow::on_webView_statusBarMessage(const QString &text)
{
    ui->statusbar->showMessage(text);
}

void MainWindow::LinkHovered(const QString & link, const QString & title, const QString & textContent)
{
    Q_UNUSED(title);
    Q_UNUSED(textContent);
    ui->statusbar->showMessage(link);
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
    Q_UNUSED(index);
    QWidget *widget=ui->tabWidget->currentWidget();
    if ((widget==ui->summaryTab) || (widget==ui->helpTab)) {
        qstatus2->setVisible(false);
    } else if (widget==daily) {
        qstatus2->setVisible(true);
        daily->graphView()->selectionTime();
    } else if (widget==overview) {
        qstatus2->setVisible(true);
        overview->graphView()->selectionTime();
    } else if (widget==oximetry) {
        qstatus2->setVisible(true);
        oximetry->graphView()->selectionTime();
    }
}

void MainWindow::on_summaryView_linkClicked(const QUrl &arg1)
{
    //qDebug() << arg1;
    on_recordsBox_linkClicked(arg1);
}

void MainWindow::on_bookmarkView_linkClicked(const QUrl &arg1)
{
    on_recordsBox_linkClicked(arg1);
}

void MainWindow::on_filterBookmarks_editingFinished()
{
    bookmarkFilter=ui->filterBookmarks->text();
    updateFavourites();
}

void MainWindow::on_filterBookmarksButton_clicked()
{
    if (!bookmarkFilter.isEmpty()) {
        ui->filterBookmarks->setText("");
        bookmarkFilter="";
        updateFavourites();
    }
}

void MainWindow::reprocessEvents(bool restart)
{
    m_restartRequired=restart;
    QTimer::singleShot(0,this,SLOT(doReprocessEvents()));
}


void MainWindow::FreeSessions()
{
    QDate first=PROFILE.FirstDay();
    QDate date=PROFILE.LastDay();
    Day *day;
    QDate current=daily->getDate();
    do {
        day=PROFILE.GetDay(date,MT_CPAP);
        if (day) {
            if (date!=current) {
                day->CloseEvents();
            }
        }
        date=date.addDays(-1);
    }  while (date>=first);
}

void MainWindow::doReprocessEvents()
{
    if (PROFILE.countDays(MT_CPAP,PROFILE.FirstDay(),PROFILE.LastDay())==0)
        return;

    m_inRecalculation=true;
    QDate first=PROFILE.FirstDay();
    QDate date=PROFILE.LastDay();
    Session *sess;
    Day *day;
    //FlowParser flowparser;

    mainwin->Notify(tr("Performance will be degraded during these recalculations."),tr("Recalculating Indices"));

    // For each day in history
    int daycount=first.daysTo(date);
    int idx=0;

    QList<Machine *> machines=PROFILE.GetMachines(MT_CPAP);

    // Disabling multithreaded save as it appears it's causing problems
    bool cache_sessions=false; //PROFILE.session->cacheSessions();
    if (cache_sessions) { // Use multithreaded save to handle reindexing.. (hogs memory like hell)
        qstatus->setText(tr("Loading Event Data"));
    } else {
        qstatus->setText(tr("Recalculating Summaries"));
    }
    if (qprogress) {
        qprogress->setValue(0);
        qprogress->setVisible(true);
    }
    bool isopen;
    do {
        day=PROFILE.GetDay(date,MT_CPAP);
        if (day) {
            for (int i=0;i<day->size();i++) {
                sess=(*day)[i];
                isopen=sess->eventsLoaded();

                // Load the events if they aren't loaded already
                sess->OpenEvents();

                //if (!sess->channelDataExists(CPAP_FlowRate)) continue;

                //QVector<EventList *> & flowlist=sess->eventlist[CPAP_FlowRate];

                // Destroy any current user flags..
                sess->destroyEvent(CPAP_UserFlag1);
                sess->destroyEvent(CPAP_UserFlag2);
                sess->destroyEvent(CPAP_UserFlag3);

                // AHI flags
                sess->destroyEvent(CPAP_AHI);
                sess->destroyEvent(CPAP_RDI);

                sess->SetChanged(true);
                if (!cache_sessions) {
                    sess->UpdateSummaries();
                    sess->machine()->SaveSession(sess);
                    if (!isopen)
                        sess->TrashEvents();
                }
            }
        }
        date=date.addDays(-1);
       // if (qprogress && (++idx % 10) ==0) {
            qprogress->setValue(0+(float(++idx)/float(daycount)*100.0));
            QApplication::processEvents();
       // }

    } while (date>=first);

    if (cache_sessions) {
        qstatus->setText(tr("Recalculating Summaries"));
        for (int i=0;i<machines.size();i++) {
            machines.at(i)->Save();
        }
    }

    qstatus->setText(tr(""));
    qprogress->setVisible(false);
    m_inRecalculation=false;
    if (m_restartRequired) {
        QMessageBox::information(this,tr("Restart Required"),tr("Recalculations are complete, the application now needs to restart to display the changes."),QMessageBox::Ok);
        RestartApplication();
        return;
    } else {
        Notify(tr("Recalculations are now complete."),tr("Task Completed"));

        FreeSessions();
        QDate current=daily->getDate();
        daily->LoadDate(current);
        if (overview) overview->ReloadGraphs();
    }
}

void MainWindow::on_actionImport_ZEO_Data_triggered()
{
    QFileDialog w;
    w.setFileMode(QFileDialog::ExistingFiles);
    w.setOption(QFileDialog::ShowDirsOnly, false);
    w.setOption(QFileDialog::DontUseNativeDialog,true);
    w.setNameFilters(QStringList("Zeo CSV File (*.csv)"));

    ZEOLoader zeo;
    if (w.exec()==QFileDialog::Accepted) {
        QString filename=w.selectedFiles()[0];
        if (!zeo.OpenFile(filename)) {
            Notify(tr("There was a problem opening ZEO File: ")+filename);
            return;
        }
        Notify(tr("Zeo CSV Import complete"));
        daily->LoadDate(daily->getDate());
    }


}

void MainWindow::on_actionImport_RemStar_MSeries_Data_triggered()
{
#ifdef REMSTAR_M_SUPPORT
    QFileDialog w;
    w.setFileMode(QFileDialog::ExistingFiles);
    w.setOption(QFileDialog::ShowDirsOnly, false);
    w.setOption(QFileDialog::DontUseNativeDialog,true);
    w.setNameFilters(QStringList("M-Series data file (*.bin)"));

    MSeriesLoader mseries;
    if (w.exec()==QFileDialog::Accepted) {
        QString filename=w.selectedFiles()[0];
        if (!mseries.Open(filename,p_profile)) {
            Notify(tr("There was a problem opening MSeries block File: ")+filename);
            return;
        }
        Notify(tr("MSeries Import complete"));
        daily->LoadDate(daily->getDate());
    }
#endif
}

void MainWindow::on_actionSleep_Disorder_Terms_Glossary_triggered()
{
    ui->webView->load(QUrl("http://sourceforge.net/apps/mediawiki/sleepyhead/index.php?title=Glossary"));
    ui->tabWidget->setCurrentWidget(ui->helpTab);
}

void MainWindow::on_actionHelp_Support_SleepyHead_Development_triggered()
{
    QUrl url=QUrl("http://sourceforge.net/apps/mediawiki/sleepyhead/index.php?title=Support_SleepyHead_Development");
    QDesktopServices().openUrl(url);
//    ui->webView->load(url);
//    ui->tabWidget->setCurrentWidget(ui->helpTab);
}

void MainWindow::on_actionChange_Language_triggered()
{
    QSettings *settings=new QSettings(getDeveloperName(),getAppName());
    settings->remove("Settings/Language");
    delete settings;
    PROFILE.Save();
    PREF.Save();

    RestartApplication(true);
}

void MainWindow::on_actionChange_Data_Folder_triggered()
{
    PROFILE.Save();
    PREF.Save();
    RestartApplication(false,true);
}
