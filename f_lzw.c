#include <u.h>
#include <libc.h>
#include "pdf.h"

typedef struct Pt Pt;
typedef struct Tb Tb;

struct Pt {
	int o, sz;
};

struct Tb {
	Pt *pt;
	int npt;

	uchar *p;
	int max;
	int off;

	Pt pr;
};

static void
tbclean(Tb *tb)
{
	int i;

	tb->off = 256;
	tb->npt = 258;

	if(tb->p == nil){
		tb->max = 1024;
		tb->p = malloc(tb->max);
	}
	if(tb->pt == nil){
		tb->pt = malloc(sizeof(Pt)*4096);
		for(i = 0; i < 256; i++){
			tb->p[i] = i;
			tb->pt[i].o = i;
			tb->pt[i].sz = 1;
		}
	}
	memset(tb->pt+tb->npt, 0, sizeof(Pt)*(4096-tb->npt));
}

static int
tbputc(Tb *tb, int c, Buffer *o)
{
	uchar *d;
	Pt *pt;
	int n;

	if(tb->pt == nil)
		tbclean(tb);

	/* firt, make sure things will fit in */
	if(tb->max <= tb->off*2){
		tb->max = tb->off*4;
		tb->p = realloc(tb->p, tb->max);
	}

	pt = &tb->pt[c];
	if(pt->sz > 0){
		d = tb->p + pt->o;
		n = pt->sz;

		if(tb->pr.sz > 0){
			/* have prefix - insert another point */
			pt = &tb->pt[tb->npt++];
			pt->o = tb->off;
			pt->sz = tb->pr.sz + 1;
			memmove(tb->p + pt->o, tb->p + tb->pr.o, tb->pr.sz);
			tb->p[pt->o + pt->sz - 1] = d[0];
			tb->off += pt->sz;
		}

		/* update prefix */
		tb->pr.o = d - tb->p;
		tb->pr.sz = n;
	}else{
		pt = &tb->pt[tb->npt++];
		pt->o = tb->off;
		pt->sz = tb->pr.sz + 1;
		memmove(tb->p + pt->o, tb->p + tb->pr.o, tb->pr.sz);
		tb->p[pt->o + pt->sz - 1] = tb->p[tb->pr.o];
		tb->off += pt->sz;

		tb->pr.o = pt->o;
		tb->pr.sz = pt->sz;

		d = tb->p + pt->o;
		n = pt->sz;
	}

	bufput(o, d, n);

	if(tb->npt < 511)
		return 9;
	if(tb->npt < 1023)
		return 10;
	if(tb->npt < 2047)
		return 11;
	if(tb->npt < 4097)
		return 12;

	return -1;
}

static void
tbfree(Tb *tb)
{
	free(tb->p);
	free(tb->pt);
	memset(tb, 0, sizeof(*tb));
}

static int
flreadall(void *aux, Buffer *bi, Buffer *bo)
{
	int i, j, x, insz, width, w;
	uchar *in;
	Tb tb;

	memset(&tb, 0, sizeof(tb));
	in = bufdata(bi, &insz);
	width = 9;
	x = 0;
	for(i = w = 0, j = 7; i < insz; j--){
		x = x<<1 | ((in[i] & (1<<j)) >> j);
		if(j < 1){
			i++;
			j = 8;
		}

		if(++w == width){
			if(x == 257) /* EOD */
				break;

			if(x == 256){ /* CLEAR */
				tbclean(&tb);
				width = 9;
			}else if((width = tbputc(&tb, x, bo)) < 0){
				werrstr("points overflow");
				tbfree(&tb);
				return -1;
			}

			w = 0;
			x = 0;
		}
	}

	tbfree(&tb);
	bi->off = bi->sz;

	return unpredict(aux, bo);
}

Filter filterLZW = {
	.name = "LZWDecode",
	.readall = flreadall,
	.open = flopenpredict,
	.close = flclosepredict,
};
