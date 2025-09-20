#ifndef CARD_H
#define CARD_H

#include <tonc.h>
#include <maxmod.h>

#include "sprite.h"

#define CARD_TID 0
#define CARD_SPRITE_OFFSET 16
#define CARD_PB 0
#define CARD_STARTING_LAYER 0

/*
// Card suits
#define HEARTS 0
#define CLUBS 1
#define DIAMONDS 2
#define SPADES 3
#define NUM_SUITS 4

// Card ranks
#define TWO 0
#define THREE 1
#define FOUR 2
#define FIVE 3
#define SIX 4
#define SEVEN 5
#define EIGHT 6
#define NINE 7
#define TEN 8
#define JACK 9
#define QUEEN 10
#define KING 11
#define ACE 12
#define NUM_RANKS 13
#define RANK_OFFSET 2 // Because the first rank is 2 and ranks start at 0
*/

enum CardSuit {
    CARD_SUIT_HEARTS,
    CARD_SUIT_CLUBS,
    CARD_SUIT_DIAMONDS,
    CARD_SUIT_SPADES,
    CARD_SUIT_MAX,
};

enum CardRank {
    CARD_RANK_TWO,
    CARD_RANK_THREE,
    CARD_RANK_FOUR,
    CARD_RANK_FIVE,
    CARD_RANK_SIX,
    CARD_RANK_SEVEN,
    CARD_RANK_EIGHT,
    CARD_RANK_NINE,
    CARD_RANK_TEN,
    CARD_RANK_JACK,
    CARD_RANK_QUEEN,
    CARD_RANK_KING,
    CARD_RANK_ACE,
    CARD_RANK_MAX,
};

#define IMPOSSIBLY_HIGH_CARD_VALUE 100

// Card types
typedef struct
{
    enum CardSuit suit;
    enum CardRank rank;
} Card;

typedef struct CardObject
{
    Card *card;
    SpriteObject *sprite_object;
} CardObject;

// Card functions
void card_init();

// Card methods
Card *card_new(enum CardSuit suit, enum CardRank rank);
void card_destroy(Card **card);
u8 card_get_value(Card *card);

// CardObject methods
CardObject *card_object_new(Card *card);
void card_object_destroy(CardObject **card_object);
void card_object_update(CardObject *card_object); // Update the card object position and scale
void card_object_set_sprite(CardObject *card_object, int layer);
void card_object_shake(CardObject* card_object, mm_word sound_id);

void card_object_set_selected(CardObject* card_object, bool selected);
bool card_object_is_selected(CardObject* card_object);
Sprite* card_object_get_sprite(CardObject* card_object);

#endif // CARD_H
