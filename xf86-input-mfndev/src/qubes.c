/*
 * Copyright 2007 Peter Hutterer
 * Copyright 2009 Przemysław Firszt
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * The Qubes OS Project, http://www.qubes-os.org
 *
 * Copyright (C) 2010  Rafal Wojtczuk  <rafal@invisiblethingslab.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <linux/input.h>
#include <linux/types.h>

#include <xf86_OSproc.h>

#include <unistd.h>

#include <xf86.h>
#include <xf86Xinput.h>
#include <exevents.h>
#include <xorgVersion.h>
#include <xkbsrv.h>


#include <windowstr.h>

#ifdef HAVE_PROPERTIES
#include <xserver-properties.h>
/* 1.6 has properties, but no labels */
#ifdef AXIS_LABEL_PROP
#define HAVE_LABELS
#else
#undef HAVE_LABELS
#endif

#endif


#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <xorg-server.h>
#include <xorgVersion.h>
#include <xf86Module.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "qubes.h"
#include "shm_cmd.h"

static InputInfoPtr QubesPreInit(InputDriverPtr drv, IDevPtr dev,
				  int flags);
static void QubesUnInit(InputDriverPtr drv, InputInfoPtr pInfo,
			 int flags);
static pointer QubesPlug(pointer module, pointer options, int *errmaj,
			  int *errmin);
static void QubesUnplug(pointer p);
static void QubesReadInput(InputInfoPtr pInfo);
static int QubesControl(DeviceIntPtr device, int what);
static int _qubes_init_buttons(DeviceIntPtr device);
static int _qubes_init_axes(DeviceIntPtr device);



_X_EXPORT InputDriverRec QUBES = {
	1,
	"qubes",
	NULL,
	QubesPreInit,
	QubesUnInit,
	NULL,
	0
};

static XF86ModuleVersionInfo QubesVersionRec = {
	"qubes",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR,
	PACKAGE_VERSION_PATCHLEVEL,
	ABI_CLASS_XINPUT,
	ABI_XINPUT_VERSION,
	MOD_CLASS_XINPUT,
	{0, 0, 0, 0}
};

_X_EXPORT XF86ModuleData qubesModuleData = {
	&QubesVersionRec,
	&QubesPlug,
	&QubesUnplug
};

static void QubesUnplug(pointer p)
{
};

static pointer
QubesPlug(pointer module, pointer options, int *errmaj, int *errmin)
{
	xf86AddInputDriver(&QUBES, module, 0);
	return module;
};

static InputInfoPtr QubesPreInit(InputDriverPtr drv,
				  IDevPtr dev, int flags)
{
	InputInfoPtr pInfo;
	QubesDevicePtr pQubes;


	if (!(pInfo = xf86AllocateInput(drv, 0)))
		return NULL;

	pQubes = xcalloc(1, sizeof(QubesDeviceRec));
	if (!pQubes) {
		pInfo->private = NULL;
		xf86DeleteInput(pInfo, 0);
		return NULL;
	}

	pInfo->private = pQubes;
	pInfo->name = xstrdup(dev->identifier);
	pInfo->flags = 0;
	pInfo->type_name = XI_MOUSE;	/* see XI.h */
	pInfo->conf_idev = dev;
	pInfo->read_input = QubesReadInput;	/* new data avl */
	pInfo->switch_mode = NULL;	/* toggle absolute/relative mode */
	pInfo->device_control = QubesControl;	/* enable/disable dev */
	/* process driver specific options */
	pQubes->device = xf86SetStrOption(dev->commonOptions,
					   "Device", "/var/run/xf86-qubes-socket");

	xf86Msg(X_INFO, "%s: Using device %s.\n", pInfo->name,
		pQubes->device);
//	xf86Msg(X_INFO, "%s: dixLookupWindow=%p.\n", pInfo->name,
//		dixLookupWindow);
//	xf86Msg(X_INFO, "%s: dixLookupResourceByClass=%p.\n", pInfo->name,
//		dixLookupResourceByClass);

	/* process generic options */
	xf86CollectInputOptions(pInfo, NULL, NULL);
	xf86ProcessCommonOptions(pInfo, pInfo->options);
	/* Open sockets, init device files, etc. */
#if 0
	SYSCALL(pInfo->fd = open(pQubes->device, O_RDWR | O_NONBLOCK));
	if (pInfo->fd == -1) {
		xf86Msg(X_ERROR, "%s: failed to open %s.",
			pInfo->name, pQubes->device);
		pInfo->private = NULL;
		xfree(pQubes);
		xf86DeleteInput(pInfo, 0);
		return NULL;
	}
	/* do more funky stuff */
	close(pInfo->fd);
#endif
	pInfo->fd = -1;
	pInfo->flags |= XI86_OPEN_ON_INIT;
	pInfo->flags |= XI86_CONFIGURED;
	return pInfo;
}

static void QubesUnInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
	QubesDevicePtr pQubes = pInfo->private;
	if (pQubes->device) {
		xfree(pQubes->device);
		pQubes->device = NULL;
		/* Common error - pInfo->private must be NULL or valid memoy before
		 * passing into xf86DeleteInput */
		pInfo->private = NULL;
	}
	xf86DeleteInput(pInfo, 0);
}

static int _qubes_init_kbd(DeviceIntPtr device)
{
         InitKeyboardDeviceStruct(device, NULL, NULL, NULL);
}
                                    
static int _qubes_init_buttons(DeviceIntPtr device)
{
	InputInfoPtr pInfo = device->public.devicePrivate;
	QubesDevicePtr pQubes = pInfo->private;
	CARD8 *map;
	int i;
	int ret = Success;
	const int num_buttons = 6;

	map = xcalloc(num_buttons, sizeof(CARD8));

	xf86Msg(X_INFO, "%s: num_buttons=%d\n", pInfo->name, num_buttons);
	
	for (i = 0; i < num_buttons; i++)
		map[i] = i;

	pQubes->labels = xalloc(sizeof(Atom));

	if (!InitButtonClassDeviceStruct
	    (device, num_buttons, pQubes->labels, map)) {
		xf86Msg(X_ERROR, "%s: Failed to register buttons.\n",
			pInfo->name);
		ret = BadAlloc;
	}

	xfree(map);
	return ret;
}

static void QubesInitAxesLabels(QubesDevicePtr pQubes, int natoms,
				 Atom * atoms)
{
#ifdef HAVE_LABELS
	Atom atom;
	int axis;
	char **labels;
	int labels_len = 0;
	char *misc_label;

	labels = rel_labels;
	labels_len = ArrayLength(rel_labels);
	misc_label = AXIS_LABEL_PROP_REL_MISC;

	memset(atoms, 0, natoms * sizeof(Atom));

	/* Now fill the ones we know */
	for (axis = 0; axis < labels_len; axis++) {
		if (pQubes->axis_map[axis] == -1)
			continue;

		atom = XIGetKnownProperty(labels[axis]);
		if (!atom)	/* Should not happen */
			continue;

		atoms[pQubes->axis_map[axis]] = atom;
	}
#endif
}


static int _qubes_init_axes(DeviceIntPtr device)
{
	InputInfoPtr pInfo = device->public.devicePrivate;
	QubesDevicePtr pQubes = pInfo->private;
	int i;
	const int num_axes = 2;
	Atom *atoms;

	pQubes->num_vals = num_axes;
	atoms = xalloc(pQubes->num_vals * sizeof(Atom));

	QubesInitAxesLabels(pQubes, pQubes->num_vals, atoms);
	if (!InitValuatorClassDeviceStruct(device, num_axes,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					   atoms,
#endif
					   GetMotionHistorySize(), 0))
		return BadAlloc;

	pInfo->dev->valuator->mode = Relative;
	if (!InitAbsoluteClassDeviceStruct(device))
		return BadAlloc;

	for (i = 0; i < pQubes->axes; i++) {
		xf86InitValuatorAxisStruct(device, i, *pQubes->labels, -1,
					   -1, 1, 1, 1);
		xf86InitValuatorDefaults(device, i);
	}
	xfree(atoms);
	return Success;
}
int connect_unix_socket(QubesDevicePtr pQubes);
int connect_unix_socket(QubesDevicePtr pQubes)
{
	int s, len;
	struct sockaddr_un remote;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return -1;
	}


	remote.sun_family = AF_UNIX;
	strncpy(remote.sun_path, pQubes->device, sizeof(remote.sun_path));
	len = strlen(remote.sun_path) + sizeof(remote.sun_family);
	if (connect(s, (struct sockaddr *) &remote, len) == -1) {
		perror("connect");
		close(s);
		return -1;
	}
	return s;
}


static int QubesControl(DeviceIntPtr device, int what)
{
	InputInfoPtr pInfo = device->public.devicePrivate;
	QubesDevicePtr pQubes = pInfo->private;

	switch (what) {
	case DEVICE_INIT:
		_qubes_init_buttons(device);
		_qubes_init_axes(device);
		_qubes_init_kbd(device);
		break;

		/* Switch device on.  Establish socket, start event delivery.  */
	case DEVICE_ON:
		xf86Msg(X_INFO, "%s: On.\n", pInfo->name);
		if (device->public.on)
			break;
		do {
			pInfo->fd = connect_unix_socket(pQubes);
			if (pInfo->fd < 0) {
				xf86Msg(X_ERROR,
					"%s: cannot open device; sleeping...\n",
					pInfo->name);
				sleep(1);
			}
		} while (pInfo->fd < 0);

		xf86FlushInput(pInfo->fd);
		xf86AddEnabledDevice(pInfo);
		device->public.on = TRUE;
		break;
	case DEVICE_OFF:
		xf86Msg(X_INFO, "%s: Off.\n", pInfo->name);
		if (!device->public.on)
			break;
		xf86RemoveEnabledDevice(pInfo);
		close(pInfo->fd);
		pInfo->fd = -1;
		device->public.on = FALSE;
		break;
	case DEVICE_CLOSE:
		/* free what we have to free */
		break;
	}
	return Success;
}

static void dump_window_mfns(WindowPtr pWin, int id, int fd);

static void dump_window_mfns(WindowPtr pWin, int id, int fd)
{
	ScreenPtr screen;
	PixmapPtr pixmap;
	int i, off, num_mfn, mfn;
	struct shm_cmd shmcmd;
	char *pixels, *pixels_end;
	if (!pWin)
		return;

	screen = pWin->drawable.pScreen;
	pixmap = (*screen->GetWindowPixmap) (pWin);

	pixels = pixmap->devPrivate.ptr;
	pixels_end =
	    pixels +
	    pixmap->drawable.width * pixmap->drawable.height *
	    pixmap->drawable.bitsPerPixel / 8;
	off = ((long) pixels) & (4096 - 1);
	pixels -= off;
	num_mfn = ((long) pixels_end - (long) pixels + 4095) >> 12;
	shmcmd.width = pixmap->drawable.width;
	shmcmd.height = pixmap->drawable.height;
	shmcmd.bpp = pixmap->drawable.bitsPerPixel;
	shmcmd.off = off;
	if (!pixmap->devPrivate.ptr)
		num_mfn = 0;
	shmcmd.num_mfn = num_mfn;
	shmcmd.domid = 0x12345678;

	write(fd, &shmcmd, sizeof(shmcmd));
	mlock(pixels, 4096 * num_mfn);
	for (i = 0; i < num_mfn; i++) {
		u2mfn_get_mfn_for_page ((long)(pixels + 4096 * i), &mfn);
		write(fd, &mfn, 4);
	}
}


static WindowPtr id2winptr(unsigned int xid)
{
	int ret;
	WindowPtr result = NULL;
	ret = dixLookupResourceByClass((void**)&result, xid, RC_DRAWABLE, 0, 0);
	if (ret == Success)
		return result;
	else
		return NULL;
}

static void process_request(int fd, InputInfoPtr pInfo)
{
	WindowPtr w1;
	unsigned int arg1, arg2;
	char cmd;
	int ret;
	char buf[100];


	ret = read(fd, buf, sizeof(buf));
	if (ret == 0) {
		xf86Msg(X_INFO, "randdev: unix closed\n");
		close(fd);
		return;
	}
	if (ret == -1) {
		xf86Msg(X_INFO, "randdev: unix error\n");
		close(fd);
		return;
	}
	buf[ret] = 0;

//	xf86Msg(X_INFO, "randdev: received %s\n", buf);

	ret = sscanf(buf, "%c 0x%x 0x%x\n", &cmd, &arg1, &arg2);
	if (ret != 3) {
		xf86Msg(X_INFO, "randdev: ret=%d, cannot parse %s\n", ret,
			buf);
		return;
	}
	write(fd, "0", 1);
	switch (cmd) {
	case 'W':
	w1 = id2winptr(arg1);
	if (!w1) {
		struct shm_cmd shmcmd;
//		xf86Msg(X_INFO, "randdev: w1=%p, xid1: 0x%x\n", w1, arg1);
		shmcmd.num_mfn = 0;
		write(fd, &shmcmd, sizeof(shmcmd));
		return;
	}
//        dump_window_to_bmp(w1);
	dump_window_mfns(w1, arg1, fd);
	break;
	case 'B':
	    xf86PostButtonEvent(pInfo->dev, 0, arg1, arg2, 0,0);
	    break;
        case 'M':
            xf86PostMotionEvent(pInfo->dev,1, 0, 2, arg1, arg2);
            break;
        case 'K':
            /* TBD, need to figure out how to use XkbInitKeyboardDeviceStruct */
            xf86PostKeyboardEvent(pInfo->dev,arg1, arg2);
            break;
        default:
            xf86Msg(X_INFO, "randdev: unknown command %c\n", cmd);
        }
}

static void QubesReadInput(InputInfoPtr pInfo)
{
	while (xf86WaitForInput(pInfo->fd, 0) > 0) {
		process_request(pInfo->fd, pInfo);
#if 0
		xf86PostMotionEvent(pInfo->dev, 0,	/* is_absolute */
				    0,	/* first_valuator */
				    1,	/* num_valuators */
				    data);
#endif
	}
}