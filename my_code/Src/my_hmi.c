#include "my_hmi.h"
#include "math.h"
#include "my_usart.h"
#include "main.h"
#include "stdio.h"
#include <stdarg.h> // 添加这个以支持可变参数
/* 默认使用USART2通信接口，配置为Tx --> PA2, Rx --> PA3 */
#define huart_hmi huart2

/**
 * @brief 在HMI文本控件上显示调试信息
 * @param format 格式化字符串（类似于printf）
 * @param ... 可变参数
 */
void HMI_Debug_Print(const char *format, ...)
{
    char buffer[256]; // 格式化字符串的缓冲区
    va_list args;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // 将格式化字符串发送到page0页面的t4文本控件
    HMI_Send_String("page0.t4", buffer);
}

/**
 * @brief 向HMI控件发送字符串数据
 * @param obj_name 控件名称（建议格式：页面名.控件名，例如"page1.text1"）
 * @param show_data 要显示的字符串内容
 */
void HMI_Send_String(char *obj_name, char *show_data)
{
    my_printf(&huart_hmi, "%s.txt=\"%s\"\xff\xff\xff", obj_name, show_data);
}

/**
 * @brief 向HMI控件发送整数数据
 * @param obj_name 控件名称（建议格式：页面名.控件名，例如"page1.number1"）
 * @param show_data 要显示的整数值
 */
void HMI_Send_Int(char *obj_name, int show_data)
{
    my_printf(&huart_hmi, "%s.val=%d\xff\xff\xff", obj_name, show_data);
}

/**
 * @brief 向HMI控件发送浮点数数据
 * @param obj_name 控件名称（建议格式：页面名.控件名，例如"page1.float1"）
 * @param show_data 要显示的浮点数值
 * @param point_index 小数点后保留的位数
 */
void HMI_Send_Float(char *obj_name, float show_data, int point_index)
{
    int temp = show_data * pow(10, point_index); // 将浮点数按小数位数放大为整数
    my_printf(&huart_hmi, "%s.val=%d\xff\xff\xff", obj_name, temp);
}

/**
 * @brief 清除指定通道上的波形数据
 * @param obj_name 波形控件名称（建议格式：页面名.控件名，例如"page1.wave1"）
 * @param ch 波形通道编号（0 ~ 3）
 */
void HMI_Wave_Clear(char *obj_name, int ch)
{
    my_printf(&huart_hmi, "cle %s,%d\xff\xff\xff", obj_name, ch);
}

/**
 * @brief 向指定波形控件添加单点数据（实时逐点发送，速度较慢）
 * @param obj_name 波形控件名称（建议格式：页面名.控件名，例如"page1.wave1"）
 * @param ch 波形通道编号（0 ~ 3）
 * @param val 数据值（0 ~ 255，需调用前归一化处理，0表示最底部）
 */
void HMI_Write_Wave_Low(char *obj_name, int ch, int val)
{
    my_printf(&huart_hmi, "add %s.id,%d,%d\xff\xff\xff", obj_name, ch, val);
}

/**
 * @brief 向指定波形控件快速发送多点数据
 * @param obj_name 波形控件名称（建议格式：页面名.控件名，例如"page1.wave1"）
 * @param ch 波形通道编号（0 ~ 3）
 * @param len 数据长度 (最大1024)
 * @param val 数据值数组（每个值范围0 ~ 255，需调用前归一化处理）
 */
void HMI_Write_Wave_Fast(char *obj_name, int ch, int len, int *val)
{
    // 发送透传的控件ID、通道编号、数据长度
    my_printf(&huart_hmi, "addt %s.id,%d,%d\xff\xff\xff", obj_name, ch, len);
    HAL_Delay(100); // 延迟确保透传数据被接受
    for (int i = 0; i < len; i++)
    {
        my_printf(&huart_hmi, "%c", val[i]); // 逐个发送数据
    }
    my_printf(&huart_hmi, "\x01\xff\xff\xff"); // 发送无效命令，确保透传任务完成
}
