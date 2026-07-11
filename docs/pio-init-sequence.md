# PIO State Machine Initialisation / PIO 状态机初始化流程

## The Missing `pio_sm_init` / 缺失的 `pio_sm_init`

The bug was: PIO program loaded, clock divider set, pins mapped — but **no output on GPIO**.

The root cause: `pio_sm_init()` was never called. All the `sm_config_set_*()` calls only modify a **local struct in RAM** — they do not write to hardware.

## Hardware Register vs. Config Struct / 硬件寄存器 vs. 配置结构体

```
                          ┌─────────────────────┐
sm_config_set_set_pins()  │                     │
sm_config_set_wrap()      │  pio_sm_config c     │  ← 仅改 RAM 中的 struct
sm_config_set_clkdiv()    │  (local variable)    │
                          └──────────┬──────────┘
                                     │
                          pio_sm_init(pio, sm, offset, &c)
                                     │
                          ┌──────────▼──────────┐
                          │  PIO hardware regs   │  ← 写入硬件
                          │  SM0_CLKDIV          │
                          │  SM0_EXECCTRL        │
                          │  SM0_SHIFTCTRL       │
                          │  SM0_PINCTRL         │
                          └─────────────────────┘
```

`pio_sm_init()` is the **only function** that writes the compiled config struct into the PIO hardware registers. Every `sm_config_set_*()` before it is just building a struct in memory — nothing reaches the silicon until `pio_sm_init` executes.

## What Each Function Does / 每个函数的作用

| Function | Writes Hardware? | Purpose |
|----------|-----------------|---------|
| `pio_sm_config` + `sm_config_set_*()` | **No** — fills a local struct | Build the desired configuration in RAM |
| `pio_add_program()` | **Yes** — writes to PIO instruction memory (INSTR_MEM) | Load the assembled PIO instructions |
| `pio_gpio_init()` | **Yes** — configures GPIO mux, pulls | Give PIO control of the physical pin |
| `pio_sm_set_consecutive_pindirs()` | **Yes** — sets pin direction override | Tell PIO to drive the pins as outputs |
| **`pio_sm_init()`** | **Yes** — writes SM0_CLKDIV, SM0_EXECCTRL, SM0_SHIFTCTRL, SM0_PINCTRL | **Commit config struct to hardware** |
| `pio_sm_set_enabled()` | **Yes** — writes SM0_CTRL.EN | Start / stop the state machine |

## Correct Initialisation Order / 正确的初始化顺序

```
1. pio_claim_unused_sm()          ← 分配一个空闲 SM
2. pio_add_program()              ← 把 .pio 字节码写入 PIO 指令内存
3. pio_gpio_init()                ← GPIO 引脚分配给 PIO
4. sm_config_set_set_pins()       ← 配置 SET 指令引脚
5. sm_config_set_wrap()           ← 配置循环边界
6. sm_config_set_clkdiv()         ← 配置时钟分频
7. pio_sm_set_consecutive_pindirs() ← 设置引脚方向
8. pio_sm_init()                  ← ★ 写入硬件寄存器
9. pio_sm_set_enabled(pio, sm, true) ← 启动状态机
```

## Lesson / 教训

> Every `sm_config_set_*()` call is just **building a blueprint**. Without `pio_sm_init()`, the blueprint stays on paper — the state machine hardware never receives it.

The Pico SDK pattern of "build config → commit once" is deliberate:
- Building the config in RAM costs zero hardware cycles
- `pio_sm_init()` atomically writes all registers at once, avoiding partial/flickering configurations
- But it means you must **remember the commit step** — there's no compiler warning for a missing `pio_sm_init`
