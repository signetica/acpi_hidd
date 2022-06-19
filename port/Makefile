# Created by: Cyrus Rahman <crahman@gmail.com>

PORTNAME=	acpi_hidd
DISTVERSION=	0.9.0
CATEGORIES=	sysutils

MAINTAINER=	crahman@gmail.com
COMMENT=	Provide basic ACPI HID kernel services

LICENSE=	BSD2CLAUSE

WRKSRC=		${WRKDIR}/${PORTNAME}
USES=		kmod uidfix

USE_GITHUB=	yes
GH_ACCOUNT=	signetica

pre-install:
	${INSTALL_MAN} ${WRKSRC}/acpi_hidd.4 ${STAGEDIR}${MANPREFIX}/man/man4

.include <bsd.port.mk>
