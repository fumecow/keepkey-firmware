/* START KEEPKEY LICENSE */
/*
 * This file is part of the KeepKey project.
 *
 * Copyright (C) 2015 KeepKey LLC
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
/* END KEEPKEY LICENSE */

/*
 * @brief General confirmation state machine.
 */

#include <stdbool.h>
#include "passphrase_sm.h"
#include "fsm.h"
#include <layout.h>
#include <msg_dispatch.h>
#include <rand.h>
#include <storage.h>
#include <timer.h>


/******************** Statics and Global variables ***********************/

/*Flag whether passphrase was canceled by init msg */
static bool passphrase_canceled_by_init = false;

//******************** PRIVATE FUNCTION DECLARATIONS *********************/

/*
 * send_passphrase_request() - send passphrase request to usb host
 *
 * INPUT - none
 * OUTPUT - none
 */
static void send_passphrase_request(void)
{
	PassphraseRequest resp;
	memset(&resp, 0, sizeof(PassphraseRequest));
	msg_write(MessageType_MessageType_PassphraseRequest, &resp);
}

/*
 * wait_for_passphrase_ack() - wait for passphrase acknowledgement from USB host
 *
 * INPUT - 
 *      *passphrase_info - pointer to load message from usb host
 * OUTPUT - none
 */
static void wait_for_passphrase_ack(PassphraseInfo *passphrase_info)
{
	/* Listen for tiny messages */
	uint8_t msg_tiny_buf[64];
	uint16_t tiny_msg = wait_for_tiny_msg(msg_tiny_buf);

	/* Check for standard passphrase ack */
	if(tiny_msg == MessageType_MessageType_PassphraseAck)
	{
		passphrase_info->passphrase_ack_msg = PASSPHRASE_ACK_RECEIVED;
		PassphraseAck *ppa = (PassphraseAck *)msg_tiny_buf;

		strcpy(passphrase_info->passphrase, ppa->passphrase);
	}

	/* Check for passphrase tumbler ack */
	//TODO:Implement passphrase tumbler

	/* Check for cancel or initialize messages */
	if(tiny_msg == MessageType_MessageType_Cancel)
		passphrase_info->passphrase_ack_msg = PASSPHRASE_ACK_CANCEL;

	if(tiny_msg == MessageType_MessageType_Initialize)
		passphrase_info->passphrase_ack_msg = PASSPHRASE_ACK_CANCEL_BY_INIT;
}

/*
 * run_passphrase_state() - passphrase state machine 
 *
 * INPUT - 
 * OUTPUT -
 */
static void run_passphrase_state(PassphraseState *passphrase_state, PassphraseInfo *passphrase_info)
{
	switch(*passphrase_state){

		/* Send passphrase request */
		case PASSPHRASE_REQUEST:
			send_passphrase_request();
			*passphrase_state = PASSPHRASE_WAITING;
			break;

		/* Wait for a passphrase */
		case PASSPHRASE_WAITING:
			wait_for_passphrase_ack(passphrase_info);
			if(passphrase_info->passphrase_ack_msg != PASSPHRASE_ACK_WAITING) {
				*passphrase_state = PASSPHRASE_FINISHED;
            }
			break;
	}
}

/*
 * passphrase_request() - request passphrase from user on usb host
 *
 * INPUT - 
 *  *passphrase_info - pointer to passphrase info
 * OUTPUT -
 *      true/false - status
 */
static bool passphrase_request(PassphraseInfo *passphrase_info)
{
	bool ret = false;
	passphrase_canceled_by_init = false;
	PassphraseState passphrase_state = PASSPHRASE_REQUEST;

	/* Run SM */
	while(1) {
		run_passphrase_state(&passphrase_state, passphrase_info);

		if(passphrase_state == PASSPHRASE_FINISHED) {
			break;
        }
	}

	/* Check for passphrase cancel */
	if (passphrase_info->passphrase_ack_msg == PASSPHRASE_ACK_RECEIVED) {
		ret = true;
    } else {
		if(passphrase_info->passphrase_ack_msg == PASSPHRASE_ACK_CANCEL_BY_INIT) {
			passphrase_canceled_by_init = true;
        }
    }

	return (ret);
}

/*
 * passphrase_protect() - set passphrase protection
 *  
 * INPUT -  none
 * OUTPUT -
 *      true/false - status
 */
bool passphrase_protect()
{
	bool ret = false;
	PassphraseInfo passphrase_info;

	if(storage_get_passphrase_protected() && !session_isPassphraseCached()) {
		/* Get passphrase and cache */
		if(passphrase_request(&passphrase_info)) {
			session_cachePassphrase(passphrase_info.passphrase);
			ret = true;
		}
	} else {
		ret = true;
    }

	return (ret);
}

/*
 * cancel_passphrase() - process passphrase cancellation
 *
 * INPUT - 
 *      code - 
 *      *text - 
 * OUTPUT -
 *      none
 */
void cancel_passphrase(FailureType code, const char *text)
{
	if(passphrase_canceled_by_init)
		call_msg_initialize_handler();
	else
		call_msg_failure_handler(code, text);

	passphrase_canceled_by_init = false;
}
