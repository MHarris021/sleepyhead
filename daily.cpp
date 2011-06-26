/********************************************************************
 Daily Panel
 Copyright (c)2011 Mark Watkins <jedimark@users.sourceforge.net>
 License: GPL
*********************************************************************/

#include "daily.h"
#include "ui_daily.h"

#include <QTextCharFormat>
#include <QTextBlock>
#include <QColorDialog>
#include <QBuffer>
#include <QPixmap>

#include "SleepLib/session.h"
#include "Graphs/graphdata_custom.h"
#include "Graphs/gLineChart.h"
#include "Graphs/gLineOverlay.h"
#include "Graphs/gFlagsLine.h"
#include "Graphs/gFooBar.h"
#include "Graphs/gXAxis.h"
#include "Graphs/gYAxis.h"
#include "Graphs/gCandleStick.h"
#include "Graphs/gBarChart.h"

Daily::Daily(QWidget *parent,QGLContext *context) :
    QWidget(parent),
    ui(new Ui::Daily)
{
    ui->setupUi(this);

    shared_context=context;

    QString prof=pref["Profile"].toString();
    profile=Profiles::Get(prof);
    if (!profile) {
        qWarning("Couldn't get profile.. Have to abort!");
        abort();
    }

    gSplitter=new QSplitter(Qt::Vertical,ui->scrollArea);
    gSplitter->setStyleSheet("QSplitter::handle { background-color: 'dark grey'; }");

    gSplitter->setChildrenCollapsible(false);
    gSplitter->setHandleWidth(3);
    ui->graphSizer->addWidget(gSplitter);

    //QPalette pal;
    //QColor col("blue");
    //pal.setColor(QPalette::Button, col);
    //gSplitter->setPaletteForegroundColor(QColor("blue"));
    //gSplitter->setBackgroundRole(QPalette::Button);
    //ui->scrollArea->setWidgetResizable(true);
    //gSplitter->setMinimumSize(500,500);
    //gSplitter->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Maximum);

    AddCPAPData(flags[3]=new FlagData(CPAP_Hypopnea,4));
    AddCPAPData(flags[0]=new FlagData(CPAP_CSR,7,1,0));
    AddCPAPData(flags[1]=new FlagData(CPAP_ClearAirway,6));
    AddCPAPData(flags[2]=new FlagData(CPAP_Obstructive,5));
    AddCPAPData(flags[4]=new FlagData(CPAP_FlowLimit,3));
    AddCPAPData(flags[5]=new FlagData(CPAP_VSnore,2));
    AddCPAPData(flags[6]=new FlagData(CPAP_RERA,1));
    AddCPAPData(flags[7]=new FlagData(PRS1_PressurePulse,1));
    AddCPAPData(flags[8]=new FlagData(PRS1_VSnore2,1));
    AddCPAPData(flags[9]=new FlagData(PRS1_Unknown0E,1));
    AddCPAPData(frw=new WaveData(CPAP_FlowRate));
    SF=new gGraphWindow(gSplitter,"Event Flags",(QGLWidget *)NULL); //
    int sfc=7;
    SF->SetLeftMargin(SF->GetLeftMargin()+gYAxis::Margin);
    SF->SetBlockZoom(true);
    SF->AddLayer(new gXAxis());
    bool extras=true;
    if (extras) {
        sfc+=2;
        SF->AddLayer(new gFlagsLine(flags[9],QColor("dark green"),"U0E",8,sfc));
        SF->AddLayer(new gFlagsLine(flags[8],QColor("red"),"VS2",7,sfc));
    }
    SF->AddLayer(new gFlagsLine(flags[6],QColor("yellow"),"RE",6,sfc));
    SF->AddLayer(new gFlagsLine(flags[5],QColor("red"),"VS",5,sfc));
    SF->AddLayer(new gFlagsLine(flags[4],QColor("black"),"FL",4,sfc));
    SF->AddLayer(new gFlagsLine(flags[3],QColor("blue"),"H",3,sfc));
    SF->AddLayer(new gFlagsLine(flags[2],QColor("aqua"),"OA",2,sfc));
    SF->AddLayer(new gFlagsLine(flags[1],QColor("purple"),"CA",1,sfc));
    SF->AddLayer(new gFlagsLine(flags[0],QColor("light green"),"CSR",0,sfc));
    SF->AddLayer(new gFooBar(10,QColor("lime green"),QColor("dark grey"),true));
    SF->setMinimumHeight(150+(extras ? 20 : 0));
   // SF->setMaximumHeight(350);

    AddCPAPData(pressure_iap=new EventData(CPAP_IAP));
    AddCPAPData(pressure_eap=new EventData(CPAP_EAP));
    AddCPAPData(prd=new EventData(CPAP_Pressure));
    PRD=new gGraphWindow(gSplitter,"Pressure",SF);
    PRD->AddLayer(new gXAxis());
    PRD->AddLayer(new gYAxis());
    PRD->AddLayer(new gFooBar());
    PRD->AddLayer(new gLineChart(prd,QColor("dark green"),4096,false,false,true));
    PRD->AddLayer(new gLineChart(pressure_iap,QColor("blue"),4096,false,true,true));
    PRD->AddLayer(new gLineChart(pressure_eap,QColor("red"),4096,false,true,true));
    PRD->setMinimumHeight(150);


    FRW=new gGraphWindow(gSplitter,"Flow Rate",SF); //shared_context);
    FRW->AddLayer(new gXAxis());
    FRW->AddLayer(new gYAxis());
    FRW->AddLayer(new gFooBar());
    FRW->AddLayer(new gLineOverlayBar(flags[0],QColor("light green"),"CSR"));
    gLineChart *g=new gLineChart(frw,QColor("black"),200000,true);
    g->ReportEmpty(true);

    FRW->AddLayer(g);
    FRW->AddLayer(new gLineOverlayBar(flags[3],QColor("blue"),"H"));
    FRW->AddLayer(new gLineOverlayBar(flags[7],QColor("red"),"PR",LOT_Dot));
    FRW->AddLayer(new gLineOverlayBar(flags[6],QColor("yellow"),"RE"));
    FRW->AddLayer(new gLineOverlayBar(flags[9],QColor("dark green"),"U0E"));
    FRW->AddLayer(new gLineOverlayBar(flags[5],QColor("red"),"VS"));
    FRW->AddLayer(new gLineOverlayBar(flags[4],QColor("black"),"FL"));
    FRW->AddLayer(new gLineOverlayBar(flags[2],QColor("aqua"),"OA"));
    FRW->AddLayer(new gLineOverlayBar(flags[1],QColor("purple"),"CA"));

    FRW->setMinimumHeight(190);

    AddCPAPData(leakdata=new EventData(CPAP_Leak,0));
    //leakdata->ForceMinY(0);
    //leakdata->ForceMaxY(120);
    LEAK=new gGraphWindow(gSplitter,"Leaks",SF);
    LEAK->AddLayer(new gXAxis());
    LEAK->AddLayer(new gYAxis());
    LEAK->AddLayer(new gFooBar());
    LEAK->AddLayer(new gLineChart(leakdata,QColor("purple"),4096,false,false,false));

    LEAK->setMinimumHeight(150);

    AddCPAPData(snore=new EventData(CPAP_SnoreGraph,0));
    //snore->ForceMinY(0);
    //snore->ForceMaxY(15);
    SNORE=new gGraphWindow(gSplitter,"Snore",SF);
    SNORE->AddLayer(new gXAxis());
    SNORE->AddLayer(new gYAxis());
    SNORE->AddLayer(new gFooBar());
    SNORE->AddLayer(new gLineChart(snore,QColor("black"),4096,false,false,true));

    SNORE->setMinimumHeight(150);

    AddOXIData(pulse=new EventData(OXI_Pulse,0,65536,true));
    //pulse->ForceMinY(40);
    //pulse->ForceMaxY(120);
    PULSE=new gGraphWindow(gSplitter,"Pulse",SF);
    PULSE->AddLayer(new gXAxis());
    PULSE->AddLayer(new gYAxis());
    PULSE->AddLayer(new gFooBar());
    PULSE->AddLayer(new gLineChart(pulse,QColor("red"),65536,false,false,true));

    PULSE->setMinimumHeight(150);

    AddOXIData(spo2=new EventData(OXI_SPO2,0,65536,true));
    //spo2->ForceMinY(60);
    //spo2->ForceMaxY(100);
    SPO2=new gGraphWindow(gSplitter,"SpO2",SF);
    SPO2->AddLayer(new gXAxis());
    SPO2->AddLayer(new gYAxis());
    SPO2->AddLayer(new gFooBar());
    SPO2->AddLayer(new gLineChart(spo2,QColor("blue"),65536,false,false,true));
    SPO2->setMinimumHeight(150);
    SPO2->LinkZoom(PULSE);
    PULSE->LinkZoom(SPO2);
    SPO2->hide();
    PULSE->hide();

    AddCPAPData(tap_eap=new TAPData(CPAP_EAP));
    AddCPAPData(tap_iap=new TAPData(CPAP_IAP));
    AddCPAPData(tap=new TAPData(CPAP_Pressure));


    TAP=new gGraphWindow(gSplitter,"",SF);
    //TAP->SetMargins(20,15,5,50);
    TAP->SetMargins(0,0,0,0);
    TAP->AddLayer(new gCandleStick(tap));

    TAP_EAP=new gGraphWindow(gSplitter,"",SF);
    TAP_EAP->SetMargins(0,0,0,0);
    TAP_EAP->AddLayer(new gCandleStick(tap_eap));

    TAP_IAP=new gGraphWindow(gSplitter,"",SF);
    TAP_IAP->SetMargins(0,0,0,0);
    TAP_IAP->AddLayer(new gCandleStick(tap_iap));

    G_AHI=new gGraphWindow(gSplitter,"",SF);
    G_AHI->SetMargins(0,0,0,0);
    AddCPAPData(g_ahi=new AHIData());
    gCandleStick *l=new gCandleStick(g_ahi);
    l->AddName("H");
    l->AddName("OA");
    l->AddName("CA");
    l->AddName("RE");
    l->AddName("FL");
    l->AddName("CSR");
    l->color.clear();
    l->color.push_back(QColor("blue"));
    l->color.push_back(QColor("aqua"));
    l->color.push_back(QColor("purple")); //0xff,0x40,0xff,0xff)); //wxPURPLE);
    l->color.push_back(QColor("yellow"));
    l->color.push_back(QColor("black"));
    l->color.push_back(QColor("light green"));
    G_AHI->AddLayer(l);
    //G_AHI->setMaximumSize(2000,30);
    //TAP->setMaximumSize(2000,30);
    NoData=new QLabel("No CPAP Data",gSplitter);
    NoData->setAlignment(Qt::AlignCenter);
    QFont font("Times",20); //NoData->font();
    //font.setBold(true);
    NoData->setFont(font);
    NoData->hide();

    G_AHI->hide();
    TAP->hide();
    TAP_IAP->hide();
    TAP_EAP->hide();

    FRW->LinkZoom(SF);
    FRW->LinkZoom(PRD);
    FRW->LinkZoom(LEAK);
    FRW->LinkZoom(SNORE);
    SF->LinkZoom(FRW);
    SF->LinkZoom(PRD);
    SF->LinkZoom(LEAK);
    SF->LinkZoom(SNORE);
    PRD->LinkZoom(SF);
    PRD->LinkZoom(FRW);
    PRD->LinkZoom(LEAK);
    PRD->LinkZoom(SNORE);
    LEAK->LinkZoom(SF);
    LEAK->LinkZoom(FRW);
    LEAK->LinkZoom(PRD);
    LEAK->LinkZoom(SNORE);
    SNORE->LinkZoom(SF);
    SNORE->LinkZoom(FRW);
    SNORE->LinkZoom(PRD);
    SNORE->LinkZoom(LEAK);

    gSplitter->addWidget(SF);
    gSplitter->addWidget(FRW);
    gSplitter->addWidget(PRD);
    gSplitter->addWidget(LEAK);
    gSplitter->addWidget(SNORE);
    gSplitter->addWidget(NoData);
    gSplitter->addWidget(PULSE);
    gSplitter->addWidget(SPO2);
    gSplitter->refresh();

    ui->graphSizer->layout();

    /*ui->graphLayout->addWidget(SF);
    ui->graphLayout->addWidget(FRW);
    ui->graphLayout->addWidget(PRD);
    ui->graphLayout->addWidget(LEAK);
    ui->graphLayout->addWidget(SNORE);
    ui->graphLayout->addWidget(PULSE);
    ui->graphLayout->addWidget(SPO2); */
   // ui->graphLayout->addWidget(G_AHI);
    //ui->graphLayout->addWidget(TAP);

    QTextCharFormat format = ui->calendar->weekdayTextFormat(Qt::Saturday);
    format.setForeground(QBrush(Qt::black, Qt::SolidPattern));
    ui->calendar->setWeekdayTextFormat(Qt::Saturday, format);
    ui->calendar->setWeekdayTextFormat(Qt::Sunday, format);

    ui->tabWidget->setCurrentWidget(ui->info);
    ReloadGraphs();
 }

Daily::~Daily()
{
    // Save any last minute changes..
    if (previous_date.isValid())
        Unload(previous_date);

    delete gSplitter;
    delete ui;
}
void Daily::ReloadGraphs()
{
    QDate d=profile->LastDay();
    if (!d.isValid()) {
        d=ui->calendar->selectedDate();
    }

    on_calendar_currentPageChanged(d.year(),d.month());
    ui->calendar->setSelectedDate(d);
    Load(d);
}
void Daily::on_calendar_currentPageChanged(int year, int month)
{
    QDate d(year,month,1);
    int dom=d.daysInMonth();

    for (int i=1;i<=dom;i++) {
        d=QDate(year,month,i);
        this->UpdateCalendarDay(d);
    }
}
void Daily::UpdateEventsTree(QTreeWidget *tree,Day *day)
{
    if (!day) return;
    tree->clear();
    tree->setColumnCount(1); // 1 visible common.. (1 hidden)

    QTreeWidgetItem *root=NULL;//new QTreeWidgetItem((QTreeWidget *)0,QStringList("Stuff"));
    map<MachineCode,QTreeWidgetItem *> mcroot;
    map<MachineCode,int> mccnt;

    for (vector<Session *>::iterator s=day->begin();s!=day->end();s++) {

        map<MachineCode,vector<Event *> >::iterator m;

        QTreeWidgetItem * ti,sroot;

        for (m=(*s)->events.begin();m!=(*s)->events.end();m++) {
            MachineCode code=m->first;
            if (code==CPAP_Leak) continue;
            if (code==CPAP_SnoreGraph) continue;
            if (code==PRS1_Unknown12) continue;
            QTreeWidgetItem *mcr;
            if (mcroot.find(code)==mcroot.end()) {
                QString st=DefaultMCLongNames[m->first];
                if (st.isEmpty())  {
                    st=st.sprintf("Fixme: %i",code);
                }
                QStringList l(st);
                l.append("");
                //QString g="";
                mcroot[code]=mcr=new QTreeWidgetItem(root,l);
                mccnt[code]=0;
            } else {
                mcr=mcroot[code];
            }
            for (vector<Event *>::iterator e=(*s)->events[code].begin();e!=(*s)->events[code].end();e++) {
                QDateTime t=(*e)->time();
                if (code==CPAP_CSR) {
                    t=t.addSecs(-((*(*e))[0]/2));
                }
                QStringList a;
                QString c;
                c.sprintf("#%04i: ",mccnt[code]++);
                a.append(c+t.toString(" HH:mm:ss"));
                a.append(t.toString("yyyy-MM-dd HH:mm:ss"));
                mcr->addChild(new QTreeWidgetItem(a));
            }
        }
    }
    int cnt=0;
    for (map<MachineCode,QTreeWidgetItem *>::iterator m=mcroot.begin();m!=mcroot.end();m++) {
        tree->insertTopLevelItem(cnt++,m->second);
    }


    tree->sortByColumn(0,Qt::AscendingOrder);
    //tree->expandAll();
}
void Daily::UpdateCalendarDay(QDate date)
{
    QTextCharFormat bold;
    QTextCharFormat cpapcol;
    QTextCharFormat normal;
    bold.setFontWeight(QFont::Bold);
    cpapcol.setForeground(QBrush(Qt::blue, Qt::SolidPattern));
    cpapcol.setFontWeight(QFont::Bold);

    if (profile->GetDay(date,MT_CPAP)) {
        ui->calendar->setDateTextFormat(date,cpapcol);
    } else if (profile->GetDay(date)) {
        ui->calendar->setDateTextFormat(date,bold);
    } else {
        ui->calendar->setDateTextFormat(date,normal);
    }
    ui->calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);

}
void Daily::on_calendar_selectionChanged()
{
    if (previous_date.isValid())
        Unload(previous_date);

    Load(ui->calendar->selectedDate());
}
void Daily::Load(QDate date)
{
    previous_date=date;
    Day *cpap=profile->GetDay(date,MT_CPAP);
    Day *oxi=profile->GetDay(date,MT_OXIMETER);
    Day *sleepstage=profile->GetDay(date,MT_SLEEPSTAGE);

    QString html="<html><body leftmargin=0 rightmargin=0 topmargin=0 marginwidth=0 marginheight=0>";
    html+="<table cellspacing=0 cellpadding=2 border=0 width='100%'>\n";
    QString tmp;

    UpdateCPAPGraphs(cpap);
    UpdateOXIGraphs(oxi);
    UpdateEventsTree(ui->treeWidget,cpap);

    const int gwwidth=270;
    const int gwheight=25;



    QString epr,modestr;
    float iap90,eap90;
    CPAPMode mode=MODE_UNKNOWN;
    PRTypes pr;
    QString a;
    if (cpap) {
        mode=(CPAPMode)cpap->summary_max(CPAP_Mode);
        pr=(PRTypes)cpap->summary_max(CPAP_PressureReliefType);
        if (pr==PR_NONE)
           epr=tr(" No Pressure Relief");
        else {
            a.sprintf(" x%i",(int)cpap->summary_max(CPAP_PressureReliefSetting));
            epr=PressureReliefNames[pr]+a;
        }
        modestr=CPAPModeNames[mode];

        float ahi=(cpap->count(CPAP_Obstructive)+cpap->count(CPAP_Hypopnea)+cpap->count(CPAP_ClearAirway))/cpap->hours();
        float csr=(100.0/cpap->hours())*(cpap->sum(CPAP_CSR)/3600.0);
        float oai=cpap->count(CPAP_Obstructive)/cpap->hours();
        float hi=cpap->count(CPAP_Hypopnea)/cpap->hours();
        float cai=cpap->count(CPAP_ClearAirway)/cpap->hours();
        float rei=cpap->count(CPAP_RERA)/cpap->hours();
        float vsi=cpap->count(CPAP_VSnore)/cpap->hours();
        float fli=cpap->count(CPAP_FlowLimit)/cpap->hours();
        //        float p90=cpap->percentile(CPAP_Pressure,0,0.9);
        eap90=cpap->percentile(CPAP_EAP,0,0.9);
        iap90=cpap->percentile(CPAP_IAP,0,0.9);
        QString submodel=tr("Unknown Model");


        html=html+"<tr><td colspan=4 align=center><i>"+tr("Machine Information")+"</i></td></tr>\n";
        if (cpap->machine->properties.find("SubModel")!=cpap->machine->properties.end())
            submodel=" <br>"+cpap->machine->properties["SubModel"];
        html=html+"<tr><td colspan=4 align=center><b>"+cpap->machine->properties["Brand"]+"</b> <br>"+cpap->machine->properties["Model"]+" "+cpap->machine->properties["ModelNumber"]+submodel+"</td></tr>\n";
        if (pref.Exists("ShowSerialNumbers") && pref["ShowSerialNumbers"].toBool()) {
            html=html+"<tr><td colspan=4 align=center>"+cpap->machine->properties["Serial"]+"</td></tr>\n";
        }

        html=html+"<tr><td align='center'><b>Date</b></td><td align='center'><b>Sleep</b></td><td align='center'><b>Wake</b></td><td align='center'><b>Hours</b></td></tr>";
        int tt=cpap->total_time();
        html=html+"<tr><td align='center'>"+cpap->first().date().toString(Qt::SystemLocaleShortDate)+"</td><td align='center'>"+cpap->first().toString("HH:mm")+"</td><td align='center'>"+cpap->last().toString("HH:mm")+"</td><td align='center'>"+a.sprintf("%02i:%02i",tt/3600,tt%60)+"</td></tr>\n";
        html=html+"<tr><td colspan=4 align=center><hr></td></tr>\n";

        if (pref.Exists("fruitsalad") && pref["fruitsalad"].toBool()) {
            html=html+("<tr><td colspan=2><table cellspacing=0 cellpadding=2 border=0 width='100%'>");
            html=html+("<tr><td align='right' bgcolor='#F88017'><b><font color='black'>")+tr("AHI")+("</font></b></td><td  bgcolor='#F88017'><b><font color='black'>")+a.sprintf("%.2f",ahi)+("</font></b></td></tr>\n");
            html=html+("<tr><td align='right' bgcolor='#4040ff'><b><font color='white'>")+tr("Hypopnea")+("</font></b></td><td bgcolor='#4040ff'><font color='white'>")+a.sprintf("%.2f",hi)+("</font></td></tr>\n");
            html=html+("<tr><td align='right' bgcolor='#40afbf'><b>")+tr("Obstructive")+("</b></td><td bgcolor='#40afbf'>")+a.sprintf("%.2f",oai)+("</td></tr>\n");
            html=html+("<tr><td align='right' bgcolor='#ff80ff'><b>")+tr("ClearAirway")+("</b></td><td bgcolor='#ff80ff'>")+a.sprintf("%.2f",cai)+("</td></tr>\n");
            html=html+("</table></td><td colspan=2><table cellspacing=0 cellpadding=2 border=0 width='100%'>");
            html=html+("<tr><td align='right' bgcolor='#ffff80'><b>")+tr("RERA")+("</b></td><td bgcolor='#ffff80'>")+a.sprintf("%.2f",rei)+("</td></tr>\n");
            html=html+("<tr><td align='right' bgcolor='#404040'><b><font color='white'>")+tr("FlowLimit")+("</font></b></td><td bgcolor='#404040'><font color='white'>")+a.sprintf("%.2f",fli)+("</font></td></tr>\n");
            html=html+("<tr><td align='right' bgcolor='#ff4040'><b>")+tr("Vsnore")+("</b></td><td bgcolor='#ff4040'>")+a.sprintf("%.2f",vsi)+("</td></tr>\n");
            html=html+("<tr><td align='right' bgcolor='#80ff80'><b>")+tr("PB/CSR")+("</b></td><td bgcolor='#80ff80'>")+a.sprintf("%.2f",csr)+("%</td></tr>\n");
            html=html+("</table></td></tr>");
        } else {
            html=html+("<tr><td colspan=2><table cellspacing=0 cellpadding=2 border=0 width='100%'>");
            html=html+("<tr><td align='right'><b><font color='black'>")+tr("AHI")+("</font></b></td><td><b><font color='black'>")+a.sprintf("%.2f",ahi)+("</font></b></td></tr>\n");
            html=html+("<tr><td align='right'><b>")+tr("Hypopnea")+("</b></td><td>")+a.sprintf("%.2f",hi)+("</td></tr>\n");
            html=html+("<tr><td align='right'><b>")+tr("Obstructive")+("</b></td><td>")+a.sprintf("%.2f",oai)+("</td></tr>\n");
            html=html+("<tr><td align='right'><b>")+tr("ClearAirway")+("</b></td><td>")+a.sprintf("%.2f",cai)+("</td></tr>\n");
            html=html+("</table></td><td colspan=2><table cellspacing=0 cellpadding=2 border=0 width='100%'>");
            html=html+("<tr><td align='right'><b>")+tr("RERA")+("</b></td><td>")+a.sprintf("%.2f",rei)+("</td></tr>\n");
            html=html+("<tr><td align='right'><b>")+tr("FlowLimit")+("</b></td><td>")+a.sprintf("%.2f",fli)+("</td></tr>\n");
            html=html+("<tr><td align='right'><b>")+tr("Vsnore")+("</b></td><td>")+a.sprintf("%.2f",vsi)+("</td></tr>\n");
            html=html+("<tr><td align='right'><b>")+tr("PB/CSR")+("</b></td><td>")+a.sprintf("%.2f%%",csr)+("</td></tr>\n");
            html=html+("</table></td></tr>");
        }
        html=html+("<tr><td colspan=4 align=center><i>")+tr("Event Breakdown")+("</i></td></tr>\n");
        {
            G_AHI->setFixedSize(gwwidth,gwheight);
            QPixmap pixmap=G_AHI->renderPixmap(gwwidth,gwheight,false);
            QByteArray byteArray;
            QBuffer buffer(&byteArray); // use buffer to store pixmap into byteArray
            buffer.open(QIODevice::WriteOnly);
            pixmap.save(&buffer, "PNG");
            html += "<tr><td colspan=4 align=center><img src=\"data:image/png;base64," + byteArray.toBase64() + "\"></td></tr>\n";
        }
        html=html+("</table>");
        html=html+("<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n");
        //html=html+("<tr><td colspan=4>&nbsp;</td></tr>\n");
        html=html+("<tr height='2'><td colspan=4 height='2'><hr></td></tr>\n");
        //html=html+wxT("<tr><td colspan=4 align=center><hr></td></tr>\n");

        if (mode==MODE_BIPAP) {
            html=html+("<tr><td colspan=4 align='center'><i>")+tr("90%&nbsp;EPAP ")+a.sprintf("%.2f",eap90)+"cmH2O</td></tr>\n";
            html=html+("<tr><td colspan=4 align='center'><i>")+tr("90%&nbsp;IPAP ")+a.sprintf("%.2f",iap90)+"</td></tr>\n";
        } else if (mode==MODE_APAP) {
            html=html+("<tr><td colspan=4 align='center'><i>")+tr("90%&nbsp;Pressure ")+a.sprintf("%.2f",cpap->summary_weighted_avg(CPAP_PressurePercentValue))+("</i></td></tr>\n");
        } else if (mode==MODE_CPAP) {
            html=html+("<tr><td colspan=4 align='center'><i>")+tr("Pressure ")+a.sprintf("%.2f",cpap->summary_min(CPAP_PressureMin))+("</i></td></tr>\n");
        }
        //html=html+("<tr><td colspan=4 align=center>&nbsp;</td></tr>\n");

        html=html+("<tr><td> </td><td><b>Min</b></td><td><b>Avg</b></td><td><b>Max</b></td></tr>");

        if (mode==MODE_APAP) {
            html=html+"<tr><td>Pressure</td><td>"+a.sprintf("%.2f",cpap->summary_min(CPAP_PressureMinAchieved));
            html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_weighted_avg(CPAP_PressureAverage));
            html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_max(CPAP_PressureMaxAchieved))+("</td></tr>");

            //  html=html+wxT("<tr><td><b>")+_("90%&nbsp;Pressure")+wxT("</b></td><td>")+wxString::Format(wxT("%.1fcmH2O"),p90)+wxT("</td></tr>\n");
        } else if (mode==MODE_BIPAP) {
            html=html+("<tr><td>EPAP</td><td>")+a.sprintf("%.2f",cpap->summary_min(BIPAP_EAPMin));
            html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_weighted_avg(BIPAP_EAPAverage));
            html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_max(BIPAP_EAPMax))+("</td></tr>");

            html=html+("<tr><td>IPAP</td><td>")+a.sprintf("%.2f",cpap->summary_min(BIPAP_IAPMin));
            html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_weighted_avg(BIPAP_IAPAverage));
            html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_max(BIPAP_IAPMax))+("</td></tr>");

        }
        html=html+("<tr><td>Leak");
        html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_min(CPAP_LeakMinimum));
        html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_weighted_avg(CPAP_LeakAverage));
        html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_max(CPAP_LeakMaximum))+("</td><tr>");

        html=html+("<tr><td>Snore");
        html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_min(CPAP_SnoreMinimum));
        html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_avg(CPAP_SnoreAverage));
        html=html+("</td><td>")+a.sprintf("%.2f",cpap->summary_max(CPAP_SnoreMaximum))+("</td><tr>");
        FRW->show();
        PRD->show();
        LEAK->show();
        SF->show();
        SNORE->show();
    } else {
        html+="<tr><td colspan=4 align=center><i>"+tr("No CPAP data available")+"</i></td></tr>";
        html+="<tr><td colspan=4>&nbsp;</td></tr>\n";

        //TAP_EAP->Show(false);
        //TAP_IAP->Show(false);
        //G_AHI->Show(false);
        FRW->hide();
        PRD->hide();
        LEAK->hide();
        SF->hide();
        SNORE->hide();
    }
    if (oxi) {
        html=html+("<tr><td>Pulse");
        html=html+("</td><td>")+a.sprintf("%.2fbpm",oxi->summary_min(OXI_PulseMin));
        html=html+("</td><td>")+a.sprintf("%.2fbpm",oxi->summary_avg(OXI_PulseAverage));
        html=html+("</td><td>")+a.sprintf("%.2fbpm",oxi->summary_max(OXI_PulseMax))+"</td><tr>";

        html=html+("<tr><td>SpO2");
        html=html+("</td><td>")+a.sprintf("%.2f%%",oxi->summary_min(OXI_SPO2Min));
        html=html+("</td><td>")+a.sprintf("%.2f%%",oxi->summary_avg(OXI_SPO2Average));
        html=html+("</td><td>")+a.sprintf("%.2f%%",oxi->summary_max(OXI_SPO2Max))+"</td><tr>";

        //html=html+wxT("<tr><td colspan=4>&nbsp;</td></tr>\n");

        PULSE->show();
        SPO2->show();
    } else {
        PULSE->hide();
        SPO2->hide();
    }
    if (!cpap && !oxi) {
        NoData->setText("No CPAP Data for "+date.toString(Qt::SystemLocaleLongDate));
        NoData->show();
    } else
        NoData->hide();

    //ui->graphSizer->invalidate();
    //ui->graphSizer->layout();
    //GraphWindow->FitInside();
    if (cpap) {
        if (mode==MODE_BIPAP) {

        } else if (mode==MODE_APAP) {
            html=html+("<tr><td colspan=4 align=center><i>")+tr("Time@Pressure")+("</i></td></tr>\n");
            TAP->setFixedSize(gwwidth,gwheight);

            QPixmap pixmap=TAP->renderPixmap(gwwidth,gwheight,false);
            QByteArray byteArray;
            QBuffer buffer(&byteArray); // use buffer to store pixmap into byteArray
            buffer.open(QIODevice::WriteOnly);
            pixmap.save(&buffer, "PNG");
            html += "<tr><td colspan=4 align=center><img src=\"data:image/png;base64," + byteArray.toBase64() + "\"></td></tr>\n";
        }
        html=html+("</table><hr height=2>");

        for (vector<Session *>::iterator s=cpap->begin();s!=cpap->end();s++) {
            tmp.sprintf(("%06i "+(*s)->first().toString("yyyy-MM-dd HH:mm ")+(*s)->last().toString("HH:mm")+"<br/>").toLatin1(),(*s)->session());
            html+=tmp;
        }
    }
    html+="</html>";

    //PRD->updateGL();
    ui->webView->setHtml(html);
    //frw->Update(cpap);
    //FRW->updateGL();

    ui->JournalNotes->clear();
    Session *journal=GetJournalSession(date);
    if (journal) {
        ui->JournalNotes->setHtml(journal->summary[GEN_Notes].toString());
    }

}
void Daily::Unload(QDate date)
{
    Session *journal=GetJournalSession(date);
    if (!ui->JournalNotes->toPlainText().isEmpty()) {
        QString jhtml=ui->JournalNotes->toHtml();
        if (journal) {
            if (journal->summary[GEN_Notes]!=jhtml) {
                journal->summary[GEN_Notes]=jhtml;
                journal->SetChanged(true);
            }

        } else {
            journal=CreateJournalSession(date);
            journal->summary[GEN_Notes]=jhtml;
            journal->SetChanged(true);
        }

    }
    if (journal) {
        Machine *jm=profile->GetMachine(MT_JOURNAL);
        if (jm) jm->SaveSession(journal);
    }
    UpdateCalendarDay(date);
}

void Daily::on_JournalNotesItalic_clicked()
{
    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QTextCharFormat format=cursor.charFormat();

    format.setFontItalic(!format.fontItalic());


    cursor.mergeCharFormat(format);
   //ui->JournalNotes->mergeCurrentCharFormat(format);

}

void Daily::on_JournalNotesBold_clicked()
{
    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QTextCharFormat format=cursor.charFormat();

    int fw=format.fontWeight();
    if (fw!=QFont::Bold)
        format.setFontWeight(QFont::Bold);
    else
        format.setFontWeight(QFont::Normal);

    cursor.mergeCharFormat(format);
    //ui->JournalNotes->mergeCurrentCharFormat(format);

}

void Daily::on_JournalNotesFontsize_activated(int index)
{
    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QTextCharFormat format=cursor.charFormat();

    QFont font=format.font();
    int fontsize=10;

    if (index==1) fontsize=15;
    else if (index==2) fontsize=25;

    font.setPointSize(fontsize);
    format.setFont(font);

    cursor.mergeCharFormat(format);
}

void Daily::on_JournalNotesColour_clicked()
{
    QColor col=QColorDialog::getColor(QColor("black"),this,tr("Pick a Colour")); //,QColorDialog::NoButtons);
    if (!col.isValid()) return;

    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QBrush b(col);
    QPalette newPalette = palette();
    newPalette.setColor(QPalette::ButtonText, col);
    ui->JournalNotesColour->setPalette(newPalette);

    QTextCharFormat format=cursor.charFormat();

    format.setForeground(b);

    cursor.setCharFormat(format);
}
Session * Daily::CreateJournalSession(QDate date)
{
    Machine *m=profile->GetMachine(MT_JOURNAL);
    if (!m) {
        m=new Machine(profile,0);
        m->SetClass("Journal");
        m->properties["Brand"]="Virtual";
        m->SetType(MT_JOURNAL);
        profile->AddMachine(m);
    }
    Session *sess=new Session(m,0);
    QDateTime dt;
    dt.setDate(date);
    dt.setTime(QTime(17,0)); //5pm to make sure it goes in the right day
    sess->set_first(dt);
    dt=dt.addSecs(3600);
    sess->set_last(dt);
    sess->SetChanged(true);
    m->AddSession(sess,profile);
    return sess;
}
Session * Daily::GetJournalSession(QDate date) // Get the first journal session
{
    Day *journal=profile->GetDay(date,MT_JOURNAL);
    if (!journal)
        return NULL; //CreateJournalSession(date);
    vector<Session *>::iterator s;
    s=journal->begin();
    if (s!=journal->end())
        return *s;
    return NULL;
}
void Daily::on_EnergySlider_sliderMoved(int position)
{
    Session *s=GetJournalSession(previous_date);
    if (!s)
        s=CreateJournalSession(previous_date);

    s->summary[GEN_Energy]=position;
    s->SetChanged(true);
}

void Daily::UpdateCPAPGraphs(Day *day)
{
    //if (!day) return;
    if (day) {
        day->OpenEvents();
        day->OpenWaveforms();
    }
    for (list<gPointData *>::iterator g=CPAPData.begin();g!=CPAPData.end();g++) {
        (*g)->Update(day);
    }
};

void Daily::UpdateOXIGraphs(Day *day)
{
    //if (!day) return;
    if (day) {
        day->OpenEvents();
        day->OpenWaveforms();
    }
    for (list<gPointData *>::iterator g=OXIData.begin();g!=OXIData.end();g++) {
        (*g)->Update(day);
    }
};


void Daily::on_treeWidget_itemSelectionChanged()
{
    QTreeWidgetItem *item=ui->treeWidget->selectedItems().at(0);
    QDateTime d;
    if (!item->text(1).isEmpty()) {
        d=d.fromString(item->text(1),"yyyy-MM-dd HH:mm:ss");
        double st=(d.addSecs(-180)).toMSecsSinceEpoch()/86400000.0;
        double et=(d.addSecs(180)).toMSecsSinceEpoch()/86400000.0;
        FRW->SetXBounds(st,et);
        SF->SetXBounds(st,et);
        PRD->SetXBounds(st,et);
        LEAK->SetXBounds(st,et);
        SNORE->SetXBounds(st,et);
    }
}
