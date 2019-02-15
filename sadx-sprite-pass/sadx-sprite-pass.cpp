#include "pch.h"

static std::vector<NJS_TEXANIM> texanims;
static std::array<NJD_COLOR_BLENDING, 2> blending_modes{};

struct QueuedSprite
{
	NJS_SPRITE sp;
	NJS_TEXLIST tlist {};
	NJS_TEXANIM tanim {};
	Int n;
	Float pri;
	NJD_SPRITE attr;
	NJD_COLOR_BLENDING srcblend, dstblend;
	NJS_MATRIX transform {};
	NJS_ARGB nj_constant_material_ {};

	QueuedSprite(const NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr)
		: sp(*sp),
		  n(n),
		  pri(pri),
		  attr(attr)
	{
		nj_constant_material_ = _nj_constant_material_;
		srcblend = blending_modes[0];
		dstblend = blending_modes[1];

		if (sp->tlist != nullptr)
		{
			this->tlist = *sp->tlist;
			this->sp.tlist = &this->tlist;
		}

		if (n >= 0)
		{
			if (static_cast<size_t>(n) >= texanims.size())
			{
				texanims.resize(n + 1);
			}

			this->tanim = sp->tanim[n];
		}

		njGetMatrix(transform);
	}

	void apply()
	{
		_nj_constant_material_ = nj_constant_material_;

		njColorBlendingMode_(NJD_SOURCE_COLOR, srcblend);
		njColorBlendingMode_(NJD_DESTINATION_COLOR, dstblend);

		if (n < 0)
		{
			return;
		}

		njSetMatrix(nullptr, transform);
		sp.tanim = texanims.data();
		texanims[n] = tanim;
	}
};

static std::vector<QueuedSprite> sprites;

void __cdecl njColorBlendingMode__r(NJD_COLOR_TARGET target, NJD_COLOR_BLENDING mode);
Trampoline njColorBlendingMode__t(0x0077EC60, 0x0077EC66, njColorBlendingMode__r);
void __cdecl njColorBlendingMode__r(NJD_COLOR_TARGET target, NJD_COLOR_BLENDING mode)
{
	auto original = reinterpret_cast<decltype(njColorBlendingMode__r)*>(njColorBlendingMode__t.Target());

	original(target, mode);

	if (mode)
	{
		if (mode == NJD_COLOR_BLENDING_BOTHSRCALPHA)
		{
			blending_modes[NJD_SOURCE_COLOR] = NJD_COLOR_BLENDING_SRCALPHA;
			blending_modes[NJD_DESTINATION_COLOR] = NJD_COLOR_BLENDING_SRCALPHA;
		}
		else
		{
			const size_t i = target % blending_modes.size();
			blending_modes[i] = mode;
		}
	}
	else
	{
		blending_modes[NJD_SOURCE_COLOR] = NJD_COLOR_BLENDING_INVSRCALPHA;
		blending_modes[NJD_DESTINATION_COLOR] = NJD_COLOR_BLENDING_INVSRCALPHA;
	}
}


void enqueue_sprite(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr)
{
	if (!sp)
	{
		return;
	}

	sprites.emplace_back(sp, n, pri, attr);
}

static void __cdecl njDrawSprite2D_DrawNow_r(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr);
static Trampoline njDrawSprite2D_DrawNow_t(0x0077E050, 0x0077E058, njDrawSprite2D_DrawNow_r);

static void __cdecl njDrawSprite2D_DrawNow_o(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr)
{
	NonStaticFunctionPointer(void, original, (NJS_SPRITE*, Int, Float, NJD_SPRITE), njDrawSprite2D_DrawNow_t.Target());
	original(sp, n, pri, attr);
}

DataPointer(void*, Direct3D_Device, 0x03D128B0);

static void reset_stream_source()
{
	// all to avoid linking with d3d8.lib
	const auto value = Direct3D_Device;

	__asm
	{
		mov eax, dword ptr [value]
		mov edx, [eax]
		push 0
		push 0
		push 0
		push eax
		call dword ptr [edx+0000014Ch]

		mov eax, dword ptr[value]
		mov ecx, [eax]
		push 0
		push 0
		push eax
		call dword ptr [ecx+00000154h]
	}
}

static void __cdecl njDrawSprite2D_DrawNow_r(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr)
{
	if (!sp || !sp->tlist)
	{
		return;
	}

	NJS_SPRITE _sp = *sp;
	_sp.sx = 0.0f;
	_sp.sy = 0.0f;

	njDrawSprite2D_DrawNow_o(&_sp, n, pri, attr);

	Direct3D_EnableHudAlpha((attr & NJD_SPRITE_ALPHA) == NJD_SPRITE_ALPHA);
	enqueue_sprite(sp, n, pri, attr);
	reset_stream_source();

	Direct3D_SetTexList(sp->tlist);

	NJS_COLOR color_, v16;

	if (attr & NJD_SPRITE_COLOR)
	{
		color_.argb.b = (uint8_t)(_nj_constant_material_.b * 255.0f);
		color_.argb.g = (uint8_t)(_nj_constant_material_.g * 255.0f);
		color_.argb.r = (uint8_t)(_nj_constant_material_.r * 255.0f);
		color_.argb.a = (uint8_t)(_nj_constant_material_.a * 255.0f);
		v16.color     = color_.color;
	}
	else
	{
		v16.color = 0xFFFFFFFF;
	}

	Direct3D_EnableHudAlpha((attr & NJD_SPRITE_ALPHA) == NJD_SPRITE_ALPHA);

	auto texid = sp->tanim[n].texid;

	CurrentHUDColor = v16.color;
	Direct3D_SetTextureNum(texid);
	Direct3D_TextureFilterLinear();
}

//#define USE_QUEUED

#ifdef USE_QUEUED
static void __cdecl njDrawSprite2D_Queue_r(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr, QueuedModelFlagsB queue_flags);
static Trampoline njDrawSprite2D_Queue_t(0x00404660, 0x00404666, njDrawSprite2D_Queue_r);

static void __cdecl njDrawSprite2D_Queue_r(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr, QueuedModelFlagsB queue_flags)
{
	if (sp == nullptr)
	{
		return;
	}

	//NonStaticFunctionPointer(void, original, (NJS_SPRITE*, Int, Float, NJD_SPRITE, QueuedModelFlagsB), njDrawSprite2D_Queue_t.Target());
	//original(sp, n, pri, attr, queue_flags);
	enqueue_sprite(sp, n, pri, attr);
}
#endif

#define EXPORT __declspec(dllexport)

extern "C"
{
	EXPORT ModInfo SADXModInfo = {
		ModLoaderVer,
		nullptr,
		nullptr,
		0,
		nullptr,
		0,
		nullptr,
		0,
		nullptr,
		0
	};

	HMODULE sadx_d3d11 = nullptr;
	void(*oit_enable_)() = nullptr;
	void(*oit_disable_)() = nullptr;
	bool(*oit_enabled_)() = nullptr;

	EXPORT void Init()
	{
		sprites.reserve(512);
		texanims.reserve(128);

		sadx_d3d11 = GetModuleHandle(L"sadx-d3d11.dll");

		if (sadx_d3d11 != INVALID_HANDLE_VALUE)
		{
			oit_enable_  = reinterpret_cast<decltype(oit_enable_)>(GetProcAddress(sadx_d3d11, "oit_enable"));
			oit_disable_ = reinterpret_cast<decltype(oit_disable_)>(GetProcAddress(sadx_d3d11, "oit_disable"));
			oit_enabled_ = reinterpret_cast<decltype(oit_enabled_)>(GetProcAddress(sadx_d3d11, "oit_enabled"));
		}

		//WriteData<2>((void*)0x0077DF14, 0x90i8);
		//WriteData<12>((void*)0x0077DF29, 0x90i8);
	}

	void oit_enable()
	{
		if (oit_enable_)
		{
			oit_enable_();
		}
	}

	void oit_disable()
	{
		if (oit_disable_)
		{
			oit_disable_();
		}
	}

	bool oit_enabled()
	{
		if (oit_enabled_)
		{
			return oit_enabled_();
		}

		return false;
	}

	EXPORT void OnRenderSceneEnd()
	{
		const bool is_oit_enabled = oit_enabled();
		oit_disable();

		Direct3D_EnableZWrite(false);
		Direct3D_SetZFunc(7);

		njPushMatrix(nullptr);
		for (auto& sprite : sprites)
		{
			sprite.apply();
			njDrawSprite2D_DrawNow_o(&sprite.sp, sprite.n, sprite.pri, sprite.attr);
		}
		njPopMatrix(1);

		sprites.clear();

		if (is_oit_enabled)
		{
			oit_enable();
		}
	}
}
