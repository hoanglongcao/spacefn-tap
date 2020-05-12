 /*
 * spacefn-evdev.c
 * James Laird-Wah (abrasive) 2018
 * This code is in the public domain.
 * Modified by Hoang-Long Cao with tapping for dedicated arrow keys
 */

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

enum {
    NONE,
    FN1,
    FN2,
    FN3,
    FN4,
    FN5,
} mode = NONE;

#define KEY_FN1 KEY_SPACE
#define KEY_FN2 KEY_RIGHTSHIFT
#define KEY_FN3 KEY_RIGHTCTRL
#define KEY_FN4 KEY_RIGHTALT
#define KEY_FN5 KEY_COMPOSE


// Global device handles {{{1
struct libevdev *idev;
struct libevdev_uinput *odev;
int fd;
// Ordered unique key buffer {{{1
#define MAX_BUFFER 8
unsigned int buffer[MAX_BUFFER];
unsigned int n_buffer = 0;


static int buffer_contains(unsigned int code) {
    for (int i=0; i<n_buffer; i++)
        if (buffer[i] == code)
            return 1;

    return 0;
}

static int buffer_remove(unsigned int code) {
    for (int i=0; i<n_buffer; i++)
        if (buffer[i] == code) {
            memcpy(&buffer[i], &buffer[i+1], (n_buffer - i - 1) * sizeof(*buffer));
            n_buffer--;
            return 1;
        }
    return 0;
}

static void buffer_clean(void) {
    for (int i=0; i<n_buffer; i++)
        memcpy(&buffer[i], &buffer[i+1], (n_buffer - i - 1) * sizeof(*buffer));
    return;
}

static int buffer_append(unsigned int code) {
    if (n_buffer >= MAX_BUFFER)
        return 1;
    buffer[n_buffer++] = code;
    return 0;
}


// Key mapping
unsigned int key_map(unsigned int code) {
    if (mode == FN1){
        switch (code) {
            case KEY_Q:
                buffer_clean();
                exit(0);
            // Navigation
            case KEY_J:
                return KEY_LEFT;
            case KEY_K:
                return KEY_DOWN;
            case KEY_I:
                return KEY_UP;
            case KEY_L:
                return KEY_RIGHT;

            case KEY_P:
            case KEY_RIGHTALT:
                return KEY_HOME;

            case KEY_SEMICOLON:
            case KEY_RIGHTCTRL:
                return KEY_END;

            case KEY_H:
            case KEY_COMPOSE:
            case KEY_APOSTROPHE:
            case KEY_DOWN:
                return KEY_PAGEDOWN;
            
            case KEY_Y:
            case KEY_LEFTBRACE:
            case KEY_RIGHTSHIFT:
            case KEY_UP:
                return KEY_PAGEUP;        
            
            // Function keys
            case KEY_1:
                return KEY_F1;
            case KEY_2:
                return KEY_F2;
            case KEY_3:
                return KEY_F3;
            case KEY_4:
                return KEY_F4;
            case KEY_5:
                return KEY_F5;
            case KEY_6:
                return KEY_F6;
            case KEY_7:
                return KEY_F7;
            case KEY_8:
                return KEY_F8;
            case KEY_9:
                return KEY_F9;
            case KEY_0:
                return KEY_F10;
            case KEY_MINUS: 
                return KEY_F11;
            case KEY_EQUAL:
                return KEY_F12;

            // Volume control
            case KEY_COMMA:
                return KEY_VOLUMEDOWN;
            case KEY_DOT: 
                return KEY_VOLUMEUP;
            case KEY_SLASH:
                return KEY_MUTE;

            // Other decicated keys
            case KEY_W:
                return KEY_SYSRQ; //PrintScreen
            case KEY_R:
                return KEY_INSERT;

            // ESC to `~
            case KEY_ESC:
                return KEY_GRAVE;
            // Delete
            case KEY_DELETE:
                return KEY_BACKSLASH;

            case KEY_M:
                return KEY_DELETE;
            case KEY_N:
                return KEY_BACKSPACE;


            // Other shortcuts
            case KEY_D:// Show desktop
                system("wmctrl -k on");
                return 0;  
            case KEY_E: // File manager
                return KEY_YEN;
            
            case KEY_B:
                return KEY_FN1;

            
        }
    }

    else if (mode == FN3){
        switch (code) {
            case KEY_RIGHTSHIFT:
                return KEY_PAGEUP;
            case KEY_COMPOSE:
                return KEY_PAGEDOWN;
        }   
    }
    return 0;
}

// Key remapping 
int keyremap(unsigned int code){
    switch (code){
        case KEY_GRAVE: // Comment out this case if you have a real 60% keyboard. I made this for my TKL
            return code = KEY_ESC; break;
        case KEY_BACKSLASH:
            return code = KEY_DELETE; break;
        case KEY_RIGHTSHIFT:
            return code = KEY_UP; break;
        case KEY_COMPOSE:
            return code = KEY_DOWN; break;
        case KEY_RIGHTCTRL:
            return code = KEY_RIGHT; break;
        case KEY_RIGHTALT:
            return code = KEY_LEFT; break;
    }
}

// Blacklist keys for which I have a mapping, to try and train myself out of using them
int blacklist(unsigned int code) {
    /*
    switch (code) {
        case KEY_UP:
        case KEY_DOWN:
        case KEY_RIGHT:
        case KEY_LEFT:
        case KEY_INSERT:
        case KEY_DELETE:
        case KEY_HOME:
        case KEY_END:
        case KEY_PAGEUP:
        case KEY_PAGEDOWN:
        case KEY_F1:
        case KEY_F2:
        case KEY_F3:
        case KEY_F4:
        case KEY_F5:
        case KEY_F6:
        case KEY_F7:
        case KEY_F8:
        case KEY_F9:
        case KEY_F10:
        case KEY_F11: 
        case KEY_F12:
            return 1;
    }
    */
    return 0;
}




// Key I/O functions {{{1
// output {{{2
#define V_RELEASE 0
#define V_PRESS 1
#define V_REPEAT 2
static void send_key(unsigned int code, int value) {
    libevdev_uinput_write_event(odev, EV_KEY, code, value);
    libevdev_uinput_write_event(odev, EV_SYN, SYN_REPORT, 0);
}

static void send_press(unsigned int code) {
    send_key(code, 1);
}

static void send_release(unsigned int code) {
    send_key(code, 0);
}

static void send_repeat(unsigned int code) {
    send_key(code, 2);
}

// input {{{2
static int read_one_key(struct input_event *ev) {
    int err = libevdev_next_event(idev, LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING, ev);
    if (err) {
        fprintf(stderr, "Failed: (%d) %s\n", -err, strerror(err));
        exit(1);
    }

    if (ev->type != EV_KEY) {
        libevdev_uinput_write_event(odev, ev->type, ev->code, ev->value);
        return -1;
    }

    if (blacklist(ev->code))
        return -1;
    return 0;
}

// State functions {{{1
enum {
    IDLE,
    DECIDE,
    SHIFT,
} state = IDLE;

static void state_idle(void) {  // {{{2
    struct input_event ev;
    mode = NONE;
    
    for (;;) {
        while (read_one_key(&ev));  
 
        if (ev.code == KEY_FN1 && ev.value == V_PRESS) {
            state = DECIDE;
            mode = FN1;
            return;
        }

        else if (ev.code == KEY_FN2 && ev.value == V_PRESS) {
            state = DECIDE;
            mode = FN2;
            return;
        }

        else if (ev.code == KEY_FN3&& ev.value == V_PRESS) {
            state = DECIDE;
            mode = FN3;
            return;
        }

        else if (ev.code == KEY_FN4&& ev.value == V_PRESS) {
            state = DECIDE;

            mode = FN4;
            return;
        }

        else if (ev.code == KEY_FN5&& ev.value == V_PRESS) {
            state = DECIDE;

            mode = FN5;
            return;
        }


        // Switch Grave to ESC, BACKSLASH to DEL
        ev.code = keyremap(ev.code);

        send_key(ev.code, ev.value);

    }
}

static void state_decide(void) {    // {{{2
    n_buffer = 0;
    struct input_event ev;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 120000;
    if (mode == FN1) timeout.tv_usec = 250000;
    else if (mode == FN5) timeout.tv_usec = 500000;
    fd_set set;
    FD_ZERO(&set);
    while (timeout.tv_usec >= 0) {
        FD_SET(fd, &set);
        int nfds = select(fd+1, &set, NULL, NULL, &timeout);
        if (!nfds)
            break;

        while (read_one_key(&ev));

        if (ev.value == V_PRESS) {
            buffer_append(ev.code);
            continue;
        }

        if (mode == FN1){
            if (ev.code == KEY_FN1 && ev.value == V_RELEASE) {
                // Switch Grave to ESC, BACKSLASH to DEL
                // Outside this loop key are original
                ev.code = keyremap(ev.code);

                send_key(ev.code, V_PRESS);
                send_key(ev.code, V_RELEASE);
                        
                for (int i=0; i<n_buffer; i++)
                    send_key(buffer[i], V_PRESS);
                state = IDLE;   
                return;
            }
        }

        else if (mode == FN2){
            if (ev.code == KEY_FN2 && ev.value == V_RELEASE) {
                // Switch Grave to ESC, BACKSLASH to DEL
                // Outside this loop key are original
                ev.code = keyremap(ev.code);

                send_key(ev.code, V_PRESS);
                send_key(ev.code, V_RELEASE);
                        
                for (int i=0; i<n_buffer; i++)
                    send_key(buffer[i], V_PRESS);
                state = IDLE;   
                return;
            }
        }
        
        else if (mode == FN3){
            if (ev.code == KEY_FN3 && ev.value == V_RELEASE) {
                // Switch Grave to ESC, BACKSLASH to DEL
                // Outside this loop key are original
                ev.code = keyremap(ev.code);

                send_key(ev.code, V_PRESS);
                send_key(ev.code, V_RELEASE);
                        
                for (int i=0; i<n_buffer; i++)
                    send_key(buffer[i], V_PRESS);
                state = IDLE;   
                return;
            }
        }

        else if (mode == FN4){
            if (ev.code == KEY_FN4 && ev.value == V_RELEASE) {
                // Switch Grave to ESC, BACKSLASH to DEL
                // Outside this loop key are original
                ev.code = keyremap(ev.code);

                send_key(ev.code, V_PRESS);
                send_key(ev.code, V_RELEASE);
                        
                for (int i=0; i<n_buffer; i++)
                    send_key(buffer[i], V_PRESS);
                state = IDLE;   
                return;
            }
        }
        else if (mode == FN5){
            if (ev.code == KEY_FN5 && ev.value == V_RELEASE) {
                // Switch Grave to ESC, BACKSLASH to DEL
                // Outside this loop key are original
                ev.code = keyremap(ev.code);

                send_key(ev.code, V_PRESS);
                send_key(ev.code, V_RELEASE);
                        
                for (int i=0; i<n_buffer; i++)
                    send_key(buffer[i], V_PRESS);
                state = IDLE;   
                return;
            }
        }

        /*

        if (ev.code == KEY_FN1 && ev.value == V_RELEASE) {
            send_key(KEY_FN1, V_PRESS);
            send_key(KEY_FN1, V_RELEASE);
            for (int i=0; i<n_buffer; i++)
                send_key(buffer[i], V_PRESS);
            state = IDLE;
            return;
        }
        */

        if (ev.value == V_RELEASE && !buffer_contains(ev.code)) {
            send_key(ev.code, ev.value);        
            continue;
        }

        if (ev.value == V_RELEASE && buffer_remove(ev.code)) {
            unsigned int code = key_map(ev.code);
            if (mode == FN2) send_key(KEY_LEFTSHIFT, V_PRESS);
            else if (mode == FN3) send_key(KEY_LEFTCTRL, V_PRESS);
            else if (mode == FN4) send_key(KEY_LEFTALT, V_PRESS);
            else if (mode == FN5) send_key(KEY_COMPOSE, V_PRESS);
            send_key(code, V_PRESS);
            send_key(code, V_RELEASE);
            if (mode == FN2) send_key(KEY_LEFTSHIFT, V_RELEASE);
            else if (mode == FN3) send_key(KEY_LEFTCTRL, V_RELEASE);
            else if (mode == FN4) send_key(KEY_LEFTALT, V_RELEASE);
            else if (mode == FN4) send_key(KEY_COMPOSE, V_RELEASE);
            state = SHIFT;
            return;
        }
    }

    printf("timed out\n");
    for (int i=0; i<n_buffer; i++) {
        unsigned int code = key_map(buffer[i]);
        if (!code)
            code = buffer[i];
        send_key(code, V_PRESS);
    }
    state = SHIFT;
}

static void state_shift(void) {
    n_buffer = 0;
    struct input_event ev;  
    
    if (mode == FN1){
        for (;;) {
            while (read_one_key(&ev));

            if (ev.code== KEY_FN1&& ev.value == V_RELEASE) {
                for (int i=0; i<n_buffer; i++)
                    send_key(buffer[i], V_RELEASE);
                state = IDLE;
                return;
            }
            if (ev.code== KEY_FN1)
                continue;

            unsigned int code = key_map(ev.code);
            
            if (code) {
                if (ev.value == V_PRESS)
                    buffer_append(code);
                else if (ev.value == V_RELEASE)
                    buffer_remove(code);    
                
                send_key(code, ev.value);
                
            } else {
                send_key(ev.code, ev.value);
            }
            
        }
    }

    else if (mode == FN2){
        unsigned int pressed = 0;
        for (;;) {
            
            while (read_one_key(&ev));

            if (ev.code== KEY_FN2&& ev.value == V_RELEASE) {
                for (int i=0; i<n_buffer; i++)
                    send_key(buffer[i], V_RELEASE);
                state = IDLE;
                if (pressed==0){
                    send_key(KEY_UP, V_PRESS);
                    send_key(KEY_UP, V_RELEASE);
                }
                return;
            }
            if (ev.code== KEY_FN2)
                continue;

            if (ev.code!=KEY_FN2) pressed = 1;
            
            unsigned int code = key_map(ev.code);
            
            send_key(KEY_LEFTSHIFT, V_PRESS);
            if (code) {
                if (ev.value == V_PRESS)
                    buffer_append(code);
                else if (ev.value == V_RELEASE)
                    buffer_remove(code);    
                
                send_key(code, ev.value);
                
            } else {
                send_key(ev.code, ev.value);
            }
            send_key(KEY_LEFTSHIFT, V_RELEASE);
            
        }
    }
    else if (mode == FN3){
        unsigned int pressed = 0;
        for (;;) {
            
            while (read_one_key(&ev));

            if (ev.code== KEY_FN3&& ev.value == V_RELEASE) {
                for (int i=0; i<n_buffer; i++)
                    send_key(buffer[i], V_RELEASE);
                state = IDLE;
                if (pressed==0){
                    send_key(KEY_RIGHT, V_PRESS);
                    send_key(KEY_RIGHT, V_RELEASE);
                }
                return;
            }
            if (ev.code== KEY_FN3)
                continue;
            if (ev.code!=KEY_FN3) pressed = 1;

            unsigned int code = key_map(ev.code);
            if ((code != KEY_PAGEUP)&&(code != KEY_PAGEDOWN)) send_key(KEY_LEFTCTRL, V_PRESS);
            if (code) {
                if (ev.value == V_PRESS)
                    buffer_append(code);
                else if (ev.value == V_RELEASE)
                    buffer_remove(code);    
                
                send_key(code, ev.value);
                
            } else {
                send_key(ev.code, ev.value);
            }
            if ((code != KEY_PAGEUP)&&(code!=KEY_PAGEDOWN)) send_key(KEY_LEFTCTRL, V_RELEASE);
            
        }
    }

    else if (mode == FN4){
        unsigned int pressed = 0;
        for (;;) {
            
            while (read_one_key(&ev));

            if (ev.code== KEY_FN4&& ev.value == V_RELEASE) {
                for (int i=0; i<n_buffer; i++)
                    send_key(buffer[i], V_RELEASE);
                state = IDLE;
                if (pressed==0){
                    send_key(KEY_LEFT, V_PRESS);
                    send_key(KEY_LEFT, V_RELEASE);
                }
                return;
            }
            if (ev.code== KEY_FN4)
                continue;
            if (ev.code!=KEY_FN4) pressed = 1;

            unsigned int code = key_map(ev.code);
            if ((code != KEY_PAGEUP)&&(code != KEY_PAGEDOWN)) send_key(KEY_LEFTALT, V_PRESS);
            if (code) {
                if (ev.value == V_PRESS)
                    buffer_append(code);
                else if (ev.value == V_RELEASE)
                    buffer_remove(code);    
                
                send_key(code, ev.value);
                
            } else {
                send_key(ev.code, ev.value);
            }
            if ((code != KEY_PAGEUP)&&(code!=KEY_PAGEDOWN)) send_key(KEY_LEFTALT, V_RELEASE);
            
        }
    }
    
    else if (mode == FN5){
        unsigned int pressed = 0;
        for (;;) {
            
            while (read_one_key(&ev));

            if (ev.code== KEY_FN5&& ev.value == V_RELEASE) {
                for (int i=0; i<n_buffer; i++)
                    send_key(buffer[i], V_RELEASE);
                state = IDLE;
                if (pressed==0){
                    send_key(KEY_COMPOSE, V_PRESS);
                    send_key(KEY_COMPOSE, V_RELEASE);
                }
                return;
            }
            if (ev.code== KEY_FN5)
                continue;
            if (ev.code!=KEY_FN5) pressed = 1;

            unsigned int code = key_map(ev.code);
            if ((code != KEY_PAGEUP)&&(code != KEY_PAGEDOWN)) send_key(KEY_COMPOSE, V_PRESS);
            if (code) {
                if (ev.value == V_PRESS)
                    buffer_append(code);
                else if (ev.value == V_RELEASE)
                    buffer_remove(code);    
                
                send_key(code, ev.value);
                
            } else {
                send_key(ev.code, ev.value);
            }
            if ((code != KEY_PAGEUP)&&(code!=KEY_PAGEDOWN)) send_key(KEY_COMPOSE, V_RELEASE);
            
        }
    }
}

static void run_state_machine(void) {       
    for (;;) {  
        printf("state %d mode %d\n", state, mode);
        switch (state) {
            case IDLE:
                state_idle();
                break;
            case DECIDE:
                state_decide();
                break;
            case SHIFT:
                state_shift();
                break;
        }
    }
}


int main(int argc, char **argv) {   // Main
    
    // Main    
    if (argc < 2) {
        printf("usage: %s /dev/input/...", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open input");
        return 1;
    }

    int err = libevdev_new_from_fd(fd, &idev);
    if (err) {
        fprintf(stderr, "Failed: (%d) %s\n", -err, strerror(err));
        return 1;
    }

    int uifd = open("/dev/uinput", O_RDWR);
    if (uifd < 0) {
        perror("open /dev/uinput");
        return 1;
    }

    err = libevdev_uinput_create_from_device(idev, uifd, &odev);
    if (err) {
        fprintf(stderr, "Failed: (%d) %s\n", -err, strerror(err));
        return 1;
    }    

    err = libevdev_grab(idev, LIBEVDEV_GRAB);
    if (err) {
        fprintf(stderr, "Failed: (%d) %s\n", -err, strerror(err));
        return 1;
    }

    run_state_machine();
}
