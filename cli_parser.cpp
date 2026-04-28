#include "cli_parser.h"
#include <iostream>
#include <stdexcept>

CliArgs CliParser::parse(int argc, char* argv[]) {
    CliArgs args;
    if (argc < 2) { printUsage(); throw std::runtime_error("No command specified."); }

    args.command = argv[1];
    static constexpr const char* validCommands[] = {"lock","unlock","list","status","cleanup"};
    bool valid = false;
    for (const char* c : validCommands) if (args.command == c) { valid = true; break; }
    if (!valid) { printUsage(); throw std::runtime_error("Unknown command: " + args.command); }

    for (int i = 2; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--game"  || flag == "-g") { requireNext(argc,argv,i,flag); args.games.push_back(argv[++i]); }
        else if (flag == "--exe"  || flag == "-e") { requireNext(argc,argv,i,flag); args.exes.push_back(argv[++i]); }
        else if (flag == "--duration" || flag == "-d") { requireNext(argc,argv,i,flag); args.durationStr = argv[++i]; }
        else if (flag == "--id") { requireNext(argc,argv,i,flag); args.lockId = argv[++i]; }
        else if (flag == "--dry-run")  { args.dryRun = true; }
        else if (flag == "--password" || flag == "-p") { args.askPassword = true; }
        else if (flag == "--trivia-config" || flag == "--trivia") { requireNext(argc,argv,i,flag); args.triviaConfig = argv[++i]; }
        else if (flag == "--yes" || flag == "-y") { args.yes = true; }
        else if (flag == "--scheduled") { args.scheduled = true; }
        else throw std::runtime_error("Unknown flag: " + flag);
    }

    if (args.command == "lock") {
        if (args.games.empty() && args.exes.empty())
            throw std::runtime_error("lock requires at least one --game or --exe");
        if (args.durationStr.empty())
            throw std::runtime_error("lock requires --duration");
    }
    if (args.command == "unlock" && !args.lockId && args.games.empty())
        throw std::runtime_error("unlock requires --id or --game");
    if (args.command == "status" && !args.lockId && args.games.empty())
        throw std::runtime_error("status requires --game or --id");

    return args;
}

void CliParser::printUsage() {
    std::cout << R"(SemiAnnoyingGameLocker - intentional friction against impulsive gaming.

USAGE:  SemiAnnoyingGameLocker.exe <command> [options]

COMMANDS:
  lock      Lock one or more games for a duration
  unlock    Unlock early (always allowed, always annoying)
  list      List all active locks
  status    Detailed status of one lock
  cleanup   Remove orphaned firewall rules and tasks

LOCK OPTIONS:
  --game <name>           Steam game name (repeatable)
  --exe <path>            Explicit executable path (repeatable)
  --duration <dur>        5d | 48h | 90m | 1d12h | 1d12h30m
  --password, -p          Prompt for unlock password (stored as SHA-256)
  --trivia-config <file>  Plain-text trivia question file
  --trivia <file>         Alias for --trivia-config
  --dry-run               Preview only, no changes
  --yes, -y               Skip confirmation prompt

UNLOCK OPTIONS:
  --game <name>  |  --id <lockId>

INTERNAL:
  --scheduled             Used by Task Scheduler auto-unlock

EXAMPLES:
  SemiAnnoyingGameLocker.exe lock --game "Squad" --duration 5d
  SemiAnnoyingGameLocker.exe lock --game "Squad" --game "CS2" --duration 3d --password
  SemiAnnoyingGameLocker.exe unlock --game "Squad"
  SemiAnnoyingGameLocker.exe status --id 20260427-Squad
  SemiAnnoyingGameLocker.exe cleanup
)";
}

void CliParser::requireNext(int argc, char* argv[], int i, const std::string& flag) {
    if (i + 1 >= argc) throw std::runtime_error(flag + " requires an argument");
}
