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

#ifndef FLASH
#define FLASH
#include <string>
#include <map>
#include <vector>

#include "decoder.h"
#include "memory.h"

class DecodedInstruction;

class AvrFlash: public Memory {
    protected:
        AvrDevice *core;
	std::vector <DecodedInstruction*> DecodedMem;

    friend int avr_op_CPSE::operator()();
    friend int avr_op_SBIC::operator()();
    friend int avr_op_SBIS::operator()();
    friend int avr_op_SBRC::operator()();
    friend int avr_op_SBRS::operator()();
    friend int AvrDevice::Step(bool &, SystemClockOffset *);
    

    public:
        void Decode();                          //Decode comple memory
        void Decode(int addr );                 //Decode only instruction at addr
        void WriteMem(unsigned char*, unsigned int, unsigned int);
        AvrFlash(AvrDevice *c, int size);
        unsigned int GetSize();
};
#endif
