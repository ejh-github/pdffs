#include <u.h>
#include <libc.h>
#include <bio.h>
#include "pdf.h"

Stream *
Sbio(void *bio)
{
	Stream *s;

	if((s = calloc(1, sizeof(*s))) == nil)
		return nil;
	s->bio = bio;

	return s;
}

Stream *
Sopen(Object *o)
{
	Stream *s;
	Buffer b, x;
	Object *of, **flts;
	Filter *f;
	int i, nflts;

	s = nil;
	if(pdfeval(&o)->type != Ostream){ /* FIXME open a string object as a stream as well? */
		werrstr("not a stream");
		return nil;
	}

	bufinit(&b, nil, 0);
	if(Sseek(o->pdf->s, o->stream.off, 0) != o->stream.off)
		return nil;
	if(bufreadn(&b, o->pdf->s, o->stream.len) < 0)
		goto err;

	/* see if there are any filters */
	if((of = dictget(o, "Filter")) != &null){
		if(pdfeval(&of)->type == Oname){ /* one filter */
			flts = &of;
			nflts = 1;
		}else if(of->type == Oarray){ /* array of filters */
			flts = of->array.e;
			nflts = of->array.ne;
		}else{
			werrstr("filters type invalid (%T)", of);
			goto err;
		}

		for(i = 0; i < nflts; i++){
			if(flts[i]->type != Oname){
				werrstr("filter type invalid (%T)", flts[i]);
				goto err;
			}
			if((f = filteropen(flts[i]->name, o)) == nil)
				goto err;
			bufinit(&x, nil, 0);
			if(filterrun(f, &b, &x) != 0){
				buffree(&x);
				goto err;
			}
			if(!bufeof(&b))
				fprint(2, "buffer has %d bytes left\n", bufleft(&b));
			buffree(&b);
			b = x;
		}
	}

	if((s = calloc(1, sizeof(*s))) == nil){
		buffree(&b);
		return nil;
	}
	s->buf = b;
	s->objoff = o->stream.off;

	return s;
err:
	werrstr("stream: %r");
	buffree(&b);
	free(s);
	return nil;
}

int
Sread(Stream *s, void *b, int sz)
{
	return s->bio != nil ? Bread(s->bio, b, sz) : bufget(&s->buf, b, sz);
}

int
Sgetc(Stream *s)
{
	int n;
	uchar c;

	n = 1;
	if(s->bio != nil)
		c = Bgetc(s->bio);
	else if((n = bufget(&s->buf, &c, 1)) < 0)
		return -2;

	return n == 0 ? -1 : (int)c;
}

int
Sungetc(Stream *s)
{
	return s->bio != nil ? Bungetc(s->bio) : Sseek(s, -1, 1);
}

int
Soffset(Stream *s)
{
	return s->bio != nil ? Boffset(s->bio) : s->buf.off;
}

int
Sobjoffset(Stream *s)
{
	return s->objoff;
}

int
Ssize(Stream *s)
{
	if(s->bio != nil)
		return -1;
	return bufleft(&s->buf);
}

struct sgetd
{
	Stream *s;
	int eof;
};

static int
Sgetdf(void *vp)
{
	int c;
	struct sgetd *sg = vp;

	c = Sgetc(sg->s);
	if(c < 0)
		sg->eof = 1;
	return c;
}

int
Sgetd(Stream *s, double *dp)
{
	double d;
	struct sgetd b;

	b.s = s;
	b.eof = 0;
	d = charstod(Sgetdf, &b);
	if(b.eof)
		return -1;
	Sungetc(s);
	*dp = d;

	return 1;
}

int
Sgeti(Stream *s, int *i)
{
	double d;
	int res, c;

	while((c = isws(Sgetc(s))));
	if(c < 0)
		return c;
	Sungetc(s);
	res = Sgetd(s, &d);
	*i = d;

	return res;
}

int
Sseek(Stream *s, int off, int whence)
{
	if(s->bio != nil)
		return Bseek(s->bio, off, whence);

	if(whence == 1)
		off += s->buf.off;
	else if(whence == 2)
		off += s->buf.sz;
	if(off < 0){
		werrstr("seek: %d < 0", off);
		off = 0;
	}else if(off > s->buf.sz){
		werrstr("seek: %d > %d", off, s->buf.sz);
		off = s->buf.sz;
	}

	s->buf.off = off;

	return off;
}

char *
Srdstr(Stream *s, int delim, int zero)
{
	int i, len;
	char *line;

	if(s->bio != nil){
		line = Brdstr(s->bio, delim, zero);
		s->linelen = Blinelen(s->bio);
		return line;
	}

	for(i = s->buf.off; i < s->buf.sz;){
		i++;
		if(s->buf.b[i-1] == delim)
			break;
	}
	if(i >= s->buf.sz)
		return nil;
	len = i - s->buf.off;
	if((line = malloc(len+1)) == nil)
		return nil;
	memmove(line, s->buf.b+s->buf.off, len);
	s->buf.off += len;
	if(line[len-1] == delim && zero)
		len--;
	line[len] = 0;
	s->linelen = len;

	return line;
}

int
Slinelen(Stream *s)
{
	return s->linelen;
}

void
Sclose(Stream *s)
{
	if(s == nil)
		return;

	buffree(&s->buf);
	if(s->bio != nil)
		Bterm(s->bio);
	free(s);
}
