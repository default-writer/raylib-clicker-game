import raylib
from raylib import (
    InitWindow, SetTargetFPS, GetMousePosition, IsMouseButtonPressed,
    DrawRectangle, DrawRectangleLines,
    MeasureText, DrawText, DrawCircle, DrawCircleLines,
    BeginDrawing, ClearBackground, EndDrawing, CloseWindow,
    WindowShouldClose, GetCharPressed, IsKeyPressed,  # для ввода имени
    LIGHTGRAY, GRAY, DARKGRAY, RAYWHITE, DARKBLUE, DARKGREEN, RED, MAROON, BLACK,
    MOUSE_BUTTON_LEFT, KEY_BACKSPACE, KEY_ENTER
)
import random
import time
import sqlite3
import os

# ----------------------------------------------------------------------
# Константы окна
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 600

# Состояния игры
STATE_MENU = 0
STATE_SELECT = 1
STATE_PLAYING = 2
STATE_GAMEOVER = 3
STATE_ENTER_NAME = 4
STATE_SCOREBOARD = 5

# Текстовые строки (bytes)
TITLE = b"Clicker Game (raylib)"
MENU_TITLE = b"CLICKER GAME"
SELECT_TITLE = b"SELECT LEVEL"
GAMEOVER_TITLE = b"GAME OVER"
ENTER_NAME_TITLE = b"Enter Your Name"
SCOREBOARD_TITLE = b"SCOREBOARD"
NO_RECORDS = b"No records yet"

BUTTON_PLAY = b"Play"
BUTTON_QUIT = b"Quit"
BUTTON_BACK = b"Back"
BUTTON_PLAY_AGAIN = b"Play Again"
BUTTON_MAIN_MENU = b"Main Menu"
BUTTON_SCOREBOARD = b"Scoreboard"
BUTTON_SAVE_SCORE = b"Save Score"
BUTTON_SAVE = b"Save"
BUTTON_CANCEL = b"Cancel"

SCORE_TEMPLATE = b"Score: %d"
TIME_TEMPLATE = b"Time: %.1f"
YOUR_SCORE_TEMPLATE = b"Your Score: %d"
NAME_PROMPT = b"Name: "

# Названия уровней (bytes)
LEVEL_EASY = b"Easy"
LEVEL_MEDIUM = b"Medium"
LEVEL_HARD = b"Hard"

# Параметры уровней
LEVELS = [
    {"name": LEVEL_EASY,   "radius": 45, "move_interval": 1.5, "duration": 30},
    {"name": LEVEL_MEDIUM, "radius": 30, "move_interval": 1.0, "duration": 30},
    {"name": LEVEL_HARD,   "radius": 20, "move_interval": 0.6, "duration": 30},
]

# База данных
DB_FILENAME = "scores.db"

# Глобальные переменные игры
current_level = -1
level_params = {}
score = 0
game_start_time = 0.0
last_move_time = 0.0
target_x = 0.0
target_y = 0.0

# Ввод имени
name_input = ""
MAX_NAME_LENGTH = 20

# ----------------------------------------------------------------------
# Работа с БД
def init_db():
    """Создаёт таблицу для рекордов, если её нет."""
    with sqlite3.connect(DB_FILENAME) as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS scores (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                score INTEGER NOT NULL,
                level TEXT NOT NULL,
                date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        """)
        conn.execute("CREATE INDEX IF NOT EXISTS idx_scores_level_score ON scores(level, score DESC)")

def add_score(name, score, level_name):
    """Добавляет запись в таблицу."""
    with sqlite3.connect(DB_FILENAME) as conn:
        conn.execute("INSERT INTO scores (name, score, level) VALUES (?, ?, ?)",
                     (name, score, level_name.decode()))  # level_name храним как текст

def get_top_scores(level_name=None, limit=10):
    """Возвращает список (name, score, date) для заданного уровня (или все, если level_name=None)."""
    with sqlite3.connect(DB_FILENAME) as conn:
        if level_name:
            cursor = conn.execute(
                "SELECT name, score, date FROM scores WHERE level = ? ORDER BY score DESC LIMIT ?",
                (level_name.decode(), limit))
        else:
            cursor = conn.execute(
                "SELECT name, score, level, date FROM scores ORDER BY score DESC LIMIT ?",
                (limit,))
        return cursor.fetchall()

# ----------------------------------------------------------------------
def spawn_target():
    global last_move_time, target_x, target_y, level_params
    r = level_params["radius"]
    target_x = random.randint(r, SCREEN_WIDTH - r)
    target_y = random.randint(r, SCREEN_HEIGHT - r)
    last_move_time = time.time()

def draw_button(x, y, width, height, text, mouse_pos, font_size=20):
    hover = (x <= mouse_pos.x <= x + width and y <= mouse_pos.y <= y + height)
    color = LIGHTGRAY if hover else GRAY
    DrawRectangle(x, y, width, height, color)
    DrawRectangleLines(x, y, width, height, DARKGRAY)
    tw = MeasureText(text, font_size)
    DrawText(text, int(x + (width - tw) / 2), int(y + (height - font_size) / 2), font_size, BLACK)
    return hover and IsMouseButtonPressed(MOUSE_BUTTON_LEFT)

# ----------------------------------------------------------------------
def handle_name_input():
    """Обрабатывает клавиатурный ввод имени и возвращает True, если ввод завершён (Enter)."""
    global name_input
    while True:
        c = GetCharPressed()
        if c == 0:
            break
        if 32 <= c <= 125 and len(name_input) < MAX_NAME_LENGTH:
            name_input += chr(c)
    if IsKeyPressed(KEY_BACKSPACE) and len(name_input) > 0:
        name_input = name_input[:-1]
    return IsKeyPressed(KEY_ENTER)

# ----------------------------------------------------------------------
def main():
    global current_level, level_params, score, game_start_time, last_move_time, target_x, target_y, name_input

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, TITLE)
    SetTargetFPS(60)
    init_db()

    state = STATE_MENU

    # Локальные переменные для UI
    enter_name_done = False

    while not WindowShouldClose():
        mouse_pos = GetMousePosition()

        # Игровая логика
        if state == STATE_PLAYING:
            elapsed = time.time() - game_start_time
            time_left = max(0.0, level_params["duration"] - elapsed)
            if time_left <= 0.0:
                state = STATE_GAMEOVER

            if IsMouseButtonPressed(MOUSE_BUTTON_LEFT):
                dx = mouse_pos.x - target_x
                dy = mouse_pos.y - target_y
                if dx * dx + dy * dy <= level_params["radius"] ** 2:
                    score += 1
                    spawn_target()

            if time.time() - last_move_time > level_params["move_interval"]:
                spawn_target()

        # Обработка состояния ввода имени
        elif state == STATE_ENTER_NAME:
            if handle_name_input() or enter_name_done:
                if name_input.strip():  # сохраняем, если имя не пустое
                    add_score(name_input.strip(), score, level_params["name"])
                state = STATE_SCOREBOARD
                name_input = ""
                enter_name_done = False

        # Отрисовка
        BeginDrawing()
        ClearBackground(RAYWHITE)

        if state == STATE_MENU:
            current_level = 0
            DrawText(MENU_TITLE, 220, 120, 50, DARKBLUE)
            if draw_button(300, 250, 200, 60, BUTTON_PLAY, mouse_pos, 30):
                state = STATE_SELECT
            if draw_button(300, 340, 200, 60, BUTTON_SCOREBOARD, mouse_pos, 30):
                state = STATE_SCOREBOARD
            if draw_button(300, 430, 200, 60, BUTTON_QUIT, mouse_pos, 30):
                break

        elif state == STATE_SELECT:
            DrawText(SELECT_TITLE, 250, 100, 40, DARKBLUE)
            y_start = 200
            for i, lvl in enumerate(LEVELS):
                if draw_button(300, y_start + i * 80, 200, 50, lvl["name"], mouse_pos, 25):
                    current_level = i
                    level_params = lvl.copy()
                    score = 0
                    game_start_time = time.time()
                    spawn_target()
                    state = STATE_PLAYING
            if draw_button(300, 460, 200, 50, BUTTON_BACK, mouse_pos, 25):
                state = STATE_MENU

        elif state == STATE_PLAYING:
            score_text = SCORE_TEMPLATE % score
            DrawText(score_text, 20, 20, 30, DARKGREEN)
            time_left = max(0.0, level_params["duration"] - (time.time() - game_start_time))
            time_text = TIME_TEMPLATE % time_left
            tw = MeasureText(time_text, 30)
            DrawText(time_text, SCREEN_WIDTH - tw - 20, 20, 30, DARKGREEN)

            DrawCircle(int(target_x), int(target_y), level_params["radius"], RED)
            DrawCircleLines(int(target_x), int(target_y), level_params["radius"], MAROON)

        elif state == STATE_GAMEOVER:
            DrawText(GAMEOVER_TITLE, 250, 150, 50, DARKBLUE)
            score_msg = YOUR_SCORE_TEMPLATE % score
            DrawText(score_msg, 280, 250, 30, BLACK)
            if draw_button(250, 330, 300, 60, BUTTON_PLAY_AGAIN, mouse_pos, 30):
                state = STATE_SELECT
            if draw_button(250, 420, 300, 60, BUTTON_MAIN_MENU, mouse_pos, 30):
                state = STATE_MENU
            # Если игра закончилась и счёт > 0, показываем кнопку сохранения рекорда
            if score > 0:
                if draw_button(250, 510, 300, 60, BUTTON_SAVE_SCORE, mouse_pos, 30):
                    state = STATE_ENTER_NAME

        elif state == STATE_ENTER_NAME:
            DrawText(ENTER_NAME_TITLE, 220, 150, 40, DARKBLUE)
            DrawText(NAME_PROMPT, 200, 250, 30, BLACK)
            # Отображаем текущий ввод
            display_name = name_input + "_" if int(time.time() * 2) % 2 == 0 else name_input  # мигающий курсор
            DrawText(display_name.encode(), 300, 250, 30, DARKGREEN)
            if draw_button(300, 350, 200, 50, BUTTON_SAVE, mouse_pos, 25):
                if name_input.strip():
                    add_score(name_input.strip(), score, level_params["name"])
                    state = STATE_SCOREBOARD
                    name_input = ""
            if draw_button(300, 420, 200, 50, BUTTON_CANCEL, mouse_pos, 25):
                state = STATE_GAMEOVER
                name_input = ""

        elif state == STATE_SCOREBOARD:
            DrawText(SCOREBOARD_TITLE, 250, 50, 40, DARKBLUE)
            # Выводим топ-10 для текущего уровня, если уровень выбран, иначе общий топ
            if current_level >= 0:
                top_scores = get_top_scores(LEVELS[current_level]["name"], 10)
            else:
                top_scores = get_top_scores(None, 10)

            y = 120
            if not top_scores:
                DrawText(NO_RECORDS, 300, y, 25, GRAY)
            else:
                for i, row in enumerate(top_scores):
                    if current_level >= 0:
                        text = f"{i+1}. {row[0]:<15} {row[1]:>5}".encode()
                    else:
                        text = f"{i+1}. {row[0]:<10} {row[1]:>5} ({row[2]})".encode()
                    DrawText(text, 200, y, 20, BLACK)
                    y += 30
                    if y > 550:
                        break

            if draw_button(300, 500, 200, 50, BUTTON_BACK, mouse_pos, 25):
                state = STATE_MENU if current_level < 0 else STATE_GAMEOVER
                # Сброс уровня после просмотра, чтобы меню работало корректно
                if current_level < 0:
                    state = STATE_MENU
                else:
                    state = STATE_GAMEOVER

        EndDrawing()

    CloseWindow()

if __name__ == "__main__":
    main()