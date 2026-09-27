#include "SceneNode.h"

#include "AndroidOut.h"

#include <cassert>
#include <cmath>

SceneNode *SceneNode::addChild() {
    children_.push_back(std::make_unique<SceneNode>());
    children_.back()->parent_ = this;
    children_.back()->markDirty();
    return children_.back().get();
}

void SceneNode::addChild(std::unique_ptr<SceneNode> child) {
    child->parent_ = this;
    child->markDirty();
    children_.push_back(std::move(child));
}

void SceneNode::setLocal(const math::Mat4 &m) {
    local_ = m;
    markDirty();
}

void SceneNode::markDirty() {
    if (!dirty_) {
        dirty_ = true;
        for (auto &child : children_) child->markDirty();
    }
}

void SceneNode::update(float dt) {
    const bool hasClip = (clip_ != nullptr && !clip_->keyframes.empty());
    const bool hasSkeletal = (skeletalClip_ != nullptr && skeleton_ != nullptr &&
                              !skeletalClip_->tracks.empty());

    if (hasClip || hasSkeletal) {
        time_ += dt * playbackRate_;
        if (hasClip) {
            auto m = anim::sample(*clip_, time_);
            if (animOffset_.x != 0.0f || animOffset_.y != 0.0f || animOffset_.z != 0.0f)
                m = math::Mat4::translation(animOffset_) * m;
            if (baseOffset_.x != 0.0f || baseOffset_.y != 0.0f || baseOffset_.z != 0.0f)
                m = math::Mat4::translation(baseOffset_) * m;
            if (animRotOffset_.x != 0.0f || animRotOffset_.y != 0.0f || animRotOffset_.z != 0.0f || animRotOffset_.w != 1.0f)
                m = m * math::Mat4::rotation(animRotOffset_);
            setLocal(m);
        }
        if (hasSkeletal) {
            auto perBoneLocal = anim::sampleSkeletal(*skeletalClip_, time_, *skeleton_);
            if (poseModifier_) poseModifier_(perBoneLocal);
            boneMatrices_ = anim::computeBoneMatrices(*skeleton_, perBoneLocal);
        }
    }

    const std::vector<anim::AnimEvent> *events = nullptr;
    float dur = 0.0f, nowW = 0.0f;
    if (hasClip && !clip_->events.empty()) {
        events = &clip_->events;
        dur = clip_->keyframes.back().time;
        nowW = anim::wrapTime(*clip_, time_);
    } else if (hasSkeletal && !skeletalClip_->events.empty()) {
        events = &skeletalClip_->events;
        dur = anim::duration(*skeletalClip_);
        nowW = anim::wrapTime(*skeletalClip_, time_);
    }
    if (events && animEventCb_ && dur > 0.0f) {
        if (prevEventTime_ < 0.0f) {
            prevEventTime_ = nowW;
        } else {
            std::vector<int> fired;
            anim::crossedEvents(*events, dur, prevEventTime_, nowW, fired);
            prevEventTime_ = nowW;
            for (int id : fired) animEventCb_(id);
        }
    }

    for (auto &child : children_) child->update(dt);
}

math::Mat4 SceneNode::world() const {
    if (dirty_) {
        cachedWorld_ = (parent_ != nullptr) ? parent_->world() * local_ : local_;
        dirty_ = false;
    }
    return cachedWorld_;
}

namespace {

bool approxMat4(const math::Mat4 &a, const math::Mat4 &b, float eps = 1e-4f) {
    for (int i = 0; i < 16; i++) {
        if (std::abs(a.m[i] - b.m[i]) > eps) return false;
    }
    return true;
}

} // namespace

void SceneNode::selfTest() {
    aout << "[scene] self-test starting" << std::endl;

    // Root with identity local — world must be identity.
    {
        SceneNode root;
        assert(root.parent() == nullptr);
        assert(approxMat4(root.world(), math::Mat4::identity()));
    }

    // Single child: parent world * child local = child world.
    {
        SceneNode root;
        SceneNode *child = root.addChild();
        const math::Mat4 childLocal =
                math::Mat4::translation(math::Vec3(1.0f, 0.0f, 0.0f));
        child->setLocal(childLocal);
        assert(approxMat4(child->world(), childLocal));
    }

    // Two levels: root → child(t) → grandchild(rotZ)
    {
        SceneNode root;
        SceneNode *child = root.addChild();
        const math::Mat4 childLocal =
                math::Mat4::translation(math::Vec3(1.0f, 0.0f, 0.0f));
        child->setLocal(childLocal);

        SceneNode *grand = child->addChild();
        const math::Mat4 grandLocal =
                math::Mat4::rotationZ(math::radians(90.0f));
        grand->setLocal(grandLocal);

        const math::Mat4 expected = childLocal * grandLocal;
        assert(approxMat4(grand->world(), expected));

        const math::Vec4 p4 = grand->world() * math::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
        assert(math::approxEqual(p4.x, 1.0f, 1e-4f));
        assert(math::approxEqual(p4.y, 1.0f, 1e-4f));
        assert(math::approxEqual(p4.z, 0.0f, 1e-4f));
    }

    // addChild(unique_ptr) — ownership transfer + parent pointer set.
    {
        SceneNode root;
        auto child = std::make_unique<SceneNode>();
        SceneNode *rawPtr = child.get();
        root.addChild(std::move(child));
        assert(rawPtr->parent() == &root);
        assert(root.children().size() == 1);
    }

    // Drawables attached to a node don't affect its world transform.
    {
        SceneNode root;
        SceneNode *child = root.addChild();
        child->setLocal(math::Mat4::translation(math::Vec3(5.0f, 0.0f, 0.0f)));
        const math::Mat4 worldBefore = child->world();
        child->addDrawable(SceneNode::Drawable{nullptr, nullptr, math::Vec3(1.0f, 1.0f, 1.0f)});
        assert(approxMat4(child->world(), worldBefore));
    }

    // Dirty flag: setLocal invalidates world; reading twice returns cached.
    {
        SceneNode root;
        SceneNode *child = root.addChild();
        child->setLocal(math::Mat4::translation(math::Vec3(3.0f, 0.0f, 0.0f)));
        const math::Mat4 w1 = child->world();
        const math::Mat4 w2 = child->world();
        assert(approxMat4(w1, w2));
        child->setLocal(math::Mat4::translation(math::Vec3(7.0f, 0.0f, 0.0f)));
        const math::Mat4 w3 = child->world();
        assert(!approxMat4(w1, w3));
        assert(math::approxEqual(w3.m[12], 7.0f, 1e-4f));
    }

    // Dirty flag: setLocal on parent propagates to children.
    {
        SceneNode root;
        SceneNode *child = root.addChild();
        SceneNode *grand = child->addChild();
        child->setLocal(math::Mat4::translation(math::Vec3(2.0f, 0.0f, 0.0f)));
        grand->setLocal(math::Mat4::translation(math::Vec3(1.0f, 0.0f, 0.0f)));
        const math::Mat4 wBefore = grand->world();
        assert(math::approxEqual(wBefore.m[12], 3.0f, 1e-4f));
        (void)grand->world();
        child->setLocal(math::Mat4::translation(math::Vec3(10.0f, 0.0f, 0.0f)));
        const math::Mat4 wAfter = grand->world();
        assert(math::approxEqual(wAfter.m[12], 11.0f, 1e-4f));
    }

    // update with no clip: time_ stays at 0, local_ unchanged.
    {
        SceneNode node;
        node.setLocal(math::Mat4::translation(math::Vec3(1.0f, 0.0f, 0.0f)));
        node.update(0.5f);
        assert(math::approxEqual(node.local().m[12], 1.0f, 1e-4f));
    }

    // update with a clip drives local_ from the sampled keyframe.
    {
        SceneNode node;
        anim::Clip clip;
        clip.loop = true;
        clip.keyframes.push_back(
            {0.0f, math::Vec3(0.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)});
        clip.keyframes.push_back(
            {1.0f, math::Vec3(4.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(2.0f)});
        node.setClip(&clip);
        node.update(0.5f);
        assert(math::approxEqual(node.local().m[12], 2.0f, 1e-4f));
        assert(math::approxEqual(node.local().m[0], 1.5f, 1e-4f));
        node.update(0.5f);
        assert(math::approxEqual(node.local().m[12], 0.0f, 1e-4f));
    }

    // Skeletal clip computes bone matrices.
    {
        SceneNode node;
        anim::Skeleton skel;
        skel.bones.push_back({-1, math::Mat4::identity()});
        skel.bones.push_back({0,  math::Mat4::identity()});

        anim::SkeletalClip clip;
        clip.loop = true;
        clip.tracks.push_back({0, {
            {0.0f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)},
            {2.0f, math::Vec3(5.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
        clip.tracks.push_back({1, {
            {0.0f, math::Vec3(1.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});

        node.setSkeleton(&skel);
        node.setSkeletalClip(&clip);
        node.update(1.0f); // t=1 → bone0: (2.5, 0, 0), bone1: (1, 0, 0)
        assert(node.hasBoneMatrices());
        assert(node.boneMatrices().size() == 2);
        assert(math::approxEqual(node.boneMatrices()[0].m[12], 2.5f, 1e-4f));
        assert(math::approxEqual(node.boneMatrices()[1].m[12], 3.5f, 1e-4f)); // parent(2.5) * child(1)
    }

    // Animation event fires at correct time (not on first frame).
    {
        SceneNode node;
        anim::Clip clip;
        clip.loop = true;
        clip.keyframes.push_back({0.0f, math::Vec3(), math::Quat::identity(), math::Vec3(1.0f)});
        clip.keyframes.push_back({2.0f, math::Vec3(), math::Quat::identity(), math::Vec3(1.0f)});
        clip.events.push_back({0.5f, 42});
        clip.events.push_back({1.5f, 99});
        int fired = 0;
        node.setClip(&clip);
        node.setAnimEventCallback([&](int id) { fired = fired * 10 + id; });
        node.update(0.3f); // t=0.3, first frame → no fire
        assert(fired == 0);
        node.update(0.3f); // t=0.6, crosses 0.5 → fire 42
        assert(fired == 42);
        node.update(0.5f); // t=1.1, no event
        assert(fired == 42);
        node.update(0.5f); // t=1.6, crosses 1.5 → fire 99, fired = 42*10+99
        assert(fired == 519);
    }

    // No callback set → no crash with events present.
    {
        SceneNode node;
        anim::Clip clip;
        clip.loop = true;
        clip.keyframes.push_back({0.0f, math::Vec3(), math::Quat::identity(), math::Vec3(1.0f)});
        clip.keyframes.push_back({1.0f, math::Vec3(), math::Quat::identity(), math::Vec3(1.0f)});
        clip.events.push_back({0.5f, 1});
        node.setClip(&clip);
        node.update(0.6f);
        node.update(0.6f);
    }

    // Looping: event re-fires after wrap.
    {
        SceneNode node;
        anim::Clip clip;
        clip.loop = true;
        clip.keyframes.push_back({0.0f, math::Vec3(), math::Quat::identity(), math::Vec3(1.0f)});
        clip.keyframes.push_back({1.0f, math::Vec3(), math::Quat::identity(), math::Vec3(1.0f)});
        clip.events.push_back({0.8f, 7});
        int count = 0;
        node.setClip(&clip);
        node.setAnimEventCallback([&](int) { ++count; });
        node.update(0.5f); // t=0.5, first frame, no fire
        assert(count == 0);
        node.update(0.4f); // t=0.9, crosses 0.8 → fire
        assert(count == 1);
        node.update(0.2f); // t=1.1→wrap to 0.1, crosses no event
        assert(count == 1);
        node.update(0.8f); // t=1.9→wrap to 0.9, crosses 0.8 → fire again
        assert(count == 2);
    }

    {
        anim::Skeleton skel;
        skel.bones.push_back({-1, math::Mat4::identity()});
        anim::SkeletalClip clip;
        clip.loop = true;
        clip.tracks.push_back({0, {{0.0f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)},
                                   {1.0f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
        clip.events.push_back({0.5f, 7});
        SceneNode node;
        node.setSkeleton(&skel);
        node.setSkeletalClip(&clip);
        int fired = 0;
        node.setAnimEventCallback([&fired](int id) { if (id == 7) ++fired; });
        node.setPoseModifier([](std::vector<math::Mat4>& local) {
            local[0] = math::Mat4::translation(math::Vec3(5.0f, 0.0f, 0.0f));
        });
        for (int i = 0; i < 25; ++i) node.update(0.1f);
        assert(fired == 2);
        assert(math::approxEqual(node.boneMatrices()[0].m[12], 5.0f, 1e-5f));
        anim::SkeletalClip other = clip;
        node.setSkeletalClip(&other);
        node.update(0.1f);
        assert(fired == 2);
    }

    aout << "[scene] self-test PASS" << std::endl;
}
