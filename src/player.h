/*
 (c) 2010 Perttu Ahola <celeron55@gmail.com>
 */

#ifndef PLAYER_HEADER
#define PLAYER_HEADER

#include <irrlicht.h>

using namespace irr;
typedef core::vector3df v3f;
typedef core::vector3d<s16> v3s16;

#define USEFUL_GEAR_COUNT 3
#define USEFUL_SKIN_COUNT 4

#define GEAR_0 ""
#define GEAR_1 "_tool"
#define GEAR_2 "_weapon"

class Map;

/*
 TODO: Make this a scene::ISceneNode
 */

class Player: public scene::ISceneNode {
public:
    Player(bool is_local);

    Player(bool is_local, scene::ISceneNode* parent, scene::ISceneManager* mgr,
            s32 id);

    ~Player();

// =========================================================

    void animateStand() {
        avatar_node->setFrameLoop(0, 80);
        avatar_node->setAnimationSpeed(32);
        moving = false;
    }

    void animateMove() {
        avatar_node->setFrameLoop(168, 188);
        avatar_node->setAnimationSpeed(32);
        moving = true;
    }

    bool isMoving() {
        return moving;
    }

    void move(f32 dtime, Map &map);

    /*
     ISceneNode methods
     */

    virtual void OnRegisterSceneNode() {
        if (IsVisible)
            SceneManager->registerNodeForRendering(this);

        ISceneNode::OnRegisterSceneNode();
    }

    virtual void render() {
        // Do nothing
    }

    virtual const core::aabbox3d<f32>& getBoundingBox() const {
        return m_box;
    }

    v3f getPosition() {
        return m_position;
    }

    void setPosition(v3f position) {
        m_position = position;
        updateSceneNodePosition();
    }

    void updatePosition(v3f position) {
        m_position = position;
    }

    v3f getRotation() {
        return m_rotation;
    }

    void setRotation(v3f rotation) {
        m_rotation = rotation;
        updateSceneNodeRotation();
    }

    void updateSceneNodeRotation() {
        ISceneNode::setRotation(m_rotation);
    }

    void updateSceneNodePosition() {
        ISceneNode::setPosition(m_position);
    }

    void updateMesh(scene::ISceneManager* mgr, u16 skin, u16 gear);

    bool isLocal() {
        return m_is_local;
    }

    /*
     * current velocity of player:
     * X-axis: left-right-wise
     * Y-axis: top-down-wise
     * Z-axis: head-rear-wise
     */
    v3f speed;
    bool touching_ground;
    u16 peer_id;
    float timeout_counter;

    u16 skin;
    u16 gear;

private:
    bool m_is_local;
    v3f m_position;
    v3f m_rotation;
    //scene::IBillboardSceneNode* m_bill;
    scene::IAnimatedMesh* avatar;
    scene::IAnimatedMeshSceneNode* avatar_node;
//    video::SMaterial material;
    core::aabbox3d<f32> m_box;
    bool moving;
};

#endif

