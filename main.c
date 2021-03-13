#include <u.h>
#include <libc.h>
#include <thread.h>
#include <ctype.h>
#include <bio.h>
#include <flate.h>
#include "pdf.h"

int mainstacksize = 128*1024;

static void
usage(void)
{
	fprint(2, "usage: %s FILE\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	Object *v, o;
	Biobuf *b;
	Stream *s;
	Pdf *pdf;
	int i, n, k;

	quotefmtinstall();
	inflateinit();

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc < 1)
		usage();
	if((b = Bopen(argv[0], OREAD)) == nil)
		sysfatal("%r");
	if((pdf = pdfopen(b)) == nil)
		sysfatal("%s: %r", argv[0]);
	for(v = pdf->top, i = 1; v != nil && i < argc; i++){
		if(isdigit(argv[i][0])){
			n = atoi(argv[i]);
			v = arrayget(v, n);
		}else if(argv[i][0] == '.' && argv[i][1] == 0 && v->type == Ostream){
			if((s = Sopen(v)) == nil)
				sysfatal("%r");
			if(write(1, s->buf.b, s->buf.sz) != s->buf.sz)
				sysfatal("write failed");
			Sclose(s);
			v = nil;
			break;
		}else if(argv[i][0] == '*' && argv[i][1] == 0 && v->type == Odict){
			for(k = 0; k < v->dict.nkv; k++)
				print("%s\n", v->dict.kv[k].key);
			v = nil;
			break;
		}else if(argv[i][0] == '@' && argv[i][1] == 0 && v->type == Ostream){
			fprint(2, "%d %d\n", v->stream.off, v->stream.len);
			v = nil;
			break;
		}else if(argv[i][0] == '@' && isdigit(argv[i][1])){
			o.ref = 1;
			o.pdf = pdf;
			o.type = Oindir;
			o.indir.id = atoi(argv[i]+1);
			v = &o;
			pdfeval(&v);
		}else{
			v = dictget(v, argv[i]);
		}
	}
	if(v == &null)
		fprint(2, "%r\n");
	else if(v != nil)
		print("%O\n", v);
/*
	if((v = dictget(pdf->info, "Creator")) != nil)
		fprint(2, "creator: %s\n", v->str);
	if((v = dictget(pdf->info, "Producer")) != nil)
		fprint(2, "producer: %s\n", v->str);
*/
	pdfclose(pdf);

	threadexitsall(nil);
}
