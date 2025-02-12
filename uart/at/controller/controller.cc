// Copyright 3bian Limited and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "interface.hh"
#include <array>
#include <compartment.h>
#include <cstdint>
#include <cstring>
#include <platform-uart.hh>
#include <string>
#include <string_view>

constexpr uint8_t bufferSize = 32;
constexpr uint8_t maxCommands = 10;

class ATCommand
{
  public:
    // Type alias for a command handler function.
    using Handler = std::string (*)(std::string_view);

    // Constructor: takes the command string, its precomputed hash, and the handler.
    ATCommand(std::string_view command, uint16_t hash, Handler handler) : command(command), hash(hash), handler(handler)
    {
    }

    // Default constructor (needed for array initialization).
    ATCommand()
    {
    }

    // Execute the command by calling its handler, passing in any arguments.
    std::string execute(std::string_view argument) const
    {
        if (handler)
        {
            return handler(argument);
        }
        return "ERROR";
    }

    // Compare a candidate command (with its precomputed hash) against this command.
    bool matches_with_hash(std::string_view candidate, uint16_t candidateHash) const
    {
        return (hash == candidateHash && command == candidate);
    }

  private:
    std::string_view command;  // The command text (e.g., "AT+RESET")
    uint16_t hash = 0;         // Precomputed hash for fast comparisons.
    Handler handler = nullptr; // Function pointer for executing the command.
};

class ATParser
{
  public:
    ATParser() : commandCount(0), bufferIndex(0)
    {
    }

    // Process one input byte from the UART.
    std::string process_input(char byte)
    {
        if (byte == '\r')
        {
            if (bufferIndex > 0)
            {
                // Null-terminate the buffer so it can be treated as a C-string.
                buffer[bufferIndex] = '\0';
                log(buffer.data());

                // Process the complete command.
                std::string result = process_command();
                log(result);

                // Reset the buffer for the next command.
                bufferIndex = 0;
                return result;
            }
        }
        else if (bufferIndex < bufferSize - 1)
        {
            // Add the byte if there is room in the buffer (reserve space for '\0').
            buffer[bufferIndex++] = byte;
        }
        return "";
    }

    // Register a new AT command along with its handler.
    bool register_command(std::string_view command, ATCommand::Handler handler)
    {
        if (commandCount >= maxCommands)
        {
            return false;
        }

        // Compute the hash for the command string using the parser's compute_hash.
        uint16_t hash = compute_hash(command);
        commands[commandCount++] = ATCommand(command, hash, handler);
        return true;
    }

  private:
    std::array<char, bufferSize> buffer;         // Buffer for incoming characters.
    size_t bufferIndex;                          // Current position in the buffer.
    std::array<ATCommand, maxCommands> commands; // Array of registered commands.
    uint8_t commandCount;                        // Number of commands registered.

    // Compute a hash for a given string using a djb2-like algorithm.
    static uint16_t compute_hash(std::string_view input)
    {
        uint16_t computedHash = 5381;
        for (char character : input)
        {
            computedHash = ((computedHash << 5) + computedHash) + character;
        }
        return computedHash;
    }

    // Process the complete command stored in the buffer.
    std::string process_command()
    {
        // Create a string_view from the null-terminated buffer.
        std::string_view fullInput(buffer.data());
        std::string_view commandCandidate;
        std::string_view argumentCandidate;
        size_t equalSignIndex = std::string_view::npos;

        // Manually search for the '=' character.
        for (size_t i = 0; i < fullInput.size(); i++)
        {
            if (fullInput[i] == '=')
            {
                equalSignIndex = i;
                break;
            }
        }

        if (equalSignIndex != std::string_view::npos)
        {
            // The command is before '=', and the argument follows.
            commandCandidate = fullInput.substr(0, equalSignIndex);
            argumentCandidate = fullInput.substr(equalSignIndex + 1);
        }
        else
        {
            // No argument provided; the entire string is the command.
            commandCandidate = fullInput;
            argumentCandidate = "";
        }

        // Compute the hash for the candidate command once.
        uint16_t candidateHash = compute_hash(commandCandidate);

        // Search for a registered command that matches.
        for (uint8_t index = 0; index < commandCount; index++)
        {
            if (commands[index].matches_with_hash(commandCandidate, candidateHash))
            {
                return commands[index].execute(argumentCandidate);
            }
        }
        return "ERROR";
    }
};

std::string handle_at_command_info(std::string_view)
{
    return "OK";
}

std::string handle_at_command_reset(std::string_view)
{
    return "RESETTING...";
}

std::string handle_at_command_get(std::string_view)
{
    return "GET OK";
}

std::string handle_at_command_set(std::string_view argument)
{
    if (!argument.empty())
    {
        return "SET: " + std::string(argument);
    }
    return "SET: NO ARGS";
}

[[noreturn]] void __cheri_compartment("controller") entry_point()
{
    // Get a handle to the UART interface.
    auto uart = MMIO_CAPABILITY(OpenTitanUart, uart);

    // Create an ATParser instance.
    ATParser atParser;

    // Register supported AT commands.
    atParser.register_command("AT", handle_at_command_info);
    atParser.register_command("AT+RESET", handle_at_command_reset);
    atParser.register_command("AT+GET", handle_at_command_get);
    atParser.register_command("AT+SET", handle_at_command_set);

    // Main loop: Read bytes from UART, process input, and send responses.
    while (true)
    {
        char byte = uart->blocking_read();
        std::string output = atParser.process_input(byte);

        if (!output.empty())
        {
            for (char character : output)
            {
                uart->blocking_write(character);
            }
            uart->blocking_write('\r');
        }
    }
}
