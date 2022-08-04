# acpi_hidd

ACPI human interface device driver for FreeBSD

# Description

The acpi_hidd kernel module provides driver services for ACPI HIDD hardware,
in particular brightness and media control keyboard support.

The driver has devd(8), evdev(4), and sysctl(8) interfaces; the sysctl(8)
interface allows inspection and control of the screen brightness, the evdev(4)
interface provides a mechanism for applications to directly receive keystrokes
corresponding to ACPI events, and the devd(8) interface provides a way to have
userland programs run when ACPI keys are pressed.

# Notes

The collection of key codes is very small at this time, comprising only the
two that are present on the GPD MicroPC.  This does not prevent use of the
driver with devd(8), but contributions from users will make the evdev(4)
interface more useful.  Submit your computer's key/code pairs and its make
and model as an issue here on GitHub.

The current key code list follows:

	// HIDD keycodes for GPD MicroPC
	#define ACPI_NOTIFY_HIDD_BRIGHTNESS_UP      0x13
	#define ACPI_NOTIFY_HIDD_BRIGHTNESS_DOWN    0x14

To find the codes associated with any ACPI keys on your keyboard, install this driver,
set 'sysctl hw.acpi.verbose=1', and look in the system log for messages like
'acpi_hidd0: hidd code 0x??' that appear when you press those keys.

Look up the name of what the key is supposed
to do in /usr/src/sys/dev/evdev/input-event-codes.h (e.g. KEY_BRIGHTNESSUP).
Make a list of the key names and the codes that appeared in the system log (not
input-event-codes.h), and submit that list.

You can also use this driver without built-in support for a key by writing devd(8) 
instructions to run an external program to do something when the key is pressed.

# Installation

This driver can be installed directly via:

	make; make install

or, if you need evdev support:

	make CONF_CFLAGS=-DEVDEV_SUPPORT; make install

You may also install it as a port by unpacking apci_hidd.shar, changing into the
resulting *acpi_hidd* directory, and building it in the usual fashion; this procedure,
however, allowing you to set an option to provide evdev(4) support:

	sh acpi_hidd.shar
	cd acpi_hidd
	make makesum
	make install

You will want evdev(4) support unless you are running a kernel from which you
have removed this support.

Finally, add 'acpi_hidd_load="YES"' to /boot/loader.conf, and load the driver via
'kldload acpi_hidd' (or a reboot).

# Additional Items

For more information related to FreeBSD on the GPD MicroPC, and in particular for
more information regarding rotating FreeBSD's console and boot loader screens
between landmark and portrait mode, please see the FreeBSD GPD MicroPC page at
https://wiki.freebsd.org/Laptops/GPD_MicroPC
