/* Copyright (c) 2018, Nuvoton Corporation */
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "jtag.h"

extern void DBG_log(int level, const char *format, ...);

static const struct name_mapping {
    enum tap_state symbol;
    const char *name;
} tap_name_mapping[] = {
    { TAP_RESET, "RESET", },
    { TAP_IDLE, "RUN/IDLE", },
    { TAP_DRSELECT, "DRSELECT", },
    { TAP_DRCAPTURE, "DRCAPTURE", },
    { TAP_DRSHIFT, "DRSHIFT", },
    { TAP_DREXIT1, "DREXIT1", },
    { TAP_DRPAUSE, "DRPAUSE", },
    { TAP_DREXIT2, "DREXIT2", },
    { TAP_DRUPDATE, "DRUPDATE", },
    { TAP_IRSELECT, "IRSELECT", },
    { TAP_IRCAPTURE, "IRCAPTURE", },
    { TAP_IRSHIFT, "IRSHIFT", },
    { TAP_IREXIT1, "IREXIT1", },
    { TAP_IRPAUSE, "IRPAUSE", },
    { TAP_IREXIT2, "IREXIT2", },
    { TAP_IRUPDATE, "IRUPDATE", },

    /* only for input:  accept standard SVF name */
    { TAP_IDLE, "IDLE", },
};

const char *tap_state_name(tap_state_t state)
{
    unsigned i;

    for (i = 0; i < ARRAY_SIZE(tap_name_mapping); i++) {
        if (tap_name_mapping[i].symbol == state)
            return tap_name_mapping[i].name;
    }
    return "???";
}

tap_state_t tap_state_by_name(const char *name)
{
    unsigned i;

    for (i = 0; i < ARRAY_SIZE(tap_name_mapping); i++) {
        /* be nice to the human */
        if (strcasecmp(name, tap_name_mapping[i].name) == 0)
            return tap_name_mapping[i].symbol;
    }
    /* not found */
    return TAP_INVALID;
}

STATUS JTAG_set_clock_frequency(int handle, unsigned int frequency)
{
	unsigned long req = JTAG_SIOCFREQ;

	printf("Set freq: %u\n", frequency);
	if (ioctl(handle, req, &frequency) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_SIOCFREQ failed");
		return ST_ERR;
	}
	return ST_OK;
}

STATUS JTAG_set_mode(int handle, unsigned int Mode)
{
	if (ioctl(handle, JTAG_SIOCMODE, &Mode) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_SIOCMODE failed");
		return ST_ERR;
	}
	return ST_OK;
}

STATUS JTAG_run_state(JTAG_Handler* state, JtagStates tap_state, unsigned int number_of_cycles)
{
	struct jtag_tap_state tapstate;

	if (state == NULL)
		return ST_ERR;

	tapstate.reset = 0;
	tapstate.tck = number_of_cycles;
	tapstate.from = JTAG_STATE_CURRENT;
	tapstate.endstate = tap_state;
	if (ioctl(state->JTAG_driver_handle, JTAG_SIOCSTATE, &tapstate) < 0) {
		perror("run state");
		return ST_ERR;
	}

	return ST_OK;
}

STATUS JTAG_wait_cycles(JTAG_Handler* state, unsigned int number_of_cycles)
{
	if (state == NULL)
		return ST_ERR;

	if (ioctl(state->JTAG_driver_handle, JTAG_RUNTEST, number_of_cycles) < 0) {
		return JTAG_run_state(state, JTAG_STATE_CURRENT, number_of_cycles);
	}

	return ST_OK;
}

//
// Request the TAP to go to the target state
//
STATUS JTAG_set_tap_state(JTAG_Handler* state, JtagStates tap_state)
{
	struct jtag_tap_state tapstate;
	unsigned long req = JTAG_SIOCSTATE;

	if (state == NULL)
		return ST_ERR;

	tapstate.reset = 0;
	tapstate.tck = 0;
	tapstate.from = JTAG_STATE_CURRENT;
	tapstate.endstate = tap_state;

	if (ioctl(state->JTAG_driver_handle, req, &tapstate) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_SIOCSTATE failed");
		perror("set tap state");
		return ST_ERR;
	}

	// move the [soft] state to the requested tap state.
	state->tap_state = tap_state;
#if 0
	if ((tap_state == JtagRTI) || (tap_state == JtagPauDR))
		if (JTAG_wait_cycles(state, 5) != ST_OK)
			return ST_ERR;
#endif
	DBG_log(LEV_DEBUG, "TapState: %d", state->tap_state);
	return ST_OK;
}

STATUS JTAG_shift(JTAG_Handler* state, struct scan_xfer *scan_xfer, unsigned int type)
{
	struct jtag_xfer xfer;
	unsigned char tdio[TDI_DATA_SIZE];
#if UINTPTR_MAX == 0xffffffff
	__u64 ptr = (__u32)tdio;
#else
	__u64 ptr = (__u64)tdio;
#endif
	memset(&xfer, 0, sizeof(xfer));
	xfer.from = JTAG_STATE_CURRENT;
	xfer.endstate = scan_xfer->end_tap_state;
	xfer.length = scan_xfer->length;
	xfer.type = type;
	xfer.direction = JTAG_READ_WRITE_XFER;
	xfer.tdio = ptr;
	memcpy(tdio, scan_xfer->tdi, scan_xfer->tdi_bytes);
	if (ioctl(state->JTAG_driver_handle, JTAG_IOCXFER, &xfer) < 0) {
		perror("jtag shift");
		return ST_ERR;
	}
	memcpy(scan_xfer->tdo, tdio, scan_xfer->tdo_bytes);

	return ST_OK;
}

int JTAG_dr_scan(JTAG_Handler* handler, int num_bits, const uint8_t *out_bits, uint8_t *in_bits,
	tap_state_t state)
{
	struct scan_xfer scan_xfer = {0};
	int remaining_bits = num_bits;
	int n, bits, index = 0;

	memset(scan_xfer.tdi, 0, sizeof(scan_xfer.tdi));
	JTAG_set_tap_state(handler, JtagShfDR);
	while (remaining_bits > 0) {
		n = (remaining_bits / 8) > TDI_DATA_SIZE ? TDI_DATA_SIZE : (remaining_bits + 7) / 8;
		memcpy(scan_xfer.tdi, out_bits + index, n);

		bits = ((n * 8) > remaining_bits)? remaining_bits: (n * 8);
		remaining_bits -= bits;
		scan_xfer.length = bits;
		scan_xfer.tdi_bytes = n;
		scan_xfer.tdo_bytes = n;
		if (remaining_bits > 0)
			scan_xfer.end_tap_state = JtagShfDR;
		else
			scan_xfer.end_tap_state = state;
		if (JTAG_shift(handler, &scan_xfer, JTAG_SDR_XFER) != ST_OK) {
			DBG_log(LEV_ERROR, "ShftDR error");
			return -1;
		}
		if (in_bits)
			memcpy(in_bits+index, scan_xfer.tdo, scan_xfer.tdo_bytes);
		index += n;
	}
	return 0;
}

int JTAG_ir_scan(JTAG_Handler* handler, int num_bits, const uint8_t *out_bits, uint8_t *in_bits,
	tap_state_t state)
{
	struct scan_xfer scan_xfer = {0};
	if (num_bits == 0)
		return -1;
	if ((num_bits + 7) / 8 > TDO_DATA_SIZE) {
		DBG_log(LEV_ERROR, "ir data len too long: %d bits", num_bits);
		return -1;
	}
	JTAG_set_tap_state(handler, JtagShfIR);
	scan_xfer.length = num_bits;
	scan_xfer.tdi_bytes = (num_bits + 7) / 8;
	memcpy(scan_xfer.tdi, out_bits, scan_xfer.tdi_bytes);
	scan_xfer.tdo_bytes = scan_xfer.tdi_bytes;
	scan_xfer.end_tap_state = state;
	if (JTAG_shift(handler, &scan_xfer, JTAG_SIR_XFER) != ST_OK) {
		DBG_log(LEV_ERROR, "ShftIR error");
		return -1;
	}
	if (in_bits)
		memcpy(in_bits, scan_xfer.tdo, scan_xfer.tdo_bytes);

	return 0;
}

