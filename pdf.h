enum {
	Obool,   /* 7.3.2 */
	Onum,    /* 7.3.3 */
	Ostr,    /* 7.3.4 */
	Oname,   /* 7.3.5 */
	Oarray,  /* 7.3.6 */
	Odict,   /* 7.3.7 */
	Ostream, /* 7.3.8 */
	Onull,   /* 7.3.9 */
	Oindir,  /* 7.3.10 */
};

typedef struct Buffer Buffer;
typedef struct Filter Filter;
typedef struct KeyValue KeyValue;
typedef struct Object Object;
typedef struct Pdf Pdf;
typedef struct PredictParms PredictParms;
#pragma incomplete PredictParms;
typedef struct Stream Stream;
typedef struct Xref Xref;

struct Buffer {
	uchar *b;
	int ro;
	int maxsz;
	int sz;
	int off;
};

struct Filter {
	char *name;
	int (*readall)(void *aux, Buffer *bi, Buffer *bo);
	int (*open)(Filter *f, Object *o);
	void (*close)(Filter *f);
	void *aux;
};

struct Object {
	int type;
	int ref;
	Pdf *pdf;
	union {
		int bool;

		struct {
			double d;
			int i;
		}num;

		struct {
			char *str;
			int len;
		};
		char *name;

		struct {
			u32int id;
			u16int gen;
		}indir;

		struct {
			KeyValue *kv;
			int nkv;
		}dict;

		struct {
			Object **e;
			int ne;
		}array;

		struct {
			KeyValue *kv;
			int nkv;
			u32int len; /* packed */
			u32int off;
		}stream;
	};
};

struct KeyValue {
	char *key;
	Object *value;
};

struct Pdf {
	Stream *s;
	Xref *xref;
	int nxref; /* 7.5.4 xref subsection number of objects */

	Object *top;
	Object *root; /* 7.7.2 root object */
	Object *info; /* 14.3.3 info dictionary */
};

struct Xref {
	u32int id;
	union {
		struct { /* uncompressed */
			u32int off;
			u16int gen;
		};

		struct { /* compressed, objstm > 0 */
			u16int index; /* index within ObjStm */
		};
	};
	u16int objstm; /* > 0 means it's compressed and points to the ObjStm */
};

struct Stream {
	Buffer buf;
	void *bio;
	int linelen;
	int objoff;
};

extern Object null;

Pdf *pdfopen(void *bio);
void pdfclose(Pdf *pdf);

/*
 * Parse an object.
 */
Object *pdfobj(Pdf *pdf, Stream *s);

/*
 * Deallocate the object and all its children. Refcount is
 * considered.
 */
void pdfobjfree(Object *o);

/*
 * Return a resolved object or &null if can't. Operation is
 * not recursive, ie values of a dictionary won't be resolved
 * automatically.
 */
Object *pdfeval(Object **o);

/*
 * Increment refcount of an object. Freeing an object (or its
 * parent) decrements ref count.
 */
Object *pdfref(Object *o);

/*
 * Returns 1 if the character is whitespace. 0 otherwise.
 * See 7.2.2.
 */
int isws(int c);

/*
 * Returns 1 if the character is a delimiter. 0 otherwise.
 * See 7.2.2.
 */
int isdelim(int c);

int isutf8(char *s, int len);

int arraylen(Object *o);
Object *arrayget(Object *o, int i);
int arrayint(Object *o, int i);

Object *dictget(Object *o, char *name);
int dictint(Object *o, char *name);
int dictintopt(Object *o, char *name, int def);
char *dictstring(Object *o, char *name);
Object *dictdict(Object *o, char *name);
int dictints(Object *o, char *name, int *el, int nel);

Stream *Sbio(void *bio);
Stream *Sopen(Object *o);
int Sread(Stream *s, void *b, int sz);
int Sgetc(Stream *s);
int Sungetc(Stream *s);
int Ssize(Stream *s);
int Soffset(Stream *s);
int Sobjoffset(Stream *s);
int Sseek(Stream *s, int off, int whence);
void Sclose(Stream *s);
int Sgetd(Stream *s, double *d);
int Sgeti(Stream *s, int *i);
char *Srdstr(Stream *s, int delim, int zero);
int Slinelen(Stream *s);

Filter *filteropen(char *name, Object *o);
int filterrun(Filter *f, Buffer *bi, Buffer *bo);
void filterclose(Filter *f);

/* 7.4.4.4 LZW and Flate predictor functions */
int unpredict(PredictParms *pp, Buffer *bo);
int flopenpredict(Filter *f, Object *o);
void flclosepredict(Filter *f);

void bufinit(Buffer *b, uchar *d, int sz);
void buffree(Buffer *b);
int bufeof(Buffer *b);
int bufleft(Buffer *b);
uchar *bufdata(Buffer *b, int *sz);
int bufreadn(Buffer *b, Stream *s, int sz);
int bufput(Buffer *b, uchar *d, int sz);
int bufget(Buffer *b, uchar *d, int sz);
void bufdump(Buffer *b);

#pragma varargck type "O" Object*
#pragma varargck type "T" Object*
#pragma varargck type "⊗" Xref
int Ofmt(Fmt *f);
int Tfmt(Fmt *f);
int ⊗fmt(Fmt *f);

int xrefreadold(Pdf *pdf);
int xrefreadstream(Pdf *pdf);
