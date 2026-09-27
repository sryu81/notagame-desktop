#ifndef NOT_A_GAME_ACTION_H
#define NOT_A_GAME_ACTION_H

#include <functional>
#include <memory>
#include <vector>

#include "Math.h"

namespace anim { class Clip; struct SkeletalClip; }
namespace grid { class BattleGrid; }
namespace game { class Unit; }
class SceneNode;
namespace unit {
    enum class Faction;
    struct Stats;
}



namespace action {

/*!
 * Abstract base for a unit action (move, attack, etc.). Lives on the
 * `action::Manager` queue, advances each frame via `update(dt)`, and is
 * removed when `isDone()` returns true. `onComplete()` runs exactly once
 * on the transition from not-done to done; that's where side effects
 * (cell update, damage application) belong.
 */
class Action {
public:
    virtual ~Action() = default;
    virtual void start() {}
    virtual void update(float dt) = 0;
    virtual void onComplete() {}
    virtual bool isDone() const = 0;
    virtual const char *name() const = 0;
};

/*!
 * Walks a character along an A*-computed cell path. Each frame the
 * character's SceneNode local_ is set to a TRS with the path cell's
 * world position (interpolated) as translation, identity rotation, and
 * a fixed 0.8 scale. The character's clip is suspended for the duration
 * of the move and restored on completion: on complete, the saved clip's
 * keyframe translation X/Z are overwritten with the new cell world
 * position (Y is preserved so the bob resumes at the new cell). Then
 * the character's logical `cell_` is updated to the last path cell.
 */
class MoveAction : public Action {
public:
    MoveAction(game::Unit *u, const grid::BattleGrid *g, std::vector<math::Vec2> path,
               float perCellDuration = 0.4f, float hoverY = 0.0f);

    void start() override;
    void update(float dt) override;
    void onComplete() override;
    bool isDone() const override {
        return currentSegment_ >= static_cast<int>(path_.size()) - 1;
    }
    const char *name() const override { return "Move"; }

private:
    struct RigPose {
        SceneNode* hipL  = nullptr;
        SceneNode* hipR  = nullptr;
        SceneNode* kneeL = nullptr;
        SceneNode* kneeR = nullptr;
        SceneNode* shldrL = nullptr;
        SceneNode* shldrR = nullptr;
        SceneNode* elbowL = nullptr;
        SceneNode* elbowR = nullptr;
        math::Mat4 hipLRest, hipRRest;
        math::Mat4 kneeLRest, kneeRRest;
        math::Mat4 shldrLRest, shldrRRest;
        math::Mat4 elbowLRest, elbowRRest;
    };

    game::Unit *unit_;
    const grid::BattleGrid *grid_;
    std::vector<math::Vec2> path_;
    float perCellDuration_;
    float hoverY_;

    int currentSegment_ = 0;
    float segmentElapsed_ = 0.0f;
    const anim::Clip *savedClip_ = nullptr;
    math::Vec3 originalClipTranslation_{0.0f};
    math::Vec3 endPos_{};

    math::Quat fromRot_;
    math::Quat toRot_;

    RigPose rig_;
    bool hasRig_ = false;
};

/*!
 * Plays an attack animation clip on the attacker, delivers damage at
 * the hit-frame event (id=1), then restores the attacker's original
 * clip. If no attack clip is given, falls back to a simple timer
 * (delay_ seconds). The target can optionally play a hit-reaction
 * clip (TRS or skeletal) at impact.
 *
 * Attack clip should be non-looping with a hit event at the desired
 * impact time. Duration is auto-detected from the last keyframe time.
 */
class AttackAction : public Action {
public:
    AttackAction(game::Unit *attacker, game::Unit *target, int damage,
                 const anim::Clip* attackClip = nullptr,
                 const anim::Clip* hitReactionClip = nullptr,
                 const anim::SkeletalClip* skeletalAttackClip = nullptr,
                 const anim::SkeletalClip* skeletalHitReaction = nullptr,
                 float delay = 0.0f);

    using HitCallback = std::function<void()>;

    void start() override;
    void update(float dt) override;
    void onComplete() override;
    bool isDone() const override;
    const char *name() const override { return "Attack"; }

    void setHitCallback(HitCallback cb) { hitCb_ = std::move(cb); }

private:
    void applyDamage();

    game::Unit *attacker_;
    game::Unit *target_;
    int damage_;
    const anim::Clip* attackClip_;
    const anim::Clip* hitReactionClip_;
    const anim::SkeletalClip* skeletalAttackClip_;
    const anim::SkeletalClip* skeletalHitReaction_;
    float delay_;
    float elapsed_ = 0.0f;
    bool hitDelivered_ = false;

    HitCallback hitCb_;

    const anim::Clip* savedClip_ = nullptr;
    const anim::SkeletalClip* savedSkelClip_ = nullptr;
    float savedTime_ = 0.0f;
    float savedPlaybackRate_ = 1.0f;
    float hitTime_ = -1.0f;
    std::function<void(int)> savedCallback_;
};

/*!
 * FIFO queue of pending actions. `update(dt)` advances the current
 * action and pops it when done. `setOnComplete` registers a callback
 * fired after every completed action — `Renderer` uses it to advance
 * the turn manager (one action per turn in V1).
 */
class Manager {
public:
    using ActionPtr = std::unique_ptr<Action>;
    using CompleteFn = std::function<void()>;

    void enqueue(ActionPtr a);
    void update(float dt);
    bool isIdle() const { return current_ == nullptr && queue_.empty(); }
    size_t pendingCount() const { return queue_.size() + (current_ ? 1 : 0); }
    void clear();

    void setOnComplete(CompleteFn fn) { onComplete_ = std::move(fn); }

    static void selfTest();

private:
    std::vector<ActionPtr> queue_;
    std::unique_ptr<Action> current_;
    CompleteFn onComplete_;
};

float lastEventTime();

} // namespace action

#endif
