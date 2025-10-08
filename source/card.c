#include "card.h"

#include <maxmod.h>
#include <stdlib.h>

#include "deck_gfx.h"
#include "graphic_utils.h"

// Audio
#include "soundbank.h"

#include "pool.h"

#define MAX_CARDS_PER_HAND (NUM_SUITS * NUM_RANKS)
#define MAX_CARDS_ON_SCREEN 16

DECLARE_POOL_TYPE(Card);
DEFINE_POOL_TYPE(Card, MAX_CARDS_PER_HAND);
DECLARE_POOL_TYPE(CardObject);
DEFINE_POOL_TYPE(CardObject, MAX_CARDS_ON_SCREEN);

// Card sprites lookup table. First index is the suit, second index is the rank. The value is the tile index.
const static u16 card_sprite_lut[NUM_SUITS][NUM_RANKS] = {
    {0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192},
    {208, 224, 240, 256, 272, 288, 304, 320, 336, 352, 368, 384, 400},
    {416, 432, 448, 464, 480, 496, 512, 528, 544, 560, 576, 592, 608},
    {624, 640, 656, 672, 688, 704, 720, 736, 752, 768, 784, 800, 816}
};

void card_init()
{
    GRIT_CPY(&pal_obj_mem[CARD_PB], deck_gfxPal);
}

// Card methods
Card *card_new(u8 suit, u8 rank)
{
    Card *card = POOL_GET(Card);

    card->suit = suit;
    card->rank = rank;

    return card;
}

void card_destroy(Card **card)
{
    POOL_FREE(Card, *card);
    *card = NULL;
}

u8 card_get_value(Card *card)
{
    if (card->rank == JACK || card->rank == QUEEN || card->rank == KING)
    {
        return 10; // Face cards are worth 10
    }
    else if (card->rank == ACE)
    {
        return 11; // Ace is worth 11
    }
    else
    {
        return card->rank + RANK_OFFSET; // 2-10 are worth their rank + RANK_OFFSET
    }

    return 0; // Should never reach here, but just in case
}

// CardObject methods
CardObject *card_object_new(Card *card)
{
    CardObject *card_object = POOL_GET(CardObject);

    card_object->card = card;
    card_object->sprite_object = sprite_object_new();

    return card_object;
}

void card_object_destroy(CardObject **card_object)
{
    if (*card_object == NULL) return;
    sprite_object_destroy(&((*card_object)->sprite_object));
    POOL_FREE(CardObject, *card_object);
    *card_object = NULL;
}

void card_object_update(CardObject* card_object)
{
    if (card_object == NULL) return;
    sprite_object_update(card_object->sprite_object);
}

void card_object_set_sprite(CardObject *card_object, int layer)
{
    int tile_index = CARD_TID + (layer * CARD_SPRITE_OFFSET);
    memcpy32(&tile_mem[4][tile_index], &deck_gfxTiles[card_sprite_lut[card_object->card->suit][card_object->card->rank] * TILE_SIZE], TILE_SIZE * CARD_SPRITE_OFFSET);
    sprite_object_set_sprite(card_object->sprite_object, sprite_new(ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF, ATTR1_SIZE_32, tile_index, 0, layer + CARD_STARTING_LAYER));
}

void card_object_shake(CardObject* card_object, mm_word sound_id)
{
    sprite_object_shake(card_object->sprite_object, sound_id);
}

void card_object_set_selected(CardObject* card_object, bool selected)
{
    if (card_object == NULL)
        return;
    sprite_object_set_selected(card_object->sprite_object, selected);
}

bool card_object_is_selected(CardObject* card_object)
{
    if (card_object == NULL)
        return false;
    return sprite_object_is_selected(card_object->sprite_object);
}

Sprite* card_object_get_sprite(CardObject* card_object)
{
    if (card_object == NULL)
        return NULL;
    return sprite_object_get_sprite(card_object->sprite_object);
}
