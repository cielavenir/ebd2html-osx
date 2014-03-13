/*
 * ebd2html - EBDump�̏o�͂���EBStudio�ւ̓���HTML�t�@�C�����č\�����鎎��
 *	Written by Junn Ohta (ohta@sdg.mdd.ricoh.co.jp), Public Domain.
 */

char	*progname = "ebd2html";
char	*version  = "experimental-0.05a for OSX";
char	*date     = "2014/03/14";

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <io.h>
//#include <dir.h>

typedef	unsigned char	byte;
typedef	unsigned int	word;
typedef	unsigned long	dword;

#ifndef O_BINARY
#define	O_BINARY	0
#endif

#define	OK		0
#define	ERR		(-1)

#define	TRUE		1
#define	FALSE		0

/* -------------------- ���[�e�B���e�B�[�֐� -------------------- */ 

#define	LOG_FILE	"ebd2html.log"	/* ���O�t�@�C��			*/

#define	MAX_WORD	256		/* �P��̍ő咷			*/

/*
 * 2�o�C�g�L����1�o�C�g�L���ϊ��\(0x8140�`0x8197)
 */
int zen2han[] = {
    ' ', ERR, ERR, ',', '.', ERR, ':', ';',
    '?', '!', ERR, ERR, ERR, ERR, ERR, '^',
    '~', '_', ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, '/','\\',
    '~', '|', '|', ERR, ERR,'\'','\'', '"',
    '"', '(', ')', '[', ']', '[', ']', '{',
    '}', ERR, ERR, ERR, ERR, ERR, ERR, ERR,
    ERR, ERR, ERR, '+', '-', ERR, ERR, ERR,
    ERR, '=', ERR, '<', '>', ERR, ERR, ERR,
    ERR, ERR, ERR, ERR, ERR, ERR, ERR,'\\',
    '$', ERR, ERR, '%', '#', '&', '*', '@'
};

int	hexval(byte *);
byte	*skipsp(byte *);
byte	*skipch(byte *, int);
byte	*skipstr(byte *, byte *);
byte	*endstr(byte *);
byte	*addstr(byte *, byte *);
int	tohan(byte *);
byte	*getuptos(byte *, byte *, byte *);
byte	*getupto(byte *, byte *, byte);
int	iskanastr(byte *);
void	write_log(char *, ...);
void	message(char *, ...);

/*
 * 2����16�i���������l��Ԃ�
 */
int
hexval(byte *p)
{
    int i, n;

    n = 0;
    for (i = 0; i < 2; i++) {
	n <<= 4;
	if (*p >= '0' && *p <= '9')
	    n += *p - '0';
	else if (*p >= 'a' && *p <= 'f')
	    n += *p - 'a' + 10;
	else if (*p >= 'A' && *p <= 'F')
	    n += *p - 'A' + 10;
	else
	    n = 0;
	p++;
    }
    return n;
}

/*
 * �󔒂�ǂݔ�΂��A���̈ʒu��Ԃ�
 */
byte *
skipsp(byte *str)
{
    while (*str == ' ' || *str == '\t')
	str++;
    return str;
}

/*
 * �w�蕶���܂œǂݔ�΂��A���̎��̈ʒu��Ԃ�
 */
byte *
skipch(byte *str, int ch)
{
    while (*str && *str != ch) {
	if (*str & 0x80)
	    str++;
	str++;
    }
    if (*str == ch)
	str++;
    return str;
}

/*
 * �w�蕶����܂œǂݔ�΂��A���̎��̈ʒu��Ԃ�
 * (�����񂪂Ȃ���Ζ����̈ʒu��Ԃ�)
 */
byte *
skipstr(byte *str, byte *key)
{
    byte *p;

    p = strstr(str, key);
    if (p)
	return p + strlen(key);
    return endstr(str);
}

/*
 * ������̖����̈ʒu��Ԃ�
 */
byte *
endstr(byte *str)
{
    while (*str)
	str++;
    return str;
}

/*
 * �o�b�t�@�ɕ������ǉ����A���̖�����Ԃ�(NUL�ŏI���͂��Ȃ�)
 */
byte *
addstr(byte *dst, byte *str)
{
    while (*str)
	*dst++ = *str++;
    return dst;
}

/*
 * 2�o�C�g�p������1�o�C�g�p�����ɕϊ�����
 */
int
tohan(byte *str)
{
    int high, low;

    high = str[0];
    low = str[1];
    if (high == 0x82) {
	if (low >= 0x4f && low <= 0x58)
	    return '0' + low - 0x4f;
	if (low >= 0x60 && low <= 0x79)
	    return 'A' + low - 0x60;
	if (low >= 0x81 && low <= 0x9a)
	    return 'a' + low - 0x81;
	return ERR;
    }
    if (high == 0x81) {
	if (low >= 0x40 && low <= 0x97)
	    return zen2han[low - 0x40];
	return ERR;
    }
    return ERR;
}

/*
 * ��؂蕶���Z�b�g�܂ł̒P���؂�o���A��؂蕶���̈ʒu��Ԃ�
 */
byte *
getuptos(byte *buf, byte *word, byte *stops)
{
    byte *p, *q;

    p = buf;
    q = word;
    while (*p && *p != '\r' && *p != '\n') {
	if (strchr(stops, *p))
	    break;
	if (*p & 0x80)
	    *q++ = *p++;
	*q++ = *p++;
    }
    *q = '\0';
    return p;
}

/*
 * ��؂蕶���܂ł̒P���؂�o���A��؂蕶���̈ʒu��Ԃ�
 */
byte *
getupto(byte *buf, byte *word, byte stop)
{
    byte *p, *q;

    p = buf;
    q = word;
    while (*p && *p != stop && *p != '\r' && *p != '\n') {
	if (*p & 0x80)
	    *q++ = *p++;
	*q++ = *p++;
    }
    *q = '\0';
    return p;
}

/*
 * �����񂪂Ђ炪��/�J�^�J�i/�����݂̂ō\������Ă��邩?
 */
int
iskanastr(byte *str)
{
    while (*str) {
	if (str[0] == 0x81 && str[1] == 0x5b ||
	    str[0] == 0x82 && str[1] >= 0x9f ||
	    str[0] == 0x83 && str[1] <= 0x96) {
	    str += 2;
	    continue;
	}
	return FALSE;
    }
    return TRUE;
}

/*
 * ���b�Z�[�W�����O�t�@�C���ɏ���
 */
void
write_log(char *fmt, ...)
{
    char buf[BUFSIZ];
    FILE *fp;
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    if ((fp = fopen(LOG_FILE, "a")) == NULL)
	return;
    fputs(buf, fp);
    fclose(fp);
}

/*
 * ���b�Z�[�W��\�����A���O�t�@�C���ɂ�����
 */
void
message(char *fmt, ...)
{
    char buf[BUFSIZ];
    FILE *fp;
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    fputs(buf, stdout);
    fflush(stdout);
    if ((fp = fopen(LOG_FILE, "a")) == NULL)
	return;
    fputs(buf, fp);
    fclose(fp);
}

/* -------------------- ���C�� -------------------- */ 

#define	MAX_PATH	1024		/* �p�X���̍ő咷		*/
#define	MAX_DLINE	1024		/* �f�[�^�t�@�C���̍ő�s��	*/
#define	MAX_HLINE	256		/* HTML�t�@�C���̂��悻�̍ő咷	*/
#define	MAX_BUFF	65535		/* �{���f�[�^�s�̍ő咷(64KB)	*/

#define	FKINDEX_FILE	"fkindex.txt"	/* ���ȃC���f�b�N�X�_���v�f�[�^	*/
#define	FKINDEX_WORK	"fkindex.tmp"	/* ���ȃC���f�b�N�X�ꎞ�t�@�C��	*/
#define	FKINDEX_DATA	"fkindex.dat"	/* �\�[�g�ς݂��ȃC���f�b�N�X	*/
#define	FKTITLE_FILE	"fktitle.txt"	/* ���Ȍ��o���_���v�f�[�^	*/
#define	FKTITLE_DATA	"fktitle.dat"	/* ���Ȍ��o����ƃo�C�i���f�[�^	*/
#define	FHINDEX_FILE	"fhindex.txt"	/* �\�L�C���f�b�N�X�_���v�f�[�^	*/
#define	FHINDEX_WORK	"fhindex.tmp"	/* �\�L�C���f�b�N�X�ꎞ�t�@�C��	*/
#define	FHINDEX_DATA	"fhindex.dat"	/* �\�[�g�ςݕ\�L�C���f�b�N�X	*/
#define	FHTITLE_FILE	"fhtitle.txt"	/* �\�L���o���_���v�f�[�^	*/
#define	FHTITLE_DATA	"fhtitle.dat"	/* �\�L���o����ƃo�C�i���f�[�^	*/
#define	FAINDEX_FILE	"faindex.txt"	/* �p���C���f�b�N�X�_���v�f�[�^	*/
#define	FAINDEX_WORK	"faindex.tmp"	/* �p���C���f�b�N�X�ꎞ�t�@�C��	*/
#define	FAINDEX_DATA	"faindex.dat"	/* �\�[�g�ς݉p���C���f�b�N�X	*/
#define	FATITLE_FILE	"fatitle.txt"	/* �p�����o���_���v�f�[�^	*/
#define	FATITLE_DATA	"fatitle.dat"	/* �p�����o����ƃo�C�i���f�[�^	*/
#define	ZGAIJI_FILE	"zgaiji.txt"	/* �S�p16�h�b�g�O���_���v�f�[�^	*/
#define	HGAIJI_FILE	"hgaiji.txt"	/* ���p16�h�b�g�O���_���v�f�[�^	*/
#define	GFONT_FILE	"Gaiji.xml"	/* EBStudio�p�O���}�b�v�t�@�C��	*/
#define	GMAP_FILE	"GaijiMap.xml"	/* EBStudio�p�O���t�H���g	*/
#define	HONMON_FILE	"honmon.txt"	/* �{���_���v�f�[�^		*/
#define	INI_FILE	"ebd2html.ini"	/* ebd2html�ݒ�t�@�C��		*/

typedef struct index_t {		/* �C���f�b�N�X/���o���f�[�^	*/
    dword	dblk;			/* �{���u���b�N�ԍ�		*/
    word	dpos;			/* �{���u���b�N���ʒu		*/
    dword	tblk;			/* ���o���u���b�N�ԍ�		*/
    word	tpos;			/* ���o���u���b�N���ʒu		*/
    byte	str[MAX_WORD];		/* �C���f�b�N�X������		*/
    byte	title[MAX_WORD];	/* ���o��������			*/
} INDEX;

typedef struct honmon_t {		/* �{���f�[�^			*/
    dword	dblk;			/* �{���u���b�N�ԍ�		*/
    word	dpos;			/* �{���u���b�N���ʒu		*/
    byte	buf[MAX_BUFF];		/* �{���e�L�X�g			*/
} HONMON;

byte	*base_path = "";		/* EBStudio��f�B���N�g��	*/
byte	*out_path = "";			/* EBStudio�o�̓f�B���N�g��	*/
byte	*sort_cmd = "";			/* �\�[�g�R�}���h�̃p�X		*/
int	auto_kana = 0;			/* �\�LINDEX���炩��INDEX�𐶐�	*/
int	eb_type = 0;			/* 0:EPWING, 1:�d�q�u�b�N	*/
byte	*book_title = "";		/* ���Ѓ^�C�g��			*/
byte	*book_type = "";		/* ���Ў��			*/
byte	*book_dir = "";			/* ���Ѓf�B���N�g����		*/
byte	*html_file = "";		/* ���������HTML�t�@�C����	*/
byte	*ebs_file = "";			/* ���������EBS�t�@�C����	*/
time_t	start_time;			/* �J�n����			*/
time_t	stop_time;			/* �I������			*/
int	zg_start_unicode;		/* �S�p�O���J�nUnicode�R�[�h	*/
int	hg_start_unicode;		/* ���p�O���J�nUnicode�R�[�h	*/
int	zg_start_ebhigh;		/* �S�p�O���J�nebcode���byte	*/
int	hg_start_ebhigh;		/* ���p�O���J�nebcode���byte	*/
int	zg_orig_ebhigh;		/* ���f�[�^�̑S�p�O���J�n�R�[�h���byte	*/
int	zg_orig_eblow;		/* ���f�[�^�̑S�p�O���J�n�R�[�h����byte	*/
int	hg_orig_ebhigh;		/* ���f�[�^�̔��p�O���J�n�R�[�h���byte	*/
int	hg_orig_eblow;		/* ���f�[�^�̔��p�O���J�n�R�[�h����byte	*/
dword	fktitle_start_block;		/* ���Ȍ��o���J�n�u���b�N�ԍ�	*/
dword	fhtitle_start_block;		/* �\�L���o���J�n�u���b�N�ԍ�	*/
dword	fatitle_start_block;		/* �p�����o���J�n�u���b�N�ԍ�	*/
int	gen_kana;			/* ���ȃC���f�b�N�X�����	*/
int	gen_hyoki;			/* �\�L�C���f�b�N�X�����	*/
int	gen_alpha;			/* �p���C���f�b�N�X�����	*/
int	have_auto_kana;			/* auto_kana�����ꂪ����	*/

int	generate_gaiji_file(void);
byte	*gstr(byte *, int);
byte	*conv_title(byte *, byte *);
int	convert_index_data(FILE *, FILE *);
int	convert_title_data(FILE *, int);
int	generate_work_file(void);
FILE	*html_newfile(void);
int	html_close(FILE *);
INDEX	*read_index_data(FILE *, int, dword, INDEX *);
HONMON	*read_honmon_data(FILE *, HONMON *);
int	compare_position(INDEX *, HONMON *);
byte	*conv_honmon(byte *, byte *);
byte	*skipindent(byte *, int *);
byte	*indentstr(int);
int	generate_html_file(void);
int	generate_ebs_file(void);
int	parse_ini_file(void);
int	work_directory(void);
int	set_sort_command(void);
int	init(char *);
void	term(int);
int	main(int, char **);

/*
 * �O���_���v�t�@�C������Gaiji.xml��GaijiMap.xml�����
 */
int
generate_gaiji_file(void)
{
    int first, unicode, ebhigh, eblow;
    byte *p, buf[MAX_DLINE];
    FILE *fp, *ffp, *mfp;

    sprintf(buf, "%s%s", base_path, GMAP_FILE);
    if ((mfp = fopen(buf, "w")) == NULL) {
	message("�O���}�b�v�t�@�C�� %s ���V�K�쐬�ł��܂���\n", buf);
	return ERR;
    }
    sprintf(buf, "%s%s", base_path, GFONT_FILE);
    if ((ffp = fopen(buf, "w")) == NULL) {
	message("�O���t�H���g�t�@�C�� %s ���V�K�쐬�ł��܂���\n", buf);
	return ERR;
    }
    fprintf(mfp, "<?xml version=\"1.0\" encoding=\"Shift_JIS\"?>\n");
    fprintf(mfp, "<gaijiSet>\n");
    fprintf(ffp, "<?xml version=\"1.0\" encoding=\"Shift_JIS\"?>\n");
    fprintf(ffp, "<gaijiData xml:space=\"preserve\">\n");
    message("�O���t�@�C���𐶐����Ă��܂�... ");
    first = TRUE;
    zg_start_unicode = 0xe000;
    zg_start_ebhigh = 0xa1;
    if ((fp = fopen(ZGAIJI_FILE, "r")) != NULL) {
	unicode = zg_start_unicode;
	ebhigh = zg_start_ebhigh;
	eblow = 0x21;
	while (fgets(buf, MAX_DLINE, fp) != NULL) {
	    if (*buf == ' ' || *buf == '#') {
		fputs(buf, ffp);
		continue;
	    }
	    if (!strncmp(buf, "<fontSet", 8)) {
		p = strstr(buf, "start=");
		zg_orig_ebhigh = hexval(p+7);
		zg_orig_eblow = hexval(p+9);
	    }
	    if (strncmp(buf, "<fontData", 9))
		continue;
	    if (first) {
		fprintf(ffp, "<fontSet size=\"16X16\" start=\"%02X%02X\">\n",
		    ebhigh, eblow);
		first = FALSE;
	    } else
		fprintf(ffp, "</fontData>\n");
	    fprintf(ffp, "<fontData ebcode=\"%02X%02X\" unicode=\"#x%04X\">\n",
		ebhigh, eblow, unicode);
	    fprintf(mfp, "<gaijiMap unicode=\"#x%04X\" ebcode=\"%02X%02X\"/>\n",
		unicode, ebhigh, eblow);
	    unicode++;
	    if (eblow < 0x7e) {
		eblow++;
	    } else {
		eblow = 0x21;
		ebhigh++;
	    }
	}
	fclose(fp);
	if (!first) {
	    fprintf(ffp, "</fontData>\n");
	    fprintf(ffp, "</fontSet>\n");
	}
    }
    first = TRUE;
    hg_start_unicode = unicode;
    hg_start_ebhigh = ebhigh + 1;
    if ((fp = fopen(HGAIJI_FILE, "r")) != NULL) {
	unicode = hg_start_unicode;
	ebhigh = hg_start_ebhigh;
	eblow = 0x21;
	while (fgets(buf, MAX_DLINE, fp) != NULL) {
	    if (*buf == ' ' || *buf == '#') {
		fputs(buf, ffp);
		continue;
	    }
	    if (!strncmp(buf, "<fontSet", 8)) {
		p = strstr(buf, "start=");
		hg_orig_ebhigh = hexval(p+7);
		hg_orig_eblow = hexval(p+9);
	    }
	    if (strncmp(buf, "<fontData", 9))
		continue;
	    if (first) {
		fprintf(ffp, "<fontSet size=\"8X16\" start=\"%02X%02X\">\n",
		    ebhigh, eblow);
		first = FALSE;
	    } else
		fprintf(ffp, "</fontData>\n");
	    fprintf(ffp, "<fontData ebcode=\"%02X%02X\" unicode=\"#x%04X\">\n",
		ebhigh, eblow, unicode);
	    fprintf(mfp, "<gaijiMap unicode=\"#x%04X\" ebcode=\"%02X%02X\"/>\n",
		unicode, ebhigh, eblow);
	    unicode++;
	    if (eblow < 0x7e) {
		eblow++;
	    } else {
		eblow = 0x21;
		ebhigh++;
	    }
	}
	fclose(fp);
	if (!first) {
	    fprintf(ffp, "</fontData>\n");
	    fprintf(ffp, "</fontSet>\n");
	}
    }
    fprintf(ffp, "</gaijiData>\n");
    fprintf(mfp, "</gaijiSet>\n");
    fclose(ffp);
    fclose(mfp);
    message("�I�����܂���\n");
    return OK;
}

/*
 * ��������O����Unicode�\�L�ɕϊ�����
 */
byte *
gstr(byte *str, int halfwidth)
{
    int high, low, code;
    static byte buf[MAX_WORD];

    if (*str == '<')
	str++;
    high = hexval(str);
    low = hexval(str+2);
    if (high < 0xa1) {
	sprintf(buf, "&#x%02X;&#x%02X;", high, low);
	return buf;
    }
    if (halfwidth) {
	code = hg_start_unicode;
	if (low < hg_orig_eblow) {
	    low += 94;
	    high--;
	}
	code += (high - hg_orig_ebhigh) * 94 + (low - hg_orig_eblow);
    } else {
	code = zg_start_unicode;
	if (low < zg_orig_eblow) {
	    low += 94;
	    high--;
	}
	code += (high - zg_orig_ebhigh) * 94 + (low - zg_orig_eblow);
    }
    sprintf(buf, "&#x%04X;", code);
    return buf;
}

/*
 * ���o����������V�t�gJIS������ɕϊ�����
 */
byte *
conv_title(byte *dst, byte *src)
{
    int n, halfwidth;
    byte *p, *q;

    halfwidth = FALSE;
    p = src;
    q = dst;
    while (*p) {
	if (*p == '<') {
	    if (p[1] >= 'A') {
		/*
		 * �O��
		 */
		q = addstr(q, gstr(p, halfwidth));
	    } else if (!strncmp(p+1, "1F04", 4)) {
		/*
		 * ���p�J�n
		 */
		halfwidth = TRUE;
	    } else if (!strncmp(p+1, "1F05", 4)) {
		/*
		 * ���p�I��
		 */
		halfwidth = FALSE;
	    } else {
		/*
		 * ���̑��̃^�O�͂Ƃ肠��������
		 */
	    }
	    p += 6;
	    continue;
	}
	if (halfwidth && (n = tohan(p)) != ERR) {
	    switch (n) {
	    case '<':
		q = addstr(q, "&lt;");
		break;
	    case '>':
		q = addstr(q, "&gt;");
		break;
	    case '&':
		q = addstr(q, "&amp;");
		break;
	    case '"':
		q = addstr(q, "&quot;");
		break;
	    case '\'':
		q = addstr(q, "&apos;");
		break;
	    default:
		*q++ = n;
		break;
	    }
	    p += 2;
	} else {
	    *q++ = *p++;
	    *q++ = *p++;
	}
    }
    *q = '\0';
    return dst;
}

/*
 * �C���f�b�N�X�f�[�^��ϊ�����
 */
int
convert_index_data(FILE *ifp, FILE *ofp)
{
    int complex, n, len, firsterr;
    dword dblk, tblk;
    word dpos, tpos;
    byte *p, buf[MAX_DLINE], str[MAX_WORD], tmp[MAX_WORD];

    firsterr = TRUE;
    complex = FALSE;
    while ((p = fgets(buf, MAX_DLINE, ifp)) != NULL) {
	if (strncmp(p, "ID=", 3))
	    continue;
	if (!strncmp(p+3, "C0", 2))
	    break;
	if (!strncmp(p+3, "D0", 2)) {
	    complex = TRUE;
	    break;
	}
    }
    if (p == NULL)
	return ERR;
    while ((p = fgets(buf, MAX_DLINE, ifp)) != NULL) {
	if (*p == '\r' || *p == '\n' ||
	    !strncmp(p, "block#", 6) ||
	    !strncmp(p, "ID=", 3)) {
	    continue;
	}
	if (complex) {
	    if (!strncmp(p, "80:", 3))
		continue;
	    if (!strncmp(p, "C0:", 3) || !strncmp(p, "00:", 3))
		p += 3;
	}
	if (*p == '[') {
	    /*
	     * �����ꂪ����ہB�O����?
	     * (�L������ܔł̕\�L�C���f�b�N�X�����ɂ���)
	     */
	    continue;
	}
	p = getupto(p, tmp, '[');
	n = sscanf(p, "[%d]\t[%lx:%x][%lx:%x]",
	    &len, &dblk, &dpos, &tblk, &tpos);
	if (n != 5) {
	    if (firsterr) {
		write_log("\n");
		firsterr = FALSE;
	    }
	    write_log("�s���ȃC���f�b�N�X�s: %s", buf);
	    continue;
	}
	if (dpos == 0x0800) {
	    dpos = 0;
	    dblk++;
	};
	if (tpos == 0x0800) {
	    tpos = 0;
	    tblk++;
	}
	fprintf(ofp, "%08lX|%04X|%08lX|%04X|%s|\n",
	    dblk, dpos, tblk, tpos, conv_title(str, tmp));
    }
    return OK;
}

/*
 * ���o���f�[�^��ϊ�����
 */
int
convert_title_data(FILE *ifp, int ofd)
{
    int n, first, firsterr;
    long pos;
    dword tblk, start_block;
    word tpos;
    byte *p, *q, buf[MAX_DLINE], str[MAX_WORD];

    first = TRUE;
    firsterr = TRUE;
    while ((p = fgets(buf, MAX_DLINE, ifp)) != NULL) {
	if (*p == '\r' || *p == '\n' || !strncmp(p, "[ID=", 4))
	    continue;
	if (sscanf(p, "[%lx:%x]", &tblk, &tpos) != 2) {
	    if (firsterr) {
		write_log("\n");
		firsterr = FALSE;
	    }
	    write_log("�s���Ȍ��o���s: %s", buf);
	    continue;
	}
	p = skipch(p, ']');
	if (*p == '\r' || *p == '\n' ||
	    !strncmp(p, "<1F02>", 6) ||
	    !strncmp(p, "<1F03>", 6)) {
	    continue;
	}
	if (first) {
	    start_block = tblk;
	    first = FALSE;
	}
	q = p;
	while (*q && *q != '\r' && *q != '\n')
	    q++;
	*q = '\0';
	n = strlen(p);
	if (n > 6 && !strncmp(&p[n-6], "<1F0A>", 6))
	    p[n-6] = '\0';
	conv_title(str, p);
	pos = (long)((tblk - start_block) * 2048 + tpos) * 4;
	n = strlen(str) + 1;
	if (lseek(ofd, pos, SEEK_SET) < 0)
	    return ERR;
	if (write(ofd, str, n) != n)
	    return ERR;
    }
    return (int)start_block;
}

/*
 * �C���f�b�N�X����ь��o���̍�ƃf�[�^�t�@�C���𐶐�����
 */
int
generate_work_file(void)
{
    int ofd, n;
    FILE *ifp, *ofp;
    byte buf[MAX_PATH];
    struct stat st;

    if (stat(FKINDEX_FILE, &st) == 0) {
	if ((ifp = fopen(FKINDEX_FILE, "r")) == NULL) {
	    message("���ȃC���f�b�N�X�t�@�C�� %s ���I�[�v���ł��܂���\n",
		FKINDEX_FILE);
	    return ERR;
	}
	if ((ofp = fopen(FKINDEX_WORK, "w")) == NULL) {
	    message("���ȃC���f�b�N�X��ƃt�@�C�� %s ���V�K�쐬�ł��܂���\n",
		FKINDEX_WORK);
	    return ERR;
	}
	message("���ȃC���f�b�N�X�f�[�^��ϊ����Ă��܂�...");
	if (convert_index_data(ifp, ofp) == ERR) {
	    message("���ȃC���f�b�N�X�f�[�^�̕ϊ��Ɏ��s���܂���\n");
	    return ERR;
	}
	message(" �I�����܂���\n");
	fclose(ifp);
	fclose(ofp);
	message("���ȃC���f�b�N�X�f�[�^���\�[�g���Ă��܂�...");
	sprintf(buf, "%s %s > %s", sort_cmd, FKINDEX_WORK, FKINDEX_DATA);
	system(buf);
	unlink(FKINDEX_WORK);
	message(" �I�����܂���\n");
    }
    if (stat(FKTITLE_FILE, &st) == 0) {
	if ((ifp = fopen(FKTITLE_FILE, "r")) == NULL) {
	    message("���Ȍ��o���t�@�C�� %s ���I�[�v���ł��܂���\n",
		FKTITLE_FILE);
	    return ERR;
	}
	if ((ofd = open(FKTITLE_DATA, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0644)) < 0) {
	    message("���Ȍ��o���f�[�^�t�@�C�� %s ���V�K�쐬�ł��܂���\n",
		FKTITLE_DATA);
	    return ERR;
	}
	message("���Ȍ��o���f�[�^�𐶐����Ă��܂�...");
	if ((n = convert_title_data(ifp, ofd)) == ERR) {
	    message("���Ȍ��o���f�[�^�̐����Ɏ��s���܂���\n");
	    return ERR;
	}
	fktitle_start_block = n;
	message(" �I�����܂���\n");
	fclose(ifp);
	close(ofd);
    }
    if (stat(FHINDEX_FILE, &st) == 0) {
	if ((ifp = fopen(FHINDEX_FILE, "r")) == NULL) {
	    message("�\�L�C���f�b�N�X�t�@�C�� %s ���I�[�v���ł��܂���\n",
		FHINDEX_FILE);
	    return ERR;
	}
	if ((ofp = fopen(FHINDEX_WORK, "w")) == NULL) {
	    message("�\�L�C���f�b�N�X��ƃt�@�C�� %s ���V�K�쐬�ł��܂���\n",
		FHINDEX_WORK);
	    return ERR;
	}
	message("�\�L�C���f�b�N�X�f�[�^��ϊ����Ă��܂�...");
	if (convert_index_data(ifp, ofp) == ERR) {
	    message("�\�L�C���f�b�N�X�f�[�^�̕ϊ��Ɏ��s���܂���\n");
	    return ERR;
	}
	message(" �I�����܂���\n");
	fclose(ifp);
	fclose(ofp);
	message("�\�L�C���f�b�N�X�f�[�^���\�[�g���Ă��܂�...");
	sprintf(buf, "%s %s > %s", sort_cmd, FHINDEX_WORK, FHINDEX_DATA);
	system(buf);
	unlink(FHINDEX_WORK);
	message(" �I�����܂���\n");
    }
    if (stat(FHTITLE_FILE, &st) == 0) {
	if ((ifp = fopen(FHTITLE_FILE, "r")) == NULL) {
	    message("�\�L���o���t�@�C�� %s ���I�[�v���ł��܂���\n",
		FHTITLE_FILE);
	    return ERR;
	}
	if ((ofd = open(FHTITLE_DATA, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0644)) < 0) {
	    message("�\�L���o���f�[�^�t�@�C�� %s ���V�K�쐬�ł��܂���\n",
		FHTITLE_DATA);
	    return ERR;
	}
	message("�\�L���o����ƃf�[�^�𐶐����Ă��܂�...");
	if ((n = convert_title_data(ifp, ofd)) == ERR) {
	    message("�\�L���o���f�[�^�̐����Ɏ��s���܂���\n");
	    return ERR;
	}
	fhtitle_start_block = n;
	message(" �I�����܂���\n");
	fclose(ifp);
	close(ofd);
    }
    if (stat(FAINDEX_FILE, &st) == 0) {
	if ((ifp = fopen(FAINDEX_FILE, "r")) == NULL) {
	    message("�p���C���f�b�N�X�t�@�C�� %s ���I�[�v���ł��܂���\n",
		FAINDEX_FILE);
	    return ERR;
	}
	if ((ofp = fopen(FAINDEX_WORK, "w")) == NULL) {
	    message("�p���C���f�b�N�X��ƃt�@�C�� %s ���V�K�쐬�ł��܂���\n",
		FAINDEX_WORK);
	    return ERR;
	}
	message("�p���C���f�b�N�X�f�[�^��ϊ����Ă��܂�...");
	if (convert_index_data(ifp, ofp) == ERR) {
	    message("�p���C���f�b�N�X�f�[�^�̕ϊ��Ɏ��s���܂���\n");
	    return ERR;
	}
	message(" �I�����܂���\n");
	fclose(ifp);
	fclose(ofp);
	message("�p���C���f�b�N�X�f�[�^���\�[�g���Ă��܂�...");
	sprintf(buf, "%s %s > %s", sort_cmd, FAINDEX_WORK, FAINDEX_DATA);
	system(buf);
	unlink(FAINDEX_WORK);
	message(" �I�����܂���\n");
    }
    if (stat(FATITLE_FILE, &st) == 0) {
	if ((ifp = fopen(FATITLE_FILE, "r")) == NULL) {
	    message("�p�����o���t�@�C�� %s ���I�[�v���ł��܂���\n",
		FATITLE_FILE);
	    return ERR;
	}
	if ((ofd = open(FATITLE_DATA, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0644)) < 0) {
	    message("�p�����o���f�[�^�t�@�C�� %s ���V�K�쐬�ł��܂���\n",
		FATITLE_DATA);
	    return ERR;
	}
	message("�p�����o����ƃf�[�^�𐶐����Ă��܂�...");
	if ((n = convert_title_data(ifp, ofd)) == ERR) {
	    message("�p�����o���f�[�^�̐����Ɏ��s���܂���\n");
	    return ERR;
	}
	fatitle_start_block = n;
	message(" �I�����܂���\n");
	fclose(ifp);
	close(ofd);
    }
    return OK;
}

/*
 * HTML�t�@�C����V�K�쐬����
 */
FILE *
html_newfile(void)
{
    byte buf[MAX_PATH];
    FILE *fp;

    sprintf(buf, "%s%s", base_path, html_file);
    if ((fp = fopen(buf, "w")) == NULL) {
	message("HTML�t�@�C�� %s ���V�K�쐬�ł��܂���\n", buf);
	return NULL;
    }
    fprintf(fp, "<html>\n");
    fprintf(fp, "<head>\n");
    fprintf(fp, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=x-sjis\">\n");
    fprintf(fp, "<meta name=\"GENERATOR\" content=\"%s %s %s\">\n",
	progname, version, date);
    fprintf(fp, "<title>Dictionary</title>\n");
    fprintf(fp, "</head>\n");
    fprintf(fp, "<body>\n");
    fprintf(fp, "<dl>\n");
    return fp;
}

/*
 * HTML�t�@�C�����N���[�Y����
 */
int
html_close(FILE *fp)
{
    fprintf(fp, "</dl>\n");
    fprintf(fp, "</body>\n");
    fprintf(fp, "</html>\n");
    return fclose(fp);
}

/*
 * �C���f�b�N�X�ƌ��o����1���ǂ�
 */
INDEX *
read_index_data(FILE *fp, int fd, dword topblk, INDEX *ip)
{
    int n;
    long pos;
    byte *p, buf[MAX_DLINE];

    if ((p = fgets(buf, MAX_DLINE, fp)) == NULL)
	return NULL;
    n = sscanf(p, "%lx|%x|%lx|%x", &ip->dblk, &ip->dpos, &ip->tblk, &ip->tpos);
    if (n != 4)
	return NULL;
    p += 28;	/* "xxxxxxxx|xxxx|yyyyyyyy|yyyy|"���X�L�b�v */
    getupto(p, ip->str, '|');
    if (ip->dblk == ip->tblk && ip->dpos == ip->tpos) {
	/*
	 * ���o���f�[�^�͖{���̌��o���s�����p����(�L������ܔ�)
	 */
	*ip->title = '\0';
    } else {
	pos = (long)((ip->tblk - topblk) * 2048 + ip->tpos) * 4;
	if (lseek(fd, pos, SEEK_SET) < 0)
	    return NULL;
	if (read(fd, ip->title, MAX_WORD) <= 0)
	    return NULL;
    }
    return ip;
}

/*
 * �{����1�s�ǂ�
 */
HONMON *
read_honmon_data(FILE *fp, HONMON *hp)
{
    byte *p, *q, buf[MAX_BUFF];

    if (fgets(buf, MAX_BUFF, fp) == NULL)
	return NULL;
    if (sscanf(buf, "[%lx:%x]", &hp->dblk, &hp->dpos) != 2) {
	hp->dblk = 0L;
	hp->dpos = 0;
	return hp;
    }
    if (hp->dpos == 0x0800) {
	hp->dpos = 0;
	hp->dblk++;
    }
    p = skipch(buf, ']');
    q = p;
    while (*q && *q != '\r' && *q != '\n')
	q++;
    *q = '\0';
    strcpy(hp->buf, p);
    return hp;
}

/*
 * �C���f�b�N�X�̎Q�Ɛ�Ɩ{���f�[�^�̈ʒu�֌W���r����
 */
int
compare_position(INDEX *ip, HONMON *dp)
{
    byte *p;
    dword ipos, dpos;

    ipos = ip->dblk * 2048 + ip->dpos;
    dpos = dp->dblk * 2048 + dp->dpos;
    /*
     * �{���̍s������
     *   <1F09><xxxx>
     *   <1F41><xxxx>
     *   <1F61>
     * �̕��т��A�����Ă��āA
     * �C���f�b�N�X�̎Q�Ɛ悪���̒���̏ꍇ��
     * �C���f�b�N�X�����̍s�����Q�Ƃ��Ă���Ƃ݂Ȃ�
     * (�W�[�j�A�X�p�a���T�Ȃ�)
     */
    if (ipos < dpos)
	return -1;
    p = dp->buf;
    while (*p && ipos > dpos) {
	if (!strncmp(p, "<1F09>", 6)) {
	    p += 12;
	    dpos += 4;
	    continue;
	}
	if (!strncmp(p, "<1F41>", 6)) {
	    p += 12;
	    dpos += 4;
	}
	if (!strncmp(p, "<1F61>", 6)) {
	    p += 6;
	    dpos += 2;
	}
	break;
    }
    if (ipos > dpos)
	return 1;
    return 0;
}

/*
 * �{���f�[�^��ϊ�����
 */
byte *
conv_honmon(byte *dst, byte *src)
{
    int n, halfwidth;
    dword dblk;
    word dpos;
    byte *p, *q, *r, *linetop;
    byte endtag[MAX_WORD], buf[MAX_DLINE], tmp[MAX_DLINE];

    halfwidth = FALSE;
    p = src;
    q = dst;
    linetop = q;
    while (*p) {
	if (*p == '<') {
	    if (p[1] == '1' && p[2] == 'F') {	/* 1Fxx */
		switch (p[3]) {
		case '0':
		    switch (p[4]) {
		    case '4':	/* 1F04: ���p�J�n */
			halfwidth = TRUE;
			p += 6;
			break;
		    case '5':	/* 1F05: ���p�I�� */
			halfwidth = FALSE;
			p += 6;
			break;
		    case '6':	/* 1F06: ���Y�����J�n */
			q = addstr(q, "<sub>");
			p += 6;
			break;
		    case '7':	/* 1F07: ���Y�����I�� */
			q = addstr(q, "</sub>");
			p += 6;
			break;
		    case 'A':	/* 1F0A: ���s */
			q = addstr(q, "<br>");
			p += 6;
			break;
		    case 'E':	/* 1F0E: ��Y�����J�n */
			q = addstr(q, "<sup>");
			p += 6;
			break;
		    case 'F':	/* 1F0F: ��Y�����I�� */
			q = addstr(q, "</sup>");
			p += 6;
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		case '1':
		    switch (p[4]) {
		    case '0':	/* 1F10: �����֎~�J�n */
			q = addstr(q, "<nobr>");
			p += 6;
			break;
		    case '1':	/* 1F11: �����֎~�I�� */
			q = addstr(q, "</nobr>");
			p += 6;
			break;
		    case '4':	/* 1F14 ... 1F15: �F���{ */
			/*
			 * "[�F���{]"�Œu��������
			 */
			p = skipstr(p+6, "<1F15>");
			q = addstr(q, "[�F���{]");
			break;
		    case 'A':	/* 1F1A xxxx: �^�u�ʒu�w�� */
		    case 'B':	/* 1F1B xxxx: �������E���オ��w�� */
		    case 'C':	/* 1F1C xxxx: �Z���^�����O�w�� */
			p += 12;
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		case '3':
		    switch (p[4]) {
		    case '9':	/* 1F39 ... 1F59: �J���[����\�� */
			/*
			 * "[����]"�Œu��������
			 */
			p = skipstr(p+6, "<1F59>");
			q = addstr(q, "[����]");
			break;
		    case 'C':	/* 1F3C ... 1F5C: �C�����C���摜�Q�� */
			/*
			 * "[�}��]"�Œu��������
			 */
			p = skipstr(p+6, "<1F5C>");
			q = addstr(q, "[�}��]");
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		case '4':
		    switch (p[4]) {
		    case '1':	/* 1F41 xxxx: �����L�[�J�n */
			p += 12;
			break;
		    case '2':	/* 1F42 text 1F62[xx:yy]: �ʍ��ڎQ�� */
			p += 6;
			while(!strncmp(p, "<1F42>", 2))p+=6;
			while(!strncmp(p, "��", 2))p+=2;
			r = strstr(p, "<1F62>");
			n = r - p;
			strncpy(buf, p, n);
			buf[n] = '\0';
			p = r + 6;
			sscanf(p, "[%lx:%x]", &dblk, &dpos);
			p = skipch(p, ']');
			sprintf(q, "<a href=\"#%08lX%04X\">%s</a>",
			    dblk, dpos, conv_honmon(tmp, buf));
			q = endstr(q);
			break;
		    case '4':	/* 1F44 ... 1F64[xx:yy]: �}�ŎQ�� */
			/*
			 * "[�}��]"�Œu��������
			 */
			p = skipstr(p+6, "<1F64>");
			p = skipch(p, ']');
			q = addstr(q, "[�}��]");
			break;
		    case 'A':	/* 1F4A ... 1F6A: �����Q�� */
			/*
			 * "[����]"�Œu��������
			 */
			p = skipstr(p+6, "<1F6A>");
			q = addstr(q, "[����]");
			break;
		    case '5':	/* 1F45 ... 1F65: �}�Ō��o�� */
		    case 'B':	/* 1F4B ... 1F6B: �J���[�摜�f�[�^�Q�Q�� */
		    case 'C':	/* 1F4C ... 1F6C: �J���[��ʃf�[�^�Q�Q�� */
		    case 'D':	/* 1F4D ... 1F6D: �J���[��ʕ\�� */
		    case 'F':	/* 1F4F ... 1F6F: �J���[��ʎQ�� */
			/*
			 * "[�}��]"�Œu��������
			 */
			sprintf(endtag, "<1F6%c>", p[4]);
			p = skipstr(p+6, endtag);
			q = addstr(q, "[�}��]");
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		case '6':
		    switch (p[4]) {
		    case '1':	/* 1F61: �����L�[�I�� */
			p += 6;
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		case 'E':
		    switch (p[4]) {
		    case '0':	/* 1FE0 xxxx ... 1FE1: �����C�� */
			p += 12;
			break;
		    default:
			p += 6;
			break;
		    }
		    break;
		default:
		    p += 6;
		    break;
		}
	    } else if (p[1] >= 'A') {
		/*
		 * �O��
		 */
		q = addstr(q, gstr(p, halfwidth));
		p += 6;
	    } else {
		/*
		 * ���̑��̃^�O�͖���
		 */
		p += 6;
	    }
	    continue;
	}
	if (halfwidth && (n = tohan(p)) != ERR) {
	    switch (n) {
	    case '<':
		q = addstr(q, "&lt;");
		break;
	    case '>':
		q = addstr(q, "&gt;");
		break;
	    case '&':
		q = addstr(q, "&amp;");
		break;
	    case '"':
		q = addstr(q, "&quot;");
		break;
	    case '\'':
		q = addstr(q, "&apos;");
		break;
	    default:
		*q++ = n;
		break;
	    }
	    p += 2;
	    if (q - linetop >= MAX_HLINE && n == '.') {
		*q++ = '\n';
		linetop = q;
	    }
	} else {
	    *q++ = *p++;
	    *q++ = *p++;
	    if (q - linetop >= MAX_HLINE && !strncmp(q-2, "�B", 2)) {
		*q++ = '\n';
		linetop = q;
	    }
	}
    }
    if (q >= dst + 4 && !strncmp(q-4, "<br>", 4)) {
	/*
	 * �Ō��<br>�͎̂Ă�
	 */
	q -= 4;
    }
    *q = '\0';
    return dst;
}

/*
 * �u1F41 xxxx �` 1F61�v�܂��́u1F41 xxxx 1F61 1FE0 xxxx �` 1FE1�v��
 * �͂܂ꂽ�^�C�g�������o���A����̈ʒu��Ԃ�
 * �������͂܂ꂽ�͈͂̒�����128�o�C�g�ȏォ
 * <1F61>��������Ȃ��ꍇ�̓^�C�g���Ƃ��Ȃ�
 */
byte *
get_title(byte *str, byte *title)
{
    int n;
    byte *p, *r;

    if (strncmp(str, "<1F41>", 6)) {
	*title = '\0';
	return str;
    }
    p = str + 12;	/* <1F41><xxxx>���X�L�b�v */
    r = strstr(p, "<1F61>");
    if (r == NULL) {
	*title = '\0';
	return str;
    }
    n = r - p;
    if (n >= MAX_WORD) {
	*title = '\0';
	return str;
    }
    strncpy(title, p, n);
    title[n] = '\0';
    p = r + 6;
    if (n == 0 && !strncmp(p, "<1FE0>", 6)) {
	/*
	 * <1F41><xxxx><1F61><1FE0><yyyy> �` <1FE1>�̌`��
	 */
	p += 12;
	r = strstr(p, "<1FE1>");
	if (r == NULL) {
	    *title = '\0';
	    return str;
	}
	n = r - p;
	strncpy(title, p, n);
	title[n] = '\0';
	p = r + 6;
    }
    return p;
}

/*
 * ���͂̎������^�O���X�L�b�v���A�������ʂ�Ԃ�
 */
byte *
skipindent(byte *str, int *indentp)
{
    if (strncmp(str, "<1F09>", 6)) {
	*indentp = 0;
	return str;
    }
    *indentp = hexval(str+7) * 256 + hexval(str+9) - 1;
    return str + 12;
}

/*
 * �o�͗p�������^�O����������
 */
byte *
indentstr(int indent)
{
    static byte buf[32];

    sprintf(buf, "&#x1f09;&#x00;&#x%02x;", indent+1);
    return buf;
}

/*
 * honmon.txt�ƍ�ƃf�[�^�t�@�C����˂����킹��HTML�t�@�C���𐶐�����
 */
int
generate_html_file(void)
{
    int first_dt, needbr, indent, indent2;
    int have_preamble, have_indent, have_indent2;
    int yield_dt, new_content;
    int ktfd, htfd, atfd;
    dword ktop, htop, atop;
    byte *p, *title;
    byte buf[MAX_BUFF], tbuf[MAX_DLINE], tmp[MAX_DLINE];
    byte istr[MAX_WORD], istr2[MAX_WORD];
    FILE *fp, *honfp, *kifp, *hifp, *aifp;
    INDEX *kp, *hp, *ap, kidx, hidx, aidx;
    HONMON *dp, honmon;

    if ((fp = html_newfile()) == NULL)
	return ERR;
    if ((honfp = fopen(HONMON_FILE, "r")) == NULL) {
	message("�{���t�@�C�� %s ���I�[�v���ł��܂���\n", HONMON_FILE);
	return ERR;
    }
    gen_kana = FALSE;
    if ((kifp = fopen(FKINDEX_DATA, "r")) != NULL &&
	(ktfd = open(FKTITLE_DATA, O_RDONLY|O_BINARY)) != -1) {
	gen_kana = TRUE;
    }
    gen_hyoki = FALSE;
    if ((hifp = fopen(FHINDEX_DATA, "r")) != NULL &&
	(htfd = open(FHTITLE_DATA, O_RDONLY|O_BINARY)) != -1) {
	gen_hyoki = TRUE;
    }
    gen_alpha = FALSE;
    if ((aifp = fopen(FAINDEX_DATA, "r")) != NULL &&
	(atfd = open(FATITLE_DATA, O_RDONLY|O_BINARY)) != -1) {
	gen_alpha = TRUE;
    }
    if (!gen_kana && !gen_hyoki && !gen_alpha) {
	message("����/�\�L/�p��������̃C���f�b�N�X/���o��������܂���\n");
	message("HTML�t�@�C���̐����𒆎~���܂�\n");
	return ERR;
    }
    message("HTML�t�@�C���𐶐����Ă��܂�...\n");
    have_auto_kana = FALSE;
    have_preamble = FALSE;
    first_dt = TRUE;
    needbr = FALSE;
    new_content = FALSE;
    kp = NULL;
    hp = NULL;
    ap = NULL;
    if (gen_kana) {
	ktop = fktitle_start_block;
	kp = read_index_data(kifp, ktfd, ktop, &kidx);
    }
    if (gen_hyoki) {
	htop = fhtitle_start_block;
	hp = read_index_data(hifp, htfd, htop, &hidx);
    }
    if (gen_alpha) {
	atop = fatitle_start_block;
	ap = read_index_data(aifp, atfd, atop, &aidx);
    }
    while ((dp = read_honmon_data(honfp, &honmon)) != NULL) {
	if (dp->dblk == 0L || *dp->buf == '\0')
	    continue;
	p = dp->buf;
	if (!strcmp(p, "<1F02>") || !strcmp(p, "<1F03>")) {
	    /*
	     * �����͂ǂ��ɂ����Ă��P�Ƃŏo�͂���
	     */
	    if (needbr) {
		fprintf(fp, "<br>\n");
		needbr = FALSE;
	    }
	    if (!strcmp(p, "<1F02>")) {
		fprintf(fp, "&#x1F02;\n");
		new_content = TRUE;
	    } else {
		fprintf(fp, "&#x1F03;\n");
	    }
	    needbr = FALSE;
	    continue;
	}
	if (new_content) {
	    /*
	     * <1F02>�̒���̍s; �������ɃA���J�[�����
	     * (�����АV�p�a�����T�Ȃ�)
	     */
	    fprintf(fp, "<a name=\"%08lX%04X\"></a>\n", dp->dblk, dp->dpos);
	    new_content = FALSE;
	}
	have_indent = FALSE;
	have_indent2 = FALSE;
	*istr = '\0';
	*istr2 = '\0';
	if (!strncmp(dp->buf, "<1F09>", 6)) {
	    p = skipindent(p, &indent);
	    have_indent = TRUE;
	    strcpy(istr, indentstr(indent));
	}
	if (!strncmp(p, "<1F41>", 6)) {
	    p = get_title(p, tmp);
	    if (!strncmp(p, "<1F09>", 6)) {
		p = skipindent(p, &indent2);
		have_indent2 = TRUE;
		strcpy(istr2, indentstr(indent2));
	    }
	    conv_honmon(tbuf, tmp);
	    conv_honmon(buf, p);
	    title = *tbuf? tbuf: buf;
	} else {
	    *tbuf = '\0';
	    title = conv_honmon(buf, p);
	}
	while (kp && compare_position(kp, dp) < 0) {
	    message("�g���Ȃ����ȃC���f�b�N�X������܂�\n");
	    message("  �{���ʒu=[%08lX:%04X], �C���f�b�N�X=[%08lX:%04X]%s\n",
		dp->dblk, dp->dpos, kp->dblk, kp->dpos, kp->str);
	    kp = read_index_data(kifp, ktfd, ktop, &kidx);
	}
	while (hp && compare_position(hp, dp) < 0) {
	    message("�g���Ȃ��\�L�C���f�b�N�X������܂�\n");
	    message("  �{���ʒu=[%08lX:%04X], �C���f�b�N�X=[%08lX:%04X]%s\n",
		dp->dblk, dp->dpos, hp->dblk, hp->dpos, hp->str);
	    hp = read_index_data(hifp, htfd, htop, &hidx);
	}
	while (ap && compare_position(ap, dp) < 0) {
	    message("�g���Ȃ��p���C���f�b�N�X������܂�\n");
	    message("  �{���ʒu=[%08lX:%04X], �C���f�b�N�X=[%08lX:%04X]%s\n",
		dp->dblk, dp->dpos, ap->dblk, ap->dpos, ap->str);
	    ap = read_index_data(aifp, atfd, atop, &aidx);
	}
	if (kp && compare_position(kp, dp) == 0 ||
	    hp && compare_position(hp, dp) == 0 ||
	    ap && compare_position(ap, dp) == 0) {
	    /*
	     * �C���f�b�N�X����Q�Ƃ���Ă���
	     */
	    if (have_preamble) {
		fprintf(fp, "\n</p>\n");
		have_preamble = FALSE;
	    }
	    if (have_indent && indent > 0 || strlen(title) > MAX_WORD) {
		/*
		 * ���������ꂽ�ʒu���Q�Ƃ���Ă��邩
		 * ���邢�̓^�C�g����128�o�C�g�𒴂��Ă���̂�
		 * ���o���Ƃ��Ă͍̗p���Ȃ�
		 * <dd>�`</dd>���ɎQ�ƈʒu���������Ƃ݂Ȃ��A
		 * �����炵��<p>���n�߂�
		 */
		yield_dt = FALSE;
	    } else {
		/*
		 * �s���ʒu���Q�Ƃ���Ă���
		 * �^�C�g������128�o�C�g�ȉ��Ȃ̂�
		 * �V����<dt id=...>���n�߂�
		 */
		yield_dt = TRUE;
	    }
	    if (yield_dt) {
		if (first_dt)
		    first_dt = FALSE;
		else
		    fprintf(fp, "\n</p></dd>\n");
		fprintf(fp, "<dt id=\"%08lX%04X\">%s</dt>\n",
		    dp->dblk, dp->dpos, title);
	    } else {
		fprintf(fp, "\n</p>\n<p>\n");
	    }
	    while (kp && compare_position(kp, dp) == 0) {
		fprintf(fp, "<key title=\"%s\" type=\"����\">%s</key>\n",
		    *kp->title? kp->title: title, kp->str);
		kp = read_index_data(kifp, ktfd, ktop, &kidx);
	    }
	    while (hp && compare_position(hp, dp) == 0) {
		fprintf(fp, "<key title=\"%s\" type=\"�\�L\">%s</key>\n",
		    *hp->title? hp->title: title, hp->str);
		if (auto_kana && iskanastr(hp->str)) {
		    fprintf(fp, "<key title=\"%s\" type=\"����\">%s</key>\n",
			*hp->title? hp->title: title, hp->str);
		    have_auto_kana = TRUE;
		}
		hp = read_index_data(hifp, htfd, htop, &hidx);
	    }
	    while (ap && compare_position(ap, dp) == 0) {
		fprintf(fp, "<key title=\"%s\" type=\"�\�L\">%s</key>\n",
		    *ap->title? ap->title: title, ap->str);
		ap = read_index_data(aifp, atfd, atop, &aidx);
	    }
	    if (yield_dt) {
		fprintf(fp, "<dd><p>\n");
		if (*tbuf && *buf)
		    fprintf(fp, "%s%s", istr2, buf);
		else
		    fprintf(fp, " ");
	    } else {
		fprintf(fp, "%s%s%s%s", istr, tbuf, istr2, buf);
	    }
	    have_indent = FALSE;
	    have_indent2 = FALSE;
	    *istr = '\0';
	    *istr2 = '\0';
	    needbr = TRUE;
	    continue;
	}
	if (needbr) {
	    fprintf(fp, "<br>\n");
	    needbr = FALSE;
	}
	if (first_dt && !have_preamble) {
	    /*
	     * �ŏ���<dt ...>���O�ɓ��e��������
	     */
	    fprintf(fp, "<p>\n");
	    have_preamble = TRUE;
	}
	fprintf(fp, "%s%s%s%s", istr, tbuf, istr2, buf);
	needbr = TRUE;
    }
    fprintf(fp, "\n");
    if (gen_kana) {
	fclose(kifp);
	close(ktfd);
    }
    if (gen_hyoki) {
	fclose(hifp);
	close(htfd);
    }
    if (gen_alpha) {
	fclose(aifp);
	close(atfd);
    }
    fclose(honfp);
    html_close(fp);
    message("HTML�t�@�C���̐������I�����܂���\n");
    return OK;
}

/*
 * EBStudio��`�t�@�C���𐶐�����
 */
int
generate_ebs_file(void)
{
    byte buf[MAX_PATH];
    FILE *fp;

    sprintf(buf, "%s%s", base_path, ebs_file);
    if ((fp = fopen(buf, "w")) == NULL) {
	message("EBStudio��`�t�@�C�� %s ���V�K�쐬�ł��܂���\n", buf);
	return ERR;
    }
    message("EBS�t�@�C���𐶐����Ă��܂�... ");
    fprintf(fp, "InPath=%s\n", base_path);
    fprintf(fp, "OutPath=%s\n", out_path);
    fprintf(fp, "IndexFile=\n");
    fprintf(fp, "Copyright=\n");
    fprintf(fp, "GaijiFile=$(BASE)\\%s\n", GFONT_FILE);
    fprintf(fp, "GaijiMapFile=$(BASE)\\%s\n", GMAP_FILE);
    fprintf(fp, "EBType=%d\n", eb_type);
    fprintf(fp, "WordSearchHyoki=%d\n", (gen_hyoki || gen_alpha)? 1: 0);
    fprintf(fp, "WordSearchKana=%d\n", (gen_kana || have_auto_kana)? 1: 0);
    fprintf(fp, "EndWordSearchHyoki=%d\n", (gen_hyoki || gen_alpha)? 1: 0);
    fprintf(fp, "EndWordSearchKana=%d\n", (gen_kana || have_auto_kana)? 1: 0);
    fprintf(fp, "KeywordSearch=0\n");
    fprintf(fp, "ComplexSearch=0\n");
    fprintf(fp, "topMenu=0\n");
    fprintf(fp, "singleLine=1\n");
    fprintf(fp, "kanaSep1=�y\n");
    fprintf(fp, "kanaSep2=�z\n");
    fprintf(fp, "hyokiSep=0\n");
    fprintf(fp, "makeFig=0\n");
    fprintf(fp, "inlineImg=0\n");
    fprintf(fp, "paraHdr=0\n");
    fprintf(fp, "ruby=1\n");
    fprintf(fp, "paraBr=0\n");
    fprintf(fp, "subTitle=0\n");
    fprintf(fp, "dfnStyle=0\n");
    fprintf(fp, "srchUnit=1\n");
    fprintf(fp, "linkChar=0\n");
    fprintf(fp, "arrowCode=222A\n");
    fprintf(fp, "eijiPronon=1\n");
    fprintf(fp, "eijiPartOfSpeech=1\n");
    fprintf(fp, "eijiBreak=1\n");
    fprintf(fp, "eijiKana=0\n");
    fprintf(fp, "leftMargin=0\n");
    fprintf(fp, "indent=0\n");
    fprintf(fp, "tableWidth=480\n");
    fprintf(fp, "StopWord=\n");
    fprintf(fp, "delBlank=1\n");
    fprintf(fp, "delSym=1\n");
    fprintf(fp, "delChars=\n");
    fprintf(fp, "refAuto=0\n");
    fprintf(fp, "titleWord=0\n");
    fprintf(fp, "autoWord=0\n");
    fprintf(fp, "autoEWord=0\n");
    fprintf(fp, "HTagIndex=0\n");
    fprintf(fp, "DTTagIndex=1\n");
    fprintf(fp, "dispKeyInSelList=0\n");
    fprintf(fp, "titleOrder=0\n");
    fprintf(fp, "omitHeader=0\n");
    fprintf(fp, "addKana=1\n");
    fprintf(fp, "autoKana=0\n");
    fprintf(fp, "withHeader=0\n");
    fprintf(fp, "optMono=0\n");
    fprintf(fp, "Size=20000;30000;100;3000000;20000;20000;20000;1000;1000;1000;1000\n");
    fprintf(fp, "Book=%s;%s;%s;_;", book_title, book_dir, book_type);
    fprintf(fp, "_;GAI16H00;GAI16F00;_;_;_;_;_;_;\n");
    fprintf(fp, "Source=$(BASE)\\%s;�{��;_;HTML;\n", html_file);
    fclose(fp);
    message("�I�����܂���\n");
    return OK;
}

/*
 * �ݒ�t�@�C����ǂݍ���
 */
int
parse_ini_file(void)
{
    int n, lineno, ret;
    byte *p;
    byte key[MAX_WORD], val[MAX_PATH], buf[BUFSIZ];
    FILE *fp;

    if ((fp = fopen(INI_FILE, "r")) == NULL) {
	message("�ݒ�t�@�C�� %s ���I�[�v���ł��܂���\n", INI_FILE);
	return ERR;
    }
    ret = OK;
    lineno = 0;
    while (fgets((char *)buf, BUFSIZ, fp) != NULL) {
	lineno++;
	p = skipsp(buf);
	if (*p == '#' || *p == '\r' || *p == '\n')
	    continue;
	p = getuptos(p, key, " \t=#");
	p = skipch(p, '=');
	p = skipsp(p);
	p = getuptos(p, val, "\t#");
	n = atoi(val);
	if (!strcmp(key, "BASEPATH")) {
	    if (p[-1] != '/' && p[-1] != '\\')
		strcat(val, "/");
	    base_path = strdup(val);
	} else if (!strcmp(key, "OUTPATH")) {
	    if (p[-1] != '/' && p[-1] != '\\')
		strcat(val, "/");
	    out_path = strdup(val);
	} else if (!strcmp(key, "SORTCMD")) {
	    sort_cmd = strdup(val);
	} else if (!strcmp(key, "AUTOKANA")) {
	    auto_kana = n;
	} else if (!strcmp(key, "EBTYPE")) {
	    eb_type = n;
	} else if (!strcmp(key, "BOOKTITLE")) {
	    book_title = strdup(val);
	    for (p = book_title; *p; p += 2) {
		if ((*p & 0x80) == 0)
		    break;
	    }
	    if (*p) {
		message("���Ѓ^�C�g����1�o�C�g�������܂܂�Ă��܂�(%d�s��)\n", lineno);
		ret = ERR;
	    }
	} else if (!strcmp(key, "BOOKTYPE")) {
	    book_type = strdup(val);
	    if (strcmp(book_type, "���ꎫ�T") &&
		strcmp(book_type, "���a���T") &&
		strcmp(book_type, "�p�a���T") &&
		strcmp(book_type, "�a�p���T") &&
		strcmp(book_type, "����p�ꎫ�T") &&
		strcmp(book_type, "��ʏ���") &&
		strcmp(book_type, "�ތꎫ�T")) {
		message("���m�̏��Ў�ʂ��w�肳��Ă��܂�(%d�s��)\n", lineno);
		ret = ERR;
	    }
	} else if (!strcmp(key, "BOOKDIR")) {
	    book_dir = strdup(val);
	    if (strlen(book_dir) > 8) {
		message("���Ѓf�B���N�g������8�o�C�g�𒴂��Ă��܂�(%d�s��)\n", lineno);
		ret = ERR;
	    }
	    for (p = book_dir; *p; p++) {
		if (*p >= 'A' && *p <= 'Z')
		    continue;
		if (*p >= '0' && *p <= '9' || *p == '_')
		    continue;
		break;
	    }
	    if (*p) {
		message("���Ѓf�B���N�g�����ɕs���ȕ���(A-Z0-9_�ȊO)���܂܂�Ă��܂�(%d�s��)\n", lineno);
		ret = ERR;
	    }
	    if (ret != ERR) {
		sprintf(buf, "%s.html", book_dir);
		html_file = strdup(buf);
		sprintf(buf, "%s.ebs", book_dir);
		ebs_file = strdup(buf);
	    }
	} else {
	    message("�ݒ�t�@�C���ɕs���ȍs������܂�(%d�s��)\n", lineno);
	    ret = ERR;
	}
    }
    fclose(fp);
    message("�ϊ��ݒ�͈ȉ��̂Ƃ���ł�\n");
    message("  BASEPATH = %s\n", base_path);
    message("  OUTPATH = %s\n", out_path);
    message("  SORTCMD = %s\n", sort_cmd);
    message("  AUTOKANA = %d\n", auto_kana);
    message("  EBTYPE = %d\n", eb_type);
    message("  BOOKTITLE = %s\n", book_title);
    message("  BOOKTYPE = %s\n", book_type);
    message("  BOOKDIR = %s\n", book_dir);
    message("  ���������HTML�t�@�C�� = %s\n", html_file);
    message("  ���������EBS�t�@�C�� = %s\n", ebs_file);
    return ret;
}

/*
 * �t�@�C���o�͐�f�B���N�g�������
 */
int
work_directory(void)
{
    int i;
    char *p, path[MAX_PATH], subpath[MAX_PATH];
    struct stat st;

    if (*base_path &&
	strcmp(base_path, "\\") &&
	strcmp(base_path + 1, ":\\")) {
	strcpy(path, base_path);
	p = &path[strlen(path) - 1];
	if (*p == '/' || *p == '\\')
	    *p = '\0';
	if (stat(path, &st) < 0) {
	    if (mkdir(path,0755) < 0) {
		message("��f�B���N�g�� %s �����܂���\n", path);
		return ERR;
	    }
	    message("��f�B���N�g�� %s ��V�K�쐬���܂���\n", path);
	} else if ((st.st_mode & S_IFMT) != S_IFDIR) {
	    message("��f�B���N�g�� %s �Ɠ����̃t�@�C��������܂�\n", path);
	    return ERR;
	}
    }
    if (*out_path &&
	strcmp(out_path, "\\") &&
	strcmp(out_path + 1, ":\\")) {
	strcpy(path, out_path);
	p = &path[strlen(path) - 1];
	if (*p == '/' || *p == '\\')
	    *p = '\0';
	if (stat(path, &st) < 0) {
	    if (mkdir(path,0755) < 0) {
		message("�o�͐�f�B���N�g�� %s �����܂���\n", path);
		return ERR;
	    }
	    message("�o�͐�f�B���N�g�� %s ��V�K�쐬���܂���\n", path);
	} else if ((st.st_mode & S_IFMT) != S_IFDIR) {
	    message("�o�͐�f�B���N�g�� %s �Ɠ����̃t�@�C��������܂�\n", path);
	    return ERR;
	}
    }
    return OK;
}

/*
 * ���p�\�ȃ\�[�g�R�}���h��ݒ肷��
 */
int
set_sort_command(void)
{
    char **p;
    struct stat st;
    static char *sort_cmd_cand[] = {
	"",
	"C:\\Windows\\System32\\sort.exe",
	"C:\\WinNT\\System32\\sort.exe",
	"C:\\Windows\\command\\sort.exe",
	NULL
    };

    if (sort_cmd && *sort_cmd)
	sort_cmd_cand[0] = sort_cmd;
    for (p = sort_cmd_cand; *p; p++) {
	if (stat(*p, &st) == 0) {
	    sort_cmd = *p;
	    return OK;
	}
    }
    message("�\�[�g�R�}���h��������܂���\n");
    return ERR;
}

/*
 * ������
 */
int
init(char *cmd_path)
{
    char *p, path[MAX_PATH];

    strcpy(path, cmd_path);
    p = &path[strlen(path) - 1];
    while (p > path && *p != '/')
	p--;
    *p = '\0';
    if (!strcmp(path + 1, ":"))
	strcat(path, "/");
    if (chdir(path) < 0) {
	printf("��ƃf�B���N�g�� %s �ւ̈ړ��Ɏ��s���܂���\n", path);
	return ERR;
    }
    time(&start_time);
    write_log("�J�n����: %s", ctime(&start_time));
    message("��ƃf�B���N�g�� %s �Ɉړ����܂���\n", path);
    return OK;
}

/*
 * �I������
 */
void
term(int status)
{
    time_t t;

    if (status == OK)
	message("�ϊ��������I�����܂���\n");
    else
	message("�ϊ������Ɏ��s���܂���\n");
    time(&stop_time);
    t = stop_time - start_time;
    write_log("�I������: %s", ctime(&stop_time));
    message("�o�ߎ���: %d:%02d\n", (int)(t / 60), (int)(t % 60));
    if (status == OK) {
	message("�� %s%s ����͂Ƃ���EBStudio�����s���Ă�������\n",
	    base_path, ebs_file);
    }
    write_log("\n");
    exit(status == ERR? 1: 0);
}

/*
 * ���C��
 */
int
main(int ac, char **av)
{
    if (init(av[0]) == ERR)
	term(ERR);
    if (parse_ini_file() == ERR)
	term(ERR);
    if (work_directory() == ERR)
	term(ERR);
    if (set_sort_command() == ERR)
	term(ERR);
    if (generate_gaiji_file() == ERR)
	term(ERR);
    if (generate_work_file() == ERR)
	term(ERR);
    if (generate_html_file() == ERR)
	term(ERR);
    if (generate_ebs_file() == ERR)
	term(ERR);
    term(OK);
}
