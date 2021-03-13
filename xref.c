#include <u.h>
#include <libc.h>
#include <ctype.h>
#include "pdf.h"

/*
 * 7.5.4 pre-1.5 xref section reader
 * PDF>=1.5 may have BOTH (or either) old xref format and xref streams
 */
int
xrefreadold(Pdf *pdf)
{
	int xref0; /* 7.5.4 xref subsection first object number */
	int nxref; /* 7.5.4 xref subsection number of objects */
	int i, j, sz, n, newnxref;
	Xref xref;
	char *s, *e;
	Xref *x;

	Sseek(pdf->s, 4, 1);	
	if(Sgeti(pdf->s, &xref0) != 1 || xref0 < 0){
		werrstr("invalid xref0");
		return -1;
	}
	if(Sgeti(pdf->s, &nxref) != 1 || nxref < 0){
		werrstr("invalid nxref");
		return -1;
	}
	if(nxref < 1)
		return 0;

	/* skip whitespace and move to the first subsection */
	do; while(isspace(Sgetc(pdf->s)));
	Sungetc(pdf->s);

	s = nil;
	if((x = realloc(pdf->xref, (pdf->nxref + nxref)*sizeof(Xref))) == nil)
		goto err;
	pdf->xref = x;

	/* read the entire thing at once */
	sz = nxref*20;
	if((s = malloc(sz)) == nil)
		goto err;
	for(i = 0; i < sz; i += n){
		if((n = Sread(pdf->s, s+i, sz-i)) < 1)
			goto err;
	}

	/* store non-free objects only */
	newnxref = pdf->nxref;
	xref.objstm = 0;
	for(e = s, i = 0; i < nxref; i++, e += 20){
		if(!isspace(e[10]) || !isspace(e[18]) || !isspace(e[19])){
			werrstr("invalid xref line (%d/%d)", i, nxref);
			goto err;
		}
		xref.id = xref0 + i;
		xref.off = strtoul(e, nil, 10);
		xref.gen = strtoul(e+11, nil, 10);

		/* search in already existing xrefs, update if found */
		for(j = 0; j < pdf->nxref; j++){
			if(pdf->xref[j].id != xref.id)
				continue;
			if(e[17] == 'f') /* it was freed */
				pdf->xref[j].id = 0;
			else if(e[17] == 'n')
				pdf->xref[j].off = xref.off;
			break;
		}
		if(j >= pdf->nxref && e[17] == 'n') /* that's a new one, insert unless it's free */
			pdf->xref[newnxref++] = xref;
	}
	free(s);
	s = nil;

	/* scale down */
	for(i = j = 0; i < newnxref; i++){
		if(pdf->xref[i].id != 0)
			pdf->xref[j++] = pdf->xref[i];
	}
	if((x = realloc(pdf->xref, j*sizeof(Xref))) == nil)
		goto err;
	pdf->xref = x;
	pdf->nxref = j;

	return 0;
err:
	free(s);
	return -1;
}

static int
getint(uchar *b, int sz, int dflt)
{
	int x, i;

	if(sz == 0)
		return dflt;
	x = 0;
	for(i = 0; i < sz; i++)
		x = x<<8 | b[i];

	return x;
}

/* 7.5.8.3 */
int
xrefreadstream(Pdf *pdf)
{
	Object *o, *p, *index;
	Stream *s;
	Xref *x;
	uchar buf[32];
	int w[8], nw, c, n, nxref, newnxref, prev, extra;
	int i, ni, nsubsec, subsec;

	s = nil;
	if((o = pdfobj(pdf, pdf->s)) == nil){
		werrstr("xref stream obj: %r");
		goto err;
	}

	index = dictget(o, "Index"); /* 7.5.8.2 subsection indexing */
	nsubsec = arraylen(index) / 2;

	if((prev = dictint(o, "Prev")) > 0){ /* 7.5.8.2 previous xref stream */
		if(Sseek(pdf->s, prev, 0) != prev){
			werrstr("xref stream prev seek failed");
			goto err;
		}
		if(xrefreadstream(pdf) != 0){
			pdfobjfree(o);
			return -1;
		}
	}
	if((s = Sopen(o)) == nil){
		werrstr("failed to stream xref: %r");
		goto err;
	}
	if((nw = dictints(o, "W", w, nelem(w))) < 3 || nw >= nelem(w)){
		werrstr("nW=%d", nw);
		goto err;
	}

	for(n = i = 0; i < nw; i++)
		n += w[i]; /* size of each element. w[i] MAY be 0 */
	if(n > sizeof(buf)){
		werrstr("W is beyond imaginable: %d bytes", n);
		goto err;
	}
	if((nxref = Ssize(s)/n) < 1){
		werrstr("no xref elements in the stream");
		goto err;
	}
	extra = Ssize(s) % (nxref*n);
	if(extra != 0)
		fprint(2, "extra %d bytes in xref stream", extra);

	newnxref = pdf->nxref + nxref;
	if((x = realloc(pdf->xref, newnxref*sizeof(Xref))) == nil)
		goto err;
	pdf->xref = x;
	x += pdf->nxref;
	i = 0;
	for(ni = subsec = 0; Sread(s, buf, n) == n; ni--, i++){ /* stop on short read or error */
		if(ni == 0 && nsubsec > 0){
			i = arrayint(index, subsec*2+0); /* index of the first object */
			ni = arrayint(index, subsec*2+1); /* number of objects in the subsection */
			subsec++;
		}

		c = getint(buf, w[0], 1); /* default type is 1 */
		if(c == 1){ /* uncompressed */
			x->objstm = 0;
			x->id = i;
			x->off = getint(buf+w[0], w[1], 0);
			x->gen = getint(buf+w[0]+w[1], w[2], 0);
			pdf->nxref++;
			x++;
		}else if(c == 2){ /* compressed */
			x->id = i;
			x->objstm = getint(buf+w[0], w[1], 0);
			x->index = getint(buf+w[0]+w[1], w[2], 0);
			pdf->nxref++;
			x++;
		}
	}

	Sclose(s);
	if((p = dictget(o, "Root")) != &null)
		pdf->root = pdfref(p);
	if((p = dictget(o, "Info")) != &null)
		pdf->info = pdfref(p);
	pdf->top = o;

	return 0;
err:
	Sclose(s);
	pdfobjfree(o);
	return -1;
}
