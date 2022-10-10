#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <gba_video.h>

#include <string.h>

// The mode 3 framebuffer is at the start of VRAM
u16 * const framebuffer = (u16*)VRAM;

// Division and modulo tables used by hsv2rgb
u16 const h_div_6[32] = {
	0, 0, 0, 0, 0, 0, 1, 1,
	1, 1, 1, 1, 2, 2, 2, 2,
	2, 2, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 5, 5
};

u16 const h_mod_6[32] = {
	0, 1, 2, 3, 4, 5, 0, 1,
	2, 3, 4, 5, 0, 1, 2, 3,
	4, 5, 0, 1, 2, 3, 4, 5,
	0, 1, 2, 3, 4, 5, 0, 1
};

void plotPixel( u32 x, u32 y, u16 colour)
{
	framebuffer[y * SCREEN_WIDTH + x] = colour;
}

// Plot some number of colors to x y using memcpy
void plotColors( u32 x, u32 y, u16 *colors, u32 numColors)
{
	memcpy( framebuffer + y*SCREEN_WIDTH + x, colors, numColors * sizeof(u16));
}

u16 getPixel( u32 x, u32 y)
{
	return framebuffer[y * SCREEN_WIDTH + x];
}

// Convert 5-bit H, S, V to 5-bit R, G, B
u16* hsv2rbg(int h, int s, int v, u16 *rgb) {
	int r, g, b, f, p, q, t, h_region;
	
	if (s == 0) {
		r = g = b = v;
	}
	else {
		//h_region = Div(h, 6);
		h_region = h_div_6[h];

		//f = (h % 6) * 6;
		f = h_mod_6[h] * 6;

		p = (v * (31 - s)) >> 5;
		q = (v * (31 - ((s * f) >> 5))) >> 5; // (v * (32 - (s * f) / 32)) >> 5
		t = (v * (31 - ((s * (31 - f)) >> 5))) >> 5; // (v * (32 - (s * (32 - f)) / 32)) >> 5

		switch (h_region) {
			case 0: r = v; g = t; b = p; break;
			case 1: r = q; g = v; b = p; break;
			case 2: r = p; g = v; b = t; break;
			case 3: r = p; g = q; b = v; break;
			case 4: r = t; g = p; b = v; break;
			default: r = v; g = p; b = q; break;
		}
	}
	rgb[0] = r;// >> 3;
	rgb[1] = g;// >> 3;
	rgb[2] = b;// >> 3;
	return rgb;
}

int main(void) {
	// Set the video mode and enable background 2
	REG_DISPCNT = MODE_3 | BG2_ON;
	// Initialise libgba interrrupt dispatcher 
	irqInit();
	// Enable the vblank irq so we can use the system VBlankIntrWait.
	// libgba dispatcher will handle acknowledgement and setting flags for bios intrwait functions
	irqEnable(IRQ_VBLANK);

	// mode 2 is 16bit per pixel, 5 bits per component.
	// libgba provides RGB5 for readable colors 
	// 2^5 - 1 = 31

	u16* rgbS = (u16*)malloc(3 * sizeof(u16));

	int value = 0;
	int saturation = 0;

	while (1) {  // loop forever
		scanKeys();
		// keysHeld tells us which buttons are currently pressed.
		// We also have keysDown and keysUp which tell us which buttons changed state in this loop
		int pressed = keysHeld();

		if ((pressed & KEY_UP) && saturation < 31) {
			saturation++;
		}
		else if ((pressed & KEY_DOWN) && saturation > 0) {
			saturation--;
		}
		else if ((pressed & KEY_LEFT) && value > 0) {
			value--;
		}
		else if ((pressed & KEY_RIGHT) && value < 31) {
			value++;
		}

		VBlankIntrWait();

		int x32 = 0;

		// Iterate colors for one line of the screen
		// Increase by SCREEN_WIDTH>>5(SCREEN_WIDTH/32) + x32&0x1(avoids decimal approximation)
		// to avoid division
		for (u16 j = 0; j < SCREEN_WIDTH; j += ((SCREEN_WIDTH >> 5) + (x32 & 0x1))) {
			hsv2rbg(x32++, 31 - saturation, 31 - value, rgbS);

			u16 bounds = (SCREEN_WIDTH >> 5) + (x32 & 0x1);

			for (u16 k = 0; k < bounds; k++) {
				framebuffer[j + k] = RGB5(rgbS[0], rgbS[1], rgbS[2]);
			}
		}

		// Copy first row of colors to all other rows
		for (u16 i = 1; i < SCREEN_HEIGHT; i++) {
			memcpy(framebuffer + i*SCREEN_WIDTH, framebuffer, SCREEN_WIDTH * sizeof(u16));
		}
	}
}
