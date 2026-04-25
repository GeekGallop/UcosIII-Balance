/**
 ******************************************************************************
 * @file    lcd.c
 * @brief   LCD driver implementation with RTOS support
 * @note    Fixed formatting and added English comments
 ******************************************************************************
 */

#include "./lcd/lcd.h"
#include "./lcd/lcd_init.h"
#include "./lcd/lcdfont.h"
#include "./delay/delay.h"
#include "os.h"

/**
 * @brief  Fill a rectangular area with specified color
 * @param  xsta: Start X coordinate
 * @param  ysta: Start Y coordinate
 * @param  xend: End X coordinate
 * @param  yend: End Y coordinate
 * @param  color: Fill color
 * @retval None
 */
void LCD_Fill(u16 xsta, u16 ysta, u16 xend, u16 yend, u16 color)
{
    u16 i, j;
    
    LCD_MUTEX_PEND();
    LCD_Address_Set(xsta, ysta, xend - 1, yend - 1);
    
    for (i = ysta; i < yend; i++)
    {
        for (j = xsta; j < xend; j++)
        {
            LCD_WR_DATA(color);
        }
    }
    
    LCD_MUTEX_POST();
}

/**
 * @brief  Draw a point at specified position (with mutex)
 * @param  x: X coordinate
 * @param  y: Y coordinate
 * @param  color: Point color
 * @retval None
 */
void LCD_DrawPoint_OS(u16 x, u16 y, u16 color)
{
    LCD_MUTEX_PEND();
    LCD_Address_Set(x, y, x, y);
    LCD_WR_DATA(color);
    LCD_MUTEX_POST();
}

/**
 * @brief  Draw a point at specified position (without mutex)
 * @param  x: X coordinate
 * @param  y: Y coordinate
 * @param  color: Point color
 * @retval None
 */
void LCD_DrawPoint(u16 x, u16 y, u16 color)
{
    LCD_Address_Set(x, y, x, y);
    LCD_WR_DATA(color);
}

/**
 * @brief  Draw a line between two points
 * @param  x1: Start X coordinate
 * @param  y1: Start Y coordinate
 * @param  x2: End X coordinate
 * @param  y2: End Y coordinate
 * @param  color: Line color
 * @retval None
 */
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
    u16 t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;
    
    LCD_MUTEX_PEND();
    
    /* Calculate coordinate increments */
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    uRow = x1;
    uCol = y1;
    
    /* Set step direction */
    if (delta_x > 0)
        incx = 1;
    else if (delta_x == 0)
        incx = 0;  /* Vertical line */
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }
    
    if (delta_y > 0)
        incy = 1;
    else if (delta_y == 0)
        incy = 0;  /* Horizontal line */
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }
    
    /* Select basic increment axis */
    if (delta_x > delta_y)
        distance = delta_x;
    else
        distance = delta_y;
    
    /* Draw line using Bresenham algorithm */
    for (t = 0; t < distance + 1; t++)
    {
        LCD_DrawPoint(uRow, uCol, color);
        xerr += delta_x;
        yerr += delta_y;
        
        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }
        
        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
    
    LCD_MUTEX_POST();
}

/**
 * @brief  Draw a rectangle
 * @param  x1: Top-left X coordinate
 * @param  y1: Top-left Y coordinate
 * @param  x2: Bottom-right X coordinate
 * @param  y2: Bottom-right Y coordinate
 * @param  color: Rectangle color
 * @retval None
 */
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
    LCD_DrawLine(x1, y1, x2, y1, color);
    LCD_DrawLine(x1, y1, x1, y2, color);
    LCD_DrawLine(x1, y2, x2, y2, color);
    LCD_DrawLine(x2, y1, x2, y2, color);
}

/**
 * @brief  Draw a circle
 * @param  x0: Center X coordinate
 * @param  y0: Center Y coordinate
 * @param  r: Radius
 * @param  color: Circle color
 * @retval None
 */
void Draw_Circle(u16 x0, u16 y0, u8 r, u16 color)
{
    int a, b;
    a = 0;
    b = r;
    
    while (a <= b)
    {
        LCD_DrawPoint(x0 - b, y0 - a, color);  /* 3 */
        LCD_DrawPoint(x0 + b, y0 - a, color);  /* 0 */
        LCD_DrawPoint(x0 - a, y0 + b, color);  /* 1 */
        LCD_DrawPoint(x0 - a, y0 - b, color);  /* 2 */
        LCD_DrawPoint(x0 + b, y0 + a, color);  /* 4 */
        LCD_DrawPoint(x0 + a, y0 - b, color);  /* 5 */
        LCD_DrawPoint(x0 + a, y0 + b, color);  /* 6 */
        LCD_DrawPoint(x0 - b, y0 + a, color);  /* 7 */
        a++;
        
        /* Check if point is outside circle */
        if ((a * a + b * b) > (r * r))
        {
            b--;
        }
    }
}

/**
 * @brief  Display Chinese characters
 * @param  x: Start X coordinate
 * @param  y: Start Y coordinate
 * @param  s: Pointer to Chinese string
 * @param  fc: Foreground color
 * @param  bc: Background color
 * @param  sizey: Font size (12/16/24/32)
 * @param  mode: 0=non-overlay, 1=overlay
 * @retval None
 */
void LCD_ShowChinese(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode)
{
    while (*s != 0)
    {
        if (sizey == 12)
            LCD_ShowChinese12x12(x, y, s, fc, bc, sizey, mode);
        else if (sizey == 16)
            LCD_ShowChinese16x16(x, y, s, fc, bc, sizey, mode);
        else if (sizey == 24)
            LCD_ShowChinese24x24(x, y, s, fc, bc, sizey, mode);
        else if (sizey == 32)
            LCD_ShowChinese32x32(x, y, s, fc, bc, sizey, mode);
        else
            return;
        
        s += 2;
        x += sizey;
    }
}

/**
 * @brief  Display 12x12 Chinese character
 * @param  x: X coordinate
 * @param  y: Y coordinate
 * @param  s: Pointer to Chinese character
 * @param  fc: Foreground color
 * @param  bc: Background color	
 * @param  sizey: Font size
 * @param  mode: 0=non-overlay, 1=overlay
 * @retval None
 */
void LCD_ShowChinese12x12(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode)
{
    u8 i, j, m = 0;
    u16 k;
    u16 HZnum;        /* Number of Chinese characters */
    u16 TypefaceNum;  /* Bytes per character */
    u16 x0 = x;
    
    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont12) / sizeof(typFNT_GB12);
    
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont12[k].Index[0] == *(s)) && (tfont12[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
             
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (!mode)  /* Non-overlay mode */
                    {
                        if (tfont12[k].Msk[i] & (0x01 << j))
                            LCD_WR_DATA(fc);
                        else
                            LCD_WR_DATA(bc);
                        
                        m++;
                        if (m % sizey == 0)
                        {
                            m = 0;
                            break;
                        }
                    }
                    else  /* Overlay mode */
                    {
                        if (tfont12[k].Msk[i] & (0x01 << j))
                            LCD_DrawPoint(x, y, fc);
                        
                        x++;
                        if ((x - x0) == sizey)
                        {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
        }
        continue;  /* Exit after finding the character */
    }
}

/**
 * @brief  Display 16x16 Chinese character
 * @param  x: X coordinate
 * @param  y: Y coordinate
 * @param  s: Pointer to Chinese character
 * @param  fc: Foreground color
 * @param  bc: Background color
 * @param  sizey: Font size
 * @param  mode: 0=non-overlay, 1=overlay
 * @retval None
 */
void LCD_ShowChinese16x16(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode)
{
    u8 i, j, m = 0;
    u16 k;
    u16 HZnum;
    u16 TypefaceNum;
    u16 x0 = x;
    
    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont16) / sizeof(typFNT_GB16);
    
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont16[k].Index[0] == *(s)) && (tfont16[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
            
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (!mode)  /* Non-overlay mode */
                    {
                        if (tfont16[k].Msk[i] & (0x01 << j))
                            LCD_WR_DATA(fc);
                        else
                            LCD_WR_DATA(bc);
                        
                        m++;
                        if (m % sizey == 0)
                        {
                            m = 0;
                            break;
                        }
                    }
                    else  /* Overlay mode */
                    {
                        if (tfont16[k].Msk[i] & (0x01 << j))
                            LCD_DrawPoint(x, y, fc);
                        
                        x++;
                        if ((x - x0) == sizey)
                        {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
        }
        continue;
    }
}

/**
 * @brief  Display 24x24 Chinese character
 * @param  x: X coordinate
 * @param  y: Y coordinate
 * @param  s: Pointer to Chinese character
 * @param  fc: Foreground color
 * @param  bc: Background color
 * @param  sizey: Font size
 * @param  mode: 0=non-overlay, 1=overlay
 * @retval None
 */
void LCD_ShowChinese24x24(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode)
{
    u8 i, j, m = 0;
    u16 k;
    u16 HZnum;
    u16 TypefaceNum;
    u16 x0 = x;
    
    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont24) / sizeof(typFNT_GB24);
    
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont24[k].Index[0] == *(s)) && (tfont24[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
            
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (!mode)  /* Non-overlay mode */
                    {
                        if (tfont24[k].Msk[i] & (0x01 << j))
                            LCD_WR_DATA(fc);
                        else
                            LCD_WR_DATA(bc);
                        
                        m++;
                        if (m % sizey == 0)
                        {
                            m = 0;
                            break;
                        }
                    }
                    else  /* Overlay mode */
                    {
                        if (tfont24[k].Msk[i] & (0x01 << j))
                            LCD_DrawPoint(x, y, fc);
                        
                        x++;
                        if ((x - x0) == sizey)
                        {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
        }
        continue;
    }
}

/**
 * @brief  Display 32x32 Chinese character
 * @param  x: X coordinate
 * @param  y: Y coordinate
 * @param  s: Pointer to Chinese character
 * @param  fc: Foreground color
 * @param  bc: Background color
 * @param  sizey: Font size
 * @param  mode: 0=non-overlay, 1=overlay
 * @retval None
 */
void LCD_ShowChinese32x32(u16 x, u16 y, u8 *s, u16 fc, u16 bc, u8 sizey, u8 mode)
{
    u8 i, j, m = 0;
    u16 k;
    u16 HZnum;
    u16 TypefaceNum;
    u16 x0 = x;
    
    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont32) / sizeof(typFNT_GB32);
    
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont32[k].Index[0] == *(s)) && (tfont32[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
            
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (!mode)  /* Non-overlay mode */
                    {
                        if (tfont32[k].Msk[i] & (0x01 << j))
                            LCD_WR_DATA(fc);
                        else
                            LCD_WR_DATA(bc);
                        
                        m++;
                        if (m % sizey == 0)
                        {
                            m = 0;
                            break;
                        }
                    }
                    else  /* Overlay mode */
                    {
                        if (tfont32[k].Msk[i] & (0x01 << j))
                            LCD_DrawPoint(x, y, fc);
                        
                        x++;
                        if ((x - x0) == sizey)
                        {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
        }
        continue;
    }
}

/**
 * @brief  Display a single ASCII character
 * @param  x: X coordinate
 * @param  y: Y coordinate
 * @param  num: Character to display
 * @param  fc: Foreground color
 * @param  bc: Background color
 * @param  sizey: Font size (12/16/24/32)
 * @param  mode: 0=non-overlay, 1=overlay
 * @retval None
 */
void LCD_ShowChar(u16 x, u16 y, u8 num, u16 fc, u16 bc, u8 sizey, u8 mode)
{
    u8 temp, sizex, t, m = 0;
    u16 i, TypefaceNum;
    u16 x0 = x;
    
    sizex = sizey / 2;
    TypefaceNum = (sizex / 8 + ((sizex % 8) ? 1 : 0)) * sizey;
    num = num - ' ';  /* Get offset value */
    
    LCD_Address_Set(x, y, x + sizex - 1, y + sizey - 1);
    
    for (i = 0; i < TypefaceNum; i++)
    {
        if (sizey == 12)
            temp = ascii_1206[num][i];
        else if (sizey == 16)
            temp = ascii_1608[num][i];
        else if (sizey == 24)
            temp = ascii_2412[num][i];
        else if (sizey == 32)
            temp = ascii_3216[num][i];
        else
            return;
        
        for (t = 0; t < 8; t++)
        {
            if (!mode)  /* Non-overlay mode */
            {
                if (temp & (0x01 << t))
                    LCD_WR_DATA(fc);
                else
                    LCD_WR_DATA(bc);
                
                m++;
                if (m % sizex == 0)
                {
                    m = 0;
                    break;
                }
            }
            else  /* Overlay mode */
            {
                if (temp & (0x01 << t))
                    LCD_DrawPoint(x, y, fc);
                
                x++;
                if ((x - x0) == sizex)
                {
                    x = x0;
                    y++;
                    break;
                }
            }
        }
    }
}

/**
 * @brief  Display a string
 * @param  x: Start X coordinate
 * @param  y: Start Y coordinate
 * @param  p: Pointer to string
 * @param  fc: Foreground color
 * @param  bc: Background color
 * @param  sizey: Font size
 * @param  mode: 0=non-overlay, 1=overlay
 * @retval None
 */
void LCD_ShowString(u16 x, u16 y, const u8 *p, u16 fc, u16 bc, u8 sizey, u8 mode)
{
		CPU_SR_ALLOC();
    while (*p != '\0')
    {
        CPU_CRITICAL_ENTER();
        LCD_ShowChar(x, y, *p, fc, bc, sizey, mode);
        CPU_CRITICAL_EXIT();
        x += sizey / 2;
        p++;
    }
}

/**
 * @brief  Power function
 * @param  m: Base
 * @param  n: Exponent
 * @retval m^n
 */
u32 mypow(u8 m, u8 n)
{
    u32 result = 1;
    while (n--)
        result *= m;
    return result;
}

/**
 * @brief  Display an integer
 * @param  x: Start X coordinate
 * @param  y: Start Y coordinate
 * @param  num: Number to display
 * @param  len: Number of digits
 * @param  fc: Foreground color
 * @param  bc: Background color
 * @param  sizey: Font size
 * @retval None
 */
void LCD_ShowIntNum(u16 x, u16 y, u16 num, u8 len, u16 fc, u16 bc, u8 sizey)
{
    u8 t, temp;
    u8 enshow = 0;
    u8 sizex = sizey / 2;
    
    for (t = 0; t < len; t++)
    {
        temp = (num / mypow(10, len - t - 1)) % 10;
        
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                LCD_ShowChar(x + t * sizex, y, ' ', fc, bc, sizey, 0);
                continue;
            }
            else
                enshow = 1;
        }
        
        LCD_ShowChar(x + t * sizex, y, temp + 48, fc, bc, sizey, 0);
    }
}

/**
 * @brief  Display a floating point number (2 decimal places)
 * @param  x: Start X coordinate
 * @param  y: Start Y coordinate
 * @param  num: Number to display
 * @param  len: Total number of digits (including decimal point)
 * @param  fc: Foreground color
 * @param  bc: Background color
 * @param  sizey: Font size
 * @retval None
 */
void LCD_ShowFloatNum1(u16 x, u16 y, float num, u8 len, u16 fc, u16 bc, u8 sizey)
{
    u8 t, temp, sizex;
    u16 num1;
    
    sizex = sizey / 2;
    num1 = num * 100;
    
    for (t = 0; t < len; t++)
    {
        temp = (num1 / mypow(10, len - t - 1)) % 10;
        
        if (t == (len - 2))
        {
            LCD_ShowChar(x + (len - 2) * sizex, y, '.', fc, bc, sizey, 0);
            t++;
            len += 1;
        }
        
        LCD_ShowChar(x + t * sizex, y, temp + 48, fc, bc, sizey, 0);
    }
}

/**
 * @brief  Display a picture
 * @param  x: Start X coordinate
 * @param  y: Start Y coordinate
 * @param  length: Picture width
 * @param  width: Picture height
 * @param  pic: Pointer to picture data
 * @retval None
 */
void LCD_ShowPicture(u16 x, u16 y, u16 length, u16 width, const u8 pic[])
{
    u16 i, j;
    u32 k = 0;
    
    LCD_Address_Set(x, y, x + length - 1, y + width - 1);
    
    for (i = 0; i < length; i++)
    {
        for (j = 0; j < width; j++)
        {
            LCD_WR_DATA8(pic[k * 2]);
            LCD_WR_DATA8(pic[k * 2 + 1]);
            k++;
        }
    }
}

/* End of file */
