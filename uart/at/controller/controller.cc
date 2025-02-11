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
    using Handler = std::string (*)(std::string_view);

    ATCommand() = default;

    ATCommand(std::string_view command, Handler handler)
        : command(command), hash(compute_hash(command)), handler(handler)
    {
    }

    std::string execute(std::string_view argument) const
    {
        if (handler)
        {
            return handler(argument);
        }
        else
        {
            return "ERROR";
        }
    }

    bool matches(std::string_view commandCandidate) const
    {
        if (hash == compute_hash(commandCandidate) && command == commandCandidate)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

  private:
    std::string_view command;
    uint16_t hash = 0;
    Handler handler = nullptr;

    static uint16_t compute_hash(std::string_view input)
    {
        uint16_t computedHash = 5381;
        for (char character : input)
        {
            computedHash = ((computedHash << 5) + computedHash) + character;
        }
        return computedHash;
    }
};

class ATParser
{
  public:
    ATParser() : commandCount(0), bufferIndex(0)
    {
    }

    std::string process_input(char byte)
    {
        if (byte == '\r')
        {
            if (bufferIndex > 0)
            {
                buffer[bufferIndex] = '\0';
                log(buffer.data());

                std::string result = process_command();
                log(result);

                bufferIndex = 0;
                return result;
            }
        }
        else if (bufferIndex < bufferSize - 1)
        {
            buffer[bufferIndex++] = byte;
        }
        return "";
    }

    bool register_command(std::string_view command, ATCommand::Handler handler)
    {
        if (commandCount >= maxCommands)
        {
            return false;
        }
        commands[commandCount++] = ATCommand(command, handler);
        return true;
    }

  private:
    std::array<char, bufferSize> buffer;
    size_t bufferIndex;
    std::array<ATCommand, maxCommands> commands;
    uint8_t commandCount;

    std::string process_command()
    {
        std::string_view fullInput(buffer.data());
        std::string_view commandCandidate;
        std::string_view argumentCandidate;
        size_t equalSignIndex = std::string_view::npos;
        for (size_t index = 0; index < fullInput.size(); index++)
        {
            if (fullInput[index] == '=')
            {
                equalSignIndex = index;
                break;
            }
        }
        if (equalSignIndex != std::string_view::npos)
        {
            commandCandidate = fullInput.substr(0, equalSignIndex);
            argumentCandidate = fullInput.substr(equalSignIndex + 1);
        }
        else
        {
            commandCandidate = fullInput;
            argumentCandidate = "";
        }
        for (uint8_t index = 0; index < commandCount; index++)
        {
            if (commands[index].matches(commandCandidate))
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
    else
    {
        return "SET: NO ARGS";
    }
}

[[noreturn]] void __cheri_compartment("controller") entry_point()
{
    auto uart = MMIO_CAPABILITY(OpenTitanUart, uart);

    ATParser atParser;
    atParser.register_command("AT", handle_at_command_info);
    atParser.register_command("AT+RESET", handle_at_command_reset);
    atParser.register_command("AT+GET", handle_at_command_get);
    atParser.register_command("AT+SET", handle_at_command_set);

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
