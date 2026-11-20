# RTOS Stopwatch/Timer System

**Course** Real Time Systems, EFS8050-3005  
**Team** Jihu, Jan, Paula

## Project overview

We built a small stopwatch with an ESP32, an OLED display and FreeRTOS. The user can start, pause, resume and reset the stopwatch through the Serial Monitor. The display shows the current status and the elapsed time in minutes, seconds and tenths of a second.

The main goal is to show how three tasks can share work and communicate safely. One task reads commands, one handles the stopwatch state, and one updates the display. A queue passes commands between tasks, and a mutex protects the shared stopwatch data and display buffer.

Since we did not have any buttons in our kits, we used the serial monitor to simulate buttons by pressing keys on the keyboard.

## User controls

Send lowercase s or r. The line ending setting can be No line ending, Newline, Carriage return, or Both NL & CR. The code ignores newline and carriage return characters.

| Command | Current state | Result |
| --- | --- | --- |
| s | Ready | Start from zero and show RUNNING |
| s | Running | Pause, keep the elapsed time and show PAUSED |
| s | Paused | Resume from the saved elapsed time and show RUNNING |
| r | Any state | Stop, clear the elapsed time and show READY |
| Other character, except a line ending | Any state | Print a help message and leave the stopwatch unchanged |

For example, run for about three seconds, send s to pause, wait, and send s again. The stopwatch should continue from the saved time. Sending r returns it to zero and leaves it stopped.

Uppercase letters and spaces are treated as invalid input. Commands are processed as individual characters, so sending several characters produces several separate input events.

## Serial input and the command queue

serialTask reads every currently available character. It accepts s and r, ignores carriage returns and newlines, and prints a help message for other characters. It does not change the stopwatch state or use the display.

After processing the available input, the task calls vTaskDelay(pdMS_TO_TICKS(20)). This gives other tasks an opportunity to run. The 20 ms value is a relative delay after input processing, not a guaranteed maximum response time.

commandQueue = xQueueCreate(5, sizeof(char)) creates a FIFO queue for five commands. xQueueSend() copies each character into the queue, so the local variable can be reused safely. timeTask receives the commands in order.

This separates receiving input from acting on it. serialTask does not need to know how the stopwatch stores its elapsed time.

Both queue operations use portMAX_DELAY for an indefinite wait in the normal ESP32 FreeRTOS configuration. When the queue is empty, timeTask waits until input arrives. When it is full, serialTask waits for space.

This prevents the queue operation from dropping a command because the queue is full. It does not guarantee that the serial input buffer can handle an unlimited stream of incoming data.

serialTask does not need oledMutex because it does not access the shared stopwatch state or display object. The queue provides the synchronization needed for transferring commands.

## Input tests

The following table lists the input checks. Expected results describe what should happen. Observed results must be recorded during testing on the hardware.

| Check | Procedure | Expected result | Observed result |
| --- | --- | --- | --- |
| Invalid input | Send x and uppercase S separately | Help message and unchanged stopwatch state | Both x and uppercase S produced a help message in the Serial Monitor. The stopwatch state remained unchanged. |
| Line endings | Send valid commands using each line ending setting | Only the command affects the state. No help message for line endings | Valid commands worked with all four line-ending settings. Newline and carriage return characters did not produce additional help messages |
| Command order | From reset, send ssr as one short input | Start, pause and reset are processed. Final state is READY at zero | After sending ssr, the display showed READY and zero elapsed time. The stopwatch remained stopped. |

The ssr check verifies the final outcome of a short command sequence. It does not prove queue saturation behavior, and the display may not show every intermediate state between refreshes.

