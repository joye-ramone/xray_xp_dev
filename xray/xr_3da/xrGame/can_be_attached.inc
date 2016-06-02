bool  ATTACHABLE_ITEM::can_be_attached		() const
{
//	if( !inherited::can_be_attached() ) return false;

	const CActor *pA = smart_cast<const CActor *>(H_Parent());
	if (pA) 
	{
		u32 slot = GetSlot();
		PIItem item = pA->inventory().m_slots[slot].m_pIItem;
		if( (const ATTACHABLE_ITEM*)smart_cast<ATTACHABLE_ITEM*>(item) == this )
			return true;
		else
			return false;
	}
	return true;
}