#pragma once

#include <algorithm>
#include <cmath>

namespace velox {

/**
 * Physically-motivated critically damped spring animator (Workflow 8).
 * Honors OS/application reduced-motion preferences (Step 6): when reduced_motion
 * is active, transitions snap immediately to the target state without oscillation.
 */
class SpringAnimator {
public:
    constexpr SpringAnimator(double initial = 0.0,
                             double stiffness = 220.0,
                             double damping = 28.0) noexcept
        : position_(initial),
          target_(initial),
          velocity_(0.0),
          stiffness_(stiffness),
          damping_(damping) {}

    constexpr void setTarget(double target) noexcept {
        target_ = target;
    }

    constexpr void snapTo(double value) noexcept {
        position_ = value;
        target_ = value;
        velocity_ = 0.0;
    }

    constexpr double step(double dt_sec = 1.0 / 60.0, bool reduced_motion = false) noexcept {
        if (reduced_motion) {
            snapTo(target_);
            return position_;
        }
        double displacement = position_ - target_;
        double spring_force = -stiffness_ * displacement;
        double damping_force = -damping_ * velocity_;
        double acceleration = spring_force + damping_force;
        velocity_ += acceleration * dt_sec;
        position_ += velocity_ * dt_sec;
        double diff_pos = position_ - target_;
        double abs_pos = diff_pos < 0.0 ? -diff_pos : diff_pos;
        double abs_vel = velocity_ < 0.0 ? -velocity_ : velocity_;
        if (abs_pos < 1e-4 && abs_vel < 1e-4) {
            position_ = target_;
            velocity_ = 0.0;
        }
        return position_;
    }

    [[nodiscard]] constexpr double value() const noexcept { return position_; }
    [[nodiscard]] constexpr double target() const noexcept { return target_; }
    [[nodiscard]] constexpr bool isSettled() const noexcept {
        double diff_pos = position_ - target_;
        double abs_pos = diff_pos < 0.0 ? -diff_pos : diff_pos;
        double abs_vel = velocity_ < 0.0 ? -velocity_ : velocity_;
        return abs_pos < 1e-3 && abs_vel < 1e-3;
    }

    [[nodiscard]] static constexpr double cubicEaseOut(double t) noexcept {
        double clamped = std::clamp(t, 0.0, 1.0);
        double inv = 1.0 - clamped;
        return 1.0 - (inv * inv * inv);
    }

    [[nodiscard]] static constexpr double catmullRom(double p0, double p1, double p2, double p3, double t) noexcept {
        double t2 = t * t;
        double t3 = t2 * t;
        return 0.5 * ((2.0 * p1) +
                      (-p0 + p2) * t +
                      (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                      (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
    }

private:
    double position_;
    double target_;
    double velocity_;
    double stiffness_;
    double damping_;
};

} // namespace velox
