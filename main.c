#include "raylib.h"
#include <sqlite3.h>
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//----------------------------------------------------------------------
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

const char *TITLE = "Clicker Game (raylib)";
const char *MENU_TITLE = "CLICKER GAME";
const char *SELECT_TITLE = "SELECT LEVEL";
const char *GAMEOVER_TITLE = "GAME OVER";
const char *ENTER_NAME_TITLE = "Enter Your Name";
const char *SCOREBOARD_TITLE = "SCOREBOARD";
const char *NO_RECORDS = "No records yet";

const char *BUTTON_PLAY = "Play";
const char *BUTTON_QUIT = "Quit";
const char *BUTTON_BACK = "Back";
const char *BUTTON_PLAY_AGAIN = "Play Again";
const char *BUTTON_MAIN_MENU = "Main Menu";
const char *BUTTON_SCOREBOARD = "Scoreboard";
const char *BUTTON_SAVE_SCORE = "Save Score";
const char *BUTTON_SAVE = "Save";
const char *BUTTON_CANCEL = "Cancel";

const char *SCORE_TEMPLATE = "Score: %d";
const char *TIME_TEMPLATE = "Time: %.1f";
const char *YOUR_SCORE_TEMPLATE = "Your Score: %d";
const char *NAME_PROMPT = "Name: ";

const char *LEVEL_EASY = "Easy";
const char *LEVEL_MEDIUM = "Medium";
const char *LEVEL_HARD = "Hard";

#if defined(PLATFORM_WEB)
const char *DB_FILENAME = "/data/scores.db";
#else
const char *DB_FILENAME = "scores.db";
#endif

typedef enum {
  STATE_MENU = 0,
  STATE_SELECT,
  STATE_PLAYING,
  STATE_GAMEOVER,
  STATE_ENTER_NAME,
  STATE_SCOREBOARD
} GameState;

#define MAX_NAME_LENGTH 20

typedef struct {
  const char *name;
  int radius;
  float move_interval;
  float duration;
} Level;

Level levels[] = {
    {"Easy", 45, 1.5f, 30.0f},
    {"Medium", 30, 1.0f, 30.0f},
    {"Hard", 20, 0.6f, 30.0f}};

#define NUM_LEVELS (sizeof(levels) / sizeof(levels[0]))

typedef struct {
  char name[MAX_NAME_LENGTH];
  int score;
  char level[20];
} ScoreEntry;

int scores_count = 0;

//----------------------------------------------------------------------
GameState state = STATE_MENU;
int current_level = -1;
Level level_params;
int score = 0;
double game_start_time = 0.0;
double last_move_time = 0.0;
float target_x = 0.0f, target_y = 0.0f;
char name_input[MAX_NAME_LENGTH] = "";

//----------------------------------------------------------------------
void sync_fs(void) {
#if defined(PLATFORM_WEB)
  EM_ASM(FS.syncfs(function(err) {
    if (err)
      console.error(err);
  }););
#endif
}

void init_db(void) {
  sqlite3 *db;
  if (sqlite3_open(DB_FILENAME, &db) == SQLITE_OK) {
    const char *sql =
        "CREATE TABLE IF NOT EXISTS scores ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "score INTEGER NOT NULL,"
        "level TEXT NOT NULL,"
        "date TIMESTAMP DEFAULT CURRENT_TIMESTAMP); "
        "CREATE INDEX IF NOT EXISTS idx_scores_level_score ON scores(level, score DESC);";

    char *err_msg = 0;
    sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (err_msg)
      sqlite3_free(err_msg);
    sqlite3_close(db);
    sync_fs();
  }
}

void add_score(const char *name, int score, const char *level) {
  sqlite3 *db;
  const char *path = "scores.db";
#if defined(PLATFORM_WEB)
  path = "/data/scores.db";
#endif
  if (sqlite3_open(path, &db) == SQLITE_OK) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT INTO scores (name, score, level) VALUES (?, ?, ?);", -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, score);
    sqlite3_bind_text(stmt, 3, level, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    sync_fs();
  }
}

int get_top_scores(ScoreEntry *dest, const char *level_filter, int limit) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int cnt = 0;

  if (sqlite3_open(DB_FILENAME, &db) != SQLITE_OK)
    return 0;

  const char *sql = (level_filter != NULL)
                        ? "SELECT name, score, level FROM scores WHERE level = ? ORDER BY score DESC LIMIT ?;"
                        : "SELECT name, score, level FROM scores ORDER BY score DESC LIMIT ?;";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
    int param_idx = 1;

    if (level_filter != NULL) {
      sqlite3_bind_text(stmt, param_idx++, level_filter, -1, SQLITE_STATIC);
    }
    sqlite3_bind_int(stmt, param_idx, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW && cnt < limit) {
      const char *name = (const char *)sqlite3_column_text(stmt, 0);
      const char *lvl = (const char *)sqlite3_column_text(stmt, 2);
      strncpy(dest[cnt].name, name ? name : "Unknown", MAX_NAME_LENGTH - 1);
      dest[cnt].score = sqlite3_column_int(stmt, 1);
      strncpy(dest[cnt].level, lvl ? lvl : "N/A", 19);
      cnt++;
    }
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return cnt;
}

//----------------------------------------------------------------------
// Игровая механика
void spawn_target(void) {
  int r = level_params.radius;
  target_x = GetRandomValue(r, SCREEN_WIDTH - r);
  target_y = GetRandomValue(r, SCREEN_HEIGHT - r);
  last_move_time = GetTime();
}

// Отрисовка кнопки и проверка клика
int draw_button(int x, int y, int w, int h, const char *text, int font_size) {
  Vector2 mouse = GetMousePosition();
  int hover =
      (x <= mouse.x && mouse.x <= x + w && y <= mouse.y && mouse.y <= y + h);
  Color col = hover ? LIGHTGRAY : GRAY;
  DrawRectangle(x, y, w, h, col);
  DrawRectangleLines(x, y, w, h, DARKGRAY);
  int tw = MeasureText(text, font_size);
  DrawText(text, x + (w - tw) / 2, y + (h - font_size) / 2, font_size, BLACK);
  return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void handle_name_input(void) {
  int c = GetCharPressed();
  while (c > 0) {
    if (c >= 32 && c <= 125 && strlen(name_input) < MAX_NAME_LENGTH - 1) {
      int len = strlen(name_input);
      name_input[len] = (char)c;
      name_input[len + 1] = '\0';
    }
    c = GetCharPressed();
  }
  if (IsKeyPressed(KEY_BACKSPACE) && strlen(name_input) > 0) {
    name_input[strlen(name_input) - 1] = '\0';
  }
}

//----------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE
void main_loop(void) {
#if defined(PLATFORM_WEB)
  static bool kb_opened = false;
#endif
  if (state == STATE_PLAYING) {
    double elapsed = GetTime() - game_start_time;
    float time_left = level_params.duration - (float)elapsed;
    if (time_left <= 0.0f) {
      state = STATE_GAMEOVER;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      Vector2 mouse = GetMousePosition();
      float dx = mouse.x - target_x;
      float dy = mouse.y - target_y;
      if (dx * dx + dy * dy <= level_params.radius * level_params.radius) {
        score++;
        spawn_target();
      }
    }

    if (GetTime() - last_move_time > level_params.move_interval) {
      spawn_target();
    }
  } else if (state == STATE_ENTER_NAME) {
    handle_name_input();

#if defined(PLATFORM_WEB)
    static bool kb_opened = false;
    if (!kb_opened) {
      emscripten_run_script("triggerKeyboard();");
      kb_opened = true;
    }
#endif

    if (IsKeyPressed(KEY_ENTER)) {
      if (strlen(name_input) > 0) {
        add_score(name_input, score, level_params.name);
        state = STATE_SCOREBOARD;
        name_input[0] = '\0';
#if defined(PLATFORM_WEB)
        kb_opened = false;
#endif
      }
    }
  }

  BeginDrawing();
  ClearBackground(RAYWHITE);

  switch (state) {
  case STATE_MENU:
    current_level = -1;
    DrawText(MENU_TITLE, 220, 120, 50, DARKBLUE);
    if (draw_button(300, 250, 200, 60, BUTTON_PLAY, 30))
      state = STATE_SELECT;
    if (draw_button(300, 340, 200, 60, BUTTON_SCOREBOARD, 30))
      state = STATE_SCOREBOARD;
    if (draw_button(300, 430, 200, 60, BUTTON_QUIT, 30)) {
#if defined(PLATFORM_WEB)
      emscripten_force_exit(0);
#else
      // На Linux это закроет окно и выйдет из while(!WindowShouldClose())
      CloseWindow();
      exit(0);
#endif
    }
    break;

  case STATE_SELECT:
    DrawText(SELECT_TITLE, 250, 100, 40, DARKBLUE);
    for (int i = 0; i < NUM_LEVELS; i++) {
      if (draw_button(300, 200 + i * 80, 200, 50, levels[i].name, 25)) {
        current_level = i;
        level_params = levels[i];
        score = 0;
        game_start_time = GetTime();
        spawn_target();
        state = STATE_PLAYING;
      }
    }
    if (draw_button(300, 460, 200, 50, BUTTON_BACK, 25))
      state = STATE_MENU;
    break;

  case STATE_PLAYING: {
    char time_text[64];
    sprintf(time_text, SCORE_TEMPLATE, score);
    DrawText(time_text, 20, 20, 30, DARKGREEN);
    float time_left =
        level_params.duration - (float)(GetTime() - game_start_time);
    if (time_left < 0)
      time_left = 0;
    sprintf(time_text, TIME_TEMPLATE, time_left);
    int tw = MeasureText(time_text, 30);
    DrawText(time_text, SCREEN_WIDTH - tw - 20, 20, 30, DARKGREEN);

    DrawCircle((int)target_x, (int)target_y, level_params.radius, RED);
    DrawCircleLines((int)target_x, (int)target_y, level_params.radius, MAROON);
    break;
  }

  case STATE_GAMEOVER: {
    char score_msg[64];
    DrawText(GAMEOVER_TITLE, 250, 150, 50, DARKBLUE);
    sprintf(score_msg, YOUR_SCORE_TEMPLATE, score);
    DrawText(score_msg, 280, 250, 30, BLACK);
    if (draw_button(250, 330, 300, 60, BUTTON_PLAY_AGAIN, 30))
      state = STATE_SELECT;
    if (draw_button(250, 420, 300, 60, BUTTON_MAIN_MENU, 30))
      state = STATE_MENU;
    if (score > 0) {
      if (draw_button(250, 510, 300, 60, BUTTON_SAVE_SCORE, 30)) {
        state = STATE_ENTER_NAME;
#if defined(PLATFORM_WEB)
        emscripten_run_script("triggerKeyboard();"); // Вызываем прямо здесь!
#endif
      }
    }
    break;
  }

  case STATE_ENTER_NAME:
    DrawText(ENTER_NAME_TITLE, 220, 150, 40, DARKBLUE);

    DrawText(NAME_PROMPT, 200, 250, 30, BLACK);

    const char *cursor = ((int)(GetTime() * 2) % 2 == 0) ? "_" : "";

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s%s", name_input, cursor);

    DrawText(buffer, 310, 250, 30, MAROON);

    if (draw_button(300, 350, 200, 50, BUTTON_SAVE, 25)) {
      if (strlen(name_input) > 0) {
        add_score(name_input, score, level_params.name);
        state = STATE_SCOREBOARD;
        name_input[0] = '\0';
      }
    }
    if (draw_button(300, 420, 200, 50, BUTTON_CANCEL, 25)) {
      state = STATE_GAMEOVER;
      name_input[0] = '\0';
#if defined(PLATFORM_WEB)
      kb_opened = false;
#endif
    }
    break;

  case STATE_SCOREBOARD: {
    DrawText(SCOREBOARD_TITLE, 250, 50, 40, DARKBLUE);
    ScoreEntry top[10];
    int cnt;
    if (current_level >= 0) {
      cnt = get_top_scores(top, levels[current_level].name, 10);
    } else {
      cnt = get_top_scores(top, NULL, 10);
    }
    int y = 120;
    if (cnt == 0) {
      DrawText(NO_RECORDS, 300, y, 25, GRAY);
    } else {
      for (int i = 0; i < cnt; i++) {
        char text[64];
        if (current_level >= 0) {
          snprintf(text, sizeof(text), "%d. %-15s %5d", i + 1, top[i].name, top[i].score);
        } else {
          snprintf(text, sizeof(text), "%d. %-10s %5d (%s)", i + 1, top[i].name, top[i].score, top[i].level);
        }
        DrawText(text, 200, y, 20, BLACK);
        y += 30;
        if (y > 550)
          break;
      }
    }
    if (draw_button(300, 500, 200, 50, BUTTON_BACK, 25)) {
      state = (current_level < 0) ? STATE_MENU : STATE_GAMEOVER;
    }
    break;
  }
  }

  EndDrawing();
}

//----------------------------------------------------------------------
int main() {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, TITLE);
  SetTargetFPS(60);
#if defined(PLATFORM_WEB)
  EM_ASM({
    if (!FS.analyzePath('/data').exists) {
      FS.mkdir('/data');
    }
    FS.mount(IDBFS, {}, '/data');
    FS.syncfs(true, function(err) {
      _on_fs_loaded(); // Вызываем загрузку только после того, как IDBFS готов
    });
  });
#else
  init_db();
  while (!WindowShouldClose()) {
    main_loop();
  }
#endif
  return 0;
}

#if defined(PLATFORM_WEB)
EMSCRIPTEN_KEEPALIVE
void on_fs_loaded(void) {
  init_db();
  emscripten_set_main_loop(main_loop, 0, 1);
}
#endif