#include "played_cards.h"

#include "audio_utils.h"
#include "card.h"
#include "game.h"
#include "hand.h"
#include "joker.h"
#include "soundbank.h"
#include "state_machine.h"
#include "timer.h"
#include "util.h"

static const BG_POINT HAND_PLAY_POS = {120, 70};
static const BG_POINT CARD_DISCARD_PNT      = {240,     70};
static const Rect PLAYED_CARDS_SCORES_RECT  = {72,      48,     240,    56  };
static const Rect HELD_CARDS_SCORES_RECT    = {72,      108,    240,    116 };
static const int SCORED_CARD_TEXT_Y = 48;

static void play_starting(void);
static void play_before_starting(void);
static void play_scoring_cards(void);
static void play_scoring_card_jokers(void);
static void play_scoring_held_cards(void);
static void play_scoring_independent_jokers(void);
static void play_scoring_hand_scored_end(void);
static void play_ending(void);
static void play_ended(void);

static StateInfo state_info[] = {
    STATE_INFO_UPDATE_FN_ONLY(play_starting),
    STATE_INFO_UPDATE_FN_ONLY(play_before_starting),
    STATE_INFO_UPDATE_FN_ONLY(play_scoring_cards),
    STATE_INFO_UPDATE_FN_ONLY(play_scoring_card_jokers),
    STATE_INFO_UPDATE_FN_ONLY(play_scoring_held_cards),
    STATE_INFO_UPDATE_FN_ONLY(play_scoring_independent_jokers),
    STATE_INFO_UPDATE_FN_ONLY(play_scoring_hand_scored_end),
    STATE_INFO_UPDATE_FN_ONLY(play_ending),
    STATE_INFO_UPDATE_FN_ONLY(play_ended),
};

static StateMachine play_sm = STATE_MACHINE_DEFINE(state_info, PLAY_STATE_MAX);

static void play_starting(void)
{
    auto played = get_played_array();
    auto played_top = get_played_top();
    auto scored_card_index = get_scored_card_index();

    bool card_selected = card_object_is_selected(played[played_top - scored_card_index]);

    for (int played_idx = 0; played_idx <= played_top; played_idx++)
    {
        if (played_idx == played_top && (g_game_vars.timer % FRAMES(10) == 0 || !card_selected) &&
            g_game_vars.timer > FRAMES(40))
        {
            scored_card_index--;

            if (scored_card_index == 0)
            {
                *joker_scored_itr() = list_itr_create(owned_jokers_list());
                g_game_vars.timer = TM_ZERO;
                state_machine_change_state(&play_sm, PLAY_BEFORE_SCORING);
            }
        }

        played[played_idx]->sprite_object->tx =
            int2fx(HAND_PLAY_POS.x) +
            (int2fx(played_top - played_idx) - int2fx(played_top) / 2) * -27;
        played[played_idx]->sprite_object->ty = int2fx(HAND_PLAY_POS.y);

        card_selected = card_object_is_selected(played[played_idx]);
        if (card_selected && played_top - played_idx >= scored_card_index)
        {
            played[played_idx]->sprite_object->ty -= int2fx(10);
        }
    }
}

static void play_before_starting(void)
{
    // Activate Jokers with an effect just before the hand is scored
    if (check_and_score_joker_for_event(joker_scored_itr(), NULL, JOKER_EVENT_ON_HAND_PLAYED))
        state_machine_change_state(&play_sm, PLAY_SCORING_CARDS);
}

static void play_scoring_cards(void)
{
    auto played = get_played_array();
    auto played_top = get_played_top();
    auto scored_card_index = get_scored_card_index();

    if (g_game_vars.timer % FRAMES(30) == 0 && g_game_vars.timer > FRAMES(40))
    {
        // We are about to score played Cards.
        // Start from the current card index
        // and seek the next scoring card
        while (scored_card_index <= played_top &&
               !card_object_is_selected(played[scored_card_index]))
        {
            scored_card_index++;
        }

        // go to the next state if there are no cards left to score
        if (scored_card_index > played_top)
        {
            // reuse these variables for held cards
            *joker_scored_itr() = list_itr_create(owned_jokers_list());
            scored_card_index = get_hand_top();

            state_machine_change_state(&play_sm, PLAY_SCORING_HELD_CARDS);

            return;
        }

        tte_erase_rect_wrapper(PLAYED_CARDS_SCORES_RECT);

        CardObject* scored_card_object = played[scored_card_index];

        if (card_object_is_selected(scored_card_object))
        {
            // Offset of 1 tile to keep the text on the card
            tte_set_pos(
                fx2int(scored_card_object->sprite_object->x) + TILE_SIZE,
                SCORED_CARD_TEXT_Y
            );

            // Set text color to blue from background memory
            tte_set_special(TTE_BLUE_PB * TTE_SPECIAL_PB_MULT_OFFSET);

            u8 card_value = card_get_value(scored_card_object->card);

            // Write the score to a character buffer variable
            char score_buffer[INT_MAX_DIGITS + 2]; // for '+' and null terminator
            snprintf(score_buffer, sizeof(score_buffer), "+%hhu", card_value);
            tte_write(score_buffer);

            card_object_shake(scored_card_object, SFX_CHIPS_CARD);

            // Relocated card scoring logic here
            set_chips(u32_protected_add(get_chips(), card_value));
            display_chips();

            // Allow Joker scoring
            *joker_scored_itr() = list_itr_create(owned_jokers_list());
            *joker_card_scored_end_itr() = list_itr_create(owned_jokers_list());
        }

        state_machine_change_state(&play_sm, PLAY_SCORING_CARD_JOKERS);
    }
}

static void play_scoring_card_jokers(void)
{
    if (g_game_vars.timer % FRAMES(30) == 0 && g_game_vars.timer > FRAMES(40))
    {
        tte_erase_rect_wrapper(PLAYED_CARDS_SCORES_RECT);

        // since we sought the next scoring card index in the previous state,
        // scored_card_index is guaranteed to be a scoring card
        if (check_and_score_joker_for_event(
                joker_scored_itr(),
                get_played_array()[get_scored_card_index()],
                JOKER_EVENT_ON_CARD_SCORED
            ))
        {
            return;
        }

        // Trigger all Jokers that have an effect when a card finishes scoring
        // (e.g. retriggers) after activating all the other scored_card Jokers normally
        if (check_and_score_joker_for_event(
                joker_card_scored_end_itr(),
                get_played_array()[get_scored_card_index()],
                JOKER_EVENT_ON_CARD_SCORED_END
            ))
        {
            set_retrigger(false);
            state_machine_change_state(&play_sm, PLAY_SCORING_CARDS);
            return;
        }

        // increment index to start seeking the next scoring card from the next card
        set_scored_card_index(get_scored_card_index() + 1);
        state_machine_change_state(&play_sm, PLAY_SCORING_CARDS);
    }
}

static void play_scoring_held_cards(void)
{
    auto played_top = get_played_top();
    for (int played_idx = 0; played_idx <= played_top; played_idx++)
    {
        if (played_idx == 0 && (g_game_vars.timer % FRAMES(30) == 0) && g_game_vars.timer > FRAMES(40))
        {
            tte_erase_rect_wrapper(HELD_CARDS_SCORES_RECT);

            CardObject** hand = get_hand_array();

            // Go through all held cards and see if they activate Jokers
            for (; get_scored_card_index() >= 0; set_scored_card_index(get_scored_card_index() - 1))
            {
                if (check_and_score_joker_for_event(
                        joker_scored_itr(),
                        hand[get_scored_card_index()],
                        JOKER_EVENT_ON_CARD_HELD
                    ))
                {
                    card_object_shake(hand[get_scored_card_index()], SFX_CARD_SELECT);
                    return;
                }
                *joker_scored_itr() = list_itr_create(owned_jokers_list());
            }

            set_scored_card_index(0);
            *joker_round_end_itr() = list_itr_create(owned_jokers_list());

            state_machine_change_state(&play_sm, PLAY_SCORING_INDEPENDENT_JOKERS);
        }
    }
}
static void play_scoring_independent_jokers(void)
{
    auto played_top = get_played_top();
    for (int played_idx = 0; played_idx <= played_top; played_idx++)
    {
        if (played_idx == 0 && (g_game_vars.timer % FRAMES(30) == 0) && g_game_vars.timer > FRAMES(40))
        {

            tte_erase_rect_wrapper(PLAYED_CARDS_SCORES_RECT);

            if (check_and_score_joker_for_event(joker_scored_itr(), NULL, JOKER_EVENT_INDEPENDENT))
            {
                return;
            }

            set_scored_card_index(played_top + 1); 

            state_machine_change_state(&play_sm, PLAY_SCORING_HAND_SCORED_END);
        }
    }
}
static void play_scoring_hand_scored_end(void)
{
    auto played_top = get_played_top();
    for (int played_idx = 0; played_idx <= played_top; played_idx++)
    {
        if (played_idx == 0 && (g_game_vars.timer % FRAMES(30) == 0) && g_game_vars.timer > FRAMES(40))
        {

            tte_erase_rect_wrapper(PLAYED_CARDS_SCORES_RECT);

            bool scored = check_and_score_joker_for_event(
                joker_round_end_itr(),
                NULL,
                JOKER_EVENT_ON_HAND_SCORED_END
            );

            if (scored)
                return;

            g_game_vars.timer = TM_ZERO;
            state_machine_change_state(&play_sm, PLAY_ENDING);
        }
    }
}

static void play_ending(void)
{
    auto played_top = get_played_top();
    auto played = get_played_array();

    for (int played_idx = 0; played_idx <= played_top; played_idx++)
    {
        bool card_selected = card_object_is_selected(played[played_top - get_scored_card_index()]);
        if (played_idx == played_top && (g_game_vars.timer % FRAMES(10) == 0 || !card_selected) &&
            g_game_vars.timer > FRAMES(40))
        {
            set_scored_card_index(get_scored_card_index() - 1);

            /* SFX_CHIPS_ACCUM has been pitch shifted to perserve high frequencies in downsampling.
            * Now it needs to be pitch shifted back to the original frequency.
            */
            int static const CHIPS_ACCUM_SFX_PITCH_RATIO = 2;

            if (get_scored_card_index() == 0)
            {
                play_sfx(
                    SFX_CHIPS_ACCUM,
                    CHIPS_ACCUM_SFX_PITCH_RATIO * MM_BASE_PITCH_RATE,
                    SFX_DEFAULT_VOLUME
                );
                g_game_vars.timer = TM_ZERO;
                state_machine_change_state(&play_sm, PLAY_ENDED);
            }
        }

        if (card_object_is_selected(played[played_idx]) && played_top - played_idx >= get_scored_card_index())
        {
            played[played_idx]->sprite_object->ty = int2fx(HAND_PLAY_POS.y);
        }
    }
}

static inline bool game_round_is_over(void)
{
    return g_game_vars.hands == 0 ||
           g_game_vars.score >= blind_get_requirement(g_game_vars.current_blind, g_game_vars.ante);
}

static void play_ended(void)
{
    auto played_top = get_played_top();
    auto played = get_played_array();

    for (int played_idx = 0; played_idx <= played_top; played_idx++)
    {
        if (!get_discarded_card() && g_game_vars.timer > FRAMES(40))
        {
            // play the sound only once per card, when it is pushed off-screen to the right
            /*
            if (!sound_played)
            {
                play_sfx(
                    SFX_CARD_DRAW,
                    MM_BASE_PITCH_RATE + cards_drawn * PITCH_STEP_DISCARD_SFX,
                    SFX_DEFAULT_VOLUME
                );
                sound_played = true;
            }
            */

            // card has exited the screen, now discard it and set it to NULL
            if (played[played_idx]->sprite_object->x >= int2fx(CARD_DISCARD_PNT.x))
            {
                discard_push(played[played_idx]->card); // Push the card to the discard pile
                card_object_destroy(&played[played_idx]);

                // played_top--;
                (*get_cards_drawn())++; // This technically isn't drawing cards, I'm just reusing the variable
                //sound_played = false; // Allow for the sound for the next card to be played

                // we reached hand_top, all cards have been discarded
                if (played_idx == played_top)
                {
                    if (game_round_is_over())
                    {
                        set_hand_state(HAND_SHUFFLING);
                    }
                    else
                    {
                        set_hand_state(HAND_DRAW);
                    }

                    state_machine_change_state(&play_sm, PLAY_STARTING);
                    *get_cards_drawn() = 0;
                    hand_set_nb_selected_cards(0);
                    played_top = -1; // Reset the played stack
                    set_scored_card_index(0);
                    *joker_scored_itr() = list_itr_create(owned_jokers_list());
                    g_game_vars.timer = TM_ZERO;
                }
            }

            // put target X position off screen to the right
            played[played_idx]->sprite_object->tx = int2fx(CARD_DISCARD_PNT.x);
            set_discarded_card(true);
        }
    }
}

void playing_cards_update_run(void)
{
    state_machine_register(&play_sm);
    state_machine_change_state(&play_sm, PLAY_STARTING);
}

void playing_cards_update_stop(void)
{
    state_machine_remove(&play_sm);
}
