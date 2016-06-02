////////////////////////////////////////////////////////////////////////////
//	Module 		: script_fvector_script.cpp
//	Created 	: 28.06.2004
//  Modified 	: 28.06.2004
//	Author		: Dmitriy Iassenev
//	Description : Script float vector script export
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"
#include "script_coords.h"

using namespace luabind;

// template <typename T> _vector2<T>& rect_get_lt(_rect<T> *r) { return r->lt;  }

Fvector2 &Frect_get_lt(Frect *r)			{ return r->lt; }
void Frect_set_lt(Frect *r, Fvector2 v)     { r->lt.set(v); }
Fvector2 &Frect_get_rb(Frect *r)			{ return r->rb; }
void Frect_set_rb(Frect *r, Fvector2 v)     { r->rb.set(v); }

Ivector2 &Irect_get_lt(Irect *r)			{ return r->lt; }
void Irect_set_lt(Irect *r, Ivector2 v)     { r->lt.set(v); }
Ivector2 &Irect_get_rb(Irect *r)			{ return r->rb; }
void Irect_set_rb(Irect *r, Ivector2 v)     { r->rb.set(v); }



#pragma optimize("s",on)
void CScriptCoords::script_register(lua_State *L)
{
	module(L)
	[
		class_<Fvector>("vector")
			.def_readwrite("x",					&Fvector::x)
			.def_readwrite("y",					&Fvector::y)
			.def_readwrite("z",					&Fvector::z)
			.def(								constructor<>())
			.def("set",							(Fvector & (Fvector::*)(float,float,float))(&Fvector::set),																return_reference_to(_1))
			.def("set",							(Fvector & (Fvector::*)(const Fvector &))(&Fvector::set),																return_reference_to(_1))
			.def("add",							(Fvector & (Fvector::*)(float))(&Fvector::add),																			return_reference_to(_1))
			.def("add",							(Fvector & (Fvector::*)(const Fvector &))(&Fvector::add),																return_reference_to(_1))
			.def("add",							(Fvector & (Fvector::*)(const Fvector &, const Fvector &))(&Fvector::add),												return_reference_to(_1))
			.def("add",							(Fvector & (Fvector::*)(const Fvector &, float))(&Fvector::add),															return_reference_to(_1))
			.def("sub",							(Fvector & (Fvector::*)(float))(&Fvector::sub),																			return_reference_to(_1))
			.def("sub",							(Fvector & (Fvector::*)(const Fvector &))(&Fvector::sub),																return_reference_to(_1))
			.def("sub",							(Fvector & (Fvector::*)(const Fvector &, const Fvector &))(&Fvector::sub),												return_reference_to(_1))
			.def("sub",							(Fvector & (Fvector::*)(const Fvector &, float))(&Fvector::sub),															return_reference_to(_1))
			.def("mul",							(Fvector & (Fvector::*)(float))(&Fvector::mul),																			return_reference_to(_1))
			.def("mul",							(Fvector & (Fvector::*)(const Fvector &))(&Fvector::mul),																return_reference_to(_1))
			.def("mul",							(Fvector & (Fvector::*)(const Fvector &, const Fvector &))(&Fvector::mul),												return_reference_to(_1))
			.def("mul",							(Fvector & (Fvector::*)(const Fvector &, float))(&Fvector::mul),															return_reference_to(_1))
			.def("div",							(Fvector & (Fvector::*)(float))(&Fvector::div),																			return_reference_to(_1))
			.def("div",							(Fvector & (Fvector::*)(const Fvector &))(&Fvector::div),																return_reference_to(_1))
			.def("div",							(Fvector & (Fvector::*)(const Fvector &, const Fvector &))(&Fvector::div),												return_reference_to(_1))
			.def("div",							(Fvector & (Fvector::*)(const Fvector &, float))(&Fvector::div),															return_reference_to(_1))
			.def("invert",						(Fvector & (Fvector::*)())(&Fvector::invert),																			return_reference_to(_1))
			.def("invert",						(Fvector & (Fvector::*)(const Fvector &))(&Fvector::invert),																return_reference_to(_1))
			.def("min",							(Fvector & (Fvector::*)(const Fvector &))(&Fvector::min),																return_reference_to(_1))
			.def("min",							(Fvector & (Fvector::*)(const Fvector &, const Fvector &))(&Fvector::min),												return_reference_to(_1))
			.def("max",							(Fvector & (Fvector::*)(const Fvector &))(&Fvector::max),																return_reference_to(_1))
			.def("max",							(Fvector & (Fvector::*)(const Fvector &, const Fvector &))(&Fvector::max),												return_reference_to(_1))
			.def("abs",							&Fvector::abs,																											return_reference_to(_1))
			.def("similar",						&Fvector::similar)
			.def("set_length",					&Fvector::set_length,																									return_reference_to(_1))
			.def("align",						&Fvector::align,																										return_reference_to(_1))
//			.def("squeeze",						&Fvector::squeeze,																										return_reference_to(_1))
			.def("clamp",						(Fvector & (Fvector::*)(const Fvector &))(&Fvector::clamp),																return_reference_to(_1))
			.def("clamp",						(Fvector & (Fvector::*)(const Fvector &, const Fvector))(&Fvector::clamp),												return_reference_to(_1))
			.def("inertion",					&Fvector::inertion,																										return_reference_to(_1))
			.def("average",						(Fvector & (Fvector::*)(const Fvector &))(&Fvector::average),															return_reference_to(_1))
			.def("average",						(Fvector & (Fvector::*)(const Fvector &, const Fvector &))(&Fvector::average),											return_reference_to(_1))
			.def("lerp",						&Fvector::lerp,																											return_reference_to(_1))
			.def("mad",							(Fvector & (Fvector::*)(const Fvector &, float))(&Fvector::mad),															return_reference_to(_1))
			.def("mad",							(Fvector & (Fvector::*)(const Fvector &, const Fvector &, float))(&Fvector::mad),										return_reference_to(_1))
			.def("mad",							(Fvector & (Fvector::*)(const Fvector &, const Fvector &))(&Fvector::mad),												return_reference_to(_1))
			.def("mad",							(Fvector & (Fvector::*)(const Fvector &, const Fvector &, const Fvector &))(&Fvector::mad),								return_reference_to(_1))
//			.def("square_magnitude",			&Fvector::square_magnitude)
			.def("magnitude",					&Fvector::magnitude)
//			.def("normalize_magnitude",			&Fvector::normalize_magn)
			.def("normalize",					(Fvector & (Fvector::*)())(&Fvector::normalize_safe),																	return_reference_to(_1))
			.def("normalize",					(Fvector & (Fvector::*)(const Fvector &))(&Fvector::normalize_safe),													return_reference_to(_1))
			.def("normalize_safe",				(Fvector & (Fvector::*)())(&Fvector::normalize_safe),																	return_reference_to(_1))
			.def("normalize_safe",				(Fvector & (Fvector::*)(const Fvector &))(&Fvector::normalize_safe),													return_reference_to(_1))
//			.def("random_dir",					(Fvector & (Fvector::*)())(&Fvector::random_dir),																		return_reference_to(_1))
//			.def("random_dir",					(Fvector & (Fvector::*)(const Fvector &, float))(&Fvector::random_dir),													return_reference_to(_1))
//			.def("random_point",				(Fvector & (Fvector::*)(const Fvector &))(&Fvector::random_point),														return_reference_to(_1))
//			.def("random_point",				(Fvector & (Fvector::*)(float))(&Fvector::random_point),																return_reference_to(_1))
			.def("dotproduct",					&Fvector::dotproduct)
			.def("crossproduct",				&Fvector::crossproduct,																									return_reference_to(_1))
			.def("distance_to_xz",				&Fvector::distance_to_xz)
			.def("distance_to_sqr",				&Fvector::distance_to_sqr)
			.def("distance_to",					&Fvector::distance_to)
//			.def("from_bary",					(Fvector & (Fvector::*)(const Fvector &, const Fvector &, const Fvector &, float, float, float))(&Fvector::from_bary),	return_reference_to(_1))
//			.def("from_bary",					(Fvector & (Fvector::*)(const Fvector &, const Fvector &, const Fvector &, const Fvector &))(&Fvector::from_bary),		return_reference_to(_1))
//			.def("from_bary4",					&Fvector::from_bary4,																									return_reference_to(_1))
//			.def("mknormal_non_normalized",		&Fvector::mknormal_non_normalized,																						return_reference_to(_1))
//			.def("mknormal",					&Fvector::mknormal,																										return_reference_to(_1))
			.def("setHP",						&Fvector::setHP,																										return_reference_to(_1))
//			.def("getHP",						&Fvector::getHP,																																	out_value(_2) + out_value(_3))
			.def("getH",						&Fvector::getH)
			.def("getP",						&Fvector::getP)

			.def("reflect",						&Fvector::reflect,																										return_reference_to(_1))
			.def("slide",						&Fvector::slide,																										return_reference_to(_1)),
//			.def("generate_orthonormal_basis",	&Fvector::generate_orthonormal_basis),

		class_<Fbox>("Fbox")
			.def_readwrite("min",				&Fbox::min)
			.def_readwrite("max",				&Fbox::max)
			.def(								constructor<>()),

		class_<Fvector2>("Fvector2")
			.def_readwrite("x",					&Fvector2::x)
			.def_readwrite("y",					&Fvector2::y)
			.def(								constructor<>())
			.def("set",							(Fvector2 & (Fvector2::*)(float,float))(&Fvector2::set),																	return_reference_to(_1))
			.def("set",							(Fvector2 & (Fvector2::*)(const Fvector2 &))(&Fvector2::set),																return_reference_to(_1))
			 ,

		class_<Frect>("Frect")
			.def(								constructor<>())
			.def("set",							(Frect & (Frect::*)(float,float,float,float))(&Frect::set),																return_reference_to(_1))			
			.property("lt",						&Frect_get_lt, &Frect_set_lt)
			.property("rb",						&Frect_get_rb, &Frect_set_rb)
			.def_readwrite("x1",				&Frect::x1)
			.def_readwrite("x2",				&Frect::x2)
			.def_readwrite("y1",				&Frect::y1)
			.def_readwrite("y2",				&Frect::y2)
			.def_readwrite("left",				&Frect::x1)
			.def_readwrite("right",				&Frect::x2)
			.def_readwrite("top",				&Frect::y1)
			.def_readwrite("bottom",			&Frect::y2)
			.property("width",					&Frect::width, &Frect::set_width)
			.property("height",					&Frect::height, &Frect::set_height)
			,

		class_<Ivector>("Ivector")
			.def_readwrite("x",					&Ivector::x)
			.def_readwrite("y",					&Ivector::y)
			.def_readwrite("z",					&Ivector::z)
			.def(								constructor<>())
			.def("set",							(Ivector & (Ivector::*)(int,int,int))(&Ivector::set),																return_reference_to(_1))
			.def("set",							(Ivector & (Ivector::*)(const Ivector &))(&Ivector::set),															return_reference_to(_1))
			.def("add",							(Ivector & (Ivector::*)(int))(&Ivector::add),																			return_reference_to(_1))
			.def("add",							(Ivector & (Ivector::*)(const Ivector &))(&Ivector::add),																return_reference_to(_1))
			.def("add",							(Ivector & (Ivector::*)(const Ivector &, const Ivector &))(&Ivector::add),												return_reference_to(_1))
			.def("add",							(Ivector & (Ivector::*)(const Ivector &, int))(&Ivector::add),															return_reference_to(_1))
			.def("sub",							(Ivector & (Ivector::*)(int))(&Ivector::sub),																			return_reference_to(_1))
			.def("sub",							(Ivector & (Ivector::*)(const Ivector &))(&Ivector::sub),																return_reference_to(_1))
			.def("sub",							(Ivector & (Ivector::*)(const Ivector &, const Ivector &))(&Ivector::sub),												return_reference_to(_1))
			.def("sub",							(Ivector & (Ivector::*)(const Ivector &, int))(&Ivector::sub),															return_reference_to(_1))
			.def("mul",							(Ivector & (Ivector::*)(int))(&Ivector::mul),																			return_reference_to(_1))
			.def("mul",							(Ivector & (Ivector::*)(const Ivector &))(&Ivector::mul),																return_reference_to(_1))
			.def("mul",							(Ivector & (Ivector::*)(const Ivector &, const Ivector &))(&Ivector::mul),												return_reference_to(_1))
			.def("mul",							(Ivector & (Ivector::*)(const Ivector &, int))(&Ivector::mul),															return_reference_to(_1))
			.def("div",							(Ivector & (Ivector::*)(int))(&Ivector::div),																			return_reference_to(_1))
			.def("div",							(Ivector & (Ivector::*)(const Ivector &))(&Ivector::div),																return_reference_to(_1))
			.def("div",							(Ivector & (Ivector::*)(const Ivector &, const Ivector &))(&Ivector::div),												return_reference_to(_1))
			.def("div",							(Ivector & (Ivector::*)(const Ivector &, int))(&Ivector::div),															return_reference_to(_1))
			.def("invert",						(Ivector & (Ivector::*)())(&Ivector::invert),																			return_reference_to(_1))
			.def("invert",						(Ivector & (Ivector::*)(const Ivector &))(&Ivector::invert),																return_reference_to(_1))
			.def("min",							(Ivector & (Ivector::*)(const Ivector &))(&Ivector::min),																return_reference_to(_1))
			.def("min",							(Ivector & (Ivector::*)(const Ivector &, const Ivector &))(&Ivector::min),												return_reference_to(_1))
			.def("max",							(Ivector & (Ivector::*)(const Ivector &))(&Ivector::max),																return_reference_to(_1))
			.def("max",							(Ivector & (Ivector::*)(const Ivector &, const Ivector &))(&Ivector::max),												return_reference_to(_1))
			,

		class_<Ivector2>("Ivector2")
			.def_readwrite("x",					&Ivector2::x)
			.def_readwrite("y",					&Ivector2::y)
			.def(								constructor<>())
			.def("set",							(Ivector2 & (Ivector2::*)(float,float))(&Ivector2::set),																	return_reference_to(_1))
			.def("set",							(Ivector2 & (Ivector2::*)(const Ivector2 &))(&Ivector2::set),																return_reference_to(_1))
			 ,

			class_<Irect>("Irect")
			.def(								constructor<>())
			.def("set",							(Irect & (Irect::*)(int,int,int,int))(&Irect::set),																return_reference_to(_1))
			.property("lt",						&Irect_get_lt, &Irect_set_lt)
			.property("rb",						&Irect_get_rb, &Irect_set_rb)

			.def_readwrite("x1",				&Irect::x1)
			.def_readwrite("x2",				&Irect::x2)
			.def_readwrite("y1",				&Irect::y1)
			.def_readwrite("y2",				&Irect::y2)
			.def_readwrite("left",				&Irect::x1)
			.def_readwrite("right",				&Irect::x2)
			.def_readwrite("top",				&Irect::y1)
			.def_readwrite("bottom",			&Irect::y2)
			.property("width",					&Irect::width, &Irect::set_width)
			.property("height",					&Irect::height, &Irect::set_height)





	];
}
