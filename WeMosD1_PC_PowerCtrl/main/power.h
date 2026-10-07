#ifndef POWER_H
#define POWER_H

/**
 * @brief Generate a short pulse on the power control GPIO (simulates a brief
 *        power-button press to wake / power on the PC).
 */
void power_press_short(void);

/**
 * @brief Generate a long pulse on the power control GPIO (simulates a held
 *        power-button press to force shutdown on most motherboards > 4s).
 */
void power_press_long(void);

#endif // POWER_H
