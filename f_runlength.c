#include <u.h>
#include <libc.h>
#include "pdf.h"

static int
flreadall(void *aux, Buffer *bi, Buffer *bo)
{
	USED(aux);
	bufput(bo, bi->b, bi->sz);
	return 0;
}

Filter filterRunLength = {
	.name = "RunLengthDecode",
	.readall = flreadall,
};
