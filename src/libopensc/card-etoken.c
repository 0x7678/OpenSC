/*
 * card-etoken.c: Support for Aladdin eToken PRO 
 *
 * Copyright (C) 2002  Andreas Jellinghaus <aj@dungeon.inka.de>
 * Copyright (C) 2001  Juha Yrj�l� <juha.yrjola@iki.fi>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "errors.h"
#include "opensc.h"
#include "log.h"

#include <ctype.h>

struct sc_card_operations etoken_ops;
const struct sc_card_driver etoken_drv = {
	"Aladdin eToken PRO",
	"etoken",
	&etoken_ops
};

const u8 etoken_atr[] = { 0x3b, 0xe2, 0x00, 0xff, 0xc1,
	0x10, 0x31, 0xfe, 0x55, 0xc8, 0x02, 0x9c };

int etoken_finish(struct sc_card *card)
{
	return 0;
}

int etoken_match_card(struct sc_card *card)
{

	if (memcmp(card->atr, etoken_atr, sizeof(etoken_atr)) == 0) {
		return 1;
	} else {
		return 0;
	}
}

int etoken_init(struct sc_card *card)
{
	card->cla = 0x00;


	return 0;
}

const static struct sc_card_error etoken_errors[] = {
/* some error inside the card */
/* i.e. nothing you can do */
{ 0x6581, SC_ERROR_MEMORY_FAILURE,	"EEPROM error; command aborted"}, 
{ 0x6fff, SC_ERROR_CARD_CMD_FAILED,	"internal assertion error"},
{ 0x6700, SC_ERROR_WRONG_LENGTH,	"LC invalid"}, 
{ 0x6985, SC_ERROR_CARD_CMD_FAILED,	"no random number available"}, 
{ 0x6f81, SC_ERROR_CARD_CMD_FAILED,	"file invalid, maybe checksum error"}, 
{ 0x6f82, SC_ERROR_CARD_CMD_FAILED,	"not enough memory in xram"}, 
{ 0x6f84, SC_ERROR_CARD_CMD_FAILED,	"general protection fault"}, 

/* the card doesn't now thic combination of ins+cla+p1+p2 */
/* i.e. command will never work */
{ 0x6881, SC_ERROR_NO_CARD_SUPPORT,	"logical channel not supported"}, 
{ 0x6a86, SC_ERROR_INCORRECT_PARAMETERS,"p1/p2 invalid"}, 
{ 0x6d00, SC_ERROR_INS_NOT_SUPPORTED,	"ins invalid"}, 
{ 0x6e00, SC_ERROR_CLASS_NOT_SUPPORTED,	"class invalid (hi nibble)"}, 

/* known command, but incorrectly used */
/* i.e. command could work, but you need to change something */
{ 0x6981, SC_ERROR_CARD_CMD_FAILED,	"command cannot be used for file structure"}, 
{ 0x6a80, SC_ERROR_INCORRECT_PARAMETERS,"invalid parameters in data field"}, 
{ 0x6a81, SC_ERROR_NOT_SUPPORTED,	"function/mode not supported"}, 
{ 0x6a85, SC_ERROR_INCORRECT_PARAMETERS,"lc does not fit the tlv structure"}, 
{ 0x6986, SC_ERROR_INCORRECT_PARAMETERS,"no current ef selected"}, 
{ 0x6a87, SC_ERROR_INCORRECT_PARAMETERS,"lc does not fit p1/p2"}, 
{ 0x6c00, SC_ERROR_WRONG_LENGTH,	"le does not fit the data to be sent"}, 
{ 0x6f83, SC_ERROR_CARD_CMD_FAILED,	"command must not be used in transaction"}, 

/* (something) not found */
{ 0x6987, SC_ERROR_INCORRECT_PARAMETERS,"key object for sm not found"}, 
{ 0x6f86, SC_ERROR_CARD_CMD_FAILED,	"key object not found"}, 
{ 0x6a82, SC_ERROR_FILE_NOT_FOUND,	"file not found"}, 
{ 0x6a83, SC_ERROR_RECORD_NOT_FOUND,	"record not found"}, 
{ 0x6a88, SC_ERROR_CARD_CMD_FAILED,	"object not found"}, 

/* (something) invalid */
{ 0x6884, SC_ERROR_CARD_CMD_FAILED,	"chaining error"}, 
{ 0x6984, SC_ERROR_CARD_CMD_FAILED,	"bs object has invalid format"}, 
{ 0x6988, SC_ERROR_INCORRECT_PARAMETERS,"key object used for sm has invalid format"}, 

/* (something) deactivated */
{ 0x6283, SC_ERROR_CARD_CMD_FAILED,	"file is deactivated"	},
{ 0x6983, SC_ERROR_AUTH_METHOD_BLOCKED,	"bs object blocked"}, 

/* access denied */
{ 0x6300, SC_ERROR_SECURITY_STATUS_NOT_SATISFIED,"authentication failed"}, 
{ 0x6982, SC_ERROR_SECURITY_STATUS_NOT_SATISFIED,"required access right not granted"}, 

/* other errors */
{ 0x6a84, SC_ERROR_CARD_CMD_FAILED,	"not enough memory"}, 

/* command ok, execution failed */
{ 0x6f00, SC_ERROR_CARD_CMD_FAILED,	"technical error (see eToken developers guide)"}, 

/* no error, maybe a note */
{ 0x9000, SC_NO_ERROR,		NULL}, 
{ 0x9001, SC_NO_ERROR,		"success, but eeprom weakness detected"}, 
{ 0x9850, SC_NO_ERROR,		"over/underflow useing in/decrease"}
};

static int etoken_check_sw(struct sc_card *card, int sw1, int sw2)
{
	const int err_count = sizeof(etoken_errors)/sizeof(etoken_errors[0]);
	int i;
			        
	for (i = 0; i < err_count; i++) {
		if (etoken_errors[i].SWs == ((sw1 << 8) | sw2)) {
			if ( etoken_errors[i].errorstr ) 
				error(card->ctx, "%s\n",
				 	etoken_errors[i].errorstr);
			return etoken_errors[i].errorno;
		}
	}

        error(card->ctx, "Unknown SWs; SW1=%02X, SW2=%02X\n", sw1, sw2);
	return SC_ERROR_CARD_CMD_FAILED;
}

u8 etoken_extract_offset(u8 *buf, int buflen) {
	int i;
	int mode;
	u8 tag,len;

	tag=0; len=0;
	mode = 0;

	for (i=0; i < buflen;) {
		if (mode == 0) {
			tag = buf[i++];
			mode=1;
			continue;
		}
		if (mode == 1) {
			len=buf[i++];
			mode=2;
			continue;
		}
		if (len == 0) {
			mode=0;
			continue;
		}
		if (tag == 0x8a && len == 1) {
			return buf[i];
		}

		i+=len;
		mode=0;
	}
	
	return 0;
}

u8* etoken_extract_fid(u8 *buf, int buflen) {
	int i;
	int mode;
	u8 tag,len;

	mode = 0;
	tag = 0;
	len = 0;


	for (i=0; i < buflen;) {
		if (mode == 0) {
			tag = buf[i++];
			mode=1;
			continue;
		}
		if (mode == 1) {
			len=buf[i++];
			mode=2;
			continue;
		}
		if (len == 0) {
			mode=0;
			continue;
		}
		if ((tag == 0x86) && (len == 2) && (i+1 < buflen)) {
			return &buf[i];
		}

		i+=len;
		mode=0;
	}
	
	return NULL;
}

int etoken_list_files(struct sc_card *card, u8 *buf, size_t buflen)
{
	struct sc_apdu apdu;
	u8 rbuf[256];
	u8 fidbuf[256];
	int r,i;
	int fids;
	int len;
	u8 offset;
	u8 *fid;

	fids=0;
	offset=0;

get_next_part:
	sc_format_apdu(card, &apdu, SC_APDU_CASE_2_SHORT, 0x16, 0x02, offset);
	apdu.cla = 0x80;
	apdu.le = 256;
	apdu.resplen = 256;
	apdu.resp = rbuf;
	r = sc_transmit_apdu(card, &apdu);
	if (r) 
		return r;
	if (apdu.resplen > 256) {
		error(card->ctx, "directory listing > 256 bytes, cutting");
		r = 256;
	}
	for (i=0; i < apdu.resplen;) {
		/* is there a file informatin block (0x6f) ? */
		if (rbuf[i] != 0x6f) {
			error(card->ctx, "directory listing not parseable");
			break;
		}
		if (i+1 > apdu.resplen) {
			error(card->ctx, "directory listing short");
			break;
		}
		len = rbuf[i+1];
		if (i + 1 + len > apdu.resplen) {
			error(card->ctx, "directory listing short");
			break;
		}
		fid = etoken_extract_fid(&rbuf[i+2], len);

		if (fid) {
			fidbuf[fids++] = fid[0];
			fidbuf[fids++] = fid[1];
			if (fids >= 128) {
				error(card->ctx,"only memory for 128 fids etoken_list_files");
				fids=128;
				goto end;
			}
		}

		offset = etoken_extract_offset(&rbuf[i+2], len);
		if (offset) 
			goto get_next_part;
		i+=len+2;
	}

end:
	memcpy(buf,fidbuf,2*fids);
	return fids;
}

const struct sc_card_driver * sc_get_driver(void)
{
	etoken_ops = *sc_get_iso7816_driver()->ops;
	etoken_ops.match_card = etoken_match_card;
	etoken_ops.init = etoken_init;
        etoken_ops.finish = etoken_finish;

        etoken_ops.list_files = etoken_list_files;
	etoken_ops.check_sw = etoken_check_sw;

        return &etoken_drv;
}

#if 1
const struct sc_card_driver * sc_get_etoken_driver(void)
{
	return sc_get_driver();
}
#endif
