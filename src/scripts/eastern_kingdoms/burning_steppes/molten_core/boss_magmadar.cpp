/*
*  Copyright (C) 2017 - Elysium Project <http://elysium-project.org/>
*
*  Script rewritten by Zerix.
*
*  Notes: This is a proof of concept that writing scripts for SD2 can look, and feel better.
*      Event based timers not only look cleaner, but they are easier to read and write for new/inexperienced
*      developers.
*
*/

#include "scriptPCH.h"
#include "molten_core.h"


enum eDialogue
{
    EMOTE_GENERIC_FRENZY_KILL   = -1000001,
};

enum eSpells
{
    SpellFrenzy                = 19451,
    SpellMagmaSpit             = 19449,
    SpellPanic                 = 19408,
    SpellLavaBomb              = 19411,
    SpellLavaBombAlt           = 19428,
    SpellLavaBreath            = 19272,
    SpellMagmaSpitEffect       = 19450
};

enum eCreatures
{
    NpcLavaBombDummy                = 20006,
    InvisibleDummyModel             = 11686
};

enum eEvents
{
    EventFrenzy,
    EventPanic,
    EventLavaBomb,
    EventLavaBombTrigger,
    EventLavaBreath,
    EventMagmaSpit,
    EventRestoreTarget
};

/*
    DELETE FROM creature_template WHERE entry=20006 OR ScriptName="boss_magmadar_lavabomb";
    INSERT INTO creature_template
    SET entry=20006, modelid_1=11686, modelid_2=11686, name="LavaBomb", subname="Script", minhealth=4000, maxhealth=5000, faction_A=14,faction_H=14,
    speed_walk=0.01, speed_run=0.01, ScriptName="boss_magmadar_lavabomb";
*/
struct boss_magmadarAI : public ScriptedAI
{
    explicit boss_magmadarAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_Instance = static_cast<ScriptedInstance*>(pCreature->GetInstanceData());
        boss_magmadarAI::Reset();
    }

    void Reset() override
    {
        m_Events.ScheduleEvent(eEvents::EventFrenzy, Seconds(15));
        m_Events.ScheduleEvent(eEvents::EventPanic, Seconds(10));
        m_Events.ScheduleEvent(eEvents::EventLavaBomb, Seconds(13));
        m_Events.ScheduleEvent(eEvents::EventLavaBreath, Seconds(30));
        m_Events.ScheduleEvent(eEvents::EventMagmaSpit, Seconds(10));
        //m_Events.ScheduleEvent(eEvents::EventLavaBombTrigger, Seconds(0));
        //m_Events.ScheduleEvent(eEvents::EventRestoreTarget, Seconds(0));

        if (!m_creature->HasAura(eSpells::SpellMagmaSpit))
            m_creature->CastSpell(m_creature, eSpells::SpellMagmaSpit, true);

        if (m_Instance && m_creature->isAlive())
            m_Instance->SetData(TYPE_MAGMADAR, NOT_STARTED);
    }

    void Aggro(Unit* /*p_Who*/) override
    {
        if (m_Instance)
            m_Instance->SetData(TYPE_MAGMADAR, IN_PROGRESS);
    }

    void JustDied(Unit* /*p_Killer*/) override
    {
        if (m_Instance)
            m_Instance->SetData(TYPE_MAGMADAR, DONE);
    }

    void UpdateAI(const uint32 p_Diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            return;

        m_Events.Update(p_Diff);
        while (auto l_EventId = m_Events.ExecuteEvent())
        {
            switch (l_EventId)
            {
                case eEvents::EventFrenzy:
                {
                    if (DoCastSpellIfCan(m_creature, eSpells::SpellFrenzy) == CAST_OK)
                    {
                        DoScriptText(EMOTE_GENERIC_FRENZY_KILL, m_creature);
                        m_Events.Repeat(randtime(Seconds(15), Seconds(20)));
                    }
                    break;
                }
                case eEvents::EventPanic:
                {
                    if (DoCastSpellIfCan(m_creature->getVictim(), eSpells::SpellPanic) == CAST_OK)
                        m_Events.Repeat(randtime(Seconds(30), Seconds(35)));
                    break;
                }

                case eEvents::EventLavaBomb:
                {
                    if (auto l_RandomTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    {
                        if (DoCastSpellIfCan(l_RandomTarget, eSpells::SpellLavaBomb) == CAST_OK)
                        {
                            m_creature->SetInFront(l_RandomTarget);
                            m_creature->SetTargetGuid(l_RandomTarget->GetObjectGuid());
                            if (auto l_LavaBombDummy = m_creature->SummonCreature(eCreatures::NpcLavaBombDummy,
                                l_RandomTarget->GetPositionX(),
                                l_RandomTarget->GetPositionY(),
                                l_RandomTarget->GetPositionZ(),
                                l_RandomTarget->GetOrientation(),
                                TEMPSUMMON_TIMED_DESPAWN,
                                30000))
                            {
                                // Scale at 0 is necessary for spell animation
                                l_LavaBombDummy->SetObjectScale(0.0f);
                                l_LavaBombDummy->setFaction(m_creature->getFaction());
                            }

                            m_Events.Repeat(randtime(Seconds(10), Seconds(13)));
                            m_Events.ScheduleEvent(eEvents::EventLavaBombTrigger, Milliseconds(500));
                            m_Events.ScheduleEvent(eEvents::EventRestoreTarget, Milliseconds(800));
                        }
                    }
                    break;
                }
                case eEvents::EventLavaBombTrigger:
                {
                    std::list<Creature*> l_CreListe;
                    GetCreatureListWithEntryInGrid(l_CreListe, m_creature, eCreatures::NpcLavaBombDummy, 100.0f);
                    for (auto const &i_Creature : l_CreListe)
                        i_Creature->CastSpell(i_Creature, eSpells::SpellLavaBombAlt, false);

                    m_Events.Repeat(Milliseconds(1150));
                    break;
                }
                case eEvents::EventRestoreTarget:
                {
                    m_creature->SetInFront(m_creature->getVictim());
                    m_creature->SetTargetGuid(m_creature->getVictim()->GetObjectGuid());

                    // Zerix: Not too sure here.
                    m_Events.CancelEvent(eEvents::EventRestoreTarget);
                }
                case eEvents::EventMagmaSpit:
                {
                    if (DoCastSpellIfCan(m_creature->getVictim(), eSpells::SpellMagmaSpitEffect) == CAST_OK)
                        m_Events.Repeat(randtime(Seconds(10), Seconds(20)));
                    break;
                }
                case eEvents::EventLavaBreath:
                {
                    if (DoCastSpellIfCan(m_creature, eSpells::SpellLavaBreath) == CAST_OK)
                        m_Events.Repeat(randtime(Seconds(10), Seconds(30)));
                    break;
                }
                default: break;
            }
        }

        DoMeleeAttackIfReady();
    }

    void UpdateAI_corpse(const uint32 p_Diff) override
    {
        // Continue activating Lava Bomb triggers while dead until they've all despawned
        if (m_Events.GetNextEventTime(eEvents::EventLavaBombTrigger))
        {
            bool l_AllDespawned = true;

            std::list<Creature*> l_CreListe;
            GetCreatureListWithEntryInGrid(l_CreListe, m_creature, eCreatures::NpcLavaBombDummy, 100.0f);
            for (auto const &i_Creature : l_CreListe)
            {
                i_Creature->CastSpell(i_Creature, eSpells::SpellLavaBombAlt, false);
                l_AllDespawned = false;
            }

            m_Events.Repeat(l_AllDespawned ? Milliseconds(0) : Milliseconds(1125));
        }
    }

private:
    EventMap m_Events;
    ScriptedInstance* m_Instance;
};

CreatureAI* GetAI_boss_magmadar(Creature* pCreature)
{
    return new boss_magmadarAI(pCreature);
}

void AddSC_boss_magmadar()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_magmadar";
    newscript->GetAI = &GetAI_boss_magmadar;
    newscript->RegisterSelf();
}
