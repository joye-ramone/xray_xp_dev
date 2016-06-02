////////////////////////////////////////////////////////////////////////////
//	Module 		: nvd.h
//	Created 	: 03.01.2016
//  Modified 	: 03.01.2016
//	Author		: Alexander Petrov
//	Description : Night vision device extracted from CTorch 
////////////////////////////////////////////////////////////////////////////
#pragma once


#include "inventory_item_object.h"
#include "script_export_space.h"
#include "hudsound.h"

class CNightVisionDevice
{

protected:
	bool					m_bEnabled;
public:
	bool					m_bPowered;	
protected:
	HUD_SOUND				m_OnSnd;
	HUD_SOUND				m_OffSnd;
	HUD_SOUND				m_IdleSnd;
	HUD_SOUND				m_BrokenSnd;

	shared_str				m_DeviceSect;
	shared_str				m_EffectorSect;


public:

			CNightVisionDevice				  (void);
  virtual   ~CNightVisionDevice				  (void);
  virtual   void	SwitchNightVision		  ();
  virtual	void	SwitchNightVision		  (bool light_on);
			void	TurnOn					  () { SwitchNightVision(true); };
			void	TurnOff					  () { SwitchNightVision(false); };
			void	UpdateSwitchNightVision   ();
			float	NightVisionBattery		  ();
  virtual	void	InitDevice				  (LPCSTR section);	  // for use with outfit section
  virtual	void	LoadConfig				  (LPCSTR section);	



  DECLARE_SCRIPT_REGISTER_FUNCTION
};


add_to_type_list(CNightVisionDevice)
#undef script_type_list
#define script_type_list save_type_list(CNightVisionDevice)

// класс портативного (отдельного от костюма) ПНВ
class CPortableNVD:
	public CNightVisionDevice,
	public CInventoryItemObject
{
private:
    typedef	CInventoryItemObject	inherited;

enum EStats{
	    eEnabled			= (1<<0),
		eActive				= (1<<1),
		eAttached			= (1<<2)
	};

public:
					CPortableNVD		(void);						
    virtual			~CPortableNVD		(void);
	virtual void	Load				(LPCSTR section)      { LoadConfig(section); inherited::Load(section); };
	virtual BOOL	net_Spawn			(CSE_Abstract* DC);
	virtual void	net_Destroy			();
	virtual void	net_Export			(NET_Packet& P);				// export to server
	virtual void	net_Import			(NET_Packet& P);				// import from server

	virtual void	OnH_A_Chield		();
	virtual void	OnH_B_Independent	(bool just_before_destroy);
	virtual void    OnMoveToRuck		()  { inherited::OnMoveToRuck(); TurnOff(); }
	virtual void	afterDetach			()  { inherited::afterDetach(); TurnOff (); }
	virtual void	UpdateCL			();

	virtual bool	can_be_attached		() const;
};
