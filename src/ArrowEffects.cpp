#include "global.h"
#include "ArrowEffects.h"
#include "Steps.h"
#include "GameConstantsAndTypes.h"
#include "GameManager.h"
#include "RageTimer.h"
#include "NoteDisplay.h"
#include "Song.h"
#include "RageMath.h"
#include "ScreenDimensions.h"
#include "PlayerState.h"
#include "GameState.h"
#include "Style.h"
#include "ThemeMetric.h"
#include <float.h>

static ThemeMetric<float>	ARROW_SPACING( "ArrowEffects", "ArrowSpacing" );
static ThemeMetric<bool>	QUANTIZE_ARROW_Y( "ArrowEffects", "QuantizeArrowYPosition");
static ThemeMetric<bool>	HIDDEN_SUDDEN_PAST_RECEPTOR( "ArrowEffects", "DrawHiddenNotesAfterReceptor");

/* For better or for worse, allow the themes to modify the various mod
 * effects for the different mods. In general, it is recommended to not
 * edit the default values and instead use percentage mods when changes
 * are wanted. Still, the option is available for those that want it.
 *
 * Is this a good idea? We'll find out. -aj & Wolfman2000 */
static ThemeMetric<float>	BLINK_MOD_FREQUENCY( "ArrowEffects", "BlinkModFrequency" );
static ThemeMetric<float>	BOOST_MOD_MIN_CLAMP( "ArrowEffects", "BoostModMinClamp" );
static ThemeMetric<float>	BOOST_MOD_MAX_CLAMP( "ArrowEffects", "BoostModMaxClamp" );
static ThemeMetric<float>	BRAKE_MOD_MIN_CLAMP( "ArrowEffects", "BrakeModMinClamp" );
static ThemeMetric<float>	BRAKE_MOD_MAX_CLAMP( "ArrowEffects", "BrakeModMaxClamp" );
static ThemeMetric<float>	WAVE_MOD_MAGNITUDE( "ArrowEffects", "WaveModMagnitude" );
static ThemeMetric<float>	WAVE_MOD_HEIGHT( "ArrowEffects", "WaveModHeight" );
static ThemeMetric<float>	BOOMERANG_PEAK_PERCENTAGE( "ArrowEffects", "BoomerangPeakPercentage" );
static ThemeMetric<float>	EXPAND_MULTIPLIER_FREQUENCY( "ArrowEffects", "ExpandMultiplierFrequency" );
static ThemeMetric<float>	EXPAND_MULTIPLIER_SCALE_FROM_LOW( "ArrowEffects", "ExpandMultiplierScaleFromLow" );
static ThemeMetric<float>	EXPAND_MULTIPLIER_SCALE_FROM_HIGH( "ArrowEffects", "ExpandMultiplierScaleFromHigh" );
static ThemeMetric<float>	EXPAND_MULTIPLIER_SCALE_TO_LOW( "ArrowEffects", "ExpandMultiplierScaleToLow" );
static ThemeMetric<float>	EXPAND_MULTIPLIER_SCALE_TO_HIGH( "ArrowEffects", "ExpandMultiplierScaleToHigh" );
static ThemeMetric<float>	EXPAND_SPEED_SCALE_FROM_LOW( "ArrowEffects", "ExpandSpeedScaleFromLow" );
static ThemeMetric<float>	EXPAND_SPEED_SCALE_FROM_HIGH( "ArrowEffects", "ExpandSpeedScaleFromHigh" );
static ThemeMetric<float>	EXPAND_SPEED_SCALE_TO_LOW( "ArrowEffects", "ExpandSpeedScaleToLow" );
static ThemeMetric<float>	TIPSY_TIMER_FREQUENCY( "ArrowEffects", "TipsyTimerFrequency" );
static ThemeMetric<float>	TIPSY_COLUMN_FREQUENCY( "ArrowEffects", "TipsyColumnFrequency" );
static ThemeMetric<float>	TIPSY_ARROW_MAGNITUDE( "ArrowEffects", "TipsyArrowMagnitude" );
static ThemeMetric<float>	TIPSY_OFFSET_TIMER_FREQUENCY( "ArrowEffects", "TipsyOffsetTimerFrequency" );
static ThemeMetric<float>	TIPSY_OFFSET_COLUMN_FREQUENCY( "ArrowEffects", "TipsyOffsetColumnFrequency" );
static ThemeMetric<float>	TIPSY_OFFSET_ARROW_MAGNITUDE( "ArrowEffects", "TipsyOffsetArrowMagnitude" );
static ThemeMetric<float>	TORNADO_POSITION_SCALE_TO_LOW( "ArrowEffects", "TornadoPositionScaleToLow" );
static ThemeMetric<float>	TORNADO_POSITION_SCALE_TO_HIGH( "ArrowEffects", "TornadoPositionScaleToHigh" );
static ThemeMetric<float>	TORNADO_OFFSET_FREQUENCY( "ArrowEffects", "TornadoOffsetFrequency" );
static ThemeMetric<float>	TORNADO_OFFSET_SCALE_FROM_LOW( "ArrowEffects", "TornadoOffsetScaleFromLow" );
static ThemeMetric<float>	TORNADO_OFFSET_SCALE_FROM_HIGH( "ArrowEffects", "TornadoOffsetScaleFromHigh" );
static ThemeMetric<float>	DRUNK_COLUMN_FREQUENCY( "ArrowEffects", "DrunkColumnFrequency" );
static ThemeMetric<float>	DRUNK_OFFSET_FREQUENCY( "ArrowEffects", "DrunkOffsetFrequency" );
static ThemeMetric<float>	DRUNK_ARROW_MAGNITUDE( "ArrowEffects", "DrunkArrowMagnitude" );
static ThemeMetric<float>	BEAT_OFFSET_HEIGHT( "ArrowEffects", "BeatOffsetHeight" );
static ThemeMetric<float>	BEAT_PI_HEIGHT( "ArrowEffects", "BeatPIHeight" );
static ThemeMetric<float>	TINY_PERCENT_BASE( "ArrowEffects", "TinyPercentBase" );
static ThemeMetric<float>	TINY_PERCENT_GATE( "ArrowEffects", "TinyPercentGate" );
static ThemeMetric<bool>	DIZZY_HOLD_HEADS( "ArrowEffects", "DizzyHoldHeads" );

float ArrowGetPercentVisible( const PlayerState* pPlayerState, float fYPosWithoutReverse );

static float GetNoteFieldHeight( const PlayerState* pPlayerState )
{
	return SCREEN_HEIGHT + fabsf(pPlayerState->m_PlayerOptions.GetCurrent().m_fPerspectiveTilt)*200;
}

namespace
{
	struct PerPlayerData
	{
		float m_fMinTornadoX[MAX_COLS_PER_PLAYER];
		float m_fMaxTornadoX[MAX_COLS_PER_PLAYER];
		float m_fInvertDistance[MAX_COLS_PER_PLAYER];
		float m_fBeatFactor;
		float m_fExpandSeconds;
	};
	PerPlayerData g_EffectData[NUM_PLAYERS];
};

void ArrowEffects::Update()
{
	const Style* pStyle = GAMESTATE->GetCurrentStyle();

	static float fLastTime = 0;
	float fTime = RageTimer::GetTimeSinceStartFast();
	
	FOREACH_EnabledPlayer( pn )
	{
		const Style::ColumnInfo* pCols = pStyle->m_ColumnInfo[pn];
		const SongPosition &position = GAMESTATE->m_bIsUsingStepTiming
		? GAMESTATE->m_pPlayerState[pn]->m_Position : GAMESTATE->m_Position;

		PerPlayerData &data = g_EffectData[pn];
		
		if( !position.m_bFreeze || !position.m_bDelay )
		{
			data.m_fExpandSeconds += fTime - fLastTime;
			data.m_fExpandSeconds = fmodf( data.m_fExpandSeconds, PI*2 );
		}
		
		// Update Tornado
		for( int iColNum = 0; iColNum < MAX_COLS_PER_PLAYER; ++iColNum )
		{
			// TRICKY: Tornado is very unplayable in doubles, so use a smaller
			// tornado width if there are many columns

			/* the below makes an assumption for dance mode.
			 * perhaps check if we are actually playing on singles without,
			 * say more than 6 columns. That would exclude IIDX, pop'n, and
			 * techno-8, all of which would be very hectic.
			 * certain non-singles modes (like halfdoubles 6cols)
			 * could possibly have tornado enabled.
			 * let's also take default resolution (640x480) into mind. -aj */
			bool bWideField = pStyle->m_iColsPerPlayer > 4;
			int iTornadoWidth = bWideField ? 2 : 3;

			int iStartCol = iColNum - iTornadoWidth;
			int iEndCol = iColNum + iTornadoWidth;
			CLAMP( iStartCol, 0, pStyle->m_iColsPerPlayer-1 );
			CLAMP( iEndCol, 0, pStyle->m_iColsPerPlayer-1 );

			data.m_fMinTornadoX[iColNum] = FLT_MAX;
			data.m_fMaxTornadoX[iColNum] = FLT_MIN;

			for( int i=iStartCol; i<=iEndCol; i++ )
			{
				data.m_fMinTornadoX[iColNum] = min( data.m_fMinTornadoX[iColNum], pCols[i].fXOffset );
				data.m_fMaxTornadoX[iColNum] = max( data.m_fMaxTornadoX[iColNum], pCols[i].fXOffset );
			}
		}

		// Update Invert
		for( int iColNum = 0; iColNum < MAX_COLS_PER_PLAYER; ++iColNum )
		{
			const int iNumCols = pStyle->m_iColsPerPlayer;
			const int iNumSides = (pStyle->m_StyleType==StyleType_OnePlayerTwoSides ||
					       pStyle->m_StyleType==StyleType_TwoPlayersSharedSides) ? 2 : 1;
			const int iNumColsPerSide = iNumCols / iNumSides;
			const int iSideIndex = iColNum / iNumColsPerSide;
			const int iColOnSide = iColNum % iNumColsPerSide;

			const int iColLeftOfMiddle = (iNumColsPerSide-1)/2;
			const int iColRightOfMiddle = (iNumColsPerSide+1)/2;

			int iFirstColOnSide = -1;
			int iLastColOnSide = -1;
			if( iColOnSide <= iColLeftOfMiddle )
			{
				iFirstColOnSide = 0;
				iLastColOnSide = iColLeftOfMiddle;
			}
			else if( iColOnSide >= iColRightOfMiddle )
			{
				iFirstColOnSide = iColRightOfMiddle;
				iLastColOnSide = iNumColsPerSide-1;
			}
			else
			{
				iFirstColOnSide = iColOnSide/2;
				iLastColOnSide = iColOnSide/2;
			}

			// mirror
			int iNewColOnSide;
			if( iFirstColOnSide == iLastColOnSide )
				iNewColOnSide = 0;
			else
				iNewColOnSide = SCALE( iColOnSide, iFirstColOnSide, iLastColOnSide, iLastColOnSide, iFirstColOnSide );
			const int iNewCol = iSideIndex*iNumColsPerSide + iNewColOnSide;

			const float fOldPixelOffset = pCols[iColNum].fXOffset;
			const float fNewPixelOffset = pCols[iNewCol].fXOffset;
			data.m_fInvertDistance[iColNum] = fNewPixelOffset - fOldPixelOffset;
		}

		// Update Beat
		do {
			float fAccelTime = 0.2f, fTotalTime = 0.5f;
			float fBeat = position.m_fSongBeatVisible + fAccelTime;

			const bool bEvenBeat = ( int(fBeat) % 2 ) != 0;

			data.m_fBeatFactor = 0;
			if( fBeat < 0 )
				break;

			// -100.2 -> -0.2 -> 0.2
			fBeat -= truncf( fBeat );
			fBeat += 1;
			fBeat -= truncf( fBeat );

			if( fBeat >= fTotalTime )
				break;

			if( fBeat < fAccelTime )
			{
				data.m_fBeatFactor = SCALE( fBeat, 0.0f, fAccelTime, 0.0f, 1.0f);
				data.m_fBeatFactor *= data.m_fBeatFactor;
			} else /* fBeat < fTotalTime */ {
				data.m_fBeatFactor = SCALE( fBeat, fAccelTime, fTotalTime, 1.0f, 0.0f);
				data.m_fBeatFactor = 1 - (1-data.m_fBeatFactor) * (1-data.m_fBeatFactor);
			}

			if( bEvenBeat )
				data.m_fBeatFactor *= -1;
			data.m_fBeatFactor *= 20.0f;
		} while( false );
	}
	fLastTime = fTime;
}

static float GetDisplayedBeat( const PlayerState* pPlayerState, float beat )
{
	// do a binary search here
	const vector<CacheDisplayedBeat> &data = pPlayerState->m_CacheDisplayedBeat;
	int max = data.size() - 1;
	int l = 0, r = max;
	while( l <= r )
	{
		int m = ( l + r ) / 2;
		if( ( m == 0 || data[m].beat <= beat ) && ( m == max || beat < data[m + 1].beat ) )
		{
			return data[m].displayedBeat + data[m].velocity * (beat - data[m].beat);
		}
		else if( data[m].beat <= beat )
		{
			l = m + 1;
		}
		else
		{
			r = m - 1;
		}
	}
	return beat;
}

/* For visibility testing: if bAbsolute is false, random modifiers must return
 * the minimum possible scroll speed. */
float ArrowEffects::GetYOffset( const PlayerState* pPlayerState, int iCol, float fNoteBeat, float &fPeakYOffsetOut, bool &bIsPastPeakOut, bool bAbsolute )
{
	// Default values that are returned if boomerang is off.
	fPeakYOffsetOut = FLT_MAX;
	bIsPastPeakOut = true;

	float fYOffset = 0;
	const SongPosition &position = pPlayerState->GetDisplayedPosition();
	
	float fSongBeat = position.m_fSongBeatVisible;
	
	Steps *pCurSteps = GAMESTATE->m_pCurSteps[pPlayerState->m_PlayerNumber];

	/* Usually, fTimeSpacing is 0 or 1, in which case we use entirely beat spacing or
	 * entirely time spacing (respectively). Occasionally, we tween between them. */
	if( pPlayerState->m_PlayerOptions.GetCurrent().m_fTimeSpacing != 1.0f )
	{
		if( GAMESTATE->m_bInStepEditor ) {
			// Use constant spacing in step editor
			fYOffset = fNoteBeat - fSongBeat;
		} else {
			fYOffset = GetDisplayedBeat(pPlayerState, fNoteBeat) - GetDisplayedBeat(pPlayerState, fSongBeat);
			fYOffset *= pCurSteps->GetTimingData()->GetDisplayedSpeedPercent(
								     position.m_fSongBeatVisible,
								     position.m_fMusicSecondsVisible );
		}
		fYOffset *= 1 - pPlayerState->m_PlayerOptions.GetCurrent().m_fTimeSpacing;
	}

	if( pPlayerState->m_PlayerOptions.GetCurrent().m_fTimeSpacing != 0.0f )
	{
		float fSongSeconds = GAMESTATE->m_Position.m_fMusicSecondsVisible;
		float fNoteSeconds = pCurSteps->GetTimingData()->GetElapsedTimeFromBeat(fNoteBeat);
		float fSecondsUntilStep = fNoteSeconds - fSongSeconds;
		float fBPM = pPlayerState->m_PlayerOptions.GetCurrent().m_fScrollBPM;
		float fBPS = fBPM/60.f;
		float fYOffsetTimeSpacing = fSecondsUntilStep * fBPS;
		fYOffset += fYOffsetTimeSpacing * pPlayerState->m_PlayerOptions.GetCurrent().m_fTimeSpacing;
	}

	// TODO: If we allow noteskins to have metricable row spacing
	// (per issue 24), edit this to reflect that. -aj
	fYOffset *= ARROW_SPACING;
	
	// don't mess with the arrows after they've crossed 0
	if( fYOffset < 0 )
		return fYOffset * pPlayerState->m_PlayerOptions.GetCurrent().m_fScrollSpeed;

	const float* fAccels = pPlayerState->m_PlayerOptions.GetCurrent().m_fAccels;
	//const float* fEffects = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects;

	float fYAdjust = 0;	// fill this in depending on PlayerOptions

	if( fAccels[PlayerOptions::ACCEL_BOOST] != 0 )
	{
		float fEffectHeight = GetNoteFieldHeight(pPlayerState);
		float fNewYOffset = fYOffset * 1.5f / ((fYOffset+fEffectHeight/1.2f)/fEffectHeight); 
		float fAccelYAdjust =	fAccels[PlayerOptions::ACCEL_BOOST] * (fNewYOffset - fYOffset);
		// TRICKY: Clamp this value, or else BOOST+BOOMERANG will draw a ton of arrows on the screen.
		CLAMP( fAccelYAdjust, BOOST_MOD_MIN_CLAMP, BOOST_MOD_MAX_CLAMP );
		fYAdjust += fAccelYAdjust;
	}
	if( fAccels[PlayerOptions::ACCEL_BRAKE] != 0 )
	{
		float fEffectHeight = GetNoteFieldHeight(pPlayerState);
		float fScale = SCALE( fYOffset, 0.f, fEffectHeight, 0, 1.f );
		float fNewYOffset = fYOffset * fScale; 
		float fBrakeYAdjust = fAccels[PlayerOptions::ACCEL_BRAKE] * (fNewYOffset - fYOffset);
		// TRICKY: Clamp this value the same way as BOOST so that in BOOST+BRAKE, BRAKE doesn't overpower BOOST
		CLAMP( fBrakeYAdjust, BRAKE_MOD_MIN_CLAMP, BRAKE_MOD_MAX_CLAMP );
		fYAdjust += fBrakeYAdjust;
	}
	if( fAccels[PlayerOptions::ACCEL_WAVE] != 0 )
		fYAdjust +=	fAccels[PlayerOptions::ACCEL_WAVE] * WAVE_MOD_MAGNITUDE *RageFastSin( fYOffset/WAVE_MOD_HEIGHT );

	fYOffset += fYAdjust;

	// Factor in boomerang
	if( fAccels[PlayerOptions::ACCEL_BOOMERANG] != 0 )
	{
		float fPeakAtYOffset = SCREEN_HEIGHT * BOOMERANG_PEAK_PERCENTAGE;	// zero point of boomerang function
		fPeakYOffsetOut = (-1*fPeakAtYOffset*fPeakAtYOffset/SCREEN_HEIGHT) + 1.5f*fPeakAtYOffset;
		bIsPastPeakOut = fYOffset < fPeakAtYOffset;

		fYOffset = (-1*fYOffset*fYOffset/SCREEN_HEIGHT) + 1.5f*fYOffset;
	}

	// Factor in scroll speed
	float fScrollSpeed = pPlayerState->m_PlayerOptions.GetCurrent().m_fScrollSpeed;
	
	if( pPlayerState->m_PlayerOptions.GetCurrent().m_fRandomSpeed > 0 && !bAbsolute )
	{
		// Generate a deterministically "random" speed for each arrow.
		unsigned seed = GAMESTATE->m_iStageSeed + ( BeatToNoteRow( fNoteBeat ) << 8 ) + (iCol * 100);

		for( int i = 0; i < 3; ++i )
			seed = ((seed * 1664525u) + 1013904223u) & 0xFFFFFFFF;
		float fRandom = seed / 4294967296.0f;

		/* Random speed always increases speed: a random speed of 10 indicates
		 * [1,11]. This keeps it consistent with other mods: 0 means no effect. */
		fScrollSpeed *=
				SCALE( fRandom,
						0.0f, 1.0f,
						1.0f, pPlayerState->m_PlayerOptions.GetCurrent().m_fRandomSpeed + 1.0f );
	}

	if( fAccels[PlayerOptions::ACCEL_EXPAND] != 0 )
	{
		// TODO: Don't index by PlayerNumber.
		PerPlayerData &data = g_EffectData[pPlayerState->m_PlayerNumber];
	
		float fExpandMultiplier = SCALE( RageFastCos(data.m_fExpandSeconds*EXPAND_MULTIPLIER_FREQUENCY), 
						EXPAND_MULTIPLIER_SCALE_FROM_LOW, EXPAND_MULTIPLIER_SCALE_FROM_HIGH,
						EXPAND_MULTIPLIER_SCALE_TO_LOW, EXPAND_MULTIPLIER_SCALE_TO_HIGH );
		fScrollSpeed *=	SCALE( fAccels[PlayerOptions::ACCEL_EXPAND], 
				      EXPAND_SPEED_SCALE_FROM_LOW, EXPAND_SPEED_SCALE_FROM_HIGH,
				      EXPAND_SPEED_SCALE_TO_LOW, fExpandMultiplier );
	}

	fYOffset *= fScrollSpeed;
	fPeakYOffsetOut *= fScrollSpeed;

	return fYOffset;
}

static void ArrowGetReverseShiftAndScale( const PlayerState* pPlayerState, int iCol, float fYReverseOffsetPixels, float &fShiftOut, float &fScaleOut )
{
	// XXX: Hack: we need to scale the reverse shift by the zoom.
	float fMiniPercent = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects[PlayerOptions::EFFECT_MINI];
	float fZoom = 1 - fMiniPercent*0.5f;

	// don't divide by 0
	if( fabsf(fZoom) < 0.01 )
		fZoom = 0.01f;

	float fPercentReverse = pPlayerState->m_PlayerOptions.GetCurrent().GetReversePercentForColumn(iCol);
	fShiftOut = SCALE( fPercentReverse, 0.f, 1.f, -fYReverseOffsetPixels/fZoom/2, fYReverseOffsetPixels/fZoom/2 );
	float fPercentCentered = pPlayerState->m_PlayerOptions.GetCurrent().m_fScrolls[PlayerOptions::SCROLL_CENTERED];
	fShiftOut = SCALE( fPercentCentered, 0.f, 1.f, fShiftOut, 0.0f );

	fScaleOut = SCALE( fPercentReverse, 0.f, 1.f, 1.f, -1.f );
}

float ArrowEffects::GetYPos( const PlayerState* pPlayerState, int iCol, float fYOffset, float fYReverseOffsetPixels, bool WithReverse )
{
	float f = fYOffset;

	if( WithReverse )
	{
		float fShift, fScale;
		ArrowGetReverseShiftAndScale( pPlayerState, iCol, fYReverseOffsetPixels, fShift, fScale );

		f *= fScale;
		f += fShift;
	}

	const float* fEffects = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects;

	if( fEffects[PlayerOptions::EFFECT_TIPSY] != 0 )
		f += fEffects[PlayerOptions::EFFECT_TIPSY] 
			* ( RageFastCos( RageTimer::GetTimeSinceStartFast()*TIPSY_TIMER_FREQUENCY 
					+ iCol*TIPSY_COLUMN_FREQUENCY) * ARROW_SIZE*TIPSY_ARROW_MAGNITUDE );

	// In beware's DDR Extreme-focused fork of StepMania 3.9, this value is
	// floored, making arrows show on integer Y coordinates. Supposedly it makes
	// the arrows look better, but testing needs to be done.
	// todo: make this a noteskin metric instead of a theme metric? -aj
	return QUANTIZE_ARROW_Y ? floor(f) : f;
}

float ArrowEffects::GetYOffsetFromYPos( const PlayerState* pPlayerState, int iCol, float YPos, float fYReverseOffsetPixels )
{
	float f = YPos;

	const float* fEffects = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects;
	if( fEffects[PlayerOptions::EFFECT_TIPSY] != 0 )
		f -= fEffects[PlayerOptions::EFFECT_TIPSY] 
			* ( RageFastCos( RageTimer::GetTimeSinceStartFast()*TIPSY_OFFSET_TIMER_FREQUENCY 
					+ iCol*TIPSY_OFFSET_COLUMN_FREQUENCY) * ARROW_SIZE*TIPSY_OFFSET_ARROW_MAGNITUDE );

	float fShift, fScale;
	ArrowGetReverseShiftAndScale( pPlayerState, iCol, fYReverseOffsetPixels, fShift, fScale );

	f -= fShift;
	if( fScale )
		f /= fScale;

	return f;
}

float ArrowEffects::GetXPos( const PlayerState* pPlayerState, int iColNum, float fYOffset ) 
{
	float fPixelOffsetFromCenter = 0; // fill this in below

	const Style* pStyle = GAMESTATE->GetCurrentStyle();
	const float* fEffects = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects;

	// TODO: Don't index by PlayerNumber.
	const Style::ColumnInfo* pCols = pStyle->m_ColumnInfo[pPlayerState->m_PlayerNumber];
	PerPlayerData &data = g_EffectData[pPlayerState->m_PlayerNumber];

	if( fEffects[PlayerOptions::EFFECT_TORNADO] != 0 )
	{
		const float fRealPixelOffset = pCols[iColNum].fXOffset;
		const float fPositionBetween = SCALE( fRealPixelOffset, data.m_fMinTornadoX[iColNum], data.m_fMaxTornadoX[iColNum], 
						     TORNADO_POSITION_SCALE_TO_LOW, TORNADO_POSITION_SCALE_TO_HIGH );
		float fRads = acosf( fPositionBetween );
		fRads += fYOffset * TORNADO_OFFSET_FREQUENCY / SCREEN_HEIGHT;

		const float fAdjustedPixelOffset = SCALE( RageFastCos(fRads), TORNADO_OFFSET_SCALE_FROM_LOW, TORNADO_OFFSET_SCALE_FROM_HIGH, 
							 data.m_fMinTornadoX[iColNum], data.m_fMaxTornadoX[iColNum] );

		fPixelOffsetFromCenter += (fAdjustedPixelOffset - fRealPixelOffset) * fEffects[PlayerOptions::EFFECT_TORNADO];
	}

	if( fEffects[PlayerOptions::EFFECT_DRUNK] != 0 )
		fPixelOffsetFromCenter += fEffects[PlayerOptions::EFFECT_DRUNK] * 
			( RageFastCos( RageTimer::GetTimeSinceStartFast() + iColNum*DRUNK_COLUMN_FREQUENCY
				      + fYOffset*DRUNK_OFFSET_FREQUENCY/SCREEN_HEIGHT) * ARROW_SIZE*DRUNK_ARROW_MAGNITUDE );
	if( fEffects[PlayerOptions::EFFECT_FLIP] != 0 )
	{
		const int iFirstCol = 0;
		const int iLastCol = pStyle->m_iColsPerPlayer-1;
		const int iNewCol = SCALE( iColNum, iFirstCol, iLastCol, iLastCol, iFirstCol );
		const float fOldPixelOffset = pCols[iColNum].fXOffset;
		const float fNewPixelOffset = pCols[iNewCol].fXOffset;
		const float fDistance = fNewPixelOffset - fOldPixelOffset;
		fPixelOffsetFromCenter += fDistance * fEffects[PlayerOptions::EFFECT_FLIP];
	}
	if( fEffects[PlayerOptions::EFFECT_INVERT] != 0 )
		fPixelOffsetFromCenter += data.m_fInvertDistance[iColNum] * fEffects[PlayerOptions::EFFECT_INVERT];

	if( fEffects[PlayerOptions::EFFECT_BEAT] != 0 )
	{
		const float fShift = data.m_fBeatFactor*RageFastSin( fYOffset / BEAT_OFFSET_HEIGHT + PI/BEAT_PI_HEIGHT );
		fPixelOffsetFromCenter += fEffects[PlayerOptions::EFFECT_BEAT] * fShift;
	}

	if( fEffects[PlayerOptions::EFFECT_XMODE] != 0 )
	{
		// based off of code by v1toko for StepNXA, except it should work on
		// any gametype now.
		switch( pStyle->m_StyleType )
		{
			case StyleType_OnePlayerTwoSides:
			case StyleType_TwoPlayersSharedSides: // fall through?
				{
					// find the middle, and split based on iColNum
					// it's unknown if this will work for routine.
					const int iMiddleColumn = static_cast<int>(floor(pStyle->m_iColsPerPlayer/2.0f));
					if( iColNum > iMiddleColumn-1 )
						fPixelOffsetFromCenter += fEffects[PlayerOptions::EFFECT_XMODE]*-(fYOffset);
					else
						fPixelOffsetFromCenter += fEffects[PlayerOptions::EFFECT_XMODE]*fYOffset;
				}
				break;
			case StyleType_OnePlayerOneSide:
			case StyleType_TwoPlayersTwoSides: // fall through
				{
					// the code was the same for both of these cases in StepNXA.
					if( pPlayerState->m_PlayerNumber == PLAYER_2 )
						fPixelOffsetFromCenter += fEffects[PlayerOptions::EFFECT_XMODE]*-(fYOffset);
					else
						fPixelOffsetFromCenter += fEffects[PlayerOptions::EFFECT_XMODE]*fYOffset;
				}
				break;
			DEFAULT_FAIL(pStyle->m_StyleType);
		}
	}

	fPixelOffsetFromCenter += pCols[iColNum].fXOffset;

	if( fEffects[PlayerOptions::EFFECT_TINY] != 0 )
	{
		// Allow Tiny to pull tracks together, but not to push them apart.
		float fTinyPercent = fEffects[PlayerOptions::EFFECT_TINY];
		fTinyPercent = min( powf(TINY_PERCENT_BASE, fTinyPercent), (float)TINY_PERCENT_GATE );
		fPixelOffsetFromCenter *= fTinyPercent;
	}

	return fPixelOffsetFromCenter;
}

float ArrowEffects::GetRotationX( const PlayerState *pPlayerState, float fYOffset )
{
	const float* fEffects = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects;
	float fRotation = 0;
	if( fEffects[PlayerOptions::EFFECT_ROLL] != 0 )
	{
		fRotation = fEffects[PlayerOptions::EFFECT_ROLL] * fYOffset/2;
	}
	return fRotation;
}

float ArrowEffects::GetRotationY( const PlayerState *pPlayerState, float fYOffset )
{
	const float* fEffects = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects;
	float fRotation = 0;
	if( fEffects[PlayerOptions::EFFECT_TWIRL] != 0 )
	{
		fRotation = fEffects[PlayerOptions::EFFECT_TWIRL] * fYOffset/2;
	}
	return fRotation;
}

float ArrowEffects::GetRotationZ( const PlayerState* pPlayerState, float fNoteBeat, bool bIsHoldHead ) 
{
	const float* fEffects = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects;
	float fRotation = 0;
	if( fEffects[PlayerOptions::EFFECT_CONFUSION] != 0 )
		fRotation += ReceptorGetRotationZ( pPlayerState );

	// As usual, enable dizzy hold heads at your own risk. -Wolfman2000
	if( fEffects[PlayerOptions::EFFECT_DIZZY] != 0 && ( DIZZY_HOLD_HEADS || !bIsHoldHead ) )
	{
		const float fSongBeat = pPlayerState->m_Position.m_fSongBeatVisible;
		float fDizzyRotation = fNoteBeat - fSongBeat;
		fDizzyRotation *= fEffects[PlayerOptions::EFFECT_DIZZY];
		fDizzyRotation = fmodf( fDizzyRotation, 2*PI );
		fDizzyRotation *= 180/PI;
		fRotation += fDizzyRotation;
	}
	return fRotation;
}

float ArrowEffects::ReceptorGetRotationZ( const PlayerState* pPlayerState ) 
{
	const float* fEffects = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects;
	float fRotation = 0;

	if( fEffects[PlayerOptions::EFFECT_CONFUSION] != 0 )
	{
		float fConfRotation = pPlayerState->m_Position.m_fSongBeatVisible;
		fConfRotation *= fEffects[PlayerOptions::EFFECT_CONFUSION];
		fConfRotation = fmodf( fConfRotation, 2*PI );
		fConfRotation *= -180/PI;
		fRotation += fConfRotation;
	}
	return fRotation;
}

#define CENTER_LINE_Y 160	// from fYOffset == 0
#define FADE_DIST_Y 40

static float GetCenterLine( const PlayerState* pPlayerState )
{
	/* Another mini hack: if EFFECT_MINI is on, then our center line is at
	 * eg. 320, not 160. */
	const float fMiniPercent = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects[PlayerOptions::EFFECT_MINI];
	const float fZoom = 1 - fMiniPercent*0.5f;
	return CENTER_LINE_Y / fZoom;
}

static float GetHiddenSudden( const PlayerState* pPlayerState ) 
{
	const float* fAppearances = pPlayerState->m_PlayerOptions.GetCurrent().m_fAppearances;
	return fAppearances[PlayerOptions::APPEARANCE_HIDDEN] *
		fAppearances[PlayerOptions::APPEARANCE_SUDDEN];
}

//
//  -gray arrows-
// 
//  ...invisible...
//  -hidden end line-
//  -hidden start line-
//  ...visible...
//  -sudden end line-
//  -sudden start line-
//  ...invisible...
//
// TRICKY:  We fudge hidden and sudden to be farther apart if they're both on.
static float GetHiddenEndLine( const PlayerState* pPlayerState )
{
	return GetCenterLine( pPlayerState ) + 
		FADE_DIST_Y * SCALE( GetHiddenSudden(pPlayerState), 0.f, 1.f, -1.0f, -1.25f ) + 
		GetCenterLine( pPlayerState ) * pPlayerState->m_PlayerOptions.GetCurrent().m_fAppearances[PlayerOptions::APPEARANCE_HIDDEN_OFFSET];
}

static float GetHiddenStartLine( const PlayerState* pPlayerState )
{
	return GetCenterLine( pPlayerState ) + 
		FADE_DIST_Y * SCALE( GetHiddenSudden(pPlayerState), 0.f, 1.f, +0.0f, -0.25f ) + 
		GetCenterLine( pPlayerState ) * pPlayerState->m_PlayerOptions.GetCurrent().m_fAppearances[PlayerOptions::APPEARANCE_HIDDEN_OFFSET];
}

static float GetSuddenEndLine( const PlayerState* pPlayerState )
{
	return GetCenterLine( pPlayerState ) + 
		FADE_DIST_Y * SCALE( GetHiddenSudden(pPlayerState), 0.f, 1.f, -0.0f, +0.25f ) + 
		GetCenterLine( pPlayerState ) * pPlayerState->m_PlayerOptions.GetCurrent().m_fAppearances[PlayerOptions::APPEARANCE_SUDDEN_OFFSET];
}

static float GetSuddenStartLine( const PlayerState* pPlayerState )
{
	return GetCenterLine( pPlayerState ) + 
		FADE_DIST_Y * SCALE( GetHiddenSudden(pPlayerState), 0.f, 1.f, +1.0f, +1.25f ) + 
		GetCenterLine( pPlayerState ) * pPlayerState->m_PlayerOptions.GetCurrent().m_fAppearances[PlayerOptions::APPEARANCE_SUDDEN_OFFSET];
}

// used by ArrowGetAlpha and ArrowGetGlow below
float ArrowGetPercentVisible( const PlayerState* pPlayerState, float fYPosWithoutReverse )
{
	const float fDistFromCenterLine = fYPosWithoutReverse - GetCenterLine( pPlayerState );

	if( fYPosWithoutReverse < 0 && HIDDEN_SUDDEN_PAST_RECEPTOR)	// past Gray Arrows
		return 1;	// totally visible

	const float* fAppearances = pPlayerState->m_PlayerOptions.GetCurrent().m_fAppearances;

	float fVisibleAdjust = 0;

	if( fAppearances[PlayerOptions::APPEARANCE_HIDDEN] != 0 )
	{
		float fHiddenVisibleAdjust = SCALE( fYPosWithoutReverse, GetHiddenStartLine(pPlayerState), GetHiddenEndLine(pPlayerState), 0, -1 );
		CLAMP( fHiddenVisibleAdjust, -1, 0 );
		fVisibleAdjust += fAppearances[PlayerOptions::APPEARANCE_HIDDEN] * fHiddenVisibleAdjust;
	}
	if( fAppearances[PlayerOptions::APPEARANCE_SUDDEN] != 0 )
	{
		float fSuddenVisibleAdjust = SCALE( fYPosWithoutReverse, GetSuddenStartLine(pPlayerState), GetSuddenEndLine(pPlayerState), -1, 0 );
		CLAMP( fSuddenVisibleAdjust, -1, 0 );
		fVisibleAdjust += fAppearances[PlayerOptions::APPEARANCE_SUDDEN] * fSuddenVisibleAdjust;
	}

	if( fAppearances[PlayerOptions::APPEARANCE_STEALTH] != 0 )
		fVisibleAdjust -= fAppearances[PlayerOptions::APPEARANCE_STEALTH];
	if( fAppearances[PlayerOptions::APPEARANCE_BLINK] != 0 )
	{
		float f = RageFastSin(RageTimer::GetTimeSinceStartFast()*10);
		f = Quantize( f, BLINK_MOD_FREQUENCY );
		fVisibleAdjust += SCALE( f, 0, 1, -1, 0 );
	}
	if( fAppearances[PlayerOptions::APPEARANCE_RANDOMVANISH] != 0 )
	{
		const float fRealFadeDist = 80;
		fVisibleAdjust += SCALE( fabsf(fDistFromCenterLine), fRealFadeDist, 2*fRealFadeDist, -1, 0 )
			* fAppearances[PlayerOptions::APPEARANCE_RANDOMVANISH];
	}

	return clamp( 1+fVisibleAdjust, 0, 1 );
}

float ArrowEffects::GetAlpha( const PlayerState* pPlayerState, int iCol, float fYOffset, float fPercentFadeToFail, float fYReverseOffsetPixels, float fDrawDistanceBeforeTargetsPixels, float fFadeInPercentOfDrawFar )
{
	// Get the YPos without reverse (that is, factor in EFFECT_TIPSY).
	float fYPosWithoutReverse = ArrowEffects::GetYPos( pPlayerState, iCol, fYOffset, fYReverseOffsetPixels, false );

	float fPercentVisible = ArrowGetPercentVisible( pPlayerState, fYPosWithoutReverse );

	if( fPercentFadeToFail != -1 )
		fPercentVisible = 1 - fPercentFadeToFail;


	float fFullAlphaY = fDrawDistanceBeforeTargetsPixels*(1-fFadeInPercentOfDrawFar);
	if( fYPosWithoutReverse > fFullAlphaY )
	{
		float f = SCALE( fYPosWithoutReverse, fFullAlphaY, fDrawDistanceBeforeTargetsPixels, 1.0f, 0.0f );
		return f;
	}


	return (fPercentVisible>0.5f) ? 1.0f : 0.0f;
}

float ArrowEffects::GetGlow( const PlayerState* pPlayerState, int iCol, float fYOffset, float fPercentFadeToFail, float fYReverseOffsetPixels, float fDrawDistanceBeforeTargetsPixels, float fFadeInPercentOfDrawFar )
{
	// Get the YPos without reverse (that is, factor in EFFECT_TIPSY).
	float fYPosWithoutReverse = ArrowEffects::GetYPos( pPlayerState, iCol, fYOffset, fYReverseOffsetPixels, false );

	float fPercentVisible = ArrowGetPercentVisible( pPlayerState, fYPosWithoutReverse );

	if( fPercentFadeToFail != -1 )
		fPercentVisible = 1 - fPercentFadeToFail;

	const float fDistFromHalf = fabsf( fPercentVisible - 0.5f );
	return SCALE( fDistFromHalf, 0, 0.5f, 1.3f, 0 );
}

float ArrowEffects::GetBrightness( const PlayerState* pPlayerState, float fNoteBeat )
{
	if( GAMESTATE->IsEditing() )
		return 1;

	float fSongBeat = pPlayerState->m_Position.m_fSongBeatVisible;
	float fBeatsUntilStep = fNoteBeat - fSongBeat;

	float fBrightness = SCALE( fBeatsUntilStep, 0, -1, 1.f, 0.f );
	CLAMP( fBrightness, 0, 1 );
	return fBrightness;
}


float ArrowEffects::GetZPos( const PlayerState* pPlayerState, int iCol, float fYOffset )
{
	float fZPos=0;
	const float* fEffects = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects;

	if( fEffects[PlayerOptions::EFFECT_BUMPY] != 0 )
		fZPos += fEffects[PlayerOptions::EFFECT_BUMPY] * 40*RageFastSin( fYOffset/16.0f );

	return fZPos;
}

bool ArrowEffects::NeedZBuffer( const PlayerState* pPlayerState )
{
	const float* fEffects = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects;
	// We also need to use the Z buffer if twirl is in play, because of
	// hold modulation. -vyhd (OpenITG r623)
	if( fEffects[PlayerOptions::EFFECT_BUMPY] != 0 ||
		fEffects[PlayerOptions::EFFECT_TWIRL] != 0 )
		return true;

	return false;
}

float ArrowEffects::GetZoom( const PlayerState* pPlayerState )
{
	float fZoom = 1.0f;
	// FIXME: Move the zoom values into Style
	if( GAMESTATE->GetCurrentStyle()->m_bNeedsZoomOutWith2Players &&
		(GAMESTATE->GetNumSidesJoined()==2 || GAMESTATE->AnyPlayersAreCpu()) )
		fZoom *= 0.6f;

	float fTinyPercent = pPlayerState->m_PlayerOptions.GetCurrent().m_fEffects[PlayerOptions::EFFECT_TINY];
	if( fTinyPercent != 0 )
	{
		fTinyPercent = powf( 0.5f, fTinyPercent );
		fZoom *= fTinyPercent;
	}
	return fZoom;
}

static ThemeMetric<float>	FRAME_WIDTH_EFFECTS_PIXELS_PER_SECOND( "ArrowEffects", "FrameWidthEffectsPixelsPerSecond" );
static ThemeMetric<float>	FRAME_WIDTH_EFFECTS_MIN_MULTIPLIER( "ArrowEffects", "FrameWidthEffectsMinMultiplier" );
static ThemeMetric<float>	FRAME_WIDTH_EFFECTS_MAX_MULTIPLIER( "ArrowEffects", "FrameWidthEffectsMaxMultiplier" );
static ThemeMetric<bool>	FRAME_WIDTH_LOCK_EFFECTS_TO_OVERLAPPING( "ArrowEffects", "FrameWidthLockEffectsToOverlapping" );
static ThemeMetric<float>	FRAME_WIDTH_LOCK_EFFECTS_TWEEN_PIXELS( "ArrowEffects", "FrameWidthLockEffectsTweenPixels" );

float ArrowEffects::GetFrameWidthScale( const PlayerState* pPlayerState, float fYOffset, float fOverlappedTime )
{
	float fFrameWidthMultiplier = 1.0f;

	float fPixelsPerSecond = FRAME_WIDTH_EFFECTS_PIXELS_PER_SECOND;
	float fSecond = fYOffset / fPixelsPerSecond;
	float fWidthEffect = pPlayerState->m_EffectHistory.GetSample( fSecond );
	if( fWidthEffect != 0 && FRAME_WIDTH_LOCK_EFFECTS_TO_OVERLAPPING )
	{
		// Don't display effect data that happened before this hold overlapped the top.
		float fFromEndOfOverlapped = fOverlappedTime - fSecond;
		float fTrailingPixels = FRAME_WIDTH_LOCK_EFFECTS_TWEEN_PIXELS;
		float fTrailingSeconds = fTrailingPixels / fPixelsPerSecond;
		float fScaleEffect = SCALE( fFromEndOfOverlapped, 0.0f, fTrailingSeconds, 0.0f, 1.0f );
		CLAMP( fScaleEffect, 0.0f, 1.0f );
		fWidthEffect *= fScaleEffect;
	}

	if( fWidthEffect > 0 )
		fFrameWidthMultiplier *= SCALE( fWidthEffect, 0.0f, 1.0f, 1.0f, FRAME_WIDTH_EFFECTS_MAX_MULTIPLIER );
	else if( fWidthEffect < 0 )
		fFrameWidthMultiplier *= SCALE( fWidthEffect, 0.0f, -1.0f, 1.0f, FRAME_WIDTH_EFFECTS_MIN_MULTIPLIER );

	return fFrameWidthMultiplier;
}

/*
 * (c) 2001-2004 Chris Danford
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
