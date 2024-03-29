/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * SleepLib Common Functions
 *
 * Copyright (c) 2011-2014 Mark Watkins <jedimark@users.sourceforge.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the Linux
 * distribution for more details. */

#include <QDateTime>
#include <QDir>

#include "profiles.h"

// Used by internal settings


const QString getDeveloperName()
{
    return STR_DeveloperName;
}

const QString getAppName()
{
    QString name=STR_AppName;
#ifdef UNSTABLE_BUILD
    name+=STR_Unstable;
#endif
    return name;
}

const QString getDefaultAppRoot()
{
    QString approot=STR_AppRoot;
#ifdef UNSTABLE_BUILD
    approot+=STR_Unstable;
#endif
    return approot;
}


qint64 timezoneOffset() {
    static bool ok=false;
    static qint64 _TZ_offset=0;

    if (ok) return _TZ_offset;
    QDateTime d1=QDateTime::currentDateTime();
    QDateTime d2=d1;
    d1.setTimeSpec(Qt::UTC);
    _TZ_offset=d2.secsTo(d1);
    _TZ_offset*=1000L;
    return _TZ_offset;
}

QString weightString(float kg, UnitSystem us)
{
    if (us==US_Undefined)
        us=PROFILE.general->unitSystem();

    if (us==US_Metric) {
        return QString("%1kg").arg(kg,0,'f',2);
    } else if (us==US_Archiac) {
        int oz=(kg*1000.0) / (float)ounce_convert;
        int lb=oz / 16.0;
        oz = oz % 16;
        return QString("%1lb %2oz").arg(lb,0,10).arg(oz);
    }
    return("Bad UnitSystem");
}

bool operator <(const ValueCount & a, const ValueCount & b)
{
     return a.value < b.value;
}

bool removeDir(const QString & path)
{
    bool result = true;
    QDir dir(path);

    if (dir.exists(path)) {
        Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
            if (info.isDir()) { // Recurse to remove this child directory
                result = removeDir(info.absoluteFilePath());
            } else { // File
                result = QFile::remove(info.absoluteFilePath());
            }

            if (!result) {
                return result;
            }
        }
        result = dir.rmdir(path);
    }

    return result;
}

QString STR_UNIT_CM;
QString STR_UNIT_INCH;
QString STR_UNIT_FOOT;
QString STR_UNIT_POUND;
QString STR_UNIT_OUNCE;
QString STR_UNIT_KG;
QString STR_UNIT_CMH2O;
QString STR_UNIT_Hours;
QString STR_UNIT_BPM;       // Beats per Minute
QString STR_UNIT_LPM;       // Litres per Minute

QString STR_MESSAGE_ERROR;
QString STR_MESSAGE_WARNING;

QString STR_TR_BMI;         // Short form of Body Mass Index
QString STR_TR_Weight;
QString STR_TR_Zombie;
QString STR_TR_PulseRate;   // Pulse / Heart rate
QString STR_TR_SpO2;
QString STR_TR_Plethy;      // Plethysomogram
QString STR_TR_Pressure;

QString STR_TR_Daily;
QString STR_TR_Overview;
QString STR_TR_Oximetry;

QString STR_TR_Oximeter;
QString STR_TR_EventFlags;

// Machine type names.
QString STR_TR_CPAP;    // Constant Positive Airway Pressure
QString STR_TR_BIPAP;   // Bi-Level Positive Airway Pressure
QString STR_TR_BiLevel; // Another name for BiPAP
QString STR_TR_EPAP;    // Expiratory Positive Airway Pressure
QString STR_TR_EPAPLo;  // Expiratory Positive Airway Pressure, Low
QString STR_TR_EPAPHi;  // Expiratory Positive Airway Pressure, High
QString STR_TR_IPAP;    // Inspiratory Positive Airway Pressure
QString STR_TR_IPAPLo;  // Inspiratory Positive Airway Pressure, Low
QString STR_TR_IPAPHi;  // Inspiratory Positive Airway Pressure, High
QString STR_TR_APAP;    // Automatic Positive Airway Pressure
QString STR_TR_ASV;     // Assisted Servo Ventilator
QString STR_TR_STASV;

QString STR_TR_Humidifier;

QString STR_TR_H;       // Short form of Hypopnea
QString STR_TR_OA;      // Short form of Obstructive Apnea
QString STR_TR_UA;      // Short form of Unspecified Apnea
QString STR_TR_CA;      // Short form of Clear Airway Apnea
QString STR_TR_FL;      // Short form of Flow Limitation
QString STR_TR_LE;      // Short form of Leak Event
QString STR_TR_EP;      // Short form of Expiratory Puff
QString STR_TR_VS;      // Short form of Vibratory Snore
QString STR_TR_VS2;     // Short form of Secondary Vibratory Snore (Some Philips Respironics Machines have two sources)
QString STR_TR_RERA;    // Acronym for Respiratory Effort Related Arousal
QString STR_TR_PP;      // Short form for Pressure Pulse
QString STR_TR_P;       // Short form for Pressure Event
QString STR_TR_RE;      // Short form of Respiratory Effort Related Arousal
QString STR_TR_NR;      // Short form of Non Responding event? (forgot sorry)
QString STR_TR_NRI;     // Sorry I Forgot.. it's a flag on Intellipap machines
QString STR_TR_O2;      // SpO2 Desaturation
QString STR_TR_PC;      // Short form for Pulse Change
QString STR_TR_UF1;     // Short form for User Flag 1
QString STR_TR_UF2;     // Short form for User Flag 2
QString STR_TR_UF3;     // Short form for User Flag 3



QString STR_TR_PS;     // Short form of Pressure Support
QString STR_TR_AHI;    // Short form of Apnea Hypopnea Index
QString STR_TR_RDI;    // Short form of Respiratory Distress Index
QString STR_TR_AI;     // Short form of Apnea Index
QString STR_TR_HI;     // Short form of Hypopnea Index
QString STR_TR_UAI;    // Short form of Uncatagorized Apnea Index
QString STR_TR_CAI;    // Short form of Clear Airway Index
QString STR_TR_FLI;    // Short form of Flow Limitation Index
QString STR_TR_REI;    // Short form of RERA Index
QString STR_TR_EPI;    // Short form of Expiratory Puff Index
QString STR_TR_CSR;    // Short form of Cheyne Stokes Respiration
QString STR_TR_PB;     // Short form of Periodic Breathing


// Graph Titles
QString STR_TR_IE;              // Inspiratory Expiratory Ratio
QString STR_TR_InspTime;        // Inspiratory Time
QString STR_TR_ExpTime;         // Expiratory Time
QString STR_TR_RespEvent;       // Respiratory Event
QString STR_TR_FlowLimitation;
QString STR_TR_FlowLimit;
QString STR_TR_PatTrigBreath;   // Patient Triggered Breath
QString STR_TR_TgtMinVent;      // Target Minute Ventilation
QString STR_TR_TargetVent;      // Target Ventilation
QString STR_TR_MinuteVent;      // Minute Ventilation
QString STR_TR_TidalVolume;
QString STR_TR_RespRate;        // Respiratory Rate
QString STR_TR_Snore;
QString STR_TR_Leak;
QString STR_TR_Leaks;
QString STR_TR_TotalLeaks;
QString STR_TR_UnintentionalLeaks;
QString STR_TR_MaskPressure;
QString STR_TR_FlowRate;
QString STR_TR_SleepStage;
QString STR_TR_Usage;
QString STR_TR_Sessions;
QString STR_TR_PrRelief; // Pressure Relief

QString STR_TR_NoData;
QString STR_TR_Bookmarks;
QString STR_TR_SleepyHead;
QString STR_TR_SleepyHeadVersion;

QString STR_TR_Mode;
QString STR_TR_Model;
QString STR_TR_Brand;
QString STR_TR_Serial;
QString STR_TR_Machine;
QString STR_TR_Channel;
QString STR_TR_Settings;

QString STR_TR_Name;
QString STR_TR_DOB;    // Date of Birth
QString STR_TR_Phone;
QString STR_TR_Address;
QString STR_TR_Email;
QString STR_TR_PatientID;
QString STR_TR_Date;

QString STR_TR_BedTime;
QString STR_TR_WakeUp;
QString STR_TR_MaskTime;
QString STR_TR_Unknown;
QString STR_TR_None;
QString STR_TR_Ready;

QString STR_TR_First;
QString STR_TR_Last;
QString STR_TR_Start;
QString STR_TR_End;
QString STR_TR_On;
QString STR_TR_Off;

QString STR_TR_Min;    // Minimum
QString STR_TR_Max;    // Maximum

QString STR_TR_Average;
QString STR_TR_Median;
QString STR_TR_Avg;    // Short form of Average
QString STR_TR_WAvg;   // Short form of Weighted Average

void initializeStrings()
{
    STR_UNIT_CM=QObject::tr("cm");
    STR_UNIT_INCH=QObject::tr("\"");
    STR_UNIT_FOOT=QObject::tr("ft");
    STR_UNIT_POUND=QObject::tr("lb");
    STR_UNIT_OUNCE=QObject::tr("oz");
    STR_UNIT_KG=QObject::tr("Kg");
    STR_UNIT_CMH2O=QObject::tr("cmH2O");
    STR_UNIT_Hours=QObject::tr("Hours");

    STR_UNIT_BPM=QObject::tr("bpm");          // Beats per Minute
    STR_UNIT_LPM=QObject::tr("L/m");          // Litres per Minute

    STR_MESSAGE_ERROR=QObject::tr("Error");
    STR_MESSAGE_WARNING=QObject::tr("Warning");

    STR_TR_BMI=QObject::tr("BMI");                // Short form of Body Mass Index
    STR_TR_Weight=QObject::tr("Weight");
    STR_TR_Zombie=QObject::tr("Zombie");
    STR_TR_PulseRate=QObject::tr("Pulse Rate");   // Pulse / Heart rate
    STR_TR_SpO2=QObject::tr("SpO2");
    STR_TR_Plethy=QObject::tr("Plethy");          // Plethysomogram
    STR_TR_Pressure=QObject::tr("Pressure");

    STR_TR_Daily=QObject::tr("Daily");
    STR_TR_Overview=QObject::tr("Overview");
    STR_TR_Oximetry=QObject::tr("Oximetry");

    STR_TR_Oximeter=QObject::tr("Oximeter");
    STR_TR_EventFlags=QObject::tr("Event Flags");

    // Machine type names.
    STR_TR_CPAP=QObject::tr("CPAP");      // Constant Positive Airway Pressure
    STR_TR_BIPAP=QObject::tr("BiPAP");    // Bi-Level Positive Airway Pressure
    STR_TR_BiLevel=QObject::tr("Bi-Level"); // Another name for BiPAP
    STR_TR_EPAP=QObject::tr("EPAP");      // Expiratory Positive Airway Pressure
    STR_TR_EPAPLo=QObject::tr("Min EPAP"); // Lower Expiratory Positive Airway Pressure
    STR_TR_EPAPHi=QObject::tr("Max EPAP"); // Higher Expiratory Positive Airway Pressure
    STR_TR_IPAP=QObject::tr("IPAP");      // Inspiratory Positive Airway Pressure
    STR_TR_IPAPLo=QObject::tr("Min IPAP");  // Lower Inspiratory Positive Airway Pressure
    STR_TR_IPAPHi=QObject::tr("Max IPAP");  // Higher Inspiratory Positive Airway Pressure
    STR_TR_APAP=QObject::tr("APAP");      // Automatic Positive Airway Pressure
    STR_TR_ASV=QObject::tr("ASV");        // Assisted Servo Ventilator
    STR_TR_STASV=QObject::tr("ST/ASV");

    STR_TR_Humidifier=QObject::tr("Humidifier");

    STR_TR_H=QObject::tr("H");        // Short form of Hypopnea
    STR_TR_OA=QObject::tr("OA");      // Short form of Obstructive Apnea
    STR_TR_UA=QObject::tr("A");       // Short form of Unspecified Apnea
    STR_TR_CA=QObject::tr("CA");      // Short form of Clear Airway Apnea
    STR_TR_FL=QObject::tr("FL");      // Short form of Flow Limitation
    STR_TR_LE=QObject::tr("LE");      // Short form of Leak Event
    STR_TR_EP=QObject::tr("EP");      // Short form of Expiratory Puff
    STR_TR_VS=QObject::tr("VS");      // Short form of Vibratory Snore
    STR_TR_VS2=QObject::tr("VS2");    // Short form of Secondary Vibratory Snore (Some Philips Respironics Machines have two sources)
    STR_TR_RERA=QObject::tr("RERA");  // Acronym for Respiratory Effort Related Arousal
    STR_TR_PP=QObject::tr("PP");      // Short form for Pressure Pulse
    STR_TR_P=QObject::tr("P");        // Short form for Pressure Event
    STR_TR_RE=QObject::tr("RE");      // Short form of Respiratory Effort Related Arousal
    STR_TR_NR=QObject::tr("NR");      // Short form of Non Responding event? (forgot sorry)
    STR_TR_NRI=QObject::tr("NRI");    // Sorry I Forgot.. it's a flag on Intellipap machines
    STR_TR_O2=QObject::tr("O2");      // SpO2 Desaturation
    STR_TR_PC=QObject::tr("PC");      // Short form for Pulse Change
    STR_TR_UF1=QObject::tr("UF1");      // Short form for User Flag 1
    STR_TR_UF2=QObject::tr("UF2");      // Short form for User Flag 2
    STR_TR_UF3=QObject::tr("UF3");      // Short form for User Flag 3



    STR_TR_PS=QObject::tr("PS");      // Short form of Pressure Support
    STR_TR_AHI=QObject::tr("AHI");    // Short form of Apnea Hypopnea Index
    STR_TR_RDI=QObject::tr("RDI");    // Short form of Respiratory Distress Index
    STR_TR_AI=QObject::tr("AI");      // Short form of Apnea Index
    STR_TR_HI=QObject::tr("HI");      // Short form of Hypopnea Index
    STR_TR_UAI=QObject::tr("UAI");    // Short form of Uncatagorized Apnea Index
    STR_TR_CAI=QObject::tr("CAI");    // Short form of Clear Airway Index
    STR_TR_FLI=QObject::tr("FLI");    // Short form of Flow Limitation Index
    STR_TR_REI=QObject::tr("REI");    // Short form of RERA Index
    STR_TR_EPI=QObject::tr("EPI");    // Short form of Expiratory Puff Index
    STR_TR_CSR=QObject::tr("ÇSR");    // Short form of Cheyne Stokes Respiration
    STR_TR_PB=QObject::tr("PB");      // Short form of Periodic Breathing


    // Graph Titles
    STR_TR_IE=QObject::tr("IE");      // Inspiratory Expiratory Ratio
    STR_TR_InspTime=QObject::tr("Insp. Time");    // Inspiratory Time
    STR_TR_ExpTime=QObject::tr("Exp. Time");      // Expiratory Time
    STR_TR_RespEvent=QObject::tr("Resp. Event");    // Respiratory Event
    STR_TR_FlowLimitation=QObject::tr("Flow Limitation");
    STR_TR_FlowLimit=QObject::tr("Flow Limit");
    STR_TR_PatTrigBreath=QObject::tr("Pat. Trig. Breath"); // Patient Triggered Breath
    STR_TR_TgtMinVent=QObject::tr("Tgt. Min. Vent");        // Target Minute Ventilation
    STR_TR_TargetVent=QObject::tr("Target Vent.");          // Target Ventilation
    STR_TR_MinuteVent=QObject::tr("Minute Vent."); // Minute Ventilation
    STR_TR_TidalVolume=QObject::tr("Tidal Volume");
    STR_TR_RespRate=QObject::tr("Resp. Rate");    // Respiratory Rate
    STR_TR_Snore=QObject::tr("Snore");
    STR_TR_Leak=QObject::tr("Leak");
    STR_TR_Leaks=QObject::tr("Leaks");
    STR_TR_TotalLeaks=QObject::tr("Total Leaks");
    STR_TR_UnintentionalLeaks=QObject::tr("Unintentional Leaks");
    STR_TR_MaskPressure=QObject::tr("MaskPressure");
    STR_TR_FlowRate=QObject::tr("Flow Rate");
    STR_TR_SleepStage=QObject::tr("Sleep Stage");
    STR_TR_Usage=QObject::tr("Usage");
    STR_TR_Sessions=QObject::tr("Sessions");
    STR_TR_PrRelief=QObject::tr("Pr. Relief"); // Pressure Relief

    STR_TR_NoData=QObject::tr("No Data");
    STR_TR_Bookmarks=QObject::tr("Bookmarks");
    STR_TR_SleepyHead=QObject::tr("SleepyHead");
    STR_TR_SleepyHeadVersion=STR_TR_SleepyHead+" v"+VersionString;

    STR_TR_Mode=QObject::tr("Mode");
    STR_TR_Model=QObject::tr("Model");
    STR_TR_Brand=QObject::tr("Brand");
    STR_TR_Serial=QObject::tr("Serial");
    STR_TR_Machine=QObject::tr("Machine");
    STR_TR_Channel=QObject::tr("Channel");
    STR_TR_Settings=QObject::tr("Settings");

    STR_TR_Name=QObject::tr("Name");
    STR_TR_DOB=QObject::tr("DOB");    // Date of Birth
    STR_TR_Phone=QObject::tr("Phone");
    STR_TR_Address=QObject::tr("Address");
    STR_TR_Email=QObject::tr("Email");
    STR_TR_PatientID=QObject::tr("Patient ID");
    STR_TR_Date=QObject::tr("Date");

    STR_TR_BedTime=QObject::tr("Bedtime");
    STR_TR_WakeUp=QObject::tr("Wake-up");
    STR_TR_MaskTime=QObject::tr("Mask Time");
    STR_TR_Unknown=QObject::tr("Unknown");
    STR_TR_None=QObject::tr("None");
    STR_TR_Ready=QObject::tr("Ready");

    STR_TR_First=QObject::tr("First");
    STR_TR_Last=QObject::tr("Last");
    STR_TR_Start=QObject::tr("Start");
    STR_TR_End=QObject::tr("End");
    STR_TR_On=QObject::tr("On");
    STR_TR_Off=QObject::tr("Off");

    STR_TR_Min=QObject::tr("Min");    // Minimum
    STR_TR_Max=QObject::tr("Max");    // Maximum

    STR_TR_Average=QObject::tr("Average");
    STR_TR_Median=QObject::tr("Median");
    STR_TR_Avg=QObject::tr("Avg");        // Average
    STR_TR_WAvg=QObject::tr("W-Avg");     // Weighted Average
}
