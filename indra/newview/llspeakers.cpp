/** 
 * @file llspeakers.cpp
 * @brief Management interface for muting and controlling volume of residents currently speaking
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llspeakers.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llmutelist.h"
#include "llsdutil.h"
#include "lluicolortable.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "llworld.h"

const F32 SPEAKER_TIMEOUT = 10.f; // seconds of not being on voice channel before removed from list of active speakers
const LLColor4 INACTIVE_COLOR(0.3f, 0.3f, 0.3f, 0.5f);
const LLColor4 ACTIVE_COLOR(0.5f, 0.5f, 0.5f, 1.f);

LLSpeaker::LLSpeaker(const LLUUID& id, const std::string& name, const ESpeakerType type) : 
	mStatus(LLSpeaker::STATUS_TEXT_ONLY),
	mLastSpokeTime(0.f), 
	mSpeechVolume(0.f), 
	mHasSpoken(FALSE),
	mHasLeftCurrentCall(FALSE),
	mDotColor(LLColor4::white),
	mID(id),
	mTyping(FALSE),
	mSortIndex(0),
	mType(type),
	mIsModerator(FALSE),
	mModeratorMutedVoice(FALSE),
	mModeratorMutedText(FALSE)
{
	if (name.empty() && type == SPEAKER_AGENT)
	{
		lookupName();
	}
	else
	{
		mDisplayName = name;
	}

	gVoiceClient->setUserVolume(id, LLMuteList::getInstance()->getSavedResidentVolume(id));

	mActivityTimer.resetWithExpiry(SPEAKER_TIMEOUT);
}


void LLSpeaker::lookupName()
{
	gCacheName->get(mID, FALSE, boost::bind(&LLSpeaker::onAvatarNameLookup, this, _1, _2, _3, _4));
}

void LLSpeaker::onAvatarNameLookup(const LLUUID& id, const std::string& first, const std::string& last, BOOL is_group)
{
	mDisplayName = first + " " + last;
}

LLSpeakerUpdateModeratorEvent::LLSpeakerUpdateModeratorEvent(LLSpeaker* source)
: LLEvent(source, "Speaker add moderator event"),
  mSpeakerID (source->mID),
  mIsModerator (source->mIsModerator)
{
}

LLSD LLSpeakerUpdateModeratorEvent::getValue()
{
	LLSD ret;
	ret["id"] = mSpeakerID;
	ret["is_moderator"] = mIsModerator;
	return ret;
}

LLSpeakerTextModerationEvent::LLSpeakerTextModerationEvent(LLSpeaker* source)
: LLEvent(source, "Speaker text moderation event")
{
}

LLSD LLSpeakerTextModerationEvent::getValue()
{
	return std::string("text");
}


LLSpeakerVoiceModerationEvent::LLSpeakerVoiceModerationEvent(LLSpeaker* source)
: LLEvent(source, "Speaker voice moderation event")
{
}

LLSD LLSpeakerVoiceModerationEvent::getValue()
{
	return std::string("voice");
}

LLSpeakerListChangeEvent::LLSpeakerListChangeEvent(LLSpeakerMgr* source, const LLUUID& speaker_id)
: LLEvent(source, "Speaker added/removed from speaker mgr"),
  mSpeakerID(speaker_id)
{
}

LLSD LLSpeakerListChangeEvent::getValue()
{
	return mSpeakerID;
}

// helper sort class
struct LLSortRecentSpeakers
{
	bool operator()(const LLPointer<LLSpeaker> lhs, const LLPointer<LLSpeaker> rhs) const;
};

bool LLSortRecentSpeakers::operator()(const LLPointer<LLSpeaker> lhs, const LLPointer<LLSpeaker> rhs) const
{
	// Sort first on status
	if (lhs->mStatus != rhs->mStatus) 
	{
		return (lhs->mStatus < rhs->mStatus);
	}

	// and then on last speaking time
	if(lhs->mLastSpokeTime != rhs->mLastSpokeTime)
	{
		return (lhs->mLastSpokeTime > rhs->mLastSpokeTime);
	}
	
	// and finally (only if those are both equal), on name.
	return(	lhs->mDisplayName.compare(rhs->mDisplayName) < 0 );
}


//
// LLSpeakerMgr
//

LLSpeakerMgr::LLSpeakerMgr(LLVoiceChannel* channelp) : 
	mVoiceChannel(channelp)
{
}

LLSpeakerMgr::~LLSpeakerMgr()
{
}

LLPointer<LLSpeaker> LLSpeakerMgr::setSpeaker(const LLUUID& id, const std::string& name, LLSpeaker::ESpeakerStatus status, LLSpeaker::ESpeakerType type)
{
	if (id.isNull()) return NULL;

	LLPointer<LLSpeaker> speakerp;
	if (mSpeakers.find(id) == mSpeakers.end())
	{
		speakerp = new LLSpeaker(id, name, type);
		speakerp->mStatus = status;
		mSpeakers.insert(std::make_pair(speakerp->mID, speakerp));
		mSpeakersSorted.push_back(speakerp);
		fireEvent(new LLSpeakerListChangeEvent(this, speakerp->mID), "add");
	}
	else
	{
		speakerp = findSpeaker(id);
		if (speakerp.notNull())
		{
			// keep highest priority status (lowest value) instead of overriding current value
			speakerp->mStatus = llmin(speakerp->mStatus, status);
			speakerp->mActivityTimer.resetWithExpiry(SPEAKER_TIMEOUT);
			// RN: due to a weird behavior where IMs from attached objects come from the wearer's agent_id
			// we need to override speakers that we think are objects when we find out they are really
			// residents
			if (type == LLSpeaker::SPEAKER_AGENT)
			{
				speakerp->mType = LLSpeaker::SPEAKER_AGENT;
				speakerp->lookupName();
			}
		}
	}

	return speakerp;
}

void LLSpeakerMgr::update(BOOL resort_ok)
{
	if (!gVoiceClient)
	{
		return;
	}
	
	LLColor4 speaking_color = LLUIColorTable::instance().getColor("SpeakingColor");
	LLColor4 overdriven_color = LLUIColorTable::instance().getColor("OverdrivenColor");

	if(resort_ok) // only allow list changes when user is not interacting with it
	{
		updateSpeakerList();
	}

	// update status of all current speakers
	BOOL voice_channel_active = (!mVoiceChannel && gVoiceClient->inProximalChannel()) || (mVoiceChannel && mVoiceChannel->isActive());
	for (speaker_map_t::iterator speaker_it = mSpeakers.begin(); speaker_it != mSpeakers.end();)
	{
		LLUUID speaker_id = speaker_it->first;
		LLSpeaker* speakerp = speaker_it->second;
		
		speaker_map_t::iterator  cur_speaker_it = speaker_it++;

		if (voice_channel_active && gVoiceClient->getVoiceEnabled(speaker_id))
		{
			speakerp->mSpeechVolume = gVoiceClient->getCurrentPower(speaker_id);
			BOOL moderator_muted_voice = gVoiceClient->getIsModeratorMuted(speaker_id);
			if (moderator_muted_voice != speakerp->mModeratorMutedVoice)
			{
				speakerp->mModeratorMutedVoice = moderator_muted_voice;
				speakerp->fireEvent(new LLSpeakerVoiceModerationEvent(speakerp));
			}

			if (gVoiceClient->getOnMuteList(speaker_id) || speakerp->mModeratorMutedVoice)
			{
				speakerp->mStatus = LLSpeaker::STATUS_MUTED;
			}
			else if (gVoiceClient->getIsSpeaking(speaker_id))
			{
				// reset inactivity expiration
				if (speakerp->mStatus != LLSpeaker::STATUS_SPEAKING)
				{
					speakerp->mLastSpokeTime = mSpeechTimer.getElapsedTimeF32();
					speakerp->mHasSpoken = TRUE;
				}
				speakerp->mStatus = LLSpeaker::STATUS_SPEAKING;
				// interpolate between active color and full speaking color based on power of speech output
				speakerp->mDotColor = speaking_color;
				if (speakerp->mSpeechVolume > LLVoiceClient::OVERDRIVEN_POWER_LEVEL)
				{
					speakerp->mDotColor = overdriven_color;
				}
			}
			else
			{
				speakerp->mSpeechVolume = 0.f;
				speakerp->mDotColor = ACTIVE_COLOR;

				if (speakerp->mHasSpoken)
				{
					// have spoken once, not currently speaking
					speakerp->mStatus = LLSpeaker::STATUS_HAS_SPOKEN;
				}
				else
				{
					// default state for being in voice channel
					speakerp->mStatus = LLSpeaker::STATUS_VOICE_ACTIVE;
				}
			}
		}
		// speaker no longer registered in voice channel, demote to text only
		else if (speakerp->mStatus != LLSpeaker::STATUS_NOT_IN_CHANNEL)
		{
			if(speakerp->mType == LLSpeaker::SPEAKER_EXTERNAL)
			{
				// external speakers should be timed out when they leave the voice channel (since they only exist via SLVoice)
				speakerp->mStatus = LLSpeaker::STATUS_NOT_IN_CHANNEL;
			}
			else
			{
				speakerp->mStatus = LLSpeaker::STATUS_TEXT_ONLY;
				speakerp->mSpeechVolume = 0.f;
				speakerp->mDotColor = ACTIVE_COLOR;
			}
		}
	}

	if(resort_ok)  // only allow list changes when user is not interacting with it
	{
		// sort by status then time last spoken
		std::sort(mSpeakersSorted.begin(), mSpeakersSorted.end(), LLSortRecentSpeakers());
	}

	// for recent speakers who are not currently speaking, show "recent" color dot for most recent
	// fading to "active" color

	S32 recent_speaker_count = 0;
	S32 sort_index = 0;
	speaker_list_t::iterator sorted_speaker_it;
	for(sorted_speaker_it = mSpeakersSorted.begin(); 
		sorted_speaker_it != mSpeakersSorted.end(); )
	{
		LLPointer<LLSpeaker> speakerp = *sorted_speaker_it;
		
		// color code recent speakers who are not currently speaking
		if (speakerp->mStatus == LLSpeaker::STATUS_HAS_SPOKEN)
		{
			speakerp->mDotColor = lerp(speaking_color, ACTIVE_COLOR, clamp_rescale((F32)recent_speaker_count, -2.f, 3.f, 0.f, 1.f));
			recent_speaker_count++;
		}

		// stuff sort ordinal into speaker so the ui can sort by this value
		speakerp->mSortIndex = sort_index++;

		// remove speakers that have been gone too long
		if (speakerp->mStatus == LLSpeaker::STATUS_NOT_IN_CHANNEL && speakerp->mActivityTimer.hasExpired())
		{
			fireEvent(new LLSpeakerListChangeEvent(this, speakerp->mID), "remove");

			mSpeakers.erase(speakerp->mID);
			sorted_speaker_it = mSpeakersSorted.erase(sorted_speaker_it);
		}
		else
		{
			++sorted_speaker_it;
		}
	}
}

void LLSpeakerMgr::updateSpeakerList()
{
	// are we bound to the currently active voice channel?
	if ((!mVoiceChannel && gVoiceClient->inProximalChannel()) || (mVoiceChannel && mVoiceChannel->isActive()))
	{
		LLVoiceClient::participantMap* participants = gVoiceClient->getParticipantList();
		if(participants)
		{
			LLVoiceClient::participantMap::iterator participant_it;

			// add new participants to our list of known speakers
			for (participant_it = participants->begin(); participant_it != participants->end(); ++participant_it)
			{
				LLVoiceClient::participantState* participantp = participant_it->second;
				setSpeaker(participantp->mAvatarID, participantp->mDisplayName, LLSpeaker::STATUS_VOICE_ACTIVE, (participantp->isAvatar()?LLSpeaker::SPEAKER_AGENT:LLSpeaker::SPEAKER_EXTERNAL));
			}
		}
	}
}

LLPointer<LLSpeaker> LLSpeakerMgr::findSpeaker(const LLUUID& speaker_id)
{
	//In some conditions map causes crash if it is empty(Windows only), adding check (EK)
	if (mSpeakers.size() == 0)
		return NULL;
	speaker_map_t::iterator found_it = mSpeakers.find(speaker_id);
	if (found_it == mSpeakers.end())
	{
		return NULL;
	}
	return found_it->second;
}

void LLSpeakerMgr::getSpeakerList(speaker_list_t* speaker_list, BOOL include_text)
{
	speaker_list->clear();
	for (speaker_map_t::iterator speaker_it = mSpeakers.begin(); speaker_it != mSpeakers.end(); ++speaker_it)
	{
		LLPointer<LLSpeaker> speakerp = speaker_it->second;
		// what about text only muted or inactive?
		if (include_text || speakerp->mStatus != LLSpeaker::STATUS_TEXT_ONLY)
		{
			speaker_list->push_back(speakerp);
		}
	}
}

const LLUUID LLSpeakerMgr::getSessionID() 
{ 
	return mVoiceChannel->getSessionID(); 
}


void LLSpeakerMgr::setSpeakerTyping(const LLUUID& speaker_id, BOOL typing)
{
	LLPointer<LLSpeaker> speakerp = findSpeaker(speaker_id);
	if (speakerp.notNull())
	{
		speakerp->mTyping = typing;
	}
}

// speaker has chatted via either text or voice
void LLSpeakerMgr::speakerChatted(const LLUUID& speaker_id)
{
	LLPointer<LLSpeaker> speakerp = findSpeaker(speaker_id);
	if (speakerp.notNull())
	{
		speakerp->mLastSpokeTime = mSpeechTimer.getElapsedTimeF32();
		speakerp->mHasSpoken = TRUE;
	}
}

BOOL LLSpeakerMgr::isVoiceActive()
{
	// mVoiceChannel = NULL means current voice channel, whatever it is
	return LLVoiceClient::voiceEnabled() && mVoiceChannel && mVoiceChannel->isActive();
}


//
// LLIMSpeakerMgr
//
LLIMSpeakerMgr::LLIMSpeakerMgr(LLVoiceChannel* channel) : LLSpeakerMgr(channel)
{
}

void LLIMSpeakerMgr::updateSpeakerList()
{
	// don't do normal updates which are pulled from voice channel
	// rely on user list reported by sim
	
	// We need to do this to allow PSTN callers into group chats to show in the list.
	LLSpeakerMgr::updateSpeakerList();
	
	return;
}

void LLIMSpeakerMgr::setSpeakers(const LLSD& speakers)
{
	if ( !speakers.isMap() ) return;

	if ( speakers.has("agent_info") && speakers["agent_info"].isMap() )
	{
		LLSD::map_const_iterator speaker_it;
		for(speaker_it = speakers["agent_info"].beginMap();
			speaker_it != speakers["agent_info"].endMap();
			++speaker_it)
		{
			LLUUID agent_id(speaker_it->first);

			LLPointer<LLSpeaker> speakerp = setSpeaker(
				agent_id,
				LLStringUtil::null,
				LLSpeaker::STATUS_TEXT_ONLY);

			if ( speaker_it->second.isMap() )
			{
				BOOL is_moderator = speakerp->mIsModerator;
				speakerp->mIsModerator = speaker_it->second["is_moderator"];
				speakerp->mModeratorMutedText =
					speaker_it->second["mutes"]["text"];
				// Fire event only if moderator changed
				if ( is_moderator != speakerp->mIsModerator )
					fireEvent(new LLSpeakerUpdateModeratorEvent(speakerp), "update_moderator");
			}
		}
	}
	else if ( speakers.has("agents" ) && speakers["agents"].isArray() )
	{
		//older, more decprecated way.  Need here for
		//using older version of servers
		LLSD::array_const_iterator speaker_it;
		for(speaker_it = speakers["agents"].beginArray();
			speaker_it != speakers["agents"].endArray();
			++speaker_it)
		{
			const LLUUID agent_id = (*speaker_it).asUUID();

			LLPointer<LLSpeaker> speakerp = setSpeaker(
				agent_id,
				LLStringUtil::null,
				LLSpeaker::STATUS_TEXT_ONLY);
		}
	}
}

void LLIMSpeakerMgr::updateSpeakers(const LLSD& update)
{
	if ( !update.isMap() ) return;

	if ( update.has("agent_updates") && update["agent_updates"].isMap() )
	{
		LLSD::map_const_iterator update_it;
		for(
			update_it = update["agent_updates"].beginMap();
			update_it != update["agent_updates"].endMap();
			++update_it)
		{
			LLUUID agent_id(update_it->first);
			LLPointer<LLSpeaker> speakerp = findSpeaker(agent_id);

			LLSD agent_data = update_it->second;

			if (agent_data.isMap() && agent_data.has("transition"))
			{
				if (agent_data["transition"].asString() == "LEAVE" && speakerp.notNull())
				{
					speakerp->mStatus = LLSpeaker::STATUS_NOT_IN_CHANNEL;
					speakerp->mDotColor = INACTIVE_COLOR;
					speakerp->mActivityTimer.resetWithExpiry(SPEAKER_TIMEOUT);
				}
				else if (agent_data["transition"].asString() == "ENTER")
				{
					// add or update speaker
					speakerp = setSpeaker(agent_id);
				}
				else
				{
					llwarns << "bad membership list update " << ll_print_sd(agent_data["transition"]) << llendl;
				}
			}

			if (speakerp.isNull()) continue;

			// should have a valid speaker from this point on
			if (agent_data.isMap() && agent_data.has("info"))
			{
				LLSD agent_info = agent_data["info"];

				if (agent_info.has("is_moderator"))
				{
					BOOL is_moderator = speakerp->mIsModerator;
					speakerp->mIsModerator = agent_info["is_moderator"];
					// Fire event only if moderator changed
					if ( is_moderator != speakerp->mIsModerator )
						fireEvent(new LLSpeakerUpdateModeratorEvent(speakerp), "update_moderator");
				}

				if (agent_info.has("mutes"))
				{
					speakerp->mModeratorMutedText = agent_info["mutes"]["text"];
				}
			}
		}
	}
	else if ( update.has("updates") && update["updates"].isMap() )
	{
		LLSD::map_const_iterator update_it;
		for (
			update_it = update["updates"].beginMap();
			update_it != update["updates"].endMap();
			++update_it)
		{
			LLUUID agent_id(update_it->first);
			LLPointer<LLSpeaker> speakerp = findSpeaker(agent_id);

			std::string agent_transition = update_it->second.asString();
			if (agent_transition == "LEAVE" && speakerp.notNull())
			{
				speakerp->mStatus = LLSpeaker::STATUS_NOT_IN_CHANNEL;
				speakerp->mDotColor = INACTIVE_COLOR;
				speakerp->mActivityTimer.resetWithExpiry(SPEAKER_TIMEOUT);
			}
			else if ( agent_transition == "ENTER")
			{
				// add or update speaker
				speakerp = setSpeaker(agent_id);
			}
			else
			{
				llwarns << "bad membership list update "
						<< agent_transition << llendl;
			}
		}
	}
}


//
// LLActiveSpeakerMgr
//

LLActiveSpeakerMgr::LLActiveSpeakerMgr() : LLSpeakerMgr(NULL)
{
}

void LLActiveSpeakerMgr::updateSpeakerList()
{
	// point to whatever the current voice channel is
	mVoiceChannel = LLVoiceChannel::getCurrentVoiceChannel();

	// always populate from active voice channel
	if (LLVoiceChannel::getCurrentVoiceChannel() != mVoiceChannel)
	{
		fireEvent(new LLSpeakerListChangeEvent(this, LLUUID::null), "clear");
		mSpeakers.clear();
		mSpeakersSorted.clear();
		mVoiceChannel = LLVoiceChannel::getCurrentVoiceChannel();
	}
	LLSpeakerMgr::updateSpeakerList();

	// clean up text only speakers
	for (speaker_map_t::iterator speaker_it = mSpeakers.begin(); speaker_it != mSpeakers.end(); ++speaker_it)
	{
		LLUUID speaker_id = speaker_it->first;
		LLSpeaker* speakerp = speaker_it->second;
		if (speakerp->mStatus == LLSpeaker::STATUS_TEXT_ONLY)
		{
			// automatically flag text only speakers for removal
			speakerp->mStatus = LLSpeaker::STATUS_NOT_IN_CHANNEL;
		}
	}

}



//
// LLLocalSpeakerMgr
//

LLLocalSpeakerMgr::LLLocalSpeakerMgr() : LLSpeakerMgr(LLVoiceChannelProximal::getInstance())
{
}

LLLocalSpeakerMgr::~LLLocalSpeakerMgr ()
{
}

void LLLocalSpeakerMgr::updateSpeakerList()
{
	// pull speakers from voice channel
	LLSpeakerMgr::updateSpeakerList();

	if (gDisconnected)//the world is cleared.
	{
		return ;
	}

	// pick up non-voice speakers in chat range
	std::vector<LLUUID> avatar_ids;
	std::vector<LLVector3d> positions;
	LLWorld::getInstance()->getAvatars(&avatar_ids, &positions, gAgent.getPositionGlobal(), CHAT_NORMAL_RADIUS);
	for(U32 i=0; i<avatar_ids.size(); i++)
	{
		setSpeaker(avatar_ids[i]);
	}

	// check if text only speakers have moved out of chat range
	for (speaker_map_t::iterator speaker_it = mSpeakers.begin(); speaker_it != mSpeakers.end(); ++speaker_it)
	{
		LLUUID speaker_id = speaker_it->first;
		LLSpeaker* speakerp = speaker_it->second;
		if (speakerp->mStatus == LLSpeaker::STATUS_TEXT_ONLY)
		{
			LLVOAvatar* avatarp = (LLVOAvatar*)gObjectList.findObject(speaker_id);
			if (!avatarp || dist_vec(avatarp->getPositionAgent(), gAgent.getPositionAgent()) > CHAT_NORMAL_RADIUS)
			{
				speakerp->mStatus = LLSpeaker::STATUS_NOT_IN_CHANNEL;
				speakerp->mDotColor = INACTIVE_COLOR;
				speakerp->mActivityTimer.resetWithExpiry(SPEAKER_TIMEOUT);
			}
		}
	}
}
