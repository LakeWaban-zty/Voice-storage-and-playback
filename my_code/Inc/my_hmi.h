#ifndef __MY_HMI_H
#define __MY_HMI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"
#include <stdarg.h> // 为可变参数添加

    /* 外部声明 */
    extern UART_HandleTypeDef huart2; // 添加这一行

    /**
     * @brief 向HMI控件发送字符串数据
     * @param obj_name 控件名称（建议格式：页面名.控件名，例如"page1.text1"）
     * @param show_data 要显示的字符串内容
     */
    void HMI_Send_String(char *obj_name, char *show_data);

    /**
     * @brief 向HMI控件发送整数数据
     * @param obj_name 控件名称（建议格式：页面名.控件名，例如"page1.number1"）
     * @param show_data 要显示的整数值
     */
    void HMI_Send_Int(char *obj_name, int show_data);

    /**
     * @brief 向HMI控件发送浮点数数据
     * @param obj_name 控件名称（建议格式：页面名.控件名，例如"page1.float1"）
     * @param show_data 要显示的浮点数值
     * @param point_index 小数点后保留的位数
     */
    void HMI_Send_Float(char *obj_name, float show_data, int point_index);

    /**
     * @brief 清除指定通道上的波形数据
     * @param obj_name 波形控件名称（建议格式：页面名.控件名，例如"page1.wave1"）
     * @param ch 波形通道编号（0 ~ 3）
     */
    void HMI_Wave_Clear(char *obj_name, int ch);

    /**
     * @brief 向指定波形控件添加单点数据（实时逐点发送，速度较慢）
     * @param obj_name 波形控件名称（建议格式：页面名.控件名，例如"page1.wave1"）
     * @param ch 波形通道编号（0 ~ 3）
     * @param val 数据值（0 ~ 255，需调用前归一化处理，0表示最底部）
     */
    void HMI_Write_Wave_Low(char *obj_name, int ch, int val);

    /**
     * @brief 向指定波形控件快速发送多点数据
     * @param obj_name 波形控件名称（建议格式：页面名.控件名，例如"page1.wave1"）
     * @param ch 波形通道编号（0 ~ 3）
     * @param len 数据长度 (最大1024)
     * @param val 数据值数组（每个值范围0 ~ 255，需调用前归一化处理）
     */
    void HMI_Write_Wave_Fast(char *obj_name, int ch, int len, int *val);

    /**
     * @brief 在HMI文本控件上显示调试信息
     * @param format 格式化字符串（类似于printf）
     * @param ... 可变参数
     */
    void HMI_Debug_Print(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* __MY_HMI_H */
