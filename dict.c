#include <u.h>
#include <libc.h>
#include "pdf.h"

/* 7.3.7 Dictionary Objects */

Object *
pdfdict(Pdf *pdf, Stream *s)
{
	Object *o, *k, *v;
	KeyValue *kv;
	int c, nokey;

	/* skip '<<' */
	Sseek(s, 2, 1);

	k = v = nil;
	o = calloc(1, sizeof(*o));
	o->type = Odict;
	o->pdf = pdf;
	for(nokey = 0;;){
		if((c = Sgetc(s)) < 0)
			goto err;
		if(c == '>'){
			if(Sgetc(s) == '>')
				break;
			werrstr("no '>>'");
			goto err;
		}
		if(nokey){
			werrstr("no '>>'");
			goto err;
		}

		Sungetc(s);
		if((k = pdfobj(pdf, s)) == nil){
			nokey = 1;
			continue;
		}
		if(k->type != Oname){
			werrstr("expected name as a key");
			goto err;
		}
		if((v = pdfobj(pdf, s)) == nil)
			goto err;

		if((kv = realloc(o->dict.kv, (o->dict.nkv+1)*sizeof(KeyValue))) == nil)
			goto err;

		o->dict.kv = kv;
		kv[o->dict.nkv].key = strdup(k->name);
		pdfobjfree(k);
		kv[o->dict.nkv++].value = v;
		k = v = nil;
	}

	return o;
err:
	pdfobjfree(o);
	pdfobjfree(k);
	pdfobjfree(v);
	werrstr("dict: %r");

	return nil;
}

Object *
dictget(Object *o, char *name)
{
	int i;

	pdfeval(&o);
	if((o->type != Ostream && o->type != Odict) || name == nil)
		return &null;
	for(i = 0; i < o->dict.nkv; i++){
		if(strcmp(name, o->dict.kv[i].key) == 0){
			o = pdfeval(i < o->dict.nkv ? &o->dict.kv[i].value : nil);
			return o;
		}
	}

	return &null;
}

int
dictintopt(Object *o, char *name, int def)
{
	o = dictget(o, name);
	return o->type == Onum ? o->num.i : def;
}

int
dictint(Object *o, char *name)
{
	return dictintopt(o, name, 0);
}

char *
dictstring(Object *o, char *name)
{
	o = dictget(o, name);
	return o->type == Ostr ? o->str : "";
}

Object *
dictdict(Object *o, char *name)
{
	o = dictget(o, name);
	return o->type == Odict ? o : &null;
}

int
dictints(Object *o, char *name, int *el, int nel)
{
	int n;
	Object *v;

	o = dictget(o, name);
	if(o->type != Oarray){
		werrstr("not an array");
		return -1;
	}

	for(n = 0; n < o->array.ne && n < nel; n++){
		if((v = o->array.e[n])->type != Onum){
			werrstr("not an integer array");
			return -1;
		}
		el[n] = v->num.i;
	}

	return n;
}
