#include "global.h"
#include "NoteSkinManager.h"
#include "RageFileManager.h"
#include "RageLog.h"
#include "GameInput.h"
#include "GameState.h"
#include "Game.h"
#include "Style.h"
#include "RageUtil.h"
#include "RageDisplay.h"
#include "arch/Dialog/Dialog.h"
#include "PrefsManager.h"
#include "Foreach.h"
#include "ActorUtil.h"
#include "XmlFileUtil.h"
#include "Sprite.h"
#include <map>
#include "SpecialFiles.h"

/** @brief Have the NoteSkinManager available throughout the program. */
NoteSkinManager*	NOTESKIN = NULL; // global and accessible from anywhere in our program

const RString GAME_COMMON_NOTESKIN_NAME = "common";
const RString GAME_BASE_NOTESKIN_NAME = "default";

// this isn't a global because of nondeterministic global actor ordering
// might init this before SpecialFiles::NOTESKINS_DIR
#define GLOBAL_BASE_DIR (SpecialFiles::NOTESKINS_DIR + GAME_COMMON_NOTESKIN_NAME + "/")

static map<RString,RString> g_PathCache;

struct NoteSkinData
{
	RString sName;
	IniFile metrics;

	// When looking for an element, search these dirs from head to tail.
	vector<RString> vsDirSearchOrder;

	LuaReference m_Loader;
};

namespace
{
	static map<RString,NoteSkinData> g_mapNameToData;
};

NoteSkinManager::NoteSkinManager()
{
	m_pCurGame = NULL;
	m_PlayerNumber = PlayerNumber_Invalid;
	m_GameController = GameController_Invalid;

	// Register with Lua.
	{
		Lua *L = LUA->Get();
		lua_pushstring( L, "NOTESKIN" );
		this->PushSelf( L );
		lua_settable( L, LUA_GLOBALSINDEX );
		LUA->Release( L );
	}
}

NoteSkinManager::~NoteSkinManager()
{
	// Unregister with Lua.
	LUA->UnsetGlobal( "NOTESKIN" );

	g_mapNameToData.clear();
}

void NoteSkinManager::RefreshNoteSkinData( const Game* pGame )
{
	m_pCurGame = pGame;

	// clear path cache
	g_PathCache.clear();

	RString sBaseSkinFolder = SpecialFiles::NOTESKINS_DIR + pGame->m_szName + "/";
	vector<RString> asNoteSkinNames;
	GetDirListing( sBaseSkinFolder + "*", asNoteSkinNames, true );

	StripCvsAndSvn( asNoteSkinNames );
	StripMacResourceForks( asNoteSkinNames );

	g_mapNameToData.clear();
	for( unsigned j=0; j<asNoteSkinNames.size(); j++ )
	{
		RString sName = asNoteSkinNames[j];
		sName.MakeLower();
		LoadNoteSkinData( sName, g_mapNameToData[sName] );
	}
}

void NoteSkinManager::LoadNoteSkinData( const RString &sNoteSkinName, NoteSkinData& data_out )
{
	data_out.sName = sNoteSkinName;
	data_out.metrics.Clear();
	data_out.vsDirSearchOrder.clear();

	// Read the current NoteSkin and all of its fallbacks
	LoadNoteSkinDataRecursive( sNoteSkinName, data_out );
}

void NoteSkinManager::LoadNoteSkinDataRecursive( const RString &sNoteSkinName_, NoteSkinData& data_out )
{
	RString sNoteSkinName(sNoteSkinName_);

	int iDepth = 0;
	bool bLoadedCommon = false;
	bool bLoadedBase = false;
	while(1)
	{
		++iDepth;
		ASSERT_M( iDepth < 20, "Circular NoteSkin fallback references detected." );

		RString sDir = SpecialFiles::NOTESKINS_DIR + m_pCurGame->m_szName + "/" + sNoteSkinName + "/";
		if( !FILEMAN->IsADirectory(sDir) )
		{
			sDir = GLOBAL_BASE_DIR + sNoteSkinName + "/";
			if( !FILEMAN->IsADirectory(sDir) )
			{
				LOG->Trace( "NoteSkin \"%s\" references skin \"%s\" that is not present",
					data_out.sName.c_str(), sNoteSkinName.c_str() );
				return;
			}
		}

		LOG->Trace( "LoadNoteSkinDataRecursive: %s (%s)", sNoteSkinName.c_str(), sDir.c_str() );

		// read global fallback the current NoteSkin (if any)
		IniFile ini;
		ini.ReadFile( sDir+"metrics.ini" );

		if( !sNoteSkinName.CompareNoCase(GAME_BASE_NOTESKIN_NAME) )
			bLoadedBase = true;
		if( !sNoteSkinName.CompareNoCase(GAME_COMMON_NOTESKIN_NAME) )
			bLoadedCommon = true;

		RString sFallback;
		if( !ini.GetValue("Global","FallbackNoteSkin", sFallback) )
		{
			if( !bLoadedBase )
				sFallback = GAME_BASE_NOTESKIN_NAME;
			else if( !bLoadedCommon )
				sFallback = GAME_COMMON_NOTESKIN_NAME;
		}

		XmlFileUtil::MergeIniUnder( &ini, &data_out.metrics );

		data_out.vsDirSearchOrder.push_back( sDir );

		if( sFallback.empty() )
			break;
		sNoteSkinName = sFallback;
	}

	LuaReference refScript;
	for( vector<RString>::reverse_iterator dir = data_out.vsDirSearchOrder.rbegin(); dir != data_out.vsDirSearchOrder.rend(); ++dir )
	{
		RString sFile = *dir + "NoteSkin.lua";
		RString sScript;
		if( !FILEMAN->IsAFile(sFile) )
			continue;

		if( !GetFileContents(sFile, sScript) )
			continue;

		LOG->Trace( "Load script \"%s\"", sFile.c_str() );

		Lua *L = LUA->Get();
		RString sError;
		refScript.PushSelf( L );
		if( !LuaHelpers::RunScript(L, sScript, "@" + sFile, sError, 1, 1) )
		{
			LOG->Trace( "Error running %s: %s", sFile.c_str(), sError.c_str() );
			lua_pop( L, 1 );
		}
		else
		{
			refScript.SetFromStack( L );
		}
		LUA->Release( L );
	}
	data_out.m_Loader = refScript;
}


void NoteSkinManager::GetNoteSkinNames( vector<RString> &AddTo )
{
	GetNoteSkinNames( GAMESTATE->m_pCurGame, AddTo );
}

void NoteSkinManager::GetNoteSkinNames( const Game* pGame, vector<RString> &AddTo )
{
	GetAllNoteSkinNamesForGame( pGame, AddTo );
}

bool NoteSkinManager::DoesNoteSkinExist( const RString &sSkinName )
{
	vector<RString> asSkinNames;
	GetAllNoteSkinNamesForGame( GAMESTATE->m_pCurGame, asSkinNames );
	for( unsigned i=0; i<asSkinNames.size(); i++ )
		if( 0==stricmp(sSkinName, asSkinNames[i]) )
			return true;
	return false;
}

bool NoteSkinManager::DoNoteSkinsExistForGame( const Game *pGame )
{
	vector<RString> asSkinNames;
	GetAllNoteSkinNamesForGame( pGame, asSkinNames );
	return !asSkinNames.empty();
}

void NoteSkinManager::GetAllNoteSkinNamesForGame( const Game *pGame, vector<RString> &AddTo )
{
	if( pGame == m_pCurGame )
	{
		// Faster:
		for( map<RString,NoteSkinData>::const_iterator iter = g_mapNameToData.begin();
		     iter != g_mapNameToData.end(); ++iter )
		{
			AddTo.push_back( iter->second.sName );
		}
	}
	else
	{
		RString sBaseSkinFolder = SpecialFiles::NOTESKINS_DIR + pGame->m_szName + "/";
		GetDirListing( sBaseSkinFolder + "*", AddTo, true );
		StripCvsAndSvn( AddTo );
		StripMacResourceForks( AddTo );
	}
}	

RString NoteSkinManager::GetMetric( const RString &sButtonName, const RString &sValue )
{
	ASSERT( !m_sCurrentNoteSkin.empty() );
	RString sNoteSkinName = m_sCurrentNoteSkin;
	sNoteSkinName.MakeLower();
	map<RString,NoteSkinData>::const_iterator it = g_mapNameToData.find(sNoteSkinName);
	ASSERT_M( it != g_mapNameToData.end(), sNoteSkinName );	// this NoteSkin doesn't exist!
	const NoteSkinData& data = it->second;

	RString sReturn;
	if( data.metrics.GetValue( sButtonName, sValue, sReturn ) )
		return sReturn;
	if( !data.metrics.GetValue( "NoteDisplay", sValue, sReturn ) )
		RageException::Throw( "Could not read metric \"%s::%s\" or \"NoteDisplay::%s\" in \"%s\".",
				      sButtonName.c_str(), sValue.c_str(), sValue.c_str(), sNoteSkinName.c_str() );
	return sReturn;
}

int NoteSkinManager::GetMetricI( const RString &sButtonName, const RString &sValueName )
{
	return StringToInt( GetMetric(sButtonName,sValueName) );
}

float NoteSkinManager::GetMetricF( const RString &sButtonName, const RString &sValueName )
{
	return StringToFloat( GetMetric(sButtonName,sValueName) );
}

bool NoteSkinManager::GetMetricB( const RString &sButtonName, const RString &sValueName )
{
	// Could also call GetMetricI here...hmm.
	return StringToInt( GetMetric(sButtonName,sValueName) ) != 0;
}

apActorCommands NoteSkinManager::GetMetricA( const RString &sButtonName, const RString &sValueName )
{
	return ActorUtil::ParseActorCommands( GetMetric(sButtonName,sValueName) );
}

RString NoteSkinManager::GetPath( const RString &sButtonName, const RString &sElement )
{
	const RString CacheString = m_sCurrentNoteSkin + "/" + sButtonName + "/" + sElement;
	map<RString,RString>::iterator it = g_PathCache.find( CacheString );
	if( it != g_PathCache.end() )
		return it->second;

	map<RString,NoteSkinData>::const_iterator iter = g_mapNameToData.find( m_sCurrentNoteSkin );
	ASSERT( iter != g_mapNameToData.end() );
	const NoteSkinData &data = iter->second;

	RString sPath;	// fill this in below
	FOREACH_CONST( RString, data.vsDirSearchOrder, lIter )
	{
		if( sButtonName.empty() )
			sPath = GetPathFromDirAndFile( *lIter, sElement );
		else
			sPath = GetPathFromDirAndFile( *lIter, sButtonName+" "+sElement );
		if( !sPath.empty() )
			break;	// done searching
	}

	if( sPath.empty() )
	{
		FOREACH_CONST( RString, data.vsDirSearchOrder, lIter )
		{
			if( !sButtonName.empty() )
				sPath = GetPathFromDirAndFile( *lIter, "Fallback "+sElement );
			if( !sPath.empty() )
				break;	// done searching
		}
	}

	if( sPath.empty() )
	{
		RString sPaths;
		FOREACH_CONST( RString, data.vsDirSearchOrder, dir )
		{
			if( !sPaths.empty() )
				sPaths += ", ";

			sPaths += *dir;
		}

		RString message = ssprintf(
			"The NoteSkin element \"%s %s\" could not be found in any of the following directories:\n%s", 
			sButtonName.c_str(), sElement.c_str(), 
			sPaths.c_str() );

		if( Dialog::AbortRetryIgnore(message) == Dialog::retry )
		{
			FOREACH_CONST( RString, data.vsDirSearchOrder, dir )
				FILEMAN->FlushDirCache( *dir );
			g_PathCache.clear();
			return GetPath( sButtonName, sElement );
		}
		
		RageException::Throw( "%s", message.c_str() ); 
	}

	int iLevel = 0;
	while( GetExtension(sPath) == "redir" )
	{
		iLevel++;
		ASSERT_M( iLevel < 100, ssprintf("Infinite recursion while looking up %s - %s", sButtonName.c_str(), sElement.c_str()) );
			
		RString sNewFileName;
		GetFileContents( sPath, sNewFileName, true );
		RString sRealPath;

		FOREACH_CONST( RString, data.vsDirSearchOrder, lIter )
		{
			 sRealPath = GetPathFromDirAndFile( *lIter, sNewFileName );
			 if( !sRealPath.empty() )
				 break;	// done searching
		}

		if( sRealPath == "" )
		{
			RString message = ssprintf(
					"NoteSkinManager:  The redirect \"%s\" points to the file \"%s\", which does not exist. "
					"Verify that this redirect is correct.",
					sPath.c_str(), sNewFileName.c_str());

			if( Dialog::AbortRetryIgnore(message) == Dialog::retry )
			{
				FOREACH_CONST( RString, data.vsDirSearchOrder, dir )
					FILEMAN->FlushDirCache( *dir );
				g_PathCache.clear();
				return GetPath( sButtonName, sElement );
			}

			RageException::Throw( "%s", message.c_str() ); 
		}
		
		sPath = sRealPath;
	}

	g_PathCache[CacheString] = sPath;
	return sPath;
}

bool NoteSkinManager::PushActorTemplate( Lua *L, const RString &sButton, const RString &sElement, bool bSpriteOnly )
{
	map<RString,NoteSkinData>::const_iterator iter = g_mapNameToData.find( m_sCurrentNoteSkin );
	ASSERT( iter != g_mapNameToData.end() );
	const NoteSkinData &data = iter->second;

	LuaThreadVariable varPlayer( "Player", LuaReference::Create(m_PlayerNumber) );
	LuaThreadVariable varController( "Controller", LuaReference::Create(m_GameController) );
	LuaThreadVariable varButton( "Button", sButton );
	LuaThreadVariable varElement( "Element", sElement );
	LuaThreadVariable varSpriteOnly( "SpriteOnly", LuaReference::Create(bSpriteOnly) );

	ASSERT( !data.m_Loader.IsNil() );
	data.m_Loader.PushSelf( L );
	lua_remove( L, -2 );
	lua_getfield( L, -1, "Load" );

	return ActorUtil::LoadTableFromStackShowErrors(L);
}

Actor *NoteSkinManager::LoadActor( const RString &sButton, const RString &sElement, Actor *pParent, bool bSpriteOnly )
{
	Lua *L = LUA->Get();

	if( !PushActorTemplate(L, sButton, sElement, bSpriteOnly) )
	{
		// ActorUtil will warn about the error
		return new Actor;
	}

	auto_ptr<XNode> pNode( XmlFileUtil::XNodeFromTable(L) );
	if( pNode.get() == NULL )
	{
		// XNode will warn about the error
		return new Actor;
	}

	LUA->Release( L );

	Actor *pRet = ActorUtil::LoadFromNode( pNode.get(), pParent );

	if( bSpriteOnly )
	{
		// Make sure pActor is a Sprite (or something derived from Sprite).
		Sprite *pSprite = dynamic_cast<Sprite *>( pRet );
		if( pSprite == NULL )
			LOG->Warn( "%s: %s %s must be a Sprite", m_sCurrentNoteSkin.c_str(), sButton.c_str(), sElement.c_str() );
	}

	return pRet;
}

RString NoteSkinManager::GetPathFromDirAndFile( const RString &sDir, const RString &sFileName )
{
	vector<RString> matches;		// fill this with the possible files

	GetDirListing( sDir+sFileName+"*",		matches, false, true );

	if( matches.empty() )
		return RString();

	if( matches.size() > 1 )
	{
		RString sError = "Multiple files match '"+sDir+sFileName+"'.  Please remove all but one of these files.";
		Dialog::OK( sError );
	}
	
	return matches[0];
}

// lua start
#include "LuaBinding.h"

/** @brief Allow Lua to have access to the NoteSkinManager. */ 
class LunaNoteSkinManager: public Luna<NoteSkinManager>
{
public:
	DEFINE_METHOD( GetPath, GetPath(SArg(1), SArg(2)) );
	DEFINE_METHOD( GetMetric, GetMetricI(SArg(1), SArg(2)) );
	DEFINE_METHOD( GetMetricI, GetMetricI(SArg(1), SArg(2)) );
	DEFINE_METHOD( GetMetricF, GetMetricF(SArg(1), SArg(2)) );
	DEFINE_METHOD( GetMetricB, GetMetricB(SArg(1), SArg(2)) );
	static int GetMetricA( T* p, lua_State *L )		{ p->GetMetricA(SArg(1),SArg(2))->PushSelf(L); return 1; }
	static int LoadActor( T* p, lua_State *L )
	{
		RString sButton = SArg(1);
		RString sElement = SArg(2);
		if( !p->PushActorTemplate(L, sButton, sElement, false) )
			lua_pushnil( L );

		return 1;
	}
#define FOR_NOTESKIN(x,n) \
	static int x ## ForNoteSkin( T* p, lua_State *L ) \
	{ \
		const RString sOldNoteSkin = p->GetCurrentNoteSkin(); \
		p->SetCurrentNoteSkin( SArg(n+1) ); \
		x( p, L ); \
		p->SetCurrentNoteSkin( sOldNoteSkin ); \
		return 1; \
	}
	FOR_NOTESKIN( GetPath, 2 );
	FOR_NOTESKIN( GetMetric, 2 );
	FOR_NOTESKIN( GetMetricI, 2 );
	FOR_NOTESKIN( GetMetricF, 2 );
	FOR_NOTESKIN( GetMetricB, 2 );
	FOR_NOTESKIN( GetMetricA, 2 );
	FOR_NOTESKIN( LoadActor, 2 );
#undef FOR_NOTESKIN
	static int GetNoteSkinNames( T* p, lua_State *L )
	{
		vector<RString> vNoteskins;
		p->GetNoteSkinNames( vNoteskins );
		LuaHelpers::CreateTableFromArray(vNoteskins, L);
		return 1;
	}
	/*
	static int GetNoteSkinNamesForGame( T* p, lua_State *L )
	{
		Game *pGame = Luna<Game>::check( L, 1 );
		vector<RString> vGameNoteskins;
		p->GetNoteSkinNames( pGame, vGameNoteskins );
		LuaHelpers::CreateTableFromArray(vGameNoteskins, L);
		return 1;
	}
	*/
	static int DoesNoteSkinExist( T* p, lua_State *L ) { lua_pushboolean(L, p->DoesNoteSkinExist( SArg(1) )); return 1; }

	LunaNoteSkinManager()
	{
		ADD_METHOD( GetPath );
		ADD_METHOD( GetMetric );
		ADD_METHOD( GetMetricI );
		ADD_METHOD( GetMetricF );
		ADD_METHOD( GetMetricB );
		ADD_METHOD( GetMetricA );
		ADD_METHOD( LoadActor );
		ADD_METHOD( GetPathForNoteSkin );
		ADD_METHOD( GetMetricForNoteSkin );
		ADD_METHOD( GetMetricIForNoteSkin );
		ADD_METHOD( GetMetricFForNoteSkin );
		ADD_METHOD( GetMetricBForNoteSkin );
		ADD_METHOD( GetMetricAForNoteSkin );
		ADD_METHOD( LoadActorForNoteSkin );
		ADD_METHOD( GetNoteSkinNames );
		ADD_METHOD( DoesNoteSkinExist ); // for the current game
	}
};

LUA_REGISTER_CLASS( NoteSkinManager )
// lua end

/*
 * (c) 2003-2004 Chris Danford
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
