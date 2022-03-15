/**
* @file llpanelprofile.cpp
* @brief Profile panel implementation
*
* $LicenseInfo:firstyear=2009&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2010, Linden Research, Inc.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation;
* version 2.1 of the License only.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
*/

#include "llviewerprecompiledheaders.h"
#include "llpanelprofile.h"

// Common
#include "llavatarnamecache.h"
#include "llsdutil.h"
#include "llslurl.h"
#include "lldateutil.h" //ageFromDate

// UI
#include "llavatariconctrl.h"
#include "llclipboard.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "llloadingindicator.h"
#include "llmenubutton.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "lltoggleablemenu.h"
#include "llgrouplist.h"

// Newview
#include "llagent.h" //gAgent
#include "llagentpicksinfo.h"
#include "llavataractions.h"
#include "llavatarpropertiesprocessor.h"
#include "llcallingcard.h"
#include "llcommandhandler.h"
#include "llfloaterreg.h"
#include "llfirstuse.h"
#include "llgroupactions.h"
#include "llmutelist.h"
#include "llnotificationsutil.h"
#include "llpanelblockedlist.h"
#include "llpanelprofileclassifieds.h"
#include "llpanelprofilepicks.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewermenu.h" //is_agent_mappable
#include "llvoiceclient.h"
#include "llweb.h"


static LLPanelInjector<LLPanelProfileSecondLife> t_panel_profile_secondlife("panel_profile_secondlife");
static LLPanelInjector<LLPanelProfileWeb> t_panel_web("panel_profile_web");
static LLPanelInjector<LLPanelProfileInterests> t_panel_interests("panel_profile_interests");
static LLPanelInjector<LLPanelProfilePicks> t_panel_picks("panel_profile_picks");
static LLPanelInjector<LLPanelProfileFirstLife> t_panel_firstlife("panel_profile_firstlife");
static LLPanelInjector<LLPanelProfileNotes> t_panel_notes("panel_profile_notes");
static LLPanelInjector<LLPanelProfile>          t_panel_profile("panel_profile");

static const std::string PANEL_SECONDLIFE   = "panel_profile_secondlife";
static const std::string PANEL_WEB          = "panel_profile_web";
static const std::string PANEL_INTERESTS    = "panel_profile_interests";
static const std::string PANEL_PICKS        = "panel_profile_picks";
static const std::string PANEL_CLASSIFIEDS  = "panel_profile_classifieds";
static const std::string PANEL_FIRSTLIFE    = "panel_profile_firstlife";
static const std::string PANEL_NOTES        = "panel_profile_notes";
static const std::string PANEL_PROFILE_VIEW = "panel_profile_view";

static const std::string PROFILE_PROPERTIES_CAP = "AgentProfile";


//////////////////////////////////////////////////////////////////////////

void request_avatar_properties_coro(std::string cap_url, LLUUID agent_id)
{
    LLCore::HttpRequest::policy_t httpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t
        httpAdapter(new LLCoreHttpUtil::HttpCoroutineAdapter("request_avatar_properties_coro", httpPolicy));
    LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
    LLCore::HttpHeaders::ptr_t httpHeaders;

    LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);
    httpOpts->setFollowRedirects(true);

    std::string finalUrl = cap_url + "/" + agent_id.asString();

    LLSD result = httpAdapter->getAndSuspend(httpRequest, finalUrl, httpOpts, httpHeaders);

    LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

    if (!status
        || !result.has("id")
        || agent_id != result["id"].asUUID())
    {
        LL_WARNS("AvatarProperties") << "Failed to get agent information for id " << agent_id << LL_ENDL;
        return;
    }

    LLFloater* floater_profile = LLFloaterReg::findInstance("profile", LLSD().with("id", agent_id));
    if (!floater_profile)
    {
        // floater is dead, so panels are dead as well
        return;
    }

    LLPanel *panel = floater_profile->findChild<LLPanel>(PANEL_PROFILE_VIEW, TRUE);
    LLPanelProfile *panel_profile = dynamic_cast<LLPanelProfile*>(panel);
    if (!panel_profile)
    {
        LL_WARNS() << PANEL_PROFILE_VIEW << " not found" << LL_ENDL;
        return;
    }


    // Avatar Data

    LLAvatarData *avatar_data = &panel_profile->mAvatarData;
    std::string birth_date;

    avatar_data->agent_id = agent_id;
    avatar_data->avatar_id = agent_id;
    avatar_data->image_id = result["sl_image_id"].asUUID();
    avatar_data->fl_image_id = result["fl_image_id"].asUUID();
    avatar_data->partner_id = result["partner_id"].asUUID();
    // Todo: new descriptio size is 65536, check if it actually fits or has scroll
    avatar_data->about_text = result["sl_about_text"].asString();
    // Todo: new descriptio size is 65536, check if it actually fits or has scroll
    avatar_data->fl_about_text = result["fl_about_text"].asString();
    avatar_data->born_on = result["member_since"].asDate();
    avatar_data->profile_url = getProfileURL(agent_id.asString());

    avatar_data->flags = 0;

    if (result["online"].asBoolean())
    {
        avatar_data->flags |= AVATAR_ONLINE;
    }
    if (result["allow_publish"].asBoolean())
    {
        avatar_data->flags |= AVATAR_ALLOW_PUBLISH;
    }

    if (result["charter_member"].asBoolean())
    {
        const S32 TYPE_CHARTER_MEMBER = 2;
        avatar_data->caption_index = TYPE_CHARTER_MEMBER;
    }
    else
    {
        const S32 TYPE_RESIDENT = 0; // See ACCT_TYPE
        avatar_data->caption_index = TYPE_RESIDENT;
    }

    panel = floater_profile->findChild<LLPanel>(PANEL_SECONDLIFE, TRUE);
    LLPanelProfileSecondLife *panel_sl = dynamic_cast<LLPanelProfileSecondLife*>(panel);
    if (panel_sl)
    {
        panel_sl->processProfileProperties(avatar_data);
    }

    panel = floater_profile->findChild<LLPanel>(PANEL_WEB, TRUE);
    LLPanelProfileWeb *panel_web = dynamic_cast<LLPanelProfileWeb*>(panel);
    if (panel_web)
    {
        panel_web->updateButtons();
    }

    panel = floater_profile->findChild<LLPanel>(PANEL_FIRSTLIFE, TRUE);
    LLPanelProfileFirstLife *panel_first = dynamic_cast<LLPanelProfileFirstLife*>(panel);
    if (panel_first)
    {
        panel_first->mCurrentDescription = avatar_data->fl_about_text;
        panel_first->mDescriptionEdit->setValue(panel_first->mCurrentDescription);
        panel_first->mPicture->setValue(avatar_data->fl_image_id);
        panel_first->updateButtons();
    }

    // Picks

    LLSD picks_array = result["picks"];
    LLAvatarPicks avatar_picks;
    avatar_picks.agent_id = agent_id; // Not in use?
    avatar_picks.target_id = agent_id;

    for (LLSD::array_const_iterator it = picks_array.beginArray(); it != picks_array.endArray(); ++it)
    {
        const LLSD& pick_data = *it;
        avatar_picks.picks_list.emplace_back(pick_data["id"].asUUID(), pick_data["name"].asString());
    }

    panel = floater_profile->findChild<LLPanel>(PANEL_PICKS, TRUE);
    LLPanelProfilePicks *panel_picks = dynamic_cast<LLPanelProfilePicks*>(panel);
    if (panel_picks)
    {
        panel_picks->processProperties(&avatar_picks);
    }

    // Groups

    LLSD groups_array = result["groups"];
    LLAvatarGroups avatar_groups;
    avatar_groups.agent_id = agent_id; // Not in use?
    avatar_groups.avatar_id = agent_id; // target_id

    for (LLSD::array_const_iterator it = groups_array.beginArray(); it != groups_array.endArray(); ++it)
    {
        const LLSD& group_info = *it;
        LLAvatarGroups::LLGroupData group_data;
        group_data.group_powers = 0; // Not in use?
        group_data.group_title = group_info["name"].asString(); // Missing data, not in use?
        group_data.group_id = group_info["id"].asUUID();
        group_data.group_name = group_info["name"].asString();
        group_data.group_insignia_id = group_info["image_id"].asUUID();

        avatar_groups.group_list.push_back(group_data);
    }

    if (panel_sl)
    {
        panel_sl->processGroupProperties(&avatar_groups);
    }

    // Notes
    LLAvatarNotes avatar_notes;

    avatar_notes.agent_id = agent_id;
    avatar_notes.target_id = agent_id;
    avatar_notes.notes = result["notes"].asString();

    panel = floater_profile->findChild<LLPanel>(PANEL_NOTES, TRUE);
    LLPanelProfileNotes *panel_notes = dynamic_cast<LLPanelProfileNotes*>(panel);
    if (panel_notes)
    {
        panel_notes->processProperties(&avatar_notes);
    }
}

//TODO: changes take two minutes to propagate!
// Add some storage that holds updated data for two minutes
// for new instances to reuse the data
// Profile data is only relevant to won avatar, but notes
// are for everybody
void put_avatar_properties_coro(std::string cap_url, LLUUID agent_id, LLSD data)
{
    LLCore::HttpRequest::policy_t httpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t
        httpAdapter(new LLCoreHttpUtil::HttpCoroutineAdapter("request_avatar_properties_coro", httpPolicy));
    LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
    LLCore::HttpHeaders::ptr_t httpHeaders;

    LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);
    httpOpts->setFollowRedirects(true);

    std::string finalUrl = cap_url + "/" + agent_id.asString();

    LLSD result = httpAdapter->putAndSuspend(httpRequest, finalUrl, data, httpOpts, httpHeaders);

    LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

    if (!status)
    {
        LL_WARNS("AvatarProperties") << "Failed to put agent information for id " << agent_id << LL_ENDL;
        return;
    }
}

//////////////////////////////////////////////////////////////////////////
// LLProfileHandler

class LLProfileHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLProfileHandler() : LLCommandHandler("profile", UNTRUSTED_THROTTLE) { }

	bool handle(const LLSD& params, const LLSD& query_map,
		LLMediaCtrl* web)
	{
		if (params.size() < 1) return false;
		std::string agent_name = params[0];
		LL_INFOS() << "Profile, agent_name " << agent_name << LL_ENDL;
		std::string url = getProfileURL(agent_name);
		LLWeb::loadURLInternal(url);

		return true;
	}
};
LLProfileHandler gProfileHandler;


//////////////////////////////////////////////////////////////////////////
// LLAgentHandler

class LLAgentHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLAgentHandler() : LLCommandHandler("agent", UNTRUSTED_THROTTLE) { }

	bool handle(const LLSD& params, const LLSD& query_map,
		LLMediaCtrl* web)
	{
		if (params.size() < 2) return false;
		LLUUID avatar_id;
		if (!avatar_id.set(params[0], FALSE))
		{
			return false;
		}

		const std::string verb = params[1].asString();
		if (verb == "about")
		{
			LLAvatarActions::showProfile(avatar_id);
			return true;
		}

		if (verb == "inspect")
		{
			LLFloaterReg::showInstance("inspect_avatar", LLSD().with("avatar_id", avatar_id));
			return true;
		}

		if (verb == "im")
		{
			LLAvatarActions::startIM(avatar_id);
			return true;
		}

		if (verb == "pay")
		{
			if (!LLUI::getInstance()->mSettingGroups["config"]->getBOOL("EnableAvatarPay"))
			{
				LLNotificationsUtil::add("NoAvatarPay", LLSD(), LLSD(), std::string("SwitchToStandardSkinAndQuit"));
				return true;
			}

			LLAvatarActions::pay(avatar_id);
			return true;
		}

		if (verb == "offerteleport")
		{
			LLAvatarActions::offerTeleport(avatar_id);
			return true;
		}

		if (verb == "requestfriend")
		{
			LLAvatarActions::requestFriendshipDialog(avatar_id);
			return true;
		}

		if (verb == "removefriend")
		{
			LLAvatarActions::removeFriendDialog(avatar_id);
			return true;
		}

		if (verb == "mute")
		{
			if (! LLAvatarActions::isBlocked(avatar_id))
			{
				LLAvatarActions::toggleBlock(avatar_id);
			}
			return true;
		}

		if (verb == "unmute")
		{
			if (LLAvatarActions::isBlocked(avatar_id))
			{
				LLAvatarActions::toggleBlock(avatar_id);
			}
			return true;
		}

		if (verb == "block")
		{
			if (params.size() > 2)
			{
				const std::string object_name = LLURI::unescape(params[2].asString());
				LLMute mute(avatar_id, object_name, LLMute::OBJECT);
				LLMuteList::getInstance()->add(mute);
				LLPanelBlockedList::showPanelAndSelect(mute.mID);
			}
			return true;
		}

		if (verb == "unblock")
		{
			if (params.size() > 2)
			{
				const std::string object_name = params[2].asString();
				LLMute mute(avatar_id, object_name, LLMute::OBJECT);
				LLMuteList::getInstance()->remove(mute);
			}
			return true;
		}
		return false;
	}
};
LLAgentHandler gAgentHandler;



//////////////////////////////////////////////////////////////////////////
// LLPanelProfileSecondLife

LLPanelProfileSecondLife::LLPanelProfileSecondLife()
 : LLPanelProfileTab()
 , mStatusText(NULL)
 , mAvatarNameCacheConnection()
{
}

LLPanelProfileSecondLife::~LLPanelProfileSecondLife()
{
    if (getAvatarId().notNull())
    {
        LLAvatarTracker::instance().removeParticularFriendObserver(getAvatarId(), this);
    }

    if (LLVoiceClient::instanceExists())
    {
        LLVoiceClient::getInstance()->removeObserver((LLVoiceClientStatusObserver*)this);
    }

    if (mAvatarNameCacheConnection.connected())
    {
        mAvatarNameCacheConnection.disconnect();
    }
}

BOOL LLPanelProfileSecondLife::postBuild()
{
    mStatusText             = getChild<LLTextBox>("status");
    mGroupList              = getChild<LLGroupList>("group_list");
    mShowInSearchCheckbox   = getChild<LLCheckBoxCtrl>("show_in_search_checkbox");
    mSecondLifePic          = getChild<LLTextureCtrl>("2nd_life_pic");
    mSecondLifePicLayout    = getChild<LLPanel>("image_stack");
    mDescriptionEdit        = getChild<LLTextBase>("sl_description_edit");
    mTeleportButton         = getChild<LLButton>("teleport");
    mShowOnMapButton        = getChild<LLButton>("show_on_map_btn");
    mBlockButton            = getChild<LLButton>("block");
    mUnblockButton          = getChild<LLButton>("unblock");
    mNameLabel              = getChild<LLUICtrl>("name_label");
    mDisplayNameButton      = getChild<LLButton>("set_name");
    mAddFriendButton        = getChild<LLButton>("add_friend");
    mGroupInviteButton      = getChild<LLButton>("group_invite");
    mPayButton              = getChild<LLButton>("pay");
    mIMButton               = getChild<LLButton>("im");
    mCopyMenuButton         = getChild<LLMenuButton>("copy_btn");
    mGiveInvPanel           = getChild<LLPanel>("give_stack");

    mStatusText->setVisible(FALSE);
    mCopyMenuButton->setVisible(FALSE);

    mAddFriendButton->setCommitCallback(boost::bind(&LLPanelProfileSecondLife::onAddFriendButtonClick, this));
    mIMButton->setCommitCallback(boost::bind(&LLPanelProfileSecondLife::onIMButtonClick, this));
    mTeleportButton->setCommitCallback(boost::bind(&LLPanelProfileSecondLife::onTeleportButtonClick, this));
    mShowOnMapButton->setCommitCallback(boost::bind(&LLPanelProfileSecondLife::onMapButtonClick, this));
    mPayButton->setCommitCallback(boost::bind(&LLPanelProfileSecondLife::pay, this));
    mBlockButton->setCommitCallback(boost::bind(&LLPanelProfileSecondLife::onClickToggleBlock, this));
    mUnblockButton->setCommitCallback(boost::bind(&LLPanelProfileSecondLife::onClickToggleBlock, this));
    mGroupInviteButton->setCommitCallback(boost::bind(&LLPanelProfileSecondLife::onGroupInvite,this));
    mDisplayNameButton->setCommitCallback(boost::bind(&LLPanelProfileSecondLife::onClickSetName, this));
    mSecondLifePic->setCommitCallback(boost::bind(&LLPanelProfileSecondLife::onCommitTexture, this));

    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar commit;
    commit.add("Profile.CopyName", [this](LLUICtrl*, const LLSD& userdata) { onCommitMenu(userdata); });

    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable;
    enable.add("Profile.EnableCall",                [this](LLUICtrl*, const LLSD&) { return mVoiceStatus; });
    enable.add("Profile.EnableGod",                 [](LLUICtrl*, const LLSD&) { return gAgent.isGodlike(); });

    mGroupList->setDoubleClickCallback(boost::bind(&LLPanelProfileSecondLife::openGroupProfile, this));
    mGroupList->setReturnCallback(boost::bind(&LLPanelProfileSecondLife::openGroupProfile, this));

    LLVoiceClient::getInstance()->addObserver((LLVoiceClientStatusObserver*)this);
    mCopyMenuButton->setMenu("menu_name_field.xml", LLMenuButton::MP_BOTTOM_RIGHT);

    return TRUE;
}

void LLPanelProfileSecondLife::onOpen(const LLSD& key)
{
    LLPanelProfileTab::onOpen(key);

    resetData();

    LLUUID avatar_id = getAvatarId();
    LLAvatarPropertiesProcessor::getInstance()->addObserver(avatar_id, this);

    BOOL own_profile = getSelfProfile();

    mGroupInviteButton->setVisible(!own_profile);
    mShowOnMapButton->setVisible(!own_profile);
    mPayButton->setVisible(!own_profile);
    mTeleportButton->setVisible(!own_profile);
    mIMButton->setVisible(!own_profile);
    mAddFriendButton->setVisible(!own_profile);
    mBlockButton->setVisible(!own_profile);
    mUnblockButton->setVisible(!own_profile);
    mGroupList->setShowNone(!own_profile);
    mGiveInvPanel->setVisible(!own_profile);

    mSecondLifePic->setOpenTexPreview(!own_profile);

    if (own_profile && !getEmbedded())
    {
        // Group list control cannot toggle ForAgent loading
        // Less than ideal, but viewing own profile via search is edge case
        mGroupList->enableForAgent(false);
    }

    if (own_profile && !getEmbedded() )
    {
        mNameLabel->setVisible(FALSE);
        mDisplayNameButton->setVisible(TRUE);
        mDisplayNameButton->setEnabled(TRUE);
    }

    mDescriptionEdit->setParseHTML(!own_profile && !getEmbedded());

    LLProfileDropTarget* drop_target = getChild<LLProfileDropTarget>("drop_target");
    drop_target->setVisible(!own_profile);
    drop_target->setEnabled(!own_profile);

    if (!own_profile)
    {
        mVoiceStatus = LLAvatarActions::canCall() && (LLAvatarActions::isFriend(avatar_id) ? LLAvatarTracker::instance().isBuddyOnline(avatar_id) : TRUE);
        drop_target->setAgentID(avatar_id);
        updateOnlineStatus();
    }

    updateButtons();

    mAvatarNameCacheConnection = LLAvatarNameCache::get(getAvatarId(), boost::bind(&LLPanelProfileSecondLife::onAvatarNameCache, this, _1, _2));
}

void LLPanelProfileSecondLife::apply(LLAvatarData* data)
{
    if (getIsLoaded() && getSelfProfile())
    {
        // Might be a better idea to accumulate changes in floater
        // instead of sending a request per tab
        std::string cap_url = gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP);
        if (!cap_url.empty())
        {
            LLSD params = LLSDMap();
            if (data->image_id != mSecondLifePic->getImageAssetID())
            {
                params["sl_image_id"] = mSecondLifePic->getImageAssetID();
            }
            if (data->about_text != mDescriptionEdit->getValue().asString())
            {
                params["sl_about_text"] = mDescriptionEdit->getValue().asString();
            }
            if ((bool)data->allow_publish != mShowInSearchCheckbox->getValue().asBoolean())
            {
                params["allow_publish"] = mShowInSearchCheckbox->getValue().asBoolean();
            }
            if (!params.emptyMap())
            {
                LLCoros::instance().launch("putAgentUserInfoCoro",
                    boost::bind(put_avatar_properties_coro, cap_url, getAvatarId(), params));
            }
        }
        else
        {
            LL_WARNS() << "Failed to update profile data, no cap found" << LL_ENDL;
        }
    }
}

void LLPanelProfileSecondLife::updateData()
{
    LLUUID avatar_id = getAvatarId();
    if (!getIsLoading() && avatar_id.notNull() && !(getSelfProfile() && !getEmbedded()))
    {
        setIsLoading();

        std::string cap_url = gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP);
        if (!cap_url.empty())
        {
            LLCoros::instance().launch("requestAgentUserInfoCoro",
                boost::bind(request_avatar_properties_coro, cap_url, avatar_id));
        }
        else
        {
            LL_WARNS() << "Failed to update profile data, no cap found" << LL_ENDL;
        }
    }
}

void LLPanelProfileSecondLife::processProperties(void* data, EAvatarProcessorType type)
{

    if (APT_PROPERTIES == type)
    {
        const LLAvatarData* avatar_data = static_cast<const LLAvatarData*>(data);
        if(avatar_data && getAvatarId() == avatar_data->avatar_id)
        {
            processProfileProperties(avatar_data);
        }
    }
}

void LLPanelProfileSecondLife::resetData()
{
    resetLoading();
    getChild<LLUICtrl>("complete_name")->setValue(LLStringUtil::null);
    getChild<LLUICtrl>("register_date")->setValue(LLStringUtil::null);
    getChild<LLUICtrl>("acc_status_text")->setValue(LLStringUtil::null);
    getChild<LLUICtrl>("partner_text")->setValue(LLStringUtil::null);

    // Set default image and 1:1 dimensions for it
    mSecondLifePic->setValue(mSecondLifePic->getDefaultImageAssetID());
    LLRect imageRect = mSecondLifePicLayout->getRect();
    mSecondLifePicLayout->reshape(imageRect.getHeight(), imageRect.getHeight());

    mDescriptionEdit->setValue(LLStringUtil::null);
    mStatusText->setVisible(FALSE);
    mCopyMenuButton->setVisible(FALSE);
    mGroups.clear();
    mGroupList->setGroups(mGroups);
}

void LLPanelProfileSecondLife::processProfileProperties(const LLAvatarData* avatar_data)
{
    LLUUID avatar_id = getAvatarId();
    if (!LLAvatarActions::isFriend(avatar_id) && !getSelfProfile())
    {
        // this is non-friend avatar. Status will be updated from LLAvatarPropertiesProcessor.
        // in LLPanelProfileSecondLife::processOnlineStatus()

        // subscribe observer to get online status. Request will be sent by LLPanelProfileSecondLife itself.
        // do not subscribe for friend avatar because online status can be wrong overridden
        // via LLAvatarData::flags if Preferences: "Only Friends & Groups can see when I am online" is set.
        processOnlineStatus(avatar_data->flags & AVATAR_ONLINE);
    }

    fillCommonData(avatar_data);

    fillPartnerData(avatar_data);

    fillAccountStatus(avatar_data);

    updateButtons();
}

void LLPanelProfileSecondLife::processGroupProperties(const LLAvatarGroups* avatar_groups)
{
    //KC: the group_list ctrl can handle all this for us on our own profile
    if (getSelfProfile() && !getEmbedded())
    {
        return;
    }

    // *NOTE dzaporozhan
    // Group properties may arrive in two callbacks, we need to save them across
    // different calls. We can't do that in textbox as textbox may change the text.

    LLAvatarGroups::group_list_t::const_iterator it = avatar_groups->group_list.begin();
    const LLAvatarGroups::group_list_t::const_iterator it_end = avatar_groups->group_list.end();

    for (; it_end != it; ++it)
    {
        LLAvatarGroups::LLGroupData group_data = *it;
        mGroups[group_data.group_name] = group_data.group_id;
    }

    mGroupList->setGroups(mGroups);
}

void LLPanelProfileSecondLife::openGroupProfile()
{
    LLUUID group_id = mGroupList->getSelectedUUID();
    LLGroupActions::show(group_id);
}

void LLPanelProfileSecondLife::onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
{
    mAvatarNameCacheConnection.disconnect();

    getChild<LLUICtrl>("complete_name")->setValue( av_name.getCompleteName() );
    mCopyMenuButton->setVisible(TRUE);
}

void LLPanelProfileSecondLife::fillCommonData(const LLAvatarData* avatar_data)
{
    // Refresh avatar id in cache with new info to prevent re-requests
    // and to make sure icons in text will be up to date
    LLAvatarIconIDCache::getInstance()->add(avatar_data->avatar_id, avatar_data->image_id);

    LLStringUtil::format_map_t args;
    {
        std::string birth_date = LLTrans::getString("AvatarBirthDateFormat");
        LLStringUtil::format(birth_date, LLSD().with("datetime", (S32) avatar_data->born_on.secondsSinceEpoch()));
        args["[REG_DATE]"] = birth_date;
    }

    args["[AGE]"] = LLDateUtil::ageFromDate( avatar_data->born_on, LLDate::now());
    std::string register_date = getString("RegisterDateFormat", args);
    getChild<LLUICtrl>("register_date")->setValue(register_date );
    mDescriptionEdit->setValue(avatar_data->about_text);
    mSecondLifePic->setValue(avatar_data->image_id);

    //Don't bother about boost level, picker will set it
    LLViewerFetchedTexture* imagep = LLViewerTextureManager::getFetchedTexture(avatar_data->image_id);
    if (imagep->getFullHeight())
    {
        onImageLoaded(true, imagep);
    }
    else
    {
        imagep->setLoadedCallback(onImageLoaded,
                                  MAX_DISCARD_LEVEL,
                                  FALSE,
                                  FALSE,
                                  new LLHandle<LLPanel>(getHandle()),
                                  NULL,
                                  FALSE);
    }

    if (getSelfProfile())
    {
        mShowInSearchCheckbox->setValue((BOOL)(avatar_data->flags & AVATAR_ALLOW_PUBLISH));
    }
}

void LLPanelProfileSecondLife::fillPartnerData(const LLAvatarData* avatar_data)
{
    LLTextEditor* partner_text = getChild<LLTextEditor>("partner_text");
    if (avatar_data->partner_id.notNull())
    {
        partner_text->setText(LLSLURL("agent", avatar_data->partner_id, "inspect").getSLURLString());
    }
    else
    {
        partner_text->setText(getString("no_partner_text"));
    }
}

void LLPanelProfileSecondLife::fillAccountStatus(const LLAvatarData* avatar_data)
{
    LLStringUtil::format_map_t args;
    args["[ACCTTYPE]"] = LLAvatarPropertiesProcessor::accountType(avatar_data);
    args["[PAYMENTINFO]"] = LLAvatarPropertiesProcessor::paymentInfo(avatar_data);

    std::string caption_text = getString("CaptionTextAcctInfo", args);
    getChild<LLUICtrl>("acc_status_text")->setValue(caption_text);
}

void LLPanelProfileSecondLife::onMapButtonClick()
{
    LLAvatarActions::showOnMap(getAvatarId());
}

void LLPanelProfileSecondLife::pay()
{
    LLAvatarActions::pay(getAvatarId());
}

void LLPanelProfileSecondLife::onClickToggleBlock()
{
    bool blocked = LLAvatarActions::toggleBlock(getAvatarId());

    updateButtons();
    // we are hiding one button and showing another, set focus
    if (blocked)
    {
        mUnblockButton->setFocus(true);
    }
    else
    {
        mBlockButton->setFocus(true);
    }
}

void LLPanelProfileSecondLife::onAddFriendButtonClick()
{
    LLAvatarActions::requestFriendshipDialog(getAvatarId());
}

void LLPanelProfileSecondLife::onIMButtonClick()
{
    LLAvatarActions::startIM(getAvatarId());
}

void LLPanelProfileSecondLife::onTeleportButtonClick()
{
    LLAvatarActions::offerTeleport(getAvatarId());
}

void LLPanelProfileSecondLife::onGroupInvite()
{
    LLAvatarActions::inviteToGroup(getAvatarId());
}

void LLPanelProfileSecondLife::onImageLoaded(BOOL success, LLViewerFetchedTexture *imagep)
{
    LLRect imageRect = mSecondLifePicLayout->getRect();
    if (!success || imagep->getFullWidth() == imagep->getFullHeight())
    {
        mSecondLifePicLayout->reshape(imageRect.getHeight(), imageRect.getHeight());
    }
    else
    {
        // assume 3:4, for sake of firestorm
        mSecondLifePicLayout->reshape(imageRect.getHeight() * 4 / 3, imageRect.getHeight());
    }
}

//static
void LLPanelProfileSecondLife::onImageLoaded(BOOL success,
                                             LLViewerFetchedTexture *src_vi,
                                             LLImageRaw* src,
                                             LLImageRaw* aux_src,
                                             S32 discard_level,
                                             BOOL final,
                                             void* userdata)
{
    if (!userdata) return;

    LLHandle<LLPanel>* handle = (LLHandle<LLPanel>*)userdata;

    if (!handle->isDead())
    {
        LLPanelProfileSecondLife* panel = static_cast<LLPanelProfileSecondLife*>(handle->get());
        if (panel)
        {
            panel->onImageLoaded(success, src_vi);
        }
    }

    if (final || !success)
    {
        delete handle;
    }
}

// virtual, called by LLAvatarTracker
void LLPanelProfileSecondLife::changed(U32 mask)
{
    updateOnlineStatus();
    updateButtons();
}

// virtual, called by LLVoiceClient
void LLPanelProfileSecondLife::onChange(EStatusType status, const std::string &channelURI, bool proximal)
{
    if(status == STATUS_JOINING || status == STATUS_LEFT_CHANNEL)
    {
        return;
    }

    mVoiceStatus = LLAvatarActions::canCall() && (LLAvatarActions::isFriend(getAvatarId()) ? LLAvatarTracker::instance().isBuddyOnline(getAvatarId()) : TRUE);
}

void LLPanelProfileSecondLife::setAvatarId(const LLUUID& avatar_id)
{
    if (avatar_id.notNull())
    {
        if (getAvatarId().notNull())
        {
            LLAvatarTracker::instance().removeParticularFriendObserver(getAvatarId(), this);
        }

        LLPanelProfileTab::setAvatarId(avatar_id);

        if (LLAvatarActions::isFriend(getAvatarId()))
        {
            LLAvatarTracker::instance().addParticularFriendObserver(getAvatarId(), this);
        }
    }
}

bool LLPanelProfileSecondLife::isGrantedToSeeOnlineStatus()
{
    // set text box visible to show online status for non-friends who has not set in Preferences
    // "Only Friends & Groups can see when I am online"
    if (!LLAvatarActions::isFriend(getAvatarId()))
    {
        return true;
    }

    // *NOTE: GRANT_ONLINE_STATUS is always set to false while changing any other status.
    // When avatar disallow me to see her online status processOfflineNotification Message is received by the viewer
    // see comments for ChangeUserRights template message. EXT-453.
    // If GRANT_ONLINE_STATUS flag is changed it will be applied when viewer restarts. EXT-3880
    const LLRelationship* relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
    return relationship->isRightGrantedFrom(LLRelationship::GRANT_ONLINE_STATUS);
}

// method was disabled according to EXT-2022. Re-enabled & improved according to EXT-3880
void LLPanelProfileSecondLife::updateOnlineStatus()
{
    if (!LLAvatarActions::isFriend(getAvatarId())) return;
    // For friend let check if he allowed me to see his status
    const LLRelationship* relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
    bool online = relationship->isOnline();
    processOnlineStatus(online);
}

void LLPanelProfileSecondLife::processOnlineStatus(bool online)
{
    mStatusText->setVisible(isGrantedToSeeOnlineStatus());

    std::string status = getString(online ? "status_online" : "status_offline");

    mStatusText->setValue(status);
    mStatusText->setColor(online ?
    LLUIColorTable::instance().getColor("StatusUserOnline") :
    LLUIColorTable::instance().getColor("StatusUserOffline"));
}

void LLPanelProfileSecondLife::updateButtons()
{
    LLPanelProfileTab::updateButtons();

    if (getSelfProfile() && !getEmbedded())
    {
        mShowInSearchCheckbox->setVisible(TRUE);
        mShowInSearchCheckbox->setEnabled(TRUE);
        mDescriptionEdit->setEnabled(TRUE);
    }

    if (!getSelfProfile())
    {
        LLUUID av_id = getAvatarId();
        bool is_buddy_online = LLAvatarTracker::instance().isBuddyOnline(getAvatarId());

        if (LLAvatarActions::isFriend(av_id))
        {
            mTeleportButton->setEnabled(is_buddy_online);
            //Disable "Add Friend" button for friends.
            mAddFriendButton->setEnabled(false);
        }
        else
        {
            mTeleportButton->setEnabled(true);
            mAddFriendButton->setEnabled(true);
        }

        bool enable_map_btn = (is_buddy_online && is_agent_mappable(av_id)) || gAgent.isGodlike();
        mShowOnMapButton->setEnabled(enable_map_btn);

        bool enable_block_btn = LLAvatarActions::canBlock(av_id) && !LLAvatarActions::isBlocked(av_id);
        mBlockButton->setVisible(enable_block_btn);

        bool enable_unblock_btn = LLAvatarActions::isBlocked(av_id);
        mUnblockButton->setVisible(enable_unblock_btn);
    }
}

void LLPanelProfileSecondLife::onClickSetName()
{
    LLAvatarNameCache::get(getAvatarId(), boost::bind(&LLPanelProfileSecondLife::onAvatarNameCacheSetName, this, _1, _2));

    LLFirstUse::setDisplayName(false);
}

void LLPanelProfileSecondLife::onCommitTexture()
{
    LLViewerFetchedTexture* imagep = LLViewerTextureManager::getFetchedTexture(mSecondLifePic->getImageAssetID());
    if (imagep->getFullHeight())
    {
        onImageLoaded(true, imagep);
    }
    else
    {
        imagep->setLoadedCallback(onImageLoaded,
            MAX_DISCARD_LEVEL,
            FALSE,
            FALSE,
            new LLHandle<LLPanel>(getHandle()),
            NULL,
            FALSE);
    }
}

void LLPanelProfileSecondLife::onCommitMenu(const LLSD& userdata)
{
    LLAvatarName av_name;
    if (!LLAvatarNameCache::get(getAvatarId(), &av_name))
    {
        // shouldn't happen, button(menu) is supposed to be invisible while name is fetching
        LL_WARNS() << "Failed to get agent data" << LL_ENDL;
        return;
    }

    const std::string item_name = userdata.asString();
    LLWString wstr;
    if (item_name == "display")
    {
        wstr = utf8str_to_wstring(av_name.getDisplayName(true));
    }
    else if (item_name == "name")
    {
        wstr = utf8str_to_wstring(av_name.getAccountName());
    }
    else if (item_name == "id")
    {
        wstr = utf8str_to_wstring(getAvatarId().asString());
    }
    LLClipboard::instance().copyToClipboard(wstr, 0, wstr.size());
}

void LLPanelProfileSecondLife::onAvatarNameCacheSetName(const LLUUID& agent_id, const LLAvatarName& av_name)
{
    if (av_name.getDisplayName().empty())
    {
        // something is wrong, tell user to try again later
        LLNotificationsUtil::add("SetDisplayNameFailedGeneric");
        return;
    }

    LL_INFOS("LegacyProfile") << "name-change now " << LLDate::now() << " next_update "
        << LLDate(av_name.mNextUpdate) << LL_ENDL;
    F64 now_secs = LLDate::now().secondsSinceEpoch();

    if (now_secs < av_name.mNextUpdate)
    {
        // if the update time is more than a year in the future, it means updates have been blocked
        // show a more general message
        static const S32 YEAR = 60*60*24*365;
        if (now_secs + YEAR < av_name.mNextUpdate)
        {
            LLNotificationsUtil::add("SetDisplayNameBlocked");
            return;
        }
    }

    LLFloaterReg::showInstance("display_name");
}

//////////////////////////////////////////////////////////////////////////
// LLPanelProfileWeb

LLPanelProfileWeb::LLPanelProfileWeb()
 : LLPanelProfileTab()
 , mWebBrowser(NULL)
 , mAvatarNameCacheConnection()
{
}

LLPanelProfileWeb::~LLPanelProfileWeb()
{
    if (mAvatarNameCacheConnection.connected())
    {
        mAvatarNameCacheConnection.disconnect();
    }
}

void LLPanelProfileWeb::onOpen(const LLSD& key)
{
    LLPanelProfileTab::onOpen(key);

    resetData();

    mAvatarNameCacheConnection = LLAvatarNameCache::get(getAvatarId(), boost::bind(&LLPanelProfileWeb::onAvatarNameCache, this, _1, _2));
}

BOOL LLPanelProfileWeb::postBuild()
{
    mWebBrowser = getChild<LLMediaCtrl>("profile_html");
    mWebBrowser->addObserver(this);
    mWebBrowser->setHomePageUrl("about:blank");

    return TRUE;
}

void LLPanelProfileWeb::processProperties(void* data, EAvatarProcessorType type)
{
    if (APT_PROPERTIES == type)
    {
        const LLAvatarData* avatar_data = static_cast<const LLAvatarData*>(data);
        if (avatar_data && getAvatarId() == avatar_data->avatar_id)
        {
            updateButtons();
        }
    }
}

void LLPanelProfileWeb::resetData()
{
    mWebBrowser->navigateHome();
}

void LLPanelProfileWeb::apply(LLAvatarData* data)
{

}

void LLPanelProfileWeb::updateData()
{
    LLUUID avatar_id = getAvatarId();
    if (!getIsLoading() && avatar_id.notNull() && !mURLWebProfile.empty())
    {
        setIsLoading();

        mWebBrowser->setVisible(TRUE);
        mPerformanceTimer.start();
        mWebBrowser->navigateTo(mURLWebProfile, HTTP_CONTENT_TEXT_HTML);
    }
}

void LLPanelProfileWeb::onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
{
    mAvatarNameCacheConnection.disconnect();

    std::string username = av_name.getAccountName();
    if (username.empty())
    {
        username = LLCacheName::buildUsername(av_name.getDisplayName());
    }
    else
    {
        LLStringUtil::replaceChar(username, ' ', '.');
    }

    mURLWebProfile = getProfileURL(username, true);
    if (mURLWebProfile.empty())
    {
        return;
    }

    //if the tab was opened before name was resolved, load the panel now
    updateData();
}

void LLPanelProfileWeb::onCommitLoad(LLUICtrl* ctrl)
{
    if (!mURLHome.empty())
    {
        LLSD::String valstr = ctrl->getValue().asString();
        if (valstr.empty())
        {
            mWebBrowser->setVisible(TRUE);
            mPerformanceTimer.start();
            mWebBrowser->navigateTo( mURLHome, HTTP_CONTENT_TEXT_HTML );
        }
        else if (valstr == "popout")
        {
            // open in viewer's browser, new window
            LLWeb::loadURLInternal(mURLHome);
        }
        else if (valstr == "external")
        {
            // open in external browser
            LLWeb::loadURLExternal(mURLHome);
        }
    }
}

void LLPanelProfileWeb::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
    switch(event)
    {
        case MEDIA_EVENT_STATUS_TEXT_CHANGED:
            childSetValue("status_text", LLSD( self->getStatusText() ) );
        break;

        case MEDIA_EVENT_NAVIGATE_BEGIN:
        {
            if (mFirstNavigate)
            {
                mFirstNavigate = false;
            }
            else
            {
                mPerformanceTimer.start();
            }
        }
        break;

        case MEDIA_EVENT_NAVIGATE_COMPLETE:
        {
            LLStringUtil::format_map_t args;
            args["[TIME]"] = llformat("%.2f", mPerformanceTimer.getElapsedTimeF32());
            childSetValue("status_text", LLSD( getString("LoadTime", args)) );
        }
        break;

        default:
            // Having a default case makes the compiler happy.
        break;
    }
}

void LLPanelProfileWeb::updateButtons()
{
    LLPanelProfileTab::updateButtons();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const S32 WANT_CHECKS = 8;
static const S32 SKILL_CHECKS = 6;

LLPanelProfileInterests::LLPanelProfileInterests()
 : LLPanelProfileTab()
{
}

LLPanelProfileInterests::~LLPanelProfileInterests()
{
}

void LLPanelProfileInterests::onOpen(const LLSD& key)
{
    LLPanelProfileTab::onOpen(key);

    resetData();
}

BOOL LLPanelProfileInterests::postBuild()
{
    mWantToEditor = getChild<LLLineEditor>("want_to_edit");
    mSkillsEditor = getChild<LLLineEditor>("skills_edit");
    mLanguagesEditor = getChild<LLLineEditor>("languages_edit");

    for (S32 i = 0; i < WANT_CHECKS; ++i)
    {
        std::string check_name = llformat("chk%d", i);
        mWantChecks[i] = getChild<LLCheckBoxCtrl>(check_name);
    }

    for (S32 i = 0; i < SKILL_CHECKS; ++i)
    {
        std::string check_name = llformat("schk%d", i);
        mSkillChecks[i] = getChild<LLCheckBoxCtrl>(check_name);
    }

    return TRUE;
}


void LLPanelProfileInterests::processProperties(void* data, EAvatarProcessorType type)
{
    if (APT_INTERESTS_INFO == type)
    {
        const LLInterestsData* interests_data = static_cast<const LLInterestsData*>(data);
        if (interests_data && getAvatarId() == interests_data->avatar_id)
        {
            for (S32 i = 0; i < WANT_CHECKS; ++i)
            {
                if (interests_data->want_to_mask & (1<<i))
                {
                    mWantChecks[i]->setValue(TRUE);
                }
                else
                {
                    mWantChecks[i]->setValue(FALSE);
                }
            }

            for (S32 i = 0; i < SKILL_CHECKS; ++i)
            {
                if (interests_data->skills_mask & (1<<i))
                {
                    mSkillChecks[i]->setValue(TRUE);
                }
                else
                {
                    mSkillChecks[i]->setValue(FALSE);
                }
            }

            mWantToEditor->setText(interests_data->want_to_text);
            mSkillsEditor->setText(interests_data->skills_text);
            mLanguagesEditor->setText(interests_data->languages_text);

            updateButtons();
        }
    }
}

void LLPanelProfileInterests::resetData()
{
    mWantToEditor->setValue(LLStringUtil::null);
    mSkillsEditor->setValue(LLStringUtil::null);
    mLanguagesEditor->setValue(LLStringUtil::null);

    for (S32 i = 0; i < WANT_CHECKS; ++i)
    {
        mWantChecks[i]->setValue(FALSE);
    }

    for (S32 i = 0; i < SKILL_CHECKS; ++i)
    {
        mSkillChecks[i]->setValue(FALSE);
    }
}

void LLPanelProfileInterests::apply()
{
    if (getIsLoaded() && getSelfProfile())
    {
        LLInterestsData interests_data = LLInterestsData();

        interests_data.want_to_mask = 0;
        for (S32 i = 0; i < WANT_CHECKS; ++i)
        {
            if (mWantChecks[i]->getValue().asBoolean())
            {
                interests_data.want_to_mask |= (1 << i);
            }
        }

        interests_data.skills_mask = 0;
        for (S32 i = 0; i < SKILL_CHECKS; ++i)
        {
            if (mSkillChecks[i]->getValue().asBoolean())
            {
                interests_data.skills_mask |= (1 << i);
            }
        }

        interests_data.want_to_text = mWantToEditor->getText();
        interests_data.skills_text = mSkillsEditor->getText();
        interests_data.languages_text = mLanguagesEditor->getText();

        LLAvatarPropertiesProcessor::getInstance()->sendInterestsInfoUpdate(&interests_data);
    }

}

void LLPanelProfileInterests::updateButtons()
{
    LLPanelProfileTab::updateButtons();

    if (getSelfProfile() && !getEmbedded())
    {
        mWantToEditor->setEnabled(TRUE);
        mSkillsEditor->setEnabled(TRUE);
        mLanguagesEditor->setEnabled(TRUE);

        for (S32 i = 0; i < WANT_CHECKS; ++i)
        {
            mWantChecks[i]->setEnabled(TRUE);
        }

        for (S32 i = 0; i < SKILL_CHECKS; ++i)
        {
            mSkillChecks[i]->setEnabled(TRUE);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LLPanelProfileFirstLife::LLPanelProfileFirstLife()
 : LLPanelProfileTab(),
 mIsEditing(false)
{
}

LLPanelProfileFirstLife::~LLPanelProfileFirstLife()
{
}

BOOL LLPanelProfileFirstLife::postBuild()
{
    mDescriptionEdit = getChild<LLTextEditor>("fl_description_edit");
    mPicture = getChild<LLTextureCtrl>("real_world_pic");

    mDescriptionEdit->setFocusReceivedCallback(boost::bind(&LLPanelProfileFirstLife::onDescriptionFocusReceived, this));

    return TRUE;
}

void LLPanelProfileFirstLife::onOpen(const LLSD& key)
{
    LLPanelProfileTab::onOpen(key);

    resetData();
}


void LLPanelProfileFirstLife::onDescriptionFocusReceived()
{
    if (!mIsEditing && getSelfProfile())
    {
        mIsEditing = true;
        mDescriptionEdit->setParseHTML(false);
        mDescriptionEdit->setText(mCurrentDescription);
    }
}

void LLPanelProfileFirstLife::processProperties(void* data, EAvatarProcessorType type)
{
    if (APT_PROPERTIES == type)
    {
        const LLAvatarData* avatar_data = static_cast<const LLAvatarData*>(data);
        if (avatar_data && getAvatarId() == avatar_data->avatar_id)
        {
            mCurrentDescription = avatar_data->fl_about_text;
            mDescriptionEdit->setValue(mCurrentDescription);
            mPicture->setValue(avatar_data->fl_image_id);
            updateButtons();
        }
    }
}

void LLPanelProfileFirstLife::processProperties(const LLAvatarData* avatar_data)
{
    mCurrentDescription = avatar_data->fl_about_text;
    mDescriptionEdit->setValue(mCurrentDescription);
    mPicture->setValue(avatar_data->fl_image_id);
    updateButtons();
}

void LLPanelProfileFirstLife::resetData()
{
    mDescriptionEdit->setValue(LLStringUtil::null);
    mPicture->setValue(mPicture->getDefaultImageAssetID());
}

void LLPanelProfileFirstLife::apply(LLAvatarData* data)
{

    std::string cap_url = gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP);
    if (getIsLoaded() && !cap_url.empty())
    {
        LLSD params = LLSDMap();
        if (data->fl_image_id != mPicture->getImageAssetID())
        {
            params["fl_image_id"] = mPicture->getImageAssetID();
        }
        if (data->fl_about_text != mDescriptionEdit->getValue().asString())
        {
            params["fl_about_text"] = mDescriptionEdit->getValue().asString();
        }
        if (!params.emptyMap())
        {
            LLCoros::instance().launch("putAgentUserInfoCoro",
                boost::bind(put_avatar_properties_coro, cap_url, getAvatarId(), params));
        }
    }

    data->fl_image_id = mPicture->getImageAssetID();
    data->fl_about_text = mDescriptionEdit->getValue().asString();
}

void LLPanelProfileFirstLife::updateButtons()
{
    LLPanelProfileTab::updateButtons();

    if (getSelfProfile() && !getEmbedded())
    {
        mDescriptionEdit->setEnabled(TRUE);
        mPicture->setEnabled(TRUE);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LLPanelProfileNotes::LLPanelProfileNotes()
: LLPanelProfileTab()
, mAvatarNameCacheConnection()
{

}

LLPanelProfileNotes::~LLPanelProfileNotes()
{
    if (getAvatarId().notNull())
    {
        LLAvatarTracker::instance().removeParticularFriendObserver(getAvatarId(), this);
    }

    if (mAvatarNameCacheConnection.connected())
    {
        mAvatarNameCacheConnection.disconnect();
    }
}

void LLPanelProfileNotes::updateData()
{
    LLUUID avatar_id = getAvatarId();
    if (!getIsLoading() && avatar_id.notNull())
    {
        setIsLoading();

        std::string cap_url = gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP);
        if (!cap_url.empty())
        {
            LLCoros::instance().launch("requestAgentUserInfoCoro",
                boost::bind(request_avatar_properties_coro, cap_url, avatar_id));
        }
    }
}

BOOL LLPanelProfileNotes::postBuild()
{
    mOnlineStatus = getChild<LLCheckBoxCtrl>("status_check");
    mMapRights = getChild<LLCheckBoxCtrl>("map_check");
    mEditObjectRights = getChild<LLCheckBoxCtrl>("objects_check");
    mNotesEditor = getChild<LLTextEditor>("notes_edit");

    mEditObjectRights->setCommitCallback(boost::bind(&LLPanelProfileNotes::onCommitRights, this));

    mNotesEditor->setCommitCallback(boost::bind(&LLPanelProfileNotes::onCommitNotes,this));

    return TRUE;
}

void LLPanelProfileNotes::onOpen(const LLSD& key)
{
    LLPanelProfileTab::onOpen(key);

    resetData();

    fillRightsData();

    mAvatarNameCacheConnection = LLAvatarNameCache::get(getAvatarId(), boost::bind(&LLPanelProfileNotes::onAvatarNameCache, this, _1, _2));
}

void LLPanelProfileNotes::apply()
{
    onCommitNotes();
    applyRights();
}

void LLPanelProfileNotes::fillRightsData()
{
    mOnlineStatus->setValue(FALSE);
    mMapRights->setValue(FALSE);
    mEditObjectRights->setValue(FALSE);

    const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
    // If true - we are viewing friend's profile, enable check boxes and set values.
    if(relation)
    {
        S32 rights = relation->getRightsGrantedTo();

        mOnlineStatus->setValue(LLRelationship::GRANT_ONLINE_STATUS & rights ? TRUE : FALSE);
        mMapRights->setValue(LLRelationship::GRANT_MAP_LOCATION & rights ? TRUE : FALSE);
        mEditObjectRights->setValue(LLRelationship::GRANT_MODIFY_OBJECTS & rights ? TRUE : FALSE);
    }

    enableCheckboxes(NULL != relation);
}

void LLPanelProfileNotes::onCommitNotes()
{
    std::string cap_url = gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP);
    if (getIsLoaded())
    {
        if (!cap_url.empty())
        {
            std::string notes = mNotesEditor->getValue().asString();
            LLCoros::instance().launch("putAgentUserInfoCoro",
                boost::bind(put_avatar_properties_coro, cap_url, getAvatarId(), LLSD().with("notes", notes)));
        }
        else
        {
            LL_WARNS() << "Failed to update notes, no cap found" << LL_ENDL;
        }
    }
}

void LLPanelProfileNotes::rightsConfirmationCallback(const LLSD& notification,
        const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option != 0)
    {
        mEditObjectRights->setValue(mEditObjectRights->getValue().asBoolean() ? FALSE : TRUE);
    }
}

void LLPanelProfileNotes::confirmModifyRights(bool grant)
{
    LLSD args;
    args["NAME"] = LLSLURL("agent", getAvatarId(), "completename").getSLURLString();


    LLNotificationsUtil::add(grant ? "GrantModifyRights" : "RevokeModifyRights", args, LLSD(),
        boost::bind(&LLPanelProfileNotes::rightsConfirmationCallback, this, _1, _2));

}

void LLPanelProfileNotes::onCommitRights()
{
	const LLRelationship* buddy_relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());

	if (!buddy_relationship)
	{
		LL_WARNS("LegacyProfile") << "Trying to modify rights for non-friend avatar. Skipped." << LL_ENDL;
		return;
	}

	bool allow_modify_objects = mEditObjectRights->getValue().asBoolean();

	// if modify objects checkbox clicked
	if (buddy_relationship->isRightGrantedTo(
		LLRelationship::GRANT_MODIFY_OBJECTS) != allow_modify_objects)
	{
		confirmModifyRights(allow_modify_objects);
	}
}

void LLPanelProfileNotes::applyRights()
{
    const LLRelationship* buddy_relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());

    if (!buddy_relationship)
    {
        // Lets have a warning log message instead of having a crash. EXT-4947.
        LL_WARNS("LegacyProfile") << "Trying to modify rights for non-friend avatar. Skipped." << LL_ENDL;
        return;
    }

    S32 rights = 0;

    if (mOnlineStatus->getValue().asBoolean())
    {
        rights |= LLRelationship::GRANT_ONLINE_STATUS;
    }
    if (mMapRights->getValue().asBoolean())
    {
        rights |= LLRelationship::GRANT_MAP_LOCATION;
    }
    if (mEditObjectRights->getValue().asBoolean())
    {
        rights |= LLRelationship::GRANT_MODIFY_OBJECTS;
    }

    LLAvatarPropertiesProcessor::getInstance()->sendFriendRights(getAvatarId(), rights);
}

void LLPanelProfileNotes::processProperties(void* data, EAvatarProcessorType type)
{
    if (APT_NOTES == type)
    {
        LLAvatarNotes* avatar_notes = static_cast<LLAvatarNotes*>(data);
        if (avatar_notes && getAvatarId() == avatar_notes->target_id)
        {
            processProperties(avatar_notes);
            LLAvatarPropertiesProcessor::getInstance()->removeObserver(getAvatarId(),this);
        }
    }
}

void LLPanelProfileNotes::processProperties(LLAvatarNotes* avatar_notes)
{
    mNotesEditor->setValue(avatar_notes->notes);
    mNotesEditor->setEnabled(TRUE);
    updateButtons();
}

void LLPanelProfileNotes::resetData()
{
    resetLoading();
    mNotesEditor->setValue(LLStringUtil::null);
    mOnlineStatus->setValue(FALSE);
    mMapRights->setValue(FALSE);
    mEditObjectRights->setValue(FALSE);

    mURLWebProfile.clear();
}

void LLPanelProfileNotes::enableCheckboxes(bool enable)
{
    mOnlineStatus->setEnabled(enable);
    mMapRights->setEnabled(enable);
    mEditObjectRights->setEnabled(enable);
}

// virtual, called by LLAvatarTracker
void LLPanelProfileNotes::changed(U32 mask)
{
    // update rights to avoid have checkboxes enabled when friendship is terminated. EXT-4947.
    fillRightsData();
}

void LLPanelProfileNotes::setAvatarId(const LLUUID& avatar_id)
{
    if (avatar_id.notNull())
    {
        if (getAvatarId().notNull())
        {
            LLAvatarTracker::instance().removeParticularFriendObserver(getAvatarId(), this);
        }
        LLPanelProfileTab::setAvatarId(avatar_id);
        LLAvatarTracker::instance().addParticularFriendObserver(getAvatarId(), this);
    }
}

void LLPanelProfileNotes::onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
{
    mAvatarNameCacheConnection.disconnect();

    std::string username = av_name.getAccountName();
    if (username.empty())
    {
        username = LLCacheName::buildUsername(av_name.getDisplayName());
    }
    else
    {
        LLStringUtil::replaceChar(username, ' ', '.');
    }

    mURLWebProfile = getProfileURL(username, false);
}


//////////////////////////////////////////////////////////////////////////
// LLPanelProfile

LLPanelProfile::LLPanelProfile()
 : LLPanelProfileTab()
{
}

LLPanelProfile::~LLPanelProfile()
{
}

BOOL LLPanelProfile::postBuild()
{
    return TRUE;
}

void LLPanelProfile::processProperties(void* data, EAvatarProcessorType type)
{
    //*TODO: figure out what this does
    mTabContainer->setCommitCallback(boost::bind(&LLPanelProfile::onTabChange, this));

    // Load data on currently opened tab as well
    onTabChange();
}

void LLPanelProfile::onTabChange()
{
    LLPanelProfileTab* active_panel = dynamic_cast<LLPanelProfileTab*>(mTabContainer->getCurrentPanel());
    if (active_panel)
    {
        active_panel->updateData();
    }
    updateBtnsVisibility();
}

void LLPanelProfile::updateBtnsVisibility()
{
    getChild<LLUICtrl>("ok_btn")->setVisible(((getSelfProfile() && !getEmbedded()) || isNotesTabSelected()));
    getChild<LLUICtrl>("cancel_btn")->setVisible(((getSelfProfile() && !getEmbedded()) || isNotesTabSelected()));
}

void LLPanelProfile::onOpen(const LLSD& key)
{
    LLUUID avatar_id = key["id"].asUUID();

    // Don't reload the same profile
    if (getAvatarId() == avatar_id)
    {
        return;
    }

    LLPanelProfileTab::onOpen(avatar_id);

    mTabContainer       = getChild<LLTabContainer>("panel_profile_tabs");
    mPanelSecondlife    = findChild<LLPanelProfileSecondLife>(PANEL_SECONDLIFE);
    mPanelWeb           = findChild<LLPanelProfileWeb>(PANEL_WEB);
    mPanelInterests     = findChild<LLPanelProfileInterests>(PANEL_INTERESTS);
    mPanelPicks         = findChild<LLPanelProfilePicks>(PANEL_PICKS);
    mPanelClassifieds   = findChild<LLPanelProfileClassifieds>(PANEL_CLASSIFIEDS);
    mPanelFirstlife     = findChild<LLPanelProfileFirstLife>(PANEL_FIRSTLIFE);
    mPanelNotes         = findChild<LLPanelProfileNotes>(PANEL_NOTES);

    mPanelSecondlife->onOpen(avatar_id);
    mPanelWeb->onOpen(avatar_id);
    mPanelInterests->onOpen(avatar_id);
    mPanelPicks->onOpen(avatar_id);
    mPanelClassifieds->onOpen(avatar_id);
    mPanelFirstlife->onOpen(avatar_id);
    mPanelNotes->onOpen(avatar_id);

    mPanelSecondlife->setEmbedded(getEmbedded());
    mPanelWeb->setEmbedded(getEmbedded());
    mPanelInterests->setEmbedded(getEmbedded());
    mPanelPicks->setEmbedded(getEmbedded());
    mPanelClassifieds->setEmbedded(getEmbedded());
    mPanelFirstlife->setEmbedded(getEmbedded());
    mPanelNotes->setEmbedded(getEmbedded());

    // Always request the base profile info
    resetLoading();
    updateData();

    updateBtnsVisibility();

    // KC - Not handling pick and classified opening thru onOpen
    // because this would make unique profile floaters per slurl
    // and result in multiple profile floaters for the same avatar
}

void LLPanelProfile::updateData()
{
    LLUUID avatar_id = getAvatarId();
    // Todo: getIsloading functionality needs to be expanded to
    // include 'inited' or 'data_provided' state to not rerequest
    if (!getIsLoading() && avatar_id.notNull())
    {
        setIsLoading();

        std::string cap_url = gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP);
        if (!cap_url.empty())
        {
            LLCoros::instance().launch("requestAgentUserInfoCoro",
                boost::bind(request_avatar_properties_coro, cap_url, avatar_id));
        }
    }
}

void LLPanelProfile::apply()
{
    if (getSelfProfile())
    {
        //KC - AvatarData is spread over 3 different panels
        // collect data from the last 2 and give to the first to save
        mPanelFirstlife->apply(&mAvatarData);
        mPanelWeb->apply(&mAvatarData);
        mPanelSecondlife->apply(&mAvatarData);

        mPanelInterests->apply();
        mPanelPicks->apply();
        mPanelNotes->apply();
        mPanelClassifieds->apply();

        //KC - Classifieds handles this itself
    }
    else
    {
        mPanelNotes->apply();
    }
}

void LLPanelProfile::showPick(const LLUUID& pick_id)
{
    if (pick_id.notNull())
    {
        mPanelPicks->selectPick(pick_id);
    }
    mTabContainer->selectTabPanel(mPanelPicks);
}

bool LLPanelProfile::isPickTabSelected()
{
	return (mTabContainer->getCurrentPanel() == mPanelPicks);
}

bool LLPanelProfile::isNotesTabSelected()
{
	return (mTabContainer->getCurrentPanel() == mPanelNotes);
}

void LLPanelProfile::showClassified(const LLUUID& classified_id, bool edit)
{
    if (classified_id.notNull())
    {
        mPanelClassifieds->selectClassified(classified_id, edit);
    }
    mTabContainer->selectTabPanel(mPanelClassifieds);
}



