TOOLCHAIN_DIR = C:/MounRiver/MounRiver_Studio2/resources/app/resources/win32/components/WCH/Toolchain/RISC-V Embedded GCC12/bin
CC      = "$(TOOLCHAIN_DIR)/riscv-wch-elf-gcc.exe"
AS      = "$(TOOLCHAIN_DIR)/riscv-wch-elf-gcc.exe"
OBJCOPY = "$(TOOLCHAIN_DIR)/riscv-wch-elf-objcopy.exe"
OBJDUMP = "$(TOOLCHAIN_DIR)/riscv-wch-elf-objdump.exe"
SIZE    = "$(TOOLCHAIN_DIR)/riscv-wch-elf-size.exe"

TARGET  = CH585D
OUTDIR  = obj

# 自动确保输出目录存在
$(shell if not exist obj mkdir obj)
$(shell if not exist obj\StdPeriphDriver mkdir obj\StdPeriphDriver)
$(shell if not exist obj\HAL mkdir obj\HAL)
$(shell if not exist obj\src mkdir obj\src)
$(shell if not exist obj\Startup mkdir obj\Startup)
$(shell if not exist obj\LIB mkdir obj\LIB)

# 架构和编译优化参数
ARCH_FLAGS = -march=rv32imac -mabi=ilp32 -mcmodel=medany -msmall-data-limit=8 -mno-save-restore
CFLAGS     = $(ARCH_FLAGS) -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -std=gnu99 -DDEBUG=1
ASFLAGS    = $(ARCH_FLAGS) -x assembler-with-cpp -DDEBUG=1
LDFLAGS    = $(ARCH_FLAGS) -T"Ld/Link.ld" -nostartfiles -Xlinker --gc-sections -Wl,--print-memory-usage -Wl,-Map="$(OUTDIR)/$(TARGET).map" --specs=nano.specs --specs=nosys.specs -L"StdPeriphDriver" -lISP585 -L"LIB" -lCH58xBLE

# 头文件目录
INCLUDES = \
	-I"StdPeriphDriver/inc" \
	-I"RVMSIS" \
	-I"LIB" \
	-I"HAL/include" \
	-I"src"

# C 源文件
SRCS_C = \
	StdPeriphDriver/CH58x_adc.c \
	StdPeriphDriver/CH58x_clk.c \
	StdPeriphDriver/CH58x_flash.c \
	StdPeriphDriver/CH58x_gpio.c \
	StdPeriphDriver/CH58x_i2c.c \
	StdPeriphDriver/CH58x_pwr.c \
	StdPeriphDriver/CH58x_pwm.c \
	StdPeriphDriver/CH58x_spi0.c \
	StdPeriphDriver/CH58x_spi1.c \
	StdPeriphDriver/CH58x_sys.c \
	StdPeriphDriver/CH58x_timer0.c \
	StdPeriphDriver/CH58x_timer1.c \
	StdPeriphDriver/CH58x_timer2.c \
	StdPeriphDriver/CH58x_timer3.c \
	StdPeriphDriver/CH58x_uart0.c \
	StdPeriphDriver/CH58x_uart1.c \
	StdPeriphDriver/CH58x_uart2.c \
	StdPeriphDriver/CH58x_uart3.c \
	StdPeriphDriver/CH58x_usbdev.c \
	HAL/KEY.c \
	HAL/LED.c \
	HAL/MCU.c \
	HAL/RTC.c \
	HAL/SLEEP.c \
	src/Main.c \
	src/board_test.c \
	src/ble_dev.c \
	src/usb_fs_dev.c \
	src/ch585_usbhs_device.c \
	src/usb_hs_cdc.c \
	src/usb_desc.c

# 汇编源文件
SRCS_S = \
	Startup/startup_CH585.S \
	LIB/ble_task_scheduler.S

OBJS = $(patsubst %.c,$(OUTDIR)/%.o,$(SRCS_C)) $(patsubst %.S,$(OUTDIR)/%.o,$(SRCS_S))

all: $(OUTDIR)/$(TARGET).elf $(OUTDIR)/$(TARGET).hex $(OUTDIR)/$(TARGET).bin

$(OUTDIR)/$(TARGET).elf: $(OBJS)
	@echo [LD] $@
	@$(CC) $(OBJS) $(LDFLAGS) -o $@
	@echo [SIZE]
	@$(SIZE) $@

$(OUTDIR)/$(TARGET).hex: $(OUTDIR)/$(TARGET).elf
	@echo [HEX] $@
	@$(OBJCOPY) -O ihex $< $@

$(OUTDIR)/$(TARGET).bin: $(OUTDIR)/$(TARGET).elf
	@echo [BIN] $@
	@$(OBJCOPY) -O binary $< $@

$(OUTDIR)/%.o: %.c
	@echo [CC] $<
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OUTDIR)/%.o: %.S
	@echo [AS] $<
	@$(AS) $(ASFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@rm -rf $(OUTDIR)
