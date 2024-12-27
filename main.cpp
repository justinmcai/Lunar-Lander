// main.cpp
#define LOG(argument) std::cout << argument << '\n'
#define STB_IMAGE_IMPLEMENTATION
#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"


// ————— CONSTANTS ————— //
constexpr int WINDOW_WIDTH = 640,
WINDOW_HEIGHT = 480;

constexpr float BG_RED = 0.0f,
BG_BLUE = 0.0f,
BG_GREEN = 0.0f,
BG_OPACITY = 1.0f;

constexpr int VIEWPORT_X = 0,
VIEWPORT_Y = 0,
VIEWPORT_WIDTH = WINDOW_WIDTH,
VIEWPORT_HEIGHT = WINDOW_HEIGHT;

constexpr char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

constexpr float MILLISECONDS_IN_SECOND = 1000.0;
constexpr char  PLATFORM_FILEPATH[] = "assets/platformPack_tile027.png";
constexpr char PLAYER_TEXTURE_FILEPATH[] = "assets/SpaceShip.png";
constexpr char FONT_TEXTURE_FILEPATH[] = "assets/font1.png";
GLuint g_font_texture_id;

constexpr int FONTBANK_SIZE = 16;
constexpr GLint NUMBER_OF_TEXTURES = 1;
constexpr GLint LEVEL_OF_DETAIL = 0;
constexpr GLint TEXTURE_BORDER = 0;

constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;
constexpr float ACC_OF_GRAVITY = -9.81f;
constexpr int   PLATFORM_COUNT = 4;

constexpr float INITIAL_FUEL = 500.0f; // Maximum fuel
float g_player_fuel = INITIAL_FUEL;   // Current fuel level


// ————— STRUCTS AND ENUMS —————//
enum AppStatus { RUNNING, TERMINATED };
enum MissionStatus { NONE, FAILED, ACCOMPLISHED };
MissionStatus g_mission_status = NONE;


struct GameState
{
    Entity* player;
    Entity* platforms;
};

// ————— VARIABLES ————— //
GameState g_game_state;

SDL_Window* g_display_window;
AppStatus g_app_status = RUNNING;

ShaderProgram g_shader_program = ShaderProgram();
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_time_accumulator = 0.0f;

// ———— GENERAL FUNCTIONS ———— //
GLuint load_texture(const char* filepath);

void initialise();
void process_input();
void update();
void render();
void shutdown();

GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components,
        STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER,
        GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return textureID;
}

void draw_text(ShaderProgram* program, GLuint font_texture_id, std::string text,
    float font_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairs—one for
    // each character. Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their
        //    position relative to the whole sentence)
        int spritesheet_index = (int)text[i];  // ascii value of character
        float offset = (font_size + spacing) * i;

        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float)(spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float)(spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            });

        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
            });
    }

    // 4. And render all of them using the pairs
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    program->set_model_matrix(model_matrix);
    glUseProgram(program->get_program_id());

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0,
        vertices.data());
    glEnableVertexAttribArray(program->get_position_attribute());
    glVertexAttribPointer(program->get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0,
        texture_coordinates.data());
    glEnableVertexAttribArray(program->get_tex_coordinate_attribute());

    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int)(text.size() * 6));

    glDisableVertexAttribArray(program->get_position_attribute());
    glDisableVertexAttribArray(program->get_tex_coordinate_attribute());
}

void initialise()
{
    SDL_Init(SDL_INIT_VIDEO);
    g_display_window = SDL_CreateWindow("Hello, Entities!",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

    if (context == nullptr)
    {
        LOG("ERROR: Could not create OpenGL context.\n");
        shutdown();
    }

#ifdef _WINDOWS
    glewInit();
#endif

    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    // ————— PLAYER ————— //
    GLuint player_texture_id = load_texture(PLAYER_TEXTURE_FILEPATH);

    // Initialize the player entity with the new texture
    g_game_state.player = new Entity(player_texture_id, 1.0f);

    // Set the player's starting properties
    g_game_state.player->set_acceleration(glm::vec3(0.0f, ACC_OF_GRAVITY * 0.1, 0.0f));
    g_game_state.player->set_position(glm::vec3(0.0f, 0.0f, 0.0f)); // Initial position

    // ————— PLATFORM ————— //
    g_game_state.platforms = new Entity[PLATFORM_COUNT];

    g_game_state.platforms[0].set_texture_id(load_texture(PLATFORM_FILEPATH));
    g_game_state.platforms[0].set_position(glm::vec3(-4.0f, -3.0f, 0.0f)); // Leftmost platform
    g_game_state.platforms[0].update(0.0f, nullptr, 0);

    g_game_state.platforms[1].set_texture_id(load_texture(PLATFORM_FILEPATH));
    g_game_state.platforms[1].set_position(glm::vec3(-2.0f, -2.5f, 0.0f)); // Higher platform
    g_game_state.platforms[1].update(0.0f, nullptr, 0);

    g_game_state.platforms[2].set_texture_id(load_texture(PLATFORM_FILEPATH));
    g_game_state.platforms[2].set_position(glm::vec3(2.0f, -2.0f, 0.0f)); // Slightly higher platform
    g_game_state.platforms[2].update(0.0f, nullptr, 0);

    g_game_state.platforms[3].set_texture_id(load_texture(PLATFORM_FILEPATH));
    g_game_state.platforms[3].set_position(glm::vec3(4.0f, -3.0f, 0.0f)); // Rightmost platform
    g_game_state.platforms[3].update(0.0f, nullptr, 0);

    // FONT
    g_font_texture_id = load_texture(FONT_TEXTURE_FILEPATH);

    // ————— GENERAL ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    // VERY IMPORTANT: If nothing is pressed, we don't want to go anywhere
    glm::vec3 acceleration = glm::vec3(0.0f, ACC_OF_GRAVITY * 0.1f, 0.0f);

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_app_status = TERMINATED;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_app_status = TERMINATED;
                break;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);

    if (g_player_fuel > 0.0f)
    {
        if (key_state[SDL_SCANCODE_LEFT])
        {
            acceleration.x = -1.0f;
            g_player_fuel -= 0.01f; // Consume fuel
        }
        else if (key_state[SDL_SCANCODE_RIGHT])
        {
            acceleration.x = 1.0f;
            g_player_fuel -= 0.01f; // Consume fuel
        }

        if (key_state[SDL_SCANCODE_UP])
        {
            acceleration.y = 1.0f;
            g_player_fuel -= 0.01f; // Consume fuel
        }
    }

    g_game_state.player->set_acceleration(acceleration);

    // Prevent fuel from going below zero
    if (g_player_fuel < 0.0f)
        g_player_fuel = 0.0f;


    // This makes sure that the player can't move faster diagonally
    if (glm::length(g_game_state.player->get_movement()) > 1.0f)
        g_game_state.player->normalise_movement();
}

void update()
{
    // ————— DELTA TIME ————— //
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    // ————— FIXED TIMESTEP ————— //
    // STEP 1: Keep track of how much time has passed since last step
    delta_time += g_time_accumulator;

    // STEP 2: Accumulate the ammount of time passed while we're under our fixed timestep
    if (delta_time < FIXED_TIMESTEP)
    {
        g_time_accumulator = delta_time;
        return;
    }

    // STEP 3: Once we exceed our fixed timestep, apply that elapsed time into the 
    //         objects' update function invocation
    while (delta_time >= FIXED_TIMESTEP)
    {
        if (g_mission_status == NONE)
        {
            // Update the player and check for conditions
            g_game_state.player->update(FIXED_TIMESTEP, g_game_state.platforms, PLATFORM_COUNT);

            // Check if the player touches the borders (failure condition)
            glm::vec3 player_pos = g_game_state.player->get_position();
            if (player_pos.x < -5.0f || player_pos.x > 5.0f || player_pos.y < -3.75f || player_pos.y > 3.75f)
            {
                g_mission_status = FAILED;
                break;
            }

            // Check if the player lands on a platform (success condition)
            for (int i = 0; i < PLATFORM_COUNT; i++)
            {
                if (g_game_state.player->check_collision(&g_game_state.platforms[i]))
                {
                    g_mission_status = ACCOMPLISHED;
                    break;
                }
            }
        }

        delta_time -= FIXED_TIMESTEP;
    }

    g_time_accumulator = delta_time;
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    if (g_mission_status == FAILED)
    {
        draw_text(&g_shader_program, g_font_texture_id, "MISSION FAILED", 0.5f, 0.05f, glm::vec3(-4.0f, 0.0f, 0.0f));
    }
    else if (g_mission_status == ACCOMPLISHED)
    {
        draw_text(&g_shader_program, g_font_texture_id, "MISSION", 0.5f, 0.05f, glm::vec3(-4.0f, 0.5f, 0.0f));
        draw_text(&g_shader_program, g_font_texture_id, "ACCOMPLISHED", 0.5f, 0.05f, glm::vec3(-4.0f, 0.0f, 0.0f));
    }
    else
    {
        // Render player and platforms
        g_game_state.player->render(&g_shader_program);
        for (int i = 0; i < PLATFORM_COUNT; i++)
        {
            g_game_state.platforms[i].render(&g_shader_program);
        }

        // Render fuel level
        std::string fuel_text = "Fuel: " + std::to_string((int)g_player_fuel); // Convert to integer for display
        draw_text(&g_shader_program, g_font_texture_id, fuel_text, 0.5f, 0.05f, glm::vec3(-4.5f, 3.0f, 0.0f)); // Top-left
    }

    SDL_GL_SwapWindow(g_display_window);
}


void shutdown()
{
    SDL_Quit();

    delete   g_game_state.player;
    delete[] g_game_state.platforms;
}


int main(int argc, char* argv[])
{
    initialise();

    while (g_app_status == RUNNING)
    {
        process_input();

        // Only update the game if the mission status is NONE
        if (g_mission_status == NONE)
        {
            update();
        }

        render();
    }

    shutdown();
    return 0;
}