#ifndef GAME_H
#define GAME_H

#define MAX_HAND_SIZE          16
#define MAX_DECK_SIZE          52
#define MAX_JOKERS_HELD_SIZE   5 // This doesn't account for negatives right now.
#define MAX_SHOP_JOKERS        2 // TODO: Make this dynamic and allow for other items besides jokers
#define MAX_SELECTION_SIZE     5
#define MAX_CARD_SCORE_DIGITS  2 // Current digit limit for score received from cards including mult etc. from jokers
#define MAX_CARD_SCORE_STR_LEN (MAX_CARD_SCORE_DIGITS + 1) // For the '+' or 'X'
#define FRAMES(x)              (((x) + game_speed - 1) / game_speed)

// TODO: Turn into enum?
#define BG_ID_CARD_SELECTING 1
#define BG_ID_CARD_PLAYING   2
#define BG_ID_ROUND_END      3
#define BG_ID_SHOP           4
#define BG_ID_BLIND_SELECT   5
#define BG_ID_MAIN_MENU      6

// Input bindings
#define SELECT_CARD    KEY_A
#define DESELECT_CARDS KEY_B
#define PEEK_DECK      KEY_L // Not implemented
#define SORT_HAND      KEY_R
#define PAUSE_GAME     KEY_START // Not implemented
#define SELL_KEY       KEY_L

enum GameState
{
    GAME_SPLASH_SCREEN,
    GAME_MAIN_MENU,
    GAME_PLAYING,
    GAME_ROUND_END,
    GAME_SHOP,
    GAME_BLIND_SELECT,
    GAME_LOSE,
    GAME_WIN
};

enum HandState
{
    HAND_DRAW,
    HAND_SELECT,
    HAND_SHUFFLING, // This is actually a misnomer because it's used for the deck, but it mechanically makes sense to be
                    // a state of the hand
    HAND_DISCARD,
    HAND_PLAY,
    HAND_PLAYING
};

enum PlayState
{
    PLAY_PLAYING,
    PLAY_SCORING,
    PLAY_ENDING,
    PLAY_ENDED
};

// Hand types
enum HandType
{
    NONE,
    HIGH_CARD,
    PAIR,
    TWO_PAIR,
    THREE_OF_A_KIND,
    FOUR_OF_A_KIND,
    STRAIGHT,
    FLUSH,
    FULL_HOUSE,
    STRAIGHT_FLUSH,
    ROYAL_FLUSH,
    FIVE_OF_A_KIND,
    FLUSH_HOUSE,
    FLUSH_FIVE
};

// Game functions
void game_init();
void game_update();
void game_set_state(enum GameState new_game_state);

// Forward declaration
struct List;
typedef struct List List;

// Utility functions for other files
typedef struct CardObject CardObject; // forward declaration, actually declared in card.h
typedef struct Card Card;
typedef struct JokerObject JokerObject;
CardObject** get_hand_array(void);
int get_hand_top(void);
int hand_get_size(void);
CardObject** get_played_array(void);
int get_played_top(void);
List* get_jokers(void);
bool is_joker_owned(int joker_id);
bool card_is_face(Card* card);

int get_deck_top(void);
int get_num_discards_remaining(void);
int get_num_hands_remaining(void);
int get_money(void);

int get_game_speed(void);
void set_game_speed(int new_game_speed);

#endif // GAME_H
