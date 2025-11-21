#include "Public\GameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Entity_IsDead_True, "Entity.IsDead.True", "The entity is dead, long live the entity");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Perception_Player_Seen, "Entity.Player.Seen", "We have line of sight to the player from an enemy unit in the last 8 ticks.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Encounter_Wave_Lost, "Encounter.Wave.Lost", "Players have defeated a full wave.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Encounter_Wave_Forming, "Encounter.Wave.Forming", "We're prepping a new wave for the players.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Encounter_Boss_Needed, "Encounter.Boss.Needed", "We need to spawn a boss.");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Move_Possible, "Orders.Move.Possible", "Tag denoting this entity can possibly execute a move order.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Move_Available, "Orders.Move.Available", "Tag denoting this entity can receive a move order.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Move_Needed, "Orders.Move.Needed", "Tag denoting this entity needs a move order. Often means the unit is stuck or arrived.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Move_Break, "Orders.Move.Break", "Tag denoting this entity is stopping a move order. Often means the unit is stuck or arrived.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Target_Available, "Orders.Target.Available", "Tag denoting this entity is ready to be provided a target.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Target_Break, "Orders.Target.Break", "Tag set to indicate that an entity should break off from its current target.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Target_Needed, "Orders.Target.Needed", "Tag denoting this entity's target is no longer valid.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Attack_Opportunistic, "Orders.Attack.Opportunistic", "Tag to mark that an entity should pick targets when it can.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Attack_Available, "Orders.Attack.Available", "Tag to mark that an entity is ready for attack orders. Mostly used to mark cooldown is finished.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Attack_Break, "Orders.Attack.Break", "Tag set to mark that an entity should not be attacking. Often used for panicked retreats or stealth.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Attack_HarrassOnly, "Orders.Attack.HarassOnly", "Accessory tag: Don't kill your target. Just do damage.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Rally_PreferSquad, "Orders.Rally.PreferSquad", "Accessory tag: fill squads before looking for any other rally.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Orders_Rally_Needed, "Orders.Rally.Needed", "This entity is useless where it is. Send it to patrol, rally, or squad up.")


UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EnemyProjectile, "GameplayTag.EnemyProjectile", "Tag denoting something is a projectile from an enemy.")
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Multimode_Rallying, "Status.Multimode.Rallying", "The entity is in the rally behavior mode. Rally permits other modes.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Multimode_NoRallying, "Status.Multimode.NoRallying", "This permits all modes except rally. Used to cool down rally.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Status_Multimode_Assault, "Status.Multimode.Assault", "The entity is in the assault behavior mode.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Object_Supports_Rallying, "Object.Supports.Rallying", "This object, generally a smart object, supports rallying behavior attempts.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_Enemy, "Enemy", "This object supports trying to murder the player.");
