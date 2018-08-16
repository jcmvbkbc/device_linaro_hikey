/*
 * Copyright (c) 2016 - 2017 Cadence Design Systems Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtensa/corebits.h>
#include <xtensa/xtruntime.h>
#include "xrp_api.h"
#include "xrp_dsp_hw.h"

struct ring_buffer {
	uint32_t panic;
	uint32_t interrupt;
	uint32_t ccount;
	uint32_t read;
	uint32_t write;
	uint32_t size;
	uint32_t stack;
	uint32_t reserved[9];
	char data[0];
};

static volatile struct ring_buffer *p = (volatile void *)0x8b300000;

static ssize_t debug_write(void *cookie, const char *buf, size_t size)
{
	volatile struct ring_buffer *rb = cookie;
	uint32_t read = rb->read;
	uint32_t write = rb->write;
	size_t total;
	size_t tail;
	uint32_t stack;

	asm volatile ("mov %0, a1" : "=a"(stack) :: "memory");
	if (stack < rb->stack)
		rb->stack = stack;

	tail = rb->size - write;
	if (read > write) {
		total = read - 1 - write;
		tail = total;
	} else if (read == write) {
		total = rb->size - 1;
	} else {
		total = rb->size - 1 - write + read;
		if (total < tail)
			tail = total;
	}

	if (size < tail)
		tail = size;

	memcpy((char *)rb->data + write, buf, tail);
	buf += tail;
	write += tail;
	if (write == rb->size)
		write = 0;
	size -= tail;
	total -= tail;
	if (size && total) {
		if (size < total)
			total = size;
		memcpy((char *)rb->data, buf, total);
		write += total;
	} else {
		total = 0;
	}
	rb->write = write;
	return tail + total;
}

static void hang(void) __attribute__((noreturn));
static void hang(void)
{
	for (;;) {
		uint32_t tmp;
		uint32_t interrupt;
		uint32_t ccount;

		asm volatile ("rsil %0, 15" : "=a" (tmp) :: "memory");
		asm volatile ("rsr.interrupt %0" : "=a" (interrupt) :: "memory");
		asm volatile ("rsr.ccount %0" : "=a" (ccount) :: "memory");
		p->interrupt = interrupt;
		p->ccount = ccount;
		p->panic = 0xdeadbabe;
	}
}

void abort(void)
{
	fprintf(stderr, "abort() is called; halting\n");
	hang();
}

static void exception(void)
{
	unsigned long exccause, excvaddr, ps, epc1;

	__asm__ volatile ("rsr %0, exccause\n\t"
			  "rsr %1, excvaddr\n\t"
			  "rsr %2, ps\n\t"
			  "rsr %3, epc1"
			  : "=a"(exccause), "=a"(excvaddr),
			    "=a"(ps), "=a"(epc1));

	fprintf(stderr, "%s: EXCCAUSE = %ld, EXCVADDR = 0x%08lx, PS = 0x%08lx, EPC1 = 0x%08lx\n",
		__func__, exccause, excvaddr, ps, epc1);
	hang();
}

static void register_exception_handlers(void)
{
	static const int cause[] = {
		EXCCAUSE_ILLEGAL,
		EXCCAUSE_INSTR_ERROR,
		EXCCAUSE_LOAD_STORE_ERROR,
		EXCCAUSE_DIVIDE_BY_ZERO,
		EXCCAUSE_PRIVILEGED,
		EXCCAUSE_UNALIGNED,
		EXCCAUSE_INSTR_DATA_ERROR,
		EXCCAUSE_LOAD_STORE_DATA_ERROR,
		EXCCAUSE_INSTR_ADDR_ERROR,
		EXCCAUSE_LOAD_STORE_ADDR_ERROR,
		EXCCAUSE_ITLB_MISS,
		EXCCAUSE_ITLB_MULTIHIT,
		EXCCAUSE_INSTR_RING,
		EXCCAUSE_INSTR_PROHIBITED,
		EXCCAUSE_DTLB_MISS,
		EXCCAUSE_DTLB_MULTIHIT,
		EXCCAUSE_LOAD_STORE_RING,
		EXCCAUSE_LOAD_PROHIBITED,
		EXCCAUSE_STORE_PROHIBITED,
	};
	unsigned i;

	for (i = 0; i < sizeof(cause) / sizeof(cause[0]); ++i) {
		_xtos_set_exception_handler(cause[i], exception);
	}
}

struct ipcm_struct {
	uint32_t source;
	uint32_t dset;
	uint32_t dclear;
	uint32_t dstatus;
	uint32_t mode;
	uint32_t imask;
	uint32_t iclear;
	uint32_t send;
	uint32_t dr[8];
};

struct ipcm_int_struct {
	uint32_t mis;
	uint32_t ris;
};


volatile struct ipcm_struct *ipcm = (volatile struct ipcm_struct *)0xe896b000;
volatile struct ipcm_int_struct *ipcm_int = (volatile struct ipcm_int_struct *)0xe896b800;

void ipcm_send(void)
{
	volatile struct ipcm_struct *mb = ipcm + 2;

	mb->iclear = 0x10;
	if (!(mb->mode & 0x10)) {
	}
	mb->source = 0x10;
	mb->dclear = ~0;
	mb->dset = 0x0;
	mb->imask = ~0x11;
	mb->mode = 0x1;
	mb->dr[0] = 0x1;
	mb->send = 0x10;
}

void ipcm_ack(void)
{
	ipcm[18].iclear = ~ipcm[18].imask & 0x10;
}

int main(void)
{
	enum xrp_status status;
	struct xrp_device *device;
	static cookie_io_functions_t ring_buffer_ops = {
		.write = debug_write,
	};

	p->read = 0;
	p->write = 0;
	p->size = 0x1000 - sizeof(struct ring_buffer);
	p->panic = 0;
	p->interrupt = 0;
	p->ccount = 0;

	stdout = fopencookie((void *)p, "w", ring_buffer_ops);
	stderr = fopencookie((void *)p, "w", ring_buffer_ops);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	register_exception_handlers();
	device = xrp_open_device(0, &status);
	if (status != XRP_STATUS_SUCCESS) {
		printf("xrp_open_device failed\n");
		abort();
	}

	ipcm_ack();

	for (;;) {
		uint32_t interrupt;
		uint32_t ccount;

		asm volatile ("rsr.interrupt %0" : "=a" (interrupt) :: "memory");
		asm volatile ("rsr.ccount %0" : "=a" (ccount) :: "memory");
		p->interrupt = interrupt;
		p->ccount = ccount;
		p->panic = (p->panic + 1) & 0x7fffffff;
		status = xrp_device_dispatch(device);
		if (status == XRP_STATUS_PENDING)
			xrp_hw_wait_device_irq();
	}
	return 0;
}
