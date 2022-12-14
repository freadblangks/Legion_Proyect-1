/*
    https://uwow.biz/
    Dungeon : Cathedral of Eternal Night 7.2
*/

#include "cathedral_of_eternal_night.h"
#include "AreaTrigger.h"
#include "AreaTriggerAI.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"

enum Says
{
    SAY_INTRO           = 0,
    SAY_AGGRO           = 1,
    SAY_WAVE            = 2,
    SAY_WAVE_TWO        = 3,
    SAY_CHAOTIC_ENERGY  = 4,
    SAY_CHAOTIC_ENERGY_1  = 5,
    SAY_DEATH           = 6,
};

enum Spells
{
    SPELL_WAVE_ONE          = 235822,
    SPELL_WAVE_TWO          = 235862,
    
    SPELL_FELSOUL_CLEAVE    = 236543, // 18
    SPELL_CHAOTIC_ENERGY    = 239339, // (visual only ?)
    SPELL_CHAOTIC_ENERGY_DMG = 234107, // 
    
    SPELL_ENRAGE            = 243157, // 25%
    
    // egida
    SPELL_EGIDA_DISPLAY_BAR = 236421,
    SPELL_EGIDA_AT          = 239350,
    SPELL_EGIDA_AT_BUFF     = 238410,
    SPELL_EGIDA_OUTRO       = 239352,
    
    // portals (118834)
    SPELL_PORTAL_PERIODIC_ONE   = 235827,  // heroic - imps,
    SPELL_PORTAL_PERIODIC_TWO   = 235881,  // heroic - guards, mythic - shivarras
    SPELL_PORTAL_MYTHIC_ADD     = 241622, 
    
    // other
    SPELL_VISUAL_SPAWN      = 211762,
    SPELL_INTRO             = 241001,
    SPELL_OUTRO             = 241382, 
};

enum eEvents
{
    EVENT_FELSOUL_CLEAVE   = 1,
    EVENT_CHAOTIC_ENERGY,
    EVENT_WAVE_ONE,
    EVENT_WAVE_TWO,
    EVENT_ENRAGE
};

Position const portals_position[4]
{
    {-519.67f, 2548.22f, 533.94f, 3.94f},
    {-522.056f, 2504.01f, 533.94f, 2.24f},
    {-564.17f, 2503.62f, 533.94f, 0.77f},
    {-561.85f, 2549.77f, 533.94f, 5.45f}    
};

// 118804
struct boss_domatrax : public BossAI
{
    boss_domatrax(Creature* creature) : BossAI(creature, DATA_DOMATRAX) {}
    
    uint8 phase = 0;
    uint32 const phases_hp_required[3] = {90, 50, 25};

    void Reset() override
    {
        _Reset();
     
        phase = 0;
        me->SetPower(POWER_ENERGY, 0);
        
        if (Creature* egida = instance->instance->GetCreature(instance->GetGuidData(NPC_EGIDA_START)))
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, egida);
            instance->SendEncounterUnit(ENCOUNTER_FRAME_INSTANCE_END, egida);
            
            egida->RemoveAurasDueToSpell(SPELL_EGIDA_DISPLAY_BAR);
            egida->RemoveAurasDueToSpell(SPELL_EGIDA_AT);
        }
        
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_NOT_SELECTABLE);
        me->SetReactState(REACT_PASSIVE);
        me->SetVisible(false);
    }

    void EnterCombat(Unit* /*who*/) override
    {
        Talk(SAY_AGGRO);
        _EnterCombat();
        
        phase = 0;
        me->SetPower(POWER_ENERGY, 0);

        events.RescheduleEvent(EVENT_FELSOUL_CLEAVE, 8000);
        events.RescheduleEvent(EVENT_CHAOTIC_ENERGY, 1000);
        
        if (Creature* egida = instance->instance->GetCreature(instance->GetGuidData(NPC_EGIDA_START)))
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, egida);
            instance->SendEncounterUnit(ENCOUNTER_FRAME_INSTANCE_START, egida);
            
            egida->CastSpell(egida, SPELL_EGIDA_DISPLAY_BAR);
            egida->AddDelayedEvent(1000, [egida]()-> void
            {
                egida->CastSpell(egida, SPELL_EGIDA_AT);
            });
        }
        
    }
    
    void JustSummoned(Creature* summon) override
    {            
        summons.Summon(summon);
        
        summon->CastSpell(summon, SPELL_VISUAL_SPAWN);
        if (summon->GetEntry() == NPC_PORTAL_OWNER)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_ENGAGE, summon);
            instance->SendEncounterUnit(ENCOUNTER_FRAME_INSTANCE_START, summon);
            
            summon->SetReactState(REACT_PASSIVE);
            summon->CastSpell(summon, phase == 1 ? SPELL_PORTAL_PERIODIC_ONE: SPELL_PORTAL_PERIODIC_TWO); // periodic summon adds
            
            // me->SummonCreature(phase == 1 ? NPC_IMP : (GetDifficultyID() == DIFFICULTY_HEROIC ? NPC_FELGUARD : NPC_SHIVARRA), summon->GetPositionX(), summon->GetPositionY(), summon->GetPositionZ(), summon->GetOrientation());
            
            if (GetDifficultyID() != DIFFICULTY_HEROIC) // mythic and other. normal mode isn't exist
                summon->CastSpell(summon, SPELL_PORTAL_MYTHIC_ADD);
        }
    
        DoZoneInCombat(summon, 150.0f);
    }
    
    
    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        if (summon->GetEntry() == NPC_PORTAL_OWNER)
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, summon);
            instance->SendEncounterUnit(ENCOUNTER_FRAME_INSTANCE_END, summon);
        }
    }
    
    void SpellFinishCast(const SpellInfo* spell) override
    {
        switch(spell->Id)
        {
            case SPELL_WAVE_TWO:
                Talk(SAY_WAVE_TWO);
                // no break
            case SPELL_WAVE_ONE:
                Talk(SAY_WAVE);
                uint8 position = urand(0 ,3);
                for (uint8 i = 0; i < 2; ++i)
                {
                    me->SummonCreature(NPC_PORTAL_OWNER, portals_position[position++]);
                    
                    if (position >= 4)
                        position = 0;
                }
                break;
        }  
    }

    void DoAction(int32 const action) override
    {
        me->SetVisible(true);
        DoCast(SPELL_INTRO);
        
        me->AddDelayedEvent(1000, [this]()-> void
        {
            Talk(SAY_INTRO);
        });
        
        me->AddDelayedEvent(3000, [this]()-> void
        {
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_NOT_SELECTABLE);
            me->SetReactState(REACT_DEFENSIVE);
        });
        
    }
    
    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_DEATH);
        _JustDied();
        
        if (Creature* egida = instance->instance->GetCreature(instance->GetGuidData(NPC_EGIDA_START)))
        {
            instance->SendEncounterUnit(ENCOUNTER_FRAME_DISENGAGE, egida);
            instance->SendEncounterUnit(ENCOUNTER_FRAME_INSTANCE_END, egida);
            
            egida->RemoveAurasDueToSpell(SPELL_EGIDA_DISPLAY_BAR);
            egida->RemoveAurasDueToSpell(SPELL_EGIDA_AT);
            
            me->CastSpell(egida, SPELL_OUTRO, true);
            egida->CastSpell(egida, SPELL_EGIDA_OUTRO);
            egida->DespawnOrUnsummon(3000);
            
            // egida->SummonCreature(NPC_EGIDA_GIVE_BUFF, egida->GetPositionX() + frand(-3, 3), egida->GetPositionY() + frand(-3, 3), egida->GetPositionZ(), egida->GetOrientation());
        }
    }
    
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType dmgType) override
    {
        if (phase < 3 && me->HealthBelowPct(phases_hp_required[phase]))
        {
            switch(phase++)
            {
                case 0:
                    events.RescheduleEvent(EVENT_WAVE_ONE, 100);
                    break;
                case 1:
                    events.RescheduleEvent(EVENT_WAVE_TWO, 100);
                    break;
                case 2:
                    events.RescheduleEvent(EVENT_ENRAGE, 100);
                    break;
                default:
                    break;
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_FELSOUL_CLEAVE:
                    DoCast(SPELL_FELSOUL_CLEAVE);
                    events.RescheduleEvent(EVENT_FELSOUL_CLEAVE, 18000);
                    break;
                case EVENT_CHAOTIC_ENERGY:
                    if (me->GetPower(POWER_ENERGY) < 100)
                    {
                        me->SetPower(POWER_ENERGY, me->GetPower(POWER_ENERGY) + urand(2, 4));
                        events.RescheduleEvent(EVENT_CHAOTIC_ENERGY, 1000);
                    }
                    else
                    {
                        Talk(SAY_CHAOTIC_ENERGY);
                        Talk(SAY_CHAOTIC_ENERGY_1);
                        DoCast(SPELL_CHAOTIC_ENERGY);
                        DoCast(SPELL_CHAOTIC_ENERGY_DMG);
                        events.RescheduleEvent(EVENT_CHAOTIC_ENERGY, 5000);
                    }
                    break;
                case EVENT_WAVE_ONE:
                    DoCast(SPELL_WAVE_ONE);
                    break;
                case EVENT_WAVE_TWO:
                    DoCast(SPELL_WAVE_TWO);
                    break;
                case EVENT_ENRAGE:
                    DoCast(SPELL_ENRAGE);
                    break;
            }
        }
        DoMeleeAttackIfReady();
    }
};

// 118884
struct npc_coen_egida : public ScriptedAI
{
    npc_coen_egida(Creature* creature) : ScriptedAI(creature)
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);
    }

    void Reset() override {}

    void sGossipSelect(Player* player, uint32 sender, uint32 action) override
    {
        me->RemoveAurasDueToSpell(144373);
        me->RemoveFlag(UNIT_FIELD_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        
        if (InstanceScript* instance = me->GetInstanceScript())
            if (Creature* domatrax = instance->instance->GetCreature(instance->GetGuidData(BOSS_DOMATRAX)))
                domatrax->AI()->DoAction(true);
    }
    
    void UpdateAI(uint32 diff) override { }
};

// 14825
struct areatrigger_domatrax_egida_shield : AreaTriggerAI
{
    areatrigger_domatrax_egida_shield(AreaTrigger* areatrigger) : AreaTriggerAI(areatrigger)
    {
        scale_by_one_pct = (at->GetRadius()) / 100;
    }
    
    float scale_by_one_pct{};
    
    void ActionOnUpdate(GuidList& affectedPlayers) override
    {
        Unit* caster = at->GetCaster();
        if (!caster)
            return;
        
        float new_scale = scale_by_one_pct * caster->GetPower(POWER_ALTERNATE);
        if (!new_scale)
        {
            at->Despawn();
            return;
        }
        
        at->SetSphereScale(new_scale, 100, true);
    }

};

// 235827, 235881
class spell_domatrax_portals : public AuraScript
{
    PrepareAuraScript(spell_domatrax_portals);

    void OnTick(AuraEffect const* /*aurEff*/)
    {
        Unit* caster = GetCaster();
        if (!caster || caster->GetEntry() != NPC_PORTAL_OWNER)
            return;
        
        Unit* owner = caster->GetAnyOwner();
        if (!owner || owner->GetEntry() != BOSS_DOMATRAX)
            return;
        
        owner->SummonCreature(GetSpellInfo()->Id == SPELL_PORTAL_PERIODIC_ONE ? NPC_IMP : (caster->GetMap()->GetDifficultyID() == DIFFICULTY_HEROIC ? NPC_FELGUARD : NPC_SHIVARRA), caster->GetPositionX(), caster->GetPositionY(), caster->GetPositionZ(), caster->GetOrientation());
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_domatrax_portals::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};


void AddSC_boss_domatrax()
{
    RegisterCreatureAI(boss_domatrax);
    RegisterCreatureAI(npc_coen_egida);

    RegisterAuraScript(spell_domatrax_portals);

    RegisterAreaTriggerAI(areatrigger_domatrax_egida_shield);
}
