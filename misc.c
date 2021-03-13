#include <u.h>
#include <libc.h>
#include "pdf.h"

static char *otypes[] = {
	[Obool] = "bool",
	[Onum] = "num",
	[Ostr] = "str",
	[Oname] = "name",
	[Oarray] = "array",
	[Odict] = "dict",
	[Ostream] = "stream",
	[Onull] = "null",
	[Oindir] = "indir",
};

Object null = {
	.type = Onull,
};

int
Ofmt(Fmt *f)
{
	Object *o;
	int i;

	o = va_arg(f->args, Object*);
	if(o == nil || o == &null)
		return fmtprint(f, "null");
	switch(o->type){
	case Obool:
		return fmtprint(f, o->bool ? "true" : "false");

	case Onum:
		return fmtprint(f, "%g", o->num.d);

	case Ostr:
		if(isutf8(o->str, o->len))
			return fmtprint(f, "%q", o->str);
		return fmtprint(f, "<%.*H>", o->len, o->str);

	case Oname:
		return fmtprint(f, "/%s", o->name);

	case Oarray:
		fmtprint(f, "[");
		for(i = 0; i < o->array.ne; i++)
			fmtprint(f, "%s%O", i > 0 ? ", " : "", o->array.e[i]);
		return fmtprint(f, "]");

	case Ostream: /* FIXME dump the stream? */
	case Odict:
		fmtprint(f, "<<");
		for(i = 0; i < o->dict.nkv; i++)
			fmtprint(f, "%s%s = %O", i > 0 ? ", " : "", o->dict.kv[i].key, o->dict.kv[i].value);
		return fmtprint(f, ">>%s", o->type == Ostream ? "+stream" : "");

	case Onull:
		return fmtprint(f, "null");

	case Oindir:
		return fmtprint(f, "@%d[gen=%d]", o->indir.id, o->indir.gen);

	}
	return fmtprint(f, "???");
}

int
Tfmt(Fmt *f)
{
	Object *o;

	o = va_arg(f->args, Object*);
	if(o == nil || o == &null)
		return fmtprint(f, "null");
	if(o->type < 0 || o->type >= nelem(otypes))
		return fmtprint(f, "????");
	return fmtprint(f, "%s", otypes[o->type]);
}

int
⊗fmt(Fmt *f)
{
	Xref x;

	x = va_arg(f->args, Xref);

	if(x.objstm > 0)
		return fmtprint(f, "<compressed id=%d objstm=%d index=%d>", x.id, x.objstm, x.index);

	return fmtprint(f, "<uncompressed id=%d off=%d gen=%d>", x.id, x.off, x.gen);
}

int
isws(int c)
{
	return /* \0 is missing on purpose */
		c == '\t' || c == '\n' || c == '\f' || c == '\r' ||
		c == ' ';
}

int
isdelim(int c)
{
	return
		c == '(' || c == ')' || c == '<' || c == '>' ||
		c == '[' || c == ']' || c == '{' || c == '}' ||
		c == '/' || c == '%';
}

int
isutf8(char *s, int len)
{
	int i, n;
	Rune r;

	for(i = 0; i < len; i += n, s += n){
		if((n = chartorune(&r, s)) < 1 || r == Runeerror)
			break;
	}

	return i >= len;
}
