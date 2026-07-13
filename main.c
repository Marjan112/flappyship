#include <raylib.h>
#include <raymath.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

#include <sounds_SFX_Jump_09.wav.h>
#include <sounds_explosion.wav.h>
#include <sounds_wha-wha.mp3.h>

#include <textures_DurrrSpaceShip_0.png.h>
#include <textures_Layered_Rock_0.png.h>
#include <textures_Space_Background.png.h>
#include <textures_explosion_pixelfied.png.h>

#define SCREEN_WIDTH                600
#define SCREEN_HEIGHT               800

#define GRAVITY                     1000

#define HITBOX_FACTOR               0.8f

#define BACKGROUND_SCROLL_SPEED     70

#define VELOCITY_ACCELERATE         5

#define SHIP_WIDTH                  50
#define SHIP_HEIGHT                 50
#define SHIP_HITBOX_WIDTH           (SHIP_WIDTH*HITBOX_FACTOR)
#define SHIP_HITBOX_HEIGHT          (SHIP_HEIGHT*HITBOX_FACTOR)
#define SHIP_JUMP_SPEED             500
#define SHIP_ROTATION_SPEED         4

#define OBSTACLE_WIDTH              70
#define OBSTACLE_HEIGHT             70
#define OBSTACLE_HITBOX_WIDTH       (OBSTACLE_WIDTH*HITBOX_FACTOR)
#define OBSTACLE_HITBOX_HEIGHT      (OBSTACLE_HEIGHT*HITBOX_FACTOR)
#define OBSTACLE_ROTATION_SPEED     10
#define OBSTACLE_VELOCITY           100

#define EXPLOSION_COLUMN            4
#define EXPLOSION_ROW               4
#define EXPLOSION_FRAMES            (EXPLOSION_COLUMN*EXPLOSION_ROW)

#define da_append(array, item)                                                                      \
    do {                                                                                            \
        if ((array)->count >= (array)->capacity) {                                                  \
            (array)->capacity = (array)->capacity == 0 ? 1 : (array)->capacity * 2;                 \
            (array)->items = realloc((array)->items, (array)->capacity*sizeof(*(array)->items));    \
            assert((array)->items != NULL);                                                         \
        }                                                                                           \
        (array)->items[(array)->count++] = (item);                                                  \
    } while (false)

#define da_remove_unordered(array, i)                               \
    do {                                                            \
        const size_t j = (i);                                       \
        (array)->items[j] = (array)->items[--(array)->count];       \
    } while (false)                                                 \

static Texture load_texture_from_memory(const char *file_type, const unsigned char *file_data, int data_size)
{
    Image image = LoadImageFromMemory(file_type, file_data, data_size);
    Texture texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

static Sound load_sound_from_memory(const char *file_type, const unsigned char *file_data, int data_size)
{
    Wave wave = LoadWaveFromMemory(file_type, file_data, data_size);
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

static void draw_text_centered(int font_size, Color color, const char *text, ...)
{
    char buf[1024] = {0};

    va_list args;
    va_start(args, text);
    vsnprintf(buf, sizeof(buf), text, args);
    va_end(args);

    int max_width = 0, line_count = 0;

    char **lines = TextSplit(buf, '\n', &line_count);
    for (int i = 0; i < line_count; ++i) {
        int width = MeasureText(lines[i], font_size);
        if (width > max_width) max_width = width;
    }

    DrawText(buf, SCREEN_WIDTH / 2 - max_width / 2, SCREEN_HEIGHT / 2 - (line_count*font_size) / 2, font_size, color);
}

typedef struct {
    Texture texture;
    float position; // y-position
    float scroll_speed;
} Background;

Background new_background()
{
    return CLITERAL(Background) {
        .texture = load_texture_from_memory(".png", textures_Space_Background_png, textures_Space_Background_png_size),
        .position = 0,
        .scroll_speed = BACKGROUND_SCROLL_SPEED
    };
}

typedef struct {
    Sound jump_sound;
    Texture texture;
    Rectangle hitbox;
    Vector2 position;
    float velocity;
    float rotation;
} Ship;

Ship new_ship()
{
    const Vector2 position = {
        .x = SCREEN_WIDTH / 2.0f - SHIP_WIDTH / 2.0f,
        .y = SCREEN_HEIGHT / 2.0f - SHIP_HEIGHT / 2.0f,
    };

    return CLITERAL(Ship) {
        .jump_sound = load_sound_from_memory(".wav", sounds_SFX_Jump_09_wav, sounds_SFX_Jump_09_wav_size),
        .texture = load_texture_from_memory(".png", textures_DurrrSpaceShip_0_png, textures_DurrrSpaceShip_0_png_size),
        .hitbox = CLITERAL(Rectangle) {
            .x = position.x - SHIP_WIDTH / 2.0f,
            .y = position.y - SHIP_HEIGHT / 2.0f,
            .width = SHIP_HITBOX_WIDTH,
            .height = SHIP_HITBOX_HEIGHT
        },
        .position = position,
        .velocity = 0,
        .rotation = 0,
    };
}

typedef struct {
    Sound sound;
    Texture texture;
    Vector2 position;
    float velocity;
    int frame;
    float timer;
    float frame_time;
    bool active;
} Explosion;

typedef struct {
    Sound shared_sound;
    Texture shared_texture;
    Explosion *items;
    size_t count;
    size_t capacity;
} Explosions;

Explosion new_explosion(float x, float y, float velocity, Texture texture, Sound sound) {
    return CLITERAL(Explosion) {
        .sound = sound,
        .texture = texture,
        .velocity = velocity,
        .position = CLITERAL(Vector2) {
            .x = x,
            .y = y
        },
        .frame = 0,
        .timer = 0,
        .frame_time = 0.05f,
        .active = true
    };
}

void explosion_update(Explosion *explosion, float delta_time) {
    if (!explosion->active) return;

    explosion->timer += delta_time;

    if (!IsSoundPlaying(explosion->sound)) PlaySound(explosion->sound);

    if (explosion->timer >= explosion->frame_time) {
        explosion->frame++;
        if (explosion->frame >= EXPLOSION_FRAMES) explosion->active = false;
        explosion->timer -= explosion->frame_time;
    }
}

void explosion_draw(Explosion explosion) {
    if (!explosion.active) return;

    const int frame_width = explosion.texture.width/EXPLOSION_COLUMN;
    const int frame_height = explosion.texture.height/EXPLOSION_ROW;

    Rectangle source = {
        .x = (explosion.frame % 4)*frame_width,
        .y = (explosion.frame / 4.0f)*frame_height,
        .width = frame_width,
        .height = frame_height
    };

    Rectangle dest = {
        .x = explosion.position.x,
        .y = explosion.position.y,
        .width = 64,
        .height = 64
    };

    Vector2 origin = {32, 32};
    DrawTexturePro(explosion.texture, source, dest, origin, 0, WHITE);
}

typedef struct {
    Texture texture;
    Rectangle hitbox;
    Vector2 position;
    float rotation;
    bool destroyed;
} Obstacle;

Obstacle new_obstacle(int x, int y, Texture texture)
{
    return CLITERAL(Obstacle) {
        .texture = texture,
        .hitbox = CLITERAL(Rectangle) {
            .x = x - OBSTACLE_WIDTH / 2.0f,
            .y = y - OBSTACLE_HEIGHT / 2.0f,
            .width = OBSTACLE_HITBOX_WIDTH,
            .height = OBSTACLE_HITBOX_HEIGHT
        },
        .position = CLITERAL(Vector2) {
            .x = x,
            .y = y
        },
        .rotation = 0,
        .destroyed = false
    };
}

typedef struct {
    Texture shared_texture;
    Obstacle *items;
    size_t count;
    size_t capacity;
    float spawn_timer;
    float spawn_interval;
    float velocity;
} Obstacles;

typedef enum {
    GAME_STATE_NORMAL,
    GAME_STATE_SHIP_EXPLOSION,
    GAME_STATE_OVER,
    GAME_STATE_PAUSE
} GameState;

typedef struct {
    Ship ship;
    Explosions explosions;
    Obstacles obstacles;
    Sound wha_wha_sound;
    Background background;
    int destroyed_obstacles;
    int obstacles_missed;
    int last_speedup;
    GameState state;
    bool draw_hitbox;
} Game;

Game game_init()
{
    Obstacles obstacles = {0};
    obstacles.shared_texture = load_texture_from_memory(".png", textures_Layered_Rock_0_png, textures_Layered_Rock_0_png_size);
    obstacles.spawn_interval = 3;
    obstacles.velocity = OBSTACLE_VELOCITY;

    Explosions explosions = {0};
    explosions.shared_sound = load_sound_from_memory(".wav", sounds_explosion_wav, sounds_explosion_wav_size);
    explosions.shared_texture = load_texture_from_memory(".png", textures_explosion_pixelfied_png, textures_explosion_pixelfied_png_size);

    return CLITERAL(Game) {
        .obstacles = obstacles,
        .ship = new_ship(),
        .wha_wha_sound = load_sound_from_memory(".mp3", sounds_wha_wha_mp3, sounds_wha_wha_mp3_size),
        .explosions = explosions,
        .background = new_background(),
        .destroyed_obstacles = 0,
        .obstacles_missed = 0,
        .last_speedup = 0,
        .draw_hitbox = false,
        .state = GAME_STATE_NORMAL
    };
}

void game_reset(Game *game) {
    game->ship.velocity = 0;
    game->ship.position.y = SCREEN_HEIGHT / 2.0f - SHIP_HEIGHT / 2.0f;
    game->ship.hitbox.y = game->ship.position.y;

    game->background.position = 0;
    game->background.scroll_speed = BACKGROUND_SCROLL_SPEED;

    game->state = GAME_STATE_NORMAL;

    game->obstacles.count = 0;
    game->obstacles.spawn_timer = 0;
    game->obstacles.spawn_interval = 3;
    game->obstacles.velocity = OBSTACLE_VELOCITY;

    game->explosions.count = 0;

    game->destroyed_obstacles = 0;
    game->obstacles_missed = 0;
    game->last_speedup = 0;
}

void game_draw_game_over(Game *game)
{
    int score = game->destroyed_obstacles - game->obstacles_missed;

    if (score < 0) {
        draw_text_centered(30, WHITE,
                          "Game over\n"
                          "You missed too much\n"
                          "Destroyed obstacles: %d\n"
                          "Missed obstacles: %d\n"
                          "Press ENTER to restart.",
                          game->destroyed_obstacles, game->obstacles_missed);
    } else {
        draw_text_centered(30, WHITE,
                          "Game over\n"
                          "Destroyed obstacles: %d\n"
                          "Missed obstacles: %d\n"
                          "Score: %d\n"
                          "Press ENTER to restart.",
                          game->destroyed_obstacles, game->obstacles_missed, score);
    }
}

void background_update(Background *background, float delta_time)
{
    background->position -= background->scroll_speed*delta_time;
    if (background->position <= -background->texture.width) background->position = 0;
}

void background_draw(Background background)
{
    DrawTexture(background.texture, background.position, 0, WHITE);
    DrawTexture(background.texture, background.position + background.texture.width, 0, WHITE);
}

void game_update_ship(Game *game, float delta_time)
{
    game->ship.velocity += GRAVITY*delta_time;

    if (IsKeyPressed(KEY_SPACE)) {
        game->ship.velocity = -SHIP_JUMP_SPEED;
        game->ship.rotation = -30;
        PlaySound(game->ship.jump_sound);
    }

    if (game->state == GAME_STATE_NORMAL && IsSoundPlaying(game->wha_wha_sound))
        StopSound(game->wha_wha_sound);

    if (game->ship.hitbox.y + SHIP_HITBOX_HEIGHT / 2.0f >= SCREEN_HEIGHT) {
        PlaySound(game->wha_wha_sound);
        game->state = GAME_STATE_OVER;
    } else if (game->ship.position.y - SHIP_HITBOX_HEIGHT / 2.0f <= 0) game->ship.velocity = 100;

    float target_rotation = 0;

    if (game->ship.velocity < 0) target_rotation = -30;
    else target_rotation = 45;

    game->ship.position.y += game->ship.velocity*delta_time;
    game->ship.hitbox.x = game->ship.position.x - SHIP_WIDTH / 2.0f;
    game->ship.hitbox.y = game->ship.position.y - SHIP_WIDTH / 2.0f;
    game->ship.rotation = Lerp(game->ship.rotation, target_rotation, SHIP_ROTATION_SPEED*delta_time);
}

void game_draw_ship(Game game)
{
    Rectangle source = {0, 0, (float)game.ship.texture.width, (float)game.ship.texture.height};
    Rectangle dest = {game.ship.position.x, game.ship.position.y, SHIP_WIDTH, SHIP_HEIGHT};
    Vector2 origin = {SHIP_WIDTH / 2.0f, SHIP_HEIGHT / 2.0f};
    DrawTexturePro(game.ship.texture, source, dest, origin, game.ship.rotation, WHITE);
    if (game.draw_hitbox) DrawRectangleLines(game.ship.hitbox.x, game.ship.hitbox.y, game.ship.hitbox.width, game.ship.hitbox.height, RED);
}

void game_update_obstacle(Game *game, Obstacle *obstacle, float delta_time)
{
    obstacle->position.x -= game->obstacles.velocity*delta_time;
    obstacle->hitbox.x = obstacle->position.x - OBSTACLE_WIDTH / 2.0f;
    obstacle->hitbox.y = obstacle->position.y - OBSTACLE_HEIGHT / 2.0f;

    if (obstacle->position.x <= 0) {
        obstacle->destroyed = true;
        ++game->obstacles_missed;
        if (game->destroyed_obstacles - game->obstacles_missed < 0) {
            PlaySound(game->wha_wha_sound);
            game->state = GAME_STATE_OVER;
        }
        return;
    }

    obstacle->rotation += OBSTACLE_ROTATION_SPEED*delta_time;
    if (obstacle->rotation >= 360) obstacle->rotation = 0;

    if (CheckCollisionRecs(obstacle->hitbox, game->ship.hitbox)) {
        PlaySound(game->wha_wha_sound);

        game->state = GAME_STATE_SHIP_EXPLOSION;
        game->background.scroll_speed = 0;

        Explosion explosion = new_explosion(
            game->ship.position.x, game->ship.position.y,
            0,
            game->explosions.shared_texture, LoadSoundAlias(game->explosions.shared_sound));
        da_append(&game->explosions, explosion);
        return;
    }

    if (game->ship.position.x > obstacle->position.x && game->ship.position.y < obstacle->position.y - OBSTACLE_HEIGHT / 2.0f) {
        obstacle->destroyed = true;
        ++game->destroyed_obstacles;
        Explosion explosion = new_explosion(
            obstacle->position.x, obstacle->position.y,
            game->obstacles.velocity,
            game->explosions.shared_texture, LoadSoundAlias(game->explosions.shared_sound));
        da_append(&game->explosions, explosion);
    }
}

void game_draw_obstacle(Game game, size_t obstacle_index)
{
    Obstacle obstacle = game.obstacles.items[obstacle_index];
    Rectangle source = {0, 0, obstacle.texture.width, obstacle.texture.height};
    Rectangle dest = {obstacle.position.x, obstacle.position.y, OBSTACLE_WIDTH, OBSTACLE_HEIGHT};
    Vector2 origin = {OBSTACLE_WIDTH / 2.0f, OBSTACLE_HEIGHT / 2.0f};

    DrawTexturePro(obstacle.texture, source, dest, origin, obstacle.rotation, WHITE);
    if (game.draw_hitbox) DrawRectangleLines(obstacle.hitbox.x, obstacle.hitbox.y, obstacle.hitbox.width, obstacle.hitbox.height, RED);
}

void game_update_obstacles(Game *game, float delta_time)
{
    for (size_t i = 0; i < game->obstacles.count;) {
        Obstacle *obstacle = &game->obstacles.items[i];

        game_update_obstacle(game, obstacle, delta_time);

        if (obstacle->destroyed) {
            da_remove_unordered(&game->obstacles, i);
            continue;
        }

        ++i;
    }
}

void game_draw_obstacles(Game game)
{
    for (size_t i = 0; i < game.obstacles.count; ++i) game_draw_obstacle(game, i);
}

bool game_update_explosions(Game *game, float delta_time)
{
    for (size_t i = 0; i < game->explosions.count;) {
        Explosion *explosion = &game->explosions.items[i];

        explosion_update(explosion, delta_time);
        explosion->position.x -= explosion->velocity*delta_time;

        if (!explosion->active) {
            da_remove_unordered(&game->explosions, i);
            continue;
        }

        ++i;
    }

    return game->explosions.count != 0;
}

void game_draw_explosions(Game game)
{
    for (size_t i = 0; i < game.explosions.count; ++i) explosion_draw(game.explosions.items[i]);
}

void game_update(Game *game, float delta_time)
{
    game->obstacles.spawn_timer += delta_time;

    switch (game->state) {
        case GAME_STATE_NORMAL:
            background_update(&game->background, delta_time);

            if (IsKeyPressed(KEY_H)) game->draw_hitbox = !game->draw_hitbox;
            if (IsKeyPressed(KEY_P)) game->state = GAME_STATE_PAUSE;

            if (game->destroyed_obstacles > game->last_speedup && game->destroyed_obstacles % 5 == 0) {
                game->last_speedup = game->destroyed_obstacles;
                game->obstacles.velocity += VELOCITY_ACCELERATE;
                game->background.scroll_speed += VELOCITY_ACCELERATE;
            }

            if (game->obstacles.spawn_timer >= game->obstacles.spawn_interval) {
                int margin = OBSTACLE_HEIGHT / 2;
                int y = GetRandomValue(margin, SCREEN_HEIGHT - margin);
                Obstacle new = new_obstacle(SCREEN_WIDTH + OBSTACLE_WIDTH, y, game->obstacles.shared_texture);
                da_append(&game->obstacles, new);
                game->obstacles.spawn_timer = 0;
                game->obstacles.spawn_interval = GetRandomValue(15, 30) / 10.0f;
            }

            game_update_obstacles(game, delta_time);
            game_update_explosions(game, delta_time);
            game_update_ship(game, delta_time);
            break;
        case GAME_STATE_SHIP_EXPLOSION:
            if (!game_update_explosions(game, delta_time)) game->state = GAME_STATE_OVER;
            break;
        case GAME_STATE_OVER:
            if (IsKeyPressed(KEY_ENTER)) game_reset(game);
            break;
        case GAME_STATE_PAUSE:
            if (IsKeyPressed(KEY_P)) game->state = GAME_STATE_NORMAL;
            break;
        default: assert(false && "Unreachable");
    }
}

void game_draw(Game game)
{
    background_draw(game.background);

    switch (game.state) {
        case GAME_STATE_NORMAL:
            game_draw_obstacles(game);
            game_draw_explosions(game);
            game_draw_ship(game);

            DrawText(TextFormat("Score: %d", game.destroyed_obstacles - game.obstacles_missed), 0.05f*SCREEN_WIDTH, 0.05f*SCREEN_HEIGHT, 30, WHITE);
            break;
        case GAME_STATE_SHIP_EXPLOSION:
            game_draw_explosions(game);
            break;
        case GAME_STATE_OVER:
            game_draw_game_over(&game);
            break;
        case GAME_STATE_PAUSE:
            draw_text_centered(30, WHITE, "Paused");
            break;
        default: assert(false && "Unreachable");
    }
}

void game_unload(Game game)
{
    if (game.obstacles.items) free(game.obstacles.items);
    if (game.explosions.items) free(game.explosions.items);

    UnloadSound(game.ship.jump_sound);
    UnloadSound(game.wha_wha_sound);
    UnloadSound(game.explosions.shared_sound);

    UnloadTexture(game.ship.texture);
    UnloadTexture(game.background.texture);
    UnloadTexture(game.obstacles.shared_texture);
    UnloadTexture(game.explosions.shared_texture);
}

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "flappyship");
    InitAudioDevice();
    SetTargetFPS(60);

    Game game = game_init();
    while (!WindowShouldClose()) {
        game_update(&game, GetFrameTime());

        BeginDrawing();

        game_draw(game);

        ClearBackground(BLACK);

        EndDrawing();
    }
    game_unload(game);

    CloseAudioDevice();
    CloseWindow();
    return 0;
}
