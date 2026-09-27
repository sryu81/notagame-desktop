#include "Action.h"

#include "BattleGrid.h"
#include "Engine.h"
#include "Unit.h"

#include <cassert>
#include <cmath>
#include <utility>

namespace action {

namespace {
float s_lastEventAtkTime = -1.0f;
}

float lastEventTime() { return s_lastEventAtkTime; }

MoveAction::MoveAction(game::Unit *u, const grid::BattleGrid *g,
                       std::vector<math::Vec2> path,
                       float perCellDuration, float hoverY)
        : unit_(u), grid_(g), path_(std::move(path)),
          perCellDuration_(perCellDuration), hoverY_(hoverY) {
    assert(unit_ != nullptr);
    assert(grid_ != nullptr);
    assert(perCellDuration_ > 0.0f);
    assert(!path_.empty());
}

void MoveAction::start() {
    currentSegment_ = 0;
    segmentElapsed_ = 0.0f;
    aout << "[action] MoveAction::start() unit_="
         << static_cast<const void*>(unit_) << std::endl;
    auto *node = unit_->node();
    assert(node != nullptr);
    savedClip_ = node->clip();
    originalClipTranslation_ = (savedClip_ && !savedClip_->keyframes.empty())
        ? savedClip_->keyframes[0].translation : math::Vec3(0.0f);

    const math::Vec3 world = grid_->cellToWorld(static_cast<int>(path_.front().x),
                                                  static_cast<int>(path_.front().y));
    endPos_ = math::Vec3(world.x, world.y + hoverY_, world.z);

    toRot_ = math::Quat::identity();
    if (path_.size() >= 2) {
        float dx = path_[1].x - path_[0].x;
        float dz = path_[1].y - path_[0].y;
        if (dx != 0.0f || dz != 0.0f) {
            float yaw = std::atan2(-dx, -dz);
            toRot_ = math::Quat::fromAxisAngle(math::Vec3(0,1,0), yaw);
        }
    }
    fromRot_ = node->animRotOffset();

    if (savedClip_ && !savedClip_->keyframes.empty()) {
        math::Mat4 clipMat = anim::sample(*savedClip_, node->time());
        node->setLocal(math::Mat4::translation(endPos_ - originalClipTranslation_
                                              + unit_->baseOffset())
                       * clipMat * math::Mat4::rotation(toRot_));
    } else {
        node->setLocal(math::Mat4::translation(endPos_ + unit_->baseOffset()) *
                       math::Mat4::scale(math::Vec3(0.56f)) *
                       math::Mat4::rotation(toRot_));
    }

    hasRig_ = false;
    if (node->children().size() >= 6) {
        auto& c = node->children();
        rig_.hipL    = c[1].get();
        rig_.hipR    = c[2].get();
        rig_.shldrL  = c[3].get();
        rig_.shldrR  = c[4].get();
        if (rig_.hipL->children().size() >= 2)
            rig_.kneeL = rig_.hipL->children()[1].get();
        if (rig_.hipR->children().size() >= 2)
            rig_.kneeR = rig_.hipR->children()[1].get();
        if (rig_.shldrL->children().size() >= 2)
            rig_.elbowL = rig_.shldrL->children()[1].get();
        if (rig_.shldrR->children().size() >= 2)
            rig_.elbowR = rig_.shldrR->children()[1].get();
        rig_.hipLRest    = rig_.hipL->local();
        rig_.hipRRest    = rig_.hipR->local();
        rig_.kneeLRest   = rig_.kneeL->local();
        rig_.kneeRRest   = rig_.kneeR->local();
        rig_.shldrLRest  = rig_.shldrL->local();
        rig_.shldrRRest  = rig_.shldrR->local();
        rig_.elbowLRest  = rig_.elbowL->local();
        rig_.elbowRRest  = rig_.elbowR->local();
        hasRig_ = true;
    }
}

void MoveAction::update(float dt) {
    if (isDone()) return;
    auto *node = unit_->node();
    if (node == nullptr) return;

    const int oldSeg = currentSegment_;
    segmentElapsed_ += dt;
    const int lastIdx = static_cast<int>(path_.size()) - 1;
    while (segmentElapsed_ >= perCellDuration_ &&
           currentSegment_ < lastIdx) {
        segmentElapsed_ -= perCellDuration_;
        currentSegment_++;
    }

    if (currentSegment_ != oldSeg && currentSegment_ < lastIdx) {
        fromRot_ = toRot_;
        float dx = path_[currentSegment_+1].x - path_[currentSegment_].x;
        float dz = path_[currentSegment_+1].y - path_[currentSegment_].y;
        if (dx != 0.0f || dz != 0.0f) {
            float yaw = std::atan2(-dx, -dz);
            toRot_ = math::Quat::fromAxisAngle(math::Vec3(0,1,0), yaw);
        }
    }

    if (currentSegment_ >= lastIdx) {
        const math::Vec3 world = grid_->cellToWorld(static_cast<int>(path_[lastIdx].x),
                                                     static_cast<int>(path_[lastIdx].y));
        endPos_ = math::Vec3(world.x, world.y + hoverY_, world.z);
    } else {
        const float t = segmentElapsed_ / perCellDuration_;
        const math::Vec3 fromW = grid_->cellToWorld(static_cast<int>(path_[currentSegment_].x),
                                                     static_cast<int>(path_[currentSegment_].y));
        const math::Vec3 toW = grid_->cellToWorld(static_cast<int>(path_[currentSegment_ + 1].x),
                                                   static_cast<int>(path_[currentSegment_ + 1].y));
        const math::Vec3 fromH(fromW.x, fromW.y + hoverY_, fromW.z);
        const math::Vec3 toH(toW.x, toW.y + hoverY_, toW.z);
        endPos_ = fromH + (toH - fromH) * t;
    }

    const float slerpT = std::min(segmentElapsed_ / perCellDuration_, 1.0f);
    const math::Quat rot = math::Quat::slerp(fromRot_, toRot_, slerpT);

    if (savedClip_ && !savedClip_->keyframes.empty()) {
        math::Mat4 clipMat = anim::sample(*savedClip_, node->time());
        node->setLocal(math::Mat4::translation(endPos_ - originalClipTranslation_
                                              + unit_->baseOffset())
                       * clipMat * math::Mat4::rotation(rot));
    } else {
        node->setLocal(math::Mat4::translation(endPos_ + unit_->baseOffset()) *
                       math::Mat4::scale(math::Vec3(0.56f)) *
                       math::Mat4::rotation(rot));
    }

    if (hasRig_) {
        const float phase = segmentElapsed_ * 12.0f;
        const float sinP = std::sin(phase);

        rig_.hipL->setLocal(rig_.hipLRest * math::Mat4::rotationX(sinP * 0.4f));
        rig_.hipR->setLocal(rig_.hipRRest * math::Mat4::rotationX(-sinP * 0.4f));
        rig_.kneeL->setLocal(rig_.kneeLRest * math::Mat4::rotationX(std::max(0.0f,  sinP) * 0.5f));
        rig_.kneeR->setLocal(rig_.kneeRRest * math::Mat4::rotationX(std::max(0.0f, -sinP) * 0.5f));

        rig_.shldrL->setLocal(rig_.shldrLRest * math::Mat4::rotationX(-sinP * 0.25f));
        rig_.shldrR->setLocal(rig_.shldrRRest * math::Mat4::rotationX( sinP * 0.25f));
        rig_.elbowL->setLocal(rig_.elbowLRest * math::Mat4::rotationX(std::max(0.0f, -sinP) * 0.2f));
        rig_.elbowR->setLocal(rig_.elbowRRest * math::Mat4::rotationX(std::max(0.0f,  sinP) * 0.2f));
    }
}

void MoveAction::onComplete() {
    assert(unit_ != nullptr);
    unit_->setCell(path_.back());

    auto *node = unit_->node();
    if (node == nullptr) return;

    const math::Vec3 finalPos(endPos_.x, endPos_.y - hoverY_, endPos_.z);
    node->setAnimRotOffset(toRot_);

    if (savedClip_ != nullptr && !savedClip_->keyframes.empty()) {
        math::Vec3 offset = finalPos - originalClipTranslation_;
        node->setAnimOffset(offset);

        math::Mat4 clipMat = anim::sample(*savedClip_, node->time());
        node->setLocal(math::Mat4::translation(finalPos - originalClipTranslation_
                                              + unit_->baseOffset())
                       * clipMat * math::Mat4::rotation(toRot_));
    } else {
        node->setLocal(math::Mat4::translation(finalPos) *
                       math::Mat4::scale(math::Vec3(0.56f)) *
                       math::Mat4::rotation(toRot_));
    }

    if (hasRig_) {
        rig_.hipL->setLocal(rig_.hipLRest);
        rig_.hipR->setLocal(rig_.hipRRest);
        if (rig_.kneeL) rig_.kneeL->setLocal(rig_.kneeLRest);
        if (rig_.kneeR) rig_.kneeR->setLocal(rig_.kneeRRest);
        rig_.shldrL->setLocal(rig_.shldrLRest);
        rig_.shldrR->setLocal(rig_.shldrRRest);
        if (rig_.elbowL) rig_.elbowL->setLocal(rig_.elbowLRest);
        if (rig_.elbowR) rig_.elbowR->setLocal(rig_.elbowRRest);
    }
}

AttackAction::AttackAction(game::Unit *attacker, game::Unit *target,
                           int damage,
                           const anim::Clip* attackClip,
                           const anim::Clip* hitReactionClip,
                           const anim::SkeletalClip* skeletalAttackClip,
                           const anim::SkeletalClip* skeletalHitReaction,
                           float delay)
        : attacker_(attacker), target_(target),
          damage_(damage),
          attackClip_(attackClip),
          hitReactionClip_(hitReactionClip),
          skeletalAttackClip_(skeletalAttackClip),
          skeletalHitReaction_(skeletalHitReaction),
          delay_(delay),
          hitTime_(-1.0f) {
    assert(attacker_ != nullptr);
    assert(target_ != nullptr);
    assert(damage_ > 0);
    assert(delay_ >= 0.0f);
    if (attackClip_) {
        for (const auto& ev : attackClip_->events)
            if (ev.id == 1) { hitTime_ = ev.time; break; }
    } else if (skeletalAttackClip_) {
        hitTime_ = 0.15f;
    }
    if (hitTime_ < 0.0f) hitTime_ = delay_;
}

void AttackAction::start() {
    elapsed_ = 0.0f;
    hitDelivered_ = false;
    auto* node = attacker_->node();
    if (node == nullptr) return;

    savedClip_ = node->clip();
    savedSkelClip_ = node->skeletalClip();
    savedTime_ = node->time();
    savedCallback_ = node->animEventCallback();
    savedPlaybackRate_ = 1.0f;

    if (skeletalAttackClip_ && node->skeleton()) {
        node->setSkeletalClip(skeletalAttackClip_);
    } else if (attackClip_) {
        node->setClip(attackClip_);
    }
    node->setTime(0.0f);
    node->setPlaybackRate(0.0f);

    game::Unit* rawTarget = target_;
    int rawDamage = damage_;
    const anim::Clip* reactionClip = hitReactionClip_;
    const anim::SkeletalClip* skelReaction = skeletalHitReaction_;
    SceneNode* rawAttacker = node;
    node->setAnimEventCallback([rawTarget, rawDamage, reactionClip, skelReaction, rawAttacker](int id) {
        if (id != 1 || rawTarget == nullptr || rawTarget->isDead()) return;
        rawTarget->takeDamage(rawDamage);
        rawTarget->applyStatus({unit::StatusType::Poisoned, 3, 1});
        auto* tnode = rawTarget->node();
        if (tnode) {
            if (skelReaction && tnode->skeleton()) {
                tnode->setSkeletalClip(skelReaction);
            } else if (reactionClip) {
                tnode->setClip(reactionClip);
            }
            tnode->setTime(0.0f);
        }
        aout << "[action] HIT via event  atk_time=" << rawAttacker->time()
             << " hit_time=" << (rawAttacker->time() >= 0.14f && rawAttacker->time() <= 0.16f ? "ok" : "DRIFT")
             << std::endl;
        s_lastEventAtkTime = rawAttacker->time();
    });
}

void AttackAction::update(float dt) {
    if (isDone()) return;
    elapsed_ += dt;
    auto* node = attacker_->node();
    if (node != nullptr) node->setTime(elapsed_);
    if (!hitDelivered_ && hitTime_ >= 0.0f && elapsed_ >= hitTime_) {
        aout << "[action] HIT via timer  atk_time=" << elapsed_
             << " hit_time=" << hitTime_ << std::endl;
        applyDamage();
    }
}

bool AttackAction::isDone() const {
    if (attackClip_ || skeletalAttackClip_) {
        float dur = delay_;
        if (attackClip_ && !attackClip_->keyframes.empty())
            dur = std::max(dur, attackClip_->keyframes.back().time);
        if (skeletalAttackClip_ && !skeletalAttackClip_->tracks.empty()) {
            float maxT = 0.0f;
            for (const auto& trk : skeletalAttackClip_->tracks)
                if (!trk.keyframes.empty() && trk.keyframes.back().time > maxT)
                    maxT = trk.keyframes.back().time;
            dur = std::max(dur, maxT);
        }
        return elapsed_ >= dur;
    }
    return elapsed_ >= delay_;
}

void AttackAction::applyDamage() {
    if (hitDelivered_ || target_ == nullptr) return;
    hitDelivered_ = true;
    target_->takeDamage(damage_);
    target_->applyStatus({unit::StatusType::Poisoned, 3, 1});
    if (hitCb_) hitCb_();
}

void AttackAction::onComplete() {
    if (!hitDelivered_) applyDamage();

    auto* node = attacker_->node();
    if (node) {
        node->setAnimEventCallback(std::move(savedCallback_));
        savedCallback_ = nullptr;
        node->setClip(savedClip_);
        node->setSkeletalClip(savedSkelClip_);
        node->setTime(savedTime_);
        node->setPlaybackRate(savedPlaybackRate_);
    }

    if (target_) {
        aout << "[action] " << (attacker_ ? attacker_->cell().x : -1) << ","
             << (attacker_ ? attacker_->cell().y : -1) << " attacks "
             << target_->cell().x << "," << target_->cell().y
             << " for " << damage_ << " damage (HP="
             << target_->hp() << "/" << target_->maxHp() << ") +Poisoned(3)" << std::endl;
    }
}

void Manager::enqueue(ActionPtr a) {
    if (a != nullptr) queue_.push_back(std::move(a));
}

void Manager::update(float dt) {
    if (current_ == nullptr && !queue_.empty()) {
        current_ = std::move(queue_.front());
        queue_.erase(queue_.begin());
        current_->start();
    }
    if (current_ != nullptr) {
        current_->update(dt);
        if (current_->isDone()) {
            current_->onComplete();
            current_.reset();
            if (onComplete_) onComplete_();
        }
    }
}

void Manager::clear() {
    queue_.clear();
    current_.reset();
}

void Manager::selfTest() {
    aout << "[action] self-test starting" << std::endl;
    grid::BattleGrid g(6, 6, 1.0f);
    g.setPassable(2, 2, false);
    g.setPassable(3, 2, false);
    g.setPassable(2, 3, false);
    g.setPassable(3, 3, false);

    {
        SceneNode parent;
        game::Unit u(&parent, unit::Faction::Player, unit::Stats{10, 5, 3, 4});

        anim::Clip clip;
        clip.loop = true;
        clip.keyframes = {
            {0.0f, math::Vec3(-1.5f, 0.5f, 0.5f), math::Quat::identity(),
             math::Vec3(0.8f)},
            {1.0f, math::Vec3(-1.5f, 0.5f, 0.5f), math::Quat::identity(),
             math::Vec3(0.8f)},
        };
        u.node()->setClip(&clip);

        Manager mgr;
        std::vector<math::Vec2> path = {{1, 3}, {2, 3}, {3, 3}, {4, 3}};
        mgr.enqueue(std::make_unique<MoveAction>(&u, &g, path, 0.4f, 0.5f));
        assert(mgr.pendingCount() == 1);
        assert(!mgr.isIdle());

        for (int i = 0; i < 15; ++i) mgr.update(0.1f);
        assert(mgr.isIdle());
        assert(u.cell().x == 4);
        assert(u.cell().y == 3);
        assert(u.node()->clip() == &clip);
        assert(math::approxEqual(u.node()->animOffset().x, 3.0f, 1e-4f));
        assert(math::approxEqual(u.node()->animOffset().z, 0.0f, 1e-4f));
        assert(math::approxEqual(clip.keyframes[0].translation.x, -1.5f, 1e-4f));
    }

    {
        SceneNode parent;
        game::Unit attacker(&parent, unit::Faction::Player,
                                  unit::Stats{10, 5, 3, 4});
        game::Unit target(&parent, unit::Faction::Enemy,
                                unit::Stats{8, 4, 2, 3});

        Manager mgr;
        mgr.enqueue(std::make_unique<AttackAction>(&attacker, &target, 3, nullptr, nullptr, nullptr, nullptr, 0.3f));
        assert(target.hp() == 8);

        for (int i = 0; i < 10; ++i) mgr.update(0.1f);
        assert(mgr.isIdle());
        assert(target.hp() == 5);
    }

    {
        SceneNode parent;
        game::Unit a(&parent, unit::Faction::Player, unit::Stats{10, 5, 3, 4});
        game::Unit b(&parent, unit::Faction::Enemy, unit::Stats{10, 2, 0, 3});

        Manager mgr;
        mgr.enqueue(std::make_unique<AttackAction>(&a, &b, 2, nullptr, nullptr, nullptr, nullptr, 0.0f));
        mgr.enqueue(std::make_unique<AttackAction>(&a, &b, 3, nullptr, nullptr, nullptr, nullptr, 0.0f));
        assert(mgr.pendingCount() == 2);

        mgr.update(0.1f);
        assert(!mgr.isIdle());
        assert(b.hp() == 8);
        mgr.update(0.1f);
        assert(mgr.isIdle());
        assert(b.hp() == 5);

        int callbackCount = 0;
        Manager mgr2;
        mgr2.setOnComplete([&callbackCount]() { ++callbackCount; });
        mgr2.enqueue(std::make_unique<AttackAction>(&a, &b, 1, nullptr, nullptr, nullptr, nullptr, 0.0f));
        mgr2.update(0.1f);
        assert(callbackCount == 1);
        assert(mgr2.isIdle());
    }

    {
        anim::Clip atkClip;
        atkClip.loop = false;
        atkClip.keyframes = {
            {0.0f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)},
            {0.5f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)}};
        atkClip.events.push_back({0.1f, 1});

        SceneNode parent;
        game::Unit atk(&parent, unit::Faction::Player, unit::Stats{10, 5, 3, 4});
        game::Unit def(&parent, unit::Faction::Enemy, unit::Stats{10, 2, 0, 3});

        atk.node()->setClip(&atkClip);

        Manager mgr;
        mgr.enqueue(std::make_unique<AttackAction>(&atk, &def, 4, &atkClip));
        mgr.update(0.05f);
        assert(def.hp() == 10);
        mgr.update(0.1f);
        assert(def.hp() == 6);
        for (int i = 0; i < 10; ++i) mgr.update(0.1f);
        assert(mgr.isIdle());
        assert(atk.node()->clip() == &atkClip);
    }

    {
        SceneNode parent;
        game::Unit u(&parent, unit::Faction::Player, unit::Stats{10, 5, 3, 4});

        Manager mgr;
        std::vector<math::Vec2> path = {{1, 3}};
        mgr.enqueue(std::make_unique<MoveAction>(&u, &g, path, 0.4f, 0.5f));
        mgr.update(0.1f);
        assert(mgr.isIdle());
        assert(u.cell().x == 1);
        assert(u.cell().y == 3);
    }

    {
        grid::BattleGrid elevGrid(4, 4, 1.0f);
        elevGrid.setCellY(1, 1, 0.0f);
        elevGrid.setCellY(2, 1, 0.5f);
        elevGrid.setCellY(3, 1, 0.0f);

        SceneNode parent;
        game::Unit u(&parent, unit::Faction::Player, unit::Stats{10, 5, 3, 4});

        Manager mgr;
        std::vector<math::Vec2> path = {{1, 1}, {2, 1}, {3, 1}};
        mgr.enqueue(std::make_unique<MoveAction>(&u, &elevGrid, path, 0.4f, 0.5f));

        mgr.update(0.2f);
        assert(!mgr.isIdle());
        const math::Vec3 mid = u.node()->local().transformPoint(math::Vec3(0, 0, 0));
        assert(mid.y > 0.5f && mid.y < 1.0f);

        for (int i = 0; i < 10; ++i) mgr.update(0.1f);
        assert(mgr.isIdle());
        assert(u.cell().x == 3);
        assert(u.cell().y == 1);
        const math::Vec3 end = u.node()->local().transformPoint(math::Vec3(0, 0, 0));
        assert(math::approxEqual(end.y, 0.0f, 1e-4f));
    }

    aout << "[action] self-test PASS" << std::endl;
}

} // namespace action
