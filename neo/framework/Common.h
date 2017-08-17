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

#ifndef __COMMON_H__
#define __COMMON_H__

/*
==============================================================

  Common

==============================================================
*/

extern idCVar com_engineHz;
extern float com_engineHz_latched;
extern int64 com_engineHz_numerator;
extern int64 com_engineHz_denominator;

// Returns the msec the frame starts on
ID_INLINE int FRAME_TO_MSEC( int64 frame ) {
	return (int)( ( frame * com_engineHz_numerator ) / com_engineHz_denominator );
}
// Rounds DOWN to the nearest frame
ID_INLINE int MSEC_TO_FRAME_FLOOR( int msec ) {
	return (int)( ( ( (int64)msec * com_engineHz_denominator ) + ( com_engineHz_denominator - 1 ) ) / com_engineHz_numerator );
}
// Rounds UP to the nearest frame
ID_INLINE int MSEC_TO_FRAME_CEIL( int msec ) {
	return (int)( ( ( (int64)msec * com_engineHz_denominator ) + ( com_engineHz_numerator - 1 ) ) / com_engineHz_numerator );
}
// Aligns msec so it starts on a frame bondary
ID_INLINE int MSEC_ALIGN_TO_FRAME( int msec ) {
	return FRAME_TO_MSEC( MSEC_TO_FRAME_CEIL( msec ) );
}

class idGame;
class idRenderWorld;
class idSoundWorld;
class idSession;
class idCommonDialog;
class idUserInterface;
class idSaveLoadParms;
class idMatchParameters;

struct lobbyConnectInfo_t;

ID_INLINE void BeginProfileNamedEventColor( uint32 color, const char * szName ) {
}
ID_INLINE void EndProfileNamedEvent() {
}

ID_INLINE void BeginProfileNamedEvent( const char * szName ) {
	BeginProfileNamedEventColor( (uint32) 0xFF00FF00, szName );
}

class idScopedProfileEvent {
public:
	idScopedProfileEvent( const char * name ) { BeginProfileNamedEvent( name ); }
	~idScopedProfileEvent() { EndProfileNamedEvent(); }
};

#define SCOPED_PROFILE_EVENT( x ) idScopedProfileEvent scopedProfileEvent_##__LINE__( x )

ID_INLINE bool BeginTraceRecording( char * szName ) {
	return false;
}

ID_INLINE bool EndTraceRecording() {
	return false;
}

typedef enum {
	EDITOR_NONE					= 0,
	EDITOR_RADIANT				= BIT(1),
	EDITOR_GUI					= BIT(2),
	EDITOR_DEBUGGER				= BIT(3),
	EDITOR_SCRIPT				= BIT(4),
	EDITOR_LIGHT				= BIT(5),
	EDITOR_SOUND				= BIT(6),
	EDITOR_DECL					= BIT(7),
	EDITOR_AF					= BIT(8),
	EDITOR_PARTICLE				= BIT(9),
	EDITOR_PDA					= BIT(10),
	EDITOR_AAS					= BIT(11),
	EDITOR_MATERIAL				= BIT(12)
} toolFlag_t;

#define STRTABLE_ID				"#str_"
#define STRTABLE_ID_LENGTH		5

extern idCVar		com_version;
extern idCVar		com_developer;
extern idCVar		com_allowConsole;
extern idCVar		com_speeds;
extern idCVar		com_showFPS;
extern idCVar		com_showMemoryUsage;
extern idCVar		com_updateLoadSize;
extern idCVar		com_productionMode;

struct MemInfo_t {
	idStr			filebase;

	int				total;
	int				assetTotals;

	// memory manager totals
	int				memoryManagerTotal;

	// subsystem totals
	int				gameSubsystemTotal;
	int				renderSubsystemTotal;

	// asset totals
	int				imageAssetsTotal;
	int				modelAssetsTotal;
	int				soundAssetsTotal;
};

struct mpMap_t {

	void operator=( const mpMap_t & src ) {
		mapFile = src.mapFile;
		mapName = src.mapName;
		supportedModes = src.supportedModes;
	}

	idStr			mapFile;
	idStr			mapName;
	uint32			supportedModes;
};

static const int	MAX_LOGGED_STATS = 60 * 120;		// log every half second 

enum currentGame_t {
	DOOM_CLASSIC,
	DOOM2_CLASSIC,
	DOOM3_BFG
};

class idCommon {
public:
	virtual						~idCommon() {}

								// Initialize everything.
								// if the OS allows, pass argc/argv directly (without executable name)
								// otherwise pass the command line in a single string (without executable name)
	virtual void				Init( int argc, const char * const * argv, const char *cmdline ) = 0;

								// Shuts down everything.
	virtual void				Shutdown() = 0;
	virtual bool				IsShuttingDown() const = 0;

	virtual	void				CreateMainMenu() = 0;

								// Shuts down everything.
	virtual void				Quit() = 0;

								// Returns true if common initialization is complete.
	virtual bool				IsInitialized() const = 0;

								// Called repeatedly as the foreground thread for rendering and game logic.
	virtual void				Frame() = 0;

	// Redraws the screen, handling games, guis, console, etc
	// in a modal manner outside the normal frame loop
	virtual void				UpdateScreen() = 0;

	virtual void				UpdateLevelLoadPacifier() = 0;
	

								// Checks for and removes command line "+set var arg" constructs.
								// If match is NULL, all set commands will be executed, otherwise
								// only a set with the exact name.
	virtual void				StartupVariable( const char * match ) = 0;

								// Update the screen with every message printed.
	virtual void				SetRefreshOnPrint( bool set ) = 0;

								// Prints all queued warnings.
	virtual void				PrintWarnings() = 0;

								// Removes all queued warnings.
	virtual void				ClearWarnings( const char *reason ) = 0;

								// Returns key bound to the command
	virtual const char *		KeysFromBinding( const char *bind ) = 0;

								// Returns the binding bound to the key
	virtual const char *		BindingFromKey( const char *key ) = 0; 

								// Directly sample a button.
	virtual int					ButtonState( int key ) = 0;

								// Directly sample a keystate.
	virtual int					KeyState( int key ) = 0;

	// Returns true if a multiplayer game is running.
	// CVars and commands are checked differently in multiplayer mode.
	virtual bool				IsMultiplayer() = 0;
	virtual bool				IsServer() = 0;
	virtual bool				IsClient() = 0;

	// Returns true if the player has ever enabled the console
	virtual bool				GetConsoleUsed() = 0;

	// Returns the rate (in ms between snaps) that we want to generate snapshots
	virtual int					GetSnapRate() = 0;

	virtual void				NetReceiveReliable( int peer, int type, idBitMsg & msg ) = 0;
	virtual void				NetReceiveSnapshot( class idSnapShot & ss ) = 0;
	virtual void				NetReceiveUsercmds( int peer, idBitMsg & msg ) = 0;

	// Processes the given event.
	virtual	bool				ProcessEvent( const sysEvent_t * event ) = 0;

	virtual bool				LoadGame( const char * saveName ) = 0;
	virtual bool				SaveGame( const char * saveName ) = 0;

	virtual idGame *			Game() = 0;
	virtual idRenderWorld *		RW() = 0;
	virtual idSoundWorld *		SW() = 0;
	virtual idSoundWorld *		MenuSW() = 0;
	virtual idSession *			Session() = 0;
	virtual idCommonDialog &	Dialog() = 0;

	virtual void				OnSaveCompleted( idSaveLoadParms & parms ) = 0;
	virtual void				OnLoadCompleted( idSaveLoadParms & parms ) = 0;
	virtual void				OnLoadFilesCompleted( idSaveLoadParms & parms ) = 0;
	virtual void				OnEnumerationCompleted( idSaveLoadParms & parms ) = 0;
	virtual void				OnDeleteCompleted( idSaveLoadParms & parms ) = 0;
	virtual void				TriggerScreenWipe( const char * _wipeMaterial, bool hold ) = 0;

	virtual void				OnStartHosting( idMatchParameters & parms ) = 0;

	virtual int					GetGameFrame() = 0;

	virtual void				LaunchExternalTitle( int titleIndex, int device, const lobbyConnectInfo_t * const connectInfo ) = 0;

	virtual void				InitializeMPMapsModes() = 0;
	virtual const idStrList &			GetModeList() const = 0;
	virtual const idStrList &			GetModeDisplayList() const = 0;
	virtual const idList<mpMap_t> &		GetMapList() const = 0;

	virtual void				ResetPlayerInput( int playerIndex ) = 0;

	virtual bool				JapaneseCensorship() const = 0;

	virtual void				QueueShowShell() = 0;		// Will activate the shell on the next frame.

	virtual currentGame_t		GetCurrentGame() const = 0;
	virtual void				SwitchToGame( currentGame_t newGame ) = 0;
};

extern idCommon *		common;

#endif /* !__COMMON_H__ */
