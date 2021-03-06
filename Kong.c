#include <conf.h>
#include <kernel.h>
#include <io.h>
#include <bios.h>
#include <time.h>

#include "maps.h"

#define RAND_2(MAX, MIN) (rand() % (MAX - MIN)) + MIN

#define TICKS_IN_A_SECOND 18
#define CYCLE_UPDATER 0
#define CYCLE_MANAGER 0
#define CYCLE_DRAWER 0
#define SCHED_ARR_LENGTH 5
#define DEFAULT_CYCLE_LENGTH 1

#define ARROW_UP 72
#define ARROW_DOWN 80
#define ARROW_RIGHT 77
#define ARROW_LEFT 75

#define KEY_W 17
#define KEY_A 30
#define KEY_S 31
#define KEY_D 32
#define KEY_SPACE 57
#define KEY_ENTER 28
#define KEY_ESC 1

#define SOUND_PLAY_DELAY 1
#define SOUND_BARREL_HIT_FREQ 87
#define SOUND_HAMMER_PICKUP_FREQ 294
#define SOUND_HAMMER_HIT_FREQ 49
#define SOUND_NEW_LEVEL_FREQ 294
#define SOUND_GAME_OVER_FREQ 18
#define SOUND_GAME_WON_FREQ 3951

#define MAIN_MENU_COUNT 2
#define GAME_OVER_COUNT 2
#define MENU_MAX_STRINGS 4

#define MAX_GAME_OBJECTS 64
#define MAX_BARRELS_OBJECT MAX_GAME_OBJECTS - 1
#define MAX_SAVED_INPUT 4

#define JUMP_DURATION_IN_TICKS 25
#define HAMMER_DURATION_IN_TICKS 3

#define GAME_OBJECT_LABEL_LENGTH 18

#define PLAYER_LIFE_COUNT 3
#define PLAYER_START_POS_X 40
#define PLAYER_START_POS_Y 20
#define HAMMER_MAX_HITS 4

#define FALLING_BARREL_SPAWN_IN_TICKS 18*4
#define FALLING_BARREL_MAX_FALL 6*14

#define MAX_POINTS 99999
#define MIN_POINTS 0
#define POINTS_BARREL_HIT 100
#define POINTS_GAME_WON 750
#define POINTS_LEVEL_WON 500
#define POINTS_EVERY_SEC 10
#define POINTS_EVERY_MINUTE 50
#define POINTS_LOSING_LIFE -300

extern struct intmap far *sys_imp;

/* Enums */
typedef enum gameState{
    InGame = 0,
    InMenu = 1,
    InGameOver = 2,
    InGameWon = 3
} GameState;

/* Structs */
// Used to save the position of elements
typedef struct Position{
    int x;
    int y;
} position;

// Used to store information about game objects in the game
typedef struct GameObject{
    // The label of the game object
    // Can be used to identified the object
    char label[GAME_OBJECT_LABEL_LENGTH];

    // The top left point of the game object
    position top_left_point;

    // The width of the game object
    int width;
    // The height of the game object
    int height;

    // The model of the game object
    char** model;
} gameObject;

// Used to store info about the barrel
typedef struct Barrel{
    // The barrel game object
    gameObject* obj;

    // How many ticks to move
    int movement_ticks;

    // How many ticks to apply gravity
    int gravity_ticks;

    // Is the barrel grounded ?
    int is_grounded;

    // The direction of the movement (1 = Right, -1 = Left)
    int movement_direction;

    // Is the barrel a falling down barrel ?
    int is_falling_barrel;

    // How mant ticks to fall
    int falling_ticks;
} barrel;

/* Time vars */
// Counting the ticks
int clock_ticks = 0;
// Counting the seconds
int clock_seconds = 0;
// Counting the minutes
int clock_minutes = 0;
// Counter of deltaTimes so we can keep track of time
int deltaTime_counter = 0;
// Saves the last elapsed time the function was called
int time_handler_last_call = 0;
// A way to not lose any seconds
int deltaSeconds = 0;
// External counting of ticks that has passed (in the file clkint.c)
extern int elapsed_time;

/* Schedule vars */
int sched_arr_pid[SCHED_ARR_LENGTH] = { -1 };
int sched_arr_int[SCHED_ARR_LENGTH] = { -1 };
int point_in_cycle;
int gcycle_length;
int gno_of_pids;

/* Input Queue vars */
// Saves the input from the player
int input_queue[MAX_SAVED_INPUT];
// Saves the count of input that was recevied
int input_queue_received = 0;
// The index of the recevied inputs
int input_queue_tail = 0;

/* PIDs vars */
int receiver_pid;
int time_handler_pid;
int updater_pid;
int drawer_pid;
int manager_pid;
int bg_pid;
int sounder_pid;

/* Drawer vars */
// the whole display is represented here: 1 pixel = 1 cell
char display[SCREEN_SIZE + 1];
// Saves the color of each cell in the display
char display_color[SCREEN_SIZE + 1];
// a draft of the display, later we copy the draft to the display array
char display_draft[SCREEN_HEIGHT][SCREEN_WIDTH];
// a draft of the display color
char display_draft_color[SCREEN_HEIGHT][SCREEN_WIDTH];

/* Game vars */
int game_level = 1;
// Mario got to the princess ?
int mario_got_to_princess = 0;
// The counter that holds how many lifes mario has
int player_lives = PLAYER_LIFE_COUNT;
// Holds the score of the player
int player_score = 0;
// The index of the selected button on the menu
int menu_index = 0;
// is the game ready for playing ?
int game_init = 0;
// Saves the prev game state
enum GameState prev_game_state = -1;
// Holds the current state of the game (in game, in game over, in menu...)
enum GameState gameState = InMenu;

/* Player vars */
char mario_model[3][3] = 
{
    " 0 ",
    "-#-",
    "| |"
};
// The game object of the player
gameObject playerObject = {"Player", {PLAYER_START_POS_X, PLAYER_START_POS_Y}, 3, 3, (char**) mario_model};
// Saves the time that the player jumped (used to know if to apply gravity to the player)
int air_duration_elapsed = 0;
// is the player on top of a ladder
int on_top_ladder = 0;
// is the player with the hammer
int is_with_hammer = 0;
// The direction the player is 'looking' (used to align the hammer)
int player_movement_direction = 0;

/* Princess vars */
char princess_model[2][2] = {
    "$$",
    "$$"
};
// The game object of the princess
gameObject princessObject = {"Princess", {35 ,2}, 2, 2, (char**) princess_model};

/* Kong vars */
char kong_model[3][3] = 
{
    "_K_",
    "<#>",
    "V V"
};
// The game object of kong
gameObject kongObject = {"Kong", {22, 5}, 3, 3, (char**) kong_model};

/* Hammer vars */
char hammer_model[1][2] = 
{
    "%%"
};
// The game object of THE HAMMER
gameObject hammerObject = {"Hammer", {37, 20}, 2, 1, (char**) hammer_model};
// Count how many hits left to the hammer
int hammer_hits_left = HAMMER_MAX_HITS;
// The time it takes to recover from a hit
int hammer_hit_duration = 0;
// Is the hammer in the map
int is_hammer_exist = 0;

/* Barrels vars*/
char barrel_model[1][2] = 
{
    "00"
};
char falling_barrel_model[1][2] = 
{
    "OO"
};
// Array that holds all the barrels that are in the game
barrel* barrels_array[MAX_GAME_OBJECTS];
// The index that iterate the barrels array
int barrels_array_index = 0;
// Timer to know when to spawn a new barrel
int spawn_barrel_timer = 0;
// How long do we wait between spawning a new barrel
int spawn_barrel_speed_in_ticks = 6 * 18;
// How long do we wait between moving the barrels
int barrel_movement_speed_in_ticks = 5;
// Timer to know when to spawn a new falling barrel
int spawn_falling_barrel_timer = 0;
// How long do we wait between spawning a new falling barrel
int spawn_falling_barrel_speed_in_ticks = 4 * 18;

/* Ladders vars */
// Using a pointer to know what (level) ladders to draw
char* ladder_map_ptr = NULL;
gameObject laddersObject = {"Ladders", {0,0}, SCREEN_WIDTH, SCREEN_HEIGHT, (char**) ladders_level_1};

/* Gravity vars */
// Apply gravity every number of ticks
int apply_gravity_every_ticks = 5;
// The counter of passed ticks
int gravity_ticks = 5;

/* Screen vars */
// Saves the color byte of the screen before the game
char saved_color_byte;
// Is the user exited the game
int game_exited = 0;
// Screen game object, used to detect if the objects are inside it
gameObject screenObject = {"Screen", {0,0}, SCREEN_WIDTH, SCREEN_HEIGHT, (char**) map_1};

// Changes the state of the game
// Also saves the prev one
void change_game_state(GameState new_state){
    prev_game_state = gameState;
    gameState = new_state;
}

// Turns the speaker on or off
void set_speaker(int status){
    // Holds the port 61h info
    int port_val = 0;

    // Getting the info from port 61h
    asm{
        PUSH AX
        MOV AL, 61h
        MOV BYTE PTR port_val, AL
        POP AX
    }

    // if we want to turn on the speakers we need to switch on bit 1 and 2
    // if we want to turn off the speakers we need to switch off bit 1 and 2
    // 3 = 00000011
    if (status) port_val |= 3;
    else port_val &=~ 3;

    // Setting the action we want after we changed the values (we need to set them back to port 61h)
    asm{
        PUSH AX
        MOV AX, WORD PTR port_val
        OUT 61h, AL
        POP AX
    }
}

// Plays sound with the freq set to hertz
void play_sound(int hertz){
    // The formula is: counter = (PIT frequency) / (frequency we want)
    unsigned int final_counter = 1193180L / hertz;

    // Setting the speakers on
    set_speaker(TRUE);

    // Setting our wanted vars to the PIT
    // 0B6h = 10110110
    // Left-To-Right: 10 - PIT Counter 2, 11 Read/Write lower byte first, 011 Mode 3, 0 Binary Counting
    asm{
        PUSH AX
        MOV AL, 0B6h
        OUT 43h, AL
        POP AX
    }

    // Port 42 for Counter #2
    // Setting the counter (LSB)
    asm{
        PUSH AX
        MOV AX, WORD PTR final_counter
        AND AX, 0FFh
        OUT 42h, AL
        POP AX
    }

    // Setting the counter (MSB)
    asm{
        PUSH AX
        MOV AX, WORD PTR final_counter
        MOV AL, AH
        OUT 42h, AL
        POP AX
    }
}

// Turns the speakers off
void stop_sound(){
    set_speaker(0);
}

// Handles the sound in the game
void sounder(){
    while(TRUE){
        play_sound(receive());
        sleept(SOUND_PLAY_DELAY);
        stop_sound();
    }
}

// Sends a msg to the sounder to play a note
void send_sound(int hertz){
    send(sounder_pid, hertz);
}

// Adds/Subs from the player's lives
void add_player_life(int lp){
    player_lives += lp;
}

// Adds score points the the player's score
void add_score_points(int points){
    player_score += points;

    // Boundries for the score
    if (player_score > MAX_POINTS) player_score = MAX_POINTS;
    if (player_score < MIN_POINTS) player_score = MIN_POINTS;
}

// Decrease the player's life by 1
// Subs points from the score as well
void sub_player_life(){
    add_player_life(-1);
    add_score_points(POINTS_LOSING_LIFE);
}

// This function is used to better print to the console
// Avoiding flickering, more color options and shit...
void print_to_screen(){
    int i = 0;
    int j = 0;
    char current_pixel;
    char current_color;

    // The default color byte that we use to print to the console
    // the default is white text with black background
    // 15 = 00001111 = white text black bg
    // 12 = 00001100 = red text black bg
    char default_color_byte = 15;

    // Init the screen to top left point
    asm{
        MOV AX, 0B800h
        MOV ES, AX
        MOV DI, 0
    }

    for (i = 0; i < SCREEN_HEIGHT; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            // Getting the char we need to print
            current_pixel = display[i * SCREEN_WIDTH + j];
            current_color = display_color[i * SCREEN_WIDTH + j];

            // Printing the char to the console
            // and advancing the index to the next cell of the console
            asm{
                MOV AL, BYTE PTR current_pixel
                MOV AH, BYTE PTR current_color

                MOV ES:[DI], AX
                ADD DI, 2
            }
        }
    }
}

// Wipes the entire screen
void wipe_entire_screen(){
    int i = 0;
    int j = 0;

    // Loop through the display var and fills it with empty spaces
    // and fills the color of every cell with the default white text black background
    for (i = 0; i < SCREEN_HEIGHT; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            display[i * SCREEN_WIDTH + j] = ' ';
            display_color[i * SCREEN_WIDTH + j] = 15;
        }
    }

    // The game is in exiting stage
    game_exited = 1;
    // Prints a black screen
    print_to_screen();
}

// Returns the output to white text and black
void reset_output_to_screen(){
    // Sets the color byte of the screen
    asm{
        MOV AX, 0B800h
        MOV ES, AX

        MOV AH, BYTE PTR saved_color_byte
    }
}

// Saves the color byte of the output to the console
// so we can reset it when closing the game
void save_out_to_screen(){
    // Saves the color byte of the screen
    asm{
        MOV AX, 0B800h
        MOV ES, AX

        MOV saved_color_byte, AH
    }
}

// Handles the scan codes and ascii codes from the input
// Returns the scan code of the key that was pressed
int scanCode_handler(int scan, int ascii){
    // if the user pressed CTRL+C
    // We want to terminate xinu (int 27 -> terminate xinu)
    if ((scan == 46) && (ascii == 3)){
        // Resets the output to the screen
        reset_output_to_screen();
        wipe_entire_screen();
        set_speaker(0);
        asm INT 27;
    }
    
    // returns the scan code of the key that was pressed
    return scan;
}

// Routine #9
INTPROC _int9(int mdevno){
    int result = 0;
    int scan_code = 0;
    int ascii_code = 0;

    // Gets the input from the keyboard

    // ah = 1: Check if there was a key press
    // then if there is no key press we skip (going to skip_input)
    // if there is a key press we setting ah = 0
    // and calling routine 16
    // after the routine ah = scan code and al = ascii code
    asm{
        MOV AH, 1
        INT 16H
        JZ SKIP_INPUT
        MOV AH, 0
        INT 16H
        MOV BYTE PTR scan_code, AH
        MOV BYTE PTR ascii_code, AL
    }

    result = scanCode_handler(scan_code, ascii_code);

    // Sends a msg (the msg is the result) to the receiver process
    send(receiver_pid, result);

    SKIP_INPUT:
}

// Sets our new routine instead of the old one
void set_int9(){
    int i;
    for(i = 0; i < 32; i++){
        if (sys_imp[i].ivec == 9){
            sys_imp[i].newisr = _int9;
            return;
        }
    }
}

// The scheduling function
SYSCALL schedule(int no_of_pids, int cycle_length, int pid1, ...){
    int i;
    int ps;
    int *iptr;

    disable(ps);

    gcycle_length = cycle_length;
    point_in_cycle = 0;
    gno_of_pids = no_of_pids;
    
    iptr = &pid1;
    for (i = 0; i < no_of_pids; i++){
        sched_arr_pid[i] = *iptr;
        iptr++;
        sched_arr_int[i] = *iptr;
        iptr++;
    }

    restore(ps);
}

// Keeps track of time
void time_handler(){
    /* Time keeping vars */
    // Holds the time diffrences between the last call and the current call of this function
    int deltaTime = 0;
    // The msg the was recieved
    int received_msg;

    int i = 0;

    while(TRUE){
        // Waiting for the time routine to wake up this process
        // Basically waiting for a tick to pass
        received_msg = receive();

        // if the game is in game and the game is ready for play
        if (gameState == InGame && game_init){
            /* Time tracking */
            // delta time is the measurement of the time between calls
            deltaTime = elapsed_time - time_handler_last_call;
            // delta time counter is the counter of how many ticks passed
            deltaTime_counter += deltaTime;
            // Saving the counter to global use
            clock_ticks = deltaTime_counter;
            // saving the last call of the updater to measure the delta time
            time_handler_last_call = elapsed_time;

            // Updating the timer of the gravity
            gravity_ticks -= deltaTime;
            // Updating the timer of the spawning of barrels
            spawn_barrel_timer -= deltaTime;

            // if it's not the first level
            if (game_level > 1){
                // Updating the timer of the spawning of falling barrels
                spawn_falling_barrel_timer -= deltaTime;
            }

            // Updating the timers of all the barrel's movement
            for (i = 0; i < MAX_BARRELS_OBJECT; i++){
                // if the barrel exists
                if (barrels_array[i]){
                    barrels_array[i]->movement_ticks -= deltaTime;
                    // if it's not the first level
                    if (game_level > 1){
                        // if it's a falling barrel, update it's falling timer
                        if (barrels_array[i]->is_falling_barrel){
                            barrels_array[i]->falling_ticks -= deltaTime;
                        }
                    }
                }
            }

            // Checks if a second has passed
            if (deltaTime_counter >= TICKS_IN_A_SECOND){
                clock_seconds++;
                // if the player is playing the game
                // every second add points to his score for survival
                if (gameState == InGame && game_init) add_score_points(POINTS_EVERY_SEC);

                // At least a minute has passed
                if (clock_seconds >= 60){
                    // Gets how many seconds we are passed the 60 seconds mark
                    deltaSeconds = clock_seconds - 60;
                    // Resets the seconds clock
                    clock_seconds = 0;
                    // Adds to it the delta seconds so that we dont lose any seconds
                    clock_seconds += deltaSeconds;
                    // A minute has passed!
                    clock_minutes++;
                    // if the player is playing the game
                    // every minute add points for his score for survival
                    if (gameState == InGame && game_init) add_score_points(POINTS_EVERY_MINUTE);
                }

                // We want every second to reset the counter
                deltaTime_counter = 0;
                // Resetting the global ticks counter
                clock_ticks = 0;
            }
        }
    }
}

// Handles the drawing to the 'screen'
void drawer(){
    while (TRUE){
        receive();
        // if the game was exited we dont want to keep drawing to the screen
        if (game_exited) continue;
        print_to_screen();
    }
}

// Deletes a barrel from the game
// frees it's memory and stuff
void delete_barrel(int index_in_array){
    barrel* barrelToDelete = barrels_array[index_in_array];

    // Setting the index to be free so we can spawn more barrels
    barrels_array[index_in_array] = NULL;

    // First freeing the gameobject memory than the barrel
    freemem(barrelToDelete->obj, sizeof(struct GameObject));
    freemem(barrelToDelete, sizeof(struct Barrel));
}

// Checks a collision between 2 game objects
// Returns 1 if there was a collision between the two game objects
// Returns 0 if no collision
int check_collision_with_rectangle(gameObject* obj_a, gameObject* obj_b){
    // Taking the top left point of the model of the game object
    position top_left = obj_a->top_left_point;

    // Getting the dimensions of the game object's model
    int model_height = obj_a->height;
    int model_width = obj_a->width;

    // Getting the position and size of the second game object
    int obj_b_x = obj_b->top_left_point.x;
    int obj_b_y = obj_b->top_left_point.y;
    int obj_b_height = obj_b->height;
    int obj_b_width = obj_b->width;
    
    // Calculating edges for the first rectangle
    int obj_a_right = top_left.x + model_width - 1;
    int obj_a_left = top_left.x;
    int obj_a_top = top_left.y;
    int obj_a_bottom = top_left.y + model_height - 1;

    // Calculating edges for the second rectangle
    int obj_b_right = obj_b_x + obj_b_width - 1;
    int obj_b_left = obj_b_x;
    int obj_b_top = obj_b_y;
    int obj_b_bottom = obj_b_y + obj_b_height - 1;
    
    // Checks for collision between 2 rectangles
    if (obj_a_right >= obj_b_left && obj_a_left <= obj_b_right && obj_a_bottom >= obj_b_top && obj_a_top <= obj_b_bottom){
        return 1;
    }
    
    return 0;
}

// Checks if there is a collision between the gameobject and the barrel in index_in_array
void check_collision_with_a_barrel(gameObject* obj, int index_in_array){
    // Taking the top left point of the model of the game object
    position top_left = obj->top_left_point;

    // Getting the dimensions of the game object's model
    int model_height = obj->height;
    int model_width = obj->width;

    // Getting the position and size of the barrel
    barrel* barrel = barrels_array[index_in_array];
    int barrel_x = barrel->obj->top_left_point.x;
    int barrel_y = barrel->obj->top_left_point.y;
    int barrel_height = barrel->obj->height;
    int barrel_width = barrel->obj->width;
    
    // Calculating edges for the first rectangle
    int obj_right = top_left.x + model_width - 1;
    int obj_left = top_left.x;
    int obj_top = top_left.y;
    int obj_bottom = top_left.y + model_height - 1;

    // Calculating edges for the second rectangle
    int b_right = barrel_x + barrel_width - 1;
    int b_left = barrel_x;
    int b_top = barrel_y;
    int b_bottom = barrel_y + barrel_height - 1;

    // Checks for collision between 2 rectangles
    if (obj_right >= b_left && obj_left <= b_right && obj_bottom >= b_top && obj_top <= b_bottom){
        // if we are here there was a collision with a barrel

        // Play a sound to indicate there was a collision with a barrel
        send_sound(SOUND_BARREL_HIT_FREQ);
        // Delete the barrel we just hitted
        delete_barrel(index_in_array);
        
        // if the game object is the player
        // Decrease his lives
        if (strstr(obj->label, "Player"))
            sub_player_life();
    }

}

// Checks for collisions inside the game object model
// Returns 0 - no collisions
// Returns 1 - ladder
int check_collision_with_ladder(gameObject* obj, int below){
    // Taking the top left point of the model of the game object
    position top_left = obj->top_left_point;

    // Getting the dimensions of the game object's model
    int model_height = obj->height;
    int model_width = obj->width;

    int i = 0;
    int j = 0;
    char currentLadderPixel;

    // if we want to check inside the player model
    if (!below){
        // looping from the top left point to the right bottom point
        // and checking for collisions
        for (i = top_left.y; i < top_left.y + model_height; i++){
            for (j = top_left.x; j < top_left.x + model_width; j++){
                if (ladder_map_ptr != NULL){
                    if (ladder_map_ptr[i * SCREEN_WIDTH + j] == '_') return 1;
                }
            }
        }
    }else {
        // Check all the pixels below the objects model
        for (i = top_left.x; i < top_left.x + model_width; i++){
            // Getting the current pixel in the ladders matrix
            currentLadderPixel = ladder_map_ptr[(top_left.y + model_height) * SCREEN_WIDTH + i];
            if (currentLadderPixel == '_' || currentLadderPixel == '|') return 1;
        }
    }

    return 0;
}

// Checks for collisions
// Returns 1 for no collisions
// Returns 0 for collision with the map
int check_collision_with_map(gameObject* obj, int x_movement, int y_movement){
    // Taking the top left point of the model of the game object
    position top_left = obj->top_left_point;

    // Getting the dimensions of the game object's model
    int model_height = obj->height;
    int model_width = obj->width;

    // Used to know from where to start checking for collision
    // Init with the top left point cords
    int check_pos_x = top_left.x;
    int check_pos_y = top_left.y;

    int i = 0;
    char currentLadderPixel;

    // if we have any movement on the x axis
    if (x_movement != 0){
        // Assuming that the x movement is to the left (negative)
        check_pos_x -= 1;
        // if the x movement is to the right (positive) we want to
        // add the width of the model of the game object plus 1 (because we subtracted it)
        if (x_movement > 0) check_pos_x += model_width + 1;


        // We want to check the pixels to the right/left of the model
        // meaning if the model height is 3, we need to check 3 pixels to the right/left
        // of the model for collision
        for (i = top_left.y; i < top_left.y + model_height; i++){
            // Check for collision with the map elements
            if (map_1[i][check_pos_x] == 'z' || map_1[i][check_pos_x] == 'Z') return 0;
        }
    }

    // if we have any movement on the y axis
    if (y_movement != 0){
        // Assuming that the y movement is up (negative)
        check_pos_y -= 1;
        // if the y movement is down (positive) we want to
        // add the height of the model of the game object plus 1 (because we subtracted it)
        if (y_movement > 0) check_pos_y += model_height + 1;

        // We want to check the pixels up/down of the model
        // meaning if the model width is 2, we need to check 2 pixels above/below
        // of the model for collision
        for (i = top_left.x; i < top_left.x + model_width; i++){
            // Check for collision with the map elements
            if (map_1[check_pos_y][i] == 'z' || map_1[check_pos_y][i] == 'Z') return 0;
        }
    }

    // if we dont have any collisions we can move
    return 1;
}

// Adds a movement to the object
void add_to_object_position(gameObject* obj, int x_movement, int y_movement){
    // Adds the movement on the axis to the position of the game object
    (obj->top_left_point).x += x_movement;
    (obj->top_left_point).y += y_movement;
}

// Spawns the hammer at one of the first 3 platform (NOT ON KONG PLATFORM)
void spawn_hammer(){
    // Rough est.
    // First platform: X: [22,57] Y: 22
    // Second platform: X: [22,53] Y: 17
    // Third platform: X: [28,57] Y: 12

    int rnd_x;
    int rnd_y;
    int rnd_platform;
    int platform_choices = 3;
    int num_of_platforms = 3;
    int offset = 1;

    // We want to get the player's feet level
    int player_y = playerObject.top_left_point.y + playerObject.height - 1;

    // if the player has the hammer or the hammer exist on the map
    // we dont want to spawn it again
    if (is_with_hammer || is_hammer_exist) return;
    
    // if the player is on the first platform
    if (player_y <= 22 + offset){
        platform_choices = 3;
    }else if (player_y <= 17 + offset){
        // second platform
        platform_choices = 2;
    }else if (player_y <= 12 + offset){
        // thrid platform
        platform_choices = 1;
    }

    // Randomly select the platform to spawn the hammer on
    rnd_platform = rand() % platform_choices + 1;

    hammerObject.top_left_point.x = 0;
    hammerObject.top_left_point.y = 0;

    // Based on what platform we got to spawn the hammer on
    // Randmoly select x pos to spawn on
    switch(rnd_platform){
        case 1:
            rnd_x = RAND_2(57, 22);
            rnd_y = 22;
        break;

        case 2:
            rnd_x = RAND_2(53, 22);
            rnd_y = 17;
        break;

        case 3:
            rnd_x = RAND_2(57, 28);
            rnd_y = 12;
        break;
    }

    // Sets the position of the hammer to the spawn point
    hammerObject.top_left_point.x = rnd_x;
    hammerObject.top_left_point.y = rnd_y;
    // Telling the game there is a hammer on the map
    is_hammer_exist = 1;
    // Set the hammer hits to the default (4)
    hammer_hits_left = HAMMER_MAX_HITS;

}

// 'Resets' the hammer, basically makes it disappear
void reset_hammer(){
    // Set that the player dont have the hammer
    is_with_hammer = 0;
    // The hammer don't exist
    is_hammer_exist = 0;
    // Spawn a new hammer
    spawn_hammer();
}

// Makes the player wield the hammer!
void set_hammer_player_position(){
    // if the player is not with the hammer than dont do anything here
    if (!is_with_hammer) return;

    // if the player is look to the right
    if (player_movement_direction == 1){
        hammerObject.top_left_point.x = playerObject.top_left_point.x + 3;
    }else {
        // if the player is looking to the left
        hammerObject.top_left_point.x = playerObject.top_left_point.x - 2;
    }
    // The position of the hammer is in the middle of the player's model (talking about height)
    hammerObject.top_left_point.y = playerObject.top_left_point.y + 1;
}

// Movement with collisions
// Moves the object if there are no collisions
// If we want to move any object we want to use this function
void move_object(gameObject* obj, int x_movement, int y_movement){
    // Checks if there is a collision with the map
    int check_movement_map;
    // Checks if there is a collision with a ladder (inside the game object's model)
    int check_movement_ladder_inside;
    // Checks if there is a collision with a ladder (below the game object's model)
    int check_movement_ladder_below;

    // Checks for collisions with the map
    check_movement_map = check_collision_with_map(obj, x_movement, y_movement);

    // if the player has the hammer, than make it walk with it
    // just setting the position of the hammer to the position of the player
    // we gives the hammer time to draw the 'hit' so that's why we got a timer here
    if (is_with_hammer && (elapsed_time - hammer_hit_duration) >= HAMMER_DURATION_IN_TICKS){
        set_hammer_player_position();
    }

    // if there is movement on the y axis
    // and the object is the player
    // annddd the player is on a ladder
    // we want a different movement... ladder movement!
    if (y_movement != 0 && strstr(obj->label, "Player") && on_top_ladder){
        // if we got a down movement
        if (y_movement > 0){
            // Checks for ladders
            check_movement_ladder_inside = check_collision_with_ladder(obj, 0);
            check_movement_ladder_below = check_collision_with_ladder(obj, 1);

            // if the player is colliding with a ladder
            // and he can move without colliding with the map
            // we can move
            if (check_movement_ladder_inside && check_movement_map){
                add_to_object_position(obj, x_movement, y_movement);            
            }else {
                // if the player is not colliding with a ladder
                // or he is colliding with the map
                // and if there is a ladder below him -> we can move
                if (check_movement_ladder_below)
                    add_to_object_position(obj, x_movement, y_movement);
            }
        }else {
            // if the movement is up we dont fancy calculations
            add_to_object_position(obj, x_movement, y_movement);
        }
    }else {
        // If the new movement is valid (1 = no collisions)
        if (check_movement_map){
            add_to_object_position(obj, x_movement, y_movement);
        }
    }
}

// Makes the hammer hit!
void hammer_hit(){
    int i = 0;

    // Move the hammer for the hit
    move_object(&hammerObject, 0, 1);
    // Setting the start of the hit
    hammer_hit_duration = elapsed_time;
    // Check for collision with the barrels
    for (i = 0; i < MAX_BARRELS_OBJECT; i++){
        if (barrels_array[i]){
            if (check_collision_with_rectangle(&hammerObject, barrels_array[i]->obj)){
                // Playing a sound to indicate that the hammer hit a barrel
                send_sound(SOUND_HAMMER_HIT_FREQ);
                // Delete the barrel
                delete_barrel(i);
                // Decrease the hammer hits that is left
                hammer_hits_left--;
                // Adds points to the player for destroying the barrel
                add_score_points(POINTS_BARREL_HIT);
                // if we ran out of hits, spawn a new hammer
                if (hammer_hits_left <= 0){
                    reset_hammer();
                }
            }
        }
    }
}

// Makes the player jump
void player_jump(){
    // Checks if the player is grounded
    if (!check_collision_with_map(&playerObject, 0, 1)){
        // Try to move the player up
        move_object(&playerObject, 0, -1);
        // Set the duration if the air, so we have some air time
        air_duration_elapsed = elapsed_time;
    }
}

// Moves all the barrels in the map
void move_barrels(){
    int i = 0;
    // Holds the current barrel that we work with
    barrel* barrel;

    for (i = 0; i < MAX_BARRELS_OBJECT; i++){
        // if the barrel exist
        if (barrels_array[i]){
            // if the barrel game object exists
            if (barrels_array[i]->obj){
                barrel = barrels_array[i];
                // if it's time to move the barrel, end of timer
                if (barrel->movement_ticks <= 0) {
                    // Resetting the barrel's movement timer
                    barrel->movement_ticks = barrel_movement_speed_in_ticks;
                    // if the barrel in on the platform (grounded)
                    // we want a movement on the x axis only if the barrel is grounded
                    if (!check_collision_with_map(barrel->obj, 0, 1)){
                        // The barrel is grounded
                        barrel->is_grounded = 1;
                        // Try to move the barrel to the movement direction
                        move_object(barrel->obj, barrel->movement_direction, 0);
                    }else {
                        // if the barrel was on top of a platform
                        // and now it's falling, we want to change the direction of movement
                        if (barrel->is_grounded){
                            // Now the barrel is falling
                            barrel->is_grounded = 0;
                            // Changing the direction of the barrel (to make to zig zag movement)
                            barrel->movement_direction *= -1;
                        }
                    }
                }

                // if the barrel is a falling barrel
                if (barrel->is_falling_barrel){
                    // if it's time for the barrel to fall and the barrel in on the ground
                    if (barrel->falling_ticks <= 0 && barrel->is_grounded){
                        // The barrel is not on the ground (because it's falling.... dah)
                        barrel->is_grounded = 0;
                        // Changing the direction of the movement
                        barrel->movement_direction *= -1;
                        // Drop the barrel one cell down
                        // Because right now the barrel in on top of a platform and we want it to fall
                        add_to_object_position(barrel->obj, 0, 1);
                        // Sets a new timer 'randomly'
                        barrel->falling_ticks = rand() % (FALLING_BARREL_MAX_FALL + 1) + spawn_falling_barrel_speed_in_ticks;
                    }
                }
                // Check for collision with the player
                check_collision_with_a_barrel(&playerObject, i);

                // if the barrel does not collide with the screen
                // than it's outside the screen so we want to delete it
                if (!check_collision_with_rectangle(barrel->obj, &screenObject)) delete_barrel(i);
            }
        }
    }
}

// Apply gravity to all the game objects
void apply_gravity_to_game_objects(){
    int i = 0;
    gameObject* barrelObj;

    // if the player is not on a ladder
    if (!on_top_ladder){
        // if it's time to try to apply gravity to the player
        // (we give the player some air time so we have the effect of a fall)
        if ((elapsed_time - air_duration_elapsed) >= JUMP_DURATION_IN_TICKS){
            // Try to move the player down
            move_object(&playerObject, 0, 1);
        }
    }

    for (i = 0; i < MAX_BARRELS_OBJECT; i++){
        // if the barrel exists
        if (barrels_array[i]){
            if (barrels_array[i]->obj){
                // Try to move the barrel down
                move_object(barrels_array[i]->obj, 0, 1);
                // if the barrel it outside the screen, delete it
                if (!check_collision_with_rectangle(barrels_array[i]->obj, &screenObject)) delete_barrel(i);
            }
        }
    }
}

// Copies the display_draft to the display so it can be draw
void save_display_draft(){
    int i = 0;
    int j = 0;
    
    // Loops through the display and copy the display draft to it
    for (i = 0; i < SCREEN_HEIGHT; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            display[SCREEN_WIDTH * i + j] = display_draft[i][j];
            display_color[SCREEN_WIDTH * i + j] = display_draft_color[i][j];
        }
    }
    // Setting the null char (the last char of the array)
    display[SCREEN_SIZE] = '\0';
}

// Inserts the ladders to the map
// level - the level num
void insert_ladders_to_map(int level){
    int i = 0;
    int j = 0;

    // Change the ladders map accroding to what level we are
    switch (level){
        case 1: ladder_map_ptr = ladders_level_1[0];
        break;
        case 2: ladder_map_ptr = ladders_level_2[0];
        break;
        case 3: ladder_map_ptr = ladders_level_3[0];
        break;
    }

    // Copying the ladders to the draft
    for (i = 0; i < SCREEN_HEIGHT; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            // Getting the current cell in the matrix of ladders
            char currentPixel = ladder_map_ptr[SCREEN_WIDTH * i + j];
            // if it's a ladder we want to add it to the draft
            if (currentPixel == '|' || currentPixel == '_'){
                display_draft[i][j] = currentPixel;
                // light gray color for the ladders
                display_draft_color[i][j] = 7;
            }
        }
    }
}

// Used to insert models of game objects to the display draft
void insert_model_to_draft(gameObject* gameObj, char color_byte){
    // To iterate the display draft height
    int i = 0;
    // To iterate the display draft width
    int j = 0;
    // To iterate the object model
    int k = 0;

    /* Vars to ease of use */
    // The position of the top left point of the model of the object
    position objectPos = gameObj->top_left_point;

    // Getting the model's position
    int top_left_x = objectPos.x;
    int top_left_y = objectPos.y;

    // Getting the model dimensions
    int model_height = gameObj->height;
    int model_width = gameObj->width;

    // Getting the object's model
    char* objectModel = (char*) gameObj->model;

    // We need to start saving to the draft from the
    // top left point (kinda the position of the object)
    // and keep drawing from there
    // top_left_y <= i < top_left_y + model height
    // top_left_x <= j < top_left_x + model width
    for (i = top_left_y; i < top_left_y + model_height; i++){
        for (j = top_left_x; j < top_left_x + model_width; j++){
            // We want to add only the parts that are inside the screen borders
            // So we check if the current position is inside or outside the screen
            if (!(i >= SCREEN_HEIGHT)){
                // Insert the model to the display draft
                display_draft[i][j] = objectModel[k];
                // Insert the model's color to the display draft
                display_draft_color[i][j] = color_byte;
                k++;
            }
        }
    }
}

// Refills the dispaly draft with map
void refill_display_draft(char map[SCREEN_HEIGHT][SCREEN_WIDTH], char color_byte){
    int i = 0;
    int j = 0;

    // Loops through the display draft and fills in with the map
    // and also fills the display draft color with the wanted color
    for (i = 0; i < SCREEN_HEIGHT; i++){
        for (j = 0; j < SCREEN_WIDTH; j++){
            display_draft[i][j] = map[i][j];
            display_draft_color[i][j] = color_byte;
        }
    }
}

// Creates a barrel with at (x,y) with movement ticks and gravity ticks
void create_barrel(int x, int y, int movement, int gravity, int is_falling, int falling_ticks){
    barrel* barrel;
    gameObject* barrelObj;

    // if there is no place in the array for the barrel
    // we dont want to create it because we are full
    if (barrels_array[barrels_array_index]) return;

    // Allocate memory
    barrel = (struct Barrel*) getmem(sizeof(struct Barrel));
    barrelObj = (struct GameObject*) getmem(sizeof(struct GameObject));

    // Failed to allocate memory
    if (!barrel || !barrelObj) return;

    // Init the game object of the barrel
    strcpy(barrelObj->label, "Barrel");
    if (!is_falling)
        barrelObj->model = (char**) barrel_model;
    else barrelObj->model = (char**) falling_barrel_model;
    barrelObj->top_left_point.x = x;
    barrelObj->top_left_point.y = y;
    barrelObj->width = 2;
    barrelObj->height = 1;

    // Init the barrel object
    barrel->obj = barrelObj;
    barrel->movement_ticks = movement;
    barrel->gravity_ticks = gravity;
    barrel->is_grounded = 1;
    barrel->movement_direction = 1;
    barrel->is_falling_barrel = is_falling;
    barrel->falling_ticks = falling_ticks;

    // Adding the barrel to the barrels array
    barrels_array[barrels_array_index] = barrel;
    barrels_array_index++;
    if (barrels_array_index >= MAX_BARRELS_OBJECT) barrels_array_index = 0;
}

// Handles the scan code of the input from the keybaord
void handle_player_movement(int input_scan_code){
    // Using the input change the position of the player
    //position* playerPos = &(playerObject.top_left_point);

    // Checks for collision below the player with the map
    int collision_result_map = check_collision_with_map(&playerObject, 0, 1);
    // Checks for collision inside the player for ladders
    int check_movement_ladder_inside = check_collision_with_ladder(&playerObject, 0);
    // Checks for collision below the player for ladders
    int check_movement_ladder_below = check_collision_with_ladder(&playerObject, 1);

    // 1: if the player collided with the map and he is not inside a ladder
    // we are not on a ladder
    // 2: if the player is not near a ladder and there is no ladder below him
    // we are not on a ladder
    if (collision_result_map && !check_movement_ladder_inside ||
    (!check_movement_ladder_inside && !check_movement_ladder_below)) {
        on_top_ladder = 0;
    }

    // if the player is not grounded we dont want it to control mario
    // or is the player standing near a ladder
    if (!collision_result_map || check_movement_ladder_inside || check_movement_ladder_below){
        if ((input_scan_code == ARROW_UP) || (input_scan_code == KEY_W)){
            // if the player is near a ladder
            if (check_movement_ladder_inside){
                // The player is on a ladder
                on_top_ladder = 1;
                // Drops the hammer
                if (is_with_hammer)
                    is_hammer_exist = 0;
                //if (is_with_hammer)
                //    reset_hammer();
                // Try to move the player up
                move_object(&playerObject, 0, -1);
            }else {
                // if the player is not near a ladder
                // than he is trying to jump
                on_top_ladder = 0;
                // The player want to jump jumpy
                player_jump();
            }
        }else if ((input_scan_code == ARROW_RIGHT) || (input_scan_code == KEY_D)){
            // Movement direction is to the right
            player_movement_direction = 1;
            move_object(&playerObject, 1, 0);
        }else if ((input_scan_code == ARROW_LEFT) || (input_scan_code == KEY_A)){
            // Movement direction it to the left
            player_movement_direction = -1;
            move_object(&playerObject, -1, 0);
        }else if ((input_scan_code == ARROW_DOWN) || (input_scan_code == KEY_S)){
            // if the player is above a ladder or on top of a ladder

            // 1: if the player is colliding with a ladder and he is on it -> we can move down the ladder
            // 2: if the player stands above a ladder -> we can move down the ladder
            if ((check_movement_ladder_inside && on_top_ladder) || check_movement_ladder_below){
                // The player is on top of a ladder
                on_top_ladder = 1;
                move_object(&playerObject, 0, 1);
            }
        }else if (input_scan_code == KEY_SPACE){
            // if the player is with the hammer
            if (is_with_hammer){
                // Make the hammer hit
                hammer_hit();
            }
        }
    }

}

// Handles the input from the user when at the menu screens
// Returns 1 if enter was pressed
// Returns 0 other wise
int handle_menu_movement(int input_scan_code, int count_of_menues){
    // Basically updates the index of what button we selected

    if (input_scan_code == ARROW_UP){
        menu_index--;
    }else if (input_scan_code == ARROW_DOWN){
        menu_index++;
    }else if (input_scan_code == KEY_ENTER){
        return 1;
    }

    // Making sure that the index is not out of bounds
    if (menu_index >= count_of_menues) menu_index = 0;
    if (menu_index < 0) menu_index = count_of_menues - 1;
    
    return 0;
}

// Inserts the indication of the life of the player to the display draft
void inesrt_player_life_to_draft(){
    char heart;
    int i = 0;
    int clock_offset = 7;

    for (i = 0; i < player_lives; i++){
        // For every life the player has we print
        display_draft[0][SCREEN_WIDTH - clock_offset - i] = '$';
        // red color
        display_draft_color[0][SCREEN_WIDTH - clock_offset - i] = 4;
    }
}

// Inserts the score text of the player to the display draft
void insert_player_score_to_draft(int y, int offset, char color_byte){
    int i = 0;
    int iteration_score = player_score;

    for (i = 0; i < 5; i++){
        // int to ascii
        display_draft[y][SCREEN_WIDTH - offset - i] = (iteration_score % 10) + '0';
        display_draft_color[y][SCREEN_WIDTH - offset - i] = color_byte;
        iteration_score /= 10;
    }
}

// Inserts the clock the the display draft
void insert_clock_to_draft(){
    // Just inserts the clock of the game to the dispaly draft
    char c_min_h;
    char c_min_l;
    char c_sec_h;
    char c_sec_l;

    c_min_h = (clock_minutes / 10 % 10) + '0';
    c_min_l = (clock_minutes % 10) + '0';
    c_sec_h = (clock_seconds / 10 % 10) + '0';
    c_sec_l = (clock_seconds % 10) + '0';

    display_draft[0][SCREEN_WIDTH - 5] = c_min_h;
    display_draft[0][SCREEN_WIDTH - 4] = c_min_l;
    display_draft[0][SCREEN_WIDTH - 3] = ':';
    display_draft[0][SCREEN_WIDTH - 2] = c_sec_h;
    display_draft[0][SCREEN_WIDTH - 1] = c_sec_l;
}

// Deletes all the barrels
void delete_all_barrels(){
    int i = 0;
    
    for (i = 0; i < MAX_BARRELS_OBJECT; i++){
        if (barrels_array[i])
            delete_barrel(i);
    }
}

// Init the vars for a new level
void init_vars_level(){
    // Lock the game
    game_init = 0;

    delete_all_barrels();

    player_lives = PLAYER_LIFE_COUNT;

    /* Reposition the player */
    playerObject.top_left_point.x = PLAYER_START_POS_X;
    playerObject.top_left_point.y = PLAYER_START_POS_Y;

    /* Restart the clock */
    clock_ticks = 0;
    clock_seconds = 0;
    clock_minutes = 0;
    elapsed_time = 0;
    deltaTime_counter = 0;
    time_handler_last_call = 0;
    deltaSeconds = 0;

    /* Restart the input queue */
    input_queue_received = 0;
    input_queue_tail = 0;

    /* Restart player vars */
    mario_got_to_princess = 0;
    air_duration_elapsed = 0;
    on_top_ladder = 0;
    is_with_hammer = 0;
    player_movement_direction = 0;
    hammer_hits_left = HAMMER_MAX_HITS;
    hammer_hit_duration = 0;
    is_hammer_exist = 0;

    /* Restart barrels vars */
    barrels_array_index = 0;
    spawn_barrel_timer = 0;
    spawn_barrel_speed_in_ticks = 6 * 18;
    barrel_movement_speed_in_ticks = 5;
    spawn_falling_barrel_timer = 4 * 18;
    spawn_falling_barrel_speed_in_ticks = 4 * 18;

    /* Restart gravity vars */
    apply_gravity_every_ticks = 5;
    gravity_ticks = 5;

    // Spawn a new hammer
    reset_hammer();

    // The game is ready to be played
    game_init = 1;
}

// Init all the vars for a new game
void init_vars(){
    // Lock the game
    game_init = 0;

    player_score = 0;

    init_vars_level();

    // Release the game
    game_init = 1;
}

// Returns where on the x axis we need to start printing to center the text
int center_text_in_screen(int len){
    return ((SCREEN_WIDTH / 2) - (len / 2));
}

// Inserts text to display draft
void insert_text_to_draft(char* text, int len, int start_x, int start_y, char color_byte, int left_offset){
    int i = 0;

    for (i = 0; i < len; i++){
        display_draft[start_y][start_x + i + left_offset] = text[i];
        display_draft_color[start_y][start_x + i + left_offset] = color_byte;
    }
}

// Inserts text to the center of the line to the display draft
void insert_text_to_center_of_draft(char* text, int len, int start_y, char color_byte, int left_offset){
    insert_text_to_draft(text, len, center_text_in_screen(len), start_y, color_byte, left_offset);
}

// Handles the pressing of menu button
void handle_menu_entered(int entered, GameState changeToState){
    // if the user pressed enter
    if (entered){
        switch(menu_index){
            // case 0 is always to change to state of the game
            case 0:
                change_game_state(changeToState);
            break;

            case 1:
                // The player want to exit the game
                // Resets the output to the screen
                reset_output_to_screen();
                wipe_entire_screen();
                set_speaker(0);
                asm INT 27;
            break;
        }
    }
}

// Handles input from the user when the game is in one of the menus
// Returns 1 if pressed enter
int updater_handle_menu_input(int count_of_menues){
    int i = 0;
    int result = 0;

    // Loop all the input from the user
    for (i = 0; i < input_queue_received; i++){
        result = handle_menu_movement(input_queue[i], count_of_menues);
    }
    // Resetting the input queue vars for next time
    input_queue_received = 0;
    input_queue_tail = 0;

    return result;
}

// Inserts the models of all the barrels to the dispaly draft
void updater_insert_barrels_to_display_draft(){
    int i = 0;

    for (i = 0; i < MAX_BARRELS_OBJECT; i++){
        // Only if the barrel exists
        if (barrels_array[i]){
            if (barrels_array[i]->obj){
                // if the barrel is a normal one we want to insert to normal barrel model
                // if it's a falling barrel we want to insert a falling barrel model
                if (!barrels_array[i]->is_falling_barrel){
                    insert_model_to_draft(barrels_array[i]->obj, 3);
                } else insert_model_to_draft(barrels_array[i]->obj, 9);
            }
        }
    }
}

// Inserts models to the display draft
void updater_insert_models_to_display_draft(){
    // Barrels
    updater_insert_barrels_to_display_draft();
    // Princess
    insert_model_to_draft(&princessObject, 13);
    // Player
    insert_model_to_draft(&playerObject, 14);
    // Kong
    insert_model_to_draft(&kongObject, 6);

    // Only if the hammer exist in the map we want to draw it
    if (is_hammer_exist)
        insert_model_to_draft(&hammerObject, 15);
}

// Checks for collisions of the player with the barrels
void updater_player_barrels_collision(){
    int j = 0;

    for (j = 0; j < MAX_BARRELS_OBJECT; j++){
        // if the barrel exists
        if (barrels_array[j]){
            // if the barrel's game object exists
            if (barrels_array[j]->obj){
                // Check if the player is colliding with the barrels
                check_collision_with_a_barrel(&playerObject, j);
            }
        }
    }
}

// Checks if it's time to spawn a new falling barrel
void updater_spawn_falling_barrel_timer(){
    // if the spawning falling barrel timer is done we need to spawn a new one
    if (spawn_falling_barrel_timer <= 0 && game_level > 1){
        create_barrel(kongObject.top_left_point.x + 1, kongObject.top_left_point.y + 2,
        barrel_movement_speed_in_ticks, 0, 1, FALLING_BARREL_SPAWN_IN_TICKS);
        // Resetting the spawning falling barrel timer
        spawn_falling_barrel_timer = spawn_falling_barrel_speed_in_ticks;
    }
}

// Checks if it's time to spawn a new normal barrel
void updater_spawn_normal_barrel_timer(){
    // if the spawning barrel timer is done we need to spawn a new one
    if (spawn_barrel_timer <= 0){
        // We create a new barrel at kong's position
        // and init it with the speed of the movement and speed of gravity
        create_barrel(kongObject.top_left_point.x + 1, kongObject.top_left_point.y + 2,
        barrel_movement_speed_in_ticks, 0, 0, 0);
        // Resetting the spawning barrel timer
        spawn_barrel_timer = spawn_barrel_speed_in_ticks;
    }
}

// Checks is the gravity timer is done and we need to apply gravity
void updater_gravity_timer(){
    // if the gravity timer is dont we need to apply gravity
    if (gravity_ticks <= 0){
        // Try to apply gravity to the game objects
        apply_gravity_to_game_objects();
        // Reset the gravity timer
        gravity_ticks = apply_gravity_every_ticks;
    }
}

// Checks and handels if the player is not inside of the screen
void updater_check_is_player_in_screen_boundries(){
    // Checks if the player is not inside the screen
    if (!check_collision_with_rectangle(&playerObject, &screenObject)){
        // decrease the player lifes
        sub_player_life();

        // Reset the player position to the default one
        playerObject.top_left_point.x = PLAYER_START_POS_X;
        playerObject.top_left_point.y = PLAYER_START_POS_Y;
    }
}

// Handles the input from the player
void updater_handle_player_input(){
    int i = 0;
    
    // if there is a input from the player we need to handle it
    for (i = 0; i < input_queue_received; i++){
        // Handle the input from the player
        handle_player_movement(input_queue[i]);
        // Check for collision with the princess
        if (check_collision_with_rectangle(&playerObject, &princessObject)){
            mario_got_to_princess = 1;
        }
        // Check for collision with the hammer
        if (check_collision_with_rectangle(&playerObject, &hammerObject) && !is_with_hammer && !on_top_ladder){
            send_sound(SOUND_HAMMER_PICKUP_FREQ);
            is_with_hammer = 1;
        }
    }
    // Resetting the input queue vars for next time
    input_queue_received = 0;
    input_queue_tail = 0;
}

// Handles the updating of stuff and shit
void updater(){

    int i;
    int j;
    // The recieved msg
    int received_msg;
    // Holds if the user pressed enter or not
    int menu_result = 0;

    while (TRUE){
        received_msg = receive();

        // if we are in game and the game is ready to be played
        if (gameState == InGame && game_init){
            // Handle input from the player
            updater_handle_player_input();
                
            // Refill the display draft with the game map
            refill_display_draft(map_1, 12);
            // Insert the ladders into the draft
            insert_ladders_to_map(game_level);

            // Check and handle that the player is inside the screen
            updater_check_is_player_in_screen_boundries();
            
            // Is is time to apply gravity ?
            updater_gravity_timer();

            // for debug
            if (spawn_barrel_timer > 0){
                display_draft[0][4] = (spawn_barrel_timer / 100 % 10) + '0';
                display_draft[0][5] = (spawn_barrel_timer / 10 % 10) + '0';
                display_draft[0][6] = (spawn_barrel_timer % 10) + '0';
            }

            // Is it time to spawn a new (normal) barrel
            updater_spawn_normal_barrel_timer();
            // Is it time to spawn a new (falling) barrel
            updater_spawn_falling_barrel_timer();

            // Move the barrels
            move_barrels();

            // Check for collisions with barrels
            updater_player_barrels_collision();

            // for debug
            display_draft[0][0] = (barrels_array_index / 10 % 10) + '0';
            display_draft[0][1] = (barrels_array_index % 10) + '0';

            /* Inserts the needed models to the display draft */
            updater_insert_models_to_display_draft();

            /* Inserts the 'HUD' (Heads up display) text */
            insert_clock_to_draft();
            inesrt_player_life_to_draft();
            insert_player_score_to_draft(0, 11, 15);

            // Saves the changes of the display draft to the display
            save_display_draft();
        }else if (gameState == InMenu){
            // If we are in the menu

            refill_display_draft(menu, 6);

            // Handle the input from the user
            menu_result = updater_handle_menu_input(2);

            // if the user pressed enter (ENTER -> menu_result = 1)
            handle_menu_entered(menu_result, InGame);

            // Inserts the menus with the effect of hovering above the selected one
            if (menu_index == 0){
                insert_text_to_center_of_draft("Start Game", 10, 13, 1, 0);
                insert_text_to_center_of_draft("Exit", 4, 15, 7, 0);
            }else {
                insert_text_to_center_of_draft("Start Game", 10, 13, 7, 0);
                insert_text_to_center_of_draft("Exit", 4, 15, 1, 0);
            }

            save_display_draft();
        }else if (gameState == InGameOver){
            // if we are in the game over menu
            refill_display_draft(menu_game_over, 4);
            
            // Handle the input from the user
            menu_result = updater_handle_menu_input(2);

            // if the user pressed enter (ENTER -> menu_result = 1)
            handle_menu_entered(menu_result, InMenu);

            // Inserts the points the player scored
            insert_text_to_center_of_draft("Points:", 7, 11, 15, -6);
            insert_player_score_to_draft(11, center_text_in_screen(5) - 5, 15);

            // Inserts the menus with the effect of hovering above the selected one
            if (menu_index == 0){
                insert_text_to_center_of_draft("Main Menu", 9, 13, 1, 0);
                insert_text_to_center_of_draft("Exit", 4, 15, 7, 0);
            }else {
                insert_text_to_center_of_draft("Main Menu", 9, 13, 7, 0);
                insert_text_to_center_of_draft("Exit", 4, 15, 1, 0);
            }

            save_display_draft();
        }else if (gameState == InGameWon){
            // if we are in the game won menu
            refill_display_draft(menu_game_won, 14);
            
            // Handle the input from the user
            menu_result = updater_handle_menu_input(2);

            // if the user pressed enter (ENTER -> menu_result = 1)
            handle_menu_entered(menu_result, InMenu);

            // Inserts the points the player scored
            insert_text_to_center_of_draft("Points:", 7, 11, 15, -6);
            insert_player_score_to_draft(11, center_text_in_screen(5) - 5, 15);

            // Inserts the menus with the effect of hovering above the selected one
            if (menu_index == 0){
                insert_text_to_center_of_draft("Main Menu", 9, 13, 1, 0);
                insert_text_to_center_of_draft("Exit", 4, 15, 7, 0);
            }else {
                insert_text_to_center_of_draft("Main Menu", 9, 13, 7, 0);
                insert_text_to_center_of_draft("Exit", 4, 15, 1, 0);
            }

            save_display_draft();
        }
    }
}

// Handles the input from the user
void receiver(){
    while (TRUE){
        // Getting the input from the routine
        input_queue[input_queue_tail] = receive();
        // Next cell in queue
        input_queue_tail++;
        // Making sure we dont overflow
        if (input_queue_tail >= MAX_SAVED_INPUT) input_queue_tail = 0;
        
        // Saving how many recevied input we got saved
        input_queue_received++;
    }
}

// Init the game for a new game
void init_game(){
    // Making sure the game is locked
    game_init = 0;
    // Load the first level
    game_level = 1;
    // Init all the game vars
    init_vars();
}

// When the game is won we need to init some stuff
void game_won_init(){
    // Add points for winning
    add_score_points(POINTS_GAME_WON);
    // Lock the game
    game_init = 0;
    // Change the state of the game to Game Won menu
    change_game_state(InGameWon);
    // Play a sound for winning
    send_sound(SOUND_GAME_WON_FREQ);
}

// When the game is over we need to init some stuff
void game_over_init(){
    // Lock the game
    game_init = 0;
    // Change the state of the game to Game Over menu
    change_game_state(InGameOver);
    // Play a sound for losing
    send_sound(SOUND_GAME_OVER_FREQ);
}

// Manages different game things and thangs
void manager(){
    int last_min = 0;

    while (TRUE){
        receive();
        // Checks if the game state changed
        if (prev_game_state != gameState){
            // if the game state changed to in game (want to play)
            if (gameState == InGame){
                // we need to init the game
                init_game();
                last_min = 0;
            }
            prev_game_state = gameState;
        }

        // Check for this only if the player in playing the game
        // and only if the game is ready to be played
        if (gameState == InGame && game_init){
            // if the player ran out of lives
            // or if the clock got to 3 minutes
            if ((player_lives <= 0) || 
            (clock_minutes >= 3 && last_min != 3)){
                // Game over!
                game_over_init();
            }

            // if mario got to the princess
            if (mario_got_to_princess){
                mario_got_to_princess = 0;
                // if there are any more levels
                if (game_level + 1 <= 3){
                    // Init the level vars
                    init_vars_level();
                    // Add points for winning the level
                    add_score_points(POINTS_LEVEL_WON);
                    // Next LEVEL!!
                    game_level++;
                    // The clock is resetted to 0:0 so the last minute needs to be 0
                    last_min = 0;
                    send_sound(SOUND_NEW_LEVEL_FREQ);
                }else {
                    // Game won!
                    game_won_init();
                }
            }

            // if it's the second level and a minute has passed we need to speed up the barrel spawn
            if (game_level >= 2 && last_min != clock_minutes){
                spawn_barrel_speed_in_ticks /= 2;
            }

            // Saving the last minute
            if (last_min != clock_minutes){
                last_min = clock_minutes;
            }
        }

    }
}

// Starts all the processes of the game
void start_processes(){
    int up_pid, draw_pid, recv_pid, timer_pid, mang_pid, sou_pid;
    int i = 0;
    time_t t;
    srand((unsigned) time(&t));

    // The priority of the process as we think:
    // Top to bottom (top = most important)
    // Time handler - We want to update the time first, before anything else
    // Recevier - We want to get the input from the player as fast as we can
    // Updater + Manager - We want to update and manage all the different things that makes the game work
    // Drawer - After every thing we want to print to the screen (to give feedback to the player)
    // Sounder - Not really important, we waking him up every time we need to play a sound
    resume(timer_pid = create(time_handler, INITSTK, INITPRIO + 4, "KONG: TIME HANDLER", 0));
    resume(recv_pid = create(receiver, INITSTK, INITPRIO + 3, "KONG: RECEIVER", 0));
    resume(up_pid = create(updater, INITSTK, INITPRIO + 2, "KONG: UPDATER", 0));
    resume(mang_pid = create(manager, INITSTK, INITPRIO + 2, "KONG: MANAGER", 0));
    resume(draw_pid = create(drawer, INITSTK, INITPRIO + 1, "KONG: DRAWER", 0));
    resume(sou_pid = create(sounder, INITSTK, INITPRIO, "KONG: SOUNDER", 0));

    // Saving the pids of the process for global use
    receiver_pid = recv_pid;
    time_handler_pid = timer_pid;
    updater_pid = up_pid;
    drawer_pid = draw_pid;
    manager_pid = mang_pid;
    sounder_pid = sou_pid;

    // Schedules the drawer and updater and manager
    // The default cycle is 1, and all the cycles of the process are 0
    // Because we want to fastest calling, to make the game feel smoother
    // Because the clock routine checks every tick if the current point in the cycle
    // is equal to one of the schedules of one of the processes
    // if it is we are telling XINU to re-schedule the processes
    schedule(3, DEFAULT_CYCLE_LENGTH, draw_pid, CYCLE_DRAWER, up_pid, CYCLE_UPDATER, mang_pid, CYCLE_MANAGER);
}

xmain(){
    // Saves the color byte
    save_out_to_screen();

    // Changes routine #9 to ours
    set_int9();

    start_processes();

    return;
}
