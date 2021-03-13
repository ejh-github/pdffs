#include <u.h>
#include <libc.h>
#include "pdf.h"

/* 7.3.6 Array Objects */

Object *
pdfarray(Pdf *pdf, Stream *s)
{
	Object *o, *m;
	Object **a;
	int c, noel;

	o = calloc(1, sizeof(*o));
	o->pdf = pdf;
	o->type = Oarray;
	Sgetc(s); /* throw away '[' */

	for(noel = 0;;){
		if((c = Sgetc(s)) < 0 || c == ']')
			break;
		if(noel){
			werrstr("no ']'");
			goto err;
		}

		Sungetc(s);
		if((m = pdfobj(pdf, s)) == nil){
			noel = 1;
			continue;
		}

		if((a = realloc(o->array.e, (o->array.ne+1)*sizeof(Object*))) == nil){
			pdfobjfree(m);
			goto err;
		}

		o->array.e = a;
		a[o->array.ne++] = m;
	}

	if(c != ']'){
		werrstr("no ']'");
		goto err;
	}

	return o;
err:
	werrstr("array: %r");
	pdfobjfree(o);
	return nil;
}

int
arraylen(Object *o)
{
	if(o == nil || o == &null)
		return 0;
	return (o->type == Oarray) ? o->array.ne : 1;
}

Object *
arrayget(Object *o, int i)
{
	if(arraylen(o) <= i)
		sysfatal("array: indexing out of range");
	o = o->type == Oarray ? o->array.e[i] : o;

	return pdfeval(&o);
}

int
arrayint(Object *o, int i)
{
	return (o = arrayget(o, i))->type == Onum ? o->num.i : 0;
}
