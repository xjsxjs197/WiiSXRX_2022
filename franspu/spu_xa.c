////////////////////////////////////////////////////////////////////////
// XA GLOBALS
////////////////////////////////////////////////////////////////////////

#include "franspu.h"

xa_decode_t   * xapGlobal=0;

unsigned long * XAFeed  = NULL;
unsigned long * XAPlay  = NULL;
unsigned long * XAStart = NULL;
unsigned long * XAEnd   = NULL;
unsigned long   XARepeat  = 0;
// add xjsxjs197 start
unsigned long   XALastVal = 0;
// add xjsxjs197 end

int             iLeftXAVol  = 32767;
int             iRightXAVol = 32767;

// MIX XA
void MixXA(void)
{
	int i;
	// upd xjsxjs197 start
	//unsigned long XALastVal = 0;
	// upd xjsxjs197 end
	int leftvol =iLeftXAVol;
	int rightvol=iRightXAVol;
	int *ssuml=SSumL;
	int *ssumr=SSumR;

    // upd xjsxjs197 start
	/*for(i=0;i<NSSIZE && XAPlay!=XAFeed;i++)
	{
		XALastVal=*XAPlay++;
		if(XAPlay==XAEnd) XAPlay=XAStart;
		//(*ssuml++)+=(((short)(XALastVal&0xffff))       * leftvol)/32768;
		//(*ssumr++)+=(((short)((XALastVal>>16)&0xffff)) * rightvol)/32768;
		(*ssuml++)+=(((int)(short)(XALastVal&0xffff))       * leftvol) >> 15;
		(*ssumr++)+=(((int)(short)((XALastVal>>16)&0xffff)) * rightvol) >> 15;
	}

	if(XAPlay==XAFeed && XARepeat)
	{
		XARepeat--;
		for(;i<NSSIZE;i++)
		{
			//(*ssuml++)+=(((short)(XALastVal&0xffff))       * leftvol)/32768;
			//(*ssumr++)+=(((short)((XALastVal>>16)&0xffff)) * rightvol)/32768;
			(*ssuml++)+=(((int)(short)(XALastVal&0xffff))       * leftvol) >> 15;
			(*ssumr++)+=(((int)(short)((XALastVal>>16)&0xffff)) * rightvol) >> 15;
		}
	}*/
	uint32_t v;
	if (XAPlay != XAFeed || XARepeat > 0)
    {
        if (XAPlay == XAFeed)
        {
            XARepeat--;
        }

        v = XALastVal;

        for (i = 0; i < NSSIZE; i++)
        {
            if (XAPlay != XAFeed) v = *XAPlay++;
            if (XAPlay == XAEnd) XAPlay = XAStart;

            (*ssuml++) += (((int)(short)(v & 0xffff)) * leftvol) >> 15;
            (*ssumr++) += (((int)(short)((v >> 16) & 0xffff)) * rightvol) >> 15;
        }

        XALastVal = v;
    }
	// upd xjsxjs197 end

    // add xjsxjs197 start
    /*for(i = 0; i < NSSIZE && CDDAPlay != CDDAFeed && (CDDAPlay != CDDAEnd - 1 || CDDAFeed != CDDAStart); i++)
    {
        v = *CDDAPlay++;
        if (CDDAPlay == CDDAEnd) CDDAPlay = CDDAStart;

        (*ssuml++) += (((int)(short)(v & 0xffff)) * leftvol) >> 15;
        (*ssumr++) += (((int)(short)((v >> 16) & 0xffff)) * rightvol) >> 15;
    }*/
	// add xjsxjs197 end
}

// FEED XA
void FeedXA(xa_decode_t *xap)
{
	int sinc,spos,i,iSize,iPlace;

	if(!bSPUIsOpen) return;

	xapGlobal = xap;                                      // store info for save states
	XARepeat  = 100;                                      // set up repeat

	iSize=((44100*xap->nsamples)/xap->freq);              // get size
	if(!iSize) return;                                    // none? bye

	if(XAFeed<XAPlay) {
		//if ((XAPlay-XAFeed)==0) return;               // how much space in my buf?
		iPlace = XAPlay - XAFeed;
	} else {
		//if (((XAEnd-XAFeed) + (XAPlay-XAStart))==0) return;
		iPlace = (XAEnd - XAFeed) + (XAPlay - XAStart);
	}
	#ifdef DISP_DEBUG
	if (iSize > iPlace)
    {
        //PRINT_LOG2("PlayCDDA(FeedXA) bufSize: %d, DataSize: %d", iPlace, iSize);
    }
    else
    {
        //PRINT_LOG1("PlayCDDA(FeedXA) Play XA DataSize: %d", iSize);
    }
	#endif
	if (iPlace == 0) return;

	spos=0x10000L;
	sinc = (xap->nsamples << 16) / iSize;                 // calc freq by num / size

	if(xap->stereo)
	{
		unsigned long * pS=(unsigned long *)xap->pcm;
		unsigned long l=0;

		for(i=0;i<iSize;i++)
		{
			while(spos>=0x10000L)
			{
				l = *pS++;
				spos -= 0x10000L;
			}

			*XAFeed++=l;

			if(XAFeed==XAEnd)
				XAFeed=XAStart;
			if(XAFeed==XAPlay)
			{
				if(XAPlay!=XAStart)
					XAFeed=XAPlay-1;
				break;
			}

			spos += sinc;
		}
	}
	else
	{
		unsigned short * pS=(unsigned short *)xap->pcm;
		short s=0;

		for(i=0;i<iSize;i++)
		{
			while(spos>=0x10000L)
			{
				s = *pS++;
				spos -= 0x10000L;
			}
			unsigned long l=s;

			*XAFeed++=(l|(l<<16));

			if(XAFeed==XAEnd)
				XAFeed=XAStart;
			if(XAFeed==XAPlay)
			{
				if(XAPlay!=XAStart)
					XAFeed=XAPlay-1;
				break;
			}

			spos += sinc;
		}
	}
}

////////////////////////////////////////////////////////////////////////
// FEED CDDA
////////////////////////////////////////////////////////////////////////
int FeedCDDA(unsigned char *pcm, int nBytes)
{
    int space;
    space = ((CDDAPlay - CDDAFeed -1) << 2) & (CDDA_BUFFER_SIZE - 1);
    if (space < nBytes)
    {
        return 0x7761; // rearmed_wait
    }

    while (nBytes > 0)
    {
        if (CDDAFeed == CDDAEnd)
        {
            CDDAFeed = CDDAStart;
        }

        space = ((CDDAPlay - CDDAFeed - 1) << 2) & (CDDA_BUFFER_SIZE - 1);
        if (CDDAFeed + (space >> 2) > CDDAEnd)
        {
            space = (CDDAEnd - CDDAFeed) << 2;
        }

        if (space > nBytes)
        {
            space = nBytes;
        }

        memcpy(CDDAFeed, pcm, space);
        CDDAFeed += space >> 2;
        nBytes -= space;
        pcm += space;
    }

    return 0x676f; // rearmed_go
}
