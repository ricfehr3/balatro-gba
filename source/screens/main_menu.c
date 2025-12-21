#include "affine_background.h"
#include "card.h"
#include "game.h"
#include "main_menu.h"
#include "audio_utils.h"
#include "soundbank.h"

static const BG_POINT MAIN_MENU_ACE_T = {88, 26};
static CardObject* main_menu_ace = NULL;

 // Remove this once all buttons are implemented
static const int MAIN_MENU_IMPLEMENTED_BUTTONS = 1;
static const int MAIN_MENU_PLAY_BTN_IDX = 0;

void game_main_menu_on_init(void)
{
    affine_background_change_background(AFFINE_BG_MAIN_MENU);
    change_background(BG_MAIN_MENU);
    main_menu_ace = card_object_new(card_new(SPADES, ACE));
    card_object_set_sprite(main_menu_ace, 0); // Set the sprite for the ace of spades
    main_menu_ace->sprite_object->sprite->obj->attr0 |=
        ATTR0_AFF_DBL; // Make the sprite double sized
    main_menu_ace->sprite_object->tx = int2fx(MAIN_MENU_ACE_T.x);
    main_menu_ace->sprite_object->x = main_menu_ace->sprite_object->tx;
    main_menu_ace->sprite_object->ty = int2fx(MAIN_MENU_ACE_T.y);
    main_menu_ace->sprite_object->y = main_menu_ace->sprite_object->ty;
    main_menu_ace->sprite_object->tscale = float2fx(0.8f);
}

void game_main_menu_on_update(void)
{
    GameVars* vars = get_game_vars();
    change_background(BG_MAIN_MENU);

    card_object_update(main_menu_ace);
    main_menu_ace->sprite_object->trotation = lu_sin((vars->frame << 8) / 2) / 3;
    main_menu_ace->sprite_object->rotation = main_menu_ace->sprite_object->trotation;

    // Seed randomization
    vars->rng_seed++;
    // If the keys have changed, make it more pseudo-random
    if (key_curr_state() != key_prev_state())
    {
        vars->rng_seed *= 2;
    }

    if (key_hit(KEY_LEFT))
    {
        if (vars->selection_x > 0)
        {
            vars->selection_x--;
        }
    }
    else if (key_hit(KEY_RIGHT))
    {
        if (vars->selection_x < MAIN_MENU_IMPLEMENTED_BUTTONS - 1)
        {
            vars->selection_x++;
        }
    }

    if (vars->selection_x == MAIN_MENU_PLAY_BTN_IDX)
    {
        memset16(&pal_bg_mem[MAIN_MENU_PLAY_BUTTON_OUTLINE_PID], HIGHLIGHT_COLOR, 1);

        if (key_hit(SELECT_CARD))
        {
            play_sfx(SFX_BUTTON, MM_BASE_PITCH_RATE, BUTTON_SFX_VOLUME);
            //game_start();
        }
    }
    else
    {
        memcpy16(
            &pal_bg_mem[MAIN_MENU_PLAY_BUTTON_OUTLINE_PID],
            &pal_bg_mem[MAIN_MENU_PLAY_BUTTON_MAIN_COLOR_PID],
            1
        );
    }
}

// main menu ace in game.c needs to be removed, but it is destroyed in game_start
// So obviously need a destroy callback here, and also some way to call game_start
// from elsewhere. It needs to be in it's own set of callbacks I'm pretty sure.
// Yeah, make game_start a state, and have it do it's usual thing, besides the
// destroy function. Maybe instead make it easier by having the intialization of
// the blind screen do the other part after the destroy
//
// And also, add a game_reset, that does all the card setting up.
