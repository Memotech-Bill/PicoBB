/*
 * Copyright (c) 2023 Memotech-Bill
 *
 * Provides a stdio driver for the Pico SDK using a Bluetooth serial
 * connection.
 *
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <hardware/sync.h>
#include <btstack.h>
#include <stdio_bt.h>

#define RFCOMM_SERVER_CHANNEL 1

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t rfcomm_channel_id = 0;
static uint8_t  spp_service_buffer[150];
static btstack_packet_callback_registration_t hci_event_callback_registration;

// Received data

#ifndef NUM_BT_PKT
#define NUM_BT_PKT  2
#endif
#ifndef BT_PKT_LEN
#define BT_PKT_LEN  128
#endif
#define BT_BUF_LEN  (NUM_BT_PKT * BT_PKT_LEN)

static char buffer[BT_BUF_LEN];
static int rptr = 0;
static int wptr = 0;
static int ncredit = NUM_BT_PKT;
static void (*received_cb)(void *) = NULL;
static void *cb_param = NULL;

// Data to send

static const char *psend = NULL;
static int nsend = 0;
static volatile bool bSent = false;

static void add_credit (void)
    {
    int nfree = rptr - wptr;
    if ( nfree <= 0 ) nfree += BT_BUF_LEN;
    int ngrant = ( nfree - 1 ) / BT_PKT_LEN - ncredit;
    if ( ngrant > 0 )
        {
        rfcomm_grant_credits (rfcomm_channel_id, ngrant);
        }
    }

static void save_data (const char *data, int nlen)
    {
    if ( nlen <= 0 ) return;
    if ( wptr + nlen > BT_BUF_LEN )
        {
        int nwr = BT_BUF_LEN - wptr;
        memcpy (&buffer[wptr], data, nwr);
        wptr = 0;
        nlen -= nwr;
        data += nwr;
        }
    memcpy (&buffer[wptr], data, nlen);
    wptr += nlen;
    if ( received_cb != NULL ) received_cb (cb_param);
    }

/* @section SPP Service Setup 
 *s
 * @text To provide an SPP service, the L2CAP, RFCOMM, and SDP protocol layers 
 * are required. After setting up an RFCOMM service with channel nubmer
 * RFCOMM_SERVER_CHANNEL, an SDP record is created and registered with the SDP server.
 * Example code for SPP service setup is
 * provided in Listing SPPSetup. The SDP record created by function
 * spp_create_sdp_record consists of a basic SPP definition that uses the provided
 * RFCOMM channel ID and service name. For more details, please have a look at it
 * in \path{src/sdp_util.c}. 
 * The SDP record is created on the fly in RAM and is deterministic.
 * To preserve valuable RAM, the result could be stored as constant data inside the ROM.   
 */

static void spp_service_setup(void)
    {
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    rfcomm_init();
    rfcomm_register_service_with_initial_credits(packet_handler, RFCOMM_SERVER_CHANNEL, BT_PKT_LEN, ncredit);  // reserved channel

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    spp_create_sdp_record(spp_service_buffer, 0x10001, RFCOMM_SERVER_CHANNEL, "PicoBB");
    sdp_register_service(spp_service_buffer);
    // printf("SDP service record size: %u\n", de_get_len(spp_service_buffer));
    }

/* @section Bluetooth Logic 
 * @text The Bluetooth logic is implemented within the 
 * packet handler, see Listing SppServerPacketHandler. In this example, 
 * the following events are passed sequentially: 
 * - BTSTACK_EVENT_STATE,
 * - HCI_EVENT_PIN_CODE_REQUEST (Standard pairing) or 
 * - HCI_EVENT_USER_CONFIRMATION_REQUEST (Secure Simple Pairing),
 * - RFCOMM_EVENT_INCOMING_CONNECTION,
 * - RFCOMM_EVENT_CHANNEL_OPENED, 
 * - RFCOMM_EVETN_CAN_SEND_NOW, and
 * - RFCOMM_EVENT_CHANNEL_CLOSED
 */

/* @text Upon receiving HCI_EVENT_PIN_CODE_REQUEST event, we need to handle
 * authentication. Here, we use a fixed PIN code "0000".
 *
 * When HCI_EVENT_USER_CONFIRMATION_REQUEST is received, the user will be 
 * asked to accept the pairing request. If the IO capability is set to 
 * SSP_IO_CAPABILITY_DISPLAY_YES_NO, the request will be automatically accepted.
 *
 * The RFCOMM_EVENT_INCOMING_CONNECTION event indicates an incoming connection.
 * Here, the connection is accepted. More logic is need, if you want to handle connections
 * from multiple clients. The incoming RFCOMM connection event contains the RFCOMM
 * channel number used during the SPP setup phase and the newly assigned RFCOMM
 * channel ID that is used by all BTstack commands and events.
 *
 * If RFCOMM_EVENT_CHANNEL_OPENED event returns status greater then 0,
 * then the channel establishment has failed (rare case, e.g., client crashes).
 * On successful connection, the RFCOMM channel ID and MTU for this
 * channel are made available to the heartbeat counter. After opening the RFCOMM channel, 
 * the communication between client and the application
 * takes place. In this example, the timer handler increases the real counter every
 * second. 
 *
 * RFCOMM_EVENT_CAN_SEND_NOW indicates that it's possible to send an RFCOMM packet
 * on the rfcomm_cid that is include

 */

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
    {
    UNUSED(channel);

    bd_addr_t event_addr;
    uint8_t   rfcomm_channel_nr;
    uint16_t  mtu;
    int i;

    switch (packet_type)
        {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet))
                {
                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    // printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // ssp: inform about user confirmation request
                    // printf("SSP User Confirmation Request with numeric value '%06"PRIu32"'\n", little_endian_read_32(packet, 8));
                    // printf("SSP User Confirmation Auto accept\n");
                    break;

                case RFCOMM_EVENT_INCOMING_CONNECTION:
                    rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
                    rfcomm_channel_nr = rfcomm_event_incoming_connection_get_server_channel(packet);
                    rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    // printf("RFCOMM channel %u requested for %s\n", rfcomm_channel_nr, bd_addr_to_str(event_addr));
                    rfcomm_accept_connection(rfcomm_channel_id);
                    break;
               
                case RFCOMM_EVENT_CHANNEL_OPENED:
                    if (rfcomm_event_channel_opened_get_status(packet))
                        {
                        // printf("RFCOMM channel open failed, status 0x%02x\n", rfcomm_event_channel_opened_get_status(packet));
                        }
                    else
                        {
                        rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        // printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_channel_id, mtu);
                        }
                    break;
                case RFCOMM_EVENT_CAN_SEND_NOW:
                    if ( rfcomm_send(rfcomm_channel_id, (uint8_t*) psend, (uint16_t) nsend) == 0 )
                        {
                        bSent = true;
                        }
                    else
                        {
                        rfcomm_request_can_send_now_event (rfcomm_channel_id);
                        }
                    break;

                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    // printf("RFCOMM channel closed\n");
                    rfcomm_channel_id = 0;
                    break;
                
                default:
                    break;
                }
            break;

        case RFCOMM_DATA_PACKET:
            save_data ((const char *) packet, size);
            --ncredit;
            add_credit ();
            break;

        default:
            break;
        }
    }

static void init_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
    {
    UNUSED(size);
    UNUSED(channel);
    bd_addr_t local_addr;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet))
        {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            gap_local_bd_addr(local_addr);
            // printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
            break;
        default:
            break;
        }
    }

void stdio_bt_init (void)
    {
    // printf ("stdio_bt_init\n");

    // inform about BTstack state
    hci_event_callback_registration.callback = &init_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    
    spp_service_setup();

    gap_discoverable_control(1);
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_set_local_name("PicoBB 00:00:00:00:00:00");

    // turn on!
    hci_power_control(HCI_POWER_ON);
    }

static void stdio_bt_out_chars (const char *buf, int len)
    {
    if ( rfcomm_channel_id == 0 ) return;
    bSent = false;
    psend = buf;
    nsend = len;
    rfcomm_request_can_send_now_event (rfcomm_channel_id);
    while ( ! bSent )
        {
        __wfi ();
        }
    }

static int stdio_bt_in_chars (char *buf, int len)
    {
    int nrd1 = 0;
    int nrd2 = wptr - rptr;
    if ( nrd2 < 0 ) nrd2 += BT_BUF_LEN;
    if ( nrd2 > len ) nrd2 = len;
    if ( rptr + nrd2 > BT_BUF_LEN )
        {
        nrd1 = BT_BUF_LEN - rptr;
        memcpy (buf, &buffer[rptr], nrd1);
        rptr = 0;
        nrd2 -= nrd1;
        buf += nrd1;
        }
    memcpy (buf, &buffer[rptr], nrd2);
    rptr += nrd2;
    add_credit ();
    return nrd1 + nrd2;
    }

static void stdio_bt_set_chars_available_callback(void (*fn)(void*), void *param)
    {
    received_cb = fn;
    cb_param = param;
    }

stdio_driver_t stdio_bt = {
    .out_chars = stdio_bt_out_chars,
    .in_chars = stdio_bt_in_chars,
    .set_chars_available_callback = stdio_bt_set_chars_available_callback,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = PICO_STDIO_DEFAULT_CRLF
#endif
};

bool stdio_bt_connected (void)
    {
    return rfcomm_channel_id > 0;
    };
