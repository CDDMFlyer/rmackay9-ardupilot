/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * AP_IRLock_PX4.cpp
 *
 *  Created on: Nov 16, 2014
 *      Author: MLandes
 */

#if CONFIG_HAL_BOARD == HAL_BOARD_PX4

#include "AP_IRLock_PX4.h"

#include <fcntl.h>
#include <unistd.h>

#include "AP_HAL.h"
#include "drivers/drv_irlock.h"

extern const AP_HAL::HAL& hal;

AP_IRLock_PX4::AP_IRLock_PX4() :
		_fd(0),
		_last_timestamp(0)
{}

void AP_IRLock_PX4::init()
{
	_fd = open(IRLOCK0_DEVICE_PATH, O_RDONLY);
	if (_fd < 0) {
		hal.console->printf("Unable to open " IRLOCK0_DEVICE_PATH "\n");
		return;
	}

	_flags.healthy = true;
}

void AP_IRLock_PX4::update()
{
    // return immediately if not healthy
	if (!_flags.healthy) {
		return;
	}

	// read position of all objects
	struct irlock_s report;
	_num_blocks = 0;
	while(::read(_fd, &report, sizeof(struct irlock_s)) == sizeof(struct irlock_s) && report.timestamp >_last_timestamp) {
		_current_frame[_num_blocks].signature = report.signature;
		_current_frame[_num_blocks].center_x = report.center_x;
		_current_frame[_num_blocks].center_y = report.center_y;
		_current_frame[_num_blocks].width = report.width;
		_current_frame[_num_blocks].height = report.height;

		++_num_blocks;
		_last_timestamp = report.timestamp;
		_last_update = hal.scheduler->millis();
	}
}

#endif // CONFIG_HAL_BOARD == HAL_BOARD_PX4
