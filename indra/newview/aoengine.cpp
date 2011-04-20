/**
 * @file aoengine.cpp
 * @brief The core Animation Overrider engine
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Zi Ree @ Second Life
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
 */

#include "llviewerprecompiledheaders.h"


#include "roles_constants.h"

#include "aoengine.h"
#include "aoset.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llanimationstates.h"
#include "llassetstorage.h"
#include "llcommon.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llinventoryobserver.h"
#include "llstring.h"
#include "llvfs.h"
#include "llviewerinventory.h"

// is there a global define for this folder somewhere?
#define ROOT_FIRESTORM_FOLDER 	"#Firestorm"
#define ROOT_AO_FOLDER			"#AO"
#include <boost/graph/graph_concepts.hpp>

const F32 INVENTORY_POLLING_INTERVAL=5.0;

AOEngine::AOEngine() :
	LLSingleton<AOEngine>(),
	mCurrentSet(0),
	mDefaultSet(0),
	mEnabled(FALSE),
	mInMouselook(FALSE),
	mImportSet(0),
	mImportCategory(LLUUID::null),
	mAOFolder(LLUUID::null),
	mLastMotion(ANIM_AGENT_STAND),
	mLastOverriddenMotion(ANIM_AGENT_STAND)
{
}

AOEngine::~AOEngine()
{
	clear();
}

void AOEngine::init()
{
}

// static
void AOEngine::onLoginComplete()
{
	AOEngine::instance().init();
}

void AOEngine::clear()
{
	mSets.clear();
	mCurrentSet=0;
}

void AOEngine::stopAllStandVariants()
{
	llwarns << "stopping all STAND variants." << llendl;
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_1,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_2,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_3,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_STAND_4,ANIM_REQUEST_STOP);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_1);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_2);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_3);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_4);
}

void AOEngine::stopAllSitVariants()
{
	llwarns << "stopping all SIT variants." << llendl;
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_FEMALE,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GROUND,ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GROUND_CONSTRAINED,ANIM_REQUEST_STOP);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_FEMALE);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GENERIC);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GROUND);
	gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GROUND_CONSTRAINED);
}

void AOEngine::setLastMotion(LLUUID motion)
{
	if(motion!=ANIM_AGENT_TYPE)
		mLastMotion=motion;
}

void AOEngine::setLastOverriddenMotion(LLUUID motion)
{
	if(motion!=ANIM_AGENT_TYPE)
		mLastOverriddenMotion=motion;
}

BOOL AOEngine::foreignAnimations()
{
	for(LLVOAvatar::AnimSourceIterator srcIt=gAgentAvatarp->mAnimationSources.begin();
		srcIt!=gAgentAvatarp->mAnimationSources.end();srcIt++)
	{
		llwarns << srcIt->first << " - " << srcIt->second << llendl;
		if(srcIt->first!=gAgent.getID())
		{
			return TRUE;
		}
	}
	return FALSE;
}

void AOEngine::enable(BOOL yes)
{
	llwarns << "using " << mLastMotion << " enable " << yes << llendl;
	mEnabled=yes;

	if(!mCurrentSet)
	{
		llwarns << "enable(" << yes << ") without animation set loaded." << llendl;
		return;
	}

	AOSet::AOState* state=mCurrentSet->getStateByRemapID(mLastMotion);
	if(mEnabled)
	{
		if(state && !state->mAnimations.empty())
		{
			llwarns << "Enabling animation state " << state->mName << llendl;

			gAgent.sendAnimationRequest(mLastOverriddenMotion,ANIM_REQUEST_STOP);

			LLUUID animation=override(mLastMotion,TRUE);
			if(animation.isNull())
				return;

			if(mLastMotion==ANIM_AGENT_STAND)
			{
				stopAllStandVariants();
			}
			else if(mLastMotion==ANIM_AGENT_WALK)
			{
				llwarns << "Last motion was a WALK, stopping all variants." << llendl;
				gAgent.sendAnimationRequest(ANIM_AGENT_WALK_NEW,ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_WALK,ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_WALK_NEW,ANIM_REQUEST_STOP);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_WALK_NEW);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_WALK);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_WALK_NEW);
			}
			else if(mLastMotion==ANIM_AGENT_RUN)
			{
				llwarns << "Last motion was a RUN, stopping all variants." << llendl;
				gAgent.sendAnimationRequest(ANIM_AGENT_RUN_NEW,ANIM_REQUEST_STOP);
				gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_RUN_NEW,ANIM_REQUEST_STOP);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_RUN_NEW);
				gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_RUN_NEW);
			}
			else if(mLastMotion==ANIM_AGENT_SIT)
			{
				stopAllSitVariants();
			}
			else
				llwarns << "Unhandled last motion id " << mLastMotion << llendl;

			gAgent.sendAnimationRequest(animation,ANIM_REQUEST_START);
		}
	}
	else
	{
		// stop all overriders, catch leftovers
		for(S32 index=0;index<AOSet::AOSTATES_MAX;index++)
		{
			state=mCurrentSet->getState(index);
			if(state)
			{
				LLUUID animation=state->mCurrentAnimationID;
				if(animation.notNull())
				{
					llwarns << "Stopping leftover animation from state " << index << llendl;
					gAgent.sendAnimationRequest(animation,ANIM_REQUEST_STOP);
				}
			}
			else
				lldebugs << "state "<< index <<" returned NULL." << llendl;
		}

		if(!foreignAnimations())
			gAgent.sendAnimationRequest(mLastMotion,ANIM_REQUEST_START);
		llwarns << "stopTimer()" << llendl;
		mCurrentSet->stopTimer();
	}
}

const LLUUID AOEngine::override(const LLUUID motion,BOOL start)
{
	LLUUID animation;

	if(!mEnabled)
	{
		if(start && mCurrentSet)
		{
			const AOSet::AOState* state=mCurrentSet->getStateByRemapID(motion);
			if(state)
			{
				setLastMotion(motion);
				llwarns << "(disabled AO) setting last motion id to " <<  gAnimLibrary.animStateToString(mLastMotion) << llendl;
				if(!state->mAnimations.empty())
				{
					setLastOverriddenMotion(motion);
					llwarns << "(disabled AO) setting last overridden motion id to " <<  gAnimLibrary.animStateToString(mLastOverriddenMotion) << llendl;
				}
			}
		}
		return animation;
	}

	if(mSets.empty())
	{
		lldebugs << "No sets loaded. Skipping overrider." << llendl;
		return animation;
	}

	if(!mCurrentSet)
	{
		lldebugs << "No current AO set chosen. Skipping overrider." << llendl;
		return animation;
	}

	AOSet::AOState* state=mCurrentSet->getStateByRemapID(motion);
	if(!state)
	{
		lldebugs << "No current AO state for motion " << gAnimLibrary.animStateToString(motion) << " - Skipping overrider." << llendl;
		return animation;
	}

	llwarns << "stopTimer()" << llendl;
	mCurrentSet->stopTimer();
	if(start)
	{
		// Disable start stands in Mouselook
		if(mCurrentSet->getMouselookDisable() &&
			motion==ANIM_AGENT_STAND &&
			mInMouselook)
		{
			setLastMotion(motion);
			llwarns << "(enabled AO, mouselook stand stopped) setting last motion id to " <<  gAnimLibrary.animStateToString(mLastMotion) << llendl;
			return animation;
		}

		// Do not start override sits if not selected
		if(!mCurrentSet->getSitOverride() && motion==ANIM_AGENT_SIT)
		{
			setLastMotion(motion);
			llwarns << "(enabled AO, sit override stopped) setting last motion id to " <<  gAnimLibrary.animStateToString(mLastMotion) << llendl;
			return animation;
		}

		setLastMotion(motion);
		llwarns << "(enabled AO) setting last motion id to " <<  gAnimLibrary.animStateToString(mLastMotion) << llendl;

		if(!state->mAnimations.empty())
		{
			setLastOverriddenMotion(motion);
			llwarns << "(enabled AO) setting last overridden motion id to " <<  gAnimLibrary.animStateToString(mLastOverriddenMotion) << llendl;
		}

		// do not remember typing as set-wide motion
		if(motion!=ANIM_AGENT_TYPE)
			mCurrentSet->setMotion(motion);

		animation=mCurrentSet->getAnimationForState(state);
		state->mCurrentAnimationID=animation;
		llwarns << "overriding " <<  gAnimLibrary.animStateToString(motion)
				<< " with " << animation
				<< " in state " << state->mName
				<< " of set " << mCurrentSet->getName()
				<< " (" << mCurrentSet << ")" << llendl;

		F32 timeout=state->mCycleTime;
		llwarns << "Setting cycle timeout for state " << state->mName << " of " << timeout << llendl;
		if(timeout>0.0f)
			mCurrentSet->startTimer(timeout);

		if(motion==ANIM_AGENT_SIT)
			mSitCancelTimer.oneShot();
	}
	else
	{
		animation=state->mCurrentAnimationID;
		state->mCurrentAnimationID.setNull();
		
		// for typing animaiton, just return the stored animation and don't memorize anything else
		if(motion==ANIM_AGENT_TYPE)
			return animation;

		if(motion!=mCurrentSet->getMotion())
		{
			llwarns << "trying to stop-override motion " <<  gAnimLibrary.animStateToString(motion)
					<< " but the current set has motion " <<  gAnimLibrary.animStateToString(mCurrentSet->getMotion()) << llendl;
			return animation;
		}

		mCurrentSet->setMotion(LLUUID::null);

		// stop the underlying Linden Lab motion, in case it's still running.
		// frequently happens with sits, so we keep it only for those currently.
		if(mLastMotion==ANIM_AGENT_SIT)
			stopAllSitVariants();

		llwarns << "stopping cycle timer for motion " <<  gAnimLibrary.animStateToString(motion) <<
					" using animation " << animation <<
					" in state " << state->mName << llendl;
	}

	return animation;
}

void AOEngine::checkSitCancel()
{
	if(foreignAnimations())
	{
		LLUUID animation=mCurrentSet->getStateByRemapID(ANIM_AGENT_SIT)->mCurrentAnimationID;
		if(animation.notNull())
		{
			llwarns << "Stopping sit animation due to foreign animations running" << llendl;
			gAgent.sendAnimationRequest(animation,ANIM_REQUEST_STOP);
			mSitCancelTimer.stop();
		}
	}
}

void AOEngine::cycleTimeout(const AOSet* set)
{
	if(!mEnabled)
		return;

	if(set!=mCurrentSet)
	{
		llwarns << "cycleTimeout for set " << set->getName() << " but ouot current set is " << mCurrentSet->getName() << llendl;
		return;
	}

	cycle(CycleAny);
}

void AOEngine::cycle(eCycleMode cycleMode)
{
	LLUUID motion=mCurrentSet->getMotion();

	if(motion==ANIM_AGENT_SIT && !mCurrentSet->getSitOverride())
		return;

	if(motion==ANIM_AGENT_STAND && mCurrentSet->getMouselookDisable() && mInMouselook)
		return;

	AOSet::AOState* state=mCurrentSet->getStateByRemapID(motion);
	if(!state)
	{
		lldebugs << "cycle without state." << llendl;
		return;
	}

	LLUUID animation=state->mCurrentAnimationID;
	if(!animation.isNull())
		gAgent.sendAnimationRequest(animation,ANIM_REQUEST_STOP);

	if(cycleMode==CycleAny)
	{
		llwarns << "CycleAny" << llendl;
		animation=override(motion,TRUE);
	}
	else
	{
		if(cycleMode==CyclePrevious)
		{
			llwarns << "CyclePrevious" << llendl;
			state->mCurrentAnimation--;
			if(state->mCurrentAnimation<0)
				state->mCurrentAnimation=state->mAnimations.size()-1;
		}
		else if(cycleMode==CycleNext)
		{
			llwarns << "CycleNext" << llendl;
			state->mCurrentAnimation++;
			if(state->mCurrentAnimation==state->mAnimations.size())
				state->mCurrentAnimation=0;
		}
		animation=state->mAnimations[state->mCurrentAnimation].mAssetUUID;
		state->mCurrentAnimationID=animation;
	}

	if(!animation.isNull())
		gAgent.sendAnimationRequest(animation,ANIM_REQUEST_START);
}

void AOEngine::updateSortOrder(AOSet::AOState* state)
{
	for(S32 index=0;index<state->mAnimations.size();index++)
	{
		S32 sortOrder=state->mAnimations[index].mSortOrder;

		if(sortOrder!=index)
		{
			std::ostringstream numStr("");
			numStr << index;

			llwarns << "sort order is " << sortOrder << " but index is " << index
					<< ", setting sort order description: " << numStr.str() << llendl;

			state->mAnimations[index].mSortOrder=index;

			LLViewerInventoryItem* item=gInventory.getItem(state->mAnimations[index].mInventoryUUID);
			if(!item)
			{
				llwarns << "NULL inventory item found while trying to copy " << state->mAnimations[index].mInventoryUUID << llendl;
				continue;
			}
			LLPointer<LLViewerInventoryItem> newItem=
				new LLViewerInventoryItem(item);

			llwarns << newItem->getUUID() << " " << newItem->getName() << llendl;

			newItem->setDescription(numStr.str());
			newItem->setComplete(TRUE);
			newItem->updateServer(FALSE);

			gInventory.updateItem(newItem);
		}
	}
}

LLUUID AOEngine::addSet(const std::string name,BOOL reload)
{
	llwarns << "adding set folder " << name << llendl;
	LLUUID newUUID=gInventory.createNewCategory(mAOFolder,LLFolderType::FT_NONE,name);

	if(reload)
		mTimerCollection.enableReloadTimer(TRUE);
	return newUUID;
}

BOOL AOEngine::createAnimationLink(const AOSet* set,AOSet::AOState* state,const LLInventoryItem* item)
{
	llwarns << "Asset ID " << item->getAssetUUID() << " inventory id " << item->getUUID() << " category id " << state->mInventoryUUID << llendl;
	if(state->mInventoryUUID.isNull())
	{
		llwarns << "no " << state->mName << " folder yet. Creating ..." << llendl;
		gInventory.createNewCategory(set->getInventoryUUID(),LLFolderType::FT_NONE,state->mName);

		llwarns << "looking for folder to get UUID ..." << llendl;

		LLUUID newStateFolderUUID;

		LLInventoryModel::item_array_t* items;
		LLInventoryModel::cat_array_t* cats;
		gInventory.getDirectDescendentsOf(set->getInventoryUUID(),cats,items);

		for(S32 index=0;index<cats->count();index++)
		{
			if(cats->get(index)->getName().compare(state->mName)==0)
			{
				llwarns << "UUID found!" << llendl;
				newStateFolderUUID=cats->get(index)->getUUID();
				state->mInventoryUUID=newStateFolderUUID;
				break;
			}
		}
	}

	if(state->mInventoryUUID.isNull())
	{
		llwarns << "state inventory UUID not found, failing." << llendl;
		return FALSE;
	}

	link_inventory_item(
		gAgent.getID(),
		item->getUUID(),
		state->mInventoryUUID,
		item->getName(),
		item->getDescription(),
		LLAssetType::AT_LINK,
		NULL);

	return TRUE;
}

BOOL AOEngine::addAnimation(const AOSet* set,AOSet::AOState* state,const LLInventoryItem* item,BOOL reload)
{
	AOSet::AOAnimation anim;
	anim.mAssetUUID=item->getAssetUUID();
	anim.mInventoryUUID=item->getUUID();
	anim.mName=item->getName();
	anim.mSortOrder=state->mAnimations.size()+1;
	state->mAnimations.push_back(anim);

	createAnimationLink(set,state,item);

	if(reload)
		mTimerCollection.enableReloadTimer(TRUE);
	return TRUE;
}

void AOEngine::purgeFolder(LLUUID uuid)
{
	// trash it
	remove_category(&gInventory,uuid);

	// clean it
	gInventory.purgeDescendentsOf(uuid);
	gInventory.notifyObservers();

	// purge it
	gInventory.purgeObject(uuid);
	gInventory.notifyObservers();
}

BOOL AOEngine::removeSet(AOSet* set)
{
	purgeFolder(set->getInventoryUUID());
	mTimerCollection.enableReloadTimer(TRUE);
	return TRUE;
}

BOOL AOEngine::removeAnimation(const AOSet* set,AOSet::AOState* state,S32 index)
{
	S32 numOfAnimations=state->mAnimations.size();
	if(numOfAnimations==0)
		return FALSE;

	llwarns << __LINE__ << " purging: " << state->mAnimations[index].mInventoryUUID << llendl;
	gInventory.purgeObject(state->mAnimations[index].mInventoryUUID); // item->getUUID());
	gInventory.notifyObservers();

	state->mAnimations.erase(state->mAnimations.begin()+index);

	if(state->mAnimations.size()==0)
	{
		llwarns << "purging folder " << state->mName << " from inventory because it's empty." << llendl;

		LLInventoryModel::item_array_t* items;
		LLInventoryModel::cat_array_t* cats;
		gInventory.getDirectDescendentsOf(set->getInventoryUUID(),cats,items);

		for(S32 index=0;index<cats->count();index++)
		{
			std::vector<std::string> params;
			LLStringUtil::getTokens(cats->get(index)->getName(),params,":");
			std::string stateName=params[0];

			if(state->mName.compare(stateName)==0)
			{
				llwarns << "folder found: " << cats->get(index)->getName() << " purging uuid " << cats->get(index)->getUUID() << llendl;

				purgeFolder(cats->get(index)->getUUID());
				state->mInventoryUUID.setNull();
				break;
			}
		}
	}
	else
		updateSortOrder(state);

	return TRUE;
}

BOOL AOEngine::swapWithPrevious(AOSet::AOState* state,S32 index)
{
	S32 numOfAnimations=state->mAnimations.size();
	if(numOfAnimations<2 || index==0)
		return FALSE;

	AOSet::AOAnimation tmpAnim=state->mAnimations[index];
	state->mAnimations.erase(state->mAnimations.begin()+index);
	state->mAnimations.insert(state->mAnimations.begin()+index-1,tmpAnim);

	updateSortOrder(state);

	return TRUE;
}

BOOL AOEngine::swapWithNext(AOSet::AOState* state,S32 index)
{
	S32 numOfAnimations=state->mAnimations.size();
	if(numOfAnimations<2 || index==(numOfAnimations-1))
		return FALSE;

	AOSet::AOAnimation tmpAnim=state->mAnimations[index];
	state->mAnimations.erase(state->mAnimations.begin()+index);
	state->mAnimations.insert(state->mAnimations.begin()+index+1,tmpAnim);

	updateSortOrder(state);

	return TRUE;
}

void AOEngine::reloadStateAnimations(AOSet::AOState* state)
{
	LLInventoryModel::item_array_t* items;
	LLInventoryModel::cat_array_t* dummy;

	state->mAnimations.clear();

	gInventory.getDirectDescendentsOf(state->mInventoryUUID,dummy,items);
	for(S32 num=0;num<items->count();num++)
	{
		llwarns << "Found animation link " << items->get(num)->LLInventoryItem::getName()
				<< " desc " << items->get(num)->LLInventoryItem::getDescription()
				<< " asset " << items->get(num)->getAssetUUID() << llendl;

		AOSet::AOAnimation anim;
		anim.mAssetUUID=items->get(num)->getAssetUUID();
		LLViewerInventoryItem* linkedItem=items->get(num)->getLinkedItem();
		if(linkedItem==0)
		{
			llwarns << "linked item for link " << items->get(num)->LLInventoryItem::getName() << " not found. Skipping." << llendl;
			continue;
		}
		anim.mName=linkedItem->LLInventoryItem::getName();
		anim.mInventoryUUID=items->get(num)->getUUID();

		S32 sortOrder;
		if(!LLStringUtil::convertToS32(items->get(num)->LLInventoryItem::getDescription(),sortOrder))
			sortOrder=-1;
		anim.mSortOrder=sortOrder;

		llwarns << "current sort order is " << sortOrder << llendl;

		if(sortOrder==-1)
		{
			llwarns << "sort order was unknown so append to the end of the list" << llendl;
			state->mAnimations.push_back(anim);
		}
		else
		{
			BOOL inserted=FALSE;
			for(S32 index=0;index<state->mAnimations.size();index++)
			{
				if(state->mAnimations[index].mSortOrder>sortOrder)
				{
					llwarns << "inserting at index " << index << llendl;
					state->mAnimations.insert(state->mAnimations.begin()+index,anim);
					inserted=TRUE;
					break;
				}
			}
			if(!inserted)
			{
				llwarns << "not inserted yet, appending to the list instead" << llendl;
				state->mAnimations.push_back(anim);
			}
		}
		llwarns << "Animation count now: " << state->mAnimations.size() << llendl;
	}

	updateSortOrder(state);
}

void AOEngine::update()
{
	if(mAOFolder.isNull())
		return;

	LLInventoryModel::cat_array_t* categories;
	LLInventoryModel::item_array_t* items;

	BOOL allComplete=TRUE;
	mTimerCollection.enableSettingsTimer(FALSE);

	gInventory.getDirectDescendentsOf(mAOFolder,categories,items);
	for(S32 index=0;index<categories->count();index++)
	{
		LLViewerInventoryCategory* currentCategory=categories->get(index);
		const std::string& setFolderName=currentCategory->getName();
		std::vector<std::string> params;
		LLStringUtil::getTokens(setFolderName,params,":");

		AOSet* newSet=getSetByName(params[0]);
		if(newSet==0)
		{
			lldebugs << "Adding set " << setFolderName << " to AO." << llendl;
			newSet=new AOSet(currentCategory->getUUID());
			newSet->setName(params[0]);
			mSets.push_back(newSet);
		}
		else
		{
			if(newSet->getComplete())
			{
				lldebugs << "Set " << params[0] << " already complete. Skipping." << llendl;
				continue;
			}
			lldebugs << "Updating set " << setFolderName << " in AO." << llendl;
		}
		allComplete=FALSE;

		for(S32 num=1;num<params.size();num++)
		{
			if(params[num].size()!=2)
				llwarns << "Unknown AO set option " << params[num] << llendl;
			else if(params[num]=="SO")
				newSet->setSitOverride(TRUE);
			else if(params[num]=="DM")
				newSet->setMouselookDisable(TRUE);
			else if(params[num]=="**")
			{
				mDefaultSet=newSet;
				mCurrentSet=newSet;
			}
			else
				llwarns << "Unknown AO set option " << params[num] << llendl;
		}

		if(gInventory.isCategoryComplete(currentCategory->getUUID()))
		{
			lldebugs << "Set " << params[0] << " is complete, reading states ..." << llendl;

			LLInventoryModel::cat_array_t* stateCategories;
			gInventory.getDirectDescendentsOf(currentCategory->getUUID(),stateCategories,items);
			newSet->setComplete(TRUE);

			for(S32 index=0;index<stateCategories->count();index++)
			{
				std::vector<std::string> params;
				LLStringUtil::getTokens(stateCategories->get(index)->getName(),params,":");
				std::string stateName=params[0];

				AOSet::AOState* state=newSet->getStateByName(stateName);
				if(state==NULL)
				{
					llwarns << "Unknown state " << stateName << ". Skipping." << llendl;
					continue;
				}
				lldebugs << "Reading state " << stateName << llendl;

				state->mInventoryUUID=stateCategories->get(index)->getUUID();
				for(S32 num=1;num<params.size();num++)
				{
					if(params[num]=="RN")
					{
						state->mRandom=TRUE;
						lldebugs << "Random on" << llendl;
					}
					else if(params[num].substr(0,2)=="CT")
					{
						LLStringUtil::convertToS32(params[num].substr(2,params[num].size()-2),state->mCycleTime);
						lldebugs << "Cycle Time specified:" << state->mCycleTime << llendl;
					}
					else
						llwarns << "Unknown AO set option " << params[num] << llendl;
				}

				if(!gInventory.isCategoryComplete(state->mInventoryUUID))
				{
					llwarns << "State category " << stateName << " is incomplete, fetching descendents" << llendl;
					gInventory.fetchDescendentsOf(state->mInventoryUUID);
					allComplete=FALSE;
					newSet->setComplete(FALSE);
					continue;
				}
				reloadStateAnimations(state);
			}
		}
		else
		{
			llwarns << "Set " << params[0] << " is incomplete, fetching descendents" << llendl;
			gInventory.fetchDescendentsOf(currentCategory->getUUID());
		}
	}

	if(allComplete)
	{
		if(!mCurrentSet && !mSets.empty())
		{
			llwarns << "No default set defined, choosing the first in the list." << llendl;
			mCurrentSet=mSets[0];
		}
		mTimerCollection.enableInventoryTimer(FALSE);
		mTimerCollection.enableSettingsTimer(TRUE);

		// this should be a preferences option
		mEnabled=TRUE;
		selectSet(mCurrentSet);

		llwarns << "sending update signal" << llendl;
		mUpdatedSignal();
	}
}

void AOEngine::reload()
{
	BOOL wasEnabled=mEnabled;

	mTimerCollection.enableReloadTimer(FALSE);

	if(wasEnabled)
		enable(FALSE);

	gAgent.stopCurrentAnimations();
	mLastOverriddenMotion=ANIM_AGENT_STAND;

	clear();
	mTimerCollection.enableInventoryTimer(TRUE);
	update();

	if(wasEnabled)
		enable(TRUE);
}

AOSet* AOEngine::getSetByName(const std::string name)
{
	AOSet* found=0;
	for(S32 index=0;index<mSets.size();index++)
	{
		if(mSets[index]->getName().compare(name)==0)
		{
			found=mSets[index];
			break;
		}
	}
	return found;
}

const std::string AOEngine::getCurrentSetName() const
{
	if(mCurrentSet)
		return mCurrentSet->getName();
	return std::string();
}

const AOSet* AOEngine::getDefaultSet() const
{
	return mDefaultSet;
}

void AOEngine::selectSet(AOSet* set)
{
	BOOL wasEnabled=mEnabled;
	if(wasEnabled)
		enable(FALSE);

	gAgent.stopCurrentAnimations();
	mLastOverriddenMotion=ANIM_AGENT_STAND;

	mCurrentSet=set;
	llwarns << "Selected AO set " << set->getName() << llendl;

	if(wasEnabled)
		enable(TRUE);
}

AOSet* AOEngine::selectSetByName(const std::string name)
{
	AOSet* set=getSetByName(name);
	if(set)
	{
		selectSet(set);
		return set;
	}
	llwarns << "Could not find AO set " << name << llendl;
	return NULL;
}

const std::vector<AOSet*> AOEngine::getSetList() const
{
	return mSets;
}

void AOEngine::saveSet(const AOSet* set)
{
	if(!set)
		return;

	std::string setParams=set->getName();
	if(set->getSitOverride())
		setParams+=":SO";
	if(set->getMouselookDisable())
		setParams+=":DM";
	if(set==mDefaultSet)
		setParams+=":**";

/*
	// This works fine, but LL seems to have added a few helper functions in llinventoryfunctions.h
	// so let's make use of them. This code is just for reference

	LLViewerInventoryCategory* cat=gInventory.getCategory(set->getInventoryUUID());
	llwarns << cat << llendl;
	cat->rename(setParams);
	cat->updateServer(FALSE);
	gInventory.addChangedMask(LLInventoryObserver::LABEL, cat->getUUID());
	gInventory.notifyObservers();
*/
	rename_category(&gInventory,set->getInventoryUUID(),setParams);
}

void AOEngine::saveState(const AOSet::AOState* state)
{
	std::string stateParams=state->mName;
	F32 time=state->mCycleTime;
	if(time>0.0)
	{
		std::ostringstream timeStr;
		timeStr << ":CT" << state->mCycleTime;
		stateParams+=timeStr.str();
	}
	if(state->mRandom)
		stateParams+=":RN";

	rename_category(&gInventory,state->mInventoryUUID,stateParams);
}

void AOEngine::saveSettings()
{
	for(S32 index=0;index<mSets.size();index++)
	{
		AOSet* set=mSets[index];
		if(set->getDirty())
		{
			saveSet(set);
			llwarns << "dirty set saved " << set->getName() << llendl;
			set->setDirty(FALSE);
		}

		for(S32 stateIndex=0;stateIndex<AOSet::AOSTATES_MAX;stateIndex++)
		{
			AOSet::AOState* state=set->getState(stateIndex);
			if(state->mDirty)
			{
				saveState(state);
				llwarns << "dirty state saved " << state->mName << llendl;
				state->mDirty=FALSE;
			}
		}
	}
}

void AOEngine::inMouselook(BOOL yes)
{
	if(mInMouselook==yes)
		return;

	mInMouselook=yes;

	llwarns << "mouselook mode " << yes << llendl;
	if(!mCurrentSet)
		return;

	if(!mCurrentSet->getMouselookDisable())
		return;

	if(!mEnabled)
		return;

	if(mLastMotion!=ANIM_AGENT_STAND)
		return;

	if(yes)
	{
		AOSet::AOState* state=mCurrentSet->getState(AOSet::Standing);
		if(!state)
			return;

		LLUUID animation=state->mCurrentAnimationID;
		if(!animation.isNull())
		{
			gAgent.sendAnimationRequest(animation,ANIM_REQUEST_STOP);
			llwarns << " stopped animation " << animation << " in state " << state->mName << llendl;
		}
		gAgent.sendAnimationRequest(ANIM_AGENT_STAND,ANIM_REQUEST_START);
	}
	else
	{
		stopAllStandVariants();
		gAgent.sendAnimationRequest(override(ANIM_AGENT_STAND,TRUE),ANIM_REQUEST_START);
	}
}

void AOEngine::setDefaultSet(AOSet* set)
{
	mDefaultSet=set;
	for(S32 index=0;index<mSets.size();index++)
		mSets[index]->setDirty(TRUE);

	llwarns << "default set now " << set << llendl;
}

void AOEngine::setOverrideSits(AOSet* set,BOOL yes)
{
	set->setSitOverride(yes);
	set->setDirty(TRUE);

	if(mLastMotion!=ANIM_AGENT_SIT)
		return;

	if(yes)
	{
		stopAllSitVariants();
		gAgent.sendAnimationRequest(override(ANIM_AGENT_SIT,TRUE),ANIM_REQUEST_START);
	}
	else
	{
		AOSet::AOState* state=mCurrentSet->getState(AOSet::Sitting);
		if(!state)
			return;

		LLUUID animation=state->mCurrentAnimationID;
		if(!animation.isNull())
			gAgent.sendAnimationRequest(animation,ANIM_REQUEST_STOP);

		gAgent.sendAnimationRequest(ANIM_AGENT_SIT,ANIM_REQUEST_START);
	}
}

void AOEngine::setDisableStands(AOSet* set,BOOL yes)
{
	set->setMouselookDisable(yes);
	set->setDirty(TRUE);
	llwarns << "disable stands in mouselook now " << yes << llendl;

	// make sure an update happens if needed
	mInMouselook=!gAgentCamera.cameraMouselook();
	inMouselook(!mInMouselook);
}

void AOEngine::setRandomize(AOSet::AOState* state,BOOL yes)
{
	state->mRandom=yes;
	state->mDirty=TRUE;
	llwarns << "randomize now " << yes << llendl;
}

void AOEngine::setCycleTime(AOSet::AOState* state,F32 time)
{
	state->mCycleTime=time;
	state->mDirty=TRUE;
	llwarns << "cycle time now " << time << llendl;
}

void AOEngine::tick()
{
	const LLUUID categoryID=gInventory.findCategoryByName(ROOT_FIRESTORM_FOLDER);
	llwarns << "tick()" << categoryID << llendl;
	if(categoryID.isNull())
	{
		llwarns << "no " << ROOT_FIRESTORM_FOLDER << " folder yet. Creating ..." << llendl;
		gInventory.createNewCategory(gInventory.getRootFolderID(),LLFolderType::FT_NONE,ROOT_FIRESTORM_FOLDER);
	}
	else
	{
		LLInventoryModel::cat_array_t* categories;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(categoryID,categories,items);
		llwarns << "cat " << categories->count() << " items " << items->count() << llendl;

		for(S32 index=0;index<categories->count();index++)
		{
			const std::string& catName=categories->get(index)->getName();
			if(catName.compare(ROOT_AO_FOLDER)==0)
			{
				mAOFolder=categories->get(index)->getUUID();
				break;
			}
		}

		if(mAOFolder.isNull())
		{
			llwarns << "no " << ROOT_AO_FOLDER << " folder yet. Creating ..." << llendl;
			gInventory.createNewCategory(categoryID,LLFolderType::FT_NONE,ROOT_AO_FOLDER);
		}
		else
		{
			llwarns << "AO basic folder structure intact." << llendl;
//			LLInventoryModel::cat_array_t* sets;
//			gInventory.getDirectDescendentsOf(categoryID,sets,items);
			update();
		}
	}
}

BOOL AOEngine::importNotecard(const LLInventoryItem* item)
{
	if(item)
	{
		llwarns << "importing notecard: " << item->getName() << llendl;
		if(getSetByName(item->getName()))
		{
			llwarns << "set with this name already exists" << llendl;
			return FALSE;
		}

		if(!gAgent.allowOperation(PERM_COPY,item->getPermissions(),GP_OBJECT_MANIPULATE) && !gAgent.isGodlike())
		{
			llwarns << "Insufficient permissions to read notecard." << llendl;
			return FALSE;
		}

		if(item->getAssetUUID().notNull())
		{
			mImportSet=new AOSet(item->getParentUUID());
			if(!mImportSet)
			{
				llwarns << "could not create import set." << llendl;
				return FALSE;
			}
			mImportSet->setName(item->getName());

			LLUUID* newUUID=new LLUUID(item->getAssetUUID());
			const LLHost sourceSim=LLHost::invalid;

			gAssetStorage->getInvItemAsset
			(
				sourceSim,
				gAgent.getID(),
				gAgent.getSessionID(),
				item->getPermissions().getOwner(),
				LLUUID::null,
				item->getUUID(),
				item->getAssetUUID(),
				item->getType(),
				&onNotecardLoadComplete,
				(void*) newUUID,
				TRUE
			);

			return TRUE;
		}
	}
	return FALSE;
}

// static
void AOEngine::onNotecardLoadComplete(	LLVFS* vfs,const LLUUID& assetUUID,LLAssetType::EType type,
											void* userdata,S32 status,LLExtStat extStatus)
{
	if(status!=LL_ERR_NOERR)
	{
		llwarns << "Error downloading import notecard." << llendl;
		// NULL tells the importer to cancel all operations and free the import set memory
		AOEngine::instance().parseNotecard(NULL);
		return;
	}
	llwarns << "Downloading import notecard complete." << llendl;

	S32 notecardSize=vfs->getSize(assetUUID,type);
	char* buffer=new char[notecardSize];
	vfs->getData(assetUUID,type,(U8*) buffer,0,notecardSize);

	AOEngine::instance().parseNotecard(buffer);
}

void AOEngine::parseNotecard(const char* buffer)
{
	llwarns << "parsing import notecard" << llendl;

	BOOL isValid=FALSE;

	if(!buffer)
	{
		llwarns << "buffer==NULL - aborting import" << llendl;
		// NOTE: cleanup is always the same, needs streamlining
		delete mImportSet;
		mImportSet=0;
		mUpdatedSignal();
		return;
	}

	std::string text(buffer);
	delete buffer;

	std::vector<std::string> lines;
	LLStringUtil::getTokens(text,lines,"\n");

	S32 found=-1;
	for(S32 index=0;index<lines.size();index++)
	{
		if(lines[index].find("Text length ")==0)
		{
			found=index;
			break;
		}
	}

	if(found==-1)
	{
		llwarns << "notecard is missing the text portion" << llendl;
		delete mImportSet;
		mImportSet=0;
		mUpdatedSignal();
		return;
	}

	LLViewerInventoryCategory* importCategory=gInventory.getCategory(mImportSet->getInventoryUUID());
	if(!importCategory)
	{
		llwarns << "couldn't find folder to read the animations" << llendl;
		delete mImportSet;
		mImportSet=0;
		mUpdatedSignal();
		return;
	}

	std::map<std::string,LLUUID> animationMap;
	LLInventoryModel::cat_array_t* dummy;
	LLInventoryModel::item_array_t* items;

	gInventory.getDirectDescendentsOf(mImportSet->getInventoryUUID(),dummy,items);
	for(S32 index=0;index<items->size();index++)
	{
		animationMap[items->get(index)->getName()]=items->get(index)->getUUID();
		llwarns <<	"animation " << items->get(index)->getName() <<
					" has inventory UUID " << animationMap[items->get(index)->getName()] << llendl;
	}

	// [ State ]Anim1|Anim2|Anim3
	for(S32 index=found+1;index<lines.size();index++)
	{
		std::string line=lines[index];

		// cut off the trailing } of a notecard asset's text portion in the last line
		if(index==lines.size()-1)
			line=line.substr(0,line.size()-1);

		llwarns << line << llendl;

		LLStringUtil::trim(line);
		if(line.find("[")!=0)
		{
			llwarns << "line " << index << " has no valid [ state prefix" << llendl;
			continue;
		}

		S32 endTag=line.find("]");
		if(endTag==std::string::npos)
		{
			llwarns << "line " << index << " has no valid ] delimiter" << llendl;
			continue;
		}

		std::string stateName=line.substr(1,endTag-2);
		LLStringUtil::trim(stateName);

		AOSet::AOState* newState=mImportSet->getStateByName(stateName);
		if(!newState)
		{
			llwarns << "state name " << stateName << " not found." << llendl;
			continue;
		}

		std::string animationLine=line.substr(endTag+1);
		std::vector<std::string> animationList;
		LLStringUtil::getTokens(animationLine,animationList,"|");

		for(S32 animIndex=0;animIndex<animationList.size();animIndex++)
		{
			AOSet::AOAnimation animation;
			animation.mName=animationList[animIndex];
			animation.mInventoryUUID=animationMap[animation.mName];
			if(animation.mInventoryUUID.isNull())
			{
				llwarns << "couldn't find animation " << animation.mName << " in animation map." << llendl;
				continue;
			}
			animation.mSortOrder=animIndex;
			newState->mAnimations.push_back(animation);
			isValid=TRUE;
		}
	}

	if(!isValid)
	{
		llwarns << "Notecard didn't contain any usable data. Aborting import." << llendl;
		// NOTE: cleanup is always the same, needs streamlining
		delete mImportSet;
		mImportSet=0;
		mUpdatedSignal();
		return;
	}

	mTimerCollection.enableImportTimer(TRUE);
	mImportRetryCount=0;
	processImport();
}

void AOEngine::processImport()
{
	if(mImportCategory.isNull())
	{
		mImportCategory=addSet(mImportSet->getName(),FALSE);
		if(mImportCategory.isNull())
		{
			mImportRetryCount++;
			if(mImportRetryCount==5)
			{
				// NOTE: cleanup is the same as at the end of this function. Needs streamlining.
				mTimerCollection.enableImportTimer(FALSE);
				delete mImportSet;
				mImportSet=0;
				mImportCategory.setNull();
				mUpdatedSignal();
				llwarns << "could not create import category for set " << mImportSet->getName() << " ... giving up" << llendl;
			}
			else
				llwarns << "could not create import category for set " << mImportSet->getName() << " ... retrying ..." << llendl;
			return;
		}
		mImportSet->setInventoryUUID(mImportCategory);
	}

	BOOL allComplete=TRUE;
	for(S32 index=0;index<AOSet::AOSTATES_MAX;index++)
	{
		AOSet::AOState* state=mImportSet->getState(index);
		if(state->mAnimations.size())
		{
			allComplete=FALSE;
			llwarns << "state " << state->mName << " still has animations to link." << llendl;

			for(S32 animationIndex=state->mAnimations.size()-1;animationIndex>=0;animationIndex--)
			{
				llwarns << "linking animation " << state->mAnimations[animationIndex].mName << llendl;
				if(createAnimationLink(mImportSet,state,gInventory.getItem(state->mAnimations[animationIndex].mInventoryUUID)))
				{
					llwarns << "link success, size "<< state->mAnimations.size() << ", removing animation " <<
				                          (*(state->mAnimations.begin()+animationIndex)).mName << " from import state" << llendl;
					state->mAnimations.erase(state->mAnimations.begin()+animationIndex);
					llwarns << "deleted, size now: " << state->mAnimations.size() << llendl;
				}
				else
					llwarns << "link failed!" << llendl;
			}
		}
	}

	if(allComplete)
	{
		mTimerCollection.enableImportTimer(FALSE);
		delete mImportSet;
		mImportSet=0;
		mImportCategory.setNull();
		reload();
	}
}

// ----------------------------------------------------

AOSitCancelTimer::AOSitCancelTimer()
:	LLEventTimer(0.1),
	mTickCount(0)
{
	mEventTimer.stop();
}

AOSitCancelTimer::~AOSitCancelTimer()
{
}

void AOSitCancelTimer::oneShot()
{
	mTickCount=0;
	mEventTimer.start();
}

void AOSitCancelTimer::stop()
{
	mEventTimer.stop();
}

BOOL AOSitCancelTimer::tick()
{
	mTickCount++;
	AOEngine::instance().checkSitCancel();
	if(mTickCount==10)
		mEventTimer.stop();
	return FALSE;
}

// ----------------------------------------------------

AOTimerCollection::AOTimerCollection()
:	 LLEventTimer(INVENTORY_POLLING_INTERVAL),
	mInventoryTimer(TRUE),
	mSettingsTimer(FALSE),
	mReloadTimer(FALSE),
	mImportTimer(FALSE)
{
	updateTimers();
}

AOTimerCollection::~AOTimerCollection()
{
}

BOOL AOTimerCollection::tick()
{
	if(mInventoryTimer)
	{
		llwarns << "Inventory timer tick()" << llendl;
		AOEngine::instance().tick();
	}
	if(mSettingsTimer)
	{
		llwarns << "Settings timer tick()" << llendl;
		AOEngine::instance().saveSettings();
	}
	if(mReloadTimer)
	{
		llwarns << "Reload timer tick()" << llendl;
		AOEngine::instance().reload();
	}
	if(mImportTimer)
	{
		llwarns << "Import timer tick()" << llendl;
		AOEngine::instance().processImport();
	}

// always return FALSE or the LLEventTimer will be deleted -> crash
	return FALSE;
}

void AOTimerCollection::enableInventoryTimer(BOOL yes)
{
	mInventoryTimer=yes;
	updateTimers();
}

void AOTimerCollection::enableSettingsTimer(BOOL yes)
{
	mSettingsTimer=yes;
	updateTimers();
}

void AOTimerCollection::enableReloadTimer(BOOL yes)
{
	mReloadTimer=yes;
	updateTimers();
}

void AOTimerCollection::enableImportTimer(BOOL yes)
{
	mImportTimer=yes;
	updateTimers();
}

void AOTimerCollection::updateTimers()
{
	if(!mInventoryTimer && !mSettingsTimer && !mReloadTimer && !mImportTimer)
	{
		llwarns << "no timer needed, stopping internal timer." << llendl;
		mEventTimer.stop();
	}
	else
	{
		llwarns << "timer needed, starting internal timer." << llendl;
		mEventTimer.start();
	}
}
