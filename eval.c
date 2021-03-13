#include <u.h>
#include <libc.h>
#include "pdf.h"

static Object *
evalobjstm(Pdf *pdf, Xref *x)
{
	Object *ostm, *o;
	Stream *s;
	Xref *xstm;
	int i, off, nobj, first, index;

	ostm = nil;
	s = nil;
	o = &null;
	/* x is pointing at ObjStm, need to eval it to the actual object */
	for(i = 0; i < pdf->nxref && pdf->xref[i].id != x->objstm; i++);
	if(i >= pdf->nxref){
		werrstr("no object id %d in xref", x->objstm);
		goto err;
	}
	xstm = &pdf->xref[i];

	if(Sseek(pdf->s, xstm->off, 0) != xstm->off){
		werrstr("xref seek failed");
		goto err;
	}
	if((ostm = pdfobj(pdf, pdf->s)) == nil)
		goto err;
	first = -1;
	if((nobj = dictint(ostm, "N")) < 1 || (first = dictint(ostm, "First")) < 0){
		werrstr("invalid ObjStm: nobj=%d first=%d", nobj, first);
		goto err;
	}

	if((s = Sopen(ostm)) == nil)
		goto err;
	for(i = 0; i < nobj; i++){
		Sgeti(s, &index);
		Sgeti(s, &off);
		if(x->id == index){
			off += first;
			if(Sseek(s, off, 0) != off){
				werrstr("xref obj seek failed");
				goto err;
			}
			if((o = pdfobj(pdf, s)) == nil)
				goto err;
			o = pdfeval(&o);
			break;
		}
	}
	Sclose(s);

	return o;

err:
	pdfobjfree(ostm);
	Sclose(s);
	return &null;
}

Object *
pdfeval(Object **oo)
{
	Object *d, *o;
	Xref *x;
	int i;

	if(oo == nil)
		sysfatal("nil oo");
	if(*oo == nil){
		*oo = &null;
		return &null;
	}
	o = *oo;
	if(o->type != Oindir)
		return o;

	for(x = nil, i = 0; i < o->pdf->nxref; i++){
		x = &o->pdf->xref[i];
		if(x->id == o->indir.id)
			break;
	}
	if(i >= o->pdf->nxref){
		werrstr("no object id %d in xref", o->indir.id);
		return &null;
	}
	if(x->objstm > 0){
		if((o = evalobjstm(o->pdf, x)) == &null)
			werrstr("ObjStm: %r");
		*oo = o;
		return o;
	}

	if(Sseek(o->pdf->s, x->off, 0) != x->off){
		werrstr("xref seek failed");
		return &null;
	}
	if((d = pdfobj(o->pdf, o->pdf->s)) == nil){
		werrstr("eval: %r [at %p]", (void*)x->off);
		return &null;
	}
	*oo = d;
	pdfobjfree(o);

	return d;
}
