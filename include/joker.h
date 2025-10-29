#ifndef JOKER_H
#define JOKER_H

#include <maxmod.h>

#include "sprite.h"
#include "card.h"
#include "game.h"
#include "graphic_utils.h"

// This won't be more than the number of jokers in your current deck
// plus the amount that can fit in the shop, 8 should be fine. For now...
#define MAX_ACTIVE_JOKERS 8

#define JOKER_TID (MAX_HAND_SIZE + MAX_SELECTION_SIZE) * JOKER_SPRITE_OFFSET // Tile ID for the starting index in the tile memory
#define JOKER_SPRITE_OFFSET 16 // Offset for the joker sprites
#define JOKER_BASE_PB 4 // The starting palette index for the jokers
#define JOKER_LAST_PB (NUM_PALETTES - 1) 
// Currently allocating the rest of the palettes for the jokers.
// This number needs to be decreased once we need to allocated palettes for other sprites
// such as planet cards etc.

#define JOKER_STARTING_LAYER 27

#define BASE_EDITION 0
#define FOIL_EDITION 1
#define HOLO_EDITION 2
#define POLY_EDITION 3
#define NEGATIVE_EDITION 4

#define MAX_EDITIONS 5

#define COMMON_JOKER 0
#define UNCOMMON_JOKER 1
#define RARE_JOKER 2
#define LEGENDARY_JOKER 3

// Percent chance to get a joker of each rarity
// Note that this deviates slightly from the Balatro wiki to allow legendary
// jokers to appear without spectral cards implemented
#define COMMON_JOKER_CHANCE 70
#define UNCOMMON_JOKER_CHANCE 25
#define RARE_JOKER_CHANCE 5
#define LEGENDARY_JOKER_CHANCE 0

#define MAX_JOKER_OBJECTS 32 // The maximum number of joker objects that can be created at once

#define DEFAULT_JOKER_ID 0
#define GREEDY_JOKER_ID 1 // This is just an example to show the patern of making joker IDs
#define JOKER_STENCIL_ID 16
#define PAREIDOLIA_JOKER_ID 30
#define JOKER_BRAINSTORM_ID 40

#include "list.h"

typedef struct 
{
    u8 id; // Unique ID for the joker, used to identify different jokers
    u8 modifier; // base, foil, holo, poly, negative
    u8 value;
    u8 rarity;
    bool processed;
} Joker;

typedef struct JokerObject
{
    Joker *joker;
    SpriteObject* sprite_object;
} JokerObject;

typedef struct  // These jokers are triggered after the played hand has finished scoring.
{
    int chips;
    int mult;
    int xmult;
    int money;
    bool retrigger; // Retrigger played hand (e.g. "Dusk" joker, even though on the wiki it says "On Scored" it makes more sense to have it here)
} JokerEffect;

typedef JokerEffect (*JokerEffectFunc)(Joker *joker, Card *scored_card);
typedef struct {
    u8 rarity;
    u8 base_value;
    JokerEffectFunc effect;
} JokerInfo;
const JokerInfo* get_joker_registry_entry(int joker_id);
size_t get_joker_registry_size(void);

void joker_init();

Joker *joker_new(u8 id);
void joker_destroy(Joker **joker);

// Unique effects like "Four Fingers" or "Credit Card" will be hard coded into game.c with a conditional check for the joker ID from the players owned jokers
// game.c should probably be restructured so most of the variables in it are moved to some sort of global variable header file so they can be easily accessed and modified for the jokers
JokerEffect joker_get_score_effect(Joker *joker, Card *scored_card);
int joker_get_sell_value(const Joker* joker);

JokerObject *joker_object_new(Joker *joker);
void joker_object_destroy(JokerObject **joker_object);
void joker_object_update(JokerObject *joker_object);
void joker_object_shake(JokerObject *joker_object, mm_word sound_id); // This doesn't actually score anything, it just performs an animation and plays a sound effect
bool joker_object_score(JokerObject *joker_object, Card* scored_card, int *chips, int *mult, int *xmult, int *money, bool *retrigger); // This scores the joker and returns true if it was scored successfully (Card = NULL means the joker is independent and not scored by a card)

void joker_object_set_selected(JokerObject* joker_object, bool selected);
bool joker_object_is_selected(JokerObject* joker_object);

Sprite* joker_object_get_sprite(JokerObject* joker_object);
int joker_get_random_rarity();

#endif // JOKER_H
