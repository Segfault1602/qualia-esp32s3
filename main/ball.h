#pragma once

#include <box2d/box2d.h>
#include <esp_err.h>
#include <lvgl.h>

typedef struct _ball_t
{
    int x;
    int y;
    lv_obj_t* obj;
    b2Body* body;
} ball_t;

esp_err_t ball_create(ball_t* ball, float x, float y, b2World& world);
void ball_destroy(ball_t* ball);

void ball_draw(ball_t* ball);
