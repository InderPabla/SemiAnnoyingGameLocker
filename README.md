# SemiAnnoyingGameLocker

A Windows CLI tool that locks Steam game executables behind Windows Firewall outbound-block rules and a Task Scheduler auto-unlock timer. Early unlock is always allowed - but you have to earn it through configurable friction (password, trivia, typed prompts).

**Not security. Not DRM. Just self-imposed friction.**

---

## Requirements

- Windows 10/11
- Visual Studio 2022 (build only)
- **Must be run as Administrator** - firewall and task scheduler require elevation

---

## Building

From a terminal in the repository root:

```powershell
msbuild .\SemiAnnoyingGameLocker.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64
```

Output: `x64\Release\SemiAnnoyingGameLocker.exe`

The project vendors `nlohmann/json` (v3.11.3) in `vendor/nlohmann/json.hpp` - no package manager step needed. SHA-256 uses the Windows CNG BCrypt API (`bcrypt.lib`), which is linked automatically.

### Running tests

Build the test project:

```powershell
msbuild .\Tests\Tests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
```

Run tests:

```powershell
.\Tests\x64\Debug\Tests.exe
```

---

## Usage

```
SemiAnnoyingGameLocker.exe <command> [options]
```

All commands require an elevated (Administrator) prompt.

---

### `lock` - Lock one or more games

```
SemiAnnoyingGameLocker.exe lock --game "Squad" --duration 5d
SemiAnnoyingGameLocker.exe lock --game "Squad" --game "CS2" --duration 3d
SemiAnnoyingGameLocker.exe lock --exe "D:\Games\MyGame\game.exe" --duration 48h
SemiAnnoyingGameLocker.exe lock --game "Squad" --duration 5d --password
SemiAnnoyingGameLocker.exe lock --game "Squad" --duration 5d --trivia-config questions.txt
SemiAnnoyingGameLocker.exe lock --game "Squad" --duration 5d --password --trivia-config questions.txt
SemiAnnoyingGameLocker.exe lock --game "Squad" --duration 5d --dry-run
SemiAnnoyingGameLocker.exe lock --game "Squad" --duration 5d --yes
```

| Option | Description |
|---|---|
| `--game <name>`, `-g` | Steam game name - auto-resolved from your Steam library. Repeatable. |
| `--exe <path>`, `-e` | Explicit path to an executable. Repeatable. Can be combined with `--game`. |
| `--duration <dur>`, `-d` | How long to lock. Supports `5d`, `48h`, `90m`, `1d12h`, `1d12h30m`. |
| `--password`, `-p` | Prompt for a password at lock time. Stored as a SHA-256 hash (Windows BCrypt). |
| `--trivia-config <file>` / `--trivia <file>` | Path to a trivia question file (see format below). |
| `--dry-run` | Preview the lock plan - shows what would be created without making any changes. |
| `--yes`, `-y` | Skip the confirmation prompt. |

**What it does:** creates one Windows Firewall outbound-block rule per executable (`SAGL::<lockId>::<exeHash>`) and one Task Scheduler task (`SAGL_Unlock_<lockId>`) that fires at the unlock time and removes everything automatically.

---

### `unlock` - Unlock early (always allowed, friction enforced)

```
SemiAnnoyingGameLocker.exe unlock --game "Squad"
SemiAnnoyingGameLocker.exe unlock --id 20260427-Squad
```

| Option | Description |
|---|---|
| `--game <name>` | Unlock by game name. |
| `--id <lockId>` | Unlock by the lock ID shown in `list`. |

`--scheduled` is used internally by Task Scheduler for auto-unlock and is not meant for manual use.

**Unlock friction flow:**

1. If a password was set: prompt for it. Correct -> unlock immediately.
2. If no password, or wrong password: 10-step typed confirmation sequence (type `CONTINUE`, `YES`, `PROCEED`, ...).
3. If a trivia file was configured: all questions must be answered correctly (up to 10, wrong answer restarts from Q1).
4. Final typed confirmation: `UNLOCK`.

---

### `list` - Show all active locks

```
SemiAnnoyingGameLocker.exe list
```

```
[ACTIVE LOCKS]

ID:          20260427-Squad
Games:       Squad
Ends:        May 2, 2026 1:30 PM
Remaining:   4d 2h
Executables: 1
Protection:  Password
```

---

### `status` - Detailed status of one lock

```
SemiAnnoyingGameLocker.exe status --game "Squad"
SemiAnnoyingGameLocker.exe status --id 20260427-Squad
```

Shows full details including each firewall rule and scheduled task with live `[ACTIVE]` / `[MISSING]` status.

---

### `cleanup` - Remove orphaned rules and tasks

```
SemiAnnoyingGameLocker.exe cleanup
```

Scans for `SAGL::*` firewall rules and `SAGL_Unlock_*` scheduled tasks that have no corresponding lock config file, and removes them. Also removes any expired locks whose scheduled task was missed (e.g. machine was off at unlock time).

---

## Trivia file format

Plain text, one question block per two lines:

```
1. What is 12 * 8?
96
2. What is the capital of France?
paris
3. How many sides does a hexagon have?
6
```

- Lines starting with `N. ` are questions.
- The next non-blank line is the accepted answer (case-insensitive).
- Up to 10 questions are asked per unlock attempt.
- A wrong answer restarts from Q1.

A sample file is included at `questions.txt`.

---

## Architecture

```
SemiAnnoyingGameLocker/
|-- SemiAnnoyingGameLocker.cpp   Entry point - admin check, CLI dispatch
|
|-- cli_parser.h                 Parses argc/argv into CliArgs
|-- lock_manager.h               Orchestrates all five commands
|
|-- config.h                     Lock config persistence
|                                 JSON via nlohmann/json
|                                 Stored in %PROGRAMDATA%\SemiAnnoyingGameLocker\locks\
|-- types.h                      LockConfig, FrictionConfig, CliArgs structs
|
|-- crypto_utils.h               SHA-256 via Windows CNG (BCrypt API)
|                                 Used for password hashing and firewall rule naming
|-- firewall_manager.h           netsh advfirewall - add/delete/list outbound block rules
|                                 Rule name format: SAGL::<lockId>::<exeHash>
|-- scheduler_manager.h          schtasks - one-shot task at unlock datetime
|                                 Task name format: SAGL_Unlock_<lockId>
|
|-- steam_resolver.h             Reads libraryfolders.vdf + appmanifest_*.acf
|                                 Walks exe files, filters out launchers/redistributables
|-- duration_parser.h            Parses "5d", "48h", "1d12h30m" -> seconds; formats remaining
|-- friction_engine.h            Password prompt (_getch masked), trivia loop, annoyance prompts
|-- logger.h                     Append-only file log -> %PROGRAMDATA%\SemiAnnoyingGameLocker\logs\
|
|-- vendor/nlohmann/json.hpp     nlohmann/json v3.11.3 (vendored single header)
|-- questions.txt                Sample trivia file
|-- Tests/                       GoogleTest project for parser/config/firewall/scheduler/friction
'-- vcpkg.json                   Manifest (for future vcpkg integration)
```

### Key design decisions

**Enforcement is firewall-only.** A single outbound-block rule per exe via `netsh advfirewall`. No process killing, no exe patching, no Steam API hooks. The game launches fine - it just can't reach online servers.

**Friction, not security.** The lock is trivially bypassable by anyone who knows to open Windows Firewall. That's intentional. The goal is to add just enough friction to prevent impulsive unlocking, not to stop a determined user.

**One rule, one exe.** Rule names embed a short SHA-256 of the exe path (`SAGL::20260427-Squad::a91f3b`) so multiple games in one lock each get their own rule, collisions are essentially impossible, and cleanup is unambiguous.

**Auto-unlock via Task Scheduler.** The scheduled task uses `/SC ONCE /RL HIGHEST` - runs once at the unlock datetime with highest privileges so it can delete the firewall rules.

**Passwords hash with Windows BCrypt.** The CNG SHA-256 implementation is FIPS-validated and hardware-accelerated. The hash is stored in the lock config JSON; plaintext is never persisted.

---

## Data files

| Path | Contents |
|---|---|
| `%PROGRAMDATA%\SemiAnnoyingGameLocker\locks\<lockId>.json` | One JSON file per active lock |
| `%PROGRAMDATA%\SemiAnnoyingGameLocker\logs\sagl.log` | Append-only event log |
