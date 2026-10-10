#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using lv_color_t = std::uint32_t;
inline constexpr int LV_COLOR_FORMAT_ARGB8888 = 1;
inline constexpr int LV_OPA_TRANSP = 0;
inline constexpr int LV_OPA_COVER = 255;
inline constexpr int LV_OPA_90 = 230;
inline constexpr int LV_RADIUS_CIRCLE = 32767;
inline constexpr int LV_PALETTE_GREEN = 1;
inline constexpr int LV_PALETTE_BLUE = 2;
inline constexpr int LV_PART_MAIN = 0;
inline constexpr int LV_TEXT_ALIGN_CENTER = 0;
inline constexpr int LV_ALIGN_CENTER = 0;

struct lv_area_t
{
    int x1;
    int y1;
    int x2;
    int y2;
};
struct _lv_draw_buf_t
{
    int width;
    int height;
};
using lv_draw_buf_t = _lv_draw_buf_t;
struct _lv_obj_t
{
    enum class Kind
    {
        Screen,
        Ring,
        Canvas,
        Label
    };
    Kind kind;
    lv_draw_buf_t* draw_buf = nullptr;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int moves = 0;
    lv_area_t rectangle{};
    std::string text;
};
using lv_obj_t = _lv_obj_t;
struct lv_layer_t
{
    lv_obj_t* canvas;
};
struct lv_draw_rect_dsc_t
{
    int bg_opa;
    lv_color_t bg_color;
    int border_width;
    int border_opa;
    lv_color_t border_color;
    int radius;
};

namespace host_lvgl
{
inline std::vector<lv_obj_t*> objects;
inline int live_buffers = 0;
inline int object_deletions = 0;
inline int buffer_deletions = 0;
inline int allocation_budget = -1;

inline bool allocate()
{
    if (allocation_budget == 0)
    {
        return false;
    }
    if (allocation_budget > 0)
    {
        --allocation_budget;
    }
    return true;
}

inline lv_obj_t* create(lv_obj_t::Kind kind)
{
    if (!allocate())
    {
        return nullptr;
    }
    auto* object = new lv_obj_t{kind, nullptr, 0, 0, 0, 0, 0, {}, {}};
    objects.push_back(object);
    return object;
}

inline lv_obj_t* find(lv_obj_t::Kind kind)
{
    const auto it = std::ranges::find_if(objects, [kind](const auto* object) { return object->kind == kind; });
    return it == objects.end() ? nullptr : *it;
}
} // namespace host_lvgl

inline lv_obj_t* lv_screen_active()
{
    static lv_obj_t screen{lv_obj_t::Kind::Screen, nullptr, 0, 0, 0, 0, 0, {}, {}};
    return &screen;
}
inline lv_obj_t* lv_obj_create(lv_obj_t*)
{
    return host_lvgl::create(lv_obj_t::Kind::Ring);
}
inline lv_obj_t* lv_canvas_create(lv_obj_t*)
{
    return host_lvgl::create(lv_obj_t::Kind::Canvas);
}
inline lv_obj_t* lv_label_create(lv_obj_t*)
{
    return host_lvgl::create(lv_obj_t::Kind::Label);
}
inline void lv_obj_delete(lv_obj_t* object)
{
    const auto it = std::ranges::find(host_lvgl::objects, object);
    assert(it != host_lvgl::objects.end());
    host_lvgl::objects.erase(it);
    ++host_lvgl::object_deletions;
    delete object;
}
inline lv_draw_buf_t* lv_draw_buf_create(int width, int height, int, int)
{
    if (!host_lvgl::allocate())
    {
        return nullptr;
    }
    ++host_lvgl::live_buffers;
    return new lv_draw_buf_t{width, height};
}
inline void lv_draw_buf_destroy(lv_draw_buf_t* buffer)
{
    for (const auto* object : host_lvgl::objects)
    {
        assert(object->draw_buf != buffer);
    }
    --host_lvgl::live_buffers;
    ++host_lvgl::buffer_deletions;
    delete buffer;
}
inline void lv_canvas_set_draw_buf(lv_obj_t* object, lv_draw_buf_t* buffer)
{
    object->draw_buf = buffer;
}
inline void lv_canvas_fill_bg(lv_obj_t*, lv_color_t, int)
{
}
inline void lv_canvas_init_layer(lv_obj_t* object, lv_layer_t* layer)
{
    layer->canvas = object;
}
inline void lv_canvas_finish_layer(lv_obj_t*, lv_layer_t*)
{
}
inline void lv_draw_rect_dsc_init(lv_draw_rect_dsc_t* descriptor)
{
    *descriptor = {};
}
inline void lv_draw_rect(lv_layer_t* layer, const lv_draw_rect_dsc_t*, const lv_area_t* area)
{
    layer->canvas->rectangle = *area;
}
inline lv_color_t lv_color_black()
{
    return 0;
}
inline lv_color_t lv_color_white()
{
    return 0xFFFFFF;
}
inline lv_color_t lv_palette_main(int palette)
{
    return palette;
}
inline lv_color_t lv_palette_darken(int palette, int)
{
    return palette;
}
inline void lv_obj_set_pos(lv_obj_t* object, int x, int y)
{
    object->x = x;
    object->y = y;
    ++object->moves;
}
inline void lv_obj_set_size(lv_obj_t* object, int width, int height)
{
    object->width = width;
    object->height = height;
}
inline void lv_label_set_text(lv_obj_t* object, const char* text)
{
    object->text = text;
}
inline void lv_obj_remove_style_all(lv_obj_t*)
{
}
inline void lv_obj_set_style_radius(lv_obj_t*, int, int)
{
}
inline void lv_obj_set_style_border_width(lv_obj_t*, int, int)
{
}
inline void lv_obj_set_style_border_color(lv_obj_t*, lv_color_t, int)
{
}
inline void lv_obj_set_style_border_opa(lv_obj_t*, int, int)
{
}
inline void lv_obj_set_style_text_color(lv_obj_t*, lv_color_t, int)
{
}
inline void lv_obj_set_style_text_align(lv_obj_t*, int, int)
{
}
inline void lv_obj_align(lv_obj_t*, int, int, int)
{
}
