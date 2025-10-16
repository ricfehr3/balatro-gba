#include "graphic_utils.h"

#include "util.h"

#include <tonc_core.h>
#include <tonc_math.h>
#include <tonc_tte.h>

const Rect FULL_SCREENBLOCK_RECT = { 0, 0, SE_ROW_LEN - 1, SE_COL_LEN - 1 };

// Clips a rect of screenblock entries to a specified rect
// The bounding rect is not required to be within screenblock boundaries
static void clip_se_rect_to_bounding_rect(Rect* rect, const Rect* bounding_rect)
{
    rect->right = min(rect->right, bounding_rect->right);
    rect->bottom = min(rect->bottom, bounding_rect->bottom);
    rect->left = max(rect->left, bounding_rect->left);
    rect->top = max(rect->top, bounding_rect->top);
}

// Can be unstaticed if needed
// Clips a rect of screenblock entries to screenblock boundaries
static void clip_se_rect_to_screenblock(Rect* rect)
{
    clip_se_rect_to_bounding_rect(rect, &FULL_SCREENBLOCK_RECT);
}

SE main_bg_se_get_se(BG_POINT pos)
{
    return se_mat[MAIN_BG_SBB][pos.y][pos.x];
}

// Clips a rect of screenblock entries to be within one step of
// screenblock boundaries vertically depending on direction.
static void clip_se_rect_within_step_of_full_screen_vert(Rect* se_rect, int direction)
{
    Rect bounding_rect = FULL_SCREENBLOCK_RECT;
    if(direction == SE_UP)
    {
        bounding_rect.top += 1;
    }
    else if(direction == SE_DOWN)
    {
        bounding_rect.bottom -= 1;
    }

    clip_se_rect_to_bounding_rect(se_rect, &bounding_rect);
}

void main_bg_se_clear_rect(Rect se_rect)
{
    if(se_rect.left > se_rect.right)
        return;
    // Clip to avoid screenblock overflow
    clip_se_rect_to_screenblock(&se_rect);

    for(int y = se_rect.top; y < se_rect.bottom; y++)
    {
        memset16(&(se_mat[MAIN_BG_SBB][y][se_rect.left]), 0x0000, rect_width(&se_rect));
    }
}

// Internal static function to merge implementation of move/copy functions.
static void bg_se_copy_or_move_rect_1_tile_vert(u16 bg_sbb, Rect se_rect, int direction, bool move)
{
    if(se_rect.left > se_rect.right || (direction != SE_UP && direction != SE_DOWN))
    {
        return;
    }

    // Clip to avoid read/write overflow of the screenblock
    clip_se_rect_within_step_of_full_screen_vert(&se_rect, direction);

    int start = (direction == SE_UP) ? se_rect.top : se_rect.bottom;
    int end = (direction == SE_UP) ? se_rect.bottom : se_rect.top;

    for(int y = start; y != end - direction; y -= direction)
    {
        memcpy16(&(se_mat[bg_sbb][y + direction][se_rect.left]),
                 &se_mat[bg_sbb][y][se_rect.left],
                 rect_width(&se_rect));
    }

    if(move)
    {
        memset16(&se_mat[bg_sbb][end][se_rect.left], 0x0000, rect_width(&se_rect));
    }
}

static void main_bg_se_copy_or_move_rect_1_tile_vert(Rect se_rect, int direction, bool move)
{
    bg_se_copy_or_move_rect_1_tile_vert(MAIN_BG_SBB, se_rect, direction, move);
}

void bg_se_copy_rect_1_tile_vert(u16 bg_sbb, Rect se_rect, int direction)
{
    bg_se_copy_or_move_rect_1_tile_vert(MAIN_BG_SBB, se_rect, direction, false);
}

void bg_se_move_rect_1_tile_vert(u16 bg_sbb, Rect se_rect, int direction)
{
    bg_se_copy_or_move_rect_1_tile_vert(MAIN_BG_SBB, se_rect, direction, true);
}

void main_bg_se_copy_rect_1_tile_vert(Rect se_rect, int direction)
{
    main_bg_se_copy_or_move_rect_1_tile_vert(se_rect, direction, false);
}

void main_bg_se_move_rect_1_tile_vert(Rect se_rect, int direction)
{
    main_bg_se_copy_or_move_rect_1_tile_vert(se_rect, direction, true);
}

void main_bg_se_copy_rect(Rect se_rect, BG_POINT pos)
{
    if(se_rect.left > se_rect.right || se_rect.top > se_rect.bottom)
        return;

    // Clip to avoid screenblock overflow
    clip_se_rect_to_screenblock(&se_rect);

    int width = rect_width(&se_rect);
    int height = rect_height(&se_rect);
    SE tile_map[height][width];

    // Copy the rect to the tile map
    for(int sy = 0; sy < height; sy++)
    {
        memcpy16(&tile_map[sy][0], &se_mat[MAIN_BG_SBB][se_rect.top + sy][se_rect.left], width);
    }

    // TODO: Avoid overflow
    // Copy the tilemap to the new rect position
    for(int sy = 0; sy < height; sy++)
    {
        memcpy16(&se_mat[MAIN_BG_SBB][pos.y + sy][pos.x], &tile_map[sy][0], width);
    }
}

void main_bg_se_fill_rect_with_se(SE se, Rect se_rect)
{
    if(se_rect.left > se_rect.right || se_rect.top > se_rect.bottom)
        return;

    // Clip to avoid screenblock overflow
    clip_se_rect_to_screenblock(&se_rect);

    int width = rect_width(&se_rect);
    int height = rect_height(&se_rect);

    for(int sy = 0; sy < height; sy++)
    {
        memset16(&se_mat[MAIN_BG_SBB][se_rect.top + sy][se_rect.left], se, width);
    }
}

// Helper: Copy the corners of a 3x3 tile block
static void main_bg_se_expand_3x3_copy_corners(const Rect* se_dest_rect,
                                               const BG_POINT* src_top_left_pnt,
                                               int dest_rect_width,
                                               int dest_rect_height)
{
    SE top_left_se = se_mat[MAIN_BG_SBB][src_top_left_pnt->y][src_top_left_pnt->x];
    se_mat[MAIN_BG_SBB][se_dest_rect->top][se_dest_rect->left] = top_left_se;

    SE top_right_se = se_mat[MAIN_BG_SBB][src_top_left_pnt->y][src_top_left_pnt->x + 2];
    se_mat[MAIN_BG_SBB][se_dest_rect->top][se_dest_rect->left + dest_rect_width - 1] = top_right_se;

    SE bottom_left_se = se_mat[MAIN_BG_SBB][src_top_left_pnt->y + 2][src_top_left_pnt->x];
    se_mat[MAIN_BG_SBB][se_dest_rect->top + dest_rect_height - 1][se_dest_rect->left] = bottom_left_se;

    SE bottom_right_se = se_mat[MAIN_BG_SBB][src_top_left_pnt->y + 2][src_top_left_pnt->x + 2];
    se_mat[MAIN_BG_SBB][se_dest_rect->top + dest_rect_height - 1][se_dest_rect->left + dest_rect_width - 1] =
        bottom_right_se;
}

// Helper: Copy the top and bottom sides of a 3x3 tile block
static void
main_bg_se_expand_3x3_copy_top_bottom(const Rect* se_dest_rect, const BG_POINT* src_top_left_pnt, int dest_rect_width)
{
    if(dest_rect_width > 2)
    {
        SE top_middle_se = se_mat[MAIN_BG_SBB][src_top_left_pnt->y][src_top_left_pnt->x + 1];
        SE bottom_middle_se = se_mat[MAIN_BG_SBB][src_top_left_pnt->y + 2][src_top_left_pnt->x + 1];
        memset16(&se_mat[MAIN_BG_SBB][se_dest_rect->top][se_dest_rect->left + 1], top_middle_se, dest_rect_width - 2);
        memset16(&se_mat[MAIN_BG_SBB][se_dest_rect->bottom][se_dest_rect->left + 1],
                 bottom_middle_se,
                 dest_rect_width - 2);
    }
}

// Helper: Copy the left and right sides of a 3x3 tile block
static void main_bg_se_expand_3x3_copy_left_right(const Rect* se_dest_rect,
                                                  const BG_POINT* src_top_left_pnt,
                                                  int dest_rect_width,
                                                  int dest_rect_height)
{
    SE middle_left_se = se_mat[MAIN_BG_SBB][src_top_left_pnt->y + 1][src_top_left_pnt->x];
    SE middle_right_se = se_mat[MAIN_BG_SBB][src_top_left_pnt->y + 1][src_top_left_pnt->x + 2];
    for(int y = 1; y < dest_rect_height - 1; y++)
    {
        se_mat[MAIN_BG_SBB][se_dest_rect->top + y][se_dest_rect->left] = middle_left_se;
        se_mat[MAIN_BG_SBB][se_dest_rect->top + y][se_dest_rect->left + dest_rect_width - 1] = middle_right_se;
    }
}

void main_bg_se_copy_expand_3x3_rect(Rect se_dest_rect, BG_POINT src_top_left_pnt)
{
    clip_se_rect_to_screenblock(&se_dest_rect);

    int dest_rect_width = rect_width(&se_dest_rect);
    int dest_rect_height = rect_height(&se_dest_rect);

    // Verify the dest rect is at least 2x2
    if(dest_rect_width < 2 || dest_rect_height < 2)
    {
        return;
    }

    // Copy the corners
    main_bg_se_expand_3x3_copy_corners(&se_dest_rect, &src_top_left_pnt, dest_rect_width, dest_rect_height);

    // Copy top and bottom sides
    main_bg_se_expand_3x3_copy_top_bottom(&se_dest_rect, &src_top_left_pnt, dest_rect_width);

    // Copy left and right sides
    main_bg_se_expand_3x3_copy_left_right(&se_dest_rect, &src_top_left_pnt, dest_rect_width, dest_rect_height);

    // Fill the center if needed
    if(dest_rect_width > 2 && dest_rect_height > 2)
    {
        SE middle_fill_se = se_mat[MAIN_BG_SBB][src_top_left_pnt.y + 1][src_top_left_pnt.x + 1];
        Rect dest_inner_fill_rect = { se_dest_rect.left + 1,
                                      se_dest_rect.top + 1,
                                      se_dest_rect.right - 1,
                                      se_dest_rect.bottom - 1 };
        main_bg_se_fill_rect_with_se(middle_fill_se, dest_inner_fill_rect);
    }
}

void tte_erase_rect_wrapper(Rect rect)
{
    tte_erase_rect(rect.left, rect.top, rect.right, rect.bottom);
}

void update_text_rect_to_right_align_num(Rect* rect, int num, int overflow_direction)
{
    int num_digits = get_digits(num);
    if(overflow_direction == OVERFLOW_LEFT)
    {
        rect->left = max(0, rect->right - num_digits * TILE_SIZE);
    }
    else if(overflow_direction == OVERFLOW_RIGHT)
    {
        int num_fitting_digits = rect_width(rect) / TILE_SIZE;
        if(num_digits < num_fitting_digits)
            rect->left += (num_fitting_digits - num_digits) * TILE_SIZE;
        // else nothing is to be updated, entire rect is filled and may overflow
    }
}

void memcpy16_tile8_with_palette_offset(u16* dst, const u16* src, uint hwcount, u8 palette_offset)
{
    const u16 offset = (((palette_offset) << 8) | (palette_offset));
    for(int i = 0; i < hwcount; i++)
    {
        // Copying u8 data twice across u16 data
        dst[i] = src[i] + offset;
    }
}

void memcpy32_tile8_with_palette_offset(u32* dst, const u32* src, uint wcount, u8 palette_offset)
{
    const u32 offset = (palette_offset << 24) | (palette_offset << 16) | (palette_offset << 8) | palette_offset;
    for(int i = 0; i < wcount; i++)
    {
        // Copying u8 data 4 times across u32 data
        dst[i] = src[i] + offset;
    }
}

void toggle_windows(bool win0, bool win1)
{
    if(win0)
    {
        REG_DISPCNT |= DCNT_WIN0;
    }
    else
    {
        REG_DISPCNT &= ~DCNT_WIN0;
    }

    if(win1)
    {
        REG_DISPCNT |= DCNT_WIN1;
    }
    else
    {
        REG_DISPCNT &= ~DCNT_WIN1;
    }

    if(win0 || win1)
    {
        REG_BLDCNT = BLD_BUILD(BLD_BG1, BLD_BG2, 1);
    }
    else
    {
        REG_BLDCNT = 0;
    }
}
