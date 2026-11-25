/*
 * TeacherServiceControl.h - run the ITLabControl service only while the Teacher Console is open (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

// On computers installed with the role "Teacher" (configuration ITLab/Role = Teacher) the service is
// not started with Windows. The Teacher Console starts it when it opens (it is needed for local
// features such as the demo server) and stops it when it closes, so nothing of ITLabControl keeps
// running in the background. The installer allows interactive users to start/stop the service.
namespace TeacherServiceControl {

bool isTeacherComputer();
void startService();
void stopService();

}
