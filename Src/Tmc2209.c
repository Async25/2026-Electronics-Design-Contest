#include "Tmc2209.h"
#include <stdio.h>
#include "Oled.h"
#include "delay.h"
#include <string.h>
#include "main.h"

extern UART_HandleTypeDef huart1;

/*
 * ============================================================================
 * 4 个核心寄存器（参考: TMC2209 Datasheet Rev1.09）：
 * 1. GCONF       (0x00)  全局功能开关
 * 2. IHOLD_IRUN  (0x10)  保持/运行电流
 * 3. VACTUAL     (0x22)  内部速度模式，这里固定为 0，不使用
 * 4. CHOPCONF    (0x6C)  细分/斩波核心配置
 * ============================================================================
 */

// ============================================================================
// GCONF (0x00): 全局配置寄存器
//   寄存器位域（共 32 位，仅低 10 位有效）：
//   [31:10] Reserved                       保留位（写 0）
//   [9]     shaft           = 0            电机方向: 0=正常, 1=反转方向
//   [8]     multistep_filt  = 1            多步滤波: 1=对STEP脉冲进行去抖滤波,增强抗干扰
//   [7]     pdn_disable     = 1            引脚控制禁用: 1=禁用PDN_UART引脚硬件控制功能,
//                                                   启用UART软件接口控制
//   [6]     mstep_reg_select= 0            微步选择源: 0=由硬件引脚MS1/MS2决定,
//                                                   1=由寄存器CHOPCONF.MRES决定
//   [5:3]   Reserved                       保留位
//   [2]     en_spreadcycle  = 0            斩波模式: 0=StealthChop2™ 静音模式（低速静音）,
//                                                   1=SpreadCycle™ 动态模式（高速大扭矩）
//   [1:0]   Reserved                       保留位
//
//   当前配置含义:
//     SpreadCycle 动态模式 (高扭矩) | UART 控制使能 | STEP 脉冲滤波使能
//     → 电机方向正常、微步由硬件引脚决定
// ============================================================================
static const uint32_t TMC2209_GCONF_VALUE = 0x00000184;

// ============================================================================
// CHOPCONF (0x6C): 斩波控制寄存器
//   寄存器位域（共 32 位，核心位分布如下）：
//   [31]     diss2vs         = 0           保留位（写 0）
//   [30]     diss2g          = 0           保留位（写 0）
//   [29]     dedge           = 0           双沿STEP检测: 0=仅在上升沿、1=双沿(倍频)
//   [28]     intpol          = 1           MicroPlyer™ 插值使能: 1=将当前MRES细分插值
//                                                   平滑到256微步（降低低速振动噪声）
//   [27:24]  mres            = 8           微步分辨率 MRES（软件覆盖硬件引脚MS1/MS2）:
//                                             0=256微步    1=128微步    2=64微步
//                                             3=32微步     4=16微步     5=8微步
//                                             6=4微步      7=2微步(半步) 8=全步
//   [23:20]  sync            = 0           斩波同步相位 (0~15, 通常为0)
//   [19]     vhighchm        = 0           高速斩波模式切换 (仅StealthChop)
//   [18]     vhighfs         = 0           高速全步模式切换 (仅StealthChop)
//   [17]     vsense          = 0           电流检测量程:
//                                             0=高灵敏度 325mV (对应 RSense=110mΩ)
//                                             1=低灵敏度 180mV (对应内部RSense)
//   [16:15]  tbl             = 0           斩波器空白时间(Blanking Time):
//                                             0=16 clk    1=24 clk    2=36 clk    3=54 clk
//   [14]     chm             = 0           斩波器模式: 0=标准SpreadCycle、1=恒关断时间
//   [13]     rndtf           = 0           随机斩波关断时间: 0=固定、1=±12.5%随机化(降噪)
//   [12]     disfdcc         = 0           快速衰减比较器: 0=使能FDC、1=禁用
//   [11]     fd3             = 0           保留位
//   [10:7]   hend            = 0           迟滞结束值 HEND (sine wave offset, -3~12):
//                                                  低值=快速衰减占比大, 高值=慢衰减占比大
//   [6:4]    hstrt           = 5           迟滞起始值 HSTRT (1~8):
//                                                  迟滞窗口 = HSTRT - HEND, 越大电流纹波越大
//   [3:0]    toff            = 3           斩波器关断时间 TOFF:
//                                             0=驱动禁用(H桥高阻态)
//                                             1=仅当TBL≥2时可用
//                                             2~15=使能驱动, 值越大斩波频率越低
//
//   MRES 微步分辨率速查表（仅修改 bits[27:24] 即可切换）:
//     10000053 → MRES=0 → 256微步    11000053 → MRES=1 → 128微步
//     12000053 → MRES=2 → 64微步     13000053 → MRES=3 → 32微步
//     14000053 → MRES=4 → 16微步     15000053 → MRES=5 → 8微步
//     16000053 → MRES=6 → 4微步      17000053 → MRES=7 → 半步
//     18000053 → MRES=8 → 全步
//
//   当前配置含义:
//     全步模式 | MicroPlyer 插值使能(平滑) | 16clk 空白时间
//     | TOFF=3 | HSTRT=5 / HEND=0 | VSense=325mV 高灵敏度
// ============================================================================
static const uint32_t TMC2209_CHOPCONF_VALUE = 0x11000053;

// ============================================================================
// IHOLD_IRUN (0x10): 电流控制寄存器
//   寄存器位域（共 32 位）：
//   [31:24]  Reserved                       保留位（写 0）
//   [23:20]  iholddelay      = 6            待机电流衰减延迟 IHOLDDELAY (0~15):
//                                                  延迟 = IHOLDDELAY × 2^17 / fCLK
//                                                  fCLK=12MHz 时每单位 ≈ 10.9ms
//                                                  当前 6 × 10.9ms ≈ 65.5ms 后降为保持电流
//   [19:13]  Reserved                       保留位（写 0）
//   [12:8]   irun            = 17           运行电流 IRUN (0~31):
//                                                  电机正常运转时的相电流设定
//                                                  I_RMS ≈ (IRUN/32) × (325mV/Rsense) × 0.707
//   [7:5]    Reserved                       保留位（写 0）
//   [4:0]    ihold           = 8            保持电流 IHOLD (0~31):
//                                                  电机静止超 iholddelay 后的保持相电流
//                                                  用于降低待机发热，同时维持锁紧力矩
//
//   电流计算（典型 RSense = 110mΩ, Vsense = 0 即 325mV 量程）:
//     I_RMS  ≈ (IRUN/32) × 325mV / (RSense+20mΩ) × 1/√2
//            ≈ (IRUN/32) × 325mV / 130mΩ × 0.707
//            ≈ (IRUN/32) × 1.767A
//
//   电流速查表（IHOLDDELAY=6 固定，仅修改 ihold/irun 即可）:
//   ┌──────────┬──────┬───────┬──────────────┐
//   │ 目标RMS  │ IRUN │ IHOLD │ 寄存器值      │
//   ├──────────┼──────┼───────┼──────────────┤
//   │ 0.33A    │ 5    │ 2     │ 0x00060502   │
//   │ 0.50A    │ 8    │ 4     │ 0x00060804   │
//   │ 0.72A    │ 12   │ 6     │ 0x00060C06   │
//   │ 0.83A    │ 14   │ 7     │ 0x00060E07   │
//   │ 1.00A    │ 17   │ 8     │ 0x00061108   │
//   │ 1.22A    │ 21   │ 10    │ 0x0006150A   │
//   │ 1.33A    │ 23   │ 11    │ 0x0006170B   │
//   │ 1.50A    │ 26   │ 13    │ 0x00061A0D   │
//   │ 1.77A(Max)│31   │ 15    │ 0x00061F0F   │
//   └──────────┴──────┴───────┴──────────────┘
//
// ============================================================================

// PITCH 轴 (addr 0) 额定 1.3A，采用保守设置: 1.00A RMS (IRUN=17), 保持电流 0.50A RMS (IHOLD=8)
static const uint32_t TMC2209_IHOLD_IRUN_VALUE_PITCH = 0x0006150A;

// YAW 轴 (addr 1) 额定 1.5A，采用保守设置: 1.22A RMS (IRUN=21), 保持电流 0.61A RMS (IHOLD=10)
static const uint32_t TMC2209_IHOLD_IRUN_VALUE_YAW = 0x00061A0D;

// ============================================================================
// VACTUAL (0x22): 内部速度模式寄存器 (内部脉冲发生器)
//   32-bit 有符号整数 (int32_t):
//     0        = 电机停止（不使用内部脉冲发生器）
//     正值     = 正向旋转
//     负值     = 反向旋转
//   速度公式:
//     v[Hz] = VACTUAL × fCLK / (2^24 × 2048)
//           = VACTUAL × 0.715 Hz   (fCLK=12MHz 典型值时)
//   说明:
//     该寄存器启用 TMC2209 内部 STEP 脉冲发生器，无需外部 PWM 脉冲即可自主旋转。
//     设为 0 表示禁用内部发生器，完全依赖外部 STEP/DIR 引脚控制。
//
//   当前配置含义:
//     0 = 不使用内部速度模式，电机由外部 MCU 的 STEP/DIR 引脚控制
// ============================================================================
static const uint32_t TMC2209_VACTUAL_VALUE = 0x00000000;

void tmc_calc_crc(uint8_t *dat, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < (len - 1); i++)
    {
        uint8_t currentByte = dat[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if ((crc >> 7) ^ (currentByte & 0x01))
            {
                crc = (crc << 1) ^ 0x07;
            }
            else
            {
                crc = (crc << 1);
            }
            currentByte >>= 1;
        }
    }
    dat[len - 1] = crc;
}

void TMC2209_WriteReg(uint8_t address, uint8_t reg, uint32_t val)
{
    uint8_t txBuffer[8];
    txBuffer[0] = 0x05;
    txBuffer[1] = address;
    txBuffer[2] = reg | 0x80;
    txBuffer[3] = (val >> 24) & 0xFF;
    txBuffer[4] = (val >> 16) & 0xFF;
    txBuffer[5] = (val >> 8) & 0xFF;
    txBuffer[6] = val & 0xFF;
    tmc_calc_crc(txBuffer, 8);

    HAL_UART_Transmit(&huart1, txBuffer, 8, 10);
    uint8_t trash[8];
    HAL_UART_Receive(&huart1, trash, 8, 5);
}

uint8_t TMC2209_ReadReg_Robust(uint8_t tmc_addr, uint8_t reg, uint32_t *val_out)
{
    uint8_t txBuffer[4];
    uint8_t rxBuffer[24];

    txBuffer[0] = 0x05;
    txBuffer[1] = tmc_addr;
    txBuffer[2] = reg;
    tmc_calc_crc(txBuffer, 4);

    __HAL_UART_FLUSH_DRREGISTER(&huart1);
    memset(rxBuffer, 0, sizeof(rxBuffer));

    HAL_UART_Transmit(&huart1, txBuffer, 4, 10);

    HAL_UART_Receive(&huart1, rxBuffer, 20, 50);

    // 滑动窗口搜索特征头 (0x05 0xFF)
    for (uint8_t i = 0; i < 12; i++)
    {
        // 特征：当前是 0x05，下一个是 0xFF (主机地址)
        if (rxBuffer[i] == 0x05 && rxBuffer[i + 1] == 0xFF)
        {
            if (rxBuffer[i + 2] != reg)
            {
                continue;
            }

            // 5.2 提取这 8 个字节进行 CRC 校验
            uint8_t potential_packet[8];
            memcpy(potential_packet, &rxBuffer[i], 8);

            uint8_t received_crc = potential_packet[7];
            tmc_calc_crc(potential_packet, 8);

            if (received_crc == potential_packet[7])
            {
                *val_out = 0;
                *val_out |= (uint32_t)potential_packet[3] << 24;
                *val_out |= (uint32_t)potential_packet[4] << 16;
                *val_out |= (uint32_t)potential_packet[5] << 8;
                *val_out |= (uint32_t)potential_packet[6];
                return 1; // 成功
            }
        }
    }

    return 0;
}

void Tmc2209_Init(uint8_t tmc_addr)
{

    OLED_String(0, 0, "Tmc", 16);
    OLED_Num(25, 0, tmc_addr, 1, 16);
    delay_ms(100);
    TMC2209_WriteReg(tmc_addr, TMC2209_GCONF, TMC2209_GCONF_VALUE);
    TMC2209_WriteReg(tmc_addr, TMC2209_CHOPCONF, TMC2209_CHOPCONF_VALUE);
    
    // 根据电机地址配置不同的电流
    if (tmc_addr == 0)
    {
        // PITCH 轴
        TMC2209_WriteReg(tmc_addr, TMC2209_IHOLD_IRUN, TMC2209_IHOLD_IRUN_VALUE_PITCH);
    }
    else if (tmc_addr == 1)
    {
        // YAW 轴
        TMC2209_WriteReg(tmc_addr, TMC2209_IHOLD_IRUN, TMC2209_IHOLD_IRUN_VALUE_YAW);
    }

    TMC2209_WriteReg(tmc_addr, TMC2209_VACTUAL, TMC2209_VACTUAL_VALUE);

    delay_ms(50);

    uint32_t read_val = 0;
    uint8_t retry = 0;
    uint8_t read_ok = 0;

    const uint32_t expected_gconf_mask = 0x00000184;
    while (retry < 10)
    {
        if (TMC2209_ReadReg_Robust(tmc_addr, TMC2209_GCONF, &read_val))
        {
            OLED_String(0, 17, "read sucess", 12);
            if ((read_val & expected_gconf_mask) == expected_gconf_mask)
            {
                read_ok = 1;
                break;
            }
        }
        else
        {
            OLED_String(0, 17, "read false  ", 12);
        }
        OLED_String(0, 30, "Attempts:", 12);
        OLED_Num(60, 30, retry + 1, 4, 12);
        retry++;
        delay_ms(200);
    }
    if (!read_ok)
    {
        OLED_String(0, 17, "GCONF warn ", 12);
        delay_ms(1000);
    }
    OLED_String(41, 0, " Init OK  ", 16);
    OLED_Clear();
}
