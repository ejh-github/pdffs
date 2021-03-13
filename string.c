#include <u.h>
#include <libc.h>
#include "pdf.h"

/* 7.3.4 String Objects */

static char esc[] = {
	['n'] = '\n',
	['r'] = '\r',
	['t'] = '\t',
	['b'] = '\b',
	['f'] = '\f',
	['('] = '(',
	[')'] = ')',
	['\\'] = '\\',
	['\n'] = -1,
};

static Object *
stringhex(Stream *stream)
{
	char *s;
	Object *o;
	int len, n;

	if((s = Srdstr(stream, '>', 0)) == nil)
		return nil;
	len = Slinelen(stream) - 1;
	if(s[len] != '>'){
		werrstr("no '>'");
		free(s);
		return nil;
	}
	s[len] = '0'; /* the final zero may be missing */
	n = len/2;
	o = nil;
	if(dec16((uchar*)s, n, s+1, len) != n){
		werrstr("invalid hex");
	}else if((o = malloc(sizeof(*o))) != nil){
		o->str = s;
		s[n] = 0;
		o->len = n;
		o->type = Ostr;
		return o;
	}

	free(s);
	return o;
}

Object *
pdfstring(Stream *stream)
{
	Object *o;
	char *s, *r;
	char oct[4];
	int i, c, paren, sz, maxsz;

	maxsz = 64;
	if((s = malloc(maxsz)) == nil)
		return nil;

	for(paren = sz = 0;;){
		if((c = Sgetc(stream)) < 0)
			break;

		switch(c){
		case '<':
			if(sz == 0){
				Sungetc(stream);
				return stringhex(stream);
			}
			break;

		case '(':
			paren++;
			continue;

		case ')':
			paren--;
			if(paren < 1){
				c = 0;
				goto end;
			}
			continue;

		case '\\':
			if((c = Sgetc(stream)) <= 0)
				break;
			if(c >= '0' && c <= '7'){ /* octal */
				oct[0] = c;
				for(i = 1; i < 3 && (c = Sgetc(stream)) >= '0' && c <= '7'; i++)
					oct[i] = c;
				if(c <= 0)
					break;
				if(c < '0' || c > '7')
					Sungetc(stream);
				oct[i] = 0;
				c = strtol(oct, nil, 8);
			}else if(c >= nelem(esc) || (c = esc[c]) == 0){
				werrstr("unknown escape char '%c'", c);
				goto err;
			}else if(c < 0){
				continue;
			}
			break;

		default:
			if(paren < 1){
				werrstr("unexpected char %02x '%c'", (u8int)c, c);
				goto err;
			}
			break;
		}

		if(c < 0)
			break;
		if(sz+1 > maxsz){
			maxsz *= 2;
			if((r = realloc(s, maxsz)) == nil)
				goto err;
			s = r;
		}
		s[sz++] = c;
	}
end:
	if(paren != 0){
		werrstr("bad paren");
		goto err;
	}
	if(c < 0){
		werrstr("short");
		goto err;
	}

	if(c >= 0 && (o = malloc(sizeof(*o))) != nil){
		s[sz] = 0;
		o->str = s;
		o->len = sz;
		o->type = Ostr;
		return o;
	}

err:
	free(s);
	werrstr("string: %r");
	return nil;
}
