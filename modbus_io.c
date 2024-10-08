/*

modbus_io.c - plugin for for MODBUS I/O extension of grblHAL

Copyright (c) 2024 Richard Toth

This plugin is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This plugin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this plugin.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "driver.h"

#if MBIO_ENABLE

#include "modbus_io.h"
#include "grbl/config.h"
#include "grbl/hal.h"
#include "grbl/gcode.h"
#include "grbl/modbus.h"
#include "grbl/protocol.h"
#include "grbl/state_machine.h"
#include "grbl/report.h"
#ifdef MBIO_DEBUG
    #include <stdio.h>
#endif

static user_mcode_ptrs_t user_mcode;
static on_report_options_ptr on_report_options;
static void mbio_rx_packet (modbus_message_t *msg);
static void mbio_rx_exception (uint8_t code, void *context);

static const modbus_callbacks_t callbacks = {
    .on_rx_packet = mbio_rx_packet,
    .on_rx_exception = mbio_rx_exception
};

static void mbio_raise_alarm (void *data) {
    system_raise_alarm(Status_ExpressionInvalidResult); // TODO implement own error code?
}

bool mbio_failed(void) {
    bool ok = true;

    if (sys.cold_start) {
        protocol_enqueue_foreground_task(mbio_raise_alarm, NULL);
    }
    else {
        system_raise_alarm(Status_ExpressionInvalidResult); // TODO implement own error code?
        protocol_enqueue_foreground_task(report_warning, "What's this?");
    }

    return ok;
}

static void mbio_rx_exception(uint8_t code, void *context) {
    // Alarm needs to be raised directly to correctly handle an error during reset (the rt command queue is
    // emptied on a warm reset). Exception is during cold start, where alarms need to be queued.
    mbio_failed();
}

static void mbio_report_options(bool newopt) {
    on_report_options(newopt);

    if (newopt) {
        hal.stream.write(",MBIO");
    }
    else {
        hal.stream.write("[PLUGIN:MODBUS IO v0.1]" ASCII_EOL);
    }
}

void mbio_modbus_send_command(modbus_message_t _cmd, bool block) {
#ifdef MBIO_DEBUG
    char buf[30];
    sprintf(buf, "MODBUS TX: %02X %02X %02X %02X %02X %02X", _cmd.adu[0], _cmd.adu[1], _cmd.adu[2], _cmd.adu[3], _cmd.adu[4], _cmd.adu[5]);
    report_message(buf, Message_Plain);
#endif

    modbus_send(&_cmd, &callbacks, true);
}

void mbio_ModBus_ReadCoils(char device_address, uint16_t register_address, uint16_t value) {
    modbus_message_t _cmd = {
        .context = (void *)MBIO_Command,
        .crc_check = true,
        .adu[0] = device_address, // slave device address
        .adu[1] = ModBus_ReadCoils, // function
        .adu[2] = MODBUS_SET_MSB16(register_address), // register address
        .adu[3] = MODBUS_SET_LSB16(register_address),
        .adu[4] = MODBUS_SET_MSB16(value), // value
        .adu[5] = MODBUS_SET_LSB16(value),
        .tx_length = 8,
        .rx_length = 6
    };
    mbio_modbus_send_command(_cmd, true);
}

void mbio_ModBus_WriteCoil(char device_address, uint16_t register_address, uint16_t value) {
    modbus_message_t _cmd = {
        .context = (void *)MBIO_Command,
        .crc_check = true,
        .adu[0] = device_address, // slave device address
        .adu[1] = ModBus_WriteCoil, // function
        .adu[2] = MODBUS_SET_MSB16(register_address), // register address
        .adu[3] = MODBUS_SET_LSB16(register_address),
        .adu[4] = MODBUS_SET_MSB16(value), // value
        .adu[5] = MODBUS_SET_LSB16(value),
        .tx_length = 8,
        .rx_length = 8
    };
    mbio_modbus_send_command(_cmd, true);
}

void mbio_ModBus_ReadDiscreteInputs(char device_address, uint16_t register_address, uint16_t value) {
    modbus_message_t _cmd = {
        .context = (void *)MBIO_Command,
        .crc_check = true,
        .adu[0] = device_address, // slave device address
        .adu[1] = ModBus_ReadDiscreteInputs, // function
        .adu[2] = MODBUS_SET_MSB16(register_address), // register address
        .adu[3] = MODBUS_SET_LSB16(register_address),
        .adu[4] = MODBUS_SET_MSB16(value), // number of registers to read
        .adu[5] = MODBUS_SET_LSB16(value),
        .tx_length = 8,
        .rx_length = 6
    };
    mbio_modbus_send_command(_cmd, true);
}

void mbio_ModBus_ReadHoldingRegisters(char device_address, uint16_t register_address) {
    modbus_message_t _cmd = {
        .context = (void *)MBIO_Command,
        .crc_check = true,
        .adu[0] = device_address, // slave device address
        .adu[1] = ModBus_ReadHoldingRegisters, // function
        .adu[2] = MODBUS_SET_MSB16(register_address), // register address
        .adu[3] = MODBUS_SET_LSB16(register_address),
        .adu[4] = 0x00, // number of registers to read
        .adu[5] = 0x01,
        .tx_length = 8,
        .rx_length = 7
    };
    mbio_modbus_send_command(_cmd, true);
}

void mbio_ModBus_ReadInputRegisters(char device_address, uint16_t register_address, uint16_t value) {
    modbus_message_t _cmd = {
        .context = (void *)MBIO_Command,
        .crc_check = true,
        .adu[0] = device_address, // slave device address
        .adu[1] = ModBus_ReadInputRegisters, // function
        .adu[2] = MODBUS_SET_MSB16(register_address), // register address
        .adu[3] = MODBUS_SET_LSB16(register_address),
        .adu[4] = MODBUS_SET_MSB16(value), // number of registers to read
        .adu[5] = MODBUS_SET_LSB16(value),
        .tx_length = 8,
        .rx_length = 7
    };
    mbio_modbus_send_command(_cmd, true);
}

void mbio_ModBus_WriteRegister(char device_address, uint16_t register_address, uint16_t value) {
    modbus_message_t _cmd = {
        .context = (void *)MBIO_Command,
        .crc_check = true,
        .adu[0] = device_address, // slave device address
        .adu[1] = ModBus_WriteRegister, // function
        .adu[2] = MODBUS_SET_MSB16(register_address), // register address
        .adu[3] = MODBUS_SET_LSB16(register_address),
        .adu[4] = MODBUS_SET_MSB16(value), // number of registers to read
        .adu[5] = MODBUS_SET_LSB16(value),
        .tx_length = 8,
        .rx_length = 8
    };
    mbio_modbus_send_command(_cmd, true);
}

int32_t mbio_Wait_ReadDiscreteInputs(char device_address, uint16_t register_address, int32_t value, float timeout) {
    int32_t ret = -1;
    uint_fast16_t delay = (uint_fast16_t)ceilf((1000.0f / MBIO_WAIT_STEP) * timeout) + 1;

    do {
        mbio_ModBus_ReadDiscreteInputs(device_address, register_address, 1);
        if (sys.var5399 == value) {
            ret = value;
            break;
        }
        
        if (delay) {
            protocol_execute_realtime();
            hal.delay_ms(50, NULL);
        } 
        else {
            break;
        }
    } while(--delay && !sys.abort);

#ifdef MBIO_DEBUG
    char buf[40];
    sprintf(buf, "MODBUS WAIT VAL: %d, expected %d, rt %.2f s", ret, value, delay * MBIO_WAIT_STEP / 1000.0f);
    report_message(buf, Message_Plain);

#endif

    return ret;
}


// Check if M-code is handled here.
// parameters: mcode - M-code to check for (some are predefined in user_mcode_t in grbl/gcode.h), use a cast if not.
// returns:    mcode if handled, UserMCode_Ignore otherwise (UserMCode_Ignore is defined in grbl/gcode.h).
static user_mcode_t mbio_check(user_mcode_t mcode) {
    return mcode == UserMCode_Generic1 || mcode == UserMCode_Generic2
                     ? mcode
                     : (user_mcode.check ? user_mcode.check(mcode) : UserMCode_Ignore);
}

// Validate M-code parameters
// parameters: gc_block - pointer to parser_block_t struct (defined in grbl/gcode.h).
//             deprecated - ?
// returns:    status_code_t enum (defined in grbl/gcode.h): Status_OK if validated ok, appropriate status from enum if not.
static status_code_t mbio_validate(parser_block_t *gc_block, parameter_words_t *deprecated) {
	status_code_t state = Status_GcodeValueWordMissing;

    switch (gc_block->user_mcode) {

        // M101 D{0..247} E{1,2,3,4,5,6} P{1..9999} [Q{0..65535}]
        case UserMCode_Generic1:
            // device address D[0..247]: required
            if (!gc_block->words.d || !isintf(gc_block->values.d)) { // Check if D parameter value is supplied.
                state = Status_BadNumberFormat;
            }

            // function code E[2,4,5,6]: required
            if (!gc_block->words.e || !isintf(gc_block->values.e)) { // Check if E parameter value is supplied.
                state = Status_BadNumberFormat;
            }

            // register address P[1..9999]: required
            if (!gc_block->words.p || !isintf(gc_block->values.p)) {// Check if P parameter value is supplied.
                state = Status_BadNumberFormat;
            }

            // value Q[0.0 .. 65535.0]: optional, some functions (2,4) have fixed value
            // TODO add check for required Q on certain E!
            if (gc_block->words.q && !isintf(gc_block->values.q)) { // Check if Q parameter value is supplied.
                state = Status_BadNumberFormat;
            }

            // value
            if (state != Status_BadNumberFormat) { // Are required parameters provided?
                // briefly check ranges
                if (gc_block->values.d < 0.0f || gc_block->values.d > 247.0f
                    ||
                    (gc_block->values.e != (float)ModBus_ReadDiscreteInputs && gc_block->values.e != (float)ModBus_ReadInputRegisters 
                        && gc_block->values.e != (float)ModBus_WriteCoil && gc_block->values.e != (float)ModBus_WriteRegister
                        && gc_block->values.e != (float)ModBus_ReadHoldingRegisters && gc_block->values.e != (float)ModBus_ReadCoils)
                    ||
                    gc_block->values.p < 1.0f || gc_block->values.p > 9999.0f
                    ||
                    gc_block->values.q < 0.0f || gc_block->values.q > 65535.0f) {
                	
                    state = Status_GcodeValueOutOfRange;                    
                }
                else {
                    switch ((char)gc_block->values.e) {
                        case ModBus_ReadDiscreteInputs:
                        case ModBus_ReadInputRegisters:
                            gc_block->values.q = 1.0f;    
                            break;
                        case ModBus_WriteCoil:
                            break;
                        case ModBus_WriteRegister:
                            break;
                    }
                    
                	state = Status_OK;
                }
                    
                gc_block->words.d = gc_block->words.e = gc_block->words.p = gc_block->words.q = Off; // Claim parameters.
                //gc_block->user_mcode_sync = true;                           // Optional: execute command synchronized
            }
            break;

        // M102 D{0..247} P{1..9999} Q{0,1} R{0..3600}
        case UserMCode_Generic2: 
            // device address D[0..247]: required
            if (!gc_block->words.d || !isintf(gc_block->values.d)) {
                state = Status_BadNumberFormat;
            }

            // register address P[1..9999]: required
            if (!gc_block->words.p || !isintf(gc_block->values.p)) {
                state = Status_BadNumberFormat;
            }

            // value Q[0,1]: required
            if (!gc_block->words.q || !isintf(gc_block->values.q)) {
                state = Status_BadNumberFormat;
            }

            // value R[0..3600]: required
            if (!gc_block->words.r || isnanf(gc_block->values.r)) {
                state = Status_BadNumberFormat;
            }

            // value
            if (state != Status_BadNumberFormat) { // Are required parameters provided?
                // briefly check ranges
                if (gc_block->values.d < 0.0f || gc_block->values.d > 247.0f
                    ||
                    gc_block->values.p < 1.0f || gc_block->values.p > 9999.0f
                    ||
                    gc_block->values.q < 0.0f || gc_block->values.q > 1.0f
                    ||
                    gc_block->values.r < 0.0f || gc_block->values.r > 3600.0f) {
                    
                    state = Status_GcodeValueOutOfRange;                    
                }
                else {
                    state = Status_OK;
                }
                    
                gc_block->words.d = gc_block->words.p = gc_block->words.q = gc_block->words.r = Off; // Claim parameters.
            }
            break;            

        default:
            state = Status_Unhandled;
            break;
    }

    // If not handled by us and another handler present then call it.
    return state == Status_Unhandled && user_mcode.validate ? user_mcode.validate(gc_block, deprecated) : state;
}


// Execute M-code
// parameters: state - sys.state (bitmap, defined in system.h)
//             gc_block - pointer to parser_block_t struct (defined in grbl/gcode.h).
// returns:    -
static void mbio_execute(sys_state_t state, parser_block_t *gc_block) {
    bool handled = true;
    char device_address = (char)gc_block->values.d;
    uint16_t register_address = (uint16_t)gc_block->values.p - 1;

    switch(gc_block->user_mcode) {
        case UserMCode_Generic1:
            uint16_t value = (uint16_t)gc_block->values.q;
            if ((char)gc_block->values.e == 5) {
                value = value > 0 ? 0xff00 : 0;
            }

            switch ((char)gc_block->values.e) {
                case ModBus_ReadCoils: // 1
                    mbio_ModBus_ReadCoils(device_address, register_address, value);
                    break;

                case ModBus_ReadDiscreteInputs: // 2
                    mbio_ModBus_ReadDiscreteInputs(device_address, register_address, 1);
                    break;

                case ModBus_ReadInputRegisters: // 4
                    mbio_ModBus_ReadInputRegisters(device_address, register_address, 1);
                    break;

                case ModBus_ReadHoldingRegisters: // 3
                    mbio_ModBus_ReadHoldingRegisters(device_address, register_address);
                    break;

                case ModBus_WriteCoil: // 5
                    mbio_ModBus_WriteCoil(device_address, register_address, value);
                    break;

                case ModBus_WriteRegister: // 6
                    mbio_ModBus_WriteRegister(device_address, register_address, value);
                    break;
            }
            break;

        case UserMCode_Generic2: 
            int32_t ret = mbio_Wait_ReadDiscreteInputs(device_address, register_address, (int32_t)gc_block->values.q, gc_block->values.r);
            if (ret < 0) {
                system_raise_alarm(Status_GCodeTimeout);
            }
            break;

        default:
            handled = false;
            break;
    }

    // If not handled by us and another handler present, call it.
    if (!handled && user_mcode.execute) {
        user_mcode.execute(state, gc_block);
    }
}

static void mbio_rx_packet (modbus_message_t *msg) {
    if (!(msg->adu[0] & 0x80)) {
        switch((mbio_response_t)msg->context) {
            case MBIO_Command:
                // rewrite in opposite way - use context to distinguish between commands and then check if the response corresponds to command sent!

                // process the responses (put red value into the system.var5399)
                switch (msg->adu[1]) {
                    case ModBus_ReadDiscreteInputs:
                        sys.var5399 = msg->adu[3] & 0x01;
                        break;

                    case ModBus_ReadCoils:
                        sys.var5399 = (int32_t)msg->adu[3];
                        break;

                    case ModBus_ReadInputRegisters:
                    case ModBus_ReadHoldingRegisters:
                        sys.var5399 = (int32_t)modbus_read_u16(&msg->adu[3]);
                        break;
                }

#ifdef MBIO_DEBUG
                char buf[30];
                sprintf(buf, "MODBUS RX: %02X %02X %02X %02X %02X %02X %02X %02X", msg->adu[0], msg->adu[1], msg->adu[2], msg->adu[3], msg->adu[4], msg->adu[5], msg->adu[6], msg->adu[7]);
                report_message(buf, Message_Plain);

                switch (msg->adu[1]) {
                    case ModBus_ReadDiscreteInputs:
                        if (msg->adu[3] & 0x01 == 0x01) {
                            sprintf(buf, "MODBUS RESPONSE: on (0x%02X)", msg->adu[3]);
                            report_message(buf, Message_Plain);
                        }
                        else if (msg->adu[3] & 0x01 == 0x00) {
                            sprintf(buf, "MODBUS RESPONSE: off (0x%02X)", msg->adu[3]);
                            report_message(buf, Message_Plain);
                        }
                        break;

                    case ModBus_ReadCoils:
                        sprintf(buf, "MODBUS RESPONSE: %d (0x%02X)", msg->adu[3], msg->adu[3]);
                        report_message(buf, Message_Plain);
                        break;

                    case ModBus_ReadInputRegisters:
                    case ModBus_ReadHoldingRegisters:
                        uint16_t value = modbus_read_u16(&msg->adu[3]);
                        sprintf(buf, "MODBUS RESPONSE: %u (0x%04X)", value, value);
                        report_message(buf, Message_Plain);
                        break;

                    case ModBus_WriteCoil:
                        report_message("MODBUS RESPONSE: OK", Message_Plain);
                        break;

                    case ModBus_WriteRegister:
                        report_message("MODBUS RESPONSE: OK", Message_Plain);
                        break;
                }
#endif
                break;

            default:
                break;
        }
        
    }
    else {
        report_message("MODBUS ERROR", Message_Warning);
    }
}



void mbio_init(void) {
	hal.user_mcode.check = mbio_check;
    hal.user_mcode.validate = mbio_validate;
    hal.user_mcode.execute = mbio_execute;

	on_report_options = grbl.on_report_options;
    grbl.on_report_options = mbio_report_options;
}

#endif