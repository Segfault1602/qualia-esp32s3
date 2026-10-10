#pragma once

#include <box2d/box2d.h>
#include <esp_err.h>

#include "gui/ball_view.h"

namespace app
{
/**
 * @brief Owns a physics ball and its view. The world must outlive the ball.
 *
 * Create, draw, and destroy on the UI task while the active screen is valid.
 */
class Ball
{
  public:
    Ball() = default;
    ~Ball();
    Ball(const Ball&) = delete;
    Ball& operator=(const Ball&) = delete;

    esp_err_t create(b2WorldId world, b2Vec2 position, bool report_hits = false);
    esp_err_t draw();
    void destroy();

    b2BodyId body() const
    {
        return body_;
    }

  private:
    gui::BallView view_;
    b2BodyId body_ = b2_nullBodyId;
};
} // namespace app