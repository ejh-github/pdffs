#include <u.h>
#include <libc.h>
#include <flate.h>
#include "pdf.h"


static int
bw(void *aux, void *d, int n)
{
	return bufput(aux, d, n);
}

static int
bget(void *aux)
{
	uchar c;

	return bufget(aux, &c, 1) == 1 ? c : -1;
}

static int
flreadall(void *aux, Buffer *bi, Buffer *bo)
{
	int r;

	do{
		r = inflatezlib(bo, bw, bi, bget);
	}while(r == FlateOk && !bufeof(bi));

	if(r != FlateOk && bufleft(bo) < 1){
		werrstr("%s", flateerr(r));
		return -1;
	}

	return unpredict(aux, bo);
}

Filter filterFlate = {
	.name = "FlateDecode",
	.readall = flreadall,
	.open = flopenpredict,
	.close = flclosepredict,
};
