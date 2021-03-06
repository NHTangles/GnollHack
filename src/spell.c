/* GnollHack 4.0	spell.c	$NHDT-Date: 1546565814 2019/01/04 01:36:54 $  $NHDT-Branch: GnollHack-3.6.2-beta01 $:$NHDT-Revision: 1.88 $ */
/*      Copyright (c) M. Stephenson 1988                          */
/* GnollHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* spellmenu arguments; 0 thru n-1 used as spl_book[] index when swapping */
#define SPELLMENU_DETAILS (-4)
#define SPELLMENU_PREPARE (-3)
#define SPELLMENU_CAST (-2)
#define SPELLMENU_VIEW (-1)
#define SPELLMENU_SORT (MAXSPELL) /* special menu entry */

/* spell retention period, in turns; at 10% of this value, player becomes
   eligible to reread the spellbook and regain 100% retention (the threshold
   used to be 1000 turns, which was 10% of the original 10000 turn retention
   period but didn't get adjusted when that period got doubled to 20000) */
#define KEEN 20000
/* x: need to add 1 when used for reading a spellbook rather than for hero
   initialization; spell memory is decremented at the end of each turn,
   including the turn on which the spellbook is read; without the extra
   increment, the hero used to get cheated out of 1 turn of retention */
#define incrnknow(spell, x) (spl_book[spell].sp_know = KEEN + (x))

#define spellev(spell) spl_book[spell].sp_lev
#define spellamount(spell) spl_book[spell].sp_amount
#define spellmatcomp(spell) spl_book[spell].sp_matcomp
#define spellcooldownlength(spell) spl_book[spell].sp_cooldownlength
#define spellcooldownleft(spell) spl_book[spell].sp_cooldownleft
#define spellskillchance(spell) spl_book[spell].sp_skillchance
#define spellhotkey(spell) spl_book[spell].sp_hotkey
#define spellname(spell) OBJ_NAME(objects[spellid(spell)])
#define spellet(spell) \
    ((char) ((spell < 26) ? ('a' + spell) : ('A' + spell - 26)))

STATIC_DCL int FDECL(spell_let_to_idx, (CHAR_P));
STATIC_DCL boolean FDECL(cursed_book, (struct obj * bp));
STATIC_DCL boolean FDECL(confused_book, (struct obj *));
STATIC_DCL void FDECL(deadbook, (struct obj *));
STATIC_PTR int NDECL(learn);
STATIC_DCL boolean NDECL(rejectcasting);
STATIC_DCL boolean FDECL(getspell, (int *, int));
STATIC_PTR int FDECL(CFDECLSPEC spell_cmp, (const genericptr,
                                            const genericptr));
STATIC_DCL void NDECL(sortspells);
STATIC_DCL boolean NDECL(spellsortmenu);
STATIC_DCL boolean FDECL(dospellmenu, (const char *, int, int *));
STATIC_DCL int FDECL(percent_success, (int));
STATIC_DCL int FDECL(attribute_value_for_spellbook, (int));
STATIC_DCL char *FDECL(spellretention, (int, char *));
STATIC_DCL int FDECL(throwspell, (int));
STATIC_DCL void FDECL(spell_backfire, (int));
STATIC_DCL const char *FDECL(spelltypemnemonic, (int));
STATIC_DCL boolean FDECL(spell_aim_step, (genericptr_t, int, int));
STATIC_DCL int FDECL(domaterialcomponentsmenu, (int));
STATIC_DCL void FDECL(add_spell_cast_menu_item, (winid, int, int, int, char*, int*, BOOLEAN_P));
STATIC_DCL void FDECL(add_spell_cast_menu_heading, (winid, int, BOOLEAN_P));
STATIC_DCL void FDECL(add_spell_prepare_menu_item, (winid, int, int, int, int, BOOLEAN_P));
STATIC_DCL void FDECL(add_spell_prepare_menu_heading, (winid, int, int, BOOLEAN_P));

/* The roles[] table lists the role-specific values for tuning
 * percent_success().
 *
 * Reasoning:
 *   spelbase, spelheal:
 *      Arc are aware of magic through historical research
 *      Bar abhor magic (Conan finds it "interferes with his animal instincts")
 *      Cav are ignorant to magic
 *      Hea are very aware of healing magic through medical research
 *      Kni are moderately aware of healing from Paladin training
 *      Mon use magic to attack and defend in lieu of weapons and armor
 *      Pri are very aware of healing magic through theological research
 *      Ran avoid magic, preferring to fight unseen and unheard
 *      Rog are moderately aware of magic through trickery
 *      Sam have limited magical awareness, preferring meditation to conjuring
 *      Tou are aware of magic from all the great films they have seen
 *      Val have limited magical awareness, preferring fighting
 *      Wiz are trained mages
 *
 *      The arms penalty is lessened for trained fighters Bar, Kni, Ran,
 *      Sam, Val -- the penalty is its metal interference, not encumbrance.
 *      The `spelspec' is a single spell which is fundamentally easier
 *      for that role to cast.
 *
 *  spelspec, spelsbon:
 *      Arc map masters (SPE_MAGIC_MAPPING)
 *      Bar fugue/berserker (SPE_HASTE_SELF)
 *      Cav born to dig (SPE_DIG)
 *      Hea to heal (SPE_CURE_SICKNESS)
 *      Kni to turn back evil (SPE_TURN_UNDEAD)
 *      Mon to preserve their abilities (SPE_RESTORE_ABILITY)
 *      Pri to bless (SPE_REMOVE_CURSE)
 *      Ran to hide (SPE_INVISIBILITY)
 *      Rog to find loot (SPE_DETECT_TREASURE)
 *      Sam to be At One (SPE_CLAIRVOYANCE)
 *      Tou to smile (SPE_SPHERE_OF_CHARMING)
 *      Val control the cold (SPE_CONE_OF_COLD)
 *      Wiz all really, but SPE_MAGIC_MISSILE is their party trick
 *
 *      See percent_success() below for more comments.
 *
 *  uarmbon, uarmsbon, uarmhbon, uarmgbon, uarmfbon:
 *      Fighters find body armour & shield a little less limiting.
 *      Headgear, Gauntlets and Footwear are not role-specific (but
 *      still have an effect, except helm of brilliance, which is designed
 *      to permit magic-use).
 */

/* since the spellbook itself doesn't blow up, don't say just "explodes" */
static const char explodes[] = "radiates explosive energy";

/* convert a letter into a number in the range 0..51, or -1 if not a letter */
STATIC_OVL int
spell_let_to_idx(ilet)
char ilet;
{
    int indx;

    indx = ilet - 'a';
    if (indx >= 0 && indx < 26)
        return indx;
    indx = ilet - 'A';
    if (indx >= 0 && indx < 26)
        return indx + 26;
    return -1;
}

/* TRUE: book should be destroyed by caller */
STATIC_OVL boolean
cursed_book(bp)
struct obj *bp;
{
    boolean was_in_use;
    int lev = objects[bp->otyp].oc_spell_level;
    int dmg = 0;

    switch (rn2(max(1, (lev + 4) / 2))) {
    case 0:
        You_feel("a wrenching sensation.");
        tele(); /* teleport him */
        break;
    case 1:
        You_feel("threatened.");
        aggravate();
        break;
    case 2:
        make_blinded(Blinded + rn1(100, 250), TRUE);
        break;
    case 3:
        take_gold();
        break;
    case 4:
        pline("These runes were just too much to comprehend.");
        make_confused(itimeout_incr(HConfusion, rn1(7, 16)), FALSE);
        break;
    case 5:
        pline_The("book was coated with contact poison!");
        if (uarmg) {
            erode_obj(uarmg, "gloves", ERODE_CORRODE, EF_GREASE | EF_VERBOSE);
            break;
        }
        /* temp disable in_use; death should not destroy the book */
        was_in_use = bp->in_use;
        bp->in_use = FALSE;
        losestr(Poison_resistance ? rn1(2, 1) : rn1(4, 3));
        losehp(rnd(Poison_resistance ? 6 : 10), "contact-poisoned spellbook",
               KILLED_BY_AN);
        bp->in_use = was_in_use;
        break;
    case 6:
        if (Antimagic) {
            shieldeff(u.ux, u.uy);
            pline_The("book %s, but you are unharmed!", explodes);
        } else {
            pline("As you read the book, it %s in your %s!", explodes,
                  body_part(FACE));
            dmg = 2 * rnd(10) + 5;
            losehp(Maybe_Half_Phys(dmg), "exploding rune", KILLED_BY_AN);
        }
        return TRUE;
    default:
        rndcurse();
        break;
    }
    return FALSE;
}

/* study while confused: returns TRUE if the book is destroyed */
STATIC_OVL boolean
confused_book(spellbook)
struct obj *spellbook;
{
    boolean gone = FALSE;

    if (!rn2(3) && spellbook->otyp != SPE_BOOK_OF_THE_DEAD) {
        spellbook->in_use = TRUE; /* in case called from learn */
        pline(
         "Being confused you have difficulties in controlling your actions.");
        display_nhwindow(WIN_MESSAGE, FALSE);
        You("accidentally tear the spellbook to pieces.");
        if (!objects[spellbook->otyp].oc_name_known
            && !objects[spellbook->otyp].oc_uname)
            docall(spellbook);
        useup(spellbook);
        gone = TRUE;
    } else {
        You("find yourself reading the %s line over and over again.",
            spellbook == context.spbook.book ? "next" : "first");
    }
    return gone;
}

/* special effects for The Book of the Dead */
STATIC_OVL void
deadbook(book2)
struct obj *book2;
{
    struct monst *mtmp, *mtmp2;
    coord mm;

    You("turn the pages of the Book of the Dead...");
    makeknown(SPE_BOOK_OF_THE_DEAD);
    /* KMH -- Need ->known to avoid "_a_ Book of the Dead" */
    book2->known = 1;
    if (invocation_pos(u.ux, u.uy) && !On_stairs(u.ux, u.uy)) {
        register struct obj *otmp;
        register boolean arti1_primed = FALSE, arti2_primed = FALSE,
                         arti_cursed = FALSE;

        if (book2->cursed) {
            pline_The("runes appear scrambled.  You can't read them!");
            return;
        }

        if (!u.uhave.bell || !u.uhave.menorah) {
            pline("A chill runs down your %s.", body_part(SPINE));
            if (!u.uhave.bell)
                You_hear("a faint chime...");
            if (!u.uhave.menorah)
                pline("Vlad's doppelganger is amused.");
            return;
        }

        for (otmp = invent; otmp; otmp = otmp->nobj) {
            if (otmp->otyp == CANDELABRUM_OF_INVOCATION && otmp->spe == 7
                && otmp->lamplit) {
                if (!otmp->cursed)
                    arti1_primed = TRUE;
                else
                    arti_cursed = TRUE;
            }
            if (otmp->otyp == BELL_OF_OPENING
                && (moves - otmp->age) < 5L) { /* you rang it recently */
                if (!otmp->cursed)
                    arti2_primed = TRUE;
                else
                    arti_cursed = TRUE;
            }
        }

        if (arti_cursed) {
            pline_The("invocation fails!");
            pline("At least one of your artifacts is cursed...");
        } else if (arti1_primed && arti2_primed) {
            unsigned soon =
                (unsigned) d(2, 6); /* time til next intervene() */

            /* successful invocation */
            mkinvokearea();
            u.uevent.invoked = 1;
            /* in case you haven't killed the Wizard yet, behave as if
               you just did */
            u.uevent.udemigod = 1; /* wizdead() */
            if (!u.udg_cnt || u.udg_cnt > soon)
                u.udg_cnt = soon;
        } else { /* at least one artifact not prepared properly */
            You("have a feeling that %s is amiss...", something);
            goto raise_dead;
        }
        return;
    }

    /* when not an invocation situation */
    if (book2->cursed)
	{
    raise_dead:

        You("raised the dead!");
        /* first maybe place a dangerous adversary */
        if (!rn2(3) && ((mtmp = makemon(&mons[PM_MASTER_LICH], u.ux, u.uy,
                                        MM_NO_MONSTER_INVENTORY)) != 0
                        || (mtmp = makemon(&mons[PM_NALFESHNEE], u.ux, u.uy,
                                           MM_NO_MONSTER_INVENTORY)) != 0)) {
            mtmp->mpeaceful = 0;
            set_malign(mtmp);
        }
        /* next handle the affect on things you're carrying */
        (void) revive_from_inventory(&youmonst);
        /* last place some monsters around you */
        mm.x = u.ux;
        mm.y = u.uy;
        mkundead(&mm, TRUE, MM_NO_MONSTER_INVENTORY);
    } 
	else if (book2->blessed)
	{
        for (mtmp = fmon; mtmp; mtmp = mtmp2) 
		{
            mtmp2 = mtmp->nmon; /* tamedog() changes chain */
            if (DEADMONSTER(mtmp))
                continue;

            if ((is_undead(mtmp->data) || is_vampshifter(mtmp))
                && cansee(mtmp->mx, mtmp->my))
			{
                mtmp->mpeaceful = TRUE;

				if (sgn(mtmp->data->maligntyp) == sgn(u.ualign.type)
					&& distu(mtmp->mx, mtmp->my) < 4)
				{
					if (mtmp->mtame)
					{
						if (mtmp->mtame < 20)
							mtmp->mtame++;
					}
					else
						(void)tamedog(mtmp, (struct obj*) 0, FALSE, FALSE, 0, FALSE);
				}
                else
                    monflee(mtmp, 0, FALSE, TRUE);
            }
        }
    }
	else
	{
        switch (rn2(3))
		{
        case 0:
            Your("ancestors are annoyed with you!");
            break;
        case 1:
            pline_The("headstones in the cemetery begin to move!");
            break;
        default:
            pline("Oh my!  Your name appears in the book!");
        }
    }
    return;
}

/* 'book' has just become cursed; if we're reading it and realize it is
   now cursed, interrupt */
void
book_cursed(book)
struct obj *book;
{
    if (occupation == learn && context.spbook.book == book
        && book->cursed && book->bknown && multi >= 0)
        stop_occupation();
}

STATIC_PTR int
learn(VOID_ARGS)
{
    int i;
    short booktype;
    char splname[BUFSZ];
    boolean costly = TRUE;
    struct obj *book = context.spbook.book;
	boolean learnsuccess = FALSE;

    /* JDS: lenses give 50% faster reading; 33% smaller read time */
    if (context.spbook.delay && Enhanced_vision && rn2(2))
        context.spbook.delay++;
    if (Confusion && book->otyp != SPE_BOOK_OF_THE_DEAD) 
	{ /* became confused while learning */
		context.spbook.reading_result = READING_RESULT_CONFUSED;
#if 0
        (void) confused_book(book);
        context.spbook.book = 0; /* no longer studying */
        context.spbook.o_id = 0;
        nomul(context.spbook.delay); /* remaining delay is uninterrupted */
        multi_reason = "reading a book";
        nomovemsg = 0;
        context.spbook.delay = 0;
        return 0;
#endif
    }
    if (context.spbook.delay) {
        /* not if (context.spbook.delay++), so at end delay == 0 */
        context.spbook.delay++;
        return 1; /* still busy */
    }


	/* STUDY RESULT */
	booktype = book->otyp;

	/* Book of the Dead always has the same result */
	if (booktype == SPE_BOOK_OF_THE_DEAD) {
		deadbook(book);
		return 0;
	}

	/* Possible failures */
	boolean reading_result = context.spbook.reading_result;
	if (reading_result == READING_RESULT_FAIL)
	{
		boolean gone = FALSE;

		if (book->cursed)
			gone = cursed_book(book);
		else if (!book->blessed && !rn2(2))
		{
			pline("These runes were just too much to comprehend.");
			make_confused(itimeout_incr(HConfusion, rnd(4) + 5), FALSE);
		}
		else
		{
			pline("Despite your best efforts, you fail to understand the spell in %s.", the(cxname(book)));
		}

		if (gone || !rn2(2)) {
			if (!gone)
				pline_The("spellbook crumbles to dust!");
			if (!objects[book->otyp].oc_name_known
				&& !objects[book->otyp].oc_uname)
				docall(book);
			useup(book);
		}
		else
			book->in_use = FALSE;
		return 0;
	}
	else if (reading_result == READING_RESULT_CONFUSED)
	{
		if (!confused_book(book))
		{
			book->in_use = FALSE;
		}
		return 0;
	}


	/* SUCCESS */
	exercise(A_WIS, TRUE); /* you're studying. */

    Sprintf(splname,
            objects[booktype].oc_name_known ? "\"%s\"" : "the \"%s\" spell",
            OBJ_NAME(objects[booktype]));
    for (i = 0; i < MAXSPELL; i++)
        if (spellid(i) == booktype || spellid(i) == NO_SPELL)
            break;

    if (i == MAXSPELL)
	{
        impossible("Too many spells memorized!");
    }
	else if (spellid(i) == booktype)
	{
        /* normal book can be read and re-read a total of 4 times */
        if (book->spestudied > MAX_SPELL_STUDY)
		{
            pline("This spellbook is too faint to be read any more.");
            book->otyp = booktype = SPE_BLANK_PAPER;
            /* reset spestudied as if polymorph had taken place */
            book->spestudied = rn2(book->spestudied);
        }
		else if (spellknow(i) > KEEN / 10)
		{
            You("know %s quite well already.", splname);
            costly = FALSE;
        }
		else
		{ /* spellknow(i) <= KEEN/10 */
            Your("knowledge of %s is %s.", splname,
                 spellknow(i) ? "keener" : "restored");
            incrnknow(i, 1);
            book->spestudied++;
            exercise(A_WIS, TRUE); /* extra study */
        }
        /* make book become known even when spell is already
           known, in case amnesia made you forget the book */
        makeknown((int) booktype);
    }
	else
	{ /* (spellid(i) == NO_SPELL) */
        /* for a normal book, spestudied will be zero, but for
           a polymorphed one, spestudied will be non-zero and
           one less reading is available than when re-learning */
        if (book->spestudied >= MAX_SPELL_STUDY) {
            /* pre-used due to being the product of polymorph */
            pline("This spellbook is too faint to read even once.");
            book->otyp = booktype = SPE_BLANK_PAPER;
            /* reset spestudied as if polymorph had taken place */
            book->spestudied = rn2(book->spestudied);
        } else {
            spl_book[i].sp_id = booktype;
            spl_book[i].sp_lev = objects[booktype].oc_spell_level;
			spl_book[i].sp_matcomp = objects[booktype].oc_material_components;
			if(spl_book[i].sp_matcomp)
				spl_book[i].sp_amount = 0; //How many times material components have been mixed
			else
				spl_book[i].sp_amount = -1; //Infinite
			spl_book[i].sp_cooldownlength = objects[booktype].oc_spell_cooldown;
			spl_book[i].sp_cooldownleft = 0;
			spl_book[i].sp_skillchance = objects[booktype].oc_spell_skill_chance;

			incrnknow(i, 1);
            book->spestudied++;
            You(i > 0 ? "add %s to your repertoire." : "learn %s.", splname);
			learnsuccess = TRUE;
        }
        makeknown((int) booktype);
    }

    if (book->cursed) { /* maybe a demon cursed it */
        if (cursed_book(book)) {
            useup(book);
            context.spbook.book = 0;
            context.spbook.o_id = 0;
            return 0;
        }
    }
    if (costly)
        check_unpaid(book);

	if (learnsuccess)
	{
		pline_The("spellbook crumbles to dust.");
		useup(book);
	}

	context.spbook.book = 0;
    context.spbook.o_id = 0;
    return 0;
}

int
study_book(spellbook)
register struct obj *spellbook;
{
	if (!spellbook)
		return 0;

    int booktype = spellbook->otyp;
    boolean confused = (Confusion != 0);
	boolean hallucinated = (Hallucination != 0);
	boolean too_hard = FALSE;
	char lvlbuf[BUFSZ] = "";
	char buf[BUFSZ] = "";
	char namebuf[BUFSZ] = "";
	char Namebuf2[BUFSZ] = "";
	boolean takeround = 0;
	boolean perusetext = 0;

	if (!(context.spbook.delay && spellbook == context.spbook.book)
		&& !(objects[spellbook->otyp].oc_flags & O1_NON_SPELL_SPELLBOOK)
		&& spellbook->otyp != SPE_BLANK_PAPER
		&& spellbook->otyp != SPE_BOOK_OF_THE_DEAD
		&& spellbook->otyp != SPE_NOVEL)
	{
		strcpy(namebuf, OBJ_NAME(objects[booktype]));
		strcpy(Namebuf2, OBJ_NAME(objects[booktype]));
		*Namebuf2 = highc(*Namebuf2);

		if(objects[booktype].oc_spell_level == -1)
			Sprintf(lvlbuf, "minor %s cantrip", spelltypemnemonic(objects[booktype].oc_skill));
		else if (objects[booktype].oc_spell_level == 0)
			Sprintf(lvlbuf, "major %s cantrip", spelltypemnemonic(objects[booktype].oc_skill));
		else if (objects[booktype].oc_spell_level > 0)
			Sprintf(lvlbuf, "level %d %s spell", objects[booktype].oc_spell_level, spelltypemnemonic(objects[booktype].oc_skill));

		if (!confused && !hallucinated)
		{
			if (!objects[spellbook->otyp].oc_name_known)
			{
				if (objects[spellbook->otyp].oc_aflags & S1_SPELLBOOK_MUST_BE_READ_TO_IDENTIFY)
				{
					if(!objects[spellbook->otyp].oc_content_desc || strcmp(objects[spellbook->otyp].oc_content_desc, "") == 0)
						Sprintf(buf, "The topic of %s is unclear. Read it?", the(cxname(spellbook)));
					else
						Sprintf(buf, "This spellbook contains %s. Read it?", objects[spellbook->otyp].oc_content_desc);
					takeround = 1;
					perusetext = 1;
				}
				else
				{
					pline("This spellbook contains \"%s\".", namebuf);
					makeknown(spellbook->otyp);
					takeround = 1;
					Sprintf(buf, "\"%s\" is %s. Continue?", Namebuf2, an(lvlbuf));
				}
			}
			else
			{
				Sprintf(buf, "\"%s\" is %s. Continue?", Namebuf2, an(lvlbuf));
			}
		}
		else if (hallucinated)
		{
			Sprintf(buf, "Whoa! This book contains some real deep stuff. Continue?");
			takeround = 1;
		}
		else //Confused
		{
			takeround = 1;
			if (!rn2(3))
				Sprintf(buf, "This spellbook contains %s. Read it?", an(lvlbuf));
			else
				Sprintf(buf, "The runes in %s seem to be all over the place. Continue?", the(cxname(spellbook)));
		}
		if (yn(buf) != 'y')
			return takeround; // Takes one round to read the header if not known in advance

		if (perusetext)
			You("start perusing %s...", the(cxname(spellbook)));

		/* attempting to read dull book may make hero fall asleep */
		if (!confused && !Sleep_resistance
			&& !strcmp(OBJ_DESCR(objects[booktype]), "dull")) {
			const char *eyes;
			int dullbook = rnd(25) - ACURR(A_WIS);

			/* adjust chance if hero stayed awake, got interrupted, retries */
			if (context.spbook.delay && spellbook == context.spbook.book)
				dullbook -= rnd(objects[booktype].oc_spell_level);

			if (dullbook > 0) {
				eyes = body_part(EYE);
				if (eyecount(youmonst.data) > 1)
					eyes = makeplural(eyes);
				pline("This book is so dull that you can't keep your %s open.",
					  eyes);
				dullbook += rnd(2 * objects[booktype].oc_spell_level);
				fall_asleep(-dullbook, TRUE);
				return 1;
			}
		}
	}

    if (context.spbook.delay && !confused && spellbook == context.spbook.book
        /* handle the sequence: start reading, get interrupted, have
           context.spbook.book become erased somehow, resume reading it */
        && booktype != SPE_BLANK_PAPER) {
        You("continue your efforts to %s.",
            (booktype == SPE_NOVEL) ? "read the novel" : "memorize the spell");
    } 
	else 
	{
        /* KMH -- Simplified this code */
        if (booktype == SPE_BLANK_PAPER) {
            pline("This spellbook is all blank.");
            makeknown(booktype);
            return 1;
        }

        /* 3.6 tribute */
        if (booktype == SPE_NOVEL) 
		{
            /* Obtain current Terry Pratchett book title */
            const char *tribtitle = noveltitle(&spellbook->novelidx);

            if (read_tribute("books", tribtitle, 0, (char *) 0, 0,
                             spellbook->o_id))
			{
                u.uconduct.literate++;
                check_unpaid(spellbook);
                makeknown(booktype);
                if (!u.uevent.read_tribute) 
				{
                    /* give bonus of 20 xp and 4*20+0 pts */
                    more_experienced(20, 0);
                    newexplevel();
                    u.uevent.read_tribute = 1; /* only once */
                }
            }
            return 1;
        }

		context.spbook.delay = -min(8, max(1, (objects[booktype].oc_spell_level)))
			* objects[booktype].oc_delay;

        /* Books are often wiser than their readers (Rus.) */
        spellbook->in_use = TRUE;
        if (!spellbook->blessed && spellbook->otyp != SPE_BOOK_OF_THE_DEAD) {
            if (spellbook->cursed)
			{
                too_hard = TRUE;
            }
			else
			{

				/* uncursed - chance to fail */
				int read_ability = attribute_value_for_spellbook(spellbook->otyp)
					+ 8 
					+ u.ulevel
					+ 4 * max(0, P_SKILL(spell_skilltype(spellbook->otyp)) - 1)
					- 2 * objects[booktype].oc_spell_level
					+ (Enhanced_vision ? 2 : 0);

                if (read_ability < 20 && !confused) //Role_if(PM_WIZARD) && 
				{
                    char qbuf[QBUFSZ];
					char descbuf[BUFSZ] = "difficult";

					if (read_ability <= 0)
						Sprintf(descbuf, "%sseems impossible", perusetext ? "still " : "");
					else if (read_ability <= 4)
						Sprintf(descbuf, "is %svery difficult", perusetext ? "still " : "");
					else if (read_ability <= 8)
						Sprintf(descbuf, "is %sdifficult", perusetext ? "still " : "");
					else if (read_ability <= 12)
						Sprintf(descbuf, "%sseems rather difficult", perusetext ? "still " : "");
					else if (read_ability <= 16)
						Sprintf(descbuf, "seems somewhat easy");
					else
						Sprintf(descbuf, "seems easy");

					Sprintf(qbuf, "This spellbook %s to comprehend. Continue?", descbuf);

                    if (yn(qbuf) != 'y') 
					{
                        spellbook->in_use = FALSE;
                        return 1;
                    }
                }
                /* its up to random luck now */
                if (rnd(20) > read_ability) {
                    too_hard = TRUE;
                }
            }
        }

		/* Now, check the result */
		context.spbook.reading_result = READING_RESULT_SUCCESS;
        if (too_hard)
		{
			context.spbook.reading_result = READING_RESULT_FAIL;
#if 0
			boolean gone = FALSE;
			
			if(spellbook->cursed)
				gone = cursed_book(spellbook);
			else if (!spellbook->blessed && !rn2(2))
			{
				pline("These runes were just too much to comprehend.");
				make_confused(itimeout_incr(HConfusion, rnd(4) + 5), FALSE);
			}

            nomul(context.spbook.delay); /* study time */
            multi_reason = "reading a book";
            nomovemsg = 0;
            context.spbook.delay = 0;
            if (gone || !rn2(2)) {
                if (!gone)
                    pline_The("spellbook crumbles to dust!");
                if (!objects[spellbook->otyp].oc_name_known
                    && !objects[spellbook->otyp].oc_uname)
                    docall(spellbook);
                useup(spellbook);
            } else
                spellbook->in_use = FALSE;
            return 1;
#endif
        } 
		else if (confused) 
		{
			context.spbook.reading_result = READING_RESULT_CONFUSED;
#if 0
			if (!confused_book(spellbook))
			{
                spellbook->in_use = FALSE;
            }
            nomul(context.spbook.delay);
            multi_reason = "reading a book";
            nomovemsg = 0;
            context.spbook.delay = 0;
            return 1;
#endif
        }
        spellbook->in_use = FALSE;

//		if (perusetext)
//			pline("The spellbook seems comprehensible enough.");

		You("begin to %s the runes.",
			spellbook->otyp == SPE_BOOK_OF_THE_DEAD ? "recite" : "memorize");
    }

    context.spbook.book = spellbook;
    if (context.spbook.book)
        context.spbook.o_id = context.spbook.book->o_id;
    set_occupation(learn, "studying", 0);
    return 1;
}

/* a spellbook has been destroyed or the character has changed levels;
   the stored address for the current book is no longer valid */
void
book_disappears(obj)
struct obj *obj;
{
    if (obj == context.spbook.book) {
        context.spbook.book = (struct obj *) 0;
        context.spbook.o_id = 0;
    }
}

/* renaming an object usually results in it having a different address;
   so the sequence start reading, get interrupted, name the book, resume
   reading would read the "new" book from scratch */
void
book_substitution(old_obj, new_obj)
struct obj *old_obj, *new_obj;
{
    if (old_obj == context.spbook.book) {
        context.spbook.book = new_obj;
        if (context.spbook.book)
            context.spbook.o_id = context.spbook.book->o_id;
    }
}

/* called from moveloop() */
void
age_spells()
{
    int i;
    /*
     * The time relative to the hero (a pass through move
     * loop) causes all spell knowledge to be decremented.
     * The hero's speed, rest status, conscious status etc.
     * does not alter the loss of memory.
     */
    for (i = 0; i < MAXSPELL && spellid(i) != NO_SPELL; i++)
        if (spellknow(i))
            decrnknow(i);
    return;
}

/* return True if spellcasting is inhibited;
   only covers a small subset of reasons why casting won't work */
STATIC_OVL boolean
rejectcasting()
{
    /* rejections which take place before selecting a particular spell */
    if (Stunned)
	{
        You("are too impaired to cast a spell.");
        return TRUE;
    } 
	else if (!can_chant(&youmonst))
	{
        You("are unable to chant the incantation.");
        return TRUE;
    }
	else if (Cancelled)
	{
		Your("magic is not flowing properly to allow for casting a spell.");
		return TRUE;
	}
	else if (!freehand()) {
        /* Note: !freehand() occurs when weapon and shield (or two-handed
         * weapon) are welded to hands, so "arms" probably doesn't need
         * to be makeplural(bodypart(ARM)).
         *
         * But why isn't lack of free arms (for gesturing) an issue when
         * poly'd hero has no limbs?
         */
        Your("arms are not free to cast!");
        return TRUE;
    }
    return FALSE;
}

/*
 * Return TRUE if a spell was picked, with the spell index in the return
 * parameter.  Otherwise return FALSE.
 */
STATIC_OVL boolean
getspell(spell_no, spell_list_type)
int *spell_no;
int spell_list_type;
{
    int nspells, idx;
    char ilet, lets[BUFSZ], qbuf[QBUFSZ];

    if (spellid(0) == NO_SPELL) {
        You("don't know any spells right now.");
        return FALSE;
    }
    if (rejectcasting())
        return FALSE; /* no spell chosen */

    if (flags.menu_style == MENU_TRADITIONAL) {
        /* we know there is at least 1 known spell */
        for (nspells = 1; nspells < min(MAXSPELL, 52) && spellid(nspells) != NO_SPELL;
             nspells++)
            continue;

        if (nspells == 1)
            Strcpy(lets, "a");
        else if (nspells < 27)
            Sprintf(lets, "a-%c", 'a' + nspells - 1);
        else if (nspells == 27)
            Sprintf(lets, "a-zA");
        /* this assumes that there are at most 52 spells... */
        else
            Sprintf(lets, "a-zA-%c", 'A' + nspells - 27);

        for (;;) {
            Sprintf(qbuf, "%s which spell? [%s *?]", (spell_list_type == 2 ? "Provide information on" : spell_list_type == 1 ? "Prepare" : "Cast"), lets);
            ilet = yn_function(qbuf, (char *) 0, '\0');
            if (ilet == '*' || ilet == '?')
                break; /* use menu mode */
            if (index(quitchars, ilet))
                return FALSE;

            idx = spell_let_to_idx(ilet);
            if (idx < 0 || idx >= nspells) {
                You("don't know that spell.");
                continue; /* ask again */
            }
            *spell_no = idx;
            return TRUE;
        }
    }
    return dospellmenu((spell_list_type == 2 ? "Choose a spell to view or manage" : spell_list_type == 1 ? "Choose a spell to prepare" : "Choose a spell to cast"),
		(spell_list_type == 2 ? SPELLMENU_DETAILS : spell_list_type == 1 ? SPELLMENU_PREPARE : SPELLMENU_CAST),
                       spell_no);
}

/* the 'Z' command -- cast a spell */
int
docast()
{
    int spell_no;

    if (getspell(&spell_no, FALSE))
        return spelleffects(spell_no, FALSE);

    return 0;
}

/* the M('z') command -- spell info / descriptions */
int
dospellviewormanage()
{
	int spell_no;

	if (getspell(&spell_no, 2))
		return dospellmanagemenu(spell_no);
	return 0;
}


STATIC_OVL const char *
spelltypemnemonic(skill)
int skill;
{
    switch (skill) {
    case P_ARCANE_SPELL:
        return "arcane";
    case P_HEALING_SPELL:
        return "healing";
    case P_DIVINATION_SPELL:
        return "divination";
    case P_ENCHANTMENT_SPELL:
        return "enchantment";
    case P_CLERIC_SPELL:
        return "clerical";
    case P_MOVEMENT_SPELL:
        return "movement";
    case P_TRANSMUTATION_SPELL:
        return "transmutation";
	case P_CONJURATION_SPELL:
		return "conjuration";
	case P_ABJURATION_SPELL:
		return "abjuration";
	case P_NECROMANCY_SPELL:
		return "necromancy";
	default:
        impossible("Unknown spell skill, %d;", skill);
        return empty_string;
    }
}

int
spell_skilltype(booktype)
int booktype;
{
    return objects[booktype].oc_skill;
}

/* attempting to cast a forgotten spell will cause disorientation */
STATIC_OVL void
spell_backfire(spell)
int spell;
{
    long duration = (long) ((spellev(spell) + 1) * 3), /* 6..24 */
         old_stun = (HStun & TIMEOUT), old_conf = (HConfusion & TIMEOUT);

    /* Prior to 3.4.1, only effect was confusion; it still predominates.
     *
     * 3.6.0: this used to override pre-existing confusion duration
     * (cases 0..8) and pre-existing stun duration (cases 4..9);
     * increase them instead.   (Hero can no longer cast spells while
     * Stunned, so the potential increment to stun duration here is
     * just hypothetical.)
     */
    switch (rn2(10)) {
    case 0:
    case 1:
    case 2:
    case 3:
        make_confused(old_conf + duration, FALSE); /* 40% */
        break;
    case 4:
    case 5:
    case 6:
        make_confused(old_conf + 2L * duration / 3L, FALSE); /* 30% */
        make_stunned(old_stun + duration / 3L, FALSE);
        break;
    case 7:
    case 8:
        make_stunned(old_stun + 2L * duration / 3L, FALSE); /* 20% */
        make_confused(old_conf + duration / 3L, FALSE);
        break;
    case 9:
        make_stunned(old_stun + duration, FALSE); /* 10% */
        break;
    }
    return;
}

int
getspellenergycost(spell)
int spell;
{
	if (spell < 0)
		return 0;

	int energy = get_spellbook_adjusted_mana_cost(spellid(spell));

	return energy;
}

int
get_spellbook_adjusted_mana_cost(objid)
int objid;
{
	if (objid <= STRANGE_OBJECT || objid >= NUM_OBJECTS)
		return 0;

	int skill = objects[objid].oc_skill;
	int skill_level = P_SKILL(skill);
	int multiplier = 100;
	switch (skill_level)
	{
	case P_BASIC:
		multiplier = 85;
		break;
	case P_SKILLED:
		multiplier = 70;
		break;
	case P_EXPERT:
		multiplier = 50;
		break;
	default:
		break;
	}

	int energy = max(2, (objects[objid].oc_spell_mana_cost * multiplier) / 100);

	return energy;
}



int
spelldescription(spell)
int spell;
{

	if (spellknow(spell) <= 0)
	{
		You("cannot recall this spell anymore.");
		return 0;
	}


	winid datawin = WIN_ERR;

	datawin = create_nhwindow(NHW_MENU);

	int booktype = spellid(spell);
	char buf[BUFSZ];
	char buf2[BUFSZ];
	char buf3[BUFSZ];
	const char* txt;

	extern const struct propname {
		int prop_num;
		const char* prop_name;
		const char* prop_noun;
	} propertynames[]; /* timeout.c */

	/* Name */
	strcpy(buf, spellname(spell));
	*buf = highc(*buf);
	txt = buf;
	putstr(datawin, 0, txt);

	/* Level & category*/
	if (objects[booktype].oc_spell_level == -1)
		Sprintf(buf, "Minor %s cantrip", spelltypemnemonic(objects[booktype].oc_skill));
	else if (objects[booktype].oc_spell_level == 0)
		Sprintf(buf, "Major %s cantrip", spelltypemnemonic(objects[booktype].oc_skill));
	else if (objects[booktype].oc_spell_level > 0)
		Sprintf(buf, "Level %d %s spell", objects[booktype].oc_spell_level, spelltypemnemonic(objects[booktype].oc_skill));

	txt = buf;
	putstr(datawin, 0, txt);

	/* One empty line here */
	Sprintf(buf, "");
	txt = buf;
	putstr(datawin, 0, txt);

	/* Attributes*/
	if (objects[booktype].oc_spell_attribute >= 0)
	{
		char statbuf[BUFSZ];

		switch (objects[booktype].oc_spell_attribute)
		{
		case A_STR:
			strcpy(statbuf, "Strength");
			break;
		case A_DEX:
			strcpy(statbuf, "Dexterity");
			break;
		case A_CON:
			strcpy(statbuf, "Constitution");
			break;
		case A_INT:
			strcpy(statbuf, "Intelligence");
			break;
		case A_WIS:
			strcpy(statbuf, "Wisdom");
			break;
		case A_CHA:
			strcpy(statbuf, "Charisma");
			break;
		case A_MAX_INT_WIS:
			strcpy(statbuf, "Higher of intelligence and wisdom");
			break;
		case A_MAX_INT_CHA:
			strcpy(statbuf, "Higher of intelligence and charisma");
			break;
		case A_MAX_WIS_CHA:
			strcpy(statbuf, "Higher of wisdom and charisma");
			break;
		case A_MAX_INT_WIS_CHA:
			strcpy(statbuf, "Higher of intelligence, wisdom, and charisma");
			break;
		case A_AVG_INT_WIS:
			strcpy(statbuf, "Average of intelligence and wisdom");
			break;
		case A_AVG_INT_CHA:
			strcpy(statbuf, "Average of intelligence and charisma");
			break;
		case A_AVG_WIS_CHA:
			strcpy(statbuf, "Average of wisdom and charisma");
			break;
		case A_AVG_INT_WIS_CHA:
			strcpy(statbuf, "Average of intelligence, wisdom, and charisma");
			break;
		default:
			strcpy(statbuf, "Not applicable");
			break;
		}

		Sprintf(buf, "Attributes:   %s", statbuf);
		txt = buf;
		putstr(datawin, 0, txt);
	}

	/* Mana cost*/
	int manacost = get_spellbook_adjusted_mana_cost(booktype);
	if (manacost > 0)
	{
		Sprintf(buf2, "%d", manacost);
	}
	else
	{
		Sprintf(buf2, "None");
	}
	Sprintf(buf, "Mana cost:    %s", buf2);
	txt = buf;
	putstr(datawin, 0, txt);

	/* Cooldown */
	if (objects[booktype].oc_spell_cooldown > 0)
	{
		Sprintf(buf2, "%d round%s", objects[booktype].oc_spell_cooldown, objects[booktype].oc_spell_cooldown == 1 ? "" : "s");
	}
	else
	{
		Sprintf(buf2, "None");
	}
	Sprintf(buf, "Cooldown:     %s", buf2);
	txt = buf;
	putstr(datawin, 0, txt);

	/* DirType */
	if (objects[booktype].oc_dir > 0)
	{
		strcpy(buf2, "");
		switch (objects[booktype].oc_dir)
		{
		case NODIR:
			strcpy(buf2, "None");
			break;
		case IMMEDIATE:
			strcpy(buf2, "One target in selected direction");
			break;
		case RAY:
			if(objects[booktype].oc_aflags & S1_SPELL_EXPLOSION_EFFECT)
				strcpy(buf2, "Ray that explodes on hit");
			else
				strcpy(buf2, "Ray in selected direction");
			break;
		case TARGETED:
			strcpy(buf2, "Target selected on screen");
			break;
		case TOUCH:
			strcpy(buf2, "Touch");
			break;
		case IMMEDIATE_MULTIPLE_TARGETS:
			strcpy(buf2, "Multiple targets in selected direction");
			break;
		default:
			break;
		}
		Sprintf(buf, "Targeting:    %s", buf2);
		putstr(datawin, 0, txt);
	}

	/* Range */
	if (objects[booktype].oc_spell_range > 0)
	{
		Sprintf(buf, "Range:        %d'", objects[booktype].oc_spell_range * 5);
		txt = buf;
		putstr(datawin, 0, txt);
	}

	/* Radius */
	if (objects[booktype].oc_spell_radius > 0)
	{
		Sprintf(buf, "Radius:       %d'", objects[booktype].oc_spell_radius * 5);
		txt = buf;
		putstr(datawin, 0, txt);
	}

	/* Damage or Healing */
	if (objects[booktype].oc_spell_dmg_dice > 0 || objects[booktype].oc_spell_dmg_diesize > 0 || objects[booktype].oc_spell_dmg_plus > 0)
	{
		char plusbuf[BUFSZ];
		boolean maindiceprinted = FALSE;

		if (objects[booktype].oc_skill == P_HEALING_SPELL)
			Sprintf(buf, "Healing:      ");
		else
			Sprintf(buf, "Damage:       ");

		if (objects[booktype].oc_spell_dmg_dice > 0 && objects[booktype].oc_spell_dmg_diesize > 0)
		{
			maindiceprinted = TRUE;
			Sprintf(plusbuf, "%dd%d", objects[booktype].oc_spell_dmg_dice, objects[booktype].oc_spell_dmg_diesize);
			Strcat(buf, plusbuf);
		}

		if (objects[booktype].oc_spell_dmg_plus != 0)
		{
			if (maindiceprinted && objects[booktype].oc_spell_dmg_plus > 0)
			{
				Sprintf(plusbuf, "+");
				Strcat(buf, plusbuf);
			}
			Sprintf(plusbuf, "%d", objects[booktype].oc_spell_dmg_plus);
			Strcat(buf, plusbuf);
		}
		txt = buf;
		putstr(datawin, 0, txt);

	}

	if (objects[booktype].oc_dir_subtype > 0)
	{
		if (objects[booktype].oc_dir == RAY)
		{
			strcpy(buf2, "");
			strcpy(buf3, "Effect");
			switch (objects[booktype].oc_dir_subtype)
			{
			case RAY_MAGIC_MISSILE:
				strcpy(buf2, "Force damage");
				strcpy(buf3, "Damage");
				break;
			case RAY_FIRE:
				strcpy(buf2, "Fire damage");
				strcpy(buf3, "Damage");
				break;
			case RAY_COLD:
				strcpy(buf2, "Cold damage");
				strcpy(buf3, "Damage");
				break;
			case RAY_SLEEP:
				strcpy(buf2, "Sleeping");
				strcpy(buf3, "Effect");
				break;
			case RAY_DISINTEGRATION:
				strcpy(buf2, "Disintegration");
				strcpy(buf3, "Effect");
				break;
			case RAY_LIGHTNING:
				strcpy(buf2, "Lightning damage");
				strcpy(buf3, "Damage");
				break;
			case RAY_POISON_GAS:
				strcpy(buf2, "Poison gas");
				strcpy(buf3, "Damage");
				break;
			case RAY_ACID:
				strcpy(buf2, "Acid damage");
				strcpy(buf3, "Damage");
				break;
			case RAY_DEATH:
				strcpy(buf2, "Death");
				strcpy(buf3, "Effect");
				break;
			case RAY_DIGGING:
				strcpy(buf2, "Digs stone");
				strcpy(buf3, "Effect");
				break;
			case RAY_EVAPORATION:
				strcpy(buf2, "Evaporates water");
				strcpy(buf3, "Effect");
				break;
			default:
				break;
			}
			Sprintf(buf, "%s type:  %s", buf3, buf2);
			putstr(datawin, 0, txt);
		}
		else if (objects[booktype].oc_dir == NODIR)
		{
			strcpy(buf2, "");
			strcpy(buf3, "Effect");
			for(int j = 0; propertynames[j].prop_num; j++)
			{ 
				if (propertynames[j].prop_num == objects[booktype].oc_dir_subtype)
				{
					strcpy(buf2, propertynames[j].prop_name);
					*buf2 = highc(*buf2);
					break;
				}
			}
			if (strcmp(buf2, "") != 0) // Something else than ""
			{
				Sprintf(buf, "%s type:  %s", buf3, buf2);
				putstr(datawin, 0, txt);
			}
		}

	}

	/* Duration */
	if (objects[booktype].oc_spell_dur_dice > 0 || objects[booktype].oc_spell_dur_diesize > 0 || objects[booktype].oc_spell_dur_plus > 0)
	{
		char plusbuf[BUFSZ];
		boolean maindiceprinted = FALSE;

		Sprintf(buf, "Duration:     ");

		if (objects[booktype].oc_spell_dur_dice > 0 && objects[booktype].oc_spell_dur_diesize > 0)
		{
			maindiceprinted = TRUE;
			Sprintf(plusbuf, "%dd%d", objects[booktype].oc_spell_dur_dice, objects[booktype].oc_spell_dur_diesize);
			Strcat(buf, plusbuf); 
		}

		if (objects[booktype].oc_spell_dur_plus != 0)
		{
			if (maindiceprinted && objects[booktype].oc_spell_dur_plus > 0)
			{
				Sprintf(plusbuf, "+");
				Strcat(buf, plusbuf);
			}
			Sprintf(plusbuf, "%d", objects[booktype].oc_spell_dur_plus);
			Strcat(buf, plusbuf);
		}
		Sprintf(plusbuf, " round%s", (objects[booktype].oc_spell_dur_dice == 0 && objects[booktype].oc_spell_dur_diesize == 0 && objects[booktype].oc_spell_dur_plus == 1) ? "" : "s");
		Strcat(buf, plusbuf);

		txt = buf;
		putstr(datawin, 0, txt);
	}

	/* Skill chance */
	Sprintf(buf, "Train chance: %d%%", objects[booktype].oc_spell_skill_chance);
	txt = buf;
	putstr(datawin, 0, txt);

	/* Flags */
	if (objects[booktype].oc_aflags & S1_SPELL_BYPASSES_MAGIC_RESISTANCE)
	{
		Sprintf(buf, "Other:        %s", "Bypasses magic resistance");
		txt = buf;
		putstr(datawin, 0, txt);
	}


	/* Material components */
	if (objects[booktype].oc_material_components > 0)
	{
		Sprintf(buf2, "%d casting%s", matlists[objects[booktype].oc_material_components].spellsgained,
			matlists[objects[booktype].oc_material_components].spellsgained == 1 ? "" : "s");

		Sprintf(buf, "Material components - %s:", buf2);
		txt = buf;
		putstr(datawin, 0, txt);

		for (int j = 0; matlists[objects[booktype].oc_material_components].matcomp[j].objectid > 0; j++)
		{
			Sprintf(buf, " %2d - %s%s", (j + 1), domatcompname(&matlists[objects[booktype].oc_material_components].matcomp[j]),
				((matlists[objects[booktype].oc_material_components].matcomp[j].flags & MATCOMP_NOT_SPENT) ? " as a catalyst": ""));
			txt = buf;
			putstr(datawin, 0, txt);
		}

	}

	/* Description*/
	if (objects[booktype].oc_short_description)
	{
		/* One empty line here */
		Sprintf(buf, "");
		txt = buf;
		putstr(datawin, 0, txt);

		Sprintf(buf, "Description:");
		txt = buf;
		putstr(datawin, 0, txt);
		Sprintf(buf, objects[booktype].oc_short_description);
		txt = buf;
		putstr(datawin, 0, txt);

	}
	/*
	//This is for unimplemented longer description
	int i, subs = 0;
	const char* gang = (char*)0;
	const char** textp;
	const char* bufarray[] = { "Line 1","Line 2","Line 3","Line 4","Line 5", (char *)0 };
	textp = bufarray;

	for (i = 0; textp[i]; i++) {

		if (strstri(textp[i], "%s") != 0) {
			Sprintf(buf, textp[i]);
			txt = buf;
		}
		else
			txt = textp[i];
		putstr(datawin, 0, txt);
	}
	*/
	display_nhwindow(datawin, FALSE);
	destroy_nhwindow(datawin), datawin = WIN_ERR;

	return 0;
}

int
spelleffects(spell, atme)
int spell;
boolean atme;
{
	int energy, damage, chance, n; // , intell;
    int otyp, skill, role_skill, res = 0;
    boolean confused = (Confusion != 0);
    boolean physical_damage = FALSE;
    struct obj *pseudo;
    //coord cc;

    /*
     * Reject attempting to cast while stunned or with no free hands.
     * Already done in getspell() to stop casting before choosing
     * which spell, but duplicated here for cases where spelleffects()
     * gets called directly for ^T without intrinsic teleport capability
     * or #turn for non-priest/non-knight.
     * (There's no duplication of messages; when the rejection takes
     * place in getspell(), we don't get called.)
     */
    if (rejectcasting()) {
        return 0; /* no time elapses */
    }

	if (spellamount(spell) == 0) {
		You("do not have the spell's material components prepared.");
		return 0; /* no time elapses */
	}

	if (spellcooldownleft(spell) > 0) {
		You("cannot cast the spell before the cooldown has expired.");
		return 0; /* no time elapses */
	}

	//This might happen with amnesia etc., the spells no longer "age"
	if (spellknow(spell) <= 0)
	{
		You("cannot recall this spell anymore.");
		return 0;
	}

	/*
     * Spell casting no longer affects knowledge of the spell. A
     * decrement of spell knowledge is done every turn.
     */
	/*
    if (spellknow(spell) <= 0) {
        Your("knowledge of this spell is twisted.");
        pline("It invokes nightmarish images in your mind...");
        spell_backfire(spell);
        return 1;
    } else if (spellknow(spell) <= KEEN / 200) { // 100 turns left
        You("strain to recall the spell.");
    } else if (spellknow(spell) <= KEEN / 40) { // 500 turns left
        You("have difficulty remembering the spell.");
    } else if (spellknow(spell) <= KEEN / 20) { // 1000 turns left
        Your("knowledge of this spell is growing faint.");
    } else if (spellknow(spell) <= KEEN / 10) { // 2000 turns left
        Your("recall of this spell is gradually fading.");
    }
	*/
    /*
     *  Note: dotele() also calculates energy use and checks nutrition
     *  and strength requirements; it any of these change, update it too.
     */
    //energy = (spellev(spell) * 5);
	/* 5 <= energy <= 35 */
	energy = getspellenergycost(spell);

    /*if (u.uhunger <= 10 && spellid(spell) != SPE_DETECT_FOOD) {
        You("are too hungry to cast that spell.");
        return 0;
    } else*/
	if (ACURR(A_STR) < 4 && spellid(spell) != SPE_RESTORE_ABILITY) {
        You("lack the strength to cast spells.");
        return 0;
    } else if (check_capacity(
                "Your concentration falters while carrying so much stuff.")) {
        return 1;
    }

#if 0
	/* if the cast attempt is already going to fail due to insufficient
       energy (ie, u.uen < energy), the Amulet's drain effect won't kick
       in and no turn will be consumed; however, when it does kick in,
       the attempt may fail due to lack of energy after the draining, in
       which case a turn will be used up in addition to the energy loss */

    if (u.uhave.amulet && u.uen >= energy) {
        You_feel("the amulet draining your energy away.");
        /* this used to be 'energy += rnd(2 * energy)' (without 'res'),
           so if amulet-induced cost was more than u.uen, nothing
           (except the "don't have enough energy" message) happened
           and player could just try again (and again and again...);
           now we drain some energy immediately, which has a
           side-effect of not increasing the hunger aspect of casting */
        u.uen -= rnd(2 * energy);
        if (u.uen < 0)
            u.uen = 0;
        context.botl = 1;
        res = 1; /* time is going to elapse even if spell doesn't get cast */
    }
#endif

    if (energy > u.uen) {
        You("don't have enough energy to cast that spell.");
        return res;
    } 
	//else {
		/* If hero is a wizard, their current intelligence
			 * (bonuses + temporary + current)
			 * affects hunger reduction in casting a spell.
			 * 1. int = 17-18 no reduction
			 * 2. int = 16    1/4 hungr
			 * 3. int = 15    1/2 hungr
			 * 4. int = 1-14  normal reduction
			 * The reason for this is:
			 * a) Intelligence affects the amount of exertion
			 * in thinking.
			 * b) Wizards have spent their life at magic and
			 * understand quite well how to cast spells.
			 */
		/*
        if (spellid(spell) != SPE_DETECT_FOOD) {
            int hungr = energy * 2;

            
            intell = acurr(A_INT);
            if (!Role_if(PM_WIZARD))
                intell = 10;
            switch (intell) {
            case 25:
            case 24:
            case 23:
            case 22:
            case 21:
            case 20:
            case 19:
            case 18:
            case 17:
                hungr = 0;
                break;
            case 16:
                hungr /= 4;
                break;
            case 15:
                hungr /= 2;
                break;
            }
			*/
            /* don't put player (quite) into fainting from
             * casting a spell, particularly since they might
             * not even be hungry at the beginning; however,
             * this is low enough that they must eat before
             * casting anything else except detect food
             */
            //if (hungr > u.uhunger - 3)
            //    hungr = u.uhunger - 3;
            //morehungry(hungr);
        //}
    //}

	//One spell preparation is used, successful casting or not
	if(spellamount(spell) > 0)
		spellamount(spell)--;

	//Also, it goes under cooldown, successful or not
	int splcd = getspellcooldown(spell);
	if(splcd > 0)
		spellcooldownleft(spell) = splcd;
	else
		spellcooldownleft(spell) = 0;

	//Now check if successful
    chance = percent_success(spell);
    if (confused || (rnd(100) > chance)) {
        You("fail to cast the spell correctly.");
        u.uen -= energy / 2;
        context.botl = 1;
        return 1;
    }

    u.uen -= energy;
    context.botl = 1;
    exercise(A_WIS, TRUE);
    /* pseudo is a temporary "false" object containing the spell stats */
    pseudo = mksobj(spellid(spell), FALSE, FALSE, FALSE);
    pseudo->blessed = pseudo->cursed = 0;
    pseudo->quan = 20L; /* do not let useup get it */
    /*
     * Find the skill the hero has in a spell type category.
     * See spell_skilltype for categories.
     */
    otyp = pseudo->otyp;
    skill = spell_skilltype(otyp);
    role_skill = P_SKILL(skill);

    switch (otyp) {
    /*
     * At first spells act as expected.  As the hero increases in skill
     * with the appropriate spell type, some spells increase in their
     * effects, e.g. more damage, further distance, and so on, without
     * additional cost to the spellcaster.
     */
#if 0		
		if (throwspell(otyp)) {
            cc.x = u.dx;
            cc.y = u.dy;
			//One time only //n = rnd(8) + 1;
			int shots = 1;
			if(otyp == SPE_METEOR_SWARM)
				shots = d(objects[otyp].oc_spell_dur_dice, objects[otyp].oc_spell_dur_diesize) + objects[otyp].oc_spell_dur_plus;

			for (n = 0; n < shots; n++) {
                if (!u.dx && !u.dy && !u.dz) {
                    if ((damage = zapyourself(pseudo, TRUE)) != 0) {
                        char buf[BUFSZ];
                        Sprintf(buf, "zapped %sself with a spell",
                                uhim());
                        losehp(damage, buf, NO_KILLER_PREFIX);
                    }
                } else {
					if (otyp == SPE_METEOR_SWARM)
						You("shoot a meteor!");

                    explode(u.dx, u.dy,
                            objects[otyp].oc_dir_subtype,
							d(objects[otyp].oc_spell_dmg_dice, objects[otyp].oc_spell_dmg_diesize) + objects[otyp].oc_spell_dmg_plus,
							otyp, 0,
                            subdirtype2explosiontype(objects[otyp].oc_dir_subtype));
                }
                u.dx = cc.x + rnd(3) - 2;
                u.dy = cc.y + rnd(3) - 2;
                if (!isok(u.dx, u.dy) || !cansee(u.dx, u.dy)
                    || IS_STWALL(levl[u.dx][u.dy].typ) || u.uswallow) {
                    /* Spell is reflected back to center */
                    u.dx = cc.x;
                    u.dy = cc.y;
                }
            }
        }
        break; */
#endif
    /* these spells are all duplicates of wand effects */
	case SPE_MAGIC_ARROW:
	case SPE_FORCE_BOLT:
        physical_damage = TRUE;
    /*FALLTHRU*/
	case SPE_FIREBALL:
	case SPE_FIRE_STORM:
	case SPE_METEOR_SWARM:
	case SPE_ICE_STORM:
	case SPE_THUNDERSTORM:
	case SPE_MAGICAL_IMPLOSION:
	case SPE_MAGIC_STORM:
	case SPE_FIRE_BOLT:
	case SPE_LIGHTNING_BOLT:
	case SPE_CONE_OF_COLD:
	case SPE_DEATHSPELL:
	case SPE_SLEEP:
    case SPE_MAGIC_MISSILE:
	case SPE_KNOCK:
	case SPE_SILENCE:
	case SPE_SLOW_MONSTER:
	case SPE_MASS_SLOW:
	case SPE_HASTE_MONSTER:
	case SPE_HOLD_MONSTER:
	case SPE_MASS_HOLD:
	case SPE_WIZARD_LOCK:
    case SPE_DIG:
	case SPE_LOWER_WATER:
	case SPE_FEAR:
	case SPE_TURN_UNDEAD:
	case SPE_NEGATE_UNDEATH:
	case SPE_BANISH_DEMON:
	case SPE_POLYMORPH:
    case SPE_TELEPORT_MONSTER:
    case SPE_CANCELLATION:
	case SPE_LOWER_MAGIC_RESISTANCE:
	case SPE_NEGATE_MAGIC_RESISTANCE:
	case SPE_FORBID_SUMMONING:
	case SPE_FINGER_OF_DEATH:
	case SPE_TOUCH_OF_DEATH:
	case SPE_TOUCH_OF_PETRIFICATION:
	case SPE_FLESH_TO_STONE:
	case SPE_GAZE_OF_MEDUSA:
	case SPE_SHOCKING_TOUCH:
	case SPE_BURNING_HANDS:
	case SPE_FREEZING_TOUCH:
	case SPE_CHARM_MONSTER:
	case SPE_DOMINATE_MONSTER:
	case SPE_POWER_WORD_KILL:
	case SPE_POWER_WORD_STUN:
	case SPE_POWER_WORD_BLIND:
	case SPE_RESURRECTION:
	case SPE_RAISE_MINOR_ZOMBIE:
	case SPE_CREATE_MINOR_MUMMY:
	case SPE_RAISE_GIANT_ZOMBIE:
	case SPE_CREATE_GIANT_MUMMY:
	case SPE_CREATE_DRACOLICH:
	case SPE_RAISE_SKELETON:
	case SPE_RAISE_SKELETON_WARRIOR:
	case SPE_RAISE_SKELETON_LORD:
	case SPE_RAISE_SKELETON_KING:
	case SPE_RAISE_GIANT_SKELETON:
	case SPE_DETECT_UNSEEN:
	case SPE_LIGHT:
	case SPE_BLACK_BLADE_OF_DISASTER:
	case SPE_MAGE_ARMOR:
	case SPE_ARMAGEDDON:
	case SPE_WISH:
	case SPE_TIME_STOP:
	case SPE_ANIMATE_AIR:
	case SPE_ANIMATE_EARTH:
	case SPE_ANIMATE_FIRE:
	case SPE_ANIMATE_WATER:
	case SPE_CREATE_GOLD_GOLEM:
	case SPE_CREATE_GLASS_GOLEM:
	case SPE_CREATE_GEMSTONE_GOLEM:
	case SPE_CREATE_SILVER_GOLEM:
	case SPE_CREATE_CLAY_GOLEM:
	case SPE_CREATE_STONE_GOLEM:
	case SPE_CREATE_IRON_GOLEM:
	case SPE_CREATE_WOOD_GOLEM:
	case SPE_CREATE_PAPER_GOLEM:
	case SPE_SUMMON_DEMON:
	case SPE_FAITHFUL_HOUND:
	case SPE_CALL_DEMOGORGON:
	case SPE_HEAVENLY_WARRIOR:
	case SPE_GUARDIAN_ANGEL:
	case SPE_DIVINE_MOUNT:
	case SPE_HEAVENLY_ARMY:
	case SPE_STICK_TO_SNAKE:
	case SPE_STICK_TO_COBRA:
	case SPE_CALL_HIERARCH_MODRON:
	case SPE_GREAT_YENDORIAN_SUMMONING:
	case SPE_CALL_GHOUL:
	case SPE_MASS_RAISE_ZOMBIE:
	case SPE_MASS_CREATE_MUMMY:
	case SPE_MASS_CREATE_DRACOLICH:
	case SPE_SPHERE_OF_ANNIHILATION:
	case SPE_CIRCLE_OF_FIRE:
	case SPE_CIRCLE_OF_FROST:
	case SPE_CIRCLE_OF_LIGHTNING:
	case SPE_CIRCLE_OF_MAGIC:
	case SPE_REPLENISH_UNDEATH:
	case SPE_GREATER_UNDEATH_REPLENISHMENT:
	case SPE_CURE_BLINDNESS:
	case SPE_CURE_SICKNESS:
	case SPE_CURE_PETRIFICATION:
	case SPE_MINOR_HEALING:
	case SPE_HEALING:
    case SPE_EXTRA_HEALING:
	case SPE_GREATER_HEALING:
	case SPE_FULL_HEALING:
	case SPE_DRAIN_LIFE:
	case SPE_PROBE_MONSTER:
	case SPE_STONE_TO_FLESH:
	case SPE_COMMUNE:
	case SPE_PRAYER:
	case SPE_ABSOLUTION:
	case SPE_ENLIGHTENMENT:
	case SPE_TELEPORT_SELF:
	case SPE_CONTROLLED_TELEPORT:
	case SPE_CIRCLE_OF_TELEPORTATION:
	case SPE_LEVEL_TELEPORT:
	case SPE_CONTROLLED_LEVEL_TELEPORT:
	case SPE_PORTAL:
		if (objects[otyp].oc_dir != NODIR)
		{
            if (otyp == SPE_MINOR_HEALING || otyp == SPE_HEALING || otyp == SPE_EXTRA_HEALING || otyp == SPE_GREATER_HEALING || otyp == SPE_FULL_HEALING
				|| otyp == SPE_REPLENISH_UNDEATH || otyp == SPE_GREATER_UNDEATH_REPLENISHMENT
				) {
                /* healing and extra healing are actually potion effects,
                   but they've been extended to take a direction like wands */
                if (role_skill >= P_SKILLED)
                    pseudo->blessed = 1;
            }
            if (atme) {
                u.dx = u.dy = u.dz = 0;
            } else if (!getdir((char *) 0)) {
                /* getdir cancelled, re-use previous direction */
                /*
                 * FIXME:  reusing previous direction only makes sense
                 * if there is an actual previous direction.  When there
                 * isn't one, the spell gets cast at self which is rarely
                 * what the player intended.  Unfortunately, the way
                 * spelleffects() is organized means that aborting with
                 * "nevermind" is not an option.
                 */
                pline_The("magical energy is released!");
            }
            if (!u.dx && !u.dy && !u.dz) {
                if ((damage = zapyourself(pseudo, TRUE)) != 0) {
                    char buf[BUFSZ];

                    Sprintf(buf, "zapped %sself with a spell", uhim());
                    if (physical_damage)
                        damage = Maybe_Half_Phys(damage);
                    losehp(damage, buf, NO_KILLER_PREFIX);
                }
			}
			else
			{
				if (otyp == SPE_METEOR_SWARM)
				{
					int shots = 1;
					shots = d(objects[otyp].oc_spell_dur_dice, objects[otyp].oc_spell_dur_diesize) + objects[otyp].oc_spell_dur_plus;

					for (n = 0; n < shots; n++)
					{
						You("shoot a meteor!");
						weffects(pseudo);
					}
				}
				else
					weffects(pseudo);

			}
        } else
            weffects(pseudo);
        update_inventory(); /* spell may modify inventory */
        break;

    /* these are all duplicates of scroll effects */
    case SPE_REMOVE_CURSE:
    case SPE_CONFUSE_MONSTER:
    case SPE_DETECT_FOOD:
    case SPE_MASS_FEAR:
    case SPE_IDENTIFY:
	case SPE_BLESS:
	case SPE_CURSE:
	case SPE_ENCHANT_ARMOR:
	case SPE_ENCHANT_WEAPON:
	case SPE_PROTECT_ARMOR:
	case SPE_PROTECT_WEAPON:
		/* high skill yields effect equivalent to blessed scroll */
        if (role_skill >= P_SKILLED)
            pseudo->blessed = 1;
    /*FALLTHRU*/
    case SPE_SPHERE_OF_CHARMING:
	case SPE_SPHERE_OF_DOMINATION:
	case SPE_MASS_CHARM:
	case SPE_MASS_DOMINATION:
	case SPE_MASS_SLEEP:
	case SPE_HOLY_WORD:
	case SPE_MAGIC_MAPPING:
	case SPE_DETECT_TRAPS:
	case SPE_STINKING_CLOUD:
	case SPE_CREATE_MONSTER:
        (void) seffects(pseudo);
        break;

    /* these are all duplicates of potion effects */
    case SPE_HASTE_SELF:
    case SPE_DETECT_TREASURE:
    case SPE_DETECT_MONSTERS:
    case SPE_LEVITATION:
    case SPE_RESTORE_ABILITY:
        /* high skill yields effect equivalent to blessed potion */
        if (role_skill >= P_SKILLED)
            pseudo->blessed = 1;
    /*FALLTHRU*/
    case SPE_INVISIBILITY:
        (void) peffects(pseudo);
        break;
    /* end of potion-like spells */

	case SPE_CREATE_FAMILIAR:
        (void) make_familiar((struct obj *) 0, u.ux, u.uy, FALSE);
        break;
	case SPE_SUMMONING_CALL:
		use_magic_whistle((struct obj*) 0);
		break;
	case SPE_CLAIRVOYANCE:
        if (!BClairvoyant) {
            if (role_skill >= P_SKILLED)
                pseudo->blessed = 1; /* detect monsters as well as map */
            do_vicinity_map(pseudo);
        /* at present, only one thing blocks clairvoyance */
        } else if (uarmh && uarmh->otyp == CORNUTHAUM)
            You("sense a pointy hat on top of your %s.", body_part(HEAD));
        break;
    case SPE_PROTECTION:
	case SPE_SHIELD:
	case SPE_BARKSKIN:
	case SPE_STONESKIN:
	case SPE_REFLECTION:
	case SPE_ANTI_MAGIC_SHELL:
	case SPE_PROTECTION_FROM_FIRE:
	case SPE_PROTECTION_FROM_COLD:
	case SPE_PROTECTION_FROM_LIGHTNING:
	case SPE_PROTECTION_FROM_ACID:
	case SPE_PROTECTION_FROM_POISON:
	case SPE_PROTECTION_FROM_LIFE_DRAINING:
	case SPE_PROTECTION_FROM_DEATH_MAGIC:
	case SPE_PROTECTION_FROM_DISINTEGRATION:
	case SPE_PROTECTION_FROM_SICKNESS:
	case SPE_PROTECTION_FROM_PETRIFICATION:
	case SPE_PROTECTION_FROM_LYCANTHROPY:
	case SPE_PROTECTION_FROM_CURSES:
	case SPE_WATER_BREATHING:
	case SPE_WATER_WALKING:
	case SPE_TELEPATHY:
	case SPE_MIRROR_IMAGE:
	case SPE_MASS_CONFLICT:
	case SPE_GLOBE_OF_INVULNERABILITY:
	case SPE_DIVINE_INTERVENTION:
		You("successfully cast \"%s\".", spellname(spell));
		addspellintrinsictimeout(otyp);
		break;
	case SPE_JUMPING:
        if (!jump(max(role_skill, 1)))
            pline1(nothing_happens);
        break;
	case SPE_COLD_ENCHANT_ITEM:
	case SPE_FIRE_ENCHANT_ITEM:
	case SPE_LIGHTNING_ENCHANT_ITEM:
	case SPE_DEATH_ENCHANT_ITEM:
	{
		const char enchant_objects[] = { ALL_CLASSES, ALLOW_NONE, 0 };
		struct obj* otmp;
		char buf[BUFSZ] = "";
		strcpy(buf, otyp == SPE_COLD_ENCHANT_ITEM ? "cold-enchant" :
			otyp == SPE_FIRE_ENCHANT_ITEM ? "fire-enchant" :
			otyp == SPE_LIGHTNING_ENCHANT_ITEM ? "lightning-enchant" :
			otyp == SPE_DEATH_ENCHANT_ITEM ? "death-enchant" :
			"enchant");

		otmp = getobj(enchant_objects, buf, 0, "");
		if (!otmp)
			return 0;

		if (inaccessible_equipment(otmp, buf, FALSE))
			return 0;

		if (otmp && otmp != &zeroobj) {
			switch (otyp)
			{
			case SPE_DEATH_ENCHANT_ITEM:
				if (is_elemental_enchantable(otmp) && is_deathenchantable(otmp))
				{
					You("enchant %s with death magic.", yname(otmp));
					otmp = elemental_enchant_quan(otmp, rnd(2), DEATH_ENCHANTMENT);
					prinv((char*)0, otmp, 0L);
					//otmp->elemental_enchantment = DEATH_ENCHANTMENT;
				}
				else
				{
					pline("%s in black energy for a moment.", Tobjnam(otmp, "flicker"));
				}
				break;
			case SPE_COLD_ENCHANT_ITEM:
				if (otmp->elemental_enchantment == DEATH_ENCHANTMENT)
				{
					pline("%s in blue for a moment, but then glows black.", Tobjnam(otmp, "flicker"));
					break;
				}
				if (otmp->elemental_enchantment == FIRE_ENCHANTMENT)
				{
					pline("The cold energies dispel the flaming enchantment on %s.", yname(otmp));
					otmp->elemental_enchantment = 0;
					break;
				}

				if (is_elemental_enchantable(otmp))
				{
					You("enchant %s with freezing magic.", yname(otmp));
					otmp = elemental_enchant_quan(otmp, 20, COLD_ENCHANTMENT);
					prinv((char*)0, otmp, 0L);
					//otmp->elemental_enchantment = COLD_ENCHANTMENT;
				}
				else
				{
					pline("%s in blue for a moment.", Tobjnam(otmp, "flicker"));
				}
				break;
			case SPE_FIRE_ENCHANT_ITEM:
				if (otmp->elemental_enchantment == DEATH_ENCHANTMENT)
				{
					pline("%s in red for a moment, but then glows black.", Tobjnam(otmp, "flicker"));
					break;
				}
				if (otmp->elemental_enchantment == COLD_ENCHANTMENT)
				{
					pline("The fiery energies dispel the freezing enchantment on %s.", yname(otmp));
					otmp->elemental_enchantment = 0;
					break;
				}

				if (is_elemental_enchantable(otmp))
				{
					You("enchant %s with fire magic.", yname(otmp));
					otmp = elemental_enchant_quan(otmp, 10, FIRE_ENCHANTMENT);
					prinv((char*)0, otmp, 0L);
					//otmp->elemental_enchantment = FIRE_ENCHANTMENT;
				}
				else
				{
					pline("%s in red for a moment.", Tobjnam(otmp, "flicker"));
				}
				break;
			case SPE_LIGHTNING_ENCHANT_ITEM:
				if (otmp->elemental_enchantment == DEATH_ENCHANTMENT)
				{
					pline("%s in blue for a moment, but then glows black.", Tobjnam(otmp, "flicker"));
					break;
				}
				if (is_elemental_enchantable(otmp))
				{
					otmp = elemental_enchant_quan(otmp, 5, LIGHTNING_ENCHANTMENT);
					You("enchant %s with lightning magic.", yname(otmp));
					prinv((char*)0, otmp, 0L);
					//otmp->elemental_enchantment = LIGHTNING_ENCHANTMENT;
				}
				else
				{
					pline("%s in blue for a moment.", Tobjnam(otmp, "flicker"));
				}
				break;
			}
		}
		break;
	}
	default:
        impossible("Unknown spell %d attempted.", spell);
        obfree(pseudo, (struct obj *) 0);
        return 0;
    }


    /* gain skill for successful cast */
	int pointsmultiplier = max(0, spellskillchance(spell) / 100);
	int randomizedchance = spellskillchance(spell) - pointsmultiplier * 100;

	if (rn2(100) < randomizedchance)
		pointsmultiplier++;

	if (pointsmultiplier > 0)
		use_skill(skill, (spellev(spell) + 2) * pointsmultiplier);

	int result = 1;
	if (pseudo->otyp == SPE_PROBE_MONSTER)
		result = 0;

    obfree(pseudo, (struct obj *) 0); /* now, get rid of it */

    return result;
}

void
addspellintrinsictimeout(otyp)
int otyp;
{
	if (otyp <= 0)
		return;

	if (objects[otyp].oc_dir_subtype <= 0 || objects[otyp].oc_dir_subtype > LAST_PROP)
		return;

	boolean hadbefore = u.uprops[objects[otyp].oc_dir_subtype].intrinsic || u.uprops[objects[otyp].oc_dir_subtype].extrinsic;
	long duration = d(objects[otyp].oc_spell_dur_dice, objects[otyp].oc_spell_dur_diesize) + objects[otyp].oc_spell_dur_plus;
	long oldtimeout = u.uprops[objects[otyp].oc_dir_subtype].intrinsic & TIMEOUT;
	long oldprop = u.uprops[objects[otyp].oc_dir_subtype].intrinsic & ~TIMEOUT;

	if (oldtimeout > duration || duration <= 0)
		return;

	u.uprops[objects[otyp].oc_dir_subtype].intrinsic = oldprop | duration;
	if (!hadbefore)
	{
		switch(objects[otyp].oc_dir_subtype)
		{
		case REFLECTING:
			Your("skin feels more reflecting than before.");
			break;
		case FIRE_RES:
			Your("skin feels less prone to burning than before.");
			break;
		case COLD_RES:
			Your("skin feels less prone to frostbites than before.");
			break;
		case SHOCK_RES:
			Your("skin feels less prone to electricity than before.");
			break;
		case DISINT_RES:
			Your("body feels firmer than before.");
			break;
		case POISON_RES:
			You_feel("more healthy than before.");
			break;
		case ACID_RES:
			Your("skin feels less prone to acid than before.");
			break;
		case STONE_RES:
			Your("skin feels limber than before.");
			break;
		case DRAIN_RES:
			You_feel("less suspectible to draining than before.");
			break;
		case SICK_RES:
			You_feel("unbothered by disease agents.");
			break;
		case INVULNERABLE:
			Your("skin feels less prone to damage than before.");
			break;
		case ANTIMAGIC:
			You_feel("more protected from magic.");
			break;
		case DEATH_RES:
			Your("soul's silver cord feels thicker than before.");
			break;
		case CHARM_RES:
			You_feel("more firm about your own motivations.");
			break;
		case FEAR_RES:
			You_feel("more courageous.");
			break;
		case MIND_SHIELDING:
			You_feel("shielded from mental detection.");
			break;
		case LYCANTHROPY_RES:
			You_feel("more protected from lycanthropy.");
			break;
		case CURSE_RES:
			You_feel("more protected from curses.");
			break;
		case LIFESAVED:
			You_feel("less mortal than before.");
			break;
		case DETECT_MONSTERS:
			You_feel("sensitive to the presence of monsters.");
			break;
		case BLIND_TELEPAT:
			You_feel("telepathic when blind.");
			break;
		case TELEPAT:
			You_feel("telepathic.");
			break;
		case XRAY_VISION:
			You("can see through walls.");
			break;
		case WATER_WALKING:
			You_feel("like you could walk on water.");
			break;
		case MAGICAL_BREATHING:
			You_feel("like you breathe in water.");
			break;
		case DISPLACED:
			Your("image is duplicated by a displaced double.");
			break;
		case CONFLICT:
			Your("neighborhood feels more quarrelsome than before.");
			break;
		case PROTECTION:
			You_feel("protected!");
			break;
		case MAGICAL_SHIELDING:
			You_feel("shielded!");
			break;
		case MAGICAL_BARKSKIN:
			Your("skin hardens into bark!");
			break;
		case MAGICAL_STONESKIN:
			Your("skin hardens into stone!");
			break;
		default:
			break;
		}
	}

	
	return;
}

int
subdirtype2explosiontype(subdir)
int subdir;
{
	int expltype = 0;
	switch (subdir)
	{
	case RAY_COLD:
	case RAY_WND_COLD:
		expltype = EXPL_FROSTY;
		break;
	case RAY_FIRE:
	case RAY_WND_FIRE:
		expltype = EXPL_FIERY;
		break;
	case RAY_POISON_GAS:
	case RAY_WND_POISON_GAS:
	case RAY_ACID:
	case RAY_WND_ACID:
		expltype = EXPL_NOXIOUS;
		break;
	case RAY_SLEEP:
	case RAY_WND_SLEEP:
	case RAY_DEATH:
	case RAY_WND_DEATH:
	case RAY_DISINTEGRATION:
	case RAY_WND_DISINTEGRATION:
	default:
		expltype = EXPL_MAGICAL;
		break;
	}

	return expltype;
}

/*ARGSUSED*/
STATIC_OVL boolean
spell_aim_step(arg, x, y)
genericptr_t arg UNUSED;
int x, y;
{
    if (!isok(x,y))
        return FALSE;
    if (!ZAP_POS(levl[x][y].typ)
        && !(IS_DOOR(levl[x][y].typ) && (levl[x][y].doormask & D_ISOPEN)))
        return FALSE;
    return TRUE;
}

/* Choose location where spell takes effect. */
STATIC_OVL int
throwspell(otyp)
int otyp;
{
    coord cc, uc;
    struct monst *mtmp;
	int range = 10;
	if (otyp >= 0 && objects[otyp].oc_spell_range > 0)
		range = objects[otyp].oc_spell_range;

    if (u.uinwater) {
        pline("You're joking!  In this weather?");
        return 0;
    } else if (Is_waterlevel(&u.uz)) {
        You("had better wait for the sun to come out.");
        return 0;
    }

    pline("Where do you want to cast the spell?");
    cc.x = u.ux;
    cc.y = u.uy;
    if (getpos(&cc, TRUE, "the desired position") < 0)
        return 0; /* user pressed ESC */
    /* The number of moves from hero to where the spell drops.*/
    if (distmin(u.ux, u.uy, cc.x, cc.y) > range) {
        pline_The("spell dissipates over the distance!");
        return 0;
    } else if (u.uswallow) {
        pline_The("spell is cut short!");
        exercise(A_WIS, FALSE); /* What were you THINKING! */
        u.dx = 0;
        u.dy = 0;
        return 1;
    } else if ((!cansee(cc.x, cc.y)
                && (!(mtmp = m_at(cc.x, cc.y)) || !canspotmon(mtmp)))
               || IS_STWALL(levl[cc.x][cc.y].typ)) {
        Your("mind fails to lock onto that location!");
        return 0;
    }

    uc.x = u.ux;
    uc.y = u.uy;

    walk_path(&uc, &cc, spell_aim_step, (genericptr_t) 0);

    u.dx = cc.x;
    u.dy = cc.y;
    return 1;
}

/* add/hide/remove/unhide teleport-away on behalf of dotelecmd() to give
   more control to behavior of ^T when used in wizard mode */
int
tport_spell(what)
int what;
{
    static struct tport_hideaway {
        struct spell savespell;
        int tport_indx;
    } save_tport;
    int i;
/* also defined in teleport.c */
#define NOOP_SPELL  0
#define HIDE_SPELL  1
#define ADD_SPELL   2
#define UNHIDESPELL 3
#define REMOVESPELL 4

    for (i = 0; i < MAXSPELL; i++)
        if (spellid(i) == SPE_TELEPORT_MONSTER || spellid(i) == NO_SPELL)
            break;
    if (i == MAXSPELL) {
        impossible("tport_spell: spellbook full");
        /* wizard mode ^T is not able to honor player's menu choice */
    } else if (spellid(i) == NO_SPELL) {
        if (what == HIDE_SPELL || what == REMOVESPELL) {
            save_tport.tport_indx = MAXSPELL;
        } else if (what == UNHIDESPELL) {
            /*assert( save_tport.savespell.sp_id == SPE_TELEPORT_MONSTER );*/
            spl_book[save_tport.tport_indx] = save_tport.savespell;
            save_tport.tport_indx = MAXSPELL; /* burn bridge... */
        } else if (what == ADD_SPELL) {
            save_tport.savespell = spl_book[i];
            save_tport.tport_indx = i;
            spl_book[i].sp_id = SPE_TELEPORT_MONSTER;
            spl_book[i].sp_lev = objects[SPE_TELEPORT_MONSTER].oc_spell_level;
			spl_book[i].sp_matcomp = objects[SPE_TELEPORT_MONSTER].oc_material_components;
			spl_book[i].sp_cooldownlength = objects[SPE_TELEPORT_MONSTER].oc_spell_cooldown;
			spl_book[i].sp_cooldownleft = 0;
			spl_book[i].sp_skillchance = objects[SPE_TELEPORT_MONSTER].oc_spell_skill_chance;
			spl_book[i].sp_amount = -1; //Infinite??
			spl_book[i].sp_know = KEEN;
            return REMOVESPELL; /* operation needed to reverse */
        }
    } else { /* spellid(i) == SPE_TELEPORT_MONSTER */
        if (what == ADD_SPELL || what == UNHIDESPELL) {
            save_tport.tport_indx = MAXSPELL;
        } else if (what == REMOVESPELL) {
            /*assert( i == save_tport.tport_indx );*/
            spl_book[i] = save_tport.savespell;
            save_tport.tport_indx = MAXSPELL;
        } else if (what == HIDE_SPELL) {
            save_tport.savespell = spl_book[i];
            save_tport.tport_indx = i;
            spl_book[i].sp_id = NO_SPELL;
            return UNHIDESPELL; /* operation needed to reverse */
        }
    }
    return NOOP_SPELL;
}

/* forget a random selection of known spells due to amnesia;
   they used to be lost entirely, as if never learned, but now we
   just set the memory retention to zero so that they can't be cast */
void
losespells()
{
    int n, nzap, i;

    /* in case reading has been interrupted earlier, discard context */
    context.spbook.book = 0;
    context.spbook.o_id = 0;
    /* count the number of known spells */
    for (n = 0; n < MAXSPELL; ++n)
        if (spellid(n) == NO_SPELL)
            break;

    /* lose anywhere from zero to all known spells;
       if confused, use the worse of two die rolls */
    nzap = rn2(n + 1);
    if (Confusion) {
        i = rn2(n + 1);
        if (i > nzap)
            nzap = i;
    }
    /* good Luck might ameliorate spell loss */
    if (nzap > 1 && !rnl(7))
        nzap = rnd(nzap);

    /*
     * Forget 'nzap' out of 'n' known spells by setting their memory
     * retention to zero.  Every spell has the same probability to be
     * forgotten, even if its retention is already zero.
     *
     * Perhaps we should forget the corresponding book too?
     *
     * (3.4.3 removed spells entirely from the list, but always did
     * so from its end, so the 'nzap' most recently learned spells
     * were the ones lost by default.  Player had sort control over
     * the list, so could move the most useful spells to front and
     * only lose them if 'nzap' turned out to be a large value.
     *
     * Discarding from the end of the list had the virtue of making
     * casting letters for lost spells become invalid and retaining
     * the original letter for the ones which weren't lost, so there
     * was no risk to the player of accidentally casting the wrong
     * spell when using a letter that was in use prior to amnesia.
     * That wouldn't be the case if we implemented spell loss spread
     * throughout the list of known spells; every spell located past
     * the first lost spell would end up with new letter assigned.)
     */
    for (i = 0; nzap > 0; ++i) {
        /* when nzap is small relative to the number of spells left,
           the chance to lose spell [i] is small; as the number of
           remaining candidates shrinks, the chance per candidate
           gets bigger; overall, exactly nzap entries are affected */
        if (rn2(n - i) < nzap) {
            /* lose access to spell [i] */
            spellknow(i) = 0;
#if 0
            /* also forget its book */
            forget_single_object(spellid(i));
#endif
            /* and abuse wisdom */
            exercise(A_WIS, FALSE);
            /* there's now one less spell slated to be forgotten */
            --nzap;
        }
    }
}

/*
 * Allow player to sort the list of known spells.  Manually swapping
 * pairs of them becomes very tedious once the list reaches two pages.
 *
 * Possible extensions:
 *      provide means for player to control ordering of skill classes;
 *      provide means to supply value N such that first N entries stick
 *      while rest of list is being sorted;
 *      make chosen sort order be persistent such that when new spells
 *      are learned, they get inserted into sorted order rather than be
 *      appended to the end of the list?
 */
enum spl_sort_types {
    SORTBY_LETTER = 0,
    SORTBY_ALPHA,
    SORTBY_LVL_LO,
    SORTBY_LVL_HI,
    SORTBY_SKL_AL,
    SORTBY_SKL_LO,
    SORTBY_SKL_HI,
    SORTBY_CURRENT,
    SORTRETAINORDER,

    NUM_SPELL_SORTBY
};

static const char *spl_sortchoices[NUM_SPELL_SORTBY] = {
    "by casting letter",
    "alphabetically",
    "by level, low to high",
    "by level, high to low",
    "by skill group, alphabetized within each group",
    "by skill group, low to high level within group",
    "by skill group, high to low level within group",
    "maintain current ordering",
    /* a menu choice rather than a sort choice */
    "reassign casting letters to retain current order",
};
static int spl_sortmode = 0;   /* index into spl_sortchoices[] */
static int *spl_orderindx = 0; /* array of spl_book[] indices */

/* qsort callback routine */
STATIC_PTR int CFDECLSPEC
spell_cmp(vptr1, vptr2)
const genericptr vptr1;
const genericptr vptr2;
{
    /*
     * gather up all of the possible parameters except spell name
     * in advance, even though some might not be needed:
     *  indx. = spl_orderindx[] index into spl_book[];
     *  otyp. = spl_book[] index into objects[];
     *  levl. = spell level;
     *  skil. = skill group aka spell class.
     */
    int indx1 = *(int *) vptr1, indx2 = *(int *) vptr2,
        otyp1 = spl_book[indx1].sp_id, otyp2 = spl_book[indx2].sp_id,
        levl1 = objects[otyp1].oc_spell_level, levl2 = objects[otyp2].oc_spell_level,
        skil1 = objects[otyp1].oc_skill, skil2 = objects[otyp2].oc_skill;

    switch (spl_sortmode) {
    case SORTBY_LETTER:
        return indx1 - indx2;
    case SORTBY_ALPHA:
        break;
    case SORTBY_LVL_LO:
        if (levl1 != levl2)
            return levl1 - levl2;
        break;
    case SORTBY_LVL_HI:
        if (levl1 != levl2)
            return levl2 - levl1;
        break;
    case SORTBY_SKL_AL:
        if (skil1 != skil2)
            return skil1 - skil2;
        break;
    case SORTBY_SKL_LO:
        if (skil1 != skil2)
            return skil1 - skil2;
        if (levl1 != levl2)
            return levl1 - levl2;
        break;
    case SORTBY_SKL_HI:
        if (skil1 != skil2)
            return skil1 - skil2;
        if (levl1 != levl2)
            return levl2 - levl1;
        break;
    case SORTBY_CURRENT:
    default:
        return (vptr1 < vptr2) ? -1
                               : (vptr1 > vptr2); /* keep current order */
    }
    /* tie-breaker for most sorts--alphabetical by spell name */
    return strcmpi(OBJ_NAME(objects[otyp1]), OBJ_NAME(objects[otyp2]));
}

/* sort the index used for display order of the "view known spells"
   list (sortmode == SORTBY_xxx), or sort the spellbook itself to make
   the current display order stick (sortmode == SORTRETAINORDER) */
STATIC_OVL void
sortspells()
{
    int i;
#if defined(SYSV) || defined(DGUX)
    unsigned n;
#else
    int n;
#endif

    if (spl_sortmode == SORTBY_CURRENT)
        return;
    for (n = 0; n < MAXSPELL && spellid(n) != NO_SPELL; ++n)
        continue;
    if (n < 2)
        return; /* not enough entries to need sorting */

    if (!spl_orderindx) {
        /* we haven't done any sorting yet; list is in casting order */
        if (spl_sortmode == SORTBY_LETTER /* default */
            || spl_sortmode == SORTRETAINORDER)
            return;
        /* allocate enough for full spellbook rather than just N spells */
        spl_orderindx = (int *) alloc(MAXSPELL * sizeof(int));
        for (i = 0; i < MAXSPELL; i++)
            spl_orderindx[i] = i;
    }

    if (spl_sortmode == SORTRETAINORDER) {
        struct spell tmp_book[MAXSPELL];

        /* sort spl_book[] rather than spl_orderindx[];
           this also updates the index to reflect the new ordering (we
           could just free it since that ordering becomes the default) */
        for (i = 0; i < MAXSPELL; i++)
            tmp_book[i] = spl_book[spl_orderindx[i]];
        for (i = 0; i < MAXSPELL; i++)
            spl_book[i] = tmp_book[i], spl_orderindx[i] = i;
        spl_sortmode = SORTBY_LETTER; /* reset */
        return;
    }

    /* usual case, sort the index rather than the spells themselves */
    qsort((genericptr_t) spl_orderindx, n, sizeof *spl_orderindx, spell_cmp);
    return;
}

/* called if the [sort spells] entry in the view spells menu gets chosen */
STATIC_OVL boolean
spellsortmenu()
{
    winid tmpwin;
    menu_item *selected;
    anything any;
    char let;
    int i, n, choice;

    tmpwin = create_nhwindow(NHW_MENU);
    start_menu(tmpwin);
    any = zeroany; /* zero out all bits */

    for (i = 0; i < SIZE(spl_sortchoices); i++) {
        if (i == SORTRETAINORDER) {
            let = 'z'; /* assumes fewer than 26 sort choices... */
            /* separate final choice from others with a blank line */
            any.a_int = 0;
            add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, "",
                     MENU_UNSELECTED);
        } else {
            let = 'a' + i;
        }
        any.a_int = i + 1;
        add_menu(tmpwin, NO_GLYPH, &any, let, 0, ATR_NONE, spl_sortchoices[i],
                 (i == spl_sortmode) ? MENU_SELECTED : MENU_UNSELECTED);
    }
    end_menu(tmpwin, "View known spells list sorted");

    n = select_menu(tmpwin, PICK_ONE, &selected);
    destroy_nhwindow(tmpwin);
    if (n > 0) {
        choice = selected[0].item.a_int - 1;
        /* skip preselected entry if we have more than one item chosen */
        if (n > 1 && choice == spl_sortmode)
            choice = selected[1].item.a_int - 1;
        free((genericptr_t) selected);
        spl_sortmode = choice;
        return TRUE;
    }
    return FALSE;
}

/* the '+' command -- view known spells */
int
dovspell()
{
    char qbuf[QBUFSZ];
    int splnum, othnum;
    struct spell spl_tmp;

    if (spellid(0) == NO_SPELL)
	{
        You("don't know any spells right now.");
    } 
	else 
	{
        while (dospellmenu("Choose a spell to reorder", SPELLMENU_VIEW, &splnum))
		{
            if (splnum == SPELLMENU_SORT) 
			{
                if (spellsortmenu())
                    sortspells();
            } 
			else 
			{
                Sprintf(qbuf, "Reordering spells; swap '%c' with", spellet(splnum));
                if (!dospellmenu(qbuf, splnum, &othnum))
                    break;

                spl_tmp = spl_book[splnum];
                spl_book[splnum] = spl_book[othnum];
                spl_book[othnum] = spl_tmp;
            }
        }
    }
    if (spl_orderindx) 
	{
        free((genericptr_t) spl_orderindx);
        spl_orderindx = 0;
    }
    spl_sortmode = SORTBY_LETTER; /* 0 */
    return 0;
}

STATIC_OVL boolean
dospellmenu(prompt, splaction, spell_no)
const char *prompt;
int splaction; /* SPELLMENU_CAST, SPELLMENU_VIEW, or spl_book[] index */
int *spell_no;
{
	winid tmpwin;
	int i, n, how, splnum;
	char buf[BUFSZ], descbuf[BUFSZ], fmt[BUFSZ];
	char colorbufs[BUFSZ][MAXSPELL];
	int colorbufcnt = 0;
    //const char *fmt;
    menu_item *selected;
    anything any;
	const char* nodesc = "(No short description)";

	for (int j = 0; j < MAXSPELL; j++)
		strcpy(colorbufs[j], "");

    tmpwin = create_nhwindow(NHW_MENU);
    start_menu(tmpwin);
    any = zeroany; /* zero out all bits */

	int hotkeys[11] = { 0 };

	/* -1 means no hotkey; otherwise, values can be 0... MAXSPELL - 1, indicating the spell learnt */
	for (i = 0; i <= 10; i++)
		hotkeys[i] = -1;

	for (i = 0; i < MAXSPELL && spellid(i) != NO_SPELL; i++)
	{
		if (spellhotkey(i) > 0 && spellhotkey(i) <= 10)
			hotkeys[spellhotkey(i)] = i;
	}

    /*
     * The correct spacing of the columns when not using
     * tab separation depends on the following:
     * (1) that the font is monospaced, and
     * (2) that selection letters are pre-pended to the
     * given string and are of the form "a - ".
     */
	if (splaction == SPELLMENU_DETAILS || splaction == SPELLMENU_VIEW || splaction == SPELLMENU_SORT || splaction >= 0)
	{
		int maxlen = 15;
		int maxnamelen = 0;

		for (i = 0; i < min(MAXSPELL, 52) && spellid(i) != NO_SPELL; i++)
		{
			int desclen = 0;
			splnum = !spl_orderindx ? i : spl_orderindx[i];
			if (objects[spellid(splnum)].oc_short_description)
				desclen = strlen(objects[spellid(splnum)].oc_short_description);
			else
				desclen = strlen(nodesc);
			if (desclen > maxlen)
				maxlen = desclen;

			int namelen = 0;
			namelen = strlen(spellname(splnum));
			if (namelen > maxnamelen)
				maxnamelen = namelen;

		}

		int extraspaces = maxlen - 15;
		if (extraspaces > 42)
			extraspaces = 42;

		int extraleftforname_and_hotkey = 42 - extraspaces;
		int hotkey_chars = 2;
		int namelength = max(10, min(maxnamelen, 20 + extraleftforname_and_hotkey - hotkey_chars));


		char spacebuf[BUFSZ] = "";

		for (i = 0; i < extraspaces; i++)
			Strcat(spacebuf, " ");

		if (!iflags.menu_tab_sep) {
			Sprintf(fmt, "%%-%ds#  Description    %%s", namelength + 6);
			Sprintf(buf, fmt, "    Name", spacebuf);
		}
		else {
			Sprintf(buf, "Name\tH\tDescription");
		}

		add_menu(tmpwin, NO_GLYPH, &any, 0, 0, iflags.menu_headings, buf,
			MENU_UNSELECTED);


		for (i = 0; i < min(MAXSPELL, 52) && spellid(i) != NO_SPELL; i++) {
			splnum = !spl_orderindx ? i : spl_orderindx[i];
			char shortenedname[BUFSZ] = "";
			char fullname[BUFSZ] = "";
			char categorybuf[BUFSZ] = "";

			if (!iflags.menu_tab_sep) {
				Sprintf(fmt, "%%-%ds  %%c  %%s", namelength);
			}
			else {
				strcpy(fmt, "%s\t%c\t%s");
			}

			Sprintf(fullname, "%s", spellname(splnum));

			//Spell name
			if (strlen(fullname) > (size_t)(namelength))
				strncpy(shortenedname, fullname, (size_t)(namelength));
			else
				strcpy(shortenedname, fullname);


			//Shorten description, if needed
			char shorteneddesc[BUFSZ] = "";
			char fulldesc[BUFSZ];

			if(objects[spellid(splnum)].oc_short_description)
			{
				strcpy(fulldesc, objects[spellid(splnum)].oc_short_description);

				if (strlen(fulldesc) > 57)
					strncpy(shorteneddesc, fulldesc, 57);
				else
					strcpy(shorteneddesc, fulldesc);

				strcpy(descbuf, shorteneddesc);
			}
			else
				strcpy(descbuf, nodesc);

			char hotkeychar = ' ';
			if (spellhotkey(i) == 10)
				hotkeychar = '0';
			else if(spellhotkey(i) > 0)
				hotkeychar = spellhotkey(i) + '0';

			//Finally print
			if (spellknow(splnum) <= 0)
				Sprintf(buf, fmt, shortenedname, hotkeychar,  "(You cannot recall this spell)");
			else
				Sprintf(buf, fmt, shortenedname, hotkeychar, descbuf);

			any.a_int = splnum + 1; /* must be non-zero */
			add_menu(tmpwin, NO_GLYPH, &any, spellet(splnum), 0, ATR_NONE, buf,
				(splnum == splaction) ? MENU_SELECTED : MENU_UNSELECTED);

		}
	}
	else if (splaction == SPELLMENU_PREPARE)
	{
		int maxlen = 23;
		int maxnamelen = 0;

		for (i = 0; i < min(MAXSPELL, 52) && spellid(i) != NO_SPELL; i++)
		{
			int desclen = 0;
			int namelen = 0;
			splnum = !spl_orderindx ? i : spl_orderindx[i];
			desclen = strlen(matlists[spellmatcomp(splnum)].description_short);
			namelen = strlen(spellname(splnum));
			if (desclen > maxlen)
				maxlen = desclen;
			if (namelen > maxnamelen)
				maxnamelen = namelen;
		}

		int extraspaces = maxlen - 23;
		if (extraspaces > 16)
			extraspaces = 16;

		int extraleftforname = 16 - extraspaces;
		int namelength = max(10, min(maxnamelen, 20 + extraleftforname));


		boolean hotkeyfound = FALSE;
		add_spell_prepare_menu_heading(tmpwin, namelength, extraspaces, FALSE);

		for (i = 1; i <= 10; i++)
		{
			if (hotkeys[i] >= 0)
			{
				add_spell_prepare_menu_item(tmpwin, hotkeys[i], splaction, namelength, extraspaces, TRUE);
				hotkeyfound = TRUE;
			}
		}
		if (hotkeyfound)
		{
			add_spell_prepare_menu_heading(tmpwin, namelength, extraspaces, TRUE);
		}

		for (i = 0; i < min(MAXSPELL, 52) && spellid(i) != NO_SPELL; i++)
		{
			add_spell_prepare_menu_item(tmpwin, i, splaction, namelength, extraspaces, FALSE);
		}
	}
	else
	{
		int maxnamelen = 0;

		for (i = 0; i < min(MAXSPELL, 52) && spellid(i) != NO_SPELL; i++)
		{
			int namelen = 0;
			splnum = !spl_orderindx ? i : spl_orderindx[i];
			namelen = strlen(spellname(splnum));
			if (namelen > maxnamelen)
				maxnamelen = namelen;
		}

		int namelength = max(10, min(maxnamelen, 27));

		boolean hotkeyfound = FALSE;
		add_spell_cast_menu_heading(tmpwin, namelength, FALSE);
		for (i = 1; i <= 10; i++)
		{
			if (hotkeys[i] >= 0)
			{
				add_spell_cast_menu_item(tmpwin, hotkeys[i], splaction, namelength, colorbufs[colorbufcnt], &colorbufcnt, TRUE);
				hotkeyfound = TRUE;
			}
		}
		if (hotkeyfound)
		{
			add_spell_cast_menu_heading(tmpwin, namelength, TRUE);
		}

		for (i = 0; i < min(MAXSPELL, 52) && spellid(i) != NO_SPELL; i++)
		{
			add_spell_cast_menu_item(tmpwin, i, splaction, namelength, colorbufs[colorbufcnt], &colorbufcnt, FALSE);
		}
	}

    how = PICK_ONE;
    if (splaction == SPELLMENU_VIEW) {
        if (spellid(1) == NO_SPELL) {
            /* only one spell => nothing to swap with */
            how = PICK_NONE;
        } else {
            /* more than 1 spell, add an extra menu entry */
            any.a_int = SPELLMENU_SORT + 1;
            add_menu(tmpwin, NO_GLYPH, &any, '+', 0, ATR_NONE,
                     "[sort spells]", MENU_UNSELECTED);
        }
    }
    end_menu(tmpwin, prompt);

	//Show menu
    n = select_menu(tmpwin, how, &selected);
    destroy_nhwindow(tmpwin);
	
	//Remove menucolors
	for (int j = 0; j < colorbufcnt; j++)
	{
		free_menu_coloring_str(colorbufs[j]);
	}

    if (n > 0) {
        *spell_no = selected[0].item.a_int - 1;
        /* menu selection for `PICK_ONE' does not
           de-select any preselected entry */
        if (n > 1 && *spell_no == splaction)
            *spell_no = selected[1].item.a_int - 1;
        free((genericptr_t) selected);
        /* default selection of preselected spell means that
           user chose not to swap it with anything */
        if (*spell_no == splaction)
            return FALSE;
        return TRUE;
    } else if (splaction >= 0) {
        /* explicit de-selection of preselected spell means that
           user is still swapping but not for the current spell */
        *spell_no = splaction;
        return TRUE;
    }
    return FALSE;
} 


STATIC_OVL
void
add_spell_prepare_menu_heading(tmpwin, namelength, extraspaces, addemptyline)
winid tmpwin;
int namelength;
int extraspaces;
boolean addemptyline;
{
	char buf[BUFSZ], fmt[BUFSZ];
	anything any = zeroany;


	char spacebuf[BUFSZ] = "";

	for (int i = 0; i < extraspaces; i++)
		Strcat(spacebuf, " ");

	if (!iflags.menu_tab_sep)
	{
		Sprintf(fmt, "%%-%ds  Casts  Adds  Material components    %%s", namelength + 4);
		Sprintf(buf, fmt, "    Name", spacebuf);
	}
	else
	{
		Sprintf(buf, "Name\tCasts\tAdds\tMaterial components");
	}

	if (addemptyline)
	{
		any = zeroany;
		add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, "", MENU_UNSELECTED);
	}

	any = zeroany;
	add_menu(tmpwin, NO_GLYPH, &any, 0, 0, iflags.menu_headings, buf,
		MENU_UNSELECTED);

}

STATIC_OVL
void
add_spell_prepare_menu_item(tmpwin, i, splaction, namelength, extraspaces, usehotkey)
winid tmpwin;
int i;
int splaction;
int namelength;
int extraspaces;
boolean usehotkey;
{
	int splnum = !spl_orderindx || usehotkey ? i : spl_orderindx[i];
	char buf[BUFSZ], availablebuf[BUFSZ], matcompbuf[BUFSZ], levelbuf[10], fmt[BUFSZ];
	anything any = zeroany;

	if (!iflags.menu_tab_sep) {
		if (spellknow(splnum) <= 0)
			Sprintf(fmt, "%%-%ds  %%s", namelength);
		else
		{
			char lengthbuf[BUFSZ] = "";
			Sprintf(lengthbuf, "%ds", 23 + extraspaces);
			Sprintf(fmt, "%%-%ds  %%5s  %%4s  %%-", namelength);
			Strcat(fmt, lengthbuf);
			//fmt = "%-20s  %s   %5s  %-35s";
			//		fmt = "%-20s  %2d   %-12s %4d %3d%% %9s";
		}
	}
	else
	{
		if (spellknow(splnum) <= 0)
			strcpy(fmt, "%s\t%s");
		else
			strcpy(fmt, "%s\t%s\t%s\t%s");
		//		fmt = "%s\t%-d\t%s\t%-d\t%-d%%\t%s";
	}

	//Shorten spell name if need be
	char shortenedname[BUFSZ] = "";
	char fullname[BUFSZ];
	char addsbuf[BUFSZ];

	strcpy(fullname, spellname(splnum));

	if (strlen(fullname) > (size_t)namelength)
		strncpy(shortenedname, fullname, namelength);
	else
		strcpy(shortenedname, fullname);

	//Print spell level
	if (spellev(splnum) < -1)
		strcpy(levelbuf, " *");
	else if (spellev(splnum) == -1)
		strcpy(levelbuf, " c");
	else if (spellev(splnum) == 0)
		strcpy(levelbuf, " C");
	else
		Sprintf(levelbuf, "%2d", spellev(splnum));

	//Print spell amount
	if (spellamount(splnum) >= 0)
		Sprintf(availablebuf, "%d", spellamount(splnum));
	else
		strcpy(availablebuf, "Inf.");

	//Print spells gained
	if (spellmatcomp(splnum) > 0)
		Sprintf(addsbuf, "%d", matlists[spellmatcomp(splnum)].spellsgained);
	else
		strcpy(addsbuf, "N/A");

	//Shorten matcomp description, if needed
	char shortenedmatcompdesc[BUFSZ] = "";
	char fullmatcompdesc[BUFSZ];

	strcpy(fullmatcompdesc, matlists[spellmatcomp(splnum)].description_short);

	if (strlen(fullmatcompdesc) > 39)
		strncpy(shortenedmatcompdesc, fullmatcompdesc, 39);
	else
		strcpy(shortenedmatcompdesc, fullmatcompdesc);

	if (spellmatcomp(splnum))
		strcpy(matcompbuf, shortenedmatcompdesc);
	else
		strcpy(matcompbuf, "(Not required)");

	//Finally print everything to buf
	if (spellknow(splnum) <= 0)
	{
		Sprintf(buf, fmt, shortenedname, "(You cannot recall this spell)");
	}
	else
	{
		Sprintf(buf, fmt, shortenedname,
			availablebuf, addsbuf, matcompbuf);
	}

	char letter = '\0';
	if (usehotkey)
	{
		if (spellhotkey(i) == 10)
			letter = '0';
		else
			letter = spellhotkey(i) + '0';
	}
	else
		letter = spellet(splnum);


	any.a_int = splnum + 1; /* must be non-zero */
	add_menu(tmpwin, NO_GLYPH, &any, letter, 0, ATR_NONE, buf,
		(splnum == splaction) ? MENU_SELECTED : MENU_UNSELECTED);

}




STATIC_OVL
void
add_spell_cast_menu_heading(tmpwin, namelength, addemptyline)
winid tmpwin;
int namelength;
boolean addemptyline;
{
	char buf[BUFSZ], fmt[BUFSZ];
	anything any = zeroany;

	if (!iflags.menu_tab_sep)
	{
		Sprintf(fmt, "%%-%ds     Level %%-13s Mana Stat Fail Cool Casts", namelength);
		Sprintf(buf, fmt, "    Name",
			"Category");
	}
	else {
		Sprintf(buf, "Name\tLevel\tCategory\tMana\tStat\tFail\tCool\tCasts");
	}

	if (addemptyline)
	{
		any = zeroany;
		add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, "", MENU_UNSELECTED);
	}

	any = zeroany;
	add_menu(tmpwin, NO_GLYPH, &any, 0, 0, iflags.menu_headings, buf,
		MENU_UNSELECTED);

}

STATIC_OVL
void
add_spell_cast_menu_item(tmpwin, i, splaction, namelength, colorbufs, colorbufcnt, usehotkey)
winid tmpwin;
int i;
int splaction;
int namelength;
char *colorbufs;
int* colorbufcnt;
boolean usehotkey;
{
	int splnum = !spl_orderindx || usehotkey ? i : spl_orderindx[i];
	char buf[BUFSZ], availablebuf[BUFSZ], levelbuf[10], statbuf[10], fmt[BUFSZ];
	char shortenedname[BUFSZ] = "";
	char fullname[BUFSZ] = "";
	char categorybuf[BUFSZ] = "";
	anything any = zeroany;

	if (!iflags.menu_tab_sep) {
		if (spellknow(splnum) <= 0)
			Sprintf(fmt, "%%-%ds  %%s", namelength);
		else
			Sprintf(fmt, "%%-%ds  %%s   %%-13s %%4d  %%s %%3d%%%% %%4d  %%4s", namelength);
		//		fmt = "%-20s  %2d   %-12s %4d %3d%% %8s";
	}
	else {
		if (spellknow(splnum) <= 0)
			strcpy(fmt, "%s\t%s");
		else
			strcpy(fmt, "%s\t%s\t%s\t%-d\t%-d%%\t%-d\t%s");
		//		fmt = "%s\t%-d\t%s\t%-d\t%-d%%\t%s";
	}

	Sprintf(fullname, "%s%s", spellcooldownleft(splnum) > 0 ? "-" : "",
		spellname(splnum));

	//Spell name
	if (strlen(fullname) > (size_t)(spellcooldownleft(splnum) > 0 ? namelength - 1 : namelength))
		strncpy(shortenedname, fullname, (size_t)(spellcooldownleft(splnum) > 0 ? namelength - 1 : namelength));
	else
		strcpy(shortenedname, fullname);

	if (spellcooldownleft(splnum) > 0)
		Strcat(shortenedname, "-");

	//Spell level
	if (spellev(splnum) < -1)
		strcpy(levelbuf, " *");
	else if (spellev(splnum) == -1)
		strcpy(levelbuf, " c");
	else if (spellev(splnum) == 0)
		strcpy(levelbuf, " C");
	else
		Sprintf(levelbuf, "%2d", spellev(splnum));

	strcpy(categorybuf, spelltypemnemonic(spell_skilltype(spellid(splnum))));


	if (spellamount(splnum) >= 0)
		Sprintf(availablebuf, "%d", spellamount(splnum));
	else
		strcpy(availablebuf, "Inf.");

	switch (objects[spellid(splnum)].oc_spell_attribute)
	{
	case A_STR:
		strcpy(statbuf, "Str");
		break;
	case A_DEX:
		strcpy(statbuf, "Dex");
		break;
	case A_CON:
		strcpy(statbuf, "Con");
		break;
	case A_INT:
		strcpy(statbuf, "Int");
		break;
	case A_WIS:
		strcpy(statbuf, "Wis");
		break;
	case A_CHA:
		strcpy(statbuf, "Cha");
		break;
	case A_MAX_INT_WIS:
		strcpy(statbuf, "I/W");
		break;
	case A_MAX_INT_CHA:
		strcpy(statbuf, "I/C");
		break;
	case A_MAX_WIS_CHA:
		strcpy(statbuf, "W/C");
		break;
	case A_MAX_INT_WIS_CHA:
		strcpy(statbuf, "Any");
		break;
	case A_AVG_INT_WIS:
		strcpy(statbuf, "I+W");
		break;
	case A_AVG_INT_CHA:
		strcpy(statbuf, "I+C");
		break;
	case A_AVG_WIS_CHA:
		strcpy(statbuf, "W+C");
		break;
	case A_AVG_INT_WIS_CHA:
		strcpy(statbuf, "All");
		break;
	default:
		strcpy(statbuf, "N/A");
		break;
	}

	//Category
	if (spellknow(splnum) <= 0)
		Sprintf(buf, fmt, shortenedname, "(You cannot recall this spell)");
	else
		Sprintf(buf, fmt, shortenedname, levelbuf,//spellev(splnum),
			categorybuf,
			getspellenergycost(splnum),
			statbuf,
			100 - percent_success(splnum),
			spellcooldownleft(splnum) > 0 ? spellcooldownleft(splnum) : getspellcooldown(splnum),
			availablebuf);  //spellretention(splnum, retentionbuf));

	any.a_int = splnum + 1; /* must be non-zero */

	char letter = '\0';
	if (usehotkey)
	{
		if (spellhotkey(i) == 10)
			letter = '0';
		else
			letter = spellhotkey(i) + '0';
	}
	else
		letter = spellet(splnum);

	add_menu(tmpwin, NO_GLYPH, &any, letter, 0, ATR_NONE, buf,
		(splnum == splaction) ? MENU_SELECTED : MENU_UNSELECTED);

	Strcat(shortenedname, "=black");
	if (spellcooldownleft(splnum) > 0 && splaction != SPELLMENU_PREPARE)
	{
		add_menu_coloring(shortenedname);
		strcpy(colorbufs, shortenedname);
		(*colorbufcnt)++;
	}
	/* //Should not be needed
	else
	{
		free_menu_coloring_str(buf);
	}
	*/
}

int
dospellmanagemenu(spell)
int spell;
{
	int i = '\0';

	menu_item* pick_list = (menu_item*)0;
	winid win;
	anything any;

	any = zeroany;
	win = create_nhwindow(NHW_MENU);
	start_menu(win);


	struct available_selection_item
	{
		int charnum;
		char name[BUFSZ];
		int (*function_ptr)(int);
	};
	struct available_selection_item available_selection_item[10] = { 0 };
	int selnum = 0;


	/* Spell description */
	strcpy(available_selection_item[selnum].name, "Detailed spell description");
	available_selection_item[selnum].function_ptr = &spelldescription;
	available_selection_item[selnum].charnum = 'a' + selnum;

	any = zeroany;
	any.a_char = available_selection_item[selnum].charnum;

	add_menu(win, NO_GLYPH, &any,
		any.a_char, 0, ATR_NONE,
		available_selection_item[selnum].name, MENU_UNSELECTED);

	selnum++;

	/* Set or clear hotkey */
	char hotbuf[BUFSZ] = "";
	if(spellhotkey(spell))
		Sprintf(hotbuf, " (currently \'%d\')", (spellhotkey(spell) % 10));

	Sprintf(available_selection_item[selnum].name, "Set or clear hotkey%s", hotbuf);
	available_selection_item[selnum].function_ptr = &setspellhotkey;
	available_selection_item[selnum].charnum = 'a' + selnum;

	any = zeroany;
	any.a_char = available_selection_item[selnum].charnum;

	add_menu(win, NO_GLYPH, &any,
		any.a_char, 0, ATR_NONE,
		available_selection_item[selnum].name, MENU_UNSELECTED);

	selnum++;


	/* Forget spell */
	strcpy(available_selection_item[selnum].name, "Forget spell");
	available_selection_item[selnum].function_ptr = &forgetspell;
	available_selection_item[selnum].charnum = 'a' + selnum;

	any = zeroany;
	any.a_char = available_selection_item[selnum].charnum;

	add_menu(win, NO_GLYPH, &any,
		any.a_char, 0, ATR_NONE,
		available_selection_item[selnum].name, MENU_UNSELECTED);

	selnum++;


	/* Finalize the menu */
	char qbuf[BUFSZ] = "";
	Sprintf(qbuf, "What do you want to do with \'%s\'?", spellname(spell));

	/* Finish the menu */
	end_menu(win, qbuf);


	if (selnum <= 0)
	{
		You("can't do anything with this spell.");
		destroy_nhwindow(win);
		return 0;
	}


	/* Now generate the menu */
	if (select_menu(win, PICK_ONE, &pick_list) > 0)
	{
		i = pick_list->item.a_char;
		free((genericptr_t)pick_list);
	}
	destroy_nhwindow(win);

	if (i == '\0')
		return 0;

	int res = 0;
	for (int j = 0; j < selnum; j++)
	{
		if (available_selection_item[j].charnum == i)
		{
			if (i != '\0')
			{
				res = (available_selection_item[j].function_ptr)(spell);
			}
			break;
		}
	}

	return res;
}


int
setspellhotkey(spell)
int spell;
{
	char promptbuf[BUFSZ] = "";
	//char answerbuf[BUFSZ] = "";
	char answerchar = '\0';
	char buf[BUFSZ] = "";
	const char* letters = "1234567890-q";
	Sprintf(promptbuf, "Which hotkey do you want to use for \'%s\'?", spellname(spell));
	//getlin(promptbuf, answerbuf);
	answerchar = yn_function(promptbuf, letters, '-');
	//(void)mungspaces(answerbuf);
	if (answerchar == '\033' || answerchar == 'q')
	{
		pline(Never_mind);
		return 0;
	}
	else if (answerchar == '\0' || answerchar == '-')
	{
		spellhotkey(spell) = 0;
		Sprintf(buf, "Hotkey for \'%s\' cleared.", spellname(spell));
		pline(buf);
	}
	else if (answerchar >= '0' && answerchar <= '9')
	{
		int selected_hotkey = 0;
		if(answerchar == '0')
			selected_hotkey = 10;
		else
			selected_hotkey = answerchar - '0';

		/* clear the existing hotkey */
		for (int n = 0; n < MAXSPELL; n++)
		{
			if(spl_book[n].sp_hotkey == selected_hotkey)
			{
				spl_book[n].sp_hotkey = 0;
				break;
			}
			if (spellid(n) == NO_SPELL)
				break;
		}

		spellhotkey(spell) = selected_hotkey;
		Sprintf(buf, "Hotkey for \'%s\' set to \'%c\'.", spellname(spell), answerchar);
		pline(buf);
	}
	else
		pline("Illegal hotkey.");

	return 0;
}

int
forgetspell(spell)
int spell;
{
	char qbuf[BUFSZ] = "";
	Sprintf(qbuf, "Are you sure you want to forget \'%s\' permanently?", spellname(spell));
	if (ynq(qbuf) == 'y')
	{
		struct spell empty_spell = { 0 };
		for (int n = spell + 1; n <= MAXSPELL; n++)
		{
			if (n == MAXSPELL)
				spl_book[n - 1] = empty_spell;
			else
			{ 
				spl_book[n - 1] = spl_book[n];
				if (spellid(n) == NO_SPELL)
					break;
			}
		}
		char buf[BUFSZ] = "";
		Sprintf(buf, "You removed \'%s\' from your memory permanently.", spellname(spell));
		pline(buf);
	}

	return 0;
}

STATIC_OVL int
percent_success(spell)
int spell;
{
    /* Intrinsic and learned ability are combined to calculate
     * the probability of player's success at cast a given spell.
     */
    int chance, splcaster, statused = A_INT;
//    int difficulty;
    int skill;
	boolean armorpenalty = TRUE;

	if (Role_if(PM_PRIEST) &&
		(objects[spellid(spell)].oc_spell_attribute == A_WIS
			|| objects[spellid(spell)].oc_spell_attribute == A_MAX_INT_WIS
			|| objects[spellid(spell)].oc_spell_attribute == A_MAX_WIS_CHA
			|| objects[spellid(spell)].oc_spell_attribute == A_MAX_INT_WIS_CHA
			|| objects[spellid(spell)].oc_spell_attribute == A_AVG_INT_WIS
			|| objects[spellid(spell)].oc_spell_attribute == A_AVG_WIS_CHA
			|| objects[spellid(spell)].oc_spell_attribute == A_AVG_INT_WIS_CHA
			))
		armorpenalty = FALSE;

    /* Calculate intrinsic ability (splcaster) */
	splcaster = 0; // urole.spelbase;
    //special = urole.spelheal;
	statused = attribute_value_for_spellbook(spellid(spell));

	if (armorpenalty)
	{
		if (uarm)
			splcaster += objects[uarm->otyp].oc_spell_casting_penalty;
		if (uarms)
			splcaster += objects[uarms->otyp].oc_spell_casting_penalty;
		if (uarmh)
			splcaster += objects[uarmh->otyp].oc_spell_casting_penalty;
		if (uarmg)
			splcaster += objects[uarmg->otyp].oc_spell_casting_penalty;
		if (uarmf)
			splcaster += objects[uarmf->otyp].oc_spell_casting_penalty;
		if (uarmu)
			splcaster += objects[uarmu->otyp].oc_spell_casting_penalty;
		if (uarmo)
			splcaster += objects[uarmo->otyp].oc_spell_casting_penalty;
		if (uarmb)
			splcaster += objects[uarmb->otyp].oc_spell_casting_penalty;
		if (umisc)
			splcaster += objects[umisc->otyp].oc_spell_casting_penalty;
		if (umisc2)
			splcaster += objects[umisc2->otyp].oc_spell_casting_penalty;
		if (umisc3)
			splcaster += objects[umisc3->otyp].oc_spell_casting_penalty;
		if (umisc4)
			splcaster += objects[umisc4->otyp].oc_spell_casting_penalty;
		if (umisc5)
			splcaster += objects[umisc5->otyp].oc_spell_casting_penalty;
		if (uamul)
			splcaster += objects[uamul->otyp].oc_spell_casting_penalty;
		if (uleft)
			splcaster += objects[uleft->otyp].oc_spell_casting_penalty;
		if (uright)
			splcaster += objects[uright->otyp].oc_spell_casting_penalty;
		if (ublindf)
			splcaster += objects[ublindf->otyp].oc_spell_casting_penalty;
		if (uwep)
			splcaster += objects[uwep->otyp].oc_spell_casting_penalty;
	}

    if (spellid(spell) == urole.spelspec)
        splcaster += urole.spelsbon;

	splcaster -= u.uspellcastingbonus; /* This is a bonus, not a penalty */


    /* Calculate success chance */

	chance = 0;
	chance += 10 * statused;

    /*
     * High level spells are harder.  Easier for higher level casters.
     * The difficulty is based on the hero's level and their skill level
     * in that spell type.
     */
    skill = P_SKILL(spell_skilltype(spellid(spell)));
    skill = max(skill, P_UNSKILLED) - 1; /* unskilled => 0 */
    //difficulty =
    //    (spellev(spell) - 1) * 4 - ((skill * 6) + ((u.ulevel - 1) * 2) + 0);

	chance += -50 * (spellev(spell) + 1);
	chance += 80 * skill;
	chance += 20 * u.ulevel;
	chance += -5 * splcaster;

//	if (difficulty > 0) {
        /* Player is too low level or unskilled. */
//        chance -= isqrt(900 * difficulty + 2000);
//    } else {
        /* Player is above level.  Learning continues, but the
         * law of diminishing returns sets in quickly for
         * low-level spells.  That is, a player quickly gains
         * no advantage for raising level.
         */
//        int learning = 15 * -difficulty / spellev(spell);
//        chance += learning > 20 ? 20 : learning;
//    }

    /* Clamp the chance: >18 stat and advanced learning only help
     * to a limit, while chances below "hopeless" only raise the
     * specter of overflowing 16-bit ints (and permit wearing a
     * shield to raise the chances :-).
     */
	/*
    if (chance < 0)
        chance = 0;
    if (chance > 500)
        chance = 500;
	*/
    /* Wearing anything but a light shield makes it very awkward
     * to cast a spell.  The penalty is not quite so bad for the
     * player's role-specific spell.
     */
	/*
    if (uarms && weight(uarms) > (int) objects[SMALL_SHIELD].oc_weight) {
        if (spellid(spell) == urole.spelspec) {
            chance *= 0.75;
        } else {
            chance /= 2;
        }
    }*/

    /* Finally, chance (based on player intell/wisdom and level) is
     * combined with ability (based on player intrinsics and
     * encumbrances).  No matter how intelligent/wise and advanced
     * a player is, intrinsics and encumbrance can prevent casting;
     * and no matter how able, learning is always required.
     */
	// chance = chance * (20 - splcaster) / 15 - splcaster;

    /* Clamp to percentile */
    if (chance > 100)
        chance = 100;
    if (chance < 0)
        chance = 0;

    return chance;
}

STATIC_OVL
int
attribute_value_for_spellbook(objectid)
int objectid;
{
	int statused = 0;

	if (objects[objectid].oc_spell_attribute >= 0 && objects[objectid].oc_spell_attribute < A_MAX)
		statused = ACURR(objects[objectid].oc_spell_attribute); //ACURR(urole.spelstat);
	else if (objects[objectid].oc_spell_attribute == A_MAX_INT_WIS)
		statused = max(ACURR(A_INT), ACURR(A_WIS));
	else if (objects[objectid].oc_spell_attribute == A_MAX_INT_CHA)
		statused = max(ACURR(A_INT), ACURR(A_CHA));
	else if (objects[objectid].oc_spell_attribute == A_MAX_WIS_CHA)
		statused = max(ACURR(A_CHA), ACURR(A_WIS));
	else if (objects[objectid].oc_spell_attribute == A_MAX_INT_WIS_CHA)
		statused = max(max(ACURR(A_INT), ACURR(A_WIS)), ACURR(A_CHA));
	else if (objects[objectid].oc_spell_attribute == A_AVG_INT_WIS)
		statused = (ACURR(A_INT) + ACURR(A_WIS)) / 2;
	else if (objects[objectid].oc_spell_attribute == A_AVG_INT_CHA)
		statused = (ACURR(A_INT) + ACURR(A_CHA)) / 2;
	else if (objects[objectid].oc_spell_attribute == A_AVG_WIS_CHA)
		statused = (ACURR(A_CHA) + ACURR(A_WIS)) / 2;
	else if (objects[objectid].oc_spell_attribute == A_AVG_INT_WIS_CHA)
		statused = (ACURR(A_INT) + ACURR(A_WIS) + ACURR(A_CHA)) / 3;

	return statused;

}

STATIC_OVL char *
spellretention(idx, outbuf)
int idx;
char *outbuf;
{
    long turnsleft, percent, accuracy;
    int skill;

    skill = P_SKILL(spell_skilltype(spellid(idx)));
    skill = max(skill, P_UNSKILLED); /* restricted same as unskilled */
    turnsleft = spellknow(idx);
    *outbuf = '\0'; /* lint suppression */

    if (turnsleft < 1L) {
        /* spell has expired; hero can't successfully cast it anymore */
        Strcpy(outbuf, "(gone)");
    } else if (turnsleft >= (long) KEEN) {
        /* full retention, first turn or immediately after reading book */
        Strcpy(outbuf, "100%");
    } else {
        /*
         * Retention is displayed as a range of percentages of
         * amount of time left until memory of the spell expires;
         * the precision of the range depends upon hero's skill
         * in this spell.
         *    expert:  2% intervals; 1-2,   3-4,  ...,   99-100;
         *   skilled:  5% intervals; 1-5,   6-10, ...,   95-100;
         *     basic: 10% intervals; 1-10, 11-20, ...,   91-100;
         * unskilled: 25% intervals; 1-25, 26-50, 51-75, 76-100.
         *
         * At the low end of each range, a value of N% really means
         * (N-1)%+1 through N%; so 1% is "greater than 0, at most 200".
         * KEEN is a multiple of 100; KEEN/100 loses no precision.
         */
        percent = (turnsleft - 1L) / ((long) KEEN / 100L) + 1L;
        accuracy =
            (skill == P_EXPERT) ? 2L : (skill == P_SKILLED)
                                           ? 5L
                                           : (skill == P_BASIC) ? 10L : 25L;
        /* round up to the high end of this range */
        percent = accuracy * ((percent - 1L) / accuracy + 1L);
        Sprintf(outbuf, "%ld%%-%ld%%", percent - accuracy + 1L, percent);
    }
    return outbuf;
}


boolean
already_learnt_spell_type(otyp)
int otyp;
{
	if (otyp <= STRANGE_OBJECT || otyp >= NUM_OBJECTS)
		return FALSE;

	if (objects[otyp].oc_class != SPBOOK_CLASS)
		return FALSE;

	for (int i = 0; i < MAXSPELL; i++)
		if (spellid(i) == NO_SPELL)
			break;
		else if (spellid(i) == otyp)
			return TRUE;

	return FALSE;
}


/* Learn a spell during creation of the initial inventory */
void
initialspell(obj)
struct obj *obj;
{
    int i, otyp = obj->otyp;

    for (i = 0; i < MAXSPELL; i++)
        if (spellid(i) == NO_SPELL || spellid(i) == otyp)
            break;

    if (i == MAXSPELL) {
        impossible("Too many spells memorized!");
    } else if (spellid(i) != NO_SPELL) {
        /* initial inventory shouldn't contain duplicate spellbooks */
        impossible("Spell %s already known.", OBJ_NAME(objects[otyp]));
    } else {
        spl_book[i].sp_id = otyp;
        spl_book[i].sp_lev = objects[otyp].oc_spell_level;
		spl_book[i].sp_matcomp = objects[otyp].oc_material_components;
		if(spl_book[i].sp_matcomp)
			spl_book[i].sp_amount = matlists[spl_book[i].sp_matcomp].spellsgained; /* Some amount in the beginning */
		else
			spl_book[i].sp_amount = -1;
		spl_book[i].sp_cooldownlength = objects[otyp].oc_spell_cooldown;
		spl_book[i].sp_cooldownleft = 0;
		spl_book[i].sp_skillchance = objects[otyp].oc_spell_skill_chance;

		incrnknow(i, 0);
    }
    return;
}


/* Mixing starts here*/

/* the 'X' command, two-weapon moved to M(x) */
int
domix()
{
	int spell_no;

	if (getspell(&spell_no, 1))
	{
		//Open mixing menu and explain what components are needed
		return domaterialcomponentsmenu(spell_no);
	}
	return 0;
}


STATIC_OVL int
domaterialcomponentsmenu(spell)
int spell;
{
	//This might happen with amnesia etc., the spells no longer "age"
	if (spellknow(spell) <= 0)
	{
		You("cannot recall this spell or its material components anymore.");
		return 0;
	}

	if (!spellmatcomp(spell))
	{
		pline("That spell does not require material components.");
		return 0;
	}

	if (!invent)
	{
		You("have nothing to prepare spells with.");
		return 0;
	}

	//Start assuming components are ok, unless we find otherwise below
	int result = 1;


	if (*u.ushops)
		sellobj_state(SELL_DELIBERATE);

	struct obj* selcomps[MAX_MATERIALS];
	for (int j = 0; j < MAX_MATERIALS; j++)
		selcomps[j] = (struct obj*)0;

	char spellname[BUFSZ] = "";
	char capspellname[BUFSZ] = "";
	strcpy(spellname, obj_descr[spellid(spell)].oc_name);
	strcpy(capspellname, spellname);
	*capspellname = highc(*capspellname); //Make first letter capital

	int matcnt = 0;
	int lowest_multiplier = 999;

	//Check the material components here
	for(int j = 0; matlists[spellmatcomp(spell)].matcomp[j].amount != 0; j++)
	{
		matcnt++;
		char buf[BUFSZ], buf3[BUFSZ], buf4[BUFSZ] = "";
		struct materialcomponent* mc = &matlists[spellmatcomp(spell)].matcomp[j];
		strcpy(buf3, domatcompname(mc));

		Sprintf(buf, "You need %s%s. ",
			buf3, (mc->flags& MATCOMP_NOT_SPENT ? " as a catalyst" : " as a component"));


		struct obj* otmp = (struct obj*) 0;
		char buf5[BUFSZ];
		Sprintf(buf5, "prepare \"%s\" with", spellname);

		char classletter = FOOD_CLASS;
		if (mc->objectid != CORPSE && mc->objectid != TIN && mc->objectid != EGG)
			classletter = objects[mc->objectid].oc_class;

		otmp = getobj(&classletter, buf5, 0, buf);

		if (!otmp)
			return 0;

		//Save the material component
		selcomps[j] = otmp;

		//Check if acceptable
		boolean acceptable = FALSE;
		if (otmp->otyp == mc->objectid)
			acceptable = TRUE;

		if ((mc->flags & MATCOMP_BLESSED_REQUIRED) && !otmp->blessed)
			acceptable = FALSE;

		if ((mc->flags & MATCOMP_CURSED_REQUIRED) && !otmp->cursed)
			acceptable = FALSE;

		if ((mc->flags & MATCOMP_NOT_CURSED) && otmp->cursed)
			acceptable = FALSE;

		if ((mc->flags & MATCOMP_DEATH_ENCHANTMENT_REQUIRED) && otmp->elemental_enchantment != DEATH_ENCHANTMENT)
			acceptable = FALSE;

		if ((mc->objectid == CORPSE || mc->objectid == TIN || mc->objectid == EGG) && mc->monsterid >= 0 && mc->monsterid != otmp->corpsenm)
			acceptable = FALSE;

		//Check quantity
		if (otmp->quan < mc->amount)
		{
			pline("%s requires %s as %s, but you have only %d.",
				spellname,
				buf3,
				otmp->quan);
			return 0;
		}

		int quan_mult = mc->amount > 0 ? otmp->quan / mc->amount : 1;

		if (quan_mult < lowest_multiplier)
			lowest_multiplier = quan_mult;

		//Note: You might ask for another pick from another type (e.g., using both blessed and uncursed items), but this gets a bit too complicated
		if (acceptable)
		{
			//Correct ingredient
		}
		else
		{
			//Incorrect ingredient
			result = 0;
		}

	}

	boolean failure = !result || ((Confusion || Stunned) && spellev(spell) > 1 && rn2(spellev(spell)));
	int spells_gained_per_mixing = matlists[spellmatcomp(spell)].spellsgained;
	int selected_multiplier = 1;

	if (lowest_multiplier > 1)
	{
		char qbuf[BUFSZ] = "";
		char buf[BUFSZ] = "";
		Sprintf(qbuf, "You get %d casting%s per mixing. How many times to mix? [max %d] (1)", spells_gained_per_mixing, plur(spells_gained_per_mixing), lowest_multiplier);
		getlin(qbuf, buf);
		(void)mungspaces(buf);

		if (buf[0] == '\033')
		{
			return 0;
		}
		else if (buf[0] == ' ' || buf[0] == '\0')
		{
			selected_multiplier = 1;
		}
		else
		{
			int count = atoi(buf);
			if (count > 0)
			{
				selected_multiplier = min(lowest_multiplier, count);
			}
		}
	}

	//Now go through the selected material components
	for (int j = 0; j < matcnt; j++)
	{
		struct obj* otmp = selcomps[j];
		struct materialcomponent* mc = &matlists[spellmatcomp(spell)].matcomp[j];

		if (!otmp || !mc)
			continue;

		if (!failure)
			makeknown(otmp->otyp);

		boolean usecomps = !(mc->flags & MATCOMP_NOT_SPENT);

		//Resisting objects cannot be consumed
		if (obj_resists(otmp, 0, 100))
			usecomps = FALSE;

		if (otmp->otyp != mc->objectid)
		{
			//Wrong item
			if(otmp->oclass != objects[mc->objectid].oc_class
				&& otmp->oclass != POTION_CLASS && otmp->oclass != SCROLL_CLASS && otmp->oclass != FOOD_CLASS)
			{
				//The same class may get consumed, but not a different class, unless it is a potion, scroll, or food
				usecomps = FALSE;
			}

			//Disintegration resistant items, indestructible items, and fire-resistant items won't get destroyed
			//Loadstone will not get destroyed, nor do containers or worn items, which can be cursed
			if((objects[otmp->otyp].oc_flags & O1_DISINTEGRATION_RESISTANT)
				|| (objects[otmp->otyp].oc_flags & O1_FIRE_RESISTANT)
				|| (objects[otmp->otyp].oc_flags & O1_LIGHTNING_RESISTANT)
				|| (objects[otmp->otyp].oc_flags & O1_INDESTRUCTIBLE)
				|| Is_container(otmp)
				|| objects[otmp->otyp].oc_flags & O1_CANNOT_BE_DROPPED_IF_CURSED
				|| objects[otmp->otyp].oc_flags & O1_BECOMES_CURSED_WHEN_PICKED_UP_AND_DROPPED
				|| otmp->owornmask & ~W_WEAPON
				|| otmp->oclass == WEAPON_CLASS && (otmp->owornmask & W_WEAPON))
				usecomps = FALSE;
		}
		//Use them all up
		if (!(mc->flags & MATCOMP_NOT_SPENT) && !obj_resists(otmp,0,100) && otmp->oclass == objects[mc->objectid].oc_class)
		{
			int used_amount = (failure ? 1 : selected_multiplier) * mc->amount;
			if(otmp->quan >= used_amount)
			{
				for (int i = 0; i < used_amount; i++)
					useup(otmp);
			}
			else
			{
				impossible("There should always be enough material components at this stage");
				useupall(otmp);
			}
		}
	}

	//And now the result
	if (failure)
	{
		//Explosion
		int dmg = d(max(1, spellev(spell)), 6);
		if (spellev(spell) == 0)
			dmg = rnd(4);
		else if (spellev(spell) == -1)
			dmg = rnd(2);
		else if (spellev(spell) < -1)
			dmg = 1;

		char buf[BUFSZ];
		Sprintf(buf, "killed by a failed spell preparation");

		int explosionchance = min(100, max(0, (spellev(spell) - 2) * 10 + dmg * 4));

		if ((99-rnl(100)) < explosionchance)//If lucky explosions happen less frequently
		{
			//One more damage
			Your("concoction explodes in a large ball of fire!");
			losehp(Maybe_Half_Phys(dmg), buf, NO_KILLER_PREFIX);
			explode(u.ux, u.uy, RAY_FIRE, 1, 0, 0,
				EXPL_MAGICAL);
		}
		else
		{
			Your("concoction flares up, burning you!");
			losehp(Maybe_Half_Phys(dmg), buf, NO_KILLER_PREFIX);
		}
	}
	else
	{
		//Success
		int addedamount = spells_gained_per_mixing * selected_multiplier;
		spellamount(spell) += addedamount;
		You("successfully prepared the material components.", spellname);
		if (addedamount == 1)
			You("now have one more casting of \"%s\" prepared.", spellname);
		else
			You("now have %d more castings of \"%s\" prepared.", addedamount, spellname);

#if 0
		/* gain skill for successful preparation */
		int otyp = spellid(spell);
		int skill = spell_skilltype(otyp);
		int skillchance = spellskillchance(spell);
		int skillpoints = max(0, max(spellev(spell) + 2, 1) * addedamount * skillchance / 100);

		if(skillpoints > 0)	
			use_skill(skill, skillpoints);
#endif
	}


	if (*u.ushops)
		sellobj_state(SELL_NORMAL);
	if (result)
		reset_occupations();

	return result;

}

const char*
domatcompname(mc)
struct materialcomponent* mc;
{
	struct objclass* perobj = (struct objclass*)0;
	if (mc->objectid >= 0)
		perobj = &objects[mc->objectid];

	struct permonst* permon = (struct permonst*)0;
	if (mc->monsterid >= 0)
		permon = &mons[mc->monsterid];

	if (!perobj || !mc || mc->amount == 0)
		return empty_string;

	static char buf3[BUFSZ];
	char buf2[BUFSZ], buf4[BUFSZ] = "";

	if (permon && perobj)
	{
		if (mc->objectid == CORPSE || mc->objectid == EGG)
			//Add "lizard" to "corpse" to get "lizard corpse" (or lizard egg)
			Sprintf(buf4, "%s %s", permon->mname, obj_descr[perobj->oc_name_idx].oc_name);
		else if (mc->objectid == TIN)
			//Add "lizard" to "tin" to get "lizard corpse"
			Sprintf(buf4, "%s of %s meat", obj_descr[perobj->oc_name_idx].oc_name, permon->mname);
		else
			Sprintf(buf4, "%s%s", obj_descr[perobj->oc_name_idx].oc_name, GemStone(mc->objectid) ? " stone" : "");
	}
	else
	{
		Sprintf(buf4, "%s%s%s",
			objects[mc->objectid].oc_class == SCROLL_CLASS ? "scroll of " :
			objects[mc->objectid].oc_class == POTION_CLASS ? "potion of " :
			objects[mc->objectid].oc_class == WAND_CLASS ? "wand of " :
			objects[mc->objectid].oc_class == SPBOOK_CLASS ? "spellbook of " : "",
			obj_descr[perobj->oc_name_idx].oc_name,
			GemStone(mc->objectid) ? " stone" : "");
	}

	//Correct type of component
	Sprintf(buf2, "%s%s%s",
		(mc->flags & MATCOMP_BLESSED_REQUIRED ? "blessed " : mc->flags & MATCOMP_CURSED_REQUIRED ? "cursed " : (mc->flags & MATCOMP_NOT_CURSED ? "noncursed " : "")),
		(mc->flags & MATCOMP_DEATH_ENCHANTMENT_REQUIRED ? "deathly " : ""),
		buf4);

	//Indicate how many
	if (mc->amount == 1)
		strcpy(buf3, an(buf2));
	else
		Sprintf(buf3, "%d %s", mc->amount, makeplural(buf2));

	return buf3;
}

int
getspellcooldown(spell)
int spell;
{
	if (spell < 0)
		return 0;

	//int cooldown = objects[spellid(spell)].oc_spell_cooldown;
	int cooldown = spellcooldownlength(spell);

	/*
	int spell_level = spellev(spell);
	int cooldown = spell_level + 1;

	if (spell_level < -1)
		cooldown = 0;

	if (spell_level == 8)
		cooldown = 10;
	else if (spell_level == 9)
		cooldown = 12;
	else if (spell_level == 10)
		cooldown = 14;
		*/
	return cooldown;
}


/*spell.c*/
