/*
 * KeyTools.h - authentication key helpers for pairing teacher and student computers (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QStringList>

namespace KeyTools {

// names of the private (true) or public (false) keys installed on this computer
QStringList keyNames( bool privateKeys );

// true if this computer accepts at least one teacher key (a public key without own private key)
bool hasTeacherKeys();

// public key of the key the Teacher Console uses (teacher computer)
bool teacherPublicKey( QString* keyName, QString* publicKeyPem );

// student computer: installs the public key of the teacher, sets the room and enables key file
// authentication; needs administrator or SYSTEM rights
bool applyTeacherKey( const QString& keyName, const QString& publicKeyPem, const QString& room, QString* error );

// runs veyon-wcli (no console window) and returns true on success
bool runCli( const QStringList& arguments, QString* output = nullptr );

}
