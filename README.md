# acpi_hidd

ACPI human interface device subsystem for FreeBSD

# Description

The acpi_hidd driver provides the human interface device features of the ACPI
module, in particular brightness and media control keyboard support.

The driver has sysctl(8) and devd(8) interfaces; the sysctl(8) interface
allows inspection and control of the screen brightness, and the devd(8)
interface provides notifications about keypresses that do not have support for
their functionality built into the driver (such as the media keys).

# Notes

The collection of key codes is very small at this time, comprising only
the two that are present on the GPD MicroPC.  This does not prevent use
of the driver with devd(8), but contributions from users will make the
driver more useful.  Inside the driver you'll find a list of key codes
and the keys they represent.  Add your computer's codes to this list and
leave your additions, the make, and model of your computer as an
issue here on GitHub.

The current key code list follows:

	// HIDD keycodes for GPD MicroPC
	#define ACPI_NOTIFY_HIDD_BRIGHTNESS_UP      0x13
	#define ACPI_NOTIFY_HIDD_BRIGHTNESS_DOWN    0x14
