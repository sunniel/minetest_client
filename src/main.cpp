/*
 (c) 2010 Perttu Ahola <celeron55@gmail.com>

 Minetest

 TODO: Check for obsolete todos
 TODO: Storing map on disk, preferably dynamically
 TODO: struct settings, with a mutex and get/set functions
 TODO: Implement torches and similar light sources (halfway done)
 TODO: A menu
 TODO: A cache class that can be used with lightNeighbors,
 unlightNeighbors and probably many others. Look for
 implementation in lightNeighbors
 TODO: Proper objects for random stuff in this file, like
 g_selected_material
 TODO: See if changing to 32-bit position variables doesn't raise
 memory consumption a lot.
 Now:
 TODO: Have to implement mutexes to MapSectors; otherwise initial
 lighting might fail
 TODO: Adding more heightmap points to MapSectors

 Network protocol:
 - Client map data is only updated from the server's,
 EXCEPT FOR lighting.

 Actions:

 - User places block
 -> Client sends PLACED_BLOCK(pos, node)
 -> Server validates and sends MAP_SINGLE_CHANGE(pos, node)
 -> Client applies change and recalculates lighting and face cache

 - User starts digging
 -> Client sends START_DIGGING(pos)
 -> Server starts timer
 - if user stops digging:
 -> Client sends STOP_DIGGING
 -> Server stops timer
 - if user continues:
 -> Server waits timer
 -> Server sends MAP_SINGLE_CHANGE(pos, node)
 -> Client applies change and recalculates lighting and face cache

 */

/*
 Setting this to 1 enables a special camera mode that forces
 the renderers to think that the camera statically points from
 the starting place to a static direction.

 This allows one to move around with the player and see what
 is actually drawn behind solid things etc.
 */
#define FIELD_OF_VIEW_TEST 0

// Enable unit tests
#define ENABLE_TESTS 0

#ifdef _MSC_VER
#pragma comment(lib, "Irrlicht.lib")
#pragma comment(lib, "jthread.lib")
// This would get rid of the console window
//#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define sleep_ms(x) Sleep(x)
#else
#include <unistd.h>
#define sleep_ms(x) usleep(x*1000)
#endif

#include <iostream>
#include <time.h>
#include <jmutexautolock.h>
namespace jthread {
} // JThread 1.2 support
using namespace jthread;
// JThread 1.3 support
#include "common_irrlicht.h"
#include "map.h"
#include "player.h"
#include "main.h"
#include "environment.h"
//#include "server.h"
#include "client.h"
#include <string>

#include <map>
#include "clientserver.h"
#include "properties.h"

const char *g_material_filenames[MATERIALS_COUNT] = { "../data/stone.png",
        "../data/grass.png", "../data/water.png", "../data/highlight.png" };

#define FPS_MIN 15
#define FPS_MAX 25

#define VIEWING_RANGE_NODES_MIN MAP_BLOCKSIZE
#define VIEWING_RANGE_NODES_MAX 35

JMutex g_viewing_range_nodes_mutex;
s16 g_viewing_range_nodes = MAP_BLOCKSIZE;

s16 MAP_LENGTH = properties::getShort("map", "length", 100);
s16 MAP_WIDTH = properties::getShort("map", "width", 100);
s16 MAP_HEIGHT = properties::getShort("map", "height", 10);
s16 MAP_BOTTOM = properties::getShort("map", "bottom", -10);

/*
 Random stuff
 */
u16 g_selected_material = 0;
u16 g_selected_skin = 1;
u16 g_selected_gear = 0;

/*
 Debug streams
 - use these to disable or enable outputs of parts of the program
 */

std::ofstream dfile("debug.txt");
//std::ofstream dfile2("debug2.txt");

// Connection
//std::ostream dout_con(std::cout.rdbuf());
std::ostream dout_con(dfile.rdbuf());

// Server
//std::ostream dout_server(std::cout.rdbuf());
std::ostream dout_server(dfile.rdbuf());

// Client
//std::ostream dout_client(std::cout.rdbuf());
std::ostream dout_client(dfile.rdbuf());

std::ostream dout_map(dfile.rdbuf());

Player *player;

class MyEventReceiver: public IEventReceiver {
public:
    // This is the one method that we have to implement
    virtual bool OnEvent(const SEvent& event) {
        // Remember whether each key is down or up
        if (event.EventType == irr::EET_KEY_INPUT_EVENT) {
            keyIsDown[event.KeyInput.Key] = event.KeyInput.PressedDown;
            if (event.KeyInput.PressedDown) {
                if (event.KeyInput.Key == irr::KEY_KEY_F) {
                    if (g_selected_material < USEFUL_MATERIAL_COUNT - 1)
                        g_selected_material++;
                    else
                        g_selected_material = 0;
                    std::cout << "Selected material: " << g_selected_material
                            << std::endl;
                }
                if (event.KeyInput.Key == irr::KEY_KEY_G) {
                    if (g_selected_gear < USEFUL_GEAR_COUNT - 1)
                        g_selected_gear++;
                    else
                        g_selected_gear = 0;
//                    std::cout << "Selected gear: " << g_selected_gear
//                            << std::endl;
                }
                if (event.KeyInput.Key == irr::KEY_KEY_V) {
                    if (g_selected_skin < USEFUL_SKIN_COUNT)
                        g_selected_skin++;
                    else
                        g_selected_skin = 1;
//                    std::cout << "Selected skin: " << g_selected_skin
//                            << std::endl;
                }
            }
            if (event.KeyInput.Key == KEY_KEY_W
                    || event.KeyInput.Key == KEY_KEY_A
                    || event.KeyInput.Key == KEY_KEY_S
                    || event.KeyInput.Key == KEY_KEY_D) {

                if ((event.KeyInput.PressedDown) && (!walking)) //this will be done once
                        {
                    walking = true;
//                    player->animateMove();
                } else if ((!event.KeyInput.PressedDown) && (walking)) //this will be done on key up
                        {
                    walking = false;
//                    player->animateStand();
                }
            }
            // currently disable the node add/remove function
//            if ((event.KeyInput.Key == KEY_LCONTROL
//                    || event.KeyInput.Key == KEY_RCONTROL)
//                    && event.KeyInput.PressedDown) {
//                ctrl = !ctrl;
//            }
            if ((event.KeyInput.Key == KEY_ESCAPE && event.KeyInput.PressedDown)) {
                if (!isPaused) {
                    isPaused = true;
                } else {
                    isExit = true;
                }
            }
            if ((event.KeyInput.Key == KEY_KEY_Q && isPaused)) {
                isPaused = false;
            }
        }

        if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
            if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN) {
                leftclicked = true;
            }
            if (event.MouseInput.Event == EMIE_RMOUSE_PRESSED_DOWN) {
                rightclicked = true;

            }
            if (event.MouseInput.Event == EMIE_MOUSE_WHEEL) {
                wheel = event.MouseInput.Wheel;
            }
        }

        return false;
    }

    // This is used to check whether a key is being held down
    virtual bool IsKeyDown(EKEY_CODE keyCode) const {
        return keyIsDown[keyCode];
    }

    MyEventReceiver() {
        for (u32 i = 0; i < KEY_KEY_CODES_COUNT; ++i)
            keyIsDown[i] = false;
        leftclicked = false;
        rightclicked = false;
        wheel = 0;
        ctrl = false;
        walking = false;
        isPaused = false;
        isExit = false;
    }

    bool leftclicked;
    bool rightclicked;
    float wheel;
    bool ctrl;
    bool walking;
    bool isPaused;
    bool isExit;
private:
    // We use this array to store the current state of each key
    bool keyIsDown[KEY_KEY_CODES_COUNT];
    //s32 mouseX;
    //s32 mouseY;
};

con::Connection login_con(PROTOCOL_ID, 512);
u32 data_maxsize = 10000;
Buffer<u8> data(data_maxsize);
u16 peer_id;
u32 datasize;
void LoginReceive() {
    try {
        datasize = login_con.Receive(peer_id, *data, data_maxsize);

        std::cout << "received data from login server" << endl;

    } catch (con::NoIncomingDataException &e) {

//        std::cout << "NoIncomingDataException" << endl;

        // exit of the for loop
//        break;
    } catch (con::InvalidIncomingDataException &e) {

        std::cout << "LoginReceive: "
                "InvalidIncomingDataException: what()=" << e.what()
                << std::endl;

        dout_client << "LoginReceive: "
                "InvalidIncomingDataException: what()=" << e.what()
                << std::endl;
    }
}
bool login(const char* connect_name, unsigned short port,
        unsigned short playerId) {
    unsigned short loin_port = (unsigned short) properties::getUInteger(
            "network", "login_port", 20000);
    Address connect_address(0, 0, 0, 0, loin_port);
    try {
        connect_address.Resolve(connect_name);
    } catch (ResolveError &e) {
        std::cout << "Couldn't resolve address" << std::endl;
        return 0;
    }

    long t = static_cast<long>(time(NULL));
    srand((unsigned int) t);
    int num = rand() % 100;
    std::string var = std::to_string(t) + std::to_string(num);
    u16 peerId = pearsonHash16(var);

    login_con.SetPeerID(peerId);
    login_con.setTimeoutMs(0);
    login_con.LoginConnect(connect_address);

    std::cout << "Logging into Virtual Net" << endl;

    LoginReceive();
    while (!login_con.LoginServerConnected()) {
//        std::cout << "not connected" << endl;
        LoginReceive();
        sleep_ms(200);
    }

    std::cout << "Login connection created" << endl;

    // send port number to
    SharedBuffer<u8> data(2 + 2 + 2);
    writeU16(&data[0], TOSERVER_PLAYERPORT);
    writeU16(&data[2], port);
    writeU16(&data[4], playerId);
    // Send as unreliable
    login_con.Send(PEER_ID_LOGIN_SERVER, 0, data, true);

    std::cout << "Connection port sent" << endl;

    // check reply
    bool result = false;
    sleep_ms(200);
    while (!result) {

//        std::cout << "To receive command from login server" << endl;

        LoginReceive();
        if (datasize == 4) {

            std::cout << "Command received from login server" << endl;

            ToClientCommand command = (ToClientCommand) readU16(&data[0]);
            if (command == TOCLIENT_LOGIN) {

                std::cout << "command TOCLIENT_LOGIN received" << endl;

                unsigned short reply = readS16(&data[2]);
                std::cout << "login result: " << result << std::endl;
                result = (bool) reply;
                break;
            }
        }

//        std::cout << "no command received from login server" << endl;
        sleep_ms(500);
    }

    return result;
}

int main() {
    sockets_init();
    atexit(sockets_cleanup);

    //return 0; //DEBUG

    /*
     Initialization
     */

    srand(time(0));

    g_viewing_range_nodes_mutex.Init();
    assert(g_viewing_range_nodes_mutex.IsInitialized());

    MyEventReceiver receiver;

    // create device and exit if creation failed

    /*
     Host selection
     */
//    char connect_name[100];
    std::string connect_name = properties::getString("network", "server",
            "0.0.0.0");
    std::cout << "Connecting to server: " << connect_name << std::endl;

    unsigned short port = properties::getUInteger("network", "server_port",
            9999);
    std::cout << "server port: " << port << std::endl;

    // input client to avoid peer ID conflict
    std::cout
            << "Unique client ID [client<system time in milliseconds + random number>]: ";
    char cmdline[100];
    std::cin.getline(cmdline, 100);
    std::string clientId;
    if (cmdline[0] == 0) {
        long t = static_cast<long>(time(NULL));
        srand((unsigned int) t);
        int num = rand() % 100;
        std::string var = std::to_string(t) + std::to_string(num);
        clientId = "client<" + var + ">";
    } else
        clientId = std::string(cmdline);
    u16 playerId = pearsonHash16(clientId);

    // login to Virtual Net
    std::cout << "To create a Mesh, please wait ..." << endl;
    bool success = login(connect_name.c_str(), port, playerId);
    if (!success) {
        // could not create a Mesh
//        return 1;
        std::cout << "Mesh creation failed!" << endl;
        char errline[100];
        std::cin.getline(errline, 100);
    }

    unsigned int wait_length = properties::getUInteger("connection",
            "init_time", 15);
    std::cout << "Wait " << wait_length << " seconds for Mesh creation ..."
            << endl;
    sleep_ms(wait_length * 1000);

    /*
     Resolution selection
     */

    u16 screenW = 1024;
    u16 screenH = 768;

    //
    video::E_DRIVER_TYPE driverType;

#ifdef _WIN32
    //driverType = video::EDT_DIRECT3D9; // Doesn't seem to work
    driverType = video::EDT_OPENGL;
#else
    driverType = video::EDT_OPENGL;
#endif

    IrrlichtDevice *device;
    device = createDevice(driverType, core::dimension2d<u32>(screenW, screenH),
            16, false, false, false, &receiver);

    if (device == 0)
        return 1; // could not create selected driver.

    /*
     Continue initialization
     */

    video::IVideoDriver* driver = device->getVideoDriver();
    // These make the textures not to show at all
    //driver->setTextureCreationFlag(video::ETCF_ALWAYS_16_BIT);
    //driver->setTextureCreationFlag(video::ETCF_OPTIMIZED_FOR_SPEED );

    scene::ISceneManager* smgr = device->getSceneManager();

    gui::IGUIEnvironment* guienv = device->getGUIEnvironment();

    video::ITexture* image = driver->getTexture("../data/pause.png");
    u16 imgWidth = 600;
    u16 imgHeight = 600;
    gui::IGUIImage* pauseOverlay = guienv->addImage(image,
            core::position2d<int>(screenW / 2 - imgWidth / 2,
                    screenH / 2 - imgHeight / 2));
    pauseOverlay->setVisible(false);

    gui::IGUISkin* skin = guienv->getSkin();
    gui::IGUIFont* font = guienv->getFont("../data/fontlucida.png");
    if (font)
        skin->setFont(font);
    //skin->setColor(gui::EGDC_BUTTON_TEXT, video::SColor(255,0,0,0));
    skin->setColor(gui::EGDC_BUTTON_TEXT, video::SColor(255, 255, 255, 255));
    //skin->setColor(gui::EGDC_3D_HIGH_LIGHT, video::SColor(0,0,0,0));
    //skin->setColor(gui::EGDC_3D_SHADOW, video::SColor(0,0,0,0));
    skin->setColor(gui::EGDC_3D_HIGH_LIGHT, video::SColor(255, 0, 0, 0));
    skin->setColor(gui::EGDC_3D_SHADOW, video::SColor(255, 0, 0, 0));

    const wchar_t *text = L"Loading...";
    core::vector2d<s32> center(screenW / 2, screenH / 2);
    core::dimension2d<u32> textd = font->getDimension(text);
    std::cout << "Text w=" << textd.Width << " h=" << textd.Height << std::endl;
    // Have to add a bit to disable the text from word wrapping
    //core::vector2d<s32> textsize(textd.Width+4, textd.Height);
    core::vector2d<s32> textsize(300, textd.Height);
    core::rect<s32> textrect(center - textsize / 2, center + textsize / 2);
    gui::IGUIStaticText *gui_loadingtext = guienv->addStaticText(text, textrect,
            false, false);
    gui_loadingtext->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_UPPERLEFT);

    // render loading text in simulation loading
    driver->beginScene(true, true, video::SColor(255, 0, 0, 0));
    guienv->drawAll();
    driver->endScene();

    video::SMaterial materials[MATERIALS_COUNT];
    for (u16 i = 0; i < MATERIALS_COUNT; i++) {
        materials[i].Lighting = false;
        materials[i].BackfaceCulling = false;

        const char *filename = g_material_filenames[i];
        if (filename != NULL) {
            video::ITexture *t = driver->getTexture(filename);
            if (t == NULL) {
                std::cout << "Texture could not be loaded: \"" << filename
                        << "\"" << std::endl;
                return 1;
            }
            materials[i].setTexture(0, driver->getTexture(filename));
        }
        //materials[i].setFlag(video::EMF_TEXTURE_WRAP, video::ETC_REPEAT);
        materials[i].setFlag(video::EMF_BILINEAR_FILTER, false);
        //materials[i].setFlag(video::EMF_ANISOTROPIC_FILTER, false);
    }

    // Make a scope here for the client so that it gets removed
    // before the irrlicht device
    {
        std::cout << "Creating server and client" << std::endl;

//        Server *server = NULL;
//        if (hosting) {
//            server = new Server();
//            server->start(port);
//        }

        Client client(smgr, materials, playerId);
        Address connect_address(0, 0, 0, 0, port);
        try {
            connect_address.Resolve(connect_name.c_str());
        } catch (ResolveError &e) {
            std::cout << "Couldn't resolve address" << std::endl;
            return 0;
        }
        client.connect(connect_address);

//        Player *player = client.getLocalPlayer();
        player = client.getLocalPlayer();
        // Initialize player state. Otherwise, the state will be unknown
        player->animateStand();

        /*
         Create the camera node
         */
        scene::ICameraSceneNode* camera = smgr->addCameraSceneNode(0, // Camera parent
                v3f(BS * 100, BS * 2, BS * 100), // Look from
                v3f(BS * 100 + 1, BS * 2, BS * 100), // Look to
                -1 // Camera ID
                );
        if (camera == NULL)
            return 1;
        // default camera vision area
        camera->setFOV(FOV_ANGLE);
        // visual perception range = 10, half of the AoI (5000)
        camera->setFarValue(BS * 50);

#define ZOOM_MAX 1.5
#define ZOOM_MIN (-5.0*BS)
#define ZOOM_SPEED (0.02*BS)
#define ROTATE_SPEED 1

        f32 camera_yaw = 0; // "right/left"
        f32 camera_pitch = 0; // "up/down"

        // camera zoom distance control
        f32 zoom_max = ZOOM_MAX;
        f32 zoom_min = ZOOM_MIN;
        f32 zoom_speed = ZOOM_SPEED;
        f32 camera_zoom = 0;

        // camera rotate control
        f64 rotate_speed = ROTATE_SPEED;
        f64 camera_rotate = 0;

        // Random constants
#define WALK_ACCELERATION (4.0 * BS)
#define WALKSPEED_MAX (4.0 * BS)
//#define WALKSPEED_MAX (20.0 * BS)
        f32 walk_acceleration = WALK_ACCELERATION;
        f32 walkspeed_max = WALKSPEED_MAX;

        /*
         The mouse cursor needs not be visible, so we hide it via the
         irr::IrrlichtDevice::ICursorControl.
         */
        device->getCursorControl()->setVisible(false);

        // loaded
        gui_loadingtext->remove();
        gui::IGUIStaticText *guitext = guienv->addStaticText(L"Minetest-c55",
                core::rect<s32>(5, 5, 5 + 350, 5 + textsize.Y), false, false);
        gui::IGUIStaticText* pos_text = guienv->addStaticText(
                L"Current position: (0, 0, 0)",
                core::rect<s32>(5, 5 + textsize.Y, 5 + 500, 5 + 2 * textsize.Y),
                false, false);
        gui::IGUIStaticText* rot_text = guienv->addStaticText(
                L"Current direction to north: 0",
                core::rect<s32>(5, 5 + 2 * textsize.Y, 5 + 500,
                        5 + 3 * textsize.Y), false, false);
        gui::IGUIStaticText* load_text = guienv->addStaticText(
                L"Current direction to north: 0",
                core::rect<s32>(5, 5 + 3 * textsize.Y, 5 + 245,
                        5 + 4 * textsize.Y), false, false);

        /*
         Main loop
         */
        bool first_loop_after_window_activation = true;
        s32 lastFPS = -1;
        // Time is in milliseconds
        u32 lasttime = device->getTimer()->getTime();

        map<v3s16, MapNode> selected;

        // frame-based simulation update
        while (device->run()) {

            // client exit check
            if (receiver.isExit) {
                client.exit();
                // wait 10 seconds
//                sleep_ms(5000);
                break;
            }

            // game pause check
            if (receiver.isPaused) {
                if (!pauseOverlay->isVisible()) {
                    device->getTimer()->stop();
                    pauseOverlay->setVisible(true);
                }
                if (device->isWindowActive()) {
                    driver->beginScene(true, true,
                            video::SColor(0, 200, 200, 200));

                    guienv->drawAll();

                    driver->endScene();
                }
            } else {

                if (pauseOverlay->isVisible()) {
                    pauseOverlay->setVisible(false);
                    device->getTimer()->start();
                }

                /*
                 Time difference calculation
                 */
                u32 time = device->getTimer()->getTime();
                f32 dtime; // step interval, in seconds
                if (time > lasttime)
                    dtime = (time - lasttime) / 1000.0;
                else
                    dtime = 0;
                lasttime = time;

                // Collected during the loop and displayed
                core::list<core::aabbox3d<f32> > hilightboxes;

                /*
                 Special keys
                 */

                v3f zoom_direction = v3f(0, 0, 1);
                v3f speed = v3f(0, 0, 0);
                if (client.init) {
                    zoom_direction.rotateXZBy(camera_yaw);
                    /*Camera zoom*/
                    if (receiver.IsKeyDown(irr::KEY_UP)) {
                        camera_zoom += zoom_speed;
                    }

                    if (receiver.IsKeyDown(irr::KEY_DOWN)) {
                        camera_zoom -= zoom_speed;
                    }
                    if (receiver.wheel != 0) {
                        camera_zoom += receiver.wheel;
                        receiver.wheel = 0;
                    }
                    if (camera_zoom < zoom_min) {
                        camera_zoom = zoom_min;
                    } else if (camera_zoom > zoom_max) {
                        camera_zoom = zoom_max;
                    }

                    /*Camera rotate*/
                    if (receiver.IsKeyDown(irr::KEY_LEFT)) {
                        camera_rotate -= rotate_speed;
                    }

                    if (receiver.IsKeyDown(irr::KEY_RIGHT)) {
                        camera_rotate += rotate_speed;
                    }

                    /*
                     Player speed control
                     */
                    // get movement direction
                    // default direction, facing ahead
                    v3f move_direction = v3f(0, 0, 1);
                    move_direction.rotateXZBy(camera_yaw);
                    // determine movement direction
                    if (receiver.IsKeyDown(irr::KEY_KEY_W)) {
                        speed += move_direction;
                    }
                    if (receiver.IsKeyDown(irr::KEY_KEY_S)) {
                        speed -= move_direction;
                    }
                    if (receiver.IsKeyDown(irr::KEY_KEY_A)) {
                        // counter-clockwise rotation against the plane formed by move_direction and Y direction
                        speed += move_direction.crossProduct(v3f(0, 1, 0));
                    }
                    if (receiver.IsKeyDown(irr::KEY_KEY_D)) {
                        // clockwise rotation against the plane: move_direction + Y direction
                        speed += move_direction.crossProduct(v3f(0, -1, 0));
                    }
                    if (receiver.IsKeyDown(irr::KEY_SPACE)) {
                        if (player->touching_ground) {
                            //player_speed.Y = 30*BS;
                            //player.speed.Y = 5*BS;
                            player->speed.Y = 6.5 * BS;
                        }
                    }
                }

                // Calculate the maximal movement speed of the player (Y is ignored): direction * max_speed
                speed = speed.normalize() * walkspeed_max / 4;
                // speed value change per loop
                f32 inc = walk_acceleration * BS * dtime;
                // new player speed calculation, limited by the max_speed
                // x-wise
                if (player->speed.X < speed.X - inc)
                    player->speed.X += inc; // positive new direction, new speed not exceeding the max_speed
                else if (player->speed.X > speed.X + inc)
                    player->speed.X -= inc; // negative new direction, new speed not exceeding the max_speed
                else if (player->speed.X < speed.X)
                    player->speed.X = speed.X; // positive new direction, new speed exceeding the max_speed
                else if (player->speed.X > speed.X)
                    player->speed.X = speed.X; // negative new direction, new speed exceeding the max_speed
                // z-wise
                if (player->speed.Z < speed.Z - inc)
                    player->speed.Z += inc;
                else if (player->speed.Z > speed.Z + inc)
                    player->speed.Z -= inc;
                else if (player->speed.Z < speed.Z)
                    player->speed.Z = speed.Z;
                else if (player->speed.Z > speed.Z)
                    player->speed.Z = speed.Z;

                /*
                 Process environment: simulation logical step increment
                 */
                // TODO: ???
                {
                    client.step(dtime);
                }
//                if (server != NULL) {
//                    server->step(dtime);
//                }

                /*
                 Mouse and camera control
                 */
                if (device->isWindowActive()) {
                    // bi-while loop change
                    if (first_loop_after_window_activation) {
                        first_loop_after_window_activation = false;
                    } else {
                        // calculate the range of pitch and yaw
//                    s32 dx = device->getCursorControl()->getPosition().X - 320;
//                    s32 dy = device->getCursorControl()->getPosition().Y - 240;
                        s32 dx = device->getCursorControl()->getPosition().X
                                - screenW / 2;
                        s32 dy = device->getCursorControl()->getPosition().Y
                                - screenH / 2;
                        // convert to angle value
                        camera_yaw -= dx * 0.2;
                        camera_pitch += dy * 0.2;
                        if (camera_pitch < -89.9)
                            camera_pitch = -89.9;
                        if (camera_pitch > 89.9)
                            camera_pitch = 89.9;
                    }
                    // reset cursor (i.e., crosshair) to the center of the screen
//                device->getCursorControl()->setPosition(320, 240);
                    device->getCursorControl()->setPosition(screenW / 2,
                            screenH / 2);
                } else {
                    first_loop_after_window_activation = true;
                }
                // default direction
                v3f camera_direction = v3f(0, 0, 1);
                // change the horizontal direction
                camera_direction.rotateYZBy(camera_pitch);
                // change the vertical direction
                camera_direction.rotateXZBy(camera_yaw);

                // change camera height to the position approximate to eye of player
//            v3f camera_position = player->getPosition()
//                    + v3f(0, BS + BS / 2, 0);

                v3f p = player->getPosition();
                // adjust camera if pressing left/right arrow
                zoom_direction.rotateXZBy(camera_rotate);
                camera_direction.rotateXZBy(camera_rotate);
                player->setRotation(v3f(0, -1 * camera_yaw, 0));
                // BS*1.7 is the value of PLAYER_HEIGHT in player.cpp
                v3f camera_position = p + v3f(0, BS * 1.65, zoom_max)
                        + zoom_direction * camera_zoom;

                // update camera position and look-at target
                camera->setPosition(camera_position);
                // look-at target: unit vector (representing the direction) from the current player position
                camera->setTarget(camera_position + camera_direction);
                // TODO: ???
                if (FIELD_OF_VIEW_TEST) {
                    //client.m_env.getMap().updateCamera(v3f(0,0,0), v3f(0,0,1));
                    client.updateCamera(v3f(0, 0, 0), v3f(0, 0, 1));
                } else {
                    //client.m_env.getMap().updateCamera(camera_position, camera_direction);
                    client.updateCamera(camera_position, camera_direction);
                }

                /*
                 Calculate which block the crosshair is pointing to:
                 by drawing a line between the player and the point d*BS units
                 distant from the player along with the camera direction
                 */
                // TODO
                for (map<v3s16, MapNode>::iterator it = selected.begin();
                        it != selected.end(); it++) {
                    client.restoreNode(it->first, it->second);
                }
                selected.clear();

                if (!receiver.walking && receiver.ctrl) {

                    //u32 t1 = device->getTimer()->getTime();
                    f32 d = 4; // max. distance: 5 BS
                    core::line3d<f32> shootline(camera_position,
                            camera_position + camera_direction * BS * (d + 1));
                    bool nodefound = false;
                    // position of the surface node intersecting the shootline
                    v3s16 nodepos;
                    v3s16 neighbourpos;
                    // the bounding box of the found block face
                    core::aabbox3d<f32> nodefacebox;
                    f32 mindistance = BS * 1001;
                    v3s16 pos_i = Map::floatToInt(player->getPosition());

                    s16 a = d;
                    // create a vector representing the shootline
                    // start < end
                    s16 ystart = pos_i.Y + 0 - (camera_direction.Y < 0 ? a : 1);
                    s16 zstart = pos_i.Z - (camera_direction.Z < 0 ? a : 1);
                    s16 xstart = pos_i.X - (camera_direction.X < 0 ? a : 1);
                    s16 yend = pos_i.Y + 1 + (camera_direction.Y > 0 ? a : 1);
                    s16 zend = pos_i.Z + (camera_direction.Z > 0 ? a : 1);
                    s16 xend = pos_i.X + (camera_direction.X > 0 ? a : 1);

                    for (s16 y = ystart; y <= yend; y++) {
                        for (s16 z = zstart; z <= zend; z++) {
                            for (s16 x = xstart; x <= xend; x++) {
                                try {
                                    //if(client.m_env.getMap().getNode(x,y,z).d == MATERIAL_AIR){
                                    if (client.getNode(v3s16(x, y, z)).d
                                            == MATERIAL_AIR) {
                                        continue;
                                    }
                                } catch (InvalidPositionException &e) {
                                    continue;
                                }

                                // node position
                                v3s16 np(x, y, z);
                                v3f npf = Map::intToFloat(np);

                                f32 d = 0.01;

                                // facets
                                v3s16 directions[6] = { v3s16(0, 0, 1), // back
                                v3s16(0, 1, 0), // top
                                v3s16(1, 0, 0), // right
                                v3s16(0, 0, -1), v3s16(0, -1, 0), v3s16(-1, 0,
                                        0), };

                                for (u16 i = 0; i < 6; i++) {
                                    //{u16 i=3;
                                    v3f dir_f = v3f(directions[i].X,
                                            directions[i].Y, directions[i].Z);
                                    v3f centerpoint = npf + dir_f * BS / 2; // center point of the face
                                    f32 distance = (centerpoint
                                            - camera_position).getLength(); // distance to the camera

                                    // find the closest block
                                    if (distance < mindistance) {
                                        //std::cout<<"for centerpoint=("<<centerpoint.X<<","<<centerpoint.Y<<","<<centerpoint.Z<<"): distance < mindistance"<<std::endl;
                                        //std::cout<<"npf=("<<npf.X<<","<<npf.Y<<","<<npf.Z<<")"<<std::endl;
                                        // TODO: ???
                                        core::CMatrix4<f32> m;
                                        m.buildRotateFromTo(v3f(0, 0, 1),
                                                dir_f);
                                        // This is the back face
                                        v3f corners[2] = { v3f(BS / 2, BS / 2,
                                        BS / 2), v3f(-BS / 2, -BS / 2,
                                        BS / 2 + d) };
                                        for (u16 j = 0; j < 2; j++) {
                                            m.rotateVect(corners[j]);
                                            corners[j] += npf;
                                            //std::cout<<"box corners["<<j<<"]: ("<<corners[j].X<<","<<corners[j].Y<<","<<corners[j].Z<<")"<<std::endl;
                                        }
                                        //core::aabbox3d<f32> facebox(corners[0],corners[1]);
                                        core::aabbox3d<f32> facebox(corners[0]);
                                        facebox.addInternalPoint(corners[1]);
                                        // find the face of the block
                                        if (facebox.intersectsWithLine(
                                                shootline)) {
                                            nodefound = true;
                                            nodepos = np;
                                            neighbourpos = np + directions[i];
                                            // find the closest block
                                            mindistance = distance;
                                            nodefacebox = facebox;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // highlight the selected surface and add/remove block
                    if (nodefound) {
                        //std::cout<<"nodefound == true"<<std::endl;
                        //std::cout<<"nodepos=("<<nodepos.X<<","<<nodepos.Y<<","<<nodepos.Z<<")"<<std::endl;
                        //std::cout<<"neighbourpos=("<<neighbourpos.X<<","<<neighbourpos.Y<<","<<neighbourpos.Z<<")"<<std::endl;

                        static v3s16 nodepos_old(-1, -1, -1);
                        if (nodepos != nodepos_old) {
                            std::cout << "Pointing at (" << nodepos.X << ","
                                    << nodepos.Y << "," << nodepos.Z << ")"
                                    << std::endl;
                            nodepos_old = nodepos;

                            /*wchar_t positiontext[20];
                             swprintf(positiontext, 20, L"(%i,%i,%i)",
                             nodepos.X, nodepos.Y, nodepos.Z);
                             positiontextgui->setText(positiontext);*/
                        }

                        // for highlighting the found surface of the block
                        // TODO
//                hilightboxes.push_back(nodefacebox);
                        selected[nodepos] = client.getNode(nodepos);
                        client.highlightNode(nodepos);

                        if (receiver.leftclicked) {
                            // TODO
                            selected.erase(nodepos);

                            std::cout << "Removing block (MapNode)"
                                    << std::endl;
                            u32 time1 = device->getTimer()->getRealTime();

                            //client.m_env.getMap().removeNodeAndUpdate(nodepos);
                            client.removeNode(nodepos);

                            u32 time2 = device->getTimer()->getRealTime();
                            u32 dtime = time2 - time1;
                            std::cout << "Took " << dtime << "ms" << std::endl;
                        }
                        if (receiver.rightclicked) {
                            // TODO
                            client.restoreNode(nodepos, selected[nodepos]);
                            selected.erase(nodepos);

                            std::cout << "Placing block (MapNode)" << std::endl;
                            u32 time1 = device->getTimer()->getRealTime();

                            /*f32 light = client.m_env.getMap().getNode(neighbourpos).light;
                             MapNode n;
                             n.d = g_selected_material;
                             client.m_env.getMap().setNode(neighbourpos, n);
                             client.m_env.getMap().nodeAddedUpdate(neighbourpos, light);*/
                            MapNode n;
                            n.d = g_selected_material;
                            client.addNode(neighbourpos, n);

                            u32 time2 = device->getTimer()->getRealTime();
                            u32 dtime = time2 - time1;
                            std::cout << "Took " << dtime << "ms" << std::endl;
                        }
                    } else {
                        //std::cout<<"nodefound == false"<<std::endl;
                        //positiontextgui->setText(L"");
                    }
                }

                receiver.leftclicked = false;
                receiver.rightclicked = false;

                static u8 old_selected_material = MATERIAL_AIR;
                static u16 old_selected_gear = 0;
                static u16 old_selected_skin = 1;
                if (g_selected_gear != old_selected_gear
                        || g_selected_skin != old_selected_skin) {
                    Player* player = client.getLocalPlayer();
                    player->updateMesh(smgr, g_selected_skin, g_selected_gear);
                }
                if (g_selected_material != old_selected_material
                        || g_selected_gear != old_selected_gear
                        || g_selected_skin != old_selected_skin) {
                    old_selected_material = g_selected_material;
                    old_selected_gear = g_selected_gear;
                    old_selected_skin = g_selected_skin;
                    wchar_t temptext[80];
                    swprintf(temptext, 80,
                            L"Minetest-c55 (F: material=%i, G: gear=%i, V: skin=%i)",
                            g_selected_material, g_selected_gear,
                            g_selected_skin);
                    guitext->setText(temptext);
                }

                // update location indicator
                wchar_t pos_temp[100];
                v3f position = client.getLocalPlayer()->getPosition();
                swprintf(pos_temp, 100, L"Current position: (%ld, %ld, %ld)",
                        (signed long) position.X, (signed long) position.Y,
                        (signed long) position.Z);
                pos_text->setText(pos_temp);

                // update direction indicator
                wchar_t rot_temp[100];
                float rotation = client.getLocalPlayer()->getRotation().Y;
                float angle =
                        rotation >= 0 ?
                                fmod(rotation, 360.0) :
                                360.0 + fmod(rotation, 360.0);
                swprintf(rot_temp, 100,
                        L"Current rotation clockwise to the north: %.1f degree",
                        angle);
                rot_text->setText(rot_temp);

                // signal the player state load
                wchar_t load_temp[40];
                if (!client.init) {
                    swprintf(load_temp, 40,
                            L" Loading player state, please wait ......");
                    load_text->setDrawBackground(true);
                    load_text->setDrawBorder(true);
                } else {
                    swprintf(load_temp, 40, L"");
                    load_text->setDrawBackground(false);
                    load_text->setDrawBorder(false);
                }
                load_text->setText(load_temp);

                /*
                 Drawing begins
                 */

                /*
                 Background color is choosen based on whether the player is
                 much beyond the initial ground level
                 */
                /*video::SColor bgcolor;
                 v3s16 p0 = Map::floatToInt(player->position);
                 s16 gy = client.m_env.getMap().getGroundHeight(v2s16(p0.X, p0.Z));
                 if(p0.Y > gy - MAP_BLOCKSIZE)
                 bgcolor = video::SColor(255,90,140,200);
                 else
                 bgcolor = video::SColor(255,0,0,0);*/
                video::SColor bgcolor = video::SColor(255, 90, 140, 200);

                driver->beginScene(true, true, bgcolor);

                //std::cout<<"smgr->drawAll()"<<std::endl;

                smgr->drawAll();

                // draw the crosshair
                core::vector2d<s32> displaycenter(screenW / 2, screenH / 2);
                driver->draw2DLine(displaycenter - core::vector2d<s32>(10, 0),
                        displaycenter + core::vector2d<s32>(10, 0),
                        video::SColor(255, 255, 255, 255));
                driver->draw2DLine(displaycenter - core::vector2d<s32>(0, 10),
                        displaycenter + core::vector2d<s32>(0, 10),
                        video::SColor(255, 255, 255, 255));

                /*
                 * draw a box for the highlighted face
                 * currently, there is only one box, since the hilightboxes is always re-initialized in each loop
                 */
                video::SMaterial m;
                m.Thickness = 10;
                m.Lighting = false;
                driver->setMaterial(m);
                for (core::list<core::aabbox3d<f32> >::Iterator i =
                        hilightboxes.begin(); i != hilightboxes.end(); i++) {
                    driver->draw3DBox(*i, video::SColor(255, 0, 0, 0));
                }

                guienv->drawAll();

                driver->endScene();

                /*
                 Drawing ends
                 */

                // display the current FPS on GUI
                u16 fps = driver->getFPS();
                if (lastFPS != fps) {
                    core::stringw str = L"Minetest [";
                    str += driver->getName();
                    str += "] FPS:";
                    str += fps;
                    device->setWindowCaption(str.c_str());
                    lastFPS = fps;
                }

                /*}
                 else
                 device->yield();*/
            }
        } // End of while loop

//        if (server != NULL)
//            delete server;

    } // client is deleted at this point

    /*
     In the end, delete the Irrlicht device.
     */
    device->drop();

    return 0;
}

//END
