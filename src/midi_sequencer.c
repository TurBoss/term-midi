/* HUNTER WHYTE 2021
 PART OF working title - A COLLECTION OF TEXT BASED MUSIC PRODUCTION TOOLS
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#include "agon/vdp_vdu.h"
#include "acurses.h"


#define SOKOL_IMPL
#include "sokol_time.h"

void UART1_INIT(void);
void UART1_SEND(uint8_t);
void UART1_WRITE(uint8_t*);
int UART1_READ(void);

/*
 TODO:
 investigate best way to advance step without hogging the entirety of cpu (maybe look at orca?)
 add interpolation to play notes in between steps
 add step variable to note type? or create a 2d array with all notes - this seems worse but who knows
 so then every "step" we iterate through all the notes in the note array and check if any of them note.step == current.step
 but then to play we dont just play at current timestamp we add the note.offset (difference in time between quantized step and when note is played)
 so we pass the note_t variable to the note.play this also includes note.duration so we can queue up the note on and note off midi commands ahead of time
 for this to work we may have to pass the portmidi timestamp into the play function as well, so that the offset is from the actual quantized step
 fix playing notes right next to eachother and also fix the double playing notes (happens because it is getting played by the stepper as well as the manual play on input)
 add set tempo with variable
 set loop length with variable
 control tempo and loop length with keypad
 add stop start with space bar
 figure out better way to display loop (notes get input into the middle of the timeline?) ableton style scrolling? bar moves but notes stationary?
 consider adding bars to show timeline
 consider adding dividers inbetween notes to demark? alternatively make the bars of the timeline have alternating pattern ie
 consider adding colours?
 fix variable types
 */

/* TYPES */
typedef struct {
	uint16_t step;
	uint16_t offset;
	uint16_t duration;
	uint8_t number;
} note_t;

/* FUNCTION PROTOTYPES */
void play(note_t note);

void note_on(int note);
void note_off(int note);
void set(int y, int x, char c, int status);
void draw(int offset);
void draw_grid(int offset);
void step(void);
void remove_note(int index);
void add_note(int row);
int get_note(int in);
void port_init(void);
void midi_send(uint8_t *data);
int midi_recv();

/* ENUMS */
enum notes {
	C = 0x00,
	C_ = 0x01,
	D = 0x02,
	D_ = 0x03,
	E = 0x04,
	F = 0x05,
	F_ = 0x06,
	G = 0x07,
	G_ = 0x08,
	A = 0x09,
	A_ = 0x0A,
	B = 0x0B,
	C1 = 0x0C,
	C_1 = 0x0D,
	D1 = 0x0E,
	D_1 = 0x0F,
	E1 = 0x10,
	F1 = 0x11,
	F_1 = 0x12,
	G1 = 0x13,
	G_1 = 0x14,
	A1 = 0x15,
	A_1 = 0x16,
	B1 = 0x17,
	C2 = 0x18,
};

/* MACROS */
#define MIDI_OFFSET    48 // offset to use when translating keyboard to midi note number
#define NOTE_OFF     0x80 // midi status code for note off
#define NOTE_ON      0x90 // midi status code for note on
#define CHANNEL      0x00 // midi channel to use TODO: make this user selectable
#define NOTE_DURATION 100 // standard minimum note duration TODO: dont use this
#define COL_OFFSET     13 // offset to start notes on (width of keyboard)
#define STEP_LENGTH    10 // duration of one step in ms (optimzing for appearance and effeciency of step checks)
#define NUM_STEPS      64 // number of steps in a loop TODO: make this user selectable
#define MAX_NOTES     256 // maximum number of notes allowed TODO: add command line option to change this from default

#define STEPS_PER_BEAT  16 // number of steps per beat (for 4/4 time signature)

/* GLOBALS */
note_t note_queue[MAX_NOTES] = { };
int num_notes = 0;   // keep track of number of notes in the loop
int keyboard_offset; // vertical/row offset to draw the keyboard and corresponding notes,
int32_t cur_step_time;
uint8_t cur_step;
int bpm = 120; // Default BPM value


/* FUNCTIONS */
int main() {
	stm_setup();
	port_init();

	// vdp_keyboard_cotrol(5, 100, 1);

	// init ncurses screen

	initscr();

	noecho();
	curs_set(0);

	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE); // turns getch into a non-blocking function
	timeout(10); // getch returns -1 if no data within 10ms

	keyboard_offset = (LINES / 2) - 12; // put keyboard roughly in middle of terminal
	draw(keyboard_offset);
	draw_grid(keyboard_offset);

	unsigned long long step_length;
	cur_step_time = stm_now();
	cur_step = 0;
	int32_t note_time;
	uint8_t note_pressed = 0;
	int input;
	int note_num;
	bool playing = false;

	mvprintw(0, 30, "BPM: %03d", bpm);

	while (1) {
		step_length = (600 / (bpm * STEPS_PER_BEAT));
		if (playing) {
			// if enough time has passed, increment step
			if ((stm_now() - cur_step_time) > step_length) {
				cur_step_time = stm_now();
				step();
				refresh();
			}
		}
		// get user input
		input = wgetch(0);

		if (input != -1) { // if user input received

			// exit keyboard mode
			if (input == 'k') {
				while (num_notes)
					remove_note(0);
				endwin();
				// TODO: stop all notes
				return 0;
			}

			// clear notes
			if (input == 'l') {
				while (num_notes)
					remove_note(0);
			}
			// toggle play/stop
			if (input == ' ') {
				playing = !playing;
			}

			// increase BPM
			if (input == '+') {
				bpm += 1;
				if (bpm > 255) bpm = 255;
				mvprintw(0, 30, "BPM: %03d", bpm);
			}

			// decrease BPM
			if (input == '-') {
				bpm -= 1;
				if (bpm < 0) bpm = 0; // Prevent negative BPM
				mvprintw(0, 30, "BPM: %03d", bpm);
			}

			// convert input to note number and play note
			note_num = get_note(input); // on note down

			if (note_num == -1)
				continue;

			if (!note_pressed) {

				note_on(note_num);
				add_note(note_num);
				note_pressed = 1;
				note_time = stm_now();

				mvprintw(0, 0, "note down           %02x", note_num);
			}
			else if (note_num != note_queue[num_notes - 1].number) { // note played while last input was a note, check if it is a new note
				note_queue[num_notes - 1].duration = stm_now() - note_time; // add the note since we now know the end duration
				note_off(note_queue[num_notes - 1].number);
				note_pressed = 0;
				mvprintw(0, 0, "note up       %03d", note_queue[num_notes - 1].duration);

				note_on(note_num);
				add_note(note_num);
				note_pressed = 1;
				note_time = stm_now();

				mvprintw(0, 0, "note down           %02x", note_num);
			}
		}
		else if (note_pressed) { // if there are any notes pressed, this is on note up

			note_queue[num_notes - 1].duration = stm_now() - note_time; // add the note since we now know the end duration
			note_off(note_queue[num_notes - 1].number);
			note_pressed = 0;
			mvprintw(0, 0, "note up       %03d", note_queue[num_notes - 1].duration);
		}
	}
	clear();
	endwin();
	return 0;
}

uint8_t note_cmd[3];
uint8_t note_on_send;
uint8_t note_off_send;

uint8_t note_to_send;
uint8_t note_vel_send;

void play(note_t note) {
	// note on channel 1, note number, velocity = 0xFF

	note_on_send = NOTE_ON | CHANNEL;
	note_to_send = note.number + MIDI_OFFSET;
	note_vel_send = 0x7F;

	note_cmd[0] = note_on_send;
	note_cmd[1] = note_to_send;
	note_cmd[2] = note_vel_send;

	midi_send(note_cmd);

	// note off channel 1, note number, velocity = 0xFF

	note_off_send = NOTE_OFF + CHANNEL;
	note_to_send = note.number + MIDI_OFFSET;
	note_vel_send = 0x00;

	note_cmd[0] = note_off_send;
	note_cmd[1] = note_to_send;
	note_cmd[2] = note_vel_send;

	midi_send(note_cmd);
}


// turn note on given note number
void note_on(int val) {
    // note on channel 1, note number, velocity = 0xFF
    note_on_send = NOTE_ON | CHANNEL;
    note_to_send = val + MIDI_OFFSET;
    note_vel_send = 0x7f;

	note_cmd[0] = note_on_send;
	note_cmd[1] = note_to_send;
	note_cmd[2] = note_vel_send;

	midi_send(note_cmd);
}

// turn note off given note number
void note_off(int val) {
    // note off channel 1, note number, velocity = 0xFF
    note_off_send = NOTE_OFF | CHANNEL;
    note_to_send = val + MIDI_OFFSET;
    note_vel_send = 0x00;

	note_cmd[0] = note_on_send;
	note_cmd[1] = note_to_send;
	note_cmd[2] = note_vel_send;

	midi_send(note_cmd);
}


// increment global step
void step(void) {
	// erase scroll bar
	for (int j = 0; j < 27; j++) // 27 is test value for the total bar height, maybe change
		mvaddch(keyboard_offset + j, cur_step + COL_OFFSET, ' ');

	for (int i = 0; i < num_notes; i++) {
		// check if playing any notes this step
		if (note_queue[i].step == cur_step) {
			play(note_queue[i]);
			mvaddch(note_queue[i].number + keyboard_offset + 1,	note_queue[i].step + COL_OFFSET, '#'); // add to screen
		}
	}

	if (cur_step == NUM_STEPS)
		cur_step = 0;
	else
		cur_step++;

	// draw scroll bar
	for (int j = 0; j < 27; j++) {// 27 is test value for the total bar height, maybe change
		mvaddch(keyboard_offset + j, cur_step + COL_OFFSET, '|');
	}

	draw_grid(keyboard_offset);
}


// Add note to queue
void add_note(int note_num) {
	note_t n = { };
	n.step = cur_step;
	n.offset = stm_now() - cur_step_time;
	n.duration = NOTE_DURATION; // default duration, this gets changed if we read that the note is longer
	n.number = note_num;

	mvaddch(n.number + keyboard_offset , n.step + COL_OFFSET, '#'); // add to screen
	if (num_notes == MAX_NOTES) // if we are at max replace the oldest note
	{
		remove_note(0);
		note_queue[num_notes] = n;
	} else {
		note_queue[num_notes] = n;
		num_notes += 1;
	}
}


// remove note from queue
void remove_note(int index) {
	note_off(note_queue[index].number); // stop note in case its still playing TODO:add check if note is playing so that we arent sending errant note off messages
	mvaddch(note_queue[index].number + keyboard_offset + 1,
			note_queue[index].step + COL_OFFSET, ' '); // delete from screen

	while (index < num_notes) // backfill the space left by note removed from array
	{
		note_queue[index] = note_queue[index + 1];
		index++;
	}

	num_notes--; // decrement number of total notes
	refresh(); // refresh display to show the removed note
}


// draw the keyboard interface
void draw(int offset) {
	clear();

	// mvprintw(0, 30, "BPM: %d", bpm); // Display BPM
	mvprintw(2, 0, "press \'space\' to play, \'k\' back to MOS");

	mvprintw(offset, 0, "_________ ");
	mvprintw(offset + 1, 0, " z  _____|");
	mvprintw(offset + 2, 0, "___|__s__|");
	mvprintw(offset + 3, 0, " x  _____|");
	mvprintw(offset + 4, 0, "___|__d__|");
	mvprintw(offset + 5, 0, "_c_______|");
	mvprintw(offset + 6, 0, " v  _____|");
	mvprintw(offset + 7, 0, "___|__g__|");
	mvprintw(offset + 8, 0, " b  _____|");
	mvprintw(offset + 9, 0, "___|__h__|");
	mvprintw(offset + 10, 0, " q  _____|");
	mvprintw(offset + 11, 0, "___|__2__|");
	mvprintw(offset + 12, 0, "_w_______|");
	mvprintw(offset + 13, 0, " e  _____|");
	mvprintw(offset + 14, 0, "___|__4__|");
	mvprintw(offset + 15, 0, " r  _____|");
	mvprintw(offset + 16, 0, "___|__5__|");
	mvprintw(offset + 17, 0, "_t_______|");
	mvprintw(offset + 18, 0, " y  _____|");
	mvprintw(offset + 19, 0, "___|__7__|");
	mvprintw(offset + 20, 0, " u  _____|");
	mvprintw(offset + 21, 0, "___|__8__|");
	mvprintw(offset + 22, 0, " i  _____|");
	mvprintw(offset + 23, 0, "___|__9__|");
	mvprintw(offset + 24, 0, "_o_______|");
	mvprintw(offset + 25, 0, "_p_______|");

	refresh();
}

void draw_grid(int offset) {

	for (int i = 0; i < 28; i+=2) {
		mvprintw(offset + i, COL_OFFSET + 0, "|");
	}
	for (int i = 0; i < 28; i+=2) {
		mvprintw(offset + i, COL_OFFSET + 16, "|");
	}
	for (int i = 0; i < 28; i+=2) {
		mvprintw(offset + i, COL_OFFSET + 32, "|");
	}
	for (int i = 0; i < 28; i+=2) {
		mvprintw(offset + i, COL_OFFSET + 48, "|");
	}
	for (int i = 0; i < 28; i+=2) {
		mvprintw(offset + i, COL_OFFSET + 64, "|");
	}
	refresh();
}

// get corresponding note number for keyboard input
int get_note(int in) {
	switch (in) {
	case 'z':
		return (C);
	case 'x':
		return (D);
	case 'c':
		return (E);
	case 'v':
		return (F);
	case 'b':
		return (G);
	case 'q':
		return (A);
	case 'w':
		return (B);
	case 'e':
		return (C1);
	case 'r':
		return (D1);
	case 't':
		return (E1);
	case 'y':
		return (F1);
	case 'u':
		return (G1);
	case 'i':
		return (A1);
	case 'o':
		return (B1);
	case 'p':
		return (C2);
	case 's':
		return (C_);
	case 'd':
		return (D_);
	case 'g':
		return (F_);
	case 'h':
		return (G_);
	case '2':
		return (A_);
	case '4':
		return (C_1);
	case '5':
		return (D_1);
	case '7':
		return (F_1);
	case '8':
		return (G_1);
	case '9':
		return (A_1);
	default:
		return -1; // not valid input
	}
}


// UART1 initialization and configuration
void port_init(void) {
	UART1_INIT();
}

// Send a byte via UART1
void midi_send(uint8_t data[3]) {

    mvprintw(0, 40, "SEND CMD:  0x%02x 0x%02x 0x%02x", data[0], data[1], data[2]);
    UART1_WRITE(data);
}

// Read a byte via UART1
int midi_recv() {
	UART1_READ();
	return 0;
}
