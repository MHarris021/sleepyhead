/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * gLineChart Header
 *
 * Copyright (c) 2011-2014 Mark Watkins <jedimark@users.sourceforge.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the Linux
 * distribution for more details. */

#ifndef GLINECHART_H
#define GLINECHART_H

#include <QVector>
#include "SleepLib/event.h"
#include "SleepLib/day.h"
#include "gGraphView.h"
//#include "graphlayer.h"

/*! \class AHIChart
    \brief Another graph calculating the AHI/hour, this one looks at all the sessions for a day. Currently Unused.
    */
class AHIChart:public Layer
{
public:
    //! \brief Constructs an AHIChart object, with QColor col for the line plots.
    AHIChart(QColor col=QColor("black"));
    ~AHIChart();

    //! \brief Draws the precalculated data to the Vertex buffers
    virtual void paint(gGraph & w,int left, int top, int width, int height);

    //! \brief AHI/hr Calculations are done for this day here.
    //! This also uses the sliding window method
    virtual void SetDay(Day *d);

    //! \brief Returns the minimum AHI/hr value caculated
    virtual EventDataType Miny() { return m_miny; }

    //! \brief Returns the maximum AHI/hr value caculated
    virtual EventDataType Maxy() { return m_maxy; }

    //! \brief Returns true if no data was available
    virtual bool isEmpty() { return m_data.size()==0; }

protected:
    //! \brief Contains the plot data (Y-axis) generated for this day
    QVector<EventDataType> m_data;

    //! \brief Contains the time codes (X-axis) generated for this day
    QVector<quint64> m_time;

    EventDataType m_miny;
    EventDataType m_maxy;
    QColor m_color;
    gVertexBuffer * lines;
};

/*! \class gLineChart
    \brief Draws a 2D linechart from all Session data in a day. EVL_Waveforms typed EventLists are accelerated.
    */
class gLineChart:public Layer
{
    public:
        /*! \brief Creates a new 2D gLineChart Layer
            \param code  The Channel that gets drawn by this layer
            \param col  Color of the Plot
            \param square_plot Whether or not to use square plots (only effective for EVL_Event typed EventList data)
            \param disable_accel Whether or not to disable acceleration for EVL_Waveform typed EventList data
            */
        gLineChart(ChannelID code,const QColor col=QColor("black"), bool square_plot=false,bool disable_accel=false);
        virtual ~gLineChart();

        //! \brief The drawing code that fills the vertex buffers
        virtual void paint(gGraph & w,int left, int top, int width, int height);

        //! \brief Set Use Square plots for non EVL_Waveform data
        void SetSquarePlot(bool b) { m_square_plot=b; }

        //! \brief Returns true if using Square plots for non EVL_Waveform data
        bool GetSquarePlot() { return m_square_plot; }

        //! \brief Set this if you want this layer to draw it's empty data message
        //! They don't show anymore due to the changes in gGraphView. Should modify isEmpty if this still gets to live
        void ReportEmpty(bool b) { m_report_empty=b; }

        //! \brief Returns whether or not to show the empty text message
        bool GetReportEmpty() { return m_report_empty; }

        //! \brief Sets the ability to Disable waveform plot acceleration (which hides unseen data)
        void setDisableAccel(bool b) { m_disable_accel=b; }

        //! \brief Returns true if waveform plot acceleration is disabled
        bool disableAccel() { return m_disable_accel; }

        //! \brief Sets the Day object containing the Sessions this linechart draws from
        virtual void SetDay(Day *d);

        //! \brief Returns Minimum Y-axis value for this layer
        virtual EventDataType Miny();

        //! \brief Returns Maximum Y-axis value for this layer
        virtual EventDataType Maxy();

        //! \brief Returns true if all subplots contain no data
        virtual bool isEmpty();

        //! \brief Add Subplot 'code'. Note the first one is added in the constructor.
        void addPlot(ChannelID code, QColor color, bool square) { m_codes.push_back(code); m_colors.push_back(color); m_enabled[code]=true; m_square.push_back(square); }

        //! \brief Returns true of the subplot 'code' is enabled.
        bool plotEnabled(ChannelID code) { if ((m_enabled.contains(code)) && m_enabled[code]) return true; else return false; }

        //! \brief Enable or Disable the subplot identified by code.
        void setPlotEnabled(ChannelID code, bool b) { m_enabled[code]=b; }

protected:
        bool m_report_empty;
        bool m_square_plot;
        bool m_disable_accel;
        QColor m_line_color;

        gVertexBuffer * lines;
        //GLShortBuffer * lines;
        //GLShortBuffer * outlines;

        //! \brief Used by accelerated waveform plots. Must be >= Screen Resolution (or at least graph width)
        static const int max_drawlist_size=10000;

        //! \brief The list of screen points used for accelerated waveform plots..
        QPoint m_drawlist[max_drawlist_size];

        int subtract_offset;

        QVector<ChannelID> m_codes;
        QVector<QColor> m_colors;
        QVector<bool> m_square;
        QHash<ChannelID,bool> m_enabled;
};

#endif // GLINECHART_H
