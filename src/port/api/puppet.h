#pragma once

#include <stdint.h>

#include "api.h"
#include "m_actor.h"

// Puppets are player figures a mod moves, such as other players in an online town. A puppet follows the last pose set
// on it and is in whatever scene is being played; remove it when the player it stands for is somewhere else.

#define C_PUPPET_MAX 16

// Fishing: the bobber is out, at bobber.
#define C_POSE_BOBBER (1 << 0)
// Holding the sponge for washing Gracie's car.
#define C_POSE_SPONGE (1 << 1)

// A player's place, animation, look and what they hold. The numbers are the game's own, so a pose read on one game
// shows the same on another.
typedef struct {
    float x;
    float y;
    float z;
    int16_t angle_y;
    int16_t anim;   // mPlayer_ANIM_*
    float frame;    // where in the animation
    float speed;    // animation frames per game frame
    uint8_t gender; // 0 boy, 1 girl
    uint8_t face;   // 0 to 7
    uint16_t cloth; // clothing number, as the player's cloth.idx; see C_GetPlayerDesign for the ones that differ
    uint8_t anim_mode; // 0 plays once, 1 repeats
    uint8_t eye;       // eye texture, 0 to 7: blinks and expressions
    uint8_t mouth;     // mouth texture, 0 to 5
    uint8_t flags;     // C_POSE_*
    int16_t head_angle[3]; // x, y, z: looking at something
    int16_t tilt_z;        // leaning, as in Kapp'n's boat
    int8_t item_kind;      // mPlayer_ITEM_KIND_* in hand, or -1
    int8_t item_main;      // mPlayer_ITEM_MAIN_*: what is being done with it
    int8_t item_shape;     // mPlayer_ITEM_DATA_* model or skeleton, or -1
    int8_t item_anim;      // mPlayer_ITEM_DATA_* animation of the net, rod, balloon or pinwheel, or -1
    uint8_t item_mode;     // 0 plays once, 1 repeats
    int8_t umbrella_state; // the umbrella being taken out, held or put away (aTOL_ACTION_*)
    int16_t rod_angle_z;
    float item_frame;
    float item_speed;
    float item_scale;
    int16_t net_angle[3];
    int16_t balloon_lean;
    int16_t balloon_angle[3];
    int16_t balloon_rot_x;
    float bobber[3];
    int16_t bobber_angle_x;
    int16_t bobber_angle_y;
    uint16_t held_item;    // an item being picked up or dug up, or 0xFFF1 (EMPTY_NO) for none
    float held_scale;
    float held_offset[3];  // from x, y, z
} C_PlayerPose;

enum {
    C_DESIGN_CLOTH,
    C_DESIGN_UMBRELLA,
};

// A 32x32 4-bit texture and its 16 colours, as the game keeps a design.
typedef struct {
    uint8_t tex[512];
    uint16_t pal[16];
} C_Design;

// Writes the pose of the player being played. Returns FALSE outside of gameplay.
#define C_GetPlayerPose(pose) C_GetPlayerPoseSized((pose), sizeof(*(pose)))
BW_API int C_GetPlayerPoseSized(C_PlayerPose* pose, uint32_t size);
// Writes the design the player wears (C_DESIGN_CLOTH) or carries as an umbrella (C_DESIGN_UMBRELLA) when another game
// can't show it by number: one of the player's own designs, or clothes or an umbrella added by a mod. Returns FALSE
// for the game's own clothes and umbrellas, and outside of gameplay.
BW_API int C_GetPlayerDesign(int which, C_Design* design);

// Makes a puppet and returns it, or -1 when C_PUPPET_MAX are in use. It shows once it has a pose.
BW_API int C_SpawnPuppet(void);
// Moves the puppet to the pose, easing towards it when it is close.
#define C_SetPuppetPose(puppet, pose) C_SetPuppetPoseSized((puppet), (pose), sizeof(*(pose)))
BW_API void C_SetPuppetPoseSized(int puppet, const C_PlayerPose* pose, uint32_t size);
// Shows the design on the puppet's clothes or umbrella in place of the number in its pose. NULL goes back to the
// number.
BW_API void C_SetPuppetDesign(int puppet, int which, const C_Design* design);
// Plays a sound the player made where the puppet is, offset by dx, dy, dz from it. kind and id are as in the
// PlayerSound event.
BW_API void C_PuppetPlaySound(int puppet, int kind, uint16_t id, float dx, float dy, float dz);
// Makes an effect the player made where the puppet is, offset by dx, dy, dz from it. The rest is as in the PlayerEffect
// event. Does nothing for the arrow the game points at the player's own catch.
BW_API void C_PuppetMakeEffect(int puppet, int effect, float dx, float dy, float dz, int prio, int16_t angle,
                               uint16_t item, int16_t arg0, int16_t arg1);
// Removes the puppet. A mod's puppets are removed when it is unloaded.
BW_API void C_RemovePuppet(int puppet);
// The puppet's actor, or NULL while it isn't in the scene.
BW_API ACTOR* C_GetPuppetActor(int puppet);
