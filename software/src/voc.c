/* air-quality-bricklet
 * Copyright (C) 2017-2019 Olaf Lüke <olaf@tinkerforge.com>
 *
 * voc.c: Air Quality Bricklet read/write state machine
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "voc.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "xmc_spi.h"

#include "bricklib2/hal/ccu4_timer/ccu4_timer.h"
#include "bricklib2/hal/system_timer/system_timer.h"
#include "bricklib2/os/coop_task.h"
#include "bricklib2/logging/logging.h"
#include "bricklib2/bootloader/bootloader.h"
#include "bricklib2/utility/util_definitions.h"

#include "configs/config_bme680.h"

#include "bsec_interface.h"
#include "bsec_datatypes.h"
#include "communication.h"

// Config for 3.3V, 3 seconds, 4 days
const uint8_t voc_config_iaq_33v_3s_4d[BSEC_MAX_PROPERTY_BLOB_SIZE]  = {4,7,4,1,61,0,0,0,0,0,0,0,174,1,0,0,48,0,1,0,0,192,168,71,64,49,119,76,0,0,225,68,137,65,0,63,205,204,204,62,0,0,64,63,205,204,204,62,0,0,0,0,216,85,0,100,0,0,0,0,0,0,0,0,28,0,2,0,0,244,1,225,0,25,0,0,128,64,0,0,32,65,144,1,0,0,112,65,0,0,0,63,16,0,3,0,10,215,163,60,10,215,35,59,10,215,35,59,9,0,5,0,0,0,0,0,1,88,0,9,0,229,208,34,62,0,0,0,0,0,0,0,0,218,27,156,62,225,11,67,64,0,0,160,64,0,0,0,0,0,0,0,0,94,75,72,189,93,254,159,64,66,62,160,191,0,0,0,0,0,0,0,0,33,31,180,190,138,176,97,64,65,241,99,190,0,0,0,0,0,0,0,0,167,121,71,61,165,189,41,192,184,30,189,64,12,0,10,0,0,0,0,0,0,0,0,0,229,0,254,0,2,1,5,48,117,100,0,44,1,112,23,151,7,132,3,197,0,92,4,144,1,64,1,64,1,144,1,48,117,48,117,48,117,48,117,100,0,100,0,100,0,48,117,48,117,48,117,100,0,100,0,48,117,48,117,100,0,100,0,100,0,100,0,48,117,48,117,48,117,100,0,100,0,100,0,48,117,48,117,100,0,100,0,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,255,255,255,255,255,255,255,255,220,5,220,5,220,5,255,255,255,255,255,255,220,5,220,5,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,44,1,0,0,0,0,138,80,0,0};

// Config for 3.3V, 3 seconds, 28 days
const uint8_t voc_config_iaq_33v_3s_28d[BSEC_MAX_PROPERTY_BLOB_SIZE] = {4,7,4,1,61,0,0,0,0,0,0,0,174,1,0,0,48,0,1,0,0,168,19,73,64,49,119,76,0,0,225,68,137,65,0,63,205,204,204,62,0,0,64,63,205,204,204,62,0,0,0,0,216,85,0,100,0,0,0,0,0,0,0,0,28,0,2,0,0,244,1,225,0,25,0,0,128,64,0,0,32,65,144,1,0,0,112,65,0,0,0,63,16,0,3,0,10,215,163,60,10,215,35,59,10,215,35,59,9,0,5,0,0,0,0,0,1,88,0,9,0,229,208,34,62,0,0,0,0,0,0,0,0,218,27,156,62,225,11,67,64,0,0,160,64,0,0,0,0,0,0,0,0,94,75,72,189,93,254,159,64,66,62,160,191,0,0,0,0,0,0,0,0,33,31,180,190,138,176,97,64,65,241,99,190,0,0,0,0,0,0,0,0,167,121,71,61,165,189,41,192,184,30,189,64,12,0,10,0,0,0,0,0,0,0,0,0,229,0,254,0,2,1,5,48,117,100,0,44,1,112,23,151,7,132,3,197,0,92,4,144,1,64,1,64,1,144,1,48,117,48,117,48,117,48,117,100,0,100,0,100,0,48,117,48,117,48,117,100,0,100,0,48,117,48,117,100,0,100,0,100,0,100,0,48,117,48,117,48,117,100,0,100,0,100,0,48,117,48,117,100,0,100,0,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,8,7,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,112,23,255,255,255,255,255,255,255,255,220,5,220,5,220,5,255,255,255,255,255,255,220,5,220,5,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,44,1,0,0,0,0,169,55,0,0};

VOC voc;
CoopTask voc_task;

#define voc_rx_irq_handler IRQ_Hdlr_11
#define voc_tx_irq_handler IRQ_Hdlr_12

void __attribute__((optimize("-O3"))) voc_rx_irq_handler(void) {
	while(!XMC_USIC_CH_RXFIFO_IsEmpty(BME680_USIC)) {
		voc.data_spi[voc.data_read_index] = BME680_USIC->OUTR;
		voc.data_read_index++;
	}
}

void __attribute__((optimize("-O3"))) voc_tx_irq_handler(void) {
	while(!XMC_USIC_CH_TXFIFO_IsFull(BME680_USIC)) {
		BME680_USIC->IN[0] = voc.data_spi[voc.data_write_index];
		voc.data_write_index++;
		if(voc.data_write_index >= voc.data_length) {
			XMC_USIC_CH_TXFIFO_DisableEvent(BME680_USIC, XMC_USIC_CH_TXFIFO_EVENT_CONF_STANDARD);
			return;
		}
	}
}

void voc_task_spi_transceive(void) {
	XMC_SPI_CH_EnableSlaveSelect(BME680_USIC, XMC_SPI_CH_SLAVE_SELECT_0);
	XMC_USIC_CH_TXFIFO_EnableEvent(BME680_USIC, XMC_USIC_CH_TXFIFO_EVENT_CONF_STANDARD);
	XMC_USIC_CH_TriggerServiceRequest(BME680_USIC, BME680_SERVICE_REQUEST_TX);

	while(voc.data_read_index < voc.data_length) {
		// Turn rx event off before we manually look at rx fifo to be sure that there can't be any race condition
		XMC_USIC_CH_RXFIFO_DisableEvent(BME680_USIC, XMC_USIC_CH_RXFIFO_EVENT_CONF_STANDARD | XMC_USIC_CH_RXFIFO_EVENT_CONF_ALTERNATE);
		voc_rx_irq_handler();
		XMC_USIC_CH_RXFIFO_EnableEvent(BME680_USIC, XMC_USIC_CH_RXFIFO_EVENT_CONF_STANDARD | XMC_USIC_CH_RXFIFO_EVENT_CONF_ALTERNATE);
		coop_task_yield();
	}

	XMC_SPI_CH_DisableSlaveSelect(BME680_USIC);
}

int8_t voc_task_spi_read(uint8_t dev_id, uint8_t reg, uint8_t *data, uint16_t length) {
	voc.data_read_index  = 0;
	voc.data_write_index = 0;
	voc.data_spi[0] = reg | REG_READ;
	for(uint16_t i = 0; i < length; i++) {
		voc.data_spi[i+1] = reg + 1 + i;
	}

	voc.data_length = length + 1;

	voc_task_spi_transceive();

	memcpy(data, &voc.data_spi[1], length);

	return 0;
}

int8_t voc_task_spi_write(uint8_t dev_id, uint8_t reg, uint8_t *data, uint16_t length) {
	voc.data_read_index  = 0;
	voc.data_write_index = 0;
	voc.data_spi[0] = reg;
	memcpy(&voc.data_spi[1], data, length);

	voc.data_length = length + 1;

	voc_task_spi_transceive();

	return 0;
}

void voc_task_update_config(void) {
	bme680_set_sensor_settings(BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_FILTER_SEL | BME680_GAS_SENSOR_SEL, &voc.bme680);
}

void voc_task_bsec_trigger_measurement(bsec_bme_settings_t *sensor_settings) {
	// Check if a forced-mode measurement should be triggered now
	if(sensor_settings->trigger_measurement) {
		// Set sensor configuration
		voc.bme680.tph_sett.os_hum     = sensor_settings->humidity_oversampling;
		voc.bme680.tph_sett.os_pres	   = sensor_settings->pressure_oversampling;
		voc.bme680.tph_sett.os_temp	   = sensor_settings->temperature_oversampling;
		voc.bme680.gas_sett.run_gas	   = sensor_settings->run_gas;
		voc.bme680.gas_sett.heatr_temp = sensor_settings->heater_temperature;
		voc.bme680.gas_sett.heatr_dur  = sensor_settings->heating_duration;
		voc.bme680.power_mode          = BME680_FORCED_MODE;
		bme680_set_sensor_settings(BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_GAS_SENSOR_SEL, &voc.bme680);

		// Set power mode as forced mode and trigger forced mode measurement
		bme680_set_sensor_mode(&voc.bme680);

		// Delay until the measurement is ready.
		uint16_t period;
		bme680_get_profile_dur(&period, &voc.bme680);
		coop_task_sleep_ms(period);
	}

	bme680_get_sensor_mode(&voc.bme680);

	// When the measurement is completed and data is ready for reading, the sensor must be in BME680_SLEEP_MODE.
	// Read operation mode to check whether measurement is completely done and wait until the sensor is no more
	// in BME680_FORCED_MODE.
	while(voc.bme680.power_mode == BME680_FORCED_MODE) {
		coop_task_sleep_ms(5);
		bme680_get_sensor_mode(&voc.bme680);
	}
}

int8_t voc_task_bsec_read_data(int64_t time_stamp_trigger, bsec_input_t *inputs, uint8_t *num_bsec_inputs, int32_t bsec_process_data) {
	static struct bme680_field_data data;
	static float last_temperature = -10000;
	static float last_pressure    = -10000;
	static float last_humidity    = -10000;
	static uint8_t return_counter = 0;

	// We only have to read data if the previous call the bsec_sensor_control() asked for it
	if(bsec_process_data) {
		int8_t result = bme680_get_sensor_data(&data, &voc.bme680);
		if(result != BME680_OK) {
			logd("Unexpected bme680_get_sensor_data result: %d\n\r", result);
			return result;
		}

		// Disallow unrealistic temperature/humidity/pressure changes
		// But if the peak stays for more then 5 cycles we accept it.
		if(return_counter < 5) {
			if(last_temperature != -10000) {
				if(ABS(last_temperature - data.temperature) > 3) { // °C
					return_counter++;
					return -1;
				}
			}

			if(last_pressure != -10000) {
				if(ABS(last_pressure - data.pressure) > 10000) { // Pa
					return_counter++;
					return -1;
				}
			}

			if(last_humidity != -10000) {
				if(ABS(last_pressure - data.pressure) > 10) { // %RH
					return_counter++;
					return -1;
				}
			}
		}

		last_temperature = data.temperature;
		last_pressure    = data.pressure;
		last_humidity    = data.humidity;
		return_counter   = 0;

		if(data.status & BME680_NEW_DATA_MSK) {
			// Pressure to be processed by BSEC
			if(bsec_process_data & BSEC_PROCESS_PRESSURE) {
				// Place presssure sample into input struct
				inputs[*num_bsec_inputs].sensor_id = BSEC_INPUT_PRESSURE;
				inputs[*num_bsec_inputs].signal = data.pressure;
				inputs[*num_bsec_inputs].time_stamp = time_stamp_trigger;
				(*num_bsec_inputs)++;
			}

			// Temperature to be processed by BSEC
			if(bsec_process_data & BSEC_PROCESS_TEMPERATURE) {
				// Place temperature sample into input struct
				inputs[*num_bsec_inputs].sensor_id = BSEC_INPUT_TEMPERATURE;
				inputs[*num_bsec_inputs].signal = data.temperature;
				inputs[*num_bsec_inputs].time_stamp = time_stamp_trigger;
				(*num_bsec_inputs)++;

				// Add optional heatsource input which will be subtracted from the temperature reading to
				// compensate for device-specific self-heating (supported in BSEC IAQ solution)

				// In our tests we found that the XMC1400 adds about 0.5°C above ambient to the
				// temperature of the circuit board (tested in room temperature conditions).
				//
				// Additionally we add the user defined offset. This is useful if the Bricklet
				// is mounted in an enclosure with additional heat sources (for example).
				inputs[*num_bsec_inputs].sensor_id = BSEC_INPUT_HEATSOURCE;
				inputs[*num_bsec_inputs].signal = 0.5 + voc.temperature_offset / 100.0f;
				inputs[*num_bsec_inputs].time_stamp = time_stamp_trigger;
				(*num_bsec_inputs)++;
			}

			// Humidity to be processed by BSEC
			if(bsec_process_data & BSEC_PROCESS_HUMIDITY) {
				// Place humidity sample into input struct
				inputs[*num_bsec_inputs].sensor_id = BSEC_INPUT_HUMIDITY;
				inputs[*num_bsec_inputs].signal = data.humidity;
				inputs[*num_bsec_inputs].time_stamp = time_stamp_trigger;
				(*num_bsec_inputs)++;
			}

			// Gas to be processed by BSEC
			if(bsec_process_data & BSEC_PROCESS_GAS) {
				// Check whether gas_valid flag is set
				if(data.status & BME680_GASM_VALID_MSK) {
					// Place sample into input struct
					inputs[*num_bsec_inputs].sensor_id = BSEC_INPUT_GASRESISTOR;
					inputs[*num_bsec_inputs].signal = data.gas_resistance;
					inputs[*num_bsec_inputs].time_stamp = time_stamp_trigger;
					(*num_bsec_inputs)++;
				}
			}
		}
	}

	return BME680_OK;
}

bsec_library_return_t voc_task_bsec_process_data(bsec_input_t *bsec_inputs, uint8_t num_bsec_inputs) {
	// Output buffer set to the maximum virtual sensor outputs supported
	bsec_output_t bsec_outputs[BSEC_NUMBER_OUTPUTS];
	uint8_t num_bsec_outputs = 0;
	uint8_t index = 0;

	// Check if something should be processed by BSEC
	if(num_bsec_inputs > 0) {
		// Set number of outputs to the size of the allocated buffer
		// BSEC_NUMBER_OUTPUTS to be defined
		num_bsec_outputs = BSEC_NUMBER_OUTPUTS;

		// Perform processing of the data by BSEC
		//   Note:
		//   * The number of outputs you get depends on what you asked for during bsec_update_subscription(). This is
		//	 handled under bme680_bsec_update_subscription() function in this example file.
		//   * The number of actual outputs that are returned is written to num_bsec_outputs.
		bsec_library_return_t status = bsec_do_steps(bsec_inputs, num_bsec_inputs, bsec_outputs, &num_bsec_outputs);
		if(status != BSEC_OK) {
			logd("Unexpected bsec_do_steps status: %d\n\r", status);
			return status;
		}

		// Iterate through the outputs and extract the relevant ones.
		for(index = 0; index < num_bsec_outputs; index++) {
			switch(bsec_outputs[index].sensor_id) {
				case BSEC_OUTPUT_IAQ:
					voc.iaq_index = (int32_t)bsec_outputs[index].signal;
					voc.iaq_accuracy = bsec_outputs[index].accuracy;
					break;
				case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
					voc.compensated_temperature = (int32_t)(bsec_outputs[index].signal*100);
					break;
				case BSEC_OUTPUT_RAW_PRESSURE:
					voc.raw_air_pressure = (int32_t)bsec_outputs[index].signal;
					break;
				case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
					voc.compensated_humidity = (int32_t)(bsec_outputs[index].signal*100);
					break;
				case BSEC_OUTPUT_RAW_GAS:
					voc.raw_gas = (int32_t)bsec_outputs[index].signal;
					break;
				case BSEC_OUTPUT_RAW_TEMPERATURE:
					voc.raw_temperature = (int32_t)(bsec_outputs[index].signal*100);
					break;
				case BSEC_OUTPUT_RAW_HUMIDITY:
					voc.raw_humidity = (int32_t)(bsec_outputs[index].signal*100);
					break;
				case BSEC_OUTPUT_STABILIZATION_STATUS:
					voc.stabilization_status = (uint8_t)bsec_outputs[index].signal;
					break;
				case BSEC_OUTPUT_RUN_IN_STATUS:
					voc.run_in_status = (uint8_t)bsec_outputs[index].signal;
					break;
				default:
					continue;
			}
		}
	}

	return BSEC_OK;
}

uint16_t voc_state_load(uint8_t *data) {
	uint32_t page[EEPROM_PAGE_SIZE/sizeof(uint32_t)];
	bootloader_read_eeprom_page(VOC_STATE_DATA1_PAGE, page);
	memcpy(data, page, BSEC_MAX_STATE_BLOB_SIZE);

	uint32_t checksum = 0;
	for(uint8_t i = 0; i < EEPROM_PAGE_SIZE/sizeof(uint32_t)-1; i++) {
		checksum ^= page[i];
	}

	logd("voc_state_load: magic %d, length %d, checksum %d, duration %d\n\r", page[VOC_STATE_INFO_MAGIC_POS], page[VOC_STATE_INFO_LENGTH_POS], page[VOC_STATE_INFO_CHECKSUM_POS], page[VOC_STATE_INFO_DURATION_POS]);
	if((page[VOC_STATE_INFO_MAGIC_POS] == VOC_STATE_MAGIC) && (page[VOC_STATE_INFO_CHECKSUM_POS] == checksum) && (page[VOC_STATE_INFO_LENGTH_POS] <= BSEC_MAX_STATE_BLOB_SIZE)) {
		if(page[VOC_STATE_INFO_DURATION_POS] == AIR_QUALITY_DURATION_4_DAYS) {
			voc.calibration_duration = AIR_QUALITY_DURATION_4_DAYS;
		} else {
			voc.calibration_duration = AIR_QUALITY_DURATION_28_DAYS;
		}

		return page[VOC_STATE_INFO_LENGTH_POS];
	}

	// Default duration is 28 days		
	voc.calibration_duration = AIR_QUALITY_DURATION_28_DAYS;

	logd("Unexpected state: magic %u = %u, checksum %u = %u, length %u\n\r",
		 page[VOC_STATE_INFO_MAGIC_POS],
		 VOC_STATE_MAGIC,
		 page[VOC_STATE_INFO_CHECKSUM_POS],
		 checksum,
		 page[VOC_STATE_INFO_LENGTH_POS]);

	return 0;
}

void voc_state_invalidate(void) {
	uint32_t page[EEPROM_PAGE_SIZE/sizeof(uint32_t)] = {0};
	bootloader_write_eeprom_page(VOC_STATE_DATA1_PAGE, page);
	bootloader_write_eeprom_page(VOC_STATE_DATA2_PAGE, page);
}

void voc_state_save(uint8_t *data, const uint16_t length) {
	uint32_t page[EEPROM_PAGE_SIZE/sizeof(uint32_t)];

	// Write first data page and calculate checksum
	memset(page, 0, EEPROM_PAGE_SIZE);
	memcpy(page, data, length); // max length is BSEC_MAX_STATE_BLOB_SIZE
	bootloader_write_eeprom_page(VOC_STATE_DATA1_PAGE, page);

	// Write magic, length and checksum to end of first page
	page[VOC_STATE_INFO_DURATION_POS] = voc.calibration_duration;
	page[VOC_STATE_INFO_MAGIC_POS]	  = VOC_STATE_MAGIC;
	page[VOC_STATE_INFO_LENGTH_POS]   = length;
	uint32_t checksum = 0;
	for(uint8_t i = 0; i < EEPROM_PAGE_SIZE/sizeof(uint32_t); i++) {
		checksum ^= page[i];
	}
	page[VOC_STATE_INFO_CHECKSUM_POS] = checksum;

	logd("voc_state_save: magic %d, length %d, checksum %d, duration %d\n\r", VOC_STATE_MAGIC, length, checksum, voc.calibration_duration);

	bootloader_write_eeprom_page(VOC_STATE_DATA1_PAGE, page);
}

void voc_tick_task_init(void) {
	int8_t result = bme680_init(&voc.bme680);
	logd("BME680 init: %d\n\r", result);

	bsec_library_return_t ret = bsec_init();
	logd("BME680 BSEC init: %d\n\r", ret);

	bsec_version_t bsec_version_p;
	ret = bsec_get_version(&bsec_version_p);
	logd("BME680 BSEC version: %d.%d.%d.%d\n\r", bsec_version_p.major, bsec_version_p.minor, bsec_version_p.major_bugfix, bsec_version_p.minor_bugfix);

	if(voc.calibration_duration == AIR_QUALITY_DURATION_4_DAYS) {
		bsec_set_configuration(voc_config_iaq_33v_3s_4d, BSEC_MAX_PROPERTY_BLOB_SIZE, voc.work_buffer, BSEC_MAX_WORKBUFFER_SIZE);
	} else {
		bsec_set_configuration(voc_config_iaq_33v_3s_28d, BSEC_MAX_PROPERTY_BLOB_SIZE, voc.work_buffer, BSEC_MAX_WORKBUFFER_SIZE);
	}

#if BME680_ENABLE_CALIBRATION
	// Load previous BSEC state (if available) and use it
	uint8_t bsec_state[BSEC_MAX_STATE_BLOB_SIZE];
	uint8_t bsec_state_work_buffer[BSEC_MAX_WORKBUFFER_SIZE];

	uint16_t length = voc_state_load(bsec_state);
	if(length != 0) {
		bsec_library_return_t status = bsec_set_state(bsec_state, length, bsec_state_work_buffer, BSEC_MAX_WORKBUFFER_SIZE);
		if(status != BSEC_OK) {
			logw("Unexpected status during state set: %d\n\r", status);
		} else {
			logd("BSEC state read from flash\n\r");
		}
	} else {
		logd("No BSEC state found in flash\n\r");
	}
#endif

	bsec_sensor_configuration_t requested_virtual_sensors[9];
	uint8_t requested_virtual_sensors_num = 9;

	bsec_sensor_configuration_t required_sensor_settings[BSEC_MAX_PHYSICAL_SENSOR];
	uint8_t required_sensor_settings_num = BSEC_MAX_PHYSICAL_SENSOR;

	// TODO: The lowest sample rate in the BOSCH config is "BSEC_SAMPLE_RATE_LP",
	// which corresponds to one sample for each 3 seconds. Per trial and error we found
	// that it works with 1 sample per second, but breaks if we increase the rate.
	// Is it OK to put this to 1 SPS or should we decrease it to 0.33 SPS?
	requested_virtual_sensors[0].sensor_id = BSEC_OUTPUT_IAQ;
	requested_virtual_sensors[0].sample_rate = BSEC_SAMPLE_RATE_LP;
	requested_virtual_sensors[1].sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE;
	requested_virtual_sensors[1].sample_rate = BSEC_SAMPLE_RATE_LP;
	requested_virtual_sensors[2].sensor_id = BSEC_OUTPUT_RAW_PRESSURE;
	requested_virtual_sensors[2].sample_rate = BSEC_SAMPLE_RATE_LP;
	requested_virtual_sensors[3].sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY;
	requested_virtual_sensors[3].sample_rate = BSEC_SAMPLE_RATE_LP;
	requested_virtual_sensors[4].sensor_id = BSEC_OUTPUT_RAW_GAS;
	requested_virtual_sensors[4].sample_rate = BSEC_SAMPLE_RATE_LP;
	requested_virtual_sensors[5].sensor_id = BSEC_OUTPUT_RAW_TEMPERATURE;
	requested_virtual_sensors[5].sample_rate = BSEC_SAMPLE_RATE_LP;
	requested_virtual_sensors[6].sensor_id = BSEC_OUTPUT_RAW_HUMIDITY;
	requested_virtual_sensors[6].sample_rate = BSEC_SAMPLE_RATE_LP;
	requested_virtual_sensors[7].sensor_id = BSEC_OUTPUT_STABILIZATION_STATUS;
	requested_virtual_sensors[7].sample_rate = BSEC_SAMPLE_RATE_LP;
	requested_virtual_sensors[8].sensor_id = BSEC_OUTPUT_RUN_IN_STATUS;
	requested_virtual_sensors[8].sample_rate = BSEC_SAMPLE_RATE_LP;

	bsec_update_subscription(requested_virtual_sensors, requested_virtual_sensors_num, required_sensor_settings, &required_sensor_settings_num);
}

uint64_t voc_get_timestamp(void) {
	return (ccu4_timer_get_value_64bit()/12)*125; // *1000/96 to go from 96MHz timer to ns
}

void voc_tick_task(void) {
	voc_tick_task_init();

	bsec_bme_settings_t settings;
	bsec_input_t bsec_inputs[BSEC_MAX_PHYSICAL_SENSOR];

	uint32_t timestamp_state = system_timer_get_ms();

	while(true) {
		int64_t timestamp = voc_get_timestamp();

		// Get settings from bsec
		bsec_sensor_control(timestamp, &settings);

		// Trigger measurement with new settings
		voc_task_bsec_trigger_measurement(&settings);

		// Read data from measurement
		uint8_t num_bsec_inputs = 0;
		if(voc_task_bsec_read_data(timestamp, bsec_inputs, &num_bsec_inputs, settings.process_data) != BME680_OK) {
			continue;
		}

		// Give data to BSEC for processing
		if(voc_task_bsec_process_data(bsec_inputs, num_bsec_inputs) != BSEC_OK) {
			continue;
		}

#if BME680_ENABLE_CALIBRATION
		// Update saved state if 12 hours are elapsed since last save
		// At 50000 page erase cycles (see XMC1400 datasheet page 10.1.1) and 2 pages written per state save
		// we expect the flash to live for about ~30 years of continuous usage.
		if(system_timer_is_time_elapsed_ms(timestamp_state, 1000*60*60*12)) {
			uint8_t bsec_state[BSEC_MAX_STATE_BLOB_SIZE];
			uint8_t bsec_state_work_buffer[BSEC_MAX_WORKBUFFER_SIZE];
			uint32_t bsec_state_length = 0;
			
			bsec_library_return_t status = bsec_get_state(0, bsec_state, BSEC_MAX_STATE_BLOB_SIZE, bsec_state_work_buffer, BSEC_MAX_WORKBUFFER_SIZE, &bsec_state_length);
			if(status == BSEC_OK) {
				voc_state_save(bsec_state, bsec_state_length);
				logd("New state saved, time: %u\n\r", timestamp_state);
			} else {
				logw("Unexpected status during state save: %d\n\r", status);
			}

			timestamp_state = system_timer_get_ms();
		}
#endif

		if(voc.new_calibration_duration) {
			voc.new_calibration_duration = false;
			uint8_t bsec_state[BSEC_MAX_STATE_BLOB_SIZE] = {0};
			uint32_t bsec_state_length = 0;

			// Set new calibration duration and invalidate old calibration
			voc_state_save(bsec_state, bsec_state_length);
			logd("Set new calibration duration: %d\n\r", voc.calibration_duration);
		}

		// Refresh timestamp and check how long we have to sleep
		timestamp = voc_get_timestamp();
		if(settings.next_call > timestamp) {
			uint32_t sleep_time = (settings.next_call - timestamp)/(1000*1000);
			coop_task_sleep_ms(sleep_time);
		}
	}
}

void voc_init(void) {
	memset(&voc, 0, sizeof(VOC));
	voc.bme680.dev_id = 0;
	voc.bme680.intf = BME680_SPI_INTF;
	voc.bme680.read = voc_task_spi_read;
	voc.bme680.write = voc_task_spi_write;
	voc.bme680.delay_ms = coop_task_sleep_ms;
	voc.calibration_duration = AIR_QUALITY_DURATION_28_DAYS;

	// USIC channel configuration
	const XMC_SPI_CH_CONFIG_t channel_config = {
		.baudrate       = BME680_SPI_BAUDRATE,
		.bus_mode       = XMC_SPI_CH_BUS_MODE_MASTER,
		.selo_inversion = XMC_SPI_CH_SLAVE_SEL_INV_TO_MSLS,
		.parity_mode    = XMC_USIC_CH_PARITY_MODE_NONE
	};

	// MOSI pin configuration
	const XMC_GPIO_CONFIG_t mosi_pin_config = {
		.mode             = BME680_MOSI_PIN_AF,
		.output_level     = XMC_GPIO_OUTPUT_LEVEL_HIGH
	};

	// MISO pin configuration
	const XMC_GPIO_CONFIG_t miso_pin_config = {
		.mode             = XMC_GPIO_MODE_INPUT_TRISTATE,
		.input_hysteresis = XMC_GPIO_INPUT_HYSTERESIS_STANDARD
	};

	// SCLK pin configuration
	const XMC_GPIO_CONFIG_t sclk_pin_config = {
		.mode             = BME680_SCLK_PIN_AF,
		.output_level     = XMC_GPIO_OUTPUT_LEVEL_HIGH
	};

	// SELECT pin configuration
	const XMC_GPIO_CONFIG_t select_pin_config = {
		.mode             = BME680_SELECT_PIN_AF,
		.output_level     = XMC_GPIO_OUTPUT_LEVEL_HIGH
	};

	// Configure MISO pin
	XMC_GPIO_Init(BME680_MISO_PIN, &miso_pin_config);

	// Initialize USIC channel in SPI master mode
	XMC_SPI_CH_Init(BME680_USIC, &channel_config);
	BME680_USIC->SCTR &= ~USIC_CH_SCTR_PDL_Msk; // Set passive data level to 0
	BME680_USIC->PCR_SSCMode &= ~USIC_CH_PCR_SSCMode_TIWEN_Msk; // Disable time between bytes

	XMC_SPI_CH_SetBitOrderMsbFirst(BME680_USIC);

	XMC_SPI_CH_SetWordLength(BME680_USIC, (uint8_t)8U);
	XMC_SPI_CH_SetFrameLength(BME680_USIC, (uint8_t)64U);

	XMC_SPI_CH_SetTransmitMode(BME680_USIC, XMC_SPI_CH_MODE_STANDARD);

	// Configure the clock polarity and clock delay
	XMC_SPI_CH_ConfigureShiftClockOutput(BME680_USIC,
	                                     XMC_SPI_CH_BRG_SHIFT_CLOCK_PASSIVE_LEVEL_1_DELAY_DISABLED,
	                                     XMC_SPI_CH_BRG_SHIFT_CLOCK_OUTPUT_SCLK);
	// Configure Leading/Trailing delay
	XMC_SPI_CH_SetSlaveSelectDelay(BME680_USIC, 2);


	// Set input source path
	XMC_SPI_CH_SetInputSource(BME680_USIC, BME680_MISO_INPUT, BME680_MISO_SOURCE);

	// SPI Mode: CPOL=1 and CPHA=1
	BME680_USIC_CHANNEL->DX1CR |= USIC_CH_DX1CR_DPOL_Msk;

	// Configure transmit FIFO
	XMC_USIC_CH_TXFIFO_Configure(BME680_USIC, 48, XMC_USIC_CH_FIFO_SIZE_16WORDS, 8);

	// Configure receive FIFO
	XMC_USIC_CH_RXFIFO_Configure(BME680_USIC, 32, XMC_USIC_CH_FIFO_SIZE_16WORDS, 8);

	// Set service request for tx FIFO transmit interrupt
	XMC_USIC_CH_TXFIFO_SetInterruptNodePointer(BME680_USIC, XMC_USIC_CH_TXFIFO_INTERRUPT_NODE_POINTER_STANDARD, BME680_SERVICE_REQUEST_TX);  // IRQ BME680_IRQ_TX

	// Set service request for rx FIFO receive interrupt
	XMC_USIC_CH_RXFIFO_SetInterruptNodePointer(BME680_USIC, XMC_USIC_CH_RXFIFO_INTERRUPT_NODE_POINTER_STANDARD, BME680_SERVICE_REQUEST_RX);  // IRQ BME680_IRQ_RX
	XMC_USIC_CH_RXFIFO_SetInterruptNodePointer(BME680_USIC, XMC_USIC_CH_RXFIFO_INTERRUPT_NODE_POINTER_ALTERNATE, BME680_SERVICE_REQUEST_RX); // IRQ BME680_IRQ_RX

	//Set priority and enable NVIC node for transmit interrupt
	NVIC_SetPriority((IRQn_Type)BME680_IRQ_TX, BME680_IRQ_TX_PRIORITY);
	NVIC_EnableIRQ((IRQn_Type)BME680_IRQ_TX);

	// Set priority and enable NVIC node for receive interrupt
	NVIC_SetPriority((IRQn_Type)BME680_IRQ_RX, BME680_IRQ_RX_PRIORITY);
	NVIC_EnableIRQ((IRQn_Type)BME680_IRQ_RX);

	// Start SPI
	XMC_SPI_CH_Start(BME680_USIC);

	// Configure SCLK pin
	XMC_GPIO_Init(BME680_SCLK_PIN, &sclk_pin_config);

	// Configure slave select pin
	XMC_GPIO_Init(BME680_SELECT_PIN, &select_pin_config);

	// Configure MOSI pin
	XMC_GPIO_Init(BME680_MOSI_PIN, &mosi_pin_config);

	XMC_USIC_CH_RXFIFO_Flush(BME680_USIC);
	XMC_USIC_CH_RXFIFO_EnableEvent(BME680_USIC, XMC_USIC_CH_RXFIFO_EVENT_CONF_STANDARD | XMC_USIC_CH_RXFIFO_EVENT_CONF_ALTERNATE);

	// Start simple full-speed 96MHz timer
	ccu4_timer_init(XMC_CCU4_SLICE_PRESCALER_1, 0xFFFF);

	// BME680 initialization function has to be called from coop task
	coop_task_init(&voc_task, voc_tick_task);
}

void voc_tick(void) {
	coop_task_tick(&voc_task);
}

int32_t voc_get_humidity(void) {
	return voc.compensated_humidity;
}

int32_t voc_get_temperature(void) {
	return voc.compensated_temperature;
}

int32_t voc_get_air_pressure(void) {
	return voc.raw_air_pressure;
}

int32_t voc_get_iaq_index(void) {
	return voc.iaq_index;
}

uint8_t voc_get_iaq_index_accuracy(void) {
	return voc.iaq_accuracy;
}
