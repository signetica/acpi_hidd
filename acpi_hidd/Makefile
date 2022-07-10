# Created by: Cyrus Rahman <crahman@gmail.com>

PORTNAME=	acpi_hidd
DISTVERSION=	1.1.2
CATEGORIES=	sysutils

MAINTAINER=	crahman@gmail.com
COMMENT=	Provide basic ACPI HID kernel services

LICENSE=	BSD2CLAUSE

USES=		kmod uidfix

USE_GITHUB=	yes
GH_ACCOUNT=	signetica

OPTIONS_DEFINE=		EVDEV_SUPPORT
OPTIONS_DEFAULT=	EVDEV_SUPPORT
EVDEV_SUPPORT_DESC=	Enable evdev/libinput support
EVDEV_SUPPORT_CFLAGS=	-DEVDEV_SUPPORT

pre-install:
	${INSTALL_MAN} ${WRKSRC}/acpi_hidd.4 ${STAGEDIR}${MANPREFIX}/man/man4

.include <bsd.port.mk>
