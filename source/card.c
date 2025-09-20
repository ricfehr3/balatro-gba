#include "card.h"

#include <maxmod.h>
#include <stdlib.h>

#include "deck_gfx.h"
#include "graphic_utils.h"

// Audio
#include "soundbank.h"

#define FACE_VAL 10
#define ACE_VAL  11

// Card sprites lookup table. First index is the suit, second index is the rank. The value is the tile index.
static const u16 card_sprite_lut[NUM_SUITS][NUM_RANKS] = {
    {0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192},
    {208, 224, 240, 256, 272, 288, 304, 320, 336, 352, 368, 384, 400},
    {416, 432, 448, 464, 480, 496, 512, 528, 544, 560, 576, 592, 608},
    {624, 640, 656, 672, 688, 704, 720, 736, 752, 768, 784, 800, 816}
};

static const int card_scores[CARD_RANK_MAX] =
{
     1, // CARD_RANK_TWO
     2, // CARD_RANK_THREE
     3, // CARD_RANK_FOUR
     4, // CARD_RANK_FIVE
     5, // CARD_RANK_SIX
     6, // CARD_RANK_SEVEN
     7, // CARD_RANK_EIGHT
     8, // CARD_RANK_NINE
     9, // CARD_RANK_TEN
    10, // CARD_RANK_JACK
    10, // CARD_RANK_QUEEN
    10, // CARD_RANK_KING
    11, // CARD_RANK_ACE
};

void card_init()
{
    GRIT_CPY(&pal_obj_mem[CARD_PB], deck_gfxPal);
}

// Card methods
Card *card_new(enum CardSuit suit, enum CardRank rank)
{
    Card *card = malloc(sizeof(Card));

    card->suit = suit;
    card->rank = rank;

    return card;
}

void card_destroy(Card **card)
{
    if (*card == NULL) return;
    free(*card);
    *card = NULL;
}

u8 card_get_value(Card *card)
{
    return card_scores[card->rank];
}

// CardObject methods
CardObject *card_object_new(Card *card)
{
    CardObject *card_object = malloc(sizeof(CardObject));

    card_object->card = card;
    card_object->sprite_object = sprite_object_new();

    return card_object;
}

void card_object_destroy(CardObject **card_object)
{
    if (*card_object == NULL) return;
    sprite_object_destroy(&((*card_object)->sprite_object));
    //card_destroy(&(*card_object)->card); // In practice, this is unnecessary because the card will be inserted into the discard pile and then back into the deck. If you need to destroy the card, you can do it manually before calling this function.
    free(*card_object);
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

    u8 suit = card_object->card->suit;
    u8 rank = card_object->card->rank;
    u16 card_sprite = card_sprite_lut[suit][rank];
    u16 offset = card_sprite * TILE_SIZE;
    u32 size = TILE_SIZE * CARD_SPRITE_OFFSET;
    u16 a0 = ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF;
    u16 a1 = ATTR1_SIZE_32;

    Sprite* sprite = sprite_new(a0, a1, tile_index, 0, layer + CARD_STARTING_LAYER);

    memcpy32(&tile_mem[4][tile_index], &deck_gfxTiles[offset], size);

    sprite_object_set_sprite(card_object->sprite_object, sprite);
}

void card_object_shake(CardObject* card_object, mm_word sound_id)
{
    sprite_object_shake(card_object->sprite_object, sound_id);
}

void card_object_set_selected(CardObject* card_object, bool selected)
{
    if (card_object == NULL) return;
    sprite_object_set_selected(card_object->sprite_object, selected);
}

bool card_object_is_selected(CardObject* card_object)
{
    if (card_object == NULL) return false;
    return sprite_object_is_selected(card_object->sprite_object);
}

Sprite* card_object_get_sprite(CardObject* card_object)
{
    if (card_object == NULL) return NULL;
    return sprite_object_get_sprite(card_object->sprite_object);
}
