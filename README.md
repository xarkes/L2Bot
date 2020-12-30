# L2Bot

This is mainly a Proof Of Concept of a Lineage II bot which should be compatible with
L2 classic 1.5 and L2 classic 3.0.

You can find here a small description of my work: https://xarkes.com/b/making-a-lineage-2-bot.html

If you want to contribute, share ideas or whatever, feel free :)

## Requirements

- Qt for Windows
- Visual Studio 2019 (but any version should work)
- Qt tools extension for Visual Studio

## Compiling

Just load the `.sln` file into Visual Studio, compile et voilà.

## Usage

The bot is not really production ready but here are some information on how to run it:

1. Compile L2BotLib `.dll` file
2. Change the `.dll` file path in `L2Bot/Injector.h`
3. Recompile and run the bot
4. Run Lineage 2
5. When the bot has detected your instance of Lineage 2, attach to the detected l2 instance
6. Login in, select your character, and join the game
7. You are in, configure the bot and press "Start Botting"

