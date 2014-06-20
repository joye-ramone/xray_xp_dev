#include "stdafx.h"
#include "UIInventoryWnd.h"
#include "UISleepWnd.h"
#include "../level.h"
#include "../actor.h"
#include "../ActorCondition.h"
#include "../hudmanager.h"
#include "../inventory.h"
#include "UIInventoryUtilities.h"

#include "UICellItem.h"
#include "UICellItemFactory.h"
#include "UIDragDropListEx.h"
#include "UI3tButton.h"

#include "../build_config_defines.h"

CUICellItem* CUIInventoryWnd::CurrentItem()
{
	return m_pCurrentCellItem;
}

PIItem CUIInventoryWnd::CurrentIItem()
{
	return	(m_pCurrentCellItem)?(PIItem)m_pCurrentCellItem->m_pData : NULL;
}

void CUIInventoryWnd::SetCurrentItem(CUICellItem* itm)
{
	if(m_pCurrentCellItem == itm) return;
	m_pCurrentCellItem				= itm;
	UIItemInfo.InitItem			(CurrentIItem());
}

void CUIInventoryWnd::SendMessage(CUIWindow *pWnd, s16 msg, void *pData)
{
	if(pWnd == &UIPropertiesBox &&	msg==PROPERTY_CLICKED)
	{
		ProcessPropertiesBoxClicked	();
	}else 
	if (UIExitButton == pWnd && BUTTON_CLICKED == msg)
	{
		GetHolder()->StartStopMenu			(this,true);
	}

	CUIWindow::SendMessage(pWnd, msg, pData);
}


void CUIInventoryWnd::InitInventory_delayed()
{
	m_b_need_reinit = true;
}

void CUIInventoryWnd::InitInventory() 
{
	CInventoryOwner *pInvOwner	= smart_cast<CInventoryOwner*>(Level().CurrentEntity());
	if(!pInvOwner)				return;

	m_pInv						= &pInvOwner->inventory();

	UIPropertiesBox.Hide		();
	ClearAllLists				();
	m_pMouseCapturer			= NULL;
	SetCurrentItem				(NULL);

	//Slots

	PIItem  _itm							= m_pInv->m_slots[PISTOL_SLOT].m_pIItem;
	if(_itm)
	{
		CUICellItem* itm				= create_cell_item(_itm);
		m_pUIPistolList->SetItem		(itm);
	}
	_itm								= m_pInv->m_slots[RIFLE_SLOT].m_pIItem;
	if(_itm)
	{
		CUICellItem* itm				= create_cell_item(_itm);
		m_pUIAutomaticList->SetItem		(itm);
	}
	
#ifdef INV_NEW_SLOTS_SYSTEM
	if (GameID() == GAME_SINGLE){
			_itm								= m_pInv->m_slots[KNIFE_SLOT].m_pIItem;
			if(_itm)
			{
				CUICellItem* itm				= create_cell_item(_itm);
				m_pUIKnifeList->SetItem		(itm);
			}
			_itm								= m_pInv->m_slots[APPARATUS_SLOT].m_pIItem;
			if(_itm)
			{
				CUICellItem* itm				= create_cell_item(_itm);
				m_pUIBinocularList->SetItem		(itm);
			}
			_itm								= m_pInv->m_slots[DETECTOR_SLOT].m_pIItem;
			if(_itm)
			{
				CUICellItem* itm				= create_cell_item(_itm);
				m_pUIDetectorList->SetItem		(itm);
			}
			_itm								= m_pInv->m_slots[TORCH_SLOT].m_pIItem;
			if(_itm)
			{
				CUICellItem* itm				= create_cell_item(_itm);
				m_pUITorchList->SetItem		(itm);
			}
			_itm								= m_pInv->m_slots[PDA_SLOT].m_pIItem;
			if(_itm)
			{
				CUICellItem* itm				= create_cell_item(_itm);
				m_pUIPDAList->SetItem		(itm);
			}
			_itm								= m_pInv->m_slots[HELMET_SLOT].m_pIItem;
			if(_itm)
			{
				CUICellItem* itm				= create_cell_item(_itm);
				m_pUIHelmetList->SetItem		(itm);
			}
			_itm								= m_pInv->m_slots[SLOT_QUICK_ACCESS_0].m_pIItem;
			if(_itm)
			{
				CUICellItem* itm				= create_cell_item(_itm);
				m_pUISlotQuickAccessList_0->SetItem		(itm);
			}
			_itm								= m_pInv->m_slots[SLOT_QUICK_ACCESS_1].m_pIItem;
			if(_itm)
			{
				CUICellItem* itm				= create_cell_item(_itm);
				m_pUISlotQuickAccessList_1->SetItem		(itm);
			}
			_itm								= m_pInv->m_slots[SLOT_QUICK_ACCESS_2].m_pIItem;
			if(_itm)
			{
				CUICellItem* itm				= create_cell_item(_itm);
				m_pUISlotQuickAccessList_2->SetItem		(itm);
			}
			_itm								= m_pInv->m_slots[SLOT_QUICK_ACCESS_3].m_pIItem;
			if(_itm)
			{
				CUICellItem* itm				= create_cell_item(_itm);
				m_pUISlotQuickAccessList_3->SetItem		(itm);
			}
	}
#endif

	PIItem _outfit						= m_pInv->m_slots[OUTFIT_SLOT].m_pIItem;
	CUICellItem* outfit					= (_outfit)?create_cell_item(_outfit):NULL;
	m_pUIOutfitList->SetItem			(outfit);

	TIItemContainer::iterator it, it_e;
	for(it=m_pInv->m_belt.begin(),it_e=m_pInv->m_belt.end(); it!=it_e; ++it) 
	{
		CUICellItem* itm			= create_cell_item(*it);
		m_pUIBeltList->SetItem		(itm);
	}


	
	ruck_list		= m_pInv->m_ruck;
	std::sort		(ruck_list.begin(),ruck_list.end(),InventoryUtilities::GreaterRoomInRuck);

	int i=1;
	for(it=ruck_list.begin(),it_e=ruck_list.end(); it!=it_e; ++it,++i) 
	{
		CUICellItem* itm			= create_cell_item(*it);
		m_pUIBagList->SetItem		(itm);
	}
	//fake
	_itm								= m_pInv->m_slots[GRENADE_SLOT].m_pIItem;
	if(_itm)
	{
		CUICellItem* itm				= create_cell_item(_itm);
		m_pUIBagList->SetItem			(itm);
	}

	InventoryUtilities::UpdateWeight					(UIBagWnd, true);

	m_b_need_reinit					= false;
}

void CUIInventoryWnd::DropCurrentItem(bool b_all)
{

	CActor *pActor			= smart_cast<CActor*>(Level().CurrentEntity());
	if(!pActor)				return;

	if(!b_all && CurrentIItem() && !CurrentIItem()->IsQuestItem())
	{
		SendEvent_Item_Drop		(CurrentIItem());
		SetCurrentItem			(NULL);
		InventoryUtilities::UpdateWeight			(UIBagWnd, true);
		return;
	}

	if(b_all && CurrentIItem() && !CurrentIItem()->IsQuestItem())
	{
		u32 cnt = CurrentItem()->ChildsCount();

		for(u32 i=0; i<cnt; ++i){
			CUICellItem*	itm				= CurrentItem()->PopChild();
			PIItem			iitm			= (PIItem)itm->m_pData;
			SendEvent_Item_Drop				(iitm);
		}

		SendEvent_Item_Drop					(CurrentIItem());
		SetCurrentItem						(NULL);
		InventoryUtilities::UpdateWeight	(UIBagWnd, true);
		return;
	}
}

//------------------------------------------

bool CUIInventoryWnd::ToSlot(CUICellItem* itm, bool force_place)
{
	CUIDragDropListEx*	old_owner			= itm->OwnerList();
	PIItem	iitem							= (PIItem)itm->m_pData;
	u32 _slot								= iitem->GetSlot();

	if(GetInventory()->CanPutInSlot(iitem)){
		CUIDragDropListEx* new_owner		= GetSlotList(_slot);
		
		if(_slot==GRENADE_SLOT && !new_owner )return true; //fake, sorry (((
		#ifdef INV_NEW_SLOTS_SYSTEM
			if ((_slot == SLOT_QUICK_ACCESS_0)||(_slot == SLOT_QUICK_ACCESS_1)||(_slot == SLOT_QUICK_ACCESS_2)||(_slot == SLOT_QUICK_ACCESS_3)){
				for(u32 i=SLOT_QUICK_ACCESS_0; i <= SLOT_QUICK_ACCESS_3; ++i ) 
				{	
					if(i != _slot){
						 PIItem l_pIItem = GetInventory()->m_slots[i].m_pIItem;
						 if(l_pIItem){
							if ((!xr_strcmp(l_pIItem->object().cNameSect(), iitem->object().cNameSect()))&&(l_pIItem != iitem)){
								PIItem	_iitem						= GetInventory()->m_slots[i].m_pIItem;
								CUIDragDropListEx* slot_list		= GetSlotList(i);
								VERIFY								(slot_list->ItemsCount()==1);
								CUICellItem* slot_cell				= slot_list->GetItemIdx(0);
								VERIFY								(slot_cell && ((PIItem)slot_cell->m_pData)==_iitem);
								bool result							= ToBag(slot_cell, false);
								VERIFY								(result);
							}
						 }
					}
				}
			}
		#endif
		
		bool result							= GetInventory()->Slot(iitem);
		VERIFY								(result);

		CUICellItem* i						= old_owner->RemoveItem(itm, (old_owner==new_owner) );
		
		new_owner->SetItem					(i);
		SendEvent_Item2Slot					(iitem);
		#ifdef INV_NEW_SLOTS_SYSTEM
		if ((_slot < OUTFIT_SLOT)||(_slot == ARTEFACT_SLOT))
		#endif
			SendEvent_ActivateSlot				(iitem);

		/************************************************** added by Ray Twitty (aka Shadows) START **************************************************/
		// обновл€ем статик веса в инвентаре
		InventoryUtilities::UpdateWeight	(UIBagWnd, true);
		/*************************************************** added by Ray Twitty (aka Shadows) END ***************************************************/

		return								true;
	}else
	{ // in case slot is busy
		if(!force_place ||  _slot==NO_ACTIVE_SLOT || GetInventory()->m_slots[_slot].m_bPersistent) return false;

		PIItem	_iitem						= GetInventory()->m_slots[_slot].m_pIItem;
		CUIDragDropListEx* slot_list		= GetSlotList(_slot);
		VERIFY								(slot_list->ItemsCount()==1);

		CUICellItem* slot_cell				= slot_list->GetItemIdx(0);
		VERIFY								(slot_cell && ((PIItem)slot_cell->m_pData)==_iitem);

		bool result							= ToBag(slot_cell, false);
		VERIFY								(result);

		return ToSlot						(itm, false);
	}
}

bool CUIInventoryWnd::ToBag(CUICellItem* itm, bool b_use_cursor_pos)
{
	PIItem	iitem						= (PIItem)itm->m_pData;

	if(GetInventory()->CanPutInRuck(iitem))
	{
		CUIDragDropListEx*	old_owner		= itm->OwnerList();
		CUIDragDropListEx*	new_owner		= NULL;
		if(b_use_cursor_pos){
				new_owner					= CUIDragDropListEx::m_drag_item->BackList();
				VERIFY						(new_owner==m_pUIBagList);
		}else
				new_owner					= m_pUIBagList;


		bool result							= GetInventory()->Ruck(iitem);
		VERIFY								(result);
		CUICellItem* i						= old_owner->RemoveItem(itm, (old_owner==new_owner) );

		/************************************************** added by Ray Twitty (aka Shadows) START **************************************************/
		// обновл€ем статик веса в инвентаре
		InventoryUtilities::UpdateWeight	(UIBagWnd, true);
		/*************************************************** added by Ray Twitty (aka Shadows) END ***************************************************/

#ifdef INV_RUCK_UNLIMITED_FIX		
		if (result = new_owner->CanSetItem(i) )
		{
#endif
			if(b_use_cursor_pos)
				new_owner->SetItem				(i,old_owner->GetDragItemPosition() );
			else
				new_owner->SetItem				(i);
			SendEvent_Item2Ruck					(iitem);
#ifdef INV_RUCK_UNLIMITED_FIX
		}
		else
		{
			//Msg("Place item fail!");

			NET_Packet					P;
			iitem->object().u_EventGen	(P, GE_OWNERSHIP_REJECT, iitem->object().H_Parent()->ID());
			P.w_u16						(u16(iitem->object().ID()));
			iitem->object().u_EventSend(P);
		}

		return result;
#else
		return true;
#endif
	}
	return false;
}

bool CUIInventoryWnd::ToBelt(CUICellItem* itm, bool b_use_cursor_pos)
{
	PIItem	iitem						= (PIItem)itm->m_pData;

	if(GetInventory()->CanPutInBelt(iitem))
	{
		CUIDragDropListEx*	old_owner		= itm->OwnerList();
		CUIDragDropListEx*	new_owner		= NULL;
		if(b_use_cursor_pos){
				new_owner					= CUIDragDropListEx::m_drag_item->BackList();
				VERIFY						(new_owner==m_pUIBeltList);
		}else
				new_owner					= m_pUIBeltList;

		bool result							= GetInventory()->Belt(iitem);
		VERIFY								(result);
		CUICellItem* i						= old_owner->RemoveItem(itm, (old_owner==new_owner) );
		
	//.	UIBeltList.RearrangeItems();
		if(b_use_cursor_pos)
			new_owner->SetItem				(i,old_owner->GetDragItemPosition());
		else
			new_owner->SetItem				(i);

		SendEvent_Item2Belt					(iitem);

		/************************************************** added by Ray Twitty (aka Shadows) START **************************************************/
		// обновл€ем статик веса в инвентаре
		InventoryUtilities::UpdateWeight	(UIBagWnd, true);
		/*************************************************** added by Ray Twitty (aka Shadows) END ***************************************************/

		return true;
	}
	return									false;
}

void CUIInventoryWnd::AddItemToBag(PIItem pItem)
{
	CUICellItem* itm						= create_cell_item(pItem);
	m_pUIBagList->SetItem					(itm);
}

bool CUIInventoryWnd::OnItemStartDrag(CUICellItem* itm)
{
	return false; //default behaviour
}

bool CUIInventoryWnd::OnItemSelected(CUICellItem* itm)
{
	SetCurrentItem		(itm);
	return				false;
}

bool CUIInventoryWnd::OnItemDrop(CUICellItem* itm)
{
	CUIDragDropListEx*	old_owner		= itm->OwnerList();
	CUIDragDropListEx*	new_owner		= CUIDragDropListEx::m_drag_item->BackList();
	if(old_owner==new_owner || !old_owner || !new_owner)
					return false;

	EListType t_new		= GetType(new_owner);
	EListType t_old		= GetType(old_owner);

#if defined(INV_NEW_SLOTS_SYSTEM) || defined(INV_DOUBLE_WPN_SLOTS)
	// ƒл€ слотов проверим ниже. Real Wolf.
	if(t_new == t_old && t_new != iwSlot) return true;
#else
	if(t_new == t_old) return true;
#endif
	switch(t_new){
		case iwSlot:
		{
#if defined(INV_NEW_SLOTS_SYSTEM) || defined(INV_DOUBLE_WPN_SLOTS)
			uint32 slot = CurrentIItem()->GetSlot();
			if(GetSlotList(slot)==new_owner && t_new != t_old)
#else
			if(GetSlotList(CurrentIItem()->GetSlot())==new_owner)
#endif
				ToSlot	(itm, true);
#if defined(INV_NEW_SLOTS_SYSTEM) || defined(INV_DOUBLE_WPN_SLOTS)
	#if !defined(INV_NEW_SLOTS_SYSTEM) && defined(INV_DOUBLE_WPN_SLOTS)
			else if (new_owner == m_pUIPistolList && slot == RIFLE_SLOT)
			{
				CurrentIItem()->SetSlot(PISTOL_SLOT);
				ToSlot	(itm, true);
			}
			else if (new_owner == m_pUIAutomaticList && slot == PISTOL_SLOT)
			{
				CurrentIItem()->SetSlot(RIFLE_SLOT);
				ToSlot	(itm, true);
			}
	#else
			else{
				u32 slot_drop;
					if (new_owner == m_pUIKnifeList){	
								slot_drop = KNIFE_SLOT;
					}else if (new_owner == m_pUIPistolList){	
								slot_drop = PISTOL_SLOT;
												
					}else if (new_owner == m_pUIAutomaticList){	
								slot_drop = RIFLE_SLOT;
											
					}else if (new_owner == m_pUIBinocularList){	
								slot_drop = APPARATUS_SLOT;
											
					}else if (new_owner == m_pUIDetectorList){	
								slot_drop = DETECTOR_SLOT;
								
					}else if (new_owner == m_pUIOutfitList){	
								slot_drop = OUTFIT_SLOT;
												
					}else if (new_owner == m_pUITorchList){		
								slot_drop = TORCH_SLOT;
										
					}else if (new_owner == m_pUIPDAList){	
								slot_drop = PDA_SLOT;
												
					}else if (new_owner == m_pUIHelmetList){	
								slot_drop = HELMET_SLOT;
									
					}else if (new_owner == m_pUISlotQuickAccessList_0){	
								slot_drop = SLOT_QUICK_ACCESS_0;
								
					}else if (new_owner == m_pUISlotQuickAccessList_1){	
								slot_drop = SLOT_QUICK_ACCESS_1;
								
					}else if (new_owner == m_pUISlotQuickAccessList_2){	
								slot_drop = SLOT_QUICK_ACCESS_2;
									
					}else if (new_owner == m_pUISlotQuickAccessList_3){	
								slot_drop = SLOT_QUICK_ACCESS_3;		
					}

				u32 list_slot_local[SLOTS_TOTAL];
				CurrentIItem()->GetSlotList(list_slot_local);
				for(u32 i=0; i<SLOTS_TOTAL; ++i ) 
				{
					if (list_slot_local[i]==slot_drop) {
						CurrentIItem()->SetSlot(slot_drop);
						ToSlot	(itm, true);
						break;
					}
				}
			}
	#endif
#endif
		}break;
		case iwBag:{
			ToBag	(itm, true);
		}break;
		case iwBelt:{
			ToBelt	(itm, true);
		}break;
	};

	DropItem				(CurrentIItem(), new_owner);

	return true;
}

bool CUIInventoryWnd::OnItemDbClick(CUICellItem* itm)
{
	PIItem __item = (PIItem)itm->m_pData;
	u32 __slot = __item->GetSlot();
	#ifdef INV_NEW_SLOTS_SYSTEM
	if 	((__slot != SLOT_QUICK_ACCESS_0)&&(__slot != SLOT_QUICK_ACCESS_1)&&(__slot != SLOT_QUICK_ACCESS_2)&&(__slot != SLOT_QUICK_ACCESS_3))	
	#endif
		if(TryUseItem((PIItem)itm->m_pData))		
			return true;

	CUIDragDropListEx*	old_owner		= itm->OwnerList();
	EListType t_old						= GetType(old_owner);

	switch(t_old){
		case iwSlot:{
			ToBag	(itm, false);
		}break;

		case iwBag:
		{
#if defined(INV_NEW_SLOTS_SYSTEM) || defined(INV_DOUBLE_WPN_SLOTS)
		#if !defined(INV_NEW_SLOTS_SYSTEM) && defined(INV_DOUBLE_WPN_SLOTS)
			// ѕри двойном клике выбираем свободный слот. Real Wolf.
			PIItem item = (PIItem)itm->m_pData;
			u32 slot = item->GetSlot();
			if (slot == PISTOL_SLOT || slot == RIFLE_SLOT)
			{
				u32 double_slot = slot == PISTOL_SLOT? RIFLE_SLOT: PISTOL_SLOT;
				if (GetInventory()->m_slots[slot].m_pIItem)
					if (!GetInventory()->m_slots[double_slot].m_pIItem)
						item->SetSlot(double_slot);
			}
		#else
			PIItem item = (PIItem)itm->m_pData;
			u32 slot = item->GetSlot();
			u32 list_slot_local[SLOTS_TOTAL];
			item->GetSlotList(list_slot_local);	
			for(u32 i=0; i<SLOTS_TOTAL; ++i ) 
			{
				if (list_slot_local[i]!=NO_ACTIVE_SLOT){
					if (GetInventory()->m_slots[slot].m_pIItem)
						if (!GetInventory()->m_slots[list_slot_local[i]].m_pIItem){
							item->SetSlot(list_slot_local[i]);
							break;
						}
				}
			};			
		#endif
#endif
			if(!ToSlot(itm, false))
			{
				if( !ToBelt(itm, false) )
					ToSlot	(itm, true);
			}
		}break;

		case iwBelt:{
			ToBag	(itm, false);
		}break;
	};

	return true;
}


bool CUIInventoryWnd::OnItemRButtonClick(CUICellItem* itm)
{
	SetCurrentItem				(itm);
	ActivatePropertiesBox		();
	return						false;
}

CUIDragDropListEx* CUIInventoryWnd::GetSlotList(u32 slot_idx)
{
	if(slot_idx == NO_ACTIVE_SLOT || GetInventory()->m_slots[slot_idx].m_bPersistent)	return NULL;
	switch (slot_idx)
	{
		case PISTOL_SLOT:
			return m_pUIPistolList;
			break;

		case RIFLE_SLOT:
			return m_pUIAutomaticList;
			break;

		case OUTFIT_SLOT:
			return m_pUIOutfitList;
			break;
			
#ifdef INV_NEW_SLOTS_SYSTEM
		case KNIFE_SLOT:
			return m_pUIKnifeList;
			break;
		case APPARATUS_SLOT:
			return m_pUIBinocularList;
			break;
		case DETECTOR_SLOT:
			return m_pUIDetectorList;
			break;
		case TORCH_SLOT:
			return m_pUITorchList;
			break;
		case PDA_SLOT:
			return m_pUIPDAList;
			break;
		case HELMET_SLOT:
			return m_pUIHelmetList;
			break;
		case SLOT_QUICK_ACCESS_0:
			return m_pUISlotQuickAccessList_0;
			break;
		case SLOT_QUICK_ACCESS_1:
			return m_pUISlotQuickAccessList_1;
			break;
		case SLOT_QUICK_ACCESS_2:
			return m_pUISlotQuickAccessList_2;
			break;
		case SLOT_QUICK_ACCESS_3:
			return m_pUISlotQuickAccessList_3;
			break;
#endif
	};
	return NULL;
}



void CUIInventoryWnd::ClearAllLists()
{
	m_pUIBagList->ClearAll					(true);
	m_pUIBeltList->ClearAll					(true);
	m_pUIOutfitList->ClearAll				(true);
	m_pUIPistolList->ClearAll				(true);
	m_pUIAutomaticList->ClearAll			(true);

#ifdef INV_NEW_SLOTS_SYSTEM
//if (GameID() == GAME_SINGLE){	// нельз€ так делать - g_pGameLevel уже удален
if (IsGameTypeSingle()) {
	m_pUIKnifeList->ClearAll				(true);
	m_pUIBinocularList->ClearAll			(true);
	m_pUIDetectorList->ClearAll				(true);
	m_pUITorchList->ClearAll				(true);
	m_pUIPDAList->ClearAll					(true);
	m_pUIHelmetList->ClearAll				(true);
	m_pUISlotQuickAccessList_0->ClearAll	(true);
	m_pUISlotQuickAccessList_1->ClearAll	(true);
	m_pUISlotQuickAccessList_2->ClearAll	(true);
	m_pUISlotQuickAccessList_3->ClearAll	(true);
}
#endif
}