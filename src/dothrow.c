/* GnollHack 4.0	dothrow.c	$NHDT-Date: 1556201496 2019/04/25 14:11:36 $  $NHDT-Branch: GnollHack-3.6.2-beta01 $:$NHDT-Revision: 1.160 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2013. */
/* GnollHack may be freely redistributed.  See license for details. */

/* Contains code for 't' (throw) */

#include "hack.h"

STATIC_DCL int FDECL(throw_obj, (struct obj *, int));
STATIC_DCL boolean FDECL(ok_to_throw, (int *));
STATIC_DCL void NDECL(autoquiver);
STATIC_DCL int FDECL(gem_accept, (struct monst *, struct obj *));
STATIC_DCL void FDECL(tmiss, (struct obj *, struct monst *, BOOLEAN_P));
STATIC_DCL int FDECL(throw_gold, (struct obj *));
STATIC_DCL void FDECL(breakmsg, (struct obj *, BOOLEAN_P));
STATIC_DCL boolean FDECL(toss_up, (struct obj *, BOOLEAN_P));
STATIC_DCL void FDECL(sho_obj_return_to_u, (struct obj * obj));
STATIC_DCL boolean FDECL(mhurtle_step, (genericptr_t, int, int));

static NEARDATA const char toss_objs[] = { ALLOW_COUNT, COIN_CLASS,
                                           ALL_CLASSES, WEAPON_CLASS, 0 };
/* different default choices when wielding a sling (gold must be included) */
static NEARDATA const char bullets[] = { ALLOW_COUNT, COIN_CLASS, ALL_CLASSES,
                                         GEM_CLASS, 0 };

/* thrownobj (decl.c) tracks an object until it lands */

extern boolean notonhead; /* for long worms */

/* Throw the selected object, asking for direction */
STATIC_OVL int
throw_obj(obj, shotlimit)
struct obj *obj;
int shotlimit;
{
    struct obj *otmp;
    int multishot;
    schar skill;
    long wep_mask;
    //boolean weakmultishot;

    /* ask "in what direction?" */
    if (!getdir((char *) 0)) {
        /* No direction specified, so cancel the throw;
         * might need to undo an object split.
         * We used to use freeinv(obj),addinv(obj) here, but that can
         * merge obj into another stack--usually quiver--even if it hadn't
         * been split from there (possibly triggering a panic in addinv),
         * and freeinv+addinv potentially has other side-effects.
         */
        if (obj->o_id == context.objsplit.parent_oid
            || obj->o_id == context.objsplit.child_oid)
            (void) unsplitobj(obj);
        return 0; /* no time passes */
    }

    /*
     * Throwing money is usually for getting rid of it when
     * a leprechaun approaches, or for bribing an oncoming
     * angry monster.  So throw the whole object.
     *
     * If the money is in quiver, throw one coin at a time,
     * possibly using a sling.
     */
    if (obj->oclass == COIN_CLASS && obj != uquiver)
        return throw_gold(obj);

    if (!canletgo(obj, "throw"))
        return 0;
    if ((objects[obj->otyp].oc_flags & O1_CAN_BE_THROWN_ONLY_IF_WIELDED) && obj != uwep) {
        pline("%s must be wielded before it can be thrown.", The(xname(obj)));
        return 0;
    }
    if (//(obj->oartifact == ART_MJOLLNIR && ACURR(A_STR) < STR18(100)) //STR19(25)
        (obj->otyp == BOULDER && !(throws_rocks(youmonst.data))))  // || (int)obj->owt <= enclevelmaximumweight(UNENCUMBERED))))
	{
        pline("It's too heavy.");
        return 1;
    }
    if (!u.dx && !u.dy && !u.dz) {
        You("cannot throw an object at yourself.");
        return 0;
    }
    u_wipe_engr(2);
    if (!uarmg && obj->otyp == CORPSE && touch_petrifies(&mons[obj->corpsenm])
        && !Stone_resistance) {
        You("throw %s with your bare %s.",
            corpse_xname(obj, (const char *) 0, CXN_PFX_THE),
            /* throwing with one hand, but pluralize since the
               expression "with your bare hands" sounds better */
            makeplural(body_part(HAND)));
        Sprintf(killer.name, "throwing %s bare-handed", killer_xname(obj));
        instapetrify(killer.name);
    }
    if (welded(obj, &youmonst)) {
        weldmsg(obj);
        return 1;
    }
    if (is_wet_towel(obj))
        dry_a_towel(obj, -1, FALSE);

    /* Multishot calculations
     * (potential volley of up to N missiles; default for N is 1)
     */
    multishot = 1;
	int multishotrndextra = 0;
    skill = objects[obj->otyp].oc_skill;
    if (obj->quan > 1L /* no point checking if there's only 1 */
        /* ammo requires corresponding launcher be wielded */
        && (is_ammo(obj) ? matching_launcher(obj, uwep)
                         /* otherwise any stackable (non-ammo) weapon */
                         : (obj->oclass == WEAPON_CLASS && !is_launcher(obj)))
        && !(Confusion || Stunned)) 
	{
#if 0
        /* some roles don't get a volley bonus until becoming expert */
        weakmultishot = (Role_if(PM_WIZARD) || Role_if(PM_PRIEST)
                         || (Role_if(PM_HEALER) && skill != P_DAGGER)
                         || (Role_if(PM_TOURIST) && skill != -P_DART)
                         /* poor dexterity also inhibits multishot */
                         || Fumbling || ACURR(A_DEX) <= 6);
        /* Bonus if the player is proficient in this weapon... */
		/*
        switch (P_SKILL(weapon_skill_type(obj))) {
        case P_EXPERT:
			if (!weakmultishot)
				multishot++;
			break;
        case P_SKILLED:
			if (!weakmultishot)
				multishotrndextra++;
			break;
        default:
            break;
        }
		*/
#endif

		get_multishot_stats(&youmonst, obj, uwep, TRUE, &multishot, &multishotrndextra);

#if 0
		struct obj* otmpmulti = (struct obj*)0;
		if(obj && is_ammo(obj) && uwep && matching_launcher(obj, uwep))
			otmpmulti = uwep;
		else if(obj)
			otmpmulti = obj;

		if (otmpmulti && objects[otmpmulti->otyp].oc_multishot_style > 1)
		{
			int skilllevel = P_SKILL(weapon_skill_type(otmpmulti));
			boolean multishotok = TRUE;

			/*
			if ((objects[otmpmulti->otyp].oc_flags3 & O3_MULTISHOT_REQUIRES_SKILL_MASK) == O3_MULTISHOT_REQUIRES_EXPERT_SKILL && skilllevel < P_EXPERT)
				multishotok = FALSE;
			else if ((objects[otmpmulti->otyp].oc_flags3 & O3_MULTISHOT_REQUIRES_SKILL_MASK) == O3_MULTISHOT_REQUIRES_SKILLED_SKILL && skilllevel < P_SKILLED)
				multishotok = FALSE;
			else if ((objects[otmpmulti->otyp].oc_flags3 & O3_MULTISHOT_REQUIRES_SKILL_MASK) == O3_MULTISHOT_REQUIRES_BASIC_SKILL && skilllevel < P_BASIC)
				multishotok = FALSE;
			*/

			if (multishotok)
			{
				multishot = objects[otmpmulti->otyp].oc_multishot_style;
			}
		}
#endif


#if 0
        /* ...or is using a special weapon for their role... */
        switch (Role_switch) {
        case PM_MONK:
            /* allow higher volley count despite skill limitation */
            if (skill == -P_THROWN_WEAPON && !weakmultishot)
                multishot++;
            break;
        default:
            break; /* No bonus */
        }
        /* ...or using their race's special bow; no bonus for spears */
		/*
        if (!weakmultishot)
            switch (Race_switch) {
            case PM_ELF:
                if (obj->otyp == ELVEN_ARROW && uwep
                    && uwep->otyp == ELVEN_LONG_BOW)
                    multishot++;
                break;
            case PM_ORC:
                if (obj->otyp == ORCISH_ARROW && uwep
                    && uwep->otyp == ORCISH_SHORT_BOW)
                    multishot++;
                break;
            case PM_GNOLL:
				if (obj->otyp == CROSSBOW_BOLT && uwep
					&& uwep->otyp == CROSSBOW)
					multishot++;
				break;
            case PM_HUMAN:
            case PM_DWARF:
            default:
                break; 
            }
		*/
        /* crossbows are slow to load and probably shouldn't allow multiple
           shots at all, but that would result in players never using them;
           instead, high strength is necessary to load and shoot quickly */

		/*
		if (multishot > 1)
		{
			if (
				(obj && uwep && ammo_and_launcher(obj, uwep)
				&& (ACURR(A_STR) < objects[uwep->otyp].oc_multishot_str))
					|| (obj && throwing_weapon(obj) && objects[obj->otyp].oc_multishot_str > 0 && ACURR(A_STR) < objects[obj->otyp].oc_multishot_str)
					)
			{
				multishot = 1;
				multishotrndextra = 0;
			}
		}
		*/
#endif

		if(multishotrndextra > 0)
	        multishot += rn2(multishotrndextra + 1); //add random amount;
        
		if ((long) multishot > obj->quan)
            multishot = (int) obj->quan;
        if (shotlimit > 0 && multishot > shotlimit)
            multishot = shotlimit;
    }

    m_shot.s = ammo_and_launcher(obj, uwep) ? TRUE : FALSE;
    /* give a message if shooting more than one, or if player
       attempted to specify a count */
    if (multishot > 1 || shotlimit > 0) {
        /* "You shoot N arrows." or "You throw N daggers." */
        You("%s %d %s.", m_shot.s ? "shoot" : "throw",
            multishot, /* (might be 1 if player gave shotlimit) */
            (multishot == 1) ? singular(obj, xname) : xname(obj));
    }

    wep_mask = obj->owornmask;
    m_shot.o = obj->otyp;
    m_shot.n = multishot;
    for (m_shot.i = 1; m_shot.i <= m_shot.n; m_shot.i++) 
	{
		context.multishot_target_killed = FALSE;
        /* split this object off from its slot if necessary */
        if (obj->quan > 1L) {
            otmp = splitobj(obj, 1L);
        } else {
            otmp = obj;
            if (otmp->owornmask)
                remove_worn_item(otmp, FALSE);
        }
        freeinv(otmp);
        throwit(otmp, wep_mask);
		if (context.multishot_target_killed == TRUE)
		{
			context.multishot_target_killed = FALSE;
			break;
		}
    }
    m_shot.n = m_shot.i = 0;
    m_shot.o = STRANGE_OBJECT;
    m_shot.s = FALSE;

    return 1;
}

void
get_multishot_stats(magr, otmp, weapon, thrown, output_multishot_constant, output_multishot_rnd)
struct monst* magr;
struct obj* otmp;
struct obj* weapon;
boolean thrown;
int* output_multishot_constant;
int* output_multishot_rnd;
{
	if (!output_multishot_constant || !output_multishot_rnd)
		return;

	*output_multishot_constant = 1;
	*output_multishot_rnd = 0;

	if (!magr)
		return;

	if (!otmp || otmp == uarmg)
	{
		int skilllevel = 0;
		/* martial arts */
		if (magr == &youmonst)
		{
			skilllevel = P_SKILL(P_MARTIAL_ARTS);
		}
		else
		{
			switch (magr->mnum)
			{
			case PM_MONK:
				skilllevel = P_SKILLED;
				break;
			case PM_NINJA:
			case PM_ROSHI:
			case PM_ABBOT:
				skilllevel = P_EXPERT;
				break;
			case PM_GRAND_MASTER:
			case PM_MASTER_KAEN:
				skilllevel = P_GRAND_MASTER;
				break;
			default:
				break;
			}
		}

		switch (skilllevel)
		{
		case P_BASIC:
			*output_multishot_constant = 1;
			break;
		case P_SKILLED:
			*output_multishot_constant = 1;
			if (rn2(4) < 1)
				(*output_multishot_constant)++;
			break;
		case P_EXPERT:
			*output_multishot_constant = 1;
			if (rn2(4) < 2)
				(*output_multishot_constant)++;
			break;
		case P_MASTER:
			*output_multishot_constant = 1;
			if (rn2(4) < 3)
				(*output_multishot_constant)++;
			break;
		case P_GRAND_MASTER:
			*output_multishot_constant = 2;
			break;
		default:
			break;
		}
		return;
	}

	boolean isammo = is_ammo(otmp);
	boolean matching = FALSE;
	int skilllevel = 0;
	int used_multishotstyle = 0; 
	/*
	   1 = ammo with launcher (use launcher multishot stats for launching only),
	   2 = thrown (use thrown object's multishot stats for thrown only),
	   3 = melee (use weapon's multishot stats for melee only)
	*/
	/* E.g., if you hit in melee with a repeating crossbow, it does not give you multiple strikes */
	struct obj* otmpmulti = (struct obj*)0;
	otmpmulti = otmp; /* Unless launcher will be used */

	/* Check if launcher stats should be used */
	if (isammo && weapon)
	{
		matching = matching_launcher(otmp, weapon);
		if (matching)
			otmpmulti = weapon;
	}

	/* Find skill level */
	if (magr == &youmonst)
	{
		skilllevel = P_SKILL(weapon_skill_type(otmpmulti));
	}
	else
	{
		if (is_prince(magr->data))
			skilllevel = P_EXPERT;
		else if (is_lord(magr->data))
			skilllevel = P_EXPERT;
		else
			skilllevel = P_BASIC;
	}


	/* choose multishot style */
	if (matching && thrown)
		used_multishotstyle = 1;
	else if(!matching && thrown)
		used_multishotstyle = 2;
	else
		used_multishotstyle = 3;


	int multishotstyle = objects[otmpmulti->otyp].oc_multishot_style;

	if(used_multishotstyle == 1)
	{
		/* ammo and laucher */
		switch (multishotstyle)
		{
		case MULTISHOT_LAUNCHER_1D2_NOSKILL:
			*output_multishot_rnd = 1;
			break;
		case MULTISHOT_LAUNCHER_1D2_BASIC:
			if(skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
		case MULTISHOT_LAUNCHER_1D2_SKILLED:
			if (skilllevel >= P_SKILLED)
				*output_multishot_rnd = 1;
		case MULTISHOT_LAUNCHER_1D2_EXPERT:
			if (skilllevel >= P_EXPERT)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_LAUNCHER_2_NOSKILL:
			*output_multishot_constant = 2;
			break;
		case MULTISHOT_LAUNCHER_2_BASIC:
			if (skilllevel >= P_BASIC)
				*output_multishot_constant = 2;
		case MULTISHOT_LAUNCHER_2_SKILLED:
			if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
		case MULTISHOT_LAUNCHER_2_EXPERT:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 2;
			break;
		case MULTISHOT_LAUNCHER_1D2_1_NOSKILL:
			*output_multishot_constant = 2;
			*output_multishot_rnd = 1;
			break;
		case MULTISHOT_LAUNCHER_3_NOSKILL:
			*output_multishot_constant = 3;
			break; 
		case MULTISHOT_LAUNCHER_1D3_NOSKILL:
			*output_multishot_rnd = 2;
			break;
		case MULTISHOT_LAUNCHER_2_SKILLED_1D2_BASIC:
			if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_LAUNCHER_1D2_1_EXPERT_2_SKILLED_1D2_BASIC:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 2, * output_multishot_rnd = 1;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
			break; 
		case MULTISHOT_LAUNCHER_3_EXPERT_2_SKILLED_1D2_BASIC:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 3;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_LAUNCHER_4_EXPERT_3_SKILLED_2_BASIC:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 4;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 3;
			else if (skilllevel >= P_BASIC)
				*output_multishot_constant = 2;
			break;
		case MULTISHOT_LAUNCHER_2_EXPERT_1D2_SKILLED:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_LAUNCHER_3_EXPERT_2_SKILLED:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 3;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			break;
		default:
			break;
		}

	}
	else if (used_multishotstyle == 2)
	{
		/* thrown weapons */
		switch (multishotstyle)
		{
		case MULTISHOT_THROWN_1D2_NOSKILL:
			*output_multishot_rnd = 1;
			break;
		case MULTISHOT_THROWN_1D2_BASIC:
			if (skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
		case MULTISHOT_THROWN_1D2_SKILLED:
			if (skilllevel >= P_SKILLED)
				*output_multishot_rnd = 1;
		case MULTISHOT_THROWN_1D2_EXPERT:
			if (skilllevel >= P_EXPERT)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_THROWN_2_NOSKILL:
			*output_multishot_constant = 2;
			break;
		case MULTISHOT_THROWN_2_BASIC:
			if (skilllevel >= P_BASIC)
				*output_multishot_constant = 2;
		case MULTISHOT_THROWN_2_SKILLED:
			if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
		case MULTISHOT_THROWN_2_EXPERT:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 2;
			break;
		case MULTISHOT_THROWN_1D2_1_NOSKILL:
			*output_multishot_constant = 2;
			*output_multishot_rnd = 1;
			break;
		case MULTISHOT_THROWN_3_NOSKILL:
			*output_multishot_constant = 3;
			break;
		case MULTISHOT_THROWN_1D3_NOSKILL:
			*output_multishot_rnd = 2;
			break;
		case MULTISHOT_THROWN_2_SKILLED_1D2_BASIC:
			if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_THROWN_1D2_1_EXPERT_2_SKILLED_1D2_BASIC:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 2, * output_multishot_rnd = 1;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_THROWN_3_EXPERT_2_SKILLED_1D2_BASIC:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 3;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_THROWN_4_EXPERT_3_SKILLED_2_BASIC:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 4;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 3;
			else if (skilllevel >= P_BASIC)
				*output_multishot_constant = 2;
			break;
		case MULTISHOT_THROWN_2_EXPERT_1D2_SKILLED:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_THROWN_3_EXPERT_2_SKILLED:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 3;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			break;
		default:
			break;
		}
	}
	else if (used_multishotstyle == 3)
	{
		/* melee weapons */
		switch (multishotstyle)
		{
		case MULTISHOT_MELEE_1D2_NOSKILL:
			*output_multishot_rnd = 1;
			break;
		case MULTISHOT_MELEE_1D2_BASIC:
			if (skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
		case MULTISHOT_MELEE_1D2_SKILLED:
			if (skilllevel >= P_SKILLED)
				*output_multishot_rnd = 1;
		case MULTISHOT_MELEE_1D2_EXPERT:
			if (skilllevel >= P_EXPERT)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_MELEE_2_NOSKILL:
			*output_multishot_constant = 2;
			break;
		case MULTISHOT_MELEE_2_BASIC:
			if (skilllevel >= P_BASIC)
				*output_multishot_constant = 2;
		case MULTISHOT_MELEE_2_SKILLED:
			if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
		case MULTISHOT_MELEE_2_EXPERT:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 2;
			break;
		case MULTISHOT_MELEE_1D2_1_NOSKILL:
			*output_multishot_constant = 2;
			*output_multishot_rnd = 1;
			break;
		case MULTISHOT_MELEE_3_NOSKILL:
			*output_multishot_constant = 3;
			break;
		case MULTISHOT_MELEE_1D3_NOSKILL:
			*output_multishot_rnd = 2;
			break;
		case MULTISHOT_MELEE_2_SKILLED_1D2_BASIC:
			if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_MELEE_1D2_1_EXPERT_2_SKILLED_1D2_BASIC:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 2, * output_multishot_rnd = 1;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_MELEE_3_EXPERT_2_SKILLED_1D2_BASIC:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 3;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_BASIC)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_MELEE_4_EXPERT_3_SKILLED_2_BASIC:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 4;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 3;
			else if (skilllevel >= P_BASIC)
				*output_multishot_constant = 2;
			break;
		case MULTISHOT_MELEE_2_EXPERT_1D2_SKILLED:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 2;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_rnd = 1;
			break;
		case MULTISHOT_MELEE_3_EXPERT_2_SKILLED:
			if (skilllevel >= P_EXPERT)
				*output_multishot_constant = 3;
			else if (skilllevel >= P_SKILLED)
				*output_multishot_constant = 2;
			break;
		default:
			break;
		}
	}



	return;
}


/* common to dothrow() and dofire() */
STATIC_OVL boolean
ok_to_throw(shotlimit_p)
int *shotlimit_p; /* (see dothrow()) */
{
    /* kludge to work around parse()'s pre-decrement of `multi' */
    *shotlimit_p = (multi || save_cm) ? multi + 1 : 0;
    multi = 0; /* reset; it's been used up */

    if (notake(youmonst.data)) {
        You("are physically incapable of throwing or shooting anything.");
        return FALSE;
    } else if (nohands(youmonst.data)) {
        You_cant("throw or shoot without hands."); /* not body_part(HAND) */
        return FALSE;
        /*[what about !freehand(), aside from cursed missile launcher?]*/
    }
    if (check_capacity((char *) 0))
        return FALSE;
    return TRUE;
}

/* t command - throw */
int
dothrow()
{
    register struct obj *obj;
    int shotlimit;

    /*
     * Since some characters shoot multiple missiles at one time,
     * allow user to specify a count prefix for 'f' or 't' to limit
     * number of items thrown (to avoid possibly hitting something
     * behind target after killing it, or perhaps to conserve ammo).
     *
     * Prior to 3.3.0, command ``3t'' meant ``t(shoot) t(shoot) t(shoot)''
     * and took 3 turns.  Now it means ``t(shoot at most 3 missiles)''.
     *
     * [3.6.0:  shot count setup has been moved into ok_to_throw().]
     */
    if (!ok_to_throw(&shotlimit))
        return 0;

    obj = getobj(uslinging() ? bullets : toss_objs, "throw", 0, "");
    /* it is also possible to throw food */
    /* (or jewels, or iron balls... ) */

    return obj ? throw_obj(obj, shotlimit) : 0;
}

/* KMH -- Automatically fill quiver */
/* Suggested by Jeffrey Bay <jbay@convex.hp.com> */
static void
autoquiver()
{
    struct obj *otmp, *oammo = 0, *omissile = 0, *omisc = 0, *altammo = 0;

    if (uquiver)
        return;

    /* Scan through the inventory */
    for (otmp = invent; otmp; otmp = otmp->nobj) {
        if (otmp->owornmask || otmp->oartifact || !otmp->dknown) {
            ; /* Skip it */
        } else if (is_rock(otmp)
                   /* seen rocks or known flint or known glass */
                   || (otmp->otyp == FLINT
                       && objects[otmp->otyp].oc_name_known)
                   || (otmp->oclass == GEM_CLASS
                       && objects[otmp->otyp].oc_material == MAT_GLASS
                       && objects[otmp->otyp].oc_name_known)) {
            if (uslinging())
                oammo = otmp;
            else if (ammo_and_launcher(otmp, uswapwep))
                altammo = otmp;
            else if (!omisc)
                omisc = otmp;
        } else if (otmp->oclass == GEM_CLASS) {
            ; /* skip non-rock gems--they're ammo but
                 player has to select them explicitly */
        } else if (is_ammo(otmp)) {
            if (ammo_and_launcher(otmp, uwep))
                /* Ammo matched with launcher (bow+arrow, crossbow+bolt) */
                oammo = otmp;
            else if (ammo_and_launcher(otmp, uswapwep))
                altammo = otmp;
            else
                /* Mismatched ammo (no better than an ordinary weapon) */
                omisc = otmp;
        } else if (is_missile(otmp)) {
            /* Missile (dart, shuriken, etc.) */
            omissile = otmp;
        } else if (otmp->oclass == WEAPON_CLASS && throwing_weapon(otmp)) {
            /* Ordinary weapon */
            if (objects[otmp->otyp].oc_skill == P_DAGGER && !omissile)
                omissile = otmp;
            else
                omisc = otmp;
        }
    }

    /* Pick the best choice */
    if (oammo)
        setuqwep(oammo);
    else if (omissile)
        setuqwep(omissile);
    else if (altammo)
        setuqwep(altammo);
    else if (omisc)
        setuqwep(omisc);

    return;
}

/* f command -- fire: throw from the quiver */
int
dofire()
{
    int shotlimit;
    struct obj *obj;

    /*
     * Same as dothrow(), except we use quivered missile instead
     * of asking what to throw/shoot.
     *
     * If quiver is empty, we use autoquiver to fill it when the
     * corresponding option is on.  If the option is off or if
     * autoquiver doesn't select anything, we ask what to throw.
     * Then we put the chosen item into the quiver slot unless
     * it is already in another slot.  [Matters most if it is a
     * stack but also matters for single item if this throw gets
     * aborted (ESC at the direction prompt).  Already wielded
     * item is excluded because wielding might be necessary
     * (Mjollnir) or make the throw behave differently (aklys),
     * and alt-wielded item is excluded because switching slots
     * would end two-weapon combat even if throw gets aborted.]
     */
	if (!uwep || !is_launcher(uwep))
	{
		You("are not wielding a ranged weapon that fires ammunition.");
		return 0;
	}

	if (!ok_to_throw(&shotlimit))
        return 0;

    if ((obj = uquiver) == 0) {
        if (!flags.autoquiver) {
            You("have no ammunition readied.");
        } else {
            autoquiver();
            if ((obj = uquiver) == 0)
                You("have nothing appropriate for your quiver.");
        }
        /* if autoquiver is disabled or has failed, prompt for missile;
           fill quiver with it if it's not wielded or worn */
        if (!obj) {
            /* in case we're using ^A to repeat prior 'f' command, don't
               use direction of previous throw as getobj()'s choice here */
            in_doagain = 0;
            /* choose something from inventory, then usually quiver it */
            obj = getobj(uslinging() ? bullets : toss_objs, "throw", 0, "");
            /* Q command doesn't allow gold in quiver */
            if (obj && !obj->owornmask && obj->oclass != COIN_CLASS)
                setuqwep(obj); /* demi-autoquiver */
        }
        /* give feedback if quiver has now been filled */
        if (uquiver) {
            uquiver->owornmask &= ~W_QUIVER; /* less verbose */
            prinv("You ready:", uquiver, 0L);
            uquiver->owornmask |= W_QUIVER;
        }
    }

    return obj ? throw_obj(obj, shotlimit) : 0;
}

/* if in midst of multishot shooting/throwing, stop early */
void
endmultishot(verbose)
boolean verbose;
{
    if (m_shot.i < m_shot.n) {
        if (verbose && !context.mon_moving) {
            You("stop %s after the %d%s %s.",
                m_shot.s ? "firing" : "throwing", m_shot.i, ordin(m_shot.i),
                m_shot.s ? "shot" : "toss");
        }
        m_shot.n = m_shot.i; /* make current shot be the last */
    }
}

/* Object hits floor at hero's feet.
   Called from drop(), throwit(), hold_another_object(). */
void
hitfloor(obj, verbosely)
struct obj *obj;
boolean verbosely; /* usually True; False if caller has given drop message */
{
    if (IS_SOFT(levl[u.ux][u.uy].typ) || u.uinwater || u.uswallow) {
        dropy(obj);
        return;
    }
    if (IS_ALTAR(levl[u.ux][u.uy].typ))
        doaltarobj(obj);
    else if (verbosely)
        pline("%s %s the %s.", Doname2(obj), otense(obj, "hit"),
              surface(u.ux, u.uy));

    if (hero_breaks(obj, u.ux, u.uy, TRUE))
        return;
    if (ship_object(obj, u.ux, u.uy, FALSE))
        return;
    dropz(obj, TRUE);
}

/*
 * Walk a path from src_cc to dest_cc, calling a proc for each location
 * except the starting one.  If the proc returns FALSE, stop walking
 * and return FALSE.  If stopped early, dest_cc will be the location
 * before the failed callback.
 */
boolean
walk_path(src_cc, dest_cc, check_proc, arg)
coord *src_cc;
coord *dest_cc;
boolean FDECL((*check_proc), (genericptr_t, int, int));
genericptr_t arg;
{
    int x, y, dx, dy, x_change, y_change, err, i, prev_x, prev_y;
    boolean keep_going = TRUE;

    /* Use Bresenham's Line Algorithm to walk from src to dest.
     *
     * This should be replaced with a more versatile algorithm
     * since it handles slanted moves in a suboptimal way.
     * Going from 'x' to 'y' needs to pass through 'z', and will
     * fail if there's an obstable there, but it could choose to
     * pass through 'Z' instead if that way imposes no obstacle.
     *     ..y          .Zy
     *     xz.    vs    x..
     * Perhaps we should check both paths and accept whichever
     * one isn't blocked.  But then multiple zigs and zags could
     * potentially produce a meandering path rather than the best
     * attempt at a straight line.  And (*check_proc)() would
     * need to work more like 'travel', distinguishing between
     * testing a possible move and actually attempting that move.
     */
    dx = dest_cc->x - src_cc->x;
    dy = dest_cc->y - src_cc->y;
    prev_x = x = src_cc->x;
    prev_y = y = src_cc->y;

    if (dx < 0) {
        x_change = -1;
        dx = -dx;
    } else
        x_change = 1;
    if (dy < 0) {
        y_change = -1;
        dy = -dy;
    } else
        y_change = 1;

    i = err = 0;
    if (dx < dy) {
        while (i++ < dy) {
            prev_x = x;
            prev_y = y;
            y += y_change;
            err += dx << 1;
            if (err > dy) {
                x += x_change;
                err -= dy << 1;
            }
            /* check for early exit condition */
            if (!(keep_going = (*check_proc)(arg, x, y)))
                break;
        }
    } else {
        while (i++ < dx) {
            prev_x = x;
            prev_y = y;
            x += x_change;
            err += dy << 1;
            if (err > dx) {
                y += y_change;
                err -= dx << 1;
            }
            /* check for early exit condition */
            if (!(keep_going = (*check_proc)(arg, x, y)))
                break;
        }
    }

    if (keep_going)
        return TRUE; /* successful */

    dest_cc->x = prev_x;
    dest_cc->y = prev_y;
    return FALSE;
}

/* hack for hurtle_step() -- it ought to be changed to take an argument
   indicating lev/fly-to-dest vs lev/fly-to-dest-minus-one-land-on-dest
   vs drag-to-dest; original callers use first mode, jumping wants second,
   grappling hook backfire and thrown chained ball need third */
boolean
hurtle_jump(arg, x, y)
genericptr_t arg;
int x, y;
{
    boolean res;
    long save_EWwalking = EWwalking;

    /* prevent jumping over water from being placed in that water */
    EWwalking |= I_SPECIAL;
    res = hurtle_step(arg, x, y);
    EWwalking = save_EWwalking;
    return res;
}

/*
 * Single step for the hero flying through the air from jumping, flying,
 * etc.  Called from hurtle() and jump() via walk_path().  We expect the
 * argument to be a pointer to an integer -- the range -- which is
 * used in the calculation of points off if we hit something.
 *
 * Bumping into monsters won't cause damage but will wake them and make
 * them angry.  Auto-pickup isn't done, since you don't have control over
 * your movements at the time.
 *
 * Possible additions/changes:
 *      o really attack monster if we hit one
 *      o set stunned if we hit a wall or door
 *      o reset nomul when we stop
 *      o creepy feeling if pass through monster (if ever implemented...)
 *      o bounce off walls
 *      o let jumps go over boulders
 */
boolean
hurtle_step(arg, x, y)
genericptr_t arg;
int x, y;
{
    int ox, oy, *range = (int *) arg;
    struct obj *obj;
    struct monst *mon;
    boolean may_pass = TRUE, via_jumping, stopping_short;
    struct trap *ttmp;
    int dmg = 0;

    if (!isok(x, y)) {
        You_feel("the spirits holding you back.");
        return FALSE;
    } else if (!in_out_region(x, y)) {
        return FALSE;
    } else if (*range == 0) {
        return FALSE; /* previous step wants to stop now */
    }
    via_jumping = (EWwalking & I_SPECIAL) != 0L;
    stopping_short = (via_jumping && *range < 2);

    if (!Passes_walls || !(may_pass = may_passwall(x, y))) {
        boolean odoor_diag = (IS_DOOR(levl[x][y].typ)
                              && (levl[x][y].doormask & D_ISOPEN)
                              && (u.ux - x) && (u.uy - y));

        if (IS_ROCK(levl[x][y].typ) || closed_door(x, y) || odoor_diag) {
            const char *s;

            if (odoor_diag)
                You("hit the door edge!");
            pline("Ouch!");
            if (IS_TREE(levl[x][y].typ))
                s = "bumping into a tree";
            else if (IS_ROCK(levl[x][y].typ))
                s = "bumping into a wall";
            else
                s = "bumping into a door";
            dmg = rnd(2 + *range);
            losehp(Maybe_Half_Phys(dmg), s, KILLED_BY);
            wake_nearto(x,y, 10);
            return FALSE;
        }
        if (levl[x][y].typ == IRONBARS) {
            You("crash into some iron bars.  Ouch!");
            dmg = rnd(2 + *range);
            losehp(Maybe_Half_Phys(dmg), "crashing into iron bars",
                   KILLED_BY);
            wake_nearto(x,y, 20);
            return FALSE;
        }
        if ((obj = sobj_at(BOULDER, x, y)) != 0) {
            You("bump into a %s.  Ouch!", xname(obj));
            dmg = rnd(2 + *range);
            losehp(Maybe_Half_Phys(dmg), "bumping into a boulder", KILLED_BY);
            wake_nearto(x,y, 10);
            return FALSE;
        }
        if (!may_pass) {
            /* did we hit a no-dig non-wall position? */
            You("smack into something!");
            dmg = rnd(2 + *range);
            losehp(Maybe_Half_Phys(dmg), "touching the edge of the universe",
                   KILLED_BY);
            wake_nearto(x,y, 10);
            return FALSE;
        }
        if ((u.ux - x) && (u.uy - y) && bad_rock(youmonst.data, u.ux, y)
            && bad_rock(youmonst.data, x, u.uy)) {
            boolean too_much = (invent && (inv_weight() + weight_cap() > 600));

            /* Move at a diagonal. */
            if (bigmonst(youmonst.data) || too_much) {
                You("%sget forcefully wedged into a crevice.",
                    too_much ? "and all your belongings " : "");
                dmg = rnd(2 + *range);
                losehp(Maybe_Half_Phys(dmg), "wedging into a narrow crevice",
                       KILLED_BY);
                wake_nearto(x,y, 10);
                return FALSE;
            }
        }
    }

    if ((mon = m_at(x, y)) != 0
#if 0   /* we can't include these two exceptions unless we know we're
         * going to end up past the current spot rather than on it;
         * for that, we need to know that the range is not exhausted
         * and also that the next spot doesn't contain an obstacle */
        && !(mon->mundetected && hides_under(mon) && (Flying || Levitation))
        && !(mon->mundetected && mon->data->mlet == S_EEL
             && (Flying || Levitation || Wwalking))
#endif
        ) {
        const char *mnam, *pronoun;
        int glyph = glyph_at(x, y);

        mon->mundetected = 0; /* wakeup() will handle mimic */
        mnam = a_monnam(mon); /* after unhiding */
        pronoun = noit_mhim(mon);
        if (!strcmp(mnam, "it")) {
            mnam = !strcmp(pronoun, "it") ? "something" : "someone";
        }
        if (!glyph_is_monster(glyph) && !glyph_is_invisible(glyph))
            You("find %s by bumping into %s.", mnam, pronoun);
        else
            You("bump into %s.", mnam);
        wakeup(mon, FALSE);
        if (!canspotmon(mon))
            map_invisible(mon->mx, mon->my);
        setmangry(mon, FALSE);
        wake_nearto(x, y, 10);
        return FALSE;
    }

    if ((u.ux - x) && (u.uy - y)
        && bad_rock(youmonst.data, u.ux, y)
        && bad_rock(youmonst.data, x, u.uy)) {
        /* Move at a diagonal. */
        if (Sokoban) {
            You("come to an abrupt halt!");
            return FALSE;
        }
    }

    /* Caller has already determined that dragging the ball is allowed */
    if (Punished && uball->where == OBJ_FLOOR) {
        int bc_control;
        xchar ballx, bally, chainx, chainy;
        boolean cause_delay;

        if (drag_ball(x, y, &bc_control, &ballx, &bally, &chainx,
                      &chainy, &cause_delay, TRUE))
            move_bc(0, bc_control, ballx, bally, chainx, chainy);
    }

    ox = u.ux;
    oy = u.uy;
    u_on_newpos(x, y); /* set u.<ux,uy>, u.usteed-><mx,my>; cliparound(); */
    newsym(ox, oy);    /* update old position */
    vision_recalc(1);  /* update for new position */
    flush_screen(1);
    /* if terrain type changes, levitation or flying might become blocked
       or unblocked; might issue message, so do this after map+vision has
       been updated for new location instead of right after u_on_newpos() */
    if (levl[u.ux][u.uy].typ != levl[ox][oy].typ)
        switch_terrain();

    if (is_pool(x, y) && !u.uinwater) {
        if ((Is_waterlevel(&u.uz) && levl[x][y].typ == WATER)
            || !(Levitation || Flying || Wwalking)) {
            multi = 0; /* can move, so drown() allows crawling out of water */
            (void) drown();
            return FALSE;
        } else if (!Is_waterlevel(&u.uz) && !stopping_short) {
            Norep("You move over %s.", an(is_moat(x, y) ? "moat" : "pool"));
       }
    } else if (is_lava(x, y) && !stopping_short) {
        Norep("You move over some lava.");
    }

    /* FIXME:
     * Each trap should really trigger on the recoil if it would
     * trigger during normal movement. However, not all the possible
     * side-effects of this are tested [as of 3.4.0] so we trigger
     * those that we have tested, and offer a message for the ones
     * that we have not yet tested.
     */
    if ((ttmp = t_at(x, y)) != 0) {
        if (stopping_short) {
            ; /* see the comment above hurtle_jump() */
        } else if (ttmp->ttyp == MAGIC_PORTAL) {
            dotrap(ttmp, 0);
            return FALSE;
        } else if (ttmp->ttyp == VIBRATING_SQUARE) {
            pline("The ground vibrates as you pass it.");
            dotrap(ttmp, 0); /* doesn't print messages */
        } else if (ttmp->ttyp == FIRE_TRAP) {
            dotrap(ttmp, 0);
        } else if ((is_pit(ttmp->ttyp) || is_hole(ttmp->ttyp))
                   && Sokoban) {
            /* air currents overcome the recoil in Sokoban;
               when jumping, caller performs last step and enters trap */
            if (!via_jumping)
                dotrap(ttmp, 0);
            *range = 0;
            return TRUE;
        } else {
            if (ttmp->tseen)
                You("pass right over %s.",
                    an(defsyms[trap_to_defsym(ttmp->ttyp)].explanation));
        }
    }
    if (--*range < 0) /* make sure our range never goes negative */
        *range = 0;
    if (*range != 0)
        delay_output();
    return TRUE;
}

STATIC_OVL boolean
mhurtle_step(arg, x, y)
genericptr_t arg;
int x, y;
{
    struct monst *mon = (struct monst *) arg;

    /* TODO: Treat walls, doors, iron bars, pools, lava, etc. specially
     * rather than just stopping before.
     */
    if (goodpos(x, y, mon, 0) && m_in_out_region(mon, x, y)) {
        remove_monster(mon->mx, mon->my);
        newsym(mon->mx, mon->my);
        place_monster(mon, x, y);
        newsym(mon->mx, mon->my);
        set_apparxy(mon);
        (void) mintrap(mon);
        return TRUE;
    }
    return FALSE;
}

/*
 * The player moves through the air for a few squares as a result of
 * throwing or kicking something.
 *
 * dx and dy should be the direction of the hurtle, not of the original
 * kick or throw and be only.
 */
void
hurtle(dx, dy, range, verbose)
int dx, dy, range;
boolean verbose;
{
    coord uc, cc;

    /* The chain is stretched vertically, so you shouldn't be able to move
     * very far diagonally.  The premise that you should be able to move one
     * spot leads to calculations that allow you to only move one spot away
     * from the ball, if you are levitating over the ball, or one spot
     * towards the ball, if you are at the end of the chain.  Rather than
     * bother with all of that, assume that there is no slack in the chain
     * for diagonal movement, give the player a message and return.
     */
    if (Punished && !carried(uball)) {
        You_feel("a tug from the iron ball.");
        nomul(0);
        return;
    } else if (u.utrap) {
        You("are anchored by the %s.",
            u.utraptype == TT_WEB
                ? "web"
                : u.utraptype == TT_LAVA
                      ? hliquid("lava")
                      : u.utraptype == TT_INFLOOR
                            ? surface(u.ux, u.uy)
                            : u.utraptype == TT_BURIEDBALL ? "buried ball"
                                                           : "trap");
        nomul(0);
        return;
    }

    /* make sure dx and dy are [-1,0,1] */
    dx = sgn(dx);
    dy = sgn(dy);

    if (!range || (!dx && !dy) || u.ustuck)
        return; /* paranoia */

    nomul(-range);
    multi_reason = "moving through the air";
    nomovemsg = ""; /* it just happens */
    if (verbose)
        You("%s in the opposite direction.", range > 1 ? "hurtle" : "float");
    /* if we're in the midst of shooting multiple projectiles, stop */
    endmultishot(TRUE);
    sokoban_guilt();
    uc.x = u.ux;
    uc.y = u.uy;
    /* this setting of cc is only correct if dx and dy are [-1,0,1] only */
    cc.x = u.ux + (dx * range);
    cc.y = u.uy + (dy * range);
    (void) walk_path(&uc, &cc, hurtle_step, (genericptr_t) &range);
}

/* Move a monster through the air for a few squares. */
void
mhurtle(mon, dx, dy, range)
struct monst *mon;
int dx, dy, range;
{
    coord mc, cc;

    /* At the very least, debilitate the monster */
    mon->movement = 0;
	increase_mon_property(mon, STUNNED, 5 + rnd(10));

    /* Is the monster stuck or too heavy to push?
     * (very large monsters have too much inertia, even floaters and flyers)
     */
    if (mon->data->msize >= MZ_HUGE || mon == u.ustuck || mon->mtrapped)
        return;

    /* Make sure dx and dy are [-1,0,1] */
    dx = sgn(dx);
    dy = sgn(dy);
    if (!range || (!dx && !dy))
        return; /* paranoia */
    /* don't let grid bugs be hurtled diagonally */
    if (dx && dy && NODIAG(monsndx(mon->data)))
        return;

    /* Send the monster along the path */
    mc.x = mon->mx;
    mc.y = mon->my;
    cc.x = mon->mx + (dx * range);
    cc.y = mon->my + (dy * range);
    (void) walk_path(&mc, &cc, mhurtle_step, (genericptr_t) mon);
    return;
}

void
check_shop_obj(obj, x, y, broken)
struct obj *obj;
xchar x, y;
boolean broken;
{
    boolean costly_xy;
    struct monst *shkp = shop_keeper(*u.ushops);

    if (!shkp)
        return;

    costly_xy = costly_spot(x, y);
    if (broken || !costly_xy || *in_rooms(x, y, SHOPBASE) != *u.ushops) {
        /* thrown out of a shop or into a different shop */
        if (is_unpaid(obj))
            (void) stolen_value(obj, u.ux, u.uy, is_peaceful(shkp),
                                FALSE);
        if (broken)
            obj->no_charge = 1;
    } else if (costly_xy) {
        char *oshops = in_rooms(x, y, SHOPBASE);

        /* ushops0: in case we threw while levitating and recoiled
           out of shop (most likely to the shk's spot in front of door) */
        if (*oshops == *u.ushops || *oshops == *u.ushops0) {
            if (is_unpaid(obj))
                subfrombill(obj, shkp);
            else if (x != shkp->mx || y != shkp->my)
                sellobj(obj, x, y);
        }
    }
}

/*
 * Hero tosses an object upwards with appropriate consequences.
 *
 * Returns FALSE if the object is gone.
 */
STATIC_OVL boolean
toss_up(obj, hitsroof)
struct obj *obj;
boolean hitsroof;
{
    const char *action;
    boolean petrifier = ((obj->otyp == EGG || obj->otyp == CORPSE)
                         && touch_petrifies(&mons[obj->corpsenm]));
    /* note: obj->quan == 1 */

    if (!has_ceiling(&u.uz)) {
        action = "flies up into"; /* into "the sky" or "the water above" */
    } else if (hitsroof) {
        if (breaktest(obj)) {
            pline("%s hits the %s.", Doname2(obj), ceiling(u.ux, u.uy));
            breakmsg(obj, !Blind);
            breakobj(obj, u.ux, u.uy, TRUE, TRUE);
            return FALSE;
        }
        action = "hits";
    } else {
        action = "almost hits";
    }
    pline("%s %s the %s, then falls back on top of your %s.", Doname2(obj),
          action, ceiling(u.ux, u.uy), body_part(HEAD));

    /* object now hits you */

    if (obj->oclass == POTION_CLASS) {
        potionhit(&youmonst, obj, POTHIT_HERO_THROW);
    } else if (breaktest(obj)) {
        int otyp = obj->otyp;
        int blindinc;

        /* need to check for blindness result prior to destroying obj */
        blindinc = ((otyp == CREAM_PIE || otyp == BLINDING_VENOM)
                    /* AT_WEAP is ok here even if attack type was AT_SPIT */
                    && can_blnd(&youmonst, &youmonst, AT_WEAP, obj))
                       ? rnd(25)
                       : 0;
        breakmsg(obj, !Blind);
        breakobj(obj, u.ux, u.uy, TRUE, TRUE);
        obj = 0; /* it's now gone */
        switch (otyp) {
        case EGG:
            if (petrifier && !Stone_resistance
                && !(poly_when_stoned(youmonst.data)
                     && polymon(PM_STONE_GOLEM))) {
                /* egg ends up "all over your face"; perhaps
                   visored helmet should still save you here */
                if (uarmh)
                    Your("%s fails to protect you.", helm_simple_name(uarmh));
                goto petrify;
            }
            /*FALLTHRU*/
        case CREAM_PIE:
        case BLINDING_VENOM:
            pline("You've got it all over your %s!", body_part(FACE));
            if (blindinc) {
                if (otyp == BLINDING_VENOM && !Blind)
                    pline("It blinds you!");
                u.ucreamed += blindinc;
                make_blinded(Blinded + (long) blindinc, FALSE);
                if (!Blind)
                    Your1(vision_clears);
            }
            break;
        default:
            break;
        }
        return FALSE;
    } else { /* neither potion nor other breaking object */
        boolean less_damage = uarmh && is_metallic(uarmh), artimsg = FALSE;
        int dmg = is_launcher(obj) ? d(1, 2) : weapon_total_dmg_value(obj, &youmonst, &youmonst);

        if (obj->oartifact)
            /* need a fake die roll here; rn1(18,2) avoids 1 and 20 */
            artimsg = artifact_hit((struct monst *) 0, &youmonst, obj, &dmg,
                                   rn1(18, 2));

        if (!dmg) { /* probably wasn't a weapon; base damage on weight */
            dmg = (int) obj->owt / 100;
            if (dmg < 1)
                dmg = 1;
            else if (dmg > 6)
                dmg = 6;
            if (youmonst.data == &mons[PM_SHADE]
                && objects[obj->otyp].oc_material != MAT_SILVER)
                dmg = 0;
        }
        if (dmg > 1 && less_damage)
            dmg = 1;
        if (dmg > 0)
            dmg += u.ubasedaminc + u.udaminc;
        if (dmg < 0)
            dmg = 0; /* beware negative rings of increase damage */
        dmg = Maybe_Half_Phys(dmg);

        if (uarmh) {
            if (less_damage && dmg < (Upolyd ? u.mh : u.uhp)) {
                if (!artimsg)
                    pline("Fortunately, you are wearing a hard helmet.");
                /* helmet definitely protects you when it blocks petrification
                 */
            } else if (!petrifier) {
                if (flags.verbose)
                    Your("%s does not protect you.", helm_simple_name(uarmh));
            }
        } else if (petrifier && !Stone_resistance
                   && !(poly_when_stoned(youmonst.data)
                        && polymon(PM_STONE_GOLEM))) {
 petrify:
            killer.format = KILLED_BY;
            Strcpy(killer.name, "elementary physics"); /* "what goes up..." */
            You("turn to stone.");
            if (obj)
                dropy(obj); /* bypass most of hitfloor() */
            thrownobj = 0;  /* now either gone or on floor */
            done(STONING);
            return obj ? TRUE : FALSE;
        }
        hitfloor(obj, TRUE);
        thrownobj = 0;
        losehp(Maybe_Half_Phys(dmg), "falling object", KILLED_BY_AN);
    }
    return TRUE;
}

/* return true for weapon meant to be thrown; excludes ammo */
boolean
throwing_weapon(obj)
struct obj *obj;
{
    return (boolean) (nonmelee_throwing_weapon(obj) || (objects[obj->otyp].oc_flags & O1_MELEE_AND_THROWN_WEAPON));
}

/* return true for weapon meant to be only thrown and cannot be used in melee */
boolean
nonmelee_throwing_weapon(obj)
struct obj* obj;
{
	return (boolean)(is_missile(obj) || (objects[obj->otyp].oc_flags & O1_THROWN_WEAPON_ONLY));
}


/* the currently thrown object is returning to you (not for boomerangs) */
STATIC_OVL void
sho_obj_return_to_u(obj)
struct obj *obj;
{
    /* might already be our location (bounced off a wall) */
    if ((u.dx || u.dy) && (bhitpos.x != u.ux || bhitpos.y != u.uy)) {
        int x = bhitpos.x - u.dx, y = bhitpos.y - u.dy;

        tmp_at(DISP_FLASH, obj_to_glyph(obj, rn2_on_display_rng));
        while (isok(x,y) && (x != u.ux || y != u.uy)) {
            tmp_at(x, y);
            delay_output();
            x -= u.dx;
            y -= u.dy;
        }
        tmp_at(DISP_END, 0);
    }
}

/* throw an object, NB: obj may be consumed in the process */
void
throwit(obj, wep_mask)
struct obj *obj;
long wep_mask; /* used to re-equip returning boomerang / aklys / Mjollnir / Javelin of Returning */
{
    register struct monst *mon;
    register int range, urange;
    boolean impaired = (Confusion || Stunned || Blind
                                     || Hallucination || Fumbling);
    boolean tethered_weapon = (obj->otyp == AKLYS && (wep_mask & W_WIELDED_WEAPON) != 0);

    notonhead = FALSE; /* reset potentially stale value */
    if ((obj->cursed || obj->greased) && (u.dx || u.dy) && !rn2(7)) {
        boolean slipok = TRUE;

        if (ammo_and_launcher(obj, uwep)) {
            pline("%s!", Tobjnam(obj, "misfire"));
        } else {
            /* only slip if it's greased or meant to be thrown */
            if (obj->greased || throwing_weapon(obj))
                /* BUG: this message is grammatically incorrect if obj has
                   a plural name; greased gloves or boots for instance. */
                pline("%s as you throw it!", Tobjnam(obj, "slip"));
            else
                slipok = FALSE;
        }
        if (slipok) {
            u.dx = rn2(3) - 1;
            u.dy = rn2(3) - 1;
            if (!u.dx && !u.dy)
                u.dz = 1;
            impaired = TRUE;
        }
    }

    if ((u.dx || u.dy || (u.dz < 1))
        && calc_capacity((int) obj->owt) > SLT_ENCUMBER
        && (Upolyd ? (u.mh < 5 && u.mh != u.mhmax)
                   : (u.uhp < 10 && u.uhp != u.uhpmax))
        && obj->owt > (unsigned) ((Upolyd ? u.mh : u.uhp) * 2)
        && !Is_airlevel(&u.uz)) {
        You("have so little stamina, %s drops from your grasp.",
            the(xname(obj)));
        exercise(A_CON, FALSE);
        u.dx = u.dy = 0;
        u.dz = 1;
    }

    thrownobj = obj;
    thrownobj->was_thrown = 1;

    if (u.uswallow) {
        mon = u.ustuck;
        bhitpos.x = mon->mx;
        bhitpos.y = mon->my;
        if (tethered_weapon)
            tmp_at(DISP_TETHER, obj_to_glyph(obj, rn2_on_display_rng));
    } else if (u.dz) {
        if (u.dz < 0
            /* Mjollnir must we wielded to be thrown--caller verifies this;
               aklys must we wielded as primary to return when thrown */
            && (((objects[obj->otyp].oc_flags & O1_RETURNS_TO_HAND_AFTER_THROWING) && !((objects[obj->otyp].oc_flags & O1_CAN_BE_THROWN_ONLY_IF_WIELDED) && 1)  && !inappropriate_character_type(obj))
                || tethered_weapon)
            && !impaired)
		{
			if(wep_mask & W_WIELDED_WEAPON)
				pline("%s the %s and returns to your %s!", Tobjnam(obj, "hit"),
					  ceiling(u.ux, u.uy), body_part(HAND));
			else
				pline("%s the %s and returns to you!", Tobjnam(obj, "hit"),
					ceiling(u.ux, u.uy));

            obj = addinv(obj);
            (void) encumber_msg();
            if (obj->owornmask & W_QUIVER) /* in case addinv() autoquivered */
                setuqwep((struct obj *) 0);
            if(wep_mask)
				setuwep(obj, wep_mask);
		} else if (u.dz < 0) {
            (void) toss_up(obj, rn2(5) && !Underwater);
        } else if (u.dz > 0 && u.usteed && obj->oclass == POTION_CLASS
                   && rn2(6)) {
            /* alternative to prayer or wand of opening/spell of knock
               for dealing with cursed saddle:  throw holy water > */
            potionhit(u.usteed, obj, POTHIT_HERO_THROW);
        } else {
            hitfloor(obj, TRUE);
        }
        thrownobj = (struct obj *) 0;
        return;

    } else if (obj->otyp == BOOMERANG && !Underwater) {
        if (Is_airlevel(&u.uz) || Levitation)
            hurtle(-u.dx, -u.dy, 1, TRUE);
        mon = boomhit(obj, u.dx, u.dy);
        if (mon == &youmonst) { /* the thing was caught */
            exercise(A_DEX, TRUE);
            obj = addinv(obj);
            (void) encumber_msg();
            if (wep_mask && !(obj->owornmask & wep_mask)) {
                setworn(obj, wep_mask);
            }
            thrownobj = (struct obj *) 0;
            return;
        }
    } else {
		/* urange your moving in a weightless levitation situation */
		if (is_ammo(obj) && uwep && ammo_and_launcher(obj, uwep))
			range = weapon_range(obj, uwep);
		else
			range = weapon_range(obj, (struct obj*)0);

		urange = 0;
        if (Is_airlevel(&u.uz) || Levitation) {
            /* action, reaction... */
			if(youmonst.data->cwt)
	            urange = min(BOLT_LIM, range * obj->owt / youmonst.data->cwt);
			else
				urange = min(BOLT_LIM, range * obj->owt / (16 * 2 * 80)); //about 160 lbs = about 80 kg

            if (urange < 1)
                urange = 1;
            range -= urange;
            if (range < 1)
                range = 1;
        }

        if (Underwater)
            range = 1;

        mon = bhit(u.dx, u.dy, range, 0,
                   tethered_weapon ? THROWN_TETHERED_WEAPON : THROWN_WEAPON,
                   (int FDECL((*), (MONST_P, OBJ_P))) 0,
                   (int FDECL((*), (OBJ_P, OBJ_P))) 0, &obj, TRUE, FALSE);
        thrownobj = obj; /* obj may be null now */

        /* have to do this after bhit() so u.ux & u.uy are correct */
        if (Is_airlevel(&u.uz) || Levitation)
            hurtle(-u.dx, -u.dy, urange, TRUE);

        if (!obj) {
            /* bhit display cleanup was left with this caller
               for tethered_weapon, but clean it up now since
               we're about to return */
            if (tethered_weapon)
                tmp_at(DISP_END, 0);
            return;
        }
    }

    if (mon) {
        boolean obj_gone;

        if (mon->isshk && obj->where == OBJ_MINVENT && obj->ocarry == mon) {
            thrownobj = (struct obj *) 0;
            return; /* alert shk caught it */
        }
        (void) snuff_candle(obj);
        notonhead = (bhitpos.x != mon->mx || bhitpos.y != mon->my);
        obj_gone = thitmonst(mon, obj, FALSE);
		if (mon && DEADMONSTER(mon))
			context.multishot_target_killed = TRUE;

        /* Monster may have been tamed; this frees old mon */
        mon = m_at(bhitpos.x, bhitpos.y);

        /* [perhaps this should be moved into thitmonst or hmon] */
        if (mon && mon->isshk
            && (!inside_shop(u.ux, u.uy)
                || !index(in_rooms(mon->mx, mon->my, SHOPBASE), *u.ushops)))
            hot_pursuit(mon);

        if (obj_gone)
            thrownobj = 0;
    }

    if (!thrownobj) {
        /* missile has already been handled */
        if (tethered_weapon) tmp_at(DISP_END, 0);
    } else if (u.uswallow) {
        if (tethered_weapon) {
            tmp_at(DISP_END, 0);
            pline("%s returns to your %s!", The(xname(thrownobj)), body_part(HAND));
            thrownobj = addinv(thrownobj);
            (void) encumber_msg();
            /* in case addinv() autoquivered */
            if (thrownobj->owornmask & W_QUIVER)
                setuqwep((struct obj *) 0);
            setuwep(thrownobj, W_WEP);
        } else {
            /* ball is not picked up by monster */
            if (obj != uball)
                (void) mpickobj(u.ustuck, obj);
            thrownobj = (struct obj *) 0;
        }
    } else {
        /* Mjollnir must we wielded to be thrown--caller verifies this;
           aklys must we wielded as primary to return when thrown */
        if (// (obj->oartifact == ART_MJOLLNIR && Role_if(PM_VALKYRIE))
            tethered_weapon || ((objects[obj->otyp].oc_flags & O1_RETURNS_TO_HAND_AFTER_THROWING) && !inappropriate_character_type(obj))) {
//            if (rn2(100)) {
                if (tethered_weapon)
                    tmp_at(DISP_END, BACKTRACK);
                else
                    sho_obj_return_to_u(obj); /* display its flight */

                if (!impaired) // && rn2(100))
				{
					/* if uwep, more things need to be done than otherwise */
					if(wep_mask & W_WIELDED_WEAPON) // (objects[obj->otyp].oc_flags& O1_CAN_BE_THROWN_ONLY_IF_WIELDED) ||
					{
						pline("%s to your %s!", Tobjnam(obj, "return"), body_part(HAND));
						obj = addinv(obj);
						(void) encumber_msg();
						/* addinv autoquivers an aklys if quiver is empty;
						   if obj is quivered, remove it before wielding */
						if (obj->owornmask & W_QUIVER)
							setuqwep((struct obj *) 0);
						setuwep(obj, wep_mask);
					}
					else
					{
						pline("%s to you!", Tobjnam(obj, "return"));
						obj = addinv(obj);
						(void)encumber_msg();
						/* addinv autoquivers an aklys if quiver is empty;
						   if obj is quivered, remove it before wielding */
						if ((obj->owornmask & W_QUIVER) && !(wep_mask & W_QUIVER))
							setuqwep((struct obj*) 0);

						/* Wields if necessary */
						if ((wep_mask & W_SWAPWEP))
							setuswapwep(obj, W_SWAPWEP);
						else if ((wep_mask & W_SWAPWEP2))
							setuswapwep(obj, W_SWAPWEP2);
						else if ((wep_mask & W_WEP))
							setuwep(obj, W_WEP);
						else if ((wep_mask & W_WEP2))
							setuwep(obj, W_WEP2);
					}
                    if (cansee(bhitpos.x, bhitpos.y))
                        newsym(bhitpos.x, bhitpos.y);
                } else {
                    int dmg = rn2(2);

                    if (!dmg) {
                        pline(Blind ? "%s lands %s your %s."
                                    : "%s back to you, landing %s your %s.",
                              Blind ? Something : Tobjnam(obj, "return"),
                              Levitation ? "beneath" : "at",
                              makeplural(body_part(FOOT)));
                    } else {
                        dmg += rnd(3);
                        pline(Blind ? "%s your %s!"
                                    : "%s back toward you, hitting your %s!",
                              Tobjnam(obj, Blind ? "hit" : "fly"),
                              body_part(ARM));
                        if (obj->oartifact)
                            (void) artifact_hit((struct monst *) 0, &youmonst,
                                                obj, &dmg, 0);
                        losehp(Maybe_Half_Phys(dmg), killer_xname(obj),
                               KILLED_BY);
                    }
                    if (ship_object(obj, u.ux, u.uy, FALSE)) {
                        thrownobj = (struct obj *) 0;
                        return;
                    }
                    dropy(obj);
                }
                thrownobj = (struct obj *) 0;
                return;
 //           } else {
 //               if (tethered_weapon)
 //                   tmp_at(DISP_END, 0);
                /* when this location is stepped on, the weapon will be
                   auto-picked up due to 'obj->was_thrown' of 1;
                   addinv() prevents thrown Mjollnir from being placed
                   into the quiver slot, but an aklys will end up there if
                   that slot is empty at the time; since hero will need to
                   explicitly rewield the weapon to get throw-and-return
                   capability back anyway, quivered or not shouldn't matter */
//                pline("%s to return!", Tobjnam(obj, "fail"));
                /* continue below with placing 'obj' at target location */
//            }
        }

        if ((!IS_SOFT(levl[bhitpos.x][bhitpos.y].typ) && breaktest(obj))
            /* venom [via #monster to spit while poly'd] fails breaktest()
               but we want to force breakage even when location IS_SOFT() */
            || obj->oclass == VENOM_CLASS) {
            tmp_at(DISP_FLASH, obj_to_glyph(obj, rn2_on_display_rng));
            tmp_at(bhitpos.x, bhitpos.y);
            delay_output();
            tmp_at(DISP_END, 0);
            breakmsg(obj, cansee(bhitpos.x, bhitpos.y));
            breakobj(obj, bhitpos.x, bhitpos.y, TRUE, TRUE);
            thrownobj = (struct obj *) 0;
            return;
        }
        if (flooreffects(obj, bhitpos.x, bhitpos.y, "fall")) {
            thrownobj = (struct obj *) 0;
            return;
        }
        obj_no_longer_held(obj);
        if (mon && mon->isshk && is_pick(obj)) {
            if (cansee(bhitpos.x, bhitpos.y))
                pline("%s snatches up %s.", Monnam(mon), the(xname(obj)));
            if (*u.ushops || obj->unpaid)
                check_shop_obj(obj, bhitpos.x, bhitpos.y, FALSE);
            (void) mpickobj(mon, obj); /* may merge and free obj */
            thrownobj = (struct obj *) 0;
            return;
        }
        (void) snuff_candle(obj);
        if (!mon && ship_object(obj, bhitpos.x, bhitpos.y, FALSE)) {
            thrownobj = (struct obj *) 0;
            return;
        }
        thrownobj = (struct obj *) 0;
        place_object(obj, bhitpos.x, bhitpos.y);
        /* container contents might break;
           do so before turning ownership of thrownobj over to shk
           (container_impact_dmg handles item already owned by shop) */
        if (!IS_SOFT(levl[bhitpos.x][bhitpos.y].typ))
            /* <x,y> is spot where you initiated throw, not bhitpos */
            container_impact_dmg(obj, u.ux, u.uy);
        /* charge for items thrown out of shop;
           shk takes possession for items thrown into one */
        if ((*u.ushops || obj->unpaid) && obj != uball)
            check_shop_obj(obj, bhitpos.x, bhitpos.y, FALSE);

        stackobj(obj);
        if (obj == uball)
            drop_ball(bhitpos.x, bhitpos.y);
        if (cansee(bhitpos.x, bhitpos.y))
            newsym(bhitpos.x, bhitpos.y);
        if (obj_sheds_light(obj))
            vision_full_recalc = 1;
    }
}

/* an object may hit a monster; various factors adjust chance of hitting */
int
omon_adj(mon, obj, mon_notices)
struct monst *mon;
struct obj *obj;
boolean mon_notices;
{
    int tmp = 0;

    /* size of target affects the chance of hitting */
    tmp += (mon->data->msize - MZ_MEDIUM); /* -2..+5 */
    /* sleeping target is more likely to be hit */
    if (mon->msleeping) {
        tmp += 2;
        if (mon_notices)
            mon->msleeping = 0;
    }
    /* ditto for immobilized target */
    if (!mon->mcanmove || !mon->data->mmove) {
        tmp += 4;
        if (mon_notices && mon->data->mmove && !rn2(10)) {
            mon->mcanmove = 1;
            mon->mfrozen = 0;
        }
    }

    /* some objects are more likely to hit than others */
    switch (obj->otyp) {
    case HEAVY_IRON_BALL:
        if (obj != uball)
            tmp += 2;
        break;
    case BOULDER:
        tmp += 6;
        break;
    default:
        if (obj->oclass == WEAPON_CLASS || is_weptool(obj)
            || obj->oclass == GEM_CLASS)
            tmp += weapon_to_hit_value(obj, mon, (struct monst *)0);
        break;
    }
    return tmp;
}

/* thrown object misses target monster */
STATIC_OVL void
tmiss(obj, mon, maybe_wakeup)
struct obj *obj;
struct monst *mon;
boolean maybe_wakeup;
{
    const char *missile = mshot_xname(obj);

    /* If the target can't be seen or doesn't look like a valid target,
       avoid "the arrow misses it," or worse, "the arrows misses the mimic."
       An attentive player will still notice that this is different from
       an arrow just landing short of any target (no message in that case),
       so will realize that there is a valid target here anyway. */
    if (!canseemon(mon) || (M_AP_TYPE(mon) && M_AP_TYPE(mon) != M_AP_MONSTER))
        pline("%s %s.", The(missile), otense(obj, "miss"));
    else
        miss(missile, mon);
    if (maybe_wakeup && !rn2(3))
        wakeup(mon, TRUE);
    return;
}

#define quest_arti_hits_leader(obj, mon)      \
    (obj->oartifact && is_quest_artifact(obj) \
     && mon->m_id == quest_status.leader_m_id)

/*
 * Object thrown by player arrives at monster's location.
 * Return 1 if obj has disappeared or otherwise been taken care of,
 * 0 if caller must take care of it.
 * Also used for kicked objects and for polearms/grapnel applied at range.
 */
int
thitmonst(mon, obj, is_golf)
register struct monst *mon;
register struct obj *obj; /* thrownobj or kickedobj or uwep */
register boolean is_golf;
{
	if (!mon || !obj)
		return 0;

    register int tmp;     /* Base chance to hit */
    register int disttmp; /* distance modifier */
    int otyp = obj->otyp, hmode;
    boolean guaranteed_hit = (u.uswallow && mon == u.ustuck);
    int dieroll;

    hmode = (obj == uwep) ? HMON_APPLIED
              : (obj == kickedobj) ? (is_golf ? HMON_GOLF : HMON_KICKED)
                : HMON_THROWN;

    /* Differences from melee weapons:
     *
     * Dex still gives a bonus, but strength does not.
     * Polymorphed players lacking attacks may still throw.
     * There's a base -1 to hit.
     * No bonuses for fleeing or stunned targets (they don't dodge
     *    melee blows as readily, but dodging arrows is hard anyway).
     * Not affected by traps, etc.
     * Certain items which don't in themselves do damage ignore 'tmp'.
     * Distance and monster size affect chance to hit.
     */
    tmp = -1 + Luck + u_ranged_strdex_to_hit_bonus() + find_mac(mon) + u.ubasehitinc + u.uhitinc
          + maybe_polyd(youmonst.data->mlevel, u.ulevel);

    /* Modify to-hit depending on distance; but keep it sane.
     * Polearms get a distance penalty even when wielded; it's
     * hard to hit at a distance.
     */
	int mindistance = distmin(u.ux, u.uy, mon->mx, mon->my);
    disttmp = 2 - mindistance / 3;
    if (disttmp < -4)
        disttmp = -4;
    tmp += disttmp;

		//Bows have point black penalty
		//OTHER BONUSES FROM BOW ARE GIVEN BELOW, THIS IS FOR POINT BLACK RANGE ONLY
	if (hmode == HMON_THROWN) 
	{
		if (obj && uwep && mon && ammo_and_launcher(obj, uwep))
		{
			if (mindistance <= 1) 
			{
				switch (objects[uwep->otyp].oc_skill) 
				{
				case P_BOW:
					tmp -= 20;
					break;
				case P_CROSSBOW:
					tmp -= 16;
					break;
				default:
					tmp -= 20;
					break;
				}
			}
			//Bracers bonus
			if(uarmb && uarmb->otyp == BRACERS_OF_ARCHERY)
				tmp += (uarmb->cursed ? -2 : 2) + (uarmb->blessed ? 1 : 0) + uarmb->spe;
		}
		else
		{
			tmp -= 0;
		}
	}

	//Bonus from weapon_to_hit_value(obj) and other if monster is still etc.
    tmp += omon_adj(mon, obj, TRUE);

	//Elfs get a bonus
    if (is_orc(mon->data)
        && maybe_polyd(is_elf(youmonst.data), Race_if(PM_ELF)))
        tmp++;

	//Guaranteed hit
    if (guaranteed_hit) 
	{
        tmp += 1000; /* Guaranteed hit */
    }

    if (obj->oclass == GEM_CLASS && is_unicorn(mon->data)) 
	{
        if (!mon_can_move(mon))
		{
            tmiss(obj, mon, FALSE);
            return 0;
        } 
		else if (is_tame(mon)) 
		{
            pline("%s catches and drops %s.", Monnam(mon), the(xname(obj)));
            return 0;
        } 
		else 
		{
            pline("%s catches %s.", Monnam(mon), the(xname(obj)));
            return gem_accept(mon, obj);
        }
    }

    /* don't make game unwinnable if naive player throws artifact
       at leader... (kicked artifact is ok too; HMON_APPLIED could
       occur if quest artifact polearm or grapnel ever gets added) */
    if (hmode != HMON_APPLIED && quest_arti_hits_leader(obj, mon)) 
	{
        /* AIS: changes to wakeup() means that it's now less inappropriate here
           than it used to be, but the manual version works just as well */
        mon->msleeping = 0;
        mon->mstrategy &= ~STRAT_WAITMASK;

        if (mon_can_move(mon)) 
		{
            pline("%s catches %s.", Monnam(mon), the(xname(obj)));
            if (is_peaceful(mon))
			{
                boolean next2u = monnear(mon, u.ux, u.uy);

                finish_quest(obj); /* acknowledge quest completion */
                pline("%s %s %s back to you.", Monnam(mon),
                      (next2u ? "hands" : "tosses"), the(xname(obj)));
                if (!next2u)
                    sho_obj_return_to_u(obj);
                obj = addinv(obj); /* back into your inventory */
                (void) encumber_msg();
            } 
			else 
			{
                /* angry leader caught it and isn't returning it */
                if (*u.ushops || obj->unpaid) /* not very likely... */
                    check_shop_obj(obj, mon->mx, mon->my, FALSE);
                (void) mpickobj(mon, obj);
            }
            return 1; /* caller doesn't need to place it */
        }
        return 0;
    }

    dieroll = rnd(20);

	boolean is_golf_swing_with_stone = (hmode == HMON_GOLF && (obj->oclass == GEM_CLASS || objects[obj->otyp].oc_skill == -P_SLING));

    if (obj->oclass == WEAPON_CLASS || is_weptool(obj) || obj->oclass == GEM_CLASS) 
	{
        if (hmode == HMON_KICKED)
		{
            /* throwing adjustments and weapon skill bonus don't apply */
            tmp -= (is_ammo(obj) ? 5 : 3);
        } 
		else if (is_ammo(obj) || is_golf_swing_with_stone)
		{
            if (!ammo_and_launcher(obj, uwep) && !is_golf_swing_with_stone)
			{
                tmp -= 4;
            } 
			else if (uwep)
			{
				tmp += weapon_to_hit_value(uwep, mon, &youmonst);	//tmp += uwep->spe - greatest_erosion(uwep);
                tmp += weapon_skill_hit_bonus(uwep, is_golf_swing_with_stone ? P_THROWN_WEAPON : P_NONE); //Players get skill bonuses
//                if (uwep->oartifact)
//                    tmp += spec_abon(uwep, mon);
                /*
                 * Elves and Samurais are highly trained w/bows,
                 * especially their own special types of bow.
                 * Polymorphing won't make you a bow expert.
                 */
                if ((Race_if(PM_ELF) || Role_if(PM_SAMURAI))
                    && (!Upolyd || your_race(youmonst.data))
                    && objects[uwep->otyp].oc_skill == P_BOW) 
				{
                    tmp++;
                    if (Race_if(PM_ELF) && uwep->otyp == ELVEN_LONG_BOW)
                        tmp++;
                    else if (Role_if(PM_SAMURAI) && uwep->otyp == YUMI)
                        tmp++;
                }
            }
        } 
		else 
		{ /* thrown non-ammo or applied polearm/grapnel */
            if (throwing_weapon(obj)) /* meant to be thrown */
                tmp += 2;
            else if (obj == thrownobj) /* not meant to be thrown */
                tmp -= 2;
            /* we know we're dealing with a weapon or weptool handled
               by WEAPON_SKILLS once ammo objects have been excluded */
            tmp += weapon_skill_hit_bonus(obj, is_golf_swing_with_stone ? P_THROWN_WEAPON : P_NONE);
        }

        if (tmp >= dieroll) 
		{
            boolean wasthrown = (thrownobj != 0),
                    /* remember weapon attribute; hmon() might destroy obj */
                    chopper = is_axe(obj);

            /* attack hits mon */
            if (hmode == HMON_APPLIED)
                u.uconduct.weaphit++;

			//DAMAGE IS DONE HERE
			boolean obj_destroyed = FALSE;
            if (hmon(mon, obj, hmode, dieroll, &obj_destroyed)) 
			{ /* mon still alive */
                if (mon->wormno)
                    cutworm(mon, bhitpos.x, bhitpos.y, chopper);
            }
			if (obj_destroyed)
			{
				obj = 0;
				return 1;
			}
            exercise(A_DEX, TRUE);
            /* if hero was swallowed and projectile killed the engulfer,
               'obj' got added to engulfer's inventory and then dropped,
               so we can't safely use that pointer anymore; it escapes
               the chance to be used up here... */
            if (wasthrown && !thrownobj)
                return 1;

            /* projectiles other than magic stones sometimes disappear
               when thrown; projectiles aren't among the types of weapon
               that hmon() might have destroyed so obj is intact */
            if (objects[otyp].oc_skill < P_NONE
                && objects[otyp].oc_skill >= -P_THROWN_WEAPON
                && !objects[otyp].oc_magic) 
			{
                /* we were breaking 2/3 of everything unconditionally.
                 * we still don't want anything to survive unconditionally,
                 * but we need ammo to stay around longer on average.
                 */
                int broken, chance;

                chance = 0 + greatest_erosion(obj) - obj->spe;
                if (chance > 1)
                    broken = rn2(chance);
                else
                    broken = !rn2(20);

                if (obj->blessed)
                    broken = 0;

                if (broken) 
				{
                    if (*u.ushops || obj->unpaid)
                        check_shop_obj(obj, bhitpos.x, bhitpos.y, TRUE);
                    obfree(obj, (struct obj *) 0);
                    return 1;
                }
            }
            passive_obj(mon, obj, (struct attack *) 0);
        } 
		else
		{
            tmiss(obj, mon, TRUE);
            if (hmode == HMON_APPLIED)
                wakeup(mon, TRUE);
        }

    } 
	else if (otyp == HEAVY_IRON_BALL) 
	{
        exercise(A_STR, TRUE);
        if (tmp >= dieroll)
		{
            int was_swallowed = guaranteed_hit;

            exercise(A_DEX, TRUE);
			boolean obj_destroyed = FALSE;
			boolean malive = hmon(mon, obj, hmode, dieroll, &obj_destroyed);
			if (obj_destroyed)
				obj = 0;
			if (!malive)
			{ /* mon killed */

                if (was_swallowed && !u.uswallow && obj == uball)
                    return 1; /* already did placebc() */
            }

        } 
		else 
		{
            tmiss(obj, mon, TRUE);
        }

    } else if (otyp == BOULDER) 
	{
        exercise(A_STR, TRUE);
        if (tmp >= dieroll) 
		{
            exercise(A_DEX, TRUE);
			boolean obj_destroyed = FALSE;
			(void) hmon(mon, obj, hmode, dieroll, &obj_destroyed);
			if (obj_destroyed)
				obj = 0;
        }
		else 
		{
            tmiss(obj, mon, TRUE);
        }

    } 
	else if ((otyp == EGG || otyp == CREAM_PIE || otyp == BLINDING_VENOM || otyp == ACID_VENOM)
               && (guaranteed_hit || ACURR(A_DEX) > rnd(25)))
	{
		boolean obj_destroyed = FALSE;
        (void) hmon(mon, obj, hmode, dieroll, &obj_destroyed);
        return 1; /* hmon used it up */

    } 
	else if (obj->oclass == POTION_CLASS && (guaranteed_hit || ACURR(A_DEX) > rnd(25))) 
	{
        potionhit(mon, obj, POTHIT_HERO_THROW);
        return 1;

    } 
	else if (befriend_with_obj(mon->data, obj)
               || (is_tame(mon) && dogfood(mon, obj) <= ACCFOOD)) 
	{
        if (tamedog(mon, obj, FALSE, FALSE, 0, TRUE)) 
		{
            return 1; /* obj is gone */
        } 
		else 
		{
            tmiss(obj, mon, FALSE);
            mon->msleeping = 0;
            mon->mstrategy &= ~STRAT_WAITMASK;
        }
    } 
	else if (guaranteed_hit) 
	{
        /* this assumes that guaranteed_hit is due to swallowing */
        wakeup(mon, TRUE);
        if (obj->otyp == CORPSE && touch_petrifies(&mons[obj->corpsenm])) 
		{
            if (is_animal(u.ustuck->data)) 
			{
				int existing_stoning = get_mon_property(u.ustuck, STONED);
				set_mon_property_verbosely(u.ustuck, STONED, max(1, min(existing_stoning - 1, 5)));
				//minstapetrify(u.ustuck, TRUE);
                /* Don't leave a cockatrice corpse available in a statue */
                if (!u.uswallow) 
				{
                    delobj(obj);
                    return 1;
                }
            }
        }
        pline("%s into %s %s.", Tobjnam(obj, "vanish"),
              s_suffix(mon_nam(mon)),
              is_animal(u.ustuck->data) ? "entrails" : "currents");
    } 
	else 
	{
        tmiss(obj, mon, TRUE);
    }

    return 0;
}

STATIC_OVL int
gem_accept(mon, obj)
register struct monst *mon;
register struct obj *obj;
{
    char buf[BUFSZ];
    boolean is_buddy = sgn(mon->data->maligntyp) == sgn(u.ualign.type);
    boolean is_gem = objects[obj->otyp].oc_material == MAT_GEMSTONE;
    int ret = 0;
	int luck_change = 0;
    static NEARDATA const char nogood[] = " is not interested in your junk.";
    static NEARDATA const char acceptgift[] = " accepts your gift.";
    static NEARDATA const char maybeluck[] = " hesitatingly";
    static NEARDATA const char noluck[] = " graciously";
    static NEARDATA const char addluck[] = " gratefully";

    Strcpy(buf, Monnam(mon));
    mon->mpeaceful = 1;
    mon->mavenge = 0;

    /* object properly identified */
    if (obj->dknown && objects[obj->otyp].oc_name_known) {
        if (is_gem) {
            if (is_buddy) {
                Strcat(buf, addluck);
				luck_change += 5;
            } else {
                Strcat(buf, maybeluck);
				luck_change += rn2(7) - 3;
            }
        } else {
            Strcat(buf, nogood);
            goto nopick;
        }
        /* making guesses */
    } else if (has_oname(obj) || objects[obj->otyp].oc_uname) {
        if (is_gem) {
            if (is_buddy) {
                Strcat(buf, addluck);
				luck_change += 2;
            } else {
                Strcat(buf, maybeluck);
				luck_change += rn2(3) - 1;
            }
        } else {
            Strcat(buf, nogood);
            goto nopick;
        }
        /* value completely unknown to @ */
    } else {
        if (is_gem) {
            if (is_buddy) {
                Strcat(buf, addluck);
				luck_change += 1;
            } else {
                Strcat(buf, maybeluck);
				luck_change += rn2(3) - 1;
            }
        } else {
            Strcat(buf, noluck);
        }
    }
    Strcat(buf, acceptgift);
    if (*u.ushops || obj->unpaid)
        check_shop_obj(obj, mon->mx, mon->my, TRUE);
    (void) mpickobj(mon, obj); /* may merge and free obj */
    ret = 1;

 nopick:
    if (!Blind)
        pline1(buf);

	change_luck(luck_change, TRUE);

    if (!tele_restrict(mon))
        (void) rloc(mon, TRUE);
    return ret;
}

/*
 * Comments about the restructuring of the old breaks() routine.
 *
 * There are now three distinct phases to object breaking:
 *     breaktest() - which makes the check/decision about whether the
 *                   object is going to break.
 *     breakmsg()  - which outputs a message about the breakage,
 *                   appropriate for that particular object. Should
 *                   only be called after a positive breaktest().
 *                   on the object and, if it going to be called,
 *                   it must be called before calling breakobj().
 *                   Calling breakmsg() is optional.
 *     breakobj()  - which actually does the breakage and the side-effects
 *                   of breaking that particular object. This should
 *                   only be called after a positive breaktest() on the
 *                   object.
 *
 * Each of the above routines is currently static to this source module.
 * There are two routines callable from outside this source module which
 * perform the routines above in the correct sequence.
 *
 *   hero_breaks() - called when an object is to be broken as a result
 *                   of something that the hero has done. (throwing it,
 *                   kicking it, etc.)
 *   breaks()      - called when an object is to be broken for some
 *                   reason other than the hero doing something to it.
 */

/*
 * The hero causes breakage of an object (throwing, dropping it, etc.)
 * Return 0 if the object didn't break, 1 if the object broke.
 */
int
hero_breaks(obj, x, y, from_invent)
struct obj *obj;
xchar x, y;          /* object location (ox, oy may not be right) */
boolean from_invent; /* thrown or dropped by player; maybe on shop bill */
{
    boolean in_view = Blind ? FALSE : (from_invent || cansee(x, y));

    if (!breaktest(obj))
        return 0;
    breakmsg(obj, in_view);
    breakobj(obj, x, y, TRUE, from_invent);
    return 1;
}

/*
 * The object is going to break for a reason other than the hero doing
 * something to it.
 * Return 0 if the object doesn't break, 1 if the object broke.
 */
int
breaks(obj, x, y)
struct obj *obj;
xchar x, y; /* object location (ox, oy may not be right) */
{
    boolean in_view = Blind ? FALSE : cansee(x, y);

    if (!breaktest(obj))
        return 0;
    breakmsg(obj, in_view);
    breakobj(obj, x, y, FALSE, FALSE);
    return 1;
}

void
release_camera_demon(obj, x, y)
struct obj *obj;
xchar x, y;
{
    struct monst *mtmp;
    if (!rn2(3)
        && (mtmp = makemon(&mons[rn2(3) ? PM_HOMUNCULUS : PM_IMP], x, y,
                           NO_MM_FLAGS)) != 0) {
        if (canspotmon(mtmp))
            pline("%s is released!", Hallucination
                                         ? An(rndmonnam(NULL))
                                         : "The picture-painting demon");
        mtmp->mpeaceful = !obj->cursed;
        set_malign(mtmp);
    }
}

/*
 * Unconditionally break an object. Assumes all resistance checks
 * and break messages have been delivered prior to getting here.
 */
void
breakobj(obj, x, y, hero_caused, from_invent)
struct obj *obj;
xchar x, y;          /* object location (ox, oy may not be right) */
boolean hero_caused; /* is this the hero's fault? */
boolean from_invent;
{
    boolean fracture = FALSE;

    switch (obj->oclass == POTION_CLASS ? POT_WATER : obj->otyp) {
    case MIRROR:
	case MAGIC_MIRROR:
		if (hero_caused)
            change_luck(-2, TRUE);
        break;
    case POT_WATER:      /* really, all potions */
        obj->in_use = 1; /* in case it's fatal */
        if (obj->otyp == POT_OIL && obj->lamplit) {
            explode_oil(obj, x, y);
        } else if (distu(x, y) <= 2) {
            if (!has_innate_breathless(youmonst.data) || haseyes(youmonst.data)) {
                if (obj->otyp != POT_WATER) {
                    if (!has_innate_breathless(youmonst.data)) {
                        /* [what about "familiar odor" when known?] */
                        You("smell a peculiar odor...");
                    } else {
                        const char *eyes = body_part(EYE);

                        if (eyecount(youmonst.data) != 1)
                            eyes = makeplural(eyes);
                        Your("%s %s.", eyes, vtense(eyes, "water"));
                    }
                }
                potionbreathe(obj);
            }
        }
        /* monster breathing isn't handled... [yet?] */
        break;
    case EXPENSIVE_CAMERA:
        release_camera_demon(obj, x, y);
        break;
    case EGG:
        /* breaking your own eggs is bad luck */
        if (hero_caused && obj->spe && obj->corpsenm >= LOW_PM)
            change_luck((schar) -min(obj->quan, 5L), TRUE);
        break;
    case BOULDER:
    case STATUE:
        /* caller will handle object disposition;
           we're just doing the shop theft handling */
        fracture = TRUE;
        break;
    default:
        break;
    }

    if (hero_caused) {
        if (from_invent || obj->unpaid) {
            if (*u.ushops || obj->unpaid)
                check_shop_obj(obj, x, y, TRUE);
        } else if (!obj->no_charge && costly_spot(x, y)) {
            /* it is assumed that the obj is a floor-object */
            char *o_shop = in_rooms(x, y, SHOPBASE);
            struct monst *shkp = shop_keeper(*o_shop);

            if (shkp) { /* (implies *o_shop != '\0') */
                static NEARDATA long lastmovetime = 0L;
                static NEARDATA boolean peaceful_shk = FALSE;
                /*  We want to base shk actions on her peacefulness
                    at start of this turn, so that "simultaneous"
                    multiple breakage isn't drastically worse than
                    single breakage.  (ought to be done via ESHK)  */
                if (moves != lastmovetime)
                    peaceful_shk = is_peaceful(shkp);
                if (stolen_value(obj, x, y, peaceful_shk, FALSE) > 0L
                    && (*o_shop != u.ushops[0] || !inside_shop(u.ux, u.uy))
                    && moves != lastmovetime)
                    make_angry_shk(shkp, x, y);
                lastmovetime = moves;
            }
        }
    }
    if (!fracture)
        delobj(obj);
}

/*
 * Check to see if obj is going to break, but don't actually break it.
 * Return 0 if the object isn't going to break, 1 if it is.
 */
boolean
breaktest(obj)
struct obj *obj;
{
    if (obj_resists(obj, 1, 99))
        return 0;
    if (objects[obj->otyp].oc_material == MAT_GLASS && !obj->oartifact
        && obj->oclass != GEM_CLASS)
        return 1;
    switch (obj->oclass == POTION_CLASS ? POT_WATER : obj->otyp) {
    case EXPENSIVE_CAMERA:
    case POT_WATER: /* really, all potions */
    case EGG:
    case CREAM_PIE:
    case MELON:
    case ACID_VENOM:
    case BLINDING_VENOM:
        return 1;
    default:
        return 0;
    }
}

STATIC_OVL void
breakmsg(obj, in_view)
struct obj *obj;
boolean in_view;
{
    const char *to_pieces;

    to_pieces = "";
    switch (obj->oclass == POTION_CLASS ? POT_WATER : obj->otyp) {
    default: /* glass or crystal wand */
        if (obj->oclass != WAND_CLASS)
            impossible("breaking odd object?");
        /*FALLTHRU*/
    case CRYSTAL_PLATE_MAIL:
    case LENSES:
	case SUNGLASSES:
	case MIRROR:
	case MAGIC_MIRROR:
	case CRYSTAL_BALL:
	case EXPENSIVE_WATCH:
	case EXPENSIVE_CAMERA:
        to_pieces = " into a thousand pieces";
    /*FALLTHRU*/
    case POT_WATER: /* really, all potions */
        if (!in_view)
            You_hear("%s shatter!", something);
        else
            pline("%s shatter%s%s!", Doname2(obj),
                  (obj->quan == 1L) ? "s" : "", to_pieces);
        break;
    case EGG:
    case MELON:
        pline("Splat!");
        break;
    case CREAM_PIE:
        if (in_view)
            pline("What a mess!");
        break;
    case ACID_VENOM:
    case BLINDING_VENOM:
        pline("Splash!");
        break;
    }
}

STATIC_OVL int
throw_gold(obj)
struct obj *obj;
{
    int range, odx, ody;
    register struct monst *mon;

    if (!u.dx && !u.dy && !u.dz) {
        You("cannot throw gold at yourself.");
        return 0;
    }
    freeinv(obj);
    if (u.uswallow) {
        pline(is_animal(u.ustuck->data) ? "%s in the %s's entrails."
                                        : "%s into %s.",
              "The money disappears", mon_nam(u.ustuck));
        add_to_minv(u.ustuck, obj);
        return 1;
    }

    if (u.dz) {
        if (u.dz < 0 && !Is_airlevel(&u.uz) && !Underwater
            && !Is_waterlevel(&u.uz)) {
            pline_The("gold hits the %s, then falls back on top of your %s.",
                      ceiling(u.ux, u.uy), body_part(HEAD));
            /* some self damage? */
            if (uarmh)
                pline("Fortunately, you are wearing %s!",
                      an(helm_simple_name(uarmh)));
        }
        bhitpos.x = u.ux;
        bhitpos.y = u.uy;
    } else {
        /* consistent with range for normal objects */
        range = (int) ((ACURRSTR) / 2 - obj->owt / 40);

        /* see if the gold has a place to move into */
        odx = u.ux + u.dx;
        ody = u.uy + u.dy;
        if (!ZAP_POS(levl[odx][ody].typ) || closed_door(odx, ody)) {
            bhitpos.x = u.ux;
            bhitpos.y = u.uy;
        } else {
            mon = bhit(u.dx, u.dy, range, 0, THROWN_WEAPON,
                       (int FDECL((*), (MONST_P, OBJ_P))) 0,
                       (int FDECL((*), (OBJ_P, OBJ_P))) 0, &obj, TRUE, FALSE);
            if (!obj)
                return 1; /* object is gone */
            if (mon) {
                if (ghitm(mon, obj)) /* was it caught? */
                    return 1;
            } else {
                if (ship_object(obj, bhitpos.x, bhitpos.y, FALSE))
                    return 1;
            }
        }
    }

    if (flooreffects(obj, bhitpos.x, bhitpos.y, "fall"))
        return 1;
    if (u.dz > 0)
        pline_The("gold hits the %s.", surface(bhitpos.x, bhitpos.y));
    place_object(obj, bhitpos.x, bhitpos.y);
    if (*u.ushops)
        sellobj(obj, bhitpos.x, bhitpos.y);
    stackobj(obj);
    newsym(bhitpos.x, bhitpos.y);
    return 1;
}

/*dothrow.c*/
