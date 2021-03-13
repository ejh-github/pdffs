#include <u.h>
#include <libc.h>
#include "pdf.h"

/* 7.4 Filters */

extern Filter filterASCII85;
extern Filter filterASCIIHex;
extern Filter filterCCITTFax;
extern Filter filterCrypt;
extern Filter filterDCT;
extern Filter filterFlate;
extern Filter filterJBIG2;
extern Filter filterJPX;
extern Filter filterLZW;
extern Filter filterRunLength;

static Filter *filters[] = {
	&filterASCII85,
	&filterASCIIHex,
	&filterCCITTFax,
	&filterCrypt,
	&filterDCT,
	&filterFlate,
	&filterJBIG2,
	&filterJPX,
	&filterLZW,
	&filterRunLength,
};

Filter *
filteropen(char *name, Object *o)
{
	int i;
	Filter *f;

	for(i = 0; i < nelem(filters) && strcmp(filters[i]->name, name) != 0; i++);
	if(i >= nelem(filters)){
		werrstr("no such filter %q", name);
		return nil;
	}
	if(filters[i]->readall == nil){
		werrstr("filter %q not implemented", name);
		return nil;
	}
	if((f = malloc(sizeof(*f))) == nil)
		return nil;
	memmove(f, filters[i], sizeof(*f));
	if(f->open != nil && f->open(f, o) != 0){
		free(f);
		return nil;
	}

	return f;
}

int
filterrun(Filter *f, Buffer *bi, Buffer *bo)
{
	if(f->readall(f->aux, bi, bo) != 0){
		werrstr("filter[%s]: %r", f->name);
		return -1;
	}
	return 0;
}

void
filterclose(Filter *f)
{
	if(f->close != nil)
		f->close(f);
	free(f);
}
