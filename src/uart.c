#include "memmap.h"
#include "types.h"
#include "uart.h"
#include "processor.h"

#define R(reg)  (volatile u32 *)(UARTBASE+reg)

/* Following defination from "PrimeCell® UART (PL011)" */
#define DR  0x00                // uart data register offset
#define FR  0x18                // uart status register offset
#define FR_RXFE (1<<4)          // receive fifo empty
#define FR_TXFF (1<<5)          // transmit fifo full
#define FR_RXFF (1<<6)          // receive fifo full
#define FR_TXFE (1<<7)          // transmit fifo empty
#define IBRD  0x24
#define FBRD  0x28
#define LCRH  0x2c              // Line Control Register
#define LCRH_FEN  (1<<4)        // 启用FIFO功能
#define LCRH_WLEN_8BIT  (3<<5)  // 指定数据长度为8位
#define CR    0x30              // Control Register
#define IMSC  0x38              // Interrupt Mask Set Clear Register
#define INT_RX_ENABLE (1<<4)
#define INT_TX_ENABLE (1<<5)
#define MIS   0x40              /* Masked Interrupt Status Register
                                 * 每个位表示一个中断源的状态, 且会收到中断屏蔽的影响
                                 * 位位置	 中断类型	              描述
                                 *   0	  UARTMIS_RIMIS	    Ring Indicator 中断状态
                                 *   1	  UARTMIS_CTSMIS	Clear to Send 中断状态
                                 *   2	  UARTMIS_DCDMIS	Data Carrier Detect 中断状态
                                 *   3	  UARTMIS_DSRMIS	Data Set Ready 中断状态
                                 *   4	  UARTMIS_RXMIS	    接收 FIFO 达到触发阈值时触发的中断
                                 *   5	  UARTMIS_TXMIS	    发送 FIFO 达到触发阈值时触发的中断
                                 *   6	  UARTMIS_RTMIS	    接收超时中断状态
                                 *   7	  UARTMIS_FEMIS	    帧错误中断状态
                                 *   8	  UARTMIS_PEMIS	    奇偶校验错误中断状态
                                 *   9	  UARTMIS_BEMIS	    Break 错误中断状态
                                 *   10	  UARTMIS_OEMIS	    FIFO 溢出中断状态
                                 */
#define ICR   0x44              // Interrupt Clear Register


void uart_putc(char c)
{
    // while (!(*R(FR) & FR_TXFE)) {        // 这种也行
    // while (*R(FR) & FR_RXFE) {           // 这么写的话, 需要一次键盘输入才行！ 因为是当receive fifo不为空时才结束循环
    while (*R(FR) & FR_TXFF) {
        cpu_relax();
    }
    *R(DR) = c;
}

void uart_puts(char *s)
{
    char c;
    while((c = *s++)) {
        uart_putc(c);
    }
}

int uart_getc(void)
{
    if (*R(FR) & FR_RXFE) {
        return -1;
    } else {
        return *R(DR);
    }
}

/* uart interrupt handler for el2 */
void uartintr() {
    int status = *R(MIS);

    if(status & INT_RX_ENABLE) {
        for(;;) {
        int c = uart_getc();
        if(c < 0)
            break;
        }
        // vcpu_dump(cur_vcpu());
    }
    /* Receive interrupt clear. Clears the UARTRXINTR interrupt. */
    *R(ICR) = (1 << 4);
}

void clear_uart_interrupt() {
    //*R(ICR) = (1 << 4);
    *R(ICR) = 0xFFFF;
}

unsigned int uart_get_interrupt_status() {
    return *R(MIS);
}

void uart_init(void)
{
    *R(CR)   = 0;                         // 清0复位
    *R(IMSC) = 0;                         // 禁用所有uart中断
    *R(LCRH) = LCRH_FEN | LCRH_WLEN_8BIT; // 启用FIFO功能并指定数据位长度为8位
    *R(CR)   = 0x301;                     // RXE(位9): 接受使能, TXE(位8): 发送使能, UARTEN(位0): UART控制器使能
    *R(IMSC) = (1<<4);                    // 允许接收中断发生(INT_RX_ENABLE)
}