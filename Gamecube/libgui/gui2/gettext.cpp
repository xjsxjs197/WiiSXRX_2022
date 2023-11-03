#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gctypes.h>
#include <unistd.h>

#include "gettext.h"
#include "filelist.h"
#include "../../wiiSXconfig.h"
#include "../../MEM2.h"

typedef struct _MSG
{
    u32 id;
    char* msgstr;
    struct _MSG *next;
} MSG;
static MSG *baseMSG = 0;
static MSG *baseMSG_en = 0;
static MSG *baseMSG_chs = 0;
static MSG *baseMSG_kr = 0;
static MSG *baseMSG_es = 0;
static MSG *baseMSG_pt = 0;
static MSG *baseMSG_it = 0;
static MSG *baseMSG_de = 0;
static MSG *baseMSG_cht = 0;
static MSG *baseMSG_jp = 0;
static MSG *baseMSG_fr = 0;
static MSG *baseMSG_br = 0;
static MSG *baseMSG_ca = 0;
static MSG *baseMSG_tu = 0;

#define HASHWORDBITS 32

/* Defines the so called `hashpjw' function by P.J. Weinberger
 [see Aho/Sethi/Ullman, COMPILERS: Principles, Techniques and Tools,
 1986, 1987 Bell Telephone Laboratories, Inc.]  */
static inline u32 hash_string(const char *str_param)
{
    u32 hval, g;
    const char *str = str_param;

    /* Compute the hash value for the given string.  */
    hval = 0;
    while (*str != '\0')
    {
        hval <<= 4;
        hval += (u8) * str++;
        g = hval & ((u32) 0xf << (HASHWORDBITS - 4));
        if (g != 0)
        {
            hval ^= g >> (HASHWORDBITS - 8);
            hval ^= g;
        }
    }
    return hval;
}

/* Expand some escape sequences found in the argument string.  */
static char *
expand_escape(const char *str)
{
    char *retval, *rp;
    const char *cp = str;

    retval = (char *) malloc(strlen(str) + 1);
    if (retval == NULL)
        return NULL;
    rp = retval;

    while (cp[0] != '\0' && cp[0] != '\\')
        *rp++ = *cp++;
    if (cp[0] == '\0')
        goto terminate;
    do
    {

        /* Here cp[0] == '\\'.  */
        switch (*++cp)
        {
        case '\"': /* " */
            *rp++ = '\"';
            ++cp;
            break;
        case 'a': /* alert */
            *rp++ = '\a';
            ++cp;
            break;
        case 'b': /* backspace */
            *rp++ = '\b';
            ++cp;
            break;
        case 'f': /* form feed */
            *rp++ = '\f';
            ++cp;
            break;
        case 'n': /* new line */
            *rp++ = '\n';
            ++cp;
            break;
        case 'r': /* carriage return */
            *rp++ = '\r';
            ++cp;
            break;
        case 't': /* horizontal tab */
            *rp++ = '\t';
            ++cp;
            break;
        case 'v': /* vertical tab */
            *rp++ = '\v';
            ++cp;
            break;
        case '\\':
            *rp = '\\';
            ++cp;
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        {
            int ch = *cp++ - '0';

            if (*cp >= '0' && *cp <= '7')
            {
                ch *= 8;
                ch += *cp++ - '0';

                if (*cp >= '0' && *cp <= '7')
                {
                    ch *= 8;
                    ch += *cp++ - '0';
                }
            }
            *rp = ch;
        }
            break;
        default:
            *rp = '\\';
            break;
        }

        while (cp[0] != '\0' && cp[0] != '\\')
            *rp++ = *cp++;
    } while (cp[0] != '\0');

    /* Terminate string.  */
    terminate: *rp = '\0';
    return retval;
}

static MSG *findMSG(u32 id)
{
    MSG *msg;
    for (msg = baseMSG; msg; msg = msg->next)
    {
        if (msg->id == id)
            return msg;
    }
    return NULL;
}

static MSG *setMSG(const char *msgid, const char *msgstr)
{
    u32 id = hash_string(msgid);
    MSG *msg = findMSG(id);
    if (!msg)
    {
        msg = (MSG *) malloc(sizeof(MSG));
        msg->id = id;
        msg->msgstr = NULL;
        msg->next = baseMSG;
        baseMSG = msg;
    }
    if (msg)
    {
        if (msgstr)
        {
            if (msg->msgstr)
                free(msg->msgstr);

            msg->msgstr = expand_escape(msgstr);
        }
        return msg;
    }
    return NULL;
}

static void gettextCleanUp(void)
{
    while (baseMSG)
    {
        MSG *nextMsg = baseMSG->next;
        free(baseMSG->msgstr);
        free(baseMSG);
        baseMSG = nextMsg;
    }
}

static char * memfgets(char * dst, int maxlen, char * src)
{
    if(!src || !dst || maxlen <= 0)
        return NULL;

    char * newline = strchr(src, '\n');

    if(newline == NULL)
        return NULL;

    memcpy(dst, src, (newline-src));
    dst[(newline-src)] = 0;
    return ++newline;
}

static FILE* getLangFile(char* sdUsb)
{
    char langPathBuf[256];
    switch(lang)
    {
        case SIMP_CHINESE:
        case TRAD_CHINESE:
            sprintf(langPathBuf, "%s%s", sdUsb, ":/wiisxrx/lang/zh.lang");
            break;

        case KOREAN:
            sprintf(langPathBuf, "%s%s", sdUsb, ":/wiisxrx/lang/Kr.lang");
            break;

        case SPANISH:
            sprintf(langPathBuf, "%s%s", sdUsb, ":/wiisxrx/lang/es.lang");
            break;

        case PORTUGUESE:
            sprintf(langPathBuf, "%s%s", sdUsb, ":/wiisxrx/lang/pt.lang");
            break;

        case ITALIAN:
            sprintf(langPathBuf, "%s%s", sdUsb, ":/wiisxrx/lang/it.lang");
            break;

        case GERMAN:
            sprintf(langPathBuf, "%s%s", sdUsb, ":/wiisxrx/lang/de.lang");
            break;

        case JAPANESE:
            sprintf(langPathBuf, "%s%s", sdUsb, ":/wiisxrx/lang/jp.lang");
            break;

        case FRENCH:
            sprintf(langPathBuf, "%s%s", sdUsb, ":/wiisxrx/lang/fr.lang");
            break;

        case BRAZILIAN_PORTUGUESE:
            sprintf(langPathBuf, "%s%s", sdUsb, ":/wiisxrx/lang/br.lang");
            break;

        case CATALAN:
            sprintf(langPathBuf, "%s%s", sdUsb, ":/wiisxrx/lang/ca.lang");
            break;

        case TURKISH:
            sprintf(langPathBuf, "%s%s", sdUsb, ":/wiisxrx/lang/tu.lang");
            break;

        default:
            return NULL;
    }
    return fopen(langPathBuf, "rb");
}

static void loadLangFile(char *file, char *eof)
{
    char line[200];
    char *lastID = NULL;

    while (file && file < eof)
    {
        file = memfgets(line, sizeof(line), file);

        if(!file)
            break;

        // lines starting with # are comments
        if (line[0] == '#')
            continue;

        if (strncmp(line, "msgid \"", 7) == 0)
        {
            char *msgid, *end;
            if (lastID)
            {
                free(lastID);
                lastID = NULL;
            }
            msgid = &line[7];
            end = strrchr(msgid, '"');
            if (end && end - msgid > 1)
            {
                *end = 0;
                lastID = strdup(msgid);
            }
        }
        else if (strncmp(line, "msgstr \"", 8) == 0)
        {
            char *msgstr, *end;

            if (lastID == NULL)
                continue;

            msgstr = &line[8];
            end = strrchr(msgstr, '"');
            if (end && end - msgstr > 1)
            {
                *end = 0;
                setMSG(lastID, msgstr);
            }
            free(lastID);
            lastID = NULL;
        }
    }
}

bool LoadLanguage()
{
    baseMSG = baseMSG_en;
    loadLangFile((char *)en_lang, (char *)en_lang + en_lang_size);
    baseMSG_en = baseMSG;

    baseMSG = baseMSG_chs;
    loadLangFile((char *)zh_lang, (char *)zh_lang + zh_lang_size);
    baseMSG_chs = baseMSG;

    baseMSG = baseMSG_kr;
    loadLangFile((char *)kr_lang, (char *)kr_lang + kr_lang_size);
    baseMSG_kr = baseMSG;

    baseMSG = baseMSG_es;
    loadLangFile((char *)es_lang, (char *)es_lang + es_lang_size);
    baseMSG_es = baseMSG;

    baseMSG = baseMSG_pt;
    loadLangFile((char *)pt_lang, (char *)pt_lang + pt_lang_size);
    baseMSG_pt = baseMSG;

    baseMSG = baseMSG_it;
    loadLangFile((char *)it_lang, (char *)it_lang + it_lang_size);
    baseMSG_it = baseMSG;

    baseMSG = baseMSG_de;
    loadLangFile((char *)de_lang, (char *)de_lang + de_lang_size);
    baseMSG_de = baseMSG;

//    baseMSG = baseMSG_cht;
//    loadLangFile((char *)cht_lang, (char *)cht_lang + cht_lang_size);
//    baseMSG_cht = baseMSG;
//
//    baseMSG = baseMSG_jp;
//    loadLangFile((char *)jp_lang, (char *)jp_lang + jp_lang_size);
//    baseMSG_jp = baseMSG;
//
//    baseMSG = baseMSG_fr;
//    loadLangFile((char *)fr_lang, (char *)fr_lang + fr_lang_size);
//    baseMSG_fr = baseMSG;
//
//    baseMSG = baseMSG_ca;
//    loadLangFile((char *)ca_lang, (char *)ca_lang + ca_lang_size);
//    baseMSG_ca = baseMSG;
//
//    baseMSG = baseMSG_tu;
//    loadLangFile((char *)tu_lang, (char *)tu_lang + tu_lang_size);
//    baseMSG_tu = baseMSG;

    return true;
}

void ChangeLanguage()
{
    switch(lang)
    {
        case SIMP_CHINESE:
        case TRAD_CHINESE:
            baseMSG = baseMSG_chs;
            break;

        case KOREAN:
            baseMSG = baseMSG_kr;
            break;

        case SPANISH:
            baseMSG = baseMSG_es;
            break;

        case PORTUGUESE:
            baseMSG = baseMSG_pt;
            break;

        case ITALIAN:
            baseMSG = baseMSG_it;
            break;

        case GERMAN:
            baseMSG = baseMSG_de;
            break;

        case JAPANESE:
            baseMSG = baseMSG_jp;
            break;

        case FRENCH:
            baseMSG = baseMSG_fr;
            break;

        case BRAZILIAN_PORTUGUESE:
            baseMSG = baseMSG_br;
            break;

        case CATALAN:
            baseMSG = baseMSG_ca;
            break;

        case TURKISH:
            baseMSG = baseMSG_tu;
            break;

        default:
            baseMSG = baseMSG_en;
            break;
    }
}

void ReleaseLanguage()
{
    baseMSG = baseMSG_en;
    gettextCleanUp();

    baseMSG = baseMSG_chs;
    gettextCleanUp();

    baseMSG = baseMSG_kr;
    gettextCleanUp();

    baseMSG = baseMSG_es;
    gettextCleanUp();

    baseMSG = baseMSG_pt;
    gettextCleanUp();

    baseMSG = baseMSG_it;
    gettextCleanUp();

    baseMSG = baseMSG_de;
    gettextCleanUp();

//    baseMSG = baseMSG_cht;
//    gettextCleanUp();
//
//    baseMSG = baseMSG_jp;
//    gettextCleanUp();
//
//    baseMSG = baseMSG_fr;
//    gettextCleanUp();
//
//    baseMSG = baseMSG_ca;
//    gettextCleanUp();
//
//    baseMSG = baseMSG_tu;
//    gettextCleanUp();
}

const char *gettext(const char *msgid)
{
    MSG *msg = findMSG(hash_string(msgid));

    if (msg && msg->msgstr)
    {
        return msg->msgstr;
    }
    return msgid;
}

char joinStr[100];
char * JoinString(char *s1, char *s2)
{
    joinStr[0] = '\0';
    const char *utf8Txt = gettext(s1);

    strcpy(joinStr, utf8Txt);
    strcat(joinStr, s2);

    return joinStr;
}
