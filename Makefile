# $FreeBSD$

.PATH:	${SRCTOP}/sys/dev/acpi_support

KMOD=	acpi_hidd
SRCS=	acpi_hidd.c opt_acpi.h opt_evdev.h acpi_if.h bus_if.h device_if.h
SRCS+= opt_ddb.h

.include <bsd.kmod.mk>
