/**
 * @file options_menu.h
 *
 * @brief Options menu state functions.
 */
#ifndef PLAYED_CARDS_MENU_H
#define PLAYED_CARDS_MENU_H

enum PlayState
{
    PLAY_STARTING,
    PLAY_BEFORE_SCORING,
    PLAY_SCORING_CARDS,
    PLAY_SCORING_CARD_JOKERS,
    PLAY_SCORING_HELD_CARDS,
    PLAY_SCORING_INDEPENDENT_JOKERS,
    PLAY_SCORING_HAND_SCORED_END,
    PLAY_ENDING,
    PLAY_ENDED,
    PLAY_STATE_MAX,
};

void play_starting(void);
void play_before_starting(void);
void play_scoring_cards(void);
void play_scoring_card_jokers(void);
void play_scoring_held_cards(void);
void play_scoring_independent_jokers(void);
void play_scoring_hand_scored_end(void);
void play_ending(void);
void play_ended(void);

void playing_cards_update_run(void);
void playing_cards_update_stop(void);

#endif // PLAYED_CARDS_MENU_H
