/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#ifndef __GTE_H__
#define __GTE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"
#include "r3000a.h"

void gteMFC2_U();
void gteCFC2_U();
void gteMTC2_U();
void gteCTC2_U();
void gteLWC2_U();
void gteSWC2_U();

void gteRTPS_U();
void gteNCLIP_U();
void gteOP_U();
void gteDPCS_U();
void gteINTPL_U();
void gteMVMVA_U();
void gteNCDS_U();
void gteCDP_U();
void gteNCDT_U();
void gteNCCS_U();
void gteCC_U();
void gteNCS_U();
void gteNCT_U();
void gteSQR_U();
void gteDCPL_U();
void gteDPCT_U();
void gteAVSZ3_U();
void gteAVSZ4_U();
void gteRTPT_U();
void gteGPF_U();
void gteGPL_U();
void gteNCCT_U();

#ifdef __cplusplus
}
#endif
#endif
