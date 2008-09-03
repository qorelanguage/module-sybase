AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

noinst_HEADERS = src/AbstractQoreAsn1Object.h \
	src/QC_ASN1BitString.h \
	src/QC_ASN1Boolean.h \
	src/QC_ASN1Integer.h \
	src/QC_ASN1ObjectIdentifier.h \
	src/QC_ASN1OctetString.h \
	src/QC_ASN1Sequence.h \
	src/QC_ASN1String.h \
	src/QC_ASN1UTF8String.h \
	src/qore-asn1.h

EXTRA_DIST = COPYING ChangeLog AUTHORS README \
	qore-asn1-module.spec docs/asn1-module-doc.html

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
