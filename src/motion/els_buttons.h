#pragma once

#include <stdint.h>

class ElsButtons {
public:
	static void init();
	static void update();

private:
	static void handleShortPress(int8_t dir);
	static void startJog(int8_t dir);
	static void stopJog(int8_t dir);
};
