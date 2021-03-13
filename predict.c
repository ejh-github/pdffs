#include <u.h>
#include <libc.h>
#include "pdf.h"

struct PredictParms {
	int predictor;
	int columns;
};

static uchar 
paeth(uchar a, uchar b, uchar c)
{
	int p, pa, pb, pc;

	p = a + b - c;
	pa = abs(p - a);
	pb = abs(p - b);
	pc = abs(p - c);

	if(pa <= pb && pa <= pc)
		return a;
	return pb <= pc ? b : c;
}

static int
pngunpredict(int pred, uchar *buf, uchar *up, int len)
{
	int i;

	switch(pred){
	case 0: /* None */
		break;

	case 1: /* Sub */
		for(i = 1; i < len; ++i)
			buf[i] += buf[i-1];
		break;

	case 2: /* Up */
		for(i = 0; i < len; ++i)
			buf[i] += up[i];
		break;

	case 3: /* Average */
		buf[0] += up[0]/2;
		for(i = 1; i < len; ++i)
			buf[i] += (buf[i-1]+up[i])/2;
		break;

	case 4: /* Paeth */
		buf[0] += paeth(0, up[0], 0);
		for(i = 0; i < len; ++i)
			buf[i] += paeth(buf[i-1], up[i], up[i-1]);
		break;

	/* FIXME 5 optimum??? */

	default:
		werrstr("unsupported predictor %d", pred);
		return -1;
	}

	return 0;
}

int
unpredict(PredictParms *pp, Buffer *bo)
{
	uchar *x, *y, *zero;
	int i, r, rows, n;

	if(pp->predictor < 10 || pp->columns < 1)
		return 0;

	n = pp->columns + 1;
	rows = bo->sz/n;
	x = bo->b;
	y = bo->b;
	zero = mallocz(pp->columns, 1);
	for(i = r = 0; i < rows && r == 0; i++, x += n, y += n)
		r = pngunpredict(x[0], x+1, i < 1 ? zero : y+1-n, pp->columns);
	free(zero);

	if(r != 0)
		return r;

	x = bo->b;
	y = bo->b+1;
	for(i = 0; i < rows; i++, x += pp->columns, y += n)
		memmove(x, y, pp->columns);
	bo->sz -= rows;

	return r;
}

int
flopenpredict(Filter *f, Object *o)
{
	Object *parms;
	PredictParms *pp;
	int predictor, columns;

	parms = dictget(o, "DecodeParms");
	predictor = dictint(parms, "Predictor");
	columns = dictint(parms, "Columns");
	if((predictor >= 2 && predictor < 10) || predictor >= 15){
		werrstr("unsupported flate predictor %d", predictor);
		return -1;
	}
	if(predictor >= 10 && predictor <= 15 && columns < 1){
		werrstr("invalid columns %d for predictor %d", columns, predictor);
		return -1;
	}

	if((pp = malloc(sizeof(*pp))) == nil)
		return -1;
	pp->predictor = predictor;
	pp->columns = columns;
	f->aux = pp;

	return 0;
}

void
flclosepredict(Filter *f)
{
	free(f->aux);
}
