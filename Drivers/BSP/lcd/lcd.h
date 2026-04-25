/**
 ******************************************************************************
 * @file    lcd.h
 * @brief   LCD driver header file with RTOS support
 * @note    Fixed formatting and added English comments
 ******************************************************************************
 */

#ifndef __LCD_H
#define __LCD_H

#include "sys.h"
#include "os.h"
#include "stddef.h"

/* ==================== Mutex Definition ==================== */

extern OS_MUTEX LCD_Mutex;

/**
 * @brief  LCD mutex operation macros (simplified code)
 */
#define LCD_MUTEX_PEND()  do { \
    OS_ERR err; \
    OSMutexPend(&LCD_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err); \
} while(0)

#define LCD_MUTEX_POST()  do { \
    OS_ERR err; \
    OSMutexPost(&LCD_Mutex, OS_OPT_POST_NONE, &err); \
} while(0)

/* ==================== Function Declarations ==================== */

/* Basic drawing functions */
void LCD_Fill(u16 xsta, u16 ysta, u16 xend, u16 yend, u16 color);
void LCD_DrawPoint(u16 x, u16 y, u16 color);
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void Draw_Circle(u16 x0, u16 y0, u8 r, u16 color);

/* Chinese character display functions */
void LCD_ShowChinese(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese12x12(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese16x16(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese24x24(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowChinese32x32(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode);

/* ASCII character display functions */
void LCD_ShowChar(u16 x, u16 y, u8 num, u16 fc, u16 bc, u8 sizey, u8 mode);
void LCD_ShowString(u16 x, u16 y, const u8 *p, u16 fc, u16 bc, u8 sizey, u8 mode);

/* Number display functions */
u32 mypow(u8 m, u8 n);
void LCD_ShowIntNum(u16 x, u16 y, u16 num, u8 len, u16 fc, u16 bc, u8 sizey);
void LCD_ShowFloatNum1(u16 x, u16 y, float num, u8 len, u16 fc, u16 bc, u8 sizey);

/* Picture display function */
void LCD_ShowPicture(u16 x, u16 y, u16 length, u16 width, const u8 pic[]);

/* Advanced display functions */
void Draw_Wave(u16 *buf, u8 len);
void LCD_Show_Euler_Simple(float roll, float pitch, float yaw);

/* ==================== Color Definitions ==================== */

/* Grid colors */
#define COLOR_YELLOW_MAIN   0xFFE0  /* Main grid / coordinate axis (yellow) */
#define COLOR_YELLOW_SUB    0xF79E  /* Sub grid (light yellow) */
#define COLOR_BACKGROUND    0x0000  /* Background color (black) */

/* Grid intervals */
#define X_MAIN_INTERVAL     20      /* X-axis main grid interval */
#define X_SUB_INTERVAL      4       /* X-axis sub grid interval */
#define Y_MAIN_INTERVAL     16      /* Y-axis main grid interval */
#define Y_SUB_INTERVAL      4       /* Y-axis sub grid interval */

/* Line widths */
#define WIDTH_MAIN          2       /* Main grid line width */
#define WIDTH_SUB           1       /* Sub grid line width */
#define WIDTH_AXIS          3       /* Coordinate axis line width */

/* LCD resolution */
#define LCD_WIDTH           160     /* Width: X range 0~159 */
#define LCD_HEIGHT          128     /* Height: Y range 0~127 */

/* Basic colors */
#define WHITE               0xFFFF
#define BLACK               0x0000
#define BLUE                0x001F
#define BRED                0xF81F
#define GRED                0xFFE0
#define GBLUE               0x07FF
#define RED                 0xF800
#define MAGENTA             0xF81F
#define GREEN               0x07E0
#define CYAN                0x7FFF
#define YELLOW              0xFFE0
#define BROWN               0xBC40
#define BRRED               0xFC07
#define GRAY                0x8430
#define DARKBLUE            0x01CF
#define LIGHTBLUE           0x7D7C
#define GRAYBLUE            0x5458
#define LIGHTGREEN          0x841F
#define LGRAY               0xC618
#define LGRAYBLUE           0xA651
#define LBBLUE              0x2B12

#endif /* __LCD_H */
