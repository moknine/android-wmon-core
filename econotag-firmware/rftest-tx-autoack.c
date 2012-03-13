/*
 * Copyright (c) 2010, Mariano Alvira <mar@devl.org> and other contributors
 * to the MC1322x project (http://mc1322x.devl.org)
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of libmc1322x: see http://mc1322x.devl.org
 * for details. 
 *
 *
 */

#include <mc1322x.h>
#include <board.h>
#include <stdio.h>

#include "tests.h"
#include "config.h"

#define LED LED_RED

/* 802.15.4 PSDU is 127 MAX */
/* 2 bytes are the FCS */
/* therefore 125 is the max payload length */
#define PAYLOAD_LEN 16
#define DELAY 0
int next_packet;
unsigned int cnt;


void fill_packet(volatile packet_t *p) {
	static volatile uint8_t count=0;

	p->length = 20;
	p->offset = 0;
	p->data[0] = 0x71;  /* 0b 10 01 10 000 1 1 0 0 001 data, ack request, short addr */
	p->data[1] = 0x98;  /* 0b 10 01 10 000 1 1 0 0 001 data, ack request, short addr */
	p->data[2] = count++; /* dsn */
	p->data[3] = 0xaa;    /* pan */
	p->data[4] = 0xaa;
	p->data[5] = 0x11;    /* dest. short addr. */
 	p->data[6] = 0x11;
	p->data[7] = 0x22;    /* src. short addr. */
 	p->data[8] = 0x22;

	/* payload */
	p->data[9] = 'a';
	p->data[10] = 'c';
	p->data[11] = 'k';
	p->data[12] = 't';
	p->data[13] = 'e';
	p->data[14] = 's';
	p->data[15] = 't';

	p->data[19] = cnt & 0xff;
	p->data[18] = (cnt >> 8*1) & 0xff;
	p->data[17] = (cnt >> 8*2) & 0xff;
	p->data[16] = (cnt >> 8*3) & 0xff;
	cnt++;
}

void maca_tx_callback(volatile packet_t *p) {
	unsigned int val=0;
	val = val | (p->data[16] << 8*3);
	val = val | (p->data[17] << 8*2);
	val = val | (p->data[18] << 8*1);
	val = val | (p->data[19]);
	
	switch(p->status) {
	case 0:
		printf("%u TX OK\n\r", val);
		break;
	case 3:
		printf("%u CRC ERR\n\r", val);
		break;
	case 5:
		printf("%u NO ACK\n\r", val);
		break;
	default:
		printf("unknown status: %d\n", (int)p->status);
	}
	next_packet=1;
}

void main(void) {
	volatile packet_t *p;
//	char c;
	uint16_t r=30; /* start reception 100us before ack should arrive */
	uint16_t end=180; /* 750 us receive window*/
	next_packet=1;
	int i;
	cnt=0;

	/* trim the reference osc. to 24MHz */
	trim_xtal();
	uart_init(INC, MOD, SAMP);
	maca_init();

	set_channel(9); /* channel 11 */
//	set_power(0x0f); /* 0xf = -1dbm, see 3-22 */
//	set_power(0x11); /* 0x11 = 3dbm, see 3-22 */
	set_power(0x12); /* 0x12 is the highest, not documented */

        /* sets up tx_on, should be a board specific item */
	GPIO->FUNC_SEL_44 = 1;	 
	GPIO->PAD_DIR_SET_44 = 1;	 

	GPIO->FUNC_SEL_45 = 2;	 
	GPIO->PAD_DIR_SET_45 = 1;	 

	*MACA_RXACKDELAY = r;
	
	printf("rx warmup: %d\n\r", (int)(*MACA_WARMUP & 0xfff));

	*MACA_RXEND = end;

	printf("rx end: %d\n\r", (int)(*MACA_RXEND & 0xfff));

	set_prm_mode(AUTOACK);

	print_welcome("rftest-tx");

	while(1) {		
		for(i=0; i<DELAY; i++) { continue; }
	    		
		/* call check_maca() periodically --- this works around */
		/* a few lockup conditions */
		check_maca();

		while((p = rx_packet())) {
			if(p) {
				printf("RX: ");
				print_packet(p);
				free_packet(p);
			}
		}

		if(next_packet==1) {
			next_packet=0;
			p = get_free_packet();
			if(p) {
				fill_packet(p);
				tx_packet(p);				
			}
		}
	}

}