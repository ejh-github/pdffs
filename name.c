#include <u.h>
#include <libc.h>
#include "pdf.h"

/* 7.3.5 Name Objects */

Object *
pdfname(Stream *stream)
{
	Object *o;
	char *s, *r, hex[3];
	int c, sz, maxsz;

	Sgetc(stream); /* skip '/' */

	maxsz = 32;
	if((s = malloc(maxsz)) == nil)
		goto err;

	for(sz = 0;;){
		if((c = Sgetc(stream)) < 0){
			if(c == -1)
				break;
			goto err;
		}

		if(isws(c) || isdelim(c)){
			Sungetc(stream);
			break;
		}
		if(c < '!' || c > '~'){
			werrstr("invalid char %02x", c);
			goto err;
		}
		if(c == '#'){
			if((c = Sgetc(stream)) < 0)
				goto err;
			hex[0] = c;
			if((c = Sgetc(stream)) < 0)
				goto err;
			hex[1] = c;
			if(dec16((uchar*)hex, 1, hex, 2) != 1){
				werrstr("invalid hex");
				goto err;
			}
			c = hex[0];
		}
		if(sz+1 >= maxsz){
			maxsz *= 2;
			if((r = realloc(s, maxsz)) == nil)
				goto err;
			s = r;
		}
		s[sz++] = c;
	}

	if((o = malloc(sizeof(*o))) != nil){
		o->name = s;
		s[sz] = 0;
		o->type = Oname;
		return o;
	}

err:
	werrstr("name: %r");
	free(s);
	return nil;
}
