//const float global_uno = 1.0;
		string32   tmp;
		const void *ptr = &global_uno;
		sprintf_s (tmp, 32, "global_uno=0x%x", (u32)ptr - (u32)GetModuleHandle("xr_3da.exe"));
		Msg("#DBG: %s", tmp);
