#ifndef _GETTEXT_H_
#define _GETTEXT_H_

bool LoadLanguage();

/*
 * input msg = a text in ASCII
 * output = the translated msg in utf-8
 */
extern "C" const char *gettext(const char *msg);
extern "C" char * JoinString(char *s1, char *s2);

enum {
	LANG_JAPANESE = 0,
	LANG_ENGLISH,
	LANG_GERMAN,
	LANG_FRENCH,
	LANG_SPANISH,
	LANG_ITALIAN,
	LANG_DUTCH,
	LANG_SIMP_CHINESE,
	LANG_TRAD_CHINESE,
	LANG_KOREAN,
	LANG_PORTUGUESE,
	LANG_BRAZILIAN_PORTUGUESE,
	LANG_CATALAN,
	LANG_TURKISH,
	LANG_LENGTH
};

#endif /* _GETTEXT_H_ */
