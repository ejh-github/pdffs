#include <u.h>
#include <libc.h>
#include <ctype.h>
#include "pdf.h"

static int
trailerread(Pdf *pdf)
{
	Object *o;
	int prev;

	if((o = pdfobj(pdf, pdf->s)) == nil)
		goto err;
	if(o->type != Odict){
		werrstr("isn't a dictionary");
		goto err;
	}
	if((prev = dictint(o, "Prev")) > 0 && (Sseek(pdf->s, prev, 0) < 0 || xrefreadold(pdf) != 0))
		goto err;

	pdf->root = pdfref(dictget(o, "Root"));
	pdf->info = pdfref(dictget(o, "Info"));
	pdf->top = o;

	return 0;
err:
	pdfobjfree(o);
	return -1;
}

Pdf *
pdfopen(void *bio)
{
	Pdf *pdf;
	Object *o;
	Stream *stream;
	char tmp[64], *s, *x;
	int xreftb; /* 7.5.4 xref table offset from the beginning of the file */
	int i, n, off;

	fmtinstall('H', encodefmt);
	fmtinstall('O', Ofmt);
	fmtinstall('T', Tfmt);
	fmtinstall(L'⊗', ⊗fmt);

	o = nil;
	pdf = nil;
	if((stream = Sbio(bio)) == nil || (pdf = calloc(1, sizeof(*pdf))) == nil)
		goto err;
	pdf->s = stream;

	/* check header */
	if(Sread(stream, tmp, 8) != 8 ||
	   strncmp(tmp, "%PDF-", 5) != 0 || !isdigit(tmp[5]) || tmp[6] != '.' || !isdigit(tmp[7])){
		werrstr("not a pdf");
		goto err;
	}

	/* 7.5.4, 7.5.8 xref table */

	/* read a block of data */
	n = sizeof(tmp)-1;
	Sseek(stream, -n, 2);
	if(Sread(stream, tmp, n) != n){
badtrailer:
		werrstr("invalid trailer");
		goto err;
	}
	tmp[n] = 0;

	/* search for a valid string that the block ends with */
	for(i = n-1, s = &tmp[i]; i > 0 && *s != 0; i--, s--);
	s++;

	/* find "startxref" */
	if((x = strrchr(s, 'f')) == nil || !isws(x[1]) || x-8 < s+1 || memcmp(x-8, "startxref", 9) != 0)
		goto badtrailer;
	x++;
	if((xreftb = strtol(x, nil, 10)) < 1)
		goto badtrailer;

	/* read xref */
	if(Sseek(stream, xreftb, 0) != xreftb){
		werrstr("xref position out of range");
		goto err;
	}
	for(;;){
		while(isspace(Sgetc(stream)));
		Sungetc(stream);
		off = Soffset(stream);
		if(Sread(stream, tmp, sizeof(tmp)) < 8){
badxref:
			werrstr("invalid xref: %r");
			goto err;
		}
		if(memcmp(tmp, "xref", 4) == 0){
			if(Sseek(stream, -sizeof(tmp), 1) < 0 || xrefreadold(pdf) != 0)
				goto err;
			/* there could be more updates, try it */
		}else if(memcmp(tmp, "trailer", 7) == 0){ /* 7.5.5 file trailer */
			/* move to the trailer dictionary */
			n = off + 8;
			if(Sseek(stream, n, 0) != n || trailerread(pdf) != 0){
				werrstr("invalid trailer: %r");
				goto err;
			}
			/* trailer is supposed to be the last thing */
			break;
		}else if(isdigit(tmp[0])){ /* could be 7.5.8 xref stream (since PDF 1.5) */
			if(Sseek(stream, xreftb, 0) != xreftb)
				goto badxref;
			if(xrefreadstream(pdf) != 0)
				goto err;
			break;
		}
	}

	/* root is required, info is optional */
	if(pdf->root == &null){
		werrstr("no root: %r");
		goto err;
	}

	return pdf;
err:
	werrstr("pdfopen: %r [at %p]", (void*)Soffset(stream));
	pdfclose(pdf);
	pdfobjfree(o);
	return nil;
}

void
pdfclose(Pdf *pdf)
{
	if(pdf == nil)
		return;
	if(pdf->s != nil)
		Sclose(pdf->s);
	free(pdf->xref);
	free(pdf);
}
