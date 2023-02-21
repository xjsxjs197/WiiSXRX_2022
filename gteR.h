/***************************************************************************
 *   PCSX-Revolution - PlayStation Emulator for Nintendo Wii               *
 *   Copyright (C) 2009-2010  PCSX-Revolution Dev Team                     *
 *   <http://code.google.com/p/pcsx-revolution/>                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

#ifndef __GTE_H__
#define __GTE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "r3000a.h"

struct psxCP2Regs;

void gteRTPS_R(struct psxCP2Regs *regs);
void gteOP_R(struct psxCP2Regs *regs);
void gteNCLIP_R(struct psxCP2Regs *regs);
void gteDPCS_R(struct psxCP2Regs *regs);
void gteINTPL_R(struct psxCP2Regs *regs);
void gteMVMVA_R(struct psxCP2Regs *regs);
void gteNCDS_R(struct psxCP2Regs *regs);
void gteNCDT_R(struct psxCP2Regs *regs);
void gteCDP_R(struct psxCP2Regs *regs);
void gteNCCS_R(struct psxCP2Regs *regs);
void gteCC_R(struct psxCP2Regs *regs);
void gteNCS_R(struct psxCP2Regs *regs);
void gteNCT_R(struct psxCP2Regs *regs);
void gteSQR_R(struct psxCP2Regs *regs);
void gteDCPL_R(struct psxCP2Regs *regs);
void gteDPCT_R(struct psxCP2Regs *regs);
void gteAVSZ3_R(struct psxCP2Regs *regs);
void gteAVSZ4_R(struct psxCP2Regs *regs);
void gteRTPT_R(struct psxCP2Regs *regs);
void gteGPF_R(struct psxCP2Regs *regs);
void gteGPL_R(struct psxCP2Regs *regs);
void gteNCCT_R(struct psxCP2Regs *regs);

#ifdef __cplusplus
}
#endif
#endif
