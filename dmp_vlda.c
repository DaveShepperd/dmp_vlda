#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include "vlda_structs.h"
#include "formats.h"

#ifndef n_elts
	#define n_elts(x) (sizeof(x)/sizeof((x)[0]))
#endif

typedef struct
{
	int num;
	char *str;
	size_t structSize;
} Xref_t;

static const Xref_t ObjCodes[] =
{
	{ VLDA_ABS, "ABS", sizeof(VLDA_abs) },  /* vlda relocatible text record */
	{ VLDA_TXT, "TXT" },    /* vlda relocatible text record */
	{ VLDA_GSD, "GSD" },    /* psect/symbol definition record */
	{ VLDA_ORG,	"ORG" },    /* set the PC record */
	{ VLDA_ID,  "ID", sizeof(VLDA_id) },   /* misc header information */
	{ VLDA_EXPR, "EXPR" },  /* expression */
	{ VLDA_TPR,	"TPR" },    /* transparent record (raw data follows */
	{ VLDA_SLEN, "SLEN", sizeof(VLDA_slen) },    /* segment length */
	{ VLDA_XFER, "XFER", sizeof(VLDA_abs) }, /* transfer address */
	{ VLDA_TEST, "TEST", sizeof(VLDA_test) },   /* test and display message if result is false */
	{ VLDA_DBGDFILE, "DBGFILE", sizeof(VLDA_dbgdfile) }, /* dbg file specification */
	{ VLDA_DBGSEG, "DBGSEG", sizeof(VLDA_dbgseg) }, /* dbg segment descriptors */
	{ VLDA_BOFF, "BOFF", sizeof(VLDA_test) },   /* branch offset out of range test */
	{ VLDA_OOR,	"OOR", sizeof(VLDA_test) }      /* operand value out of range test */
};

static uint16_t getU16(const uint8_t *rcd)
{
	uint16_t ans;
	ans = (rcd[1]<<8) | rcd[0];
	return ans;
}

static uint32_t getU32(const uint8_t *rcd)
{
	uint32_t ans;
	ans = (rcd[3]<<24) | (rcd[2]<<16) | (rcd[1]<<8) | rcd[0];
	return ans;
}

static const char* getObjCode(int code)
{
	int ii;
	for ( ii = 0; ii < n_elts(ObjCodes); ++ii )
	{
		if ( ObjCodes[ii].num == code )
			return ObjCodes[ii].str;
	}
	return "*Undefined*";
}

static int outputBytes(const uint8_t *rcd, int cnt, char *line, int lineSize)
{
	int ii, len;

	for ( len = ii = 0; ii < cnt; ++ii )
	{
		if ( len >= lineSize - (13 + 3) )
		{
			len += snprintf(line + len, lineSize - len, " +%d more", cnt - ii);
			break;
		}
		len += snprintf(line + len, lineSize - len, " %02X", rcd[ii]);
	}
	return len;
}

static int decodeExpr(const uint8_t *rcd, int cnt, char *line, int lineSize)
{
	int ptr,len=0;
	
	ptr = cnt ? rcd[0] : 0;
	len += snprintf(line+len,lineSize-len," Expr Terms=%d", ptr);
	if ( cnt )
	{
		++rcd;
		--cnt;
		while ( (ptr > 0) && (cnt > 0) && (len < lineSize-28) )
		{
			int code;
			code = *rcd++;
			--cnt;
			switch (code)
			{
				case VLDA_EXPR_SYM:		/* expression component is a symbol */
					len += snprintf(line+len,lineSize-len," %%%d", getU16(rcd));
					rcd += sizeof(uint16_t);
					cnt -= sizeof(uint16_t);
					--ptr;
					continue;
				case VLDA_EXPR_CSYM:	/* symbol with 1 byte identifier */
					len += snprintf(line+len,lineSize-len," %%%d", *rcd);
					++rcd;
					--cnt;
					--ptr;
					continue;
				case VLDA_EXPR_0VALUE:	/* value of 0 */
					len += snprintf(line+len,lineSize-len," 0");
					++rcd;
					--cnt;
					--ptr;
					continue;
				case VLDA_EXPR_CVALUE:	/* 1 byte value */
					len += snprintf(line+len,lineSize-len," %d", *rcd);
					++rcd;
					--cnt;
					--ptr;
					continue;
				case VLDA_EXPR_WVALUE:	/* 2 byte value */
					len += snprintf(line+len,lineSize-len," %d", getU16(rcd));
					rcd += sizeof(uint16_t);
					cnt -= sizeof(uint16_t);
					--ptr;
					continue;
				case VLDA_EXPR_VALUE:	/* expression component is an absolute value */
					len += snprintf(line+len,lineSize-len," %d", getU32(rcd));
					rcd += sizeof(uint32_t);
					cnt -= sizeof(uint32_t);
					--ptr;
					continue;
				case VLDA_EXPR_OPER:	/* expression component is an operator */
					{
						char cc, oper[3], *op;
						op = oper;
						cc = *rcd++;
						*op++ = cc;
						--cnt;
						if ( cc == '!' )
						{
							*op++ = *rcd++;
							--cnt;
						}
						*op = 0;
						len += snprintf(line+len,lineSize-len," %s", oper);
						--ptr;
					}
					continue;
				case VLDA_EXPR_L:		/* segment length */
					len += snprintf(line+len,lineSize-len," L");
					continue;
				case VLDA_EXPR_B:		/* segment base */
					len += snprintf(line+len,lineSize-len," B");
					continue;
				case VLDA_EXPR_TAG:	/* expression tag follows */
					len += snprintf(line+len,lineSize-len," :%c %d", rcd[0], getU32(rcd+1));
					rcd += 1+sizeof(uint32_t);
					cnt -= 1+sizeof(uint32_t);
					continue;
				case VLDA_EXPR_1TAG:	/* tag with implied count of 1 */
					len += snprintf(line+len,lineSize-len," :%c", rcd[0]);
					++rcd;
					--cnt;
					continue;
				case VLDA_EXPR_CTAG:	/* tag with 1 byte count */
					len += snprintf(line+len,lineSize-len," :%c %d", rcd[0], rcd[1]);
					rcd += 2;
					cnt -= 2;
					continue;
				case VLDA_EXPR_WTAG:	/* tag with 2 byte count */
					len += snprintf(line+len,lineSize-len," :%c %d", rcd[0], getU16(rcd+1));
					rcd += 1+sizeof(uint16_t);
					cnt -= 1+sizeof(uint16_t);
					continue;
				default:
					len += snprintf(line+len,lineSize-len," 0x%X(UNDEF) (+%d more)", code, cnt);
					return len;
			}
		}
		if ( cnt > 0 )
			len += snprintf(line+len,lineSize-len," (+%d more bytes)", cnt);
	}
	return len;
}

static const char* decodeType(uint16_t cnt, const uint8_t *rcd, char *line, int lineSize)
{
	uint8_t code = rcd[0];
	int len;

	code = rcd[0];
	len = snprintf(line, lineSize, "%d(%s)", code, getObjCode(code));
	switch (code)
	{
	case VLDA_XFER:     /* transfer address */
	case VLDA_ABS:      /* vlda relocatible text record */
	case VLDA_TXT:      /* vlda text record */
		{
			VLDA_abs *vabs = (VLDA_abs *)rcd;
			if ( cnt < (int)sizeof(VLDA_abs) )
				len += snprintf(line + len, lineSize - len, " *** Size is too small. s/b > " FMT_SZ " **", sizeof(VLDA_abs));
			else
			{
				len += snprintf(line + len, lineSize - len, " %08X ->", vabs->vlda_addr);
				len += outputBytes(rcd + sizeof(VLDA_abs), cnt - sizeof(VLDA_abs), line + len, lineSize - len);
			}
			break;
		}
	case VLDA_ID:       /* misc header information */
		{
			VLDA_id *vid = (VLDA_id *)rcd;
			if ( cnt < (int)sizeof(VLDA_id) || cnt < vid->vid_time + 1 )
				len += snprintf(line + len, lineSize - len, " *** Size is too small. is %d, s/b > " FMT_SZ " (siz=%d(" FMT_SZ "), maj=%d, min=%d, symsiz=%d(" FMT_SZ "), segsiz=%d(" FMT_SZ "), img=%d, targ=%d, time=%d, err=%d, warn=%d) **",
								cnt,
								cnt < (int)sizeof(VLDA_id) ? (int)sizeof(VLDA_id) : (size_t)vid->vid_time + 1,
								vid->vid_siz, sizeof(VLDA_id),
								vid->vid_maj, vid->vid_min,
								vid->vid_symsiz, sizeof(VLDA_sym),
								vid->vid_segsiz, sizeof(VLDA_seg),
								vid->vid_image, vid->vid_target, vid->vid_time,
								vid->vid_errors, vid->vid_warns);
			else if ( vid->vid_siz != (int)sizeof(VLDA_id) )
				len += snprintf(line + len, lineSize - len, " *** VLDA_id struct size is %d. s/b " FMT_SZ " **", vid->vid_siz, sizeof(VLDA_id));
			else if ( vid->vid_maj != VLDA_MAJOR || vid->vid_min != VLDA_MINOR )
				len += snprintf(line + len, lineSize - len, " *** Version is %d.%d. s/b %d.%d **", vid->vid_maj, vid->vid_min, VLDA_MAJOR, VLDA_MINOR);
			else if ( vid->vid_symsiz != (int)sizeof(VLDA_sym) || vid->vid_segsiz != (int)sizeof(VLDA_seg) )
				len += snprintf(line + len, lineSize - len, " *** VLDA_sym struct size is %d. s/b " FMT_SZ " and VLDA_seg struct size is %d. s/b " FMT_SZ "**",
								vid->vid_symsiz, sizeof(VLDA_sym), vid->vid_segsiz, sizeof(VLDA_seg));
			else
			{
				len += snprintf(line + len, lineSize - len, " err=%d, warn=%d, img=%s, target=%s, %s",
								vid->vid_errors, vid->vid_warns,
								(char *)(rcd)+vid->vid_image,
								(char *)(rcd)+vid->vid_target,
								(char *)(rcd)+vid->vid_time);
			}
			break;
		}
	case VLDA_GSD:      /* psect/symbol definition record */
		{
			VLDA_sym *vsym = (VLDA_sym *)rcd;
			if ( (vsym->vsym_flags & VSYM_SYM) )
			{
				if (   cnt < (int)sizeof(VLDA_sym)
					 || (vsym->vsym_noff && cnt < vsym->vsym_noff + 1)
					 || (vsym->vsym_eoff && cnt < vsym->vsym_eoff)
				   )
				{
					len += snprintf(line + len, lineSize - len, " *** SYM: Size is too small. is %d, s/b > " FMT_SZ " (flags=0x%04X, symb=%s, ident=%d, value=%d, eoff=%d) **",
									cnt,
									sizeof(VLDA_sym),
									vsym->vsym_flags,
									vsym->vsym_noff ? (char *)rcd + vsym->vsym_noff : NULL,
									vsym->vsym_ident,
									vsym->vsym_value,
									vsym->vsym_eoff
								   );
				}
				else
				{
					len += snprintf(line + len, lineSize - len, " SYM: flags=0x%04X, symb=%s, ident=%d, value=%d: ",
									vsym->vsym_flags,
									vsym->vsym_noff ? (char *)rcd + vsym->vsym_noff : NULL,
									vsym->vsym_ident,
									vsym->vsym_value
								   );
					if ( vsym->vsym_eoff )
					{
/*						len += outputBytes(rcd + vsym->vsym_eoff, cnt - vsym->vsym_eoff, line + len, lineSize - len); */
						len += decodeExpr(rcd+vsym->vsym_eoff,cnt-vsym->vsym_eoff,line+len,lineSize-len);
					}
					else
						len += snprintf(line+len,lineSize-len," <no expression>");
				}
			}
			else
			{
				VLDA_seg *vseg = (VLDA_seg *)rcd;
				if ( cnt < (int)sizeof(VLDA_seg) || (vseg->vseg_noff && cnt < vseg->vseg_noff + 1) )
				{
					len += snprintf(line + len, lineSize - len, " *** SEG: Size is too small. is %d, s/b > " FMT_SZ " (flags=0x%04X, segm=%s, ident=%d, salign=%d, dalign=%d, base=0x%X, max=0x%X, offset=0x%X) **",
									cnt,
									(size_t)cnt < sizeof(VLDA_seg) ? sizeof(VLDA_seg) : (size_t)vseg->vseg_noff + 1,
									vseg->vseg_flags,
									vseg->vseg_noff ? (char *)rcd + vseg->vseg_noff : NULL,
									vseg->vseg_ident,
									vseg->vseg_salign,
									vseg->vseg_dalign,
									vseg->vseg_base,
									vseg->vseg_maxlen,
									vseg->vseg_offset
								   );
				}
				else
				{
					len += snprintf(line + len, lineSize - len, " SEG: flags=0x%04X, ssegm=%s, ident=%d, salign=%d, dalign=%d, base=0x%X, max=0x%X, offset=0x%X",
									vseg->vseg_flags,
									vseg->vseg_noff ? (char *)rcd + vseg->vseg_noff : NULL,
									vseg->vseg_ident,
									vseg->vseg_salign,
									vseg->vseg_dalign,
									vseg->vseg_base,
									vseg->vseg_maxlen,
									vseg->vseg_offset
								   );
				}
			}
			break;
		}
	case VLDA_ORG:      /* set the PC record */
	case VLDA_TPR:      /* transparent record (raw data follows */
	case VLDA_DBGDFILE: /* dbg file specification */
	case VLDA_DBGSEG:   /* dbg segment descriptors */
		len += outputBytes(rcd+1,cnt-1,line+len,lineSize-len);
		break;
	case VLDA_EXPR:     /* expression */
		len += decodeExpr(rcd+1,cnt-1,line+len,lineSize-len);
		break;
	case VLDA_SLEN:     /* segment length */
		{
			VLDA_slen *slen = (VLDA_slen *)rcd;
			len += snprintf(line + len, lineSize - len, " ident=%d, length=%d", slen->vslen_ident, slen->vslen_len);
			break;
		}
	case VLDA_TEST:     /* test and display message if result is false */
	case VLDA_BOFF:     /* branch offset out of range test */
	case VLDA_OOR:      /* operand value out of range test */
		break;
	default:
		break;
	}
	return line;
}

int main(int argc, char *argv[])
{
	uint16_t cnt;
	uint8_t typ;
	int pad, recNum = 0;
	size_t sts;
	FILE *inp;
	const char *codeName;
	uint8_t *fileContents = NULL, *rcdPtr;
	size_t fileExtent = 0, fileSize = 0;

	if ( argc > 1 )
	{
		inp = fopen(argv[1], "r");
		if ( !inp )
		{
			printf("Error opening '%s': %s\n", argv[1], strerror(errno));
			return 1;
		}
	}
	else
		inp = stdin;
	while ( 1 )
	{
		if ( fileSize >= fileExtent  )
		{
			fileExtent += 65536;
			fileContents = (uint8_t *)realloc(fileContents, fileExtent);
			if ( !fileContents )
			{
				printf("Out of memory. fileExtent=" FMT_SZ "\n", fileExtent);
				return 1;
			}
		}
		if ( (sts = fread(fileContents + fileSize, 1, 65536, inp)) <= 0 )
		{
			if ( !feof(inp) )
			{
				printf("Error reading file. fileSize=" FMT_SZ ", fileExtent=" FMT_SZ ", sts=" FMT_SZ ": %s\n", fileSize, fileExtent, sts, strerror(errno));
				return 1;
			}
			break;
		}
		fileSize += sts;
	}
	if ( inp != stdin )
		fclose(inp);
	printf("FileSize=" FMT_SZ ", fileExtent=" FMT_SZ "\n", fileSize, fileExtent);
	rcdPtr = fileContents;
	while ( rcdPtr < fileContents + fileSize )
	{
		char line[200];

		cnt = (rcdPtr[1] << 8) | rcdPtr[0];
		typ = rcdPtr[2];
		codeName = getObjCode(typ);
		pad = (cnt & 1);
		if ( cnt >= 16384 )
		{
			printf("%3d: 0x%04X%c %02X(%s) Record count >= 16384. Probably out of sync\n", recNum, cnt, pad ? '*' : ' ', typ, codeName);
			break;
		}
		decodeType(cnt, rcdPtr + 2, line, sizeof(line) - 1);
		printf("%3d: 0x%04X%c %s\n", recNum, cnt, pad ? '*' : ' ', line);
		rcdPtr += cnt + 2 + pad;
		++recNum;
	}
	return 0;
}
