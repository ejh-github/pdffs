#include <u.h>
#include <libc.h>
#include <ctype.h>
#include "pdf.h"

Object *pdfstring(Stream *s);
Object *pdfname(Stream *s);
Object *pdfarray(Pdf *pdf, Stream *s);
Object *pdfdict(Pdf *pdf, Stream *s);

/* General function to parse an object of any type. */
Object *
pdfobj(Pdf *pdf, Stream *s)
{
	Object *o, *o2;
	vlong off;
	int c, tf;
	Xref xref;
	char b[16];

	o = o2 = nil;
	do; while(isws(c = Sgetc(s)));
	if(c < 0)
		goto err;

	switch(c){
	case '<': /* dictionary or a string */
		c = Sgetc(s);
		if(c == '<'){
			Sseek(s, -2, 1);
			if((o = pdfdict(pdf, s)) != nil){
				/* check for attached stream */
				off = Soffset(s);
				do; while(isws(Sgetc(s)));
				Sungetc(s);
				if(Sread(s, b, 7) == 7 && memcmp(b, "stream", 6) == 0 && isws(c = b[6])){
					/* there IS a stream */
					if(c == '\r' && (c = Sgetc(s)) < 0)
						goto err;
					if(c != '\n'){
						werrstr("stream has no newline after dict");
						goto err;
					}
					o->stream.off = Soffset(s);
					o->type = Ostream;
					o->stream.len = dictint(o, "Length");
					return o;
				}
				Sseek(s, off, 0);
			}
			return o;
		}
		Sungetc(s);
		/* fall through */

	case '(':
		Sungetc(s);
		if((o = pdfstring(s)) != nil)
			o->pdf = pdf;
		return o;

	case '/':
		Sungetc(s);
		if((o = pdfname(s)) != nil)
			o->pdf = pdf;
		return o;

	case '[':
		Sungetc(s);
		if((o = pdfarray(pdf, s)) != nil)
			o->pdf = pdf;
		return o;

	case 'n':
		off = Soffset(s);
		if(Sgetc(s) == 'u' && Sgetc(s) == 'l' && Sgetc(s) == 'l' && (isws(c = Sgetc(s)) || isdelim(c))){
			Sungetc(s);
			return &null;
		}
		Sseek(s, off, 0);
		c = 'f';
		goto unexpected;

	case 't':
		off = Soffset(s);
		tf = 1;
		if(Sgetc(s) == 'r' && Sgetc(s) == 'u' && Sgetc(s) == 'e' && (isws(c = Sgetc(s)) || isdelim(c)))
			goto bool;
		Sseek(s, off, 0);
		c = 't';
		goto unexpected;

	case 'f':
		off = Soffset(s);
		tf = 0;
		if(Sgetc(s) == 'a' && Sgetc(s) == 'l' && Sgetc(s) == 's' && Sgetc(s) == 'e' && (isws(c = Sgetc(s)) || isdelim(c)))
			goto bool;
		Sseek(s, off, 0);
		c = 'f';
		goto unexpected;
bool:
		Sungetc(s);
		if((o = calloc(1, sizeof(*o))) == nil)
			goto err;
		o->type = Obool;
		o->pdf = pdf;
		o->bool = tf;
		return o;

	default:
		if(!isdigit(c) && c != '-'){
unexpected:
			Sungetc(s);
			werrstr("unexpected char '%c' at %#x+%#x (%d left)", c, Sobjoffset(s), Soffset(s), Ssize(s));
			goto err;
		}
		 /* it could be a number or an indirect object */
		Sungetc(s);
		if((o = calloc(1, sizeof(*o))) == nil)
			goto err;
		o->pdf = pdf;
		Sgetd(s, &o->num.d); /* get the first number */
		o->num.i = o->num.d;
		off = Soffset(s); /* seek here if not an indirect object later */

		if((o2 = pdfobj(pdf, s)) != nil && o2->type == Onum){ /* second object is number too */
			do; while(isws(c = Sgetc(s)));
			if(c < 0)
				goto err;
			if(c == 'R'){ /* indirect object */
				o->type = Oindir;
				o->indir.id = o->num.i;
				o->indir.gen = o2->num.i;
				pdfobjfree(o2);
				return o;
			}
			if(c == 'o' && Sgetc(s) == 'b' && Sgetc(s) == 'j'){ /* object */
				xref.id = o->num.i;
				xref.gen = o2->num.i;
				/* FIXME put into a map */
				pdfobjfree(o2);
				if((o2 = pdfobj(pdf, s)) != nil){
					pdfobjfree(o);
					return o2;
				}else{
					werrstr("obj: %r");
					goto err;
				}
			}
		}

		/* just a number, go back and return it */
		o->type = Onum;
		if(Sseek(s, off, 0) != off){
			werrstr("seek failed");
			goto err;
		}
		return o;
	}

err:
	werrstr("object: %r");
	pdfobjfree(o);
	pdfobjfree(o2);
	return nil;
}

void
pdfobjfree(Object *o)
{
	int i;

	if(o == nil || --o->ref >= 0)
		return;

	switch(o->type){
	case Onull:
		return;

	case Ostr:
	case Oname:
		free(o->str);
		break;

	case Obool:
	case Onum:
		break;

	case Oarray:
		for(i = 0; i < o->array.ne; i++)
			pdfobjfree(o->array.e[i]);
		free(o->array.e);
		break;

	case Odict:
	case Ostream:
		for(i = 0; i < o->dict.nkv; i++){
			free(o->dict.kv[i].key);
			pdfobjfree(o->dict.kv[i].value);
		}
		free(o->dict.kv);
		break;

	case Oindir:
		break;
	}

	free(o);
}

Object *
pdfref(Object *o)
{
	o->ref++;
	return o;
}
