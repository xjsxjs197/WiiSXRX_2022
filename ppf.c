/*  PPF Patch Support for PCSX-Reloaded
 *  Copyright (c) 2009, Wei Mingzhi <whistler_wmz@users.sf.net>.
 *
 *  Based on P.E.Op.S CDR Plugin by Pete Bernert.
 *  Copyright (c) 2002, Pete Bernert.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA
 */

#include "psxcommon.h"
#include "ppf.h"
#include "cdrom.h"

typedef struct tagPPF_DATA {
	s32					addr;
	s32					pos;
	s32					anz;
	struct tagPPF_DATA	*pNext;
} PPF_DATA;

typedef struct tagPPF_CACHE {
	s32					addr;
	struct tagPPF_DATA	*pNext;
} PPF_CACHE;

static PPF_CACHE		*ppfCache = NULL;
static PPF_DATA			*ppfHead = NULL, *ppfLast = NULL;
static int				iPPFNum = 0;

// using a linked data list, and address array
static void FillPPFCache() {
	PPF_DATA		*p;
	PPF_CACHE		*pc;
	s32				lastaddr;

	p = ppfHead;
	lastaddr = -1;
	iPPFNum = 0;

	while (p != NULL) {
		if (p->addr != lastaddr) iPPFNum++;
		lastaddr = p->addr;
		p = p->pNext;
	}

	if (iPPFNum <= 0) return;

	pc = ppfCache = (PPF_CACHE *)malloc(iPPFNum * sizeof(PPF_CACHE));

	iPPFNum--;
	p = ppfHead;
	lastaddr = -1;

	while (p != NULL) {
		if (p->addr != lastaddr) {
			pc->addr = p->addr;
			pc->pNext = p;
			pc++;
		}
		lastaddr = p->addr;
		p = p->pNext;
	}
}

void FreePPFCache() {
	PPF_DATA *p = ppfHead;
	void *pn;

	while (p != NULL) {
		pn = p->pNext;
		free(p);
		p = (PPF_DATA *)pn;
	}
	ppfHead = NULL;
	ppfLast = NULL;

	if (ppfCache != NULL) free(ppfCache);
	ppfCache = NULL;
}

void CheckPPFCache(unsigned char *pB, unsigned char m, unsigned char s, unsigned char f) {
	PPF_CACHE *pcstart, *pcend, *pcpos;
	//int addr = MSF2SECT(btoi(m), btoi(s), btoi(f)), pos, anz, start;
	int addr = MSF2SECT((m), (s), btoi(f)), pos, anz, start;

	if (ppfCache == NULL) return;

	pcstart = ppfCache;
	if (addr < pcstart->addr) return;
	pcend = ppfCache + iPPFNum;
	if (addr > pcend->addr) return;

	while (1) {
		if (addr == pcend->addr) { pcpos = pcend; break; }

		pcpos = pcstart + (pcend - pcstart) / 2;
		if (pcpos == pcstart) break;
		if (addr < pcpos->addr) {
			pcend = pcpos;
			continue;
		}
		if (addr > pcpos->addr) {
			pcstart = pcpos;
			continue;
		}
		break;
	}

	if (addr == pcpos->addr) {
		PPF_DATA *p = pcpos->pNext;
		while (p != NULL && p->addr == addr) {
			pos = p->pos - (CD_FRAMESIZE_RAW - DATA_SIZE);
			anz = p->anz;
			if (pos < 0) { start = -pos; pos = 0; anz -= start; }
			else start = 0;
			memcpy(pB + pos, (unsigned char *)(p + 1) + start, anz);
			p = p->pNext;
		}
	}
}

static void AddToPPF(s32 ladr, s32 pos, s32 anz, unsigned char *ppfmem) {
	if (ppfHead == NULL) {
		ppfHead = (PPF_DATA *)malloc(sizeof(PPF_DATA) + anz);
		ppfHead->addr = ladr;
		ppfHead->pNext = NULL;
		ppfHead->pos = pos;
		ppfHead->anz = anz;
		memcpy(ppfHead + 1, ppfmem, anz);
		iPPFNum = 1;
		ppfLast = ppfHead;
	} else {
		PPF_DATA *p = ppfHead;
		PPF_DATA *plast = NULL;
		PPF_DATA *padd;

		if (ladr > ppfLast->addr || (ladr == ppfLast->addr && pos > ppfLast->pos)) {
			p = NULL;
			plast = ppfLast;
		} else {
			while (p != NULL) {
				if (ladr < p->addr) break;
				if (ladr == p->addr) {
					while (p && ladr == p->addr && pos > p->pos) {
						plast = p;
						p = p->pNext;
					}
					break;
				}
				plast = p;
				p = p->pNext;
			}
		}

		padd = (PPF_DATA *)malloc(sizeof(PPF_DATA) + anz);
		padd->addr = ladr;
		padd->pNext = p;
		padd->pos = pos;
		padd->anz = anz;
		memcpy(padd + 1, ppfmem, anz);
		iPPFNum++;
		if (plast == NULL) ppfHead = padd;
		else plast->pNext = padd;

		if (padd->pNext == NULL) ppfLast = padd;
	}
}

void BuildPPFCache() {
	FILE			*ppffile;
	char			buffer[12];
	char			undo = 0, blockcheck = 0;
	int				method, dizlen, dizyn;
	unsigned char	ppfmem[512];
	char			szPPF[MAXPATHLEN * 2];
	int				count, seekpos, pos;
	u32				anz; // use 32-bit to avoid stupid overflows
	s32				ladr, off, anx;

	FreePPFCache();

	if (CdromId[0] == '\0') return;

	// Generate filename in the format of SLUS_123.45
	buffer[0] = toupper(CdromId[0]);
	buffer[1] = toupper(CdromId[1]);
	buffer[2] = toupper(CdromId[2]);
	buffer[3] = toupper(CdromId[3]);
	buffer[4] = '_';
	buffer[5] = CdromId[4];
	buffer[6] = CdromId[5];
	buffer[7] = CdromId[6];
	buffer[8] = '.';
	buffer[9] = CdromId[7];
	buffer[10] = CdromId[8];
	buffer[11] = '\0';

	sprintf(szPPF, "%s%s", Config.PatchesDir, buffer);
	#ifdef DISP_DEBUG
	sprintf(debugInfo, "%s", szPPF);
	#endif // DISP_DEBUG

	ppffile = fopen(szPPF, "rb");
	if (ppffile == NULL)
    {
        #ifdef DISP_DEBUG
        strcat(debugInfo, " file open error");
        #endif // DISP_DEBUG
        return;
    }

	memset(buffer, 0, 5);
	if (fread(buffer, 3, 1, ppffile) != 1)
    {
        #ifdef DISP_DEBUG
        strcat(debugInfo, " fread 3 Error");
        #endif // DISP_DEBUG
        goto fail_io;
    }

	if (strcmp(buffer, "PPF") != 0) {
        #ifdef DISP_DEBUG
        strcat(debugInfo, " Invalid PPF");
        #endif // DISP_DEBUG
		SysPrintf(_("Invalid PPF patch: %s.\n"), szPPF);
		fclose(ppffile);
		return;
	}

	fseek(ppffile, 3, SEEK_SET);
	method = fgetc(ppffile);
	#ifdef DISP_DEBUG
    sprintf(debugInfo, " method: %c", method);
    #endif // DISP_DEBUG

    method -= 48;
	switch (method) {
		case 1: // ppf1
			fseek(ppffile, 0, SEEK_END);
			count = ftell(ppffile);
			count -= 56;
			seekpos = 56;
			break;

		case 2: // ppf2
			fseek(ppffile, -8, SEEK_END);

			memset(buffer, 0, 5);
			if (fread(buffer, 4, 1, ppffile) != 1)
            {
                #ifdef DISP_DEBUG
                sprintf(debugInfo, " ppf2 fread end error");
                #endif // DISP_DEBUG
                goto fail_io;
            }

			if (strcmp(".DIZ", buffer) != 0) {
                #ifdef DISP_DEBUG
                sprintf(debugInfo, " DIZ = 0");
                #endif // DISP_DEBUG
				dizyn = 0;
			} else {
				if (fread(&dizlen, 4, 1, ppffile) != 1)
				{
				    #ifdef DISP_DEBUG
                    sprintf(debugInfo, " dizlen ERROR");
                    #endif // DISP_DEBUG
                    goto fail_io;
				}

				dizlen = SWAP32(dizlen);
				dizyn = 1;
			}

			fseek(ppffile, 0, SEEK_END);
			count = ftell(ppffile);

			if (dizyn == 0) {
				count -= 1084;
				seekpos = 1084;
			} else {
				count -= 1084;
				count -= 38;
				count -= dizlen;
				seekpos = 1084;
			}
			break;

		case 3: // ppf3
			fseek(ppffile, 57, SEEK_SET);
			blockcheck = fgetc(ppffile);
			undo = fgetc(ppffile);

			fseek(ppffile, -6, SEEK_END);
			memset(buffer, 0, 5);
			if (fread(buffer, 4, 1, ppffile) != 1)
				goto fail_io;
			dizlen = 0;

			if (strcmp(".DIZ", buffer) == 0) {
				fseek(ppffile, -2, SEEK_END);
				// TODO: Endian/size unsafe?
				if (fread(&dizlen, 2, 1, ppffile) != 1)
					goto fail_io;
				dizlen = SWAP32(dizlen);
				dizlen += 36;
			}

			fseek(ppffile, 0, SEEK_END);
			count = ftell(ppffile);
			count -= dizlen;

			if (blockcheck) {
				seekpos = 1084;
				count -= 1084;
			} else {
				seekpos = 60;
				count -= 60;
			}
			break;

		default:
		    #ifdef DISP_DEBUG
            sprintf(debugInfo, " default ERROR %d", method);
            #endif // DISP_DEBUG
			fclose(ppffile);
			SysPrintf(_("Unsupported PPF version (%d).\n"), method + 1);
			return;
	}

	// now do the data reading
	do {
		fseek(ppffile, seekpos, SEEK_SET);
		if (fread(&pos, sizeof(pos), 1, ppffile) != 1)
        {
            #ifdef DISP_DEBUG
            sprintf(debugInfo, " data reading Pos Error");
            #endif // DISP_DEBUG
            goto fail_io;
        }

		pos = SWAP32(pos);

		if (method == 3) {
			// skip 4 bytes on ppf3 (no int64 support here)
			if (fread(buffer, 4, 1, ppffile) != 1)
            {
                #ifdef DISP_DEBUG
                sprintf(debugInfo, " skip 4 bytes on ppf3 (no int64 support here)");
                #endif // DISP_DEBUG
				goto fail_io;
		    }
		}

		anz = fgetc(ppffile);
		if (fread(ppfmem, anz, 1, ppffile) != 1)
        {
            #ifdef DISP_DEBUG
            sprintf(debugInfo, " anz pos Error");
            #endif // DISP_DEBUG
            goto fail_io;
        }

		ladr = pos / CD_FRAMESIZE_RAW;
		off = pos % CD_FRAMESIZE_RAW;

		if (off + anz > CD_FRAMESIZE_RAW) {
			anx = off + anz - CD_FRAMESIZE_RAW;
			anz -= (unsigned char)anx;
			AddToPPF(ladr + 1, 0, anx, &ppfmem[anz]);
		}

		AddToPPF(ladr, off, anz, ppfmem); // add to link list

		if (method == 3) {
			if (undo) anz += anz;
			anz += 4;
		}

		seekpos = seekpos + 5 + anz;
		count = count - 5 - anz;
	} while (count > 0); // loop til end

	fclose(ppffile);

	FillPPFCache(); // build address array

	SysPrintf(_("Loaded PPF %d.0 patch: %s.\n"), method + 1, szPPF);
	#ifdef DISP_DEBUG
    sprintf(debugInfo, "Loaded PPF %d.0 patch: %s.\n", method + 1, szPPF);
    #endif // DISP_DEBUG

    ppffile = NULL;

fail_io:
#ifndef NDEBUG
	SysPrintf(_("File IO error in <%s:%s>.\n"), __FILE__, __func__);
#endif
    if (ppffile != NULL)
    {
        fclose(ppffile);
    }
}

// redump.org SBI files, slightly different handling from PCSX-Reloaded
unsigned char *sbi_sectors;

int LoadSBI(const char *fname, int sector_count) {
	char buffer[16];
	FILE *sbihandle;
	u8 sbitime[3], t;
	int s;

	sbihandle = fopen(fname, "rb");
	if (sbihandle == NULL)
		return -1;

	sbi_sectors = calloc(1, sector_count / 8);
	if (sbi_sectors == NULL) {
		fclose(sbihandle);
		return -1;
	}

	// 4-byte SBI header
	if (fread(buffer, 1, 4, sbihandle) != 4)
		goto fail_io;

	while (1) {
		s = fread(sbitime, 1, 3, sbihandle);
		if (s != 3)
			goto fail_io;
		if (fread(&t, sizeof(t), 1, sbihandle) != 1)
			goto fail_io;
		switch (t) {
		default:
		case 1:
			s = 10;
			break;
		case 2:
		case 3:
			s = 3;
			break;
		}
		fseek(sbihandle, s, SEEK_CUR);

		//s = MSF2SECT(btoi(sbitime[0]), btoi(sbitime[1]), btoi(sbitime[2]));
		s = MSF2SECT((sbitime[0]), (sbitime[1]), btoi(sbitime[2]));
		if (s < sector_count)
			sbi_sectors[s >> 3] |= 1 << (s&7);
		else
			SysPrintf(_("SBI sector %d >= %d?\n"), s, sector_count);
	}

	fclose(sbihandle);
	return 0;

fail_io:
#ifndef NDEBUG
	SysPrintf(_("File IO error in <%s:%s>.\n"), __FILE__, __func__);
#endif
	fclose(sbihandle);
	return -1;
}

void UnloadSBI(void) {
	if (sbi_sectors) {
		free(sbi_sectors);
		sbi_sectors = NULL;
	}
}
