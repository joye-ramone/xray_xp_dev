#include "StdAfx.h"
#include "..\xrRender\light.h"
#include "..\cl_intersect.h"

const	u32	delay_small_min			= 1;
const	u32	delay_small_max			= 3;
const	u32	delay_large_min			= 10;
const	u32	delay_large_max			= 20;
const	u32	cullfragments			= 4;

#pragma optimize("gyts", off)


void	light::vis_prepare			()
{
	if (int(indirect_photons)!=ps_r2_GI_photons)	gi_generate	();
	if (vis.pending) vis_update(); // reset previuos 


	//	. test is sheduled for future	= keep old result
	//	. test time comes :)
	//		. camera inside light volume	= visible,	shedule for 'small' interval
	//		. perform testing				= ???,		pending

	u32	frame	= Device.dwFrame;
	if (frame	<	vis.frame2test)	return;

	float	safe_area = 0; 	
	{
		float	a1	= deg2rad(Device.fFOV / 2.f);
		float	a0	= a1 * Device.fASPECT;		
		float	x0	= VIEWPORT_NEAR/_cos	(a0);
		float	x1	= VIEWPORT_NEAR/_cos	(a1);
		float	c	= _sqrt					(x0 * x0 + x1 * x1);
		safe_area	= _max(_max(VIEWPORT_NEAR,_max(x0,x1)), c);
	}

	//Msg	("sc[%f,%f,%f]/c[%f,%f,%f] - sr[%f]/r[%f]",VPUSH(spatial.center),VPUSH(position),spatial.radius,range);
	//Msg	("dist:%f, sa:%f",Device.vCameraPosition.distance_to(spatial.center),safe_area);
	// small error - если источник достаточно близко к камере
	static u32 debug_id = 0;
	if (debug_id == light_id)
		__asm nop;


	Fvector ltp = spatial.sphere.P;  
	if (virtual_size > 0.05f)
		ltp = position;

	float   lt_dist		= Device.vCameraPosition.distance_to(ltp);
	// virtual_size > 0.05f || 
	bool	skiptest	= lt_dist <= spatial.sphere.R * 1.0f + safe_area || lt_dist > 700.f;
	if (ps_r2_ls_flags.test(R2FLAG_EXP_DONT_TEST_UNSHADOWED) && !flags.bShadow)	skiptest = true;

		
	if (!skiptest && lt_dist > 15.f)
	{
		// отсечение огней сзади и побокам
		Fvector cdir(Device.vCameraDirection);
		Fvector ldir;
		ldir.sub(ltp, Device.vCameraPosition);  // направление на источник света
		ldir.normalize();
		Fvector cp;
		cp = cp.crossproduct(cdir, ldir);
		float  dp = cdir.dotproduct(ldir);
		float ang = atan2(cp.magnitude(), dp);
		if (abs(ang) > PI / 2.f)
		{
			skiptest = true;			
			if (vis.visible)
				MsgV("OCCQ5", "# #RENDER: last visible light#%d (range = %.1f, vsize = %.1f, sphere.P = { %7.3f, %7.3f, %7.3f }) now invisible, distance = %.2f, angle = %.3f ",
						light_id, range, virtual_size, ltp.x, ltp.y, ltp.z, lt_dist, ang);
			vis.visible = false;
		}


	}
	else
		if (skiptest)
			vis.visible  =	(lt_dist <= 700.f);

	if (skiptest)	{			
		vis.pending		=	false;
		vis.query_id	=   (u32)-1;
		vis.frame2test	=	frame	+ ::Random.randI(delay_small_min, delay_small_max);
		return;
	}
		

	// testing
	vis.pending										= true;
	/*
	auto it = lights_map.find(location);
	if (it != lights_map.end())
	{
		vis.query_id = (u32)-1;  // cached by other light
		query_owner = it->second;
		return;
	}
	// */
	
	xform_calc										();
	RCache.set_xform_world							(m_xform);

	vis.query_order	= RImplementation.occq_begin	(vis.query_id);
	RImplementation.Target->draw_volume				(this);
	RImplementation.occq_end						(vis.query_id);
}


void	light::vis_update			()
{
	//	. not pending	->>> return (early out)
	//	. test-result:	visible:
	//		. shedule for 'large' interval
	//	. test-result:	invisible:
	//		. shedule for 'next-frame' interval

	if (!vis.pending)	return;

	u32 quid			= vis.query_id;
	u32	frame			= Device.dwFrame;
	u32 fragments		= 0; 
	float elps = 0.f;
	bool last_visible = vis.visible;	
	{
		fragments = RImplementation.occq_get(vis.query_id);
		elps	  = RImplementation.occq_elapsed();
		vis.visible			= (fragments > cullfragments);
		vis.pending			= (vis.query_id < 0x10000);	
		if (vis.pending)
			MsgCB("!#WARN: light #%d still pending after occq_get", light_id);

	}
	//Log					("",fragments);
	auto &p = position;

	if (vis.visible)	{
		vis.frame2test	=	frame	+ ::Random.randI(delay_large_min, delay_large_max);

	} else {
		vis.frame2test	=	frame	+ 1;  // все невидимые 
		float dist = Device.vCameraPosition.distance_to(p);
		if (last_visible && dist < 5.f)
			MsgV("OCCQ5", "# #RENDER: last visible light#%d occq_id = 0x%08x (range = %.1f, vsize = %.1f, pos = { %7.3f, %7.3f, %7.3f }) now invisible, distance = %.2f, fragments = %d",
					light_id, quid, range, virtual_size, p.x, p.y, p.z, dist, fragments);
	}

	if (elps > 0.1f)
	__try{	
		MsgCB("!#PERF_WARN: light#%d (range = %.1f, vsize = %.1f, pos = { %7.3f, %7.3f, %7.3f }) occlusion get elapsed time = %.3f sec",
				 	light_id, range, virtual_size, p.x, p.y, p.z, elps);			
		vis.frame2test += 100;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)  {
		Log("!#EXCEPTION: light::vis_update"); 
	}

}
