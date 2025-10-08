#ifndef SPRITE_H
#define SPRITE_H

#include <tonc.h>
#include <maxmod.h>

#define CARD_SPRITE_SIZE 32

typedef struct 
{
    OBJ_ATTR *obj;
    OBJ_AFFINE *aff;
    POINT pos;
} Sprite;

// A sprite object is a sprite that is selectable and movable in animation
typedef struct
{
    Sprite* sprite;
    FIXED tx, ty; // target position
    FIXED x, y; // position
    FIXED vx, vy; // velocity
    FIXED tscale;
    FIXED scale;
    FIXED vscale;
    FIXED trotation; // this never gets used so i might remove it later
    FIXED rotation;
    FIXED vrotation;
    bool selected;
    bool focused;
    
} SpriteObject;

// Sprite methods
Sprite *sprite_new(u16 a0, u16 a1, u32 tid, u32 pb, int sprite_index);
Sprite *affine_sprite_new(u16 a0, u16 a1, u32 tid, u32 pb);
void sprite_destroy(Sprite **sprite);
int sprite_get_layer(Sprite *sprite);
INLINE void sprite_position(Sprite *sprite, int x, int y)
{
    sprite->pos.x = x;
    sprite->pos.y = y;

    obj_set_pos(sprite->obj, x, y);
}

// Sprite functions
void sprite_init();
void sprite_draw();
int sprite_get_pb(const Sprite* sprite);

// SpriteObject methods
SpriteObject *sprite_object_new();
void sprite_object_destroy(SpriteObject **sprite_object);
void sprite_object_set_sprite(SpriteObject* sprite_object, Sprite* sprite);
void sprite_object_reset_transform(SpriteObject* sprite_object);
void sprite_object_update(SpriteObject *sprite_object);
void sprite_object_shake(SpriteObject* sprite_object, mm_word sound_id);

void sprite_object_set_selected(SpriteObject* sprite_object, bool selected);
bool sprite_object_is_selected(SpriteObject* sprite_object);
Sprite* sprite_object_get_sprite(SpriteObject* sprite_object);
void sprite_object_set_focus(SpriteObject* sprite_object, bool focus);
bool sprite_object_is_focused(SpriteObject* sprite_object);

#endif // SPRITE_H