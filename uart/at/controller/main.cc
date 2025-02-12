// Copyright 3bian Limited and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <platform-uart.hh>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <thread.h>

constexpr uint8_t ArgumentSize = 16;
constexpr uint8_t BufferSize = 32;
constexpr uint8_t MaxCommands = 10;

typedef void (*ATHandler)(volatile OpenTitanUart *uart, const char *argument);

struct ATCommand
{
    const char *command;
    uint16_t hash;
    ATHandler handler;
};

struct ATState
{
    ATCommand commands[MaxCommands];
    uint8_t commandCount;
    char buffer[BufferSize];
    char argument[ArgumentSize];
    uint8_t bufferIndex;
};

ATState atState; // Global AT state

void at_state_initialize(ATState *state);
bool at_state_register(ATState *state, const char *command, ATHandler handler);

void at_state_process_input(volatile OpenTitanUart *uart, ATState *state);
void at_state_process_command(volatile OpenTitanUart *uart, ATState *state);

uint16_t hash_command(const char *command);
void uart_write(volatile OpenTitanUart *uart, const char *string);

void handle_at_command_info(volatile OpenTitanUart *uart, const char *argument);
void handle_at_command_reset(volatile OpenTitanUart *uart, const char *argument);
void handle_at_command_get(volatile OpenTitanUart *uart, const char *argument);
void handle_at_command_set(volatile OpenTitanUart *uart, const char *argument);

[[noreturn]] void __cheri_compartment("controller") entry_point()
{
    auto uart = MMIO_CAPABILITY(OpenTitanUart, uart);
    at_state_initialize(&atState);
    at_state_register(&atState, "AT", handle_at_command_info);
    at_state_register(&atState, "AT+RESET", handle_at_command_reset);
    at_state_register(&atState, "AT+GET", handle_at_command_get);
    at_state_register(&atState, "AT+SET", handle_at_command_set);

    while (true)
    {
        at_state_process_input(uart, &atState);
    }
}

void at_state_initialize(ATState *state)
{
    state->bufferIndex = 0;
    state->commandCount = 0;
}

bool at_state_register(ATState *state, const char *command, ATHandler handler)
{
    if (state->commandCount >= MaxCommands)
    {
        return false;
    }

    uint8_t lastIndex = state->commandCount;
    state->commands[lastIndex].command = command;
    state->commands[lastIndex].hash = hash_command(command);
    state->commands[lastIndex].handler = handler;
    state->commandCount++;

    return true;
}

void at_state_process_input(volatile OpenTitanUart *uart, ATState *state)
{
    char byte = uart->blocking_read();
    if (byte == '\r' || byte == '\n')
    {
        if (state->bufferIndex > 0)
        {
            state->buffer[state->bufferIndex] = '\0';
            at_state_process_command(uart, state);
            state->bufferIndex = 0;
        }
    }
    else if (state->bufferIndex < BufferSize - 1)
    {
        state->buffer[state->bufferIndex++] = byte;
    }
}

void at_state_process_command(volatile OpenTitanUart *uart, ATState *state)
{
    char *command = state->buffer;
    char *argument = nullptr;

    for (uint8_t i = 0; i < state->bufferIndex; i++)
    {
        if (state->buffer[i] == '=')
        {
            state->buffer[i] = '\0';
            argument = &state->buffer[i + 1];
            break;
        }
    }

    uint16_t receivedHash = hash_command(command);
    for (uint8_t i = 0; i < state->commandCount; i++)
    {
        if (state->commands[i].hash == receivedHash && strcmp(state->commands[i].command, command) == 0)
        {
            state->commands[i].handler(uart, argument);
            return;
        }
    }
    uart_write(uart, "ERROR\n");
}

uint16_t hash_command(const char *command)
{
    uint16_t hash = 5381;
    while (*command)
    {
        hash = ((hash << 5) + hash) + *command++;
    }
    return hash;
}

void uart_write(volatile OpenTitanUart *uart, const char *string)
{
    while (*string)
    {
        uart->blocking_write(*string++);
    }
}

void handle_at_command_info(volatile OpenTitanUart *uart, const char *argument)
{
    uart_write(uart, "OK\n");
}

void handle_at_command_reset(volatile OpenTitanUart *uart, const char *argument)
{
    uart_write(uart, "RESETTING...\n");
}

void handle_at_command_get(volatile OpenTitanUart *uart, const char *argument)
{
    uart_write(uart, "GET OK\n");
}

void handle_at_command_set(volatile OpenTitanUart *uart, const char *argument)
{
    uart_write(uart, "SET OK: ");
    uart_write(uart, argument ? argument : "(no args)");
    uart_write(uart, "\n");
}