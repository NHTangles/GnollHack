/* GnollHack 4.0	objects.c	$NHDT-Date: 1535422421 2018/08/28 02:13:41 $  $NHDT-Branch: GnollHack-3.6.2-beta01 $:$NHDT-Revision: 1.51 $ */
/* Copyright (c) Janne Gustafsson 2019.                           */
/* GnollHack may be freely redistributed.  See license for details. */

#include "hack.h"

struct materialcomponentlist matlists[] =
{
	{STRANGE_OBJECT,
	"",
	1,
	{NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_WISH,
	"Blessed diamond",
	1,
	{{DIAMOND, NOT_APPLICABLE, 1, MATCOMP_BLESSED_REQUIRED}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_TIME_STOP,
	"Blessed sapphire",
	1,
	{{SAPPHIRE, NOT_APPLICABLE, 1, MATCOMP_BLESSED_REQUIRED}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_ARMAGEDDON,
	"Black pearl and death bone dagger",
	1,
	{{BLACK_PEARL, NOT_APPLICABLE, 1, MATCOMP_NOT_CURSED},
	 {BONE_DAGGER, NOT_APPLICABLE, 1, MATCOMP_DEATH_ENCHANTMENT_REQUIRED | MATCOMP_NOT_SPENT | MATCOMP_NOT_CURSED},
	 NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_BLACK_BLADE_OF_DISASTER,
	"Black opal",
	1,
	{{BLACK_OPAL, NOT_APPLICABLE, 1, MATCOMP_NOT_CURSED}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_IDENTIFY,
	"Jade stone",
	1,
	{{JADE, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_TOUCH_OF_DEATH,
	"Jet stone",
	1,
	{{JET, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_FINGER_OF_DEATH,
	"Obsidian stone",
	1,
	{{OBSIDIAN, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_DEATHSPELL,
	"Obsidian stone and bone dagger",
	1,
	{{OBSIDIAN, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, {BONE_DAGGER, NOT_APPLICABLE, 1, MATCOMP_NOT_SPENT | MATCOMP_NOT_CURSED}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_POLYMORPH,
	"Amethyst",
	3,
	{{AMETHYST, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_FULL_HEALING,
	"Emerald",
	1,
	{{EMERALD, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_DIG,
	"Chrysoberyl stone",
	1,
	{{CHRYSOBERYL, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_MAGIC_MAPPING,
	"Pearl",
	1,
	{{PEARL, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_CURSE,
	"Mandrake root and holy symbol",
	3,
	{{MANDRAKE_ROOT, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, {HOLY_SYMBOL, NOT_APPLICABLE, 1, MATCOMP_NOT_SPENT}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_BLESS,
	"Potion of water and holy symbol",
	3,
	{{POT_WATER, NOT_APPLICABLE, 1, MATCOMP_NOT_CURSED}, {HOLY_SYMBOL, NOT_APPLICABLE, 1, MATCOMP_NOT_SPENT | MATCOMP_NOT_CURSED}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_ENCHANT_WEAPON,
	"Garnet stone",
	1,
	{{GARNET, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_ENCHANT_ARMOR,
	"Jasper stone",
	1,
	{{JASPER, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_PROTECT_WEAPON,
	"Fluorite stone",
	1,
	{{FLUORITE, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_PROTECT_ARMOR,
	"Agate stone",
	1,
	{{AGATE, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{SPE_NEGATE_UNDEATH,
	"Clove of garlic and holy symbol",
	1,
	{{CLOVE_OF_GARLIC, NOT_APPLICABLE, 1, MATCOMP_NOT_CURSED}, {HOLY_SYMBOL, NOT_APPLICABLE, 1, MATCOMP_NOT_SPENT | MATCOMP_NOT_CURSED}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

	{ SPE_BANISH_DEMON,
	"Silver arrow and holy symbol",
	1,
	{{SILVER_ARROW, NOT_APPLICABLE, 1, MATCOMP_NOT_CURSED}, {HOLY_SYMBOL, NOT_APPLICABLE, 1, MATCOMP_NOT_SPENT | MATCOMP_NOT_CURSED}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_RESURRECTION,
	"Garlic, ginseng, and holy symbol",
	2,
	{{CLOVE_OF_GARLIC, NOT_APPLICABLE, 1, MATCOMP_NOT_CURSED}, {GINSENG_ROOT, NOT_APPLICABLE, 1, MATCOMP_NOT_CURSED}, {HOLY_SYMBOL, NOT_APPLICABLE, 1, MATCOMP_NOT_SPENT | MATCOMP_NOT_CURSED}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_GLOBE_OF_INVULNERABILITY,
	"Pearl",
	1,
	{{PEARL, NOT_APPLICABLE, 1, MATCOMP_NOT_CURSED}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_DIVINE_INTERVENTION,
	"Spider silk and blessed pearl",
	1,
	{{THREAD_OF_SPIDER_SILK, NOT_APPLICABLE, 1, MATCOMP_NOT_CURSED}, {PEARL, NOT_APPLICABLE, 1, MATCOMP_BLESSED_REQUIRED}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_CANCELLATION,
	"Opal",
	5,
	{{OPAL, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_CALL_DEMOGORGON,
	"Cursed ape corpse, bl.pearl, mandrake",
	1,
	{{CORPSE, PM_APE, 1, MATCOMP_CURSED_REQUIRED}, {BLACK_PEARL, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, {MANDRAKE_ROOT, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_WATER_BREATHING,
	"Pearl",
	3,
	{{PEARL, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_WATER_WALKING,
	"Raven feather",
	3,
	{{RAVEN_FEATHER, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_SLEEP,
	"Ginseng root",
	5,
	{{GINSENG_ROOT, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_FEAR,
	"Mandrake root",
	5,
	{{MANDRAKE_ROOT, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_MASS_FEAR,
	"Mandrake root and fungal spore",
	1,
	{{MANDRAKE_ROOT, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS},  {FUNGAL_SPORE, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_CHARM_MONSTER,
	"Ginseng and wolfsbane",
	5,
	{{GINSENG_ROOT, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, {SPRIG_OF_WOLFSBANE, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_SPHERE_OF_CHARMING,
	"Ginseng, wolfsbane, fungal spore",
	3,
	{{GINSENG_ROOT, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, {SPRIG_OF_WOLFSBANE, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, {FUNGAL_SPORE, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_MASS_CHARM,
	"Ginseng, wolfsbane, 2 fungal spores",
	2,
	{{GINSENG_ROOT, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, {SPRIG_OF_WOLFSBANE, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, {FUNGAL_SPORE, NOT_APPLICABLE, 2, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_DOMINATE_MONSTER,
	"Blessed ginseng and wolfsbane",
	1,
	{{GINSENG_ROOT, NOT_APPLICABLE, 1, MATCOMP_BLESSED_REQUIRED}, {SPRIG_OF_WOLFSBANE, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

	{ SPE_SPHERE_OF_DOMINATION,
	"Blessed ginseng, wolfsbane, fungal spore",
	1,
	{{GINSENG_ROOT, NOT_APPLICABLE, 1, MATCOMP_BLESSED_REQUIRED}, {SPRIG_OF_WOLFSBANE, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, {FUNGAL_SPORE, NOT_APPLICABLE, 1, MATCOMP_NO_FLAGS}, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP} },

		//Array terminator, uses spellsgained
	{STRANGE_OBJECT,
	"",
	0,
	{NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP, NO_MATCOMP}},

};


void matcomps_init();

/* dummy routine used to force linkage */
void
matcomps_init()
{
    return;
}

/*matcomps.c*/
