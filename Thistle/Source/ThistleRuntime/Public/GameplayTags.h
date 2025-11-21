#pragma once

#include "NativeGameplayTags.h"
//orders are a two-way mechanism for state trees to understand what a unit is ready to do, and what it needs.
//state trees shape behavior and reactivity by issuing orders.
//orders are granular tasks and we have very few of them. most behaviors are multiple orders.
//orders can be executed simultaneously. a good example of this is that we'd really like enemies to move and shoot at the same time, but...
//we might want them to aim differently depending on how they are moving versus how the player is moving. So move and shoot are obviously orders
//but so is aim! In fact, we have only four orders at the moment. Move, Target, Attack, Rally.
//
//While a unit can receive one order of each type at a time, most can't take multiples of a type.
//So Aim is a kind of Target order. Ranged attack is a kind of Attack Order. So is Harrying. Move generally takes the form of a simple PoI to navigate to.
//Rally is patrol, squad up, take cover, retreat, convoy, and of course, rally for wave attack. Any objective, really.
//
//These tags let a state tree or any sort of state machine know if a unit is available. Available units may ALSO be in the needed state, where their existing order
//can't be completed. Not all entities support all orders or all forms of orders.

//For anything else, you really may wish to use Mass via the slowly expanding artillery integration.
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Entity_IsDead_True);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Perception_Player_Seen);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Encounter_Wave_Lost);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Encounter_Wave_Forming);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Encounter_Boss_Needed);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_EnemyProjectile)
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Object_Supports_Rallying);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Status_Multimode_Rallying);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Status_Multimode_NoRallying);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Status_Multimode_Assault);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Rally_Needed);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Rally_PreferSquad);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Attack_HarrassOnly);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Attack_Break);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Attack_Available);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Attack_Opportunistic);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Target_Needed);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Target_Break);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Target_Available);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Move_Needed);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Move_Break);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Move_Available);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Orders_Move_Possible);
THISTLERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Enemy);