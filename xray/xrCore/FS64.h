#pragma once

ICF u32 HIDWORD(const u64 value)
{
	return ((u32*)&value)[1];
}
ICF u32 LODWORD(const u64 value)
{
	return ((u32*)&value)[0];
}
