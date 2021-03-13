#include <u.h>
#include <libc.h>
#include <bio.h>
#include "pdf.h"

static int
bufgrow(Buffer *b, int sz)
{
	uchar *r;
	int maxsz;

	if(b->maxsz < 1){
		if((b->b = mallocz(128, 1)) == nil)
			return -1;
		b->maxsz = 128;
	}
	for(maxsz = b->maxsz; b->sz+sz > maxsz; maxsz *= 2);
	if(maxsz != b->maxsz){
		if((r = realloc(b->b, maxsz)) == nil)
			return -1;
		memset(r+b->maxsz, 0, maxsz-b->maxsz);
		b->b = r;
		b->maxsz = maxsz;
	}

	return 0;
}

void
bufinit(Buffer *b, uchar *d, int sz)
{
	memset(b, 0, sizeof(*b));
	if(d != nil){
		b->b = d;
		b->sz = sz;
		b->ro = 1;
	}
}

void
buffree(Buffer *b)
{
	if(b->ro == 0)
		free(b->b);
}

int
bufeof(Buffer *b)
{
	return bufleft(b) == 0;
}

int
bufleft(Buffer *b)
{
	return b->sz - b->off;
}

uchar *
bufdata(Buffer *b, int *sz)
{
	*sz = b->sz;
	return b->b;
}

int
bufreadn(Buffer *b, Stream *s, int sz)
{
	int n, end;

	if(bufgrow(b, sz) != 0)
		return -1;
	for(end = b->sz+sz; b->sz < end; b->sz += n){
		if((n = Sread(s, b->b+b->sz, sz)) < 1)
			return -1;
		sz -= n;
	}
	return 0;
}

int
bufput(Buffer *b, uchar *d, int sz)
{
	if(b->ro)
		sysfatal("bufferput on readonly buffer");
	if(bufgrow(b, sz) != 0)
		return -1;

	memmove(b->b+b->sz, d, sz);
	b->sz += sz;

	return sz;
}

int
bufget(Buffer *b, uchar *d, int sz)
{
	if(sz == 0)
		return 0;

	if(b->off > b->sz)
		sysfatal("buffer: off(%d) > sz(%d)", b->off, b->sz);
	if(sz > b->sz - b->off)
		sz = b->sz - b->off;
	memmove(d, b->b+b->off, sz);
	b->off += sz;

	return sz;
}

void
bufdump(Buffer *b)
{
	Biobuf bio;
	int i, j;

	Binit(&bio, 2, OWRITE);
	Bprint(&bio, "%d bytes:\n", b->sz);
	for(i = 0; i < b->sz;){
		Bprint(&bio, "%04x\t", i);
		for(j = 0; i < b->sz && j < 16; j++, i++)
			Bprint(&bio, "%02x%s", b->b[i], (j+1)&7 ? " " : "  ");
		Bprint(&bio, "\n");
	}
	Bterm(&bio);
}
