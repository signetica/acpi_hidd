/*-
 * Copyright (c) 2020 Cyrus Rahman
 * Copyright (c) 2000 Mitsaru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2000 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000 BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include "opt_evdev.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/sysctl.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>

#ifdef EVDEV_SUPPORT
#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>
#endif

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_OEM
ACPI_MODULE_NAME("HIDD")

struct acpi_hidd_softc {
    device_t	hidd_dev;
    ACPI_HANDLE	hidd_handle;
    int		hidd_brightness;
#define		ACPI_BRIGHTNESS_INCREMENT   5		/* 5% of 0-100 */
    int		hidd_brightness_keycontrol;
    int		hidd_brightness_keycontrol_unlock;
#ifdef EVDEV_SUPPORT
    struct evdev_dev *hidd_evdev;
#endif
    struct sysctl_ctx_list  hidd_sysctl_ctx;
};

/* HIDD keycodes for GPD MicroPC */
#define ACPI_NOTIFY_HIDD_BRIGHTNESS_UP	    0x13
#define ACPI_NOTIFY_HIDD_BRIGHTNESS_DOWN    0x14

static int	acpi_hidd_probe(device_t dev);
static int	acpi_hidd_attach(device_t dev);
static int	acpi_hidd_detach(device_t dev);
static int	acpi_hidd_suspend(device_t dev);
static int	acpi_hidd_resume(device_t dev);
static void 	acpi_hidd_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context);
static int	acpi_hidd_brightness_sysctl(SYSCTL_HANDLER_ARGS);
static int	acpi_hidd_brightness_keycontrol_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t acpi_hidd_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_hidd_probe),
    DEVMETHOD(device_attach,	acpi_hidd_attach),
    DEVMETHOD(device_detach,	acpi_hidd_detach),
    DEVMETHOD(device_suspend,	acpi_hidd_suspend),
    DEVMETHOD(device_resume,	acpi_hidd_resume),
    DEVMETHOD_END
};

static driver_t acpi_hidd_driver = {
    "acpi_hidd",
    acpi_hidd_methods,
    sizeof(struct acpi_hidd_softc),
};

static devclass_t acpi_hidd_devclass;
DRIVER_MODULE(acpi_hidd, acpi, acpi_hidd_driver, acpi_hidd_devclass, 0, 0);

static int
acpi_hidd_probe(device_t dev)
{
    static char *hidd_ids[] = { "INT33D5", NULL };
    int rv;

    if (acpi_disabled("hidd"))
	return (ENXIO);
    rv = ACPI_ID_PROBE(device_get_parent(dev), dev, hidd_ids, NULL);
    if (rv <= 0)
	device_set_desc(dev, "Human Interface Device");
    return (rv);
}

static int
acpi_hidd_attach(device_t dev)
{
    struct acpi_hidd_softc	*sc;
    struct acpi_softc		*acpi_sc;
    struct sysctl_oid		*oid;
    ACPI_STATUS			status;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    sc = device_get_softc(dev);
    sc->hidd_dev = dev;
    sc->hidd_handle = acpi_get_handle(dev);
    sc->hidd_brightness_keycontrol = 1;
    acpi_sc = acpi_device_get_parent_softc(sc->hidd_dev);

    sysctl_ctx_init(&sc->hidd_sysctl_ctx);
    oid = SYSCTL_ADD_NODE(&sc->hidd_sysctl_ctx,
			  SYSCTL_CHILDREN(acpi_sc->acpi_sysctl_tree),
			  OID_AUTO, "hidd", CTLFLAG_RW, 0, "ACPI hidd devices");
    SYSCTL_ADD_PROC(&sc->hidd_sysctl_ctx, SYSCTL_CHILDREN(oid),
		   OID_AUTO, "brightness",
		   CTLTYPE_INT|CTLFLAG_RW|CTLFLAG_MPSAFE,
		   sc, 0, acpi_hidd_brightness_sysctl,
		   "I", "Screen brightness setting (0-100)");
    SYSCTL_ADD_PROC(&sc->hidd_sysctl_ctx, SYSCTL_CHILDREN(oid),
		   OID_AUTO, "brightness_keycontrol",
		   CTLTYPE_INT|CTLFLAG_RW|CTLFLAG_ANYBODY|CTLFLAG_MPSAFE,
		   sc, 0, acpi_hidd_brightness_keycontrol_sysctl,
		   "I", "Enable keyboard control of screen brightness");
    SYSCTL_ADD_INT(&sc->hidd_sysctl_ctx, SYSCTL_CHILDREN(oid),
		   OID_AUTO, "brightness_keycontrol_unlock", CTLFLAG_RW,
		   &sc->hidd_brightness_keycontrol_unlock, 0,
		   "Unlock hw.acpi.hidd.hidd_brightness_keycontrol");

    status = AcpiInstallNotifyHandler(sc->hidd_handle, ACPI_DEVICE_NOTIFY,
				      acpi_hidd_notify_handler, sc);
    if (ACPI_FAILURE(status)) {
	device_printf(sc->hidd_dev, "couldn't install notify handler - %s\n",
		      AcpiFormatException(status));
	sysctl_ctx_free(&sc->hidd_sysctl_ctx);
	return_VALUE (ENXIO);
    }

#ifdef EVDEV_SUPPORT
    sc->hidd_evdev = evdev_alloc();
    evdev_set_name(sc->hidd_evdev, device_get_desc(dev));
    evdev_set_phys(sc->hidd_evdev, device_get_nameunit(dev));
    evdev_set_id(sc->hidd_evdev, BUS_HOST, 0, 0, 1);
    evdev_support_event(sc->hidd_evdev, EV_KEY);
    evdev_support_key(sc->hidd_evdev, KEY_BRIGHTNESSDOWN);
    evdev_support_key(sc->hidd_evdev, KEY_BRIGHTNESSUP);

    if (evdev_register(sc->hidd_evdev)) {
	evdev_free(sc->hidd_evdev);
	AcpiRemoveNotifyHandler(sc->hidd_handle, ACPI_DEVICE_NOTIFY,
				acpi_hidd_notify_handler);
	sysctl_ctx_free(&sc->hidd_sysctl_ctx);
	return_VALUE (ENXIO);
    }
#endif

    return_VALUE (0);
}

static int
acpi_hidd_detach(device_t dev)
{
    struct acpi_hidd_softc	*sc;
    ACPI_STATUS			status;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    sc = device_get_softc(dev);
    sysctl_ctx_free(&sc->hidd_sysctl_ctx);

#ifdef EVDEV_SUPPORT
    evdev_free(sc->hidd_evdev);
#endif

    status = AcpiRemoveNotifyHandler(sc->hidd_handle, ACPI_DEVICE_NOTIFY,
				     acpi_hidd_notify_handler);
    if (ACPI_FAILURE(status)) {
	device_printf(sc->hidd_dev, "couldn't remove notify handler - %s\n",
		      AcpiFormatException(status));
	return_VALUE (ENXIO);
    }

    return_VALUE (0);
}

static int
acpi_hidd_suspend(device_t dev)
{
    return (0);
}

static int
acpi_hidd_resume(device_t dev)
{
    return (0);
}

static void 
acpi_hidd_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
    struct acpi_hidd_softc  *sc;
    struct acpi_softc	    *acpi_sc;
#ifdef EVDEV_SUPPORT
    uint16_t key;
#endif
    ACPI_STATUS status;
    int hidd_hidm;
    char notify_buf[16];

    ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, notify);

    sc = (struct acpi_hidd_softc *)context;
    acpi_sc = acpi_device_get_parent_softc(sc->hidd_dev);

    status = acpi_GetInteger(sc->hidd_handle, "\\_SB.HIDD.HDEM", &hidd_hidm);
    if (ACPI_FAILURE(status)) {
	ACPI_VPRINT(sc->hidd_dev, acpi_sc, "error fetching hidd code -- %s\n",
		    AcpiFormatException(status));
	return_VOID;
    }

    ACPI_VPRINT(sc->hidd_dev, acpi_sc, "hidd code %#x\n", hidd_hidm);
    if (sc->hidd_brightness_keycontrol &&
	(hidd_hidm == ACPI_NOTIFY_HIDD_BRIGHTNESS_UP ||
	 hidd_hidm == ACPI_NOTIFY_HIDD_BRIGHTNESS_DOWN)) {
	    acpi_GetInteger(sc->hidd_handle, "\\_SB.DPLY._BQC", &sc->hidd_brightness);
	    if (hidd_hidm == ACPI_NOTIFY_HIDD_BRIGHTNESS_UP) {
		sc->hidd_brightness += ACPI_BRIGHTNESS_INCREMENT;
		sc->hidd_brightness =
		    sc->hidd_brightness > 100 ? 100 : sc->hidd_brightness;
	    } else {
		sc->hidd_brightness -= ACPI_BRIGHTNESS_INCREMENT;
		sc->hidd_brightness =
		    sc->hidd_brightness < 0 ? 0 : sc->hidd_brightness;
	    }
	    acpi_SetInteger(sc->hidd_handle, "\\_SB.DPLY._BCM", sc->hidd_brightness);

	    /* Keep devd(8) notification consistent with acpi_video.c */
	    snprintf(notify_buf, sizeof(notify_buf), "notify=%d", sc->hidd_brightness);
	    devctl_notify("ACPI", "Video", "brightness", notify_buf);
    } else
	    acpi_UserNotify("HIDD", h, hidd_hidm);

#ifdef EVDEV_SUPPORT
    /* Support for media keys should go here. */
    switch (hidd_hidm) {
    case ACPI_NOTIFY_HIDD_BRIGHTNESS_UP:
	key = KEY_BRIGHTNESSUP;
	break;
    case ACPI_NOTIFY_HIDD_BRIGHTNESS_DOWN:
	key = KEY_BRIGHTNESSDOWN;
	break;
    default:
	goto nokey;
    }
    evdev_push_key(sc->hidd_evdev, key, 1);
    evdev_sync(sc->hidd_evdev);
    evdev_push_key(sc->hidd_evdev, key, 0);
    evdev_sync(sc->hidd_evdev);
nokey:
#endif

    return_VOID;
}

static int
acpi_hidd_brightness_sysctl(SYSCTL_HANDLER_ARGS)
{
    struct acpi_hidd_softc *sc;
    int brightness, error;
    char notify_buf[16];

    sc = (struct acpi_hidd_softc *)arg1;
    acpi_GetInteger(sc->hidd_handle, "\\_SB.DPLY._BQC", &sc->hidd_brightness);

    brightness = sc->hidd_brightness;
    error = sysctl_handle_int(oidp, &brightness, 0, req);
    if (error != 0 || req->newptr == NULL)
	return (error);
    if (brightness < 0 || brightness > 100)
	return (EINVAL);

    sc->hidd_brightness = brightness;
    acpi_SetInteger(sc->hidd_handle, "\\_SB.DPLY._BCM", sc->hidd_brightness);

    snprintf(notify_buf, sizeof(notify_buf), "notify=%d", sc->hidd_brightness);
    devctl_notify("ACPI", "Video", "brightness", notify_buf);

    return_VALUE (0);
} 

/*
 * The hw.acpi.hidd.hidd_brightness_keycontrol_unlock sysctl will unlock
 * hw.acpi.hidd.hidd_brightness_keycontrol if set.
 *
 * If unlocked, the hidd_brightness_keycontrol sysctl is writable by any user or
 * process.  This allows a user to adjust it according to the demands of their
 * windowing system.
 *
 * On a server this facility might be undesirable, so by default the sysctl is
 * locked.
 */
static int
acpi_hidd_brightness_keycontrol_sysctl(SYSCTL_HANDLER_ARGS) {
    struct acpi_hidd_softc	*sc;
    int keycontrol, error;

    sc = (struct acpi_hidd_softc *)arg1;

    keycontrol = sc->hidd_brightness_keycontrol;
    error = sysctl_handle_int(oidp, &keycontrol, 0, req);
    if (error != 0 || req->newptr == NULL)
	return (error);
    if (!sc->hidd_brightness_keycontrol_unlock)
	return (EPERM);

    sc->hidd_brightness_keycontrol = keycontrol;

    return_VALUE (0);
}
