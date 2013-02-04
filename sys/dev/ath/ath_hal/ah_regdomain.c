/*
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
 * Copyright (c) 2005-2006 Atheros Communications, Inc.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */
#include "opt_ah.h"

#include "ah.h"

#include <net80211/_ieee80211.h>
#include <net80211/ieee80211_regdomain.h>

#include "ah_internal.h"
#include "ah_eeprom.h"
#include "ah_devid.h"

#include "ah_regdomain.h"

/*
 * XXX this code needs a audit+review
 */

/* used throughout this file... */
#define	N(a)	(sizeof (a) / sizeof (a[0]))

#define HAL_MODE_11A_TURBO	HAL_MODE_108A
#define HAL_MODE_11G_TURBO	HAL_MODE_108G

/*
 * Mask to check whether a domain is a multidomain or a single domain
 */
#define MULTI_DOMAIN_MASK 0xFF00

/*
 * Enumerated Regulatory Domain Information 8 bit values indicate that
 * the regdomain is really a pair of unitary regdomains.  12 bit values
 * are the real unitary regdomains and are the only ones which have the
 * frequency bitmasks and flags set.
 */
#include "ah_regdomain/ah_rd_regenum.h"

#define	WORLD_SKU_MASK		0x00F0
#define	WORLD_SKU_PREFIX	0x0060

/*
 * THE following table is the mapping of regdomain pairs specified by
 * an 8 bit regdomain value to the individual unitary reg domains
 */
#include "ah_regdomain/ah_rd_regmap.h"

/* 
 * The following tables are the master list for all different freqeuncy
 * bands with the complete matrix of all possible flags and settings
 * for each band if it is used in ANY reg domain.
 */

#define	COUNTRY_ERD_FLAG        0x8000
#define WORLDWIDE_ROAMING_FLAG  0x4000

/*
 * This table maps country ISO codes from net80211 into regulatory
 * domains which the ath regulatory domain code understands.
 */
#include "ah_regdomain/ah_rd_ctry.h"

/*
 * The frequency band collections are a set of frequency ranges
 * with shared properties - max tx power, max antenna gain, channel width,
 * channel spacing, DFS requirements and passive scanning requirements.
 *
 * These are represented as entries in a frequency band bitmask.
 * Each regulatory domain entry in ah_regdomain_domains.h uses one
 * or more frequency band entries for each of the channel modes
 * supported (11bg, 11a, half, quarter, turbo, etc.)
 *
 */
#include "ah_regdomain/ah_rd_freqbands.h"

/*
 * This is the main regulatory database. It defines the supported
 * set of features and requirements for each of the defined regulatory
 * zones. It uses combinations of frequency ranges - represented in
 * a bitmask - to determine the requirements and limitations needed.
 */
#include "ah_regdomain/ah_rd_domains.h"

static const struct cmode modes[] = {
	{ HAL_MODE_TURBO,	IEEE80211_CHAN_ST },
	{ HAL_MODE_11A,		IEEE80211_CHAN_A },
	{ HAL_MODE_11B,		IEEE80211_CHAN_B },
	{ HAL_MODE_11G,		IEEE80211_CHAN_G },
	{ HAL_MODE_11G_TURBO,	IEEE80211_CHAN_108G },
	{ HAL_MODE_11A_TURBO,	IEEE80211_CHAN_108A },
	{ HAL_MODE_11A_QUARTER_RATE,
	  IEEE80211_CHAN_A | IEEE80211_CHAN_QUARTER },
	{ HAL_MODE_11A_HALF_RATE,
	  IEEE80211_CHAN_A | IEEE80211_CHAN_HALF },
	{ HAL_MODE_11G_QUARTER_RATE,
	  IEEE80211_CHAN_G | IEEE80211_CHAN_QUARTER },
	{ HAL_MODE_11G_HALF_RATE,
	  IEEE80211_CHAN_G | IEEE80211_CHAN_HALF },
	{ HAL_MODE_11NG_HT20,	IEEE80211_CHAN_G | IEEE80211_CHAN_HT20 },
	{ HAL_MODE_11NG_HT40PLUS,
	  IEEE80211_CHAN_G | IEEE80211_CHAN_HT40U },
	{ HAL_MODE_11NG_HT40MINUS,
	  IEEE80211_CHAN_G | IEEE80211_CHAN_HT40D },
	{ HAL_MODE_11NA_HT20,	IEEE80211_CHAN_A | IEEE80211_CHAN_HT20 },
	{ HAL_MODE_11NA_HT40PLUS,
	  IEEE80211_CHAN_A | IEEE80211_CHAN_HT40U },
	{ HAL_MODE_11NA_HT40MINUS,
	  IEEE80211_CHAN_A | IEEE80211_CHAN_HT40D },
};

static void ath_hal_update_dfsdomain(struct ath_hal *ah);

static OS_INLINE uint16_t
getEepromRD(struct ath_hal *ah)
{
	return AH_PRIVATE(ah)->ah_currentRD &~ WORLDWIDE_ROAMING_FLAG;
}

/*
 * Test to see if the bitmask array is all zeros
 */
static HAL_BOOL
isChanBitMaskZero(const uint64_t *bitmask)
{
#if BMLEN > 2
#error	"add more cases"
#endif
#if BMLEN > 1
	if (bitmask[1] != 0)
		return AH_FALSE;
#endif
	return (bitmask[0] == 0);
}

/*
 * Return whether or not the regulatory domain/country in EEPROM
 * is acceptable.
 */
static HAL_BOOL
isEepromValid(struct ath_hal *ah)
{
	uint16_t rd = getEepromRD(ah);
	int i;

	if (rd & COUNTRY_ERD_FLAG) {
		uint16_t cc = rd &~ COUNTRY_ERD_FLAG;
		for (i = 0; i < N(allCountries); i++)
			if (allCountries[i].countryCode == cc)
				return AH_TRUE;
	} else {
		for (i = 0; i < N(regDomainPairs); i++)
			if (regDomainPairs[i].regDmnEnum == rd)
				return AH_TRUE;
	}
	HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
	    "%s: invalid regulatory domain/country code 0x%x\n", __func__, rd);
	return AH_FALSE;
}

/*
 * Find the pointer to the country element in the country table
 * corresponding to the country code
 */
static COUNTRY_CODE_TO_ENUM_RD*
findCountry(HAL_CTRY_CODE countryCode)
{
	int i;

	for (i = 0; i < N(allCountries); i++) {
		if (allCountries[i].countryCode == countryCode)
			return &allCountries[i];
	}
	return AH_NULL;
}

static REG_DOMAIN *
findRegDmn(int regDmn)
{
	int i;

	for (i = 0; i < N(regDomains); i++) {
		if (regDomains[i].regDmnEnum == regDmn)
			return &regDomains[i];
	}
	return AH_NULL;
}

static REG_DMN_PAIR_MAPPING *
findRegDmnPair(int regDmnPair)
{
	int i;

	if (regDmnPair != NO_ENUMRD) {
		for (i = 0; i < N(regDomainPairs); i++) {
			if (regDomainPairs[i].regDmnEnum == regDmnPair)
				return &regDomainPairs[i];
		}
	}
	return AH_NULL;
}

/*
 * Calculate a default country based on the EEPROM setting.
 */
static HAL_CTRY_CODE
getDefaultCountry(struct ath_hal *ah)
{
	REG_DMN_PAIR_MAPPING *regpair;
	uint16_t rd;

	rd = getEepromRD(ah);
	if (rd & COUNTRY_ERD_FLAG) {
		COUNTRY_CODE_TO_ENUM_RD *country;
		uint16_t cc = rd & ~COUNTRY_ERD_FLAG;
		country = findCountry(cc);
		if (country != AH_NULL)
			return cc;
	}
	/*
	 * Check reg domains that have only one country
	 */
	regpair = findRegDmnPair(rd);
	return (regpair != AH_NULL) ? regpair->singleCC : CTRY_DEFAULT;
}

static HAL_BOOL
IS_BIT_SET(int bit, const uint64_t bitmask[])
{
	int byteOffset, bitnum;
	uint64_t val;

	byteOffset = bit/64;
	bitnum = bit - byteOffset*64;
	val = ((uint64_t) 1) << bitnum;
	return (bitmask[byteOffset] & val) != 0;
}

static HAL_STATUS
getregstate(struct ath_hal *ah, HAL_CTRY_CODE cc, HAL_REG_DOMAIN regDmn,
    COUNTRY_CODE_TO_ENUM_RD **pcountry,
    REG_DOMAIN **prd2GHz, REG_DOMAIN **prd5GHz)
{
	COUNTRY_CODE_TO_ENUM_RD *country;
	REG_DOMAIN *rd5GHz, *rd2GHz;

	if (cc == CTRY_DEFAULT && regDmn == SKU_NONE) {
		/*
		 * Validate the EEPROM setting and setup defaults
		 */
		if (!isEepromValid(ah)) {
			/*
			 * Don't return any channels if the EEPROM has an
			 * invalid regulatory domain/country code setting.
			 */
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: invalid EEPROM contents\n",__func__);
			return HAL_EEBADREG;
		}

		cc = getDefaultCountry(ah);
		country = findCountry(cc);
		if (country == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "NULL Country!, cc %d\n", cc);
			return HAL_EEBADCC;
		}
		regDmn = country->regDmnEnum;
		HALDEBUG(ah, HAL_DEBUG_REGDOMAIN, "%s: EEPROM cc %u rd 0x%x\n",
		    __func__, cc, regDmn);

		if (country->countryCode == CTRY_DEFAULT) {
			/*
			 * Check EEPROM; SKU may be for a country, single
			 * domain, or multiple domains (WWR).
			 */
			uint16_t rdnum = getEepromRD(ah);
			if ((rdnum & COUNTRY_ERD_FLAG) == 0 &&
			    (findRegDmn(rdnum) != AH_NULL ||
			     findRegDmnPair(rdnum) != AH_NULL)) {
				regDmn = rdnum;
				HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
				    "%s: EEPROM rd 0x%x\n", __func__, rdnum);
			}
		}
	} else {
		country = findCountry(cc);
		if (country == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "unknown country, cc %d\n", cc);
			return HAL_EINVAL;
		}
		if (regDmn == SKU_NONE)
			regDmn = country->regDmnEnum;
		HALDEBUG(ah, HAL_DEBUG_REGDOMAIN, "%s: cc %u rd 0x%x\n",
		    __func__, cc, regDmn);
	}

	/*
	 * Setup per-band state.
	 */
	if ((regDmn & MULTI_DOMAIN_MASK) == 0) {
		REG_DMN_PAIR_MAPPING *regpair = findRegDmnPair(regDmn);
		if (regpair == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: no reg domain pair %u for country %u\n",
			    __func__, regDmn, country->countryCode);
			return HAL_EINVAL;
		}
		rd5GHz = findRegDmn(regpair->regDmn5GHz);
		if (rd5GHz == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: no 5GHz reg domain %u for country %u\n",
			    __func__, regpair->regDmn5GHz, country->countryCode);
			return HAL_EINVAL;
		}
		rd2GHz = findRegDmn(regpair->regDmn2GHz);
		if (rd2GHz == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: no 2GHz reg domain %u for country %u\n",
			    __func__, regpair->regDmn2GHz, country->countryCode);
			return HAL_EINVAL;
		}
	} else {
		rd5GHz = rd2GHz = findRegDmn(regDmn);
		if (rd2GHz == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: no unitary reg domain %u for country %u\n",
			    __func__, regDmn, country->countryCode);
			return HAL_EINVAL;
		}
	}
	if (pcountry != AH_NULL)
		*pcountry = country;
	*prd2GHz = rd2GHz;
	*prd5GHz = rd5GHz;
	return HAL_OK;
}

/*
 * Construct the channel list for the specified regulatory config.
 */
static HAL_STATUS
getchannels(struct ath_hal *ah,
    struct ieee80211_channel chans[], u_int maxchans, int *nchans,
    u_int modeSelect, HAL_CTRY_CODE cc, HAL_REG_DOMAIN regDmn,
    HAL_BOOL enableExtendedChannels,
    COUNTRY_CODE_TO_ENUM_RD **pcountry,
    REG_DOMAIN **prd2GHz, REG_DOMAIN **prd5GHz)
{
#define CHANNEL_HALF_BW		10
#define CHANNEL_QUARTER_BW	5
#define	HAL_MODE_11A_ALL \
	(HAL_MODE_11A | HAL_MODE_11A_TURBO | HAL_MODE_TURBO | \
	 HAL_MODE_11A_QUARTER_RATE | HAL_MODE_11A_HALF_RATE)
	REG_DOMAIN *rd5GHz, *rd2GHz;
	u_int modesAvail;
	const struct cmode *cm;
	struct ieee80211_channel *ic;
	int next, b;
	HAL_STATUS status;

	HALDEBUG(ah, HAL_DEBUG_REGDOMAIN, "%s: cc %u regDmn 0x%x mode 0x%x%s\n",
	    __func__, cc, regDmn, modeSelect, 
	    enableExtendedChannels ? " ecm" : "");

	status = getregstate(ah, cc, regDmn, pcountry, &rd2GHz, &rd5GHz);
	if (status != HAL_OK)
		return status;

	/* get modes that HW is capable of */
	modesAvail = ath_hal_getWirelessModes(ah);
	/* optimize work below if no 11a channels */
	if (isChanBitMaskZero(rd5GHz->chan11a) &&
	    (modesAvail & HAL_MODE_11A_ALL)) {
		HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
		    "%s: disallow all 11a\n", __func__);
		modesAvail &= ~HAL_MODE_11A_ALL;
	}

	next = 0;
	ic = &chans[0];
	for (cm = modes; cm < &modes[N(modes)]; cm++) {
		uint16_t c, c_hi, c_lo;
		uint64_t *channelBM = AH_NULL;
		REG_DMN_FREQ_BAND *fband = AH_NULL,*freqs;
		int low_adj, hi_adj, channelSep, lastc;
		uint32_t rdflags;
		uint64_t dfsMask;
		uint64_t pscan;

		if ((cm->mode & modeSelect) == 0) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: skip mode 0x%x flags 0x%x\n",
			    __func__, cm->mode, cm->flags);
			continue;
		}
		if ((cm->mode & modesAvail) == 0) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: !avail mode 0x%x (0x%x) flags 0x%x\n",
			    __func__, modesAvail, cm->mode, cm->flags);
			continue;
		}
		if (!ath_hal_getChannelEdges(ah, cm->flags, &c_lo, &c_hi)) {
			/* channel not supported by hardware, skip it */
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: channels 0x%x not supported by hardware\n",
			    __func__,cm->flags);
			continue;
		}
		switch (cm->mode) {
		case HAL_MODE_TURBO:
		case HAL_MODE_11A_TURBO:
			rdflags = rd5GHz->flags;
			dfsMask = rd5GHz->dfsMask;
			pscan = rd5GHz->pscan;
			if (cm->mode == HAL_MODE_TURBO)
				channelBM = rd5GHz->chan11a_turbo;
			else
				channelBM = rd5GHz->chan11a_dyn_turbo;
			freqs = &regDmn5GhzTurboFreq[0];
			break;
		case HAL_MODE_11G_TURBO:
			rdflags = rd2GHz->flags;
			dfsMask = rd2GHz->dfsMask;
			pscan = rd2GHz->pscan;
			channelBM = rd2GHz->chan11g_turbo;
			freqs = &regDmn2Ghz11gTurboFreq[0];
			break;
		case HAL_MODE_11A:
		case HAL_MODE_11A_HALF_RATE:
		case HAL_MODE_11A_QUARTER_RATE:
		case HAL_MODE_11NA_HT20:
		case HAL_MODE_11NA_HT40PLUS:
		case HAL_MODE_11NA_HT40MINUS:
			rdflags = rd5GHz->flags;
			dfsMask = rd5GHz->dfsMask;
			pscan = rd5GHz->pscan;
			if (cm->mode == HAL_MODE_11A_HALF_RATE)
				channelBM = rd5GHz->chan11a_half;
			else if (cm->mode == HAL_MODE_11A_QUARTER_RATE)
				channelBM = rd5GHz->chan11a_quarter;
			else
				channelBM = rd5GHz->chan11a;
			freqs = &regDmn5GhzFreq[0];
			break;
		case HAL_MODE_11B:
		case HAL_MODE_11G:
		case HAL_MODE_11G_HALF_RATE:
		case HAL_MODE_11G_QUARTER_RATE:
		case HAL_MODE_11NG_HT20:
		case HAL_MODE_11NG_HT40PLUS:
		case HAL_MODE_11NG_HT40MINUS:
			rdflags = rd2GHz->flags;
			dfsMask = rd2GHz->dfsMask;
			pscan = rd2GHz->pscan;
			if (cm->mode == HAL_MODE_11G_HALF_RATE)
				channelBM = rd2GHz->chan11g_half;
			else if (cm->mode == HAL_MODE_11G_QUARTER_RATE)
				channelBM = rd2GHz->chan11g_quarter;
			else if (cm->mode == HAL_MODE_11B)
				channelBM = rd2GHz->chan11b;
			else
				channelBM = rd2GHz->chan11g;
			if (cm->mode == HAL_MODE_11B)
				freqs = &regDmn2GhzFreq[0];
			else
				freqs = &regDmn2Ghz11gFreq[0];
			break;
		default:
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: Unkonwn HAL mode 0x%x\n", __func__, cm->mode);
			continue;
		}
		if (isChanBitMaskZero(channelBM))
			continue;
		/*
		 * Setup special handling for HT40 channels; e.g.
		 * 5G HT40 channels require 40Mhz channel separation.
		 */
		hi_adj = (cm->mode == HAL_MODE_11NA_HT40PLUS ||
		    cm->mode == HAL_MODE_11NG_HT40PLUS) ? -20 : 0;
		low_adj = (cm->mode == HAL_MODE_11NA_HT40MINUS || 
		    cm->mode == HAL_MODE_11NG_HT40MINUS) ? 20 : 0;
		channelSep = (cm->mode == HAL_MODE_11NA_HT40PLUS ||
		    cm->mode == HAL_MODE_11NA_HT40MINUS) ? 40 : 0;

		for (b = 0; b < 64*BMLEN; b++) {
			if (!IS_BIT_SET(b, channelBM))
				continue;
			fband = &freqs[b];
			lastc = 0;

			for (c = fband->lowChannel + low_adj;
			     c <= fband->highChannel + hi_adj;
			     c += fband->channelSep) {
				if (!(c_lo <= c && c <= c_hi)) {
					HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
					    "%s: c %u out of range [%u..%u]\n",
					    __func__, c, c_lo, c_hi);
					continue;
				}
				if (next >= maxchans){
					HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
					    "%s: too many channels for channel table\n",
					    __func__);
					goto done;
				}
				if ((fband->usePassScan & IS_ECM_CHAN) &&
				    !enableExtendedChannels) {
					HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
					    "skip ecm channel\n");
					continue;
				}
#if 0
				if ((fband->useDfs & dfsMask) && 
				    (cm->flags & IEEE80211_CHAN_HT40)) {
					/* NB: DFS and HT40 don't mix */
					HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
					    "skip HT40 chan, DFS required\n");
					continue;
				}
#endif
				/*
				 * Make sure that channel separation
				 * meets the requirement.
				 */
				if (lastc && channelSep &&
				    (c-lastc) < channelSep)
					continue;
				lastc = c;

				OS_MEMZERO(ic, sizeof(*ic));
				ic->ic_freq = c;
				ic->ic_flags = cm->flags;
				ic->ic_maxregpower = fband->powerDfs;
				ath_hal_getpowerlimits(ah, ic);
				ic->ic_maxantgain = fband->antennaMax;
				if (fband->usePassScan & pscan)
					ic->ic_flags |= IEEE80211_CHAN_PASSIVE;
				if (fband->useDfs & dfsMask)
					ic->ic_flags |= IEEE80211_CHAN_DFS;
				if (IEEE80211_IS_CHAN_5GHZ(ic) &&
				    (rdflags & DISALLOW_ADHOC_11A))
					ic->ic_flags |= IEEE80211_CHAN_NOADHOC;
				if (IEEE80211_IS_CHAN_TURBO(ic) &&
				    (rdflags & DISALLOW_ADHOC_11A_TURB))
					ic->ic_flags |= IEEE80211_CHAN_NOADHOC;
				if (rdflags & NO_HOSTAP)
					ic->ic_flags |= IEEE80211_CHAN_NOHOSTAP;
				if (rdflags & LIMIT_FRAME_4MS)
					ic->ic_flags |= IEEE80211_CHAN_4MSXMIT;
				if (rdflags & NEED_NFC)
					ic->ic_flags |= CHANNEL_NFCREQUIRED;

				ic++, next++;
			}
		}
	}
done:
	*nchans = next;
	/* NB: pcountry set above by getregstate */
	if (prd2GHz != AH_NULL)
		*prd2GHz = rd2GHz;
	if (prd5GHz != AH_NULL)
		*prd5GHz = rd5GHz;
	return HAL_OK;
#undef HAL_MODE_11A_ALL
#undef CHANNEL_HALF_BW
#undef CHANNEL_QUARTER_BW
}

/*
 * Retrieve a channel list without affecting runtime state.
 */
HAL_STATUS
ath_hal_getchannels(struct ath_hal *ah,
    struct ieee80211_channel chans[], u_int maxchans, int *nchans,
    u_int modeSelect, HAL_CTRY_CODE cc, HAL_REG_DOMAIN regDmn,
    HAL_BOOL enableExtendedChannels)
{
	return getchannels(ah, chans, maxchans, nchans, modeSelect,
	    cc, regDmn, enableExtendedChannels, AH_NULL, AH_NULL, AH_NULL);
}

/*
 * Handle frequency mapping from 900Mhz range to 2.4GHz range
 * for GSM radios.  This is done when we need the h/w frequency
 * and the channel is marked IEEE80211_CHAN_GSM.
 */
static int
ath_hal_mapgsm(int sku, int freq)
{
	if (sku == SKU_XR9)
		return 1520 + freq;
	if (sku == SKU_GZ901)
		return 1544 + freq;
	if (sku == SKU_SR9)
		return 3344 - freq;
	if (sku == SKU_XC900M)
		return 1517 + freq;
	HALDEBUG(AH_NULL, HAL_DEBUG_ANY,
	    "%s: cannot map freq %u unknown gsm sku %u\n",
	    __func__, freq, sku);
	return freq;
}

/*
 * Setup the internal/private channel state given a table of
 * net80211 channels.  We collapse entries for the same frequency
 * and record the frequency for doing noise floor processing
 * where we don't have net80211 channel context.
 */
static HAL_BOOL
assignPrivateChannels(struct ath_hal *ah,
	struct ieee80211_channel chans[], int nchans, int sku)
{
	HAL_CHANNEL_INTERNAL *ic;
	int i, j, next, freq;

	next = 0;
	for (i = 0; i < nchans; i++) {
		struct ieee80211_channel *c = &chans[i];
		for (j = i-1; j >= 0; j--)
			if (chans[j].ic_freq == c->ic_freq) {
				c->ic_devdata = chans[j].ic_devdata;
				break;
			}
		if (j < 0) {
			/* new entry, assign a private channel entry */
			if (next >= N(AH_PRIVATE(ah)->ah_channels)) {
				HALDEBUG(ah, HAL_DEBUG_ANY,
				    "%s: too many channels, max %zu\n",
				    __func__, N(AH_PRIVATE(ah)->ah_channels));
				return AH_FALSE;
			}
			/*
			 * Handle frequency mapping for 900MHz devices.
			 * The hardware uses 2.4GHz frequencies that are
			 * down-converted.  The 802.11 layer uses the
			 * true frequencies.
			 */
			freq = IEEE80211_IS_CHAN_GSM(c) ?
			    ath_hal_mapgsm(sku, c->ic_freq) : c->ic_freq;

			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: private[%3u] %u/0x%x -> channel %u\n",
			    __func__, next, c->ic_freq, c->ic_flags, freq);

			ic = &AH_PRIVATE(ah)->ah_channels[next];
			/*
			 * NB: This clears privFlags which means ancillary
			 *     code like ANI and IQ calibration will be
			 *     restarted and re-setup any per-channel state.
			 */
			OS_MEMZERO(ic, sizeof(*ic));
			ic->channel = freq;
			c->ic_devdata = next;
			next++;
		}
	}
	AH_PRIVATE(ah)->ah_nchan = next;
	HALDEBUG(ah, HAL_DEBUG_ANY, "%s: %u public, %u private channels\n",
	    __func__, nchans, next);
	return AH_TRUE;
}

/*
 * Setup the channel list based on the information in the EEPROM.
 */
HAL_STATUS
ath_hal_init_channels(struct ath_hal *ah,
    struct ieee80211_channel chans[], u_int maxchans, int *nchans,
    u_int modeSelect, HAL_CTRY_CODE cc, HAL_REG_DOMAIN regDmn,
    HAL_BOOL enableExtendedChannels)
{
	COUNTRY_CODE_TO_ENUM_RD *country;
	REG_DOMAIN *rd5GHz, *rd2GHz;
	HAL_STATUS status;

	status = getchannels(ah, chans, maxchans, nchans, modeSelect,
	    cc, regDmn, enableExtendedChannels, &country, &rd2GHz, &rd5GHz);
	if (status == HAL_OK &&
	    assignPrivateChannels(ah, chans, *nchans, AH_PRIVATE(ah)->ah_currentRD)) {
		AH_PRIVATE(ah)->ah_rd2GHz = rd2GHz;
		AH_PRIVATE(ah)->ah_rd5GHz = rd5GHz;

		ah->ah_countryCode = country->countryCode;
		HALDEBUG(ah, HAL_DEBUG_REGDOMAIN, "%s: cc %u\n",
		    __func__, ah->ah_countryCode);

		/* Update current DFS domain */
		ath_hal_update_dfsdomain(ah);
	} else
		status = HAL_EINVAL;

	return status;
}

/*
 * Set the channel list.
 */
HAL_STATUS
ath_hal_set_channels(struct ath_hal *ah,
    struct ieee80211_channel chans[], int nchans,
    HAL_CTRY_CODE cc, HAL_REG_DOMAIN rd)
{
	COUNTRY_CODE_TO_ENUM_RD *country;
	REG_DOMAIN *rd5GHz, *rd2GHz;
	HAL_STATUS status;

	switch (rd) {
	case SKU_SR9:
	case SKU_XR9:
	case SKU_GZ901:
	case SKU_XC900M:
		/*
		 * Map 900MHz sku's.  The frequencies will be mapped
		 * according to the sku to compensate for the down-converter.
		 * We use the FCC for these sku's as the mapped channel
		 * list is known compatible (will need to change if/when
		 * vendors do different mapping in different locales).
		 */
		status = getregstate(ah, CTRY_DEFAULT, SKU_FCC,
		    &country, &rd2GHz, &rd5GHz);
		break;
	default:
		status = getregstate(ah, cc, rd,
		    &country, &rd2GHz, &rd5GHz);
		rd = AH_PRIVATE(ah)->ah_currentRD;
		break;
	}
	if (status == HAL_OK && assignPrivateChannels(ah, chans, nchans, rd)) {
		AH_PRIVATE(ah)->ah_rd2GHz = rd2GHz;
		AH_PRIVATE(ah)->ah_rd5GHz = rd5GHz;

		ah->ah_countryCode = country->countryCode;
		HALDEBUG(ah, HAL_DEBUG_REGDOMAIN, "%s: cc %u\n",
		    __func__, ah->ah_countryCode);
	} else
		status = HAL_EINVAL;

	if (status == HAL_OK) {
		/* Update current DFS domain */
		(void) ath_hal_update_dfsdomain(ah);
	}
	return status;
}

#ifdef AH_DEBUG
/*
 * Return the internal channel corresponding to a public channel.
 * NB: normally this routine is inline'd (see ah_internal.h)
 */
HAL_CHANNEL_INTERNAL *
ath_hal_checkchannel(struct ath_hal *ah, const struct ieee80211_channel *c)
{
	HAL_CHANNEL_INTERNAL *cc = &AH_PRIVATE(ah)->ah_channels[c->ic_devdata];

	if (c->ic_devdata < AH_PRIVATE(ah)->ah_nchan &&
	    (c->ic_freq == cc->channel || IEEE80211_IS_CHAN_GSM(c)))
		return cc;
	if (c->ic_devdata >= AH_PRIVATE(ah)->ah_nchan) {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: bad mapping, devdata %u nchans %u\n",
		   __func__, c->ic_devdata, AH_PRIVATE(ah)->ah_nchan);
		HALASSERT(c->ic_devdata < AH_PRIVATE(ah)->ah_nchan);
	} else {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: no match for %u/0x%x devdata %u channel %u\n",
		   __func__, c->ic_freq, c->ic_flags, c->ic_devdata,
		   cc->channel);
		HALASSERT(c->ic_freq == cc->channel || IEEE80211_IS_CHAN_GSM(c));
	}
	return AH_NULL;
}
#endif /* AH_DEBUG */

#define isWwrSKU(_ah) \
	((getEepromRD((_ah)) & WORLD_SKU_MASK) == WORLD_SKU_PREFIX || \
	  getEepromRD(_ah) == WORLD)

/*
 * Return the test group for the specific channel based on
 * the current regulatory setup.
 */
u_int
ath_hal_getctl(struct ath_hal *ah, const struct ieee80211_channel *c)
{
	u_int ctl;

	if (AH_PRIVATE(ah)->ah_rd2GHz == AH_PRIVATE(ah)->ah_rd5GHz ||
	    (ah->ah_countryCode == CTRY_DEFAULT && isWwrSKU(ah)))
		ctl = SD_NO_CTL;
	else if (IEEE80211_IS_CHAN_2GHZ(c))
		ctl = AH_PRIVATE(ah)->ah_rd2GHz->conformanceTestLimit;
	else
		ctl = AH_PRIVATE(ah)->ah_rd5GHz->conformanceTestLimit;
	if (IEEE80211_IS_CHAN_B(c))
		return ctl | CTL_11B;
	if (IEEE80211_IS_CHAN_G(c))
		return ctl | CTL_11G;
	if (IEEE80211_IS_CHAN_108G(c))
		return ctl | CTL_108G;
	if (IEEE80211_IS_CHAN_TURBO(c))
		return ctl | CTL_TURBO;
	if (IEEE80211_IS_CHAN_A(c))
		return ctl | CTL_11A;
	return ctl;
}


/*
 * Update the current dfsDomain setting based on the given
 * country code.
 *
 * Since FreeBSD/net80211 allows the channel set to change
 * after the card has been setup (via ath_hal_init_channels())
 * this function method is needed to update ah_dfsDomain.
 */
void
ath_hal_update_dfsdomain(struct ath_hal *ah)
{
	const REG_DOMAIN *rd5GHz = AH_PRIVATE(ah)->ah_rd5GHz;
	HAL_DFS_DOMAIN dfsDomain = HAL_DFS_UNINIT_DOMAIN;

	if (rd5GHz->dfsMask & DFS_FCC3)
		dfsDomain = HAL_DFS_FCC_DOMAIN;
	if (rd5GHz->dfsMask & DFS_ETSI)
		dfsDomain = HAL_DFS_ETSI_DOMAIN;
	if (rd5GHz->dfsMask & DFS_MKK4)
		dfsDomain = HAL_DFS_MKK4_DOMAIN;
	AH_PRIVATE(ah)->ah_dfsDomain = dfsDomain;
	HALDEBUG(ah, HAL_DEBUG_REGDOMAIN, "%s ah_dfsDomain: %d\n",
	    __func__, AH_PRIVATE(ah)->ah_dfsDomain);
}


/*
 * Return the max allowed antenna gain and apply any regulatory
 * domain specific changes.
 *
 * NOTE: a negative reduction is possible in RD's that only
 * measure radiated power (e.g., ETSI) which would increase
 * that actual conducted output power (though never beyond
 * the calibrated target power).
 */
u_int
ath_hal_getantennareduction(struct ath_hal *ah,
    const struct ieee80211_channel *chan, u_int twiceGain)
{
	int8_t antennaMax = twiceGain - chan->ic_maxantgain*2;
	return (antennaMax < 0) ? 0 : antennaMax;
}
