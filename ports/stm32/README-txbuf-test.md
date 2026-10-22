# UART transmit buffer (`txbuf`): test plan

Fork patch: `machine.UART(..., txbuf=N)` on the stm32 port queues writes in an
interrupt-drained ring instead of busy-waiting per character and for TC.
`txbuf=0` (the default) must behave exactly as before. Files: `uart.c`,
`uart.h`, `machine_uart.c`, `main.c` (`MICROPY_HW_UART_REPL_TXBUF` hook, off),
`docs/library/machine.UART.rst`.

Run every step on an H743 board (frame or measurement). Steps 1 to 9 need one
spare UART with TX wired to RX (loopback jumper). Set `UART_ID` and keep
`rxbuf` larger than the largest single write so the receive side never drops.

## 0. Before flashing

- `MICROPY_HW_ENABLE_ISR_UART_FLASH_FUNCS_IN_RAM=1` boards: confirm the drain
  stayed inside the RAM copy of the IRQ handler:

  ```bash
  arm-none-eabi-objdump -d build-<BOARD>/firmware.elf \
      | awk '/<uart_irq_handler>:/,/^$/' \
      | grep -E '\s(bl|blx|b|b\.w|b\.n)\s' | grep -oE '<[A-Za-z0-9_.]+>' | sort -u
  ```

  This lists every branch target outside the handler itself (targets with
  a `+offset` are branches inside it). Only the calls that were there before
  the patch may appear (`mp_sched_keyboard_interrupt`, `mp_irq_handler`).
  There must be no
  `uart_tx_ring_drain` symbol in `arm-none-eabi-nm firmware.elf`. The
  handler address must lie in RAM (`nm firmware.elf | grep uart_irq_handler`,
  0x2000xxxx / 0x24xxxxxx), between `__isr_vector_ram_start` and `_end`.
- Compile, besides the H7 boards, one board without a UART FIFO (an F4 such
  as PYBV11, or a G0) so the TXEIE branches build, and one H7 build with
  `CFLAGS_EXTRA=-DMICROPY_HW_UART_REPL_TXBUF=256` for the REPL hook.

## 1. Unbuffered regression (txbuf not given)

```python
import time
from machine import UART
UART_ID = 4
u = UART(UART_ID, 115200, rxbuf=1024)
print(u)                      # ..., rxbuf=1024) exactly as before, no txbuf
data = bytes(range(64))
t = time.ticks_us(); u.write(data); dt = time.ticks_diff(time.ticks_us(), t)
print(dt)                     # about 64 * 86.8 us = 5.6 ms: blocks to the last stop bit
print(u.txdone())             # True
time.sleep_ms(5); print(u.read() == data)
```

## 2. Buffered write returns at once, flush and txdone

```python
u = UART(UART_ID, 115200, rxbuf=1024, txbuf=256)
print(u)                      # ..., txbuf=256, ...
data = bytes(i & 0xff for i in range(200))
t = time.ticks_us(); n = u.write(data); dt = time.ticks_diff(time.ticks_us(), t)
print(n, dt)                  # 200, well under 1 ms
print(u.txdone())             # False
t = time.ticks_us(); u.flush(); dt = time.ticks_diff(time.ticks_us(), t)
print(dt, u.txdone())         # about 200 * 86.8 us = 17.4 ms, True
time.sleep_ms(2); print(u.read() == data)
```

## 3. Ordering between the direct FIFO fill and the ring

Small writes go straight into the 16-character FIFO while the ring is empty;
larger ones spill into the ring. Mix both and `writechar` and check the order.

```python
u.read()
expected = bytearray()
for i in range(300):
    chunk = bytes(((i + k) & 0x7f) for k in range(i % 23 + 1))
    u.write(chunk); expected += chunk
    if i % 7 == 0:
        u.writechar(0xff); expected.append(0xff)
u.flush(); time.sleep_ms(5)
got = u.read()
print(len(got), got == expected)
```

## 4. Full ring

```python
u = UART(UART_ID, 115200, rxbuf=4096, txbuf=64)
data = bytes(i & 0xff for i in range(1000))
t = time.ticks_us(); n = u.write(data); dt = time.ticks_diff(time.ticks_us(), t)
print(n, dt)                  # 1000; about (1000 - 64 - 16) * 86.8 us = 80 ms
u.flush(); time.sleep_ms(5); print(u.read() == data)

import select
u = UART(UART_ID, 1200, rxbuf=256, txbuf=64)   # 8.3 ms per character
p = select.poll(); p.register(u, select.POLLOUT)
u.write(bytes(80))            # fills FIFO and ring, returns when the ring is full
print(p.poll(0))              # [] while the ring is full
time.sleep_ms(20)
print(p.poll(0))              # POLLOUT once a character has left
u.flush()
```

Stall: with `flow=UART.CTS` and CTS held inactive (if the board routes a CTS
pin), `write(bytes(200))` on `txbuf=64` must return about 80 (FIFO + ring)
after the per-character timeout, and `flush()` must raise `OSError(ETIMEDOUT)`.

## 5. Flush timing

Scope TX and a spare GPIO. Toggle the GPIO high before `flush()` and low after
it. The falling GPIO edge must come after the last stop bit and within one
character time of it at 115200 and at 1_000_000 baud.

## 6. Writes with the UART interrupt unable to run

The ring must drain by polling when the caller blocks the UART IRQ.

```python
import machine
u = UART(UART_ID, 115200, rxbuf=4096, txbuf=64)
state = machine.disable_irq()
n = u.write(bytes(i & 0xff for i in range(300)))   # must not hang
machine.enable_irq(state)
print(n); u.flush(); time.sleep_ms(5); print(len(u.read()))   # 300, 300

state = machine.disable_irq()
u.write(bytes(200)); u.write(bytes(200)); u.writechar(0x55)
machine.enable_irq(state)
u.flush(); time.sleep_ms(5); print(len(u.read()))              # 401
```

`writechar` and the second `write` must not wait out `timeout` with a full
ring: the wait for room drains by polling.

Also with a hard IRQ on the same UART writing to itself:

```python
calls = [0]
def echo(uart):
    if calls[0] < 3:          # the loopback would re-trigger forever
        calls[0] += 1
        uart.write(b"0123456789" * 10)
u.irq(echo, UART.IRQ_RXIDLE, hard=True)
u.write(b"go")                # three 100-byte writes from the IRQ, no hang
```

Then with BASEPRI raised by the internal-flash block device: run an asyncio
task streaming lines to the buffered UART every 5 ms while another task writes
and syncs a 64 KiB file to `/flash`. Every line must arrive intact, and the
scope must show no inter-character gap on TX longer than one character time
(the IRQ handler runs from RAM during the erase).

## 7. Soft reboot and reinit with a non-empty ring

```python
u = UART(UART_ID, 9600, txbuf=4096)
u.write(bytes(4000))          # about 4 s of queued data
machine.soft_reset()          # Ctrl-D equivalent
```

The board must come back to the REPL without a fault, and after it
`print(UART(UART_ID))` shows the UART unconfigured. Repeat with
`u.init(115200, txbuf=128)` and with `u.deinit()` in place of the reset: no
fault, the queued data is cut off (documented), and the UART works again
afterwards. Run `gc.collect()` a few times after each to catch a stale ring
pointer.

Dropping the buffer by `init()`: `u.init(115200)` without `txbuf` on a
buffered UART must print without `txbuf`, and a following 64-byte write
must block for about 5.6 ms again (as in step 1).

## 8. 9-bit characters

```python
u = UART(UART_ID, 115200, bits=9, rxbuf=256, txbuf=32)
import array
data = array.array("H", [0x1ff, 0x100, 0x0ff, 0x001] * 20)
u.write(data); u.flush(); time.sleep_ms(2)
back = array.array("H", u.read())
print(back == data)
```

## 9. sendbreak after a buffered write

`u.write(bytes(100)); u.sendbreak()`: on the scope the break must start after
the last stop bit of the 100th character, not inside the data.

## 10. RS-485 with hardware DE (measurement board, USART3)

With the Modbus driver's DEM setup applied and `txbuf` set: DE must stay
asserted over the whole request with no drop between characters, release
DEDT after the last stop bit, and no gap inside a frame may reach 1.5
character times at 9600 and 19200 baud. The driver must `flush()` (or poll
`txdone()`) before it starts the response timer.

## 11. Renode

Build the measurement binary and the frame-legacy binary from the patched
fork (frame-legacy through `build-scripts/frame-legacy-renode-bin.sh`), then
run `emulation/renode/resc/measurement.resc`, `frame-legacy.resc` and the SIL
scenarios. Expected: boot, REPL and links unchanged. Renode's
`STM32F7_USART` model is expected to report TX-empty at all times and send
instantly, so the buffered path takes the direct FIFO branch and the ring and
the TX threshold interrupt are not exercised there: Renode is a regression
check, the ring is proven only on hardware (steps 2 to 10). If a SIL scenario
hangs on `flush()`, check whether the model sets TC.

## 12. Interrupts per character

With a debugger (ST-Link and gdb on the ELF): `break uart_irq_handler`,
`ignore 1 100000`, continue, run `u.write(bytes(1400))` at 115200 on a
`txbuf=2048` UART, interrupt, `info breakpoints`. The hit count should be
about 100 (one per 14 characters with the 7/8 TX FIFO threshold), not
about 700 (1/8) or 1400.

## 13. Bench scope and loop jitter

Scope TX at 115200 with a 200-byte NDJSON line written through a buffered
`UartTransport`: continuous characters, no gaps. Measure the asyncio loop
jitter (a 1 ms task logging `ticks_diff` maxima) with and without `txbuf` on
the link UART; the buffered case should lose the per-line busy-wait of about
87 us per byte. With `UartTransport(uart, txbuf=256)` on a `txbuf=256` UART,
send a 600-byte line (written in slices) then a 200-byte line (written
whole): both arrive in order and intact, and no loop pass blocks for more
than a few characters.
