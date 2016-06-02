////////////////////////////////////////////////////////////////////////////
//	Module 		: nvd_script.cpp
//	Created 	: 03.01.2016
//  Modified 	: 03.01.2016
//	Author		: Alexander Petrov
//	Description : Night vision device extracted from CTorch 
////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "pch_script.h"
#include "nvd.h"
#include "script_game_object.h"
#include "inventory_item_object.h"
// #include "../LightAnimLibrary.h"

using namespace luabind;

#pragma optimize("s",on)
void CNightVisionDevice::script_register(lua_State *L)
{

	module(L)
		[
			class_<CNightVisionDevice>("CNightVisionDevice")
			// работа с ПНВ
			.def_readwrite("enabled"	,		&CNightVisionDevice::m_bEnabled) // для контроля включаемости
			.def_readonly("powered"		,		&CNightVisionDevice::m_bPowered)
			.def("turn_on"				,		&CNightVisionDevice::TurnOn)
			.def("turn_off"				,		&CNightVisionDevice::TurnOff)
			.def("switch"				,		(void (CNightVisionDevice::*)()) (&CNightVisionDevice::SwitchNightVision))

		];

}
