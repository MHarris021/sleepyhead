/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Intellipap Loader Header
 *
 * Copyright (c) 2011-2014 Mark Watkins <jedimark@users.sourceforge.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the Linux
 * distribution for more details. */

#ifndef INTELLIPAP_LOADER_H
#define INTELLIPAP_LOADER_H

#include "SleepLib/machine.h" // Base class: MachineLoader
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"


//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation.
//
const int intellipap_data_version=2;
//
//********************************************************************************************

/*! \class Intellipap
    \brief Intellipap customized machine object
    */
class Intellipap:public CPAP
{
public:
    Intellipap(Profile *p,MachineID id=0);
    virtual ~Intellipap();
};


const int intellipap_load_buffer_size=1024*1024;


const QString intellipap_class_name=STR_MACH_Intellipap;

/*! \class IntellipapLoader
    \brief Loader for DeVilbiss Intellipap Auto data
    This is only relatively recent addition and still needs more work
    */
class IntellipapLoader : public MachineLoader
{
public:
    IntellipapLoader();
    virtual ~IntellipapLoader();
    //! \brief Scans path for Intellipap data signature, and Loads any new data
    virtual int Open(QString & path,Profile *profile);

    //! \brief Returns SleepLib database version of this IntelliPap loader
    virtual int Version() { return intellipap_data_version; }

    //! \brief Returns the machine class name of this IntelliPap, "Intellipap"
    virtual const QString & ClassName() { return intellipap_class_name; }

    //! \brief Creates a machine object, indexed by serial number
    Machine *CreateMachine(QString serial,Profile *profile);

    //! \brief Registers this MachineLoader with the master list, so Intellipap data can load
    static void Register();
protected:
    QString last;
    QHash<QString,Machine *> MachList;

    unsigned char * m_buffer;
};


#endif // INTELLIPAP_LOADER_H
