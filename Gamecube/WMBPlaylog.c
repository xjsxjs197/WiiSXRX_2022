/*
	WMBPlaylog.c
	This code allows to modify play_rec.dat in order to store the
	game time in Wii's log correctly.

	by Marc
	Thanks to tueidj for giving me some hints on how to do it :)
	Most of the code was taken from here:
	http://forum.wiibrew.org/read.php?27,22130

	Modified by Dimok and SuSo
*/

#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <malloc.h>

#define ALIGN32(x) (((x) + 31) & ~31)

#define SECONDS_TO_2000 946684800LL
#define TICKS_PER_SECOND 60750000LL

//! Should be 32 byte aligned
static const char PLAYRECPATH[] ATTRIBUTE_ALIGN(32) = "/title/00000001/00000002/data/play_rec.dat";

typedef struct _PlayRec
{
	u32 checksum;
	union
	{
		u32 data[31];
		struct
		{
			u16 name[42];
			u64 ticks_boot;
			u64 ticks_last;
			char title_id[6];
			char unknown[18];
		} ATTRIBUTE_PACKED;
	};
} PlayRec;

static u64 getWiiTime(void)
{
	time_t uTime = time(NULL);
	return TICKS_PER_SECOND * (uTime - SECONDS_TO_2000);
}

int Playlog_Exit(void)
{
	s32 res = -1;
	u32 sum = 0;
	u8 i;

	//Open play_rec.dat
	s32 fd = IOS_Open(PLAYRECPATH, IPC_OPEN_RW);
	if(fd < 0)
		return fd;

	PlayRec * playrec_buf = memalign(32, ALIGN32(sizeof(PlayRec)));
	if(!playrec_buf)
		goto cleanup;

	//Read play_rec.dat
	if(IOS_Read(fd, playrec_buf, sizeof(PlayRec)) != sizeof(PlayRec))
		goto cleanup;

	if(IOS_Seek(fd, 0, 0) < 0)
		goto cleanup;

	// update exit time
	u64 stime = getWiiTime();
	playrec_buf->ticks_last = stime;
	
	//Calculate and update checksum
	for(i = 0; i < 31; i++)
		sum += playrec_buf->data[i];

	playrec_buf->checksum = sum;

	if(IOS_Write(fd, playrec_buf, sizeof(PlayRec)) != sizeof(PlayRec))
		goto cleanup;

	res = 0;

cleanup:
	free(playrec_buf);
	IOS_Close(fd);
	return res;
}
