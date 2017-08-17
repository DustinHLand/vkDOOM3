/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company. 
Copyright (C) 2016-2017 Dustin Land

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").  

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "Common_local.h"

idCVar com_logFile( "logFile", "0", CVAR_SYSTEM | CVAR_NOCHEAT, "1 = buffer log, 2 = flush after each print", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );
idCVar com_logFileName( "logFileName", "qconsole.log", CVAR_SYSTEM | CVAR_NOCHEAT, "name of log file, if empty, qconsole.log will be used" );
idCVar com_timestampPrints( "com_timestampPrints", "0", CVAR_SYSTEM, "print time with each console print, 1 = msec, 2 = sec", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );

#ifndef ID_RETAIL
idCVar com_printFilter( "com_printFilter", "", CVAR_SYSTEM, "only print lines that contain this, add multiple filters with a ; delimeter");
#endif

/*
==================
idCommonLocal::CloseLogFile
==================
*/
void idCommonLocal::CloseLogFile() {
	if ( logFile ) {
		com_logFile.SetBool( false ); // make sure no further VPrintf attempts to open the log file again
		fileSystem->CloseFile( logFile );
		logFile = NULL;
	}
}

/*
==================
idCommonLocal::SetRefreshOnPrint
==================
*/
void idCommonLocal::SetRefreshOnPrint( bool set ) {
	com_refreshOnPrint = set;
}