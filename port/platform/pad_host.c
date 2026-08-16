/*
 * PAD: GameCube controllers, served by SDL game controllers.
 *
 * SDL's game controller layer already normalises Xbox pads, the Razer Kishi,
 * DualSense and most Bluetooth controllers onto one button naming scheme, so
 * this maps that scheme onto the GameCube's rather than handling each device.
 *
 * The two layouts do not correspond exactly. The GameCube has one digital
 * shoulder button (Z) plus two analog triggers that also click; a modern pad
 * has two digital shoulders and two analog triggers that do not click. The
 * mapping below puts Z on the right shoulder, which is where a player reaching
 * for it expects to find it, and derives the trigger "clicks" from how far the
 * analog triggers are pressed.
 */

#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "Dolphin/pad.h"
#include "types.h"

/* An analog trigger counts as clicked past this fraction of its travel. The
 * GameCube's own click sat near the end of the throw. */
#define TRIGGER_CLICK_POINT 0xE0

/* Sticks are centred within this much of zero. SDL reports small non-zero
 * values at rest on most pads, and the game reads the stick every frame. */
#define STICK_DEADZONE 3200

static SDL_GameController* s_pads[PAD_MAX_CONTROLLERS];
static int s_initialised;

static void open_controllers(void)
{
	int i, slot = 0;
	int n       = SDL_NumJoysticks();

	for (i = 0; i < PAD_MAX_CONTROLLERS; i++) {
		if (s_pads[i]) {
			SDL_GameControllerClose(s_pads[i]);
			s_pads[i] = NULL;
		}
	}
	for (i = 0; i < n && slot < PAD_MAX_CONTROLLERS; i++) {
		if (SDL_IsGameController(i)) {
			s_pads[slot] = SDL_GameControllerOpen(i);
			if (s_pads[slot]) {
				slot++;
			}
		}
	}
}

BOOL PADInit(void)
{
	if (s_initialised) {
		return TRUE;
	}
	if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
		fprintf(stderr, "PADInit: %s\n", SDL_GetError());
		return FALSE;
	}
	SDL_GameControllerEventState(SDL_IGNORE); /* polled, not event-driven */
	open_controllers();
	s_initialised = 1;
	return TRUE;
}

/* SDL axis (-32768..32767) to GameCube stick (-128..127), with a deadzone. */
static s8 axis_to_stick(Sint16 v)
{
	int scaled;
	if (v > -STICK_DEADZONE && v < STICK_DEADZONE) {
		return 0;
	}
	scaled = v / 258; /* 32767/258 ~= 127 */
	if (scaled > 127) {
		scaled = 127;
	}
	if (scaled < -128) {
		scaled = -128;
	}
	return (s8)scaled;
}

static u8 axis_to_trigger(Sint16 v)
{
	int scaled = v / 128; /* SDL triggers report 0..32767 */
	if (scaled < 0) {
		scaled = 0;
	}
	if (scaled > 255) {
		scaled = 255;
	}
	return (u8)scaled;
}

u32 PADRead(PADStatus* status)
{
	u32 connected = 0;
	int i;

	if (!s_initialised) {
		PADInit();
	}
	SDL_GameControllerUpdate();

	for (i = 0; i < PAD_MAX_CONTROLLERS; i++) {
		SDL_GameController* c = s_pads[i];
		u16 buttons           = 0;

		memset(&status[i], 0, sizeof(status[i]));

		if (!c || !SDL_GameControllerGetAttached(c)) {
			status[i].err = PAD_ERR_NO_CONTROLLER;
			continue;
		}

#define HELD(btn) SDL_GameControllerGetButton(c, (btn))
		if (HELD(SDL_CONTROLLER_BUTTON_A)) {
			buttons |= PAD_BUTTON_A;
		}
		if (HELD(SDL_CONTROLLER_BUTTON_B)) {
			buttons |= PAD_BUTTON_B;
		}
		if (HELD(SDL_CONTROLLER_BUTTON_X)) {
			buttons |= PAD_BUTTON_X;
		}
		if (HELD(SDL_CONTROLLER_BUTTON_Y)) {
			buttons |= PAD_BUTTON_Y;
		}
		if (HELD(SDL_CONTROLLER_BUTTON_START)) {
			buttons |= PAD_BUTTON_START;
		}
		if (HELD(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
			buttons |= PAD_BUTTON_LEFT;
		}
		if (HELD(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
			buttons |= PAD_BUTTON_RIGHT;
		}
		if (HELD(SDL_CONTROLLER_BUTTON_DPAD_UP)) {
			buttons |= PAD_BUTTON_UP;
		}
		if (HELD(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
			buttons |= PAD_BUTTON_DOWN;
		}
		/* Z lives on the right shoulder; the left shoulder doubles as Z so
		 * either reach works. */
		if (HELD(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || HELD(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
			buttons |= PAD_TRIGGER_Z;
		}
#undef HELD

		status[i].stickX    = axis_to_stick(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX));
		/* GameCube reports stick up as positive; SDL reports down as positive. */
		status[i].stickY    = axis_to_stick(-SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY));
		status[i].substickX = axis_to_stick(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX));
		status[i].substickY = axis_to_stick(-SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY));

		status[i].triggerLeft  = axis_to_trigger(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
		status[i].triggerRight = axis_to_trigger(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));

		if (status[i].triggerLeft >= TRIGGER_CLICK_POINT) {
			buttons |= PAD_TRIGGER_L;
		}
		if (status[i].triggerRight >= TRIGGER_CLICK_POINT) {
			buttons |= PAD_TRIGGER_R;
		}

		status[i].button = buttons;
		status[i].err    = PAD_ERR_NONE;
		connected |= (1u << i);
	}

	return connected;
}

void PADClamp(PADStatus* status)
{
	/* Hardware clamped the raw stick reading into a circle and re-centred it.
	 * SDL has already given us calibrated, centred axes, and axis_to_stick
	 * applies the deadzone, so there is nothing further to do -- clamping again
	 * here would eat travel the player can feel. */
	(void)status;
}

void PADControlMotor(s32 chan, u32 command)
{
	SDL_GameController* c;

	if (chan < 0 || chan >= PAD_MAX_CONTROLLERS) {
		return;
	}
	c = s_pads[chan];
	if (!c) {
		return;
	}
	/* command 0 stops, 1 rumbles, 2 stops hard. Duration is open-ended because
	 * the game decides when to stop. */
	if (command == 1) {
		SDL_GameControllerRumble(c, 0xC000, 0xC000, 60000);
	} else {
		SDL_GameControllerRumble(c, 0, 0, 0);
	}
}

BOOL PADReset(u32 mask)
{
	(void)mask;
	if (s_initialised) {
		open_controllers(); /* pick up anything plugged in since */
	}
	return TRUE;
}

BOOL PADRecalibrate(u32 mask)
{
	(void)mask;
	return TRUE;
}

void PADSetSpec(u32 spec)
{
	/* Selects the controller reporting format on hardware. SDL gives us one
	 * normalised format regardless. */
	(void)spec;
}
