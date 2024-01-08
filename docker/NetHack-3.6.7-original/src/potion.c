/* NetHack 3.6	potion.c	$NHDT-Date: 1573848199 2019/11/15 20:03:19 $  $NHDT-Branch: NetHack-3.6 $:$NHDT-Revision: 1.167 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Robert Patrick Rankin, 2013. */
/* NetHack may be freely redistributed.  See license for details. */

/* JNetHack Copyright */
/* (c) Issei Numata, Naoki Hamada, Shigehiro Miyashita, 1994-2000  */
/* For 3.4-, Copyright (c) SHIRAKATA Kentaro, 2002-2023            */
/* JNetHack may be freely redistributed.  See license for details. */

#include "hack.h"

boolean notonhead = FALSE;

static NEARDATA int nothing, unkn;
static NEARDATA const char beverages[] = { POTION_CLASS, 0 };

STATIC_DCL long FDECL(itimeout, (long));
STATIC_DCL long FDECL(itimeout_incr, (long, int));
STATIC_DCL void NDECL(ghost_from_bottle);
STATIC_DCL boolean
FDECL(H2Opotion_dip, (struct obj *, struct obj *, BOOLEAN_P, const char *));
STATIC_DCL short FDECL(mixtype, (struct obj *, struct obj *));

/* force `val' to be within valid range for intrinsic timeout value */
STATIC_OVL long
itimeout(val)
long val;
{
    if (val >= TIMEOUT)
        val = TIMEOUT;
    else if (val < 1)
        val = 0;

    return val;
}

/* increment `old' by `incr' and force result to be valid intrinsic timeout */
STATIC_OVL long
itimeout_incr(old, incr)
long old;
int incr;
{
    return itimeout((old & TIMEOUT) + (long) incr);
}

/* set the timeout field of intrinsic `which' */
void
set_itimeout(which, val)
long *which, val;
{
    *which &= ~TIMEOUT;
    *which |= itimeout(val);
}

/* increment the timeout field of intrinsic `which' */
void
incr_itimeout(which, incr)
long *which;
int incr;
{
    set_itimeout(which, itimeout_incr(*which, incr));
}

void
make_confused(xtime, talk)
long xtime;
boolean talk;
{
    long old = HConfusion;

    if (Unaware)
        talk = FALSE;

    if (!xtime && old) {
        if (talk)
/*JP
            You_feel("less %s now.", Hallucination ? "trippy" : "confused");
*/
            You("%s�������ޤä���", Hallucination ? "�إ�إ�" : "����");
    }
    if ((xtime && !old) || (!xtime && old))
        context.botl = TRUE;

    set_itimeout(&HConfusion, xtime);
}

void
make_stunned(xtime, talk)
long xtime;
boolean talk;
{
    long old = HStun;

    if (Unaware)
        talk = FALSE;

    if (!xtime && old) {
        if (talk)
#if 0 /*JP:T*/
            You_feel("%s now.",
                     Hallucination ? "less wobbly" : "a bit steadier");
#else
            You_feel("%s��",
                     Hallucination ? "�ؤ��餬�����ޤä�" : "������󤷤ä��ꤷ�Ƥ���");
#endif
    }
    if (xtime && !old) {
        if (talk) {
            if (u.usteed)
/*JP
                You("wobble in the saddle.");
*/
                You("�Ȥξ�Ǥ��餰�餷����");
            else
/*JP
                You("%s...", stagger(youmonst.data, "stagger"));
*/
                You("���餯�餷��������");
        }
    }
    if ((!xtime && old) || (xtime && !old))
        context.botl = TRUE;

    set_itimeout(&HStun, xtime);
}

/* Sick is overloaded with both fatal illness and food poisoning (via
   u.usick_type bit mask), but delayed killer can only support one or
   the other at a time.  They should become separate intrinsics.... */
void
make_sick(xtime, cause, talk, type)
long xtime;
const char *cause; /* sickness cause */
boolean talk;
int type;
{
    struct kinfo *kptr;
    long old = Sick;

#if 0   /* tell player even if hero is unconscious */
    if (Unaware)
        talk = FALSE;
#endif
    if (xtime > 0L) {
        if (Sick_resistance)
            return;
        if (!old) {
            /* newly sick */
/*JP
            You_feel("deathly sick.");
*/
            You("�µ��ǻ�ˤ�������");
        } else {
            /* already sick */
            if (talk)
/*JP
                You_feel("%s worse.", xtime <= Sick / 2L ? "much" : "even");
*/
                You("%s���������褦�ʵ������롥", xtime <= Sick/2L ? "�����" : "��ä�");
        }
        set_itimeout(&Sick, xtime);
        u.usick_type |= type;
        context.botl = TRUE;
    } else if (old && (type & u.usick_type)) {
        /* was sick, now not */
        u.usick_type &= ~type;
        if (u.usick_type) { /* only partly cured */
            if (talk)
/*JP
                You_feel("somewhat better.");
*/
                You("����äȤ褯�ʤä���");
            set_itimeout(&Sick, Sick * 2); /* approximation */
        } else {
            if (talk)
/*JP
                You_feel("cured.  What a relief!");
*/
                pline("�������������������ä���");
            Sick = 0L; /* set_itimeout(&Sick, 0L) */
        }
        context.botl = TRUE;
    }

    kptr = find_delayed_killer(SICK);
    if (Sick) {
        exercise(A_CON, FALSE);
        /* setting delayed_killer used to be unconditional, but that's
           not right when make_sick(0) is called to cure food poisoning
           if hero was also fatally ill; this is only approximate */
        if (xtime || !old || !kptr) {
            int kpfx = ((cause && !strcmp(cause, "#wizintrinsic"))
                        ? KILLED_BY : KILLED_BY_AN);

            delayed_killer(SICK, kpfx, cause);
        }
    } else
        dealloc_killer(kptr);
}

void
make_slimed(xtime, msg)
long xtime;
const char *msg;
{
    long old = Slimed;

#if 0   /* tell player even if hero is unconscious */
    if (Unaware)
        msg = 0;
#endif
    set_itimeout(&Slimed, xtime);
    if ((xtime != 0L) ^ (old != 0L)) {
        context.botl = TRUE;
        if (msg)
            pline("%s", msg);
    }
    if (!Slimed) {
        dealloc_killer(find_delayed_killer(SLIMED));
        /* fake appearance is set late in turn-to-slime countdown */
        if (U_AP_TYPE == M_AP_MONSTER
            && youmonst.mappearance == PM_GREEN_SLIME) {
            youmonst.m_ap_type = M_AP_NOTHING;
            youmonst.mappearance = 0;
        }
    }
}

/* start or stop petrification */
void
make_stoned(xtime, msg, killedby, killername)
long xtime;
const char *msg;
int killedby;
const char *killername;
{
    long old = Stoned;

#if 0   /* tell player even if hero is unconscious */
    if (Unaware)
        msg = 0;
#endif
    set_itimeout(&Stoned, xtime);
    if ((xtime != 0L) ^ (old != 0L)) {
        context.botl = TRUE;
        if (msg)
            pline("%s", msg);
    }
    if (!Stoned)
        dealloc_killer(find_delayed_killer(STONED));
    else if (!old)
        delayed_killer(STONED, killedby, killername);
}

void
make_vomiting(xtime, talk)
long xtime;
boolean talk;
{
    long old = Vomiting;

    if (Unaware)
        talk = FALSE;

    set_itimeout(&Vomiting, xtime);
    context.botl = TRUE;
    if (!xtime && old)
        if (talk)
/*JP
            You_feel("much less nauseated now.");
*/
            You("�Ǥ����������ޤä���");
}

/*JP
static const char vismsg[] = "vision seems to %s for a moment but is %s now.";
*/
static const char vismsg[] = "�볦�ϰ��%s�ʤä����ޤ�%s�ʤä���";
/*JP
static const char eyemsg[] = "%s momentarily %s.";
*/
static const char eyemsg[] = "%s�ϰ��%s��";

void
make_blinded(xtime, talk)
long xtime;
boolean talk;
{
    long old = Blinded;
    boolean u_could_see, can_see_now;
#if 0 /*JP*/
    const char *eyes;
#endif

    /* we need to probe ahead in case the Eyes of the Overworld
       are or will be overriding blindness */
    u_could_see = !Blind;
    Blinded = xtime ? 1L : 0L;
    can_see_now = !Blind;
    Blinded = old; /* restore */

    if (Unaware)
        talk = FALSE;

    if (can_see_now && !u_could_see) { /* regaining sight */
        if (talk) {
            if (Hallucination)
/*JP
                pline("Far out!  Everything is all cosmic again!");
*/
                pline("�����ʤˤ⤫�⤬�ޤ������˸����롪");
            else
/*JP
                You("can see again.");
*/
                You("�ޤ�������褦�ˤʤä���");
        }
    } else if (old && !xtime) {
        /* clearing temporary blindness without toggling blindness */
        if (talk) {
            if (!haseyes(youmonst.data)) {
                strange_feeling((struct obj *) 0, (char *) 0);
            } else if (Blindfolded) {
#if 0 /*JP*/
                eyes = body_part(EYE);
                if (eyecount(youmonst.data) != 1)
                    eyes = makeplural(eyes);
                Your(eyemsg, eyes, vtense(eyes, "itch"));
#else
                Your(eyemsg, body_part(EYE), "���椯�ʤä�");
#endif
            } else { /* Eyes of the Overworld */
/*JP
                Your(vismsg, "brighten", Hallucination ? "sadder" : "normal");
*/
                Your(vismsg, "���뤯", Hallucination ? "���Ť�" : "���̤�");
            }
        }
    }

    if (u_could_see && !can_see_now) { /* losing sight */
        if (talk) {
            if (Hallucination)
/*JP
                pline("Oh, bummer!  Everything is dark!  Help!");
*/
                pline("�Ť��衼�������衼�������衼��");
            else
/*JP
                pline("A cloud of darkness falls upon you.");
*/
                pline("�Ź��α������ʤ���ʤ�ä���");
        }
        /* Before the hero goes blind, set the ball&chain variables. */
        if (Punished)
            set_bc(0);
    } else if (!old && xtime) {
        /* setting temporary blindness without toggling blindness */
        if (talk) {
            if (!haseyes(youmonst.data)) {
                strange_feeling((struct obj *) 0, (char *) 0);
            } else if (Blindfolded) {
#if 0 /*JP*/
                eyes = body_part(EYE);
                if (eyecount(youmonst.data) != 1)
                    eyes = makeplural(eyes);
                Your(eyemsg, eyes, vtense(eyes, "twitch"));
#else
                Your(eyemsg, body_part(EYE), "�ԥ��ԥ�����");
#endif
            } else { /* Eyes of the Overworld */
/*JP
                Your(vismsg, "dim", Hallucination ? "happier" : "normal");
*/
                Your(vismsg, "���Ť�", Hallucination ? "�ϥåԡ���" : "���̤�");
            }
        }
    }

    set_itimeout(&Blinded, xtime);

    if (u_could_see ^ can_see_now) { /* one or the other but not both */
        toggle_blindness();
    }
}

/* blindness has just started or just ended--caller enforces that;
   called by Blindf_on(), Blindf_off(), and make_blinded() */
void
toggle_blindness()
{
    boolean Stinging = (uwep && (EWarn_of_mon & W_WEP) != 0L);

    /* blindness has just been toggled */
    context.botl = TRUE; /* status conditions need update */
    vision_full_recalc = 1; /* vision has changed */
    /* this vision recalculation used to be deferred until moveloop(),
       but that made it possible for vision irregularities to occur
       (cited case was force bolt hitting an adjacent potion of blindness
       and then a secret door; hero was blinded by vapors but then got the
       message "a door appears in the wall" because wall spot was IN_SIGHT) */
    vision_recalc(0);
    if (Blind_telepat || Infravision || Stinging)
        see_monsters(); /* also counts EWarn_of_mon monsters */
    /*
     * Avoid either of the sequences
     * "Sting starts glowing", [become blind], "Sting stops quivering" or
     * "Sting starts quivering", [regain sight], "Sting stops glowing"
     * by giving "Sting is quivering" when becoming blind or
     * "Sting is glowing" when regaining sight so that the eventual
     * "stops" message matches the most recent "Sting is ..." one.
     */
    if (Stinging)
        Sting_effects(-1);
    /* update dknown flag for inventory picked up while blind */
    if (!Blind)
        learn_unseen_invent();
}

boolean
make_hallucinated(xtime, talk, mask)
long xtime; /* nonzero if this is an attempt to turn on hallucination */
boolean talk;
long mask; /* nonzero if resistance status should change by mask */
{
    long old = HHallucination;
    boolean changed = 0;
    const char *message, *verb;

    if (Unaware)
        talk = FALSE;

#if 0 /*JP:T*/
    message = (!xtime) ? "Everything %s SO boring now."
                       : "Oh wow!  Everything %s so cosmic!";
#else
    message = (!xtime) ? "���⤫�⤬���������%s�롥"
                       : "��������⤫��������%s�롪";
#endif
/*JP
    verb = (!Blind) ? "looks" : "feels";
*/
    verb = (!Blind) ? "����" : "����";

    if (mask) {
        if (HHallucination)
            changed = TRUE;

        if (!xtime)
            EHalluc_resistance |= mask;
        else
            EHalluc_resistance &= ~mask;
    } else {
        if (!EHalluc_resistance && (!!HHallucination != !!xtime))
            changed = TRUE;
        set_itimeout(&HHallucination, xtime);

        /* clearing temporary hallucination without toggling vision */
        if (!changed && !HHallucination && old && talk) {
            if (!haseyes(youmonst.data)) {
                strange_feeling((struct obj *) 0, (char *) 0);
            } else if (Blind) {
#if 0 /*JP:T*/
                const char *eyes = body_part(EYE);

                if (eyecount(youmonst.data) != 1)
                    eyes = makeplural(eyes);
                Your(eyemsg, eyes, vtense(eyes, "itch"));
#else
                Your(eyemsg, body_part(EYE), "���椯�ʤä�");
#endif
            } else { /* Grayswandir */
/*JP
                Your(vismsg, "flatten", "normal");
*/
                Your(vismsg, "��������", "���̤�");
            }
        }
    }

    if (changed) {
        /* in case we're mimicking an orange (hallucinatory form
           of mimicking gold) update the mimicking's-over message */
        if (!Hallucination)
            eatmupdate();

        if (u.uswallow) {
            swallowed(0); /* redraw swallow display */
        } else {
            /* The see_* routines should be called *before* the pline. */
            see_monsters();
            see_objects();
            see_traps();
        }

        /* for perm_inv and anything similar
        (eg. Qt windowport's equipped items display) */
        update_inventory();

        context.botl = TRUE;
        if (talk)
            pline(message, verb);
    }
    return changed;
}

void
make_deaf(xtime, talk)
long xtime;
boolean talk;
{
    long old = HDeaf;

    if (Unaware)
        talk = FALSE;

    set_itimeout(&HDeaf, xtime);
    if ((xtime != 0L) ^ (old != 0L)) {
        context.botl = TRUE;
        if (talk)
/*JP
            You(old ? "can hear again." : "are unable to hear anything.");
*/
            You(old ? "�ޤ�ʹ������褦�ˤʤä���" : "����ʹ�����ʤ��ʤä���");
    }
}

/* set or clear "slippery fingers" */
void
make_glib(xtime)
int xtime;
{
    set_itimeout(&Glib, xtime);
    /* may change "(being worn)" to "(being worn; slippery)" or vice versa */
    if (uarmg)
        update_inventory();
}

void
self_invis_message()
{
#if 0 /*JP:T*/
    pline("%s %s.",
          Hallucination ? "Far out, man!  You"
                        : "Gee!  All of a sudden, you",
          See_invisible ? "can see right through yourself"
                        : "can't see yourself");
#else
    pline("%s���ʤ���%s��",
          Hallucination ? "�����" : "��������",
          See_invisible ? "��ʬ���Ȥ������ȸ����ʤ��ʤä�"
                        : "��ʬ���Ȥ������ʤ��ʤä�");
#endif
}

STATIC_OVL void
ghost_from_bottle()
{
    struct monst *mtmp = makemon(&mons[PM_GHOST], u.ux, u.uy, NO_MM_FLAGS);

    if (!mtmp) {
/*JP
        pline("This bottle turns out to be empty.");
*/
        pline("�Ӥ϶��äݤ��ä���");
        return;
    }
    if (Blind) {
/*JP
        pline("As you open the bottle, %s emerges.", something);
*/
        pline("�Ӥ򳫤���ȡ��������ФƤ�����");
        return;
    }
#if 0 /*JP:T*/
    pline("As you open the bottle, an enormous %s emerges!",
          Hallucination ? rndmonnam(NULL) : (const char *) "ghost");
#else
    pline("�Ӥ򳫤���ȡ������%s���ФƤ�����",
          Hallucination ? rndmonnam(NULL) : (const char *) "ͩ��");
#endif
    if (flags.verbose)
/*JP
        You("are frightened to death, and unable to move.");
*/
        You("�ޤä����ˤʤäƶä���ư���ʤ��ʤä���");
    nomul(-3);
/*JP
    multi_reason = "being frightened to death";
*/
    multi_reason = "��̤ۤɶä������";
/*JP
    nomovemsg = "You regain your composure.";
*/
    nomovemsg = "���ʤ���ʿ�Ť����ᤷ����";
}

/* "Quaffing is like drinking, except you spill more." - Terry Pratchett */
int
dodrink()
{
    register struct obj *otmp;
    const char *potion_descr;

    if (Strangled) {
/*JP
        pline("If you can't breathe air, how can you drink liquid?");
*/
        pline("©��Ǥ��ʤ��Τˡ��ɤ���äƱ��Τ����������");
        return 0;
    }
    /* Is there a fountain to drink from here? */
    if (IS_FOUNTAIN(levl[u.ux][u.uy].typ)
        /* not as low as floor level but similar restrictions apply */
        && can_reach_floor(FALSE)) {
/*JP
        if (yn("Drink from the fountain?") == 'y') {
*/
        if (yn("���ο����ߤޤ�����") == 'y') {
            drinkfountain();
            return 1;
        }
    }
    /* Or a kitchen sink? */
    if (IS_SINK(levl[u.ux][u.uy].typ)
        /* not as low as floor level but similar restrictions apply */
        && can_reach_floor(FALSE)) {
/*JP
        if (yn("Drink from the sink?") == 'y') {
*/
        if (yn("ή����ο����ߤޤ�����") == 'y') {
            drinksink();
            return 1;
        }
    }
    /* Or are you surrounded by water? */
    if (Underwater && !u.uswallow) {
/*JP
        if (yn("Drink the water around you?") == 'y') {
*/
        if (yn("�ޤ��ο����ߤޤ�����") == 'y') {
/*JP
            pline("Do you know what lives in this water?");
*/
            pline("���ο���ǲ��������Ƥ���Τ��ΤäƤ뤫����");
            return 1;
        }
    }

    otmp = getobj(beverages, "drink");
    if (!otmp)
        return 0;

    /* quan > 1 used to be left to useup(), but we need to force
       the current potion to be unworn, and don't want to do
       that for the entire stack when starting with more than 1.
       [Drinking a wielded potion of polymorph can trigger a shape
       change which causes hero's weapon to be dropped.  In 3.4.x,
       that led to an "object lost" panic since subsequent useup()
       was no longer dealing with an inventory item.  Unwearing
       the current potion is intended to keep it in inventory.] */
    if (otmp->quan > 1L) {
        otmp = splitobj(otmp, 1L);
        otmp->owornmask = 0L; /* rest of original stuck unaffected */
    } else if (otmp->owornmask) {
        remove_worn_item(otmp, FALSE);
    }
    otmp->in_use = TRUE; /* you've opened the stopper */

    potion_descr = OBJ_DESCR(objects[otmp->otyp]);
    if (potion_descr) {
/*JP
        if (!strcmp(potion_descr, "milky")
*/
        if (!strcmp(potion_descr, "�ߥ륯����")
            && !(mvitals[PM_GHOST].mvflags & G_GONE)
            && !rn2(POTION_OCCUPANT_CHANCE(mvitals[PM_GHOST].born))) {
            ghost_from_bottle();
            useup(otmp);
            return 1;
/*JP
        } else if (!strcmp(potion_descr, "smoky")
*/
        } else if (!strcmp(potion_descr, "�줬�ǤƤ���")
                   && !(mvitals[PM_DJINNI].mvflags & G_GONE)
                   && !rn2(POTION_OCCUPANT_CHANCE(mvitals[PM_DJINNI].born))) {
            djinni_from_bottle(otmp);
            useup(otmp);
            return 1;
        }
    }
    return dopotion(otmp);
}

int
dopotion(otmp)
register struct obj *otmp;
{
    int retval;

    otmp->in_use = TRUE;
    nothing = unkn = 0;
    if ((retval = peffects(otmp)) >= 0)
        return retval;

    if (nothing) {
        unkn++;
#if 0 /*JP:T*/
        You("have a %s feeling for a moment, then it passes.",
            Hallucination ? "normal" : "peculiar");
#else
        You("%s��ʬ�ˤ�����줿���������˾ä����ä���",
            Hallucination ? "���̤�" : "���ä�");
#endif
    }
    if (otmp->dknown && !objects[otmp->otyp].oc_name_known) {
        if (!unkn) {
            makeknown(otmp->otyp);
            more_experienced(0, 10);
        } else if (!objects[otmp->otyp].oc_uname)
            docall(otmp);
    }
    useup(otmp);
    return 1;
}

int
peffects(otmp)
register struct obj *otmp;
{
    register int i, ii, lim;

    switch (otmp->otyp) {
    case POT_RESTORE_ABILITY:
    case SPE_RESTORE_ABILITY:
        unkn++;
        if (otmp->cursed) {
/*JP
            pline("Ulch!  This makes you feel mediocre!");
*/
            pline("�����󡤤ɤ��⤵���ʤ��ʤ���");
            break;
        } else {
            /* unlike unicorn horn, overrides Fixed_abil */
#if 0 /*JP:T*/
            pline("Wow!  This makes you feel %s!",
                  (otmp->blessed)
                      ? (unfixable_trouble_count(FALSE) ? "better" : "great")
                      : "good");
#else
            pline("�������ʬ��%s�ʤä���",
                  (otmp->blessed)
                      ? (unfixable_trouble_count(FALSE) ? "�����֤褯" : "�ȤƤ�褯")
                      : "�褯");
#endif
            i = rn2(A_MAX); /* start at a random point */
            for (ii = 0; ii < A_MAX; ii++) {
                lim = AMAX(i);
                /* this used to adjust 'lim' for A_STR when u.uhs was
                   WEAK or worse, but that's handled via ATEMP(A_STR) now */
                if (ABASE(i) < lim) {
                    ABASE(i) = lim;
                    context.botl = 1;
                    /* only first found if not blessed */
                    if (!otmp->blessed)
                        break;
                }
                if (++i >= A_MAX)
                    i = 0;
            }

            /* when using the potion (not the spell) also restore lost levels,
               to make the potion more worth keeping around for players with
               the spell or with a unihorn; this is better than full healing
               in that it can restore all of them, not just half, and a
               blessed potion restores them all at once */
            if (otmp->otyp == POT_RESTORE_ABILITY && u.ulevel < u.ulevelmax) {
                do {
                    pluslvl(FALSE);
                } while (u.ulevel < u.ulevelmax && otmp->blessed);
            }
        }
        break;
    case POT_HALLUCINATION:
        if (Hallucination || Halluc_resistance)
            nothing++;
        (void) make_hallucinated(itimeout_incr(HHallucination,
                                          rn1(200, 600 - 300 * bcsign(otmp))),
                                 TRUE, 0L);
        break;
    case POT_WATER:
        if (!otmp->blessed && !otmp->cursed) {
/*JP
            pline("This tastes like %s.", hliquid("water"));
*/
            pline("%s�Τ褦��̣�����롥", hliquid("��"));
            u.uhunger += rnd(10);
            newuhs(FALSE);
            break;
        }
        unkn++;
        if (is_undead(youmonst.data) || is_demon(youmonst.data)
            || u.ualign.type == A_CHAOTIC) {
            if (otmp->blessed) {
/*JP
                pline("This burns like %s!", hliquid("acid"));
*/
                pline("%s�Τ褦���夬�Ҥ�Ҥꤹ�롪", hliquid("��"));
                exercise(A_CON, FALSE);
                if (u.ulycn >= LOW_PM) {
/*JP
                    Your("affinity to %s disappears!",
*/
                    Your("%s�ؤοƶᴶ�Ϥʤ��ʤä���",
                         makeplural(mons[u.ulycn].mname));
                    if (youmonst.data == &mons[u.ulycn])
                        you_unwere(FALSE);
                    set_ulycn(NON_PM); /* cure lycanthropy */
                }
/*JP
                losehp(Maybe_Half_Phys(d(2, 6)), "potion of holy water",
*/
                losehp(Maybe_Half_Phys(d(2, 6)), "�����",
                       KILLED_BY_AN);
            } else if (otmp->cursed) {
/*JP
                You_feel("quite proud of yourself.");
*/
                You("��º���򴶤�����");
                healup(d(2, 6), 0, 0, 0);
                if (u.ulycn >= LOW_PM && !Upolyd)
                    you_were();
                exercise(A_CON, TRUE);
            }
        } else {
            if (otmp->blessed) {
/*JP
                You_feel("full of awe.");
*/
                You("���ݤ�ǰ�ˤ���줿��");
                make_sick(0L, (char *) 0, TRUE, SICK_ALL);
                exercise(A_WIS, TRUE);
                exercise(A_CON, TRUE);
                if (u.ulycn >= LOW_PM)
                    you_unwere(TRUE); /* "Purified" */
                /* make_confused(0L, TRUE); */
            } else {
                if (u.ualign.type == A_LAWFUL) {
/*JP
                    pline("This burns like %s!", hliquid("acid"));
*/
                    pline("%s�Τ褦���夬�Ҥ�Ҥꤹ�롪", hliquid("��"));
/*JP
                    losehp(Maybe_Half_Phys(d(2, 6)), "potion of unholy water",
*/
                    losehp(Maybe_Half_Phys(d(2, 6)), "�Ծ��ʿ��",
                           KILLED_BY_AN);
                } else
/*JP
                    You_feel("full of dread.");
*/
                    You("���ݤ�ǰ�ˤ���줿��");
                if (u.ulycn >= LOW_PM && !Upolyd)
                    you_were();
                exercise(A_CON, FALSE);
            }
        }
        break;
    case POT_BOOZE:
        unkn++;
#if 0 /*JP:T*/
        pline("Ooph!  This tastes like %s%s!",
              otmp->odiluted ? "watered down " : "",
              Hallucination ? "dandelion wine" : "liquid fire");
#else
        pline("�����äס������%s%s�Τ褦��̣�����롪",
              otmp->odiluted ? "������᤿" : "",
              Hallucination ? "����ݤݤΤ���" : "ǳ��������");
#endif
        if (!otmp->blessed)
            make_confused(itimeout_incr(HConfusion, d(3, 8)), FALSE);
        /* the whiskey makes us feel better */
        if (!otmp->odiluted)
            healup(1, 0, FALSE, FALSE);
        u.uhunger += 10 * (2 + bcsign(otmp));
        newuhs(FALSE);
        exercise(A_WIS, FALSE);
        if (otmp->cursed) {
/*JP
            You("pass out.");
*/
            You("���䤷����");
            multi = -rnd(15);
/*JP
            nomovemsg = "You awake with a headache.";
*/
            nomovemsg = "�ܤ����᤿��Ƭ�ˤ����롥";
        }
        break;
    case POT_ENLIGHTENMENT:
        if (otmp->cursed) {
            unkn++;
/*JP
            You("have an uneasy feeling...");
*/
            You("�԰¤ʵ����ˤʤä�������");
            exercise(A_WIS, FALSE);
        } else {
            if (otmp->blessed) {
                (void) adjattrib(A_INT, 1, FALSE);
                (void) adjattrib(A_WIS, 1, FALSE);
            }
/*JP
            You_feel("self-knowledgeable...");
*/
            You("��ʬ���Ȥ�Ƚ��褦�ʵ�������������");
            display_nhwindow(WIN_MESSAGE, FALSE);
            enlightenment(MAGICENLIGHTENMENT, ENL_GAMEINPROGRESS);
/*JP
            pline_The("feeling subsides.");
*/
            pline("���δ����Ϥʤ��ʤä���");
            exercise(A_WIS, TRUE);
        }
        break;
    case SPE_INVISIBILITY:
        /* spell cannot penetrate mummy wrapping */
        if (BInvis && uarmc->otyp == MUMMY_WRAPPING) {
/*JP
            You_feel("rather itchy under %s.", yname(uarmc));
*/
            You("%s�β����ॺ�ॺ������", xname(uarmc));
            break;
        }
        /* FALLTHRU */
    case POT_INVISIBILITY:
        if (Invis || Blind || BInvis) {
            nothing++;
        } else {
            self_invis_message();
        }
        if (otmp->blessed)
            HInvis |= FROMOUTSIDE;
        else
            incr_itimeout(&HInvis, rn1(15, 31));
        newsym(u.ux, u.uy); /* update position */
        if (otmp->cursed) {
/*JP
            pline("For some reason, you feel your presence is known.");
*/
            pline("�ʤ�����¸�ߤ��Τ��Ƥ���褦�ʵ���������");
            aggravate();
        }
        break;
    case POT_SEE_INVISIBLE: /* tastes like fruit juice in Rogue */
    case POT_FRUIT_JUICE: {
        int msg = Invisible && !Blind;

        unkn++;
        if (otmp->cursed)
#if 0 /*JP:T*/
            pline("Yecch!  This tastes %s.",
                  Hallucination ? "overripe" : "rotten");
#else
            pline("�������������%s���塼����̣�����롥",
                  Hallucination ? "�Ϥ�������" : "��ä�");
#endif
        else
#if 0 /*JP:T*/
            pline(
                Hallucination
                    ? "This tastes like 10%% real %s%s all-natural beverage."
                    : "This tastes like %s%s.",
                otmp->odiluted ? "reconstituted " : "", fruitname(TRUE));
#else
            pline(
                Hallucination
                    ? "10%%%s�ν㼫�������Τ褦��̣�����롥"
                    : "%s%s���塼���Τ褦��̣�����롥",
                otmp->odiluted ? "��ʬĴ�����줿" : "", fruitname(TRUE));
#endif
        if (otmp->otyp == POT_FRUIT_JUICE) {
            u.uhunger += (otmp->odiluted ? 5 : 10) * (2 + bcsign(otmp));
            newuhs(FALSE);
            break;
        }
        if (!otmp->cursed) {
            /* Tell them they can see again immediately, which
             * will help them identify the potion...
             */
            make_blinded(0L, TRUE);
        }
        if (otmp->blessed)
            HSee_invisible |= FROMOUTSIDE;
        else
            incr_itimeout(&HSee_invisible, rn1(100, 750));
        set_mimic_blocking(); /* do special mimic handling */
        see_monsters();       /* see invisible monsters */
        newsym(u.ux, u.uy);   /* see yourself! */
        if (msg && !Blind) {  /* Blind possible if polymorphed */
/*JP
            You("can see through yourself, but you are visible!");
*/
            You("Ʃ���Ǥ��롥������������褦�ˤʤä���");
            unkn--;
        }
        break;
    }
    case POT_PARALYSIS:
        if (Free_action) {
/*JP
            You("stiffen momentarily.");
*/
            You("���ư���ʤ��ʤä���");
        } else {
            if (Levitation || Is_airlevel(&u.uz) || Is_waterlevel(&u.uz))
/*JP
                You("are motionlessly suspended.");
*/
                You("�����ư���ʤ��ʤä���");
            else if (u.usteed)
/*JP
                You("are frozen in place!");
*/
                You("���ξ��ư���ʤ��ʤä���");
            else
#if 0 /*JP:T*/
                Your("%s are frozen to the %s!", makeplural(body_part(FOOT)),
                     surface(u.ux, u.uy));
#else
                You("ư���ʤ��ʤä���");
#endif
            nomul(-(rn1(10, 25 - 12 * bcsign(otmp))));
/*JP
            multi_reason = "frozen by a potion";
*/
            multi_reason = "���ǹ�ľ���Ƥ������";
            nomovemsg = You_can_move_again;
            exercise(A_DEX, FALSE);
        }
        break;
    case POT_SLEEPING:
        if (Sleep_resistance || Free_action) {
/*JP
            You("yawn.");
*/
            You("�����Ӥ򤷤���");
        } else {
/*JP
            You("suddenly fall asleep!");
*/
            pline("����̲�äƤ��ޤä���");
            fall_asleep(-rn1(10, 25 - 12 * bcsign(otmp)), TRUE);
        }
        break;
    case POT_MONSTER_DETECTION:
    case SPE_DETECT_MONSTERS:
        if (otmp->blessed) {
            int x, y;

            if (Detect_monsters)
                nothing++;
            unkn++;
            /* after a while, repeated uses become less effective */
            if ((HDetect_monsters & TIMEOUT) >= 300L)
                i = 1;
            else
                i = rn1(40, 21);
            incr_itimeout(&HDetect_monsters, i);
            for (x = 1; x < COLNO; x++) {
                for (y = 0; y < ROWNO; y++) {
                    if (levl[x][y].glyph == GLYPH_INVISIBLE) {
                        unmap_object(x, y);
                        newsym(x, y);
                    }
                    if (MON_AT(x, y))
                        unkn = 0;
                }
            }
            see_monsters();
            if (unkn)
/*JP
                You_feel("lonely.");
*/
                You("���٤��ʤä���");
            break;
        }
        if (monster_detect(otmp, 0))
            return 1; /* nothing detected */
        exercise(A_WIS, TRUE);
        break;
    case POT_OBJECT_DETECTION:
    case SPE_DETECT_TREASURE:
        if (object_detect(otmp, 0))
            return 1; /* nothing detected */
        exercise(A_WIS, TRUE);
        break;
    case POT_SICKNESS:
/*JP
        pline("Yecch!  This stuff tastes like poison.");
*/
        pline("���������ǤΤ褦��̣�����롥");
        if (otmp->blessed) {
/*JP
            pline("(But in fact it was mildly stale %s.)", fruitname(TRUE));
*/
            pline("(�������ºݤ���Ͼ����Ť��ʤä�%s��)", fruitname(TRUE));
            if (!Role_if(PM_HEALER)) {
                /* NB: blessed otmp->fromsink is not possible */
/*JP
                losehp(1, "mildly contaminated potion", KILLED_BY_AN);
*/
                losehp(1, "�µ��˱������줿����", KILLED_BY_AN);
            }
        } else {
            if (Poison_resistance)
/*JP
                pline("(But in fact it was biologically contaminated %s.)",
*/
                pline("(�������ºݤ������ʪ��Ū�˱������줿%s����)",
                      fruitname(TRUE));
            if (Role_if(PM_HEALER)) {
/*JP
                pline("Fortunately, you have been immunized.");
*/
                pline("�����ʤ��Ȥˡ����ʤ����ȱ֤����롥");
            } else {
                char contaminant[BUFSZ];
                int typ = rn2(A_MAX);

#if 0 /*JP:T*/
                Sprintf(contaminant, "%s%s",
                        (Poison_resistance) ? "mildly " : "",
                        (otmp->fromsink) ? "contaminated tap water"
                                         : "contaminated potion");
#else
                Sprintf(contaminant, "%s�������줿%s��",
                        (Poison_resistance) ? "����" : "",
                        (otmp->fromsink) ? "��"
                                         : "��");
#endif
                if (!Fixed_abil) {
                    poisontell(typ, FALSE);
                    (void) adjattrib(typ, Poison_resistance ? -1 : -rn1(4, 3),
                                     1);
                }
                if (!Poison_resistance) {
                    if (otmp->fromsink)
                        losehp(rnd(10) + 5 * !!(otmp->cursed), contaminant,
                               KILLED_BY);
                    else
                        losehp(rnd(10) + 5 * !!(otmp->cursed), contaminant,
                               KILLED_BY_AN);
                } else {
                    /* rnd loss is so that unblessed poorer than blessed */
                    losehp(1 + rn2(2), contaminant,
                           (otmp->fromsink) ? KILLED_BY : KILLED_BY_AN);
                }
                exercise(A_CON, FALSE);
            }
        }
        if (Hallucination) {
/*JP
            You("are shocked back to your senses!");
*/
            You("�޴��˾׷���������");
            (void) make_hallucinated(0L, FALSE, 0L);
        }
        break;
    case POT_CONFUSION:
        if (!Confusion) {
            if (Hallucination) {
/*JP
                pline("What a trippy feeling!");
*/
                pline("�ʤ󤫥إ�إ��롪");
                unkn++;
            } else
/*JP
                pline("Huh, What?  Where am I?");
*/
                pline("�ۤ������ï��");
        } else
            nothing++;
        make_confused(itimeout_incr(HConfusion,
                                    rn1(7, 16 - 8 * bcsign(otmp))),
                      FALSE);
        break;
    case POT_GAIN_ABILITY:
        if (otmp->cursed) {
/*JP
            pline("Ulch!  That potion tasted foul!");
*/
            pline("���������������롪");
            unkn++;
        } else if (Fixed_abil) {
            nothing++;
        } else {      /* If blessed, increase all; if not, try up to */
            int itmp; /* 6 times to find one which can be increased. */

            i = -1;   /* increment to 0 */
            for (ii = A_MAX; ii > 0; ii--) {
                i = (otmp->blessed ? i + 1 : rn2(A_MAX));
                /* only give "your X is already as high as it can get"
                   message on last attempt (except blessed potions) */
                itmp = (otmp->blessed || ii == 1) ? 0 : -1;
                if (adjattrib(i, 1, itmp) && !otmp->blessed)
                    break;
            }
        }
        break;
    case POT_SPEED:
        /* skip when mounted; heal_legs() would heal steed's legs */
        if (Wounded_legs && !otmp->cursed && !u.usteed) {
            heal_legs(0);
            unkn++;
            break;
        }
        /* FALLTHRU */
    case SPE_HASTE_SELF:
        if (!Very_fast) { /* wwf@doe.carleton.ca */
/*JP
            You("are suddenly moving %sfaster.", Fast ? "" : "much ");
*/
            You("����%s®����ư�Ǥ���褦�ˤʤä���", Fast ? "" : "�ȤƤ�");
        } else {
/*JP
            Your("%s get new energy.", makeplural(body_part(LEG)));
*/
            pline("%s�˥��ͥ륮���������ޤ��褦�ʴ�����������", body_part(LEG));
            unkn++;
        }
        exercise(A_DEX, TRUE);
        incr_itimeout(&HFast, rn1(10, 100 + 60 * bcsign(otmp)));
        break;
    case POT_BLINDNESS:
        if (Blind)
            nothing++;
        make_blinded(itimeout_incr(Blinded,
                                   rn1(200, 250 - 125 * bcsign(otmp))),
                     (boolean) !Blind);
        break;
    case POT_GAIN_LEVEL:
        if (otmp->cursed) {
            unkn++;
            /* they went up a level */
            if ((ledger_no(&u.uz) == 1 && u.uhave.amulet)
                || Can_rise_up(u.ux, u.uy, &u.uz)) {
/*JP
                const char *riseup = "rise up, through the %s!";
*/
                const char *riseup ="%s���ͤ�ȴ������";

                if (ledger_no(&u.uz) == 1) {
                    You(riseup, ceiling(u.ux, u.uy));
                    goto_level(&earth_level, FALSE, FALSE, FALSE);
                } else {
                    register int newlev = depth(&u.uz) - 1;
                    d_level newlevel;

                    get_level(&newlevel, newlev);
                    if (on_level(&newlevel, &u.uz)) {
/*JP
                        pline("It tasted bad.");
*/
                        pline("�ȤƤ�ޤ�����");
                        break;
                    } else
                        You(riseup, ceiling(u.ux, u.uy));
                    goto_level(&newlevel, FALSE, FALSE, FALSE);
                }
            } else
/*JP
                You("have an uneasy feeling.");
*/
                You("�԰¤ʵ����ˤʤä���");
            break;
        }
        pluslvl(FALSE);
        /* blessed potions place you at a random spot in the
           middle of the new level instead of the low point */
        if (otmp->blessed)
            u.uexp = rndexp(TRUE);
        break;
    case POT_HEALING:
/*JP
        You_feel("better.");
*/
        You("��ʬ���褯�ʤä���");
        healup(d(6 + 2 * bcsign(otmp), 4), !otmp->cursed ? 1 : 0,
               !!otmp->blessed, !otmp->cursed);
        exercise(A_CON, TRUE);
        break;
    case POT_EXTRA_HEALING:
/*JP
        You_feel("much better.");
*/
        You("��ʬ���ȤƤ�褯�ʤä���");
        healup(d(6 + 2 * bcsign(otmp), 8),
               otmp->blessed ? 5 : !otmp->cursed ? 2 : 0, !otmp->cursed,
               TRUE);
        (void) make_hallucinated(0L, TRUE, 0L);
        exercise(A_CON, TRUE);
        exercise(A_STR, TRUE);
        break;
    case POT_FULL_HEALING:
/*JP
        You_feel("completely healed.");
*/
        You("�����˲���������");
        healup(400, 4 + 4 * bcsign(otmp), !otmp->cursed, TRUE);
        /* Restore one lost level if blessed */
        if (otmp->blessed && u.ulevel < u.ulevelmax) {
            /* when multiple levels have been lost, drinking
               multiple potions will only get half of them back */
            u.ulevelmax -= 1;
            pluslvl(FALSE);
        }
        (void) make_hallucinated(0L, TRUE, 0L);
        exercise(A_STR, TRUE);
        exercise(A_CON, TRUE);
        break;
    case POT_LEVITATION:
    case SPE_LEVITATION:
        /*
         * BLevitation will be set if levitation is blocked due to being
         * inside rock (currently or formerly in phazing xorn form, perhaps)
         * but it doesn't prevent setting or incrementing Levitation timeout
         * (which will take effect after escaping from the rock if it hasn't
         * expired by then).
         */
        if (!Levitation && !BLevitation) {
            /* kludge to ensure proper operation of float_up() */
            set_itimeout(&HLevitation, 1L);
            float_up();
            /* This used to set timeout back to 0, then increment it below
               for blessed and uncursed effects.  But now we leave it so
               that cursed effect yields "you float down" on next turn.
               Blessed and uncursed get one extra turn duration. */
        } else /* already levitating, or can't levitate */
            nothing++;

        if (otmp->cursed) {
            /* 'already levitating' used to block the cursed effect(s)
               aside from ~I_SPECIAL; it was not clear whether that was
               intentional; either way, it no longer does (as of 3.6.1) */
            HLevitation &= ~I_SPECIAL; /* can't descend upon demand */
            if (BLevitation) {
                ; /* rising via levitation is blocked */
            } else if ((u.ux == xupstair && u.uy == yupstair)
                    || (sstairs.up && u.ux == sstairs.sx && u.uy == sstairs.sy)
                    || (xupladder && u.ux == xupladder && u.uy == yupladder)) {
                (void) doup();
                /* in case we're already Levitating, which would have
                   resulted in incrementing 'nothing' */
                nothing = 0; /* not nothing after all */
            } else if (has_ceiling(&u.uz)) {
                int dmg = rnd(!uarmh ? 10 : !is_metallic(uarmh) ? 6 : 3);

#if 0 /*JP:T*/
                You("hit your %s on the %s.", body_part(HEAD),
                    ceiling(u.ux, u.uy));
#else
                You("%s��%s�ˤ֤Ĥ�����", body_part(HEAD),
                    ceiling(u.ux,u.uy));
#endif
/*JP
                losehp(Maybe_Half_Phys(dmg), "colliding with the ceiling",
*/
                losehp(Maybe_Half_Phys(dmg), "ŷ���Ƭ��֤Ĥ���",
                       KILLED_BY);
                nothing = 0; /* not nothing after all */
            }
        } else if (otmp->blessed) {
            /* at this point, timeout is already at least 1 */
            incr_itimeout(&HLevitation, rn1(50, 250));
            /* can descend at will (stop levitating via '>') provided timeout
               is the only factor (ie, not also wearing Lev ring or boots) */
            HLevitation |= I_SPECIAL;
        } else /* timeout is already at least 1 */
            incr_itimeout(&HLevitation, rn1(140, 10));

        if (Levitation && IS_SINK(levl[u.ux][u.uy].typ))
            spoteffects(FALSE);
        /* levitating blocks flying */
        float_vs_flight();
        break;
    case POT_GAIN_ENERGY: { /* M. Stephenson */
        int num;

        if (otmp->cursed)
/*JP
            You_feel("lackluster.");
*/
            You("�յ�����������");
        else
/*JP
            pline("Magical energies course through your body.");
*/
            pline("��ˡ�Υ��ͥ륮�������ʤ����Τ���������");

        /* old: num = rnd(5) + 5 * otmp->blessed + 1;
         *      blessed:  +7..11 max & current (+9 avg)
         *      uncursed: +2.. 6 max & current (+4 avg)
         *      cursed:   -2.. 6 max & current (-4 avg)
         * new: (3.6.0)
         *      blessed:  +3..18 max (+10.5 avg), +9..54 current (+31.5 avg)
         *      uncursed: +2..12 max (+ 7   avg), +6..36 current (+21   avg)
         *      cursed:   -1.. 6 max (- 3.5 avg), -3..18 current (-10.5 avg)
         */
        num = d(otmp->blessed ? 3 : !otmp->cursed ? 2 : 1, 6);
        if (otmp->cursed)
            num = -num; /* subtract instead of add when cursed */
        u.uenmax += num;
        if (u.uenmax <= 0)
            u.uenmax = 0;
        u.uen += 3 * num;
        if (u.uen > u.uenmax)
            u.uen = u.uenmax;
        else if (u.uen <= 0)
            u.uen = 0;
        context.botl = 1;
        exercise(A_WIS, TRUE);
        break;
    }
    case POT_OIL: { /* P. Winner */
        boolean good_for_you = FALSE;

        if (otmp->lamplit) {
            if (likes_fire(youmonst.data)) {
/*JP
                pline("Ahh, a refreshing drink.");
*/
                pline("����������֤롥");
                good_for_you = TRUE;
            } else {
/*JP
                You("burn your %s.", body_part(FACE));
*/
                Your("%s�Ϲ��Ǥ��ˤʤä���", body_part(FACE));
                /* fire damage */
/*JP
                losehp(d(Fire_resistance ? 1 : 3, 4), "burning potion of oil",
*/
                losehp(d(Fire_resistance ? 1 : 3, 4), "ǳ���Ƥ�����������",
                       KILLED_BY_AN);
            }
        } else if (otmp->cursed)
/*JP
            pline("This tastes like castor oil.");
*/
            pline("�Ҥޤ����Τ褦��̣�����롥");
        else
/*JP
            pline("That was smooth!");
*/
            pline("�������꤬�褤��");
        exercise(A_WIS, good_for_you);
        break;
    }
    case POT_ACID:
        if (Acid_resistance) {
            /* Not necessarily a creature who _likes_ acid */
/*JP
            pline("This tastes %s.", Hallucination ? "tangy" : "sour");
*/
            pline("%ṣ�����롥", Hallucination ? "�Ԥ�äȤ���" : "����");
        } else {
            int dmg;

#if 0 /*JP:T*/
            pline("This burns%s!",
                  otmp->blessed ? " a little" : otmp->cursed ? " a lot"
                                                             : " like acid");
#else
            pline("%s�Ǥ�����",
                  otmp->blessed ? "����" : otmp->cursed ? "������"
                                                        : "");
#endif
            dmg = d(otmp->cursed ? 2 : 1, otmp->blessed ? 4 : 8);
/*JP
            losehp(Maybe_Half_Phys(dmg), "potion of acid", KILLED_BY_AN);
*/
            losehp(Maybe_Half_Phys(dmg), "������������", KILLED_BY_AN);
            exercise(A_CON, FALSE);
        }
        if (Stoned)
            fix_petrification();
        unkn++; /* holy/unholy water can burn like acid too */
        break;
    case POT_POLYMORPH:
/*JP
        You_feel("a little %s.", Hallucination ? "normal" : "strange");
*/
        You("����%s�ʴ�����������", Hallucination ? "����" : "��");
        if (!Unchanging)
            polyself(0);
        break;
    default:
        impossible("What a funny potion! (%u)", otmp->otyp);
        return 0;
    }
    return -1;
}

#ifdef  JPEXTENSION
void
make_totter(xtime, talk)
long xtime;     /* nonzero if this is an attempt to turn on hallucination */
boolean talk;
{
        const char *message = 0;

        if (!xtime)
            message = "�������Ф�����ˤʤä���";
        else
            message = "�������Ф����㤷����";

        set_itimeout(&Totter, xtime);
        pline(message);
}
#endif

void
healup(nhp, nxtra, curesick, cureblind)
int nhp, nxtra;
register boolean curesick, cureblind;
{
    if (nhp) {
        if (Upolyd) {
            u.mh += nhp;
            if (u.mh > u.mhmax)
                u.mh = (u.mhmax += nxtra);
        } else {
            u.uhp += nhp;
            if (u.uhp > u.uhpmax)
                u.uhp = (u.uhpmax += nxtra);
        }
    }
    if (cureblind) {
        /* 3.6.1: it's debatible whether healing magic should clean off
           mundane 'dirt', but if it doesn't, blindness isn't cured */
        u.ucreamed = 0;
        make_blinded(0L, TRUE);
        /* heal deafness too */
        make_deaf(0L, TRUE);
    }
    if (curesick) {
        make_vomiting(0L, TRUE);
        make_sick(0L, (char *) 0, TRUE, SICK_ALL);
    }
    context.botl = 1;
    return;
}

void
strange_feeling(obj, txt)
struct obj *obj;
const char *txt;
{
    if (flags.beginner || !txt)
#if 0 /*JP:T*/
        You("have a %s feeling for a moment, then it passes.",
            Hallucination ? "normal" : "strange");
#else
        You("%s��ʬ�ˤ�����줿���������˾ä����ä���",
            Hallucination ? "���̤�" : "��̯��");
#endif
    else
        pline1(txt);

    if (!obj) /* e.g., crystal ball finds no traps */
        return;

    if (obj->dknown && !objects[obj->otyp].oc_name_known
        && !objects[obj->otyp].oc_uname)
        docall(obj);

    useup(obj);
}

#if 0 /*JP:T*/
const char *bottlenames[] = { "bottle", "phial", "flagon", "carafe",
                              "flask",  "jar",   "vial" };
#else
const char *bottlenames[] = { "��", "������", "�쾣��", "�庹��",
                              "�ե饹��", "��", "���饹��" };
#endif

const char *
bottlename()
{
    return bottlenames[rn2(SIZE(bottlenames))];
}

/* handle item dipped into water potion or steed saddle splashed by same */
STATIC_OVL boolean
H2Opotion_dip(potion, targobj, useeit, objphrase)
struct obj *potion, *targobj;
boolean useeit;
const char *objphrase; /* "Your widget glows" or "Steed's saddle glows" */
{
    void FDECL((*func), (OBJ_P)) = 0;
    const char *glowcolor = 0;
#define COST_alter (-2)
#define COST_none (-1)
    int costchange = COST_none;
    boolean altfmt = FALSE, res = FALSE;

    if (!potion || potion->otyp != POT_WATER)
        return FALSE;

    if (potion->blessed) {
        if (targobj->cursed) {
            func = uncurse;
            glowcolor = NH_AMBER;
            costchange = COST_UNCURS;
        } else if (!targobj->blessed) {
            func = bless;
            glowcolor = NH_LIGHT_BLUE;
            costchange = COST_alter;
            altfmt = TRUE; /* "with a <color> aura" */
        }
    } else if (potion->cursed) {
        if (targobj->blessed) {
            func = unbless;
/*JP
            glowcolor = "brown";
*/
            glowcolor = "�㿧��";
            costchange = COST_UNBLSS;
        } else if (!targobj->cursed) {
            func = curse;
            glowcolor = NH_BLACK;
            costchange = COST_alter;
            altfmt = TRUE;
        }
    } else {
        /* dipping into uncursed water; carried() check skips steed saddle */
        if (carried(targobj)) {
            if (water_damage(targobj, 0, TRUE) != ER_NOTHING)
                res = TRUE;
        }
    }
    if (func) {
        /* give feedback before altering the target object;
           this used to set obj->bknown even when not seeing
           the effect; now hero has to see the glow, and bknown
           is cleared instead of set if perception is distorted */
        if (useeit) {
            glowcolor = hcolor(glowcolor);
            /*JP:3.6.0�����Ǥ�ư���"glow"�����ʤΤǷ����*/
            if (altfmt)
#if 0 /*JP:T*/
                pline("%s with %s aura.", objphrase, an(glowcolor));
#else
                pline("%s��%s������ˤĤĤޤ줿��", objphrase, glowcolor);
#endif
            else
#if 0 /*JP:T*/
                pline("%s %s.", objphrase, glowcolor);
#else
                pline("%s��%s��������", objphrase, jconj_adj(glowcolor));
#endif
            iflags.last_msg = PLNMSG_OBJ_GLOWS;
            targobj->bknown = !Hallucination;
        } else {
            /* didn't see what happened:  forget the BUC state if that was
               known unless the bless/curse state of the water is known;
               without this, hero would know the new state even without
               seeing the glow; priest[ess] will immediately relearn it */
            if (!potion->bknown || !potion->dknown)
                targobj->bknown = 0;
            /* [should the bknown+dknown exception require that water
               be discovered or at least named?] */
        }
        /* potions of water are the only shop goods whose price depends
           on their curse/bless state */
        if (targobj->unpaid && targobj->otyp == POT_WATER) {
            if (costchange == COST_alter)
                /* added blessing or cursing; update shop
                   bill to reflect item's new higher price */
                alter_cost(targobj, 0L);
            else if (costchange != COST_none)
                /* removed blessing or cursing; you
                   degraded it, now you'll have to buy it... */
                costly_alteration(targobj, costchange);
        }
        /* finally, change curse/bless state */
        (*func)(targobj);
        res = TRUE;
    }
    return res;
}

/* potion obj hits monster mon, which might be youmonst; obj always used up */
void
potionhit(mon, obj, how)
struct monst *mon;
struct obj *obj;
int how;
{
    const char *botlnam = bottlename();
    boolean isyou = (mon == &youmonst);
    int distance, tx, ty;
    struct obj *saddle = (struct obj *) 0;
    boolean hit_saddle = FALSE, your_fault = (how <= POTHIT_HERO_THROW);

    if (isyou) {
        tx = u.ux, ty = u.uy;
        distance = 0;
#if 0 /*JP:T*/
        pline_The("%s crashes on your %s and breaks into shards.", botlnam,
                  body_part(HEAD));
#else
        pline("%s�����ʤ���%s�ξ�ǲ������ҤȤʤä���", botlnam,
                  body_part(HEAD));
#endif
#if 0 /*JP*/
        losehp(Maybe_Half_Phys(rnd(2)),
               (how == POTHIT_OTHER_THROW) ? "propelled potion" /* scatter */
                                           : "thrown potion",
               KILLED_BY_AN);
#else /*�ɤ������ꤲ��줿�פǤ褤*/
        losehp(Maybe_Half_Phys(rnd(2)), "�ꤲ��줿����", KILLED_BY_AN);
#endif
    } else {
        tx = mon->mx, ty = mon->my;
        /* sometimes it hits the saddle */
        if (((mon->misc_worn_check & W_SADDLE)
             && (saddle = which_armor(mon, W_SADDLE)))
            && (!rn2(10)
                || (obj->otyp == POT_WATER
                    && ((rnl(10) > 7 && obj->cursed)
                        || (rnl(10) < 4 && obj->blessed) || !rn2(3)))))
            hit_saddle = TRUE;
        distance = distu(tx, ty);
        if (!cansee(tx, ty)) {
/*JP
            pline("Crash!");
*/
            pline("�������");
        } else {
            char *mnam = mon_nam(mon);
            char buf[BUFSZ];

            if (hit_saddle && saddle) {
#if 0 /*JP:T*/
                Sprintf(buf, "%s saddle",
                        s_suffix(x_monnam(mon, ARTICLE_THE, (char *) 0,
                                          (SUPPRESS_IT | SUPPRESS_SADDLE),
                                          FALSE)));
#else
                Sprintf(buf, "%s�ΰ�",
                        x_monnam(mon, ARTICLE_THE, (char *) 0,
                                          (SUPPRESS_IT | SUPPRESS_SADDLE),
                                          FALSE));
#endif
            } else if (has_head(mon->data)) {
#if 0 /*JP:T*/
                Sprintf(buf, "%s %s", s_suffix(mnam),
                        (notonhead ? "body" : "head"));
#else
                Sprintf(buf, "%s��%s", mnam,
                        (notonhead ? "��" : "Ƭ"));
#endif
            } else {
                Strcpy(buf, mnam);
            }
#if 0 /*JP:T*/
            pline_The("%s crashes on %s and breaks into shards.", botlnam,
                      buf);
#else
            pline("%s��%s�ξ�ǲ������ҤȤʤä���", botlnam,
                      buf);
#endif
        }
        if (rn2(5) && mon->mhp > 1 && !hit_saddle)
            mon->mhp--;
    }

    /* oil doesn't instantly evaporate; Neither does a saddle hit */
    if (obj->otyp != POT_OIL && !hit_saddle && cansee(tx, ty))
/*JP
        pline("%s.", Tobjnam(obj, "evaporate"));
*/
        pline("%s�Ͼ�ȯ������", xname(obj));

    if (isyou) {
        switch (obj->otyp) {
        case POT_OIL:
            if (obj->lamplit)
                explode_oil(obj, u.ux, u.uy);
            break;
        case POT_POLYMORPH:
/*JP
            You_feel("a little %s.", Hallucination ? "normal" : "strange");
*/
            You("%s�ʴ�����������", Hallucination ? "����" : "��");
            if (!Unchanging && !Antimagic)
                polyself(0);
            break;
        case POT_ACID:
            if (!Acid_resistance) {
                int dmg;

#if 0 /*JP:T*/
                pline("This burns%s!",
                      obj->blessed ? " a little"
                                   : obj->cursed ? " a lot" : "");
#else
                pline("%sǳ������",
                      obj->blessed ? "����"
                                   : obj->cursed ? "�Ϥ�����" : "");
#endif
                dmg = d(obj->cursed ? 2 : 1, obj->blessed ? 4 : 8);
/*JP
                losehp(Maybe_Half_Phys(dmg), "potion of acid", KILLED_BY_AN);
*/
                losehp(Maybe_Half_Phys(dmg), "����������Ӥ�", KILLED_BY_AN);
            }
            break;
        }
    } else if (hit_saddle && saddle) {
        char *mnam, buf[BUFSZ], saddle_glows[BUFSZ];
        boolean affected = FALSE;
        boolean useeit = !Blind && canseemon(mon) && cansee(tx, ty);

        mnam = x_monnam(mon, ARTICLE_THE, (char *) 0,
                        (SUPPRESS_IT | SUPPRESS_SADDLE), FALSE);
        Sprintf(buf, "%s", upstart(s_suffix(mnam)));

        switch (obj->otyp) {
        case POT_WATER:
/*JP
            Sprintf(saddle_glows, "%s %s", buf, aobjnam(saddle, "glow"));
*/
            Sprintf(saddle_glows, "%s", buf);
            affected = H2Opotion_dip(obj, saddle, useeit, saddle_glows);
            break;
        case POT_POLYMORPH:
            /* Do we allow the saddle to polymorph? */
            break;
        }
        if (useeit && !affected)
/*JP
            pline("%s %s wet.", buf, aobjnam(saddle, "get"));
*/
            pline("%s��Ǩ�줿��", buf);
    } else {
        boolean angermon = your_fault, cureblind = FALSE;

        switch (obj->otyp) {
        case POT_FULL_HEALING:
            cureblind = TRUE;
            /*FALLTHRU*/
        case POT_EXTRA_HEALING:
            if (!obj->cursed)
                cureblind = TRUE;
            /*FALLTHRU*/
        case POT_HEALING:
            if (obj->blessed)
                cureblind = TRUE;
            if (mon->data == &mons[PM_PESTILENCE])
                goto do_illness;
            /*FALLTHRU*/
        case POT_RESTORE_ABILITY:
        case POT_GAIN_ABILITY:
 do_healing:
            angermon = FALSE;
            if (mon->mhp < mon->mhpmax) {
                mon->mhp = mon->mhpmax;
                if (canseemon(mon))
/*JP
                    pline("%s looks sound and hale again.", Monnam(mon));
*/
                    pline("%s�ϸ����ˤʤä��褦�˸����롥", Monnam(mon));
            }
            if (cureblind)
                mcureblindness(mon, canseemon(mon));
            break;
        case POT_SICKNESS:
            if (mon->data == &mons[PM_PESTILENCE])
                goto do_healing;
            if (dmgtype(mon->data, AD_DISE)
                /* won't happen, see prior goto */
                || dmgtype(mon->data, AD_PEST)
                /* most common case */
                || resists_poison(mon)) {
                if (canseemon(mon))
#if 0 /*JP:T*/
                    pline("%s looks unharmed.", Monnam(mon));
#else
                    pline("%s�Ϥʤ�Ȥ�ʤ��褦����", Monnam(mon));
#endif
                break;
            }
 do_illness:
            if ((mon->mhpmax > 3) && !resist(mon, POTION_CLASS, 0, NOTELL))
                mon->mhpmax /= 2;
            if ((mon->mhp > 2) && !resist(mon, POTION_CLASS, 0, NOTELL))
                mon->mhp /= 2;
            if (mon->mhp > mon->mhpmax)
                mon->mhp = mon->mhpmax;
            if (canseemon(mon))
/*JP
                pline("%s looks rather ill.", Monnam(mon));
*/
                pline("%s���µ��äݤ������롥", Monnam(mon));
            break;
        case POT_CONFUSION:
        case POT_BOOZE:
            if (!resist(mon, POTION_CLASS, 0, NOTELL))
                mon->mconf = TRUE;
            break;
        case POT_INVISIBILITY: {
            boolean sawit = canspotmon(mon);

            angermon = FALSE;
            mon_set_minvis(mon);
            if (sawit && !canspotmon(mon) && cansee(mon->mx, mon->my))
                map_invisible(mon->mx, mon->my);
            break;
        }
        case POT_SLEEPING:
            /* wakeup() doesn't rouse victims of temporary sleep */
            if (sleep_monst(mon, rnd(12), POTION_CLASS)) {
/*JP
                pline("%s falls asleep.", Monnam(mon));
*/
                pline("%s��̲�äƤ��ޤä���", Monnam(mon));
                slept_monst(mon);
            }
            break;
        case POT_PARALYSIS:
            if (mon->mcanmove) {
                /* really should be rnd(5) for consistency with players
                 * breathing potions, but...
                 */
                paralyze_monst(mon, rnd(25));
            }
            break;
        case POT_SPEED:
            angermon = FALSE;
            mon_adjust_speed(mon, 1, obj);
            break;
        case POT_BLINDNESS:
            if (haseyes(mon->data)) {
                int btmp = 64 + rn2(32)
                            + rn2(32) * !resist(mon, POTION_CLASS, 0, NOTELL);

                btmp += mon->mblinded;
                mon->mblinded = min(btmp, 127);
                mon->mcansee = 0;
            }
            break;
        case POT_WATER:
            if (is_undead(mon->data) || is_demon(mon->data)
                || is_were(mon->data) || is_vampshifter(mon)) {
                if (obj->blessed) {
#if 0 /*JP:T*/
                    pline("%s %s in pain!", Monnam(mon),
                          is_silent(mon->data) ? "writhes" : "shrieks");
#else
                    pline("%s�϶���%s��", Monnam(mon),
                          is_silent(mon->data) ? "�˿Ȥ��������" : "�ζ������򤢤���");
#endif
                    if (!is_silent(mon->data))
                        wake_nearto(tx, ty, mon->data->mlevel * 10);
                    mon->mhp -= d(2, 6);
                    /* should only be by you */
                    if (DEADMONSTER(mon))
                        killed(mon);
                    else if (is_were(mon->data) && !is_human(mon->data))
                        new_were(mon); /* revert to human */
                } else if (obj->cursed) {
                    angermon = FALSE;
                    if (canseemon(mon))
/*JP
                        pline("%s looks healthier.", Monnam(mon));
*/
                        pline("%s�Ϥ�긵���ˤʤä��褦�˸����롥", Monnam(mon));
                    mon->mhp += d(2, 6);
                    if (mon->mhp > mon->mhpmax)
                        mon->mhp = mon->mhpmax;
                    if (is_were(mon->data) && is_human(mon->data)
                        && !Protection_from_shape_changers)
                        new_were(mon); /* transform into beast */
                }
            } else if (mon->data == &mons[PM_GREMLIN]) {
                angermon = FALSE;
                (void) split_mon(mon, (struct monst *) 0);
            } else if (mon->data == &mons[PM_IRON_GOLEM]) {
                if (canseemon(mon))
/*JP
                    pline("%s rusts.", Monnam(mon));
*/
                    pline("%s�ϻ��Ӥ���", Monnam(mon));
                mon->mhp -= d(1, 6);
                /* should only be by you */
                if (DEADMONSTER(mon))
                    killed(mon);
            }
            break;
        case POT_OIL:
            if (obj->lamplit)
                explode_oil(obj, tx, ty);
            break;
        case POT_ACID:
            if (!resists_acid(mon) && !resist(mon, POTION_CLASS, 0, NOTELL)) {
#if 0 /*JP:T*/
                pline("%s %s in pain!", Monnam(mon),
                      is_silent(mon->data) ? "writhes" : "shrieks");
#else
                pline("%s�϶���%s��", Monnam(mon),
                      is_silent(mon->data) ? "�˿Ȥ��������" : "�ζ������򤢤���");
#endif
                if (!is_silent(mon->data))
                    wake_nearto(tx, ty, mon->data->mlevel * 10);
                mon->mhp -= d(obj->cursed ? 2 : 1, obj->blessed ? 4 : 8);
                if (DEADMONSTER(mon)) {
                    if (your_fault)
                        killed(mon);
                    else
                        monkilled(mon, "", AD_ACID);
                }
            }
            break;
        case POT_POLYMORPH:
            (void) bhitm(mon, obj);
            break;
        /*
        case POT_GAIN_LEVEL:
        case POT_LEVITATION:
        case POT_FRUIT_JUICE:
        case POT_MONSTER_DETECTION:
        case POT_OBJECT_DETECTION:
            break;
        */
        }
        /* target might have been killed */
        if (!DEADMONSTER(mon)) {
            if (angermon)
                wakeup(mon, TRUE);
            else
                mon->msleeping = 0;
        }
    }

    /* Note: potionbreathe() does its own docall() */
    if ((distance == 0 || (distance < 3 && rn2(5)))
        && (!breathless(youmonst.data) || haseyes(youmonst.data)))
        potionbreathe(obj);
    else if (obj->dknown && !objects[obj->otyp].oc_name_known
             && !objects[obj->otyp].oc_uname && cansee(tx, ty))
        docall(obj);

    if (*u.ushops && obj->unpaid) {
        struct monst *shkp = shop_keeper(*in_rooms(u.ux, u.uy, SHOPBASE));

        /* neither of the first two cases should be able to happen;
           only the hero should ever have an unpaid item, and only
           when inside a tended shop */
        if (!shkp) /* if shkp was killed, unpaid ought to cleared already */
            obj->unpaid = 0;
        else if (context.mon_moving) /* obj thrown by monster */
            subfrombill(obj, shkp);
        else /* obj thrown by hero */
            (void) stolen_value(obj, u.ux, u.uy, (boolean) shkp->mpeaceful,
                                FALSE);
    }
    obfree(obj, (struct obj *) 0);
}

/* vapors are inhaled or get in your eyes */
void
potionbreathe(obj)
register struct obj *obj;
{
    int i, ii, isdone, kn = 0;
    boolean cureblind = FALSE;

    /* potion of unholy water might be wielded; prevent
       you_were() -> drop_weapon() from dropping it so that it
       remains in inventory where our caller expects it to be */
    obj->in_use = 1;

    switch (obj->otyp) {
    case POT_RESTORE_ABILITY:
    case POT_GAIN_ABILITY:
        if (obj->cursed) {
            if (!breathless(youmonst.data))
/*JP
                pline("Ulch!  That potion smells terrible!");
*/
                pline("�����������Ϥ�Τ��������������롪");
            else if (haseyes(youmonst.data)) {
#if 0 /*JP:T*/
                const char *eyes = body_part(EYE);

                if (eyecount(youmonst.data) != 1)
                    eyes = makeplural(eyes);
                Your("%s %s!", eyes, vtense(eyes, "sting"));
#else
                Your("%s�������������롪", body_part(EYE));
#endif
            }
            break;
        } else {
            i = rn2(A_MAX); /* start at a random point */
            for (isdone = ii = 0; !isdone && ii < A_MAX; ii++) {
                if (ABASE(i) < AMAX(i)) {
                    ABASE(i)++;
                    /* only first found if not blessed */
                    isdone = !(obj->blessed);
                    context.botl = 1;
                }
                if (++i >= A_MAX)
                    i = 0;
            }
        }
        break;
    case POT_FULL_HEALING:
        if (Upolyd && u.mh < u.mhmax)
            u.mh++, context.botl = 1;
        if (u.uhp < u.uhpmax)
            u.uhp++, context.botl = 1;
        cureblind = TRUE;
        /*FALLTHRU*/
    case POT_EXTRA_HEALING:
        if (Upolyd && u.mh < u.mhmax)
            u.mh++, context.botl = 1;
        if (u.uhp < u.uhpmax)
            u.uhp++, context.botl = 1;
        if (!obj->cursed)
            cureblind = TRUE;
        /*FALLTHRU*/
    case POT_HEALING:
        if (Upolyd && u.mh < u.mhmax)
            u.mh++, context.botl = 1;
        if (u.uhp < u.uhpmax)
            u.uhp++, context.botl = 1;
        if (obj->blessed)
            cureblind = TRUE;
        if (cureblind) {
            make_blinded(0L, !u.ucreamed);
            make_deaf(0L, TRUE);
        }
        exercise(A_CON, TRUE);
        break;
    case POT_SICKNESS:
        if (!Role_if(PM_HEALER)) {
            if (Upolyd) {
                if (u.mh <= 5)
                    u.mh = 1;
                else
                    u.mh -= 5;
            } else {
                if (u.uhp <= 5)
                    u.uhp = 1;
                else
                    u.uhp -= 5;
            }
            context.botl = 1;
            exercise(A_CON, FALSE);
        }
        break;
    case POT_HALLUCINATION:
/*JP
        You("have a momentary vision.");
*/
        You("��ָ��ƤˤĤĤޤ줿��");
        break;
    case POT_CONFUSION:
    case POT_BOOZE:
        if (!Confusion)
/*JP
            You_feel("somewhat dizzy.");
*/
            You("��ޤ��򴶤�����");
        make_confused(itimeout_incr(HConfusion, rnd(5)), FALSE);
        break;
    case POT_INVISIBILITY:
        if (!Blind && !Invis) {
            kn++;
#if 0 /*JP:T*/
            pline("For an instant you %s!",
                  See_invisible ? "could see right through yourself"
                                : "couldn't see yourself");
#else
            pline("��ּ�ʬ���Ȥ�%s�����ʤ��ʤä���",
                  See_invisible ? "������"
                                : "");
#endif
        }
        break;
    case POT_PARALYSIS:
        kn++;
        if (!Free_action) {
/*JP
            pline("%s seems to be holding you.", Something);
*/
            pline("%s�����ʤ���Ĥ��ޤ��Ƥ���褦�ʵ���������", Something);
            nomul(-rnd(5));
/*JP
            multi_reason = "frozen by a potion";
*/
            multi_reason = "���ǹ�ľ���Ƥ������";
            nomovemsg = You_can_move_again;
            exercise(A_DEX, FALSE);
        } else
/*JP
            You("stiffen momentarily.");
*/
            You("��ֹ�ľ������");
        break;
    case POT_SLEEPING:
        kn++;
        if (!Free_action && !Sleep_resistance) {
/*JP
            You_feel("rather tired.");
*/
            You("��������줿��");
            nomul(-rnd(5));
/*JP
            multi_reason = "sleeping off a magical draught";
*/
            multi_reason = "��ˡŪ��̲�äƤ���֤�";
            nomovemsg = You_can_move_again;
            exercise(A_DEX, FALSE);
        } else
/*JP
            You("yawn.");
*/
            You("�����Ӥ򤷤���");
        break;
    case POT_SPEED:
        if (!Fast)
/*JP
            Your("knees seem more flexible now.");
*/
            Your("ɨ�Ϥ�ꤹ�Ф䤯ư���褦�ˤʤä���");
        incr_itimeout(&HFast, rnd(5));
        exercise(A_DEX, TRUE);
        break;
    case POT_BLINDNESS:
        if (!Blind && !Unaware) {
            kn++;
/*JP
            pline("It suddenly gets dark.");
*/
            pline("�����Ť��ʤä���");
        }
        make_blinded(itimeout_incr(Blinded, rnd(5)), FALSE);
        if (!Blind && !Unaware)
            Your1(vision_clears);
        break;
    case POT_WATER:
        if (u.umonnum == PM_GREMLIN) {
            (void) split_mon(&youmonst, (struct monst *) 0);
        } else if (u.ulycn >= LOW_PM) {
            /* vapor from [un]holy water will trigger
               transformation but won't cure lycanthropy */
            if (obj->blessed && youmonst.data == &mons[u.ulycn])
                you_unwere(FALSE);
            else if (obj->cursed && !Upolyd)
                you_were();
        }
        break;
    case POT_ACID:
    case POT_POLYMORPH:
        exercise(A_CON, FALSE);
        break;
    /*
    case POT_GAIN_LEVEL:
    case POT_LEVITATION:
    case POT_FRUIT_JUICE:
    case POT_MONSTER_DETECTION:
    case POT_OBJECT_DETECTION:
    case POT_OIL:
        break;
     */
    }
    /* note: no obfree() -- that's our caller's responsibility */
    if (obj->dknown) {
        if (kn)
            makeknown(obj->otyp);
        else if (!objects[obj->otyp].oc_name_known
                 && !objects[obj->otyp].oc_uname)
            docall(obj);
    }
}

/* returns the potion type when o1 is dipped in o2 */
STATIC_OVL short
mixtype(o1, o2)
register struct obj *o1, *o2;
{
    int o1typ = o1->otyp, o2typ = o2->otyp;

    /* cut down on the number of cases below */
    if (o1->oclass == POTION_CLASS
        && (o2typ == POT_GAIN_LEVEL || o2typ == POT_GAIN_ENERGY
            || o2typ == POT_HEALING || o2typ == POT_EXTRA_HEALING
            || o2typ == POT_FULL_HEALING || o2typ == POT_ENLIGHTENMENT
            || o2typ == POT_FRUIT_JUICE)) {
        /* swap o1 and o2 */
        o1typ = o2->otyp;
        o2typ = o1->otyp;
    }

    switch (o1typ) {
    case POT_HEALING:
        if (o2typ == POT_SPEED)
            return POT_EXTRA_HEALING;
        /*FALLTHRU*/
    case POT_EXTRA_HEALING:
    case POT_FULL_HEALING:
        if (o2typ == POT_GAIN_LEVEL || o2typ == POT_GAIN_ENERGY)
            return (o1typ == POT_HEALING) ? POT_EXTRA_HEALING
                   : (o1typ == POT_EXTRA_HEALING) ? POT_FULL_HEALING
                     : POT_GAIN_ABILITY;
        /*FALLTHRU*/
    case UNICORN_HORN:
        switch (o2typ) {
        case POT_SICKNESS:
            return POT_FRUIT_JUICE;
        case POT_HALLUCINATION:
        case POT_BLINDNESS:
        case POT_CONFUSION:
            return POT_WATER;
        }
        break;
    case AMETHYST: /* "a-methyst" == "not intoxicated" */
        if (o2typ == POT_BOOZE)
            return POT_FRUIT_JUICE;
        break;
    case POT_GAIN_LEVEL:
    case POT_GAIN_ENERGY:
        switch (o2typ) {
        case POT_CONFUSION:
            return (rn2(3) ? POT_BOOZE : POT_ENLIGHTENMENT);
        case POT_HEALING:
            return POT_EXTRA_HEALING;
        case POT_EXTRA_HEALING:
            return POT_FULL_HEALING;
        case POT_FULL_HEALING:
            return POT_GAIN_ABILITY;
        case POT_FRUIT_JUICE:
            return POT_SEE_INVISIBLE;
        case POT_BOOZE:
            return POT_HALLUCINATION;
        }
        break;
    case POT_FRUIT_JUICE:
        switch (o2typ) {
        case POT_SICKNESS:
            return POT_SICKNESS;
        case POT_ENLIGHTENMENT:
        case POT_SPEED:
            return POT_BOOZE;
        case POT_GAIN_LEVEL:
        case POT_GAIN_ENERGY:
            return POT_SEE_INVISIBLE;
        }
        break;
    case POT_ENLIGHTENMENT:
        switch (o2typ) {
        case POT_LEVITATION:
            if (rn2(3))
                return POT_GAIN_LEVEL;
            break;
        case POT_FRUIT_JUICE:
            return POT_BOOZE;
        case POT_BOOZE:
            return POT_CONFUSION;
        }
        break;
    }

    return STRANGE_OBJECT;
}

/* #dip command */
int
dodip()
{
/*JP
    static const char Dip_[] = "Dip ";
*/
    static const char Dip_[] = "����";
    register struct obj *potion, *obj;
    struct obj *singlepotion;
    uchar here;
    char allowall[2];
    short mixture;
    char qbuf[QBUFSZ], obuf[QBUFSZ];
#if 0 /*JP*/
    const char *shortestname; /* last resort obj name for prompt */
#endif

    allowall[0] = ALL_CLASSES;
    allowall[1] = '\0';
    if (!(obj = getobj(allowall, "dip")))
        return 0;
/*JP
    if (inaccessible_equipment(obj, "dip", FALSE))
*/
    if (inaccessible_equipment(obj, "�򿻤�", FALSE))
        return 0;

#if 0 /*JP*/
    shortestname = (is_plural(obj) || pair_of(obj)) ? "them" : "it";
#endif
    /*
     * Bypass safe_qbuf() since it doesn't handle varying suffix without
     * an awful lot of support work.  Format the object once, even though
     * the fountain and pool prompts offer a lot more room for it.
     * 3.6.0 used thesimpleoname() unconditionally, which posed no risk
     * of buffer overflow but drew bug reports because it omits user-
     * supplied type name.
     * getobj: "What do you want to dip <the object> into? [xyz or ?*] "
     */
    Strcpy(obuf, short_oname(obj, doname, thesimpleoname,
                             /* 128 - (24 + 54 + 1) leaves 49 for <object> */
                             QBUFSZ - sizeof "What do you want to dip \
 into? [abdeghjkmnpqstvwyzBCEFHIKLNOQRTUWXZ#-# or ?*] "));

    here = levl[u.ux][u.uy].typ;
    /* Is there a fountain to dip into here? */
    if (IS_FOUNTAIN(here)) {
#if 0 /*JP*/
        Sprintf(qbuf, "%s%s into the fountain?", Dip_,
                flags.verbose ? obuf : shortestname);
#else
        Sprintf(qbuf, "����%s��", Dip_);
#endif
        /* "Dip <the object> into the fountain?" */
        if (yn(qbuf) == 'y') {
            dipfountain(obj);
            return 1;
        }
    } else if (is_pool(u.ux, u.uy)) {
        const char *pooltype = waterbody_name(u.ux, u.uy);

#if 0 /*JP*/
        Sprintf(qbuf, "%s%s into the %s?", Dip_,
                flags.verbose ? obuf : shortestname, pooltype);
#else /*JP:�Ѹ�Ǥϲ��򿻤�����ޤ�Ƥ��뤬���ܸ�ǤϽ������Թ�ǤȤꤢ������ά*/
        Sprintf(qbuf, "%s��%s��", pooltype, Dip_);
#endif
        /* "Dip <the object> into the {pool, moat, &c}?" */
        if (yn(qbuf) == 'y') {
            if (Levitation) {
                floating_above(pooltype);
            } else if (u.usteed && !is_swimmer(u.usteed->data)
                       && P_SKILL(P_RIDING) < P_BASIC) {
                rider_cant_reach(); /* not skilled enough to reach */
            } else {
                if (obj->otyp == POT_ACID)
                    obj->in_use = 1;
                if (water_damage(obj, 0, TRUE) != ER_DESTROYED && obj->in_use)
                    useup(obj);
            }
            return 1;
        }
    }

#if 0 /*JP*/
    /* "What do you want to dip <the object> into? [xyz or ?*] " */
    Sprintf(qbuf, "dip %s into", flags.verbose ? obuf : shortestname);
#else
    /* "What do you want to dip into? [xyz or ?*] " */
    Sprintf(qbuf, "dip into");
#endif
    potion = getobj(beverages, qbuf);
    if (!potion)
        return 0;
    if (potion == obj && potion->quan == 1L) {
/*JP
        pline("That is a potion bottle, not a Klein bottle!");
*/
        pline("��������Ӥ������饤����ۤ���ʤ���");
        return 0;
    }
    potion->in_use = TRUE; /* assume it will be used up */
    if (potion->otyp == POT_WATER) {
        boolean useeit = !Blind || (obj == ublindf && Blindfolded_only);
#if 0 /*JP*/
        const char *obj_glows = Yobjnam2(obj, "glow");
#else
        const char *obj_glows = cxname(obj);
#endif

        if (H2Opotion_dip(potion, obj, useeit, obj_glows))
            goto poof;
    } else if (obj->otyp == POT_POLYMORPH || potion->otyp == POT_POLYMORPH) {
        /* some objects can't be polymorphed */
        if (obj->otyp == potion->otyp /* both POT_POLY */
            || obj->otyp == WAN_POLYMORPH || obj->otyp == SPE_POLYMORPH
            || obj == uball || obj == uskin
            || obj_resists(obj->otyp == POT_POLYMORPH ? potion : obj,
                           5, 95)) {
            pline1(nothing_happens);
        } else {
            short save_otyp = obj->otyp;

            /* KMH, conduct */
            u.uconduct.polypiles++;

            obj = poly_obj(obj, STRANGE_OBJECT);

            /*
             * obj might be gone:
             *  poly_obj() -> set_wear() -> Amulet_on() -> useup()
             * if obj->otyp is worn amulet and becomes AMULET_OF_CHANGE.
             */
            if (!obj) {
                makeknown(POT_POLYMORPH);
                return 1;
            } else if (obj->otyp != save_otyp) {
                makeknown(POT_POLYMORPH);
                useup(potion);
                prinv((char *) 0, obj, 0L);
                return 1;
            } else {
/*JP
                pline("Nothing seems to happen.");
*/
                pline("���ⵯ����ʤ��ä��褦����");
                goto poof;
            }
        }
        potion->in_use = FALSE; /* didn't go poof */
        return 1;
    } else if (obj->oclass == POTION_CLASS && obj->otyp != potion->otyp) {
        int amt = (int) obj->quan;
        boolean magic;

        mixture = mixtype(obj, potion);

        magic = (mixture != STRANGE_OBJECT) ? objects[mixture].oc_magic
            : (objects[obj->otyp].oc_magic || objects[potion->otyp].oc_magic);
#if 0 /*JP*/
        Strcpy(qbuf, "The"); /* assume full stack */
#else
        Strcpy(qbuf, "");
#endif
        if (amt > (magic ? 3 : 7)) {
            /* trying to dip multiple potions will usually affect only a
               subset; pick an amount between 3 and 8, inclusive, for magic
               potion result, between 7 and N for non-magic */
            if (magic)
                amt = rnd(min(amt, 8) - (3 - 1)) + (3 - 1); /* 1..6 + 2 */
            else
                amt = rnd(amt - (7 - 1)) + (7 - 1); /* 1..(N-6) + 6 */

            if ((long) amt < obj->quan) {
                obj = splitobj(obj, (long) amt);
/*JP
                Sprintf(qbuf, "%ld of the", obj->quan);
*/
                Sprintf(qbuf, "%ld�ܤ�", obj->quan);
            }
        }
        /* [N of] the {obj(s)} mix(es) with [one of] {the potion}... */
#if 0 /*JP*/
        pline("%s %s %s with %s%s...", qbuf, simpleonames(obj),
              otense(obj, "mix"), (potion->quan > 1L) ? "one of " : "",
              thesimpleoname(potion));
#else /* [N�ܤ�]{obj}��{the potion}[�ΰ��]�Ⱥ����������� */
        pline("%s%s��%s%s�Ⱥ�����������", qbuf, simpleonames(obj),
              thesimpleoname(potion),
              (potion->quan > 1L) ? "�ΰ��" : "");
#endif
        /* get rid of 'dippee' before potential perm_invent updates */
        useup(potion); /* now gone */
        /* Mixing potions is dangerous...
           KMH, balance patch -- acid is particularly unstable */
        if (obj->cursed || obj->otyp == POT_ACID || !rn2(10)) {
            /* it would be better to use up the whole stack in advance
               of the message, but we can't because we need to keep it
               around for potionbreathe() [and we can't set obj->in_use
               to 'amt' because that's not implemented] */
            obj->in_use = 1;
/*JP
            pline("%sThey explode!", !Deaf ? "BOOM!  " : "");
*/
            pline("%s��ȯ������", !Deaf ? "�С���" : "");
            wake_nearto(u.ux, u.uy, (BOLT_LIM + 1) * (BOLT_LIM + 1));
            exercise(A_STR, FALSE);
            if (!breathless(youmonst.data) || haseyes(youmonst.data))
                potionbreathe(obj);
            useupall(obj);
            losehp(amt + rnd(9), /* not physical damage */
/*JP
                   "alchemic blast", KILLED_BY_AN);
*/
                   "Ĵ��μ��Ԥ�", KILLED_BY_AN);
            return 1;
        }

        obj->blessed = obj->cursed = obj->bknown = 0;
        if (Blind || Hallucination)
            obj->dknown = 0;

        if (mixture != STRANGE_OBJECT) {
            obj->otyp = mixture;
        } else {
            struct obj *otmp;

            switch (obj->odiluted ? 1 : rnd(8)) {
            case 1:
                obj->otyp = POT_WATER;
                break;
            case 2:
            case 3:
                obj->otyp = POT_SICKNESS;
                break;
            case 4:
                otmp = mkobj(POTION_CLASS, FALSE);
                obj->otyp = otmp->otyp;
                obfree(otmp, (struct obj *) 0);
                break;
            default:
                useupall(obj);
#if 0 /*JP:T*/
                pline_The("mixture %sevaporates.",
                          !Blind ? "glows brightly and " : "");
#else
                pline_The("�����������%s��ȯ������",
                          !Blind ? "���뤯������" : "");
#endif
                return 1;
            }
        }
        obj->odiluted = (obj->otyp != POT_WATER);

        if (obj->otyp == POT_WATER && !Hallucination) {
/*JP
            pline_The("mixture bubbles%s.", Blind ? "" : ", then clears");
*/
            pline("���򺮤����%sˢ���ä���", Blind ? "" : "���Ф餯");
        } else if (!Blind) {
/*JP
            pline_The("mixture looks %s.",
*/
            pline("����������%s���˸����롥",
                      hcolor(OBJ_DESCR(objects[obj->otyp])));
        }

        /* this is required when 'obj' was split off from a bigger stack,
           so that 'obj' will now be assigned its own inventory slot;
           it has a side-effect of merging 'obj' into another compatible
           stack if there is one, so we do it even when no split has
           been made in order to get the merge result for both cases;
           as a consequence, mixing while Fumbling drops the mixture */
        freeinv(obj);
#if 0 /*JP:T*/
        (void) hold_another_object(obj, "You drop %s!", doname(obj),
                                   (const char *) 0);
#else
        (void) hold_another_object(obj, "%s�������", doname(obj),
                                   (const char *) 0);
#endif
        return 1;
    }

    if (potion->otyp == POT_ACID && obj->otyp == CORPSE
        && obj->corpsenm == PM_LICHEN) {
#if 0 /*JP:T*/
        pline("%s %s %s around the edges.", The(cxname(obj)),
              otense(obj, "turn"), Blind ? "wrinkled"
                                   : potion->odiluted ? hcolor(NH_ORANGE)
                                     : hcolor(NH_RED));
#else
        pline("%s�Ϥդ���%s�ʤä���", The(cxname(obj)),
              Blind ? "���路���"
              : potion->odiluted ? hcolor_adv(NH_ORANGE)
                        : hcolor_adv(NH_RED));
#endif
        potion->in_use = FALSE; /* didn't go poof */
        if (potion->dknown
            && !objects[potion->otyp].oc_name_known
            && !objects[potion->otyp].oc_uname)
            docall(potion);
        return 1;
    }

    if (potion->otyp == POT_WATER && obj->otyp == TOWEL) {
/*JP
        pline_The("towel soaks it up!");
*/
        pline_The("������Ͽ��ۤ��������");
        /* wetting towel already done via water_damage() in H2Opotion_dip */
        goto poof;
    }

    if (is_poisonable(obj)) {
        if (potion->otyp == POT_SICKNESS && !obj->opoisoned) {
            char buf[BUFSZ];

            if (potion->quan > 1L)
/*JP
                Sprintf(buf, "One of %s", the(xname(potion)));
*/
                Sprintf(buf, "%s�ΰ��", the(xname(potion)));
            else
                Strcpy(buf, The(xname(potion)));
/*JP
            pline("%s forms a coating on %s.", buf, the(xname(obj)));
*/
            pline("%s��%s���ɤ�줿��", buf, the(xname(obj)));
            obj->opoisoned = TRUE;
            goto poof;
        } else if (obj->opoisoned && (potion->otyp == POT_HEALING
                                      || potion->otyp == POT_EXTRA_HEALING
                                      || potion->otyp == POT_FULL_HEALING)) {
/*JP
            pline("A coating wears off %s.", the(xname(obj)));
*/
            pline("�Ǥ�%s��������������", the(xname(obj)));
            obj->opoisoned = 0;
            goto poof;
        }
    }

    if (potion->otyp == POT_ACID) {
        if (erode_obj(obj, 0, ERODE_CORRODE, EF_GREASE) != ER_NOTHING)
            goto poof;
    }

    if (potion->otyp == POT_OIL) {
        boolean wisx = FALSE;

        if (potion->lamplit) { /* burning */
            fire_damage(obj, TRUE, u.ux, u.uy);
        } else if (potion->cursed) {
/*JP
            pline_The("potion spills and covers your %s with oil.",
*/
            pline("�������ӻ��ꤢ�ʤ���%s�ˤ����ä���",
                      fingers_or_gloves(TRUE));
            make_glib((int) (Glib & TIMEOUT) + d(2, 10));
        } else if (obj->oclass != WEAPON_CLASS && !is_weptool(obj)) {
            /* the following cases apply only to weapons */
            goto more_dips;
            /* Oil removes rust and corrosion, but doesn't unburn.
             * Arrows, etc are classed as metallic due to arrowhead
             * material, but dipping in oil shouldn't repair them.
             */
        } else if ((!is_rustprone(obj) && !is_corrodeable(obj))
                   || is_ammo(obj) || (!obj->oeroded && !obj->oeroded2)) {
            /* uses up potion, doesn't set obj->greased */
            if (!Blind)
#if 0 /*JP:T*/
                pline("%s %s with an oily sheen.", Yname2(obj),
                      otense(obj, "gleam"));
#else
                pline("%s�����θ����Ǥ����ȸ��ä���", Yname2(obj));
#endif
            else /*if (!uarmg)*/
/*JP
                pline("%s %s oily.", Yname2(obj), otense(obj, "feel"));
*/
                pline("%s�����äݤ���", Yname2(obj));
        } else {
#if 0 /*JP:T*/
            pline("%s %s less %s.", Yname2(obj),
                  otense(obj, !Blind ? "are" : "feel"),
                  (obj->oeroded && obj->oeroded2)
                      ? "corroded and rusty"
                      : obj->oeroded ? "rusty" : "corroded");
#else
            pline("%s��%s����줿%s��", Yname2(obj),
                  (obj->oeroded && obj->oeroded2)
                      ? "�忩�Ȼ�"
                      : obj->oeroded ? "��" : "�忩",
                  !Blind ? "" : "�褦��");
#endif
            if (obj->oeroded > 0)
                obj->oeroded--;
            if (obj->oeroded2 > 0)
                obj->oeroded2--;
            wisx = TRUE;
        }
        exercise(A_WIS, wisx);
        if (potion->dknown)
            makeknown(potion->otyp);
        useup(potion);
        return 1;
    }
 more_dips:

    /* Allow filling of MAGIC_LAMPs to prevent identification by player */
    if ((obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP)
        && (potion->otyp == POT_OIL)) {
        /* Turn off engine before fueling, turn off fuel too :-)  */
        if (obj->lamplit || potion->lamplit) {
            useup(potion);
            explode(u.ux, u.uy, 11, d(6, 6), 0, EXPL_FIERY);
            exercise(A_WIS, FALSE);
            return 1;
        }
        /* Adding oil to an empty magic lamp renders it into an oil lamp */
        if ((obj->otyp == MAGIC_LAMP) && obj->spe == 0) {
            obj->otyp = OIL_LAMP;
            obj->age = 0;
        }
        if (obj->age > 1000L) {
/*JP
            pline("%s %s full.", Yname2(obj), otense(obj, "are"));
*/
            pline("%s�ˤϤ��äפ����äƤ��롥", Yname2(obj));
            potion->in_use = FALSE; /* didn't go poof */
        } else {
/*JP
            You("fill %s with oil.", yname(obj));
*/
            You("%s���������줿��", yname(obj));
            check_unpaid(potion);        /* Yendorian Fuel Tax */
            /* burns more efficiently in a lamp than in a bottle;
               diluted potion provides less benefit but we don't attempt
               to track that the lamp now also has some non-oil in it */
            obj->age += (!potion->odiluted ? 4L : 3L) * potion->age / 2L;
            if (obj->age > 1500L)
                obj->age = 1500L;
            useup(potion);
            exercise(A_WIS, TRUE);
        }
        if (potion->dknown)
            makeknown(POT_OIL);
        obj->spe = 1;
        update_inventory();
        return 1;
    }

    potion->in_use = FALSE; /* didn't go poof */
    if ((obj->otyp == UNICORN_HORN || obj->otyp == AMETHYST)
        && (mixture = mixtype(obj, potion)) != STRANGE_OBJECT) {
        char oldbuf[BUFSZ], newbuf[BUFSZ];
        short old_otyp = potion->otyp;
        boolean old_dknown = FALSE;
        boolean more_than_one = potion->quan > 1L;

        oldbuf[0] = '\0';
        if (potion->dknown) {
            old_dknown = TRUE;
/*JP
            Sprintf(oldbuf, "%s ", hcolor(OBJ_DESCR(objects[potion->otyp])));
*/
            Sprintf(oldbuf, "%s", hcolor(OBJ_DESCR(objects[potion->otyp])));
        }
        /* with multiple merged potions, split off one and
           just clear it */
        if (potion->quan > 1L) {
            singlepotion = splitobj(potion, 1L);
        } else
            singlepotion = potion;

        costly_alteration(singlepotion, COST_NUTRLZ);
        singlepotion->otyp = mixture;
        singlepotion->blessed = 0;
        if (mixture == POT_WATER)
            singlepotion->cursed = singlepotion->odiluted = 0;
        else
            singlepotion->cursed = obj->cursed; /* odiluted left as-is */
        singlepotion->bknown = FALSE;
        if (Blind) {
            singlepotion->dknown = FALSE;
        } else {
            singlepotion->dknown = !Hallucination;
            *newbuf = '\0';
            if (mixture == POT_WATER && singlepotion->dknown)
/*JP
                Sprintf(newbuf, "clears");
*/
                Sprintf(newbuf, "Ʃ��");
            else if (!Blind)
#if 0 /*JP*/
                Sprintf(newbuf, "turns %s",
                        hcolor(OBJ_DESCR(objects[mixture])));
#else
                Sprintf(newbuf, "%s��",
                        hcolor(OBJ_DESCR(objects[mixture])));
#endif
            if (*newbuf)
#if 0 /*JP:T*/
                pline_The("%spotion%s %s.", oldbuf,
                          more_than_one ? " that you dipped into" : "",
                          newbuf);
#else
                pline_The("%s%s����%s�ˤʤä���.",
                          more_than_one ? "������" : "",
                          oldbuf, newbuf);
#endif
            else
/*JP
                pline("Something happens.");
*/
                pline("��������������");

            if (old_dknown
                && !objects[old_otyp].oc_name_known
                && !objects[old_otyp].oc_uname) {
                struct obj fakeobj;

                fakeobj = zeroobj;
                fakeobj.dknown = 1;
                fakeobj.otyp = old_otyp;
                fakeobj.oclass = POTION_CLASS;
                docall(&fakeobj);
            }
        }
        obj_extract_self(singlepotion);
        singlepotion = hold_another_object(singlepotion,
/*JP
                                           "You juggle and drop %s!",
*/
                                           "����̤���%s����Ȥ��Ƥ��ޤä���",
                                           doname(singlepotion),
                                           (const char *) 0);
        nhUse(singlepotion);
        update_inventory();
        return 1;
    }

/*JP
    pline("Interesting...");
*/
    pline("���򤤡�����");
    return 1;

 poof:
    if (potion->dknown
        && !objects[potion->otyp].oc_name_known
        && !objects[potion->otyp].oc_uname)
        docall(potion);
    useup(potion);
    return 1;
}

/* *monp grants a wish and then leaves the game */
void
mongrantswish(monp)
struct monst **monp;
{
    struct monst *mon = *monp;
    int mx = mon->mx, my = mon->my, glyph = glyph_at(mx, my);

    /* remove the monster first in case wish proves to be fatal
       (blasted by artifact), to keep it out of resulting bones file */
    mongone(mon);
    *monp = 0; /* inform caller that monster is gone */
    /* hide that removal from player--map is visible during wish prompt */
    tmp_at(DISP_ALWAYS, glyph);
    tmp_at(mx, my);
    /* grant the wish */
    makewish();
    /* clean up */
    tmp_at(DISP_END, 0);
}

void
djinni_from_bottle(obj)
struct obj *obj;
{
    struct monst *mtmp;
    int chance;

    if (!(mtmp = makemon(&mons[PM_DJINNI], u.ux, u.uy, NO_MM_FLAGS))) {
#if 0 /*JP*/
        pline("It turns out to be empty.");
#else
        if (obj->otyp == MAGIC_LAMP) {
            pline("���פ϶��äݤ��ä���");
        } else {
            pline("���϶��äݤ��ä���");
        }
#endif
        return;
    }

    if (!Blind) {
/*JP
        pline("In a cloud of smoke, %s emerges!", a_monnam(mtmp));
*/
        pline("����椫�顤%s������줿��", a_monnam(mtmp));
/*JP
        pline("%s speaks.", Monnam(mtmp));
*/
        pline("%s���ä���������", Monnam(mtmp));
    } else {
/*JP
        You("smell acrid fumes.");
*/
        You("�ĥ�Ȥ���������������");
/*JP
        pline("%s speaks.", Something);
*/
        pline("%s���ä���������", Something);
    }

    chance = rn2(5);
    if (obj->blessed)
        chance = (chance == 4) ? rnd(4) : 0;
    else if (obj->cursed)
        chance = (chance == 0) ? rn2(4) : 4;
    /* 0,1,2,3,4:  b=80%,5,5,5,5; nc=20%,20,20,20,20; c=5%,5,5,5,80 */

    switch (chance) {
    case 0:
/*JP
        verbalize("I am in your debt.  I will grant one wish!");
*/
        verbalize("�����ˤϼڤ꤬�Ǥ�������Ĵꤤ�򤫤ʤ��Ƥ����");
        /* give a wish and discard the monster (mtmp set to null) */
        mongrantswish(&mtmp);
        break;
    case 1:
/*JP
        verbalize("Thank you for freeing me!");
*/
        verbalize("�������Ƥ��줿���Ȥ򴶼դ��롪");
        (void) tamedog(mtmp, (struct obj *) 0);
        break;
    case 2:
/*JP
        verbalize("You freed me!");
*/
        verbalize("�������Ƥ��줿�ΤϤ�������");
        mtmp->mpeaceful = TRUE;
        set_malign(mtmp);
        break;
    case 3:
/*JP
        verbalize("It is about time!");
*/
        verbalize("����Ф���");
        if (canspotmon(mtmp))
/*JP
            pline("%s vanishes.", Monnam(mtmp));
*/
            pline("%s�Ͼä�����", Monnam(mtmp));
        mongone(mtmp);
        break;
    default:
/*JP
        verbalize("You disturbed me, fool!");
*/
        verbalize("���ޤ��ϻ��̲���˸������������Τᡪ");
        mtmp->mpeaceful = FALSE;
        set_malign(mtmp);
        break;
    }
}

/* clone a gremlin or mold (2nd arg non-null implies heat as the trigger);
   hit points are cut in half (odd HP stays with original) */
struct monst *
split_mon(mon, mtmp)
struct monst *mon,  /* monster being split */
             *mtmp; /* optional attacker whose heat triggered it */
{
    struct monst *mtmp2;
    char reason[BUFSZ];

    reason[0] = '\0';
    if (mtmp)
#if 0 /*JP:T*/
        Sprintf(reason, " from %s heat",
                (mtmp == &youmonst) ? the_your[1]
                                    : (const char *) s_suffix(mon_nam(mtmp)));
#else
        Sprintf(reason, "%s��Ǯ��",
                (mtmp == &youmonst) ? the_your[1]
                                    : (const char *) mon_nam(mtmp));
#endif

    if (mon == &youmonst) {
        mtmp2 = cloneu();
        if (mtmp2) {
            mtmp2->mhpmax = u.mhmax / 2;
            u.mhmax -= mtmp2->mhpmax;
            context.botl = 1;
/*JP
            You("multiply%s!", reason);
*/
            You("%sʬ��������", reason);
        }
    } else {
        mtmp2 = clone_mon(mon, 0, 0);
        if (mtmp2) {
            mtmp2->mhpmax = mon->mhpmax / 2;
            mon->mhpmax -= mtmp2->mhpmax;
            if (canspotmon(mon))
/*JP
                pline("%s multiplies%s!", Monnam(mon), reason);
*/
                pline("%s��%sʬ��������", Monnam(mon), reason);
        }
    }
    return mtmp2;
}

/*potion.c*/
