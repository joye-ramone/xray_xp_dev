void CLASS_IMPL::SetColor (u32 color, int target)
{
	Fcolor c;
	c.set (color);
	switch (target)
	{
		case 0: target_0->set_color(c); break;
		case 1: target_1->set_color(c); break;
		case 2: target_2->set_color(c); break;			
	}
	
}

void CLASS_IMPL::SetRGB (float r, float g, float b, int target)
{
	switch (target)
	{
		case 0: target_0->set_color(r, g, b); break;
		case 1: target_1->set_color(r, g, b); break;
		case 2: target_2->set_color(r, g, b); break;
	}

}

void CLASS_IMPL::SetAngle(float angle, int target)
{	
	switch (target)
	{
	case 0: target_0->set_cone(angle); break;
	case 1: target_1->set_cone(angle); break;	
	}	
}

void CLASS_IMPL::SetRange(float range, int target)
{	
	switch (target)
	{
		case 0: target_0->set_range(range); break;
		case 1: target_1->set_range(range); break;
		case 2: target_2->set_radius(range); break;
	}
}

void CLASS_IMPL::SetTexture(LPCSTR texture, int target)
{
	switch (target)
	{
	case 0: target_0->set_texture(texture); break;
	case 1: target_1->set_texture(texture); break;
	case 2: target_2->set_texture(texture); break;
	}
}
