#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Color Definitions (RGB 565) */
#define BLACK  0x0000
#define WHITE  0xFFFF
#define RED    0xF800
#define YELLOW 0xFFE0
#define GRAY   0xC618
#define BLUE   0x001F
#define DGRAY  0x39E7
#define ORANGE 0xFD20 
#define LRED   0xF810 

/* Memory Mapped I/O Addresses */
volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
volatile int * KEY_ptr        = (int *)0xFF200050;
volatile int * KEY_edge_ptr   = (int *)0xFF20005C; 
volatile int * HEX3_0_ptr     = (int *)0xFF200020;
volatile char * char_buffer = (char *)0x09000000;
/* Game Constants */
#define SCREEN_W 320
#define SCREEN_H 240
#define SHIP_W 15
#define SHIP_H 17 
#define ASTEROID_SIZE 12 
#define MAX_ASTEROIDS 5
#define SHIP_SPEED 15 

#define MAX_BULLETS 15 
#define BULLET_SPEED 6
#define MAX_BLASTS 5
#define NUM_STARS 20 

/* Global Variables */
int pixel_buffer_start;
int ship_x;
int ship_y = 205; 
int hp;
int score;
int high_score = 0; 
bool paused = false;
int current_fall_delay = 2; 
char text_buf[40]; 

/* Entity Arrays */
int ast_x[MAX_ASTEROIDS], ast_y[MAX_ASTEROIDS], ast_type[MAX_ASTEROIDS];
int bullet_x[MAX_BULLETS], bullet_y[MAX_BULLETS];
bool bullet_active[MAX_BULLETS];
int blast_x[MAX_BLASTS], blast_y[MAX_BLASTS], blast_timer[MAX_BLASTS];
int star_x[NUM_STARS], star_y[NUM_STARS];

/* HEX patterns */
char seg7[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x67}; 

/* Function Prototypes */
void plot_pixel(int x, int y, short int color);
void clear_screen();
void draw_box(int x, int y, int w, int h, short int color);
void draw_circle(int xc, int yc, int r, short int color);
void draw_custom_ship(int x, int y);
void erase_custom_ship(int x, int y);
void draw_asteroid(int x, int y);
void draw_mine(int x, int y);
void wait_for_vsync();
void update_hp_display();
void write_char(int x, int y, char c);
void write_string(int x, int y, char *str);
void clear_char_buffer();

int main(void) {
    int i, edge, frame_counter, spawned, a, b, idx, r;
    bool move_ast;
    short color;

    pixel_buffer_start = *pixel_ctrl_ptr;
    *KEY_edge_ptr = 0xF; 

    for(i = 0; i < NUM_STARS; i++) {
        star_x[i] = rand() % SCREEN_W;
        star_y[i] = rand() % SCREEN_H;
    }

    while (1) {
        clear_screen();
        clear_char_buffer();
        draw_custom_ship(150, 100);
        write_string(30, 30, "PRESS KEY0 TO START");
        wait_for_vsync();
        
        while(1) {
            edge = *KEY_edge_ptr;
            if(edge & 0x1) { *KEY_edge_ptr = edge; break; } 
        }

        clear_screen();
        clear_char_buffer();
        hp = 5;
        score = 0;
        ship_x = 150;
        paused = false;
        current_fall_delay = 2; 
        frame_counter = 0; 

        for(i = 0; i < MAX_ASTEROIDS; i++) {
            ast_x[i] = rand() % (SCREEN_W - 12);
            ast_y[i] = -(rand() % 100); 
            ast_type[i] = (rand() % 5 == 0) ? 1 : 0;
        }
        for(i = 0; i < MAX_BULLETS; i++) bullet_active[i] = false;
        for(i = 0; i < MAX_BLASTS; i++) blast_timer[i] = 0;

        while (hp > 0) {
            update_hp_display();
            if (score > high_score) high_score = score;
            if (score >= 15) current_fall_delay = 1; 
            
            erase_custom_ship(ship_x, ship_y); 
            for(i = 0; i < MAX_ASTEROIDS; i++) draw_box(ast_x[i], ast_y[i], ASTEROID_SIZE, ASTEROID_SIZE, BLACK);
            for(i = 0; i < MAX_BULLETS; i++) if (bullet_active[i]) draw_box(bullet_x[i], bullet_y[i], 3, 6, BLACK);
            for(i = 0; i < MAX_BLASTS; i++) if (blast_timer[i] >= 0) draw_box(blast_x[i]-12, blast_y[i]-12, 32, 32, BLACK);
            for(i = 0; i < NUM_STARS; i++) plot_pixel(star_x[i], star_y[i], BLACK);

            edge = *KEY_edge_ptr;
            if (edge != 0) {
                if (edge & 0x1) { paused = !paused; } 
                
                if (!paused) {
                    if (edge & 0x4) if (ship_x < SCREEN_W - SHIP_W - SHIP_SPEED) ship_x += SHIP_SPEED; 
                    if (edge & 0x8) if (ship_x > SHIP_SPEED) ship_x -= SHIP_SPEED; 
                    
                    if (edge & 0x2) { 
                        spawned = 0;
                        for(i = 0; i < MAX_BULLETS; i++) {
                            if (!bullet_active[i]) {
                                bullet_active[i] = true;
                                if (spawned == 0) {
                                    bullet_x[i] = ship_x; bullet_y[i] = ship_y + 4; spawned++;
                                } else if (spawned == 1) {
                                    bullet_x[i] = ship_x + 13; bullet_y[i] = ship_y + 4; spawned++;
                                } else if (spawned == 2) {
                                    bullet_x[i] = ship_x + 7; bullet_y[i] = ship_y - 6; break; 
                                }
                            }
                        }
                    }
                }
                *KEY_edge_ptr = edge; 
            }

            if (!paused) {
                frame_counter++;
                move_ast = (frame_counter >= current_fall_delay);
                if (move_ast) frame_counter = 0;

                for(i = 0; i < NUM_STARS; i++) {
                    star_y[i] += 1;
                    if (star_y[i] >= SCREEN_H) { star_y[i] = 0; star_x[i] = rand() % SCREEN_W; }
                }

                for(i = 0; i < MAX_BULLETS; i++) {
                    if (bullet_active[i]) {
                        bullet_y[i] -= BULLET_SPEED;
                        if (bullet_y[i] < 0) bullet_active[i] = false; 
                    }
                }

                for(a = 0; a < MAX_ASTEROIDS; a++) {
                    if (move_ast) ast_y[a] += 1; 

                    for(b = 0; b < MAX_BULLETS; b++) {
                        if (bullet_active[b] && bullet_x[b] >= ast_x[a]-2 && bullet_x[b] <= ast_x[a] + ASTEROID_SIZE && 
                            bullet_y[b] >= ast_y[a] && bullet_y[b] <= ast_y[a] + ASTEROID_SIZE) {
                            
                            bullet_active[b] = false; 
                            score += (ast_type[a] == 1) ? 3 : 1;
                            
                            for(idx = 0; idx < MAX_BLASTS; idx++) {
                                if (blast_timer[idx] <= 0) {
                                    blast_x[idx] = ast_x[a]; blast_y[idx] = ast_y[a];
                                    blast_timer[idx] = 12; break;
                                }
                            }
                            ast_y[a] = -10; 
                            ast_x[a] = rand() % (SCREEN_W - 12);
                            ast_type[a] = (rand() % 5 == 0) ? 1 : 0; 
                        }
                    }

                    if (ast_y[a] + ASTEROID_SIZE >= ship_y && ast_y[a] <= ship_y + SHIP_H) {
                        if (ast_x[a] + ASTEROID_SIZE >= ship_x && ast_x[a] <= ship_x + SHIP_W) {
                            if (ast_type[a] == 1) hp = 0; 
                            else hp--;   
                            
                            ast_y[a] = -10; 
                            ast_x[a] = rand() % (SCREEN_W - 12);
                            ast_type[a] = (rand() % 5 == 0) ? 1 : 0;

                            draw_box(0, 0, SCREEN_W, SCREEN_H, RED); 
                            wait_for_vsync();
                            clear_screen(); 
                        }
                    }

                    if (ast_y[a] > SCREEN_H) { 
                        ast_y[a] = -10; 
                        ast_x[a] = rand() % (SCREEN_W - 12); 
                        ast_type[a] = (rand() % 5 == 0) ? 1 : 0;
                    }
                }
                for(i = 0; i < MAX_BLASTS; i++) if (blast_timer[i] > 0) blast_timer[i]--;
            }

            for(i = 0; i < NUM_STARS; i++) plot_pixel(star_x[i], star_y[i], GRAY); 
            draw_custom_ship(ship_x, ship_y); 
            
            for(i = 0; i < MAX_ASTEROIDS; i++) {
                if (ast_type[i] == 0) draw_asteroid(ast_x[i], ast_y[i]); 
                else draw_mine(ast_x[i], ast_y[i]);     
            }

            for(i = 0; i < MAX_BULLETS; i++) if (bullet_active[i]) draw_box(bullet_x[i], bullet_y[i], 3, 6, ORANGE); 
            
            for(i = 0; i < MAX_BLASTS; i++) {
                if (blast_timer[i] > 0) {
                    r = 14 - blast_timer[i]; 
                    color = (blast_timer[i] % 2 == 0) ? ORANGE : RED; 
                    draw_circle(blast_x[i] + ASTEROID_SIZE/2, blast_y[i] + ASTEROID_SIZE/2, r, color);
                }
            }

            if (paused) write_string(36, 28, "PAUSED");
            else write_string(36, 28, "      "); 

            sprintf(text_buf, "HP: %d/5", hp);
            write_string(2, 2, text_buf);
            sprintf(text_buf, "SCORE: %d", score);
            write_string(35, 2, text_buf);
            sprintf(text_buf, "HIGH SCORE: %d", high_score);
            write_string(64, 2, text_buf);

            wait_for_vsync(); 
        }

        if (hp <= 0) {
            for(i = 0; i < 30; i++) {
                draw_circle(ship_x + 7, ship_y + 8, i, (i % 2 == 0) ? RED : ORANGE);
                wait_for_vsync();
            }
            clear_screen();
            clear_char_buffer();
            update_hp_display(); 

            write_string(35, 26, "GAME OVER");
            sprintf(text_buf, "FINAL SCORE: %d", score);
            write_string(33, 28, text_buf);
            write_string(29, 32, "PRESS KEY0 TO RESTART");

            while(1) {
                edge = *KEY_edge_ptr;
                if (edge & 0x1) { *KEY_edge_ptr = edge; break; } 
            }
        }
    } 
    return 0;
}

void plot_pixel(int x, int y, short int color) {
    if (x >= 0 && x < 320 && y >= 0 && y < 240) {
        *(volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = color;
    }
}

void clear_screen() {
    int x, y;
    for (x = 0; x < 320; x++) {
        for (y = 0; y < 240; y++) plot_pixel(x, y, BLACK);
    }
}

void draw_box(int x, int y, int w, int h, short int color) {
    int i, j;
    for (i = 0; i < w; i++) {
        for (j = 0; j < h; j++) plot_pixel(x + i, y + j, color);
    }
}

void draw_circle(int xc, int yc, int r, short int color) {
    int x, y;
    for (x = -r; x <= r; x++) {
        for (y = -r; y <= r; y++) {
            if (x*x + y*y <= r*r) plot_pixel(xc + x, yc + y, color);
        }
    }
}

void draw_custom_ship(int x, int y) {
    draw_box(x + 6, y, 3, 4, RED);         
    draw_box(x + 5, y + 4, 5, 6, WHITE);   
    draw_box(x + 4, y + 10, 7, 5, WHITE);  
    draw_box(x + 2, y + 8, 3, 4, GRAY);    
    draw_box(x + 10, y + 8, 3, 4, GRAY);   
    draw_box(x, y + 6, 2, 8, RED);         
    draw_box(x + 13, y + 6, 2, 8, RED);    
    draw_box(x + 5, y + 15, 2, 2, BLUE);   
    draw_box(x + 8, y + 15, 2, 2, BLUE);   
}

void erase_custom_ship(int x, int y) {
    draw_box(x + 6, y, 3, 4, BLACK);
    draw_box(x + 5, y + 4, 5, 6, BLACK);
    draw_box(x + 4, y + 10, 7, 5, BLACK);
    draw_box(x + 2, y + 8, 3, 4, BLACK);
    draw_box(x + 10, y + 8, 3, 4, BLACK);
    draw_box(x, y + 6, 2, 8, BLACK);
    draw_box(x + 13, y + 6, 2, 8, BLACK);
    draw_box(x + 5, y + 15, 2, 2, BLACK);
    draw_box(x + 8, y + 15, 2, 2, BLACK);
}

void draw_asteroid(int x, int y) {
    draw_box(x + 2, y, 4, 8, DGRAY);
    draw_box(x, y + 2, 8, 4, GRAY);
    draw_box(x + 1, y + 1, 6, 6, GRAY);
    plot_pixel(x + 2, y + 2, WHITE); 
}

void draw_mine(int x, int y) {
    draw_box(x + 3, y, 2, 8, RED);
    draw_box(x, y + 3, 8, 2, RED);
    draw_box(x + 2, y + 2, 4, 4, LRED);
    draw_box(x + 3, y + 3, 2, 2, WHITE); 
}

void update_hp_display() { *HEX3_0_ptr = seg7[hp % 10]; }

void write_char(int x, int y, char c) {
    if (x >= 0 && x < 80 && y >= 0 && y < 60) {
        *(char_buffer + (y << 7) + x) = c; 
    }
}

void write_string(int x, int y, char *str) {
    while (*str) {
        write_char(x, y, *str);
        x++;
        str++;
    }
}

void clear_char_buffer() {
    int x, y;
    for (x = 0; x < 80; x++) {
        for (y = 0; y < 60; y++) write_char(x, y, ' '); 
    }
}

void wait_for_vsync() {
    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
    register int status;
    *pixel_ctrl_ptr = 1; 
    status = *(pixel_ctrl_ptr + 3);
    while ((status & 0x01) != 0) status = *(pixel_ctrl_ptr + 3);
}