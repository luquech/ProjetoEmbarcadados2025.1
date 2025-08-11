#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_err.h>
#include <stdlib.h>
#include <math.h>

#include "font5x7.h"
#include "display.h"
#include "mpu6050.h"
#include "sdcard.h"

// Botões de navegação
#define SELECT_BUTTON GPIO_NUM_27
#define NAVIGATE_BUTTON GPIO_NUM_4

// Configurações gerais
#define GAME_SPEED 300 // ms

// BUZZER
#define BUZZER_PIN GPIO_NUM_25
#define BUZZER_LEDC_CHANNEL LEDC_CHANNEL_0
#define BUZZER_LEDC_TIMER LEDC_TIMER_0
#define BUZZER_LEDC_MODE LEDC_HIGH_SPEED_MODE
#define BUZZER_LEDC_DUTY_RES LEDC_TIMER_8_BIT

static const char *TAG = "game_system";

typedef enum {
    GAME_SNAKE = 0,
    GAME_PONG,
    GAME_DODGE,
    GAME_TILT_MAZE,
    GAME_COUNT  // Mantém a contagem de jogos
} GameSelection;

// Estrutura para posição
typedef struct {
    int x;
    int y;
} Position;

// Estrutura para o jogo Dodge the Blocks
typedef struct {
    Position player;
    Position blocks[10]; // Array de blocos
    int block_count;
    int block_speed;
    bool game_over;
    int score;
    int lives;
    int high_score;
} DodgeGame;

// Estrutura para o jogo da cobrinha
typedef struct {
    Position body[100];
    int length;
    int direction;
    Position food;
    bool game_over;
    int score;
    int high_score;
} SnakeGame;

// Estrutura para o jogo Pong
typedef struct {
    Position ball;
    Position ball_velocity;
    int paddle_pos;
    int paddle_width;
    bool game_over;
    int score;
    int high_score;
} PongGame;

typedef struct {
    Position player;
    Position foods[4]; // 4 comidas por nível
    int food_count;
    Position walls[50]; // Array de paredes
    int wall_count;
    int level;
    bool game_over;
    bool level_complete;
    int high_score;
} TiltMazeGame;

// Funções do buzzer
void buzzer_init() {
    ledc_timer_config_t timer_conf = {
        .speed_mode = BUZZER_LEDC_MODE,
        .duty_resolution = BUZZER_LEDC_DUTY_RES,
        .timer_num = BUZZER_LEDC_TIMER,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = BUZZER_LEDC_MODE,
        .channel = BUZZER_LEDC_CHANNEL,
        .timer_sel = BUZZER_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel_conf);
}

void buzzer_play_tone(int frequency, int duration_ms) {
    ledc_set_freq(BUZZER_LEDC_MODE, BUZZER_LEDC_TIMER, frequency);
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 128); // 50% duty cycle
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
    vTaskDelay(duration_ms / portTICK_PERIOD_MS);
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0); // Desliga
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
}

void play_game_over_sound() {
    buzzer_play_tone(300, 200);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    buzzer_play_tone(200, 300);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    buzzer_play_tone(150, 400);
}

void play_new_record_sound() {
    buzzer_play_tone(1000, 100);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    buzzer_play_tone(1200, 100);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    buzzer_play_tone(1500, 200);
}

// Implementações dos jogos
void snake_game_init(SnakeGame *game) {
    game->length = 3;
    game->direction = 1;
    game->game_over = false;
    game->score = 0;
    game->high_score = read_high_score("snake");

    for (int i = 0; i < game->length; i++) {
        game->body[i].x = (WIDTH/2) - i * 4;
        game->body[i].y = HEIGHT/2;
    }

    game->food.x = (rand() % (WIDTH / 4)) * 4;
    game->food.y = (rand() % (HEIGHT / 4)) * 4;
}

void snake_game_update(SnakeGame *game) {
    if (game->game_over) return;

    // Move o corpo
    for (int i = game->length - 1; i > 0; i--) {
        game->body[i] = game->body[i-1];
    }

    // Move a cabeça
    switch (game->direction) {
        case 0: game->body[0].y -= 4; break;
        case 1: game->body[0].x += 4; break;
        case 2: game->body[0].y += 4; break;
        case 3: game->body[0].x -= 4; break;
    }

    // Verifica comida
    if (game->body[0].x == game->food.x && game->body[0].y == game->food.y) {
        game->length++;
        game->score += 10;
        game->food.x = (rand() % (WIDTH / 4)) * 4;
        game->food.y = (rand() % (HEIGHT / 4)) * 4;
    }

    // Verifica colisões
    if (game->body[0].x < 0 || game->body[0].x >= WIDTH ||
        game->body[0].y < 0 || game->body[0].y >= HEIGHT) {
        game->game_over = true;
        return;
    }

    for (int i = 1; i < game->length; i++) {
        if (game->body[0].x == game->body[i].x && 
            game->body[0].y == game->body[i].y) {
            game->game_over = true;
            return;
        }
    }
}

void snake_game_render(SnakeGame *game) {
    clear_screen();
    
    // Desenha a cobra
    for (int i = 0; i < game->length; i++) {
        draw_rect(game->body[i].x, game->body[i].y, 4, 4, true);
    }
    
    // Desenha a comida
    draw_rect(game->food.x, game->food.y, 4, 4, false);
    
    // Desenha a pontuação e recorde
    char score_text[30];
    snprintf(score_text, sizeof(score_text), "Score: %d", game->score);
    draw_text(0, 0, score_text);
    
    snprintf(score_text, sizeof(score_text), "Recorde: %d", game->high_score);
    draw_text(WIDTH - 70, 0, score_text);
    
    update_display();
}

void pong_game_init(PongGame *game) {
    game->ball.x = WIDTH / 2;
    game->ball.y = HEIGHT / 2;
    game->ball_velocity.x = 2;
    game->ball_velocity.y = 2;
    game->paddle_pos = WIDTH / 2;
    game->paddle_width = 20;
    game->game_over = false;
    game->score = 0;
    game->high_score = read_high_score("pong");
}

void pong_game_update(PongGame *game) {
    if (game->game_over) return;

    // Move a bola
    game->ball.x += game->ball_velocity.x;
    game->ball.y += game->ball_velocity.y;

    // Colisão com as paredes laterais
    if (game->ball.x <= 0 || game->ball.x >= WIDTH - 1) {
        game->ball_velocity.x = -game->ball_velocity.x;
    }

    // Colisão com a parede superior
    if (game->ball.y <= 0) {
        game->ball_velocity.y = -game->ball_velocity.y;
    }

    // Colisão com a raquete
    if (game->ball.y >= HEIGHT - 4 && 
        game->ball.x >= game->paddle_pos - game->paddle_width/2 && 
        game->ball.x <= game->paddle_pos + game->paddle_width/2) {
        game->ball_velocity.y = -game->ball_velocity.y;
        game->score += 5;
    }

    // Verifica se a bola passou da raquete
    if (game->ball.y >= HEIGHT) {
        game->game_over = true;
    }
}

void pong_game_render(PongGame *game) {
    clear_screen();
    
    // Desenha a bola
    draw_rect(game->ball.x - 1, game->ball.y - 1, 3, 3, true);
    
    // Desenha a raquete
    draw_rect(game->paddle_pos - game->paddle_width/2, HEIGHT - 2, game->paddle_width, 2, true);
    
    // Desenha a pontuação e recorde
    char score_text[30];
    snprintf(score_text, sizeof(score_text), "Score: %d", game->score);
    draw_text(0, 0, score_text);
    
    snprintf(score_text, sizeof(score_text), "Recorde: %d", game->high_score);
    draw_text(WIDTH - 70, 0, score_text);
    
    update_display();
}

void dodge_game_init(DodgeGame *game) {
    game->player.x = WIDTH / 2;
    game->player.y = HEIGHT - 10;
    game->block_count = 3; // Começa com 3 blocos
    game->block_speed = 2;
    game->game_over = false;
    game->score = 0;
    game->lives = 3;
    game->high_score = read_high_score("dodge");
    
    // Posiciona os blocos aleatoriamente no topo
    for (int i = 0; i < game->block_count; i++) {
        game->blocks[i].x = rand() % (WIDTH - 10);
        game->blocks[i].y = -10 - (i * 30); // Espaçamento vertical
    }
}

void dodge_game_update(DodgeGame *game) {
    if (game->game_over) return;
    
    // Move os blocos para baixo
    for (int i = 0; i < game->block_count; i++) {
        game->blocks[i].y += game->block_speed;
        
        // Verifica colisão com o jogador
        if (game->blocks[i].y + 8 >= game->player.y && 
            game->blocks[i].y <= game->player.y + 8 &&
            game->blocks[i].x + 10 >= game->player.x && 
            game->blocks[i].x <= game->player.x + 10) {
            
            game->lives--;
            if (game->lives <= 0) {
                game->game_over = true;
                return;
            }
            // Reposiciona o bloco
            game->blocks[i].x = rand() % (WIDTH - 10);
            game->blocks[i].y = -10;
        }
        
        // Reposiciona blocos que saíram da tela
        if (game->blocks[i].y > HEIGHT) {
            game->blocks[i].x = rand() % (WIDTH - 10);
            game->blocks[i].y = -10;
            game->score++;
            
            // Aumenta a dificuldade a cada 10 pontos
            if (game->score % 10 == 0) {
                game->block_speed++;
                if (game->block_count < 10) {
                    game->block_count++;
                }
            }
        }
    }
}

void dodge_game_render(DodgeGame *game) {
    clear_screen();
    
    // Desenha o jogador (um quadrado)
    draw_rect(game->player.x, game->player.y, 10, 8, true);
    
    // Desenha os blocos
    for (int i = 0; i < game->block_count; i++) {
        draw_rect(game->blocks[i].x, game->blocks[i].y, 10, 8, false);
    }
    
    // Desenha a pontuação e vidas
    char score_text[40];
    snprintf(score_text, sizeof(score_text), "Score: %d", game->score);
    draw_text(0, 0, score_text);
    
    snprintf(score_text, sizeof(score_text), "Vidas: %d", game->lives);
    draw_text(WIDTH - 40, 0, score_text);
    
    snprintf(score_text, sizeof(score_text), "Recorde: %d", game->high_score);
    draw_text(0, 10, score_text);
    
    update_display();
}

void tilt_maze_init_level(TiltMazeGame *game, int level) {
    game->level = level;
    game->food_count = 4;
    game->level_complete = false;
    
    // Limpa paredes
    game->wall_count = 0;
    
    // Posição inicial do jogador (depende do nível)
    game->player.x = 10;
    game->player.y = 10;
    
    // Configuração dos níveis
    switch(level) {
        case 1:
            // Nível 1 - layout simples
            game->foods[0] = (Position){30, 10};
            game->foods[1] = (Position){90, 10};
            game->foods[2] = (Position){30, 50};
            game->foods[3] = (Position){90, 50};
            
            // Paredes
            game->walls[game->wall_count++] = (Position){60, 20};
            game->walls[game->wall_count++] = (Position){60, 30};
            game->walls[game->wall_count++] = (Position){60, 40};
            break;
            
        case 2:
            // Nível 2 - mais paredes
            game->foods[0] = (Position){20, 20};
            game->foods[1] = (Position){100, 20};
            game->foods[2] = (Position){20, 40};
            game->foods[3] = (Position){100, 40};
            
            // Paredes em forma de cruz
            for(int i=20; i<40; i++) {
                game->walls[game->wall_count++] = (Position){60, i};
            }
            for(int i=40; i<80; i++) {
                game->walls[game->wall_count++] = (Position){i, 30};
            }
            break;
            
        case 3:
            // Nível 3 - labirinto mais complexo
            game->foods[0] = (Position){10, 50};
            game->foods[1] = (Position){110, 10};
            game->foods[2] = (Position){110, 50};
            game->foods[3] = (Position){10, 10};
            
            // Paredes
            for(int i=10; i<60; i++) {
                if(i != 30) game->walls[game->wall_count++] = (Position){40, i};
            }
            for(int i=40; i<90; i++) {
                if(i != 60) game->walls[game->wall_count++] = (Position){i, 30};
            }
            for(int i=30; i<60; i++) {
                game->walls[game->wall_count++] = (Position){80, i};
            }
            break;
            
        case 4:
            // Nível 4 - corredores
            game->foods[0] = (Position){10, 10};
            game->foods[1] = (Position){110, 10};
            game->foods[2] = (Position){10, 50};
            game->foods[3] = (Position){110, 50};
            
            // Paredes em zigue-zague
            for(int i=0; i<5; i++) {
                int y = 15 + i*8;
                game->walls[game->wall_count++] = (Position){20, y};
                game->walls[game->wall_count++] = (Position){40, y+4};
                game->walls[game->wall_count++] = (Position){60, y};
                game->walls[game->wall_count++] = (Position){80, y+4};
                game->walls[game->wall_count++] = (Position){100, y};
            }
            break;
            
        case 5:
            // Nível 5 - desafio final
            game->foods[0] = (Position){5, 5};
            game->foods[1] = (Position){115, 5};
            game->foods[2] = (Position){5, 55};
            game->foods[3] = (Position){115, 55};
            
            // Paredes formando um labirinto
            for(int i=10; i<60; i++) {
                game->walls[game->wall_count++] = (Position){20, i};
                if(i < 30 || i > 40) game->walls[game->wall_count++] = (Position){60, i};
            }
            for(int i=20; i<110; i++) {
                if(i < 50 || i > 70) game->walls[game->wall_count++] = (Position){i, 30};
            }
            for(int i=30; i<60; i++) {
                game->walls[game->wall_count++] = (Position){90, i};
            }
            break;
    }
}

void tilt_maze_init(TiltMazeGame *game) {
    game->game_over = false;
    game->level = 0;
    game->high_score = read_high_score("tilt_maze");
    tilt_maze_init_level(game, 1); // Começa no nível 1
}

bool is_wall(TiltMazeGame *game, int x, int y) {
    for(int i=0; i<game->wall_count; i++) {
        if(game->walls[i].x == x && game->walls[i].y == y) {
            return true;
        }
    }
    return false;
}

void tilt_maze_update(TiltMazeGame *game, int dx, int dy) {
    if(game->game_over || game->level_complete) return;
    
    // Calcula nova posição
    int new_x = game->player.x + dx;
    int new_y = game->player.y + dy;
    
    // Verifica limites da tela
    if(new_x < 0 || new_x >= WIDTH || new_y < 0 || new_y >= HEIGHT) {
        return;
    }
    
    // Verifica colisão com paredes
    if(is_wall(game, new_x, new_y)) {
        return;
    }
    
    // Atualiza posição do jogador
    game->player.x = new_x;
    game->player.y = new_y;
    
    // Verifica se pegou comida
    for(int i=0; i<4; i++) {
        if(abs(game->player.x - game->foods[i].x) <= 4 && 
           abs(game->player.y - game->foods[i].y) <= 4) {
            
            game->foods[i].x = -10; // Remove a comida
            game->foods[i].y = -10;
            game->food_count--;
            
            // Toca som de coleta
            buzzer_play_tone(800, 50);
            
            if(game->food_count == 0) {
                game->level_complete = true;
                // Toca som de nível completo
                buzzer_play_tone(1000, 100);
                vTaskDelay(50 / portTICK_PERIOD_MS);
                buzzer_play_tone(1200, 150);
            }
            break;
        }
    }
}

void tilt_maze_render(TiltMazeGame *game) {
    clear_screen();
    
    // Desenha o nível atual e recorde
    char level_text[40];
    snprintf(level_text, sizeof(level_text), "Nivel: %d", game->level);
    draw_text(0, 0, level_text);
    
    snprintf(level_text, sizeof(level_text), "Recorde: %d", game->high_score);
    draw_text(WIDTH - 70, 0, level_text);
    
    // Desenha paredes
    for(int i=0; i<game->wall_count; i++) {
        draw_rect(game->walls[i].x, game->walls[i].y, 4, 4, true);
    }
    
    // Desenha comidas
    for(int i=0; i<4; i++) {
        if(game->foods[i].x >= 0) { // Só desenha se estiver na tela
            draw_rect(game->foods[i].x - 2, game->foods[i].y - 2, 8, 8, false);
        }
    }
    
    // Desenha jogador
    draw_rect(game->player.x, game->player.y, 4, 4, true);
    
    // Se nível completo, mostra mensagem
    if(game->level_complete) {
        draw_text(WIDTH/2 - 30, HEIGHT/2 - 10, "Nivel Completo!");
    }
    
    update_display();
}

// Mostra o menu de seleção de jogos
void show_menu(GameSelection selection) {
    clear_screen();
    
    // Título
    draw_text(20, 10, "Selecione o Jogo");
    
    // Opções
    draw_text(30, 25, "Snake");
    draw_text(30, 35, "Pong");
    draw_text(30, 45, "Dodge Blocks");
    draw_text(30, 55, "Tilt Maze");
    
    // Indicador de seleção (seta)
    draw_text(15, 25 + (selection * 10), ">");
    
    update_display();
}

// Mostra tela de Game Over e espera por qualquer botão
void show_game_over_screen(int score, int high_score, bool new_record) {
    char score_text[30];
    
    if (new_record) {
        play_new_record_sound();
    } else {
        play_game_over_sound();
    }
    
    while(1) {
        clear_screen();
        
        draw_text(WIDTH/2 - 30, 15, "Game Over");
        
        snprintf(score_text, sizeof(score_text), "Pontuacao: %d", score);
        draw_text(WIDTH/2 - 30, 30, score_text);
        
        snprintf(score_text, sizeof(score_text), "Recorde: %d", high_score);
        draw_text(WIDTH/2 - 30, 40, score_text);
        
        if (new_record) {
            draw_text(WIDTH/2 - 40, 50, "Novo Recorde!");
        } else {
            draw_text(WIDTH/2 - 50, 55, "Pressione qualquer");
            draw_text(WIDTH/2 - 40, 65, "botao para voltar");
        }
        
        update_display();
        
        if (gpio_get_level(SELECT_BUTTON) || gpio_get_level(NAVIGATE_BUTTON)) {
            while (gpio_get_level(SELECT_BUTTON) || gpio_get_level(NAVIGATE_BUTTON)) {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            break;
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// Tarefa principal do sistema de jogos
void game_task(void *pvParameters) {
    gpio_set_direction(SELECT_BUTTON, GPIO_MODE_INPUT);
    gpio_set_direction(NAVIGATE_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(SELECT_BUTTON, GPIO_PULLDOWN_ONLY);
    gpio_set_pull_mode(NAVIGATE_BUTTON, GPIO_PULLDOWN_ONLY);

    GameSelection current_selection = GAME_SNAKE;
    bool in_menu = true;

    while (1) {
        if (in_menu) {
            show_menu(current_selection);
            
            if (gpio_get_level(NAVIGATE_BUTTON)) {
                current_selection = (current_selection + 1) % GAME_COUNT;
                vTaskDelay(200 / portTICK_PERIOD_MS);
            }
            
            if (gpio_get_level(SELECT_BUTTON)) {
                in_menu = false;
                vTaskDelay(200 / portTICK_PERIOD_MS);
                
                if (current_selection == GAME_SNAKE) {
                    SnakeGame snake_game;
                    snake_game_init(&snake_game);
                    
                    int16_t ax, ay, az;
                    float filtered_ax = 0, filtered_ay = 0;
                    const float alpha = 0.2;
                    const float threshold = 0.3;
                    
                    while (!snake_game.game_over) {
                        mpu6050_read_accel(&ax, &ay, &az);
                        float gx = ax / 16384.0;
                        float gy = ay / 16384.0;
                        filtered_ax = low_pass_filter(gx, filtered_ax, alpha);
                        filtered_ay = low_pass_filter(gy, filtered_ay, alpha);
                        
                        if (fabsf(filtered_ax) > fabsf(filtered_ay)) {
                            if (filtered_ax > threshold && snake_game.direction != 3) {
                                snake_game.direction = 1;
                            } else if (filtered_ax < -threshold && snake_game.direction != 1) {
                                snake_game.direction = 3;
                            }
                        } else {
                            if (filtered_ay > threshold && snake_game.direction != 0) {
                                snake_game.direction = 2;
                            } else if (filtered_ay < -threshold && snake_game.direction != 2) {
                                snake_game.direction = 0;
                            }
                        }
                        
                        snake_game_update(&snake_game);
                        snake_game_render(&snake_game);
                        vTaskDelay(GAME_SPEED / portTICK_PERIOD_MS);
                    }
                    
                    bool new_record = false;
                    int current_high_score = read_high_score("snake");
                    if (snake_game.score > current_high_score) {
                        new_record = true;
                        write_high_score("snake", snake_game.score);
                        current_high_score = snake_game.score;
                    }
                    
                    show_game_over_screen(snake_game.score, current_high_score, new_record);
                    
                } else if (current_selection == GAME_PONG) {
                    PongGame pong_game;
                    pong_game_init(&pong_game);
                    
                    int16_t ax, ay, az;
                    float filtered_ax = 0;
                    const float alpha = 0.2;
                    
                    while (!pong_game.game_over) {
                        mpu6050_read_accel(&ax, &ay, &az);
                        float gx = ax / 16384.0;
                        filtered_ax = low_pass_filter(gx, filtered_ax, alpha);
                        
                        pong_game.paddle_pos = WIDTH/2 + (filtered_ax * 50);
                        if (pong_game.paddle_pos < pong_game.paddle_width/2) {
                            pong_game.paddle_pos = pong_game.paddle_width/2;
                        }
                        if (pong_game.paddle_pos > WIDTH - pong_game.paddle_width/2) {
                            pong_game.paddle_pos = WIDTH - pong_game.paddle_width/2;
                        }
                        
                        pong_game_update(&pong_game);
                        pong_game_render(&pong_game);
                        vTaskDelay(GAME_SPEED / portTICK_PERIOD_MS);
                    }
                    
                    bool new_record = false;
                    int current_high_score = read_high_score("pong");
                    if (pong_game.score > current_high_score) {
                        new_record = true;
                        write_high_score("pong", pong_game.score);
                        current_high_score = pong_game.score;
                    }
                    
                    show_game_over_screen(pong_game.score, current_high_score, new_record);
                    
                } else if (current_selection == GAME_DODGE) {
                    DodgeGame dodge_game;
                    dodge_game_init(&dodge_game);
                    
                    int16_t ax, ay, az;
                    float filtered_ax = 0;
                    const float alpha = 0.2;
                    
                    while (!dodge_game.game_over) {
                        mpu6050_read_accel(&ax, &ay, &az);
                        float gx = ax / 16384.0;
                        filtered_ax = low_pass_filter(gx, filtered_ax, alpha);
                        
                        dodge_game.player.x += (int)(filtered_ax * 5);
                        
                        if (dodge_game.player.x < 0) dodge_game.player.x = 0;
                        if (dodge_game.player.x > WIDTH - 10) dodge_game.player.x = WIDTH - 10;
                        
                        dodge_game_update(&dodge_game);
                        dodge_game_render(&dodge_game);
                        vTaskDelay(GAME_SPEED / portTICK_PERIOD_MS);
                    }
                    
                    bool new_record = false;
                    int current_high_score = read_high_score("dodge");
                    if (dodge_game.score > current_high_score) {
                        new_record = true;
                        write_high_score("dodge", dodge_game.score);
                        current_high_score = dodge_game.score;
                    }
                    
                    show_game_over_screen(dodge_game.score, current_high_score, new_record);
                    
                } else if (current_selection == GAME_TILT_MAZE) {
                    TiltMazeGame tilt_game;
                    tilt_maze_init(&tilt_game);
                    
                    int16_t ax, ay, az;
                    float filtered_ax = 0, filtered_ay = 0;
                    const float alpha = 0.2;
                    const float threshold = 0.3;
                    
                    while (!tilt_game.game_over) {
                        mpu6050_read_accel(&ax, &ay, &az);
                        float gx = ax / 16384.0;
                        float gy = ay / 16384.0;
                        filtered_ax = low_pass_filter(gx, filtered_ax, alpha);
                        filtered_ay = low_pass_filter(gy, filtered_ay, alpha);
                        
                        int dx = 0, dy = 0;
                        
                        if (fabsf(filtered_ax) > threshold || fabsf(filtered_ay) > threshold) {
                            if (fabsf(filtered_ax) > fabsf(filtered_ay)) {
                                dx = (filtered_ax > 0) ? 1 : -1;
                            } else {
                                dy = (filtered_ay > 0) ? 1 : -1;
                            }
                        }
                        
                        tilt_maze_update(&tilt_game, dx, dy);
                        tilt_maze_render(&tilt_game);
                        
                        if(tilt_game.level_complete) {
                            vTaskDelay(2000 / portTICK_PERIOD_MS);
                            
                            if(tilt_game.level < 5) {
                                tilt_maze_init_level(&tilt_game, tilt_game.level + 1);
                                tilt_game.level_complete = false;
                                tilt_game.food_count = 4;
                                
                                vTaskDelay(500 / portTICK_PERIOD_MS); 
                            } else {
                                tilt_game.game_over = true;
                                
                                buzzer_play_tone(1000, 100);
                                vTaskDelay(50 / portTICK_PERIOD_MS);
                                buzzer_play_tone(1200, 100);
                                vTaskDelay(50 / portTICK_PERIOD_MS);
                                buzzer_play_tone(1500, 200);
                                
                                int score = tilt_game.level * 100;
                                bool new_record = false;
                                int current_high_score = read_high_score("tilt_maze");
                                if (score > current_high_score) {
                                    new_record = true;
                                    write_high_score("tilt_maze", score);
                                    current_high_score = score;
                                }
                                
                                char end_text[30];
                                if(tilt_game.level == 5 && tilt_game.level_complete) {
                                    snprintf(end_text, sizeof(end_text), "Voce venceu!");
                                } else {
                                    snprintf(end_text, sizeof(end_text), "Fim de jogo");
                                }
                                
                                clear_screen();
                                draw_text(WIDTH/2 - 30, HEIGHT/2 - 20, end_text);
                                draw_text(WIDTH/2 - 40, HEIGHT/2 - 10, "Pontuacao:");
                                char score_text[20];
                                snprintf(score_text, sizeof(score_text), "%d", score);
                                draw_text(WIDTH/2 - 20, HEIGHT/2, score_text);
                                
                                if (new_record) {
                                    draw_text(WIDTH/2 - 40, HEIGHT/2 + 10, "Novo Recorde!");
                                    play_new_record_sound();
                                } else {
                                    draw_text(WIDTH/2 - 40, HEIGHT/2 + 10, "Recorde:");
                                    snprintf(score_text, sizeof(score_text), "%d", current_high_score);
                                    draw_text(WIDTH/2 - 20, HEIGHT/2 + 20, score_text);
                                }
                                
                                draw_text(WIDTH/2 - 50, HEIGHT/2 + 30, "Pressione um botao");
                                update_display();
                                
                                while(!gpio_get_level(SELECT_BUTTON) && !gpio_get_level(NAVIGATE_BUTTON)) {
                                    vTaskDelay(100 / portTICK_PERIOD_MS);
                                }
                                while(gpio_get_level(SELECT_BUTTON) || gpio_get_level(NAVIGATE_BUTTON)) {
                                    vTaskDelay(100 / portTICK_PERIOD_MS);
                                }
                            }
                        }
                        
                        vTaskDelay(GAME_SPEED / portTICK_PERIOD_MS);
                    }
                }
                
                in_menu = true;
            }
            
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}
void app_main() {
    i2c_master_init();
    ssd1306_init();
    mpu6050_init();
    buzzer_init(); // Inicializa o buzzer
    
    // Tenta inicializar o cartão SD
    sd_card_initialized = init_sd_card();
    if (!sd_card_initialized) {
        ESP_LOGE(TAG, "Falha ao inicializar o cartão SD. O sistema continuará sem armazenamento de recordes.");
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
    xTaskCreate(game_task, "game_system", 8192, NULL, 5, NULL);
}
