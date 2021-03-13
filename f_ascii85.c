#include <u.h>
#include <libc.h>
#include "pdf.h"

static int
flreadall(void *aux, Buffer *bi, Buffer *bo)
{
	uchar *in, c[4];
	int i, j, insz;
	u32int x;

	USED(aux);

	in = bufdata(bi, &insz);
	for(i = j = 0; i < insz; i++){
		if(!isws(in[i]))
			in[j++] = in[i];
	}
	insz = j;
	for(i = 0; i < insz; i += 5){
		for(x = 0, j = 0; j < 5; j++)
			x = x*85 + ((i+j < insz ? in[i+j] : 'u') - 33);
		c[0] = x >> 24;
		c[1] = x >> 16;
		c[2] = x >> 8;
		c[3] = x;
		bufput(bo, c, 4);
	}
	bi->off = bi->sz;

	return 0;
}

Filter filterASCII85 = {
	.name = "ASCII85Decode",
	.readall = flreadall,
};
