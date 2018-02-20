#include <Gamebuino-Meta.h>

#define ENEMY_W 5  // In pixels
#define ENEMY_H 3  // In pixels
#define ENEMY_PADDING_W 4  // Distance between 2 enemies
#define ENEMY_PADDING_H 3  // Distance between 2 enemies
#define ENEMY_SIDE_SHIFT_TIME 12  // How many frames between 2 sideshifts
#define ENEMY_DOWN_SHIFT_TIME 48  // How many frames between 2 downshifts

#define ENEMY_INIT_OFFSET_Y 6

#define PLAYER_W 6
#define PLAYER_H 4
#define PLAYER_Y gb.display.height() - 7

#define MISSILE_W 2
#define MISSILE_H 3

#define MODE_PLAY 0
#define MODE_GAME_OVER 1
#define MODE_WIN 2

#define COLOR_SOURCES_MIN_DIST 32  // This is the minimum distance SQUARED between 2 color sources on the enemy grid. Close sources make ugly gradients

#define EXPL_ANIM_TIME 13  // Length of the "explosion" animation
#define EXPL_MAX_W 3 * ENEMY_W
#define EXPL_MAX_H 3 * ENEMY_H


// ===== EXPLOSION CLASS ===== //

class Explosion {
    int frame_counter;  // Counts frames since begining of explotion
    uint8_t red;
    uint8_t green;
    uint8_t blue;

  public:
    int pos_x, pos_y;  // Explosion center
    bool active;  // If true, then the exposion is displayed

    Explosion() {
      frame_counter = EXPL_ANIM_TIME;
    }

    void reset(int _x, int _y, Color color) {
      pos_x = _x;
      pos_y = _y;
      // Convert RBG565 to RBG888 to be able to create a cutsum color in the draw function
      red = map((uint16_t(color) >> 11) & 0x1F, 0, 0x1F, 0, 255);
      green = map((uint16_t(color) >> 5) & 0x3F, 0, 0x3F, 0, 255);
      blue = map(uint16_t(color) & 0x001F, 0, 0x1F, 0, 255);
      frame_counter = EXPL_ANIM_TIME;
      active = true;
    }

    void update_explosion() {
      if (!active) return;

      frame_counter--;
      if (frame_counter == 0)
        active = false;
    }

    void draw() {
      if (!active) return;
      Color color = gb.createColor(red * frame_counter / EXPL_ANIM_TIME, green * frame_counter / EXPL_ANIM_TIME, blue * frame_counter / EXPL_ANIM_TIME);
      gb.display.setColor(color);
      int w = map(frame_counter, EXPL_ANIM_TIME, 0, ENEMY_W, EXPL_MAX_W);
      int h = map(frame_counter, EXPL_ANIM_TIME, 0, ENEMY_H, EXPL_MAX_H);
      gb.display.fillRect(pos_x - w / 2, pos_y - h / 2, w, h);

      gb.lights.setColor(color);
      gb.lights.fill();
    }
};


// Enemies are on a 8 x 6 grid represented by this byte array. A 1 represents an enemy
// Start with a full grid
uint8_t enemies[] = {
  0Xff,
  0Xff,
  0Xff,
  0Xff,
  0Xff,
  0Xff,
};

// Create colors
Color colors[8][6];

int enemy_grid_offset_x;
int enemy_grid_offset_y;
int enemy_side_shift_timer;
int enemy_down_shift_timer;
int side_shift_dir;  // +1 (to the right) or -1 (to the left)

int side_shift_min;  // Minimum distance the enemies can move side-to-side
int side_shift_max;  // Maximum distance the enemies can move side-to-side

int player_pos;  // X position of the player
bool is_missile_active;  // If true, then the missile was shot
int missile_pos_x;
int missile_pos_y;

int score;  // Score starts at 0 and counts the frames until victory
bool is_new_record;  // If the score done is a new record, then this is true. Otherwise false
int enemies_killed;  // The bigger, the faster the enemies move. Also used to check for victory

Explosion explosion1;
Explosion explosion2;  // To use when explosion1 is active (in case the player shoots really fast hehe)

char mode;


// ===== SETUP AND LOOP ===== //

void setup() {
  // put your setup code here, to run once:
  gb.begin();
  gb.pickRandomSeed();
  mode = MODE_PLAY;

  reset_game();
}

void loop() {
  // put your main code here, to run repeatedly:
  if (gb.update()) {
    switch (mode) {
      case MODE_PLAY:
        play_loop();
        break;
      case MODE_GAME_OVER:
        game_over_loop();
        break;
      case MODE_WIN:
        game_win_loop();
        break;
    }
  }
}


// ===== MAIN GAME LOOP ===== //

void play_loop() {
  // INPUTS
  if (gb.buttons.pressed(BUTTON_MENU)) {
    reset_game();
  }

  if (gb.buttons.repeat(BUTTON_LEFT, 2) && player_pos > 0) {
    player_pos--;
  }
  else if (gb.buttons.repeat(BUTTON_RIGHT, 2) && player_pos < gb.display.width() - PLAYER_W) {
    player_pos++;
  }

  if (gb.buttons.pressed(BUTTON_A) && !is_missile_active) {
    is_missile_active = true;
    missile_pos_x = player_pos + PLAYER_W / 2 - MISSILE_W / 2;
    missile_pos_y = PLAYER_Y;
    gb.sound.playTick();
  }

  // LOGIC
  score++;
  enemy_side_shift_timer++;
  enemy_down_shift_timer++;

  // Update explosions
  explosion1.update_explosion();
  explosion2.update_explosion();

  // Enemy movement
  if (enemy_side_shift_timer >= ENEMY_SIDE_SHIFT_TIME - enemies_killed / 6) {
    enemy_side_shift_timer = 0;
    enemy_grid_offset_x += side_shift_dir;
    explosion1.pos_x += side_shift_dir;
    explosion2.pos_x += side_shift_dir;
    // If the side of the window is hit
    if ((side_shift_dir == 1 && enemy_grid_offset_x >= side_shift_max)
        || (side_shift_dir == -1 && enemy_grid_offset_x <= side_shift_min))
      side_shift_dir *= -1;
  }
  if (enemy_down_shift_timer >= ENEMY_DOWN_SHIFT_TIME - enemies_killed / 3) {
    enemy_down_shift_timer = 0;
    enemy_grid_offset_y++;
    explosion1.pos_y++;
    explosion2.pos_y++;
  }

  if (is_missile_active) {
    missile_pos_y--;
    if (missile_pos_y < -MISSILE_H) {  // If missile exits the screen
      is_missile_active = false;
    }
  }
  // Collision check //
  for (char y = 0; y < 6; y++) {
    for (char x = 0; x < 8; x++) {
      if (enemies[y] & (0b10000000 >> x)) {
        // Missile - Enemy collition
        if (is_missile_active && gb.collideRectRect(missile_pos_x, missile_pos_y, MISSILE_W, MISSILE_H,
            x * ENEMY_W + (x + 1) * ENEMY_PADDING_W + enemy_grid_offset_x,
            y * ENEMY_H + (y + 1) * ENEMY_PADDING_H + enemy_grid_offset_y,
            ENEMY_W, ENEMY_H)) {
          enemies[y] &= ~(0b10000000 >> x);  // Set the bit at (x ,y) to 0
          enemies_killed++;
          if (enemies_killed == 48) {
            mode = MODE_WIN;
          }
          is_missile_active = false;  // Remove missile
          if (explosion1.active == false)
            explosion1.reset(x * ENEMY_W + (x + 1) * ENEMY_PADDING_W + enemy_grid_offset_x + ENEMY_W / 2,
                             y * ENEMY_H + (y + 1) * ENEMY_PADDING_H + enemy_grid_offset_y + ENEMY_H / 2,
                             colors[x][y]);
          else
            explosion2.reset(x * ENEMY_W + (x + 1) * ENEMY_PADDING_W + enemy_grid_offset_x + ENEMY_W / 2,
                             y * ENEMY_H + (y + 1) * ENEMY_PADDING_H + enemy_grid_offset_y + ENEMY_H / 2,
                             colors[x][y]);


          gb.sound.playOK();
        }

        // Player - Enemy collition and if an enemy went below the screen
        if (gb.collideRectRect(player_pos, PLAYER_Y, PLAYER_W, PLAYER_H,
                               x * ENEMY_W + (x + 1) * ENEMY_PADDING_W + enemy_grid_offset_x,
                               y * ENEMY_H + (y + 1) * ENEMY_PADDING_H + enemy_grid_offset_y,
                               ENEMY_W, ENEMY_H)
            || y * ENEMY_H + (y + 1) * ENEMY_PADDING_H + enemy_grid_offset_y > gb.display.height()) {
          score = -1;  // No socre because you failed to kill all enemies
          mode = MODE_GAME_OVER;
        }

      }
    }
  }


  // DRAW
  gb.display.clear();
  gb.lights.clear();

  // Explosions
  explosion1.draw();
  explosion2.draw();

  // Enemies
  for (char y = 0; y < 6; y++) {
    for (char x = 0; x < 8; x++) {
      // Check is there is an enemy at (x,y)
      if (enemies[y] & (0b10000000 >> x)) {
        gb.display.setColor(colors[x][y]);
        gb.display.fillRect(x * ENEMY_W + (x + 1) * ENEMY_PADDING_W + enemy_grid_offset_x,
                            y * ENEMY_H + (y + 1) * ENEMY_PADDING_H + enemy_grid_offset_y,
                            ENEMY_W,
                            ENEMY_H);
      }
    }
  }

  // Missile
  if (is_missile_active) {
    gb.display.setColor(WHITE);
    gb.display.fillRect(missile_pos_x, missile_pos_y, MISSILE_W, MISSILE_H);
  }

  // PLAYER
  gb.display.setColor(WHITE);
  gb.display.fillRect(player_pos, PLAYER_Y, PLAYER_W, PLAYER_H);

  // Don't mind this small hack :P
  if (mode == MODE_WIN) {
    gb.display.clear();
    gb.lights.clear();
    int hs = gb.save.get(0);
    if (hs == 0 || hs > score) {  // hs=0 means that there is no high-score
      is_new_record = true;
      gb.save.set(0, score);  // Save hs
    }
  }
}


// ===== GAME OVER LOOP ===== //

#define TEXT_ANIM_TIME 3
int text_offset = 0;
int text_anim_dir = 1;
int text_anim_timer = 0;

void game_over_loop() {
  // INPUTS
  if (gb.buttons.pressed(BUTTON_A)) {
    mode = MODE_PLAY;
    reset_game();
  }
  // LOGIC
  text_anim_timer++;
  if (text_anim_timer == TEXT_ANIM_TIME) {
    text_anim_timer = 0;
    text_offset += text_anim_dir;
    if (abs(text_offset) == 2) text_anim_dir *= -1;
  }

  // DRAW
  gb.lights.clear();
  gb.display.clear();
  gb.display.setColor(WHITE);
  gb.display.setCursorX(5);
  gb.display.setCursorY(10);
  if (is_new_record)
    gb.display.print("New best time!");
  else if (score >= 0)
    gb.display.print("Victory!");
  else
    gb.display.print("Game Over!");

  gb.display.setCursorX(5);
  gb.display.setCursorY(20);
  if (score >= 0)
    gb.display.printf("Your time: %d:%d", score / 25 / 60, score / 25 % 60);
  else
    gb.display.print("Mission failed :(");

  gb.display.setCursorX(5);
  gb.display.setCursorY(30);
  gb.display.printf("Best time: %d:%d", gb.save.get(0) / 25 / 60, gb.save.get(0) / 25 % 60);

  gb.display.setColor(GRAY);
  gb.display.setCursorX(22);
  gb.display.setCursorY(52 + text_offset);
  gb.display.print("Press A...");
}


// ===== WIN LOOP ===== //

int win_anim_rect_x;  // Inittial value is -50 so that it is reset imidialty
int win_anim_rect_y;
int win_anim_rect_speed_x;
int win_anim_rect_speed_y;
int win_anim_rect_grav_time;  // The lower, the less gravity there is
int grav_timer = 0;
Color win_anim_rect_color;

void game_win_loop() {
  // INPUTS
  if (gb.buttons.pressed(BUTTON_A)) {
    mode = MODE_GAME_OVER;
  }

  // LOGIC
  if (win_anim_rect_x < -ENEMY_W || win_anim_rect_x > gb.display.width() || win_anim_rect_y > gb.display.height()) {
    win_anim_rect_x = random(0, gb.display.width() - ENEMY_W);
    win_anim_rect_y = PLAYER_Y;
    win_anim_rect_speed_x = random(-4, 5);
    if (win_anim_rect_speed_x == 0) win_anim_rect_speed_x = 1;  // Not 0 please
    win_anim_rect_speed_y = random(2, 6);
    win_anim_rect_grav_time = random(2, 5);
    grav_timer = 0;
    win_anim_rect_color = colors[random(0, 8)][random(0, 6)];
  }

  grav_timer++;
  if (grav_timer == win_anim_rect_grav_time) {
    grav_timer = 0;
    win_anim_rect_speed_y--;
  }

  win_anim_rect_x += win_anim_rect_speed_x;
  win_anim_rect_y -= win_anim_rect_speed_y;

  // DRAW
  gb.display.setColor(win_anim_rect_color);
  gb.display.fillRect(win_anim_rect_x, win_anim_rect_y, ENEMY_W, ENEMY_H);

  gb.display.setColor(WHITE);
  gb.display.setCursorX(21);
  gb.display.setCursorY(58);
  gb.display.print("VICTORY :D");
}


void reset_game() {
  score = 0;
  is_new_record = false;
  player_pos = (gb.display.width() - PLAYER_W) / 2;

  enemy_side_shift_timer = 0;
  enemy_down_shift_timer = 0;
  side_shift_dir = 1;
  side_shift_min = -2;
  side_shift_max = 8;

  enemy_grid_offset_x = side_shift_min;
  enemy_grid_offset_y = ENEMY_INIT_OFFSET_Y;

  //  are_lights_on = false;
  explosion1.active = false;
  explosion2.active = false;

  // Reset enemies too
  for (char y = 0; y < 6; y++) {
    enemies[y] = 0xff;
  }
  enemies_killed = 0;

  // Generate random color gradiant //
  // Select random sources of light with appropriate distances
  char red_source_x, red_source_y;
  char green_source_x, green_source_y;
  char blue_source_x, blue_source_y;
  do {
    red_source_x = random(0, 8);
    red_source_y = random(0, 6);

    green_source_x = random(0, 8);
    green_source_y = random(0, 6);

    blue_source_x = random(0, 8);
    blue_source_y = random(0, 6);
  } while ((red_source_x - green_source_x) * (red_source_x - green_source_x) + (red_source_y - green_source_y) * (red_source_y - green_source_y) <= COLOR_SOURCES_MIN_DIST ||
           (green_source_x - blue_source_x) * (green_source_x - blue_source_x) + (green_source_y - blue_source_y) * (green_source_y - blue_source_y) <= COLOR_SOURCES_MIN_DIST ||
           (blue_source_x - red_source_x) * (blue_source_x - red_source_x) + (blue_source_y - red_source_y) * (blue_source_y - red_source_y) <= COLOR_SOURCES_MIN_DIST);

  for (char y = 0; y < 6; y++) {
    for (char x = 0; x < 8; x++) {
      // All distances are the square of their actual values (to avoir floats)
      char dist_r = (red_source_x - x) * (red_source_x - x) + (red_source_y - y) * (red_source_y - y);
      char dist_g = (green_source_x - x) * (green_source_x - x) + (green_source_y - y) * (green_source_y - y);
      char dist_b = (blue_source_x - x) * (blue_source_x - x) + (blue_source_y - y) * (blue_source_y - y);

      // Set a color intensity proportionaly to distance (max distance squared = 8*8 + 6*6 = 100)
      // Max intensity 255
      // Min intensity 10
      uint8_t r = map(dist_r, 0, 100, 255, 10);
      uint8_t g = map(dist_g, 0, 100, 255, 10);
      uint8_t b = map(dist_b, 0, 100, 255, 10);
      Color c = gb.createColor(r, g, b);
      colors[x][y] = c;
    }
  }

  win_anim_rect_x = -50;  // A very negative value is set by default so that upon winning, a proper reset of all 'win_anim_rect_*' is done
}


