/*
 ****************************************************************************
 *
 * simulavr - A simulator for the Atmel AVR family of microcontrollers.
 * Copyright (C) 2001, 2002, 2003   Klaus Rudolph		
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ****************************************************************************
 *
 *  $Id$
 */

#ifndef HWIRQSYSTEM
#define HWIRQSYSTEM

#include <map>

#include "hwsreg.h"
#include "hardware.h"
#include "funktor.h"
#include "printable.h"
#include "avrdevice.h"

class IrqStatisticEntry {
        
    public:
        SystemClockOffset flagSet;
        SystemClockOffset flagCleared;
        SystemClockOffset handlerStarted;
        SystemClockOffset handlerFinished;

        SystemClockOffset setClear;
        SystemClockOffset setStarted;   
        SystemClockOffset setFinished;  
        SystemClockOffset startedFinished;


        IrqStatisticEntry():
          flagSet(0), flagCleared(0),
          handlerStarted(0), handlerFinished(0),
          setClear(0), setStarted(0),   setFinished(0),  startedFinished(0)
        {}
        void CalcDiffs();
};

class IrqStatisticPerVector {
    protected:
        IrqStatisticEntry long_SetClear;
        IrqStatisticEntry short_SetClear;

        IrqStatisticEntry long_SetStarted;
        IrqStatisticEntry short_SetStarted;

        IrqStatisticEntry long_SetFinished;
        IrqStatisticEntry short_SetFinished;

        IrqStatisticEntry long_StartedFinished;
        IrqStatisticEntry short_StartedFinished;

        friend std::ostream& operator<<(std::ostream &os, const IrqStatisticPerVector &ispv) ;

    public:

        IrqStatisticEntry actual;
        IrqStatisticEntry next;

        void CalculateStatistic();
        void CheckComplete();

        IrqStatisticPerVector();
};

std::ostream& operator<<(std::ostream &, const IrqStatisticEntry&);
std::ostream& operator<<(std::ostream &, const IrqStatisticPerVector&);

class IrqStatistic: public Printable {
    protected:
        AvrDevice *core; //only used to get the (file) name of the core device
        
    public:
	std::map<unsigned int, IrqStatisticPerVector> entries;
        IrqStatistic(AvrDevice *);
        void operator()();

        virtual ~IrqStatistic() {}

        friend std::ostream& operator<<(std::ostream &, const IrqStatistic&);
};

std::ostream& operator<<(std::ostream &, const IrqStatistic&);



class HWIrqSystem {
    protected:
        int bytesPerVector;
        HWSreg *status;

        //setup a stack for hardwareIrqPartners
	std::map<unsigned int, Hardware *> irqPartnerList;
        AvrDevice *core;
        IrqStatistic irqStatistic;

    public:
        HWIrqSystem (AvrDevice* _core, int bytes): bytesPerVector(bytes), core(_core), irqStatistic(_core) { }

        unsigned int GetNewPc(unsigned int &vecNo); //if an IRQ occured, we need the new PC,
        //if PC==0xFFFFFFFF there is no IRQ
        unsigned int JumpToVector(unsigned int vector) ;

        //void RegisterIrqPartner(Hardware *, unsigned int vector);
        void SetIrqFlag(Hardware *, unsigned int vector);
        void ClearIrqFlag(unsigned int vector);
        void IrqHandlerStarted(unsigned int vector);
        void IrqHandlerFinished(unsigned int vector);
};


class IrqFunktor: public Funktor {
    protected:
        HWIrqSystem *irqSystem;
        void (HWIrqSystem::*fp)(unsigned int);
        unsigned int vectorNo;

    public:
        IrqFunktor(HWIrqSystem *i, void (HWIrqSystem::*_fp)(unsigned int), unsigned int _vector):irqSystem(i), fp(_fp), vectorNo(_vector) {}
        void operator()() { (irqSystem->*fp)(vectorNo); }
        Funktor* clone() { return new IrqFunktor(*this); }
};



#endif

