#include "game.h"

#include <maxmod.h>
#include <tonc.h>
#include <stdlib.h>

#include "tonc_memdef.h"
#include "util.h"
#include "sprite.h"
#include "card.h"
#include "hand_analysis.h"
#include "blind.h"
#include "joker.h"
#include "affine_background.h"
#include "graphic_utils.h"
#include "audio_utils.h"
#include "selection_grid.h"
#include "splash_screen.h"

#include "background_gfx.h"
#include "background_shop_gfx.h"
#include "background_blind_select_gfx.h"
#include "affine_background_gfx.h"
#include "background_main_menu_gfx.h"

#include "soundbank.h"

#include "list.h"
#include "pool.h"

typedef enum
{
    GAME_SHOP_INTRO,
    GAME_SHOP_ACTIVE,
    GAME_SHOP_EXIT,
    GAME_SHOP_MAX
} _GameShopStates;

typedef enum
{
    ROUND_END_START,
    START_EXPAND_POPUP,
    DISPLAY_FINISHED_BLIND,
    DISPLAY_SCORE_MIN,
    UPDATE_BLIND_REWARD,
    BLIND_PANEL_EXIT,
    DISPLAY_REWARDS,
    DISPLAY_CASHOUT,
    DISMISS_ROUND_END_PANEL,
    ROUND_END_EXIT
} _GameRoundEndStates;

typedef enum
{
    START_ANIM_SEQ,
    BLIND_SELECT,
    BLIND_SELECTED_ANIM_SEQ,
    DISPLAY_BLIND_PANEL,
    BLIND_SELECT_MAX
} _BlindSelectStates;

typedef struct 
{
    int chips;
    int mult;
    char *display_name;
} HandValues;

typedef void (*SubStateActionFn)();

// Used as a No Operation for game states that have no init and/or exit function.
// ricfehr3 did the work of determining whether a noop or a NULL check was more 
// efficient. Well, this is the answer.
// Thanks!
// https://github.com/cellos51/balatro-gba/issues/137#issuecomment-3322485129
static void _noop() {}

// These functions need to be forward declared
// so they're visible to the state_info array,
// and the sub-state function tables.
// This could be done, and maybe should be done,
// with an X macro, but I'll leave that to the 
// reviewer(s).
static void game_main_menu_on_init();
static void game_main_menu_on_update();
static void game_round_on_init();
static void game_playing_on_update();
static void game_round_end_on_update();
static void game_round_end_on_exit();
static void game_shop_on_update();
static void game_shop_on_exit();
static void game_blind_select_on_update();
static void game_blind_select_on_exit();
static void game_lose_on_init();
static void game_lose_on_update();
static void game_over_on_exit();
static void game_win_on_init();
static void game_win_on_update();
static void game_shop_intro();
static void game_shop_process_user_input();
static void game_shop_outro();
static void game_blind_select_start_anim_seq();
static void game_blind_select_handle_input();
static void game_blind_select_selected_anim_seq();
static void game_blind_select_display_blind_panel();
static void game_round_end_start();
static void game_round_end_start_expand_popup();
static void game_round_end_display_finished_blind();
static void game_round_end_display_score_min();
static void game_round_end_update_blind_reward();
static void game_round_end_panel_exit();
static void game_round_end_display_rewards();
static void game_round_end_display_cashout();
static void game_round_end_dismiss_round_end_panel();

static uint rng_seed = 0;

static uint timer = 0; // This might already exist in libtonc but idk so i'm just making my own
static int game_speed = 1; // BY DEFAULT IS SET TO 1, but if changed to 2 or more, should speed up all (or most) of the game aspects that should be sped up by speed, as in the original game.
static int background = 0;

static StateInfo state_info[] = 
{
#define DEF_STATE_INFO(stateEnum, init_fn, update_fn, exit_fn) \
    { .on_init = init_fn, .on_update = update_fn, .on_exit = exit_fn, .substate = 0 },
#include "../include/def_state_info_table.h"
#undef DEF_STATE_INFO
};

static const HandValues hand_base_values[] = 
{
    { .chips = 0,       .mult = 0,      .display_name = NULL        },  // NONE
    { .chips = 5,       .mult = 1,      .display_name = "HIGH C"    },  // HIGH_CARD
    { .chips = 10,      .mult = 2,      .display_name = "PAIR"      },  // PAIR
    { .chips = 20,      .mult = 2,      .display_name = "2 PAIR"    },  // TWO_PAIR
    { .chips = 30,      .mult = 3,      .display_name = "3 OAK"     },  // THREE_OF_A_KIND
    { .chips = 60,      .mult = 7,      .display_name = "4 OAK"     },  // FOUR_OF_A_KIND
    { .chips = 30,      .mult = 4,      .display_name = "STRT"      },  // STRAIGHT
    { .chips = 35,      .mult = 4,      .display_name = "FLUSH"     },  // FLUSH
    { .chips = 40,      .mult = 4,      .display_name = "FULL H"    },  // FULL_HOUSE 
    { .chips = 100,     .mult = 8,      .display_name = "STRT F"    },  // STRAIGHT_FLUSH
    { .chips = 100,     .mult = 8,      .display_name = "ROYAL F"   },  // ROYAL_FLUSH
    { .chips = 120,     .mult = 12,     .display_name = "5 OAK"     },  // FIVE_OF_A_KIND
    { .chips = 140,     .mult = 14,     .display_name = "FLUSH H"   },  // FLUSH_HOUSE
    { .chips = 160,     .mult = 16,     .display_name = "FLUSH 5"   }   // FLUSH_FIVE
};

static const SubStateActionFn shop_state_actions[] = 
{ 
    game_shop_intro, game_shop_process_user_input, game_shop_outro 
};

static const SubStateActionFn blind_select_state_actions[] = 
{ 
    game_blind_select_start_anim_seq, game_blind_select_handle_input, 
    game_blind_select_selected_anim_seq, game_blind_select_display_blind_panel
};

static const SubStateActionFn round_end_state_actions[] =
{
    game_round_end_start, game_round_end_start_expand_popup, 
    game_round_end_display_finished_blind, game_round_end_display_score_min,
    game_round_end_update_blind_reward, game_round_end_panel_exit,
    game_round_end_display_rewards, game_round_end_display_cashout,
    game_round_end_dismiss_round_end_panel
};

static enum GameState game_state = GAME_STATE_UNDEFINED; // The current game state, this is used to determine what the game is doing at any given time
static enum HandState hand_state = HAND_DRAW;
static enum PlayState play_state = PLAY_PLAYING;

static enum HandType hand_type = NONE;

static CardObject *main_menu_ace = NULL;

static Sprite *playing_blind_token = NULL; // The sprite that displays the blind when in "GAME_PLAYING/GAME_ROUND_END" state
static Sprite *round_end_blind_token = NULL; // The sprite that displays the blind when in "GAME_ROUND_END" state

static Sprite *blind_select_tokens[BLIND_TYPE_MAX] = {NULL}; // The sprites that display the blinds when in "GAME_BLIND_SELECT" state

static int current_blind = BLIND_TYPE_SMALL;

// The current state of the blinds, this is used to determine what the game is doing at any given time
static enum BlindState blinds[BLIND_TYPE_MAX] =
{
    BLIND_STATE_CURRENT,
    BLIND_STATE_UPCOMING,
    BLIND_STATE_UPCOMING
}; // The current state of the blinds, this is used to determine what the game is doing at any given time

static int blind_reward = 0;
static int hand_reward = 0;
static int interest_reward = 0;

// Red deck default (can later be moved to a deck.h file or something)
static int max_hands = 4;
static int max_discards = 4;
// Set in game_init and game_round_init
static int hands = 0;
static int discards = 0;

static int round = 0;
static int ante = 0;
static int money = 0;
static int score = 0;
static int temp_score = 0; // This is the score that shows in the same spot as the hand type.
static FIXED lerped_score = 0;
static FIXED lerped_temp_score = 0;

static int chips = 0;
static int mult = 0;

static int hand_size = 8; // Default hand size is 8
static int cards_drawn = 0;
static int hand_selections = 0;

static int selection_x = 0;
static int selection_y = 0;

static bool sort_by_suit = false;

static List _all_jokers_list;
static List _discarded_jokers_list;
static List _avail_shop_jokers_list; // List of joker IDs
static List _shop_jokers_list;

// Stacks
static CardObject *played[MAX_SELECTION_SIZE] = {NULL};
static int played_top = -1;

static CardObject *hand[MAX_HAND_SIZE] = {NULL};
static int hand_top = -1;

static Card *deck[MAX_DECK_SIZE] = {NULL};
static int deck_top = -1;

static Card *discard_pile[MAX_DECK_SIZE] = {NULL};
static int discard_top = -1;

// Played stack
static inline void played_push(CardObject *card_object)
{
    if (played_top >= MAX_SELECTION_SIZE - 1) return;
    played[++played_top] = card_object;
}

static inline CardObject *played_pop()
{
    if (played_top < 0) return NULL;
    return played[played_top--];
}

// Deck stack
static inline void deck_push(Card *card)
{
    if (deck_top >= MAX_DECK_SIZE - 1) return;
    deck[++deck_top] = card;
}

static inline Card *deck_pop()
{
    if (deck_top < 0) return NULL;
    return deck[deck_top--];
}

// Discard stack
static inline void discard_push(Card *card)
{
    if (discard_top >= MAX_DECK_SIZE - 1) return;
    discard_pile[++discard_top] = card;
}

static inline Card *discard_pop()
{
    if (discard_top < 0) return NULL;
    return discard_pile[discard_top--];
}

// Resets bottom row bg tiles of the top left panel (shop/blind) after
// it is dismissed to match the rest of the game panel background.
static inline void reset_top_left_panel_bottom_row()
{
    int y = 6;

    memset16(&se_mem[MAIN_BG_SBB][32 * (y - 1)], 0x0006, 1);
    memset16(&se_mem[MAIN_BG_SBB][1 + 32 * (y - 1)], 0x0007, 2);
    memset16(&se_mem[MAIN_BG_SBB][3 + 32 * (y - 1)], 0x0008, 1);
    memset16(&se_mem[MAIN_BG_SBB][4 + 32 * (y - 1)], 0x0009, 3);
    memset16(&se_mem[MAIN_BG_SBB][7 + 32 * (y - 1)], 0x000A, 1);
    memset16(&se_mem[MAIN_BG_SBB][8 + 32 * (y - 1)], 0x0406, 1);
}

// get-functions, for other files to view game state (mainly for jokers)
CardObject **get_hand_array(void) {
    return hand;
}

int get_hand_top(void) {
    return hand_top;
}

int hand_get_size(void)
{
    return hand_top + 1;
}

CardObject **get_played_array(void) {
    return played;
}

int get_played_top(void) {
    return played_top;
}

List* get_jokers_list(void) {
    return &_all_jokers_list;
}

// is it possible to... directly map a jokers ID to the offset?
bool is_joker_owned(int joker_id) {
    ListItr itr = list_itr_new(&_all_jokers_list);
    ListNode* ln;

    while((ln = list_itr_next(&itr)))
    {
        JokerObject *joker = POOL_AT(JokerObject, list_get_at_idx(_all_jokers_list, ln->elem_idx));
        if (joker->joker->id == joker_id)
        {
            return true;
        }
    }
    return false;
}

void add_joker(JokerObject *joker_object)
{
    list_push_back(&_all_jokers_list, POOL_IDX(JokerObject, joker_object));
}

int get_deck_top(void)
{
    return deck_top;
}

int get_num_discards_remaining(void)
{
    return discards;
}

int get_num_hands_remaining(void)
{
    return hands;
}
int get_game_speed(void)
{
    return game_speed;
}

// for the future when a menu actually lets this variable be changed.
void set_game_speed(int new_game_speed)
{
    game_speed = new_game_speed;
}

int get_money(void)
{
    return money;
}

// Consts

// Rects                                       left     top     right   bottom
// Screenblock rects
static const Rect ROUND_END_MENU_RECT       = {9,       7,      24,     20 }; 

static const Rect POP_MENU_ANIM_RECT        = {9,       7,      24,     31 };
// The rect for popping menu animations (round end, shop, blinds) 
// - extends beyond the visible screen to the end of the screenblock
// It includes both the target and source position rects. 
// This is because when popping, the target position is blank so we just animate 
// the whole rect so we don't have to track its position

static const Rect SINGLE_BLIND_SELECT_RECT  = {9,       7,      13,     32 };

static const Rect HAND_BG_RECT_SELECTING    = {9,       11,     24,     17 };
// TODO: Currently unused, remove?
//static const Rect HAND_BG_RECT_PLAYING      = {9,       14,     24,     18 };

static const Rect TOP_LEFT_ITEM_SRC_RECT    = {0,       20,     8,      25 };
static const BG_POINT TOP_LEFT_PANEL_POINT  = {0,       0, };
static const Rect TOP_LEFT_PANEL_ANIM_RECT  = {0,       0,      8,      4  };
/* Contains the shop icon/current blind etc. 
 * The difference between TOP_LEFT_PANEL_ANIM_RECT and TOP_LEFT_PANEL_RECT 
 * is due to an overlap between the bottom of the top left panel
 * and the top of the score panel in the tiles connecting them.
 * TOP_LEFT_PANEL_ANIM_RECT should be used for animations, 
 * TOP_LEFT_PANEL_RECT for copies etc. but mind the overlap
 */
static const BG_POINT TOP_LEFT_BLIND_TITLE_POINT = {0,  21, };
static const Rect BIG_BLIND_TITLE_SRC_RECT  = {0,       26,     8,      26 };
static const Rect BOSS_BLIND_TITLE_SRC_RECT = {0,       27,     8,      27 };
static const BG_POINT GAME_OVER_SRC_RECT_3X3_POS = {25, 29};
static const Rect GAME_OVER_DIALOG_DEST_RECT= {11,      21,      23,     26};
static const Rect GAME_OVER_ANIM_RECT       = {11,      8,       23,     26};

// Rects for TTE (in pixels)
static const Rect HAND_SIZE_RECT            = {128,     128,    152,    160 }; // Seems to include both SELECT and PLAYING
static const Rect HAND_SIZE_RECT_SELECT     = {128,     128,    152,    136 };
static const Rect HAND_SIZE_RECT_PLAYING    = {128,     152,    152,    160 };
static const Rect HAND_TYPE_RECT            = {8,       64,     64,     72  };
// Score displayed in the same place as the hand type
static const Rect TEMP_SCORE_RECT           = {8,       64,     64,     72  }; 
static const Rect SCORE_RECT                = {32,      48,     64,     56  };

static const Rect PLAYED_CARDS_SCORES_RECT  = {72,      48,     240,    56  };
static const Rect BLIND_TOKEN_TEXT_RECT     = {80,      72,     200,    160 };
static const Rect MONEY_TEXT_RECT           = {8,       120,    64,     128 };
static const Rect CHIPS_TEXT_RECT           = {8,       80,     32,     88  };
static const Rect MULT_TEXT_RECT            = {40,      80,     64,     88  };
static const Rect BLIND_REWARD_RECT         = {40,      32,     64,     40  };
static const Rect BLIND_REQ_TEXT_RECT       = {32,      24,     64,     32  };
static const Rect SHOP_PRICES_TEXT_RECT     = {72,      56,     192,    160 };

// Rects with UNDEFINED are only used in tte_printf, they need to be fully defined
// to be used with tte_erase_rect_wrapper()
static const Rect HANDS_TEXT_RECT           = {16,      104,    UNDEFINED, UNDEFINED };
static const Rect DISCARDS_TEXT_RECT        = {48,      104,    UNDEFINED, UNDEFINED };
static const Rect DECK_SIZE_RECT            = {200,     152,    UNDEFINED, UNDEFINED };
static const Rect ROUND_TEXT_RECT           = {48,      144,    UNDEFINED, UNDEFINED };
static const Rect ANTE_TEXT_RECT            = {8,       144,    UNDEFINED, UNDEFINED };
static const Rect ROUND_END_BLIND_REQ_RECT  = {104,     96,     136,       UNDEFINED };
static const Rect ROUND_END_BLIND_REWARD_RECT = { 168,  96,     UNDEFINED, UNDEFINED };
static const Rect ROUND_END_NUM_HANDS_RECT  = {88,      116,    UNDEFINED, UNDEFINED };
static const Rect HAND_REWARD_RECT          = {168,     UNDEFINED, UNDEFINED, UNDEFINED };
static const Rect CASHOUT_RECT              = {88,      72,     UNDEFINED, UNDEFINED };
static const Rect SHOP_REROLL_RECT          = {88,      96,     UNDEFINED, UNDEFINED };
static const Rect GAME_LOSE_MSG_TEXT_RECT   = {104,     72,     UNDEFINED, UNDEFINED};
// 1 character to the right oF GAME_LOSE
static const Rect GAME_WIN_MSG_TEXT_RECT    = {112,      72,     UNDEFINED, UNDEFINED};

static const BG_POINT HELD_JOKERS_POS       = {108,     10};
static const BG_POINT JOKER_DISCARD_TARGET  = {240,     30};
static const BG_POINT CARD_DRAW_POS         = {208,     110};
static const BG_POINT CUR_BLIND_TOKEN_POS   = {8,       18};
static const BG_POINT CARD_DISCARD_PNT      = {240,     70};
static const BG_POINT HAND_START_POS        = {120,     90};
static const BG_POINT MAIN_MENU_ACE_T       = {88,      26};

#define ITEM_SHOP_Y 71 // TODO: Needs to be a rect?

#define MAIN_MENU_BUTTONS 2
#define MAIN_MENU_IMPLEMENTED_BUTTONS 1 // Remove this once all buttons are implemented

//TODO: Properly define and use
#define MENU_POP_OUT_ANIM_FRAMES 20
#define GAME_OVER_ANIM_FRAMES 15

#define SCORED_CARD_TEXT_Y 48

#define HIGHLIGHT_COLOR 0xFFFF
#define SHOP_LIGHTS_1_CLR 0xFFFF
#define SHOP_LIGHTS_2_CLR 0x32BE
#define SHOP_LIGHTS_3_CLR 0x4B5F
#define SHOP_LIGHTS_4_CLR 0x5F9F

#define PITCH_STEP_DISCARD_SFX      (-64)
#define PITCH_STEP_DRAW_SFX         24
#define PITCH_STEP_UNDISCARD_SFX    2*PITCH_STEP_DRAW_SFX    

#define STARTING_ROUND 0
#define STARTING_ANTE 1
#define STARTING_MONEY 4
#define STARTING_SCORE 0

#define TEN_K 10000
#define ONE_K 1000

#define CARD_FOCUSED_UNSEL_Y 10
#define CARD_UNFOCUSED_SEL_Y 15
#define CARD_FOCUSED_SEL_Y 20

// Timer defs
#define TM_ZERO 0
#define TM_RESET_STATIC_VARS 30
#define TM_END_POP_MENU_ANIM 13
#define TM_START_ROUND_END_MENU_AMIN 1
#define TM_END_DISPLAY_FIN_BLIND 30
#define TM_END_DISPLAY_SCORE_MIN 4
#define TM_ELLIPSIS_PRINT_MAX_TM 16
#define TM_DISPLAY_REWARDS_CONT_WAIT 30
#define TM_HAND_REWARD_INCR_WAIT 45
#define TM_DISMISS_ROUND_END_TM 20
#define TM_CREATE_SHOP_ITEMS_WAIT 1
#define TM_SHIFT_SHOP_ICON_WAIT 7
#define TM_END_GAME_SHOP_INTRO 12
#define TM_SHOP_PRC_INPUT_START 1
#define TM_DISP_BLIND_PANEL_FINISH 7
#define TM_DISP_BLIND_PANEL_START 1
#define TM_BLIND_SELECT_START 1
#define TM_END_ANIM_SEQ 12

// Palette IDs
#define PLAY_HAND_BTN_SELECTED_BORDER_PID 1
#define BOSS_BLIND_PRIMARY_PID 1
#define BLIND_BG_SHADOW_PID 2
#define MAIN_MENU_PLAY_BUTTON_OUTLINE_PID 2
#define REROLL_BTN_PID 3
#define BLIND_BG_SECONDARY_PID 5
#define BLIND_SKIP_BTN_PID 5 
#define MAIN_MENU_PLAY_BUTTON_MAIN_COLOR_PID 5
#define NEXT_ROUND_BTN_SELECTED_BORDER_PID 5
#define SHOP_PANEL_SHADOW_PID 6
#define BOSS_BLIND_SHADOW_PID 7
#define PLAY_HAND_BTN_PID 7
#define REROLL_BTN_SELECTED_BORDER_PID 7
#define SHOP_LIGHTS_1_PID 8
#define DISCARD_BTN_SELECTED_BORDER_PID 9
#define BLIND_SKIP_BTN_SELECTED_BORDER_PID 10
#define DISCARD_BTN_PID 12 
#define SHOP_LIGHTS_2_PID 14
#define BLIND_SELECT_BTN_PID 15
#define NEXT_ROUND_BTN_PID 16 
#define SHOP_LIGHTS_3_PID 17
#define BLIND_SELECT_BTN_SELECTED_BORDER_PID 18
#define BLIND_BG_PRIMARY_PID 19
#define REWARD_PANEL_BORDER_PID 19
#define SHOP_LIGHTS_4_PID 22
#define SHOP_BOTTOM_PANEL_BORDER_PID 26


// Naming the stage where cards return from the discard pile to the deck "undiscard"

// General functions
void set_seed(int seed)
{
    rng_seed = seed;
    srand(rng_seed);
}

void sort_hand_by_suit()
{
    for (int a = 0; a < hand_top; a++)
    {
        for (int b = a + 1; b <= hand_top; b++)
        {
            if (hand[a] == NULL || (hand[b] != NULL && (hand[a]->card->suit > hand[b]->card->suit || (hand[a]->card->suit == hand[b]->card->suit && hand[a]->card->rank > hand[b]->card->rank))))
            {
                CardObject* temp = hand[a];
                hand[a] = hand[b];
                hand[b] = temp;
            }
        }
    }
}

void sort_hand_by_rank()
{
    for (int a = 0; a < hand_top; a++)
    {
        for (int b = a + 1; b <= hand_top; b++)
        {
            if (hand[a] == NULL || (hand[b] != NULL && hand[a]->card->rank > hand[b]->card->rank))
            {
                CardObject* temp = hand[a];
                hand[a] = hand[b];
                hand[b] = temp;
            }
        }
    }
}

void sort_cards()
{
    if (sort_by_suit)
    {
        sort_hand_by_suit();
    }
    else
    {
        sort_hand_by_rank();
    }

    // Update the sprites in the hand by destroying them and creating new ones in the correct order
    // (This is feels like a diabolical solution but like literally how else would you do this)
    for (int i = 0; i <= hand_top; i++)
    {
        if (hand[i] != NULL)
        {
            // card_object_get_sprite() will not work here since we need the address
            sprite_destroy(&(hand[i]->sprite_object->sprite));
        }
    }

    for (int i = 0; i <= hand_top; i++)
    {
        if (hand[i] != NULL)
        {
            //hand[i]->sprite = sprite_new(ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF, ATTR1_SIZE_32, card_sprite_lut[hand[i]->card->suit][hand[i]->card->rank], 0, i);
            card_object_set_sprite(hand[i], i); // Set the sprite for the card object
            sprite_position(card_object_get_sprite(hand[i]), fx2int(hand[i]->sprite_object->x), fx2int(hand[i]->sprite_object->y));
        }
    }
}

enum HandType hand_get_type()
{
    enum HandType res_hand_type = NONE;

    // Idk if this is how Balatro does it but this is how I'm doing it
    if (hand_selections == 0 || hand_state == HAND_DISCARD)
    {
        res_hand_type = NONE;
        return res_hand_type;
    }

    res_hand_type = HIGH_CARD;

    u8 suits[NUM_SUITS];
    u8 ranks[NUM_RANKS];
    get_hand_distribution(ranks, suits);

    // Check for flush
    if (hand_contains_flush(suits))
        res_hand_type = FLUSH;

    // Check for straight
    if (hand_contains_straight(ranks)) {
        if (res_hand_type == FLUSH)
            res_hand_type = STRAIGHT_FLUSH;
        else
            res_hand_type = STRAIGHT;
    }

    // Check for royal flush vs regular straight flush
    if (res_hand_type == STRAIGHT_FLUSH) {
        if (ranks[TEN] && ranks[JACK] && ranks[QUEEN] && ranks[KING] && ranks[ACE])
            return ROYAL_FLUSH;
        return STRAIGHT_FLUSH;
    }

    // The following can be optimized better but not sure how much it matters
    u8 n_of_a_kind = hand_contains_n_of_a_kind(ranks);

    if (n_of_a_kind >= 5) {
        if (res_hand_type == FLUSH) {
            return FLUSH_FIVE;
        }
        return FIVE_OF_A_KIND;
    }

    if (n_of_a_kind == 4) {
        return FOUR_OF_A_KIND;
    }

    if (n_of_a_kind == 3 && hand_contains_full_house(ranks)) {
        return FULL_HOUSE;
    }

    // Flush is more valuable than the remaining hand types, so return now
    if (res_hand_type == FLUSH) {
        return FLUSH;
    }

    if (n_of_a_kind == 3) {
        return THREE_OF_A_KIND;
    }

    if (n_of_a_kind == 2) {
        if (hand_contains_two_pair(ranks)) {
            return TWO_PAIR;
        }
        return PAIR;
    }

    return res_hand_type; // should be HIGH_CARD
}

// Returns true if the card is *considered* a face card
bool card_is_face(Card *card) {
    // Card is a face card, or Pareidolia is present
    return (
        card->rank == JACK  ||
        card->rank == QUEEN ||
        card->rank == KING  ||
        is_joker_owned(PAREIDOLIA_JOKER_ID)
    );
}

/* Copies the appropriate item into the top left panel (blind/shop icon)
 * from where it was put outside the screenview
 */
void bg_copy_current_item_to_top_left_panel()
{
    main_bg_se_copy_rect(TOP_LEFT_ITEM_SRC_RECT, TOP_LEFT_PANEL_POINT);
}

void change_background(int id)
{
    if (background == id)
    {
        return;
    }
    else if (id == BG_ID_CARD_SELECTING)
    {
        tte_erase_rect_wrapper(HAND_SIZE_RECT_PLAYING);
        REG_WIN0V = (REG_WIN0V << 8) | 0x80; // Set window 0 top to 128

        if (background == BG_ID_CARD_PLAYING)
        {
            int offset = 11;
            memcpy16(&se_mem[MAIN_BG_SBB][SE_ROW_LEN * offset], &background_gfxMap[SE_ROW_LEN * offset], SE_ROW_LEN * 8);
        }
        else
        {
            toggle_windows(true, true); // Enable window 0 for the hand shadow
            
            // Load the tiles and palette
            // Background
            GRIT_CPY(pal_bg_mem, background_gfxPal);
            GRIT_CPY(&tile8_mem[MAIN_BG_CBB], background_gfxTiles); 
            GRIT_CPY(&se_mem[MAIN_BG_SBB], background_gfxMap);

            if (current_blind == BLIND_TYPE_BIG) // Change text and palette depending on blind type
            {
                main_bg_se_copy_rect(BIG_BLIND_TITLE_SRC_RECT, TOP_LEFT_BLIND_TITLE_POINT);
            }
            else if (current_blind == BLIND_TYPE_BOSS)
            {
                main_bg_se_copy_rect(BOSS_BLIND_TITLE_SRC_RECT, TOP_LEFT_BLIND_TITLE_POINT);

                affine_background_set_color(blind_get_color(BLIND_TYPE_BOSS, BLIND_SHADOW_COLOR_INDEX));
            }

            bg_copy_current_item_to_top_left_panel();

            // This would change the palette of the background to match the blind, but the backgroun doesn't use the blind token's exact colors so a different approach is required
            memset16(&pal_bg_mem[BLIND_BG_PRIMARY_PID], blind_get_color(current_blind, BLIND_BACKGROUND_MAIN_COLOR_INDEX), 1);
            memset16(&pal_bg_mem[BLIND_BG_SECONDARY_PID], blind_get_color(current_blind, BLIND_BACKGROUND_SECONDARY_COLOR_INDEX), 1);
            memset16(&pal_bg_mem[BLIND_BG_SHADOW_PID], blind_get_color(current_blind, BLIND_BACKGROUND_SHADOW_COLOR_INDEX), 1);

            // Copy the Play Hand and Discard button colors to their selection highlights
            memcpy16(&pal_bg_mem[PLAY_HAND_BTN_SELECTED_BORDER_PID], &pal_bg_mem[PLAY_HAND_BTN_PID], 1);
            memcpy16(&pal_bg_mem[DISCARD_BTN_SELECTED_BORDER_PID], &pal_bg_mem[DISCARD_BTN_PID], 1);
        }
    }
    else if (id == BG_ID_CARD_PLAYING)
    {
        if (background != BG_ID_CARD_SELECTING)
        {
            change_background(BG_ID_CARD_SELECTING);
            background = BG_ID_CARD_PLAYING;
        }

        REG_WIN0V = (REG_WIN0V << 8) | 0xA0; // Set window 0 bottom to 160
        toggle_windows(true, true);

        for (int i = 0; i <= 2; i++)
        {
            main_bg_se_move_rect_1_tile_vert(HAND_BG_RECT_SELECTING, SE_DOWN);
        }

        tte_erase_rect_wrapper(HAND_SIZE_RECT_SELECT);
    }
    else if (id == BG_ID_ROUND_END)
    {
        if (background != BG_ID_CARD_SELECTING && background != BG_ID_CARD_PLAYING)
        {
            change_background(BG_ID_CARD_SELECTING);
            background = BG_ID_ROUND_END;
        }

       toggle_windows(false, true); // Disable window 0 so it doesn't make the cashout menu transparent

        main_bg_se_clear_rect(ROUND_END_MENU_RECT);
        tte_erase_rect_wrapper(HAND_SIZE_RECT);
    }
    else if (id == BG_ID_SHOP)
    {
        toggle_windows(false, true);

        GRIT_CPY(pal_bg_mem, background_shop_gfxPal);
        GRIT_CPY(&tile_mem[MAIN_BG_CBB], background_shop_gfxTiles);
        GRIT_CPY(&se_mem[MAIN_BG_SBB], background_shop_gfxMap);

        // Set the outline colors for the shop background. This is used for the alternate shop palettes when opening packs
        memset16(&pal_bg_mem[SHOP_BOTTOM_PANEL_BORDER_PID], 0x213D, 1);
        memset16(&pal_bg_mem[SHOP_PANEL_SHADOW_PID], 0x10B4, 1);
        
        memset16(&pal_bg_mem[SHOP_LIGHTS_2_PID], SHOP_LIGHTS_2_CLR, 1); // Reset the shop lights to correct colors
        memset16(&pal_bg_mem[SHOP_LIGHTS_3_PID], SHOP_LIGHTS_3_CLR, 1);
        memset16(&pal_bg_mem[SHOP_LIGHTS_4_PID], SHOP_LIGHTS_4_CLR, 1);
        memset16(&pal_bg_mem[SHOP_LIGHTS_1_PID], SHOP_LIGHTS_1_CLR, 1);

        memcpy16(&pal_bg_mem[REROLL_BTN_SELECTED_BORDER_PID], &pal_bg_mem[REROLL_BTN_PID], 1); // Disable the button highlight colors
        memcpy16(&pal_bg_mem[NEXT_ROUND_BTN_SELECTED_BORDER_PID], &pal_bg_mem[NEXT_ROUND_BTN_PID], 1); 
    }
    else if (id == BG_ID_BLIND_SELECT)
    {
        for(int i = 0; i < BLIND_TYPE_MAX; i++)
        {
            obj_unhide(blind_select_tokens[i]->obj, 0);
        }

        const int default_y = 89 + (TILE_SIZE * 12); // Default y position for the blind select tokens. 12 is the amound of tiles the background is shifted down by
        // TODO refactor magic numbers '80/120/160' into a map to loop with
        sprite_position(blind_select_tokens[BLIND_TYPE_SMALL], 80, default_y);
        sprite_position(blind_select_tokens[BLIND_TYPE_BIG], 120, default_y);
        sprite_position(blind_select_tokens[BLIND_TYPE_BOSS], 160, default_y);

        toggle_windows(false, true);

        GRIT_CPY(pal_bg_mem, background_blind_select_gfxPal);
        GRIT_CPY(&tile_mem[MAIN_BG_CBB], background_blind_select_gfxTiles);
        GRIT_CPY(&se_mem[MAIN_BG_SBB], background_blind_select_gfxMap);

        // Copy boss blind colors to blind select palette
        memset16(&pal_bg_mem[1], blind_get_color(BLIND_TYPE_BOSS, BLIND_BACKGROUND_MAIN_COLOR_INDEX), 1);
        memset16(&pal_bg_mem[7], blind_get_color(BLIND_TYPE_BOSS, BLIND_BACKGROUND_SHADOW_COLOR_INDEX), 1);

        // Disable the button highlight colors
        // Select button PID is 15 and the outline is 18
        memcpy16(&pal_bg_mem[BLIND_SELECT_BTN_SELECTED_BORDER_PID], &pal_bg_mem[BLIND_SELECT_BTN_PID], 1);
		// It seems the skip button (and score multiplier and deck) PB idx is
		// actually 5, not 10. 10 is the selected border color
		// Setting this palette value though doesn't seem to have an 
		// effect.
        memcpy16(&pal_bg_mem[BLIND_SKIP_BTN_SELECTED_BORDER_PID], &pal_bg_mem[BLIND_SKIP_BTN_PID], 1);

        for (int i = 0; i < BLIND_TYPE_MAX; i++)
        {
            if (blinds[i] != BLIND_STATE_CURRENT &&
                (i == BLIND_TYPE_SMALL || i == BLIND_TYPE_BIG)) // Make the skip button gray
            {
                // TODO: Switch all the copies here to use main_bg_se_copy_rect()
                // Note, this is difficult as the tiles to copy are not aligned.
                int x_from = 0;
                int y_from = 24 + (i * 4);

                int x_to = 9 + (i * 5);
                int y_to = 29;

                for (int j = 0; j < BLIND_TYPE_MAX; j++)
                {
                    memcpy16(&se_mem[MAIN_BG_SBB][x_to + 32 * y_to], &se_mem[MAIN_BG_SBB][x_from + 32 * y_from], 5);
                    y_from++;
                    y_to++;
                }
            }

            switch(blinds[i]) {
                case BLIND_STATE_CURRENT: // Raise the blind panel up a bit
                {
                    int x_from = 0;
                    int y_from = 27;

                    Rect blind_rect = SINGLE_BLIND_SELECT_RECT;

                    // There's no gap between them
                    blind_rect.left += i * rect_width(&SINGLE_BLIND_SELECT_RECT);
                    blind_rect.right += i * rect_width(&SINGLE_BLIND_SELECT_RECT);
                    main_bg_se_copy_rect_1_tile_vert(blind_rect, SE_UP);

                    int x_to = blind_rect.left;
                    int y_to = 31;

                    if (i == BLIND_TYPE_BIG)
                    {
                        y_from = 31;
                    }
                    else if (i == BLIND_TYPE_BOSS)
                    {
                        x_from = x_to;
                        y_from = 30;
                    }

                    // Copy plain tiles onto the bottom of the raised blind panel to fill the gap created by the raise
                    Rect gap_fill_rect = {x_from, y_from, x_from + rect_width(&SINGLE_BLIND_SELECT_RECT) - 1, y_from}; // - 1 to stay within rect boundaries
                    BG_POINT gap_fill_point = {x_to, y_to};
                    main_bg_se_copy_rect(gap_fill_rect, gap_fill_point);

                    sprite_position(blind_select_tokens[i], blind_select_tokens[i]->pos.x, blind_select_tokens[i]->pos.y - TILE_SIZE); // Move token up by a tile
                    break;
                }
                case BLIND_STATE_UPCOMING: // Change the select icon to "NEXT" 
                {
                    int x_from = 0;
                    int y_from = 20;

                    int x_to = 10 + (i * 5);
                    int y_to = 20;

                    memcpy16(&se_mem[MAIN_BG_SBB][x_to + 32 * y_to], &se_mem[MAIN_BG_SBB][x_from + 32 * y_from], 3);
                    break;
                }
                case BLIND_STATE_SKIPPED: // Change the select icon to "SKIP"
                {
                    int x_from = 3;
                    int y_from = 20;

                    int x_to = 10 + (i * 5);
                    int y_to = 20;

                    memcpy16(&se_mem[MAIN_BG_SBB][x_to + 32 * y_to], &se_mem[MAIN_BG_SBB][x_from + 32 * y_from], 3);
                    break;
                }
                case BLIND_STATE_DEFEATED: // Change the select icon to "DEFEATED"
                {
                    int x_from = 6;
                    int y_from = 20;

                    int x_to = 10 + (i * 5);
                    int y_to = 20;

                    memcpy16(&se_mem[MAIN_BG_SBB][x_to + 32 * y_to], &se_mem[MAIN_BG_SBB][x_from + 32 * y_from], 3);
                    break;
                }
                default:
                    break;
            }
        }
    }
    else if (id == BG_ID_MAIN_MENU)
    {
        toggle_windows(false, false);

        tte_erase_screen();
        GRIT_CPY(pal_bg_mem, background_main_menu_gfxPal);
        GRIT_CPY(&tile_mem[MAIN_BG_CBB], background_main_menu_gfxTiles);
        GRIT_CPY(&se_mem[MAIN_BG_SBB], background_main_menu_gfxMap);

        // Disable the button highlight colors
        memcpy16(&pal_bg_mem[MAIN_MENU_PLAY_BUTTON_OUTLINE_PID], &pal_bg_mem[MAIN_MENU_PLAY_BUTTON_MAIN_COLOR_PID], 1);
    }
    else
    {
        return; // Invalid background ID
    }

    background = id;
}

void display_temp_score(int value)
{
    int x_offset = 40 - get_digits_even(value) * TILE_SIZE;
    tte_erase_rect_wrapper(TEMP_SCORE_RECT);
    tte_printf("#{P:%d,%d; cx:0x%X000}%d", x_offset, TEMP_SCORE_RECT.top, TTE_WHITE_PB, value);
}

void display_score(int value)
{
    // Clear the existing text before redrawing
    tte_erase_rect_wrapper(SCORE_RECT);
    
    char score_suffix = ' ';
    int display_value = value;
    
    if(value >= TEN_K)
    {
        score_suffix = 'k';
        display_value = value / ONE_K; // 12,986 = 12k
    }
    
    // Calculate text width: digits + suffix character (if 'k')
    int num_digits = get_digits(display_value);
    int text_width = num_digits * TILE_SIZE;
    if(score_suffix == 'k')
    {
        text_width += TILE_SIZE; // Add width for 'k' suffix
    }
    
    // Calculate center position within SCORE_RECT
    int rect_width = SCORE_RECT.right - SCORE_RECT.left;
    int x_offset = SCORE_RECT.left + (rect_width - text_width) / 2;
    
    tte_printf("#{P:%d,48; cx:0x%X000}%d%c", x_offset, TTE_WHITE_PB, display_value, score_suffix);
}

void display_money(int value)
{
    int x_offset = 32 - get_digits_odd(value) * TILE_SIZE;
    tte_erase_rect_wrapper(MONEY_TEXT_RECT);
    tte_printf("#{P:%d,%d; cx:0x%X000}$%d", x_offset, MONEY_TEXT_RECT.top, TTE_YELLOW_PB, value);
}

void display_chips(int value)
{
    Rect chips_text_rect = CHIPS_TEXT_RECT;
    tte_erase_rect_wrapper(CHIPS_TEXT_RECT);
    update_text_rect_to_right_align_num(&chips_text_rect, value, OVERFLOW_LEFT);
    tte_printf("#{P:%d,%d; cx:0x%X000;}%d", chips_text_rect.left, chips_text_rect.top, TTE_WHITE_PB, value);
}

void display_mult(int value)
{
    tte_erase_rect_wrapper(MULT_TEXT_RECT);
    tte_printf("#{P:%d,%d; cx:0x%X000;}%d", MULT_TEXT_RECT.left, MULT_TEXT_RECT.top, TTE_WHITE_PB, value); // Mult
}

void display_round(int value)
{
    //tte_erase_rect_wrapper(ROUND_TEXT_RECT);
    tte_printf("#{P:%d,%d; cx:0x%X000}%d", ROUND_TEXT_RECT.left, ROUND_TEXT_RECT.top, TTE_YELLOW_PB, round);
}

void display_ante(int value)
{
    tte_printf("#{P:%d,%d; cx:0xC000}%d#{cx:0xF000}/%d", ANTE_TEXT_RECT.left, ANTE_TEXT_RECT.top, value, MAX_ANTE);
}

void display_hands(int value)
{
    //tte_erase_rect_wrapper(HANDS_TEXT_RECT);
    tte_printf("#{P:%d,%d; cx:0xD000}%d", HANDS_TEXT_RECT.left, HANDS_TEXT_RECT.top, hands); // Hand
}

void display_discards(int value)
{
    //tte_erase_rect_wrapper(DISCARDS_TEXT_RECT);
    tte_printf("#{P:%d,%d; cx:0xE000}%d", DISCARDS_TEXT_RECT.left, DISCARDS_TEXT_RECT.top, discards); // Discard
}

static void print_hand_type(const char* hand_type_str)
{
    if (hand_type_str == NULL)
        return; // NULL-checking paranoia
    tte_printf("#{P:%d,%d; cx:0x%X000}%s", HAND_TYPE_RECT.left, HAND_TYPE_RECT.top, TTE_WHITE_PB, hand_type_str);
}

void set_hand()
{
    tte_erase_rect_wrapper(HAND_TYPE_RECT);
    hand_type = hand_get_type();

    HandValues hand = hand_base_values[hand_type];

    chips = hand.chips;
    mult = hand.mult;

    print_hand_type(hand.display_name);
    display_chips(chips);
    display_mult(mult);
}

void card_draw()
{
    if (deck_top < 0 || hand_top >= hand_size - 1 || hand_top >= MAX_HAND_SIZE - 1) return;

    CardObject *card_object = card_object_new(deck_pop());

    const FIXED deck_x = int2fx(CARD_DRAW_POS.x);
    const FIXED deck_y = int2fx(CARD_DRAW_POS.y);

    card_object->sprite_object->x = deck_x;
    card_object->sprite_object->y = deck_y;

    hand[++hand_top] = card_object;

    // Sort the hand after drawing a card
    sort_cards();

    play_sfx(SFX_CARD_DRAW, MM_BASE_PITCH_RATE + cards_drawn*PITCH_STEP_DRAW_SFX);
}

void hand_set_focus(int index)
{
    if (hand_state != HAND_SELECT) return;

    // Wrap around to the other side of the hand when going out of bounds on either side
    if (index < 0)
    {
        selection_x = hand_top;
    }
    else if (index > hand_top)
    {
        selection_x = 0;
    }
    else
    {
        selection_x = index;
    }

    play_sfx(SFX_CARD_FOCUS, MM_BASE_PITCH_RATE + rand() % 512);
}

void hand_toggle_card_selection()
{
    if (hand_state != HAND_SELECT || hand[selection_x] == NULL) return;

    if (card_object_is_selected(hand[selection_x]))
    {
        card_object_set_selected(hand[selection_x], false);
        hand_selections--;
        play_sfx(SFX_CARD_DESELECT, MM_BASE_PITCH_RATE);
    }
    else if (hand_selections < MAX_SELECTION_SIZE)
    {
        card_object_set_selected(hand[selection_x], true);
        hand_selections++;
        play_sfx(SFX_CARD_SELECT, MM_BASE_PITCH_RATE);
    }
}

void hand_deselect_all_cards()
{
    bool any_cards_deselected = false;
    for (int i = 0; i <= get_hand_top(); i++)
    {
        if (card_object_is_selected(hand[i]))
        {
            card_object_set_selected(hand[i], false);
            hand_selections--;
            any_cards_deselected = true;
        }
    }

    if (any_cards_deselected)
    {
        play_sfx(SFX_CARD_DESELECT, MM_BASE_PITCH_RATE);
    }
}

void hand_change_sort()
{
    sort_by_suit = !sort_by_suit;
    sort_cards();
}

int hand_get_max_size()
{
    return hand_size;
}

bool hand_discard()
{
    if (hand_state != HAND_SELECT || hand_selections == 0) return false;
    return true;
}

bool hand_play()
{
    if (hand_state != HAND_SELECT || hand_selections == 0) return false;
    return true;
}

int deck_get_size()
{
    return deck_top + 1;
}

int deck_get_max_size()
{
    return hand_top + played_top + deck_top + discard_top + 4; // This is the max amount of cards that the player currently has in their possession
}

void deck_shuffle()
{
    for (int i = deck_top; i > 0; i--) 
    {
        int j = rand() % (i + 1);
        Card *temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

void increment_blind(enum BlindState increment_reason)
{
    current_blind++;
    if (current_blind >= BLIND_TYPE_MAX)
    {
        current_blind = 0;
        blinds[0] = BLIND_STATE_CURRENT; // Reset the blinds to the first one
        blinds[1] = BLIND_STATE_UPCOMING; // Set the next blind to upcoming
        blinds[2] = BLIND_STATE_UPCOMING; // Set the next blind to upcoming
    }
    else
    {
        blinds[current_blind] = BLIND_STATE_CURRENT;
        blinds[current_blind - 1] = increment_reason;
    }
}

static void game_round_on_init()
{
    hand_state = HAND_DRAW;
    cards_drawn = 0;
    hand_selections = 0;

    playing_blind_token = blind_token_new(current_blind, CUR_BLIND_TOKEN_POS.x, CUR_BLIND_TOKEN_POS.y, MAX_SELECTION_SIZE + MAX_HAND_SIZE + 1); // Create the blind token sprite at the top left corner
    // TODO: Hide blind token and display it after sliding blind rect animation
    //if (playing_blind_token != NULL)
    //{
    //    obj_hide(playing_blind_token->obj); // Hide the blind token sprite for now
    //}
    round_end_blind_token = blind_token_new(current_blind, 81, 86, MAX_SELECTION_SIZE + MAX_HAND_SIZE + 2); // Create the blind token sprite for round end

    if (round_end_blind_token != NULL)
    {
        obj_hide(round_end_blind_token->obj); // Hide the blind token sprite for now
    }

    Rect blind_req_text_rect = BLIND_REQ_TEXT_RECT;
    int blind_requirement = blind_get_requirement(current_blind, ante);
    
    // TODO: Address Copilot review at
    // https://github.com/cellos51/balatro-gba/pull/46#pullrequestreview-3045772903
    char score_suffix = ' ';
    if(blind_requirement >= TEN_K)
    {
        // clear existing text
        tte_erase_rect_wrapper(blind_req_text_rect);
        
        score_suffix = 'k';
        blind_requirement /= ONE_K; // 11,000 = 11k
    }
    
    // Update text rect for right alignment AFTER shortening the number
    update_text_rect_to_right_align_num(&blind_req_text_rect, blind_requirement, OVERFLOW_RIGHT);
    
    // If we added a suffix, adjust position to account for the extra character
    if(score_suffix == 'k')
    {
        blind_req_text_rect.left -= TILE_SIZE; // Move left by one character width to make room for 'k'
    }

    tte_printf("#{P:%d,%d; cx:0x%X000}%d%c", blind_req_text_rect.left, blind_req_text_rect.top, TTE_RED_PB, blind_requirement, score_suffix); // Blind requirement
    tte_printf("#{P:%d,%d; cx:0x%X000}$%d", BLIND_REWARD_RECT.left, BLIND_REWARD_RECT.top, TTE_YELLOW_PB, blind_get_reward(current_blind)); // Blind reward

    deck_shuffle(); // Shuffle the deck at the start of the round
}

static void game_main_menu_on_init()
{
    affine_background_change_background(AFFINE_BG_MAIN_MENU);
    change_background(BG_ID_MAIN_MENU);
    main_menu_ace = card_object_new(card_new(SPADES, ACE));
    card_object_set_sprite(main_menu_ace, 0); // Set the sprite for the ace of spades
    main_menu_ace->sprite_object->sprite->obj->attr0 |= ATTR0_AFF_DBL; // Make the sprite double sized
    main_menu_ace->sprite_object->tx = int2fx(MAIN_MENU_ACE_T.x);
    main_menu_ace->sprite_object->x = main_menu_ace->sprite_object->tx;
    main_menu_ace->sprite_object->ty = int2fx(MAIN_MENU_ACE_T.y);
    main_menu_ace->sprite_object->y = main_menu_ace->sprite_object->ty;
    main_menu_ace->sprite_object->tscale = float2fx(0.8f);
}

static void game_over_init()
{
    // Clears the round end menu
    main_bg_se_clear_rect(POP_MENU_ANIM_RECT);
    main_bg_se_copy_expand_3x3_rect(GAME_OVER_DIALOG_DEST_RECT, GAME_OVER_SRC_RECT_3X3_POS);
}

static void game_lose_on_init()
{
    game_over_init();
    // Using the text color to match the "Game Over" text
    affine_background_set_color(TEXT_CLR_RED);

}

static void game_win_on_init()
{
    game_over_init();
    // Using the text color to match the "You Win" text
    affine_background_set_color(TEXT_CLR_BLUE);
}

// Game functions
void game_change_state(enum GameState new_game_state)
{
    timer = TM_ZERO; // Reset the timer
    
    if (game_state >= 0 && game_state < GAME_STATE_MAX)
    {
        state_info[game_state].substate = 0;
        state_info[game_state].on_exit();
    }

    if (new_game_state >= 0 && new_game_state < GAME_STATE_MAX)
    {
        state_info[new_game_state].on_init();

        game_state = new_game_state;
    }
}

void jokers_available_to_shop_init()
{
    int num_defined_jokers = get_joker_registry_size();

    list_destroy(&_avail_shop_jokers_list);
    _avail_shop_jokers_list = list_new();
    
    for (intptr_t i = 0; i < num_defined_jokers; i++)
    {
        // Add all joker IDs sequentially
        list_push_back(&_avail_shop_jokers_list, i);
    }
}

void game_init()
{
    // Initialize all jokers list once
    _all_jokers_list = list_new();
    _discarded_jokers_list = list_new();
    _avail_shop_jokers_list = list_new();
    _shop_jokers_list = list_new();

    jokers_available_to_shop_init();

    hands = max_hands;
    discards = max_discards;
    timer = TM_ZERO;
    current_blind = BLIND_TYPE_SMALL;
    blinds[0] = BLIND_STATE_CURRENT;
    blinds[1] = BLIND_STATE_UPCOMING;
    blinds[2] = BLIND_STATE_UPCOMING;
    round = STARTING_ROUND; 
    ante = STARTING_ANTE;
    money = STARTING_MONEY;
    score = STARTING_SCORE;

    blind_select_tokens[BLIND_TYPE_SMALL] = blind_token_new(BLIND_TYPE_SMALL, CUR_BLIND_TOKEN_POS.x, CUR_BLIND_TOKEN_POS.y, MAX_SELECTION_SIZE + MAX_HAND_SIZE + 3);
    blind_select_tokens[BLIND_TYPE_BIG] = blind_token_new(BLIND_TYPE_BIG, CUR_BLIND_TOKEN_POS.x, CUR_BLIND_TOKEN_POS.y, MAX_SELECTION_SIZE + MAX_HAND_SIZE + 4);
    blind_select_tokens[BLIND_TYPE_BOSS] = blind_token_new(BLIND_TYPE_BOSS, CUR_BLIND_TOKEN_POS.x, CUR_BLIND_TOKEN_POS.y, MAX_SELECTION_SIZE + MAX_HAND_SIZE + 5);

    obj_hide(blind_select_tokens[BLIND_TYPE_SMALL]->obj);
    obj_hide(blind_select_tokens[BLIND_TYPE_BIG]->obj);
    obj_hide(blind_select_tokens[BLIND_TYPE_BOSS]->obj);
}

void game_start()
{
    //set_seed(rng_seed);
    set_seed(9); // 9 is a full house

    affine_background_change_background(AFFINE_BG_GAME);

    // Normally I would just cache these and hide/unhide but I didn't feel like dealing with defining a layer for it
    card_destroy(&main_menu_ace->card);
    card_object_destroy(&main_menu_ace);

    hands = max_hands;
    discards = max_discards;

    // Fill the deck with all the cards. Later on this can be replaced with a more dynamic system that allows for different decks and card types.
    for (int suit = 0; suit < NUM_SUITS; suit++)
    {
        for (int rank = 0; rank < NUM_RANKS; rank++)
        {
            Card *card = card_new(suit, rank);
            deck_push(card);
        }
    }

    change_background(BG_ID_BLIND_SELECT);

    tte_printf("#{P:%d,%d; cx:0x%X000}%d/%d", DECK_SIZE_RECT.left, DECK_SIZE_RECT.top, TTE_WHITE_PB, deck_get_size(), deck_get_max_size()); // Deck size/max size
    
    display_round(round); // Set the round display
    display_score(score); // Set the score display

    display_chips(chips); // Set the chips display
    display_mult(mult); // Set the multiplier display

    display_hands(hands); // Hand
    display_discards(discards); // Discard

    display_money(money); // Set the money display

    tte_printf("#{P:%d,%d; cx:0x%X000}%d#{cx:0x%X000}/%d", ANTE_TEXT_RECT.left, ANTE_TEXT_RECT.top, TTE_YELLOW_PB, ante, TTE_WHITE_PB, MAX_ANTE); // Ante

    game_change_state(GAME_STATE_BLIND_SELECT);
}

static void game_playing_process_hand_select_input()
{
    static bool discard_button_highlighted = false; // true = play button highlighted, false = discard button highlighted

    if (key_hit(KEY_LEFT))
    {
        if (selection_y == 0)
        {
            hand_set_focus(selection_x + 1); // The reason why this adds 1 is because the hand is drawn from right to left. There is no particular reason for this, it's just how I did it.
        }
        else
        {
            discard_button_highlighted = false; // Play button
        }
    }
    else if (key_hit(KEY_RIGHT))
    {
        if (selection_y == 0)
        {
            hand_set_focus(selection_x - 1);
        }
        else
        {
            discard_button_highlighted = true; // Discard button
        }
    }
    else if (key_hit(KEY_UP) && selection_y != 0)
    {
        selection_y = 0;
    }
    else if (key_hit(KEY_DOWN) && selection_y != 1)
    {
        selection_y = 1;

        if (selection_x > hand_top / 2)
        {
            discard_button_highlighted = false; // Play button
        }
        else
        {
            discard_button_highlighted = true; // Discard button
        }
    }
    else if (selection_y == 1) // On row of play/discard buttons
    {
        if (discard_button_highlighted == false) // Play button logic
        {
            memset16(&pal_bg_mem[PLAY_HAND_BTN_SELECTED_BORDER_PID], HIGHLIGHT_COLOR, 1);
            memcpy16(&pal_bg_mem[DISCARD_BTN_SELECTED_BORDER_PID], &pal_bg_mem[DISCARD_BTN_PID], 1);

            if (key_hit(SELECT_CARD) && hands > 0 && hand_play())
            {
                hand_state = HAND_PLAY;
                selection_x = 0;
                selection_y = 0;
                display_hands(--hands);
            }
        }
        else // Discard button logic
        {
			// 7 is score and play hand button color
            memcpy16(&pal_bg_mem[PLAY_HAND_BTN_SELECTED_BORDER_PID], &pal_bg_mem[PLAY_HAND_BTN_PID], 1);
            memset16(&pal_bg_mem[DISCARD_BTN_SELECTED_BORDER_PID], HIGHLIGHT_COLOR, 1);

            if (key_hit(SELECT_CARD) && discards > 0 && hand_discard())
            {
                hand_state = HAND_DISCARD;
                selection_x = 0;
                selection_y = 0;
                display_hands(--discards);
                set_hand();
                tte_printf("#{P:%d,%d; cx:0x%X000}%d", DISCARDS_TEXT_RECT.left, DISCARDS_TEXT_RECT.top, TTE_RED_PB, discards);
            }
        }
    }
    else if (selection_y == 0) // On row of cards
    {
        memcpy16(&pal_bg_mem[PLAY_HAND_BTN_SELECTED_BORDER_PID], &pal_bg_mem[PLAY_HAND_BTN_PID], 1); // Play button highlight color
        memcpy16(&pal_bg_mem[DISCARD_BTN_SELECTED_BORDER_PID], &pal_bg_mem[DISCARD_BTN_PID], 1); // Discard button highlight color
        
        if (key_hit(SELECT_CARD))
        {
            hand_toggle_card_selection();
            set_hand();
        }

        if (key_hit(DESELECT_CARDS))
        {
            hand_deselect_all_cards();
            set_hand();
        }
    }

    if (key_hit(SORT_HAND))
    {
        hand_change_sort();
    }
}

static void game_playing_process_input_and_state()
{
    if (hand_state == HAND_SELECT)
    {
        game_playing_process_hand_select_input();
    }
    else if (play_state == PLAY_ENDING)
    {
        if (mult > 0)
        {
            temp_score = chips * mult;
            lerped_temp_score = int2fx(temp_score);
            lerped_score = int2fx(score);

            display_temp_score(temp_score);

            chips = 0;
            mult = 0;
            display_mult(mult);
            display_chips(chips);
        }
    }
    else if (play_state == PLAY_ENDED)
    {
        lerped_temp_score -= int2fx(temp_score * get_game_speed())/ 40;
        lerped_score += int2fx(temp_score * get_game_speed()) / 40;

        if (lerped_temp_score > 0)
        {
            display_temp_score(fx2int(lerped_temp_score));

            // We actually don't need to erase this because the score only increases
            display_score(fx2int(lerped_score)); // Set the score display

            if (temp_score <= 0)
            {
                tte_erase_rect_wrapper(TEMP_SCORE_RECT);
            }
        }
        else
        {
            score += temp_score;
            temp_score = 0;
            lerped_temp_score = 0;
            lerped_score = 0;

            tte_erase_rect_wrapper(TEMP_SCORE_RECT); // Just erase the temp score

            display_score(score);
        }
    }
}

static void game_playing_process_card_draw()
{
    if (hand_state == HAND_DRAW && cards_drawn < hand_size)
    {
        if (timer % FRAMES(10) == 0) // Draw a card every 10 frames
        {
            cards_drawn++;
            card_draw();
        }
    }
    else if (hand_state == HAND_DRAW)
    {
        hand_state = HAND_SELECT; // Change the hand state to select after drawing all the cards
        cards_drawn = 0;
        timer = TM_ZERO;
    }
}

static bool game_round_is_over()
{
    return hands == 0 || score >= blind_get_requirement(current_blind, ante);
}

static void game_playing_handle_round_over()
{
    enum GameState next_state = GAME_STATE_ROUND_END;

    if (score >= blind_get_requirement(current_blind, ante))
    {
        if (current_blind == BLIND_TYPE_BOSS)
        {
            if (ante < MAX_ANTE)
            {
                display_ante(++ante);
            }
            else
            {
                next_state = GAME_STATE_WIN;
            }
        }
    }
    else if (hands == 0)
    {
        next_state = GAME_STATE_LOSE;
    }

    game_change_state(next_state);
}

static void game_playing_discarded_cards_loop()
{
    // Discarded cards loop (mainly for shuffling)
    if (hand_get_size() == 0 && hand_state == HAND_SHUFFLING && discard_top >= -1 && timer > FRAMES(10))
    {
        change_background(BG_ID_ROUND_END); // Change the background to the round end background. This is how it works in Balatro, so I'm doing it this way too.

        // We take each discarded card and put it back into the deck with a short animation
        static CardObject* discarded_card_object = NULL;
        if (discarded_card_object == NULL)
        {
            discarded_card_object = card_object_new(discard_pop());
            //discarded_card_object->sprite = sprite_new(ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF, ATTR1_SIZE_32, card_sprite_lut[discarded_card_object->card->suit][discarded_card_object->card->rank], 0, 0);
            card_object_set_sprite(discarded_card_object, 0); // Set the sprite for the discarded card object
            sprite_object_reset_transform(discarded_card_object->sprite_object);

            discarded_card_object->sprite_object->tx = int2fx(204);
            discarded_card_object->sprite_object->ty = int2fx(112);
            discarded_card_object->sprite_object->x = int2fx(240);
            discarded_card_object->sprite_object->y = int2fx(80);

            card_object_update(discarded_card_object);
        }
        else
        {
            card_object_update(discarded_card_object);

            if (discarded_card_object->sprite_object->y >= discarded_card_object->sprite_object->ty)
            {
                deck_push(discarded_card_object->card); // Put the card back into the deck
                card_object_destroy(&discarded_card_object);

                play_sfx(SFX_CARD_DRAW, MM_BASE_PITCH_RATE + PITCH_STEP_UNDISCARD_SFX);
            }
        }

        if (discard_top == -1 && discarded_card_object == NULL) // If there are no more discarded cards, stop shuffling
        {
            // After HAND_SHUFFLING the round is over
            game_playing_handle_round_over();
        }
    }
}

static const int HAND_SPACING_LUT[MAX_HAND_SIZE] = { 28, 28, 28, 28, 27, 21, 18, 15, 13, 12, 10, 9, 9, 8, 8, 7 }; // This is a stupid way to do this but I don't care

void card_in_hand_loop_handle_discard_and_shuffling(int card_idx, bool* discarded_card, FIXED* hand_x, FIXED* hand_y, bool* sound_played, bool* break_loop)
{
    if (hand_state != HAND_DISCARD && hand_state != HAND_SHUFFLING)
    {
        // Assumes hand_state is one of these
        return;
    }

    *break_loop = false;
    if (card_object_is_selected(hand[card_idx]) || hand_state == HAND_SHUFFLING)
    {
        if (!*discarded_card)
        {
            *hand_x = int2fx(CARD_DISCARD_PNT.x);
            *hand_y = int2fx(CARD_DISCARD_PNT.y);

            if (!*sound_played)
            {
                play_sfx(SFX_CARD_DRAW, MM_BASE_PITCH_RATE + cards_drawn * PITCH_STEP_DISCARD_SFX);
                *sound_played = true;
            }

            if (hand[card_idx]->sprite_object->x >= *hand_x)
            {
                discard_push(hand[card_idx]->card);
                card_object_destroy(&hand[card_idx]);
                sort_cards();

                hand_top--;
                cards_drawn++; // This technically isn't drawing cards, I'm just reusing the variable
                *sound_played = false;
                timer = TM_ZERO;

                *hand_y = hand[card_idx]->sprite_object->y;
                *hand_x = hand[card_idx]->sprite_object->x;
            }

            *discarded_card = true;
        }
        else
        {
            if (hand_state == HAND_DISCARD)
            {
                *hand_y -= int2fx(15); // Don't raise the card if we're mass discarding, it looks stupid.
            }
            else // hand_state == HAND_SHUFFLING
            {
                *hand_y += int2fx(24);
            }
            *hand_x = *hand_x + (int2fx(card_idx) - int2fx(hand_top) / 2) * -HAND_SPACING_LUT[hand_top];
        }
    }
    else
    {
        *hand_x = *hand_x + (int2fx(card_idx) - int2fx(hand_top) / 2) * -HAND_SPACING_LUT[hand_top];
    }

    if (card_idx == 0 && *discarded_card == false && timer % FRAMES(10) == 0)
    {
        // This is never reached in the case of HAND_SHUFFLING.        // Not sure why but that's how it's supposed to be.
        hand_state = HAND_DRAW;
        *sound_played = false;
        cards_drawn = 0;
        hand_selections = 0;
        timer = TM_ZERO;
        *break_loop = true;
        return;
    };
}

static void cards_in_hand_update_loop(bool* discarded_card, int* played_selections, bool* sound_played)
{
    for (int i = hand_top + 1; i >= 0; i--) // Start from the end of the hand and work backwards because that's how Balatro does it
    {
        if (hand[i] != NULL)
        {
            FIXED hand_x = int2fx(HAND_START_POS.x);
            FIXED hand_y = int2fx(HAND_START_POS.y);

            switch (hand_state)
            {
            case HAND_DRAW:
                hand_x = hand_x + (int2fx(i) - int2fx(hand_top) / 2) * -HAND_SPACING_LUT[hand_top];
                break;
            case HAND_SELECT:
                bool is_focused = (i == selection_x && selection_y == 0);

                if (is_focused && !card_object_is_selected(hand[i]))
                {
                    hand_y -= int2fx(CARD_FOCUSED_UNSEL_Y);
                }
                else if (!is_focused && card_object_is_selected(hand[i]))
                {
                    hand_y -= int2fx(CARD_UNFOCUSED_SEL_Y);
                }
                else if (is_focused && card_object_is_selected(hand[i]))
                {
                    hand_y -= int2fx(CARD_FOCUSED_SEL_Y);
                }

                if (i != selection_x && hand[i]->sprite_object->y > hand_y)
                {
                    hand[i]->sprite_object->y = hand_y;
                    hand[i]->sprite_object->vy = 0;
                }

                hand_x = hand_x + (int2fx(i) - int2fx(hand_top) / 2) * -HAND_SPACING_LUT[hand_top]; // TODO: Change this later to reference a 2D LUT of positions
                break;
            case HAND_SHUFFLING:
                /* FALL THROUGH */
            case HAND_DISCARD: // TODO: Add sound
                bool break_loop;
                card_in_hand_loop_handle_discard_and_shuffling(i, discarded_card, &hand_x, &hand_y, sound_played, &break_loop);
                if (break_loop) break;

                break;
            case HAND_PLAY:
                hand_x = hand_x + (int2fx(i) - int2fx(hand_top) / 2) * -HAND_SPACING_LUT[hand_top];
                hand_y += int2fx(24);

                if (card_object_is_selected(hand[i]) && *discarded_card == false && timer % FRAMES(10) == 0)
                {
                    card_object_set_selected(hand[i], false);
                    played_push(hand[i]);
                    sprite_destroy(&hand[i]->sprite_object->sprite);
                    hand[i] = NULL;
                    sort_cards();

                    play_sfx(SFX_CARD_DRAW, MM_BASE_PITCH_RATE + cards_drawn*PITCH_STEP_DISCARD_SFX);

                    hand_top--;
                    hand_selections--;
                    cards_drawn++;

                    *discarded_card = true;
                }

                if (i == 0 && *discarded_card == false && timer % FRAMES(10) == 0)
                {
                    hand_state = HAND_PLAYING;
                    cards_drawn = 0;
                    hand_selections = 0;
                    timer = TM_ZERO;
                    *played_selections = played_top + 1;

                    switch (hand_type) // select the cards that apply to the hand type
                    {
                    case NONE:
                        break;
                    case HIGH_CARD: // find the card with the highest rank in the hand
                        int highest_rank_index = 0;

                        for (int i = 0; i <= played_top; i++)
                        {
                            if (played[i]->card->rank > played[highest_rank_index]->card->rank)
                            {
                                highest_rank_index = i;
                            }
                        }

                        card_object_set_selected(played[highest_rank_index], true);
                        break;
                    case PAIR: // find two cards with the same rank (Requires recursion)
                        for (int i = 0; i <= played_top - 1; i++)
                        {
                            for (int j = i + 1; j <= played_top; j++)
                            {
                                if (played[i]->card->rank == played[j]->card->rank)
                                {
                                    card_object_set_selected(played[i], true);
                                    card_object_set_selected(played[j], true);
                                    break;
                                }
                            }

                            if (card_object_is_selected(played[i])) break;
                        }
                        break;
                    case TWO_PAIR: // find two pairs of cards with the same rank (Requires recursion)
                        int i;

                        for (i = 0; i <= played_top - 1; i++)
                        {
                            for (int j = i + 1; j <= played_top; j++)
                            {
                                if (played[i]->card->rank == played[j]->card->rank)
                                {
                                    card_object_set_selected(played[i], true);
                                    card_object_set_selected(played[j], true);

                                    break;
                                }
                            }

                            if (card_object_is_selected(played[i])) break;
                        }

                        for (; i <= played_top - 1; i++) // Find second pair
                        {
                            for (int j = i + 1; j <= played_top; j++)
                            {
                                if (played[i]->card->rank == played[j]->card->rank && !card_object_is_selected(played[i]) && !card_object_is_selected(played[j]))
                                {
                                    card_object_set_selected(played[i], true);
                                    card_object_set_selected(played[j], true);
                                    break;
                                }
                            }
                        }
                        break;
                    case THREE_OF_A_KIND: // find three cards with the same rank (requires recursion)
                        for (int i = 0; i <= played_top - 1; i++)
                        {
                            for (int j = i + 1; j <= played_top; j++)
                            {
                                if (played[i]->card->rank == played[j]->card->rank)
                                {
                                    card_object_set_selected(played[i], true);
                                    card_object_set_selected(played[j], true);

                                    for (int k = j + 1; k <= played_top; k++)
                                    {
                                        if (played[i]->card->rank == played[k]->card->rank && !card_object_is_selected(played[k]))
                                        {
                                            card_object_set_selected(played[k], true);
                                            break;
                                        }
                                    }

                                    break;
                                }
                            }

                            if (card_object_is_selected(played[i])) break;
                        }
                        break;
                    case FOUR_OF_A_KIND: // find four cards with the same rank (requires recursion)
                        if (played_top >= 3) // If there are 5 cards selected we just need to find the one card that doesn't match, and select the others
                        {
                            int unmatched_index = -1;

                            for (int i = 0; i <= played_top; i++)
                            {
                                if (played[i]->card->rank != played[(i + 1) % played_top]->card->rank && played[i]->card->rank != played[(i + 2) % played_top]->card->rank)
                                {
                                    unmatched_index = i;
                                    break;
                                }
                            }

                            for (int i = 0; i <= played_top; i++)
                            {
                                if (i != unmatched_index)
                                {
                                    card_object_set_selected(played[i], true);
                                }
                            }
                        }
                        else // If there are only 4 cards selected we know they match
                        {
                            for (int i = 0; i <= played_top; i++)
                            {
                                card_object_set_selected(played[i], true);
                            }
                        }
                        break;
                    case STRAIGHT:
                        /* FALL THROUGH */
                    case FLUSH:
                        /* FALL THROUGH */
                    case FULL_HOUSE:
                        /* FALL THROUGH */
                    case STRAIGHT_FLUSH:
                        /* FALL THROUGH */
                    case ROYAL_FLUSH:
                        /* FALL THROUGH */
                    case FIVE_OF_A_KIND:
                        /* FALL THROUGH */
                    case FLUSH_HOUSE:
                        /* FALL THROUGH */
                    case FLUSH_FIVE: // Select all played cards in the hand (This is functionally identical as the above hand types)
                        for (int i = 0; i <= played_top; i++)
                        {
                            card_object_set_selected(played[i], true);
                        }
                        break;
                    }
                }

                break;
            case HAND_PLAYING: // Don't need to do anything here, just wait for the player to select cards
                hand_x = hand_x + (int2fx(i) - int2fx(hand_top) / 2) * -HAND_SPACING_LUT[hand_top];
                hand_y += int2fx(24);
                break;
            }

            hand[i]->sprite_object->tx = hand_x;
            hand[i]->sprite_object->ty = hand_y;
            card_object_update(hand[i]);
        }
    }
}

static void played_cards_update_loop(bool* discarded_card, int* played_selections, bool* sound_played)
{
    // So this one is a bit fucking weird because I have to work kinda backwards for everything because of the order of the pushed cards from the hand to the play stack
    // (also crazy that the company that published Balatro is called "Playstack" and this is a play stack, but I digress)
    for (int i = 0; i <= played_top; i++)
    {
        if (played[i] != NULL)
        {
            if (card_object_get_sprite(played[i]) == NULL)
            {
                //played[i]->sprite = sprite_new(ATTR0_SQUARE | ATTR0_4BPP | ATTR0_AFF, ATTR1_SIZE_32, card_sprite_lut[played[i]->card->suit][played[i]->card->rank], 0, i + MAX_HAND_SIZE);
                card_object_set_sprite(played[i], i + MAX_HAND_SIZE); // Set the sprite for the played card object
            }

            FIXED played_x = int2fx(120);
            FIXED played_y = int2fx(70);
            FIXED played_scale = FIX_ONE;

            played_x = played_x + (int2fx(played_top - i) - int2fx(played_top) / 2) * -27;

            switch (play_state)
            {
                case PLAY_PLAYING:
                    if (i == 0 && (timer % FRAMES(10) == 0 || !card_object_is_selected(played[played_top - *played_selections])) && timer > FRAMES(40))
                    {
                        (*played_selections)--;

                        if (*played_selections == 0)
                        {
                            play_state = PLAY_SCORING;
                            timer = TM_ZERO;
                        }
                    }

                    if (card_object_is_selected(played[i]) && played_top - i >= *played_selections)
                    {
                        played_y -= int2fx(10);
                    }
                    break;
                case PLAY_SCORING:
                    if (i == 0 && (timer % FRAMES(30) == 0) && timer > FRAMES(40))
                    {
                        // So pretend "played_selections" is now called "scored_cards" and it counts the number of cards that have been scored
                        int scored_cards = 0;
                        for (int j = 0; j <= played_top; j++)
                        {
                            tte_erase_rect_wrapper(PLAYED_CARDS_SCORES_RECT);

                            if (*played_selections > 0)
                            {
                                ListItr itr = list_itr_new(&_all_jokers_list);
                                ListNode* ln;

                                while((ln = list_itr_next(&itr)))
                                {
                                    JokerObject* joker_object = POOL_AT(JokerObject, ln->elem_idx);

                                    if (joker_object_score(joker_object, played[*played_selections - 1]->card, &chips, &mult, NULL, &money, NULL)) // NULLs aren't implemented yet
                                    {
                                        display_chips(chips);
                                        display_mult(mult);
                                        display_money(money);

                                        return;
                                    }

                                }
                            }

                            if (card_object_is_selected(played[j]))
                            {
                                scored_cards = j + 1; // Count the number of cards that have been scored
                                if (scored_cards > *played_selections)
                                {
                                    ListItr itr = list_itr_new(&_all_jokers_list);
                                    ListNode* ln;

                                    while((ln = list_itr_next(&itr)))
                                    {
                                        JokerObject* joker_object = POOL_AT(JokerObject, ln->elem_idx);
                                        joker_object->joker->processed = false; // Reset the joker's processed state for the next score
                                    }

                                    tte_set_pos(fx2int(played[j]->sprite_object->x) + 8, SCORED_CARD_TEXT_Y); // Offset of 16 pixels to center the text on the card
                                    tte_set_special(0xD000); // Set text color to blue from background memory

                                    // Write the score to a character buffer variable
                                    char score_buffer[INT_MAX_DIGITS + 2]; // for '+' and null terminator
                                    snprintf(score_buffer, sizeof(score_buffer), "+%d", card_get_value(played[j]->card));
                                    tte_write(score_buffer);

                                    *played_selections = scored_cards;
                                    card_object_shake(played[j], SFX_CARD_SELECT);

                                    // Relocated card scoring logic here
                                    chips += card_get_value(played[j]->card);
                                    display_chips(chips);

                                    break;
                                }
                            }

                            if (j == played_top && scored_cards == *played_selections) // Check if it's the last card 
                            {
                                tte_erase_rect_wrapper(PLAYED_CARDS_SCORES_RECT);

                                ListItr itr = list_itr_new(&_all_jokers_list);
                                ListNode* ln;

                                while((ln = list_itr_next(&itr)))
                                {
                                    JokerObject* joker_object = POOL_AT(JokerObject, ln->elem_idx);
                                    if (joker_object_score(joker_object, NULL, &chips, &mult, NULL, &money, NULL)) // NULLs aren't implemented yet
                                    {
                                        display_chips(chips);
                                        display_mult(mult);
                                        display_money(money);

                                        return; // Returning was just the easiest way to break out of the loop
                                    }
                                }

                                itr = list_itr_new(&_all_jokers_list);

                                while((ln = list_itr_next(&itr)))
                                {
                                    JokerObject* joker_object = POOL_AT(JokerObject, ln->elem_idx);

                                    joker_object->joker->processed = false; // Reset the joker's processed state for the next round
                                }

                                play_state = PLAY_ENDING;
                                timer = TM_ZERO;
                                *played_selections = played_top + 1; // Reset the played selections to the top of the played stack
                                break;
                            }
                        }
                    }

                    if (card_object_is_selected(played[i]))
                    {
                        played_y -= int2fx(10);
                    }
                    break;
                case PLAY_ENDING: // This is the reverse of PLAY_PLAYING. The cards get reset back to their neutral position sequentially
                    if (i == 0 && (timer % FRAMES(10) == 0 || !card_object_is_selected(played[played_top - *played_selections])) && timer > FRAMES(40))
                    {
                        (*played_selections)--;

                        if (*played_selections == 0)
                        {
                            play_state = PLAY_ENDED;
                            timer = TM_ZERO;
                        }
                    }

                    if (card_object_is_selected(played[i]) && played_top - i <= *played_selections - 1)
                    {
                        played_y -= int2fx(10);
                    }
                    break;
                case PLAY_ENDED: // Basically a copy of HAND_DISCARD
                    if (!*discarded_card && played[i] != NULL && timer > FRAMES(40))
                    {
                        played_x = int2fx(240);
                        played_y = int2fx(70);

                        if (!*sound_played)
                        {
                            play_sfx(SFX_CARD_DRAW, MM_BASE_PITCH_RATE + cards_drawn*PITCH_STEP_DISCARD_SFX);
                            *sound_played = true;
                        }

                        if (played[i]->sprite_object->x >= played_x)
                        {
                            discard_push(played[i]->card); // Push the card to the discard pile
                            card_object_destroy(&played[i]);

                            //played_top--; 
                            cards_drawn++; // This technically isn't drawing cards, I'm just reusing the variable
                            *sound_played = false;

                            if (i == played_top)
                            {
                                if (game_round_is_over())
                                {
                                    hand_state = HAND_SHUFFLING;
                                }
                                else
                                {
                                    hand_state = HAND_DRAW;
                                }

                                play_state = PLAY_PLAYING;
                                cards_drawn = 0;
                                hand_selections = 0;
                                *played_selections = 0;
                                played_top = -1; // Reset the played stack
                                timer = TM_ZERO;
                                break; // Break out of the loop to avoid accessing an invalid index
                            }
                        }

                        *discarded_card = true;
                    }

                    break;
            }

            played[i]->sprite_object->tx = played_x;
            played[i]->sprite_object->ty = played_y;
            played[i]->sprite_object->tscale = played_scale;
            card_object_update(played[i]);
        }
    }
}

static void game_playing_ui_text_update()
{
    static int last_hand_size = 0;
    static int last_deck_size = 0;

    if (last_hand_size != hand_get_size() || last_deck_size != deck_get_size())
    {
        if (background == BG_ID_CARD_SELECTING)
        {
            tte_printf("#{P:%d,%d; cx:0x%X000}%d/%d", HAND_SIZE_RECT_SELECT.left, HAND_SIZE_RECT_SELECT.top, TTE_WHITE_PB, hand_get_size(), hand_get_max_size()); // Hand size/max size
        }
        else if (background == BG_ID_CARD_PLAYING)
        {
            tte_printf("#{P:%d,%d; cx:0x%X000}%d/%d", HAND_SIZE_RECT_PLAYING.left, HAND_SIZE_RECT_PLAYING.top, TTE_WHITE_PB, hand_get_size(), hand_get_max_size()); // Hand size/max size
        }

        tte_printf("#{P:%d,%d; cx:0x%X000}%d/%d", DECK_SIZE_RECT.left, DECK_SIZE_RECT.top, TTE_WHITE_PB, deck_get_size(), deck_get_max_size()); // Deck size/max size

        last_hand_size = hand_get_size();
        last_deck_size = deck_get_size();
    }
}

static void game_round_end_cashout()
{
    money += hands + blind_get_reward(current_blind); // Reward the player
    display_money(money);

    hands = max_hands; // Reset the hands to the maximum
    discards = max_discards; // Reset the discards to the maximum
    display_hands(hands); // Set the hands display
    display_discards(discards); // Set the discards display

    score = 0;
    display_score(score); // Set the score display
}

static void game_playing_on_update()
{
    // Background logic (thissss might be moved to the card'ssss logic later. I'm a sssssnake)
    if (hand_state == HAND_DRAW || hand_state == HAND_DISCARD || hand_state == HAND_SELECT)
    {
        change_background(BG_ID_CARD_SELECTING);
    }
    else if (hand_state != HAND_SHUFFLING)
    {
        change_background(BG_ID_CARD_PLAYING);
    }

    game_playing_process_input_and_state();

    // Card logic

    game_playing_process_card_draw();

    game_playing_discarded_cards_loop();

    static int played_selections = 0;
    static bool sound_played = false;
    bool discarded_card = false;

    cards_in_hand_update_loop(&discarded_card, &played_selections, &sound_played);
	played_cards_update_loop(&discarded_card, &played_selections, &sound_played);
    
    game_playing_ui_text_update();
}

static void game_round_end_on_exit()
{
    // Cleanup blind tokens from this round to avoid accumulating 
    // allocated blind sprites each round
    blind_reward = 0;
    hand_reward = 0;
    interest_reward = 0;
    sprite_destroy(&playing_blind_token);
    sprite_destroy(&round_end_blind_token);
    // TODO: Reuse sprites for blind selection?
}

static void game_round_end_on_update()
{
    if (state_info[game_state].substate == ROUND_END_EXIT) 
    {
        game_change_state(GAME_STATE_SHOP);
        return;
    }

    int substate = state_info[game_state].substate;
    round_end_state_actions[substate]();
}

static void game_round_end_start()
{
    if (timer == TM_RESET_STATIC_VARS) // Reset static variables to default values upon re-entering the round end state
    {   
        change_background(BG_ID_ROUND_END); // Change the background to the round end background
        state_info[game_state].substate = START_EXPAND_POPUP; // Change the state to the next one
        timer = TM_ZERO; // Reset the timer
        blind_reward = blind_get_reward(current_blind);
        hand_reward = hands;
    }
}

static void game_round_end_start_expand_popup()
{
    main_bg_se_copy_rect_1_tile_vert(POP_MENU_ANIM_RECT, SE_UP);
    
    if (timer == TM_END_POP_MENU_ANIM)
    {
        state_info[game_state].substate = DISPLAY_FINISHED_BLIND;
        timer = TM_ZERO;
    }
}

static void game_round_end_display_finished_blind()
{
    obj_unhide(round_end_blind_token->obj, 0);
    
    int current_ante = ante;
    if (current_blind == BLIND_TYPE_BOSS) current_ante--; // Beating the boss blind increases the ante, so we need to display the previous ante value
    
    Rect blind_req_rect = ROUND_END_BLIND_REQ_RECT;
    int blind_req = blind_get_requirement(current_blind, current_ante);
    update_text_rect_to_right_align_num(&blind_req_rect, blind_req, OVERFLOW_RIGHT);
    
    tte_printf("#{P:%d,%d; cx:0x%X000}%d", blind_req_rect.left, blind_req_rect.top, TTE_RED_PB, blind_req);
    
    if (timer == TM_START_ROUND_END_MENU_AMIN)
    {
        Rect single_line_rect = ROUND_END_MENU_RECT;
        single_line_rect.top = 11;
        single_line_rect.bottom = single_line_rect.top + 1;
        main_bg_se_copy_rect_1_tile_vert(single_line_rect, SE_DOWN);
    }
    
    if (timer >= TM_END_DISPLAY_FIN_BLIND)
    {
        state_info[game_state].substate = DISPLAY_SCORE_MIN;
        timer = TM_ZERO;
    }
}

static void game_round_end_display_score_min()
{
    const int timer_offset = timer - 1;
    const int x_from = 0;
    const int y_from = 29;
    const int x_to = 13;
    const int y_to = 11;
    
    memcpy16(&se_mem[MAIN_BG_SBB][x_to + timer_offset + 32 * y_to], &se_mem[MAIN_BG_SBB][x_from + timer_offset + 32 * y_from], 1);
    
    if (timer >= TM_END_DISPLAY_SCORE_MIN)
    {
        state_info[game_state].substate = UPDATE_BLIND_REWARD;
        timer = TM_ZERO;
    }
}

static void game_round_end_update_blind_reward()
{
    if (timer % FRAMES(20) != 0) return;
    
    // TODO: Add sound effect here
    
    if (blind_reward > 0)
    {
        blind_reward--;
        tte_printf("#{P:%d,%d; cx:0x%X000}$%d", BLIND_REWARD_RECT.left , BLIND_REWARD_RECT.top, TTE_YELLOW_PB, blind_reward);
        tte_printf("#{P:%d,%d; cx:0x%X000}$%d", ROUND_END_BLIND_REWARD_RECT.left, ROUND_END_BLIND_REWARD_RECT.top, TTE_YELLOW_PB, blind_get_reward(current_blind) - blind_reward);
    }
    else if (timer > FRAMES(20))
    {
        tte_erase_rect_wrapper(BLIND_REWARD_RECT);
        tte_erase_rect_wrapper(BLIND_REQ_TEXT_RECT);
        obj_hide(playing_blind_token->obj);
        affine_background_load_palette(affine_background_gfxPal);
        state_info[game_state].substate = BLIND_PANEL_EXIT;
        timer = TM_ZERO;
    }
}

static void game_round_end_panel_exit()
{
    // TODO: make heads or tails of what's going on here and replace
    // magic numbers.
    if (timer < 8)
    {
        main_bg_se_copy_rect_1_tile_vert(TOP_LEFT_PANEL_ANIM_RECT, SE_UP);
    
        if (timer == 1) // Copied from shop. Feels slightly too niche of a function for me personally to make one.
        {
            reset_top_left_panel_bottom_row();
        }
        else if (timer == 2)
        {
            int y = 5;
            memset16(&se_mem[MAIN_BG_SBB][32 * (y - 1)], 0x0001, 1);
            memset16(&se_mem[MAIN_BG_SBB][1 + 32 * (y - 1)], 0x0002, 7);
            memset16(&se_mem[MAIN_BG_SBB][8 + 32 * (y - 1)], 0x0401, 1); 
        }
    }   
    else if (timer > FRAMES(20))
    {
        memset16(&pal_bg_mem[REWARD_PANEL_BORDER_PID], 0x1483, 1);
        state_info[game_state].substate = DISPLAY_REWARDS;
        timer = TM_ZERO;
    }
}

static void game_round_end_display_rewards()
{
    int hand_y = 0;
    
    // TODO: Implement interest
    //int interest_y = 0;
    
    if (hands > 0)
    {
        hand_y = 1;
    }
    
    // TODO: implement interest
    // if (interest > 0)
    // {
    //     interest_y = 1 + hand_y;
    // }
    
    if (hand_reward <= 0 && interest_reward <= 0) // Once all rewards are accounted for go to the next state
    {
        timer = TM_ZERO; // Reset the timer
        state_info[game_state].substate = DISPLAY_CASHOUT; // Go to the next state
    }
    else if (timer == TM_START_ROUND_END_MENU_AMIN) // Expand the black part of the panel down by one tile
    {
        Rect single_line_rect = ROUND_END_MENU_RECT;
        single_line_rect.top = 12;
        single_line_rect.bottom = single_line_rect.top + 1;
        main_bg_se_copy_rect_1_tile_vert(single_line_rect, SE_DOWN);
    }
    else if (timer < TM_ELLIPSIS_PRINT_MAX_TM) // Use TTE to print '.' until the end of the panel width
    {
        // Print the separator dots
        int x = (8 + timer) * TILE_SIZE;
        int y = (13) * TILE_SIZE;
    
        tte_printf("#{P:%d,%d; cx:0x%X000}.", x, y, TTE_WHITE_PB); 
    }
    else if (timer >= TM_DISPLAY_REWARDS_CONT_WAIT && hand_reward > 0) // Wait an additional 15 frames since the last sequenced action
    {
        if (timer == TM_DISPLAY_REWARDS_CONT_WAIT) // Expand the black part of the panel down by one tile again
        {
            Rect single_line_rect = ROUND_END_MENU_RECT;
            single_line_rect.top = 12 + hand_y;
            single_line_rect.bottom = single_line_rect.top + 1;
            main_bg_se_copy_rect_1_tile_vert(single_line_rect, SE_DOWN);
    
            tte_printf("#{P:%d,%d; cx:0x%X000}%d #{cx:0x%X000}Hands", ROUND_END_NUM_HANDS_RECT.left, ROUND_END_NUM_HANDS_RECT.top, TTE_BLUE_PB,  hand_reward, TTE_WHITE_PB); // Print the hand reward
        }
        else if (timer > TM_HAND_REWARD_INCR_WAIT && timer % FRAMES(20) == 0) // After 15 frames, every 20 frames, increment the hand reward text until the hand reward variable is depleted
        {
            int y = (13 + hand_y) * TILE_SIZE;
            hand_reward--;
            tte_printf("#{P:%d, %d; cx:0x%X000}$%d", HAND_REWARD_RECT.left, y, TTE_YELLOW_PB, hands - hand_reward); // Print the hand reward
        }
    }
}

static void game_round_end_display_cashout()
{
    if (timer == FRAMES(40)) // Put the "cash out" button onto the round end panel
    {
        Rect left_rect = {4, 29, 4, 31};
        BG_POINT left_point = {10, 8};
        main_bg_se_copy_rect(left_rect, left_point);
    
        Rect right_rect = {7, 29, 7, 31};
        BG_POINT right_point = {23, 8};
        main_bg_se_copy_rect(right_rect, right_point);
    
        Rect top_rect = {11, 8, 22, 8};
        BG_POINT top_point = {6, 29};
        main_bg_se_fill_rect_with_se(main_bg_se_get_se(top_point), top_rect);
    
        Rect middle_rect = {11, 9, 22, 9};
        BG_POINT middle_point = {6, 30};
        main_bg_se_fill_rect_with_se(main_bg_se_get_se(middle_point), middle_rect);
    
        Rect bottom_rect = {11, 10, 22, 10};
        BG_POINT bottom_point = {6, 31};
        main_bg_se_fill_rect_with_se(main_bg_se_get_se(bottom_point), bottom_rect);
    
        tte_printf("#{P:%d, %d; cx:0x%X000}Cash Out: $%d", CASHOUT_RECT.left, CASHOUT_RECT.top, TTE_WHITE_PB, hands + blind_get_reward(current_blind)); // Print the cash out amount
    }
    else if (timer > FRAMES(40) && key_hit(SELECT_CARD)) // Wait until the player presses A to cash out
    {
        game_round_end_cashout();
    
        state_info[game_state].substate = DISMISS_ROUND_END_PANEL; // Go to the next state
        timer = TM_ZERO; // Reset the timer
    
        obj_hide(round_end_blind_token->obj); // Hide the blind token object
        tte_erase_rect_wrapper(BLIND_TOKEN_TEXT_RECT); // Erase the blind token text
    }
}

static void game_round_end_dismiss_round_end_panel()
{
    Rect round_end_down = ROUND_END_MENU_RECT;
    round_end_down.top--;
    main_bg_se_copy_rect_1_tile_vert(round_end_down, SE_DOWN);
    
    if (timer >= TM_DISMISS_ROUND_END_TM)
    {
        timer = TM_ZERO; 
        state_info[game_state].substate = ROUND_END_EXIT;
    }
}

// Shop
#define REROLL_BASE_COST 5 // Base cost for rerolling the shop items
static int reroll_cost = REROLL_BASE_COST;

#define NEXT_ROUND_BTN_SEL_X 0

#define REROLL_BTN_FRAME_PAL_IDX        7
#define REROLL_BTN_PAL_IDX              3

void print_price_under_sprite_object(SpriteObject *sprite_object, int price)
{
    int x = fx2int(sprite_object->tx) + TILE_SIZE - (get_digits_even(price) - 1) * TILE_SIZE;
    int y = fx2int(sprite_object->ty) + CARD_SPRITE_SIZE + TILE_SIZE; // TODO: Should probably extract the sprite size
    tte_printf("#{P:%d,%d; cx:0x%X000}$%d", x, y, TTE_YELLOW_PB, price);
}

void erase_price_under_sprite_object(SpriteObject *sprite_object)
{
    Rect price_rect;
    price_rect.left = fx2int(sprite_object->tx);
    price_rect.top = fx2int(sprite_object->ty) + CARD_SPRITE_SIZE + TILE_SIZE; 
    // TODO: Should probably extract the size from the sprite
    
    price_rect.right = price_rect.left + TILE_SIZE * 3;
    price_rect.bottom = price_rect.top + 2*TILE_SIZE; // Taking 2 tiles down so the highlighted case is also covered
    tte_erase_rect_wrapper(price_rect);
}

static int game_shop_get_random_joker_idx()
{
    // Roll for what rarity the joker will be
    int joker_rarity = joker_get_random_rarity();
        
    // Now determine how many jokers are available based on the rarity
    //int jokers_avail_size = list_get_size(jokers_available_to_shop);
    int jokers_avail_size = list_get_len(_avail_shop_jokers_list);
    int matching_indices[jokers_avail_size];
    int match_count = 0;

    ListItr itr = list_itr_new(&_avail_shop_jokers_list);
    ListNode* ln;

    int i = 0;
    while((ln = list_itr_next(&itr)))
    {
        //intptr_t joker_id = int_list_get(jokers_available_to_shop, i);
        intptr_t joker_id = 0;
        const JokerInfo *info = get_joker_registry_entry(joker_id); 
        if (info->rarity == joker_rarity)
        {
            matching_indices[match_count] = i;
            match_count++;
        }
        i++;
    }

    int selected_joker_idx = 0;
    if (match_count > 0)
    {
        // If we counted at least one joker with matching rarity, pick one of them randomly
        selected_joker_idx = matching_indices[random() % match_count];
    }
    else
    {
        // Didn't find any jokers of matching rarity, just pick one at random instead
        selected_joker_idx = random() % jokers_avail_size;
    }

    return selected_joker_idx;
}

static void game_shop_create_items()
{
    tte_erase_rect_wrapper(SHOP_PRICES_TEXT_RECT);
    if (list_is_empty(_avail_shop_jokers_list))
    {
        // No jokers to create
        return;
    }

    list_destroy(&_shop_jokers_list);
    _shop_jokers_list = list_new();

    for (int i = 0; i < MAX_SHOP_JOKERS; i++)
    {
        intptr_t joker_id = 0;
        #ifdef TEST_JOKER_ID0 // Allow defining an ID for a joker to always appear in shop and be tested
        if (int_list_exists(jokers_available_to_shop, TEST_JOKER_ID0))
        {
            joker_id = TEST_JOKER_ID0;
            list_remove_at_idx(&_avail_shop_jokers_list, joker_idx);
        }
        else
        #endif
        #ifdef TEST_JOKER_ID1
        if (int_list_exists(jokers_available_to_shop, TEST_JOKER_ID1))
        {
            joker_id = TEST_JOKER_ID1;
            list_remove_at_idx(&_avail_shop_jokers_list, joker_idx);
        }
        else
        #endif
        {
            int joker_idx = game_shop_get_random_joker_idx();
            joker_id = list_get_at_idx(_avail_shop_jokers_list, joker_idx);
            list_remove_at_idx(&_avail_shop_jokers_list, joker_idx);
        }
        
        JokerObject *joker_object = joker_object_new(joker_new(joker_id));

        joker_object->sprite_object->x = int2fx(120 + i * CARD_SPRITE_SIZE);
        joker_object->sprite_object->y = int2fx(160);
        joker_object->sprite_object->tx = joker_object->sprite_object->x;
        joker_object->sprite_object->ty = int2fx(ITEM_SHOP_Y);

        print_price_under_sprite_object(joker_object->sprite_object, joker_object->joker->value);

        sprite_position(joker_object_get_sprite(joker_object), fx2int(joker_object->sprite_object->x), fx2int(joker_object->sprite_object->y));

        list_push_back(&_shop_jokers_list, POOL_IDX(JokerObject, joker_object));
    }
}

// Intro sequence (menu and shop icon coming into frame)
static void game_shop_intro()
{
    main_bg_se_copy_rect_1_tile_vert(POP_MENU_ANIM_RECT, SE_UP);

    if (timer == TM_CREATE_SHOP_ITEMS_WAIT)
    {
        game_shop_create_items();
    }

    if (timer >= TM_SHIFT_SHOP_ICON_WAIT) // Shift the shop icon
    {
        int timer_offset = timer - 6;

        // TODO: Extract to generic function?
        for (int y = 0; y < timer_offset; y++)
        {
            int y_from = 26 + y - timer_offset;
            int y_to = 0 + y;

            Rect from = { 0, y_from, 8, y_from };
            BG_POINT to = { 0, y_to };

            main_bg_se_copy_rect(from, to);
        }
    }

    if (timer == TM_END_GAME_SHOP_INTRO)
    {
        state_info[game_state].substate = GAME_SHOP_ACTIVE;
        timer = TM_ZERO; // Reset the timer
    }
}

static void game_shop_reroll(int *reroll_cost)
{
    money -= *reroll_cost;
    display_money(money); // Update the money display

    ListItr itr = list_itr_new(&_all_jokers_list);
    ListNode* ln;

    while((ln = list_itr_next(&itr)))
    {
        JokerObject* joker_object = POOL_AT(JokerObject, ln->elem_idx);

        if (joker_object != NULL)
        {
            list_push_back(&_avail_shop_jokers_list, joker_object->joker->id);
            joker_object_destroy(&joker_object); // Destroy the joker object if it exists
        }
    }

    list_destroy(&_shop_jokers_list);
    _shop_jokers_list = list_new();

    game_shop_create_items();
    
    itr = list_itr_new(&_all_jokers_list);

    while((ln = list_itr_next(&itr)))
    {
        JokerObject* joker_object = POOL_AT(JokerObject, ln->elem_idx);

        if (joker_object != NULL)
        {
            joker_object->sprite_object->y = joker_object->sprite_object->ty; // Set the y position to the target position
            joker_object_shake(joker_object, UNDEFINED); // Give the joker a little wiggle animation
        }
    }

    (*reroll_cost)++;
    tte_printf("#{P:%d,%d; cx:0x%X000}$%d", SHOP_REROLL_RECT.left, SHOP_REROLL_RECT.top, TTE_WHITE_PB, *reroll_cost);
}

static int jokers_sel_row_get_size()
{
    return list_get_len(_all_jokers_list);
}

static void jokers_sel_row_on_selection_changed(SelectionGrid *selection_grid,
                                                int row_idx, 
                                                const Selection *prev_selection, 
                                                const Selection *new_selection)
{
    if (prev_selection->y == row_idx)
    {
        JokerObject* joker_object = POOL_AT(JokerObject, list_get_at_idx(_all_jokers_list, prev_selection->x));
        if(joker_object != NULL) {
            erase_price_under_sprite_object(joker_object->sprite_object);
            sprite_object_set_focus(joker_object->sprite_object, false);
        }
    }

    if (new_selection->y == row_idx)
    {
        JokerObject* joker_object = POOL_AT(JokerObject, list_get_at_idx(_all_jokers_list, new_selection->x));
        if(joker_object != NULL) {
            sprite_object_set_focus(joker_object->sprite_object, true);
            print_price_under_sprite_object(joker_object->sprite_object, joker_get_sell_value(joker_object->joker));
        }
    }
}

void joker_start_discard_animation(JokerObject *joker_object)
{
    joker_object->sprite_object->tx = int2fx(JOKER_DISCARD_TARGET.x);
    joker_object->sprite_object->ty = int2fx(JOKER_DISCARD_TARGET.y);
    list_push_back(&_discarded_jokers_list, POOL_IDX(JokerObject, joker_object));
}

void game_sell_joker(int joker_idx)
{
    if (joker_idx < 0 || joker_idx > list_get_len(_all_jokers_list))
        return;
    
    int list_offset = list_get_at_idx(_all_jokers_list, joker_idx);
    JokerObject *joker_object = POOL_AT(JokerObject, list_offset);
    money += joker_get_sell_value(joker_object->joker);
    display_money(money);
    erase_price_under_sprite_object(joker_object->sprite_object);

    list_remove_at_idx(&_all_jokers_list, joker_idx);
    list_push_back(&_avail_shop_jokers_list, (intptr_t)joker_object->joker->id);

    joker_start_discard_animation(joker_object);
}

static void jokers_sel_row_on_key_hit(SelectionGrid* selection_grid, Selection* selection)
{
    if (!key_hit(SELL_KEY))
        return;

    game_sell_joker(selection->x);
    // Move the selection away from the jokers so it doesn't point to an invalid place
    selection_grid_move_selection_vert(selection_grid, SCREEN_DOWN);
}

// Shop input
static int shop_top_row_get_size()
{
    return list_get_len(_shop_jokers_list) + 1; // + 1 to account for next round button
}

static void add_to_held_jokers(JokerObject *joker_object)
{
    joker_object->sprite_object->ty = int2fx(HELD_JOKERS_POS.y);
    add_joker(joker_object);
}

static void game_shop_buy_joker(int shop_joker_idx)
{
    int val = shop_joker_idx;
    JokerObject *joker_object = POOL_AT(JokerObject, list_get_at_idx(_shop_jokers_list, val));

    money -= joker_object->joker->value; // Deduct the money spent on the joker
    display_money(money);                // Update the money display
    erase_price_under_sprite_object(joker_object->sprite_object);
    sprite_object_set_focus(joker_object->sprite_object, false);
    add_to_held_jokers(joker_object);
    list_remove_at_idx(&_shop_jokers_list, val); // Remove the joker from the shop
}

static void shop_top_row_on_key_hit(SelectionGrid* selection_grid, Selection* selection)
{
    if (!key_hit(SELECT_CARD))
        return;

    if (selection->x == NEXT_ROUND_BTN_SEL_X)
    {
        // Go to next blind selection game state
        state_info[game_state].substate = GAME_SHOP_EXIT; // Go to the outro sequence state
        timer = TM_ZERO; // Reset the timer
        reroll_cost = REROLL_BASE_COST;

        memcpy16(&pal_bg_mem[NEXT_ROUND_BTN_SELECTED_BORDER_PID], &pal_bg_mem[SHOP_PANEL_SHADOW_PID], 1);

        // memcpy16(&pal_bg_mem[16], &pal_bg_mem[6], 1); 
        // This changes the color of the button to a dark red.
        // However, it shares a palette with the shop icon, so it will change the color of the shop icon as well.
        // And I don't care enough to fix it right now.
    }
    else 
    {
        int shop_joker_idx = selection->x - 1; // - 1 to account for next round button
        JokerObject *joker_object = POOL_AT(JokerObject, list_get_at_idx(_shop_jokers_list, shop_joker_idx));
        if (joker_object == NULL 
            || list_get_len(_all_jokers_list) >= MAX_JOKERS_HELD_SIZE
            || money < joker_object->joker->value)
        {
            return;
        }

        game_shop_buy_joker(shop_joker_idx);

        // In Balatro the selection actually stays on the purchased joker it's easier to just move it left
        selection_grid_move_selection_horz(selection_grid, -1);
    }
}

static void shop_top_row_on_selection_changed(SelectionGrid* selection_grid, int row_idx, 
                                              const Selection* prev_selection, 
                                              const Selection* new_selection)
{
    // The selection grid system only guarantees that the new selection is within bounds
    if (prev_selection->y == row_idx && prev_selection->x >= 0 && prev_selection->x < shop_top_row_get_size())
    {
        if (prev_selection->x == NEXT_ROUND_BTN_SEL_X)
        {
            // Remove next round button highlight
            memcpy16(&pal_bg_mem[NEXT_ROUND_BTN_SELECTED_BORDER_PID], &pal_bg_mem[NEXT_ROUND_BTN_PID], 1);
        }
        else 
        {
            int idx = prev_selection->x - 1;
            JokerObject *joker_object = POOL_AT(JokerObject, list_get_at_idx(_shop_jokers_list, idx));
            sprite_object_set_focus(joker_object->sprite_object, false); 
            // -1 to account for next round button
        }
    }

    if (new_selection->y == row_idx)
    {
        if (new_selection->x == NEXT_ROUND_BTN_SEL_X)
        {
            // Highlight next round button
            memset16(&pal_bg_mem[NEXT_ROUND_BTN_SELECTED_BORDER_PID], HIGHLIGHT_COLOR, 1);
        }
        else 
        {
            int idx = new_selection->x - 1;
            JokerObject *joker_object = POOL_AT(JokerObject, list_get_at_idx(_shop_jokers_list, idx));
            sprite_object_set_focus(joker_object->sprite_object, true); 
        }
    }
}

static int shop_reroll_row_get_size()
{
    return 1; // Only the reroll button
}

static void shop_reroll_row_on_selection_changed(SelectionGrid* selection_grid, int row_idx, const Selection* prev_selection, const Selection* new_selection)
{
    if (row_idx == prev_selection->y)
    {
        // Remove highlight
        memcpy16(&pal_bg_mem[REROLL_BTN_SELECTED_BORDER_PID], &pal_bg_mem[REROLL_BTN_PID], 1);
    }
    else if (row_idx == new_selection->y)
    {
        memset16(&pal_bg_mem[REROLL_BTN_SELECTED_BORDER_PID], HIGHLIGHT_COLOR, 1);
    }
}

static void shop_reroll_row_on_key_hit(SelectionGrid* selection_grid, Selection* selection)
{
    if (money >= reroll_cost)
    {
        game_shop_reroll(&reroll_cost);
    }
}

SelectionGridRow shop_selection_rows[] =
{
    {0, jokers_sel_row_get_size, jokers_sel_row_on_selection_changed, jokers_sel_row_on_key_hit},
    {1, shop_top_row_get_size, shop_top_row_on_selection_changed, shop_top_row_on_key_hit},
    {2, shop_reroll_row_get_size, shop_reroll_row_on_selection_changed, shop_reroll_row_on_key_hit}
};

static const Selection SHOP_INIT_SEL = {-1, 1};

SelectionGrid shop_selection_grid = {shop_selection_rows, NUM_ELEM_IN_ARR(shop_selection_rows), SHOP_INIT_SEL};

// Shop menu input and selection
static void game_shop_process_user_input()
{
    if (timer == TM_SHOP_PRC_INPUT_START)
    {
        // The selection grid is initialized outside of bounds and moved 
        // to trigger the selection change so the initial selection is visible
        shop_selection_grid.selection = SHOP_INIT_SEL;
        selection_grid_move_selection_horz(&shop_selection_grid, 1);
        tte_printf("#{P:%d,%d; cx:0x%X000}$%d", SHOP_REROLL_RECT.left, SHOP_REROLL_RECT.top, TTE_WHITE_PB, reroll_cost);
    }

    // Shop input logic
    selection_grid_process_input(&shop_selection_grid);
}

static void game_shop_lights_anim_frame()
{
    // Shift palette around the border of the shop icon
    COLOR shifted_palette[4];
    memcpy16(&shifted_palette[0], &pal_bg_mem[SHOP_LIGHTS_2_PID], 1);
    memcpy16(&shifted_palette[1], &pal_bg_mem[SHOP_LIGHTS_3_PID], 1);
    memcpy16(&shifted_palette[2], &pal_bg_mem[SHOP_LIGHTS_4_PID], 1);
    memcpy16(&shifted_palette[3], &pal_bg_mem[SHOP_LIGHTS_1_PID], 1);

    // Circularly shift the palette
    int last = shifted_palette[3];

    for (int i = 3; i > 0; --i)
    {
        shifted_palette[i] = shifted_palette[i - 1];
    }

    shifted_palette[0] = last;

    memcpy16(&pal_bg_mem[SHOP_LIGHTS_2_PID], &shifted_palette[0], 1); // Copy the shifted palette to the next 4 slots
    memcpy16(&pal_bg_mem[SHOP_LIGHTS_3_PID], &shifted_palette[1], 1);
    memcpy16(&pal_bg_mem[SHOP_LIGHTS_4_PID], &shifted_palette[2], 1);
    memcpy16(&pal_bg_mem[SHOP_LIGHTS_1_PID], &shifted_palette[3], 1);
}

// Outro sequence (menu and shop icon going out of frame)
static void game_shop_outro()
{
    // Shift the shop panel
    main_bg_se_move_rect_1_tile_vert(POP_MENU_ANIM_RECT, SE_DOWN);

    main_bg_se_copy_rect_1_tile_vert(TOP_LEFT_PANEL_ANIM_RECT, SE_UP);

    // TODO: make heads or tails of what's going on here and replace
    // magic numbers.
    if (timer == 1)
    {
        tte_erase_rect_wrapper(SHOP_PRICES_TEXT_RECT); // Erase the shop prices text

        ListItr itr = list_itr_new(&_shop_jokers_list);
        ListNode* ln;
        while((ln = list_itr_next(&itr)))
        {
            JokerObject *joker_object = POOL_AT(JokerObject, ln->elem_idx);
            if (joker_object != NULL)
            {
                joker_object->sprite_object->ty = int2fx(160);
            }
        }

        reset_top_left_panel_bottom_row();
    }
    else if (timer == 2)
    {
        int y = 5;
        memset16(&se_mat[MAIN_BG_SBB][y - 1][0], 0x0001, 1);
        memset16(&se_mat[MAIN_BG_SBB][y - 1][1], 0x0002, 7);
        memset16(&se_mat[MAIN_BG_SBB][y - 1][8], SE_HFLIP | 0x0001, 1);
    }

    if (timer >= MENU_POP_OUT_ANIM_FRAMES)
    {
        state_info[game_state].substate = GAME_SHOP_MAX; // Go to the next state
        timer = TM_ZERO; // Reset the timer
    }
}

static void game_shop_on_update()
{
    change_background(BG_ID_SHOP);

    if (!list_is_empty(_shop_jokers_list))
    {
        ListItr itr = list_itr_new(&_shop_jokers_list);
        ListNode* ln;
        while((ln = list_itr_next(&itr)))
        {
            JokerObject *joker_object = POOL_AT(JokerObject, ln->elem_idx);
            if (joker_object != NULL)
            {
                joker_object_update(joker_object);
            }
        }
    }

    if (timer % 20 == 0)
    {
        game_shop_lights_anim_frame();
    }

    if (state_info[game_state].substate == GAME_SHOP_MAX)
    {
        game_change_state(GAME_STATE_BLIND_SELECT);
        return;
    }

    int substate = state_info[game_state].substate;

    shop_state_actions[substate]();
}

static void game_shop_on_exit()
{
    ListItr itr = list_itr_new(&_shop_jokers_list);
    ListNode* ln;

    while((ln = list_itr_next(&itr)))
    {
        //JokerObject* joker_object = list_get(_shop_jokers_list, i);
        JokerObject* joker_object = POOL_AT(JokerObject, ln->elem_idx);
        if (joker_object != NULL)
        {
            //_shop_jokers_list
            //list_push_back(List *p_list, int elem_idx);
            // Make the joker available back to shop
            //int_list_append(jokers_available_to_shop, (intptr_t)joker_object->joker->id);
            list_push_back(&_avail_shop_jokers_list, joker_object->joker->id);
        }
        joker_object_destroy(&joker_object); // Destroy the joker objects
    }

    list_destroy(&_shop_jokers_list);
    
    increment_blind(BLIND_STATE_DEFEATED); // TODO: Move to game_round_end()?
}

static void game_blind_select_on_update()
{
    if (state_info[game_state].substate == BLIND_SELECT_MAX)
    {
        game_change_state(GAME_STATE_PLAYING);
        return;
    }

    int substate = state_info[game_state].substate;
    blind_select_state_actions[substate]();
}

static void game_blind_select_start_anim_seq()
{
    change_background(BG_ID_BLIND_SELECT);
    main_bg_se_copy_rect_1_tile_vert(POP_MENU_ANIM_RECT, SE_UP);
    
    for (int i = 0; i < BLIND_TYPE_MAX; i++)
    {
        sprite_position(blind_select_tokens[i], blind_select_tokens[i]->pos.x, blind_select_tokens[i]->pos.y - TILE_SIZE);
    }
    
    if (timer == TM_END_ANIM_SEQ)
    {
        state_info[game_state].substate = BLIND_SELECT;
        timer = TM_ZERO; // Reset the timer
    }
}

static void game_blind_select_handle_input()
{
    if (timer == TM_BLIND_SELECT_START && current_blind == BLIND_TYPE_BOSS)
    {
        selection_y = 0;
    }
    
    // Blind select input logic
    if (key_hit(KEY_UP))
    {
        selection_y = 0;
    }
    else if (key_hit(KEY_DOWN) && current_blind != BLIND_TYPE_BOSS)
    {
        selection_y = 1;
    }
    else if (key_hit(SELECT_CARD))
    {
        if (selection_y == 0) // Blind selected
        {
            state_info[game_state].substate = BLIND_SELECTED_ANIM_SEQ;
            timer = TM_ZERO;
            display_round(++round);
        }
        else if (current_blind != BLIND_TYPE_BOSS)
        {
            increment_blind(BLIND_STATE_SKIPPED);
            
            background = UNDEFINED; // Force refresh of the background
            change_background(BG_ID_BLIND_SELECT);
    
            // TODO: Create a generic vertical move by any number of tiles to avoid for loops?
            for (int i = 0; i < 12; i++)
            {
                main_bg_se_copy_rect_1_tile_vert(POP_MENU_ANIM_RECT, SE_UP);
            }
    
            for (int i = 0; i < BLIND_TYPE_MAX; i++)
            {
                sprite_position(blind_select_tokens[i], blind_select_tokens[i]->pos.x, blind_select_tokens[i]->pos.y - (TILE_SIZE * 12));
            }
    
            timer = TM_ZERO;
        }
    }
    
    if (selection_y == 0)
    {
    	// 5 is the multiplier palette color and the skip button color
        memset16(&pal_bg_mem[BLIND_SELECT_BTN_SELECTED_BORDER_PID], 0xFFFF, 1);
        memcpy16(&pal_bg_mem[BLIND_SKIP_BTN_SELECTED_BORDER_PID], &pal_bg_mem[BLIND_SKIP_BTN_PID], 1);
    }
    else
    {
    	// 15 is the select button color
        memcpy16(&pal_bg_mem[BLIND_SELECT_BTN_SELECTED_BORDER_PID], &pal_bg_mem[BLIND_SELECT_BTN_PID], 1);
        memset16(&pal_bg_mem[BLIND_SKIP_BTN_SELECTED_BORDER_PID], 0xFFFF, 1);
    }
}

static void game_blind_select_selected_anim_seq()
{
    if (timer < 15)
    {
        Rect blinds_rect = POP_MENU_ANIM_RECT;
        blinds_rect.top -= 1; // Because of the raised blind
        main_bg_se_move_rect_1_tile_vert(blinds_rect, SE_DOWN);
    
        for (int i = 0; i < BLIND_TYPE_MAX; i++)
        {
            sprite_position(blind_select_tokens[i], blind_select_tokens[i]->pos.x, blind_select_tokens[i]->pos.y + TILE_SIZE);
        }
    }
    else if (timer >= MENU_POP_OUT_ANIM_FRAMES)
    {
        for (int i = 0; i < BLIND_TYPE_MAX; i++)
        {
            obj_hide(blind_select_tokens[i]->obj);
        }
    
        state_info[game_state].substate = DISPLAY_BLIND_PANEL; // Reset the state
        timer = TM_ZERO; // Reset the timer
    }
}

static void game_blind_select_display_blind_panel()
{
    if (timer >= TM_DISP_BLIND_PANEL_FINISH)
    {
    	state_info[game_state].substate = BLIND_SELECT_MAX;
    	return;
    }
    
    if (timer == TM_DISP_BLIND_PANEL_START) // Switches to the selecting background and clears the blind panel area
    {
        change_background(BG_ID_CARD_SELECTING);
    
        main_bg_se_clear_rect(ROUND_END_MENU_RECT);
    
        for (int y = 0; y < 5; y++)
        {
            int y_from = 28;
            int y_to = 0 + y;
    
            Rect from = {0, y_from, 8, y_from + 1};
            BG_POINT to = {0, y_to};
    
            main_bg_se_copy_rect(from, to);
        }
    
        reset_top_left_panel_bottom_row();
    }
    
    for (int y = 0; y < timer; y++) // Shift the blind panel down onto screen
    {
        int y_from = 26 + y - timer;
        int y_to = 0 + y;
    
        Rect from = {0, y_from, 8, y_from};
        BG_POINT to = {0, y_to};
    
        main_bg_se_copy_rect(from, to);
    }
}

static void game_blind_select_on_exit()
{
    selection_y = 0;
    background = UNDEFINED;
}

static void game_main_menu_on_update()
{
    change_background(BG_ID_MAIN_MENU);

    card_object_update(main_menu_ace);
    main_menu_ace->sprite_object->trotation = lu_sin((timer << 8) / 2) / 3;
    main_menu_ace->sprite_object->rotation = main_menu_ace->sprite_object->trotation;

    // Seed randomization
    rng_seed++;
    if (key_curr_state() != key_prev_state()) // If the keys have changed, make it more pseudo-random
    {
        rng_seed *= 2;
    }

    if (key_hit(KEY_LEFT))
    {
        if (selection_x > 0)
        {
            selection_x--;
        }
    }
    else if (key_hit(KEY_RIGHT))
    {
        if (selection_x < MAIN_MENU_IMPLEMENTED_BUTTONS - 1)
        {
            selection_x++;
        }
    }
    
    if (selection_x == 0) // Play button
    {   
        // Select button PID is 5 and the outline is 3
        memset16(&pal_bg_mem[MAIN_MENU_PLAY_BUTTON_OUTLINE_PID], HIGHLIGHT_COLOR, 1);

        if (key_hit(KEY_A))
        {
            // Start the game
            game_start();
        }
    }
    else
    {
        // Select button PID is 5 and the outline is 3
        memcpy16(&pal_bg_mem[MAIN_MENU_PLAY_BUTTON_OUTLINE_PID], &pal_bg_mem[MAIN_MENU_PLAY_BUTTON_MAIN_COLOR_PID], 1);
    }
}

static void discarded_jokers_update_loop()
{
    if(list_is_empty(_discarded_jokers_list)) {
        return;
    }

    ListItr itr = list_itr_new(&_discarded_jokers_list);
    ListNode* ln;
    while((ln = list_itr_next(&itr)))
    {
        JokerObject* joker_object = POOL_AT(JokerObject, ln->elem_idx);

        joker_object_update(joker_object);
        if (joker_object->sprite_object->x == joker_object->sprite_object->tx
            && joker_object->sprite_object->y == joker_object->sprite_object->ty)
        {
            list_remove_node(&_discarded_jokers_list, ln);
            joker_object_destroy(&joker_object);
        }

    }
}

static void held_jokers_update_loop()
{
    const int spacing_lut[MAX_JOKERS_HELD_SIZE][MAX_JOKERS_HELD_SIZE] = 
    {
        {0, 0, 0, 0, 0},
        {13, -13, 0, 0, 0},
        {26, 0, -26, 0, 0},
        {39, 13, -13, -39, 0},
        {40, 20, 0, -20, -40}
    };

    FIXED hand_x = int2fx(HELD_JOKERS_POS.x);

    int jokers_top = list_get_len(_all_jokers_list) - 1;
    for (int i = jokers_top; i >= 0; i--)
    {
        JokerObject *joker = POOL_AT(JokerObject, list_get_at_idx(_all_jokers_list, i));
        joker->sprite_object->tx = hand_x - int2fx(spacing_lut[jokers_top][i]);

        joker_object_update(joker);
    }
}

static void jokers_update_loop()
{
    held_jokers_update_loop();
    discarded_jokers_update_loop();
}

static void game_over_anim_frame()
{
    main_bg_se_move_rect_1_tile_vert(GAME_OVER_ANIM_RECT, SE_UP);
}

static void game_lose_on_update()
{
    if (timer < GAME_OVER_ANIM_FRAMES)
    {
        game_over_anim_frame();
    }
    else if (timer == GAME_OVER_ANIM_FRAMES)
    {
        tte_printf("#{P:%d,%d; cx:0x%X000}GAME OVER", GAME_LOSE_MSG_TEXT_RECT.left, GAME_LOSE_MSG_TEXT_RECT.top, TTE_RED_PB);
    }

    if (key_hit(KEY_ANY)) game_change_state(GAME_STATE_BLIND_SELECT);
}

// This function isn't set in stone. This is just a placeholder
// allowing the player to restart the game. Thought it would be nice to have
// util we decide what we want to do after a game over.
static void game_over_on_exit()
{
    ListItr itr = list_itr_new(&_all_jokers_list);
    ListNode* ln;

    while((ln = list_itr_next(&itr)))
    {
        JokerObject* joker_object = POOL_AT(JokerObject, ln->elem_idx);
        joker_object_destroy(&joker_object);
    }

    tte_erase_screen();

    // For some reason that I haven't figured out yet,
    // if I don't destroy the blind tokens they won't
    // show up on the next run.
    sprite_destroy(&playing_blind_token);
    sprite_destroy(&round_end_blind_token);
    sprite_destroy(&blind_select_tokens[BLIND_TYPE_SMALL]);
    sprite_destroy(&blind_select_tokens[BLIND_TYPE_BIG]);
    sprite_destroy(&blind_select_tokens[BLIND_TYPE_BOSS]);
    list_destroy(&_avail_shop_jokers_list);

    game_init();

    display_round(round);
    display_score(score);
    display_chips(chips);
    display_mult(mult);
    display_hands(hands);
    display_discards(discards);
    display_money(money);
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}%d#{cx:0x%X000}/%d",
        ANTE_TEXT_RECT.left,
        ANTE_TEXT_RECT.top,
        TTE_YELLOW_PB,
        ante,
        TTE_WHITE_PB,
        MAX_ANTE
    ); // Ante

    affine_background_load_palette(affine_background_gfxPal);
}

static void game_win_on_update()
{
    if (timer < GAME_OVER_ANIM_FRAMES)
    {
        game_over_anim_frame();
    }
    else if (timer == GAME_OVER_ANIM_FRAMES)
    {
        tte_printf("#{P:%d,%d; cx:0x%X000}YOU WIN", GAME_WIN_MSG_TEXT_RECT.left, GAME_WIN_MSG_TEXT_RECT.top, TTE_BLUE_PB);
    }

    if (key_hit(KEY_ANY)) game_change_state(GAME_STATE_BLIND_SELECT);
}

void game_update()
{
    timer++;

    jokers_update_loop();

    state_info[game_state].on_update();
}
