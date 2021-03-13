</$objtype/mkfile

BIN=/$objtype/bin
TARG=pdffs

OFILES=\
	array.$O\
	buffer.$O\
	dict.$O\
	eval.$O\
	f_ascii85.$O\
	f_asciihex.$O\
	f_ccittfax.$O\
	f_crypt.$O\
	f_dct.$O\
	f_flate.$O\
	f_jbig2.$O\
	f_jpx.$O\
	f_lzw.$O\
	f_runlength.$O\
	filter.$O\
	main.$O\
	misc.$O\
	name.$O\
	object.$O\
	pdf.$O\
	pdffs.$O\
	predict.$O\
	stream.$O\
	string.$O\
	xref.$O\

HFILES=\
	pdf.h\

UPDATE=\
	$HFILES\
	${OFILES:%.$O=%.c}\
	mkfile\

default:V:	all

</sys/src/cmd/mkone
