/*-
 * Copyright (c) 1998 - 2008 S�ren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ata.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>
#include <ata_if.h>

void
ata_sata_phy_check_events(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t error = ATA_IDX_INL(ch, ATA_SERROR);

    /* clear error bits/interrupt */
    ATA_IDX_OUTL(ch, ATA_SERROR, error);

    /* if we have a connection event deal with it */
    if ((error & ATA_SE_PHY_CHANGED) && (ch->pm_level == 0)) {
	if (bootverbose) {
	    u_int32_t status = ATA_IDX_INL(ch, ATA_SSTATUS);
	    if (((status & ATA_SS_CONWELL_MASK) == ATA_SS_CONWELL_GEN1) ||
		((status & ATA_SS_CONWELL_MASK) == ATA_SS_CONWELL_GEN2)) {
		    device_printf(dev, "CONNECT requested\n");
	    } else
		    device_printf(dev, "DISCONNECT requested\n");
	}
	taskqueue_enqueue(taskqueue_thread, &ch->conntask);
    }
}

int
ata_sata_scr_read(struct ata_channel *ch, int port, int reg, uint32_t *val)
{
    int r;

    if (port < 0) {
	*val = ATA_IDX_INL(ch, reg);
	return (0);
    } else {
	switch (reg) {
	    case ATA_SSTATUS:
		r = 0;
		break;
	    case ATA_SERROR:
		r = 1;
		break;
	    case ATA_SCONTROL:
		r = 2;
		break;
	    default:
		return (EINVAL);
	}
	return (ch->hw.pm_read(ch->dev, port, r, val));
    }
}

int
ata_sata_scr_write(struct ata_channel *ch, int port, int reg, uint32_t val)
{
    int r;

    if (port < 0) {
	ATA_IDX_OUTL(ch, reg, val);
	return (0);
    } else {
	switch (reg) {
	    case ATA_SERROR:
		r = 1;
		break;
	    case ATA_SCONTROL:
		r = 2;
		break;
	    default:
		return (EINVAL);
	}
	return (ch->hw.pm_write(ch->dev, port, r, val));
    }
}

static int
ata_sata_connect(struct ata_channel *ch, int port)
{
    u_int32_t status;
    int timeout;

    /* wait up to 1 second for "connect well" */
    for (timeout = 0; timeout < 100 ; timeout++) {
	if (ata_sata_scr_read(ch, port, ATA_SSTATUS, &status))
	    return (0);
	if ((status & ATA_SS_CONWELL_MASK) == ATA_SS_CONWELL_GEN1 ||
	    (status & ATA_SS_CONWELL_MASK) == ATA_SS_CONWELL_GEN2)
	    break;
	ata_udelay(10000);
    }
    if (timeout >= 100) {
	if (bootverbose) {
	    if (port < 0) {
		device_printf(ch->dev, "SATA connect timeout status=%08x\n",
		    status);
	    } else {
		device_printf(ch->dev, "p%d: SATA connect timeout status=%08x\n",
		    port, status);
	    }
	}
	return 0;
    }
    if (bootverbose) {
	if (port < 0) {
	    device_printf(ch->dev, "SATA connect time=%dms status=%08x\n",
		timeout * 10, status);
	} else {
	    device_printf(ch->dev, "p%d: SATA connect time=%dms status=%08x\n",
		port, timeout * 10, status);
	}
    }

    /* clear SATA error register */
    ata_sata_scr_write(ch, port, ATA_SERROR, 0xffffffff);

    return 1;
}

int
ata_sata_phy_reset(device_t dev, int port, int quick)
{
    struct ata_channel *ch = device_get_softc(dev);
    int loop, retry;
    uint32_t val;

    if (quick) {
	if (ata_sata_scr_read(ch, port, ATA_SCONTROL, &val))
	    return (0);
	if ((val & ATA_SC_DET_MASK) == ATA_SC_DET_IDLE)
	    return ata_sata_connect(ch, port);
    }

    if (bootverbose) {
	if (port < 0) {
	    device_printf(dev, "hardware reset ...\n");
	} else {
	    device_printf(dev, "p%d: hardware reset ...\n", port);
	}
    }
    for (retry = 0; retry < 10; retry++) {
	for (loop = 0; loop < 10; loop++) {
	    if (ata_sata_scr_write(ch, port, ATA_SCONTROL, ATA_SC_DET_RESET))
		return (0);
	    ata_udelay(100);
	    if (ata_sata_scr_read(ch, port, ATA_SCONTROL, &val))
		return (0);
	    if ((val & ATA_SC_DET_MASK) == ATA_SC_DET_RESET)
		break;
	}
	ata_udelay(5000);
	for (loop = 0; loop < 10; loop++) {
	    if (ata_sata_scr_write(ch, port, ATA_SCONTROL,
		    ATA_SC_DET_IDLE | ((ch->pm_level > 0) ? 0 :
		    ATA_SC_IPM_DIS_PARTIAL | ATA_SC_IPM_DIS_SLUMBER)))
		return (0);
	    ata_udelay(100);
	    if (ata_sata_scr_read(ch, port, ATA_SCONTROL, &val))
		return (0);
	    if ((val & ATA_SC_DET_MASK) == 0)
		return ata_sata_connect(ch, port);
	}
    }
    return 0;
}

int
ata_sata_setmode(device_t dev, int target, int mode)
{

	return (min(mode, ATA_UDMA5));
}

int
ata_sata_getrev(device_t dev, int target)
{
	struct ata_channel *ch = device_get_softc(dev);

	if (ch->r_io[ATA_SSTATUS].res)
		return ((ATA_IDX_INL(ch, ATA_SSTATUS) & 0x0f0) >> 4);
	return (0xff);
}

int
ata_request2fis_h2d(struct ata_request *request, u_int8_t *fis)
{

    if (request->flags & ATA_R_ATAPI) {
	fis[0] = 0x27;  		/* host to device */
	fis[1] = 0x80 | (request->unit & 0x0f);
	fis[2] = ATA_PACKET_CMD;
	if (request->flags & (ATA_R_READ | ATA_R_WRITE))
	    fis[3] = ATA_F_DMA;
	else {
	    fis[5] = request->transfersize;
	    fis[6] = request->transfersize >> 8;
	}
	fis[7] = ATA_D_LBA;
	fis[15] = ATA_A_4BIT;
	return 20;
    }
    else {
	fis[0] = 0x27;			/* host to device */
	fis[1] = 0x80 | (request->unit & 0x0f);
	fis[2] = request->u.ata.command;
	fis[3] = request->u.ata.feature;
	fis[4] = request->u.ata.lba;
	fis[5] = request->u.ata.lba >> 8;
	fis[6] = request->u.ata.lba >> 16;
	fis[7] = ATA_D_LBA;
	if (!(request->flags & ATA_R_48BIT))
	    fis[7] |= (ATA_D_IBM | (request->u.ata.lba >> 24 & 0x0f));
	fis[8] = request->u.ata.lba >> 24;
	fis[9] = request->u.ata.lba >> 32; 
	fis[10] = request->u.ata.lba >> 40; 
	fis[11] = request->u.ata.feature >> 8;
	fis[12] = request->u.ata.count;
	fis[13] = request->u.ata.count >> 8;
	fis[15] = ATA_A_4BIT;
	return 20;
    }
    return 0;
}

void
ata_pm_identify(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    u_int32_t pm_chipid, pm_revision, pm_ports;
    int port;

    /* get PM vendor & product data */
    if (ch->hw.pm_read(dev, ATA_PM, 0, &pm_chipid)) {
	device_printf(dev, "error getting PM vendor data\n");
	return;
    }

    /* get PM revision data */
    if (ch->hw.pm_read(dev, ATA_PM, 1, &pm_revision)) {
	device_printf(dev, "error getting PM revison data\n");
	return;
    }

    /* get number of HW ports on the PM */
    if (ch->hw.pm_read(dev, ATA_PM, 2, &pm_ports)) {
	device_printf(dev, "error getting PM port info\n");
	return;
    }
    pm_ports &= 0x0000000f;

    /* chip specific quirks */
    switch (pm_chipid) {
    case 0x37261095:
	/* This PM declares 6 ports, while only 5 of them are real.
	 * Port 5 is enclosure management bridge port, which has implementation
	 * problems, causing probe faults. Hide it for now. */
	device_printf(dev, "SiI 3726 (rev=%x) Port Multiplier with %d (5) ports\n",
		      pm_revision, pm_ports);
	pm_ports = 5;
	break;

    case 0x47261095:
	/* This PM declares 7 ports, while only 5 of them are real.
	 * Port 5 is some fake "Config  Disk" with 640 sectors size,
	 * port 6 is enclosure management bridge port.
	 * Both fake ports has implementation problems, causing
	 * probe faults. Hide them for now. */
	device_printf(dev, "SiI 4726 (rev=%x) Port Multiplier with %d (5) ports\n",
		      pm_revision, pm_ports);
	pm_ports = 5;
	break;

    default:
	device_printf(dev, "Port Multiplier (id=%08x rev=%x) with %d ports\n",
		      pm_chipid, pm_revision, pm_ports);
    }

    /* reset all ports and register if anything connected */
    for (port=0; port < pm_ports; port++) {
	u_int32_t signature;

	if (!ata_sata_phy_reset(dev, port, 1))
	    continue;

	/*
	 * XXX: I have no idea how to properly wait for PMP port hardreset
	 * completion. Without this delay soft reset does not completes
	 * successfully.
	 */
	DELAY(1000000);

	signature = ch->hw.softreset(dev, port);

	if (bootverbose)
	    device_printf(dev, "p%d: SIGNATURE=%08x\n", port, signature);

	/* figure out whats there */
	switch (signature >> 16) {
	case 0x0000:
	    ch->devices |= (ATA_ATA_MASTER << port);
	    continue;
	case 0xeb14:
	    ch->devices |= (ATA_ATAPI_MASTER << port);
	    continue;
	}
    }
}
