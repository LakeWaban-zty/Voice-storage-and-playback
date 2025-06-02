#include "my_usart.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"

// 串口1的缓冲区和状态变量
uint8_t rxBuffer1[RX_BUFFER_SIZE];     ///< 串口1接收缓冲区
uint8_t rxTemp1;                       ///< 串口1中断接收临时变量
uint16_t rxIndex1 = 0;                 ///< 串口1当前接收缓冲区索引
volatile uint8_t commandReceived1 = 0; ///< 串口1接收到命令标志

// 串口3的缓冲区和状态变量
uint8_t rxBuffer3[RX_BUFFER_SIZE];     ///< 串口3接收缓冲区
uint8_t rxTemp3;                       ///< 串口3中断接收临时变量
uint16_t rxIndex3 = 0;                 ///< 串口3当前接收缓冲区索引
volatile uint8_t commandReceived3 = 0; ///< 串口3接收到命令标志

// 串口2数据包缓冲区和变量
uint8_t rxBuffer2[RX_BUFFER_SIZE]; ///< 串口2接收缓冲区
uint8_t rxTemp2;                   ///< 串口2中断接收临时变量
uint16_t rxIndex2 = 0;             ///< 串口2当前接收缓冲区索引
volatile uint8_t commandReceived2 = 0; ///< 帧开始标志

/**
 * @brief 格式化打印并通过指定串口发送
 * @param huart 串口句柄
 * @param format 格式化字符串
 * @param ... 可变参数
 * @return 返回发送的字节数
 */
int my_printf(UART_HandleTypeDef *huart, const char *format, ...)
{
    char buffer[512]; // 临时缓冲区，用于存储格式化后的字符串
    va_list arg;      // 可变参数列表
    int len;

    va_start(arg, format);                                // 初始化可变参数列表
    len = vsnprintf(buffer, sizeof(buffer), format, arg); // 格式化字符串
    va_end(arg);                                          // 清理可变参数列表

    HAL_UART_Transmit(huart, (uint8_t *)buffer, (uint16_t)len, 0xFF); // 发送格式化后的字符串
    return len;
}

/**
 * @brief 串口接收中断回调函数
 * @param huart 串口句柄
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) // USART2的接收中断
    {
        if (rxIndex2 < RX_BUFFER_SIZE - 1)
        {
            // 如果接收到换行符并且前一个字符是回车符，标记命令接收完成
            if (rxTemp2 == '\n' && rxBuffer2[rxIndex2 - 1] == '\r')
            {
                commandReceived2 = 1;           // 设置命令接收标志
                rxBuffer2[rxIndex2 - 1] = '\0'; // 添加字符串结束符
                rxIndex2 = 0;                   // 重置接收索引
            }
            else
            {
                rxBuffer2[rxIndex2++] = rxTemp2; // 保存接收到的数据
            }
        }
        else
        {
            rxIndex2 = 0; // 缓冲区溢出，重置索引
        }
        HAL_UART_Receive_IT(&huart2, &rxTemp2, 1); // 再次启动接收中断
    }
}

