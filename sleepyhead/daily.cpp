/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Daily Panel
 *
 * Copyright (c) 2011-2014 Mark Watkins <jedimark@users.sourceforge.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the Linux
 * distribution for more details. */

#include <QTextCharFormat>
#include <QPalette>
#include <QTextBlock>
#include <QColorDialog>
#include <QSpacerItem>
#include <QBuffer>
#include <QPixmap>
#include <QMessageBox>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSpacerItem>
#include <QWebFrame>
#include <QFontMetrics>
#include <QLabel>

#include <cmath>
//#include <QPrinter>
//#include <QProgressBar>

#include "daily.h"
#include "ui_daily.h"

#include "common_gui.h"
#include "SleepLib/profiles.h"
#include "SleepLib/session.h"
#include "Graphs/graphdata_custom.h"
#include "Graphs/gLineOverlay.h"
#include "Graphs/gFlagsLine.h"
#include "Graphs/gFooBar.h"
#include "Graphs/gXAxis.h"
#include "Graphs/gYAxis.h"
#include "Graphs/gSegmentChart.h"
#include "Graphs/gStatsLine.h"

//extern QProgressBar *qprogress;
extern MainWindow * mainwin;

const int min_height=150;

Daily::Daily(QWidget *parent,gGraphView * shared)
    :QWidget(parent), ui(new Ui::Daily)
{
    ui->setupUi(this);

    // Remove Incomplete Extras Tab
    //ui->tabWidget->removeTab(3);

    ZombieMeterMoved=false;
    BookmarksChanged=false;

    lastcpapday=NULL;

    QList<int> a;
    a.push_back(300);
    a.push_back(this->width()-300);
    ui->splitter_2->setStretchFactor(1,1);
    ui->splitter_2->setSizes(a);
    ui->splitter_2->setStretchFactor(1,1);

    layout=new QHBoxLayout(ui->graphMainArea);
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->setContentsMargins(0,0,0,0);

    dateDisplay=new QLabel("",this);
    dateDisplay->setAlignment(Qt::AlignCenter);
    dateDisplay->setTextFormat(Qt::RichText);
    ui->sessionBarLayout->addWidget(dateDisplay,1);

//    const bool sessbar_under_graphs=false;
//    if (sessbar_under_graphs) {
//        ui->sessionBarLayout->addWidget(sessbar,1);
//    } else {
//        ui->splitter->insertWidget(2,sessbar);
//        sessbar->setMaximumHeight(sessbar->height());
//        sessbar->setMinimumHeight(sessbar->height());
//    }

    sessbar=NULL;

    webView=new MyWebView(this);

    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);

    ui->tabWidget->insertTab(0,webView,QIcon(),"Details");

    ui->graphMainArea->setLayout(layout);
    //ui->graphMainArea->setLayout(layout);

    ui->graphMainArea->setAutoFillBackground(false);

    GraphView=new gGraphView(ui->graphMainArea,shared);
    GraphView->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    snapGV=new gGraphView(GraphView); //ui->graphMainArea);
    snapGV->setMinimumSize(172,172);
    snapGV->hideSplitter();
    snapGV->hide();

    scrollbar=new MyScrollBar(ui->graphMainArea);
    scrollbar->setOrientation(Qt::Vertical);
    scrollbar->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Expanding);
    scrollbar->setMaximumWidth(20);

    ui->bookmarkTable->setColumnCount(2);
    ui->bookmarkTable->setColumnWidth(0,70);
    //ui->bookmarkTable->setEditTriggers(QAbstractItemView::SelectedClicked);
    //ui->bookmarkTable->setColumnHidden(2,true);
    //ui->bookmarkTable->setColumnHidden(3,true);
    GraphView->setScrollBar(scrollbar);
    layout->addWidget(GraphView,1);
    layout->addWidget(scrollbar,0);

    int default_height=PROFILE.appearance->graphHeight();
    SF=new gGraph(GraphView,STR_TR_EventFlags,STR_TR_EventFlags,default_height);

    SF->setPinned(true);
    FRW=new gGraph(GraphView,STR_TR_FlowRate, schema::channel[CPAP_FlowRate].fullname()+"\n"+schema::channel[CPAP_FlowRate].description()+"\n("+schema::channel[CPAP_FlowRate].units()+")",default_height);
    //FRW->setPinned(true);


    if (PROFILE.general->calculateRDI()) {
        AHI=new gGraph(GraphView,STR_TR_RDI,   schema::channel[CPAP_RDI].fullname()+"\n"+schema::channel[CPAP_RDI].description()+"\n("+schema::channel[CPAP_RDI].units()+")",default_height);
    } else AHI=new gGraph(GraphView,STR_TR_AHI,schema::channel[CPAP_AHI].fullname()+"\n"+schema::channel[CPAP_AHI].description()+"\n("+schema::channel[CPAP_AHI].units()+")",default_height);

    MP=new gGraph(GraphView,schema::channel[CPAP_MaskPressure].label(), schema::channel[CPAP_MaskPressure].fullname()+"\n"+schema::channel[CPAP_MaskPressure].description()+"\n("+schema::channel[CPAP_MaskPressure].units()+")",default_height);
    PRD=new gGraph(GraphView,schema::channel[CPAP_Pressure].label(),    schema::channel[CPAP_Pressure].fullname()+"\n"+schema::channel[CPAP_Pressure].description()+"\n("+schema::channel[CPAP_Pressure].units()+")",default_height);
    LEAK=new gGraph(GraphView,STR_TR_Leak,                              schema::channel[CPAP_Leak].fullname()+"\n"+schema::channel[CPAP_Leak].description()+"\n("+schema::channel[CPAP_Leak].units()+")",default_height);
    SNORE=new gGraph(GraphView,STR_TR_Snore,                            schema::channel[CPAP_Snore].fullname()+"\n"+schema::channel[CPAP_Snore].description()+"\n("+schema::channel[CPAP_Snore].units()+")",default_height);
    RR=new gGraph(GraphView,STR_TR_RespRate,                            schema::channel[CPAP_RespRate].fullname()+"\n"+schema::channel[CPAP_RespRate].description()+"\n("+schema::channel[CPAP_RespRate].units()+")",default_height);
    TV=new gGraph(GraphView,STR_TR_TidalVolume,                         schema::channel[CPAP_TidalVolume].fullname()+"\n"+schema::channel[CPAP_TidalVolume].description()+"\n("+schema::channel[CPAP_TidalVolume].units()+")",default_height);
    MV=new gGraph(GraphView,STR_TR_MinuteVent,                          schema::channel[CPAP_MinuteVent].fullname()+"\n"+schema::channel[CPAP_MinuteVent].description()+"\n("+schema::channel[CPAP_MinuteVent].units()+")",default_height);
    //TgMV=new gGraph(GraphView,STR_TR_TgtMinVent,                      schema::channel[CPAP_TgMV].fullname()+"\n"+schema::channel[CPAP_TgMV].description()+"\n("+schema::channel[CPAP_TgMV].units()+")",default_height);
    FLG=new gGraph(GraphView,STR_TR_FlowLimit,                          schema::channel[CPAP_FLG].fullname()+"\n"+schema::channel[CPAP_FLG].description()+"\n("+schema::channel[CPAP_FLG].units()+")",default_height);
    PTB=new gGraph(GraphView,STR_TR_PatTrigBreath,                      schema::channel[CPAP_PTB].fullname()+"\n"+schema::channel[CPAP_PTB].description()+"\n("+schema::channel[CPAP_PTB].units()+")",default_height);
    RE=new gGraph(GraphView,STR_TR_RespEvent,                           schema::channel[CPAP_RespEvent].fullname()+"\n"+schema::channel[CPAP_RespEvent].description()+"\n("+schema::channel[CPAP_RespEvent].units()+")",default_height);
    TI=new gGraph(GraphView,STR_TR_InspTime,                            schema::channel[CPAP_Ti].fullname()+"\n"+schema::channel[CPAP_Ti].description()+"\n("+schema::channel[CPAP_Ti].units()+")",default_height);
    TE=new gGraph(GraphView,STR_TR_ExpTime,                             schema::channel[CPAP_Te].fullname()+"\n"+schema::channel[CPAP_Te].description()+"\n("+schema::channel[CPAP_Te].units()+")",default_height);
    IE=new gGraph(GraphView,schema::channel[CPAP_IE].label(),           schema::channel[CPAP_IE].fullname()+"\n"+schema::channel[CPAP_IE].description()+"\n("+schema::channel[CPAP_IE].units()+")",default_height);

    STAGE=new gGraph(GraphView,STR_TR_SleepStage,                       schema::channel[ZEO_SleepStage].fullname()+"\n"+schema::channel[ZEO_SleepStage].description()+"\n("+schema::channel[ZEO_SleepStage].units()+")",default_height);
    int oxigrp=PROFILE.ExistsAndTrue("SyncOximetry") ? 0 : 1;
    PULSE=new gGraph(GraphView,STR_TR_PulseRate,                        schema::channel[OXI_Pulse].fullname()+"\n"+schema::channel[OXI_Pulse].description()+"\n("+schema::channel[OXI_Pulse].units()+")",default_height,oxigrp);
    SPO2=new gGraph(GraphView,STR_TR_SpO2,                              schema::channel[OXI_SPO2].fullname()+"\n"+schema::channel[OXI_SPO2].description()+"\n("+schema::channel[OXI_SPO2].units()+")",default_height,oxigrp);
    INTPULSE=new gGraph(GraphView,tr("Int. Pulse"),                     schema::channel[OXI_Pulse].fullname()+"\n"+schema::channel[OXI_Pulse].description()+"\n("+schema::channel[OXI_Pulse].units()+")",default_height,oxigrp);
    INTSPO2=new gGraph(GraphView,tr("Int. SpO2"),                       schema::channel[OXI_SPO2].fullname()+"\n"+schema::channel[OXI_SPO2].description()+"\n("+schema::channel[OXI_SPO2].units()+")",default_height,oxigrp);
    PLETHY=new gGraph(GraphView,STR_TR_Plethy,                          schema::channel[OXI_Plethy].fullname()+"\n"+schema::channel[OXI_Plethy].description()+"\n("+schema::channel[OXI_Plethy].units()+")",default_height,oxigrp);

    // Event Pie Chart (for snapshot purposes)
    // TODO: Convert snapGV to generic for snapshotting multiple graphs (like reports does)
//    TAP=new gGraph(GraphView,"Time@Pressure",STR_UNIT_CMH2O,100);
//    TAP->showTitle(false);
//    gTAPGraph * tap=new gTAPGraph(CPAP_Pressure,GST_Line);
//    TAP->AddLayer(AddCPAP(tap));
    //TAP->setMargins(0,0,0,0);


    GAHI=new gGraph(snapGV,tr("Breakdown"),tr("events"),172);
    gSegmentChart * evseg=new gSegmentChart(GST_Pie);
    evseg->AddSlice(CPAP_Hypopnea,QColor(0x40,0x40,0xff,0xff),STR_TR_H);
    evseg->AddSlice(CPAP_Apnea,QColor(0x20,0x80,0x20,0xff),STR_TR_UA);
    evseg->AddSlice(CPAP_Obstructive,QColor(0x40,0xaf,0xbf,0xff),STR_TR_OA);
    evseg->AddSlice(CPAP_ClearAirway,QColor(0xb2,0x54,0xcd,0xff),STR_TR_CA);
    evseg->AddSlice(CPAP_RERA,QColor(0xff,0xff,0x80,0xff),STR_TR_RE);
    evseg->AddSlice(CPAP_NRI,QColor(0x00,0x80,0x40,0xff),STR_TR_NR);
    evseg->AddSlice(CPAP_FlowLimit,QColor(0x40,0x40,0x40,0xff),STR_TR_FL);
    //evseg->AddSlice(CPAP_UserFlag1,QColor(0x40,0x40,0x40,0xff),tr("UF"));

    GAHI->AddLayer(AddCPAP(evseg));
    GAHI->setMargins(0,0,0,0);
    //SF->AddLayer(AddCPAP(evseg),LayerRight,100);

    gFlagsGroup *fg=new gFlagsGroup();
    SF->AddLayer(AddCPAP(fg));
    fg->AddLayer((new gFlagsLine(CPAP_CSR, COLOR_CSR, STR_TR_PB,false,FT_Span)));
    fg->AddLayer((new gFlagsLine(CPAP_ClearAirway, COLOR_ClearAirway, STR_TR_CA,false)));
    fg->AddLayer((new gFlagsLine(CPAP_Obstructive, COLOR_Obstructive, STR_TR_OA,true)));
    fg->AddLayer((new gFlagsLine(CPAP_Apnea, COLOR_Apnea, STR_TR_UA)));
    fg->AddLayer((new gFlagsLine(CPAP_Hypopnea, COLOR_Hypopnea, STR_TR_H,true)));
    fg->AddLayer((new gFlagsLine(CPAP_ExP, COLOR_ExP, STR_TR_EP,false)));
    fg->AddLayer((new gFlagsLine(CPAP_LeakFlag, COLOR_LeakFlag, STR_TR_LE,false)));
    fg->AddLayer((new gFlagsLine(CPAP_NRI, COLOR_NRI, STR_TR_NRI,false)));
    fg->AddLayer((new gFlagsLine(CPAP_FlowLimit, COLOR_FlowLimit, STR_TR_FL)));
    fg->AddLayer((new gFlagsLine(CPAP_RERA, COLOR_RERA, STR_TR_RE)));
    fg->AddLayer((new gFlagsLine(CPAP_VSnore, COLOR_VibratorySnore, STR_TR_VS)));
    fg->AddLayer((new gFlagsLine(CPAP_VSnore2, COLOR_VibratorySnore, STR_TR_VS2)));
    if (PROFILE.cpap->userEventFlagging()) {
        fg->AddLayer((new gFlagsLine(CPAP_UserFlag1, COLOR_Yellow, STR_TR_UF1)));
        fg->AddLayer((new gFlagsLine(CPAP_UserFlag2, COLOR_DarkGreen, STR_TR_UF2)));
        fg->AddLayer((new gFlagsLine(CPAP_UserFlag3, COLOR_Brown, STR_TR_UF3)));
    }
    //fg->AddLayer((new gFlagsLine(PRS1_0B,COLOR_DarkGreen,tr("U0B"))));
    SF->setBlockZoom(true);
    SF->AddLayer(new gShadowArea());

    SF->AddLayer(new gFlagsLabelArea(fg),LayerLeft,gYAxis::Margin);
    //SF->AddLayer(new gFooBar(),LayerBottom,0,1);
    SF->AddLayer(new gXAxis(COLOR_Text,false),LayerBottom,0,20); //gXAxis::Margin);


    gLineChart *l;
    l=new gLineChart(CPAP_FlowRate,COLOR_Black,false,false);
    gLineOverlaySummary *los=new gLineOverlaySummary(tr("Selection AHI"),5,-4);
    AddCPAP(l);
    FRW->AddLayer(new gXGrid());
    FRW->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_CSR, COLOR_CSR, STR_TR_CSR,FT_Span)));
    FRW->AddLayer(l);
    FRW->AddLayer(new gYAxis(),LayerLeft,gYAxis::Margin);
    FRW->AddLayer(new gXAxis(),LayerBottom,0,20);
    FRW->AddLayer(AddCPAP(los->add(new gLineOverlayBar(CPAP_Hypopnea,COLOR_Hypopnea,STR_TR_H))));
    FRW->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_PressurePulse,COLOR_PressurePulse,STR_TR_PP,FT_Dot)));
    //FRW->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_Pressure, COLOR_White,STR_TR_P,FT_Dot)));
    FRW->AddLayer(AddCPAP(new gLineOverlayBar(PRS1_0B,COLOR_Blue,"0B",FT_Dot)));
    FRW->AddLayer(AddCPAP(new gLineOverlayBar(PRS1_10,COLOR_Orange,"10",FT_Dot)));
    FRW->AddLayer(AddCPAP(new gLineOverlayBar(PRS1_0E,COLOR_DarkRed,"0E",FT_Dot)));
    if (PROFILE.general->calculateRDI())
        FRW->AddLayer(AddCPAP(los->add(new gLineOverlayBar(CPAP_RERA, COLOR_RERA, STR_TR_RE))));
    else
        FRW->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_RERA, COLOR_RERA, STR_TR_RE)));

    FRW->AddLayer(AddCPAP(los->add(new gLineOverlayBar(CPAP_Apnea, COLOR_Apnea, STR_TR_UA))));
    FRW->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_VSnore, COLOR_VibratorySnore, STR_TR_VS)));
    FRW->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_FlowLimit, COLOR_FlowLimit, STR_TR_FL)));
    FRW->AddLayer(AddCPAP(los->add(new gLineOverlayBar(CPAP_Obstructive, COLOR_Obstructive, STR_TR_OA))));
    FRW->AddLayer(AddCPAP(los->add(new gLineOverlayBar(CPAP_ClearAirway, COLOR_ClearAirway, STR_TR_CA))));
    if (PROFILE.cpap->userEventFlagging()) {
        FRW->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_UserFlag1, COLOR_Yellow, tr("U1"),FT_Bar)));
        FRW->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_UserFlag2, COLOR_Orange, tr("U2"),FT_Bar)));
        FRW->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_UserFlag3, COLOR_Brown, tr("U3"),FT_Bar)));
    }
    FRW->AddLayer(AddCPAP(new gLineOverlayBar(OXI_SPO2Drop, COLOR_SPO2Drop, STR_TR_O2)));

    FRW->AddLayer(AddOXI(new gLineOverlayBar(OXI_SPO2Drop, COLOR_SPO2Drop, STR_TR_O2)));
    //FRW->AddLayer(AddOXI(new gLineOverlayBar(OXI_PulseChange, COLOR_PulseChange, STR_TR_PC,FT_Dot)));

    FRW->AddLayer(AddCPAP(los));


    gGraph *graphs[]={ PRD, LEAK, AHI, SNORE, PTB, MP, RR, MV, TV, FLG, IE, TI, TE, SPO2, PLETHY, PULSE, STAGE, INTSPO2, INTPULSE};
    int ng=sizeof(graphs)/sizeof(gGraph*);
    for (int i=0;i<ng;i++){
        graphs[i]->AddLayer(new gXGrid());
    }
    /*PRD->AddLayer(AddCPAP(new gStatsLine(CPAP_Pressure,"Pressure")),LayerBottom,0,20,1);
    PRD->AddLayer(AddCPAP(new gStatsLine(CPAP_EPAP,"EPAP")),LayerBottom,0,20,1);
    PRD->AddLayer(AddCPAP(new gStatsLine(CPAP_IPAP,"IPAP")),LayerBottom,0,20,1);
    LEAK->AddLayer(AddCPAP(new gStatsLine(CPAP_Leak)),LayerBottom,0,20,1);
    SNORE->AddLayer(AddCPAP(new gStatsLine(CPAP_Snore)),LayerBottom,0,20,1);
    PTB->AddLayer(AddCPAP(new gStatsLine(CPAP_PatientTriggeredBreaths)),LayerBottom,0,20,1);
    RR->AddLayer(AddCPAP(new gStatsLine(CPAP_RespiratoryRate)),LayerBottom,0,20,1);
    MV->AddLayer(AddCPAP(new gStatsLine(CPAP_MinuteVentilation)),LayerBottom,0,20,1);
    TV->AddLayer(AddCPAP(new gStatsLine(CPAP_TidalVolume)),LayerBottom,0,20,1);
    FLG->AddLayer(AddCPAP(new gStatsLine(CPAP_FlowLimitGraph)),LayerBottom,0,20,1);
    IE->AddLayer(AddCPAP(new gStatsLine(CPAP_IE)),LayerBottom,0,20,1);
    TE->AddLayer(AddCPAP(new gStatsLine(CPAP_Te)),LayerBottom,0,20,1);
    TI->AddLayer(AddCPAP(new gStatsLine(CPAP_Ti)),LayerBottom,0,20,1); */


    bool square=PROFILE.appearance->squareWavePlots();
    gLineChart *pc=new gLineChart(CPAP_Pressure, COLOR_Pressure, square);
    PRD->AddLayer(AddCPAP(pc));
    pc->addPlot(CPAP_EPAP, COLOR_EPAP, square);
    pc->addPlot(CPAP_IPAPLo, COLOR_IPAPLo, square);
    pc->addPlot(CPAP_IPAP, COLOR_IPAP, square);
    pc->addPlot(CPAP_IPAPHi, COLOR_IPAPHi, square);

    if (PROFILE.general->calculateRDI()) {
        AHI->AddLayer(AddCPAP(new gLineChart(CPAP_RDI, COLOR_RDI, square)));
//        AHI->AddLayer(AddCPAP(new AHIChart(QColor("#37a24b"))));
    } else {
        AHI->AddLayer(AddCPAP(new gLineChart(CPAP_AHI, COLOR_AHI, square)));
    }

    gLineChart *lc=new gLineChart(CPAP_LeakTotal, COLOR_LeakTotal, square);
    lc->addPlot(CPAP_Leak, COLOR_Leak, square);
    lc->addPlot(CPAP_MaxLeak, COLOR_MaxLeak, square);
    LEAK->AddLayer(AddCPAP(lc));
    //LEAK->AddLayer(AddCPAP(new gLineChart(CPAP_Leak, COLOR_Leak,square)));
    //LEAK->AddLayer(AddCPAP(new gLineChart(CPAP_MaxLeak, COLOR_MaxLeak,square)));
    SNORE->AddLayer(AddCPAP(new gLineChart(CPAP_Snore, COLOR_Snore, true)));

    PTB->AddLayer(AddCPAP(new gLineChart(CPAP_PTB, COLOR_PTB, square)));
    MP->AddLayer(AddCPAP(new gLineChart(CPAP_MaskPressure, COLOR_MaskPressure, false)));
    RR->AddLayer(AddCPAP(lc=new gLineChart(CPAP_RespRate, COLOR_RespRate, square)));

    // Delete me!!
//    lc->addPlot(CPAP_Test1, COLOR_DarkRed,square);

    MV->AddLayer(AddCPAP(lc=new gLineChart(CPAP_MinuteVent, COLOR_MinuteVent, square)));
    lc->addPlot(CPAP_TgMV,COLOR_TgMV,square);

    TV->AddLayer(AddCPAP(lc=new gLineChart(CPAP_TidalVolume,COLOR_TidalVolume,square)));
    //lc->addPlot(CPAP_Test2,COLOR_DarkYellow,square);



    //TV->AddLayer(AddCPAP(new gLineChart("TidalVolume2",COLOR_Magenta,square)));
    FLG->AddLayer(AddCPAP(new gLineChart(CPAP_FLG, COLOR_FLG, true)));
    //RE->AddLayer(AddCPAP(new gLineChart(CPAP_RespiratoryEvent,COLOR_Magenta,true)));
    IE->AddLayer(AddCPAP(lc=new gLineChart(CPAP_IE, COLOR_IE, square)));
    TE->AddLayer(AddCPAP(lc=new gLineChart(CPAP_Te, COLOR_Te, square)));
    TI->AddLayer(AddCPAP(lc=new gLineChart(CPAP_Ti, COLOR_Ti, square)));
    //lc->addPlot(CPAP_Test2,COLOR:DarkYellow,square);

    STAGE->AddLayer(AddSTAGE(new gLineChart(ZEO_SleepStage, COLOR_SleepStage, true)));

    gLineOverlaySummary *los1=new gLineOverlaySummary(tr("Events/hour"),5,-4);
    gLineOverlaySummary *los2=new gLineOverlaySummary(tr("Events/hour"),5,-4);
    PULSE->AddLayer(AddOXI(los1->add(new gLineOverlayBar(OXI_PulseChange, COLOR_PulseChange, STR_TR_PC,FT_Span))));
    PULSE->AddLayer(AddOXI(los1));
    SPO2->AddLayer(AddOXI(los2->add(new gLineOverlayBar(OXI_SPO2Drop, COLOR_SPO2Drop, STR_TR_O2,FT_Span))));
    SPO2->AddLayer(AddOXI(los2));

    PULSE->AddLayer(AddOXI(new gLineChart(OXI_Pulse, COLOR_Pulse, square)));
    SPO2->AddLayer(AddOXI(new gLineChart(OXI_SPO2, COLOR_SPO2, true)));
    PLETHY->AddLayer(AddOXI(new gLineChart(OXI_Plethy, COLOR_Plethy,false)));

    gLineOverlaySummary *los3=new gLineOverlaySummary(tr("Events/hour"),5,-4);
    gLineOverlaySummary *los4=new gLineOverlaySummary(tr("Events/hour"),5,-4);

    INTPULSE->AddLayer(AddCPAP(los3->add(new gLineOverlayBar(OXI_PulseChange, COLOR_PulseChange, STR_TR_PC,FT_Span))));
    INTPULSE->AddLayer(AddCPAP(los3));
    INTSPO2->AddLayer(AddCPAP(los4->add(new gLineOverlayBar(OXI_SPO2Drop, COLOR_SPO2Drop, STR_TR_O2,FT_Span))));
    INTSPO2->AddLayer(AddCPAP(los4));
    INTPULSE->AddLayer(AddCPAP(new gLineChart(OXI_Pulse, COLOR_Pulse, square)));
    INTSPO2->AddLayer(AddCPAP(new gLineChart(OXI_SPO2, COLOR_SPO2, true)));


    PTB->setForceMaxY(100);
    SPO2->setForceMaxY(100);
    INTSPO2->setForceMaxY(100);

    for (int i=0;i<ng;i++){
        graphs[i]->AddLayer(new gYAxis(),LayerLeft,gYAxis::Margin);
        graphs[i]->AddLayer(new gXAxis(),LayerBottom,0,20);
    }

    layout->layout();

    QTextCharFormat format = ui->calendar->weekdayTextFormat(Qt::Saturday);
    format.setForeground(QBrush(COLOR_Black, Qt::SolidPattern));
    ui->calendar->setWeekdayTextFormat(Qt::Saturday, format);
    ui->calendar->setWeekdayTextFormat(Qt::Sunday, format);

    Qt::DayOfWeek dow=firstDayOfWeekFromLocale();

    ui->calendar->setFirstDayOfWeek(dow);

    ui->tabWidget->setCurrentWidget(webView);

    webView->settings()->setFontSize(QWebSettings::DefaultFontSize,QApplication::font().pointSize());
    webView->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    connect(webView,SIGNAL(linkClicked(QUrl)),this,SLOT(Link_clicked(QUrl)));

    int ews=PROFILE.general->eventWindowSize();
    ui->evViewSlider->setValue(ews);
    ui->evViewLCD->display(ews);

    GraphView->LoadSettings("Daily");

    icon_on=new QIcon(":/icons/session-on.png");
    icon_off=new QIcon(":/icons/session-off.png");

    ui->splitter->setVisible(false);

    if (PROFILE.general->unitSystem()==US_Archiac) {
        ui->weightSpinBox->setSuffix(STR_UNIT_POUND);
        ui->weightSpinBox->setDecimals(0);
        ui->ouncesSpinBox->setVisible(true);
        ui->ouncesSpinBox->setSuffix(STR_UNIT_OUNCE);
    } else {
        ui->ouncesSpinBox->setVisible(false);
        ui->weightSpinBox->setDecimals(3);
        ui->weightSpinBox->setSuffix(STR_UNIT_KG);
    }
    GraphView->setCubeImage(images["nodata"]);
    GraphView->setEmptyText(STR_TR_NoData);
    previous_date=QDate();
}

Daily::~Daily()
{
    GraphView->SaveSettings("Daily");
//    disconnect(sessbar, SIGNAL(toggledSession(Session*)), this, SLOT(doToggleSession(Session*)));

    disconnect(webView,SIGNAL(linkClicked(QUrl)),this,SLOT(Link_clicked(QUrl)));
    // Save any last minute changes..
    if (previous_date.isValid())
        Unload(previous_date);

//    delete splitter;
    delete ui;
    delete icon_on;
    delete icon_off;
}

void Daily::doToggleSession(Session * sess)
{
    Q_UNUSED(sess)
   // sess->StoreSummary();
    Day *day=PROFILE.GetDay(previous_date,MT_CPAP);
    if (day) {
        day->machine->Save();
        this->LoadDate(previous_date);
    }
}

void Daily::Link_clicked(const QUrl &url)
{
    QString code=url.toString().section("=",0,0).toLower();
    QString data=url.toString().section("=",1);
    int sid=data.toInt();
    Day *day=NULL;
    if (code=="togglecpapsession") { // Enable/Disable CPAP session
        day=PROFILE.GetDay(previous_date,MT_CPAP);
        Session *sess=day->find(sid);
        if (!sess)
            return;
        int i=webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-webView->page()->mainFrame()->scrollBarValue(Qt::Vertical);
        sess->setEnabled(!sess->enabled());

        // Messy, this rewrites both summary & events.. TODO: Write just the session summary file
        day->machine->Save();

        // Reload day
        this->LoadDate(previous_date);
        webView->page()->mainFrame()->setScrollBarValue(Qt::Vertical, webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-i);
        return;
    } else  if (code=="toggleoxisession") { // Enable/Disable Oximetry session
        day=PROFILE.GetDay(previous_date,MT_OXIMETER);
        Session *sess=day->find(sid);
        if (!sess)
            return;
        int i=webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-webView->page()->mainFrame()->scrollBarValue(Qt::Vertical);
        sess->setEnabled(!sess->enabled());
        // Messy, this rewrites both summary & events.. TODO: Write just the session summary file
        day->machine->Save();

        // Reload day
        this->LoadDate(previous_date);
        webView->page()->mainFrame()->setScrollBarValue(Qt::Vertical, webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-i);
        return;
    } else if (code=="cpap")  {
        day=PROFILE.GetDay(previous_date,MT_CPAP);
    } else if (code=="oxi") {
        day=PROFILE.GetDay(previous_date,MT_OXIMETER);
        Session *sess=day->machine->sessionlist[sid];
        if (mainwin->getOximetry()) {
            mainwin->getOximetry()->openSession(sess);
            mainwin->selectOximetryTab();
        }
        return;
    } else if (code=="event")  {
        QList<QTreeWidgetItem *> list=ui->treeWidget->findItems(schema::channel[sid].fullname(),Qt::MatchContains);
        if (list.size()>0) {
            ui->treeWidget->collapseAll();
            ui->treeWidget->expandItem(list.at(0));
            QTreeWidgetItem *wi=list.at(0)->child(0);
            ui->treeWidget->setCurrentItem(wi);
            ui->tabWidget->setCurrentIndex(1);
        } else {
            mainwin->Notify(tr("No %1 events are recorded this day").arg(schema::channel[sid].fullname()),"",1500);
        }
    } else if (code=="graph") {
        qDebug() << "Select graph " << data;
    } else {
        qDebug() << "Clicked on" << code << data;
    }
    if (day) {

        Session *sess=day->machine->sessionlist[sid];
        if (sess && sess->enabled()) {
            GraphView->SetXBounds(sess->first(),sess->last());
        }
    }
}

void Daily::ReloadGraphs()
{
    ui->splitter->setVisible(true);
    QDate d;

    if (previous_date.isValid()) {
        d=previous_date;
//        Unload(d);
    }
    d=PROFILE.LastDay();
    if (!d.isValid()) {
        d=ui->calendar->selectedDate();
    }
    on_calendar_currentPageChanged(d.year(),d.month());
    // this fires a signal which unloads the old and loads the new
    ui->calendar->setSelectedDate(d);
    //Load(d);
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
    tree->clear();
    if (!day) return;

    tree->setColumnCount(1); // 1 visible common.. (1 hidden)

    QTreeWidgetItem *root=NULL;
    QHash<ChannelID,QTreeWidgetItem *> mcroot;
    QHash<ChannelID,int> mccnt;
    int total_events=0;
    bool userflags=p_profile->cpap->userEventFlagging();

    qint64 drift=0, clockdrift=PROFILE.cpap->clockDrift()*1000L;
    for (QList<Session *>::iterator s=day->begin();s!=day->end();++s) {
        if (!(*s)->enabled()) continue;

        QHash<ChannelID,QVector<EventList *> >::iterator m;

        for (m=(*s)->eventlist.begin();m!=(*s)->eventlist.end();m++) {
            ChannelID code=m.key();
            if ((code!=CPAP_Obstructive)
                && (code!=CPAP_Hypopnea)
                && (code!=CPAP_Apnea)
                && (code!=PRS1_0B)
                && (code!=CPAP_ClearAirway)
                && (code!=CPAP_CSR)
                && (code!=CPAP_RERA)
                && (code!=CPAP_UserFlag1)
                && (code!=CPAP_UserFlag2)
                && (code!=CPAP_UserFlag3)
                && (code!=CPAP_NRI)
                && (code!=CPAP_LeakFlag)
                && (code!=CPAP_ExP)
                && (code!=CPAP_FlowLimit)
                && (code!=CPAP_PressurePulse)
                && (code!=CPAP_VSnore2)
                && (code!=CPAP_VSnore)) continue;

            if (!userflags && ((code==CPAP_UserFlag1) || (code==CPAP_UserFlag2) || (code==CPAP_UserFlag3))) continue;
            drift=(*s)->machine()->GetType()==MT_CPAP ? clockdrift : 0;

            QTreeWidgetItem *mcr;
            if (mcroot.find(code)==mcroot.end()) {
                int cnt=day->count(code);
                if (!cnt) continue; // If no events than don't bother showing..
                total_events+=cnt;
                QString st=schema::channel[code].fullname();
                if (st.isEmpty())  {
                    st=QString("Fixme %1").arg(code);
                }
                st+=" ";
                if (cnt==1) st+=tr("%1 event").arg(cnt);
                else st+=tr("%1 events").arg(cnt);

                QStringList l(st);
                l.append("");
                mcroot[code]=mcr=new QTreeWidgetItem(root,l);
                mccnt[code]=0;
            } else {
                mcr=mcroot[code];
            }

            for (int z=0;z<m.value().size();z++) {
                EventList & ev=*(m.value()[z]);

                for (quint32 o=0;o<ev.count();o++) {
                    qint64 t=ev.time(o)+drift;

                    if (code==CPAP_CSR) { // center it in the middle of span
                        t-=float(ev.raw(o)/2.0)*1000.0;
                    }
                    QStringList a;
                    QDateTime d=QDateTime::fromTime_t(t/1000L);
                    QString s=QString("#%1: %2 (%3)").arg((int)(++mccnt[code]),(int)3,(int)10,QChar('0')).arg(d.toString("HH:mm:ss")).arg(m.value()[z]->raw(o));
                    a.append(s);
                    QTreeWidgetItem *item=new QTreeWidgetItem(a);
                    item->setData(0,Qt::UserRole,t);
                    //a.append(d.toString("yyyy-MM-dd HH:mm:ss"));
                    mcr->addChild(item);
                }
            }
        }
    }
    int cnt=0;
    for (QHash<ChannelID,QTreeWidgetItem *>::iterator m=mcroot.begin();m!=mcroot.end();m++) {
        tree->insertTopLevelItem(cnt++,m.value());
    }
    //tree->insertTopLevelItem(cnt++,new QTreeWidgetItem(QStringList("[Total Events ("+QString::number(total_events)+")]")));
    tree->sortByColumn(0,Qt::AscendingOrder);
    //tree->expandAll();
}

void Daily::UpdateCalendarDay(QDate date)
{
    QTextCharFormat nodata;
    QTextCharFormat cpaponly;
    QTextCharFormat cpapjour;
    QTextCharFormat oxiday;
    QTextCharFormat oxicpap;
    QTextCharFormat jourday;
    QTextCharFormat stageday;

    cpaponly.setForeground(QBrush(COLOR_Blue, Qt::SolidPattern));
    cpaponly.setFontWeight(QFont::Normal);
    cpapjour.setForeground(QBrush(COLOR_Blue, Qt::SolidPattern));
    cpapjour.setFontWeight(QFont::Bold);
    oxiday.setForeground(QBrush(COLOR_Red, Qt::SolidPattern));
    oxiday.setFontWeight(QFont::Normal);
    oxicpap.setForeground(QBrush(COLOR_Red, Qt::SolidPattern));
    oxicpap.setFontWeight(QFont::Bold);
    stageday.setForeground(QBrush(COLOR_Magenta, Qt::SolidPattern));
    stageday.setFontWeight(QFont::Bold);
    jourday.setForeground(QBrush(COLOR_Black, Qt::SolidPattern));
    jourday.setFontWeight(QFont::Bold);
    nodata.setForeground(QBrush(COLOR_Black, Qt::SolidPattern));
    nodata.setFontWeight(QFont::Normal);

    bool hascpap=PROFILE.GetDay(date,MT_CPAP)!=NULL;
    bool hasoxi=PROFILE.GetDay(date,MT_OXIMETER)!=NULL;
    bool hasjournal=PROFILE.GetDay(date,MT_JOURNAL)!=NULL;
    bool hasstage=PROFILE.GetDay(date,MT_SLEEPSTAGE)!=NULL;
    if (hascpap) {
        if (hasoxi) {
            ui->calendar->setDateTextFormat(date,oxicpap);
        } else if (hasjournal) {
            ui->calendar->setDateTextFormat(date,cpapjour);
        } else if (hasstage) {
            ui->calendar->setDateTextFormat(date,stageday);
        } else {
            ui->calendar->setDateTextFormat(date,cpaponly);
        }
    } else if (hasoxi) {
        ui->calendar->setDateTextFormat(date,oxiday);
    } else if (hasjournal) {
        ui->calendar->setDateTextFormat(date,jourday);
    } else {
        ui->calendar->setDateTextFormat(date,nodata);
    }
    ui->calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);

}
void Daily::LoadDate(QDate date)
{
    ui->calendar->blockSignals(true);
    if (date.month()!=previous_date.month()) {
        on_calendar_currentPageChanged(date.year(),date.month());
    }
    ui->calendar->setSelectedDate(date);
    ui->calendar->blockSignals(false);
    on_calendar_selectionChanged();
}

void Daily::on_calendar_selectionChanged()
{
    QTime time;
    time_t unload_time, load_time, other_time;
    time.start();

    this->setCursor(Qt::BusyCursor);
    if (previous_date.isValid()) {
       // GraphView->fadeOut();
        Unload(previous_date);
    }
    unload_time=time.restart();
    //bool fadedir=previous_date < ui->calendar->selectedDate();
    ZombieMeterMoved=false;
    Load(ui->calendar->selectedDate());
    load_time=time.restart();

    //GraphView->fadeIn(fadedir);
    GraphView->redraw();
    ui->calButton->setText(ui->calendar->selectedDate().toString(Qt::TextDate));
    ui->calendar->setFocus(Qt::ActiveWindowFocusReason);

    if (PROFILE.general->unitSystem()==US_Archiac) {
        ui->weightSpinBox->setSuffix(STR_UNIT_POUND);
        ui->weightSpinBox->setDecimals(0);
        ui->ouncesSpinBox->setVisible(true);
        ui->ouncesSpinBox->setSuffix(STR_UNIT_OUNCE);
    } else {
        ui->ouncesSpinBox->setVisible(false);
        ui->weightSpinBox->setDecimals(3);
        ui->weightSpinBox->setSuffix(STR_UNIT_KG);
    }
    this->setCursor(Qt::ArrowCursor);
    other_time=time.restart();

    qDebug() << "Page change time (in ms): Unload ="<<unload_time<<"Load =" << load_time << "Other =" << other_time;
}
void Daily::ResetGraphLayout()
{
    GraphView->resetLayout();
}
void Daily::graphtogglebutton_toggled(bool b)
{
    Q_UNUSED(b)
    for (int i=0;i<GraphView->size();i++) {
        QString title=(*GraphView)[i]->title();
        (*GraphView)[i]->setVisible(GraphToggles[title]->isChecked());
    }
    GraphView->updateScale();
    GraphView->redraw();
}

MyWebPage::MyWebPage(QObject *parent):
   QWebPage(parent)
{
   // Enable plugin support
   settings()->setAttribute(QWebSettings::PluginsEnabled, true);
}

QObject *MyWebPage::createPlugin(const QString &classid, const QUrl &url, const QStringList & paramNames, const QStringList & paramValues)
{
    Q_UNUSED(paramNames)
    Q_UNUSED(paramValues)
    Q_UNUSED(url)

    if (classid=="SessionBar") {
        return mainwin->getDaily()->sessionBar();
    }
    qDebug() << "Request for unknown MyWebPage plugin";
    return new QWidget();
}

MyWebView::MyWebView(QWidget *parent):
   QWebView(parent),
   m_page(this)
{
   // Set the page of our own PageView class, MyPageView,
   // because only objects of this class will handle
   // object-tags correctly.
   setPage(&m_page);
}


QString Daily::getSessionInformation(Day * cpap, Day * oxi, Day * stage)
{
    QString html;
    QList<Day *> list;
    if (cpap) list.push_back(cpap);
    if (oxi) list.push_back(oxi);
    if (stage) list.push_back(stage);

    if (list.isEmpty())
        return html;

    html="<table cellpadding=0 cellspacing=0 border=0 width=100%>";
    html+=QString("<tr><td colspan=5 align=center><b>"+tr("Session Information")+"</b></td></tr>");
    html+="<tr><td colspan=5 align=center>&nbsp;</td></tr>";
    QFontMetrics FM(*defaultfont);
    QRect r=FM.boundingRect('@');
    if (cpap) {
        html+=QString("<tr><td colspan=5 align=center>"
        "<object type=\"application/x-qt-plugin\" classid=\"SessionBar\" name=\"sessbar\" height=%1 width=100%></object>"
//        "<script>"
//        "sessbar.show();"
//        "</script>"
        "</td></tr>").arg(r.height()*3,0,10);
        html+="<tr><td colspan=5 align=center>&nbsp; </td></tr>";
    }


    QDateTime fd,ld;
    bool corrupted_waveform=false;
    QString tooltip;

    html+=QString("<tr><td><b>"+STR_TR_On+"</b></td><td align=center><b>"+STR_TR_Date+"</b></td><td align=center><b>"+STR_TR_Start+"</b></td><td align=center><b>"+STR_TR_End+"</b></td><td align=left><b>"+tr("Duration")+"</b></td></tr>");
    QList<Day *>::iterator di;
    QString type;

    for (di=list.begin();di!=list.end();di++) {
        Day * day=*di;
        html+="<tr><td align=left colspan=5><i>";
        switch (day->machine_type()) {
        case MT_CPAP: type="cpap";
            html+=tr("CPAP Sessions");
            break;
        case MT_OXIMETER: type="oxi";
            html+=tr("Oximetery Sessions");
            break;
        case MT_SLEEPSTAGE: type="stage";
            html+=tr("Sleep Stage Sessions");
            break;
        default:
            type="unknown";
            html+=tr("Unknown Session");
            break;
        }
        html+="</i></td></tr>\n";
        for (QList<Session *>::iterator s=day->begin();s!=day->end();++s) {

            if ((day->machine_type()==MT_CPAP) &&
                ((*s)->settings.find(CPAP_BrokenWaveform)!=(*s)->settings.end()))
                    corrupted_waveform=true;

            fd=QDateTime::fromTime_t((*s)->first()/1000L);
            ld=QDateTime::fromTime_t((*s)->last()/1000L);
            int len=(*s)->length()/1000L;
            int h=len/3600;
            int m=(len/60) % 60;
            int s1=len % 60;
            tooltip=day->machine->GetClass()+QString(":#%1").arg((*s)->session(),8,10,QChar('0'));
            // tooltip needs to lookup language.. :-/

            Session *sess=*s;
            if (!sess->settings.contains(SESSION_ENABLED)) {
                sess->settings[SESSION_ENABLED]=true;
            }
            bool b=sess->settings[SESSION_ENABLED].toBool();
            html+=QString("<tr>"
                          "<td width=26><a href='toggle"+type+"session=%1'>"
                          "<img src='qrc:/icons/session-%4.png' width=24px></a></td><td align=center>%5</td><td align=center>%6</td><td align=center>%7</td><td align=left><a class=info href='"+type+"=%1'>%3<span>%2</span></a></td></tr>")
                    .arg((*s)->session())
                    .arg(tooltip)
                    .arg(QString("%1h %2m %3s").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(s1,2,10,QChar('0')))
                    .arg((b ? "on" : "off"))
                    .arg(fd.date().toString(Qt::SystemLocaleShortDate))
                    .arg(fd.toString("HH:mm"))
                    .arg(ld.toString("HH:mm"));
        }
    }

    if (corrupted_waveform) {
        html+=QString("<tr><td colspan=5 align=center><i>%1</i></td></tr>").arg(tr("One or more waveform record for this session had faulty source data. Some waveform overlay points may not match up correctly."));
    }
    html+="</table>";
    return html;
}

QString Daily::getMachineSettings(Day * cpap) {
    QString html;
    if (cpap && cpap->hasEnabledSessions()) {
        html="<table cellpadding=0 cellspacing=0 border=0 width=100%>";
        html+=QString("<tr><td colspan=5 align=center><b>%1</b></td></tr>").arg(tr("Machine Settings"));
        html+="<tr><td colspan=5>&nbsp;</td></tr>";
        if (cpap->settingExists(CPAP_PresReliefType)) {
            int i=cpap->settings_max(CPAP_PresReliefType);
            int j=cpap->settings_max(CPAP_PresReliefSet);
            QString flexstr=(i>1) ? schema::channel[CPAP_PresReliefType].option(i)+" x"+QString::number(j) : STR_TR_None;
            html+=QString("<tr><td><a class='info' href='#'>%1<span>%2</span></a></td><td colspan=4>%3</td></tr>")
                    .arg(STR_TR_PrRelief)
                    .arg(schema::channel[CPAP_PresReliefType].description())
                    .arg(flexstr);
        }
        QString mclass=cpap->machine->GetClass();
        if (mclass==STR_MACH_PRS1 || mclass==STR_MACH_FPIcon) {
            int humid=round(cpap->settings_wavg(CPAP_HumidSetting));
            html+=QString("<tr><td><a class='info' href='#'>"+STR_TR_Humidifier+"<span>%1</span></a></td><td colspan=4>%2</td></tr>")
                .arg(schema::channel[CPAP_HumidSetting].description())
                .arg(humid==0 ? STR_GEN_Off : "x"+QString::number(humid));
        }
        html+="</table>";
        html+="<hr/>\n";
    }
    return html;
}

QString Daily::getOximeterInformation(Day * oxi)
{
    QString html;
    if (oxi && oxi->hasEnabledSessions()) {
        html="<table cellpadding=0 cellspacing=0 border=0 width=100%>";
        html+=QString("<tr><td colspan=5 align=center><b>%1</b></td></tr>\n").arg(tr("Oximeter Information"));
        html+="<tr><td colspan=5 align=center>&nbsp;</td></tr>";
        html+="<tr><td colspan=5 align=center>"+oxi->machine->properties[STR_PROP_Brand]+" "+oxi->machine->properties[STR_PROP_Model]+"</td></tr>\n";
        html+="<tr><td colspan=5 align=center>&nbsp;</td></tr>";
        html+=QString("<tr><td colspan=5 align=center>%1: %2 (%3)\%</td></tr>").arg(tr("SpO2 Desaturations")).arg(oxi->count(OXI_SPO2Drop)).arg((100.0/oxi->hours()) * (oxi->sum(OXI_SPO2Drop)/3600.0),0,'f',2);
        html+=QString("<tr><td colspan=5 align=center>%1: %2 (%3)\%</td></tr>").arg(tr("Pulse Change events")).arg(oxi->count(OXI_PulseChange)).arg((100.0/oxi->hours()) * (oxi->sum(OXI_PulseChange)/3600.0),0,'f',2);
        html+=QString("<tr><td colspan=5 align=center>%1: %2\%</td></tr>").arg(tr("SpO2 Baseline Used")).arg(oxi->settings_wavg(OXI_SPO2Drop),0,'f',2); // CHECKME: Should this value be wavg OXI_SPO2 isntead?
        html+="</table>\n";
        html+="<hr/>\n";
    }
    return html;
}

QString Daily::getCPAPInformation(Day * cpap)
{
    QString html;
    if (!cpap)
        return html;

    QString brand=cpap->machine->properties[STR_PROP_Brand];
    QString series=cpap->machine->properties[STR_PROP_Series];
    QString model=cpap->machine->properties[STR_PROP_Model];
    QString number=cpap->machine->properties[STR_PROP_ModelNumber];

    html="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";

    html+="<tr><td colspan=4 align=center><a class=info2 href='#'>"+model+"<span>";
    QString tooltip=(brand+"\n"+series+" "+number+"\n"+cpap->machine->properties[STR_PROP_Serial]);
    tooltip=tooltip.replace(" ","&nbsp;");


    html+=tooltip;
    html+="</span></td></tr>\n";
    CPAPMode mode=(CPAPMode)(int)cpap->settings_max(CPAP_Mode);
    html+="<tr><td colspan=4 align=center>";


    QString modestr;

    if (mode==MODE_CPAP) modestr=STR_TR_CPAP;
    else if (mode==MODE_APAP) modestr=STR_TR_APAP;
    else if (mode==MODE_BIPAP) modestr=STR_TR_BiLevel;
    else if (mode==MODE_ASV) modestr=STR_TR_ASV;
    else modestr=STR_TR_Unknown;
    html+=tr("PAP Mode: %1<br/>").arg(modestr);

    if (mode==MODE_CPAP) {
        EventDataType min=round(cpap->settings_wavg(CPAP_Pressure)*2)/2.0;
        // eg: Pressure: 13cmH2O
        html+=QString("%1: %2%3").arg(STR_TR_Pressure).arg(min).arg(STR_UNIT_CMH2O);
    } else if (mode==MODE_APAP) {
        EventDataType min=cpap->settings_min(CPAP_PressureMin);
        EventDataType max=cpap->settings_max(CPAP_PressureMax);
        // eg: Pressure: 7.0-10.0cmH2O
        html+=QString("%1: %2-%3%4").arg(STR_TR_Pressure).arg(min,0,'f',1).arg(max,0,'f',1).arg(STR_UNIT_CMH2O);
    } else if (mode>=MODE_BIPAP) {
        if (cpap->settingExists(CPAP_EPAPLo)) {
            html+=QString(STR_TR_EPAPLo+": %1")
                    .arg(cpap->settings_min(CPAP_EPAPLo),0,'f',1);

            if (cpap->settingExists(CPAP_EPAPHi)) {
                    html+=QString("-%2")
                    .arg(cpap->settings_max(CPAP_EPAPHi),0,'f',1);
            }
            html+=STR_UNIT_CMH2O+"</br>";
        } else if (cpap->settingExists(CPAP_EPAP)) {
            EventDataType epap=cpap->settings_min(CPAP_EPAP);

            html+=QString("%1: %2%3<br/>").arg(STR_TR_EPAP)
                    .arg(epap,0,'f',1)
                    .arg(STR_UNIT_CMH2O);

            if (!cpap->settingExists(CPAP_IPAPHi)) {
                if (cpap->settingExists(CPAP_PSMax)) {
                    html+=QString("%1: %2%3<br/>").arg(STR_TR_IPAPHi)
                        .arg(epap+cpap->settings_max(CPAP_PSMax),0,'f',1)
                        .arg(STR_UNIT_CMH2O);

                }
            }
        }
        if (cpap->settingExists(CPAP_IPAPHi)) {
            html+=QString(STR_TR_IPAPHi+": %1"+STR_UNIT_CMH2O+"<br/>")
                    .arg(cpap->settings_max(CPAP_IPAPHi),0,'f',1);
        } else
        if (cpap->settingExists(CPAP_IPAP)) {
            html+=QString(STR_TR_IPAP+": %1"+STR_UNIT_CMH2O+"<br/>")
                    .arg(cpap->settings_max(CPAP_IPAP),0,'f',1);
        }

        if (cpap->settingExists(CPAP_PS)) {
            html+=QString(STR_TR_PS+": %1"+STR_UNIT_CMH2O+"<br/>")
                .arg(cpap->settings_max(CPAP_PS),0,'f',1);
        } else if (cpap->settingExists(CPAP_PSMin)) {
            EventDataType psl=cpap->settings_min(CPAP_PSMin);
            EventDataType psh=cpap->settings_max(CPAP_PSMax);
            html+=QString(STR_TR_PS+": %1-%2"+STR_UNIT_CMH2O+"<br/>")
                .arg(psl,0,'f',1)
                .arg(psh,0,'f',1);
        }
    }
    html+="</td></tr>\n";
    html+="</table>\n";
    html+="<hr/>\n";
    return html;
}


QString Daily::getStatisticsInfo(Day * cpap,Day * oxi)
{

    QList<Day *> list;

    list.push_back(cpap);
    list.push_back(oxi);


    int mididx=PROFILE.general->prefCalcMiddle();
    SummaryType ST_mid;
    if (mididx==0) ST_mid=ST_PERC;
    if (mididx==1) ST_mid=ST_WAVG;
    if (mididx==2) ST_mid=ST_AVG;

    float percentile=PROFILE.general->prefCalcPercentile()/100.0;

    SummaryType ST_max=PROFILE.general->prefCalcMax() ? ST_MAX : ST_PERC;
    const EventDataType maxperc=0.995F;

    QString midname;
    if (ST_mid==ST_WAVG) midname=tr("Avg");
    else if (ST_mid==ST_AVG) midname=tr("Avg");
    else if (ST_mid==ST_PERC) midname=tr("Med");

    QString html;

    html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
    html+=QString("<tr><td colspan=5 align=center><b>%1</b></td></tr>\n").arg(tr("Statistics"));
    html+=QString("<tr><td><b>%1</b></td><td><b>%2</b></td><td><b>%3</b></td><td><b>%4</b></td><td><b>%5</b></td></tr>")
            .arg(STR_TR_Channel)
            .arg(STR_TR_Min)
            .arg(midname)
            .arg(tr("%1%").arg(percentile*100,0,'f',0))
            .arg(STR_TR_Max);

    ChannelID chans[]={
        CPAP_Pressure,CPAP_EPAP,CPAP_IPAP,CPAP_PS,CPAP_PTB,
        CPAP_MinuteVent, CPAP_RespRate, CPAP_RespEvent,CPAP_FLG,
        CPAP_Leak, CPAP_LeakTotal, CPAP_Snore,CPAP_IE,CPAP_Ti,CPAP_Te, CPAP_TgMV,
        CPAP_TidalVolume, OXI_Pulse, OXI_SPO2
    };
    int numchans=sizeof(chans)/sizeof(ChannelID);
    int ccnt=0;
    EventDataType tmp,med,perc,mx,mn;

    QList<Day *>::iterator di;

    for (di=list.begin();di!=list.end();di++) {
        Day * day=*di;

        if (!day)
            continue;

        for (int i=0;i<numchans;i++) {

            ChannelID code=chans[i];

            if (!day->channelHasData(code))
                continue;

            QString tooltip=schema::channel[code].description();

            if (!schema::channel[code].units().isEmpty()) tooltip+=" ("+schema::channel[code].units()+")";

            if (ST_max==ST_MAX) {
                mx=day->Max(code);
            } else {
                mx=day->percentile(code,maxperc);
            }

            mn=day->Min(code);
            perc=day->percentile(code,percentile);

            if (ST_mid==ST_PERC) {
                med=day->percentile(code,0.5);
                tmp=day->wavg(code);
                if (tmp>0 || mx==0) {
                    tooltip+=QString("<br/>"+STR_TR_WAvg+": %1").arg(tmp,0,'f',2);
                }
            } else if (ST_mid==ST_WAVG) {
                med=day->wavg(code);
                tmp=day->percentile(code,0.5);
                if (tmp>0 || mx==0) {
                    tooltip+=QString("<br/>"+STR_TR_Median+": %1").arg(tmp,0,'f',2);
                }
            } else if (ST_mid==ST_AVG) {
                med=day->avg(code);
                tmp=day->percentile(code,0.5);
                if (tmp>0 || mx==0) {
                    tooltip+=QString("<br/>"+STR_TR_Median+": %1").arg(tmp,0,'f',2);
                }
            }

            html+=QString("<tr><td align=left class='info' onmouseover=\"style.color='blue';\" onmouseout=\"style.color='"+COLOR_Text.name()+"';\">%1<span>%6</span></td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                .arg(schema::channel[code].label())
                .arg(mn,0,'f',2)
                .arg(med,0,'f',2)
                .arg(perc,0,'f',2)
                .arg(mx,0,'f',2)
                .arg(tooltip);
            ccnt++;
        }
    }
    if (GraphView->isEmpty() && (ccnt>0)) {
        html+="<tr><td colspan=5>&nbsp;</td></tr>\n";
        html+=QString("<tr><td colspan=5 align=center><i>%1</i></td></tr>").arg(tr("<b>Please Note:</b> This day just contains summary data, only limited information is available ."));
    }
    html+="</table>\n";
    html+="<hr/>\n";
    return html;
}

QString Daily::getEventBreakdown(Day * cpap)
{
    Q_UNUSED(cpap)
    QString html;
    html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";

    html+="</table>";
    return html;
}

QString Daily::getSleepTime(Day * cpap, Day * oxi)
{
    QString html;

    Day * day=NULL;
    if (cpap && cpap->hours()>0)
        day=cpap;
    else if (oxi && oxi->hours()>0)
        day=oxi;
    else
        return html;
    html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
    html+="<tr><td align='center'><b>"+STR_TR_Date+"</b></td><td align='center'><b>"+tr("Sleep")+"</b></td><td align='center'><b>"+tr("Wake")+"</b></td><td align='center'><b>"+STR_UNIT_Hours+"</b></td></tr>";
    int tt=qint64(day->total_time())/1000L;
    QDateTime date=QDateTime::fromTime_t(day->first()/1000L);
    QDateTime date2=QDateTime::fromTime_t(day->last()/1000L);

    int h=tt/3600;
    int m=(tt/60)%60;
    int s=tt % 60;
    html+=QString("<tr><td align='center'>%1</td><td align='center'>%2</td><td align='center'>%3</td><td align='center'>%4</td></tr>\n")
            .arg(date.date().toString(Qt::SystemLocaleShortDate))
            .arg(date.toString("HH:mm"))
            .arg(date2.toString("HH:mm"))
            .arg(QString().sprintf("%02i:%02i:%02i",h,m,s));
    html+="</table>\n";
//    html+="<hr/>";

    return html;
}


void Daily::Load(QDate date)
{
    dateDisplay->setText("<i>"+date.toString(Qt::SystemLocaleLongDate)+"</i>");
    previous_date=date;
    Day *cpap=PROFILE.GetDay(date,MT_CPAP);
    Day *oxi=PROFILE.GetDay(date,MT_OXIMETER);
    Day *stage=PROFILE.GetDay(date,MT_SLEEPSTAGE);

    if (!PROFILE.session->cacheSessions()) {
        // Getting trashed on purge last day...
        if (lastcpapday && (lastcpapday!=cpap)) {
            for (QList<Session *>::iterator s=lastcpapday->begin();s!=lastcpapday->end();++s) {
                (*s)->TrashEvents();
            }
        }
    }

    if ((cpap && oxi) && oxi->hasEnabledSessions()) {
        int gr;

        if (qAbs(cpap->first() - oxi->first())>30000) {
            mainwin->Notify(tr("Oximetry data exists for this day, but its timestamps are too different, so the Graphs will not be linked."),"",3000);
            gr=1;
        } else
            gr=0;

        GraphView->findGraph(STR_TR_PulseRate)->setGroup(gr);
        GraphView->findGraph(STR_TR_SpO2)->setGroup(gr);
        GraphView->findGraph(STR_TR_Plethy)->setGroup(gr);
    }
    lastcpapday=cpap;

    QString html="<html><head><style type='text/css'>"
    "p,a,td,body { font-family: '"+QApplication::font().family()+"'; }"
    "p,a,td,body { font-size: "+QString::number(QApplication::font().pointSize() + 2)+"px; }"
    "</style>"
    "<link rel='stylesheet' type='text/css' href='qrc:/docs/tooltips.css' />"
    "<script language='javascript'><!--"
            "func dosession(sessid) {"
            ""
            "}"
    "--></script>"
    "</head>"
    "<body leftmargin=0 rightmargin=0 topmargin=0 marginwidth=0 marginheight=0>";
    QString tmp;

    UpdateOXIGraphs(oxi);
    UpdateCPAPGraphs(cpap);
    UpdateSTAGEGraphs(stage);
    UpdateEventsTree(ui->treeWidget,cpap);

    mainwin->refreshStatistics();

    snapGV->setDay(cpap);

    GraphView->ResetBounds(false);

    if (!cpap && !oxi) {
        scrollbar->hide();
    } else {
        scrollbar->show();
    }

    QString modestr;
    CPAPMode mode=MODE_UNKNOWN;
    QString a;
    bool isBrick=false;

    updateGraphCombo();


    if (cpap) {
        float hours=cpap->hours();
        if (GraphView->isEmpty() && (hours>0)) {
            if (!PROFILE.hasChannel(CPAP_Obstructive) && !PROFILE.hasChannel(CPAP_Hypopnea)) {
                GraphView->setCubeImage(images["brick"]);
                GraphView->setEmptyText(tr("No Graphs :("));

                isBrick=true;
            }
        }

        mode=(CPAPMode)(int)cpap->settings_max(CPAP_Mode);

        modestr=schema::channel[CPAP_Mode].m_options[mode];

        EventDataType ahi=(cpap->count(CPAP_Obstructive)+cpap->count(CPAP_Hypopnea)+cpap->count(CPAP_ClearAirway)+cpap->count(CPAP_Apnea));
        if (PROFILE.general->calculateRDI()) ahi+=cpap->count(CPAP_RERA);
        ahi/=hours;
        EventDataType csr,uai,oai,hi,cai,rei,fli,nri,lki,vs,vs2,exp,lk2;

        if (!isBrick && hours>0) {
            html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
            ChannelID ahichan=CPAP_AHI;
            QString ahiname=STR_TR_AHI;
            if (PROFILE.general->calculateRDI()) {
                ahichan=CPAP_RDI;
                ahiname=STR_TR_RDI;
            }
            html+="<tr>";
            html+=QString("<td colspan=4 bgcolor='%1' align=center><a class=info2 href='#'><font size=+4 color='%2'><b>%3</b></font><span>%4</span></a> &nbsp; <font size=+4 color='%2'><b>%5</b></font></td>\n")
                        .arg("#F88017").arg(COLOR_Text.name()).arg(ahiname).arg(schema::channel[ahichan].description()).arg(ahi,0,'f',2);
            html+="</tr>\n";
            html+="</table>\n";
            html+=getCPAPInformation(cpap);
            html+=getSleepTime(cpap,oxi);

            struct ChannelInfo {
                ChannelID id;
                QColor color;
                QColor color2;
                EventDataType value;
            };
            ChannelInfo chans[]={
                { CPAP_Hypopnea,    COLOR_Hypopnea,     Qt::white, hi=(cpap->count(CPAP_ExP)+cpap->count(CPAP_Hypopnea))/hours },
                { CPAP_Obstructive, COLOR_Obstructive,  Qt::black, oai=cpap->count(CPAP_Obstructive)/hours },
                { CPAP_Apnea,       COLOR_Apnea,        Qt::black, uai=cpap->count(CPAP_Apnea)/hours },
                { CPAP_ClearAirway, COLOR_ClearAirway,  Qt::black, cai=cpap->count(CPAP_ClearAirway)/hours },
                { CPAP_NRI,         COLOR_NRI,          Qt::black, nri=cpap->count(CPAP_NRI)/hours },
                { CPAP_FlowLimit,   COLOR_FlowLimit,    Qt::white, fli=cpap->count(CPAP_FlowLimit)/hours },
                { CPAP_ExP,         COLOR_ExP,          Qt::black, exp=cpap->count(CPAP_ExP)/hours },
                { CPAP_RERA,        COLOR_RERA,         Qt::black, rei=cpap->count(CPAP_RERA)/hours },
                { CPAP_VSnore,      COLOR_VibratorySnore, Qt::black, vs=cpap->count(CPAP_VSnore)/cpap->hours() },
                { CPAP_VSnore2,     COLOR_VibratorySnore, Qt::black, vs2=cpap->count(CPAP_VSnore2)/cpap->hours() },
                { CPAP_LeakFlag,    COLOR_LeakFlag,     Qt::black, lki=cpap->count(CPAP_LeakFlag)/hours },
                { PRS1_10,          COLOR_LeakFlag,     Qt::black, lk2=cpap->count(PRS1_10)/hours },
                { CPAP_CSR,         COLOR_CSR,      Qt::black, csr=(100.0/cpap->hours())*(cpap->sum(CPAP_CSR)/3600.0) }
            };
            int numchans=sizeof(chans)/sizeof(ChannelInfo);

            html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
            for (int i=0;i<numchans;i++) {
                if (!cpap->channelHasData(chans[i].id))
                    continue;
                if ((cpap->machine->GetClass()==STR_MACH_PRS1) && (chans[i].id==CPAP_VSnore))
                    continue;
                html+=QString("<tr><td align='left' bgcolor='%1'><b><font color='%2'><a href='event=%5'>%3</a></font></b></td><td width=20% bgcolor='%1'><b><font color='%2'>%4</font></b></td></tr>\n")
                        .arg(chans[i].color.name()).arg(chans[i].color2.name()).arg(schema::channel[chans[i].id].fullname()).arg(chans[i].value,0,'f',2).arg(chans[i].id);

                // keep in case tooltips are needed
                //html+=QString("<tr><td align='left' bgcolor='%1'><b><font color='%2'><a class=info href='event=%6'>%3<span>%4</span></a></font></b></td><td width=20% bgcolor='%1'><b><font color='%2'>%5</font></b></td></tr>\n")
                // .arg(chans[i].color.name()).arg(chans[i].color2.name()).arg(chans[i].name).arg(schema::channel[chans[i].id].description()).arg(chans[i].value,0,'f',2).arg(chans[i].id);
            }
            html+="</table>";

            html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
            // Show Event Breakdown pie chart
            if ((hours > 0) && PROFILE.appearance->graphSnapshots()) {  // AHI Pie Chart
                if ((oai+hi+cai+uai+rei+fli)>0) {
                    html+="<tr><td align=center>&nbsp;</td></tr>";
                    html+=QString("<tr><td align=center><b>%1</b></td></tr>").arg(tr("Event Breakdown"));
                    GAHI->setShowTitle(false);

                    int w=155;
                    int h=155;
                    QPixmap pixmap=GAHI->renderPixmap(w,h,false);
                    if (!pixmap.isNull()) {
                        QByteArray byteArray;
                        QBuffer buffer(&byteArray); // use buffer to store pixmap into byteArray
                        buffer.open(QIODevice::WriteOnly);
                        pixmap.save(&buffer, "PNG");
                        html += "<tr><td align=center><img src=\"data:image/png;base64," + byteArray.toBase64() + QString("\" width='%1' height='%2px'></td></tr>\n").arg(w).arg(h);
                    } else {
                        html += "<tr><td align=center>Unable to display Pie Chart on this system</td></tr>\n";
                    }
                } else {
                    html += "<tr><td align=center><img src=\"qrc:/docs/0.0.gif\"></td></tr>\n";
                }
            }

            html+="</table>\n";

        } else {
            html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
            if (!isBrick) {
                html+="<tr><td colspan='5'>&nbsp;</td></tr>\n";
                if (cpap->size()>0) {
                    html+="<tr><td colspan='5' align='center'><b><h2>"+tr("Sessions all off!")+"</h2></b></td></tr>";
                    html+="<tr><td colspan='5' align='center'><i>"+tr("Sessions exist for this day but are switched off.")+"</i></td></tr>\n";
                } else {
                    html+="<tr><td colspan='5' align='center'><b><h2>"+tr("Impossibly short session")+"</h2></b></td></tr>";
                    html+="<tr><td colspan='5' align='center'><i>"+tr("Zero hours??")+"</i></td></tr>\n";
                }
            } else { // machine is a brick
                html+="<tr><td colspan='5' align='center'><b><h2>"+tr("BRICK :(")+"</h2></b></td></tr>";
                html+="<tr><td colspan='5' align='center'><i>"+tr("Sorry, your machine only provides compliance data.")+"</i></td></tr>\n";
                html+="<tr><td colspan='5' align='center'><i>"+tr("Complain to your Equipment Provider!")+"</i></td></tr>\n";
            }
            html+="<tr><td colspan='5'>&nbsp;</td></tr>\n";
            html+="</table>\n";
        }
        html+="<hr/>\n";

    } // if (!CPAP)
    else html+=getSleepTime(cpap,oxi);

    if ((cpap && !isBrick && (cpap->hours()>0)) || oxi) {

        html+=getStatisticsInfo(cpap,oxi);
    } else {
        if (cpap && cpap->hours()==0) {
        } else {
            html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
            html+="<tr><td colspan=5 align=center><i>"+tr("No data available")+"</i></td></tr>\n";
            html+="<tr><td colspan=5>&nbsp;</td></tr>\n";
            html+="</table>\n";
            html+="<hr/>\n";
        }

    }

    html+=getOximeterInformation(oxi);
    html+=getMachineSettings(cpap);
    html+=getSessionInformation(cpap,oxi,stage);

    html+="</body></html>";

    QColor cols[]={
        QColor("gold"),
        QColor("light blue"),
    };
    const int maxcolors=sizeof(cols)/sizeof(QColor);
    QList<Session *>::iterator i;

    // WebView trashes it without asking.. :(
    if (cpap) {
        sessbar=new SessionBar(this);
        sessbar->setMouseTracking(true);
        connect(sessbar, SIGNAL(toggledSession(Session*)), this, SLOT(doToggleSession(Session*)));
        int c=0;

        for (i=cpap->begin();i!=cpap->end();++i) {
            Session * s=*i;
            sessbar->add(s, cols[c % maxcolors]);
            c++;
        }
    } else sessbar=NULL;
    //sessbar->update();

    webView->setHtml(html);

    ui->JournalNotes->clear();

    ui->bookmarkTable->clearContents();
    ui->bookmarkTable->setRowCount(0);
    QStringList sl;
    //sl.append(tr("Starts"));
    //sl.append(tr("Notes"));
    ui->bookmarkTable->setHorizontalHeaderLabels(sl);
    ui->ZombieMeter->blockSignals(true);
    ui->weightSpinBox->blockSignals(true);
    ui->ouncesSpinBox->blockSignals(true);

    ui->weightSpinBox->setValue(0);
    ui->ouncesSpinBox->setValue(0);
    ui->ZombieMeter->setValue(5);
    ui->ouncesSpinBox->blockSignals(false);
    ui->weightSpinBox->blockSignals(false);
    ui->ZombieMeter->blockSignals(false);
    ui->BMI->display(0);
    ui->BMI->setVisible(false);
    ui->BMIlabel->setVisible(false);

    BookmarksChanged=false;
    Session *journal=GetJournalSession(date);
    if (journal) {
        bool ok;
        if (journal->settings.contains(Journal_Notes))
            ui->JournalNotes->setHtml(journal->settings[Journal_Notes].toString());

        if (journal->settings.contains(Journal_Weight)) {
            double kg=journal->settings[Journal_Weight].toDouble(&ok);

            if (PROFILE.general->unitSystem()==US_Metric) {
                ui->weightSpinBox->setDecimals(3);
                ui->weightSpinBox->blockSignals(true);
                ui->weightSpinBox->setValue(kg);
                ui->weightSpinBox->blockSignals(false);
                ui->ouncesSpinBox->setVisible(false);
                ui->weightSpinBox->setSuffix(STR_UNIT_KG);
            } else {
                float ounces=(kg*1000.0)/ounce_convert;
                int pounds=ounces/16.0;
                double oz;
                double frac=modf(ounces,&oz);
                ounces=(int(ounces) % 16)+frac;
                ui->weightSpinBox->blockSignals(true);
                ui->ouncesSpinBox->blockSignals(true);
                ui->weightSpinBox->setValue(pounds);
                ui->ouncesSpinBox->setValue(ounces);
                ui->ouncesSpinBox->blockSignals(false);
                ui->weightSpinBox->blockSignals(false);

                ui->weightSpinBox->setSuffix(STR_UNIT_POUND);
                ui->weightSpinBox->setDecimals(0);
                ui->ouncesSpinBox->setVisible(true);
                ui->ouncesSpinBox->setSuffix(STR_UNIT_OUNCE);
            }
            double height=PROFILE.user->height()/100.0;
            if (height>0 && kg>0) {
                double bmi=kg/(height*height);
                ui->BMI->setVisible(true);
                ui->BMIlabel->setVisible(true);
                ui->BMI->display(bmi);
            }
        }

        if (journal->settings.contains(Journal_ZombieMeter)) {
            ui->ZombieMeter->blockSignals(true);
            ui->ZombieMeter->setValue(journal->settings[Journal_ZombieMeter].toDouble(&ok));
            ui->ZombieMeter->blockSignals(false);
        }

        if (journal->settings.contains(Bookmark_Start)) {
            QVariantList start=journal->settings[Bookmark_Start].toList();
            QVariantList end=journal->settings[Bookmark_End].toList();
            QStringList notes=journal->settings[Bookmark_Notes].toStringList();

            ui->bookmarkTable->blockSignals(true);


            qint64 clockdrift=PROFILE.cpap->clockDrift()*1000L,drift;
            Day * dday=PROFILE.GetDay(previous_date,MT_CPAP);
            drift=(dday!=NULL) ? clockdrift : 0;

            bool ok;
            for (int i=0;i<start.size();i++) {
                qint64 st=start.at(i).toLongLong(&ok)+drift;
                qint64 et=end.at(i).toLongLong(&ok)+drift;

                QDateTime d=QDateTime::fromTime_t(st/1000L);
                //int row=ui->bookmarkTable->rowCount();
                ui->bookmarkTable->insertRow(i);
                QTableWidgetItem *tw=new QTableWidgetItem(notes.at(i));
                QTableWidgetItem *dw=new QTableWidgetItem(d.time().toString("HH:mm:ss"));
                dw->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
                ui->bookmarkTable->setItem(i,0,dw);
                ui->bookmarkTable->setItem(i,1,tw);
                tw->setData(Qt::UserRole,st);
                tw->setData(Qt::UserRole+1,et);
            } // for (int i
            ui->bookmarkTable->blockSignals(false);
        } // if (journal->settings.contains(Bookmark_Start))
    } // if (journal)
}

void Daily::UnitsChanged()
{
    double kg;
    if (PROFILE.general->unitSystem()==US_Archiac) {
        kg=ui->weightSpinBox->value();
        float ounces=(kg*1000.0)/ounce_convert;
        int pounds=ounces/16;
        float oz=fmodf(ounces,16);
        ui->weightSpinBox->setValue(pounds);
        ui->ouncesSpinBox->setValue(oz);

        ui->weightSpinBox->setDecimals(0);
        ui->weightSpinBox->setSuffix(STR_UNIT_POUND);
        ui->ouncesSpinBox->setVisible(true);
        ui->ouncesSpinBox->setSuffix(STR_UNIT_OUNCE);
    } else {
        kg=(ui->weightSpinBox->value()*(ounce_convert*16.0))+(ui->ouncesSpinBox->value()*ounce_convert);
        kg/=1000.0;
        ui->weightSpinBox->setDecimals(3);
        ui->weightSpinBox->setValue(kg);
        ui->ouncesSpinBox->setVisible(false);
        ui->weightSpinBox->setSuffix(STR_UNIT_KG);
    }
}

void Daily::clearLastDay()
{
    lastcpapday=NULL;
}


void Daily::Unload(QDate date)
{
    Session *journal=GetJournalSession(date);

    bool nonotes=ui->JournalNotes->toPlainText().isEmpty();
    if (journal) {
        QString jhtml=ui->JournalNotes->toHtml();
        if ((!journal->settings.contains(Journal_Notes) && !nonotes) || (journal->settings[Journal_Notes]!=jhtml)) {
            journal->settings[Journal_Notes]=jhtml;
            journal->SetChanged(true);
        }
    } else {
        if (!nonotes) {
            journal=CreateJournalSession(date);
            if (!nonotes) {
                journal->settings[Journal_Notes]=ui->JournalNotes->toHtml();
                journal->SetChanged(true);
            }
        }
    }

    if (journal) {
        if (nonotes) {
            QHash<ChannelID,QVariant>::iterator it=journal->settings.find(Journal_Notes);
            if (it!=journal->settings.end()) {
                journal->settings.erase(it);
            }
        }
        if (journal->IsChanged()) {
            // blah.. was updating overview graphs here.. Was too slow.
        }
        Machine *jm=PROFILE.GetMachine(MT_JOURNAL);
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
    QColor col=QColorDialog::getColor(COLOR_Black,this,tr("Pick a Colour")); //,QColorDialog::NoButtons);
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
    Machine *m=PROFILE.GetMachine(MT_JOURNAL);
    if (!m) {
        m=new Machine(p_profile,0);
        m->SetClass("Journal");
        m->properties[STR_PROP_Brand]="Virtual";
        m->SetType(MT_JOURNAL);
        PROFILE.AddMachine(m);
    }
    Session *sess=new Session(m,0);
    qint64 st,et;
    Day *cday=PROFILE.GetDay(date,MT_CPAP);
    if (cday) {
        st=cday->first();
        et=cday->last();
    } else {
        QDateTime dt(date,QTime(20,0));
        st=qint64(dt.toTime_t())*1000L;
        et=st+3600000;
    }
    sess->set_first(st);
    sess->set_last(et);
    sess->SetChanged(true);
    m->AddSession(sess,p_profile);
    return sess;
}
Session * Daily::GetJournalSession(QDate date) // Get the first journal session
{
    Day *journal=PROFILE.GetDay(date,MT_JOURNAL);
    if (!journal)
        return NULL; //CreateJournalSession(date);
    QList<Session *>::iterator s;
    s=journal->begin();
    if (s!=journal->end())
        return *s;
    return NULL;
}

void Daily::UpdateCPAPGraphs(Day *day)
{
    //if (!day) return;
    if (day) {
        day->OpenEvents();
    }
    for (QList<Layer *>::iterator g=CPAPData.begin();g!=CPAPData.end();g++) {
        (*g)->SetDay(day);
    }
}
void Daily::UpdateSTAGEGraphs(Day *day)
{
    //if (!day) return;
    if (day) {
        day->OpenEvents();
    }
    for (QList<Layer *>::iterator g=STAGEData.begin();g!=STAGEData.end();g++) {
        (*g)->SetDay(day);
    }
}

void Daily::UpdateOXIGraphs(Day *day)
{
    //if (!day) return;

    if (day) {
        day->OpenEvents();
    }
    for (QList<Layer *>::iterator g=OXIData.begin();g!=OXIData.end();g++) {
        (*g)->SetDay(day);
    }
}

void Daily::RedrawGraphs()
{
    GraphView->redraw();
}

void Daily::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    QDateTime d;
    if (!item->data(0,Qt::UserRole).isNull()) {
        qint64 winsize=qint64(PROFILE.general->eventWindowSize())*60000L;
        qint64 t=item->data(0,Qt::UserRole).toLongLong();

        double st=t-(winsize/2);
        double et=t+(winsize/2);


        gGraph *g=GraphView->findGraph(STR_TR_EventFlags);
        if (!g) return;
        if (st<g->rmin_x) {
            st=g->rmin_x;
            et=st+winsize;
        }
        if (et>g->rmax_x) {
            et=g->rmax_x;
            st=et-winsize;
        }
        GraphView->SetXBounds(st,et);
    }
}

void Daily::on_treeWidget_itemSelectionChanged()
{
    if (ui->treeWidget->selectedItems().size()==0) return;
    QTreeWidgetItem *item=ui->treeWidget->selectedItems().at(0);
    if (!item) return;
    on_treeWidget_itemClicked(item, 0);
}

void Daily::on_JournalNotesUnderline_clicked()
{
    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QTextCharFormat format=cursor.charFormat();

    format.setFontUnderline(!format.fontUnderline());

    cursor.mergeCharFormat(format);
   //ui->JournalNotes->mergeCurrentCharFormat(format);
}

void Daily::on_prevDayButton_clicked()
{
    if (!PROFILE.ExistsAndTrue("SkipEmptyDays")) {
        LoadDate(previous_date.addDays(-1));
    } else {
        QDate d=previous_date;
        for (int i=0;i<90;i++) {
            d=d.addDays(-1);
            if (PROFILE.GetDay(d)) {
                LoadDate(d);
                break;
            }
        }
    }
}

void Daily::on_nextDayButton_clicked()
{
    if (!PROFILE.ExistsAndTrue("SkipEmptyDays")) {
        LoadDate(previous_date.addDays(1));
    } else {
        QDate d=previous_date;
        for (int i=0;i<90;i++) {
            d=d.addDays(1);
            if (PROFILE.GetDay(d)) {
                LoadDate(d);
                break;
            }
        }
    }
}

void Daily::on_calButton_toggled(bool checked)
{
    //bool b=!ui->calendar->isVisible();
    bool b=checked;
    ui->calendar->setVisible(b);
    if (!b) ui->calButton->setArrowType(Qt::DownArrow);
    else ui->calButton->setArrowType(Qt::UpArrow);
}


void Daily::on_todayButton_clicked()
{
    QDate d=QDate::currentDate();
    if (d > PROFILE.LastDay()) d=PROFILE.LastDay();
    LoadDate(d);
}

void Daily::on_evViewSlider_valueChanged(int value)
{
    ui->evViewLCD->display(value);
    PROFILE.general->setEventWindowSize(value);

    int winsize=value*60;

    gGraph *g=GraphView->findGraph(STR_TR_EventFlags);
    if (!g) return;
    qint64 st=g->min_x;
    qint64 et=g->max_x;
    qint64 len=et-st;
    qint64 d=st+len/2.0;

    st=d-(winsize/2)*1000;
    et=d+(winsize/2)*1000;
    if (st<g->rmin_x) {
        st=g->rmin_x;
        et=st+winsize*1000;
    }
    if (et>g->rmax_x) {
        et=g->rmax_x;
        st=et-winsize*1000;
    }
    GraphView->SetXBounds(st,et);
}

void Daily::on_bookmarkTable_itemClicked(QTableWidgetItem *item)
{
    int row=item->row();
    qint64 st,et;

//    qint64 clockdrift=PROFILE.cpap->clockDrift()*1000L,drift;
//    Day * dday=PROFILE.GetDay(previous_date,MT_CPAP);
//    drift=(dday!=NULL) ? clockdrift : 0;

    QTableWidgetItem *it=ui->bookmarkTable->item(row,1);
    bool ok;
    st=it->data(Qt::UserRole).toLongLong(&ok);
    et=it->data(Qt::UserRole+1).toLongLong(&ok);
    qint64 st2=0,et2=0,st3,et3;
    Day * day=PROFILE.GetGoodDay(previous_date,MT_CPAP);
    if (day) {
        st2=day->first();
        et2=day->last();
    }
    Day * oxi=PROFILE.GetGoodDay(previous_date,MT_OXIMETER);
    if (oxi) {
        st3=oxi->first();
        et3=oxi->last();
    }
    if (oxi && day) {
        st2=qMin(st2,st3);
        et2=qMax(et2,et3);
    } else if (oxi) {
        st2=st3;
        et2=et3;
    } else if (!day) return;
    if ((et<st2) || (st>et2)) {
        mainwin->Notify(tr("This bookmarked is in a currently disabled area.."));
        return;
    }

    if (st<st2) st=st2;
    if (et>et2) et=et2;
    GraphView->SetXBounds(st,et);
    GraphView->redraw();
}

void Daily::on_addBookmarkButton_clicked()
{
    qint64 st,et;
    ui->bookmarkTable->blockSignals(true);
    GraphView->GetXBounds(st,et);
    QDateTime d=QDateTime::fromTime_t(st/1000L);
    int row=ui->bookmarkTable->rowCount();
    ui->bookmarkTable->insertRow(row);
    QTableWidgetItem *tw=new QTableWidgetItem(tr("Bookmark at %1").arg(d.time().toString("HH:mm:ss")));
    QTableWidgetItem *dw=new QTableWidgetItem(d.time().toString("HH:mm:ss"));
    dw->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
    ui->bookmarkTable->setItem(row,0,dw);
    ui->bookmarkTable->setItem(row,1,tw);
    qint64 clockdrift=PROFILE.cpap->clockDrift()*1000L,drift;
    Day * day=PROFILE.GetDay(previous_date,MT_CPAP);
    drift=(day!=NULL) ? clockdrift : 0;

    // Counter CPAP clock drift for storage, in case user changes it later on
    // This won't fix the text string names..

    tw->setData(Qt::UserRole,st-drift);
    tw->setData(Qt::UserRole+1,et-drift);

    ui->bookmarkTable->blockSignals(false);
    update_Bookmarks();
    mainwin->updateFavourites();

    //ui->bookmarkTable->setItem(row,2,new QTableWidgetItem(QString::number(st)));
    //ui->bookmarkTable->setItem(row,3,new QTableWidgetItem(QString::number(et)));
}
void Daily::update_Bookmarks()
{
    QVariantList start;
    QVariantList end;
    QStringList notes;
    QTableWidgetItem *item;
    for (int row=0;row<ui->bookmarkTable->rowCount();row++) {
        item=ui->bookmarkTable->item(row,1);

        start.push_back(item->data(Qt::UserRole));
        end.push_back(item->data(Qt::UserRole+1));
        notes.push_back(item->text());
    }
    Session *journal=GetJournalSession(previous_date);
    if (!journal) {
        journal=CreateJournalSession(previous_date);
    }

    journal->settings[Bookmark_Start]=start;
    journal->settings[Bookmark_End]=end;
    journal->settings[Bookmark_Notes]=notes;
    journal->SetChanged(true);
    BookmarksChanged=true;
    mainwin->updateFavourites();
}

void Daily::on_removeBookmarkButton_clicked()
{
    int row=ui->bookmarkTable->currentRow();
    if (row>=0) {
        ui->bookmarkTable->blockSignals(true);
        ui->bookmarkTable->removeRow(row);
        ui->bookmarkTable->blockSignals(false);
        update_Bookmarks();
    }
    mainwin->updateFavourites();
}
void Daily::on_ZombieMeter_valueChanged(int action)
{
    Q_UNUSED(action);
    ZombieMeterMoved=true;
    Session *journal=GetJournalSession(previous_date);
    if (!journal) {
        journal=CreateJournalSession(previous_date);
    }
    journal->settings[Journal_ZombieMeter]=ui->ZombieMeter->value();
    journal->SetChanged(true);

    if (mainwin->getOverview()) mainwin->getOverview()->ResetGraph("Zombie");
}

void Daily::on_bookmarkTable_itemChanged(QTableWidgetItem *item)
{
    Q_UNUSED(item);
    update_Bookmarks();
}

void Daily::on_weightSpinBox_valueChanged(double arg1)
{
    // Update the BMI display
    double kg;
    if (PROFILE.general->unitSystem()==US_Archiac) {
         kg=((arg1*pound_convert) + (ui->ouncesSpinBox->value()*ounce_convert)) / 1000.0;
    } else kg=arg1;
    double height=PROFILE.user->height()/100.0;
    if ((height>0) && (kg>0)) {
        double bmi=kg/(height * height);
        ui->BMI->display(bmi);
        ui->BMI->setVisible(true);
    }
}

void Daily::on_weightSpinBox_editingFinished()
{
    double arg1=ui->weightSpinBox->value();

    double height=PROFILE.user->height()/100.0;
    Session *journal=GetJournalSession(previous_date);
    if (!journal) {
        journal=CreateJournalSession(previous_date);
    }

    double kg;
    if (PROFILE.general->unitSystem()==US_Archiac) {
            kg=((arg1*pound_convert) + (ui->ouncesSpinBox->value()*ounce_convert)) / 1000.0;
    } else {
            kg=arg1;
    }
    journal->settings[Journal_Weight]=kg;
    gGraphView *gv=mainwin->getOverview()->graphView();
    gGraph *g;
    if (gv) {
        g=gv->findGraph(STR_TR_Weight);
        if (g) g->setDay(NULL);
    }
    if ((height>0) && (kg>0)) {
        double bmi=kg/(height * height);
        ui->BMI->display(bmi);
        ui->BMI->setVisible(true);
        journal->settings[Journal_BMI]=bmi;
        if (gv) {
            g=gv->findGraph(STR_TR_BMI);
            if (g) g->setDay(NULL);
        }
    }
    journal->SetChanged(true);
}

void Daily::on_ouncesSpinBox_valueChanged(int arg1)
{
    // just update for BMI display
    double height=PROFILE.user->height()/100.0;
    double kg=((ui->weightSpinBox->value()*pound_convert) + (arg1*ounce_convert)) / 1000.0;
    if ((height>0) && (kg>0)) {
        double bmi=kg/(height * height);
        ui->BMI->display(bmi);
        ui->BMI->setVisible(true);
    }
}

void Daily::on_ouncesSpinBox_editingFinished()
{
    double arg1=ui->ouncesSpinBox->value();
    Session *journal=GetJournalSession(previous_date);
    if (!journal) {
        journal=CreateJournalSession(previous_date);
    }
    double height=PROFILE.user->height()/100.0;
    double kg=((ui->weightSpinBox->value()*pound_convert) + (arg1*ounce_convert)) / 1000.0;
    journal->settings[Journal_Weight]=kg;

    gGraph *g;
    if (mainwin->getOverview()) {
        g=mainwin->getOverview()->graphView()->findGraph(STR_TR_Weight);
        if (g) g->setDay(NULL);
    }

    if ((height>0) && (kg>0)) {
        double bmi=kg/(height * height);
        ui->BMI->display(bmi);
        ui->BMI->setVisible(true);

        journal->settings[Journal_BMI]=bmi;
        if (mainwin->getOverview()) {
            g=mainwin->getOverview()->graphView()->findGraph(STR_TR_BMI);
            if (g) g->setDay(NULL);
        }
    }
    journal->SetChanged(true);
    if (mainwin->getOverview()) mainwin->getOverview()->ResetGraph("Weight");
}

QString Daily::GetDetailsText()
{
    webView->triggerPageAction(QWebPage::SelectAll);
    QString text=webView->page()->selectedText();
    webView->triggerPageAction(QWebPage::MoveToEndOfDocument);
    webView->triggerPageAction(QWebPage::SelectEndOfDocument);
    return text;
}

void Daily::on_graphCombo_activated(int index)
{
    if (index<0)
        return;

    gGraph *g;
    QString s;
    s=ui->graphCombo->currentText();
    bool b=!ui->graphCombo->itemData(index,Qt::UserRole).toBool();
    ui->graphCombo->setItemData(index,b,Qt::UserRole);
    if (b) {
        ui->graphCombo->setItemIcon(index,*icon_on);
    } else {
        ui->graphCombo->setItemIcon(index,*icon_off);
    }
    g=GraphView->findGraph(s);
    g->setVisible(b);

    updateCube();
    GraphView->updateScale();
    GraphView->redraw();
}
void Daily::updateCube()
{
    //brick..
    if ((GraphView->visibleGraphs()==0)) {
        ui->toggleGraphs->setArrowType(Qt::UpArrow);
        ui->toggleGraphs->setToolTip(tr("Show all graphs"));
        ui->toggleGraphs->blockSignals(true);
        ui->toggleGraphs->setChecked(true);
        ui->toggleGraphs->blockSignals(false);

        if (ui->graphCombo->count()>0) {
            GraphView->setEmptyText(tr("No Graphs On!"));
            GraphView->setCubeImage(images["nographs"]);

        } else {
            GraphView->setEmptyText(STR_TR_NoData);
            GraphView->setCubeImage(images["nodata"]);
        }
    } else {
        GraphView->setCubeImage(images["sheep"]);

        ui->toggleGraphs->setArrowType(Qt::DownArrow);
        ui->toggleGraphs->setToolTip(tr("Hide all graphs"));
        ui->toggleGraphs->blockSignals(true);
        ui->toggleGraphs->setChecked(false);
        ui->toggleGraphs->blockSignals(false);
    }
}


void Daily::on_toggleGraphs_clicked(bool checked)
{
    QString s;
    QIcon *icon=checked ? icon_off : icon_on;
    for (int i=0;i<ui->graphCombo->count();i++) {
        s=ui->graphCombo->itemText(i);
        ui->graphCombo->setItemIcon(i,*icon);
        ui->graphCombo->setItemData(i,!checked,Qt::UserRole);
    }
    for (int i=0;i<GraphView->size();i++) {
        (*GraphView)[i]->setVisible(!checked);
    }

    updateCube();
    GraphView->updateScale();
    GraphView->redraw();

}

void Daily::updateGraphCombo()
{
    ui->graphCombo->clear();
    gGraph *g;

    for (int i=0;i<GraphView->size();i++) {
        g=(*GraphView)[i];
        if (g->isEmpty()) continue;

        if (g->visible()) {
            ui->graphCombo->addItem(*icon_on,g->title(),true);
        } else {
            ui->graphCombo->addItem(*icon_off,g->title(),false);
        }
    }
    ui->graphCombo->setCurrentIndex(0);
    updateCube();
}

void Daily::on_zoomFullyOut_clicked()
{
    GraphView->ResetBounds(true);
    GraphView->redraw();
}

void Daily::on_resetLayoutButton_clicked()
{
    GraphView->resetLayout();
}
