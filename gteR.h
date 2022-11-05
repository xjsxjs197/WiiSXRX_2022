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

void gteMFC2_R();
void gteCFC2_R();
void gteMTC2_R();
void gteCTC2_R();
void gteLWC2_R();
void gteSWC2_R();

void gteRTPS_R();
void gteOP_R();
void gteNCLIP_R();
void gteDPCS_R();
void gteINTPL_R();
void gteMVMVA_R();
void gteNCDS_R();
void gteNCDT_R();
void gteCDP_R();
void gteNCCS_R();
void gteCC_R();
void gteNCS_R();
void gteNCT_R();
void gteSQR_R();
void gteDCPL_R();
void gteDPCT_R();
void gteAVSZ3_R();
void gteAVSZ4_R();
void gteRTPT_R();
void gteGPF_R();
void gteGPL_R();
void gteNCCT_R();

#ifdef __cplusplus
}
#endif
#endif
