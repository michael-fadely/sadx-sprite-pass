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

//#define USE_QUEUED

#ifdef USE_QUEUED
static void __cdecl njDrawSprite2D_Queue_r(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr, QueuedModelFlagsB queue_flags);
static Trampoline njDrawSprite2D_Queue_t(0x00404660, 0x00404666, njDrawSprite2D_Queue_r);
#endif

static void __cdecl njDrawSprite2D_DrawNow_r(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr);
static Trampoline njDrawSprite2D_DrawNow_t(0x0077E050, 0x0077E058, njDrawSprite2D_DrawNow_r);
static void __cdecl njDrawSprite2D_DrawNow_o(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr)
{
	NonStaticFunctionPointer(void, original, (NJS_SPRITE*, Int, Float, NJD_SPRITE), njDrawSprite2D_DrawNow_t.Target());
	original(sp, n, pri, attr);
}

void enqueue_sprite(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr)
{
	if (!sp)
	{
		return;
	}

	sprites.emplace_back(sp, n, pri, attr);
}

#ifdef USE_QUEUED
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

static void __cdecl njDrawSprite2D_DrawNow_r(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr)
{
	enqueue_sprite(sp, n, pri, attr);
}

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

	EXPORT void Init()
	{
		sprites.reserve(512);
		texanims.reserve(128);
	}

	EXPORT void OnRenderSceneEnd()
	{
		//Direct3D_SetDefaultRenderState();
		//Direct3D_SetDefaultTextureStageState();

		//Direct3D_SetNullTexture();
		//Direct3D_EnableZWrite(false);
		//Direct3D_SetZFunc(7);

		//njAlphaMode(2);
		//njColorBlendingMode_(NJD_SOURCE_COLOR, NJD_COLOR_BLENDING_SRCALPHA);
		//njColorBlendingMode_(NJD_DESTINATION_COLOR, NJD_COLOR_BLENDING_INVSRCALPHA);

		auto nj_constant_material_ = _nj_constant_material_;

		njPushMatrix(nullptr);
		for (auto& sprite : sprites)
		{
			sprite.apply();
			njDrawSprite2D_DrawNow_o(&sprite.sp, sprite.n, sprite.pri, sprite.attr);
		}
		njPopMatrix(1);

		_nj_constant_material_ = nj_constant_material_;

		sprites.clear();
	}
}
