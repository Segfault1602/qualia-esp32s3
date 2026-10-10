#pragma once

#include <box2d/box2d.h>
#include <esp_err.h>

#include "gui/ball_view.h"

typedef struct _ball_t
{
    gui::BallView view;
    b2BodyId body;
} ball_t;

esp_err_t ball_create(ball_t* ball, float x, float y, b2WorldId world);

void ball_draw(ball_t* ball);