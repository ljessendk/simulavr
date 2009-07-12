/*
 ****************************************************************************
 *
 * simulavr - A simulator for the Atmel AVR family of microcontrollers.
 * Copyright (C) 2001, 2002, 2003   Theodore A. Roth, Klaus Rudolph		
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

#include "decoder.h"
#include "hwstack.h"
#include "flash.h"
#include "hwwado.h"



void avr_core_stack_push( AvrDevice *core, int cnt, long val);
dword avr_core_stack_pop( AvrDevice *core, int cnt); 
static int n_bit_unsigned_to_signed( unsigned int val, int n );

static int get_add_carry( byte res, byte rd, byte rr, int b );
static int get_add_overflow( byte res, byte rd, byte rr );
static int get_sub_carry( byte res, byte rd, byte rr, int b );
static int get_sub_overflow( byte res, byte rd, byte rr );
static int get_compare_carry( byte res, byte rd, byte rr, int b );
static int get_compare_overflow( byte res, byte rd, byte rr );
enum decoder_operand_masks {
    /** 2 bit register id  ( R24, R26, R28, R30 ) */
    mask_Rd_2     = 0x0030,
    /** 3 bit register id  ( R16 - R23 ) */
    mask_Rd_3     = 0x0070,
    /** 4 bit register id  ( R16 - R31 ) */
    mask_Rd_4     = 0x00f0,
    /** 5 bit register id  ( R00 - R31 ) */
    mask_Rd_5     = 0x01f0,

    /** 3 bit register id  ( R16 - R23 ) */
    mask_Rr_3     = 0x0007,
    /** 4 bit register id  ( R16 - R31 ) */
    mask_Rr_4     = 0x000f,
    /** 5 bit register id  ( R00 - R31 ) */
    mask_Rr_5     = 0x020f,

    /** for 8 bit constant */
    mask_K_8      = 0x0F0F,
    /** for 6 bit constant */
    mask_K_6      = 0x00CF,

    /** for 7 bit relative address */
    mask_k_7      = 0x03F8,
    /** for 12 bit relative address */
    mask_k_12     = 0x0FFF,
    /** for 22 bit absolute address */
    mask_k_22     = 0x01F1,

    /** register bit select */
    mask_reg_bit  = 0x0007,
    /** status register bit select */
    mask_sreg_bit = 0x0070,
    /** address displacement (q) */
    mask_q_displ  = 0x2C07,

    /** 5 bit register id  ( R00 - R31 ) */
    mask_A_5      = 0x00F8,
    /** 6 bit IO port id */
    mask_A_6      = 0x060F
};
/*++++++++++++*/
static int get_rd_2( word opcode );
static int get_rd_3( word opcode );
static int get_rd_4( word opcode );
static int get_rd_5( word opcode );
static int get_rr_3( word opcode );
static int get_rr_4( word opcode );
static int get_rr_5( word opcode );
static byte get_K_8( word opcode );
static byte get_K_6( word opcode );
static int get_k_7( word opcode );
static int get_k_12( word opcode );
static int get_k_22( word opcode );
static int get_reg_bit( word opcode );
static int get_sreg_bit( word opcode );
static int get_q( word opcode );
static int get_A_5( word opcode );
static int get_A_6( word opcode );

int funyFunc(int x) { return 0; }

avr_op_ADC::avr_op_ADC(word opcode, AvrDevice *c) : 
    DecodedInstruction(c, get_rd_5(opcode),get_rr_5(opcode)), R1(((*(c->R))[get_rd_5(opcode)])), R2(((*(c->R))[get_rr_5(opcode)])), 
    status(core->status)
{ }

int avr_op_ADC::operator()() { 
    unsigned char rd=R1;
    unsigned char rr=R2;
    unsigned char res = rd + rr + status->C;

    status->H = get_add_carry( res, rd, rr, 3 ) ;
    status->V = get_add_overflow( res, rd, rr ) ;
    status->N = ((res >> 7) & 0x1)              ;
    status->S = (status->N ^ status->V)   ;
    status->Z = ((res & 0xff) == 0)             ;
    status->C = get_add_carry( res, rd, rr, 7 ) ;

    R1=res;

    return 1;   //used clocks
}

avr_op_ADD::avr_op_ADD(word opcode, AvrDevice *c) : 
    DecodedInstruction(c, get_rd_5(opcode), get_rr_5(opcode)),
    R1(((*(c->R))[get_rd_5(opcode)])),
    R2(((*(c->R))[get_rr_5(opcode)])), 
    status(core->status)
{ }

int avr_op_ADD::operator()() { 
    unsigned char rd=R1;
    unsigned char rr=R2;
    unsigned char res = rd + rr ;

    status->H = get_add_carry( res, rd, rr, 3 ) ;
    status->V = get_add_overflow( res, rd, rr ) ;
    status->N = ((res >> 7) & 0x1)              ;
    status->S = (status->N ^ status->V)   ;
    status->Z = ((res & 0xff) == 0)             ;
    status->C = get_add_carry( res, rd, rr, 7 ) ;

    R1=res;

    return 1;   //used clocks
}

avr_op_ADIW::avr_op_ADIW(word opcode, AvrDevice *c): 
    DecodedInstruction(c, get_K_6(opcode),  get_rd_2(opcode)),
    Rl( (*(core->R))[get_rd_2(opcode)]),
    Rh( (*(core->R))[get_rd_2(opcode)+1]),
    K(get_K_6(opcode)),
    status(c->status) {
    }

int avr_op_ADIW::operator()() {
    word rd = (Rh << 8) + Rl;
    word res = rd + K;

    unsigned char rdh= Rh;


    status->V = (~(rdh >> 7 & 0x1) & (res >> 15 & 0x1)) ; 
    status->N = ((res >> 15) & 0x1)                     ;
    status->S = (status->N ^ status->V)                                 ;
    status->Z = ((res & 0xffff) == 0)                   ;
    status->C = (~(res >> 15 & 0x1) & (rdh >> 7 & 0x1)) ; 

    Rl= res &0xff;
    Rh= res>>8;

    return 2; 
}

avr_op_AND::avr_op_AND(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_rr_5(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    R2((*(core->R))[get_rr_5(opcode)]),
    status(core->status) {}


    int avr_op_AND::operator()() {

        byte res = R1 & R2;


        status->V = 0                   ;
        status->N = ((res >> 7) & 0x1)  ; 
        status->S = (status->N ^ status->V)             ;
        status->Z = ((res & 0xff) == 0) ; 

        R1= res;



        return 1; 
    }

avr_op_ANDI::avr_op_ANDI
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_4(opcode), get_K_8(opcode)),
    R1((*(core->R))[get_rd_4(opcode)]),
    status(core->status) ,
    K(get_K_8(opcode))
{}

int avr_op_ANDI::operator()() 
{
    byte rd = R1;
    byte res = rd & K;

    status->V = 0                   ;
    status->N = ((res >> 7) & 0x1)  ; 
    status->S = (status->N ^ status->V) ;
    status->Z = ((res & 0xff) == 0) ; 

    R1=res;
    return 1;
}

avr_op_ASR::avr_op_ASR
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    status(core->status) 
{}

int avr_op_ASR::operator()() 
{
    byte rd = R1; 
    byte res = (rd >> 1) + (rd & 0x80);

    status->N = ((res >> 7) & 0x1) ;
    status->C = (rd & 0x1) ;
    status->V = (status->N ^ status->C) ;
    status->S = (status->N ^ status->V) ;
    status->Z = ((res & 0xff) == 0) ;

    R1=res;


    return 1;
}


avr_op_BCLR::avr_op_BCLR
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_sreg_bit(opcode),0),
    status(core->status) ,
    K(~(1<<get_sreg_bit(opcode)))
{}

int avr_op_BCLR::operator()() 
{
    //(*status)&=K;
    *status= (*status)&K;
    return 1;
}

avr_op_BLD::avr_op_BLD
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_reg_bit(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    Kadd(1<<get_reg_bit(opcode)),
    Kremove(~(1<<get_reg_bit(opcode))),
    status(core->status) 
{}

int avr_op_BLD::operator()() 
{


    byte rd  = R1;
    int  T   = status->T;
    byte res;

    if (T == 0)
        res = rd & Kremove;
    else
        res = rd | Kadd; 

    R1=res;

    return 1;
}

avr_op_BRBC::avr_op_BRBC
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_reg_bit(opcode), n_bit_unsigned_to_signed( get_k_7(opcode), 7 )),
    status(core->status) ,
    bitmask(1<<get_reg_bit(opcode)),
    offset(n_bit_unsigned_to_signed( get_k_7(opcode), 7))
{}

int avr_op_BRBC::operator()() 
{

    int clks;

    if  ((bitmask & (*(status)))==0)
    {
        core->PC+=offset;
        clks=2;
    }
    else
    {
        clks=1;
    }

    return clks;
}


avr_op_BRBS::avr_op_BRBS
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_reg_bit(opcode), n_bit_unsigned_to_signed( get_k_7(opcode), 7 )),
    status(core->status) ,
    bitmask(1<<get_reg_bit(opcode)),
    offset(n_bit_unsigned_to_signed( get_k_7(opcode), 7))
{}

int avr_op_BRBS::operator()()
{

    int clks;

    if  ((bitmask & (*(status)))!=0)
    {
        core->PC+=offset;
        clks=2;
    }
    else
    {
        clks=1;
    }

    return clks;
}

avr_op_BSET::avr_op_BSET
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_reg_bit(opcode)),
    status(core->status) ,
    K(1<<get_sreg_bit(opcode))
{}

int avr_op_BSET::operator()() 
{

    *(status)=*(status)|K;
    return 1;
}

avr_op_BST::avr_op_BST
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_reg_bit(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    status(core->status) ,
    K(1<<get_reg_bit(opcode))
{}

int avr_op_BST::operator()() 
{

    status->T= ((R1&K)!=0); 

    return 1;
}



avr_op_CALL::avr_op_CALL
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0, 0, true),
    KH(get_k_22(opcode))
{}

int avr_op_CALL::operator()() 
{

    int kh=KH;
    word *MemPtr=(word*)core->Flash->myMemory;
    word offset=MemPtr[(core->PC)+1];         //this is k!
    offset=(offset>>8)+((offset&0xff)<<8);
    int kl=offset;
    int k=(kh<<16)+kl;
    int pc_bytes=core->PC_size;

    avr_core_stack_push( core, pc_bytes, core->PC+2 );
    core->PC=k-1;

    return pc_bytes+2;
}

avr_op_CBI::avr_op_CBI
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_A_5(opcode), get_reg_bit(opcode)),
    ioreg((*(core->ioreg))[get_A_5(opcode)]),
    K(~(1<<get_reg_bit(opcode)))
{}

int avr_op_CBI::operator()() 
{

    ioreg=ioreg&K;
    return 2;
}

avr_op_COM::avr_op_COM
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    status(core->status) 
{}

int avr_op_COM::operator()() 
{

    byte rd  = R1;
    byte res = 0xff - rd;


    status->N = ((res >> 7) & 0x1) ;
    status->C = 1 ;
    status->V = 0 ;
    status->S = (status->N ^ status->V) ;
    status->Z = ((res & 0xff) == 0) ;

    R1= res;

    return 1;
}

avr_op_CP::avr_op_CP
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),get_rr_5(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    R2((*(core->R))[get_rr_5(opcode)]),
    status(core->status) 
{}

int avr_op_CP::operator()() 
{

    byte rd  = R1;
    byte rr  = R2;
    byte res = rd - rr;


    status->H = get_compare_carry( res, rd, rr, 3 ) ;
    status->V = get_compare_overflow( res, rd, rr ) ;
    status->N = ((res >> 7) & 0x1);
    status->S = (status->N ^ status->V);
    status->Z = ((res & 0xff) == 0) ;
    status->C = get_compare_carry( res, rd, rr, 7 ) ;

    return 1;
}

avr_op_CPC::avr_op_CPC
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_rr_5(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    R2((*(core->R))[get_rr_5(opcode)]),
    status(core->status) 
{}

int avr_op_CPC::operator()() 
{

    byte rd  = R1;
    byte rr  = R2;
    byte res = rd - rr - status->C;


    status->H = get_compare_carry( res, rd, rr, 3 ) ;
    status->V = get_compare_overflow( res, rd, rr ) ;
    status->N = ((res >> 7) & 0x1) ;
    status->S = (status->N ^ status->V) ;
    status->C = get_compare_carry( res, rd, rr, 7 ) ;

    /* Previous value remains unchanged when result is 0; cleared otherwise */
    bool Z = ((res & 0xff) == 0);
    bool prev_Z = core->status->Z;
    status->Z = Z && prev_Z ;

    return 1;
}


avr_op_CPI::avr_op_CPI
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_4(opcode), get_K_8(opcode)),
    R1((*(core->R))[get_rd_4(opcode)]),
    status(core->status) ,
    K(get_K_8(opcode))
{}

int avr_op_CPI::operator()() 
{

    byte rd  = R1;
    byte res = rd - K;

    status->H = get_compare_carry( res, rd, K, 3 ) ;
    status->V = get_compare_overflow( res, rd, K ) ;
    status->N = ((res >> 7) & 0x1) ;
    status->S = (status->N ^ status->V) ;
    status->Z = ((res & 0xff) == 0) ;
    status->C = get_compare_carry( res, rd, K, 7 ) ;

    return 1;
}

avr_op_CPSE::avr_op_CPSE
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_rr_5(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    R2((*(core->R))[get_rr_5(opcode)]),
    status(core->status) 
{}

int avr_op_CPSE::operator()() 
{

    int skip;

    byte rd = R1;
    byte rr = R2;
    int clks;

    if ( core->Flash->DecodedMem[(core->PC)+1]->IsInstruction2Words() ) {
        skip = 3;
    } else {
        skip = 2;
    }

    if (rd == rr)
    {
        core->PC+=skip-1;
        clks=skip;
    }
    else
    {
        clks=1;
    }

    return clks;
}


avr_op_DEC::avr_op_DEC
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), 0),
    R1((*(core->R))[get_rd_5(opcode)]),
    status(core->status) 
{}

int avr_op_DEC::operator()() 
{

    byte res = R1-1;

    status->N = ((res >> 7) & 0x1) ;
    status->V = (res == 0x7f) ;
    status->S = (status->N ^ status->V) ;
    status->Z = ((res & 0xff) == 0) ;

    R1=res;

    return 1;
}

avr_op_EICALL:: avr_op_EICALL
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0, 0),
    RL((*(core->R))[30]),
    RH((*(core->R))[31]),
    eind((*(core->ioreg))[EIND])
{}

int  avr_op_EICALL::operator()() 
{

    int pc_bytes = 3;

    int new_PC=RL+(RH<<8)+((*(core->ioreg))[EIND]<<16);
    //avr_warning( "needs serious code review\n" );

    avr_core_stack_push( core, pc_bytes, (core->PC)+1 );

    core->PC=new_PC;

    return 4;
}

avr_op_EIJMP::avr_op_EIJMP
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0,0),
    RL((*(core->R))[30]),
    RH((*(core->R))[31]),
    eind((*(core->ioreg))[EIND])
{}

int avr_op_EIJMP::operator()() 
{

    core->PC = ((eind & 0x3f) << 16) + (RH << 8)+ RL;

    return 2;
}

avr_op_ELPM_Z::avr_op_ELPM_Z
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    ZL((*(core->R))[30]),
    ZH((*(core->R))[31])
{}

int avr_op_ELPM_Z::operator()() 
{

    int Z, flash_addr;
    word data;

    Z = ((core->GetRampz() & 0x3f) << 16) +
        (ZH << 8) +
        ZL;


    flash_addr = Z ^ 0x0001;
    data = core->Flash->myMemory[flash_addr];
    R1=data;

    return 3;
}

avr_op_ELPM_Z_incr::avr_op_ELPM_Z_incr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    ZL((*(core->R))[30]),
    ZH((*(core->R))[31])
{}

int avr_op_ELPM_Z_incr::operator()() 
{


    int Z, flash_addr;
    word data;

    Z = ((core->GetRampz() & 0x3f) << 16) +
        (ZH << 8) +
        ZL;

    flash_addr = Z ^ 0x0001;

    data = core->Flash->myMemory[flash_addr];
    R1=data;

    /* post increment Z */
    Z += 1;
    core->SetRampz((Z >> 16) & 0x3f);
    ZL=Z&0xff;
    ZH=(Z>>8)&0xff;

    return 3;
}


avr_op_ELPM::avr_op_ELPM
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0,0),
    R0((*(core->R))[0]),
    ZL((*(core->R))[30]),
    ZH((*(core->R))[31])
{}

int avr_op_ELPM::operator()() 
{

    int Z, flash_addr;
    word data;

    Z = ((core->GetRampz() & 0x3f) << 16) +
        (ZH << 8) +
        ZL;

    flash_addr = Z ^ 0x0001;

    data = core->Flash->myMemory[flash_addr];
    R0=data;
    return 3;
}


avr_op_EOR::avr_op_EOR
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_rr_5(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    R2((*(core->R))[get_rr_5(opcode)]),
    status(core->status) 
{}

int avr_op_EOR::operator()() 
{

    byte rd = R1; 
    byte rr = R2;

    byte res = rd ^ rr;


    status->V = 0 ;
    status->N = ((res >> 7) & 0x1) ;
    status->S = (status->N ^ status->V) ;
    status->Z = ((res & 0xff) == 0 ) ;

    R1=res;

    return 1;
}

avr_op_ESPM::avr_op_ESPM
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0,0),
    R1((*(core->R))[get_rd_5(opcode)]),
    R2((*(core->R))[get_rr_5(opcode)]),
    status(core->status) ,
    K(get_K_6(opcode))
{}

int avr_op_ESPM::operator()()  //TODO currently not supported
{
    return 0;
}



avr_op_FMUL::avr_op_FMUL
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_3(opcode), get_rr_3(opcode)),
    R0((*(core->R))[0]),
    R1((*(core->R))[1]),
    Rd((*(core->R))[get_rd_3(opcode)]),
    Rr((*(core->R))[get_rr_3(opcode)]),
    status(core->status) ,
    K(get_K_6(opcode))
{}

int avr_op_FMUL::operator()() 
{

    byte rd =  Rd;
    byte rr =  Rr;

    word resp = rd * rr;
    word res = resp << 1;


    status->Z=((res & 0xffff) == 0) ;
    status->C=((resp >> 15) & 0x1) ;


    /* result goes in R1:R0 */
    R0=res&0xff;
    R1=res>>8;

    return 2;
}


avr_op_FMULS::avr_op_FMULS
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_3(opcode), get_rr_3(opcode)),
    R0((*(core->R))[0]),
    R1((*(core->R))[1]),
    Rd((*(core->R))[get_rd_3(opcode)]),
    Rr((*(core->R))[get_rr_3(opcode)]),
    status(core->status) 
{}

int avr_op_FMULS::operator()() 
{

    sbyte rd = Rd; 
    sbyte rr = Rr;

    word resp = rd * rr;
    word res = resp << 1;


    status->Z = ((res & 0xffff) == 0) ;
    status->C = ((resp >> 15) & 0x1) ;

    /* result goes in R1:R0 */
    R0= res& 0xff;
    R1= res>>8;

    return 2;
}


avr_op_FMULSU::avr_op_FMULSU
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_3(opcode), get_rr_3(opcode)),

    R0((*(core->R))[0]),
    R1((*(core->R))[1]),
    Rd((*(core->R))[get_rd_3(opcode)]),
    Rr((*(core->R))[get_rr_3(opcode)]),
    status(core->status) 
{}

int avr_op_FMULSU::operator()() 
{

    sbyte rd = Rd;
    byte rr = Rr;

    word resp = rd * rr;
    word res = resp << 1;


    status->Z=((res & 0xffff) == 0) ;
    status->C=((resp >> 15) & 0x1) ;
    R0= res& 0xff;
    R1= res>>8;

    return 2;
}

avr_op_ICALL::avr_op_ICALL
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0,0),
    Rl((*(core->R))[30]),
    Rh((*(core->R))[31]),
    pc_bytes((c->PC_size) )
{}

int avr_op_ICALL::operator()() 
{

    int pc = core->PC;

    /* Z is R31:R30 */
    int new_pc = (Rh << 8) + Rl;

    avr_core_stack_push( core, pc_bytes, pc+1 );

    core->PC=new_pc-1;

    return pc_bytes+1;
}


avr_op_IJMP::avr_op_IJMP
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0,0),
    Rl((*(core->R))[30]),
    Rh((*(core->R))[31])
{}

int avr_op_IJMP::operator()() 
{

    int new_pc = (Rh << 8) + Rl;
    core->PC=new_pc-1;

    return 2;
}


avr_op_IN::avr_op_IN
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_A_6(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    ioreg((*(core->ioreg))[get_A_6(opcode)])
{}

int avr_op_IN::operator()() 
{

    R1=ioreg;

    return 1;
}

avr_op_INC::avr_op_INC
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    status(core->status) 
{}

int avr_op_INC::operator()() 
{

    byte rd  = R1;
    byte res = rd + 1;

    status->N = ((res >> 7) & 0x1) ;
    status->V = (rd == 0x7f) ;
    status->S = (status->N ^ status->V) ;
    status->Z = ((res & 0xff) == 0) ;

    R1=res;


    return 1;
}

avr_op_JMP::avr_op_JMP
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_k_22(opcode), 0, true),
    K(get_k_22(opcode))
{}

int avr_op_JMP::operator()() 
{

    word *MemPtr=(word*)core->Flash->myMemory;
    word offset=MemPtr[(core->PC)+1];         //this is k!
    offset=(offset>>8)+((offset&0xff)<<8);

    core->PC=K+offset-1;

    return 3;
}

avr_op_LDD_Y::avr_op_LDD_Y
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_q(opcode)),
    Rl((*(core->R))[28]),
    Rh((*(core->R))[29]),
    Rd((*(core->R))[get_rd_5(opcode)]),
    K(get_q(opcode))
{}

int avr_op_LDD_Y::operator()() 
{

    word Y;

    /* Y is R29:R28 */
    Y = ( Rh << 8) + Rl; 
    Rd = (*(core->Sram))[Y+K];

    return 2;
}

avr_op_LDD_Z::avr_op_LDD_Z
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_q(opcode)),
    Rd((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[30]),
    Rh((*(core->R))[31]),
    K(get_q(opcode))
{}

int avr_op_LDD_Z::operator()() 
{

    word Z;

    /* Z is R31:R30 */
    Z = (Rh<< 8) + Rl;

    Rd = (*(core->Sram))[Z+K];

    return 2;
}



avr_op_LDI::avr_op_LDI
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_4(opcode),  get_K_8(opcode)),
    R1((*(core->R))[get_rd_4(opcode)]),
    K(get_K_8(opcode)) {
    }


int avr_op_LDI::operator()() { 
    R1=K;

    return 1;   //used clocks
}


avr_op_LDS::avr_op_LDS
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), 0, true),
    R1((*(core->R))[get_rd_5(opcode)])
{}

int avr_op_LDS::operator()()  //TODO, read sram as reference also! read second word!!!
{

    /* Get data at k in current data segment and put into Rd */
    word *MemPtr=(word*)core->Flash->myMemory;
    word offset=MemPtr[(core->PC)+1];         //this is k!
    offset=(offset>>8)+((offset&0xff)<<8);
    R1=(*(core->Sram))[offset];
    core->PC++; //2 word instr

    return 2;
}

avr_op_LD_X::avr_op_LD_X
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    Rl((*(core->R))[26]),
    Rh((*(core->R))[27]),
    Rd((*(core->R))[get_rd_5(opcode)])
{}

int avr_op_LD_X::operator()() 
{

    word X;


    /* X is R27:R26 */
    X = (Rh << 8) + Rl;
    Rd= (*(core->Sram))[X];

    return 2;
}

avr_op_LD_X_decr::avr_op_LD_X_decr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    Rd((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[26]),
    Rh((*(core->R))[27]),
    status(core->status) ,
    K(get_K_6(opcode))
{}

int avr_op_LD_X_decr::operator()() 
{

    word X;

    //TODO
    //if ( (Rd == 26) || (Rd == 27) )
    //  avr_error( "Results of operation are undefined" );

    /* X is R27:R26 */
    X = (Rh << 8 ) + Rl; 

    /* Perform pre-decrement */
    X -= 1;
    Rd = (*(core->Sram))[X];
    Rl =X&0xff;
    Rh=X>>8;

    return 2;
}



avr_op_LD_X_incr::avr_op_LD_X_incr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    Rd((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[26]),
    Rh((*(core->R))[27])
{}

int avr_op_LD_X_incr::operator()() 
{

    word X;


    //TODO
    //if ( (Rd == 26) || (Rd == 27) )
    //  avr_error( "Results of operation are undefined" );

    /* X is R27:R26 */
    X = (Rh << 8 ) + Rl;

    Rd= (*(core->Sram))[X];

    /* Perform post-increment */
    X += 1;
    Rl=X&0xff;
    Rh=X>>8;

    return 2;
}


avr_op_LD_Y_decr::avr_op_LD_Y_decr
(word opcode, AvrDevice *c):
    DecodedInstruction(c,  get_rd_5(opcode), 0),
    Rd((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[28]),
    Rh((*(core->R))[29])
{}

int avr_op_LD_Y_decr::operator()() 
{

    word Y;


    //TODO
    //if ( (p1 == 28) || (p1 == 29) )
    //  avr_error( "Results of operation are undefined" );

    /* Y is R29:R28 */
    Y = ( Rh << 8) + Rl;

    /* Perform pre-decrement */
    Y -= 1;
    Rd=(*(core->Sram))[Y];
    Rl=Y&0xff;
    Rh=Y>>8;

    return 2;
}


avr_op_LD_Y_incr::avr_op_LD_Y_incr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0 ),
    Rd((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[28]),
    Rh((*(core->R))[29])
{}

int avr_op_LD_Y_incr::operator()() 
{

    word Y;


    //TODO
    //if ( (Rd == 28) || (Rd == 29) )
    //  avr_error( "Results of operation are undefined" );

    /* Y is R29:R28 */
    Y = (Rh << 8) + Rl;
    Rd=(*(core->Sram))[Y];
    Y += 1;
    Rl=Y&0xff;
    Rh=Y>>8;

    return 2;
}

avr_op_LD_Z_incr::avr_op_LD_Z_incr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    Rd((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[30]),
    Rh((*(core->R))[31])
{}

int avr_op_LD_Z_incr::operator()() 
{

    word Z;


    //TODO
    ////if ( (Rd == 30) || (Rd == 31) )
    //  avr_error( "Results of operation are undefined" );

    /* Z is R31:R30 */
    Z = (Rh<<8) + Rl;

    /* Perform post-increment */
    Rd=(*(core->Sram))[Z];
    Z += 1;
    Rl = Z&0xff;
    Rh = Z>>8;

    return 2;
}

avr_op_LD_Z_decr::avr_op_LD_Z_decr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0 ),
    Rd((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[30]),
    Rh((*(core->R))[31])
{}

int avr_op_LD_Z_decr::operator()() 
{

    word Z;

    //TODO
    //if ( (Rd == 30) || (Rd == 31) )
    //  avr_error( "Results of operation are undefined" );

    /* Z is R31:R30 */
    Z = (Rh << 8 ) + Rl ;


    /* Perform pre-decrement */
    Z -= 1;
    Rd=(*(core->Sram))[Z];
    Rl=Z&0xff;
    Rh=Z>>8;

    return 2;
}

avr_op_LPM_Z::avr_op_LPM_Z
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    Rd((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[30]),
    Rh((*(core->R))[31])
{}

int  avr_op_LPM_Z::operator()() 
{

    word Z;
    word data;


    /* Z is R31:R30 */
    Z = (Rh << 8 ) + Rl;

    Z^=0x0001;
    data = core->Flash->myMemory[Z];

    Rd=data;

    return 3;
}

avr_op_LPM::avr_op_LPM
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0,0),
    R0((*(core->R))[0]),
    Rl((*(core->R))[30]),
    Rh((*(core->R))[31])
{}

int avr_op_LPM::operator()() 
{

    word Z;
    word data;
    Z = (Rh << 8 ) + Rl; 

    Z^=0x0001;
    data = core->Flash->myMemory[Z];

    R0=data;
    return 3;
}


avr_op_LPM_Z_incr::avr_op_LPM_Z_incr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), 0),
    Rd((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[30]),
    Rh((*(core->R))[31])
{}

int avr_op_LPM_Z_incr::operator()() 
{

    word Z, flashAddr;
    word data;


    Z = ( Rh << 8 ) + Rl ;
    flashAddr= Z^ 0x0001;
    data = core->Flash->myMemory[flashAddr];

    Rd=data;
    Z += 1;
    Rl=Z&0xff;
    Rh=Z>>8;

    return 3;
}

avr_op_LSR::avr_op_LSR
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    Rd((*(core->R))[get_rd_5(opcode)]),
    status(core->status) 
{}

int avr_op_LSR::operator()() 
{


    byte rd = Rd; 

    byte res = (rd >> 1) & 0x7f;


    status->C = (rd & 0x1) ;
    status->N = (0) ;
    status->V = (status->N ^ status->C) ;
    status->S = (status->N ^ status->V) ;
    status->Z = ((res & 0xff) == 0) ;

    Rd=res;

    return 1;
}


avr_op_MOV::avr_op_MOV
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_rr_5(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    R2((*(core->R))[get_rr_5(opcode)])
{}

int avr_op_MOV::operator()() 
{
    R1=R2;
    return 1;
}



avr_op_MOVW::avr_op_MOVW
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_4(opcode), get_rr_4(opcode)),
    Rdl((*(core->R))[(get_rd_4(opcode)-16)<<1]),
    Rdh((*(core->R))[((get_rd_4(opcode)-16)<<1)+1]),
    Rsl((*(core->R))[(get_rr_4(opcode)-16)<<1]),
    Rsh((*(core->R))[((get_rr_4(opcode)-16)<<1) +1]),
    status(core->status) ,
    K(get_K_6(opcode))
{}

int avr_op_MOVW::operator()() 
{
    Rdl=Rsl;
    Rdh=Rsh;

    return 1;
}


avr_op_MUL::avr_op_MUL
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_rr_5(opcode)),
    R0((*(core->R))[0]),
    R1((*(core->R))[1]),
    Rd((*(core->R))[get_rd_5(opcode)]),
    Rr((*(core->R))[get_rr_5(opcode)]),
    status(core->status) 
{}

int avr_op_MUL::operator()() 
{

    byte rd = Rd;
    byte rr = Rr;

    word res = rd * rr;

    core->status->Z=((res & 0xffff) == 0) ;
    core->status->C=((res >> 15) & 0x1) ;


    /* result goes in R1:R0 */
    R0=res&0xff;
    R1=res>>8;

    return 2;
}


avr_op_MULS::avr_op_MULS
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_4(opcode), get_rr_4(opcode)),
    Rd((*(core->R))[get_rd_4(opcode)]),
    Rr((*(core->R))[get_rr_4(opcode)]),
    R0((*(core->R))[0]),
    R1((*(core->R))[1]),
    status(core->status) 
{}

int avr_op_MULS::operator()() 
{

    sbyte rd = (sbyte)Rd;
    sbyte rr = (sbyte)Rr;
    sword res = rd * rr;


    status->Z=((res & 0xffff) == 0) ;
    status->C=((res >> 15) & 0x1) ;

    /* result goes in R1:R0 */
    R0=res&0xff;
    R1=res>>8;

    return 2;
}

avr_op_MULSU::avr_op_MULSU
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_3(opcode), get_rr_3(opcode)),
    Rd((*(core->R))[get_rd_3(opcode)]),
    Rr((*(core->R))[get_rr_3(opcode)]),
    R0((*(core->R))[0]),
    R1((*(core->R))[1]),
    status(core->status) 
{}

int avr_op_MULSU::operator()() 
{

    sbyte rd = (sbyte)Rd;
    byte rr = Rr;

    sword res = rd * rr;


    status->Z=((res & 0xffff) == 0) ;
    status->C=((res >> 15) & 0x1) ;


    /* result goes in R1:R0 */
    R0=res&0xff;
    R1=res>>8;

    return 2;
}


avr_op_NEG::avr_op_NEG
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    Rd((*(core->R))[get_rd_5(opcode)]),
    status(core->status) 
{}

int avr_op_NEG::operator()() 
{

    byte rd  = Rd;
    byte res = (0x0 - rd) & 0xff;

    core->status->H = (((res >> 3) | (rd >> 3)) & 0x1) ;
    core->status->V = (res == 0x80) ;
    core->status->N = ((res >> 7) & 0x1) ;
    core->status->S = (core->status->N ^ core->status->V) ;
    core->status->Z = (res == 0x0) ;
    core->status->C = (res != 0x0) ;

    Rd =res;

    return 1;
}


avr_op_NOP::avr_op_NOP
(word opcode, AvrDevice *c):
    DecodedInstruction(c,0,0)
{}

int avr_op_NOP::operator()() 
{

    return 1;
}

avr_op_OR::avr_op_OR
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_rr_5(opcode)),
    Rd((*(core->R))[get_rd_5(opcode)]),
    Rr((*(core->R))[get_rr_5(opcode)]),
    status(core->status) 
{}

int avr_op_OR::operator()() 
{

    byte res = Rd | Rr;

    core->status->V = (0) ;
    core->status->N = ((res >> 7) & 0x1) ;
    core->status->S = (core->status->N ^ core->status->V) ;
    core->status->Z = (res == 0x0) ;

    Rd = res;

    return 1;
}


avr_op_ORI::avr_op_ORI
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_4(opcode), get_K_8(opcode)),
    R1((*(core->R))[get_rd_4(opcode)]),
    status(core->status) ,
    K(get_K_8(opcode))
{}

int avr_op_ORI::operator()() 
{


    byte res = R1 | K;

    status->V = (0) ;
    status->N = ((res >> 7) & 0x1) ;
    status->S = (status->N ^ status->V) ;
    status->Z = (res == 0x0) ;

    R1=res;

    return 1;
}



avr_op_OUT::avr_op_OUT
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_A_6(opcode), get_rd_5(opcode)),
    ioreg((*(core->ioreg))[get_A_6(opcode)]),
    R1((*(core->R))[get_rd_5(opcode)])
{}

int avr_op_OUT::operator()() 
{

    ioreg=R1;

    return 1;
}


avr_op_POP::avr_op_POP
(word opcode, AvrDevice *c):
    DecodedInstruction(c,  get_rd_5(opcode),0 ),
    R1((*(core->R))[get_rd_5(opcode)])
{}

int avr_op_POP::operator()() 
{

    R1= avr_core_stack_pop(core, 1);

    return 2;
}

avr_op_PUSH::avr_op_PUSH
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), 0),
    R1((*(core->R))[get_rd_5(opcode)])
{}

int avr_op_PUSH::operator()() 
{

    avr_core_stack_push( core, 1, R1 );

    return 2;
}

avr_op_RCALL::avr_op_RCALL
(word opcode, AvrDevice *c):
    DecodedInstruction(c, n_bit_unsigned_to_signed( get_k_12(opcode), 12 ),0),
    K(n_bit_unsigned_to_signed( get_k_12(opcode), 12 ))
{}

int avr_op_RCALL::operator()() 
{
    int pc       = core->PC;
    int pc_bytes = core->PC_size;

    avr_core_stack_push( core, pc_bytes, pc+1 );
    core->PC+=K;
    core->PC&=(core->Flash->GetSize()-1)>>1;

    return pc_bytes+1;
}

avr_op_RET::avr_op_RET
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0,0)
{}

int avr_op_RET::operator()() 
{
    int pc_bytes = core->PC_size;
    int pc       = avr_core_stack_pop( core, pc_bytes );

    core->PC=pc-1;

    return pc_bytes+2;
}

avr_op_RETI::avr_op_RETI
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0,0),
    status(core->status) 
{}

int avr_op_RETI::operator()() 
{


    int pc_bytes = core->PC_size;
    int pc = avr_core_stack_pop( core, pc_bytes );

    core->PC=pc-1;
    status->I=1;

    return pc_bytes+2;
}

avr_op_RJMP::avr_op_RJMP
(word opcode, AvrDevice *c):
    DecodedInstruction(c, n_bit_unsigned_to_signed( get_k_12(opcode), 12 ),0),
    K(n_bit_unsigned_to_signed( get_k_12(opcode), 12 ))
{}

int avr_op_RJMP::operator()() 
{

    core->PC+=K;
    core->PC&=(core->Flash->GetSize()-1)>>1;

    return 2;
}



avr_op_ROR::avr_op_ROR
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    status(core->status) 
{}

int avr_op_ROR::operator()() 
{


    byte rd = R1;

    byte res = (rd >> 1) | ((( status->C ) << 7) & 0x80);


    status->C = (rd & 0x1) ;
    status->N = ((res >> 7) & 0x1) ;
    status->V = (status->N ^ status->C) ;
    status->S = (status->N ^ status->V) ;
    status->Z = (res == 0) ;

    R1=res;

    return 1;
}


avr_op_SBC::avr_op_SBC
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_rr_5(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    R2((*(core->R))[get_rr_5(opcode)]),
    status(core->status) 
{}

int avr_op_SBC::operator()() 
{

    byte rd = R1;
    byte rr = R2;

    byte res = rd - rr - ( status->C );


    status->H = (get_sub_carry( res, rd, rr, 3 )) ;
    status->V = (get_sub_overflow( res, rd, rr )) ;
    status->N = ((res >> 7) & 0x1) ;
    status->S = (status->N ^ status->V) ;
    status->C = (get_sub_carry( res, rd, rr, 7 )) ;

    if ((res & 0xff) != 0)
        status->Z = (0) ;

    R1=res;

    return 1;
}

avr_op_SBCI::avr_op_SBCI
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_4(opcode), get_K_8(opcode)),
    R1((*(core->R))[get_rd_4(opcode)]),
    status(core->status) ,
    K(get_K_8(opcode))
{}

int avr_op_SBCI::operator()() 
{

    byte rd = R1;

    byte res = rd - K - ( status->C );


    status->H = (get_sub_carry( res, rd, K, 3 )) ;
    status->V = (get_sub_overflow( res, rd, K )) ;
    status->N = ((res >> 7) & 0x1) ;
    status->S = (status->N ^ status->V) ;
    status->C = (get_sub_carry( res, rd, K, 7 )) ;

    if ((res & 0xff) != 0)
        status->Z = 0 ;

    R1=res;

    return 1;
}


avr_op_SBI::avr_op_SBI
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_A_5(opcode), get_reg_bit(opcode)),
    ioreg((*(core->ioreg))[get_A_5(opcode)]),
    K(1<<get_reg_bit(opcode))
{}

int avr_op_SBI::operator()() 
{

    ioreg=ioreg|  K ;

    return 2;
}


avr_op_SBIC::avr_op_SBIC
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_A_5(opcode), get_reg_bit(opcode)),
    ioreg((*(core->ioreg))[get_A_5(opcode)]),
    K(1<<get_reg_bit(opcode))
{}

int avr_op_SBIC::operator()() 
{

    int skip;

    int clks;


    if ( core->Flash->DecodedMem[core->PC+1]->IsInstruction2Words() ) skip = 3;
    else skip = 2;

    if (( ioreg&K  ) == 0)
    {
        core->PC+=skip-1;
        clks=skip;
    }
    else
    {
        clks=1;
    }

    return clks;
}

avr_op_SBIS::avr_op_SBIS
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_A_5(opcode), get_reg_bit(opcode)),
    ioreg((*(core->ioreg))[get_A_5(opcode)]),
    K(1<<get_reg_bit(opcode))
{}

int avr_op_SBIS::operator()() 
{

    int skip;

    int clks;

    if ( core->Flash->DecodedMem[core->PC+1]->IsInstruction2Words() )
        skip = 3;
    else
        skip = 2;

    if ( (ioreg&K ) != 0)
    {
        core->PC+=skip-1;
        clks=skip;
    }
    else
    {
        clks=1;
    }

    return clks;
}


avr_op_SBIW::avr_op_SBIW
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_2(opcode), get_K_6(opcode)),
    Rl((*(core->R))[get_rd_2(opcode)]),
    Rh((*(core->R))[get_rd_2(opcode)+1]),
    status(core->status) ,
    K(get_K_6(opcode))
{}

int avr_op_SBIW::operator()() 
{

    byte rdl = Rl;
    byte rdh = Rh;

    word rd = (rdh << 8) + rdl;

    word res = rd - K;


    status->V = ((rdh >> 7 & 0x1) & ~(res >> 15 & 0x1)) ;
    status->N = ((res >> 15) & 0x1) ;
    status->S = (status->N ^ status->V) ;
    status->Z = ((res & 0xffff) == 0) ;
    status->C = ((res >> 15 & 0x1) & ~(rdh >> 7 & 0x1)) ;

    Rl=res&0xff;
    Rh=res>>8;

    return 2;
}


avr_op_SBRC::avr_op_SBRC
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_reg_bit(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    K(1<<get_reg_bit(opcode))
{}

int avr_op_SBRC::operator()() 
{
    int skip;
    int clks;

    if ( core->Flash->DecodedMem[(core->PC)+1]->IsInstruction2Words() )
        skip = 3;
    else
        skip = 2;

    if ((R1 & K ) == 0)
    {
        core->PC+=skip-1;
        clks=skip;
    }
    else
    {
        clks=1;
    }

    return clks;
}

avr_op_SBRS::avr_op_SBRS
(word opcode, AvrDevice *c):
    DecodedInstruction(c,  get_rd_5(opcode), get_reg_bit(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    K(1<<get_reg_bit(opcode))
{}

int avr_op_SBRS::operator()() 
{

    int skip;
    int clks;


    if ( core->Flash->DecodedMem[core->PC+1]->IsInstruction2Words() )
        skip = 3;
    else
        skip = 2;

    if ((R1 & K ) != 0) 
    {
        core->PC+=skip-1;
        clks=skip;
    }
    else
    {
        clks=1;
    }

    return clks;
}

avr_op_SLEEP::avr_op_SLEEP
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0,0)
{}

int avr_op_SLEEP::operator()() 
{


#ifdef LLL  //TODO
    MCUCR *mcucr = (MCUCR *)avr_core_get_vdev_by_name( core, "MCUCR" );

    if (mcucr == NULL)
        avr_error( "MCUCR register not installed" );

    /* See if sleep mode is enabled */
    if ( mcucr_get_bit(mcucr, bit_SE) )
    {
        if ( mcucr_get_bit(mcucr, bit_SM) == 0 )
        {
            /* Idle Mode */
            avr_core_set_sleep_mode( core, SLEEP_MODE_IDLE);
        }
        else
        {
            /* Power Down Mode */
            avr_core_set_sleep_mode( core, SLEEP_MODE_PWR_DOWN);
        }
    }

#endif

    return 0;
}

avr_op_SPM::avr_op_SPM
(word opcode, AvrDevice *c):
    DecodedInstruction(c,0,0),
    R0((*(core->R))[0]),
    R1((*(core->R))[1])
{}

int avr_op_SPM::operator()() 
{


    return 0;
}

avr_op_STD_Y::avr_op_STD_Y
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_q(opcode), get_rd_5(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[28]),
    Rh((*(core->R))[29]),
    K(get_q(opcode))
{}

int avr_op_STD_Y::operator()() 
{

    //int Rd = get_rd_5(opcode);
    //int q  = get_q(opcode);
    //int q=p1;

    /* Y is R29:R28 */
    int Y = ( Rh << 8 )  + Rl;

    (*(core->Sram))[Y+K]= R1 ;

    //avr_core_PC_incr( core, 1 );
    //avr_core_inst_CKS_set( core, 2 );

    return 2;
}


avr_op_STD_Z::avr_op_STD_Z
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_q(opcode), get_rd_5(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[30]),
    Rh((*(core->R))[31]),
    K(get_q(opcode))
{}

int avr_op_STD_Z::operator()() 
{

    int Z;

    //int Rd = get_rd_5(opcode);

    /* Z is R31:R30 */
    Z = (Rh << 8 ) + Rl;

    //avr_core_mem_write( core, Z+q, (*(core->R))[ Rd] );
    (*(core->Sram))[Z+K]=R1;

    //avr_core_PC_incr( core, 1 );
    //avr_core_inst_CKS_set( core, 2 );

    return 2;
}


avr_op_STS::avr_op_STS
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0, true),
    R1((*(core->R))[get_rd_5(opcode)])
{}

int avr_op_STS::operator()() 
{

    //int Rd = get_rd_5(opcode);

    /* Get data at k in current data segment and put into Rd */
    //int k_pc = avr_core_PC_get(core) + 1;
    //int k    = core->Flash->myMemory[ k_pc];
    word *MemPtr=(word*)core->Flash->myMemory;
    word k=MemPtr[(core->PC)+1];         //this is k!
    k=(k>>8)+((k&0xff)<<8);


    //avr_core_mem_write( core, k, (*(core->R))[ Rd] );

    (*(core->Sram))[k]=R1;
    core->PC++;

    //avr_core_PC_incr( core, 2 );
    //avr_core_inst_CKS_set( core, 2 );

    return 2;
}

avr_op_ST_X::avr_op_ST_X
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[26]),
    Rh((*(core->R))[27])
{}

int avr_op_ST_X::operator()() 
{

    word X;

    //int Rd = get_rd_5(opcode);

    /* X is R27:R26 */
    //X = ((*(core->R))[ 27] << 8) + (*(core->R))[ 26];
    X = (Rh << 8 ) + Rl;

    //avr_core_mem_write( core, X, (*(core->R))[ Rd] );
    (*(core->Sram))[X]=R1;


    //avr_core_PC_incr( core, 1 );
    //avr_core_inst_CKS_set( core, 2 );

    return 2;
}


avr_op_ST_X_decr::avr_op_ST_X_decr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[26]),
    Rh((*(core->R))[27])
{}

int avr_op_ST_X_decr::operator()() 
{

    word X;


    //TODO
    //if ( (Rd == 26) || (Rd == 27) )
    //  avr_error( "Results of operation are undefined: 0x%04x", opcode );

    /* X is R27:R26 */
    X = (Rh << 8 ) + Rl ;

    /* Perform pre-decrement */
    X -= 1;
    Rl=X & 0xff;
    Rh=X >> 8;

    (*(core->Sram))[X]=R1;

    return 2;
}

avr_op_ST_X_incr::avr_op_ST_X_incr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[26]),
    Rh((*(core->R))[27])
{}

int avr_op_ST_X_incr::operator()() 
{

    word X;

    /* X is R27:R26 */
    X = ( Rh << 8 ) + Rl;

    (*(core->Sram))[X]=R1;

    /* Perform post-increment */
    X += 1;
    Rl=X&0xff;
    Rh=X>>8;

    return 2;
}

avr_op_ST_Y_decr::avr_op_ST_Y_decr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[28]),
    Rh((*(core->R))[29])
{}

int avr_op_ST_Y_decr::operator()() 
{

    //TODO
    //if ( (Rd == 28) || (Rd == 29) )
    //  avr_error( "Results of operation are undefined: 0x%04x", opcode );

    /* Y is R29:R28 */
    unsigned int Y = (Rh << 8 ) + Rl;

    /* Perform pre-decrement */
    Y -= 1;
    Rl=Y&0xff;
    Rh=Y>>8;

    (*(core->Sram))[Y]=R1;


    return 2;
}

avr_op_ST_Y_incr::avr_op_ST_Y_incr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[28]),
    Rh((*(core->R))[29])
{}

int avr_op_ST_Y_incr::operator()() 
{

    word Y;

    //TODO
    //if ( (Rd == 28) || (Rd == 29) )
    //  avr_error( "Results of operation are undefined: 0x%04x", opcode );

    /* Y is R29:R28 */
    Y = (Rh << 8 ) + Rl;

    (*(core->Sram))[Y]=R1;

    /* Perform post-increment */
    Y += 1;
    Rl=Y&0xff;
    Rh=Y>>8;

    return 2;
}

avr_op_ST_Z_decr::avr_op_ST_Z_decr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0 ),
    R1((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[30]),
    Rh((*(core->R))[31])
{}

int avr_op_ST_Z_decr::operator()() 
{

    word Z;


    //TODO
    //if ( (Rd == 30) || (Rd == 31) )
    //  avr_error( "Results of operation are undefined: 0x%04x", opcode );

    /* Z is R31:R30 */
    Z = ( Rh << 8) + Rl;

    /* Perform pre-decrement */
    Z -= 1;
    Rl=Z&0xff;
    Rh=Z>>8;

    (*(core->Sram))[Z]=R1;

    return 2;
}


avr_op_ST_Z_incr::avr_op_ST_Z_incr
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)]),
    Rl((*(core->R))[30]),
    Rh((*(core->R))[31])
{}

int avr_op_ST_Z_incr::operator()() 
{

    word Z;


    //TODO
    //if ( (Rd == 30) || (Rd == 31) )
    //  avr_error( "Results of operation are undefined: 0x%04x", opcode );

    /* Z is R31:R30 */
    Z = ( Rh << 8 ) + Rl;

    (*(core->Sram))[Z]=R1;

    /* Perform post-increment */
    Z += 1;
    Rl=Z&0xff;
    Rh=Z>>8;

    return 2;
}

avr_op_SUB::avr_op_SUB
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode), get_rr_5(opcode)),
    R1((*(core->R))[get_rd_5(opcode)]),
    R2((*(core->R))[get_rr_5(opcode)]),
    status(core->status) 
{}

int avr_op_SUB::operator()() 
{

    byte rd = R1;
    byte rr = R2;

    byte res = rd - rr;

    status->H = (get_sub_carry( res, rd, rr, 3 )) ;
    status->V = (get_sub_overflow( res, rd, rr )) ;
    status->N = ((res >> 7) & 0x1) ;
    status->S = (status->N ^ status->V) ;
    status->Z = ((res & 0xff) == 0) ;
    status->C = (get_sub_carry( res, rd, rr, 7 )) ;
    R1=res;

    return 1;
}



avr_op_SUBI::avr_op_SUBI
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_4(opcode), get_K_8(opcode)),
    R1((*(core->R))[get_rd_4(opcode)]),
    status(core->status) ,
    K(get_K_8(opcode))
{}

int avr_op_SUBI::operator()() 
{

    byte rd = R1;

    byte res = rd - K;


    status->H = (get_sub_carry( res, rd, K, 3 )) ;
    status->V = (get_sub_overflow( res, rd, K )) ;
    status->N = ((res >> 7) & 0x1) ;
    status->S = (status->N ^ status->V) ;
    status->Z = ((res & 0xff) == 0) ;
    status->C = (get_sub_carry( res, rd, K, 7 )) ;

    R1= res;

    return 1;
}

avr_op_SWAP::avr_op_SWAP
(word opcode, AvrDevice *c):
    DecodedInstruction(c, get_rd_5(opcode),0),
    R1((*(core->R))[get_rd_5(opcode)])
{}

int avr_op_SWAP::operator()() 
{


    byte rd = R1;

    R1= ((rd << 4) & 0xf0) | ((rd >> 4) & 0x0f) ;


    return 1;
}

avr_op_WDR::avr_op_WDR
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0,0)
{}

int avr_op_WDR::operator()() 
{

    core->wado->Wdr();

    return 1;
}

avr_op_ILLEGAL::avr_op_ILLEGAL
(word opcode, AvrDevice *c):
    DecodedInstruction(c, 0,0)
{}

int avr_op_ILLEGAL::operator()() 
{
    std::cerr<< "Simulation terminated! IllegalInstruction executed!" << std::endl;
    exit(0);
    return 1;
}

void avr_core_stack_push( AvrDevice *core, int cnt, long val) {
    for (int tt=0; tt<cnt; tt++) {
        core->stack->Push(val&0xff);
        val>>=8;
    }
}

dword avr_core_stack_pop( AvrDevice *core, int cnt) {
    dword val=0;
    for (int tt=0; tt<cnt; tt++) {
        val=val<<8;
        val+=core->stack->Pop();
    }
    return val;
}


static int get_add_carry( byte res, byte rd, byte rr, int b )
{
    byte resb = res >> b & 0x1;
    byte rdb  = rd  >> b & 0x1;
    byte rrb  = rr  >> b & 0x1;
    return (rdb & rrb) | (rrb & ~resb) | (~resb & rdb);
}

static int get_add_overflow( byte res, byte rd, byte rr )
{
    byte res7 = res >> 7 & 0x1;
    byte rd7  = rd  >> 7 & 0x1;
    byte rr7  = rr  >> 7 & 0x1;
    return (rd7 & rr7 & ~res7) | (~rd7 & ~rr7 & res7);
}

static int get_sub_carry( byte res, byte rd, byte rr, int b )
{
    byte resb = res >> b & 0x1;
    byte rdb  = rd  >> b & 0x1;
    byte rrb  = rr  >> b & 0x1;
    return (~rdb & rrb) | (rrb & resb) | (resb & ~rdb);
}

static int get_sub_overflow( byte res, byte rd, byte rr )
{
    byte res7 = res >> 7 & 0x1;
    byte rd7  = rd  >> 7 & 0x1;
    byte rr7  = rr  >> 7 & 0x1;
    return (rd7 & ~rr7 & ~res7) | (~rd7 & rr7 & res7);
}

static int get_compare_carry( byte res, byte rd, byte rr, int b )
{
    byte resb = res >> b & 0x1;
    byte rdb  = rd  >> b & 0x1;
    byte rrb  = rr  >> b & 0x1;
    return (~rdb & rrb) | (rrb & resb) | (resb & ~rdb);
}

static int get_compare_overflow( byte res, byte rd, byte rr )
{
    byte res7 = res >> 7 & 0x1;
    byte rd7  = rd  >> 7 & 0x1;
    byte rr7  = rr  >> 7 & 0x1;
    /* The atmel data sheet says the second term is ~rd7 for CP
     * but that doesn't make any sense. You be the judge. */
    return (rd7 & ~rr7 & ~res7) | (~rd7 & rr7 & res7);
}
static int n_bit_unsigned_to_signed( unsigned int val, int n ) 
{
    /* Convert n-bit unsigned value to a signed value. */
    unsigned int mask;

    if ( (val & (1 << (n-1))) == 0)
        return (int)val;

    /* manually calculate two's complement */
    mask = (1 << n) - 1; 
    return -1 * ((~val & mask) + 1);
}


static int get_rd_2( word opcode )
{
    int reg = ((opcode & mask_Rd_2) >> 4) & 0x3;
    return (reg * 2) + 24;
}

static int get_rd_3( word opcode )
{
    int reg = opcode & mask_Rd_3;
    return ((reg >> 4) & 0x7) + 16;
}

static int get_rd_4( word opcode )
{
    int reg = opcode & mask_Rd_4;
    return ((reg >> 4) & 0xf) + 16;
}

static int get_rd_5( word opcode )
{
    int reg = opcode & mask_Rd_5;
    return ((reg >> 4) & 0x1f);
}

static int get_rr_3( word opcode )
{
    return (opcode & mask_Rr_3) + 16;
}

static int get_rr_4( word opcode )
{
    return (opcode & mask_Rr_4) + 16;
}

static int get_rr_5( word opcode )
{
    int reg = opcode & mask_Rr_5;
    return (reg & 0xf) + ((reg >> 5) & 0x10);
}

static byte get_K_8( word opcode )
{
    int K = opcode & mask_K_8;
    return ((K >> 4) & 0xf0) + (K & 0xf);
}

static byte get_K_6( word opcode )
{
    int K = opcode & mask_K_6;
    return ((K >> 2) & 0x0030) + (K & 0xf);
}

static int get_k_7( word opcode )
{
    return (((opcode & mask_k_7) >> 3) & 0x7f);
}

static int get_k_12( word opcode )
{
    return (opcode & mask_k_12);
}

static int get_k_22( word opcode )
{
    /* Masks only the upper 6 bits of the address, the other 16 bits
     * are in PC + 1. */
    int k = opcode & mask_k_22;
    return ((k >> 3) & 0x003e) + (k & 0x1);
}

static int get_reg_bit( word opcode )
{
    return opcode & mask_reg_bit;
}

static int get_sreg_bit( word opcode )
{
    return (opcode & mask_sreg_bit) >> 4;
}

static int get_q( word opcode )
{
    /* 00q0 qq00 0000 0qqq : Yuck! */
    int q = opcode & mask_q_displ;
    int qq = ( ((q >> 1) & 0x1000) + (q & 0x0c00) ) >> 7;
    return (qq & 0x0038) + (q & 0x7);
}

static int get_A_5( word opcode )
{
    return (opcode & mask_A_5) >> 3;
}

static int get_A_6( word opcode )
{
    int A = opcode & mask_A_6;
    return ((A >> 5) & 0x0030) + (A & 0xf);
}




DecodedInstruction* lookup_opcode( word opcode, AvrDevice *core )
{
    int decode;

    switch (opcode) {
        /* opcodes with no operands */
        case 0x9519: return new  avr_op_EICALL(opcode, core);              /* 1001 0101 0001 1001 | EICALL */
        case 0x9419: return new  avr_op_EIJMP(opcode, core);               /* 1001 0100 0001 1001 | EIJMP */
        case 0x95D8: return new  avr_op_ELPM(opcode, core);                /* 1001 0101 1101 1000 | ELPM */
        case 0x95F8: return new  avr_op_ESPM(opcode, core);                /* 1001 0101 1111 1000 | ESPM */
        case 0x9509: return new  avr_op_ICALL(opcode, core);               /* 1001 0101 0000 1001 | ICALL */
        case 0x9409: return new  avr_op_IJMP(opcode, core);                /* 1001 0100 0000 1001 | IJMP */
        case 0x95C8: return new  avr_op_LPM(opcode, core);                 /* 1001 0101 1100 1000 | LPM */
        case 0x0000: return new  avr_op_NOP(opcode, core);                 /* 0000 0000 0000 0000 | NOP */
        case 0x9508: return new  avr_op_RET(opcode, core);                 /* 1001 0101 0000 1000 | RET */
        case 0x9518: return new  avr_op_RETI(opcode, core);                /* 1001 0101 0001 1000 | RETI */
        case 0x9588: return new  avr_op_SLEEP(opcode, core);               /* 1001 0101 1000 1000 | SLEEP */
        case 0x95E8: return new  avr_op_SPM(opcode, core);                 /* 1001 0101 1110 1000 | SPM */
        case 0x95A8: return new  avr_op_WDR(opcode, core);                 /* 1001 0101 1010 1000 | WDR */
        default:
                     {
                         /* opcodes with two 5-bit register (Rd and Rr) operands */
                         decode = opcode & ~(mask_Rd_5 | mask_Rr_5);
                         switch ( decode ) {
                             case 0x1C00: return new  avr_op_ADC(opcode, core);         /* 0001 11rd dddd rrrr | ADC or ROL */
                             case 0x0C00: return new  avr_op_ADD(opcode, core);         /* 0000 11rd dddd rrrr | ADD or LSL */
                             case 0x2000: return new  avr_op_AND(opcode, core);         /* 0010 00rd dddd rrrr | AND or TST */
                             case 0x1400: return new  avr_op_CP(opcode, core);          /* 0001 01rd dddd rrrr | CP */
                             case 0x0400: return new  avr_op_CPC(opcode, core);         /* 0000 01rd dddd rrrr | CPC */
                             case 0x1000: return new  avr_op_CPSE(opcode, core);        /* 0001 00rd dddd rrrr | CPSE */
                             case 0x2400: return new  avr_op_EOR(opcode, core);         /* 0010 01rd dddd rrrr | EOR or CLR */
                             case 0x2C00: return new  avr_op_MOV(opcode, core);         /* 0010 11rd dddd rrrr | MOV */
                             case 0x9C00: return new  avr_op_MUL(opcode, core);         /* 1001 11rd dddd rrrr | MUL */
                             case 0x2800: return new  avr_op_OR(opcode, core);          /* 0010 10rd dddd rrrr | OR */
                             case 0x0800: return new  avr_op_SBC(opcode, core);         /* 0000 10rd dddd rrrr | SBC */
                             case 0x1800: return new  avr_op_SUB(opcode, core);         /* 0001 10rd dddd rrrr | SUB */
                         }

                         /* opcode with a single register (Rd) as operand */
                         decode = opcode & ~(mask_Rd_5);
                         switch (decode) {
                             case 0x9405: return new  avr_op_ASR(opcode, core);         /* 1001 010d dddd 0101 | ASR */
                             case 0x9400: return new  avr_op_COM(opcode, core);         /* 1001 010d dddd 0000 | COM */
                             case 0x940A: return new  avr_op_DEC(opcode, core);         /* 1001 010d dddd 1010 | DEC */
                             case 0x9006: return new  avr_op_ELPM_Z(opcode, core);      /* 1001 000d dddd 0110 | ELPM */
                             case 0x9007: return new  avr_op_ELPM_Z_incr(opcode, core); /* 1001 000d dddd 0111 | ELPM */
                             case 0x9403: return new  avr_op_INC(opcode, core);         /* 1001 010d dddd 0011 | INC */
                             case 0x9000: return new  avr_op_LDS(opcode, core);         /* 1001 000d dddd 0000 | LDS */
                             case 0x900C: return new  avr_op_LD_X(opcode, core);        /* 1001 000d dddd 1100 | LD */
                             case 0x900E: return new  avr_op_LD_X_decr(opcode, core);   /* 1001 000d dddd 1110 | LD */
                             case 0x900D: return new  avr_op_LD_X_incr(opcode, core);   /* 1001 000d dddd 1101 | LD */
                             case 0x900A: return new  avr_op_LD_Y_decr(opcode, core);   /* 1001 000d dddd 1010 | LD */
                             case 0x9009: return new  avr_op_LD_Y_incr(opcode, core);   /* 1001 000d dddd 1001 | LD */
                             case 0x9002: return new  avr_op_LD_Z_decr(opcode, core);   /* 1001 000d dddd 0010 | LD */
                             case 0x9001: return new  avr_op_LD_Z_incr(opcode, core);   /* 1001 000d dddd 0001 | LD */
                             case 0x9004: return new  avr_op_LPM_Z(opcode, core);       /* 1001 000d dddd 0100 | LPM */
                             case 0x9005: return new  avr_op_LPM_Z_incr(opcode, core);  /* 1001 000d dddd 0101 | LPM */
                             case 0x9406: return new  avr_op_LSR(opcode, core);         /* 1001 010d dddd 0110 | LSR */
                             case 0x9401: return new  avr_op_NEG(opcode, core);         /* 1001 010d dddd 0001 | NEG */
                             case 0x900F: return new  avr_op_POP(opcode, core);         /* 1001 000d dddd 1111 | POP */
                             case 0x920F: return new  avr_op_PUSH(opcode, core);        /* 1001 001d dddd 1111 | PUSH */
                             case 0x9407: return new  avr_op_ROR(opcode, core);         /* 1001 010d dddd 0111 | ROR */
                             case 0x9200: return new  avr_op_STS(opcode, core);         /* 1001 001d dddd 0000 | STS */
                             case 0x920C: return new  avr_op_ST_X(opcode, core);        /* 1001 001d dddd 1100 | ST */
                             case 0x920E: return new  avr_op_ST_X_decr(opcode, core);   /* 1001 001d dddd 1110 | ST */
                             case 0x920D: return new  avr_op_ST_X_incr(opcode, core);   /* 1001 001d dddd 1101 | ST */
                             case 0x920A: return new  avr_op_ST_Y_decr(opcode, core);   /* 1001 001d dddd 1010 | ST */
                             case 0x9209: return new  avr_op_ST_Y_incr(opcode, core);   /* 1001 001d dddd 1001 | ST */
                             case 0x9202: return new  avr_op_ST_Z_decr(opcode, core);   /* 1001 001d dddd 0010 | ST */
                             case 0x9201: return new  avr_op_ST_Z_incr(opcode, core);   /* 1001 001d dddd 0001 | ST */
                             case 0x9402: return new  avr_op_SWAP(opcode, core);        /* 1001 010d dddd 0010 | SWAP */
                         }

                         /* opcodes with a register (Rd) and a constant data (K) as operands */
                         decode = opcode & ~(mask_Rd_4 | mask_K_8);
                         switch ( decode ) {
                             case 0x7000: return new  avr_op_ANDI(opcode, core);        /* 0111 KKKK dddd KKKK | CBR or ANDI */
                             case 0x3000: return new  avr_op_CPI(opcode, core);         /* 0011 KKKK dddd KKKK | CPI */
                             case 0xE000: return new  avr_op_LDI(opcode, core);         /* 1110 KKKK dddd KKKK | LDI or SER */
                             case 0x6000: return new  avr_op_ORI(opcode, core);         /* 0110 KKKK dddd KKKK | SBR or ORI */
                             case 0x4000: return new  avr_op_SBCI(opcode, core);        /* 0100 KKKK dddd KKKK | SBCI */
                             case 0x5000: return new  avr_op_SUBI(opcode, core);        /* 0101 KKKK dddd KKKK | SUBI */
                         }

                         /* opcodes with a register (Rd) and a register bit number (b) as operands */
                         decode = opcode & ~(mask_Rd_5 | mask_reg_bit);
                         switch ( decode ) {
                             case 0xF800: return new  avr_op_BLD(opcode, core);         /* 1111 100d dddd 0bbb | BLD */
                             case 0xFA00: return new  avr_op_BST(opcode, core);         /* 1111 101d dddd 0bbb | BST */
                             case 0xFC00: return new  avr_op_SBRC(opcode, core);        /* 1111 110d dddd 0bbb | SBRC */
                             case 0xFE00: return new  avr_op_SBRS(opcode, core);        /* 1111 111d dddd 0bbb | SBRS */
                         }

                         /* opcodes with a relative 7-bit address (k) and a register bit number (b) as operands */
                         decode = opcode & ~(mask_k_7 | mask_reg_bit);
                         switch ( decode ) {
                             case 0xF400: return new  avr_op_BRBC(opcode, core);        /* 1111 01kk kkkk kbbb | BRBC */
                             case 0xF000: return new  avr_op_BRBS(opcode, core);        /* 1111 00kk kkkk kbbb | BRBS */
                         }

                         /* opcodes with a 6-bit address displacement (q) and a register (Rd) as operands */
                         decode = opcode & ~(mask_Rd_5 | mask_q_displ);
                         switch ( decode ) {
                             case 0x8008: return new  avr_op_LDD_Y(opcode, core);       /* 10q0 qq0d dddd 1qqq | LDD */
                             case 0x8000: return new  avr_op_LDD_Z(opcode, core);       /* 10q0 qq0d dddd 0qqq | LDD */
                             case 0x8208: return new  avr_op_STD_Y(opcode, core);       /* 10q0 qq1d dddd 1qqq | STD */
                             case 0x8200: return new  avr_op_STD_Z(opcode, core);       /* 10q0 qq1d dddd 0qqq | STD */
                         }

                         /* opcodes with a absolute 22-bit address (k) operand */
                         decode = opcode & ~(mask_k_22);
                         switch ( decode ) {
                             case 0x940E: return new  avr_op_CALL(opcode, core);        /* 1001 010k kkkk 111k | CALL */
                             case 0x940C: return new  avr_op_JMP(opcode, core);         /* 1001 010k kkkk 110k | JMP */
                         }

                         /* opcode with a sreg bit select (s) operand */
                         decode = opcode & ~(mask_sreg_bit);
                         switch ( decode ) {
                             /* BCLR takes place of CL{C,Z,N,V,S,H,T,I} */
                             /* BSET takes place of SE{C,Z,N,V,S,H,T,I} */
                             case 0x9488: return new  avr_op_BCLR(opcode, core);        /* 1001 0100 1sss 1000 | BCLR */
                             case 0x9408: return new  avr_op_BSET(opcode, core);        /* 1001 0100 0sss 1000 | BSET */
                         }

                         /* opcodes with a 6-bit constant (K) and a register (Rd) as operands */
                         decode = opcode & ~(mask_K_6 | mask_Rd_2);
                         switch ( decode ) {
                             case 0x9600: return new  avr_op_ADIW(opcode, core);        /* 1001 0110 KKdd KKKK | ADIW */
                             case 0x9700: return new  avr_op_SBIW(opcode, core);        /* 1001 0111 KKdd KKKK | SBIW */
                         }

                         /* opcodes with a 5-bit IO Addr (A) and register bit number (b) as operands */
                         decode = opcode & ~(mask_A_5 | mask_reg_bit);
                         switch ( decode ) {
                             case 0x9800: return new  avr_op_CBI(opcode, core);         /* 1001 1000 AAAA Abbb | CBI */
                             case 0x9A00: return new  avr_op_SBI(opcode, core);         /* 1001 1010 AAAA Abbb | SBI */
                             case 0x9900: return new  avr_op_SBIC(opcode, core);        /* 1001 1001 AAAA Abbb | SBIC */
                             case 0x9B00: return new  avr_op_SBIS(opcode, core);        /* 1001 1011 AAAA Abbb | SBIS */
                         }

                         /* opcodes with a 6-bit IO Addr (A) and register (Rd) as operands */
                         decode = opcode & ~(mask_A_6 | mask_Rd_5);
                         switch ( decode ) {
                             case 0xB000: return new  avr_op_IN(opcode, core);          /* 1011 0AAd dddd AAAA | IN */
                             case 0xB800: return new  avr_op_OUT(opcode, core);         /* 1011 1AAd dddd AAAA | OUT */
                         }

                         /* opcodes with a relative 12-bit address (k) operand */
                         decode = opcode & ~(mask_k_12);
                         switch ( decode ) {
                             case 0xD000: return new  avr_op_RCALL(opcode, core);       /* 1101 kkkk kkkk kkkk | RCALL */
                             case 0xC000: return new  avr_op_RJMP(opcode, core);        /* 1100 kkkk kkkk kkkk | RJMP */
                         }

                         /* opcodes with two 4-bit register (Rd and Rr) operands */
                         decode = opcode & ~(mask_Rd_4 | mask_Rr_4);
                         switch ( decode ) {
                             case 0x0100: return new  avr_op_MOVW(opcode, core);        /* 0000 0001 dddd rrrr | MOVW */
                             case 0x0200: return new  avr_op_MULS(opcode, core);        /* 0000 0010 dddd rrrr | MULS */
                         }

                         /* opcodes with two 3-bit register (Rd and Rr) operands */
                         decode = opcode & ~(mask_Rd_3 | mask_Rr_3);
                         switch ( decode ) {
                             case 0x0300: return new  avr_op_MULSU(opcode, core);       /* 0000 0011 0ddd 0rrr | MULSU */
                             case 0x0308: return new  avr_op_FMUL(opcode, core);        /* 0000 0011 0ddd 1rrr | FMUL */
                             case 0x0380: return new  avr_op_FMULS(opcode, core);       /* 0000 0011 1ddd 0rrr | FMULS */
                             case 0x0388: return new  avr_op_FMULSU(opcode, core);      /* 0000 0011 1ddd 1rrr | FMULSU */
                         }

                     } /* default */
    } /* first switch */

    //return NULL;
    return new avr_op_ILLEGAL(opcode, core);

} /* decode opcode function */

