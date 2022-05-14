// Commander X16 Emulator
// Copyright (c) 2021 Michael Steil
// Copyright (c) 2021-2022 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#include "i2c.h"
#include "rtc.h"
#include "smc.h"
#include <stdbool.h>
#include <stdio.h>

#define LOG_LEVEL 0
#define LOG_PRINTF(LEVEL, ...)          \
	if constexpr (LOG_LEVEL >= LEVEL) { \
		printf(__VA_ARGS__);            \
	}

#define DEVICE_SMC 0x42
#define DEVICE_RTC 0x6F

#define STATE_START 0
#define STATE_STOP -1

i2c_port_t i2c_port;

static int     state     = STATE_STOP;
static bool    read_mode = false;
static uint8_t value     = 0;
static int     count     = 0;
static uint8_t device;
static uint8_t offset;

uint8_t i2c_read(uint8_t device, uint8_t offset)
{
	uint8_t value;
	switch (device) {
		case DEVICE_SMC:
			return smc_read(offset);
		case DEVICE_RTC:
			return rtc_read(offset);
		default:
			value = 0xff;
	}
	LOG_PRINTF(1, "I2C READ($%02X:$%02X) = $%02X\n", device, offset, value);
	return value;
}

void i2c_write(uint8_t device, uint8_t offset, uint8_t value)
{
	switch (device) {
		case DEVICE_SMC:
			smc_write(offset, value);
			break;
		case DEVICE_RTC:
			rtc_write(offset, value);
			break;
			//        default:
			// no-op
	}
	LOG_PRINTF(1, "I2C WRITE $%02X:$%02X, $%02X\n", device, offset, value);
}

void i2c_step()
{
	static i2c_port_t old_i2c_port;
	if (old_i2c_port.clk_in != i2c_port.clk_in || old_i2c_port.data_in != i2c_port.data_in) {
		LOG_PRINTF(5, "I2C(%d) C:%d D:%d\n", state, i2c_port.clk_in, i2c_port.data_in);
		if (state == STATE_STOP && i2c_port.clk_in == 0 && i2c_port.data_in == 0) {
			LOG_PRINTF(2, "I2C START\n");
			state = STATE_START;
		}
		if (state == 1 && i2c_port.clk_in == 1 && i2c_port.data_in == 1 && old_i2c_port.data_in == 0) {
			LOG_PRINTF(2, "I2C STOP\n");
			state     = STATE_STOP;
			count     = 0;
			read_mode = false;
		}
		if (state != STATE_STOP && i2c_port.clk_in == 1 && old_i2c_port.clk_in == 0) {
			i2c_port.data_out = I2C_DATA_MASK;
			if (state < 8) {
				if (read_mode) {
					if (state == 0) {
						value = i2c_read(device, offset);
					}
					i2c_port.data_out = (value & 0x80) >> 5;
					value <<= 1;
					LOG_PRINTF(4, "I2C OUT#%d: %d\n", state, i2c_port.data_out);
					state++;
				} else {
					LOG_PRINTF(4, "I2C BIT#%d: %d\n", state, i2c_port.data_in);
					value <<= 1;
					value |= i2c_port.data_in;
					state++;
				}
			} else { // state == 8
				if (read_mode) {
					bool nack = i2c_port.data_in;
					if (nack) {
						LOG_PRINTF(3, "I2C OUT DONE (NACK)\n");
						count     = 0;
						read_mode = false;
					} else {
						LOG_PRINTF(3, "I2C OUT DONE (ACK)\n");
						offset++;
					}
				} else {
					bool ack = true;
					switch (count) {
						case 0:
							device    = value >> 1;
							read_mode = value & 1;
							if (device != DEVICE_SMC && device != DEVICE_RTC) {
								ack = false;
							}
							break;
						case 1:
							offset = value;
							break;
						default:
							i2c_write(device, offset, value);
							offset++;
							break;
					}
					if (ack) {
						LOG_PRINTF(3, "I2C ACK(%d) $%02X\n", count, value);
						i2c_port.data_out = 0;
						count++;
					} else {
						LOG_PRINTF(3, "I2C NACK(%d) $%02X\n", count, value);
						count     = 0;
						read_mode = false;
					}
				}
				state = STATE_START;
			}
		}
		old_i2c_port = i2c_port;
	}
}
